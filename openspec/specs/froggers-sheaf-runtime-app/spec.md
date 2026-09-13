# froggers-sheaf-runtime-app Specification

## Purpose
Froggers satisfies `synth::SynthApplication`; it runs under Sheaf Runtime via `sheaf-patch` launcher registration and under the browser host via the browser app entry macro, with a JUCE-free app core and the Daisy firmware under `src/` left untouched.
## Requirements
### Requirement: Froggers is a Sheaf SynthApplication
The Froggers app SHALL be a type satisfying `synth::SynthApplication` — providing `Config()`, `Init(context)`, `ProcessBlock(block)`, and `PortableSurface()` — and SHALL assert that conformance at compile time. The app core SHALL NOT depend on JUCE.

#### Scenario: Concept conformance is enforced at build time
- **WHEN** the app target is compiled
- **THEN** a static assertion confirms the app type satisfies `synth::SynthApplication`
- **THEN** the app core translation units include no JUCE header

#### Scenario: Headless process produces audio
- **WHEN** a test harness calls `Init` then `ProcessBlock` without any host shell
- **THEN** the block is filled with finite stereo samples

### Requirement: Desktop hosting through the Sheaf launcher
Froggers SHALL be launchable on desktop by registering with the `sheaf-patch` launcher, reaching `Runtime<App>` through the launcher's generic registration path. Froggers SHALL NOT define its own JUCE application entry point.

#### Scenario: App appears in the launcher and runs
- **WHEN** the operator selects Froggers in the `sheaf-patch` launcher
- **THEN** a Runtime session is created for the Froggers app
- **THEN** audio processes and the portable surface renders

### Requirement: Browser hosting through the Sheaf browser ABI
The same app type SHALL be hostable in the Sheaf browser/patcher host via the browser app entry macro, with no app-core changes between the desktop and browser hosts.

#### Scenario: One app core serves both hosts
- **WHEN** the browser build is produced from the same app type as the desktop build
- **THEN** no host-specific branching exists in the app core
- **THEN** both hosts drive the identical `ProcessBlock` and `PortableSurface`

### Requirement: Operator documentation ships with the app
THE app SHALL carry its manual and quick dictionary locally in every host
it ships in — standalone, VST3 and AU — and SHALL let the operator open
both from inside the app without a network connection. The documents SHALL
be embedded from the repository's single copy at build time, so that no
second checked-in copy exists to drift from the first. The browser build
MAY instead link to the published documents, because it is already running
in a browser with the network available.

Neither the way the operator reaches the documents nor the way the app
locates them SHALL assume a single platform's conventions. Where a host's
platform has no equivalent of the macOS main menu, the app SHALL present the
same entries by that platform's own means; where it has no application
bundle, the app SHALL find the documents where that platform's build places
them.

#### Scenario: Reading the manual offline in a DAW
- **WHEN** the plugin is loaded in a DAW on a machine with no network
- **THEN** the operator can open the manual and the quick dictionary from
  the plugin itself
- **AND** the content matches the repository's copy for that build

#### Scenario: The standalone app carries its own documentation
- **WHEN** the standalone app is opened with no network
- **THEN** both documents are reachable from inside the app

#### Scenario: Every shipped standalone platform reaches its documents
- **WHEN** the standalone app is opened on any platform it is released for
- **THEN** the manual and quick dictionary open from inside the app
- **AND** the files opened are the ones that build placed, not a path that
  resolves only on the platform the feature was written on

#### Scenario: One copy, not two
- **WHEN** the manual or the quick dictionary is edited in the repository
- **THEN** the next build carries the edit
- **AND** no checked-in duplicate of either document has to be re-synced

### Requirement: The runtime chrome reports its load honestly

The load readout in the runtime sidebar SHALL hold its peak only briefly enough
that a reader can take it for the current load without being wrong.

It shows the highest sample over a recent window, so that a transient spike stays
visible long enough to be read — worth keeping, because a spike is what produces
an audible click. But the window SHALL be short enough that a peak does not
outlive the condition that caused it: a startup transient still on screen seconds
after the instrument went idle reports an overload that is not happening.

