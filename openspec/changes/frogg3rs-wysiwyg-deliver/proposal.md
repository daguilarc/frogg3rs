# Proposal — `frogg3rs-wysiwyg-deliver`

**Created 2026-09-12.** Supersedes `frogg3rs-wysiwyg-finish`, which superseded
`frogg3rs-wysiwyg-controls`. Neither was committed, so nothing outside this
repository's own promoted spec ever pointed at them; those two pointers move
here in this change. The design work both did stands and is not reopened: their
findings, measurements and rulings are carried forward below, stated once.

The thesis, in the operator's words: WYSIWYG is the omni rule instantiated in
the UX domain. A control's name and position are claims about what it does, held
to the same standard as any other claim — traced, not asserted. Where the label
and the mechanism disagree, the mechanism is the defect and the label is the
specification, unless reading says otherwise.

## Why a third change rather than another repair

Its predecessor grew to 836 lines across two artifacts carrying roughly fifty
numeric claims — counts of citations, of files, of gates, of scenarios. Every
audit round audited those claims, found some stale, and repaired them, which
produced new claims. The predecessor's task list grew from 338 lines to 554
while six of its forty-two tasks completed, all six of them hygiene. No audio
code changed across four sessions.

The mechanism is not carelessness, it is arithmetic: **a prose claim about the
tree is falsified by the next edit to the tree, and an artifact dense with such
claims generates audit findings faster than the work behind it gets done.** So
this change is written to hold as few falsifiable claims as it can while still
saying what must be true.

Three rules follow, and they are the substantive difference from both
predecessors:

1. **A count is not written down where a check can produce it.** "Repair the
   split citations the gate reports" cannot go stale; "repair the six split
   citations" was wrong within a day, twice.
2. **No document states what another document's sections are numbered.** Group
   numbers rotted in both predecessors and in the promoted spec, where one NOTE
   pointed at a group that did not exist while its twin pointed correctly.
   References here name the SUBJECT.
3. **Completed work is recorded as an outcome, not as a narrative.** What a
   finished task did, in one or two sentences. How it was arrived at belongs to
   the session that did it.

## State of the tree

**Everything below described as delivered is committed and pushed.** The code
and document work is in `e565331`; this change's own artifacts are in `585f332`.
The working tree is clean apart from `frogg3rs-midi-controller-resilience`,
which is a separate change the operator is holding and which this change does
not touch. Where `main`'s tip sits is not restated here — `git log` produces it,
and a commit id written into prose is wrong at the next commit.

The `openspec archive` of `frogg3rs-effect-page-hierarchy` and
`frogg3rs-prose-claims-get-gates` was run and is on disk under
`openspec/changes/archive/`, which `.gitignore` excludes deliberately: archived
changes stay local as a working record. There is nothing to commit there, and
no task asks for it.

So this change opens with its inherited work already banked, which is the state
its staging was written to reach. What remains is under "Not delivered" below.

### Delivered and in the tree

- **Fold's map is inverted.** `dsp::FrogBlock::SetFold` maps with the ceiling
  and floor exchanged, so raising the knob divides less and fold density rises.
  The map is geometric, so the knob's midpoint lands on the same value either
  way and the shipped default is preserved by construction.
- **Link is gone; Feedback is at the slot it held.** `SetFeedback`,
  `feedbackCoefficient`, `kMaxFeedbackCoefficient` and `folderFeedback` are in
  `app/dsp/Drive.hpp`, and the loop wraps the folder's leg alone rather than the
  whole shaping stage. The gain-coupling term Link used to carry survives as
  `kGainCouplingScalar` inside `dsp::PolynomialDrive::SetCoefs`, at exactly the
  weight Link's registered default produced, so removing the control does not
  move the voice at that default.
- **The Fuzz and Comb/Peak blends are floored.** `dsp::FlooredEqualPowerBlend`
  and `dsp::FloorBlendGains` are defined in `app/dsp/Limiter.hpp` and called by
  `dsp::FrogBlock`'s Fuzz blend in `app/dsp/Drive.hpp` and by
  `dsp::FilterFxChain`'s Comb/Peak blend in `app/dsp/FilterFx.hpp` — one
  definition, two call sites. Fold is no longer multiplied by zero at Fuzz
  maximum.
- **Anti-alias is an equal-power crossfade with a warped knob.**
  `dsp::Oversampler2x::SetAntiAliasBrightness`, which `dsp::FrogBlock` reaches
  through its oversampler member, applies `kAntiAliasKnobExponent` and
  the crossfade calls `dsp::EqualPowerWetDry`.
