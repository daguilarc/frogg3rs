# Proposal — `frogg3rs-effect-page-hierarchy`

**Created 2026-09-10**, from an operator review of the Drive page: do its
parameters do what they say, why is Blend's floor inaudible, why does XOR go
quiet at twelve o'clock, does Anti-alias do anything, and what does Drive do
that the others do not.

Every number below was measured by driving `dsp::FrogBlock` and
`dsp::DriveBlendPhase` through the same setters, in the same order, that
`FroggersAppCore::RouteDriveBank` (`app/FroggersAppCore.hpp:1690-1715`) drives
them with, at 48 kHz, with a 220 Hz sine at 0.5 unless a line says otherwise.
Levels are RMS; THD is harmonics 2..12 against the fundamental; "difference"
is the RMS of the sample-by-sample difference against a named reference, in dB
relative to that reference.

## State, 2026-09-11

**Implemented and green.** Gate 371 PASS / 0 FAIL against a 356 PASS baseline,
through `make -C app test -j2` and every test binary run directly by path.
`tasks.md` marks what each group did; group 13 is delivery.

This document has been revised several times DURING execution, because
measurements kept contradicting it. A reader who wants only the corrections
should read these, each of which withdraws something an earlier draft asserted:

- **Section 3** withdraws the alias table. It was measured at 3 kHz, which
  divides 48 kHz exactly, so every fold-image lands on a bin a real harmonic
  occupies and the stated classifier had nothing to count. The replacement
  measures at tones chosen not to divide the sample rate, and the improvement
  FALLS with the note rather than rising as the first draft had it.
- **Section 7** withdraws the argument that the Drive page needs a transparent
  wet path. It held one page to a property neither sibling has -- measured,
  Reverb at every control's default with its master fully up is 8.46 dB below
  its input. The family's actual shared rule is that the master returns dry
  exactly at its floor, which Blend already did.
- **Section 11** records that every wet/dry control on the instrument
  crossfaded linearly, which holds level only between fully correlated legs,
  and what replacing that law did and did not fix.
- The **fold normalization** is withdrawn entirely; see `tasks.md` group 4 for
  the measurement that killed it.
- The **cost gate** is gone: there is no deadline harness in this app, and the
  measured headroom makes the overrun it guarded against impossible.
- **Group 12 -- the Damping level compensation -- is withdrawn entirely**, by a
  second preflight pass run in a fresh context (`preflight.md`). The deficit it
  existed to fix had been measured on the stage's full mixed output with the
  dry floor summed in, and called the wet leg; the wet leg's own loss is
  monotonic and 9.98 dB rather than humped and 6.4 dB. Damping's registered
  default is 0.0 and sits at the loudest end of the travel, not 0.4 near the
  worst. And the damping filter is outside the tank's feedback loop, not inside
  it -- the description fits Delay's tone filter, which is what made task 12.2
  ask for a compensation inside a feedback path that its own safety rule
  forbids. Measured, no static term derived from the filter's coefficient meets
  the task's own 1.5 dB criterion on more than one source, and every one of
  them wrecks the 110 Hz case that passes it today at 0.52 dB. See `tasks.md`
  group 12 for the tables.

Four defects were found by the two preflight audits rather than by execution,
and all four would have shipped silently. The second pass also found that the
spec delta asserted a "stage-independence case" that existed in no test file,
and a Damping requirement whose check was still marked not-yet-delivered; both
now have real cases behind them
(`drive_gain_makes_distortion_and_the_manglers_act_at_any_gain` and
`reverb_damping_darkens_and_quiets_the_tank_while_room_size_does_neither`).
From the first pass: the spec delta contradicted three promoted
requirements with no MODIFIED section, and a default-patch overlay addressing
Drive slot 0 by number would have shipped the instrument 20% wet once that slot
became Wet/Dry. Both are recorded in `preflight.md`.

## What the review found

**Every one of the fourteen controls does what the manual says it does.** Four
answers the manual does not give, and two controls whose travel does not spend
itself where the manual implies.

### 1. The page ships bypassed, and that is what Blend's floor is

`Blend` used to crossfade linearly: `dry * (1 - blend) + phased * blend`;
this change replaced that law with the equal-power crossfade detailed below
(`app/dsp/Drive.hpp:969`). Its default is 0, and at 0 the stage returns dry
exactly, so **every other control on the page is inaudible at the page's own
defaults** — measured bit-identical (-240 dB) for all thirteen. Distortion is
opt-in by construction. That part is deliberate and documented.

What is not documented is what the first part of the travel does. The wet
path's fundamental is out of phase with the dry path at most Drive settings
(measured: at Drive 0.25, 0.5 and 1.0 the wet render correlates NEGATIVELY
with the dry render), so a partial Blend **subtracts** the fundamental before
the distortion is loud enough to replace it:

| Blend (Drive 0.5, rest default) | RMS | THD |
| --- | --- | --- |
| 0.00 (dry) | 0.3535 | 0.86% |
| 0.10 | 0.3009 | 21.5% |
| 0.25 | 0.2610 | 77.0% |
| 0.50 | 0.3291 | 567% |
| 1.00 | 0.5516 | 259% |

The knob's first quarter is a **-2.6 dB notch**, not a fade-in. That is the
"inaudibly low floor": turning Blend up first makes the sound thinner and
quieter. This is NOT inherent to summing a phase-inverting waveshaper against
dry — that framing was wrong. The wet path's fundamental is partly
anti-correlated with dry, and the SIGN of that correlation flips across the
Drive knob (measured correlation of the wet render against dry: -0.22 at
Gain 0.25, -0.35 at 0.50, +0.02 at 0.75, -0.085 at 1.00), which is exactly why
no fixed polarity and no fixed Phase setting removes it: Phase's own remap
(defect 2, below) narrows the dip but cannot close it, because at a bass note
the allpass it rotates barely moves the phase at all. The actual cause is
`Blend`'s crossfade LAW: `dry * (1 - blend) + phased * blend` only holds
level when the two legs are correlated, and holds nothing when the
correlation changes sign across the knob it is supposed to serve uniformly.

The fix is the crossfade law, not the Phase stage: `dsp::DriveBlendPhase::Process`
(`app/dsp/Drive.hpp`) now mixes with an EQUAL-POWER law,
`dry * cos(blend * pi/2) + phased * sin(blend * pi/2)`, energy-preserving
regardless of the correlation between the two legs. Both endpoints are
special-cased to stay bit-exact (blend == 0 returns `dry` unchanged; blend == 1
reaches `phased` with no dry leakage — `std::cos` does not land on exactly
0.0f at the blend == 1 end in float, so that end is pinned directly rather
than left to the trig call). Measured worst-case dip anywhere across
110/220/440/880 Hz and Gain 0.25/0.50/0.75/1.00, Phase at its 0.86 default:
the linear law dipped at all 16 of those points, worst -4.10 dB; the
equal-power law holds exactly 0.00 dB at 11 of the 16 and dips no worse than
-1.09 dB anywhere. The notch is not eliminated — the underlying correlation
still exists and still costs a little level at a few settings — but it no
longer notches audibly across most of the page's travel. Which brings the
next defect.

