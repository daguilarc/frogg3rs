# Fourth preflight audit — the split, and `frogg3rs-midi-preset-preconditions`

Run 2026-09-13 in a fresh context that wrote neither change and neither prior
report. Read-only against the worktree at
`.claude/worktrees/midi-controller-resilience`, submodule `External/Sheaf` on
branch `midi-controller-resilience` at `ba3898e4`. Nothing installed, nothing
built, no branch operation, no read of the main checkout.

`preflight.md`, `preflight-2.md` and `preflight-3.md` were read as context.
Every finding below was re-derived by opening the file. Where I restate one of
`preflight-3.md`'s findings it is because I reproduced it and it is now mine;
where I disagree I say so.

This report carries the cross-cutting material for both halves of the split.
The Sheaf half's own report is at
`External/Sheaf/openspec/changes/midi-controller-resilience/preflight.md` and
references this one.

---

## VERDICT — `frogg3rs-midi-preset-preconditions`: **REJECT**

The change's central mechanism has no owner. Its design, its overlap table and
its delivery task all state that Sheaf's `midi-controller-resilience` lands the
`MidiAppDeviceDefault` preconditions field this change populates, and that
change contains no requirement, no task, no Impact entry and no delta touching
`MidiAppDeviceDefault`. A second piece falls the same way: task 4.3 and the
ADDED requirement assert that the Controllers page shows a preset's
declarations, and the Controllers page is Sheaf code governed by
`synth-runtime-ui`, which this change does not delta and the Sheaf change does
not cover. Separately the change makes this repository's own gates red — I ran
them — one of the two failures caused by the split's own rename, which neither
change names. And the fader-1 exclusion, one of the design's two named
decisions, is cited to three reports that contain no occurrence of the word
"fader". The frogg3rs half is still the small, real change `preflight-3.md`
described. It is not this one yet.

The Sheaf half's verdict is **REJECT** as well, on four blocking findings of its
own; see that report.

---

## Was the split the right call?

Yes, and it was carried out well in most respects. `preflight-3.md`'s shape
diagnosis was correct and the Sheaf change is a genuinely better artifact than
the thing it replaces: it writes deltas against the two capabilities that
govern the code, it names smi-14 and smi-16 and amends them, it states the
binding asymmetry instead of asserting symmetry, it spells its paths from
`projects/synth/` so the `include/synth/runtime` defect cannot be transcribed,
and its overlap table names all eleven active changes with task counts I
verified one by one.

What the split did not do is own the seam between its halves. Two things sit in
the gap, each change assuming the other holds it, and nothing in either
artifact set catches that. A third thing — the renamed directory — fell out of
the tree entirely. The split is the right shape; it is not finished.

---

## `preflight-3.md`'s eight blocking findings, disposed one at a time

### Finding 1 — clear-on-reopen rests on two false claims; the bindings are not symmetric — **PARTIALLY RESOLVED**

The false half is gone and correctly reversed. `design.md` Context now reads
"The Engine owns neither the reconciliation plan nor its execution", cites the
two production callers of `ExecuteReconcilePlan`, and states the asymmetry in
its own paragraph. I re-derived all of it:
`include/synth/browser/BrowserMidiBridge.hpp:126-129` pushes
`{.type = ActionType::OpenInput, …}` onto `actions_` and returns true, touching
no engine state; `runtime/MidiConnectionManager.hpp:450` is
`ops.openInput = […](…) { return OpenInput(ix, identifier); }`, inline.
`MidiReconcile.hpp:109` says the runtime binds the ops to Engine calls. The
proposal's host table states the consequence per host. That is the repair
`preflight-3.md` asked for.

The part it did not reach is the part `preflight-3.md` flagged as the harder
one, and it is still open. `design.md` rules that in the browser "the clear
fires when the deferred open completes, not when the action is enqueued", and
task 3.5 then names that moment as
`include/synth/browser/BrowserMidiBridge.hpp:126-129` — the enqueue lambda the
design has just said is the wrong moment. The actual completion is not in C++
at all. I traced it: `BrowserMidiBridge::DequeueAction` (`:178`) →
`BrowserRuntime::DequeueMidiAction` (`:757`) → the ABI at
`browser/cpp/BrowserRuntimeAbi.cpp:176` → `browser/src/worker.ts:673`, which
drains actions inside the `message-tick` handler → `browser/src/midi.ts:228`
`applyAction`, whose `case "open-input"` at `:230-243` opens the Web MIDI port
and returns. Nothing calls back into the engine. Two of its branches return
without opening at all (`if (!port) return;` and the already-open guard), so
"the deferred open completing" is a branch rather than an event.

So the browser's clear has no seam, and creating one means a new inbound call
that no artifact names and no task writes. This is blocking for the Sheaf
change — see its report, Finding S1.

### Finding 2 — `include/synth/runtime` does not exist, and the artifacts fail an installed gate — **RESOLVED as to the path; the gate class RECURRED**

The path defect is gone. Every path in the Sheaf artifacts is spelled from
`projects/synth/`, and `runtime/MidiConnectionManager.hpp` is the spelling used
throughout. `find` confirms that is the only `MidiConnectionManager*` in the
tree. Splitting fixed this exactly as `preflight-3.md` predicted it would.

