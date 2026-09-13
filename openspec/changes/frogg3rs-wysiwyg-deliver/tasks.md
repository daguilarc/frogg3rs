# Tasks — `frogg3rs-wysiwyg-deliver`

**Work not listed in this file does not become a task unless the operator
adds it.**

Supersedes `frogg3rs-wysiwyg-finish`. Only work NOT yet in the tree appears
here; what is already delivered is listed in the proposal's state-of-the-tree
section and must not be re-done.

## How this change is sequenced, and why

**Every stage ends in a commit and a push.** Its predecessors held four sessions
of green, passing work in one uncommitted tree, so no progress was ever banked
and every fresh context had to re-audit the whole surface from nothing. A stage
that is green is delivered before the next begins. If this change is abandoned
halfway, everything up to the last completed stage is already on `main`.

Delivery is a push to `main`; this repository does not use pull requests. **No
AI attribution appears in any commit message**, per this repository's own
instruction.

**Every commit stages the paths it names and nothing else.** `git add -A`,
`git add .` and `git commit -a` are forbidden here for a specific reason:
`openspec/changes/frogg3rs-midi-controller-resilience/` is untracked in this
tree, so any blanket stage sweeps a change the operator is deliberately holding
into a commit on `main`, and there is no pull request in this repository to
catch it. Check `git status --short` before every commit and confirm that
directory is still untracked afterwards.

All three apply to every stage below, not to one of them.

Stages are ordered by what they touch, not by subject. They are NOT independent:
Stage 2 and Stage 3 both write `app/FroggersDspParityTests.cpp` and `MANUAL.md`.
What keeps two executors off one file is the stage gate — a stage is green,
committed and pushed before the next begins — not the ordering. Within a stage,
tasks that name disjoint files may run together.

## How every figure in this change is produced

- Measure through the PRODUCTION ROUTER at the bank's registered defaults,
  naming any knob deliberately moved off its default. A hand-configured DSP
  object is not the instrument and the divergence is silent. `RouteFilterBank`
  and `RouteDriveBank` are private members of `FroggersAppCore` — not
  `FroggersApp`, which names neither — so a test reaches them either through the
  app's public entry or through a replica that mirrors the router's setter
  order. `ProcessDriveBank` in
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

## Stage 0 — baseline

- [x] 0.1 Baseline. `nice -n 10 make -C app test -j2`, then run every binary in
      `app/build/` BY PATH and record pass/fail per binary. The recipe stops at
      the first failure, so a green recipe alone says nothing about binaries it
      never reached. Never raise `-j` above 2. Before believing any binary's
      count, check its mtime against its sources — `make` no-ops when they match
      to the second, and the run then reports the previous build.
      DONE: the recipe reached its last target at exit 0, and all twelve binaries
      then ran by path at exit 0. Two counters written for this task were dead on
      first use — one resolved no binaries, one counted Catch2 summary lines this
      suite does not print — so the counter now prints VOID when it reads no
      PASS/FAIL lines rather than reporting a silent zero as a clean run.
- [x] 0.2 The inherited work is banked. It was committed and pushed before this
      change began: the delivered items in the proposal's state-of-the-tree
      section and the hygiene repairs are in `e565331`, and this change's own
      artifacts are in `585f332`. The `openspec archive` of
      `frogg3rs-effect-page-hierarchy` and `frogg3rs-prose-claims-get-gates`
      is NOT committable and no task should ask for it: `.gitignore` excludes
      `openspec/changes/archive/` so archived changes stay local as a working
      record. `frogg3rs-midi-controller-resilience` stays untouched — it is a
      separate change the operator is holding.

## Stage 1 — the gates that accept what they reject

Independent of every other stage: separate files, no shared symbols.

- [x] 1.1 `app/check_spec_checks_resolve.py` verifies that a cited file defines
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
      DONE. A `Check:` now resolves only by naming a real case, a file that
      defines the case it is paired with, a gate the `test` target actually runs
      — established by reading the target, not by matching a filename — or a
      declared-manual marker. The index also reads the declaration forms Sheaf's
      tests use, which no earlier version could see; 72 real tests in two files
      were invisible to it, so "unindexed" had been a property of the recogniser
      rather than of those trees. Closing this turned 33 lines red, of which 16
      were legitimate patterns the first cut broke and the rest were citations
      that had never been evidence; those are repaired under the citation task
      below. The gate and the citations it reads move together, as they did when
      the other citation gate tightened.
