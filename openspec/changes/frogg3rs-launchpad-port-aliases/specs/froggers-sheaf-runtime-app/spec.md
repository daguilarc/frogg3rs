# Delta — `froggers-sheaf-runtime-app`

## ADDED Requirements

### Requirement: A preset's port aliases are per direction and name what the host reports

Each device preset this application offers SHALL carry its input aliases and its output aliases separately, and each list SHALL include the endpoint name the host reports for that direction, because the page pairs a connected unit with a preset by whole-string, case-insensitive equality against that name. Where a unit's two endpoints are named differently — a device whose ports carry a direction word, so that the input reads "Out" and the output reads "In" — the two lists SHALL differ accordingly. A preset MAY additionally carry shorter forms for hosts that report them, and SHALL NOT carry an alias that would pair the unit's DAW port when its MIDI port is the one the preset drives.

#### Scenario: A connected Launchpad is offered its own preset

- **WHEN** a Launchpad Mini MK3 is connected and the Controllers page enumerates its ports as "Launchpad Mini MK3 LPMiniMK3 MIDI Out" and "Launchpad Mini MK3 LPMiniMK3 MIDI In"
- **THEN** the page pairs it with the Launchpad Mini MK3 preset
- **AND** the unit's DAW ports pair with no preset
- Check: `app/FroggersControllersPageTests.cpp`,
  `launchpad_presets_pair_with_the_port_names_a_host_reports`.

#### Scenario: The name a host reports is read from the host

- **WHEN** an alias is written for a unit
- **THEN** it is the name the host enumerates, read from that host rather than
  constructed from a manual
- Check: `app/FroggersControllersPageTests.cpp`, the same case; the Mini MK3's
  rows of its table were read by calling `getAvailableDevices()` through this
  application's own JUCE with the unit connected, the other two models' rows
  are the same construction and are unconfirmed.
