# Proposal — `frogg3rs-pm-constant-deviation`

**Created 2026-09-06. Preflighted 2026-09-07; revised under that audit and
approved for execution.** This design was applied once without a proposal as
`ed956cd` and reverted as `3899cf4` the same evening; that commit is the
reference diff, not an approval.

Paths are repo-root relative. Line numbers are 2026-09-07 reads of `main` at
`c65d603`, re-read at revision time. The first draft's `Vco.hpp` numbers were
seven low throughout: they were taken before `e69c5d9` inserted a comment
block, and the preflight caught every one.

## What the operator reported

With the PM rate knob at its floor and the per-VCO PM depth knobs up, VCO1's
phase modulation is inaudible and VCO2's and VCO3's are barely audible. The
floor was raised from 0.3 Hz to 2 Hz earlier today (`e69c5d9`) and it did
not help enough.

## What the tree does now

**The path.** Each VCO owns one PM LFO (`pmLfoPhase`, `app/dsp/Vco.hpp:164`).
`StepPmLfo(pmRateKnob01, sampleRate)` (`:219-225`) maps the shared rate knob
exponentially to `[kPmLfoMinHz, kPmLfoMaxHz]` = [2, 20] Hz (`:124-125`),
returns `Sine01(pmLfoPhase)` (a sine in [-1, 1], `app/dsp/DspMath.hpp:37-42`)
and advances the phase by `hz / sampleRate`. `Process()` (`:232-278`) builds
`pmOffset = kPmLfoDepth * PmDepthScale(pmKnob01) * StepPmLfo(...)` with
`kPmLfoDepth` 0.15 (`:126`) and adds it to the carrier phase through
`WrapPhase` (`:239`). The only audible effect of a phase offset on a lone
oscillator is its slope, the instantaneous frequency deviation:
`d(pmOffset)/dt = 2 pi x 0.15 x PmDepthScale x rate` Hz peak, 0.94 Hz per
Hz of LFO rate at full depth.

**Why the floor cannot fix it.** At 2 Hz the peak deviation is 1.9 Hz, and it
is the same 1.9 Hz on every carrier: the deviation is `2 pi x kPmLfoDepth x
rate`, which carries no pitch term. In cents it therefore shrinks as the
carrier rises -- 29 cents at 110 Hz, 15 at 220, 10 at 330, 3 at 1 kHz, 0.7 at
5 kHz (`kPitchMaxHz`, `:142`). A vibrato needs tens of cents to be plain, so
the bottom of the rate knob is thin on the low carriers and vanishing on the
upper half of the pitch range; reaching a plain vibrato at 1 kHz needs a rate
near the 20 Hz ceiling, which leaves the knob nothing to do.

**The report's per-VCO ordering is not explained by the code, and this change
does not claim to explain it.** The default patch is 110/220/330 Hz for
VCO1/2/3 (knobs 0.3087/0.4343/0.5077, `app/FroggersParameters.hpp:154`), so
VCO1 holds the LOWEST carrier and takes the MOST cents of the three -- 29
against 15 and 10, the reverse of the reported order. The PM path was then
traced end to end for a per-VCO difference and has none: Ph.mod 1/2/3 reach
VCO 1/2/3 index-for-index (`app/FroggersParameters.hpp:70-72,163` through
`app/FroggersAppCore.hpp:1609-1614`), `PmDepthScale` is a static pure function
of the knob that takes no VCO identity (`app/dsp/Vco.hpp:193-195`), and the
three sum at equal weight (Sustain 1.0 each, `app/FroggersParameters.hpp:190`;
VCO balance 0.5 yields w = 1/3 each, `app/dsp/VoiceEnvelope.hpp:718-735`).
Measured on the built DSP at the floor at full depth, peak deviation is
1.8824 / 1.8839 / 1.8846 Hz for VCO1/2/3 -- equal within 0.002 Hz and matching
the 1.885 Hz prediction, with carrier means landing on 109.88/219.98/329.93 Hz
as the control that the probe was live. The three VCOs are a 1:2:3 harmonic
series at equal level, so a vibrato on the fundamental sits beneath its own
partials; that is a perceptual account, not a traced one, and nothing here
rests on it. What the change rests on is the 1.9 Hz figure, which is thin for
every VCO. If VCO1 still reads as weaker than VCO2 and VCO3 after this change,
the cause is not in the signal path and a separate change owns it.

**What the firmware did.** The port comments (`Vco.hpp:10-15`) cite
`08b5fd3:src/core/FroggersEngine.hpp:135-137,706-712`: the same fixed
`x_pmLfoDepth` 0.15 and a 0.05 to 20 Hz range, so the firmware's PM had the
same rate-proportional depth and was near-silent at its floor too.

**What is asserted today.** `app/FroggersDspParityTests.cpp:6255` (knob 0 maps
to the floor rate), `:6274,6288` (midpoint pinned), `:6306` (rate shared,
depth decoupled), `:6361,6365` (floor clears the audible-rate bound, LFO
moves). Every one reads `pmLfoPhase` or the rate; none measures the deviation.
Five further tests touch PM without reading the LFO's magnitude: the depth
taper pair (`:117,:129`), the wave-morph and pitch-map endpoints, and
`:160`/`:178`/`:193`, which read `Process()` output but assert only a zero
result at zero depth or a boolean divergence between two instances carrying
the same rate. The full inventory was walked in preflight; none goes red.
`openspec/specs/froggers-vco-topology/spec.md:91-115` requires the floor to
be "a slow but plainly audible modulation rate", also stated in cycles, not
in deviation. `MANUAL.md:376` describes PM rate as the shared LFO rate.

