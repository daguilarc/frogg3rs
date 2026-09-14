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
- Check: `app/FroggersDspParityTests.cpp`, `drive_fold_moves_the_output_at_fuzz_maximum`, which sweeps Fold at Fuzz maximum through the production router and asserts the output moves. Its positive control is the behaviour it replaced: the linear blend multiplied the folder's leg by exactly zero there, so the same sweep measured bit-identical at -240 dB. The manual states which quantity moves at that setting: at Fuzz maximum Fold moves the balance between harmonics rather than density, against a total level that barely moves. It is not a spectral tilt — measured across a Gain by Shape grid there is no monotone slope, and which harmonic moves depends on Gain and Shape.

### Requirement: A crossfade between two signal paths holds its level

A control that crossfades two paths SHALL NOT lose level partway through its travel. Where the two paths are not fully correlated -- including where they differ only in group delay -- an equal-power law SHALL be used, or the paths SHALL be aligned so that a linear law holds.

#### Scenario: The anti-alias control does not dip mid-travel

- **WHEN** the Drive page's anti-alias control is swept end to end on a tone whose harmonics fold, with the bank's other parameters at their registered defaults
- **THEN** the output level does not dip below either endpoint by more than about 1 dB anywhere on that travel
- **AND** where the dip is larger at some other setting of Gain or Shape, the manual says so rather than leaving the control's behaviour there unstated
- **AND** alias reduction is distributed across the travel rather than concentrated in one quarter of it
- Check: `app/FroggersDspParityTests.cpp`, `drive_anti_alias_travel_does_not_dip_below_either_endpoint`, which sweeps the whole travel at five tones through `ProcessDriveBank` with Blend raised to 1.0 and the bank's other parameters at their registered defaults, and asserts the worst dip below the lower endpoint stays under 1 dB; and `drive_anti_alias_crossfade_falls_monotonically_and_the_old_one_pole_barely_moved_it` for the distribution of alias reduction across the travel. The scenario names that operating point deliberately. The bound is met there and is not claimed everywhere: away from those defaults the dip is larger at some Gain and Shape settings, which is what the second clause sends to the manual rather than leaving unstated.

### Requirement: A control's travel does not silently cost level

A control whose name promises emphasis, boost or gain SHALL NOT cost output level silently: where its travel costs level that cannot be removed without degrading the output, the cost SHALL be stated where the control is described, and SHALL be pinned by a check measuring it. A cost that CAN be removed without degrading the output is removed instead of documented.

#### Scenario: Peak gain's level cost is measured, stated, and pinned

- **WHEN** the Filter page's Peak gain is swept from its floor to its top, measured through the whole filter chain rather than through the resonant bump alone
- **THEN** the level at the resonant bump's own centre frequency stays flat across the whole travel
- **AND** the level at frequencies at least three octaves away falls at every step of that travel, which is the liveness control for the flat row and is asserted in the same case
- **AND** the manual and the quick dictionary each state what the control delivers at the output and what it costs, rather than quoting the bump's own centre gain
- Check: `app/FroggersDspParityTests.cpp`, `filter_bank_peak_gain_travel_measurement_at_and_away_from_resonance`, which drives the Filter bank through a replica of `RouteFilterBank`'s setter order at the registered defaults and pins both halves in one case: the level at the bump's own centre frequency stays flat across the whole travel, and the level on both rows at least three octaves away falls at every step and by several dB end to end, which is the flat row's liveness control. The manual and the quick dictionary each state what the control delivers at the output and what it costs. The cost cannot be removed, only reduced: the peak branch is divided by its own height, and any makeup compensating that sits ahead of `peakLimiter`, whose ceiling is `kStageCeiling` and whose `OutputLimiter::DesiredMagnitude` asymptotes toward it without reaching it, so the branch's contribution is hard-capped whatever makeup precedes the limiter. An earlier wording of this requirement demanded that total level not fall at all, which is unsatisfiable at that placement. Every figure lives in the check rather than here, because a figure in prose cannot fail when it drifts.

### Requirement: A document describing a control states what the control does

Prose in the shipped manual SHALL describe the behaviour the code produces at the output, not the behaviour of a stage read in isolation, and SHALL NOT promise an endpoint the signal path does not reach.

#### Scenario: The manual's Comb/Peak endpoints match the blend the DSP applies

