# Tasks — Density's documents, and the spec delta close-out

**Work not listed in this file does not become a task unless the operator adds
it.**

Ticked tasks are delivered and committed. Each carries its OUTCOME in one or two
sentences; how it was arrived at belongs to the session that did it. Do not
re-do them.

## How this change is sequenced, and why

**Every STAGE ends in one commit and a push, after that stage's postflight.**
Not every task. The superseded change put six commits on `main` mid-stage with
no independent pass, and ran postflight per task, which audits a tree the next
task immediately changes and puts two audits on a collision course over the same
files.

**Every commit stages the paths it names and nothing else.** `git add -A`,
`git add .` and `git commit -a` are forbidden here:
`openspec/changes/frogg3rs-midi-controller-resilience/` is untracked in this
tree, so any blanket stage sweeps a change the operator is deliberately holding
onto `main`, and there is no pull request in this repository to catch it. Check
`git status --short` before every commit and confirm that directory is still
untracked afterwards. No AI attribution appears in any commit message.

**Coordinator-authored artifact text is not privileged over executor output.**
It carries the worst defect rate in this change's history. Either dispatch it
like any other work, or do not count it done until a context that did not write
it has read it.

## How every figure in this change is produced

- Measure through the PRODUCTION ROUTER, naming every knob moved off its
  registered default and why. `RouteFilterBank` and `RouteDriveBank` are private
  members of `FroggersAppCore` in `app/FroggersAppCore.hpp`, so a test reaches
  production through the app's public entry or through a replica mirroring the
  router's setter order. A replica states how it proved its own equivalence.
- **THE REVERB AND DELAY PAGES' REGISTERED DEFAULTS ARE A DEAD INSTRUMENT, so
  "measure at the registered defaults" is not available on either page and no
  task may ask for it.** Both pages' Wet/dry, Send and Stereo width rows omit
  their third field in `app/FroggersParameters.hpp` and take
  `FroggersParamSpec`'s own `0.0f`. Send at 0 starves the wet path and the probe
  reads `nan`. Stereo width at 0 makes `wetL` and `wetR` bit-equal, because
  `dsp::Reverb::Process` computes `wetL = mid + width * (aOut - mid)` and the
  same for `wetR`, so correlation reads a flat +1 whatever the tank does. A flat
  +1 row or a `nan` row is VOID rather than negative.
- **A THRESHOLD MEASURED AGAINST A SUPERSEDED MECHANISM IS NOT A THRESHOLD.**
  The superseded change wrote bounds taken from the tank before slot 7 became a
  diffuser and before Damping split, and they failed against the delivered tank
  by factors of twenty-three and sixty-seven. Re-measure against what ships.
- A figure lands in a TEST, never in prose.
- State the GRID and the TAP POINT beside any figure, precisely enough that
  someone else can re-derive it: what was tapped, where in the signal path,
  under which settings. The inherited 0.044 is the worked example of what
  happens otherwise — two contexts agreed on it because the second rebuilt the
  first's method, and it does not reproduce.
- Carry the liveness control inside the fixture and never let it be the whole of
  a case.
- Every task producing a check states what the check must ASSERT. A task that
  does not is defective — report it rather than choosing a threshold.

## Stage 1 — the check the superseded change measured and could not land

- [ ] 1.1 Pin Density's travel against Stereo width's on L/R correlation of the
      wet leg, in ONE case over ONE grid, against the DELIVERED tank.
      WHY THIS IS NOT ALREADY DONE: the superseded change measured the bound
      against the pre-change tank and an executor then found it dead — the width
      row moves 1.035547 rather than 0.044, and the Density row deviates
      0.005256, 0.008960, 0.002572 and 0.006261, every point above the 0.002
      bound that change recorded. It reverted rather than retuning, which was
      correct.
      MEASURE FIRST, then write the bound into this task before dispatching the
      implementing work. Grid: Damping and Density each over
      {0.0, 0.25, 0.5, 0.75, 1.0}, Stereo width held off its `0.0f` default and
      the value named, because at 0 the wet pair is bit-equal and the row is
      VOID.
      THE ASSERTION'S SHAPE, which does not depend on the numbers: the Stereo
      width row moves correlation by much more than the Density row does, with
      the width row carrying the liveness proof for the near-flat one. Density
      now moves correlation A LITTLE rather than not at all, because an allpass
      on the input genuinely decorrelates where a cross-feed weight did not, so
      the near-zero wording the superseded change used is wrong for the
      delivered mechanism.
      DO NOT ASSERT DENSITY'S DIRECTION unless the measurement clears its own
      noise floor by a stated margin, and report that floor.
      Reuse the correlation helper the existing cases establish rather than
      writing a second one.