### 2. Phase spends nine tenths of its travel doing nothing audible — DEFECT

`Phase` used to map linearly to the allpass coefficient,
`a = 0.98 * (2 * knob - 1)`, since replaced by a mapping through the
allpass's own break frequency (`app/dsp/Drive.hpp:926`). An allpass is
unity-gain, so the only way to hear it is through how the rotated wet sums
with dry — exactly the cancellation above. Measured at Blend 0.25, Drive 0.5,
as RMS against a 0.3536 dry:

| input | knob 0.00 | 0.25 | 0.50 | 0.75 | 0.90 | 1.00 |
| --- | --- | --- | --- | --- | --- | --- |
| 220 Hz | 0.2610 | 0.2610 | 0.2610 | 0.2612 | 0.2626 | 0.3330 |
| 880 Hz | 0.2602 | 0.2603 | 0.2607 | 0.2638 | 0.2824 | 0.3606 |
| 2 kHz | 0.2499 | 0.2502 | 0.2522 | 0.2664 | 0.3124 | 0.3555 |
| 6 kHz | 0.2175 | 0.2192 | 0.2346 | 0.2865 | 0.3210 | 0.3297 |

At 220 Hz the first three quarters of the knob move the output by 0.08%. All
of the audible action is in the last tenth, and the knob's DEFAULT (0) sits at
the far end from the only setting that lifts the notch. A linear sweep of `a`
is not a linear sweep of what an allpass does to a bass note: the low-frequency
rotation is negligible until `a` approaches +0.98.

### 3. Anti-alias brightness cannot affect aliasing — DEFECT

`SetAntiAliasBrightness` used to map the knob to a one-pole corner of 0.32..0.5
cycles/sample, which at the oversampler's own 96 kHz rate was
**30.7 kHz to 47.9 kHz** — the whole range sat above the 24 kHz
Nyquist of the output rate, so the filter was nearly transparent to the very
content it existed to remove. This change replaced that one-pole trim with a
clean/grit crossfade (`app/dsp/Drive.hpp:266`) whose clean path runs a fixed
21 kHz corner (`app/dsp/Drive.hpp:200`). Measured with a 7 kHz tone driven hard, whose
28 kHz and 35 kHz harmonics fold back to 20 kHz and 13 kHz:

| knob | corner | alias at 20 kHz | alias at 13 kHz | harmonic at 14 kHz |
| --- | --- | --- | --- | --- |
| 0.00 | 30.7 kHz | -29.07 dB | -34.16 dB | -29.54 dB |
| 0.50 | 38.4 kHz | -29.07 dB | -35.95 dB | -28.74 dB |
| 1.00 | 47.9 kHz | -29.06 dB | -37.49 dB | -28.13 dB |

Across its entire travel the knob moves the 20 kHz alias by **0.01 dB** and
in-band brightness at 14 kHz by **1.4 dB**. The manual is honest about the size
("a brightness adjustment, small either way"); the NAME is what promises a job
the range cannot do, and the underlying one-pole leaves aliasing at -29 dB at
every setting.

A wider range does not fix that either. What does fix it is a decimation
filter with enough slope to work with, and enough oversampling for it to work
on -- both, not either.

**This proposal's first draft said the opposite**, and recommended renaming the
control because "no reachable corner improves the alias-to-signal ratio". That
was an artefact of the measurement, not a fact about the filter. Bin magnitudes
were taken with a rectangular window, whose sidelobes leak the harmonic peaks
into neighbouring bins at about -35 dB -- louder than the aliasing under
measurement. Every filter, corner, oversampling factor and upsampler returned
the same figure because the figure was the window. Hann-windowing the same
measurement drops its floor below -85 dB, and the differences appear.

Measured against a high-factor alias-free reference, with a 3 kHz tone at
Drive 1.0, Shape 0.6, counting the energy a path puts in bins the reference
leaves empty, an earlier draft reported 2x at -61.9 dB against 4x at -78.1 dB.

**That table is withdrawn: 3 kHz is the one tone at which its own method
cannot measure.** 48000/3000 is exactly 16, so every image of a folded harmonic
lands precisely on a bin a genuine harmonic already occupies, and a classifier
defined as "energy in bins the reference leaves empty" has no empty bins to
count. Re-measured at tones deliberately chosen NOT to divide the sample rate,
reporting the worst fold-image against the fundamental (the quantity the
"What each option sounds like" section argues is the audible one), and skipping
any image within 40 Hz of a real harmonic:

| tone | 2x | 4x | 8x | 16x |
| --- | --- | --- | --- | --- |
| 220 Hz | -61.1 dB | -57.5 dB | -56.8 dB | -56.7 dB |
| 1487 Hz | +1.8 dB | **-16.1 dB** | -14.7 dB | -14.4 dB |
| 2971 Hz | +6.3 dB | +1.8 dB | **-19.3 dB** | -19.0 dB |
| 4441 Hz | +4.0 dB | +4.0 dB | +2.7 dB | **-12.4 dB** |
| 5939 Hz | +6.3 dB | +6.3 dB | +1.8 dB | **-20.9 dB** |

At 220 Hz the factor changes nothing, because nothing folds -- the control on
this measurement, and it reads correctly. Above that, **the factor a note needs
rises with the note**: a harmonic that exceeds the oversampled domain's OWN
Nyquist aliases inside that domain, where no decimation filter can reach it. 4x
clears everything up to about 1.5 kHz, 8x to about 3 kHz, 16x to about 6 kHz.
A positive figure means the worst inharmonic partial is LOUDER than the
fundamental, which at Drive 1.0 is itself 21.7 dB down.

This does not overturn the choice of 4x, whose stated job is the notes up to
about A6 (1760 Hz); on the real shipped path 4x improves a 1487 Hz tone by
23.3 dB. It does bound what the control may claim: above roughly 2 kHz the 4x
clean path is not clean, and the manual says so rather than implying the knob
fixes the top of the range.

The linear interpolation on the way up, suspected next, is NOT a problem:
replacing it with zero-stuff-and-filter moves the result by 0.2 dB.

How much there is to clean depends entirely on the note, which is what makes
this a musical control rather than a quality setting. An earlier draft put the
improvement at 12.1 dB at 1.5 kHz, 18.9 dB at 3 kHz and 25.8 dB at 6 kHz --
rising with the note. **That trend is backwards** and is withdrawn with the
table above it: those three figures came from the same bin classifier, and two
of the three tones divide 48 kHz exactly. Improvement FALLS with the note,
because a higher fundamental puts more harmonics past the oversampled domain's
own Nyquist, where the decimation filter cannot reach them. Measured on the
shipped path, at tones chosen not to divide the sample rate:

| input | improvement, grit to clean |
| --- | --- |
| 110 Hz | none: nothing folds |
| 1487 Hz | 23.3 dB |
| 2971 Hz | 8.7 dB |
| 5939 Hz | 7.8 dB |

