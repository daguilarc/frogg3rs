# Tasks — `frogg3rs-midi-shift`

Revised 2026-09-07 after the preflight of the same day; execution approved
2026-09-07. Tier per task: **H** = mechanical,
**S** = decides what something means. Builds under `nice`, `-j2` at most. Line
numbers are reads of `main` at `e891e1f`.

Group 1 is the Sheaf half and runs first; groups 2 onward assume its API:
`UISystemMessage::Shift`, `MessageIn::Shift(timestamp, held)`, the
association fields `shiftedPress`, `shiftedAppAction`, `shiftedAppActionValue`,
`synth::FileExport`, the `HasFileExports<App>` hook and
`Engine::SetFileExportHandler`.

## 0. Hygiene — step zero

- [x] 0.1 **S** Sweep by concept, not spelling: "which button does what on the
      Twister", "how a held modifier reaches the processor", "what the row
      dropdown offers", "where a recording gets its name". Paths: `app/`
      (the catalog, its tests, `FroggersMain.cpp`, the surface read only),
      `MANUAL.md`, `openspec/changes/`, `openspec/specs/`, and the Sheaf half's
      Impact inside `External/Sheaf/projects/synth/` and
      `External/Sheaf/openspec/changes/` (the two active changes it stacks on). Findings and dispositions are the
      proposal's nine; report found versus changed.
- [x] 0.2 **H** `git status --short` at both levels. Any dirt beyond this
      change's directory stops execution.

## 1. Sheaf half — first, in order

Paths in this group are relative to `External/Sheaf/projects/synth/`; line
numbers are reads of `b50cca18`. Tier marks as above.

### 1.S0 Hygiene — step zero

- [x] 1.S0.1 **S** Sweep `include/synth/` and `src/` by concept: "where a held
      modifier lives", "where an app-action index is resolved", "what the row
      dropdown offers", "how a combo field is emitted". Expected: one modifier
      (Hold Drill), one resolve site (`Engine.hpp:1040`), one dropdown builder
      (`MidiConfigViewModel.cpp:500`), three combo branches
      (`ControllersPageUI.hpp:2666-2725`). Report found versus changed.
- [x] 1.S0.2 **H** `git status --short` clean on `app-midi-catalog`; cut branch
      `shift-and-file-export` from it.

### 1.S1 The kind

- [x] 1.S1.1 **H** Append `Shift` after `HoldDrill` in `MessageIn::Type`
      (`include/synth/ParameterModulation.hpp:978`) and `UISystemMessage`
      (`include/synth/MidiConfigViewModel.hpp:227`). Add
      `MessageIn::Shift(std::uint64_t, bool held)` beside `HoldDrill`
      (`:1044`; body beside `src/ParameterModulation.cpp:4071-4078`). Update
      the two kind-count comments (`include/synth/MidiConfigBlocks.hpp:75`,
      `src/MidiConfigBlocks.cpp:26`) to 26.
- [x] 1.S1.2 **H** Build the library once before editing with its output
      captured (`nice make -j2 build/libsynth.a 2>&1 | grep -c "not handled in
      switch"`, expected 0: `-Wall` carries `-Wswitch` and there is no
      `-Werror`, `Makefile:2`). Visit every switch in the proposal's family table and add
      the `Shift` case beside `HoldDrill` with the same disposition: name
      `"shift"` (`src/MidiController.cpp:233-234`, `:288-289`); feedback
      exclusion (`:1856`); JSON (`:2411`, `:2483`); bus `Apply` no-op
      (`src/ParameterModulation.cpp:4239`); wizard switches
      (`src/ControllerWizard.cpp:185`, `:396`); view-model arg switches
      (`src/MidiConfigViewModel.cpp:50`, `:92`, `:123`), kind mapping
      (`:188-189`), press `Shift(0, true)` and release `Shift(0, false)`
      (`:241-242`, `:276-277`), library-produced choice labelled "Shift"
      (`:507-508`), row label "shift on"/"shift off" (`:726`); sort key
      (`src/MidiConfigBlocks.cpp:119-122`). Rebuild with the same grep: a
      count above 0 names a site this list missed; fix it and report the
      count found versus the table.

