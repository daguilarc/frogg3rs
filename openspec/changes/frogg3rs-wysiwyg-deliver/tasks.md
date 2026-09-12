# Tasks — `frogg3rs-wysiwyg-deliver`

Supersedes `frogg3rs-wysiwyg-finish`. Only work NOT yet in the tree appears
here; what is already delivered is listed in the proposal's state-of-the-tree
section and must not be re-done.

## How this change is sequenced, and why

**Every stage ends in a commit and a push.** Its predecessors held four sessions
of green, passing work in one uncommitted tree, so no progress was ever banked
and every fresh context had to re-audit the whole surface from nothing. A stage
that is green is delivered before the next begins. If this change is abandoned
halfway, everything up to the last completed stage is already on `main`.

Stages are ordered by what they touch, not by subject, so that a stage's work
can run in parallel without two executors writing one file.

## How every figure in this change is produced

- Measure through the PRODUCTION ROUTER at the bank's registered defaults,
  naming any knob deliberately moved off its default. A hand-configured DSP
  object is not the instrument and the divergence is silent. `RouteFilterBank`
  and `RouteDriveBank` are private members of `FroggersApp`, so a test reaches
  them either through the app's public entry or through a replica that mirrors
  the router's setter order. `ProcessDriveBank` in
  `app/FroggersDspParityTests.cpp` is the existing precedent for the Drive bank.
  THERE IS NO FILTER-BANK EQUIVALENT: every Peak-branch test in that file
  configures `dsp::FilterFxChain` by hand, which is the defect the Peak
  measurement below exists to avoid.
- A figure lands in a TEST, never in prose.
- State the GRID and the TAP POINT beside any worst-case figure. Where the claim
  is about an improvement, measure both states in ONE run over ONE grid and
  assert the comparison.
- Carry the liveness control inside the fixture and never let it be the whole of
  a case.
- Every task that produces a check states what the check must ASSERT. A task
  that does not is defective — report it rather than choosing a threshold.

## Stage 0 — bank what is already green

- [ ] 0.1 Baseline. `nice -n 10 make -C app test -j2`, then run every binary in
      `app/build/` BY PATH and record pass/fail per binary. The recipe stops at
      the first failure, so a green recipe alone says nothing about binaries it
      never reached. Never raise `-j` above 2. Before believing any binary's
      count, check its mtime against its sources — `make` no-ops when they match
      to the second, and the run then reports the previous build.
- [ ] 0.2 Commit and push the inherited work: the delivered items in the
      proposal's state-of-the-tree section, the hygiene repairs, and the
      uncommitted `openspec archive` of `frogg3rs-effect-page-hierarchy` and
      `frogg3rs-prose-claims-get-gates`. Leave
      `frogg3rs-midi-controller-resilience` untouched — it is a separate change
      the operator is deliberately holding.
      NO AI ATTRIBUTION in the commit message, per this repository's own
      instruction. Delivery is a push to `main`; this repository does not use
      pull requests.

## Stage 1 — the gates that accept what they reject

Independent of every other stage: separate files, no shared symbols.

- [ ] 1.1 `app/check_spec_checks_resolve.py` verifies that a cited file defines
      the cited test case only for `app/`-owned paths, because only those have
      their cases indexed. Any other indexed path — an `External/Sheaf/...`
      source, any indexed document — therefore resolves with NO pairing check,
      so a real but unrelated test case sitting beside it passes as evidence.
      ASSERT: a `Check:` naming a file and a case where that file does not
      define that case FAILS whether or not the file is under `app/`; and a
      `Check:` whose every part is a path that defines no test case at all FAILS
      rather than resolving, because a citation naming no case names no check.
      Choose between indexing cases from the other trees and refusing to treat
      an unindexed path as pairing evidence by reading which files the script's
      index builder can actually reach; say which you chose in the script's own
      header. Re-run the whole spec corpus: if closing this turns an existing
      `Check:` red, that `Check:` was never evidence and the scenario needs one.
