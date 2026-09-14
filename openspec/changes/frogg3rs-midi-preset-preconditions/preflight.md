# Preflight audit — `frogg3rs-midi-controller-resilience`

Run against `/Users/diegoaguilar-canabal/Desktop/frogg3rs` at working-tree state
of 2026-09-10 (submodule `External/Sheaf` at `ba3898e4`, branch
`launchpad-model-on-the-row`). Read-only; no builds run.

## VERDICT

**REJECT.**

The diagnosis is sound and the artifacts are unusually well written, but the
change cannot be executed as it stands. Its central behaviour — a held modifier
that clears without a release — is the direct negation of a SHALL requirement
that an unarchived Sheaf change already owns (`shift-and-file-export`, smi-16:
"A Shift press whose release never arrives SHALL leave the profile shifted until
the next Shift press and release"), and no artifact names that change, that
capability, or that requirement. §9's "enumerate the other active changes first"
was not run: five active changes overlap this one, four of them in flight right
now, including `app-midi-catalog` (26/28) which owns both `MidiAppDeviceDefault`
— the type task 5.1 edits — and the Controllers page status dot this change
declares BREAKING. The Impact list omits every definition site the work actually
lands on (`MidiAppCatalog.hpp`, `Engine.hpp`, `MidiReconcile.hpp`, and the two
independent `MidiEndpointOps` binding sites), which also means §8.0's sweep is
scoped to the wrong tree; and the one file Impact *does* name for clearing state
on reopen, `MidiReconcile.cpp`, is a JUCE-free planner with no engine handle and
no path to `ShiftState`. Finally the change's own preflight task 1.4 asks the
operator to reproduce a device fault through a utility the same proposal says is
not installed on that machine. The citation work is otherwise accurate — see
below — so this is a scoping and sequencing rejection, not a tracing one.

---

## Citation audit

Every `file:line` in the four artifacts, verified by reading. `✓` = says what
the artifact claims. Working-tree line numbers; `MANUAL.md` is dirty in the
working tree but the dirt begins at line 521, below every cited line, and the
cited lines are identical at `HEAD` (checked with `git show HEAD:MANUAL.md`).

| Citation | Artifact | Verdict | What the file actually says |
|---|---|---|---|
| `External/Sheaf/projects/synth/include/synth/MidiController.hpp:256` | proposal Impact, design Context | ✓ | `struct ShiftState {` — `bool held = false;` at :257, closing `};` at :258. Doc comment at :254-255 states "set by a Shift button's press, cleared by its release". |
| `External/Sheaf/projects/synth/src/MidiController.cpp:969` | proposal Impact, design Context | ✓ | `shift_->held = isPress;` — the sole writer; guarded by `if (shift_ != nullptr)` at :968, inside `if (association->press.type == MessageIn::Type::Shift)` at :964. `grep -n "shift_->held"` returns exactly :969 (write) and :975 (read). |
| `External/Sheaf/projects/synth/src/MidiController.cpp:975` | design Context | ✓ | `const bool shifted = shift_ != nullptr && shift_->held && association->shiftedPress.has_value();` inside `if (isPress)` at :974; the push is at :976. |
| `External/Sheaf/projects/synth/src/MidiController.cpp:3028` | design Context, task 1.3, task 3.2 | ✓ | `result.shift = std::make_unique<ShiftState>();` in `CreateMidiControllerProfileImpl` (opens :3019). `result.holdDrill = std::make_unique<HoldDrillState>();` is the line before, at :3026. |
| `External/Sheaf/projects/synth/src/MidiController.cpp:957-961` | task 1.2 | **STALE** | The cited range is `holdDrill_->drilled.clear();` (:957) through `return;` (:961). The write the claim is about — `holdDrill_->held = true;` — is at **:956**, one line above the range, and `if (isPress)` opens at :955. The correct range is **:955-961**. `grep -n "holdDrill_->held"` returns :712 (read), :956 (write true), :959 (write false); the task's "single-writer" framing is right, the range is off by one at the top. |
| `app/FroggersMidiCatalog.hpp:14-24` | proposal Why + Impact, design Context, task 5.2 | ✓ | The Twister precondition comment block, exactly: relative encoders "Enc 3FH/41H" (:15), "all six side buttons set to CC Hold" (:16), "Bank Side Buttons unchecked" (:18), and at :23-24 "CC Hold is required on every side button because the release is what ends Shift". File is clean in git. |
| `MANUAL.md:319-320` | proposal Why + Impact, task 7.1 | ✓ | ":319 …If the controller is unplugged while Shift is still held, its / :320 buttons stay shifted until Shift is pressed and released again." The artifact's paraphrase is fair. |
| `MANUAL.md:336-339` | proposal Why + Impact | ✓ | The Midi Fighter Utility paragraph, naming all three settings. Note :338 phrases the address as "CC 8 to 13 on channel 4 (channel 3 counted from 0)" — the opposite indexing convention from the delta spec's "channel 3 (channel 4 counted from 1)". See finding S3. |
| `app/vst/FroggersPluginProcessor.cpp:312` | design Decisions | ✓ **but materially incomplete** | `startTimerHz(30);` — correct. But it sits inside `if (juce::MessageManager::getInstanceWithoutCreating() != nullptr)` at :311, so the timer does not start in any headless binary, and this is the **VST only**. See finding S8. |
| `External/Sheaf/projects/synth/include/synth/ControllersPageUI.hpp` | proposal Impact | ✓ (file exists) | Present. `EndpointStatusColor` at :791 and `EmitStatusDot` at :808 are the status-dot sites; the exhaustive switch is :795-799 and a parallel test table at :3347-3352. |
| `External/Sheaf/projects/synth/src/MidiReconcile.cpp` | proposal Impact, design Context, task 3.3 | **WRONG FILE for the stated job** | The file exists and does track endpoint identity and online/offline. It cannot "clear modifier state when an endpoint reopens": `PlanMidiReconciliation`/`ExecuteReconcilePlan` are documented as a "Pure, JUCE-free reconciliation planner" (`MidiReconcile.hpp:55`), reach devices only through the `MidiEndpointOps` callback struct (`MidiReconcile.hpp:114-120`), and have no `Engine` handle. See B4. |
| `MidiAppDeviceDefault` (task 5.1) | task 5.1 | **uncited, and outside Impact** | The type is at `External/Sheaf/projects/synth/include/synth/MidiAppCatalog.hpp:31-38` — a Sheaf header the Impact section never names. Task 5.1 reads as if it were an `app/` edit. |
| "the same comment block" (task 5.3) | task 5.3 | **STALE** | The APC40 Track 1 caveat is a *different* block, `app/FroggersMidiCatalog.hpp:26-35`, not the Twister block at :14-24. |
| "`shift-and-file-export`" | — | **ABSENT** | Not cited anywhere, though it owns `ShiftState` and smi-16. See B1. |

Citations not otherwise flagged were spot-checked and hold. `openspec validate
frogg3rs-midi-controller-resilience --strict` and `... frogg3rs-midi-shift
--strict` both report valid.

---

## Unverified behavioural claims

§9: *"A proposal states only what has been read. Reject one carrying a claim
about behaviour nobody has verified, however honestly hedged."*

### 1. That a profile rebuild clears the modifier in practice

> design.md: "`result.shift = std::make_unique<ShiftState>()` at :3028 means a
> profile rebuild constructs a fresh `ShiftState`, so rebuild-clears is close to
> free."

**Status: structurally true, behaviourally under-traced — but the plan's
conclusion survives.** I traced it forward rather than leaving it: the sole
production caller of `CreateMidiControllerProfile` is
`Engine.hpp:990`, inside `RebuildMidiProcessors()` (:967-996), which rebuilds
**every** slot and assigns `midiProcessors_ = std::move(rebuilt)` at :995 —
documented at :928 as "the single call site for the `midiProcessors_`
assignment, so it covers every rebuild". The raw `ShiftState*` handed to
`SystemButtonMidiInProcessor` (`MidiController.cpp:3029`) points into a
`unique_ptr` member (`MidiController.hpp:962`), so the move into the vector
leaves the pointee address stable. A rebuild therefore does construct a fresh,
cleared `ShiftState`, and the old one is destroyed with the old chain.

Two consequences the artifacts do not state and should:

- A rebuild triggered by editing **any** slot clears **every** slot's modifier,
  because `RebuildMidiProcessors()` rebuilds all of them. That is a
  behavioural side effect of the trigger the change is adopting.
- `Engine.hpp:936` says outright: a caller "would need to … otherwise handle the
  endpoint-reopen consequences of a fresh chain, **but there is no such caller
  currently**." The design's "What does not exist today is a rebuild on endpoint
  reopen" is correct and well stated; task 1.3 correctly schedules the
  enumeration. Credit where due — this is the one behavioural premise the change
  handles the way §9 asks.

**Cheapest run that would settle the remainder:** an existing-harness test —
`SynthRig::InstallInstrumentForTest` / `RebuildMidiProcessorsForTest()`
(`Engine.hpp:1001`) already exists — set `shift->held = true` on slot 1,
rebuild, assert slot 1's modifier is clear. No device, no build of the app. This
is task 3.2 and it is well specified; the positive control in 3.6 is the right
shape.

### 2. That `MessageThreadTick` is pumped at 30 Hz on every host

> design.md: "The engine already pumps output processors from
> `MessageThreadTick` at 30 Hz (`app/vst/FroggersPluginProcessor.cpp:312`),
> which is a sufficient granularity for a ceiling measured in seconds and adds
> no new thread."

**Status: one host cited, three hosts ship, and they differ.** Traced:

| Host | Pump | Rate | Notes |
|---|---|---|---|
| VST | `FroggersPluginProcessor.cpp:748` from a `juce::Timer` | `startTimerHz(30)` at :312 | **Guarded** by `MessageManager::getInstanceWithoutCreating() != nullptr` (:311). No timer in any headless CTest binary; those drive `timerCallback()` by hand via `PumpMessageThreadForTest()`. |
| Standalone / miniapp | `Runtime.hpp:335` | `startTimerHz(config.uiFrameHz > 0 ? … : 30)`; frogg3rs sets `config.uiFrameHz = 30` at `app/FroggersAppCore.hpp:251` | 30 Hz, but via a *configurable* field the design never mentions. |
| Browser (wasm) | `BrowserRuntime.hpp:722` inside `MessageTick`, driven from JS | `setInterval(…, 1000/30)` at `browser/src/main.ts:249`, default at :406 | **Not reliably 30 Hz.** `requestFrame()` (:343-357) coalesces while a frame is in flight, each tick is a worker round-trip, and browsers throttle `setInterval` in a backgrounded tab to ~1 Hz. |

The *rate* claim being loose does not break the 30-second ceiling — 1 Hz is
ample granularity for it. What the design does not establish, and must, is that
a pump **exists on each host**, and that the ceiling's positive control (task
3.6) can actually fire where it will be written. The design also never cites the
pump itself: the loop is `Engine.hpp:570-574`, not
`FroggersPluginProcessor.cpp:312`, which is only where the tick's *rate* is set.