## Design

**Hold the pitch deviation constant across the rate knob.** `StepPmLfo`
returns `Sine01(pmLfoPhase) * (kPmLfoMaxHz / hz)`: unity at the top of the
knob, ten at the floor. The peak deviation becomes
`2 pi x kPmLfoDepth x kPmLfoMaxHz x PmDepthScale` = 18.8 Hz at full depth
at every rate; the rate knob sets only how fast the wobble runs, and the
depth knobs alone set how far. The top of the knob sounds exactly as it does
today, since the scale is 1 there.

**What the present form is, checked rather than assumed.** Today's code is
phase modulation in the strict sense. PM's modulation index is the phase
deviation itself, `beta = d(phi) = kp x Am`, independent of the modulating
frequency, while FM's is `beta = df / fm`; so when the modulator rate moves,
PM's index holds and FM's does not
(tutorialspoint.com/analog_communication/analog_communication_angle_modulation.htm,
eeeguide.com/theory-of-frequency-modulation-and-phase-modulation/). Since
`df = beta x fm`, a constant index means PM's frequency deviation rises and
falls WITH the modulator rate. That is the definition of PM, not a fault in
it, and `kPmLfoDepth` being a constant phase swing is what makes this a PM
control. The scaling converts it to constant deviation, the FM form.

The reason to prefer that here is narrow and worth stating honestly, because
the obvious general argument does not survive checking. Synthesizer vibrato
depth is conventionally specified as a pitch deviation in CENTS, rate
independent -- SoundFont's `vibLfoToPitch` is in cents (sfzformat.com/
tutorials/vibrato/, blog.soundparticles.com/synth-tutorial-lfos-explained).
That convention is the cents-constant option in "Not in this change", not the
Hz-constant one taken here. Hz-constant is neither textbook PM nor the vibrato
convention; what it buys is that the top of the rate knob is bit-for-bit what
it is today, at the cost of one multiply, and that the deviation no longer
collapses at the bottom. If the resulting cents spread across the three
carriers reads wrong by ear, cents-constant is the same one-line edit against
`PitchToPhaseIncrement`'s carrier, and this proposal's reasoning above is the
argument for making it.

The phase swing at the floor is 1.5 cycles peak. `WrapPhase` is
`p - floor(p)` (`app/dsp/DspMath.hpp:45-48`), which folds an offset of any
magnitude into [0, 1). `EvalWaveMorph` reads only the wrapped phase, and
the ring-mod carrier (`ringCarrierPhase`) is untouched.

**The Hz form, taken.** Constant deviation in Hz keeps the knob's top sounding
exactly as it does now and costs one multiply; constant deviation in cents
would be a pitch-proportional vibrato, equal at every carrier, and a new sound
at every rate. The operator's go on this proposal is the go on the Hz form.
What it sounds like at the floor at full depth, on the default patch: 274
cents on VCO1, 142 on VCO2, 96 on VCO3 -- a wide warble, and precisely what
the top of the rate knob already produces today, since the scale is 1 there.

**Measurement before assertion.** A parity test measures the deviation
directly, `kPmLfoDepth x peak |delta StepPmLfo| x sampleRate`, at knob 0,
0.5 and 1, and asserts all three within 3% of 18.8 Hz. Run against today's
code it must fail at knob 0 (1.9 Hz) and 0.5 (6.0 Hz) and pass at 1: the
positive control. The reverted commit `ed956cd` carries this test and the
one-line change; on approval the change is cherry-picked, not rewritten.

**Docs and spec.** The manual states what the controls do, in the present
tense, with no account of earlier behaviour. `MANUAL.md:376-378` already
says "Depth and rate are independent"; that sentence is false today (the
audible swing shrinks with the rate) and true after this change, and it
stays. The Ph.mod paragraph (`MANUAL.md:366-368`) gains one clause: the
swing's size does not change with the rate. The code comment on the
multiply states the rule and its reason (the audible quantity is the slope
of the phase offset) and names no earlier constant or fix. The
vco-topology requirement gains the deviation rule and a scenario.

## Overlaps with active changes

Grep over `openspec/changes/*/proposal.md` and `tasks.md` (2026-09-06):
`frogg3rs-guitar-and-solo-variants` (30/33) and `frogg3rs-omni-audit-repairs`
(61/62) mention `Vco.hpp` only as citation targets of already-executed
work; neither has an open task that edits `StepPmLfo`, the PM constants,
or the vco-topology spec. Task 1.1 re-reads both before execution.

## Not in this change

- Any change to the floor or ceiling rates (2 and 20 Hz stay).
- Constant deviation in cents (the alternative weighed above and not taken).
- The firmware.

## Spec deltas

`froggers-vco-topology`: MODIFIED "The PM rate control's minimum is a moving
rate, not a second off switch" (the deviation rule; scenario "The pitch
deviation is the same at every rate").

## Gates

| gate | loads | control | run when |
|---|---|---|---|
| `nice make -C app -j2 test` | all of `app/` | the deviation test red at knob 0 on today's code | after group 2 |
| `nice cmake --build app/vst/build -j2 && ctest --test-dir app/vst/build --output-on-failure` | `app/vst/` | the rebuild | at the end |
| `openspec validate --all --strict` | `openspec/` | — | after the spec delta |

## Delivery

One commit (the cherry-pick of `ed956cd` plus the spec and manual), no AI
attribution lines, fast-forward push to `main`, then rebuild the desktop
app, VST/AU and browser releases; no pull request.
