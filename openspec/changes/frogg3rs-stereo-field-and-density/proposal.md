# Proposal — one definition for the stereo field, and Reverb's slot 7

**Created 2026-09-12.** Supersedes `frogg3rs-wysiwyg-deliver`, which superseded
`frogg3rs-wysiwyg-finish`, which superseded `frogg3rs-wysiwyg-controls`. That
chain's delivered work is committed; this change carries forward only what is
left, plus the whole spec delta.

The thesis, in the operator's words: WYSIWYG is the omni rule instantiated in
the UX domain. A control's name and position are claims about what it does,
held to the same standard as any other claim — traced, not asserted. Where the
label and the mechanism disagree, the mechanism is the defect and the label is
the specification, unless reading says otherwise.

## Why every claim below was re-read rather than copied

`omni-rule.md`'s preamble carries a rule that governs this document more
directly than anything else in it: **PROVENANCE IS NEVER VERIFICATION.** No
route by which text enters a change carries authority with it. Copying is
authoring. The obligation attaches to a claim at the point of USE, every time,
and is discharged only by reading.

This change exists three generations down a chain that copied claims forward
twice without re-reading them, and an audit of the two planning files found
roughly forty defects in exactly that carried-forward text. That count comes
from the auditing session's own report and is INHERITED: the defects it names
were repaired, so the tree at this commit cannot reproduce it. The mechanism
behind it is narrow and worth naming, because it is not carelessness:

**Preflight's own output was never preflighted.** Four repairs in the
superseding session were each written by the context that held the finding and
shipped straight to an executor. The context holding a finding is the one least
able to see what its own fix assumes, because the fix is built from the same
reading that produced the finding. Each repair carried a new defect:

- A gate rule was narrowed to match the trees it was believed able to index.
  The belief was false: "unindexed" was a property of the recogniser, not of
  those trees. Widening the recogniser made 72 real tests in two files visible
  that no earlier version could see — a figure read from the repairing commit's
  own message, not re-counted here.
- A selection criterion was written as "the ceiling does not rise". Any makeup
  ahead of a limiter whose response is strictly increasing raises the ceiling by
  something, so that criterion is satisfiable only where the makeup is
  indistinguishable from 1 — it ruled out its own search by construction.
- A check was rewritten to assert a comparison between the new law and the
  shipped law. The measuring task then shipped no new law, which left the
  assertion comparing the shipped law against itself.
- A symbol was named from an `awk` heuristic over the tree rather than from
  reading a declaration, and the name was simply false. THIS ONE IS RECORDED
  FROM THE SESSION'S REPORT AND NOT TRACED HERE: the defective name is no
  longer in the artifacts, so the tree at this commit cannot confirm which
  symbol it was. No task relies on it.

**The coverage finding is the reason a fifth gate now exists.** The artifacts in
this chain that had mechanical checks over them came back from that audit clean.
The two that had none — a change's `tasks.md` and `proposal.md`, which nothing
read — carried the roughly forty defects. `app/check_artifact_symbols_resolve.py`
closes that gap: it resolves backticked paths, qualified names, `k`-prefixed
constants, call forms and lower snake_case names in exactly those two files.

**Say plainly what that gate cannot do.** It resolves NAMES. It cannot tell
whether a claim about a real name is true. A sentence asserting that a real
method does something it does not do, is called from somewhere it is not, or
holds a value it does not hold passes it untouched. Its green is evidence that
no name dangles, and evidence of nothing else.

**And it reads only files git TRACKS.** A change directory that has not been
committed is invisible to it, so the gate returns 0 on an uncommitted handoff
without opening a single file. Commit these artifacts, then trust the green.

## How claims are marked in this change

Every claim carried out of the superseded change was re-read against the tree at
this commit. Claims settled by reading cite the file and the symbol. Claims that
reading cannot settle — chiefly figures produced by a measurement in a session
that is over — are marked INHERITED AND UNTRACED where they appear, and **no
task in this change may rest on one.** Where such a figure matters, a MEASURE
task produces it again.

## State of the tree

