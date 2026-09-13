# Tasks — one definition for the stereo field, and Reverb's slot 7

**Work not listed in this file does not become a task unless the operator adds
it.**

Ticked tasks are delivered and committed. Each carries its OUTCOME in one or two
sentences; how it was arrived at belongs to the session that did it. Do not
re-do them.

## How this change is sequenced, and why

**Every stage ends in a commit and a push.** Earlier changes in this chain held
four sessions of green work in one uncommitted tree, so no progress was banked
and every fresh context re-audited the whole surface from nothing. A stage that
is green is delivered before the next begins.

**Every commit stages the paths it names and nothing else.** `git add -A`,
`git add .` and `git commit -a` are forbidden here for a specific reason:
`openspec/changes/frogg3rs-midi-controller-resilience/` is untracked in this
tree, so any blanket stage sweeps a change the operator is deliberately holding
onto `main`, and there is no pull request in this repository to catch it. Check
`git status --short` before every commit and confirm that directory is still
untracked afterwards. No AI attribution appears in any commit message.

Within a stage, tasks naming disjoint files may run together. Six of the live
tasks write `app/FroggersDspParityTests.cpp`; two executors in one file is the
collision the staging exists to prevent.

## How every figure in this change is produced

- Measure through the PRODUCTION ROUTER, naming every knob moved off its
  registered default and why. A hand-configured DSP object is not the
  instrument and the divergence is silent.
- **THE REVERB BANK'S REGISTERED DEFAULTS ARE A DEAD INSTRUMENT, so "measure at
  the registered defaults" is not available on that page and no task may ask
  for it.** Read from `app/FroggersParameters.hpp`: the Reverb rows for Wet/dry,
  Send and Stereo width each omit the third field and so take
  `FroggersParamSpec`'s own `0.0f`. Send at 0 leaves the tank unfed. Wet/dry at
  0 leaves the wet leg out of the app's output. Stereo width at 0 makes `wetL`
  and `wetR` bit-equal, because `dsp::Reverb::Process` computes
  `wetL = mid + width * (aOut - mid)` and the same for `wetR`. A correlation
  probe on that pair reads a flat +1 whatever the tank does. The Delay rows for
  Wet/dry, Send and Stereo width omit it too. Every Reverb and Delay
  measurement therefore states its own operating point, knob by knob, and a
  row that reads flat +1 or `nan` is VOID rather than negative. `RouteFilterBank`
  and `RouteDriveBank` are private members of `FroggersAppCore` in
  `app/FroggersAppCore.hpp` — not of `FroggersApp` in `app/Froggers.hpp`, which
  derives from it and declares neither — so a test reaches them either through
  the app's public entry or through a replica mirroring the router's setter
  order. `ProcessDriveBank` in `app/FroggersDspParityTests.cpp` is the existing
  precedent for the Drive bank; `filter_bank_peak_gain_travel_measurement_at_and_away_from_resonance`
  in the same file is now the precedent for the Filter bank.
- A figure lands in a TEST, never in prose.
- State the GRID and the TAP POINT beside any worst-case figure. Where the claim
  is about an improvement, measure both states in ONE run over ONE grid and
  assert the comparison.
- **A figure inherited from a finished session is not an acceptance criterion.**
  Nothing in this file gates on one.
- Carry the liveness control inside the fixture and never let it be the whole of
  a case.
- Every task producing a check states what the check must ASSERT. A task that
  does not is defective — report it rather than choosing a threshold.

## Stage 0 — baseline

- [x] 0.1 Baseline the suite by running `nice -n 10 make -C app test -j2` and
      then every binary in `app/build/` by path, because the recipe stops at its
      first failure. OUTCOME: the recipe reached its last target at exit 0 and
      all twelve binaries then ran by path at exit 0.
- [x] 0.2 Bank the inherited work before this change begins. OUTCOME: the
      delivered code, gate and document work is committed and pushed; the local
      `openspec/changes/archive/` is excluded by `.gitignore` deliberately and
      is not committable.

## Stage 1 — the gates that accepted what they reject

- [x] 1.1 Repair `app/check_spec_checks_resolve.py`, which paired a cited case
      against a cited file only for `app/`-owned paths. OUTCOME: a `Check:` now
      resolves only by naming a real case, a file that defines the case it is
      paired with, a gate the `test` target runs, or a declared-manual marker,
      and no name is read out of a comment. Widening the recogniser made 72 real
      tests in two files visible that no earlier version could see.
- [x] 1.2 Repair `app/check_modified_requirements_restate_promoted.py`, which
      let one generic fragment absorb every dropped promoted clause. OUTCOME: a
      declared fragment now covers a dropped clause only where it occurs in
      exactly one, and scenarios pair by clause overlap rather than by title.
      Binding the kept text to the bullet that replaced the dropped clause is
      NOT attempted and is recorded in the script's header as infeasible under
      the present declaration syntax.
