# Delta — `froggers-transport-and-reset-controls`

Carried from `frogg3rs-o1-audit`: Reset All on a parameter page did not
reach a fresh launch's state (level-2 depths and neutral depths survived),
and re-arming Record raced the audio thread on the capture buffer. Both
requirements carry unchanged in substance; the Reset requirement gains the
gesture clause and names the checks the rig runs. The Play plate's
transport-running read moves to what the engine publishes; its scenarios
are unchanged.

### Requirement: Stopping a long take never holds a UI tick
WHEN a take stops, THE app SHALL encode it on a thread other than the UI and message thread, and SHALL offer the file at the first message-thread tick after the encode finishes; the file's bytes SHALL be the bytes the one-pass encode produces.

#### Scenario: A 30-minute take
- **WHEN** a 30-minute take stops
- **THEN** no UI or message-thread call spends more than 33,333,333 ns on it
- **AND** the file is offered within one tick of the encode finishing
- **AND** its bytes equal the one-pass encode's
- Check: not yet delivered; the change's encode task adds the case

## MODIFIED Requirements

### Requirement: Reset restores the default patch
THE Reset controls SHALL revert to the instrument's fresh-launch
default patch — the values a first launch presents, which are 0 for
most parameters but not all — never to a flat all-zeros state no
launch ever shows. Reset All SHALL be global: every bank's page
parameters, every parameter's modulation depths, every bank's local
Crispy, and the shared global Crunchy all revert to their defaults.
Reset Page SHALL revert the currently shown page's slice of that same
default patch, including that page's Crispy, and SHALL NOT touch other
pages. From a drilled-in modulation grid, reset SHALL revert the
selected parameter's modulation depths to their default-patch values.
The default patch SHALL have a single definition shared by launch,
reset, and New, so the three can never drift apart.

Equality with a fresh launch SHALL be evaluated over which parameters and
modulation depths EXIST as well as the values they carry. A depth parameter that
was materialized by an operation and left at a neutral value is not equal to one
that was never materialized.

Equality SHALL further be observable in the instrument's AUDIO OUTPUT, not only
in its stored values. A reset that leaves every stored value correct while the
instrument goes on sounding differently has not restored the default patch. This
clause exists because parameter-level equality and audible behaviour are not
interchangeable evidence: stored equality has held while the instrument
audibly did not decay.

Every Reset SHALL restore each parameter it resets through the framework's whole reset of that parameter, which returns every level below it, SHALL release the depths left neutral in the same block, and SHALL take the knobs and depths it resets out of every gesture in every scene it resets, since gesture membership is patch state.

#### Scenario: Reset All lands exactly on a fresh launch
- **WHEN** the operator has changed parameters, depths, Crispy, and
  Crunchy — including via Randomize All — and presses Reset All
- **THEN** the instrument's entire state equals a fresh launch's
  default patch, field for field, in both scene poles
- **THEN** the set of materialized modulation depth parameters equals a fresh
  launch's set, with no extra depths left over from the operations before it
- **THEN** Crispy on every bank and global Crunchy are at their
  default values

#### Scenario: Reset All restores the envelope's decay, not only its values

- **WHEN** the operator presses Randomize All, and then presses Reset All on a
  later block than the randomize landed on
- **THEN** the instrument decays to silence on the same schedule a fresh launch
  does, measured in the audible band rather than as a broadband level
- **THEN** this holds however many blocks separate the two presses, so that a
  reset arriving on the same block as the randomize is not the only case that
  works

#### Scenario: Reset Page restores that page's defaults, not zeros
- **WHEN** the Audio page's parameters have been edited and Reset Page
  is pressed while the Audio page is shown
- **THEN** the Audio page's parameters return to their default-patch
  values — including the non-zero VCO shape defaults and the default
  cross-VCO pitch modulation depths — and the page's Crispy returns to
  its default
- **THEN** every other page's state is untouched

#### Scenario: One definition of the default patch
- **WHEN** the default patch is changed in a future edit
- **THEN** launch, reset, and New all present the changed defaults, because
  all three read the same single definition

#### Scenario: Reset All after a drilled randomize equals a fresh launch, gestures included
- **WHEN** Randomize All is pressed in a modulation view, depths are edited, a gesture button is held while a knob moves, and Reset All is pressed on the rig
- **THEN** every parameter value, every depth's existence and every gesture mask equal a fresh rig's, in both scenes
- Check: not yet delivered; the change's task 2.2 carries the two reset-to-launch cases (reset_all_after_drilled_randomize_equals_a_fresh_launch_including_which_depths_exist, reset_with_a_gesture_button_held_matches_a_fresh_launch) into FroggersModulationTests.cpp

### Requirement: The Play plate shows whether the transport is running
The Play plate SHALL show a held state whenever the transport is running and SHALL show its idle state whenever the transport is stopped, on every host that shows Play. The held state SHALL swap the plate and glyph colours, the same way the Freeze plate shows its latch and the Record plate shows that it is armed. The state SHALL be read on every rebuild from the transport state the engine publishes in its clock diagnostics through `AppContext`, never from a mirror the app keeps, so it follows every route that starts or stops the transport: the Play, Stop and Freeze buttons, their MIDI mappings, a MIDI Start or Stop message, and the transport restart after an audio-device change.

#### Scenario: Play is held while the transport runs
- **WHEN** Play is pressed and the transport is running
- **THEN** the Play plate's draw commands swap its plate and glyph colours
- Check: `app/FroggersSurfaceTests.cpp: play_plate_is_held_while_the_transport_runs`

#### Scenario: Stop and Freeze release the Play plate
- **WHEN** the transport is running and Stop is pressed
- **THEN** the Play plate shows its idle colours
- **WHEN** the transport is running and Freeze is pressed
- **THEN** the Play plate shows its idle colours while the drone holds
- **WHEN** Freeze is then pressed a second time
- **THEN** the Play plate shows its held colours again
- Check: `app/FroggersSurfaceTests.cpp: play_plate_is_held_while_the_transport_runs`

#### Scenario: The held colours are a real exchange
- **WHEN** the Play plate's draw commands are built held and idle
- **THEN** the held plate colour is the idle glyph colour and the held glyph colour is the idle plate colour
- Check: `app/FroggersSurfaceTests.cpp: play_draw_commands_swap_plate_and_glyph_colours_while_running`

## ADDED Requirements

### Requirement: Arming a take never touches the buffer while the audio thread writes it
WHEN Record is armed, re-armed, or stopped and armed again, THE app SHALL clear and resize the capture buffer and reset its frame count only after the audio thread has stopped writing the previous take, and the new take SHALL begin at its first frame with no leading silence carried over. The audio thread SHALL still never allocate, and a Record press SHALL still be refused while the transport is stopped.

#### Scenario: Stop and re-arm against a running transport
- **WHEN** one thread stops and re-arms Record in a loop while the audio thread runs blocks with the transport running
- **THEN** a thread-sanitizer build reports no data race on the capture buffer or its frame count
- Check: none in the gate; the change's Record task runs its thread-sanitizer probe and records no race where the unfixed tree reports two
