# Preflight 9 — adjudication

Adjudicator did not write the change, the briefs, or any report. Every verdict
below is my own command against the trees, run in this session. Convergence
between reporters is recorded in the coverage note at the end and is not
evidence for any verdict.

Trees at adjudication:

```
$ git worktree list
/Users/diegoaguilar-canabal/Desktop/frogg3rs                                    188b109 [main]
/Users/diegoaguilar-canabal/Desktop/frogg3rs/.claude/worktrees/midi-resilience  0acc093 [worktree-midi-resilience]
$ git status --short ; git -C External/Sheaf status --short
            (both empty)
$ openspec validate frogg3rs-midi-preset-preconditions --strict
Change 'frogg3rs-midi-preset-preconditions' is valid          exit=0
$ openspec validate midi-controller-resilience --strict        # in External/Sheaf
Change 'midi-controller-resilience' is valid                   exit=0
```

---

# VERDICT: REJECT

Eight blocking findings confirmed, one blocking severity refuted, none left
open. The engineering under the artifacts held up everywhere I traced it; what
fails is a set of artifact statements — four of them assertions that a command
now contradicts, four of them gates whose assertion cannot fail. Every one of
the eight is a paragraph-sized repair. None requires redesigning either change.

Counts among BLOCKING: **8 CONFIRMED, 1 REFUTED, 0 OPEN.**

---

# 1. Confirmed blocking findings

Work down this list. Each is stated as a condition on the artifacts.

---

## BL-1 — Task 1.1's confirmation gate asserts a git result that is false, names a commit reachable from no ref, and orders verification of an "md5 assurance" defined nowhere

**Sources:** A1, B1, D1. **CONFIRMED. Blocks execution.**
**Class:** a measurement frozen into an artifact and invalidated by a later
rebase, plus an assertion whose target is written down nowhere.

`tasks.md:21-25`, read verbatim:

> `4da0206`, not `6e77142`, is the commit this branch can actually reach —
> `git branch -a --contains 4da0206` names `worktree-midi-resilience`, while
> the same command against `6e77142` returns nothing, and the two commits'
> trees differ — so `4da0206` is the one to cite and to verify the md5
> assurance against, not `6e77142`.

My runs:

```
$ git branch -a --contains 4da0206
exit=0        (no output)
$ git for-each-ref --contains 4da0206
exit=0        (no output)
$ git for-each-ref --contains 6e77142
exit=0        (no output)
$ git branch -a --contains 7d31a7e
* worktree-midi-resilience
$ git merge-base --is-ancestor 4da0206 HEAD; echo exit=$?
exit=1
$ git rev-parse 4da0206^{tree} 6e77142^{tree} 7d31a7e^{tree}
1a0a278cac302232a6a6ca476036aefde81d4309
09757729269c00e0a28513e4e8c2ca8ae1f7db9b
0bd3a49a5fffb0de2189aa3aebc287d819e7975c
$ git diff --stat 4da0206 7d31a7e | tail -1
 51 files changed, 870 insertions(+), 10538 deletions(-)
$ git log --all --oneline | grep -i "Collect the MIDI resilience"
7d31a7e Collect the MIDI resilience work into one worktree
$ grep -in 'md5' openspec/changes/frogg3rs-midi-preset-preconditions/{tasks,design,proposal}.md
tasks.md:25:      the one to cite and to verify the md5 assurance against, not `6e77142`.
```

**What breaks.** Task 1.1 is the first task and says "do not proceed past this
task on the strength of this note; run the commands yourself." The command it
prescribes returns the artifact's own stated signature of the *rejected*
commit, so an executor applying the artifact's rule concludes the cited commit
is the one to discard. The commit designated as the md5 assurance target names
a tree differing from this branch's by 51 files. Both pre-rebase objects are
reachable from no ref and are gc-eligible, so the task can stop being runnable
without anyone touching the branch. "md5" occurs exactly once across all three
artifacts — the sentence demanding the verification. No command, no digest, no
statement of what is assured.

**Remedy (condition on the artifact).** Task 1.1 cites `7d31a7e` as the
reachable fold commit; the `4da0206`/`6e77142` discrimination sentence is
deleted, since both objects are now equally unreachable and settle nothing; and
the md5 clause either carries its command and expected digest or is removed.
Task 1.1's Sheaf-side citation needs no change — `git -C External/Sheaf branch
-a --contains caae5c2e` names `midi-resilience-merge`, and its two `ls` /
`git branch --list` commands still return what the task says.

---

## BL-2 — Task 3.2, design.md's Risks bullet and proposal.md's main-checkout row describe a tree state that has inverted: the named coordination party archived out of this worktree, the real live party is named nowhere, and no disposition is written for the result the re-run actually returns

**Sources:** B2, D2, F/ADV2 (and A3, filed SHOULD-FIX). **CONFIRMED. Blocks
execution of group 3, and of tasks 4.7 and 5.6, which also rewrite `MANUAL.md`.**
**Class:** the same frozen measurement as BL-1, plus the brief's own
blank-filled-toward-green defect — a check whose failing branch is unwritten.

`tasks.md:264-272`, verbatim:

> Coordinate every `MANUAL.md` edit with `frogg3rs-delay-capacity-and-width-finish`
> (present in this worktree's own `openspec/changes/`, since this branch is at
> `main`'s tip; … re-count with `grep -c '^- \[x\]'`/`'^- \[ \]'` on its
> `tasks.md` …). Traced directly on the main checkout, not assumed:
> `git -C <main checkout> status --short MANUAL.md QUICK_DICT.md` returns
> nothing … re-run that check rather than relying on this note.

My runs:

```
$ ls openspec/changes/
frogg3rs-midi-preset-preconditions
$ ls openspec/changes/frogg3rs-delay-capacity-and-width-finish
ls: openspec/changes/frogg3rs-delay-capacity-and-width-finish: No such file or directory
$ ls /Users/diegoaguilar-canabal/Desktop/frogg3rs/openspec/changes/
archive
frogg3rs-envelope-curve-direction
$ git -C /Users/diegoaguilar-canabal/Desktop/frogg3rs status --short MANUAL.md QUICK_DICT.md
 M MANUAL.md
 M QUICK_DICT.md
$ git -C /Users/diegoaguilar-canabal/Desktop/frogg3rs diff -U0 MANUAL.md | grep '^@@'
@@ -452,3 +452,6 @@ position.
@@ -579,2 +582,6 @@ tanh-style saturator (top of travel) inside the waveshaper stage. The saturator'
$ git log --oneline HEAD..main | wc -l
       0
