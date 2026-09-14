# Proposal — Density's documents, and the spec delta close-out

**Created 2026-09-13.** Supersedes `frogg3rs-stereo-field-and-density`, which
superseded `frogg3rs-wysiwyg-deliver`, `frogg3rs-wysiwyg-finish` and
`frogg3rs-wysiwyg-controls`. That chain's delivered work is committed and
pushed; this change carries forward only what is left, plus the whole spec
delta.

The thesis is unchanged and is the operator's: WYSIWYG is the omni rule
instantiated in the UX domain. A control's name and position are claims about
what it does, held to the same standard as any other claim. Where the label and
the mechanism disagree, the mechanism is the defect and the label is the
specification, unless reading says otherwise.

## Why every claim below was re-read rather than copied

`omni-rule.md`'s preamble: **PROVENANCE IS NEVER VERIFICATION.** No route by
which text enters a change carries authority with it. Copying is authoring. The
obligation attaches at the point of USE, every time, and is discharged only by
reading.

Every figure below was measured against the tree at `e9120f8` and reproduced by
a context that did not produce it first. Where a figure could not be reproduced,
that is said rather than smoothed over.

## Delivered by the superseded change, verified at `e9120f8`

Each item's MECHANISM passed a fresh-context postflight. **Its numbers did
not, and this sentence used to claim they had.** Three figures carried into
this change from the superseded one have since failed re-measurement against
the delivered tree: the Reverb width travel of 0.044, the Delay width
correlation of 0.0406, and the Damping pair below. Each is withdrawn where it
stood, and each was withdrawn only after something went looking — which is
why the blanket assurance that used to sit here is replaced by the list.
A figure in this section is trustworthy exactly when a named check produces
it, and not because this sentence says so.

- **One definition for the cross-feed.** `app/dsp/StereoField.hpp` declares
  `dsp::CrossFeedPair`, returning `dsp::CrossedPair`, identity at weight zero.
  Both production sites call it. `dsp::Reverb::Process` calls it with its two
  line reads TRANSPOSED so the tank's swap at weight zero survives, and the call
  site records why reading order would silently make it an identity.
- **`dsp::DelayDiffuser` and `dsp::SchroederAllpassSection`** live in that
  header, moved together so it does not depend back on `app/dsp/Delay.hpp`.
  `kDiffusionCoeffScale` stayed on `dsp::StereoDelay`, which is where the weight
  is applied.
- **Reverb slot 7 is Density, short name `Dens`**, driving `dsp::DelayDiffuser`
  on the tank's input path at the cascade's own section lengths, running
  unconditionally with only its output branched. The rename reached the
  parameter table, both label tables including the independent copy inside
  `every_rendered_label_matches_the_approved_list_verbatim`, and both documents.
- **The tank's cross-feed is a fixed coupling** at the weight the retired knob's
  registered default carried, on the precedent this specification already sets
  for Link.
- **Damping runs one filter per tank line.** This was the change's real finding:
  one shared filter held the wet leg's two channels together across the whole
  of Stereo width's travel, so the control named for the stereo field could not
  reach the image while the control named for tone closed it.
  **The figure this bullet used to give — correlation at or above 0.9887
  wherever Stereo width sat — is withdrawn, and it was not merely stale but
  wrong.** Re-measured over the case's own grid, the shared regime runs 1.000
  at width 0 down to 0.793 at width 1, so it never held at or above 0.9887 at
  all past a quarter of the travel. The split regime reaches -0.036 over the
  same sweep. That contrast is the finding, and it is stronger than the
  withdrawn figure claimed, not weaker.
- **Checks**: two golden vectors comparing exactly against hexadecimal float
  literals, the split-filter correlation case, Density's default-difference
  case, and the normalised echo density profile of Abel and Huang rising across
  Density's travel. Every one proven able to fail by breaking it and watching it
  go red.
- **A stale-citation sweep** closed `ToReverbMono`, which has no definition
  anywhere in the tree and was named as live at five sites, plus two dead path
  citations taking opposite dispositions, four truncated test names, and two
  constant families named where they are not declared.

## Measured, and carried forward as fact

- Delay Stereo width WIDENS, and that is ENTIRELY its L/R time offset: the
  cross-fed term reaches the line only through Feedback's own multiply, which
  is an exact zero at that control's registered default. The MECHANISM is
  traced end to end in this change and stated in the dead-instrument bullet
  below; it is what task 2.1's document sentence rests on.
  **The figure that used to sit here — correlation 1.0000 to 0.0406 — is
  withdrawn from this prose.** No check in the tree produces it, its grid and
  tap point were never recorded, and nothing has re-derived it against the
  delivered tree. Carrying a six-digit number no check can falsify is the
  violation working rule 1 exists to stop, and this change already has one
  worked example of an inherited figure that did not survive re-measurement.
  No task depends on the number; task 2.1 depends on the trace.