The window SHALL be expressed in time rather than in frames, because the UI tick
rate is per-application configuration and the same frame count is a different
hold on different hosts.

The displayed precision SHALL NOT exceed the precision the figure has. A held
maximum is not accurate to a tenth of a percent.

The readout SHALL render within the sidebar's own width at every value it can
take, including three digits.

#### Scenario: A stale peak does not outlive its cause
- **WHEN** the load spikes and then returns to a lower steady level
- **THEN** the readout returns to that level within the window's own span

#### Scenario: A spike is still visible
- **WHEN** a single sample spikes
- **THEN** the readout shows it rather than averaging it away

#### Scenario: It fits the column it renders in
- **WHEN** the load readout renders at three digits
- **THEN** it renders within the sidebar's width without truncation

#### Scenario: A held peak is not reported to a tenth of a percent
- **WHEN** the load readout renders
- **THEN** it shows whole percent

### Requirement: The shipped documentation addresses someone learning the instrument

`README.md` and `MANUAL.md` SHALL be written for a reader finding out what this
instrument is and how to play it.

Build instructions for a frozen hardware target, toolchain paths, flashing
procedures and repository working process SHALL NOT occupy the README's body.
Where that material is still true it belongs in the document that already covers
that target; where it describes something that no longer ships it goes.

The README SHALL open by saying what the instrument is and what is unusual about
it, and a reader SHALL be able to reach the parameter reference without reading
past material addressed to someone building the project.

#### Scenario: The README leads with the instrument
- **WHEN** a reader opens the README
- **THEN** what the instrument is, and what distinguishes it, comes before
  anything about building it

#### Scenario: Firmware build detail is not in the README body
- **WHEN** the README is read end to end
- **THEN** it contains no toolchain installation paths, DFU addresses, or linker
  script selection, all of which live in the hardware target's own manual

#### Scenario: Repository working process is not operator documentation
- **WHEN** the README is read end to end
- **THEN** it does not document planning artifacts, agent conventions, or where
  shared source belongs

### Requirement: Form controls are sized to what they contain

A button in a runtime configuration page SHALL be sized to its own label rather
than stretched to the width of the page's control column, and SHALL sit at the
control column's left edge. A two-word button rendered several hundred pixels
wide reads as a mistake, and invites the reader to look for the rest of it.

This SHALL hold on every host. The defect is in the shared form-grid layout, not
in one backend: a combo box hides it because the browser draws a `select` at its
own width regardless of its box, while a button fills whatever box it is given.

Controls that genuinely want the column's width — a text field, a device
selector — SHALL keep it. Filling the column SHALL be a choice a control makes,
and SHALL remain what a control that declares nothing does, so that adding this
capability changes no page that does not ask for it.

#### Scenario: A captioned form button is label-width
- **WHEN** a configuration page renders a captioned button
- **THEN** its width is close to its label's width rather than the column's

#### Scenario: A narrower control still starts where the column starts
- **WHEN** that button is rendered beside full-width device selectors
- **THEN** its left edge is the same as theirs

#### Scenario: The controls that want the column keep it
- **WHEN** the same page renders its device selectors
- **THEN** their cells still span the control column as before

### Requirement: A control's declared width survives to the control

A control style that declares a width SHALL have that declaration reach the
control node, not only the caption row that wraps it.

Where a style carries one layout declaration, the layout SHALL NOT silently
spend it on the wrapper and substitute a default for the control. A control that
declares nothing SHALL be laid out exactly as before.

The form grid SHALL still guarantee that a row's cells fit the row. Sizing a
control to its content SHALL NOT be able to make a row overflow, and a control
whose content is wider than the column available to it SHALL be held to that
column rather than escaping it.

#### Scenario: An undeclared control is unchanged
- **WHEN** a control style declares no width for the control itself
- **THEN** the control fills the control column exactly as it did before

#### Scenario: A declaration is not consumed by the wrapper
- **WHEN** a captioned control declares a content-sized width
- **THEN** the control node is content-sized and the row's own height is
  unaffected

