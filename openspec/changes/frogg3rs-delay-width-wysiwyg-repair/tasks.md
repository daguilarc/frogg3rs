# Tasks — Repair the Delay page's Stereo width, and close the spec delta

**Work not listed in this file does not become a task unless the operator adds
it.** The superseded change carried this repair as PROSE IN THE PROPOSAL WITH NO
TASK, which is one reason it was handed forward instead of done. It is task 1.2
here.

Ticked tasks are delivered and committed. Do not re-do them.

## How this change is sequenced

**Every STAGE ends in one commit and a push, after that stage's postflight.**
Not every task.

**Every commit stages the paths it names and nothing else.** `git add -A`,
`git add .` and `git commit -a` are forbidden:
`openspec/changes/frogg3rs-midi-controller-resilience/` and
`openspec/changes/frogg3rs-randomize-depth-reclaim/` are untracked in this tree
and owned by other sessions, so any blanket stage sweeps in work the operator is
holding, and there is no pull request to catch it. Check `git status --short`
before every commit and confirm both directories are still untracked afterwards.
No AI attribution in any commit message.

**"Suite green" has exactly two named exclusions.**
`check-spec-checks-resolve` fails on two `Check:` names in
`openspec/changes/frogg3rs-randomize-depth-reclaim/`. Report the suite as green
apart from those two, name them, and confirm the count has not grown. Any other
failure stops the stage.

**Mechanics that have wasted time before, recorded once:**
`make test` halts at that pre-existing gate failure BEFORE building anything, so
use `make -k`. `app/Makefile`'s `BUILD_DIR` is an ABSOLUTE path, so
`make build/<binary>` has no rule and make exits **0** saying "up to date" for
any binary that already exists — build the absolute-path target or `rm` the
binary first, or a failed rebuild will hide. Run every test binary by path. Cap
builds at `nice -n 10 make -j2`; this Mac freezes above that. The baseline
before this change is **394 passing, 0 failing**.

## How every figure in this change is produced

- Measure through the production path, naming every knob moved off its
  registered default and why.
- **The Delay page's registered defaults are a dead instrument for this
  question.** Wet/dry, Send, Stereo width, Feedback and Freeze all omit their
  third field in `app/FroggersParameters.hpp` and take `FroggersParamSpec`'s own
  `0.0f`. Send at 0 never feeds the line, so the wet path is empty and a
  correlation probe on a constant pair reads a flat **+1** — VOID, not negative.
  Open Send and name the value.
- **A flat +1 is the VOID signature. A `nan` is NOT the same finding** and is not
  covered by `Correlation::Value()`'s guard, which catches a zero denominator
  only; a non-finite sample gives an infinite denominator, `Inf > 0.0` passes,
  and the division yields `nan` anyway. A task meeting a `nan` stops and reports
  it as unexplained.
- A figure lands in a TEST, never in prose.
- State the GRID and the TAP POINT beside any figure, precisely enough to
  re-derive it.
- Every task producing a check states what the check must ASSERT. A task that
  does not is defective — report it rather than choosing a threshold.

## Stage 1 — the repair

- [ ] 1.1 MEASURE and REPORT, landing no production change. Answer the question
      the proposal says must be answered before a mechanism is chosen: **can the
      cross-feed widen this signal at all?**
      Both delay lines are fed the same mono input and differ only in read time,
      so cross-feeding blends two time-shifted copies of one signal. Measure
      wet-pair L/R correlation across `cross` on its own, with the width
      time-offset held fixed, and report whether ANY cross weight decorrelates
      the pair or whether every weight re-correlates it toward the exact mono at
      0.5.
      Report at Feedback 0.0 and 0.7. State the grid and tap point.
      If no cross weight widens, say so plainly — that finding decides the
      mechanism and is this task's whole deliverable.
- [ ] 1.2 REPAIR the mechanism so Stereo width widens across its whole travel at
      every Feedback setting, then land its check.
      THE BOUND COMES FROM 1.1 AND IS WRITTEN INTO THIS TASK BEFORE THE WORK IS
      DISPATCHED. Do not dispatch 1.2 with this paragraph unfilled.
      THE ASSERTION'S SHAPE, which does not depend on the numbers: wet-pair L/R
      correlation falls point to point across Stereo width's travel, asserted at
      Feedback's registered default AND at a raised Feedback that is named. The
      second row is the one that matters — the defect only appears once the
      feedback path carries the cross-fed content.
      CARRY A LIVENESS BOUND on each row so a dead line cannot pass vacuously,
      and make sure the check cannot pass MORE comfortably when the mechanism is
      disconnected. The superseded change shipped exactly that hole and caught it
      only under an adversarial read: a ratio assertion went vacuous when its
      denominator went to zero.
      PROVE IT CAN FAIL, separately for each assertion, and report the red
      output for each.
      CANDIDATE MECHANISMS, with the constraint map in the proposal binding on
      all of them: take the cross-feed off the Width knob and retire it to a
      fixed coupling (two precedents in this specification); invert its sense so
      the blend is strongest at width 0 where it is provably a no-op; or make
      Width balance the complementary ratio its own design document specifies.
      The last is not merely a candidate — see 1.3.
      **Do not edit `dsp::CrossFeedPair`.** It is shared with the reverb tank.
