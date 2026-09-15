# Adversarial postflight 2-a — dsp::StereoDelay capacity guard

Scratch copy: `$S/attack2-a/app`, where
`S=/private/tmp/claude-501/-Users-diegoaguilar-canabal-Desktop/68105815-8b90-42de-aa6a-51f093984b11/scratchpad`.
Sheaf include dir: `/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include`.
All commands below are run from `$S/attack2-a/app` unless stated otherwise. `dsp/Delay.hpp.orig`
in that copy is the untouched original; every attack restores from it before mutating.

Build command for the parity suite (used throughout):

```
rm -f build/froggers_dsp_parity_tests && \
clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include \
  -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2 \
  FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests && \
./build/froggers_dsp_parity_tests
```

Gate commands:

```
python3 check_delay_capacity_parameters_are_swept.py "$S/attack2-a/app"
python3 check_delay_capacity_break_proofs.py "$S/attack2-a/app" "clang++" \
  "-I$S/attack2-a/app -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2" \
  "$S/attack2-a/app/FroggersDspParityTests.cpp"
```

## The mechanism, as read from the code

`StereoDelay::Process` (`dsp/Delay.hpp`) computes a forward-tap lag from three
terms in order — `baseSeconds = min(baseSecondsRaw, capacitySeconds)`,
`modSeconds = min(modSecondsRaw, capacitySeconds - baseSeconds)`,
`widthSpread = min(widthSpreadRaw, max(0, capacitySeconds - baseSeconds - modSeconds))`
— then asserts `timeL <= capacitySeconds` and `timeR <= capacitySeconds`
before calling `ReadAt`. Both asserts, and the two in `ReadAt`
(`idx0 < line.size()`, `idx1 < line.size()`) and the one in `WriteSample`
(`writePos < line.size()`), are wrapped in `#if defined(FROGGERS_DSP_CHECKS)`
and are ordinary `<cassert>` `assert()`s — so they compile to nothing at all
under `-DNDEBUG`, independent of `FROGGERS_DSP_CHECKS`. `app/Makefile` never
defines `NDEBUG`, so in every binary it builds these asserts are live.

`ReadAt`'s own index arithmetic (`idx0I = floorPos % capacityI`, then
`WrapIndex`) makes `idx0`/`idx1` provably `< capacity` for *any* finite
`seconds` argument — the modulo, not the caller's bound, is what keeps a read
in-bounds. So `ReadAt`'s own asserts can only ever fire when `capacity` and
`line.size()` have diverged (the array is shorter than the variable believes).
The thing that actually enforces "lag never exceeds capacity" as a *semantic*
promise (not just a memory-safety one) is the pair of asserts in `Process`
directly comparing `timeL`/`timeR` against `capacitySeconds` — every other
defense (the grid-sweep test, the random-walk test, `WriteSample`'s own
assert) is either downstream of that same comparison or only catches the
subset of defects large enough to move a *measured* lag outside a tolerance
band.

## Control run

```
$ rm -f build/froggers_dsp_parity_tests && clang++ -I. -I.../include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2 FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
(clean compile, no warnings)
$ ./build/froggers_dsp_parity_tests
...
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
  [random knob walk] points=2000 sample rates=44100/48000/88200/96000
[PASS] stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks
191/191 tests passed
$ echo EXIT=$?
EXIT=0

$ python3 check_delay_capacity_parameters_are_swept.py "$S/attack2-a/app"
check-delay-capacity-parameters-are-swept: capacity-surface checks found: stereo_delay_width_spread_never_reads_past_the_line_capacity, stereo_delay_width_spread_bound_is_inert_away_from_capacity, stereo_delay_width_spread_bound_holds_across_the_reachable_grid
  sample rate: [48000.0, 96000.0]
  Delay time (p.dtim): [0.0, 0.3, 0.5, 0.9, 0.99, 1.0]
  Stereo width (p.dwid): [0.0, 0.5, 0.75, 1.0]
  Width balance: [0.0, 0.5, 1.0]
  Mod depth (p.dmod): [0.0, 1.0]
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
(exit 0)

$ python3 check_delay_capacity_break_proofs.py "$S/attack2-a/app" clang++ "-I$S/attack2-a/app -I.../include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2" "$S/attack2-a/app/FroggersDspParityTests.cpp"
check-delay-capacity-break-proofs: PASS  modulation-bound-removed  exit=-6  40.23s
check-delay-capacity-break-proofs: PASS  width-bound-removed  exit=-6  40.09s
check-delay-capacity-break-proofs: PASS  modulation-dropped-from-width-budget  exit=-6  40.91s
check-delay-capacity-break-proofs: PASS  wrap-admits-capacity  exit=-6  4.12s
check-delay-capacity-break-proofs: PASS  lines-allocated-short  exit=-6  4.29s
check-delay-capacity-break-proofs: PASS  off-grid-width-balance-headroom  exit=-6  44.40s
check-delay-capacity-break-proofs: PASS  capacity-headroom-shortens-reads  exit=1  46.70s
check-delay-capacity-break-proofs: total 220.75s across 7 breaks
check-delay-capacity-break-proofs: OK -- every break was rejected
(exit 0)
```

