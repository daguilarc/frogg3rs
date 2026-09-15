# Handoff: the MIDI resilience pair, superseded

Worktree `.claude/worktrees/midi-resilience`, branch `worktree-midi-resilience`.
Two changes: `openspec/changes/frogg3rs-midi-preset-preconditions/` here and
`openspec/changes/midi-controller-resilience/` in the `External/Sheaf`
submodule (branch `midi-resilience-merge`). Neither has executed.

## 1. Read this before anything else

This session ran twelve preflights on the pair and never reached execution.
From the eighth round on, nearly every blocking finding was against the plan's
own bookkeeping: a rule for rewriting `Check:` lines, grep probes inside a
cross-repository gate, a citation backstop, count literals baselined into
tasks. Each repair of those descriptions was new text for the next adversarial
pair to attack. Blocking counts went 21, 16, 7, 14, 8, 12, 5, 11. The operator
stopped it under the omni rule's 6.2 (two same-shape failures say everything a
third will) and ordered this supersede.

**The operator binds the next session to the following. These are not
suggestions.**

1. Do not preflight this pair as it stands. Supersede it first (section 3).
2. A preflight rejects only for: a missing or partial trace; a behavioural
   premise nobody measured; a task that cannot be run as written without
   inventing a value; a false claim about the code. A finding that a
   coordinator PROCEDURE could be gamed (how `Check:` lines are rewritten, how
   a submodule pin is confirmed, how citations are re-checked, how boxes are
   ticked) is recorded and is NOT blocking. Those procedures belong to the
   coordinator and are verified by the repository's own gates and by the
   postflight's adversarial pass against shipped code.
3. Two same-shape failures stop the loop. If a second preflight rejects on the
   same class of defect as the first, stop, write the contradicted belief into
   this file, and hand off. Never dispatch a third repair of the same shape.
4. The successor's task list carries no self-referential gates: no task whose
   assertion is about another task's text, no grep-count invariant over
   artifacts, no probe over source for a symbol name, no guard on a guard. A
   check is either an assertion in a test binary that a real break turns red,
   or a script over the tree with one stated positive control. Anything else
   is deleted, not repaired.
5. The audit shape itself stays: attackers in one batch with identical briefs,
   no prior findings shown, an exchange, a non-participant adjudicator who
   reproduces each claim. What changes is what counts as blocking (item 2).

## 2. State of the tree at this commit

```
$ git log --oneline -1                       # this branch
$ git log --oneline HEAD..origin/main | wc -l  # 0 at the time of writing; main moves, rebase before merging
$ git -C External/Sheaf log --oneline -1     # the Sheaf commit this branch pins
$ git -C External/Sheaf branch -r --contains HEAD   # prints nothing: that commit exists only in this
                                             # worktree's submodule object store; do not remove this worktree
$ git -C External/Sheaf remote -v            # origin = jvictor0/Sheaf, fork = daguilarc/Sheaf (fetched)
$ git status --short; git -C External/Sheaf status --short   # both empty
$ ls External/Sheaf/projects/synth/browser/node_modules      # absent; npm ci there is operator-approved
```
All artifact gates (`app/check_*.py` and `.sh` under `app/Makefile`'s test
target, `openspec validate --strict` in both repositories, Sheaf's
`make openspec-check`) were green at this commit. No test binary has been built
on this branch except once, in a throwaway worktree, for the measurement
recorded in the frogg3rs `design.md` (an analog-only `Generic` default passes
the Controllers page tests; only the two `deviceDefaults.size() == 6`
assertions fail).

## 3. What the successor keeps and what it drops

Keep, because it is the product and was traced:
- Sheaf: `declaredPreconditions` on `MidiAppDeviceDefault`, threaded through
  `ControllerWizardDescriptor` and rendered as sibling `Label` nodes in the
  Controllers page add row; the held-modifier lifetime with its five clear
  triggers (`HeldModifierClearSource`), atomic state, the `held` precondition
  on the ceiling, the `Engine` accessors the ceiling checks need; the mismatch
  report with its mapped set defined as the union of every matcher in the input
  chain; the template-change recognizer at the head of the chain with the rate
  limiter; the browser endpoint-open seam (ABI entry point, worker dispatch,
  `midi.ts` call) with its check re-homed where it can drive and observe.