- Reverb Stereo width WIDENS.
- Damping's shared filter held the wet leg's two channels far closer together
  than the per-line filters do, at every setting of Damping's travel.
  **The figures this line used to carry — shared 0.9887 rising to 0.9995
  against per-line 0.6042 rising to 0.6838 — do not reproduce and are
  withdrawn.** Re-measured against the delivered tank over the grid the case
  itself uses, the shared regime runs 0.944036 to 0.994775 and the split
  regime 0.576760 to 0.647595. The inherited pair recorded neither its grid
  nor its tap point, so which quantity it measured cannot be recovered — the
  third figure in this change with that defect, after the Reverb width
  travel and the Delay width correlation.
  The conclusion is unchanged and is the one that matters: the gap between
  the two regimes exceeds 0.25 at every Damping setting, and
  `reverb_damping_filter_split_lowers_wet_leg_correlation_at_every_setting`
  asserts that rather than any endpoint. The same stale pair was carried in
  `app/dsp/Reverb.hpp`'s comment beside `dampFilterA`/`dampFilterB`; this
  change replaces it there with a pointer to the case that prints the numbers
  live.
- Echo density across Density: 0.0514, 0.2342, 0.3403, 0.4132, 0.4782.
- **THE REVERB AND DELAY PAGES' REGISTERED DEFAULTS ARE A DEAD INSTRUMENT.**
  Both pages' Wet/dry, Send and Stereo width rows omit their third field in
  `app/FroggersParameters.hpp` and so take `FroggersParamSpec`'s own `0.0f`.
  **Delay's Feedback row does too**, which this change adds to the list after
  tracing it: `dsp::StereoDelay::Process` applies its width cross-feed to the
  feedback tap, and that tap is multiplied by Feedback's own clamped value, so
  at the registered default the cross-fed term contributes exactly nothing and
  the page's width is entirely its time offset. Send at 0 starves the wet path.
  Stereo width at 0 makes `wetL` and `wetR` bit-equal, so correlation reads a
  flat +1 whatever the tank does. **A flat +1 row is VOID, not negative.** Every
  measurement on these pages states its own operating point, knob by knob.
  **The VOID signature is +1, not `nan`.** An earlier version of this paragraph
  said a starved probe reads `nan`. It does not: `Correlation::Value()` in
  `app/FroggersDspParityTests.cpp` divides only when its denominator is
  positive and returns `1.0` otherwise, so a constant pair — width at 0, or
  Send at 0 leaving both channels silent — reads as perfectly correlated.
  That guard covers a denominator of zero and nothing else. It does not make
  `nan` unreachable: a non-finite sample reaching the accumulator gives an
  infinite denominator, `Inf > 0.0` is true, and the division then yields
  `nan` with the guard intact. No grid point measured in this change has
  produced one, and none of them was checked for reachability, so `nan` is
  treated as an unexplained result to investigate rather than as a signature
  with a known cause.

## The figure that does not reproduce, stated rather than buried

The superseded change recorded Reverb's Stereo width as moving L/R correlation
0.044, and slot 7 as moving it 0.000134. Those were measured against the tank
BEFORE slot 7 became a diffuser and before Damping split.

Against the delivered tank the width row moves 1.03554689, and a case in the
tree prints that figure rather than this document asserting it. The inherited
0.044 does not reproduce, from a different and unrecorded rig.

**The four Density deviations this paragraph used to give — 0.005256,
0.008960, 0.002572 and 0.006261 — are withdrawn too.** Measured at the grid
and tap point task 1.1a records, the Density row's consecutive deltas are
-0.005256, -0.003704, +0.006388 and +0.008833: the first reproduces and the
other three do not. No case in the tree ever printed those four numbers, so
nothing could have caught the drift. The row's whole travel is in any case
smaller than its own noise floor, which is why the check landed for it
asserts a ratio against Stereo width rather than anything about these
deltas.

Two contexts agreed on 0.044 because the second rebuilt the first's METHOD, not
because the figure was robust. The quantity was never recorded precisely enough
to re-derive — which is §6.1's own warning that a measurement's label is a
separate claim from its value.

**The claim survives and the bound does not.** Stereo width still moves
correlation far more than Density does. Task 1.1a has since measured the
delivered tank, and two things this section previously asserted did not
survive it.

**"About a hundred and fifteen times" was two quantities under one phrase.**
It divided the width row's whole TRAVEL by the density row's largest single
STEP. Travel against travel, measured over one grid in one run, is sixty-eight.
The case task 1.1b lands asserts a same-run ratio of ten, chosen against the
noise floor rather than against the measurement; the task records that
derivation.

