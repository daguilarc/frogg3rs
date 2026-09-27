The executor's deliverable is a report; code changes are a side effect of it.
A conflict between this file and the code, the story, the proposal or another
document is reported and stops that task. Each task names the break that turns
its check red; the executor states that break in the report, beside the check
it breaks, and a delivery verifier that wrote none of the code runs every
named break and the full gate once before the push (task 4.1). A test that is
red before the task starts is reported, never edited; a test that asserts
behaviour this change reverses is named in the report with the task that
reverses it, and changed only by that task. Build and run one binary at a
time, background included; C++ and JUCE builds run at `-j2` under `nice`.
Nothing is installed. Sheaf tasks carry the prefix S and are done in
`External/Sheaf` on the branch `app-plugin-controllers` (task 4.1). Sheaf test
paths are relative to `External/Sheaf/projects/synth`. `projects/synth test`
does not build the runtime shell, so every S task that touches `runtime/` or
`juce/` also builds and runs `make -C apps/miniapp test`. The plugin's
policies (ownership, size, state keys, cadence, notification, test devices)
are in the proposal's Policies section and bind every task below.

## 1. Sheaf

- [x] S1 A host that names no configuration file opens no startup patch.
      Behaviour: `Engine::Initialize` runs its startup-patch step (the block
      that reads `lastPatchVersionRecord_`) only when
      `dataPaths_.configFile` is non-empty. `Engine::LoadRuntimeConfiguration`
      is left as it is: with an empty path it finds no file and changes
      nothing. Writing is already refused by the empty-path return in
      `Engine::SaveRuntimeConfiguration`; its comment, which names only a
      bare test engine, and the Initialize binding-order comment's step 8
      are rewritten to say a host that keeps no configuration file opens no
      startup patch and writes none. Story: PLG-24, PLG-22. Check: NEW
      `engine_initialize_without_a_configuration_file_opens_no_startup_patch`
      in `tests/engine_tests.cpp`: with `MidiCatalogTestApp` and
      `patchCarriesMappings` true, a patches root holding one saved patch
      whose probe parameter is off its default and whose `midiInstrument`
      section holds a controller, and `configFile` cleared; after
      `Initialize` and one `MessageThreadTick`, the probe equals its default
      and `LiveInstrument` equals `DefaultInstrument`. Break: remove the
      guard; the probe assertion goes red. `engine_startup_loads_lexicographically_latest_patch`
      and `engine_initialize_treats_missing_runtime_config_as_defaults_and_still_loads_startup_patch`
      stay green. Spec: `sar-42`.
