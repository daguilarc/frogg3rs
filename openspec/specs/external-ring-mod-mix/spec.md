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

The ring-mod term SHALL be `(extIn × VCO1 + extIn × VCO2 + extIn × VCO3) / 3`
in both variants. There SHALL be no product term
`extIn × VCO1 × VCO2 × VCO3` and no blend or morph between mix shapes.

The ring-mod term SHALL NOT route through `MixOscVoices`.

Guitar's dry term and its ring-mod term SHALL enter the same chain as one summed
input, not as two chain instances. The chain is nonlinear, so the terms interact;
that is the specified behaviour.

#### Scenario: One chain, not two
- **WHEN** a Guitar build processes a sample with the gate open
- **THEN** exactly one `FrogBlock` and one output-FX pass run for that sample

