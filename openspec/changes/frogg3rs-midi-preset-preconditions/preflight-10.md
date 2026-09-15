# Preflight 10 — adjudication

Adjudicator did not write the change, the briefs, or any of the six reports.
Every ruling below rests on a command run here or a file opened here. Agreement
between reporters was not treated as evidence; several findings that five
auditors confirmed are graded down below on my own measurement, and one
sub-claim that five auditors asserted is refuted.

Trees at adjudication:

```
$ git rev-parse HEAD                                    67b36ed9ba17e5645bf4b703ad89d09bc88a3f5d
$ git -C External/Sheaf rev-parse HEAD                  daee0c1c3dcf10a7cabfb9795529b01b7b02376c
$ git status --short  /  git -C External/Sheaf status --short     (both empty)
$ make --version | head -1                              GNU Make 3.81
```

Nothing built, nothing edited in either tree, nothing installed. Writes were two
throwaway files in the scratchpad (`adj_ws.txt`, `adj_filecheck.cpp`). No
`preflight*.md` inside a change directory was used as evidence.

---

# 1. Verdict

**REJECT the pair.**

Twelve blocking findings confirmed. The reason is not their number but their
shape: **the two mechanisms this cycle relies on to keep itself honest — frogg3rs
task 4.1's cross-repository gate and task 6.6's Check-line discipline — are each
independently unable to do the job they are named for**, and Sheaf's task 1.9
leaves three rulings to the executor in a task whose own preamble says none is
left to the executor. On top of that, one requirement (`sru-63`) is specified
narrower than the mechanism it claims to mirror, which ships the feature inverted
for the change's own headline addition.

Counts among blocking (as graded after the exchange):
**confirmed 12 · refuted-at-blocking 3 (all survive as SHOULD-FIX) · open 0.**

Two items the reports left OPEN are **closed** here by `measure-a2-report.md`.
One OPEN (A4) stands and is already dispositioned by the operator's ruling 1.

---

# 2. Confirmed blocking findings

Work down this list. Each names its sources, the class of defect, what breaks,
whether it blocks execution or delivery, and the remedy as a condition on the
artifacts.

---

## BF-1 — Task 4.1's gate step 6 is inert: it measures 15 today against a stated 0, and asserts "at least 1"

**Sources:** A1 · B3 (escalated SHOULD-FIX → BLOCKING in exchange) · C9 (upgraded
NOTE → BLOCKING in exchange) · E-ADV2 · F-ADV4.
**Class:** a blank filled toward green — the export's name was deliberately not
fixed in Sheaf task 3.6, so the gate had no literal to grep and fell back to a
prefix the file has always carried.
**Blocks execution.**

```
$ git -C External/Sheaf show HEAD:projects/synth/browser/src/build-browser-apps.mjs | grep -c '_synth_browser_'
15
$ grep -n "_synth_browser_" openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md
412:         | grep -c '_synth_browser_'` reports at least `1` inside that file's
```

`tasks.md:411-413` reads "reports at least `1` … (today it reports `0`)". The
parenthetical is false and the assertion is satisfied before any Sheaf work runs.
This is one of the two probes task 4.1 names as what makes the gate hard to fake
("faking a whole test file and a wasm export list is a materially different, and
much harder, thing than adding a comment"), and the gate admits frogg3rs group 4,
task 3.1 and task 5.2.

**Remedy (condition on tasks.md):** step 6 must assert a set derived from the
`extern "C"` *definitions* in `browser/cpp/BrowserRuntimeAbi.cpp` against
`EXPORTED_FUNCTIONS`, or name the literal export Sheaf task 3.6 delivers. The
recorded pre-state must be the measured `15`, not `0`.

---

## BF-2 — Task 4.1's gate step 5 rejects honest delivery on the browser half, and accepts a comment on both halves

**Sources:** E-ADV1 (the rejection half, sole) · F-ADV5 (the comment half) ·
B's own correction to its "measured TRUE" section.
**Class:** a probe shaped to one file's test convention and applied to two, plus a
filter its sibling steps carry and it does not.
**Blocks execution.**

```
$ grep -c 'TEST_CASE' External/Sheaf/projects/synth/tests/browser_midi_bridge_tests.cpp
0
$ grep -c TestReconcileBindsSlotsIndependentlyAndResyncsOutputs  …/browser_midi_bridge_tests.cpp   2
$ grep -c TestOfflineSlotDoesNotRemapAnotherSelectedSlot          …/browser_midi_bridge_tests.cpp   2
$ grep -c TestIncomingAndOutgoingSysexStayOnSelectedControllerSlot …/browser_midi_bridge_tests.cpp  2
$ grep -c engine_initialize_orders_init_before_ui_state           …/engine_tests.cpp                1
$ grep -n '#define TEST_CASE' -A4 …/engine_tests.cpp
68:#define TEST_CASE(name) \
69-    void name(); \
70-    Register reg_##name(#name, &name); \
71-    void name()
```

`browser_midi_bridge_tests.cpp` uses no `TEST_CASE`; every case is a definition
plus a call from `main()`, so a delivered name occurs **twice**. Step 5b demands
exactly `1`. The only shapes giving `1` are a definition never called, a call
never defined (does not link), or a comment. `engine_tests.cpp`'s auto-registering
macro puts each name on one line, which is why step 5a's `2` is right there and
step 5b's `1` is wrong here.

Second half, read from `tasks.md:397-415`: steps 3 and 4 pipe through
`sed 's#//.*##'` and the task spends a paragraph on why; **step 5 has no filter of
any kind**. Two `// TODO: <case name>` lines satisfy step 5a, one satisfies 5b.

Composed with BF-1, task 4.1's gate has **no working probe for the browser half at
all**.

**Remedy (condition on tasks.md):** step 5 strips comments the way steps 3-4 do,
**and** asserts `>= 1` per case name separately rather than as one alternation
with a fixed total.

---

## BF-3 — Task 6.6's `<M>` invariant is unsatisfiable on correct work, and its second positive control is wrong in direction

