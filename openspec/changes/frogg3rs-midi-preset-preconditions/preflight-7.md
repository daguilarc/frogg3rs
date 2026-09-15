# Preflight 7 — adjudication

Pair: frogg3rs `openspec/changes/frogg3rs-midi-preset-preconditions/` and Sheaf
`openspec/changes/midi-controller-resilience/`.

Adjudicated at frogg3rs `7b2dd5de855e28a76131f4f2ed6fae1384c316f5` (worktree
`midi-resilience`, branch `worktree-midi-resilience`, `git status --short` empty) and
Sheaf `1e2f0f562b4f77cb695d568e093165dc7c581073` (branch `midi-resilience-merge`,
`git status --short` empty). Nothing was edited, committed, stashed, built or installed;
nothing under `/Applications` was opened; no `preflight*.md` inside either change
directory was read.

Every ruling below was reached by running the command shown and reading its output in
this context. Agreement between the six reports was not treated as evidence: each claim
was re-derived. Where a report's line number differed from mine by a line or two the
construct is the same and I cite mine.

**Verdict: REJECT.** Seven confirmed blocking findings; one blocking framing refuted;
nothing left open among the blocking set.

---

## Baseline: what is green today

All five non-compiling frogg3rs python gates pass, and both changes validate strictly.
There is no pre-existing red to attribute to anyone.

```
$ python3 app/check_spec_checks_resolve.py app
check-spec-checks-resolve: OK - 68 Check reference(s) resolved, 17 declared as having no automated check   [exit=0]
$ python3 app/check_citations_resolve.py app
check-citations-resolve: OK - 95 commit-pinned, 277 into pinned or frozen trees, 0 unresolvable, 0 line-numbered into this tree, 0 split across two lines   [exit=0]
$ python3 app/check_artifact_symbols_resolve.py app
check-artifact-symbols-resolve: OK - 6 artifact file(s) resolve, 1 name(s) declared as not yet created   [exit=0]
$ python3 app/check_modified_requirements_restate_promoted.py app
check-modified-requirements-restate-promoted: NOTE - .../froggers-midi-controller-mappings/spec.md: 'The MIDI Fighter Twister preset maps five buttons with shifted jobs and one Shift' no longer restates promoted scenario 'A missing release leaves Shift held until the next press and release' and its 4 clause(s); no delta scenario carries more than half of them
check-modified-requirements-restate-promoted: OK - 5 MODIFIED requirement(s), 134 promoted clause(s) restated or declared, 9 declared edit(s), 1 promoted scenario(s) no longer restated   [exit=0]
$ python3 app/check_no_planning_history.py app
check-no-planning-history: OK - 59 files, no planning-history references   [exit=0]
$ npx openspec validate frogg3rs-midi-preset-preconditions --strict
Change 'frogg3rs-midi-preset-preconditions' is valid   [exit=0]
$ cd External/Sheaf && npx openspec validate midi-controller-resilience --strict
Change 'midi-controller-resilience' is valid   [exit=0]
```

That green baseline is itself part of the problem: five of the seven blocking findings
are things no gate in either repository reads.

---

# 1. Confirmed blocking findings

Work down this list. Each is stated as a condition on the artifacts.

---

## BL-1 — CONFIRMED. The only cross-repository gate names the wrong Sheaf group, so the manual can ship documenting three recoveries no shipped code performs

**Sources:** A1, F-ADV1 (reached independently). Confirmed in the exchange by B, C, D, E, F.
**Blocks:** execution of frogg3rs group 3, and delivery.

