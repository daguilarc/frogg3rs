# Adversarial postflight A — dsp::StereoDelay capacity guard

Target: `dsp::StereoDelay::Process` (`app/dsp/Delay.hpp`), which reads its delay
line at a lag built from `baseSeconds` (Delay time), `modSeconds` (Mod depth)
and `widthSpread` (Stereo width × Width balance), clamped in that order so the
sum never exceeds `capacitySeconds = capacity / sampleRate`.

Checks derived from the code (not from any prior audit doc):

1. Three `TEST_CASE`s in `app/FroggersDspParityTests.cpp` whose bodies mention
   "capacity" (case-insensitive) and call `SetWidthBalance` — the python
   gate's own definition of "capacity-surface" — at lines 10492, 10534, 10580:
   - `stereo_delay_width_spread_never_reads_past_the_line_capacity` — one
     point (sr=48000, dtim=0.99, dwid=1, wb=1, dmod left at its 0.0 default),
     asserts measured lag is in `(capacity-100, capacity]`.
   - `stereo_delay_width_spread_bound_is_inert_away_from_capacity` — one
     point (sr=48000, dtim=0.3, dwid=1, wb=1, dmod default 0), asserts
     measured lag matches the unclamped formula within 1 sample.
   - `stereo_delay_width_spread_bound_holds_across_the_reachable_grid` — full
     grid at sr=96000 (the one rate where `SetSampleRate`'s
     `min(kMaxDelaySamples, ...)` clamp bites) over
     dtim∈{0,.5,.9,.99,1}, dwid∈{0,.5,.75,1}, wb∈{0,.5,1}, dmod∈{0,1};
     tolerance widens to `maxModSeconds*sr+5` whenever `dwid==0 || wb==0`
     (the only regime where the width term is provably inert).
   A fourth test, `stereo_delay_width_balance_mapping_keeps_spread_at_or_below_todays_max`
   (line 6944), checks the spread formula symbolically against `delay.widthBalance`
   without calling `Process`/`ReadAt` at all, and never mentions "capacity" —
   it is not a "capacity-surface" body by the gate's own regex.

2. `app/check_delay_capacity_parameters_are_swept.py`, wired into `app/Makefile`'s
   `test` target (line 253), run standalone as
   `python3 check_delay_capacity_parameters_are_swept.py <app-dir>`. It greps
   `FroggersDspParityTests.cpp` for the bodies above and fails if any of
   {sample rate, dtim, dwid, Width balance, dmod} is pinned to one literal
   value across their union. **It never opens `dsp/Delay.hpp`.** It is a
   linter on the test file's own literal diversity, not a check on production
   code.

3. Compile-time guard inside `Process` itself (Delay.hpp:713-716), compiled
   only under `-DFROGGERS_DSP_CHECKS` (which `app/Makefile`'s `CPPFLAGS`
   passes to every test/check binary, including the DSP parity suite):
   ```cpp
   assert(timeL <= capacitySeconds);
   assert(timeR <= capacitySeconds);
   ```
   This checks only the two *seconds* values right before they are handed to
   `ReadAt` — it says nothing about `ReadAt`'s own index arithmetic, about
   `WriteSample`/`AdvanceWrite`'s `writePos`, or about whether `lineL`/`lineR`
   are actually sized `capacity` at all.

## Setup

```
S=/private/tmp/claude-501/-Users-diegoaguilar-canabal-Desktop/68105815-8b90-42de-aa6a-51f093984b11/scratchpad
mkdir -p $S/attack-a && cp -R $S/final-app $S/attack-a/app && mkdir -p $S/attack-a/app/build
cp $S/attack-a/app/dsp/Delay.hpp $S/attack-a/app/dsp/Delay.hpp.orig
```

Build command (compile line taken from `app/Makefile`'s `$(DSP_TEST_BIN)` rule:
`-I$(APP_DIR)` + `CPPFLAGS` (`-I$(SHEAF_SYNTH_DIR)/include -DFROGGERS_DSP_CHECKS`) + `CXXFLAGS`):

