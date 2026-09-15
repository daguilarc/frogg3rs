# Preflight 12 — adjudication

Adjudicated by a context that wrote neither the change, the briefs, nor any
report. Every ruling below is my own command against the trees or my own read of
the artifact, with the literal output inline. Agreement between reporters is not
evidence here; where I could not reproduce a claim I say so.

**Trees.** frogg3rs worktree `/Users/diegoaguilar-canabal/Desktop/frogg3rs/.claude/worktrees/midi-resilience`
at `d640e18439128fceac833d82942e23b3b1768ecc`, working tree clean (0 lines from
`git status --porcelain`). `External/Sheaf` at
`a07f2222d4f2d2b7d4f0275133573722abbf7d8a`, clean. Nothing was edited, built,
installed, committed or stashed. One scratch tree (copies of
`check_spec_checks_resolve.py`, `check_citations_resolve.py`, `check_common.py`
plus synthetic Makefiles and spec files) was built under the scratchpad for the
resolver probes and deleted afterwards.

**Baseline both changes still clear:**

```
$ openspec validate frogg3rs-midi-preset-preconditions --strict
Change 'frogg3rs-midi-preset-preconditions' is valid          EXIT=0
$ (in External/Sheaf) openspec validate midi-controller-resilience --strict
Change 'midi-controller-resilience' is valid                   EXIT=0
```

---

# 1. Verdict

**REJECT.**

Eleven blocking defects are confirmed, ten of them consolidated from the reports
and one reached only here. Four are single-command demonstrations. Task 6.6 —
the gate that stands between postflight and `openspec archive` — carries four
independent defects and cannot be patched clause by clause; §5 states the single
condition its rewrite must satisfy.

| # | Finding | Sources | Blocks |
|---|---|---|---|
| 1 | Task 6.6's Check loop reads a commit range the change's work is not in | B1 | execution of 6.6, delivery |
| 2 | Task 6.6's prescribed Check-line form is ambiguous and its stated justification is false | C1, F-ADV1 | execution of 6.6/6.7, delivery |
| 3 | Task 6.6's script-line test never reads the line it is checking; four device scenarios share one grep | E-ADV3, F-ADV1(2nd half) | delivery |
| 4 | Task 6.6 attributes the new target's wiring to task 1.6, which cannot have done it | *(this ruling)* | execution of 6.6 |
| 5 | The emitter's output format and the generated heading are blanks task 4.8 is sent to a section to read | B3 | execution of 4.7/4.8 |
| 6 | Task 4.2's stated citation backstop reads neither line numbers nor the files carrying the citations | E-ADV1, F-ADV2 | execution of 4.2 |
| 7 | Task 4.1(b) prints its pass value for a path that does not exist | E-ADV8 | execution of groups 3/4/5 |
| 8 | `design.md` says a moved address still dispatches; the cited code drops it | A1 | delivery |
| 9 | Sheaf's Impact and §8.0 sweep both miss `projects/synth/Makefile`, and the Impact never names the gate task 1.9 creates | A2, D1 | delivery |
| 10 | Sheaf task 1.3 asserts a site count no written predicate produces | B2 | execution of 1.3 |
| 11 | Sheaf task 3.7's two clear-asserting cases pass when the modifier was never held | F-ADV3 | delivery (ships smi-16's control vacuous) |

Refuted after verification: **none of the eleven.** One sub-claim inside #9 and
one inside #11 are narrowed; the narrowings are recorded and do not overturn
either finding. B2's blocking grade was disputed to NOTE by A and to SHOULD-FIX
by C and E; F-ADV3's was disputed to SHOULD-FIX by D. I rule on both below and
keep the dissenting positions on the record.

---

# 2. Confirmed blocking findings

Each entry names the class, what breaks, and a remedy stated as a condition on
the artifacts rather than as an edit.

## BLK-1 — Task 6.6's Check loop reads a commit range the change's work is not in yet

**Sources:** B1. **CONFIRMED.** **Class:** a check that cannot observe the thing
it measures — silent by construction.

Task 6.6's loop is a two-dot commit-range diff:

> for the eight naming a case, `git diff <base>..HEAD -- app/FroggersMidiCatalogTests.cpp | grep '^+' | grep -F '<case>'` must return at least one line […]
> for the two naming the script, `git diff <base>..HEAD -- app/check_docs_match_device_preconditions.py | grep '^+' | wc -l` must report a nonzero count.
> *(tasks.md:955-966)*

Run at HEAD, with 6.6's own `<base>`:

```
$ B=$(git merge-base main HEAD); echo $B
188b109d9f32b870d7f6f7ea4cc08d9fb08a5fff
$ git diff $B..HEAD --name-only -- app/ | wc -l
       0
$ git diff $B..HEAD -- app/check_docs_match_device_preconditions.py | grep '^+' | wc -l
       0
$ git diff $B..HEAD -- app/FroggersMidiCatalogTests.cpp | grep '^+' | grep -F 'device_defaults_declare_their_preconditions' | wc -l
       0
```

The range holds artifacts only — `External/Sheaf`, `design.md`, eleven
`preflight*.md`, `proposal.md`, the spec delta and `tasks.md`. No `app/` file is
in it.

The sequencing is what makes it bite. I searched the whole frogg3rs task list for
a commit action ordered before 6.6 and found none; the sole hit is inside 6.7:

```
$ grep -nE 'git commit|→ commit' openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md
993:      → commit → push branch `worktree-midi-resilience` to `origin` and
```

