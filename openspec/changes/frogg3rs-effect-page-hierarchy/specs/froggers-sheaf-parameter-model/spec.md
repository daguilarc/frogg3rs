# Delta — `froggers-sheaf-parameter-model`

## ADDED Requirements

### Requirement: A Drive page control's travel spends itself where the control is heard

WHEN a Drive page control is mapped from its knob to the coefficient it drives, THE application SHALL choose the mapping so that comparable knob movements produce comparable audible changes across the control's travel, measured against the band the instrument actually produces rather than against the coefficient's own arithmetic range, and SHALL place the control's floor where it begins to act rather than below it: no more than a hundredth of a control's travel may be indistinguishable from its floor, judged on magnitude spectra so that inaudible phase rotation is not counted as an effect. A control whose stated job is inaudible across most of its travel SHALL be re-mapped, re-defaulted, or re-named to say what it does. Two controls on this page are held to this: the Blend stage's Phase allpass, whose audible effect exists only through how the rotated wet signal sums with the dry path, and the anti-alias control, which SHALL reject aliasing measurably across its travel rather than carry a name its filter cannot deliver.

#### Scenario: Phase moves the blend across its whole travel

- **WHEN** a 220 Hz tone is driven with Drive at 0.5 and Blend at 0.25, and Phase is swept from 0 to 1
- **THEN** the output level changes progressively across the sweep rather than only in its last tenth
- Check: `app/FroggersDspParityTests.cpp`, `drive_phase_sweep_moves_output_meaningfully_across_each_quarter_of_travel`, the Phase-travel case.

#### Scenario: The anti-alias control rejects aliasing

- **WHEN** a tone whose harmonics fold is driven hard, and the anti-alias control is swept from end to end
- **THEN** the loudest inharmonic partial, measured against the fundamental, falls monotonically by at least 15 dB across the sweep
- **AND** the in-band level moves by no more than 3 dB between the ends, so the sweep trades grit for cleanliness rather than trading tone for tone
- **AND** the test tone SHALL NOT divide the sample rate, and SHALL sit inside the range the chosen oversampling factor serves: at a tone dividing the sample rate every fold-image lands on a bin a genuine harmonic occupies, so no measurement can separate them, and above roughly 2 kHz the harmonics that matter have passed the 4x domain's own Nyquist where no decimation filter reaches them
- Check: `app/FroggersDspParityTests.cpp`, `drive_anti_alias_crossfade_falls_monotonically_and_the_old_one_pole_barely_moved_it`, the alias-sweep case, at 1487 Hz, read through a Hann-windowed bin measurement — measured 23.3 dB of travel with the in-band level moving 2.46 dB.

#### Scenario: A quantized control spends no travel on steps that do nothing

- **WHEN** the Bit depth control is swept from its floor
- **THEN** the first hundredth of its travel already scrambles a number of bits an operator can hear, rather than the first fifth resting on counts that are inaudible
- Check: `app/FroggersDspParityTests.cpp`, `drive_bit_depth_first_audible_knob_value_falls_from_0_19_to_a_hundredth`, the knob-threshold case.

#### Scenario: The knob's default reproduces what shipped before it

- **WHEN** the anti-alias control sits at its default
- **THEN** the path is bit-identical to the 2x one-pole oversampler that shipped before this control existed
- Check: `app/FroggersDspParityTests.cpp`, `frog_block_default_knob_values_reproduce_original_output_exactly`, which drives the whole chain at its registered defaults and compares against the pre-existing path. The oversampler parity pin alone does not prove this: it pins the 2x unit's own behaviour, not that the knob's default selects it.

#### Scenario: A control that is inert at a default is documented as such

- **WHEN** the Drive page's Link is read at Drive 0, or its Waveshaper offset at Drive 0 and Shape 0
- **THEN** each is bit-identical across its own range, because the polynomial it modifies is linear there
- **AND** the manual says which control has to be raised for it to act
- Check: `app/FroggersDspParityTests.cpp`, the inert-at-default cases: `drive_link_is_inert_at_zero_drive_and_moves_the_output_once_driven` and `drive_bias_cancels_at_zero_drive_and_moves_the_output_once_driven`.

