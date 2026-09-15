# Adversarial postflight 2-b: dsp::StereoDelay capacity mechanism

Independent adversarial pass against `app/dsp/Delay.hpp`'s capacity/index
guards and the checks that protect them: the compile-gated `assert`s in
`Process`/`ReadAt`/`WriteSample`, the DSP parity suite
(`app/FroggersDspParityTests.cpp`), and the two python gates
(`app/check_delay_capacity_parameters_are_swept.py`,
`app/check_delay_capacity_break_proofs.py`). Worked entirely from the code —
`openspec/` was not opened except to write this file. All work happened in
`$S/attack2-b/app`, a scratch copy of `$S/final-app-2`
(`$S` = `/private/tmp/claude-501/-Users-diegoaguilar-canabal-Desktop/68105815-8b90-42de-aa6a-51f093984b11/scratchpad`).
`.orig` copies of the two touched files live in `$S/attack2-b/attacks/`
alongside every diff and full build log named below.

## Control

Unmodified copy, compiled exactly as `app/Makefile`'s `$(DSP_TEST_BIN)` rule does:

```
cd $S/attack2-b/app
rm -f build/froggers_dsp_parity_tests
clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include \
    -std=c++20 -Wall -Wextra -Wpedantic -O2 -DFROGGERS_DSP_CHECKS \
    FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
./build/froggers_dsp_parity_tests
```

Output (`$S/attack2-b/attacks/logs/run-control.log`):
```
COMPILE_EXIT=0
EXIT=0
191/191 tests passed
```

Sweep gate:
```
cd $S/attack2-b/app && python3 check_delay_capacity_parameters_are_swept.py "$S/attack2-b/app"
```
```
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
EXIT=0
```

Break-proof gate:
```
cd $S/attack2-b/app && python3 check_delay_capacity_break_proofs.py "$S/attack2-b/app" "clang++" \
    "-I$S/attack2-b/app -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2" \
    "$S/attack2-b/app/FroggersDspParityTests.cpp"
```
```
check-delay-capacity-break-proofs: PASS  modulation-bound-removed  exit=-6  57.02s
check-delay-capacity-break-proofs: PASS  width-bound-removed  exit=-6  57.47s
check-delay-capacity-break-proofs: PASS  modulation-dropped-from-width-budget  exit=-6  49.24s
check-delay-capacity-break-proofs: PASS  wrap-admits-capacity  exit=-6  4.70s
check-delay-capacity-break-proofs: PASS  lines-allocated-short  exit=-6  4.65s
check-delay-capacity-break-proofs: PASS  off-grid-width-balance-headroom  exit=-6  45.23s
check-delay-capacity-break-proofs: PASS  capacity-headroom-shortens-reads  exit=1  42.00s
check-delay-capacity-break-proofs: OK -- every break was rejected
EXIT=0
```

All three instruments are live.

## Enumeration of accepting paths

Reading `Process`/`ReadAt`/`WriteSample`/`ApplyReverse`/`AdvanceWrite`/`SetSampleRate`
top to bottom, every branch that lets a value through without re-checking it
against `capacity`:

1. `ReadAt`'s own index math (`% capacityI`, `WrapIndex`) is unconditional
   modulo arithmetic — it cannot itself produce an index `>= capacity` for
   *any* input `seconds`, however large, provided `capacity` and the line's
   `.size()` agree. It structurally cannot violate "no index past the line";
   the only way to break that invariant is to desync `capacity` from a
   line's actual allocated size, or break the modulo in `WrapIndex` itself.
2. The three forward-tap `std::min` bounds (`modSeconds`, `widthSpread`, and
   their shared `maxSpreadSeconds`/`maxModSeconds` remainders) are anchored
   verbatim by `check_delay_capacity_break_proofs.py`'s `BREAKS` table — any
   edit to those exact lines is caught either as a real behavioral break
   (compile-gated `assert` fires under the real suite) or, if the edit
   changes the anchor text itself, as an anchor-drift `FAIL` in the
   break-proof gate.
