Every new test is shown to fail with its production change reverted, by the
executor, before the task is reported done, and the report says so. The
executor's deliverable is a report; code changes are a side effect of it.

Groups 1 to 6 were executed on the previous base, frogg3rs `b0c03a9` with
Sheaf `f73d4202`, and carried onto the next base by group 7. A Check that
reports a run's counts reports that run; group 9 repeats every run on the
current base.

The open work runs in this order: the postflight (9.0), which reads the
documentation, then 9.1, 9.2 and the commit to this worktree (9.3). Group 10
runs after the operator approves the 9.2 screenshots.

## 1. Sheaf: shifted encoder turns

Work in this worktree's `External/Sheaf`, on the branch
`shifted-encoder-turns`, based on `f6266560`, the tip of
`fold-controller-wizard-into-add-row` (jvictor0/Sheaf#19).

- [x] 1.1 Initialize the worktree's `External/Sheaf` and cut the branch. The
      stack tip exists only on the fork, so the checkout gets its objects from
      the main checkout's `External/Sheaf` rather than from upstream.
      Check: `git -C External/Sheaf merge-base HEAD e8894727` prints
      `e8894727…` and `git -C External/Sheaf branch --show-current` prints
      `shifted-encoder-turns`.
- [x] 1.2 Create the Sheaf change, `External/Sheaf/openspec/changes/shifted-encoder-turns/`,
      in the shape of `shift-and-file-export`: a proposal holding the Sheaf
      half of this change's proposal (What Changes under "Sheaf", the
      Shift + Crunchy data flow, the sweep finding, the Restore fix, the
      Evidence for Sheaf), a tasks file holding tasks 1.3 to 1.9, and the two
      spec deltas (smi-16 and smi-17; sru-10, sru-15 and sru-66).
      Check: `openspec validate --strict shifted-encoder-turns` passes in
      `External/Sheaf`; `openspec validate --strict frogg3rs-transport-and-shift-ux`
      passes here; this change has no `sheaf-specs/` directory.
- [x] 1.3 Add the scene-blend increment: NEW `MessageIn::Type::SceneBlendIncDec`
      appended after `Shift`, its factory, NEW `ParameterManager::IncDecSceneBlend`
      beside `ParameterManager::SetSceneBlend`, its `MessageInBus::Apply` case,
      and a case at every site in the proposal's `SetSceneBlend` family.
      Check: NEW `SceneBlendIncrementAddsToTheBlendAndClamps` passes; the
      proposal's Evidence lists each family site as changed or not needed,
      with the reason for the one not needed.
- [x] 1.4 Add the shifted job to the turn mapping: NEW `EncoderShiftedJob`
      (`None`, `SceneBlend`) as `shiftedJob` on `EncoderMidiMapping`, its JSON
      key written only when set, a missing key read as none, an unknown value
      failing the load, and a push mapping carrying one reported invalid by
      profile validation.
      Check: NEW `EncoderTurnJsonRoundTripsShiftedJobAndRejectsAnUnknownOne`
      and NEW `ProfileWithAShiftedEncoderPushIsInvalid` pass.
- [x] 1.5 Hand the profile's `ShiftState` to `EncoderMidiInProcessor` and
      handle a turn in the order the proposal gives: Hold Drill, then the
      shifted job while Shift is held, then the turn as on the base. Correct
      the `ShiftState` comment.
      Check: NEW `ShiftHeldTurnPushesSceneBlendIncrementAndReleaseRestoresTheParameter`,
      NEW `ShiftHeldAbsoluteTurnSetsTheSceneBlend` and
      NEW `HoldDrillDrillsAShiftedTurnWhileBothAreHeld` pass, and the existing
      `ShiftAndHoldDrillAreIndependent` still passes.
- [x] 1.6 Keep a turn with a shifted job out of `ReconstructEncoderBlocks`
      runs, and have a block expand to turns with no shifted job.
      Check: NEW `ReconstructEncoderBlocksKeepsAShiftedTurnOutOfItsBlock`
      passes.
- [x] 1.7 Controllers page: give Individual encoder-turn rows the Shift field
      with the choices none and Scene Blend, committed through the row's
      existing flush path; show the Shift field on system rows and turn rows
      only when the row dropdown offers Shift, as NEW
      `MessageCatalogOffersShift` answers it. This reaches these sites: the
      turn-row field list, which `BuildSectionRows` and
      `MidiConfigViewModel::GroupColumnFields` both read from NEW
      `EncoderTurnEditableFields`, the way system rows share
      `SystemRowEditableFields`; `ApplyMappingEdit`'s `EncoderMidiMapping`
      branch, whose `ShiftAction` case maps 0 to no shifted job and 1 to
      Scene blend; the `Field::ShiftAction` combo branch in
      `External/Sheaf/projects/synth/include/synth/ControllersPageUI.hpp`,
      which for a turn row offers NEW `EncoderShiftedJobCatalog()` and selects
      NEW `MidiConfigViewModel::EncoderTurnShiftedJobIndex` instead of using
      `ShiftCatalog()` and `ShiftChoiceIndex()`; and `SystemRowEditableFields`.
      Correct the `Field::ShiftAction` comment ("System row only") and the
      `GroupColumnFields` comment that calls the per-group field tables a
      single source of truth.
      Check: NEW `TurnShiftFieldEditCommitsSceneBlendAndNoneClearsIt`,
      NEW `TurnRowsExposeShiftFieldOnlyWhenShiftIsOffered` and
      NEW `NoShiftFieldWhenTheRowDropdownOffersNoShift` pass. NEW
      `TestTurnRowShiftComboOffersSceneBlendAndCommits` in
      `controllers_page_ui_tests.cpp` passes: on a page built with a catalog
      that offers Shift, a turn row's combo offers exactly none and Scene
      Blend, shows the turn's shifted job as selected, commits a change, and
      shows the committed value after a rebuild. The existing
      `ShiftFieldEditCommitsShiftedPressAndNoneClearsIt`,
      `SystemRowsExposeShiftFieldExceptOnShiftAndHoldDrillRows` and
      `TestSystemMessageShiftFieldRendersAndCommits` pass with a catalog that
      offers Shift; the `GroupColumnFieldsMatches…` parity tests pass with and
      without a Shift-offering catalog; `TestControllersRowFitsWithinFroggersNarrowestHost`
      passes with a shifted turn row in its open states.
