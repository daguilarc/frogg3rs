# froggers-sheaf-parameter-model

## ADDED Requirements

### Requirement: A knob's travel moves the quantity it is named for

A control's travel SHALL move the quantity its name promises, rather than a correlated quantity that happens to change with it. Where a knob's mapping fuses two quantities that move in opposition, the fused pair SHALL be separated so that the named quantity rises monotonically across the travel.

#### Scenario: A fold control adds folds as it is turned up

- **WHEN** the Drive page's Fold is swept from its floor to its top with the page fully wet, Shape at its own registered default
- **THEN** fold depth increases across the travel, measured by a metric that does not saturate at the input level under test
- **AND** where the same sweep is non-monotonic at some other Shape setting, the manual or the check's own comment says so rather than leaving Fold's behaviour there unstated
- **AND** the set of sounds the control reaches is unchanged from before the fix, so no level reaches the crush stages that did not reach them before
- **AND** the knob's midpoint reproduces the shipped default bit-for-bit
- Check: `app/FroggersDspParityTests.cpp`, `frog_block_fold_density_rises_with_knob_through_the_production_router`, which drives `RouteDriveBank` at the bank's registered defaults and asserts the density ratio across the travel, carrying its own liveness control. The scenario names that operating point (Shape at its own default) deliberately: away from it the same measure is not monotonic, which the check's own header comment now states rather than leaving unstated. The reachable set and the bit-identical midpoint are pinned separately by `frog_block_fold_inverted_map_matches_original_within_two_ulp_at_every_mirrored_pair` and `frog_block_fold_inversion_bit_identical_to_original_at_dyadic_knobs`.

#### Scenario: A symmetry control skews the wave monotonically, in both directions

- **WHEN** the Drive page's Symmetry is swept from one end of its travel to the other with the folder engaged
- **THEN** the output's signed asymmetry moves monotonically in one direction across a clear majority of Gain, Fold, Shape and input-level combinations, rather than reversing near-universally the way the shipped control does
- **AND** the two halves of the travel skew the wave in opposite directions from a centred default
- **AND** the control is audible where the folder is engaged, against the same bar the inertness check uses
- **AND** the bound the control is given achieves that at a strictly higher rate than any wider bound, which is what the bound is chosen on
- **AND** the registered default sits at the centre of the travel, where the offset is exactly zero, so the page's default sound is the one that shipped before the control moved
- **AND** the minority of combinations where the statistic still backslides is recorded next to the check that measures it, rather than left unstated
- Check: `app/FroggersDspParityTests.cpp`, `drive_symmetry_signed_asymmetry_is_monotone_across_gain_shape_fold_and_amplitude`, which sweeps a signed second-harmonic statistic (not an energy ratio) across 180 Shape/Gain/Fold/amplitude cells at the +-0.02 bound — measured 154 of 180; `drive_symmetry_bound_is_the_widest_with_the_best_monotonicity` for the comparison the bound is chosen on, measuring 154 against 113 at +-0.06, 65 at +-0.125 and 31 at the superseded +-0.25; `drive_symmetry_energy_ratio_control_distinguishes_from_the_signed_check` as the positive control, proving the rig tells a signed statistic from an energy ratio apart on the same grid; `drive_symmetry_top_of_travel_does_not_read_weaker_than_its_floor` for the specific reversal the superseded control has and this design does not; and `drive_symmetry_registered_default_reproduces_no_offset` for the centred default.

Monotonicity at EVERY cell is deliberately not required, and an earlier version of this scenario did require it. No bound delivers it, and the cause is the one already traced for the withdrawn even-harmonic-energy criterion: `PolynomialDrive` supplies its own even harmonics whose ratio swings non-monotonically with Gain, so the output's even energy passes through a null wherever the polynomial opposes the folder. Removing that means changing the page's ported voice, which is a different change. Requiring it here would promote a requirement nothing satisfies — the failure the promotion rule exists to stop — so the scenario asserts the rate, and the bound comparison it is chosen on, both of which reproduce.

