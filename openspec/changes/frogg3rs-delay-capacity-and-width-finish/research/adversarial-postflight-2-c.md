# Adversarial postflight 2-c: `dsp::StereoDelay` capacity mechanism

Scratch copies only, under
`/private/tmp/claude-501/-Users-diegoaguilar-canabal-Desktop/68105815-8b90-42de-aa6a-51f093984b11/scratchpad`
(`$S` below). `$S/attack2-c/app` is `$S/final-app-2` copied verbatim;
`$S/attack2-c-2/app` is a second, independent copy of the same source, used
for attack 2 so attack 1's mutation never mixes with attack 2's. The real
repository was never written to, and no `git` command was run against it.

Compile line (mirrors `app/Makefile`'s `$(DSP_TEST_BIN)` rule, `CXXFLAGS`,
and `CPPFLAGS`'s `-DFROGGERS_DSP_CHECKS`):

```
cd <app-copy> && rm -f build/froggers_dsp_parity_tests && \
  clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include \
    -std=c++20 -Wall -Wextra -Wpedantic -O2 -DFROGGERS_DSP_CHECKS \
    FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests && \
  ./build/froggers_dsp_parity_tests
```

Python gates run with the Makefile's own arguments, `<app-dir>` substituted
with the copy's own path:

```
python3 check_delay_capacity_parameters_are_swept.py "<app-dir>"

python3 check_delay_capacity_break_proofs.py "<app-dir>" clang++ \
  "-I<app-dir> -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2" \
  "<app-dir>/FroggersDspParityTests.cpp"
```

## The mechanism and its accepting paths

`dsp::StereoDelay::Process` (`app/dsp/Delay.hpp`) computes `timeL`/`timeR`
(seconds of read lag) from three telescoped terms, each clamped against what
the terms before it leave of `capacitySeconds = capacity / sampleRate`:

- `baseSeconds = std::min(baseSecondsRaw, capacitySeconds)` — **not** anchored
  by the break-proof gate (its own header comment says why: see attack 2).
- `maxModSeconds = capacitySeconds - baseSeconds`,
  `modSeconds = std::min(modSecondsRaw, maxModSeconds)`.
- `maxSpreadSeconds = max(0, capacitySeconds - baseSeconds - modSeconds)`,
  `widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds)`.

Three layers exist to reject a broken version of this:

1. Compile-gated compares (`#if defined(FROGGERS_DSP_CHECKS)`): two
   `assert`s in `Process` (`timeL`/`timeR <= capacitySeconds`), two in
   `ReadAt` (`idx0`/`idx1 < line.size()`), one in `WriteSample`
   (`writePos < line.size()`). Every test/check binary `app/Makefile` builds
   passes `-DFROGGERS_DSP_CHECKS`; no shipping build does.
2. `check_delay_capacity_break_proofs.py` — mutates a copy of
   `dsp/Delay.hpp` with seven fixed (name, exact-text, exact-text) breaks,
   recompiles the real `FroggersDspParityTests.cpp` against the mutated
   copy, and requires the binary to abort (nonzero exit). A break whose
   anchor text does not match the header **exactly once** fails loudly on
   its own, independent of whether the resulting binary would have caught
   anything — so any edit to one of those six exact lines is self-detecting
   at the anchor-match step, before the compiled behavior is even reached.
3. `check_delay_capacity_parameters_are_swept.py` — scans
   `FroggersDspParityTests.cpp` (not `Delay.hpp`) for `TEST_CASE` bodies
   that mention both "capacity" and `SetWidthBalance`, and fails if any of
   the five reachable knobs (sample rate, `p.dtim`, `p.dwid`, width balance,
   `p.dmod`) is pinned to one literal value across all of them. It is a
   coverage check on the test file, not a behavior check on the header, and
   is invariant across every attack below (none of them touch the test
   file).

Accepting paths enumerated for attack: (a) the compile-gated asserts
themselves — nothing in layers 2 or 3 verifies their presence, only their
effect when combined with one of the seven listed breaks; (b) the base-term
clamp, explicitly excluded from the break-proof table by its own comment;
(c) any new bug on a line that is not one of the six break-proof anchors.

## Control run

```
$ cd $S/attack2-c/app && rm -f build/froggers_dsp_parity_tests && \
  clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include \
    -std=c++20 -Wall -Wextra -Wpedantic -O2 -DFROGGERS_DSP_CHECKS \
    FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
$ ./build/froggers_dsp_parity_tests; echo EXIT=$?
...
  [width spread grid sweep] dtim=1 dwid=1 widthBalance=1 dmod=1 expected=96000 measured=96000
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
  [random knob walk] points=2000 sample rates=44100/48000/88200/96000
[PASS] stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks
191/191 tests passed
EXIT=0
```

Instrument is live: 191/191, exit 0.

## Attack 1: delete the three compile-gated `assert` pairs

One coherent mutation on `$S/attack2-c/app/dsp/Delay.hpp` (`.orig` kept
alongside): removes both `assert`s in `Process`, both in `ReadAt`, and the
one in `WriteSample`, leaving every `#if defined(FROGGERS_DSP_CHECKS)` /
`#endif` pair empty. No clamp, no math, no test file touched.

```diff
--- dsp/Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -711,8 +711,6 @@
 #if defined(FROGGERS_DSP_CHECKS)
-        assert(timeL <= capacitySeconds);
-        assert(timeR <= capacitySeconds);
 #endif
 
         float dL = ReadAt(timeL, lineL);
@@ -1000,8 +998,6 @@
 #if defined(FROGGERS_DSP_CHECKS)
-        assert(idx0 < line.size());
-        assert(idx1 < line.size());
 #endif
         return line[idx0] * (1.0f - frac) + line[idx1] * frac;
     }
@@ -1010,7 +1006,6 @@
     void WriteSample(float sample, std::vector<float>& line)
     {
 #if defined(FROGGERS_DSP_CHECKS)
-        assert(writePos < line.size());
 #endif
         line[writePos] = sample;
     }
```

Command and output — direct parity-suite run (unchanged compile line above,
run against the mutated header):

```
$ ./build/froggers_dsp_parity_tests; echo EXIT=$?
...
  [width spread grid sweep] dtim=1 dwid=1 widthBalance=1 dmod=1 expected=96000 measured=96000
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
  [random knob walk] points=2000 sample rates=44100/48000/88200/96000
[PASS] stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks
191/191 tests passed
EXIT=0
```

**191/191, unchanged.** The direct DSP parity suite is blind to the asserts'
removal by itself — expected, since with every clamp still intact nothing
the asserts guard against ever fires; the asserts were pure defense-in-depth
over math that already holds.

`check_delay_capacity_parameters_are_swept.py` (unaffected, test file
untouched):

```
$ python3 check_delay_capacity_parameters_are_swept.py "$S/attack2-c/app"
...
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
$ echo EXIT=$?
EXIT=0
```

`check_delay_capacity_break_proofs.py` against the same mutated header (this
is where the finding is):

```
$ python3 check_delay_capacity_break_proofs.py "$S/attack2-c/app" clang++ \
    "-I$S/attack2-c/app -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2" \
    "$S/attack2-c/app/FroggersDspParityTests.cpp"
GATE_EXIT=1
check-delay-capacity-break-proofs: PASS  modulation-bound-removed  exit=1  56.51s
check-delay-capacity-break-proofs: PASS  width-bound-removed  exit=1  58.39s
check-delay-capacity-break-proofs: PASS  modulation-dropped-from-width-budget  exit=1  50.14s
check-delay-capacity-break-proofs: PASS  wrap-admits-capacity  exit=1  46.41s
check-delay-capacity-break-proofs: FAIL  lines-allocated-short  exit=0  42.34s
check-delay-capacity-break-proofs:   reason: stayed green
check-delay-capacity-break-proofs: FAIL  off-grid-width-balance-headroom  exit=0  42.30s
check-delay-capacity-break-proofs:   reason: stayed green
check-delay-capacity-break-proofs: PASS  capacity-headroom-shortens-reads  exit=1  40.47s
check-delay-capacity-break-proofs: total 336.56s across 7 breaks
check-delay-capacity-break-proofs: FAIL
```

(Run twice for reproducibility — first run at 18:04, identical PASS/FAIL
partition and identical "stayed green" reasons, only wall-clock seconds
differ.)

**Verdict: caught, but only by this one gate, and only by 5 of its 7
entries.** With the asserts gone, reintroducing `lines-allocated-short`
(`lineL.assign(capacity, 0.0f)` → `lineL.assign(capacity - 64, 0.0f)`, a
genuinely undersized buffer) now compiles and runs to exit 0 — the missing
`assert(idx0 < line.size())`/`assert(idx1 < line.size())` was the only thing
that had ever caught that specific defect; the "expected vs. measured"
value checks never notice an out-of-bounds read that still returns a finite
float. Reintroducing `off-grid-width-balance-headroom` (a `maxSpreadSeconds`
term that vanishes exactly at `widthBalance` ∈ {0.0, 0.5, 1.0}, the only
values the grid sweep test iterates) also now stays green — the missing
`assert(timeL/timeR <= capacitySeconds)` was the only thing that could have
caught an off-grid `widthBalance` overflow, since the grid test's own
"expected == measured" check is exact on-grid and the random-knob-walk test
(which does hit off-grid `widthBalance` values) checks only
`std::isfinite`, never the bound itself.

This blocks delivery (`check-delay-capacity-break-proofs` is a prerequisite
of `make test`, and the recipe above exits 1), so the attack does not reach
a green `make test`. But it demonstrates the accepting path the rules ask
to enumerate: **the compiled parity binary alone (the thing
`$(DSP_TEST_BIN)` in `make test` actually runs) would report 191/191 and
exit 0 with these asserts gone and no other change** — anyone reading only
that binary's output, or any future break not already in the seven-entry
table, would see nothing wrong. The break-proof gate's own value depends
entirely on its fixed table continuing to include exactly the defect shapes
these asserts were the last line of defense for; the table itself says nothing
protects it from a defect shape not yet in it.

## Attack 2: remove the base-term clamp (the gate's own declared gap)

`check_delay_capacity_break_proofs.py`'s header states the base-term bound
(`baseSeconds = std::min(baseSecondsRaw, capacitySeconds)`) is "deliberately
absent" from its table because removing it "does not, on its own, produce
an unrejected break." Tested directly, on a fresh copy
(`$S/attack2-c-2/app`), asserts left intact:

```diff
--- dsp/Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -685,7 +685,7 @@
-        const float baseSeconds = std::min(baseSecondsRaw, capacitySeconds);
+        const float baseSeconds = baseSecondsRaw;
         const float modSecondsRaw = std::sin(lfoPhase) * p.dmod * baseSeconds * 0.08f;
         const float maxModSeconds = capacitySeconds - baseSeconds;
         const float modSeconds = std::min(modSecondsRaw, maxModSeconds);
```

This line is not one of the seven break-proof anchors and shares no text
with any of them, so it does not disturb that gate's other six checks.

```
$ cd $S/attack2-c-2/app && rm -f build/froggers_dsp_parity_tests && \
  clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include \
    -std=c++20 -Wall -Wextra -Wpedantic -O2 -DFROGGERS_DSP_CHECKS \
    FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
$ ./build/froggers_dsp_parity_tests; echo EXIT=$?
...
  [width spread grid sweep] dtim=1 dwid=1 widthBalance=1 dmod=1 expected=96000 measured=96000
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
  [random knob walk] points=2000 sample rates=44100/48000/88200/96000
[PASS] stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks
191/191 tests passed
EXIT=0
```

Every `dtim=1` grid row (sample rate 96000 Hz, where `kMaxDelaySamples`
caps `capacity` below `kMaxDelaySeconds`, so `capacitySeconds == 1.0` while
`baseSecondsRaw` at `dtim=1` reaches `2.0`) still reports
`expected=96000 measured=96000` — no divergence, no assert fires.

**Not exploitable; the gate's own claim holds.** Trace: whenever
`baseSeconds > capacitySeconds`, `maxModSeconds = capacitySeconds -
baseSeconds` goes negative; `modSeconds = min(modSecondsRaw, maxModSeconds)`
then always resolves to `maxModSeconds` itself, since `modSecondsRaw` (built
from `dmod ∈ [0,1]`, `sin ∈ [-1,1]`) can never be smaller than an
arbitrarily negative `maxModSeconds`. So `timeL = baseSeconds + modSeconds =
baseSeconds + (capacitySeconds - baseSeconds) = capacitySeconds` exactly,
regardless of how far `baseSeconds` overshot. `maxSpreadSeconds` then
collapses to `max(0, capacitySeconds - baseSeconds - modSeconds) =
max(0, 0) = 0`, so `widthSpread = 0` and `timeR = capacitySeconds` too. The
telescoped subtraction structure repairs a missing upstream clamp
downstream, by construction, for this specific term — confirmed by trace
and by the literal `expected == measured == capacitySeconds*sr` output
above at every grid point where `baseSeconds` would otherwise have
overshot.

## Accepting paths enumerated and not attacked

- **The five other break-proof anchors's exact-text mechanism itself.**
  Directly editing any of `modSeconds = std::min(...)`, `widthSpread =
  std::min(...)`, the `maxSpreadSeconds` line (shared by two table entries),
  `WrapIndex`'s `while (idx >= capacity)`, or `lineL.assign(capacity,
  0.0f)` was not attempted as a fresh, differently-worded bug, because
  attack 1 already established the general principle more cheaply: the
  anchor match is on exact text, not on behavior, so any edit to these six
  lines is self-detecting (old_text count drops to 0, the gate FAILs loudly
  on "old_text matched 0 times") independent of whether the new bug is
  logically equivalent to the table's own entry. A rewritten version of one
  of these five would face the identical anchor-mismatch tripwire attack 1's
  reasoning already covers; running it would spend another ~5-6 minutes of
  build time to re-confirm the same mechanism.
- **`SetSampleRate`'s own `capacity = std::min(kMaxDelaySamples,
  ceil(kMaxDelaySeconds*sampleRate))` cap.** Weakening or removing the
  `kMaxDelaySamples` half of this min was considered and not run: `lineL`/
  `lineR` are allocated from the same `capacity` variable this computes, so
  inflating `capacity` inflates the backing vectors in lockstep — self-
  consistent, no index or read/write ever exceeds the (now larger) `line
  .size()`. It is a resource-budget regression (unbounded memory at high
  sample rates), not a capacity-overflow of the kind this postflight's
  checks exist to catch, so it sits outside this attack's target invariant.
- **Attacking `FroggersDspParityTests.cpp` itself** (e.g., loosening
  `REQUIRE_NEAR` tolerances, or narrowing the `dtims`/`dwids`/`wbs`/`dmods`
  grids while keeping >1 distinct literal per parameter so
  `check_delay_capacity_parameters_are_swept.py` still reports OK) was not
  attempted in the main pass below the fold — the sweep gate's own coverage
  requirement (>1 distinct value, nothing about grid density or tolerance
  width) is a distinct, real gap. The Exchange round below (Finding V)
  covers exactly this path, contributed by another attacker and reproduced
  here.

## Exchange

Two findings from other attackers, reproduced here in fresh scratch copies
(`$S/attack2-c-u`, `$S/attack2-c-v`) against the same control. Same compile
line and gate invocations as above, `<app-dir>` substituted per copy.

### Finding U: `#define NDEBUG` ahead of `#include <cassert>`