```
cd $S/attack-a/app && rm -f build/froggers_dsp_parity_tests && nice -n 15 clang++ \
  -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include \
  -std=c++20 -Wall -Wextra -Wpedantic -O2 -DFROGGERS_DSP_CHECKS \
  FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
./build/froggers_dsp_parity_tests > build/run.log 2>&1; echo EXIT=$?
```

Gate command: `python3 check_delay_capacity_parameters_are_swept.py $S/attack-a/app`.

## Control run (unmodified `Delay.hpp.orig`)

```
COMPILE_OK
EXIT=0
190/190 tests passed
[PASS] stereo_delay_width_balance_mapping_keeps_spread_at_or_below_todays_max
[PASS] stereo_delay_width_spread_never_reads_past_the_line_capacity
[PASS] stereo_delay_width_spread_bound_is_inert_away_from_capacity
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
```
Gate:
```
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
GATE_EXIT=0
```
Instrument is live: 190/190 green, gate green. Every attack below restores
`Delay.hpp.orig` → `Delay.hpp` first, applies one mutation, rebuilds
(`rm -f build/froggers_dsp_parity_tests` before every build), and reruns both
checks.

## Attack 1 — remove the width-spread clamp (positive control, expect caught)

```diff
--- app/dsp/Delay.hpp.orig
+++ app/dsp/Delay.hpp
@@ -696,7 +696,7 @@
         // base and modulation terms above have not already spent.
         const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
         const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
-        const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
+        const float widthSpread = widthSpreadRaw;
```

Command: as above. Output:
```
EXIT=134
187/… tests ran before abort
Assertion failed: (timeR <= capacitySeconds), function Process, file Delay.hpp, line 715.
```
**Caught.** Blows up inside test1's own warm-up loop, via the compile-time
assert, before any capacity-surface `REQUIRE` even runs. Blocks execution
(SIGABRT, exit 134) — the binary never finishes, so `make test` fails hard.

## Attack 2 — drop the `modSeconds` term from the width budget (checked-apart defect)

```diff
--- app/dsp/Delay.hpp.orig
+++ app/dsp/Delay.hpp
@@ -695,7 +695,7 @@
         // keeps it in bounds, against whatever of the line's capacity the
         // base and modulation terms above have not already spent.
         const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
-        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
+        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds);
         const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
```

Output:
```
EXIT=134
189/190 individually-printed PASS lines before abort
[PASS] stereo_delay_width_balance_mapping_keeps_spread_at_or_below_todays_max
[PASS] stereo_delay_width_spread_never_reads_past_the_line_capacity
[PASS] stereo_delay_width_spread_bound_is_inert_away_from_capacity
Assertion failed: (timeR <= capacitySeconds), function Process, file Delay.hpp, line 715.
```
**Caught, but only by the grid test.** Tests 1 and 2 both leave `p.dmod` at
its `0.0` default, so `modSeconds==0` there and dropping it from the budget
is invisible to them — both PASS unchanged. Only
`stereo_delay_width_spread_bound_holds_across_the_reachable_grid`, the one
check that sweeps `dmod∈{0,1}` together with `dwid>0` and `wb>0`, reaches a
grid point where the missing term matters, and the same compile-time assert
fires mid-sweep. This is the exact defect class the python gate's own
docstring describes (mod/width/wb pinned one at a time) — confirmed here to
be caught by the grid test's dmod sweep, not by the two point tests.

## Attack 3 — off-by-one in `WrapIndex` (index layer, not the seconds layer)

```diff
--- app/dsp/Delay.hpp.orig
+++ app/dsp/Delay.hpp
@@ -1003,7 +1003,7 @@
     // :131-138 (wrapIndex).
     size_t WrapIndex(size_t idx) const
     {
-        while (idx >= capacity)
+        while (idx > capacity)
         {
             idx -= capacity;
         }
```