- [x] 1.2 `app/check_modified_requirements_restate_promoted.py` decides whether a
      declared edit covers a dropped promoted clause with a bare substring test,
      so one generic fragment absorbs every dropped clause at once, including
      the kind the script's own header says it exists to catch. Separately, its
      `keeps:` text is searched across the whole restated requirement rather
      than the bullet that replaced the dropped clause.
      ASSERT: a declared fragment covers a dropped clause only when it
      identifies that clause rather than merely occurring inside it. State the
      rule chosen in the script's header.
      This change's own declared fragments must stay green across the repair —
      that is the regression control. If a repair turns them red, read whether
      the fragment or the repair is wrong before changing either.
      THE `keeps:` HALF IS INFEASIBLE AND IS NOT ATTEMPTED. It was written here
      as a second repair — bind the kept text to the bullet that replaced the
      dropped clause rather than to the whole requirement — and execution showed
      the declaration form cannot express it. An entry is a fragment and a
      `keeps:` phrase, with no field naming the replacement bullet, and
      positional correspondence does not survive real edits: an inserted bullet
      shifts everything after it, and the Reverb bank's slot rework maps two
      promoted bullets onto four. Three independent alignments — ordinal
      position, leftover pairing, and longest-common-subsequence — were run
      against the real declared edits and none identifies a single replacement
      bullet. Closing it needs a new field in the declaration syntax and a
      rewrite of every declared edit in the tree, which is a change of its own.
      The limitation is stated in the script's header, where the first version
      of the gate had already conceded it.
- [x] 1.3 ONE adversarial pass over both repaired scripts, by a context that did
      not repair them: enumerate the accepting surface by reading, construct at
      least one evasion per accepting branch against the real scripts with
      fixtures kept outside the repo, and report what got through.
      WHAT GOT THROUGH IS RECORDED HERE, NOT AUTOMATICALLY FIXED. A hole is
      repaired only if it defeats the gate's purpose; every gate has holes, and
      reading a HOLED verdict as an instruction to rebuild is what consumed a
      session of this change's predecessor on a single check.
      THE TEST FOR "DEFEATS THE GATE'S PURPOSE", so the judgement is not
      re-argued each time: a hole is fatal when the thing that gets through is
      the very defect the gate was written to catch. A hole that lets an
      unrelated shape pass is ordinary and is recorded only. Apply that test and
      state the verdict per hole; do not rebuild anything on the strength of a
      hole existing.
      Already known and out of scope: any indexed PATH counts as evidence in
      `check_spec_checks_resolve.py`, so a full-path citation of a document
      proves as little as a bare basename did.

      DONE, AND NOTHING WAS REBUILT. 21 accepting branches enumerated, 14
      evasions run against the real scripts, 11 through, 7 branches enumerated
      but not attacked and named as open items. What got through, by mechanism:

      In the spec-check resolver — a `Check:` line spelled `Check :` or bulleted
      with `*` is invisible to the whole gate, inspected by nothing; a case name
      appearing only inside a `//` comment is indexed as a real test; a bare
      test function counts as invoked when its own declaration plus any single
      incidental mention, a comment included, reach two occurrences; a Makefile
      target defined twice has both recipe bodies unioned by the parser where
      GNU Make discards one, so a gate that never runs can still resolve a
      citation; and specs under the archive are never scanned.

      In the restate gate — renaming a scenario's title by as little as one
      comma makes every bullet under it read as not restated, exempting a
      dropped clause from any check; a `keeps:` phrase satisfies itself from
      anywhere in the document, which is the limitation already recorded under
      the task above; and a requirements heading not spelled MODIFIED makes the
      whole file invisible.

      TWO DEFEAT PURPOSE RATHER THAN DECORATE IT, and both are repaired: the
      comma-renamed scenario title, because detecting dropped clauses is the
      restate gate's entire job and one character disabled it for a whole
      scenario, and the comment-mentioned case name, because resolving a
      citation against a name no test defines is exactly what the other gate
      exists to stop. The rest are recorded above and left alone.
      The earlier rebuild rounds on the resolver were not the cost of fixing
      holes. They were a preflight miss: this task's own assertion asserted that
      a citation naming no test case names no check, which one grep against the
      existing `Check:` lines falsifies — gate scripts and supporting source
      files are cited beside real cases throughout. Catching that before
      dispatch was preflight's job and it is where that class of defect dies.
- [ ] 1.4 Stage gate: full suite green, then commit and push.

## Stage 2 — Peak gain holds its level