Claim: one line in `dsp/Delay.hpp`, `#define NDEBUG` placed before its
`#include <cassert>`, turns every compile-gated `assert` into a no-op while
leaving `FROGGERS_DSP_CHECKS` defined. Alone: parity suite 191/191, sweep
gate OK, clean compile. Against the break-proof gate: 2 of 7 entries "stayed
green".

Mutation, on a fresh copy (`$S/attack2-c-u/app/dsp/Delay.hpp`):

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

Direct parity suite + sweep gate:

```
$ cd $S/attack2-c-u/app && rm -f build/froggers_dsp_parity_tests && \
  clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include \
    -std=c++20 -Wall -Wextra -Wpedantic -O2 -DFROGGERS_DSP_CHECKS \
    FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
$ ./build/froggers_dsp_parity_tests; echo EXIT=$?
...
191/191 tests passed
EXIT=0
$ python3 check_delay_capacity_parameters_are_swept.py "$S/attack2-c-u/app"
...
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
EXIT=0
```

Break-proof gate:

```
$ python3 check_delay_capacity_break_proofs.py "$S/attack2-c-u/app" clang++ \
    "-I$S/attack2-c-u/app -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2" \
    "$S/attack2-c-u/app/FroggersDspParityTests.cpp"
GATE_EXIT=1
check-delay-capacity-break-proofs: PASS  modulation-bound-removed  exit=1  41.12s
check-delay-capacity-break-proofs: PASS  width-bound-removed  exit=1  44.57s
check-delay-capacity-break-proofs: PASS  modulation-dropped-from-width-budget  exit=1  42.36s
check-delay-capacity-break-proofs: PASS  wrap-admits-capacity  exit=1  44.75s
check-delay-capacity-break-proofs: FAIL  lines-allocated-short  exit=0  43.88s
check-delay-capacity-break-proofs:   reason: stayed green
check-delay-capacity-break-proofs: FAIL  off-grid-width-balance-headroom  exit=0  42.70s
check-delay-capacity-break-proofs:   reason: stayed green
check-delay-capacity-break-proofs: PASS  capacity-headroom-shortens-reads  exit=1  45.40s
check-delay-capacity-break-proofs: total 304.79s across 7 breaks
check-delay-capacity-break-proofs: FAIL
```

