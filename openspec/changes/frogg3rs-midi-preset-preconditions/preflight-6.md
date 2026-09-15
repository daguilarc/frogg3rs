# Preflight 6 — adjudication

Adjudicator context. I did not write the change, the briefs, or any report. For
every finding below I opened the artifact or ran the command myself; the literal
output is quoted. Agreement between reporters is recorded in the coverage note
at the end and is not evidence here. Where several reports found one defect they
are consolidated into a single entry with its source ids and one remedy.

Read-only throughout: no edit, no commit, no stash, no install, no project
build, nothing under `/Applications` opened, no `preflight*.md` inside a change
directory read. Commands ran in
`/Users/diegoaguilar-canabal/Desktop/frogg3rs/.claude/worktrees/midi-resilience`
(or its `External/Sheaf`) unless the path says otherwise. One throwaway Makefile
was written in the scratchpad as a positive control for GNU make semantics.

**Verdict: REJECT.** 16 confirmed blocking findings.

Counts among blocking: **CONFIRMED 16 · REFUTED 0 · OPEN 0.** Three *sub-claims*
inside otherwise-confirmed findings are refuted (recorded at each finding and
summarised in §3). Four items are genuinely open; none of them is a blocking
finding in its own right (§4).

---

## 1. Confirmed blocking findings

Severity is ruled by what breaks, per the brief: *blocks execution* = an
executor cannot proceed without inventing something; *blocks delivery* = it can
proceed but the result cannot ship.

---

### BLK-1 — The Launch Control XL fader map is stated as a read fact in one artifact and as an unperformed reading in another

**Sources:** A1, F-ADV1; E-ADV15 upgraded NOTE→BLOCKING in its exchange; B6
upgraded SHOULD-FIX→BLOCKING in its exchange.
**Severity: BLOCKING. Blocks execution (group 5 stops at 5.1).**

**What breaks.** Tasks 2.1 and 5.1 hand the executor CC 77 and channel 8 as
determined values while the change's own task text says the only source was not
opened. An executor forbidden to invent has no source the artifacts admit to
having read, and no check anywhere can reject a wrong number.

**Evidence I produced.**

```
$ awk 'NR>=42&&NR<=48{printf "%d: %s\n",NR,$0}' openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md
42:       finding stands. Per the operator's ruling, the map is read instead from
43:       the one source on this Mac that encodes it: Ableton Live 12 Suite's own
44:       control-surface script for the device (Live 12.4.3), cited by path in
45:       proposal.md and design.md, not opened or disassembled by this task's
46:       executor. That script's constants give factory template 1 and CC 77 to
47:       CC 84 across the eight faders, channel 8 counted from 0 (channel 9
48:       counted from 1). This task is satisfied.
```

```
$ awk 'NR>=201&&NR<=212{printf "%d: %s\n",NR,$0}' openspec/changes/frogg3rs-midi-preset-preconditions/design.md
201: The script's module-level constants `PREFIX_TEMPLATE_SYSEX` and
202: `LIVE_TEMPLATE_SYSEX` build exactly the `Change current template` message the
203: Programmer's Reference Guide documents: `F0 00 20 29 02 11 77 08 F7` —
205: script selects **factory template 1**. Its `LIVE_CHANNEL` constant is 8; the
210: not channel 8. The script's slider factory constructs each fader as a
211: `SliderElement` on `LIVE_CHANNEL`, and its fader list comprehension assigns CC
212: `77 + i` for `i` in `0..7` — CC 77 to CC 84, one per fader.
```

Named module-level constants, a "slider factory" and a "fader list
comprehension" are source-level structures; the cited file is a `.pyc`. No
artifact attributes the reading to anyone or carries the output of the command
that performed it — the only three hits for an attribution are the version
string, not the read:

```
$ grep -rn "disassembl\|marshal\|python3 -m dis\|the lead\|I read\|read by\|Info.plist" \
    openspec/changes/frogg3rs-midi-preset-preconditions/{proposal,design,tasks}.md
tasks.md:45:      proposal.md and design.md, not opened or disassembled by this task's
proposal.md:84:read from the application's own `Info.plist`). That script's module-level
design.md:195:the application's own `Info.plist`, `CFBundleShortVersionString`). This is
```

The numbers are the behaviour — dispatch is a bare address equality, so a wrong
CC makes the preset silently inert rather than wrong-looking:

```
$ awk 'NR>=846&&NR<=848{printf "%d: %s\n",NR,$0}' External/Sheaf/projects/synth/src/MidiController.cpp
846:     const MidiControlAddress address{.channel = midi.Channel(), .cc = midi.GetCC()};
847:     if (config_.sceneBlend.has_value() && *config_.sceneBlend == address) {
848:         Push(MessageIn::SetSceneBlend(NextTimestamp(), normalized));
```

**Consistency with operator ruling 1.** The ruling settles that the map ships
and that the lead read it by disassembly. This finding is squarely the part the
ruling leaves in scope: how the artifacts *record* that reading. No remedy here
removes the preset or asks for a hardware re-measurement.

**Remedy (condition on the artifacts).** The artifacts must record one
consistent account of the reading: who performed it, by what command, and with
what output, cited in design.md; and tasks.md 2.1 must not simultaneously assert
the result and deny the reading. Task 5.1's check must be described as what it
is — a literal-against-literal assertion that can catch only a later edit, not a
wrong value — and the change must say plainly that the CC numbers rest on that
one reading and on the operator's post-delivery hardware check (ruling 2).

---

### BLK-2 — The Launch Control XL device default's `id` is written nowhere

**Sources:** B1. (F narrowed the `displayName` half in its exchange; see §3.)
**Severity: BLOCKING. Blocks execution (5.1; read again by 4.5 and 5.5).**

**What breaks.** `id` is a required member, it is persisted into stored user
patches, and task 4.5's loop keys on it. Task 5.1 specifies kind, aliases and
the analog section and no identity at all.

**Evidence I produced.**

```
$ grep -rn "froggers\.\|device\.id\|displayName\|wizardId" \
    openspec/changes/frogg3rs-midi-preset-preconditions/{proposal,design,tasks}.md \
    openspec/changes/frogg3rs-midi-preset-preconditions/specs/froggers-midi-controller-mappings/spec.md
grep-exit=1                      # zero matches in all four artifacts
```

```
$ awk 'NR>=31&&NR<=38{printf "%d: %s\n",NR,$0}' External/Sheaf/projects/synth/include/synth/MidiAppCatalog.hpp
31: struct MidiAppDeviceDefault {
32:     std::string id;          // wizard id, stored in MidiControllerSlot::wizardId
33:     std::string displayName; // dropdown label
```

The convention exists but does not determine a token — `froggers.lcxl`,
`froggers.launchcontrol.xl` and `froggers.launchcontrolxl` are all consistent
with it:

```
$ grep -n 'device.id = "' app/FroggersMidiCatalog.hpp
92:    device.id = "froggers.twister";
173:    device.id = "froggers.apc40.generic";
184:    device.id = "froggers.apc40.ableton";
$ grep -n 'froggers\.launchpad' app/FroggersMidiCatalog.hpp
266:    ... "froggers.launchpad.x", "Launchpad X",
276:    ... "froggers.launchpad.promk3", "Launchpad Pro MK3",
286:    ... "froggers.launchpad.minimk3", "Launchpad Mini MK3",
```

**Remedy.** The artifacts must state the `id` literal for the new default. The
`displayName` needs no remedy (§3, R-2).

---

### BLK-3 — Sheaf task 1.5 declines to baseline the miniapp JUCE `test` target on the ground that it does not exist; it exists and runs seven binaries

**Sources:** A2, B3, C1 (three reports, independently).
**Severity: BLOCKING. Blocks execution (the baseline must precede group 3) and
delivery (task 7.2 cannot attribute what it measures).**

