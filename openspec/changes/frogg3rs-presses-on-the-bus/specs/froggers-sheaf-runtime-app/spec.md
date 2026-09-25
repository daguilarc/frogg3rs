# Delta — `froggers-sheaf-runtime-app`

Seven controls reach the audio thread through single-slot atomics that
lose count and order inside one message tick. Sheaf's UI bus already orders
every other control. The requirements below put the seven on that bus and
state the rule that keeps them there.

## ADDED Requirements

### Requirement: A press travels on the engine's UI bus
The app SHALL deliver every press that changes audio-thread state (Randomize All, Randomize Page, Reset All, Reset Page, page select, page previous, page next, encoder press) as a `MessageIn::AppCommand` on `AppContext::uiBus`, applied on the audio thread by `FroggersAppCore::ApplyAppCommand` in the block that pops it, in the order pushed, by the same drain that applies encoder turns and scene blend; Start, Continue, Stop and Clock remain the engine's realtime messages, lifted out of both buses and applied after both have drained. The app SHALL NOT hold such a press in an atomic, a flag or a queue of its own, and SHALL NOT hold the bus behind one. A press whose target depends on the current page (page previous, page next) SHALL resolve that target on the audio thread from the audio thread's own current page, never from the page the surface last displayed. A press that originates on a MIDI controller SHALL join the bus at the surface's action handler, the same point a click does.

#### Scenario: Presses keep their count and order inside one message tick
- **WHEN** two Back presses are dispatched inside one message tick from drill level 2
- **THEN** the drill is at level 0 after the next block
- **WHEN** two Page Next presses are dispatched inside one message tick from page 1
- **THEN** page 3 is shown after the next block
- **WHEN** Reset All then Randomize All are dispatched inside one message tick
- **THEN** the state after the next block is randomized; reversed, it is the launch state
- Check: `app/FroggersModulationTests.cpp: app_commands_apply_every_press_in_bus_order_within_one_tick`

#### Scenario: The plugin's page restore is a dispatched press
- **WHEN** a host restores a session that names a visible page
- **THEN** the page is selected through the surface's action handler and shows after one block

