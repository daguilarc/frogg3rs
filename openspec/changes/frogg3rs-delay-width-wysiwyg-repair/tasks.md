# Tasks — Repair the Delay page's Stereo width

**Work not listed here does not become a task unless the operator adds it.**

## Mechanics

**Every stage ends in one commit and a push, after that stage's postflight —
not every task.** Every commit stages only the paths it names.
`git add -A`, `git add .` and `git commit -a` are forbidden:
`openspec/changes/frogg3rs-midi-controller-resilience/` and
`openspec/changes/frogg3rs-randomize-depth-reclaim/` are untracked in this
tree, owned by other sessions, and there is no pull request to catch a
blanket stage sweeping them in. Check `git status --short` before every
commit and confirm both stay untracked afterward. No AI attribution in any
commit message. Delivery is a push to `main`; never a pull request.

**"Suite green" excludes one path, not a fixed count or set of names:** any
`check-spec-checks-resolve` failure whose reported path lies under
`openspec/changes/frogg3rs-randomize-depth-reclaim/` is not this change's and
does not stop the stage, whatever its count or the names involved — that
session's own `Check:` lines have moved between at least three different
counts within one session (2, then 3, then 0, measured today), so a recorded
count or name from an earlier measurement is not evidence about the next
one. Re-measure at every gate rather than trusting a prior count: run the
gate, read which failures (if any) report a path under that one directory,
and report those separately from the rest. A failure whose reported path
lies outside `openspec/changes/frogg3rs-randomize-depth-reclaim/` — including
one under `openspec/changes/frogg3rs-midi-controller-resilience/`, which
produces none today — stops the stage; a future failure there is a new fact,
not something this carve-out already covers.

**Build mechanics.** `make test` halts at the exclusion above before building
anything — use `make -k`, then run every one of the twelve test binaries by
path. `app/Makefile`'s `BUILD_DIR` is an absolute path, so
`make build/<binary>` exits 0 saying "up to date" without rebuilding anything
that already exists — `rm` the binary first, or build the absolute-path
target. Cap builds at `nice -n 10 make -j2`. Baseline before this change:
394 passing, 0 failing, all twelve binaries.

**No task hands an executor a check without its assertion already written.**
Every figure a check needs comes from `proposal.md`'s "The check" section or
from that task's own MEASURE step — never invented to fill a blank. An
executor that meets a conflict between this change and what it finds reports
and stops; it does not adjust a threshold, weaken an assertion, or edit a
requirement to match code.

## Stage 1 — the repair