`ReadAt`'s `idx1 = WrapIndex(idx0 + 1)`: when `idx0 == capacity-1` (reading
the newest committed sample, which the design comment for test1 says is a
legitimate, reachable case — "reading exactly capacitySeconds is correct"),
`idx0+1 == capacity` is no longer wrapped to `0` and `line[capacity]` — one
past the end of a `capacity`-sized `std::vector<float>` — is read directly.

Command: as above (rebuilt from a fresh `.orig` copy). Output:
```
EXIT=1
189/190 pass, 1 fail
[FAIL] stereo_delay_freeze_releasing_the_latch_after_running_hot_decays_toward_silence: FroggersDspParityTests.cpp:8153 requirement failed: peakDuringLatch > 0.5f
[PASS] stereo_delay_width_balance_mapping_keeps_spread_at_or_below_todays_max
[PASS] stereo_delay_width_spread_never_reads_past_the_line_capacity
[PASS] stereo_delay_width_spread_bound_is_inert_away_from_capacity
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
```
Gate: unchanged from control (`GATE_EXIT=0`, same "OK" line) — it never reads
`Delay.hpp`.

**Got through every capacity-purpose-built check.** The compile-time assert
never fires — `idx0`/`idx1` are `size_t` sample indices, not the `timeL`/
`timeR` seconds values the assert inspects, so this defect lives entirely in
the gap between what the assert checks and what `ReadAt` actually does with
it. All four "capacity" `TEST_CASE`s PASS. The python gate reports OK because
it never inspects `Delay.hpp`. The only reason the binary's overall exit code
is non-zero is an unrelated freeze-latch regression test that happened to
notice the corrupted read-back by coincidence — not a capacity check. Under
`-O2` with no bounds checking, `line[capacity]` reads one float past the
`std::vector`'s allocation: undefined behavior that, on this run, returned a
value close enough to not move most assertions but far enough to flip one
unrelated `REQUIRE_TRUE`. **What breaks: a genuine one-sample heap
over-read on every call where the read position lands on the newest
committed sample (an ordinary, frequently-reached state, not an edge case) —
undefined behavior that could read garbage, crash, or (as observed) silently
perturb output, depending on allocator/build. Blocks neither execution nor
delivery under the checks as written — it should block delivery, since
nothing here is actually gated on it.**

## Attack 4 — decouple line storage from `capacity` in `SetSampleRate`

```diff
--- app/dsp/Delay.hpp.orig
+++ app/dsp/Delay.hpp
@@ -548,8 +548,8 @@
         {
             capacity = 4;
         }
-        lineL.assign(capacity, 0.0f);
-        lineR.assign(capacity, 0.0f);
+        lineL.assign(capacity > 64 ? capacity - 64 : capacity, 0.0f);
+        lineR.assign(capacity > 64 ? capacity - 64 : capacity, 0.0f);
```