Everything under "Delivered" is committed. The working tree is otherwise clean
apart from `frogg3rs-midi-controller-resilience`, a separate change the operator
is holding; this change does not touch it and no blanket `git add` may sweep it
into a commit.

### Delivered, verified against the tree at this commit

- **Both citation gates reject what they were written to reject.**
  `app/check_spec_checks_resolve.py` resolves a `Check:` only by naming a real
  case, a file that defines the case it is paired with, a gate the test target
  runs, or a declared-manual marker, and reads no name out of a comment.
  `app/check_modified_requirements_restate_promoted.py` covers a dropped
  promoted clause only where the declared fragment occurs in exactly one, and
  pairs scenarios by clause overlap rather than by title. The citations both
  gates invalidated are repaired across the two promoted specs. Both are wired
  into `app/Makefile`'s `test` target.
- **Peak gain is measured and closed NEGATIVE. No code shipped.** The
  `1.0f / peak.height` trim on the peak branch in `app/dsp/FilterFx.hpp` STAYS.
  Removing it leaves the output bounded — `peakLimiter` is what bounds it — but
  costs 6.3 to 13.8 dB of harmonic distortion and 7.0 to 15.8 dB of
  intermodulation at the bank's registered defaults, and swings the limiter's
  gain reduction 8.90 dB at up to 198 dB per second. INHERITED AND UNTRACED:
  those five figures come from a measurement session that is over, and nothing
  in the tree reproduces them. What IS in the tree is the regression pin,
  `filter_bank_peak_gain_travel_measurement_at_and_away_from_resonance` in
  `app/FroggersDspParityTests.cpp`, which asserts the level at the bump's own
  centre frequency is flat across the whole travel and that both away rows fall
  at every step. `MANUAL.md` and `QUICK_DICT.md` both state what the control
  delivers at the output and what it costs.
- **The stale ceiling comments name the constant.** Every comment site that
  stated the resonant-bump ceiling as a numeral now reads
  `dsp::kMaxResonantBumpHeight`, whose definition is in `app/dsp/FilterFx.hpp`.
  No count is written here: `grep` produces it and a count in prose is wrong at
  the next commit.
- **Document work for the Drive controls, the cross-instrument Feedback
  section, and the Anti-alias qualification** is in both documents.
- **`app/check_artifact_symbols_resolve.py` exists** and is wired into the same
  `test` target.

### Not delivered — this change's whole subject

- **The cross-feed is defined twice.** `dsp::StereoDelay::Process` computes
  `fbL = dL * (1.0f - cross) + dR * cross` and `dsp::Reverb::Process` computes
  `aFb = valB * (1.0f - cross) + valA * cross`. Same weighted average,
  transposed operands. NEW `app/dsp/StereoField.hpp` does not exist;
  `dsp::DelayDiffuser` and `dsp::SchroederAllpassSection` are both in
  `app/dsp/Delay.hpp`.
- **Reverb's slot 7 is labelled Diffusion and is a cross-feed weight**, not a
  diffuser. `dsp::Reverb::Process` takes `diffusionKnob01` and uses it only as
  `cross = diffusion * 0.5f`.
- **The DAMPING knob sets the stereo image.** See the finding below.
- The correlation, default and echo-density measurements; the remaining
  document pass; the spec-delta close-out.

## The finding that has to be carried, and what of it is traced

`dsp::Reverb::Process` holds ONE `OnePoleLowPass dampFilter` and calls
`dampFilter.Process(valA)` then `dampFilter.Process(valB)` on it in the same
sample. `OnePoleLowPass::Process` in `app/dsp/DspMath.hpp` is
`output = alpha * input + (1.0f - alpha) * output`, one recursive state.
Substituting, line B's output is `alpha * valB + (1.0f - alpha) * aOut`: line B
is mixed with line A by the filter's own memory, so the Damping control sets how
correlated the two tank lines are.

**TRACED against production at this commit**, not against a replica: the shared
instance, the two calls, and the recursion that makes the second depend on the
first are all read from `app/dsp/Reverb.hpp` and `app/dsp/DspMath.hpp`.

**INHERITED AND UNTRACED**, and no task rests on them: the correlation figures a
replica produced for damping 0, 0.5 and 1.0, and the figure for splitting into
per-line filters. A replica is not the instrument. The measurement task below
produces these again through the production router.

