# Preflight 5 — adjudication

Pair: frogg3rs `openspec/changes/frogg3rs-midi-preset-preconditions/` (worktree
`/Users/diegoaguilar-canabal/Desktop/frogg3rs/.claude/worktrees/midi-resilience`, uncommitted
working tree) and Sheaf `openspec/changes/midi-controller-resilience/`
(`External/Sheaf`, uncommitted working tree).

**Verdict: REJECT.** 21 consolidated BLOCKING findings, all independently CONFIRMED. 0 refuted.
2 OPEN items, both needing the operator.

## How this ruling was made

I did not weigh the six reports. For each finding I opened the artifact and the code it names, or
ran the command, in the two working trees (read-only; no build, no edit, no install). Every
verdict below carries my own command and its literal output. Agreement between reporters is
recorded only in the coverage note at the end, and is not evidence for any verdict.

`$W` = `/Users/diegoaguilar-canabal/Desktop/frogg3rs/.claude/worktrees/midi-resilience`.
`$S` = `$W/External/Sheaf/projects/synth`.
`$CH` = `$W/External/Sheaf/openspec/changes/midi-controller-resilience`.
`$FCH` = `$W/openspec/changes/frogg3rs-midi-preset-preconditions`.

---

# 1. Confirmed BLOCKING findings

Work down this list. Each remedy states what must be true of the artifacts afterwards.

---

## ADJ-01 — The pump loop the ceiling is assigned to dereferences null state on a blacklisted slot

**Sources:** A1; E-ADV9 path 3. **CONFIRMED. Blocks execution.**

**What breaks.** `design.md:17-19` tells the executor the loop variable at `Engine.hpp:570-574`
"holds `holdDrill` and `shift` — so the state is reachable there without new plumbing", and task
3.4 puts the ceiling evaluation in that loop. Both members are `std::unique_ptr` and
`CreateBlacklistedMidiControllerProfile()` sets neither, while pushing that result into the same
`midiProcessors_` vector the loop walks. Following the sentence as written produces a null
dereference on the message thread for any instrument carrying one blacklisted controller slot.
Every host pumps that loop. The artifacts never use the word "blacklisted".

**My evidence.**
```
$ sed -n '957,963p' $S/include/synth/MidiController.hpp
struct MidiControllerProfileResult {
    std::unique_ptr<MidiInProcessor> input;
    std::vector<std::unique_ptr<MidiInProcessor>> inputThru;
    std::vector<std::unique_ptr<MidiOutputProcessor>> outputs;
    std::unique_ptr<HoldDrillState> holdDrill;
    std::unique_ptr<ShiftState> shift;
};

$ sed -n '3188,3192p' $S/src/MidiController.cpp
MidiControllerProfileResult CreateBlacklistedMidiControllerProfile() {
    MidiControllerProfileResult result;
    result.input = std::make_unique<DropMidiInProcessor>();
    return result;
}

$ sed -n '978,981p;995p' $S/include/synth/Engine.hpp
        for (std::size_t ix = 0; ix < controllers.size(); ++ix) {
            if (controllers[ix].disposition == MidiControllerDisposition::Blacklisted) {
                rebuilt.push_back(CreateBlacklistedMidiControllerProfile());
                continue;
        midiProcessors_ = std::move(rebuilt);

$ sed -n '570,574p' $S/include/synth/Engine.hpp
        for (MidiControllerProfileResult& processors : midiProcessors_) {
            for (auto& output : processors.outputs) {
                output->Process();
            }
        }

$ grep -rni "blacklist" $CH/design.md $CH/tasks.md $CH/proposal.md ; echo "rc=$?"
rc=1
```

**Remedy (condition on the artifacts).** design.md's Context sentence and task 3.4 state that
`holdDrill` and `shift` are null on a blacklisted slot and prescribe the guard; task 3.9's
positive control includes a blacklisted slot alongside an active one carrying a held modifier.

---

## ADJ-02 — The ceiling formula is unguarded unsigned subtraction over a clock that returns 0 when unset

**Sources:** E-ADV9 paths 1-2; A13 (filed OPEN/NOTE, upgraded in exchange); F-ADV5 (secondary
hazard). **CONFIRMED. Blocks execution.**

**What breaks.** Task 3.4 prescribes `NextTimestamp() - heldSinceMicros` exceeding
`kHeldModifierCeilingMicros`, with no guard, over `std::uint64_t`. `NextTimestamp()` returns
literally `0` when no provider is installed. Provider absent at both ends: `0 - 0 = 0`, the
ceiling never fires and the modifier is stuck forever with a green suite. Provider present at the
writer and absent at the evaluation site: `0 - heldSince` wraps past any ceiling and every held
modifier clears on the next tick. Both production hosts currently pass a real provider, so this
is latent rather than shipped — but the expression the executor is told to write admits both, and
the named check injects a fake provider, so it is green under all of them.

**My evidence.**
```
$ sed -n '492,494p' $S/src/MidiController.cpp
std::uint64_t MidiInProcessor::NextTimestamp() const {
    return timestampProvider_ == nullptr ? 0 : timestampProvider_();
}

$ sed -n '64,67p' $CH/tasks.md
- [ ] 3.4 Clear once `NextTimestamp() - heldSinceMicros` exceeds
      `kHeldModifierCeilingMicros`, evaluated in the loop at
      `include/synth/Engine.hpp:570-574`, above the pinned CAS at `:576-580`,
      recording `Ceiling`.

$ sed -n '96,99p' $S/include/synth/MidiController.hpp     # blacklisted slot's processor
class DropMidiInProcessor final : public MidiInProcessor {
public:
    using MidiInProcessor::MidiInProcessor;
    void Process(const BasicMidi&) override {}

$ grep -n "engine_(" $S/runtime/Runtime.hpp $S/include/synth/browser/BrowserRuntime.hpp
runtime/Runtime.hpp:105:        , engine_([this]() -> std::uint64_t { return NowMicros(); })
include/synth/browser/BrowserRuntime.hpp:363:        : engine_([this] { return timestampMicros_.load(std::memory_order_relaxed); })
```

**Remedy.** design.md and task 3.4 state the comparison saturating
(`now > heldSinceMicros && now - heldSinceMicros > kHeldModifierCeilingMicros`), name the clock
source explicitly as the Engine's own `timestampProvider_` rather than a processor's
`NextTimestamp()`, and say what a profile with no timestamp provider does. Task 3.9 carries a
negative case: a profile built with a default-constructed provider does not clear on the first
tick.

---

## ADJ-03 — Both new triggers write input-chain state from another thread; the artifacts decide no seam

**Sources:** F-ADV4 only. **CONFIRMED. Blocks execution.**

**What breaks.** `HoldDrillState::held` and `ShiftState::held` are plain `bool`s. Every current
write and read is inside the input chain, which the codebase tags `ScopedThreadId(ThreadId::MidiInput)`.
Task 3.4 puts a writer in `Engine::MessageThreadTick()` and task 3.5 puts another on the
reconcile path — both reached from `Runtime::timerCallback()` on the message thread. Neither
artifact mentions atomics, a publication seam, or thread discipline; the design's only sentence on
the subject is "Evaluate the ceiling in the existing loop, not a new one". No sanitizer build
exists and every named check is single-threaded, so nothing in either repository can observe the
result. The executor cannot write the code without deciding the seam, and the cheapest decision
(a plain assignment) leaves a data race on the exact flags this change exists to protect.

The change also falsifies a stated rationale on its own edit path: `MidiController.hpp:254-255`
gives the single-reader invariant as the reason the type is shaped as it is.

**My evidence.**
```
$ sed -n '244,258p' $S/include/synth/MidiController.hpp
struct HoldDrillState {
    bool held = false;
    std::vector<bool> drilled;  // one flag per encoder-turn mapping
};

// Per-profile: set by a Shift button's press, cleared by its release, read
// only by that profile's system-button processor.
struct ShiftState {
    bool held = false;
};

$ grep -n -- "->held" $S/src/MidiController.cpp
712:            if (holdDrill_ != nullptr && holdDrill_->held) {
956:            holdDrill_->held = true;
959:            holdDrill_->held = false;
969:            shift_->held = isPress;
975:        const bool shifted = shift_ != nullptr && shift_->held && association->shiftedPress.has_value();

$ sed -n '106,111p' $S/runtime/MidiConnectionManager.hpp
    void Process(const synth::BasicMidi& midi) override {
        synth::ScopedThreadId tag(synth::ThreadId::MidiInput);
        if (target_ != nullptr) {
            target_->Process(midi);
        }
    }

$ sed -n '943,946p' $S/runtime/Runtime.hpp
    void timerCallback() override {
        engine_.MessageThreadTick();
        midiConnections_->OnTimerTick();

$ grep -c "sanitize" $S/Makefile
0
$ grep -rni "atomic\|data race" $CH/design.md $CH/tasks.md ; echo "rc=$?"
rc=1
```

**Remedy.** design.md decides the seam before any code — either `HeldModifierState`'s fields are
`std::atomic` with stated orderings, or the off-thread triggers post a request the input chain
consumes — and smi-16 says which thread each of the five triggers fires on, so a reviewer can
check it. `MidiController.hpp:254-255`'s rationale is rewritten in the same change.

---

## ADJ-04 — `kTemplateChangeRateLimitPerSecond` has no value, and task 5.3's assertion is written against it

**Sources:** B1, C5, E-ADV2. **CONFIRMED. Blocks execution.**