- [ ] 1.2 `app/check_modified_requirements_restate_promoted.py` decides whether a
      declared edit covers a dropped promoted clause with a bare substring test,
      so one generic fragment absorbs every dropped clause at once, including
      the kind the script's own header says it exists to catch. Separately, its
      `keeps:` text is searched across the whole restated requirement rather
      than the bullet that replaced the dropped clause.
      ASSERT: a declared fragment covers a dropped clause only when it
      identifies that clause rather than merely occurring inside it; and the
      kept text is found in the replacement, not merely somewhere in the
      requirement. State the rule chosen in the script's header.
      This change's own declared fragments must stay green across the repair —
      that is the regression control. If a repair turns them red, read whether
      the fragment or the repair is wrong before changing either.
- [ ] 1.3 ONE adversarial pass over both repaired scripts, by a context that did
      not repair them: enumerate the accepting surface by reading, construct at
      least one evasion per accepting branch against the real scripts with
      fixtures kept outside the repo, and report what got through.
      WHAT GOT THROUGH IS RECORDED HERE, NOT AUTOMATICALLY FIXED. A hole is
      repaired only if it defeats the gate's purpose; every gate has holes, and
      reading a HOLED verdict as an instruction to rebuild is what consumed a
      session of this change's predecessor on a single check.
      Already known and out of scope: any indexed PATH counts as evidence in
      `check_spec_checks_resolve.py`, so a full-path citation of a document
      proves as little as a bare basename did.
- [ ] 1.4 Fail a `TEST_CASE` that carries at least one assertion where EVERY
      assertion in it is liveness-shaped — `isfinite`, `> 0`, `>= 0`, `!= 0`,
      `isnormal`, a non-empty container. A case carrying a real assertion BESIDE
      a liveness control is correct and MUST pass, since the liveness control is
      required. Prove it red by breaking it once. This is a gate, so it takes
      the same single adversarial pass and the same recording rule as 1.3.
      Its claim to run before the measurement stages is a QA argument, not a
      technical dependency: those stages produce no symbol it consumes and
      carry written assertions already, so it does not block them.
- [ ] 1.5 Stage gate: full suite green, then commit and push.

## Stage 2 — Peak gain holds its level

Writes `app/dsp/FilterFx.hpp`, `app/FroggersAppCore.hpp`,
`app/FroggersDspParityTests.cpp`, `MANUAL.md` and `QUICK_DICT.md`.

- [ ] 2.1 MEASURE FIRST, read-only apart from recording the numbers here. Build
      the Filter-bank replica that does not yet exist, mirroring
      `RouteFilterBank`'s setter order the way `ProcessDriveBank` mirrors
      `RouteDriveBank`, and drive it at the Filter bank's registered defaults.
      NOT `dsp::FilterFxChain` configured by hand, and NOT `dsp::ResonantBump`
      alone: the isolated bump reads ASCENDING and hides the defect entirely,
      and a hand-routed chain that never set the comb offset moved one frequency
      row by 2.6 dB while the others agreed, which is why the defect survived
      four audit passes.
      Report, with the grid and the tap point stated: the level across Peak
      gain's travel AT the bump's own resonant frequency, and at frequencies
      away from it.
      ASSERTION, to be written once this reports: the check pins the PAIRING —
      flat at the bump's own resonance across the whole travel, AND a monotonic
      fall away from it. A flat row alone is what a dead rig prints, so the
      moving rows are the liveness control and must be asserted in the same case.