**Cheapest run that would settle it:** none needed for the VST or standalone —
this was settled by reading, which is why it is reported here rather than
labelled. For the browser, the deciding quantity is ticks-per-second observed by
the wasm side over 60 s with the tab backgrounded; one `performance.now()` delta
counter in `MessageTick` answers it. Cheaper still: state the ceiling as
"evaluated on the next tick after the deadline, whenever that arrives" and the
question stops being load-bearing.

Additionally, and not stated anywhere: **the VST has no
`MidiConnectionManager`** (`grep -rn "MidiConnectionManager" app/` returns
nothing). The VST receives MIDI through the host's `MidiBuffer`, never opens an
endpoint, and never reconciles. Of the four clear triggers the design calls
"independent", the endpoint-reopen one **does not exist in the VST at all**. The
design's claim that "taken together they close every path found" is therefore
host-dependent in a way no artifact records.

### 3. That a second press with no intervening release is distinguishable

> proposal: "A second press of the same address clears it as well, so a device
> that sends press-only edges can still turn the modifier off."

**Status: distinguishable — verified — but the mechanism does not reach the
captured incident.** `isPress` is computed at `MidiController.cpp:945-946` as
`midi.IsCC() ? midi.GetValue() > 0 : midi.Status() == kStatusNote &&
midi.GetValue() > 0`. At :964-970 the Shift branch has `shift_->held` in hand
before overwriting it, so `shift_->held && isPress` is exactly "a second press
with no intervening release". One bool suffices; no new state is needed for the
discrimination itself. The premise holds.

