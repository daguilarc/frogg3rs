# Adversarial postflight B — `dsp::StereoDelay` capacity guard

Scope: `dsp::StereoDelay::Process` (`app/dsp/Delay.hpp`) reads `lineL`/`lineR` at a lag
built from `baseSeconds` (Delay time), `modSeconds` (Mod depth × LFO), and `widthSpread`
(Stereo width × Width balance). That lag must never exceed `capacity` (the line's
allocated length). Derived directly from the code (not from any prior audit doc) as the
set of checks whose job is to reject a version of this mechanism that violates that bound:

1. **Compile-time-gated runtime assert**, inside `Process()` itself
   (`app/dsp/Delay.hpp`, guarded by `#if defined(FROGGERS_DSP_CHECKS)`):
   `assert(timeL <= capacitySeconds); assert(timeR <= capacitySeconds);`
   `app/Makefile` defines `FROGGERS_DSP_CHECKS` for every test/check binary and never
   passes `-DNDEBUG`, so the assert is live in the built test binary. It fires only for
   parameter combinations a caller actually drives through `Process()`.
2. **Behavioral parity tests**, `app/FroggersDspParityTests.cpp`:
   - `stereo_delay_width_spread_never_reads_past_the_line_capacity` — single point
     (dtim=0.99, dwid=1.0, wb=1.0, 48kHz), measures the actual read lag by impulse/peak
     detection on `wet.r` and asserts it is `<= capacity` (and `> capacity-100`).
   - `stereo_delay_width_spread_bound_is_inert_away_from_capacity` — single point
     (dtim=0.3), sanity companion.
   - `stereo_delay_width_spread_bound_holds_across_the_reachable_grid` — sweeps
     `dtim∈{0,0.5,0.9,0.99,1.0} × dwid∈{0,0.5,0.75,1.0} × wb∈{0,0.5,1.0} × dmod∈{0,1}`
     (120 points) at 96kHz, comparing a peak-detected measured lag on `wet.r` against an
     independently-computed expected lag, `REQUIRE_NEAR` with 5-sample tolerance (widened
     to `maxModSeconds*sr+5` only when `dwid==0 || wb==0`).
   All three measure only the **right channel** (`wet.r`); none inspect `wet.l` directly.
   Structurally this is safe only because `timeR = baseSeconds+modSeconds+widthSpread >=
   timeL = baseSeconds+modSeconds` whenever `widthSpread>=0`, which holds for `dwid,
   widthBalance >= 0` in the unmutated code.
3. **Static test-coverage gate**, `check_delay_capacity_parameters_are_swept.py`, wired
   into `app/Makefile`'s `test` target. Finds every `TEST_CASE` body containing
   `"capacity"` (case-insensitive) **and** `SetWidthBalance` (its definition of
   "capacity-surface" test), then regex-extracts literal values assigned to `p.dtim`,
   `p.dwid`, `SetWidthBalance(...)`/`widthBalance`, `p.dmod`, and `SetSampleRate`/`sr`
   across those bodies, and fails if any tracked parameter takes only one distinct value.
   It inspects only the **text of the test file** — it never compiles or runs
   `Delay.hpp`, so no mutation confined to `Delay.hpp` can be seen by it at all.

All three build from `app/Makefile`'s `$(DSP_TEST_BIN)` rule:
`clang++ -I<app> -I<Sheaf synth>/include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra
-Wpedantic -O2 FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests`.

Working copy: `$S/attack-b/app` (`$S` = the scratchpad directory below), copied from
`$S/final-app` per the task's setup and never written back to the real repo.

```
S=/private/tmp/claude-501/-Users-diegoaguilar-canabal-Desktop/68105815-8b90-42de-aa6a-51f093984b11/scratchpad
```

## Control run

```
cd $S/attack-b/app && rm -f build/froggers_dsp_parity_tests && \
clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include \
  -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2 \
  FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
./build/froggers_dsp_parity_tests
```
Compile: exit 0, no warnings. Run: exit 0.
```
190/190 tests passed
```
`python3 check_delay_capacity_parameters_are_swept.py $S/attack-b/app` on the unmodified
tree: exit 0, `OK -- every tracked parameter takes more than one value across the
capacity-surface checks`. Instrument is live.