- [x] 1.7a Correct the comment 1.7 added above the shifted turn in
      `TestControllersRowFitsWithinFroggersNarrowestHost`
      (`External/Sheaf/projects/synth/tests/portable_ui_tests.cpp`), which
      says "the Shift field this change adds": it describes the field by the
      change that added it rather than by what it is. Runs after 7.3; this
      is the Sheaf change's task 5a.
      Check: the comment names the turn row's Shift field without naming a
      change; `TestControllersRowFitsWithinFroggersNarrowestHost` passes.
- [x] 1.8 Run Sheaf's full `projects/synth` suite, and build and run the
      miniapp runtime target, which that suite does not build.
      Check: pass and fail counts reported per binary as measured; every
      failure is either fixed here or shown to fail identically at the base.
- [x] 1.9 In `ControllersPageSurface`, configure the view model
      `CommitLifecycleAction` builds exactly as the surface's own view model is
      configured (message catalog, analog action catalog, layouts from
      `ControllersPageCallbacks`), through NEW
      `ControllersPageSurface::ConfigureViewModel`, which both the constructor
      and `CommitLifecycleAction` call. On a successful Restore,
      `HandleRestoreController` drops that controller's cached section rows on
      the surface's own view model through NEW
      `MidiConfigViewModel::NoteControllerConfigReplaced`, beside
      `NoteControllerRenamed`, so an open section shows the restored mappings.
      Add this fix to the Sheaf change's proposal (What Changes, Impact, the
      additive list in Delivery) and tasks.
      Check: NEW `TestRestoreResolvesAnAppPreset` in
      `controllers_page_ui_tests.cpp` passes: on a page built with a registry
      made from a catalog whose device default has an app-specific wizard id,
      a row carrying that id whose config diverges from the preset is
      restored to the preset by the page's Restore action. It fails with the
      old `CommitLifecycleAction`. No Sheaf test covers the cache drop; task
      4.3's `twister_row_saved_before_the_shifted_turn_gains_it_on_restore`
      does, and fails without it.

## 2. frogg3rs: the Play plate shows the transport

- [x] 2.1 Give `BuildPlayDrawCommands` a `running` flag that swaps plate and
      glyph colours, and make the Play Draw node a lambda reading
      `FroggersAppCore::TransportRunning` on every rebuild. Correct the
      `BuildFreezeDrawCommands` comment that says the other two plates take no
      state flag.
      Check: NEW `play_draw_commands_swap_plate_and_glyph_colours_while_running`
      and NEW `play_plate_is_held_while_the_transport_runs` pass. The second
      drives Play, Stop, Play, Freeze and Freeze through the surface with
      blocks run between presses, and reads the Play node's draw commands
      after each.
- [x] 2.2 Correct the comment on `FroggersAppCore::TransportRunning`, which
      on the base names the BPM control as its only reader.
      Check: the comment names every reader the code has.

## 3. frogg3rs: releasing Freeze returns to the transport it stopped

- [x] 3.1 Record `FroggersAppCore::TransportRunning` in NEW
      `freezeEngagedWhileTransportRunning_` when Freeze engages. On release,
      call NEW `FroggersUiSurface::StartTransport` when the record is true and
      clear the latch alone when it is false. The kPlay branch calls the same
      `StartTransport()`. Correct the comments that describe the old release:
      the kFreeze branch comment; the header above
      `play_disarms_the_freeze_latch_and_returns_the_voice_gate_to_the_transport`;
      the quoted kFreeze branch in `PumpHostParameterBridge`
      (`app/vst/FroggersPluginProcessor.cpp`); the gate comment in
      `app/FroggersAppCore.hpp` that says a release tears the drone down; and
      the cleanup comment in `app/vst/FroggersVstHostTests.cpp`, which says
      the latch is released where the code only releases resources.
      Check: NEW `releasing_freeze_resumes_the_transport_it_stopped` passes
      (Play, run blocks, Freeze, run blocks, stopped and latched, Freeze, run
      blocks, running and unlatched). NEW
      `freeze_engaged_while_stopped_releases_to_silence_within_the_bound`
      passes and replaces freeze_latch_release_while_stopped_silences_within_the_bound
      (deleted): it engages Freeze on a stopped transport, confirms the drone
      is audible before the release, and asserts silence within the bound and
      a stopped transport after it. releasing_freeze_does_not_restart_the_transport
      is deleted (it no longer exists in the tree). Sequence 2's comment in
      `no_freeze_stop_press_sequence_leaves_the_instrument_sounding_after_stop`
      is corrected. The whole `app/FroggersAudioRoutingTests.cpp` binary
      passes.

