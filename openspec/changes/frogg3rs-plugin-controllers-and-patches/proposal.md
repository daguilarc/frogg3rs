# Proposal — `frogg3rs-plugin-controllers-and-patches`

Written against frogg3rs `main` at `d385c2e`, which pins Sheaf `1cec77a4` (the
tip of `fork/unbounded-patch-serialization-arena`, jvictor0/Sheaf#22). Its
library half is the Sheaf change `app-plugin-controllers`
(`External/Sheaf/openspec/changes/app-plugin-controllers/`). Every task for
both trees is in this change's `tasks.md`; Sheaf tasks carry the prefix S. The
ratified story is `openspec/story.md`.

## Who, and what they cannot do now

A user running Frogg3rs as a VST3 or AU inside a DAW:

- cannot use a MIDI Fighter Twister, an APC40 mkII or a Launchpad the way the
  standalone and browser builds let them (story 1.12 and 1.13: the
  Controllers page, presets, encoders that follow the page and drill level,
  Shift, Hold Drill, the Page, Back, Randomize, Reset and Scene buttons,
  ring and colour feedback). The editor (`FroggersPluginEditor`) renders the
  bare app surface with no sidebar, so there is no Controllers page, and
  `FroggersPluginProcessor::processBlock` ignores its MIDI buffer. The only
  route is DAW MIDI-learn onto the 92 host parameters (PLG-08, PLG-18), which
  binds one knob to one fixed parameter: no page following, no drill, no
  buttons, no feedback.
- cannot Save, Save As or Load a patch (PLG-22). The editor has no File page.

A newly inserted plugin instance also opens the standalone's controller setup
and sound. `ProductionDataPaths` (`app/vst/FroggersPluginProcessor.cpp`) hands
`Engine::SetRuntimeDataPaths` the standalone's own `config` file and
`patches/frogg3rs/` root, and `Engine::Initialize` loads that configuration
(`Engine::LoadRuntimeConfiguration`) and opens its recorded patch version, or
the newest patch when it records none. Measured: a processor constructed over
a scratch root holding a saved patch and a configuration with the knob at
0.9888 (default 0.3087) and one controller row opens with the knob at 0.9888
and one row; the configuration's bytes and the root's file list are unchanged;
over an empty root it opens at the default with no rows.

## Design

Each decision, with its reason.

1. **Each controller row opens its own ports, in the plugin as in the
   standalone.** The processor owns a
   `synth_runtime::MidiConnectionManager<FroggersApp>`, constructed as
   `Runtime<App>` constructs one (the engine and a
   `synth_juce::RuntimeMidiEpoch` captured from the processor's start time),
   wired to the engine's will-rebuild and rebuilt callbacks, reconciled once
   at construction and polled from the processor's message-thread timer.
   Rows keep their port identity, their MIDI in and MIDI out selectors and
   status dots, and reconnect, resync and connect messages as in the
   standalone. The host's MIDI buffer is not read for controllers: a VST3
   `Event` (`ivstevents.h` in the VST3 SDK JUCE carries) holds `busIndex`,
   `sampleOffset`, `ppqPosition` and `flags` and no source port, so a plugin reading its track cannot tell
   one controller, or a keyboard, from another. Opening the ports keeps the
   Controllers page and every controller step identical to the standalone.
   In Live the user switches the controller port's Track and Remote switches
   off (Settings, Link, Tempo & MIDI tab, MIDI Ports), so Live neither plays
   nor maps it as well. JUCE's CoreMIDI backend opens an input by connecting
   the source to a port of its own client (`MIDIPortConnectSource` in
   `juce_CoreMidi_mac.mm`), so the plugin, Live and the standalone can each
   open the same device; whether Live keeps the port usable to the plugin is
   part of the operator's run. Port exclusivity on Windows is not run.
2. **A controller cannot start or stop the plugin's transport, or arm a
   recording.** The host playhead reaches the transport only because
   `FroggersPluginProcessor::timerCallback` dispatches the `kPlay` and
   `kStop` actions; every controller button job arrives through the same
   `FroggersUiSurface::DispatchAction` (`Engine::MessageThreadTick`, the
   `AppAction` branch). `FroggersUiSurface::StartTransport` is already the
   one definition of starting (the `kPlay` branch and the Freeze release
   call it); stopping is the `kStop` branch's call to
   `FroggersUiSurface::LatchThenTransport`. The surface gets a public start
   and stop; the host edge and the smoke-test seam call them directly; and
   `FroggersUiSurface::HandleAction`'s `kPlay`, `kStop` and `kRecord`
   branches do nothing when plugin host mode is set. The plugin editor draws
   no Play, Stop or Record, so in a plugin those three actions come only from
   controller rows. Freeze is not gated: a mapped Freeze latches (PLG-08). The
   mapping stays on the row, so a patch keeps it for the standalone.
3. **Host parameters, DAW automation and DAW MIDI-learn stay** (PLG-08,
   PLG-09, PLG-18). A controller row's edit reaches the host as any core
   change does (`FroggersPluginProcessor::PumpHostParameterBridge`). In Live,
   MIDI-learn needs the port's Remote switch on; with that switch on and a
   row bound to the same port, a control both map moves both targets.
4. **The sidebar shows Controllers and File, and nothing else.** The DAW owns
   audio devices, tempo and transport (PLG-04, LOAD-01), so Audio I/O, Sync
   and the load readout stay out. The editor hosts Sheaf's
   `RuntimeMainComponent` over plugin services, the component the
   standalone and browser use, with the page set declared through Sheaf task
   S5.
5. **The plugin keeps no runtime configuration.** A plugin instance reads and
   writes no configuration file and opens no startup patch; a new instance
   starts at the default patch with no controller rows and no current patch
   (PLG-24). Each instance has its own state (PLG-12); the standalone's
   configuration holds its audio device, sync settings and reopen record,
   which no plugin instance may change; and a plugin writing it while the
   standalone runs would overwrite the standalone's saves. The plugin's
   patch actions therefore never change what the standalone reopens (QR-01).
   Controller setup persists per instance in the DAW project (PLG-10) and
   travels between instances and hosts in patches (PLG-22). Sheaf task S1
   makes the engine honour a host that names no configuration file.
6. **The File page works on the standalone's patches folder.** Load applies
   sound and setup to this instance (CTL-21); patches carry mappings
   (`catalog.patchCarriesMappings`). What the plugin saves appears in the
   standalone's File page. The standalone, when it has a reopen record,
   reopens its own recorded version; with none (a first launch, or a
   configuration older than the record), its startup rule opens the newest
   saved version, which can be one the plugin saved.
7. **The DAW project carries the whole instance, through one patch
   requester.** The session snapshot already carries the controller rows:
   the engine's serialize handling writes the `midiInstrument` section when
   the catalog sets `patchCarriesMappings`, and a restore applies it through
   `Engine::EditInstrument`. This change adds the File page's current patch,
   named on restore through Sheaf task S3, so Save after reopening adds a
   version to the same patch (FILE-06). Today the processor pushes its own
   `SerializeToJSON` requests, and `PatchManager::ProcessResponses` discards
   every response that is not its pending save, while the processor discards
   every response it did not request; a Save pressed while the DAW's
   snapshot is in flight writes nothing, or leaves the project's saved state
   stuck. `PatchSerializationContext` promises its arena is safe only while
   every serialize request flows through `PatchManager`. So `PatchManager`
   becomes the only requester (Sheaf task S2): the processor asks it for
   snapshots and receives each document in a consumer, and a Save asked for
   during a snapshot is held and dispatched right after it.
8. **The host is told when non-parameter state changes**, after a Controllers
   page commit and after an accepted New, Save As or Load. JUCE's VST3
   wrapper turns that flag into `setDirty(true)`; JUCE's AU wrapper (the
   `update` method of `AudioProcessorChangedUpdater` in
   `juce_audio_plugin_client_AU_1.mm`) never forwards it. The manual tells
   the user to save the Live Set after changing the controller setup.
9. **No host presets step.** Live's Save as Default Configuration does not
   save a plug-in's state; the manual names the File page as the route
   between instances.
10. **No modified indicator and no page LEDs.** The web's File page has no
    modified indicator (`NodeIds` in Sheaf `RuntimePages.hpp`; FILE-01), and
    no preset sends button feedback (`outputFeedback = false` in
    `AppActionButton` and `HeldButton`, `app/FroggersMidiCatalog.hpp`; APA-01
    and LPX-01), so neither is built.

## Policies

What each task follows where the code leaves a choice open.

- **Ownership.** The processor owns the engine, the connection manager and
  the timer, as `Runtime<App>` does. The editor owns the plugin services, the
  `RuntimeMainComponent` and the renderer, as `synth_runtime::MainPane` does.
  The processor offers a single-slot rebuilt hook that the services set and
  clear, as `Runtime<App>::SetMidiProcessorsRebuiltHook` does, and its timer's
  last step is the editor's repaint hook, which calls
  `RuntimeMainComponent::Refresh` and then refreshes the renderer.
- **Editor reopen.** Closing and reopening the editor shows the instrument
  view; the controller rows are the instrument's, and each row opens
  collapsed, as on a first visit to the page.
- **Size and the ? button.** The editor's design size is
  `RuntimeMainComponent::IntrinsicBounds` (the app's 900 by 712 plus the
  sidebar's 96). The ? button keeps its 20-pixel size and sits in the sidebar
  column directly below the last declared row, centred in the column, mapped
  through the same scale and offset as the renderer, so it covers no sidebar
  entry and no app control.
- **Services the plugin does not show.** The runtime services concept
  (`synth::runtime_ui::RuntimeMainServices`) still requires the Audio, Sync
  and deadline methods; the plugin services implement them as no-ops (the
  deadline reads 0) because task S5 makes those pages unreachable, not
  absent from the concept. `SaveRuntimeConfiguration` does nothing. The
  Controllers page's `saveRuntimeConfiguration` callback tells the host its
  state changed and answers true, so the page never shows a save failure for
  a configuration the plugin does not keep.
- **Directories.** The plugin creates no directory at construction. Save As
  creates the patch directory, as in the standalone.
- **State keys.** The current patch is stored in the snapshot's
  `sessionExtras` object under `currentPatch`, as a path relative to the
  patches root with `/` separators, absent when there is no current patch.
  A restore names the stored path through `Engine::NameCurrentPatch`; a
  missing, non-string or refused value leaves no current patch. Every
  snapshot also carries `controllerRows: true` in `sessionExtras`, marking
  state this version wrote. A restore applies the state's controller rows
  only when that mark is the boolean true. State without it was written by an
  earlier plugin, whose snapshot held the rows it had read from the
  standalone's configuration (the defect decision 5 fixes), never a setup the
  user made in the plugin: such a restore pushes a parameters-only copy of the
  document (its `schema`, `patchName` and `parameterValues` under
  `schemaVersion` 1, which `LoadPatchJSON` reads without an instrument
  section) and, in the same pump, sets the live instrument to
  `Engine::DefaultInstrument` through `Engine::EditInstrument`, so the
  instance restores its sound, IN:, page and Freeze and opens with no
  controller rows. One function writes every `sessionExtras` key, for the
  construction-time seed and for every snapshot.
- **Snapshot cadence.** The timer asks for a snapshot whenever none is
  outstanding and no save is outstanding or held; `getStateInformation`
  returns the last completed snapshot, which may be one save's round trip
  old while a save is in flight.
- **Notification.** The host is told of a change after a Controllers page
  commit, and after New and Load answer Ok and Save As or Save As
  (overwrite) answers Pending. Save, a restore, and a reconcile's own port
  write-back do not notify.
- **Tests and devices.** The processor's test constructor takes a device
  access; its default lists no devices and opens nothing, so no host test
  sees or drives a plugged-in controller.

## Story steps served, and the plugin steps this change writes

The plugin branch covers only what differs from the standalone (story section
1, bounds). These steps replace PLG-04, PLG-10, PLG-16, PLG-17 and PLG-19 to
PLG-22, keep PLG-18 with one added sentence, and add PLG-23 to PLG-27. Task
3.2 writes them into `openspec/story.md` once the operator ratifies them. APC40
and Launchpad steps are backed by code tests and are not run on hardware.
Logic, Reaper, Bitwig and Windows are not run.

| Plugin step | Mirrors | Text |
|---|---|---|
| PLG-04 `[SWEEP]` | SUR-01, SUR-09 | **Action:** open the editor, then open Controllers and File and press Back on each; close and reopen the editor. **Result:** the row shows Freeze with a "FREEZE" label and **IN:**, and no Play, Stop or Record. The sidebar holds Controllers and File, and no Audio I/O, Sync or load readout. A **?** button sits in the sidebar column below File. Each page replaces the instrument view, and Back restores it with its state intact. The reopened editor shows the instrument view. |
| PLG-10 `[SWEEP]` | QR-01, QR-05, CTL-17 | **Action:** edit knobs, set IN:, add a controller row, Save As a patch, then save, close and reopen the project. **Result:** the values, the IN: choice, the visible page, the controller rows and the File page's current patch return. Each row's ports reopen and its feedback is sent again, and Save adds a version to that patch. Saving and reopening the project writes no patch and changes nothing the standalone reads. A project older than the IN: setting opens with input off, one with no current patch opens with none, and new parameters keep their defaults. A project saved by an earlier version of the plugin opens with its sound and IN: choice and no controller rows. |
| PLG-16 `[SWEEP]` | CTL-01 to CTL-16, CON-01 to CON-03, PRT-01 | **Action:** open Controllers and go through CTL-01 to CTL-16, CON-01 to CON-03 and PRT-01. **Result:** as in S. |
| PLG-17 `[SWEEP]` | CTL-17, TWI-01 to TWI-09 | **Start:** a Twister set up as TWI-01, its port's Track and Remote switches off in the DAW as the manual describes, and a Twister row. **Action:** TWI-02 to TWI-09; then close and reopen the editor. **Result:** as in S, with two differences: the left middle button (Play, and Stop under Shift) does nothing (PLG-26), and Crispy's knob under Shift moves the tempo only while the host reports no tempo (PLG-06). The rows are as the page last had them after the editor reopens. |
| PLG-18 `[SWEEP]` | (kept) | **Action:** DAW-MIDI-learn knobs to host parameters, including relative encoders. **Result:** each moves one fixed parameter. How a relative stream moves it is up to the DAW. In Live the controller's port needs its Remote switch on for this; a control that a Live mapping and a controller row both map moves both targets. |
| PLG-19 `[SWEEP]` | APG-01 to APG-05, APA-01 to APA-03, LPX-01 to LPX-05, LPP-01, LPM-01 to LPM-03, CUS-01 to CUS-13, CTL-18 to CTL-21, GES-01 to GES-04 | **Action:** repeat those steps in the plugin. **Result:** as in S, except that PLAY, STOP and RECORD, and the Launchpad's Play, Stop and Record pads, do nothing (PLG-26), the APC40 master fader moves BPM only while the host reports no tempo (PLG-06), and CTL-21's open saves no configuration. |
| PLG-20 `[SWEEP]` | TWI-03, TWI-04, CON-03, APA-01, LPX-01 | **Action:** watch the controller through PLG-17 and PLG-19. **Result:** the Twister's rings and colours follow the page and the drill level. The APC40 (Ableton) connect message and each Launchpad's programmer-mode message are sent when the output connects. No clock goes out: the plugin has no Sync page. |
| PLG-21 `[UX]` | (kept) | **Action:** save and reopen the project with DAW mappings and controller rows. **Result:** the DAW restores its own mappings, and the project restores the rows (PLG-10). |
| PLG-22 `[SWEEP]` | FILE-01 to FILE-13, CTL-21 | **Action:** open File and go through FILE-01 to FILE-13. **Result:** as in S. The patches root is the standalone's patches folder, and the patches listed are the standalone's. Load applies sound and setup to this instance only, and saves no configuration. A version saved here appears in the standalone's File page. The standalone's next launch reopens the version it last opened or saved; a standalone with no such record opens the newest saved version, wherever it was saved from. |
| PLG-23 `[SWEEP]` | SWX-01 | **Start:** two rows, both connected. **Action:** use both, then replug one. **Result:** as in S. |
| PLG-24 `[SWEEP]` | LCH-01, LCH-02, LCH-03 | **Action:** insert a new instance. **Result:** the default patch (LCH-02, LCH-03), no controller rows and no current patch. The standalone's patches and configuration are not read, and nothing is written. |
| PLG-25 `[SWEEP]` | CTL-14, QR-07 | **Action:** unplug the controller, then plug it back in. **Result:** both dots go offline, and after replugging the row reconnects within one poll and resends its feedback. |
| PLG-26 `[SWEEP]` | TWI-05, APG-04, LPX-02 | **Action:** press a controller button mapped to Play, Stop or Record, with the DAW stopped and then playing; then press one mapped to Freeze. **Result:** Play, Stop and Record do nothing; the DAW's transport starts and stops the instrument (PLG-05). Freeze latches (PLG-08). |
| PLG-27 `[UX]` | (new) | **Action:** change only the controller setup, then close the Live Set, once with the VST3 and once with the AU. **Result:** the manual's instruction to save the Live Set after changing the controller setup holds: with the VST3, Live asks to save if it honours the plugin's change flag; with the AU, it does not ask. |

## Impact

- `app/vst/`: `FroggersPluginProcessor.hpp` and `.cpp` (data paths, the
  connection manager, the snapshot consumer, session extras, the host
  notification, the transport edge), `FroggersPluginEditor.hpp` and `.cpp`
  (the runtime main component, its size, the ? button), a new
  `FroggersPluginServices.hpp`, `FroggersVstHostTests.cpp`,
  `FroggersVstEditorTest.cpp`, and a comment in `CMakeLists.txt`.
- `app/`: `FroggersUiSurface.hpp` (the public start and stop, and the plugin
  gate on Play, Stop and Record).
- `External/Sheaf/projects/synth/`: the files the Sheaf change lists.
- `External/Sheaf/openspec/changes/app-plugin-controllers/`.
- `openspec/story.md`, `MANUAL.md`, and `openspec/specs/` through the archive.
- The `External/Sheaf` pin.

Overlap with other active changes:

- `frogg3rs-operator-runs` adds two `froggers-vst-host` requirements this
  change does not restate. Its task 2.1 stores a pending start edge from
  `prepareToPlay` for the timer to press Play; after this change the timer's
  start edge calls the surface's public start, which starts the transport as
  the `kPlay` branch did. Its task 2.2 edits the plugin editor's drag
  handling, which this change moves under `RuntimeMainComponent`; the renderer
  still delivers drags to the same app surface. Its task 1.1 (whether two
  input ports' callbacks run on two threads) now covers the plugin too,
  because the plugin opens input ports.
- Sheaf `app-operator-runs` task 1.2, if its run confirms it, changes how
  `MidiConnectionManager` builds input handlers; task S4 routes that through
  the device access, and whichever lands second follows the first.
- Sheaf `shorten-deadline-readout-window` edits the load readout task S5
  makes optional; `ui-state-before-audio` edits `Engine::MessageThreadTick`,
  which calls the `ProcessResponses` task S2 changes. Neither changes the
  behaviour this change depends on.

## Delivery

Sheaf: the S tasks land on the branch `app-plugin-controllers`, made from the
pinned commit (the tip of `fork/unbounded-patch-serialization-arena`), pushed
to the fork `daguilarc/Sheaf` and opened as the next sequential pull request
against `jvictor0/Sheaf` `main`, stacked on #22. frogg3rs: the pin moves to that
branch's tip, this change is archived, and the result is pushed to `main` on
`daguilarc/frogg3rs`, never as a pull request.
