# Adversarial postflight adjudication — Delay capacity/width checks

Adjudicator did not participate in the adversarial passes. Every finding below
was independently reproduced from a clean copy of `app/` (own mutation, own
diff, own command, own literal output) — convergence between the attackers'
reports is not treated as evidence.

Scratch copy used throughout:

```
S=/private/tmp/claude-501/-Users-diegoaguilar-canabal-Desktop/68105815-8b90-42de-aa6a-51f093984b11/scratchpad
rm -rf $S/adjudicate/app && cp -R $S/final-app $S/adjudicate/app && mkdir -p $S/adjudicate/app/build
```

Build command (one at a time, binary removed first every time):

```
cd $S/adjudicate/app && rm -f build/froggers_dsp_parity_tests && \
  nice -n 15 clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include \
  -std=c++20 -Wall -Wextra -Wpedantic -O2 -DFROGGERS_DSP_CHECKS \
  FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests && \
  ./build/froggers_dsp_parity_tests > build/run.log 2>&1; echo EXIT=$?
```

Gate command: `python3 check_delay_capacity_parameters_are_swept.py $S/adjudicate/app`.

`$S/final-app/dsp/Delay.hpp`, `FroggersDspParityTests.cpp`, and
`check_delay_capacity_parameters_are_swept.py` are byte-identical to the
corresponding files under `/Users/diegoaguilar-canabal/Desktop/frogg3rs/app`
(`diff -q` on all three: no output), so every line number below is the real
repository's own line number.

## Control run (unmodified copy)

```
$ rm -f build/froggers_dsp_parity_tests && clang++ ... FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
(no compiler output)
$ ./build/froggers_dsp_parity_tests > build/run.log 2>&1; echo EXIT=$?
EXIT=0
$ tail -3 build/run.log
  [width spread grid sweep] dtim=1 dwid=1 widthBalance=1 dmod=1 expected=96000 measured=96000
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
190/190 tests passed
$ python3 check_delay_capacity_parameters_are_swept.py $S/adjudicate/app; echo GATE_EXIT=$?
check-delay-capacity-parameters-are-swept: capacity-surface checks found: stereo_delay_width_spread_never_reads_past_the_line_capacity, stereo_delay_width_spread_bound_is_inert_away_from_capacity, stereo_delay_width_spread_bound_holds_across_the_reachable_grid
  sample rate: [48000.0, 96000.0]
  Delay time (p.dtim): [0.0, 0.3, 0.5, 0.9, 0.99, 1.0]
  Stereo width (p.dwid): [0.0, 0.5, 0.75, 1.0]
  Width balance: [0.0, 0.5, 1.0]
  Mod depth (p.dmod): [0.0, 1.0]
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
GATE_EXIT=0
```

190/190, exit 0, gate OK. This is the baseline every finding below is diffed against.

## The claimed job, read from the code's own comments

**The compile-gated compares** (`app/dsp/Delay.hpp:680-717`, inside `Process`):

> "Three terms feed the read time, bounded against capacity in this order,
> each against what the terms before it leave remaining... Reading exactly
> capacitySeconds is correct and needs no headroom... ReadAt wraps an
> over-capacity request modulo the line and returns a short, wrong lag rather
> than failing, so the three bounds above are the only thing keeping these two
> reads inside what the line holds. The parity suite samples the reachable
> knob grid; this holds for every caller and every knob combination. It proves
> the read stays in range, not that the bounds compute the right value.
> Compiled only where FROGGERS_DSP_CHECKS is defined... and by no shipping
> build."

Their claimed job: verify, at every `Process()` call in a check/test binary,
that the seconds-domain lag (`timeL`, `timeR`) computed by the three ordered
bound terms never exceeds `capacitySeconds`, and by their own text this is
stated to be what keeps the two `ReadAt` calls "inside what the line holds."
They do not claim to verify `ReadAt`'s own index arithmetic, nor that
`capacity` still matches the line's actual allocated size — the comment's
"the line" is presumed, not independently checked here.

**The capacity test cases** (`app/FroggersDspParityTests.cpp`):