- [x] 1.3 One adversarial pass over both repaired scripts, by a context that did
      not repair them. OUTCOME: the pass is spent and what got through is
      recorded in each script's own header. Two holes defeated a gate's purpose
      and are closed — a scenario retitled by one comma exempting every bullet
      under it, and a case name appearing only in a comment being indexed as a
      real test. The rest are recorded and left alone.
- [x] 1.4 Stage gate. OUTCOME: suite green; committed and pushed.

## Stage 2 — Peak gain

- [x] 2.1 Measure Peak gain's travel through a Filter-bank replica mirroring
      `RouteFilterBank`'s setter order, at the bank's registered defaults.
      OUTCOME: the level at the bump's own resonant frequency is flat across the
      whole travel and the level away from it falls at every step.
- [x] 2.2 CLOSED NEGATIVE — no law shipped, `app/dsp/FilterFx.hpp` unchanged.
      OUTCOME: no makeup at this placement can hold level, because it sits ahead
      of a limiter whose response is strictly increasing in its argument, so
      every makeup above rounding raises the output ceiling. Removing the trim
      instead was measured and rejected: the output stays bounded without it but
      the extra limiter duty costs audible distortion, intermodulation and
      syllabic-rate pumping at the bank's own registered defaults.
- [x] 2.3 SUPERSEDED — 2.2 shipped no second law, so a comparison between two
      laws has nothing to compare. OUTCOME: the surviving check pins the pairing
      instead, and its thresholds are anchored to that run's own grid.
- [x] 2.4 MEASURED, result recorded under 2.2. OUTCOME: the blowout the trim was
      added for is not reachable from the knobs, because `dsp::Comb::GetFeedback`
      in `app/dsp/FilterFx.hpp` caps the feedback magnitude deliberately, so the
      measurement pins the loudest reachable state and says so.
- [x] 2.5 VERIFIED. OUTCOME: Peak gain's registered default leaves the trim at
      unity and no multiply was added, so today's output is reproduced exactly.
- [x] 2.6 Fix the stale ceiling comments. OUTCOME: every comment site stating the
      resonant-bump ceiling as a numeral now reads `dsp::kMaxResonantBumpHeight`;
      the sweep was run by operand across the tree rather than at the two sites
      originally named.
- [x] 2.7 Rewrite both documents' Peak gain entries. OUTCOME: `MANUAL.md` and
      `QUICK_DICT.md` state that the control raises the peak relative to its
      surroundings by attenuating the surroundings, and state the cost, rather
      than quoting the bump's own centre gain as if the output reached it.
- [x] 2.8 Stage gate. OUTCOME: suite green; committed and pushed.

## Stage 3 — one definition for the cross-feed, and Reverb's slot 7

These share NEW `app/dsp/StereoField.hpp`, `app/dsp/Reverb.hpp`,
`app/dsp/Delay.hpp`, `app/FroggersDspParityTests.cpp`,
`app/FroggersParameters.hpp`, `app/FroggersUiSurface.hpp`,
`app/FroggersSurfaceTests.cpp`, `MANUAL.md` and `QUICK_DICT.md`, so they are one
stage and run in order.

- [ ] 3.0 Repair the stale `ToReverbMono` citations before anything reads them
      again. The symbol has no definition anywhere in the tracked tree; it is
      the pre-port simulator's name for work now done elsewhere. It is named as
      a live symbol at `app/dsp/Delay.hpp:436`, `:970` and `:1011`, at
      `app/FroggersAudioRoutingTests.cpp:2360`, and in
      `app/check_artifact_symbols_resolve.py`'s own header, where it serves as
      that gate's example of what counts as code.
      This is §8.0 hygiene on the change's own path, and it is sequenced first
      because those comments are the GENERATOR: task 3.1 named the symbol as
      live purely because `app/dsp/Delay.hpp` still does. Repairing the task
      while leaving the comments would reschedule the same defect.
      Each site names the symbol that does the work today, with a stated
      disposition. The wet-level follower's mono sum is
      `AdvanceWetLevel(std::fabs((lastWet.l + lastWet.r) * 0.5f))` inside
      `dsp::StereoDelay::Process`; `dsp::StereoDelay::ToStereo` carries no mono
      sum at all. Do not invent a replacement name where reading does not
      settle which symbol a comment meant — report that site and stop.
      Grep the bare word case-insensitively across `app/`, the root documents
      and `openspec/`, and report FOUND versus CHANGED. `openspec/changes/archive/`
      is a historical record and is excluded.
      NOTHING ELSE BELONGS IN THIS TASK. It is the one hygiene item that BLOCKS
      3.1, and the rest of the stale-citation sweep is 3.0a, which blocks
      nothing. Bundling them under one checkbox would leave 3.1's readiness
      ambiguous if this stalls halfway.