### Requirement: An insert effect page's master returns the dry signal at its floor

WHEN an insert effect page carries a wet/dry master, THAT control SHALL crossfade the page's output against its input using a POWER-COMPLEMENTARY law — the two legs' gains SHALL be `cos` and `sin` of a common angle, so that their squares sum to one — and SHALL return the input exactly, sample for sample, at its floor, and it SHALL be named as an effect pedal names it and placed first among that page's parameters. A linear crossfade SHALL NOT be used: it holds level only where its two legs are fully correlated, and no page's wet leg is. Both endpoints SHALL be exact by construction rather than by trusting the trigonometric functions, which do not land on zero at a right angle in single precision. Where a page's wet path is fed through a Send, the master SHALL NOT duck the dry signal in proportion to a wet path that Send has left empty, and that behaviour SHALL be one shared mechanism rather than one implementation per page.

This requirement does NOT ask a page's wet path to be transparent at rest. No page's wet path is: a reverb's wet path is a tank, a delay's is echoes, and a distortion's is a waveshaper. Measured on the Reverb bank with every control at its registered default and the master turned fully up, the output is 8.46 dB below its input and differs from it by -4.07 dB relative to that input. Reverb and Delay read as transparent at rest because their Sends default closed and their wet paths are empty, not because those paths are unity. The property the three pages genuinely share is the one stated above, and each of them meets it by construction of the crossfade.

A page's gain stage SHALL be named Gain and SHALL govern the stage it drives rather than the page as a whole, because a page's bit and rate manglers act at any level and are not gated by gain. Every insert effect page whose wet path is a send-and-return SHALL open with the same two controls in the same order — the master wet/dry first, that page's Send second. A page's wet control SHALL leave a floor of dry signal in place where fully replacing the source destroys what the page is processing, as it does for a reverb and a delay, and SHALL reach fully wet where replacing the source is a sound the page exists to make, as it is for a distortion; wherever that floor applies, both the code and the manual SHALL record why it is there.

#### Scenario: The master returns the dry signal exactly at its floor

- **WHEN** an insert effect page's wet/dry control sits at its floor
- **THEN** that page's output is its input, sample for sample
- Check: `app/FroggersDspParityTests.cpp`'s `drive_blend_phase_authored_zero_blend_is_exact_passthrough`, the dry-at-floor pin for the Drive page, which passes today at -240 dB; `app/dsp/Reverb.hpp`'s mix and `app/dsp/Delay.hpp`'s are the same crossfade expression.

#### Scenario: The master does not lose level partway through its travel

- **WHEN** an insert effect page's wet/dry master is swept from its floor to its top
- **THEN** the page's output level does not dip below its dry level by more than a small margin anywhere on that travel, rather than notching partway and recovering
- **AND** this holds across notes and across the page's own gain settings, not at one tested frequency
- Check: `app/FroggersDspParityTests.cpp`, `drive_blend_travel_holds_level_within_1_2_db_across_gain_and_frequency`, the Drive page's blend-travel case, which measures a worst dip of -1.15 dB where the linear law it replaced measured -4.10 dB.

#### Scenario: A tone control's level change is documented rather than compensated

- **WHEN** a control that shapes a path's spectrum — a reverb's Damping, a delay's Feedback tone — is swept from end to end
- **THEN** the level it removes is whatever its filter removes, and the manual states both what the control darkens and that it quiets the path it shapes
- **AND** no makeup gain is applied to hide that change, because how much level a lowpass removes depends on where the signal's energy sits: Reverb's Damping costs 9.98 dB across its travel on broadband noise and 0.52 dB on a 110 Hz tone, so a static term set for either one is wrong for the other
- Check: `app/FroggersDspParityTests.cpp`'s `reverb_damping_darkens_and_quiets_the_tank_while_room_size_does_neither`, which pins both the monotone broadband attenuation and the monotone spectral tilt, and carries Room size over the same travel as its positive control.

