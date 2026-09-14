# Proposal — Repair the Delay page's Stereo width, and close the spec delta

**Created 2026-09-13.** Supersedes `frogg3rs-density-documents-and-spec`, which
superseded `frogg3rs-stereo-field-and-density`, which superseded
`frogg3rs-wysiwyg-deliver`, `frogg3rs-wysiwyg-finish` and
`frogg3rs-wysiwyg-controls`.

That chain's delivered work is committed and pushed. This change carries forward
one repair, the spec delta, and the archival.

## READ THIS FIRST: why five sessions reached this point and none finished

**Nobody has ever attempted this fix.** That is the finding, and it is not the
same as "the fix keeps failing." Traced through the history:

- `af643e3` / `43856d1` (2026-08-29) first found it: "Delay's Stereo width
  survives only through the feedback loop... A control named for an image the
  signal path cannot carry." Disposition: "Recorded rather than fixed."
- The parallel REVERB-side defect — one shared damping filter holding both tank
  lines together — was diagnosed and fixed in the same lineage at `e9120f8`.
  The Delay-side twin was handed forward again.
- `frogg3rs-density-documents-and-spec` re-measured it, root-caused it, wrote it
  up, and handed it forward a further time.

Every session that reached this defect recorded it and passed it on. The
handing-forward is the failure mode, not any rejected repair.

**The second reason, from the same record: every audit rejection in this
lineage hit PROSE — a figure, a bound, an assertion's wording, a scope note.
Not one rejected a line of DSP.** The executors' code held. The coordinating
context's own text is where the defects were. Hold this document to the standard
you hold an executor's work to, or this change joins the chain.

## The operator ruling that already settles this

`04efe9c`, 2026-08-29: **"the signal SHALL NOT be collapsed to mono in the
middle of the chain. Folding belongs at the output."**

That ruling shipped — `ToReverbMono` no longer exists anywhere under `app/`.
Its shipping is what made this defect audible: before it, everything downstream
was mono anyway and the collapse was inert.

A cross-feed that reaches weight 0.5 inside the feedback write IS a mid-chain
mono collapse. The repair is not a new design decision. It is the application of
a ruling the operator already made, to a site that was missed when it was made.

## The defect, measured

Delay bank, wet pair, Stereo width over {0, 0.25, 0.5, 0.75, 1.0}, Send opened
to 1.0, LCG seed 20260913, 12000-sample warmup then 12000-sample measure,
everything else at its registered default. Measured independently by three
contexts, agreeing to every digit printed:

| Stereo width | 0.00 | 0.25 | 0.50 | 0.75 | 1.00 |
|---|---|---|---|---|---|
| Feedback 0.0 | 1.000000 | 0.000896 | -0.019932 | -0.012245 | -0.006245 |
| Feedback 0.7 | 1.000000 | 0.001677 | 0.009796 | 0.052801 | 0.119325 |

At Feedback's registered default the row collapses at the first step and then
wanders without direction. **With Feedback open, correlation RISES across the
top three quarters of the travel: the control named for the stereo field
narrows the image there.**

**The cause is arithmetic, not statistics.** `dsp::StereoDelay::Process`
computes `cross = p.dwid * 0.5f * widthBalance` and passes it to
`dsp::CrossFeedPair`, whose result is written INTO the recirculating delay
lines. At width 1.0 with Width balance at its own default the weight is exactly
0.5, where `CrossFeedPair` returns two identical values by construction — its
own header says 0.5 "mixes them in equal measure". The feedback content becomes
mono and replays on every repeat.

## The parent defect: Width balance is a ratio in name and a gain in code

Found while tracing the above, and it is the more interesting half.

`openspec/changes/archive/2026-08-18-frogg3rs-post-expansion-consolidation/research/RESEARCH2-drive-delay.md`,
section "Width Balance — `WBal` — NEW", is slot 12's design document. It
specifies the two hardcoded weights becoming **"a pair of COMPLEMENTARY
knob-derived weights"**: 0.0 all cross-feed with the spread pinned near zero,
1.0 all time-offset spread with the cross pinned near zero. Its stated reason to
exist is that the mod matrix "cannot change THE RATIO BETWEEN THEM... Only a new
knob that literally is that ratio can reach it."

