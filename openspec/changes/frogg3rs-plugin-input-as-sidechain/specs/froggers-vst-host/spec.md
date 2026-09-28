# Delta — `froggers-vst-host`

"Bus and MIDI posture match the core's real I/O" states the input bus's
presence, channel layout, and opt-in consent semantics, but nowhere states
how the bus is categorized to a VST3 host. This adds that.

## ADDED Requirements

### Requirement: The input bus presents as an aux, not main, to a VST3 host
THE plugin SHALL report its optional audio input bus to a VST3 host as an
auxiliary (sidechain) bus, never as the host's designated main input, so
that a host offering audio to an instrument only through an aux/sidechain
bus can route a track's output into it. This changes only how the bus is
categorized to the host; the bus's presence, channel layout, default-off
state, and opt-in consent semantics are unchanged.

#### Scenario: The host queries the input bus's designation
- **WHEN** a VST3 host queries bus info for the plugin's input bus at index 0
- **THEN** the reported bus type is aux, not main
- Check: NEW test in app/vst/FroggersVstHostTests.cpp asserting the
  processor's VST3 client extensions exist and report no main input (task 2)

#### Scenario: A track routes into the plugin through a sidechain input
- **WHEN** the operator, in Ableton Live 12 or Bitwig, routes another
  track's output into the plugin's input bus
- **THEN** the bus is offered and selectable as a sidechain/aux routing
  target, and the routed signal reaches External Audio and External EF once
  the operator opts the input in
- Check: not yet delivered, pending the operator's Live/Bitwig run (task 3)
