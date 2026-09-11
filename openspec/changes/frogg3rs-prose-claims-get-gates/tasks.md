# Tasks — `frogg3rs-prose-claims-get-gates`

## 0. Baseline — runs BEFORE preflight

A baseline is a measurement of the tree as it stands, not execution of this
change, so it is the one task that precedes the preflight in task 9.1. Recorded
because the preflight audit flagged the ordering: `tasks.md` was edited while
that audit was reading it, which moved an artifact under a running auditor. The
measurement itself changed no file in this change's scope; the edit mid-audit is
the defect, and it is the same "a result is stale once its inputs move" rule
that governs any gate.

- [x] 0.1 Recorded in `baseline.md`: **371 PASS / 0 FAIL**, twelve binaries,
      every check script OK, tree at `81ee8cf`. Re-measured rather than carried
      forward from this morning's run, since the claim that a number is still
      current is the exact kind this change exists to stop trusting.

## 1. Repo hygiene (§8.0), over `app/` and `openspec/`

- [x] 1.1 Swept `app/` and `openspec/`. The sweep's own findings became tasks 6.0-6.2; two promoted `Check:` lines in `openspec/specs/froggers-sheaf-runtime-app/spec.md` that named nothing resolvable were repaired to cite `browser/tests/visual-criteria.spec.ts`, where the criterion they describe actually lives.
- [x] 1.2 Re-swept against this change's own final diff (21 files). All three checks green over the post-sweep tree, no probe residue from task 5.1's deliberate breaks, and the three new scripts carry a `python3` shebang. The re-sweep is the point: an opening sweep cannot see debt the change itself introduces, and a 150-site comment rewrite is exactly the edit that would introduce it.

## 2. `check_spec_checks_resolve.py`

- [x] 2.1 Failing case first: the script reported the `stage-independence case` class of miss immediately, and found 29 unresolvable references on its first run over the live tree.
- [x] 2.2 Scoped to `openspec/specs/**/spec.md` and non-archived `openspec/changes/**/specs/**/spec.md`.
- [x] 2.3 `none` / `operator step` accepted and counted separately; one line in the tree relies on it.
- [x] 2.4 Reports resolved count and declared-manual count. Now OK at 47 resolved, 1 declared manual.

## 3. `check_no_planning_history.py`

- [x] 3.1 Failing case first: 53 references, matching the corrected enumeration exactly.
- [x] 3.2 Patterns are operands: `\\btasks?\\b`, `\\bitem N`, `\\bgroup N`, `\\bpacket N`, `§N`, case-insensitive.
- [x] 3.3 `D[0-9]+` deliberately absent, with the reason recorded in the script's own docstring so it is not added back.
- [x] 3.4 One allow-list entry, `TEMP-BREAK`, reason recorded.

## 4. `check_citations_resolve.py`

- [x] 4.1 Failing case first: 22 unresolvable, 75 line-numbered -- reproducing the corrected enumeration exactly, independently of the grep that produced it.
- [x] 4.2 Pattern admits digits, dots and hyphens in path segments and recognises `sha:`, `sha^:` and `sha~N:` pins.
- [x] 4.3 Fails only on unpinned citations resolving under none of `app/`, `External/Sheaf/`, `src/`; exclusions and reasons in the docstring.

## 5. Prove each check goes red

- [x] 5.1 All four failure modes proven live against the GREEN tree, not merely against the population the checks were written for: a fresh `Task Q` comment, a citation to a file that exists nowhere, a line-numbered citation into this tree, and a spec `Check:` naming a test that was never written. Each was caught by the right check with the right count -- 1 planning-history, 1 unresolvable, 1 line-numbered, 1 unresolvable Check -- and all three checks returned green after restore, with the probe files verified clean.
- [x] 5.2 Invocation traced against all five existing checks and diffed: same single absolute `$(APP_DIR)` argument, same `name: OK -` / `name: FAIL -` convention, same nonzero-exit-stops-`test` behaviour. The two scripts needing repo-root access derive it as that argument's parent rather than adding a second convention. Verified live: the OK path exits 0 and the FAIL path stops `make` with Error 1.

## 6. The sweep