The gate consequence recurred in the frogg3rs half, in a new form and worse.
`app/check_spec_checks_resolve.py` reads untracked spec files, so it is red in
this worktree right now: seven unresolvable `Check:` references, all of them in
this change's delta. `app/check_artifact_symbols_resolve.py` is also red right
now, on the dangling directory the split's rename created, and gains two more
failures the moment this change is committed. Both runs are recorded under
"The repository's own gates" below.

### Finding 3 — smi-14 governs Hold Drill, sits on #13, and is unnamed — **RESOLVED in substance; a NEW contradiction introduced**

smi-14 is named, amended, and the amendment is minimal and deliberate. I
diffed it against `app-midi-catalog`'s text: one edit to the state list, one
appended sentence, four scenarios restated byte-identically, one scenario
added. smi-16 the same. Task 2.4 rebases #14, #15 and this branch rather than
#15 alone.

The new problem is that the change now states two incompatible mechanisms for
the same amendment. `proposal.md`'s What Changes says smi-14 and smi-16 are
amended "in this change's own delta"; its overlap table says "Amend smi-14 in
this delta" and "Amend smi-16 in this delta"; `design.md`'s Decisions says the
same. Tasks 2.2 and 2.3 instead say to amend them on #13 and #14 and to update
those changes' own tasks and checks. Doing both leaves the same requirement
text defined in two active changes with nothing keeping them in sync; doing one
makes three artifacts wrong. Nothing reconciles them. Blocking for the Sheaf
change — Finding S4 there.

### Finding 4 — the overlap table names 6 of 12, one of them archived — **RESOLVED for Sheaf; PARTIALLY for frogg3rs**

My own enumeration is below. The Sheaf table names all eleven active changes
(ten others plus itself), and I verified every task count against the
checkboxes; all ten are exactly right. Two dispositions are imprecise and one
framing sentence is wrong, all recorded in the Sheaf report.

The frogg3rs table names the live document collision the old one missed, and
`frogg3rs-effect-page-hierarchy` is gone. What it misses is that the split's
own rename broke the other active change — Finding F3 below.

### Finding 5 — Impact omits two edited definition sites — **RESOLVED**

`proposal.md`'s Impact now carries
`projects/synth/include/synth/ControllersPageUI.hpp:793-801` with
`EndpointStatusColor` named and
`projects/synth/src/MidiConfigViewModel.cpp:600-618` with `DeviceLabel` named,
alongside `MidiConfigViewModel.hpp:351-352`. I opened all three.

### Finding 6 — the substance is Sheaf code with no Sheaf artifact — **PARTIALLY RESOLVED**

A real Sheaf change now exists and owns modifier lifetime, mismatch reporting
and inbound template changes, with deltas on `synth-midi-instrument` and
`synth-runtime-ui`. smi-16 no longer loses its statement of modifier lifetime;
it gains one. That is the substance of the finding, and it is addressed.

Two pieces did not make the move, and both are now in the gap between the
changes:

- **The `MidiAppDeviceDefault` preconditions field.** `grep -rn -i
  'precondition|MidiAppDeviceDefault|device default'` over the entire Sheaf
  change directory returns four hits: `design.md:47` (a Non-Goal saying
  preconditions data is out of scope), `proposal.md:80` (the overlap row
  crediting the struct to `app-midi-catalog`), and `proposal.md:129-132`, the
  Out of scope paragraph, which asserts the frogg3rs half is "sequenced after
  this one lands the `MidiAppDeviceDefault` field it populates." No requirement,
  no task, no Impact entry and no delta in that change adds a field to
  `MidiAppDeviceDefault`. The struct as it stands
  (`External/Sheaf/projects/synth/include/synth/MidiAppCatalog.hpp:31-38`)
  carries `id`, `displayName`, `kind`, `inputAliases`, `outputAliases`,
  `config` and nothing else. Blocking here as F1, and a false claim in the
  Sheaf proposal as S2.
- **The Controllers-page render of declared preconditions.** Blocking here as
  F5.

### Finding 7 — `~/Desktop/midi-log/` does not exist — **RESOLVED in form; a NEW provenance defect**

There is no capture file anywhere in either change, and task 5.3 synthesises
the sequence in the test with a stated assertion, which is one of the two
remedies `preflight-3.md` offered. The logger claim is gone.

What replaced it has no source. `F0 00 20 29 02 11 77 NN F7`, the cycle
`0F 0C 0B 0A 09 08 07 06 05 04 03 02 01 00`, and the rate "127 per second for
ten seconds" appear in exactly two places in this repository and its submodule:
the Sheaf change's `tasks.md:97-98` and its `spec.md:93`. Task 5.3 calls it
"the documented byte sequence". Nothing documents it. `grep -n -i
'fader|2026-09-09|CC 77'` over all three prior preflight reports returns zero,
and `preflight.md:591` — the only place the flood is described — says only that
it "is recorded with its capture", which is the capture `preflight-3.md` proved
does not exist. Blocking for Sheaf as S3, and the same defect recurs in the
frogg3rs Open question as F4.

### Finding 8 — the press-only scenario contradicts the second-press clear — **RESOLVED**

The scenario is gone. Its replacement,
`specs/froggers-midi-controller-mappings/spec.md:26-30`, states the premise
that was actually observed ("its Shift address stops transmitting") and asserts
about the manual's text rather than about button behaviour under a latched
Shift. The delta also opens with a paragraph declaring the reversal explicitly,
which answers `preflight-3.md`'s Finding 16 about the silent drop. I confirmed
`app/check_modified_requirements_restate_promoted.py` still reports the drop as
a NOTE and passes.