$ grep -n 'Delivery' /Users/diegoaguilar-canabal/Desktop/frogg3rs/openspec/changes/frogg3rs-envelope-curve-direction/proposal.md
30:- **Delivery:** one commit and a push to `main`; then a message to the session working in `.claude/worktrees/midi-resilience` that main is final and it can rebase. No pull request. No AI attribution.
```

`proposal.md:197` makes the pass condition explicit — "empty output is this
row's own pass condition, not a fact recorded once and reused" — and that pass
condition fails right now. proposal.md also contradicts tasks.md on the same
fact (`proposal.md:196` records both former parties as archived into `main`);
the proposal is the correct half.

**What breaks.** Four things at once. The prescribed `grep -c` has no file at
any path in this checkout. The traced `status` assertion is false and no branch
is written for the actual result. The live editor of `MANUAL.md` and
`QUICK_DICT.md` — `frogg3rs-envelope-curve-direction` — is named in neither
repository's artifacts. And that change's own Delivery section commits it to a
push to `main`, which makes `git log --oneline HEAD..main | wc -l` nonzero, on
which frogg3rs task 1.2 says STOP: the pair's opening gate is scheduled to fail
by a change neither artifact mentions. The content collision is in fact benign
(the live hunks at `:452` and `:582` are disjoint from this change's
`MANUAL.md:258-390`) — but that survives by accident of line range, not by any
reasoning the artifacts contain, and an executor cannot reach it from anything
written down.

**Remedy.** Task 3.2 and design.md's Risks bullet name
`frogg3rs-envelope-curve-direction` as the live `MANUAL.md`/`QUICK_DICT.md`
holder with its measured hunk ranges and their disjointness from `:258-390`;
the `frogg3rs-delay-capacity-and-width-finish` row and its `grep -c` recipe are
deleted; task 3.2 states what the executor does when the re-run returns a
non-empty `status` (what makes it safe to proceed, what makes it a STOP); and
task 1.2 states its disposition for the rebase that change's Delivery section
commits to requesting.

---

## BL-3 — Sheaf task 3.10's opening precondition asserts an equality that is false on the tree, names a spelling the file does not use, and supplies a command for only one of its two operands

**Sources:** B3; F/ADV6 (filed SHOULD-FIX). **CONFIRMED. Blocks execution of
Sheaf task 3.10**, the positive-control task for the whole held-modifier group,
and so 7.2/7.3 and frogg3rs 4.1's gate.
**Class:** an assertion written before it was measured.

Sheaf `tasks.md:750-755`, verbatim:

> `engine_tests.cpp` has no construction of `Engine<App>` with a settable
> timestamp today: run `grep -c 'return std::uint64_t{0}' tests/engine_tests.cpp`
> and confirm the count equals the number of `Engine<App>` constructions in the
> file, i.e. that every existing construction passes a fixed zero-returning
> lambda and none passes an advanceable one. (Record both numbers as the
> command prints them …)

My runs, in `External/Sheaf/projects/synth`:

```
$ grep -c 'return std::uint64_t{0}' tests/engine_tests.cpp
55
$ grep -c 'Engine<App>' tests/engine_tests.cpp
0                                                    (exit 1)
$ grep -cE 'synth::Engine<[A-Za-z_]+>[[:space:]]+[a-zA-Z_]+[({]' tests/engine_tests.cpp
73
$ grep -nE 'synth::Engine<[A-Za-z_]+>[[:space:]]+[a-zA-Z_]+\(\[\][[:space:]]*\{[[:space:]]*return std::uint64_t\{[1-9]' tests/engine_tests.cpp | wc -l
      17
```

Three failures in one instruction: the literal `Engine<App>` occurs zero times
(the file spells it `synth::Engine<EngineTestApp>` and five other
instantiations); 55 ≠ 73, so no count of constructions can equal the marker
count, and "both numbers" cannot be recorded because only one command is given;
and the proposition the equality stands in for is false — 17 constructions pass
a fixed **non-zero** lambda.

**What breaks.** The executor is ordered to confirm an equality that does not
hold, at the first step of the task that supplies every positive control for
the held-modifier group. They either STOP on a non-defect or record a number
pair that means nothing.

**Remedy.** Replace the equality with the assertion that is true and
mechanically checkable — that no existing construction passes an *advanceable*
(capturing) clock — with the command that settles it, e.g.
`grep -nE 'synth::Engine<[A-Za-z_]+>[[:space:]]+[a-zA-Z_]+[({]'` listing every
construction and a confirmation that each lambda's capture list is `[]`.

**Severity dispute ruled.** F graded this SHOULD-FIX while writing
"**Blocks: execution** (of task 3.10's first step)" in the same finding. By the
brief's own definition a finding that blocks execution is not a should-fix.
BLOCKING stands.

---

## BL-4 — The tick convention for Sheaf tasks 7.7, 7.8 and 7.9 is written in neither repository, so frogg3rs 4.1 step 2's required `0` is either unreachable or reachable only by recording a completion nobody performed

**Sources:** B4 (7.8/7.9) and F/ADV3 (7.7), consolidated — they are two halves
of one unwritten convention and the repair is one statement covering all three.
**CONFIRMED. Blocks execution of frogg3rs group 4, task 3.1 and task 5.2, and
therefore delivery.**
**Class:** a cross-repository handoff where one repository asserts a convention
the other never wrote, and no artifact names the actor who performs it.

frogg3rs `tasks.md:301-310`, step 2, verbatim:

> `git -C External/Sheaf show HEAD:openspec/changes/midi-controller-resilience/tasks.md | grep -c '^- \[ \]'`
> reports `0` — every task through 7.9 is ticked in the commit at `HEAD`, not
> only in the working tree (task 7.7, archiving in that repository, may be
> reported *blocked* rather than complete … **but is still ticked, since
> Sheaf's own task 7.7 states that a blocked archive does not block 7.8's
> commit**; if it is unticked, that is a different, unresolved state and this
> gate does not pass).

Sheaf task 7.7, verbatim ending: "This task is written so that the blocked
archive is reported and does not block 7.8: the branch is committed either way,
and what the commit carries is stated in the report." It says *reported* and
*does not block 7.8*. It nowhere says **ticked** — frogg3rs draws an inference
and attributes it to Sheaf as a quotation.

Sheaf task 7.8, verbatim opening and close: "**Coordinator commit point.** The
executor commits nothing. When 7.1-7.7 have been reported … the **coordinator**
commits this branch … Verify a clean tree at the end: `git status --short`
empty." 7.9 follows it and is the last task in the file.

My runs:

```
$ grep -n -i 'tick' External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md
   … 21 hits, none at or about 7.7, 7.8 or 7.9 …