The instrument is live: every one of the 7 known breaks aborts (`exit=-6` is
`SIGABRT` from a fired `assert`; `capacity-headroom-shortens-reads` is caught
instead by the suite's own `REQUIRE_NEAR`, `exit=1`).

## Attack A — silently disable the compile-gated asserts (`#define NDEBUG`)

`FROGGERS_DSP_CHECKS` gates whether the `assert()` calls are *present in the
source*; it does nothing about whether `assert()` itself is a no-op.
Standard `<assert.h>` has no include guard on the macro body — it is
designed to be re-included and redefine `assert` from whatever `NDEBUG`
currently is, even if some earlier header already pulled in `<cassert>`.
One line right before `dsp/Delay.hpp`'s own `#include <cassert>` turns every
`assert()` later in the file into `((void)0)`, permanently, for that
translation unit — while leaving `FROGGERS_DSP_CHECKS` defined, so
`check_delay_capacity_parameters_are_swept.py`'s test-body scan (which never
looks at the header) sees nothing wrong, and none of the seven `old_text`
anchors `check_delay_capacity_break_proofs.py` matches against are touched.
The suite's own `REQUIRE_TRUE`/`REQUIRE_NEAR` macros are hand-rolled
(`FroggersDspParityTests.cpp:64,73`) and do not use `assert`, so they are
unaffected — this is a pure removal of the header's own guard, not of the
test harness.

```diff
--- dsp/Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -60,6 +60,7 @@
 
 #include <algorithm>
 #include <array>
+#define NDEBUG
 #include <cassert>
 #include <cmath>
 #include <cstddef>
```

Command: the same build+run as the control, against this one-line mutation.

```
$ rm -f build/froggers_dsp_parity_tests && clang++ -I. -I.../include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2 FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
(clean compile, no warnings — -Wall -Wextra -Wpedantic raise nothing)
$ ./build/froggers_dsp_parity_tests
...
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
  [random knob walk] points=2000 sample rates=44100/48000/88200/96000
[PASS] stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks
191/191 tests passed
EXIT=0

$ python3 check_delay_capacity_parameters_are_swept.py "$S/attack2-a/app"
... check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
(exit 0, unchanged from control — this gate never reads dsp/Delay.hpp)
```

**Got through, on its own, with zero observable effect** — the mutation
disables a defense layer without breaking anything the current, correct
formula would trip. That is exactly what makes it dangerous: nothing in the
suite, the diff review surface, or a compiler warning flags it. Its actual
effect only shows up combined with a second mutation, which is what
`check_delay_capacity_break_proofs.py`'s own machinery does automatically
(it copies `app_dir/dsp`, i.e. my already-`NDEBUG`'d copy, then layers each
of the 7 known breaks on top):

```
$ python3 check_delay_capacity_break_proofs.py "$S/attack2-a/app" clang++ "-I$S/attack2-a/app -I.../include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2" "$S/attack2-a/app/FroggersDspParityTests.cpp"
check-delay-capacity-break-proofs: PASS  modulation-bound-removed  exit=1  42.23s
check-delay-capacity-break-proofs: PASS  width-bound-removed  exit=1  47.13s
check-delay-capacity-break-proofs: PASS  modulation-dropped-from-width-budget  exit=1  44.37s
check-delay-capacity-break-proofs: PASS  wrap-admits-capacity  exit=1  57.92s
check-delay-capacity-break-proofs: FAIL  lines-allocated-short  exit=0  58.68s
check-delay-capacity-break-proofs:   reason: stayed green
check-delay-capacity-break-proofs: FAIL  off-grid-width-balance-headroom  exit=0  49.48s
check-delay-capacity-break-proofs:   reason: stayed green
check-delay-capacity-break-proofs: PASS  capacity-headroom-shortens-reads  exit=1  45.66s
check-delay-capacity-break-proofs: total 345.47s across 7 breaks
check-delay-capacity-break-proofs: FAIL
```