and 6.7's order is `postflight (6.1-6.5) → documentation hygiene → 6.6's
Check-line rewrite passes → openspec archive → commit → push`. Task 4.2 stages
only — `git -C External/Sheaf checkout <…> && git add External/Sheaf`,
"naming the exact commit SHA in the commit message" (tasks.md:506-509) — and
even on the most generous reading that it commits, tasks 4.3, 4.4, 4.5, 4.6, 4.7,
4.8, 5.1, 5.2 and 5.5 all run after it.

**What breaks.** At 6.6's stated run time all ten scenarios return nothing. 6.6
says "one silent scenario is 6.7's own STOP condition, not merely a warning
here", so the executor either stops on all ten or ticks a check that measured
nothing. **Blocks execution of 6.6 and therefore delivery.**

**Remedy (condition).** 6.6's loop must read a diff that contains the work it is
checking at the moment 6.6 runs — either the task list names a commit point ahead
of 6.6, or the loop's diff is not a commit range. Whichever is chosen, BLK-3
becomes live the instant this is fixed (see §5): they must land in one edit.

## BLK-2 — Task 6.6's prescribed Check-line form is ambiguous, and its stated justification is false

**Sources:** C1 (withdrawn as BLOCKING by its author in the exchange, restated
SHOULD-FIX), F-ADV1. Confirmed independently by A, B, D, E. **CONFIRMED as
BLOCKING.** **Class:** an instruction whose stated mechanism the tree contradicts.

*This is the dispute I was asked to settle: what task 6.6 actually prescribes,
and what the real script does with each form.*

**What 6.6 prescribes.** Both senses of "bare" appear in one paragraph
(tasks.md:934-947). For the eight case lines:

> in the form `app/FroggersMidiCatalogTests.cpp: <case>` this delta's own
> already-resolved lines already use **(a bare basename is not accepted)**.

Here "bare" means *path-less*. Six lines later, for the two script lines:

> so rewrite each to name that script **bare, with no case after a colon** —
> `check_spec_checks_resolve.py` resolves a bare script name as a wired gate
> (task 1.6 records it as `check-docs-match-device-preconditions` in
> `app/Makefile`'s `test:` prerequisite list), and naming a case beside it
> would assert something that file does not define.

Here the appositive glosses "bare" as *case-less*. The paragraph therefore
carries two incompatible senses of its own operative word, and the reading is
load-bearing: one of the two forms turns a `test:` prerequisite red.

**What the real script does with each form.** I built a minimal tree from copies
of `app/check_spec_checks_resolve.py` and `app/check_common.py`, with
`app/Makefile` shaped as tasks 1.8/4.8 leave it (the target in `test:`, its
recipe naming `$(APP_DIR)/check_docs_match_device_preconditions.py`) and the
script file present, and ran the real resolver against each form:

```
--- FORM: `check_docs_match_device_preconditions.py`          (path-less)
check-spec-checks-resolve: FAIL - openspec/specs/demo/spec.md:3 names nothing that resolves: `check_docs_match_device_preconditions.py`
check-spec-checks-resolve: FAIL - 1 unresolvable Check reference(s)
EXIT=1
--- FORM: `app/check_docs_match_device_preconditions.py`      (path, case-less)
check-spec-checks-resolve: OK - 1 Check reference(s) resolved, 0 declared as having no automated check
EXIT=0
--- FORM: none. nothing automated reads this yet.
check-spec-checks-resolve: OK - 0 Check reference(s) resolved, 1 declared as having no automated check
EXIT=0
```

Two negative controls prove the probe discriminates rather than passing
everything:

```
=== NEG CONTROL 1: target wired into test:, script file absent ===
check-spec-checks-resolve: FAIL - …:3 names nothing that resolves: `app/check_docs_match_device_preconditions.py`
EXIT=1
=== NEG CONTROL 2: script file present, target NOT wired into test: ===
check-spec-checks-resolve: FAIL - …:3 names only context, no case and no wired gate: `app/check_docs_match_device_preconditions.py`
EXIT=1
=== POS CONTROL restored ===
EXIT=0
```

The mechanism is in the source. `gate_scripts()` stores
`os.path.normpath(os.path.join(app_rel, name))` — always `app/<name>`
(`check_spec_checks_resolve.py:291`). `part_resolves` is `part in tests or part
in FILES` (`:333`), and `check_common.path_index`'s docstring states
"Basenames are deliberately not indexed […] The index is also the only way a
path is accepted". The unresolvable branch `bad = [t for t in claims if not
resolves(t, tests)]` with `continue` (`:412-416`) fires **before** `gate_here`
(`:461`) is ever evaluated, so a path-less token never reaches the gate test.

**So both positions in the dispute are partly right, and the finding survives
either way.** C is right that the appositive defines "bare" as case-less, and
right that the `app/`-prefixed case-less form resolves. F is right that the
paragraph's own adjacent usage makes the path-less reading available, and that
the path-less form turns the gate red — which I reproduced. What settles it
above both readings is that **the sentence's stated justification is false as
written under either reading**: the resolver does not resolve "a bare script
name"; it resolves an indexed repo-relative path that is also wired into
`test:`, and requires both (neg controls 1 and 2). A task that justifies a form
by a mechanism the script does not have is not repairable by choosing a reading.

Corroborated by the tree's own convention, which is uniform:

```
$ grep -rn -- '- Check: `app/check_' openspec/specs/ | wc -l
       6
$ grep -rnoE -- '- Check: `check_[a-z_]+\.(py|sh)`' openspec/ | wc -l
       0
```

Six existing gate-script citations, every one `app/`-prefixed; zero bare
basenames anywhere.

**What breaks.** On the path-less reading, `check-spec-checks-resolve` exits 1.
It is the **sixth** prerequisite of `app/Makefile:344`'s linear `test:` list,
ahead of all twelve binaries, so `make test` aborts before any binary runs — and
6.6's own Check requires `python3 app/check_spec_checks_resolve.py app` to exit
`0` immediately after the rewrite. **Blocks execution of 6.6 and 6.7, and blocks
delivery:** `spec_files()` walks `openspec/specs`, and 6.7 runs no gate after
`openspec archive`, so a red line reaches `origin` undetected.

**Remedy (condition).** 6.6 must prescribe one form, spelled out literally, that
the resolver accepts in the tree state 6.6 runs in, and must state the actual
acceptance rule (an indexed repo-relative path whose target is a `test:`
prerequisite) rather than "a bare script name". The word "bare" must not appear
in two senses in the same paragraph.

## BLK-3 — Task 6.6's script-line test never reads the line it checks, and four device scenarios share one grep

**Sources:** E-ADV3, F-ADV1 (second half). **CONFIRMED.** **Class:** a check
whose result is independent of what it is supposed to be checking.

Three legs, each checked.

*(a) `gate_scripts()` never opens the script.* Its body (`:267-293`) reads
`app/Makefile` only and scans each `test:`-prerequisite target's recipe for a
literal `$(APP_DIR)/<name>`. In my probe tree I truncated the script to zero
bytes and re-ran:

```
=== ZERO-BYTE script, wired into test: ===
check-spec-checks-resolve: OK - 1 Check reference(s) resolved, 0 declared as having no automated check
EXIT=0
```

An empty file resolves identically to a working one. Task 4.8's drift half — the
only mechanism behind the ADDED requirement's "a check SHALL fail when the
generated text and the declarations disagree" — can be a `pass`.

*(b) The loop's script-line test is a function of the file, not of the Check
line.* Both script scenarios run one byte-identical command against
`app/check_docs_match_device_preconditions.py`; the spec line's text is never
read. Whatever the line says, the count is the same. And `Check: none.` passes
the resolver outright (run above, EXIT=0), via
`NO_CHECK = re.compile(r"^\s*(none|operator step|not yet delivered)", re.I)` and
`if declared and not tokens: declared_manual += 1; continue` (`:96`, `:383-385`).
Every deferred line in the delta today carries zero backticked tokens, so the
short-circuit is reachable for both:

```
$ grep -c '^- Check: not yet delivered' …/specs/froggers-midi-controller-mappings/spec.md
10
$ grep '^- Check: not yet delivered' … | grep -o '`[^`]*`' | sort -u | wc -l
       0
