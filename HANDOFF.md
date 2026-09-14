# Handoff — `frogg3rs-delay-width-wysiwyg-repair`

Start by reading `omni-rule.md`, then
`openspec/changes/frogg3rs-delay-width-wysiwyg-repair/proposal.md` and
`tasks.md`. Everything below is in those two files; this page tells you where to
put your first hour so you do not spend it the way earlier sessions did.

## The one thing that matters

**The defect is diagnosed and nobody has landed the fix.** Several sessions
reached it, wrote it up, and handed it forward. The parallel defect in the
reverb tank was repaired in the same lineage while this one was passed on again.
Deferral is the failure mode here, not any rejected repair.

An operator ruling already settles *whether* it is fixed — `04efe9c`,
2026-08-29: *"the signal SHALL NOT be collapsed to mono in the middle of the
chain. Folding belongs at the output."* It does not settle *how*.

## What three measurement rounds already settled — do not re-run them

The proposal's "READ THIS FIRST" carries six numbered Findings with the numbers.
In short:

1. The cross-feed cannot widen **given the symmetric input feed** that
   `Process` uses today. Every weight in [0, 1] re-correlates the pair; the
   qualifier is load-bearing.
2. At Feedback 0 — Feedback's own registered default — `fbEff` is exactly zero,
   so nothing in the feedback path reaches the output at all.
3. Feeding the two lines asymmetrically **pans** rather than widens. Two such
   laws were measured and both reach exact silence in the right channel at the
   top of the travel. Both are disqualified.
4. Side/mid energy ratio is `sqrt((1-rho)/(1+rho))` at equal level, so it is not
   a second opinion on correlation — and with unequal levels it drifts toward
   1.0, which makes it read panning as width. It is not this change's
   instrument.
5. **The probe stimulus was the broken instrument.** LCG white noise has an
   autocorrelation width of about one sample, so any read offset past a few
   samples decorrelates it completely and correlation sits on its floor for the
   rest of the knob. Every confusing row in this lineage has that shape.
6. Both channels share one LFO phase, which is the one per-channel difference
   over time the page has never used.

## The open question is answered; 1.1a/1.1b are what is left

Not "can the cross-feed widen this signal" — Finding 1 answered that. The
real open question was Finding 5's: under a stimulus whose autocorrelation
width is comparable to the travel's read-time offset, does the read-time
offset ALONE widen monotonically once the cross-feed is retired? **That
question is answered, not open.** `research/instrument-derivation.md`
measured it across the full grid `w ∈ {0.0,0.25,0.5,0.75,1.0}` at Feedback
`∈ {0.0,0.35,0.7}`, under the band-limited stimulus this change derived:
"the read-time offset alone produces a wet-pair correlation that falls
strictly monotonically point-to-point across Stereo width's whole travel at
every measured Feedback row (0.0, 0.35, 0.7) ... the level-balance figure
stays under 5% throughout, so this is not panning in disguise." An
independent adversarial pass in the same file (seed variation, a 41-point
grid, a cutoff sweep, a sample-rate sweep, all at the pinned `dtim = 0.3`)
could not break it. The mechanism is settled as `cross = 0.0f`; Finding 6's
anti-phase hypothesis is not needed.

What is not yet done is mechanical, not open: task 1.1 was split into
**1.1a** (MEASURE — run the pinned instrument at Feedback 0.7 across the
width grid and set the landed check's two bounds) and **1.1b** (IMPLEMENT —
land `cross = 0.0f` and the check itself). Both are unstarted in `tasks.md`;
read them there, and `proposal.md`'s "The check" section, rather than
re-deriving anything here.

## The instrument, fixed once so no task re-invents it

Band-limited noise, its bandwidth named and chosen so the autocorrelation width
is the same order as `widthSpread`'s maximum at the check's `dtim`. Pearson
correlation of the wet pair as the statistic, because it is level-invariant and
a law therefore cannot inflate it by panning. **A channel level-balance
assertion beside every image assertion**, because Finding 3 shows panning is the
failure mode that otherwise passes. Side/mid is never used. The proposal states
this in full under "The instrument this change uses".

## Do not re-derive these

1. `dsp::CrossFeedPair` is **shared** with `dsp::Reverb::Process`. Never edit
   the helper. `app/dsp/Reverb.hpp` and `app/dsp/StereoField.hpp` are out of
   scope; a repair needing either supersedes the proposal rather than widening
   it.
2. `stereo_delay_cross_feed_reproduces_its_captured_output_exactly` is a
   **self-capture** certifying that a de-duplication refactor changed nothing.
   Recapture it, and say in its own comment that the capture point moved and
   why. Silently recapturing it is what an auditor will correctly reject.
3. **No firmware parity exists.** The Daisy firmware never shipped a delay;
   `git show f236915^:sim/StereoDelay.hpp` carries `const float cross = p.dwid *
   0.5f;` verbatim. The defect is original to this project.