Four of the seven (`modulation-bound-removed`, `width-bound-removed`,
`modulation-dropped-from-width-budget`, `wrap-admits-capacity`) are still
caught — but now by a `REQUIRE_*` failure (`exit=1`) instead of the assert
(`exit=-6` in the control), because those particular defects are large
enough, or land inside the grid-sweep/`never_reads_past_the_line_capacity`
test's own expected-vs-measured comparison, that a behavioral test still
notices. `lines-allocated-short` and `off-grid-width-balance-headroom` do
not — they now compile, run, and return 0. Reproduced standalone below as
Attacks B and C.

## Attack B — `#define NDEBUG` + line shorter than `capacity`

Same `NDEBUG` insertion, plus the same one-line change
`check_delay_capacity_break_proofs.py` calls `lines-allocated-short`:
`lineL` is allocated 64 samples short of what `capacity` (and therefore
`WriteSample`'s and `ReadAt`'s index math) believes it is. Every `WriteSample`
call for `writePos` in `[capacity-64, capacity-1]` writes past the end of the
actual `std::vector<float>` allocation; every `ReadAt` landing an index in
that range reads past it — a real heap buffer overflow, not just a
wrong-but-in-bounds lag.

```diff
--- dsp/Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -60,6 +60,7 @@
 
 #include <algorithm>
 #include <array>
+#define NDEBUG
 #include <cassert>
 #include <cmath>
 #include <cstddef>
@@ -548,7 +549,7 @@
         {
             capacity = 4;
         }
-        lineL.assign(capacity, 0.0f);
+        lineL.assign(capacity - 64, 0.0f);
         lineR.assign(capacity, 0.0f);
         writePos = 0;
         lfoPhase = 0.0f;
```

```
$ rm -f build/froggers_dsp_parity_tests && clang++ -I. -I.../include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2 FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
(clean compile)
$ ./build/froggers_dsp_parity_tests
...
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
  [random knob walk] points=2000 sample rates=44100/48000/88200/96000
[PASS] stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks
191/191 tests passed
EXIT=0
```

**Got through.** A live out-of-bounds heap read/write in the delay's own
sample buffer — every one of `lineL`'s last 64 slots is written and read
out of its actual allocation on the very first buffer wrap, which the warm-up
loops in the capacity-surface tests and the random-walk test's `capacity+8`
priming both reach — with the full 191-test suite, including the tests
purpose-built for exactly this class of defect, reporting all-green. (An
`-fsanitize=address` rebuild was attempted for a second, independent
confirmation of the corruption; it did not run to completion on this Mac —
`AddressSanitizer: CHECK failed: sanitizer_malloc_mac.inc:189
"((!asan_init_is_running)) != (0)"` on process start, an environment/ASan
interceptor-ordering issue unrelated to this mutation — so that leg is
reported as inconclusive, not as corroboration.)

## Attack C — `#define NDEBUG` + width-balance headroom that vanishes exactly at the tested grid

Same `NDEBUG` insertion, plus `check_delay_capacity_break_proofs.py`'s
`off-grid-width-balance-headroom` mutation: an extra
`-2·wb·(wb-0.5)·(wb-1)` term added to `maxSpreadSeconds`. That cubic is
exactly zero at `wb ∈ {0, 0.5, 1}` — precisely the three values
`stereo_delay_width_spread_bound_holds_across_the_reachable_grid`'s `wbs[]`
sweeps — and strictly positive over most of `(0.5, 1)` (e.g. `+0.094` at
`wb=0.75`), which lets `widthSpread`, and therefore `timeR`, exceed
`capacitySeconds` for any width balance the grid doesn't happen to name.