Writes `app/dsp/FilterFx.hpp`, `app/FroggersAppCore.hpp`,
`app/FroggersDspParityTests.cpp`, `MANUAL.md` and `QUICK_DICT.md`.

- [x] 2.1 MEASURE FIRST, read-only apart from recording the numbers here. Build
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
- [x] 2.2 CLOSED NEGATIVE — no law shipped, code unchanged. Keep `1.0f / peak.height` on the branch and add a makeup gain that
      REDUCES the level the travel costs. It does not hold level, and no law at
      this placement can: `peakLimiter`'s ceiling is `kStageCeiling` (0.80) and
      `OutputLimiter::DesiredMagnitude` asymptotes toward it without ever
      reaching it, so the peak branch's contribution is hard-capped however
      large the pre-limiter makeup grows. Measured directly: at the travel's own
      endpoint, height 3.0, total broadband RMS stays 2.47% below the height-1
      reference even at a makeup of one million. The carried ruling asks for no
      more than this — it took candidate (c) because the alternatives "moved
      total level the wrong way or further", which is a comparison, not a hold.
      CHOOSING THE LAW'S FORM, and the criterion below replaces an earlier one
      that was wrong. The required makeup diverges before the travel ends, so no
      finite power law reproduces it and the form is a choice, not a solve. Take
      `m(h) = h^p` and pick `p` by measurement. The bound is NOT "the ceiling
      does not rise": any makeup ahead of the limiter raises the ceiling by
      something, because `OutputLimiter::DesiredMagnitude` is strictly
      increasing in its argument, so that criterion is satisfiable only where
      the makeup is indistinguishable from 1 and it rules out the whole search
      by construction. The bound is the one the blowout check already names —
      the level the blowout case reached BEFORE the trim existed. Measure that
      level on the pinned self-oscillating comb, and take the largest `p` whose
      ceiling stays below it. Report the `p` grid, the retained level at each
      and the ceiling at each.
      IF THE BLOWOUT MEASUREMENT SHOWS THE LIMITER ALREADY BOUNDS THAT CASE,
      this task is superseded rather than satisfied: the trim would then be
      buying almost nothing at a cost of 7.10 dB, and what to do about it is the
      operator's call, not a law to tune. Report and stop.
      NAME THE CONSTANT FOR WHAT IT HOLDS. Under `m(h) = h^p` the value beside
      the trim is an EXPONENT, so a `k`-prefixed name ending in `Gain` holding
      0.5 would mislabel its own value. A measurement's label is a separate
      claim from its value, and the same is true of a constant's.

      RESULT: no law is shipped and `app/dsp/FilterFx.hpp` is unchanged.
      Retained level does improve with `p` — the fall shrinks from 7.10 dB to
      1.17 dB across the travel — but every `p` above rounding raises the output
      ceiling, because the makeup sits ahead of a limiter whose response is
      strictly increasing in its argument. Measured on the pinned comb at the
      Filter bank's registered defaults with Peak gain 1.0, Comb feedback 1.0
      and Topology 1.0, at the peak branch tap: shipped 0.796, trim cancelled
      0.800, limiter bypassed 1.027, neither 3.082. `peakLimiter` is what bounds
      the output, and removing the trim leaves it bounded within 0.04 dB. What
      the trim buys is limiter DUTY — required gain reduction falls from 11.7 dB
      to 2.2 dB statically — not output level.
      The carried ruling keeps the trim, and the makeup that ruling pairs it
      with cannot exist at this placement. Peak gain's level cost is therefore
      structural, and what this stage delivers is the documents saying so.
      REMOVING THE TRIM WAS MEASURED AND REJECTED. It would recover the 7.10 dB,
      and the output stays bounded without it, so the question was whether the
      extra limiter duty costs anything audible. It does, at the bank's own
      registered defaults rather than only at an extreme: cancelling the trim
      costs 6.3 to 13.8 dB of harmonic distortion and 7.0 to 15.8 dB of
      intermodulation across the travel, and the limiter's gain reduction swings
      8.90 dB at up to 198 dB per second where the trimmed law moves 1.67 dB at
      55 — pumping at syllabic rates. Separations run 350 to 5000 times the
      measurement's own spread, which is zero run to run. The trim earns its
      7.10 dB, so the label is what changes: the documents say what the control
      delivers and what it costs.
      THE BLOWOUT THE TRIM WAS ADDED FOR IS NOT REACHABLE from the knobs:
      `dsp::Comb::GetFeedback` caps the feedback magnitude at 0.95 deliberately,
      so undecaying self-oscillation cannot be driven through the production
      router. The measurement pins the loudest reachable state instead — the
      comb's in-loop saturator at its clamp — and says so rather than reporting
      a number from a state the instrument cannot enter.
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
- [x] 2.3 SUPERSEDED — 2.2 shipped no second law, so the comparison has nothing to compare. ASSERT, in one case measured in one run, as a COMPARISON between the
      two laws over one grid — never against a figure copied from a document:
      total output level across the travel falls by LESS under the new law than
      under the shipped law, and the peak's level relative to its surroundings
      rises by MORE. Both laws measured in the same run, on the same grid, from
      the same source. The shipped law's own fall is the positive control: it is
      7.10 dB end to end on the broadband source the measuring task fixed, and a
      run where that number does not appear is measuring something else.
      THE PRINT-ONLY CASES DO NOT SURVIVE THIS TASK. The measuring task left two
      `TEST_CASE`s that print tables behind a bare `isfinite` check and cannot
      fail. This assertion replaces them. Either they carry it, or they are
      deleted — a case that cannot fail must not reach the stage gate.

      DELIVERED, and the thresholds are recorded here because the task did not
      carry them and the executor had to choose: resonance spread below 0.01 dB
      across the whole travel, against a measured 0.00025 dB, and a strict fall
      at every step on both away rows totalling more than 4.0 dB, against a
      measured 6.04 and 6.52 dB. Both are anchored to this grid's own numbers
      with room to spare, and both are now written down rather than living in
      the executor's report. Handing a check without its assertion is a defect
      in the task, and this one had it.
      The break-it control falsified BOTH halves at once: pinning the replica's
      bump frequency off the routed knob moved the resonance row 6.36 dB and
      froze an away row at one value across all five cells. Restored from a
      removed binary, byte-identical.