### Shape verdict — **RESOLVED, and correct**

See "Was the split the right call?" above.

---

## Citation audit

Every file:line below was opened and read on 2026-09-13. Sheaf paths are
relative to `External/Sheaf/projects/synth/` where the artifacts spell them
that way.

### Sheaf change — correct

| Cited as | Verdict |
|---|---|
| `MidiController.hpp:249-252` `HoldDrillState`, `:256-258` `ShiftState` | **Correct.** `struct HoldDrillState {` opens :249 and closes :252; `ShiftState` :256-258. `preflight-3.md`'s off-by-one is fixed. |
| `MidiController.hpp:72` `TimestampProvider`, `:79` setter, `:82` `NextTimestamp()` | **Correct**, all three exactly. |
| `src/MidiController.cpp:955-961` Hold Drill writer, `:964-970` Shift writer, `:975` shifted read | **Correct.** `holdDrill_->held = true` :956; `shift_->held = isPress` :969; `const bool shifted = …` :975. |
| `src/MidiController.cpp:3026-3029` fresh state on build | **Correct**, the four `make_unique`/raw-pointer lines. |
| `src/MidiController.cpp:3307`, `:3335`, `:3392` three factories | **Correct** — WrldBldr, Twister, Launchpad. |
| `Engine.hpp:497` `MessageThreadTick` | **Correct.** |
| `Engine.hpp:570-574` the output loop | **Correct**, and the claim about it is correct: the loop variable is `MidiControllerProfileResult& processors`, which holds `holdDrill` and `shift`. |
| `Engine.hpp:576-580` the pinned CAS | **Correct**, the comment opens ":576 // ui-state-before-audio (design "Mechanism", PINNED):". |
| `Engine.hpp:990-991` rebuild and timestamp threading | **Correct.** |
| `Engine.hpp:1462` `timestampProvider_` | **Correct.** |
| `MidiReconcile.hpp:109` "the runtime binds these" | **Correct.** |
| `MidiReconcile.hpp:114-122` `MidiEndpointOps` including `resync` at `:121` | **Correct.** `preflight-3.md`'s truncation is fixed. |
| `BrowserMidiBridge.hpp:125-171` binding, `:126-129` openInput, `:175` `ExecuteReconcilePlan` | **Correct**, all three. |
| `MidiConnectionManager.hpp:449-462` binding, `:450` openInput, `:495` `ExecuteReconcilePlan` | **Correct**, all three. |
| `MidiConfigViewModel.hpp:351-352` the two status fields | **Correct.** |
| `ControllersPageUI.hpp:793-801` | **Correct as a range** — it is the `switch` exactly. The symbol `EndpointStatusColor` it is labelled with is declared at `:791`, outside the range. Cosmetic. |
| `src/MidiConfigViewModel.cpp:600-618` `DeviceLabel` | **Correct at the start**, one line short at the end: the function opens :600 and closes :619. Cosmetic. |
| `BrowserRuntime.hpp:722`, `runtime/Runtime.hpp:944` pump callers | **Correct**, both `engine_.MessageThreadTick();`. |
| `browser/src/main.ts:249` `setInterval`, `:406` `1000 / 30`, `browser/src/worker.ts:648` `messageTick` | **Correct**, all three. Chain traced end to end below. |
| `tests/reconcile_executor_tests.cpp:93-111`, `tests/reconcile_tests.cpp:657-659`, `tests/controllers_page_ui_tests.cpp:1462-1466` | **Correct** as three test binding sites. |
| `MidiAppCatalog.hpp:31-38` `MidiAppDeviceDefault` | **Correct**, struct opens :31, closes :38. |

### Sheaf change — wrong or unsupported

| Cited as | Verdict |
|---|---|
| `design.md`: "`MidiEndpointStatus` has two dispatch sites" | **Undercounts.** There is a third exhaustive enumeration of all three enumerators, the legend table at `ControllersPageUI.hpp:3349-3353` (`{".online", …, Online}`, `{".offline", …, Offline}`, `{".not_set", …, Unconfigured}`), plus two single-value guards at `:857` and `:872`. Finding S5. |
| `proposal.md`: "Ten delta one or both of the two specs this change touches" | **Wrong; it is eight.** `bank-addressed-absolute-write` deltas `synth-parameter-modulation` only and `fix-task-analyzer-plan-derived-tasks` deltas `task-analyzer-data-gathering` only — as the table's own dispositions say. `design.md` states eight for `synth-runtime-ui` and is right. Finding S6. |
| `proposal.md` / `tasks.md 4.4`: `rework-controllers-block-editing` "lands first" / "after … lands its row model" | **Already landed.** `git log` on the submodule shows `1d97e5ca` "Lay the controller row out on two lines so it fits a 900-wide host", `582e400a` "Keep a renamed controller's row open", `185a7a09` "Give a controller row one control per job" as ancestors of `ba3898e4`. Its six remaining unchecked tasks are a random-simulation harness and cleanup, not the row model. Finding S7. |
| `tasks.md 5.3`: "the documented byte sequence" | **Nothing documents it.** See Finding 7 above. |
| `tasks.md 3.5`: browser clear "on the deferred open completing at `BrowserMidiBridge.hpp:126-129`" | **That is the enqueue site**, which `design.md` explicitly says is the wrong moment. Finding S1. |
| `proposal.md`: Out of scope, "after this one lands the `MidiAppDeviceDefault` field" | **This change lands no such field.** Finding S2. |

