# Delta — `froggers-vst-host`

"Tempo is external via the DAW" states the BPM control is unconditionally
suppressed whenever the plugin runs. The shipped code (`hostTempoUsable`,
`FroggersPluginProcessor.cpp`) only suppresses it while the host reports a
usable tempo; when the host reports none, the BPM control is editable, the
same as standalone with no external clock.

## MODIFIED Requirements

### Requirement: Tempo is external via the DAW
WHEN hosted as a plugin and the host reports a usable tempo, THE master clock SHALL follow the host tempo
through the core's existing external-clock slaving, and the BPM control
SHALL behave exactly as it does when slaved to external MIDI clock:
display-direction only, with user tempo requests suppressed. WHEN hosted as
a plugin and the host reports no usable tempo, THE BPM control SHALL stay
editable, the same as standalone with no external clock.

<!-- RESTATES-EXCEPT
the DAW tempo changes while the plugin runs
  keeps: the DAW tempo changes while the plugin runs
-->

#### Scenario: Host tempo drives the clock
- **WHEN** the DAW tempo changes while the plugin runs and the host reports a usable tempo
- **THEN** the instrument's clock follows the host tempo
- **THEN** the BPM control displays the host tempo and does not accept a
  user tempo change

#### Scenario: No usable host tempo leaves BPM editable
- **WHEN** the plugin runs and the host reports no usable tempo (no valid playhead tempo, or the host has
  stopped calling `processBlock`)
- **THEN** the instrument's clock is not slaved to the host
- **THEN** the BPM control accepts a user tempo change, the same as standalone with no external clock