```diff
--- dsp/Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -60,6 +60,7 @@
 
 #include <algorithm>
 #include <array>
+#define NDEBUG
 #include <cassert>
 #include <cmath>
 #include <cstddef>
@@ -695,7 +696,7 @@
         // keeps it in bounds, against whatever of the line's capacity the
         // base and modulation terms above have not already spent.
         const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
-        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
+        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds + (-2.0f) * widthBalance * (widthBalance - 0.5f) * (widthBalance - 1.0f));
         const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
         float timeL = std::max(0.001f, baseSeconds + modSeconds);
         float timeR = std::max(0.001f, baseSeconds + modSeconds + widthSpread);
```

```
$ rm -f build/froggers_dsp_parity_tests && clang++ -I. -I.../include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2 FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
(clean compile)
$ ./build/froggers_dsp_parity_tests
...
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
  [random knob walk] points=2000 sample rates=44100/48000/88200/96000
[PASS] stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks
191/191 tests passed
EXIT=0
```

**Got through.** This is the semantic invariant itself broken — `timeR` can
exceed `capacitySeconds`, the exact thing the two `assert`s in `Process`
exist to catch — for any Width balance the operator can actually dial in
outside `{0, 0.5, 1.0}` (e.g. 0.75), and the 191-test suite, which includes
`stereo_delay_width_spread_bound_holds_across_the_reachable_grid` naming
Width balance as one of its four swept knobs, reports it clean. It survives
because: the grid-sweep test's own three `wbs[]` values are exactly this
cubic's roots (by construction of the known-break table this mutation is
borrowed from — the polynomial was designed to vanish there); the
`never_reads_past_the_line_capacity` and `bound_is_inert_away_from_capacity`
tests both pin `SetWidthBalance(1.0f)`, another root; and
`stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks`, the
one test in the suite that samples Width balance at arbitrary (non-root)
values, checks only `std::isfinite(wet.l) && std::isfinite(wet.r)` — a
wrapped, aliased-but-finite read passes it outright, so with the `assert`
gone nothing in that test's own `REQUIRE_TRUE` can catch the violation
either.

## Accepting paths enumerated and not attacked

- **`ReadAt`'s own `idx0/idx1` bound.** Proven safe by construction for any
  finite `seconds` argument (`floorPos % capacityI` followed by `WrapIndex`
  always yields `< capacity`) — the only way to violate it is to make
  `capacity` and `line.size()` diverge, which is exactly Attack B. Not a
  separate attack surface once B is covered.
- **`WriteSample`'s bound.** `writePos` is kept `< capacity` unconditionally
  by `AdvanceWrite`; the only way to defeat its assert is the same
  `capacity`/`line.size()` divergence as above (again, Attack B) or breaking
  `AdvanceWrite`'s own wrap, which is a straightforward one-line change with
  no plausible "still looks right" cover story, so not pursued as a separate
  finding.
- **`p.dsnd <= 0.0001f || capacity == 0` early return.** Returns silence and
  never touches `lineL`/`lineR`; there is no line access on this path to
  violate.