## Attack 1 — off-grid Width balance, GOT THROUGH

Every literal `SetWidthBalance(...)` call and every `wbs[]`/`wb` value anywhere in
`FroggersDspParityTests.cpp` is `0.0`, `0.5`, or `1.0` (checked by grepping every
`p\.dtim\s*=|p\.dwid\s*=|p\.dmod\s*=|SetWidthBalance\(` occurrence in the file — 3
distinct Width-balance literals total, in that exact set). The mutation adds a "headroom
margin" to `maxSpreadSeconds` that is an exact cubic in `widthBalance` with roots at
0, 0.5 and 1.0 — i.e. it is *exactly* zero at every Width-balance value the suite ever
drives, and positive everywhere else, loosening the clamp precisely in the gap between
tested grid points.

Diff (`$S/attack-b/attack1.diff`, against `app/dsp/Delay.hpp`):
```diff
--- a/app/dsp/Delay.hpp
+++ b/app/dsp/Delay.hpp
@@ -695,7 +695,12 @@
         // keeps it in bounds, against whatever of the line's capacity the
         // base and modulation terms above have not already spent.
         const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
-        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
+        // Small headroom margin so a Width balance setting that sits
+        // between the knob's calibration points (0, 0.5, 1.0) is not
+        // starved of spread by rounding in the subtraction above.
+        const float wbMargin = widthBalance * (widthBalance - 0.5f) * (widthBalance - 1.0f);
+        const float maxSpreadSeconds =
+            std::max(0.0f, capacitySeconds - baseSeconds - modSeconds) + std::fabs(wbMargin) * capacitySeconds;
         const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
         float timeL = std::max(0.001f, baseSeconds + modSeconds);
         float timeR = std::max(0.001f, baseSeconds + modSeconds + widthSpread);
```

Command:
```
cd $S/attack-b/app && rm -f build/froggers_dsp_parity_tests && \
clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include \
  -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2 \
  FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
./build/froggers_dsp_parity_tests
python3 check_delay_capacity_parameters_are_swept.py $S/attack-b/app
```
Output:
```
COMPILE_EXIT=0
190/190 tests passed
```
(190 `[PASS]`, 0 `[FAIL]`, run exit 0 — includes
`[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid`.)
```
check-delay-capacity-parameters-are-swept: capacity-surface checks found: stereo_delay_width_spread_never_reads_past_the_line_capacity, stereo_delay_width_spread_bound_is_inert_away_from_capacity, stereo_delay_width_spread_bound_holds_across_the_reachable_grid
  sample rate: [48000.0, 96000.0]
  Delay time (p.dtim): [0.0, 0.3, 0.5, 0.9, 0.99, 1.0]
  Stereo width (p.dwid): [0.0, 0.5, 0.75, 1.0]
  Width balance: [0.0, 0.5, 1.0]
  Mod depth (p.dmod): [0.0, 1.0]
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
```
All three checks accept the mutated `Delay.hpp` unmodified.

**The violation is real**, demonstrated with a standalone probe outside the gate
(`$S/attack-b/probe1.cpp`, included below in full since it is short) that calls the same
mutated `StereoDelay::Process` at `dtim=0.99, dwid=1.0` (both grid values) with
`widthBalance=0.75` (never used anywhere in the suite):

