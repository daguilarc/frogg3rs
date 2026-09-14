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
  `FroggersParamSpec`'s own `0.0f`, and so does DELAY'S FEEDBACK row, which this
  change adds to the list after tracing it. Send at 0 starves the wet path.
  Stereo width at 0 makes `wetL` and `wetR` bit-equal, because
  `dsp::Reverb::Process` computes `wetL = mid + width * (aOut - mid)` and the
  same for `wetR`, so correlation reads a flat +1 whatever the tank does.
  A flat +1 row is VOID rather than negative.
  **IT READS +1, NOT `nan`.** Earlier artifact text in this change said a
  starved probe reads `nan`; that is wrong for the only correlation code in the
  tree. `Correlation::Value()` divides only when its denominator is positive and
  returns `1.0` otherwise, so a constant pair — width at 0, or Send at 0 leaving
  both channels silent — reads as perfectly correlated. The VOID signature to
  watch for is a flat +1.
  A `nan` is NOT the same finding and is not covered by that guard, which
  catches a zero denominator only. A non-finite sample reaching the accumulator
  makes the denominator infinite, `Inf > 0.0` passes, and the division yields
  `nan` anyway. No measurement in this change has produced one and none checked
  whether the tank can; a task that meets a `nan` stops and reports it as
  unexplained rather than reading it as a starved probe.
- **DELAY'S FEEDBACK DEFAULT IS WHY THAT PAGE'S WIDTH IS ALL TIME OFFSET.**
  `dsp::StereoDelay::Process` computes its cross-feed weight as
  `p.dwid * 0.5f * widthBalance` and applies it to the FEEDBACK tap; that tap
  reaches the line multiplied by `fbk`, which is Feedback's own clamped value.
  At Feedback's registered `0.0f` the cross-fed term contributes exactly
  nothing, while the width time-spread on the read taps still reaches the
  output. Any task claiming Delay's width does or does not cross-feed states
  which Feedback value it measured at.
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

Working rule 3 makes MEASURE and IMPLEMENT separate tasks wherever the
threshold is not already known. It is not known here, so 1.1 is split: 1.1a
measures and reports, 1.1b writes the assertion from what 1.1a returns. An
executor handed both at once is handed a blank to fill toward green.

- [x] 1.1a MEASURE the two rows against the DELIVERED tank and REPORT them. Write
      no assertion and land no check in this task; its whole deliverable is the
      report.
      WHY THIS IS NOT ALREADY DONE: the superseded change measured the bound
      against the pre-change tank and an executor then found it dead. It
      reverted rather than retuning, which was correct.
      GRID, both rows over {0.0, 0.25, 0.5, 0.75, 1.0}:
      the WIDTH row sweeps Stereo width with Density at its registered default;
      the DENSITY row sweeps Density with Stereo width HELD at 0.5 and that
      value named. Stereo width may not be held at its own `0.0f` registered
      default on either row, because at 0 the wet pair is bit-equal and the row
      is VOID. Damping is held at its registered default on both rows and is not
      swept — it is not part of this comparison, and the task line that used to
      name it here in place of Stereo width was a defect this change repaired.
      TAP POINT: `dsp::Reverb`'s own `wetL`/`wetR` after `Process`, Send opened
      to 1.0 because its own `0.0f` default never feeds the tank, Room size,
      Decay and Pre-delay at their registered defaults, over the same
      warmup-then-measure noise burst
      `reverb_damping_filter_split_lowers_wet_leg_correlation_at_every_setting`
      establishes.
      THE WIDTH ROW IS ALREADY MEASURED at that tap point and reproduces: the
      case above prints a post-split travel of 1.03554689 over exactly this
      grid. 1.1a re-derives it as its own liveness control and reports whether
      it reproduces; a width travel that does not reproduce means the rig
      differs from the one named here, and the run is VOID rather than
      negative.
      1.1a ALSO REPORTS the Density row's own NOISE FLOOR: re-run one Density
      point with a different noise seed and report the spread, so 1.1b knows
      what margin a direction claim would have to clear.
