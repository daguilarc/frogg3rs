# Proposal — `frogg3rs-presses-on-the-bus`

Paired with the Sheaf change `app-commands-on-the-bus` (External/Sheaf;
its proposal is what the upstream reviewer reads, and every task for both
trees is in this change's `tasks.md`). This change supersedes the unpushed
`frogg3rs-o1-audit` / `app-o1-audit` pair: it keeps the bug fixes those
changes made toward ratified story steps and replaces their press-ordering
and storage machinery with Sheaf's own message stream and Sheaf's own
storage request. It is written from origin/main 2255303 and Sheaf 62829a4a,
the bases both unpushed branches share. The ratified story, NEW
`openspec/story.md`, is carried to main by this change (task 2.0); its
player-facing step IDs are the ones cited below.

## Why

Every control on the Frogg3rs surface that commands the audio thread
already does so on Sheaf's ordered UI bus except seven: Randomize All,
Randomize Page, Reset All, Reset Page, page select, encoder press and the
BPM slider. (Freeze, Record and the plugin's input selection cross as
values, which stays; the page arrows are page selects the surface computes
on the message thread.) Those seven call `FroggersAppCore::Request*`, which
writes a single-slot atomic that `ProcessFrame` drains once per block in a
fixed order. The header comment
that justifies the bridge says Sheaf has "no existing generic `MessageIn`
shape" for them. That was true when it was written and is false now:

- Tempo. Sheaf carries `MessageIn::SetTempoBpmNormalized` and
  `TempoBpmIncDec`, and `Engine` wires both buses to the master clock with
  the catalog's tempo action range (`MessageInBus::SetTempoClock`). Frogg3rs
  already declares that action (`catalog.tempoAction = kBpm`, range 30–300);
  a Twister shifted turn moves the tempo through it today. Only the slider
  still goes through `pendingTempoBpmRequest_`.
- Presses. Sheaf carries `MessageIn::AppAction` on both buses, but applies it
  by forwarding to the message thread (`ParameterMessageOut::AppAction` →
  `Engine::MessageThreadTick` → `PortableSurface().DispatchAction`), and the
  encoder press the same way as `ParameterMessageOut::AppEncoderPress`. There
  is no audio-thread delivery of an app command. That gap is why the bridge
  exists, and the Sheaf half of this change closes it with one message type
  and one optional app hook.

The bridge loses presses. Two presses of one kind inside one 33 ms message
tick collapse into one (MOD-09's two Back presses), presses of different
kinds reorder (the drain runs Randomize before Reset, so a Randomize pressed
after a Reset is undone by it), and two page-arrow presses in one tick
advance one page, because the surface computes the target from the
published page (SUR-03). The story steps the player hits: RND-01, RND-02,
RND-05, RST-01, RST-04, MOD-09, SUR-03, TRN-02, BPM-01, SYN-03 to SYN-05,
PLG-06. `frogg3rs-o1-audit` answered this
with a second queue beside Sheaf's (`FroggersPressQueue`, a 64-slot ring with
a producer mutex), a shared sequence number between that queue and Sheaf's
patch bus (`InputOrder`, `HasOrderedPresses`), and a Sheaf construct that
holds a press until depth storage covers it (`RunIfDepthStorageCovers`).
None of that is needed once presses travel on the bus that already orders
everything else.

Depth storage runs short through real player paths, with nothing on screen.
The before-code audit reproduced three on origin/main through the rig
(evidence/adjudication-design, storage_probe): a parameter-page Randomize
All after a drill session draws short (RND-01, RND-02); a relaunch, plugin
session restore or Load of a patch saved from that session drops depths and
reports Ok (FILE-07, QR-01, QR-04, PLG-10); a patch that fits on its own,
loaded in the same tick as a Randomize All, leaves the press short. The
operator ruled the first the bug to fix. The answer is Sheaf's existing
low-water storage request with a number sized for this app, and a patch
that gets its storage before it applies, at startup and running, the way it
already gets its arena.

The stories are canonical and do not change. Frogg3rs's Reset and Randomize
keep their own semantics (one-shot presses that redraw or restore, as 1.8
states); they are not Sheaf's held Reset and Random modifiers, and this
change does not route them there. What changes is the transport that
carries the press, never what the press does.

## The rule this change installs

What crosses between the message thread and the audio thread is one of
three things, and every cross-thread member of `FroggersAppCore` is declared
as one of them:

- A **command** is counted and ordered: it travels as a `MessageIn` on
  `AppContext::uiBus` and is applied on the audio thread in the order
  pushed, by the same drain that applies encoder turns and scene blend.
  Start, Continue, Stop and Clock are realtime messages the engine lifts out
  of both buses and applies after both have drained, as today. The six
  presses are commands.
- A **value** is last-writer-wins and unordered against commands: one
  atomic the message thread writes for another thread to read. A press
  whose effect the message thread computes, and that crosses as the
  resulting value, is a value. The host's routed-input signal, the Freeze
  latch and the Record arm are values the audio thread reads (the Record
  arm also has one audio-thread writer, the length cap); the desired
  transport state is a value `PrepareToPlay` reads, on whichever thread
  calls `Engine::Prepare`.
- An **audio-thread publication** is written by the audio thread for the UI
  to read: the drill level shown in the header, whether the last randomize
  drew short, and the recorded frame count and truncation flag. This is
  Sheaf's `UIState` idiom.

One member is outside the rule and says so at its declaration: the Record
writer handshake the audio thread raises around a block, which the
re-arm fix (carried task 2.1) uses.

## What changes

### Sheaf (`app-commands-on-the-bus`)

1. NEW `MessageIn::Type::AppCommand`, appended after `SetTempoBpmNormalized`
   so every enumerator keeps its ordinal. It carries an app-defined command
   number and a float value, and Sheaf never interprets either: the app
   pushes it from its surface and applies it in its own hook.
   `Engine::DrainMessageBus` hands it to `app_.ApplyAppCommand(command,
   value)` when the app declares the concept NEW `HasAppCommands`; for an
   app without the hook the bus's own `Apply` drops it, since nothing but
   that app's own surface produces one. No hold: a command is applied in
   the block that pops it, like every other message.
2. The depth-storage low watermark becomes a `ParameterGroup` setting
   (NEW `ParameterGroup::SetStorageLowWatermark`), read by the existing
   `RequestParameterStorageBatchIfLow` at its existing call sites and as the
   request-size floor. The default stays what it is today
   (`numModulators * 2`).
3. A patch whose depths would leave available storage below the watermark
   gets its storage before it ever reaches the parameter authority, not
   retried after a failed apply. The caller that parses the patch document
   -- on the message thread, in every case -- already holds the
   `ParameterManager` before the load message even exists, so it provisions
   there, once, immediately before pushing: NEW
   `ParameterManager::ProvisionStorageForPatchValues` adds, to every group
   the patch's depths would leave short of its own watermark, one storage
   batch sized at the missing count plus the watermark (the same call the
   engine's existing low-water top-up already makes), and leaves a group
   with enough room already untouched. Two callers reach it, each
   immediately before its own push: Sheaf's `PatchManager::LoadPatchVersion`
   (an on-disk Load, whether it is the startup patch `Engine::Initialize`
   opens or a Load on a running rig -- both go through the same call), and
   Frogg3rs's own `FroggersPluginProcessor::PumpStatePersistence` (a DAW
   host's `setStateInformation` restore), through `engine_.Manager()`, the
   accessor it already uses to build the state-snapshot patch; its bus
   ownership (patchInputBus/patchOutputBus never shared with `PatchManager`)
   is unchanged. A relaunch, a browser reload, a plugin session restore and
   a running Load all therefore open with the player's patch whole (QR-01,
   QR-04, FILE-07, PLG-10) on the very drain that pops the message -- a
   relaunch's own pre-audio drain at startup, or the first `ProcessBlock`
   that drains it while running -- with no retry needed. The engine's
   existing arena-exhausted retry (inline at startup; stashed for
   `MessageThreadTick` to grow while running) is unchanged and independent
   of this provisioning. The count of depths a patch needs is the one
   `app-o1-audit` already wrote (`MissingDepthsForValuesJSON`, carried from
   6ac80442 without the construct or the startup branch around it).
4. Carried from `app-o1-audit` as they stand, each toward a ratified step,
   with their spec deltas: the storage-batch race (2.1), output processors
   resending a declined update (2.2), the `Engine.hpp` false comments (2.3),
   library devices an app offers (3.1), a gesture reference names a gesture
   the app has (3.2), the Launchpad "Model" caption (3.3), the stale text
   (3.4), the per-tick app hook `HasMessageThreadTick` (3.5; the capture
   buffer needs it), a row field edit without the section rebuild (4.1),
   the hygiene commit (1.1–1.4), and the `app-operator-runs` change, left
   open. Each lands in the stacked PR whose diff introduced the concept it
   extends, else in the new branch (task list).
5. Dropped from `app-o1-audit`: 2.7 (`RunIfDepthStorageCovers` and every
   caller), 2.8 (`InputOrder`), 5.1 (the depth compute-skip, whose removal
   is the WIP the branch stopped in), 5.8 (the browser launch stall, out of
   scope by the handoff), and every 5.x task gated on a frogg3rs
   measurement that was not a finding.
6. `AppContext` exposes to the app's UI what the engine already publishes
   for the runtime pages: the clock diagnostics publication (`currentBpm`,
   and NEW `ClockDiagnostics::transportState`, which the master clock fills)
   as NEW `AppContext::clockDiagnostics`, and the requested sync
   configuration as NEW `AppContext::syncConfiguration`. The app's mirror
   atomics for tempo, external clock and transport-running go.

### Frogg3rs

1. NEW `FroggersCommand`, the app's own command numbers: page select, page
   previous, page next, encoder press, Randomize All, Randomize Page, Reset
   All, Reset Page. NEW `FroggersAppCore::ApplyAppCommand(command, value)`
   runs what `ProcessFrame`'s drain runs today for each, with the same
   bodies, the page or encoder position carried in the value, and sets a
   recompute flag that `ProcessFrame` answers with one
   `ComputeAllParameters` per block, as today. The page arrows resolve their
   target from the audio thread's own current page, and are no-ops in a
   view, so two arrow presses in one tick advance two pages (SUR-03); the
   surface no longer computes a page from the published display. No catalog lookup is involved: the encoder press has no
   catalog action (it is the catalog's `encoderPressAction`, dispatched to
   the surface with its position), and the six presses are the app's own
   commands at both ends. `ProcessFrame` keeps the routed-input value, the
   recompute, the neutral-depth release and the per-block work that is not
   a press.
2. `FroggersUiSurface::HandleAction` maps the eight action names to commands
   through one table and pushes `MessageIn::AppCommand(NowMicros(),
   command, value)` through the surface's existing `PushMessage`, exactly
   as it pushes `ParamIncDec` and `SetSceneBlend`. The BPM branch pushes
   `MessageIn::SetTempoBpmNormalized` with the slider value placed in the
   catalog range, keeping its externally-clocked guard. `Request*`,
   `pendingPageSelect_`, `pendingEncoderPress_`, `pendingRandomizeAll_`,
   `pendingRandomizePage_`, `pendingResetAll_`, `pendingResetPage_`,
   `pendingTempoBpmRequest_`, `tempoDisplayBpm_`, `tempoExternallyClocked_`
   and `transportRunningDisplay_` are deleted. `ArmRecording`, which
   refuses while the transport is stopped, reads the transport state from
   the context's clock diagnostics.
3. MIDI-originated presses keep Sheaf's route (`AppAction` or
   `AppEncoderPress` → message thread → `DispatchAction` → `HandleAction`)
   and so join the same bus at the same point as a click. The plugin's page
   restore calls `PortableSurface().DispatchAction` instead of
   `RequestPageSelect`.
