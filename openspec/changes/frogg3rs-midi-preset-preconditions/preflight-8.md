# Preflight 8 — adjudication

Adjudicator did not write the change, the briefs, or any report. Every verdict
below rests on a command I ran myself, with its literal output. Agreement
between reporters is recorded but is not evidence. Where I could not settle a
claim I say so.

Read-only throughout: no edit, no commit, no stash, no install, no build,
nothing opened under `/Applications`, nothing written outside this file and one
throwaway probe in the scratchpad.

---

## 0. The tree has moved since the reports were written

Three facts changed between the reports and this adjudication. They bear on
several findings, so they are stated first.

```
$ git -C <worktree> log --oneline -1
312b5dd Repair the MIDI preset preconditions artifacts against the third adjudicated preflight
$ git -C <worktree> status --short | wc -l
       0
$ git -C <worktree>/External/Sheaf log --oneline -1
17af9825 Repair the midi-controller-resilience artifacts against the third adjudicated preflight
$ git -C <worktree>/External/Sheaf status --short | wc -l
       0
```

Both audit HEADs are unchanged and both trees are clean. But:

```
$ git -C <worktree> log --oneline HEAD..main
188b109 Promote the randomize depth-reclaim requirement and archive the change
777c309 Promote the Delay width requirements and archive the change
79d7a6a Pin Release to the floor in the three routing tests that measure silence
```

`main` has moved by three commits. Every report ran this and got `0`.

```
$ git -C /Users/diegoaguilar-canabal/Desktop/frogg3rs worktree list
/Users/diegoaguilar-canabal/Desktop/frogg3rs                                    188b109 [main]
/Users/diegoaguilar-canabal/Desktop/frogg3rs/.claude/worktrees/midi-resilience  312b5dd [worktree-midi-resilience]
$ ls /Users/diegoaguilar-canabal/Desktop/frogg3rs/.claude/worktrees/
midi-resilience
```

The `randomize-depth-reclaim` worktree **has been removed**. A `git worktree
remove` of a sibling has already happened inside this session's lifetime, which
converts BLOCK-5 from a predicted hazard into an observed mechanism.

Consequences carried into the findings below:

- frogg3rs task 1.2's **first** gate (`git log --oneline HEAD..main | wc -l` →
  `0`) now fails on its own terms. That is the task working as designed; the
  remedy is a rebase. It is independent of BLOCK-1 and does not rescue it.
- `proposal.md`'s pasted `git worktree list` (`proposal.md:151-155`) is now
  wrong on all three of its rows: `main` reads `634d292`, this worktree reads
  `a66f651`, and the third row names a worktree that no longer exists.
- Any recorded figure in these artifacts must be re-measured after the rebase,
  not merely corrected to the values in this document.

---

## 1. Confirmed BLOCKING findings

Fourteen. Each carries the **class** it belongs to, so a repair can close the
class rather than the instance. Remedies are stated as conditions on the
artifacts.

Six recurring classes appear:

- **C-FROZEN** — a figure or command output measured once, written into an
  artifact as a literal, and never re-measured when the artifact was next
  edited. (BLOCK-1, BLOCK-10; and SF-1, SF-3, SF-8 below.)
- **C-RECORD** — the reading that produced a shipped value is not the reading
  that is written down. (BLOCK-2.)
- **C-OBJECT** — the object a step confirms, pins, or relies on is not the
  object that is checked, published, or reachable. (BLOCK-3, BLOCK-4, BLOCK-5.)
- **C-BLANK** — the artifacts leave a decision unwritten that an executor must
  make, and one of the available fillings is green. (BLOCK-6, BLOCK-7, BLOCK-8.)
- **C-DEAD-CHECK** — a check is placed where the thing it measures cannot
  occur, or its rule cannot reject the text/shape it was written against.
  (BLOCK-9, BLOCK-13; and SF-13, SF-14.)
- **C-UNDEFINED** — a requirement or promoted claim has no defined behaviour
  for a shape the change actually ships. (BLOCK-11, BLOCK-12.)

---

### BLOCK-1 — tasks 1.2 and 1.6 record a gate output the tree does not produce