**What breaks.** The seven existing binaries compile the very sources group 3
edits. Meeting that target cold at 7.2 means a red among the seven is
unattributable, and the only gate covering the runtime half was never baselined.

**Evidence I produced.**

```
$ grep -n '^test:' External/Sheaf/projects/synth/apps/miniapp/Makefile
84:test: check-juce $(GEOMETRY_TEST) $(PORTABLE_BACKEND_TEST) $(MINIAPP_PARITY_TEST) $(RUNTIME_PAGES_TEST) $(FILE_PAGE_SIM_TEST) $(RUNTIME_SHELL_SESSION_TEST) $(CONTROLLERS_PAGE_SIM_TEST)
$ ls External/Sheaf/projects/synth/juce/*Tests.cpp | wc -l
       7
$ grep -n "SYNTH_SRC :=\|SYNTH_SRC +=" External/Sheaf/projects/synth/runtime/juce_build.mk
43:SYNTH_SRC := … $(SYNTH_ROOT)/src/MidiController.cpp …
45:SYNTH_SRC += $(SYNTH_ROOT)/src/MasterClock.cpp $(SYNTH_ROOT)/src/ControllerWizard.cpp
```

Task 1.5 and task 7.2, verbatim:

> 1.5 … There is no gate yet for the JUCE-linked runtime binding this change's
> task 3.6 adds (`make -C projects/synth/apps/miniapp test` does not exist yet
> with that target) — nothing to baseline for it …

> 7.2 … PLUS the two gates this change itself adds that 1.5 had nothing to
> baseline: `make -C projects/synth/apps/miniapp test` …

and task 3.6 contradicts 1.5 inside the same document, describing the target's
own pattern: "wired into `projects/synth/apps/miniapp/Makefile`'s `test` target
the same way its seven existing `juce/*Tests.cpp` files are … joining the
`test:` prerequisite list — see `apps/miniapp/Makefile:34-84`".

**Remedy.** Task 1.5 must baseline `make -C projects/synth/apps/miniapp test`
with the count and exit code of its seven existing binaries, before group 3
edits their sources. Task 7.2 must describe the change's contribution as one new
binary joining that target, not as a new gate.

---

### BLK-4 — `npm run test:unit` cannot start in this worktree, and the tree that could run it is on the wrong commit

**Sources:** B2, C2 (independently).
**Severity: BLOCKING. Blocks execution (Sheaf 3.5 makes the ABI bump
conditional on it) and delivery (1.5, 7.2).**

**What breaks.** Sheaf 3.5 says to run this gate "after the bump and before
calling this task done". It has no binary to invoke here, and the only checkout
carrying the dependency tree is on this change's branch *parent*, so running it
there exercises a tree without the change in it.

**Evidence I produced.**

```
$ find External/Sheaf -maxdepth 5 -name node_modules -type d
                                        (no output)
$ command -v tsc || echo "tsc NOT FOUND"
tsc NOT FOUND
$ python3 -c "import json;d=json.load(open('External/Sheaf/projects/synth/browser/package.json'));print('test:unit =',d['scripts']['test:unit']);print('build     =',d['scripts']['build'])"
test:unit = npm run build && node --test dist/tests/*.test.mjs
build     = tsc -p tsconfig.json
$ ls -d External/Sheaf/projects/synth/browser/dist
ls: External/Sheaf/projects/synth/browser/dist: No such file or directory

$ ls -d /Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/browser/node_modules
/Users/…/frogg3rs/External/Sheaf/projects/synth/browser/node_modules
$ ls -l  …/node_modules/.bin/tsc
lrwxr-xr-x …/.bin/tsc -> ../typescript/bin/tsc
$ git -C /Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf rev-parse --short HEAD; git -C … branch --show-current
ba3898e4
launchpad-model-on-the-row
$ git -C External/Sheaf rev-parse --short HEAD; git -C External/Sheaf branch --show-current
caae5c2e
midi-resilience-merge
```

**Remedy, stated both ways per operator ruling 4.**
*If the operator approves `npm ci` under `projects/synth/browser` in this
worktree:* tasks 1.5, 3.5 and 7.2 must name that install as a prerequisite step
with the approval recorded.
*If approval is withheld:* the artifacts must not carry a gate the change cannot
run. Task 3.5's "run `npm run test:unit` … before calling this task done" must be
replaced by a condition the executor can actually discharge, and 1.5/7.2 must
record the gate as unrunnable in this tree with the reason, rather than
baselining it. Naming the main checkout as the gate's home is not an available
remedy as written: that tree is another live session's and is on `ba3898e4`.

---

### BLK-5 — The manual-vs-declarations check has no written assertion, covers one of four devices, has no drift-half positive control, and its recovery-half assertion contradicts its own control