This is a label-versus-mechanism defect of the same family as the rest of this
chain: a control named Damping moves the stereo width, and the controls named
for the stereo field move it less.

## Rulings carried forward

- **Peak gain keeps the trim and ships no makeup.** No makeup at that placement
  can hold level: it sits ahead of a limiter whose response is strictly
  increasing, so it raises the output ceiling. TRACED: `peakLimiter`'s ceiling
  is `kStageCeiling`, defined in `app/dsp/Limiter.hpp`.
- **The reverb tank's diffuser is `dsp::DelayDiffuser` at its own section
  lengths**, not retuned for the tank. TRACED that those lengths are 4.7, 12.3
  and 21.1 ms, from `kSection1BaseSeconds`, `kSection2BaseSeconds` and
  `kSection3BaseSeconds` in `app/dsp/Delay.hpp`. INHERITED AND UNTRACED: that
  every shorter set measured inert across the whole travel.
- **`dsp::CrossFeedPair` takes a weight in [0, 0.5] and is identity at 0.** This
  is a design constraint on a symbol that does not exist yet, not a claim about
  the tree. TRACED that both production sites already scale a knob by `0.5f`
  before using it as the weight.
- **The operator holds no stored patches** (2026-09-11), so a default-sound
  change needs no migration. INHERITED: an operator statement, not a fact about
  the tree.
- **The operator ruled the new diffuser may colour** (2026-09-11). A ruling that
  something may colour is not a licence to leave it undocumented.

## Working rules

1. **A count or a figure is not written into prose where a check can produce
   it.** A prose claim about the tree is falsified by the next edit to the tree.
2. **No task hands an executor a check without its assertion already written.**
   A blank is concrete, demands filling, and will be filled toward green. The
   blank is a defect in the task, not in the executor.
3. **MEASURE and IMPLEMENT are separate tasks** wherever the threshold is not
   already known. The measuring task is read-only and reports a number.
4. **An executor that meets a conflict reports and stops.** It may not adjust a
   threshold, weaken an assertion, or edit a requirement to match code.
5. **A check that asserts only liveness asserts nothing.** `isfinite` and
   `> 0` are required as controls and are never the whole of a case.
6. **A gate gets ONE adversarial pass from a context that did not build it.**
   What got through is RECORDED. It is rebuilt only where a hole defeats the
   gate's purpose, since every gate has holes.
7. **A figure inherited from a finished session is not an acceptance
   criterion.** Measure it again, or state the grid and tap point and measure
   first.

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
  The two transitional NOTEs in the promoted text that pointed at the superseded
  change point here instead, and this delta's declared edits move with them.
- **Directories this change touches, each of which gets the hygiene sweep:**
  `app/dsp/`, `app/` (its test sources and gate scripts), `openspec/specs/`,
  `openspec/changes/`, and the repository root's documents.
- **Code this change edits:** `app/dsp/Reverb.hpp`, `app/dsp/Delay.hpp`, a NEW
  `app/dsp/StereoField.hpp`, `app/FroggersParameters.hpp`,
  `app/FroggersUiSurface.hpp`, `app/FroggersSurfaceTests.cpp`,
  `app/FroggersDspParityTests.cpp`.
- **Out of scope, stated so it is not rediscovered:**
  `src/core/FroggersEngine.hpp`'s own cross-feed inside `ProcessReverb` is
  frozen firmware, and the app tree's port is a sanctioned copy that
  `app/Makefile` forbids including from.
- **Affected gates:** `app/check_docs_match_parameter_table.py` binds every
  manual and quickdict bold entry to the parameter table by name and slot, so a
  parameter rename and its two document rows must land in the same step.
  `app/check_artifact_symbols_resolve.py` reads this change's own `tasks.md` and
  `proposal.md`, and only once they are committed.
- **Affected documents:** `MANUAL.md`, `QUICK_DICT.md`.
- **Sound.** Reverb's slot 7 changes mechanism entirely, and Reverb's Stereo
  width gains the cross-feed.
- **Delivery is a push to `main`.** This repository does not use pull requests.
