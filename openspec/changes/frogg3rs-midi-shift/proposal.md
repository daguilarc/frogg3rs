# Proposal — `frogg3rs-midi-shift`

**Created 2026-09-07. Revised 2026-09-07 after operator feedback and again
after the preflight of the same day. Execution approved 2026-09-07.**

Paths are repo-root relative. Line numbers are 2026-09-07 reads of `main` at
`e891e1f`; the working tree is clean apart from this change's own directory.
Sheaf is the submodule at `External/Sheaf`, checked out on branch
`app-midi-catalog` at `b50cca18`, the head of the open pull request
jvictor0/Sheaf#13. Sheaf paths below are relative to
`External/Sheaf/projects/synth/`.

This is one change with two halves, executed in order: the Sheaf half
first (task group 1, on a branch of the Sheaf fork, delivered as the next
sequential pull request), then the app half (groups 2 onward, pushed to
`main`). Shift lives in Sheaf as a library message kind the app merely
keeps in its catalog; the Twister preset's shifted jobs are a per-button
field the Sheaf half adds to the mapping model; a finished recording reaches
every host through a file-export seam the Sheaf half adds. An earlier draft
put Shift in the app with fixed pairs; the operator rejected fixed pairs
(2026-09-07: "editable per mapping").

## What the operator asked for

Quoted from the request of 2026-09-07:

> i want to add a shift button that is only accessible to midi devices, not on
> the screen. the purpose of this button is to make more froggers-buttons
> accessible to devices that have fewer push buttons than number of features.
> excluding config menu buttons on the sidebar, please propose a mapping of
> buttons with shifted pairs. the goal is for midi twister to have play, stop,
> and freeze available (doesn't really need record) as well as scene selection
> (one button for scene 1, shift for scene 2), maybe randomize page + shift for
> randomize all; what others would work? the point is not to expand the number
> of buttons that can be controlled by midi aside from adding shift, but to
> give non-shift buttons two jobs instead of one with the shift button. in the
> twister preset, shift should be the bottom right button.

And the rulings that followed, same day:

- Bank Next is the press and Bank Previous the shifted job, because the app
  starts on the Audio page and most people go forward first.
- Freeze's shifted job is Reset Page, not Record, because no Reset was
  reachable from the hardware.
- Shifted jobs are editable per mapping on the Controllers page; the
  directed table below is the Twister preset's default, and no other preset
  gains a Shift button, since they have the buttons already.
- Recording gets a default file name of today's date, `YYYY-MM-DD`, whether or
  not a controller is involved. This change carries it.
- A recording made in the browser must be saveable ("yeah, no shit"). This
  change carries that too: the app hands a finished recording to the
  runtime as a file export, and each host saves it its own way.

The sidebar exclusion is already true of the catalog: its action list
(`app/FroggersMidiCatalog.hpp:257-276`) carries transport, randomize, reset,
bank, scene and BPM only; `kInputSelect` and `kViewportNarrow` are not
offered (`app/Makefile:264-268`).

## Impact

Directories this change edits; each is swept in task group 0 and named in the
sweep report.

- `app/`: `FroggersMidiCatalog.hpp` (Shift kept in `libraryKinds`, Twister
  layout with shifted jobs, one held-button helper, header comment),
  `FroggersMidiCatalogTests.cpp`; `FroggersAppCore.hpp` (a finished
  recording becomes a named file export), `FroggersUiSurface.hpp` (the
  Record branch's finish call, one line), `FroggersMain.cpp` (the standalone
  saves an export through its dialog), `FroggersSurfaceTests.cpp`;
  `app/browser/e2e/` (one new spec).
- repo root: `MANUAL.md` (`:102-111`, the transport section, gains Freeze
  and Record; `:237-242`, the browser section; `:288-309`, the mappable
  list, a Shift subsection, the Twister section); `QUICK_DICT.md` (`:14`,
  one transport line).
- `openspec/changes/`: this change. `openspec/specs/`: a new spec
  `froggers-midi-controller-mappings/` and an added requirement in
  `froggers-transport-and-reset-controls/` land on archive. The Sheaf
  half's spec deltas start in this change's `sheaf-specs/` and move to
  `External/Sheaf/openspec/changes/shift-and-file-export/specs/` in task
  1.S5.1, which deletes `sheaf-specs/`: every Sheaf pull request carries
  its own change directory (`External/Sheaf/openspec/changes/app-midi-catalog/`
  for jvictor0/Sheaf#13), and a copy in two repos is one family in two
  places.
- `External/Sheaf/projects/synth/`: the Sheaf half, task group 1. `include/synth/MidiController.hpp`,
  `ParameterModulation.hpp`, `MidiConfigViewModel.hpp`, `MidiConfigBlocks.hpp`,
  `ControllersPageUI.hpp`; `src/MidiController.cpp`, `ParameterModulation.cpp`,
  `MidiConfigViewModel.cpp`, `MidiConfigBlocks.cpp`, `ControllerWizard.cpp`;
  `tests/instrument_tests.cpp`, `engine_tests.cpp`, `viewmodel_tests.cpp`,
  `portable_ui_tests.cpp`, `controllers_page_ui_tests.cpp`.

Worth saying: Shift never reaches the app. The surface's action router
changes only where Record finishes; its action constants, the coverage
check (`app/check_catalog_covers_screen_actions.sh`) and its exclusion list
are unchanged.

## Data flow, as it runs today

Every claim below was read at the cited line.

**A side button, pressed.** The Twister sends CC 8..13 on channel 3, value
127 on press and 0 on release, because the manual has the operator set the
buttons to CC Hold (`app/FroggersMidiCatalog.hpp:14-19`, `MANUAL.md:305-309`).
Sheaf's system-button processor finds the association by address and treats
a CC above zero as a press (`src/MidiController.cpp:940-941`). A press whose
message kind is Hold Drill flips per-profile state and pushes nothing
(`:943-957`); any other press pushes the association's `press` message, and
a release pushes `*release` only when the association has one (`:959-966`).
The per-profile state object is created once per controller profile and
handed to the encoder and system processors (`:2960-2961`, `:2982`,
`:2998`); the profile result owns it (`include/synth/MidiController.hpp:937`).

**What an association is.** `MidiControllerSystemMessageAssociation`
(`include/synth/MidiController.hpp:907-917`): an address, `press`, an optional
`release`, `feedback`, `outputFeedback`, and the `appAction`/`appActionValue`
pair that names an app action when `press` is `AppAction`. The processor
works from a trimmed copy with address, `press` and `release` only
(`:368-373`, built at `src/MidiController.cpp:2990-2996`). JSON writes `press`,
nullable `release`, and the name/value pair only for an `AppAction` press
(`:2589-2600`); reading tolerates absent keys (`:2636-2662`).

**From the bus to the app.** An `AppAction` message is forwarded to the
engine's message thread (`src/ParameterModulation.cpp:4234-4237`) and
dispatched as a `ui::Action` named by the catalog entry at the message's
index (`include/synth/Engine.hpp:504-514`). That index is resolved on every
processor rebuild, on a copy of the slot's config, by name/value pair
(`:961-964`, `:1040-1052`).

**Hold Drill is the template.** It is the one library kind that is a held
modifier: `MessageIn::Type::HoldDrill` and `UISystemMessage::HoldDrill` are
the last enumerators of their enums (`include/synth/ParameterModulation.hpp:
978`, `include/synth/MidiConfigViewModel.hpp:227`); the kind appears at
every switch over message kinds. Enumerated by operand: 25 sites in
`src/MidiController.cpp`, 12 in `src/MidiConfigViewModel.cpp`, 3 in
`src/ParameterModulation.cpp`, 2 each in `src/ControllerWizard.cpp` and
`src/MidiConfigBlocks.cpp`, 6 in `include/synth/MidiController.hpp`, 2 each in
`include/synth/ParameterModulation.hpp` and `MidiConfigViewModel.hpp`, 1 in
`include/synth/MidiConfigBlocks.hpp`. A new kind visits the same family; the Sheaf half below lists them. The Controllers page offers Hold Drill as a choice the
library produces rather than the static catalog (`src/MidiConfigViewModel.cpp:
506-509`), and an app keeps it by naming it in `libraryKinds`
(`app/FroggersMidiCatalog.hpp:278-284`).

**The Controllers page row.** A system-message row's editable fields are its
address fields, then the message kind, then an argument field when the kind
takes one (`src/MidiConfigViewModel.cpp:1040-1067`). The page draws one combo
per field at a fixed width (`include/synth/ControllersPageUI.hpp:556-600`;
kind and app-action combos 150 px), and sizes the section to the widest row
plus its gaps (`:2852-2886`, gap 4 px at `:480`). A gate proves the page fits
a 900 px host in every open state and can fail
(`tests/portable_ui_tests.cpp:3010`); it also asserts the Generic system
row's Message combo offers 24 choices (`:3376-3377`), which is Froggers'
five kept kinds plus nineteen actions.

**Recording.** The surface's Record branch arms, stops and fires a
finished callback (`app/FroggersUiSurface.hpp:2207-2229`). Only the
standalone registers that callback (`app/FroggersMain.cpp:192-231`): it copies
the samples out, then opens a save dialog whose default file is
`frogg3rs-recording.wav` in the user's documents folder (`:211-215`), with
overwrite warning on (`:216-217`). That string appears nowhere else in the
tree. The plugin has no Record: its transport is the DAW's, and the surface
in plugin mode draws only Freeze (`app/FroggersUiSurface.hpp:1244-1280`,
`MANUAL.md:218-219`; plugin mode is set only by the plugin processor,
`app/vst/FroggersPluginProcessor.cpp:205`). The browser build is not in plugin
mode, so it draws Play, Stop and Record (`MANUAL.md:239`), yet registers no
finished callback (no other caller of `SetOnRecordingFinished` exists outside
tests), so in the browser a finished recording is captured and dropped
(`app/FroggersAppCore.hpp:549-552`).

**Tests that pin the current shape.** `app/FroggersMidiCatalogTests.cpp:434-448`
asserts the Twister's six system messages, their order, addresses and
feedback flag; `:405-414` runs every preset through `SlotValidForKind`;
`:550-577` resolves every Launchpad pad action against the catalog.
`app/FroggersControllersPageTests.cpp:96-118` builds the real catalog through
the view model. `app/FroggersSurfaceTests.cpp:3220-3238` pins that a stopped
recording fires the finished callback exactly once.

## Design

**D1. Shift is a Sheaf library kind, held like Hold Drill.** A button mapped
to Shift sets per-profile state on press and clears it on release, pushes
nothing to the bus, and the app never sees it. That is why there is no
on-screen Shift: nothing on screen can be it. A release that never arrives
(unplug mid-hold, or a device sending CC Toggle) leaves the profile shifted
until the next Shift press and release, the same as Hold Drill; the manual
says so.

**D2. Each button carries its own shifted job.** The association gains an
optional shifted press with its own action name/value pair. While Shift is
held, a press pushes the shifted message when there is one and the ordinary
message otherwise; a release behaves as it does today. The Controllers page
shows the shifted job as one more combo on the system row, headed Shift,
offering none plus every argument-free choice of the row dropdown. The
combo's arithmetic: a Generic system row is 90 + 66 + 66 + 150 + 74 wide plus
four gaps today, 462 px; with Shift, 616 px, well inside the 900 px gate,
which task group 1 re-runs rather than trusts.

**D3. The Twister preset's default.** Five buttons carry ten jobs. Reset All
and Record stay on screen only.

| CC (ch 3) | position | press | Shift + press |
|---|---|---|---|
| 8 | left, top | Bank Next | Bank Previous |
| 9 | left, middle | Play | Stop |
| 10 | left, bottom | Freeze | Reset Page |
| 11 | right, top | Scene 1 | Scene 2 |
| 12 | right, middle | Randomize Page | Randomize All |
| 13 | right, bottom | Shift | (none) |

The Utility settings do not change: CC Hold on every side button is what
makes Shift's release observable. Bank navigation wraps in both directions
(`app/FroggersUiSurface.hpp:2320-2336`), so Bank Previous from bank 1 still
lands on bank 6.

**D4. Other presets are unchanged.** APC40 and Launchpad tables keep every
button; the APC40's own Shift key stays on Hold Drill
(`app/FroggersMidiCatalog.hpp:118`). Shift appears in the row dropdown, so any
device can be given a Shift button and any button a shifted job by hand.

**D5. A finished recording is a file export the app names.** The app core
already encodes WAV in pure standard C++ (`app/FroggersAppCore.hpp:2524`).
When a recording stops with data, the core encodes it, names it
`<YYYY-MM-DD>.wav` from the local clock via `<chrono>`/`strftime`, and
queues it as a file export: name, media type, bytes, and a note that reads
"stopped at the 30-minute limit" when the capture was truncated. A capture
that hits the cap is disarmed by the audio thread itself
(`app/FroggersAppCore.hpp:1186-1194`), so no stop press ever reaches the
surface's branch for it; the core queues that export from the poll the
engine already runs once per tick (`TakePendingFileExport`), and
`ArmRecording` queues it first when a Record press arrives before the next
tick, so the file is offered the moment the cap hits and a fresh take never
wipes an unsaved one. The
`SetOnRecordingFinished`/`NotifyRecordingFinished` pair
(`app/FroggersAppCore.hpp:543-552`) goes away; the surface's Record branch
(`app/FroggersUiSurface.hpp:2225-2228`) calls the queueing method instead.
The Sheaf half adds the seam the queue drains through: an optional app hook
the engine polls once per message tick and hands to a host-installed
handler. Each host saves as it can. The standalone installs its existing
dialog and byte-stream code as that handler, opening on the export's name in
the documents folder with the overwrite warning kept, and shows the note in
its completion alert (`app/FroggersMain.cpp:206-256`, minus the encoding and
the naming, which moved into the core). The browser runtime's handler is
built in: the worker posts the export to the page, which offers it as a
download under that name. Numbering same-day files was considered and not
taken: the operator asked for the date, and the standalone's warning makes
the collision visible; a browser download is renamed by the browser itself.

**D6. The surface shows Record's refusal itself.** The core already keeps
the refusal text (`app/FroggersAppCore.hpp:488-510`). The surface's Record
branch (`app/FroggersUiSurface.hpp:2207-2229`) sets a transport notice to
that text when arming is refused and clears it when a Record arms or Play is
pressed; the transport cell (`:1227-1300`) becomes a column of the plates
row and, beneath it and only while the notice applies, a label carrying
it. The row of four plates is 28 px tall inside a cell one sixth of the
left column's height, so the second line fits without any other row
moving; beside the plates it does not: the row is 285 px wide at the wide
layout and the sentence needs about 300. That is one
definition, rendered by every host that shows Record. Task 4.3 asserts the notice's resolved bounds lie inside the transport
cell's and that the plates' bounds are unchanged when no notice applies;
task 3.4 asserts the first in the browser. The
`SetOnRecordRefused`/`NotifyRecordRefused` pair leaves the core and the
standalone's modal alert leaves its main; the standalone shows the same
inline text as the browser.

**D7. One held-button helper.** `HoldDrillButton`
(`app/FroggersMidiCatalog.hpp:62-71`) becomes `HeldButton(address, kind)` used
for Hold Drill on the APC40 and Shift on the Twister; `AppActionButton` gains
the shifted action/value pair as trailing parameters that default to empty,
so the APC40 and Launchpad tables do not change.

## Sweep findings

Nine found, nine dispositioned.

1. **The Controllers page offers BPM as a button.** `MakeUISystemMessageChoices`
   includes analog-ranged actions in the row dropdown
   (`src/MidiConfigViewModel.cpp:514-522`); a button mapped to BPM dispatches
   the range minimum on every press. Fixed in the Sheaf half. The 24-choice
   assertion at `tests/portable_ui_tests.cpp:3376` stays true for Froggers:
   Shift joins the kept kinds and BPM leaves the dropdown.
2. **Three near-identical combo branches in the page's field emitter**
   (`include/synth/ControllersPageUI.hpp:2666-2725`: message kind, app
   action, encoder mode). A Shift combo would be a fourth. The Sheaf half
   folds them into one branch parameterised by options and current index.
3. **Record in the browser was captured and dropped.** The browser build
   shows Record and runs its own transport (`MANUAL.md:239`), but no browser
   host registered the finished callback (see Recording, above). The plugin
   is not affected: its transport is external and it shows no Record. Fixed
   here by D5 and the Sheaf half's export seam; the manual's browser section
   says a stopped recording downloads under today's date (task 7.2).
4. **Record's refusal was silent in the browser too.** "Press Play before
   recording." reached only a host-registered callback
   (`app/FroggersAppCore.hpp:554-558`), which only the standalone installed,
   as a modal alert (`app/FroggersMain.cpp:192-195`). In the browser the
   press did nothing visible. Fixed here by D6: the surface shows the
   refusal itself, so no host seam is needed and the modal goes.
5. **The Freeze button and Record are undocumented.** The manual's transport
   section (`MANUAL.md:102-111`) and the quick dictionary's transport line
   (`QUICK_DICT.md:14`) describe Play and Stop only; Record appears in the
   manual only in the plugin, browser and MIDI sections, and the Freeze
   latch nowhere. The operator's note of 2026-09-07 had already flagged the
   Freeze half; the docs pass that landed since fixed parameter names, not
   this. Fixed here, tasks 7.2 and 7.3, since this change is what gives
   Record its file name and its notice.
6. **`MANUAL.md:302-309` restates the Twister layout.** Rewritten, task 7.1.
7. **Manual drift on the Delay bank** (the operator's note). Fixed on main by
   `45b3193`, whose docs check reads only the bank sections
   (`app/check_docs_match_parameter_table.py:1-20`) and is unaffected here.
8. **Other active changes.** frogg3rs: none; this is the only entry under
   `openspec/changes/`. Sheaf: `app-midi-catalog` (two open tasks: postflight,
   commits) and `rework-controllers-block-editing` (six open tasks, all
   simulation and harness; its code is on this branch, `5b6d1cb7`). Both
   define requirements the Sheaf half modifies, so its deltas apply after
   both archive. The other seven Sheaf changes were grepped
   for app actions, the resolve function and the dropdown builder; none
   mentions them. Every Sheaf pull request from #9 to #13 is open against
   upstream `main`, stacked, and carries its own change directory; this
   change's Sheaf half does the same (Impact, above).
9. **The held-button pair is built in two places.** The view model's
   `PressForUISystemMessage`/`ReleaseForUISystemMessage` build
   `HoldDrill(0, true)`/`HoldDrill(0, false)` for a page edit
   (`src/MidiConfigViewModel.cpp:241-242`, `:276-277`); the app's
   `HoldDrillButton` builds the same pair for a device default
   (`app/FroggersMidiCatalog.hpp:62-71`). The library's builder takes a
   `UISystemMessageChoice`, and the static catalog carries none for a held
   kind (`:507-508`), so the app cannot call it. Found 2, changed 1: the
   app's helper becomes `HeldButton` (D7) and serves both held kinds.

## The Sheaf half

Runs first, as task group 1, on a branch of the Sheaf fork. Paths in this
section are relative to `External/Sheaf/projects/synth/`; line numbers are
reads of `b50cca18`. Its spec deltas are in this change's `sheaf-specs/`
directory (smi-1, smi-2, smi-8, sru-15, sru-16, sru-59 modified; smi-16,
sar-33, sbw-12 added) and are applied to Sheaf's spec tree by task 1.S5.1.

### Why

Two seams the frogg3rs change needs and the library lacks. The first is
Shift. The second is a way for an app to hand a file to whatever host it
runs on: frogg3rs records audio, and in the browser the recording is
captured and dropped, because the app's finished-recording callback is a
seam only its JUCE main knows how to fill (the browser runtime is generic,
`browser/src/build-browser-apps.mjs`, and binds only the app type,
`include/synth/browser/BrowserAppEntry.hpp:8-16`). The runtime has no way
for an app to say "here is a file, save it" that works on both hosts.

**Shift.** An app with more buttons on screen than a controller has under hand needs a
Shift: a held button that gives every other button a second job. The
library has one held modifier today, Hold Drill, and it shows the shape:
a message kind whose press and release flip per-profile state in the
system-button processor and push nothing to the bus
(`src/MidiController.cpp:943-957`). It has no notion of a button doing a
different thing while a modifier is held. The operator ruled that the
second job belongs to the mapping, editable per row on the Controllers
page, not to a fixed table in the app.

### What Changes

- **A `Shift` message kind**, `MessageIn::Type::Shift` and
  `UISystemMessage::Shift`, appended after `HoldDrill` in both enums so no
  ordinal moves, with `MessageIn::Shift(timestamp, held)` shaped like
  `MessageIn::HoldDrill` (`src/ParameterModulation.cpp:4071-4078`). The kind
  visits every switch Hold Drill visits; the family is enumerated below.
- **A shifted press on the association.**
  `MidiControllerSystemMessageAssociation` gains `std::optional<MessageIn>
  shiftedPress`, `std::string shiftedAppAction`, `std::string
  shiftedAppActionValue`. The processor's trimmed copy
  (`SystemButtonMidiAssociation`, `include/synth/MidiController.hpp:368-373`)
  gains `shiftedPress` and the profile builder copies it
  (`src/MidiController.cpp:2990-2996`).
- **Per-profile Shift state.** A `ShiftState { bool held; }` owned by the
  profile result beside `holdDrill` (`include/synth/MidiController.hpp:937`),
  created at `src/MidiController.cpp:2960` and handed to the system-button
  processor only; the encoder processor does not read it.
- **Processor behaviour.** A press whose kind is Shift sets held; its release
  clears held; neither pushes. Any other press pushes `*shiftedPress` when
  Shift is held and the association has one, else `press`. A release is
  unchanged. Shift and Hold Drill are independent: both can be held.
- **Persistence.** `shiftedPress` serializes beside `release`, null when
  absent; `shiftedAppAction`/`shiftedAppActionValue` are written only when
  the shifted press is `AppAction`. A document without the keys loads with
  no shifted press, the way `openSysEx` did (`:2750-2753`).
- **Resolve.** `ResolveAppActionsAgainstCatalog` (`include/synth/Engine.hpp:
  1040-1052`) resolves a shifted `AppAction` press by its own name/value pair.
  An unresolvable shifted press is cleared on the copy; the row itself is
  kept, so a preset whose shifted job a later catalog drops keeps its
  ordinary job.
- **Controllers page.** A new `MidiMappingRowVM::Field::ShiftAction`,
  declared last in `Field` (after `GridYMax`; a field's token is the
  enumerator's integer, `include/synth/ControllersPageUI.hpp:761-777`, so
  appending keeps every existing token), on every individual
  system-message row whose own kind is neither Shift nor Hold Drill,
  appended after the argument field (`src/MidiConfigViewModel.cpp:
  1063-1067`), 150 px wide (`include/synth/ControllersPageUI.hpp:561-563`),
  column header "Shift". Its choices are none, then every entry of the row
  dropdown whose kind takes no argument (`UISystemMessageHasArg`,
  `src/MidiConfigViewModel.cpp:97-127`), read through a new
  `ShiftChoiceIndex` shaped like `UISystemMessageIndex` (`:1828-1856`);
  the choice list is derived once, where `SetMessageCatalog` stores the
  row dropdown (`:541-543`), not per field per frame and
  committed through the `MessageKind` edit path's shape (`:2690-2705`)
  against `shiftedPress`. The row dropdown offers Shift the way it offers
  Hold Drill (`:506-509`). The row label gains "shift on"/"shift off"
  (`:726-728`); the sort key treats Shift like Hold Drill
  (`src/MidiConfigBlocks.cpp:119-122`).
- **A file export seam.** `synth::FileExport { std::string fileName;
  std::string mediaType; std::vector<std::uint8_t> bytes; std::string
  note; }` and an optional app hook `HasFileExports<App>`:
  `app.TakePendingFileExport()` returning `std::optional<FileExport>`,
  declared beside the other optional hooks (`include/synth/AppConcepts.hpp:
  69-82`). `Engine` gains `SetFileExportHandler(std::function<void(FileExport)>)`
  and, at the end of `MessageThreadTick` after the bus drain
  (`include/synth/Engine.hpp:497-535`), takes every pending export and hands
  it to the handler; with no handler installed it logs the drop by name. An
  app without the hook sees nothing.
- **The browser saves an export as a download.** The browser runtime
  installs a handler that queues exports (`include/synth/browser/
  BrowserRuntime.hpp:713-723`, beside the persistence flag), and a new ABI
  entry `synth_browser_dequeue_file_export` returns the next one in the
  pointer-plus-size shape `synth_browser_build_ui_frame` uses
  (`browser/src/worker.ts:340-345`), name and media type as UTF-8 fields.
  `BrowserRuntimeWorker`'s `message-tick` case (`worker.ts:619-622`) then
  drains the exports and emits each through its `emitStatus` callback as
  `{ type: "file-export", fileName, mediaType, bytes }`, the way
  `page-status` is emitted (`:46`, `:478`, `:593`). Both runtime clients
  are served by that one drain: the direct client (`main.ts:89-121`) runs
  the worker class in the page realm and hands `emitStatus` straight to
  its status handlers; the worker client (`:124-155`) receives it as a
  worker message and forwards it to its status handlers the way it
  forwards `page-status` (`:129-131`), with its reply listener ignoring it
  the same way (`:136-138`). `SynthBrowserApp`'s status subscription
  (`:183`) turns a `file-export` into a Blob, an object URL and an anchor
  click with `download` set to the name, revokes the URL, and passes every
  other status to `renderStatus` as today. The generic-runtime check
  (`browser/tests/check-generic-runtime.mjs`) scans these files for app
  identity and forbidden audio fallbacks; the new code names no app. The
  standalone needs nothing from the library: its main already reaches the
  engine (`frogg3rs app/FroggersMain.cpp:177`) and installs its own
  dialog as the handler.
- **Sweep finding, fixed here: analog-ranged actions leave the row
  dropdown.** `MakeUISystemMessageChoices` (`src/MidiConfigViewModel.cpp:
  514-522`) offers every catalog action as a button target, BPM included; a
  button so mapped dispatches the range minimum on every press
  (`include/synth/Engine.hpp:508-511` with message value 0). The dropdown now
  skips analog-ranged actions; `MakeAnalogAppActionChoices` (`:526-539`)
  owns them.
- **Sweep finding, fixed here: one combo emitter.** The page's field
  emitter has three near-identical combo branches, message kind, app action
  and encoder mode (`include/synth/ControllersPageUI.hpp:2666-2725`), differing
  only in the option source and the current-index read. The Shift combo
  would be a fourth. They become one branch taking options and current
  index.

### The Hold Drill family, by operand

Every site is a site Shift visits; the executor reports found versus
changed against this list, zeros included.

| file | sites | what they are |
|---|---|---|
| `src/MidiController.cpp` | 25 | kind name and parse (`:233-234`, `:288-289`); processor state and press/release handling (`:676-681`, `:708-714`, `:918-921`, `:943-957`); feedback exclusion (`:1856`); MessageIn JSON (`:2411`, `:2483`); profile creation and hand-off (`:2960-2961`, `:2982`, `:2998`) |
| `src/MidiConfigViewModel.cpp` | 12 | arg switches (`:50`, `:92`, `:123`), kind mapping (`:188-189`), press and release builders (`:241-242`, `:276-277`), the library-produced choice (`:507-508`), row label (`:726`) |
| `src/ParameterModulation.cpp` | 3 | factory (`:4071`), bus `Apply` no-op (`:4239`) |
| `src/ControllerWizard.cpp` | 2 | built-in Twister form switches (`:185`, `:396`) |
| `src/MidiConfigBlocks.cpp` | 2 | kind-count comment (`:26`), sort key (`:119`) |
| `include/synth/MidiController.hpp` | 6 | state struct and constructor parameters (`:249-275`, `:383-394`), profile result (`:937`) |
| `include/synth/ParameterModulation.hpp` | 2 | enumerator (`:978`), factory (`:1044`) |
| `include/synth/MidiConfigViewModel.hpp` | 2 | enumerator (`:227`), comment (`:266`) |
| `include/synth/MidiConfigBlocks.hpp` | 1 | kind-count comment (`:75`) |

The encoder-processor sites (`:676-681`, `:708-714`, `:2982`) are Hold
Drill's alone; Shift is consumed where it is set. Both counts are reported.

### Impact

- `include/synth/MidiController.hpp`, `ParameterModulation.hpp`,
  `MidiConfigViewModel.hpp`, `MidiConfigBlocks.hpp`, `ControllersPageUI.hpp`,
  `Engine.hpp`, `AppConcepts.hpp`, `browser/BrowserRuntime.hpp`.
- `src/MidiController.cpp`, `ParameterModulation.cpp`,
  `MidiConfigViewModel.cpp`, `MidiConfigBlocks.cpp`, `ControllerWizard.cpp`;
  `browser/cpp/BrowserRuntimeAbi.cpp`; `browser/src/worker.ts`, `main.ts`,
  `protocol.ts`.
- `tests/instrument_tests.cpp`, `engine_tests.cpp`, `viewmodel_tests.cpp`,
  `portable_ui_tests.cpp`, `controllers_page_ui_tests.cpp`,
  `browser_runtime_contract_tests.cpp`; `browser/tests/fixtures/cpp/
  FakeBrowserApp.hpp` and one new Playwright spec under `browser/tests/`.
- `openspec/changes/shift-and-file-export/`: the Sheaf-side change
  directory task 1.S5.1 creates (proposal, tasks, and `specs/` moved from
  this change's `sheaf-specs/`), in the shape of `app-midi-catalog/`;
  deltas to smi-1, smi-2, smi-8, sru-59, sru-15, sru-16 as they stand in
  the two active changes, and new smi-16, sar-33, sbw-12.

Not touched: `include/synth/ControllersPageUI.hpp`'s width constants; the
fits gate decides the row (below). `ProfileConfigValidForKind`
(`src/MidiController.cpp:3388`) constrains sections and addresses, not a
message's presence, and needs no change.

### Data flow after the change

Shift button down → CC above zero → processor finds the association, kind
Shift → `shift_->held = true`, nothing pushed. Another button down → its
association has a shifted press → that message is pushed with the same
stamp `press` would have had; if it is `AppAction`, its index was resolved
on the rebuilt copy by the shifted name/value pair, and the engine
dispatches the catalog entry at that index exactly as for an ordinary press
(`include/synth/Engine.hpp:504-514`). Shift button up → held cleared.

### The row fits

A Generic system row is address type 90, channel 66, CC 66, kind 150,
argument 74 and four gaps of 4 (`include/synth/ControllersPageUI.hpp:
556-600`, `:480`, `:2852-2862`): 462 px. With the Shift combo, 616 px. The
gate `TestControllersRowFitsWithinFroggersNarrowestHost`
(`tests/portable_ui_tests.cpp:3010`) builds every open state at 900 px and
has a proven positive control; it is re-run, not reasoned about. Its
24-choice assertion (`:3376-3377`) counts a hand-built fixture catalog
(`:3070-3112`): five kept kinds, ten plain actions, six banks, two scenes
and one analog-ranged BPM. The fixture mirrors Froggers' catalog, so task
1.S4.4 adds `Shift` to its `libraryKinds` as task 2.1 does to the app's;
BPM leaves the dropdown and the count stays 24 (six kinds, eighteen
actions).

### Data flow of an export

App stops a recording on the message thread → it queues one `FileExport` →
the engine's next tick takes it and calls the handler. Standalone: the
handler is the app main's dialog; the file is written where the operator
chooses. Browser: the runtime's handler appends to a queue; the worker's
tick dequeues, copies name, media type and bytes out of the wasm heap, and
posts them to the page with the bytes transferred; the page offers the
download. Nothing is persisted in the runtime's own storage and nothing
touches the audio thread.

## Delivery

In order. The Sheaf half lands on a new branch `shift-and-file-export` on the
Sheaf fork, cut from `app-midi-catalog`, pushed to the fork, and opened as
the next pull request against upstream `main`, the same way as
jvictor0/Sheaf#9 through #13. The submodule pin moves to that branch's
head. Then the app half is pushed to frogg3rs `main`.
