# Proposal — `frogg3rs-pm-constant-deviation`

**Created 2026-09-06. PLANNED ONLY: nothing executes before the operator's go.
Not yet preflighted.** This design was applied once without a proposal as
`ed956cd` and reverted as `3899cf4` the same evening; that commit is the
reference diff, not an approval.

Paths are repo-root relative. Line numbers are 2026-09-06 reads of `main` at
`3899cf4`.

## What the operator reported

With the PM rate knob at its floor and the per-VCO PM depth knobs up, VCO1's
phase modulation is inaudible and VCO2's and VCO3's are barely audible. The
floor was raised from 0.3 Hz to 2 Hz earlier today (`e69c5d9`) and it did
not help enough.

## What the tree does now

**The path.** Each VCO owns one PM LFO (`pmLfoPhase`, `app/dsp/Vco.hpp:157`).
`StepPmLfo(pmRateKnob01, sampleRate)` (`:212-218`) maps the shared rate knob
exponentially to `[kPmLfoMinHz, kPmLfoMaxHz]` = [2, 20] Hz (`:117-118`),
returns `Sine01(pmLfoPhase)` (a sine in [-1, 1], `app/dsp/DspMath.hpp:37-42`)
and advances the phase by `hz / sampleRate`. `Process()` (`:225-233`) builds
`pmOffset = kPmLfoDepth * PmDepthScale(pmKnob01) * StepPmLfo(...)` with
`kPmLfoDepth` 0.15 (`:119`) and adds it to the carrier phase through
`WrapPhase` (`:232`). The only audible effect of a phase offset on a lone
oscillator is its slope, the instantaneous frequency deviation:
`d(pmOffset)/dt = 2 pi x 0.15 x PmDepthScale x rate` Hz peak, 0.94 Hz per
Hz of LFO rate at full depth.

**Why the floor cannot fix it.** At 2 Hz the peak deviation is 1.9 Hz: 16
cents on a 200 Hz carrier, 3 cents at 1 kHz, 0.7 cents at 5 kHz
(`kPitchMaxHz`, `:142`). A vibrato needs tens of cents to be plain; at 1 kHz
that needs a rate near the 20 Hz ceiling, which would leave the knob nothing
to do. The three VCOs differ only by carrier pitch in this respect, which is
why the report names VCO1: at the same Hz of deviation a higher carrier moves
fewer cents. (The default pitches were not traced; that is task 1.3.)

**What the firmware did.** The port comments (`Vco.hpp:10-15`) cite
`08b5fd3:src/core/FroggersEngine.hpp:135-137,706-712`: the same fixed
`x_pmLfoDepth` 0.15 and a 0.05 to 20 Hz range, so the firmware's PM had the
same rate-proportional depth and was near-silent at its floor too.

**What is asserted today.** `app/FroggersDspParityTests.cpp:6255` (knob 0 maps
to the floor rate), `:6274,6288` (midpoint pinned), `:6306` (rate shared,
depth decoupled), `:6361,6365` (floor clears the audible-rate bound, LFO
moves). Every one reads the phase or the rate; none measures the deviation.
`openspec/specs/froggers-vco-topology/spec.md:91-115` requires the floor to
be "a slow but plainly audible modulation rate", also stated in cycles, not
in deviation. `MANUAL.md:376` describes PM rate as the shared LFO rate.

## Design

**Hold the pitch deviation constant across the rate knob.** `StepPmLfo`
returns `Sine01(pmLfoPhase) * (kPmLfoMaxHz / hz)`: unity at the top of the
knob, ten at the floor. The peak deviation becomes
`2 pi x kPmLfoDepth x kPmLfoMaxHz x PmDepthScale` = 18.8 Hz at full depth
at every rate; the rate knob sets only how fast the wobble runs, and the
depth knobs alone set how far. This is how an LFO-to-pitch vibrato is
controlled on synthesizers generally (depth and rate independent); the
fixed-phase-swing form is what made this one collapse at low rates. The
top of the knob sounds exactly as it does today, since the scale is 1 there.

The phase swing at the floor is 1.5 cycles peak. `WrapPhase` is
`p - floor(p)` (`app/dsp/DspMath.hpp:45-48`), which folds an offset of any
magnitude into [0, 1). `EvalWaveMorph` reads only the wrapped phase, and
the ring-mod carrier (`ringCarrierPhase`) is untouched.

**Decision reserved for the operator (aesthetic, not derivable):** constant
deviation in Hz (this design: the same sound as today's top-of-knob, bass
carriers wobble more in cents than trebles) versus constant deviation in
cents (a pitch-proportional vibrato, equal at every carrier, a new sound at
every rate). The Hz form is proposed because it changes nothing at the
knob's top and is one multiply.

**Measurement before assertion.** A parity test measures the deviation
directly, `kPmLfoDepth x peak |delta StepPmLfo| x sampleRate`, at knob 0,
0.5 and 1, and asserts all three within 3% of 18.8 Hz. Run against today's
code it must fail at knob 0 (1.9 Hz) and 0.5 (5.9 Hz) and pass at 1: the
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
- Constant deviation in cents (the reserved decision above; if chosen, this
  proposal is rewritten before execution).
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
