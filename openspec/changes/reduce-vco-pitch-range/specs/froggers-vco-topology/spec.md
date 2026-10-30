## MODIFIED Requirements

### Requirement: Froggers oscillator topology is preserved
The app SHALL implement the Froggers three-oscillator topology: per-VCO pitch on a continuous exponential map spanning 27.5 Hz (A0) to 1760 Hz (A6), exactly six octaves; a continuous waveform **Shape** morph crossfading sine to saw over the lower half of its range and saw to square over the upper half; and per-VCO phase modulation driven by that VCO's **own** dedicated sine LFO whose frequency is an exponential function of a phase-modulation **rate** control. Each VCO SHALL keep its own LFO instance and its own phase-modulation depth control; the rate control MAY be a single control shared by all three LFOs. The registered pitch defaults SHALL map to 110 Hz, 220 Hz and 330 Hz for VCO1, VCO2 and VCO3 respectively.

#### Scenario: Shape morph sweeps continuously
- **WHEN** a VCO's Shape control is swept from minimum to maximum
- **THEN** the waveform morphs continuously from sine through saw to square without discontinuity

#### Scenario: Phase-modulation depth is self-contained
- **WHEN** any VCO's phase-modulation depth control is raised
- **THEN** only that VCO's phase is modulated
- **THEN** no other VCO's output changes as a result

#### Scenario: The phase-modulation rate control is shared by design
- **WHEN** the shared phase-modulation rate control is changed
- **THEN** every VCO whose own phase-modulation depth is above zero changes its LFO rate together
- **THEN** a VCO whose own phase-modulation depth is at zero stays unmodulated, unaffected by the rate

#### Scenario: Pitch covers six octaves continuously
- **WHEN** the normalized pitch control is set at each sixth of its travel with phase and ring modulation off
- **THEN** the pitch mapping and running oscillator advance at 27.5, 55, 110, 220, 440, 880 and 1760 Hz respectively
- **THEN** this pitch mapping is the same at sample rates of 44.1, 48 and 96 kHz
- Check: `app/FroggersDspParityTests.cpp`, `vco_pitch_exp_map_matches_named_range`.

#### Scenario: Registered default pitches survive reset
- **WHEN** the app registers its three pitch parameters or restores their registered defaults
- **THEN** their scene centers map to 110, 220 and 330 Hz respectively
- Check: `app/FroggersParameterModelTests.cpp`, `vco_registered_pitch_defaults_preserve_110_220_330_hz`.
