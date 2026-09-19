# froggers-midi-controller-mappings Specification

## Purpose
TBD - created by archiving change frogg3rs-midi-shift. Update Purpose after archive.
## Requirements
### Requirement: A button's shifted job is part of its mapping
The app SHALL keep the runtime library's Shift message kind in its MIDI catalog, so that any button on any controller can be mapped to Shift from the Controllers page, and SHALL offer no on-screen Shift control. Every system-message row on the Controllers page SHALL carry a shifted job, chosen from none or any of the app's actions, that fires instead of the row's ordinary job while a Shift button on the same controller is held. The app SHALL define no fixed pairing of actions: which job a button does while shifted is that button's own mapping, editable per row and stored with the patch like every other mapping.

#### Scenario: Shift is offered and has no screen control
- **WHEN** the Controllers page builds a system-message row's message dropdown for this app
- **THEN** Shift is among the choices
- **AND** no node of the app's own surface dispatches or renders a Shift control
- Check: `app/FroggersMidiCatalogTests.cpp: device_defaults_are_valid_and_address_exactly_the_documented_controls`; `app/check_catalog_covers_screen_actions.sh` (unchanged; Shift is not a `FroggersActions` constant)

#### Scenario: A shifted job is edited per row and survives a patch
- **WHEN** the operator sets a row's Shift column to Reset Page, saves the patch, and loads it
- **THEN** the row's Shift column reads Reset Page
- **AND** pressing that button while Shift is held resets the page
- Check: Sheaf `External/Sheaf/projects/synth/tests/instrument_tests.cpp: ShiftHeldSwapsPressForShiftedPressAndReleaseClearsIt`, `AssociationJsonRoundTripsShiftedPressAndTreatsAbsentAsNone`, `External/Sheaf/projects/synth/tests/engine_tests.cpp: engine_rebuild_resolves_shifted_app_action_and_clears_an_unknown_one`; `app/FroggersControllersPageTests.cpp: twister_system_rows_carry_shift_editable_field_and_derived_choice_index`

### Requirement: The MIDI Fighter Twister preset maps five buttons with shifted jobs and one Shift
The MIDI Fighter Twister preset SHALL map its six side buttons on channel 3 (channel 4 counted from 1) at CC 8 to 13 as: CC 8 Bank Next, shifted Bank Previous; CC 9 Play, shifted Stop; CC 10 Freeze, shifted Reset Page; CC 11 Scene 1, shifted Scene 2; CC 12 Randomize Page, shifted Randomize All; CC 13 Shift. Reset All and Record SHALL remain on screen and SHALL NOT be on the Twister's side buttons. The preset SHALL continue to require CC Hold on every side button, because the release is what ends Shift. The preset SHALL also give one encoder turn a shifted job: the turn that moves Crunchy, position 15 on channel 0, whose shifted job SHALL be Scene blend, so that while Shift is held a turn of that encoder moves the scene blend by one turn step per detent in the direction turned and Crunchy does not move, and once Shift is released the same encoder moves Crunchy again. The preset SHALL find that turn by Crunchy's slot, not by a CC number written into the preset. No other preset SHALL map a Shift button or a shifted job, on a button or on an encoder turn.

#### Scenario: The preset's side buttons are the eleven named jobs
- **WHEN** the Twister device default is read
- **THEN** its six system messages, in CC order, are Bank Next, Play, Freeze, Scene 1 (value "0"), Randomize Page, and a held Shift, at channel 3 CC 8 to 13 with feedback off
- **AND** the first five carry shifted jobs Bank Previous, Stop, Reset Page, Scene 2 (value "1") and Randomize All, each resolving against the catalog
- Check: `app/FroggersMidiCatalogTests.cpp: device_defaults_are_valid_and_address_exactly_the_documented_controls`

#### Scenario: Only the Twister carries Shift or shifted jobs
- **WHEN** every device default is read
- **THEN** no APC40 or Launchpad association has a shifted press or a Shift press
- **AND** no APC40 encoder turn has a shifted job
- **AND** the Twister's only encoder turn with a shifted job is the one at Crunchy's slot, and that job is Scene blend
- Check: `app/FroggersMidiCatalogTests.cpp: device_defaults_are_valid_and_address_exactly_the_documented_controls`

#### Scenario: A missing release leaves Shift held until the next press and release
- **WHEN** a controller is unplugged while its Shift button is down
- **THEN** that controller's mappings stay shifted until a Shift press and release arrive
- **AND** the manual states this and the recovery
- Check: none. MANUAL.md's Shift subsection documents this; no automated check reads it.

#### Scenario: Shift turns Crunchy's knob into the scene blend
- **WHEN** a Twister row is live, Shift is held, and the encoder at Crunchy's slot is turned clockwise
- **THEN** the scene blend rises by one turn step per detent and Crunchy's value does not change
- **WHEN** Shift is released and the same encoder is turned clockwise
- **THEN** Crunchy's value rises and the scene blend does not change
- Check: `app/FroggersMidiCatalogTests.cpp: twister_shift_turns_crunchys_knob_into_the_scene_blend`

#### Scenario: The Controllers page shows Crunchy's shifted job
- **WHEN** a Twister row's encoder section is opened on the Controllers page
- **THEN** the turn at Crunchy's slot is its own row whose Shift field reads Scene Blend
- **AND** the other fifteen turns still read as one block
- Check: `app/FroggersControllersPageTests.cpp: twister_crunchy_turn_row_shows_its_shifted_scene_blend`