(At any tone dividing the sample rate -- 3 kHz among them -- the measurement
stops meaning anything: every image lands on a harmonic bin and the classifier
cannot separate them. Not reported as a result, and not used as a test tone.)

So the control becomes real by crossfading between a genuinely clean path and
the path that ships. Measured at 1487 Hz -- inside the range 4x actually
serves, and not a divisor of the sample rate -- from clean to today's grit:

| knob | 0.00 | 0.25 | 0.50 | 0.75 | 1.00 |
| --- | --- | --- | --- | --- | --- |
| alias-to-signal | -18.92 dB | -9.47 dB | -2.92 dB | +1.18 dB | +4.34 dB |
| fundamental | -27.84 dB | -28.47 dB | -29.10 dB | -29.72 dB | -30.30 dB |

Monotonic, 23.3 dB of travel. A positive alias-to-signal means the worst
inharmonic partial is louder than the fundamental, which at Drive 1.0 is itself
far down -- that is the metallic ring the next section describes, and at the
grit end it is what ships today.

The in-band level is NOT constant across the sweep: the fundamental falls
2.46 dB from clean to grit. An earlier draft claimed it did not move at all
("-6.36 dB at every knob position"); it does, by a small but real amount, so
the knob is not purely a grit control and the manual says so. One end is
bit-identical to what ships today, so nothing is taken away.

### 4. XOR's twelve o'clock does not go quiet — it moves the sound upstairs

Measured with the shaper driven (Drive 0.6, Shape 0.4, Blend 1), RMS across the
whole XOR sweep stays between 0.41 and 0.51 — the level barely moves. What
moves is where the energy sits:

| XOR knob | fundamental | energy under 1 kHz | energy over 5 kHz |
| --- | --- | --- | --- |
| 0.00 | -24.83 dB | -16.80 dB | -14.39 dB |
| 0.30 | -42.95 dB | -29.65 dB | -14.44 dB |
| 0.50 | -39.87 dB | -33.09 dB | -14.74 dB |
| 0.80 | -34.16 dB | -23.53 dB | -16.17 dB |
| 1.00 | -24.88 dB | -16.84 dB | -14.42 dB |

The middle of the knob strips **16 dB out of everything below 1 kHz** while
leaving the top octaves where they were, and it does that across a plateau from
roughly 0.3 to 0.7 rather than at a point. Body gone, fizz intact — which is
what reads as "quiet", especially into a filter. Confirmed at 110 Hz and
440 Hz inputs and at two levels. This is what an 8-bit XOR does to a waveform
and is not a defect; it is undocumented.

The sweep is also mirror-symmetric: knob `k` and `1 - k` differ by an exact
polarity inversion (measured -30 to -46 dB against each other's negation),
because `flip` and `255 - flip` differ by XOR `0xFF`, which the stage's own
`Mangle(input) - Mangle(0)` DC removal turns into a sign flip. **This is not
wasted travel**, which is what a first reading suggested: at full wet the two
halves are audibly identical, but at partial Blend the sign is summed against
dry and the halves diverge (difference against each other 0.57-3.04 dB at
Blend 0.25 versus the 6.02 dB of a pure negation at Blend 1). The top half
earns its place everywhere the page is actually used.

### 5. What Drive does that the others do not

`Drive` is the only control that scales the shaper's INPUT (`SetGain`,
1x-5x, `app/dsp/Drive.hpp:100`). Two consequences no other knob has:

- It gates `Link` entirely. `SetCoefs` uses `link * (gain - 1)`
  (`app/dsp/Drive.hpp:119`), so at Drive 0 the gain is exactly 1 and Link is
  bit-identical across its whole range — measured -240 dB at knobs 0, 0.25,
  0.5 and 1.0. With Drive at 0.6 the same sweep moves the output by up to
  2.89 dB.
- With Shape also at 0 the polynomial is exactly `y = gain * x`, and `Bias`
  cancels by construction (`Process(in + bias) - Process(bias)` is linear) —
  measured -141 dB, the noise floor. With the shaper driven (Drive 0.6,
  Shape 0.4, the rest at their defaults) Bias changes the waveform without
  changing the level: across its travel the output RMS moves by at most
  0.19 dB, while the difference against its own neutral reaches -4.05 dB
  relative to that reference — about two thirds of the signal. An earlier
  draft recorded this as "up to 5.06 dB" without naming the settings it was
  taken at, which under this file's own difference convention would mean a
  difference LARGER than the reference. It does not reproduce; the figures
  above do, and are what the parity pin asserts.

And it is not a loudness control: from knob 0 to 1 the fundamental FALLS
21.7 dB (3.24 dB to -18.49 dB) while THD rises from 2.9% to 1190%. Shape
reaches similar THD by moving the coefficients instead; Fold and Fuzz change
what the fold stage does with what Drive hands it.

### 6. Where each control starts acting, in knob values

A control's floor should sit where it starts doing something; travel below
that is a knob position that cannot be used. Measured by sweeping each control
in hundredths from its own floor and comparing magnitude spectra -- phase
rotation excluded, because a steady tone's absolute phase is not heard -- at
Blend 0.25, Drive 0.6, Shape 0.4, 660 Hz. `-40 dB` is a one-percent spectral
difference, at the edge of an A/B; `-26 dB` is five percent, plainly audible.

| control | first -40 dB | first -26 dB | travel below it |
| --- | --- | --- | --- |
| Drive, Shape, SRR 1, SRR 2, Link, Fold, Tone | 0.01 | 0.01-0.03 | 1% |
| XOR | 0.01 | 0.04 | 1% |
| Bias | 0.01 | 0.04 | 1% |
| Fuzz | 0.02 | 0.06 | 2% |
| **Bit depth** | **0.19** | **0.44** | **19%** |
| **Anti-alias** | **0.26** | never | **26%** |
| **Phase** | never | never | **100%** |

Eleven of the fourteen start acting within a hundredth of their floor, so the
page mostly already puts its floors where they belong. The three that do not
are the two this change already fixes, plus one it had not found:

**Bit depth loses its first fifth.** `SetHash` used to round the knob to
whole bits, `hashBits = round(knob * 8)`, so the control had nine
positions, and the first two of them -- everything up to knob 0.19 -- scrambled
either no bits or one, which was inaudible. A knob whose bottom fifth was
indistinguishable from off, on a control an operator reaches for expecting
grit -- until this change remapped it onto bit counts that actually scramble
something (`app/dsp/Drive.hpp:565`).

**Phase never moves the spectrum at all here**, which is a stronger result than
this proposal's own section 2, and consistent with it: the allpass is
unity-gain, so it can only be heard through cancellation against the dry path,
and how much cancellation there is depends on the note and on the shaper's
settings. At 220 Hz with Drive 0.5 the top tenth of the knob lifts the notch by
2.4 dB; at 660 Hz with Drive 0.6 and Shape 0.4 no knob position changes the
magnitude spectrum by even one percent. Its audible range is not merely small,
it is conditional.