```

6.6's own positive control cannot catch this. The control is "leave exactly one
line as `Check: none.` and confirm the loop returns nothing for that one
scenario". On a script line the loop greps the script file, so it returns a count
regardless — the control is structurally incapable of firing there, and an
executor will run it on a case line and report that it fired.

*(c) Four scenarios share one case name.*

```
$ grep -n 'device_defaults_declare_their_preconditions' …/specs/froggers-midi-controller-mappings/spec.md
55:- Check: not yet delivered; task 4.3 adds a device_defaults_declare_their_preconditions case …
60:- Check: not yet delivered; task 4.4 adds a device_defaults_declare_their_preconditions case …
65:- Check: not yet delivered; task 4.4 adds a device_defaults_declare_their_preconditions case …
83:- Check: not yet delivered; task 5.2 adds a device_defaults_declare_their_preconditions case …
```

One `grep -F` hit on the diff satisfies all four. 6.6 concedes this and answers
it with a prose "Verify by hand"; nothing mechanical separates them.

**What breaks.** Two of the ten scenarios (the delta's lines 45 and 70) and three
of the four device scenarios can be undelivered with every gate green, and the
promoted spec then ships ADDED requirements whose Check lines assert coverage
that does not exist. **Blocks delivery.**

**Remedy (condition).** For each of the ten scenarios the check must read that
scenario's own rewritten line *and* an artifact only that scenario's delivering
task produces, and must be shown red when that one scenario's delivery is
removed. Under the lead's standing direction, a scenario for which no such
demonstration can be constructed has its check removed rather than guarded.

## BLK-4 — Task 6.6 attributes the new target's wiring to task 1.6, which cannot have done it

**Sources:** none — reached only in this ruling. **CONFIRMED.** **Class:** an
internal contradiction between two tasks of the same change.

6.6 justifies the script form with "(task 1.6 records it as
`check-docs-match-device-preconditions` in `app/Makefile`'s `test:` prerequisite
list)". Task 1.6 is a **baseline of the pre-existing twelve**, run after task 1.2
confirms the branch is at `main`'s tip:

> Record, with its exit code, all twelve `check-*` prerequisites (`check-no-juce`,
> … `check-delay-capacity-break-proofs` — read from `app/Makefile`'s `test:` line
> itself…) *(tasks.md, task 1.6)*

`check-docs-match-device-preconditions` is not among them, and is not in the tree:

```
$ grep -n '^test:' app/Makefile
344:test: check-no-juce check-no-firmware-includes check-microphone-usage check-catalog-covers-screen-actions check-docs-match-parameter-table check-spec-checks-resolve check-citations-resolve check-modified-requirements-restate-promoted check-no-planning-history check-artifact-symbols-resolve check-delay-capacity-parameters-are-swept check-delay-capacity-break-proofs $(TEST_BIN) …
```

Twelve `check-*` targets, none of them the new one. The change's own task 6.3
contradicts 6.6 directly:

> This change adds one target (`check-docs-match-device-preconditions`, **first
> wired by task 1.8** with only the recovery half, extended by task 4.8) to the
> twelve. *(tasks.md:908-910)*

**What breaks.** 6.6's justification points the executor at a task that neither
creates nor wires the target, in the same clause where it asserts a resolution
rule the resolver does not have (BLK-2). An executor who checks the claim finds
1.6's recorded list does not contain the name and cannot tell whether 6.6 or 6.3
is right. **Blocks execution of 6.6.**

**Remedy (condition).** 6.6 must name the task that actually wires the target
(1.8, extended by 4.8), consistently with 6.3.

## BLK-5 — The emitter's output format and the generated heading are blanks, and task 4.8 is sent to a section that does not contain them

**Sources:** B3. **CONFIRMED.** **Class:** the operator's own corollary — a blank
in a brief, filled toward green.

Both literals are named "fixed" and neither is given:

```
$ sed -n '351p;384,385p' …/design.md
`declaredPreconditions` list, one line of a fixed, parseable format per
device directly from the emitter's own output (one declared string per line,
in list order, under a fixed heading) and compares it, byte for byte, against
$ grep -n '^```' …/design.md
222 242 646 658 679 727 802 810
$ grep -c 'declaredPreconditions' /Users/diegoaguilar-canabal/Desktop/frogg3rs/MANUAL.md
0
```

I accounted for every fence rather than sampling: 222/242 is the seventh-default
build run, 646/658 and 679/727 the two disassemblies, 802/810 the `device.id`
grep. None is an emitter-output sample, and `MANUAL.md` carries no convention to
read a format off.

Task 4.8 then forecloses inventing one *in the task* while pointing at the
section that lacks it:

> implementing exactly the rule, floor and positive controls design.md's "The
> drift check has two independent halves" section states — **read that section
> before writing this script; nothing here restates it.** […] prints each entry
> […] **in the fixed, parseable format design.md's "the alternative this task
> adopts" section describes** — this is the drift half's only source for both the
> expected values and the floor count. *(tasks.md, task 4.8)*

I record the counter-evidence, because it bounds the finding: design.md *does*
specify the marker pair (`<!-- declaredPreconditions:froggers.twister -->` …
`<!-- /declaredPreconditions:froggers.twister -->`), the floor, the five-word
marker-scan vocabulary, and both positive controls. The gap is specific to the
per-device line format and the heading — the two literals the byte-for-byte
comparison is taken over.

**What breaks.** Task 4.7 writes the `MANUAL.md` side and task 4.8 writes the
renderer; both are group 4, one executor. Whatever format it invents agrees with
itself on arrival, so 4.8's "Run and record, now, against the tree as it stands
after task 4.7 […] the drift half must pass" is satisfied by construction. More
simply: 4.8 sends its executor to a named section for a literal that section does
not contain, so the instruction cannot be discharged as written. **Blocks
execution of 4.7 and 4.8.**

*(Not a reason to remove the check under the lead's direction: 4.8 carries two
stated positive controls, (a) and (b), and I confirmed both are real and
directional in design.md:414-423. The mechanism is sound as a regression
tripwire once a format exists; the defect is at authoring time.)*

**Remedy (condition).** design.md must print the emitter's per-device line and
one complete marker region verbatim, including the heading and the empty-list
case for the four devices whose `declaredPreconditions` are empty, so that 4.7's
text and 4.8's renderer are each written against a literal neither executor chose.

## BLK-6 — Task 4.2's stated citation backstop reads neither line numbers nor the files carrying the citations

**Sources:** E-ADV1, F-ADV2 (reached independently of each other). **CONFIRMED.**
**Class:** a check described as covering what it does not read.

Task 4.2 (tasks.md:533-537):

> **Re-run `python3 app/check_citations_resolve.py app` immediately after this
> pin advance and before continuing.** That script verifies a cited line number
> falls within the pinned tree's current file length; it cannot tell a citation
> that still happens to be in range from one that now points at different code…

Both halves are false. My probe, against a copy of the real script:

```
=== A: a citation to line 999999 of a 3-line MidiController.cpp, from app/probe.hpp
check-citations-resolve: OK - 0 commit-pinned, 1 into pinned or frozen trees, 0 unresolvable, 0 line-numbered into this tree, 0 split across two lines
EXIT=0
=== B: plus a dangling External/Sheaf/.../NoSuchFile.cpp:12 citation in openspec/changes/x/design.md
check-citations-resolve: OK - 0 commit-pinned, 1 into pinned or frozen trees, 0 unresolvable, …
EXIT=0
=== C: NEGATIVE CONTROL — the same dangling citation inside app/probe2.hpp
check-citations-resolve: FAIL - app/probe2.hpp:1 cites `External/Sheaf/projects/synth/src/NoSuchFile.cpp`, which is not a file under app/, External/Sheaf/, src/ and carries no commit pin; give the path from the repository root
EXIT=1
```

Run C fires, so the probe discriminates. Run A shows no length is ever compared;
run B shows the counts are unchanged when an `.md` under `openspec/` is added,
i.e. it is never opened. The source agrees: `SCAN_EXT = (".cpp", ".hpp")` (`:65`)
and both walks are `walk_sources(app_dir, …)` (`:124`, `:146`); resolution is
`if path not in paths` against `path_index(repo, SEARCH_ROOTS)`, and
`m.group("line")` is consumed only inside the error message at `:141`.

**What breaks.** Every citation task 4.2 names as most at risk —
`MidiController.cpp:932-976`, `:964-971`, `:969`, `:975` and task 5.4's `:847` —
lives in `openspec/` artifacts, outside the scan entirely; and Sheaf's task 3.1
moves those very lines. The re-run is green before and after the pin whatever
happens, so the executor's sense of what the by-hand pass must catch is set by a
backstop that does not exist. **Blocks execution of 4.2.**

*(Dissent recorded: C graded this SHOULD-FIX on the ground that 4.2 already
demands a by-hand re-check. I rule blocking because the task does not merely
under-describe the script — it describes a capability the script does not have,
and that description is what sizes the by-hand pass.)*

**Remedy (condition).** Task 4.2 must state what the script actually checks (path
existence under `app/`, `External/Sheaf/`, `src/`, over `app/**/*.{cpp,hpp}`
only), state that no citation in this change's artifacts is in its scope, and
enumerate by path and line the citations the by-hand re-read must cover.

## BLK-7 — Task 4.1(b) prints its pass value for a path that does not exist

**Sources:** E-ADV8. **CONFIRMED.** **Class:** a gate that passes most
comfortably when the thing it measures is dead.

Task 4.1(b) (tasks.md:431-447): "`git -C External/Sheaf show
HEAD:openspec/changes/midi-controller-resilience/tasks.md | grep -c '^- \[ \]'`
reports `0`", and the task calls (a) and (b) together "what distinguish the
completion commit from an intermediate one".

```
$ cd External/Sheaf
$ git show HEAD:openspec/changes/midi-controller-resilience/tasks.md | grep -c '^- \[ \]'
46
$ git show HEAD:openspec/changes/midi-controller-resilence/tasks.md 2>/dev/null | grep -c '^- \[ \]'   # one letter dropped
0
pipeline_exit=1
$ git show HEAD:openspec/changes/no-such-change/tasks.md 2>/dev/null | grep -c '^- \[ \]'
0
$ git ls-files openspec/changes/archive | wc -l
     700