### 1.S2 The model and the processor

- [x] 1.S2.1 **H** `MidiControllerSystemMessageAssociation`
      (`include/synth/MidiController.hpp:907-917`): add
      `std::optional<MessageIn> shiftedPress;`, `std::string shiftedAppAction;`,
      `std::string shiftedAppActionValue;` after the existing pair, with a
      comment saying what a shifted press is.
      `SystemButtonMidiAssociation` (`:368-373`): add `shiftedPress`.
- [x] 1.S2.2 **S** `struct ShiftState { bool held = false; };` beside
      `HoldDrillState` (`:249`). `MidiControllerProfileResult` (`:930-938`)
      gains `std::unique_ptr<ShiftState> shift;`.
      `SystemButtonMidiInProcessor` (`:377-395`) takes `ShiftState* shift =
      nullptr` after `holdDrill`; member `shift_`.
- [x] 1.S2.3 **S** `src/MidiController.cpp`: create the state at `:2960-2961`'s
      pattern; copy `shiftedPress` at `:2990-2996`; pass `shift` at `:2998`.
      In `Process` (`:940-966`): after the Hold Drill branch, `if
      (association->press.type == Shift) { if (shift_) shift_->held =
      isPress; return; }`; then `if (isPress) { PushStamped(shift_ &&
      shift_->held && association->shiftedPress ? *association->shiftedPress
      : association->press); return; }`. Release unchanged.
- [x] 1.S2.4 **H** JSON (`:2589-2600`, `:2636-2662`): write `shiftedPress` (null
      when absent) after `release`; write the two shifted strings only when
      the shifted press is `AppAction`; read all three tolerating absence.
- [x] 1.S2.5 **S** `Engine.hpp:1040-1052`: inside the resolved branch, if
      `it->shiftedPress` is `AppAction`, resolve by
      `(shiftedAppAction, shiftedAppActionValue)`; on failure log once by
      name and `it->shiftedPress.reset()` on the copy. Update the comment
      (`:1033-1039`).

### 1.S3 The Controllers page

