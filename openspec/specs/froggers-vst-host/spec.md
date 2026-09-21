# froggers-vst-host Specification

## Purpose
A JUCE plugin host (VST3 and AU) for the Sheaf-hosted app whose entire DAW-facing surface — transport, tempo, parameters, and editor chrome — is external by design: the DAW is the authority for everything a DAW touches, and the app core stays JUCE-free.
## Requirements
### Requirement: Plugin host wraps the JUCE-free core
THE VST host SHALL be a JUCE audio plugin (VST3 and AU, instrument) that
wraps the app core without modifying it: all JUCE code SHALL live in the
plugin host layer, and the core's no-JUCE gate SHALL remain in force and
green, unmodified.

#### Scenario: Core stays JUCE-free
- **WHEN** the app test suite runs after the plugin host lands
- **THEN** the no-JUCE gate passes with no changes to the gate or the core

### Requirement: Transport is external via the DAW
WHEN hosted as a plugin, THE transport SHALL be driven exclusively by the
DAW: host play-state transitions from the plugin playhead SHALL produce the
same transport messages the standalone UI produces, edge-triggered (exactly
one start or stop per host transition), and the plugin editor SHALL NOT
render internal transport controls — Play, Stop, and Record (recording is
the DAW's job: the host records the plugin's output natively). Freeze is a
musical control, not transport: it SHALL remain available as an automatable
parameter with the standalone latch semantics preserved, its surface button
SHALL stay rendered, and it SHALL gain a "FREEZE" text label beside it in
the row space the suppressed controls free (operator instruction
2026-08-18).

#### Scenario: Host play drives the instrument
- **WHEN** the DAW transport starts and later stops
- **THEN** the instrument starts and stops with it, one transport message
  per transition, with no internal play/stop control involved

#### Scenario: Freeze automatable, not transport-coupled
- **WHEN** the DAW automates the Freeze parameter while its transport runs
- **THEN** the freeze latch engages and releases per the standalone
  semantics, independent of the host transport state

#### Scenario: Freeze button labeled in the thinned row
- **WHEN** the plugin editor renders the transport row
- **THEN** Play, Stop, and Record are absent, the Freeze button renders,
  and a "FREEZE" text label sits beside it

### Requirement: Tempo is external via the DAW
WHEN hosted as a plugin, THE master clock SHALL follow the host tempo
through the core's existing external-clock slaving, and the BPM control
SHALL behave exactly as it does when slaved to external MIDI clock:
display-direction only, with user tempo requests suppressed.

#### Scenario: Host tempo drives the clock
- **WHEN** the DAW tempo changes while the plugin runs
- **THEN** the instrument's clock follows the host tempo
- **THEN** the BPM control displays the host tempo and does not accept a
  user tempo change

### Requirement: Bus and MIDI posture match the core's real I/O
THE plugin SHALL present a stereo output bus and one optional audio
input bus that feeds the external-audio modulation sources: the bus
SHALL NOT be required for the plugin to instantiate or make sound. The
external-audio sources SHALL start disconnected and SHALL connect when,
and only when, the operator has explicitly opted the input in AND that
bus carries at least one channel. An enabled bus alone SHALL NOT count
as consent: a host may enable an optional input bus with nothing routed
into it, and a channel existing has never meant the operator asked for
it. This is the same contract the standalone applies through device
selection, whose default is likewise no device. The plugin SHALL NOT consume MIDI note
input (the core exposes no note-input seam): DAW MIDI reaches the
instrument exclusively through host-parameter mappings.

#### Scenario: Instrument scans with the declared layout
- **WHEN** the DAW scans and instantiates the plugin
- **THEN** it presents as an instrument with a stereo output and an
  optional audio input bus, and instantiates whether or not the host
  connects that bus
- **THEN** incoming MIDI notes are not required and produce no core-side
  effect

#### Scenario: Opting the input in connects the sources
- **WHEN** the DAW user routes a signal into the plugin's input bus and
  the operator opts that input in
- **THEN** External Audio and External EF present as connected and
  modulate from that signal
- **WHEN** either the opt-in is withdrawn or the bus is disconnected
- **THEN** both sources return to their inert, disconnected state

#### Scenario: A host-enabled bus nobody asked for stays inert
- **WHEN** the host instantiates the plugin with the input bus enabled
  but the operator has not opted the input in