- [x] 1.1b Land ONE case pinning Density's travel against Stereo width's, over
      1.1a's grid and tap point.
      1.1a'S MEASURED NUMBERS, at seed 20260913, warmup 12000, measure 12000:
      the WIDTH row runs 1.000000, 0.874229, 0.576760, 0.246914, -0.035547,
      travel 1.03554689, reproducing the known figure exactly. The DENSITY row
      runs 0.576760, 0.571504, 0.567800, 0.574188, 0.583021, travel 0.01522106,
      not monotone — it dips to the middle of the grid and rises again.
      THE NOISE FLOOR IS LARGER THAN THE DENSITY TRAVEL. Re-running the
      Density 0.5 / width 0.5 point at seed 778899001 gave 0.620062 against
      0.567800, a spread of 0.05226205 — three and a half times the whole
      density row's travel. So DENSITY'S DIRECTION IS NOT ASSERTED, and the
      near-flat row is not asserted to be flat either; only the comparison
      below survives, and the case says why in its own comment.
      THE CASE MEASURES AND PRINTS THAT SPREAD ITSELF rather than quoting it.
      The floor is what the ten-times bound is chosen against, so a floor
      living only in prose is a bound resting on a number nothing can
      falsify — the same defect this change withdrew four inherited figures
      for. It is PRINTED, not asserted, on the precedent the damping-split
      case already sets for its own width travel: a seed-to-seed spread is
      not a stable bound and must not become one.
      THE BOUND: the width row's travel is at least TEN TIMES the density
      row's, both measured in the SAME run, never pinned as absolutes. It
      measured 68x. Ten is chosen against the noise floor rather than against
      the measurement: density's travel is noise-dominated, so another seed
      could plausibly inflate it to the floor itself, 0.0523, which would still
      leave the ratio near 20. Ten carries about a factor of two of headroom
      below that worst case. A bound near the measured 68 would be pinning
      noise.
      THE LIVENESS PROOF IS THE WIDTH ROW, asserted in the same case: its
      travel exceeds 0.5, twenty times the noise floor. A run whose width row
      does not move has a dead tank, and every density number in it is VOID
      rather than small.
      A FOURTH ASSERTION CLOSES THE RATIO'S OWN ESCAPE HATCH, added after an
      adversarial read: `densityTravel > 0.0`. Without it the ratio passes
      VACUOUSLY when Density stops reaching the tank at all — an unwired knob
      makes the five points bit-equal, drives the density travel to exactly
      zero, and reduces the comparison to "the width row moved", so the case
      would pass more comfortably broken than working. Proven by disconnecting
      Density and re-running: density travel 0, ratio inf, the ratio assertion
      still GREEN and only the new one red. The run is deterministic, so an
      exact zero means bit-equal rather than merely small, and the assertion
      claims nothing about how far or which way Density moves.
      THE WIDTH ROW IS ALSO ASSERTED TO FALL MONOTONICALLY, point to point.
      It does: 1.000000, 0.874229, 0.576760, 0.246914, -0.035547, strictly
      decreasing at every step. This clause is not decoration — the spec delta's
      scenario "A stereo-image control widens across its whole travel" requires
      correlation to fall monotonically, and task 3.1 moves that scenario's
      `Check:` onto this case. A case that did not assert monotonicity would
      leave that scenario promoted with nothing behind it, which is the exact
      failure the promotion rule exists to stop.
      THE PROPOSAL'S "ABOUT A HUNDRED AND FIFTEEN TIMES" IS NOT THIS RATIO and
      must not be written into the case. It divided the width row's TRAVEL by
      the density row's largest single STEP — two different quantities under
      one phrase. Travel against travel is 68. Of the four density deltas the
      superseded change recorded, only the first reproduces; the other three do
      not, which says its rig is not recoverable from what it wrote down.
      Fix the proposal's sentence in the same stage.
      THE ASSERTION'S SHAPE, which does not depend on the numbers: the Stereo
      width row moves correlation by much more than the Density row does, with
      the width row carrying the liveness proof for the near-flat one. Density
      now moves correlation A LITTLE rather than not at all, because an allpass
      on the input genuinely decorrelates where a cross-feed weight did not, so
      the near-zero wording the superseded change used is wrong for the
      delivered mechanism.
      DO NOT ASSERT DENSITY'S DIRECTION unless 1.1a's measurement clears the
      noise floor 1.1a reported, by a margin stated in the case.
      THE CORRELATION HELPER HAS TO BE LIFTED BEFORE IT CAN BE REUSED. The only
      correlation arithmetic in the tree is `struct Correlation`, declared
      INSIDE the body of
      `reverb_damping_filter_split_lowers_wet_leg_correlation_at_every_setting`
      in `app/FroggersDspParityTests.cpp`, so it is invisible to any other case.
      Lift it to file scope unchanged and have both cases call it — a second
      copy is the duplication the repetition rule forbids, and a second
      independent implementation is worse.
      POSITIVE CONTROL FOR THE LIFT, both halves required. First, a literal
      text diff of the struct body before and after, showing only the change of
      scope. Second, that existing case's five printed gaps and its printed
      post-split travel, byte-identical before and after, both listings
      reported. The printed output ALONE is not sufficient and the diff is what
      closes the hole: the file prints at the default six significant digits, so
      a lift that quietly reordered the accumulation would round away below the
      last printed digit and still read as byte-identical.