What does not hold is its usefulness against the incident this change responds
to. The capture says **CC 13 transmitted nothing** — not press-only edges,
nothing. A message that never arrives never reaches `FindAssociation`, so the
second-press path is inert in the failure that motivated the change. The
press-only addresses that *did* arrive, CC 0 and CC 1, are unmapped, so they
exit at :940-943 (`PassToThru`) and never reach the Shift branch either. The
design's "Any single trigger leaves a hole … taken together they close every
path found" is defensible, but the proposal's framing invites the reader to
think the second-press clear addresses the captured evidence. It does not. Say
so.

**Cheapest run that would settle it:** already settled by reading; no run
required.

---

## Forward enumeration (§5, run forward on every named concept)

Case-insensitive, by operand, across `app/`, `src/`, the Sheaf synth source
(`include/ src/ runtime/ juce/ apps/`), `openspec/{specs,changes}`, and
`External/Sheaf/openspec/{specs,changes}`. `node_modules/`, `dist/`, `build/`,
`e2e/`, `vendor/`, `quest-runner/` and `analysis/` excluded as vendored or
generated. CHANGED is 0 for every row: this is a preflight, nothing is
implemented.

| Concept the change creates | Operands grepped | FOUND (domain sense) | FOUND (word, any sense) | CHANGED | Disposition |
|---|---|---|---|---|---|
| declared-preconditions field on `MidiAppDeviceDefault` | `precondition`, `MidiAppDeviceDefault`, `deviceDefaults` | **0** | 21 files, all doc-comment "Precondition:" (`MidiReconcile.hpp:60`, `MasterClock.hpp:193/196`, `RuntimeMainComponent.hpp:334`, `MidiDevicePoller.cpp:137`) | 0 | No data-shaped precondition concept exists. **But** `MidiAppDeviceDefault` (`MidiAppCatalog.hpp:31-38`) is owned by Sheaf change `app-midi-catalog` at 26/28 tasks — see B2. Prose collision only; the word is safe. |
| mismatch state on a slot | `mismatch`, `unmapped`, `EndpointStatus` | **0** | 96 files; the MIDI-adjacent ones are `MidiReconcile.hpp:62` (a size mismatch in a precondition note), `ControllerWizard.hpp:111` (a form-type error string), and `synth-runtime-ui` spec:1018 ("Version mismatch fails loudly"). `synth-midi-instrument` spec:285 has "Unmapped active controller still supplies clock" — a *different* sense (a slot with no mappings, not a device sending unmapped addresses). | 0 | No existing concept to reuse. Two prose collisions worth renaming around: "unmapped" already means "has no mappings" in `synth-midi-instrument`. The *state* has a structural problem — see B7. |
| observed-address set | `observedaddress`, `observed address`, `observed` | **0** | `observed` appears 11× in unrelated senses (`MasterClock` sample/timestamp observation, `Engine`'s CAS comments) | 0 | Genuinely new. Nothing in the input chain records inbound addresses today; `MidiInProcessor::Process` dispatches and drops. No duplication scheduled. |
| hold ceiling constant | `holdceiling`, `hold ceiling`, `ceiling` | **0** | `app/dsp/Drive.hpp:696` ("Decay/Hold ceiling" — a reverb gain limit); `Runtime.hpp:551`, `RuntimeShellSessionTests.cpp:432/452` (channel-count ceilings) | 0 | Genuinely new. Name it away from the DSP sense (`kShiftHoldCeilingMicros`, not `kHoldCeiling`). |
| clear-source enum | `clearsource`, `clearedby`, `ClearReason` | **0** | 0 in source; 1 TypeScript vendor hit | 0 | Genuinely new. |
| token bucket | `tokenbucket`, `token bucket`, `bucket`, `ratelimit`, `throttl` | **0** | `bucket` 10× in `DspSpectral.hpp`/`ParameterModulation.cpp` (FFT bins, switch quantization). `throttle` 7× in `Engine.hpp` — the **UI-state publish throttle** (`uiPublishInterval_`, :285/:1480), a genuinely adjacent rate-limiting mechanism. | 0 | Not a duplicate: `uiPublishInterval_` throttles *outbound* UI publication by audio-block count; the token bucket throttles *inbound* per-controller SysEx by wall time. Different operands, different sink. Classified and kept separate. |
| inbound template-change handling | `templatechange`, `template change` | **0** | 0 (excluding C++ `template<`) | 0 | Genuinely new. The app has no concept of a device template today. |
| declared ignore list | `ignorelist`, `ignore list` | **0** | 1 archived proposal (CI path filtering), 1 Sheaf spec (`sheaf-chat-scoped-tools:282`, a path ignore list) | 0 | Genuinely new in this domain. |

**Family enumeration (§5, "a named symbol is one member of a family"):**
`ShiftState` and `HoldDrillState` are not merely similar — they are built by the
**same helper**, `HeldButton(address, kind)` at
`app/FroggersMidiCatalog.hpp:79-88`, whose own comment (:76-78) says "A
momentary control that holds a state while pressed and releases it on lift: Hold
Drill on the APC40, Shift on the Twister." They are constructed adjacently
(`MidiController.cpp:3026` and :3028), declared adjacently
(`MidiController.hpp:249-252` and :254-258), stored adjacently
(`MidiController.hpp:961-962`), and written adjacently in the same function
(:956/:959 and :969). FOUND = 2 members; the plan treats 1. See B6.

---

## Findings

### B1 — blocking — The change contradicts a live SHALL requirement it never names

`External/Sheaf/openspec/changes/shift-and-file-export/specs/synth-midi-instrument/spec.md:195`
(requirement **smi-16**) states:

> "…THE synth system SHALL hold per-profile state, a held flag, set by that
> button's press **and cleared by its release**… **A Shift press whose release
> never arrives SHALL leave the profile shifted until the next Shift press and
> release.**"

That is the exact behaviour this change's new capability abolishes:

> "A held modifier SHALL clear without requiring any further message from the
> device that set it."

`shift-and-file-export` is ✓ Complete and **unarchived**, so smi-16 is live
governing text. It carries a passing check,
`instrument_tests.cpp: ShiftHeldSwapsPressForShiftedPressAndReleaseClearsIt`.
Nothing in `proposal.md`, `design.md`, `tasks.md` or either delta spec names
`shift-and-file-export`, `synth-midi-instrument`, or smi-16. There is no
MODIFIED delta against `synth-midi-instrument` anywhere in this change, and task
2.1 defers the entire Sheaf artifact set to execution time.

This is precisely the failure the operator's own memory records as "a spec
requirement pinned the string I renamed."

**Must do:** name smi-16, write the Sheaf-side MODIFIED delta against
`synth-midi-instrument` that supersedes its last sentence, and state which of
the two changes governs. Under §9's "where they overlap, the more thoroughly
traced one governs", smi-16 is currently the better-traced of the pair — it has
a passing check; this change has none yet.

### B2 — blocking — §9's "enumerate the other active changes FIRST" was not run

`tasks.md` 2.2 schedules it, and only for Sheaf, and only after 2.1 has already
written the Sheaf change. §9 requires it before the plan is approved, because
its output changes scope. Run now, it returns five overlaps, none named in any
artifact:

| Change | Repo | Status | Overlap |
|---|---|---|---|
| `app-midi-catalog` | Sheaf | **26/28** | Owns `MidiAppDeviceDefault` (task 5.1 adds a field to it), the Controllers page **status dot**, and `MidiEndpointStatus` — its delta touches `synth-runtime-ui` and `synth-midi-instrument`. Direct collision with §5 and §7 of this change. |
| `rework-controllers-block-editing` | Sheaf | **22/28** | `synth-runtime-ui`; the Controllers page block/section editing. Task 5.4 renders preconditions "under the Layout dropdown" on that page. |
| `launchpad-model-on-the-row` | Sheaf | **11/14** | `synth-midi-instrument` + `synth-runtime-ui`. **This is the branch the submodule is checked out on right now** (`git submodule status` → `heads/launchpad-model-on-the-row`). |
| `shift-and-file-export` | Sheaf | ✓ Complete, unarchived | smi-16 — see B1. |
| `frogg3rs-effect-page-hierarchy` | frogg3rs | **6/34** | Holds `MANUAL.md` and `QUICK_DICT.md` dirty in the working tree (18 modified files total). Tasks 7.1 and 7.2 of this change edit both. |

**Must do:** enumerate these in the proposal, state a disposition per overlap
(§5: "close out every hit, in the same breath"), and sequence against the three
in-flight ones. Note also that `launchpad-model-on-the-row` being the checked-out
branch means task 1.5's gate baseline would baseline a tree carrying another
change's incomplete work.

### B3 — blocking — The Impact list omits every definition site the change edits

§8.0 scopes the hygiene sweep to "EVERY directory the change's own Impact
section names". The list as written misses:

- `External/Sheaf/projects/synth/include/synth/MidiAppCatalog.hpp:31-38` —
  where `MidiAppDeviceDefault` actually lives. Task 5.1 edits it; Impact reads
  as though the preconditions field were an `app/` change.
- `External/Sheaf/projects/synth/include/synth/Engine.hpp` — the **only**
  production rebuild site (`:990`, inside `RebuildMidiProcessors()` :967-996)
  and the **only** pump site (`:570-574`), where task 3.5's ceiling must be
  evaluated. The file is named nowhere in any artifact.
- `External/Sheaf/projects/synth/include/synth/MidiReconcile.hpp:114-120` —
  `MidiEndpointOps`, the struct a clear-on-open trigger has to grow a member on.
- `External/Sheaf/projects/synth/runtime/MidiConnectionManager.hpp:~450` **and**
  `External/Sheaf/projects/synth/include/synth/browser/BrowserMidiBridge.hpp:125-171`
  — the **two independent** places `MidiEndpointOps` is bound. Neither
  directory (`runtime/`, `include/synth/browser/`) is reachable from any Impact
  entry, so both go unswept.

The two binding sites are a §5 family that cannot be collapsed — one is JUCE,
one is wasm — which under §5's own rule means the deliverable is "a check that
FAILS on drift, proven to fail by breaking it once", not an enumeration. The
change schedules no such check. A clear-on-open op added to only one binding
leaves the browser (or the standalone) silently without the trigger.

### B4 — blocking — `MidiReconcile.cpp` cannot do the job Impact assigns it

Impact: "`External/Sheaf/projects/synth/src/MidiReconcile.cpp` — clearing
modifier state when an endpoint reopens." Task 3.3: "driven from
`MidiReconcile`'s open actions."

`MidiReconcile.hpp:55` documents the unit as a "Pure, JUCE-free reconciliation
planner". `ExecuteReconcilePlan` (`MidiReconcile.cpp:226`) reaches the outside
world only through `MidiEndpointOps` callbacks; it holds no `Engine`, no slot
vector, no `MidiControllerProfileResult`. And there is no accessor at all for
the per-slot `ShiftState`: `Engine` exposes `MidiControllerCount()` (:731),
`MidiInputProcessor(ix)` (:738-741) and `ResetMidiOutputProcessors(ix)`
(:841-844), and nothing that reaches `midiProcessors_[ix].shift`.

§1's corollary applies: a structural instruction that forces a worse structure
is suspect, not the implementer. Either the clear becomes a new `MidiEndpointOps`
member bound at both sites (B3), or it moves to `MidiConnectionManager` /
`BrowserMidiBridge` directly. The change must say which, and Impact must name
the file that will actually hold it.

### B5 — blocking — MODIFIED against a capability that does not exist, with no ordering task

`openspec show froggers-midi-controller-mappings` → `Unknown item`. The
capability is not in `openspec/specs/`; it is **ADDED** by
`frogg3rs-midi-shift`, which is ✓ Complete and unarchived. `openspec validate
--strict` passes on both changes, because validation checks delta syntax, not
promotion targets — so the green is not evidence here.

The MODIFIED delta is *coherent as authored* — the requirement header matches
the source exactly, and the copied body is correct (see the diff under S4) — but
it is **blocked on ordering**: archiving this change before `frogg3rs-midi-shift`
gives `openspec archive` no requirement to modify. The proposal acknowledges the
situation in prose ("complete and not yet archived; this change stacks on it")
but neither `tasks.md` §8 (Postflight and delivery) nor anything else makes
"archive `frogg3rs-midi-shift` first" an actionable step. The identical problem
applies on the Sheaf side against `shift-and-file-export` (B1).

**Must do:** add the archive-ordering prerequisite to tasks as a real step, for
both repos.

### B6 — blocking — `HoldDrillState` has the identical exposure, and the requirement already binds it

Task 1.2 asks for a disposition. Determined here:

`HoldDrillState` (`MidiController.hpp:249-252`) is the same shape
(`{bool held; std::vector<bool> drilled;}`), written in the same function by the
same `isPress` at :956/:959, constructed fresh by the same rebuild at :3026, and
read at :712 by `EncoderMidiInProcessor::Process`. It has **no ceiling, no
second-press path, no clear on reopen** — exactly the four holes this change
closes for Shift. The two are produced by one shared helper, `HeldButton()`
(`app/FroggersMidiCatalog.hpp:79-88`), whose comment names them as one concept.

The consequence of a stuck `holdDrill_->held` is at least as bad as a stuck
Shift: every encoder turn on that controller drills once and then drops
(`:712-722`), so the operator loses all knob control on the device, and the
`drilled` vector means each knob works exactly once. The APC40 mkII Generic
default maps it to note 98 (`FroggersMidiCatalog.hpp:140`), on a device whose
own comment block (:26-35) already documents a mode-dependent addressing caveat.

Leaving it out is a §5 "scheduled duplication", and worse, **the change's own
requirement already asserts the fix for it**: "The app SHALL clear **every
controller's held-modifier state**" — `HoldDrillState` is held-modifier state.
Meanwhile `design.md` names only `ShiftState`, Goals/Non-Goals never mention
Hold Drill, tasks 3.1-3.6 name only `ShiftState`, and not one scenario covers
it. The requirement is broader than the plan that is supposed to deliver it —
§9's promotion rule in advance.

**Must do:** either bring `HoldDrillState` into §3's tasks (the cheaper option —
it is the same four edits on an adjacent member), or narrow the requirement text
to Shift and say plainly that Hold Drill keeps the exposure, naming the change
that will fix it.

