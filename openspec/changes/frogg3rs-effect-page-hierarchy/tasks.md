# Tasks — `frogg3rs-effect-page-hierarchy`

**State as of 2026-09-11**, after the second preflight pass (`preflight.md`,
run in a fresh context, which is what §9 asks for and the first pass could not
provide). Everything marked `[x]` is implemented and green.

Group 12 is WITHDRAWN on measurement rather than built: the deficit it existed
to fix was measured on a different quantity than the one it names, and the
structure it proposed to fix was read backwards. Two parity cases were added in
its place, one of which pins what the withdrawal leaves asserted.

Gate: 371 PASS / 0 FAIL, both through `make -C app test -j2` and every binary
run directly by path, against a 356 PASS / 0 FAIL baseline (`baseline.md`) and
the 369 this tree measured before today's two additions. Group 13 is the only
work outstanding.

## 0. Operator calls

- [x] 0.1 The wet cap: cap stays on Delay and Reverb only, Drive reaches fully
      wet. SUPERSEDED in part by task 12.3 -- the cap's original justification
      turned out to be the crossfade defect that group 11 fixed.
- [x] 0.2 How Reverb frees the slot Send needs: collapse Mod depth and Mod rate
      into one Mod control. Nothing cut; Pre-delay's range error fixed in 6.2.
- [x] 0.3 Which factor sits at the clean end: 4x.
- [x] 0.4 Phase's default: moves to the measured neutral, no stored patches.
- [x] 0.5 Drive page transparency at rest: WITHDRAWN, see group 4.

## 1. Pin what is already true

- [x] 1.1 Five parity cases added pinning Blend's notch, XOR's under-1 kHz
      collapse, Link inert at Gain 0, Bias inert at Gain 0 / Shape 0, and
      Delay transparent at rest with Send closed. Each inertness case carries
      an asserted positive control. (Blend's notch pin was later re-derived by
      group 11, which removed the notch it pinned.)

## 2. Phase

- [x] 2.1 Failing case first: quarter-by-quarter travel. Failed at 0.00225%
      movement in the first quarter against a 0.1% threshold.
- [x] 2.2 Knob mapped through the allpass's break frequency,
      `a = tan(pi/4 - pi*fb)` with a `sqrt(knob)` pre-warp. Endpoints still
      exactly +-0.98, strictly inside the unit circle.
- [x] 2.3 Static-Phase neutrality pin re-derived, not deleted.
- [x] 2.4 Phase default measured at 0.86, the quadrature point. Quarters now
      move 0.544% / 3.656% / 10.14% / 10.95%.

## 3. Anti-alias brightness

- [x] 3.1 Failing case first, at 1487 Hz. The tone matters: 3 kHz divides
      48 kHz exactly, so every fold-image lands on a harmonic bin and the
      measurement cannot separate them, and 3 kHz is also above what 4x can
      clean. Measured 4x improvement: 23.3 dB at 1487 Hz, 8.7 dB at 2971 Hz,
      7.8 dB at 5939 Hz.
- [x] 3.2 8th-order Butterworth decimation cascade from `dsp::BiquadDf1`, 4x.
- [x] 3.3 Control crossfades the clean path against the shipping 2x one-pole.
      Default moved to the all-grit end; the existing oversampler parity pin is
      byte-identical and passing.
- [x] 3.4 Cost recorded rather than gated: 20.85 ns/sample today, 56.6 ns for
      the new path, 0.10% and 0.27% of the 48 kHz 128-sample block budget. No
      deadline harness exists in this app and none was added -- a gate against
      an overrun traced to be impossible is a branch to remove.

## 3b. Bit depth's dead fifth

- [x] 3b.1 Threshold was knob 0.19 (hashBits 1 is a mathematical no-op for the
      scramble, so it is bit-identical, not merely quiet). Remapped onto the
      counts that act; the first audible value is now 0.02, same -40.71 dB.
      The default knob's own count is unchanged.

## 4. The fold: withdrawn

- [x] 4.1 WITHDRAWN. Normalizing the sine fold and moving Fold's default was
      justified by "a scalar cannot change harmonic character [...] this
      changes the page's level, never its sound's shape". That is false:
      `digitalReorganizer` runs after the fold and maps onto a FIXED absolute
      8-bit grid, so gain does not commute with it. Residual after a best-fit
      gain match: -140.5 dB with the manglers at their floors (the positive
      control), but -6.0 dB with XOR engaged, -4.7 dB with Bit depth, -10.2 dB
      with both. The property being bought was one neither Reverb nor Delay
      has. The fold, Fold's default and the fold's parity pin are untouched.

## 5. The names and the position

- [x] 5.1 Drive renamed Gain, Blend renamed Wet/Dry (short `Wet`), Wet/Dry
      moved to slot 0. `check_docs_match_parameter_table.py` went from 27
      failures to OK.