- [x] 3.1a Correct two phrases in the kFreeze branch comment 3.1 wrote in
      `FroggersUiSurface::HandleAction` (`app/FroggersUiSurface.hpp`) that
      describe the code by its history: "exactly as the operator described
      it" and "same as before this branch existed". The comment says what
      the release does, not who asked for it or what the code did before.
      Runs after 7.3.
      Check: neither phrase remains in `app/FroggersUiSurface.hpp`; the
      comment still says what the release does in each case.

## 4. frogg3rs: Shift + Crunchy on the Twister

- [x] 4.1 In `TwisterDeviceDefault`, set the shifted job of the turn whose
      position is `kFroggersCrunchySlot` to Scene blend, and correct the
      catalog header comment's Twister paragraph.
      Check: `device_defaults_are_valid_and_address_exactly_the_documented_controls`
      asserts that the Twister's only shifted turn is at Crunchy's slot and no
      APC40 turn has one, and passes. NEW
      `twister_shift_turns_crunchys_knob_into_the_scene_blend` passes: it feeds
      a live Twister profile the raw MIDI a Twister sends (Shift down, encoder
      16 clockwise, Shift up, encoder 16 clockwise) and asserts the scenario's
      blend and Crunchy values after each turn. NEW
      `twister_crunchy_turn_row_shows_its_shifted_scene_blend` passes.
- [x] 4.2 Commit the Sheaf work on `shifted-encoder-turns` and pin
      `External/Sheaf` at its tip.
      Check: `git -C External/Sheaf log --stat e8894727..HEAD` shows the
      implementation and tests, not only openspec text; `git ls-tree HEAD External/Sheaf`
      names the commit `git -C External/Sheaf rev-parse HEAD` prints.
- [x] 4.3 Through the Controllers page's own Restore action, a Twister row
      whose config is the preset as it was before this change (no shifted
      job on any turn) becomes the current preset.
      Check: NEW `twister_row_saved_before_the_shifted_turn_gains_it_on_restore`
      in `app/FroggersControllersPageTests.cpp` passes. It opens the row's
      Encoders section and reads it through the page's view model: before
      Restore the row does not match its preset and its turns form one block;
      after the page's Restore action the open section shows Crunchy's own
      row with Scene blend and the row matches its preset; after one further
      encoder edit on that row, Crunchy's turn still carries Scene blend. It
      fails with the old `CommitLifecycleAction`, and fails without the
      cache drop.

## 5. frogg3rs gates

- [x] 5.1 Run `make test` in `app/`, then every test binary by path, since
      `make test` stops at the first failing binary.
      Check: pass and fail counts per binary, as measured. The two 96 kHz
      deadline tests known to fail on this Mac are named as such; every other
      failure is fixed in this change.
- [x] 5.2 Build the playable app with `./app/build-launcher.sh`, and the
      plugin target.
      Check: both build; `Frogg3rs.app`'s `CFBundleExecutable` names the
      binary in `Contents/MacOS/`.
- [x] 5.3 Once each test named on a `not yet delivered` Check line in this
      change's `specs/` exists, rewrite that line as an ordinary Check line
      naming the test in backticks, file and case, so the gate resolves it.
      Check: no `not yet delivered` line is left in this change's `specs/`.

## 6. Postflight, documentation and screenshots

- [x] 6.1 One postflight over the whole change, Sheaf and frogg3rs
      together, by a context that wrote none of the code, after groups 1 to 4
      land. It may be split across delegates, for example one per story, but
      every delegate reads both trees, since no story can be judged from one
      side of the pin. It is driven by the three stories: it compares the
      landed diff with this proposal and, per story, builds and runs rather
      than reads: it reverts each production change itself and watches that
      story's test go red, deleting the test binary before each rebuild; and
      it tries plausible wrong implementations against the story tests (a
      Freeze release that always restarts the transport, a Play plate reading
      the surface's desired-running flag instead of the clock, a shifted job
      applied to a push or winning over Hold Drill) and reports which get
      through. It adds no tests or gates for hygiene fixes and does not
      re-audit its own repairs unless a repair touches a story's code path;
      two findings of the same kind stop it for a rethink. Nothing is
      committed until the postflight passes.
- [x] 6.1a After the last edit, run both full suites on the exact tree to be
      committed: every test binary by path in both trees and the miniapp
      runtime target.
      Check: per-binary counts as measured, from a run started after the
      final edit.
- [x] 6.2 After postflight passes, the documentation step: `MANUAL.md` (Play's
      held plate, Freeze release, Shift on knobs, Shift + Crunchy in the
      Twister subsection), `QUICK_DICT.md` (the Play / Stop and Freeze /
      Record entries), and the Sheaf documentation the Sheaf change touches.
      The Twister subsection says: a Twister row added from the MIDI Fighter
      Twister preset before this version keeps its old mappings and shows
      Restore; pressing Restore installs Shift + Crunchy and replaces edits
      made to that row; a patch saved earlier carries the old row, so load
      it, press Restore and save it again; a Twister row that shows no
      Restore predates the preset, so delete it and add the MIDI Fighter
      Twister preset again.
      Check: each statement about Play, Freeze release, Shift and the Twister
      is true of the built app.