- [x] 6.0 53 planning-history lines reworded to name the behaviour. FOUND 53, CHANGED 53. Nothing was deleted outright: every flagged line carried real technical content beside the label, and deleting the comment to satisfy the check would have destroyed the rationale worth keeping.
- [x] 6.1 22 unresolvable citations. FOUND 22, CHANGED 22. Twenty-one got real commit pins, each line range verified against the pinned commit's own content before the pin was written -- `b9a8199^` for the retired desktop tree, `f236915^` for the retired simulator. One, a reference to JUCE's `juce_Timer.cpp`, has no findable commit because JUCE is not vendored in this repo; it was reworded to drop the false path rather than given a pin that would not resolve either.
- [x] 6.2 75 `app/`-internal citations converted to symbol form. FOUND 75, CHANGED 75, each after opening the cited file at the cited line. **Roughly a third were already stale** -- the prose named the right symbol while the line number pointed elsewhere in the same file, sometimes at a wholly unrelated function. That is the rot this task exists to stop, measured rather than asserted.
- [x] 6.3 The 214 `External/Sheaf/`, 65 `src/` and 72 pre-existing pinned citations were not touched. `check_citations_resolve.py` now reports 95 commit-pinned and 279 into pinned or frozen trees.

## 7. Wire in

- [x] 7.1 All three added to `app/Makefile`: own targets with comments, `.PHONY`, and prerequisites of `test` beside the existing five.
- [x] 7.2 **371 PASS / 0 FAIL, exactly the baseline**, re-run after every repair in groups 10 and 11: `make -C app test -j2` exit 0 with binaries deleted first, and every binary re-run by path. All EIGHT check scripts OK. The count not moving is the result -- this change compiled no new code into any binary, so a higher count would have been as much a defect as a lower one.

## 8. The spec delta

- [x] 8.1 `field-operator-doc-parity` carries the requirement, with four scenarios. Each names a check that exists -- and `check_spec_checks_resolve.py` now enforces that on this very delta, which is why its own two scenarios failed the check until the third script was written.
- [x] 8.2 Automated. The scenario now reads that the gate fails on a surviving `app/`-internal citation carrying a trailing line number, checked by `check_citations_resolve.py`, rather than promising a manual review this repository has twice failed to perform.

## 10. Postflight findings, fixed inside the change

The first postflight pass found three evasions and one duplication. All four are
repaired here rather than filed.

- [x] 10.1 **`check_spec_checks_resolve.py` passed the bug it was written to
      catch.** It accepted a `Check:` line when ANY backticked token resolved,
      so a real file beside a fictional case name passed -- the exact shape of
      the requirement that shipped naming a test existing nowhere. The lenient
      rule had been chosen hours earlier to stop false positives on incidental
      identifiers, and it reopened the hole. Fixed by SHAPE: a token carrying a
      separator or an underscore is evidence and must resolve; a bare CamelCase
      word is part of the sentence. Both evasions verified caught, and a control
      confirms a line naming a real file, a real case and two incidental
      identifiers still passes.
- [x] 10.2 **Naming a test file is not evidence.** A `Check:` naming an `app/`
      C++ test file must also name a resolvable `TEST_CASE`: these files run to
      thousands of lines, so "the stage-independence case" in prose beside one
      cannot be resolved by anyone. Eight lines failed; all eight fixed, each
      case read against the scenario it is cited for before being named. The 17
      `Check:` lines naming Sheaf and browser suites are deliberately left under
      the weaker rule, with the reason in the script: tightening those is its
      own change.
- [x] 10.3 **Three citations were invisible to the citation check** because
      `.h` and `.mm` were missing from its extension list -- the JUCE
      `juce_Timer.cpp` citation was caught only because `.cpp` happened to be
      listed. Extensions added; the three dead JUCE citations found and fixed.
- [x] 10.4 **The three scripts each reimplemented the same tree walk**, and the
      copies had ALREADY drifted before shipping: one omitted `.git` from its
      exclusions, so a matching filename inside git's object store would have
      read as resolving. Extracted to `app/check_common.py`. This is the
      change's own subject matter appearing in the change itself -- one rule
      enforced by three enumerations, each protecting only its own pass.
