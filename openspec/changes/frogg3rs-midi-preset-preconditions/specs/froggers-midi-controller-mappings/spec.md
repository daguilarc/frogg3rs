# Delta — `froggers-midi-controller-mappings`

The promoted scenario "A missing release leaves Shift held until the next press
and release" is REVERSED by this delta, not merely reworded. Its behaviour is
replaced by the four triggers Sheaf's `midi-controller-resilience` gives a held
modifier, and the manual sentence it points at is rewritten. This is stated here
because a reader comparing the promoted spec to this one would otherwise see the
scenario quietly disappear.

## MODIFIED Requirements

### Requirement: The MIDI Fighter Twister preset maps five buttons with shifted jobs and one Shift
The MIDI Fighter Twister preset SHALL map its six side buttons on channel 3 (channel 4 counted from 1) at CC 8 to 13 as: CC 8 Bank Next, shifted Bank Previous; CC 9 Play, shifted Stop; CC 10 Freeze, shifted Reset Page; CC 11 Scene 1, shifted Scene 2; CC 12 Randomize Page, shifted Randomize All; CC 13 Shift. Reset All and Record SHALL remain on screen and SHALL NOT be on the Twister's side buttons. The preset SHALL declare CC Hold on every side button as a device precondition, because the button's release is the promptest of the triggers that end a held modifier, and SHALL remain usable in its unshifted form when that precondition is unmet. No other preset SHALL map a Shift button or a shifted job.

#### Scenario: The preset's side buttons are the eleven named jobs
- **WHEN** the Twister device default is read
- **THEN** its six system messages, in CC order, are Bank Next, Play, Freeze, Scene 1 (value "0"), Randomize Page, and a held Shift, at channel 3 CC 8 to 13 with feedback off
- **AND** the first five carry shifted jobs Bank Previous, Stop, Reset Page, Scene 2 (value "1") and Randomize All, each resolving against the catalog
- Check: `app/FroggersMidiCatalogTests.cpp: device_defaults_are_valid_and_address_exactly_the_documented_controls`

#### Scenario: Only the Twister carries Shift or shifted jobs
- **WHEN** every device default is read
- **THEN** no APC40 or Launchpad association has a shifted press or a Shift press
- Check: `app/FroggersMidiCatalogTests.cpp: device_defaults_are_valid_and_address_exactly_the_documented_controls`

#### Scenario: A Shift whose release never arrives ends by another trigger, and the manual says which
- **WHEN** a controller is unplugged while its Shift button is down, or its Shift address stops transmitting
- **THEN** the manual's Shift subsection states the triggers that end a held modifier and does not state a recovery that requires the Shift address to transmit
- **AND** it states the same for Hold Drill
- Check: not yet delivered; added by this change as `app/check_docs_match_device_preconditions.py`, which fails when the manual's recovery text names no trigger

## ADDED Requirements

### Requirement: A preset declares the device settings it depends on
A device default SHALL carry its device-side preconditions as declared data, each naming the setting, the value the preset requires, and where that value is set. The Controllers page SHALL show a preset's declared preconditions where that preset is chosen. `MANUAL.md`'s per-device settings section SHALL be generated from those declarations, and a check SHALL fail when the generated text and the declarations disagree. A preset SHALL NOT depend on a device setting it does not declare.

#### Scenario: The Twister preset declares its three settings
- **WHEN** the MIDI Fighter Twister device default is read
- **THEN** it declares encoders set to relative "Enc 3FH/41H", all six side buttons set to "CC Hold", and "Bank Side Buttons" unchecked, each naming the Midi Fighter Utility as where it is set
- Check: not yet delivered; added by this change as `app/FroggersMidiCatalogTests.cpp: device_defaults_declare_their_preconditions`

#### Scenario: The APC40 preset declares its track-selection caveat
- **WHEN** either APC40 mkII device default is read
- **THEN** it declares that Track 1 must stay selected, naming the consequence of selecting another track
- Check: not yet delivered; added by this change as `app/FroggersMidiCatalogTests.cpp: device_defaults_declare_their_preconditions`

#### Scenario: The manual and the declarations cannot disagree
- **WHEN** a device default's declared preconditions differ from the manual's per-device settings section
- **THEN** the check fails and names the device and the setting that differs
- Check: not yet delivered; added by this change as `app/check_docs_match_device_preconditions.py`

#### Scenario: Choosing a preset shows what the device needs
- **WHEN** a preset is selected in the Controllers page's Layout dropdown
- **THEN** that preset's declared preconditions are shown on the page
- Check: not yet delivered; added by this change as `app/FroggersControllersPageTests.cpp: layout_choice_shows_declared_preconditions`

### Requirement: A Launch Control XL preset carries scene blend on a fader
The catalogue SHALL offer a Novation Launch Control XL device default of `Generic` kind, which the instrument model permits an analog section. That default SHALL assign scene blend to one of the device's faders other than fader 1, and SHALL declare the template its control map depends on as a device-side precondition. It SHALL NOT map a Shift button or a shifted job.

#### Scenario: The preset offers scene blend on a fader
- **WHEN** the Launch Control XL device default is read
- **THEN** its kind is `Generic`, its analog section sets scene blend to a fader address, and that address is not fader 1's
- Check: not yet delivered; added by this change as `app/FroggersMidiCatalogTests.cpp: device_defaults_are_valid_and_address_exactly_the_documented_controls`

#### Scenario: The preset declares the template its map depends on
- **WHEN** the Launch Control XL device default is read
- **THEN** it declares which template the device must be on, because the control map moves with the template
- Check: not yet delivered; added by this change as `app/FroggersMidiCatalogTests.cpp: device_defaults_declare_their_preconditions`

#### Scenario: Scene blend reaches the engine from that fader
- **WHEN** the declared fader sends a control change on the declared template
- **THEN** the scene blend value changes by the same path the APC40 crossfader uses
- Check: not yet delivered; added by this change as `app/FroggersControllersPageTests.cpp: launch_control_xl_fader_drives_scene_blend`