### B7 — blocking — The "mismatch state" has no definition site, and the BREAKING label points at a thing that does not exist

The proposal declares: "**BREAKING** for the Controllers page's status line,
which gains a mismatch state alongside its existing online and offline states."

There is no singular status line. A slot carries **two** endpoint statuses —
`MidiConfigViewModel.hpp:351-352`, `inputStatus` and `outputStatus`, each a
`MidiEndpointStatus` — rendered as separate dots via `EmitStatusDot`
(`ControllersPageUI.hpp:808`). `MidiEndpointStatus` is declared at
`MidiReconcile.hpp:12` as `{ Unconfigured, Offline, Online }` and consumed by an
exhaustive switch at `ControllersPageUI.hpp:795-799`, by `DeviceLabel`
(`MidiConfigViewModel.cpp:600-615`), and by a parallel label table at
`ControllersPageUI.hpp:3347-3352`.

Adding a fourth enumerator is the wrong shape: mismatch is a property of the
**slot**, orthogonal to each endpoint's connection state — a mismatched
controller is still Online, and collapsing the two loses the information the dot
exists to show. It would also break three exhaustive consumers and the test
table, none of which the Impact names. `design.md` never decides this; task 4.3
says only "distinct from online and offline".

And the enum plus the dot are owned by Sheaf's `synth-runtime-ui`, currently
under edit by `app-midi-catalog` at 26/28 (B2).