- [x] 10.5 **One pattern dropped after the fix, not before.** `design doc` had
      exactly one match in the tree, and that match is legitimate: prose
      observing that no document constrains a choice. A subagent satisfied the
      check by rewording it to "design write-up", which is a synonym dodge
      rather than a repair. The pattern is gone and the original wording
      restored. Every surviving pattern now rests on a counted population:
      kept `proposal` (2 true) and uppercase `STEP N` (3 true); rejected
      `design doc` (1, legitimate), lowercase `step N` (3, numbered lists in
      the same comment), `part N` (6, test structure), `phase N` (3, signal
      phase) and `D[0-9]+` (9, Envelope decay labels). 5 kept against 22
      avoided, and the rejections are recorded in the script so they are not
      added back.

## 11. Second postflight: the repair did not close the hole

Group 10's R1 fix was audited by a second fresh-context pass, which built its
own evasions instead of re-reading the two already fixed. Verdict: NOT SAFE TO
COMMIT. Five of eight constructed evasions still passed.

- [x] 11.1 **The compound token was the original bug, unpatched.**
      `resolves()` split `File.cpp: case_name` on the colon and returned true on
      the FIRST part that resolved, never looking at the rest -- so a real file
      beside a fictional case passed. That form is the dominant `Check:` style
      across the live spec tree, so the script was green while the exact defect
      it exists to catch was expressible in the format most specs already use.
      Fixed: every named part of a token must resolve.
- [x] 11.2 **`none` / `operator step` short-circuited before reading tokens**,
      so `Check: none, see \`a_fake_case\`` passed with a false claim attached.
      The declaration is still accepted; anything it names is now resolved too.
- [x] 11.3 **A line whose only tokens were sentence-shaped passed on nothing.**
      A fictional CamelCase name, or a punctuation-only token, satisfied the
      check by being exempt. A line must now carry at least one claim.
- [x] 11.4 **A real case name beside the WRONG file passed**, because the file
      and the case were each checked on their own. Cases are now indexed per
      file, and a `Check:` naming an `app/` test file must name a case defined
      IN that file.
- [x] 11.5 `Makefile` exposed the shape rule's edge: a real file with no slash,
      dot or underscore read as prose. A token is now a claim if its shape says
      so OR if it names a real file, and that predicate is shared by the call
      site and the resolver rather than written twice -- which is how the two
      had already disagreed.
- [x] 11.6 `spec_files()` still carried a private `os.walk`. Migrated. The only
      `os.walk` calls left in any check script are the two inside
      `check_common.py`.
- [x] 11.7 Twelve-case battery run against the real script and the real tree:
      all nine evasions caught -- the original prose bug, a fictional backticked
      name, the compound form, `none` and `operator step` with false claims
      attached, a bare CamelCase token, a punctuation-only token, a real case
      beside the wrong file, and a fictional path sharing a real basename -- and
      all three controls still allowed, including the compound form when the
      case genuinely lives in the file named beside it.

Everything else the pass checked came back clean: the eight re-cited `Check:`
lines each name a case that genuinely asserts its scenario, the JUCE rewordings
keep their meaning without gaming the regex, the shared walker narrows no
coverage, `FroggersPluginEditor.cpp` is byte-identical to HEAD, and the twenty
modified sources carry no executable change.

## 9. Delivery

- [x] 9.1 Preflight run in a fresh context: ACCEPT WITH CHANGES. It reproduced
      the citation enumeration exactly (448/214/75/72/65/22) and rejected the
      planning-history half -- both the count and the patterns. Its four
      substantive findings are folded into tasks 2.2, 3.1-3.4, 4.2 and 8.1, and
      its invocation trace into the proposal. Per §9 this revision is itself
      unaudited; postflight reads the revision, not the draft.
- [x] 9.2 First postflight pass run in a fresh context. Sections 1, 3, 4 and 6 clean -- every `[x]` task verified against the tree, zero excluded-bucket citations touched, seven commit pins spot-checked against `git show` and all seven accurate, no hygiene residue. Sections 2 and 5 produced the four findings recorded as group 10, all repaired inside the change. A second, scoped pass covers the repairs, since a postflight whose findings are acted on has produced an unaudited change.
- [x] 9.3 Pushed to `main` as `ab90924`. No Sheaf file is touched, so the submodule stays pinned at `ba3898e4` and there is no PR to raise upstream. `frogg3rs-midi-controller-resilience` is left open and untouched.