4. Storage. `FroggersModulationSlate::Init` sets the group's low watermark
   to `kDepthParameterStorageCapacity`, the constant that already sizes the
   launch batch as the most depths one press can create (every parameter
   carrying a depth for every source). After any allocation on the audio
   thread, Sheaf's existing low-water request asks for the difference and
   the next message tick provisions it, so available storage is back above
   one press before the next tick's presses. The Load rule in Sheaf item 3
   keeps that true across a Load. The `partial` path in
   `RandomizeParameterModulationDepths` and the `LastRandomizePartial`
   accessor stay as they are: they are the observable the storage check
   asserts never fires across the reproduced player paths, at startup and
   running. Residual,
   stated: two Randomize All presses landing in one audio block after depths
   have grown past the launch storage could leave the second short, which
   no story step reaches (RND-02 is a second press, not a burst).
5. Carried from `frogg3rs-o1-audit` as they stand: the ratified story with
   its unratified mechanics lines removed (0eefd3f, task 2.0), Record re-arm
   race (5.1), Reset returns to the launch state through Sheaf's whole reset
   and takes what it resets out of every gesture (5.5, RST-01 and the
   gesture ruling), WRLD.Bldr out of the Preset selector (7.1), the manual
   sections (7.2, 7.4–7.6), gestures over MIDI (7.3), the Controllers-page
   row build (8.1), export without the copy and the worker encode (8.2,
   8.8), the capture buffer zeroed in parts (8.7), the false comments (4.x,
   rewritten again where this change makes them false), the citation gate
   that refuses line citations into Sheaf, the post-execution audit repairs
   to kept code (842c760, the `FroggersAppCore.hpp` and
   `FroggersDspParityTests.cpp` and `app/dsp/` hunks), each carried task's
   spec deltas, and the two operator-runs changes, left open. Dropped: 5.6
   and 5.7 as designed (replaced by items 1–4 above), 8.4 (gated, no
   finding), and the `evidence/` outputs. Carried work is stated by commit
   and behaviour; how its hunks apply against this tree is the executor's
   to resolve and report, not predicted here.
