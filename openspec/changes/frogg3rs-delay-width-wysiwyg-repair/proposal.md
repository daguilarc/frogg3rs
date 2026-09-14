# Proposal — Repair the Delay page's Stereo width

Supersedes `frogg3rs-density-documents-and-spec`, which superseded
`frogg3rs-stereo-field-and-density`, which superseded `frogg3rs-wysiwyg-deliver`,
`frogg3rs-wysiwyg-finish` and `frogg3rs-wysiwyg-controls`. That chain's other
work is committed and pushed; this change carries forward the one remaining
repair, the spec delta, and archival.

**Provenance.** Every measurement cited below — refuted mechanisms, the
instrument's derivation, retracted figures, prior-art citations — is recorded
in `research/INDEX.md`. Cited here one line at a time per its own topic; this
document does not re-narrate it.

## The defect

`dsp::StereoDelay::Process` (`app/dsp/Delay.hpp:720`) computes
`cross = p.dwid * 0.5f * widthBalance` and passes it to
`dsp::CrossFeedPair(dL, dR, cross)` (`app/dsp/StereoField.hpp:39`), whose
result is written into the recirculating feedback lines.
`CrossFeedPair(a, b, w)` returns `{a*(1-w)+b*w, b*(1-w)+a*w}`, which at
`w = 0.5` returns two identical values regardless of `a` and `b`. Raising
Stereo width therefore drives the feedback content toward mono at the top of
its travel: the control named for the stereo field drives a mechanism that
re-correlates it. Correlation figures across the travel:
`research/INDEX.md` → cross-feed-widening.

An operator ruling already in the tree settles that this is a defect to fix,
not a design to keep — `04efe9c`, 2026-08-29: "the signal SHALL NOT be
collapsed to mono in the middle of the chain. Folding belongs at the output."
Everything downstream of this call site was mono anyway before that ruling
shipped, which is why the collapse was inert until then.

## The repair

`cross` becomes a fixed `0.0f`, decoupled from `p.dwid` and from
`widthBalance`. `widthSpread` (`app/dsp/Delay.hpp:679`,
`p.dwid * baseSeconds * 0.35f * widthBalance`) is untouched and becomes the
sole mechanism Stereo width drives.

## Why zero, not a smaller fixed weight

A fixed nonzero coupling was investigated and rejected on three grounds:

- A periodicity argument for keeping some coupling rested on figures
  (1408/2552 samples vs. 980/980) since retracted as artifacts of a
  global-argmax measurement over a decayed comb tail — three reasonable
  window choices gave three different "winning" periods, all harmonics of one
  underlying period. `research/INDEX.md` → retired-mechanisms.
- A tonal-material argument rested on one measured point, `|corr| = 0.019` at
  220 Hz. A full frequency sweep shows that point sits in one narrow trough of
  a curve ranging 0.00032 to 0.99998 (mean 0.64); at a fixed weight of 0.125
  the same curve is worse than today's shipped law at 43-47% of frequencies.
  `research/INDEX.md` → retired-mechanisms.
- The remaining concern — that two fully independent delay lines sound wrong —
  is answered by prior art, not by measurement. Logic Pro's Stereo Delay sets
  Delay, Feedback and Mix separately per channel and exposes Crossfeed as its
  own control, decoupled from the channels at 0%. Bitwig's Delay exposes
  Width and Cross Feedback as independent controls. Valhalla Delay's
  ping-pong is a distinct routing mode alongside Digital/Analog/Tape/BBD, not
  a weight living inside a width control. Three shipped products treat
  crossfeed as its own control and never as a term inside width — exactly the
  defect this change repairs.
  `openspec/changes/archive/2026-08-18-frogg3rs-post-expansion-consolidation/research/RESEARCH2-drive-delay.md`
  already cites Bitwig for this. Read correctly it is precedent for keeping
  crossfeed and width separate, not for a ratio knob between them — which
  resolves the parent defect below rather than leaving it open.
  `research/INDEX.md` → prior-art-crossfeed-separation.

`cross = 0.0f` is also the only weight that is bit-exact at the registered
default `dwid = 0`; see the correction directly below.

## A correction to the record

