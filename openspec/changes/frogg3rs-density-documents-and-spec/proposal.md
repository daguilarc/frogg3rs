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

Each item passed a fresh-context postflight that re-derived its numbers rather
than reading an executor's.

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
  one shared filter held L/R correlation at or above 0.9887 wherever Stereo
  width sat, so the control named for the stereo field could not reach the
  image while the control named for tone closed it.
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

- Delay Stereo width WIDENS, correlation 1.0000 to 0.0406, and that is ENTIRELY
  its L/R time offset: the cross-fed term is gated to an exact zero at
  Feedback's registered default, traced in code and confirmed by isolation.
- Reverb Stereo width WIDENS.
- Damping, shared filter, 0.9887 rising to 0.9995; per-line filters 0.6042
  rising to 0.6838, over one grid in one run.
- Echo density across Density: 0.0514, 0.2342, 0.3403, 0.4132, 0.4782.
- **THE REVERB AND DELAY PAGES' REGISTERED DEFAULTS ARE A DEAD INSTRUMENT.**
  Both pages' Wet/dry, Send and Stereo width rows omit their third field in
  `app/FroggersParameters.hpp` and so take `FroggersParamSpec`'s own `0.0f`.
  Send at 0 starves the wet path and a correlation probe reads `nan`. Stereo
  width at 0 makes `wetL` and `wetR` bit-equal, so correlation reads a flat +1
  whatever the tank does. **A flat +1 row or a `nan` row is VOID, not
  negative.** Every measurement on these pages states its own operating point,
  knob by knob.

## The figure that does not reproduce, stated rather than buried

The superseded change recorded Reverb's Stereo width as moving L/R correlation
0.044, and slot 7 as moving it 0.000134. Those were measured against the tank
BEFORE slot 7 became a diffuser and before Damping split.

Against the delivered tank the width row moves 1.035547 and the Density row
deviates 0.005256, 0.008960, 0.002572 and 0.006261 across the same grid. The
tree says so at the case that carries it: the inherited 0.044 does not
reproduce, from a different and unrecorded rig.

Two contexts agreed on 0.044 because the second rebuilt the first's METHOD, not
because the figure was robust. The quantity was never recorded precisely enough
to re-derive — which is §6.1's own warning that a measurement's label is a
separate claim from its value.

**The claim survives and the bound does not.** Stereo width still moves
correlation about a hundred and fifteen times more than Density. Density now
moves it a little rather than not at all, because an allpass on the input
genuinely decorrelates where a cross-feed weight did not. Task 1.1 measures the
delivered tank and writes the bound from that.

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
  check only. No DSP header changes.
- **Affected gates:** `app/check_docs_match_parameter_table.py` binds every
  manual and quickdict bold entry to the parameter table by name and slot.
- **Affected documents:** `MANUAL.md`, `QUICK_DICT.md`.
- **Out of scope, stated so it is not rediscovered:**
  `src/core/FroggersEngine.hpp` is frozen firmware and `app/Makefile` forbids
  including from it. `openspec/changes/frogg3rs-midi-controller-resilience/` is
  untracked and held deliberately; no blanket stage may sweep it in.
- **Delivery is a push to `main`.** This repository does not use pull requests.

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