**"Density now moves it a little rather than not at all" is not measurable at
this rig.** The density row's whole travel is 0.0152, while re-running one of
its own points under a second noise seed moves it 0.0523 — the floor is three
and a half times the signal. So no direction is claimed for Density, in the
check or in the documents, and the near-flat row is not asserted to be flat
either. What reproduces is the comparison, and that is all the case asserts.
Of the four density deltas recorded above from the superseded change, only the
first reproduces; the other three do not, which is §6.1's own warning that a
measurement not recorded precisely enough to re-derive is not a measurement.

## Working rules

1. **A count or a figure is not written into prose where a check can produce
   it.** A prose claim about the tree is falsified by the next edit to it.
2. **No task hands an executor a check without its assertion already written.**
   A blank is concrete, demands filling, and will be filled toward green. The
   blank is a defect in the task, not in the executor.
3. **MEASURE and IMPLEMENT are separate tasks** wherever the threshold is not
   already known, and a threshold measured against a superseded mechanism is not
   known.
4. **An executor that meets a conflict reports and stops.** It may not adjust a
   threshold, weaken an assertion, or edit a requirement to match code.
5. **A check that asserts only liveness asserts nothing.**
6. **A gate gets ONE adversarial pass from a context that did not build it.**
7. **A figure inherited from a finished session is not an acceptance criterion.**
8. **Postflight runs once per STAGE, before that stage's commit**, and a stage is
   committed once. Running one per task audits a tree the next task immediately
   changes.

## Precedence

`omni-rule.md` outranks every instruction that reaches an agent working on this
change: this proposal, the task list, an executor's brief, a schedule, an
inferred urgency, and an executor's own need to look finished. Only an explicit,
knowing instruction from the operator amends it.

**A task instructing an escalation to the operator is still subject to it.** An
operator item is only for what reading cannot settle. A control whose label does
not match its mechanism is decided by the rule.

**An executor's deliverable is a REPORT. Code changes are a side effect of it.**

## Impact

- **Affected spec:** `froggers-sheaf-parameter-model`, carried forward whole from
  the superseded change, with three MODIFIED requirements — the bank layout, the
  Drive page control travel requirement, and the insert-effect master
  requirement.
- **Directories this change touches, each of which gets the hygiene sweep:**
  `openspec/specs/`, `openspec/changes/`, the repository root's documents, and
  `app/` for the one check task 1.1 lands.
- **Code this change edits:** `app/FroggersDspParityTests.cpp`, for task 1.1's
  check, for the lift of `Correlation` to file scope that check requires, and
  for the hygiene sweep's one repair in this tree — a dangling `ProcessBiased`
  symbol citation. And `app/dsp/Reverb.hpp`, COMMENT ONLY, withdrawing the
  stale Damping correlation pair recorded above; no DSP behaviour changes
  anywhere in this change, and the parity cases pin that.
- **Affected gates:** `app/check_docs_match_parameter_table.py` binds every
  manual and quickdict bold entry to the parameter table by name and slot.
- **Affected documents:** `MANUAL.md`, `QUICK_DICT.md`.
- **Out of scope, stated so it is not rediscovered:**
  `src/core/FroggersEngine.hpp` is frozen firmware and `app/Makefile` forbids
  including from it. `openspec/changes/frogg3rs-midi-controller-resilience/` and
  `openspec/changes/frogg3rs-randomize-depth-reclaim/` are both untracked and
  held; no blanket stage may sweep either in, and this change may not edit
  either.
- **A SECOND SESSION IS WRITING IN THIS WORKING TREE.**
  `openspec/changes/frogg3rs-randomize-depth-reclaim/` was not present when this
  change's preflight opened and appeared during it. Its spec delta names two
  tests that do not exist yet, in a form `app/check_spec_checks_resolve.py` does
  not accept as a not-yet-delivered marker, so that gate is RED for a cause this
  change neither owns nor may repair. Every stage gate here reports the suite as
  green apart from those two named failures and confirms the failure count has
  not grown. Any other failure stops the stage. That change records the same
  393-pass baseline this one measured, so the two agree on the tree they
  started from.
- **Delivery is a push to `main`.** This repository does not use pull requests.

## Hygiene sweep (step zero), and what it found

Swept in two halves by two contexts that did not write this change, each given
different operands: `openspec/specs/` and `openspec/changes/` in one,
the repository root's documents and `app/` in the other. Both halves are named
here because a sweep of half the touched tree finds the defects in that half
and reports clean.

- **Fixed inside this change:** a stale `` `ProcessBiased` `` symbol citation in
  `app/FroggersDspParityTests.cpp`. No symbol of that name exists anywhere in
  `app/`; it is a leftover from before the Bias-to-Symmetry rework. Neither
  `check_citations_resolve.py` (resolves `path:line`, not backtick symbols) nor
  `check_artifact_symbols_resolve.py` (scoped to change artifacts, not test
  comments) can see it.