- [x] 2.4 MEASURED — result recorded under 2.2. Re-measure the blowout the trim exists for: a pinned self-oscillating
      comb through the peak at maximum. ASSERT the ceiling is no higher than the
      shipped law's, measured in the same run. Its positive control is the level
      that case reached before the trim existed. A level fix that reopens the
      blowout is not a fix.
- [x] 2.5 VERIFIED — the registered default leaves the trim at unity and no multiply was added. ASSERT that Peak gain's registered default reproduces today's output
      bit-for-bit. Verify from `FroggersBankLayouts` that the default leaves the
      trim at unity rather than assuming it; the makeup is a new multiply in the
      path and the default is exactly where a stray factor hides.
- [x] 2.6 Fix the stale ceiling comments. TWO SITES, not one. On
      `RouteFilterBank`'s `peak.SetHeight` line a comment states a 2x (+6 dB)
      ceiling where the constant it comments, `dsp::kMaxResonantBumpHeight`, is
      3.0f. Separately in `app/FroggersAppCore.hpp`, near the output-limiter
      headroom reasoning, a comment hardcodes 4.0f and "4x" for the same
      constant. Repair both and report FOUND versus CHANGED: this defect has
      already been repaired once at other sites and reappeared, so enumerate the
      constant by operand across the tree rather than fixing the two named here
      and stopping.

      FOUND versus CHANGED: the operand sweep found 39 `kMaxResonantBumpHeight`
      hits across 7 files, of which 4 comment sites stated the ceiling as a
      numeral — three in `app/FroggersAppCore.hpp` (the `peak.SetHeight` block,
      the output-limiter headroom bound, and the headroom figure that followed
      from it) and one in `app/dsp/Drive.hpp` — and all 4 were changed to read
      the constant; no numeral form of the ceiling survives in `app/`,
      `MANUAL.md` or `QUICK_DICT.md`.
- [x] 2.7 BOTH documents carry "up to about +9.5 dB (3×)", which describes the
      resonant bump read in isolation and is false of the output — that gain
      never reaches it. `MANUAL.md` and `QUICK_DICT.md` each carry it, and no
      task in the superseded change touched the quickdict copy. Rewrite both to
      what the control does, which the measurements above fix: the level at the
      bump's own resonant frequency is flat across the whole travel to 0.00025
      dB, the level away from it falls 6.04 dB at 1 kHz and 6.52 dB at 5 kHz,
      and total broadband level falls 7.10 dB end to end. The control raises the
      peak relative to its surroundings by attenuating the surroundings. Say
      that, and say what it costs, because the requirement this change promotes
      demands the remaining cost be stated where the control is described.
- [ ] 2.8 Stage gate: full suite green, then commit and push.