What shipped multiplies BOTH terms by the same `widthBalance`, with
`SetWidthBalance` an identity map. That is a common gain: at 0 both mechanisms
vanish together, and the ratio between them is fixed at 0.35:0.5 at every
setting — the one thing the knob exists to change is the one thing it cannot.

The spec delta this change carries says slot 12 is "**the ratio between** the
Width knob's time-offset spread and its cross-feed blend." The shipped identity
map does not satisfy that sentence.

## The question the repair must answer first

**Can cross-feed widen this signal at all?** Both delay lines are fed the same
mono input and differ only in read time, so cross-feeding them blends two
time-shifted copies of one signal — strictly re-correlating, reaching exact mono
at weight 0.5. If it cannot widen a mono-fed pair, then no law that lets Stereo
width raise `cross` is correct, and the repair is to take the cross-feed off
that knob entirely.

The research's own precedent points the same way: Bitwig's stereo Delay ships
**Width** and **Cross Feedback** as two independent controls, treating them as
separable dimensions rather than one knob's two halves.

This specification has already made that move twice: the reverb tank's
cross-feed was retired to a fixed coupling at the retired knob's default weight,
and the Drive page's Link coupling was kept as a named constant when its knob
was removed. Both times the default voice was preserved exactly.

Answer this by measurement before choosing a mechanism. Do not assume it.

## The constraint map — traced, so nobody re-derives it

1. **`dsp::CrossFeedPair` is SHARED** between `dsp::StereoDelay::Process` and
   `dsp::Reverb::Process`. Changing the helper changes the reverb tank. The
   repair belongs in the delay's weight computation or in where the crossed pair
   is applied — never in the helper.
2. **`cross_feed_pair_is_identity_at_zero_and_an_equal_blend_at_one_half`** pins
   the helper's own arithmetic including the 0.5 midpoint. It stays green under
   any repair that leaves the helper alone, and that is a feature: the mono point
   is correct FOR THE HELPER and wrong only as a destination a UI knob can reach.
3. **`stereo_delay_cross_feed_reproduces_its_captured_output_exactly` is a
   RECAPTURE TARGET, not a constraint.** Its own comment says its literals "were
   captured by running this same fixture against `dsp::StereoDelay::Process`
   before that de-duplication" — it certifies that a refactor changed nothing.
   It is not a parity pin against any external source. Re-capturing it after a
   deliberate behaviour change is legitimate; re-capturing it silently is not.
   State the capture point's move in the test's own comment.
4. **There is no firmware to be in parity with.** The Daisy firmware never
   shipped a delay and no `StereoDelay` exists anywhere under `src/`.
   `app/dsp/Delay.hpp` is a copy of the RETIRED simulator's `StereoDelay.hpp`,
   and `git show f236915^:sim/StereoDelay.hpp` carries `const float cross =
   p.dwid * 0.5f;` verbatim. The defect is original to this project. The port
   has already deliberately diverged from that source once, documented in the
   header under "REMOVED since" — that is the precedent, and this repair amends
   the header's "nothing on this page is newly authored" sentence the same way.
5. **At `dwid == 0` the cross-feed is a no-op at any weight**, because
   `widthSpread` is zero so `timeL == timeR`, the input is mono, so `dL == dR`
   and `CrossFeedPair(x, x, w)` returns `{x, x}` for every `w`. Any law that
   agrees with the current one at width 0 costs nothing at the registered
   default.
6. **Other pins that touch `Process`'s output**: the Freeze family
   (`stereo_delay_freeze_*`), `stereo_delay_default_knob_values_reproduce_original_output_exactly`,
   and `stereo_delay_width_balance_mapping_keeps_cross_in_0_1_and_spread_at_or_below_todays_max`.
   The last asserts a bound SHAPE, not literals, and a repair must keep
   satisfying it. Enumerate all of them before editing; do not discover them
   from a red build.

## Delivered by the superseded change, already on `main`

- `18f99b5` — Reverb's Density travel pinned against Stereo width's, the
  correlation helper lifted to file scope, four inherited figures withdrawn.
- `b05e83a` — `MANUAL.md` and `QUICK_DICT.md` for both pages' Diffusion,
  Density, Stereo width and Damping.

Both passed a fresh-context postflight before their commit. Do not re-do them.

