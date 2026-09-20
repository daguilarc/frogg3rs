The executor's deliverable is a report; code changes are a side effect of it.
A conflict between this file and the code is reported, not reconciled. Every
new assertion is shown to fail with its production change reverted, by the
executor, before the task is reported done, and the report says so. A test
that is already red on the base is reported, never edited. Build and run one
binary at a time.

The library work is in `External/Sheaf/projects/synth`; paths in tasks 1 and 2
are relative to it. Task 3 is this repository's.

## 1. The row says it differs from its preset

- [ ] 1.1 Add the notice line. In `include/synth/ControllersPageUI.hpp`:
      NEW `NodeIds::ControllerPresetNotice(controllerIx)` beside
      `NodeIds::ControllerRestore`; NEW
      `ControllersLayout::kPresetNoticeText`, the single definition of the
      sentence `This row differs from its preset. Restore replaces its
      mappings and discards any edits.`; NEW
      `ControllersLayout::kControllerNoticeWidth`, `kActiveHeaderLine1Width`
      less one `kLifecycleControlGap` and `kLifecycleRestoreWidth`; NEW
      `kActiveHeaderLine3Width`, that width plus the gap plus Restore. Add no
      `static_assert` comparing it to `kActiveHeaderLine1Width`: by this
      arithmetic `kActiveHeaderLine3Width` equals `kActiveHeaderLine1Width`
      exactly, for any values the underlying constants take, so such an
      assertion could never fail and would guard nothing; task 1.3's
      glyph-measurement check is the real fit guard for this line. NEW
      `kControllerHeaderHeightWithNotice`, three times
      `kControllerHeaderLineHeight`. An active row's Column takes that height
      while `rowVm.hasResolvedWizard && !rowVm.matchesWizardProfile` and
      `kControllerHeaderHeight` otherwise, and under the same condition emits
      `NodeIds::ControllerRow(controllerIx) + ".line3"` holding the notice
      Label at `kControllerNoticeWidth` then the Restore button. The Restore
      block moves off `.line2`, so `kActiveLifecycleWidth` would only ever
      name Delete alone: delete it, so `kActiveLifecycleWidth` does not
      exist afterward, use `kLifecycleDeleteWidth` in
      `kActiveHeaderLine2Width`, and correct the comment above it, which says
      line two's lifecycle block is Delete plus Restore.
      Check: `kControllerHeaderMinWidth` still evaluates to
      `kActiveHeaderLine1Width` (724.0f).
      `tests/portable_ui_tests.cpp`'s
      `TestControllersRowFitsWithinFroggersNarrowestHost` builds a second
      Twister row, `restoreDivergedRow`, with a `wizardId` that resolves and
      a stored config that diverges from what that preset generates —
      both halves of the notice condition — and that row stays in the
      fixture through every `requireFits()` call that follows it in the
      test. Once that row renders the notice's third header line, whether
      those calls still pass at the narrowest host is not established by
      this task; run the binary and report each affected `requireFits()`
      call's result. A failure is reported as a fit problem to solve, never
      papered over by loosening what the test measures.
- [ ] 1.2 Pin the three row states in
      `tests/controllers_page_ui_tests.cpp`'s
      `TestRestoreReinstallsADivergedPresetAndIsGatedByDivergence`, whose
      fixture already holds row 0 installed from the preset and untouched,
      row 1 never created from a preset, and row 2 diverged through the
      per-field commit path. On the tree built after the turn-step commit:
      `FindNodeById(tree, NodeIds::ControllerPresetNotice(0)) == nullptr`;
      `FindNodeById(tree, NodeIds::ControllerPresetNotice(1)) == nullptr`;
      `FindNodeById(tree, NodeIds::ControllerPresetNotice(2)) != nullptr` with
      its text equal to `ControllersLayout::kPresetNoticeText`;
      `NodeIds::ControllerRestore(2)` and that notice are both children of
      `NodeIds::ControllerRow(2) + ".line3"`;
      `FindNodeById(tree, NodeIds::ControllerRow(2))->bounds.height ==
      ControllersLayout::kControllerHeaderHeightWithNotice` and
      `FindNodeById(tree, NodeIds::ControllerRow(0))->bounds.height ==
      ControllersLayout::kControllerHeaderHeight`; the edited turn's step in
      `harness.instrument.controllers[2]` is still `0.25` at that point, the
      row having been told about and not acted on. On the tree built after
      the Restore dispatch the fixture already makes:
      `FindNodeById(afterRestoreTree, NodeIds::ControllerPresetNotice(2)) ==
      nullptr` and that row's height is back to `kControllerHeaderHeight`.
      Check: the case passes; with the `.line3` emission removed it fails on
      the row 2 notice assertion, and the report says so.