**CONFIRMED**, all four sub-claims, numbers exact: 191/191 alone, sweep gate
OK alone, and exactly 2 of 7 break-proof entries ("lines-allocated-short",
"off-grid-width-balance-headroom") "stayed green" — the identical pair, for
the identical reason, as this postflight's own attack 1 above (deleting the
`assert` text directly). `#define NDEBUG` and physically removing the
`assert` calls are two routes to the same nullified macro; the evidence
matches attack 1's literal output line for line. Inside the job the checks
claim (the compile-gated compares are exactly what the break-proof gate's
docstring says it protects). End to end under `make test`: **caught** —
`check-delay-capacity-break-proofs` is a `test:` prerequisite and exits 1,
so the two "combined" cases the finding describes do block delivery, matching
its own claim. The "alone" case blocks nothing on its own, also matching the
claim, and is the same latent gap attack 1 already flagged: the direct
`$(DSP_TEST_BIN)` run any human would look at first shows 191/191 and gives
no hint that two specific defect classes have gone blind.

### Finding V: test-file-only — `dmod` pinned in the grid test, coverage faked in dead code

Claim: pin the real grid test's `dmods[]` to `{0.0f}`, add one inert
`TEST_CASE` elsewhere whose "swept" literals for all five tracked
parameters sit inside `if (false)` plus an unused local named `capacity`;
`check_delay_capacity_parameters_are_swept.py` reports OK; the parity suite
passes; claimed to block neither, since the compares still catch an actual
broken bound.