- [x] 5.2 `ApplyDriveBankOverlay` followed to Gain's new slot. Left alone it
      would have shipped the default patch 20% wet. Two tests now cover it,
      one reading the registered default dynamically.

## 6. The wet cap, and the Reverb bank

- [x] 6.1 `kMaxWetMix` replaced by `kMinDryLevel = 0.30`, which states the
      guarantee rather than encoding it for one crossfade law. See 12.3.
- [x] 6.2 `PreDelayNormFromKnob` fixed from 1..100 SAMPLES to milliseconds,
      ceiling derived from `kSize - 1` per call (85.3 ms at 48 kHz, verified
      at 96 kHz too). Failed first at ~2.06 ms of total travel.
- [x] 6.3 Mod depth and Mod rate collapsed to one Mod control, law chosen by
      measuring a 5x5 depth-by-rate grid: the axes are near-orthogonal, so a
      fixed rate under a depth knob preserves nearly the whole reachable range
      while a rate-follows-depth law loses the deep-slow and shallow-fast
      corners. Rate fixed at 0.35 Hz.
- [x] 6.4 `Send` added at Reverb slot 1; the wet-level follower and
      `WetAuthority()` extracted into one shared `WetAuthorityFollower` both
      stages own. One definition of each constant, single-sourced at the call
      graph.

## 7. The Delay bank

- [x] 7.1 Pinned before moving anything.
- [x] 7.2 `Wet mix` renamed `Wet/dry` (short `Wet`), moved to slot 0, `Send`
      at slot 1.
- [x] 7.3 Positional references re-derived across the tree.
- [x] 7.4 Pins re-run against the moved slots.

## 8. The spec delta

- [x] 8.1 `## MODIFIED Requirements` added. Three promoted scenarios pinned
      "slots 0-8 are unchanged" for all three banks, one pinned Reverb slot 9
      as Mod Rate, one required 40% dry, and two still named Delay's "Wet mix".
      `openspec validate --strict` passes without these, so it is not the
      check that this was done.
- [x] 8.2 Transparency requirement restated as the rule the family keeps.

## 9. Repo hygiene

- [x] 9.1 Planning-history labels stripped from comments in the files this
      change touches.
- [x] 9.2 `openspec/specs/field-button-input-latency/spec.md:52` cited
      `src/FroggersTiga`; rewritten to name `src/FroggersSolo` and
      `src/FroggersGuitar`, and its "documented in the change design" pointer
      replaced with `src/mk/config.mk`, which the same requirement already
      names as the source of those flags.
- [x] 9.3 Thirteen comments in `app/FroggersDspParityTests.cpp` carried
      `D1`-`D10` design-list shorthand and one "this task's own requirement"
      phrase, all introduced by this change's own edit to that file. Reworded
      to name the behaviour each case pins. Found 13, changed 13.
- [x] 9.4 Three delivered changes archived: `frogg3rs-midi-shift`,
      `frogg3rs-launchpad-port-aliases`, `frogg3rs-launchpad-variant`. Their
      spec deltas promoted by `openspec archive`; `openspec validate --all
      --strict` passes 23/23. `frogg3rs-midi-controller-resilience` is left
      open and untouched, on the operator's instruction.
      Reported, outside this change's directories, NOT fixed here:
      `.git/hooks/pre-commit` is a no-op guarding a `sim/` path this tree does
      not have (and is untracked machine state with no tracked template);
      `package.json:2` still names the project `froggers-tiga`;
      `app/.gitignore:4` cites `src/FroggersTiga/build/` and a `tasks.md 12.4`
      that resolves nowhere; `build/manifest/` is orphaned scratch describing a
      Sequencer the app no longer ships;
      `openspec/.sessions/marbles-mod-led-level-meter-progress.md` is orphaned
      from an archived change.

## 10. Documents

- [x] 10.1 `MANUAL.md` and `QUICK_DICT.md` updated for every renamed and moved
      control; `check_docs_match_parameter_table.py` reports OK.
- [x] 10.2 The prose pass: the Drive page's hierarchy, Wet/Dry's behaviour,
      XOR's mid-range, Phase's conditional range, what the anti-alias control
      now reaches AND where it runs out, Reverb's Send, the collapsed Mod,
      Pre-delay's new range, and -- since group 12 withdrew the compensation
      that would have hidden it -- what Damping costs in level and why that
      figure depends on the material.

## 11. The wet/dry crossfade law

- [x] 11.1 All three pages moved from a linear crossfade to equal power,
      `dry*cos(theta) + wet*sin(theta)`. A linear law holds level only when
      its legs are fully correlated; none of these are. Endpoints special-cased
      for bit-exactness (`std::cos(pi/2)` in float returns -4.371e-8, not 0).
      Drive's worst dip: -4.10 dB to -1.15 dB.