- [ ] 1.1a MEASURE the repaired law and set the check's two bounds. No
      `todayCorr` is computed here or anywhere below: once `cross` is fixed
      at `0.0f`, nothing in production computes today's shipped law any
      more, and the reverb precedent's shared-tap shape does not transfer
      (`proposal.md`'s "The check" — Delay's `cross` feeds back into
      `lineL`/`lineR`, unlike reverb's damping, which never writes back into
      `lineA`/`lineB`). Apply `cross = 0.0f` in `dsp::StereoDelay::Process`
      (`app/dsp/Delay.hpp:720`) locally, uncommitted, and run the pinned
      instrument (band-limited noise at 50 Hz over the seed-20260913 LCG
      burst, `dtim = 0.3`, 12000-sample warmup, 12000-sample measure, tap
      point `Process`'s returned `DelayWetPair`) at Feedback 0.7 across the
      Stereo width grid `{0.0, 0.25, 0.5, 0.75, 1.0}` — the same five-point
      grid the spec delta's own prior Delay measurement used
      (`specs/froggers-sheaf-parameter-model/spec.md`'s stereo-image
      scenario `Check:`). Record, per grid point: `|corr|` (via
      `Correlation`, liveness-gated on that row's `rmsL`/`rmsR` nonzero) and
      the level-balance ratio `|rmsL - rmsR| / (rmsL + rmsR)`. Width 0 is
      measured and recorded like every other point, but is EXCLUDED from the
      max-taking below: at width 0 the two taps are the same read and
      `|corr|` is a genuine 1.0, not `Correlation::Value()`'s
      zero-denominator fallback reading a dead line as correlated
      (`app/FroggersDspParityTests.cpp:9500`) — folding a real 1.0 into
      "measured maxima across the grid" forces the correlation bound to at
      least 1.0, which no grid point can then exceed, making the sanity
      check below unsatisfiable by construction. From the remaining rows
      (width > 0), set two numbers with margin above their respective
      measured maxima across the grid: the correlation bound and the
      level-balance bound. Also record, across those same rows, whether
      `|corr|` is strictly lower at each successive width than at the one
      before it (row-to-row ordering, not a bound) — the data is already
      gathered, so this costs nothing further; tasks 1.1b and 3.1 use the
      answer. Sanity-check the correlation bound before handing it forward:
      rerun the identical instrument with `cross` restored to
      `p.dwid * 0.5f * widthBalance` and confirm at least one grid point
      (width > 0) exceeds it — a bound that does not discriminate here is
      the same defect as a margin that cannot fail, in a new shape. Write
      both numbers, and the monotonicity finding, into task 1.1b below
      before it is dispatched; leave no blank for that task to fill.
- [ ] 1.1b IMPLEMENT the repair and its check, using 1.1a's numbers as
      written into this task — invent neither bound. If this task reaches an
      executor with either bound below (`<1.1a's correlation bound>` or
      `<1.1a's level-balance bound>`) still a literal, unfilled placeholder,
      that executor stops and reports rather than picking a number to fill
      it: 1.1a's own measurement is the only legitimate source for both, and
      a filled blank that did not come from 1.1a is the same defect as an
      invented threshold, in a new shape.
      **Repair:** `cross = 0.0f` in `dsp::StereoDelay::Process`
      (`app/dsp/Delay.hpp:720`), decoupled from `p.dwid` and `widthBalance`.
      Do not edit `dsp::CrossFeedPair`, `app/dsp/Reverb.hpp`, or
      `app/dsp/StereoField.hpp` — the first is shared with the reverb tank,
      the other two are out of scope; a repair needing either supersedes
      this proposal, report and stop.
      **Check:** land it in `app/FroggersDspParityTests.cpp`, per
      `proposal.md`'s "The check". Measure `repairedCorr` (`cross = 0.0f`)
      at Feedback 0.7, across the same Stereo width grid (width > 0 only,
      per 1.1a), using the pinned instrument. Assert
      `repairedCorr < <1.1a's correlation bound>` at every grid point.
      Assert the level-balance companion
      (`|rmsL - rmsR| / (rmsL + rmsR) < <1.1a's level-balance bound>`) and
      liveness (`rmsL`, `rmsR` nonzero, for this one correlation) on every
      row. If 1.1a found `|corr|` strictly falling row-to-row across the
      grid, assert that same ordering in this case too; if it did not hold,
      assert nothing about ordering here and leave the scenario clause to
      task 3.1. Prove each assertion can fail, on the landed check: revert
      the repair locally, rerun the identical check, and confirm the
      correlation assertion goes red. For the level-balance assertion, do
      not reuse that same break to test it: `Correlation::Value()` returns a
      hardcoded `1.0` on a zero denominator
      (`app/FroggersDspParityTests.cpp:9500`), so an adversarial law built as
      exact silence in one channel (a pan law) trips the CORRELATION
      assertion first, and `REQUIRE_TRUE` throws on its first failure
      (`app/FroggersDspParityTests.cpp:57`), so the level-balance assertion
      never runs — crediting it as "caught" would be crediting an assertion
      that never executed. Construct instead a pure nonzero per-channel gain
      scale applied to the already-decorrelated (repaired) pair at the tap
      point (`dL' = dL * gL`, `dR' = dR * gR`, `gL != gR`, both nonzero,
      before the statistics are computed). Pearson correlation is
      scale-invariant per channel, so this construction leaves
      `repairedCorr` — and the correlation assertion — untouched while
      moving `|rmsL - rmsR| / (rmsL + rmsR)`, which is what isolates the
      level-balance assertion as the one doing the catching. Report the red
      output for both breaks, then restore the repair.
- [ ] 1.2 Rule on Width balance, per proposal's "The parent defect" section.
      After 1.1b, `widthBalance` scales only `widthSpread`, the same term
      Stereo width itself already scales. Decide by reading whether that
      duplication is acceptable or is a second WYSIWYG defect in a shipping
      control; if the latter, report it with measurements rather than
      handing it forward. State which `widthBalance` value reproduces
      today's shipped voice under whichever law lands.
- [ ] 1.3 MEASURE and report the `ReadAt` capacity-overflow fix, per
      proposal's "A second defect in scope" section. Compare clamping the
      requested delay against `capacity` inside `ReadAt`
      (`app/dsp/Delay.hpp:949`) against bounding `widthSpread` so `timeR`
      never exceeds `capacity` at any `dtim`/`dwid` combination. Land
      whichever the measurement supports; if they sound the same, either is
      fine. If they differ audibly, spec conformance breaks the tie toward
      bounding `widthSpread`: the promoted scenario's clause reads "the
      time-offset spread ... never lengthens a read tap beyond the delay
      buffer's own capacity" (`specs/froggers-sheaf-parameter-model/spec.md`,
      Delay bank scenario, slot 12) — its grammatical subject is the spread
      itself. Bounding `widthSpread` makes that sentence true; clamping
      inside `ReadAt` leaves `widthSpread` itself computing an out-of-range
      value (the read stays safe, but the spread does not), leaving the
      sentence false. Report the audible difference measured either way;
      do not pick the clamp on preference alone if the two differ.
- [ ] 1.4 Enumerate every pin on `Process`'s output, then land the drift
      check. Run all four operand passes proposal's "Pins the repair will
      move" names: literal `dwid` assignments, non-literal ones, the
      production router's `FroggersBankId::Delay, 4`, and
      `dsp::MapRowsToDelayParams(` call sites with the row as a named
      constructor argument. Then run the second criterion that same section
      names: every `TEST_CASE` that reimplements one of `Process`'s formulas
      locally instead of calling `Process`. That second pass is what finds
      `stereo_delay_width_balance_mapping_keeps_cross_in_0_1_and_spread_at_or_below_todays_max`
      (`app/FroggersDspParityTests.cpp:6935`): it never calls `Process`, so
      the first criterion's four operand passes cannot see it, and it
      recomputes `cross` locally rather than reading production's, so it
      will not show up as a `[PASS]`-to-`[FAIL]` flip in the before/after
      diff below either — its own state never changes, because nothing
      about what it locally measures changed. Retire or rewrite its bound
      (a) (on the locally-recomputed `cross`) against the fixed `0.0f`
      constant; keep its bound (b) (on `spread`), which stays meaningful.
      Recapture `stereo_delay_cross_feed_reproduces_its_captured_output_exactly`
      (`app/FroggersDspParityTests.cpp:6978`) and record in its own comment
      that the capture point moved and why. Confirm
      `stereo_delay_freeze_at_default_reproduces_pinned_original_output_through_real_process`
      (`:7815`) moves as expected and recapture it too if it does not match.
      Then record the full `[PASS]`/`[FAIL]` set of all twelve binaries
      before this stage's changes and again after, and diff them. Every test
      whose state changed must already be on an expected list with a stated
      reason; one that changed and is not on the list stops the stage and is
      reported, not explained away.
- [ ] 1.5 Amend three stale comments in `app/dsp/Delay.hpp`, all made false
      by the repair.
      **The provenance header** (`Delay.hpp:1-59`, the file's opening
      comment) says nothing on this page is newly authored, and the repair
      and (if 1.3 changes code) the `ReadAt` fix make that false. Record
      each the same way the header already records its one prior deliberate
      divergence (`:28`, "REMOVED since") — naming what changed and why the
      ported behavior was wrong.
      **`SetWidthBalance`'s doc comment** (`Delay.hpp:617-634`) derives bound
      (a) (cross-feed stays in `[0,1]`) from "both new weights in
      `Process()` are `<literal> * widthBalance * p.dwid`" — true only of
      today's shipped `cross`. Once `cross` is a fixed `0.0f`, only
      `widthSpread` still has that shape; rewrite the comment so bound (a)
      is stated correctly for the repaired code (`cross` is a constant, not
      a product anything can bound), and bound (b) continues to read as it
      does today.
      **The comment immediately preceding the `cross` assignment**
      (`Delay.hpp:714-719`, ending at `const float cross = ...` on `:720`)
      describes `0.5f` as "half of a width blend that widthBalance then
      scales further" — also false once `cross` is a fixed `0.0f` scaled by
      nothing. Rewrite or remove it to describe the repaired assignment.
- [ ] 1.5a Correct the wet-limiter design comment and determine whether
      independent per-channel limiting produces the image shift it warns
      against. `app/dsp/Delay.hpp:271-272` declares `wetLimiterL`/
      `wetLimiterR`, in the file this change already edits, so this is in
      scope under §8.0. The comment immediately above those two fields
      gives two reasons for per-channel rather than linked limiters. The
      first (a linked, max-driven limiter would duck both channels whenever
      either alone crossed threshold) is independent of this repair and
      stays as written. The second — "`dwid`'s cross-feed already keeps L
      and R close to each other in practice, so the two channels rarely
      diverge enough for one limiter engaging alone to read as an image
      shift" — is killed by this repair: `cross` becomes a fixed `0.0f`, so
      the mechanism the comment credits with keeping the channels close no
      longer does, and after the repair `widthSpread` driving the two
      channels apart is the whole point.
      (a) Rewrite the comment: remove or replace the killed second reason so
      it no longer credits cross-feed with keeping the channels close; keep
      the first (linked-limiter-ducking) reason as written, since nothing in
      this repair touches it.
      (b) MEASURE whether independent per-channel limiting on the
      now-decorrelated pair produces the image shift the killed reason was
      guarding against, and report the result — "measured, and it does",
      "measured, and it does not", and "measured, unreachable in practice,
      here is why" are all legitimate outcomes; do not decide the outcome in
      advance, and do not carry forward a number from this proposal as a
      premise. This task's own first step establishes the numbers it needs:
      read `kDelayWetLimiterThreshold`'s current value from source
      (`Delay.hpp:122`), and separately measure task 1.1a/1.1b's own check
      instrument's actual per-channel RMS at its pinned operating point
      (band-limited noise, `dtim = 0.3`, Feedback 0.7) — do not assume
      either number; both are cheap to read or compute directly. If that
      measured RMS sits at or below the limiter's threshold, the existing
      check cannot exercise this risk by construction, and this task needs
      its own operating point (raised feedback, hot input, enough headroom
      to drive at least one channel's limiter into engagement) and its own
      statistic — a transient, per-channel divergence measure, since the
      12000-sample steady-state aggregate `proposal.md`'s "The check" uses
      is built to average over a transient, not resolve one. Determine both
      and report what you chose and why, alongside the measurement and its
      outcome.
- [ ] 1.6 Stage gate: suite green with the path-scoped exclusion
      (re-measured this gate — any `check-spec-checks-resolve` failure whose
      reported path lies under `openspec/changes/frogg3rs-randomize-depth-reclaim/`), one
      postflight in a fresh context over the whole stage, then one commit
      and a push.

## Stage 2 — the documents the repair makes stale

- [ ] 2.1 `MANUAL.md` and `QUICK_DICT.md`'s Delay **Stereo width** entries.
      They currently say the cross-feed rides the feedback path and reaches
      the repeats once Feedback or Freeze leaves 0. Rewrite from what ships
      after stage 1: the offset is the only widening mechanism, and the
      cross-feed is fixed at zero regardless of Feedback or Freeze.
- [ ] 2.2 `MANUAL.md` and `QUICK_DICT.md`'s Delay **Width balance** entries,
      per 1.2's ruling. `MANUAL.md` currently calls it "an overall scalar on
      how strongly Stereo width's cross-feed and time-spread apply," which
      is false once cross-feed is fixed. House style: plain present tense
      saying what the control does; no jokes, no defining by negation, no
      history the reader never saw, no task numbers or change-name
      references. `app/check_docs_match_parameter_table.py` binds every bold
      entry to the parameter table by name, label and slot; it reports 84
      entries and 0 failures per document today — confirm it still does.
- [ ] 2.3 `frogg3rs.code-workspace`'s two stale `files.watcherExclude`
      entries, `**/wasm/build/**` and `**/desktop/build/**`. Neither
      `wasm/` nor `desktop/` exists at the repository root. Its other two
      entries resolve and stay.
- [ ] 2.4 Before staging anything in this stage, check whether
      `openspec/changes/frogg3rs-midi-controller-resilience/` has begun
      editing `MANUAL.md` or `QUICK_DICT.md` in this same working tree — that
      held change edits different sections of the same two files. Run
      `git diff HEAD -- MANUAL.md QUICK_DICT.md` (a bare `git diff` compares
      against the index and misses what the other session already staged)
      and read every hunk before staging; stage only this change's own hunks
      with `git add -p`. Re-run the diff immediately before `git commit`. If
      another session's edits are present, stop and report rather than
      committing them.
- [ ] 2.5 Correct or retire `HANDOFF.md`'s "Do not re-derive these" item 4.
      It claims the cross-feed is "a no-op at any weight, confirmed
      bitwise" at `dwid == 0` — false in floating point, per this proposal's
      own "A correction to the record" section: weights 0.15, 0.35 and 0.40
      diverge from bit-exact output after roughly 950-1035 samples at
      Feedback 0.35/0.70, and only `K = 0` is exact by construction. Left as
      written, archival makes the error permanent and orphaned from the
      correction that moves into `archive/`. Edit only item 4 and, because
      this change's own renumbering makes it stale too, `HANDOFF.md`'s "The
      open question, which is task 1.1" section — repoint its "task 1.1"
      references (the heading and its body) to 1.1a/1.1b, or to whichever
      still applies if item 4's correction changes what remains open. The
      other five items in the list are not this task's concern.
- [ ] 2.6 Stage gate: suite green with the path-scoped exclusion
      (re-measured this gate — any `check-spec-checks-resolve` failure whose
      reported path lies under `openspec/changes/frogg3rs-randomize-depth-reclaim/`), one
      postflight in a fresh context over the whole stage, then one commit
      and a push.

## Stage 3 — the spec delta and archival

Most of this was done under the superseded change and is carried forward in
the delta already; verify rather than redo.

- [ ] 3.1 The stereo-image scenario has two problems for the Delay page, not
      one. First: its `Check:` currently records the Delay page as REFUTED,
      and its diagnosis text says the repair makes "the two mechanisms widen
      together." Repoint it onto the check task 1.1b lands (resolve the name
      by grepping for it), and correct the diagnosis: after the repair the
      page drives one mechanism (`widthSpread`), not two.
      Second, separately: the scenario's own THEN clause reads "L/R
      correlation of that stage's wet signal falls monotonically," and the
      check task 1.1b lands asserts an absolute per-grid-point bound, never
      a row-to-row ordering — a value staying below a ceiling at every point
      does not establish that the points fall in order. Do not narrow or
      reword the requirement to match the check; that is forbidden. Resolve
      it from what 1.1a already measured: if 1.1a found `|corr|` strictly
      falling row-to-row across the grid (width > 0) and 1.1b asserts that
      ordering in its own case, cite that same case for this clause too —
      one check backing both halves of the scenario. If 1.1a did not find a
      strictly falling row-to-row ordering, mark this clause plainly as not
      backed for the Delay page, the same way this SAME scenario's `Check:`
      text already marks Reverb's own second clause vacuous for Reverb
      ("Because Reverb's Stereo width drives only the one mechanism named
      above, the second clause is vacuous for this page" —
      `specs/froggers-sheaf-parameter-model/spec.md`'s "A stereo-image
      control widens across its whole travel" scenario) rather than silently
      dropping or rewording the requirement — recording that this change
      measured it and did not find it, since no other task in this change
      attempts a different mechanism for it.
- [ ] 3.1a Mark the slot-12 Width Balance clause not yet delivered. Both the
      promoted spec and this change's own delta describe slot 12 as "the
      ratio between the Width knob's time-offset spread and its cross-feed
      blend, independent of the Width knob's own value"
      (`specs/froggers-sheaf-parameter-model/spec.md`, Delay bank scenario).
      `widthSpread / cross` reduces algebraically to `0.7 * baseSeconds`
      (from `widthSpread = p.dwid * baseSeconds * 0.35f * widthBalance`,
      `Delay.hpp:679`, and `cross = p.dwid * 0.5f * widthBalance`,
      `Delay.hpp:720` — `p.dwid` and `widthBalance` cancel identically) —
      independent of `widthBalance` as well as of the Width knob, which
      makes the clause false of today's shipped code, before any repair in
      this change touches it. Editing a requirement to match an
      implementation is forbidden, so do not reword the clause into one
      that describes a ratio stripped of `widthBalance`. Instead, apply
      §9's own mechanism for a requirement not backed: mark the clause
      plainly as not yet delivered, and name this same change,
      `frogg3rs-delay-width-wysiwyg-repair`, only as the one that found and
      recorded the gap — not as the one that closes it, since no task here
      attempts a real ratio control. `proposal.md` states, in its own
      prose, that whether Width Balance should become a real ratio control
      is a separate change's decision.
- [ ] 3.2 Re-count every MODIFIED requirement against the promoted text — the
      restate gate does not compare requirement prose (its bullet collector
      keeps only dash-opening lines), so read the three requirements'
      paragraphs by hand.
- [ ] 3.3 Every scenario's `Check:` names a test that exists and passes, or
      is marked not yet delivered in the form the gate recognizes. Resolve
      every one by grepping. No `Check:` may point at this change's own
      `tasks.md` — archival moves that path and any such pointer dangles.
- [ ] 3.4 Confirm the two transitional NOTEs in the promoted spec name this
      change; they and the delta's declared edits move together.
- [ ] 3.5 Carry the requirement's two over-reaches to the operator, per
      proposal's closing section: the "same job across pages" sentence, and
      "across its whole travel" at a Feedback setting where no feedback-path
      mechanism can act. Neither is edited by this change. Record the
      operator's ruling, or that it is still open — an executor may not
      resolve it.
- [ ] 3.6 Populate `research/` from the measurement sessions and gate
      archival on it. `research/INDEX.md` names six files — one per topic —
      and none exist: its own header currently claims they were "extracted
      verbatim from the measurement sessions... by a pass separate from the
      one that wrote proposal.md," which is false about the tree as it
      stands. `app/check_citations_resolve.py` does not catch this — it only
      scans `path:line` citations in `app/` C++ comments, not this
      directory — and every figure this change needs is already inlined in
      `proposal.md`'s own prose, so no task is gated on the files existing
      except this one. Extract each file from the actual measurement
      sessions per `research/INDEX.md`'s own per-file description: the
      underlying sweeps, grids and sessions behind each conclusion
      `proposal.md` states, not a restatement of the conclusion alone. Do
      not archive this change (3.7) until all six exist and
      `research/INDEX.md`'s header accurately describes the tree rather
      than a future state.
- [ ] 3.7 Archive this change, promoting its delta.
- [ ] 3.8 Stage gate: suite green with the path-scoped exclusion
      (re-measured this gate — any `check-spec-checks-resolve` failure whose
      reported path lies under `openspec/changes/frogg3rs-randomize-depth-reclaim/`), one
      postflight in a fresh context, then one commit and a push.

## Audits

- [ ] A.1 Preflight, in a context that did not write this change, covering
      the artifacts and the code together. May reject. Re-run after any
      rewrite of `proposal.md` or `tasks.md`.
- [ ] A.2 Each stage gets one postflight in a fresh context before its
      commit — not one per task.
- [ ] A.3 The repair's check (task 1.1b) gets one adversarial pass from a
      context that did not build it: try to get a broken mechanism past it,
      including one that pans instead of widening — the specific evasion the
      level-balance companion exists to reject — and one that drives an
      intermediate non-finite value, to check whether the liveness gate as
      landed catches it. Report what gets through.