An earlier draft of this change claimed the cross-feed is a no-op at
`dwid == 0` at any weight, reasoning that a mono, bit-identical input makes
`CrossFeedPair(x, x, K)` return `x` regardless of `K`. That reasoning is false
in floating point: `CrossFeedPair(x, x, K) = x*(1-K) + x*K` is two multiplies
and an add, and whether that lands exactly on `x` is a per-value accident, not
a guarantee. Measured at `dwid = 0` with Feedback 0.35 or 0.70 (so the
residual recirculates and compounds): weights 0.15, 0.35 and 0.40 diverge from
bit-exact output after roughly 950-1035 samples; weights 0.05, 0.10, 0.20,
0.25, 0.30, 0.45 and 0.50 hold. Only `K = 0` is exact by construction. At
Feedback 0 nothing recirculates, so no weight fails there.
`research/INDEX.md` → cross-feed-widening.

## The parent defect: Width balance

`SetWidthBalance` (`app/dsp/Delay.hpp:635`) is an identity map, and both
`widthSpread` and (today) `cross` are multiplied by the same `widthBalance`,
so the ratio between the two mechanisms it was specified to balance is fixed
at every setting — the one thing the knob exists to change
(`openspec/changes/archive/2026-08-18-frogg3rs-post-expansion-consolidation/research/RESEARCH2-drive-delay.md`,
"Width Balance"). After this repair, `cross` is a constant no longer scaled by
`widthBalance`, so `widthBalance` scales only `widthSpread` — the same knob
Stereo width itself already scales. Task 1.2 rules on whether that
duplication is acceptable or is a second WYSIWYG defect, and states which
`widthBalance` value reproduces today's shipped voice (the registered default
is 1.0; the design document's own complementary-weights premise assumed 0.5).

Separately: the promoted spec's slot-12 description calls Width Balance "the
ratio between the Width knob's time-offset spread and its cross-feed blend,
independent of the Width knob's own value." `widthSpread / cross` reduces to
`0.7 * baseSeconds` — independent of `widthBalance` too, so the clause is
false of today's shipped code regardless of anything this change repairs.
Task 3.1a marks it not yet delivered rather than rewording it to match the
code. Whether Width Balance should become an actual ratio control is a
separate change's decision; this one neither designs nor makes it.

## A second defect in scope: `ReadAt` has no capacity clamp

`dsp::StereoDelay::ReadAt` (`app/dsp/Delay.hpp:949`) computes
`floorPos % capacityI` with no check that the requested delay fits inside
`capacity` (`kMaxDelaySamples = 96000`, 2.0 s at 48 kHz, `Delay.hpp:250`). At
`dtim = 0.99`, `baseSeconds ≈ 1.8536 s` (`ExpMapCompute(0.001, 2.0, 0.99)`,
`Delay.hpp:669`), and `widthSpread` at full width and default Width balance
adds `≈0.6488 s` (`Delay.hpp:679`), so `timeR ≈ 2.5024 s` — past the 2.0 s
buffer. The modulo wraps that request to a short lag instead of clamping or
refusing it: the right channel reads roughly 0.5 s of delay when the knob
asks for about 2.5 s.

This is reachable from the shipping UI (`dtim` and `dwid` both travel to
1.0), it is in the file this change already edits, and after the repair
`widthSpread` is the only mechanism Stereo width drives — shipping a repair
that leaves the sole remaining mechanism broken over part of its travel is
worse than not repairing the cross-feed at all. Task 1.3 measures the fix
options (clamping the read against capacity vs. bounding `widthSpread` so
`timeR` always fits) and lands whichever the measurement supports; if they
differ audibly, spec conformance breaks the tie toward bounding
`widthSpread`, and task 1.3 states why. `research/INDEX.md` →
read-at-capacity.

## The check

Stated once here so no task reinvents it.

**A monotonicity assertion does not discriminate.** Under band-limited noise,
today's shipped law also produces a monotonically falling `|corr|` across
Stereo width's travel, because `widthSpread` grows alongside `cross` in
production. A check asserting only "correlation falls across the travel"
passes on the defect it exists to catch.

