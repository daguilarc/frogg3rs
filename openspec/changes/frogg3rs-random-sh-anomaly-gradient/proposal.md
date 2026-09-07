# Proposal — `frogg3rs-random-sh-anomaly-gradient`

**Created 2026-09-06. Preflighted inline 2026-09-06 (rulings under "Preflight
rulings"); this revision is the executable text.**

Paths are repo-root relative. Line numbers are 2026-09-06 reads of `main` at
`58e232d`.

## What the operator asked for

Organise the six Random S&H sources so that each one's tendency to produce
anomalous values scales inversely with its step period and its slew: the
shortest, least slewed lane draws from the tails of the distribution and
resamples every step; the longest, smoothest lane hugs the centre and
repeats. Grade every shaping axis (deja vu, spread, quantization, slew) along
that one ordering, lane 1 to lane 6, and give the locked bags a way to be
redrawn.

## What the tree does now

**The six sources.** Five are `dsp::RandomShLane` instances
(`app/dsp/RandomShLane.hpp:133-252`), constructed once from fixed seeds
(`app/FroggersModulation.hpp:221-227`, `kRandomShSeeds` at `:648-650`) by
the factories `lanes::MakeSource1..5` (`RandomShLane.hpp:271-306`). The sixth
is Sheaf's `GangedRandomLfoProcessor<1>` (`gangedRandomLfo6_`,
`FroggersModulation.hpp:681`, default-constructed), which each round draws a
uniform centre and a normal target around it (sigma `targetInternalSigma`,
`External/Sheaf/projects/synth/include/synth/DspRandomLfo.hpp:419-430`),
waits, then glides to it with `ShapedInterpolate`
(`DspRandomLfo.hpp:106-110`; the interpolant is
`Sheaf DspMath.hpp:139-150`: a blend, by a per-round uniform `shape`
(`DspRandomLfo.hpp:433-436`), of linear `t` and the raised cosine
`0.5 - 0.5 cos(pi t)`, so the output is a monotone path from the previous
target to the new one). One wait-plus-glide round spans sixteen quarter
notes, two thirds waiting and one third gliding
(`FroggersModulation.hpp:324-345`, `kQuarterNotesPerMove`). That is
Marbles' Y: a slow sampled random with the STEPS knob at its smoothest; not
a random walk, which is what the spec (`openspec/specs/froggers-marbles-modulator/spec.md:8`)
calls it.

**The lane's axes.** A lane has a bag of eight slots drawn from its own
`RGen` at construction (`RandomShLane.hpp:171-174`); `Increment()`
(`:181-207`) implements deja vu in three regimes: knob 0 overwrites the
current slot every step, 0 to 0.5 overwrites with probability
`2*(0.5-knob)`, exactly 0.5 never overwrites and walks the index (a locked
8-step phrase), above 0.5 never overwrites but jumps the index to a random
slot with probability `2*(knob-0.5)`. `Process()` (`:214-224`) quantizes the
slot value to `quantizeLevels_` (`:217-221`), narrows it linearly around 0.5
by `spread_` and offsets by `bias_` (`:222`), then runs it through a one-pole
slew (`filter_`, cutoff set in cycles per sample at `:175`;
`OnePoleLowPass::SetAlphaFromNatFreq`, `app/dsp/DspMath.hpp:98-104`, gives
`alpha = 1 - exp(-2 pi f)`). The linear narrowing runs from constant (spread
0) to uniform (spread 1) and can never reach Marbles' bimodal tails.

Three constructor arguments are the same at every call site: `bagSize` is 8
in all five factories, `stepChance` is 1.0 in all five, `bias` is 0 in all
five (`:273,280,290,298,305`); no test constructs a lane except through the
factories (`app/FroggersDspParityTests.cpp:1152-1156,1180-1181,1192,1202,1224,4639,4660`).
The only writers of `slots_` are the constructor loop (`:173`) and
`Increment()`'s fresh-draw branch (`:204`).

**The current table** (`RandomShLane.hpp:271-306`; tick multipliers
`FroggersModulation.hpp:532-536`):

