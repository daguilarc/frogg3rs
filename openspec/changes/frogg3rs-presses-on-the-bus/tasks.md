# Tasks — `frogg3rs-presses-on-the-bus`

Base: origin/main 2255303; External/Sheaf 62829a4a. One task list for both
trees. Tasks prefixed S are Sheaf work, done in `External/Sheaf`; the Sheaf
change there (`openspec/changes/app-commands-on-the-bus/`) holds only the
proposal the upstream reviewer reads and the spec deltas that are Sheaf's
files.

Order of execution: S1 to S6 (the Sheaf tip is assembled locally), then
1.1 and 1.2 (the sweep, and the pin moved to that local tip, since every
frogg3rs task from 2 on compiles against it), then 2, 3, 4, 6, 7, 8. The
Sheaf push is 8.2, after the frogg3rs suite is green against the local tip.

Every check below is run through `synth_rig::SynthRig`
(`External/Sheaf/projects/synth/tests/support/SynthRig.hpp`): presses via
`rig.Application().PortableSurface().DispatchAction(ui::Action)` and
`rig.RunBlocks(n)`. No task adds a test binary, a test that drives a press
through `ProcessFrame()`, or a test of a mechanism a story step does not
reach. Each check names the break that turns it red; the executor runs that
break before reporting the check green, and deletes the test binary before
each rebuild of a check or its break (task 3.1 also fixes the dependency
line that makes this necessary). Comments carry no task numbers.

Carried work, method. Every carry below states what is carried (the source
commit and the behaviour, by symbol) and the check it must pass. How the
hunks apply, what conflicts, how many sites a gate will report and which
branch first holds a seam are not predicted here: the executor resolves
application, conflicts and ownership by applying, and stops and reports any
conflict this text does not settle. Hunks that touch `FroggersPressQueue`,
`InputOrder`, `HasOrderedPresses`, `RunIfDepthStorageCovers`,
`DepthStorageCovers`, `ProvisionStorageBatch`, `RandomizeNewDepthBound`,
`RandomizeKind`, `ResetKind`, sequence stamping or a storage-wait test are
never carried; everything else in a named commit is, unless a task says
otherwise.

Names this change creates, declared once here for the artifact gate: NEW
`MessageIn::Type::AppCommand`, NEW `HasAppCommands`, NEW
`ParameterGroup::SetStorageLowWatermark`, NEW
`ParameterGroup::StorageLowWatermark`, NEW `ClockDiagnostics::transportState`,
NEW `AppContext::clockDiagnostics`, NEW `AppContext::syncConfiguration`, NEW
`FroggersCommand`, NEW `FroggersAppCore::ApplyAppCommand`, NEW
`openspec/story.md`; carried from the unpushed branches, so new on main:
NEW `MissingDepthsForValuesJSON`, NEW `GestureCountWouldExceedCap`, NEW
`reset_all_after_drilled_randomize_equals_a_fresh_launch_including_which_depths_exist`,
NEW `reset_with_a_gesture_button_held_matches_a_fresh_launch`, NEW
`a_stopped_take_is_encoded_off_the_message_thread_to_the_same_bytes`.

## S. Sheaf (`External/Sheaf`)

The fork's open PRs against upstream main (#9, 11, 12, 13, 14, 15, 17, 18,
19, 20) are one linear stack, each branch having its parent as an ancestor,
and frogg3rs pins its tip. Placement rule for every S task: it lands in the
PR whose diff introduced the concept the task extends, found by reading
`git diff fork/main <branch>` for that seam and taking the lowest branch
that has it; where the task's code depends on anything present only
higher in the stack, or the seam is upstream code no PR introduced, it
lands in the new branch `app-commands-on-the-bus` on the new tip, the next
sequential PR. No PR number is assigned here; the executor reports each
placement with the diff that decided it. Amending a branch rebases every
branch above it; every changed branch is force-pushed at 8.2 and its PR
updates. Comments carry no task numbers. Every check names its break; the
executor runs the break before reporting green. No test is added for a
mechanism no app or story reaches.

## S1. App commands