**A live two-regime comparison is not constructible here.** The reverb
precedent `reverb_damping_filter_split_lowers_wet_leg_correlation_at_every_setting`
(`app/FroggersDspParityTests.cpp:9531`) measures its "shared" and "split"
regimes from the same tank taps in one pass because `dsp::Reverb`'s damping
filter reads `valA`/`valB` for the output path only and never writes back
into `lineA`/`lineB` — both regimes are post-hoc views of one
regime-invariant recirculating stream, and the test reaches them by peeking
`Reverb`'s own `lineA`/`lineB`, public because the struct carries no access
specifier. Delay's `cross` is not a post-hoc view: it feeds
`fed = CrossFeedPair(dL, dR, cross)` (`Delay.hpp:726`) into `fbL`/`fbR`,
which are exactly what gets written into `lineL`/`lineR` at
`Delay.hpp:808-809` (both private, `Delay.hpp:924`, unreachable to a peeking
test). Today's law and the repaired law are two different feedback
trajectories from the first sample on, not two views of one trajectory — and
once the repair lands, nothing in production computes today's law at all, so
there is no live `todayCorr` a single `Process()` run could produce alongside
`repairedCorr`. The reverb precedent's shape does not transfer.

**What the check asserts instead.** At Feedback 0.7, full width, the
repaired law measures `|corr| = 0.240` where today's shipped law measures
`0.370` (`research/INDEX.md` → cross-feed-widening) — far enough apart that
an absolute bound between them exists. (This section's `0.370` is measured
under the band-limited stimulus pinned below; an earlier `0.119325` at the
same nominal setting was measured under a white-noise burst — different
stimulus, different correlation, which is the entire reason the instrument
section pins the stimulus.) The check asserts the repaired law's
`|corr|` stays under such a bound at every grid point, set by task 1.1a's own
measurement of the repaired law across the whole grid (margin above its
measured maximum), not by this one full-width point and not invented to fill
this section. This gives up the relative shape's self-documenting "strictly
more decorrelated than today" statement at runtime, since nothing at runtime
can compute today's side of it any more. What replaces that guarantee is
task 1.1b's proof, required independently of which shape is chosen: revert
the one-line repair locally, run the identical landed check against the
unrepaired code, confirm it goes red, and report that output — which is what
establishes the bound actually discriminates, in place of a runtime
comparison the repaired code no longer supports.

**The instrument, pinned so no task re-derives it:**
- Stimulus: band-limited noise — `dsp::OnePoleLowPass` at 50 Hz
  (`SetAlphaFromNatFreq(50.0f/48000.0f)`) over an LCG burst
  (`lcg = lcg*1664525 + 1013904223`, seed `20260913`), amplitude
  `0.5f*(uniform in [-1,1))`. Its autocorrelation e-folding width (152.8
  samples) is close to `widthSpread`'s maximum at `dtim = 0.3` (164.29
  samples), which is what gives the probe dynamic range across the whole
  travel instead of saturating at the first step. White noise saturates
  immediately and is not used. `research/INDEX.md` → instrument-derivation.
- Statistic: `|corr|`, via `struct Correlation` (`app/FroggersDspParityTests.cpp:9487`).
  `Correlation::Value()` returns a hardcoded `1.0` on a zero denominator, so a
  dead line reads as perfect correlation. Liveness (`rmsL`, `rmsR` nonzero)
  is asserted for the one correlation this check computes, at every grid
  point — a claim about that specific quantity, not a blanket "every row"
  framing that would leave a second, independently-dead signal uncovered if
  this check is ever extended to compute more than one.
- **A flat +1 is the VOID signature. A `nan` is NOT the same finding** and is
  not covered by `Correlation::Value()`'s guard, which catches a zero
  denominator only; a non-finite sample gives an infinite denominator,
  `Inf > 0.0` passes, and the division yields `nan` anyway. The liveness gate
  above is written as `rmsL`/`rmsR` nonzero — as `!= 0.0f`, that comparison is
  TRUE for a NaN under IEEE-754, so a non-finite instrument passes the very
  gate meant to catch a dead one. A task meeting a `nan` stops and reports it
  as unexplained.
- Operating point: `dtim = 0.3`, 12000-sample warmup discarded, 12000-sample
  measure. At `dtim >= 0.8` the window is noise-dominated and monotonicity
  breaks even at baseline; at 1000 total samples it breaks even at baseline
  too. `research/INDEX.md` → instrument-derivation.
- Tap point: `Process`'s returned `DelayWetPair`, post-Diffusion, post-wet-limiter.
- Companion assertion: channel level balance
  (`|rmsL - rmsR| / (rmsL + rmsR)`) stays under a bound set by task 1.1a's
  own measurement (margin above its measured maximum), on every asserted
  row, so a law that pans rather than widens cannot pass.