$ grep -c '^- \[ \]' External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md
46
$ grep -c '^- \[x\]' External/Sheaf/openspec/changes/midi-controller-resilience/tasks.md
0
$ grep -c "smi-14\|smi-16" External/Sheaf/openspec/specs/synth-midi-instrument/spec.md
0
$ ls External/Sheaf/openspec/changes/ | grep -E 'app-midi-catalog|shift-and-file-export'
app-midi-catalog
shift-and-file-export
```

**What breaks.** Two independent ways the gate fails to reach `0`.

(a) 7.7's archive is refused today and is *expected* to stay refused this cycle
— both prerequisite changes are still live directories, and the operator's own
ruling defers the Sheaf archive to the upstream step. The actor who would have
to write `- [x]` beside 7.7 is reading Sheaf 7.7, which tells them to "STOP and
report; do not force the archive" and says nothing about ticking. If they leave
it unticked, frogg3rs 4.1 reports `1` and, by step 2's own words, "this gate
does not pass" — permanently. If they tick it, a completion that did not happen
is written into the pinned commit and the pushed branch.

(b) 7.8 is self-referential (it must be ticked in the commit it itself makes)
and 7.9 is a closing statement, not an action. Nothing in either repository
says who ticks them or that the ticks precede the commit, so a coordinator
following 7.8 literally commits with two unticked items and the gate reports a
nonzero count with no written remedy.

**Remedy.** One statement, written in both repositories and agreeing: Sheaf 7.8
says the coordinator ticks 7.7 (with its blocked state recorded in the task's
own text and in the report), 7.8 and 7.9 in the working tree and *then* commits,
so the commit carries zero unticked items; frogg3rs 4.1 step 2 says that is what
it is measuring, and drops the clause attributing the 7.7 convention to Sheaf.
Alternatively 4.1 step 2 accepts `0` or `1` and, on `1`, requires the single
unticked item to be `7.7` — but that still needs the 7.8/7.9 writer named.

**Severity disputes ruled.**

- **C refuted B4's blocking framing**, arguing that 7.8's closing "Verify a
  clean tree at the end: `git status --short` empty" forces tick-then-commit,
  so the gate can pass. **I refute the refutation.** A commit that carries
  `tasks.md` with 7.8 and 7.9 unticked also leaves a clean tree — the check
  constrains only that nothing is uncommitted, never that the ticks were
  written. Both orderings satisfy 7.8's last line; only one satisfies frogg3rs
  4.1 step 2, and nothing written selects it. B4 stands, at BLOCKING.
- **F withdrew its own BLOCKING grade on ADV3** on the ground that 4.1 step 2's
  parenthetical gives the executor an unambiguous, if wrong, direction. That is
  true of the *frogg3rs* executor and false of the actor who must write the
  tick, who is reading Sheaf 7.7. Graded BLOCKING inside this consolidated
  finding.

---

## BL-5 — Sheaf task 1.9's rule 3 has no prose carve-out, so four of this change's own `Check:` lines become unresolvable the moment rule 2(b) forces their rewrite; the gate sits first in a linear `test:` prerequisite list and takes every Sheaf test binary down with it

**Source:** C1 — reached by exactly one report. **CONFIRMED. Blocks delivery
of the Sheaf half**, and in practice blocks the change's own verification from
task 1.9 onward.
**Class:** a gate specified without the carve-out its already-shipping sibling
carries, turned red by the change's own text.

Rule text, read verbatim from Sheaf `tasks.md`:

> 2. It begins `not yet delivered`. Resolves **only if both** hold: (a) its own
>    prose names a task number `N.M` that is present as a `- [ ]` item … and
>    (b) **no** backticked token on the line already resolves as a real test
>    case under rule 3. … Condition (b) is what stops a delivered claim from
>    staying stale: the moment the named test exists, the line must be
>    rewritten to say so.
> 3. Otherwise every backticked token on the line must resolve as one of:
>    `<basename>: <case>` …; a bare `<case>` resolved against the most recent
>    file named earlier on the same line; or the name of a script that
>    `projects/synth/Makefile`'s or `apps/miniapp/Makefile`'s own `test:`
>    prerequisite list actually runs …

and, in the same task: "**Every task below that delivers a named test therefore
rewrites, in that same task and before ticking it, every `Check:` line citing
that task**".

My run, over the three delta files:

```
$ python3 - <<'EOF'
import re,glob
for f in sorted(glob.glob('specs/*/spec.md')):
    for n,l in enumerate(open(f),1):
        m=re.match(r'^\s*-\s*Check:\s*(.+)$',l)
        if m:
            t=re.findall(r'`([^`]+)`',m.group(1))
            if len(t)>1: print(f"{f}:{n} tokens={len(t)} -> {t}")
EOF
specs/synth-midi-instrument/spec.md:71 tokens=8 -> ['engine_tests.cpp: HeldModifierCeilingIsWallClockNotPumpCount', 'MessageThreadTick()', 'engine_tests.cpp', 'Engine', 'midiProcessors_', 'Engine', 'instrument_tests.cpp', 'Engine<App>']
specs/synth-midi-instrument/spec.md:76 tokens=3 -> ['engine_tests.cpp: HeldModifierCeilingDoesNotFireBeforeItElapses', 'Engine', '0']
specs/synth-midi-instrument/spec.md:81 tokens=3 -> ['engine_tests.cpp: NeverHeldModifierSurvivesTheCeilingWithRebuildAsItsClearSource', 'Engine', 'held']
specs/synth-midi-instrument/spec.md:97 tokens=11 -> ['MidiConnectionManagerReconcileTests.cpp: EndpointOpenClearsHeldModifierAtRuntimeBinding', 'MidiConnectionManager::Reconcile', 'Engine<App>', 'browser_midi_bridge_tests.cpp: EndpointOpenClearsHeldModifierThroughTheAbiEntryPoint', 'browser_midi_bridge_tests.cpp: ReconcileAloneDoesNotClearAHeldModifier', 'openInput', 'openOutput', 'Engine', 'midi-timing.test.mjs', 'applyAction', 'version-drift.test.mjs']
```

Exactly four lines carry more than one token, and no other `Check:` line in any
of the three deltas does. All four begin `not yet delivered` today, so rule 2
governs them and the gate is green on arrival:

```
$ for n in 71 76 81 97; do sed -n "${n}p" specs/synth-midi-instrument/spec.md | cut -c1-40; done
- Check: not yet delivered; task 3.10 ad
- Check: not yet delivered; task 3.10 ad
- Check: not yet delivered; task 3.10 ad
- Check: not yet delivered; task 3.7 add
$ grep -h '^- Check:' specs/*/spec.md | wc -l ; grep -h '^- Check: not yet delivered' specs/*/spec.md | wc -l
      31
      24