## Stage 3 — one definition for the cross-feed, and Reverb's slot 7

These share a new `app/dsp/StereoField.hpp`, `app/dsp/Reverb.hpp`,
`app/dsp/Delay.hpp`, `app/FroggersDspParityTests.cpp`,
`app/FroggersParameters.hpp`, `app/FroggersUiSurface.hpp`,
`app/FroggersSurfaceTests.cpp`, `MANUAL.md` and `QUICK_DICT.md`, so they are one
stage and run in order.

**This stage cannot start until the stage before it has committed and pushed.**
Six of its ten tasks write `app/FroggersDspParityTests.cpp`, which the previous
stage's work also writes. That is not a scheduling preference: two executors in
one file is the collision the staging exists to prevent.

- [ ] 3.1 Enumerate the cross-feed by OPERAND across the whole tree and report
      FOUND versus CHANGED with a disposition per hit, zeros included.
      THE OPERAND IS THE PAIRED WEIGHTING, NOT `* 0.5f`. Search for two line
      reads combined as `(1.0f - w)` against `w`, or for a local named `cross`.
      A bare `0.5f * (a + b)` mono sum is NOT a member: `dsp::Reverb::Process`
      alone carries three of those — the input mono sum, the mid for the width
      blend, and the wet-authority target — and `dsp::StereoDelay::ToStereo`
      and `ToReverbMono` carry more. An enumeration by the `* 0.5f` shape
      returns all of them and buries the four real hits.
      DISPOSITIONS ALREADY SETTLED, so the executor does not have to guess:
      `dsp::StereoDelay::Process` and `dsp::Reverb::Process` are the two
      production copies and are the de-duplication's subject.
      `src/core/FroggersEngine.hpp`'s copy in `ProcessReverb` is OUT OF SCOPE —
      frozen firmware, and the app tree's port is a sanctioned copy that
      `app/Makefile` forbids including from. Leave it and say so.
      `stereo_delay_width_balance_mapping_keeps_cross_in_0_1_and_spread_at_or_below_todays_max`
      is KEEP, not collapse: it re-derives the weight by hand and never calls
      `dsp::StereoDelay::Process`, which is what makes it an independent oracle
      for a promoted requirement. Collapsing it into the shared helper turns
      production into its own witness and the requirement loses its check while
      the suite stays green.
- [ ] 3.2 One definition, in a NEW `app/dsp/StereoField.hpp`. Not
      `app/dsp/Limiter.hpp`: following `dsp::EqualPowerWetDry`'s precedent by
      destination rather than by shape would put a cross-feed and an allpass
      cascade in a file named for a limiter, which is the defect this change
      exists to fix.
      Name it `dsp::CrossFeedPair`, taking the two channel values and a cross
      weight in [0, 0.5] and returning the crossed pair, identity at weight 0.
      REVERB'S CALL PASSES ITS TWO LINE READS TRANSPOSED. Delay computes
      `fbL = dL*(1-c) + dR*c`, which is identity at c == 0. Reverb computes
      `aFb = valB*(1-c) + valA*c`, which at c == 0 is a full SWAP: line A is fed
      from line B's tap. The two are the same weighted average with transposed
      operands, so the natural call — `CrossFeedPair(valA, valB, c)` — is a
      DIFFERENT TANK at every knob position including the registered default,
      and it is a silent audio change.
      ASSERT BIT-IDENTITY AT BOTH CALL SITES across the de-duplication, against
      the parity replicas, BEFORE those replicas are touched. The one test that
      would otherwise catch this,
      `reverb_process_matches_manual_tank_replica_at_neutral_mod_and_hold`,
      carries its own inline copy of the formula, and a later task tells the
      same executor to update the parity-side copies — both sides edited by one
      hand detects nothing.
      Reverb's own coefficient scale at its call site is `kTankCrossFeedScale`,
      defined beside the call in `app/dsp/Reverb.hpp`. Delay's `0.5f` STAYS a
      literal and is not folded into that constant: Delay's is half of a width
      blend that a separate balance scalar then multiplies, Reverb's bounds a
      tank cross. Two quantities that happen to share a value. Say so at both
      sites rather than leaving the asymmetry unexplained.