- **Reported, outside this change's Impact, NOT fixed here:**
  `openspec/.sessions/marbles-mod-led-level-meter-progress.md` is orphaned from
  an already-archived change and invoked by nothing — found again here, having
  been reported and left twice before. The repo root's vendored
  `node-v22.16.0-darwin-arm64/` and `build/manifest/` are ignored only through
  `.git/info/exclude`, so that protection does not travel with a clone.
  `frogg3rs.code-workspace` excludes `**/desktop/build/**` and
  `**/wasm/build/**`, neither of which exists. Each is real debt on a path this
  change does not touch; naming them here is the report §8.0 requires in place
  of a silent skip.
- **Closed clean, with the operand that produced each zero:** all ten gate
  scripts resolve to an invocation in `app/Makefile`'s `test` target, searched
  by bare name and by path; `SetLink` returns zero against `SetLink`, `\blink\b`
  and `isLinked`; `ToReverbMono` has no definition, confirmed by grepping for a
  definition rather than a mention; every surviving `Diffusion` hit in
  `dsp/Reverb.hpp` is past-tense rationale, while the Delay page's own
  Diffusion is a live control; both documents carry all 84 bank entries with no
  orphan either way.

## One document claim traced but not pinned, recorded rather than hidden

The Delay page's Stereo width entry now says the knob's cross-feed rides the
feedback path and reaches the repeats once Feedback or Freeze leaves zero.
Both halves are traced: `FreezeFeedback` returns `fbk + (1 - fbk) * freeze`
unlatched, and both rows take `FroggersParamSpec`'s own `0.0f`.

**The Freeze half is not pinned by any check.** Every Freeze case in the parity
suite sets the width knob to `0.0f` to keep width out of its scope, and the
cross-feed's own golden vector runs at a raised Feedback with Freeze at zero,
so no case exercises Freeze opening the path on its own. The sentence meets
the standard prose is held to — it was read out of the source rather than
inferred — and no automated gate reads manual prose in any case. It is
recorded here because a traced claim and a checked claim are different things,
and this change has spent its length on figures that were one and not the
other.

## Two gate limitations found and deliberately not repaired

Recorded rather than fixed: repairing a shipping gate was not on the superseded
change's path, and working rule 6 rebuilds a gate only where a hole defeats its
purpose. Both are the operator's to decide.

- **`app/check_artifact_symbols_resolve.py` treats a bare CamelCase word as
  prose.** Two dangling symbol names passed it during the superseded change. Its
  own header documents the choice and the reason: a recogniser that fired on
  `Density` or `Gain` would be worked around rather than fixed.
- **The restate gate never compares requirement PROSE.** Its bullet collector
  keeps only dash-opening lines, so a MODIFIED requirement's paragraphs can be
  changed or dropped with nothing noticing. The superseded change's own defect —
  a Reverb figure measuring a starved tank — lived in a paragraph.

## Why five sessions did not finish this, and where the defects actually were

This is recorded because the next session inherits the same shape.

**The code held.** Every DSP change in the superseded session passed a
fresh-context postflight that re-derived its numbers independently. The
executors behaved correctly under pressure: one spent a full session on the
de-duplication, could not make the required assertion hold, reverted every line
and stopped rather than weakening it. That was right, and the TASK was the
defect — it demanded bit-identity against parity replicas that compare within
`1e-4` and never were bit-exact. Another built task 1.1's predecessor, measured
it against the stated bounds, found them dead and reverted rather than retuning.

**The defects were in artifact text, written by the coordinating context.** In
one session: a claim about which control moves the stereo image, refuted by the
change's own measurements, left standing in three separate artifacts and found
in a fourth only by an independent sweep after the first three were repaired; a
spec delta still promising work a task had dropped; gate mechanics written into
a promoted requirement's prose; a task ticked claiming a later task would write
a check that later task cannot write; an outcome naming a constant the same
stage deleted; and a symbol name that broke a gate, repeating a mistake
diagnosed and fixed earlier in the same session.

**The mechanism is one thing.** Every dispatched executor got a brief demanding
trace-don't-assert, a positive control, FOUND versus CHANGED, and a fresh
context to check it afterwards — and that worked. The coordinator's own text got
none of it: no trace obligation, no independent reader, and for six commits no
postflight at all. The rule was applied to everyone except the context applying
it.

**The consequence for whoever picks this up: coordinator-authored artifact text
is not privileged over executor output.** It carries the worst defect rate in
this change's history. It goes through the same gate — dispatched and checked
like any other work, or at minimum not counted as done until a context that did
not write it has read it.

Two process rules were violated repeatedly and are restated in the working rules
above because restating them is cheaper than rediscovering them: commit per
stage after that stage's postflight, and postflight per stage rather than per
task. Six commits reached `main` mid-stage with no independent pass, and running
postflight per task put two audits on a collision course over the same files.