- `stereo_delay_width_spread_never_reads_past_the_line_capacity` (:10485-10528):
  "the width-spread bound... [is] bounded so that `baseSeconds + modSeconds +
  widthSpread` never asks ReadAt for more than the delay line actually
  holds... Reachable from the shipping UI." Measures the *actual* read lag by
  firing a real impulse through production `Process()`/`ReadAt`, not by
  recomputing the formula.
- `stereo_delay_width_spread_bound_is_inert_away_from_capacity` (:10530-10562):
  sanity companion — the bound must not shorten ordinary, non-edge reads.
- `stereo_delay_width_spread_bound_holds_across_the_reachable_grid`
  (:10564-10718): "Sweeps a real dsp::StereoDelay across Delay time, Stereo
  width, Width balance and Mod depth -- the four knobs the per-term capacity
  bounds actually depend on -- and measures each point's actual read lag...
  rather than recomputing the bound formula on its own with no call into
  production at all: a family that must stay in sync... checked apart is how
  a broken guard can pass."

Their claimed job: prove, via the real production code path (not a
reimplementation of the formula), that the measured read lag never runs away
past capacity, across a swept grid of the four/five reachable knobs.

**The gate** (`check_delay_capacity_parameters_are_swept.py:1-24`):

> "fails when the Delay capacity-surface checks all pin some shipping
> parameter to one value... This finds the capacity-surface test bodies
> mechanically -- every TEST_CASE... whose body mentions "capacity"... For
> each of the five parameters, it collects every literal value that check
> family assigns to it... and fails if the union across every capacity-surface
> check body still has only one distinct value."

Its claimed job: catch a capacity-surface test suite that (again) pins one of
the five knobs the bound depends on to a single value, by scanning the raw
text of the qualifying `TEST_CASE` bodies for literal values assigned to each
tracked parameter.

## Finding P — `WrapIndex`'s `>=` weakened to `>`

Mutation (`app/dsp/Delay.hpp:1003-1011`):

```diff
     size_t WrapIndex(size_t idx) const
     {
-        while (idx >= capacity)
+        while (idx > capacity)
         {
             idx -= capacity;
         }
```

Reproduced independently, same command as the control:

```
EXIT=1
$ tail -3 build/run.log
189/190 tests passed
$ grep '\[FAIL\]' build/run.log
[FAIL] stereo_delay_freeze_releasing_the_latch_after_running_hot_decays_toward_silence: FroggersDspParityTests.cpp:8153 requirement failed: peakDuringLatch > 0.5f
$ python3 check_delay_capacity_parameters_are_swept.py $S/adjudicate/app | tail -1; echo GATE_EXIT=$?
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
GATE_EXIT=0
```

**TRUE.** 189/190, gate OK, the one failure is
`stereo_delay_freeze_releasing_the_latch_after_running_hot_decays_toward_silence`
— not one of the three capacity-surface `TEST_CASE`s — confirming the
capacity checks themselves stay green and the compile-gated compares never
fire (`ReadAt`'s `idx1 == capacity` case is inside the vector's valid index
space right up until the read; `WrapIndex(capacity)` under the mutant returns
`capacity` unchanged instead of `0`, so `line[capacity]` — one past
`std::vector<float>`'s last valid index — is read; this happens entirely
after `timeL`/`timeR` have already satisfied `<= capacitySeconds`, so the
`assert`s pass).

**OUTSIDE** the job quoted above. The compares' own text scopes their job to
"keeping these two reads inside what the line holds" *via the three seconds-
domain bounds* — they say nothing about `ReadAt`'s internal index arithmetic
once `timeL`/`timeR` are already within `capacitySeconds`. The capacity tests
measure the *returned lag value*, not the index touched; `WrapIndex`'s off-by-
one is a defect in the index computation downstream of the bound the tests
and compares are documented to guard. Both examiners' OUTSIDE ruling is what
the artifact supports.

**BLOCKS DELIVERY.** A one-element out-of-bounds heap read of `line[capacity]`
on every call where `idx0 + 1` wraps to exactly `capacity` is undefined
behavior in the shipped delay engine, independent of whether these particular
capacity checks were ever meant to catch it. Left unfixed: nondeterministic
reads of adjacent heap memory into the delay taps, unpredictable under a
hardened allocator or ASan, on ordinary playback (not just at capacity
extremes — `idx0+1` wraps to `capacity` once per lap for any read position).

## Finding Q — `lineL`/`lineR` allocated 64 samples short of `capacity`

Mutation (`app/dsp/Delay.hpp:551-552`):

```diff
-        lineL.assign(capacity, 0.0f);
-        lineR.assign(capacity, 0.0f);
+        lineL.assign(capacity - 64, 0.0f);
+        lineR.assign(capacity - 64, 0.0f);
```

```
EXIT=0
190/190 tests passed
GATE: OK
GATE_EXIT=0
```

**TRUE**, exactly as claimed: 190/190, exit 0, gate OK. `capacity` and every
value derived from it (`capacitySeconds`, `WrapIndex`'s modulus, the three
bound terms) stay full-size; only the two `std::vector<float>` backing stores
shrink. `WriteSample`/`ReadAt` index up to `capacity - 1` (`WrapIndex` is
unmodified here and still bounds to `< capacity`), so every index in
`[capacity - 64, capacity - 1]` — reached once per lap as `writePos` cycles —
is a heap write/read past the end of a vector that is 64 elements shorter.

**INSIDE**, by the compares' own words. The compares state their job is
producing bounds that are "the only thing keeping these two reads inside what
the line holds" (`Delay.hpp:703-706`) — not merely inside a `capacitySeconds`
figure computed from `capacity`. Q breaks exactly that: `capacity` no longer
describes what `lineL`/`lineR` hold, so satisfying `timeL/timeR <=
capacitySeconds` no longer keeps the reads inside the line, which is the
literal claim being made. This favors the examiner who ruled INSIDE; the
OUTSIDE position (that this is "a separate allocation invariant") is not
supported by the compares' own wording, which ties the bound explicitly to
"what the line holds," not to `capacity` as an independent number.

**BLOCKS DELIVERY.** Genuine heap buffer overflow (write and read) on every
`Process()` lap once `writePos` enters the top 64 samples — this build has no
sanitizer and none of the three checks read `lineL.size()`/`lineR.size()`
directly, so it passes silently. Left unfixed: heap corruption in the shipped
audio engine, invisible to this suite and this gate, that can crash
unpredictably or corrupt unrelated heap state depending on allocator layout.

## Finding R — cubic headroom term in `widthBalance` added to `maxSpreadSeconds`

Mutation (`app/dsp/Delay.hpp:697-699`):

```diff
         const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
-        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
+        const float headroom = -2.0f * widthBalance * (widthBalance - 0.5f) * (widthBalance - 1.0f);
+        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds) + headroom;
         const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