- [x] S2 `PatchManager` is the only serialize requester. Behaviour:
      `PatchManager` gains NEW `PatchManager::RequestHostSnapshot(std::string
      patchName)` and NEW `PatchManager::SetHostSnapshotConsumer`, whose
      consumer takes the `JsonDocument` by reference and is called on the
      thread that calls `ProcessResponses`. A snapshot request pushes
      `PatchMessageIn::SerializeToJSON` with an id from `nextRequestId_` and
      answers Pending; it answers Busy and pushes nothing while a snapshot,
      a save or a held save is outstanding, and QueueFull when the push
      fails. `SavePatch`, `SavePatchAs` and `SavePatchAsOverwrite`, asked
      while only a snapshot is outstanding, run their own checks (a missing
      current patch, an existing or missing directory) first, then store the
      request in one held slot and answer Pending; a second save while one
      is outstanding or held answers Busy with the first one's id and path,
      as today. `ProcessResponses` pops while a snapshot or a save is
      outstanding; a `SerializedJSON` whose id is the snapshot's is handed to
      the consumer, the snapshot is cleared, and then, after the consumer
      returns, the held save is dispatched through
      `PatchManager::DispatchSerialize`; the call answers NoCompletion for
      a snapshot. A response matching neither is skipped, as today.
      `NewPatch` and `PatchManager::LoadPatchVersion` clear the held slot as
      they clear `pendingSave_`; neither clears an outstanding snapshot. The
      `PatchSerializationContext::arena` comment names the snapshot as a
      request that flows through `PatchManager`. Story: PLG-10, FILE-06.
      Check: new cases in `tests/engine_tests.cpp`, driven through an `Engine`
      with a scratch data root:
      NEW `host_snapshot_holds_a_save_requested_while_it_is_outstanding` (a
      snapshot is requested, then `SavePatch` with a current patch answers
      Pending, not Busy; after `ProcessBlock`, `MessageThreadTick`,
      `ProcessBlock`, `MessageThreadTick`, the consumer ran once and
      `ConsumeLastTickPatchResult` reports Written with a new version file in
      the current patch directory);
      NEW `host_snapshot_reaches_the_consumer_and_writes_no_file` (the consumer's
      document holds the engine's live parameter values, and the patches
      root's file list is unchanged); and
      NEW `host_snapshot_is_refused_while_a_save_is_outstanding` (with a save
      outstanding, `RequestHostSnapshot` answers Busy and
      `PatchMessageInBus::Size` does not move). Break: answer Busy from a save
      while a snapshot is outstanding; the first case goes red. Spec: `spp-14`.
- [x] S3 A host names the current patch without loading it. Behaviour: NEW
      `Engine::NameCurrentPatch`, taking an optional path relative to the
      patches root, returns whether a patch is now current, and NEW
      `Engine::CurrentPatchRelativePath` returns the current patch
      directory relative to `DataPaths().patchesRoot`, with `/` separators.
      Naming resolves `relative` with `PatchBrowser::ResolveLoadPath` over a
      `PatchBrowser` rooted at `DataPaths().patchesRoot` (which refuses an
      absolute path, a `..` component, a missing directory, a regular file
      and a symbolic link that resolves outside the root), then refuses a
      result whose `weakly_canonical` form equals the root's, then removes a
      trailing separator, and records the directory through NEW
      `PatchManager::SetCurrentPatchDirectory`, which pushes no message and
      clears no outstanding request. An empty optional or any refusal leaves no
      current patch. A nested directory inside the root is accepted, since a
      Load can open one. Save does not re-check the directory; it writes
      into it as it writes into any current patch. Story: PLG-10, FILE-06,
      FILE-11. Check: new cases in `tests/engine_tests.cpp`:
      NEW `naming_an_existing_patch_makes_it_current_without_a_message` (naming
      `p` and `p/` leaves `PatchMessageInBus::Size` at 0, reads back `p`,
      and a Save writes its version into the root's `p`); and
      NEW `naming_a_patch_outside_the_root_leaves_no_current_patch` (a missing
      name, a regular file, `.`, the empty path, `../x`, an absolute path,
      and a symbolic link inside the root to a directory outside it each
      leave no current patch, and Save answers NeedsSaveAsPath). Break:
      record `relative` without resolving it; the outside-root case goes red.
      Spec: `spp-15`.
- [x] S4 `MidiConnectionManager` takes an injectable device access. Behaviour:
      NEW `synth_runtime::MidiDeviceAccess` in
      `runtime/MidiConnectionManager.hpp` holds an `enumerate` function, an
      input endpoint factory and an output endpoint factory (each taking the
      `RuntimeMidiEpoch`), and a poll interval; its member defaults are
      `detail::EnumerateDevices`, factories that build
      `synth_juce::MidiInHandler` and `synth_juce::MidiOutputHandler`, and
      five seconds, so `Runtime<App>` passes nothing. Two NEW abstract
      classes, NEW `synth_runtime::MidiInputEndpoint` (`Open`, `Close`, `SetProcessor`) and
      NEW `synth_runtime::MidiOutputEndpoint` (`Open`, `Close`, and
      `synth::IMidiOutputSink`) are what the manager holds;
      `synth_juce::MidiInHandler` and `synth_juce::MidiOutputHandler` derive
      from them. The constructor takes the access as a third argument with
      that default. `ResizeToControllerCount` builds endpoints through the
      factories, `Reconcile`'s callers and
      `MidiConnectionManager::EnumerateNow` enumerate through `enumerate`,
      `MidiConnectionManager::StartupReconcile` starts the poller with the
      access's interval, and the poller still gets
      `detail::ForceDirtyEnumerate`. The class comment's "exactly one owner
      per physical device" sentence becomes one owner per controller slot of
      this manager, since two managers in one process can each open a device.
      Story: none directly; it is the seam every plugin controller check
      needs, and the Sheaf reviewer's check that the default path is unchanged.
      Check: every JUCE-linked test `make -C apps/miniapp test` runs stays
      green; NEW `CheckConnectionManagerOpensTheInjectedEndpoints` in NEW
      `juce/MidiConnectionManagerTests.cpp`, added to `apps/miniapp`'s
      `test` target: a MiniApp engine with one controller whose refs name a
      fake pair, a manager given a fake access listing that pair, and
      `StartupReconcile`: the fake input and output each report one `Open`
      with the pair's identifiers, and the engine's `MidiSender` sink 0 is the
      fake output; with the pair removed from the list and the poller's
      change consumed, `MidiConnectionManager::State` marks both offline.
      Break: build the JUCE handlers in `ResizeToControllerCount` regardless
      of the access; the `Open` assertions go red.
- [x] S5 A host declares which runtime pages its sidebar offers. Behaviour:
      NEW `synth::runtime_ui::RuntimeSidebarPages` in `RuntimePages.hpp`
      (audio, controllers, sync, file and loadReadout, each true by default);
      `SidebarSnapshot` carries it; `BuildSidebarTree` emits only declared
      entries, in today's order; `Layout::SidebarRootBounds` takes the number
      of rows shown; `RuntimeMainComponent`'s constructor takes it as a third
      argument defaulting to all pages, passes it to its sidebar, and
      `RuntimeMainComponent::HandleSidebarAction` ignores an action for an
      undeclared page. The comments on `SidebarRootBounds` and
      `RuntimeMainComponent::RequireCompositionHolds` that say the sidebar is
      five fixed rows are rewritten to say it is as tall as its rows. Story:
      PLG-04. Check: NEW `TestSidebarShowsOnlyTheDeclaredPages` in
      `tests/runtime_main_component_tests.cpp`: with Controllers and File
      declared, the tree holds `kSidebarControllers` and `kSidebarFile` and
      no `kSidebarAudio`, `kSidebarSync` or `kSidebarDeadline`, the sidebar
      root is 80 tall, each declared entry opens its page and Back returns,
      and dispatching `kSidebarAudio` and `kSidebarSync` leaves the page at
      Application. `TestSidebarOpensEachPageAndBackRestoresApp`,
      `TestRefreshUpdatesRuntimePageModelsAndRollingDeadline` and
      `TestRejectsASurfaceTooShortForTheRuntimeSidebar` stay green. Break:
      ignore the declaration in `BuildSidebarTree`; the absence assertion goes
      red; ignore it in `HandleSidebarAction`; the routing assertion goes red.
      Spec: `sru-69`.
- [x] S6 One construct per duplicated host binding. Behaviour, each built
      once and called by `JuceRuntimeMainServices`,
      `BrowserRuntimeMainServices` and, in task 2.3, the plugin services:
      (a) NEW `synth::runtime_ui::ControllersPageBinding<EngineType>` in NEW
      `include/synth/ControllersPageBinding.hpp` owns the
      `ControllerWizardDiscoveryCache` and the controllers-dirty and
      instrument-dirty flags; its `MakeCallbacks(onBack, connectionState,
      saveRuntimeConfiguration)` fills every other field of
      `ControllersPageCallbacks` from the engine (the instrument snapshot,
      the commit through `Engine::EditInstrument` with its cache update and
      flags, `MakeUISystemMessageChoices`, `MakeAnalogAppActionChoices`,
      `MakeControllerWizardRegistry`, `GestureCount`, and a `setStatus` that
      does nothing); `MarkInstrumentRebuilt()` sets both flags;
      `UpdateDeviceList` and `HasDeviceList` feed the cache; and
      `Refresh(ControllersPageSurface&)` does the rest of today's
      `RefreshControllers` body. (b) NEW
      `synth_runtime::FeedControllersDeviceList(binding, manager)` in
      `runtime/MidiConnectionManager.hpp` seeds the cache from
      `DeviceListSnapshot` when it has none and then applies
      `ConsumeDeviceListChange`, for the two manager-backed hosts; the
      browser keeps its revision check. (c) NEW
      `synth::runtime_ui::MakeEngineFileCallbacks(engine, onCommand)` in
      `RuntimeFileService.hpp` binds `RuntimeFileCallbacks` to
      `Engine::NewPatch`, `Engine::LoadPatch`, `Engine::Patches` and
      `DataPaths().patchesRoot`, and hands each command's name and
      `PatchCommandResult` to `onCommand` when one is given; the JUCE services
      pass one that logs as `Runtime<App>`'s own patch commands do. Before
      writing, grep `MakeUISystemMessageChoices`, `MakeControllerWizardRegistry`,
      `GestureCount`, `wizardDiscoveryCache_`, `RuntimeFileCallbacks` and
      `callbacks.savePatch`, case-insensitively, over
      `External/Sheaf/projects/synth` and `app/`, and report found against
      changed. Story: none directly; serves the reviewer and the user when
      one host's copy would drift (a Gestures row offering a gesture the app
      lacks in one host only). Check: the runtime, Controllers page, file
      service and browser contract test binaries in the Sheaf gate and
      `make -C apps/miniapp test` stay green, and task 2.3's host tests read
      the plugin's choices through the binding. Break: none of its own; a
      refactor's check is the unchanged suites.

## 2. frogg3rs

- [x] 2.1 The plugin keeps no runtime configuration. Behaviour: the
      anonymous-namespace `ProductionDataPaths` becomes NEW public static
      `FroggersPluginProcessor::PluginDataPaths(const std::filesystem::path&
      dataRoot)`, returning `SheafPatchDataPathsForApp(dataRoot,
      FroggersManifest().appId)` with `configFile` cleared; the production
      constructor passes it `SheafUserApplicationDataRoot()`. Story: PLG-24.
      Check: NEW `new_instance_reads_and_writes_none_of_the_standalones_data`
      in `app/vst/FroggersVstHostTests.cpp`: a scratch data root laid out as
      `PluginDataPaths` composes it, holding under the patches root one saved
      patch (Audio page slot 0 at 0.9888, one Twister row) and, at the path
      `SheafPatchDataPathsForApp` gives `configFile`, a configuration that
      records that version and holds the Twister row; a processor built from
      `PluginDataPaths(scratch)` reads 0.3087 for that host parameter,
      `MidiControllerCount` 0 and no current patch after one pump; the
      configuration's bytes and the root's file list are unchanged. Break:
      keep `configFile` in `PluginDataPaths`; the knob and row assertions go
      red.
- [x] 2.2 The processor opens each row's ports. Behaviour: the processor owns
      NEW `std::unique_ptr<synth_runtime::MidiConnectionManager<FroggersApp>>
      midiConnections_`, declared after `engine_`, constructed with the engine,
      a `RuntimeMidiEpoch::Capture(startTime_)` and a `MidiDeviceAccess`. The
      constructor sets the engine's will-rebuild callback to
      `OnMidiProcessorsWillRebuild` and its rebuilt callback to
      `OnInstrumentRebuilt` followed by the rebuilt hook, before
      `engine_.Initialize`, as `Runtime<App>`'s constructor does; after
      `Initialize` it starts the engine's `MidiSender` and calls
      `StartupReconcile`. `timerCallback` calls `OnTimerTick` right after
      `engine_.MessageThreadTick`. `~FroggersPluginProcessor` stops the timer,
      stops the `MidiSender`, then resets `midiConnections_`, all before
      `engine_` is destroyed. NEW `GetEngine`, `MidiConnections` and
      `SetMidiProcessorsRebuiltHook` serve the plugin services. The test
      constructor becomes `FroggersPluginProcessor(synth::RuntimeDataPaths,
      synth_runtime::MidiDeviceAccess)`, its access defaulting to one that
      lists no devices and whose endpoints never open. The host tests gain a
      fake device access whose output endpoint records every sent message,
      each `Open` and `Close`, and at `Close` whether
      `ContextForTest().midiSender->IsRunning()`, and whose input endpoint
      lets a test deliver a `synth::BasicMidi` to the processor it was given.
      `processBlock` still ignores its MIDI buffer. Story: PLG-16, PLG-17,
      PLG-19, PLG-20, PLG-23, PLG-25. Check: new cases in
      `app/vst/FroggersVstHostTests.cpp`, each with a Twister row committed
      through `Engine::EditInstrument` with refs naming a fake Twister pair:
      NEW `twister_row_turn_moves_the_shown_pages_slot_zero` (channel 1 CC 0
      value 65 delivered on the input raises Audio slot 0 after the next pump
      and block; after selecting the Envelope page the same turn moves
      Envelope slot 0 and not Audio slot 0, and that host parameter reads it
      back); NEW `twister_side_button_moves_to_the_next_page` (channel 4 CC 8
      value 127 then 0 moves the visible page from Audio to Envelope);
      NEW `twister_row_feedback_reaches_its_output_port` (after the turn, the fake
      output recorded channel 1 CC 0 with the new ring value);
      NEW `replugged_twister_reconnects_and_resends_feedback` (with the access's
      poll interval at 1 ms, removing the pair and pumping until the poller's
      change is consumed, at most 100 pumps 10 ms apart, marks both ports
      offline; restoring it reopens both and the output records the full
      ring set again); NEW `connect_messages_go_to_each_rows_output_when_it_opens`
      (an APC40 mkII (Ableton) row's output records `F0 47 7F 29 60 00 04 41
      09 07 01 F7` and a Launchpad X row's output records its programmer-mode
      message, each first); NEW `two_rows_each_read_only_their_own_port` (with a
      Twister row and an APC40 mkII (Generic) row on separate fake pairs,
      channel 1 CC 14 on the Twister input moves the shown page's Crispy and
      leaves BPM as it was); and
      NEW `destroying_the_processor_stops_the_sender_before_closing_outputs`
      (with feedback queued, destroying the processor records `Close` on the
      fake output with `IsRunning` false, and no send after it). Breaks: skip
      `StartupReconcile` (every case goes red); skip `OnTimerTick` (the
      replug case goes red); drop the explicit sender stop from the
      destructor (the teardown case goes red).
- [x] 2.3 The editor shows Controllers and File. Behaviour: NEW
      `frogg3rs_vst::FroggersPluginServices` in NEW
      `app/vst/FroggersPluginServices.hpp` satisfies
      `synth::runtime_ui::RuntimeMainServices`: Controllers through a
      `ControllersPageBinding` (S6) with `connectionState` from
      `MidiConnections().State()`, `RefreshControllers` through
      `FeedControllersDeviceList` and the binding, and a
      `saveRuntimeConfiguration` that calls the processor's notification
      (task 2.6) and answers true; File through `MakeEngineFileCallbacks`
      (S6) with an `onCommand` that notifies per the proposal's policy; Audio,
      Sync, deadline and `SaveRuntimeConfiguration` as no-ops (the deadline
      reads 0). It sets and clears the processor's rebuilt hook to
      `MarkInstrumentRebuilt`, as `JuceRuntimeMainServices` does with
      `Runtime<App>`. `FroggersPluginEditor` owns the services, a
      `RuntimeMainComponent<FroggersApp, FroggersPluginServices>` built with
      Controllers and File declared (S5), and a `PortableComponent` over that
      component; its design size is `IntrinsicBounds`; its repaint hook calls
      `Refresh` and then refreshes the renderer; it keeps its action handler
      on the app surface and sets the same deferred refresh on the main
      component; the ? button is placed per the proposal's policy. Story:
      PLG-04, PLG-16, PLG-22. Check: NEW
      `editor_sidebar_holds_controllers_and_file_only` in
      `app/vst/FroggersVstEditorTest.cpp` (the renderer holds
      `kSidebarControllers` and `kSidebarFile` and no `kSidebarAudio`,
      `kSidebarSync` or `kSidebarDeadline`; the app root is at 0,0 at 900 by
      712; the editor's design width is 996; the ? button's bounds intersect
      no sidebar node's bounds mapped through the renderer's transform, at the
      default size and at 1200 by 900; opening Controllers and pressing Back
      leaves the visible page and drill level as they were). New cases in
      `app/vst/FroggersVstHostTests.cpp`, driving a `RuntimeMainComponent`
      over `FroggersPluginServices` with no editor:
      NEW `controllers_page_add_twister_binds_the_fake_device` (Add with a fake
      Twister pair listed installs a row bound to both fake ports);
      NEW `file_page_save_as_writes_a_version_under_the_patches_root` (Save As
      "p" writes one version under `patches/p` and the File page's name reads
      "p"); NEW `file_page_load_applies_sound_and_rows_and_writes_no_configuration`
      (loading a patch holding an APC40 mkII row and Audio slot 0 at 0.9
      gives this instance that row and that value, and the data root gains no
      file outside `patches/`); and NEW `file_page_new_returns_every_parameter_to_default`
      (FILE-12). `dispatching_an_action_refreshes_the_renderer_through_the_action_handler_with_no_timer_tick`
      stays green. Breaks: construct the component without the page
      declaration (the editor case goes red); leave the ? button at the
      window's top right (its overlap assertion goes red); unbind `savePatchAs`
      (the Save As case goes red).
- [x] 2.4 A controller cannot run the plugin's transport. Behaviour:
      `FroggersUiSurface::StartTransport` becomes public and NEW
      `FroggersUiSurface::StopTransport` holds the `kStop` branch's call;
      `FroggersUiSurface::HandleAction`'s `kPlay`, `kStop` and `kRecord`
      branches return without acting when `pluginHostMode_` is set;
      `FroggersPluginProcessor::timerCallback`'s host edge and
      `TestStartTransport`/`TestStopTransport` call the public start and stop.
      `SetPluginHostMode`'s comment and the `kPlay` branch's comment say that
      in plugin mode only the host starts and stops the transport. Story:
      PLG-26, PLG-05. Check: NEW
      `controller_play_stop_record_do_nothing_and_freeze_latches` in
      `app/vst/FroggersVstHostTests.cpp`: with a Twister row and the fake
      playhead stopped, channel 4 CC 9 value 127 leaves the transport stopped;
      a playhead start edge then runs it; with Shift (channel 4 CC 13 value
      127) held, channel 4 CC 9 leaves it running; with an APC40 mkII
      (Generic) row and the playhead running, note 93 leaves `RecordArmed`
      false; channel 4 CC 10 on the Twister latches Freeze.
      `transport_edges_run_stop_run_produce_exactly_one_message_per_transition`,
      `freeze_via_production_seam_holds_audio_and_reads_stopped_like_t7_3a`
      and `FroggersVstSmokeTest` stay green. Break: remove the `kRecord` gate
      (the Record assertion goes red, because the playhead is running); remove
      the `kPlay` gate (the first assertion goes red).
- [x] 2.5 The DAW project restores the current patch, and the snapshot comes
      through `PatchManager`. Behaviour: `PumpStatePersistence` no longer
      pushes `SerializeToJSON`; `pendingStateSnapshotRequestId_` and
      `nextStateRequestId_` are removed. The constructor installs a
      `PatchManager::SetHostSnapshotConsumer` consumer that calls NEW
      `FroggersPluginProcessor::AttachSessionExtras(synth::JsonArena&,
      synth::JSON&)` on the document, dumps it, and stores the text under
      `stateBlockMutex_`. `AttachSessionExtras` writes `freezeLatched`,
      `visibleBankIndex`, `inputSelection`, `controllerRows` (always the
      boolean true) and, from `Engine::CurrentPatchRelativePath`,
      `currentPatch`; the constructor's
      seed builds its document with `BuildPatchJSON(..., InstrumentSnapshot(),
      {}, patchCarriesMappings)` and the same function. The timer calls
      `RequestHostSnapshot` per the proposal's cadence. A restore calls
      `Engine::NameCurrentPatch` with the stored `currentPatch` string, or
      an empty optional when it is missing or not a string, in the same pump that
      pushes the restore. A restore whose `sessionExtras.controllerRows` is not
      the boolean true pushes, in place of the parsed document, NEW
      `ParametersOnlyRestoreDocument`'s result: a new root in the same arena
      holding the parsed root's `schema`, `patchName` and `parameterValues`
      under `schemaVersion` 1, which `LoadPatchJSON` loads without reading any
      `midiInstrument` section; in the same pump it sets the live instrument to
      `Engine::DefaultInstrument` through `Engine::EditInstrument`. The Freeze,
      page, IN: and current-patch keys restore as for marked state. The PumpStatePersistence and `pendingRestoreJsonText_`
      comments are rewritten: the message thread is the patch input bus's only
      producer (the restore push and `PatchManager`'s commands both run there),
      and `PatchManager` is the only serialize requester. Story: PLG-10,
      PLG-24, FILE-06. Check: new cases in `app/vst/FroggersVstHostTests.cpp`:
      NEW `project_restores_controller_rows_and_the_current_patch` (instance A
      adds a Twister row on a fake pair, Save As "p", moves a host parameter;
      its state restored into instance B gives B the row with its fake output
      opened, `CurrentPatchRelativePath` "p", a Save writing a second version
      into `patches/p`, and the parameter; state whose `sessionExtras` has no
      `currentPatch` restores with no current patch); and
      NEW `save_during_a_snapshot_writes_a_version_and_refreshes_the_state` (with
      a snapshot outstanding, Save on "p" writes one version, and after the
      next snapshot the state text holds the value set just before Save); and
      NEW `unmarked_state_restores_sound_and_input_and_no_controller_rows` (a
      document laid out as an earlier plugin wrote it, built in the test with
      `BuildPatchJSON` carrying a Twister row on a fake pair and a
      `sessionExtras` holding `freezeLatched`, `visibleBankIndex` and
      `inputSelection` but no `controllerRows`, restored into an instance whose
      input bus is enabled in stereo and which holds an APC40 mkII (Generic)
      row: after the pumps, the host parameter the document set, the page and
      the input selection are the document's, `MidiControllerCount` is 0, and
      the fake Twister endpoints record no `Open`; the same document with
      `controllerRows` true restores the Twister row, which is the case's
      control that the rows were there to apply). Every existing
      `state_information_*` case stays green. Breaks: drop the
      `currentPatch` key (the first case's current-patch assertion goes red);
      push the parsed document whatever the mark says (the unmarked case's
      row-count and `Open` assertions go red); skip the `EditInstrument` to the
      default instrument (the unmarked case's row count reads 1, the APC40 row,
      and goes red);
      keep the processor's own serialize push beside `PatchManager`'s (the
      second case goes red, as the two requesters take each other's
      responses).
- [x] 2.6 The host learns of a non-parameter change. Behaviour: NEW
      `FroggersPluginProcessor::NotifyHostOfNonParameterChange` calls
      `updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true))`;
      task 2.3's services call it per the proposal's notification policy.
      Story: PLG-27, PLG-10. Check: NEW
      `controller_commit_and_file_actions_mark_non_parameter_state_changed`
      in `app/vst/FroggersVstHostTests.cpp`: a `juce::AudioProcessorListener`
      records one `audioProcessorChanged` with `nonParameterStateChanged`
      after an Add, after New, after Save As and after Load, and none after a
      Save or a state restore. Break: remove the call from the
      `saveRuntimeConfiguration` callback; the Add assertion goes red.
- [x] 2.7 The operator's run in Ableton Live 12 Suite on macOS, with the
      final VST3 and AU builds and one MIDI Fighter Twister. Setup, as the
      manual writes it: open Settings (Cmd-comma), the Link, Tempo & MIDI tab;
      in MIDI Ports, set the Twister's input and output Track, Sync and
      Remote switches off; set up the Twister as TWI-01; insert Frogg3rs on a
      MIDI track. Steps, each recorded as passed or failed with what was
      seen, in the VST3 and then the AU: PLG-04 (Controllers and File, the ?
      button); PLG-24 (a new instance); PLG-16 and PLG-17 (Add the Twister,
      turn, page, Shift, Hold Drill); PLG-20 (rings and colours follow);
      PLG-25 (unplug and replug); PLG-26 (the Twister's Play and, under Shift,
      Stop do nothing with Live stopped and playing, and Freeze latches);
      PLG-22 (Save As, Save, Load, New on the File page, and the saved patch
      listed in the standalone's File page); PLG-10 (save the Live Set, close
      it, reopen it); PLG-27 (the save prompt). A failure is a finding against
      the task that owns the behaviour. APC40 and Launchpad steps are backed by
      the code tests of task 2.2 and 2.4 and are not run on hardware; Logic,
      Reaper, Bitwig and Windows are not run.

## 3. Documents

- [x] 3.1 `MANUAL.md`: the Plugin subsection says the plugin has Controllers
      and File pages and no Audio I/O, Sync or load readout; that each
      controller row opens its own ports as in the standalone; the Live setup
      of task 2.7 word for word, and that Live MIDI-learn on a port needs its
      Remote switch on, in which case a control both map moves both targets;
      that a controller's Play, Stop and Record do nothing in the plugin; that
      BPM follows the host (PLG-06); that the patches are the standalone's;
      that the DAW project keeps the controller setup and a new instance starts
      with none; to save the Live Set after changing the controller setup,
      because the AU never asks; and that the File page, not a host preset,
      carries a setup between instances. The sentences "it has no Audio I/O,
      Controllers, Sync or File sidebar page", "MIDI reaches this instrument
      entirely through host-parameter automation", "The plugin accepts the
      host's MIDI buffer but does not read it itself" and the Overview's "(the
      plugin takes MIDI through host automation, as the Plugin subsection
      says)" are rewritten, and the Saving subsection gains the plugin's
      project. A reader who wrote none of it checks the rewritten sections
      against the built plugin.
- [x] 3.2 `openspec/story.md`: after the operator ratifies the proposal's
      table, its steps replace PLG-04, PLG-10, PLG-16, PLG-17 and PLG-19 to
      PLG-22, PLG-18 gains its sentence, and PLG-23 to PLG-27 are added; the
      platform notes that say the plugin has no sidebar (SUR-01's Caps, the
      1.12 and 1.14 headers, LOAD-01's Caps), section 2.2 row 19, section 3.2
      rows X-04 and X-06, section 3.3's "The plugin has no sidebar", and
      section 4's P columns for controller rows, mapping rows, connect
      messages, patches, versions, the same-name suffix, MIDI output sinks and
      absolute-feedback routes are brought into line with them.
- [x] 3.3 Code comments this change makes false, each rewritten in the task
      that makes it false and listed in that task's report: the
      `FroggersPluginProcessor.hpp` header (the sentence that the class
      duplicates none of `Runtime.hpp`'s MIDI-connection machinery, the
      sentence that no plugin-side MIDI mapping exists, and the reference to
      `ProductionDataPaths`); the `TestStartTransport` comment in the header
      and the `.cpp` ("Not reachable from any host UI (no editor)", and routing
      through `DispatchAction`); the `timerCallback` host-edge comment; the
      Freeze inventory comment in `BuildHostParameterInventory` ("No
      plugin-side MIDI mapping/learn is introduced anywhere"); the
      `cachedStateJsonText_` comment; the `FroggersPluginEditor.hpp` header
      ("NO sidebar"); and the `app/vst/CMakeLists.txt` comment on
      `HostDataPaths.cpp` that points at the processor header's reasoning.
      The comments on `processBlock`'s MIDI buffer, `acceptsMidi` and
      `NEEDS_MIDI_INPUT` stay true and stay as they are. Check: a reader who
      wrote none of it reads each listed comment against the landed code.
- [x] 3.4 Every scenario in this change's two spec deltas names the check
      that backs it by test name, and each named test exists in the file named
      or is marked NEW in the task that writes it; before the archive, each
      "not yet delivered" line names a test that now exists and passes.

## 4. Delivery

- [x] 4.1 In this order: in `External/Sheaf`, make the branch
      `app-plugin-controllers` from the pinned commit, commit S1 to S6 and
      the Sheaf change's spec deltas, run the Sheaf gate and
      `make -C apps/miniapp test`; if a task's change sits inside a concept an
      open stacked Sheaf pull request introduced, report which before pushing,
      since that commit then belongs in that pull request's branch with the
      branches above it rebased and force-pushed; archive
      `app-plugin-controllers` with `openspec archive app-plugin-controllers`
      and commit; `git -C External/Sheaf push fork app-plugin-controllers`
      (remote `fork` = `git@github.com:daguilarc/Sheaf.git`, added with
      `git remote add` if `git remote get-url fork` fails); open the next
      sequential pull request with `gh pr create --repo jvictor0/Sheaf --base
      main --head daguilarc:app-plugin-controllers`, its body saying it stacks
      on #22; stage the pin at the pushed tip; the delivery verifier runs every
      break the executors named and the full frogg3rs gate (`make test`, then
      each `app/vst` test binary the CI workflows run, by path, after `make
      test` stops); archive this change with `openspec archive
      frogg3rs-plugin-controllers-and-patches` and commit; `git push origin
      HEAD:main`, never a pull request. A fix after an archive is a new commit,
      never a second archive.