**Must do:** decide, in `design.md`, whether mismatch is a new enumerator or an
orthogonal per-slot field; name the definition site and every call site the
choice breaks; and reconcile with `app-midi-catalog`.

### B8 — blocking — Task 1.4 asks the operator to do something the proposal says is impossible

Task 1.4: "instrument `SystemButtonMidiInProcessor::Process` to log held
transitions, **run the Twister with a side button set to something other than CC
Hold**, and record the observed transitions."

The same change's Impact section says: "That setting lives in the device's flash
and is reachable only through DJ TechTools' Midi Fighter Utility, **which is not
installed on the operator's machine**." And Decisions says the operator has
**already corrected** the device — "Verified by capture 2026-09-10: all six side
buttons now transmit on channel 4, CC 8-13, with matched `7F`/`00` pairs."

So the task asks the operator to (a) install a utility the proposal treats as
unavailable, (b) deliberately re-break a device they just fixed, to (c) prove a
premise the same document already reports as captured evidence. The preamble is
explicit: "An operator check asserts the thing is OBSERVABLE… Verify that path
exists before writing the task, or you spend their effort proving nothing and
get back a false negative you then have to explain."

**Must do:** replace 1.4 with a check that does not need the hardware. The
premise — "a press with no release leaves `held` true" — is settled by reading
(`MidiController.cpp:945-946`, :964-971) and is provable in
`instrument_tests.cpp` by feeding a `7F` with no `00` and asserting the next
press on another address pushes `shiftedPress`. That is minutes, deterministic,
and is the positive control task 3.6 needs anyway. If a hardware capture is
genuinely wanted, say what it adds over the reading, and note that the operator
cannot currently produce it.