- [ ] 3.0a The rest of the stale-citation sweep. Nothing in this change depends
      on it, so it runs whenever the stage has room, and a stall here does not
      hold 3.1.
      TWO CITED PATHS NO LONGER RESOLVE, and they take OPPOSITE dispositions.
      §8.0's fork is restore-or-remove, and restoration is correct only where
      the consumer outlives the thing removed — a claim to verify per file, not
      a rule to apply to both.
      `openspec/specs/field-button-input-latency/spec.md` says hardware
      diagnostics "are recorded in `docs/daisy-field-diagnostics.md`" in the
      present tense; there is no `docs/` directory. That file went out at
      `b9a8199`, "ship from app/, and retire the trees that no longer ship",
      as collateral of retiring the trees that stopped shipping rather than
      because Daisy diagnostics were being cleared. The content survives: the
      `SW1` stuck-input entry the clause points at is alive under
      Troubleshooting in `DAISY_MANUAL.md`. REDIRECT the citation there. Do not
      delete it — the exception clause is still open and the fact is still
      locatable, and dropping the pointer throws both away.
      `app/FroggersModulationTests.cpp` cites `UPSTREAM-SHEAF-ASK.md`, deleted
      at `e027e5b`, "Clear correspondence and scratch from the repository root",
      which is a content-specific clearing that names its successor. REMOVE the
      pointer, keep whatever the comment still asserts truly.
      FOUR COMMENTS CITE A TRUNCATED TEST NAME, each of which does not exist as
      written. `filter_fx_chain_scoop_full_does_not_cancel` does not exist and
      `topology_morph_peak_branch_headroom` does not exist; both are at
      `app/FroggersDspParityTests.cpp:3193-3194`, and the full names are
      `filter_fx_chain_scoop_full_does_not_cancel_a_boosted_peak_at_the_shared_center_frequency`
      and `topology_morph_peak_branch_headroom_across_full_range`.
      `randomize_all_storm_test_never_blows_out` does not exist, at
      `app/FroggersAudioRoutingTests.cpp:2800`; the full name is
      `randomize_all_storm_test_never_blows_out_or_permanently_silences`.
      `fuego_seam_transform` does not exist, at
      `app/FroggersParameterModelTests.cpp:461` and `:506`; the full name is
      `fuego_seam_transform_reaches_cached_knob_value_matching_dsp_stack`.
      Spell each name in full so a resolver finds it, and replace the positional
      words beside them — "above", "idiom above" — with the name, since a
      relative pointer decays on the next edit to the file.
      TWO COMMENTS NAME CONSTANT FAMILIES THAT ARE NOT DECLARED WHERE THEY ARE
      CITED. In `app/dsp/Limiter.hpp`, `kThreshold` does not exist, `kCeiling`
      does not exist, `kHeadroom` does not exist, `kAttackSeconds` does not
      exist and `kReleaseSeconds` does not exist, yet a comment names all five
      as symbols that "were `static constexpr`". That file's real constants are
      `kDefaultThreshold`, `kDefaultCeiling`, `kDefaultAttackSeconds`,
      `kDefaultReleaseSeconds`, `kStageCeiling` and `kSharedCeiling`. The
      sentence also narrates a refactor the reader never saw, which this
      change's house style forbids, so say what the constants are and why they
      are per-instance rather than what they used to be.
      In `app/FroggersTransferFunctionVisualizer.hpp`, `kMinDb` does not exist
      and `kMaxDb` does not exist, yet a comment cites that pair as the clamp
      window. Name the real bound or state the numbers.