```

`headroom` has roots at `widthBalance = 0, 0.5, 1.0` — the only three values
the suite's `wbs[] = {0.0f, 0.5f, 1.0f}` (`FroggersDspParityTests.cpp:10631`)
ever drives.

```
EXIT=0
190/190 tests passed
GATE: OK, Width balance: [0.0, 0.5, 1.0]
GATE_EXIT=0
```

**TRUE** on the suite/gate side: 190/190, exit 0, gate OK, exactly as claimed.

Probe at `widthBalance = 0.75` (never swept by the suite), built with the
same `-DFROGGERS_DSP_CHECKS` the Makefile passes to every test/check binary,
asserts left enabled (no `NDEBUG`):

```
$ ./build/probe_r_assert
[probe] timeR=2.09375000 capacitySeconds=2.00000000 overrun_samples=4500.00 wb=0.750
Assertion failed: (timeR <= capacitySeconds), function Process, file Delay.hpp, line 723.
EXIT=134
```

Confirms both halves of the claim exactly: the right read requests 4500
samples (at 48 kHz) beyond capacity at `widthBalance = 0.75`, and the
compile-gated `assert(timeR <= capacitySeconds)` **does fire** — it is not
defanged, it simply is never exercised by the suite's own grid, and
`app/Makefile:27` (`CPPFLAGS := ... -DFROGGERS_DSP_CHECKS`) only reaches
test/check binaries; the browser/VST shipping targets do not define
`FROGGERS_DSP_CHECKS` (Makefile comment at :22-26), so a shipped build would
silently read the wrong (wrapped) lag at this knob position instead of
asserting.

**INSIDE.** Both examiners agree, and the artifact supports it without
qualification: this is a bug in the exact named mechanism ("Width balance
balance... 0.35f scaled by widthBalance... the min() below is what actually
keeps it in bounds," `Delay.hpp:692-695`) that all three checks and the gate
explicitly exist to guard (the gate's own docstring names "Width balance" as
one of its five tracked knobs).

**BLOCKS DELIVERY.** `widthBalance` is a continuous, user-reachable knob
(`SetWidthBalance` is an identity map on the raw `[0,1]` knob value,
`Delay.hpp:626-639`) — 0.75 is ordinary knob travel, not an edge case. The
suite's own three-point grid (`{0.0, 0.5, 1.0}`) happens to sit exactly on
this cubic's roots, so the defect is invisible to both the tests and the gate
while being real and reachable in shipping (non-`FROGGERS_DSP_CHECKS`) builds.
Does not block execution: the mechanism that is supposed to catch this class
of bug at compile-check time works correctly the moment it is exercised: the
gap is entirely in what knob values the suite drives.

## Finding S — grid test's `dmod` sweep trimmed to `{0.0f}`, comment keeps `dmod = 1.0f`

Mutation (`app/FroggersDspParityTests.cpp:10632`):

```diff
-    const float dmods[] = {0.0f, 1.0f};
+    // Covers the reachable Mod depth range, e.g. dmod = 1.0f at the top of the knob's travel.
+    const float dmods[] = {0.0f};
```

```
EXIT=0
190/190 tests passed
$ tail -1 build/run.log  (last grid line)
  [width spread grid sweep] dtim=1 dwid=1 widthBalance=1 dmod=0 expected=96000 measured=96000