---

### S1 — should-fix — The Why section's causal claim is refuted by one `git show`

> "`frogg3rs-midi-shift` introduced this exposure. Before it, each of the six
> side buttons fired on its press alone, which a press-only button satisfies.
> Shift can only end on a release, so that change made correct operation depend
> on the Twister's own settings."

`git show 40b417c^:app/FroggersMidiCatalog.hpp` (the commit before midi-shift)
carries the precondition comment at lines **14-19, verbatim**, including "all
six side buttons set to CC Hold (127 on press, 0 on release, **not the factory
bank-switch behaviour on the middle pair**)" and "**Bank Side Buttons unchecked
so the side buttons keep this default's CC addresses whatever Twister bank is
lit**". Correct operation depended on the Twister's own settings *before*
midi-shift, and the artifacts' own diagnosis says so: with Bank Side Buttons
checked, the addresses **move**, so the pre-shift buttons did not fire on their
press either — they fired nothing, at addresses nobody mapped.

What midi-shift changed is the *consequence*: before, a misconfigured device
lost buttons; after, it also strands every remaining button in its shifted job.
That is a real and sufficient motivation. The current wording is §1's "asserting
a plausible mechanism from adjacent evidence", one `git show` away from the
truth, and it misattributes the defect to a change that inherited it.