- [ ] 3.3 Move `dsp::DelayDiffuser` into the same header together with
      `dsp::SchroederAllpassSection`, which it holds, so the header does not
      depend back on `app/dsp/Delay.hpp`. Moving one without the other creates a
      cycle; moving only the section leaves Reverb to re-derive the constants,
      which reschedules the duplication rather than removing it.
      `kDiffusionCoeffScale` STAYS on `dsp::StereoDelay`: it is the weight Delay
      applies when DRIVING the cascade, not a constant the cascade holds.
      ASSERT that Delay's existing diffusion is BIT-IDENTICAL across the move —
      it is a relocation, and its own parity pins prove it.
      THE INBOUND HALF REACHES PAST `app/dsp/Delay.hpp`. Comments there refer to
      `DelayDiffuser` as a neighbour in the same file, and
      `app/FroggersDspParityTests.cpp` cites the `dsp/Delay.hpp` path beside the
      two moving symbols. Enumerate rather than trusting a count — and do not
      enumerate by symbol name alone: at least one breaking site names neither
      symbol on the line that breaks, referring to them only positionally. Grep
      the PATH as well as the names, and repair each with a stated disposition.
- [ ] 3.4 MEASURE FIRST, read-only. Both cross-feeds are the same weighted
      average in isolation, so the direction each knob moves comes entirely from
      what surrounds them: on the Delay page the same knob also drives an L/R
      time offset, and that pairing — not the cross-feed — is what the widening
      claim rests on. Nothing in the tree settles this by reading.
      Measure L/R correlation of the wet leg across Delay's Stereo width travel
      and across Reverb's slot 7 travel, through the production routers at the
      registered defaults, with the grid and tap point stated.
      FIVE KNOBS DEFAULT TO 0.0 AND EACH KILLS THIS MEASUREMENT DIFFERENTLY.
      Reverb Send and Delay Send leave their wet legs unfed and the probe reads
      `nan` — loud failures. Reverb Wet/dry and Delay Wet/dry leave the wet leg
      out of the app's output. The dangerous one is REVERB'S STEREO WIDTH,
      also 0.0: `wetL = mid + width*(aOut - mid)` makes `wetL == wetR` EXACTLY,
      so correlation reads a flat +1 across the whole travel — a plausible
      number from a dead rig, not a `nan`. Name every knob you move off its
      default and state why. A flat +1 row is VOID, not negative.
      MEASURE BOTH SLOTS THE LATER TASKS GATE ON: slot 7's travel AND slot 6's,
      since the task that folds the cross-feed under Stereo width asserts about
      slot 6 and this measurement is its only baseline.
- [ ] 3.5 DROPPED — the premise is false, and what replaced it is below.
      Folding the cross-feed under Stereo width was to make one knob move the
      stereo image coherently. Two findings kill it. Published practice keeps
      the two independent: Dattorro's cross-feed is a fixed figure-eight with no
      knob and stereo comes from the output tap structure, Schroeder uses an
      output mixing matrix, Freeverb has no cross-feed at all, and Jot-style
      networks put mixing in the feedback matrix and width in the output matrix.
      No surveyed design ties them in either direction.
      And the knob barely moves the quantity anyway. Measured on a replica of
      the tank's linear core, the cross weight moves inter-aural correlation by
      0.0066 across its whole travel, because correlation is pinned near 0.947
      by something else entirely — see below. Folding a control that moves
      0.0066 under another control delivers nothing and costs the tank half its
      state at the top of the travel, which is a density loss rather than a
      width gain.
      ASSERT both pages' Stereo width move L/R correlation monotonically in the
      SAME direction, measured in one run.
      THE DELAY HALF MAY NOT BE MONOTONIC AND THIS CHANGE CANNOT FIX IT.
      Delay's `p.dwid` drives `widthSpread` and `cross` from one knob in
      OPPOSING directions, so its correlation curve can turn. The stop
      condition therefore covers monotonicity as well as sense: REPORT AND STOP
      if the measured Delay row is non-monotonic on the stated grid, not only if
      its direction contradicts. Name the grid and the tolerance in the task
      before dispatch — a monotonicity claim without a tolerance is decided by
      whatever floating-point noise the executor happens to see.