- [ ] 3.1 Enumerate the cross-feed by OPERAND across the whole tree and report
      FOUND versus CHANGED with a disposition per hit, zeros included.
      THE OPERAND IS THE PAIRED WEIGHTING, NOT `* 0.5f`. Search for two line
      reads combined as `(1.0f - w)` against `w`, or for a local named `cross`.
      A bare `0.5f * (a + b)` mono sum is NOT a member: `dsp::Reverb::Process`
      alone carries three — the input mono sum, the mid for the width blend, and
      the wet-authority target — and `dsp::StereoDelay::Process` carries a
      fourth, the wet-level follower's argument. An enumeration by the `* 0.5f`
      shape returns all four and buries the real hits.
      DO NOT SEARCH FOR `ToReverbMono`. It has no definition anywhere in the
      tree and survives only in stale comments, which this stage repairs under
      3.0. An earlier version of this task named it as a live symbol.
      DISPOSITIONS ALREADY SETTLED, so the executor does not guess:
      `dsp::StereoDelay::Process` and `dsp::Reverb::Process` are the two
      production copies and are the de-duplication's subject.
      `src/core/FroggersEngine.hpp`'s copy inside `ProcessReverb` is OUT OF
      SCOPE — frozen firmware, and the app tree's port is a sanctioned copy that
      `app/Makefile` forbids including from. Leave it and say so.
      `stereo_delay_width_balance_mapping_keeps_cross_in_0_1_and_spread_at_or_below_todays_max`
      in `app/FroggersDspParityTests.cpp` is KEEP, not collapse: it re-derives
      the weight by hand and never calls `dsp::StereoDelay::Process`, which is
      what makes it an independent oracle for a promoted requirement. Collapsing
      it turns production into its own witness and the requirement loses its
      check while the suite stays green.
- [ ] 3.2 One definition, in a NEW `app/dsp/StereoField.hpp`. Not
      `app/dsp/Limiter.hpp`: following `dsp::EqualPowerWetDry`'s precedent by
      destination rather than by shape would put a cross-feed and an allpass
      cascade in a file named for a limiter, which is the defect this change
      exists to fix.
      Name it `dsp::CrossFeedPair`, taking the two channel values and a cross
      weight in [0, 0.5] and returning the crossed pair, identity at weight 0.
      REVERB'S CALL PASSES ITS TWO LINE READS TRANSPOSED, and this is read from
      the tree rather than assumed. `dsp::StereoDelay::Process` computes
      `fbL = dL * (1.0f - cross) + dR * cross`, which is identity at zero.
      `dsp::Reverb::Process` computes `aFb = valB * (1.0f - cross) + valA * cross`,
      which at zero is a full SWAP: line A is fed from line B's tap. The two are
      the same weighted average with transposed operands, so the natural call
      would be a DIFFERENT TANK at every knob position including the registered
      default, and it is a silent audio change.
      ASSERT BIT-IDENTITY AT BOTH CALL SITES across the de-duplication, against
      the parity replicas, BEFORE those replicas are touched. The one test that
      would otherwise catch this,
      `reverb_process_matches_manual_tank_replica_at_neutral_mod_and_hold`,
      carries its own inline copy of the formula, and a later task tells the
      same executor to update the parity-side copies — both sides edited by one
      hand detects nothing.
      Reverb's own coefficient scale at its call site becomes a NEW
      `kTankCrossFeedScale`, defined beside the call in `app/dsp/Reverb.hpp`.
      Delay's `0.5f` STAYS a literal and is not folded into that constant:
      Delay's is half of a width blend that a separate balance scalar then
      multiplies, Reverb's bounds a tank cross. Two quantities sharing a value.
      Say so at both sites rather than leaving the asymmetry unexplained.
- [ ] 3.3 Move `dsp::DelayDiffuser` into the same header together with
      `dsp::SchroederAllpassSection`, which it holds, so the header does not
      depend back on `app/dsp/Delay.hpp`. Both are declared in that file today.
      Moving one without the other creates a cycle; moving only the section
      leaves Reverb to re-derive the constants, which reschedules the
      duplication rather than removing it.
      `kDiffusionCoeffScale` STAYS on `dsp::StereoDelay`: it is the weight Delay
      applies when DRIVING the cascade, which `ApplyDiffusion` in
      `app/dsp/Delay.hpp` shows directly, not a constant the cascade holds.
      ASSERT that Delay's existing diffusion is BIT-IDENTICAL across the move —
      it is a relocation, and its own parity pins prove it.
      THE INBOUND HALF REACHES PAST `app/dsp/Delay.hpp`. Comments there refer to
      `DelayDiffuser` as a neighbour in the same file, and
      `app/FroggersDspParityTests.cpp` cites the `dsp/Delay.hpp` path beside the
      two moving symbols. Enumerate rather than trusting a count — and not by
      symbol name alone: at least one breaking site names neither symbol on the
      line that breaks, referring to them positionally. Grep the PATH as well as
      the names, and repair each with a stated disposition.