**The Delay Stereo width entry those commits wrote is accurate to the CURRENT
mechanism.** This repair changes that mechanism, so those sentences become stale
the moment it lands and are part of this change's own work.

## Working rules

1. **A count or a figure is not written into prose where a check can produce
   it.** Four figures were withdrawn from the superseded change for exactly
   this; do not add a fifth.
2. **No task hands an executor a check without its assertion already written.**
   A blank is concrete, demands filling, and will be filled toward green.
3. **MEASURE and IMPLEMENT are separate tasks** wherever the threshold is not
   already known.
4. **An executor that meets a conflict reports and stops.** It may not adjust a
   threshold, weaken an assertion, or edit a requirement to match code.
5. **A check that asserts only liveness asserts nothing**, and a check that
   passes MORE comfortably when the thing it measures is dead is worse than
   none. The superseded change shipped one of those and caught it only under an
   adversarial read.
6. **A gate gets one adversarial pass from a context that did not build it.**
7. **Coordinator-authored artifact text is not privileged over executor
   output.** It carries every recorded rejection in this lineage.
8. **Postflight runs once per STAGE, before that stage's commit**, and a stage
   is committed once.
9. **Do not hand this defect forward again.** Five sessions did. If it cannot be
   repaired, the report says why, with measurements — that is a completed task.
   Recording it and passing it on is not.

## Impact

- **Affected spec:** `froggers-sheaf-parameter-model`, carried forward whole with
  three MODIFIED requirements. Its stereo-image scenario is currently REFUTED for
  the Delay page and this change's repair is what makes it true.
- **Directories swept:** `openspec/specs/`, `openspec/changes/`, the repository
  root's documents, and `app/`.
- **Code this change edits:** `app/dsp/Delay.hpp` (the repair, and its provenance
  header), `app/FroggersDspParityTests.cpp` (the repair's check, and the
  recaptured golden vector), `MANUAL.md` and `QUICK_DICT.md` (the Stereo width
  and Width balance entries the repair makes stale).
- **Affected gates:** `app/check_docs_match_parameter_table.py`,
  `app/check_spec_checks_resolve.py`,
  `app/check_modified_requirements_restate_promoted.py`.
- **Out of scope:** `src/core/FroggersEngine.hpp` is frozen firmware.
  `openspec/changes/frogg3rs-midi-controller-resilience/` and
  `openspec/changes/frogg3rs-randomize-depth-reclaim/` are untracked, held, and
  owned by other sessions; no blanket stage may sweep either in, and this change
  may not edit either.
- **A concurrent session is writing in this tree.**
  `frogg3rs-randomize-depth-reclaim` appeared mid-session and its spec delta
  names two tests that do not exist, so `check-spec-checks-resolve` is RED for a
  cause this change neither owns nor may repair. Every stage gate reports the
  suite as green apart from those two named failures and confirms the count has
  not grown. Any other failure stops the stage.
- **Delivery is a push to `main`.** This repository does not use pull requests.
  No AI attribution appears in any commit message.

## Known gaps carried forward, recorded rather than hidden

- **The Freeze half of the Delay width claim is traced but pinned by nothing.**
  Every Freeze case sets the width knob to zero to keep width out of scope, and
  the cross-feed's golden vector runs at a raised Feedback with Freeze at zero,
  so no case exercises Freeze opening the cross-feed path on its own.
- **Two gate limitations, deliberately unrepaired.**
  `app/check_artifact_symbols_resolve.py` treats a bare CamelCase word as prose.
  `app/check_modified_requirements_restate_promoted.py` never compares
  requirement PROSE — its bullet collector keeps only dash-opening lines, so a
  MODIFIED requirement's paragraphs can change or vanish with nothing noticing.
  Stage 3 leans on that gate, so its three requirements are read by hand.
- **Hygiene found outside this change's Impact, unfixed and reported:**
  `openspec/.sessions/marbles-mod-led-level-meter-progress.md` is orphaned from
  an archived change and invoked by nothing, now found by three separate sweeps.
  The repo root's vendored `node-v22.16.0-darwin-arm64/` and `build/manifest/`
  are ignored only through `.git/info/exclude`, so that protection does not
  survive a clone. `frogg3rs.code-workspace` excludes two directories that do
  not exist.
