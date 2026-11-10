## ADDED Requirements

### Requirement: Separate engine and matrix cadence
The app SHALL run four audio/source samples for every host-rate matrix frame. At a 48 kHz host rate this SHALL be 192 kHz audio and a 48 kHz matrix. Target smoothing SHALL preserve its former time constant; the Fuego seam and UI smoothing SHALL execute once per matrix frame. Recording SHALL remain at host rate.

#### Scenario: Nonaligned callback boundaries preserve cadence
- **WHEN** a 48 kHz host processes three 37-frame blocks
- **THEN** audio/source processing advances 444 samples and the matrix advances 111 frames
- **AND** recording reports 48 kHz
- Check: `engine_runs_four_samples_per_host_matrix_frame_and_records_at_host_rate`

#### Scenario: Host rate changes retain real-time parameter timing
- **WHEN** the host negotiates 96 kHz
- **THEN** the engine runs at 384 kHz and the matrix runs at 96 kHz with converted one-pole coefficients
- Check: `configure_processing_timing_is_wired_at_96khz_prepare`

### Requirement: Filter sources and interpolate processed roots
The reusable Sheaf bridge SHALL low-pass source samples before matrix-rate evaluation, retain source-filter history across callbacks, and publish interpolated root caches at weights 1/4, 1/2, 3/4 and 1. The application seam SHALL be applied once before interpolation, for every configured voice.

#### Scenario: Source alias rejection
- **WHEN** a 1 kHz source and a 47 kHz source are evaluated at 192/48 kHz
- **THEN** the 1 kHz source remains present and the 47 kHz residual is below one thousandth of its RMS
- Check: `rate_separated_sources_reject_aliases_before_matrix_evaluation`

#### Scenario: Processed multivoice publication
- **WHEN** two voices receive distinct post-seam values
- **THEN** each voice publishes its own four-step interpolation and the seam runs once
- Check: `rate_separated_parameters_interpolate_post_seam_once_per_frame`

### Requirement: Explicit formal correspondence boundary
The Lean model SHALL describe cadence, publication and Sheaf-specific behavioral extensions independently of runtime evaluation strategy. Its gate SHALL kernel-check theorems, reject unapproved axioms and report changes to reviewed source fingerprints. Rational model proofs SHALL NOT be presented as verification of C++ floating-point behavior.

### Requirement: Source hold and editable frozen parameters
Stopped transport SHALL hold modulation source-filter state exactly while matrix evaluation and parameter edits continue.

#### Scenario: Stopped source hold
- **WHEN** the transport stops after a live noise source has been observed moving
- **THEN** the matrix source value remains bit-identical across subsequent stopped blocks
- Check: `free_running_modulation_source_holds_while_stopped_with_positive_control`

#### Scenario: Frozen encoder edit reaches audio
- **WHEN** a matched frozen control and edited instrument receive different delay-mix values
- **THEN** the edited parameter converges through the normal matrix path and its audio differs from the unchanged control
- Check: `encoder_edit_while_frozen_changes_the_output_measurably`

### Requirement: Scalar depth reuse preserves edits
The evaluator MAY reuse an unchanged scalar nested leaf. It SHALL re-evaluate after a changed fresh raw center, cache overwrite or wiring edit.

#### Scenario: Reused leaves remain editable
- **WHEN** a reused leaf receives a direct scene-reference edit, cache overwrite or child wiring change
- **THEN** its evaluated cache reflects the current inputs
- Check: `prepared_scalar_depth_leaf_reuses_only_an_unchanged_image`

### Requirement: Checked continuous-band source-filter attenuation
The formal gate SHALL export and check all 20 configured binary32 source-filter
coefficients against their exact real embeddings in Lean. At a 192 kHz engine
rate, the real fixed-coefficient response SHALL have power gain at most
1/4000000 throughout 47..96 kHz and at least 1/2 throughout 0..1 kHz. The model
SHALL prove a relative amplitude-gain bound of 1/1000 for every high/reference
pair from those bands and the 47 kHz to 1 kHz four-to-one sampling identity.
These claims SHALL describe steady-state frequency response; they SHALL NOT be
presented as IEEE processing-error bounds, transient guarantees, transition-band
suppression, output-FIR proofs or whole-synth alias freedom.

#### Scenario: Native response agrees with proved band bounds
- **WHEN** the configured C++ source filter's response is checked at 8193 points in each proved band
- **THEN** every point meets the formal power bounds and the 47 kHz/1 kHz pair meets the relative bound
- Check: `rate_separated_source_filter_matches_proved_response_band_bounds`

### Requirement: Stateful finite-arithmetic and nonlinear composition contracts
The formal model SHALL include full direct-form-I biquad memory, verified
positive energy certificates for the exported coefficients, initial-history decay,
and accumulated bounded disturbances. It SHALL model nearest-even binary32 and
binary64 arithmetic, subnormal spacing, FTZ/DAZ effects, and finite-operand budgets.
The actual 287 stored output FIR taps SHALL be exported and checked. FIR history
flush, L1 error gain, guarded mixed precision, and final-clamp error propagation
SHALL be proved. The model SHALL expose its finite-operand, rounding-mode and
compiler-correspondence assumptions and SHALL NOT claim a universal rounded or
nonlinear whole-synth spectral bound.

Fuego's cell/code crossings SHALL carry explicit jump budgets. Fixed-code,
within-cell behavior MAY preserve a perturbation; the linear filter's small gain
SHALL NOT be assumed across unconstrained bit-cell crossings.

#### Scenario: Actual state and arithmetic agree with the contracts
- **WHEN** actual source-filter state evolution, 512-sample DC priming and rounding residuals are exercised
- **THEN** state copies, transient checks and guarded residual bounds pass
- Check: `source_filter_state_decay_and_512_sample_priming_match_model`
- Check: `source_filter_native_rounding_residual_fits_state_disturbance_budget`

#### Scenario: Native floating-point modes are explicit
- **WHEN** nearest-even ties, subnormal handling and actual ARM64 FTZ/DAZ modes are tested
- **THEN** their observed behavior matches the model and the environment is restored
- Check: `binary32_native_ties_and_subnormal_handling_fit_rounding_model`
- Check: `binary32_native_flush_mode_matches_ftz_and_daz_contract`

#### Scenario: Actual FIR memory and mixed precision match the model
- **WHEN** the stored taps, differing initial histories and real mixed-precision path are exercised
- **THEN** history flush, L1 gain, exact tap order and arithmetic error budgets pass
- Check: `actual_output_fir_history_flush_and_l1_gain_match_formal_model`
- Check: `actual_output_fir_mixed_precision_error_fits_proved_budget`

#### Scenario: Nonlinear boundaries are retained in composition
- **WHEN** Fuego is tested within cells across all masks/rows and across its first bit boundary
- **THEN** within-cell behavior agrees to floating-point tolerance and the adjacent-float jump remains observable
- Check: `fuego_preserves_within_cell_errors_across_every_mask_and_supported_row`
- Check: `fuego_first_bit_boundary_amplifies_an_adjacent_float_as_the_model_predicts`