- [x] 6.3 Capture the Delivery Gate screenshots from the built app for the
      operator.

## 7. Carry the change onto the current base

- [x] 7.1 Rebase this branch onto `b06ba16` and `shifted-encoder-turns` onto
      `e8894727`. The base's Controllers page has no Release or Reclaim
      action, so the lifecycle fix keeps only its Restore half, and its test
      is `TestRestoreResolvesAnAppPreset`.
      Check: `git merge-base HEAD b06ba16` prints `b06ba160…`;
      `git -C External/Sheaf merge-base HEAD e8894727` prints `e8894727…`;
      `TestRestoreResolvesAnAppPreset` is defined in
      `External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp`.
- [x] 7.2 Teach the base's Twister manual diagrams the shifted turn:
      `app/GenerateTwisterManualLabels.cpp` writes a NEW `shiftedTurn` label
      on every encoder row from `EncoderShiftedJobCatalog()`;
      `app/render_twister_manual_diagrams.mjs` accepts `shiftedJob` and, in
      the Shift-held diagram, draws a shifted turn's label in place of its
      ordinary one; `make -C app manual-diagrams` regenerates
      `assets/manual/twister-controls.json` and
      `assets/manual/twister-preset-shift.png`; `MANUAL.md`'s alt text for
      the Shift-held diagram says Crunchy's knob reads Scene Blend.
      Check: `make -C app check-twister-manual-diagrams-drift` passes, and
      the label file's only encoder row whose `shiftedTurn` is not "(none)"
      is position 15, Crunchy, with "Scene Blend". That check reads the label
      file and `MANUAL.md`'s side-button table, not the PNGs:
      `assets/manual/twister-preset-shift.png`, viewed, shows the bottom
      right encoder reading Scene Blend with Shift held.
- [x] 7.3 Rebase this branch onto frogg3rs `main` at
      `37c1b9c`, and `shifted-encoder-turns` onto `f6266560`, the Sheaf
      commit `37c1b9c` pins. `f6266560` is not in this worktree's
      `External/Sheaf`; fetch it from the main checkout's `External/Sheaf`,
      as task 1.1 did. Pin `External/Sheaf` at the rebased
      `shifted-encoder-turns` tip. Then update every statement in this
      change's and the Sheaf change's proposal and tasks that names the base
      (`b06ba16`, `e8894727`, #19 as the stack tip) to the new one; an
      Evidence block keeps the commit it ran at and says so.
      Check: `git merge-base HEAD 37c1b9c` prints `37c1b9c…`;
      `git -C External/Sheaf merge-base HEAD f6266560` prints `f6266560…`;
      `git ls-tree HEAD External/Sheaf` names the commit
      `git -C External/Sheaf rev-parse HEAD` prints;
      `check-spec-checks-resolve`, `check-citations-resolve`,
      `check-modified-requirements-restate-promoted`,
      `check-no-planning-history`, `check-artifact-symbols-resolve`,
      and both `openspec validate --strict` runs pass.
- [x] 7.4 After 7.3, build the plugin's test binaries and run
      `FroggersVstHostTests`.
      Check: `state_information_save_and_restore_never_write_the_shared_data_root`
      passes, which is what the proposal's "Failure present on the base"
      section records as reported and not yet verified. If it fails, the
      failure stands as a finding of this change: that section is rewritten
      to say so, with the run's output, and 9.1 names it.
- [x] 7.5 A context that wrote none of it reviews what the rebases authored,
      since nothing authored enters a push until such a context has read it:
      every conflict resolution in both trees
      (`git range-diff b0c03a9..f37d586 b06ba16..4ab820f`,
      `git -C External/Sheaf range-diff f73d4202..ddb0b106 e8894727..751e82e0`,
      and the same two for 7.3's rebase) and the task 7.2 edits, read against
      this proposal and the base's Controllers page.
      Check: every finding is fixed or recorded as open in this change before
      9.1 starts.

## 8. frogg3rs: the Filter page's limiter moves to the page's output

The proposal's section "The Filter page's limiter moves to the page's output"
holds the story, the operator's ruling, the measurements and the "Decisions"
preflight ruled on frogg3rs `37c1b9c` with Sheaf `f6266560`. Every new test,
and every test whose assertions change, is shown to fail against the
behaviour it guards -- `FilterFxChain::Process` at the old placement, unless
the task names a different break. A test changed only in comments, labels or
names needs no such run. The executor deletes the test binary before each
rebuild in a break-and-restore sequence. The measurement harness is in
`openspec/changes/frogg3rs-transport-and-shift-ux/evidence/limiter/`: build it
with that directory's `setup.sh` and `Makefile`, as the proposal's
Measurements section shows.

- [x] 8.1 Move the limiter. In `FilterFxChain::Process`, the peak branch's
      trimmed output goes to the Comb/Peak blend unlimited, and `Process`
      returns the limiter applied to the blend, at its current four tuning
      constants and values, unconditionally, with no comb-specific limiter.
      The default patch's level: no compensation. The move adds no gain
      stage before or after the limiter, and changes none of the four
      tuning constants' values.

      Correct every comment the move makes false:
      - `app/dsp/FilterFx.hpp`:
        - the tuning comment above the constants ("inserted after the peak
          branch's own `1/height` scalar trim");
        - the member's comment: its placement, "NOT applied to
          `filterOut`/the composite", and "the comb branch is already
          provably bounded";
        - the `FilterFxChain` constructor comment's "would silently
          under-tune this branch";
        - `Process`'s header comment ("Every other computation (combTrim,
          peakTrim, peakLimiter, the Comb/Peak blend) stays exactly as it
          was", and the processing-order sentence);
        - the comment above the limited peak line;
        - the comb-trim comment: `|comb| <= A + |fb|` is false below Comb
          drive 1 (measured in the proposal); "normalizes the worst case
          ... to exactly 1.0" is false at every Comb drive -- at A=0 the
          worst case is 0.95/1.95 = 0.487, and only A=1 gives 1.0;
        - `Comb::GetFeedback`'s "the fed-back term can never exceed
          |fb|*1.0 no matter how large |fb| is", and the Comb drive
          comment's "does not reopen that same concern from the low-drive
          end", both false below Comb drive 1 (the proposal's `combbound`
          measurement).
      - `app/dsp/Limiter.hpp`: the header comment naming a second instance
        "on the Filter bank's peak branch", the per-stage lists that name the
        peak stage, the VST note's "peak-branch instance", and the per-stage
        list that gives Delay and Reverb a 0.9 threshold while
        `kDelayWetLimiterThreshold` and `kReverbWetLimiterThreshold` are
        both 0.72 and calls the tuning "identical at all four sites".
      - `app/FroggersAppCore.hpp`:
        - the `filterChain_.Configure` comment in `PrepareToPlay`;
        - the comments on the limiter accessors, including the sentence in
          `TestOutputLimiter`'s own comment that points to the peak branch's
          own limiter instance;
        - the output-limiter comment naming the peak branch;
        - the recovery comments naming `filterChain_.peakLimiter`;
        - `RouteFilterBank`'s "The comb feeding this stage is bounded near
          |in| + 0.95", which is false below Comb drive 1 (measured in the
          proposal).
      - `app/dsp/Delay.hpp`, `app/dsp/Drive.hpp` and `app/dsp/Reverb.hpp`:
        the comments naming the limiter's place.

      FOUND before the change, per file, from
      `git grep -n -i -E "peakLimiter|peak[- ]limiter|peak[- ]branch('s)?( own)? limiter|kPeakLimiter|TestFilterPeakLimiter" -- app openspec/specs`:
      `app/FroggersAppCore.hpp` 9, `app/FroggersDspParityTests.cpp` 39,
      `app/dsp/Delay.hpp` 3, `app/dsp/Drive.hpp` 3, `app/dsp/FilterFx.hpp` 25,
      `app/dsp/Limiter.hpp` 5, `app/dsp/Reverb.hpp` 1, and
      `openspec/specs/froggers-sheaf-parameter-model/spec.md` 1. The widened
      alternative catches the "OWN limiter" phrasing a line-wrapped instance
      still slips past -- which is why `TestOutputLimiter`'s comment is
      named above by hand rather than left to the grep. The report gives
      FOUND against CHANGED per file from this grep.
      Check: every comment listed above is true after the move, including
      the corrected bound statements; the four tuning constants' values are
      unchanged, read from `app/dsp/FilterFx.hpp`; `make check-citations-resolve
      check-no-planning-history` passes in `app/`.