#### Scenario: A content-sized control cannot overflow its row
- **WHEN** a control's content is wider than the column it is given
- **THEN** it is held to the column and the row still holds its children

### Requirement: Like-type controls share column positions

A form grid SHALL align its participating rows to shared column positions:
every row SHALL put its n-th cell at the same x offset, so the page reads down
a straight edge.

This capability had no written rule for that; it lived only in a test
criterion. It is stated here as ADDED rather than MODIFIED because there was
nothing in this spec to modify — which is why the rule could be changed twice
without anyone reading it first.

Cells in a column SHALL NOT be required to share a width. Control width is a
per-control declaration: a control that declares nothing fills the column, and
one that declares a content width is narrower and left-aligned within it. A
column of mixed widths sharing a left edge is the intended rendering, not a
violation.

#### Scenario: A column holds its left edge
- **WHEN** a form grid renders rows whose controls declare different widths
- **THEN** every control cell in the column reports the same x offset

#### Scenario: Differing widths in a column are not a violation
- **WHEN** one row's control is content-sized and another's fills the column
- **THEN** the alignment check reports no violation

### Requirement: A rendered control is routable

Every action a runtime page can emit SHALL be routed by the host that renders
that page. A control that renders and dispatches into nothing SHALL be caught by
a check rather than by an operator finding it inert.

Where a page's actions form a fixed set, that set SHALL have one definition,
read by both the page that emits from it and the host that routes from it. Two
lists expected to agree are the defect: adding a control to one of them is not
required to touch the other, and the button ships live and dead at once.

A page whose routing rule is not a fixed set — one that also admits actions by
prefix — SHALL share the fixed half and keep the prefix rule, which membership
cannot express.

#### Scenario: A page cannot emit an unroutable action
- **WHEN** the Audio, File or Sync page's tree is built with every control shown
- **THEN** every action it emits appears in the list its host routes from

#### Scenario: The page and the host read one list
- **WHEN** the host decides whether an action belongs to the Audio, File, Sync
  or sidebar surface
- **THEN** it reads that surface's own action list rather than restating it

#### Scenario: Removing an action from the shared list is caught
- **WHEN** an action a page emits is removed from that page's action list
- **THEN** a check fails naming the page that emits it

### Requirement: Each controller row control does one job
The MIDI configuration page SHALL offer exactly one control that lists devices — the add row's selector — and SHALL NOT offer a device or preset list on a configured row. A configured row SHALL NOT name a preset at all; it SHALL offer a Restore action, and only while it was created from a preset and its stored configuration differs from that preset, so that the action's presence is itself the signal that the row has been edited. Every distinct device or operating mode SHALL be its own preset, chosen once when the row is created; the page SHALL NOT offer a second control asking which model or mode a row is. A row SHALL offer to release a bound controller whenever a device is bound to it, and SHALL NOT offer that control otherwise.

#### Scenario: A row never offers another device's preset
- **WHEN** a MIDI Fighter Twister row is presented
- **THEN** no control on that row offers an Akai APC40 preset, or any preset for a kind other than the row's own
- **AND** the only control listing devices anywhere on the page is the add row's selector
- Check: not yet delivered as a resolvable case. controllers_page_ui_tests.cpp's row-control tests cover a Twister row never showing another kind's preset, which the deleted preset combo (task 2.3, zero remaining references to ControllerLayout/kControllerLayout) makes true by construction but which no test asserts by name. The page-wide half — the add row's selector is the only device-listing control on the page — no longer holds: Sheaf commit ba3898e4 ("Let a Launchpad row say which Launchpad it is", branch launchpad-model-on-the-row) reintroduced a per-row Variant selector for Launchpad rows, exercised by TestLaunchpadRowOffersVariantAndRetargetsItsPads. This scenario needs the operator's decision (permit the Variant selector as an exception, or reconsider it) before either half can be marked delivered. Operator, tasks 6.1 and 6.3, predate that commit.

