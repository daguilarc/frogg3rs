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
The reusable Sheaf bridge SHALL low-pass source samples before matrix-rate evaluation, retain source-filter history across callbacks, interpolate root targets at weights 1/4, 1/2, 3/4 and 1, and pass each interpolated root through the six-section root low-pass before publication (change `conversion-alias-rejection`). The application seam SHALL be applied once before interpolation, for every configured voice.

#### Scenario: Source alias rejection
- **WHEN** a 1 kHz source and a 47 kHz source are evaluated at 192/48 kHz
- **THEN** the 1 kHz source remains present and the 47 kHz residual is below one thousandth of its RMS
- Check: `rate_separated_sources_reject_aliases_before_matrix_evaluation`

#### Scenario: Processed multivoice publication
- **WHEN** two voices receive distinct post-seam values
- **THEN** each voice publishes its own four-step interpolation, filtered by its own root low-pass history, and the seam runs once
- Check: `rate_separated_parameters_interpolate_post_seam_once_per_frame`

### Requirement: Explicit formal correspondence boundary
The Lean model SHALL describe cadence, publication and Sheaf-specific behavioral extensions independently of runtime evaluation strategy. Its gate SHALL kernel-check theorems, reject unapproved axioms and report changes to reviewed source fingerprints. Rational model proofs SHALL NOT be presented as verification of C++ floating-point behavior.

#### Scenario: Formal gate rejects drift
- **WHEN** a reviewed source, coefficient, theorem statement or settlement constant changes without a reviewed manifest update
- **THEN** `make formal-test` fails
- Check: none — the formal gate runs in the CI workflow .github/workflows/parameter-model.yml, not as a test case

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
The formal gate SHALL export and check all 30 configured binary32 coefficients
of the six-section converter against their exact real embeddings in Lean.
The complete closed normalized pass band [0,5/48] SHALL have power gain in
[981/1000,10001/10000]; the stop band [1/8,1/2] SHALL have power gain at most
981/1000000000000. Every stop/pass pair SHALL have power ratio at most
1/1000000000. At a 192 kHz engine these are 0..20 kHz and 24..96 kHz; other
engine rates scale the physical edges. The model SHALL prove strict denominator
positivity, all six product identities, complete interval coverage and closed
trigonometric endpoints, retaining the 47 kHz to 1 kHz sampling identity.
These claims SHALL describe steady-state frequency response; they SHALL NOT be
presented as IEEE processing-error bounds, transient guarantees, transition-band
suppression, output-FIR proofs or whole-synth alias freedom.

#### Scenario: Native response agrees with proved band bounds
- **WHEN** the configured C++ six-section source filter is checked across the pass and stop bands
- **THEN** native response meets its numeric band criteria, separately from the continuous kernel proof
- Check: `rate_separated_source_filter_dense_response_band_bounds`

### Requirement: Stateful finite-arithmetic and nonlinear composition contracts
The formal model SHALL retain full direct-form-I biquad memory identities and
generic positive-energy, initial-history decay and disturbance theorems conditional
on FilterState.Certificate. It SHALL NOT claim actual-six-section stability
instances or describe runtime DC-equilibrium Reset as 512-sample priming. It SHALL model nearest-even binary32 and
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
- **WHEN** actual source-filter DC initialization, endpoint settling and guarded rounding residuals are exercised
- **THEN** native initialization/settling criteria and local residual bounds pass, without claiming the generic conditional state budget applies to the actual cascade
- Check: `source_filter_dc_equilibrium_and_settled_endpoints`
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