**Sources:** B1 · E-ADV3 · F-ADV1 (C and D confirmed by reading the script's
branches).
**Class:** an aggregate assertion derived from a wrong model of the script's
bucketing.
**Blocks execution** (the task's own gate cannot pass).

```
$ D=openspec/changes/frogg3rs-midi-preset-preconditions
$ grep -h '^- Check:' $D/specs/*/spec.md | wc -l                                12
$ grep -h '^- Check:' $D/specs/*/spec.md | grep -c 'not yet delivered'          10
$ grep -h '^- Check:' $D/specs/*/spec.md | grep -ciE '^- check: *(none|operator step)'   1
$ grep -rhiE '^- Check: *(none|operator step|not yet delivered)' openspec/specs/ | wc -l  7
$ grep -rh  '^- Check:' openspec/specs/ | wc -l                                 61
$ grep -h '^- Check: not yet delivered' $D/specs/*/spec.md | grep -c '`'         0
$ python3 app/check_spec_checks_resolve.py app
check-spec-checks-resolve: OK - 55 Check reference(s) resolved, 18 declared as having no automated check
```

`check_spec_checks_resolve.py:96` `NO_CHECK` accepts `not yet delivered`;
`:383-385` sends such a line (no backticked token — measured `0` above) to
`declared_manual`; `:468-471` increments `resolved` only for `not declared`.
So 7 + 11 = 18 = `<M>`, and 61 + 12 = 73 = 55 + 18. A complete rewrite of the ten
moves them to `resolved`: **`<M>` falls from 18 to 8**.

Task 6.6 (`tasks.md`) asserts "the `<M>` (declared-manual) figure is **unchanged**
from the one task 1.6 recorded" — false on correct work — and its second positive
control asserts the placeholder makes `<M>` "rise by one against task 1.6's
baseline", where nine rewrites plus one placeholder leaves `<M>` at 9, a fall of
nine. **Both assertions are unsatisfiable on correct work.**

Also confirmed: 6.6 enumerates nine tasks (1.8, 4.3, 4.4, 4.5, 4.6(a), 4.6(b),
4.8, 5.1, 5.2) while ten lines carry the deferral (4.4 is cited twice), so its
`<N>` parenthetical "(nine, as of this writing)" is wrong by one. Its own
"re-count rather than trusting nine" covers that and covers nothing of `<M>`.

**Sub-claim REFUTED — recorded so it is not carried into the repair.** B, C, D, E
and F all state that 6.6's stated *reason* is false: "frogg3rs's
`check_spec_checks_resolve.py` does not short-circuit on the 'not yet delivered'
prefix the way Sheaf's does". It is **accurate**. The script does not
short-circuit — `:378-385`'s own comment says an earlier version "returned
immediately on seeing it, so `Check: none, see \`a_fake_case\`` passed with a
false claim attached. The declaration is accepted; anything it names is still
resolved." The header at `:67-71` states the same ("An honest gap stays sayable").
6.6's reason is right; only its arithmetic is wrong. Do not "fix" the reason.

**Remedy (condition on tasks.md):** replace the `<M>`-unchanged clause with
`<M>` falls by exactly the count the first command measured, and `<N>` rises by
the same count; restate the second positive control as `<M>` falling by one
*less* than that count. Land BS-3 (whitespace anchor, below) in the same edit.

---

## BF-4 — Task 6.6's rewritten form is satisfied by any pre-existing unrelated case, or by any wired gate script

**Sources:** E-ADV4 · F-ADV3.
**Class:** aggregate assertions blind to what a line names.
**Blocks delivery** — what survives is promoted verbatim into `openspec/specs/`
at archive, where the script accepts it indefinitely.

```
$ sed -n '331,347p' app/check_spec_checks_resolve.py
    return part in tests or part in FILES          # part_resolves
…
    named = [p for p in split_parts(tok) if is_claim(p)]
    if not named: return False
    return all(part_resolves(p, tests) for p in named)     # resolves
$ sed -n '461,466p' app/check_spec_checks_resolve.py
                gate_here = any(p in GATE_SCRIPTS for t in claims for p in split_parts(t))
                if not declared and not line_cases and not gate_here:
```

`tests` and `FILES` are tree-wide sets. Nothing ties a cited case to the scenario
above it, to the delivering task, or to newness; and `gate_here` resolves a line
with **no case at all** when it names any script the `test:` recipe runs
(`gate_scripts()`, `:248-288`, reads the rule's own prerequisites). Task 6.6's
three assertions (`grep -c 'not yet delivered'` → 0, exit 0, the `<N>`/`<M>` pair)
are all aggregate and cannot see which case a line names. Task 6.4 ("every
scenario in the delta either has a check that passes now") carries no command.

The tree already demonstrates the first path behaviourally: the delta's one
non-deferred line, `specs/…/spec.md:24`, cites
`device_defaults_are_valid_and_address_exactly_the_documented_controls`, which
`git log -S` puts at `9f54947`, before this change — and the script counts it in
`<N>`.

**Remedy (condition on tasks.md):** record the delta's case-name set at task 1.6,
and at 6.6 assert that every rewritten line names a case **not in that baseline
set** and defined in the file the line names; forbid a bare gate-script citation
on a line that replaces a deferral.

---

## BF-5 — Sheaf rule 2(a) has no quantifier, and four deferred `Check:` lines cite more than one task

**Sources:** B2 · C3 (which reports three lines; four is correct).
**Class:** an undefined quantifier in a rule whose own preamble says nothing is
left to the executor.
**Blocks execution.**

Rule 2(a), `External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md:253-258`:

> …its own prose names a task number `N.M` that is present as a `- [ ]` item in
> the `tasks.md` of the change whose directory this spec file sits under, and (b)
> **no** backticked token on the line already resolves as a real test case under
> rule 3. **If the cited task** is absent, or is present as `- [x]`, or any
> backticked token does resolve, the line is RED.

The resolve clause is existential ("a task number … that is present"); the RED
clause is singular-definite ("the cited task"). With two citations and one ticked,
they give opposite answers. My enumeration (case-insensitive, per-line distinct
task numbers):

```
total Check lines: 31 deferred: 24 multi-task: 4
  specs/synth-midi-instrument/spec.md 71 ['3.10', '3.5']
  specs/synth-midi-instrument/spec.md 76 ['3.10', '3.5']
  specs/synth-midi-instrument/spec.md 81 ['3.10', '3.5']
  specs/synth-midi-instrument/spec.md 97 ['3.5', '3.6', '3.7']
```

The window is mandatory, not hypothetical — `tasks.md:917`: "task 3.5 must be
complete before any of them is written". And `projects/synth/Makefile:274`'s
`test:` is one linear prerequisite list with `check-ui-boundary`,
`check-ui-boundary-empty-discovery`, `check-app-bundle-plist`, `test-wasm32` ahead
of every `$(…_TEST_BIN)`; under GNU Make 3.81 a red first prerequisite aborts the
recipe. **Under the all-of reading, no test binary in the Sheaf repository runs
for that window, and the four lines have no legal repair.** Positive control (c)
presumes a single citation and settles nothing.

**Remedy (condition on tasks.md):** write the quantifier ("at least one cited task
number is present as `- [ ]`; a line citing a task only for context is not held to
that task's tick state"), and add a fifth positive control over a multi-task line
with one citation ticked.

---

## BF-6 — `browser_midi_bridge_tests.cpp` cannot drive the ABI entry point as tasks 3.6/3.7 specify

**Sources:** C1 (reached by no other report).
**Class:** a prescribed construction whose two mechanical preconditions are absent
from the tree and named in no artifact.
**Blocks execution.**

*Link line.* `BrowserRuntimeAbi.o` is on exactly one link line, and it is not the
bridge tests':

```
$ grep -n 'BrowserRuntimeAbi' External/Sheaf/projects/synth/Makefile
223:$(BUILD_DIR)/BrowserRuntimeAbi.o: browser/cpp/BrowserRuntimeAbi.cpp | $(BUILD_SENTINEL)
226:$(BROWSER_CONTRACT_TEST_BIN): tests/browser_runtime_contract_tests.cpp $(BUILD_DIR)/BrowserRuntimeAbi.o $(LIB)
227:	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) $< $(BUILD_DIR)/BrowserRuntimeAbi.o $(LIB) -o $@
$ sed -n '235,236p' External/Sheaf/projects/synth/Makefile
$(BROWSER_MIDI_BRIDGE_TEST_BIN): tests/browser_midi_bridge_tests.cpp … $(LIB)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LIB) -o $@
```

The extra cost is real: `BrowserRuntimeAbi.cpp:3` declares
`extern "C" synth_browser::RuntimeAbi* synth_browser_create_runtime();` and `:48`
calls it, which is why `tests/browser_runtime_contract_tests.cpp:26-29` supplies
its own definition.

*Handle reach.* `BrowserRuntimeAbi.cpp:7-10` reinterpret-casts the handle to
`synth_browser::RuntimeAbi*`. Its `engine_`/`midiBridge_` are **private**:

```
$ sed -n '781p;1074,1075p' …/include/synth/browser/BrowserRuntime.hpp
    synth::Engine<App>& Engine() { return engine_; }
    synth::Engine<App> engine_;
    BrowserMidiBridge<synth::Engine<App>> midiBridge_;
$ awk 'NR<=1075 && /^[[:space:]]*(public|private|protected):/ {last=NR": "$0} END{print last}' …
801:  private:
$ sed -n '90,91p' …/tests/browser_midi_bridge_tests.cpp
using RealEngine = synth::Engine<RealBridgeTestApp>;
using RealBridge = synth_browser::BrowserMidiBridge<RealEngine>;
```

Task 3.7 prescribes, verbatim, "**drives** the new browser-runtime ABI entry point
by name, on a `RealBridge` over a `RealEngine`" — two different engines. The only
working route (a `RuntimeAbiAdapter` rooted at a `BrowserRuntime`, read back
through the public `Engine()` at `:781`) is written down nowhere:
`grep -n 'BrowserRuntimeAbi.o\|RuntimeAbiAdapter\|BROWSER_MIDI_BRIDGE_TEST_BIN'`
over the change's `tasks.md` and `design.md` returns no hit for any of the three.

The green-ward shortcut is to call the bridge's clear directly and call it "the
ABI entry point"; the companion case `ReconcileAloneDoesNotClearAHeldModifier`
does not catch that, since by 3.7's own description it goes red only for a clear
placed in `ops.openInput`/`ops.openOutput`.

**Remedy (condition on tasks.md):** name the Makefile edit (add
`$(BUILD_DIR)/BrowserRuntimeAbi.o` to `$(BROWSER_MIDI_BRIDGE_TEST_BIN)`'s
prerequisites and link line, and supply the `synth_browser_create_runtime`
definition the way the contract test does), **and** state the construction that
lets the entry point and the observation see the same engine — or move the case to
a binary where both already hold. Note that BF-1's step 6 and BF-2's step 5b both
gate on this case existing in this file.

---

## BF-7 — `launchpad-model-on-the-row`'s disposition is scoped by a task count the tree inverts, and the STOP instrument re-confirms the count

**Sources:** D1 (reached by no other report).
**Class:** a disposition scoped by a count instead of a path, with a detection
instrument that can only confirm the count, never the premise.
**Blocks execution.**

```
$ grep -n '2\.1 Add' External/Sheaf/openspec/changes/launchpad-model-on-the-row/tasks.md
20:- [ ] 2.1 Add `launchpadModel` to `MidiControllerProfileConfig`.
$ grep -rn 'launchpadModel' External/Sheaf/projects/synth/include/synth/MidiController.hpp
954:    LaunchpadController launchpadModel = LaunchpadController::LaunchpadX;
$ grep -rn 'launchpadModel' app/
app/FroggersMidiCatalog.hpp:249:    config.launchpadModel = controller;
app/FroggersMidiCatalogTests.cpp:658:        REQUIRE_TRUE(device.config.launchpadModel == preset.controller);
$ x=11 open=3    (the 11/14 the proposal records — both correct)
```

`proposal.md:199` states "**`launchpad-model-on-the-row`'s task 2.1 has not
started**, so its frogg3rs-side task 6.2 is not reachable yet." Task 2.1's field
is live in Sheaf and already consumed in `app/`. Task 6.2's third clause is landed
too:

```
$ sed -n '294p;313,315p' openspec/specs/froggers-sheaf-runtime-app/spec.md
… SHALL let a Launchpad row choose which Launchpad model it addresses and no other row choose anything of the sort …
- **AND** a Launchpad row shows a "Variant" selector on that first line,
  holding the model its profile records
```

The only instrument the row hands the executor is "re-run this row's task-count
grep before assuming the baseline this table records still holds" — which returns
the same 11/3 and confirms the row rather than the premise. The collision surface
is `app/FroggersMidiCatalog.hpp` (6.2's preset edits at `:249`, inside the shared
`LaunchpadDeviceDefault` helper, against this change's task 5.5 editing the
`catalog.deviceDefaults` initializer list at `:329-336`) and the submodule pin
task 4.2 moves. The change's own neighbouring row gets this right ("Read the tree,
not the boxes").

**Remedy (condition on proposal.md):** rescope the row's disposition and its STOP
instrument to the **paths** (`grep -rn 'launchpadModel' app/ External/Sheaf/projects/synth/include/`
plus the promoted-spec clauses), not to the box counts, and restate the
no-collision verdict on what the tree shows.

---

## BF-8 — The EndpointOpen phrase family's scope is stated four ways across three artifacts, and nothing gives a `MANUAL.md` subsection a host

**Sources:** E-ADV10.
**Class:** one rule stated with two different quantifiers inside the paragraph the
implementer works from.
**Blocks execution** — task 1.8 is the first code-writing task and cannot be
written without the ruling.

```
specs/froggers-midi-controller-mappings/spec.md:45
  "…for every trigger this change's recovery text offers … ('unplugging and
   reconnecting the controller' for EndpointOpen — a subsection naming only
   some of these fails)"                                        unconditional
design.md:414-416  "Rebuild and Ceiling always, and EndpointOpen wherever it applies"   conditional
design.md:419-420  "A subsection naming only one of these three families fails rule 1"  "only one"
tasks.md:187       "…plus EndpointOpen on hosts where it applies"                       conditional
tasks.md:192-193   "A subsection missing any one of these three phrase-families fails"  unconditional
```

The script's subject is two heading-delimited `MANUAL.md` subsections
(`MANUAL.md:308` `### Hold Drill`, `:314` `### Shift`), and nothing in either
repository partitions a `MANUAL.md` subsection by host. "Wherever it applies" has
no mechanical referent.

The cheap resolution (EndpointOpen optional) degenerates rule 1 to a two-family
conjunction and ships a Hold Drill subsection with no reconnect route — where rule
3 then never fires, because it triggers on the word "reconnect".

**Narrowing accepted from C, recorded so the repair is minimal:** `tasks.md:192`'s
unconditional sentence is the operative instruction and sits four lines after
`:187`'s conditional, so a careful reader can resolve it. The defect is the
contradiction and the `only one` / `any one` disagreement, not an absent ruling
that nothing in the artifacts answers.

**Remedy (condition on all three artifacts):** state one quantifier. If
EndpointOpen is unconditional, delete "wherever it applies" and "on hosts where it
applies"; fix `design.md:420`'s "only one" to "any one".

---

## BF-9 — `sru-63` defines the mapped set over system-button associations only, while the input chain has five matchers; four of seven presets and this change's own seventh default ship inverted

**Sources:** F-ADV6 (reached by no other report). The most consequential finding in
the round: it is a **requirement** defect, not a check defect.
**Class:** a requirement specified narrower than the mechanism it claims to mirror,
with no named scenario able to expose the gap.
**Blocks execution** — the implementer builds what the requirement defines.

`specs/synth-runtime-ui/spec.md:10` opens broad and then enumerates narrow:

> THE synth system SHALL decide what a profile maps **by the same predicate its
> input chain matches inbound messages with, covering both of the addressing modes
> a profile may use**. A control-addressed **association** … A position-addressed
> **association** …

The input chain has five matchers, not two:

```
$ grep -n 'FindTurn\|FindPush\|FindGesture\|FindAppAction\|FindAssociation\|config_.sceneBlend' \
      External/Sheaf/projects/synth/src/MidiController.cpp
711:        if (const EncoderMidiMapping* mapping = FindTurn(midi)) {
755:    if (const EncoderMidiMapping* mapping = FindPush(midi)) {
836:    if (const AnalogMidiMapping* mapping = FindGesture(midi)) {
841:    if (const AnalogAppActionMapping* mapping = FindAppAction(midi)) {
847:    if (config_.sceneBlend.has_value() && *config_.sceneBlend == address) {
939:    const SystemButtonMidiAssociation* association = FindAssociation(midi);
$ sed -n '227,231p' External/Sheaf/projects/synth/include/synth/MidiController.hpp
struct EncoderMidiMapping { MidiControlAddress control; std::size_t slotIx; std::size_t position; };
```

`EncoderMidiMapping` is its own struct and is called an association nowhere. The
delta never mentions the other two families:

```
$ grep -ic 'encoder\|analog' External/Sheaf/openspec/changes/midi-controller-resilience/specs/synth-runtime-ui/spec.md
0
$ grep -n 'FindAssociation\|mapped set' …/design.md
597:`SystemButtonMidiInProcessor::FindAssociation` matches an inbound address by
611:So a mapped set built from `association.control` is **empty for every Launchpad-kind slot**…
627:mapped set is well defined for both modes — a control-addressed association …
```

Production consequence, on this repository's own presets
(`app/FroggersMidiCatalog.hpp:161-167`): `config.encoderInput` turns, plus
`config.analogInput`'s `sceneBlend` at channel 0 CC 15 and the BPM app action at
channel 0 CC 14, all land in the **unmapped** set. That is the same inversion
`design.md:611-613` congratulates itself on having avoided for Launchpad profiles,
reintroduced for the non-Launchpad presets.

No named scenario can expose it. All six are written in system-message terms
(channel 4 controls 0-1, channel 4 controls 8-13), and the one non-system case is
Launchpad-kind, where `KindSupport` (`src/MidiController.cpp:3451`) reports
`encoders = false, analogs = false`.

**And this change's own headline addition is in the hole.** The seventh default is
Generic-kind and analog-only — `measure-a2-report.md:36-38` records
`config.analogInput = { .sceneBlend = {channel 8, cc 77} }` and nothing else — so
the Launch Control XL would ship reporting its one working control as unmapped.

**Remedy (condition on `specs/synth-runtime-ui/spec.md` and `design.md`):** state
the mapped set as the union over **every** matcher the input chain runs
(`FindTurn`, `FindPush`, `FindGesture`, `FindAppAction`, `sceneBlend`,
`FindAssociation`), not over associations alone; add a scenario carrying an
encoder turn and an analog address whose positive control is "compute the mapped
set from system-message associations alone, and this case fails".

---

## BF-10 — Sheaf task 1.9 specifies no comment handling for the file a `Check:` line names

**Sources:** F-ADV11 (reached by no other report).
**Class:** an omission in an ordered dispatch the task presents as exhaustive.
**Blocks execution** — the script is written in group 1; fixing it later is a
rewrite.

```
$ awk '/^- \[ \] 1\.9/,/^- \[ \] 1\.10|^## 2\./' \
    External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md | grep -ic 'comment\|blank'
0
```

Rule 3 (`tasks.md:261-275`) resolves a `TEST_CASE(name)`, a bare
`[static] void Name()` "that something else in the same file calls a second time by
name", or a JS `test(...)`/`it(...)` "read from that file" — with no statement that
the file is read with its comment regions blanked. The frogg3rs sibling documents
this exact hole as its own motivating defect and implements the fix:

```
$ sed -n '55,62p' app/check_spec_checks_resolve.py
COMMENT TEXT DEFINES NOTHING. The index reads each file with its comment
regions blanked out first … `// TEST_CASE(gone)` otherwise indexes exactly like a
case the file runs … The bare-function form is the same hole one move further
along: its rule is a declaration plus a second mention of the name, and a
commented-out call is a mention, so the declaration alone would be enough.
$ grep -n 'def blank_comment_regions' app/check_spec_checks_resolve.py
154:def blank_comment_regions(text, template_strings):
```

None of controls (a)-(d) exercises it — all four act on the `Check:` line, never on
the file it names. Composed with BF-2, one commented-out case name satisfies both
repositories' gates.

**Remedy (condition on tasks.md):** rule 3 states that the named file is read with
`//` and `/* */` regions blanked (string and character literals preserved), and a
fifth positive control comments out a resolving case's declaration and confirms its
line goes red.

---

## BF-11 — Task 6.7's archive STOP condition is unperformable, and the count it sets the executor up to expect is understated

**Sources:** F-ADV17 (reached by no other report).
**Class:** a stop condition written against output the tool does not produce.
**Blocks delivery.**

Task 6.7 (`tasks.md`): "If the command's own output (or `--json`, if this runs
non-interactively) **names any OTHER unticked task besides 5.4**, or a validation
failure, STOP and report it." The tool emits a count and never a name, in every
mode:

```
$ grep -n 'incompleteTasks\|Proceed with spec updates' \
    /opt/homebrew/lib/node_modules/@fission-ai/openspec/dist/core/archive.js
274:  const incompleteTasks = Math.max(progress.total - progress.completed, 0);
278:    throw new ArchiveBlockedError('archive_tasks_incomplete', `${incompleteTasks} incomplete task(s) found for change '${changeName}'.`, …)
284:    message: `Warning: ${incompleteTasks} incomplete task(s) found. Continue?`
293:    console.log(`Warning: ${incompleteTasks} incomplete task(s) found. Continuing due to --yes flag.`)
323:    message: 'Proceed with spec updates?'
$ sed -n '29,35p' /opt/homebrew/lib/node_modules/@fission-ai/openspec/dist/utils/task-progress.js
export function formatTaskStatus(progress) {
    if (progress.total === 0) return 'No tasks';
    if (progress.completed === progress.total) return '✓ Complete';
    return `${progress.completed}/${progress.total} tasks`;
}
```

Two further facts confirmed: (i) task 6.7's own order is "… → `openspec archive
… -y` → commit → push", so 6.7 is itself unticked when the archive runs — the
count is **≥ 2** (5.4 and 6.7), not the single expected exception the task's
framing sets up; (ii) `--yes` also silently auto-answers the second prompt at
`:316-323` ("Proceed with spec updates?"), which 6.7 describes as "used here for
one known, disclosed reason".

**Remedy (condition on tasks.md):** replace the names-a-task STOP with an assertion
that the printed count **equals** an enumerated set of deliberately-unticked tasks,
verified by `grep -n '^- \[ \] ' tasks.md` immediately before the archive; and
disclose that `-y` answers the spec-update prompt too.

---

## BF-12 — `design.md`'s recorded build-and-run block is not the transcript of the run it records

**Sources:** A2 (three sub-defects) · D4 (sub-defect c only).
**Class:** a recorded command that does not travel with its literal output.
**Blocks delivery** — it is the change's only behavioural evidence for its central
addition.

The block at `design.md:215-235`. All three sub-defects confirmed here:

**(a) the `make` line names targets that cannot resolve.** Recorded:
`$ nice make -C app -j2 app/build/froggers_controllers_page_tests app/build/froggers_midi_catalog_tests`

```
$ make -C app -n app/build/froggers_midi_catalog_tests
make: *** No rule to make target `app/build/froggers_midi_catalog_tests'.  Stop.
$ make -C app -n build/froggers_midi_catalog_tests
make: *** No rule to make target `build/froggers_midi_catalog_tests'.  Stop.
$ make -C app -n "$PWD/app/build/froggers_midi_catalog_tests"
/Library/Developer/CommandLineTools/usr/bin/make -C …/External/Sheaf/projects/synth build
```

`app/Makefile:17` `APP_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))`
and `:29` `BUILD_DIR := $(APP_DIR)/build` — every binary target is absolute.

**(b) the failure lines show a bare filename; this Makefile produces an absolute
one.** `app/Makefile:121` `MIDI_CATALOG_SOURCES := $(APP_DIR)/FroggersMidiCatalogTests.cpp`,
`:325` compiles it directly, `grep -c 'prefix-map' app/Makefile` → `0`, and
`REQUIRE_TRUE` (`app/FroggersMidiCatalogTests.cpp:72-79`) emits `__FILE__`. Settled
without building, by preprocessing a one-line scratch file given an absolute path:

```
$ clang++ -E /…/scratchpad/adj_filecheck.cpp | tail -1
const char* f = "/private/tmp/…/scratchpad/adj_filecheck.cpp";
```

**(c) the case count is off by one.** `grep -c '^TEST_CASE(' app/FroggersMidiCatalogTests.cpp`
→ **9**; one `[FAIL]` plus "… (seven other cases, all [PASS])" accounts for eight.
The companion half is right — `app/FroggersControllersPageTests.cpp` → **4**, and
the block names all four — which is what isolates the defect to the elided half.

**The cause is settled, not open.** `measure-a2-report.md` is the genuine record:
the measurement *was* taken, in a detached worktree `$M` at commit `0acc093`, with
both binaries deleted first and a baseline re-run without the seventh default. Its
command is `nice make -C "$M/app" -j2 "$M/app/build/…" "$M/app/build/…"` (absolute,
resolvable), its failure lines read `/private/tmp/.../app/FroggersMidiCatalogTests.cpp:399`
(absolute, elided with `...`), and its `froggers_midi_catalog_tests` transcript
lists **nine** cases, one FAIL and eight PASS. `design.md`'s block is a lossy
transcription of that record: the `$M` prefixes were rewritten to relative paths
that do not resolve, the elided absolute file paths were shortened to basenames,
and eight PASS lines were compressed to "seven other cases".

**Remedy (condition on design.md), per the operator's ruling:** the design carries
`measure-a2-report.md`'s **literal lines** — its absolute `$M` paths in the build
command and the elided `/private/tmp/...` file paths exactly as printed, with all
nine `[PASS]`/`[FAIL]` lines. The measurement is **not** to be repeated.

**Two OPEN items in the reports close on this same record:**
- F's and E's "which of two causes produced the block" — closed: neither. The run
  was taken; the transcription lost it.
- B's OPEN, whether `real_catalog_defaults_generate_and_accept_adds_through_the_view_model`
  passes with an analog-only Generic seventh default — closed: it **PASSES**
  (`measure-a2-report.md:112`, and the classified failure list at `:179`, which
  records the wizard, `GenerateProfile`, `AddController` and every
  `AddSingle`/`AddBlock` call succeeding for the new default). `design.md:215-220`'s
  substantive claim is therefore true; only its transcript is wrong.

---

# 3. Severity disputes — ruled

All three disputes are ruled **against the BLOCKING grade**, on my own
measurement. In each case the mechanism is real and confirmed; what fails is the
claim about consequence.

---

## BS-1 (was F-ADV7, BLOCKING in F; confirmed BLOCKING by A, B, D, E; REFUTED at BLOCKING by C) — task 1.8 wires a knowingly-red gate into `app/Makefile`'s `test:`

**Ruling: mechanism CONFIRMED · severity SHOULD-FIX · blocks neither.**

Mechanism confirmed: `app/Makefile:344`'s `test:` is a linear prerequisite list of
twelve `check-*` targets ahead of every `$(…_BIN)`, all invoked from the recipe;
GNU Make 3.81 aborts at the first failing prerequisite. The new check is genuinely
red — rule 2 forbids "pressed and released again" and `MANUAL.md:320` carries it
verbatim. Task 1.8 requires four separate failures and says "Wire only this half
into `app/Makefile` for now". Task 3.1, which closes the window, opens
"**Gate.** … do not start until Sheaf's `midi-controller-resilience` change is
complete through its own task 7.9."

C's refutation basis verified at `tasks.md:155-160`, which carries the exact
instruction F proposes as its remedy, with the same GNU Make 3.81 citation:

> `make -C app test` does not necessarily run any of the twelve binaries if any
> `check-*` prerequisite is red — GNU Make 3.81 aborts a linear prerequisite list
> at the first failing one — so if any check is red when this task runs, run each
> of the twelve binaries by path directly, note which ran and which did not, and
> say so explicitly rather than describing `make -C app test` as this change's
> green gate.

And nothing is rendered unperformable:

```
$ grep -n 'make -C app test' openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md
155:      `make -C app test` does not necessarily run any of the twelve binaries
160:      describing `make -C app test` as this change's green gate.
```

**No task between 1.8 and 3.1 asserts `make -C app test` exits 0.** No task is
blocked; no gate produces a false green; delivery is unaffected (6.3's postflight
runs after 3.1 has closed the window). The residual is real and narrow: 1.6's
instruction is scoped to "when this task runs" and is not carried forward across
the long 1.8 → 3.1 window, so an unrelated regression arriving in it is
indistinguishable from the known red.

**Remedy:** one clause in task 1.8 carrying 1.6's existing sentence forward through
the window.

---

## BS-2 (was F-ADV12, BLOCKING in F; confirmed BLOCKING by B, D, E; REFUTED at BLOCKING by C) — the marker-completeness guard has no defined subject

**Ruling: wording defect CONFIRMED · consequence claim REFUTED · severity
SHOULD-FIX · blocks neither.**

Wording defect confirmed. The guard is scoped to "`specs/**/spec.md` under **the
invoking change's own directory**" (`tasks.md:235`), while the Wiring clause
(`:367-370`) says to add it to `projects/synth/Makefile`'s `test` target "the same
way `check-ui-boundary`, `check-ui-boundary-empty-discovery` and
`check-app-bundle-plist` are wired" — and those are bare, argument-less targets:

```
$ sed -n '74,82p' External/Sheaf/projects/synth/Makefile
check-ui-boundary:
	bash scripts/check_ui_boundary.sh
check-ui-boundary-empty-discovery:
	bash scripts/check_ui_boundary_empty_discovery.sh
check-app-bundle-plist:
	bash scripts/check_app_bundle_plist.sh
```

F's consequence claim — that both resolutions lose and control (d) cannot
distinguish them — is **refuted on the control's own text**. Control (d),
`tasks.md:385-392`, verbatim:

> (d) scope: remove the marker from one of the three delta files (each still
> carries a `## ADDED Requirements` section) — the script **must exit non-zero
> naming that file**, not merely report a reduced marked-file count while still
> exiting `0`; restore, exit `0`.

After the removal the marked-file scan no longer finds that file — that is what
removing the marker does — so only the completeness guard can name it. A guard
applied to nothing exits `0` and **fails control (d)**. And the repo-wide reading
is eliminated by 1.9's own "The script is green on arrival" (`:393-395`):

```
$ grep -rl '^## \(ADDED\|MODIFIED\|REMOVED\) Requirements' openspec/changes/*/specs/*/spec.md | wc -l   24
$ … same, restricted to midi-controller-resilience                                                       3
```

21 sibling delta files would be flagged on arrival. Both bad readings are caught,
and a subject that needs no argument is derivable (the change directory each marked
file sits under).

**Remedy:** replace "the invoking change's own directory" with "the change
directory each marked file sits under".

---

## BS-3 (was F-ADV2 + F-ADV13, BLOCKING in F; confirmed BLOCKING by A, B, C, E; severity dissent by D) — the gate greps are anchored more strictly than the script they certify

**Ruling: mechanism CONFIRMED · severity SHOULD-FIX · blocks neither — but the
remedy must land inside BF-3's repair, not after it.**

Mechanism confirmed on a two-line scratch fixture:

```
$ printf -- '- Check: not yet delivered; foo\n  - Check: not yet delivered; bar\n' > adj_ws.txt
$ grep -c '^- Check:' adj_ws.txt          # task 6.6's gate, and task 7.2's two guards
1
$ python3 …re.compile(r'^\s*-\s*Check:\s*(?P<body>.+)$')…   # check_spec_checks_resolve.py:93
2
```

So an indented deferred line is invisible to the gate, still read by the script,
and still counted as declared — the gate reports `0` with the undelivered claim
intact, and `openspec archive` promotes it verbatim. The same asymmetry applies to
Sheaf task 7.2's two guards, both anchored `^- Check:`, against rule 1.9's "Take
the text after `Check:`" with no whitespace statement.

D's dissent basis verified:

```
$ grep -h  '^- Check:' $D/specs/*/spec.md | wc -l                              12
$ grep -hE '^[[:space:]]*-[[:space:]]*Check:' $D/specs/*/spec.md | wc -l       12
```

All twelve sit at column 0 today, so reaching the escape requires a deliberate
re-indent during the rewrite. Nothing an honest executor does walks into it.
Blocks neither execution nor delivery.

**Why it still cannot be deferred:** once BF-3's `<M>` clause is dropped as
unsatisfiable, the `grep -c 'not yet delivered'` → 0 count is the *remaining*
guard on that task, and it is the one this escapes.

**Remedy (bind to BF-3's edit):** anchor every Check-line grep in frogg3rs task 6.6
and Sheaf task 7.2 as `^[[:space:]]*-[[:space:]]*Check:`, and have the script print
its own line count so the grep and the script cannot disagree about scope. State
in Sheaf rule 1.9 that leading whitespace is accepted.

---

# 4. SHOULD-FIX and NOTE, consolidated

Adjudicated where a single command settles it; otherwise flagged. **Verified**
means I ran the command or opened the artifact myself.

## Verified here

| id | sources | finding | blocks | evidence |
|---|---|---|---|---|
| S1 | A3 | The Launch Control XL disassembly record in `design.md` is trimmed without disclosure. | neither | **Verified.** My own 3.11 control prints `0 RESUME` before `make_slider`'s `LOAD_GLOBAL` at 2, and `2 RESUME` between the listcomp's `COPY_FREE_VARS` at 0 and `BUILD_LIST` at 4. `design.md` shows `2 LOAD_GLOBAL … SliderElement` (`:661`) and `0 COPY_FREE_VARS` / `4 BUILD_LIST` (`:671-672`) — both `RESUME` lines removed, offset gaps intact, no elision marker. One other elision *is* marked, which makes these two read as oversight. **Same class as BF-12**; fix both in one pass. Remedy: paste untrimmed, or mark each elision. |
| S2 | A4 | The fader's channel and CC rest on `SliderElement.__init__`'s argument order, which is not in the recorded disassembly. | neither | **Verified as stated; the closing evidence is OPEN** — see §5. `LIVE_CHANNEL = 8` serving as both the SysEx template byte and the fader channel is one constant in two roles, not a cross-check. Remedy: one sentence in `design.md` naming the parameter order as an unread premise. |
| S3 | A5 (folded into BF-6) | The Sheaf Impact's "the one existing binary that constructs both a real `Engine` and a real `BrowserMidiBridge`" is false. | neither | **Verified.** `grep -ln 'BrowserMidiBridge<synth::Engine'` over `tests/*.cpp` returns `browser_audio_device_tests.cpp` and `browser_runtime_contract_tests.cpp` besides the bridge tests; `Makefile:33/35/36` makes them three distinct binaries. `proposal.md:237` carries the claim; `design.md`'s four reasons do not over-claim. Remedy: drop "the one", and let BF-6 decide the host. |
| S4 | A6 | Task 4.2 moves the `External/Sheaf` gitlink; the frogg3rs Impact's file list does not name it. | neither | **Verified.** The Impact lists six entries — `app/FroggersMidiCatalog.hpp`, the two test files, `app/Makefile`, `MANUAL.md`, `README.md`, the promoted spec. No gitlink entry. |
| S5 | A7 | `proposal.md:9` and `design.md:4` cite `app/FroggersMidiCatalog.hpp:14-18` for three preconditions whose third ends at `:19`. | neither | **Verified.** `sed -n '14,19p'` shows the third precondition ("Bank Side Buttons unchecked … whatever Twister bank is lit") ending on `:19`; `tasks.md:488` spells it `:14-19` correctly, so the two disagree. |
| S6 | B4 | Task 1.1 says `HeldButton` "is expected to report zero everywhere in this repository"; a literal repo-wide grep is not zero. | neither | **Verified, count corrected.** `grep -rn 'HeldButton'` over Sheaf returns **0** hits outside the change's own artifacts and 3 inside them (`tasks.md:12`, `:16`, `design.md:8`), not B's 4. The intent is clear from `design.md:8`; the wording is what needs scoping to code. |
| S7 | B7 / E-ADV15 | `SystemButtonMidiInProcessor` is `final`, so the "subclass" half of task 4.6's positive control (2) does not compile. | neither | **Verified.** `MidiController.hpp:387` `class SystemButtonMidiInProcessor final : public MidiInProcessor`. Task 4.6 says "add a test-local **subclass or wrapper**" — the wrapper route survives, so the control is constructible. Remedy: drop "subclass or", note the `final`. |
| S8 | B9 | Sheaf task 1.9 tells the executor to add a marker line that is already present. | neither | **Verified.** All three delta files carry `<!-- check-lines-resolve -->` on line 3. Idempotent; control (d) still works. |
| S9 | B10 | frogg3rs 4.1 step 2 silently depends on Sheaf's task 7.7 archive having been blocked. | neither today | **Verified.** `git -C External/Sheaf show HEAD:openspec/changes/midi-controller-resilience/tasks.md` resolves; `grep -c 'smi-14\|smi-16' External/Sheaf/openspec/specs/synth-midi-instrument/spec.md` → `0`, so the archive is blocked as measured and the coupling is benign. It would block execution if 7.7 ever ran first. Remedy: one sentence in 4.1 step 2. |
| S10 | C2 | Task 3.7 claims `browser_midi_bridge_tests.cpp` "already constructs a real `synth::Engine<App>` with a **settable** timestamp lambda". False. | neither | **Verified.** The file's only such construction is `browser_midi_bridge_tests.cpp:468` `RealEngine engine([] { return std::uint64_t{10'000}; });` — empty capture, fixed return. Bears directly on BF-6 and on the ceiling cases 3.7 prescribes. |
| S11 | C4 | Task 1.9's own enumeration recipe is case-sensitive and under-reports the tasks it binds. | neither | **Verified.** My case-insensitive enumeration (BF-5's run) finds `Task 3.6` cited on `specs/synth-midi-instrument/spec.md:97`, which the task's own `grep -oE 'task[s]? [0-9]+\.[0-9]+'` misses. Remedy: add `-i`; add the rewrite-before-ticking sentence to task 3.6. |
| S12 | C5 | Task 1.9 states a line-count total that matches nothing it measures. | neither | **Verified.** `grep -c '^- Check:' specs/*/spec.md` → 18 / 10 / 3, sum **31**; `tasks.md:354` reads "totals whose sum is the 29 above", with no 29 stated above it. Remedy: 29 → 31, or delete the clause. |
| S13 | C6 | Rule 3 names `apps/miniapp/Makefile`, which does not exist. | neither | **Verified.** `ls apps/miniapp/Makefile` → no such file; `projects/synth/apps/miniapp/Makefile` exists. `tasks.md:98` and `:279` use the short spelling; `:858` uses the full one. |
| S14 | C7 | Task 5.1 cites `grep -n 'IsSysEx' src/MidiController.cpp` as naming a definition; that command's output is empty. | neither | **Verified.** The grep exits 1 with no output; `IsSysEx` appears once in `projects/synth/`, at `include/synth/MidiController.hpp:67`. The substantive conclusion is if anything stronger than stated. |
| S15 | D3 | Two Sheaf doc comments state the exact rationale the change inverts; `design.md` requires the rewrite and **no task assigns it**. | neither | **Verified.** `MidiController.hpp:243-247` ("Release clears held and drilled so each knob is a plain knob again") and `:254-255` ("set by a Shift button's press, cleared by its release"). Sheaf `design.md:355-366` names both and says the rewrite is required "not as a later hygiene pass". `grep -ci 'comment'` over Sheaf `tasks.md` → **1**, at `:899`, unrelated. The `:249-258` Impact range D cites I could not locate; that sub-claim is **unverified**. |
| S16 | D5 | Task 5.5 says "name both" where a third case also consumes the catalogue. | neither | **Verified, and narrowed.** `launchpad_presets_pair_with_the_port_names_a_host_reports` does build a catalogue-derived `registry`, but asserts against a local `kPortCount` array literal, not the catalogue count — and `measure-a2-report.md:148` records it **passing** with the seventh default present. A wording point only. |
| S17 | E-ADV5 | Task 4.2's re-run of `check_citations_resolve.py` cannot re-check the citations 4.2 is worried about. | neither | **Verified in part.** The script scans `walk_sources(app_dir, SCAN_EXT)` with `SCAN_EXT = (".cpp", ".hpp")` (`:64-65`, `:124`) — it never reads `openspec/`, so it cannot re-check `design.md`'s and task 4.6's citations after the pin advance. Task 4.2 already mandates a by-hand re-check, so the residual is the attributed property. The line-number half **not adjudicated**. |
| S18 | E-ADV7 / F-ADV18 | The `NOTE` exemption in `check-modified-requirements-restate-promoted` is pre-authorised, so a second undeclared scenario drop would also pass. | neither | **Gate output verified; exemption mechanism not adjudicated.** `python3 app/check_modified_requirements_restate_promoted.py app` → one `NOTE` about the dropped promoted scenario `'A missing release leaves Shift held until the next press and release'` and its 4 clauses, then `OK - 1 MODIFIED requirement(s), 7 promoted clause(s) …`, **rc=0**. Verify the leak at the point of repair. |
| S19 | F-ADV19 | `openspec validate --strict` is not a backstop for any finding above. | neither | **Verified.** Both changes validate clean at the audit HEAD: `Change 'frogg3rs-midi-preset-preconditions' is valid` (rc 0) and `Change 'midi-controller-resilience' is valid` (rc 0). |
| S20 | B12 | Sheaf task 1.5's `npm ci` needs network; `node_modules` is absent. | resolved | **Resolved by operator ruling 3** — `npm ci` under the Sheaf browser directory is approved. The residual is that task 3.6 re-requires it, so the dependency appears twice. |
| S21 | B11 | Tasks needing an operator, a device, or a step outside this cycle (5.4, 1.2, 3.2, 6.5 / Sheaf 7.4, Sheaf 7.7, 7.8/7.9, 2.1). | disclosed | **Not adjudicated individually**; each is disclosed in the artifacts and several are settled by operator rulings 2 and 4. Sheaf 7.7's blocked archive is measured under S9. |

## Not adjudicated — verify at the point of repair

Recorded with sources so the author can pick them up; none was confirmed or
refuted here, and none was reported as blocking after the exchange.

- **B5** (folded into BS-1), **B6** (task 4.6's positive control edits a Sheaf file
  Sheaf's own change is editing, with no stated ordering; the remedy is one
  sentence naming when 4.6 may run), **B8** (stale literals an executor measures as
  mismatches).
- **C8** (rule 3 resolves a multi-citation line on one of its citations — bounds
  what the gate promises; adjacent to BF-5).
- **D2** (the main-checkout row's disposition command is path-scoped to
  `MANUAL.md`/`QUICK_DICT.md` and blind to a sibling change's `app/` edits),
  **D6** (`rework-controllers-block-editing`'s disposition is scoped by a
  characterisation, not a path, and its open tasks edit `ControllersPageUI.hpp`).
  Both are the **same class as BF-7** — a disposition scoped by something other
  than a path — and should be repaired together with it.
- **E-ADV6** (cross-scenario bullet satisfaction in
  `check-modified-requirements-restate-promoted`, provable by a single-line edit),
  **E-ADV8** (gate step 2's vacuous `0` on a missing path; step 4's one-of-three),
  **E-ADV9 / F-ADV8** (recovery-half rule 2 rests on one literal the rewrite
  necessarily replaces), **E-ADV11/12 / F-ADV21** (drift half: id-keyed markers with
  no section affinity; both positive controls on the one device with content; control
  (a) does not delete the emitter before rebuilding), **E-ADV13 / F-ADV16** (the
  rate-limit bound is one-sided), **E-ADV14 / F-ADV14** (rule 3's at-least-one token
  quantifier; a task number mentioned in another item's prose), **E-ADV16**
  (`design.md` is read by no gate), **E-ADV17** (the LCXL requirement has no
  rejecting mechanism this cycle — dispositioned by operator ruling 1).
- **F-ADV9** (rules 1 and 3 are phrase-presence rules satisfiable by text that says
  the opposite — the same rule BF-8 is about; repair together), **F-ADV10** (task
  4.2's post-pin citation re-check — see S17), **F-ADV15** (`ADV15` ≡ S7),
  **F-ADV20** (the runtime endpoint-open binding's check disappears silently where
  JUCE is absent).

---

# 5. OPEN items

| item | source | what settles it | needs the operator? |
|---|---|---|---|
| `SliderElement.__init__`'s positional argument order — whether the second positional is a MIDI channel and the third a CC number, on which the shipped `CC 77 / channel 8` rests. | A4 | One further recorded disassembly, same 3.11 interpreter, of `SliderElement` or the `_Framework` module that defines it, showing `co_varnames`; plus `md5` of `LaunchControlXL.pyc` recorded beside the existing blocks to pin file identity. | **Yes, or already dispositioned.** The file is under `/Applications`, which this audit and this adjudication are barred from opening. Operator ruling 1 already sequences hardware confirmation on the live site after merge (task 5.4), so this need not gate the cycle — but the design should say the premise is unread rather than corroborated (S2). |
| Whether `design.md:355-366`'s comment-rewrite obligation is meant to be carried by Sheaf task 3.1 or by a new task. | D3 / S15 | A one-line decision by whoever repairs Sheaf `tasks.md`. | No. |

**Two OPEN items the reports carried are closed here**, both by
`measure-a2-report.md`: the *cause* of BF-12's block (the run was taken; the
transcription was lossy), and B's question whether
`real_catalog_defaults_generate_and_accept_adds_through_the_view_model` passes with
an analog-only Generic seventh default (it does).

---

# 6. Coverage note

**Confirmed blocking findings reached independently by more than one report:**

| finding | reports | count |
|---|---|---|
| BF-1 (gate step 6 inert) | A1, B3, C9, E-ADV2, F-ADV4 | 5 |
| BF-3 (6.6's `<M>` unsatisfiable) | B1, E-ADV3, F-ADV1 | 3 |
| BF-2 (gate step 5) | E-ADV1 (rejection half), F-ADV5 (comment half) | 2, from different angles |
| BF-4 (6.6's rewrite names anything) | E-ADV4, F-ADV3 | 2 |
| BF-5 (rule 2(a) quantifier) | B2, C3 | 2 |
| BF-12 (recorded run block) | A2 (all three sub-defects), D4 (sub-defect c only) | 2 |

**Confirmed blocking findings reached by exactly one report:**

| finding | report |
|---|---|
| BF-6 — the browser ABI case is not constructible as specified | C |
| BF-7 — a sibling change's disposition scoped by a count the tree inverts | D |
| BF-8 — EndpointOpen's scope stated four ways | E |
| BF-9 — `sru-63`'s mapped set excludes every encoder and analog address | F |
| BF-10 — Sheaf rule 3 specifies no comment handling | F |
| BF-11 — `openspec archive -y`'s STOP condition is unperformable | F |

Half the confirmed blocking set was reached by a single auditor, and the two most
consequential — BF-9, which is the only requirement-level defect in the round and
would ship this change's own headline addition inverted, and BF-6, which makes the
browser half of the endpoint-open work unbuildable as specified — were each found
once. Convergence tracked the *cheap* findings (five auditors on a `grep -c`); the
expensive ones were found alone.

**What this adjudication did not do.** It did not build either tree, run any test
binary, construct a replica tree, or open anything under `/Applications`. Where a
report's evidence was a sandbox replica run (E's and F's `65/8`, `64/9` figures),
I confirmed the arithmetic and the script's bucketing from source and my own counts
rather than repeating the replica — exact for those aggregate questions, and it
would not catch an arithmetic slip inside their replica runs. SHOULD-FIX and NOTE
findings were adjudicated selectively, as marked in §4.