4. At `dwid == 0`, `cross = 0.0f` — the repaired law — is bit-exact by
   construction (the zero multiply in `CrossFeedPair`). **No other weight is
   safe there.** This list previously claimed the cross-feed was a no-op at
   any weight at `dwid == 0`, "confirmed bitwise." That was false. The
   proposal's own correction (`proposal.md`, "A correction to the record"):
   "Measured at `dwid = 0` with Feedback 0.35 or 0.70 (so the residual
   recirculates and compounds): weights 0.15, 0.35 and 0.40 diverge from
   bit-exact output after roughly 950-1035 samples; weights 0.05, 0.10, 0.20,
   0.25, 0.30, 0.45 and 0.50 hold. Only `K = 0` is exact by construction. At
   Feedback 0 nothing recirculates, so no weight fails there." The repaired
   law costs nothing at the registered default because the weight is exactly
   zero, not because any weight agreeing with today's law would have.
5. **The pins on `Process`'s output are enumerated by criterion, not by count.**
   Three drafts of the proposal stated a count and all three were wrong. The
   criterion and the found list are in constraint map item 6. FOUR passes are
   needed: literal `dwid` assignments, non-literal ones, the production router's
   `FroggersBankId::Delay, 4`, and `dsp::MapRowsToDelayParams(` call sites that
   set the rows as named constructor arguments. The enumeration is not the
   deliverable — a before/after diff of the whole suite's pass set is, because
   it finds a pin whatever spelling assigns its width.
6. **Width balance is the parent defect.** Designed as complementary weights,
   shipped as a common scalar. Its design document is
   `openspec/changes/archive/2026-08-18-frogg3rs-post-expansion-consolidation/research/RESEARCH2-drive-delay.md`,
   section "Width Balance — `WBal` — NEW" — and Finding 1 refutes that
   document's own premise that the cross-feed is a widening.

## The requirement over-reaches in two places, and this change does not edit it

The promoted scenario demands both pages' Stereo width "perform the same job"
and widen "across its whole travel". Reverb's mechanism was chosen to un-break a
cancellation rather than on design grounds, and Finding 2 shows no feedback-path
mechanism acts at Feedback's registered default. **Editing a requirement to
match an implementation is forbidden**, so both over-reaches go to the operator
as task 3.5 and an executor may not resolve either.

## What will actually get you rejected

Every rejection this change has drawn hit **prose** — a figure, a bound, an
enumeration's completeness, a stale cross-reference. Three preflights rejected
in a row, and the third was rejecting the repairs the second asked for. None
rejected DSP code, though no code has been written yet, so read that as a fact
about this change rather than about the lineage. Route your own write-up through
a fresh context exactly as you route an executor's, and re-run preflight after
every rewrite: an audit that rewrites a proposal has produced an unaudited
proposal, and the auditor that found a defect may not certify its own repair.

Specific traps, all cheap to avoid:

- `make test` halts at a pre-existing gate failure **before building anything**.
  Use `make -k`, then run every test binary by path.
- `app/Makefile`'s `BUILD_DIR` is an **absolute** path, so `make build/<binary>`
  has no rule and make exits **0** saying "up to date". A failed rebuild hides
  completely. Build the absolute-path target or `rm` the binary first.
- Cap builds at `nice -n 10 make -j2`; this Mac freezes above that.
- The gate exclusion is **path-scoped, not a count** — see `tasks.md`. Any
  `check-spec-checks-resolve` failure whose reported path lies under
  `openspec/changes/frogg3rs-randomize-depth-reclaim/` is not yours, whatever
  its count or name; any failure outside that path is. That directory belongs to
  another session editing it continuously: within one session its failure count
  went 2, then 3, then 0, and the test names changed too. Re-measure at every
  gate; never trust a recorded number, including this sentence's.
- Never `git add -A`, `git add .`, or `git commit -a` — two untracked change
  directories belong to other sessions. And `MANUAL.md` / `QUICK_DICT.md` are
  shared with the held `frogg3rs-midi-controller-resilience` change in this same
  tree, so read every hunk of `git diff HEAD --` on those two files before
  staging them, and stage with `git add -p` (task 2.4).
- A check that passes **more comfortably when the thing it measures is dead** is
  worse than no check. This change caught one in its own proposed instrument
  before landing it — see Finding 4.

## State of the tree

Baseline is **394 passing, 0 failing**, confirmed by running all twelve binaries
and counting `[PASS]`/`[FAIL]` lines. Clean apart from this change's own
modified `proposal.md`/`tasks.md` and two untracked directories owned by other
sessions (`frogg3rs-midi-controller-resilience`, `frogg3rs-randomize-depth-reclaim`).
Leave both alone.

Preflight has converged: twelve rounds are recorded in this change's own
directory (`preflight-1.md` through `preflight-12.md`). The last two are a terminating
executability check on tasks 1.1a/1.1b specifically — preflight-11 found a
real blocker (Send left unnamed, defaulting to its closed registered value
and killing every grid point), the documents were fixed, and preflight-12
confirms 1.1a and 1.1b execute exactly as written, inventing no value.
Preflight-12 also records two residual notes that do not block 1.1a/1.1b: a
NaN/non-finite liveness-gate gap (bears on task A.3) and a stale defect-table
figure (bears on task 3.6).

Stage 1 has not started: no line of `app/dsp/Delay.hpp` or
`app/FroggersDspParityTests.cpp` is modified, and 1.1b's own two check bounds
are still literal `<...>` placeholders in `tasks.md`. Stages 2 and 3 depend
on it.