- [ ] 3.4 MEASURE FIRST, read-only. Both cross-feeds are the same weighted
      average in isolation, so the direction each knob moves comes entirely from
      what surrounds them: on the Delay page the same knob also drives an L/R
      time offset, and that pairing — not the cross-feed — is what the widening
      claim rests on. Nothing in the tree settles this by reading.
      Measure L/R correlation of the wet leg across Delay's Stereo width travel
      and across Reverb's slot 7 travel, through the production routers at the
      registered defaults, with the grid and tap point stated.
      SEVERAL KNOBS CARRY NO EXPLICIT DEFAULT AND SO SIT AT 0.0, and each kills
      this measurement differently. This is read from `app/FroggersParameters.hpp`,
      where a bank row without a third field takes `FroggersParamSpec`'s own
      `0.0f`, and the Reverb and Delay rows for Wet/dry, Send and Stereo width
      all omit it. Reverb Send and Delay Send leave their wet legs unfed and the
      probe reads `nan` — loud failures. Reverb Wet/dry and Delay Wet/dry leave
      the wet leg out of the app's output. The dangerous one is REVERB'S STEREO
      WIDTH: `app/dsp/Reverb.hpp` computes `wetL = mid + width * (aOut - mid)`
      and the same for `wetR`, so at width 0 the two are EXACTLY equal and
      correlation reads a flat +1 across the whole travel — a plausible number
      from a dead rig, not a `nan`. Name every knob you move off its default and
      state why. A flat +1 row is VOID, not negative.
      MEASURE BOTH SLOTS THE LATER TASKS GATE ON: slot 7's travel AND slot 6's.
- [ ] 3.4a MEASURE FIRST, read-only, and it ships no code. THE PROMOTED SPEC
      CARRIES A FIGURE THAT MEASURES SOMETHING OTHER THAN WHAT IT SAYS, and
      this change's own dead-instrument finding is what exposes it.
      `openspec/specs/froggers-sheaf-parameter-model/spec.md`, under
      "An insert effect page's master returns the dry signal at its floor",
      states: "Measured on the Reverb bank with every control at its registered
      default and the master turned fully up, the output is 8.46 dB below its
      input and differs from it by -4.07 dB relative to that input." The VERY
      NEXT SENTENCE says Reverb and Delay "read as transparent at rest because
      their Sends default closed and their wet paths are empty". Both cannot
      describe the same run. With Send at its registered
      `0.0f` the tank is never fed, so the pair of figures cannot be the tank's
      character, which is the thing the surrounding prose uses them to
      establish.
      REPORT WHICH QUANTITY REPRODUCES 8.46 dB AND -4.07 dB. Per §6.1 a
      disagreement is not a finding: identify what the original run actually
      measured — most likely the crossfade's own dry floor with an empty wet leg
      — rather than stopping at "my number differs". Then measure the quantity
      the prose CLAIMS, with Send and Wet/dry open, stating every knob's value.
      Report both, with the grid and the tap point.
      This task does not edit the spec. 5.2a decides the restatement on what
      this returns, and the figures belong in a check rather than in prose.
- [ ] 3.5 DROPPED — folding the cross-feed under Stereo width rests on a false
      premise. Published practice keeps the two independent: Dattorro's
      cross-feed is a fixed figure-eight with no knob and stereo comes from the
      output tap structure, Schroeder uses an output mixing matrix, Freeverb has
      no cross-feed at all, and Jot-style networks put mixing in the feedback
      matrix and width in the output matrix. No surveyed design ties them.
      A second reason was recorded — that the cross weight barely moves
      correlation because something else pins it — and that figure came from a
      replica. It is INHERITED AND UNTRACED and this task does not rest on it;
      3.4 measures the quantity through production.
- [ ] 3.5a WHAT PINS REVERB'S STEREO IMAGE, and it is neither knob.
      TRACED AGAINST PRODUCTION: `app/dsp/Reverb.hpp` declares one
      `OnePoleLowPass dampFilter` and `dsp::Reverb::Process` calls
      `dampFilter.Process(valA)` and then `dampFilter.Process(valB)` on that one
      instance in the same sample. `OnePoleLowPass::Process` in
      `app/dsp/DspMath.hpp` is `output = alpha * input + (1.0f - alpha) * output`,
      one recursive state, so line B's output is
      `alpha * valB + (1.0f - alpha) * aOut` — B is mixed with A by the filter's
      own memory, and the Damping knob therefore sets how correlated the two
      lines are. The file header records the shared filter as a verbatim port
      from the firmware original; it does not record that the sharing is what
      sets the stereo image.
      THE FIGURES ARE NOT TRACED. Correlation values at damping 0, 0.5 and 1.0,
      and the value after splitting into per-line filters, came from a replica of
      the tank's linear core in a session that is over. MEASURE THEM AGAIN
      through the production router, with the grid and tap point stated and with
      Stereo width held off its 0.0 default for the reason 3.4 gives. Report and
      stop: this task ships no code.
      SPLITTING THE FILTER CHANGES THE REVERB'S SOUND at every setting, so it is
      not folded into this stage silently. Measure, record, and let the operator
      decide with the numbers.
      This is a label-versus-mechanism defect of the same family as the rest of
      this change: a control named Damping moves the stereo width, and the
      controls named for the stereo field move it less.