#### Scenario: The gain stage makes distortion and the manglers do not need it

- **WHEN** Gain sits at its floor and the XOR, Bit depth or either rate reducer is raised
- **THEN** that stage is audible, because it acts on the signal at any gain
- **AND** with every stage at its floor, raising Gain alone is what introduces harmonic distortion
- Check: `app/FroggersDspParityTests.cpp`'s `drive_gain_makes_distortion_and_the_manglers_act_at_any_gain`, which measures XOR, Bit depth and both rate reducers against a bit-identical all-floor reference at Gain 0, and Gain's own THD rise with every mangler at its floor.

#### Scenario: A fed page's master does nothing until the page is fed

- **WHEN** the Delay bank's Send sits at zero and its wet/dry control is raised to its top
- **THEN** the output is the dry signal, neither ducked nor replaced
- **AND** the Reverb bank behaves identically, through the same shared mechanism rather than a second copy of it
- Check: `app/dsp/Delay.hpp`'s `WetAuthority()` provides this today; the case pins it against the renamed control and against Reverb's new Send.

#### Scenario: The master's position and name say what it does

- **WHEN** the Drive, Delay and Reverb banks are enumerated
- **THEN** each opens with its wet/dry master at slot 0, and each page that has a Send carries it at slot 1
- **AND** the Drive page's master is named Wet/Dry and its gain stage is named Gain
- Check: `app/check_docs_match_parameter_table.py`, which cross-checks the table against the manual and the quick dictionary.

#### Scenario: The default patch does not arrive wet

- **WHEN** the instrument is loaded with its own default patch
- **THEN** the Drive page's Wet/Dry sits at its registered default rather than at a value a slot-indexed overlay left behind
- Check: `app/FroggersModulationTests.cpp`'s `default_patch_gain_is_20_percent` and `default_patch_wet_dry_reads_its_own_registered_default`; the overlay is `ApplyDriveBankOverlay` (`app/FroggersModulation.hpp`), which addresses its target by slot number.

#### Scenario: A control's range reaches the effect it is named for

- **WHEN** a reverb's pre-delay control is swept from end to end
- **THEN** the tail's arrival moves by tens of milliseconds, far enough for a listener to hear the source and its tail as separate events
- **AND** no two adjacent knob positions produce identical output
- Check: `app/FroggersDspParityTests.cpp`'s `reverb_predelay_sweeps_tens_of_milliseconds_with_no_dead_adjacent_steps` and `reverb_predelay_floor_and_ceiling_match_derived_millisecond_range`. Written first against the 2.08 ms total sweep the samples-for-milliseconds range produced, where they failed.

## MODIFIED Requirements

### Requirement: One sixteen-slot bank per Froggers page
Each existing Froggers page SHALL become exactly one bank of sixteen parameter slots. Pages SHALL NOT be
merged. A bank's own parameters SHALL occupy the leading slots; remaining parameter slots MAY be empty,
or MAY hold additional named parameters where a bank's slate has been explicitly decided and expanded.

#### Scenario: Page identity is preserved
- **WHEN** the banks are enumerated
- **THEN** there is one bank per original Froggers page
- **THEN** each bank contains that page's parameters and no other page's

#### Scenario: Sparse banks are valid
- **WHEN** a bank has fewer parameters than available slots
- **THEN** the unused slots render as empty
- **THEN** the occupied slots keep their positions rather than being renumbered

#### Scenario: The Audio bank holds fourteen parameters, complete
- **WHEN** the Audio bank is enumerated
- **THEN** it holds fourteen named parameters at slot indices 0 through 13, not nine
- **THEN** slots 0-8 are unchanged from the Audio bank's existing nine parameters, without every one of
  the nine necessarily originating as a page row