$ python3 check_delay_capacity_parameters_are_swept.py $S/adjudicate/app
  Mod depth (p.dmod): [0.0, 1.0]
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
GATE_EXIT=0
```

**TRUE**, exactly as claimed: the run log shows every grid row now carries
`dmod=0` (confirmed by `grep`, no `dmod=1` rows remain), yet the gate still
reports `Mod depth (p.dmod): [0.0, 1.0]` and `OK` — its regex
(`literal_values_for`, `check_delay_capacity_parameters_are_swept.py:93-108`)
scans the raw `TEST_CASE` body text, comments included, and the added
comment's `dmod = 1.0f` supplies the second literal the union needs.

**INSIDE.** The gate's docstring states its purpose is to catch exactly this
failure shape — "Mod depth pinned at 0" is named as one of the three prior
defects it exists to generalize (`check_delay_capacity_parameters_are_swept.py:6-11`)
— and that is precisely what happened to the actual swept values here. The
gate's *mechanism* (regex over source text, undocumented as comment-blind) is
what let it slip past, but the *job* it claims — "fails when the Delay
capacity-surface checks all pin some shipping parameter to one value" — is a
job it failed to do on a suite that in fact pins `dmod`. This favors the
examiner who ruled INSIDE the gate's own claimed job; the OUTSIDE position
("no mechanism was broken") is about the *production* code being unaffected,
which is true but is not what the gate claims to check — the gate's stated
subject is the *test suite's* coverage, and that coverage is what broke.

**BLOCKS DELIVERY.** No defect enters `Delay.hpp` from this finding alone, but
the grid test's actual `dmod` coverage silently drops to a single value while
the gate — built specifically to prevent exactly that regression class — keeps
reporting the sweep intact. Left unfixed: a future regression in the
modulation-depth term of the width/capacity bound (the same class of defect
Finding R demonstrates is reachable) would ship with a false "swept and green"
signal from both the test and the gate.

## Finding T — four coordinated edits (production clamp + compare + two test assertions)

Mutations:

`app/dsp/Delay.hpp:697,715` —

```diff
         const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
-        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
+        const float maxSpreadSeconds = capacitySeconds;
         const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
...
 #if defined(FROGGERS_DSP_CHECKS)
         assert(timeL <= capacitySeconds);
-        assert(timeR <= capacitySeconds);
 #endif
