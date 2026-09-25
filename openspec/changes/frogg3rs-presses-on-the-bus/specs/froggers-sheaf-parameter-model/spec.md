# Delta — `froggers-sheaf-parameter-model`

The Delay bank's Width Balance scenario states a ratio between the Width
knob's time-offset spread and its cross-feed blend that no code computes:
the cross-feed weight is fixed at zero and does not read Width Balance at
all, so there is no ratio for that control to hold.

## MODIFIED Requirements

### Requirement: One sixteen-slot bank per Froggers page

Each existing Froggers page SHALL become exactly one bank of sixteen parameter slots. Pages SHALL NOT be merged. A bank's own parameters SHALL occupy the leading slots; remaining parameter slots MAY be empty, or MAY hold additional named parameters where a bank's slate has been explicitly decided and expanded.

The Drive and Reverb scenarios carry edits beyond the slots this change moves, and they are declared here rather than left to a diff: each bank's count reads "fourteen parameters, complete" rather than leaving the earlier "not nine" phrasing implicit, Reverb's slot 8 and 9 follow the post-archive layout in which Mod is collapsed and Hold carries its own short name, Reverb's renamed slot 7 takes the short name `Dens` on the same rule its neighbours follow -- the word truncated, as `Diffusion` gave `Diff` and `Damping` gives `Damp` -- and Drive's slot 9 description is corrected. That last one is a finding, not a tidy-up: the promoted text describes slot 9 in terms the shipped code does not match, and a specification records what the system does.

Every promoted clause this requirement does not carry forward word for word is named below. The list is checked mechanically: a clause dropped without appearing here fails the build, and an entry here that matches no dropped clause fails it too, so neither the omission nor the stale declaration can sit reading as deliberate.

<!-- RESTATES-EXCEPT
the ratio between the Width knob's time-offset spread and its cross-feed blend
  keeps: slot 12 is Width Balance (short name `WBal`)
not yet delivered for the slot-12 ratio clause
  keeps: DELIVERED for the capacity clause, that the time-offset spread this balance produces never lengthens a read tap beyond the delay buffer's own capacity
-->

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
- **THEN** it holds fourteen named parameters at slot indices 0 through 13
- **THEN** slot 0 is Wet/Dry (short name `Wet`), the page's master, and slot 1 is Gain (short name `Gain`), the polynomial waveshaper's own input gain
- **THEN** slots 2-8 carry the remaining seven of the bank's original nine parameters, in their established order
- **THEN** slot 9 is Anti-Alias Brightness (short name `ABrt`), crossfading a clean oversampled shaper path against the grittier path that shipped before it
- **THEN** slot 10 is Feedback (short name `Fb`), the amount of the folder's own output returned to the folder's own input, one oversampled sample later — not to the whole shaping stage, because a loop around the stage oscillates from knob 0.15 upward once Gain leaves its default while the folder leg alone holds a flat, Gain-independent onset. It replaces Link, whose coupling of the Gain knob into the Shape stage's coefficients was measured bit-identical across its whole range at Gain 0 (-240 dB) and otherwise moved the same spectral tilt Fold and Tone already move. Research found no precedent for a fixed, single-purpose control whose only job is coupling one stage's resolved value into another stage's coefficients; the nearest idiom is a macro assignment, which is conventionally user-assignable. `SetLink`, the `link` field and the tests pinning them are deleted with it. The coupling term itself is NOT deleted: `PolynomialDrive::SetCoefs` keeps it at the weight Link's own default carried, as a named constant rather than a knob, so removing the control does not change the voice at that default. The operator holds no stored patches (2026-09-11), so no saved sound keys on the replaced slot
- **THEN** Feedback is a page parameter like any other and carries modulation depths against every registered source, requiring no source of its own — the bank's fixed sixteen physical positions cap the slate at fifteen sources, which it already holds
- **THEN** slot 11 is Fold (short name `Fold`), the drive INTO a fixed sine-folder, so that fold depth rises monotonically across the travel rather than moving against the folder's own small-signal gain
- **THEN** slot 12 is Tone (short name `Tone`), a post-chain one-pole lowpass applied after every other Drive stage
- **THEN** slot 13 is Symmetry (short name `Sym`), a bipolar offset injected at the folder's own input in phase units, bounded to ±0.02 cycles with a centred default, anchored so that silence-in still produces silence-out — the anchor applying to the fed-back value as well as the output, which is what makes silence an exact fixed point of the feedback recursion at every Symmetry setting. It replaces Bias, whose offset was cancelled again before the folder could see it and which measured inert at the default patch

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
- **THEN** slot 12 is Width Balance (short name `WBal`), which scales only the Width knob's own
  time-offset spread term; the cross-feed weight is a fixed 0 and does not read Width Balance at all
