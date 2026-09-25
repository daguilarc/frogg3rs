# Proposal — `frogg3rs-presses-on-the-bus`

Paired with the Sheaf change `app-commands-on-the-bus` (External/Sheaf;
its proposal is what the upstream reviewer reads, and every task for both
trees is in this change's `tasks.md`). This change supersedes the unpushed
`frogg3rs-o1-audit` / `app-o1-audit` pair: it keeps the bug fixes those
changes made toward ratified story steps and replaces their press-ordering
and storage machinery with Sheaf's own message stream. It is written from
origin/main 2255303 and Sheaf 62829a4a, the bases both unpushed branches
share. The ratified story, NEW `openspec/story.md`, is carried to main by
this change (task 2.0); its step IDs are the ones cited below.

## Why

Every control on the Frogg3rs surface already reaches the audio thread on
Sheaf's ordered UI bus except seven: Randomize All, Randomize Page, Reset
All, Reset Page, page select, encoder press and the BPM slider. Those seven
call `FroggersAppCore::Request*`, which writes a single-slot atomic that
`ProcessFrame` drains once per block in a fixed order. The header comment
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
tick collapse into one (MOD-09's two Back presses), and presses of different
kinds reorder (a Reset pressed after a Randomize can run before it). The
story steps the player hits: RND-01, RND-02, RND-05, RST-01, RST-04, MOD-09,
TRN-02, BPM-01, SYN-03 to SYN-05, PLG-06. `frogg3rs-o1-audit` answered this
with a second queue beside Sheaf's (`FroggersPressQueue`, a 64-slot ring with
a producer mutex), a shared sequence number between that queue and Sheaf's
patch bus (`InputOrder`, `HasOrderedPresses`), and a Sheaf construct that
holds a press until depth storage covers it (`RunIfDepthStorageCovers`).
None of that is needed once presses travel on the bus that already orders
everything else.

The stories are canonical and do not change. Frogg3rs's Reset and Randomize
keep their own semantics (one-shot presses that redraw or restore, as 1.8
states); they are not Sheaf's held Reset and Random modifiers, and this
change does not route them there. What changes is the transport that
carries the press, never what the press does.

## The rule this change installs

What crosses from the message thread to the audio thread is either a
**command** or a **value**. A command is counted and ordered: it travels as a
`MessageIn` on `AppContext::uiBus` and is applied on the audio thread in the
order pushed, by the same drain that applies encoder turns and scene blend.
(Start, Continue, Stop and Clock are realtime messages the engine lifts out
of both buses and applies after both have drained, as today; nothing here
changes that.) A value is last-writer-wins and unordered against commands:
one atomic the message thread writes and the audio thread reads. A press
whose effect the message thread computes, and that crosses as the resulting
value, is a value: the Freeze latch, the Record arm and the desired
transport state are that today and stay so, with their declarations saying
which class they are. Every cross-thread member of `FroggersAppCore` is
declared as one or the other. After this change the command class holds the
six presses, and the value class holds the host's routed-input signal, the
Freeze latch, the Record arm, the desired transport state and the drill
level published for display.

## What changes

### Sheaf (`app-commands-on-the-bus`)

1. NEW `MessageIn::Type::AppCommand`, appended last so every enumerator keeps
   its ordinal. It carries an app-defined command number and a float value,
   and Sheaf never interprets either: the app pushes it from its surface and
   applies it in its own hook. `Engine::DrainMessageBus` hands it to
   `app_.ApplyAppCommand(command, value)` when the app declares the concept
   NEW `HasAppCommands`; for an app without the hook the bus's own `Apply`
   drops it, since nothing but that app's own surface produces one.
2. A hold. `ApplyAppCommand` returns `bool`: true means applied; false means
   the command could not run yet, the message stays at the head of the bus
   (NEW `MessageInBus::Peek`, the same head read `Pop` does without the
   advance), and the drain of that bus ends for this block. Everything
   behind it waits in Sheaf's ring, in order, and the next block tries
   again. This replaces `RunIfDepthStorageCovers`, `InputOrder` and
   `HasOrderedPresses`.
3. The depth-storage low watermark becomes a `ParameterGroup` setting
   (NEW `ParameterGroup::SetStorageLowWatermark`), read by the existing
   `RequestParameterStorageBatchIfLow` at its existing call sites and as the
   request-size floor. The default stays what it is today
   (`numModulators * 2`).
4. A patch Load whose depths exceed free storage requests the shortfall and
   retries on a later block, through the stash-and-retry `Engine` already
   uses for an exhausted serialization arena (`pendingPatchMessage_`,
   `arenaGrowPending_`), generalized to a pending storage batch. The running
   patch is untouched until the Load applies whole. This keeps
   `app-o1-audit` 2.6's requirement and drops its construct.
5. Carried from `app-o1-audit` as they stand, each toward a ratified step:
   the storage-batch race (2.1), output processors resending a declined
   update (2.2), the per-tick app hook `HasMessageThreadTick` (3.5; the
   capture buffer needs it), library devices an app offers (3.1), a gesture
   reference names a gesture the app has (3.2), the Launchpad "Model" caption
   (3.3), the stale text (3.4), a row field edit without the section rebuild
   (4.1), the `Engine.hpp` false comments (2.3), and the hygiene commit
   (1.1–1.4). Each lands in the stacked PR that owns its seam (task list).
6. Dropped from `app-o1-audit`: 2.7 (`RunIfDepthStorageCovers`), 2.8
   (`InputOrder`), 5.1 (the depth compute-skip, whose removal is the WIP the
   branch stopped in), 5.8 (the browser launch stall, out of scope by the
   handoff), and every 5.x task gated on a frogg3rs measurement that was not
   a finding.
7. `AppContext` exposes to the app's UI what the engine already publishes
   for the runtime pages: the clock diagnostics publication (`currentBpm`,
   and NEW `ClockDiagnostics::transportState`, which the master clock fills)
   as NEW `AppContext::clockDiagnostics`, and the requested sync
   configuration as NEW `AppContext::syncConfiguration`. The app's mirror
   atomics for tempo, external clock and transport-running go.

### Frogg3rs

1. NEW `FroggersCommand`, the app's own command numbers: page select, encoder
   press, Randomize All, Randomize Page, Reset All, Reset Page. NEW
   `FroggersAppCore::ApplyAppCommand(command, value)` runs what
   `ProcessFrame`'s drain runs today for each, with the same bodies, the
   page or encoder position carried in the value. No catalog lookup is
   involved: the encoder press has no catalog action (it is the catalog's
   `encoderPressAction`, dispatched to the surface with its position), and
   the six presses are the app's own commands at both ends. `ProcessFrame`
   keeps the routed-input value and the per-block work that is not a press.
2. `FroggersUiSurface::HandleAction` pushes
   `MessageIn::AppCommand(NowMicros(), command, value)` for those six actions
   through the surface's existing `PushMessage`, exactly as it pushes
   `ParamIncDec` and `SetSceneBlend`. The BPM branch pushes
   `MessageIn::SetTempoBpmNormalized` with the slider value placed in the
   catalog range, keeping its externally-clocked guard. `Request*`,
   `pendingPageSelect_`, `pendingEncoderPress_`, `pendingRandomizeAll_`,
   `pendingRandomizePage_`, `pendingResetAll_`, `pendingResetPage_`,
   `pendingTempoBpmRequest_`, `tempoDisplayBpm_`, `tempoExternallyClocked_`
   and `transportRunningDisplay_` are deleted. `DrillLevel` stays: it is
   app state published for display, Sheaf's `UIState` idiom. `ArmRecording`,
   which refuses while the transport is stopped, reads the transport state
   from the context's clock diagnostics.
3. MIDI-originated presses keep Sheaf's route (`AppAction` or
   `AppEncoderPress` → message thread → `DispatchAction` → `HandleAction`)
   and so join the same bus at the same point as a click. The plugin's page
   restore calls `PortableSurface().DispatchAction` instead of
   `RequestPageSelect`.