- [x] 11.2 Delay and Reverb improved less, to -6.25 dB and -6.81 dB, because
      their defect is a second one underneath: the wet leg ARRIVES quieter than
      dry. Reverb's wet leg measures -2.35 dB at 440 Hz, -6.57 dB at 880 Hz,
      -8.99 dB on broadband noise.
- [x] 11.4 POSTFLIGHT FINDING, fixed inside the change. Re-running §5 against
      the diff rather than against the proposal found what writing group 11
      had made redundant: the equal-power law existed in THREE copies --
      `Drive.hpp`, `Reverb.hpp` and `Delay.hpp` -- with the two floored pages'
      copies byte-identical to each other and each citing the other page's
      comment as though that made them shared. Extracted to
      `dsp::EqualPowerWetDry` (`app/dsp/Limiter.hpp`), beside `kMinDryLevel`
      and `WetAuthorityFollower`, which all three files already include.
      `minDryLevel` is the only parameter separating the pages: 0 for the
      Drive page, `kMinDryLevel` for the other two. Found 3, changed 3, one
      definition at the call graph. The Drive page keeps its own two exact
      endpoint branches, because an uncapped travel reaches pi/2 where
      `std::cos` does not land on 0.0f; that is a caller's exactness concern,
      not part of the law.
      Proven live rather than assumed: replacing the helper's body with the
      linear law it displaced, rebuilt from a deleted binary, turns three pins
      red -- one per page (`reverb_process_matches_manual_tank_replica_at_
      neutral_mod_and_hold`, `stereo_delay_to_reverb_mono_scales_the_mix_
      formula_by_wet_authority`, `drive_blend_travel_holds_level_within_1_2_db_
      across_gain_and_frequency`). Every one of the three call sites has a
      check that fails if the shared definition drifts, which is what §5 asks
      for where members cannot be collapsed further. Restored, 163/163.
- [x] 11.5 POSTFLIGHT FINDING, fixed inside the change. Four comments this
      change introduced attributed `WetAuthorityFollower` to `Drive.hpp`
      (`Reverb.hpp:249`, `Delay.hpp:439`, `:546`, `:1018`); it is defined in
      `Limiter.hpp`. Found 4, changed 4. No check script reaches a citation
      inside a comment, which is how four of them landed at once.
- [x] 11.3 A level follower normalizing wet to dry was prototyped and REJECTED
      on measurement: 4 to 8 dB of makeup movement across a take, which is
      audible breathing, and it barely helps dynamic material.

## 12. WITHDRAWN — Damping's level change is the lowpass working

The second preflight pass measured the premise before any of this was built
(§9), and it does not hold. `preflight.md`'s second pass carries the full
tables; the short of it:

- [x] 12.1 WITHDRAWN. Three separate errors, each traced:
      **(a)** The deficit was measured on the wrong signal. "Damping moves the
      wet leg's broadband level 6.4 dB (-9.15 / -10.59 / -7.72 / -5.44 /
      -4.21)" is not the wet leg; it is the stage's full mixed output at
      Wet/dry maximum with `kMinDryLevel` worth of dry still summed in, which
      is why it reads as a hump. Re-measured on `rv.wetL`/`rv.wetR` themselves,
      the loss is strictly monotonic at 2.50 dB per step -- exactly
      `10*log10` of `DampAlphaFromKnob`'s own ratio per step.
      **(b)** The damping filter is NOT inside the feedback path.
      `app/dsp/Reverb.hpp:669-671` filters the tank read; the feedback taps at
      `:604-605` come from the undamped `valA`/`valB`. The sentence describes
      Delay's tone filter, which genuinely is in its loop
      (`app/dsp/Delay.hpp:894-895`).
      **(c)** Damping's registered default is 0.0, not 0.4
      (`app/FroggersParameters.hpp:277-289`; there is no Reverb overlay), and
      0.0 is the loudest end of the travel, not a point near the worst.
      What the wet leg actually loses across Damping's travel: 9.98 dB on
      broadband noise, 7.20 dB on a plucked signal, 4.74 dB on a 440 Hz sine,
      0.52 dB on a 110 Hz sine. Positive control in the same run: Decay moves
      the same figure 6.94 dB, Room size 0.03 dB, Diffusion 0.04 dB.
      The spread IS the finding. Every static term derivable from the filter's
      own coefficient was measured across that travel, normalised so the
      registered default stays bit-identical:

      | law | noise | 440 Hz | 110 Hz | plucked |
      | --- | --- | --- | --- | --- |
      | none (ships) | 9.98 | 4.74 | **0.52** | 7.20 |
      | full power | **0.44** | 5.82 | 9.90 | 3.21 |
      | three-quarter | 2.17 | 3.85 | 7.29 | **1.21** |
      | half | 4.77 | 1.96 | 4.69 | 2.40 |

      None meets 12.1's own 1.5 dB criterion on more than one source, and every
      one makes the 110 Hz case -- which passes that criterion TODAY at
      0.52 dB -- four to nineteen times worse. A static scalar cannot undo a
      frequency-dependent gain, and this gain is a lowpass sweeping its corner
      from 1704 Hz to 154 Hz. The compensations that could track it are
      signal-dependent, and 11.3 already measured and rejected a follower for
      audible breathing.
      The change's own words for the Filter bank apply here unchanged:
      removing spectral energy is a filter's entire function. Damping is a
      filter. Its level change is documented in `MANUAL.md`, and the control
      that adds the level back is the `Send` this change gives Reverb -- which
      is the answer §11 took from Valhalla and had stopped short of applying.
