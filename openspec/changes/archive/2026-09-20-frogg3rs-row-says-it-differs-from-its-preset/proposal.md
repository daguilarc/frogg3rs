# Proposal — `frogg3rs-row-says-it-differs-from-its-preset`

## Why

A player set up a MIDI Fighter Twister row before the Twister preset learned
Shift + Crunchy. The preset improved; their row did not. Holding Shift and
turning Crunchy moved Crunchy and left the scene blend alone, and nothing on
the Controllers page said why. The row looked finished: its name, its device
label, its ports, a Delete button and a Restore button. Restore is the fix,
and it reads like an undo button for something the player does not remember
doing.

A row keeps the mappings it was saved with. The runtime config is loaded from
its JSON and assigned over the live instrument
(`External/Sheaf/projects/synth/src/PatchPersistence.cpp:220`, reaching
`instrumentConfig_` at
`External/Sheaf/projects/synth/include/synth/Engine.hpp:770`), so an
improvement to a preset reaches only rows created after it.

The page already knows. Every rebuild asks whether a row's stored
configuration is still exactly what its preset generates today, by
regenerating that preset into a scratch slot and comparing the serialized
JSON (`SlotMatchesWizardProfile`,
`External/Sheaf/projects/synth/src/MidiConfigViewModel.cpp:674-704`,
read into `matchesWizardProfile` at
`src/MidiConfigViewModel.cpp:932`). The page spends that answer on one
decision: whether to draw the Restore button
(`External/Sheaf/projects/synth/include/synth/ControllersPageUI.hpp:2707-2716`).
A player cannot read an answer that is only ever spelled as the presence of a
control. So the page tells them.

## What Changes

A controller row grows a third header line while it differs from its preset,
on the condition the Restore button already uses. The line holds one sentence:

> This row differs from its preset. Restore replaces its mappings and discards any edits.

Restore moves from the second line onto this one, beside the words that
describe it. A row that matches its preset, and a row that was never created
from one, keep the two-line header they have now and say nothing.

The page still changes nothing on its own. A row that differs is either a row
an improved preset left behind or a row the player edited on purpose, and
`SlotMatchesWizardProfile` answers "differs", never "why": a row stores the
identity of the preset that created it and its current mappings, and nothing
else about where those mappings came from
(`MidiControllerRowVM::wizardId` and its note,
`External/Sheaf/projects/synth/include/synth/MidiConfigViewModel.hpp:391-395`).
Refreshing a row automatically would therefore overwrite deliberate edits to
fix an invisible problem, which is a worse failure than the one being fixed.
The page says what it knows and the player decides.

## Where the line goes

The second line has no room for a sentence. Its width is
`kActiveHeaderLine2Width` = 642px against the first line's 724px
(`ControllersPageUI.hpp:360-374`), and a `static_assert` at
`ControllersPageUI.hpp:380-381` holds the second line inside the first so the
page's minimum width, `kControllerHeaderMinWidth`
(`ControllersPageUI.hpp:396`, the floor the scroll area is sized to at
`ControllersPageUI.hpp:2005-2006`), stays where it is. That leaves 82px of
slack on the second line for a sentence of eighty-seven characters at the
page's 13px text size (`RuntimePageStyle.hpp:10`), which is under a pixel a
character. What the sentence does need is measured against the box it gets,
by the glyph-width check this change adds beside the one that already
measures every device name. Taking the rest from the first line's budget would
widen the page for every row, including the rows with nothing to say, on the
narrowest host the page is measured against.

A third line has 644px for the sentence — the first line's budget less
Restore and one gap — and costs nothing to a row that does not show it,
because it is emitted only under the condition that already gates Restore.
Taking Restore down with it keeps a control beside its explanation and leaves
the second line holding Delete alone, 80px narrower than it is today.

## Evidence

The comparison the page already makes, and the one use it puts it to:

```
$ grep -rn "matchesWizardProfile\|hasResolvedWizard\|SlotMatchesWizardProfile" include src tests browser
include/synth/MidiConfigViewModel.hpp:385:    bool hasResolvedWizard = false;
include/synth/MidiConfigViewModel.hpp:390:    bool matchesWizardProfile = false;
include/synth/MidiConfigViewModel.hpp:393:    // matchesWizardProfile above and Restore keep working on a row that has
include/synth/ControllerWizard.hpp:148:// MidiConfigViewModel's hasResolvedWizard/matchesWizardProfile/
include/synth/ControllersPageUI.hpp:864:// SlotMatchesWizardProfile (regenerating a row's wizard profile to compare it
include/synth/ControllersPageUI.hpp:2707:                            if (rowVm.hasResolvedWizard && !rowVm.matchesWizardProfile)
src/MidiConfigViewModel.cpp:674:bool SlotMatchesWizardProfile(const MidiControllerSlot& slot,
src/MidiConfigViewModel.cpp:930:        row.hasResolvedWizard = slot.wizardId.has_value() &&
src/MidiConfigViewModel.cpp:932:        row.matchesWizardProfile = SlotMatchesWizardProfile(slot, Layouts());
tests/controllers_page_ui_tests.cpp:1397:    // SlotMatchesWizardProfile will regenerate and compare against -- not a
```

Run in `External/Sheaf/projects/synth`. One reader, at
`ControllersPageUI.hpp:2707`: the Restore button's own condition. Nothing on
the page says the row differs.

A second, unrelated finding, fixed here because this change reads the same
file. `include/synth/Engine.hpp:255-265` justifies snapshotting
`defaultInstrumentConfig_` by claiming that without it "a later
RevertAllToDefault (via NewPatch() with no saved patch) would reset MIDI
routing/audio device selection to empty". That path does not touch the
instrument:

```
$ sed -n '570,588p' src/PatchPersistence.cpp
PatchApplyStatus ApplyPatchMessage(
    const PatchMessageIn& message, ParameterManager& manager,
    MidiInstrumentConfig& instrument, const MidiInstrumentConfig& defaultInstrument,
    AudioDeviceState& audioDevice, const AudioDeviceState& defaultAudioDevice,
    MessageOutBus& outputBus, PatchSerializationContext context,
    bool carryInstrument, std::optional<MidiInstrumentConfig>* loadedInstrument) {
    (void)defaultInstrument;
    (void)defaultAudioDevice;
    switch (message.type) {
...
    case PatchMessageIn::Type::RevertAllToDefault:
        manager.RevertAllToDefaults();
        return PatchApplyStatus::Reverted;
```

`defaultInstrument` is discarded at `src/PatchPersistence.cpp:576` and the
revert case reverts parameters only. The snapshot has other readers —
`Engine::DefaultInstrument()` at `include/synth/Engine.hpp:899` and
`AppContext::defaultInstrument` at
`include/synth/AppContext.hpp:282` — so the snapshot itself stays; only the
reason written beside it, in `include/synth/Engine.hpp:255-265`, is wrong.

## Impact

- `External/Sheaf/projects/synth/include/synth/ControllersPageUI.hpp`: the
  notice line's node id, width and height constants, the emission of the
  third header line, and Restore's move onto it.
- `External/Sheaf/projects/synth/include/synth/Engine.hpp`: the snapshot's
  stated reason.
- `External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp` and
  `External/Sheaf/projects/synth/juce/ControllersPageSimulationTests.cpp`:
  the checks below.
- `MANUAL.md`: the Twister section's account of what an older row shows.
- `app/check_artifact_symbols_resolve.py`: task 1.3's `Check:` text cites
  `juce::GlyphArrangement::addLineOfText` and
  `juce::Font(juce::FontOptions(...))`, which this gate could not otherwise
  resolve, so it gains an `EXTERNAL_SCOPES` exemption for a `juce`-scoped
  qualified name.

## Capabilities

### Modified Capabilities

- `froggers-sheaf-runtime-app`: "Each controller row control does one job"
  and "The MIDI configuration page fits this application's window in every
  state" modified.

## Delivery

The library edits land in `External/Sheaf` and reach the product as Sheaf's
own pull request from the fork, in the sequence the open ones already hold.
`MANUAL.md` is this repository's.
