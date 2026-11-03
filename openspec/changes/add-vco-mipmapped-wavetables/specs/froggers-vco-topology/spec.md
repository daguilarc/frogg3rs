## ADDED Requirements

### Requirement: Shared frequency-adaptive waveform source
The VCO SHALL reuse Sheaf AdaptiveWavetable generation and evaluation for saw and square, with one immutable shared table bank prepared outside audio processing. The sine endpoint SHALL retain the existing sine evaluation. All six VCO instances SHALL use the same waveform implementation without sharing mutable phase or filter state.

#### Scenario: Shape retains its meaning
- **WHEN** Shape is 0, 0.5 or 1
- **THEN** the source is respectively the existing sine, a mipmapped ascending saw, or a mipmapped square
- **THEN** adjacent endpoints crossfade over their existing halves of the control range
- Delivery: not yet implemented; tasks.md task 3 will supply runtime coverage.

#### Scenario: Table selection follows carrier pitch
- **WHEN** carrier frequency or host sample rate changes
- **THEN** table selection receives that carrier frequency and half of the 4× internal sample rate as its frequency ceiling
- **THEN** Sheaf's level selection and interpolation are used without a new selection algorithm
- Delivery: not yet implemented; tasks.md task 3 will supply runtime coverage. This does not assert a strict spectral cutoff under interpolation or modulation.

#### Scenario: Processing starts with prepared data
- **WHEN** any supported host or a standalone modulation source starts processing
- **THEN** waveform tables are already prepared and Process only reads them
- **THEN** generating tables, allocating storage and first-use initialization do not occur in Process
- Delivery: not yet implemented; tasks.md tasks 1 and 3 will verify initialization reachability and runtime behavior.

### Requirement: Mipmapping supplements complete oscillator oversampling
The oscillator SHALL retain four PM/waveform/ring substeps followed by the existing 4×→2×→1× halfband FIR cascade. Carrier/PM/ring phase advance ordering, pitch mapping, defaults, modulation floors, reset and recovery SHALL remain as specified by the existing instrument.

#### Scenario: Modulation remains inside oversampling
- **WHEN** PM and ring modulation are active
- **THEN** mipmapped waveform lookup occurs at the PM-offset phase before ring multiplication within each substep
- **THEN** the complete result passes through both existing halfband stages
- Delivery: not yet implemented; tasks.md task 3 will adapt the independent convolution reference to the new waveform source.