- Side/mid energy ratio is not used: at equal channel levels it equals
  `sqrt((1-rho)/(1+rho))` exactly, so it carries no information correlation
  does not, and with unequal levels it drifts toward 1.0 regardless of
  correlation, reading panning as width. `research/INDEX.md` → side-mid-rejection.
- Send (`p.dsnd`) is pinned at 1.0 (fully open) for every measurement in
  this document. Its registered default is 0.0, and that default is
  degenerate for this instrument: `dsp::StereoDelay::Process` opens with an
  early return whenever `p.dsnd <= 0.0001f` (`Delay.hpp:655`), which zeroes
  both channels before anything downstream computes, so at Send's default
  `rmsL == rmsR == 0` at every grid point and `Correlation::Value()`'s
  zero-denominator fallback reads every row as `|corr| = 1.0` — neither
  bound in this section is derivable there. Send is not swept; it is held
  at 1.0 throughout.
- Knobs off their registered defaults are otherwise named with their
  values; a knob sitting at its registered default needs no naming except
  where that default is degenerate for the measurement, which is exactly
  Send's case above. Stereo width at 0 never feeds a nonzero offset, so a
  correlation probe there reads a flat `+1` — void, not negative, and never
  asserted as a discriminating point.

## Pins the repair will move

No count is stated: three earlier drafts of this change each stated one and
each was wrong. The criterion has two parts, because one test evades a
criterion built only around `Process`: (1) every `TEST_CASE` that reaches
`StereoDelay::Process` with `p.dwid` nonzero and the feedback path live
(`dfbk` nonzero, `dfrz` nonzero, or `dfrzLatched`) is a candidate pin, found
by four operand passes — literal `dwid` assignments, non-literal ones, the
production router's `FroggersBankId::Delay, 4`, and
`dsp::MapRowsToDelayParams(` call sites that set the row as a named
constructor argument with no `dwid` token present anywhere; and (2) every
`TEST_CASE` that reimplements one of `Process`'s formulas locally, instead of
calling `Process`, is also a candidate pin regardless of whether it reaches
`Process` at all. Three are already known to move because they pin literal
output or a stale formula:

- `stereo_delay_cross_feed_reproduces_its_captured_output_exactly`
  (`app/FroggersDspParityTests.cpp:6978`) — a self-capture certifying a past
  de-duplication refactor changed nothing, not a parity pin against any
  external source. Recapture it and record in the test's own comment that the
  capture point moved and why.
- `stereo_delay_freeze_at_default_reproduces_pinned_original_output_through_real_process`
  (`app/FroggersDspParityTests.cpp:7815`) — runs `width=0.25` at
  `feedback=0.6` and pins literal output.
- `stereo_delay_width_balance_mapping_keeps_cross_in_0_1_and_spread_at_or_below_todays_max`
  (`app/FroggersDspParityTests.cpp:6935`) — never calls `Process`; it
  recomputes `cross = dwid * 0.5f * delay.widthBalance` locally and checks it
  against bound (a). It evades criterion (1) by construction — no `Process`
  call, so no operand pass finds it — and is exactly why criterion (2)
  exists. Its bound (b), on `spread`, stays meaningful after the repair. Its
  bound (a), on the locally-recomputed `cross`, does not: production `cross`
  is a fixed `0.0f` after task 1.1b, so bound (a) checks a formula nothing
  in production computes any more, and because the test never calls
  `Process`, its own `[PASS]`/`[FAIL]` state never changes either — it is
  invisible to both the enumeration and the before/after diff under
  criterion (1) alone. This test is the positive control proving criterion
  (2) catches something criterion (1) misses; task 1.4 retires or rewrites
  its bound (a) against the fixed constant and keeps bound (b).

Task 1.4's deliverable is the full `[PASS]`/`[FAIL]` set of all twelve test
binaries, taken before the repair and after, diffed. Every test whose state
changes must already be on an expected list with a stated reason; one that
changes and is not on the list stops the stage.

## Impact

- **Affected spec:** `froggers-sheaf-parameter-model`, carried forward with
  three MODIFIED requirements. Its stereo-image scenario is currently
  REFUTED for the Delay page and its recorded diagnosis text says the repair
  makes "the two mechanisms widen together" — false after this repair, since
  the page drives one mechanism (`widthSpread`), not two. Task 3.1 rewrites it.