- [ ] 3.5a WHAT ACTUALLY PINS REVERB'S STEREO IMAGE, and it is neither knob.
      `dsp::Reverb::Process` calls `dampFilter.Process(valA)` and then
      `dampFilter.Process(valB)` on ONE filter with ONE recursive state, so line
      B's output is `(1 - alpha) * aOut + alpha * valB` — B is mixed with A by
      the filter's own memory. Measured on the replica, correlation tracks
      `1 - alpha` and therefore follows the DAMPING knob: 0.838, 0.948, 0.983 at
      damping 0, 0.5 and 1.0. Splitting into per-line filters drops it to 0.196.
      The file header records the shared filter as a verbatim port from the
      firmware original; it does not record that the sharing is what sets the
      stereo image.
      This is a label-versus-mechanism defect of the same family as the rest of
      this change: a control named Damping moves the stereo width, and the
      controls named for the stereo field do not.
      VERIFY IT AGAINST PRODUCTION FIRST. The figures above come from a replica
      of the tank's linear core, not from `dsp::Reverb::Process` itself. Confirm
      through the production router before any code moves, and if production
      disagrees the replica is the defect.
      SPLITTING THE FILTER CHANGES THE REVERB'S SOUND at every setting, so it is
      not folded into this stage silently. Measure, record, and decide with the
      numbers.

- [ ] 3.6 MEASURE FIRST, read-only. Pin today's slot 7 against slot 6 on L/R
      correlation of the wet leg, so the replacement has something to turn green.
      TWO DEAD INSTRUMENTS. Reverb's Send default is 0.0,
      where the tank is never fed and the probe reads `nan` — a VOID run, not a
      result. The sharper one is Stereo width, also 0.0: there the wet leg is
      exactly mono and slot 7's row reads a flat +1 whatever the tank does, so a
      pin written at that width can neither turn green nor turn red. HOLD WIDTH
      OFF ITS DEFAULT while sweeping slot 7 and say at what value, and name
      every knob you move.
      ASSERTION, once this reports: the check pins BOTH travels in one case,
      with its threshold and grid taken from what this measurement returns and
      written into the task before the implementing task is dispatched. A
      "near-zero row" is a blank until this reports the number; the row is
      meaningful only because the other row moves in the same run.
- [ ] 3.7 Replace the cross-feed behind slot 7 with `dsp::DelayDiffuser`
      UNCHANGED, at its own section lengths rather than retuned for the tank.
      PLACEMENT: THE INPUT PATH, ahead of the tank, not inside the loop and not
      on the wet output. This is Dattorro's input-diffusion role — decorrelate
      the incoming signal before it reaches the tank, which is what stops tank
      recirculation becoming audible as cyclic events on percussive material.
      In-loop placement is the other established role and is stable in principle
      — an allpass has unit gain, so the loop's stability condition is unchanged
      provided each section's coefficient magnitude stays below 1 — but the risk
      there is coloration rather than instability, and it compounds: every trip
      around the loop passes the cascade again. This cascade's sections are
      4.7, 12.3 and 21.1 ms, and two of the three exceed the length below which
      allpass diffusion is usually described as colourless. Colouring once on
      the way in is the bounded version of that cost, and it is the one this
      control is for. Output placement is rejected outright: it cannot break up
      a recirculation pattern that is already in the signal.
      FOUR INTEGRATION DUTIES, each already a fixed defect elsewhere in this
      tree, which is why they are named rather than left to be rediscovered:
      the diffuser's state is cleared by `Reset()`; it is visited by
      `StateFinite()` and by `StateMagnitude()`; its sample rate arrives through
      the same `Configure()` path that reaches `SetSampleRate`; and it RUNS
      UNCONDITIONALLY with only its OUTPUT branched — `dsp::StereoDelay`'s own
      call site records why, having once written the zero case as a branch
      around the call and left the cascade's state frozen while the knob sat at
      zero, so the first move off zero replayed stale content.
      `app/dsp/Reverb.hpp`'s FILE HEADER names the slot and goes false on the
      rename. It is in the repair list with a stated disposition.
      Rename the parameter to `Density`, short name `Dens`, in the parameter
      table and in BOTH label tables — `FroggersBankLayouts()` in
      `app/FroggersParameters.hpp`, `FroggersApprovedLabels()` in
      `app/FroggersUiSurface.hpp`, and the deliberately independent second copy
      inside `every_rendered_label_matches_the_approved_list_verbatim` in
      `app/FroggersSurfaceTests.cpp`. The second copy is independent on purpose,
      so a rename reaching only one turns the surface gate red.
      TWO COLLISIONS MAKE A BLIND REPLACE WRONG, and both are the DELAY page's
      own Diffusion, which this change does NOT rename: `diffusionKnob01` is one
      spelling for two unrelated parameters across FOUR bindings, not two.
      Production: `dsp::Reverb::Process`'s, the rename target, and
      `dsp::MapRowsToDelayParams`'s, which carries Delay's knob. Test side:
      `UnsaturatedTankReplica`'s `Step` and a second replica's `Step` beside it,
      both mirroring the Reverb parameter and both going stale the moment
      production's does. Rename by qualified symbol, never by the bare
      identifier, and report a disposition for all four. And
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
      THE METRIC IS THE NORMALISED ECHO DENSITY PROFILE of Abel and Huang:
      over a sliding 20-30 ms Hanning-weighted window, the fraction of impulse
      response samples whose magnitude exceeds that window's standard
      deviation, divided by `erfc(1/sqrt(2))` which is about 0.3173 so that
      Gaussian-dense output reads 1. It runs from 0 at sparse to about 1 at
      fully dense, rises monotonically with diffusion, needs one pass and no
      FFT, and is insensitive to level, equalisation, decay time and sample
      rate. ASSERT it rises monotonically with Density, with the grid and the
      tap point stated.
      SPECTRAL FLATNESS IS REJECTED AND MUST NOT BE SUBSTITUTED. An allpass
      changes no magnitude spectrum, so a flatness measure is constant by
      construction against the parameter under test — a check that cannot fail,
      which is the defect this change spends its length hunting. Kurtosis is
      rejected too: it has poor dynamic range and throws false peaks exactly at
      low density, which is the regime a diffusion sweep starts in.
      TWO DEAD INSTRUMENTS, and the second is sharper than the first. An
      impulse that never reaches the tank produces a flat row that passes while
      measuring nothing, so assert in the SAME case that the impulse response
      carries energy at Density's floor. But an UNCONFIGURED `dsp::DelayDiffuser`
      passes that check and still measures the wrong mechanism: with its
      sections left at a one-sample delay it is a phaser, which its own header
      warns about, and it carries energy happily. Assert in the same case that
      every section's configured delay is longer than one sample at the
      measurement's sample rate, or the run is VOID.