3. The base-term bound (`baseSeconds = std::min(baseSecondsRaw,
   capacitySeconds)`) is *not* anchored and *is* algebraically redundant:
   `modSeconds <= maxModSeconds = capacitySeconds - baseSeconds` forces
   `baseSeconds + modSeconds <= capacitySeconds` regardless of whether
   `baseSeconds` itself was pre-clamped, and the same chain protects
   `widthSpread`. Removing this line cannot desync from the invariant on its
   own — confirmed algebraically, not attacked (the file's own comment
   already discloses this).
4. `assert(timeL <= capacitySeconds); assert(timeR <= capacitySeconds);`
   (`Process`, compile-gated) is a hard backstop independent of *how*
   `timeL`/`timeR` were computed — any mutation that makes either exceed
   `capacitySeconds` aborts, provided some exercised input reaches that
   state. The DSP parity suite's grid test
   (`stereo_delay_width_spread_bound_holds_across_the_reachable_grid`) and
   random-walk fuzzer
   (`stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks`,
   2,000,000 `Process()` calls across dtim/dwid/wb/dmod and four sample
   rates) both exercise states that reach this backstop for essentially any
   scaling mistake in the forward-tap chain. Attacks 2 and (structurally)
   1 confirm this.
5. `ApplyReverse` (the Reverse Blend backward tap) computes its own read
   `seconds` from a free-running `rev.pos`, re-anchored periodically by a
   wrap/crossfade trigger (`rev.elapsed >= delaySamplesWindow - fadeSamples`)
   that has **no compile-gated assert of its own** bounding the resulting
   lag against `capacitySeconds`, and is excluded from the
   `check_delay_capacity_parameters_are_swept.py` "capacity-surface" filter
   (that filter requires the body to call `SetWidthBalance`, which the
   reverse-blend tests never do). `ReadAt`'s own memory safety (point 1)
   still protects it, so breaking this trigger cannot produce an
   out-of-bounds *index*, but it can produce a lag with no relation to
   `capacitySeconds` at all — a live accepting path, attacked below and
   caught only by unrelated functional tests, not by any capacity guard.
6. `AdvanceWrite`'s own wrap (`if (writePos >= capacity) writePos = 0;`) is
   not anchored by the break-proof table (only `WrapIndex`'s `while (idx >=
   capacity)` is). An off-by-one here is not directly asserted either — it
   is caught transitively, one call later, by `WriteSample`'s
   `assert(writePos < line.size())`. Attacked below.
7. `SetSampleRate`'s two `.assign(capacity, ...)` calls: only `lineL`'s is
   anchored by the break-proof table (`lines-allocated-short`); `lineR`'s
   assignment is a second, textually distinct instance of the same
   opportunity and is not itself anchored. Attacked below; caught by the
   same generic `ReadAt` assert that protects `lineL`, confirming point 1's
   claim that this class of defect is structurally covered regardless of
   which specific `.assign()` call is broken.
8. `check_delay_capacity_parameters_are_swept.py` reads only
   `FroggersDspParityTests.cpp`'s source text: it requires a `TEST_CASE`
   body to contain the substring `capacity` (comments stripped) and a call
   to `SetWidthBalance`, then unions literal values it can find for five
   token patterns *anywhere in that body*, including inside dead code
   (`if (false) { ... }`) or unused locals. It does not check that the
   values it counts ever reach `dsp::StereoDelay::Process`. Attacked below
   — successfully.

## Attack 1: disable the reverse-tap wrap/crossfade trigger

Targets accepting path 5. Makes the trigger condition unreachable so
`rev.pos` free-runs for the whole test instead of periodically re-anchoring
near the write head.

```diff
--- Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -1111,7 +1111,7 @@
                 rev.fadeGain = 0.0f;
             }
         }
