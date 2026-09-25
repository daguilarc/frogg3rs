# Tasks — `frogg3rs-presses-on-the-bus`

Base: origin/main 2255303; External/Sheaf 62829a4a. One task list for both
trees. Tasks prefixed S are Sheaf work, done in `External/Sheaf`; the Sheaf
change there (`openspec/changes/app-commands-on-the-bus/`) holds only the
proposal the upstream reviewer reads and the spec deltas that are Sheaf's
files. The S tasks run first because frogg3rs main pins a Sheaf commit and
CI fetches the pin from the fork; a numbered task that names an S task runs
after it.

Every check below is run through `synth_rig::SynthRig`
(`External/Sheaf/projects/synth/tests/support/SynthRig.hpp`): presses via
`rig.Application().PortableSurface().DispatchAction(ui::Action)` and
`rig.RunBlocks(n)`. No task adds a test binary, a test that drives a press
through `ProcessFrame()`, or a test of a mechanism a story step does not
reach. Each check names the break that turns it red; the executor runs that
break before reporting the check green. Comments carry no task numbers.

Names this change creates, declared once here for the artifact gate: NEW
`MessageIn::Type::AppCommand`, NEW `MessageInBus::Peek`, NEW `HasAppCommands`,
NEW `ParameterGroup::SetStorageLowWatermark`, NEW
`ParameterGroup::StorageLowWatermark`, NEW `ClockDiagnostics::transportState`,
NEW `AppContext::clockDiagnostics`, NEW `AppContext::syncConfiguration`, NEW
`FroggersCommand`, NEW `FroggersAppCore::ApplyAppCommand`, NEW
`openspec/story.md`; carried test cases NEW
`reset_all_after_drilled_randomize_equals_a_fresh_launch_including_which_depths_exist`
and NEW `reset_with_a_gesture_button_held_matches_a_fresh_launch`.

## S. Sheaf (`External/Sheaf`)

The fork's open PRs #9 to #20 are one linear stack against upstream main,
and frogg3rs pins its tip. Each S task lands in the PR that introduced the
seam it completes ("Lands in" below), so the upstream reviewer sees each
concept whole in one PR. Amending a branch rebases every branch above it;
every changed branch is force-pushed and its PR updates. Work with no owning
PR lands in a new branch `app-commands-on-the-bus` on the new tip, the next
sequential PR. Ownership was read from the tree (the earliest branch whose
`projects/synth` contains the seam); the executor re-reads it the same way
before amending. Comments carry no task numbers. Every check names its
break; the executor runs the break before reporting green. No test is added
for a mechanism no app or story reaches.

## S1. App commands

- [ ] S1.0 Lands in: #13 `app-midi-catalog` (the catalog and the AppAction
      forwarding seam, `SetAppActionOut`, live there). S1.1–S1.4 amend that
      branch.
- [ ] S1.1 `MessageIn::Type::AppCommand`, appended after
      `SetTempoBpmNormalized`; `MessageIn::AppCommand(timestamp, command,
      value)`, where `command` is an app-defined number Sheaf never reads
      (stored in a field of its own beside `appActionIx`, since the two are
      different namespaces) and `value` is a float. Every switch over
      `MessageIn::Type` handles it: in `MessageInBus::Apply` it is dropped
      (only an app's own surface produces one, and an app without the hook
      has no consumer); in `MidiConfigBlocks`, `MidiConfigViewModel` and
      `MidiController` it is classified with `AppAction`, and no controller
      profile can name it. `SystemMessageSortKey`'s comment lists the new
      ordinal.
      Check: the build with `-Wswitch`; the ordinal test in
      `MidiConfigBlocks` tests still passes (break: insert the enumerator
      before `Shift`; red).
- [ ] S1.2 `HasAppCommands` in `AppConcepts.hpp`:
      `{ app.ApplyAppCommand(std::size_t, float) } -> std::same_as<bool>`,
      documented as the hooks above it are: what thread, what `true` and
      `false` mean, that `false` holds the bus.
- [ ] S1.3 `MessageInBus::Peek(MessageIn&, timestamp)`: `Pop` without the
      head advance; `Pop` is expressed as `Peek` plus the advance so the
      gate is written once.
      Check: `ParameterModulation` tests: a peeked message pops identical;
      a future-stamped message is neither peeked nor popped.