```

and the gate is wired first in a linear prerequisite list:

```
$ grep -n '^test:' External/Sheaf/projects/synth/Makefile
274:test: check-ui-boundary check-ui-boundary-empty-discovery check-app-bundle-plist test-wasm32 $(TEST_BIN) … $(CONTROLLER_WIZARD_TEST_BIN)
```

**What breaks.** `Engine`, `Engine<App>`, `midiProcessors_`,
`MessageThreadTick()`, `openInput`, `openOutput`, `applyAction`, `0` and `held`
are none of rule 3's three accepted shapes; `MidiConnectionManager::Reconcile`
carries no colon-space so it falls to the bare-case branch and resolves as
nothing; bare `engine_tests.cpp`, `instrument_tests.cpp`,
`midi-timing.test.mjs` and `version-drift.test.mjs` name a file with no case,
which the rule explicitly treats as not a check. So tasks 3.7 and 3.10 cannot
both perform the rewrite rule 2(b) demands *and* tick without turning the gate
red — and because the gate is the change's own first `test:` prerequisite,
`make -C projects/synth test` then runs no binary at all. Task 1.9's own
"Green in each state" analysis reasons about the cited case appearing and never
about the other backticked tokens rule 3 then demands. The defect cannot be
found by running the script before execution; it fires only at tick time.

The assertion the executor needs is written down one repository over and was
not carried across:

```
$ sed -n '97,101p' app/check_spec_checks_resolve.py
# A token is evidence if it looks like a path or a test name: it carries a
# separator, or it is snake_case. A bare CamelCase or lowercase word is part of
# the sentence -- `scrollWidth`, `BlockStartPos` -- and is not required to
# resolve to anything.
NAMES_SOMETHING = re.compile(r"[/.]|_")
```

**Remedy.** Task 1.9's rule 3 states the carve-out rather than leaving the
executor to invent it: a line passes if at least one token is a real case, a
wired gate, or a rule-1/rule-2 declaration, and only tokens of
`<basename>: <case>` shape are required to resolve. Note that frogg3rs's regex
alone is not sufficient here — `midiProcessors_` carries `_` and would still be
demanded — so the "at least one token is a real case" clause is the load-bearing
half. Then re-apply the amended rule by hand to all 31 lines and record the
result in the task.

---

## BL-6 — frogg3rs task 4.1's cross-repository gate rejects almost nothing: step 2 reads self-reported ticks, and steps 3-4's comment filter drops only one of three comment spellings

**Source:** E/ADV4 — reached by exactly one report. **CONFIRMED. Blocks
execution:** 4.1 is the only gate deciding whether groups 3, 4 and 5 run.
**Class:** a gate that reads declarations where it must read deliverables.

Task 4.1's own stated rationale, verbatim: "Reading the symbols out of `HEAD`
(a commit), not the working tree, and filtering out any matched line whose text
begins with `//`, is what stops a stray comment or a
`// TODO: add declaredPreconditions` from satisfying steps 3-4 without a real
declaration existing."

My run, against the literal `git grep -n` output shape and the gate's own
filter:

```
$ cat adj_filter.txt
HEAD:x.hpp:1:int z; // declaredPreconditions
HEAD:x.hpp:2:  /* declaredPreconditions is coming later */
HEAD:x.hpp:3:    // TODO: add declaredPreconditions
HEAD:x.hpp:4:    std::vector<std::string> declaredPreconditions;
$ grep -v -E ':[[:space:]]*//' adj_filter.txt
HEAD:x.hpp:1:int z; // declaredPreconditions
HEAD:x.hpp:2:  /* declaredPreconditions is coming later */
HEAD:x.hpp:4:    std::vector<std::string> declaredPreconditions;
```

Only the leading-`//` spelling — the one the task names — is dropped. A
trailing `//` comment and a `/* */` block comment both satisfy steps 3 and 4.

**What breaks.** Step 2 cannot distinguish a delivered Sheaf half from a ticked
task list (and BL-4 shows those ticks are either unwritable or writable only
falsely). Steps 3 and 4 probe two *data declarations* and are satisfiable by
comment text. The symbols the frogg3rs half then documents — the ceiling
constant, the rate limiter, the new browser ABI export, task 3.5's Engine
accessors — are invisible to the gate entirely. So the pair can pin a Sheaf
commit in which the recoveries do not exist, task 3.1 writes `MANUAL.md` text
asserting them, and every gate stays green.

**Remedy.** Steps 3-4 anchor to a declaration shape rather than to a
line-leading `//`, and the gate adds probes read out of `HEAD` against the
delivered artifacts — the two `engine_tests.cpp` ceiling cases,
`browser_midi_bridge_tests.cpp: ReconcileAloneDoesNotClearAHeldModifier`, the
new `_synth_browser_*` name in `build-browser-apps.mjs`, and
`TemplateChangeRateLimiter`/`kHeldModifierCeilingMicros` in
`include/synth/MidiController.hpp`. Step 2 is replaced rather than amended, per
BL-4.

---

## BL-7 — Task 6.6's check is satisfied by rewriting every deferred `Check:` line to begin `operator step`, with zero tests delivered, and the rewritten line then passes permanently after promotion

**Source:** E/ADV5 — reached by exactly one report. **CONFIRMED. Blocks
delivery:** 6.6 is the last gate before 6.7's `openspec archive`.
**Class:** a check whose assertion is not the assertion the task means.

Task 6.6's Check, verbatim: "`grep -h '^- Check:' …/specs/*/spec.md | grep -c
'not yet delivered'` reports `0`, and `python3 app/check_spec_checks_resolve.py
app` exits `0`. Positive control: leave exactly one `not yet delivered` line
unrewritten and confirm the first of those two commands reports a nonzero
count."

My runs, against the shipped script's own objects:

```
$ python3 -c "<load app/check_spec_checks_resolve.py, match one line>"
'operator step -- somebody looks at the screen once'  NO_CHECK match: True  tokens: []
$ sed -n '383,385p' app/check_spec_checks_resolve.py
                if declared and not tokens:
                    declared_manual += 1
                    continue
$ sed -n '96p' app/check_spec_checks_resolve.py
NO_CHECK = re.compile(r"^\s*(none|operator step|not yet delivered)", re.I)
$ sed -n '111p' app/check_spec_checks_resolve.py
    specs = os.path.join(repo, "openspec", "specs")
$ echo hello | grep -c 'not yet delivered'; echo exit=$?
0
exit=1
```

**What breaks.** A line rewritten to `- Check: operator step — …` carrying no
backticked token matches `NO_CHECK`, takes the `declared and not tokens`
branch, is counted as declared-manual, and the script exits `0`; the grep then
reports `0`. Both of 6.6's conditions are met with nothing delivered. The
specified positive control proves only that `grep` counts — it says nothing
about what the rewritten lines say. And the script scans `openspec/specs/`, so
once promoted at 6.7 the laundered line keeps passing forever. Sub-note, also
reproduced above: `grep -c` exits `1` while printing `0`, so an `&&`-chained
form of 6.6's own check reads failure at the moment of success.

**Remedy.** 6.6's check additionally asserts that the count of `Check:` lines
beginning `operator step` or `none` is unchanged from the figure recorded at
task 1.6 (today's baseline for the whole repository is
`check-spec-checks-resolve: OK - 55 Check reference(s) resolved, 18 declared as
having no automated check`), and that the script's printed "resolved" figure
rose by the number of lines 6.6 rewrote. The delta names the delivering case
for each deferred line so the rewrite is a substitution rather than a free
choice.

---

## BL-8 — smi-17's "the controller's other input still flows" clause cannot fail at the chain position task 5.1 specifies

**Source:** F/ADV1 — reached by exactly one report. **CONFIRMED. Blocks
delivery of the Sheaf half:** the scenario is promoted into `openspec/specs/`
naming a check that cannot fail on the half of the requirement it exists to
defend.
**Class:** a check whose assertion cannot fail.

Sheaf task 5.1, verbatim: `TemplateChangeMidiInProcessor` is "appended to the
input chain in `CreateMidiControllerProfileImpl` … the same way those are."
smi-17's scenario clause (`specs/synth-midi-instrument/spec.md:111`): "**THEN**
every control-change message on a mapped address is processed, so the
recognizer and the rate limiter neither consume a message meant for another
processor in the input chain nor stop passing messages along it."

My runs:

```
$ sed -n '3035,3046p' External/Sheaf/projects/synth/src/MidiController.cpp
    auto appendInput = [&](std::unique_ptr<MidiInProcessor> processor) {
        …
        tail->SetThru(processor.get());
        tail = processor.get();
        result.inputThru.push_back(std::move(processor));
    };