- **WHEN** the manual's description of a blend control is read against the blend the code applies
- **THEN** it does not describe either endpoint as isolating one branch, where the blend is floored so both branches remain present
- Check: operator step. The manual's Comb/Peak entry now states the floored equal-power blend and the roughly -22 dB the held-back branch keeps at either extreme, matching the blend in `app/dsp/FilterFx.hpp` and the parity cases over it. No automated gate reads manual prose against DSP behaviour: `app/check_docs_match_parameter_table.py` checks that every table entry exists, not what its sentence claims.

#### Scenario: The manual's Peak gain figure matches the output

- **WHEN** the manual's stated gain for a boost control is compared against the level that control produces at the chain's output
- **THEN** the stated figure is the one the output reaches
- Check: `app/FroggersDspParityTests.cpp`, `filter_bank_peak_gain_travel_measurement_at_and_away_from_resonance`, which drives the Filter bank through a replica of `RouteFilterBank`'s setter order at the registered defaults and pins both halves in one case: the level at the bump's own centre frequency stays flat across the whole travel, and the level at frequencies at least three octaves away falls at every step. Both documents now state what the control delivers at the output — the peak rises relative to its surroundings because the surroundings fall — and state the cost, rather than quoting the bump's own centre gain as if the output reached it.

### Requirement: A gate asserting a claim resolves distinguishes evidence from coincidence

A build gate that fails when a prose claim names something that does not exist SHALL NOT accept a token as resolved merely because some unrelated file in the tree shares its name. A marker declaring a scenario not yet delivered SHALL be recognised by that gate rather than passing as an incidental resolution.

#### Scenario: A bare filename is not evidence that a named check exists

- **WHEN** a spec `Check:` line names a token that matches only a common basename appearing throughout the tree
- **THEN** the gate does not count that line as resolved
- **AND** a line declaring its scenario not yet delivered is counted as declared rather than resolved
- Check: `app/check_spec_checks_resolve.py`, run by the test target in `app/Makefile` as check-spec-checks-resolve. Its resolver is inverted: a token resolves only as an exact known test-case name or an exact repo-relative path that exists, and its declared-manual marker recognises the phrase not yet delivered beside none and operator step. The gate is its own check, and the change proved it red by breaking it once. What it still accepts is recorded in the script's own header rather than claimed closed: an indexed PATH counts as evidence, so a full-path citation of a document proves as little as a bare basename did. The delegated adversarial pass against the resolver is spent, run by a context that did not build it, and the two holes that defeated the gate's purpose are closed — a scenario retitled by one comma exempting every bullet under it from any check, and a case name appearing only in comment text being indexed as a real test. The rest are recorded there and left alone, because every gate has holes and a recorded hole is a finding rather than an instruction.

### Requirement: One mechanism carries one name across pages

A control name SHALL identify one mechanism across the whole instrument. Where two pages carry controls with the same name, those controls SHALL perform the same job, so that what a player learns on one page transfers to the next. Where two pages carry different mechanisms, those mechanisms SHALL carry different names.

A control named for a mechanism SHALL perform that mechanism. Diffusion means smearing a signal in time; a control that only cross-feeds two channels is not diffusion whatever the ported firmware called it.

#### Scenario: Diffusion smears in time on every page that offers it

- **WHEN** a control named Diffusion is swept on any page
- **THEN** it smears transients in time through an allpass with a real per-section delay, rather than cross-feeding two channels
- Check: `app/FroggersDspParityTests.cpp`, `reverb_density_travel_raises_the_impulse_responses_echo_density`, which drives Density's travel and asserts the impulse response's normalised echo density rises across it, asserting in the same case that the response carries energy at Density's floor and that every section's configured delay exceeds one sample, so an unconfigured cascade acting as a phaser cannot pass. Both pages smear in time through the same cascade: the Delay bank's Diffusion drives it on the wet tap, Reverb's Density on the tank's input.

#### Scenario: A stereo-image control widens across its whole travel

