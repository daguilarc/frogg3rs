# Delta — `froggers-vst-host`

Two plugin behaviours that reading could not settle. `prepareToPlay` pushes a
transport start onto a bus written for one producer, which is safe only if every
host calls it on the message thread. Knob drags in the plugin window reach the
host as value changes with no begin or end gesture, which the manual's
MIDI-learn route may depend on. Each is settled by an operator run in named
hosts; the code changes only where the run confirms the defect.

## ADDED Requirements

### Requirement: The plugin's transport start reaches the UI bus from one producer
WHEN a host calls `prepareToPlay` on any thread, THE plugin SHALL deliver the transport start it posts without a second thread writing the UI bus at the same time as the plugin's timer, so that no bus message is lost or corrupted.

#### Scenario: prepareToPlay on the hosts the plugin supports
- **WHEN** the plugin is loaded and played in Logic, Ableton Live and Reaper
- **THEN** either every `prepareToPlay` call arrives on the message thread, or the plugin's post from `prepareToPlay` is routed so the timer thread stays the bus's only writer
- Check: operator step: the change's thread-id run in the three hosts

### Requirement: A knob drag in the plugin window reaches the host as one gesture
WHEN the player drags a knob in the plugin window, THE plugin SHALL report the drag to the host so that the host's MIDI-learn lists the parameter and its touch automation records the drag from its start to its end, which the manual names as the way to map a plugin control.

#### Scenario: MIDI-learn and touch automation see a drag
- **WHEN** one encoder is dragged in the plugin window in Ableton Live's MIDI Map mode, and in Logic with the AU in Touch mode
- **THEN** Live lists the parameter, and Logic writes automation for the length of the drag
- Check: operator step: the change's host run in Live and Logic