6. Comments. The `FroggersAppCore.hpp` and `FroggersUiSurface.hpp` header
   blocks that explain the bridge are replaced by the rule above, stated
   once, at `FroggersAppCore`; every other comment that names the bridge,
   the pending atomics or the drain order is rewritten (task 6.1 lists
   them).

## Tests: Sheaf's rig is the harness, and the guardrail

Every Frogg3rs test that drives a press drives it through
`synth_rig::SynthRig` (Sheaf, `tests/support/SynthRig.hpp`): presses through
`rig.Application().PortableSurface().DispatchAction(...)` and `RunBlocks`,
never through `Request*`, which no longer exists, so the compiler enforces
it. `ProcessFrame` stays public because the routed-input value stays in it,
and the two routed-input tests in `FroggersHeadlessTests.cpp` keep calling
it on a hand-built context; no other test does. Each behaviour this change
asserts gets one check, in the file that already covers that area, with the
break control the task names; no new test binary. The storage checks are
the audit's reproduced player paths, run as rig cases on both the startup
and the running patch path. Tests that
covered a removed mechanism are deleted with it, including every
press-queue, `InputOrder` and storage-wait test on the unpushed branches.
`FroggersDspParityTests.cpp` is DSP-level and rig-free by design; it
receives only carried comment and citation hunks.