- **Symmetry is the bipolar law.** `dsp::FrogBlock::SetSymmetry` is bipolar
  about the knob's centre, injected at the folder's input, with its anchor
  applied to the output and to the fed-back value. The slot's registered default
  is unchanged from the control it replaced; what changed is the mechanism, not
  the number — the old offset was cancelled before it reached the folder and
  this one reaches it.
- **Both citation gates resolve exactly, with one stated exemption.**
  `app/check_spec_checks_resolve.py` and `app/check_citations_resolve.py`
  accept a token only as an exact known test-case name or an exact
  repo-relative path. The exemption is a citation carrying a git commit pin
  (`sha:path:line`), which `check_citations_resolve.py` accepts and counts
  without looking the path up at all, because a path pinned to a commit is not
  expected to resolve in the working tree. The pin is not resolved against the
  object database either, so a malformed sha passes. That is recorded here, not
  scheduled: no task in this change repairs it.
- **`app/check_citations_resolve.py` fails on a split citation.** It joins each
  comment line to the next line carrying text and fails when a citation resolves
  and straddles the join. It found citations no line-by-line scan could see,
  some of which were also hiding a line number into this tree from the same
  script's own rule against those. All of them are repaired, and the gate now
  reports what remains rather than this sentence doing so.
  **Its adversarial pass is spent, and what got through is recorded here.** A
  fresh context enumerated nine accepting branches and ran nine evasions against
  the real script; four passed. The instrument is live: the same citation on one
  line exits 0 and split across a break exits 1. Three of the four are coverage
  gaps in the joiner — two blank spacer lines exceed its one-line lookahead, a
  three-way split never chains a second join, and an unrelated filler comment
  between the fragments is consumed as the joined line. The fourth is fatal on
  its own terms: a citation that is BOTH split AND points at a path that exists
  nowhere passes silently, because the split report fires only when the join
  resolves. THE GATE IS NOT REBUILT FOR ANY OF THEM, and no task in this change
  repairs them. Every gate has holes; a recorded hole is a finding, not an
  instruction. Closing the fatal one would also make the gate newly red against
  an unmeasured number of existing comments, which is a change of its own with
  its own trace, not a repair to fold into this one.
- **`app/check_modified_requirements_restate_promoted.py` exists** and is wired
  into `app/Makefile`'s `test` target.
- **The manual and quickdict** carry Fold, Feedback, Fuzz, Symmetry and
  Comb/Peak at their delivered behaviour.
- **Hygiene is swept.** `app/dsp/`, `app/` and its gate scripts, `app/vst/`,
  `openspec/specs/`, the repo-root documents and `External/Sheaf/` were swept;
  what that found is repaired, including a stale ceiling figure restated twice
  in `app/FroggersDspParityTests.cpp` and a comment there claiming the Filter
  bank wires a literal where it wires `dsp::kMaxResonantBumpHeight`.

### Not delivered

- **Peak gain costs level everywhere it is not heard.** `dsp::FilterFxChain`
  divides the peak branch by `peak.height` with no makeup, so raising the
  control flattens the bump's own resonance while attenuating everything else.
- **The cross-feed is defined twice.** `dsp::StereoDelay` and `dsp::Reverb` each
  compute the same weighted L/R average inline. `app/dsp/StereoField.hpp` does
  not exist; `dsp::DelayDiffuser` and `dsp::SchroederAllpassSection` are in
  `app/dsp/Delay.hpp`.
- **Reverb's slot 7 is labelled Diffusion and is a cross-feed weight**, not a
  diffuser. `dsp::Reverb::Process` still takes `diffusionKnob01`. The label is
  spelled at every site the surface gate checks, and at each of them the Delay page's own
  Diffusion sits beside it.
- **Two gates accept what they were written to reject** — recorded under the
  gate-holes work below.
- The coloration measurement, the remaining document pass, the spec-delta
  re-count, and delivery.

### Gate and test status

The suite is `nice -n 10 make -C app test -j2`. **Never raise `-j` above 2**:
this is an 8-core/16GB machine that freezes above it.

The pass and fail counts are not written here. The recipe stops at its first
failure, so its exit code alone says nothing about the binaries it never
reached, and a count in prose is falsified by the next commit that adds a case.
The baseline task in the task list produces both — the recipe's exit code and a
per-binary pass count from running each binary by path — and that task's result
is where the figures live.

## Rulings carried forward, not reopened

