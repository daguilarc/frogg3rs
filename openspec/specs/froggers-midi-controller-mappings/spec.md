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
The MIDI Fighter Twister preset SHALL map its six side buttons on channel 3 (channel 4 counted from 1) at CC 8 to 13 as: CC 8 Bank Next, shifted Bank Previous; CC 9 Play, shifted Stop; CC 10 Freeze, shifted Reset Page; CC 11 Scene 1, shifted Scene 2; CC 12 Randomize Page, shifted Randomize All; CC 13 Shift. Reset All and Record SHALL remain on screen and SHALL NOT be on the Twister's side buttons. The preset SHALL continue to require CC Hold on every side button, because the release is what ends Shift. The preset SHALL also give two encoder turns a shifted job: the turn that moves Crunchy, position 15 on channel 0, whose shifted job SHALL be Scene blend, so that while Shift is held a turn of that encoder moves the scene blend by one turn step per detent in the direction turned and Crunchy does not move; and the turn that moves Crispy, position 14 on channel 0, whose shifted job SHALL be Tempo, so that while Shift is held a turn of that encoder moves the tempo in the direction turned, clamped to 30 at the bottom and 300 at the top, and Crispy does not move. Crispy's shifted turn SHALL move the tempo by the same amount whichever parameter page is on screen, because the shifted job addresses the tempo directly and never Crispy's own page; unshifted, Crispy SHALL keep behaving as any other per-page control, scoped to whichever page is current. Once Shift is released each encoder SHALL move its own parameter again. The preset SHALL find both turns by their parameters' slots, not by CC numbers written into the preset. No other preset SHALL map a Shift button or a shifted job, on a button or on an encoder turn.

#### Scenario: The preset's side buttons are the eleven named jobs
- **WHEN** the Twister device default is read
- **THEN** its six system messages, in CC order, are Bank Next, Play, Freeze, Scene 1 (value "0"), Randomize Page, and a held Shift, at channel 3 CC 8 to 13 with feedback off
- **AND** the first five carry shifted jobs Bank Previous, Stop, Reset Page, Scene 2 (value "1") and Randomize All, each resolving against the catalog
- Check: `app/FroggersMidiCatalogTests.cpp: device_defaults_are_valid_and_address_exactly_the_documented_controls`

#### Scenario: Only the Twister carries Shift or shifted jobs
- **WHEN** every device default is read
- **THEN** no APC40 or Launchpad association has a shifted press or a Shift press
- **AND** no APC40 encoder turn has a shifted job
- **AND** the Twister's only encoder turns with a shifted job are the one at Crunchy's slot, whose job is Scene blend, and the one at Crispy's slot, whose job is Tempo
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

#### Scenario: Shift turns Crispy's knob into the tempo
- **WHEN** a Twister row is live, Shift is held, and the encoder at Crispy's slot is turned clockwise
- **THEN** the tempo rises and Crispy's value does not change
- **WHEN** Shift is released and the same encoder is turned clockwise
- **THEN** Crispy's value rises and the tempo does not change
- Check: `app/FroggersMidiCatalogTests.cpp: twister_shift_turns_crispys_knob_into_the_tempo`

#### Scenario: A shifted tempo turn stops at each end of the range
- **WHEN** the tempo is at 30, Shift is held, and the encoder at Crispy's slot is turned one detent counter-clockwise
- **THEN** the tempo stays at 30
- **WHEN** the same encoder is then turned one detent clockwise
- **THEN** the tempo rises above 30
- **WHEN** the tempo is at 300 and the encoder is turned one detent clockwise
- **THEN** the tempo stays at 300
- Check: `app/FroggersMidiCatalogTests.cpp: twister_shifted_tempo_turn_stops_at_each_end_of_the_range`

#### Scenario: A shifted tempo turn moves the tempo the same on every page
- **WHEN** a Twister row is live, Shift is held, and the encoder at Crispy's slot is turned clockwise one detent on the current page
- **THEN** the tempo rises by some amount
- **WHEN** the page is then switched with Bank Next and the same encoder is turned clockwise one detent again under Shift
- **THEN** the tempo rises by that same amount
- **AND** turning Crispy unshifted on the new page leaves the previous page's own Crispy value where it was, as any other per-page control does
- Check: `app/FroggersMidiCatalogTests.cpp: twister_shifted_tempo_turn_moves_the_tempo_the_same_on_every_page`

#### Scenario: The Controllers page shows both shifted jobs
- **WHEN** a Twister row's encoder section is opened on the Controllers page
- **THEN** the turn at Crunchy's slot is its own row whose Shift field reads Scene Blend
- **AND** the turn at Crispy's slot is its own row whose Shift field reads BPM
- **AND** the other fourteen turns still read as one block
- Check: `app/FroggersControllersPageTests.cpp: twister_crunchy_turn_row_shows_its_shifted_scene_blend`

### Requirement: The app tells the library which of its actions is the tempo
The app's MIDI catalog SHALL name its BPM action as the catalog's tempo action, so that the runtime library takes the tempo range it clamps a shifted turn to from that action's declared analog range rather than holding a range of its own. The range SHALL stay stated in one place in the app, the constants the BPM slider is built from, and the catalog SHALL carry it once, as that action's analog range.

#### Scenario: The catalog names the BPM action as its tempo action
- **WHEN** the app's MIDI catalog is read
- **THEN** its tempo action is the BPM action's name
- **AND** that action declares an analog range of 30 to 300, taken from the same constants the BPM slider's bounds are taken from
- Check: `app/FroggersMidiCatalogTests.cpp: catalog_names_the_bpm_action_as_its_tempo_action`