- [ ] 3.6 MEASURE FIRST, read-only. Pin today's slot 7 against slot 6 on L/R
      correlation of the wet leg, so the replacement has something to turn green.
      TWO DEAD INSTRUMENTS, both traced to the missing explicit defaults above.
      Reverb's Send sits at 0.0, where the tank is never fed and the probe reads
      `nan` — a VOID run, not a result. The sharper one is Stereo width, also
      0.0: there the wet leg is exactly mono and slot 7's row reads a flat +1
      whatever the tank does, so a pin written at that width can neither turn
      green nor red. HOLD WIDTH OFF ITS DEFAULT while sweeping slot 7, say at
      what value, and name every knob you move.
      ASSERTION, once this reports: the check pins BOTH travels in one case,
      with its threshold and grid taken from what this measurement returns and
      written into this task before the implementing task is dispatched. A
      "near-zero row" is a blank until this reports the number; the row is
      meaningful only because the other row moves in the same run.
- [ ] 3.7 Replace the cross-feed behind slot 7 with `dsp::DelayDiffuser`
      UNCHANGED, at its own section lengths rather than retuned for the tank.
      PLACEMENT: THE INPUT PATH, ahead of the tank, not inside the loop and not
      on the wet output. This is Dattorro's input-diffusion role — decorrelate
      the incoming signal before it reaches the tank, which is what stops tank
      recirculation becoming audible as cyclic events on percussive material.
      In-loop placement is the other established role and is stable in principle,
      since an allpass has unit gain and the loop's stability condition is
      unchanged while each section's coefficient magnitude stays below 1, but the
      risk there is coloration rather than instability and it compounds: every
      trip around the loop passes the cascade again. This cascade's sections are
      4.7, 12.3 and 21.1 ms — read from `kSection1BaseSeconds`,
      `kSection2BaseSeconds` and `kSection3BaseSeconds` in `app/dsp/Delay.hpp` —
      and two of the three exceed the length below which allpass diffusion is
      usually described as colourless. Colouring once on the way in is the
      bounded version of that cost, and it is the one this control is for.
      Output placement is rejected outright: it cannot break up a recirculation
      pattern already in the signal.
      FOUR INTEGRATION DUTIES, each already a fixed defect elsewhere in this
      tree, named so they are not rediscovered. `dsp::Reverb` declares `Reset()`,
      `StateFinite()`, `StateMagnitude()` and `Configure()` in
      `app/dsp/Reverb.hpp`, and the new member must be reached by all four: its
      state cleared by the first, visited by the second and third, and given its
      sample rate through the fourth. And it RUNS UNCONDITIONALLY with only its
      OUTPUT branched — `ApplyDiffusion` in `app/dsp/Delay.hpp` records at its
      own call site why, having once written the zero case as a branch around the
      call and left the cascade's state frozen while the knob sat at zero, so the
      first move off zero replayed stale content.
      `app/dsp/Reverb.hpp`'s FILE HEADER names the slot and goes false on the
      rename. Repair it with a stated disposition.
      Rename the parameter to Density, short name Dens, in the parameter table
      and in BOTH label tables — `FroggersBankLayouts()` in
      `app/FroggersParameters.hpp`, `FroggersApprovedLabels()` in
      `app/FroggersUiSurface.hpp`, and the deliberately independent second copy
      inside `every_rendered_label_matches_the_approved_list_verbatim` in
      `app/FroggersSurfaceTests.cpp`. That second copy is independent on purpose,
      so a rename reaching only one turns the surface gate red.
      TWO COLLISIONS MAKE A BLIND REPLACE WRONG, and both are the DELAY page's
      own Diffusion, which this change does NOT rename. `diffusionKnob01` is one
      spelling for two unrelated parameters across FOUR bindings, traced:
      `dsp::Reverb::Process`'s parameter in `app/dsp/Reverb.hpp`, which is the
      rename target; `dsp::MapRowsToDelayParams`'s parameter in
      `app/dsp/Delay.hpp`, which carries Delay's knob into the `ddif` field; and
      two parity-side replica `Step` signatures in
      `app/FroggersDspParityTests.cpp`, both mirroring the Reverb parameter and
      both going stale the moment production's does. Rename by qualified symbol,
      never by the bare identifier, and report a disposition for all four.
      The literal Diffusion and Diff is not unique at any label site or in either
      document: Delay's row sits beside Reverb's at every one. Change the Reverb
      row by its SLOT, and report the Delay row untouched at each site as an
      explicit disposition rather than by silence.
      THE DOC ROWS MOVE IN THIS SAME STEP: `app/check_docs_match_parameter_table.py`
      binds each manual and quickdict bold entry to the parameter table by name
      and slot, so renaming the code while the documents still say Diffusion
      fails the build.