```

A single dropped letter produces the gate's exact pass value. The task's
condition is "reports `0`" and never reads the pipeline's exit status. The
archive route is real too — `openspec/changes/archive` is tracked with 700 files,
so `openspec archive` genuinely removes the path — though I record C's and D's
limit: Sheaf's own 7.7 archive is blocked this cycle and the operator has
deferred it, so the typo/rename route is the one live today. That does not narrow
the defect; it changes only which producer of the dead path fires.

**What breaks.** 4.1 is the sequencing gate for frogg3rs groups 3, 4 and 5. Its
sibling conditions do not catch this: (a) compares a SHA, (c) builds Sheaf
binaries that build and pass with the change directory gone, (d) is a clean-tree
check. **Blocks execution of groups 3, 4 and 5.**

**Remedy (condition).** (b) must first assert the path exists at that commit
(`git cat-file -e HEAD:<path>`), and must assert a nonzero *total* box count, so
that 0-of-0 is a STOP rather than a pass.

## BLK-8 — `design.md` says a moved address still dispatches; the code it cites drops it

**Sources:** A1. **CONFIRMED.** **Class:** a traced claim its own cited code
contradicts, about the change's motivating incident.

```
$ sed -n '151,153p' openspec/changes/frogg3rs-midi-preset-preconditions/design.md
own CC Hold is unmet on the device (so no matching release ever arrives, or
the addresses move) still fires a job on every press; nothing about dispatch
depends on that button's own release ever arriving.

$ sed -n '938,943p' External/Sheaf/projects/synth/src/MidiController.cpp
    const SystemButtonMidiAssociation* association = FindAssociation(midi);
    if (association == nullptr) {
        PassToThru(midi);
        return;
    }