- **THEN** the cross-feed weight this balance produces stays within 0 to 1 inclusive at every knob position,
  so the left/right feedback pair stays a convex combination of the two delay-line reads
- **THEN** the time-offset spread this balance produces never lengthens a read tap beyond the delay
  buffer's own capacity
- **THEN** slot 13 is Crush (short name `Crsh`), a bitcrush stage applied to the feedback tap's repeats
- Check: DELIVERED for the slot-12 scaling clause: `dsp::StereoDelay`'s `widthBalance` field (Delay.hpp) scales only the width-spread term's 0.35f weight, and the cross-feed weight is a fixed 0.0f that does not read it. DELIVERED for the capacity clause, that the time-offset spread this balance produces never lengthens a read tap beyond the delay buffer's own capacity, backed by `app/FroggersDspParityTests.cpp`, `stereo_delay_width_spread_never_reads_past_the_line_capacity`, `stereo_delay_width_spread_bound_holds_across_the_reachable_grid` and `stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks`. The cross-feed convexity clause holds vacuously: at a weight fixed at zero, the left/right feedback pair is trivially a convex combination of the two delay-line reads (weight 0 and weight 1 are both endpoints of the 0-to-1 range), so the clause is satisfied by construction rather than by any ratio Width Balance computes.

#### Scenario: The Reverb bank holds fourteen parameters, complete

- **WHEN** the Reverb bank is enumerated
- **THEN** it holds fourteen named parameters at slot indices 0 through 13
- **THEN** slot 0 is Wet/dry (short name `Wet`) and slot 1 is Send, the page's feed into its tank
- **THEN** slots 2-6 carry Room size, Decay, Pre-delay, Damping and Stereo width, with Damping running one filter per tank line so that damping the tail does not also collapse the stereo image, and Stereo width driving the tank's mid/side output scaling
- **THEN** the tank's L/R cross-feed is a fixed coupling rather than a control, held at the weight the removed Diffusion knob's registered default carried, so retiring that knob does not change the tank at its default
- **THEN** slot 7 is Density (short name `Dens`), the tank's initial echo density, produced by a cascade of allpass sections rather than by cross-feeding the tank's two lines
- Check: DELIVERED for the three clauses above. Slot 7 is Density driving an allpass cascade on the tank's input; the tank's cross-feed is a fixed coupling at the weight the retired knob's default carried; and Damping runs one filter per tank line. They are backed by `reverb_process_reproduces_its_captured_output_exactly`, `reverb_density_travel_raises_the_impulse_responses_echo_density` and `reverb_damping_filter_split_lowers_wet_leg_correlation_at_every_setting` in `app/FroggersDspParityTests.cpp`. The Density and Damping citations directly measure the clauses they back. The cross-feed citation is weaker: `reverb_process_reproduces_its_captured_output_exactly` is a byte-exact regression pin at three Density settings, not a case isolating the cross-feed weight, so it would catch a future regression but does not itself demonstrate the weight is fixed at 0.0 rather than knob-controlled. That structural fact is read directly from source instead: `app/dsp/Reverb.hpp`'s tank cross-wire calls `CrossFeedPair` with a literal zero weight, and the comment immediately above that call states this used to be a knob-controlled blend toward a half-and-half mix as the retired Diffusion knob rose, now fixed regardless of Density's setting. Driving the cross-feed from Stereo width was considered and dropped: no surveyed design ties the two, Dattorro's cross-feed being a fixed figure-eight with no knob and stereo coming from the output tap structure. The fixed-coupling clause follows the same rule this specification already applies to Link, whose coupling term was kept at its own default weight as a named constant when the knob was retired. Every other clause in this scenario describes the bank as it ships. This marker is written because the clauses read identically whether the behaviour exists or is merely wanted, and the scenarios under this requirement carry no `Check:` line by the promoted spec's own convention — which leaves an undelivered clause here indistinguishable from a delivered one.
- **THEN** slot 8 is Mod (short name `Mod`), the collapsed modulation control, and slot 9 is Hold (short name `Hold`)
- **THEN** slot 10 is Tank Drive (short name `TkDv`), a pre-gain applied to the input of the tank feedback path's own in-loop saturator, never to that saturator's output
- **THEN** slot 11 is Grit (short name `Grit`), the tank feedback path routed through a bit-scramble stage ahead of that same in-loop saturator
- **THEN** slot 12 is Tilt (short name `Tilt`), a bipolar post-tank tone shave applied before the existing wet limiter
- **THEN** slot 13 is Tuned (short name `Tund`), the tank's own delay-line lengths driven directly by this parameter's resolved value, with no pitch tracker