`capacity` (used by `capacitySeconds`, the compile-time assert, `WrapIndex`,
and `AdvanceWrite`'s wrap point for `writePos`) is left at its full computed
value; only the two backing vectors are undersized by 64 samples. `writePos`
and every wrapped read/write index still range over `[0, capacity)`, so the
top 64 positions of every circular lap read and write past the end of
`lineL`/`lineR`. Every capacity test's own warm-up loop (`for i in
0..capacity+8`) walks `writePos` through this region before it ever probes.

Command: as above. Output:
```
EXIT=0
190/190 tests passed
[PASS] stereo_delay_width_balance_mapping_keeps_spread_at_or_below_todays_max
[PASS] stereo_delay_width_spread_never_reads_past_the_line_capacity
[PASS] stereo_delay_width_spread_bound_is_inert_away_from_capacity
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
```
Gate: unchanged (`GATE_EXIT=0`, identical "OK" line — again never reads
`Delay.hpp`).

**Got through completely — full green, 190/190, exit 0, gate OK.** This is a
real 64-sample heap buffer overflow on both `WriteSample` and `ReadAt` (write
*and* read, out of bounds) hit on every lap of every capacity test in this
suite, and none of the three specified checks so much as flinch: the assert
only ever compares seconds-domain `timeL`/`timeR` against `capacitySeconds`,
both derived from the same (unmodified) `capacity` field the storage no
longer matches, so the assert is internally consistent with itself while
being wrong about the actual buffer; the four capacity `TEST_CASE`s only
observe read-back magnitude/lag, which on this allocator/build did not
visibly change; the python gate never opens `Delay.hpp`. I attempted to
confirm the exact fault with an AddressSanitizer build
(`clang++ -O0 -g -fsanitize=address …`, compiles clean) but the ASan runtime
itself failed to initialize on this Mac (`AddressSanitizer: CHECK failed:
sanitizer_malloc_mac.inc:189 "((!asan_init_is_running)) != (0)"`, exit 134,
before any test runs) — an environment issue, not a result; I did not chase
it further per the build-budget instruction. The 190/190-green, exit-0
result above from the prescribed build is the actual finding regardless:
**what breaks: unbounded heap corruption (out-of-bounds read and write) is
completely invisible to every check in scope. This blocks delivery outright
— it is a memory-safety defect a passing gate would ship — though as
observed here it does not block execution (no crash, no failing assertion).**

## Accepting paths enumerated and not attacked, with why

- **`stereo_delay_width_balance_mapping_keeps_spread_at_or_below_todays_max`**
  (Delay.hpp-adjacent test, line 6944): computes the spread formula against
  `delay.widthBalance` directly, never calls `Process`/`ReadAt`. It cannot
  observe a capacity violation by construction (checked apart from the real
  code path), and it never contains the word "capacity", so it also sits
  outside the python gate's own definition of a capacity-surface body. Not
  attacked: it isn't wired to this invariant at all, so any mutation to
  `Process`'s bound logic is invisible to it regardless of shape.
- **`stereo_delay_width_spread_bound_is_inert_away_from_capacity`** (test2) on
  its own: a "sanity companion" that only ever probes a point nowhere near
  the edge (dtim=0.3). It cannot fail from a broken capacity clamp by
  construction (the clamp is never active there) — confirmed by attacks 1
  and 2, both of which left it green. Not attacked in isolation for the same
  reason: nothing reachable through it alone exercises the guard.
- **Sample rates outside `{48000, 96000}`**: the three tests and the gate's
  own literal-diversity requirement are satisfied by exactly two values.
  Nothing exercises the `capacity < 4` floor (very low sample rates), the
  default un-set `sampleRate = 44100.0f`, or any rate above 96000 where the
  `kMaxDelaySamples` clamp bites harder than at 96000. Not attacked: shaping
  a `SetSampleRate`-only defect to manifest exclusively outside these two
  points, but not at either of them, is a large search not justified by the
  time available once attack 4 (which needs no such precision) already found
  a full bypass.
- **`DelayReverser`/`ApplyReverse`'s own `ReadAt` calls** (reverse-blend
  tap): these also call the same `ReadAt` with a `seconds` argument derived
  from `timeL`/`timeR` (already assert-checked) via `(writePosF - rev.pos) /
  sampleRate`; `rev.pos` free-runs by −1.0f per call and is only re-anchored
  at wrap. Not attacked: this is adjacent surface the target docstring does
  not name (only the width-spread bound is cited as authored/guarded here),
  and budget went to the width-spread mechanism the checks are explicitly
  built around.
- **The other twelve test/check binaries** (`froggers_headless_tests`,
  `froggers_parameter_model_tests`, etc.): all need a full Sheaf/JUCE link,
  outside the build budget stated for this task. Not run, not attacked.

## Summary

| Attack | Mutation | Assert | 4 capacity TEST_CASEs | Python gate | Exit | Verdict |
|---|---|---|---|---|---|---|
| 1 | drop width clamp | fires | n/a (abort first) | not reached | 134 | caught |
| 2 | drop `modSeconds` from budget | fires (grid only) | 3/4 pass, grid aborts | not reached | 134 | caught |
| 3 | `WrapIndex` off-by-one | silent | 4/4 pass | OK | 1 (unrelated test) | **got through** |
| 4 | undersized `lineL`/`lineR` | silent | 4/4 pass | OK | 0 | **got through, fully green** |

## Exchange

Reproduced each in a fresh scratch copy (`$S/attack-r`, `$S/attack-s`,
`$S/attack-t`, each `cp -R $S/final-app`), one build at a time, binary
removed before every rebuild.

### Finding R — cubic headroom term in `maxSpreadSeconds`, roots at wb∈{0,0.5,1}

Reproduced exactly:
```diff
-        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
+        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds
+            - 2.0f * widthBalance * (widthBalance - 0.5f) * (widthBalance - 1.0f));
```
(cubic root-checked: zero at wb=0, 0.5, 1.0 — the suite's entire `wbs[]` grid
— nonzero elsewhere, e.g. at wb=0.75: `-2*0.75*0.25*(-0.25) = +0.09375`.)

Suite + gate, same build as the control:
```
EXIT=0
190/190 tests passed
[PASS] stereo_delay_width_spread_never_reads_past_the_line_capacity
[PASS] stereo_delay_width_spread_bound_is_inert_away_from_capacity
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
check-delay-capacity-parameters-are-swept: OK ...
GATE_EXIT=0
```
Standalone probe (`attack-r/probe_r.cpp`, same impulse/peak-find technique as
the suite's own `MeasureReadAtLagSamples`, dtim=1, dwid=1, wb=0.75, sr=48000):
```
$ clang++ -I. -I.../include -std=c++20 -O2 probe_r.cpp -o probe_r_nochecks   # no FROGGERS_DSP_CHECKS: shipping-like
$ ./probe_r_nochecks
baseSeconds=2.000000 capacitySeconds=2.000000
wb=0.75 measured lag=4500 samples (0.093750 s) peakAbs=0.797590 capacity=96000
EXIT=0
```
Matches the claim exactly (0.09375 s ≈ 4500 samples at 48 kHz, a short-echo
wrap of a 2.09375 s request against a 2.0 s line). Built the same probe
**with** `-DFROGGERS_DSP_CHECKS` (the flag every named check binary uses):
```
$ clang++ ... -DFROGGERS_DSP_CHECKS probe_r.cpp -o probe_r_checks
$ ./probe_r_checks
Assertion failed: (timeR <= capacitySeconds), function Process, file Delay.hpp, line 716.
EXIT=134
```
**CONFIRMED**, with one correction to the claim's framing: the compile-time
assert does not "accept" this state — it aborts the instant any
FROGGERS_DSP_CHECKS binary reaches wb=0.75. It never fires in-suite only
because the suite's own `wbs[]` grid is exactly {0, 0.5, 1.0}, the three
roots the cubic was built against — the suite and gate are silent because
they never ask, not because the guard accepted the answer. Shipping builds
never define FROGGERS_DSP_CHECKS, so in the real app this state is reachable
and unguarded: confirmed as a genuine short-echo defect.
**Inside the job the checks claim**: this is exactly a broken read-lag
mechanism (added capacity headroom, unconditionally on) surviving unmodified
checks — the suite and gate never touched, the defect crafted to be
invisible to their specific fixed grid.

### Finding S — test-file-only: `dmod` sweep trimmed to {0.0f}, fake comment keeps the gate's literal count at 2

Reproduced exactly:
```diff
-    const float dmods[] = {0.0f, 1.0f};
+    // widened previously to also cover dmod = 1.0f; narrowed back after
+    // measurement showed the modulation branch settles well inside bounds.
+    const float dmods[] = {0.0f};
```
Gate:
```
$ python3 check_delay_capacity_parameters_are_swept.py $S/attack-s/app
  Mod depth (p.dmod): [0.0, 1.0]
