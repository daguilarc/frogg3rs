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
The MIDI Fighter Twister preset SHALL map its six side buttons on channel 3 (channel 4 counted from 1) at CC 8 to 13 as: CC 8 Bank Next, shifted Bank Previous; CC 9 Play, shifted Stop; CC 10 Freeze, shifted Reset Page; CC 11 Scene 1, shifted Scene 2; CC 12 Randomize Page, shifted Randomize All; CC 13 Shift. Reset All and Record SHALL remain on screen and SHALL NOT be on the Twister's side buttons. The preset SHALL continue to require CC Hold on every side button, because the release is what ends Shift. No other preset SHALL map a Shift button or a shifted job.

#### Scenario: The preset's side buttons are the eleven named jobs
- **WHEN** the Twister device default is read
- **THEN** its six system messages, in CC order, are Bank Next, Play, Freeze, Scene 1 (value "0"), Randomize Page, and a held Shift, at channel 3 CC 8 to 13 with feedback off
- **AND** the first five carry shifted jobs Bank Previous, Stop, Reset Page, Scene 2 (value "1") and Randomize All, each resolving against the catalog
- Check: `app/FroggersMidiCatalogTests.cpp: device_defaults_are_valid_and_address_exactly_the_documented_controls`

#### Scenario: Only the Twister carries Shift or shifted jobs
- **WHEN** every device default is read
- **THEN** no APC40 or Launchpad association has a shifted press or a Shift press
- Check: `app/FroggersMidiCatalogTests.cpp: device_defaults_are_valid_and_address_exactly_the_documented_controls`

#### Scenario: A missing release leaves Shift held until the next press and release
- **WHEN** a controller is unplugged while its Shift button is down
- **THEN** that controller's mappings stay shifted until a Shift press and release arrive
- **AND** the manual states this and the recovery
- Check: none. MANUAL.md's Shift subsection documents this; no automated check reads it.