**What breaks.** Task 5.3 asserts that observer notifications over the window "do not exceed
`kTemplateChangeRateLimitPerSecond` times the window's duration in seconds", and defines the
stimulus as a burst "sent faster than `kTemplateChangeRateLimitPerSecond`". The constant has no
value anywhere in either repository. Task 5.2 instructs the executor to author it in design.md.
Both sides of the inequality and the stimulus are the same unwritten symbol, so no value can make
the check red; the window ("for several seconds") is not a number either. The sibling constant
shows this is not house style: `kHeldModifierCeilingMicros` is written as 30 seconds at
design.md:87-88 with its reasoning.

**My evidence.**
```
$ grep -rn "kTemplateChangeRateLimitPerSecond" $W --include='*.md' --include='*.hpp' --include='*.cpp' --include='*.ts' | grep -v preflight
…/midi-controller-resilience/tasks.md:127:      `kTemplateChangeRateLimitPerSecond` in `design.md`; its value is this
…/midi-controller-resilience/tasks.md:134:      `kTemplateChangeRateLimitPerSecond` for several seconds — with no capture
…/midi-controller-resilience/tasks.md:137:      over the window do not exceed `kTemplateChangeRateLimitPerSecond` times
…/midi-controller-resilience/design.md:170:`kTemplateChangeRateLimitPerSecond`, this change's own choice — set well below

$ sed -n '169,175p' $CH/design.md
`kTemplateChangeRateLimitPerSecond`, this change's own choice — set well below
the flood scenario's synthetic burst rate and well above any rate a human
changes templates at, so the limiter engages on a flood and never on ordinary
use. …

$ sed -n '87,88p' $CH/design.md
**The ceiling is wall-clock elapsed, never a pump count.** `kHeldModifierCeilingMicros`
= 30 seconds, compared against `NextTimestamp() - heldSinceMicros`.
```

**Remedy.** design.md carries the rate as a written number, with the one-paragraph defence the
ceiling gets. Task 5.3's burst rate and its measurement window are written numbers too, so the
assertion compares against figures the artifacts fixed rather than against the implementation's
own parameter.

---

## ADJ-05 — `HeldModifierState`'s field defaults are unspecified, and the Rebuild scenario reads one of them

**Sources:** B2; F-ADV6 (same mechanism, filed SHOULD-FIX). **CONFIRMED. Blocks execution.**

**What breaks.** smi-16's scenario requires that after a profile rebuild "the recorded clear
trigger is the rebuild". Task 1.7 writes no code for that trigger — it says Rebuild "is already
achieved by fresh construction". So the value the assertion reads is whatever default member
initialiser the executor invents, and design.md gives the struct with none. If the default is the
first enumerator `Release`, the scenario fails; if it is `Rebuild`, a modifier that was never held
reports having been cleared by a rebuild. Either is an invention and one of them is invented
toward green. The two structs being replaced do carry defaults today, so the omission is against
the house style of the very declaration block.

**My evidence.**
```
$ sed -n '80,83p' $CH/design.md
**One shared type for both modifiers.** `HeldModifierState { bool held;
std::uint64_t heldSinceMicros; HeldModifierClearSource lastClear; }`, with
`HeldModifierClearSource` enumerating `Release`, `Rebuild`, `EndpointOpen`,
`SecondPress`, `Ceiling`.

$ sed -n '57,58p' $CH/tasks.md
- [ ] 3.1 Add `HeldModifierState` and `HeldModifierClearSource`; embed the state
      in `ShiftState` and `HoldDrillState`, keeping `drilled` on the latter.

$ sed -n '35,37p' $CH/tasks.md
- [ ] 1.7 Confirm `Rebuild` is already achieved by fresh construction at
      `src/MidiController.cpp:3026-3029`, and that the three factories at
      `:3307`, `:3335`, `:3392` do the same. Record the disposition per site.

$ sed -n '55,58p' $CH/specs/synth-midi-instrument/spec.md
#### Scenario: A rebuild clears a held modifier
- **WHEN** Shift is held and that controller's profile is rebuilt
- **THEN** the profile is unshifted, and the recorded clear trigger is the rebuild
- Check: not yet delivered; added by this change as `instrument_tests.cpp: HeldModifierClearsOnProfileRebuild`

$ sed -n '249,257p' $S/include/synth/MidiController.hpp   # existing structs DO carry defaults
struct HoldDrillState {
    bool held = false;
…
struct ShiftState {
    bool held = false;
```

**Remedy.** design.md states each field's default, and says explicitly whether the Rebuild trigger
is satisfied by a default or by an assignment at construction. If by a default, the artifacts say
what a never-held modifier reports, and smi-16's scenario is written so that value cannot satisfy
it vacuously.

---

## ADJ-06 — smi-16's endpoint-open check is pinned to a test file that `#error`s on the type it must name

**Sources:** B3, C4. **CONFIRMED. Blocks execution.**

**What breaks.** smi-16 pins the check to
`reconcile_executor_tests.cpp: EndpointOpenClearsHeldModifierAtBothBindings`, and task 3.6
requires it "asserted against the two production types by name so a fake cannot satisfy it". One
of the two production types is `synth_runtime::MidiConnectionManager`, whose header includes
JUCE. That test file carries an explicit `#error` if JUCE is visible, and its Makefile recipe
carries no JUCE include path. The three statements cannot all hold. The executor's ways out —
relocate the check, change the recipe, or assert against fakes — are all reinterpretations of
scope, and the third is what the design forbids.

**My evidence.**
```
$ sed -n '1,5p' $S/tests/reconcile_executor_tests.cpp
#include "synth/MidiReconcile.hpp"

#ifdef JUCE_MAJOR_VERSION
#error "synth module tests must not see JUCE headers"
#endif

$ grep -n "juce_audio_devices" $S/runtime/MidiConnectionManager.hpp
83:#include <juce_audio_devices/juce_audio_devices.h>

$ sed -n '193,194p' $S/Makefile ; grep -n "^CPPFLAGS" $S/Makefile
$(RECONCILE_EXECUTOR_TEST_BIN): tests/reconcile_executor_tests.cpp $(LIB) include/synth/MidiReconcile.hpp include/synth/MidiController.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LIB) -o $@
3:CPPFLAGS ?= -Iinclude

$ sed -n '80p' $CH/specs/synth-midi-instrument/spec.md
- Check: not yet delivered; added by this change as `reconcile_executor_tests.cpp: EndpointOpenClearsHeldModifierAtBothBindings`
```

**Remedy.** smi-16's Check line and task 3.6 name a test binary that links JUCE and can see both
production types (or name two cases, one per binding, and say so in the design). That binary's
build recipe is named in the task, and the target is added to task 1.5's baseline list.

---

## ADJ-07 — The browser half of the endpoint-open trigger is ungated, and task 3.5 contradicts smi-16 on the reopen case

**Sources:** E-ADV4, F-ADV7; B4 (gate half). **CONFIRMED. Blocks execution and delivery.**

**What breaks.** Two things.

(a) Task 3.5 instructs the new ABI call to be made "after the `if (!port) return;` and
already-open guards, not before them". The already-open guard in `midi.ts` is exactly the reopen
case: a reconcile that re-issues OpenInput for a port already bound returns without opening and
now also without clearing. smi-16's scenario says the trigger fires on "opens **or reopens**".
That is a direct contradiction between a task and the requirement it delivers, decided nowhere.

(b) Task 1.5 baselines `projects/synth test`, the miniapp target and `browser-midi-bridge-test` —
all C++/Make. Task 7.2 re-runs "every gate baselined in 1.5". The TypeScript call site the whole
browser trigger depends on is exercised by no baselined gate; the synth Makefile contains no npm,
vitest or playwright invocation at all. A runnable node-level gate over `applyAction` already
exists (`browser/tests/midi-timing.test.mjs`, reachable via `npm run test:unit`, no Playwright),
so the remedy costs a line rather than a harness.

**My evidence.**
```
$ sed -n '228,236p' $S/browser/src/midi.ts
  private applyAction(action: MidiAction, access: MidiAccess): void {
    switch (action.type) {
      case "open-input": {
        const port = this.inputFor(access, action.identifier);
        if (!port) return;
        const existing = this.inputs.get(action.controllerIx);
        if (existing?.identifier === port.id && existing.port === port) return;

$ sed -n '77,79p' $CH/specs/synth-midi-instrument/spec.md
- **WHEN** a controller's input or output endpoint opens or reopens while a modifier on that controller is held, on a host that opens endpoints

$ sed -n '29,33p' $CH/tasks.md
- [ ] 1.5 Baseline every gate with counts before touching anything:
      `projects/synth test` (expect the two carried 96 kHz deadline failures),
      the miniapp target built explicitly, and `browser-midi-bridge-test`, which
      covers one of the two bindings 3.5 is about.

$ grep -c "npm\|vitest\|playwright" $S/Makefile
0
$ grep -rln "applyAction\|open-input" $S/browser/tests
…/browser/tests/midi-flow.spec.ts
…/browser/tests/midi-timing.test.mjs
$ sed -n '1,5p' $S/browser/tests/midi-timing.test.mjs
import assert from "node:assert/strict";
import test from "node:test";

import { BrowserMidiManager } from "../src/midi.js";
```
(Also verified: `ops.openOutput` in `BrowserMidiBridge.hpp:130-135` has a third early return that
never enqueues the action when sink or sender is null.)

**Remedy.** smi-16 and task 3.5 agree on what the already-open path does — if the trigger must
fire on a reopen, the call moves above that guard and the task says so. Task 1.5's baseline and
task 7.2's re-run name a browser gate that executes `applyAction`'s open path, and smi-16's Check
line names it beside the C++ case.

---

## ADJ-08 — The mandated ABI version bump has a 24-site blast radius outside the declared sweep, caught by no gate

**Sources:** B4 only. **CONFIRMED. Blocks delivery.**