### frogg3rs change — correct

| Cited as | Verdict |
|---|---|
| `MANUAL.md:319-320` the stuck-Shift recovery | **Correct.** :319-320 end "…stay shifted until Shift is pressed and released again." |
| `MANUAL.md:336-339` the Utility settings | **Correct**, exactly those four lines. |
| `MANUAL.md:348` crossfader and master fader | **Correct**: "…the crossfader is the scene blend and the master fader is BPM." |
| `app/FroggersMidiCatalog.hpp:14-24` the precondition comment | **Correct**, :14 opens "Twister: the manual's Utility settings must match this default" and :24 closes "…because the release is what ends Shift." |
| `app/FroggersMidiCatalog.hpp:165-166` | **Correct**: `.sceneBlend = {.channel = 0, .cc = 15}` at :165, `.appActions = {{…cc = 14…}, kBpm, ""}}` at :166. They sit in `Apc40BaseConfig()` (opens :123), which `Apc40GenericDeviceDefault()` installs at :178 and `Apc40AbletonDeviceDefault()` inherits at :183 — so both APC40 entries do carry both. |
| `External/Sheaf/…/MidiController.hpp:298` `sceneBlend`, `:296-300` `AnalogMidiInConfig` | **Correct**, both exactly. |
| `MidiAppCatalog.hpp:31-38` | **Correct.** |
| `app/Makefile` eight checks at `:176`, `:183`, `:191`, `:199`, `:208`, `:221`, `:228`, `:238` | **Correct**, all eight, in that order: `check_no_firmware_includes.sh`, `check_microphone_usage_strings.sh`, `check_docs_match_parameter_table.py`, `check_spec_checks_resolve.py`, `check_citations_resolve.py`, `check_modified_requirements_restate_promoted.py`, `check_no_planning_history.py`, `check_artifact_symbols_resolve.py`. |
| `synth-midi-instrument` pins "twister: encoders, system messages; launchpad: system messages only" and "generic: all sections" | **Correct**, and in the **promoted** spec, `External/Sheaf/openspec/specs/synth-midi-instrument/spec.md:16` (smi-1). Not only in an unmerged delta. |
| `frogg3rs-density-documents-and-spec` 0/14, created 2026-09-13 | **Correct.** 14 checkboxes, 0 ticked; `.openspec.yaml` absent, matching this repository's convention. Its holding `MANUAL.md` and `QUICK_DICT.md` modified in the main checkout I could not verify — the brief forbids reading that tree. Its own Impact does name both as "Affected documents". |

### frogg3rs change — wrong or unsupported

| Cited as | Verdict |
|---|---|
| `proposal.md`: fader 1 "tied to an unreproduced SysEx flood on that device, recorded in this change's `preflight.md`, `preflight-2.md` and `preflight-3.md`"; `design.md`: "characterised across the three preflight reports carried in this change" | **False.** `grep -n -i 'fader\|2026-09-09\|CC 77'` across all three returns **zero hits**. They record no fader, no capture date and no CC number. Finding F4. |
| `proposal.md`: "captures on 2026-09-09 show fader 1 sending CC 77 on channel 6 under one template and CC 77 on channel 2 under another" | **No source anywhere.** Same zero. Finding F4. |
| `proposal.md`: "The catalogue's other five devices cannot carry either" | **Wrong; it is four.** The catalogue holds six defaults (`app/FroggersMidiCatalog.hpp:329-336`): Twister, APC40 Generic, APC40 Ableton, Launchpad X, Launchpad Pro MK3, Launchpad Mini MK3. Both APC40 entries carry scene blend and BPM. `design.md` says "The Twister and all three Launchpads" — four — and is right. Finding F6. |
| `design.md`: "the field Sheaf's change adds" | **That change adds no field.** Finding F1. |
| `tasks.md 6.5`: deliver "after Sheaf's `midi-controller-resilience` has landed the field this change populates" | Same. Finding F1. |
| `proposal.md` / `design.md`: "the four triggers Sheaf's change defines" | **Ambiguous across the pair, and the count is five.** smi-16's four are release, rebuild, endpoint open, second press, with the ceiling stated separately; the Sheaf proposal's host table's four are profile rebuild, second press, hold ceiling, endpoint reopen; `HeldModifierClearSource` enumerates five. Task 3.1 rewrites operator-facing manual text against this phrase. Finding F11. |

---

## Overlap enumeration, re-run in both trees

Run by me on 2026-09-13 by listing `openspec/changes/` in both trees and
counting checkboxes in each `tasks.md`. Open PRs on the fork are not reachable
from this sandbox; PR numbers are read off the changes' own proposals.

**frogg3rs — two active:** `frogg3rs-density-documents-and-spec` 0/14, and this
change 0/23.

**Sheaf — eleven active**, each verified:

| change | counted | proposal says | spec deltas |
|---|---|---|---|
| `app-midi-catalog` | 26/28 | 26/28 | wizards, midi-instrument, patch-persistence, portable-runtime-shell, runtime-ui |
| `bank-addressed-absolute-write` | 5/7 | 5/7 | parameter-modulation |
| `browser-slider-value-readout` | 1/7 | 1/7 | runtime-ui |
| `fix-out-of-tree-app-gaps` | 29/29 | complete | app-runtime, portable-runtime-shell, portable-visualizers, runtime-ui |
| `fix-task-analyzer-plan-derived-tasks` | 20/20 | complete | task-analyzer-data-gathering |
| `launchpad-model-on-the-row` | 11/14 | 11/14 | midi-instrument, runtime-ui |
| `rework-controllers-block-editing` | 22/28 | 22/28 | runtime-ui |
| `shift-and-file-export` | 29/29 | complete | app-runtime, browser-wasm-runtime, midi-instrument, runtime-ui |
| `shorten-deadline-readout-window` | 0/6 | 0/6 | runtime-ui |
| `ui-state-before-audio` | 4/7 | 4/7 | runtime-ui |
| `midi-controller-resilience` | 0/35 | — | midi-instrument, runtime-ui |

Every count matches. Eight of the ten others delta one of this change's two
capabilities, not ten (Finding S6).

**Dispositions.** Eight of the ten are defensible as written. Two are not:

- `rework-controllers-block-editing` — "Sequence after … lands first" describes
  work that is already in this branch's history. Finding S7.
- `app-midi-catalog` — "Amend smi-14 in this delta" contradicts task 2.2's
  "Amend smi-14 on #13". Same for `shift-and-file-export` and task 2.3.
  Finding S4.

**The frogg3rs table's three rows.** The density row is right and names the live
document collision. The `app-midi-catalog` row is right: neither change
redefines `MidiAppDeviceDefault`, and `MidiAppCatalog.hpp:31-38` is where it
lives. The Sheaf `midi-controller-resilience` row asserts a dependency that
change does not deliver (F1).

**Testing the two claimed sequencing dependencies.**

- *On the Sheaf change.* The dependency is real in direction — the frogg3rs half
  populates a field on a Sheaf struct and renders it on a Sheaf page — and
  unsatisfiable in fact, because neither the field nor the render exists in the
  Sheaf change. The sequencing is correctly ordered and points at nothing.
- *On `frogg3rs-density-documents-and-spec`.* Real and correctly stated. Both
  changes edit `MANUAL.md`; the density change's Impact names `MANUAL.md` and
  `QUICK_DICT.md` as affected documents and `app/check_docs_match_parameter_table.py`
  as an affected gate; this change's task 4.4 makes a `MANUAL.md` section
  generated, which touches that gate's subject. The gate overlap is not
  mentioned in the disposition. Minor; task 1.4 baselines all eight gates and
  would surface it.

