# Proposal — Finish the Delay capacity and width repair

Supersedes `frogg3rs-delay-read-capacity-repair`, which superseded
`frogg3rs-delay-width-wysiwyg-repair` and the WYSIWYG chain before it.

**Why superseded.** The predecessor's plan was rewritten so many times during
execution — four preflight rounds, each triggering the rewrite rule — that its
text now records its own corrections more than it records the work. The code it
landed is sound and carried forward whole. What this change replaces is the
plan, not the repair.

## What is already landed, uncommitted, in the working tree

Verified present after a recovery (an executor was killed mid-procedure while it
had checked out `HEAD` versions to measure a baseline, and the tree was restored
from a snapshot):

- `cross = 0.0f` in `dsp::StereoDelay::Process` — the cross-feed weight is
  decoupled from Stereo width and Width balance. The read-time offset is the
  only mechanism width drives.
- Three ordered zero-headroom capacity bounds: base time term, then modulation
  against what remains, then width spread against what remains after that. A
  read of exactly the line capacity is correct and is not shortened.
- A liveness gate rejecting non-finite values directly rather than by accident.
- A capacity grid check driving a real `StereoDelay` at 96 kHz across four
  swept parameters instead of recomputing a formula in isolation.
- A near-capacity assertion changed from strict to inclusive.
- Two recaptured golden-vector pins; a width-balance mapping test rewritten; a
  per-channel storm-test silence check.
- `app/check_delay_capacity_parameters_are_swept.py`, wired into `app/Makefile`,
  failing when a parameter the capacity bound depends on stops being swept.
- Five amended comments in `app/dsp/Delay.hpp`.

## Three capacity routes, all closed

`ReadAt` wraps an over-capacity request modulo the line with no clamp. Three
routes reached it, and the third was found only in the fourth audit round:

1. The width term into the right read. 120114 samples requested, 24114 returned.
2. The modulation term, into BOTH reads — not the left only, as an earlier draft
   said. At zero width the spread is zero, so bounding it cannot pull the right
   read below base plus modulation.
3. The base time term alone, above 48 kHz. `capacity` clamps at 96000 samples
   while `baseSeconds` is sample-rate-independent. At 96 kHz with width off and
   modulation at default: 177947 requested, 81947 returned.

Reading exactly `capacity` is correct — the write follows the read within one
`Process` call. Zero headroom; the one-sample margin an earlier version carried
was derived against a criterion the research had already rejected.

## Known-open defects in what landed

Found by postflight, not yet fixed. These are the first work of this change.

- **A shipped assertion that cannot fail.** The width-balance mapping test
  declares a local `const float cross = 0.0f;` and asserts it lies in `[0,1]` —
  compile-time true against a literal the test wrote itself. It cannot detect a
  change to production's weight, and its comment claims otherwise.
- **The grid check has a gap that hides the defect it exists to catch.**
  Deleting the modulation term from the width-spread budget passes all 190
  tests. The swept `dtim` values miss the regime where that deletion binds,
  around 0.9. Confirmed against real broken code: predicted overrun 6216.19
  samples, measured 6216.
- **Two records that do not exist.** The twelve-binary before/after pass-state
  diff, and break-proofs for the three capacity bounds. Both were run and
  reported in transcripts that are now gone. An adjudicator on the predecessor
  already ruled that an audit's own reporting is not a record.
- **The grid check samples rather than bounds.** Its coverage is a hand-picked
  list, so each gap costs an audit round. An assertion inside `Process` that the
  emitted lag never exceeds capacity would bound it for any caller — two float
  compares, debug-only. Its named blind spot: neither that nor randomisation
  proves the formula is right, only that output stays in range.

## Adjudicated postflight findings against the shipped checks

Three attackers in one batch, an exchange round, and an adjudicator that
reproduced every finding itself (`research/adversarial-postflight-adjudication.md`).
All five are true. What each one shows getting past the checks, and the
repair that closes it:

- **P.** A wrap that admits `capacity` reads one past the line; every
  capacity check stays green. **Q.** Lines allocated shorter than `capacity`
  overflow on every lap; green. Both because every check compares seconds
  against `capacity` and nothing compares the indices used against the
  vector indexed. Repair: index-domain compares in `ReadAt` and
  `WriteSample`, compile-gated like the lag compares.
- **R.** A headroom term shaped as a cubic with roots at the grid's three
  Width balance values overflows at 0.75; green, because nothing drives that
  value. Repair: a random-walk test that drives `Process` across random knob
  values at four sample rates with the compile-gated compares as oracle.
- **S.** The sweep gate counts a literal inside a comment. Repair: strip
  comments before scanning.
- **T.** Four coordinated edits, two of them to the tests' own assertions,
  blind everything. No fixed check rejects an edit to itself; the mechanism
  that does is a mutation gate that compiles the suite against each recorded
  break and requires red. Repair: that gate, in `make test`.

Every repair is new untraced text and gets a fresh attacker batch.

## Rulings this change carries, already settled

- **Width balance is a second WYSIWYG defect.** After the repair, `p.dwid` and
  `widthBalance` enter `Process` only as one product on one term. Two knobs, one
  degree of freedom. Today's shipped voice is at `widthBalance = 1.0`, the
  registered default, not the design document's assumed 0.5.
- **"Same job across pages" is VIOLATED, and is not an operator question.** The
  requirement's own first sentence defines the term: "A control name SHALL
  identify one mechanism across the whole instrument." Reverb's mid/side scaling
  and Delay's read-time offset are different mechanisms sharing one name.
  Recorded, not closed — renaming a shipping control is out of scope.
- **The whole-travel clause at Feedback 0 is satisfiable**, contrary to an
  earlier claim this chain carried to the operator twice. `research/instrument-derivation.md`
  records the repaired law at `dfbk = 0.00` falling 1.000, 0.775, 0.571, 0.439,
  0.345. It is NOT YET DELIVERED — no check exercises Feedback 0.
- **The slot-12 Width Balance clause is false of the code** and was false before
  any repair: both mechanisms were scaled by the same factor, which cancels.

## Impact

- **Code:** `app/dsp/Delay.hpp`, `app/FroggersDspParityTests.cpp`,
  `app/FroggersAudioRoutingTests.cpp`, `app/Makefile`,
  `app/check_delay_capacity_parameters_are_swept.py`,
  `app/check_delay_capacity_break_proofs.py`, `MANUAL.md`,
  `QUICK_DICT.md`, `frogg3rs.code-workspace` (three stale watcher entries —
  only `.emsdk` resolves).
- **Gates:** the four the predecessor named, plus `check_citations_resolve.py`
  and `check_artifact_symbols_resolve.py`. The latter passes today only because
  this directory is untracked; it begins asserting once staged.
- **Out of scope:** `dsp::CrossFeedPair`, `app/dsp/Reverb.hpp`,
  `app/dsp/StereoField.hpp`, `src/core/FroggersEngine.hpp`. The two untracked
  sibling changes are owned by other sessions.
- **Suite green excludes one PATH, never a count**: any
  `check-spec-checks-resolve` failure reported under
  the randomize-depth-reclaim change's own directory under openspec/changes.
- **Delivery is a push to `main`.** No pull requests. No AI attribution.