$ sed -n '985,1001p' External/Sheaf/projects/synth/src/MidiController.cpp
const SystemButtonMidiAssociation* SystemButtonMidiInProcessor::FindAssociation(const BasicMidi& midi) const {
    …
        if (address.has_value() && association.control.has_value() && *association.control == *address) { return &association; }
        if (address.has_value() && association.launchpadPosition.has_value()) { … LaunchpadPositionToNote(…) == address->cc … }
    …
    return nullptr;
}
```

`FindAssociation` matches on exact address equality or a derived Launchpad note.
A moved CC address matches neither arm, `Process` returns at `:939-942` through
`PassToThru`, and nothing is dispatched. The falsifying lines sit inside the
range the same paragraph cites (`MidiController.cpp:932-976`). And the claim is
false about the incident the proposal opens with:

```
$ sed -n '11,12p' …/proposal.md
An operator whose Twister had "Bank Side Buttons" on lost three of six side
buttons: the device moved their CC addresses, and the app reported nothing.
```

A's scoping is right and I re-checked it: the delta is bounded correctly and is
not affected — `spec.md:13` says "SHALL NOT be silently dropped **for want of a
release**", and both scenarios (`:32`, `:37`) say "no matching release message
ever arrives". Only `design.md` overreaches.

**What breaks.** The archived change records, as traced fact, the opposite of the
code about the behaviour the whole change exists to fix. **Blocks delivery.**

**Remedy (condition).** Delete "or the addresses move" from design.md:151, or
replace it with the traced behaviour (a moved address matches no association and
is passed to thru without dispatch), so the paragraph agrees with the code it
cites and with the proposal's opening.

## BLK-9 — Sheaf's Impact and §8.0 sweep both miss `projects/synth/Makefile`, and the Impact never names the gate task 1.9 creates

**Sources:** A2; D1 reached the Makefile half independently at SHOULD-FIX and
regraded to A2 in the exchange. **CONFIRMED, narrowed as E, F and D all state.**
**Class:** an artifact whose stated completeness contract it does not meet.

```
$ P=openspec/changes/midi-controller-resilience/proposal.md
check_spec_checks_resolve                0
projects/synth/Makefile                  1
instrument_tests.cpp                     0
engine_tests.cpp                         0
controller_wizard_tests.cpp              0
midi-timing.test.mjs                     0
MiniAppCore.hpp                          0
$ grep -n "projects/synth/Makefile" $P
221:  isolated from the top-level one: `projects/synth/Makefile`'s `test` recipe
```

The single hit is prose about the miniapp target's isolation, not a definition
site. The tasks edit that file repeatedly:

```
$ grep -n "projects/synth/Makefile" openspec/changes/midi-controller-resilience/tasks.md
… 427:      **Wiring.** Add it to `projects/synth/Makefile`'s `test` target as a
… 838:          `projects/synth/Makefile:226-227`); add it to
… 1011, 1057 …
```

And the sweep is directory-scoped, with `projects/synth/Makefile` above every
listed directory:

```
$ sed -n '252,256p' $P
§8.0's sweep therefore covers `projects/synth/src`, `projects/synth/include/synth`,
`projects/synth/include/synth/browser`, `projects/synth/runtime`, `projects/synth/tests`,
`projects/synth/browser`, `projects/synth/apps`, `projects/synth/juce`,
`projects/synth/docs`, `projects/synth/scripts`, and `openspec/` — every directory
this change's tasks read or edit, read from the tasks rather than assumed narrower.
```

That closing clause is the sentence that fails: the tasks edit
`projects/synth/Makefile`, which no listed directory contains.

The second half is the new gate script. Task 1.9 creates
`projects/synth/scripts/check_spec_checks_resolve.py`; the Impact's bullet for
that directory reads:

```
$ sed -n '245,247p' $P
- `projects/synth/scripts/` — holds `check_ui_boundary.sh`,
  `check_ui_boundary_empty_discovery.sh` and `check_app_bundle_plist.sh`,
  three gates `projects/synth test` baselines in task 1.5.
$ ls projects/synth/scripts/
check_app_bundle_plist.sh  check_ui_boundary.sh  check_ui_boundary_empty_discovery.sh
```

Exactly three, with no mention that this change adds a fourth.

**Narrowing (E, F, D; I reproduce it).** Of A's eight sites, only
`projects/synth/Makefile` is outside *both* the Impact and the sweep. The new
gate script and the six test/spec files sit under swept directories, so §8.0
reaches them; what fails for those is the Impact's enumeration, not the sweep.

**What breaks.** §8.0's hygiene sweep never looks at the file where a new gate is
wired into a linear `test:` list, so anything this change leaves there ships
unswept; and the `scripts/` bullet's "three gates" is stale in the same sentence
that should have named the fourth. **Blocks delivery.**

**Remedy (condition).** The Impact must name `projects/synth/Makefile` and
`projects/synth/scripts/check_spec_checks_resolve.py` as sites this change edits
and creates, the `scripts/` bullet's count must match what the change leaves, and
§8.0's sweep enumeration must cover every path the tasks edit — including files
that are not inside any listed directory.

## BLK-10 — Sheaf task 1.3 asserts a site count no written predicate produces

**Sources:** B2. **CONFIRMED on fact; I rule BLOCKING at execution.**
**Class:** a check the executor cannot discharge — a figure that will be ticked
rather than verified.

*This is one of the disputes I was asked to settle: A refuted it to NOTE; C and E
confirmed the fact at SHOULD-FIX; D and F confirmed at BLOCKING.*

Task 1.3: "Enumerate every `MidiEndpointOps` construction/binding site and
**confirm the count is seven, not five**". The mechanical enumeration:

```
$ cd External/Sheaf/projects/synth
$ grep -rn "MidiEndpointOps [a-z]" src include runtime tests browser apps juce | wc -l
      19