- **Peak gain keeps the existing `1/height` trim, and the makeup that ruling
  paired it with does not exist.** The original ruling took candidate (c) — keep
  the trim, add a level-holding makeup — after measuring three candidates
  through the full chain with the comb ringing, where the other two moved total
  level the wrong way or further and the blowout ceiling was identical for all
  three. The makeup half is refuted: any makeup at the required placement sits
  ahead of a limiter whose response is strictly increasing, so it raises the
  output ceiling, and no finite law holds total level to the end of the travel.
  The trim half is confirmed, and by a different argument than the ruling gave.
  It is not what bounds the output — `peakLimiter` is, and cancelling the trim
  leaves the output bounded within 0.04 dB. What the trim buys is a clean
  output: cancelling it costs 6.3 to 13.8 dB of harmonic distortion and 7.0 to
  15.8 dB of intermodulation at the bank's registered defaults, and swings the
  limiter's gain reduction 8.90 dB at up to 198 dB per second against 1.67 dB at
  55. So Peak gain's level cost stays, and the documents state it.
- **The reverb tank's diffuser is `dsp::DelayDiffuser` at its own section
  lengths**, not retuned for the tank. Every shorter set measured inert across
  the whole travel.
- **`dsp::CrossFeedPair` takes a weight in [0, 0.5] and is identity at 0.**
- **The operator holds no stored patches** (2026-09-11), so a default-sound
  change needs no migration.
- **The operator ruled the new diffuser may colour** (2026-09-11). A ruling that
  something may colour is not a licence to leave it undocumented.

## Working rules

1. **A figure from a document is never an acceptance criterion.** A task that
   gates on a number states its grid and tap point, or measures first and sets
   the threshold from what it measured.
2. **No task hands an executor a check without its assertion already written.**
   A blank is a defect in the task, not in the executor.
3. **MEASURE and IMPLEMENT are separate tasks** wherever the threshold is not
   already known. The measuring task is read-only and reports a number.
4. **An executor that meets a conflict reports and stops.** It may not adjust a
   threshold, weaken an assertion, or edit a requirement to match code.
5. **A check that asserts only liveness asserts nothing.** `isfinite` and `> 0`
   are required as controls and are never the whole of a case.
6. **Every executor's diff is read before its result is accepted**, specifically
   for weakened assertions.
7. **A gate gets ONE adversarial pass from a context that did not build it.**
   What got through is RECORDED. It is rebuilt only if a hole defeats the gate's
   purpose — not because a hole exists, since every gate has holes. This rule is
   new here, and it is what stops the predecessor's loop: three rebuild rounds
   went into one check because a HOLED verdict was read as an instruction to
   rebuild rather than as the finding it is.

## Precedence

`omni-rule.md` outranks every instruction that reaches an agent working on this
change: this proposal, the task list, an executor's brief, a schedule, an
inferred urgency, and an executor's own need to look finished. Only an explicit,
knowing instruction from the operator amends it.

**An executor's deliverable is a REPORT. Code changes are a side effect of it.**
An executor that reports a conflict and writes nothing has COMPLETED its task.
One that ships working code past a stop condition has failed, whatever the gate
says afterwards.

## Impact

- **Affected spec:** `froggers-sheaf-parameter-model`, with two MODIFIED
  requirements — the bank layout, and the Drive page control travel requirement.
- **Code this change still edits:** `app/dsp/FilterFx.hpp`,
  `app/FroggersAppCore.hpp`, `app/dsp/Reverb.hpp`, `app/dsp/Delay.hpp`, a new
  `app/dsp/StereoField.hpp`, `app/FroggersParameters.hpp`,
  `app/FroggersUiSurface.hpp`, `app/FroggersSurfaceTests.cpp`,
  `app/FroggersDspParityTests.cpp`, `app/check_spec_checks_resolve.py` and
  `app/check_modified_requirements_restate_promoted.py`.
- **Code already changed, not to be re-done:** `app/dsp/Drive.hpp`,
  `app/dsp/Limiter.hpp`, `app/check_citations_resolve.py`, `app/check_common.py`,
  `app/Makefile`, and the files under `app/` carrying comment-only
  citation-path edits. Those last are comment-only in full: the count and the
  list are produced by `git diff` rather than restated here, because a list of
  filenames in prose is a claim that rots on the next edit.
- **Why files outside this change's subject appear in its diff.** Making a cited
  path resolve exactly turned every bare citation already in the tree into a
  build failure, so repairing them was neither optional nor separable — the gate
  and the citations it reads have to move together or the build is red between
  the two commits.
- **Affected gates:** `app/check_docs_match_parameter_table.py` binds every
  manual and quickdict bold entry to the parameter table by name and slot, so a
  parameter rename and its two document rows must land in the same step.
- **Affected documents:** `MANUAL.md`, `QUICK_DICT.md`.
- **Sound.** The Drive page's default already moved once, when the Fuzz blend
  was floored. Reverb's slot 7 changes mechanism entirely and Reverb's Stereo
  width gains the cross-feed.
- **Delivery is a push to `main`.** This repository does not use pull requests.