- **`DelayReverser`/`ApplyReverse`'s backward pointer (`rev.pos`).** Fed to
  `ReadAt` as its own `seconds` argument, entirely independent of `timeL`/
  `timeR` and *not* covered by either of `Process`'s two asserts. In
  principle a defect in its wrap/re-anchor logic (`rev.elapsed >=
  delaySamplesWindow - fadeSamples`) could let `rev.pos` drift arbitrarily
  far from `writePos` without ever tripping a compile-gated compare, since
  `ReadAt`'s modulo makes the resulting read memory-safe regardless. Not
  attacked: the task's own framing of the mechanism under test — "a lag
  built from several knob-driven terms" bounded "against the line's
  capacity" — names the forward tap's `baseSeconds`/`modSeconds`/
  `widthSpread` composition specifically (the file's own header comment
  calls out these as the capacity-bound terms "authored here" with "no
  counterpart in the frozen source"); the reverse tap has no analogous
  capacity budget to break, and misrepresenting a "reverse window
  correctness" bug as a capacity-guard evasion would overstate the finding.
  Flagged here as a genuine gap in what `Process`'s asserts cover, for
  someone auditing that mechanism on its own terms.
- **`check_delay_capacity_parameters_are_swept.py`'s own regex scan.** It
  operates purely on `FroggersDspParityTests.cpp` text and never compiles or
  runs anything, so a header-only mutation (everything above) cannot move it
  either way — confirmed in every run above (`OK` throughout, un-perturbed
  by Attacks A/B/C). Attacking it would require editing the test file,
  outside this task's one-coherent-mutation-against-the-header scope; not
  attempted.
## Exchange

Reproduced independently in fresh copies (`$S/attack2-a/exchange-V/app`,
`$S/attack2-a/exchange-W/app`, both freshly copied from `$S/final-app-2`,
`.orig` kept), one build at a time, binary `rm`'d before each rebuild.

### Finding V — test-file-only: `dmod` grid pinned to `{0.0f}` propped up by an inert `TEST_CASE`

Reproduced the described mutation exactly: changed
`FroggersDspParityTests.cpp`'s grid test from
`const float dmods[] = {0.0f, 1.0f};` to `const float dmods[] = {0.0f};`,
and appended one new `TEST_CASE(delay_capacity_dead_coverage_probe)` whose
body calls `delay.SetWidthBalance(1.0f)`, declares an unused
`size_t capacityProbe = 0;`, and puts `float dmod = 1.0f;` inside
`if (false) { ... }`. `dsp/Delay.hpp` untouched.

```
$ python3 check_delay_capacity_parameters_are_swept.py "$S/attack2-a/exchange-V/app"
check-delay-capacity-parameters-are-swept: capacity-surface checks found: stereo_delay_width_spread_never_reads_past_the_line_capacity, stereo_delay_width_spread_bound_is_inert_away_from_capacity, stereo_delay_width_spread_bound_holds_across_the_reachable_grid, delay_capacity_dead_coverage_probe
  ...
  Mod depth (p.dmod): [0.0, 1.0]
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
(exit 0)

$ rm -f build/froggers_dsp_parity_tests && clang++ -I. -I.../include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2 FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests && ./build/froggers_dsp_parity_tests
...
192/192 tests passed
EXIT=0

$ python3 check_delay_capacity_break_proofs.py "$S/attack2-a/exchange-V/app" clang++ "-I$S/attack2-a/exchange-V/app -I.../include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2" "$S/attack2-a/exchange-V/app/FroggersDspParityTests.cpp"
check-delay-capacity-break-proofs: PASS  modulation-bound-removed  exit=-6  40.52s
check-delay-capacity-break-proofs: PASS  width-bound-removed  exit=-6  40.98s
check-delay-capacity-break-proofs: PASS  modulation-dropped-from-width-budget  exit=-6  43.90s
check-delay-capacity-break-proofs: PASS  wrap-admits-capacity  exit=-6  4.23s
check-delay-capacity-break-proofs: PASS  lines-allocated-short  exit=-6  4.12s
check-delay-capacity-break-proofs: PASS  off-grid-width-balance-headroom  exit=-6  42.48s
check-delay-capacity-break-proofs: PASS  capacity-headroom-shortens-reads  exit=1  44.91s
check-delay-capacity-break-proofs: OK -- every break was rejected
```

**CONFIRMED.** The regex-based `union` the sweep gate computes across
capacity-surface bodies picked up the dead `dmod = 1.0f` sitting inside
`if (false)` in the new `TEST_CASE` (its filter — "capacity" substring
present via `capacityProbe`, plus `SetWidthBalance` — never inspects control
flow), reporting `Mod depth: [0.0, 1.0]` and `OK` while the grid test's real,
executed sweep of `dmod` is down to a single value — exactly the failure
mode `check_delay_capacity_parameters_are_swept.py`'s own header says it
exists to catch ("fails when the capacity-surface checks all pin some
shipping parameter to one value"). Squarely inside that gate's stated job,
and the gate misses it. The second half of the claim also checks out:
`dsp/Delay.hpp` was never touched, so the compile-gated compares are exactly
as live as in the control, and `check_delay_capacity_break_proofs.py`
rejects all 7 known breaks identically to the control run (same PASS/exit
pattern, same timings within noise). With `make test` running every gate:
the sweep gate goes green on a lie, but nothing gets through end to end —
the parity suite and the break-proof gate are unaffected by a test-file-only
change and continue to catch real defects. Blocks neither execution nor
delivery today; the defect is that this specific gate's certification is
now void (a future person trusting "Mod depth: swept" to mean the grid test
covers modulation depth would be wrong), not that a broken mechanism ships.

### Finding W — delete the three compile-gated compare blocks outright

Reproduced exactly: removed the `assert(timeL <= capacitySeconds);
assert(timeR <= capacitySeconds);` pair from `Process`, the
`assert(idx0 < line.size()); assert(idx1 < line.size());` pair from
`ReadAt`, and the single `assert(writePos < line.size());` from
`WriteSample`, leaving the surrounding `#if defined(FROGGERS_DSP_CHECKS)` /
`#endif` scaffolding (now empty) and everything else byte-identical.

```diff
@@ Process() @@
 #if defined(FROGGERS_DSP_CHECKS)
-        assert(timeL <= capacitySeconds);
-        assert(timeR <= capacitySeconds);
 #endif
@@ ReadAt() @@
 #if defined(FROGGERS_DSP_CHECKS)
-        assert(idx0 < line.size());
-        assert(idx1 < line.size());
 #endif
@@ WriteSample() @@
 #if defined(FROGGERS_DSP_CHECKS)
-        assert(writePos < line.size());
 #endif
```

```
$ rm -f build/froggers_dsp_parity_tests && clang++ -I. -I.../include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2 FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests && ./build/froggers_dsp_parity_tests
...
191/191 tests passed
EXIT=0

$ python3 check_delay_capacity_break_proofs.py "$S/attack2-a/exchange-W/app" clang++ "-I$S/attack2-a/exchange-W/app -I.../include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2" "$S/attack2-a/exchange-W/app/FroggersDspParityTests.cpp"
check-delay-capacity-break-proofs: PASS  modulation-bound-removed  exit=1  42.76s
check-delay-capacity-break-proofs: PASS  width-bound-removed  exit=1  42.10s
check-delay-capacity-break-proofs: PASS  modulation-dropped-from-width-budget  exit=1  46.45s
check-delay-capacity-break-proofs: PASS  wrap-admits-capacity  exit=1  47.54s
check-delay-capacity-break-proofs: FAIL  lines-allocated-short  exit=0  45.78s
check-delay-capacity-break-proofs:   reason: stayed green
check-delay-capacity-break-proofs: FAIL  off-grid-width-balance-headroom  exit=0  42.55s
check-delay-capacity-break-proofs:   reason: stayed green
check-delay-capacity-break-proofs: PASS  capacity-headroom-shortens-reads  exit=1  40.57s
check-delay-capacity-break-proofs: FAIL
```

**CONFIRMED**, literal match on every number claimed: parity suite 191/191
(exit 0), break-proof gate 5 PASS / 2 FAIL naming the identical two breaks
("stayed green", exit=0 each), overall gate exit 1. This is the same pair
my own Attack A (`#define NDEBUG`, functionally equivalent — both compile
the three `assert()`s to nothing) found against the same table, which
cross-checks this result against an independently-derived mutation. Inside
the checks' stated job: this is a direct attack on "compile-gated compares
inside Process, ReadAt and WriteSample," one of the four check families this
audit names. With `make test` running every gate: **caught**, but only by
`check-delay-capacity-break-proofs`, and only because 2 of its 7 fixed table
entries happen to have no detector other than the deleted compares — `make`
stops at that recipe (nonzero exit) before `$(DSP_TEST_BIN)` is even built,
so delivery is blocked today. The claim that "a break not in the table would
go undetected end-to-end" is not a hypothetical: it is exactly what this
record's own Attacks B and C already demonstrate — `check_delay_capacity_break_proofs.py`
only ever tests its own 7 hardcoded transformations against whatever header
it is given, so it supplies no protection at all against a capacity defect
shaped differently from that table, with or without the compares present. If
the table's two compares-only entries were ever pruned (e.g. as
"redundant" once believed fully covered by the parity suite, which this
finding shows they are not), this exact mutation would go fully green
end-to-end with no other change required.

- **`stereo_delay_width_balance_mapping_keeps_spread_at_or_below_todays_max`
  (`FroggersDspParityTests.cpp:6951`).** Recomputes
  `dwid * baseSeconds * 0.35f * delay.widthBalance` using the same formula as
  production and compares it to itself — it never calls `Process`, so it
  cannot detect anything about the real capacity bound. It does not count as
  a capacity-surface check under `check_delay_capacity_parameters_are_swept.py`'s
  own filter (its body never contains the word "capacity"), so it also isn't
  part of what that gate certifies as swept. Noted as a vacuous test, not
  attacked as a distinct evasion route, since it grants no protection to
  route around in the first place.
