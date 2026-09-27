# Delta — `froggers-vst-host`

The plugin gains the Controllers and File pages the standalone and browser
builds have. Its controller rows open their own MIDI ports, exactly as in the
standalone; the DAW track's MIDI still reaches the instrument only through
host-parameter mappings. One promoted clause is reversed: "THE plugin SHALL
NOT implement internal MIDI mapping or MIDI learn". That requirement is
restated in full with the reversal in place. The editor and session-state
requirements are restated with the pages and the state they now carry. The
added requirements state what is new.

## MODIFIED Requirements

### Requirement: Parameters are external via a stable automation surface
THE plugin SHALL expose every user parameter of the six-bank model through
host parameters carrying a flat stable ID (for DAW automation and DAW-side
MIDI mapping) plus a grouped display name, bridged bidirectionally to the
app's single parameter authority; stable IDs SHALL be stable across
sessions and releases. THE plugin SHALL also map controller MIDI itself,
through the controller rows of its Controllers page: the same rows, presets
and mappings the standalone and browser builds offer, each row reading its own
MIDI input port, so that encoders follow the shown page and drill level and
buttons do their jobs. THE plugin SHALL NOT offer MIDI learn. A change a
controller row makes SHALL reach the host the same way any core-side change
does.

<!-- RESTATES-EXCEPT
with no plugin-side MIDI configuration
  keeps: the mapping works through the host parameter alone
-->

#### Scenario: DAW automation round-trip
- **WHEN** the DAW writes a host parameter and later reads it back
- **THEN** the app's parameter value follows the write and the readback
  matches, under the same value semantics the standalone surface uses

#### Scenario: MIDI mapping lives in the DAW
- **WHEN** the operator maps a MIDI controller to a plugin parameter in
  the DAW
- **THEN** the mapping works through the host parameter alone, whatever the plugin's controller rows hold

#### Scenario: A controller row follows the shown page
- **WHEN** a MIDI Fighter Twister row is live, the user moves from the Audio page to the Envelope page, and turns knob 1
- **THEN** slot 0 of the Envelope page moves and slot 0 of the Audio page does not
- **AND** the host parameter for the Envelope page's slot 0 reads back the new value after the next pump
- Check: not yet delivered: NEW twister_row_turn_moves_the_shown_pages_slot_zero in app/vst/FroggersVstHostTests.cpp (task 2.2)

### Requirement: Editor hosts the portable surface
THE plugin editor SHALL render the same portable app surface the
standalone launcher renders (minus DAW-owned chrome: no internal
transport controls, no audio-device page), through the same portable
renderer, so surface improvements reach the plugin without a parallel UI.
THE editor SHALL compose that surface with the runtime sidebar through the
same runtime main component the standalone and browser use, declaring the
Controllers and File pages only: no Audio I/O page, no Sync page and no load
readout, each of which belongs to the DAW.

#### Scenario: One surface, both hosts
- **WHEN** a change lands in the portable surface
- **THEN** the plugin editor renders it without plugin-specific UI work
  beyond the DAW-owned exclusions

#### Scenario: The sidebar holds Controllers and File
- **WHEN** the plugin editor opens
- **THEN** the sidebar shows Controllers and File and nothing else, and the **?** button sits in the sidebar column below File, overlapping no sidebar entry
- **AND** opening either page replaces the instrument view, and Back restores it with the visible page and drill level as they were
- Check: not yet delivered: NEW editor_sidebar_holds_controllers_and_file_only in app/vst/FroggersVstEditorTest.cpp (task 2.3)

