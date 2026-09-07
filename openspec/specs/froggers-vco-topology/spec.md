# froggers-vco-topology Specification

## Purpose
The Froggers three-oscillator topology (exponential pitch, continuous Shape morph, self-contained phase modulation) with no hardcoded cross-VCO coupling, exposing Sheaf's VCO scope UI-state and envelope followers, and gated by the master clock's quarter-note pulse so pitch and amplitude gating stay independent.
## Requirements
### Requirement: Froggers oscillator topology is preserved
The app SHALL implement the Froggers three-oscillator topology: per-VCO pitch on an exponential map spanning roughly 20 Hz to 20 kHz; a continuous waveform **Shape** morph crossfading sine to saw over the lower half of its range and saw to square over the upper half; and per-VCO phase modulation driven by that VCO's **own** dedicated sine LFO whose frequency is an exponential function of a phase-modulation **rate** control. Each VCO SHALL keep its own LFO instance and its own phase-modulation depth control; the rate control MAY be a single control shared by all three LFOs.

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

### Requirement: No hardcoded cross-VCO coupling
The oscillator section SHALL contain no hardcoded VCO-to-VCO coupling terms. All inter-oscillator routing SHALL be expressed only through the modulation matrix.

#### Scenario: Cross-VCO routing requires an explicit assignment
- **WHEN** no modulation assignment links two VCOs
- **THEN** changing one VCO's parameters does not alter another VCO's output

#### Scenario: Default patch ships ordinary modulation assignments, not topology
- **WHEN** the app starts for the first time
- **THEN** the initial patch includes cross-oscillator modulation assignments at minimal depth
- **THEN** those assignments are ordinary modulation-matrix entries the operator can remove like any other
- **THEN** removing them restores full oscillator independence, with changing one oscillator no longer altering another

#### Scenario: Ring-modulation carriers are internal to their own VCO
- **WHEN** a VCO's ring modulator is processing
- **THEN** its carrier is an oscillator generated inside that VCO's own ring-mod stage
- **THEN** no other VCO's signal is read by that stage, so ring modulation adds no inter-oscillator routing

### Requirement: Phase modulation has a true zero position
The phase-modulation control SHALL be fully inert at its minimum position, with a smooth ramp from that floor into its active range. Every other per-VCO modulation amount that carries a zero position — ring-modulation depth today — SHALL behave the same way, and all of them SHALL derive that ramp from one shared function rather than from a per-control copy of it, each passing its own floor and ramp width so one control can be tuned without changing another's behaviour.

#### Scenario: Minimum position is silent modulation
- **WHEN** a VCO's phase-modulation control is at minimum
- **THEN** that VCO's phase receives zero modulation depth

#### Scenario: Ring modulation is inert at the bottom of its own control
- **WHEN** a VCO's ring-modulation control is at or below its own zero floor
- **THEN** that VCO's signal passes through its ring-mod stage unchanged, at any carrier frequency
- **THEN** raising the control past the floor ramps the ring-modulation amount up smoothly, with no step

#### Scenario: One ramp function serves every such control
- **WHEN** the phase-modulation and ring-modulation depth ramps are computed
- **THEN** both call the same shared ramp function, given their own floor and ramp width
- **THEN** the phase-modulation control's own behaviour is unchanged from before that function was shared

### Requirement: Oscillators expose Sheaf scope state
Each VCO SHALL expose the oscillator UI-state shape Sheaf's scope visualizer consumes — connection flag, scope writer reference, scope channel, and scope color — and SHALL accept a scope writer holder and color, publishing its UI state each block. The app SHALL NOT implement its own waveform drawing for VCO scopes.

#### Scenario: Scope visualizer binds without app drawing code
- **WHEN** the surface is built with VCO scope panels
- **THEN** each panel is driven by the standard Sheaf scope visualizer over the VCO's UI state
- **THEN** no bespoke waveform rasterization exists in the app

#### Scenario: Scope reports connected after processing
- **WHEN** the app has processed at least one block
- **THEN** each VCO's scope state reports connected
- **THEN** the panel renders a live waveform trace

### Requirement: Envelope followers on each oscillator
The app SHALL provide an envelope follower per VCO with fast attack and release characteristics matching the Froggers follower, feeding the three VCO envelope-follower modulation sources.

#### Scenario: Follower tracks oscillator level
- **WHEN** a VCO's output level rises and then falls
- **THEN** its envelope follower value rises quickly and decays more slowly
- **THEN** that value is available as a modulation source