4. Storage. A Randomize press zeroes the depths it will redraw as a pass of
   its own, before any draw, and releases the zeroed slots once
   (`CollectNeutralLocalParameters`, today called after the whole press), so
   the draw that follows needs at most the slots its new sources add.
   Frogg3rs sets each group's low watermark to the most depths one press can
   add at the largest view (the existing `RandomizeNewDepthBound` from
   `frogg3rs-o1-audit` computes this), and sizes the launch batch to cover
   it, so the audio thread's own low-water request keeps available storage
   above one press at steady state with no message-thread top-up. When a
   press still finds available storage below what it needs (a Load or view
   open in the same tick consumed it), `ApplyAppCommand` requests the
   shortfall and returns false: the press waits at the head of the bus,
   unchanged, re-asks on every block until covered, and runs in the block
   after the storage arrives. The encoder press uses Sheaf's own
   `Bank::CanOpenModulationView` as its predicate. The `partial` path in
   `RandomizeParameterModulationDepths` becomes an assertion, and the
   `LastRandomizePartial` plumbing that reported it goes with it.
5. Carried from `frogg3rs-o1-audit` as they stand: the ratified story and
   the comment hygiene (0eefd3f), Record re-arm race (5.1), Reset returns to
   the launch state through Sheaf's whole reset and takes what it resets
   out of every gesture (5.5, RST-01 and the gesture ruling), WRLD.Bldr out
   of the Preset selector (7.1), the manual sections (7.2, 7.4–7.6),
   gestures over MIDI (7.3), the Controllers-page row build (8.1), export
   without the copy and the worker encode (8.2, 8.8), the capture buffer
   zeroed in parts (8.7), the false comments (4.x, rewritten again where
   this change makes them false), the citation gate that refuses line
   citations into Sheaf, the post-execution audit repairs to kept code
   (842c760), and the two operator-runs changes, left open. Dropped: 5.6
   and 5.7 as designed (replaced by items 1–4 above), 8.4 (gated, no
   finding), and the `evidence/` outputs.