- [ ] S1.1 `MessageIn::Type::AppCommand`, appended after
      `SetTempoBpmNormalized` (the last enumerator, present only at the
      tip, so this task is new-branch work by the placement rule);
      `MessageIn::AppCommand(timestamp, command, value)`, where `command` is
      an app-defined number Sheaf never reads (a field of its own beside
      `appActionIx`, since the two are different namespaces) and `value` is
      a float. Every switch over `MessageIn::Type` handles it: in
      `MessageInBus::Apply` it is dropped (only an app's own surface
      produces one, and an app without the hook has no consumer); in
      `MidiConfigBlocks`, `MidiConfigViewModel` and `MidiController` it is
      classified with `AppAction`, and no controller profile can name it.
      `SystemMessageSortKey`'s comment lists the new ordinal.
      Check: a case in `projects/synth/tests/blocks_tests.cpp` asserting the
      sort key of `AppCommand` is greater than that of
      `SetTempoBpmNormalized`; break: declare `AppCommand` before `Shift`,
      red. The Sheaf build runs with `-Werror=switch` for this task so an
      unhandled enumerator fails rather than warns.
- [ ] S1.2 `HasAppCommands` in `AppConcepts.hpp`:
      `{ app.ApplyAppCommand(std::size_t, float) } -> std::same_as<void>`,
      documented as the hooks above it are: the audio thread, called from
      the bus drain in FIFO order with the non-realtime messages, before
      `ProcessFrame`.
- [ ] S1.3 `Engine::DrainMessageBus`: for `AppCommand` under
      `if constexpr (HasAppCommands<App>)`, call `ApplyAppCommand` and
      continue; otherwise apply as today. Realtime messages (Start,
      Continue, Stop, Clock) keep their batch and are applied after both
      drains, unchanged. The `ProcessBlock` step-list comment in
      `Engine.hpp` (its step 2) names the app command beside the ordinary
      messages.
      Check: a rig test with a test app that records its commands: a
      command, a `ParamIncDec` and a `Start` pushed in that order are
      applied in one block, the command before the turn, and the transport
      is running after the block. Break: forward the command to the app
      action out bus instead; the command is applied a tick late, red.

## S2. Storage watermark

- [ ] S2.1 `ParameterGroup::SetStorageLowWatermark(std::size_t)` and
      `ParameterGroup::StorageLowWatermark()`; `RequestParameterStorageBatchIfLow`
      reads it, compared against `AvailableParameterSlots()` as today.
      Default `numModulators * 2`. `ParameterManager::RequestParameterStorageBatch`
      has no request-size floor; it requests `minimumAdditionalParameters`.
      Check: a case in `projects/synth/tests/parameter_modulation_tests.cpp`:
      with the watermark set to 100 and 89 slots available after an
      allocation, the request pushed carries the shortfall below the
      watermark; at the default watermark on the same group, no request.
      Break: ignore the setter; red.

## S3. A patch never applies with a depth missing, at startup or running

