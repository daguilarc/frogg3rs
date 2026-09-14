# Second preflight audit — `frogg3rs-midi-controller-resilience`

Run 2026-09-10 in a fresh context that did not write either draft, against
`/Users/diegoaguilar-canabal/Desktop/frogg3rs` (submodule `External/Sheaf` at
`ba3898e4`, branch `launchpad-model-on-the-row`). Read-only; no builds, no
checkouts, no working-tree writes except this file. Network was available and
used once, to confirm the three Sheaf PR numbers.

This pass audits the rewrite as a fresh proposal, per §9's own warning that an
audit which rewrites a proposal has produced an unaudited proposal. Nothing the
first audit passed was taken on trust: every file:line was opened again.

---

## VERDICT

**REJECT.**

The rewrite is a large, real improvement — B1, B5, B6, B7 and B8 are genuinely
answered, the Impact list went from decorative to mostly accurate, and the
Sheaf sequencing claims (#13 owns `MidiAppDeviceDefault`, #14 owns smi-16, #15
stacks on #14) are all true and were verified from the repo. But the change
still cannot be executed. Its headline mechanism — clear-on-endpoint-reopen —
was relocated out of `MidiReconcile` onto a structural claim that one grep
refutes: `Engine.hpp` contains no `MidiEndpointOps`, no `ExecuteReconcilePlan`
and no `PlanMidiReconciliation`, so the Engine neither owns the binding nor
applies the actions the design says it applies, and tasks 3.3 and 3.4 now
instruct the executor to do two different things. B4 changed address, not
substance. Alongside it, the Impact section and the §8.0 sweep are scoped to
`include/synth/runtime`, a directory that does not exist — so the real
`runtime/` tree holding one of the two binding sites goes unswept, which is
exactly the failure B3 rejected the first draft for. Bringing Hold Drill into
scope (the correct answer to B6) created a second unnamed requirement
collision: smi-14 is ADDED by `app-midi-catalog` (#13), sits one branch below
#14 in the stack, and defines Hold Drill's state lifetime as press-and-release
— the same shape as smi-16, unnamed in every artifact, with no amend task and
no rebase coverage. And the §9 overlap enumeration is still partial: fourteen
changes are active across the two trees, six are named, and one of the eight
omitted (`ui-state-before-audio`) is PINNED by comment inside
`Engine::MessageThreadTick` — the very function the hold ceiling is to be
evaluated in.

---

## B1–B8 disposition

| # | First audit's finding | Disposition | Evidence |
|---|---|---|---|
| **B1** | Contradicts live SHALL smi-16, never named | **ANSWERED** | smi-16 is named in the overlap table, in the design decision "Amend smi-16 rather than modify it", in the proposal's "Sheaf's `synth-midi-instrument` is NOT modified here", and in task 2.1. Verified: smi-16 exists only at `External/Sheaf/openspec/changes/shift-and-file-export/specs/synth-midi-instrument/spec.md:195`, under `## MODIFIED Requirements` (section opens :3, ADDED opens :192 — so smi-16 at :194 is a MODIFIED entry); `grep -rn "release never arrives" External/Sheaf/openspec/specs/` returns nothing, so it is unpromoted. Branch `shift-and-file-export` = `ddb14693`, open as PR #14. Amending it before it ships is coherent. **But see F3 — the identical collision for Hold Drill is unnamed.** |
| **B2** | §9's active-change enumeration never run | **PARTIALLY ANSWERED** | A table of six with dispositions now exists and each of the six is accurate. My own `openspec list` returns **4 frogg3rs + 10 Sheaf = 14** active changes. Eight are unnamed and undisposed. See "Overlap enumeration" below and F4. |
| **B3** | Impact omits every definition site the change edits | **PARTIALLY ANSWERED** | Impact now names `MidiController.hpp`, `MidiController.cpp`, `Engine.hpp` (both sites), `MidiAppCatalog.hpp`, `MidiReconcile.hpp`, both binding sites, `MidiConfigViewModel.hpp`, `app/FroggersMidiCatalog.hpp`, `MANUAL.md` — a genuine repair. Three holes remain: the `MidiConnectionManager.hpp` path is wrong and the sweep scope names a nonexistent directory (F2); `ControllersPageUI.hpp` was **dropped** from Impact although tasks 4.4, 5.3 and 6.3 all render on that page (it was correctly present in the rejected draft); `MidiConfigViewModel.cpp` — where rows are built and the new field must be populated — is named nowhere. |
| **B4** | `MidiReconcile.cpp` cannot do the job Impact assigns it | **NOT ANSWERED** | See F1. The file-level error is gone (Impact now cites `MidiReconcile.hpp:114-120`, the seam, not `MidiReconcile.cpp`). The mechanism was relocated to "the Engine's action application" on the stated ground that "the Engine already owns both the processors and the `MidiEndpointOps` binding". `grep -n "MidiEndpointOps\|ExecuteReconcilePlan\|PlanMidiReconciliation" External/Sheaf/projects/synth/include/synth/Engine.hpp` → **no matches**. |
| **B5** | MODIFIED against a capability that does not exist; no ordering task | **ANSWERED** | `openspec/changes/archive/2026-09-11-frogg3rs-midi-shift/` exists and `openspec/specs/froggers-midi-controller-mappings/spec.md` is present, so the MODIFIED delta has a real target. Requirement header matches byte-for-byte. Body diffed sentence by sentence: 1, 2 and 4 identical; 3 deliberately reworded ("SHALL continue to require CC Hold" → "SHALL declare CC Hold … as a device precondition … and SHALL keep working in its unshifted form when that precondition is unmet"). The Sheaf half is dissolved rather than ordered, by amending on the branch instead of writing a delta — a better answer than the one B5 asked for. Caveat under Artifact staleness: the archive and the promotion are **uncommitted** working-tree state. |
| **B6** | `HoldDrillState` has identical exposure and the requirement already binds it | **ANSWERED** | Both modifiers are now in scope throughout: proposal "What Changes" bullet 1, design decision "Both modifiers, one mechanism", Goals ("for every held modifier the synth keeps"), tasks 3.1, 3.7, 3.8, and two dedicated scenarios ("A stuck Hold Drill does not cost every knob", "The two modifiers clear independently"). Verified the family is real: `HoldDrillState` at `MidiController.hpp:249-252`, `ShiftState` at `:254-258`, written in adjacent branches of one function at `:955-961` / `:964-970`, constructed adjacently at `:3026` / `:3028`. The answer is correct — and it is what produced F3. |
| **B7** | Mismatch state has no definition site; BREAKING label points at nothing | **ANSWERED** | The BREAKING label is gone; the proposal now says "that enum is not extended, so its exhaustive consumers are untouched and nothing here is breaking". The design carries a decision ("Mismatch is a new per-slot field, never a fourth enum value") with the orthogonality argument. Task 4.3 names the site. Verified the conclusion independently: `MidiConfigViewModel.hpp:351-352` are exactly `inputStatus` / `outputStatus`, and a sibling field beside them changes nothing about `enum class MidiEndpointStatus { Unconfigured, Offline, Online }` (`MidiReconcile.hpp:12`). **Consumer count, mine:** exhaustive `switch` over the enum = **1** — `EndpointStatusColor`, `ControllersPageUI.hpp:791-800` (cases at :795, :797, :799). Plus one parallel three-entry label table at `ControllersPageUI.hpp:3347-3352` (a §5 drift family with the switch), and non-exhaustive comparison consumers: `DeviceLabel` (`MidiConfigViewModel.cpp:600-615`), `ControllersPageUI.hpp:857` and `:872`, ~16 `==`/assignment sites in `src/MidiReconcile.cpp`, and six test files. The design's "three consumers switch exhaustively over that enum" is a **wrong count** (F8) with a right conclusion. |
| **B8** | Task 1.4 asked the operator to do something the proposal says is impossible | **ANSWERED** | 1.4 is now "confirm by running that a profile rebuild (`Engine.hpp:990`) actually clears a held modifier … Demonstrate the modifier held first and print the state that proves it." No hardware, no Midi Fighter Utility, and a §6.1 positive control built in. One gap: the task does not say what to do if rebuild turns out not to clear — §1 wants a task "written to act on whichever answer comes back". |

---

## Citation audit

Every file:line in the four artifacts, opened and read. Working-tree state.

| Citation | Where used | Verdict | What the file says |
|---|---|---|---|
| `MidiController.hpp:250-258` | proposal Impact, design Context | **OFF BY ONE (top)** | `struct HoldDrillState {` opens at **:249**; :250 is `bool held = false;`. `ShiftState` runs :256-258. The correct range for "`HoldDrillState` and `ShiftState`, adjacent declarations" is **:249-258**. This is the identical defect the first audit filed as S2 against `:957-961`; S2 was fixed and the same error reappeared one file over. |
| `MidiController.cpp:955-961` | proposal Impact, design Context | ✓ | :955 `if (isPress) {` … :961 `return;`, containing the writes `holdDrill_->held = true;` (:956) and `= false;` (:959). S2 correctly repaired. |
| `MidiController.cpp:964-970` | proposal Impact, design Context | ✓ | :964 `if (association->press.type == MessageIn::Type::Shift) {` … :969 `shift_->held = isPress;` … :970 `}`. |
| `MidiController.cpp:975` | proposal Impact, design Context | ✓ | `const bool shifted = shift_ != nullptr && shift_->held && association->shiftedPress.has_value();` |
| `MidiController.cpp:3026-3029` | proposal Impact, design Context, task 1.4 | ✓ | :3026 `result.holdDrill = std::make_unique<HoldDrillState>();`, :3028 `result.shift = std::make_unique<ShiftState>();`, with the raw-pointer captures at :3027 and :3029. Both modifiers, exactly as claimed. |
| `Engine.hpp:990` | proposal Impact, tasks 1.4, 3.2 | ✓ | `rebuilt.push_back(CreateMidiControllerProfile(...))` inside `RebuildMidiProcessors()`. "The only profile rebuild site" holds: it is the only non-test `CreateMidiControllerProfile` call in `Engine.hpp`, and `midiProcessors_ = std::move(rebuilt)` at :995 is the single assignment. |
| `Engine.hpp:570-574` | proposal Impact, design Decisions, task 3.6 | ✓ **as a location, wrong as a hook** | The loop is exactly `for (MidiControllerProfileResult& processors : midiProcessors_) { for (auto& output : processors.outputs) { output->Process(); } }`. It walks **output** processors. `ShiftState`/`HoldDrillState` are written from the **input** chain and no output processor holds either pointer. "Evaluated at the existing pump" therefore names a place, not a mechanism. See F7. |
| `MidiAppCatalog.hpp:31-38` | proposal Impact, design Decisions, task 2.3 | ✓ exact | `struct MidiAppDeviceDefault { id; displayName; kind; inputAliases; outputAliases; config; };` — :31 through :38. |
| `MidiReconcile.hpp:114-120` | proposal Impact, design Context | ✓ with truncation | `struct MidiEndpointOps {` at :114; the struct runs to :122, so :114-120 omits `resync` (:121) and the close. `resync` is the member most like a clear-on-reopen hook, so the truncation is not harmless framing. |
| `include/synth/runtime/MidiConnectionManager.hpp` | proposal Impact, design Context, §8.0 sweep scope, tasks 1.2, 1.7, 8.1 | **PATH DOES NOT EXIST** | `ls External/Sheaf/projects/synth/include/synth/runtime` → No such file or directory. The only subdirectory of `include/synth/` is `browser/`. The real file is `External/Sheaf/projects/synth/runtime/MidiConnectionManager.hpp` (binding at :449, `ExecuteReconcilePlan` at :495). See F2. |
| `browser/BrowserMidiBridge.hpp:125-171` | proposal Impact, design Context, task 3.4 | ✓ with truncation | `synth::MidiEndpointOps ops;` at :125; the bindings run to :174 (`ops.resync` opens :171 and closes :174) and `ExecuteReconcilePlan` is called at :175. The cited range again stops mid-`resync`. |
| `MidiConfigViewModel.hpp:351-352` | proposal Impact, design Context, task 4.3 | ✓ exact | `MidiEndpointStatus inputStatus = …;` / `outputStatus = …;` |
| `app/FroggersMidiCatalog.hpp:14-24` | proposal Why + Impact, task 5.1 | ✓ exact | The Twister precondition block: relative encoders "Enc 3FH/41H" (:15), all six side buttons "CC Hold" (:16-17), "Bank Side Buttons unchecked" (:18-19), and at :23-24 "CC Hold is required on every side button because the release is what ends Shift." |
| "the same comment block" (task 5.2, APC40) | task 5.2 | **WRONG BLOCK** | The APC40 caveat is a separate block at `app/FroggersMidiCatalog.hpp:26-35`, not :14-24. The first audit filed this as S5; it is unrepaired, with the task number shifted from 5.3 to 5.2. |
| `MANUAL.md:319-320` | proposal Why + Impact, task 7.1 | ✓ | ":319 … If the controller is unplugged while Shift is still held, its / :320 buttons stay shifted until Shift is pressed and released again." |
| `MANUAL.md:336-339` | proposal Why + Impact | ✓ | The Midi Fighter Utility paragraph naming all three settings. Note :338 reads "CC 8 to 13 on channel 4 (channel 3 counted from 0)" — a third channel convention. See F9. |
| `app/vst/FroggersPluginProcessor.cpp:312` | design Decisions, task 1.3 | ✓ and correctly scoped | `startTimerHz(30);`, guarded at :311 by `if (juce::MessageManager::getInstanceWithoutCreating() != nullptr)`. The rewrite now says "which is 30 Hz in the VST" and task 1.3 says "verified for the VST only" — S8's first half repaired. |
| `ControllersPageUI.hpp` | — | **DROPPED** | Present in the rejected draft's Impact, absent from the rewrite's, although tasks 4.4, 5.3 and 6.3 all add rendering to that file. |
| `MANUAL.md` dirt | — | ✓ still safe | `git diff -U0 MANUAL.md` — first hunk at line **524**. Every cited line is below the dirt and unchanged. |

---

## Overlap enumeration, mine not theirs

`openspec list` run in both trees by this pass.

**frogg3rs (4 active):** `frogg3rs-midi-controller-resilience` 1/42 (this change);
`frogg3rs-effect-page-hierarchy` 6/34; `frogg3rs-launchpad-variant` ✓ Complete,
unarchived; `frogg3rs-launchpad-port-aliases` ✓ Complete, unarchived.

**Sheaf (10 active):** `launchpad-model-on-the-row` 11/14; `shift-and-file-export`
✓ Complete; `ui-state-before-audio` 4/7; `app-midi-catalog` 26/28;
`shorten-deadline-readout-window` 0/6; `fix-out-of-tree-app-gaps` ✓ Complete;
`browser-slider-value-readout` 1/7; `bank-addressed-absolute-write` 5/7;
`rework-controllers-block-editing` 22/28; `fix-task-analyzer-plan-derived-tasks`
✓ Complete.

**Fourteen active. The proposal names six.** Every one of the six is accurately
described and its disposition is defensible, with one exception noted below.
The eight omitted, with the disposition §5 requires and the proposal does not
give:

| Omitted change | Repo | State | Delta capabilities | Disposition this pass assigns |
|---|---|---|---|---|
| `ui-state-before-audio` | Sheaf | 4/7 | `synth-runtime-ui` | **Real overlap, and the sharpest of the eight.** `Engine.hpp:576-580` carries a comment reading "ui-state-before-audio (design \"Mechanism\", PINNED)" **inside `MessageThreadTick`, four lines below the pump loop at :570-574** where this change evaluates its hold ceiling. A change with a PINNED design constraint in the function you are adding work to is not a "note only". |
| `shorten-deadline-readout-window` | Sheaf | 0/6 | `synth-runtime-ui` | Overlaps task 1.6, which baselines "the two carried 96 kHz deadline failures". A change whose whole subject is the deadline readout window moves that baseline. Name it and say whether 1.6's expected failures are stable. |
| `browser-slider-value-readout` | Sheaf | 1/7 | `synth-runtime-ui` | Same page capability; at 1/7 probably no collision. Say so. |
| `fix-out-of-tree-app-gaps` | Sheaf | ✓ Complete | `synth-runtime-ui`, `synth-app-runtime`, `synth-portable-visualizers`, `synth-portable-runtime-shell` | Complete-unarchived and open as PR #9. Four capabilities including the page. Say so. |
| `fix-task-analyzer-plan-derived-tasks` | Sheaf | ✓ Complete | `task-analyzer-data-gathering` | No overlap. Nothing needed. |
| `frogg3rs-launchpad-variant` | frogg3rs | ✓ Complete, unarchived | `froggers-sheaf-runtime-app` | Touches the Launchpad presets in `app/FroggersMidiCatalog.hpp`, the file tasks 5.1/5.2 edit. Complete-unarchived means it archives before or after this one and the ordering matters. |
| `frogg3rs-launchpad-port-aliases` | frogg3rs | ✓ Complete, unarchived | `froggers-sheaf-runtime-app` | Same file, same ordering question. Also the downstream half of Sheaf `launchpad-model-on-the-row` task 6.2 ("In `frogg3rs`: set the model on the three Launchpad presets"). |
| — | — | — | — | — |

**The one named disposition that does not survive checking:**
`rework-controllers-block-editing` is presented as in-flight at 22/28 and tasks
2.4 sequences the Controllers page work after it. It has **no branch, local or
remote** (`git branch -a` lists none), and its change directory last moved at
`5b6d1cb7`, which **is an ancestor of both `main` and `origin/main`**. Its code
has already landed upstream. Its six outstanding tasks are a random-simulation
oracle harness (5.1-5.4) and two cleanups (6.2, 6.3) that nobody is currently
producing on any branch. "Sequence after it" is therefore a dependency on a
state that already exists on one reading and will never arrive on the other.
Reading its row model is right; waiting for it is not.

---

## Sheaf sequencing claims

All three verified from the repo; the PR numbers confirmed once over the
network (`gh pr list --repo jvictor0/Sheaf`).

- **The stack is linear and exactly as claimed.**
  `app-midi-catalog` (`b50cca18`) ⊂ `shift-and-file-export` (`ddb14693`) ⊂
  `launchpad-model-on-the-row` (`ba3898e4`), each confirmed with
  `git merge-base --is-ancestor`. All three local tips equal their `fork/` tips.
- **PR numbers:** #13 `app-midi-catalog`, #14 `shift-and-file-export`, #15
  `launchpad-model-on-the-row` — all OPEN against `jvictor0/Sheaf`, all from
  `daguilarc`. ✓ exactly as the proposal states.
- **#13 owns `MidiAppDeviceDefault`:** ✓. `git log --diff-filter=A` on
  `MidiAppCatalog.hpp` returns `0ddf2eba` ("Let an app supply the MIDI
  catalog"), which is not an ancestor of `main`. Task 2.3 lands the
  preconditions field on the right branch.
- **#15 stacks on #14, so an smi-16 edit rebases through it:** ✓. Task 2.2
  covers it.
- **smi-16 is unmerged:** ✓. It appears only in #14's delta; nothing matching
  in `External/Sheaf/openspec/specs/`.

**Where the sequencing breaks (F3).** `smi-14 — Hold Drill: momentary drill-in
gate on a held button` is **ADDED by `app-midi-catalog`**, at
`External/Sheaf/openspec/changes/app-midi-catalog/specs/synth-midi-instrument/spec.md:200-201`
(the `## ADDED Requirements` section opens at :172). It reads: held is "set by
that button's press and release … the button's press SHALL set held and clear
every drilled flag, and **its release SHALL clear held**", and "on release,
every mapping SHALL resume its ordinary … turn behavior". Hold Drill has no
promoted requirement anywhere — `grep -rn "Hold Drill\|HoldDrill"
External/Sheaf/openspec/specs/` returns nothing — so smi-14 is the requirement
that defines the lifetime this change now rewrites for Hold Drill. It is
unnamed in `proposal.md`, `design.md`, `tasks.md` and both deltas. The change's
own rule ("amend on the unmerged branch rather than modify by delta") applies
to it identically, and it lives **one branch lower in the stack**, so amending
it forces rebasing #14 *and* #15. Task 2.2 rebases only #15, through only the
#14 amendment.

---

## Forward enumeration (§5, run forward on every named concept)

Case-insensitive, by operand, across `app/`,
`External/Sheaf/projects/synth/src`, `.../include/synth` (which contains
`browser/`), `.../runtime` — substituted for the artifacts' nonexistent
`include/synth/runtime` — and both `openspec/` trees. `node_modules/` excluded.
The change's own directory excluded from FOUND so it does not count itself.

**§6.1 control on the instrument:** the first run of this sweep returned 0 for
every operand including `ShiftState`, which is false. Cause: zsh does not
word-split an unquoted variable, so the directory list collapsed to one
nonexistent path. Re-run with an array; the control operand `ShiftState` then
returned 33 hits across 10 files. The numbers below are from the live run.

CHANGED is 0 for every concept: nothing is implemented, this is a preflight.

| Concept the rewrite creates | Operands grepped | FOUND, domain sense | FOUND, any sense | CHANGED | Disposition |
|---|---|---|---|---|---|
| The shared held-state type | `HeldState`, `heldmodifier`, `held-modifier`, `held modifier`, `HeldButton`, `ShiftState`, `HoldDrillState` | **0** (no shared type exists) | `HeldState` 0; `HeldButton` 12 in 6 files; `ShiftState` 33/10; `HoldDrillState` 29/12; "held-modifier" 4 archived openspec files, unrelated senses | 0 | The family to collapse is real and is exactly two members, built by one helper. **The type is never given a name in any artifact**, so §9's "list every … type by name" is unmet and no executor-side enumeration is possible for it. |
| The clear-source record | `clearsource`, `clearedby`, `ClearReason`, `clear source` | **0** | **0** outside this change's own files | 0 | Genuinely new. Unnamed in the artifacts. |
| The hold ceiling | `holdceiling`, `hold ceiling`, `ceiling` | **0** | `hold ceiling` 1 file — `app/dsp/Drive.hpp` ("Decay/Hold ceiling", a reverb gain limit); bare `ceiling` 709 hits/90 files, all DSP or channel-count limits | 0 | Genuinely new, and colliding in prose with a DSP concept in the same repo. The first audit advised naming it away from that sense; the rewrite names it not at all. |
| The declared-preconditions field | `precondition`, `MidiAppDeviceDefault` | **0** | `precondition` 71 hits/27 files, every one the doc-comment "Precondition:" sense | 0 | No data-shaped precondition concept exists. Prose collision only. Field lands on #13 per task 2.3. |
| The observed-address set | `observedaddress`, `observed address`, `observed` | **0** | `observedaddress`/`observed address` 0 outside this change; bare `observed` 238/103, all unrelated (clock sampling, CAS comments) | 0 | Genuinely new. Nothing in the input chain records inbound addresses today. |
| The mismatch report field | `mismatch`, `unmapped`, `MidiEndpointStatus` | **0** | `mismatch` 184/103, none a per-slot device concept; `unmapped` 77/44, including `synth-midi-instrument`'s "Unmapped active controller still supplies clock" — a *different* sense (a slot with no mappings) | 0 | Genuinely new. The `unmapped` prose collision the first audit filed as N3 is unrepaired; the resilience delta uses "unmapped" for an address the profile does not cover. |
| The ignore list | `ignorelist`, `ignore list` | **0** | 2 files, both unrelated (a CI path filter in an archived frogg3rs proposal; a path ignore list in `sheaf-chat-scoped-tools`) | 0 | Genuinely new in this domain. |
| The token bucket | `tokenbucket`, `token bucket`, `bucket`, `ratelimit`, `rate-limit`, `rate limit`, `throttl` | **0** | `tokenbucket`/`token bucket` 0 outside this change; `bucket` 86/24 (FFT bins, switch quantization); `ratelimit`/`rate-limit` 3 files, all unrelated specs; `throttl` 153/53, including `Engine`'s outbound UI-publish throttle | 0 | Not a duplicate of the UI-publish throttle: that one bounds *outbound* publication by audio-block count, this one bounds *inbound* per-controller SysEx by wall time. Different operand, different sink. Classified and kept separate — but the design does not mention the adjacent mechanism at all, so nothing records that the classification was made. |
| Inbound template change | `templatechange`, `template change` | **0** | 0 in source outside this change | 0 | Genuinely new. |

**The plan's own enumeration is narrower than §9 requires.** Task 1.2 schedules
a forward sweep over the held-modifier family only — twelve operands, all of
them Shift/Hold Drill. Seven further concepts are created by tasks 4, 5 and 6
and none gets a preflight enumeration; task 8.1 defers the work to postflight
("Re-run §5's enumeration against the diff"). §9 asks for it forward, before
approval, on every named concept. This pass has now run it and every row is
zero, so the substantive risk is low — but the proposal does not carry the
result, and two of the concepts have no name to enumerate by.

---

## Findings

### F1 — BLOCKING — B4 changed address, not substance: the Engine owns neither the binding nor the action application

`design.md`, Decisions: "**Clear from the Engine, not from the planner.**
`MidiReconcile` emits `OpenInput`/`OpenOutput` actions; the Engine already owns
both the processors and the `MidiEndpointOps` binding. The clear hangs off the
Engine's action application."

Both halves of that sentence are false, by one grep each:

- `grep -n "MidiEndpointOps\|ExecuteReconcilePlan\|PlanMidiReconciliation"
  External/Sheaf/projects/synth/include/synth/Engine.hpp` → **no matches.**
- The only two production callers of `ExecuteReconcilePlan` are
  `include/synth/browser/BrowserMidiBridge.hpp:175` and
  `runtime/MidiConnectionManager.hpp:495`. Neither is the Engine. The Engine has
  no "action application".

The Engine does own the processors, and that half is fine — but it exposes no
route to the state. Its per-slot surface is `MidiControllerCount()` (:731),
`MidiInputProcessor(ix)` (:738-741, returns `midiProcessors_[ix].input.get()`)
and `ResetMidiOutputProcessors(ix)` (:840-847). Nothing reaches
`midiProcessors_[ix].shift` or `.holdDrill`.

The two tasks then disagree about what to build. Task 3.3: "driven from the
Engine's application of `MidiReconcile`'s open actions, not from the planner,
which holds no Engine handle." Task 3.4: "Cover both `MidiEndpointOps` binding
sites … and add a check that fails when one binding clears the state and the
other does not." Those are different mechanisms. An executor receiving both
will pick one and the other's check will be written against nothing.

§1's corollary is the relevant one: a structural instruction that forces a
worse structure is suspect, not the implementer. The structure the code admits
is plain — both binding sites already hold an Engine handle (`BrowserMidiBridge`
calls `engine_.EditInstrument(...)` at :154 and `engine_.ResetMidiOutputProcessors(ix)`
at :172), so a new Engine method called from both `ops.openInput` and
`ops.openOutput` bindings is one edit in three files. **The change must name
that method, name the file that holds it, and rewrite 3.3 so it and 3.4
describe one mechanism.**

### F2 — BLOCKING — `include/synth/runtime` does not exist, so §8.0's sweep is scoped to nothing and misses the tree it was rejected for missing

`ls External/Sheaf/projects/synth/include/synth/runtime` → No such file or
directory. The only subdirectory of `include/synth/` is `browser/`. The real
file is `External/Sheaf/projects/synth/runtime/MidiConnectionManager.hpp`.

The wrong path is load-bearing in four places:

- `proposal.md` Impact names
  `External/Sheaf/projects/synth/include/synth/runtime/MidiConnectionManager.hpp`
  as one of "the two independent `MidiEndpointOps` binding sites".
- `proposal.md`'s §8.0 scope sentence lists `.../include/synth/runtime` as a
  directory the sweep covers. It covers a directory that does not exist; the
  `runtime/` tree goes unswept.
- `design.md` Context repeats `include/synth/runtime/MidiConnectionManager.hpp`.
- `tasks.md` 1.2 scopes the forward enumeration to the same nonexistent
  directory, and 1.7 sweeps "every directory the Impact names".

B3's finding was "the Impact list omits every definition site the change edits,
which also means §8.0's sweep is scoped to the wrong tree". The rewrite added
the site and kept the wrong tree. Task 3.4 alone gets it right, writing
`runtime/MidiConnectionManager.hpp` — which is how one can tell the path was
transcribed rather than read.

### F3 — BLOCKING — smi-14 is the Hold Drill twin of smi-16, on a lower branch, unnamed

Detailed under "Sheaf sequencing claims" above.
`External/Sheaf/openspec/changes/app-midi-catalog/specs/synth-midi-instrument/spec.md:200-201`,
in that change's `## ADDED Requirements`, states Hold Drill's held flag is "set
by that button's press and release" and that "its release SHALL clear held".
Hold Drill appears in no promoted Sheaf spec, so this is the requirement that
governs it.

This change's new requirement — "A held modifier SHALL clear without requiring
any further message from the device that set it … This requirement binds every
held per-profile modifier the synth keeps, which today are Shift and Hold
Drill" — adds four clears smi-14 does not admit and reverses the lifetime it
defines, on the same reading that made smi-16 a blocking collision.

It is worse placed than smi-16: `app-midi-catalog` is **#13**, one branch below
#14. Amending smi-14 rebases #14 and #15. Task 2.2 rebases only #15 and only
through the #14 amendment; task 2.3 touches #13 only to add the preconditions
field. **The change must name smi-14, add an amend task on #13 beside 2.3, and
extend 2.2 to rebase both dependent branches.**

That this finding exists is not an argument against B6's answer — bringing Hold
Drill in scope was correct. It is §9's staleness rule biting: a ruling changed
one artifact's scope and the enumeration that would have caught the consequence
was not re-run against the new scope.

### F4 — BLOCKING — the overlap enumeration is still partial, and one omission sits inside the function the ceiling runs in

Fourteen active changes across the two trees; six named. Full table above. The
one that makes this blocking rather than tidy-up:
`External/Sheaf/projects/synth/include/synth/Engine.hpp:576-580` carries

> `// ui-state-before-audio (design "Mechanism", PINNED): after every`
> `// other MessageThreadTick duty, attempt to claim uiState_/ …`

four lines below the pump loop at `:570-574` that design decision "Ceiling at 30
seconds, evaluated at the existing pump" adds work to. `ui-state-before-audio`
is active at 4/7 and is named nowhere in this change. A PINNED design constraint
in the function you are editing is the definition of an overlap that governs.

Three more omissions touch `synth-runtime-ui`, the capability that owns the
Controllers page this change renders on, and two frogg3rs changes are complete
but unarchived and edit `app/FroggersMidiCatalog.hpp`, the file tasks 5.1-5.2
rewrite. §5's rule is a disposition per hit, including "nothing needed"; eight
hits have none.

### F5 — BLOCKING — Impact dropped `ControllersPageUI.hpp` and never names `MidiConfigViewModel.cpp`

Tasks 4.4 ("Render the report on the Controllers page"), 5.3 ("Render the
declarations on the Controllers page") and 6.3 ("Surface a sustained rate on the
slot") all land in `External/Sheaf/projects/synth/include/synth/ControllersPageUI.hpp`.
The rejected draft named that file in Impact; the rewrite removed it. The new
per-slot field must also be *populated*, which happens where rows are built —
`src/MidiConfigViewModel.cpp` — named nowhere.

Because both files sit under directories the sweep scope does list
(`.../include/synth` and `.../src`), §8.0 coverage survives. What does not
survive is §1's "Definition sites, each verified by reading": the proposal's
Impact section is presented as that list and two of the edited definition sites
are missing from it.

### F6 — BLOCKING — the resilience substance still has no Sheaf artifacts

Modifier lifetime, mismatch detection, the per-slot field and the template rate
limit are Sheaf code, governed by Sheaf's `synth-midi-instrument` and
`synth-runtime-ui`. The frogg3rs capability `froggers-midi-controller-resilience`
asserts all of it in SHALL form. On the Sheaf side the change schedules exactly
one spec edit: task 2.1, amending smi-16 to remove the contradiction. **No
Sheaf requirement is written for the behaviour that replaces it** — no proposal,
no design, no delta stating bounded lifetime, mismatch reporting or inbound
template-change handling for the synth.

The first audit filed this as S10. The rewrite's answer is the sentence "Sheaf's
`synth-midi-instrument` is NOT modified here", which resolves the *collision*
and leaves the *gap*. At archive time §9's promotion rule then applies to a
frogg3rs spec asserting behaviour no Sheaf requirement claims, implemented in a
submodule whose own spec says the opposite until 2.1 lands.

### F7 — BLOCKING — the hold ceiling has no hook and no time source, and three scenarios are untestable without them

`Engine.hpp:570-574` pumps `processors.outputs`. The held state lives on the
profile and is written from the input chain; no output processor holds either
pointer. "Evaluated at the existing pump" (design Decisions, task 3.6) names a
location that cannot reach the state, and neither artifact names the hook that
would. This is the same shape as F1, one mechanism over — the first audit
flagged it as S9's second half and it is unrepaired.

The monotonic clock is likewise unnamed. `CreateMidiControllerProfileImpl`
already threads a `MidiInProcessor::TimestampProvider`
(`src/MidiController.cpp:3022`), which is the injectable time source such a
ceiling needs and is what makes the ceiling testable without waiting 30 real
seconds. No artifact names it.

Consequently three scenarios cannot be checked as written:

- "A modifier held with no traffic clears on its own" — no time injection.
- "A modifier whose address stops transmitting does not strand the controller"
  — the only trigger that fires in this scenario is the ceiling (the device
  stays connected, so no rebuild and no reopen; the address is silent, so no
  second press), and the scenario states the clear with no time qualifier.
- "A modifier held at disconnect does not survive the reconnect" — **no
  reconnect path exists in the VST at all**: `grep -rn "MidiConnectionManager"
  app/` returns nothing, so the plugin never opens an endpoint or reconciles.
  The design calls the four triggers "independent" without saying that one of
  them is absent on one of the three hosts.

### F8 — SHOULD-FIX — the exhaustive-consumer count is wrong, and task 1.5 defers a question the structure already answers

Design: "three consumers switch exhaustively over that enum." Counted this
pass: **one** exhaustive `switch` — `EndpointStatusColor`,
`ControllersPageUI.hpp:791-800`. The other two the first audit listed are
`DeviceLabel` (`MidiConfigViewModel.cpp:600-615`, an `if` chain) and a parallel
three-entry label table (`ControllersPageUI.hpp:3347-3352`, data). Neither
"switches exhaustively". The conclusion B7 needed is unaffected — a sibling
field cannot change an enum — and task 1.5 schedules an enumeration to confirm
something the structure makes certain. Correct the number; keep the decision.

The parallel table and the switch are, however, a §5 family that must stay in
sync and has no drift check. Out of scope here, but it is what a fourth
enumerator would have broken and the design should say so once.

### F9 — SHOULD-FIX — the channel convention is still split three ways

`specs/froggers-midi-controller-mappings/spec.md:6` — "channel 3 (channel 4
counted from 1)". `specs/froggers-midi-controller-resilience/spec.md` — bare
"channel 4", three times, in the scenarios checks will actually be written
against. `MANUAL.md:338` — "channel 4 (channel 3 counted from 0)".
`proposal.md`'s evidence paragraph — "on channel field 3" for the broken
capture. Four phrasings, one channel. Filed as S3, unrepaired, and task 7.1
rewrites the manual's Shift subsection without touching :338.

### F10 — SHOULD-FIX — the APC40 precondition still does not fit the declared shape, and task 5.2 cites the wrong block

Task 5.2: "Populate the APC40 mkII defaults from the same comment block, which
documents the Track 1 caveat." The Twister block is `:14-24`; the APC40 block is
`:26-35`. Beyond the mis-citation, the substance the first audit raised is now
*less* decided than before: the rejected design defined a precondition as "a
setting name, its required value, and where it is set", and the rewrite's
decision drops the shape entirely. The APC40's caveat — "Track 1 must stay
selected" — is a live front-panel state, not a stored setting, and has no
utility to name. The spec scenario covers only the Twister. The executor is
handed a task with no shape to populate.

### F11 — SHOULD-FIX — the second-press clear makes Shift a device-side latch, and one scenario is untestable because of it

On a press-only device the new rule gives press → held, press → clear, press →
held. The mappings delta then asserts without qualification:

> **WHEN** the Twister's side buttons send press edges with no release edges
> **THEN** Bank Next, Play, Freeze, Scene 1 and Randomize Page each still fire
> on their press

which holds only while the latch is off. A check for it passes or fails on how
many times CC 13 was pressed first. Filed as S7, unrepaired. Qualify the
scenario, and have the design say it chose a device-side latch over none —
especially as it rejects an on-screen toggle by citing the promoted requirement
"SHALL offer no on-screen Shift control"
(`openspec/specs/froggers-midi-controller-mappings/spec.md:7`).

### F12 — SHOULD-FIX — 17 scenarios, 0 checks, 0 not-yet-delivered markers

`specs/froggers-midi-controller-resilience/spec.md`: 17 scenarios, **zero**
`Check:` lines. `specs/froggers-midi-controller-mappings/spec.md`: 5 scenarios,
**2** `Check:` lines — the promoted source had 3 of 3. The replaced scenario's
`Check: MANUAL.md, the Shift subsection` is dropped rather than re-pointed, and
task 7.1 rewrites precisely that subsection.

§9's promotion rule wants each scenario either backed by a check or marked
plainly as not yet delivered, decided when written. Task 8.3 defers the whole
question to postflight. Filed as S4, unrepaired and now larger.

### F13 — SHOULD-FIX — "Every finding is answered below" is not true

`proposal.md`, "Rejected predecessor": "This change's first draft was REJECTED
by preflight on 2026-09-10 with eight blocking findings … Every finding is
answered below."

S1, S2, S6 and S8 are answered. S3 (F9), S4 (F12), S5 (F10), S7 (F11), S9's
second half (F7) and S10 (F6) are not; N3 is not. Under §1 the plan's own text
is held to the same standard as its claims about code: a summary sentence
asserting completeness is a claim, and this one is false in six places.

### F14 — SHOULD-FIX — the shared type and the ceiling constant are never named

§9: "List every constant, helper, type, predicate and sentinel by name, grep
each by operand." The rewrite creates a "shared held-state type" and a "hold
ceiling" and names neither, so neither can be enumerated — by the executor, or
by this pass except through its neighbours. The ceiling in particular collides
in prose with `app/dsp/Drive.hpp`'s "Decay/Hold ceiling" and needs a name that
does not.

### F15 — NOTE — `rework-controllers-block-editing` is a phantom dependency

Detail under the overlap table. Its code is already in `origin/main`; it has no
branch; its six outstanding tasks are a fuzz harness nobody is producing.
Task 2.4's "Sequence the Controllers page work after it" waits for nothing.

### F16 — NOTE — the forward-looking clause in requirement 1 has no drift check

"a modifier added later SHALL be given the same lifetime or SHALL state why it
is exempt" is a family-stays-in-sync assertion, and §5 is explicit that such a
family gets a check that fails on drift, proven to fail by breaking it once —
not a sentence. Task 3.4 provides that mechanism for the two `MidiEndpointOps`
bindings and nothing provides it here.

### F17 — NOTE — behavioural premises: the deferral is acceptable, the design's confidence is not

Judged against §9's "a behavioural premise gets a behavioural check BEFORE
execution":

- **Task 1.3 (host pump rates) — acceptable as a deferral.** It sits in §1,
  ahead of §3.6 which depends on it, and it is written to act on either answer
  ("a host that does not pump invalidates the ceiling for that host"), which is
  what §1 asks of a task standing in for an unread fact. It is also honest about
  what is verified: "`startTimerHz(30)` is verified for the VST only".
- **Task 1.4 (rebuild clears the modifier) — acceptable, one gap.** It demands
  the modifier be demonstrated held first and the proving state printed, which
  is a correct §6.1 positive control. It does not say what to do if rebuild does
  *not* clear — and that answer would invalidate task 3.2, a design premise and
  a requirement clause.
- **What is not acceptable is the design asserting the conclusion anyway.**
  "Ceiling at 30 seconds, evaluated at the existing pump" is written as a
  settled Decision, and the requirement and three scenarios are written on it,
  while 1.3 says the premise is open. §9's staleness rule means a negative
  answer from 1.3 invalidates a Decision, a requirement sentence and three
  scenarios, and no artifact says which. Either state the ceiling in a form that
  does not depend on rate — "evaluated on the next tick after the deadline,
  whenever that arrives" costs nothing and removes the premise — or say in the
  design what 1.3's negative answer changes.

So: deferring these two into the change's own task list **does** satisfy §9's
ordering requirement, and neither is a reason to reject on its own. F7 is,
because it is not a rate question — it is that the named evaluation site cannot
reach the state and no time source is named.

---

## Artifact staleness

Applying §9's own rule to this pass: if these findings are accepted, the
revision needs a third reading, not an edit. What must move, and what else must
move with it:

**`proposal.md`** — Impact: correct the `MidiConnectionManager.hpp` path and the
§8.0 scope sentence (F2); restore `ControllersPageUI.hpp` and add
`MidiConfigViewModel.cpp` (F5); fix `MidiController.hpp:250-258` → `:249-258`.
Overlap table: add the eight omitted changes with dispositions, and restate
`rework-controllers-block-editing`'s (F4, F15). Add smi-14 to the #13 row (F3).
Delete or qualify "Every finding is answered below" (F13). Say that the Sheaf
resilience artifacts are part of this change's scope (F6).

**`design.md`** — Rewrite the "Clear from the Engine" decision around what the
code admits, naming the Engine method and the file it lands in (F1). Name the
ceiling's evaluation hook and its time source, and say what happens on a host
with no reconcile path (F7). Correct "three consumers switch exhaustively" to
one, and name the site (F8). Decide the precondition shape so the APC40's
operating-state caveat has somewhere to go (F10). State the device-side latch
consequence of second-press clearing (F11). Name the shared type and the ceiling
constant (F14). Record the classification against `Engine`'s existing
UI-publish throttle.

**`tasks.md`** — 1.2's enumeration scope (F2) and its coverage, which must
extend to the seven concepts tasks 4-6 create (§5 forward). 1.4 gains a branch
for the negative answer. 2.2 must rebase #14 as well as #15; a new task beside
2.3 amends smi-14 on #13 (F3). 2.4 rewritten or dropped (F15). 3.3 and 3.4
reconciled into one mechanism (F1). 3.6 given a hook and a clock (F7). 5.2
pointed at `app/FroggersMidiCatalog.hpp:26-35` and given a shape (F10). A task
that writes the Sheaf half's proposal, design and delta (F6). 8.3's work pulled
forward (F12).

**`specs/froggers-midi-controller-mappings/spec.md`** — checks restored or
not-yet-delivered markers added on three scenarios (F12); the press-only
scenario qualified (F11); channel convention (F9).

**`specs/froggers-midi-controller-resilience/spec.md`** — a check or a marker on
each of 17 scenarios (F12); channel convention (F9); the three untestable
scenarios reworked once F7 names a clock; the forward-looking clause given a
drift check or dropped (F16); "unmapped" renamed away from
`synth-midi-instrument`'s existing sense.

**New, not yet existing** — the Sheaf change's `proposal.md`, `design.md`,
`tasks.md` and its deltas against `synth-midi-instrument` and
`synth-runtime-ui` (F6), plus the smi-14 amendment on #13 (F3).

**Working-tree facts this change rests on that are not committed.** The archive
of `frogg3rs-midi-shift` and the promotion of
`openspec/specs/froggers-midi-controller-mappings/` exist only in the working
tree: `git status` shows the change's files as deleted and the promoted spec
directory as untracked. The MODIFIED delta's target is therefore real but
unpushed, and task 8.5's "verify clean trees at both levels" has to account for
it. The tree also still carries `frogg3rs-effect-page-hierarchy`'s 18 modified
files, including `MANUAL.md` and `QUICK_DICT.md`, which tasks 7.1-7.3 edit; the
cited `MANUAL.md` lines remain clean (first diff hunk at :524).

**Date.** `.openspec.yaml` records `created: 2026-09-11`, the archive directory
is `2026-09-11-frogg3rs-midi-shift`, and the proposal says "archived
2026-09-11". The session date is 2026-09-10. Harmless if the clock is what it
is; worth one look, because an archive directory name is an identifier other
tooling sorts on.