Mutation, on a fresh copy (`$S/attack2-c-v/app/FroggersDspParityTests.cpp`,
`.orig` kept alongside) — reproduced independently from the finding's
description, not copied from the other attacker's diff:

```diff
--- FroggersDspParityTests.cpp.orig
+++ FroggersDspParityTests.cpp
@@ -10636,7 +10636,7 @@
     const float dtims[] = {0.0f, 0.5f, 0.9f, 0.99f, 1.0f};
     const float dwids[] = {0.0f, 0.5f, 0.75f, 1.0f};
     const float wbs[] = {0.0f, 0.5f, 1.0f};
-    const float dmods[] = {0.0f, 1.0f};
+    const float dmods[] = {0.0f};
 
     for (float dtim : dtims) {
@@ -10767,6 +10767,27 @@
     std::cout << "  [random knob walk] points=" << points << " sample rates=44100/48000/88200/96000\n";
 }
 
+TEST_CASE(stereo_delay_capacity_surface_sweep_placeholder) {
+    // Placeholder capacity-surface coverage note.
+    size_t capacity = 0;
+    (void)capacity;
+    if (false) {
+        dsp::StereoDelay delay;
+        delay.SetWidthBalance(0.25f);
+        delay.SetSampleRate(44100.0f);
+        const float dtims[] = {0.1f, 0.6f};
+        const float dwids[] = {0.2f, 0.8f};
+        const float wbs[] = {0.25f, 0.75f};
+        const float dmods[] = {0.3f, 0.7f};
+        (void)delay;
+        (void)dtims;
+        (void)dwids;
+        (void)wbs;
+        (void)dmods;
+    }
+    REQUIRE_TRUE(true);
+}
+
 }  // namespace
 
 int main() {
```