- [ ] 3.10 Stage gate: full suite green, then commit and push.

## Stage 4 — the documents

House style: plain present tense saying what a control does; no jokes; no
defining by negation; no history the reader never saw; and no task numbers,
change names, planning-document references or rule citations in comments, test
names or strings.

- [ ] 4.1 `MANUAL.md` and `QUICK_DICT.md` for the two Filter controls and both
      pages' Diffusion, Density and Stereo width. The Drive controls are already
      updated and are not re-done.
- [x] 4.2 State what each control now does, which are conditional and on what,
      and — for Fuzz and Symmetry — which settings make another control inert.
      State the Fuzz-maximum case honestly: with the blend floored, Fold still
      moves the balance BETWEEN harmonics there while total level barely moves.
      Do not call it tilt: measured across a Gain by Shape grid at Fuzz
      maximum there is no monotone spectral slope, and which harmonic moves is
      set by Gain and Shape. "Tilt" names a slope that is not there, and it
      collides with Reverb's own Tilt control. Name the quantity that actually
      moves — the balance between harmonics, against a total level that barely
      moves — and state a figure only where a check produces it. A manual saying
      "Fold still works there" without naming the quantity replaces a silent
      defect with a misleading sentence.

      DONE. Both documents state the floored blend, what Fuzz holds back at each
      extreme, and that Fold is quieter at Fuzz maximum without going inert. The
      per-harmonic and total-level figures this task quotes are NOT stated: no
      case measures harmonics at Fuzz maximum, so the behaviour is described
      without them rather than carrying a figure nothing produces.
- [ ] 4.3 State on BOTH the Delay and Reverb pages what Diffusion and Density
      each mean, so the difference is on the page rather than in the reader's
      memory.
- [x] 4.4 Rewrite the cross-instrument Feedback section affirmatively. Several
      of its sentences define a control by what it is not, including two Delay
      entries opening "not a feedback path itself" and a closing line saying
      Reverb has no control named Feedback. Say what each control does, and say
      what Reverb's tank uses instead.

      DONE. All five entries say what their control does, both "not a feedback
      path itself" openings are gone, and the section closes with Reverb's tank
      running on one coefficient that Decay places between 0.1 and 0.98 and Hold
      moves toward 1.
- [x] 4.5 The Anti-alias entry does not carry the qualification its own scenario
      names: the no-dip bound is met at the bank's registered defaults and is
      not claimed everywhere, and away from those defaults the dip is larger at
      some Gain and Shape settings. Say so.

      DONE for the qualification: both documents now attach the 0.6 dB figure to
      the bank's registered defaults instead of claiming it everywhere. The
      larger dip away from those defaults is NOT stated — the only case that
      measures the dip runs at the registered defaults, so nothing in the tree
      backs it.
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