### Requirement: Every cross-thread member is a command, a value or a publication
A cross-thread input that crosses as its latest value (the host's routed-input signal; the Freeze latch; the Record arm; the desired transport state) SHALL be one atomic written by the message thread for another thread to read, and its declaration SHALL say it is a value and which thread reads it; a press whose effect the message thread computes and that crosses as the resulting value is such a value. State the audio thread publishes for the UI to read (the drill level shown in the header; whether the last randomize drew short; the recorded frame count and truncation flag) SHALL be one atomic written by the audio thread, declared a publication. The app SHALL NOT route a value or a publication through the press bus, and SHALL NOT route a command through an atomic. A member outside these classes (the Record writer handshake the audio thread raises around a block) SHALL say so at its declaration.

#### Scenario: The classification is visible at the declaration
- **WHEN** the app core's cross-thread members are read
- **THEN** each is declared a command (on the bus), a value (an atomic the message thread writes), a publication (an atomic the audio thread writes), or the one named exception, and no member is two of these

### Requirement: The BPM slider is the catalog's tempo action
The BPM slider SHALL push `MessageIn::SetTempoBpmNormalized` with its value placed in the catalog's tempo range (`kFroggersBpmMin` to `kFroggersBpmMax`), the same route a controller's mapped tempo control and a shifted Twister turn already take, and SHALL read its displayed tempo, its external-clock state and the transport's running state from what the engine publishes through `AppContext`. The app SHALL NOT mirror those values in atomics of its own.

#### Scenario: A slider drag reaches the master clock in one block
- **WHEN** the slider is dragged to 300 on the rig
- **THEN** the engine's clock diagnostics read 300 after one block
- Check: `app/FroggersSurfaceTests.cpp: bpm_slider_push_reaches_the_clock_diagnostics_publication_and_is_gated_by_the_context_sync_config`

#### Scenario: Slaved, the slider is a read-only line and pushes nothing
- **WHEN** receive-clock is requested and a BPM action is dispatched
- **THEN** the surface renders the read-only tempo line and pushes no tempo message

### Requirement: The add row offers this app's presets and Custom, and nothing else
The Controllers page's add row SHALL offer, in this order, MIDI Fighter Twister, Akai APC40 mkII (Generic), Akai APC40 mkII (Ableton), Launchpad X, Launchpad Pro MK3, Launchpad Mini MK3 and Custom, and SHALL offer no preset for a device this app ships no preset for. The app's MIDI catalog SHALL tell the library to add no library device of its own to that list.

#### Scenario: The Preset selector lists six presets and Custom
- **WHEN** the Controllers page is opened with nothing connected
- **THEN** the add row's Preset selector lists the six device presets above, in that order, then Custom
- **AND** it lists no WRLD.Bldr entry
- Check: `app/FroggersControllersPageTests.cpp`, `real_catalog_registers_exactly_one_descriptor_per_device_default`

#### Scenario: A WRLD.Bldr port is an unrecognised port
- **WHEN** a WRLD.Bldr is connected
- **THEN** its ports are listed under "Other inputs" and "Other outputs", like any port no preset recognizes
- Check: operator step: connect a WRLD.Bldr and open the Controllers page

## MODIFIED Requirements

### Requirement: The MIDI configuration page fits this application's window in every state

The MIDI configuration page SHALL lay every control inside this application's content width on every host in every reachable state: controller rows collapsed and expanded, each configuration section open, and a mapping row in each group that accepts an added row (Turn, Push, System, Gesture, App action) beside the rows a preset installs. The controller header SHALL be two lines — identity (name, device label, and Model for a Launchpad) and ports (MIDI in and MIDI out, each preceded by its own status dot, then Delete) — and, while the row differs from its preset, a third line carrying the sentence that says so and the Restore action, which sits there and nowhere else. The page SHALL show a controller's device by its display name: the descriptor the row's wizard id resolves against, or the bound MIDI input's stored endpoint label when none resolves, SHALL caption the add row's preset selector "Preset" and a Launchpad row's model selector "Model", SHALL let a Launchpad row choose which Launchpad model it addresses and no other row choose anything of the sort, SHALL offer on the add row this application's presets followed by exactly one Custom entry, SHALL add the preset its add row displays when the operator has chosen none, which is the preset of the first connected device waiting to be set up when one is waiting, SHALL name an added controller after its preset (with a numeric suffix when the name is taken), SHALL bind an added controller's ports to a connected device whose port names match the preset, for every preset that matches the device, and otherwise leave them "(none)", SHALL open an added controller's row with every section open, SHALL keep the rename field inside the expanded editor under the caption "Name", SHALL keep a renamed controller's row expanded and its open sections open, SHALL caption the ports "MIDI in" and "MIDI out" with a legend for the status dots above the first controller, and SHALL show a controller's full name. A combo box or text field SHALL never draw past its own box.

<!-- RESTATES-EXCEPT
a Launchpad row shows a "Variant" selector on that first line
  keeps: selector on that first line, holding the model its profile records
`TestLaunchpadRowOffersVariantAndRetargetsItsPads` (the Variant selector on a Launchpad row's first line)
  keeps: on a Launchpad row's first line
-->

#### Scenario: Every state fits

- **WHEN** a Twister, a Generic and a Launchpad controller are configured,
  the Generic row is expanded with Encoders (a Turn and a Push row added),
  System Messages (a row added) and Analogs (a Gesture and an App action
  row added) open, the Launchpad row is expanded with System Messages
  open, and the Twister row is expanded with Encoders open
- **THEN** no control lies outside the page's content width in any of
  those states
- Check: `External/Sheaf/projects/synth/tests/portable_ui_tests.cpp`, `TestControllersRowFitsWithinFroggersNarrowestHost` (task 2.6); operator, task 7.1.

#### Scenario: The row reads as its parts

- **WHEN** the operator reads a MIDI Fighter Twister row
- **THEN** it shows "MIDI Fighter Twister" on the first line as both its name
  and its device label, and no preset selector, a preset being chosen once on
  the add row; a status dot before the "MIDI in" selector, a status dot before
  the "MIDI out" selector, and Delete on the second; no rename control in the
  header
- **AND** a Launchpad row shows a "Model" selector on that first line,
  holding the model its profile records
- Check: `External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp`, `TestControllerLifecycleActionsUseTheNormalCommitAndSavePath` (no rename control in a collapsed row's header), `TestLaunchpadRowOffersVariantAndRetargetsItsPads` (the model selector on a Launchpad row's first line), and `TestLaunchpadRowModelSelectorIsCaptionedModel` (its caption reads "Model"); the per-row status-dot-precedes-its-combo and MIDI-in/MIDI-out caption assertions this scenario also names are exercised only inline in this file's own `main()`, which this repository's case index does not resolve by name.

#### Scenario: Renaming keeps the editor open

- **WHEN** the operator expands a controller's editor, opens one of its
  sections, types a new name in the Name field and presses Rename
- **THEN** the controller is renamed, its row is still expanded, and the
  section it had open is still open
- **AND** deleting a controller and adding another with the same name opens
  the added row with every section open, rather than with the deleted row's
  state
- Check: `External/Sheaf/projects/synth/tests/viewmodel_tests.cpp`, `RenameOfExpandedRowKeepsSectionPresentationOpen` (rename preserves expansion and the open section) and `SameNameReaddAfterDeleteStartsFullyCollapsed` (a re-added name gets a fresh entry, not the deleted record's); `External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp`, `TestAddedRowOpensWithEverySectionOpen` (the page opens the added row); operator, task 7.2.

#### Scenario: Adding from a preset

- **WHEN** the operator presses Add on the add row with no Twister
  connected, having chosen nothing, and the add row displays
  "MIDI Fighter Twister"
- **THEN** a row named "MIDI Fighter Twister" appears whose device label reads
  MIDI Fighter Twister and whose ports read "(none)"
- **AND** with a Twister connected on both its ports, the same action
  binds both ports to it
- **AND** with only one of its ports present, both ports still read
  "(none)" and the operator picks the present one from its selector
- **AND** the added row opens with every section open
- Check: `External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp`, `TestAddFromPresetWithNoDeviceInstallsTheDefaultPresetWithNoneEndpoints` and `TestAddFromPresetWithMatchingOnlinePairBindsBothEndpoints` (task 2.5), and `TestAddedRowOpensWithEverySectionOpen`; operator, task 7.3.

#### Scenario: A page change rebuilds every test that reads the page

- **WHEN** `include/synth/ControllersPageUI.hpp` changes and the test
  binaries are built
- **THEN** every binary whose translation unit includes that header is
  relinked from the changed source rather than reported up to date
- **AND** a binary built from more than one translation unit is rebuilt
  when a header reached by any one of them changes, not only the last
- Check: not yet delivered as a standing case. The prior wording named `app/Makefile`, which carries no such mechanism; the generated depfiles (`DEPFLAGS := -MMD -MP`) that make this true live in `External/Sheaf/projects/synth/Makefile`, on the five Sheaf test-binary rules and the split BrowserRuntimeAbi.o rule, proven once by a manual two-leg positive control (touching `External/Sheaf/projects/synth/include/synth/ControllersPageUI.hpp` rebuilt the dependent binary where the prior hand-written list reported it up to date). Sheaf's own analogous scenario records the identical one-time proof and cites no automated case either. Delivering this as a standing check means a script that dirties a leaf header, rebuilds, and asserts the expected object's mtime moved.

#### Scenario: A selector's text stays in its box

- **WHEN** a controller's Preset selector shows "MIDI Fighter Twister" in
  the browser build
- **THEN** the selector fills exactly its box and clips its text
- Check: `External/Sheaf/projects/synth/browser/tests/ui-backend.spec.ts`, `renders portable controls, canvas draws, and reachable scroll content`, whose select-fills-wrapper assertion (the combo's select child is sized to match its own wrapper's width) is the mechanism a same-width select clips its own text by.

### Requirement: Each controller row control does one job
The MIDI configuration page SHALL offer exactly one control that lists devices — the add row's selector — and SHALL NOT offer a device or preset list on a configured row. A configured row SHALL NOT offer a preset selector, and its device label names the preset that created it; it SHALL offer a Restore action, and only while it was created from a preset and its stored configuration differs from that preset, and SHALL say on that row, in words, that the row differs from its preset and what Restore does to it, so that the operator learns this by reading the row rather than by inferring it from a button's presence. The page SHALL leave the row's stored mappings as they are until the operator presses Restore, since a row that differs from its preset is either a row an improved preset has left behind or a row the operator edited on purpose, and the page cannot tell the two apart. Every distinct device or operating mode SHALL be its own preset, chosen once when the row is created; the page SHALL NOT offer a second control asking which model or mode a row is. A released row, which a configuration saved by an earlier version can hold, SHALL show its Released badge and stored ports and SHALL offer Delete.

#### Scenario: A row never offers another device's preset
- **WHEN** a MIDI Fighter Twister row is presented
- **THEN** no control on that row offers an Akai APC40 preset, or any preset for a kind other than the row's own
- **AND** the only control listing devices anywhere on the page is the add row's selector
- Check: not yet delivered as a resolvable case. controllers_page_ui_tests.cpp's row-control tests cover a Twister row never showing another kind's preset, which the deleted preset combo (task 2.3, zero remaining references to ControllerLayout/kControllerLayout) makes true by construction but which no test asserts by name. The page-wide half — the add row's selector is the only device-listing control on the page — no longer holds: Sheaf commit ba3898e4 ("Let a Launchpad row say which Launchpad it is", branch launchpad-model-on-the-row) reintroduced a per-row Variant selector for Launchpad rows, exercised by TestLaunchpadRowOffersVariantAndRetargetsItsPads. This scenario needs the operator's decision (permit the Variant selector as an exception, or reconsider it) before either half can be marked delivered. Operator, tasks 6.1 and 6.3, predate that commit.

#### Scenario: Restore appears only when there is something to restore
- **WHEN** a row created from a preset has had a mapping edited
- **THEN** the row offers Restore, and its device label still names the preset that created it
- **AND** pressing Restore reinstalls that row's own preset
- **AND** a row whose configuration still matches its preset offers no Restore
- **AND** a row that was never created from a preset offers none either
- **AND** editing a mapping and setting it back by hand withdraws Restore again
- Check: `External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp`, `TestRestoreReinstallsADivergedPresetAndIsGatedByDivergence`, which pins Restore absent on an untouched preset row, absent on a row never created from a preset, present once diverged, and gone again once the row matches its preset (there, by pressing Restore itself); operator, task 6.2, covers the by-hand-edit-reverts-it variant this test does not drive.
- Check: operator step: the Delivery Gate screenshots of an old Twister row before and after Restore, both labelled 'MIDI Fighter Twister'

#### Scenario: A device model is chosen once, as a preset
- **WHEN** the add row is opened
- **THEN** each Launchpad model is listed as its own preset, alongside the Twister and each APC40 mode
- **AND** no control anywhere on a created row asks which model or mode that row is
- **AND** a row created from a preset carrying a connect-time message sends exactly that message when its output connects, and one created from a preset without such a message sends none
- Check: the add row's preset list (each Launchpad model, Twister, and each APC40 mode) is operator-verified only (task 6.1a); frogg3rs's own Launchpad presets live in app/FroggersMidiCatalog.hpp, outside Sheaf's test tree. "No control anywhere on a created row asks which model or mode" no longer holds now that Sheaf commit ba3898e4 (branch launchpad-model-on-the-row) restored a per-row Variant selector for Launchpad rows (TestLaunchpadRowOffersVariantAndRetargetsItsPads), which needs the operator's decision. The connect-time-message half is backed: `External/Sheaf/projects/synth/tests/instrument_tests.cpp`'s `CreateMidiControllerProfileWiresOpenSysExToConnectTimeOutput` proves a preset's openSysEx message sends exactly once on connect and an empty one sends none. Operator, tasks 6.1a and 6.1b, predate that commit.

#### Scenario: A row that differs from its preset says so
- **WHEN** a row created from a preset holds mappings that differ from the ones that preset installs now
- **THEN** the row shows the line "This row differs from its preset. Restore replaces its mappings and discards any edits."
- **AND** Restore sits on that same line, beside the words that describe it
- Check: `External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp`, `TestRestoreReinstallsADivergedPresetAndIsGatedByDivergence`: after the mapping edit that diverges row 2, the node `NodeIds::ControllerPresetNotice(2)` is present, its text equals that sentence character for character, and the notice and the row's `NodeIds::ControllerRestore` node are both children of the row's third header line.

#### Scenario: A row that matches its preset says nothing
- **WHEN** a row's mappings are still the ones its preset installs, or the row was never created from a preset
- **THEN** the row shows no such line, and its header is two lines
- **AND** pressing Restore on a row that differs withdraws the line along with the action
- Check: `External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp`, `TestRestoreReinstallsADivergedPresetAndIsGatedByDivergence`: `NodeIds::ControllerPresetNotice(0)` (untouched preset row) and `NodeIds::ControllerPresetNotice(1)` (row never created from a preset) are absent, both rows measure `ControllersLayout::kControllerHeaderHeight` tall, and after the Restore dispatch `NodeIds::ControllerPresetNotice(2)` is absent and row 2 measures `kControllerHeaderHeight` too.

#### Scenario: A row the operator edited keeps its edits
- **WHEN** the operator has edited a mapping on a row created from a preset and has not pressed Restore
- **THEN** the row still holds the edited mapping
- **AND** the line names what Restore would discard, so the operator chooses whether to take the preset's mappings instead
- Check: `External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp`, `TestRestoreReinstallsADivergedPresetAndIsGatedByDivergence`: between the turn-step commit and the Restore dispatch, the edited turn's step in the harness instrument's third controller is still 0.25 and the notice text contains "discards any edits".
