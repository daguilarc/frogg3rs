# Adversarial audit 4 — shipped capacity checks, axis: get a broken mechanism past them

Fourth of five concurrent postflight passes. Axis: adversarial against the
checks that ACTUALLY SHIPPED (not the plan's description of them). All
commands below were run against the working tree as landed, uncommitted.
`app/dsp/Delay.hpp`, `app/FroggersDspParityTests.cpp` and
`app/check_delay_capacity_parameters_are_swept.py` were backed up before any
edit and restored afterward; restoration is proven by md5 at the end of each
attack section and again at the end of this file.

Baseline before any attack:

```
$ cd app && md5 dsp/Delay.hpp FroggersDspParityTests.cpp check_delay_capacity_parameters_are_swept.py
MD5 (dsp/Delay.hpp) = 65a3ad2c430e1dc28b8a2c3b6d6b52a2
MD5 (FroggersDspParityTests.cpp) = 523a39fc8f0ae58c0af6d2e5e85136cf
MD5 (check_delay_capacity_parameters_are_swept.py) = e077f890dec4ed1be19384884f379fcd
```

Baseline build and run of the DSP parity suite (self-contained, no Sheaf link
needed):

```
$ clang++ -I. -I../External/Sheaf/projects/synth/include -std=c++20 -Wall -Wextra -Wpedantic -O2 FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
(clean, no warnings, exit 0)
$ ./build/froggers_dsp_parity_tests
...
190/190 tests passed
```

## Finding 1 (GETS THROUGH) — the grid's four `dtim` points never visit the
one regime where the ordered subtraction matters, and a broken subtraction
passes all 190 tests

**The mechanism.** `StereoDelay::Process` (`app/dsp/Delay.hpp`):

```cpp
const float baseSeconds = std::min(baseSecondsRaw, capacitySeconds);
const float modSecondsRaw = std::sin(lfoPhase) * p.dmod * baseSeconds * 0.08f;
const float maxModSeconds = capacitySeconds - baseSeconds;
const float modSeconds = std::min(modSecondsRaw, maxModSeconds);
const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
```

The width bound's budget is required to be "what remains after the base and
modulation terms" — `capacitySeconds - baseSeconds - modSeconds`. I mutated
this one line to drop the `- modSeconds` term:

```cpp
const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds);
```

Rebuilt and ran the full suite:

```
$ rm -f build/froggers_dsp_parity_tests
$ clang++ -I. -I../External/Sheaf/projects/synth/include -std=c++20 -Wall -Wextra -Wpedantic -O2 FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
(clean build)
$ ./build/froggers_dsp_parity_tests
...
190/190 tests passed
```

**All three capacity-surface tests pass unchanged**, including
`stereo_delay_width_spread_bound_holds_across_the_reachable_grid`, the one
built specifically to drive a real `StereoDelay` and catch exactly this shape
of defect.

**Why it escapes.** The grid sweeps `dtim` over exactly
`{0.0, 0.5, 0.99, 1.0}`. At 96 kHz, `capacitySeconds = 1.0`. At `dtim ∈
{0.99, 1.0}`, `baseSeconds` is already clamped to (or within a few samples
of) `capacitySeconds`, so `maxModSeconds ≈ 0` regardless of the mutation —
there is nothing left for `modSeconds` to be, so subtracting it or not
subtracting it changes nothing. At `dtim ∈ {0.0, 0.5}`, `baseSeconds` is tiny
(≤ 0.045 s), so the *unmutated* budget has enormous headroom and the mutated
term (which differs from the correct one only by the small `modSeconds`
value) never causes the `min()` clamp to bind differently. The grid contains
no point where `baseSeconds` is meaningfully below `capacitySeconds` **and**
`modSeconds` is large enough, relative to the remaining budget, for the
missing subtraction to matter. `dtim = 0.9` is exactly such a point and is
not in the grid.

**Confirmed by direct computation** (mirrors the grid test's own "expected"
formula, at `dtim = 0.9, dwid = 1.0, widthBalance = 1.0, dmod = 1.0, sr =
96000`, `sin(phase) = 1` — the worst-case phase the grid test itself
deliberately advances to):

```
baseSeconds = ExpMapCompute(0.001, 2.0, 0.9) = 0.935248
modSecondsRaw = 1*1*0.935248*0.08 = 0.074820
maxModSeconds = 1.0 - 0.935248 = 0.064752
modSeconds = min(0.074820, 0.064752) = 0.064752      (mod bound already saturated)

CORRECT  maxSpreadSeconds = max(0, 1.0 - 0.935248 - 0.064752) = 0.0        -> widthSpread = 0.0     -> timeR = 1.000000 s (== capacitySeconds, correct boundary)
MUTATED  maxSpreadSeconds = max(0, 1.0 - 0.935248)            = 0.064752  -> widthSpread = 0.064752 -> timeR = 1.064752 s  (OVER capacity by 6216.19 samples)
```

**Confirmed through the real production path**, not just arithmetic — a
standalone probe driving the mutated `dsp::StereoDelay::Process` at exactly
this grid point, firing an impulse and locating the peak of the real returned
wet pair:

```
capacity=96000 capacitySeconds=1.000000
dtim=0.90 dwid=1.0 wb=1.0 dmod=1.0 sr=96000
peakAbs=0.781519 lagSamples=6216 (capacity=96000)
```

6216 measured against 6216.19 predicted. The read aliases past one full lap
of the line: instead of returning the sample from ~1.0648 s ago (impossible —
the line only holds 1.0 s), it silently returns the sample from 6216/96000 ≈
0.065 s ago, live, audible, and un-flagged. This is the exact defect class
(defect two, route one/two) the whole proposal exists to close, reopened by
one line, and it is invisible to every one of the three shipped capacity
tests.

**Restore and verify:**

```
$ cp /tmp/audit-backup/Delay.hpp.orig dsp/Delay.hpp
$ md5 dsp/Delay.hpp
MD5 (dsp/Delay.hpp) = 65a3ad2c430e1dc28b8a2c3b6d6b52a2   (matches baseline)
```

**Verdict: BLOCKS THE STAGE COMMIT.** This is not a hypothetical evasion —
it is a specific, reachable production defect (any sample rate where
`baseSeconds` sits close to but below `capacitySeconds`, e.g. Delay time
around 0.9–0.95 of its knob range at 96 kHz, with Mod depth and Stereo width
both up) that a one-line regression in the shipped ordering would let through
the entire suite undetected. The grid check's own header comment claims it
"sweeps a real `dsp::StereoDelay` across Delay time, Stereo width, Width
balance and Mod depth" and asserts the bound "holds across the reachable
grid" — the grid does not reach the point where the three-term ordering does
its only real work. The fix is not to touch the mutation (already reverted);
it is to widen the swept `dtim` grid to include a point in the regime where
`baseSeconds` is close to, but strictly below, `capacitySeconds` while
`dmod > 0` — e.g. `0.9` or `0.95` alongside the existing `{0.0, 0.5, 0.99,
1.0}` — so the ordered-subtraction interaction actually gets exercised.

## Finding 2 (NEITHER BLOCKS, RECORDED) — `check_delay_capacity_parameters_are_swept.py` is blind to assertion strength; it verifies literal diversity in source text, not that any REQUIRE runs against those literals

The gate's own docstring is explicit about its scope: it fails when a
tracked parameter is pinned to one literal value across every capacity-surface
test body. It says nothing about whether those bodies still assert anything.
I tested this directly, without touching the real tree: copied
`FroggersDspParityTests.cpp` into a scratch directory and mechanically
disabled every `REQUIRE_TRUE`/`REQUIRE_NEAR` inside the three capacity-surface
test bodies (turning each into a no-op condition) while leaving every literal
value — the `dtims[]`/`dwids[]`/`wbs[]`/`dmods[]` arrays, the `SetWidthBalance`
calls, the word "capacity" — untouched:

```
$ python3 app/check_delay_capacity_parameters_are_swept.py /tmp/gate_test_dir
check-delay-capacity-parameters-are-swept: capacity-surface checks found: stereo_delay_width_spread_never_reads_past_the_line_capacity, stereo_delay_width_spread_bound_is_inert_away_from_capacity, stereo_delay_width_spread_bound_holds_across_the_reachable_grid
  sample rate: [48000.0, 96000.0]
  Delay time (p.dtim): [0.0, 0.3, 0.5, 0.99, 1.0]
  Stereo width (p.dwid): [0.0, 0.5, 0.75, 1.0]
  Width balance: [0.0, 0.5, 1.0]
  Mod depth (p.dmod): [0.0, 1.0]
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
EXIT=0
```

The gate reports OK against a copy whose capacity assertions do nothing. This
was run against a scratch copy, not the real tree — nothing under `app/` was
touched for this finding, no restore needed.

**Why this is not itself a defect to fix here.** The gate was never scoped to
verify correctness; three prior adversarial passes each found one parameter
pinned to a single value, and this gate closes exactly that shape of hole —
a parameter *absent* from the sweep. It was not built to, and its own header
does not claim to, detect an assertion that has been weakened while the
sweep's literal values are kept intact. The actual correctness guarantee for
the capacity bound rests entirely on the three C++ `TEST_CASE` bodies
actually running and their `REQUIRE_*` calls actually holding — which is a
real dependency this change does not name anywhere in `proposal.md`'s
"Affected gates" list. `make test` running the compiled `DSP_TEST_BIN` is the
only thing standing between a weakened assertion and green.

**Verdict: NEITHER BLOCKS.** Not a defect in the landed gate — it does the
job its header says it does. Worth naming because a future editor who reads
"the sweep gate is green" as "the capacity bound is exercised and enforced"
is wrong in exactly the way this file demonstrates; the gate proves coverage
*breadth*, never assertion *strength*. No repair is proposed here since
inventing one would be scope the operator did not ask this audit to fill.

## Finding 3 (NEITHER BLOCKS, CONFIRMED FIXED) — the decorrelation liveness gate now correctly rejects non-finite

The prior hole (recorded in `research/adversarial-check-audit.md`) was that
the liveness gate read `rmsL != 0.0`, which is `true` for a NaN operand. The
shipped assertion (`FroggersDspParityTests.cpp:9583`) is:

```cpp
REQUIRE_TRUE(std::isfinite(rmsL) && rmsL != 0.0 && std::isfinite(rmsR) && rmsR != 0.0);
```

Confirmed the language-level semantics this depends on, on this toolchain,
rather than asserting it from memory:

```
$ clang++ -std=c++20 -O2 /tmp/isfinite_check.cpp -o /tmp/isfinite_check && /tmp/isfinite_check
isfinite(NaN)=0 isfinite(Inf)=0 (NaN!=0.0)=1
```

`NaN != 0.0` alone is `true` (the old hole, reproduced), but `&&`
short-circuits on `std::isfinite(rmsL)` being `false` first, so the whole
clause is `false` for a NaN or an Inf on either channel. The specific hole
the proposal names is closed. No production fault was injected to drive an
actual NaN through `StereoDelay::Process` for this check (would require a
separate, unrelated defect to manufacture one); the verification here is that
the guard's own logic cannot be defeated by the exact failure mode previously
recorded, which is what "closed" means for this finding.

**Verdict: NEITHER BLOCKS.** Confirmed repaired; recorded so a fifth pass
does not re-spend effort re-deriving the same isfinite fact.

## Finding 4 (NEITHER BLOCKS, RECORDED) — the storm test's silence check gives zero capacity assurance, and could read as if it did

`randomize_all_storm_test_never_blows_out_or_permanently_silences`
(`app/FroggersAudioRoutingTests.cpp:862`) checks, per channel, over 200
random full-engine draws, that peak amplitude exceeds `kSilenceEpsilon =
1.0e-4f` somewhere in one second of audio. This is a blowout/silence smoke
test across the whole engine, not a Delay-page or capacity-surface check —
it is absent from `check_delay_capacity_parameters_are_swept.py`'s detection
(it contains neither "capacity" nor `SetWidthBalance`) and absent from
`proposal.md`'s "Affected gates" enumeration.

The capacity-aliasing defect demonstrated in Finding 1 produces a *louder*,
not quieter, signal — a short, wrong-length echo instead of a silent one —
so this test structurally cannot detect it: a channel reading 6216 samples
instead of 96000 is exactly as far from `kSilenceEpsilon` as a correct
channel. No mutation was run against the storm test itself for this finding;
the reasoning is a direct consequence of Finding 1's own measured output
(`peakAbs=0.781519`, far above `1.0e-4f`) and needed no separate command to
settle.

**Verdict: NEITHER BLOCKS.** Not a hole in the storm test on its own axis (it
was never meant to measure capacity); recorded because "the storm test is
green across 200 randomized draws" carries no information about whether any
draw landed near the capacity boundary, and the proposal's Impact section
does not say so.

## Not pursued in this pass, and why

- **Order-masking of the `baseSeconds` clamp by the `modSeconds` bound** — the
  proposal's own text already states, as a measured finding, that an
  unclamped `baseSeconds` telescopes through the `modSeconds` bound back to
  exactly `capacitySeconds`, and that the two candidates (per-term bounds vs.
  a single guard inside `ReadAt`) measured numerically identical on every
  scenario tried. Re-deriving that arithmetic here would be re-reading lines
  already read; I hand-verified the algebra in isolation (`baseSecondsRaw =
  2.0, capacitySeconds = 1.0` gives `modSeconds = -1.0`, `timeL = 1.0`
  exactly) and it matches the proposal's claim, so no new finding follows
  from it.
- A fifth mutation attempt (reordering the three `std::min` calls themselves
  rather than one term's budget) was not run; Finding 1 already demonstrates
  the grid's blind spot is in its *swept domain* (which `dtim` values are
  tried), not merely in one specific formula bug, so further mutations of the
  same shape would land in the same untested gap and add no new information.

## End-of-pass verification

```
$ cd app && md5 dsp/Delay.hpp FroggersDspParityTests.cpp check_delay_capacity_parameters_are_swept.py
MD5 (dsp/Delay.hpp) = 65a3ad2c430e1dc28b8a2c3b6d6b52a2
MD5 (FroggersDspParityTests.cpp) = 523a39fc8f0ae58c0af6d2e5e85136cf
MD5 (check_delay_capacity_parameters_are_swept.py) = e077f890dec4ed1be19384884f379fcd
```

All three match the baseline recorded at the top of this file. `build/`
artifacts from this pass's own compiles were removed
(`rm -f build/froggers_dsp_parity_tests`); no other file under `app/` was
touched.
