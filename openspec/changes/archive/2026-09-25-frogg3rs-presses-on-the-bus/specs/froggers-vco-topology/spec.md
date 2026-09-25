# Delta — `froggers-vco-topology`

Four sentences the audit found stale against the shipped code: the pitch
ceiling, Fold's place in the true-zero control list, Freeze's exception to
"transport stopped is silence", and Ring Mod's own control being stated as a
plain depth control when it also sweeps its carrier's frequency.

## MODIFIED Requirements

### Requirement: Froggers oscillator topology is preserved
The app SHALL implement the Froggers three-oscillator topology: per-VCO pitch on an exponential map spanning roughly 20 Hz to 5 kHz; a continuous waveform **Shape** morph crossfading sine to saw over the lower half of its range and saw to square over the upper half; and per-VCO phase modulation driven by that VCO's **own** dedicated sine LFO whose frequency is an exponential function of a phase-modulation **rate** control. Each VCO SHALL keep its own LFO instance and its own phase-modulation depth control; the rate control MAY be a single control shared by all three LFOs.

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

### Requirement: Phase modulation has a true zero position
The phase-modulation control SHALL be fully inert at its minimum position, with a smooth ramp from that floor into its active range. Ring-modulation depth SHALL behave the same way at its own floor, using the same shared ramp function, given its own floor and ramp width — but unlike phase-modulation depth, which shares one rate control across all three VCOs while each VCO's own depth is a pure amount, the Ring Mod control is not a pure amount: the same per-VCO control that ramps the ring-modulated amount in from its zero floor also sweeps that VCO's own ring-mod carrier frequency across its audio-rate range, so raising Ring Mod changes the carrier and the amount together, not the amount alone.

<!-- RESTATES-EXCEPT
at any carrier frequency
  keeps: that VCO's signal passes through its ring-mod stage unchanged
with no step
  keeps: raising the control past the floor ramps the ring-modulation amount up smoothly
-->

#### Scenario: Minimum position is silent modulation
- **WHEN** a VCO's phase-modulation control is at minimum
- **THEN** that VCO's phase receives zero modulation depth

#### Scenario: Ring modulation is inert at the bottom of its own control
- **WHEN** a VCO's ring-modulation control is at or below its own zero floor
- **THEN** that VCO's signal passes through its ring-mod stage unchanged, whatever carrier frequency the control would otherwise be sweeping toward
- **THEN** raising the control past the floor ramps the ring-modulation amount up smoothly, with no step, while its own carrier frequency sweeps upward across the same travel

#### Scenario: One ramp function serves every such control
- **WHEN** the phase-modulation and ring-modulation depth ramps are computed
- **THEN** both call the same shared ramp function, given their own floor and ramp width
- **THEN** the phase-modulation control's own behaviour is unchanged from before that function was shared

### Requirement: The transport pulse gates the instrument; pitch stays on the pitch controls
The running transport's quarter-note pulse SHALL be the sole trigger for sound: while the transport runs, the amplitude envelope opens and closes in time with the quarter-note pulse. Oscillator pitch SHALL come solely from the pitch controls — there is no other source of transposition. While the transport is stopped and Freeze is not latched, the instrument SHALL be silent regardless of any other parameter setting; while Freeze is latched, the amplitude envelope SHALL stay open and the instrument SHALL keep sounding whether or not the transport is running.

#### Scenario: A running transport gates the amplitude envelope
- **WHEN** the transport is running
- **THEN** the amplitude envelope opens and closes in time with the transport's quarter-note pulse
- **THEN** oscillator pitch is determined solely by the pitch controls, unaffected by the pulse

#### Scenario: Transport stopped is silence, unless Freeze is latched
- **WHEN** the transport is not running and Freeze is not latched
- **THEN** the instrument produces no audible output, regardless of any other parameter setting
- **WHEN** the transport is not running and Freeze is latched
- **THEN** the amplitude envelope stays open and the instrument keeps sounding

### Requirement: Control bounds stay inside the useful range
EVERY control's bounds SHALL sit where the control still does something
audible. A control at either extreme SHALL produce a usable setting rather
than a silent or inert one, so that randomizing a control explores its
character instead of disabling it. Controls whose zero position is a real
setting — phase-modulation depth, ring-modulation depth, peak gain,
scoop depth and Grace — SHALL keep reaching true zero; turning an effect off
belongs to those controls alone. Fold has no such position: its travel maps
exponentially between two nonzero divisors, so it always folds by some
nonzero amount and cannot be turned off.

<!-- RESTATES-EXCEPT
peak gain, fold or scoop depth is at its minimum
  keeps: phase-modulation depth, ring-modulation depth
-->

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
- **WHEN** phase-modulation depth, ring-modulation depth, peak gain or
  scoop depth is at its minimum
- **THEN** that effect contributes nothing at all