- **Directories swept:** `openspec/specs/`, `openspec/changes/` (this change's
  own directory only — the two untracked sibling changes below are excluded),
  the repository root's documents, and `app/`.
- **Code this change edits:** `app/dsp/Delay.hpp` (the repair, the `ReadAt`
  fix from task 1.3, the provenance header, `SetWidthBalance`'s doc comment
  and the comment preceding the `cross` assignment from task 1.5, and the
  wet-limiter design comment from task 1.5a), `app/FroggersDspParityTests.cpp`
  (the new check and the recaptured golden vector), `MANUAL.md` and
  `QUICK_DICT.md` (the Stereo width and Width balance entries),
  `frogg3rs.code-workspace` (two stale `files.watcherExclude` entries —
  `**/wasm/build/**` and `**/desktop/build/**` — found by this change's own
  hygiene sweep on a tracked file in a swept tree; neither `wasm/` nor
  `desktop/` exists at the repository root), `HANDOFF.md` (its "Do not
  re-derive these" item 4 claims the cross-feed is a no-op at any weight at
  `dwid == 0`, confirmed bitwise — false per this proposal's own correction
  above; task 2.5 corrects or retires it), and the six `research/*.md` files
  `research/INDEX.md` names, none of which exist yet (task 3.6 populates
  them from the measurement sessions, gating archival).
- **Affected gates:** `app/check_docs_match_parameter_table.py`,
  `app/check_spec_checks_resolve.py`,
  `app/check_modified_requirements_restate_promoted.py` (this gate does not
  compare requirement prose — its bullet collector keeps only dash-opening
  lines — so task 3.2 reads the three MODIFIED requirements by hand).
- **Out of scope:** `src/core/FroggersEngine.hpp` is frozen firmware.
  `app/dsp/Reverb.hpp` and `app/dsp/StereoField.hpp` are not edited by this
  change (`dsp::CrossFeedPair` is shared with the reverb tank); a repair
  requiring either supersedes this proposal rather than widening it silently.
  `openspec/changes/frogg3rs-midi-controller-resilience/` and
  `openspec/changes/frogg3rs-randomize-depth-reclaim/` are untracked, held,
  and owned by other sessions; no blanket stage may sweep either in, and this
  change may not edit either.
- **A concurrent session is writing in this tree.**
  `openspec/changes/frogg3rs-randomize-depth-reclaim/` is owned by another
  session, and `check-spec-checks-resolve` may or may not be red for a cause
  under that path at any given moment — measured at the time of this writing
  it is not (every scenario there reads "not yet delivered" and the gate
  reports zero failures overall), but that session's own `Check:` lines have
  moved between at least three different counts within one session (2, then
  3, then 0), so this proposal states a rule rather than a number. Every
  stage gate re-measures and reports the suite as green apart from any
  `check-spec-checks-resolve` failure whose reported path lies under that
  one directory, whatever its count or names. A failure whose reported path
  lies outside it — including under
  `openspec/changes/frogg3rs-midi-controller-resilience/`, which produces
  none today — stops the stage.
- **Delivery is a push to `main`.** This repository does not use pull
  requests. No AI attribution appears in any commit message.

## The two requirement over-reaches this change does not resolve

The promoted stereo-image scenario reads:

> **WHEN** either page's Stereo width is swept from floor to top
> **THEN** L/R correlation of that stage's wet signal falls monotonically
> **AND** where the control drives more than one stereo mechanism, every
> mechanism it drives widens in the same direction across that travel

under a requirement whose first sentence is "Where two pages carry controls
with the same name, those controls SHALL perform the same job." Two parts of
that are over-reach, not edited by this change, and are carried to the
operator by task 3.5 rather than resolved by an executor:

1. **"The same job" across pages.** Reverb's Stereo width is a mid/side
   output scaling; a delay fed from mono has no side component for that
   mechanism to scale until something else creates one. No design document
   or external precedent is cited anywhere for requiring one shared mechanism
   across both pages.
2. **"Across its whole travel" at every Feedback setting.** No feedback-path
   mechanism can act at Feedback 0, which is Feedback's registered default,
   so a scenario clause requiring a feedback-path mechanism to act there
   cannot be satisfied by any repair confined to the feedback path.

Editing a requirement to match an implementation is forbidden; both are
recorded here so the operator rules on them.