This scenario deliberately asserts a SIGNED quantity. An earlier version required even-harmonic ENERGY relative to odd to rise monotonically across the whole travel, and that is unmeetable by any design in this chain: `PolynomialDrive` supplies its own even harmonics whose ratio swings non-monotonically with Gain, so the output's even energy passes through a null wherever the polynomial's contribution opposes the folder's. Measured across 180 operating cells, no candidate parameterisation exceeds 103 of 180 on the energy criterion and several score worse than the shipped one. Only zeroing the polynomial's even-order coefficients satisfies it, which changes the page's ported voice and breaks its firmware-parity pin — a different change. The positive control for the replacement check is that zeroing, which moves the same measurement to 150 of 150 and so proves the rig can tell the two criteria apart.

### Requirement: No control is silently disabled by another

Where one control makes another inert, that SHALL be stated in the manual, and the inertness SHALL be pinned by a check carrying its own positive control. A knob that does nothing at some setting of another knob, with nothing saying so, is indistinguishable from a broken knob.

#### Scenario: Fold is inert at the top of Fuzz

- **WHEN** Fuzz sits at its maximum and Fold is swept across its whole travel
- **THEN** either the output moves, or the manual states that Fold does nothing there
- Check: `app/FroggersDspParityTests.cpp`, `drive_fold_moves_the_output_at_fuzz_maximum`, which sweeps Fold at Fuzz maximum through the production router and asserts the output moves. Its positive control is the behaviour it replaced: the linear blend multiplied the folder's leg by exactly zero there, so the same sweep measured bit-identical at -240 dB. The manual states which quantity moves at that setting, because what Fold still moves at Fuzz maximum is spectral tilt rather than density.

### Requirement: A crossfade between two signal paths holds its level

A control that crossfades two paths SHALL NOT lose level partway through its travel. Where the two paths are not fully correlated -- including where they differ only in group delay -- an equal-power law SHALL be used, or the paths SHALL be aligned so that a linear law holds.

#### Scenario: The anti-alias control does not dip mid-travel

- **WHEN** the Drive page's anti-alias control is swept end to end on a tone whose harmonics fold, with the bank's other parameters at their registered defaults
- **THEN** the output level does not dip below either endpoint by more than about 1 dB anywhere on that travel
- **AND** where the dip is larger at some other setting of Gain or Shape, the manual says so rather than leaving the control's behaviour there unstated
- **AND** alias reduction is distributed across the travel rather than concentrated in one quarter of it
- Check: `app/FroggersDspParityTests.cpp`, `drive_anti_alias_travel_does_not_dip_below_either_endpoint`, which sweeps the whole travel at five tones through `ProcessDriveBank` with Blend raised to 1.0 and the bank's other parameters at their registered defaults, and asserts the worst dip below the lower endpoint stays under 1 dB; and `drive_anti_alias_crossfade_falls_monotonically_and_the_old_one_pole_barely_moved_it` for the distribution of alias reduction across the travel. The scenario names that operating point deliberately. The bound is met there and is not claimed everywhere: away from those defaults the dip is larger at some Gain and Shape settings, which is what the second clause sends to the manual rather than leaving unstated.

### Requirement: A control's travel does not cost level it never gives back

A control whose name promises emphasis, boost or gain SHALL NOT reduce the output's overall level across its travel. Where a stage is trimmed for headroom, the trim SHALL NOT be paid for out of the named control's own travel, and the control SHALL deliver the quantity it names at the output rather than only inside the stage it sets.

#### Scenario: Peak gain raises the peak without lowering everything else

- **WHEN** the Filter page's Peak gain is swept from its floor to its top, measured through the whole filter chain rather than through the resonant bump alone
- **THEN** the total output level does not fall across the travel
- **AND** the peak's level relative to the frequencies around it rises across the travel by more than it does today, measured against a baseline taken in the same run -- the clause discriminates only against that baseline, because the shipped defect already satisfies a bare "rises"
- **AND** the headroom case the branch trim was added for is still bounded at the control's maximum
- Check: NOT YET DELIVERED. The peak branch is currently divided by its own height, so the travel is flat at the bump's own resonant frequency at every knob position while costing most of a 10 dB attenuation at frequencies away from it. `openspec/changes/frogg3rs-wysiwyg-deliver/tasks.md`'s Peak gain stage delivers this and pins both halves through `RouteFilterBank`; the figures live in that check rather than here, because a figure in prose cannot fail when it drifts.

### Requirement: A document describing a control states what the control does