## Decisions and their reasons

**An app-defined command, not a catalog index.** The catalog indexes what a
controller can be mapped to, and the encoder press is deliberately not in
it (Sheaf forwards it as its own kind). The six presses are produced and
consumed by the same app, so the number they travel under is the app's, and
Sheaf carries it without reading it. `AppAction` stays the message-thread
route for controllers; `AppCommand` is the audio-thread route the surface
emits after it has decided.

**No hold, no second queue.** `frogg3rs-o1-audit` held a press until storage
covered it and put everything behind it in a queue of its own. The audit
showed that holding the head of Sheaf's bus would hold the Start, Stop and
Clock messages pushed behind it (they are lifted into the realtime batch
only when popped), breaking TRN-03/04/08/10/12 and PLG-05/06/08/09 whenever
a hold occurred. The operator ruled that "a press waits at most one message
tick" is not ratified and that Sheaf's queueing is not to be replaced with
bespoke machinery. So a command is applied in the block that pops it, and
storage is kept ahead of the presses instead of presses waiting for
storage. Two Randomize presses in one drain run twice; a Reset after a
Randomize runs after it in the same block; the operator ruled the 5 ms
intermediate state inaudible and separate blocks unnecessary.

**One constant, no per-press bound.** The audit showed the planned
`RandomizeNewDepthBound` at `Init` cannot work (it reads live, connected
state that `Init` does not have, and needs a Sheaf count that exists only on
the dropped branch). `kDepthParameterStorageCapacity` already states the
most depths a press can create and already sizes the launch batch; it
becomes the watermark too, so the launch batch and the watermark cannot
drift apart. The cost is memory only: storage is provisioned on the message
thread and never freed.

**Zero-then-release-before-draw is not built.** The audit showed the release
frees none of the depths the same press zeroed until a recompute has run
(`CanRecycleLocal` reads computed centres), and that even an emulated
working version still drew short on the grown state. The existing order
stays: the presses apply (after this change, in the drain before
`ProcessFrame`), then `ProcessFrame` releases, then recomputes once. The
release reads centres the recompute after it has not yet updated, so it
frees none of the depths that frame's presses zeroed; a later frame's
release frees them. The carried story's description of the defect is
corrected to say that (task 2.0).