- [x] 8.2 The rename is ruled in. `peakLimiter` becomes NEW `outputLimiter`
      (`DriveBlendPhase::outputLimiter` already gives the Drive page's last
      stage this name). The four constants become NEW
      `kFilterOutputLimiterThreshold`, NEW `kFilterOutputLimiterCeiling`, NEW
      `kFilterOutputLimiterAttackSeconds` and NEW
      `kFilterOutputLimiterReleaseSeconds`, keeping their values, with the
      static assertion that threshold stays below ceiling following them.
      `FilterFxChain::ForEachStatefulUnit` visits `outputLimiter`. The test
      locals named refPeakLimiter follow the rename.

      Delete the accessor FroggersAppCore::TestFilterPeakLimiter, which has
      had no caller since commit e96ae19, and its own comment above it;
      delete the sentence in `TestOutputLimiter`'s comment that points to
      it. No accessor replaces it. In the same accessor block, delete
      FroggersAppCore::TestDriveBlendPhase, which has no caller.

      Check: `git grep -n -i -E 'peak_?limiter|TestFilter(Peak|Output)Limiter|TestDriveBlendPhase' -- app`
      prints nothing.
- [x] 8.3 Add NEW `filter_fx_chain_limits_the_blended_output_so_a_resonant_comb_is_limited`
      to `app/FroggersDspParityTests.cpp`, using the limiter's names as 8.2
      settles them.
      - **Chains.** Two `FilterFxChain`s configured at 48 kHz: the shipped
        one, and one whose Filter output limiter is configured with
        threshold 1e6 and ceiling 2e6, the neutralizing idiom the file
        already uses. A fresh `OutputLimiter` is configured from the
        limiter's four tuning constants.
      - **Settings, both chains.** Peak and scoop notch at the registered
        defaults' mapped values: frequency 100 Hz / 48 kHz, width 0.4,
        height 1. A default-constructed `ResonantBump` has frequency 1000
        cycles/sample and diverges; the prototype showed an unlimited peak
        of 298 until the frequencies were set. `pureDelay` 1 sample, comb
        delay 100 samples, comb feedback 0.95, comb cutoff alpha 1.0, Comb
        drive 0.25.
      - **Drive.** `Process(0.25 * sin at the comb's pitch, topology 0,
        blend 1.0, scoop 0)` for 2 s.
      - **Assertions:**
        1. the neutralized chain's peak exceeds the limiter's threshold,
           which is the liveness control;
        2. every shipped sample equals the fresh limiter applied to the
           neutralized chain's sample, exactly;
        3. the shipped peak is below the neutralized peak.

      Check: the test passes. With `Process` reverted to the old placement,
      assertion 2 fails. The prototype (`bin/pt_cur`, built from the
      proposal's harness) measured, on the old placement, 95380 of 96000
      samples differing and the shipped limiter's envelope staying at
      1.0000. Then the spec delta's "not yet delivered" Check line is
      rewritten to name the test in backticks, file and case.
- [x] 8.4 Update the DSP parity tests the move turns red, and every passing
      test whose comment, label or figure describes a peak-branch limiter.
      `ProcessFilterBankPeakVariant` follows the new placement: the peak
      branch is unlimited, and the limiter processes the blended composite.

      - `filter_fx_chain_parallel_matches_manual_comb_peak_scoop_blend` and
        `filter_fx_chain_blend_extremes_hold_other_branch_at_floor_gain`:
        their replicas apply the limiter after the blend. Correct the first
        test's comment that the limiter is "expected to act as an exact
        identity here" and the second test's comment that one replica
        "serves both knob positions", both of which the move makes false.
      - filter_bank_peak_branch_trim_versus_limiter_bound_on_pinned_comb:
        retire the four-cell comparison -- remove the four cells, the
        margins, the spread check, the pinned-comb liveness block, and the
        three margin constants (kUnboundedOverBypassedDb,
        kBypassedOverLimitedDb, kLimitedPairSpreadDb). Rename the remainder
        NEW `filter_fx_chain_limits_neither_branch_ahead_of_the_blend_on_a_pinned_comb`.
        At the existing corner, with the rewritten helper, it prints the
        worst replica-against-`Process` delta and the peak-branch and
        comb-branch peaks, and asserts:
        1. worst delta <= 1e-6;
        2. both branch peaks exceed `kFilterOutputLimiterThreshold`.
        If assertion 2 fails for either branch, stop and report; do not move
        the corner or change a bound. Add the comb-branch tap assertion 2
        reads, alongside the existing `PeakBranchTap::peakBranch`. Remove
        the helper's applyLimiter parameter and the PeakBranchTap member
        combFedBack if no reader is left after this rewrite, and report FOUND against
        CHANGED for each.
      - `filter_bank_peak_trim_removal_distortion_intermodulation_and_limiter_pumping`
        shares the helper and is not named by any task before this one.
        Print its readings before and after the move. Correct its comments
        that describe the peak branch as the node the trim and the limiter
        act on, the limiter left to bound the branch on its own, and its
        figures. If an assertion fails, stop and report; do not change a
        bound.
      - `filter_fx_chain_scoop_full_does_not_cancel_a_boosted_peak_at_the_shared_center_frequency`:
        its header describes "the peak branch -- and its own `peakLimiter`",
        and its helper `OldTopologyFilterFxProcess` calls the member on the
        branch. Determine what the helper models: if it replicates
        `Process`, it follows the new placement; if it models a law that no
        longer ships -- its own comment already calls it a frozen snapshot
        of the topology from before the scoop path was added -- its comment
        says so instead. Either way it follows the rename.
      - `peak_ceiling_scoop_modulation_limiter_measurement`: correct its
        header's claims that pre- and post-limiter worst cases land at the
        same order of magnitude, and that no finite-attack limiter can
        suppress the spike, to what the test prints after the move.
      - `peak_branch_output_respects_computed_bound_under_audio_rate_height_modulation_with_limiter`,
        `peak_ceiling_candidate_limiter_measurement` and
        `topology_morph_peak_branch_headroom_across_full_range`: correct
        each comment, label and name that describes a peak-branch limiter.
      - `filter_bank_peak_gain_travel_measurement_at_and_away_from_resonance`:
        - keep `kResonanceFlatnessDb` (0.01) and `kAwayFallDb` (4.0);
        - delete the broadband band, kBroadbandFallLowDb and
          kBroadbandFallHighDb, and its two asserts; replace them with
          `REQUIRE_TRUE(broadbandDb[kNumCols-1] < broadbandDb[0])`. Its
          control: `rawPeakTrim` set to 1 makes it fail;
        - remove every figure from the comments: 0.00025 dB, 6.04, 6.52,
          "about 2 dB under", 9.03, 7.12, the four seeds, 7.09, 7.13 and
          "fifteen times";
        - each bound's comment states what it separates: flatness -- the
          trim cancels the centre gain exactly, and 0.01 is 400 times below
          `kAwayFallDb`; away rows -- their fall is the flat row's liveness
          control, since a dead rig reads no fall, and 4.0 is the "several
          dB" the spec Check names; broadband -- the row prints the figure
          the documents quote (full-scale LCG, seed 20260911u, composite
          tap); the amplitude is part of the quantity, because the source
          exceeds the output limiter's threshold;
        - rename "post peakLimiter" in the header comment and the printed
          label;
        - if the flat-row spread reaches 0.01, or either away fall is at or
          below 4.0, stop and report.

      The report gives each touched test's printed figures before and after
      the move.
      Check: the DSP parity binary passes in full, and each test whose
      assertions changed fails against the break its entry names.
- [x] 8.5 `gate_period_tracks_tempo_change`'s lower bound becomes
      `baseTransitions * 1.3`. The test's patch and the upper bound
      `< baseTransitions * 2.0` are unchanged. With the limiter after the
      blend the test's audio proxy ratio is 1.486 (proposal,
      "`gate_period_tracks_tempo_change`'s audio proxy"); the harness
      recorded the moved copy with the 1.3 bound passing 56 of 56
      (`bin/rrepin`), and with `kDoubledTempoBpm` set equal to
      `kBaseTempoBpm` and Part 1's period assertion skipped it printed
      base=70, doubled=53 (`bin/rbreak`).

      Keep the two headroom bullets (the `kBaseTempoBpm`/`kDoubledTempoBpm`
      timing). Replace the header comment's text from "Both tempos land the
      SAME…" through "…whatever the code happens to do." with text that:
      1. carries no edge counts and no ratio figures, and states the window
         (128 blocks per tempo) and the tap (channel 0, threshold 1e-3);
      2. says both tempos complete attack, decay, hold and release inside
         each half;
      3. says `CountRisingEdges` counts every rise of channel 0's magnitude
         above 1e-3, so a window's count is its gate cycles times the rises
         per cycle, including the oscillator's dips while the gate is open,
         and that the ratio is about twice the second window's rises per
         cycle over the first's, depending on the level and waveform the
         chain renders, including the Filter page's limiter, and not on the
         tempo alone;
      4. says the second window counts fewer rises per cycle than the
         first, even at an unchanged tempo;
      5. says what each bound catches: the lower bound fails when the count
         does not grow with the tempo, and the upper bound fails when the
         second window counts more rises per cycle than the first.
      Keep the 12000 BPM reason for the tempo choice, in present tense.

      Check:
      - the test passes, and the report prints its counts through a
        temporary print that is removed afterward;
      - with `kDoubledTempoBpm` set equal to `kBaseTempoBpm` and Part 1's
        period assertion skipped, the test fails at the 1.3 bound, and the
        report prints that run's counts;
      - stop and report if that run's second window does not count fewer
        rises per cycle than its first, or if the passing run's ratio is at
        or below 1.3. Do not choose another bound or change the patch.