- **THEN** External Audio and External EF remain disconnected and take
  no part in randomization

#### Scenario: The host constrains what can be selected
- **WHEN** the host provides the input bus with a given channel layout
- **THEN** the input selection offers only None and the channels that
  layout actually provides
- **WHEN** the bus is disabled or provides no channels
- **THEN** the only available selection is None

#### Scenario: A selection the host takes away
- **WHEN** a channel is selected and the host then changes the bus
  layout so that channel no longer exists
- **THEN** the selection falls back to None and both sources go inert,
  rather than reading a channel that is gone

#### Scenario: The opt-in survives the project
- **WHEN** a project saved with the input opted in is reopened
- **THEN** the opt-in is restored
- **WHEN** a project predating the setting is reopened
- **THEN** the input is off

### Requirement: Parameters are external via a stable automation surface
THE plugin SHALL expose every user parameter of the six-bank model through
host parameters carrying a flat stable ID (for DAW automation and DAW-side
MIDI mapping) plus a grouped display name, bridged bidirectionally to the
app's single parameter authority; stable IDs SHALL be stable across
sessions and releases. THE plugin SHALL NOT implement internal MIDI
mapping or MIDI learn.

#### Scenario: DAW automation round-trip
- **WHEN** the DAW writes a host parameter and later reads it back
- **THEN** the app's parameter value follows the write and the readback
  matches, under the same value semantics the standalone surface uses

#### Scenario: MIDI mapping lives in the DAW
- **WHEN** the operator maps a MIDI controller to a plugin parameter in
  the DAW
- **THEN** the mapping works through the host parameter alone, with no
  plugin-side MIDI configuration

### Requirement: Editor hosts the portable surface
THE plugin editor SHALL render the same portable app surface the
standalone launcher renders (minus DAW-owned chrome: no internal
transport controls, no audio-device page), through the same portable
renderer, so surface improvements reach the plugin without a parallel UI.

#### Scenario: One surface, both hosts
- **WHEN** a change lands in the portable surface
- **THEN** the plugin editor renders it without plugin-specific UI work
  beyond the DAW-owned exclusions

### Requirement: Session state survives the host project
THE plugin SHALL persist its full user-visible state through the host's
own state calls and restore it on reload, so that a project saved and
reopened presents the instrument exactly as it was left — including
parameter values the operator changed by hand, which no automation lane
would rewrite. Restoration SHALL go through the app's single parameter
authority, and SHALL NOT mutate the standalone application's own saved
patches as a side effect. The stored representation SHALL survive
parameter-model growth: a session saved before a bank or slot is added
SHALL still restore, without corruption, after it is.

#### Scenario: A saved project reopens unchanged
- **WHEN** the operator edits parameters by hand, saves the DAW project,
  closes it, and reopens it
- **THEN** the instrument's parameter values are the edited ones
- **AND** the host parameters read back those same values

#### Scenario: Project state is not the standalone's patch store
- **WHEN** a project restores plugin state
- **THEN** the standalone application's saved patches are unmodified

#### Scenario: An old session outlives model growth
- **WHEN** a session stored against a smaller parameter model is
  restored into a build whose model has grown
- **THEN** every stored parameter restores to its saved value
- **AND** parameters the stored session never knew keep their defaults

### Requirement: Automation does not steal the operator's view
THE plugin SHALL deliver an automated parameter's value to that
parameter's own bank and slot regardless of which page the editor is
currently showing. The editor's visible page SHALL follow operator
selection only: automation SHALL NOT move it, for a single lane or for
simultaneous lanes, and SHALL NOT close a modulation view the operator has
open.

#### Scenario: Simultaneous cross-bank lanes
- **WHEN** two automation lanes drive parameters in two different banks
  at once
- **THEN** each value lands on its own bank's parameter
- **AND** the editor's visible page does not move

#### Scenario: The operator is drilled into an automated bank
- **WHEN** the operator has a modulation view open on a page and a lane
  automates a parameter on that same page
- **THEN** the value lands on that parameter
- **AND** the open modulation view is neither closed nor written to

#### Scenario: Operator selection still moves the page
- **WHEN** the operator selects a different page
- **THEN** the editor's visible page follows that selection

