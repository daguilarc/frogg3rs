# external-ring-mod-mix Specification

## Purpose

Normative external-input gate and parallel ring-mod mix formula in the shared `FroggersEngine` across all hosts.
## Requirements
### Requirement: External gate selects VCO-only vs ring mod

`FroggersEngine` SHALL use the external-input Schmidt gate (`m_extGate` on
smoothed `|extIn|`) to choose the pre-drive mix. Both variants share the
gate-closed behaviour and differ only when the gate is open.

This requirement governs the Daisy Field firmware's `FroggersEngine`, which is
the only implementation of external-signal ring modulation. The Sheaf app's
hosts have per-VCO ring modulation from an internal carrier, which is a separate
mechanism and is not governed here.

- Gate **closed**, both variants: `OLVL × average(VCO1, VCO2, VCO3)`.
- Gate **open**, Froggers Solo: parallel ring mod alone.
- Gate **open**, Froggers Guitar: the dry external signal summed with the
  parallel ring mod, weighted 7:5.

FUEG/Crispy SHALL NOT influence the external mix in either variant.

#### Scenario: Silent external input, either variant
- **WHEN** smoothed external level is below the gate close threshold
- **THEN** output is VCO-only at `OLVL` and the external sample is not
  multiplied into the chain input

#### Scenario: Guitar with an active external input
- **WHEN** the gate is open on a Froggers Guitar build
- **THEN** the pre-drive mix is
  `(7/12) × extIn + (5/12) × ((extIn×VCO1 + extIn×VCO2 + extIn×VCO3) / 3)`
- **AND** the summed weights are 1, so the level does not rise above what Solo
  produces at the same knobs

#### Scenario: Solo is unchanged by the Guitar variant
- **WHEN** the gate is open on a Froggers Solo build
- **THEN** the pre-drive mix is `(extIn×VCO1 + extIn×VCO2 + extIn×VCO3) / 3`
  with no dry term

### Requirement: Parallel ring mod formula

When `hasExternal` is true, `MixExternalAndOsc` SHALL return:

`(extIn × VCO1 + extIn × VCO2 + extIn × VCO3) / 3`

There SHALL be no product term `extIn × VCO1 × VCO2 × VCO3` and no blend/morph between mix shapes.

#### Scenario: All hosts share one formula

- **WHEN** any host (Daisy Field, desktop, web WASM) processes a sample with external gate open
- **THEN** the pre-drive mix matches the parallel formula above

#### Scenario: FUEG knob position with external present

- **WHEN** external gate is open and Audio knob 8 is at any position
- **THEN** pre-drive mix is unchanged by knob 8; knob 8 still affects fuegoizer (and PM3 per host rules) only