**Sources:** B4, F-ADV2, E-ADV9 (drift half, independently); A8 (the control
contradiction, defended in the exchange against B4's waiver); C5 (the recovery
half's Hold Drill blind spot).
**Severity: BLOCKING. Blocks execution (4.8) and delivery (6.3, 6.4).**

**What breaks.** The executor authors the declarations (4.3, 4.4, 5.2), the
paragraph (4.7) and the check (4.8), with no correspondence rule, no floor, and
a positive control that the stated assertion cannot satisfy. This is exactly the
blank the operator's corollary warns about, and it will be filled toward green.

**Evidence I produced.**

*(a) No correspondence rule and no floor.* Task 4.8's whole statement of the
drift half is "It fails when the generated manual paragraph disagrees with the
declarations". A search of both artifacts for any rule or minimum returns
nothing:

```
$ grep -ni "non-zero\|at least one device\|minimum count\|floor\|count of\|undeclared\|does not declare" \
    openspec/changes/frogg3rs-midi-preset-preconditions/{tasks,design}.md
exit=1                                   # zero matches
```

*(b) Scope is one paragraph; four devices receive declarations.* Task 4.7 names
`MANUAL.md:336-339` only. Tasks 4.3 (Twister), 4.4 (APC40 Generic and Ableton)
and 5.2 (Launch Control XL) all populate declarations. `:336-339` is the
Twister's paragraph; the APC40 Generic's Track-1 caveat is prose in another
section at `:348-351`; the Launch Control XL's section does not exist until 5.6
writes it.

*(c) The recovery-half assertion and its own positive control are mutually
unsatisfiable as written.* design.md supplies the assertion:

```
$ awk 'NR>=142&&NR<=149{printf "%d: %s\n",NR,$0}' openspec/changes/frogg3rs-midi-preset-preconditions/design.md
142: Concretely, the check requires the Shift and Hold Drill recovery paragraphs to
143: each name at least one mechanism that does not require that button's own
144: address to transmit (unplugging/reconnecting the controller, a profile
145: rebuild, or an elapsed-time ceiling), and fails when the only recovery named is
146: "pressed and released again." The positive control is running the check
147: against the **current, unmodified** `MANUAL.md:319-320` — "buttons stay shifted
148: until Shift is pressed and released again," naming no other trigger — before
149: task 3.1 touches it: that text must turn the check red.
```

The control text names the first of the three accepted mechanisms:

```
$ awk 'NR>=319&&NR<=320{printf "%d: %s\n",NR,$0}' MANUAL.md
319: with the patch like every other mapping. If the controller is unplugged while Shift is still held, its
320: buttons stay shifted until Shift is pressed and released again.
```

design.md's own paraphrase of that text — "naming no other trigger" — is false
on its face. A check implementing the stated mechanism list finds "unplugged"
and passes; only an unwritten cause-versus-recovery distinction saves the
control, and the executor is forbidden to invent it.

*(d) Hold Drill has no recovery paragraph for the check to find at control
time.* The section is `MANUAL.md:308-312` and contains no recovery sentence;
task 3.1 writes one, and 4.8's control runs *before* 3.1.

*(e) A SHALL in the delta has no check.* `spec.md:45` carries "A preset SHALL NOT
depend on a device setting it does not declare." The grep in (a) covers
"undeclared" and "does not declare" over tasks.md and returned nothing. A
comparison between declarations and text generated from them cannot detect a
dependency absent from both.

**Remedy.** Before 4.8 is executable the artifacts must carry, already written
down: the correspondence rule (what "agrees" means over opaque strings); a
non-zero floor on what the parser must find, so a regex matching nothing cannot
report OK; a named manual paragraph for each of the four devices that receive
declarations, or an explicit statement that the check's scope is the Twister's
alone and why; a positive control for the drift half (a mutated declaration must
turn it red); a recovery-half assertion and control that are satisfiable
together, including what the check does when the paragraph it looks for is
absent; and either a check for the undeclared-dependency SHALL or a statement in
the delta that it ships unchecked and why.

---

### BLK-6 — Group 1 baselines the frogg3rs gates before the submodule pin advance; group 6 re-runs them after it and pre-fills the answer

**Sources:** B5.
**Severity: BLOCKING. Blocks delivery (6.3 cannot attribute what it measures).**

**What breaks.** Every frogg3rs test binary links a library rebuilt from the
submodule, so task 4.2's pin advance changes the inputs of all twelve. Task 6.3
compares across that advance with the conclusion already written.

**Evidence I produced.**

```
$ grep -n "SHEAF" app/Makefile | head -8
18:SHEAF_SYNTH_DIR := $(APP_DIR)/../External/Sheaf/projects/synth
22:CPPFLAGS := -I$(SHEAF_SYNTH_DIR)/include
29:SHEAF_LIB := $(SHEAF_SYNTH_DIR)/build/libsynth.a
38:TEST_CPPFLAGS := $(CPPFLAGS) -I$(SHEAF_SYNTH_DIR)/tests
160:$(SHEAF_LIB): force
161:	$(MAKE) -C $(SHEAF_SYNTH_DIR) build
167:$(APP): $(APP_SOURCES) $(SHEAF_LIB) | $(BUILD_DIR)
$ sed -n '248p' app/Makefile
$(TEST_BIN): $(TEST_SOURCES) … $(DSP_HEADERS) $(SHEAF_LIB) | $(BUILD_DIR)

$ grep -ni "baseline" openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md
13:- [ ] 1.4 Baseline every gate `app/Makefile`'s `test:` target runs, read from
23:      Also baseline `./app/build-launcher.sh` and the twelve test binaries the
198:- [ ] 6.3 Re-run every gate baselined in 1.4, naming which moved and which were
```

Three hits, no fourth: nothing re-baselines between 4.2 and 6.3. And 6.3's
written assertion is "This change adds one target
(`check-docs-match-device-preconditions`) and should move none of the other
nine" — a blank filled toward green, and the count is wrong. There are ten
`check-*` prerequisites today, which the change's own Impact and task 4.8 both
say:

```
$ sed -n '320p' app/Makefile | tr ' ' '\n' | grep -c "^check-"
10
```
> Impact: "a new `check-docs-match-device-preconditions` target joins the **ten**
> `check-*` prerequisites `test:` already runs"
> Task 4.8: "Wire it into `app/Makefile` beside the **ten** existing `check-*` targets."

**Remedy.** The gates must be re-baselined immediately after 4.2, so 6.3
compares like with like; and 6.3 must carry no pre-written expectation of the
result — it records what moved. Its comparison set must be the ten, not nine.

---

### BLK-7 — The tenth gate is red in this worktree, so `make -C app test` runs no test binary; five of the fifteen dangling names are this change's own removal and eight are branch lag

**Sources:** D1; E-ADV8 (the recipe-stops half, independently); D2 (the branch
lag half).
**Severity: BLOCKING. Blocks delivery (`make test` is unusable as this change's
own green gate, and 6.3 plans to certify the reds forward).**

**Evidence I produced.**

```
$ python3 app/check_artifact_symbols_resolve.py app 2>&1 | tail -1
check-artifact-symbols-resolve: FAIL - 15 unresolvable name(s) in change artifacts
$ python3 app/check_artifact_symbols_resolve.py app 2>&1 | grep -o '`openspec/changes/[a-z0-9-]*/' | sort | uniq -c
   2 `openspec/changes/archive/
   5 `openspec/changes/frogg3rs-midi-controller-resilience/
   8 `openspec/changes/frogg3rs-randomize-depth-reclaim/
$ sed -n '320p' app/Makefile | tr ' ' '\n' | grep -n "check-\|_BIN" | sed -n '10,13p'
11:check-artifact-symbols-resolve
12:$(TEST_BIN)
13:$(MONO_VALIDATION_BIN)
```

Make semantics settled by my own positive control rather than by reasoning
(scratchpad throwaway, structurally `test: c1 c10 bin1` with `c10` failing):

```
$ make test
c1 ok
c10 running
make: *** [c10] Error 1
EXIT=2
$ make -j2 test
c10 running
c1 ok
make: *** [c10] Error 1
make: *** Waiting for unfinished jobs....
EXIT=2
$ make --version | head -1
GNU Make 3.81
```

"BINARY BUILT AND RUN" and "TEST RECIPE RAN" printed in neither run. So while the
tenth check is red, `make -C app test` links and runs none of the twelve
binaries — including the two this change edits.

Attribution, both halves:

```
$ git log -1 --format='%H %s'
6e771427ee6d8a917d5cd508c1d661563b7382fc Collect the MIDI resilience work into one worktree
$ git show --no-patch --format='%B' HEAD | sed -n '13,16p'
They supersede an app-level change of the same name written on 2026-09-10,
which carried both halves and pointed almost entirely into the submodule. It
reached 1 of 42 tasks, was never committed anywhere, and is removed.
$ find /Users/diegoaguilar-canabal/Desktop/frogg3rs -maxdepth 7 -type d -name 'frogg3rs-midi-controller-resilience'
(no output)
$ git log --all --oneline -- 'openspec/changes/frogg3rs-midi-controller-resilience'
(no output — never tracked, so no diff can show the removal; the commit message is the evidence)

$ git rev-list --left-right --count main...HEAD
5	1
$ git ls-tree -d --name-only main openspec/changes/ | grep randomize
openspec/changes/frogg3rs-randomize-depth-reclaim
$ ls -d openspec/changes/frogg3rs-randomize-depth-reclaim
ls: openspec/changes/frogg3rs-randomize-depth-reclaim: No such file or directory
```

So the fifteen decompose cleanly: 5 dangle because this branch's own HEAD commit
removed their target; 8 dangle because this branch is five commits behind `main`
and the directory they cite exists there and not here; 2 are an archived research
file. Task 1.4's "none inside this change's directory" is true of location and
silent on cause for all thirteen.

**Remedy.** The artifacts must state the cause of each of the fifteen, not only
their location: the five inbound references to the removed pre-split change are
this change's own §8.0 obligation and must be repaired inside it; the eight must
be recorded as resolved by bringing the branch up to `main`, with the resolution
owned by a task. Task 6.3 must not certify the fifteen forward as somebody
else's. Until the gate is green, no artifact may describe `make -C app test` as
this change's green gate; any task that relies on the binaries must run each by
path and say so.

---

### BLK-8 — The catalogue header comment and the page-test header comment enumerate six device defaults by name; neither is in any Impact

**Sources:** D4, A6, C4, F-ADV12 (four reports, independently).
**Severity: ruled BLOCKING, blocks delivery. Severity was disputed — D graded it
BLOCKING, A, B, C and F graded it SHOULD-FIX.**

**Ruling on the dispute.** It stops no step and turns no check red, so it does
not block execution. It ships a file whose own header says six and enumerates
six, in a change whose whole subject is that labels must match function — the
WYSIWYG class this pair exists to fix. That cannot ship. **Blocks delivery.**

**Evidence I produced.**

```
$ awk 'NR>=6&&NR<=12{printf "%d: %s\n",NR,$0}' app/FroggersMidiCatalog.hpp
 6: // around for this app (parameter inc/dec, absolute set, push, scene blend,
 7: // hold drill, shift), and the six device defaults offered from the
 8: // Controllers page's Layout dropdown -- MIDI Fighter Twister, Akai APC40
 9: // mkII (Generic), Akai APC40 mkII (Ableton), Launchpad X, Launchpad Pro
10: // MK3, and Launchpad Mini MK3. Choosing one of the six installs its
11: // mappings onto the selected slot; Custom leaves the slot's mappings
12: // untouched and editable by hand.

$ awk 'NR>=1&&NR<=4{printf "%d: %s\n",NR,$0}' app/FroggersControllersPageTests.cpp
 1: // FroggersControllersPageTests.cpp -- drives the Controllers page's view
 2: // model (synth::MidiConfigViewModel) against synth_froggers::
 3: // FroggersMidiCatalog()'s six real device defaults (the MIDI Fighter
 4: // Twister, both APC40 mkII variants, and the three Launchpad models).
```

The Impact names, in `FroggersMidiCatalog.hpp`, only `:14-18`, `:19-24`,
`:26-35`, `:165-166` and the new function; it names
`FroggersControllersPageTests.cpp` as a file but only its count assertions at
`:99` and `:109`. Task 5.5 likewise names only `:99` and `:109`.

**Remedy.** Both header comments must be named in the Impact and owned by a
task, with the count and the enumeration updated to seven.

---

### BLK-9 — `MANUAL.md:271-272` enumerates three device families as what the configure flow offers; the seventh default joins it

**Sources:** D5.
**Severity: ruled BLOCKING, blocks delivery. Severity was disputed — D graded it
BLOCKING, A graded it SHOULD-FIX; B, C, E and F confirmed without dissent.**

**Ruling on the dispute.** A's precision is right on the language — the sentence
carries no "only", so it becomes an incomplete enumeration reading as
exhaustive rather than a false statement. That is the same class as BLK-8 and
gets the same grade: it stops no step, and it cannot ship. **Blocks delivery.**

**Evidence I produced.**

```
$ awk 'NR>=271&&NR<=272{printf "%d: %s\n",NR,$0}' MANUAL.md
271: without renaming the row, changing its ports, or releasing it. A newly connected Twister, APC40, or
272: Launchpad is also offered through the page's configure flow.
```

The configure flow's list is built from the catalogue, one descriptor per device
default, and discovery iterates every descriptor:

```
$ grep -rn "MakeControllerWizardRegistry" External/Sheaf/projects/synth/runtime External/Sheaf/projects/synth/include/synth/browser
runtime/JuceRuntimeMainServices.hpp:79:        callbacks.layouts = synth::MakeControllerWizardRegistry(runtime_.GetEngine().MidiCatalog());
include/synth/browser/BrowserRuntimeMainServices.hpp:83:        callbacks.layouts = synth::MakeControllerWizardRegistry(engine_.MidiCatalog());
$ sed -n '937,940p;962,963p' External/Sheaf/projects/synth/src/ControllerWizard.cpp
    registry.reserve(catalog.deviceDefaults.size());
    for (const MidiAppDeviceDefault& deviceDefault : catalog.deviceDefaults) {
        registry.push_back(ControllerWizardDescriptor{
    for (const ControllerWizardDescriptor& descriptor : registry) {
```

and the app's own test already pins the relation, at a line task 5.5 is editing:

```
$ awk 'NR>=97&&NR<=109{printf "%d: %s\n",NR,$0}' app/FroggersControllersPageTests.cpp
 97: TEST_CASE(real_catalog_registers_one_descriptor_per_device_default) {
108:     REQUIRE_TRUE(registry.size() == catalog.deviceDefaults.size());
109:     REQUIRE_TRUE(registry.size() == 6);
```

The Impact's `MANUAL.md` bullet names `:319-320`, `:308-313`, `:336-339`,
`:341-351`, `:353-357`, the Overview Preset list and a new section — not `:271`.

**Remedy.** `MANUAL.md:271-272` must be named in the Impact and owned by a task,
and must stop enumerating device families where the set is open.

---

### BLK-10 — Two untracked divergent duplicates answer to these change names; one is dispositioned in prose by no task, the other is unenumerated

**Sources:** D7 (the Sheaf duplicate, alone); B10, A10, E-ADV13, F-ADV13 (the
frogg3rs duplicate).
**Severity: ruled BLOCKING, blocks execution. Severity was disputed — D graded
BLOCKING, F SHOULD-FIX/blocks execution, B SHOULD-FIX, E NOTE/blocks delivery.**

**Ruling on the dispute.** The change's own design.md says the duplicate "is
deleted before either executes". No task performs it, and the file lives in
another live session's worktree where this session may not write. An executor
reaching group 1 has a stated precondition it cannot discharge. **Blocks
execution.**

**Evidence I produced.**

```
$ git worktree list
/Users/…/frogg3rs                                        7425fe8 [main]
/Users/…/frogg3rs/.claude/worktrees/midi-controller-resilience  9838862 [worktree-midi-controller-resilience]
/Users/…/frogg3rs/.claude/worktrees/midi-resilience             6e77142 [worktree-midi-resilience]
/Users/…/frogg3rs/.claude/worktrees/randomize-depth-reclaim     db65167 [worktree-randomize-depth-reclaim]

$ git -C /Users/…/worktrees/midi-controller-resilience status --short
 ? External/Sheaf
?? openspec/changes/frogg3rs-midi-preset-preconditions/
$ git -C /Users/…/worktrees/midi-controller-resilience/External/Sheaf status --short
?? openspec/changes/midi-controller-resilience/
```

The two Sheaf copies differ materially:

```
$ ls External/Sheaf/openspec/changes/midi-controller-resilience/specs/
synth-controller-wizards
synth-midi-instrument
synth-runtime-ui
$ ls /Users/…/worktrees/midi-controller-resilience/External/Sheaf/openspec/changes/midi-controller-resilience/specs/
synth-midi-instrument
synth-runtime-ui

$ grep -rn "scw-6" <sibling copy> --include='*.md' | grep -v preflight | wc -l
       0
$ grep -rn "declaredPreconditions" <sibling copy> --include='*.md' | grep -v preflight | wc -l
       0
$ grep -rn "scw-6" <this copy> --include='*.md' | grep -v preflight | wc -l
       7
$ grep -rn "declaredPreconditions" <this copy> --include='*.md' | grep -v preflight | wc -l
      16
```

Disposition exists for one duplicate only, in prose:

```
$ awk 'NR>=249&&NR<=253{printf "%d: %s\n",NR,$0}' openspec/changes/frogg3rs-midi-preset-preconditions/design.md
249: - **A divergent duplicate of this change exists.** An untracked copy of
250:   `frogg3rs-midi-preset-preconditions` sits in the `midi-controller-resilience`
251:   worktree, deltaing the same capability with different text in all four
252:   files. → This worktree's copy is authoritative; the other is deleted before
253:   either executes.
```

No task performs any deletion (the single grep hit for "delete" in either
tasks.md is tasks.md:95, about a comment kept rather than deleted), and the
Sheaf-side duplicate is named in no Sheaf artifact at all.

**Refuted sub-claim.** D7's stated consequence — that a session executing the
other copy would satisfy frogg3rs task 4.1's gate — does not hold. 4.1 carries a
mechanical content check: "Confirm with `grep -n declaredPreconditions
External/Sheaf/projects/synth/include/synth/MidiAppCatalog.hpp` — it must return
a real member". That grep would not pass. See §3, R-3.

**Remedy.** Both duplicates must be enumerated by path — the frogg3rs change and
the Sheaf change in the sibling worktree — and their disposition must be owned
by a task that names who performs it, since neither path is writable from this
session. Task 4.1's gate must name the commit SHA that carries the field rather
than a branch name, since Sheaf task 7.7 gives its own branch name as an example
("e.g. `git push fork midi-resilience-merge`") while 4.1 pins it exactly.

---

### BLK-11 — The hold ceiling has no assertion anywhere that it does not fire early

**Sources:** E-ADV1, F-ADV4 (independently).
**Severity: ruled BLOCKING, blocks delivery. Severity was disputed — E graded
BLOCKING/blocks execution, F graded SHOULD-FIX/blocks delivery.**

**Ruling on the dispute.** The executor can proceed: 3.4 tells it what to write.
What it cannot do is ship a constant no delivered check constrains — a ceiling
of `0`, or 30 000 written where 30 000 000 is meant, satisfies every assertion
in the three deltas. **Blocks delivery**, not execution.

**Evidence I produced.**

```
$ awk 'NR>=65&&NR<=68{printf "%d: %s\n",NR,$0}' External/Sheaf/openspec/changes/midi-controller-resilience/specs/synth-midi-instrument/spec.md
65: #### Scenario: The ceiling clears a modifier no message will ever clear
66: - **WHEN** a modifier has been held longer than the hold ceiling and the pump runs
67: - **THEN** the modifier is cleared and the recorded clear trigger is the ceiling
68: - **AND** a pump that runs once per second reaches the same conclusion at the same elapsed time as one that runs thirty times per second

$ grep -rniE "still held|remains held|does not clear|not cleared|survives" \
    External/Sheaf/openspec/changes/midi-controller-resilience/specs/
…/synth-midi-instrument/spec.md:73:- **THEN** the other remains held
```

One hit, and it is the cross-clear scenario at `:71-74` — about *which* modifier,
not about *when*. Both clauses of the ceiling scenario are satisfied by a ceiling
of zero.

**Remedy.** The delta must carry a scenario asserting that a modifier held for an
ordinary gesture survives a pump below the ceiling, with a check that drives the
Engine over many ticks so a count-based implementation fails it too.

---

### BLK-12 — Task 3.4's ceiling comparison has no `held` term, so it stamps `Ceiling` on every never-held modifier from thirty seconds of host uptime

**Sources:** E-ADV2 (alone).
**Severity: BLOCKING. Blocks execution.**

**What breaks.** The instruction, followed literally by an executor forbidden to
invent, produces a loop that writes `held = false` and `lastClear = Ceiling`
unconditionally and forever on every running host — destroying, on the first
post-30-second tick, the `Rebuild` stamp task 3.1 in the same group goes out of
its way to assign explicitly, and falsifying smi-16's rebuild scenario in the
field. The two tasks defeat each other.

**Evidence I produced.**

```
$ sed -n '/- \[ \] 3\.4/,/- \[ \] 3\.5/p' External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md
… For every other slot, read `const std::uint64_t now
      = timestampProvider_();` (the Engine's own member, `Engine.hpp:1462` —
      never a processor's `NextTimestamp()`) once per tick, and clear once
      `now > heldSinceMicros && now - heldSinceMicros > kHeldModifierCeilingMicros`
      — never a bare subtraction — recording `Ceiling`. …

$ grep -n 'heldSinceMicros{0}' External/Sheaf/openspec/changes/midi-controller-resilience/design.md
136:held{false}; std::atomic<std::uint64_t> heldSinceMicros{0};

$ awk 'NR>=102&&NR<=106{printf "%d: %s\n",NR,$0}' External/Sheaf/projects/synth/runtime/Runtime.hpp
102:     Runtime()
103:         : startTime_(std::chrono::steady_clock::now())
105:         , engine_([this]() -> std::uint64_t { return NowMicros(); })
$ awk 'NR>=955&&NR<=959{printf "%d: %s\n",NR,$0}' External/Sheaf/projects/synth/runtime/Runtime.hpp
955:     std::uint64_t NowMicros() const {
957:             std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTime_)
```

No `held` term in the condition; `heldSinceMicros` defaults to `0`; the Engine's
provider is elapsed-since-start and climbs. From 30 s of uptime,
`now > 0 && now - 0 > 30e6` is true on every tick for every non-blacklisted slot
whose modifier was never held.

Task 3.1, which the loop then defeats:

```
$ sed -n '/- \[ \] 3\.1/,/- \[ \] 3\.2/p' …/tasks.md
… At every site that freshly constructs this state as part of a profile rebuild
      … explicitly assign `lastClear = HeldModifierClearSource::Rebuild` after
      construction; do not rely on the type's own default for the Rebuild trigger …
```

**Not settled by me.** Neither I nor any reporter ran the one experiment that
would settle this beyond argument — constructing an `Engine` with a provider
returning a value above 30 000 000, ticking once with no modifier ever held, and
printing `lastClear`. That requires compiling, which this audit may not do. The
artifact defect stands on the text of 3.4 and the clock alone; see §4, O-4.

**Remedy.** Task 3.4's condition must carry a `held` precondition, and design.md
must carry the same condition in the same form. The delta must carry a scenario
asserting that a modifier that was never held has `lastClear == Rebuild` after
an arbitrary number of post-ceiling ticks.

---

### BLK-13 — The browser endpoint-open trigger is checked in two halves that cannot fail together

**Sources:** E-ADV3 (alone; C7 is an adjacent joint of the same check).
**Severity: BLOCKING. Blocks delivery.**

**What breaks.** Task 3.5 adds a new browser ABI entry point. The join between
the C++ export and the shipped wasm is a hand-maintained literal array consumed
only by an emcc flag; the node-side case asserts against a duck-typed test
double. A new export that is present in `BrowserRuntimeAbi.cpp`, called from
`midi.ts`, and missing from `EXPORTED_FUNCTIONS` passes every named check while
the shipped wasm lacks the symbol.

**Evidence I produced.**

```
$ grep -rn "EXPORTED_FUNCTIONS" External/Sheaf/projects/synth/browser/src External/Sheaf/projects/synth/browser/tests
src/build-browser-apps.mjs:25:const EXPORTED_FUNCTIONS = [
src/build-browser-apps.mjs:67:    `-sEXPORTED_FUNCTIONS=${JSON.stringify(EXPORTED_FUNCTIONS)}`,
```

Zero hits under `tests/`. The mitigation design.md names is version-shaped only:

```
$ grep -n '^test(' External/Sheaf/projects/synth/browser/tests/version-drift.test.mjs
50:test("the C++ version accessors return what protocol.ts defines", …
77:test("the C++ contract test asserts the same ABI version", …
92:test("the pending-audio-request sentinels agree across the ABI boundary", …
124:test("no fixture declares an ABI version the definition does not", …
164:test("no test double stubs a version the definition does not", …
```

and the facade already declares nine exports optional, so a missing symbol
degrades silently:

```
$ grep -c '_synth_browser_[a-z_]*?(' External/Sheaf/projects/synth/browser/src/worker.ts
9
```

**Remedy.** Task 3.5 must name a check that fails when a called entry point is
absent from `EXPORTED_FUNCTIONS` — a derivation of the list from the C++, not a
second hand-maintained list — and the node-level case must assert against the
real facade rather than a literal supplied by the test. If BLK-4 leaves the node
gate unrunnable here, the artifacts must say so rather than cite it as the
mitigation.

---

### BLK-14 — The unshifted-usability requirement is false for the case that motivates the change, and its scenario is green either way

**Sources:** E-ADV6; A7 (the same ground, less sharply).
**Severity: ruled BLOCKING, blocks execution. Severity was disputed — E graded
BLOCKING, A graded SHOULD-FIX/blocks delivery.**

**Ruling on the dispute.** A delta scenario that is green over a false SHALL is
not something an executor can implement — task 4.6 is told to assert the
scenario, and doing so certifies the failure the change exists to prevent.
**Blocks execution.**

**Evidence I produced.**

```
$ sed -n '13p' openspec/changes/frogg3rs-midi-preset-preconditions/specs/froggers-midi-controller-mappings/spec.md
… The preset SHALL declare CC Hold on every side button as a device precondition, because the button's
release is the promptest of the triggers that end a held modifier, and SHALL remain usable in its
unshifted form when that precondition is unmet. …
$ awk 'NR>=31&&NR<=33{printf "%d: %s\n",NR,$0}' …/spec.md
31: #### Scenario: A side button keeps its ordinary job when CC Hold is unmet
32: - **WHEN** a Twister side button's press message is received and no matching release message ever arrives (the shape an unmet CC Hold precondition produces on the device)
33: - **THEN** the button's ordinary or shifted job still fires on that press
```

The same requirement maps CC 13 to Shift. The code decides the case:

```
$ awk 'NR>=964&&NR<=977{printf "%d: %s\n",NR,$0}' External/Sheaf/projects/synth/src/MidiController.cpp
964:     if (association->press.type == MessageIn::Type::Shift) {
968:         if (shift_ != nullptr) {
969:             shift_->held = isPress;
970:         }
971:         return;
972:     }
974:     if (isPress) {
975:         const bool shifted = shift_ != nullptr && shift_->held && association->shiftedPress.has_value();
976:         PushStamped(shifted ? *association->shiftedPress : association->press);
```

When the unmet precondition is on the Shift button itself, `:969` leaves
`held == true` with no release ever arriving, and every paired side button then
fires its *shifted* job. "Remain usable in its **unshifted** form" is false in
exactly that case, while the scenario's "ordinary **or** shifted" is true.

The WHEN's parenthetical "(the shape an unmet CC Hold precondition produces on
the device)" is an unsourced hardware claim: no artifact in either change states
what a Twister side button transmits outside CC Hold, and the alternative — a
toggle sending 127 then 0 — is read as a press/release pair by this very
processor (`MidiController.cpp:945-946`).

**Remedy.** The scenario's THEN must name one outcome, not a disjunction that
spans the guarantee and its failure, and must cover the case where the unmet
precondition is on the Shift button. Either the requirement sentence is
qualified to what the code actually guarantees, or a task delivers the behaviour
it asserts. The parenthetical hardware claim must be sourced or removed.

---

### BLK-15 — smi-16's independence sentence is false for Rebuild and for Ceiling, and the scenario testing it is green by picking Release

**Sources:** F-ADV3 (alone).
**Severity: BLOCKING. Blocks execution.**

**What breaks.** One sentence of the delta forbids a trigger from clearing both
modifiers and the next sentence lists profile rebuild as a trigger. Rebuild
necessarily clears both. The executor is handed a self-contradictory requirement
and a scenario that lets it pick the one trigger under which the contradiction
does not show.

**Evidence I produced.**

```
$ awk 'NR==37' …/specs/synth-midi-instrument/spec.md
… Shift and Hold Drill SHALL be independent: either may be held while the other is, and a trigger that
clears one SHALL NOT clear the other. THE synth system SHALL clear a held modifier on each of five
triggers, recording which one did so: the release of the button that set it; the rebuild of that
profile; the opening or reopening of that controller's input or output endpoint …; a second press of
the same address …; and … the elapsed monotonic time since the hold began exceeding the hold ceiling. …
```

One construction site replaces both states:

```
$ grep -rn "make_unique<ShiftState>\|make_unique<HoldDrillState>" External/Sheaf/projects/synth/
projects/synth/src/MidiController.cpp:3026:    result.holdDrill = std::make_unique<HoldDrillState>();
projects/synth/src/MidiController.cpp:3028:    result.shift = std::make_unique<ShiftState>();
$ awk 'NR>=3019&&NR<=3029{printf "%d: %s\n",NR,$0}' External/Sheaf/projects/synth/src/MidiController.cpp
3019: MidiControllerProfileResult CreateMidiControllerProfileImpl(
3025:     MidiControllerProfileResult result;
3026:     result.holdDrill = std::make_unique<HoldDrillState>();
3028:     result.shift = std::make_unique<ShiftState>();
```

Ceiling has the same shape: task 3.4 walks a `MidiControllerProfileResult&`, and
both modifiers hang off that one object. Endpoint open is undetermined — task
3.5's last bullet says "so it clears **the** modifier for that controller index",
singular, with no statement of which.

The scenario is satisfiable with zero production change:

```
$ awk 'NR>=71&&NR<=74{printf "%d: %s\n",NR,$0}' …/specs/synth-midi-instrument/spec.md
71: #### Scenario: Clearing one modifier leaves the other held
72: - **WHEN** Shift and Hold Drill are both held and one is cleared by any trigger
73: - **THEN** the other remains held
74: - Check: not yet delivered; added by this change as `instrument_tests.cpp: ModifierTriggersDoNotCrossClear`
```

Release already separates the two today — two disjoint per-address branches at
`MidiController.cpp:948-972` — and the same delta states four lines earlier that
`instrument_tests.cpp` "never constructs or calls" `Engine::MessageThreadTick()`,
so the Ceiling trigger cannot be exercised in that binary at all.

**Remedy.** The independence sentence must be scoped to the triggers for which
it is true (address-scoped clears), and the profile-scoped triggers must be
stated as clearing both. The scenario must be split by trigger class, with the
profile-scoped case asserting that both clear, and must not leave the trigger to
the implementer's choice.

---

### BLK-16 — Task 3.1 orders the `Rebuild` stamp at four construction sites; three of the four construct nothing, and task 1.7 says so

**Sources:** A3, B7. **Both reporters graded this SHOULD-FIX while describing it
as blocking execution.** I rule by what breaks: **blocks execution (3.1).**

**What breaks.** At `:3307`, `:3335` and `:3392` there is no constructed state to
assign to — each is a one-expression `return CreateMidiControllerProfile(...)`
forwarder. An executor told to "explicitly assign `lastClear = Rebuild` after
construction" at those three sites has nothing to attach the assignment to, and
the change's own task 1.7 describes them correctly as delegating.

**Evidence I produced.**

```
$ awk 'NR>=3304&&NR<=3309{printf "%d: %s\n",NR,$0}' External/Sheaf/projects/synth/src/MidiController.cpp
3304: MidiControllerProfileResult CreateWrldBldrDefaultProfile(
3307:     return CreateMidiControllerProfile(WrldBldrDefaultProfileConfig(options), bus, sender, uiState,
3308:                                        std::move(timestampProvider));
$ awk 'NR>=3332&&NR<=3337{printf "%d: %s\n",NR,$0}' …
3332: MidiControllerProfileResult CreateMfTwisterDefaultProfile(
3335:     return CreateMidiControllerProfile(MfTwisterDefaultProfileConfig(options), bus, sender, uiState,
$ awk 'NR>=3389&&NR<=3394{printf "%d: %s\n",NR,$0}' …
3389: MidiControllerProfileResult CreateLaunchpadDefaultProfile(
3392:     return CreateMidiControllerProfile(LaunchpadDefaultProfileConfig(options), bus, sender, uiState,
```

Task 1.7, in the same document:

> 1.7 Confirm `Rebuild` is already achieved by fresh construction at
> `src/MidiController.cpp:3026-3029`, and that the three factories at `:3307`,
> `:3335`, `:3392` do the same.

**Remedy.** Task 3.1 must name one assignment site — the construction inside
`CreateMidiControllerProfileImpl` — and describe the three factories as
delegating, consistent with 1.7. The design's "one line per site" rationale must
be restated on the corrected site count.

---

## 2. Findings whose severity was disputed between reports

All four are ruled inside their entries above. Summarised here so the dispute is
visible in one place.

| Finding | Reports' grades | My ruling | Ground |
|---|---|---|---|
| BLK-8 (six device defaults) | D: BLOCKING · A, B, C, F: SHOULD-FIX | **BLOCKING, blocks delivery** | Stops no step; cannot ship — it is the WYSIWYG class the pair exists to fix |
| BLK-9 (`MANUAL.md:271`) | D: BLOCKING · A: SHOULD-FIX · B, C, E, F: confirmed, no dissent | **BLOCKING, blocks delivery** | Same class as BLK-8; A's "incomplete, not false" precision is right and does not change the grade |
| BLK-10 (duplicates) | D: BLOCKING · F: SHOULD-FIX/exec · B: SHOULD-FIX · E: NOTE/delivery | **BLOCKING, blocks execution** | design.md states a precondition ("deleted before either executes") that no task discharges and this session may not perform |
| BLK-11 (ceiling early-fire) | E: BLOCKING/exec · F: SHOULD-FIX/delivery | **BLOCKING, blocks delivery** | The executor can write 3.4; the unconstrained constant cannot ship |
| BLK-14 (unshifted usability) | E: BLOCKING/exec · A: SHOULD-FIX/delivery | **BLOCKING, blocks execution** | Task 4.6 is told to assert a scenario that certifies the failure |
| BLK-16 (three non-constructing sites) | A, B: SHOULD-FIX, both saying "blocks execution" | **BLOCKING, blocks execution** | Graded by what breaks, per the brief |
| ABI blast radius (SF-3 below) | C: SHOULD-FIX/exec · A, D: NOTE | **SHOULD-FIX, blocks delivery** | Wrong figure in a shipped artifact; the executor sweeps by measurement, not by the figure |

---

## 3. Refuted sub-claims inside confirmed findings

**R-1 — F-ADV10: "Sheaf's MODIFIED smi-14 and smi-16 amend requirements absent
from the promoted spec, and `openspec validate --strict` passes anyway."**
The *fact* is confirmed; the *blocker* is refuted. The dependency is disclosed
and mechanically gated, so nothing breaks that the change does not already
manage. Downgraded to NOTE.

```
$ grep -n "^### Requirement" External/Sheaf/openspec/specs/synth-midi-instrument/spec.md | tail -3
282:### Requirement: smi-10 — Realtime input: terminal clock and transport routing
306:### Requirement: smi-11 — Sender: scheduled broadcast realtime lane
327:### Requirement: smi-12 — Output sinks: host-timestamp scheduling contract
$ grep -rn "### Requirement: smi-14\|### Requirement: smi-16" External/Sheaf/openspec/ --include='*.md' | grep -v "changes/midi-controller-resilience"
openspec/changes/app-midi-catalog/specs/synth-midi-instrument/spec.md:200:### Requirement: smi-14 — …
openspec/changes/shift-and-file-export/specs/synth-midi-instrument/spec.md:194:### Requirement: smi-16 — …
$ openspec validate midi-controller-resilience --strict
Change 'midi-controller-resilience' is valid
EXIT=0
```

But the change names the dependency and gates on it:

```
$ grep -rn "app-midi-catalog\|shift-and-file-export" …/midi-controller-resilience/{proposal,design,tasks}.md | head -6
proposal.md:93:| `app-midi-catalog` | 26/28, PR #13 | owns smi-14 … #13 is below this branch, so it lands first. |
proposal.md:94:| `shift-and-file-export` | complete, PR #14 | owns smi-16 | Amend smi-16 in this delta; #14 is below this branch. |
design.md:111:not on #13 or #14.** Both are unmerged …
tasks.md:89:      `git merge-base --is-ancestor fork/app-midi-catalog HEAD`,
tasks.md:90:      `git merge-base --is-ancestor fork/shift-and-file-export HEAD`,
```

smi-17 is correctly filed under `## ADDED Requirements` (line 82), not MODIFIED.
What survives as a NOTE: `openspec validate --strict` cannot detect the
situation, and Sheaf has no equivalent of frogg3rs's
`check-modified-requirements-restate-promoted` gate
(`grep -rn "restate_promoted" External/Sheaf/projects/synth/Makefile` → exit 1).

**R-2 — B1's "and no `displayName`".** Refuted; F narrowed it correctly in its
exchange. The struct comment calls `displayName` the "dropdown label", and task
5.6 fixes that label's text: `tasks.md:187` — `add "Novation Launch Control XL"
to the Overview's Preset selector list (`:264-265`)` — and `spec.md:68` repeats
it. BLK-2's remedy names only the `id`.

**R-3 — D7's "a session executing the other copy satisfies frogg3rs 4.1's
gate".** Refuted; 4.1 carries a mechanical content check, quoted in BLK-10.

---

## 4. Open items

**O-1 — A1's vendor-document readings** (whether the Programmer's Reference
pages 5/6 establish the template↔channel identity, and whether design.md's
"zero-indexed channel n" corroboration resolves at the right section).
*What would settle it:* re-fetching the two PDFs at the URLs design.md cites and
quoting the relevant pages. I could not: the extracted copies sit in the
scratchpad directory my brief bars me from reading, and I ran no fetch. B and F
both marked the same sub-claim OPEN.
*Needs the operator:* no.

**O-2 — whether the `.pyc` actually encodes CC 77–84, channel 8 and template 1.**
*What would settle it:* `python3 -m dis` or `marshal.loads` over the file.
*Needs the operator:* yes — the file is under `/Applications`, which no executor
in this cycle is permitted to open. Note this does not reopen operator ruling 1:
the ruling settles that the lead read it and that the preset ships. What is open
is only whether the artifacts' record of that reading can be made checkable.

**O-3 — whether `npm ci` may be run under `projects/synth/browser`.**
*What would settle it:* the operator's answer, requested and not yet given.
*Needs the operator:* yes. BLK-4's remedy is stated both ways.

**O-4 — the two behavioural questions no one has run.**
(a) BLK-12: construct an `Engine` with a provider returning a value above
30 000 000, tick once with no modifier ever held, print `lastClear`.
(b) C3 / F-ADV8: render the Controllers add row with a non-empty
`declaredPreconditions` list and run
`portable_ui_tests.cpp: TestControllersRowFitsWithinFroggersNarrowestHost`.
Both require compiling, which this audit may not do. Per the operator's standing
rule, a run that decides a design belongs inside preflight; these two are
preflight work that has not been performed.
*Needs the operator:* no, but it needs a context permitted to build.

---

## 5. SHOULD-FIX and NOTE findings, consolidated

"Verified" = I opened the artifact or ran the command in this adjudication.
"Not adjudicated" = I did not check it; verify at the point of repair.

### Verified

| id | Sources | Finding | Breaks |
|---|---|---|---|
| SF-1 | A4 | design.md:260 routes the browser endpoint-open drain through `worker.ts`'s `message-tick` handler. That handler (`worker.ts:647-659`) drains only `dequeueFileExport`; the MIDI action queue is drained in the `midi-endpoints` handler (`:668-675`), reached from `midi.ts`'s `submitEndpoints` off a poll timer (`midi.ts:80,131,223`). Task 3.5's own instruction names the right place; design.md's mechanism statement is wrong. | Delivery — the design record misstates the mechanism it justifies |
| SF-2 | A11, D8 | The main-checkout survey in task 3.2 is stale in two ways. `frogg3rs-delay-capacity-and-width-finish` is 35/41, not 25/41 (`grep -c '^- \[x\]'` → 35; `'^- \[ \]'` → 6), and `MANUAL.md` is **not** modified there (`git status --short MANUAL.md QUICK_DICT.md` in the main checkout returns nothing), refuting "its task 2.1 already edited `MANUAL.md` … and is uncommitted there". | Delivery — the coordination premise is false |
| SF-3 | C6, A12, D12 | "a 24-site sweep across 25 files" (Sheaf tasks.md 3.5) does not reproduce: 24 `abiVersion: 6` literals across **12** files (`grep -rn … \| wc -l` → 24; `grep -rln … \| wc -l` → 12). The pair is internally impossible — more files than sites. | Delivery |
| SF-4 | D6 | `README.md:110-112` enumerates the presets ("the MIDI Fighter Twister, the Akai APC40 mkII, and three Launchpad models") and `README` appears in neither change's artifacts (`grep -rn "README" proposal.md tasks.md` → exit 1). Same class as BLK-8/BLK-9. | Delivery |
| SF-5 | B14 | Sheaf task 7.7 gives its branch name as an example ("`e.g. git push fork midi-resilience-merge`") while frogg3rs 4.1 and 6.6 pin `fork/midi-resilience-merge` exactly. | Delivery, if the Sheaf executor picks another name |
| SF-6 | D2, D3 | This branch is five commits behind `main` (`git rev-list --left-right --count main...HEAD` → `5 1`), and `frogg3rs-randomize-depth-reclaim` exists on `main` and not here — so the symbols gate's verdict depends on which checkout it runs in. Eight of BLK-7's fifteen resolve by bringing the branch forward. | Delivery |
| SF-7 | D9 | `app/check_citations_resolve.py` defines `SEARCH_ROOTS` twice — lines 59 and 64, identical — so the first is dead. | Neither |
| SF-8 | C8 | Sheaf has no mechanical Check-resolution or restate-promoted gate over its spec deltas; frogg3rs runs both (`check-spec-checks-resolve`, `check-modified-requirements-restate-promoted` on `app/Makefile:320`). This is what lets R-1 pass unnoticed. | Neither on its own |
| SF-9 | F-ADV11 | Task 4.5's loop "runs against today's six device defaults" and "Does not depend on 4.1/4.2's gate", while task 5.3 claims "Once this default exists, task 4.5's loop already covers it". If 4.5 runs when written, the Launch Control XL is not in the catalogue and the loop never sees it. | Delivery — a false coverage claim |
| SF-10 | B12, F-ADV9, E-ADV14 | The deltas carry 9 (frogg3rs) + 19 (Sheaf: 3 + 8 + 8) `Check: not yet delivered` lines. I verified the counts; I did not verify the claim that no task rewrites them after delivery. | Delivery |

### Not adjudicated — verify at the point of repair

A5 (one of three named test sites does not need updating) · A9 (§8.0's sweep
leaves eight dangling planning-doc citations under `projects/synth/runtime`) ·
A13 (Engine construction-site enumeration names three of at least nine) · A14
(smi-17's byte shape is documented by a vendor source the change fetched) · A15
(sru-64 states no behaviour for the four `Custom (...)` Preset options) · B8
(Sheaf 3.5 names two insertion points in one sentence) · B9 (four Sheaf tasks
state a check with no binary, case name or assertion) · B11 (task 4.3's two
cited ranges overlap on one line, split mid-clause) · B13 (the frogg3rs change
has no commit task, and 4.2/3.2/6.6 reference one) · B15 (4.2 detaches the
submodule HEAD; nothing restores it) · B17 (group 2 is written as complete but
left unchecked) · C7 (`Makefile:193`'s prerequisites omit `BrowserMidiBridge.hpp`)
· C9 (`HeldModifierState`'s embedding style is unspecified; five `->held` call
sites hinge on it) · C10 (baseline and citation accuracy record) · D10
(`projects/synth/docs/coverage.md` is hand-maintained and nothing invokes it) ·
E-ADV4 (the template-change rate limiter passes as a permanent latch) · E-ADV5
(sru-63's central prohibition has no check that can fail) · E-ADV7 (task 4.5's
positive control is already satisfied by pre-existing cases) · E-ADV11 (the
submodule gate is one grep — partly refuted at BLK-10; the residue is unchecked)
· E-ADV12 (`lastClear` is never rendered; the risk register's mitigation has no
deliverable) · E-ADV16 (smi-17's "moves no mapping" check cannot fail) · E-ADV17
(two citation hazards in edit instructions) · F-ADV5 (the rebuild scenario is
satisfied by a never-held profile — its site-count half is confirmed at BLK-16)
· F-ADV6 (the flood check accepts a notifier that fires once and dies) · F-ADV7
(sru-63's "a matching controller carries no report" is satisfied by a silent
controller) · F-ADV14 (the rewritten recovery names triggers nothing ties it to)
· F-ADV15 (Sheaf 3.9's null-provider negative case).

**Withdrawn by its own author in the exchange, and not re-opened here:** D11
(`tests/instrument_tests.cpp` named nowhere in the Sheaf artifacts) — D's own
re-run found 14 mentions.

**Settled by operator ruling, not a finding:** B16 (frogg3rs delivery is a side
branch to `origin`, not `main`) — ruling 3. B18 (push credentials unverified) —
operator's to check at delivery.

---

## 6. Verdict

**REJECT the pair.** Sixteen confirmed blocking findings, listed in §1. Eight of
them block execution outright — an executor cannot proceed without inventing
something: BLK-1 (the map's provenance), BLK-2 (the missing `id`), BLK-3 (the
unbaselined gate), BLK-4 (the unrunnable gate), BLK-5 (the check with no written
assertion), BLK-10 (the undischargeable deletion), BLK-12 (the ceiling condition
that defeats task 3.1), BLK-14 (a scenario that certifies the failure), BLK-15 (a
self-contradictory requirement), BLK-16 (an assignment with nothing to attach
to). The remaining six block delivery.

Two of the confirmed findings — BLK-5's undeclared-dependency clause and BLK-14's
false unshifted-usability SHALL — bear directly on the defect that motivated the
change, which raises the cost of shipping them unaddressed.

---

## 7. Coverage note

**Reached independently by more than one report** (the exchange files record the
independence, and I confirmed the facts myself):

| Finding | Reports | Count |
|---|---|---|
| BLK-8 (six device defaults) | D4, A6, C4, F-ADV12 | 4 |
| BLK-3 (miniapp gate) | A2, B3, C1 | 3 |
| BLK-5 (drift check) | B4, F-ADV2, E-ADV9 on the drift half; A8 and C5 on distinct halves of the recovery half | 3 + 2 |
| BLK-10 (the *frogg3rs* duplicate) | B10, A10, E-ADV13, F-ADV13 | 4 |
| BLK-1 (LCXL provenance) | A1, F-ADV1 independently; E-ADV15 and B6 converged on it during the exchange by upgrading their own severity | 2 + 2 |
| BLK-4 (npm gate) | B2, C2 | 2 |
| BLK-7 (red gate) | D1; E-ADV8 on the recipe-stops half; D2 on the branch-lag half | 2 |
| BLK-11 (ceiling early-fire) | E-ADV1, F-ADV4 | 2 |
| BLK-14 (unshifted usability) | E-ADV6, A7 | 2 |
| BLK-16 (non-constructing sites) | A3, B7 | 2 |

**Reached by exactly one report:**

| Finding | Report |
|---|---|
| BLK-2 (the Launch Control XL default's missing `id`) | B1 |
| BLK-6 (baseline straddles the submodule pin advance) | B5 |
| BLK-9 (`MANUAL.md:271`) | D5 |
| BLK-10, the *Sheaf* duplicate carrying no scw-6 | D7 |
| BLK-12 (task 3.4's missing `held` term) | E-ADV2 |
| BLK-13 (browser endpoint-open, two halves) | E-ADV3 |
| BLK-15 (smi-16's independence sentence) | F-ADV3 |

Seven of the sixteen rest on a single reporter's axis. Five of those seven —
BLK-2, BLK-9, the Sheaf duplicate, BLK-12 and BLK-15 — were found by the two
reports (B/D and E/F) whose briefs evidently ran along different axes from the
rest; three of them (BLK-12, BLK-13, BLK-15) are among the most consequential in
the set. Convergence, in this round, tracked how easy a defect was to reach, not
how much it mattered.