### Requirement: Session state survives the host project
THE plugin SHALL persist its full user-visible state through the host's
own state calls and restore it on reload, so that a project saved and
reopened presents the instrument exactly as it was left — including
parameter values the operator changed by hand, which no automation lane
would rewrite, the controller rows, and the File page's current patch.
Restoration SHALL go through the app's single parameter authority, and SHALL
NOT mutate the standalone application's own saved patches, or its runtime
configuration, as a side effect. The stored representation SHALL survive
parameter-model growth: a session saved before a bank or slot is added
SHALL still restore, without corruption, after it is. The state snapshot SHALL
be requested through the same patch manager the File page's saves use, so that
neither loses the other's response. THE plugin SHALL mark every state it writes
as carrying the instance's controller rows, and SHALL apply controller rows
from restored state only when that mark is present; state without it SHALL
restore its sound, input selection, page and Freeze latch and SHALL leave the
instance with no controller rows, because the rows such state holds were read
from the standalone's runtime configuration, not set up in the plugin.

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

#### Scenario: Controller rows and the current patch return with the project
- **WHEN** the user adds a Twister row, saves a patch as "p", and the instance's state is restored into a new instance
- **THEN** the new instance has the Twister row with its output port reopened, the File page's header reads "p", and Save adds a version to "p"
- **AND** state carrying no current patch restores with no current patch
- Check: not yet delivered: NEW project_restores_controller_rows_and_the_current_patch in app/vst/FroggersVstHostTests.cpp (task 2.5)

#### Scenario: State without the controller-rows mark brings no rows
- **WHEN** state whose document carries a Twister row but whose session extras carry no controller-rows mark is restored into an instance holding an APC40 mkII row
- **THEN** the restored parameter values, page and input selection are the state's
- **AND** the instance has no controller rows, and no port of the Twister row is opened
- Check: not yet delivered: NEW unmarked_state_restores_sound_and_input_and_no_controller_rows in app/vst/FroggersVstHostTests.cpp (task 2.5)

#### Scenario: A save during a snapshot loses nothing
- **WHEN** the user presses Save while the plugin's state snapshot is outstanding
- **THEN** the patch gains a version, and the state the host reads afterwards carries the values saved
- Check: not yet delivered: NEW save_during_a_snapshot_writes_a_version_and_refreshes_the_state in app/vst/FroggersVstHostTests.cpp (task 2.5)

## ADDED Requirements

### Requirement: A new instance keeps no runtime configuration
THE plugin SHALL read and write no runtime configuration file and SHALL open no startup patch, so that a newly inserted instance starts at the default patch with no controller rows and no current patch, whatever the standalone has saved.

#### Scenario: A new instance starts at defaults
- **WHEN** a plugin instance is created while the standalone's data holds a saved patch with a knob off its default and a configuration that records that patch and holds a controller row
- **THEN** every host parameter reads its default, the instance has no controller rows and no current patch, and no file in that data changes
- Check: not yet delivered: NEW new_instance_reads_and_writes_none_of_the_standalones_data in app/vst/FroggersVstHostTests.cpp (task 2.1)

### Requirement: The Controllers page works as the standalone's
THE plugin SHALL open each controller row's MIDI input and MIDI output port itself, through the same connection manager the standalone uses, SHALL show each row's MIDI in and MIDI out selectors and status dots as the standalone does, SHALL send a row's feedback and connect messages to its MIDI out, SHALL reconnect and resend a row's feedback when its device returns, and SHALL stop sending and close every port it opened before the instance is destroyed. THE plugin SHALL NOT read controller MIDI from the host's MIDI buffer.

#### Scenario: A turn and a button reach the instrument, and feedback reaches the device
- **WHEN** a Twister row is bound to the Twister's ports and the user turns knob 1, then presses the top-left side button
- **THEN** slot 0 of the shown page moves, the next press moves to the next page, and the Twister's output receives ring values for the page shown
- Check: not yet delivered: NEW twister_row_turn_moves_the_shown_pages_slot_zero, twister_side_button_moves_to_the_next_page and twister_row_feedback_reaches_its_output_port in app/vst/FroggersVstHostTests.cpp (task 2.2); operator step, task 2.7 (PLG-17, PLG-20)