- [ ] S3.1 Two apply sites already retry an arena-exhausted patch message:
      `Engine::ApplyPendingPatchMessages` (startup, before audio, on the
      thread that runs `Initialize`; it grows the arena inline and retries)
      and `Engine::ProcessBlock` (running; it stashes the message in
      `pendingPatchMessage_`, sets `arenaGrowPending_`, and
      `MessageThreadTick` grows the arena before the retry). A depth-storage
      shortfall becomes the second reason at both sites through one
      provisioning helper, and no third site. `ApplyPatchMessageAndNotifyApp`
      returns a storage-shortfall status when the patch's depths would
      leave a group's available storage below that group's watermark,
      with the per-group need counted by `MissingDepthsForValuesJSON`
      (carried from `app-o1-audit` 6ac80442; that commit's own startup
      branch and its construct are not). The arena branch's growth step
      stays as each site has it (`GrowAndReset` inline at startup;
      `GrowSerializationArenaForTick` with its cap while running, whose cap
      path clears only its own reason); the stash-and-raise step around it
      at the two running call sites (`ProcessBlock`'s retry,
      `DrainPatchInputBus`) is one shared `StashPendingPatchMessage`,
      parameterized by which status fired. The helper is the storage
      provisioning only, written once in `Engine`:
      `AddParameterStorageBatch` of need plus watermark on each group,
      called directly so the group's pending low-water request cannot
      absorb it; `MessageThreadTick`'s existing handling of a
      `ParameterStorageBatchNeeded` message (its step 1) calls that helper
      too, so the provisioning line exists once. Startup: the helper runs
      inline and the message retries at once, as the arena case does.
      Running: the audio thread writes the needs into a fixed-capacity
      member beside the stash (one entry per group, sized from the
      manager's group count at `Initialize`), only when no storage stash is
      pending, then sets a storage-grow flag distinct from
      `arenaGrowPending_` with release order. The barrier that holds the
      stash holds it while either flag is set; a retry that reports the
      storage shortfall again re-stashes under the storage reason, never
      the terminal branch; `MessageThreadTick` reads the flag with acquire
      order, runs the helper, and clears the flag with release order after
      its last read of the needs; the stashed message retries on the first
      block after the clear. The `ProcessBlock` step-list comment (its step
      1, which names `ArenaExhausted` as the one stash reason) names both,
      and the stash says that a message applied while a Load waits is
      overwritten by the patch when it applies.
      Check: two cases in `projects/synth/tests/engine_tests.cpp`. Startup:
      a rig created on data paths whose last-opened patch needs more depths
      than the launch batch leaves above the watermark opens with that
      patch whole, before its first block; break: return the shortfall
      status without provisioning, startup opens with no patch, red.
      Running, with the rig ticking at the production cadence (one message
      tick per six blocks, never per block, so an early retry cannot hide):
      `LoadPatch` of the same patch on a running rig leaves the running
      patch unchanged across the blocks before the provisioning tick and
      every depth live after the retry; the existing arena retry test still
      passes; break: retry before the flag clears, the Load is dropped,
      red.

## S4. Carried from `app-o1-audit`

- [ ] S4.1 Commit 80ff0b25 (hygiene: retired specs and dead code removed,
      planning labels stripped); commit a8b2a616 (storage batches appended
      while the audio thread reads; output processors resend a declined
      update; `Engine.hpp` false comments, re-read against this change);
      commit 74a408b2 (library devices an app offers, `libraryDeviceKinds`;
      a gesture reference names a gesture the app has; the Launchpad
      "Model" caption; the stale `sar-30` text; `HasMessageThreadTick`);
      commit 6e0902fd (a row field edit without the section rebuild);
      commit 6b85ee75's `GestureCountWouldExceedCap`, row-rollback and
      Generic-processor resend hunks. The `app-operator-runs` change
      directory is carried (rewritten as frogg3rs 2.7 says); the
      `app-o1-audit` change directory is not.
      Check: every test those commits carried passes at the new tip;
      `projects/synth test` green.
- [ ] S4.2 Where a carried caption or text contradicts an open change's own
      spec or task text in the stack (the Launchpad row's selector is
      captioned "Model", and `launchpad-model-on-the-row`'s files still say
      "Variant"), that change's text is amended in its own branch.
      Check: `grep -rn Variant External/Sheaf/openspec/changes` finds no
      caption claim.
- [ ] S4.3 Every spec delta in `app-o1-audit`'s change directory is copied
      into this change's `specs/`; the executor pairs each delta with the
      carried code by reading both, and reports the pairing.
      Check: `openspec validate app-commands-on-the-bus` and
      `app-operator-runs` green in the Sheaf tree.
- [ ] S4.4 Two open changes ADD a requirement numbered `sar-33` with
      different titles (`fix-out-of-tree-app-gaps` and
      `shift-and-file-export`). The later one is renumbered to the next
      number free after every open change's and this change's (sar-38 to
      40) and the carried sar-35 to 37, and every reference in its files
      follows.
      Check: `grep -rn 'sar-33' External/Sheaf/openspec/changes` finds one
      title.

## S5. AppContext exposure

- [ ] S5.1 `ClockDiagnostics::transportState`, filled by
      `MasterClock::DiagnosticsSnapshot`, packed into
      `ClockDiagnosticsPublication`'s metadata word.
- [ ] S5.2 `AppContext::clockDiagnostics` (const pointer to the publication)
      and `AppContext::syncConfiguration` (a callable returning the
      requested `SyncConfig`), set by `Engine`'s constructor beside
      `context_.uiBus`, each with its thread stated at the field as the
      other fields do. No MODIFIED delta of `sar-3`: an open change in the
      stack already modifies it, and the fields are an ADDED requirement of
      their own.
      Check: after `Start` on the rig, `context->clockDiagnostics->
      Snapshot().transportState == Running`; after `Stop`, `Stopped`;
      after `RequestSyncConfiguration({.receiveClock = true})`,
      `syncConfiguration().receiveClock`. Break: leave the field
      unpacked; red on Running.

## S6. Stack assembly (local; the push is 8.2)

- [ ] S6.1 Place each S task by the placement rule, amending in stack order
      and rebasing every branch above each amendment; then branch
      `app-commands-on-the-bus` from the new tip for the new-branch work
      and the two change directories. `fork/app-midi-out` (the scrapped
      MIDI-out record) stays on its old base and is not rebased or pushed.
      Report, per PR, the placement decision and the
      `git range-diff <old parent>..<old head> <new parent>..<new head>`
      output.
      Check: the ancestry walk from the lowest open PR's branch to
      `app-commands-on-the-bus` reports every step stacked.
- [ ] S6.2 `projects/synth test`, every deadline test by path, the miniapp
      target, the browser build, at the new tip.

## 1. Hygiene sweep and the local pin

- [ ] 1.1 Sweep `app/`, `app/vst/`, `openspec/` (excluding
      `openspec/changes/archive/` and this change's own directory),
      `MANUAL.md`, `README.md` and
      `External/Sheaf/projects/synth/{include,src,tests}`, case-insensitive,
      word-bounded, whole tree, output kept as the enumeration, for two
      lists. Removed names: `RequestPageSelect`, `RequestEncoderPress`,
      `RequestRandomizeAll`, `RequestRandomizePage`, `RequestResetAll`,
      `RequestResetPage`, `RequestTempoBpm`, `pendingPageSelect_`,
      `pendingEncoderPress_`, `pendingRandomizeAll_`,
      `pendingRandomizePage_`, `pendingResetAll_`, `pendingResetPage_`,
      `pendingTempoBpmRequest_`, `tempoDisplayBpm_`,
      `tempoExternallyClocked_`, `transportRunningDisplay_`,
      `DisplayTempoBpm`, `TempoExternallyClocked`, `FroggersPressQueue`,
      `InputOrder`, `HasOrderedPresses`, `RunIfDepthStorageCovers`,
      `DepthStorageCovers`, `RandomizeNewDepthBound`; and, as a separate
      grep, `TransportRunning()` as a call on the app core (not
      `IsTransportRunning`, `SetDesiredTransportRunning` or the
      `*TransportRunning_` members, which stay). Added names: `AppCommand`,
      `ApplyAppCommand`, `HasAppCommands`, `SetStorageLowWatermark`,
      `FroggersCommand`. Every removed-name hit is a site task 3 changes, a
      comment task 6.1 rewrites, or a test task 7.1 moves or deletes.
      Check: after task 8.1, the removed-name grep over those paths finds
      nothing, and the added-name grep finds each name at its definition
      site and its call sites.
      Prose the grep cannot match is listed under 6.1 and 7.1.
- [ ] 1.2 Point `External/Sheaf` at the local new stack tip from S6.1 for
      every build and test below; the pin is committed with its pushed SHA
      at 8.2.

## 2. Carry the kept `frogg3rs-o1-audit` work

- [ ] 2.0 The ratified story and the comment hygiene. Carry
      `openspec/story.md` from the `o1-audit` branch tip with these edits
      and no others: in the section 2.3 paragraph "Randomize never stops
      short", keep the operator's quoted words and the "fix required" line;
      rewrite the defect sentence to what the code does (a press zeroes a
      knob's existing depths and draws new ones in one frame; the release
      that follows in the same frame frees none of them, because it reads
      centres the recompute after it has not yet updated, so they are freed
      by a later frame's release, and only the message thread adds storage,
      so a press can need old plus new slots at once); delete the sentences from "Applied (coordinator, open
      to the operator's objection)" through "(coordinator's application,
      open to the operator's objection)", keeping the quoted SR words that
      follow them as the ruling that presses keep their count and order;
      rewrite the section 4 "Materialized modulation depths" cell's
      fix-required text to the mechanism as built (the launch constant is
      the watermark; a patch applies only with its storage, at startup and
      running); rewrite the two CODE pointers that name
      `pendingPageSelect_` (the modulation section's level line and the
      inventory row for the drill) to `FroggersCommand` and
      `ApplyAppCommand`; rewrite MOD-10's code pointer (the `HandleAction`
      `kPagePrevious`/`kPageNext` `DrillLevel() == 0` gate) to
      `ApplyAppCommand`'s gate on the drill level. Carry commit 0eefd3f's change-history comment
      rewrites in `app/` other than `app/FroggersUiSurface.hpp`, whose
      present-tense rewrite on this branch stands. Not carried: the
      `frogg3rs-o1-audit/` and `frogg3rs-operator-runs/` change directories
      (2.7 carries the latter on its own terms), `evidence/`, and the Sheaf
      pin hunk.
      Check: `diff <(git show o1-audit:openspec/story.md) openspec/story.md`
      shows only the lines listed above (the defect sentence, the deleted
      application sentences, the section 4 cell, the two
      `pendingPageSelect_` pointers and the MOD-10 pointer), and the report
      lists each rewritten line; `app/check_no_planning_history.py` green.
- [ ] 2.1 Record re-arm race (commit 6c317dd, the Record hunks; delta: the
      "Arming a take never touches the buffer" requirement in this change's
      transport delta).
      Check: the thread-sanitizer probe `evidence/runs-F/tsan_probe.cpp`
      with its `evidence/runs-F/build_cmd.txt` (Homebrew llvm clang++ with
      `-isysroot`, scratch paths rewritten to the tree), both kept under
      this change's `evidence/`, reports no race on the fixed tree; its
      `.out` files are not carried.
- [ ] 2.2 Reset returns to the launch state through
      `Parameter::RevertAllToDefault` and takes what it resets out of every
      gesture (commit 6c317dd, the Reset hunks; commit 8a6c862, the reset
      comparison's positive control derived from a fresh launch). RST-01,
      RST-04, GES-03. The carried `ZeroExistingModulationDepths` and
      `ResetExistingModulationDepths` are one loop differing by the
      gesture clear; carry one function with the gesture clear as a
      parameter, called by Reset's drilled branches and the Randomize draw.
      Check: `app/FroggersModulationTests.cpp`'s carried cases
      `reset_all_after_drilled_randomize_equals_a_fresh_launch_including_which_depths_exist`
      and `reset_with_a_gesture_button_held_matches_a_fresh_launch`: every
      parameter, every depth's existence and every gesture mask equal a
      fresh rig's after Randomize All, edits, and Reset All. Break: skip
      `RevertAllToDefault` on one bank; red.
- [ ] 2.3 WRLD.Bldr out of the Preset selector, manual sections, gestures
      over MIDI, Launchpad "Model" caption (commits 8657686, e9a3d37,
      518c6fa, and f4f0d24's `app/FroggersMidiCatalogTests.cpp` hunk).
      CTL-01, GES-01 to GES-04, story section 5. Their spec deltas are
      copied into this change's `specs/` and merged with the deltas already
      there for the same spec.
      Check: the carried checks in `app/FroggersMidiCatalogTests.cpp` and
      `app/FroggersControllersPageTests.cpp` pass; a Twister row mapped to
      gesture 1 fires the gesture on the rig (break: unmap; red);
      `app/check_no_planning_history.py` green.
- [ ] 2.4 Controllers page row build, export without the copy, worker
      encode, capture buffer zeroed in parts (commit c186dee, including its
      "Stopping a long take never holds a UI tick" delta, copied into this
      change's `specs/`), plus commit 842c760's `app/FroggersAppCore.hpp`
      hunks (the capture buffer allocated uninitialised until its parts are
      zeroed; `StopRecording`'s seq_cst order with the export). Needs
      `HasMessageThreadTick` (S4.1).
      Check: the carried case
      `a_stopped_take_is_encoded_off_the_message_thread_to_the_same_bytes`
      in `app/FroggersSurfaceTests.cpp` (byte equality with
      `EncodeWavPcm16Mono`); break: drop one sample before encoding, red.
- [ ] 2.5 The citation gate that refuses line-number citations into Sheaf,
      and the symbol-named citations (commit 7100004, plus commit 842c760's
      `app/check_citations_resolve.py`, `app/check_common.py`,
      `app/FroggersDspParityTests.cpp` and `app/dsp/` hunks). Every
      line-number citation into Sheaf and every bare line number the gate
      then still reports, in any file, is rewritten by symbol.
      Check: `app/check_citations_resolve.py` green on the tree, and red on
      one planted line-number citation into Sheaf.
- [ ] 2.6 Not carried, recorded here so the after-code audit can confirm
      absence: o1-audit 5.6, 5.7, 8.4; 1086a7e entirely; the `evidence/`
      outputs; every test o1-audit added for `FroggersPressQueue`,
      `InputOrder`, `OldestPendingPressSequence` or a storage wait;
      842c760's sequence-stamping and storage-wait-test hunks and its
      spec-delta hunk (this change's `froggers-modulation-slate` delta
      supersedes it).
- [ ] 2.7 `frogg3rs-operator-runs` and Sheaf `app-operator-runs`: carried
      and left open, as the operator's ruling that created them stands
      (operator runs never hold delivery; fixes gated on them ship marked
      not yet delivered). In both `proposal.md` and `tasks.md` of each:
      every reference to a `frogg3rs-o1-audit` or `app-o1-audit` task,
      measurement, commit or evidence path is replaced by its recorded
      result stated inline, dated to the run (2026-09-23 or 24), citing no
      commit, branch or path that will not exist on a remote after 8.2;
      runs 1.7 and 1.8, whose implementations were the dropped Sheaf 5.6
      and 5.7, say the implementation is to be written if the run is a
      finding; a run whose subject is a dropped task is removed. The Sheaf
      directory sits on the new branch.
      Check: `openspec validate` green on both; no reference into a path,
      task, commit or measurement that does not exist on the branch or a
      remote.

## 3. Presses on the bus

- [ ] 3.1 `FroggersCommand`, an enum of the eight presses (page select,
      page previous, page next, encoder press, Randomize All, Randomize
      Page, Reset All, Reset Page), and
      `FroggersAppCore::ApplyAppCommand(std::size_t command, float value)`.
      It switches over `FroggersCommand` with no `default`, reads
      `context_->parameterManager` unguarded (`Init` refuses a null
      context or manager), keeps the page and encoder bounds checks, and
      runs the exact bodies `ProcessFrame` runs today for each command
      with the page or encoder position read from `value`. Page previous
      and next resolve their target from `activePageIx_` on the audio
      thread and are no-ops while `drillIn_`'s level is above 0, so two
      arrow presses in one tick advance two pages (SUR-03) and the gate the
      surface applied moves with them. A randomize or reset sets a
      recompute flag; `ProcessFrame` runs `ComputeAllParameters` once per
      block when it is set, as today, and the neutral-depth release stays
      where it is. No catalog lookup: the encoder press is not a catalog
      action (it is `catalog.encoderPressAction`, which Sheaf dispatches to
      the surface with the position as the action value), and the page
      select's catalog entries are one per page, so both arrive at the
      surface as a name and a value and leave it as a command and a value.
      The modulation test binary's dependency line in `app/Makefile`
      gains the app headers the test includes (`Froggers.hpp`,
      `FroggersAppCore.hpp`, `FroggersUiSurface.hpp`), so a header edit
      rebuilds it.
      Check: not yet delivered; this task adds a case to
      FroggersModulationTests.cpp: two Back presses dispatched in one tick
      leave the drill at level 0 from level 2 (MOD-09); two Page Next
      presses in one tick advance two pages (SUR-03); Reset All then
      Randomize All dispatched in one tick leave a randomized state,
      Randomize All then Reset All leave the launch state (RND-01, RST-01).
      Breaks, one per assertion: coalesce repeated commands of one kind in
      a drain, the two Back presses leave level 1, red; resolve the arrow's
      target in `HandleAction` from the published page, the two Page Next
      presses reach page 2 instead of 3, red; apply commands in kind order
      instead of bus order, the Reset-then-Randomize assertion goes red.
- [ ] 3.2 `FroggersUiSurface::HandleAction` maps the eight action names to
      `FroggersCommand` through one table and pushes
      `MessageIn::AppCommand(NowMicros(), command, value)` once, through
      `PushMessage`. The BPM branch pushes
      `MessageIn::SetTempoBpmNormalized(NowMicros(),
      (bpm - kFroggersBpmMin) / (kFroggersBpmMax - kFroggersBpmMin))`,
      keeping the externally-clocked guard on the value the context now
      exposes (S5). `PushMessage` is the one push site.
      Check: not yet delivered; this task adds a case to
      FroggersSurfaceTests.cpp: dragging the slider to 300 on the rig reads
      300 from `ClockDiagnosticsSnapshot().currentBpm` after one block
      (TRN-02, BPM-01); with receive-clock requested, the slider renders as
      the read-only line and a dispatched `kBpm` pushes nothing (SYN-03).
      Break: push the raw BPM as the normalized value; red at 300.
- [ ] 3.3 Delete `Request*`, the seven press atomics, the three tempo and
      transport mirror atomics, and their `ProcessFrame` drains. The surface
      reads tempo, external clock and transport-running from `AppContext`
      (S5), and so does `FroggersAppCore::ArmRecording`, the one reader of
      the deleted accessor inside the core. Every remaining cross-thread
      member is declared with its class at its declaration: values written
      by the message thread (`pendingExternalAudioRouted_`, `freezeLatched_`,
      `recordArmed_`, which the audio thread's length-cap stop also clears,
      and `desiredTransportRunning_`, whose reader is `PrepareToPlay` on
      whichever thread calls `Engine::Prepare`); audio-thread publications
      (`drillLevelDisplay_`, `lastRandomizePartial_`, `recordFrames_`,
      `recordTruncated_`); and `recordWriterInBlock_`, the Record writer
      handshake the audio thread raises around a block, which is outside
      the rule and says so. `DrillLevel()` stays.
      Check: the build and task 1.1's grep; break: leave one
      `RequestPageSelect` caller, the build and the grep go red.
- [ ] 3.4 The plugin's page restore (`app/vst/FroggersPluginProcessor.cpp`,
      the `RequestPageSelect` site) calls
      `Application().PortableSurface().DispatchAction(ui::Action::WithValue(
      kPageSelect, std::to_string(page)))` on the message thread, and the
      comment at that site (which says `RequestPageSelect` is the one
      authority that also reconstructs the drill-in and describes
      `ProcessFrame`'s bounds check) is rewritten for the dispatched press.
      Check: `app/vst/FroggersVstHostTests.cpp`'s restore check restores a
      non-zero page and reads it from `uiState` after one block (PLG-14);
      break: skip the `DispatchAction` call at the restore site, the
      visible page reads 0, red.
- [ ] 3.5 The push contract. `MessageInBus::Push` is single-producer;
      enumerate every `uiBus->Push` and `UiBus().Push` caller in `app/` and
      `app/vst/` and confirm each runs on the message thread (JUCE message
      thread, browser main thread, the rig's caller). `PrepareToPlay`'s
      `MessageIn::Start` push is reported as open pending the carried
      operator run on the plugin's `prepareToPlay` thread (BUG-05 in
      `frogg3rs-operator-runs`).
      Check: the list; a caller on any thread other than the message
      thread is a blocking finding.

## 4. Storage: a press never draws short, a patch never loads short

- [ ] 4.1 The watermark. `FroggersModulationSlate::Init` sets the group's
      low watermark to `kDepthParameterStorageCapacity`, the constant that
      already sizes the launch batch as the most depths one press can
      create; one constant, two uses. `LastRandomizePartial()` and the
      `partial` result stay as the observable. Not built, by the audit's
      runs: a per-press bound at `Init`, a hold, and a zero-then-release
      pass before the draw.
      Check: not yet delivered; this task adds four cases to
      FroggersAudioRoutingTests.cpp, each a reproduced player path driven
      through the rig at one message tick per six blocks (the way
      `arming_after_an_unpolled_truncated_capture_flushes_it_first` in
      `app/FroggersSurfaceTests.cpp` already runs blocks without a tick):
      after a drill routine on two pages that grows live depths past the
      launch storage (drill into each parameter, Randomize All at level 1,
      open each level-2 view, Randomize All there, Back), a parameter-page
      Randomize All reports no partial draw (RND-01); a patch saved from
      that session, opened on a fresh rig created on the same data paths
      (the startup path, QR-01), has every depth live before the first
      block; the same patch loaded with `LoadPatch` on a running rig
      (FILE-07, PLG-10) has every depth live after the retry; a patch that
      fits on its own, loaded in the same tick as a Randomize All, leaves
      the press whole (RND-01). Breaks: set the watermark to Sheaf's
      default, the page press goes red (the audit's `page.out` reproduces
      it at 58 available); skip the need in S3.1's provisioning (add the
      watermark only), the startup and running patch cases go red (the
      review's variant 0 reproduces 903 missing).

## 6. Comments and documentation

- [ ] 6.1 Replace the "Why a request bridge exists" block in
      `app/FroggersAppCore.hpp` and the matching block in
      `app/FroggersUiSurface.hpp` with the command, value or publication
      rule, stated once at `FroggersAppCore`. Rewrite every comment task
      1.1's grep found that describes the bridge, the pending atomics or
      the drain order, and the prose the grep cannot match, found by
      `grep -rniE 'Request\*|pending\*|request bridge|app-request|pending[- ]request|single-slot|coalesc|Display\* API'`
      over `app/`: `app/Froggers.hpp` (the bridge sentence in its header),
      `app/FroggersUiSurface.hpp` (the kRecord comment naming the bridge,
      and the "App-request bridge" comment above the press branches, which
      is already false today since page select and both Resets go through
      it), `app/FroggersAppCore.hpp` (the single-slot and coalescing
      comments on the request API, the tempo drain and the members, the
      release and recompute comments around `ProcessFrame`'s
      `CollectNeutralLocalParameters` and `ComputeAllParameters` calls
      where they describe the bridge, and the `recordArmed_` block that
      contrasts itself with the bridge), `app/vst/FroggersPluginProcessor.cpp`
      (the host-automation comment that says a request "would not apply
      until the block after", false today since the drain precedes
      `ProcessFrame` in the same block), `app/vst/FroggersPluginProcessor.hpp`
      (the `pendingTransportEdge_` rationale, "same idiom as
      `pendingPageSelect_`", which stops existing), and
      `app/dsp/Limiter.hpp` (the repro note naming `RequestRandomizeAll`).
      Carried false-comment fixes apply where their site survives.
      Check: `app/check_no_planning_history.py` green; the 6.1 grep above
      over `app/` finds only the JUCE editor's own coalescing comments
      (`app/vst/FroggersPluginEditor.hpp`, `app/vst/FroggersPluginEditor.cpp`,
      `app/vst/FroggersVstEditorTest.cpp`), the Runtime repaint-hook
      comment in `app/vst/FroggersPluginProcessor.hpp`, and
      `pendingTransportEdge_`'s own single-slot comments, each of which is
      true and describes something other than this bridge.
- [ ] 6.2 MANUAL.md: nothing the player sees changes; confirm by diffing the
      transport, Randomize/Reset and BPM sections against main and
      reporting no delta beyond the carried sections from 2.3.

## 7. Tests on the rig

- [ ] 7.1 Every test that called `Request*`, `DisplayTempoBpm`,
      `TempoExternallyClocked` or `TransportRunning()` on the app core
      (five files on main: `app/FroggersAudioRoutingTests.cpp`,
      `app/FroggersHeadlessTests.cpp`, `app/FroggersMidiCatalogTests.cpp`,
      `app/FroggersSurfaceTests.cpp`, `app/vst/FroggersVstHostTests.cpp`)
      drives the same scenario through the rig or the surface's
      `DispatchAction`. A test that existed only to exercise the bridge is
      deleted. The two routed-input tests in `app/FroggersHeadlessTests.cpp`
      keep calling `ProcessFrame()` on a hand-built context, because the
      routed-input value stays there. Test comments that describe the
      bridge (the file headers of `app/FroggersSurfaceTests.cpp` and
      `app/FroggersHeadlessTests.cpp`, and the in-test comments in
      `app/FroggersMidiCatalogTests.cpp` and
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
      host suites CI runs, the browser e2e, all against the local Sheaf
      tip; the three openspec gates (`app/check_modified_requirements_restate_promoted.py`,
      `app/check_spec_checks_resolve.py`, `app/check_artifact_symbols_resolve.py`)
      with the change tracked.
- [ ] 8.2 Push. Force-push every changed Sheaf branch to the fork (the open
      PRs update); push `app-commands-on-the-bus` and open it as the next
      sequential PR against upstream main; commit the frogg3rs pin at that
      pushed SHA; push frogg3rs to main. Then nothing stays on a local
      branch: remove this change's worktree, delete the local `o1-audit`
      and `app-o1-audit` branches and this change's local branches (fork
      branches that head open PRs stay), and fast-forward the primary
      checkout's `main` to `origin/main` with `External/Sheaf` checked out
      at the pin main records.
      Check: `git worktree list` shows the primary checkout alone;
      `git status -sb` reads `main...origin/main` with no divergence;
      `git submodule status` shows no `+`.
- [ ] 8.3 Archive this change and the Sheaf change (its proposal and
      specs); delete the session scratchpad.