#### Scenario: Restore appears only when there is something to restore
- **WHEN** a row created from a preset has had a mapping edited
- **THEN** the row offers Restore, and names no preset anywhere on it
- **AND** pressing Restore reinstalls that row's own preset
- **AND** a row whose configuration still matches its preset offers no Restore
- **AND** a row that was never created from a preset offers none either
- **AND** editing a mapping and setting it back by hand withdraws Restore again
- Check: `External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp`, `TestRestoreReinstallsADivergedPresetAndIsGatedByDivergence`, which pins Restore absent on an untouched preset row, absent on a row never created from a preset, present once diverged, and gone again once the row matches its preset (there, by pressing Restore itself); operator, task 6.2, covers the by-hand-edit-reverts-it variant this test does not drive.

#### Scenario: A device model is chosen once, as a preset
- **WHEN** the add row is opened
- **THEN** each Launchpad model is listed as its own preset, alongside the Twister and each APC40 mode
- **AND** no control anywhere on a created row asks which model or mode that row is
- **AND** a row created from a preset carrying a connect-time message sends exactly that message when its output connects, and one created from a preset without such a message sends none
- Check: the add row's preset list (each Launchpad model, Twister, and each APC40 mode) is operator-verified only (task 6.1a); frogg3rs's own Launchpad presets live in app/FroggersMidiCatalog.hpp, outside Sheaf's test tree. "No control anywhere on a created row asks which model or mode" no longer holds now that Sheaf commit ba3898e4 (branch launchpad-model-on-the-row) restored a per-row Variant selector for Launchpad rows (TestLaunchpadRowOffersVariantAndRetargetsItsPads), which needs the operator's decision. The connect-time-message half is backed: `External/Sheaf/projects/synth/tests/instrument_tests.cpp`'s `CreateMidiControllerProfileWiresOpenSysExToConnectTimeOutput` proves a preset's openSysEx message sends exactly once on connect and an empty one sends none. Operator, tasks 6.1a and 6.1b, predate that commit.

#### Scenario: Releasing a controller frees it and keeps its mappings
- **WHEN** a row with both endpoints bound is released
- **THEN** its open endpoints are closed, its stored references are retained, and another application can take the device
- **AND** reclaiming it restores its mappings
- **AND** a row with no bound device offers no release control at all, rather than a disabled one
- Check: `External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp`'s `TestControllerLifecycleActionsUseTheNormalCommitAndSavePath` proves a release closes both endpoints and retains the profile as dormant data, and `TestReleaseRequiresResolvedWizardAndBoundEndpoints` proves a row with no bound device offers no release control at all. "Reclaiming it restores its mappings" is not backed, and reading the code says why: RemoveFromBlacklist (`External/Sheaf/projects/synth/src/MidiConfigViewModel.cpp`), which the Reclaim action calls, removes the record outright rather than restoring it — matching Sheaf's own `External/Sheaf/openspec/changes/app-midi-catalog/specs/synth-runtime-ui/spec.md` scenario "Reclaim restores availability": Reclaim discards the inert record and frees its device pair for a fresh Add, it does not bring this record's mappings back. This needs the operator's correction to the requirement, not a test. Operator, task 6.5, exercised Restore and Release; its Reclaim observation was not re-verified against this reading.

### Requirement: A row remembers which preset created it
A controller row SHALL retain the identity of the preset that created it for as long as the row exists, and editing the row's mappings SHALL NOT discard that identity. Whether the row still matches that preset SHALL be determined by comparing the row's stored configuration against the preset's generated configuration, rather than by treating the recorded identity as a marker of an unedited row. Controls that depend on a row resolving to a known preset SHALL remain available after the row's mappings have been edited.

#### Scenario: An edited row keeps its provenance
- **WHEN** a mapping on a row created from a preset is edited, deleted, or added to
- **THEN** the row still resolves to the preset that created it
- **AND** the row is reported as differing from that preset
- Check: `External/Sheaf/projects/synth/tests/viewmodel_tests.cpp`, `ApplyMappingEditKeepsWizardIdProvenance`, `DeleteRowKeepsWizardIdProvenance`, `AddSingleKeepsWizardIdProvenance`, `AddBlockKeepsWizardIdProvenance` — one case per verb this scenario names (edited, deleted, added). Each pins that the row still resolves to its creating preset; that the edited row is then reported as differing from it is the divergence flag Restore's own gating proves (see the Restore scenario above).