Sweep gate:

```
$ python3 check_delay_capacity_parameters_are_swept.py "$S/attack2-c-v/app"
check-delay-capacity-parameters-are-swept: capacity-surface checks found: stereo_delay_width_spread_never_reads_past_the_line_capacity, stereo_delay_width_spread_bound_is_inert_away_from_capacity, stereo_delay_width_spread_bound_holds_across_the_reachable_grid, stereo_delay_capacity_surface_sweep_placeholder
  sample rate: [44100.0, 48000.0, 96000.0]
  Delay time (p.dtim): [0.0, 0.1, 0.3, 0.5, 0.6, 0.9, 0.99, 1.0]
  Stereo width (p.dwid): [0.0, 0.2, 0.5, 0.75, 0.8, 1.0]
  Width balance: [0.0, 0.25, 0.5, 0.75, 1.0]
  Mod depth (p.dmod): [0.0, 0.3, 0.7]
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
EXIT=0
```

Direct parity suite (Delay.hpp unmodified):

```
$ cd $S/attack2-c-v/app && rm -f build/froggers_dsp_parity_tests && \
  clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include \
    -std=c++20 -Wall -Wextra -Wpedantic -O2 -DFROGGERS_DSP_CHECKS \
    FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
$ ./build/froggers_dsp_parity_tests; echo EXIT=$?
...
[PASS] stereo_delay_capacity_surface_sweep_placeholder
192/192 tests passed
EXIT=0
```