$ sed -n '3048,3074p' External/Sheaf/projects/synth/src/MidiController.cpp | grep -nE 'appendInput|has_value|empty'
    if (config.encoderInput.has_value())   → EncoderMidiInProcessor
    if (config.analogInput.has_value())    → AnalogMidiInProcessor
    if (!config.systemMessages.empty())    → SystemButtonMidiInProcessor
    if (config.pressureInput.has_value())  → PolyphonicPressureMidiInProcessor
                                           → RealtimeMidiInProcessor
$ sed -n '829,853p' External/Sheaf/projects/synth/src/MidiController.cpp
void AnalogMidiInProcessor::Process(const BasicMidi& midi) {
    if (!midi.IsCC()) { PassToThru(midi); return; }
    …
    if (const AnalogMidiMapping* mapping = FindGesture(midi))      { Push(…); return; }
    if (const AnalogAppActionMapping* mapping = FindAppAction(midi)) { Push(…); return; }
    if (config_.sceneBlend.has_value() && *config_.sceneBlend == address) { Push(…); return; }
    PassToThru(midi);
}
$ grep -n 'PassToThru(midi);' External/Sheaf/projects/synth/src/MidiController.cpp
508: 527: 764: 831: 852: 901: 911: 935: 941:
```

`appendInput` is a tail append, and every upstream processor returns on a match
without forwarding — `PassToThru` is reached only on a miss (`:764` encoder;
`:831`/`:852` analog; `:935`/`:941` system button, both on no-address /
no-association).

**What breaks.** A control change on a **mapped** address is consumed upstream
and never reaches a tail-appended recognizer, so "every control-change message
on a mapped address is processed" is true whatever the recognizer does —
including if it swallowed every message it saw and never forwarded. Nothing
sits downstream of the tail, so the second half has no observable either. Task
5.3's own disclosure claims the opposite of what the placement permits: "what
this asserts is that the new recognizer and limiter neither swallow a message
meant for another processor nor stop passing messages down the chain, which is
a real way to break the input path and the only one available here." At the
tail it is not available at all. The specified positive control ("with the rate
limiter removed or bypassed … `N > 100`") exercises only the count bound.

**Remedy.** Either task 5.1 states that the recognizer is inserted at the
**head** of the chain — behaviourally free, since no existing processor matches
SysEx — so the pass-through claim becomes real; or task 5.3 and the scenario
interleave control changes on addresses the profile does *not* map, which do
reach the tail, and assert those still reach `RealtimeMidiInProcessor`/`thru_`.
Either way the scenario's wording and the placement must name the same thing.

---

# 2. Refuted blocking finding

## A2 — "The seventh device default has a shape no existing default has, and the all-defaults Controllers-page case that will run against it is named in no artifact" — **BLOCKING severity REFUTED; downgraded to SHOULD-FIX (see SF-1)**

**Source:** A2 (BLOCKING, OPEN). Refuted on severity in E's exchange; confirmed
structurally by B, C, D, E and F. Ruled here by the parallel measurement.

The structural claims are true and I re-verified the one that matters:

```
$ grep -c 'real_catalog_defaults_generate_and_accept_adds_through_the_view_model\|GenerateCatalogSlots' \
    openspec/changes/frogg3rs-midi-preset-preconditions/{proposal,tasks,design}.md