**Sources:** A1, B1, C4 (graded SHOULD-FIX originally, withdrawn upward to
BLOCKING in C's exchange), D1, E-ADV9, F-ADV11 (SHOULD-FIX header, "Blocks
execution" body). Six arrivals.
**Class:** C-FROZEN. **Blocks execution.** **CONFIRMED.**

Both tasks record `1 name(s)`:

```
$ sed -n '74,75p' openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md
      `check-no-planning-history`, `check-artifact-symbols-resolve` (`OK - 6
      artifact file(s) resolve, 1 name(s) declared as not yet created`),
```

(task 1.2 carries the identical literal at `tasks.md:36-37`, followed by "If
either check disagrees because `main` has moved since this was written, STOP and
report the actual state rather than resolving by guesswork.")

The tree reports `2`:

```
$ python3 app/check_artifact_symbols_resolve.py app
check-artifact-symbols-resolve: OK - 6 artifact file(s) resolve, 2 name(s) declared as not yet created
EXIT=0
```

The cause is the change's own artifacts, not `main`. The counter increments per
backticked **token occurrence**, not per distinct name:

```
$ grep -n '^ARTIFACTS' app/check_artifact_symbols_resolve.py
111:ARTIFACTS = ("tasks.md", "proposal.md")
$ sed -n '439,441p' app/check_artifact_symbols_resolve.py
                    if token in forward_here:
                        forward += 1
                        continue
$ grep -n 'app/check_docs_match_device_preconditions.py' openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md
108:- [ ] 1.8 **Create NEW `app/check_docs_match_device_preconditions.py` with its
378:- [ ] 4.8 Extend `app/check_docs_match_device_preconditions.py` (task 1.8
$ for c in a66f651 7b2dd5d 312b5dd; do printf "%s " $c; git show $c:openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md | grep -c 'app/check_docs_match_device_preconditions.py'; done
a66f651 1
7b2dd5d 1
312b5dd 2
```

One forward-declared name, counted twice; the second occurrence was introduced
by `312b5dd`, this change's own last repair commit.

**Rulings on disputed sub-claims inside this finding.** A1's attribution of the
second declaration to "the task-4.8 emitter binary", and B1's to "two distinct
names", are both **REFUTED** by the run above — B and C both withdrew these in
their exchanges and D/E/F/A converge on the occurrence-counting account, which I
confirm. C4's original SHOULD-FIX grade is **superseded**: the literal also sits
in task 1.2, where a STOP is attached to it.

**What changed since the reports.** `main` has now moved (§0), so task 1.2's
written escape hatch ("because `main` has moved") is now nominally available.
This does not rescue the finding: the two forward occurrences live in the
change's own `tasks.md` and survive any rebase, and task 1.2's first gate
(`HEAD..main` → `0`) now fails outright as well. The executor stops at task 1.2
of 27 on two independent grounds.

**Remedy (condition on the artifacts).** No recorded gate output in either
`tasks.md` may be a frozen literal. Each of tasks 1.2 and 1.6 must either state
the assertion as the command's *exit code and error count* ("exit 0, zero
unresolvable names"), or be re-run and re-recorded at the commit that ships, on
a tree rebased onto current `main`. Task 1.2's stop clause must additionally
cover a disagreement caused by the change's own artifacts, which it presently
has no instruction for.

---

### BLOCK-2 — the one figure the preset ships (CC 77) is carried by no recorded command output

**Sources:** A2 only.
**Class:** C-RECORD. **Blocks delivery.** **CONFIRMED.**

design.md's section "Who read the script, by what command, and what it printed"
records one command over the **module** code object:

```
$ sed -n '452,464p' openspec/changes/frogg3rs-midi-preset-preconditions/design.md
 … ~/.local/bin/python3.11 -c 'import marshal,dis; c=marshal.loads(open("LaunchControlXL.pyc","rb").read()[16:]); [print(...) for x in dis.get_instructions(c) if x.opname in ("LOAD_CONST","LOAD_NAME","STORE_NAME","BUILD_TUPLE","BINARY_OP")]'
 222 LOAD_CONST     8
 224 STORE_NAME     LIVE_CHANNEL
 226 LOAD_CONST     (240, 0, 32, 41, 2, 17, 119)
 228 STORE_NAME     PREFIX_TEMPLATE_SYSEX
 230 LOAD_NAME      PREFIX_TEMPLATE_SYSEX
 232 LOAD_NAME      LIVE_CHANNEL
 234 LOAD_CONST     247
 236 BUILD_TUPLE
 238 BINARY_OP      +
 242 STORE_NAME     LIVE_TEMPLATE_SYSEX
```

and then introduces the CC map in prose with no command and no output
(`design.md:466-468`), conceding at `:474` that "The CC numbers 77-84 rest on
the disassembly alone".

I built a positive control of the same shape and ran it under the interpreter
design.md itself records:

```
$ ~/.local/bin/python3.11 -V
Python 3.11.14
$ ~/.local/bin/python3.11 adj_a2.py       # LIVE_CHANNEL=8, PREFIX/LIVE_TEMPLATE_SYSEX,
                                          # def make_slider(...), [make_slider(77+i) ...]
   2 LOAD_CONST   8
   4 STORE_NAME   LIVE_CHANNEL
   6 LOAD_CONST   (240, 0, 32, 41, 2, 17, 119)
   8 STORE_NAME   PREFIX_TEMPLATE_SYSEX
  10 LOAD_NAME    PREFIX_TEMPLATE_SYSEX
  12 LOAD_NAME    LIVE_CHANNEL
  14 LOAD_CONST   247
  16 BUILD_TUPLE
  18 BINARY_OP    +
  22 STORE_NAME   LIVE_TEMPLATE_SYSEX
  24 LOAD_CONST   <code object make_slider at 0x…, line 5>
  30 LOAD_CONST   <code object <listcomp> at 0x…, line 7>
--- '77' anywhere in the filtered module-level stream? --- False
--- nested code objects NOT visited --- ['make_slider', '<listcomp>']
```

The instruction sequence reproduces the recorded output line for line, and `77`
does not appear. `dis.get_instructions` does not descend into nested code
objects, so the recorded command **cannot** have produced the fader CCs.

Version sensitivity, which the remedy must respect:

```
$ python3 -V
Python 3.13.5
$ python3 adj_a2.py | tail -2
--- '77' anywhere in the filtered module-level stream? --- True
--- nested code objects NOT visited --- ['make_slider']
```

From 3.12 (PEP 709) comprehensions inline into the enclosing code object and
`77` *does* surface at module level. Re-running the recorded command under
`python3` rather than `python3.11` would produce a misleadingly complete output.
`make_slider` is a `def` and is a nested object under every version, so the
`SliderElement(MIDI_CC_TYPE, LIVE_CHANNEL, identifier, …)` body is out of reach
either way.

**Second defect in the same record, CONFIRMED from the recorded output itself.**
design.md:469 says the bytecode "gives `LIVE_CHANNEL = 8` and the template byte
`8` directly; the two paragraphs above corroborate what those two integers
*mean*". Offsets 230-238 show the template byte **is** `LOAD_NAME LIVE_CHANNEL`.
There is one constant serving two roles, not two integers, and it cannot
corroborate itself.

**Relation to operator ruling 1.** The ruling settles the provenance and defers
hardware confirmation to the operator's post-merge check. A2 disputes neither.
It is about the record: the reading that carries the shipped number is not the
reading that is written down.

**Remedy (condition on the artifacts).** `lcxl-slider-disassembly.txt` is that
missing record — it shows `make_slider` loading `LIVE_CHANNEL`, and the
`<listcomp>` at script line 101 calling `make_slider` with `LOAD_CONST 77` +
`i`, alongside the three encoder rows at 13, 29 and 49. design.md's "Who read
the script, by what command, and what it printed" section must carry that
literal record in the same form as the module-level one, including the
interpreter version and the fact that a nested-object disassembly is required.
Do not re-measure. The "two integers" sentence must be rewritten to say that one
constant serves both the slider channel and the `Change current template` byte.

---

### BLOCK-3 — no Sheaf task commits, yet frogg3rs 4.1 confirms a working tree and 4.2 pins a commit

**Sources:** B2 only.
**Class:** C-OBJECT. **Blocks delivery.** **CONFIRMED.**

```
$ grep -n 'git commit\|git add' External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md
exit=1
$ grep -in 'commit' External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md
77:      committed `package-lock.json`; approved by the operator for this
162:      rebase, since that result establishes #13's, #14's and #15's commits
526:      in this branch's history (commits `1d97e5ca`, `582e400a`, `185a7a09`);
705:      uses. Until the operator performs that step, the commit this branch
$ sed -n '697,700p' External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md
- [ ] 7.7 This task performs no push, opens no pull request, and moves no
      pin. … Verify a clean tree and stop there.
```

Four occurrences of "commit", none an instruction. frogg3rs 4.1 confirms with
two greps "against the checkout **as it actually stands**" and then records the
SHA `git -C External/Sheaf rev-parse HEAD` reports; 4.2 pins that SHA. The
working tree and the commit are different objects, and today the commit declares
neither symbol:

```
$ git -C External/Sheaf grep -n 'declaredPreconditions' HEAD -- projects/synth/include/synth/MidiAppCatalog.hpp; echo exit=$?
exit=1
$ git -C External/Sheaf grep -n 'HeldModifierClearSource' HEAD -- projects/synth/include/synth/MidiController.hpp; echo exit=$?
exit=1
```

**Narrowing (recorded, does not disturb the finding).** Sheaf 7.7's "verify a
clean tree" would surface uncommitted work — but it is in the *Sheaf* change and
frogg3rs 4.1's gate names only Sheaf 6.1, 6.2 and 3.1, all of which precede 7.7
(see BLOCK-4), so frogg3rs group 4 can legitimately run first.

**Remedy (condition on the artifacts).** Sheaf's `tasks.md` must carry an
explicit commit task before 7.7, and frogg3rs 4.1's confirmation must read the
same object it pins — `git -C External/Sheaf grep -n <symbol> HEAD -- <path>`,
not a working-tree grep.

---

### BLOCK-4 — frogg3rs 4.1's gate is partial, and 4.2 detaches the checkout Sheaf is executed in

**Sources:** B4 only.
**Class:** C-OBJECT. **Blocks delivery.** **CONFIRMED.**

```
$ sed -n '238,247p' openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md
- [ ] 4.1 **Gate.** Do not start the rest of this group until
      `External/Sheaf`'s submodule checkout — this worktree's own copy, the
      same one Sheaf's `midi-controller-resilience` change is executed
      against, sitting on branch `midi-resilience-merge` — has executed
      Sheaf's tasks 6.1 …, 6.2 …, AND Sheaf's group 3, task 3.1 …
$ git -C External/Sheaf rev-parse --abbrev-ref HEAD
midi-resilience-merge
$ cat External/Sheaf/.git
gitdir: ../../../../../.git/worktrees/midi-resilience/modules/External/Sheaf
```

Three of Sheaf's forty-plus tasks gate frogg3rs group 4. Nothing in either
artifact requires Sheaf's change to be complete, so the pin can name a Sheaf
commit lacking Sheaf's groups 4 (sru-63), 5 (smi-17) and 7 entirely. 4.2 then
runs `git -C External/Sheaf checkout <sha>` in that same shared checkout.

**Two narrowings, both recorded.** (i) A's exchange is right that 4.2 **does**
disclose the detach ("that is the normal, expected state for a pinned submodule
gitlink … the `midi-resilience-merge` branch ref itself is untouched"). What is
not written down is the consequence: any *subsequent* Sheaf commit lands on a
detached HEAD the branch ref never sees. B4's "neither written down" is
therefore half wrong; the finding stands on the undisclosed consequence.
(ii) F's narrowing is right that the incomplete pin does not break the frogg3rs
build — frogg3rs needs only the two symbols. What ships incomplete is the Sheaf
half the operator's later merge carries.

**Remedy (condition on the artifacts).** 4.1's gate must be stated as "Sheaf's
`midi-controller-resilience` is complete through its own task 7.7 and
committed", and 4.2 must state that it is the last step to touch that checkout
and that any Sheaf work after it lands off the branch.

---

### BLOCK-5 — the pinned Sheaf commit lives only in this worktree's private submodule object store

**Sources:** B5 only.
**Class:** C-OBJECT. **Blocks delivery.** **CONFIRMED** — and the operator has
already ruled this a fact (ruling 2), which fixes the remedy.

```
$ cat /Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/.git
gitdir: ../../.git/modules/External/Sheaf
$ cat …/.claude/worktrees/midi-resilience/External/Sheaf/.git
gitdir: ../../../../../.git/worktrees/midi-resilience/modules/External/Sheaf
$ git -C /Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf cat-file -e 17af9825e6c7d0aacdd03e3fc7bd59864983f4dc; echo main exit=$?
main exit=1
$ git -C …/worktrees/midi-resilience/External/Sheaf cat-file -t 17af9825…
commit
$ git -C …/worktrees/midi-resilience/External/Sheaf branch -r --contains 17af9825…
(no output)
```

Two distinct submodule object stores. The commit currently under audit is
already absent from the main checkout's, before any new Sheaf commit exists.

**New fact, not available to any reporter.** The `randomize-depth-reclaim`
worktree has been removed during this session (§0). The mechanism B5 predicts —
`git worktree remove` destroying a worktree-local submodule object store — has
already fired once on this machine. This is no longer a hypothetical.

**Remedy, as the operator's ruling fixes it.** Nothing is pushed. The artifacts
must state, in frogg3rs task 4.2 and task 6.6, that the pinned Sheaf commit
lives only in
`.git/worktrees/midi-resilience/modules/External/Sheaf`, that a fresh clone or
the main checkout cannot resolve the pushed gitlink until the operator's merge,
and that **no step before that merge may remove this worktree or otherwise
destroy that object store**. 4.2's "no `fetch` is needed" must be scoped to
staging the pin, since it is false for anyone resolving the gitlink afterwards.

---

### BLOCK-6 — `engine_tests.cpp` cannot observe the state its three `Check:` lines assert

**Sources:** C1; E-ADV20 (filed NOTE, upgraded to BLOCKING in E's exchange,
deferring to C1).
**Class:** C-BLANK. **Blocks execution** (Sheaf task 3.9). **CONFIRMED.**

Three shipped `Check:` lines name `engine_tests.cpp` cases asserting a
modifier's `held` flag and its recorded clear trigger:

```
$ grep -n 'engine_tests.cpp' External/Sheaf/openspec/changes/midi-controller-resilience/specs/synth-midi-instrument/spec.md | cut -c1-120
69:- Check: not yet delivered; task 3.9 adds `engine_tests.cpp: HeldModifierCeilingIsWallClockNotPumpCount`
74:- Check: not yet delivered; task 3.9 adds `engine_tests.cpp: HeldModifierCeilingDoesNotFireBeforeItElapses`
79:- Check: not yet delivered; task 3.9 adds `engine_tests.cpp: NeverHeldModifierSurvivesTheCeilingWithRebuildAsItsClearSource`
```

The state lives on a private member with no reaching accessor:

```
$ sed -n '957,963p' External/Sheaf/projects/synth/include/synth/MidiController.hpp
struct MidiControllerProfileResult {
    std::unique_ptr<MidiInProcessor> input;
    std::vector<std::unique_ptr<MidiInProcessor>> inputThru;
    std::vector<std::unique_ptr<MidiOutputProcessor>> outputs;
    std::unique_ptr<HoldDrillState> holdDrill;
    std::unique_ptr<ShiftState> shift;
};
$ awk 'NR<=1461 && /^ *(public|private|protected) *:/ {l=NR": "$0} END{print l}' External/Sheaf/projects/synth/include/synth/Engine.hpp
1050: private:
$ grep -n 'midiProcessors_' External/Sheaf/projects/synth/include/synth/Engine.hpp | grep -E '^(731|741|844|1461):'
731:    std::size_t MidiControllerCount() const { return midiProcessors_.size(); }
741:        return midiProcessors_[controllerIx].input.get();
844:        for (auto& output : midiProcessors_[controllerIx].outputs) {
1461:    std::vector<MidiControllerProfileResult> midiProcessors_;
$ sed -n '396,403p' External/Sheaf/projects/synth/include/synth/MidiController.hpp
private:
    …
    HoldDrillState* holdDrill_ = nullptr;
    ShiftState* shift_ = nullptr;
};
$ grep -n '^#include' External/Sheaf/projects/synth/tests/engine_tests.cpp | head -3
1:#include "synth/AppRegistry.hpp"
2:#include "synth/Engine.hpp"
3:#include "synth/RuntimePagePolicy.hpp"
```

The entire public reach over `midiProcessors_` is `.size()`, `.input.get()` and
`.outputs`. Neither `.holdDrill` nor `.shift` is reachable;
`SystemButtonMidiInProcessor::shift_` is private with no getter; and
`engine_tests.cpp` does not include `synth/MidiController.hpp`.

Nothing in the change adds a seam:

```
$ grep -n -i 'ForTest\|accessor\|friend\|expose' External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md
55:      `InstallInstrumentForTest` (which calls
56:      `Engine::RebuildMidiProcessorsForTest()`) — a scratch `TEST_CASE` added
59:      Shift, print the flag, call `InstallInstrumentForTest`, print it again.
$ grep -n 'engine_tests' External/Sheaf/openspec/changes/midi-controller-resilience/design.md; echo exit=$?
exit=1
```

Task 3.9 is otherwise unusually specific (it names the timestamp helper, the 55
existing fixed-provider constructions, the `held` precondition, the positive
control) and says nothing about how the assertion is read back. Three of its
five further cases — the blacklisted-slot case, the exactly-one-null-pointer
case, the empty-provider case — also require reaching
`MidiControllerProfileResult`'s fields from that file. The blank is concrete, an
idiomatic fill is obvious (`Engine.hpp` already carries thirteen `…ForTest`
members), and the fill is an unbriefed production-header change.

**Remedy (condition on the artifacts).** Either group 3 gains a task that adds a
named read-only `Engine` accessor reaching `midiProcessors_[ix].holdDrill` /
`.shift`, named in the Impact, with task 3.9's cases reading through it; or the
three `Check:` lines are re-homed to a binary that can both drive the ceiling
and observe the result. A `Check:` line may not name a file with no path to what
it asserts.

---

### BLOCK-7 — Sheaf's new `check_spec_checks_resolve.py` is red on arrival, on lines this change does not own

**Sources:** C2 (three lines), E-ADV1 (six lines), D (four certain, two OPEN),
F.
**Class:** C-BLANK / new gate scoped beyond the change. **Blocks execution**
(Sheaf task 1.9) **and blocks delivery** (it joins `projects/synth`'s `test:`).
**CONFIRMED, with my own count.**

Task 1.9's rule scans `openspec/specs/**/spec.md` and
`openspec/changes/<name>/specs/**/spec.md` with only `archive` excluded, and
wires the script into `projects/synth/Makefile`'s `test` target. My own
mechanical enumeration over all 103 non-archive spec files:

```
$ python3 /tmp/adj_checks.py
spec files scanned: 103
total Check lines: 130
prefix-declared: 24  remaining: 106
```

Of the 106 non-prefixed lines, four fail the stated rule under **every** reading:

```
$ sed -n '194p;199p;204p;86p' openspec/changes/app-midi-catalog/specs/synth-runtime-ui/spec.md
- Check: `Makefile`: `DEPFLAGS := -MMD -MP` on the five test-binary rules and on the split
- Check: `controllers_page_ui_tests.cpp: main`
- Check: `controllers_page_ui_tests.cpp: main`
- Check: `controllers_page_ui_tests.cpp: main`
$ grep -c 'TEST_CASE(' External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp
0
$ grep -n 'int main' External/Sheaf/projects/synth/tests/controllers_page_ui_tests.cpp
1975:int main()
$ ls External/Sheaf/projects/synth/scripts/
check_app_bundle_plist.sh  check_ui_boundary.sh  check_ui_boundary_empty_discovery.sh
```

`main` is the entry point, not a `TEST_CASE`, not a `[static] void TestFoo()`
called a second time, not a JS `test("name")`. The `Makefile` line's backticked
content names no test case and no script in either `test:` prerequisite list
(the only three qualifying scripts are listed above).

Two more fail the rule **as written** but are rescuable by a lenient parser:

```
$ sed -n '12p' openspec/changes/app-midi-catalog/specs/synth-portable-runtime-shell/spec.md
- Check: `browser/tests/ui-backend.spec.ts`, "renders portable controls, canvas draws, and reachable scroll content" (select-fills-wrapper assertion)
$ sed -n '16p' openspec/changes/shift-and-file-export/specs/synth-browser-wasm-runtime/spec.md
- Check: `browser/tests/file-export.spec.ts`, driving one message tick after the click
```

Both backtick a repository-relative **path** where the rule accepts "a bare
basename", and put the case name (where there is one) in double quotes outside
the backticks, so the backticked content names no case. Both files exist, both
basenames are unique under `projects/synth/`, and each contains at least one
`test("name")` — so a parser reading case names from the whole line rescues
`ui-backend`; `file-export` names no case at all and `Makefile` is rescuable by
no reading.

**Rulings on the disputed count.** C2's "The defect is precisely these three
lines and nothing else" is **REFUTED** — the `Makefile` line fails on C's own
stated rule. E-ADV1's six is the correct enumeration under the rule as written.
D's two OPEN entries are **upheld as OPEN in disposition but not in kind**: the
rule does not settle the `.spec.ts` parse, and that underdetermination is part of
the same defect (see BLOCK-8). My ruling: **four certain, six under the rule as
written.**

**Blast radius, confirmed.**

```
$ sed -n '274p' External/Sheaf/projects/synth/Makefile | cut -c1-90
test: check-ui-boundary check-ui-boundary-empty-discovery check-app-bundle-plist test-wasm32 …
```

A linear prerequisite list: a red first prerequisite aborts the recipe and no
test binary runs. Sheaf task 1.5 baselines that target before the script exists,
so the baseline cannot show it.

**Remedy (condition on the artifacts).** Task 1.9 must dispose of all six lines
explicitly — by naming their repair as work in this change (with the operator's
assent to touch #13 and #14), or by scoping the script to the change under
execution and recording in design.md the ~101 lines that then go uncovered, or
by stating a named, justified exception in the script's own header for a
whole-binary check and for the path-plus-quoted-name form. Task 1.9's closing
claim ("Every `Check:` line … is written … to satisfy this rule") must be
narrowed: it is about this change's own deltas, and the script is not.

---

### BLOCK-8 — task 1.9's resolution rule is underdetermined; the `- [ ]` clause is fatal or inert depending on a choice no artifact makes

**Sources:** B3 ("certain red at 7.2"), E-ADV2 (underdetermination), F-ADV10
(SHOULD-FIX header, "Blocks delivery" body).
**Class:** C-BLANK. **Blocks execution** (at Sheaf 1.9 or 7.2, depending on the
filling). **CONFIRMED as a blank; B3's stated mechanism REFUTED in part.**

The rule and the convention:

```
$ sed -n '124,131p' External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md
      read from the file the line names; a script `projects/synth/Makefile`'s or
      `apps/miniapp/Makefile`'s own `test:` prerequisite list actually runs, …;
      or text beginning `none`, `operator step`, or `not yet delivered`, where a
      `not yet delivered` line must additionally name, in its own prose, a task
      number (`N.M`) present as a `- [ ]` item in this change's own `tasks.md`
$ grep -n 'Mark task complete' External/Sheaf/.claude/skills/openspec-apply-change/SKILL.md
78:   - Mark task complete in the tasks file: `- [ ]` → `- [x]`
$ sed -n '678,681p' External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md
      Also re-run task 1.9's `check_spec_checks_resolve.py` against this
      change's own final `Check:` lines — by now every "not yet delivered"
      line's named test either exists (resolving on its own name) or still
      correctly resolves on its task citation; confirm the exit code is `0`.
$ grep -n -i 'rewrit.*Check\|Check lines\|update the.*Check' External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md; echo exit=$?
exit=1
```

22 lines are governed, and **all 22 also carry a backticked test-shaped token**
(my own enumeration; zero exceptions):

```
$ python3 - (enumerating every "- Check: not yet delivered" line in the three deltas)
TOTAL deferred: 22
WITHOUT a test-shaped backticked token: 0
```

Three readings are available and the artifacts fix none:

1. **Pure disjunction** (the rule's literal grammar: "A line resolves if … X; …
   Y; or Z"). Green at 1.9 via branch 3, green at 7.2 via branch 1 — and the
   `- [ ]` strictness clause is then **inert for every line in this change**,
   voiding its stated purpose ("an undelivered claim cannot outlive the change
   that was supposed to deliver it").
2. **Prefix-first.** Green at 1.9, **red at 7.2 on all 22** — B3's outcome.
3. **This repository's own precedent**, the frogg3rs sibling script, which
   deliberately does *not* short-circuit on the prefix:

```
$ sed -n '374,379p' app/check_spec_checks_resolve.py
                # A `none` / `operator step` / `not yet delivered` prefix
                # declares that no automated check exists yet. It does NOT
                # license whatever follows: an earlier version returned
                # immediately on seeing it, so `Check: none, see
                # `a_fake_case`` passed with a false claim attached. The
                # declaration is accepted; anything it names is still resolved.
```

   Under that precedent the 22 lines are **red the moment task 1.9 wires the
   script in**, because the tests do not exist yet. (The frogg3rs deltas escape
   this only because their ten deferred lines deliberately leave the file and
   case names *un-backticked*; Sheaf's 22 backtick theirs.)

So B3's "certain red at 7.2" is **REFUTED as stated** — one of the three
readings never goes red — but the defect is worse than B3 described, because a
fourth possibility is red at group 1. The blank is the defect.

**Related, same rule:** two of the 22 lines carry tokens in the very shapes
BLOCK-7 rules unresolvable — `browser/tests/midi-timing.test.mjs` (a path with
no case) and
`projects/synth/juce/MidiConnectionManagerReconcileTests.cpp: EndpointOpen…` (a
path, not a bare basename). This change's own lines partly violate its own rule.

**Remedy (condition on the artifacts).** Task 1.9 must state the dispatch order
explicitly, say whether `- [x]` is accepted, say what happens to a line promoted
out of a change (its `tasks.md` moves under `archive/`, which the script
excludes), and carry a positive control for a `not yet delivered` line naming an
absent task — the only control it currently states exercises the test-name
branch. Whichever resolution is chosen must be written down, not left for the
executor.

---

### BLOCK-9 — the browser endpoint-open check is satisfiable by a clear that never reaches the browser, on the path the design itself calls wrong

**Sources:** E-ADV5; F-ADV6 (filed SHOULD-FIX, revised to BLOCKING in F's
exchange, deferring to E).
**Class:** C-DEAD-CHECK. **Blocks delivery.** **CONFIRMED.**

The check is specified as "asserted against `BrowserMidiBridge` by name so a
fake cannot satisfy it". Naming the type constrains *what*, never *where*:

```
$ sed -n '125,140p' External/Sheaf/projects/synth/include/synth/browser/BrowserMidiBridge.hpp
        synth::MidiEndpointOps ops;
        ops.openInput = [this](std::size_t ix, const std::string& identifier) {
            actions_.push_back({.type = ActionType::OpenInput, .controllerIx = ix, .identifier = identifier});
            return true;
        };
        ops.openOutput = [this](std::size_t ix, const std::string& identifier) {
            OutputSink* sink = OutputSinkFor(ix);
            …
            sink->Clear();
            sender->SetSink(ix, sink);
            actions_.push_back({.type = ActionType::OpenOutput, …});
            return true;
        };
```

Both are ordinary synchronous C++ lambdas reached from `ExecuteReconcilePlan`. A
clear placed in either makes the named case green with the entire browser seam
absent — no ABI entry point, no `midi-endpoint-open` `RuntimeCommand` variant,
no `worker.ts` dispatch case, no facade guard, no `midi.ts` call.

A clear in `ops.openInput` is additionally *wrong* in the way the design names,
because it fires at plan-execution time, before `applyAction` runs:

```
$ sed -n '228,234p' External/Sheaf/projects/synth/browser/src/midi.ts
  private applyAction(action: MidiAction, access: MidiAccess): void {
    switch (action.type) {
      case "open-input": {
        const port = this.inputFor(access, action.identifier);
        if (!port) return;
$ grep -n 'true no-op path' External/Sheaf/openspec/changes/midi-controller-resilience/design.md
355:guard just above it is not the reopen path, it is the true no-op path, and
```

The change's own design forbids what its check would accept.

**Overlooked alternative, confirmed.** A dedicated binary for this type already
exists, already lists the header, and is already in `test:` — while task 3.6
adds the prerequisite to a different binary whose rule omits the header:

```
$ sed -n '193p' External/Sheaf/projects/synth/Makefile
$(RECONCILE_EXECUTOR_TEST_BIN): tests/reconcile_executor_tests.cpp $(LIB) include/synth/MidiReconcile.hpp include/synth/MidiController.hpp
$ grep -n 'BROWSER_MIDI_BRIDGE_TEST_BIN' External/Sheaf/projects/synth/Makefile | sed -n '1,3p'
36:BROWSER_MIDI_BRIDGE_TEST_BIN := $(BUILD_DIR)/browser_midi_bridge_tests
235:$(BROWSER_MIDI_BRIDGE_TEST_BIN): tests/browser_midi_bridge_tests.cpp include/synth/browser/BrowserMidiBridge.hpp include/synth/Engine.hpp …
253:browser-midi-bridge-test: $(BROWSER_MIDI_BRIDGE_TEST_BIN)
```

**Remedy (condition on the artifacts).** The browser case must assert the clear
is reachable **only through the new ABI entry point** — drive that entry point,
not `Reconcile` — and must state where in the browser path the clear fires,
excluding the `if (!port) return;` no-op. F-ADV6's residual point stands
alongside and must also be closed: nothing asserts that the exported
`synth_browser_*` symbol's body reaches the bridge at all. The design must also
say why `tests/browser_midi_bridge_tests.cpp` is not the host for this case.

---

### BLOCK-10 — `DeviceLabel` does not enumerate `MidiEndpointStatus`; Sheaf task 1.2's STOP fires and design.md's stated reason is false

**Sources:** E-ADV10 (BLOCKING); F-ADV13 (filed NOTE, revised upward in F's
exchange).
**Class:** C-FROZEN (an unverified structural claim in an artifact) with a task
STOP attached. **Blocks execution** (Sheaf task 1.2). **CONFIRMED, and worse
than either report had it.**

```
$ sed -n '600,619p' External/Sheaf/projects/synth/src/MidiConfigViewModel.cpp | grep -nE 'DeviceLabel|status ==|return stored'
1:std::string DeviceLabel(const MidiEndpointRef& ref, MidiEndpointStatus status) {
9:    if (status == MidiEndpointStatus::Unconfigured) {
16:    if (status == MidiEndpointStatus::Offline) {
20:    return stored;
$ sed -n '12p' External/Sheaf/projects/synth/include/synth/MidiReconcile.hpp
enum class MidiEndpointStatus { Unconfigured, Offline, Online };
```

Two equality tests and a fall-through. A fourth enumerator takes the Online path
silently. Task 1.2 asserts the opposite and attaches a stop:

```
$ sed -n '/1\.2 Enumerate every `MidiEndpointStatus`/,/before any code/p' External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md | tail -4
      for the record, since it changes nothing this task needs to gate on. If
      the count of enumerating sites (the first three) is not three, the
      design's no-enumerator decision needs revisiting before any code.
```

**A further fact neither report established.** design.md's stated reason is that
adding an enumerator "would force all three dispatch sites to change". Nothing
forces any of them:

```
$ sed -n '444,448p' External/Sheaf/openspec/changes/midi-controller-resilience/design.md
**Mismatch is a new per-slot field, never a fourth enumerator.** Mismatch is
orthogonal to online and offline. Adding an enumerator would force all three
dispatch sites to change (`EndpointStatusColor`, `DeviceLabel`, the status
legend table) …
$ grep -n 'CXXFLAGS' External/Sheaf/projects/synth/Makefile | head -1
2:CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2
```

No `-Werror`. `DeviceLabel` falls through; the legend is a hand-written array
that would simply go short; and `EndpointStatusColor`'s switch has no `default:`
so `-Wswitch` warns rather than errors. The reason is false for all three sites,
not one.

**F's narrowing, recorded.** Task 1.2 uses "dispatch site" and "enumerating
site" interchangeably, and `DeviceLabel` *is* a dispatch site, so an executor
could answer "three dispatch sites, two exhaustive" without the STOP obviously
firing. The stop is keyed on "the count of **enumerating** sites (the first
three)", and that count is two; I rule the STOP fires. The falsified reason
holds under either reading.

**Remedy (condition on the artifacts).** Task 1.2 must state the count as two
exhaustive sites plus one non-exhaustive branch, using the vocabulary it already
applies correctly to the single-value guards at `:857`/`:872`. design.md's
no-enumerator paragraph must rest on a reason that is true — the orthogonality
argument is sound on its own; the "would force all three to change" clause must
go or be corrected, and must say that task 4.5's `-Wswitch` trap guards only its
own switch.

---

### BLOCK-11 — archiving promotes ten false "not yet delivered" `Check:` lines into frogg3rs's permanent spec

**Sources:** E-ADV12.
**Class:** C-UNDEFINED (a deferred claim outliving the change that deferred it).
**Blocks delivery.** **CONFIRMED on the mechanism; two of E's supporting facts
REFUTED, both in the direction that strengthens it.**

```
$ grep -h '^- Check:' openspec/changes/frogg3rs-midi-preset-preconditions/specs/*/spec.md | wc -l
      12
$ grep -h '^- Check:' openspec/changes/frogg3rs-midi-preset-preconditions/specs/*/spec.md | grep -c 'not yet delivered'
10
$ sed -n '96p' app/check_spec_checks_resolve.py
NO_CHECK = re.compile(r"^\s*(none|operator step|not yet delivered)", re.I)
$ grep -n 'openspec archive' openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md
554:      hygiene this change owes → `openspec archive frogg3rs-midi-preset-preconditions`
$ grep -n -i 'rewrit.*Check\|Check lines' openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md; echo exit=$?
exit=1
```

Ten lines of the form "not yet delivered; task 4.5 adds a loop-based case to …"
become permanent promoted spec text at task 6.6's archive, and are false the
moment the task is ticked. The gate accepts the prefix unconditionally, and no
task rewrites them.

**REFUTED (1): E's "nine of frogg3rs's twelve".** It is ten of twelve (the other
two are one real test case and one operator step). E's "31 lines" is 32. Same
frozen-count defect as BLOCK-1, this time inside an audit.

**REFUTED (2): E's "this convention has never survived an archive in either
repository".** True of Sheaf, false of frogg3rs:

```
$ grep -rh '^- Check:' openspec/specs | wc -l
      46
$ grep -rn '^- Check: *not yet delivered' openspec/specs | cut -c1-90
openspec/specs/froggers-sheaf-runtime-app/spec.md:252:- Check: not yet delivered as a resolvable case. …
openspec/specs/froggers-sheaf-runtime-app/spec.md:349:- Check: not yet delivered as a standing case. …
```

Reading those two strengthens the finding rather than weakening it: both were
written at promotion to explain what is outstanding ("as a resolvable case", "as
a standing case") and stay true afterwards. The repository's own precedent is
that a promoted deferred line must be written to survive archive. This change's
ten are of the other form.

**Remedy (condition on the artifacts).** A task before frogg3rs 6.6's archive
must rewrite every `Check:` line that will be promoted, either to name the test
it now has or to the surviving form the two promoted precedents already use. The
same obligation applies to Sheaf's 22 if ruling 4's Sheaf archive is performed
(see OPEN-5).

---

### BLOCK-12 — sru-63 has no defined behaviour for a position-addressed profile; three of seven shipping presets are misreported with every scenario green

**Sources:** F-ADV1. Reached by no other report.
**Class:** C-UNDEFINED. **Blocks execution** (the requirement must say what a
position-mapped association contributes before an implementer picks a
resolution). **CONFIRMED.**

sru-63 requires two sets, one of them "the addresses its active profile maps".
A Launchpad-kind profile carries no address at all — the validator forbids it:

```
$ sed -n '3516,3522p' External/Sheaf/projects/synth/src/MidiController.cpp
        if (kind == MidiProfileKind::Launchpad) {
            if (!association.launchpadPosition.has_value()) {
                return Fail(reason, "launchpad system-message entries must carry a launchpad position");
            }
            if (association.control.has_value()) {
                return Fail(reason, "launchpad system-message entries must not carry a control address");
            }
$ sed -n '3451,3452p' External/Sheaf/projects/synth/src/MidiController.cpp
        case MidiProfileKind::Launchpad:
            return MidiKindSupport{.encoders = false, .systemMessages = true, .analogs = false};
```

`association.control` is the only field that *is* an address, and encoders and
analogs are unsupported for the kind. The address exists only computed, at match
time, with **no channel**:

```
$ sed -n '992,997p' External/Sheaf/projects/synth/src/MidiController.cpp
        if (address.has_value() && association.launchpadPosition.has_value()) {
            const LaunchpadGridPosition& position = *association.launchpadPosition;
            const std::optional<std::uint8_t> mapped =
                LaunchpadPositionToNote(position.controller, position.x, position.y);
            if (mapped.has_value() && *mapped == address->cc) {
```

Three of the six shipping frogg3rs defaults (seven with the Launch Control XL)
are Launchpad-kind and position-addressed:

```
$ sed -n '329,336p' app/FroggersMidiCatalog.hpp
    catalog.deviceDefaults = {
        TwisterDeviceDefault(), Apc40GenericDeviceDefault(), Apc40AbletonDeviceDefault(),
        LaunchpadXDeviceDefault(), LaunchpadProMk3DeviceDefault(), LaunchpadMiniMk3DeviceDefault(),
    };
$ grep -n 'launchpadPosition' app/FroggersMidiCatalog.hpp
197:    association.launchpadPosition = synth::LaunchpadGridPosition{.controller = controller, .x = x, .y = y};
```

And no sru-63 scenario constructs the shape:

```
$ grep -in 'launchpad\|position' External/Sheaf/openspec/changes/midi-controller-resilience/specs/synth-runtime-ui/spec.md; echo exit=$?
exit=1
```

All six scenarios are written in channel/control terms ("channel 4 control 0",
"channel 4 controls 8 through 13"). So an implementation reading
`association.control` yields an empty mapped set for every Launchpad slot: the
not-transmitted set is permanently empty, every working pad press lands in the
*unmapped* set, and the page permanently accuses three of seven shipping presets
of disagreeing with their profile — the inverse of the requirement's purpose —
while all four named cases stay green.

This is the finding in the whole audit with the largest gap between "every
named check passes" and "the feature works".

**Remedy (condition on the artifacts).** sru-63 must state how a
`launchpadPosition`-mapped association enters the mapped set and with which
channel (the match path compares note number alone), and the delta must carry a
scenario plus a `controllers_page_ui_tests.cpp` case driving a Launchpad-kind
profile whose pressed pads must NOT appear in the unmapped set.

---

### BLOCK-13 — the manual-recovery half asserts presence only; the check cannot reject the sentence it was written against

**Sources:** F-ADV3 (BLOCKING); E-ADV3 (filed SHOULD-FIX, raised to BLOCKING in
E's exchange, deferring to F).
**Class:** C-DEAD-CHECK. **Blocks execution** (task 1.8's rule as written).
**CONFIRMED.**

The scenario's THEN has two clauses; the rule implements the first only:

```
$ sed -n '41,45p' openspec/changes/frogg3rs-midi-preset-preconditions/specs/froggers-midi-controller-mappings/spec.md
#### Scenario: A Shift whose release never arrives ends by another trigger, and the manual says which
- **WHEN** a controller is unplugged while its Shift button is down, or its Shift address stops transmitting
- **THEN** the manual's Shift subsection states the triggers that end a held modifier and does not state a recovery that requires the Shift address to transmit
- **AND** it states the same for Hold Drill
- Check: not yet delivered; task 1.8 adds … which fails unless the Shift and Hold Drill subsections each contain at least one of the multi-word recovery phrases …
$ grep -n -i 'must not contain\|absence\|does not contain' openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md; echo exit=$?
exit=1
$ sed -n '319,320p' MANUAL.md
with the patch like every other mapping. If the controller is unplugged while Shift is still held, its
buttons stay shifted until Shift is pressed and released again.
```

`MANUAL.md:319-320` is exactly the recovery the scenario forbids — one that
requires the Shift address to transmit. Nothing tests for absence, so it can
stay verbatim with one sentence appended and the check is green.

The two positive controls task 1.8 states do fire, because all four phrases are
absent today:

```
$ for p in "selecting a different preset" "rebuilds the row's mapping" "unplugging and reconnecting the controller" "clears automatically after"; do printf "%-45s %s\n" "$p" "$(grep -ci "$p" MANUAL.md)"; done
selecting a different preset                  0
rebuilds the row's mapping                    0
unplugging and reconnecting the controller    0
clears automatically after                    0
```

But proving the rule red against today's text is compatible with it being green
against today's text **plus one sentence**. F's three further accepting paths
follow from the same at-least-one-of-four substring rule: one phrase of four
suffices where the requirement says "trigger**s**"; a phrase inside a sentence
that *denies* the recovery passes; and the ceiling's duration is free text
joined to no constant.

**Remedy (condition on the artifacts).** Task 1.8's rule must carry an absence
half (reject "pressed and released again", and any sentence naming the
modifier's own button as the recovery), must require a phrase for each host
class the requirement distinguishes (task 3.1 requires the plugin be excluded
from the reconnect route, and nothing checks it), and must either derive the
ceiling figure from `kHeldModifierCeilingMicros` or drop the figure from the
manual.

---

### BLOCK-14 — Sheaf's `check_spec_checks_resolve.py` and frogg3rs's `check_spec_checks_resolve.py` are not the same gate, and BLOCK-7/BLOCK-8's rule is stated only in prose

This is not a separate defect; it is the note that BLOCK-7 and BLOCK-8 both
arise from one script specified entirely in a task paragraph, with a sibling
implementation in the other repository that resolves the same ambiguities
differently. Recorded here so the repair closes both together rather than
patching each symptom. **Not counted in the blocking total.**

---

## 2. SHOULD-FIX, consolidated

"Verified" means I ran the command myself. "Not adjudicated" means the brief's
priority did not reach it and it should be verified at the point of repair.

| # | Finding | Sources | Status |
|---|---|---|---|
| SF-1 | design.md:219-225's marker-word count is arithmetically impossible against the manual it says it was counted from: it claims "six of the seven device subsections", the manual has **six**, and the Twister scores 3 occurrences across 2 distinct markers, not "2". Stated as "verified by direct count against the live manual". Class C-FROZEN. | A3 | **Verified** (below) |
| SF-2 | `proposal.md:108-109` says the Programmer's Reference Guide states "the device's own MIDI channel is zero-indexed"; `design.md:429-431` says the one such sentence "is scoped to buttons, not faders or the device generally" and that "No sentence in either document states channel numbering for faders or pots". The two artifacts contradict each other about the same external sentence. | A4 | **Internal contradiction verified**; which side the vendor PDF supports → OPEN-4 |
| SF-3 | Sheaf task 1.9 states "the 14 lines that could not yet name a delivering task are fixed in tasks 3.6, 3.9, 4.1, 4.2, 5.1, 5.3 and 6.4". The tree has **22** lines naming **ten** tasks — 3.7, 4.5 and 4.6 omitted. Class C-FROZEN. | A11, B6, C3, E-ADV2 | **Verified** (below) |
| SF-4 | Five count comments state the catalogue size; task 5.5 names three. Unnamed: `app/Makefile:127` (a file this change edits for the new check target) and `app/FroggersMidiCatalogTests.cpp:8`, which is **already false today** — it says "the three device defaults" while its own `:399` asserts six. | A6, D2, E-ADV17 | **Verified** (below) |
| SF-5 | Two cited line ranges split a sentence, and tasks instruct edits keyed to exactly those boundaries: `FroggersMidiCatalog.hpp:19` carries both the tail of the third precondition and the head of the button-layout description (task 4.3 promotes `:14-18`, rewords `:19-24`); and task 4.4 cites `:26-31` where line 30 already begins the Ableton sentence — the Impact's own `:26-35` framing is the accurate one. | A7, B15, D9 | **Verified** (below) |
| SF-6 | The two proposals give incompatible accounts of the one field observation that motivates the pair. frogg3rs: an operator "lost three of six side buttons". Sheaf: "the five paired buttons did their shifted jobs". If three of six addresses moved, at most three can still dispatch. The triggers differ too. Nothing recorded can settle it. | A10 | **Verified** (below) |
| SF-7 | Neither new constant has a declaration site written down, and task 5.2's literal instruction is to set a C++ constant "in `design.md`". `grep -rn 'kHeldModifierCeilingMicros\|kTemplateChangeRateLimitPerSecond' External/Sheaf/projects/` → **0**. No header, no namespace, no unit conversion (design says "30 seconds", the name is micros). | B7 | **Verified** |
| SF-8 | design.md and tasks.md name **two different clocks** for the rate limiter. design.md:495-496: "`Engine`'s own `timestampProvider_`, `Engine.hpp:1462`". Task 5.2: "the same `MidiInProcessor::TimestampProvider` seam … the identical per-processor provider". The limiter lives inside a `MidiInProcessor` subclass with no `Engine` handle, so design.md's version is not implementable as written, and the two differ behaviourally (`NextTimestamp()` returns `0`, not an error, when unset). | F-ADV8 | **Verified** (below) |
| SF-9 | frogg3rs 4.1 says "Do not start the rest of this group until …"; 4.5 says "Does not depend on 4.1/4.2's gate"; 4.6 says "neither depends on 4.1/4.2's gate". Two written orderings for the same two tasks. | B8 | **Verified** (below) |
| SF-10 | The pin advance silently invalidates every line-numbered citation from `app/` into the submodule, and the citation gate is structurally blind to it (it verifies path existence only for pinned trees). | E-ADV13 | **Verified, count corrected** (below) |
| SF-11 | Sheaf ships two comments its own diff falsifies: `MidiController.hpp:244-248` ("Release clears held and drilled") and `:254-255` ("cleared by its release, read only by that profile's system-button processor"). design.md:311 names `:254-255` as rewritten; **no task carries that instruction**; `:244-248` is named by no artifact at all. | D4 | **Verified, with one narrowing** (below) |
| SF-12 | `projects/synth/docs/coverage.md`, the table task 7.6 extends, cites deleted files as its evidence: `miniapp-smoke.spec.ts` appears **8 times** and neither it nor `EncoderComponentGeometryTests.cpp` exists. | D5 | **Verified** (below) |
| SF-13 | The drift half's empty-region comparison is demonstrated by neither positive control. Four of seven devices (three Launchpads + the APC40 Ableton) carry markers bounding "explicitly empty generated text" (design.md:271-274); both of task 4.8's controls act on the Twister, the one device with non-empty declared text — while task 4.7 asserts "task 4.8's check confirms that agreement too". Class C-DEAD-CHECK. | F-ADV4 (BLOCKING header/"blocks execution" body; **I rule SHOULD-FIX** — nothing executes wrongly, the repair is one added control in task 4.8's own list, and what breaks is an unproven claim in an artifact, the same shape as SF-1) | **Verified** (below) |
| SF-14 | The Sheaf overlap table reports other changes' state by counting unticked boxes. `shorten-deadline-readout-window` is tabled "0/6 … Note only" while its work has landed: `80d9f4bb` is an ancestor of HEAD and `RollingMax` (not the `RollingMax256` its task 1.1 would rename) sits at `MidiConfigViewModel.hpp:40`, a file this change extends. | D6 | **Verified** (below) |
| SF-15 | frogg3rs 6.6's `openspec archive` is interactive while task 5.4 is still unticked by design (it is an operator step sequenced after the merge). `openspec archive --help` offers `-y/--yes` and `--json`; 6.6's command text carries no flag. | B10 | **Verified** (`--help` output confirmed) |
| SF-16 | The frogg3rs Overlap section closes at three Sheaf changes; `launchpad-model-on-the-row` carries an open frogg3rs-side task naming the same catalogue file and the same submodule pin 4.2 claims sole ownership of. D found the work already landed, so no collision exists in fact — what is missing is the path-scoped sweep that would have established it. | D3 | Not adjudicated — verify at the point of repair |
| SF-17 | The Sheaf Impact says the top-level Makefile does not run the JUCE tests; it does, conditionally (`projects/synth/Makefile:305-309` delegates when `$(JUCE_DIR)/modules` exists). Nothing breaks, because task 1.5 runs both targets explicitly and the carried deadline failures abort the recipe first. | D8 | Not adjudicated |
| SF-18 | `MANUAL.md:339`'s rationale twin of the sentence task 4.3 rewrites is named by no task, and task 4.7's marker boundary around it is undefined — `:339` is rationale prose no declaration list can generate. Adjacent to BLOCK-13; fold both into one repair. | D10 | Not adjudicated |
| SF-19 | Sheaf task 3.7's and 3.9's multi-trigger cases are sited in `instrument_tests.cpp`, which constructs no `Engine` and passes a constant clock, so the Ceiling and EndpointOpen branches are unreachable there. Composite with BLOCK-6: **no binary in the tree can both drive the ceiling and observe the result.** | E-ADV4 (+ E's exchange composite with C1) | Not adjudicated — but the BLOCK-6 remedy must close it |
| SF-20 | smi-17's flood scenario asserts a starvation that cannot occur (the input chain is synchronous chain-of-responsibility; the test delivers one message at a time), and bounds "observer notifications" from an observer that does not exist. | E-ADV7, F-ADV9 | Not adjudicated |
| SF-21 | The new ABI export check is one-directional: it derives `synth_browser_*` definitions from `BrowserRuntimeAbi.cpp` and asserts membership in `EXPORTED_FUNCTIONS`, but nothing ties the TypeScript literal property names in `emscriptenRuntimeFacade` to the C++ definitions. | E-ADV6, F-ADV6 residual | Not adjudicated |
| SF-22 | The drift half's floor is one-directional (an orphan marker pair for a removed device is not named as a failure) and undefined when the emitter is dead (a non-zero emitter exit compares nothing and passes). | E-ADV11 | Not adjudicated |
| SF-23 | The settings prose is duplicated outside the markers and the duplicate is only a WARNING, so editing the operator-facing copy produces a warning and exit 0. | F-ADV5 | Not adjudicated |
| SF-24 | `npm run test:unit` is defined in `browser/package.json` and invoked by no Makefile target and no CI workflow, so the anti-drift mechanism this change builds is a one-shot manual observation rather than a gate. | F-ADV7 | Not adjudicated |
| SF-25 | Task 3.9's empty-timestamp-provider case asserts the opposite of what the ceiling condition does, and is green only because the injected clock reads `0`. | E-ADV8, F-ADV12 | Not adjudicated |
| SF-26 | sru-63's SHALL-render clause has no scenario and no `Check:` line, so postflight 7.3 passes vacuously over it. | F-ADV2, E-ADV16 | Not adjudicated |

### Verification detail for the SHOULD-FIX items I ran

**SF-1.**
```
$ awk 'NR>=258 && NR<=390 && /^### /{print NR": "$0}' MANUAL.md | sed -n '8,13p'
322: ### MIDI Fighter Twister
341: ### Akai APC40 mkII (Generic)
353: ### Akai APC40 mkII (Ableton)
359: ### Launchpad X
371: ### Launchpad Pro MK3
378: ### Launchpad Mini MK3
$ python3 (counting must / set to / unchecked / stay selected / Utility per subsection)
Twister                lines 322-340: total=3 {'unchecked': 1, 'Utility': 2}
APC40 Generic          lines 341-352: total=0 {}
APC40 Ableton          lines 353-358: total=0 {}
Launchpad X            lines 359-370: total=0 {}
Launchpad Pro MK3      lines 371-377: total=0 {}
Launchpad Mini MK3     lines 378-383: total=0 {}
device subsections: 6
```
Six subsections; five others at zero, not six; Twister scores 3.

**SF-3.** My own enumeration of the 22 deferred lines (printed under BLOCK-8)
yields the task set `{3.6, 3.7, 3.9, 4.1, 4.2, 4.5, 4.6, 5.1, 5.3, 6.4}` — ten.
The `29 = 8 + 18 + 3` total in the same sentence is correct.

**SF-4.**
```
$ grep -rn 'six real device defaults\|six device defaults\|three device defaults\|six shipping defaults' app/ MANUAL.md README.md
app/Makefile:127:# six real device defaults, the catalog Sheaf's own Controllers page tests
app/FroggersMidiCatalog.hpp:7:// hold drill, shift), and the six device defaults offered from the
app/FroggersControllersPageTests.cpp:3:// FroggersMidiCatalog()'s six real device defaults (the MIDI Fighter
app/FroggersControllersPageTests.cpp:7:// descriptor, so none of them ever drive these six shipping defaults
app/FroggersMidiCatalogTests.cpp:8:// routes and nothing else; and the three device defaults validate against the
$ sed -n '399p' app/FroggersMidiCatalogTests.cpp
    REQUIRE_TRUE(catalog.deviceDefaults.size() == 6);
```
Task 5.5 names `FroggersMidiCatalog.hpp:6-12` and `FroggersControllersPageTests.cpp:1-4` and `:7`. Both unnamed sites are in files this change edits.

**SF-5.**
```
$ cat -n app/FroggersMidiCatalog.hpp | sed -n '18,19p'
    18	// Bank Side Buttons unchecked so the side buttons keep this default's CC
    19	// addresses whatever Twister bank is lit. The six side buttons are five
$ cat -n app/FroggersMidiCatalog.hpp | sed -n '30p'
    30	// pressed again. The Ableton default exists to avoid that caveat: it
$ grep -n '26-31\|26-35' openspec/changes/frogg3rs-midi-preset-preconditions/{tasks,proposal}.md
tasks.md:292:      from `app/FroggersMidiCatalog.hpp:26-31`; the Ableton default declares an
proposal.md:183:  caveat comment at `:26-35` split between the Generic and Ableton defaults;
```

**SF-6.**
```
$ sed -n '11,14p' openspec/changes/frogg3rs-midi-preset-preconditions/proposal.md
An operator whose Twister had "Bank Side Buttons" on lost three of six side
buttons: the device moved their CC addresses, and the app reported nothing. …
$ sed -n '19,22p' External/Sheaf/openspec/changes/midi-controller-resilience/proposal.md
Observed on a MIDI Fighter Twister whose side buttons were not set to CC Hold
and whose "Bank Side Buttons" option was on: the Shift address stopped
transmitting entirely, the profile stayed shifted, and the five paired buttons
did their shifted jobs with no way back.
```

**SF-8.**
```
$ sed -n '494,496p' External/Sheaf/openspec/changes/midi-controller-resilience/design.md
… The limiter reads elapsed time
from the same source the held-modifier ceiling reads (`Engine`'s own
`timestampProvider_`, `Engine.hpp:1462`, monotonic microseconds), …
$ sed -n '573,577p' External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md
      a burst and bounding observer notifications. It reads elapsed time from
      the same `MidiInProcessor::TimestampProvider` seam
      `TemplateChangeMidiInProcessor` is constructed with — the identical
      per-processor provider `EncoderMidiInProcessor`/`SystemButtonMidiInProcessor`
      already take …
```

**SF-9.** `tasks.md:238` "Do not start the rest of this group until"; `:312`
"Does not depend on 4.1/4.2's gate"; `:321` "neither depends on 4.1/4.2's gate".

**SF-10, count corrected.**
```
$ grep -rhoE "External/Sheaf/projects/synth/[A-Za-z0-9_/]*\.(cpp|hpp):[0-9]+" app/ | wc -l
     201
$ grep -rhoE "External/Sheaf/projects/synth/include/synth/Engine\.hpp:[0-9]+" app/ | sort -u | tail -3
External/Sheaf/projects/synth/include/synth/Engine.hpp:622
External/Sheaf/projects/synth/include/synth/Engine.hpp:628
External/Sheaf/projects/synth/include/synth/Engine.hpp:907
$ sed -n '570p' External/Sheaf/projects/synth/include/synth/Engine.hpp
        for (MidiControllerProfileResult& processors : midiProcessors_) {
$ python3 app/check_citations_resolve.py app
check-citations-resolve: OK - 95 commit-pinned, 277 into pinned or frozen trees, 0 unresolvable, 0 line-numbered into this tree, 0 split across two lines
```
**201**, not E's 157 — another instance of the frozen-count class, this time
inside an audit report. Every cited `Engine.hpp` line above sits below task
3.4's insertion point at `:570`; every cited `runtime/Runtime.hpp` line (100,
230, 343, 500, 580, 974, 975) sits below task 1.8's edit at `:9-10`.

**SF-11, with a narrowing.** Both comments verified present and falsified by the
diff. `grep -n '254-255' …/tasks.md` → exit 1; `design.md:311` names it.
**Narrowing:** D4 says the Impact's `MidiController.hpp:249-258` range "excludes
both doc comments". It excludes `:244-248` but **includes** `:254-255` (the
struct bodies are `:249-252` and `:256-258`, with the ShiftState comment at
`:254-255` inside the cited range). The finding survives on `:244-248` and on the
missing task.

**SF-12.**
```
$ grep -c 'miniapp-smoke.spec.ts' External/Sheaf/projects/synth/docs/coverage.md
8
$ find External/Sheaf/projects -name 'miniapp-smoke.spec.ts' -o -name 'EncoderComponentGeometryTests.cpp' | wc -l
       0
```

**SF-13.**
```
$ sed -n '271,274p' openspec/changes/frogg3rs-midi-preset-preconditions/design.md
own `id` … — one pair per entry in `catalog.deviceDefaults`, including the three Launchpads and the Ableton
default, whose markers bound explicitly empty generated text (a stated
absence, not an unmarked one).
```
Task 4.8's two controls: "(a) temporarily append a word to **the Twister's**
declared CC Hold string … (b) temporarily edit the text inside `MANUAL.md`'s
**Twister** marker pair itself".

**SF-14.**
```
$ grep -rn 'RollingMax256' External/Sheaf/projects/ | wc -l
       0
$ grep -n 'struct RollingMax' External/Sheaf/projects/synth/include/synth/MidiConfigViewModel.hpp
40:struct RollingMax {
$ git -C External/Sheaf merge-base --is-ancestor 80d9f4bb HEAD && echo "80d9f4bb is ancestor"
80d9f4bb is ancestor
$ grep -n 'shorten-deadline-readout-window' External/Sheaf/openspec/changes/midi-controller-resilience/proposal.md
99:| `shorten-deadline-readout-window` | 0/6 | deltas `synth-runtime-ui`, reads `uiFrameHz` | Note only; the ceiling is wall-clock, not frame-counted. |
```

---

## 3. NOTE, consolidated

| # | Finding | Sources | Status |
|---|---|---|---|
| N-1 | The `check-modified-requirements-restate-promoted` NOTE is real, expected and correctly disclosed; the delta's own opening paragraph declares that scenario reversed deliberately. The accepting path it reveals (a dropped promoted scenario lands in `notes`, never `errors`) is real for any future change. | A8, E-ADV14, F-ADV19 | Verified by running the gate: `NOTE` line then `OK - 5 MODIFIED requirement(s), 134 promoted clause(s) …`, exit 0 |
| N-2 | The overlap section's account of the main checkout is stale. **Superseded by §0**: `main` has moved by three commits, the archive completed, and the third worktree has been removed. The pasted `git worktree list` at `proposal.md:151-155` is now wrong on all three rows. | A9, D7 | Verified, and now worse than reported |
| N-3 | A consistent class of truncated line ranges in Sheaf's `design.md`/`tasks.md` (four instances; none points past EOF). Matters only where a task instructs an edit keyed to the range — SF-5 applied to the Sheaf half. | A12 | Not adjudicated |
| N-4 | `design.md:234` cites the receiving side of the browser tick seam (`worker.ts:648`) as the posting side; the post is at `main.ts:360`. | A13 | Not adjudicated |
| N-5 | frogg3rs 4.6's Sheaf line citations are read against the pre-advance pin and will have moved when the task runs. Narrow case of SF-10. | B11 | Not adjudicated |
| N-6 | Two windows in which the change's own gate is deliberately red (task 1.8→3.1, task 4.8 floor→5.6), both load-bearing for a later positive control. Correct as designed; recorded so neither red reads as a regression. | B13 | Not adjudicated |
| N-7 | Exactly one task in either repository needs hardware or an operator (frogg3rs 5.4), and it is sequenced after the operator's merge. Nothing in this cycle proves the reading it will take is live. | B14 | Consistent with operator ruling 1 |
| N-8 | `ResyncDoesNotClearAHeldModifier` cannot fail (no code path from resync to modifier state), and the ruling it checks is applied to one of two `resync` bindings. | E-ADV15 | Not adjudicated |
| N-9 | The `-Wswitch` guard is an accident tripwire only; and the WARNING scan opens with a standing false positive (`off`, from "DEVICE ON/OFF is Randomize Page") so it can never fail. | E-ADV18, F-ADV13 residual | Partly folded into BLOCK-10 |
| N-10 | Task 4.6's positive control turns its cases red for a reason unrelated to the failure mode, and the MODIFIED requirement's stated reason for CC Hold is true of one of six side buttons. | F-ADV14 | Not adjudicated |
| N-11 | The gates opening frogg3rs groups 3 and 4 are substring greps: a comment or a TODO satisfies both. | F-ADV15 | Not adjudicated |
| N-12 | "add `projects/synth/browser` to the sweep" describes work already done (`version-drift.test.mjs` already sweeps from `projects/`), and invites a narrowing that would drop coverage. | F-ADV16 | Not adjudicated |
| N-13 | The template-change recognizer and both its checks share one unverified shape; disclosed in design.md and the Open Questions. | F-ADV17 | Disclosed; consistent with the operator's ruling |
| N-14 | Sheaf task 2.5's archive gate matches a string, and guards a step this cycle never takes. | F-ADV18 | Folded into OPEN-5 |
| N-15 | The browser clear is fire-and-forget relative to the port binding: `applyAction` is synchronous, `reportEndpointOpen` returns `Promise<void>`, and `port.onmidimessage` is assigned one line earlier. | E-ADV19 | Not adjudicated |

---

## 4. OPEN

| # | Open item | What settles it | Operator needed? |
|---|---|---|---|
| OPEN-1 | Whether CC 77 / channel 8 / factory template 1 is what the hardware actually does. | The operator's post-merge check on the live site (task 5.4). Ruled already (ruling 1). Nothing in this change's verification surface can reject a wrong value — design.md says so itself. | Yes, as ruled |
| OPEN-2 | Whether **any** compiling gate is green. Nothing was built by any of the six auditors or by me: `check-no-juce`, `check-delay-capacity-break-proofs`, all twelve frogg3rs test binaries, `make -C projects/synth test`, the miniapp JUCE target, `browser-midi-bridge-test`. | One `-j2`-capped build of each named target. Six reports and this adjudication all disclose it; it is the largest unmeasured surface in the audit. | No, but it must happen before any claim that a baseline is green |
| OPEN-3 | Whether `npm run test:unit` / `version-drift.test.mjs` passes. `projects/synth/browser/node_modules` is absent. | `npm ci` (approved, ruling 3) then the run. | No |
| OPEN-4 | What the Programmer's Reference Guide actually says about channel↔template and about zero-indexing (SF-2, and A5's claim that the artifacts reject a vendor corroboration they hold). | Reading the PDF. I did not: the brief scoped my reading of the scratchpad to the six reports, six exchanges and `lcxl-slider-disassembly.txt`. A's full page-by-page read is on the record but is not mine, and A4/A5 disagree with `proposal.md` about the same sentence. | Yes, or an explicit authorisation to read the PDFs |
| OPEN-5 | **Operator ruling 4 says `openspec archive` runs "in each repository"; Sheaf has no archive task and cannot archive.** `grep -n 'openspec archive' External/Sheaf/…/tasks.md` returns only task 2.5's gate wording; task 7.7 ends at "Verify a clean tree and stop there"; and 2.5's gate is unsatisfiable today — `grep -n "smi-14\|smi-16" External/Sheaf/openspec/specs/synth-midi-instrument/spec.md` → exit 1, and `app-midi-catalog` and `shift-and-file-export` are both still live change directories. | The operator saying whether Sheaf archives this cycle (which requires #13 and #14 to archive first) or whether ruling 4's "each repository" means frogg3rs only. | **Yes** |
| OPEN-6 | Whether the two `.spec.ts` `Check:` lines fail Sheaf's new script. Depends on a parser choice task 1.9 leaves open. | Writing the script — which is execution. The underdetermination is itself BLOCK-8's defect and must be closed in the artifact first. | No |
| OPEN-7 | Whether any other recorded figure moves once the branch is rebased onto current `main` (§0). | Rebase, then re-run every recorded gate. Required before execution regardless of BLOCK-1. | No |

---

## 5. Verdict

**REJECT**, for the pair.

Fourteen blocking findings confirmed, zero refuted, zero open among the blocking
set. Of these, four alone would be disqualifying:

- **BLOCK-12** — three of seven shipping presets are misreported by a
  requirement that has no account of the shape they use, with every named check
  green. The largest gap in the audit between "all checks pass" and "the feature
  works".
- **BLOCK-2** — the one figure the preset ships to hardware is carried by no
  recorded command output, and the recorded command is structurally incapable of
  having produced it. The remedy is now a paste, not a measurement.
- **BLOCK-6** — three shipped `Check:` lines name a test file with no path to
  what they assert, and the obvious fill is an unbriefed production-header
  change.
- **BLOCK-9** — the change's headline browser goal is satisfiable by a clear
  placed where the design itself says firing it would be wrong, with the entire
  browser seam absent.

Two more stop the executor on the artifacts' own written stop conditions before
any code is written (BLOCK-1 at frogg3rs task 1.2, BLOCK-10 at Sheaf task 1.2),
and one makes Sheaf's whole `make test` red on arrival for reasons outside this
change (BLOCK-7).

Three belong to the delivery mechanics and are cheap but load-bearing: the
confirmed object is not the pinned object (BLOCK-3), the gate that admits the
pin is weaker than the work it admits and detaches the checkout it is taken from
(BLOCK-4), and the pinned commit exists in exactly one object store on this
machine — one that a `git worktree remove` has already destroyed once during
this session (BLOCK-5).

Two are checks that cannot reject what they were written against (BLOCK-8,
BLOCK-13), and one promotes ten false sentences into a permanent spec
(BLOCK-11).

None of the fourteen requires redesigning anything. Every remedy is a condition
on an artifact.

**What is right, and is worth naming.** The citation discipline in both Impact
sections is high: I opened a large number of cited line ranges and symbols and
found them accurate, and the mechanical range floor over every citation in both
changes returns no out-of-range reference. The Sheaf design does not hide its
unmeasured constants — it declares the ceiling and the rate limit as this
change's own reversible defaults, states that nothing documents a real device's
template-change rate, and names which single test case pins the ceiling's
magnitude. The forward enumeration of created names is complete and every name
is genuinely unoccupied. Fourteen artifact gates across both repositories exit 0.
The defects are concentrated in how the artifacts **record** what was read and in
where checks are **placed** — not in what was read.

---

## 6. Coverage note

**Confirmed blocking findings reached by more than one report independently (6):**

| Finding | Independent arrivals |
|---|---|
| BLOCK-1 (stale gate literal) | **six** — A1, B1, C4, D1, E-ADV9, F-ADV11 |
| BLOCK-7 (new gate red on arrival) | three — C2, E-ADV1, F |
| BLOCK-8 (the `- [ ]` clause / dispatch order) | three — B3, E-ADV2, F-ADV10 |
| BLOCK-9 (browser endpoint-open clear) | two — E-ADV5, F-ADV6 |
| BLOCK-10 (`DeviceLabel` does not enumerate) | two — E-ADV10, F-ADV13 |
| BLOCK-13 (recovery half, presence only) | two — F-ADV3, E-ADV3 |

**Confirmed blocking findings reached by exactly one report (8):**

| Finding | Sole reporter |
|---|---|
| BLOCK-2 (CC 77 carried by no recorded command) | **A** |
| BLOCK-3 (confirmed object ≠ pinned object) | **B** |
| BLOCK-4 (partial gate; detached shared checkout) | **B** |
| BLOCK-5 (pinned commit in one private object store) | **B** |
| BLOCK-6 (`engine_tests.cpp` cannot observe) | **C** (E-ADV20 reached the structural fact and graded it NOTE) |
| BLOCK-11 (archive promotes false Check lines) | **E** |
| BLOCK-12 (sru-63 vs position-addressed profiles) | **F** |
| SF-13 / BLOCK-14-adjacent (drift half, empty regions) | **F** |

Eight of fourteen blocking findings rest on a single reporter, and the two
highest-cost ones (BLOCK-12 and BLOCK-2) are among them. Convergence was
concentrated on the cheapest defect in the set: six auditors found the stale gate
literal, one found the requirement that misreports three of seven shipping
presets.

**Reporter-level note.** Every report reached the correct verdict (REJECT). D1
alone got BLOCK-1's cause right on the first pass. Two auditors carried the same
frozen-count defect they were auditing into their own reports (E's "nine of
twelve" → ten; E's "157 citations" → 201 by my run; C's "precisely these three
lines and nothing else" → at least four). Nobody opened the `.pyc`, and nobody
built anything: OPEN-2 is unaddressed by all seven contexts including this one.