Break-proof gate (uses this test file as `$(DSP_TEST_SOURCES)`, `Delay.hpp`
still the real header the gate mutates one break at a time):

```
$ python3 check_delay_capacity_break_proofs.py "$S/attack2-c-v/app" clang++ \
    "-I$S/attack2-c-v/app -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2" \
    "$S/attack2-c-v/app/FroggersDspParityTests.cpp"
GATE_EXIT=0
check-delay-capacity-break-proofs: PASS  modulation-bound-removed  exit=-6  42.67s
check-delay-capacity-break-proofs: PASS  width-bound-removed  exit=-6  42.37s
check-delay-capacity-break-proofs: PASS  modulation-dropped-from-width-budget  exit=-6  46.61s
check-delay-capacity-break-proofs: PASS  wrap-admits-capacity  exit=-6  4.78s
check-delay-capacity-break-proofs: PASS  lines-allocated-short  exit=-6  4.82s
check-delay-capacity-break-proofs: PASS  off-grid-width-balance-headroom  exit=-6  45.65s
check-delay-capacity-break-proofs: PASS  capacity-headroom-shortens-reads  exit=1  44.77s
check-delay-capacity-break-proofs: total 231.68s across 7 breaks
check-delay-capacity-break-proofs: OK -- every break was rejected
```

**CONFIRMED**, both sub-claims: the sweep gate reports OK (exit 0) with its
`Mod depth (p.dmod)` column reading `[0.0, 0.3, 0.7]` even though the only
value the real, live grid test ever exercises is `0.0` — the `0.3`/`0.7`
entries are regex hits inside a dead `if (false)` block the gate's
text-matching has no reachability analysis to see through, so its own
stated purpose ("fails when the Delay capacity-surface checks all pin some
shipping parameter to one value") is defeated on exactly the parameter this
mutation pins. And the break-proof gate still rejects all 7/7 (six by
`SIGABRT`, i.e. the still-intact compile-gated `assert`s firing, one by an
ordinary `REQUIRE` failure) — `off-grid-width-balance-headroom` in
particular, the entry whose only detector at off-grid `widthBalance` values
is the `Process` assert, is still caught because the unmodified
random-knob-walk test (not a "capacity-surface" body by the gate's own
`"capacity" AND "SetWidthBalance"` heuristic, so its coverage was never at
stake) still drives `p.dmod`/`widthBalance` through continuous random
values every run, landing off-grid on some of its 2000 points, and the
intact assert catches it there regardless of what the grid test's own
`dmods[]` array now contains. Inside the job the check claims (its
docstring names exactly this pin-to-one-value failure mode). End to end
under `make test`: **gets through the sweep gate, caught nowhere else
because nothing is actually broken** — `check-delay-capacity-parameters-
are-swept` reports OK when it should have failed (a real defect in that
gate, silently defeated), but `make test` as a whole still passes only
because `dsp/Delay.hpp` itself was never touched in this finding. The
finding is correctly scoped: it is a hole in one gate's own coverage
argument, not a path to a broken capacity mechanism reaching a green
`make test` — a future header regression in the dmod term would still be
caught by the random-walk test's intact assert, independent of this gate.