#### Scenario: Editing a row does not withdraw its other controls
- **WHEN** a row with both endpoints bound has one of its mappings edited
- **THEN** the row still offers to release the bound controller
- **AND** a released row that has been edited still offers Configure
- Check: `External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp`, `TestReleaseRequiresResolvedWizardAndBoundEndpoints`, which drives a mapping edit through the real per-field commit path and then finds Release still present; and `TestConfigureStaysAvailableOnAReleasedEditedRow`, which releases an edited row and finds Configure still present. Operator, task 6.5.

### Requirement: The MIDI configuration page fits this application's window in every state

The MIDI configuration page SHALL lay every control inside this application's content width on every host in every reachable state: controller rows collapsed and expanded, each configuration section open, and a mapping row in each group that accepts an added row (Turn, Push, System, Gesture, App action) beside the rows a preset installs. The controller header SHALL be two lines: identity (name, device kind, and Variant for a Launchpad) and ports (MIDI in and MIDI out, each preceded by its own status dot, then Delete and Blacklist). The page SHALL show a controller's device kind by its display name, SHALL caption the add row's preset selector "Preset" and a Launchpad row's model selector "Variant", SHALL let a Launchpad row choose which Launchpad model it addresses and no other row choose anything of the sort, SHALL offer on the add row this application's presets followed by a Custom entry per device kind and nothing else, SHALL add the preset its add row displays when the operator has chosen none, SHALL name an added controller after its preset (with a numeric suffix when the name is taken), SHALL bind an added controller's ports to a connected device that matches the preset and otherwise leave them "(none)", SHALL keep the rename field inside the expanded editor under the caption "Name", SHALL keep a renamed controller's row expanded and its open sections open, SHALL caption the ports "MIDI in" and "MIDI out" with a legend for the status dots above the first controller, and SHALL show a controller's full name. A combo box or text field SHALL never draw past its own box.

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
- **THEN** it shows "MIDI Fighter Twister" and "MF Twister" on the first line
  and no preset selector, a preset being chosen once on the add row; a status
  dot before the "MIDI in" selector, a status dot before the "MIDI out"
  selector, Delete and Blacklist on the second; no rename control in the header
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

### Requirement: The library is tested on the target the product ships to
The library's own test binaries SHALL be built and run for the browser's wasm32 target as part of the test gate, so that behaviour depending on the width of `std::size_t` is exercised at the width the shipping build uses. The gate SHALL fail, rather than skip, when the toolchain that builds for that target is unavailable. Arithmetic guarding an index or a count SHALL be correct on every target the code is built for, independent of word size.

#### Scenario: A width-dependent defect fails the gate
- **WHEN** a size bound or overflow guard behaves differently under a 32-bit `std::size_t` than under a 64-bit one
- **THEN** the test gate fails on the wasm32 target
- **AND** the failure names the function whose behaviour differs
- Check: `External/Sheaf/projects/synth/tests/blocks_tests.cpp`'s `ExpandEncoderBlockRejectsStartPositionNearSizeMaxThatWouldWrap`, `ExpandAnalogBlockRejectsStartGestureIxNearSizeMaxThatWouldWrap` and `ExpandSystemBlockRejectsStartArgNearSizeMaxThatWouldWrap` are the width-dependent cases, proven live in both directions: with the pre-fix guard restored they are among 23 failures under wasm32, and after the block validator's overflow guard is corrected the wasm32 binaries run to 249 passes and 0 failures. The gate itself is `test-wasm32` in External/Sheaf/projects/synth/Makefile, wired into that Makefile's own `test` target; this repository's gate script only reads app/Makefile for wired-gate scripts, so no citation naming it resolves as a gate here. The gate is fail-fast, so the 26 failures the same guard causes in External/Sheaf/projects/synth/tests/viewmodel_tests.cpp are measured by building that binary for the target directly, not through the gate.