- [ ] 1.3 Rule on Width balance, whose defect is the parent of 1.2's.
      Its design document specifies a pair of COMPLEMENTARY weights and the
      shipped code multiplies both by a common scalar, so the ratio between the
      two mechanisms — the only thing the knob exists to change — is fixed at
      every setting. The spec delta calls slot 12 "the ratio between" them.
      Decide by reading whether 1.2's repair subsumes this, repairs it, or
      leaves it standing, and say which. If it leaves it standing, that is a
      second WYSIWYG defect in a shipping control and it is reported here with
      its measurements, not handed forward.
      WHICHEVER LAW LANDS, state which Width balance value reproduces the
      shipped voice under it. The research says 0.5 does under a complementary
      law; the registered default is 1.0 under the shipped common-gain law.
      Both cannot be true at once.
- [ ] 1.4 RECAPTURE `stereo_delay_cross_feed_reproduces_its_captured_output_exactly`
      and say in the test's own comment that the capture point moved and why.
      It is a self-capture certifying that a de-duplication refactor changed
      nothing, not a parity pin against any external source, so recapturing it
      after a deliberate behaviour change is legitimate — and silently
      recapturing it destroys its ability to catch a future accidental change.
      ENUMERATE EVERY OTHER PIN ON `Process`'s OUTPUT FIRST, by operand, and
      report found versus changed: the Freeze family,
      `stereo_delay_default_knob_values_reproduce_original_output_exactly`, and
      `stereo_delay_width_balance_mapping_keeps_cross_in_0_1_and_spread_at_or_below_todays_max`,
      which asserts a bound SHAPE the repair must keep satisfying. Do not
      discover these from a red build.
- [ ] 1.5 Amend `app/dsp/Delay.hpp`'s provenance header. It says "nothing on
      this page is newly authored" and the repair makes that false. The header
      already records one deliberate divergence from the retired simulator under
      "REMOVED since"; this one is recorded the same way, naming what changed and
      why the ported behaviour was wrong.
- [ ] 1.6 Stage gate: suite green with the two named exclusions, one postflight
      in a fresh context over the whole stage, then one commit and a push.

## Stage 2 — the documents the repair makes stale

- [ ] 2.1 `MANUAL.md` and `QUICK_DICT.md`'s Delay **Stereo width** entries.
      They currently say the widening is entirely the time offset at the page's
      defaults and that the cross-feed rides the feedback path, reaching the
      repeats once Feedback or Freeze leaves 0. That is accurate to the CURRENT
      mechanism and the repair changes it. Rewrite from what ships after stage 1.
- [ ] 2.2 `MANUAL.md` and `QUICK_DICT.md`'s Delay **Width balance** entries, per
      1.3's ruling. The manual currently calls it "an overall scalar on how
      strongly Stereo width's cross-feed and time-spread apply", which is
      accurate to the common-gain law and false under a complementary one.
      House style: plain present tense saying what a control does; no jokes; no
      defining by negation; no history the reader never saw; no task numbers,
      change names, planning-document references or rule citations.
      `app/check_docs_match_parameter_table.py` binds every bold entry to the
      parameter table by name, label and slot — prose is free, those three are
      not. It reports 84 entries and 0 failures per document today.
- [ ] 2.3 Stage gate: suite green with the two named exclusions, one postflight
      in a fresh context over the whole stage, then one commit and a push.

## Stage 3 — the spec delta and archival

Most of this was done under the superseded change and is carried forward in the
delta already; verify rather than redo.

- [ ] 3.1 The stereo-image scenario's `Check:` currently records the Delay page
      as REFUTED with its measurements. Stage 1 makes it true — repoint it onto
      the check 1.2 lands, and resolve that name by grepping for it.
- [ ] 3.2 Re-count EVERY MODIFIED requirement against the promoted text — three
      of them. **The restate gate does not compare requirement PROSE at all**;
      its bullet collector keeps only dash-opening lines, so read the three
      requirements' paragraphs by hand.
- [ ] 3.3 Every scenario's `Check:` names a test that exists and passes, or is
      marked not yet delivered in the form the gate recognises. Resolve every
      one by grepping. **No `Check:` may point at this change's own `tasks.md`**
      — archival moves that path and every such pointer dangles.
- [ ] 3.4 Confirm the two transitional NOTEs in the promoted spec name THIS
      change. They were repointed when the previous change was superseded and
      must move again; they and the delta's declared edits move together or the
      restate gate goes red.
- [ ] 3.5 Archive this change, promoting its delta.
- [ ] 3.6 Stage gate: suite green with the two named exclusions, one postflight
      in a fresh context, then one commit and a push.

## Audits

- [ ] A.1 Preflight, in a context that did not write this change, covering the
      ARTIFACTS AND THE CODE TOGETHER. It may reject.
- [ ] A.2 Each STAGE gets one postflight in a fresh context before its commit.
      Not one per task.
- [ ] A.3 The repair's check gets one adversarial pass from a context that did
      not build it, which tries to get a broken mechanism past it. A control you
      designed tests the hypothesis you already held.