**A twelfth thing, which is neither change's row and is the split's own
breakage.** `frogg3rs-density-documents-and-spec` names
`openspec/changes/frogg3rs-midi-controller-resilience/` twice —
`proposal.md:150` ("is untracked and held deliberately; no blanket stage may
sweep it in") and `tasks.md:20` ("is untracked in this tree, so any blanket
stage sweeps a change the operator is deliberately holding … Check `git status
--short` before every commit and confirm that directory is still untracked
afterwards"). That directory was renamed to
`openspec/changes/frogg3rs-midi-preset-preconditions/` by this split. The guard
now names nothing, so it passes while protecting nothing, and
`app/check_artifact_symbols_resolve.py` fails on both lines today. Finding F3.

---

## Forward enumeration (§5, run forward, by operand, case-insensitively)

Two independent passes on different operand lists, per the plural-independence
rule: mine, and a delegate's given its own operands and its own tree list. Both
excluded `build/`, `node_modules/`, `dist/` and `.git/` and both excluded the
two change directories so they do not count themselves. Every row below is a
conclusion both passes reach.

**§6.1 control on the instrument.** Control operands returned non-zero on both
runs — mine: `ShiftState` 11 hits / 4 files, `HoldDrillState` 13 / 5,
`MidiEndpointOps` 36 / 15, `sceneBlend` 520 / 93, `MidiAppDeviceDefault` 32 / 8;
the delegate's the same five non-zero, `sceneBlend` 477 / 93 on its slightly
different counting. So a zero from either sweep is a result.

**CHANGED is 0 for every concept. Nothing is implemented; this is a preflight.**

| Concept | Operands | FOUND, domain sense | FOUND, any sense | CHANGED |
|---|---|---|---|---|
| `HeldModifierState` | `HeldModifierState`, `HeldModifier`, `HeldState`, `heldSinceMicros`, `lastClear` | **0** | 0 for all five | 0 |
| `HeldModifierClearSource` | `HeldModifierClearSource`, `ClearSource`, `EndpointOpen`, `SecondPress`, `second press` | **0** | `ClearSource` 8/2 and `clear source` 52/7, all STM32 HAL EXTI `PendClearSource` register prose in libDaisy; `second press` 4/4, the existing shifted-press sense. The prose collision on "second press" is worth a different word | 0 |
| `kHeldModifierCeilingMicros` | `kHeldModifierCeilingMicros`, `CeilingMicros`, `HoldCeiling`, `hold ceiling` | **0** | `hold ceiling` 1 hit, `app/dsp/Drive.hpp` "Decay/Hold ceiling", a reverb gain limit. The chosen spelling `kHeldModifierCeilingMicros` avoids it, which is the right call | 0 |
| `MidiSlotMismatchReport` | `MidiSlotMismatchReport`, `SlotMismatch`, `mismatch report`, `observedAddress`, `observed address`, `notTransmitted`, `not transmitted` | **0** | all 0 except `not transmitted` 6/2, libDaisy USB CDC doc comments | 0 |
| `TemplateChangeRateLimiter` | `TemplateChangeRateLimiter`, `TemplateChange`, `template change`, `RateLimiter`, `TokenBucket`, `token bucket` | **0** | `template change` 1 hit, a Python test message in `projects/agents/utils/dispatch_prompt_test.py`; bare `template` is the C++ keyword throughout | 0 |
| frogg3rs declared-preconditions field | `declaredPrecondition`, `DevicePrecondition`, `preconditions`, `precondition` | **0** | `preconditions` 20/15 and `precondition` 100/55, all ordinary English in test-setup and doc prose | 0 |
| frogg3rs check script | `check_docs_match_device_preconditions` | **0** | **0** | 0 |
| frogg3rs new test cases | `device_defaults_declare_their_preconditions`, `layout_choice_shows_declared_preconditions`, `launch_control_xl_fader_drives_scene_blend` | **0** | **0** each | 0 |
| Launch Control XL | `LaunchControl`, `LaunchControlXl`, `launch_control`, `LCXL`, `Launch Control` | **0** | `LCXL` 4/3, all coincidental substrings inside base64 blobs; `Launch Control` 1 hit, a generic phrase in an archived wizard report | 0 |

**Two observations the table makes rather than states.**

The Sheaf change names every concept it creates. Five names, five operands,
five zeros. That is the §9 obligation discharged, and it is the clearest
improvement over the superseded change, where two concepts had no name at all.

The frogg3rs change names none of its central concept. There is no type name
and no field name for the declared preconditions anywhere in its proposal,
design, tasks or delta — tasks 4.1 and 4.2 say only "Populate the Twister
default's declarations". So its forward enumeration has no operand, and the
zeros above are the answer to a proxy question. Finding F7.

---

## The Scene claim, verified independently

The frogg3rs change asserts three things and draws one conclusion. I checked
each without relying on the artifacts.

**`AnalogMidiInConfig::sceneBlend` exists.** Yes.
`External/Sheaf/projects/synth/include/synth/MidiController.hpp:296-300` is
`struct AnalogMidiInConfig { std::vector<AnalogMidiMapping> gestures;
std::optional<MidiControlAddress> sceneBlend; std::vector<AnalogAppActionMapping>
appActions; };`, with `sceneBlend` at `:298`.

**It is decoded more directly than BPM.** Yes.
`External/Sheaf/projects/synth/src/MidiController.cpp:847-848` reads
`if (config_.sceneBlend.has_value() && *config_.sceneBlend == address) {
Push(MessageIn::SetSceneBlend(NextTimestamp(), normalized)); }` — a first-class
message type with its own enumerator (`MessageIn::Type::SetSceneBlend`,
serialised as `"setSceneBlend"` at `:221-222`, `:278-279`, and handled at
`:1869`, `:2427`, `:2500`). BPM travels as an app action through
`AnalogMidiInConfig::appActions`, registered at
`app/FroggersMidiCatalog.hpp:317` with the range
`std::make_pair(kFroggersBpmMin, kFroggersBpmMax)`, which are `30.0f` and
`300.0f` at `app/FroggersUiSurface.hpp:261-262`. The design's "0-1 to 30-300
rescale" is exact.

**It is already defaulted to the APC40 crossfader.** Yes.
`app/FroggersMidiCatalog.hpp:165` sets `sceneBlend` to channel 0 CC 15 and
`:166` sets BPM to channel 0 CC 14, inside `Apc40BaseConfig()` (opens `:123`).
`Apc40GenericDeviceDefault()` installs it at `:178` and
`Apc40AbletonDeviceDefault()` inherits the whole struct at `:183`, overriding
only `id`, `displayName` and `openSysEx`. Both APC40 entries carry it.
`MANUAL.md:348` describes exactly that.

**The Launch Control XL is not in the catalogue.** Correct.
`app/FroggersMidiCatalog.hpp:329-336` lists six defaults: Twister, APC40
Generic, APC40 Ableton, Launchpad X, Launchpad Pro MK3, Launchpad Mini MK3. My
enumeration above finds no LCXL anywhere in either tree.

**Verdict on the conclusion: the conclusion holds.** The mechanism exists, it is
first-class, it is already assigned, and the gap is a missing catalogue entry.
The frogg3rs change is not built on a false premise here, and this is not a
reason to reject it.

**Two corrections inside the argument.** "The catalogue's other five devices
cannot carry either" is wrong — it is four, because both APC40 entries carry
both (Finding F6). And the premise that carries the whole Why section, "on no
device this operator owns", is an operator-inventory fact that cannot be settled
by reading and that no task settles (Finding F12).

**Is `Generic` permitted an analog section?** Yes, by the **promoted**
requirement, not only by an unmerged delta:
`External/Sheaf/openspec/specs/synth-midi-instrument/spec.md:16` (smi-1) reads
"each kind SHALL declare which config sections it supports (wrldbldr: encoders,
system messages, analogs; twister: encoders, system messages; launchpad: system
messages only; **generic: all sections**)". The two APC40 defaults are already
`MidiProfileKind::Generic` (`app/FroggersMidiCatalog.hpp:171` and `:182`) and
already carry an analog section, so the pattern is in the shipping catalogue.
Cataloguing the LCXL as `Generic` is permitted, and the design's reasoning for
preferring it over a new kind — no Sheaf spec change, no new enumerator to
thread — is sound.