**Page arrows resolve on the audio thread.** The surface computed the arrow
target from the published page, which is stale inside a tick, so two arrow
presses in one tick advanced one page on main. As commands the audio thread
resolves from its own current page, they keep their count like every other
press, and the in-a-view gate moves with them.

**Tempo is not a `Parameter`.** Sheaf's model for "a control on the bus
that is not modulated" is the catalog's analog app action, which the
shift-and-crispy change already made the tempo (`catalog.tempoAction`,
`analogRange`). The slider joins that route with one message; making the
tempo a `Parameter` object would give it scenes, gestures, depth slots and a
bank position it must then refuse, which is more structure to exclude than
to reuse.

**Freeze and Record stay values.** Both presses are resolved on the message
thread (the surface computes the toggle, and Record can refuse and must say
why on screen at once), and what crosses is the resulting latch or arm
state. That is the value class by definition. The surface's existing
ordering of a latch write before a transport push stays as it is.

**The story ships only what was ratified.** The carried story's paragraph on
storage keeps the defect description and the operator's words, and loses the
coordinator's applications (the one-tick wait, the block-per-press, the
bypass list), which the operator has since ruled unratified; its section 4
depths cell and its two `pendingPageSelect_` code pointers are rewritten to
the mechanism as built. Player-facing lines are untouched.

## Impact (directories the hygiene sweep covers)

- `app/` (FroggersAppCore.hpp, FroggersUiSurface.hpp, FroggersModulation.hpp,
  Froggers.hpp, FroggersMidiCatalog.hpp, `app/dsp/Limiter.hpp`, the test
  TUs, Makefile, the check scripts)
- `app/vst/` (FroggersPluginProcessor.cpp, FroggersPluginProcessor.hpp,
  FroggersVstHostTests.cpp)
- `openspec/` (this change; `openspec/story.md`; the operator-runs change;
  specs froggers-sheaf-runtime-app, froggers-modulation-slate,
  froggers-transport-and-reset-controls, and the carried deltas for
  froggers-sheaf-parameter-model, froggers-vco-topology, froggers-vst-host)
- `MANUAL.md`, `README.md` (carried manual sections)
- `External/Sheaf`: `projects/synth/include/synth/` (ParameterModulation.hpp,
  Engine.hpp, AppConcepts.hpp, AppContext.hpp, MasterClock.hpp,
  MidiConfigBlocks.hpp, MidiConfigViewModel.hpp, MidiController.hpp,
  PatchPersistence.hpp), `projects/synth/src/`, `projects/synth/tests/`,
  `openspec/` (this change, the carried `app-o1-audit` deltas, and
  `app-operator-runs`)

## Delivery

Both halves start from the shared bases (origin/main 2255303, Sheaf
62829a4a) and carry the kept work by cherry-pick from the `o1-audit` and
`app-o1-audit` branches, hunk by hunk as the task list names them.

Sheaf is built first and pushed last-but-one. The fork's open PRs against
upstream main (#9, 11, 12, 13, 14, 15, 17, 18, 19, 20) are one linear stack;
each Sheaf piece lands in the PR whose diff introduced the concept it
extends, or in the new branch when its code depends on anything only
higher in the stack or extends upstream code no PR introduced; the executor
decides each placement from the tree and reports it. The branches above an
amendment are rebased and force-pushed, and what has no owning PR goes up
as `app-commands-on-the-bus`, the next sequential PR on the new tip. The
frogg3rs tasks build and test against that local tip; only when the full
frogg3rs suite is green is the Sheaf stack pushed and the PR opened, then
the frogg3rs pin moved and frogg3rs pushed to main. `fork/app-midi-out` is
the scrapped MIDI-out record and stays on its old base, unpushed. Once
frogg3rs is on GitHub main nothing stays on a local branch: this change's
worktree is removed, the local `o1-audit`, `app-o1-audit` and this change's
branches deleted (fork branches that head open PRs stay), and the primary
checkout's `main` fast-forwarded to `origin/main` with `External/Sheaf` at
the pin main records (operator instruction, 2026-09-24). Nothing the
shipped tree cites points at a commit or path that exists only on a
deleted local branch. Full `make test` and the CI host suites before every
push.