- [x] 1.S3.1 **S** `MidiMappingRowVM::Field::ShiftAction`
      (`include/synth/MidiConfigViewModel.hpp`, declared last, after
      `GridYMax`: a field's token is its integer, so appending keeps every
      existing token), width 150
      (`include/synth/ControllersPageUI.hpp:561-563`), short label and
      column header "Shift" (`FieldShortLabel`/`ColumnHeadersForGroup`).
      Appended to an individual system row's fields (`src/MidiConfigViewModel.cpp:
      1063-1067`) unless the row's own kind is Shift or HoldDrill.
- [x] 1.S3.2 **S** `ShiftCatalog()` on the view model: entry 0 "(none)", then
      every `MessageCatalog()` entry whose kind takes no argument
      (`UISystemMessageHasArg`, `:97-127`) or is `AppAction`, excluding Shift
      and HoldDrill. `ShiftChoiceIndex(controllerIx, section, rowIx)` shaped
      like `UISystemMessageIndex` (`:1828-1856`), matching by kind and, for
      `AppAction`, by the shifted name/value pair. The list is derived once
      inside `SetMessageCatalog` (`:541-543`) into a `shiftCatalog_` member;
      `ShiftCatalog()` returns it.
- [x] 1.S3.3 **S** Edit path (`:2690-2705`'s shape) for `ShiftAction`: index 0
      clears `shiftedPress` and the strings; otherwise `shiftedPress =
      PressForUISystemMessage(choice.message, *association)` and, for
      `AppAction`, the index and the pair from the choice. Flushes through the
      session path like every field (sru-11).
- [x] 1.S3.4 **S** `MakeUISystemMessageChoices` (`:514-522`): skip actions with
      an analog range. Update the header comment
      (`include/synth/MidiConfigViewModel.hpp:263-272`).
- [x] 1.S3.5 **S** `ControllersPageUI.hpp:2666-2725`: fold the three combo
      branches into one taking `(options, selectedIndex)`; add the
      `ShiftAction` case as a fourth caller reading `vm.ShiftCatalog()` and
      `vm.ShiftChoiceIndex(...)`.

### 1.S3b The file export seam

- [x] 1.S3b.1 **H** `include/synth/AppConcepts.hpp:69-82`: `struct FileExport`
      (fileName, mediaType, bytes, note) and `concept HasFileExports`
      (`app.TakePendingFileExport()` returns `std::optional<FileExport>`),
      with a comment in the shape of `HasMidiCatalog`'s.
- [x] 1.S3b.2 **S** `include/synth/Engine.hpp`: `SetFileExportHandler`
      storing a `std::function<void(FileExport)>`; at the end of
      `MessageThreadTick` (`:497-535`), `if constexpr (HasFileExports<App>)`
      loop `TakePendingFileExport()` and call the handler, or `INFO` the
      dropped name when none is installed.
- [x] 1.S3b.3 **S** `include/synth/browser/BrowserRuntime.hpp`: install the
      handler in the constructor (queue of `FileExport`); `DequeueFileExport()`
      beside `ConsumePersistenceDirty` (`:778-783`); ABI
      `synth_browser_dequeue_file_export(runtime, out)` in the extern list
      (`:1509-1553`) and `browser/cpp/BrowserRuntimeAbi.cpp`, returning 0 for
      none and 1 with a struct of name pointer/size, media-type pointer/size,
      bytes pointer/size that stays valid until the next dequeue.
- [x] 1.S3b.4 **S** `browser/src/worker.ts`: a `dequeueFileExport` module
      binding in the shape of `dequeueMidiAction` (`:370-385`); the
      `message-tick` case (`:619-622`) drains it after the tick and emits
      each export through `emitStatus` (`:478`) as `{ type: "file-export",
      fileName, mediaType, bytes }`, the response type declared beside
      `page-status` (`:46`). `browser/src/main.ts`: the worker client
      forwards `file-export` to its status handlers beside `page-status`
      (`:129-131`) and its reply listener ignores it the same way
      (`:136-138`); the direct client (`:89-121`) needs nothing, its
      `emitStatus` already reaches the handlers. `SynthBrowserApp`'s status
      subscription (`:183`): on `file-export`, Blob, object URL, anchor with
      `download = fileName`, click, revoke; everything else to
      `renderStatus` as today. `browser/src/protocol.ts` if the response
      union lives there rather than `worker.ts`.
- [x] 1.S3b.5 **S** `browser/tests/fixtures/cpp/FakeBrowserApp.hpp`: the fixture
      app gains `TakePendingFileExport` and an action that queues a small
      export, so a Playwright spec (`browser/tests/file-export.spec.ts`,
      in the shape of the existing specs) can click it and assert a download
      with the right name, type and bytes.

### 1.S4 Checks

- [x] 1.S4.1 **S** `tests/instrument_tests.cpp`, beside the Hold Drill cases
      (`:460-560`): `ShiftHeldSwapsPressForShiftedPressAndReleaseClearsIt`
      (Shift down, button with shifted press → shifted message; Shift up →
      ordinary message; a button without a shifted press is unaffected
      while held); `ShiftAndHoldDrillAreIndependent`;
      `AssociationJsonRoundTripsShiftedPressAndTreatsAbsentAsNone` beside
      `:1524`.
- [x] 1.S4.2 **S** `tests/engine_tests.cpp`, beside `:3070`:
      `engine_rebuild_resolves_shifted_app_action_and_clears_an_unknown_one`
      (a shifted press resolving to an action other than index 0 dispatches
      that action; an unknown shifted action leaves the row with its ordinary
      press and no shifted press on the rebuilt copy; the snapshot still
      carries both strings).
- [x] 1.S4.3 **S** `tests/viewmodel_tests.cpp`: `MakeUISystemMessageChoicesOffersShiftLikeHoldDrill`;
      `MakeUISystemMessageChoicesSkipsAnalogRangedActions` over
      `MakeAnalogFakeAppCatalog` (`:4068-4076`); `SystemRowsExposeShiftFieldExceptOnShiftAndHoldDrillRows`;
      `ShiftFieldEditCommitsShiftedPressAndNoneClearsIt`.
- [x] 1.S4.4 **S** `tests/portable_ui_tests.cpp:3010`: the fits gate with a Shift
      column on every system row of every kind. Its fixture catalog
      (`:3070-3074`) gains `UISystemMessage::Shift` in `libraryKinds`, as the
      app's does in task 2.1; the 24-choice assertion (`:3376`) then holds as
      six kinds plus eighteen actions, and its message says so. `tests/controllers_page_ui_tests.cpp`:
      one case that the Shift combo renders and commits through
      `kMappingFieldCommit`.
- [x] 1.S4.4b **S** `tests/engine_tests.cpp`: a test app with `HasFileExports`
      queues one export; after a tick the installed handler received it and
      the app's queue is empty; with no handler the export is taken and
      logged, not retained. `tests/browser_runtime_contract_tests.cpp`: give
      `ValidApp` (`:194`) the hook and an action that queues an export; the
      dequeue ABI returns 0 with nothing queued, and with two queued returns
      1, 1, then 0, in order (sbw-12's scenario).
- [x] 1.S4.5 **H** Positive control for each new assertion: flip one expected
      value, watch the case go red, restore. Report the red line. `rm` the
      binary before each rebuild in a break/restore sequence.
- [x] 1.S4.6 **H** Run by path: `instrument_tests`, `engine_tests`,
      `viewmodel_tests`, `portable_ui_tests`, `controllers_page_ui_tests`,
      `controller_wizard_tests`, `browser_runtime_contract_tests`. The carried
      96 kHz deadline failures are not this change's. Then the browser
      build and the new Playwright spec. The JUCE Controllers page build and the browser runtime
      build must compile: the browser command buffer serializes the page
      tree, not the association, so no browser-side schema changes
      (`include/synth/browser/`, grepped for the pair: none). Browser steps,
      from `browser/`: `npm run build`, `npm run check:generic-runtime`,
      `nice make -j2 browser-fixture-app` (the fixture app is wasm, rebuilt
      the way `browser-audio-input-test` does it, `browser/Makefile:44-45`),
      then `npx playwright test tests/file-export.spec.ts --workers=1`. The
      JUCE side: `nice make -j2 -C apps/miniapp` compiles the runtime shell
      and the Controllers page (`Makefile:311-312`).

### 1.S5 Spec

- [x] 1.S5.1 **H** Create `External/Sheaf/openspec/changes/shift-and-file-export/`
      in the shape of `app-midi-catalog/` there: a `proposal.md` (the
      proposal's Sheaf half, paths relative to `projects/synth/`), a
      `tasks.md` (group 1 of this file, with its boxes ticked as delivered),
      and `specs/` moved from this change's `sheaf-specs/` (the README stays
      behind and is deleted with the directory). Every scenario's Check line
      names a check that passes now, the "not yet delivered" notes removed,
      or stays marked not delivered with the task named. `openspec validate
      shift-and-file-export` from `External/Sheaf` passes. Then `git rm -r
      openspec/changes/frogg3rs-midi-shift/sheaf-specs` in frogg3rs; the
      proposal's Impact already names the destination.

### 1.S6 Deliver the Sheaf half, then move the pin

- [x] 1.S6.1 **H** Commit on `shift-and-file-export`; clean tree; push to the
      fork; open the next pull request against upstream `main`.
- [x] 1.S6.2 **H** Move the submodule pin to that head. `make -C app test`
      must still be green before any app edit: the association gained
      optional fields with defaults and one enumerator was appended, so
      nothing here should need to change yet. A red gate here is a Sheaf-half
      defect, not a reason to edit the app side.

## 2. The catalog and the Twister (`app/FroggersMidiCatalog.hpp`)

- [x] 2.1 **H** `catalog.libraryKinds` (`:278-284`): append
      `synth::UISystemMessage::Shift`. No new action; `catalog.actions` is
      unchanged.
- [x] 2.2 **S** `AppActionButton` (`:50-60`): add trailing parameters
      `std::string shiftedAction = {}, std::string shiftedValue = {}`; when
      `shiftedAction` is non-empty set `association.shiftedPress =
      synth::MessageIn::AppAction(0, 0, 0.0f)` and the two shifted strings.
      Every existing call compiles unchanged.
- [x] 2.3 **S** Replace `HoldDrillButton` (`:62-71`) with
      `HeldButton(synth::MidiControlAddress, synth::UISystemMessage kind)`
      producing press `held = true` and release `held = false` for
      `HoldDrill` or `Shift` (`MessageIn::HoldDrill` / `MessageIn::Shift`),
      `outputFeedback = false`. Update the APC40 call at `:118`. Two uses,
      one definition.
- [x] 2.4 **S** Replace the six-entry `systemMessages` at `:86-93` with the
      proposal's D3 table: CC 8 Bank Next / Bank Previous; CC 9 Play / Stop;
      CC 10 Freeze / Reset Page; CC 11 Scene 1 (`kSceneSelect`, `"0"`) /
      Scene 2 (`kSceneSelect`, `"1"`); CC 12 Randomize Page / Randomize All;
      CC 13 `HeldButton(..., Shift)`.
- [x] 2.5 **H** Header comment (`:3-19`): five paired side buttons plus
      Shift, and why CC Hold is required (the release ends Shift). Present
      tense, no history.

## 3. A finished recording is a file export

- [x] 3.1 **S** `app/FroggersAppCore.hpp:543-558`: remove
      `SetOnRecordingFinished`, `NotifyRecordingFinished` and
      `onRecordingFinished_` (`:2490`). Add `QueueRecordingExport()`: copies
      `RecordedAudio()`, encodes with `EncodeWavPcm16Mono` (`:2524`), names
      the file from the local date (`std::chrono::system_clock` through
      `std::localtime`/`std::strftime`, `"%Y-%m-%d"`, plus `.wav`), media type
      `audio/wav`, note "stopped at the 30-minute limit" when
      `RecordingTruncated()`, and stores it in `pendingExport_`
      (`std::optional<synth::FileExport>`). Add
      `std::optional<synth::FileExport> TakePendingFileExport()` so the app
      satisfies `HasFileExports`. A capture the audio thread disarmed at
      the cap (`:1186-1194`) is queued by the core itself: a
      `QueueTruncatedExportIfPending()` (truncated, frames captured, not
      yet exported, not armed) runs at the top of `TakePendingFileExport()`
      and at the top of `ArmRecording()` before the buffer is reassigned,
      with an `exported` flag `ArmRecording` resets. Still no JUCE (`check-no-juce`).
      `SetOnRecordRefused`/`NotifyRecordRefused` go in 3.3b.
- [x] 3.2 **H** `app/FroggersUiSurface.hpp:2225-2228`: the stop branch calls
      `app_->QueueRecordingExport()` where it called
      `NotifyRecordingFinished()`; the `RecordedFrameCount() > 0` guard stays.
- [x] 3.3 **S** `app/FroggersMain.cpp:183-256`: `RegisterRecordingCallbacks`
      becomes `RegisterFileExportHandler(engine)`, called at `:177` with
      `activeSession_->GetRuntime().GetEngine()`. It installs one handler
      through `engine.SetFileExportHandler(...)`: open the chooser on the
      documents folder joined with `export.fileName`, keep the overwrite
      flags (`:216-217`), write `export.bytes` through the same stream code,
      and append `export.note` to the completion alert when non-empty. Gone
      from this file: the refusal alert (`:192-195`, the surface shows the
      notice, task 3.3b), the encoding call, and the literal
      `frogg3rs-recording.wav`. Update the header comment (`:183-190`).
- [x] 3.3b **S** Refusal notice. `app/FroggersUiSurface.hpp`: a
      `std::string transportNotice_`; the Record branch (`:2207-2229`) sets it
      to `app_->RecordRefusalReason()` when `ArmRecording()` refuses and
      clears it when arming succeeds; the Play branch (`:2160-2171`) clears
      it. The transport cell (`:1227-1300`) becomes a Column
      (`froggers.layout.left.transport.stack`, the cell's vertical weight)
      holding the plates Row (`kTransportRow` unchanged, 28 px tall) and,
      only when non-empty, `b.Label(FroggersNodeIds::kTransportNotice,
      transportNotice_, ...)` beneath it; new node id
      `froggers.transport.notice`. The plates' resolved bounds with no
      notice are identical before and after. `app/FroggersAppCore.hpp:546-558`,
      `:2491`: remove `SetOnRecordRefused`, `NotifyRecordRefused`,
      `onRecordRefused_`. `RecordRefusalReason()` stays.
- [x] 3.4 **S** `app/browser/e2e/recording.spec.mjs`, in the shape of
      `midi-activation.spec.mjs` with `helpers.mjs` selectors, added to
      `DESKTOP_SPECS` in `playwright.config.mjs:36` (a spec outside those
      lists never runs). Case one: click Play, then Record, wait one second,
      start `page.waitForEvent("download")`, click Record again; assert
      `suggestedFilename()` equals today's date plus `.wav` and the body
      starts with `RIFF`. The capture depends on the worklet rendering under
      Playwright's Chromium; Sheaf's `browser/tests/audio-flow.spec.ts:904`
      and `:987` already assert block counts of 4 and 8 there, and the
      record buffer fills inside that same process callback. Case two: click
      Record with the transport stopped and assert the node
      `froggers.transport.notice` reads "Press Play before recording." and
      its bounding box lies inside the left block's. The harness serves
      `app/browser/dist/site`, assembled by `make -C app/browser package`
      after `app/browser/build-browser.sh`; the suite's carried failures are
      recorded in memory and are not this change's.

## 4. Checks

- [x] 4.1 **S** `app/FroggersMidiCatalogTests.cpp:434-448`: the Twister order
      vector becomes Bank Next, Play, Freeze, Scene 1, Randomize Page, and a
      sixth entry whose press is `MessageIn::Type::Shift` with `boolValue`
      true and a release with `boolValue` false. For the first five, assert
      `appActionValue` and the shifted pair (Bank Previous, Stop, Reset Page,
      Scene 2 with `"1"`, Randomize All), and that each shifted pair resolves
      through `FindMidiAppAction`. Assert `libraryKinds` contains Shift
      exactly once, and no APC40 or Launchpad association carries a shifted
      press.
- [x] 4.2 **S** `app/FroggersControllersPageTests.cpp:118`
      (`real_catalog_defaults_generate_and_accept_adds_through_the_view_model`)
      must stay green with the Twister rows now carrying shifted presses;
      add an assertion that the Twister row's Shift column reads the D3
      shifted job for CC 8 and none for CC 13.
- [x] 4.3 **S** `app/FroggersSurfaceTests.cpp:3220-3238`
      (`record_action_stop_with_data_fires_the_finished_callback_exactly_once`)
      becomes `record_action_stop_with_data_queues_one_named_wav_export`:
      after the second Record, `TakePendingFileExport()` yields one export
      whose name matches `^\d{4}-\d{2}-\d{2}\.wav$`, whose bytes begin
      `RIFF` and decode to `RecordedFrameCount()` frames, and whose note is
      empty; a second take yields none. A truncated capture (`:3113-3129`'s
      rig) carries the note. The standalone dialog is an operator step:
      Record, Play, Record, the dialog opens on `date +%Y-%m-%d`.
      `:3203-3216` (`record_action_refused...`) asserts instead that after a
      Record with the transport stopped the built tree holds a
      `froggers.transport.notice` label reading "Press Play before
      recording.", that it is absent before, that Play removes it, and that
      its resolved bounds lie inside the transport cell's in the wide layout
      (the rig resolves layout the way `:273`'s helper does), and that the
      four plates' resolved bounds with no notice equal what they were
      before the column (record them from `main` first). A truncated
      capture (`:3113-3129`'s rig, no stop press) yields its export with
      the note from `TakePendingFileExport()`; a second truncated capture
      left unpolled is flushed by the next `ArmRecording()` before that
      take begins.
- [x] 4.4 **H** Positive control for each new assertion: flip one expected
      value, watch the case go red, restore. Report the red line. `rm` the
      binary before each rebuild in a break/restore sequence.
- [x] 4.5 **H** `make -C app test` under `nice -j2`, then run by path after
      the carried deadline stop: `froggers_midi_catalog_tests`,
      `froggers_controllers_page_tests`, `froggers_surface_tests`, the
      coverage check (`app/Makefile:269`), the docs check (`:190`) and
      `check-no-juce`. The two JUCE hosts include the changed headers and
      must compile: the standalone through `app/build-launcher.sh` (already
      `nice -j2`) and the plugin through `nice make -j2 -C app/vst/build`.
      Then the browser build (`app/browser/build-browser.sh`), `make -C
      app/browser package`, and the new e2e spec.

## 5. Spec

- [x] 5.1 **H** Before archive, every scenario in the two deltas names a
      check that passes now, or is marked not delivered.

## 6. Postflight

- [x] 6.1 **S** Postflight in a fresh context: implementation versus this
      proposal; enumerate `HeldButton`, the shifted fields, the date format
      string, `TakePendingFileExport`, `FileExport`, `file-export`,
      `RecordingFinished` and `RecordRefused` across both trees for a second
      definition or a dangling reader; name each gate, when it last ran, and
      whether its inputs moved since.

## 7. Documentation — after postflight passes, immediately before commit and push

- [x] 7.1 **S** "What can be mapped" (`:288-294`): add Shift. New subsection
      after "Hold Drill" (`:296-300`), "Shift": a button mapped to Shift is
      held; while held, any button with a shifted job does that job instead;
      the shifted job is the Shift column on the row; a missing release leaves
      Shift held until the next press and release. "MIDI Fighter Twister"
      (`:302-309`): replace the side-button sentence with the D3 table, keep
      the Utility paragraph. Plain present tense.
- [x] 7.2 **S** Transport section (`:102-111`) documents Play and Stop only;
      the Freeze button and Record are described nowhere. Add both, plain
      present tense. **Freeze**: a latch that stops the transport, holds the
      envelope gate open and keeps the delay recirculating so the instrument
      drones with the transport stopped; releasing it silences the
      instrument; Play disarms it (`app/FroggersUiSurface.hpp:2160-2205`,
      `app/FroggersAppCore.hpp:450-482`). **Record**: press once to arm
      while the transport runs, again to stop; a stopped recording is offered
      as `YYYY-MM-DD.wav`, the standalone through a save dialog on that name
      and the browser build as a download; pressing Record with the transport
      stopped shows "Press Play before recording." in the transport row; a
      capture stops at 30 minutes and the saved file says so
      (`app/FroggersAppCore.hpp:488-524`). Browser section (`:237-242`): a
      stopped recording downloads. The plugin has no Record (`:218-219`,
      unchanged).
- [x] 7.3 **S** `QUICK_DICT.md:14` lists Play / Stop only. Add one line,
      "**Freeze / Record**", in the same voice: the latch in one sentence,
      Record's arm/stop and the dated file in another. `README.md:110-113`
      is generic and stays. `check-docs-match-parameter-table`
      (`app/Makefile:190`) reads only the bank sections and is unaffected.

## 8. Commit and push

- [x] 8.1 **H** Commit at both levels; clean trees at both levels; push
      frogg3rs to `main`. The Sheaf pull request
      was opened in 1.S6.1.
