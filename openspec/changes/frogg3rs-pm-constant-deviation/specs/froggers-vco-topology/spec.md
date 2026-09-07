# Delta — `froggers-vco-topology`

## MODIFIED Requirements

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
