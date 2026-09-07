# mod-rack-dual-midi-jacks Specification

## Purpose
Require desktop standalone hardware MIDI to expose two independent CC-to-mod CV jacks matching the five-cell mod-rack layout.
## Requirements
### Requirement: Mod rack row shows MIDI CC 1 and MIDI CC 2 as adjacent columns

The mod rack SHALL display **MIDI CC 1** as the leftmost column (renamed from MIDI) and **MIDI CC 2** as the column immediately to its right. Both columns SHALL use the same box dimensions, vertical alignment, and inter-column gap as VCO Envelope and Random columns. Each column SHALL expose one scope trace and one patch output jack.

#### Scenario: Five-column row alignment

- **WHEN** the user views the mod rack
- **THEN** the row order left-to-right is MIDI CC 1, MIDI CC 2, VCO Envelope, Random 1 S&H, Random 2 S&H, all boxes top-aligned and evenly spaced

#### Scenario: MIDI CC 1 drives mod zero jack

- **WHEN** MIDI CC 1 input receives value 127 and the user patches the MIDI CC 1 jack
- **THEN** the patched destination receives full mod depth from `mods[0]`

#### Scenario: MIDI CC 2 drives mod one jack

- **WHEN** MIDI CC 2 input receives value 64 and the user patches the MIDI CC 2 jack
- **THEN** the patched destination receives mod CV proportional to `mods[1]`

### Requirement: MIDI Settings mirrors two inputs

MIDI Settings SHALL show **MIDI CC 1** and **MIDI CC 2** as adjacent groups on one row under the MIDI In device selector. Each group SHALL have Channel and CC controls.

#### Scenario: Settings row matches mod rack inputs

- **WHEN** the user opens MIDI Settings
- **THEN** MIDI CC 1 and MIDI CC 2 each have independent Channel and CC number controls on the same row