- frogg3rs: declared preconditions on the six defaults; the seventh default,
  Novation Launch Control XL, `Generic` kind, scene blend on fader 1 at MIDI
  channel 8 zero-indexed, CC 77, factory template 1 declared as a precondition,
  the map's source being the disassembly of Ableton Live 12's control-surface
  script recorded verbatim in `design.md`; the compiled emitter that prints each
  default's declarations, the generated manual settings section compared byte
  for byte inside delimited regions; the recovery-text rewrite naming the five
  triggers per host; the count-assertion updates the seventh default forces.

Drop from the task lists, without replacement:
- frogg3rs task 6.6 (the `Check:`-line rewrite rule and everything that
  referred to it), task 4.2's citation backstop, task 4.1's grep probes (keep
  only: submodule checkout equals the Sheaf completion commit; that tree is
  clean), the count literals in tasks 1.2 and 1.6, and every `<M>`/`<N>`
  arithmetic.
- Sheaf task 7.2's gaming guard, the marker-completeness guard in task 1.9,
  and any whitespace or comment-shape guard on a scan.
- The `preflight-*.md` files in both change directories are records of
  superseded drafts; the successor may delete them.

Still open at the product level (the only findings that matter), from the
twelfth ruling, recorded as `preflight-12.md` beside the frogg3rs change and
`preflight-9.md` beside the Sheaf change:
- The emitter's output format and the manual's generated heading are not
  written literally anywhere.
- The frogg3rs `design.md` sentence claiming a button whose addresses moved
  still dispatches is false at `FindAssociation` returning null; the
  requirement's scope is right, the design sentence is not.
The Sheaf-side findings of that ruling (Impact and sweep missing
`projects/synth/Makefile` and the new gate script; a site count with no
predicate; the endpoint-open cases asserting cleared without holding first)
were repaired in the Sheaf commit this branch pins.

## 4. Operator rulings that bind the successor

- Scope stays the full pair; do not split the engine half from the app half.
- Delivery for a cycle is one push of this branch to `origin`, then stop. No
  push of the Sheaf branch, no pull request, no pin moved on `main`, no merge.
  The operator performs the rebase and merge and, at that step, the Sheaf push
  and the next sequential pull request against upstream `main`.
- Rebase onto `main` immediately before any merge; `main` is a live tree
  another session pushes to.
- After a postflight passes: documentation hygiene (manual, quick dictionary,
  readme), then `openspec archive` of the frogg3rs change (its archive
  directory is gitignored, so the change directory leaves the tracked tree),
  then commit, then push, then the session deletes its own scratchpad under
  `/private/tmp/claude-501`. The Sheaf change is archived at the operator's
  upstream step, because its amended requirements live in the deltas of two
  open pull requests.
- No scheduled pruning of scratch directories; each session purges its own.
- Every commit and rebase is made by the coordinator, never by an executor.
- Subagents run on Sonnet or Haiku; Opus only when the operator says so for a
  named round. Auditors may run on the author's tier.
- The Launch Control XL map is confirmed on hardware only after the merge, on
  the live browser site; no device is on the execution path.

## 5. Where things are

- This file; the two change directories; the disassembly and the measurement
  inside the frogg3rs `design.md`.
- The session's scratchpad, disposable, holds every audit report and repair
  report: `/private/tmp/claude-501/-Users-diegoaguilar-canabal-Desktop/29e77bbd-b247-4b6b-9069-7be98faccf35/scratchpad/`
  with a `ledger.md` of every dispatch. Nothing in it is needed to continue.

## 6. The first hour

Write the successor artifacts by deleting what section 3 drops and fixing the
two open items, in the same two change directories. Run the artifact gates.
Then one preflight under section 1, item 2. Then execute, with a postflight
per step and the adversarial pair pointed at the shipped checks. The engine
half executes first; the app half's group that needs the field begins only at
the Sheaf completion commit.