**What breaks.** `MANUAL.md`'s Shift and Hold Drill subsections are rewritten by frogg3rs
task 3.1 to offer Rebuild ("selecting a different preset"), EndpointOpen ("unplugging and
reconnecting the controller") and Ceiling ("clears automatically after 30 seconds") as
available recoveries. Those three triggers are created by Sheaf **group 3**. The only gate
in the frogg3rs change greps for `declaredPreconditions`, which is Sheaf **group 6**. Task
3.1 carries no gate of any kind, and design.md's Migration Plan positively licenses running
it early. The shipped bundle carries the file. This is the defect the pair exists to repair,
reintroduced by the repair — and task 4.8's recovery half cannot see it, because it matches
phrases in prose.

**Evidence.**

```
$ sed -n '189,198p' openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md
- [ ] 4.1 **Gate.** Do not start the rest of this group until
      `External/Sheaf`'s submodule checkout is at a commit, reachable from
      Sheaf's `fork/midi-resilience-merge` (its own task 7.7 delivers this
      branch there and nothing else), that has executed Sheaf's tasks 6.1 (adds
      `declaredPreconditions` to `MidiAppDeviceDefault`,
      `include/synth/MidiAppCatalog.hpp:31-38`) and 6.2 (threads it through
      `ControllerWizardDescriptor` and `MakeControllerWizardRegistry`). Confirm
      with `grep -n declaredPreconditions
      External/Sheaf/projects/synth/include/synth/MidiAppCatalog.hpp` — it
      must return a real member; today it returns nothing.

$ grep -n "^## " External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md
1:## 1. Preflight
107:## 2. Branch sequencing
143:## 3. Held modifier lifetime      <- creates HeldModifierClearSource (:146-148)
344:## 4. Mismatch report
386:## 5. Inbound template changes
427:## 6. Declared preconditions seam <- creates declaredPreconditions (6.1 at :429)
463:## 7. Postflight

$ grep -rn "HeldModifierClearSource\|HeldModifierState\|kHeldModifierCeilingMicros" app External/Sheaf/projects | wc -l
       0

$ awk 'NR>=455&&NR<=462' openspec/changes/frogg3rs-midi-preset-preconditions/design.md
## Migration Plan
...
pin lands the field (tasks 4.1-4.2); until then this change's only executable
work is the manual rewrite (group 3) and the two device-generic checks that do
not depend on the field ...

$ grep -rn "MANUAL.md" app/build-launcher.sh
app/build-launcher.sh:72:cp "$REPO_ROOT/MANUAL.md" "$APP_BUNDLE_DIR/Contents/Resources/MANUAL.md"
```

Task 3.1 (`tasks.md:138-164`) was read whole: it carries no gate clause. Task 6.6
(`tasks.md:412`) ships groups 1, 3, 4 and 5 once group 4's gate is satisfied.

**Condition on the artifacts.**
1. Task 4.1's confirmation must additionally grep for `HeldModifierClearSource` in
   `External/Sheaf/projects/synth/include/synth/MidiController.hpp`, requiring a real
   enumeration — not only `declaredPreconditions` in `MidiAppCatalog.hpp`.
2. Task 3.1 must carry that same gate, or a gate at the head of group 3 requiring it.
3. design.md's Migration Plan sentence naming group 3 as the work available before the
   pin lands must be deleted, and task 6.6 must state that group 3 ships only under the
   same gate.

---

## BL-2 — CONFIRMED. `app/check_docs_match_device_preconditions.py` cannot be written as specified: its floor, its count source, its parse target, both its positive controls, and its comparison unit are each unimplementable or inert, and the spec delta states a different rule again

**Sources:** B2, C3 (upgraded to blocking in C's exchange), E-ADV1, E-ADV2, E-ADV3,
E-ADV4, F-ADV2, F-ADV3, F-ADV4, F-ADV5, F-ADV7, A3.
**Blocks:** execution (task 4.8).

This is one finding with eleven verified sub-parts. They share one remedy — design.md's
"The drift check has two independent halves" section must be rewritten before task 4.8
runs — so they are consolidated. Task 4.8 tells the executor to implement "exactly the
rule, floor and positive controls design.md states … nothing here restates it", which is
what makes each sub-part an execution stop rather than a note.

**The rule as it stands** (`design.md:175-214`, read in full):

```
175 currently seven: Twister, APC40 Generic, APC40 Ableton, Launch Control XL,
...
187 `---`. "Agree" means: every string in `declaredPreconditions` names a setting
188 (the substring before the first comma, e.g. "encoders set to relative") that
189 appears, case-insensitively, somewhere in that device's subsection prose; and
190 the subsection names no device-setting-shaped clause (a sentence containing
191 "must", "set to", "unchecked", "stay selected", or "Utility" outside the
192 device's own name) that does not correspond to a string in
193 `declaredPreconditions`.
197 **Drift half — floor.** The check must locate exactly seven device
198 subsections and seven `declaredPreconditions` lists before comparing
199 anything; if either parse finds fewer than `catalog.deviceDefaults.size()`
200 (read the same way `app/FroggersMidiCatalogTests.cpp`'s own count assertion
201 does, not hard-coded), the check fails outright with that count ...
205 **Drift half — positive controls, both directions.** (a) ... temporarily append
206 a word to the Twister's `declaredPreconditions`
207 entry for CC Hold (e.g. `"CC Hold, permanently"` in place of `"CC Hold"`) ...
209 (b) Manual-edit direction: temporarily edit `MANUAL.md`'s Twister subsection
210 to say "all six side buttons set to CC Toggle" in place of "CC Hold" ...
```

**(a) "currently seven" is wrong; the catalogue holds six.** [C3]

```
$ awk 'NR>=329&&NR<=336' app/FroggersMidiCatalog.hpp
    catalog.deviceDefaults = {
        TwisterDeviceDefault(), Apc40GenericDeviceDefault(), Apc40AbletonDeviceDefault(),
        LaunchpadXDeviceDefault(), LaunchpadProMk3DeviceDefault(), LaunchpadMiniMk3DeviceDefault(),
    };
```

**(b) "exactly seven" and `deviceDefaults.size()` are two different gates, and 4.8
precedes 5.1.** [B2] Task 4.8 wires the script into `app/Makefile` (`tasks.md:300-302`);
task 5.1 appends the seventh default and is in the next group (`tasks.md:312`). Between
them `make -C app test` runs a check whose floor expects seven against a catalogue of six.

**(c) The named count source is the hard-coded literal the same sentence forbids.** [B2,
F-ADV3, C, D, E]

```
$ grep -rn "deviceDefaults.size()\|registry.size() == 6" app/*.cpp
app/FroggersControllersPageTests.cpp:99:    REQUIRE_TRUE(catalog.deviceDefaults.size() == 6);
app/FroggersControllersPageTests.cpp:109:    REQUIRE_TRUE(registry.size() == 6);
app/FroggersMidiCatalogTests.cpp:399:    REQUIRE_TRUE(catalog.deviceDefaults.size() == 6);
```

**(d) The prescribed by-function-name parse can reach at most four of seven lists: the
three Launchpad factories have no bodies.** [F-ADV3]

```
$ grep -n "^inline synth::MidiAppDeviceDefault" app/FroggersMidiCatalog.hpp
90:  TwisterDeviceDefault()        171: Apc40GenericDeviceDefault()
182: Apc40AbletonDeviceDefault()   236: LaunchpadDeviceDefault(controller, id, displayName,
                                       inputAliases, outputAliases, programmerModeSysEx)
265: LaunchpadXDeviceDefault()     274: LaunchpadProMk3DeviceDefault()   284: LaunchpadMiniMk3DeviceDefault()

$ awk 'NR>=265&&NR<=272' app/FroggersMidiCatalog.hpp
inline synth::MidiAppDeviceDefault LaunchpadXDeviceDefault() {
    return LaunchpadDeviceDefault(synth::LaunchpadController::LaunchpadX, "froggers.launchpad.x", "Launchpad X", ... );
}
```

`:274` and `:284` have the identical shape. The shared helper's parameter list carries no
preconditions argument, and one body serves three devices, so the per-device
correspondence the rule requires is unavailable for three of seven by the code's
structure, not by the parse's choice of target.

**(e) A related parse hazard the design does not name:** `Apc40AbletonDeviceDefault()`
copy-constructs from the Generic, so task 4.4's "the Ableton default declares an **empty
list**" needs an explicit clear that the by-function-name parse of its own body cannot see.

```
$ awk 'NR>=182&&NR<=188' app/FroggersMidiCatalog.hpp
inline synth::MidiAppDeviceDefault Apc40AbletonDeviceDefault() {
    synth::MidiAppDeviceDefault device = Apc40GenericDeviceDefault();
    device.id = "froggers.apc40.ableton"; ... return device; }
```

**(f) Positive control (a) is inert.** The rule compares the substring before the first
comma. `"CC Hold"` and `"CC Hold, permanently"` share the pre-comma substring `CC Hold`
byte for byte, under either shape the entry can take. The control's own justification
("`MANUAL.md`'s Twister subsection no longer contains that exact setting text") asserts a
whole-string comparison the rule does not perform — and control (a) edits the
*declaration*, leaving the manual untouched. [E-ADV1(a), F-ADV2]

**(g) Positive control (b) is inert.** The rule searches "somewhere in that device's
subsection prose". The Twister subsection is `MANUAL.md:322-340` and carries "CC Hold"
twice; control (b) names only the first. [E-ADV1(b)]

```
$ awk 'NR>=258&&NR<=395 && /^### /' MANUAL.md | ...   (heading scan)
322:### MIDI Fighter Twister        341:### Akai APC40 mkII (Generic)
$ grep -n "CC Hold" MANUAL.md
337:"Enc 3FH/41H" (relative), all six side buttons to "CC Hold", and "Bank Side Buttons" unchecked, so the
339:shows. CC Hold is also what lets the app see the Shift button's own release.
```

So the change's flagship gate has **no demonstrated red state in either direction**.

**(h) The comparison never reaches the value or the where-it-is-set.** [E-ADV2(i),
F-ADV5] The ADDED requirement demands three parts per string; the rule compares one.

```
$ sed -n '50p' openspec/changes/frogg3rs-midi-preset-preconditions/specs/froggers-midi-controller-mappings/spec.md
A device default SHALL populate `declaredPreconditions` ... with its device-side preconditions,
each string naming the setting, the value the preset requires, and where that value is set. ...
```

Where the author places the comma is unconstrained and the same author writes both sides,
so a declaration spelled `"Bank Side Buttons", unchecked, …` is green against a manual
saying "Bank Side Buttons **checked**" — verbatim the operator incident proposal.md opens
with.

**(i) The comparison is generated-against-its-own-source, a shape design.md rejects one
page later for a different SHALL and never turns on the drift check.** [E-ADV2(ii),
F-ADV6] Task 4.7 (`tasks.md:261`) generates the manual paragraph from the declarations;
task 4.8 compares that paragraph back to them. `design.md:255-257`: "both sides are
written from the same source, so a dependency absent from both passes trivially." Post
generation the only drift left is a later hand-edit — which (g) shows the check misses.

**(j) The reverse direction's five-marker set is inert on six of the seven devices.**
[A3, E-ADV3, F-ADV4] Measured against the live manual:

```
$ for each device subsection: grep -ciE "must|set to|unchecked|stay selected|Utility"
Twister        lines 322-340 markers=2
APC40Generic   lines 341-352 markers=0
APC40Ableton   lines 353-358 markers=0
LaunchpadX     lines 359-370 markers=0
LPProMK3       lines 371-377 markers=0
LPMiniMK3      lines 378-383 markers=0
```

The APC40 Generic's real, live device-side precondition is phrased "Keep Track 1
selected:" (`MANUAL.md:348-350`) and matches none of the five markers, so deleting its
declaration leaves the manual asserting an undeclared precondition and the check silent.

**(k) The spec delta states a *different* recovery-half rule from design.md.** [F-ADV7]

```
$ sed -n '45p' openspec/changes/.../specs/froggers-midi-controller-mappings/spec.md
- Check: not yet delivered; task 4.8 adds a check script that fails when the manual's
  recovery text names no mechanism other than "pressed and released again"
```

design.md:216-230 instead requires one of a fixed multi-word phrase set. Under the spec's
wording "restart the app" passes; under design's it does not. The check's assertion is not
settled before it is written.

**(l) The mandated early run has a written expectation for one half only.** [B2]
`tasks.md:289-297` states the expected failure for the recovery half against both
subsections and states nothing for the drift half; at that moment `declaredPreconditions`
exists nowhere (`grep -n declaredPreconditions External/Sheaf/projects/synth/include/synth/MidiAppCatalog.hpp`
→ no output, exit 1), so the drift half necessarily fails its own floor with no recorded
expectation. That is a blank in a check.

**Condition on the artifacts.** Rewrite design.md's drift-check section so that, before
task 4.8 runs, all of the following are written down and not left to the executor:
1. The expected device count has exactly one source — parse `catalog.deviceDefaults`'s
   initializer list in `app/FroggersMidiCatalog.hpp`. Delete "exactly seven" and delete
   the instruction to imitate `FroggersMidiCatalogTests.cpp`'s assertion. Correct
   "currently seven" to "currently six; seven once task 5.1 appends the Launch Control XL".
2. How a device default that declares nothing is parsed — the three Launchpads share one
   helper body and the Ableton default copy-constructs from the Generic. State whether the
   floor counts lists, strings, or both, and how "declares none" is told from "was never
   asked" through those two structures.
3. The comparison unit: the whole declared string, exact, against the generated paragraph
   — not a pre-comma prefix.
4. Positive controls that flip a value the rule actually reads (CC Hold → CC Toggle in the
   declaration; unchecked → checked in the manual), and a manual-side search confined to
   the generated settings paragraph so a second occurrence elsewhere in the subsection
   cannot keep it green.
5. The reverse direction restated so it does not depend on guessing English — fence the
   generated paragraph and assert that no *other* sentence in the device's subsection
   names a device-side setting — or, at minimum, add the imperative forms the manual uses
   today (`keep`, `leave`, `off`, `template`) and state in design.md that it is a marker
   heuristic.
6. One statement of the recovery-half rule. Reword the spec delta's Check line at `:45` to
   design.md's phrase set, or design.md to the spec's rule. One of them, not both.
7. The expected output of 4.8's mandated early run, for **both** halves separately.

---

## BL-3 — CONFIRMED. Five Sheaf delta scenarios name a test no task delivers, in the repository that has no gate reading a `Check:` line

**Source:** C1 (one report). Confirmed in the exchange by A, B, D, E, F.
**Blocks:** delivery.

**What breaks.** scw-6 and sru-63 promote into `openspec/specs/` on archive carrying five
false claims that a named test exists. Sheaf has nothing of `check_spec_checks_resolve.py`'s
class, and `openspec validate --strict` checks neither test existence nor clause
restatement, so task 7.3 — a hand step — is the only thing between the defect and the
promoted spec.

**Evidence.** Per-name counts over the change's own `tasks.md` and `specs/`:

```
AppDeviceDefaultWithNoPreconditionsCarriesAnEmptyList             tasks=0 spec=1
LibraryTwisterDescriptorCarriesNoDeclaredPreconditions            tasks=0 spec=1
UnmappedInboundAddressesAreNamedInTheMismatchReport               tasks=0 spec=1
SilentMappedAddressesAreNamedOnceTheControllerHasTransmitted      tasks=0 spec=1
SilentSetIsWithheldUntilTheControllerShowsLife                    tasks=0 spec=1
AppDeviceDefaultCarriesItsDeclaredPreconditionsOntoItsDescriptor  tasks=2 spec=1   <- contrast
AMatchingControllerCarriesNoMismatchReport                        tasks=1 spec=1   <- contrast
ResyncDoesNotClearAHeldModifier                                   tasks=1 spec=0   <- see SF-1

$ ls projects/synth/scripts/
check_app_bundle_plist.sh  check_ui_boundary.sh  check_ui_boundary_empty_discovery.sh
$ grep -rn "spec_checks\|citations_resolve\|modified_requirements" projects/synth/Makefile projects/synth/apps/miniapp/Makefile Makefile
(no output; exit 1)
$ npx openspec validate midi-controller-resilience --strict
Change 'midi-controller-resilience' is valid   [exit=0]
```

The contrast rows score non-zero, so the zero is the property claimed and not an artefact
of the grep.

**Condition on the artifacts.** Extend task 6.4 to name
`AppDeviceDefaultWithNoPreconditionsCarriesAnEmptyList` and
`LibraryTwisterDescriptorCarriesNoDeclaredPreconditions`; add a task in group 4 naming the
three sru-63 mismatch-report cases with the assertion for each already written down (task
4.2 already states the withholding rule the third one checks). Separately, and the only
thing that stops recurrence: port `check_spec_checks_resolve.py` into
`projects/synth/scripts/` and wire it into `projects/synth test`, or state the omission in
design.md as an accepted gap.

---

## BL-4 — CONFIRMED. smi-14's final clause is contradicted by the design that implements it, and the scenario holding it up is green under both readings

**Source:** E-ADV6 (one report). Confirmed in the exchange by A, B, C, D, F.
**Blocks:** execution (the delta and the design cannot both be satisfied).

**Evidence.**

```
$ sed -n '5,6p' External/Sheaf/openspec/changes/midi-controller-resilience/specs/synth-midi-instrument/spec.md
### Requirement: smi-14 — Hold Drill: momentary drill-in gate on a held button
... THE synth system SHALL additionally clear held by every trigger smi-16 defines, and SHALL
clear every drilled flag whenever held is cleared by any trigger.

$ awk 'NR>=292&&NR<=295' External/Sheaf/openspec/changes/midi-controller-resilience/design.md
The vector's actual reset already happens unconditionally on every new Hold Drill press
(`holdDrill_->modifier.held = true; holdDrill_->drilled.clear();`, `MidiController.cpp:956-958`),
regardless of what triggered the prior clear,
so a `Message`-thread trigger does not need to touch `drilled` at all.

$ sed -n '30,34p' .../specs/synth-midi-instrument/spec.md
#### Scenario: A drill whose release never arrives does not cost every knob
- **THEN** held is cleared, every drilled flag is cleared with it, and the next turn on
  every mapping applies ordinarily
- Check: not yet delivered; added by this change as `instrument_tests.cpp: HoldDrillClearsOnEveryModifierTrigger`

$ sed -n '709,712p' External/Sheaf/projects/synth/src/MidiController.cpp
void EncoderMidiInProcessor::Process(const BasicMidi& midi) {
    if (midi.IsCC()) {
        if (const EncoderMidiMapping* mapping = FindTurn(midi)) {
            if (holdDrill_ != nullptr && holdDrill_->held) {
```

The drilled vector is read only under the `held` gate, so the scenario's first and third
clauses are produced by clearing `held` alone. An implementation that follows design.md —
Ceiling and EndpointOpen never touching `drilled` — passes the scenario while the
requirement's middle clause is false.

**Condition on the artifacts.** Either clear `drilled` on every trigger and have the case
assert the vector rather than the next turn's behaviour, or amend smi-14 to say the drilled
flags are gated by `held` and reset on the next press, deleting the clause the design
declines to implement. One of the two, decided in the artifacts, not by the executor.

---

## BL-5 — CONFIRMED. Sheaf task 3.5 orders a call that cannot be made from where it says to make it; the browser endpoint-open seam is named nowhere and covered by nothing

**Source:** E-ADV8 (F-ADV9 is an adjacent, weaker form — it asserts no check joins the
TypeScript name to the C++ name, and mis-locates the seam). Confirmed in the exchange by
A, B, C, D, F (C withdrew its own contrary assessment).
**Blocks:** execution (task 3.5).

**What breaks.** EndpointOpen is one of the five triggers; its browser half is the one the
frogg3rs manual will offer as "unplugging and reconnecting the controller". Task 3.5 says
to "Call it from `midi.ts`'s `applyAction`" and to "Route the new ABI call through
`BrowserRuntime` to `BrowserMidiBridge`", and describes a node case asserting "the facade
fires when `applyAction` reaches `this.inputs.set(...)`". `applyAction` cannot reach the
facade. Its only handle is a four-method command/response interface.

**Evidence.**

```
$ sed -n '1,12p' External/Sheaf/projects/synth/browser/src/midi.ts
import type { RuntimeCommand, RuntimeResponse } from "./worker.js";
export interface BrowserMidiRuntime {
  submitEndpoints(endpoints: MidiEndpoint[]): Promise<MidiAction[]>;
  deliverMidi(controllerIx: number, bytes: number[], timestampMicros: number): Promise<void>;
  dequeueMidiOutput(): Promise<MidiOutput | undefined>;
  midiDiagnostics?(): Promise<MidiOutputDiagnostics>;
}
export class BrowserMidiWorkerRuntime implements BrowserMidiRuntime {
  constructor(private readonly request: (command: RuntimeCommand) => Promise<RuntimeResponse>) {}

$ grep -n "midi-" External/Sheaf/projects/synth/browser/src/worker.ts | head -6
26:  | { type: "midi-endpoints"; endpoints: MidiEndpoint[] }
28:  | { type: "midi-input"; ... }
29:  | { type: "drain-midi-output" }
30:  | { type: "midi-diagnostics" }
668:        case "midi-endpoints": {

$ sed -n '261,273p' External/Sheaf/projects/synth/browser/src/worker.ts
export function emscriptenRuntimeFacade(module: EmscriptenModule): RuntimeModuleFacade {
  ... six `if (typeof module._synth_browser_* !== "function") throw` guards, none for a new export
```

The real path is `midi.ts` → a new `BrowserMidiRuntime` method → a new `RuntimeCommand`
variant → a `worker.ts` dispatch case → the facade → wasm. Task 3.5 (`tasks.md:197-269`,
read in full) names the C++ export, the `EXPORTED_FUNCTIONS` derivation, the `midi.ts`
call site and the facade, and none of the three pieces in between; its one mention of
`worker.ts` is `:261` as the test's import source. And the miss is silent in production:
`applyAction` is `void` and the file's established call shape is `void this.runtime.…`
(`midi.ts:238`), so a rejected command is swallowed.

**Condition on the artifacts.** Task 3.5 must name the `BrowserMidiRuntime` method, the
`RuntimeCommand` variant and the `worker.ts` dispatch case it creates; must add the new
export to `emscriptenRuntimeFacade`'s `typeof` guards; and must state that the
`midi-timing.test.mjs` case drives the real `BrowserMidiWorkerRuntime` request path rather
than a test-assembled shortcut. Fold in SF-8 (the derivation is red by one name on the
unmodified tree) in the same edit.

---

## BL-6 — CONFIRMED AS FACT; severity governed by operator ruling 2. The submodule pin this branch carries is reachable from no remote, and the URL `.gitmodules` names is not the one anything pushes to

**Sources:** B1, E-ADV13 (reached independently). Confirmed in the exchange by A, C, D, F.
**Blocks:** neither, under ruling 2 — the pin being unreachable until the operator's merge
is by design. The finding survives as a fact the artifacts must state and sequence around.

**Evidence.**

```
$ cat .gitmodules
[submodule "External/Sheaf"]
	path = External/Sheaf
	url = https://github.com/jvictor0/Sheaf.git
$ git -C External/Sheaf remote -v
fork	https://github.com/daguilarc/Sheaf.git (fetch/push)
origin	https://github.com/jvictor0/Sheaf.git (fetch/push)
$ git ls-tree HEAD External/Sheaf
160000 commit 1e2f0f562b4f77cb695d568e093165dc7c581073	External/Sheaf
$ git -C External/Sheaf branch -r --contains 1e2f0f562b4f77cb695d568e093165dc7c581073
(no output; exit 0)
$ git ls-tree main External/Sheaf
160000 commit ba3898e48f29121311b78a6be5aa460d805893cb	External/Sheaf
$ git -C External/Sheaf branch -r --contains ba3898e48f29121311b78a6be5aa460d805893cb
  fork/launchpad-model-on-the-row
```

So every pin `main` has carried is reachable from a remote ref and the one this branch
carries is not; `fork/midi-resilience-merge`, the branch task 4.1 names as the source of a
candidate commit, does not exist even as a remote-tracking ref. B's `ls-remote` evidence
that the historical pins are simultaneously upstream `refs/pull/1[345]/head` I did not
re-run (F could not either); the local ref data above is sufficient and is not open.

**Ruling.** Per operator ruling 2 this is CONFIRMED as a fact and is **not** a reason to
push anything, open a pull request, or change `.gitmodules`. Its remedy is entirely in the
artifacts.

**Condition on the artifacts.**
1. State plainly, in frogg3rs task 6.6 and in design.md's delivery section, that the
   submodule pin the pushed branch carries is reachable from no remote until the
   operator's later rebase-and-merge, and that a fresh clone or a CI checkout of that
   branch cannot resolve the gitlink until then. `.github/workflows/pages.yml` uses
   `actions/checkout@v4` with `submodules: recursive` (`:24,:26`) and triggers on push to
   `main`, so the merge is the point at which the pin must be reachable.
2. Sequence everything that needs the pin published after the operator's merge — frogg3rs
   task 5.4's live-site operator check explicitly (see SF-5).
3. Task 4.1's prose "reachable from `fork/midi-resilience-merge`" carries no command while
   every other confirmation in that file does; either give it one
   (`git -C External/Sheaf branch -r --contains <sha>`) or delete the reachability claim
   so the task does not assert something it never checks. A `grep` for a member name
   matches a comment or a local edit equally well.

---

## BL-7 — CONFIRMED. Task 4.8 must run before task 3.1 and is numbered a whole group after it

**Source:** B3 (one report; graded SHOULD-FIX there, no peer disputed).
**Blocks:** execution. I grade this above B's own rating, on what breaks: an executor
following the stated order destroys the control's subject before reaching the control.

**Evidence.**

```
$ sed -n '289,297p' openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md
      Run and record, before task 3.1 or 4.3-4.7 change anything: against the
      current, unmodified `MANUAL.md:319-320` ... the recovery half must fail; against the
      current `MANUAL.md:308-313` (no recovery sentence at all), it must also fail ...

$ sed -n '138,139p' openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md
- [ ] 3.1 Rewrite `MANUAL.md:319-320` (Shift) and add a matching recovery
      sentence to `MANUAL.md:308-313` ...
```

design.md:236-243 is explicit that the control is "running the check against the
**current, unmodified** `MANUAL.md`". Task 3.1 rewrites exactly those lines, carries no
forward instruction to run 4.8 first, and recovering the subject afterwards
(`git show HEAD:MANUAL.md`, revert, redo) is a step nobody wrote down.

**Condition on the artifacts.** Move the script's authoring and its two early runs into
group 1, or renumber it ahead of 3.1, so the list order and the stated constraint agree.

---

# 2. The one refutation, ruled

## DR-1 — frogg3rs task 4.6: citation defect CONFIRMED; blocking severity REFUTED

**Sources:** E-ADV12 (BLOCKING), F-ADV13 (SHOULD-FIX, execution), C5 (upgraded from NOTE
to BLOCKING in C's exchange), D (CONFIRM as blocking). **B refutes the blocking framing**
in its exchange.

**What both sides agree on, and I confirm.** design.md and tasks.md both spell a member
that does not exist at the pinned commit, inside a paragraph presented as a trace of the
current tree, and the same paragraph uses the pre-rename spelling three lines later:

```
$ awk 'NR>=116&&NR<=123' openspec/changes/frogg3rs-midi-preset-preconditions/design.md
That job is not always the button's *unshifted* one. `:964-971` handles a
press whose `association->press.type == MessageIn::Type::Shift` by setting
`shift_->modifier.held = isPress` and returning ...
... then reads `:975`'s `shifted = shift_ != nullptr && shift_->held && ...

$ sed -n '964,971p' External/Sheaf/projects/synth/src/MidiController.cpp
    if (association->press.type == MessageIn::Type::Shift) {
        ...
        if (shift_ != nullptr) {
            shift_->held = isPress;
        }
        return;
    }

$ sed -n '247,249p' openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md
      (b) When the button whose press-with-no-release
      is the Shift button itself, `shift_->modifier.held` is left `true`
      (`:969`) ...
```

`modifier` is Sheaf task 3.1's own composition change (`tasks.md:145-156`) and exists
nowhere today. Both statements are false about the tree. CONFIRMED.

**The disputed mechanism.** E/C/D/F hold that 4.6's cases therefore cannot be written
before the pin and stop compiling after it. B holds that the cases never name the field.
I ran B's check and it is correct:

```
$ awk 'NR>=387&&NR<=403' External/Sheaf/projects/synth/include/synth/MidiController.hpp
class SystemButtonMidiInProcessor final : public MidiInProcessor {
public:
    SystemButtonMidiInProcessor(SystemButtonMidiInConfig config, MessageInBus* bus = nullptr,
                                HoldDrillState* holdDrill = nullptr, ShiftState* shift = nullptr);
    ...
private:
    ...
    ShiftState* shift_ = nullptr;
};

$ grep -rn "ShiftState\|HoldDrillState\|->held\|\.held" app/
(no output; exit 1)
```

`shift_` is private, so `processor.shift_->…` compiles for nobody. Sheaf task 3.1 adds a
`HeldModifierState modifier;` member to `ShiftState` and `HoldDrillState` — the **type
names are unchanged** — so a test that owns its own `synth::ShiftState` and passes `&shift`
to the constructor compiles identically on both sides of the pin. And task 4.6's own
closing sentences direct the assertion away from the field: "this case asserts the shifted
dispatch, not an unshifted one", and "both cases construct the press-with-no-release input
directly". A case written as instructed never names `held` under either spelling.

**Ruling.** B's refutation is upheld. The `shift_->modifier.held` citation is a false
statement about the pinned tree in two artifacts — SHOULD-FIX — but it is not an execution
stop, and task 4.6's declared independence of the 4.1/4.2 gate is correct for the code it
actually orders. Both positions are recorded: four reports read it as blocking on a
compile-break that the private member and the surviving type name make impossible.

**Condition on the artifacts.** Rewrite `design.md:118` and `tasks.md:248` to the spelling
the pinned tree carries (`shift_->held`), or mark both explicitly as post-Sheaf-group-3
spellings; and add one sentence to 4.6 saying the case asserts the dispatched bus message
and never the held flag, so its gate-independence is a property of the instruction rather
than of the executor's taste. Fold in SF-9 (no positive control) in the same edit.

---

# 3. SHOULD-FIX, consolidated

"Verified" means I ran the command or opened the artifact in this context. "Not
adjudicated" means the claim is plausible on its face and cheap to settle at the point of
repair; I did not spend the round on it.

| id | Finding | Sources | Blocks | Status |
|---|---|---|---|---|
| SF-1 | Nine further Sheaf `Check:` lines name a test that appears in no task (the tasks describe each case but not its identifier), and `ResyncDoesNotClearAHeldModifier` is written by task 3.8 but cited by no scenario | C2 | delivery | **verified** — `ResyncDoesNotClearAHeldModifier` scores tasks=1 spec=0 in the cross-tab under BL-3; the nine names not re-tabulated individually |
| SF-2 | Both Sheaf MODIFIED requirements amend text that is not promoted (smi-14 lives only in `app-midi-catalog`'s delta, smi-16 only in `shift-and-file-export`'s), and no task constrains archive order | C4 | delivery | **verified** — promoted `synth-midi-instrument` ends at smi-12; `grep -n "smi-14\|smi-16\|smi-17" openspec/specs/synth-midi-instrument/spec.md` exit 1; both headings found only in the three change deltas |
| SF-3 | The recovery half is a presence check only: it cannot see whether a phrase is attached to the right host, cannot see a false sentence surviving beside the new one, and the manual's "30 seconds" is never related to Sheaf's `kHeldModifierCeilingMicros` | A2, E-ADV5, F-ADV6 | delivery of the manual claim | **verified** — `grep -rn kHeldModifierCeilingMicros app MANUAL.md` → no output; design.md:216-230 requires "at least one" phrase and forbids nothing |
| SF-4 | `kHeldModifierCeilingMicros` = 30 s and `kTemplateChangeRateLimitPerSecond` = 20 are justified in Sheaf design.md by unmeasured behavioural premises ("an operator mid-gesture is never cut off"; "no human retemplates faster than once a second") | A2 | delivery | partially verified — design.md's own text concedes "not a measured or documented figure"; the premises' falsity not independently measured. Remedy: state both as reversible defaults and strike the behavioural justification |
| SF-5 | frogg3rs task 5.4 is an operator step against a live site nothing in this delivery publishes (`pages.yml` triggers on push to `main`; task 6.6 pushes a branch) | B5 | delivery of the only check for one scenario | **verified** in effect by BL-6's ruling. Per ruling 2 the remedy is to sequence 5.4 after the operator's merge and say so in 5.4 and 6.6, naming which build the operator drives |
| SF-6 | CC 77-84 — the number written into shipping code — is carried in design.md as a *described* disassembly while the two less consequential integers carry literal bytecode | B6 | neither | **verified** — design.md:337-343 prints the bytecode for `LIVE_CHANNEL` and the template byte; `:350-352` says "and, from disassembling `make_slider` … with identifiers `77 + i` for `i in range(8)`", and `:353` concedes the bytecode gives the other two "directly". Consistent with operator ruling 1; remedy is to paste the second disassembly's literal output beside the first |
| SF-7 | `check-spec-checks-resolve` passes any Check line beginning "not yet delivered" with no backticked token, and task 6.4's completion criterion lets those lines survive after the tests exist — so the promoted spec can carry unresolvable Check lines for requirements that are in fact tested | E-ADV15, F-ADV16 | delivery | **verified** — `app/check_spec_checks_resolve.py:96`: `NO_CHECK = re.compile(r"^\s*(none\|operator step\|not yet delivered)", re.I)` |
| SF-8 | Sheaf task 3.5's `EXPORTED_FUNCTIONS` derivation is red by one name on the unmodified tree, so it will be resolved with a hand-maintained carve-out — the thing the task says it exists to avoid | F-ADV8 | execution of 3.5 | **verified** — reproduced: derived 30, exported 29, `DERIVED BUT NOT EXPORTED ['_synth_browser_create_runtime']`, `EXPORTED BUT NOT DERIVED []`. Remedy: state the derivation as every `extern "C"` function *definition* (declaration with a body) and record that `synth_browser_create_runtime` is a declaration |
| SF-9 | frogg3rs task 4.6 specifies no positive control, so both its cases are green on the unmodified tree with no demonstrated red state; and the loop case 4.5's control is "any non-Twister default", so it need never touch the Launch Control XL whose coverage task 5.3 claims from it | F-ADV13, F-ADV14, E-ADV20 | neither | **verified** from the task text (`tasks.md:228-260`): 4.5 says "giving any non-Twister default a shifted press"; 4.6 names no control. Remedy: specify 4.5's control on the Launch Control XL (the last-added entry) and give 4.6 one (temporarily gate the `isPress` dispatch on `association->release.has_value()`) |
| SF-10 | The EndpointOpen identity guard in `midi.ts` is exactly the recovery the manual will promise: a reconnect that leaves the same port object bound returns at the guard and clears nothing | E-ADV9 | delivery of the manual claim | **verified** — `midi.ts:234` `if (existing?.identifier === port.id && existing.port === port) return;`, four lines before the `inputs.set` where task 3.5 places the clear |
| SF-11 | Preflight gates that pass on their own failure: `git log --oneline HEAD..<bad ref> \| wc -l` prints `0` on stdout with the error on stderr, and the `ls … must return "No such file or directory"` gates are satisfied from the wrong directory | E-ADV14 | execution | **verified** — `git log --oneline HEAD..nosuchbranch \| wc -l` → `0`; `ls /nonexistent-parent/frogg3rs-delay-width-wysiwyg-repair` → "No such file or directory" |
| SF-12 | `check-artifact-symbols-resolve` reads only `tasks.md` and `proposal.md`, so `design.md` — which carries the whole Launch Control XL reading and every `External/Sheaf` citation — is scanned by nothing; `check-citations-resolve` walks `app/` sources only | E-ADV17, F-ADV15 | neither | **verified** — `app/check_artifact_symbols_resolve.py:111`: `ARTIFACTS = ("tasks.md", "proposal.md")` |
| SF-13 | The Impact's cited header range `app/FroggersControllersPageTests.cpp:1-4` excludes a fourth count statement at `:7` ("these six shipping defaults"), so task 5.5 as written leaves it saying six | B9, D1 | delivery | **verified** — `sed -n '1,8p'` shows the second count at line 7. Remedy: cite `:1-7` or "every count statement in that file's header" |
| SF-14 | `HoldDrillState`'s comment (`MidiController.hpp:244-248`, "Release clears held and drilled") states the premise this change inverts; the Impact's range begins at `:249`, design.md names only the `:254-255` half, and no task rewrites a comment anywhere | D2 | delivery | **verified** — Impact names `:249-258`; design.md:298 names `:254-255`; `grep -c "comment" External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md` → `0` |
| SF-15 | `README.md:80` and `:93` send a reader to `app/README.md` for the pinned Sheaf commit; that file records no commit, and this change moves the pin | D5 | delivery | **verified** — `grep -cE '[0-9a-f]{7,40}' app/README.md` → `0` |
| SF-16 | `MANUAL.md:276-281`'s controller-row description omits the Variant selector that is live in the pinned Sheaf, inside the MIDI section this change claims and rewrites in five places | D6 | delivery | **verified** — `grep -in "variant" MANUAL.md README.md QUICK_DICT.md` returns one hit, `README.md:77`, about Daisy firmware variants |
| SF-17 | `projects/synth/docs/coverage.md` — a file this change edits — carries nine dead links, and task 7.6 says smi-14/smi-16 each get a "(modified)" row when neither has a row to modify | D4 | delivery | not adjudicated — verify at the point of repair |
| SF-18 | Sheaf task 3.4's blacklisted-slot guard is a conjunction (`holdDrill == nullptr && shift == nullptr`) where two per-pointer checks are needed; a one-null profile falls through and both pointers are dereferenced | E-ADV7, F-ADV19 (F raised NOTE→SHOULD-FIX in its exchange) | neither | **verified, and latent** — `tasks.md:172-178` reads exactly that; `MidiController.cpp:3025-3029` constructs both, `:3188-3192` constructs neither, so no producer of the one-null shape exists today. One-word remedy: guard each dereference |
| SF-19 | The template-change rate limiter's clock is unspecified while its check is written in real seconds (a 5 s burst with per-second sub-window floors in a unit binary) | B8 | neither | not adjudicated — verify at the point of repair |
| SF-20 | The rate limiter's checks pin neither the configured rate (any value in 1..20 passes) nor which message survives (`N >= 1` is satisfied by any admitted message) | E-ADV10 | neither | not adjudicated — verify at the point of repair |
| SF-21 | `MismatchIsOrthogonalToEndpointStatus` asks a runtime C++ case to assert that no enumerator was added; a compiled test observes behaviour, not source diffs, so sru-63's SHALL NOT is unchecked | E-ADV21, F-ADV12 | neither | not adjudicated — verify at the point of repair. Remedy shape: a `static_assert` on an enumerator count, or a `switch` with no `default` in a test TU |
| SF-22 | Sheaf groups 4 and 5 name no file, class or test binary for the work they order, unlike groups 3 and 6 | B14 | neither | not adjudicated — verify at the point of repair |
| SF-23 | `npm ci` under `External/Sheaf/projects/synth/browser` is carried as an approval quoted inside the artifact it authorises | B4 | execution of Sheaf 1.5, 3.5, 7.2 | **settled by operator ruling 3** — the install is approved for this change. What survives is a NOTE: carry the approval in the executor's dispatch brief, not only in tasks.md |

---

# 4. NOTE, consolidated

| id | Finding | Sources | Status |
|---|---|---|---|
| N-1 | Task 1.1's "nothing was lost" assurance cites `6e77142`, a commit reachable from no ref whose tree differs from `4da0206`'s — the commit on this branch with that subject | A4, D3 (independent) | **verified** — `git cat-file -t 6e77142` → `commit`; `git branch -a --contains 6e77142` → no output; trees `09757729…` vs `1a0a278c…`. Remedy: replace with `4da0206` and re-verify the md5 assurance against its tree, which differs |
| N-2 | Sheaf 3.9 and two Check lines name "`engine_tests.cpp`'s controllable-timestamp `Engine<App>`"; no such fixture exists — every Engine there takes a constant lambda | B7 | **verified** — `grep -ni timestampprovider tests/engine_tests.cpp` exit 1; 52 constructions of `engine([] { return std::uint64_t{0}; })` |
| N-3 | Sheaf 3.5's placement parenthetical contradicts its own instruction: the line right after the identity guard is `this.closeInput(...)` (`midi.ts:235`), not `this.inputs.set(...)` (`:241`) | B11 | **verified** — both lines read |
| N-4 | design.md says the existing catalogue case "negates `Type::Shift`" for non-Twister defaults; it is negated nowhere | A5 | not adjudicated |
| N-5 | "Every construction site supplies a real callable" names three of roughly ninety-five | A6 | not adjudicated |
| N-6 | The frogg3rs proposal's Sheaf-overlap sweep names two changes; `shift-and-file-export` touches the same operands and owns smi-16 | A7 | not adjudicated |
| N-7 | The Getting Started Guide's factory-template sentence enumerates pots, mode buttons and pads and does not name the faders — the entire preset is a fader mapping | A8 | not adjudicated (external PDFs; out of my reach this round) |
| N-8 | Two wording defects: the spec delta's garbled "it declares factory template 1 as the device the preset requires"; and the PRG's zero-indexed-channel sentence is scoped to buttons, not faders | A9 | not adjudicated |
| N-9 | Sheaf 1.8's citation repair leaves the sentence that cited the brief dangling, and the adjacent line carries a second planning-shaped reference | B10 | not adjudicated |
| N-10 | frogg3rs 4.2 runs `git checkout` inside the submodule that is the Sheaf change's own working tree, detaching HEAD from `midi-resilience-merge`; nothing says to return it | B12 | not adjudicated |
| N-11 | frogg3rs 1.6 baselines twelve binaries against the old Sheaf and 6.3 re-runs them after the pin swap; 6.3 does not name the swap as an owned cause | B13 | not adjudicated |
| N-12 | frogg3rs task 4.6 is cited as depending only on 6.1/6.2, but 4.1's "reachable from `fork/midi-resilience-merge`" makes it wait on all of Sheaf, since 7.7 is its last task | B (cross-repo section) | not adjudicated |
| N-13 | Orphaned tooling in `projects/synth/browser` (four unused package scripts); none load-bearing | D7 | not adjudicated |
| N-14 | The template-change recognizer and its test are the same invention, so the pair cannot disagree with anything; the only corroboration is a host→device command, not an inbound one | E-ADV11 | not adjudicated (disclosed in Non-Goals) |
| N-15 | `check-modified-requirements-restate-promoted` NOTEs and exits 0 on a promoted scenario the delta deletes | E-ADV18, F-ADV17 | **verified** — the gate's literal output is in the Baseline section above. Deliberate and declared at the top of the delta |
| N-16 | The Rebuild trigger has no standing check that can fail: `held == false && lastClear == Rebuild` is true of a profile that was never held | E-ADV19 | not adjudicated |
| N-17 | Two of smi-16's three ceiling scenarios pass more comfortably when the ceiling is dead, and the ceiling-to-0 control is attached to the case that is green when it is dead | F-ADV11 | not adjudicated |
| N-18 | Two of scw-6's three scenarios and one of sru-64's two are green with tasks 6.2/6.3 entirely omitted | F-ADV10 | not adjudicated (task 7.3 already discloses it) |
| N-19 | Task 4.5's loop asserts `press.type` and `shiftedPress` only; a `Shift` message in `release` or `feedback` passes | F-ADV18 | not adjudicated |
| N-20 | sru-63's withholding rule suppresses the false accusation only while the controller is wholly silent | E-ADV22 | not adjudicated |
| N-21 | No build-time deliverable can reject a wrong CC, channel or template for the new preset | F-ADV20 | not adjudicated (disclosed in design.md; operator ruling 1 governs) |

---

# 5. OPEN items

| item | What settles it | Operator needed? |
|---|---|---|
| O-1. The Launch Control XL fader map (channel 8, CC 77-84, factory template 1) rests on one disassembly of `LaunchControlXL.pyc` under `/Applications`, which no auditor and no adjudicator was permitted to open | Operator ruling 1 settles the provenance question: the map is taken as read and no device is on the execution path. What remains open is whether the reading is *correct*, and that is settled only by the operator's hardware check on the live browser site. Per ruling 2 that check is sequenced after the operator's merge (SF-5). SF-6 reduces the exposure by pasting the second disassembly's literal output | **yes** — the hardware check is the operator's, after the merge |
| O-2. Whether the Pages checkout step actually fails on a `main` carrying the unreachable pin | One `workflow_dispatch` of `pages.yml` against a branch carrying it. Not needed for this cycle: ruling 2 makes the unreachability by design, and BL-6's remedy is a statement, not a fix | no |
| O-3. Whether the historical pins are reachable upstream as `refs/pull/1[345]/head` | `git -C External/Sheaf ls-remote origin`. B ran it and reported the three refs; F's sandbox had no network and I did not re-run it. The local evidence (`branch -r --contains`) is sufficient for BL-6 and is not open | no |
| O-4. Whether `build:cloudflare-pages` is invoked by the Cloudflare Pages project's external build command | Reading that project's configured build command in its dashboard. Out of repository scope; unrelated to this pair | **yes**, if anyone cares |
| O-5. No compiled gate was baselined by anyone: `check-no-juce`, `check-delay-capacity-break-proofs`, all twelve frogg3rs test binaries, `make -C projects/synth test`, the miniapp target, `browser-midi-bridge-test`, and `npm ci` + `npm run test:unit` are unrun across all six reports and by me | Running them. `npm ci` is approved (ruling 3); the C++ gates need a build. The carried 96 kHz deadline failures are the known exception | no — but until it is done, "the gates are green" means the non-compiling gates only |

---

# 6. Verdict

**REJECT** the pair.

Seven confirmed blocking findings:

1. **BL-1** — the cross-repository gate names Sheaf group 6 while the manual rewrite
   depends on group 3, so `MANUAL.md` can ship into the app bundle documenting three
   recoveries no shipped code performs.
2. **BL-2** — `check_docs_match_device_preconditions.py` cannot be written as specified:
   eleven verified sub-parts, including two positive controls that cannot turn it red, a
   floor whose parse can reach at most four of seven lists, and a comparison that never
   reaches the value that caused the original incident.
3. **BL-3** — five Sheaf delta scenarios name a test no task delivers, in the repository
   that has no gate reading a `Check:` line, with both scenarios' requirements bound for
   the promoted spec.
4. **BL-4** — smi-14's final clause is contradicted by the design that implements it, and
   the scenario holding it up is green under both readings.
5. **BL-5** — Sheaf task 3.5 orders a call from a place that cannot make it; the browser
   endpoint-open seam is named nowhere and covered by nothing.
6. **BL-6** — the submodule pin is reachable from no remote (confirmed as fact; under
   operator ruling 2 this blocks nothing and its remedy is a statement plus sequencing).
7. **BL-7** — task 4.8 must run before task 3.1 and is numbered a group after it.

One blocking framing refuted: **DR-1**, task 4.6's alleged compile break. The citation
defect is real; the mechanism is not, because `shift_` is private and Sheaf task 3.1
leaves the type names unchanged.

Nothing among the blocking set is open. Every remedy above is artifact work — no design is
being asked to change, and the two changes' tracing at the citation level is unusually
good: every line number I sampled resolved to the construct named, every count assertion
was exact, both deltas validate strictly, and all five non-compiling frogg3rs gates and
both Sheaf-side `openspec validate --strict` runs are green. What the pair is weak at is
the layer above that: five of the seven blocking findings are mechanisms specified so that
they cannot fail, and no gate in either repository reads them.

---

# 7. Coverage note

**Reached independently by more than one report:**

- **BL-1** — A1 and F-ADV1, from the same three artifacts, with the same remedy, by two
  auditors who could not see each other's work.
- **BL-2** — the densest convergence in the round, but not on one sub-part: B2 and C3
  found the count contradiction; E-ADV1 and F-ADV2 found control (a) independently, and E
  alone found control (b); F-ADV3 alone found the Launchpad parse; E-ADV2(i) and F-ADV5
  found the pre-comma comparison independently; A3, E-ADV3 and F-ADV4 found the marker
  set independently; F-ADV7 alone found the spec/design rule divergence. The consolidated
  finding is larger than any single report's.
- **BL-6** — B1 and E-ADV13, independently; B's contribution (`.gitmodules` names upstream,
  which the fork push does not reach) is the one that defeats the obvious remedy.
- **N-1** — A4 and D3, independently.

**Reached by exactly one report:**

- **BL-3** — C1 alone. F-ADV10 touched two of the same scenarios but only for their
  vacuity, not for the fact that no task writes them; E-ADV16 established the missing gate
  without enumerating the unbacked names.
- **BL-4** — E-ADV6 alone. No other report opened smi-14's final clause against
  design.md:295.
- **BL-5** — E-ADV8 alone in its correct form. F-ADV9 is an adjacent weaker finding that
  mis-locates the seam (it states `midi.ts` calls the facade; it cannot). C had audited
  task 3.5's ABI work and recorded it as adequately specified, withdrawing that in the
  exchange.
- **BL-7** — B3 alone, and graded SHOULD-FIX there; I grade it blocking on what breaks.
- **DR-1's refutation** — B alone. Four reports read the finding as blocking; one checked
  whether the case must name the field, and that is the check that settles it.

D contributed no blocking finding and missed BL-6 while holding the gitlink in its hand;
its six SHOULD-FIX findings (SF-13 through SF-17) are all Impact-completeness defects no
other report reached, each verified above.