---

## The repository's own gates, run

These are read-only Python scripts, already installed, bounded output. I ran
four of them.

```
check-citations-resolve:  OK - 95 commit-pinned, 277 into pinned or frozen
                          trees, 0 unresolvable, 0 line-numbered into this tree,
                          0 split across two lines
check-modified-requirements-restate-promoted:
                          OK - 4 MODIFIED requirement(s), 122 promoted clause(s)
                          restated or declared, 8 declared edit(s), 1 promoted
                          scenario(s) no longer restated  [the declared reversal]
check-spec-checks-resolve:  FAIL - 7 unresolvable Check reference(s)
check-artifact-symbols-resolve: FAIL - 2 unresolvable name(s)
```

**`check-spec-checks-resolve` is red right now.** All seven failures are in
`openspec/changes/frogg3rs-midi-preset-preconditions/specs/froggers-midi-controller-mappings/spec.md`,
at lines 30, 40, 45, 50, 55, 68 and 73. This gate reads untracked spec files, so
it does not wait for a commit. The cause is a rule the gate states on purpose
(`check_spec_checks_resolve.py:375-382`): a `not yet delivered` prefix is
accepted as a declaration, and "anything it names is still resolved". The
delta's chosen form — `Check: not yet delivered; added by this change as
\`app/FroggersMidiCatalogTests.cpp: device_defaults_declare_their_preconditions\``
— names a case that does not exist, so the declaration does not save it. The
Sheaf delta uses the same form and is not subject to this gate.

**`check-artifact-symbols-resolve` is red right now** on the two dangling
references in `frogg3rs-density-documents-and-spec` (Finding F3). I additionally
ran the gate's own `resolve()` over this change's `proposal.md` and `tasks.md`
directly, since it skips untracked files and would otherwise report a green that
proves only that it ran. Two further failures land the moment this change is
committed:

```
tasks.md:48  `app/check_docs_match_device_preconditions.py` is neither a file
             nor a directory under app/, openspec/, External/Sheaf/, src/
tasks.md:6   `launch_control` names no test case and no symbol in this tree
```

The forward-reference exemption did not fire — `declared_forward()` returned an
empty set — because the gate requires the token to be written directly after
`NEW` or `Name it`, and task 4.5 writes "Add `app/check_docs_match_device_preconditions.py`".

`openspec validate --strict` passes on both changes.

---

## Findings — `frogg3rs-midi-preset-preconditions`

**F1 — BLOCKING — the preconditions field this change populates is created by
nothing.** `design.md`'s first Decision says "Putting the declaration on
`MidiAppDeviceDefault` — the field Sheaf's change adds"; the overlap table says
that change "adds the declared-preconditions field to `MidiAppDeviceDefault`";
task 6.5 gates delivery on it having "landed the field this change populates".
The Sheaf change's entire directory mentions `MidiAppDeviceDefault` twice and
`precondition` twice, all four times to place the work out of its own scope.
`MidiAppCatalog.hpp:31-38` carries six members and none of them is this. Tasks
4.1, 4.2 and 5.2 all write into a field that does not exist and that nobody is
making.
**Remedy:** put the field in the Sheaf change — named, in its Impact, with a
task and a requirement — or define it here and drop the dependency. Whichever
way it goes, three artifacts in this change and one paragraph in the Sheaf
proposal have to agree afterwards.

**F2 — BLOCKING — the change makes two of this repository's eight gates red, and
one of them is red before a single line of code is written.**
`app/check_spec_checks_resolve.py` fails with seven unresolvable `Check:`
references, all in this change's delta, today, because it reads untracked spec
files. `app/check_artifact_symbols_resolve.py` gains two more failures on
commit. Task 1.4's instruction to "baseline every `app/` gate with counts before
touching anything" therefore records a red that the artifacts themselves caused
and hands the executor a pre-existing failure to carry forward.
**Remedy:** for the seven, either write the `Check:` lines with no backticked
name (`Check: not yet delivered`, the form the gate accepts) and put the
intended name in the task that creates it, or land the named cases first. For
the two, spell the new script in a form the gate's forward-reference rule
admits, and replace the `launch_control` operand in task 1.1 with one that is a
real token. Then re-baseline.

**F3 — BLOCKING — the split's rename left a live guard in another active change
pointing at nothing, and the gate is failing on it now.**
`frogg3rs-density-documents-and-spec` names
`openspec/changes/frogg3rs-midi-controller-resilience/` at `proposal.md:150` and
`tasks.md:20`, both times as the reason `git add -A` is forbidden in that
change and as something to re-confirm untracked after every commit. The
directory is now `frogg3rs-midi-preset-preconditions`. §8.0's inbound half —
enumerate everything that mentions a thing before removing it — was not run over
the rename. A guard pointed at nothing still passes.
**Remedy:** update both lines in the density change as part of this one, and add
the rename to this change's overlap disposition for it.

**F4 — BLOCKING — the fader-1 decision and the 2026-09-09 capture are cited to
documents that do not contain them.** `proposal.md`'s Open question says fader 1
is "the control tied to an unreproduced SysEx flood on that device, recorded in
this change's `preflight.md`, `preflight-2.md` and `preflight-3.md`", and that
"captures on 2026-09-09 show fader 1 sending CC 77 on channel 6 under one
template and CC 77 on channel 2 under another". `design.md` repeats both.
`grep -n -i 'fader|2026-09-09|CC 77'` over all three reports returns zero.
`preflight.md:591` describes the flood without naming a control, and the capture
it refers to is the one `preflight-3.md` proved does not exist. This is the
single stated reason fader 1 is excluded, and the single stated reason the CC
map is a task rather than an assertion — both decisions rest on it.
**Remedy:** state where the capture is, by path and size, or drop both claims
and give the real reason the map is measured rather than asserted (the template
dependence, which stands on its own).