```cpp
// $S/attack-b/probe1.cpp
#include "dsp/Delay.hpp"
#include <cstdio>

int main() {
    using namespace synth_froggers::dsp;
    const float sr = 48000.0f;
    StereoDelay delay;
    delay.SetSampleRate(sr);

    DelayParams p;
    p.dsnd = 1.0f;
    p.dmix = 1.0f;
    p.dtim = 0.99f;
    p.dwid = 1.0f;
    p.dmod = 0.0f;
    const float wb = 0.75f;
    delay.SetWidthBalance(wb);

    const float capacitySeconds = 2.0f;
    const float baseSeconds = ExpMapCompute(0.001f, 2.0f, p.dtim);
    const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * wb;
    const float maxSpreadSeconds_correct = std::max(0.0f, capacitySeconds - baseSeconds);
    const float wbMargin = wb * (wb - 0.5f) * (wb - 1.0f);
    const float maxSpreadSeconds_mutated = maxSpreadSeconds_correct + std::fabs(wbMargin) * capacitySeconds;
    const float widthSpread_mutated = std::min(widthSpreadRaw, maxSpreadSeconds_mutated);
    const float timeR_mutated = baseSeconds + widthSpread_mutated;

    std::printf("baseSeconds=%.6f capacitySeconds=%.6f\n", baseSeconds, capacitySeconds);
    std::printf("widthSpreadRaw=%.6f\n", widthSpreadRaw);
    std::printf("maxSpreadSeconds correct=%.6f mutated=%.6f\n", maxSpreadSeconds_correct, maxSpreadSeconds_mutated);
    std::printf("wbMargin=%.6f fabs*capacitySeconds=%.6f\n", wbMargin, std::fabs(wbMargin) * capacitySeconds);
    std::printf("timeR_mutated=%.6f  (exceeds capacitySeconds by %.6f s = %.1f samples)\n",
                 timeR_mutated, timeR_mutated - capacitySeconds, (timeR_mutated - capacitySeconds) * sr);
    std::printf("timeR_mutated <= capacitySeconds ? %s\n",
                 (timeR_mutated <= capacitySeconds) ? "true" : "FALSE -- assert(timeR <= capacitySeconds) would fire if this combo were ever run under FROGGERS_DSP_CHECKS");

    const size_t capacity = 96000;
    for (size_t i = 0; i < capacity + 8; ++i) delay.Process(0.0f, p);
    size_t peakIndex = 0; float peakAbs = 0.0f; const size_t impulseIndex = 1000;
    for (size_t i = 0; i < capacity + 5000; ++i) {
        const float bumpIn = (i == impulseIndex) ? 1.0f : 0.0f;
        const DelayWetPair wet = delay.Process(bumpIn, p);
        if (i > impulseIndex && std::fabs(wet.r) > peakAbs) { peakAbs = std::fabs(wet.r); peakIndex = i; }
    }
    const long long lagSamples = static_cast<long long>(peakIndex) - static_cast<long long>(impulseIndex);
    std::printf("peakAbs=%.6f lagSamples=%lld capacitySamples=%zu\n", peakAbs, lagSamples, capacity);
    return 0;
}
```

Commands and literal output:
```
cd $S/attack-b/app
clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -std=c++20 -O2 ../probe1.cpp -o build/probe1_nochecks
./build/probe1_nochecks
```
```
baseSeconds=1.853616 capacitySeconds=2.000000
widthSpreadRaw=0.486574
maxSpreadSeconds correct=0.146384 mutated=0.240134
wbMargin=-0.046875 fabs*capacitySeconds=0.093750
timeR_mutated=2.093750  (exceeds capacitySeconds by 0.093750 s = 4500.0 samples)
timeR_mutated <= capacitySeconds ? FALSE -- assert(timeR <= capacitySeconds) would fire if this combo were ever run under FROGGERS_DSP_CHECKS
peakAbs=0.797590 lagSamples=4500 capacitySamples=96000
```
`timeR` is 2.09375s against a 2.0s capacity: `ReadAt`'s modulo silently wraps the
over-capacity request, so the peak-detected lag reads back as **4500 samples — the wrong,
short, wrapped lag** the header comment itself warns `ReadAt` produces on overflow, not
the ~100,500-sample lag the knobs actually asked for.

Confirming the assert mechanism itself is sound and would have caught this if any test
exercised it:
```
clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -DFROGGERS_DSP_CHECKS -std=c++20 -O2 ../probe1.cpp -o build/probe1_withchecks
./build/probe1_withchecks; echo EXIT=$?
```
```
Assertion failed: (timeR <= capacitySeconds), function Process, file Delay.hpp, line 720.
EXIT=134
```

**What breaks:** at Width balance settings the shipped Delay page's knob can reach but the
suite never samples (e.g. 0.75), `StereoDelay::Process` reads the delay line at a lag up
to ~94ms past the line's capacity, silently wrapping (via `ReadAt`'s own documented
modulo fallback) to a short, wrong echo instead of the requested one — audible, silent
mis-tracking, no crash. Blocks **delivery**: the three checks that exist specifically to
reject this class of defect (assert, three parity tests, coverage gate) all accept the
mutated `Delay.hpp` unmodified; nothing in the gate would stop this shipping.