```

Nineteen lines, of which two are comments (`runtime/MidiConnectionManager.hpp:435`,
`tests/reconcile_executor_tests.cpp:89`) — 17 declaration sites. Ten of the
seventeen are `MidiEndpointOps ops = MakeOps(log…);` copy-initialisations in
`reconcile_executor_tests.cpp`. `17 − 10 = 7`, so the figure is right *under a
predicate that excludes copy-initialisation from a factory*, and that predicate
is written as a rule nowhere:

```
$ grep -rn "copy-initialis\|copy-initializ\|is a use, not\|binding site" openspec/changes/midi-controller-resilience/ | grep -v preflight
tasks.md:60:- [ ] 1.3 Enumerate every `MidiEndpointOps` construction/binding site and
design.md:57:`MidiEndpointOps` has seven construction/binding sites in total, not two: the
design.md:435:binding sites total (see Context): two production, and five in tests, of
proposal.md:148:  production bindings. `MidiEndpointOps` has seven construction/binding sites
```

**On A's refutation.** A offered three legs; I check each.
1. *"B's own command does not produce B's own number."* This is not a refutation —
   B's exchange states 19 lines of which 2 are comments, leaving 17, and A's own
   quotation of the two comment lines confirms B's arithmetic.
2. *"The rule is written in design.md at the exact citation the task uses"* —
   design.md:59's parenthetical "(`MakeOps`, **shared by most cases in that
   file**)". I read it: it describes `MakeOps` as shared. It does not state that a
   sharer is not itself a site. It is an implication an executor may draw, not a
   predicate the executor can apply. This is the strongest of A's three legs and
   it mitigates, it does not refute.
3. *"Task 1.3 does not ask anyone to derive a number."* The task's verb is
   "**Enumerate** every … site **and confirm** the count is seven". An executor
   who enumerates gets 17.
So A's refutation does not stand. C's and E's SHOULD-FIX rests on the same
mitigation as A's leg 2.

**What breaks, and why I rule blocking rather than SHOULD-FIX.** Nothing wrong
propagates: the classification downstream consumes is correct, and I verified all
five test sites by reading them —
`reconcile_tests.cpp:790` (`synth::MidiEndpointOps ops;`, unbound),
`reconcile_executor_tests.cpp:353` (`MidiEndpointOps ops; // every std::function
member left null`), `controllers_page_ui_tests.cpp:1462` (binds `closeInput` /
`closeOutput` only), `reconcile_tests.cpp:657` (binds `openInput`),
`reconcile_executor_tests.cpp:93` (`MakeOps`, binds `openInput`/`openOutput`) —
and task 3.6 consumes those two by name, never the count. What breaks is the
check itself: the only way to discharge "confirm the count is seven" is to read
the task's own list back to itself, or to tick it unverified. That is the class
this project names as a green test that cannot fail, and the operator's own
corollary applies — an assertion handed to an executor that is not derivable from
anything written will be resolved toward green. **Blocks execution of task 1.3;
blocks no delivery and propagates no wrong classification.**

**Remedy (condition).** Task 1.3 (and `design.md:57`, and `proposal.md:148`,
which repeat the same figure) must state the predicate that makes the count seven
— that `MidiEndpointOps ops = MakeOps(…)` is a use of `MakeOps`, whose own site
is `reconcile_executor_tests.cpp:93-111` — or drop the count assertion and keep
only the enumeration by path with its classification.

## BLK-11 — Sheaf task 3.7's two clear-asserting cases pass when the modifier was never held

**Sources:** F-ADV3. **CONFIRMED; I rule BLOCKING.** **Class:** a positive
control that can be vacuous.

*This is the third dispute I was asked to settle: D refuted it to SHOULD-FIX on
the ground that 3.7 does explicitly order the hold.*

The accepting path is concrete. The only writers of `held` are inside
`SystemButtonMidiInProcessor::Process`:

```
$ grep -n "held = " External/Sheaf/projects/synth/src/MidiController.cpp
956:            holdDrill_->held = true;
959:            holdDrill_->held = false;
969:            shift_->held = isPress;
```

(`SystemButtonMidiInProcessor::Process` begins at `:932`; there is no other
function between `:932` and `:980`.) That processor is appended only under a
guard:

```
$ sed -n '3055p' …/src/MidiController.cpp
    if (!config.systemMessages.empty()) {
```

And the binary task 3.7 reuses configures none:

```
$ grep -c systemMessages External/Sheaf/projects/synth/tests/browser_midi_bridge_tests.cpp
0
```

So a slot configured without `systemMessages` yields accessors that are non-null
and permanently `held == false`, and a case asserting `held == false` after the
endpoint opens is green whether or not any clear exists.

**On D's refutation.** D is right on the fact: 3.7 does say "Both cases hold BOTH
Shift and Hold Drill before the endpoint opens and assert both clear", with its
reason. I read the whole of 3.7 and confirm the instruction is there. But the
refutation does not reach the finding, for a reason the change itself supplies.
Task 3.10 spends a paragraph proving that this exact instruction silently no-ops
without three further steps, and mandates all three — install a slot whose
`config.systemMessages` binds the trigger via `EditInstrument`; assert the
accessor `!= nullptr` "as a hard precondition, so an out-of-range or
wrongly-shaped slot fails loudly instead of leaving every later assertion
vacuous"; and "presses it for real … and asserts `->held` is now `true` before
touching the clock at all." 3.7 carries none of the three mechanisms:

```
$ sed -n '994,1078p' …/tasks.md | grep -cE "systemMessages|->held|non-null|EditInstrument|MessageIn::Shift"
0
```

And 3.10 designates 3.7's cases as the control for the requirement:

> (3.6/EndpointOpen's positive control is task 3.7's two binding-specific cases,
> **which already hold both modifiers before the endpoint opens**)

So the change asserts of 3.7's cases the property that 3.10's own paragraph proves
an executor cannot obtain from an instruction alone. An instruction to "hold it",
with no mechanism and no precondition assertion, beside a written proof inside
the same change that exactly that shape is vacuous-capable, is not a wording gap.

**Narrowing (E, B, D; I reproduce it).** One of the three cases is
self-guarding: `ReconcileAloneDoesNotClearAHeldModifier` asserts *still held* and
goes red under the never-held shape. But 3.7 puts it on "a standalone `RealBridge`
over `RealEngine` (the pair described above, built for this case alone)", while
the ABI case observes "on the same adapter's `Engine()`, never on a separately
constructed `RealEngine`" — two engines, so its implicit precondition does not
reach the ABI case. The new runtime binary
(`juce/MidiConnectionManagerReconcileTests.cpp`) has no companion at all, and
3.7 says so ("no 'reconcile alone does not clear' companion applies"). Two of
three cases are unguarded.

**What breaks.** smi-16's endpoint-open clear ships with a control that passes
whether or not the clear exists, in the two cases that assert it. **Blocks
delivery.**

**Remedy (condition).** Task 3.7 must carry, for each of its two clear-asserting
cases, the three steps task 3.10 states — the `systemMessages`-bearing slot, the
`!= nullptr` hard precondition, and a real press asserting `->held == true`
before the endpoint opens — and each case's break must be ordered as a run, not
stated as a property. If 3.10 is to keep designating 3.7's cases as EndpointOpen's
positive control, that designation must name the precondition rather than assume
it.

---

# 3. SHOULD-FIX and NOTE

Consolidated, with source ids. "Verified" means I ran or read it myself in this
pass; "not adjudicated" means it is inside my scope to list and outside what a
rejection needs settled — the repair pass verifies it at the point of repair.

## Verified in this pass

| Id | Finding | Sources | Ruling |
|---|---|---|---|
| SF-1 | Sheaf `design.md:810-812` states a wider scope for task 1.9's gate than `design.md:853-857` and `tasks.md:169-172` allow | A5 | **Verified, SHOULD-FIX.** Reproduced: `design.md:810` says "every `Check:` line under `openspec/specs/**/spec.md` and `openspec/changes/<name>/specs/**/spec.md` (skipping `archive`)"; `:853-857` says "the script reads a delta file only when it carries the marker `<!-- check-lines-resolve -->` … every line in `openspec/specs/` after promotion [is] not checked"; `tasks.md:169-172` says "**Scope: marked delta files only** … and nothing else." `tasks.md` governs execution and is unambiguous, so the executor builds the right gate; the design paragraph is wrong. Fix `design.md:810-812`. |
| SF-2 | frogg3rs task 1.1's submodule subject search returns nothing where the task points the executor, triggering its own STOP | C3 | **Verified, SHOULD-FIX.** Reproduced: `git -C /Users/…/frogg3rs/External/Sheaf log --all --oneline \| grep -i "Carry the MIDI controller resilience"` → no output; the same command in this worktree's submodule store → `caae5c2e Carry the MIDI controller resilience change into the submodule`. The main checkout's `.git/modules/External/Sheaf` is a different object store. Reading 1.1, "from the main checkout" grammatically introduces only the `ls` and `git branch --list` commands; the two subject searches are in a later sentence with no stated cwd. So the text supports both readings and one of them loses the run on the first task, under an unconditional STOP. **Ask-when-confused shape: scope the sentence explicitly.** |

## Not adjudicated — verify at the point of repair

Listed so nothing is lost. Several are entangled with confirmed blockers and are
noted as such.

**frogg3rs artifacts.** A3 (the module-level disassembly is presented as literal
output but is truncated unmarked — see §4, OPEN); A4 (`design.md:752-754` calls
the `SliderElement` call positional against its own recorded `KW_NAMES` evidence);
A6 (Impact names the emitter only as prose, never the Makefile work it needs);
A7 (six citations sit one or two lines wide of the construct they name); A8
(the shipped Launch Control XL values rest on one unread premise — recorded, not
raised; the operator has ruled the preset ships).

**frogg3rs task mechanics.** B4 (task 4.1(c)'s cross-repository gate omits
Sheaf's browser/node half); B5 (task 1.6 leaves the twelve binaries' baseline
undated while 6.3 asks which "moved"); B6 (task 3.2's coordination check does not
cover `README.md`, which task 5.6 edits); B7 (task 5.5 names one of two stale
counts inside the same header comment); F-ADV11 (same target as B6).

**Check-line and gate behaviour.** C2 (`declaredPreconditions` cannot distinguish
"declares none" from "was never asked", and two scenarios assert that it does);
C4 (task 4.6's positive control edits the submodule working tree and is exempted
from the gate protecting it); C5, C6 (NOTE: four delivered cases cited by no
`Check:` line; the rewrite obligation binds eleven tasks and is restated at two);
E-ADV4 (6.6's positive control is silent by construction — **subsumed by BLK-3**);
E-ADV16 and F-ADV12 (the `none`/`operator step` short-circuit is guarded in Sheaf
and unguarded in frogg3rs — **the mechanism is confirmed inside BLK-3**);
E-ADV18, F-ADV9 (NOTE).

**Checks that are green with the mechanism dead.** E-ADV5 (the recovery half's
absence rule rejects one spelling of the sentence it exists to reject); E-ADV7
(4.1(b) *is* a tick — **superseded by BLK-7**); E-ADV9 (the LCXL requirement
ships with no mechanism that can reject a wrong value); E-ADV10 (the drift half's
floor counts marker pairs, so an all-empty catalogue passes — **independent of
BLK-5 and survives its repair**); E-ADV11 (frogg3rs' positive controls omit the
same-second-mtime guard Sheaf carries); E-ADV13 and F-ADV4 (the rate-limit
assertion is one-sided; a limiter 20× tighter than configured is green — reached
independently by two); E-ADV14 (the hold ceiling's units are asserted by nothing
in either repository); E-ADV15 (four of sru-63's nine checks are absence
assertions green with the mechanism dead, one with no control); E-ADV2, E-ADV6,
E-ADV12, E-ADV17 (NOTE); F-ADV5 (4.1's conditions a–d — **partly superseded by
BLK-7**); F-ADV6 (task 1.8's recovery rules 1 and 2); F-ADV7 (the marker regions
and floor — complementary to BLK-5, not displaced by it); F-ADV8, F-ADV13 (NOTE);
F-ADV10 (task 4.6's positive control (2)); F-ADV14 (NOTE, the delivery half of
F-ADV1 — **subsumed by BLK-2**).

**Hygiene and Impact completeness.** D2 (dangling planning-doc citation under
`projects/synth/runtime/` that task 1.8 misses); D3 (a third stale doc under
`projects/synth/docs/`, describing a mechanism that no longer ships); D4 (a
dangling reference to the removed pre-split change survives in `app/`, and task
1.7 asserts in advance that none does); D6 (19 tracked preflight audit files ship
in both repositories, cited by no artifact); D7 (a fourth behavioural test drives
the whole catalogue; task 5.5 names three and calls the set complete); D8 (the
frogg3rs Impact states an edit to `FroggersControllersPageTests.cpp` that no task
makes — the frogg3rs instance of BLK-9's class); D9 (the main-checkout
coordination command is path-scoped to four paths and omits `openspec/`); D5,
D10, D11 (NOTE). **D1 is withdrawn by its author into BLK-9.**

---

# 4. OPEN items

| Item | What settles it | Operator needed? |
|---|---|---|
| **A3** — whether `design.md:638-658`'s module-level disassembly is truncated. A's case is CPython 3.11 instruction-size arithmetic against the offsets the record itself prints (first printed line at offset 222, implying ~111 filtered module-level instructions before it), not a re-read. | Re-run the recorded command against `LaunchControlXL.pyc` and paste the full output, or mark the elision the way the next block marks its `RESUME`. | **Yes.** The file is under `/Applications`, which my brief bars and which barred A. Either the operator runs it, or the lead marks the gap as elided — which costs nothing and closes the item without the run. |
| **A4** — follows from the same block; the `KW_NAMES` evidence is *in* the recorded disassembly, so the sentence can be corrected without re-running anything. | Rewrite `design.md:752-754` as `SliderElement(MIDI_CC_TYPE, LIVE_CHANNEL, identifier, name=name)` and scope the premise to the first three positional parameters. | No. |
| **BLK-11's runtime half** — that the two clear-asserting cases would actually compile and pass under the never-held shape is confirmed structurally (no writer of `held` is reachable in that binary) and not observed. | Building and running the case. Not required for the ruling: no writer of `held` exists in that binary's reachable code, which is a stronger statement than one observed run. | No. |
| **SF-2's intended reading** of task 1.1's "from the main checkout". | The lead scopes the sentence. Two plausible readings that change execution — settle it in the artifact rather than at run time. | No. |

Nothing else is open. The disputes I was directed to rule — C1, B2, F-ADV3 — are
all closed above by command, not left open.

---

# 5. Task 6.6 must be rewritten whole

**Confirmed.** Task 6.6 carries **four** independent defects, not three:

- **BLK-1** — its loop reads a commit range the work is not in at 6.6's placement;
- **BLK-2** — its prescribed line form is ambiguous, one reading turns a `test:`
  prerequisite red, and its stated resolution rule is false under either reading;
- **BLK-3** — its two script-line tests are functions of a file, not of the line
  they check, so `Check: none.` passes; and four device scenarios share one grep;
- **BLK-4** — it attributes the new target's wiring to task 1.6, which its own
  task 6.3 contradicts.

They are coupled, which is why patching clause by clause fails. Fixing BLK-1
alone converts a visible STOP on all ten scenarios into a silent pass on at least
five (BLK-3). Fixing BLK-2 alone leaves the loop silent. Fixing both leaves the
drift half absent and everything green. And 6.6's own stated positive control
cannot fire on the two lines that need it most, so the task cannot detect any of
this about itself.

**The single condition its rewrite must satisfy:**

> For each of the ten scenarios, 6.6's check must be shown to go red when that one
> scenario's own delivery is removed — ten demonstrations, one per scenario, each
> run and recorded, not one demonstration generalised over the set.

That condition alone forces every repair. A check run over an empty diff range
never changes colour, so BLK-1 fails it. A check that cannot be run because the
gate it names exits 1 cannot be demonstrated, so BLK-2 fails it. A test that
greps the script file rather than the Check line does not change when the drift
half is removed, and a grep for a case name shared by four devices does not change
when three of the four devices' assertions are removed, so BLK-3 fails it. And
the demonstration cannot be performed at all without knowing which task wires the
target, so BLK-4 surfaces on the first attempt.

Per the lead's standing direction, any of the ten scenarios for which such a
demonstration cannot be constructed has its check **removed, not guarded** — the
scenario then says plainly that it ships unchecked and names why, which is what
task 6.4 already requires of every scenario in the delta.

---

# 6. Coverage

**Reached independently by more than one report.**

- **BLK-2** — C1 and F-ADV1, on the same mechanism, from different starting
  points (C from the gate's own convention, F from a resolver probe).
- **BLK-6** — E-ADV1 and F-ADV2, independently, with different demonstrations
  (E a sandbox probe, F a source read of `SCAN_EXT`/`SEARCH_ROOTS`).
- **BLK-3** — E-ADV3 in full; F-ADV1's second half reached the `Check: none.`
  escape by a separate route. Partial independent convergence.
- **BLK-9** — A2 in full; D1 reached the `projects/synth/Makefile` half
  independently and graded it lower, then withdrew into A2.

**Reached by exactly one report.**

- **BLK-1** (B alone) — and it is the finding with the widest blast radius, since
  it silences all ten scenarios.
- **BLK-5** (B alone). F-ADV7 and E-ADV10 are adjacent on the same mechanism but
  attack its run-time behaviour, not the authoring-time blank.
- **BLK-7** (E alone). F-ADV5 attacked the same condition on a different leg and
  missed this one.
- **BLK-8** (A alone).
- **BLK-10** (B alone) — and it was the most disputed, refuted by one auditor and
  downgraded by two.
- **BLK-11** (F alone) — F's own exchange notes no peer report contains "never
  held", `EndpointOpenClearsHeldModifierAtRuntimeBinding` or
  `ReconcileAloneDoes`; I re-ran that check and confirm it. Single-reach, and
  disputed by a second auditor on severity.

**Reached by no report.**

- **BLK-4** — 6.6's misattribution of the wiring to task 1.6. One report quotes
  the parenthetical verbatim without checking it; none checked it against task
  1.6's own list or against task 6.3's contrary statement.

**Axes that found nothing blocking.**

- The hygiene / directory-sweep / stale-document axis (D's). Eleven findings, all
  SHOULD-FIX or NOTE, and an ACCEPT verdict revised to REJECT in the exchange on
  peers' findings. Its one candidate (D1) was subsumed. That axis is not
  worthless — it produced BLK-9's Makefile half independently — but it produced no
  blocker of its own this round.
- The forward-enumeration axis (C's §1: created and renamed concepts, MODIFIED
  requirements against the text they modify) produced no surviving blocker; C's
  single blocking finding was withdrawn by its author, and I have reinstated it on
  different grounds (BLK-2), which is not the same as the axis having found it.

**Concentration.** Six of the eleven blockers (BLK-1 through BLK-5, plus BLK-6)
land on the frogg3rs delivery path — tasks 4.1, 4.2, 4.7, 4.8, 6.6, 6.7 — and
four of those on task 6.6 alone. The Sheaf change carries three (BLK-9, BLK-10,
BLK-11). The two changes fail for different reasons and neither failure is
contained by the other.

---

# 7. Method notes

- Every ruling above was produced by my own command or my own read; no reporter's
  output was carried forward as evidence. Where my run matched a reporter's, I say
  so; where I could not reproduce a claim, I say that instead.
- Every probe carries at least one negative control, so a green result is not
  mistaken for a discriminating one. BLK-2's probe has two (wired-but-absent,
  present-but-unwired); BLK-6's has one (a dangling citation inside `app/`, which
  fires).
- Nothing was edited, built, installed, committed, stashed, or pushed. No file
  under `/Applications` was opened. No `preflight*.md` inside either change
  directory was read. Other worktrees and the main checkout were touched only by
  read-only `git log`.
- The scratch tree under `scratchpad/adjprobe/` (copies of three check scripts
  plus synthetic Makefiles and spec files, no repository content) has been
  deleted.