- [ ] 2.2 Keep `1.0f / peak.height` on the branch and add a makeup gain that
      holds the chain's total output level across the travel.
      PLACEMENT IS NOT THE EXECUTOR'S TO CHOOSE. The makeup multiplies the
      SMOOTHED trim, ahead of `peakLimiter.Process`, so the limiter still sees
      the signal it was tuned to bound. It does NOT go after the limiter: that
      is attack/release-smoothed rather than a hard clip, so a factor applied
      after it raises the post-limiter ceiling by exactly that factor and makes
      the blowout assertion below unsatisfiable by construction.
      THE LAW'S FORM COMES FROM 2.1. A bare scalar cannot hold total level
      across a travel whose loss varies with `peak.height`, so do not assume the
      makeup is one. If 2.1's numbers do not admit a level-holding law under
      this placement, REPORT AND STOP — do not move the multiply past the
      limiter to make the arithmetic work.
      Name it `kPeakTrimMakeUpGain`, beside the trim it compensates in
      `app/dsp/FilterFx.hpp`. It is NOT the existing `kMakeUpGain`, which is
      function-local to `SanitizeOutputSample` in `app/FroggersAppCore.hpp` and
      is the OUTPUT limiter's fixed reciprocal of the stage ceiling — a
      different quantity, for a different stage, in a different file.
      `peakLimiter` STAYS: its own comment records the blowout it stops.
- [ ] 2.3 ASSERT, in one case measured in one run: total output level across the
      travel does not fall, and the peak's level relative to its surroundings
      rises by more than it does under the shipped law, with BOTH laws measured
      over the same grid in the same run.
- [ ] 2.4 Re-measure the blowout the trim exists for: a pinned self-oscillating
      comb through the peak at maximum. ASSERT the ceiling is no higher than the
      shipped law's, measured in the same run. Its positive control is the level
      that case reached before the trim existed. A level fix that reopens the
      blowout is not a fix.
- [ ] 2.5 ASSERT that Peak gain's registered default reproduces today's output
      bit-for-bit. Verify from `FroggersBankLayouts` that the default leaves the
      trim at unity rather than assuming it; the makeup is a new multiply in the
      path and the default is exactly where a stray factor hides.
- [ ] 2.6 Fix the stale comment on `RouteFilterBank`'s `peak.SetHeight` line: it
      states a 2x (+6 dB) ceiling where the constant it comments,
      `dsp::kMaxResonantBumpHeight`, is 3.0f.
- [ ] 2.7 BOTH documents carry "up to about +9.5 dB (3×)", which describes the
      resonant bump read in isolation and is false of the output — that gain
      never reaches it. `MANUAL.md` and `QUICK_DICT.md` each carry it, and no
      task in the superseded change touched the quickdict copy. Rewrite both to
      what the control does after 2.2.
- [ ] 2.8 Stage gate: full suite green, then commit and push.

## Stage 3 — one definition for the cross-feed, and Reverb's slot 7

These share `app/dsp/Reverb.hpp`, `app/dsp/Delay.hpp`,
`app/FroggersDspParityTests.cpp` and `MANUAL.md`, so they are one stage and run
in order.

- [ ] 3.1 Enumerate the cross-feed by OPERAND across the whole tree — a
      `* 0.5f` against a paired L/R or A/B mix is the shape — and report FOUND
      versus CHANGED with a disposition per hit, zeros included. Production and
      test-side copies both count; the test side includes
      `UnsaturatedTankReplica` in `app/FroggersDspParityTests.cpp` and at least
      one inline copy beside it.
- [ ] 3.2 One definition, in a NEW `app/dsp/StereoField.hpp`. Not
      `app/dsp/Limiter.hpp`: following `dsp::EqualPowerWetDry`'s precedent by
      destination rather than by shape would put a cross-feed and an allpass
      cascade in a file named for a limiter, which is the defect this change
      exists to fix.
      Name it `dsp::CrossFeedPair`, taking the two channel values and a cross
      weight in [0, 0.5] and returning the crossed pair, identity at weight 0.
      Reverb's own coefficient scale at its call site is `kTankCrossFeedScale`,
      defined beside the call in `app/dsp/Reverb.hpp`.