- **THEN** three of the original nine are the VCO Shape controls rather than page rows
- **THEN** slots 9 through 11 are Ring Mod (short names `RM1`, `RM2`, `RM3`), one per VCO — each VCO's own
  ring modulator carrying an internal carrier generated inside that VCO's own ring-mod stage, never a
  signal from another VCO
- **THEN** each Ring Mod knob's resolved value sets its own VCO's internal carrier frequency across an
  audio-rate range, mapped the same exponential way the Audio bank's existing pitch knobs already map
  pitch, and each VCO's own signal is multiplied by its own carrier's output
- **THEN** the bottom of each Ring Mod knob's travel is a true zero position, gating that VCO's ring-mod
  amount to exactly zero and ramping smoothly out of it, so the knob is continuous and the VCO can be heard
  unmodulated, per the shared ramp the `froggers-vco-topology` delta requires
- **THEN** each Ring Mod knob defaults to a position at or below that zero floor, so the instrument at its
  defaults sounds exactly as it did before Ring Mod existed
- **THEN** slot 12 is PM Rate (short name `PMrt`), the phase-modulation LFO's own rate, shared across all
  three VCOs and independent of the phase-modulation depth the existing Phase-mod knobs already control
- **THEN** slot 13 is VCO Balance (short name `VBal`), a single tilt sweeping mix emphasis across VCO1
  through VCO3, replacing the fixed equal-thirds average with a constant-total-gain crossfade, subject to
  the floor and cap the "VCO Balance keeps every VCO in the mix" scenario below requires

#### Scenario: VCO Balance keeps every VCO in the mix
- **WHEN** the VCO Balance crossfade weights are computed for any knob position
- **THEN** the three weights SHALL sum to exactly 1
- **THEN** each weight SHALL stay within the range 0.10 to 0.80 inclusive
- **THEN** no knob position reduces any VCO's weight to 0, and no knob position raises any VCO's weight to
  1.0
- **THEN** the three-VCO, 10%-floor arithmetic caps any single VCO's weight at 1 minus two floors, i.e.
  0.80, so the floor and the cap are the same constraint expressed from opposite ends

#### Scenario: The Envelope bank holds fourteen parameters in interleaved ADSR order
- **WHEN** the Envelope bank is enumerated
- **THEN** it holds fourteen named parameters at slot indices 0 through 13, not nine
- **THEN** slots 0-3 are Attack VCO1, Decay VCO1, Sustain VCO1, Release VCO1 (short names A1, D1, S1, R1)
- **THEN** slots 4-7 are Attack VCO2, Decay VCO2, Sustain VCO2, Release VCO2 (short names A2, D2, S2, R2)
- **THEN** slots 8-11 are Attack VCO3, Decay VCO3, Sustain VCO3, Release VCO3 (short names A3, D3, S3, R3)
- **THEN** slot 12 is Curve, applying to all three voices' Attack/Decay/Release ramp shape
- **THEN** slot 13 is Grace, a minimum Hold duration so a short gate cannot clip a note before its
  envelope completes Attack and Decay
- **THEN** each voice's Attack ramps to a peak, Decay then falls from that peak to the voice's own
  Sustain target level, and Hold sustains at that level exactly as it does today

#### Scenario: The Filter bank holds fourteen parameters, grouped by stage

- **WHEN** the Filter bank is enumerated
- **THEN** it holds fourteen named parameters at slot indices 0 through 13, grouped by the stage they
  belong to, with saved patches unaffected because persistence addresses parameters by name
- **THEN** slots 0-2 are the Peak stage: Peak Freq (`PkFreq`), Peak Gain (`PkGain`), Peak Q (`PkQ`)
- **THEN** slots 3-7 are the Comb stage: Comb Offset (`CmbOff`), Comb Delay (`CmbDly`), Comb Feedback
  (`CmbFb`), Comb LP (`CmbLP`), and Comb Drive (`CDrv`) — a saturation-depth control whose drive factor
  multiplies the in-loop saturator's input while the same factor divides its output, so the loop's
  small-signal gain is unity at every setting, the fed-back term never exceeds the delayed signal it is
  computed from, and the loop's decay argument is preserved at every drive
