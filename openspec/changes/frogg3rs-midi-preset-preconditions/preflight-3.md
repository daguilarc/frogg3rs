# Third preflight audit — `frogg3rs-midi-controller-resilience`

Run 2026-09-13 in a fresh context that did not write the change and did not
write either prior report. Read-only against the tree; no builds, no branch
operations, nothing installed. `preflight.md` and `preflight-2.md` were read as
context only — every finding below was re-derived by opening the file, and the
two places where I disagree with `preflight-2.md` are marked as disagreements.

**Artifact staleness, verified rather than assumed.** `proposal.md` 22:09,
`design.md` 22:10, `tasks.md` 22:11, both spec deltas 21:41, all on 2026-09-10.
`preflight-2.md` 22:26 the same day. No artifact has been touched since the
second audit was written, and `tasks.md` still stands at 1/42 with only 1.1
checked. Nothing in F1–F7 has been addressed.

---

## VERDICT

**SUPERSEDE.**

Every one of F1–F7 is still live and, on my own reading, every one of them is a
real finding — F7 only partly, and I say where it overstates. Two further
blocking defects are new to this pass: `~/Desktop/midi-log/` does not exist, so
the proposal asserts a logger that is not there and task 6.4 replays a capture
file that does not exist; and the mappings delta's scenario "A Twister without
CC Hold keeps its unshifted jobs" contradicts this change's own second-press
clear. The overlap table has also decayed: it names six of twelve active
changes, one of the six is archived, and the change that now holds `MANUAL.md`
and `QUICK_DICT.md` modified in the working tree is not in it — so the only
coordination mechanism the change states points at a change that no longer
exists. But the reason to supersede rather than list repairs is structural: 26
of 42 tasks and 9 of 11 definition sites are Sheaf code, governed by Sheaf's
own `synth-midi-instrument` and `synth-runtime-ui`, spread across three stacked
unmerged branches and two further active Sheaf changes — and the change writes
no Sheaf artifact for any of it. A frogg3rs proposal asserting in SHALL form
behaviour that the submodule's own specs contradict is not a shape that repairs
into a correct one.

---

## F1–F7 disposition, independently judged

### F1 — the Engine owns neither the binding nor the action application — **LIVE, and correct**

Re-derived, not adopted:

- `grep -n 'MidiEndpointOps\|ExecuteReconcilePlan\|PlanMidiReconciliation\|MidiReconcile' External/Sheaf/projects/synth/include/synth/Engine.hpp` → **zero matches.**
- The only two production callers of `ExecuteReconcilePlan` are
  `External/Sheaf/projects/synth/include/synth/browser/BrowserMidiBridge.hpp:175`
  and `External/Sheaf/projects/synth/runtime/MidiConnectionManager.hpp:495`.
  The rest are tests.
- A second operand confirms it from the other direction:
  `External/Sheaf/projects/synth/include/synth/MidiReconcile.hpp:109` says in
  its own words that "the runtime (`synth_runtime::MidiConnectionManager`) binds
  these to real device handlers **and `Engine<App>` calls**." The runtime binds;
  the Engine is what gets called.

So both halves of `design.md`'s sentence — "the Engine already owns both the
processors and the `MidiEndpointOps` binding" and "The clear hangs off the
Engine's action application" — are false. The Engine owns the processors; it
has no binding and no action application.

I add one thing the second audit did not reach, which makes the repair harder
than it looks. The two bindings are **not symmetric**, so task 3.4's check has
no symmetric thing to check:

- `runtime/MidiConnectionManager.hpp:450` — `ops.openInput` calls
  `OpenInput(ix, identifier)`, which opens a real device inline.
- `browser/BrowserMidiBridge.hpp:126-129` — `ops.openInput` only pushes
  `{.type = ActionType::OpenInput, ...}` onto `actions_` and returns true. It
  touches no engine state at all. The engine is reached from that file only in
  `updateInputRef` (:154), `updateOutputRef` (:163) and `resync` (:172).

A clear hung off `ops.openInput` fires at open time on the native runtime and
at plan time in the browser, where the open has not happened yet. The browser's
equivalent moment is the consumer of `DequeueAction()`, which no artifact names.

**Remedy:** name the Engine method, name the file that declares it, rewrite 3.3
and 3.4 so they describe one mechanism, and say explicitly where the browser's
clear fires given that its `openInput` is deferred.

### F2 — `include/synth/runtime` does not exist — **LIVE, and correct, with a consequence neither prior audit traced**

`ls External/Sheaf/projects/synth/include/synth/runtime` → No such file or
directory. The only subdirectory of `include/synth/` is `browser/`. The real
file is `External/Sheaf/projects/synth/runtime/MidiConnectionManager.hpp`,
confirmed by `find . -name 'MidiConnectionManager*'` returning exactly that one
path.

Wrong in four places, unchanged: `proposal.md` Impact (the `MidiEndpointOps`
binding-site bullet), `proposal.md`'s §8.0 scope sentence, `design.md` Context,
and `tasks.md` 1.2. Task 3.4 alone spells it `runtime/MidiConnectionManager.hpp`.

**New, and mechanical.** `app/check_artifact_symbols_resolve.py` is run by
`app/Makefile:238`. Its `PATH_ROOTS` is `('app', 'openspec', 'External/Sheaf',
'src')`, and it reads every live change's `tasks.md` and `proposal.md` once they
are tracked. A backticked token whose first segment is one of those roots is a
path claim that must resolve. I confirmed the two spellings against the
filesystem through the gate's own constants:

```
External/Sheaf/projects/synth/include/synth/runtime/MidiConnectionManager.hpp -> False
External/Sheaf/projects/synth/runtime/MidiConnectionManager.hpp              -> True
```

Running the gate as it stands reports `OK - 2 artifact file(s) resolve` — two,
because this change is untracked and therefore skipped. Committing it as
written fails that gate on `proposal.md`. This is not a style point; it is the
repository's own drift check, already installed, already pointed at exactly this
defect.

### F3 — smi-14 is the Hold Drill twin of smi-16, on a lower branch, unnamed — **LIVE, and correct**

`External/Sheaf/openspec/changes/app-midi-catalog/specs/synth-midi-instrument/spec.md:200`
is `### Requirement: smi-14 — Hold Drill: momentary drill-in gate on a held
button`, and :201 states the held flag is "set by that button's press and
release" and that "its release SHALL clear held". `grep -rli 'hold drill'
External/Sheaf/openspec/specs/` returns nothing, so no promoted Sheaf spec
covers Hold Drill and smi-14 is what governs it.

This change's ADDED requirement — "A held modifier SHALL clear without requiring
any further message from the device that set it … which today are Shift and Hold
Drill" — adds four clears smi-14 does not admit, on exactly the reading that
made smi-16 a collision the change does name.

The branch order makes it worse, and I verified it from `shift-and-file-export`'s
own proposal, which opens "This change stacks on `app-midi-catalog`
(jvictor0/Sheaf#13) and `rework-controllers-block-editing`". `app-midi-catalog`
is **below** #14. Amending smi-14 rebases #14 and #15. Task 2.2 rebases #15
only, and only through the #14 amendment; task 2.3 touches #13 only to add the
preconditions field.

**Remedy:** name smi-14, add an amend task on #13 beside 2.3, and extend 2.2 to
rebase both dependent branches.

### F4 — the overlap enumeration is partial — **LIVE, correct when written, and materially worse now.** See "Overlap enumeration, mine" below.

### F5 — Impact drops `ControllersPageUI.hpp` and never names `MidiConfigViewModel.cpp` — **LIVE, and correct**

Both files exist, both are large, and both are where tasks 4.3, 4.4, 5.3 and 6.3
must land:

- `External/Sheaf/projects/synth/include/synth/ControllersPageUI.hpp` — 3462
  lines. Holds the endpoint-status switch at :793-801 and the status→label table
  at :3350-3352, i.e. the code the mismatch field renders beside.
- `External/Sheaf/projects/synth/src/MidiConfigViewModel.cpp` — 4007 lines.
  Holds `DeviceLabel` at :600-618, i.e. where the per-slot row is built and
  where a new per-slot field gets populated.

`proposal.md` presents its Impact as "Definition sites, each verified by
reading". Two of the edited definition sites are not in it.

### F6 — the resilience substance still has no Sheaf artifacts — **LIVE, and correct**

`External/Sheaf/openspec/changes/` holds ten changes; none is this work.
Reading each one's `specs/` directory, `synth-runtime-ui` — the capability that
owns the Controllers page this change renders on — is deltaed by eight of them
(`app-midi-catalog`, `shift-and-file-export`, `launchpad-model-on-the-row`,
`rework-controllers-block-editing`, `ui-state-before-audio`,
`browser-slider-value-readout`, `shorten-deadline-readout-window`,
`fix-out-of-tree-app-gaps`). This change writes no `synth-runtime-ui` delta and
no `synth-midi-instrument` delta. Task 2.1 removes smi-16's contradiction and
writes nothing in its place, so after 2.1 lands the synth has no stated
requirement for modifier lifetime at all.

### F7 — the hold ceiling has no hook and no time source — **LIVE in two of three parts. I disagree with the first part.**

**Disagreement, first part.** F7 says `Engine.hpp:570-574` "names a location that
cannot reach the state" because it pumps `processors.outputs`. That is wrong.
The loop is `for (MidiControllerProfileResult& processors : midiProcessors_)`,
and `MidiControllerProfileResult` is declared at
`External/Sheaf/projects/synth/include/synth/MidiController.hpp:957-963` with
`std::unique_ptr<HoldDrillState> holdDrill;` at :961 and
`std::unique_ptr<ShiftState> shift;` at :962 sitting beside `outputs` at :960.
The loop variable *is* the struct that holds both. The ceiling is evaluable
there. F7's conclusion — that the artifacts never name the hook — stands; its
stated reason does not.

**Second part, correct but overstated.** No artifact names a clock, which is a
real §9 omission ("list every constant, helper, type, predicate and sentinel by
name"). But a time source exists and is threaded already:
`MidiInProcessor::TimestampProvider` at `MidiController.hpp:72`, its setter at
:79, `NextTimestamp()` at :82, the member at :93; `Engine::timestampProvider_`
at `Engine.hpp:1462`, passed into `CreateMidiControllerProfile` at
`Engine.hpp:991`. So the ceiling is implementable and injectable-for-test; the
defect is that nothing is named, not that nothing exists.

**Third part, correct and confirmed.** `grep -rn 'MidiConnectionManager' app/`
returns nothing. The VST has no endpoint open or reopen at all — it receives
MIDI from the host. So one of the four "independent" triggers is structurally
absent on one of the three hosts, the ADDED requirement states it
unconditionally ("when its input or output endpoint opens or reopens"), and the
scenario "A modifier held at disconnect does not survive the reconnect" is not
expressible there. No artifact says so.

---

## Citation audit

Every file:line in all four artifacts, opened and read on 2026-09-13.

| Cited as | Verdict |
|---|---|
| `MANUAL.md:319-320` — stuck Shift clears on press and release | **Correct.** :319-320 are the two lines ending "…stay shifted until Shift is pressed and released again." |
| `MANUAL.md:336-339` — the three Twister settings | **Correct**, exactly those four lines. |
| `app/FroggersMidiCatalog.hpp:14-24` — the precondition comment | **Correct**, exactly. :14 opens "Twister: the manual's Utility settings must match this default", :24 closes "…because the release is what ends Shift." |
| `MidiController.hpp:250-258` — `HoldDrillState` and `ShiftState`, adjacent | **Off by one at the start.** `struct HoldDrillState {` is :249; the cited range begins inside it. `ShiftState` is :256-258. Cosmetic, but this is the Impact list's own claim. |
| `src/MidiController.cpp:955-961` — Hold Drill writer | **Correct**, the `if (isPress)` body writing `holdDrill_->held`. |
| `src/MidiController.cpp:964-970` — Shift writer | **Correct.** The block opens :964; the single write `shift_->held = isPress;` is :969. |
| `src/MidiController.cpp:975` — shifted-press read | **Correct**, `const bool shifted = shift_ != nullptr && shift_->held && …`. |
| `src/MidiController.cpp:3026-3029` — fresh construct on profile build | **Correct**, exactly. |
| `Engine.hpp:990` — "the only profile rebuild site" | **Line correct, claim over-broad.** :990 is the `CreateMidiControllerProfile` call in `RebuildMidiProcessors`, and it is the only one in `Engine.hpp`. But the same function is also called at `src/MidiController.cpp:3307`, `:3335` and `:3392` (the WrldBldr, Twister and Launchpad default-profile factories) and twice in `tests/controllers_page_ui_tests.cpp`. Those all construct fresh state, so nothing breaks — but "the only rebuild site" is the sentence an executor hangs the clear hook on. |
| `Engine.hpp:570-574` — "the only pump site" | **Wrong on both counts.** :570-574 is the MIDI *output* processor loop; the pump is `MessageThreadTick()` at `Engine.hpp:497`, and it has three callers: `browser/BrowserRuntime.hpp:722`, `runtime/Runtime.hpp:944`, and the VST via `app/vst/FroggersPluginProcessor.hpp:378,401`. Calling a loop inside the tick "the pump site" hides the three-host fan-out the ceiling actually depends on. |
| `MidiAppCatalog.hpp:31-38` — `MidiAppDeviceDefault` | **Correct**, exactly: :31 `struct MidiAppDeviceDefault {` through :38 `};`. The proposal's added remark "This is the struct's real home; it is not in `app/`" is also correct. |
| `MidiReconcile.hpp:114-120` — `MidiEndpointOps` | **Truncated.** :114 is the struct opener, but the struct runs to :122; the cited range stops at `updateOutputRef` and drops `resync` at :121. `resync` is a live seam — both bindings route it to `engine_.ResetMidiOutputProcessors(ix)` (`BrowserMidiBridge.hpp:171-172`, `MidiConnectionManager.hpp:462`) — and the change never considers it as a clear point. |
| `include/synth/runtime/MidiConnectionManager.hpp` | **Path does not exist.** See F2. |
| `include/synth/browser/BrowserMidiBridge.hpp:125-171` | **Correct.** :125 `synth::MidiEndpointOps ops;` through :171 `ops.resync = …`. |
| `MidiConfigViewModel.hpp:351-352` — the two per-slot status fields | **Correct**, exactly: `inputStatus` :351, `outputStatus` :352. |
| `app/vst/FroggersPluginProcessor.cpp:312` — `startTimerHz(30)` | **Correct**, exactly. |
| `~/Desktop/midi-log/` — "a logger … retains any recurrence" (proposal) and the source file task 6.4 replays | **Does not exist.** See Finding 7. |
| `frogg3rs-effect-page-hierarchy` — "holds `MANUAL.md` modified in the working tree" (overlap table, task 7.2) | **Stale.** Archived as `openspec/changes/archive/2026-09-12-frogg3rs-effect-page-hierarchy`. |
| `frogg3rs-midi-shift` archived, `froggers-midi-controller-mappings` promoted | **Correct.** `openspec/changes/archive/2026-09-11-frogg3rs-midi-shift` exists and `openspec/specs/froggers-midi-controller-mappings/spec.md` is tracked. The MODIFIED delta's requirement header matches the promoted one verbatim, and `openspec validate --strict` passes. |

---

## Overlap enumeration, mine

`openspec list` run in both trees on 2026-09-13.

**frogg3rs (2 active):** `frogg3rs-stereo-field-and-density` 33/45, and this
change.

**Sheaf (10 active):** `launchpad-model-on-the-row` 11/14,
`shift-and-file-export` complete, `ui-state-before-audio` 4/7,
`app-midi-catalog` 26/28, `shorten-deadline-readout-window` 0/6,
`fix-out-of-tree-app-gaps` complete, `browser-slider-value-readout` 1/7,
`bank-addressed-absolute-write` 5/7, `rework-controllers-block-editing` 22/28,
`fix-task-analyzer-plan-derived-tasks` complete.

Twelve active. The proposal's table names six, and one of the six is archived.
Open PRs on jvictor0/Sheaf are **not reachable** from this sandbox; PR numbers
below are taken from the changes' own proposals, not from the forge.

| Change | In the table? | Overlap, traced | Is the disposition defensible? |
|---|---|---|---|
| `frogg3rs-stereo-field-and-density` 33/45 | **No** | Its Impact names "**Affected documents:** `MANUAL.md`, `QUICK_DICT.md`"; its tasks 4.1 and 4.6 are unchecked and edit `MANUAL.md`; `git status` shows it holding `MANUAL.md`, `QUICK_DICT.md`, `app/FroggersUiSurface.hpp`, `app/FroggersParameters.hpp`, `app/dsp/Delay.hpp`, `app/dsp/Reverb.hpp` modified right now. This change's tasks 7.1 and 7.3 edit `MANUAL.md` and `QUICK_DICT.md`. It also owns `app/check_docs_match_parameter_table.py` and `app/check_artifact_symbols_resolve.py` as affected gates — the second of which is the gate F2 trips. | **No disposition exists.** This is the live collision and it is unnamed. |
| `frogg3rs-effect-page-hierarchy` | Yes | Archived 2026-09-12. | **Dead.** Task 7.2 ("Coordinate the `MANUAL.md` edit with `frogg3rs-effect-page-hierarchy`") is the change's only stated coordination mechanism for the document collision, and it points at nothing. |
| Sheaf `app-midi-catalog` 26/28 (#13) | Yes | Correct as far as it goes: owns `MidiAppDeviceDefault`. But it also owns **smi-14** (F3), which the table does not mention. | **Partly.** The struct half is fine; the requirement half is missing. |
| Sheaf `shift-and-file-export` (#14) | Yes | Owns smi-16. | **Yes**, as stated. |
| Sheaf `launchpad-model-on-the-row` 11/14 (#15) | Yes | Stacks on #14; is the submodule's checked-out branch at `ba3898e4`, confirmed. | **Yes**, but 2.2 must also rebase through the #13 amendment F3 requires. |
| Sheaf `rework-controllers-block-editing` 22/28 | Yes | `shift-and-file-export`'s own proposal says it stacks on this change as well as #13, so it is **below** #14 in the stack, not a peer to sequence after. | **Roughly right, wrongly described.** "Sequence after it" happens to hold because it is already beneath the stack; the table's reasoning does not. |
| Sheaf `bank-addressed-absolute-write` 5/7 | Yes | Deltas `synth-parameter-modulation` only. | **Yes.** Note-only is right. |
| Sheaf `ui-state-before-audio` 4/7 | **No** | Its proposal names `include/synth/Engine.hpp` as the code it edits, and its constraint is pinned in a comment at `Engine.hpp:576-580` — **four lines below** the loop at :570-574 that this change's ceiling adds work to. The comment reads, in the tree, `// ui-state-before-audio (design "Mechanism", PINNED): after every / // other MessageThreadTick duty, …`. | **No disposition exists.** A PINNED constraint inside the function you are editing is the overlap that governs. |
| Sheaf `browser-slider-value-readout` 1/7 | **No** | Deltas `synth-runtime-ui`; browser Controllers/parameter surface. | Missing. Probably "nothing needed" — but §5 requires the disposition to be stated, including "nothing needed". |
| Sheaf `shorten-deadline-readout-window` 0/6 | **No** | Deltas `synth-runtime-ui`; task 1.6 baselines the Sheaf gate and carries "the two 96 kHz deadline failures", which is the readout this change touches. | Missing; needs at least a note. |
| Sheaf `fix-out-of-tree-app-gaps` complete | **No** | Deltas `synth-runtime-ui` among others. | Missing. |
| Sheaf `fix-task-analyzer-plan-derived-tasks` complete | **No** | `task-analyzer-data-gathering`; no overlap. | Missing, but "nothing needed" is the right answer once stated. |

Eight hits with no disposition, and one disposition pointing at an archived
change.

---

## Forward enumeration (§5, run forward, by operand, case-insensitively)

Two independent passes, per the plural-independence rule: one run by me and one
by a delegate given a different operand list and a different tree list. Both
excluded `build/`, `node_modules/`, `dist/` and `.git/`. My pass additionally
excluded this change's own directory so it does not count itself; the delegate's
did not, which is why its raw totals are larger. Every conclusion below is one
both passes reach.

**§6.1 control on the instrument.** Control operands returned non-zero on my
run — `ShiftState` 15 hits / 6 files, `HoldDrillState` 17 / 8, `MidiEndpointOps`
24 / 7, `monotonic` 180 / 88 — so a zero from this sweep is a result and not a
dead instrument.

**CHANGED is 0 for every concept. Nothing is implemented; this is a preflight.**

| Concept the change creates | Operands | FOUND in source, domain sense | FOUND, any sense | CHANGED |
|---|---|---|---|---|
| The shared held-state type | `HeldState`, `HeldButton`, `ShiftState`, `HoldDrillState` | **0** | `HeldState` 0 in source (its only hit anywhere is `preflight-2.md` saying it is unnamed); `HeldButton` 8/3, all the existing catalog helper at `app/FroggersMidiCatalog.hpp:79` and its two call sites :114, :140; `ShiftState` 15/6; `HoldDrillState` 17/8 | 0 |
| The hold ceiling | `hold ceiling`, `HoldCeiling`, `kHoldCeiling` | **0** | `HoldCeiling`/`kHoldCeiling` 0; `hold ceiling` 1 hit — `app/dsp/Drive.hpp:870`, "Decay/Hold ceiling", a reverb gain limit. A prose collision in the same repository, with a different meaning | 0 |
| The clear-source record | `ClearSource`, `ClearReason`, `clear source` | **0** | **0** in source | 0 |
| The declared-preconditions field | `DeclaredPrecondition`, `declared precondition`, `precondition` | **0** | `declared precondition` 1 hit, an archived Sheaf tasks file, unrelated sense; bare `precondition` is the doc-comment "Precondition:" sense throughout | 0 |
| The observed-address set | `ObservedAddress`, `observed address` | **0** | **0** | 0 |
| The mismatch report field | `mismatch`, `unmapped` | **0** as a per-slot device concept | `mismatch` is broad and none of it is this sense; `unmapped` collides with `synth-midi-instrument`'s existing "unmapped controller" sense, which means a slot with **no mappings**, not an address the profile does not cover | 0 |
| The ignore list | `IgnoreList`, `ignore list` | **0** | 2 hits, both unrelated: a CI path filter in an archived frogg3rs proposal, a tool-sandbox path list in `sheaf-chat-scoped-tools` | 0 |
| The token bucket / rate limit | `TokenBucket`, `token bucket`, `RateLimit`, `rate limit` | **0** | 0 in source; `RateLimit` hits are Codex API quota prose in `xagent-provider-usage` | 0 |
| Inbound template change | `TemplateChange`, `template change` | **0** | 0 in this sense; bare `template` is the C++ keyword | 0 |

**Structures the change builds on, so the numbers are comparable:**
`MidiEndpointStatus` has **3** enumerators (`Unconfigured, Offline, Online`,
`MidiReconcile.hpp:12`) and **two** dispatch sites, not three — the switch in
`ControllersPageUI.hpp:793-801` and the if-chain in
`MidiConfigViewModel.cpp:600-618`. `ControllersPageUI.hpp:857` and `:872` are
guards, not three-way dispatch. `MidiEndpointOps` has **two** production
bindings (`BrowserMidiBridge.hpp:125-171`, `MidiConnectionManager.hpp:449-462`)
and **three** test bindings (`tests/reconcile_executor_tests.cpp:93-111` full,
`tests/reconcile_tests.cpp:657-659` and
`tests/controllers_page_ui_tests.cpp:1462-1466` partial).

**The plan's own forward enumeration is narrower than §9 requires.** Task 1.2
schedules a sweep over the held-modifier family only — twelve operands, all
Shift and Hold Drill. The eight concepts in the table above get none, and task
8.1 defers the work to postflight. §9 asks for it forward, before approval, on
every named concept. Two of the concepts have **no name in any artifact** to
enumerate by: the shared held-state type and the clear-source record. The
substantive risk is low — every row is zero — but the result is not carried in
the proposal, and the naming gap is a real §9 failure.

---

## Behavioural premises

### The browser tick chain, traced end to end

The brief's unrecorded finding checks out. Every link read:

1. `External/Sheaf/projects/synth/browser/src/main.ts:249` —
   `this.frameTimer = setInterval(() => { this.requestFrame(); }, this.options.frameIntervalMs);`
   on the **main thread**.
2. `:406` — `frameIntervalMs: options.frameIntervalMs ?? 1000 / 30`.
3. `:343` `requestFrame()` → `renderFrame()`, which awaits
   `this.runtime.request({ type: "message-tick", timestampMicros: Math.round(performance.now() * 1000) })`.
4. `browser/src/worker.ts:648` — `await this.call((module, handle) => module.messageTick(handle, command.timestampMicros));`
5. `worker.ts:340` — `messageTick` maps to `module._synth_browser_message_tick`.
6. `include/synth/browser/BrowserRuntime.hpp:722` — `MessageTick` stores
   `timestampMicros_` and calls `engine_.MessageThreadTick()`.

A main-thread `setInterval` is clamped in a backgrounded tab and stops entirely
when the page is frozen. So the hold ceiling must be **wall-clock elapsed from a
stored monotonic timestamp**, never a count of ticks — and a frozen tab is then
handled correctly for free, because the first tick after resume sees a large
elapsed and clears.

`design.md`'s "Both modifiers, one mechanism" does say the shared type carries
"the monotonic timestamp the hold began", so the mechanism is right. What is
missing is the reason: nothing in any artifact records why a tick count is
wrong, and task 3.6 says only "evaluated at the existing pump" with no
elapsed-versus-counted wording. That is exactly the kind of premise that gets
optimised away by an executor who reads the task and not the design. **This
belongs in `design.md` under the ceiling decision, in these words, with
`main.ts:249` and `:406` cited.**

### Task 1.3 (host pump rates) — **the deferral is not acceptable as written**

Not because it defers, but because it defers the wrong thing. Three of the three
rates are settleable by reading, and two of them are cited nowhere:

- VST — `app/vst/FroggersPluginProcessor.cpp:312`, `startTimerHz(30);`. Cited.
- Standalone — `External/Sheaf/projects/synth/runtime/Runtime.hpp:335`,
  `startTimerHz(config.uiFrameHz > 0 ? config.uiFrameHz : 30);`. **Not cited
  anywhere.**
- Browser — `browser/src/main.ts:406`, `1000 / 30`. **Not cited anywhere.**

Task 1.3 asserts "`startTimerHz(30)` is verified for the VST only", which is a
§1 violation on its face: two values one grep away are labelled unverified. And
its deliverable is "Report the observed rate per host" — an assertion-free
blank. Meanwhile the premise that actually decides the mechanism, the background
throttle, is neither measured nor written.

**Remedy:** replace 1.3 with the three citations above plus a stated conclusion
("the ceiling is wall-clock, so no host's nominal rate is load-bearing; the
requirement on a host is only that it pumps at all"), and add the throttle
finding to `design.md`.

### Task 1.4 (does a profile rebuild clear a held modifier) — **acceptable, low value, wrong question**

`src/MidiController.cpp:3026-3029` makes fresh `HoldDrillState` and `ShiftState`
by `make_unique`, and `Engine.hpp:995` does `midiProcessors_ = std::move(rebuilt);`.
The processors that hold the raw `shift_` / `holdDrill_` pointers
(`MidiController.hpp:401-402`) are rebuilt in the same operation, so nothing
survives pointing at the old state. Running it is cheap and I would keep the
task, but the question worth asking is the one nobody wrote: *does anything hold
a raw pointer to the old state across the rebuild?* — and the answer, read, is
no. Its deliverable is also a blank ("print the state that proves it"); task 3.8
names the assertion shape, so this one is recoverable.

### The unsatisfiable trigger

`grep -rn 'MidiConnectionManager' app/` → nothing. The VST opens no endpoints,
so "clear when a controller's endpoints open or reopen" is not a trigger on that
host at all. The ADDED requirement states it unconditionally. That is a
behavioural premise the artifacts assert and the tree refutes.

---

## Findings

Numbered fresh. Where a finding restates one of F1–F7, it is because I
re-derived it and it is now mine.

**1 — BLOCKING — the clear-on-reopen mechanism rests on two false claims, and the two bindings are not symmetric.** (F1, plus the asymmetry.) `Engine.hpp` contains no `MidiEndpointOps`, `ExecuteReconcilePlan`, `PlanMidiReconciliation` or `MidiReconcile`; the two production callers are `BrowserMidiBridge.hpp:175` and `MidiConnectionManager.hpp:495`; `MidiReconcile.hpp:109` says the runtime binds the ops to Engine calls. The browser's `ops.openInput` (`BrowserMidiBridge.hpp:126-129`) only enqueues an action and never reaches the engine, while the runtime's (`MidiConnectionManager.hpp:450`) opens a device inline. **Remedy:** name the Engine method and its declaring file; rewrite tasks 3.3 and 3.4 as one mechanism; state where the browser's clear fires given the deferred open.

**2 — BLOCKING — `External/Sheaf/projects/synth/include/synth/runtime` does not exist, and committing the artifacts as written fails an installed gate.** (F2, plus the gate consequence.) Wrong in `proposal.md` Impact, `proposal.md`'s §8.0 scope sentence, `design.md` Context and task 1.2; the real path is `External/Sheaf/projects/synth/runtime/MidiConnectionManager.hpp`. `app/check_artifact_symbols_resolve.py` (run by `app/Makefile:238`) indexes `External/Sheaf` as a path root and reads every tracked live change's `proposal.md` and `tasks.md`. **Remedy:** fix all four, and add the `runtime/` tree to the §8.0 sweep scope, which currently covers a directory that is not there.

**3 — BLOCKING — smi-14 governs Hold Drill, sits on branch #13, and is unnamed.** (F3.) `External/Sheaf/openspec/changes/app-midi-catalog/specs/synth-midi-instrument/spec.md:200-201`. No promoted Sheaf spec covers Hold Drill. #13 is below #14, so amending it rebases #14 and #15; task 2.2 rebases #15 only. **Remedy:** name smi-14, add an amend task on #13, extend 2.2 to both branches.

**4 — BLOCKING — the overlap table names 6 of 12 active changes, one of the 6 is archived, and the live document collision is unnamed.** (F4, re-run.) `frogg3rs-stereo-field-and-density` holds `MANUAL.md` and `QUICK_DICT.md` modified in the working tree and edits `MANUAL.md` in two unchecked tasks; this change's tasks 7.1 and 7.3 edit exactly those files. Task 7.2's coordination partner `frogg3rs-effect-page-hierarchy` was archived 2026-09-12. `ui-state-before-audio` carries a PINNED constraint at `Engine.hpp:576-580`, four lines below the loop the ceiling adds work to, and edits `Engine.hpp`. **Remedy:** re-run the enumeration, state a disposition for all twelve including "nothing needed", and replace task 7.2 with coordination against the change that actually holds the documents.

**5 — BLOCKING — Impact omits two edited definition sites.** (F5.) `External/Sheaf/projects/synth/include/synth/ControllersPageUI.hpp` and `External/Sheaf/projects/synth/src/MidiConfigViewModel.cpp`, where tasks 4.3, 4.4, 5.3 and 6.3 land. **Remedy:** add both, with the symbols they hold (`EndpointStatusColor` at :793-801; `DeviceLabel` at :600-618).

**6 — BLOCKING — the substance is Sheaf code with no Sheaf artifact.** (F6.) Modifier lifetime, mismatch reporting, the per-slot field and the template rate limit all live in the submodule and are governed by `synth-midi-instrument` and `synth-runtime-ui`. The change writes no delta for either and no Sheaf proposal or design. After task 2.1 removes smi-16, the synth has no stated requirement for modifier lifetime at all. **Remedy:** see the shape verdict — this one does not repair in place.

**7 — BLOCKING — `~/Desktop/midi-log/` does not exist, so the proposal asserts a logger that is not there and task 6.4 replays a file that is not there.** Verified three ways: `ls ~/Desktop/midi-log/` → No such file or directory; `ls -d ~/Desktop/*midi*` → no matches; `ls -d ~/Desktop/*log*` → no matches. `proposal.md`'s Open question says "A logger at `~/Desktop/midi-log/` retains any recurrence". Task 6.4 says "Replay the captured Launch Control XL flood from `~/Desktop/midi-log/` … from the file, with no hardware required." There is no file, and the task's deliverable is "report the number" with no assertion — a blank that gets filled toward green. **Remedy:** either produce the capture and cite it by path and size, or rewrite 6.4 to synthesise the documented byte sequence in the test and state the assertion ("other addresses on that controller continue to be processed; page updates are bounded to N per second"), and delete the logger claim from the proposal.

**8 — BLOCKING — the mappings delta's press-only scenario contradicts this change's own second-press clear.** The scenario reads: "**WHEN** the Twister's side buttons send press edges with no release edges / **THEN** Bank Next, Play, Freeze, Scene 1 and Randomize Page each still fire on their press." CC 13 is Shift (`app/FroggersMidiCatalog.hpp:114`). Its press edge sets `shift_->held` (`src/MidiController.cpp:969`), and under task 3.5 the next CC 13 press clears it — so the profile alternates, and the five buttons fire their **shifted** jobs on every other Shift press. The scenario asserts the unshifted outcome unconditionally. The captured failure was CC 13 transmitting *nothing*, which is a different premise. **Remedy:** state the premise that was actually observed (the Shift address silent), or state what the five buttons do while Shift is latched, and make the two consistent.

**9 — SHOULD-FIX — `Engine.hpp:570-574` is not "the only pump site", and the three-host fan-out it hides is what the ceiling depends on.** The pump is `MessageThreadTick()` at `Engine.hpp:497`, called from `browser/BrowserRuntime.hpp:722`, `runtime/Runtime.hpp:944` and `app/vst/FroggersPluginProcessor.hpp:378,401`. :570-574 is the output-processor loop inside it. **Remedy:** cite the symbol and its three callers.

**10 — SHOULD-FIX — the ceiling's hook, its constant and its clock are named nowhere, though all three exist.** The state is reachable at `Engine.hpp:570` (the loop variable is `MidiControllerProfileResult&`, which holds `holdDrill` at `MidiController.hpp:961` and `shift` at :962); the clock is `MidiInProcessor::TimestampProvider` (:72, setter :79, `NextTimestamp()` :82) threaded from `Engine::timestampProvider_` (:1462) through `Engine.hpp:991`. §9 requires every constant, helper, type, predicate and sentinel the plan creates to be named. Two of them — the shared held-state type and the clear-source record — have no name at all, so no executor-side enumeration is possible for them. **Remedy:** name the type, the constant, the clear-source enumeration and the clock, and cite the threading.

**11 — SHOULD-FIX — one of the four "independent" triggers does not exist on the VST.** `grep -rn 'MidiConnectionManager' app/` → nothing. The ADDED requirement states endpoint reopen unconditionally, and the scenario "A modifier held at disconnect does not survive the reconnect" is not expressible there. **Remedy:** say which triggers are live on which host, and which scenario is checked where.

**12 — SHOULD-FIX — the exhaustive-consumer count is wrong and task 1.5 defers a question the structure answers.** `design.md` says "three consumers switch exhaustively"; there are two dispatch sites over three enumerators (`ControllersPageUI.hpp:793-801`; `MidiConfigViewModel.cpp:600-618`). Since the design's own decision is to add no enumerator, no consumer changes, and 1.5 has nothing to determine. **Remedy:** state the two sites and the conclusion in `design.md`; delete 1.5 or reduce it to a drift check.

**13 — SHOULD-FIX — `Engine.hpp:990` is "the only rebuild site" only within the Engine.** `CreateMidiControllerProfile` is also called at `src/MidiController.cpp:3307`, `:3335`, `:3392` and twice in `tests/controllers_page_ui_tests.cpp`. All construct fresh state, so the clear is safe — but an executor reading "the only rebuild site" will not check. **Remedy:** qualify it, and state the disposition for the three factories.

**14 — SHOULD-FIX — `resync` is a fifth seam the change never considers.** `MidiEndpointOps` runs to `MidiReconcile.hpp:122`; `resync` at :121 is bound in both places to `engine_.ResetMidiOutputProcessors(ix)`. A resync is a plausible moment to clear held state and is not discussed. **Remedy:** rule on it either way, in `design.md`.

**15 — SHOULD-FIX — task 3.4's drift check covers two bindings; there are five sites.** Two production (`BrowserMidiBridge.hpp:125-171`, `MidiConnectionManager.hpp:449-462`) and three test (`tests/reconcile_executor_tests.cpp:93-111` full, `tests/reconcile_tests.cpp:657-659` and `tests/controllers_page_ui_tests.cpp:1462-1466` partial). A check written over "both bindings" must say what it does about the fakes, or it will pass on a fake that never gained the behaviour. **Remedy:** state the check's subject precisely.

**16 — SHOULD-FIX — 22 scenarios, 0 `Check:` lines, and one silent reversal of a promoted requirement.** The ADDED delta carries 17 scenarios and the MODIFIED requirement 5, none with a `Check:`. `app/check_spec_checks_resolve.py` validates only the lines that exist — it currently reports `OK - 64 Check reference(s) resolved, 9 declared as having no automated check` — so nothing catches an absent one. Separately, `app/check_modified_requirements_restate_promoted.py` reports the delta dropping the promoted scenario "A missing release leaves Shift held until the next press and release" as a NOTE and passes. The proposal's sentence "Sheaf's `synth-midi-instrument` is NOT modified here" frames smi-16 as the only home of the behaviour being reversed; `openspec/specs/froggers-midi-controller-mappings/spec.md` states it too, and the delta reverses it without saying so. **Remedy:** declare the reversal in the proposal, and mark each scenario with its check or plainly as not yet delivered at authoring time rather than at 8.3.

**17 — SHOULD-FIX — task 4.5 implements an ignore list no requirement covers.** `design.md`'s Risks says declared preconditions carry one; the requirement "A preset declares the device settings it depends on" says nothing about ignoring addresses, and no scenario covers it. **Remedy:** add the clause and a scenario, or drop 4.5.

**18 — SHOULD-FIX — task 1.6's gate baseline omits the frogg3rs gates and the browser binding-site test.** The change edits `app/FroggersMidiCatalog.hpp` and `MANUAL.md`, and `app/` holds five check scripts (`check_artifact_symbols_resolve.py`, `check_citations_resolve.py`, `check_spec_checks_resolve.py`, `check_modified_requirements_restate_promoted.py`, `check_docs_match_parameter_table.py`, plus `check_no_planning_history.py`) that `app/Makefile` runs. 1.6 names only the Sheaf `test` target, the miniapp target and `build-launcher.sh`, and misses `browser-midi-bridge-test`, which covers one of the two bindings task 3.4 is about. **Remedy:** baseline all of them.

**19 — NOTE — two task deliverables are assertion-free blanks.** 1.3 "Report the observed rate per host" and 6.4 "report the number". Both name a measurement with no threshold, which is the defect the brief warns about: the blank gets filled toward green. 3.8 is the counter-example done right — it states what the check must show.

**20 — NOTE — `QUICK_DICT.md` touches MIDI nowhere.** `grep -i 'shift\|hold drill\|twister\|midi' QUICK_DICT.md` → 0. Task 7.3's QUICK_DICT half is a no-op as written; its README half resolves (`README.md:110-112`). Harmless, but it puts the file in the change's blast radius against the change that currently holds it modified.

**21 — NOTE — citation hygiene.** `MidiController.hpp:250-258` begins inside `HoldDrillState` (opener at :249); `MidiReconcile.hpp:114-120` truncates `MidiEndpointOps` two lines early. §1 prefers the symbol to the line for exactly this reason.

---

## Shape verdict

**This change should not exist in this shape. Split it, and sequence the halves
differently.**

The arithmetic is not close. Of eleven definition sites — the nine the proposal
lists plus the two Finding 5 adds — **nine are in `External/Sheaf`**. Of 42
tasks, groups 2, 3, 4 and 6 plus task 5.3 are Sheaf code: **26 tasks**. The
behaviours are governed by Sheaf's `synth-midi-instrument` (smi-14 on #13,
smi-16 on #14) and `synth-runtime-ui` (deltaed by eight active Sheaf changes).
The change writes no Sheaf proposal, no Sheaf design and no Sheaf delta, and
asserts all of it in SHALL form in a frogg3rs capability instead. Findings 1, 3,
6 and 11 are not four separate defects; they are the same defect seen from four
sides — the work is being planned in the wrong repository, so its requirements
land where nothing implements them and its mechanisms are described from a
vantage point (`Engine.hpp`) that does not own the seams.

The frogg3rs half is small, real, and blocked only by a document collision:
declaring the Twister's and APC40's preconditions as data, correcting
`MANUAL.md:319-320`, and generating the settings table from the declarations
with a drift gate. That is tasks 5.1, 5.2, 5.4 and 7.1 — four tasks, two files,
one new gate — and it is the part that answers the operator-visible complaint
that "nothing states the requirement where the preset is chosen."

**Recommended shape:**

1. **A Sheaf change** owning modifier lifetime, mismatch detection and reporting,
   and inbound template-change handling. Written against `synth-midi-instrument`
   and `synth-runtime-ui`, amending smi-14 on #13 and smi-16 on #14 as part of
   its own delta rather than as a foreign errand, and sequenced explicitly on the
   #13 → `rework-controllers-block-editing` → #14 → #15 stack with
   `ui-state-before-audio`'s pinned `Engine.hpp` constraint named. It carries the
   browser-throttle premise, names the shared type, the ceiling constant, the
   clear-source record and the clock, and states per host which of the four
   triggers is live.
2. **A frogg3rs change** — declared preconditions, the manual correction, the
   drift gate — sequenced after `frogg3rs-stereo-field-and-density` releases
   `MANUAL.md` and `QUICK_DICT.md`, and after the Sheaf change lands the
   `MidiAppDeviceDefault` field it populates.

Splitting this way also fixes Finding 2 for free: the Sheaf change's paths are
written from `projects/synth/`, where `runtime/MidiConnectionManager.hpp` is the
natural spelling and the wrong one is not available to transcribe.

A third rewrite of the current single change would be the third artifact in a
row authored against a mechanism the Engine does not have, in a repository that
does not own the code.