- **WHEN** either page's Stereo width is swept from floor to top
- **THEN** L/R correlation of that stage's wet signal falls monotonically
- **AND** where the control drives more than one stereo mechanism, every mechanism it drives widens in the same direction across that travel
- Check: PARTLY DELIVERED. Reverb's Stereo width drives one mechanism, the tank's mid/side output scaling; giving it the cross-feed as a second was considered and dropped, because no surveyed design ties the two and the cross-feed is retired to a fixed coupling instead. What held the control back was neither knob: one damping filter served both tank lines, so the second line's output carried the first through that filter's own memory. Each line now has its own filter and correlation falls where it could not before: `app/FroggersDspParityTests.cpp`, `reverb_damping_filter_split_lowers_wet_leg_correlation_at_every_setting`, which pins the split against the shared filter it replaced over one grid in one run. Reverb's OWN travel is now pinned in the same file by `reverb_density_correlation_travel_is_a_small_fraction_of_stereo_widths_live_monotonic_travel`, whose width row asserts each step's correlation is strictly below the one before it, across all four steps of the five-point grid {0.0, 0.25, 0.5, 0.75, 1.0} -- measuring 1.000000, 0.874229, 0.576760, 0.246914, -0.035547, a strict point-to-point monotonic fall -- with its own liveness assertion (the width row's travel exceeding 0.5) ruling out a dead tank passing vacuously. Because Reverb's Stereo width drives only the one mechanism named above, the second clause is vacuous for this page. REFUTED for the Delay page, which is a different finding from undelivered and is recorded rather than narrowed away. Measuring Delay's wet pair across the same five-point grid, twice and by two contexts, the page does not satisfy either clause. At Feedback's registered default the row reads 1.000000, 0.000896, -0.019932, -0.012245, -0.006245 -- it collapses at the first step and then wanders near zero without direction, so it does not fall monotonically. At a raised Feedback of 0.7 the row reads 1.000000, 0.001677, 0.009796, 0.052801, 0.119325: correlation RISES across the top three quarters of the travel, so the control named for the stereo field narrows the image there rather than widening it. The cause is structural rather than statistical. `dsp::StereoDelay::Process` computes its cross-feed weight as half the width knob and applies `dsp::CrossFeedPair` INSIDE the recirculating feedback write, not at an output-only tap the way the reverb tank's own width blend is applied. At the top of the travel that weight reaches exactly 0.5, where `CrossFeedPair` returns two identical values by construction, so the feedback content becomes mono and is replayed on every repeat. The two mechanisms this control drives therefore move in OPPOSITE directions as it is raised: the read-tap offset decorrelates while the cross-feed re-correlates. That is precisely what the scenario's second clause forbids. The requirement stands and the mechanism changes: `frogg3rs-delay-width-wysiwyg-repair` repairs the cross-feed so the two mechanisms widen together, and this line carries that repair's own check once it lands. Narrowing the scenario to the Reverb page was rejected as editing a requirement to match the code.

#### Scenario: A control that colours says so

- **WHEN** Density is raised toward its top
- **THEN** the manual states that it trades smoothness for coloration, names the character that coloration has, and says roughly where on the travel it becomes audible
- Check: operator step. `app/FroggersDspParityTests.cpp`'s `reverb_density_travel_raises_the_impulse_responses_echo_density` measures the impulse response's normalised echo density rising across Density's travel -- 0.0514, 0.2342, 0.3403, 0.4132, 0.4782 over the grid {0.0, 0.25, 0.5, 0.75, 1.0} -- the figures the manual's sentence is read against; most of the rise happens between the first two points. The manual's Density entry (Reverb bank) now states plainly that raising Density trades smoothness for coloration -- "the early reflections lose their clean, sparse spacing and start to sound like short, quick repeats rather than a smooth wash" -- and roughly where on the travel that becomes audible -- "turning the knob through roughly its first quarter already makes the coloration clearly audible, and the rest of the travel adds progressively less on top of it" -- matching where the cited figures rise fastest. No automated gate reads manual prose against DSP behaviour: `app/check_docs_match_parameter_table.py` checks that every table entry exists, not what its sentence claims.

## MODIFIED Requirements

### Requirement: A Drive page control's travel spends itself where the control is heard

WHEN a Drive page control is mapped from its knob to the coefficient it drives, THE application SHALL choose the mapping so that comparable knob movements produce comparable audible changes across the control's travel, measured against the band the instrument actually produces rather than against the coefficient's own arithmetic range, and SHALL place the control's floor where it begins to act rather than below it: no more than a hundredth of a control's travel may be indistinguishable from its floor, judged on magnitude spectra so that inaudible phase rotation is not counted as an effect. A control whose stated job is inaudible across most of its travel SHALL be re-mapped, re-defaulted, or re-named to say what it does. Two controls on this page are held to this: the Blend stage's Phase allpass, whose audible effect exists only through how the rotated wet signal sums with the dry path, and the anti-alias control, which SHALL reject aliasing measurably across its travel rather than carry a name its filter cannot deliver.

Restated because its inert-at-default scenario names two controls this change removes, and the tests that scenario cites are deleted with them.

<!-- RESTATES-EXCEPT
NOTE (pending `frogg3rs-delay-width-wysiwyg-repair` delivery): Link and the old Waveshaper offset/Bias
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
NOTE (pending `frogg3rs-delay-width-wysiwyg-repair` delivery): slots 10 and 13 are updated ahead
  keeps: none
slots 2-8 carry the bank's remaining original parameters, with Mod depth and Mod rate collapsed
  keeps: the collapsed modulation control
slot 9 is Hold (short name `Hold`), displaced there by Send taking slot 1
  keeps: slot 9 is Hold (short name `Hold`)
-->

What each one becomes. Drive slot 9 gains the description of what the control crossfades, which the promoted line omits. Drive slot 10 restates the same Feedback control in the delta's own words, naming the oscillation measurement behind the folder-leg topology. Drive slot 11 keeps the promoted meaning and says it as a drive rather than a scale, which is what the delivered map makes it. Drive slot 13 gains the bound and the centred default the new law carries, keeping the promoted anchor clause verbatim inside it. Reverb's slots 2-8 line splits so that Damping can be named as running one filter per tank line, and so that the tank's cross-feed can be named as a fixed coupling rather than a control. Reverb slot 9 is restated alongside slot 8 in one line. Both NOTEs are transitional pointers to this change and do not outlive it.

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
- **THEN** slots 2-6 carry Room size, Decay, Pre-delay, Damping and Stereo width, with Damping running one filter per tank line so that damping the tail does not also collapse the stereo image, and Stereo width driving the tank's mid/side output scaling
- **THEN** the tank's L/R cross-feed is a fixed coupling rather than a control, held at the weight the removed Diffusion knob's registered default carried, so retiring that knob does not change the tank at its default
- **THEN** slot 7 is Density (short name `Dens`), the tank's initial echo density, produced by a cascade of allpass sections rather than by cross-feeding the tank's two lines
- Check: DELIVERED for the three clauses above. Slot 7 is Density driving an allpass cascade on the tank's input; the tank's cross-feed is a fixed coupling at the weight the retired knob's default carried; and Damping runs one filter per tank line. They are backed by `reverb_process_reproduces_its_captured_output_exactly`, `reverb_density_travel_raises_the_impulse_responses_echo_density` and `reverb_damping_filter_split_lowers_wet_leg_correlation_at_every_setting` in `app/FroggersDspParityTests.cpp`. The Density and Damping citations directly measure the clauses they back. The cross-feed citation is weaker: `reverb_process_reproduces_its_captured_output_exactly` is a byte-exact regression pin at three Density settings, not a case isolating the cross-feed weight, so it would catch a future regression but does not itself demonstrate the weight is fixed at 0.0 rather than knob-controlled. That structural fact is read directly from source instead: `app/dsp/Reverb.hpp`'s tank cross-wire calls `CrossFeedPair` with a literal zero weight, and the comment immediately above that call states this used to be a knob-controlled blend toward a half-and-half mix as the retired Diffusion knob rose, now fixed regardless of Density's setting. Driving the cross-feed from Stereo width was considered and dropped: no surveyed design ties the two, Dattorro's cross-feed being a fixed figure-eight with no knob and stereo coming from the output tap structure. The fixed-coupling clause follows the same rule this specification already applies to Link, whose coupling term was kept at its own default weight as a named constant when the knob was retired. Every other clause in this scenario describes the bank as it ships. This marker is written because the clauses read identically whether the behaviour exists or is merely wanted, and the scenarios under this requirement carry no `Check:` line by the promoted spec's own convention — which leaves an undelivered clause here indistinguishable from a delivered one.
- **THEN** slot 8 is Mod (short name `Mod`), the collapsed modulation control, and slot 9 is Hold (short name `Hold`)
- **THEN** slot 10 is Tank Drive (short name `TkDv`), a pre-gain applied to the input of the tank feedback path's own in-loop saturator, never to that saturator's output
- **THEN** slot 11 is Grit (short name `Grit`), the tank feedback path routed through a bit-scramble stage ahead of that same in-loop saturator
- **THEN** slot 12 is Tilt (short name `Tilt`), a bipolar post-tank tone shave applied before the existing wet limiter
- **THEN** slot 13 is Tuned (short name `Tund`), the tank's own delay-line lengths driven directly by this parameter's resolved value, with no pitch tracker

### Requirement: An insert effect page's master returns the dry signal at its floor

WHEN an insert effect page carries a wet/dry master, THAT control SHALL crossfade the page's output against its input using a POWER-COMPLEMENTARY law — the two legs' gains SHALL be `cos` and `sin` of a common angle, so that their squares sum to one — and SHALL return the input exactly, sample for sample, at its floor, and it SHALL be named as an effect pedal names it and placed first among that page's parameters. A linear crossfade SHALL NOT be used: it holds level only where its two legs are fully correlated, and no page's wet leg is. Both endpoints SHALL be exact by construction rather than by trusting the trigonometric functions, which do not land on zero at a right angle in single precision. Where a page's wet path is fed through a Send, the master SHALL NOT duck the dry signal in proportion to a wet path that Send has left empty, and that behaviour SHALL be one shared mechanism rather than one implementation per page.

Restated because its explanatory paragraph stated a Reverb level measured with the page's Send at its registered default, which is closed. The tank is never fed there, so the figure described the page's output limiter acting on a dry pass-through rather than the wet path it was cited for. The sentence is dropped rather than re-measured into prose: no check pins the tank's wet level, and a figure in prose cannot fail when it drifts. Every other clause, and all eight scenarios, carry forward verbatim. What the paragraph asserts is unchanged and remains true: no page's wet path is transparent at rest.


This requirement does NOT ask a page's wet path to be transparent at rest. No page's wet path is: a reverb's wet path is a tank, a delay's is echoes, and a distortion's is a waveshaper. Reverb and Delay read as transparent at rest because their Sends default closed and their wet paths are empty, not because those paths are unity. The property the three pages genuinely share is the one stated above, and each of them meets it by construction of the crossfade.

A page's gain stage SHALL be named Gain and SHALL govern the stage it drives rather than the page as a whole, because a page's bit and rate manglers act at any level and are not gated by gain. Every insert effect page whose wet path is a send-and-return SHALL open with the same two controls in the same order — the master wet/dry first, that page's Send second. A page's wet control SHALL leave a floor of dry signal in place where fully replacing the source destroys what the page is processing, as it does for a reverb and a delay, and SHALL reach fully wet where replacing the source is a sound the page exists to make, as it is for a distortion; wherever that floor applies, both the code and the manual SHALL record why it is there.

#### Scenario: The master returns the dry signal exactly at its floor

- **WHEN** an insert effect page's wet/dry control sits at its floor
- **THEN** that page's output is its input, sample for sample
- Check: `app/FroggersDspParityTests.cpp`'s `drive_blend_phase_authored_zero_blend_is_exact_passthrough`, the dry-at-floor pin for the Drive page, which passes today at -240 dB; `app/dsp/Reverb.hpp`'s mix and `app/dsp/Delay.hpp`'s are the same crossfade expression.

#### Scenario: The master does not lose level partway through its travel

- **WHEN** an insert effect page's wet/dry master is swept from its floor to its top
- **THEN** the page's output level does not dip below its dry level by more than a small margin anywhere on that travel, rather than notching partway and recovering
- **AND** this holds across notes and across the page's own gain settings, not at one tested frequency
- Check: `app/FroggersDspParityTests.cpp`, `drive_blend_travel_holds_level_within_1_3_db_across_gain_and_frequency`, the Drive page's blend-travel case, which measures a worst dip of -1.21989 dB where the linear law it replaced measured -4.10 dB.

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
- Check: `app/FroggersDspParityTests.cpp`'s `delay_wet_dry_leaves_dry_untouched_while_send_is_closed_and_moves_it_once_fed` pins the Delay bank's dry-until-fed behavior against the renamed control, and `reverb_wet_authority_tracks_whether_send_is_open_and_the_tank_is_fed` pins the identical `wetAuthority` mechanism for Reverb's Send.

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