#### Scenario: Adding an encoder mapping succeeds in the browser build
- **WHEN** a controller row created from a preset has Add or Block pressed in the Encoders editor
- **THEN** the mapping or block is added, in the browser build as in every other
- **AND** the same holds on a row with no existing mappings, and in the Analogs and System Messages sections
- Check: `External/Sheaf/projects/synth/tests/blocks_tests.cpp`'s `ExpandEncoderBlockProducesConsecutiveCcToPositionMapping` (Encoders), `ExpandAnalogBlockProducesConsecutiveCcToGestureMapping` (Analogs) and `ExpandSystemBlockGenericSceneSelectProducesCcRunWithFeedbackEqualsPress` (System Messages) exercise exactly these add paths on wasm32, among the cases that fail before the guard is corrected and pass after; External/Sheaf/projects/synth/tests/viewmodel_tests.cpp's own failures under the same guard are measured by building that binary for the target directly, per the scenario above. And the operator on the deployed build.

### Requirement: A control's label is legible and its neighbours are separated
A text node SHALL be allocated a box wide enough for the text it renders, and adjacent controls within a row SHALL be separated by a non-zero gap, so that no label is clipped by, or visually continuous with, the control beside it. This SHALL be checked by a criterion applied to the page's rendered states, measuring text width finely enough to catch a sub-pixel overrun, rather than by inspection.

#### Scenario: A column header is not clipped by the button beside it
- **WHEN** the Encoders editor's Turn or Push group header is presented
- **THEN** every column label is fully legible
- **AND** a gap separates the last column from the Add button
- Check: `External/Sheaf/projects/synth/browser/tests/visual-criteria.spec.ts`, `the controller row's Encoders group header meets the structural criteria across a range of widths`, which asserts the "Start Pos" column header renders twice (Turn and Push) and is the text-fit criterion extended to the row-expanded/Encoders-open state, proven to fail on the 58px `BlockStartPos` allocation and the zero gap before they are corrected; and the operator on the deployed build.

#### Scenario: A rounding-width overrun is not reported as fitting
- **WHEN** a label's rendered text exceeds its allocated box by less than one pixel
- **THEN** the text-fit criterion reports a violation
- Check: `External/Sheaf/projects/synth/browser/tests/visual-criteria.spec.ts`, `every text element fits its allocated extent`, which measures `textNeededWidth`/`textAvailableWidth` rather than `scrollWidth`/`clientWidth`; those two are integers and both read 58 for the 58.3px "Start Pos" label, so the integer comparison this replaces reports no violation on a live defect.

### Requirement: A preset's port aliases are per direction and name what the host reports

Each device preset this application offers SHALL carry its input aliases and its output aliases separately, and each list SHALL include the endpoint name the host reports for that direction, because the page pairs a connected unit with a preset by whole-string, case-insensitive equality against that name. Where a unit's two endpoints are named differently — a device whose ports carry a direction word, so that the input reads "Out" and the output reads "In" — the two lists SHALL differ accordingly. A preset MAY additionally carry shorter forms for hosts that report them, and SHALL NOT carry an alias that would pair the unit's DAW port when its MIDI port is the one the preset drives.

#### Scenario: A connected Launchpad is offered its own preset

- **WHEN** a Launchpad Mini MK3 is connected and the Controllers page enumerates its ports as "Launchpad Mini MK3 LPMiniMK3 MIDI Out" and "Launchpad Mini MK3 LPMiniMK3 MIDI In"
- **THEN** the page pairs it with the Launchpad Mini MK3 preset
- **AND** the unit's DAW ports pair with no preset
- Check: `app/FroggersControllersPageTests.cpp`, `launchpad_presets_pair_with_the_port_names_a_host_reports`.

#### Scenario: The name a host reports is read from the host

- **WHEN** an alias is written for a unit
- **THEN** it is the name the host enumerates, read from that host rather than
  constructed from a manual
- Check: `app/FroggersControllersPageTests.cpp`, `launchpad_presets_pair_with_the_port_names_a_host_reports`; the Mini MK3's
  rows of its table were read by calling `getAvailableDevices()` through this
  application's own JUCE with the unit connected, the other two models' rows
  are the same construction and are unconfirmed.