- [x] 8.6 The comment above `kMaxUnitStateMagnitude` in
      `app/FroggersAppCore.hpp` states what bounds the state of the units the
      recovery watches, and every figure in it comes from a run.
      - The filter chain's input is the Drive page's output,
        `DriveBlendPhase::Process`, which ends in
        `outputLimiter.Process(blended)` at `kStageCeiling`.
        `PadeSaturator::Saturate` only clamps the comb saturator's output.
        The comment names the Drive output limiter as the input's bound.
      - The derivation states:
        1. the Drive output limiter bounds the chain's input near
           `kStageCeiling`; a run at 48 kHz on `DriveBlendPhase` with a
           large blend prints the maximum;
        2. the comb's fed-back term is at most |fb|/combDrive, which is 3.8
           at the bottom of Comb drive;
        3. at Topology 1 the peak's input is the trimmed comb branch, and
           Peak gain redrawn every sample drives the peak past its
           steady-state gain.
      - One printed run measures the watched units' state:
        - `FilterFxChain` at 48 kHz: Comb feedback knob 1, Comb drive knob
          0, Topology 1; Peak freq and Comb delay at their defaults;
        - Peak gain at maximum, then redrawn every sample;
        - a full-scale sine at the comb's pitch;
        - it prints the maximum of `StateMagnitude()` for the comb, the peak
          and the scoop notch.
        The run is a program outside the tree, or a print removed
        afterwards; its command and output go in the report.
      - The comment states that maximum and that 100 sits at least 10 times
        above it. If the maximum times 10 exceeds 100, stop and report; do
        not retune.
      - Drop the dated "Re-derived" line from the comment. Bring
        `RecoverIfNonFinite`'s "(input bounded to ~10-30 by construction)"
        into line with the corrected derivation.
      Check: every figure in the comment matches the runs' printed output;
      `make check-citations-resolve check-no-planning-history` passes in
      `app/`.