- [ ] 1.2 Stage gate: full suite green, one postflight in a fresh context over
      the whole stage, then one commit and a push.

## Stage 2 — the documents

House style: plain present tense saying what a control does; no jokes; no
defining by negation; no history the reader never saw; and no task numbers,
change names, planning-document references or rule citations in comments, test
names or strings.

- [ ] 2.1 `MANUAL.md` and `QUICK_DICT.md` for the two Filter controls and both
      pages' Diffusion, Density and Stereo width. The Drive controls are already
      updated and are not re-done.
- [ ] 2.2 State on BOTH the Delay and Reverb pages what Diffusion and Density
      each mean, so the difference is on the page rather than in the reader's
      memory. Delay's Diffusion drives an allpass cascade on its wet tap;
      Reverb's Density drives one on the tank's input.
- [ ] 2.3 `MANUAL.md` states plainly that raising Density trades smoothness for
      coloration, and at roughly which part of the travel the coloration becomes
      audible, backed by the echo-density figures already in the tree: 0.0514,
      0.2342, 0.3403, 0.4132, 0.4782 across the travel.
- [ ] 2.4 State what Damping now does to the stereo image. Each tank line has
      its own filter, so damping the tail no longer closes the image; say so
      where Damping is described, because the superseded behaviour is what a
      returning reader remembers.
- [ ] 2.5 Stage gate: full suite green, one postflight in a fresh context over
      the whole stage, then one commit and a push.

## Stage 3 — the spec delta and archival

- [ ] 3.1 Move each scenario's `Check:` line off its not-yet-delivered marker
      onto the test that now backs it, and resolve that test's name by grepping
      for it — when the line is written and again before delivery. The Reverb
      bank scenario, the stereo-image scenario and the colouration scenario all
      carry markers that the delivered work has now satisfied or changed.
- [ ] 3.2 Re-count EVERY MODIFIED requirement this delta carries against the
      promoted text — three of them. The restate gate checks clause-level drift
      inside any scenario the delta restates and requires a `keeps:` line under
      every declared edit, so the remaining manual duty is the scenario COUNT,
      which the gate deliberately does not cover.
      THE GATE DOES NOT COMPARE PROSE AT ALL. Its bullet collector keeps only
      dash-opening lines, so a requirement's paragraphs can change or vanish
      with nothing noticing. Read the three requirements' prose by hand against
      the promoted text.
- [ ] 3.3 Every scenario's `Check:` names a test that exists and passes, or is
      marked not yet delivered in the form the gate recognises. Nothing parses
      prose, which is why this is the cheapest claim in the document to make
      falsely.
- [ ] 3.4 Confirm the two transitional NOTEs in the promoted
      `froggers-sheaf-parameter-model` spec name this change rather than the
      superseded one, and that this delta's declared edits quote them verbatim.
      They were repointed once already when the previous change was superseded,
      and they must move together or the restate gate goes red.
- [ ] 3.5 Archive this change, promoting its delta.

## Audits

- [ ] A.1 Preflight, in a context that did not write this change, covering the
      ARTIFACTS AND THE CODE TOGETHER — the artifacts are claims about the code,
      so neither can be checked without the other. It may reject.
- [ ] A.2 Each STAGE gets one postflight in a fresh context before its commit,
      comparing implementation against this proposal and reporting divergence
      strictly. Not one per task.
