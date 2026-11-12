## ADDED Requirements

### Requirement: Fixed six-section converter at both rate boundaries
The Sheaf bridge SHALL filter every modulation source at engine rate before
matrix-rate evaluation, and SHALL filter every interpolated root before DSP
publication, with one fixed unity-DC order-11 elliptic coefficient table of six
second-order sections. The response SHALL be within ±0.1 dB from 0 to 5/48
engine cycles/sample and at least 90 dB down from 1/8 to 1/2 (0–20 kHz and
24–96 kHz at a 192 kHz engine). The 20–24 kHz transition band is not covered.
Each voice/modulator source and each voice/parameter root SHALL own an
independent filter history, initialized at that section's DC equilibrium.

#### Scenario: Source path rejects the whole stop band through the bridge
- **WHEN** paired-phase sines from 125 Hz to 96 kHz drive a source through the bridge
- **THEN** 125 Hz–20 kHz pass within ±0.1 dB and every probe from 24 kHz to 96 kHz is at least 90 dB down
- Check: `rate_separated_source_seam_paired_pass_stop_probes`

#### Scenario: Source histories are independent and start at DC
- **WHEN** two voices × two modulators carry different moving sources
- **THEN** each published source equals its own independently replayed filter, bit for bit, with no startup dip
- Check: `rate_separated_source_histories_independent_dc_started_and_frozen_while_held`

#### Scenario: Interpolated roots reject images
- **WHEN** roots move at 125 Hz–20 kHz through four-phase interpolation and the root filter
- **THEN** the baseband is preserved and each image at 48k−f, 48k+f and 96k−f is at most −90 dBc
- Check: `rate_separated_root_filtered_foh_preserves_baseband_rejects_images`

#### Scenario: Batch roots match the scalar filter
- **WHEN** 0, 1, 4, 15 and 91 root lanes are filtered as a batch
- **THEN** every lane equals an independent scalar biquad cascade exactly
- Check: `rate_conversion_batch_matches_independent_biquad_oracle`

### Requirement: Source histories freeze while the transport is stopped
While sources are held, the bridge SHALL keep publishing the last filtered
source value and SHALL NOT advance source filter histories; on resume the
history SHALL continue from where it stopped.

#### Scenario: Held sources resume from the frozen history
- **WHEN** sources keep moving during a 1024-sample hold and then resume
- **THEN** published values stay fixed during the hold and match a replay that skipped the held inputs afterwards
- Check: `rate_separated_source_histories_independent_dc_started_and_frozen_while_held`

### Requirement: Exact static publication after settlement
A root's unchanged-target age SHALL reset on every exact phase-zero target
change and on Configure, increment before eligibility, and saturate at 4096.
Only at age ≥ 4096 and |filtered − target| ≤ 2e-5f SHALL the exact target be
published; otherwise the clamped filtered value SHALL be published. Filter
histories SHALL always advance without feedback from this correction.

#### Scenario: Static targets land exactly at 4096
- **WHEN** a static root steps between targets in [0,1]
- **THEN** sample 4095 publishes the exact target, earlier samples publish the clamped filter output, and Configure resets the age
- Check: `rate_separated_exact_settlement_boundary_reset_and_live_history`
- Check: `rate_separated_exact_settlement_finite_target_sweep`

#### Scenario: Tiny modulation never snaps
- **WHEN** the target changes by one ulp every matrix frame
- **THEN** publication stays the clamped filter output
- Check: `rate_separated_tiny_target_changes_revoke_settlement`

### Requirement: Intentional reducers are unchanged
SRR1, SRR2 and feedback-only Delay Crush SHALL keep their algorithms, mappings
and processing positions; the converter SHALL NOT be applied inside them.

#### Scenario: Reducer code is untouched
- **WHEN** this change is diffed against its base
- **THEN** no SRR, Delay Crush or randomization source file differs
- Check: operator step — `git diff` over app/dsp and the randomization sources is empty

### Requirement: Certificate bound to the shipped coefficients and constants
The formal gate SHALL certify the exported binary32 coefficients over the
closed pass and stop bands, pin every audited theorem statement in
`formal/requirements.json`, refuse proof escape hatches anywhere in the model,
and fail when the C++ settlement constants differ from the Lean publication
law. It SHALL NOT be presented as a proof of the compiled C++ path; the native
tests above are the guard for executed behavior.

#### Scenario: CI runs both gates on every Sheaf pin change
- **WHEN** a commit changes the Sheaf submodule pin
- **THEN** CI builds and runs the native parameter and DSP tests and runs `make formal-test`
- Check: none — enforced by the CI workflow .github/workflows/parameter-model.yml, not by a test case