- [ ] 3.7a THE PARITY REPLICAS THAT 3.2 PROMISED AND NO TASK OWNED. Task 3.2
      says "a later task tells the same executor to update the parity-side
      copies". Until this task existed, no task from 3.3 to 3.9 named them, so
      3.7 would have turned the suite red with nobody holding the repair and
      working rule 4 would have correctly stopped the executor mid-stage.
      TWO inline copies of the tank's cross-feed carry the formula themselves,
      both reading `aFb = valB * (1.0f - cross) + valA * cross`: the replica
      inside `reverb_process_matches_manual_tank_replica_at_neutral_mod_and_hold`
      and the one inside `UnsaturatedTankReplica`, both in
      `app/FroggersDspParityTests.cpp`. `PreFixReverbReplica::Step` in the same
      file is NOT a third copy — it passes `diffusionKnob01` down into
      `tank.Step(...)` and holds no formula — but its signature is one of the
      four `diffusionKnob01` bindings 3.7 renames, so it gets its own stated
      disposition rather than silence.
      THE FORK EACH REPLICA PRESENTS, decided per replica and reported: a
      replica pinning the tank's PRE-CHANGE mechanism is still a true statement
      about what shipped before, so it is either retired with the mechanism it
      pins, or kept and renamed to say it pins the superseded tank. A replica
      pinning the tank's CURRENT mechanism follows the production change.
      Do not weaken an assertion or retune a threshold to keep a case green —
      that is the defect this chain exists to hunt. If a case cannot be
      classified by reading, report it and stop.
      ASSERT, before touching either replica, that the de-duplication of 3.2 and
      3.3 left both bit-identical; that assertion is 3.2's own and it must
      already be green. This task runs after 3.7, so state plainly which
      failures are 3.7's mechanism change and which would be a de-duplication
      defect, because a replica edited in the same pass as production detects
      nothing.
- [ ] 3.8 Density's default is 0.0, diffusion at minimum — the tank's own
      twin-line character, as close to today's default tank as the new stage
      allows. Bit-identity is impossible because the mechanism behind the slot
      changes entirely. ASSERT the measured difference from today's default tank
      and report the figure with its grid, rather than asserting closeness in
      prose.
- [ ] 3.9 Measure the metallic ringing the allpass cascade produces, on an
      impulse, across Density's travel, and report it as a figure with its grid.
      THE METRIC IS THE NORMALISED ECHO DENSITY PROFILE of Abel and Huang: over a
      sliding 20-30 ms Hanning-weighted window, the fraction of impulse-response
      samples whose magnitude exceeds that window's standard deviation, divided
      by the complementary error function of one over root two, about 0.3173, so
      that Gaussian-dense output reads 1. It runs from 0 at sparse to about 1 at
      fully dense, rises monotonically with diffusion, needs one pass and no FFT,
      and is insensitive to level, equalisation, decay time and sample rate.
      ASSERT it rises monotonically with Density, with the grid and the tap point
      stated.
      SPECTRAL FLATNESS IS REJECTED AND MUST NOT BE SUBSTITUTED. An allpass
      changes no magnitude spectrum, so a flatness measure is constant by
      construction against the parameter under test — a check that cannot fail,
      which is the defect this chain spends its length hunting. Kurtosis is
      rejected too: poor dynamic range, and false peaks exactly at low density,
      which is the regime a diffusion sweep starts in.
      TWO DEAD INSTRUMENTS, the second sharper than the first. An impulse that
      never reaches the tank produces a flat row that passes while measuring
      nothing, so assert in the SAME case that the impulse response carries
      energy at Density's floor. But an UNCONFIGURED `dsp::DelayDiffuser` passes
      that check and still measures the wrong mechanism: with its sections left
      at a one-sample delay it is a phaser, which its own header warns about, and
      it carries energy happily. Assert in the same case that every section's
      configured delay is longer than one sample at the measurement's sample
      rate, or the run is VOID.
- [ ] 3.10 Stage gate: full suite green, then commit and push.

## Stage 4 — the documents

House style: plain present tense saying what a control does; no jokes; no
defining by negation; no history the reader never saw; and no task numbers,
change names, planning-document references or rule citations in comments, test
names or strings.

- [ ] 4.1 `MANUAL.md` and `QUICK_DICT.md` for the two Filter controls and both
      pages' Diffusion, Density and Stereo width. The Drive controls are already
      updated and are not re-done.