- [ ] S1.4 `Engine::DrainMessageBus`: `Peek`; for `AppCommand` under
      `if constexpr (HasAppCommands<App>)`, call `ApplyAppCommand`; on
      `false` return without popping; otherwise pop and apply as today.
      Realtime messages (Start, Continue, Stop, Clock) keep their batch and
      are applied after both drains, unchanged. The `ProcessBlock` step-list
      comment in `Engine.hpp` (its step 2, "apply ordinary parameter/grid
      messages immediately") says that an app command may hold its bus.
      Check: a rig test with a test app whose `ApplyAppCommand` returns
      `false` once: the command and a `ParamIncDec` pushed after it are
      both unapplied after one block and both applied after the next, in
      order. Break: pop before the call; the command is lost, red.

## S2. Storage

- [ ] S2.0 Lands in: #9 `fix-out-of-tree-app-gaps` (an out-of-tree app's
      storage need is such a gap; the watermark and the count are upstream
      code that PR already touches).
- [ ] S2.1 `ParameterGroup::SetStorageLowWatermark(std::size_t)` and
      `ParameterGroup::StorageLowWatermark()`;
      `RequestParameterStorageBatchIfLow` and the request-size floor in
      `RequestParameterStorageBatch` read it, compared against
      `AvailableParameterSlots()` as today. Default `numModulators * 2`.
      Check: a group with the watermark set to 100 and 90 available slots
      requests 10 on the next allocation; at the default it requests
      nothing. Break: ignore the setter; red.
- [ ] S2.2 `Bank::CanOpenModulationView` and
      `Bank::MissingModulationDepthCount` reachable by an app (public, or
      through `ParameterManager`), unchanged in body.

## S3. Load never drops a depth (carries app-o1-audit 2.6's requirement)

- [ ] S3.0 Lands in: the new branch `app-commands-on-the-bus` (a library
      fix with no owning PR), together with S4.1 and S4.2.
- [ ] S3.1 `ApplyPatchMessageAndNotifyApp` returns a storage-shortfall status
      beside `ArenaExhausted`, with the requested count; `Engine::
      ProcessBlock` stashes the message and sets a grow-pending flag for
      it; `MessageThreadTick` provisions the batch and clears the flag; the
      stashed message retries on a later block. One stash, one retry path,
      two reasons. The `ProcessBlock` step-list comment in `Engine.hpp`
      (its step 1, which names `ArenaExhausted` as the one stash reason)
      names both.
      Check: a patch with more depths than free storage loads whole after
      the provisioning tick, the running patch unchanged in between.
      Break: apply on shortfall; depths missing, red. The existing arena
      retry test still passes.

## S4. Carried from `app-o1-audit` (cherry-pick by hunk; hunks that touch InputOrder, HasOrderedPresses or RunIfDepthStorageCovers are not carried)

- [ ] S4.1 2.1 storage batches appended while the audio thread reads.
      Lands in: the new branch.
- [ ] S4.2 2.2 output processors resend a declined update. Lands in: the
      new branch.
- [ ] S4.3 2.3 `Engine.hpp` false comments (re-read against this change).
      Lands in: the PR whose text each comment is; the executor reads
      ownership per comment.
- [ ] S4.4 3.1 library devices an app offers: #13 `app-midi-catalog`
      (`deviceDefaults`). 3.3 the "Model" caption: #15
      `launchpad-model-on-the-row`. 3.2 a gesture reference names a gesture
      the app has, and 3.4 stale text: the PR that introduced each, read
      from the tree.
- [ ] S4.5 3.5 `HasMessageThreadTick`: #14 `shift-and-file-export`, beside
      `HasFileExports`.