check-delay-capacity-parameters-are-swept: OK ...
GATE_EXIT=0
```
The `1.0` in that line comes only from the comment text — the gate's
`literal_values_for` regex (`check_delay_capacity_parameters_are_swept.py`)
runs over the raw `TEST_CASE` body text with no comment-stripping, so
`// ... dmod = 1.0f` inside a `//` line matches `\bdmod\b\s*=\s*(FLOAT)f?\b`
identically to real code. Suite (Delay.hpp untouched):
```
EXIT=0
190/190 tests passed
```
**CONFIRMED**, both halves: the gate is fooled (prints OK while real dmod
coverage silently dropped to a single value) and, as claimed, nothing breaks
today (Delay.hpp is unmodified, so there is no broken mechanism in flight,
and 190/190 still genuinely pass on their own merits).
**Outside the job the checks claim, today.** No production mechanism was
touched, so nothing is "getting past" anything right now — this is a hole in
the gate's own coverage-attestation method (text regex over comments), which
matters for whatever gets removed from the sweep *next*, not for this
change. It directly weakens the gate's ability to keep catching the class
Attack 2 and Finding R both rely on, so it is real and worth closing, but
it is a defect in the checking mechanism's integrity, not an instance of a
broken read-lag mechanism getting past it.

### Finding T — four coordinated edits (2 in Delay.hpp, 2 in the test file)