### Requirement: The transport pulse gates the instrument; pitch stays on the pitch controls
The running transport's quarter-note pulse SHALL be the sole trigger for sound: while the transport runs, the amplitude envelope opens and closes in time with the quarter-note pulse. Oscillator pitch SHALL come solely from the pitch controls — there is no other source of transposition. While the transport is stopped, the instrument SHALL be silent regardless of any other parameter setting.

#### Scenario: A running transport gates the amplitude envelope
- **WHEN** the transport is running
- **THEN** the amplitude envelope opens and closes in time with the transport's quarter-note pulse
- **THEN** oscillator pitch is determined solely by the pitch controls, unaffected by the pulse

#### Scenario: Transport stopped is silence
- **WHEN** the transport is not running
- **THEN** the instrument produces no audible output, regardless of any other parameter setting

### Requirement: The PM rate control's minimum is a moving rate, not a second off switch
THE shared phase-modulation rate control SHALL map its minimum position to a slow but plainly audible modulation rate, above zero — a cycle completing within a few seconds, not a near-static drift. The size of the pitch swing the modulation produces SHALL be set by the per-VCO depth controls alone and SHALL be the same at every position of the rate control, so that the rate control changes only how fast the swing runs. Turning phase modulation off SHALL be exclusively the per-VCO depth controls' job, which already provide a true zero; no position of the rate control SHALL silence or effectively freeze the modulation while any depth is nonzero.

#### Scenario: Minimum rate still audibly cycles
- **WHEN** the PM rate control is at its minimum and any VCO's PM depth is nonzero
- **THEN** that VCO's phase modulation audibly cycles, completing a full period within a few seconds

#### Scenario: The pitch deviation is the same at every rate
- **WHEN** the peak pitch deviation of a VCO's phase modulation is measured with the rate control at its minimum, its middle and its maximum, at one depth setting
- **THEN** the three measurements are equal within a few percent

#### Scenario: Off lives on the depth controls alone
- **WHEN** every VCO's PM depth control is at minimum
- **THEN** no phase modulation is applied, at any position of the rate control

#### Scenario: The floor is a real rate, not zero
- **WHEN** the rate control moves from its minimum toward maximum
- **THEN** the modulation rate rises monotonically from the nonzero floor to the maximum rate, with no dead zone at the bottom

### Requirement: Envelope times map exponentially
THE attack, decay and release controls SHALL map their position to time
exponentially, matching every other time and frequency control in the
instrument, so that equal movements of the control produce equal ratios of
time rather than equal differences. Their minimum positions SHALL sit at a
short but non-zero time, so that a control at minimum is fast rather than
instantaneous. The Grace control SHALL map exponentially too, through a
mapping that reaches exact zero at its minimum, because no minimum hold is a
real setting: it needs the exponential travel AND the true zero, not a choice
between them. (Amended: this required Grace to stay LINEAR, on the ground that
an exponential mapping cannot reach zero. That holds for the floored mapping
the other three use, not for exponential mappings generally -- a zeroed
exponential is exact at both ends, and the instrument already used one for the
sample-rate reducers. Linear spent the bottom twentieth of Grace's travel on
its entire useful short-hold range.)

#### Scenario: Equal movements give equal ratios
- **WHEN** the attack control is moved from its minimum to its midpoint, and
  again from its midpoint to its maximum
- **THEN** each movement multiplies the attack time by the same ratio

#### Scenario: A randomized envelope keeps its transient
- **WHEN** envelope times are randomized repeatedly
- **THEN** most draws produce a fast attack, and long attacks are the
  minority rather than half the draws

#### Scenario: Grace still reaches zero
- **WHEN** the Grace control is at its minimum
- **THEN** there is no minimum hold at all

### Requirement: Control bounds stay inside the useful range
EVERY control's bounds SHALL sit where the control still does something
audible. A control at either extreme SHALL produce a usable setting rather
than a silent or inert one, so that randomizing a control explores its
character instead of disabling it. Controls whose zero position is a real
setting — phase-modulation depth, ring-modulation depth, peak gain, fold,
scoop depth and Grace — SHALL keep reaching true zero; turning an effect off
belongs to those controls alone.

#### Scenario: A randomized patch stays audible
- **WHEN** every parameter is randomized
- **THEN** the instrument sounds at a usable level, without a sustain so low
  or an envelope so slow that notes disappear

#### Scenario: A randomized effect is audibly present
- **WHEN** the resonant peak, the scoop or the comb has its frequency
  randomized
- **THEN** that effect lands within the audible range and is heard working,
  rather than sitting below hearing where it does nothing

#### Scenario: Off is still reachable where off is meaningful
- **WHEN** phase-modulation depth, ring-modulation depth, peak gain, fold or
  scoop depth is at its minimum
- **THEN** that effect contributes nothing at all