- [ ] 3.3 Move `dsp::DelayDiffuser` into the same header together with
      `dsp::SchroederAllpassSection`, which it holds, so the header does not
      depend back on `app/dsp/Delay.hpp`. Moving one without the other creates a
      cycle; moving only the section leaves Reverb to re-derive the constants,
      which reschedules the duplication rather than removing it.
      `kDiffusionCoeffScale` STAYS on `dsp::StereoDelay`: it is the weight Delay
      applies when DRIVING the cascade, not a constant the cascade holds.
      ASSERT that Delay's existing diffusion is BIT-IDENTICAL across the move —
      it is a relocation, and its own parity pins prove it.
      THE INBOUND HALF: comments in `app/dsp/Delay.hpp` refer to `DelayDiffuser`
      as a neighbour in the same file. Enumerate them rather than trusting a
      count, and repair each.
- [ ] 3.4 MEASURE FIRST, read-only. Both cross-feeds are the same weighted
      average in isolation, so the direction each knob moves comes entirely from
      what surrounds them: on the Delay page the same knob also drives an L/R
      time offset, and that pairing — not the cross-feed — is what the widening
      claim rests on. Nothing in the tree settles this by reading.
      Measure L/R correlation of the wet leg across Delay's Stereo width travel
      and across Reverb's slot 7 travel, through the production routers at the
      registered defaults, with the grid and tap point stated. Name any knob
      moved off its default: Reverb's Send default is 0.0, where the tank is
      never fed and the probe reads `nan`, which is this measurement's
      dead-instrument signature.
- [ ] 3.5 Fold Reverb's cross-feed under its Stereo width, driven in whichever
      sense 3.4 measured to make both halves of that knob move correlation the
      same way. Reverb's Stereo width already drives the tank's mid/side output
      scaling; this adds the cross-feed to the same knob, and the two must not
      fight. If 3.4 contradicts the expected sense, REPORT AND STOP: the
      requirement that already states the outcome has to change before code does.
      ASSERT both pages' Stereo width move L/R correlation monotonically in the
      SAME direction, measured in one run.
- [ ] 3.6 MEASURE FIRST, read-only. Pin today's slot 7 against slot 6 on L/R
      correlation of the wet leg, so the replacement has something to turn green.
      ASSERTION, once this reports: the check pins BOTH travels in one case, so
      the near-zero row is meaningful only because the moving row moves.
- [ ] 3.7 Replace the cross-feed behind slot 7 with `dsp::DelayDiffuser`
      UNCHANGED, at its own section lengths rather than retuned for the tank.
      Rename the parameter to `Density`, short name `Dens`, in the parameter
      table and in BOTH label tables — `FroggersBankLayouts()` in
      `app/FroggersParameters.hpp`, `FroggersApprovedLabels()` in
      `app/FroggersUiSurface.hpp`, and the deliberately independent second copy
      inside `every_rendered_label_matches_the_approved_list_verbatim` in
      `app/FroggersSurfaceTests.cpp`. The second copy is independent on purpose,
      so a rename reaching only one turns the surface gate red.
      TWO COLLISIONS MAKE A BLIND REPLACE WRONG, and both are the DELAY page's
      own Diffusion, which this change does NOT rename: `diffusionKnob01` is one
      spelling for two unrelated parameters — `dsp::Reverb::Process`'s, the
      rename target, and `dsp::MapRowsToDelayParams`'s, which carries Delay's
      knob — so rename by qualified symbol, never by the bare identifier. And
      the literal `Diffusion`/`Diff` is not unique at any label site or in
      either document: Delay's row sits beside Reverb's at every one. Change the
      Reverb row by its SLOT, and report the Delay row untouched at each site as
      an explicit disposition rather than by silence.
      THE DOC ROWS MOVE IN THIS SAME STEP: `app/check_docs_match_parameter_table.py`
      binds each manual and quickdict bold entry to the parameter table by name
      and slot, so renaming the code while the documents still say Diffusion
      fails the build.
      THE DSP-SIDE SPELLING MOVES TOO, including the parity-side copy in
      `UnsaturatedTankReplica`, whose formula goes stale the moment production's
      changes. Enumerate by operand and report FOUND versus CHANGED.