### 7. The page's hierarchy is its master's name and position, not its wet path

The Daisy original's Drive page was eight knobs -- `GAIN`, `SHAPE`, `SRR1`,
`SRR2`, `DIGR`, `HASH`, `FUZZ`, `FUEG` (`DAISY_MANUAL.md:312-319`) -- with
`GAIN` defined as "polynomial drive amount", the shaper's own input gain. The
reducers and the bit manglers sat after the shaper and acted at any gain, so
even there `GAIN` gated only distortion, never the page. There was no dry/wet
at all: the page was always in the path.

An earlier draft of this section argued that the page has no hierarchy because
its wet path is not transparent at rest -- the sine fold's small-signal gain is
`2*pi / divisor` with nothing compensating it, so the wet path at rest measures
+3.25 dB with 2.67% THD -- and proposed normalizing the fold and moving its
default to the clean end. **Both are withdrawn.** The premise does not survive
being checked against the other two pages it appeals to.

The rule the three wet controls actually share is narrower: **the master
returns the dry signal exactly at its floor.** Reverb's mix was a plain
crossfade against the dry input, since replaced by the same equal-power law
(`app/dsp/Reverb.hpp:735-736`); Delay's was the same expression; and the Drive
page's Blend was too, now the same equal-power law (`app/dsp/Drive.hpp:969`) --
measured bit-identical at -240 dB at Blend 0, in this proposal's own section 1.
By the standard the family keeps, the Drive page complies today.

No page keeps the stricter property. Measured on Reverb as it ships, every
control at its registered default or floor and the master turned fully up:

| Reverb wet/dry | output level | difference from the input |
| --- | --- | --- |
| 0.60 (today's cap) | -8.46 dB | -4.07 dB |
| 1.00 | -22.05 dB | +0.37 dB |

A reverb whose wet path equalled its input would not be a reverb. Delay is the
same: its wet path is echoes, and it reads as transparent at rest only because
`Send` defaults closed and the path is empty. Under this change Reverb gains a
Send and passes for that same trivial reason. Drive has no Send -- it is always
fed -- so holding it to a rule the other two pass only by being unfed would
have meant one page paying for a property the family does not have.

That leaves a real finding with a smaller fix. No knob can gate the whole page,
because the bit manglers act at any level: crushing a quiet signal still
crushes it. So the master is the wet/dry control, its silence at rest is
correct rather than broken, and the gain knob is what a player reaches for
first because it is the one that makes distortion as opposed to crushing. That
was the original's arrangement, and this page has it -- hidden by the master's
position at slot 7 and by both controls' names. Fixing the names and the
position is the whole of the fix.

### 8. The Delay bank, and the ceiling the three wet controls do not share

Brought into scope on the operator's instruction (2026-09-10), under the rule
that a family which must stay in sync gets handled together rather than one
member at a time.

Delay's wet control is `Wet mix` at slot 6, short name `WetMx`. Behaviourally
it is not behind Reverb, it is ahead of it. Its wet path is fed only through
`Send`, which defaults to zero, so a plain crossfade would mute the instrument
the moment an operator raised the mix on an unfed line. `WetAuthority()`
(`app/dsp/Delay.hpp:1025`) scales the mix by the level the wet path actually
holds, so raising Wet mix against a silent line does nothing instead of
ducking the source. Delay is already transparent at rest for the same reason.

What Delay does NOT match is the naming and the placement: `Wet mix` at slot 6
against Reverb's `Wet/dry` at slot 0. And its `Send` is the page's own feed --
the structural twin of the Drive page's Gain, deciding how much signal the
stage gets to work on -- sitting at slot 1 where it reads as one row among
nine.

Tracing that turned up something the Drive page's own wet control does not
share with either of them. `dsp::kMinDryLevel = 0.30f` (`app/dsp/Limiter.hpp`,
formerly `kMaxWetMix` in `app/FroggersAppCore.hpp`) floors Reverb's and
Delay's dry signal at 30% of its own gain at any knob position -- applied to
the equal-power crossfade's own angle (`thetaMax = acos(kMinDryLevel)`), not
as a linear scale on the knob -- and its comment records why: the operator's
ruling of 2026-07-29, tightened 2026-08-26, that "turning a wet control up
adds processed signal instead of trading away the source". The Drive page's
Blend obeys no such floor: at 1.0 the dry signal is gone entirely, which is
measurable in this proposal's own section 1, where the level climbs while the
fundamental is traded away. Three members of one family, two of which share a
floor and a reason.

This change does NOT extend the cap to the Drive page, and says so
deliberately rather than by omission. A reverb or a delay at full wet has
thrown away the instrument; a distortion at full wet is a fuzz pedal, which is
a sound an operator asks for by name. The family is deliberately different at
that one point, which is worth recording so the next reader does not "fix" it
into agreement. The OPERATOR item below puts the question rather than assuming
the answer.

### 9. The Reverb bank: what a Send would cost, and what is actually weak there

Brought into scope on the operator's instruction (2026-09-10): both Delay and
Reverb should open with Send and Wet/Dry. Reverb has no Send, and
`kFroggersParamsPerBank = 14` (`app/FroggersParameters.hpp:76`) is structural,
so one slot has to come from somewhere.

Every Reverb control was swept against its own default and measured two ways --
the tail's spectrum, and the stereo image, because a width control lives
entirely in the difference between the channels and a left-channel measurement
cannot see it. Both corrections mattered: on a first pass Mod rate read as
bit-identical at every position and Stereo width read as nearly inert. Mod rate
was measured with Mod depth at zero, where it has no depth to set the rate of;
Stereo width was measured on one channel. Neither is weak.

Ranked by the widest change each produces (less negative is a bigger change):

| control | tail spectrum | stereo image |
| --- | --- | --- |
| Tank drive | +79.3 dB | +64.5 dB |
| Hold | +62.4 dB | +46.8 dB |
| Grit | +60.8 dB | +62.4 dB |
| Decay | +50.2 dB | +34.8 dB |
| Room size | +3.4 dB | +4.6 dB |
| Damping | -3.0 dB | +6.9 dB |
| Tuned | -3.4 dB | -5.0 dB |
| Mod rate | -4.7 dB | -3.2 dB |
| Tilt | -9.1 dB | -7.8 dB |
| Mod depth | -14.3 dB | -7.1 dB |
| Diffusion | -17.5 dB | -19.4 dB |
| **Pre-delay** | **-20.5 dB** | **-15.4 dB** |
| Stereo width | -28.1 dB | **0.0 dB** (the image is its whole job) |

**Pre-delay is the weakest, and it is weak because it is broken rather than
because it is unwanted.** `PreDelayNormFromKnob` used to be
`ExpMapCompute(1 / sampleRate, 100 / sampleRate, knob)` -- a range of one to
a hundred SAMPLES, which at 48 kHz is 0.02 ms to 2.08 ms; this change replaced
it with the millisecond-based mapping below (`app/dsp/Reverb.hpp:435`).
Measured against a click, the tail's arrival moves
from 9.81 ms to 11.88 ms across the entire knob, and two adjacent positions
(0.20 and 0.25) are bit-identical. A pre-delay earns its name at tens of
milliseconds, where it separates the source from its own tail; two milliseconds
is inside the window where a listener fuses the two. The range reads as
samples standing in for milliseconds, off by roughly the sample rate over a
thousand.

So this change does NOT propose cutting Pre-delay. Deleting a control because a
one-line range error made it inaudible would destroy something the instrument
wants, and the fix is the same size as the deletion.

The slot it proposes to free instead is the modulation PAIR. Mod depth and Mod
rate are two slots serving one capability, and Mod rate is inert at every
position whenever Mod depth sits at its own default of zero -- the same
coupling shape as the Drive page's Link under Gain. Individually they are
mid-strength (-14.3 dB and -4.7 dB). Collapsed into a single Mod control -- a
depth whose rate follows it, or a depth with the rate fixed where it is most
useful -- the bank keeps the capability, loses no distinct sound it can make
today, and frees the slot Send needs. That is the same "one control per job"
reasoning the runtime library applied to its own controller row.

The operator chose this on 2026-09-10. Which law the collapsed control follows
-- a rate that tracks depth, or a fixed rate under a depth knob -- is settled
inside the change by measuring which reaches the modulation characters the two
knobs reach together today, rather than by taste.

## What each option sounds like

The dB figures above say how much alias there is. What decides whether a player
hears it is where each alias partial sits and how it stands against the
harmonics beside it, since a partial buried 50 dB under its neighbours is
masked and one standing level with them is not. Measured per note, Drive 1.0,
Shape 0.6, reporting the loudest inharmonic partial and the loudest harmonic
within a third octave of it:

| note | path | loudest alias | its level | harmonics beside it | margin |
| --- | --- | --- | --- | --- | --- |
| A4 440 | today | 11.5 kHz | -29.2 dB | -24.7 dB | 4.5 dB under |
| A4 440 | 4x clean | 23.8 kHz | -41.5 dB | -27.5 dB | 14.0 dB under |
| A5 880 | today | 20.7 kHz | -25.3 dB | -30.4 dB | **5.1 dB OVER** |
| A5 880 | 4x clean | 20.7 kHz | -43.7 dB | -31.9 dB | 11.8 dB under |
| A6 1760 | today | 11.5 kHz | -25.6 dB | -26.5 dB | **0.9 dB OVER** |
| A6 1760 | 4x clean | 23.4 kHz | -43.2 dB | -35.6 dB | 7.6 dB under |
| E7 2637 | today | 14.3 kHz | -24.7 dB | -26.8 dB | **2.1 dB OVER** |
| E7 2637 | 4x clean | 15.3 kHz | -29.1 dB | -26.7 dB | **2.4 dB under** |

An earlier draft of this table put today's A4 alias 54 dB under and the 4x
clean path 22 to 29 dB under across the whole range to E7. Re-measured on the
shipped path, the qualitative story holds and the margins do not: 4x buys 8 to
14 dB, not 22 to 29, and at E7 it has essentially run out.

**Today, around A4.** The alias sits 4.5 dB under the harmonics beside it --
audible if listened for, but masked in a mix, and no knob position changes a
bass line or a low pad.

**Today, from A5 up.** The loudest partial in the sound is not a harmonic. At
A5 a 20.7 kHz partial stands 5.1 dB ABOVE the real harmonics around it; at A6
and E7 it is still above them. Inharmonic partials at that level read as a
metallic ring on top of the note -- the ring-modulator quality, a whistle that
belongs to no key. Loudness barely moves, because it is one partial among many;
the timbre is what changes.

The giveaway is that it does not track the note: each semitone gets a
different, unrelated whistle, which is why a line played up the keyboard
changes character note to note rather than transposing. That is the sound the
Drive page has today at the top of its range, and it is a character some
players want from a bit-crusher, which is why the fix is a knob rather than a
silent correction.

**4x clean.** The same harmonics, with the inharmonic partials pushed 8 to
14 dB under the harmonics beside them from A4 to A6 -- enough to take the ring
off the note and let it transpose cleanly. **It runs out above A6.** At E7 the
worst partial is only 2.4 dB under, still close enough to hear, and the
factor-by-factor measurement in section 3 shows why: above roughly 2 kHz the
harmonics that matter have passed the 4x domain's own Nyquist, where no
decimation filter reaches them. The control cleans the range this instrument is
usually played in and does not claim the top of it.

**8x clean.** Identical to 4x up to A6 -- the two differ by under 2.5 dB on
every note to 1760 Hz, far below the harmonics in both, so no listener has
anything to hear. They separate only at the very top: at A7 8x is 6.2 dB
cleaner, and with a 5 kHz fundamental -- above any note, but not above the
partials a bright oscillator feeds this page -- 8x is 13.6 dB cleaner
(-24.9 dB against -11.4 dB, where 4x's worst partial has climbed back to
within 11 dB of the harmonics).

An earlier draft of this section also justified 8x by audio-rate modulation of
a Drive-page knob, on the reasoning that a knob swept at audio rate makes the
shaper time-varying and reaches content no static setting produces. Measured,
that is false for this knob: the Drive knob swung across its full range at
200 Hz and at 2 kHz puts LESS energy above 15 kHz (-17.3 dB and -18.2 dB) than
its own worst static position (-13.6 dB at knob 0.75), and reaches the same top
bin. The reasoning was borrowed from this file's own DriveBlendPhase comment,
which measured a 50x blowup under per-sample-random modulation -- a different
parameter, and a peak-gain measurement rather than a spectral one. It does not
transfer, and 8x rests on the top octave alone.

So the trade is: **4x buys the entire audible improvement for notes up to
roughly A6, at twice today's waveshaper cost. 8x buys the top octave, at four
times.** Neither changes a bass note. Both keep today's sound at the knob's
other end.

### 10. Why Delay keeps the modulation pair Reverb loses

Delay carries the same coupling Reverb does: its own Mod depth gates its Mod
rate, so the rate knob is inert at the depth knob's default
(`app/dsp/Delay.hpp`, where `dmod` gates the LFO increment's audible effect).
Collapsing Reverb's pair and leaving Delay's looks like the family being
treated inconsistently, and it is worth saying why it is not.

Reverb's collapse is not a tidying. It exists to free the slot `Send` needs,
because `kFroggersParamsPerBank` is 14 and Reverb had no spare. Delay already
has a Send at slot 1. Collapsing its pair would free a slot for nothing, spend
a capability to buy it, and leave the page with an empty slot where a working
control used to be. The coupling is documented on both pages instead, which is
what the operator actually loses time to.

If a later change needs a Delay slot, this is where to find one, and the
measurement that picked Reverb's law applies unchanged.

### 11. The wet/dry law, and what the level fix is modelled on

Every wet/dry control on this instrument crossfaded LINEARLY, and a linear law
holds level only when its two legs are fully correlated. None of them are.
Measured worst dip anywhere on each control's travel, before any change:

| page | worst dip |
| --- | --- |
| Drive | -4.10 dB |
| Delay | -9.28 dB |
| Reverb | -9.66 dB |

The Drive page's cause was the wet path's fundamental sitting out of phase with
dry, with the correlation SIGN changing across Gain (-0.35 at Gain 0.5, +0.02
at 0.75), so no fixed polarity and no fixed Phase setting could fix it. An
equal-power law -- `dry*cos(theta) + wet*sin(theta)` -- takes that page to
-1.15 dB, with both endpoints held exact by special-casing rather than trusting
`std::cos` (at theta = pi/2 in float, `std::cos` returns -4.371e-8, not zero).

Delay and Reverb improved less, to -6.25 and -6.81 dB, because their defect is
a second one underneath: their wet leg ARRIVES quieter than dry. Measured on
Reverb, wet leg against dry input: -2.35 dB at 440 Hz, -6.57 dB at 880 Hz,
-8.99 dB on broadband noise. Sweeping the tank's own controls locates the
control that moves it: Damping, while Room size and Diffusion move it under
half a decibel (0.03 and 0.04 dB re-measured).

An earlier draft read the rest of that mechanism backwards, and group 12 was
built on the misreading. Three corrections, each measured in `preflight.md`:

- **Damping is NOT inside the feedback path.** `Reverb::Process` reads the tank
  at `app/dsp/Reverb.hpp:592-593`, forms the feedback taps from those raw reads
  at `:604-605`, and runs the damping filter at `:669-671` on the same raw
  reads, producing an output that is never fed back. It is a static one-pole on
  the tank's output, applied once, not a loss that compounds per circulation.
  The description fits the OTHER page: `StereoDelay`'s tone filter does sit in
  the loop (`app/dsp/Delay.hpp:894-895`), and that file's own comment says so.
- **Damping's registered default is 0.0, not 0.4**
  (`app/FroggersParameters.hpp:277-289`, no third field, no Reverb overlay),
  and 0.0 is the LOUDEST end of the travel, not a point near the worst.
- **The deficit is 9.98 dB and monotonic, not 6.4 dB and humped.** The humped
  table came from measuring the stage's full mixed output at Wet/dry maximum,
  with the dry floor still summed in, and calling it the wet leg.

What the wet leg actually does across Damping, against the input that fed it:
9.98 dB of loss on broadband noise, 7.20 dB on a plucked signal, 4.74 dB on a
440 Hz sine, and 0.52 dB on a 110 Hz sine. The spread is the finding. A lowpass
removes the energy that sits above its corner, so how much level Damping costs
depends entirely on the material, and no static term set for one source is
right for another.

The compensation this section used to propose is therefore WITHDRAWN; see
`tasks.md` group 12 for the measurement that killed it, and the paragraphs
below for the rule it was breaking.

A level follower normalizing wet to dry was prototyped and REJECTED on
measurement: it recovers the steady cases but moves the makeup gain by 4 to
8 dB across a take, which is audible breathing, and it barely helps dynamic
material at all.

**Credit where this is taken from.** The arrangement here follows Valhalla
DSP's, which solves the same two problems separately and says so plainly.
ValhallaShimmer's documentation: "The Mix control uses a power-complementary
crossfade technique, to ensure constant levels throughout the various
settings." ValhallaRoom's: "The Mix slider uses a sine/cosine crossfade, such
that the signal is balanced in volume at all settings of Mix." And, decisively
for the second half, ValhallaRoom's Depth control: "The Depth control uses a
sine/cosine crossfade. In addition, a great deal of effort went into
'normalizing' the levels of the Early and Late reverb sections, such that the
output levels are balanced over virtually the whole Decay range."

Read carefully, that Depth passage normalizes the EARLY and LATE reverb
sections against EACH OTHER -- two wet sections -- so that the Depth crossfade
between them holds level. It is not normalizing wet against dry. The Mix
control gets the power-complementary law and nothing else.

ValhallaShimmer's own notes are explicit that a quieter wet path is not
automatically corrected there either. On Diffusion above 0.91: "The decay lasts
longer and longer, but the reverb itself gets quieter and quieter [...] the
Diffusion control doesn't add any energy to the signal -- it just redistributes
it. [...] This is why the Feedback control is useful for long signals, as it
adds gain to the system." The level change is explained to the player and a
control that adds gain is pointed at, rather than hidden by a makeup stage.

So the principle taken from Valhalla is narrower than "normalize the wet path",
and it is this: **when a control crossfades two signals, make the two legs
comparable in level first, then use a power-complementary law.** Valhalla
engineered the comparable-legs half for Early-vs-Late; this change owes the
same for dry-vs-wet, and the measurement above says the term that breaks
comparability here is Damping.


ValhallaFutureVerb's manual completes the picture, and it is the most recent of
the three. Its MIX control: "Controls the balance between the dry input, and
the effected ('wet') signal from the echo and reverb sections. 0% is fully dry,
50% is an equal mix between wet and dry, and 100% is wet only." No crossfade
law is stated, and there is NO dry floor -- the control reaches fully wet. Its
answer to a wet section arriving at the wrong level is a user-facing knob, one
per section: "Level: Controls the output level of the echo section in the mix
[...] The Mix control acts as a scaler on the Echo and Reverb Levels, i.e. if
Mix is at 0%, no Echo or Reverb will be heard regardless of their respective
levels." The Reverb section has its own Level knob beside it.

Across all three products the level question is answered the same way: with a
control the player can reach, never with a hidden makeup stage. This change
carries no makeup stage, and the Damping compensation an earlier draft proposed
was withdrawn for exactly this reason once it was measured: a lowpass's level
change is the lowpass working, and the honest answer is a knob beside it and a
sentence in the manual. Reverb gains such a knob in this same change -- `Send`,
at slot 1.

Where this change parts company with ValhallaFutureVerb is the dry floor.
`dsp::kMinDryLevel = 0.30f` does cap how wet Delay's and Reverb's mixes go, and
it stays, on a ruling rather than on a measurement: the operator asked for it
(2026-07-29, tightened 2026-08-26) after those pages at full wet read as quiet
and noise-like, and the constant's own comment records it. Measured now that
the crossfade law is fixed, removing the floor costs 2.56 dB on noise and
1.17 dB on a plucked signal and GAINS 0.84 dB on a sine -- so the level
argument no longer carries it either way, but the character half does: at full
wet with no floor the dry signal is gone and only the uncorrelated wet path is
left, which is the condition the operator was reporting. A different product
may reach fully wet; this one was asked not to.

Sources: valhalladsp.com/2010/11/27/valhallashimmer-the-controls/,
valhalladsp.com/2011/05/02/valhallaroom-the-high-level-sliders/,
valhalladsp.com/shimmer/ValhallaShimmerNotes.pdf (Sean Costello, 2010/2011),
and the ValhallaFutureVerb manual, version 1.0.0.


## What Changes

- `app/dsp/Drive.hpp`, `DriveBlendPhase::Process`: the Phase knob maps to the
  allpass coefficient through the allpass's own break frequency rather than
  linearly through `a`, so equal knob movements rotate the audible band by
  comparable amounts instead of piling the whole effect into the last tenth.
  The endpoints stay strictly inside the unit circle, which is what the
  existing `0.98` scaling is there for.
- `app/FroggersParameters.hpp`: Phase's default moves off the end of its own
  travel to the value that neither cancels nor doubles the fundamental at a
  partial Blend, chosen from the same measurement above.
- `app/dsp/Drive.hpp`, `Oversampler2x`: gains a clean path -- the same
  waveshaper run at a higher factor, decimated through a Butterworth cascade
  built from the app's own `dsp::BiquadDf1` (no new primitive) -- alongside the
  2x one-pole path it runs today. The anti-alias control crossfades the two,
  so the knob's top is bit-identical to what ships and its bottom is the clean
  path. The control keeps its name, because it now does what the name says.
- `app/FroggersParameters.hpp`: the control's default sits at the end that
  reproduces today's sound, so no patch and no existing preset changes until
  the knob is moved.
- `app/dsp/Drive.hpp`, `DigitalReorganizer::SetHash`: the knob maps onto the
  bit counts that do something rather than onto 0..8, so its first fifth stops
  being a dead zone. The count the default knob produces is unchanged.
- **The sine fold is NOT touched, and Fold's default does NOT move.** An
  earlier draft normalized the fold to unity small-signal gain and moved its
  neutral to the clean end, on the reasoning that "a scalar cannot change
  harmonic character [...] so this changes the page's level, never its sound's
  shape". The second half is false. `FrogBlock::Process`
  (`app/dsp/Drive.hpp:649-662`) runs the fold inside the oversampled lambda and
  then feeds `digitalReorganizer.Process`, whose `Mangle` maps the sample onto
  a FIXED absolute 8-bit grid -- `(input + 1.0f) * 128.0f`, rounded and clamped
  to 0..255 -- and XORs and scrambles bits of that code. A gain change ahead of
  it does not commute. Measured, comparing the stage's output before and after
  a gain change with the best-fit single scalar removed, so that a pure level
  change reads as a large negative number:

  | reorganizer setting | residual after gain match |
  | --- | --- |
  | both manglers at their floors | -140.5 dB (level only) |
  | XOR engaged | -6.0 dB |
  | Bit depth engaged | -4.7 dB |
  | both engaged | -10.2 dB |

  The floor row is the positive control: there the stage is a proven exact
  bypass, so the instrument can report "level only" and does. It does not, once
  a mangler is on. Normalizing the fold would have changed the character of the
  page's crush stages at every setting where they are used. Section 7 withdraws
  the argument that asked for it.
- `app/FroggersParameters.hpp`: **Drive is renamed Gain and Blend is renamed
  Wet/Dry**, the names an effect pedal uses and, for Gain, the name the Daisy
  original used. Wet/Dry moves to slot 0, so the control that decides whether
  the page is heard sits where a hand reaches first.

  This is not a new arrangement. The **Reverb bank already ships it**: slot 0,
  named "Wet/dry", short name "Wet", which used to mix as a plain crossfade
  and now, after this change, uses the same equal-power law, returning the
  dry input exactly at its floor (`app/dsp/Reverb.hpp:723-727`). The Drive page
  is being brought to the arrangement the instrument already has, not to an
  invented one. The Audio and Envelope banks generate rather than process, and
  the Filter bank is an always-in-path chain with no wet control at all.
- `app/FroggersAppCore.hpp`'s `kMaxWetMix` (a linear cap on the knob) is
  replaced by `dsp::kMinDryLevel = 0.30f` (`app/dsp/Limiter.hpp`), applied to
  the equal-power crossfade's own angle rather than to the knob, fixing the
  dip at its cause instead of only capping the mix that fed it. Its comment
  gains the reason the floor exists in the first place -- the operator tried
  both pages at 100% wet and they sound like white noise -- alongside the
  ruling it already records. The constant stays shared by Delay and Reverb and
  is NOT extended to the Drive page.
- `app/dsp/Reverb.hpp`, `PreDelayNormFromKnob`: the range becomes milliseconds
  rather than samples, so the control sweeps a pre-delay a listener can hear
  instead of 2 ms. The top of the range is set from what the pre-delay line can
  hold (`kSize`) rather than guessed.
- `app/FroggersParameters.hpp`, Reverb bank: `Wet/dry` stays at slot 0, a new
  `Send` takes slot 1, and the slot is freed per OPERATOR item 2. Reverb's mix
  gains the same authority scaling Delay already has
  (`app/dsp/Delay.hpp:1025`), because a page whose wet path is fed through a
  Send must not duck the dry signal when that Send is closed -- one definition
  shared by both pages rather than two that agree.
- `app/FroggersParameters.hpp`, Delay bank: `Wet mix` is renamed `Wet/dry`
  (short name `Wet`, matching Reverb exactly) and moves to slot 0, and `Send`
  moves to slot 1, directly under it. `Send` keeps its own identity and its own
  job -- it is the page's feed, the structural twin of the Drive page's Gain --
  and `WetAuthority()` is kept exactly as it is, because it is what makes the
  rename honest: a control named Wet/dry must not duck the instrument when the
  line it mixes in is empty. Every effect page then opens the same way: the
  master first, the page's own feed or gain second.
- `MANUAL.md`, Drive bank: Blend's first-quarter notch, XOR's mid-range
  low-end collapse, Phase's conditional range, and what the anti-alias control
  reaches once it can reject aliasing.
- `openspec/specs/froggers-sheaf-parameter-model` (delta in this change): the
  Drive page's Phase and anti-alias mappings are stated as what they must
  achieve — even audible travel, and a corner that reaches the aliasing band —
  rather than as the literals they happen to use.

## OPERATOR

Every call below is answered. Kept as a record of what was decided and on
what basis.

1. **The wet cap.** ANSWERED 2026-09-10: the cap stays on Delay and Reverb
   only, and rises from 0.6 to 0.7. The Drive page's Wet/Dry is uncapped and
   reaches fully wet, because a distortion that replaces its source is a fuzz
   and that is a sound asked for by name; a reverb or delay that replaces its
   source is gone. The constant's comment and the manual both record the reason
   the cap exists at all, in the operator's own terms: at 100% wet those two
   pages sound like white noise.

2. **How the Reverb bank frees the slot Send needs.** ANSWERED 2026-09-10:
   collapse Mod depth and Mod rate into one Mod control. Nothing is cut for it.
   Pre-delay stays and its range error is fixed inside this change -- a control
   being inaudible because of a one-line mistake is a reason to repair it here,
   not a reason to spend its slot.

3. **Which factor sits at the clean end.** ANSWERED 2026-09-10: 4x. It buys the
   whole audible improvement for notes to about A6 at twice today's waveshaper
   cost; 8x buys the top octave alone for four times. The audio-rate-modulation
   argument that had favoured 8x was measured and does not hold.

4. **Phase's default.** ANSWERED 2026-09-10: the operator holds no stored
   patches, so the default moves to the measured value rather than staying at
   the end of its own travel for reproduction's sake.

5. **The Drive page's transparency at rest.** SETTLED 2026-09-10 by the rule
   rather than by taste. Section 7's earlier argument held the Drive page to a
   property neither Reverb nor Delay has -- a wet path that is unity and clean
   at rest -- and paid for it in the crush stages' character. A family that
   must stay in sync is governed by the rule its members actually keep: the
   master returns dry exactly at its floor, which Blend already does. The fold
   normalization and Fold's default move are withdrawn; the renames and the
   move to slot 0 stand on their own.

## Impact

- Affected specs: `froggers-sheaf-parameter-model` (ADDED and MODIFIED
  requirements). The MODIFIED half is not optional: three promoted scenarios
  pin "slots 0-8 are unchanged" for the Drive, Delay and Reverb banks
  (`openspec/specs/froggers-sheaf-parameter-model/spec.md:105`, `:120`, `:137`),
  one pins Reverb's slot 9 as Mod Rate (`:138`), one requires at least 40% dry
  at full wet (`:312`), and two still call Delay's control "Wet mix" (`:326`,
  `:341`). `openspec validate --strict` passes without them, so the tool does
  not catch this.
- Affected code: `app/dsp/Drive.hpp` (the Phase map, the anti-alias path, the
  hash map, and the Wet/Dry crossfade law -- NOT the fold),
  `app/FroggersParameters.hpp` (two names, one
  reorder, two defaults), `MANUAL.md` and `QUICK_DICT.md` (the Drive bank's
  prose and its entries), and every site keyed to the renamed parameters or to
  the Drive page's slot numbers -- which `check_docs_match_parameter_table.py`
  and `check_catalog_covers_screen_actions.sh` both enforce, so the change
  reports what each check said before and after.
  `app/FroggersDspParityTests.cpp` pins the static-Phase neutrality, the
  anti-alias default, and the old linear crossfade's notch; each pin is
  re-derived, not deleted, and the change reports FOUND versus CHANGED. The
  fold's own literal-divisor pin is left alone, the fold being unchanged.
- The default patch: `ApplyDriveBankOverlay` (`app/FroggersModulation.hpp:1464`)
  used to hardcode Drive **slot 0** to 20%, with a comment naming it as the
  Drive control. Slot 0 became Wet/Dry, so this literal had to follow Gain to
  its new slot or the instrument would have shipped its default patch 20% wet
  instead of bypassed -- inside the notch section 1 measures. This site was
  not reachable by either check script; this change moved the literal to
  slot 1, where Gain now lives (`app/FroggersModulation.hpp:1466`).
- Renumbering: moving Wet/Dry to slot 0 renumbers the Drive page, and moving
  Delay's wet control and Send renumbers that page too. Stored MIDI mappings
  and patches key on slot indices, and the operator holds none (2026-09-10), so
  this is free today and would not be later. Both pages' presets in
  `app/FroggersMidiCatalog.hpp` address parameters positionally and are
  re-derived rather than hand-edited.
- Sound: the Phase remap and its new default change how the Drive page sums at
  any Blend below 1. The operator holds no stored patches (2026-09-10), so no
  saved sound is owed reproduction; the app's own default patch is re-measured
  rather than assumed unchanged. The manglers, the fold and the polynomial are
  untouched, so nothing else on the page moves unless the anti-alias knob is
  turned off its default.
- Cost: the crossfade always evaluates BOTH the one-pole grit path and the
  new 4x/8th-order-Butterworth clean path (so a smooth crossfade has no seam
  to click at), so the cost is the same at every knob position, not just at
  the clean end. There is no deadline harness in this app -- established by
  searching for the invocation across `app/` by operand (`deadline`,
  `benchmark`, `chrono`, `steady_clock`, `elapsed`); the only hits are a date
  stamp and the VST host tests' sleeps, and the 96 kHz deadline tests are
  Sheaf's. Measured here instead (real figures, replacing this section's
  placeholder ones), best-of-five warm at 48 kHz in 128-sample blocks,
  isolating `dsp::Oversampler2x::Process` (the shaper's own oversampling and
  anti-alias stage) from the rest of `FrogBlock`: today's one-pole-only path
  spends 20.9 ns/sample, 0.10% of the 2.67 ms block budget (matches this
  section's earlier placeholder almost exactly); with this change in place
  it spends 56.6 ns/sample, 0.27% -- an added 35.8 ns/sample from running
  both paths every sample. Per §7 a gate defending against an overrun traced
  to be impossible is a branch to remove, so the change records the real
  path's measurement and carries no cost gate.
- Nothing is taken away: the knob's default end reproduces today's path
  bit-for-bit, and no note below roughly 500 Hz is affected either way, because
  nothing folds there.
- Not changed: the XOR mapping and the polynomial itself. The XOR mapping was
  suspected and cleared by measurement above; the polynomial is the ported
  original.
- Changed: `dsp::DriveBlendPhase::Process`'s Wet/Dry crossfade law
  (`app/dsp/Drive.hpp`), from `dry*(1-blend) + phased*blend` to an
  equal-power `dry*cos(blend*pi/2) + phased*sin(blend*pi/2)`. Section 1's own
  measurement found the notch was not inherent to the waveshaper, as first
  assumed, but a property of the linear law's fixed weights against a wet
  path whose correlation with dry changes sign across the Drive knob; the
  equal-power law holds level regardless of that correlation and cuts the
  worst measured dip (110-880 Hz, Gain 0.25-1.00, Phase at its 0.86 default)
  from -4.10 dB to -1.09 dB. Both crossfade endpoints stay bit-exact (see
  Section 1). `app/FroggersDspParityTests.cpp`'s own pin on the old law's
  notch is re-derived, not deleted, to assert the new bound instead.
- One definition of the crossfade law: `dsp::EqualPowerWetDry`
  (`app/dsp/Limiter.hpp`), beside `kMinDryLevel` and `WetAuthorityFollower`,
  which all three DSP files already include. Group 11 first landed the law as
  three per-page copies, and the postflight §5 pass against the diff -- not
  against this proposal -- is what found them: Delay's and Reverb's were
  byte-identical, and each cited the other page's comment as though that made
  them shared. `minDryLevel` is the only argument that separates the pages, 0
  for Drive and `kMinDryLevel` for the other two. The Drive page keeps its own
  two exact endpoint branches, because an uncapped travel reaches pi/2, where
  `std::cos` returns -4.37e-8 rather than 0; that is a caller's bit-exactness
  concern rather than part of the law.
- Delivery: one commit pushed to `main`, this repository's convention.