- [ ] S4.6 4.1 row field edit without the section rebuild: #19
      `fold-controller-wizard-into-add-row` ("save every edit as it is
      made").
- [ ] S4.7 `app-o1-audit` hygiene 1.1–1.4 (commit 80ff0b25: retired specs
      and dead code removed, planning labels stripped): each hunk lands in
      the PR that owns its file, read from the tree; the `app-o1-audit/`
      and `app-operator-runs/` change directories are not carried by this
      task (2.7 carries the latter).
- [ ] S4.8 Two open changes ADD a requirement numbered `sar-33` with
      different titles: `fix-out-of-tree-app-gaps` (#9) and
      `shift-and-file-export` (#14). The later PR renumbers: #14's becomes
      the next free sar number, and every reference to it in that change's
      files follows. Lands in: #14.
      Check: `grep -rn 'sar-33' External/Sheaf/openspec/changes` finds one
      title.

## S5. AppContext exposure

- [ ] S5.0 Lands in: #9 `fix-out-of-tree-app-gaps` (the publication, the
      sync snapshot and `AppContext` are upstream code; the gap is an
      out-of-tree app's).
- [ ] S5.1 `ClockDiagnostics::transportState`, filled by
      `MasterClock::DiagnosticsSnapshot`, packed into
      `ClockDiagnosticsPublication`'s metadata word.
- [ ] S5.2 `AppContext::clockDiagnostics` (const pointer to the publication)
      and `AppContext::syncConfiguration` (a callable returning the
      requested `SyncConfig`), set by `Engine`'s constructor beside
      `context_.uiBus`, each with its thread stated at the field as the
      other fields do. No MODIFIED delta of `sar-3`: #19 already modifies
      it, and the fields are an ADDED requirement of their own.
      Check: after `Start` on the rig, `context->clockDiagnostics->
      Snapshot().transportState == Running`; after `Stop`, `Stopped`;
      after `RequestSyncConfiguration({.receiveClock = true})`,
      `syncConfiguration().receiveClock`. Break: leave the field
      unpacked; red on Running.

## S6. Delivery

- [ ] S6.1 Amend in stack order (#9, then #13, #14, #15, #19), rebasing
      every branch above each amendment; resolve conflicts in the branch
      that owns the seam. Then branch `app-commands-on-the-bus` from the new
      tip of #20 for S3, S4.1 and S4.2.
      Check: the ancestry walk from `fix-out-of-tree-app-gaps` to
      `app-commands-on-the-bus` reports every step stacked; `git diff`
      between each old and new PR head, restricted to `projects/synth`,
      shows only that PR's S tasks.
- [ ] S6.2 `projects/synth test`, every deadline test by path, the miniapp
      target, the browser build, at the new tip.
- [ ] S6.3 Force-push every changed branch to the fork (the open PRs
      update); push `app-commands-on-the-bus` and open it as the next
      sequential PR against upstream main. Report the new tip SHA for the
      frogg3rs pin.

## 1. Hygiene sweep (before any audit axis)

- [ ] 1.1 Sweep `app/`, `app/vst/`, `openspec/`, `MANUAL.md`, `README.md` and
      `External/Sheaf/projects/synth/{include,src,tests}`, case-insensitive,
      word-bounded, whole tree, output kept as the enumeration, for two
      lists. Removed names: `RequestPageSelect`, `RequestEncoderPress`,
      `RequestRandomizeAll`, `RequestRandomizePage`, `RequestResetAll`,
      `RequestResetPage`, `RequestTempoBpm`, `pendingPageSelect_`,
      `pendingEncoderPress_`, `pendingRandomizeAll_`,
      `pendingRandomizePage_`, `pendingResetAll_`, `pendingResetPage_`,
      `pendingTempoBpmRequest_`, `tempoDisplayBpm_`,
      `tempoExternallyClocked_`, `transportRunningDisplay_`,
      `DisplayTempoBpm`, `TempoExternallyClocked`, `LastRandomizePartial`,
      `lastRandomizePartial_`, `FroggersPressQueue`, `InputOrder`,
      `HasOrderedPresses`, `RunIfDepthStorageCovers`; and, as a separate
      grep, `TransportRunning()` as a call on the app core (not
      `IsTransportRunning`, `SetDesiredTransportRunning` or the
      `*TransportRunning_` members, which stay). Added names: `AppCommand`,
      `ApplyAppCommand`, `HasAppCommands`, `SetStorageLowWatermark`,
      `FroggersCommand`, and `Peek` as a `MessageInBus` member (not
      `PeekPtr`). Every removed-name hit is a site task 3 or 4 changes, a
      comment task 6.1 rewrites, or a test task 7.1 moves or deletes.
      Check: after task 8, the removed-name grep finds nothing outside
      `openspec/changes/archive/`, and the added-name grep finds each name
      at its definition site and its call sites.
      Prose the grep cannot match is listed under 6.1 and 7.1.

## 2. Carry the kept `frogg3rs-o1-audit` work (cherry-pick by hunk)

Each item is cherry-picked from branch `o1-audit` by the hunks that task
wrote, hand-selected per task (the commits mix kept and dropped work), then
rebuilt. A hunk that touches the press bridge, `FroggersPressQueue`,
`InputOrder` or `RunIfDepthStorageCovers` is not carried; task 3 rewrites
that site. Commit 842c760 (the post-execution audit fixes) is carried the
same way: each of its hunks goes with the task whose code it repairs, named
below, and the rest is not carried.

- [ ] 2.0 The ratified story and the comment hygiene (o1-audit commit
      0eefd3f). Carry `openspec/story.md` whole; it is the source of truth
      this change's step IDs (RND-01, MOD-09, PLG-14 and the rest) point
      into, and it exists on no other branch. Carry that commit's
      change-history comment rewrites in `app/*.hpp`, `app/*Tests.cpp`,
      `app/dsp/Delay.hpp`, `app/dsp/Drive.hpp`, `app/browser/*.mjs` and
      `app/vst/*`. Not carried from it: the `frogg3rs-o1-audit/` and
      `frogg3rs-operator-runs/` change directories (2.7 carries the latter
      on its own terms), `evidence/`, and the Sheaf pin hunk.
      Check: `openspec/story.md` on the branch is byte-identical to
      `git show o1-audit:openspec/story.md`; `app/check_no_planning_history.py`
      green.
- [ ] 2.1 Record re-arm race (o1-audit 5.1; commit 6c317dd, the Record hunks).
      Check: the thread-sanitizer probe from `evidence/runs-F/tsan_probe.cpp`
      reports no race on the fixed tree; the probe source is kept under this
      change's `evidence/`, its `.out` files are not.
- [ ] 2.2 Reset returns to the launch state through
      `Parameter::RevertAllToDefault` and takes what it resets out of every
      gesture (o1-audit 5.5, 4.6; commit 6c317dd, the Reset hunks). RST-01,
      RST-04, GES-03.
      Check: `app/FroggersModulationTests.cpp`'s carried cases
      `reset_all_after_drilled_randomize_equals_a_fresh_launch_including_which_depths_exist`
      and `reset_with_a_gesture_button_held_matches_a_fresh_launch`: every
      parameter, every depth's existence and every gesture mask equal a
      fresh rig's after Randomize All, edits, and Reset All. Break: skip
      `RevertAllToDefault` on one bank; red.
- [ ] 2.3 WRLD.Bldr out of the Preset selector, manual sections, gestures
      over MIDI, Launchpad "Model" caption (o1-audit 7.1–7.6; commits
      8657686, e9a3d37, 518c6fa). CTL-01, GES-01 to GES-04, story section 5.
      Check: the carried checks in `app/FroggersMidiCatalogTests.cpp` and
      `app/FroggersControllersPageTests.cpp` pass; a Twister row mapped to
      gesture 1 fires the gesture on the rig (break: unmap; red).
- [ ] 2.4 Controllers page row build, export without the copy, worker
      encode, capture buffer zeroed in parts (o1-audit 8.1, 8.2, 8.7, 8.8;
      commit c186dee), plus 842c760's hunks in `app/FroggersAppCore.hpp`,
      `app/FroggersAudioRoutingTests.cpp` and
      `app/vst/FroggersPluginProcessor.cpp` that leave the capture buffer
      unfilled until its parts are zeroed and order Stop with the export.
      Needs `HasMessageThreadTick` (S4.5).
      Check: the carried timing checks assert a one-run before/after
      comparison on the same machine, never a figure from the o1-audit
      evidence; the export check compares the encoded bytes of a 10 s take
      with the pre-change encoder's (sha256 equal).
- [ ] 2.5 The citation gate that refuses line-number citations into Sheaf,
      and the symbol-named citations (o1-audit commit 7100004), plus
      842c760's hunks in `app/check_citations_resolve.py` and
      `app/check_common.py` that catch a bare line number in a trailing
      comment, and its `app/dsp/` citation rewrites.
      Check: `app/check_citations_resolve.py` red on one planted
      line-number citation into Sheaf; green on the tree.
- [ ] 2.6 Not carried, recorded here so the after-code audit can confirm
      absence: o1-audit 5.6, 5.7, 8.4, the `evidence/` outputs, every test
      o1-audit added for `FroggersPressQueue`, `InputOrder` or
      `OldestPendingPressSequence`, 842c760's spec-delta hunk (this change's
      `froggers-modulation-slate` delta supersedes it), and 842c760's
      test-check repairs whose subject is a dropped task.
- [ ] 2.7 `frogg3rs-operator-runs` and Sheaf `app-operator-runs` (o1-audit
      commit 0eefd3f; Sheaf 80ff0b25): carried and left open, as the
      operator's ruling that created them stands (operator runs never hold
      delivery; fixes gated on them ship marked not yet delivered). Rewrite
      their proposals' references from the archived `frogg3rs-o1-audit` to
      this change and to `app-commands-on-the-bus`; their evidence paths
      point at probe sources this change keeps under its own `evidence/`,
      or are removed where the output is not carried. A run whose subject
      is a dropped task (5.6, 5.7, 8.4, Sheaf 2.7, 2.8, 5.x) is removed
      from them.
      Check: `openspec validate` green on both; no reference into a path
      that does not exist on the branch.

## 3. Presses on the bus (after S1–S2)

- [ ] 3.1 `FroggersCommand`, an enum of the six presses (page select,
      encoder press, Randomize All, Randomize Page, Reset All, Reset Page),
      and `FroggersAppCore::ApplyAppCommand(std::size_t command, float
      value) -> bool`. It runs the exact bodies `ProcessFrame` runs today
      for each command, the page or encoder position read from `value`,
      including the `ComputeAllParameters` call after a randomize or reset.
      No catalog lookup: the encoder press is not a catalog action (it is
      `catalog.encoderPressAction`, which Sheaf dispatches to the surface
      with the position as the action value), and the page select's catalog
      entries are one per page, so both arrive at the surface as a name and
      a value and leave it as a command and a value. Returns true on every
      command except the hold in 4.3.
      Check: not yet delivered; this task adds a case to
      FroggersModulationTests.cpp: two Back presses dispatched in one tick
      leave the drill at level 0 from level 2 (MOD-09); Reset All then
      Randomize All dispatched in one tick leave a randomized state,
      Randomize All then Reset All leave the launch state (RND-01, RST-01).
      Break: apply commands in kind order instead of bus order; the second
      assertion goes red.
- [ ] 3.2 `FroggersUiSurface::HandleAction` pushes
      `MessageIn::AppCommand(NowMicros(), command, value)` for the six press
      actions and `MessageIn::SetTempoBpmNormalized(NowMicros(),
      (bpm - kFroggersBpmMin) / (kFroggersBpmMax - kFroggersBpmMin))` for
      `kBpm`, keeping the externally-clocked guard on the value the context
      now exposes (S5). The page arrows keep their `DrillLevel()` gate.
      `PushMessage` is the one push site.
      Check: not yet delivered; this task adds a case to
      FroggersSurfaceTests.cpp: dragging the slider to 300 on the rig reads
      300 from `ClockDiagnosticsSnapshot().currentBpm` after one block
      (TRN-02, BPM-01); with receive-clock requested, the slider renders as
      the read-only line and a dispatched `kBpm` pushes nothing (SYN-04).
      Break: push the raw BPM as the normalized value; red at 300.
- [ ] 3.3 Delete `Request*`, the seven press atomics, the three tempo and
      transport mirror atomics, and their `ProcessFrame` drains. The surface
      reads tempo, external clock and transport-running from `AppContext`
      (S5), and so does `FroggersAppCore::ArmRecording`, the one reader of
      the deleted accessor inside the core. Every remaining cross-thread
      member is declared with its class: `pendingExternalAudioRouted_`,
      `freezeLatched_`, `recordArmed_`, `desiredTransportRunning_` and
      `drillLevelDisplay_` are values (the first four written by the message
      thread, the last by the audio thread), and the declaration of each
      says so. `DrillLevel()` stays.
      Check: the build; task 1.1's grep.
- [ ] 3.4 The plugin's page restore (`app/vst/FroggersPluginProcessor.cpp`,
      the `RequestPageSelect` site) calls
      `Application().PortableSurface().DispatchAction(ui::Action::WithValue(
      kPageSelect, std::to_string(page)))` on the message thread, and the
      comment at that site (which says `RequestPageSelect` is the one
      authority that also reconstructs the drill-in and describes
      `ProcessFrame`'s bounds check) is rewritten for the dispatched press.
      Check: `app/vst/FroggersVstHostTests.cpp`'s restore check reads the
      visible page from `uiState` after one block; PLG-14.
- [ ] 3.5 The push contract. `MessageInBus::Push` is single-producer;
      enumerate every `uiBus->Push` and `UiBus().Push` caller in `app/` and
      `app/vst/` and confirm each runs on the message thread (JUCE message
      thread, browser main thread, the rig's caller). Report the list.
      Check: the list; a caller on the audio thread is a blocking finding.

## 4. Storage: a press never draws short

- [ ] 4.1 Zeroing leaves the draw. Today `RandomizeParameterModulationDepths`
      zeroes one parameter's existing depths (`ZeroExistingModulationDepths`)
      and then draws that parameter, and `CollectNeutralLocalParameters`
      runs once per press in `FroggersAppCore::ProcessFrame` after the whole
      `RandomizeAll` or `RandomizePage`. After this task a press zeroes
      every target it will redraw in a pass of its own, releases once
      (`CollectNeutralLocalParameters`, moved before the draw), and then
      draws, so the draw needs at most the slots its new sources add. The
      comments whose rationale this inverts are rewritten: the "second half
      of the re-roll" block around the release call in
      `app/FroggersAppCore.hpp`, and the scope note in
      `RandomizeParameterModulationDepths` that says no separate zeroing
      pass is needed.
      Check: `app/FroggersAudioRoutingTests.cpp`'s
      `randomize_storm_holds_its_depth_working_set` and
      `release_keeps_an_armed_depth_and_takes_a_neutral_one` still pass;
      the peak bound in the first tightens to the new one-run peak (record
      the number in the check, not here).
- [ ] 4.2 The watermark. `FroggersModulationSlate::Init`, which adds the
      launch batch (`kDepthParameterStorageCapacity`), sets the group's low
      watermark to `RandomizeNewDepthBound` at the largest view (o1-audit's
      function, carried from commit 1086a7e's `app/FroggersModulation.hpp`
      hunk) and sizes the launch batch to at least that.
      Check: at launch, `AvailableParameterSlots()` ≥ the watermark; after
      fifty Randomize All presses at four blocks apiece, the available
      count at every press is ≥ the bound at that view. Break: set the
      watermark to Sheaf's default; the second assertion goes red past the
      launch storage.
- [ ] 4.3 The hold. `ApplyAppCommand` for Randomize All and Randomize Page
      compares `RandomizeNewDepthBound` with `AvailableParameterSlots()`;
      for an encoder press that opens a view it asks
      `Bank::CanOpenModulationView` (S2.2), the predicate Sheaf already
      applies. When short it calls `RequestParameterStorageBatch(shortfall)`
      and returns false; a request made while one is already pending is
      absorbed by that one (`storageRequestPending_`), so the held press
      asks again on every block until covered and runs within two message
      ticks of the shortfall at most. The `partial` branch of
      `RandomizeParameterModulationDepths` becomes an assertion, and
      `LastRandomizePartial()`, `lastRandomizePartial_`, the `anyPartial`
      plumbing, the test `randomize_all_with_ample_capacity_reports_not_partial`
      in `app/FroggersHeadlessTests.cpp` and the comment in
      `app/FroggersModulationTests.cpp` that names the accessor are removed.
      Check: not yet delivered; this task adds a case to
      FroggersAudioRoutingTests.cpp: with the watermark forced to zero and
      the launch batch to one press, a Load of a depth-heavy patch and a
      Randomize All dispatched in the same tick leave the Randomize
      unapplied after the Load's block and applied whole within two
      message ticks, with no depth short and a Reset dispatched after it
      still landing after it. Break: return true on the shortfall; the
      assertion in `RandomizeParameterModulationDepths` fires.

## 5. Sheaf pin

- [ ] 5.1 Move `External/Sheaf` to the new stack tip
      (`app-commands-on-the-bus`) once S6.3 has pushed it; record the SHA
      in the commit message.

## 6. Comments and documentation

- [ ] 6.1 Replace the "Why a request bridge exists" block in
      `app/FroggersAppCore.hpp` and the matching block in
      `app/FroggersUiSurface.hpp` with the command-or-value rule, stated
      once at `FroggersAppCore`. Rewrite every comment task 1.1's grep found
      that describes the bridge, the pending atomics or the drain order,
      and the prose the grep cannot match, found by
      `grep -rniE 'Request\*|pending\*|request bridge|app-request|pending[- ]request|single-slot|coalesc|Display\* API'`
      over `app/`: `app/Froggers.hpp` (the bridge sentence in its header),
      `app/FroggersUiSurface.hpp` (the kRecord comment naming the bridge,
      and the "App-request bridge" comment above the press branches, which
      is already false today since page select and both Resets go through
      it), `app/FroggersAppCore.hpp` (the single-slot and coalescing
      comments on the request API, the tempo drain and the members, and
      the `recordArmed_` block that contrasts itself with the bridge),
      `app/vst/FroggersPluginProcessor.cpp` (the single-slot store comment
      and the host-automation comment that says a request "would not apply
      until the block after", false today since the drain precedes
      `ProcessFrame` in the same block), `app/vst/FroggersPluginProcessor.hpp`
      (the `pendingTransportEdge_` rationale, "same idiom as
      `pendingPageSelect_`", which stops existing), and
      `app/dsp/Limiter.hpp` (the repro note naming `RequestRandomizeAll`).
      Carried false-comment fixes (o1-audit 4.x) apply where their site
      survives.
      Check: `app/check_no_planning_history.py` green; the 6.1 grep above
      over `app/` finds only `pendingExternalAudioRouted_`'s declaration,
      the export queue, and the JUCE editor's own coalescing (which is not
      this bridge).
- [ ] 6.2 MANUAL.md: nothing the player sees changes; confirm by diffing the
      transport, Randomize/Reset and BPM sections against main and
      reporting no delta beyond the carried sections from 2.3.

## 7. Tests on the rig

- [ ] 7.1 Every test that called `Request*`, `DisplayTempoBpm`,
      `TempoExternallyClocked`, `TransportRunning()` on the app core, or
      `LastRandomizePartial` (five files on main:
      `app/FroggersAudioRoutingTests.cpp`, `app/FroggersHeadlessTests.cpp`,
      `app/FroggersMidiCatalogTests.cpp`, `app/FroggersSurfaceTests.cpp`,
      `app/vst/FroggersVstHostTests.cpp`) drives the same scenario through
      the rig or the surface's `DispatchAction`. A test that existed only to
      exercise the bridge is deleted. The three routed-input tests in
      `app/FroggersHeadlessTests.cpp` keep calling `ProcessFrame()` on a
      hand-built context, because the routed-input value stays there. Test
      comments that describe the bridge (the file headers of
      `app/FroggersSurfaceTests.cpp` and `app/FroggersHeadlessTests.cpp`,
      and the in-test comments in `app/FroggersMidiCatalogTests.cpp` and
      `app/vst/FroggersVstHostTests.cpp` that the 6.1 grep lists) are
      rewritten with the tests.
      Check: `make test` and every binary by path green; the count of
      deleted tests and the story step each remaining one serves, reported.
- [ ] 7.2 No test added by this change asserts a timing figure, a queue
      depth, or a mechanism the story does not name.
      Check: the after-code audit reads the test diff against the story
      steps in the proposal's Why.

## 8. Delivery

- [ ] 8.1 `make test`, every test binary by path (the carried 96 kHz
      deadline tests stop `make test` early on this Mac), the app and vst
      host suites CI runs, the browser e2e.
- [ ] 8.2 Push to main. Then nothing stays on a branch: remove this change's
      worktree and the `o1-audit` worktree, delete their frogg3rs and Sheaf
      branches, and fast-forward the primary checkout's `main` to
      `origin/main` with `External/Sheaf` checked out at the pin main
      records.
      Check: `git worktree list` shows the primary checkout alone;
      `git status -sb` reads `main...origin/main` with no divergence;
      `git submodule status` shows no `+`.
- [ ] 8.3 Archive this change and the Sheaf change (its proposal and
      specs); delete the session scratchpad.