Prose in the shipped manual SHALL describe the behaviour the code produces at the output, not the behaviour of a stage read in isolation, and SHALL NOT promise an endpoint the signal path does not reach.

#### Scenario: The manual's Comb/Peak endpoints match the blend the DSP applies

- **WHEN** the manual's description of a blend control is read against the blend the code applies
- **THEN** it does not describe either endpoint as isolating one branch, where the blend is floored so both branches remain present
- Check: operator step. The manual's Comb/Peak entry now states the floored equal-power blend and the roughly -22 dB the held-back branch keeps at either extreme, matching the blend in `app/dsp/FilterFx.hpp` and the parity cases over it. No automated gate reads manual prose against DSP behaviour: `app/check_docs_match_parameter_table.py` checks that every table entry exists, not what its sentence claims.

#### Scenario: The manual's Peak gain figure matches the output

- **WHEN** the manual's stated gain for a boost control is compared against the level that control produces at the chain's output
- **THEN** the stated figure is the one the output reaches
- Check: NOT YET DELIVERED. MANUAL.md currently states "up to about +9.5 dB (3x) at the top", which is the resonant bump's own centre gain before the branch trim removes exactly it. `openspec/changes/frogg3rs-wysiwyg-deliver/tasks.md`'s Peak gain stage delivers this.

### Requirement: A gate asserting a claim resolves distinguishes evidence from coincidence

A build gate that fails when a prose claim names something that does not exist SHALL NOT accept a token as resolved merely because some unrelated file in the tree shares its name. A marker declaring a scenario not yet delivered SHALL be recognised by that gate rather than passing as an incidental resolution.

#### Scenario: A bare filename is not evidence that a named check exists

- **WHEN** a spec `Check:` line names a token that matches only a common basename appearing throughout the tree
- **THEN** the gate does not count that line as resolved
- **AND** a line declaring its scenario not yet delivered is counted as declared rather than resolved
- Check: `app/check_spec_checks_resolve.py`, run by the test target in `app/Makefile` as check-spec-checks-resolve. Its resolver is inverted: a token resolves only as an exact known test-case name or an exact repo-relative path that exists, and its declared-manual marker recognises the phrase not yet delivered beside none and operator step. The gate is its own check, and the change proved it red by breaking it once. What it still accepts is recorded rather than claimed closed: an indexed PATH counts as evidence, so a full-path citation of a document proves as little as a bare basename did, and a delegated adversarial pass against the inverted resolver is the deliverable that reports what else gets through.

### Requirement: One mechanism carries one name across pages

A control name SHALL identify one mechanism across the whole instrument. Where two pages carry controls with the same name, those controls SHALL perform the same job, so that what a player learns on one page transfers to the next. Where two pages carry different mechanisms, those mechanisms SHALL carry different names.

A control named for a mechanism SHALL perform that mechanism. Diffusion means smearing a signal in time; a control that only cross-feeds two channels is not diffusion whatever the ported firmware called it.

#### Scenario: Diffusion smears in time on every page that offers it

- **WHEN** a control named Diffusion is swept on any page
- **THEN** it smears transients in time through an allpass with a real per-section delay, rather than cross-feeding two channels
- Check: NOT YET DELIVERED. The Delay bank's Diffusion does this today; the Reverb bank's slot 7 is a cross-feed weight with no delay and no allpass. `openspec/changes/frogg3rs-wysiwyg-deliver/tasks.md`'s cross-feed and Reverb stage delivers this by replacing the Reverb mechanism and renaming the slot.

#### Scenario: A stereo-image control widens across its whole travel

- **WHEN** either page's Stereo width is swept from floor to top
- **THEN** L/R correlation of that stage's wet signal falls monotonically
- **AND** where the control drives more than one stereo mechanism, every mechanism it drives widens in the same direction across that travel
- Check: NOT YET DELIVERED. Reverb's Stereo width gains the cross-feed currently behind slot 7. Whether that cross-feed narrows as it rises, and so has to be driven inversely, is measured by `openspec/changes/frogg3rs-wysiwyg-deliver/tasks.md`'s cross-feed measurement before its implementation; the requirement above states the outcome the control's name promises, and the sense that achieves it is read off the measurement rather than assumed here.

#### Scenario: A control that colours says so