#### Scenario: A replugged device reconnects
- **WHEN** the Twister's ports disappear and then return
- **THEN** the row's dots read offline, and after the next device poll the ports reopen and the row's feedback is sent again in full
- Check: not yet delivered: NEW replugged_twister_reconnects_and_resends_feedback in app/vst/FroggersVstHostTests.cpp (task 2.2); operator step, task 2.7 (PLG-25)

#### Scenario: Connect messages go out when the output opens
- **WHEN** an Akai APC40 mkII (Ableton) row and a Launchpad X row are bound to present ports
- **THEN** each row's MIDI out receives that row's connect messages, in order, when it opens
- Check: not yet delivered: NEW connect_messages_go_to_each_rows_output_when_it_opens in app/vst/FroggersVstHostTests.cpp (task 2.2)

#### Scenario: Two rows read only their own ports
- **WHEN** a Twister row and an APC40 mkII (Generic) row are bound to their own ports, and a message both presets map (channel 1, CC 14) arrives on the Twister's input
- **THEN** only the Twister row's target moves
- Check: not yet delivered: NEW two_rows_each_read_only_their_own_port in app/vst/FroggersVstHostTests.cpp (task 2.2)

#### Scenario: Removing an instance sends nothing afterwards
- **WHEN** an instance with an open row output and queued feedback is destroyed
- **THEN** the MIDI sender has stopped before any output port closes
- Check: not yet delivered: NEW destroying_the_processor_stops_the_sender_before_closing_outputs in app/vst/FroggersVstHostTests.cpp (task 2.2)

### Requirement: The File page saves and loads the standalone's patches
THE plugin SHALL offer the File page with New, Save, Save As and Load over the standalone's patches folder, SHALL apply a loaded patch's sound and controller setup to this instance only, and SHALL write no runtime configuration for any File page action.

#### Scenario: A patch saved in the plugin appears in the standalone's folder
- **WHEN** the user saves a patch as "p" in the plugin
- **THEN** a version exists in the patches root's `p` directory, and the header reads "p"
- Check: not yet delivered: NEW file_page_save_as_writes_a_version_under_the_patches_root in app/vst/FroggersVstHostTests.cpp (task 2.3)

#### Scenario: Load applies sound and setup to this instance
- **WHEN** the user loads a patch that carries an APC40 mkII row and a knob off its default
- **THEN** this instance's knob and controller rows become the patch's, and no configuration file is written
- Check: not yet delivered: NEW file_page_load_applies_sound_and_rows_and_writes_no_configuration in app/vst/FroggersVstHostTests.cpp (task 2.3)

### Requirement: A controller cannot run the plugin's transport
WHILE hosted as a plugin, only the host playhead SHALL start and stop the transport: a controller button mapped to Play, Stop or Record SHALL change nothing, and Freeze -- from the editor, its host parameter, or a controller -- SHALL stop the instrument and its release SHALL restart it, as in the standalone.

#### Scenario: Play, Stop and Record on a controller change nothing
- **WHEN** a Twister row is live and the DAW is stopped, and the user presses the button mapped to Play
- **THEN** the transport stays stopped
- **AND** with the DAW playing, the button mapped to Stop under Shift leaves the transport running, an APC40 mkII's RECORD arms no recording, and the button mapped to Freeze latches
- Check: not yet delivered: NEW controller_play_stop_record_do_nothing_and_freeze_latches in app/vst/FroggersVstHostTests.cpp (task 2.4); operator step, task 2.7 (PLG-27)

### Requirement: The host learns of a non-parameter change
WHEN a Controllers page commit, a File page New, Save As or Load is accepted, THE plugin SHALL tell the host that its non-parameter state changed, so that a host that honours it marks the project as needing a save.

#### Scenario: Controller and File changes notify the host
- **WHEN** a controller row is added, and then New, Save As and Load are each accepted
- **THEN** the processor's listeners receive a change carrying the non-parameter-state flag after each
- Check: not yet delivered: NEW controller_commit_and_file_actions_mark_non_parameter_state_changed in app/vst/FroggersVstHostTests.cpp (task 2.6); operator step, task 2.7 (the save prompt)