**F5 — BLOCKING — a frogg3rs capability asserts the behaviour of a Sheaf-owned
page, with no Sheaf artifact for it.** The ADDED requirement says "The
Controllers page SHALL show a preset's declared preconditions where that preset
is chosen", and task 4.3 implements it. The Controllers page is
`External/Sheaf/projects/synth/include/synth/ControllersPageUI.hpp` and its view
model is `External/Sheaf/projects/synth/src/MidiConfigViewModel.cpp`;
`app/` holds no page, only `app/FroggersControllersPageTests.cpp`, whose own
header comment says it "drives the Controllers page's view model
(synth::MidiConfigViewModel)". That page is governed by `synth-runtime-ui`,
which this change does not delta and which the Sheaf change deltas only for
sru-63's mismatch report. This change's Impact names no Sheaf file and its
§8.0 scope sentence says the sweep "does not cover `External/Sheaf`, which the
Sheaf change sweeps" — and the Sheaf change sweeps it for other work. This is
`preflight-3.md`'s Finding 6 surviving the split at reduced size.
**Remedy:** move the render requirement into the Sheaf change's `synth-runtime-ui`
delta with a task beside sru-63's, and leave this change asserting only what
`app/` owns — the declarations themselves, the manual, and the drift gate.

**F6 — SHOULD-FIX — "the catalogue's other five devices" is four.** Six defaults
at `app/FroggersMidiCatalog.hpp:329-336`; two are APC40 and both carry scene
blend and BPM through `Apc40BaseConfig()`. `design.md` says "The Twister and all
three Launchpads" and is right.
**Remedy:** make the proposal agree with the design.

**F7 — SHOULD-FIX — the change's central concept has no name, so §9's forward
enumeration has no operand.** No type, field or accessor for the declared
preconditions is named in any of the four artifacts. Two independent sweeps
returned zero on every proxy operand either of us could invent, which answers a
proxy question rather than the question.
**Remedy:** name the type and the field, then enumerate by them.

**F8 — SHOULD-FIX — a restated scenario now under-enumerates.** The MODIFIED
requirement's scenario "Only the Twister carries Shift or shifted jobs" has
**THEN** "no APC40 or Launchpad association has a shifted press or a Shift
press". This change adds a seventh default. The scenario passes and stops
covering the requirement's own SHALL ("No other preset SHALL map a Shift button
or a shifted job").
**Remedy:** state the THEN over every non-Twister default rather than over a
list of device families.

**F9 — SHOULD-FIX — task 5.3 does work no requirement or scenario covers.**
"Check that the preset maps no Shift button and no shifted job." The LCXL
requirement's closing SHALL says it; none of its three scenarios checks it.
**Remedy:** add the scenario, or fold the check into F8's repair.

**F10 — SHOULD-FIX — task 2.1 needs hardware and is not marked as needing it.**
"With the device on one pinned template, read what each of its eight faders
transmits." Every other task in the change, and the whole of group 5, is blocked
behind it. Nothing establishes the device is reachable, and nothing marks this
as an operator step, which is what §9 asks of a check whose observability is the
claim.
**Remedy:** mark 2.1 as an operator/hardware step, say what it needs to be
plugged into and what proves the reading is live, and say what the change does
if the device is not available.

**F11 — SHOULD-FIX — "the four triggers" names two different sets across the
pair, and there are five.** smi-16's four are release, rebuild, endpoint open
and second press, with the ceiling stated separately under a WHERE; the Sheaf
proposal's host table's four are profile rebuild, second press, hold ceiling and
endpoint reopen; `HeldModifierClearSource` enumerates five. Task 3.1 rewrites
`MANUAL.md:319-320` to "the triggers that actually end a held modifier", so the
ambiguity lands in front of the operator.
**Remedy:** fix the wording in the Sheaf change first — one enumeration, one
count — then have this change's manual text and delta reference it rather than
restate a number.

**F12 — NOTE — "on no device this operator owns" cannot be settled by reading
and no task settles it.** The Why section's whole frame rests on the operator not
owning an APC40 mkII, which the catalogue ships a default for.
**Remedy:** say what the claim is based on, or reframe on what is checkable —
that the LCXL is not catalogued.

**F13 — NOTE — §8.0's second sweep is not a task.** Task 1.5 is the opening
sweep. Nothing re-runs it against the change's own final diff, which §8.0
requires and which is where the debt a change introduces actually lives.
**Remedy:** add it to group 6. The Sheaf change has the same gap.

---

## What this audit does not certify

Per the preamble's rule that an audit may not certify its own repair: the
repairs these findings ask for are new untraced text and get their own preflight
in a context that did not write this report. Two things I could not check from
this sandbox, stated rather than labelled around: whether
`frogg3rs-density-documents-and-spec` currently holds `MANUAL.md` and
`QUICK_DICT.md` modified in the main checkout (the brief forbids reading that
tree), and the state of the open pull requests on the fork (not reachable).