- **WHEN** Density is raised toward its top
- **THEN** the manual states that it trades smoothness for metallic coloration, and roughly where on the travel that coloration becomes audible
- Check: NOT YET DELIVERED. `openspec/changes/frogg3rs-wysiwyg-deliver/tasks.md`'s cross-feed and Reverb stage measures the ringing and writes it down.

## MODIFIED Requirements

### Requirement: A Drive page control's travel spends itself where the control is heard

WHEN a Drive page control is mapped from its knob to the coefficient it drives, THE application SHALL choose the mapping so that comparable knob movements produce comparable audible changes across the control's travel, measured against the band the instrument actually produces rather than against the coefficient's own arithmetic range, and SHALL place the control's floor where it begins to act rather than below it: no more than a hundredth of a control's travel may be indistinguishable from its floor, judged on magnitude spectra so that inaudible phase rotation is not counted as an effect. A control whose stated job is inaudible across most of its travel SHALL be re-mapped, re-defaulted, or re-named to say what it does. Two controls on this page are held to this: the Blend stage's Phase allpass, whose audible effect exists only through how the rotated wet signal sums with the dry path, and the anti-alias control, which SHALL reject aliasing measurably across its travel rather than carry a name its filter cannot deliver.

Restated because its inert-at-default scenario names two controls this change removes, and the tests that scenario cites are deleted with them.

<!-- RESTATES-EXCEPT
NOTE (pending `frogg3rs-wysiwyg-deliver` delivery): Link and the old Waveshaper offset/Bias
  keeps: none
-->

The dropped line is the transitional note the promoted text carries while this change is in flight. It says the promoted scenario was updated ahead of this change's archival so the gate would not dangle on deleted tests. Restating it here would carry a pointer to this change into the requirement this change promotes, where it would read as a note about some later change nobody has written.

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

- **WHEN** Symmetry is read with the folder disengaged
- **THEN** Symmetry's effect on the output is more than 10 dB smaller than with the folder engaged, not bit-identical — the floor that keeps Fuzz from silencing the folder's leg entirely also keeps a small residual of Symmetry's own effect
- **AND** the manual names which control has to be moved for Symmetry to act
- Check: `app/FroggersDspParityTests.cpp`, `drive_symmetry_is_inert_when_the_folder_is_not_engaged`, which measures Symmetry's own audible effect through `ProcessDriveBank` with the folder fully engaged (Fuzz 0.0) against the same sweep with the folder floored to its minimum blend weight (Fuzz 1.0) — measured about 3 dB engaged against about -19 dB disengaged, a real but far smaller effect there, matching the manual's own statement of which control (Fuzz) has to move for Symmetry to act.

### Requirement: One sixteen-slot bank per Froggers page

Each existing Froggers page SHALL become exactly one bank of sixteen parameter slots. Pages SHALL NOT be merged. A bank's own parameters SHALL occupy the leading slots; remaining parameter slots MAY be empty, or MAY hold additional named parameters where a bank's slate has been explicitly decided and expanded.

Restated because the Drive bank's slot 10 and slot 13 change, and the Reverb bank's slot 7 changes its name and its mechanism. No slot moves. Every other scenario under this requirement is carried forward VERBATIM from the post-archive promoted text, because a MODIFIED requirement replaces the requirement's whole body and omitting them would delete four banks' layouts that this change does not touch.

The Drive and Reverb scenarios carry edits beyond the slots this change moves, and they are declared here rather than left to a diff: each bank's count reads "fourteen parameters, complete" rather than leaving the earlier "not nine" phrasing implicit, Reverb's slot 8 and 9 follow the post-archive layout in which Mod is collapsed and Hold carries its own short name, Reverb's renamed slot 7 takes the short name `Dens` on the same rule its neighbours follow -- the word truncated, as `Diffusion` gave `Diff` and `Damping` gives `Damp` -- and Drive's slot 9 description is corrected. That last one is a finding, not a tidy-up: the promoted text describes slot 9 in terms the shipped code does not match, and a specification records what the system does.

Every promoted clause this requirement does not carry forward word for word is named below. The list is checked mechanically: a clause dropped without appearing here fails the build, and an entry here that matches no dropped clause fails it too, so neither the omission nor the stale declaration can sit reading as deliberate.