**What breaks.** Task 3.5 mandates bumping `synth_browser_abi_version()`. The version is not one
literal: it is the production constant, a second constant in `protocol-versions.js`, and 24
literal `abiVersion: 6` sites across 25 files including the shell, the site builder and the whole
browser test suite — and `catalog.ts` rejects a mismatch **exactly**, so a bump without the sweep
is a hard runtime reject, not a warning. The Sheaf proposal's declared §8.0 sweep covers
`projects/synth/include/synth/browser` (headers) but not `projects/synth/browser/`, which is where
both files task 3.5 edits live. No gate in `make test` compiles or runs the TypeScript.

**My evidence.**
```
$ sed -n '28,31p' $S/browser/cpp/BrowserRuntimeAbi.cpp
extern "C" std::uint32_t synth_browser_abi_version()
{
    return 6;
}
$ grep -rn "abiVersion: 6" $S/browser/ | wc -l
      24
$ grep -rln "abiVersion: 6\|SUPPORTED_BROWSER_ABI_VERSION\|synth_browser_abi_version" $S/browser/ | wc -l
      25
$ sed -n '94,96p' $S/browser/src/catalog.ts
function exactVersion(value: unknown, supported: number, path: string): number {
  if (value !== supported) fail(path, `unsupported version ${String(value)}; supported version is ${supported}`);
  return supported;
}
$ grep -n "SUPPORTED_BROWSER_ABI_VERSION" $S/browser/src/protocol-versions.js
12:export const SUPPORTED_BROWSER_ABI_VERSION = 6;
$ sed -n '150,152p' $CH/proposal.md
§8.0's sweep therefore covers `projects/synth/src`,
`projects/synth/include/synth`, `projects/synth/include/synth/browser`,
`projects/synth/runtime`, `projects/synth/tests`, and `openspec/`.
```
(The reports' file counts differ — B says 17, A says 21 — because each greped a different pattern
set. Under the union of the three version patterns the count is 25 files; the 24 `abiVersion: 6`
literals are not in dispute.)

**Remedy.** `projects/synth/browser/` is in the sweep directories named in the proposal's Impact
and in tasks 1.1, 1.6, 7.1 and 7.5; task 3.5 carries the version-literal sweep as its own numbered
step with the site count written down; and the browser TypeScript suite is among the gates
baselined in 1.5 and re-run in 7.2.

---

## ADJ-09 — smi-17's two checks are green against the untouched tree, and greener still if the feature is inert

**Sources:** E-ADV3, F-ADV3. **CONFIRMED. Blocks execution.**

**What breaks.** Nothing in the synth reads an inbound system-exclusive message. `BasicMidi::IsSysEx()`
has no production caller; its only callers are seven test assertions, every one of them on
outbound traffic. The string "template change" appears nowhere in the source. So smi-17's
scenario 1 ("every mapping is byte-identical to what it was before the message") is satisfied on
an empty diff, and scenario 2's notification bound is satisfied by **zero** notifications — which
is today's behaviour and also the behaviour of a limiter implemented as a blanket drop with no
observer. The clause that distinguishes a token bucket from a drop, "preserving the first message
of a burst", appears in the requirement prose and in no assertion. The change buys no hardening it
can demonstrate, and group 5 must first build a subsystem no task names.

**My evidence.**
```
$ grep -rn "IsSysEx" $S --include='*.hpp' --include='*.cpp'
include/synth/MidiController.hpp:67:    bool IsSysEx() const { … }
tests/parameter_modulation_tests.cpp:7425:    REQUIRE_TRUE(sink.sent[3].IsSysEx());
tests/parameter_modulation_tests.cpp:7441:    REQUIRE_TRUE(sink.sent[4].IsSysEx());
tests/parameter_modulation_tests.cpp:7446:    REQUIRE_TRUE(midi.IsSysEx());          # on LaunchpadColorSysex(), outbound construction
tests/parameter_modulation_tests.cpp:7955:    REQUIRE_TRUE(sink.sent[0].IsSysEx());
tests/parameter_modulation_tests.cpp:7992:    REQUIRE_TRUE(sink.sent[1].IsSysEx());
tests/parameter_modulation_tests.cpp:8196:        sawSysex = sawSysex || midi.IsSysEx();   # inside for(… : sink.sent)
tests/parameter_modulation_tests.cpp:8279:        if (midi.IsSysEx() && …                   # inside for(… : sink.sent)

$ grep -rni "templatechange\|template change" $S/src $S/include $S/runtime $S/browser/src ; echo "rc=$?"
rc=1
$ grep -rn "0x77" $S/src/MidiController.cpp ; echo "rc=$?"
rc=1
$ sed -n '85,96p' $CH/specs/synth-midi-instrument/spec.md
… THE synth system SHALL rate limit inbound template-change messages per controller, preserving the first message of a burst …
- **THEN** every mapping is byte-identical to what it was before the message
- **AND** observer notifications for template changes over that interval do not exceed the rate limit's configured rate times the interval's duration
```

**Remedy.** The artifacts state that group 5 must first build inbound template-change recognition
and the observer notification, and smi-17's flood scenario carries a positive assertion silence
cannot satisfy — the first message of the burst IS observed, and the count over the window is at
least one and at most rate × duration. Task 5.3 carries the positive control: with the limiter
removed the count exceeds the bound, and with it the same run falls under.

---

## ADJ-10 — sru-64's render seam carries one string per option and has nowhere to put a list

**Sources:** B6, C6, E-ADV11, F-ADV12. **CONFIRMED. Blocks execution.**

