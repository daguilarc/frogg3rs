# Delta — `froggers-midi-controller-mappings`

The promoted scenario "A missing release leaves Shift held until the next press
and release" is REVERSED by this delta, not merely reworded. Its behaviour is
replaced by the triggers Sheaf's `midi-controller-resilience` change's
`HeldModifierClearSource` enumerates, and the manual sentence it points at is
rewritten. This is stated here because a reader comparing the promoted spec to
this one would otherwise see the scenario quietly disappear.

## MODIFIED Requirements

### Requirement: The MIDI Fighter Twister preset maps five buttons with shifted jobs and one Shift
The MIDI Fighter Twister preset SHALL map its six side buttons on channel 3 (channel 4 counted from 1) at CC 8 to 13 as: CC 8 Bank Next, shifted Bank Previous; CC 9 Play, shifted Stop; CC 10 Freeze, shifted Reset Page; CC 11 Scene 1, shifted Scene 2; CC 12 Randomize Page, shifted Randomize All; CC 13 Shift. Reset All and Record SHALL remain on screen and SHALL NOT be on the Twister's side buttons. The preset SHALL declare CC Hold on every side button as a device precondition, because CC Hold is what makes every side button address CC 127-on-press/0-on-release instead of the factory bank-switch behaviour the middle pair defaults to; for Shift specifically, that same release is also the promptest of the triggers that end a held modifier. A side button's press SHALL still dispatch a job — its ordinary job, or its shifted job if Shift is currently held — when that button's own CC Hold precondition is unmet; a button's press SHALL NOT be silently dropped for want of a release. When the unmet precondition is on Shift's own button, the other five side buttons SHALL remain usable in their shifted form, not their unshifted form, until Shift's held state is cleared by one of `HeldModifierClearSource`'s other triggers. No other preset SHALL map a Shift button or a shifted job.

<!-- RESTATES-EXCEPT
no APC40 or Launchpad association has a shifted press or a Shift press
  keeps: a shifted press or a Shift press
-->

#### Scenario: The preset's side buttons are the eleven named jobs
- **WHEN** the Twister device default is read
- **THEN** its six system messages, in CC order, are Bank Next, Play, Freeze, Scene 1 (value "0"), Randomize Page, and a held Shift, at channel 3 CC 8 to 13 with feedback off
- **AND** the first five carry shifted jobs Bank Previous, Stop, Reset Page, Scene 2 (value "1") and Randomize All, each resolving against the catalog
- Check: `app/FroggersMidiCatalogTests.cpp: device_defaults_are_valid_and_address_exactly_the_documented_controls`

#### Scenario: Only the Twister carries Shift or shifted jobs
- **WHEN** every device default is read
- **THEN** no non-Twister device default has a shifted press or a Shift press
- Check: not yet delivered; task 4.5 adds a loop-based case to app/FroggersMidiCatalogTests.cpp asserting this over every entry in the catalog's device defaults whose id is not the Twister's, with a positive control (proven on the then-last entry, and re-proven on the Launch Control XL by task 5.3 once it exists) that a shifted press turns it red

#### Scenario: A non-Shift side button's press still dispatches when its own CC Hold is unmet
- **WHEN** a non-Shift Twister side button's press message is received and no matching release message ever arrives
- **THEN** the button's currently-applicable job — ordinary, or shifted if Shift is held at that moment — still fires on that press
- Check: not yet delivered; task 4.6(a) adds a case to app/FroggersMidiCatalogTests.cpp asserting this against the input processor's dispatch, which fires on the press edge before it inspects any release

#### Scenario: The other side buttons stay in shifted form when Shift's own CC Hold is unmet
- **WHEN** the Shift button's press message is received and no matching release message ever arrives
- **THEN** every other side button with a shifted job dispatches that shifted job, not its ordinary one, on every subsequent press, until one of `HeldModifierClearSource`'s other triggers clears Shift's held state
- Check: not yet delivered; task 4.6(b) adds a case to app/FroggersMidiCatalogTests.cpp asserting this against the same dispatch path

#### Scenario: A Shift whose release never arrives ends by another trigger, and the manual says which
- **WHEN** a controller is unplugged while its Shift button is down, or its Shift address stops transmitting
- **THEN** the manual's Shift subsection states the triggers that end a held modifier and does not state a recovery that requires the Shift address to transmit
- **AND** it states the same for Hold Drill
- Check: not yet delivered; task 1.8 adds this repository's device-preconditions check script's recovery half, which fails unless the Shift and Hold Drill subsections each (1) contain at least one of the multi-word recovery phrases task 3.1's rewrite uses verbatim ("selecting a different preset", "rebuilds the row's mapping", "unplugging and reconnecting the controller", "clears automatically after"), (2) do not contain "pressed and released again" or any other sentence naming the modifier's own button as what clears it, and (3), wherever "unplugging and reconnecting the controller" appears, also contain "not available in the plugin"