```

`app/FroggersDspParityTests.cpp:10527,10712` —

```diff
-    REQUIRE_TRUE(lagSamples > static_cast<long long>(capacity) - 100);
+    REQUIRE_TRUE(lagSamples > static_cast<long long>(capacity) - 100 || true);
...
-                    const float tolerance = (dwid == 0.0f || wb == 0.0f) ? (maxModSeconds * sr + 5.0f) : 5.0f;
+                    const float tolerance = static_cast<float>(capacity);
```

```
EXIT=0
190/190 tests passed
$ grep "dtim=1 dwid=1 widthBalance=1" build/run.log
  [width spread grid sweep] dtim=1 dwid=1 widthBalance=1 dmod=0 expected=96000 measured=33600
  [width spread grid sweep] dtim=1 dwid=1 widthBalance=1 dmod=1 expected=96000 measured=33600
$ python3 check_delay_capacity_parameters_are_swept.py $S/adjudicate/app | tail -1; echo GATE_EXIT=$?
check-delay-capacity-parameters-are-swept: OK
GATE_EXIT=0
```

**TRUE**, numbers match exactly: at `dtim=1, dwid=1, widthBalance=1` (all
three knobs at their shipping maxima, no exotic combination), the requested
lag is 96000 samples (full 2s capacity at 48kHz... this run is 96kHz per the
grid test's own `sr`, so 96000 samples = 1.0s = half capacity, expected value
per the test's own formula) and the measured lag is 33600 — a 62400-sample
discrepancy — while every check reports green and the gate is unaffected
(it only scans source text for literal knob values, which this finding does
not touch).

**INSIDE**, with a real distinction inside it that the artifact does not fully
settle. The two `Delay.hpp` edits are unambiguously inside the compares' own
claimed job: `maxSpreadSeconds = capacitySeconds` deletes exactly the ordered
subtraction the compares' comment describes ("each against what the terms
before it leave remaining," `Delay.hpp:680-683`), and removing
`assert(timeR <= capacitySeconds)` deletes half of the compare pair outright
— both are direct mutations of the named mechanism. The two test-file edits
are a different character: `|| true` and `tolerance = capacity` do not touch
`Delay.hpp` at all — they rewrite the test's own pass/fail criterion into a
tautology (a `REQUIRE_TRUE` that can never fail; a `REQUIRE_NEAR` tolerance
wide enough to accept any in-range value). Relative to each test's own
docstring — which states a specific precision it is verifying ("not a
collapse to a short tap"; a 5-sample-or-`maxModSeconds`-bounded tolerance) —
gutting that precision is inside that test's stated job. But relative to the
narrower question "does a broken `Process()` slip past a check that is itself
intact," disabling the check's own assertion is a different attack than
exploiting a gap in what an intact check verifies — the artifact's comments do
not address whether a check tampered with by the same change counts as
having "done its job" or not, so on that narrower framing question I keep
both positions; the finding as a whole is INSIDE because the two `Delay.hpp`
edits alone are sufficient and unambiguous.

**BLOCKS EXECUTION.** The `timeR` compare is not merely unfired (as in
Finding R) — it is deleted from the binary the Makefile marks as the safety
net for exactly this bound, and two `REQUIRE` macros in the suite that is
supposed to gate this change are rewritten to be unconditionally true. A
190/190-green, gate-OK result from this binary is not evidence the width-
spread bound holds; the checks that would have to fail to report that
honestly no longer exist. This is a stronger claim than "the suite has a
coverage gap" (Findings R and S) — the verification mechanism itself is
inoperative for the `timeR` channel and for both defanged assertions, so
treating this run as a passing gate for this stage would be false on its
face.

## Summary of open questions

None of the five findings is OPEN — every one was independently reproduced
with matching commands and matching literal numbers (189/190 exit 1 for P;
190/190 exit 0 heap-overflow-silent for Q; the exact 4500-sample overrun and
firing assert for R; the exact `[0.0, 1.0]`-yet-`dmod=0`-everywhere gate
mismatch for S; the exact 96000-vs-33600 discrepancy for T). The one place
positions genuinely diverge without the artifact settling it is the narrower
half of Finding T's INSIDE/OUTSIDE question — whether tampering with a check's
own assertion text counts as "inside" that check's claimed job, or is a
categorically different attack on the verifier rather than the mechanism
being verified. Both readings are recorded above.