- [ ] 1.3 Measure that the sentence fits its box. NEW
      `RunPresetNoticeWidthCheck()` in
      `juce/ControllersPageSimulationTests.cpp`, beside
      `RunDeviceLabelWidthCheck()` and called from `main()` next to it, which
      lays `ControllersLayout::kPresetNoticeText` out with
      `juce::GlyphArrangement::addLineOfText` at
      `juce::Font(juce::FontOptions(synth::pagestyle::kDefaultTextSize))`, the
      font a Label node renders at, and requires the measured width to be at
      most `ControllersLayout::kControllerNoticeWidth`.
      Check: the check passes and prints the measured width; with
      `kPresetNoticeText` set to that sentence written twice it fails, and
      the report gives both measurements. Citing `juce::GlyphArrangement::
      addLineOfText` and `juce::Font(juce::FontOptions(...))` above is a
      claim `app/check_artifact_symbols_resolve.py` cannot resolve against
      this repository's own indexed trees, so this task also gives that gate
      a `juce`-scoped `EXTERNAL_SCOPES` exemption.

## 2. The instrument snapshot's stated reason

- [ ] 2.1 Correct the reason at `include/synth/Engine.hpp:255-265`. It says
      that without the `defaultInstrumentConfig_` snapshot a later
      `RevertAllToDefault` through `NewPatch()` would reset MIDI routing and
      audio device selection to empty. `ApplyPatchMessage` discards
      `defaultInstrument` (`src/PatchPersistence.cpp:576`) and its
      `RevertAllToDefault` case calls `manager.RevertAllToDefaults()` and
      nothing else (`src/PatchPersistence.cpp:585-586`), so that path never
      writes the instrument config. Say instead what the snapshot is for: it
      is what `Engine::DefaultInstrument()` (`include/synth/Engine.hpp:899`)
      and `AppContext::defaultInstrument`
      (`include/synth/AppContext.hpp:282`) hand out, and it is taken before
      any startup patch applies so those report the instrument the app's
      `Init()` configured rather than whatever a patch left behind. Change no
      assertion. `tests/engine_tests.cpp`'s
      `engine_default_instrument_snapshot_carries_app_init_controller_after_new_patch`
      and `engine_revert_all_to_default_preserves_host_audio_selection`
      already describe this correctly and take no edit from this task.
      Check: `grep -n "RevertAllToDefault" include/synth/Engine.hpp` prints
      nothing; the `tests/engine_tests.cpp` binary passes with the same case
      count as on the base, which the report gives for both runs.
## 3. The manual's account of an older Twister row

- [ ] 3.1 In `MANUAL.md`, the MIDI Fighter Twister section tells a player
      that a Twister row added from the preset before this version "keeps its
      old mappings and shows Restore". After task 1 the row also carries the notice line, so the
      sentence no longer describes what the player sees. Rewrite it to say
      that the row keeps its old mappings, says it differs from its preset,
      and offers Restore. Describe the line; do not quote it, so the manual
      and the page do not become a pair that must be kept in sync by hand.
      The sentence right after it, "Pressing Restore installs Shift +
      Crunchy and replaces edits made to that row.", is not this task's to
      keep true: `frogg3rs-shift-and-crispy-move-the-tempo` gives Crispy a
      shifted turn on the same preset, and once that change lands Restore
      installs both, making that sentence incomplete. Leave it exactly as it
      reads; that other change owns rewriting it. The two sentences after
      that one, about a patch saved earlier and about a released row that
      shows no Restore, are unchanged and stay true.
      Check: `grep -n "shows Restore" MANUAL.md` prints nothing;
      `grep -n "differs from its preset" MANUAL.md` prints the rewritten
      sentence; `python3 app/check_docs_match_parameter_table.py app` and
      `python3 app/check_twister_manual_diagrams_drift.py app`, the two gates
      that read `MANUAL.md`, both pass.