## Attack 2 — classic dropped-subtraction bug, CAUGHT (by the assert)

Control: drop the `- modSeconds` term from `maxSpreadSeconds` (the exact defect class the
suite's own `dtim=0.9` grid point and its header comment say they exist to catch).
Diff (`$S/attack-b/attack2.diff`):
```diff
-        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
+        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds);
```
Build/run: compile exit 0; run exit 134.
```
Assertion failed: (timeR <= capacitySeconds), function Process, file Delay.hpp, line 715.
```
Caught before any `REQUIRE` even runs (aborts mid-suite, at grid point
`dtim=0.5, dwid=0.75, wb=0, dmod=1`).

## Attack 3 — same bug, plus disabling the assert via `#define NDEBUG`, CAUGHT (by REQUIRE_NEAR)

Same dropped-subtraction bug as Attack 2, combined with forcing `NDEBUG` ahead of
`<cassert>` inside `Delay.hpp` itself, to see whether neutralizing check #1 lets the
classic bug through checks #2/#3.
Diff (`$S/attack-b/attack3.diff`):
```diff
 #include <algorithm>
 #include <array>
+#ifndef NDEBUG
+#define NDEBUG
+#endif
 #include <cassert>
 #include <cmath>
@@
-        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
+        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds);
```
Build/run: compile exit 0 (no warning about the redefinition); run exit 1.
```
[FAIL] stereo_delay_width_spread_bound_holds_across_the_reachable_grid: FroggersDspParityTests.cpp:10713 requirement failed: static_cast<float>(lagSamples) (6216) ~= expectedLagSamples (96000), eps=5
189/190 tests passed
```
Disabling the compile-time assert does not help this class of bug get through: the
independent behavioral `REQUIRE_NEAR` in the grid-sweep test still catches it, at grid
point `dtim=0.9, dwid=0.5, wb=0.5, dmod=1` (measured lag collapses to 6216 samples against
an expected 96000).

## Accepting paths enumerated and not attacked

- **`p.dsnd <= 0.0001f` early return** (`Process()` top): skips the entire
  base/mod/width capacity computation and returns `{}` before computing `timeL`/`timeR`.
  Not attacked: every capacity-surface test and the grid sweep pin `dsnd=1.0`/`p.dmix=1.0`
  so this branch is simply not on the path any of the three checks exercise; exploiting it
  would require also changing which `dsnd` value ships, which is a caller/parameter-model
  concern outside `Delay.hpp`.
- **`capacity == 0` early return**: only reachable before `SetSampleRate` has ever run.
  Every test and every shipping call path calls `SetSampleRate` first; not a route to a
  reachable violation.
- **`wet.r`-only measurement in all three capacity-surface tests** (never inspects
  `wet.l`): explored at length. In the unmutated formula `timeR >= timeL` always, because
  `widthSpread = min(widthSpreadRaw, maxSpreadSeconds) >= 0` whenever `dwid, widthBalance
  >= 0`, so checking R structurally covers L. Attack 1 already demonstrates the general
  class of defect this asymmetry permits (an off-grid-parameter loosened clamp); a
  dedicated "break L only" mutation would have to also break that `timeR>=timeL`
  invariant, which is a strictly harder version of the same finding, not a new one.
- **`ApplyReverse`'s own read position** (`(writePosF - rev.pos)/sampleRate` fed to
  `ReadAt`) and **`ApplyDiffusion`'s internal allpass state**: neither the assert (which
  only names `timeL`/`timeR`, the forward tap) nor the python gate's "capacity-surface"
  test-body heuristic touches either. Investigated whether breaking the reverse tap's
  periodic wrap-reset (its own `rev.elapsed >= delaySamplesWindow - fadeSamples` trigger)
  produces an unguarded capacity violation. It does not: `ReadAt`'s `% capacity` makes
  every read memory-safe and the header's own derivation shows `readPos == rev.pos (mod
  capacity)` always, independent of how large the nominal "seconds" argument grows — so
  disabling the reverse tap's re-arm changes its behavior (an unbounded backward scrub
  instead of a repeating one-window sweep) without ever asking `ReadAt` to exceed
  `capacity` in the sense the task's target describes (`timeL`/`timeR` summed from
  knob-driven terms). Not pursued as a capacity-bound finding; flagged only as a
  structurally different, unguarded mechanism worth a human decision on whether it's
  in scope.
- **The python gate's own "capacity-surface" text heuristic** (`"capacity" in body and
  "SetWidthBalance" in body`): could in principle be defeated by editing the *test file*
  (e.g. renaming a check so it no longer matches, or wording around the heuristic) — but
  that attacks the test suite's coverage bookkeeping, not `dsp::StereoDelay`, and every
  attack above left `FroggersDspParityTests.cpp` untouched by design (the task scopes the
  target to `Delay.hpp`). Noted, not attacked.
- **Negative `modSeconds`** (`sin(lfoPhase) < 0`): only ever shrinks
  `baseSeconds+modSeconds`, never grows it past capacity; wrong direction for this defect
  class, not attacked.

## Reproduction

All commands above are literal and were run from `$S/attack-b/app` after:
```
S=/private/tmp/claude-501/-Users-diegoaguilar-canabal-Desktop/68105815-8b90-42de-aa6a-51f093984b11/scratchpad
mkdir -p $S/attack-b && cp -R $S/final-app $S/attack-b/app && mkdir -p $S/attack-b/app/build
```
Diffs are saved at `$S/attack-b/attack1.diff`, `$S/attack-b/attack2.diff`,
`$S/attack-b/attack3.diff`; originals at `$S/attack-b/app/dsp/Delay.hpp.orig`,
`.orig2`, `.orig3`; the probe at `$S/attack-b/probe1.cpp`; full logs under
`$S/attack-b/app/build/*.log`.

## Exchange

Four anonymized findings from other attackers, each reproduced independently in a fresh
`$S/attack-b/finding<X>/app` copy (one build at a time, binary `rm`'d before each
rebuild).

### Finding P — `WrapIndex`'s `while (idx >= capacity)` → `while (idx > capacity)`

CONFIRMED. Patched only `WrapIndex`'s loop condition. Built/ran the unmodified test
binary:
```
cd $S/attack-b/findingP/app && rm -f build/froggers_dsp_parity_tests && \
clang++ -I. -I.../External/Sheaf/projects/synth/include -DFROGGERS_DSP_CHECKS \
  -std=c++20 -Wall -Wextra -Wpedantic -O2 FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
./build/froggers_dsp_parity_tests
```
```
[FAIL] stereo_delay_freeze_releasing_the_latch_after_running_hot_decays_toward_silence: FroggersDspParityTests.cpp:8153 requirement failed: peakDuringLatch > 0.5f
189/190 tests passed
```
All three capacity checks print `[PASS]`/`OK`; no `Assertion failed` line anywhere; the
suite exits 1 solely on the unrelated freeze-latch test — exactly as claimed. Also
rebuilt (binary `rm`'d first) with a one-line bounds check inserted right after
`WrapIndex(idx0 + 1)` in `ReadAt` (`std::fprintf`+`std::abort` if `idx1 >= line.size()`),
since AddressSanitizer would not initialize in this sandbox
(`AddressSanitizer: CHECK failed: sanitizer_malloc_mac.inc:189 "((!asan_init_is_running)) != (0)"`,
an environment limitation, not evidence either way). Literal output:
```
OOB idx1=96000 line.size()=96000 capacity=96000
```
i.e. `line[idx1]` is indexed at exactly `line.size()`, one past the end — a real
one-past-the-end read, `std::vector::operator[]` UB — firing during an unrelated Reverb
test, well before any Delay capacity test runs.
Inside/outside: **outside** the checks' specific job. The three named checks bound the
requested *lag in seconds/samples* against `capacitySeconds`/`capacity`; they say nothing
about `WrapIndex`'s own index arithmetic, which is wrong regardless of whether the
requested lag is anywhere near capacity — it fires on ordinary, well-within-bounds
operation the moment the read lands on the newest committed sample. A real, severe
memory-safety defect in the same file, but not an instance of "a broken read-lag
mechanism getting past the capacity checks."

### Finding Q — `SetSampleRate` allocates `lineL`/`lineR` 64 samples short of `capacity`

CONFIRMED. Patched only `lineL.assign(capacity, ...)`/`lineR.assign(capacity, ...)` to
`capacity - 64`. Built/ran:
```
190/190 tests passed
```
exit 0, gate `OK`, no `Assertion failed` line — matches the claim exactly. Rebuilt (binary
`rm`'d first) with a bounds check in `WriteSample` (`std::abort` if
`writePos >= line.size()`); literal output:
```
OOB write writePos=95936 line.size()=95936 capacity=96000
```
An unconditional out-of-bounds **write** (not just read) at `line.size()`, firing during
an unrelated Limiter test, 64 samples inside every lap.
Inside/outside: **outside**. Same reasoning as P — the assert and the parity tests both
compare quantities derived from the *same* `capacity` variable and are self-consistent;
neither checks that `capacity` still matches the real allocated buffer length. A different
invariant (`capacity == lineL.size()`) than the one under audit, not a violation of the
lag-vs-capacity bound itself, though it defeats the entire guard by corrupting the ruler
the guard measures against.

### Finding S — test file only: `dmod` sweep trimmed to `{0.0f}`, gate fooled by a comment

CONFIRMED. Patched only
`const float dmods[] = {0.0f, 1.0f};` → `const float dmods[] = {0.0f};  // dmod = 1.0f also swept historically`,
`Delay.hpp` untouched.
```
python3 check_delay_capacity_parameters_are_swept.py $S/attack-b/findingS/app
```
```
Mod depth (p.dmod): [0.0, 1.0]
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
```
Gate reports two values from a body that now only exercises one — because
`literal_values_for` regexes raw `TEST_CASE` body text (comments included, per
`find_test_case_bodies`'s brace-matched slice) rather than code with comments stripped.
Build/run: `190/190 tests passed`, exit 0. Matches the claim exactly.
Inside/outside: **inside** the job of the coverage-sweep gate specifically — its stated
purpose (per its own header comment) is exactly "fails when the capacity-surface checks
… pin some shipping parameter to one value," and this defeats that purpose without
changing coverage in reality. It does not itself move any DSP bound, so — as claimed —
it blocks neither today, but it silently disarms future defect-detection on the `dmod`
axis.

### Finding T — four coordinated edits (width clamp, `timeR` assert, single-point floor, grid tolerance)

CONFIRMED. Applied all four, each individually small: `maxSpreadSeconds = capacitySeconds`
(ignoring `baseSeconds`/`modSeconds`); deleted `assert(timeR <= capacitySeconds);` only
(kept the `timeL` assert); `REQUIRE_TRUE(lagSamples > static_cast<long long>(capacity) - 100 || true);`;
`const float tolerance = static_cast<float>(capacity);` in the grid sweep. Built/ran:
```
[width spread grid sweep] dtim=1 dwid=1 widthBalance=1 dmod=1 expected=96000 measured=33600
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
190/190 tests passed
```
gate `OK`. Matches the claim's numbers exactly (measured lag at dtim=1/dwid=1/wb=1 is
33600 samples against a 96000-sample request). Traced why: at that point the real
requested lag is `baseSeconds(1.0s) + widthSpread(0.35s) = 1.35s = 129600 samples`
against `capacity=96000` — a genuine 35%-over-capacity read that `ReadAt`'s modulo wraps
down to `129600 mod 96000 = 33600`, the observed "short" value. `timeL` alone (`1.0s`,
exactly `capacitySeconds`) never exceeds its own assert, so the kept `timeL`-only assert
is structurally blind to a violation that lives entirely in the width term. Confirms "no
single edit works alone": the surviving `timeL` assert requires deleting the width-clamp
subtraction's effect from `timeR` specifically or defeating both dedicated tests, which is
why all four edits are load-bearing together.
Inside/outside: **inside** — this is precisely the target mechanism (`Process`'s
forward-tap lag exceeding `capacity`) getting past all three named checks at once. Blocks
delivery.