- [x] 8.7 Documentation, in the change's documentation step.
      - `MANUAL.md` Filter bank intro: a limiter after the Comb/Peak blend
        is part of the signal path. The intro already says the Drive page
        feeds the bank and the bank hands off to Delay and then Reverb.
      - `MANUAL.md` Comb drive: replace "The saturator's own ceiling on the
        comb's output level holds regardless", which is false below the
        centre (measured in the proposal), with what the control does below
        the centre, and state that the page's output limiter holds the
        level.
      - `MANUAL.md` and `QUICK_DICT.md` Peak gain: the figures become those
        `filter_bank_peak_gain_travel_measurement_at_and_away_from_resonance`
        prints after the move. A copy of this build with the limiter moved
        printed 5.27 dB at 1 kHz, 5.75 dB at 5 kHz and 6.72 dB broadband.
        On the base both documents say 7.10 dB broadband while the test
        prints 7.12416.

      Check: each statement about the Filter page's signal path, Comb drive
      and Peak gain is true of the built app, and every figure matches the
      test's printed output. `make check-docs-match-parameter-table` passes.
- [x] 8.8 `MANUAL.md`, MIDI Fighter Twister subsection: "A Twister row that
      shows no Restore predates the preset" is false for a row released in
      an earlier version. At Sheaf `f73d4202`, which frogg3rs `b0c03a9`
      pinned, that version's Controllers page offered a Release button on an
      active row; pressing it set the row's disposition to Blacklisted.
      Neither that button nor the code that set it is in the current tree,
      but `ControllersPageUI.hpp`'s row rendering still checks the
      disposition and shows only Delete for a row carrying it -- which is
      what this build does when it loads a row a player released back then.
      Make the sentence true for both kinds of row: one that predates the
      preset, and one an earlier version's player released with that
      Release button. The advice to delete the row and add the MIDI Fighter
      Twister preset again stands.

      `External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp`:
      the `Require` message in `TestRestoreResolvesAnAppPreset` that says
      "refused entirely with the old CommitLifecycleAction, whose throwaway
      view model could never resolve …" describes the code by its history.
      Say what is required: Restore reinstalls the app preset's own config
      onto a row diverged from it, resolving the row's app-specific wizard
      id.

      Check: the sentence in `MANUAL.md` is true of the built app for both
      kinds of row; `git grep -n -i "old CommitLifecycleAction" -- External/Sheaf`
      prints nothing.