- [x] 4.2 State what each Drive control does, which are conditional and on what,
      and which settings make another control inert. OUTCOME: both documents
      state the floored blend, what Fuzz holds back at each extreme, and that
      Fold is quieter at Fuzz maximum without going inert. The per-harmonic and
      total-level figures the task quoted are deliberately NOT stated, because no
      case measures harmonics at Fuzz maximum and a figure nothing produces does
      not go in a document.
- [ ] 4.3 State on BOTH the Delay and Reverb pages what Diffusion and Density
      each mean, so the difference is on the page rather than in the reader's
      memory.
- [x] 4.4 Rewrite the cross-instrument Feedback section affirmatively. OUTCOME:
      all five entries say what their control does, both openings that defined a
      control by what it is not are gone, and the section closes with what
      Reverb's tank uses instead of a Feedback control.
- [x] 4.5 Qualify the Anti-alias entry. OUTCOME: both documents now attach the
      no-dip figure to the bank's registered defaults instead of claiming it
      everywhere. The larger dip away from those defaults is deliberately NOT
      stated, because the only case measuring the dip runs at those defaults and
      nothing in the tree backs the wider claim.
- [ ] 4.6 `MANUAL.md` states plainly that raising Density trades smoothness for
      coloration, and at roughly which part of the travel the coloration becomes
      audible, backed by 3.9's figure.
- [ ] 4.7 Stage gate: full suite green, then commit and push.

## Stage 5 — the spec delta and archival

- [ ] 5.1 As each stage lands, move its scenario's `Check:` line from its
      not-yet-delivered marker onto the test that now backs it, and resolve that
      test's name by grepping for it — when the line is written and again before
      delivery. The Peak gain scenario has already been moved this way, because
      its marker had gone stale against a test that exists and passes.
- [ ] 5.2 Re-count EVERY MODIFIED requirement this delta carries against the
      promoted text, which is three if 5.2a promotes its own and two if 5.2a
      drops itself. An earlier wording said "both" and would have left a third
      requirement counted by nobody. The restate gate checks clause-level drift
      inside any scenario the delta restates and requires a `keeps:` line under
      every declared edit, so the remaining manual duty is the scenario COUNT,
      which the gate deliberately does not cover: dropping a whole scenario is
      sometimes legitimate supersession and nothing mechanical separates that
      from an accident.
      THIS TASK COUNTS 5.2a's REQUIREMENT TOO, and 5.2a does not count its own.
      A requirement whose only scenario check is its author's assertion that it
      is fine is self-certified, which is the pattern this change's gates and
      the omni rule both exist to stop. Run 5.2 after 5.2a, never as part of it.
- [ ] 5.2a Restate "An insert effect page's master returns the dry signal at its
      floor" as a THIRD MODIFIED requirement in this delta, on what 3.4a
      returns. THIS WIDENS THE DELTA and is reported as such rather than slipped
      in: the requirement is promoted text this change did not originally touch,
      and it is reached only because §9 holds that a ruling is not recorded
      until every artifact asserting the opposite has been re-read. This change
      ruled the Reverb bank's registered defaults a dead instrument; that
      paragraph asserts a measurement taken there.
      A MODIFIED requirement replaces the whole body, so every other clause and
      scenario under it is carried forward verbatim, and every dropped promoted
      clause is declared in a `RESTATES-EXCEPT` block with its `keeps:` line —
      `app/check_modified_requirements_restate_promoted.py` fails the build
      otherwise.
      THE FIGURES DO NOT GO BACK INTO THE PROSE. Working rule 1 governs: a
      figure in prose cannot fail when it drifts. State what the control does
      and name the check that pins it. If 3.4a shows the two figures measured
      the crossfade's dry floor, say that is what they measured and let the
      check carry the number.
      If 3.4a's result makes the existing wording true as it stands, record that
      and drop this task. An inconvenient result is the finding either way.
- [ ] 5.3 Every scenario's `Check:` names a test that exists and passes, or is
      marked not yet delivered in the form the gate recognises. Nothing parses
      prose, which is why this is the cheapest claim in the document to make
      falsely.
- [ ] 5.4 Confirm the two transitional NOTEs in the promoted
      `froggers-sheaf-parameter-model` spec still name this change, and that this
      delta's declared edits quote them verbatim. They were repointed here when
      the superseded change was deleted, and the two must move together or the
      restate gate goes red.
- [ ] 5.5 Archive this change, promoting its delta.

## Audits

- [ ] A.1 Preflight, in a context that did not write this change, covering the
      ARTIFACTS AND THE CODE TOGETHER — the artifacts are claims about the code,
      so neither can be checked without the other. It may reject.
- [ ] A.2 Each stage gets its own postflight in a fresh context before its
      commit, comparing implementation against this proposal and reporting
      divergence strictly.