| lane | tick | deja vu | spread | quantize | cutoff |
|---|---|---|---|---|---|
| 1 | quarter | 0.5 locked | 1.0 | none | `kFastCutoff` 0.45 |
| 2 | eighth | 0.5 locked | 1.0 | none | 0.45 |
| 3 | eighth triplet | 0.5 locked | 0.3 | none | 0.45 |
| 4 | quarter | 0.0 fresh | 1.0 | 5 levels | 0.45 |
| 5 | four quarters | 0.0 fresh | 1.0 | none | `kSlowCutoff` 0.002 |
| 6 | 16-quarter round | n/a | n/a (`targetInternalSigma` 0.15) | none | glide |

`kSlowCutoff` at 0.002 cycles per sample is a time constant of
`1/(2 pi 0.002)` = 80 samples, 1.7 ms at 48 kHz: a click softener, not a
glide. Spread and quantization are single exceptions (lanes 3 and 4), slew
is one exception (lane 5), and no axis is a function of anything. The narrow
lane is the fastest and the "smooth" lane draws full range, which is the
opposite of the requested ordering.

**The bags never reset.** No `Reseed`/`Reset` exists on `RandomShLane`;
`RandomizeAll` (`FroggersModulation.hpp:1485`) is a free function of
`(ParameterManager&, FroggersModulationDrillIn&, FroggersParameterModel&)`
with no path to the slate; the slate itself owns no generator (its only
random draws are the lanes' `RGen`s and Sheaf's LFO). `ResetAll` (`:1643`)
does not name the lanes. With constant seeds, every launch on every machine
plays the same eight numbers in the locked lanes.

**Where the current characters are asserted.** `app/FroggersDspParityTests.cpp`
`:1149` (pairwise independence), `:1179` (same seed reconstructs), `:1189`
(source 3 narrow and centred), `:1201` (source 4 five levels), `:1214`
(locked loop replays), `:4638-4680` (locked versus fresh regimes);
`app/FroggersMarblesClockTests.cpp:108` (per-source rate ratios, which
counts index CHANGES as ticks and says so at `:96-106`: exact only while no
lane jumps), `:149` (no transport, no advance), `:171` (source 6
tempo-following input). `openspec/specs/froggers-marbles-modulator/spec.md`
fixes the labels, the "exactly one narrow" rule (`:49-55`), and the
per-source rates (`:57-58`).

**The manual.** `MANUAL.md:131` describes the six as "sample-and-hold /
LFO-style random lanes" and says nothing about how they differ.

## Design

**One ordering, four graded axes.** Lane number is the rank of an anomaly
weight `w = P(fresh draw) x tail weight / (period x slew)`. Every shaping
constant is monotone in the lane number. The table this change installs:

| lane | period | deja vu | jump or fresh per step | spread | quantize | slew (time constant) |
|---|---|---|---|---|---|---|
| 1 | 1/3 quarter | 0.0 | fresh every step | 0.95 near-bimodal | 3 levels | `kFastCutoff` (near-instant) |
| 2 | 1/2 quarter | 0.25 | fresh on half the steps | 0.85 | 5 levels | 5 ms |
| 3 | 1 quarter | 1.0 | eight values, index random every step | 0.7 | 8 levels | 20 ms |
| 4 | 2 quarters | 0.75 | 4-bar phrase, index jumps half the time | 0.6 | none | 100 ms |
| 5 | 4 quarters | 0.51 | 8-bar phrase, one jump per 50 steps | 0.4 | none | 200 ms |
| 6 | 16-quarter round | n/a | a fresh target per round | 0.25 | none | glide |