## 9. Postflight, suites, screenshots and commit

- [x] 9.0 One postflight over the whole change, Sheaf and frogg3rs
      together, after 8.1 to 8.6 land, by contexts that wrote none of it. It
      may be split by story, and every reviewer reads both trees. It is
      driven by the four stories: it compares the landed diff with this
      proposal and, per story, builds and runs rather than reads, reverting
      each production change and watching that story's test go red, and
      trying plausible wrong implementations against the story tests. A
      finding blocks only if a story fails for a player, the change fails to
      build, run or ship, or text is left stating something false. It adds
      no tests or gates about process, and does not re-audit its own
      repairs unless a repair touches a story's code path. Nothing is
      committed until it passes.
- [x] 9.1 After the last edit to either tree, run both full suites on the
      exact tree to be committed: `make test` in `app/` and then every app
      test binary by path; the plugin target and its test binaries
      (`FroggersVstSmokeTest`, `FroggersVstHostTests`, `FroggersVstEditorTest`);
      `./app/build-launcher.sh`, since both it and the plugin compile
      `FilterFxChain`; Sheaf's full `projects/synth` suite by binary; and the
      miniapp runtime target.
      Check: per-binary pass and fail counts as measured, from a run started
      after the final edit; `Frogg3rs.app`'s `CFBundleExecutable` names the
      binary in `Contents/MacOS/`. The only failures allowed are Sheaf's two
      96 kHz deadline tests known to fail on this Mac
      (`braid4_meets_96000hz_256_frame_deadline_and_continuity` and
      `braid4_sparse_modulation_meets_96000hz_256_frame_deadline`), named as
      such, and `state_information_save_and_restore_never_write_the_shared_data_root`
      only if 7.4 recorded it as a finding; every other failure is fixed in
      this change.
- [x] 9.2 Retake the Delivery Gate screenshots from the app built in 9.1, for
      the operator.
      Check: one screenshot per state the proposal's Delivery Gate lists.
- [x] 9.3 Make a new commit in each tree: the Sheaf work on
      `shifted-encoder-turns`, and the frogg3rs work on
      `frogg3rs-transport-and-shift-ux` with `External/Sheaf` pinned at the
      Sheaf commit. Amend neither tree's history -- the frogg3rs commit
      before the move, and the Sheaf commit it pins, stay reachable. Nothing
      is pushed.
      Check: `git status` is clean in both trees, with no scratch edits left
      from reverted-change controls; `git ls-tree HEAD External/Sheaf` names
      the commit `git -C External/Sheaf rev-parse HEAD` prints.

## 10. Delivery

- [ ] 10.1 Deliver in the proposal's order, after the operator approves the
      9.2 screenshots:
      - fetch `shifted-encoder-turns` from this worktree's `External/Sheaf`
        into the main checkout's `External/Sheaf` and push it to the fork; if
        the fork's stack tip is no longer the Sheaf base 7.3 rebased onto,
        rebase onto the new tip and redo 9.1 and 9.2 before this push;
      - open the next pull request against jvictor0/Sheaf `main`, after the
        open ones, with step-by-step testing instructions for Shift +
        Crunchy on a Twister, including Restore on a row made before this
        version;
      - make the "Record the delivery" commit in this worktree's
        `External/Sheaf`, fetch and push it the same way, and pin frogg3rs at
        that commit once `fork/shifted-encoder-turns` in the main checkout's
        `External/Sheaf` equals it;
      - archive this change only; `shifted-encoder-turns` stays active in
        Sheaf. Confirm the archive from its printed output and the directory
        move, not the exit code, since `openspec archive` exits 0 when it
        aborts;
      - if frogg3rs `origin/main` has moved past the base 7.3 rebased onto,
        rebase this worktree's branch onto it, pin the Sheaf tip delivered
        above, redo 9.1, and retake the 9.2 screenshots for the operator
        before pushing;
      - commit, and fast-forward push frogg3rs `main` from this worktree's
        branch.
      Check: `git status` is clean in both trees, with no scratch edits left
      from reverted-change controls; the frogg3rs pin equals
      `fork/shifted-encoder-turns` in the main checkout's `External/Sheaf`.