- [x] 12.2 WITHDRAWN, and it was forbidden by 12.1's own safety rule once (b)
      is read the right way round. That rule -- never compensate inside the
      feedback path, where the loop gain is the decay time -- is correct, and
      Delay's `SetFeedbackTone` is inside the feedback path. Outside it, a
      static gain compensates nothing, because the loss it answers compounds
      per repeat. The third call site named for the shared helper, Drive, was
      one whose "term computes to about zero": a provably inert use added to
      make a helper shared, which §4's 2-of-4 does not admit.
- [x] 12.3 The dry floor STAYS at 0.30. Measured both ways by building
      `app/dsp/` twice, Reverb at its registered defaults with Send open and
      Wet/dry at maximum, output against the dry input: removing the floor
      costs 2.56 dB on noise and 1.17 dB on a plucked signal, and GAINS
      0.84 dB on a 440 Hz sine. The level argument carries it neither way.
      The re-opening argument was that the floor existed only because of the
      linear crossfade plus "a Damping default parked near its worst point";
      the second half is false twice over (12.1(b), 12.1(c)). What remains is
      the operator's own ruling, which `kMinDryLevel`'s comment records
      verbatim (2026-07-29, tightened 2026-08-26), and OPERATOR item 1 already
      re-answered inside this change on 2026-09-10. Removing the floor restores
      the exact condition that ruling was about: at full wet with no floor the
      dry signal is gone and only the uncorrelated wet path is left.
- [x] 12.4 `reverb_damping_darkens_and_quiets_the_tank_while_room_size_does_neither`
      added for what the withdrawal leaves asserted: Damping's broadband
      attenuation AND its spectral tilt both fall monotonically across the
      travel, with Room size over the same travel as the positive control.
      Both halves are pinned deliberately -- a later change that hid the level
      with a static makeup term would have to break one of them. Proven to fail
      by neutralising `DampAlphaFromKnob` and rebuilding from a deleted binary:
      3 cases went red, including this one. The spec delta's tone-control
      scenario was rewritten onto this check, having stood as "NOT YET
      DELIVERED" pointing at group 12.
- [x] 12.5 `drive_gain_makes_distortion_and_the_manglers_act_at_any_gain`
      added. The spec delta asserted a "stage-independence case" that did not
      exist in any test file -- an ADDED requirement with no check behind it,
      which §9 does not allow. It measures XOR, Bit depth and BOTH rate
      reducers against a bit-identical all-floor reference at Gain 0 (+3.59,
      -8.40, -21.78 and -21.78 dB, all above the -26 dB this change uses as
      its plainly-audible threshold), and Gain's own THD rise with every
      mangler at its floor. The all-floor render repeated is its positive
      control, at -240 dB.

## 13. Delivery

- [x] 13.1 Postflight run in a fresh context over all five of §9's demands.
      Sections 1, 2, 4 and 5 came back clean: 21 proposal/task items matched
      the code, all 13 spec scenarios' named checks were found and verified
      against what they assert, ~20 citations resolved, and the documents
      carried no stale control name or planning-history label. Section 3, the
      §5 re-run against the DIFF, found the two defects recorded as 11.4 and
      11.5, both fixed inside this change.
- [x] 13.2 `make -C app test -j2` plus every binary by path: 371 PASS /
      0 FAIL, 12 binaries, 5 check scripts OK. Re-run after the 11.4 refactor
      touched all three DSP pages; 18 exactness/parity pins green.
- [x] 13.3 Pushed to `main`, staged by path, two commits: `85bf09e` archives
      the three delivered predecessors and promotes what they shipped;
      `814a703` is this change's own code, documents and artifacts. No Sheaf
      file is touched, so the submodule stays pinned at `ba3898e4` and there
      is no PR to raise against upstream. `frogg3rs-midi-controller-resilience`
      is left open and untouched.