Deja vu values are the knob positions `Increment()` already implements;
0.51 gives a jump probability of 0.02 per step, one stumble every 50 steps
(about 100 seconds at 120 BPM at lane 5's period). Periods are the tick
multipliers: lanes 1, 2, 3 at 3, 2, 1 ticks per quarter; lane 4 at one tick
per two quarters; lane 5 at one per four. Loop phrases: 8 steps x 2 quarters
= 4 bars for lane 4, 8 x 4 = 8 bars for lane 5.

**Spread becomes a distribution shape.** One free function
`dsp::ShapeSpread(float u01, float spread)` in `RandomShLane.hpp` replaces
the linear narrowing: centre the draw, `v = 2u - 1`; with exponent
`k = spread <= 0.5 ? 1/(2 spread) : 2 (1 - spread)` (spread 0 returns the
centre), `v' = sign(v) |v|^k`; result `0.5 + v'/2`. Three checkpoints define
it: spread 0.5 is the identity (uniform), spread 1.0 sends every draw to 0
or 1 (bimodal), spread 0 sends every draw to 0.5. Spread below 0.5
contracts toward the centre (mean |v'| falls), above 0.5 expands toward the
extremes (mean |v'| rises); no spread value bounds the output away from 0
and 1, so the check on a spread is the mean absolute deviation, not a
range. Marbles gets the same three points from an inverse-CDF table; a curve
needs no table.

**Shape first, then quantize.** `Process()` becomes shape, quantize,
filter (the shape clamps its own result and rounding a value in [0, 1] to a
grid stays in [0, 1], so no separate clamp remains). The order matters at lane 1: quantizing to three levels first would
put a third of the raw draws at the centre level and the shape would leave
them there; shaping first puts 94% of draws at an extreme (at spread 0.95,
`|v|^0.1 > 0.75` whenever `|v| > 0.056`) and the remaining 6% at the
centre level, so the three-level grid is live and the lane is tail-heavy.
The shape is applied to the slot value when read, so a locked bag's stored
values are shaped too, as today; `PopulateUiState` keeps publishing the raw
slots.

**Source 6 gets the same shape on its output.** Its target draw is uniform
inside Sheaf (`DspRandomLfo.hpp:419`) and `GangedRandomLfoInput` carries no
spread, so the slate applies `ShapeSpread` to `Output(0)` at
`FroggersModulation.hpp:392` with `kSource6Spread` 0.25. Shaping the glide's
output rather than its targets bends the path toward the centre slightly
instead of only its endpoints, and the source-6 visualizer (Sheaf's, reading
Sheaf's state) draws the unshaped path; both are stated as the approximation
they are and preferred to a Sheaf edit, which the standing ruling reserves
for changes that make Sheaf more flexible in general.

**Slew constants.** Slew is graded across all five lanes because the
ranking check below measures it: with lanes 1 and 2 both at `kFastCutoff`,
both stepping 0 to 1, their peak first differences are identical
(`alpha x 1.0`) and no strict order exists. One constexpr
`lanes::SlewCutoff(float tauSeconds)` = `1 / (2 pi tau kSlewReferenceSampleRate)`
with `kSlewReferenceSampleRate` 48000 defines the mapping once (inverse of
`SetAlphaFromNatFreq`), and four constants `kSlew5ms`, `kSlew20ms`,
`kSlew100ms`, `kSlew200ms` name the time constants; `kFastCutoff` stays as
lane 1's near-instant value. At 48 kHz the cutoffs are 6.63e-4, 1.66e-4,
3.32e-5 and 1.66e-5 cycles per sample; `kFastCutoff` at 0.45 is a time constant
of a third of a sample. They are tempo-independent and, like
today's, sample-rate dependent (the lane is built before the slate knows
its rate; at 96 kHz every time constant halves). `kSlowCutoff` is deleted
(no lane uses it after the table); the one comment naming it,
`app/dsp/FilterFx.hpp:706`, is reworded (`:733` names only `kFastCutoff`,
which stays).

Lane 5 is 200 ms, not longer, because of the margin against lane 6: lane
6's peak slope is bounded by `1.57 x delta-target / glide-samples x 2`
(raised-cosine peak, the shortest sampled glide near 52k samples at 120 BPM,
the shape's derivative `2|v|` at spread 0.25), about 4e-5 per sample; lane
5's peak is `alpha x step` = 1.04e-4 x ~0.65 at 200 ms, 1.7x above it, and
would be within noise of it at 400 ms.

**Quantization** stays the existing `quantizeLevels_` rounding, graded 3,
5, 8, none.

**The constructor loses its three constants.** `bagSize`, `stepChance` and
`bias` are removed (hygiene, §8.0: identical at every call site, so they are
capability nobody invokes); `size_`, `probability_` and `bias_` go with
them, `Increment()` loses its skip branch, `kNumSlots` is the bag size
everywhere, `UiState.size` keeps publishing `kNumSlots` for the visualizer
(`app/FroggersRandomShVisualizer.hpp:38-39` clamps it). The constructor
becomes `(seed, dejaVuKnob, filterCutoffCyclesPerSample, spread,
quantizeLevels)`.

**Ticks are counted.** `RandomShLane` gains `uint32_t tickCount_`,
incremented at the top of `Increment()`, read by `TickCount()`; the slate
exposes `RandomShLaneTickCountForTest(laneIx)`. The clock test's
index-change proxy is exact only while every lane walks its index by one;
lanes 3, 4 and 5 now jump to a random slot, one time in eight the same slot,
so the test counts ticks instead.

**The bags can be redrawn.** `RandomShLane::Reseed(uint32_t seed)`
reconstructs `rgen_` and refills the eight slots; the constructor calls it.
`index_` and `filter_` are left alone so a reseed mid-play slews from the
current output instead of clicking. The slate gains
`static constexpr uint32_t LaneSeed(size_t laneIx, uint32_t salt)` =
`kRandomShSeeds[laneIx] ^ salt`, the one definition of how a salt becomes
five distinct seeds, used by the constructor's initializer list and by
`ReseedRandomShLanes(uint32_t salt)`. `FroggersModulationSlate()` delegates
to a new `explicit FroggersModulationSlate(uint32_t launchSalt)` with one
`std::random_device` draw, so two launches differ and a test that wants a
fixed slate passes a salt; `gangedRandomLfo6_` is constructed from the same
salt (today it is default-constructed, which Sheaf seeds from
`std::random_device` per instance, `DspRandomLfo.hpp:263-264`), so a
salted slate is deterministic in all six sources. `RandomizeAll` gains a fourth parameter,
`FroggersModulationSlate&`, and its level-0 branch (the gesture that
re-rolls a patch) calls `slate.ReseedRandomShLanes(salt)` with a salt drawn
as `manager.NextRandomIndex(1 << 32)` (`ParameterModulation.cpp:3625-3634`,
a `uniform_int_distribution<size_t>` on the manager's engine, so a fixture's
injected index source still governs it); drilled-in levels randomize one
parameter's depths and do not reseed. The only production caller is
`app/FroggersAppCore.hpp:725`, which owns the slate as `modulation_`; the
test callers (`app/FroggersModulationTests.cpp`, 20 sites) all sit on a
fixture that owns `slate`.

**Measurement before assertion.** A new test in `app/FroggersMarblesClockTests.cpp`
(the parity TU includes no Sheaf, and the slate is the one definition of the
periods) drives one slate at 120 BPM and 48 kHz for 256 quarter notes
(6,144,000 samples: 768, 512, 256, 128 and 64 ticks for lanes 1 to 5 under
the new table, 16 source-6 rounds), reading every lane's output each sample
through `RandomShLaneOutputForTest(laneIx)` and `RandomSh6OutputForTest()`,
and records two numbers per lane: mean |first difference| per sample over
the whole span (activity: step size times step rate, slew-independent
because a one-pole is monotone) and peak |first difference| (slew times
step size). It prints the six pairs and asserts both rankings
1 > 2 > 3 > 4 > 5 > 6 strictly. Run first against today's constants, it
must fail: today's mean-activity order is 3 > 2 > 4 > 1 > 5 > 6 (lane 3
steps three times per quarter, narrowed to 0.3 of the range) and the peak
order ties lanes 1, 2 and 4 at `alpha x 1.0`; that failure is the positive
control. After the table lands it passes and the printed numbers go into
the ledger. Expected orders of magnitude: mean 6e-5, 4e-5, 1.7e-5, 6e-6,
3e-6, 9e-7; peak 0.94, 4e-3, 1e-3, 2e-4, 7e-5, 4e-5. If the measured lane 5
versus lane 6 margin is under 1.5x the finding supersedes this text (§9)
before any constant is tuned to the metric. Until group 5 lands the salted
constructor the slate's lanes are fixed-seeded and source 6 is
`random_device`-seeded (Sheaf's `DefaultRandomDrawSource()`,
`DspRandomLfo.hpp:263-264`), so the before line's source-6 pair is one
draw; the after line is taken with a fixed salt.

**Labels stay.** "Random S&H" is the modular name for a clocked random
value held between clocks, and Marbles calls its own X the random sampler;
the six keep their names. The spec and manual say what each lane does.

## Preflight rulings (2026-09-06)

1. The ranking premise failed analytically for lanes 1 and 2 (identical
   peak slope) and was fragile for 5 versus 6 at 400 ms; slew is now graded
   on all five lanes, lane 5 is 200 ms, and the test asserts mean activity
   as well as peak slope.
2. Task 3.2's "spread 0.25 keeps every draw within 0.25 of the centre" was
   false under the proposal's own curve (extremes map to extremes at every
   spread); the check is mean absolute deviation ordering 0.25 < 0.5 < 0.75.
3. Shape then quantize, not the reverse, so lane 1's three levels and its
   near-bimodal spread are both live.
4. `RandomizeAll` had no path to the slate and the slate had no generator;
   the slate parameter, `LaneSeed`, and the manager's index draw are the
   traced mechanism. Level 0 only.
5. The clock test's index-change proxy breaks under jumping deja vu; ticks
   are counted.
6. Hygiene inside the touched files: three constant constructor arguments
   removed; `kSlowCutoff` and its two comment mentions; the factory and
   struct comments that carry planning history ("validate by ear",
   "provisional", "most likely to be overruled") are replaced by behaviour
   statements. The sweep covered `app/dsp/RandomShLane.hpp`,
   `app/dsp/FilterFx.hpp` (mentions only), the lane, seed, rate and
   Randomize regions of `app/FroggersModulation.hpp`, the three test files
   named above, `MANUAL.md:131`, `QUICK_DICT.md:15`, and the marbles
   spec. The rest of `app/` was swept by `frogg3rs-omni-audit-repairs`
   (61/62, 2026-09-06) and is not re-swept here.
7. The spec's "one move spans sixteen quarter notes" names the whole
   wait-plus-glide round; the delta says round.

## Overlaps with active changes

Enumerated from `openspec list` on 2026-09-06. `frogg3rs-drilled-in-randomize-floor`
(0/15) edits `RandomizeParameterModulationDepths` (`app/FroggersModulation.hpp:867`)
and `RandomizeAll`'s drilled-in branch and descent (its proposal cites
`:1536`, `:1571`). This change edits `RandomizeAll`'s signature and level-0
branch and the 20 test call sites; the two touch the same function in
disjoint branches, and whichever lands second re-cites its lines. No other
change names `RandomShLane.hpp`, the Marbles clock tests, or the
marbles-modulator spec.

## Not in this change

- Renaming the sources.
- A source that samples an external signal (Marbles' external processing
  mode): a new source kind, not a setting on these.
- Any Sheaf edit; source 6's shaping is app-side.
- Tempo-following or sample-rate-following slew.

## Spec deltas

`froggers-marbles-modulator`: MODIFIED "Six Marbles-style Random S&H
sources" (source 6 described as sampled-then-glided, not a walk; the
ordering rule; the ranking scenario), MODIFIED "Stepped source character
constants are new behavior" (becomes the graded table and the
distribution-shape checkpoints), MODIFIED "Sources advance on the master
clock quarter-note pulse" (the new per-source rates), ADDED "Randomize All
redraws the locked bags".

## Gates

| gate | loads | control | run when |
|---|---|---|---|
| `nice make -C app -j2 test` (runs every `app/build/froggers_*_tests` binary) | all of `app/` | the ranking test red on today's constants before the table lands | after every task group |
| `nice cmake --build app/vst/build -j2 && ctest --test-dir app/vst/build --output-on-failure` | `app/vst/` | the rebuild | at the end |
| `openspec validate --all --strict` | `openspec/` | — | after the spec delta |

Before line (2026-09-06, before any edit): `make -C app test` exit 0; parity
147/147, modulation 46/46, marbles clock 8/8; `openspec validate --all
--strict` 27 passed, 0 failed. Ranking test on today's constants (the positive
control, red on `sum[0] > sum[1]`): mean|d| 8.71e-6, 2.26e-5, 1.54e-5,
1.48e-5, 4.31e-6, 1.07e-6; peak|d| 0.386, 0.836, 0.183, 0.941, 0.0122,
1.54e-5 for sources 1 to 6.

## Delivery

Branch from `main`, one commit per task group, no AI attribution lines,
fast-forward push to `main` once the gates are green; no pull request.