proposal.md:0
tasks.md:0
design.md:0
```

The behavioural half — the half on which the BLOCKING grade rested — is settled
by `measure-a2-report.md`, which built `$(CONTROLLERS_PAGE_BIN)` and
`$(MIDI_CATALOG_BIN)` in a throwaway worktree at `0acc093` with a Generic,
analog-only seventh default present and ran both binaries by path, with a
matching baseline run after reverting:

- WITH the seventh default: `[PASS] real_catalog_defaults_generate_and_accept_adds_through_the_view_model`.
  The only two failures were the two count assertions
  `FroggersControllersPageTests.cpp:99` and `FroggersMidiCatalogTests.cpp:399`,
  both `catalog.deviceDefaults.size() == 6` — the assertions task 5.5 already
  names. (`:109` is never reached, because `REQUIRE_TRUE` throws at `:99`.)
- WITHOUT it: both binaries exit `0`, so the two failures are attributable to
  the added entry and nothing else.

So the feared outcome does not occur: the wizard, `GenerateProfile`,
`AddController` and the whole Add/Block matrix accept an analog-only `Generic`
default, and task 5.5's three-assertion enumeration is complete in effect.
There is no red gate, therefore no blank an executor would fill toward green by
weakening the case — which was the entire blocking mechanism.

**What survives** is an Impact/enumeration gap with no runtime consequence:
`real_catalog_defaults_generate_and_accept_adds_through_the_view_model` and
`GenerateCatalogSlots` are sites the seventh default exercises and are named in
none of the three artifacts. Carried as SF-1.

---

# 3. SHOULD-FIX, consolidated

"Verified" means I ran the command or read the artifact myself in this session.
"Not adjudicated" means I did not — verify at the point of repair; the finding
is recorded with its source so the author can reach the original.

| id | finding | sources | status |
|---|---|---|---|
| SF-1 | The all-defaults Controllers-page case and `GenerateCatalogSlots` are named in no artifact, though the seventh default exercises both. Add them to task 5.5 and Impact; carry the measured result (both binaries fail only on the two named count assertions, and the all-defaults case passes). | A2 (downgraded), D14 (self-narrowed) | **verified** — grep 0/0/0 above; outcome settled by `measure-a2-report.md` |
| SF-2 | The disassembly block that carries CC 77 records no command: its `$` line is prose ("`$ python3.11 disassembly of LaunchControlXL.pyc: make_slider and the four control-row comprehensions in _create_controls`"), and its opnames lie outside the one recorded command's filter. Record the actual recursive walk over `co_consts`. | A5 | **verified** — read `design.md:572-582` |
| SF-3 | The interpreter sentence beside it is self-contradicting: `design.md:565-570` says "`python3 -V` reports `Python 3.11.14` on this machine" and three clauses later that `python3` is 3.13. Measured: `python3 -V` → `Python 3.13.5`; `~/.local/bin/python3.11 -V` → `Python 3.11.14`; `/usr/bin/python3 -V` → `Python 3.9.6`. A reader following the sentence re-runs under the interpreter the parenthetical exists to warn against. | A6 | **verified** |
| SF-4 | Task 4.2 declares itself "the last task in this change that touches `External/Sheaf`'s checkout" while task 4.6's positive control edits `MidiController.cpp` in that checkout. Make the ordering explicit, or reword 4.2 and add a revert-and-re-check to 4.6. | A4 | not adjudicated |
| SF-5 | Sheaf design.md's "the browser's `ops.resync` … enqueues an action and touches no engine state" is false at `BrowserMidiBridge.hpp:171-174`, which calls `engine_.ResetMidiOutputProcessors(ix)` synchronously. The conclusion survives; the stated reason is what a later change would reuse. | A11 | not adjudicated |
| SF-6 | Sheaf task 1.9 and design.md state 130 `Check:` lines / 29 this change's / 22 deferred; measured 132 / 31 / 24 (103 spec files and "roughly 101 in other deltas" are both right). Replace the literals with the commands. | B5, C3, E/ADV20, F/ADV16 | **verified** — `31` and `24` measured above |
| SF-7 | Task 1.6 says "run by path directly" for ten checks; three are shell scripts, one has a filename the task never gives (`check_microphone_usage_strings.sh`), and one exits `1` without the five action arguments that live only in the Makefile recipe. Give each its actual invocation, or say "read each recipe from `app/Makefile`". | B6 | not adjudicated |
| SF-8 | The recovery half's absence rule quotes `"pressed and released again,"` with a trailing comma; `MANUAL.md:320` ends the sentence with a period, so the BLOCK-13 control matches nothing. Its second clause ("or any other sentence naming the modifier's own button as what clears it") has no pattern and cannot be implemented. | B7, E/ADV2, F/ADV7 | not adjudicated |
| SF-9 | The recovery half's presence rule is an OR over four phrases, so a manual naming one trigger passes while the requirement says "triggers" plural and task 3.1 must name three with per-host availability; rule 3's plugin exclusion fires only on one exact literal. Make rule 1 a conjunction over the host-available triggers and trigger rule 3 on any reconnect-shaped substring. | E/ADV1, E/ADV3, F/ADV7 | not adjudicated |
| SF-10 | Tasks 5.1/5.3 leave `FroggersMidiCatalogTests` red until 5.5, and 5.6's marker pair leaves the drift half red until then, aborting `make -C app test` before any binary runs. Fold the count updates into 5.1, or say at 5.3 that the binary is run by path while the gate is red. | B12, F/ADV19 | not adjudicated |
| SF-11 | Sheaf rule 3's basename resolution has no `dist/`, `build/`, `node_modules/` exclusion, and this change's own approved `npm ci` + `npm run build` emits `dist/tests/midi-timing.test.mjs` and `version-drift.test.mjs`, making every browser basename a collision the rule says to report. | C2 | not adjudicated (argued from `tsconfig.json`/`package.json`, not an observed `dist/`) |
| SF-12 | Sheaf rule 1 ("begins `none` or `operator step`. Resolves. Nothing else on the line is read") reintroduces the hole frogg3rs's script documents having shipped a false claim through. Same edit as BL-5. | E/ADV7 | not adjudicated |
| SF-13 | `app/Makefile:5-7` cites `External/Sheaf/projects/synth/Makefile:89` for the `libsynth.a` rule; line 89 is blank and the rule is `$(LIB): $(OBJ)` at `:123`. `app/README.md:16` cites `…/sheaf-patch/Makefile:47-48`; the pair is at `:50-51`. `check_citations_resolve.py` excludes citations into `External/Sheaf` by construction, so nothing mechanical finds these. | D3 | **verified** — line 89 prints empty; `$(LIB):` at `:123` |
| SF-14 | `app/Makefile:317` is a section banner for a `stop-flush-repro` target that exists nowhere in the tree, followed immediately by the next banner. | D4 | **verified** |
| SF-15 | `openspec/.sessions/marbles-mod-led-level-meter-progress.md` is tracked first-person session correspondence for a change archived in June, citing four commit ids of which three are not objects. It is the directory's only content and sits inside a directory this change's §8.0 sweep claims. | D5 | **verified** — file present, sole content |
| SF-16 | Three stale line citations inside Sheaf's swept directories (`RuntimeMainComponent.hpp:301-302` → `ControllersPageUI.hpp:1074`; `MainPane.hpp:94` → `RuntimeMainComponent.hpp:156`; `engine_tests.cpp:1497` → `BrowserRuntime.hpp:717`). | D6 | not adjudicated |
| SF-17 | `projects/synth/tests/runtime_main_component_tests.cpp:98` carries `// TODO(tasks 11-13)` — a planning reference in shipping code, in a directory the Sheaf sweep claims and outside `check-no-planning-history`'s `app/`-only scope. | D7 | not adjudicated |
| SF-18 | A third dangling doc link in `projects/synth/docs` (`browser-audio-underrun-diagnosis.md:33` → `../browser/src/audio-worklet.ts`, which does not exist); task 7.6 is scoped to `coverage.md`'s table and does not reach it. | D8 | not adjudicated |
| SF-19 | The ceiling's `now > heldSinceMicros` guard turns the ceiling off permanently for a modifier if `now` ever steps backwards, and the browser's `now` is adjustable at runtime via `SetTimestampEpochOffsetMicros`. All three ceiling cases drive a monotonic test lambda and cannot observe it. | E/ADV15 | **OPEN on the browser half** — see §5 |
| SF-20 | The flood test's bounds admit a limiter running at 5% of its configured rate (`N == 5` satisfies every assertion), and separately its upper bound `N <= rate × 5` has zero margin and is red for any token bucket with capacity ≥ 1 — which design.md's own "token bucket" decision requires. Mechanism and bound must name the same thing. | E/ADV11, F/ADV12 | not adjudicated |
| SF-21 | The template-change recognizer has no negative case: every message either check feeds it is the matching pattern, so a recognizer matching any SysEx passes. | E/ADV12 | not adjudicated |
| SF-22 | sru-63's mismatch report renders nothing for a configured controller that has transmitted nothing — byte-identical to a perfectly matching controller — and two of its checks are satisfied by that silence. | E/ADV13 | not adjudicated |
| SF-23 | Sheaf task 1.9's script fails only when it finds **zero** marked files, and control (d) blesses a reduced marked-file count, so the largest delta leaves the gate's scope by deleting one HTML comment. | E/ADV8 | not adjudicated |
| SF-24 | Two of the three `engine_tests.cpp` ceiling cases are green against an Engine with no controller slot at all (task 3.5's accessors return `nullptr` for that shape by design), and no artifact states how a test *holds* a modifier on an Engine. | F/ADV4 | not adjudicated |
| SF-25 | `AWriterWithNoTimestampProviderRecordsZeroAndTheCeilingFiresAtItsEarliest` is not constructible through any seam this change adds; the only reachable route works solely for a profile whose system-button processor is the chain head, which no artifact states. | F/ADV5 | not adjudicated |
| SF-26 | The drift half compares empty against empty for four of the seven devices (APC40 Ableton and the three Launchpads), and the WARNING scan is known to score 0 on exactly those subsections. Render an explicit empty statement so the comparison has text to disagree with. | F/ADV8 | not adjudicated |
| SF-27 | `MANUAL.md:339` ("CC Hold is also what lets the app see the Shift button's own release") is falsified by task 4.3's own rewording and sits outside rule 2's heading scope, outside the marker pairs, and inside the only subsection the WARNING scan already flags. | F/ADV9 | not adjudicated |
| SF-28 | Eight of the ten frogg3rs deferred `Check:` lines name no case anywhere in the delta, so 6.6's rewrite is unconstrained in content as well as in form. Same edit as BL-7. | F/ADV10, E/ADV5 | not adjudicated |
| SF-29 | Task 3.6's export derivation covers only `extern "C"` **definitions** in `BrowserRuntimeAbi.cpp`, and the file already models the declared-here/defined-elsewhere shape — so the one export this change adds can ship absent from `EXPORTED_FUNCTIONS` with the derivation green. State that the new entry point is defined with a body in that file. | F/ADV11 | not adjudicated |
| SF-30 | The LCXL's only Shift assertion iterates zero associations (its `systemMessages` is empty), and task 4.5's "prove the control on the last entry" cannot be applied as specified because all three Launchpads come from one shared factory. | F/ADV13 | not adjudicated |
| SF-31 | Whether the new emitter binary joins `test:`'s executed list (making `app/README.md:27`'s "all twelve" wrong) is stated nowhere, and `app/README.md` is in no Impact. | D9 | **OPEN** — see §5 |
| SF-32 | `npm ci` under `projects/synth/browser` is approved by the operator but `node_modules` is absent and a default-sandbox executor cannot install; tasks 1.5, 3.6 and 7.2 all need an installed tree. | B8 | **OPEN** — see §5 |