6. Comments. The `FroggersAppCore.hpp` and `FroggersUiSurface.hpp` header
   blocks that explain the bridge are replaced by the rule above, stated
   once, at `FroggersAppCore`; every other comment that names the bridge,
   the pending atomics, the drain order or the release timing is rewritten
   (task 6.1 lists them).

## Tests: Sheaf's rig is the harness, and the guardrail

Every Frogg3rs test that drives a press drives it through
`synth_rig::SynthRig` (Sheaf, `tests/support/SynthRig.hpp`): presses through
`rig.Application().PortableSurface().DispatchAction(...)` and `RunBlocks`,
never through `Request*`, which no longer exists, so the compiler enforces
it. `ProcessFrame` stays public because the routed-input value stays in it,
and the three routed-input tests in `FroggersHeadlessTests.cpp` keep calling
it on a hand-built context; no other test does. Each behaviour this change
asserts gets one check, in the file that already covers that area, with the
break control the task names; no new test binary. Tests that covered a
removed mechanism are deleted with it, including every press-queue and
`InputOrder` test on the unpushed branches and the test that asserted
`LastRandomizePartial`. `FroggersDspParityTests.cpp` is DSP-level and
rig-free by design; it receives only carried comment and citation hunks.

## Decisions and their reasons

**An app-defined command, not a catalog index.** The catalog indexes what a
controller can be mapped to, and the encoder press is deliberately not in
it (Sheaf forwards it as its own kind). The six presses are produced and
consumed by the same app, so the number they travel under is the app's, and
Sheaf carries it without reading it. `AppAction` stays the message-thread
route for controllers; `AppCommand` is the audio-thread route the surface
emits after it has decided.