- [ ] 3.8 Density's default is 0.0, diffusion at minimum — the tank's own
      twin-line character, as close to today's default tank as the new stage
      allows. Bit-identity is impossible because the mechanism behind the slot
      changes entirely. ASSERT the measured difference from today's default tank
      and report the figure with its grid, rather than asserting closeness in
      prose.
- [ ] 3.9 Measure the metallic ringing the allpass cascade produces, on an
      impulse, across Density's travel, and report it as a figure with its grid.
      ASSERT the ringing metric rises monotonically with Density, so the
      manual's claim that the control trades smoothness for coloration is backed
      rather than asserted.
- [ ] 3.10 Stage gate: full suite green, then commit and push.

## Stage 4 — the documents

House style: plain present tense saying what a control does; no jokes; no
defining by negation; no history the reader never saw; and no task numbers,
change names, planning-document references or rule citations in comments, test
names or strings.

- [ ] 4.1 `MANUAL.md` and `QUICK_DICT.md` for the two Filter controls and both
      pages' Diffusion, Density and Stereo width. The Drive controls are already
      updated and are not re-done.
- [ ] 4.2 State what each control now does, which are conditional and on what,
      and — for Fuzz and Symmetry — which settings make another control inert.
      State the Fuzz-maximum case honestly: with the blend floored, Fold still
      moves an octave band there while barely moving total level. What it moves
      at Fuzz maximum is TILT, not density, and a manual saying "Fold still
      works there" without naming the quantity replaces a silent defect with a
      misleading sentence.
- [ ] 4.3 State on BOTH the Delay and Reverb pages what Diffusion and Density
      each mean, so the difference is on the page rather than in the reader's
      memory.
- [ ] 4.4 Rewrite the cross-instrument Feedback section affirmatively. Several
      of its sentences define a control by what it is not, including two Delay
      entries opening "not a feedback path itself" and a closing line saying
      Reverb has no control named Feedback. Say what each control does, and say
      what Reverb's tank uses instead.
- [ ] 4.5 The Anti-alias entry does not carry the qualification its own scenario
      names: the no-dip bound is met at the bank's registered defaults and is
      not claimed everywhere, and away from those defaults the dip is larger at
      some Gain and Shape settings. Say so.
- [ ] 4.6 `MANUAL.md` states plainly that raising Density trades smoothness for
      coloration, and at roughly which part of the travel the coloration becomes
      audible, backed by 3.9's figure.
- [ ] 4.7 Stage gate: full suite green, then commit and push.

## Stage 5 — the spec delta and archival

- [ ] 5.1 As each stage lands, move its scenario's `Check:` line from its
      not-yet-delivered marker onto the test that now backs it, and resolve that
      test's name by grepping for it — when the line is written and again before
      delivery.
- [ ] 5.2 Re-count both MODIFIED requirements against the promoted text. The
      restate gate checks clause-level drift inside any scenario the delta
      restates and requires a `keeps:` line under every declared edit, so the
      remaining manual duty is the scenario COUNT, which the gate deliberately
      does not cover: dropping a whole scenario is sometimes legitimate
      supersession and nothing mechanical separates that from an accident.
- [ ] 5.3 Every scenario's `Check:` names a test that exists and passes, or is
      marked not yet delivered in the form the gate recognises. Nothing parses
      prose, which is why this is the cheapest claim in the document to make
      falsely.
- [ ] 5.4 Archive this change, promoting its delta.

## Audits

- [ ] A.1 Preflight, in a context that did not write this change, covering the
      ARTIFACTS AND THE CODE TOGETHER — the artifacts are claims about the code,
      so neither can be checked without the other. It may reject.
- [ ] A.2 Each stage gets its own postflight in a fresh context before its
      commit, comparing implementation against this proposal and reporting
      divergence strictly.