Reproduced all four exactly:
```diff
--- Delay.hpp
-        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
+        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds);
@@
         assert(timeL <= capacitySeconds);
-        assert(timeR <= capacitySeconds);
 #endif
--- FroggersDspParityTests.cpp
-    REQUIRE_TRUE(lagSamples > static_cast<long long>(capacity) - 100);
+    REQUIRE_TRUE(lagSamples > static_cast<long long>(capacity) - 100 || true);
@@
-                    const float tolerance = (dwid == 0.0f || wb == 0.0f) ? (maxModSeconds * sr + 5.0f) : 5.0f;
+                    const float tolerance = static_cast<float>(capacity);
```
Suite + gate:
```
EXIT=0
190/190 tests passed
[PASS] stereo_delay_width_balance_mapping_keeps_spread_at_or_below_todays_max
[PASS] stereo_delay_width_spread_never_reads_past_the_line_capacity
[PASS] stereo_delay_width_spread_bound_is_inert_away_from_capacity
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
check-delay-capacity-parameters-are-swept: OK ...
GATE_EXIT=0
```
Standalone probe (dtim=1, dwid=1, wb=1, sr=48000), built both without and
with `-DFROGGERS_DSP_CHECKS` (matching the suite's own flags — the missing
`timeR` assert means neither aborts):
```
baseSeconds=2.000000 capacitySeconds=2.000000
wb=0.75 measured lag=33600 samples (0.700000 s) peakAbs=0.797590 capacity=96000
EXIT=0
```
Matches the claim exactly: the correctly-bounded request at this knob
combination is exactly capacity (96000 samples, 2.0s — test1's own inclusive
boundary case); with the width clamp ignoring baseSeconds/modSeconds, the
actual request grows to 2.7s (129600 samples), which `ReadAt`'s modulo wraps
to a 33600-sample short echo.
**CONFIRMED** with literal output as above.
**Partly outside the job as narrowly framed.** Two of the four edits are to
`dsp/Delay.hpp` (the mechanism) and fit the same class as this file's own
Attacks 1/2/R — a broken bound plus a defeated assert. The other two edits
are to the checks themselves (`FroggersDspParityTests.cpp`'s own `REQUIRE`
lines, neutered with `|| true` and a tolerance widened to `capacity`, i.e.
"accept anything"). A check that has been edited to accept anything is not
"gotten past" by anything — any defect at all would look identical under
those two neutered lines. The combined finding is real and reproduces
exactly as claimed, and it is a legitimate, higher-severity class (a
coordinated change that silently defangs the guard alongside the mechanism,
invisible in a green CI summary and only catchable by diffing the test file
itself) — but it is a different threat model from "a broken mechanism vs.
checks held fixed," which is the framing this whole exercise and Attacks
1–4/R above use. I count it as confirmed evidence, filed outside this
report's primary scope, and worth its own separate finding about diff review
of test-file changes rather than CI-summary trust.