---

# 4. NOTE, consolidated

All recorded with source ids. Verified where marked; otherwise **not
adjudicated — verify at the point of repair**.

| id | finding | sources | status |
|---|---|---|---|
| N-1 | The `ioreg -p IOUSB -w 0` citation in proposal.md and design.md records three USB devices; the command prints two host controllers and no peripherals, and the control that would separate "nothing attached" from "this command enumerates nothing" (`system_profiler SPUSBDataType`) is itself empty. The conclusion is undisputed; the record is wrong. | A7 | **verified** |
| N-2 | Impact omits two sites the tasks edit: the three Launchpad `MANUAL.md` subsections task 4.7 wraps in marker pairs, and `app/Makefile:127`'s "six real device defaults" comment task 5.5 edits. | A8 | not adjudicated |
| N-3 | Three claims about existing code are not true at the symbol named: (a) `Type::Shift` is negated nowhere in `FroggersMidiCatalogTests.cpp` and `:587-596` belongs to a different case than design.md names; (b) task 5.5's stated reason for the insertion position is wrong — the registry is built by `push_back` in iteration order, so position is pinned by `FroggersMidiCatalogTests.cpp:416-418` (`deviceDefaults[0]/[1]/[2]` bound to twister/generic/ableton), which task 5.5 does not name; (c) proposal.md's `shift_->held` grep searches a header the spelling does not appear in. | A9 | **(b) verified** — `:416-418` read; (a),(c) not adjudicated |
| N-4 | Task 1.2's parenthetical enumerates two occurrences of the forward-declared script name where the check reports three; the third is the parenthetical's own self-reference. The task records no figure, so nothing fails. | A10, B9, F/ADV20 | not adjudicated |
| N-5 | Sheaf proposal.md and design.md cite `ControllerWizard.cpp:925-950` as `MakeControllerWizardRegistry`'s definition; the signature is at `:926` and the closing brace at `:952`. | A12 | not adjudicated |
| N-6 | Sheaf Impact's itemized definition-site list omits `instrument_tests.cpp`, `engine_tests.cpp` and `controller_wizard_tests.cpp`, which its tasks edit; they are reached only by the closing directory sweep. | A13 | not adjudicated |
| N-7 | Task 4.7 names three of six `MANUAL.md` headings and three of six device ids; the other three Launchpad ids are arguments at a shared factory's call sites, one indirection further. | B10 | not adjudicated |
| N-8 | Sheaf task 1.3's "confirm the count is seven" is not reproducible by a literal operand grep (19-21 lines); the task rescues itself by enumerating all seven by line. | B11, C7 | not adjudicated |
| N-9 | Task 4.8's emitter binary has no build contract: no Makefile variable, no statement whether the check target takes it as a prerequisite, no statement how the script invokes it. It would be the first `check-*` needing a build. | B13 | not adjudicated |
| N-10 | The new browser ABI entry point is never named (only `synth_browser_*`), while a `Check:` asserts a test drives it "by name"; task 1.1's operand list omits it and five other names this change creates, so task 7.1's postflight enumeration is short by six. | C4 | not adjudicated |
| N-11 | Task 1.9's rule-3 parenthetical names `controller_wizard_tests.cpp` as a bare-function file; it uses `TEST_CASE` (28 of them). The two bare-function files prefix every case with `Test`, while all twelve `Check:` names for them are unprefixed. | C5 | not adjudicated |
| N-12 | Sheaf's two MODIFIED requirements amend `smi-14`/`smi-16`, which are in no promoted spec; `openspec archive` fails loudly and writes nothing, and task 2.5 already gates on exactly this. No remedy — keep the gate. | C6, D13 | **verified** — `grep -c "smi-14\|smi-16" …/openspec/specs/synth-midi-instrument/spec.md` → `0` |
| N-13 | Thirteen `preflight*.md` files (6,734 lines frogg3rs, 4,747 Sheaf) are tracked in the change directories and ship in the delivery push, then vanish at archive. Precedent is four of 106 archived changes. Operator's call — see §5. | D10 | not adjudicated |
| N-14 | `openspec/specs/froggers-midi-controller-mappings/spec.md`'s Purpose is still the generator's TBD stub naming a long-archived change; task 6.7 rewrites that file's Requirements anyway. | D11 | not adjudicated |
| N-15 | Five npm script names in `projects/synth/browser` are invoked by no gate; `test:unit` — the gate the Sheaf Impact leans on — runs only because this change types it, and no standing gate runs it afterwards. | D12 | not adjudicated |
| N-16 | Four `sru-*` requirement ids are each claimed by two or three unmerged Sheaf deltas. This pair's own four new ids collide with nothing. Operator's call at the upstream merge — see §5. | D13 | not adjudicated |
| N-17 | Impact completeness is otherwise good: every count site the seventh default falsifies is named, `MANUAL.md:277` is correctly omitted, `QUICK_DICT.md` is correctly omitted and proven rather than asserted, and all six cited `MANUAL.md` ranges resolve. The declared sweeps stop at the root `Makefile`, `src/` and `.github/` (nothing on this change's path), and at the Sheaf repo root. | D14, D15 | not adjudicated |
| N-18 | Task 6.6 states that frogg3rs's `check_spec_checks_resolve.py` "does not short-circuit on the 'not yet delivered' prefix the way Sheaf's does". It does short-circuit when the line carries no backticked token — which all ten deferred lines do. 6.6's conclusion is right; its stated reason is backwards. | E/ADV6, F/ADV10 | **verified** — the `declared and not tokens` branch, read above |
| N-19 | Sheaf rule 2(a) resolves a deferral against *any* unticked task number, so a line can cite a task that delivers no test. | E/ADV9 | not adjudicated |
| N-20 | Rule 3 resolves a name, never a claim: two smi-16 scenarios cite the same case. Reading the case settles it in the change's favour today; the accepting path stays open. | E/ADV10 | not adjudicated |
| N-21 | `git log --oneline HEAD..main | wc -l` prints `0` and exits `0` when `main` cannot be resolved, so task 1.2's staleness check reads green on a renamed or deleted `main`. Use `git merge-base --is-ancestor main HEAD` and read the exit code. Pairs with BL-2's scheduled-red direction. | E/ADV16 | not adjudicated |
| N-22 | Task 4.6's positive control tests a different seam from the scenario: `Process` is a synchronous per-message dispatch, so no implementation can make a press depend on a later release; both cases are green by construction. They are regression tripwires, and the `Check:` lines should say so. | E/ADV17, F (confirmed the control fires) | not adjudicated |
| N-23 | The declared-preconditions surface is literal-against-literal on both sides, disclosed as such by design.md; the supplementary WARNING scan never fails a build and already prints a false positive on `DEVICE ON/OFF`. | E/ADV18 | not adjudicated |
| N-24 | sru-64's only falsifiable check has no positive control, unlike tasks 4.2, 4.4 and 4.6. | E/ADV19 | not adjudicated |
| N-25 | Launchpad mapped-set membership ignores channel and control type (first match on the derived note number), so a colliding address reads as mapped and is reported in neither set. The spec blesses the predicate; no scenario presses a colliding address. | E/ADV14 | not adjudicated |
| N-26 | `HeldModifierCeilingIsWallClockNotPumpCount` does not separate its two hypotheses at low tick counts, and "the same elapsed time" carries no tolerance. | F/ADV14 | not adjudicated |
| N-27 | The below-ceiling control does not turn red when simulated elapsed time stays at zero — zero is strictly below the ceiling, and the `now > heldSinceMicros` term is then false. | F/ADV15 | not adjudicated |
| N-28 | The `-Wswitch` trap is pragma-scoped to one switch in one test file and constrains no production site; `DeviceLabel`'s fall-through would route a fourth enumerator to the Online label silently. Disclosed by task 4.5. | F/ADV17 | not adjudicated |
| N-29 | `SecondPress` on Shift is overwritten by the following release on healthy hardware, so the recorded clear source is observable in tests and effectively unobservable in production. Nothing renders it, so nothing breaks. | F/ADV18 | not adjudicated |
| N-30 | design.md:771 and task 4.1 cite "task 6.6" for the operator's push decision; the delivery task is 6.7 (design.md:719 cites it correctly, so the artifact disagrees with itself). | F/ADV20 | not adjudicated |

---

# 5. OPEN items

| id | what is open | what settles it | operator needed? |
|---|---|---|---|
| O-1 (SF-31 / D9) | Whether the new emitter binary joins `app/Makefile`'s `test:` executed list, making `app/README.md:27`'s "check that all twelve ran" wrong. | A decision stated in task 4.8. If the count moves, `app/README.md:27` joins the count sites task 5.5 repairs. | No — author's decision inside the change. |
| O-2 (SF-32 / B8) | Whether `npm ci` can run for tasks 1.5, 3.6 and 7.2. The operator has already approved the command (ruling 3); what is unsettled is whether a default-sandbox executor can perform it. | `npm ci --offline` succeeding against the local cache, or the coordinator granting network for that one command before dispatch and task 1.5 saying so. A dispatched executor without it reports BLOCKED at 1.5. | No — coordinator's grant, inside the operator's existing approval. |
| O-3 (SF-11 / C2) | Whether the `dist/` basename collision actually fires against Sheaf's new rule 3. Consequent on O-2. | `npm run build` in `projects/synth/browser`, then `find projects/synth -name 'midi-timing.test.mjs'`. | No. |
| O-4 (SF-19 / E-ADV15) | Whether the browser's `MessageTick` can deliver a `now` that steps backwards past `heldSinceMicros`, permanently disabling the ceiling on the host whose manual text promises it. | Read `browser/src/worker.ts`'s producer of `command.timestampMicros` to see whether the epoch offset is pre-applied, and add a case that holds a modifier, applies a negative offset via `SetTimestampEpochOffsetMicros`, drives `MessageTick`, and asserts the ceiling still fires. | No — a read plus one test. |
| O-5 (N-13 / D10) | Whether the thirteen tracked `preflight*.md` files are this cycle's output or working notes. The frogg3rs eight ship in the delivery push and are removed again by the archive. | The operator says which. If working notes, untrack before 6.7's push; if the record, say so in 6.7 so the push is deliberate. | **Yes.** |
| O-6 (N-16 / D13) | Which of the contending `sru-59`..`sru-62` claims promote first, on which this change's `sru-63`/`sru-64` numbering depends. | The promotion order chosen at the operator's upstream merge. Out of this cycle by ruling 2 and ruling 4. | **Yes**, at the upstream step. |

A2's behavioural half is **no longer open** — `measure-a2-report.md` settled it,
and the finding is refuted on severity (§2).

---

# 6. Coverage note

Confirmed blocking findings reached by **more than one report independently**:

| finding | reports that reached it |
|---|---|
| BL-1 (task 1.1's unreachable commit) | A1, B1, D1 — three |
| BL-2 (task 3.2's coordination party) | B2, D2, F/ADV2, and A3 at SHOULD-FIX — four |
| BL-3 (Sheaf 3.10's false equality) | B3, and F/ADV6 at SHOULD-FIX — two |
| BL-4 (the 7.7/7.8/7.9 tick convention) | B4 (7.8/7.9) and F/ADV3 (7.7) — two, each reaching a different half; neither reached the other's |

Confirmed blocking findings reached by **exactly one report**:

| finding | sole report |
|---|---|
| BL-5 (Sheaf rule 3 has no prose carve-out) | C |
| BL-6 (4.1's gate reads ticks and comments) | E |
| BL-7 (6.6 defeated by `operator step`) | E |
| BL-8 (smi-17's inert pass-through clause) | F |

Four of the eight confirmed blocking findings were each reached by exactly one
of six reports, and the two halves of BL-4 were reached by one report each. The
three findings every report converged on (BL-1, BL-2) are the two cheapest to
repair. Convergence tracked report *volume*, not severity.

The refuted finding (A2) was reached by one report (A) and was the only one
whose blocking grade rested on an outcome no auditor was permitted to measure;
it was refuted by the measurement, not by the five exchanges that confirmed its
structural half.

---

# 7. What this adjudication did not do

No build, no install, no commit, no write to either change directory or to any
other checkout. I did not open anything under `/Applications`, so the Launch
Control XL disassembly's *content* is unverified by me — SF-2 and SF-3 are
about the record of the reading, not about whether the file contains what is
quoted, and the map itself is the operator's ruling 1. I did not re-derive the
non-blocking findings marked "not adjudicated"; each carries its source so the
author can reach the original claim and its evidence. I read the six reports
and six exchanges, `measure-a2-report.md`, the two changes' artifacts, and the
code and git state named above, and nothing else in the scratchpad.