**What breaks.** sru-64 requires the Controllers page to "render each of them, in the order the
descriptor carries them, alongside that option", and task 6.3 names `BuildAddPresetOptions` as the
site. That function returns `std::vector<ui::ControlOption>`, and `ControlOption` is two strings.
There is no field for a list and no written statement of what "alongside that option" means in a
combo. The executor must choose between folding the strings into `label`, adding a field to
`ControlOption` (a shared portable-UI type used in nine files including the JUCE backend, and
absent from the proposal's Impact), or rendering somewhere other than the option, which
contradicts the requirement's own wording. Three different diffs, three different checks, nothing
written decides between them — and the cheapest, label concatenation, makes "in the order carried"
unobservable as ordering.

**My evidence.**
```
$ sed -n '77,80p' $S/include/synth/PortableUI.hpp
struct ControlOption {
    std::string id;
    std::string label;
};
$ sed -n '962,970p' $S/include/synth/ControllersPageUI.hpp
inline std::vector<ui::ControlOption> BuildAddPresetOptions(
    const std::vector<ControllerWizardDescriptor>& layouts)
{
    std::vector<ui::ControlOption> options;
    options.reserve(layouts.size() + 4);
    for (const ControllerWizardDescriptor& descriptor : layouts)
    {
        options.push_back({descriptor.id, descriptor.displayName});
    }
$ grep -rIl "ControlOption" $S | wc -l
       9
$ grep -c "PortableUI.hpp" $CH/proposal.md
0
```

**Remedy.** design.md decides the presentation before dispatch and sru-64's wording names that
site — which node carries the text, whether `ControlOption` grows a field, and what
`ControllersPageRendersAPresetsDeclaredPreconditionsWhereItIsOffered` asserts against. If the
option label is not the carrier, the check asserts the label is unchanged, so label-stuffing
cannot satisfy it. If `ControlOption` does grow a field, `PortableUI.hpp` is in the Impact.

---

## ADJ-11 — Sheaf group 2's rebase has no refs to act on, and task 7.6's fork remote does not exist

**Sources:** B5, F-ADV14; A9, E-ADV15 (same defect, filed SHOULD-FIX). **CONFIRMED. Blocks
execution of group 2; blocks delivery at 7.6.**

**What breaks.** Task 2.1 confirms a branch parentage and task 2.2 says "Rebase #14, #15 and this
branch onto #13's, #14's and #15's own current tips respectively, and confirm each still applies."
None of the three branch names resolves to any ref, local or remote. The work is already linear on
one branch, `midi-resilience-merge`, so there is nothing to rebase and no tip to rebase onto, and
2.1's "Any other order invalidates the sequencing this group and group 3 assume" gives no
disposition for "the branches are gone". Task 7.6 says "push this branch to the fork and open the
next sequential pull request" — the only remote configured is upstream `jvictor0/Sheaf`, and the
branch has no upstream. design.md's supporting parenthetical is also false: `git branch -a`
returns eleven refs, not two. (Its conclusion — no reachable ref for either branch — is true.)

**My evidence.**
```
$ cd $W/External/Sheaf && for b in app-midi-catalog shift-and-file-export launchpad-model-on-the-row; do git rev-parse --verify "$b" 2>&1 | head -1; done
fatal: Needed a single revision
fatal: Needed a single revision
fatal: Needed a single revision
$ git branch -a | wc -l
      11
$ git remote -v
origin	https://github.com/jvictor0/Sheaf.git (fetch)
origin	https://github.com/jvictor0/Sheaf.git (push)
$ git rev-parse --abbrev-ref --symbolic-full-name '@{u}'
fatal: no upstream configured for branch 'midi-resilience-merge'
$ git log --oneline -4
caae5c2e Carry the MIDI controller resilience change into the submodule
ba3898e4 Let a Launchpad row say which Launchpad it is
ddb14693 Record the delivery of shift-and-file-export: pushed to the fork as jvictor0/Sheaf#14 …
105efccf Add Shift, a held kind that gives a button a second job, and a file export seam
$ sed -n '71,72p' $CH/design.md
… and this worktree has no reachable ref for either branch
in any case (`git branch -a` here returns only `main` and this branch).
```
(Separately verified sound: the three `rework-controllers-block-editing` commits task 4.4 cites —
`1d97e5ca`, `582e400a`, `185a7a09` — are all ancestors of HEAD.)

**Remedy.** Tasks 2.1 and 2.2 are rewritten against the tree that exists: confirm by SHA that
#13's, #14's and #15's commits are ancestors of this branch, in order, and drop the rebase. Task
7.6 names the fork remote that must be added and states what the pull request contains, given that
this branch carries #13's, #14's and #15's commits. design.md's `git branch -a` parenthetical is
corrected or removed.

---

## ADJ-12 — frogg3rs group 4 cannot compile in the order given, and no task advances the submodule pin

**Sources:** B7 only. **CONFIRMED. Blocks execution.**

**What breaks.** Tasks 4.1 and 4.2 populate `declaredPreconditions` on the device defaults. That
field exists in no source file in either repository — it is created by Sheaf's scw-6, which is
unexecuted; the submodule carries only the change artifacts. An executor working the frogg3rs
tasks in the order given reaches 4.1, edits `FroggersMidiCatalog.hpp`, and gets a compile error,
with no instruction covering it. The dependency is stated only at delivery (task 6.6). And no task
anywhere advances the submodule pin, which is the mechanical step that makes the field visible to
`app/` — the only mention of the pin in the whole task list is 6.6's hygiene assertion that there
is no pin dirt. The true order is Sheaf groups 1-7 → pin move in frogg3rs → frogg3rs group 4 →
frogg3rs group 6, and the artifacts state a different one.

**My evidence.**
```
$ grep -rn "declaredPreconditions" $W/app/ $W/External/Sheaf/projects/ ; echo "rc=$?"
rc=1
$ sed -n '31,38p' $S/include/synth/MidiAppCatalog.hpp
struct MidiAppDeviceDefault {
    std::string id;          // wizard id, stored in MidiControllerSlot::wizardId
    std::string displayName; // dropdown label
    MidiProfileKind kind = MidiProfileKind::Generic;
    std::vector<std::string> inputAliases;
    std::vector<std::string> outputAliases;
    MidiControllerProfileConfig config;
};
$ grep -ni "submodule\|\bpin\b" $FCH/tasks.md
95:      both levels and no submodule-pin dirt.
```

**Remedy.** The frogg3rs tasks carry an explicit gate at the head of group 4 stating that it does
not start until Sheaf's scw-6 has landed, and the submodule-pin advance exists as its own numbered
task naming the commit it points at.

---

## ADJ-13 — The APC40 mkII (Ableton) default is required to declare a precondition the code and the manual both deny

**Sources:** A3, C3. **CONFIRMED. Blocks execution and delivery.**

**What breaks.** Task 4.2 says "Populate **both** APC40 mkII defaults' `declaredPreconditions` …
which carries the Track 1 caveat", and the delta's scenario reads "**WHEN** either APC40 mkII
device default is read **THEN** it declares that Track 1 must stay selected". Both are false for
the Ableton default, which exists specifically to remove that caveat — the catalogue comment says
so, and so does the manual. Executing as written writes a false precondition into the product;
Sheaf's sru-64 then renders it to the operator at the moment they choose the preset; and task
4.4's drift check would force the generated manual to assert, for the Ableton preset, a caveat
`MANUAL.md` currently and correctly denies. That is the WYSIWYG failure the pair exists to fix,
introduced by the fix.

**My evidence.**
```
$ sed -n '26,32p' $W/app/FroggersMidiCatalog.hpp
// APC40 mkII (Generic): the unit's eight device knobs follow whichever
// Track Select button is lit (track 1 = channel 0), so Track 1 must stay
// selected -- pressing another track moves those eight knobs to another
// channel and they stop responding to this default until Track 1 is
// pressed again. The Ableton default exists to avoid that caveat: it
// opens with a connect-time message that keeps the unit's sixteen knobs
// on channel 0 regardless of which track is selected.

$ sed -n '182,187p' $W/app/FroggersMidiCatalog.hpp
inline synth::MidiAppDeviceDefault Apc40AbletonDeviceDefault() {
    synth::MidiAppDeviceDefault device = Apc40GenericDeviceDefault();
    device.id = "froggers.apc40.ableton";
    device.displayName = "Akai APC40 mkII (Ableton)";
    device.config.openSysEx = {{0xF0, 0x47, 0x7F, 0x29, 0x60, 0x00, 0x04, 0x41, 0x09, 0x07, 0x01, 0xF7}};

$ sed -n '355,356p' $W/MANUAL.md
The same mapping, and the app switches the unit into Ableton mode when its output connects, so the
device knobs stay on encoders 9 to 16 whatever track is selected.

$ sed -n '47,49p' $FCH/specs/froggers-midi-controller-mappings/spec.md
#### Scenario: The APC40 preset declares its track-selection caveat
- **WHEN** either APC40 mkII device default is read
- **THEN** it declares that Track 1 must stay selected, naming the consequence of selecting another track
```

**Remedy.** The scenario is split: the Generic default declares the Track 1 caveat, and the
Ableton default declares the connect-time SysEx precondition it actually has, or an empty list —
which is worth stating explicitly, since it exercises scw-6's empty-list case. Task 4.2 is reworded
to match, and cites `app/FroggersMidiCatalog.hpp:26-35`, which is where the caveat actually lives
(task 4.2 currently says "the same comment block" as 4.1's `:14-24`, which does not contain it).

---

## ADJ-14 — The Launch Control XL scenario's delivering check is an already-green test that asserts nothing about it, and the count assertions it will break are named nowhere

**Sources:** A2, C1, E-ADV6, F-ADV2. **CONFIRMED. Blocks execution.**

**What breaks.** Three parts of one defect.

(a) The delta scenario "The preset offers scene blend on a fader" declares its Check as the
existing, already-passing
`app/FroggersMidiCatalogTests.cpp: device_defaults_are_valid_and_address_exactly_the_documented_controls`
— not "not yet delivered", as the other LCXL and precondition scenarios do. That test passes today
with no LCXL in the catalogue and asserts nothing about one: every `sceneBlend` assertion in the
file is inside a loop over `{&generic, &ableton}` hard-coded to channel 0 CC 15. `check_spec_checks_resolve`
passes because the test *name* resolves, and task 6.4's gate is satisfied on its face by a check
that tests nothing relevant.

(b) Task 5.1 adds a seventh device default. Three assertions pin the count at six, across two test
binaries. No task names them, and task 5.1 does not say where in the catalogue the new entry goes
— the test also indexes `deviceDefaults[0..2]` positionally, and `real_catalog_registers_one_descriptor_per_device_default`
pairs `registry[ix]` against `deviceDefaults[ix]`, so an inserted rather than appended entry
misreads the Twister and APC40 blocks in both binaries.

(c) Neither `app/FroggersMidiCatalogTests.cpp` nor `app/FroggersControllersPageTests.cpp` appears
in the proposal's Impact, although tasks 4.1, 4.2, 5.2 and 5.4 all edit them.

**My evidence.**
```
$ grep -rIn "size() == 6" $W/app/*.cpp
app/FroggersControllersPageTests.cpp:99:    REQUIRE_TRUE(catalog.deviceDefaults.size() == 6);
app/FroggersControllersPageTests.cpp:109:    REQUIRE_TRUE(registry.size() == 6);
app/FroggersControllersPageTests.cpp:287:    REQUIRE_TRUE(rows.size() == 6);          # Twister system-message rows, not the catalogue
app/FroggersMarblesClockTests.cpp:240:    REQUIRE_TRUE(layouts.size() == 6);        # banks, not the catalogue
app/FroggersMidiCatalogTests.cpp:399:    REQUIRE_TRUE(catalog.deviceDefaults.size() == 6);
app/FroggersMidiCatalogTests.cpp:434:    REQUIRE_TRUE(twister.config.systemMessages.size() == 6);  # side buttons, not the catalogue

$ sed -n '416,418p' $W/app/FroggersMidiCatalogTests.cpp
    const synth::MidiAppDeviceDefault& twister = catalog.deviceDefaults[0];
    const synth::MidiAppDeviceDefault& generic = catalog.deviceDefaults[1];
    const synth::MidiAppDeviceDefault& ableton = catalog.deviceDefaults[2];

$ sed -n '110,113p' $W/app/FroggersControllersPageTests.cpp
    for (std::size_t ix = 0; ix < registry.size(); ++ix) {
        REQUIRE_TRUE(registry[ix].id == catalog.deviceDefaults[ix].id);

$ sed -n '508,510p;527,531p' $W/app/FroggersMidiCatalogTests.cpp
    for (const synth::MidiAppDeviceDefault* device : {&generic, &ableton}) {
        REQUIRE_TRUE(device->config.analogInput.has_value());
        REQUIRE_TRUE(device->config.analogInput->sceneBlend.has_value());
        REQUIRE_TRUE(device->config.analogInput->sceneBlend->channel == 0);
        REQUIRE_TRUE(device->config.analogInput->sceneBlend->cc == 15);

$ grep -c "FroggersMidiCatalogTests\|FroggersControllersPageTests" $FCH/proposal.md
0
$ python3 app/check_spec_checks_resolve.py app
check-spec-checks-resolve: OK - 68 Check reference(s) resolved, 12 declared as having no automated check
```
Exactly three catalogue-count assertions: `FroggersMidiCatalogTests.cpp:399`,
`FroggersControllersPageTests.cpp:99` and `:109`. (Report C's prose says `:110`; the literal is at
`:109`. `:110` is the loop head.)

**Remedy.** The LCXL scenario's Check reads "not yet delivered" and names the task that extends the
catalogue test with the new entry's kind, aliases, analog section and scene-blend address. Group 5
carries a task naming all three count assertions by file and line with the new count, and stating
that the entry is appended (or pinning its position explicitly). Both test files are in the
proposal's Impact.

---

## ADJ-15 — "Only the Twister carries Shift or shifted jobs" is checked over three named devices, not every default, and task 5.3 forbids adding a check

**Sources:** C2, E-ADV5, F-ADV1. **CONFIRMED. Blocks execution.**

**What breaks.** The delta widens the scenario's THEN from "no APC40 or Launchpad association" to
"no non-Twister device default", keeps the same Check, and task 5.3 then rules that "no new Check
line is needed for it" on the strength of that generality. The named test has no such generality.
Its only negated `shiftedPress` assertions are the Twister's own Shift button, a loop over exactly
`{&generic, &ableton}`, and a loop in a *different* TEST_CASE over a hard-coded Launchpad id list.
Nothing anywhere asserts that a non-Twister default lacks a `MessageIn::Type::Shift` press — the
two `Type::Shift` assertions are positive ones on the Twister. A seventh default falls into no
loop. The only thing that forces the executor to touch this test is `size() == 6`, and the
accepting edit is one character.

**My evidence.**
```
$ grep -n "shiftedPress" $W/app/FroggersMidiCatalogTests.cpp | grep '!'
476:    REQUIRE_TRUE(!twisterShift.shiftedPress.has_value());
543:            REQUIRE_TRUE(!assoc.shiftedPress.has_value());
596:            REQUIRE_TRUE(!assoc.shiftedPress.has_value());
$ sed -n '510p' $W/app/FroggersMidiCatalogTests.cpp
    for (const synth::MidiAppDeviceDefault* device : {&generic, &ableton}) {
$ sed -n '587,590p' $W/app/FroggersMidiCatalogTests.cpp
TEST_CASE(launchpad_defaults_positions_carry_their_own_controller) {
…
    for (const LaunchpadPresetId& preset : kLaunchpadPresetIds) {
$ grep -n "Type::Shift" $W/app/FroggersMidiCatalogTests.cpp
471:    REQUIRE_TRUE(twisterShift.press.type == synth::MessageIn::Type::Shift);
474:    REQUIRE_TRUE(twisterShift.release->type == synth::MessageIn::Type::Shift);
$ sed -n '72,75p' $FCH/tasks.md
- [ ] 5.3 Check that the preset maps no Shift button and no shifted job. The
      `froggers-midi-controller-mappings` requirement's "Only the Twister
      carries Shift or shifted jobs" scenario covers this generically, over
      every non-Twister default; no new Check line is needed for it.
```

**Remedy.** Task 5.3's ruling is deleted and 5.3 becomes the check it declines: one loop over
`catalog.deviceDefaults` asserting, for every entry whose id is not the Twister's, that no
association carries a `shiftedPress` and no `press.type == MessageIn::Type::Shift`, with the
positive control that giving a non-Twister default a shifted press turns it red. (The file already
has `RequireDeviceDefault(catalog, id)` for id-based lookup.)

---

## ADJ-16 — The one new gate this change adds is green on the exact sentence it exists to reject

**Sources:** E-ADV1; F-ADV15 (weaker form of the same). **CONFIRMED. Blocks execution.**

**What breaks.** Task 4.4's second job is stated as a check that fails "when the manual's
stuck-modifier recovery text names no trigger". The defective sentence at `MANUAL.md:319-320`
already names one: `HeldModifierClearSource`'s five values are `Release`, `Rebuild`,
`EndpointOpen`, `SecondPress`, `Ceiling`, and the sentence says the recovery is a press and
*release*. So the gate is green before task 3.1 runs, green if task 3.1 is skipped, and green on
the very text whose unreachability is the change's motivating incident. Separately, the delta
scenario's THEN is a conjunction — the manual "states the triggers … **and** does not state a
recovery that requires the Shift address to transmit" — and task 4.4 checks only the first half.
The false half is the second.

**My evidence.**
```
$ sed -n '319,320p' $W/MANUAL.md
with the patch like every other mapping. If the controller is unplugged while Shift is still held, its
buttons stay shifted until Shift is pressed and released again.

$ sed -n '33,35p' $FCH/specs/froggers-midi-controller-mappings/spec.md
- **THEN** the manual's Shift subsection states the triggers that end a held modifier and does not state a recovery that requires the Shift address to transmit
- **AND** it states the same for Hold Drill
- Check: not yet delivered; task 4.4 adds a check script that fails when the manual's recovery text names no trigger

$ sed -n '82p' $CH/design.md
`HeldModifierClearSource` enumerating `Release`, `Rebuild`, `EndpointOpen`,
```

**Remedy.** Task 4.4 states the check's assertion as something the current text FAILS — both
halves of the scenario's conjunction, including the absence of a recovery whose only actor is the
Shift button itself — and the task requires proving it red against the unmodified
`MANUAL.md:319-320` **before** task 3.1, not after.

---

## ADJ-17 — Five tasks are hardware-blocked and 2.1 names no MIDI monitor; task 6.6 makes no delivery ruling

**Sources:** B8 only. **CONFIRMED. Blocks delivery.**

**What breaks.** Task 2.1 is an operator/hardware step whose deliverable is eight faders' channel
and CC plus a template identifier — sixteen-plus values, none written down and none inventable.
Tasks 2.2, 5.1, 5.2 and 5.4 all depend on it. The delta ships three scenarios whose Check lines
point at those tasks. Task 6.4 permits saying "not yet delivered", but task 6.6 then pushes to
`main` a change whose headline capability — the Launch Control XL preset — does not exist, and
whether that is acceptable is a delivery decision the artifacts never make. Second defect in the
same task: 2.1 requires verifying "a MIDI monitor shows traffic on fader movement" before reading
anything. No monitor is named, none exists in either tree, and the sandbox rule forbids installing
one.

**My evidence (run now, with the contrast control, because `system_profiler` is empty by
construction on this machine and would be a false null).**
```
$ ioreg -p IOUSB -w 0
+-o Root  <class IORegistryEntry, id 0x100000100, retain 30>
  +-o AppleT8112USBXHCI@00000000  <class AppleT8112USBXHCI, id 0x1000003f6, …>
  | +-o USB Hub@00100000  <class IOUSBHostDevice, id 0x10031d2be, …>
  | | +-o Portable SSD T5@00110000  <class IOUSBHostDevice, id 0x10031d323, …>
  | +-o USB TO DP HDMI@00200000  <class IOUSBHostDevice, id 0x10031d2d8, …>
  +-o AppleT8112USBXHCI@01000000  <class AppleT8112USBXHCI, id 0x10000036e, …>
$ ioreg -p IOUSB -w 0 | grep -i "novation\|launch" ; echo "grep exit=$?"
grep exit=1
$ system_profiler SPUSBDataType 2>/dev/null | wc -l
       0
$ sed -n '21,24p' $FCH/tasks.md
- [ ] 2.1 **Operator/hardware step.** With a Novation Launch Control XL
      connected over USB and set to one pinned template, verify the device is
      actually connected (it enumerates over USB, and a MIDI monitor shows
      traffic on fader movement) before reading anything.
```
The proposal's own hardware claim is exactly true, and the artifacts assert no CC number anywhere —
that part is handled honestly. What is missing is the monitor and the delivery ruling.

**Remedy.** Task 2.1 names the MIDI monitor the operator is to use, or the command, or the scratch
program to be built from the tree. Task 6.6 states whether delivery is permitted with group 5
undone — if it is, it says so; if it is not, it says the change does not deliver until the device
is attached.

---

## ADJ-18 — The overlap disposition is wrong by path: the live collision is `app/Makefile` and `app/check_common.py`, at the exact lines task 4.4 edits

**Sources:** A4, D2; B14, C14 (same defect, raised to BLOCKING in the exchange). **CONFIRMED.
Blocks execution.**

**What breaks.** The proposal's disposition for `frogg3rs-delay-capacity-and-width-finish` reads
"its task 2.1 edits `MANUAL.md` and `QUICK_DICT.md`'s Delay **Stereo width** entries … Same two
files, disjoint section (Delay, not MIDI)". Neither of those files is modified in the main
checkout. What is modified there is `app/Makefile` — at the `.PHONY` line, at the region
immediately after `check-artifact-symbols-resolve`, and at the `test:` aggregate, which are the
three places task 4.4 must write — plus `app/check_common.py`, which has gained a `strip_comments`
helper this worktree's copy lacks and which any new check script imports, and
`app/check_docs_match_parameter_table.py`, the drift check task 4.3/4.4's generator is modelled
on. An executor told the overlap is a disjoint documents section will not look at the file it is
about to conflict in. The state figure is also wrong, and the proposal hedged it ("state as
reported to this worktree, not independently verified here") rather than running the one read-only
command that settles it. The collision is done work, not a future risk: that change's task 1.4a
("The pinned-parameter gate, wired into `app/Makefile`") is already `[x]`.

**My evidence**, run read-only in `/Users/diegoaguilar-canabal/Desktop/frogg3rs`:
```
$ git status --short -- MANUAL.md QUICK_DICT.md
(no output)
$ git status --short | grep -v '^D  openspec'
 M app/FroggersAudioRoutingTests.cpp
 M app/FroggersDspParityTests.cpp
 M app/Makefile
 M app/check_common.py
 M app/check_docs_match_parameter_table.py
 M app/dsp/Delay.hpp
?? app/check_delay_capacity_break_proofs.py
?? app/check_delay_capacity_parameters_are_swept.py
?? openspec/changes/
$ git diff --unified=0 -- app/Makefile | grep '^@@'
@@ -22 +22,6 @@ CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2
@@ -147 +152 @@ DSP_TEST_BIN := $(BUILD_DIR)/froggers_dsp_parity_tests
@@ -239,0 +245,19 @@ check-artifact-symbols-resolve:
@@ -320 +344 @@ check-catalog-covers-screen-actions:
$ f=openspec/changes/frogg3rs-delay-capacity-and-width-finish/tasks.md
$ echo "done=$(grep -c '^- \[x\]' $f) total=$(grep -cE '^- \[[ x]\]' $f)"
done=17 total=41
$ grep -n "1.4a" $f
45:- [x] 1.4a The pinned-parameter gate, wired into `app/Makefile`, with two
$ grep -c "strip_comments" app/check_common.py ; grep -c "strip_comments" .claude/worktrees/midi-resilience/app/check_common.py
1
0
$ git status --short -- openspec/changes/frogg3rs-delay-width-wysiwyg-repair/ | wc -l
      22      # staged deletions; the directory is gone from main's disk
```
(The proposal's other row — `frogg3rs-delay-width-wysiwyg-repair` at 0/26 in this worktree — is
correct as a count: 0 of 26. But main has that change staged for deletion, so task 3.2's "Confirm
both have released the files" is asked about a change that on main no longer exists.)

**Remedy.** Both overlap rows are rewritten against the actual diff, scoped by path:
`app/Makefile`, `app/check_common.py`, `app/check_docs_match_parameter_table.py`,
`app/dsp/Delay.hpp`; the count corrected to 17/41 or re-measured at the gate rather than carried;
which of the two Delay changes is live and that the other is being deleted; and task 3.2's
coordination list carries `app/Makefile` with the same never-stage-a-whole-file discipline. Task
4.4 cites `app/Makefile` by target name, not by the line numbers, which are already stale upstream.

---

## ADJ-19 — The active-change enumeration reads one checkout of four; a divergent duplicate of this change and a third `MANUAL.md` holder are unnamed

**Sources:** D3 only. **CONFIRMED. Blocks execution.**

**What breaks.** The proposal enumerates "Two changes … active in this repository's own
`openspec/changes/`, a third … in the main checkout". `git worktree list` shows four checkouts.
Two facts it misses change execution. First, a second, untracked, textually divergent copy of
`frogg3rs-midi-preset-preconditions` exists in the `midi-controller-resilience` worktree, deltaing
the same capability — all four artifacts differ. Whichever copy an executor opens, it is editing
one of two, and if both ever reach one tree they collide on `froggers-midi-controller-mappings`.
Second, `frogg3rs-density-documents-and-spec` (0/14) in that same worktree edits `MANUAL.md` and
`QUICK_DICT.md`. Task 3.2 tells the executor to coordinate with two named changes; three hold the
file.

**My evidence**, run in `/Users/diegoaguilar-canabal/Desktop/frogg3rs`:
```
$ git worktree list
…/frogg3rs                                               0260ce9 [main]
…/frogg3rs/.claude/worktrees/midi-controller-resilience  9838862 [worktree-midi-controller-resilience]
…/frogg3rs/.claude/worktrees/midi-resilience             6e77142 [worktree-midi-resilience]
…/frogg3rs/.claude/worktrees/randomize-depth-reclaim     e38e910 [worktree-randomize-depth-reclaim]
$ ls .claude/worktrees/midi-controller-resilience/openspec/changes/
frogg3rs-density-documents-and-spec
frogg3rs-midi-preset-preconditions
$ diff -rq .claude/worktrees/midi-resilience/openspec/changes/frogg3rs-midi-preset-preconditions \
           .claude/worktrees/midi-controller-resilience/openspec/changes/frogg3rs-midi-preset-preconditions
Files …/design.md and …/design.md differ
Files …/proposal.md and …/proposal.md differ
Files …/specs/froggers-midi-controller-mappings/spec.md and …/spec.md differ
Files …/tasks.md and …/tasks.md differ
$ git -C .claude/worktrees/midi-controller-resilience status --short
 ? External/Sheaf
?? openspec/changes/frogg3rs-midi-preset-preconditions/
$ D=.claude/worktrees/midi-controller-resilience/openspec/changes/frogg3rs-density-documents-and-spec
$ grep -n "MANUAL\.md\|QUICK_DICT\.md" $D/tasks.md
98:- [ ] 2.1 `MANUAL.md` and `QUICK_DICT.md` for the two Filter controls and both
105:- [ ] 2.3 `MANUAL.md` states plainly that raising Density trades smoothness for
$ echo "done=$(grep -c '^- \[x\]' $D/tasks.md) total=$(grep -cE '^- \[[ x]\]' $D/tasks.md)"
done=0 total=14
$ grep -c "density-documents" .claude/worktrees/midi-resilience/openspec/changes/frogg3rs-midi-preset-preconditions/proposal.md
0
```

**Remedy.** The overlap table is re-derived from `git worktree list` plus a `git status --short`
and `ls openspec/changes/` in each of the four checkouts, with every disposition scoped by path;
`frogg3rs-density-documents-and-spec` is in task 3.2's coordination list; and the artifacts say
explicitly which copy of `frogg3rs-midi-preset-preconditions` is authoritative and that the other
is deleted before execution, not merged.

---

## ADJ-20 — `check-artifact-symbols-resolve` is red at baseline and no artifact says so; five of the fifteen failures are this change's own split

**Sources:** D1 (BLOCKING, withdrawn to SHOULD-FIX in the exchange), A5, C9, E-ADV14, F-ADV11,
B13. **CONFIRMED as fact. Severity ruled below: blocks delivery, not execution.**

**What breaks.** The gate exits 1 today with fifteen unresolvable names, all raised against the
sibling change `frogg3rs-delay-width-wysiwyg-repair`'s artifacts. Five of them cite
`openspec/changes/frogg3rs-midi-controller-resilience/`, the superseded change this pair's own
collecting commit removed. Task 1.4 says "Baseline every `app/` gate with counts" and task 6.3
says "Re-run every gate baselined in 1.4, naming which moved and which were carried forward" — but
unlike Sheaf's task 1.5, which names its two carried failures explicitly, the frogg3rs artifacts
name none. The implied baseline is green; an executor meeting a red gate has nothing written down
to compare against, and the change ships carrying a red gate it did not fix and did not record.

**Severity ruling.** Reports B, C, E and F escalated this to blocks-execution on the ground that
`make -C app test` aborts at the gate stage, so task 4.4's "prove it goes red" has no before/after
instrument. D, who raised it, withdrew to blocks-delivery on the ground that every gate has its own
target and every binary its own path. **I rule with D and A: blocks delivery.** Task 4.4's red
proof is of the *new* script, which is run directly (`python3 app/check_docs_match_device_preconditions.py app`)
and does not require the aggregate target; the executor is delayed and misinformed, not stopped,
and nothing must be invented to proceed. What cannot ship is a change whose postflight cannot
distinguish carried red from caused red. Both positions are recorded here.

**The attribution is not open.** Report B marked "these five are the split's own damage"
unverifiable. The collecting commit settles it in its own body.

**My evidence.**
```
$ cd $W && python3 app/check_artifact_symbols_resolve.py app >/dev/null 2>&1 ; echo "rc=$?"
rc=1
$ python3 app/check_artifact_symbols_resolve.py app 2>&1 | tail -1
check-artifact-symbols-resolve: FAIL - 15 unresolvable name(s) in change artifacts
$ python3 app/check_artifact_symbols_resolve.py app 2>&1 | grep FAIL | grep -o 'frogg3rs-[a-z0-9-]*' | sort | uniq -c
  15 frogg3rs-delay-width-wysiwyg-repair      # the file raising them
   5 frogg3rs-midi-controller-resilience      # the split-deleted directory
   2 frogg3rs-post-expansion-consolidation
   8 frogg3rs-randomize-depth-reclaim
$ git log -1 --format='%b' 6e77142 | sed -n '9,12p'
They supersede an app-level change of the same name written on 2026-09-10,
which carried both halves and pointed almost entirely into the submodule. It
reached 1 of 42 tasks, was never committed anywhere, and is removed.
```
The other nine gates are green (see §3 below), so this is the only red one.

**Remedy.** Task 1.4 records the carried failure with its exit code and its path-scoped
breakdown — five citations of the split-deleted directory, eight of a directory that lives in
another worktree, two of the untracked archive — and task 6.3's pass condition is "the same
fifteen, no new ones". The five that dangle because of this pair's own split are repaired inside
this change, since this change's split is what made them dangle.

---

## ADJ-21 — Task 1.4 baselines eight of the ten gates `make test` runs, and the omitted one is the catalogue gate

**Sources:** A6, B12, F-ADV10 (BLOCKING); C15 (SHOULD-FIX). **CONFIRMED. Severity ruled: blocks
delivery.**

**What breaks.** `app/Makefile`'s `test:` target has ten `check-*` prerequisites. Task 1.4 and the
proposal's Impact both name eight, by recipe line number. The two omitted are `check-no-juce` and
— the material one — `check-catalog-covers-screen-actions`, the gate that parses
`FroggersMidiCatalog.hpp`, the file this change edits and to which group 5 adds a seventh device
default. Task 6.3 re-runs only "every gate baselined in 1.4", so neither is ever compared against
the diff. Task 1.4 also names no test binary, although `make test` builds twelve, two of which
carry every new case this change writes.

**Severity ruling.** A6's own text says "blocks delivery", and C graded it SHOULD-FIX on the
ground that a serial `make test` still runs both omitted gates. Both are describing the same
break: what is lost is the recorded baseline, not the execution. **Blocks delivery.** An executor
can proceed without inventing anything; the change cannot ship with a postflight that is silent on
two of ten gates, one of them over the file the change rewrites.

**My evidence.**
```
$ grep -n '^check-' $W/app/Makefile
175:check-no-firmware-includes:
182:check-microphone-usage:
190:check-docs-match-parameter-table:
198:check-spec-checks-resolve:
207:check-citations-resolve:
220:check-modified-requirements-restate-promoted:
227:check-no-planning-history:
237:check-artifact-symbols-resolve:
244:check-no-juce: $(NO_JUCE_CHECK)
316:check-catalog-covers-screen-actions:
$ sed -n '320p' $W/app/Makefile | tr ' ' '\n' | grep -c '^check-'
10
$ sed -n '320p' $W/app/Makefile | tr ' ' '\n' | grep -c '^\$('
12
$ sed -n '13,16p' $FCH/tasks.md
- [ ] 1.4 Baseline every `app/` gate with counts before touching anything — the
      eight scripts `app/Makefile` runs at `:176`, `:183`, `:191`, `:199`,
      `:208`, `:221`, `:228`, `:238` — plus `./app/build-launcher.sh`.
$ sed -n '316,318p' $W/app/Makefile
check-catalog-covers-screen-actions:
	@bash $(APP_DIR)/check_catalog_covers_screen_actions.sh "$(APP_DIR)" \
	    kSceneBlend kEncoderPress kEncoderDrag kInputSelect kViewportNarrow
```

**Remedy.** Task 1.4 and the Impact name the baseline as "every `check-*` prerequisite of
`app/Makefile`'s `test:` target", read from the Makefile rather than copied, so the enumeration
cannot drift; the twelve test binaries are in the baseline too.

---

# 2. SHOULD-FIX and NOTE, consolidated

Severity here is as graded after the exchange. "Verified" means I opened it myself.

| id | sources | finding | severity | status |
|---|---|---|---|---|
| S-01 | A7 | design.md:5 says `HoldDrillState` and `ShiftState` "are both built by the catalog helper `HeldButton()`". `HeldButton` does not exist in Sheaf; it is a frogg3rs app helper building a `MidiControllerSystemMessageAssociation`. Task 1.1 lists it as a Sheaf operand, so the preflight will report 0 against a Context sentence asserting it exists. | blocks delivery | **verified**: `grep -rni heldbutton $W/External/Sheaf` returns only the two artifact lines; `app/FroggersMidiCatalog.hpp:79` is the definition. |
| S-02 | A8 | design.md:147-149 rules that `resync` shall not clear a modifier on the stated ground that it "is bound in both places to `engine_.ResetMidiOutputProcessors(ix)`". At the runtime binding it is bound to `Resync(ix)`, which detaches the slot's input processor and reinstalls a forwarder. The ruling's conclusion survives; its description is false, and task 3.8's check is to be written against the wrong mental model. | blocks execution | **verified**: `MidiConnectionManager.hpp:462` `ops.resync = [this](std::size_t ix) { Resync(ix); };`, `Resync` at `:597-605` calls `SetProcessor(nullptr)` and `InstallForwardingProcessor(ix)`. |
| S-03 | A12, B9, C7 | `MidiEndpointOps` has seven declaration sites, not the five task 1.3 instructs the executor to confirm (two production, five test — the extra two are the deliberately unbound cases at `reconcile_tests.cpp:790` and `reconcile_executor_tests.cpp:353`, the latter being `null_op_is_skipped_without_crashing`). Task 1.2 states a disposition for a differing count; 1.3 states none. Severity disputed A (NOTE) vs B/C (SHOULD-FIX). | **ruled SHOULD-FIX, blocks execution of task 1.3 as written** | **verified**: seven declarations enumerated by grep. |
| S-04 | A12 | `MidiEndpointStatus` "three sites that enumerate all of its values": only two are exhaustive. `DeviceLabel` is an if-chain testing two enumerators and falling through, structurally the same shape as the two single-value guards task 1.2 says do not count. `src/MidiReconcile.cpp`, which branches on the enumeration at sixteen places, is omitted. | NOTE | not adjudicated — verify at the point of repair |
| S-05 | A11, B11, C8, D5, E-ADV16, F-ADV15 | There is no "per-device settings table" to generate. `MANUAL.md:336-339` is a four-line prose paragraph inside the Twister's narrative; the only table nearby is the button map at `:327-334`. The APC40 caveat is mid-paragraph at `:348-351`. Tasks 4.3 and 4.4 generate and gate a structure that does not exist, with no markers, no rendering format, and therefore no written assertion. The paragraph also carries the CC addresses the incident was about, which the source comment does not. | blocks execution | **verified**: `grep -n '^### ' MANUAL.md` shows `322 Twister / 341 APC40 Generic / 353 APC40 Ableton`; `:336-339` read in full, prose, no table markup; `design.md:87` mitigation says "Only the settings table is generated". |
| S-06 | A11, A2(c), D4 | The frogg3rs Impact is incomplete: `app/FroggersMidiCatalogTests.cpp`, `app/FroggersControllersPageTests.cpp`, `app/Makefile`, `MANUAL.md:308-312` (Hold Drill), `MANUAL.md:348-351` and `:353-357` (the APC40 sections) are edited by tasks and named nowhere. | blocks execution | **verified** for the two test files and `app/Makefile` (`grep -c` → 0 in proposal.md); the manual ranges verified by reading Impact `:73-75`. |
| S-07 | A10, B10, C13 | Task 4.1 says "delete the comment those three settings came from" (`:14-24`). Lines `:19-23` of that block are not preconditions — they are the side-button layout — and nothing moves them. Task 4.2 cites "the same comment block" for the APC40 caveat, which lives at `:26-35`. The header comment at `:7-10` enumerates "the six device defaults", which a seventh falsifies, and no task names it. | blocks execution | **verified** by reading `app/FroggersMidiCatalog.hpp:6-35`. |
| S-08 | C10, F-ADV13 | The rewritten Twister requirement adds "and SHALL remain usable in its unshifted form when that precondition is unmet" — the operator failure the change exists for — with no scenario and no check. Task 6.4's unit is the scenario, so the sweep never reaches it. F adds that the clause looks false for each unmet-precondition case the catalogue's own comment describes. | blocks delivery | **verified**: the requirement body and its three scenarios read in full; none asserts it. |
| S-09 | C11 | Sheaf's design points twice at "the frogg3rs half's record" for a Launch Control XL flood that appears in no frogg3rs artifact. A requirement (smi-17), a type, a constant and task group 5 rest on an observation nobody can open. | blocks delivery | **verified**: `grep -rIni flood` across the frogg3rs tree returns one unrelated hit in `app/browser/site/mobile-stack.mjs`. |
| S-10 | C9, D1, D4 | Five dangling `openspec/changes/frogg3rs-midi-controller-resilience/` references in the sibling change, plus two in `HANDOFF.md` at the repository root and one in `app/check_modified_requirements_restate_promoted.py`'s header — the last of which also quotes a replacement scenario title that exists nowhere. The repository root is in no declared sweep. | blocks delivery | **verified** for the five (gate output) and for the script header quote (`git grep "does not outlive the profile that saw it"` → only the comment itself). `HANDOFF.md` not re-verified. |
| S-11 | D7 | `openspec/specs/froggers-midi-controller-mappings/spec.md:4` — the capability this change deltas and archives into — still carries "TBD - created by archiving change frogg3rs-midi-shift. Update Purpose after archive.", and `design.md:51` cites that same dead change id as the authority for the guard this change adds. | blocks delivery | not adjudicated — verify at the point of repair |
| S-12 | D8 | `runtime/Runtime.hpp:9-10` cites `.superpowers/sdd/p3-task-2-brief.md`, which does not exist (and is a planning-document citation inside shipping code); `Engine.hpp:248` cites `projects/synth/miniapp/Main.cpp`, which is at `projects/synth/apps/miniapp/Main.cpp`. Both files are on the Sheaf change's own cited edit path. | blocks delivery | not adjudicated — verify at the point of repair |
| S-13 | D9 | The Sheaf Impact omits `projects/synth/docs/coverage.md` — a maintained per-requirement coverage table that the change immediately below this one in the stack updates — and `projects/synth/scripts/`, which holds three gates `projects/synth test` runs and which task 1.5 baselines. | blocks delivery | **verified**: `docs/coverage.md` exists (134 KB); `grep "coverage.md\|docs/"` across the three Sheaf artifacts returns nothing; `scripts/` holds `check_app_bundle_plist.sh`, `check_ui_boundary.sh`, `check_ui_boundary_empty_discovery.sh`. |
| S-14 | D10 | The frogg3rs Impact delegates all of `External/Sheaf` to the Sheaf change; the Sheaf change claims six directories. The gap includes `projects/synth/Makefile` (the build entry point task 1.5 baselines), `apps/`, `browser/`, `docs/`, `juce/`, `scripts/` and the Sheaf root. An executor performing "the §8.0 sweep the Impact names" sweeps a fraction and reports it complete. | blocks execution of 1.5/1.6 as written | **verified** by reading both Impact sentences. |
| S-15 | E-ADV7, F-ADV8 | Four of sru-63's six scenarios are green against a `MidiSlotMismatchReport` that is declared, carried and never populated; the branch passes most comfortably when the controller is deadest. The Engine→view-model seam that would carry the observed-address set to the row is named in no artifact. F adds that "the mapped set" is not well defined, because `FindAssociation` resolves Launchpad positions by note number ignoring channel and returns the first match. | blocks execution of group 4 as specified | not adjudicated — verify at the point of repair |
| S-16 | E-ADV8 | Task 1.2 instructs "confirm the count is three, not two" and then supplies the three — a check that carries its own answer cannot fire. sru-63's "the enumeration is unchanged" scenario asserts a property of source text that a C++ unit test cannot observe. | blocks neither | not adjudicated — verify at the point of repair |
| S-17 | E-ADV10, F-ADV6 | Three of scw-6's and sru-64's five checks are green on the field declaration alone, and all three are absence assertions. Task 7.3's "break one check to prove it goes red" should name the one that can. | blocks neither | not adjudicated — verify at the point of repair |
| S-18 | E-ADV12 | The new ADDED requirement's fourth SHALL — "A preset SHALL NOT depend on a device setting it does not declare", the clause that would have caught the original incident — has no scenario and is not mechanically detectable. | blocks neither | **verified** by reading the requirement and its three scenarios. |
| S-19 | E-ADV13 | 25 of the 35 `Check:` lines across the two deltas read "not yet delivered". Nothing converts them when the delivering tasks run, and tasks 6.4 and 7.3 are worded as disjunctions both halves of which the current text satisfies, so a completed change and a skipped one are indistinguishable to every mechanism in the repo. | blocks delivery | **verified** in part: `check-spec-checks-resolve` reports "12 declared as having no automated check" and its `NO_CHECK` regex short-circuits the line. |
| S-20 | F-ADV5 | `HeldModifierCeilingIsWallClockNotPumpCount` is assigned to `instrument_tests.cpp`, which contains no reference to `Engine` or `MessageThreadTick`; the scenario's "once per second vs thirty times per second" clause is untestable in a file with no pump. | blocks execution | not adjudicated — verify at the point of repair |
| S-21 | F-ADV9, F-ADV16, C12 | Gate-scope notes: `check_modified_requirements_restate_promoted.py` downgrades a wholly deleted scenario to a NOTE and exits 0; `openspec validate` accepts MODIFIED blocks whose target requirement does not exist in any promoted spec; no gate in either repository reads Sheaf's `Check:` lines. None is this change's defect; each bounds what the green means. | NOTE | **verified**: both scripts run, exit 0; `openspec validate midi-controller-resilience --strict` → valid, while `smi-14`/`smi-16`/`smi-17` appear in no Sheaf promoted spec. |
| S-22 | B15 | Sheaf task 1.5's gate names are not runnable as literals — `projects/synth test` is not a command, and "the miniapp target built explicitly" names no target. | NOTE | not adjudicated — verify at the point of repair |
| S-23 | B16 | Sheaf task 1.4's behavioural premise ("Hold Shift, print the flag, rebuild, print it again") names no test file, no binary, no harness, and no press-message bytes. The task is right to demand the run; it does not say where the run lives. | blocks execution if read strictly | **verified** by reading task 1.4 in full. |
| S-24 | D12 | No CI workflow runs `make -C app test`; every frogg3rs gate, including the one this change adds, is local-only. | NOTE | not adjudicated — verify at the point of repair |
| S-25 | D4 | Task 3.2's `QUICK_DICT.md` half is dead instruction: that document has no MIDI content for this change to edit. | NOTE | not adjudicated — verify at the point of repair |
| S-26 | A9, F-ADV14 | Task 7.6 would open a pull request carrying #13's, #14's and #15's commits, since this branch is their linearisation. No artifact addresses what that PR contains. | blocks delivery | **verified** via `git log --oneline` (see ADJ-11). |

**Also worth recording, because the trace answered it and the reports are right about it.** Nine of
the ten `app/` gates are green; both changes pass `openspec validate --strict`; every catalogue,
`MidiController`, `Engine`, `MidiReconcile`, `ControllersPageUI`, `MidiAppCatalog`, `midi.ts` and
`MANUAL.md` line citation I opened resolved to what it claims; the smi-14 and smi-16 amendments
drop no promoted clause; sru-63, sru-64, scw-6 and smi-17 are free numbers; and the
`frogg3rs-delay-width-wysiwyg-repair` row's 0/26 is exact.

```
$ cd $W && for s in check_spec_checks_resolve check_citations_resolve check_modified_requirements_restate_promoted check_no_planning_history check_docs_match_parameter_table; do python3 app/$s.py app >/dev/null 2>&1; echo "$s rc=$?"; done
check_spec_checks_resolve rc=0
check_citations_resolve rc=0
check_modified_requirements_restate_promoted rc=0
check_no_planning_history rc=0
check_docs_match_parameter_table rc=0
$ bash app/check_catalog_covers_screen_actions.sh app kSceneBlend kEncoderPress kEncoderDrag kInputSelect kViewportNarrow ; echo "rc=$?"
check-catalog-covers-screen-actions: OK - every FroggersActions constant (minus the 5 excluded) is in the MIDI catalog and routed by HandleAction
rc=0
$ openspec validate frogg3rs-midi-preset-preconditions --strict ; openspec validate midi-controller-resilience --strict   # (the latter from External/Sheaf)
Change 'frogg3rs-midi-preset-preconditions' is valid
Change 'midi-controller-resilience' is valid
```

---

# 3. OPEN items

**OPEN-1 — Where the Sheaf branch is delivered from.** Sources: D11, F-ADV14, C (concurring).
`app/README.md:6` says "Development tracks a fork of it rather than upstream", `.gitmodules` points
at upstream, the submodule checkout's only remote is upstream, and the branch has no upstream.
Task 7.6 says "push this branch to the fork", and frogg3rs task 6.6 requires Sheaf's change to have
"landed" without saying whether that means merged upstream or only the submodule pin advanced to a
fork branch.

```
$ cat $W/.gitmodules
[submodule "External/Sheaf"]
	path = External/Sheaf
	url = https://github.com/jvictor0/Sheaf.git
$ cd $W/External/Sheaf && git remote -v
origin	https://github.com/jvictor0/Sheaf.git (fetch/push)
$ git rev-parse --abbrev-ref --symbolic-full-name '@{u}'
fatal: no upstream configured for branch 'midi-resilience-merge'
$ grep -n -i "fork" $W/app/README.md
6:at `External/Sheaf`. Development tracks a fork of it rather than upstream.
```
**What would settle it:** the operator naming the fork remote and the push route, or a `git remote -v`
in whatever checkout PRs #13/#14/#15 were raised from. **Needs the operator** — nothing in either
tree answers it, and both answers imply different repairs (add the remote and write the route into
7.6 and 6.6, or correct `app/README.md`).

**OPEN-2 — Whether delivery is permitted with group 5 undone.** Source: B8 (ADJ-17). The hardware
fact is settled — no Novation device is attached — but whether task 6.6 may push to `main` a change
whose Launch Control XL preset does not exist is a decision the artifacts never make and that
reading cannot decide. **Needs the operator.**

Two items other reports left open are **closed** here and need no operator:
- B's OPEN on the attribution of the five dangling citations — settled by the collecting commit's
  own body (ADJ-20).
- E's OPEN on whether `FroggersControllersPageTests.cpp` has an existing path-level scene-blend
  assertion for task 5.4 to extend — it has none: `grep -n "sceneBlend\|crossfader" $W/app/FroggersControllersPageTests.cpp`
  returns nothing.

---

# 4. Verdict

**REJECT** for the pair.

Twenty-one confirmed blocking findings. Seven are places where the code the artifacts describe is
not the code in the tree, and the difference changes what the executor writes: the pump loop null
(ADJ-01), the zero-returning clock (ADJ-02), the thread boundary (ADJ-03), the JUCE compile
barrier (ADJ-06), the render seam with no field (ADJ-10), the missing `declaredPreconditions`
field (ADJ-12), and the APC40 Ableton precondition the catalogue and the manual both deny
(ADJ-13). Five are blanks an assertion is written against — the rate constant (ADJ-04), the state
defaults (ADJ-05), the LCXL check (ADJ-14), the Twister-only check task 5.3 forbids (ADJ-15), and
the manual drift check that is green on the sentence it exists to reject (ADJ-16). Two are checks
green against the untouched tree (ADJ-09) or against no gate at all (ADJ-07, ADJ-08). Three are
tasks that cannot be run in these trees (ADJ-11, ADJ-17) or in the order given (ADJ-12). Four are
tree-state and baseline defects (ADJ-18, ADJ-19, ADJ-20, ADJ-21).

The trace elsewhere is strong and that belongs on the record: every line citation I opened
resolved, both changes validate strictly, nine of ten `app/` gates are green, the requirement
numbers are free, the smi-14/smi-16 amendments drop nothing, and the pair's handling of the
unreachable hardware asserts no CC number anywhere.

---

# 5. Coverage note

This is a reading on how much of the space was looked at, not on truth. Independence is taken from
the exchange files' own statements of who reached what before seeing the others.

**Reached by more than one report independently (15):**
ADJ-01 (A, E) · ADJ-02 (E, A, F) · ADJ-04 (B, C, E) · ADJ-05 (B, F) · ADJ-06 (B, C) ·
ADJ-07 (E, F) · ADJ-09 (E, F) · ADJ-10 (B, C, E, F) · ADJ-11 (A, B, E, F) · ADJ-13 (A, C) ·
ADJ-14 (A, C, E, F) · ADJ-15 (C, E, F) · ADJ-18 (A, B, C, D) · ADJ-20 (A, B, C, D, E, F) ·
ADJ-21 (A, B, C, F).

**Reached by exactly one report (6):**
- **ADJ-03** — cross-thread writes to the modifier flags — F only. Every other report read the same
  two structs and the same two trigger sites without reaching it.
- **ADJ-08** — the 24-site ABI version sweep outside the declared directories — B only.
- **ADJ-12** — frogg3rs group 4's compile order and the missing submodule-pin task — B only.
- **ADJ-16** — the manual drift check green on `MANUAL.md:320` — E only (F reached a weaker form of
  the same target).
- **ADJ-17** — the hardware block's delivery consequence and the unnamed MIDI monitor — B only
  (the hardware fact itself was reached by A, B, C, D, E, F; the delivery ruling and the monitor
  were not).
- **ADJ-19** — the fourth worktree, the divergent duplicate of this change, and the third
  `MANUAL.md` holder — D only. Four reports state in their own "what I did not cover" sections that
  they never left this worktree.

The single-source findings cluster in two places the other reports did not reach: concurrency, and
state outside the audited worktree. Both are directions to widen if another round is run.