- **THEN** slots 8-11 are the Scoop stage: Scoop Mix (`ScMix`, the notch's wet/dry blend into the chain's
  shared input, ahead of both the Comb and Peak stages), Scoop Freq (`ScFq`, the notch's own center
  frequency, independent of the Peak stage's), Scoop Width (`ScWd`, the notch's own width, independent
  of the Peak stage's), Scoop Depth (`ScDp`, the notch's own dip depth, independent of how much of it
  reaches the chain's input)
- **THEN** slots 12-13 are the routing pair: Comb/Peak (`Cmb/Pk`) and Topology (`Topo`) — Topology a
  continuous morph of the Comb and Peak stages from parallel at one end to series at the other, with no
  switched positions anywhere in its travel; at its minimum the Peak stage reads the chain's scooped
  input, the same input the Comb stage reads, and at its maximum it reads the Comb stage's output
  instead, with the Comb/Peak blend and every output trim and limiter staying in force at every
  position of both controls
- **THEN** the bank's Crispy and the global Crunchy keep slots 14 and 15

#### Scenario: The Drive bank holds fourteen parameters, complete
- **WHEN** the Drive bank is enumerated
- **THEN** it holds fourteen named parameters at slot indices 0 through 13, not nine
- **THEN** slot 0 is Wet/Dry (short name `Wet`), the page's master, and slot 1 is Gain
  (short name `Gain`), the polynomial waveshaper's own input gain
- **THEN** slots 2-8 carry the remaining seven of the bank's original nine parameters, in their
  established order
- **THEN** slot 9 is Anti-Alias Brightness (short name `ABrt`), the oversampling anti-alias filter's own
  cutoff
- **THEN** slot 10 is Link (short name `Link`), the coupling weight between the Drive knob's resolved
  gain and the Shape stage's own asymmetric coefficients, independent of Drive's and Shape's own values
- **THEN** slot 11 is Fold (short name `Fold`), the pre-fold scale ahead of the sine-fold stage,
  independent of the Drive knob's own gain
- **THEN** slot 12 is Tone (short name `Tone`), a post-chain one-pole lowpass applied after every other
  Drive stage
- **THEN** slot 13 is Bias (short name `Bias`), a DC offset applied before the polynomial waveshaper and
  exactly cancelled afterward so silence-in still produces silence-out

#### Scenario: The Delay bank holds fourteen parameters, complete
- **WHEN** the Delay bank is enumerated
- **THEN** it holds fourteen named parameters at slot indices 0 through 13, not nine
- **THEN** slot 0 is Wet/dry (short name `Wet`), named exactly as the Reverb bank names its own,
  and slot 1 is Send, the page's feed into its wet path
- **THEN** slots 2-8 carry the remaining seven of the bank's original nine parameters, in their
  established order
- **THEN** slot 9 is Feedback Drive (short name `FbDr`), a pre-gain applied to the input of the feedback
  loop's own in-loop saturator, never to that saturator's output
- **THEN** slot 10 is Feedback Tone (short name `FbTn`), a one-pole lowpass damping the feedback tap
  ahead of the same in-loop saturator
- **THEN** slot 11 is Mod Rate (short name `MdRt`), the delay's own modulation LFO rate
- **THEN** slot 12 is Width Balance (short name `WBal`), the ratio between the Width knob's time-offset
  spread and its cross-feed blend, independent of the Width knob's own value
- **THEN** the cross-feed weight this balance produces stays within 0 to 1 inclusive at every knob position,
  so the left/right feedback pair stays a convex combination of the two delay-line reads
- **THEN** the time-offset spread this balance produces never lengthens a read tap beyond the delay
  buffer's own capacity
- **THEN** slot 13 is Crush (short name `Crsh`), a bitcrush stage applied to the feedback tap's repeats

#### Scenario: The Reverb bank holds fourteen parameters, complete
- **WHEN** the Reverb bank is enumerated
- **THEN** it holds fourteen named parameters at slot indices 0 through 13, not nine
- **THEN** slot 0 is Wet/dry (short name `Wet`) and slot 1 is Send, the page's feed into its tank
- **THEN** slots 2-8 carry the bank's remaining original parameters, with Mod depth and Mod rate
  collapsed into one Mod control so that Send's slot is found without cutting a capability
- **THEN** slot 9 is Hold (short name `Hold`), displaced there by Send taking slot 1 and the modulation pair collapsing to one control
- **THEN** slot 10 is Tank Drive (short name `TkDv`), a pre-gain applied to the input of the tank feedback
  path's own in-loop saturator, never to that saturator's output
- **THEN** slot 11 is Grit (short name `Grit`), the tank feedback path routed through a bit-scramble
  stage ahead of that same in-loop saturator
- **THEN** slot 12 is Tilt (short name `Tilt`), a bipolar post-tank tone shave applied before the
  existing wet limiter
- **THEN** slot 13 is Tuned (short name `Tund`), the tank's own delay-line lengths driven directly by
  this parameter's resolved value, with no pitch tracker

### Requirement: A wet/dry control cannot remove the instrument

A control that crossfades a dry signal against a processed one SHALL NOT be able
to remove the dry signal entirely. The ceiling SHALL be applied to the mapped
mix value rather than to the knob's range, so the control keeps sweeping its
whole travel and only the value its top end maps to is bounded.

This holds for every such crossfade, not for whichever one was most recently
reported. At the Reverb bank's maximum the dry signal SHALL still make up at
least 30% of that stage's output. The Delay bank's Wet/dry is the same
expression and SHALL carry the same kind of ceiling.

A wet/dry control SHALL NOT attenuate the dry signal in exchange for a processed
signal that cannot exist. Its authority to remove dry signal SHALL scale with
how much signal reaches the processed path: where nothing feeds that path the
control SHALL have no effect, and it SHALL earn its full travel as the path
comes to hold signal. Authority SHALL follow the processed path's measured
level rather than the amount being fed into it, so that a path made loud by
feedback rather than by its feed still grants the control its travel.

Where the feed is switched fully off, the measurement's TARGET SHALL drop at
once rather than tracking the tail the path still holds. The measurement itself
SHALL settle at its ordinary release rate, so the audible result is a fade of
about 100ms rather than a cut. The discontinuity belongs in the target, not in
the output.

#### Scenario: Full wet still passes dry signal
- **WHEN** the Reverb Wet/dry control is at its maximum
- **THEN** the dry signal's contribution to this stage's output is at least 30%

#### Scenario: The control keeps its full travel
- **WHEN** the Reverb Wet/dry control is swept from minimum to maximum
- **THEN** the resulting mix rises across the whole sweep, reaching its ceiling
  only at the top

#### Scenario: The wettest setting is still audible
- **WHEN** a wet/dry control is at its maximum and its processed path is fed
- **THEN** the output still carries dry signal

#### Scenario: An unfed processed path makes the control inert
- **WHEN** the Delay bank's Send is at zero and Wet/dry is swept to maximum
- **THEN** the output is the dry signal and does not fall silent

#### Scenario: A loud echo earns the control its travel
- **WHEN** the Delay bank's Send is low, Feedback is high, and the echo is loud
- **AND** Wet/dry is at maximum
- **THEN** the control removes dry signal in proportion to that echo, not to Send

#### Scenario: Switching the feed off fades rather than tracking the tail
- **WHEN** Send is turned to zero while echoes are still sounding
- **THEN** the control's authority falls at its release rate rather than
  following the echoes' own decay
- **AND** the output settles to the dry signal

#### Scenario: The patch the instrument ships with stays audible
- **WHEN** the default patch is played with Wet/dry at maximum
- **THEN** the instrument is audible
