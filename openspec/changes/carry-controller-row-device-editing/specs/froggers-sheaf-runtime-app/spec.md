## MODIFIED Requirements

### Requirement: The MIDI configuration page fits this application's window in every state

The MIDI configuration page SHALL lay every control inside this application's content width on every host in every reachable state: controller rows collapsed and expanded, each configuration section open, and a mapping row in each group that accepts an added row (Turn, Push, System, Gesture, App action) beside the rows a preset installs. The controller header SHALL be two lines: identity (name, device label, and Variant for a Launchpad) and ports (MIDI in and MIDI out, each preceded by its own status dot, then Delete and Blacklist). The page SHALL show a controller's device by its display name: the descriptor the row's wizard id resolves against, or the bound MIDI input's stored endpoint label when none resolves, SHALL caption the add row's preset selector "Preset" and a Launchpad row's model selector "Variant", SHALL let a Launchpad row choose which Launchpad model it addresses and no other row choose anything of the sort, SHALL offer on the add row this application's presets followed by exactly one Custom entry, SHALL add the preset its add row displays when the operator has chosen none, SHALL name an added controller after its preset (with a numeric suffix when the name is taken), SHALL bind an added controller's ports to a connected device that matches the preset and otherwise leave them "(none)", SHALL keep the rename field inside the expanded editor under the caption "Name", SHALL keep a renamed controller's row expanded and its open sections open, SHALL caption the ports "MIDI in" and "MIDI out" with a legend for the status dots above the first controller, and SHALL show a controller's full name. A combo box or text field SHALL never draw past its own box.

<!-- RESTATES-EXCEPT
it shows "MIDI Fighter Twister" and "MF Twister" on the first line
  keeps: no rename control in the header
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
  the "MIDI out" selector, Delete and Blacklist on the second; no rename
  control in the header
- **AND** a Launchpad row shows a "Variant" selector on that first line,
  holding the model its profile records
- Check: `External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp`, `TestControllerLifecycleActionsUseTheNormalCommitAndSavePath` (no rename control in a collapsed row's header) and `TestLaunchpadRowOffersVariantAndRetargetsItsPads` (the Variant selector on a Launchpad row's first line); the per-row status-dot-precedes-its-combo and MIDI-in/MIDI-out caption assertions this scenario also names are exercised only inline in this file's own `main()`, which this repository's case index does not resolve by name. Operator, task 7.1.

#### Scenario: Renaming keeps the editor open

- **WHEN** the operator expands a controller's editor, opens one of its
  sections, types a new name in the Name field and presses Rename
- **THEN** the controller is renamed, its row is still expanded, and the
  section it had open is still open
- **AND** deleting a controller and adding another with the same name
  still starts that row fully collapsed
- Check: `External/Sheaf/projects/synth/tests/viewmodel_tests.cpp`, `RenameOfExpandedRowKeepsSectionPresentationOpen` (rename preserves expansion and the open section) and `SameNameReaddAfterDeleteStartsFullyCollapsed` (delete, then re-add under the same name, starts collapsed); operator, task 7.2.

#### Scenario: Adding from a preset

- **WHEN** the operator presses Add on the add row with no Twister
  connected, having chosen nothing, and the add row displays
  "MIDI Fighter Twister"
- **THEN** a row named "MIDI Fighter Twister" appears whose Preset reads
  MIDI Fighter Twister and whose ports read "(none)"
- **AND** with a Twister connected on both its ports, the same action
  binds both ports to it
- **AND** with only one of its ports present, both ports still read
  "(none)" and the operator picks the present one from its selector
- Check: `External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp`, `TestAddFromPresetWithNoDeviceInstallsTheDefaultPresetWithNoneEndpoints` and `TestAddFromPresetWithMatchingOnlinePairBindsBothEndpoints` (task 2.5); operator, task 7.3.

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

