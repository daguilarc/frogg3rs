# Adversarial postflight C -- dsp::StereoDelay capacity guard

Scratch working copy:
```
S=/private/tmp/claude-501/-Users-diegoaguilar-canabal-Desktop/68105815-8b90-42de-aa6a-51f093984b11/scratchpad
mkdir -p $S/attack-c && cp -R $S/final-app $S/attack-c/app && mkdir -p $S/attack-c/app/build
```
All commands below run from `$S/attack-c/app` against that copy. The real repo
at `/Users/diegoaguilar-canabal/Desktop/frogg3rs` was never written to.

Build command (matches `app/Makefile`'s `$(DSP_TEST_BIN)` rule: `CPPFLAGS :=
-I$(SHEAF_SYNTH_DIR)/include -DFROGGERS_DSP_CHECKS`, `CXXFLAGS ?= -std=c++20
-Wall -Wextra -Wpedantic -O2`, no `-DNDEBUG` anywhere, so `assert()` is live):
```
rm -f build/froggers_dsp_parity_tests
clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include \
  -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2 \
  FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
./build/froggers_dsp_parity_tests
```
Python gate:
```
python3 check_delay_capacity_parameters_are_swept.py $S/attack-c/app
```

## The mechanism and its checks

`dsp::StereoDelay::Process` (`app/dsp/Delay.hpp`) builds `timeR = baseSeconds +
modSeconds + widthSpread`, where each term is clamped in order against what
the terms before it have not already spent:
- `baseSeconds = min(baseSecondsRaw, capacitySeconds)`
- `modSeconds = min(modSecondsRaw, capacitySeconds - baseSeconds)`
- `widthSpread = min(widthSpreadRaw, max(0, capacitySeconds - baseSeconds - modSeconds))`

`timeL = baseSeconds + modSeconds` (never carries `widthSpread`). Three
mechanisms exist to reject a version of this that lets `timeL`/`timeR` exceed
`capacitySeconds`:

1. **Compile-time check inside `Process` itself** (Delay.hpp:713-716):
   `#if defined(FROGGERS_DSP_CHECKS) assert(timeL <= capacitySeconds);
   assert(timeR <= capacitySeconds); #endif`. Runs on every call to
   `Process`, from all 190 test cases in the suite, not just the
   delay-specific ones -- its accepting path is anything that keeps both
   inequalities true, or a build that doesn't define
   `FROGGERS_DSP_CHECKS`/defines `NDEBUG` (the Makefile's `DSP_TEST_BIN`
   target does neither, so both asserts are live in every build this record
   exercises).
2. **Three `TEST_CASE`s in `app/FroggersDspParityTests.cpp`** that measure the
   *actual* read lag by firing an impulse through real `Process()` calls and
   locating the peak in the output (not by re-deriving the bound and
   checking it against itself):
   - `stereo_delay_width_spread_never_reads_past_the_line_capacity` (line
     10492): sr=48000, dtim=0.99, dwid=1.0, wb=1.0. Accepts if
     `lagSamples <= capacity` AND `lagSamples > capacity - 100`.
   - `stereo_delay_width_spread_bound_is_inert_away_from_capacity` (line
     10534): sr=48000, dtim=0.3 (away from the edge). Accepts if the
     measured lag matches the unclamped formula within 1 sample.
   - `stereo_delay_width_spread_bound_holds_across_the_reachable_grid` (line
     10580): sr=96000, full cross product dtim x {0,.5,.9,.99,1} dwid x
     {0,.5,.75,1} wb x {0,.5,1} dmod x {0,1} (120 points). Accepts if
     measured lag matches an independently-recomputed expected value within
     5 samples (widened to `maxModSeconds*sr+5` when dwid==0 or wb==0, the
     two settings that make the width term inert by construction).
3. **`check_delay_capacity_parameters_are_swept.py`**, wired into
   `app/Makefile`'s `test` target. It does not run anything -- it greps
   `FroggersDspParityTests.cpp` for `TEST_CASE` bodies containing both
   "capacity" and "SetWidthBalance" (its mechanical stand-in for "this is a
   capacity-surface check"), then for each of five tracked parameters
   (sample rate, `p.dtim`, `p.dwid`, Width balance, `p.dmod`) collects every
   literal value assigned to it, passed to a `SetXxx(...)` call, or present
   in a same-named array literal (`dtims[] = {...}`), across the union of
   those bodies. It accepts if every parameter's collected value set has
   more than one distinct member -- a purely textual, un-executed scan.

## Control

Unmodified copy, DSP parity suite:
```
190/190 tests passed
EXIT=0
```
Capacity tests individually:
```
[PASS] stereo_delay_width_spread_never_reads_past_the_line_capacity
[PASS] stereo_delay_width_spread_bound_is_inert_away_from_capacity
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
```
Python gate:
```
check-delay-capacity-parameters-are-swept: capacity-surface checks found: stereo_delay_width_spread_never_reads_past_the_line_capacity, stereo_delay_width_spread_bound_is_inert_away_from_capacity, stereo_delay_width_spread_bound_holds_across_the_reachable_grid
  sample rate: [48000.0, 96000.0]
  Delay time (p.dtim): [0.0, 0.3, 0.5, 0.9, 0.99, 1.0]
  Stereo width (p.dwid): [0.0, 0.5, 0.75, 1.0]
  Width balance: [0.0, 0.5, 1.0]
  Mod depth (p.dmod): [0.0, 1.0]
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
GATE_EXIT=0
```
This confirms both instruments are live before any attack.

## Attack 1 -- drop both prior terms from the width-spread remainder (Delay.hpp)

Removes the "ordered subtraction" entirely: widthSpread is clamped against
raw `capacitySeconds` instead of what `baseSeconds`+`modSeconds` already
spent.

```diff
--- Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -695,7 +695,7 @@
         // keeps it in bounds, against whatever of the line's capacity the
         // base and modulation terms above have not already spent.
         const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
-        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
+        const float maxSpreadSeconds = capacitySeconds;
         const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
         float timeL = std::max(0.001f, baseSeconds + modSeconds);
         float timeR = std::max(0.001f, baseSeconds + modSeconds + widthSpread);
```
Command: the build+run command above, unmodified.
Output:
```
EXIT=134
...
  [peak trim duty]     liveness: steady-tone reduction trim=2.17407905Assertion failed: (timeR <= capacitySeconds), function Process, file Delay.hpp, line 715.
```
190 tests never all ran -- the process aborted (SIGABRT) partway through an
unrelated filter-bank test (`filter_bank_peak_trim_removal_...`, line 437 of
the log is its own `[PASS]`; the very next `Process()` call, made by some
later-registered `TEST_CASE`, is what trips the assert), long before
execution reaches any of the three capacity-named tests near the end of the
file. **Caught** by check 1, well outside the tests that were written to
target this bound. Blocks execution outright (nonzero exit / crash, not a
graceful `[FAIL]`).

## Attack 2 -- disable the one assert the bug affects, leave its twin (Delay.hpp)

Same clamp break as Attack 1, but this time also comment out only
`assert(timeR <= capacitySeconds)` (the check that watches exactly the
quantity `widthSpread` feeds) while leaving `assert(timeL <= capacitySeconds)`
live -- `timeL` never carries `widthSpread`, so its assert is structurally
blind to this bug. Tests whether check 2 (the black-box REQUIRE assertions)
is a genuinely independent net or was riding on check 1 catching things
first.

```diff
--- Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -695,7 +695,7 @@
         const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
-        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
+        const float maxSpreadSeconds = capacitySeconds;
         const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
@@ -712,7 +712,9 @@
 #if defined(FROGGERS_DSP_CHECKS)
         assert(timeL <= capacitySeconds);
-        assert(timeR <= capacitySeconds);
+        // timeR's own bound is exercised by the width-spread capacity-surface
+        // tests below; redundant with timeL's check under ordinary operation.
+        // assert(timeR <= capacitySeconds);
 #endif
```
Command: the build+run command above, unmodified.
Output:
```
EXIT=1
  [width spread grid sweep] dtim=0.899999976 dwid=0.5 widthBalance=0.5 dmod=0 expected=96000 measured=1640
[FAIL] stereo_delay_width_spread_bound_holds_across_the_reachable_grid: FroggersDspParityTests.cpp:10713 requirement failed: static_cast<float>(lagSamples) (1640) ~= expectedLagSamples (96000), eps=5
188/190 tests passed
```
and, earlier in the same run:
```
[FAIL] stereo_delay_width_spread_never_reads_past_the_line_capacity: FroggersDspParityTests.cpp:10527 requirement failed: lagSamples > static_cast<long long>(capacity) - 100
```
**Caught** by check 2 (two of the three capacity `TEST_CASE`s fail on
measured behavior), with check 1 silenced for exactly the channel the bug
lives on. Binary completes and reports `188/190`, exit 1 -- blocks `make
test`/delivery, not execution of the binary itself.

## Attack 3 -- decoy literal defeats the python gate's coverage scan (FroggersDspParityTests.cpp)

Reintroduces the exact historical regression the gate's own docstring names
("Mod depth pinned at 0") by trimming the grid test's real `dmod` sweep to a
single value, then plants a comment containing the missing literal so the
gate's regex still finds two. Delay.hpp is untouched -- this is a pure
test/gate attack, no production capacity bug is introduced.

```diff
--- FroggersDspParityTests.cpp.orig
+++ FroggersDspParityTests.cpp
@@ -10629,7 +10629,10 @@
     const float dtims[] = {0.0f, 0.5f, 0.9f, 0.99f, 1.0f};
     const float dwids[] = {0.0f, 0.5f, 0.75f, 1.0f};
     const float wbs[] = {0.0f, 0.5f, 1.0f};
-    const float dmods[] = {0.0f, 1.0f};
+    // Mod depth grid trimmed to its registered default; full-travel
+    // behavior (dmod = 1.0f) is covered by the reverse-blend and diffusion
+    // suites' own sweeps at high modulation depth.
+    const float dmods[] = {0.0f};
```
Command:
```
python3 check_delay_capacity_parameters_are_swept.py $S/attack-c/app
```
Output:
```
  Mod depth (p.dmod): [0.0, 1.0]
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
GATE_EXIT=0
```
The gate reports the same "OK" and the same `[0.0, 1.0]` set as the control,
even though the loop that actually drives `p.dmod` into `Process()` now only
ever passes `0.0f` -- the `1.0` it counts comes entirely from the comment's
prose, never from an executed call. Compiling and running the suite confirms
the regression is real and silent:
```
190/190 tests passed
EXIT=0
```
**Got through.** Neither the gate nor the C++ suite flags anything, because
nothing in this attack is behaviorally wrong yet -- it just deletes the
grid's only nonzero-`dmod` coverage while leaving the gate's textual receipt
for it intact. Blocks neither execution nor delivery today; what it breaks is
the gate's actual job -- the exact class of regression
(check_delay_capacity_parameters_are_swept.py's own header names three prior
incidents of it) can now land in `p.dmod`'s handling with the gate reporting
green, and won't be caught until whatever exercises it in the field.

## Attack 4 -- full silent defeat: break, then blind every check that would catch it (Delay.hpp + FroggersDspParityTests.cpp)

Combines the Attack-1 clamp break, the Attack-2 assert silencing, and two
targeted weakenings of the REQUIRE bodies the earlier attacks' failures
landed on (the non-degenerate lag floor in the single-point test, and the
tight-tolerance branch in the grid test) -- four small, individually
plausible-looking edits across the two files, needed together because each
one alone left a different check still catching the bug (Attacks 1 and 2
above).

```diff
--- Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -695,7 +695,7 @@
         const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
-        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
+        const float maxSpreadSeconds = capacitySeconds;
         const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
@@ -712,7 +712,7 @@
 #if defined(FROGGERS_DSP_CHECKS)
         assert(timeL <= capacitySeconds);
-        assert(timeR <= capacitySeconds);
+        // assert(timeR <= capacitySeconds);
 #endif
```
```diff
--- FroggersDspParityTests.cpp.orig
+++ FroggersDspParityTests.cpp
@@ -10524,7 +10524,7 @@
-    REQUIRE_TRUE(lagSamples > static_cast<long long>(capacity) - 100);
+    REQUIRE_TRUE(lagSamples > static_cast<long long>(capacity) - 100 || true);  // wraparound edge case, see width-balance grid test
@@ -10709,7 +10709,7 @@
-                    const float tolerance = (dwid == 0.0f || wb == 0.0f) ? (maxModSeconds * sr + 5.0f) : 5.0f;
+                    const float tolerance = (dwid == 0.0f || wb == 0.0f) ? (maxModSeconds * sr + 5.0f) : (capacity * 1.0f);
                     REQUIRE_NEAR(static_cast<float>(lagSamples), expectedLagSamples, tolerance);
```
Command: the build+run command above, unmodified, plus the gate command.
Output:
```
EXIT=0
  [width spread grid sweep] dtim=1 dwid=1 widthBalance=1 dmod=0 expected=96000 measured=33600
  [width spread grid sweep] dtim=1 dwid=1 widthBalance=1 dmod=1 expected=96000 measured=33600
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
190/190 tests passed
```
```
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
GATE_EXIT=0
```
**Got through, completely.** At dtim=1, dwid=1, widthBalance=1 -- all
reachable from the shipping UI -- the real, compiled `Process()` now asks
`ReadAt` for a lag 33600 samples (0.35s at 96kHz) short of the 96000 samples
(1.0s = full capacity) the knobs request, i.e. `ReadAt`'s unguarded `%
capacity` has silently wrapped a request that overruns the line by 0.7s into
a wrong-but-plausible-looking short echo, and both the compile-time check and
the black-box measurement test print `[PASS]`. This is the finding: no
single edit among these four defeats every check by itself (Attacks 1 and 2
show two of them individually still catch this exact clamp break), but the
four checks are not defense-in-depth against a determined multi-step
regression landing as separate, each-plausible-on-its-own review comments --
together they fully blind the mechanism. Blocks neither execution nor
delivery; the capacity violation this whole check family exists to prevent
now ships silently.

## Attack 5 -- drop only the modSeconds term from the remainder (Delay.hpp)

Enumerated as a candidate (the grid test's own header comment names dtim=0.9
as chosen specifically to catch "the width bound's ordered subtraction...
distinguishable from (capacitySeconds - baseSeconds) alone") but not assumed
correct without running it, per the same "claims travel with output" rule
this record is written under.

```diff
--- Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -695,7 +695,7 @@
         const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
-        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
+        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds);
         const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
```
Command: the build+run command above, unmodified, asserts and tests intact.
Output:
```
EXIT=134
476:  [width spread grid sweep] dtim=0.5 dwid=0.75 widthBalance=0 dmod=1 expected=4636.68018 mAssertion failed: (timeR <= capacitySeconds), function Process, file Delay.hpp, line 715.
```
**Caught** by check 1, inside the grid test itself this time (dtim=0.5, not
even the dtim=0.9 point the test's own comment calls out -- the defect is
broader than the one point the author aimed at).

## Accepting paths enumerated and not attacked

- `p.dsnd <= 0.0001f || capacity == 0` early return (Delay.hpp:659): skips
  every capacity computation and returns `lastWet = {}` without ever calling
  `ReadAt`. Not attacked -- there is no read to overrun on this path, so it
  cannot itself produce a capacity violation; it can only suppress detection
  of one that happens elsewhere, and every attack above reaches Process
  through callers with `dsnd` nonzero.
- `#if defined(FROGGERS_DSP_CHECKS)` being absent from shipping builds
  (browser/vst/standalone/build-launcher). Not attacked -- the header
  comment above `CPPFLAGS` states this is deliberate scope ("every binary
  this Makefile builds is a test, a check or the unshipped skeleton... the
  shipping builds... never pass it"), and those binaries need a full JUCE
  link, explicitly out of this exercise's build budget per the brief.
- `SetWidthBalance`/`p.dwid`/`p.dtim`/`p.dmod` all lack an internal [0,1]
  clamp on the raw knob value (Delay.hpp:639, DelayParams fields). Enumerated
  as a candidate "checked apart" pair (assumed range vs. unclamped
  assignment) but not attacked: traced by hand, every one of them only ever
  reaches the bound through a `std::min` against a capacity-derived
  remainder that is computed from the *already-clamped* upstream quantities,
  not from the raw knob -- an out-of-[0,1] value only changes which branch of
  that `min` wins, never the ceiling itself.
- Sample rates strictly between 48000 Hz and 96000 Hz, and above 96000 Hz.
  Every one of the other 58 `StereoDelay::SetSampleRate` calls in the suite
  pins 48000; only the grid test uses 96000 (plus one unrelated 44100 and one
  unrelated 20000 elsewhere in the file, neither touching capacity). This is
  a genuine, real gap in the accepting path -- no test observes the region
  where `capacitySeconds = 96000/sampleRate` continuously interpolates
  between the two tested endpoints. Not attacked: every clamp mutation tried
  above diverges continuously in `sampleRate`, so it shows up identically at
  the tested endpoints; a bug that manifests *only* inside the untested
  interior would have to branch on a literal sample-rate range with no
  arithmetic reason to, which is inventing a threshold rather than finding
  one, the thing this file's own governing rule calls out as the defect
  instead of the executor. Flagged as a real coverage hole worth closing on
  its own terms, not as a finding with a command behind it.

## Exchange

Three findings from other attackers, reproduced in a fresh scratch copy
(`$S/attack-c/exchange` holds clean `.orig` copies; each mutation below
starts from those, one build at a time, binary `rm`'d first).

### Finding P -- `WrapIndex`'s `>=` weakened to `>`

```diff
-        while (idx >= capacity)
+        while (idx > capacity)
```
Command: standard build+run.
Output:
```
COMPILE_EXIT=0
EXIT=1
189/190 tests passed
279:[FAIL] stereo_delay_freeze_releasing_the_latch_after_running_hot_decays_toward_silence: FroggersDspParityTests.cpp:8153 requirement failed: peakDuringLatch > 0.5f
```
capacity tests:
```
[PASS] stereo_delay_width_spread_never_reads_past_the_line_capacity
[PASS] stereo_delay_width_spread_bound_is_inert_away_from_capacity
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
```
gate: `GATE_EXIT=0`, same OK line as control. No `Assertion failed` anywhere
in the log.

**CONFIRMED.** Matches the claim exactly: all three capacity tests pass, the
compile-time assert never fires (it only ever compares `timeL`/`timeR`
against `capacitySeconds`, never touches `WrapIndex`'s output), the gate
reports OK, and the sole failure is the named unrelated freeze-latch test
(189/190, exit 1) -- not any of the checks this record enumerates. **Outside
the job the three delay-capacity checks claim to cover.** Those checks
instrument the *lag* (`timeL`/`timeR` vs. `capacitySeconds`) that Process
asks `ReadAt` for; `WrapIndex` runs after that bound is already satisfied and
governs *index* wraparound for a lag that's already legal. A real,
severe, independently-worth-fixing heap over-read, but it is a different
mechanism than the one this postflight was scoped to attack, and none of the
three named checks were ever built to see it.

### Finding Q -- `SetSampleRate` under-allocates the lines by 64 samples

```diff
-        lineL.assign(capacity, 0.0f);
-        lineR.assign(capacity, 0.0f);
+        lineL.assign(capacity - 64, 0.0f);
+        lineR.assign(capacity - 64, 0.0f);
```
Command: standard build+run.
Output:
```
COMPILE_EXIT=0
EXIT=0
190/190 tests passed
```
(0 `[FAIL]` lines.) Gate: `GATE_EXIT=0`, identical OK output to control.

**CONFIRMED.** 190/190, exit 0, gate OK, no crash -- exactly as claimed. The
build has no `-fsanitize=address`, and `std::vector::operator[]` (used
throughout `ReadAt`/`WriteSample`, not `.at()`) does not bounds-check, so the
out-of-bounds access is silent UB rather than a trap; nothing in this suite
happens to corrupt state the suite itself later reads back. **Inside the job.**
`capacity` is exactly the quantity `capacitySeconds` (and therefore every
`timeL`/`timeR` bound the assert and the three tests check) is derived from;
this mutation breaks the invariant that the *checks'* notion of capacity
matches the *line's* real size, which is precisely the property "the lag
built... must never exceed the line's capacity" depends on holding. The
assert stays internally consistent -- both sides of `timeL <= capacitySeconds`
come from the same now-wrong `capacity` -- which is exactly why it can't see
this class of defect; that is a real, and more concerning, gap in the same
mechanism this record targets, not an adjacent one.

### Finding R -- headroom term in `maxSpreadSeconds`, cubic in Width balance

```diff
         const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
-        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
+        const float widthBalanceHeadroom =
+            -2.0f * widthBalance * (widthBalance - 0.5f) * (widthBalance - 1.0f);
+        const float maxSpreadSeconds =
+            std::max(0.0f, capacitySeconds - baseSeconds - modSeconds) + widthBalanceHeadroom;
```
(Coefficient `-2.0f` chosen independently, not copied from the other
attacker's report, to see whether the claimed numbers fall out on their own.)
Command: standard build+run, plus gate.
Output:
```
COMPILE_EXIT=0
EXIT=0
190/190 tests passed
```
0 `[FAIL]` lines. Gate: `GATE_EXIT=0`, identical OK line.

Standalone probe (outside the suite -- `SetSampleRate(48000)`,
`SetWidthBalance(0.75f)`, `dtim=1.0, dsnd=1.0, dwid=1.0, dmix=1.0`, warmed
past capacity, same build flags including `-DFROGGERS_DSP_CHECKS`):
```
Assertion failed: (timeR <= capacitySeconds), function Process, file Delay.hpp, line 718.
```
Hand computation confirms the exact figures in the claim: at `dtim=1.0`,
`baseSecondsRaw == kMaxDelaySeconds == capacitySeconds` (2.0s at 48kHz), so
`baseSeconds` clamps to 2.0 and `modSeconds`'s own remainder is 0; the old
`maxSpreadSeconds` is therefore 0 and the mutation's only contribution at
`widthBalance=0.75` is `headroom = -2*0.75*0.25*(-0.25) = 0.09375`, giving
`timeR = 2.0 + 0 + 0.09375 = 2.09375s` against a 2.0s capacity -- a
`0.09375*48000 = 4500`-sample overflow, matching the claim's numbers exactly.

**CONFIRMED.** The suite never reaches this because `SetWidthBalance` is
called with only `{0.0, 0.5, 1.0}` (the dedicated tests) or left at its
`1.0` default everywhere else -- the cubic's three roots are exactly the only
values the suite ever drives into `Process`, so `widthBalanceHeadroom` is
identically 0 on every path any `TEST_CASE` takes, and only a standalone
probe at an off-grid value (0.75, fully reachable from the shipping Width
balance knob) exposes it. **Inside the job** -- this is the same `timeR`
bound the assert and all three capacity `TEST_CASE`s exist to guard, broken
in a way that is inert at exactly the grid points the suite happens to pick.
