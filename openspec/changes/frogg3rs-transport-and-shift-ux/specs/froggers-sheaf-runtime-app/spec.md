# Delta — `froggers-sheaf-runtime-app`

In "Each controller row control does one job", the "Restore appears only
when there is something to restore" scenario's clause "names no preset
anywhere on it" is false: the requirement's own text says a configured
row's device label names the preset that created it, and the page's device
label, `ControllersLayout::ControllerDeviceLabel`, names the preset whenever
the row offers Restore. The clause now says so, and the scenario gains a
Check line for it. Every other clause and scenario is carried forward word
for word, including the two scenarios whose Checks await the operator's
decision on the Launchpad Variant selector. The RESTATES-EXCEPT block under
the requirement names the promoted clause it changes, and
`app/check_modified_requirements_restate_promoted.py` checks it.

## MODIFIED Requirements

### Requirement: Each controller row control does one job
The MIDI configuration page SHALL offer exactly one control that lists devices — the add row's selector — and SHALL NOT offer a device or preset list on a configured row. A configured row SHALL NOT offer a preset selector, and its device label names the preset that created it; it SHALL offer a Restore action, and only while it was created from a preset and its stored configuration differs from that preset, so that the action's presence is itself the signal that the row has been edited. Every distinct device or operating mode SHALL be its own preset, chosen once when the row is created; the page SHALL NOT offer a second control asking which model or mode a row is. A released row, which a configuration saved by an earlier version can hold, SHALL show its Released badge and stored ports and SHALL offer Delete.

<!-- RESTATES-EXCEPT
names no preset anywhere on it
  keeps: the row offers Restore
-->

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