-        else if (rev.elapsed >= delaySamplesWindow - fadeSamples)
+        else if (rev.elapsed >= delaySamplesWindow - fadeSamples + 1.0e12f)
         {
```

Full diff: `$S/attack2-b/attacks/attack1.diff`.

Build/run (same command as Control, header mutated, suite unmutated):
```
COMPILE_EXIT=0
RUN_EXIT=1
189 PASS / 2 FAIL
[FAIL] stereo_delay_reverse_blend_at_maximum_time_reverses_a_sharp_attack_slow_decay_transient: FroggersDspParityTests.cpp:7531 requirement failed: peakRev >= static_cast<size_t>(kWindow) && peakRev + static_cast<size_t>(kWindow) < revOut.size()
[FAIL] stereo_delay_reverse_blend_wrap_crossfade_bounds_the_sample_to_sample_delta: FroggersDspParityTests.cpp:7651 requirement failed: maxDelta < 0.1f
```
Full log: `$S/attack2-b/attacks/logs/run-attack1.log`.

**Caught.** Not by any capacity/index assert — by the reverse-blend
functional tests, which happen to fail once the pointer stops finding
recently-written content (the free-running pointer drifts into buffer
territory that was never written, so `revOut`'s peak collapses below the
test's own liveness threshold) or stops crossfading cleanly. Confirms
accepting path 5 is real (no dedicated guard exists) but is covered
incidentally by unrelated tests, not defended by the mechanism under audit.

## Attack 2: loosen the (unanchored) modulation-budget line

Targets accepting path 2/4: the same defect class as the break-proof table's
`modulation-bound-removed` entry, expressed on a different, unanchored line
one statement earlier, so the break-proof gate's own anchors are never
touched.

```diff
--- Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -687,7 +687,7 @@
         const float baseSeconds = std::min(baseSecondsRaw, capacitySeconds);
         const float modSecondsRaw = std::sin(lfoPhase) * p.dmod * baseSeconds * 0.08f;
-        const float maxModSeconds = capacitySeconds - baseSeconds;
+        const float maxModSeconds = capacitySeconds;
         const float modSeconds = std::min(modSecondsRaw, maxModSeconds);
```

Full diff: `$S/attack2-b/attacks/attack2.diff`.

Build/run:
```
COMPILE_EXIT=0
RUN_EXIT=134
189 PASS / 0 FAIL (aborted mid-run)
Assertion failed: (timeL <= capacitySeconds), function Process, file Delay.hpp, line 714.
```
Full log: `$S/attack2-b/attacks/logs/run-attack2.log`.

**Caught.** `stereo_delay_width_spread_bound_holds_across_the_reachable_grid`'s
`dtim` near 1.0 combined with `dmod=1.0` (and phase driven to the
`sin`-peak) reaches the state where the loosened bound actually admits
`timeL > capacitySeconds`, and the compile-gated `assert` aborts the binary
before any `REQUIRE` even runs. Confirms accepting path 4 (the `Process`-level
assert) is a genuinely strong backstop, independent of where in the term
chain the defect sits, as long as an exercised input reaches the violation —
which it does here.

## Attack 3: off-by-one in `AdvanceWrite`'s wrap

Targets accepting path 6, a location the break-proof table does not anchor.

```diff
--- Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -1029,7 +1029,7 @@
     void AdvanceWrite()
     {
         writePos++;
-        if (writePos >= capacity)
+        if (writePos > capacity)
         {
             writePos = 0;
         }
```

Full diff: `$S/attack2-b/attacks/attack3.diff`.

Build/run:
```
COMPILE_EXIT=0
RUN_EXIT=134
187 PASS / 0 FAIL (aborted mid-run)
Assertion failed: (writePos < line.size()), function WriteSample, file Delay.hpp, line 1013.
```
Full log: `$S/attack2-b/attacks/logs/run-attack3.log`.

**Caught**, one `Process()` call after the off-by-one first leaves
`writePos == capacity`: `WriteSample`'s own assert fires at the very next
write. Confirms accepting path 6 is covered transitively even though no
assert sits at the actual defect site.

## Attack 4: shorten only `lineR`'s allocation

Targets accepting path 7 — the un-anchored twin of the break-proof table's
`lines-allocated-short` entry (which only ever touches `lineL`).

```diff
--- Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -549,7 +549,7 @@
         lineL.assign(capacity, 0.0f);
-        lineR.assign(capacity, 0.0f);
+        lineR.assign(capacity - 32, 0.0f);
         writePos = 0;
```

Full diff: `$S/attack2-b/attacks/attack4.diff`.

Build/run:
```
COMPILE_EXIT=0
RUN_EXIT=134
97 PASS / 0 FAIL (aborted mid-run)
Assertion failed: (writePos < line.size()), function WriteSample, file Delay.hpp, line 1013.
```
Full log: `$S/attack2-b/attacks/logs/run-attack4.log`.

**Caught**, and early (test 98 of 191, the first delay-touching test whose
`writePos` walk reaches the shortened tail) — by the generic `WriteSample`
assert, not by anything break-proof-table-specific. Confirms point 1/7's
claim: any desync between `capacity` and an actual line's `.size()` is
covered structurally, regardless of which specific allocation call carries
the defect.

## Attack 5: mask a real coverage regression in the sweep gate

Targets accepting path 8 directly — no change to `Delay.hpp` at all. This is
the one attack that got through.

Step 1 — reproduce the exact historical defect class
`check_delay_capacity_parameters_are_swept.py`'s own docstring names ("Mod
depth pinned at 0") in isolation, to confirm the gate still does its job
when nothing else is touched:

```diff
--- FroggersDspParityTests.cpp.orig
+++ FroggersDspParityTests.cpp
@@ -10636,7 +10636,7 @@
-    const float dmods[] = {0.0f, 1.0f};
+    const float dmods[] = {0.0f};
```

```
cd $S/attack2-b/app && python3 check_delay_capacity_parameters_are_swept.py "$S/attack2-b/app"
```
```
Mod depth (p.dmod): [0.0]
check-delay-capacity-parameters-are-swept: FAIL
  Mod depth (p.dmod) is held at one value (0.0) across every capacity-surface check in FroggersDspParityTests.cpp -- the capacity bound's dependence on it is unexercised
EXIT=1
```
Gate works as designed on the real regression alone.

Step 2 — add one inert `TEST_CASE`, anywhere in the file, that never calls
`dsp::StereoDelay::Process` with a nonzero mod depth: it stores its swept-looking
literals in an `if (false)` block and unused locals.

```diff
+TEST_CASE(stereo_delay_capacity_decorative_sweep_placeholder) {
+    const bool capacityProbeRan = true;
+    (void)capacityProbeRan;
+    dsp::StereoDelay delay;
+    delay.SetSampleRate(48000.0f);
+    delay.SetWidthBalance(1.0f);
+    if (false) {
+        delay.SetSampleRate(96000.0f);
+        float dtim = 0.9f;
+        float dwid = 1.0f;
+        delay.SetWidthBalance(0.25f);
+        float dmod = 0.0f;
+        dmod = 1.0f;
+        (void)dtim;
+        (void)dwid;
+        (void)dmod;
+    }
+    dsp::DelayParams p;
+    p.dtim = 0.3f;
+    p.dwid = 0.0f;
+    delay.Process(0.0f, p);
+}
```

Full combined diff (regression + placeholder): `$S/attack2-b/attacks/attack5-combined.diff`.
(Placeholder alone, with no regression: `$S/attack2-b/attacks/attack5.diff`.)

```
cd $S/attack2-b/app && python3 check_delay_capacity_parameters_are_swept.py "$S/attack2-b/app"
```
```
check-delay-capacity-parameters-are-swept: capacity-surface checks found: stereo_delay_width_spread_never_reads_past_the_line_capacity, stereo_delay_width_spread_bound_is_inert_away_from_capacity, stereo_delay_width_spread_bound_holds_across_the_reachable_grid, stereo_delay_capacity_decorative_sweep_placeholder
  sample rate: [48000.0, 96000.0]
  Delay time (p.dtim): [0.0, 0.3, 0.5, 0.9, 0.99, 1.0]
  Stereo width (p.dwid): [0.0, 0.5, 0.75, 1.0]
  Width balance: [0.0, 0.25, 0.5, 1.0]
  Mod depth (p.dmod): [0.0, 1.0]
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
EXIT=0
```

**Got through.** Two things happen at once: the gate's `capacity` substring
check (comment-stripped, but plain substring, no word boundary) accepts an
unrelated local named `capacityProbeRan`; and its literal-value union counts
`dmod = 1.0f` sitting inside `if (false) { ... }`, never reached at runtime,
never passed to `Process`. Combined with Step 1's real regression (dmod
genuinely pinned to `0.0f` in the one grid test that used to sweep it), the
gate reports `OK` — exactly the false-negative its own docstring says it
exists to prevent ("the next parameter someone pins goes unnoticed the same
way"). This is a real gap in the gate's own accepting path: it is a pure
text/regex scan with no requirement that the literals it counts are reachable
or ever flow into `dsp::StereoDelay`.

This attack does not, by itself, touch `Delay.hpp` or the DSP parity suite's
runtime assertions — `$(DSP_TEST_BIN)` still compiles and passes 191/191
with only the dmods change (nothing in the shipped code is broken, only its
test coverage is thinned). Its consequence is that a *genuinely* broken
`Delay.hpp` mod-depth-dependent bound (e.g. Attack 2's mutation, or the
break-proof table's own `modulation-bound-removed`/
`modulation-dropped-from-width-budget` entries) landing at the same time
would no longer be flagged by the sweep gate as under-tested, even though
the grid test that used to catch it (via the `assert` backstop, see Attack
2) has quietly lost its only `dmod=1.0` exercise. The break-proof gate and
the compiled asserts are unaffected by this attack and remain the real
protection for that specific bound; this finding is about the sweep gate's
own claim ("fails when the... checks all pin some shipping parameter to one
value") no longer holding once a single unrelated decorative test exists
anywhere in a 10,000+ line file.

**What breaks if not fixed:** a future edit that pins Mod depth (or any of
the other four tracked parameters) to one value in the real capacity-surface
tests — the exact regression class this gate was built three times over to
catch — passes `check-delay-capacity-parameters-are-swept` silently, as long
as any test case anywhere in the file happens to mention the right
substrings in dead code. Blocks: neither execution (the DSP binary still
runs and the break-proof/assert layer still catches an *actual* Delay.hpp
break, as Attack 2 shows) nor delivery on its own — but it removes the
early, cheap signal the sweep gate is specifically for, silently, which is
the condition its own docstring calls out as the recurring failure mode.

## Accepting paths enumerated and not attacked

- **Base-term bound removal** (`baseSeconds = std::min(baseSecondsRaw,
  capacitySeconds)`, unanchored). Not attacked: worked the algebra by hand
  (see Enumeration point 3) and it holds for every `baseSecondsRaw`,
  `modSecondsRaw`, `dwid`, `widthBalance` combination — `modSeconds`'s own
  `min` against `capacitySeconds - baseSeconds` forces `baseSeconds +
  modSeconds <= capacitySeconds` unconditionally, and the width term's
  bound is derived from that same corrected remainder. This matches the gate
  file's own disclosed rationale; re-deriving it independently rather than
  attacking it directly.
- **`kMaxDelaySamples` clamp removed** from `SetSampleRate`'s `capacity =
  std::min(kMaxDelaySamples, ...)`. Not attacked: removing it only makes
  `capacity` *larger* at sample rates above 48 kHz, which widens headroom
  rather than shrinking it — it cannot itself cause an index or lag to
  exceed the (now larger) capacity. Any resource-budget concern this clamp
  protects is outside the "must never exceed capacity / no index past the
  line" invariant under audit here.
- **`widthBalance` left unclamped in `SetWidthBalance`.** Not attacked as a
  `Delay.hpp` mutation: it is already unclamped in the shipped code (`void
  SetWidthBalance(float knob01) { widthBalance = knob01; }`), and feeding it
  a value outside `[0,1]` still cannot defeat the `widthSpread` bound, since
  `widthSpread`'s own `std::min(widthSpreadRaw, maxSpreadSeconds)` clamps
  the *result*, not the knob. This is a caller-contract gap (the comment
  above it asserts "never exceeds 1.0f" as a caller guarantee), not a
  mutation of the mechanism itself, and the DSP parity suite never exercises
  a stray out-of-range value either way — noted, not attacked, because it
  is not a break I could introduce in the mechanism under audit without
  also changing what "broken" means for that call.
- **`ApplyDiffusion`/`DelayDiffuser`'s own internal delay lines**
  (`dsp/StereoField.hpp`). Out of scope: `StereoDelay::Process`'s capacity
  bound governs `lineL`/`lineR` only; the diffuser's sections are a
  fixed-length allpass cascade with their own (unaudited) internal buffers,
  not part of "the delay line" the task names.

## Exchange

Reproduced in a fresh scratch copy, `$S/exchange-b/app` (`.orig` saved to
`$S/exchange-b/saved/Delay.hpp.orig`), same compile/run commands as Control
above. One build at a time, binary `rm`'d before each rebuild.

### Finding U — `#define NDEBUG` before `#include <cassert>`

**CONFIRMED**, every reported number, by direct reproduction:

- Alone (`$S/exchange-b/saved/U-alone.diff`): `COMPILE_EXIT=0`, `RUN_EXIT=0`,
  `191/191 tests passed`; sweep gate `OK`, `SWEEP_EXIT=0`.
- `+ lineL.assign(capacity - 64, 0.0f)` (`Delay.hpp.U_plus_lineL`):
  `COMPILE_EXIT=0`, `RUN_EXIT=0`, `191/191 tests passed`.
- `+` width-balance term vanishing at the grid's three values (the
  break-proof table's own `off-grid-width-balance-headroom` perturbation):
  `COMPILE_EXIT=0`, `RUN_EXIT=0`, `191/191 tests passed`.
- Against `check_delay_capacity_break_proofs.py`, run on the **alone**
  header:
  ```
  PASS  modulation-bound-removed          exit=1
  PASS  width-bound-removed               exit=1
  PASS  modulation-dropped-from-width-budget exit=1
  PASS  wrap-admits-capacity              exit=1
  FAIL  lines-allocated-short             exit=0   reason: stayed green
  FAIL  off-grid-width-balance-headroom   exit=0   reason: stayed green
  PASS  capacity-headroom-shortens-reads  exit=1
  check-delay-capacity-break-proofs: FAIL   EXIT=1
  ```
  Exactly "2 of 7 stayed green," reproduced.

In scope: `#define NDEBUG` sits squarely inside "compile-gated compares
inside `Process`, `ReadAt` and `WriteSample`" — the third of the three
protection layers named for this audit.

**`make test` end-to-end: caught — but the "alone blocks nothing" half of
the claim is contradicted by the finding's own break-proof evidence.**
`app/Makefile`'s `test:` target lists `check-delay-capacity-break-proofs` as
a direct prerequisite, run before `$(DSP_TEST_BIN)`. I reran that gate
against the bare **alone** header (no `lineL`/off-grid change layered in)
and got the identical `2 of 7 stayed green` / `exit 1` shown above — the
gate's own internal `lines-allocated-short` and `off-grid-width-balance-headroom`
mutations already exercise the exact two shapes whose only detector is the
neutered asserts, with no additional break required from me. So under
`make test` as currently wired, the bare `#define NDEBUG` line **already
blocks delivery by itself**, before either combination is added. I then
reran the gate against both combinations directly (not synthesized):
`+lineL` fails with `lines-allocated-short` now erroring `old_text matched 0
times` (anchor drift, since I'd already changed that exact line) plus
`off-grid-width-balance-headroom` still `stayed green`, exit 1; `+offgrid`
fails symmetrically (`modulation-dropped-from-width-budget` and
`off-grid-width-balance-headroom` anchor-drift, `lines-allocated-short`
still `stayed green`), exit 1. So "the two combinations block delivery" is
confirmed too, via the same gate — but by the time either combination is
added, delivery was already blocked. Net: Finding U's raw numbers are all
correct; its two-part narrative ("alone blocks nothing; the combinations
block it") does not hold once wired into `make test`'s actual prerequisite
order — "alone" blocks it too, immediately, and demonstrably from the
finding's own reported break-proof run.

### Finding W — delete the compile-gated compares outright

**CONFIRMED**, with one precision correction: the three sites hold **five**
asserts, not three pairs — `Process` has a pair
(`timeL`/`timeR <= capacitySeconds`), `ReadAt` has a pair
(`idx0`/`idx1 < line.size()`), but `WriteSample` has a single assert
(`writePos < line.size()`), no partner. This doesn't change the substance of
the finding (all five compares are the thing under audit) but "three...
pairs" overstates `WriteSample`'s shape by one; noting it because the task
requires naming any defect in the evidence, and the diff itself
(`$S/exchange-b/saved/W.diff`) shows it plainly.

Reproduced exactly: `COMPILE_EXIT=0`, `RUN_EXIT=0`, `191/191 tests passed`.
Break-proof gate:
```
PASS  modulation-bound-removed          exit=1
PASS  width-bound-removed               exit=1
PASS  modulation-dropped-from-width-budget exit=1
PASS  wrap-admits-capacity              exit=1
FAIL  lines-allocated-short             exit=0   reason: stayed green
FAIL  off-grid-width-balance-headroom   exit=0   reason: stayed green
PASS  capacity-headroom-shortens-reads  exit=1
check-delay-capacity-break-proofs: FAIL   EXIT=1
```
5 PASS / 2 FAIL "stayed green", exit 1 — exactly as claimed. In scope for
the same reason as Finding U.

**Tested the extrapolation, not just the reported experiment.** Finding W
claims "a break not in the table would go undetected end to end." I built
that directly: took the assert-deleted header and additionally shortened
only `lineR`'s allocation (`lineR.assign(capacity - 32, 0.0f)` — a defect
shape the break-proof table never touches, since its `lines-allocated-short`
entry only ever mutates `lineL`; this is the same novel mutation as my own
Attack 4 above). Result: `COMPILE_EXIT=0`, `RUN_EXIT=0`, `191/191 tests
passed` — the parity suite alone genuinely misses it, confirming the
mechanism behind the claim. But `check_delay_capacity_break_proofs.py`
against this same combined header still reports `5 PASS / 2 FAIL exit 1`,
identical to W-alone — because that gate's failure is driven entirely by the
asserts being gone, independent of whatever else rides along with the
deletion. So the precise, evidence-backed statement is: the compiled
DSP-parity binary alone does let an untabled break through undetected, as
claimed; but `make test` as a whole does not let the same combined state
through end to end, because deleting the asserts is, by itself, already
sufficient for `check-delay-capacity-break-proofs` to fail and stop `make
test` before delivery — the novel defect rides along inside a build that
was already going to be rejected, not one that slips past every gate.
Finding W's "caught by that gate only" undersells this: the gate does not
catch the novel defect by name, but it does catch the precondition (assert
deletion) that made the novel defect possible, which is what actually
determines the `make test` outcome.

**Bottom line for both:** identical underlying signature (`lines-allocated-short`
and `off-grid-width-balance-headroom` are the only two break-proof entries whose
sole detector is the compile-gated compares; every other entry is independently
covered by the parity suite's own expected-vs-measured `REQUIRE`s). Both
attacks are real and both are in-scope. Neither gets through `make test`
end-to-end as currently wired, because `check-delay-capacity-break-proofs`
is a `test:` prerequisite and fails on the assert-neutralization itself —
this holds for the reported experiments and for the untabled defect I added
on top to test the generalization.