### S2 — should-fix — Citation `MidiController.cpp:957-961` excludes the line it is about

See the citation table. Correct range is `:955-961`; `holdDrill_->held = true`
is at `:956`.

### S3 — should-fix — The two delta specs use opposite channel conventions

`specs/froggers-midi-controller-mappings/spec.md:6` says "channel 3 (channel 4
counted from 1)" and qualifies it. `specs/froggers-midi-controller-resilience/spec.md`
says bare "channel 4" three times (":30", ":34"). `MANUAL.md:338` uses a third
phrasing, "channel 4 (channel 3 counted from 0)". The proposal's evidence
paragraph uses "channel field 3" for the broken capture and "channel 4" for the
corrected one, which reads as two different channels and is not.

The resilience scenarios are the ones a check gets written against, and they are
the unqualified ones. Fix the convention once, everywhere, and qualify it at
every occurrence.

### S4 — should-fix — The MODIFIED requirement loses check coverage

Against the source at
`openspec/changes/frogg3rs-midi-shift/specs/froggers-midi-controller-mappings/spec.md:20-38`:

- Requirement body: sentences 1-2 copied **exactly**; sentence 3 deliberately
  rewritten ("SHALL continue to require CC Hold on every side button" →
  "SHALL declare CC Hold on every side button as a device precondition… and
  SHALL keep working in its unshifted form when that precondition is unmet");
  sentence 4 copied exactly. The edit is deliberate and correct. ✓
- Scenario "The preset's side buttons are the eleven named jobs" — copied
  exactly, Check retained. ✓
- Scenario "Only the Twister carries Shift or shifted jobs" — copied exactly,
  Check retained. ✓
- Scenario "A missing release leaves Shift held until the next press and
  release" → replaced by "A missing release does not outlive the profile that
  saw it", with the outcome reversed. Deliberate and correct, but its
  `Check: MANUAL.md, the Shift subsection` line is **dropped**, not replaced.
- Two new scenarios added ("A Twister without CC Hold keeps its unshifted jobs",
  "The preset declares what the device must be set to") — **no Check line on
  either**.

Net: the source requirement had a Check on 3 of 3 scenarios; the modified one
has a Check on 2 of 5. Task 8.3 catches this at postflight, but §9's promotion
rule wants it decided when written — each scenario gets a named check or a plain
"not yet delivered" marker now.

### S5 — should-fix — The APC40 precondition does not fit the declared shape

Task 5.3 cites "the same comment block" for the APC40; it is a different block
(`app/FroggersMidiCatalog.hpp:26-35`). More substantially, the design defines a
precondition as "a setting name, its required value, **and where it is set**".
The Twister's three fit. The APC40's does not: "Track 1 must stay selected" is a
live operating state on the unit's front panel, not a stored setting, and it has
no utility to name. Either widen the declared shape to carry an operating
caveat, or drop 5.3 and keep the APC40 caveat as prose — but decide it in
`design.md`, not in the executor's lap.

### S6 — should-fix — Say that the second-press clear is inert in the captured incident

See "Unverified behavioural claims" §3. The proposal reads as though the
second-press path answers the evidence; it does not, because CC 13 sent nothing
and CC 0/1 are unmapped. One sentence in the Why section fixes it.

### S7 — should-fix — The second-press clear makes Shift a toggle, which the mappings spec then contradicts

On a press-only device the new rule gives press → held, press → clear, press →
held: Shift becomes a latch. The mappings delta then asserts, unconditionally:

> "**WHEN** the Twister's side buttons send press edges with no release edges
> **THEN** Bank Next, Play, Freeze, Scene 1 and Randomize Page each still fire
> on their press"

That is true only while the latch is off. As written the scenario is untestable
— a check for it passes or fails depending on how many times CC 13 was pressed
first. Qualify it ("…with the modifier clear"), or say what the latch does.

Note also that `design.md` rejected an on-screen Shift latch because it
"contradicts the existing requirement that the app offers no on-screen Shift
control" (smi-16's companion in `frogg3rs-midi-shift`, requirement "A button's
shifted job is part of its mapping": "SHALL offer no on-screen Shift control").
The rejection is sound; the second-press rule reintroduces latching behaviour at
the device, which is fine, but the design should say it chose a device-side
latch rather than none.

### S8 — should-fix — Three hosts, three pump mechanisms, one cited

See "Unverified behavioural claims" §2, including that the VST has no
`MidiConnectionManager` and therefore no endpoint-reopen trigger at all.

### S9 — should-fix — The design cites the tick's rate, not the pump

`app/vst/FroggersPluginProcessor.cpp:312` sets the *rate*. The pump the ceiling
must be evaluated in is `Engine.hpp:570-574`:

```
for (MidiControllerProfileResult& processors : midiProcessors_) {
    for (auto& output : processors.outputs) {
        output->Process();
    }
}
```

Cite it. Note also that this loop walks **output** processors, while
`ShiftState` lives on the profile and is written from the **input** chain — so
"evaluate the ceiling where output processors are pumped" (task 3.5) needs a
stated hook, since no output processor holds the shift pointer today.

### S10 — should-fix — The substance of the change has no artifacts

Modifier lifetime, mismatch detection, the status state and the template rate
limit are all Sheaf work; `tasks.md` 2.1 defers the entire Sheaf proposal,
design, delta specs and preflight to execution time. The frogg3rs capability
`froggers-midi-controller-resilience` therefore asserts, in SHALL form,
behaviour that belongs to Sheaf's `synth-midi-instrument` and `synth-runtime-ui`
and that no Sheaf artifact has yet claimed. §2's chain wants the OpenSpec before
the proposal, not after; §9 wants the preflight before execution. Write the
Sheaf half's artifacts before this change is approved, and let the frogg3rs
capability cover only what frogg3rs owns — the Twister preset's declared
preconditions and its degraded behaviour.

---

### N1 — note — The tree is dirty with another change's work

`git status` shows 18 modified files including `MANUAL.md` and `QUICK_DICT.md`
(both edited by tasks 7.1/7.2 here), from `frogg3rs-effect-page-hierarchy` at
6/34. The submodule is on `launchpad-model-on-the-row` at 11/14. Task 1.5's gate
baseline would record a mixed tree, and task 8.5's "verify clean trees and no
submodule-pin dirt at both levels" cannot be satisfied while either is in
flight. Sequence against them.

### N2 — note — Cited MANUAL.md lines are stable despite the dirt

Verified: the working-tree diff to `MANUAL.md` begins at line 521, and lines
319-320 / 336-339 are byte-identical at `HEAD`. The citations are safe today,
but they will move the moment `frogg3rs-effect-page-hierarchy` lands — cite by
section heading (`### Shift`, `### MIDI Fighter Twister`) as well as line.

### N3 — note — "unmapped" already means something else

`synth-midi-instrument` spec:285, "Unmapped active controller still supplies
clock", uses "unmapped" for a slot with no mappings. This change uses it for an
address the profile does not cover. Pick a different word for one of them.

### N4 — note — Rebuild clears every slot, not one

`RebuildMidiProcessors()` rebuilds all slots (`Engine.hpp:978-995`), so adopting
rebuild as a modifier-clear trigger means editing controller 1 clears
controller 5's Shift. Harmless, probably desirable, but it is behaviour the
design should state rather than discover.

### N5 — note — The XL flood open question is handled correctly

The Launch Control XL flood is recorded with its capture, explicitly excluded
from Goals, and the change hardens against it without claiming a cause. Task 6.4
names the deciding quantity ("messages processed on the other addresses during
the flood") and demands it be reported. This is the part of the change that best
satisfies §6.1, and it should survive the rewrite unchanged.

---

## Artifact staleness

If these findings are accepted, §9's own rule applies — *"an audit that rewrites
a proposal has produced an unaudited proposal"* — so the revision needs a second
pass, not just an edit. What must be rewritten:

**`proposal.md`** — Why (S1's causal claim, S6's framing); the **BREAKING**
bullet (B7); Impact, which must gain `MidiAppCatalog.hpp`, `Engine.hpp`,
`MidiReconcile.hpp`, `runtime/MidiConnectionManager.hpp` and
`include/synth/browser/BrowserMidiBridge.hpp`, and must correct or remove
`MidiReconcile.cpp` (B3, B4); a new "other active changes" section (B2); the
archive-ordering prerequisite (B5); the Sheaf-artifacts sequencing (S10).

**`design.md`** — Context (the pump site S9, the three hosts S8, the VST's
missing reconcile path); a new decision on where the mismatch state lives (B7);
a decision on `HoldDrillState` (B6); a decision on the APC40 precondition shape
(S5); the device-side-latch consequence of second-press clearing (S7); the
`MidiEndpointOps` two-binding drift check (B3).

**`tasks.md`** — 1.2 (citation range S2, and its disposition is now decided:
fold in or narrow the requirement); 1.4 replaced entirely (B8); 1.5 and 1.6
rescoped once Impact is corrected (B3); 2.1/2.2 moved ahead of approval (B2,
S10); §3 extended or the requirement narrowed (B6); 4.3 given a definition site
(B7); 5.1 pointed at the real file (B3); 5.3 reshaped (S5); a new archive-order
task (B5).

**`specs/froggers-midi-controller-mappings/spec.md`** — checks restored on three
scenarios (S4); the press-only scenario qualified (S7); channel convention fixed
(S3).

**`specs/froggers-midi-controller-resilience/spec.md`** — "every controller's
held-modifier state" reconciled with whatever B6 decides; channel convention
fixed (S3); each scenario given a named check or a plain not-yet-delivered
marker (S4, task 8.3 pulled forward); and the whole of the modifier-lifetime and
mismatch material reconsidered for whether it belongs in a frogg3rs capability
at all (S10).

**New, not yet existing** — the Sheaf change's `proposal.md`, `design.md`,
`tasks.md`, and a MODIFIED delta against `synth-midi-instrument` superseding
smi-16 (B1).