**Hold at the head of Sheaf's bus, not a second queue.** `Pop` is already
timestamp-gated: a message the audio thread cannot take yet stays at the
head. A `Peek` is that read without the advance. Everything behind a held
press waits in the same ring, so order is kept with no app-side storage, no
sequence stamping, and no ordering hook. A knob turn pushed behind a
waiting Randomize waits with it, at most a message tick or two past the
launch storage; `frogg3rs-o1-audit` gave turns a bypass and this change
does not, because the bypass was the coordinator's own application,
recorded "open to the operator's objection", and the operator's stated
frame is that a transient of milliseconds is immaterial and a draw that
comes up short is the bug. Start, Stop and Clock are realtime messages the
engine batches out of both buses and applies after the drains, so a
transport press is never held behind a waiting command.

**Zero first, then release, then draw; watermark at one press.** The partial
draw happens because today each parameter's old depths are zeroed inside
its draw and the zeroed slots are released only after the whole press.
Zeroing every target first and releasing once means the draw needs at most
the new sources it adds. With the watermark at that bound and the launch
batch covering it, the existing audio-thread low-water request keeps
available storage above one press whenever anything allocates, with no
per-tick top-up. The hold covers the case the watermark cannot, a consumer
in the same tick. Sheaf's `RequestParameterStorageBatch` sends nothing
while a request is already pending, so a held press re-asks every block and
runs within two message ticks of the shortfall at most.

**No collapse of adjacent Randomizes, no block-to-itself for Reset.** Both
were `frogg3rs-o1-audit` applications of rulings given in reply to that
change's own queue design, which presented "rounds" and "batching" as things
that existed. On the bus there is neither: two Randomize All presses in one
drain run twice and the second draw is what the player hears; a Reset after a
Randomize runs after it, in the same block. The story is canonical and
already answers this (RND-02: press again, the new draw replaces the old),
and no step in 1.8 names a collapse or a block boundary. Both need look-ahead
into the queue to implement and serve no step, so neither is built.

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
ordering of a latch write before a transport push stays as it is, and the
happens-before it relies on is unchanged by this change.

## Impact (directories the hygiene sweep covers)

- `app/` (FroggersAppCore.hpp, FroggersUiSurface.hpp, FroggersModulation.hpp,
  Froggers.hpp, FroggersMidiCatalog.hpp, `app/dsp/Limiter.hpp`, the test
  TUs, Makefile, the check scripts)
- `app/vst/` (FroggersPluginProcessor.cpp, FroggersPluginProcessor.hpp,
  FroggersVstHostTests.cpp)
- `openspec/` (this change; `openspec/story.md`; the operator-runs change;
  specs froggers-sheaf-runtime-app, froggers-modulation-slate,
  froggers-transport-and-reset-controls)
- `MANUAL.md`, `README.md` (carried manual sections)
- `External/Sheaf`: `projects/synth/include/synth/` (ParameterModulation.hpp,
  Engine.hpp, AppConcepts.hpp, AppContext.hpp, MasterClock.hpp,
  MidiConfigBlocks.hpp, MidiConfigViewModel.hpp, MidiController.hpp,
  PatchPersistence.hpp), `projects/synth/src/`, `projects/synth/tests/`,
  `openspec/`

## Delivery

Both halves start from the shared bases (origin/main 2255303, Sheaf
62829a4a) and carry the kept work by cherry-pick from the `o1-audit` and
`app-o1-audit` branches, hunk by hunk as the task list names them.

Sheaf first. The fork's open PRs are one linear stack, so each Sheaf piece
lands in the PR that owns its seam (#9, #13, #14, #15, #19 by the task
list), the branches above are rebased and force-pushed, and what has no
owning PR goes up as `app-commands-on-the-bus`, the next sequential PR on
the new tip; the frogg3rs pin moves to that tip. Then frogg3rs, pushed to
main. Once it is on GitHub main nothing stays on a branch: this change's
worktree and the `o1-audit` worktree are removed, their frogg3rs and Sheaf
branches deleted, and the primary checkout's `main` fast-forwarded to
`origin/main` with `External/Sheaf` at the pin main records (operator
instruction, 2026-09-24). Full `make test` and the CI host suites before
every push.