## ADDED Requirements

### Requirement: A preset declares the device settings it depends on
A device default SHALL populate `declaredPreconditions` (Sheaf's `synth-controller-wizards` requirement scw-6, on `MidiAppDeviceDefault`) with its device-side preconditions, each string naming the setting, the value the preset requires, and where that value is set. A device default with no device-side preconditions SHALL declare an empty list, distinguishing "declares none" from "was never asked." `MANUAL.md`'s per-device settings paragraph SHALL be generated from those declarations, and a check SHALL fail when the generated text and the declarations disagree. A preset SHALL NOT depend on a device setting it does not declare. Where a preset's declared preconditions are shown is Sheaf's `synth-runtime-ui` requirement sru-64; this requirement covers only the data and its agreement with the manual.

#### Scenario: The Twister preset declares its three settings
- **WHEN** the MIDI Fighter Twister device default is read
- **THEN** it declares encoders set to relative "Enc 3FH/41H", all six side buttons set to "CC Hold", and "Bank Side Buttons" unchecked, each naming the Midi Fighter Utility as where it is set
- Check: not yet delivered; task 4.3 adds a device_defaults_declare_their_preconditions case to app/FroggersMidiCatalogTests.cpp

#### Scenario: The APC40 Generic preset declares its track-selection caveat
- **WHEN** the APC40 mkII (Generic) device default is read
- **THEN** it declares that Track 1 must stay selected, naming the consequence of selecting another track
- Check: not yet delivered; task 4.4 adds a device_defaults_declare_their_preconditions case to app/FroggersMidiCatalogTests.cpp

#### Scenario: The APC40 Ableton preset declares no device-side precondition
- **WHEN** the APC40 mkII (Ableton) device default is read
- **THEN** its declared preconditions list is empty, because the app's own connect-time SysEx message (sent automatically) removes the Track 1 caveat and no other device-side setting is assumed
- Check: not yet delivered; task 4.4 adds a device_defaults_declare_their_preconditions case to app/FroggersMidiCatalogTests.cpp

#### Scenario: The manual and the declarations cannot disagree
- **WHEN** a device default's declared preconditions differ from the manual's per-device settings paragraph
- **THEN** the check fails and names the device and the setting that differs
- Check: not yet delivered; task 4.8 adds this repository's device-preconditions drift check

### Requirement: A Launch Control XL preset carries scene blend on a fader
The catalogue SHALL offer a Novation Launch Control XL device default of `Generic` kind, id `froggers.launchcontrolxl`, which the instrument model permits an analog section. That default SHALL assign scene blend to fader 1 (CC 77, channel 8 counted from 0), and SHALL declare factory template 1 as a device-side precondition, because the control map moves with the template. It SHALL NOT map a Shift button or a shifted job. The fader CC/channel map and the template both come from Ableton Live 12 Suite's control-surface script for the device, not from a vendor document — neither Novation document states the map. The lead read that script by disassembly; design.md carries the command and its literal output, and the check this requirement's scenarios cite is literal-against-literal against that one reading, not an independent confirmation of it (see design.md).

#### Scenario: The preset offers scene blend on a fader
- **WHEN** the Launch Control XL device default is read
- **THEN** its kind is `Generic`, and its analog section sets scene blend to channel 8 CC 77
- Check: not yet delivered; task 5.1 adds a case to app/FroggersMidiCatalogTests.cpp

#### Scenario: The preset declares the template its map depends on
- **WHEN** the Launch Control XL device default is read
- **THEN** it declares factory template 1 as a device-side precondition, because the control map moves with the template
- Check: not yet delivered; task 5.2 adds a device_defaults_declare_their_preconditions case to app/FroggersMidiCatalogTests.cpp

#### Scenario: Scene blend reaches the engine from that fader
- **WHEN** fader 1 sends a control change on factory template 1
- **THEN** the scene blend value changes by the same path the APC40 crossfader uses
- Check: operator step (not a build-time check) — once the default exists, select the preset on the live browser site, move fader 1, and observe the on-screen scene blend value move via `AnalogMidiInProcessor::Process`'s dispatch against `AnalogMidiInConfig::sceneBlend`, the same path the APC40 crossfader already exercises in production; task 5.4 names this