<!-- RESTATES-EXCEPT
slot 9 is Anti-Alias Brightness (short name `ABrt`), the oversampling anti-alias filter's own cutoff
  keeps: slot 9 is Anti-Alias Brightness (short name `ABrt`)
slot 10 is Feedback (short name `Fb`), the amount of the folder's own output
  keeps: returned to the folder's own input, one oversampled sample later
slot 11 is Fold (short name `Fold`), the pre-fold scale ahead of the sine-fold stage
  keeps: slot 11 is Fold (short name `Fold`)
slot 13 is Symmetry (short name `Sym`), an offset injected at the folder's own input
  keeps: anchored so that silence-in still produces silence-out
NOTE (pending `frogg3rs-wysiwyg-deliver` delivery): slots 10 and 13 are updated ahead
  keeps: none
slots 2-8 carry the bank's remaining original parameters, with Mod depth and Mod rate collapsed
  keeps: the collapsed modulation control
slot 9 is Hold (short name `Hold`), displaced there by Send taking slot 1
  keeps: slot 9 is Hold (short name `Hold`)
-->

What each one becomes. Drive slot 9 gains the description of what the control crossfades, which the promoted line omits. Drive slot 10 restates the same Feedback control in the delta's own words, naming the oscillation measurement behind the folder-leg topology. Drive slot 11 keeps the promoted meaning and says it as a drive rather than a scale, which is what the delivered map makes it. Drive slot 13 gains the bound and the centred default the new law carries, keeping the promoted anchor clause verbatim inside it. Reverb's slots 2-8 line splits so that Stereo width can be named as the control that now carries the cross-feed, and Reverb slot 9 is restated alongside slot 8 in one line. Both NOTEs are transitional pointers to this change and do not outlive it.

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
- **THEN** slot 12 is Width Balance (short name `WBal`), the ratio between the Width knob's time-offset
  spread and its cross-feed blend, independent of the Width knob's own value
- **THEN** the cross-feed weight this balance produces stays within 0 to 1 inclusive at every knob position,
  so the left/right feedback pair stays a convex combination of the two delay-line reads
- **THEN** the time-offset spread this balance produces never lengthens a read tap beyond the delay
  buffer's own capacity
- **THEN** slot 13 is Crush (short name `Crsh`), a bitcrush stage applied to the feedback tap's repeats

#### Scenario: The Reverb bank holds fourteen parameters, complete

- **WHEN** the Reverb bank is enumerated
- **THEN** it holds fourteen named parameters at slot indices 0 through 13
- **THEN** slot 0 is Wet/dry (short name `Wet`) and slot 1 is Send, the page's feed into its tank
- **THEN** slots 2-6 carry Room size, Decay, Pre-delay, Damping and Stereo width, with Stereo width driving both the tank's mid/side output scaling and its L/R cross-feed, each in the widening direction
- **THEN** slot 7 is Density (short name `Dens`), the tank's initial echo density, produced by a cascade of allpass sections rather than by cross-feeding the tank's two lines
- Check: NOT YET DELIVERED for the two clauses above. Today slot 7 is Diffusion (short name `Diff`) and is the tank's L/R cross-feed weight, and Stereo width drives the mid/side output scaling alone. `openspec/changes/frogg3rs-wysiwyg-deliver/tasks.md`'s cross-feed and Reverb stage delivers both, measuring the cross-feed's sense before picking the drive direction. Every other clause in this scenario describes the bank as it ships. This marker is written because the clauses read identically whether the behaviour exists or is merely wanted, and the scenarios under this requirement carry no `Check:` line by the promoted spec's own convention — which leaves an undelivered clause here indistinguishable from a delivered one.
- **THEN** slot 8 is Mod (short name `Mod`), the collapsed modulation control, and slot 9 is Hold (short name `Hold`)
- **THEN** slot 10 is Tank Drive (short name `TkDv`), a pre-gain applied to the input of the tank feedback path's own in-loop saturator, never to that saturator's output
- **THEN** slot 11 is Grit (short name `Grit`), the tank feedback path routed through a bit-scramble stage ahead of that same in-loop saturator
- **THEN** slot 12 is Tilt (short name `Tilt`), a bipolar post-tank tone shave applied before the existing wet limiter
- **THEN** slot 13 is Tuned (short name `Tund`), the tank's own delay-line lengths driven directly by this parameter's resolved value, with no pitch tracker