- [x] 1.2 Stage gate: suite green, one postflight in a fresh context over the
      whole stage, then one commit and a push.
      "GREEN" HAS A NAMED EXCLUSION AND NO OTHER. `check-spec-checks-resolve`
      fails on two `Check:` names in
      `openspec/changes/frogg3rs-randomize-depth-reclaim/`, an untracked change
      another session is authoring in this working tree. That change is out of
      scope here for the same reason `frogg3rs-midi-controller-resilience` is,
      and this change may not edit it. Report the suite as green APART FROM
      those two named failures, listing them, and confirm the count of failures
      has not grown. Any OTHER failure stops the stage.

## Stage 2 — the documents

House style: plain present tense saying what a control does; no jokes; no
defining by negation; no history the reader never saw; and no task numbers,
change names, planning-document references or rule citations in comments, test
names or strings.

- [x] 2.1 `MANUAL.md` and `QUICK_DICT.md` for both pages' Diffusion, Density and
      Stereo width. The Drive controls are already updated and are not re-done.
      THE FILTER CONTROLS ARE ALSO NOT RE-DONE, and this line used to ask for
      them. "The two Filter controls" meant Peak gain and Comb/Peak, the two the
      promoted spec carries scenarios for. Both documents already carry what
      those scenarios demand — Peak gain's level cost at the output, and
      Comb/Peak's floored equal-power blend with the held-back branch's roughly
      -22 dB — so the clause was stale scope inherited from the superseded
      chain, not work outstanding. Read them once to confirm, change nothing,
      and say so.
      DELAY'S STEREO WIDTH ENTRY IS A WYSIWYG DEFECT, not a wording refresh.
      `MANUAL.md` describes slot 4 as "cross-feed and time-spread" and says
      higher values "blend them into each other less". This change's own
      measurement says the widening is ENTIRELY the L/R time offset, because the
      cross-fed term is gated to an exact zero at Feedback's registered default.
      Trace the gate in `app/dsp/Delay.hpp` before writing the replacement
      sentence, and say what the cross-feed does at the default and what moves
      it off zero. Where the label and the mechanism disagree the mechanism is
      the defect — but the mechanism here is not this change's to alter, so the
      document states what the code does.
- [x] 2.2 State on BOTH the Delay and Reverb pages what Diffusion and Density
      each mean, so the difference is on the page rather than in the reader's
      memory. Delay's Diffusion drives an allpass cascade on its wet tap;
      Reverb's Density drives one on the tank's input.
- [x] 2.3 `MANUAL.md` states plainly that raising Density trades smoothness for
      coloration, and at roughly which part of the travel the coloration becomes
      audible, backed by the echo-density figures already in the tree: 0.0514,
      0.2342, 0.3403, 0.4132, 0.4782 across the travel.
- [x] 2.4 State what Damping now does to the stereo image. Each tank line has
      its own filter, so damping the tail no longer closes the image; say so
      where Damping is described, because the superseded behaviour is what a
      returning reader remembers.
- [x] 2.5 Stage gate: full suite green, one postflight in a fresh context over
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
