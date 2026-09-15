# Postflight: are the delta's promoted scenarios backed

Scope: `openspec/changes/frogg3rs-delay-capacity-and-width-finish/specs/froggers-sheaf-parameter-model/spec.md`
against `openspec/specs/froggers-sheaf-parameter-model/spec.md`. Checks: `app/FroggersDspParityTests.cpp`,
`app/FroggersModulationTests.cpp`, `app/FroggersAudioRoutingTests.cpp`, `app/Makefile` gates. No git command run
that changes state; no build run by this pass. `research/` not read.

Commands used throughout (repeated per name):

```
ls -la app/build/*.log
grep -n "TEST_CASE($NAME)" app/FroggersDspParityTests.cpp app/FroggersModulationTests.cpp app/FroggersAudioRoutingTests.cpp
grep -n "$NAME" app/build/final_run.log
```

`ls -la app/build/*.log` found nine logs. The most recent run log by mtime is `app/build/final_run.log`
(Sep 14 17:44), `191/191 tests passed`, one binary (`froggers_dsp_parity_tests`, whose 191 `TEST_CASE`
count in `app/FroggersDspParityTests.cpp` matches the log's line count exactly). No other log under
`app/build/` records a test run against `FroggersModulationTests.cpp` or `FroggersAudioRoutingTests.cpp`
binaries — `froggers_modulation_tests` and `froggers_audio_routing_tests` are built in `app/build/` but
no log shows a `[PASS]`/`[FAIL]` line for any of their test names.

## ADDED Requirements

### A fold control adds folds as it is turned up

> fold depth increases across the travel ... midpoint reproduces the shipped default bit-for-bit

Check named: `frog_block_fold_density_rises_with_knob_through_the_production_router`,
`frog_block_fold_inverted_map_matches_original_within_two_ulp_at_every_mirrored_pair`,
`frog_block_fold_inversion_bit_identical_to_original_at_dyadic_knobs` — all three EXIST in
`app/FroggersDspParityTests.cpp` (lines 6573, 6690, 6724) and all three are `[PASS]` in
`final_run.log`. Each name matches the clause it is cited for (density-rises for the monotonic
clause, the two ULP/bit-identical names for the reachable-set and midpoint clauses). BACKED, passes now.

### A symmetry control skews the wave monotonically, in both directions

Checks named: `drive_symmetry_signed_asymmetry_is_monotone_across_gain_shape_fold_and_amplitude`,
`drive_symmetry_bound_is_the_widest_with_the_best_monotonicity`,
`drive_symmetry_energy_ratio_control_distinguishes_from_the_signed_check`,
`drive_symmetry_top_of_travel_does_not_read_weaker_than_its_floor`,
`drive_symmetry_registered_default_reproduces_no_offset` — all five EXIST (lines 8974, 9083, 9040,
8674, 8871) and all five are `[PASS]` in `final_run.log`. The scenario's own prose states the
measured rate (154/180) and explicitly disclaims full-grid monotonicity as unmeetable without a
different change to `PolynomialDrive`; that qualification is carried in the scenario text itself,
not smuggled past a check. BACKED, passes now, with its own stated limitation.

### Fold is inert at the top of Fuzz

Check named: `drive_fold_moves_the_output_at_fuzz_maximum` — EXISTS (line 6381), `[PASS]` in
`final_run.log`. BACKED, passes now.

### The anti-alias control does not dip mid-travel

Checks named: `drive_anti_alias_travel_does_not_dip_below_either_endpoint`,
`drive_anti_alias_crossfade_falls_monotonically_and_the_old_one_pole_barely_moved_it` — both EXIST
(lines 6321, 6176), both `[PASS]`. BACKED, passes now.

### Peak gain's level cost is measured, stated, and pinned

Check named: `filter_bank_peak_gain_travel_measurement_at_and_away_from_resonance` — EXISTS
(line 3216), `[PASS]`. BACKED, passes now.

### The manual's Comb/Peak endpoints match the blend the DSP applies / The manual's Peak gain figure matches the output

Both marked `Check: operator step` (first) and cite `filter_bank_peak_gain_travel_measurement_at_and_away_from_resonance`
(second, already verified above, PASS). Both scenarios state plainly that no automated gate reads
manual prose against DSP behaviour (`app/check_docs_match_parameter_table.py` only checks table-entry
existence). Correctly marked as not automated rather than silently unstated. Not a defect.

### A bare filename is not evidence that a named check exists

Check named: `app/check_spec_checks_resolve.py`, run by `app/Makefile`'s `check-spec-checks-resolve`
target. The script EXISTS (`app/check_spec_checks_resolve.py`, 23894 bytes, mtime Sep 12 20:45) and the
target EXISTS and is wired into `app/Makefile`'s `test:` recipe (confirmed by direct read of the
Makefile: `check-spec-checks-resolve:` at line 203 invokes it, and `test:` at line 344 lists
`check-spec-checks-resolve` as a prerequisite). No log under `app/build/*.log` records this Python
gate's own pass/fail output for the current tree (the run logs capture only the C++ `TEST_CASE` binary,
not the Makefile's Python-gate targets). EXISTS and is wired; NOT VERIFIED BY ME as passing now — no
run-log evidence either way.

### Diffusion smears in time on every page that offers it

Check named: `reverb_density_travel_raises_the_impulse_responses_echo_density` — EXISTS (line 10001),
`[PASS]`. BACKED, passes now, for the Reverb page (Density) and the Delay page (Diffusion) both, per the
scenario's own text ("Both pages smear in time through the same cascade").

### A stereo-image control widens across its whole travel

Delta text marks this scenario **PARTLY DELIVERED**: Reverb's Stereo width is DELIVERED, backed by
`reverb_damping_filter_split_lowers_wet_leg_correlation_at_every_setting` (EXISTS line 9623, `[PASS]`)
and `reverb_density_correlation_travel_is_a_small_fraction_of_stereo_widths_live_monotonic_travel`
(EXISTS line 9738, `[PASS]`, strict monotonic fall across the five-point grid with a liveness
assertion). That half is BACKED, passes now.

The Delay page is declared **REFUTED** in the delta's own prose, with the mechanism traced to
`dsp::StereoDelay::Process` computing its cross-feed weight as half the width knob, applied inside the
feedback write, re-correlating the signal as width rises. The delta states: "the requirement stands and
the mechanism changes: `frogg3rs-delay-width-wysiwyg-repair` repairs the cross-feed ... and this line
carries that repair's own check once it lands."

This citation does not resolve. `openspec/changes/frogg3rs-delay-width-wysiwyg-repair` does not exist as
a directory (`ls` confirms it). `git status --short` shows all 22 of its files staged for deletion (`D`),
and this very change's own `tasks.md` (Stage 1 preamble) names those exact 22 deletions as work that
"must land with this change's first commit" — i.e. the change this delta names as the future deliverer of
the Delay-page fix is being retired by this change's own Stage 1 commit, not by some later change.

Reading the current source directly (`app/dsp/Delay.hpp` lines 749-759): the cross-feed weight is now a
literal `const float cross = 0.0f;` with a comment stating it is "Fixed at 0.0f, decoupled from both
p.dwid and widthBalance: Stereo width no longer drives any weight into the feedback path." This is the
repair the delta says is still pending. A matching test exists and passes:
`stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width` (line 9540, `[PASS]` in
`final_run.log`), which sweeps Delay width {0.25, 0.50, 0.75, 1.00} at Feedback 0.7 (the exact figure the
delta's own REFUTED prose measured), asserts `|corr|` strictly falls row-to-row and stays below 0.55,
channel balance stays under 0.06, and carries a liveness assertion (finite, non-zero RMS) per row.

The test's grid starts at 0.25, not the control's floor (0.0); the scenario asks for "floor to top." The
delta's Reverb-page prose for the identical scenario resolves the second clause ("every mechanism it
drives widens ... in the same direction") as vacuous once a control drives only one mechanism; the same
reasoning would now apply to Delay, since cross-feed is fixed and width drives only the read-time offset
— but the delta text does not say this for Delay, because it was written when cross-feed was still
knob-driven.

FINDING: the delta's own REFUTED/PARTLY-DELIVERED marking for this scenario is stale relative to the
code and the test suite. The named future-delivering change does not exist and cannot deliver anything —
its content is being folded into and archived by this change's own Stage 1. A passing check
(`stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width`) that measures close to
what the scenario asks now exists but is not cited by the delta, and the scenario is not re-marked
DELIVERED. This is not "marked plainly as not yet delivered with the change that will deliver it named"
in any sense that resolves — the named change is being deleted by this change itself.

### A control that colours says so

Marked `Check: operator step`, cites `reverb_density_travel_raises_the_impulse_responses_echo_density`
(verified above, PASS) for the figures the manual prose is read against. Correctly marked as operator
step; not a defect.

## MODIFIED Requirements

### A Drive page control's travel spends itself where the control is heard

All four scenarios under this requirement carry explicit `Check:` lines:

- Phase moves the blend: `drive_phase_sweep_moves_output_meaningfully_across_each_quarter_of_travel`
  — EXISTS (line 5485), `[PASS]`. BACKED.
- The anti-alias control rejects aliasing: `drive_anti_alias_crossfade_falls_monotonically_and_the_old_one_pole_barely_moved_it`
  — EXISTS (line 6176), `[PASS]`. BACKED.
- A quantized control spends no travel on steps that do nothing: `drive_bit_depth_first_audible_knob_value_falls_from_0_19_to_a_hundredth`
  — EXISTS (line 5067), `[PASS]`. BACKED.
- The knob's default reproduces what shipped before it: `frog_block_default_knob_values_reproduce_original_output_exactly`
  — EXISTS (line 6783), `[PASS]`. BACKED.
- A control that is inert at a default is documented as such: `drive_symmetry_is_inert_when_the_folder_is_not_engaged`
  — EXISTS (line 8730), `[PASS]`. BACKED, and this scenario's own numbers (about 3 dB engaged vs about
  -19 dB disengaged) match its own clause (more than 10 dB smaller, not bit-identical).

This MODIFIED requirement's restated preamble states it drops the promoted text's transitional NOTE
naming Link/Bias and `frogg3rs-delay-width-wysiwyg-repair` because "this change removes" the two controls
those tests cited. Source-checked directly: `grep -rn "SetLink"` across `app/FroggersDspParityTests.cpp`
and `app/dsp/*.hpp` returns nothing — Link is gone, matching the claim. Consistent.

### One sixteen-slot bank per Froggers page

The requirement's own generic scenarios (Page identity, Sparse banks, Audio/Envelope/Filter bank
enumeration) carry no `Check:` line, matching the same no-inline-check convention the Reverb bank
scenario below states explicitly for this whole requirement ("the scenarios under this requirement carry
no `Check:` line by the promoted spec's own convention"). Not evaluated clause-by-clause here for the four
banks this change does not touch (Audio, Envelope, Filter) — the delta's own RESTATES-EXCEPT block
confirms these are carried forward verbatim from the promoted text, unchanged by this change.

- **Drive bank scenario** (slots 10 Feedback, 13 Symmetry, both changed by this delta): no inline
  `Check:` line. Source-checked: `drive_feedback_cannot_self_oscillate_at_maximum` and
  `drive_feedback_default_knob_reproduces_todays_path_exactly` EXIST in `FroggersDspParityTests.cpp`
  (lines 6441, 6507) and are `[PASS]`, backing the slot-10 Feedback description (folder-output feedback,
  not a whole-stage loop). `app/dsp/Drive.hpp:740` (`SetSymmetry`) computes
  `0.02f * (2.0f * symmetryKnob01 - 1.0f)`, matching the delta's stated ±0.02-cycle bound with a centred
  default; `drive_symmetry_registered_default_reproduces_no_offset` (verified above, PASS) backs the
  centred-default clause. BACKED by source + passing tests, though none is cited inline by the delta text
  itself for this scenario (consistent with the requirement's stated no-inline-check convention).

- **Delay bank scenario**: no inline `Check:` line. The Width Balance capacity clauses ("never lengthens
  a read tap beyond the delay buffer's own capacity") are backed by the compile-gated `assert`s in
  `app/dsp/Delay.hpp` (`timeL <= capacitySeconds`, `timeR <= capacitySeconds`, `FROGGERS_DSP_CHECKS`-gated)
  and by `stereo_delay_width_spread_bound_holds_across_the_reachable_grid` and
  `stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks`, both EXIST (lines 10587, 10733)
  and both `[PASS]` in `final_run.log`, and by `app/build/break_proofs2.log`'s six mutation-break proofs
  (all PASS, i.e. all six deliberate breaks were correctly rejected). BACKED.

- **Reverb bank scenario**: carries an explicit `Check:` line marked "DELIVERED for the three clauses
  above" (Density, fixed cross-feed, split Damping filters), citing
  `reverb_process_reproduces_its_captured_output_exactly` (EXISTS line 7053, `[PASS]`),
  `reverb_density_travel_raises_the_impulse_responses_echo_density` (verified above, PASS), and
  `reverb_damping_filter_split_lowers_wet_leg_correlation_at_every_setting` (verified above, PASS). The
  delta's own text is explicit that the cross-feed citation is weaker evidence than the other two (a
  regression pin, not an isolating case) and backs the fixed-weight claim by direct source citation
  instead (`app/dsp/Reverb.hpp`'s literal-zero `CrossFeedPair` call) — an honest, self-declared gap
  rather than an overclaim. BACKED, passes now, with the weaker-citation caveat already stated in the
  delta itself.

### An insert effect page's master returns the dry signal at its floor

All seven scenarios carry explicit `Check:` lines:

- Master returns dry at floor: `drive_blend_phase_authored_zero_blend_is_exact_passthrough` — EXISTS
  (line 5256), `[PASS]`. BACKED.
- Master does not lose level partway: `drive_blend_travel_holds_level_within_1_3_db_across_gain_and_frequency`
  — EXISTS (line 8544), `[PASS]`. BACKED.
- Tone control's level change documented: `reverb_damping_darkens_and_quiets_the_tank_while_room_size_does_neither`
  — EXISTS (line 9435), `[PASS]`. BACKED.
- Gain stage makes distortion: `drive_gain_makes_distortion_and_the_manglers_act_at_any_gain` — EXISTS
  (line 9315), `[PASS]`. BACKED.
- A fed page's master does nothing until fed: `delay_wet_dry_leaves_dry_untouched_while_send_is_closed_and_moves_it_once_fed`
  (EXISTS line 9237, `[PASS]`) and `reverb_wet_authority_tracks_whether_send_is_open_and_the_tank_is_fed`
  (EXISTS line 3981, `[PASS]`). BACKED.
- Master's position and name say what it does: `app/check_docs_match_parameter_table.py` — EXISTS
  (10320 bytes). Not a `TEST_CASE`; no `[PASS]`/`[FAIL]` line for it in any log. NOT VERIFIED BY ME as
  passing now.
- Default patch does not arrive wet: `app/FroggersModulationTests.cpp`'s `default_patch_gain_is_20_percent`
  and `default_patch_wet_dry_reads_its_own_registered_default` — both EXIST in
  `app/FroggersModulationTests.cpp` (lines 1334, 1347). **Neither name appears in any log under
  `app/build/*.log`.** The binary that would run them, `app/build/froggers_modulation_tests`, exists on
  disk but no run log records its output. Test presence confirmed; pass status NOT VERIFIED BY ME — no
  run-log evidence either way.
- A control's range reaches the effect it is named for: `reverb_predelay_sweeps_tens_of_milliseconds_with_no_dead_adjacent_steps`
  and `reverb_predelay_floor_and_ceiling_match_derived_millisecond_range` — both EXIST (lines 3680, 3742),
  both `[PASS]`. BACKED.

## Checks named in the delta that do not resolve, or resolve to a decoy

None found. Every `TEST_CASE`-shaped name cited in the delta resolves to an exact, unique `TEST_CASE(...)`
definition in one of the three named test files (checked by exact `TEST_CASE($NAME)` grep, not by
substring/basename match). No `Check:` line in this delta points at this change's own `tasks.md`.

## Findings, with block ruling

1. **`stereo_delay` width scenario: stale REFUTED marking, phantom named change.** The delta's "A
   stereo-image control widens across its whole travel" scenario declares the Delay page REFUTED and
   names `frogg3rs-delay-width-wysiwyg-repair` as the future change that repairs it. That change's
   directory does not exist; its 22 files are staged for deletion and this change's own `tasks.md` says
   they land with THIS change's first commit. The code (`app/dsp/Delay.hpp`) and a passing test
   (`stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width`, PASS in
   `final_run.log`) show the repair already landed in the working tree. The delta's scenario text was not
   updated to match. **Breaks Stage 3** (the spec-delta group is exactly where this must be fixed — either
   marked DELIVERED with the real check cited, or, if the check is judged insufficient — e.g. its grid
   omits the 0.0-to-0.25 quarter of the travel — marked not-yet-delivered against a change that actually
   exists). Does not block Stage 1's commit, which ships code, not this spec-delta text — but Stage 1's
   own commit is what deletes the named change, which is the direct cause of the break, so Stage 1 and
   Stage 3 are coupled here and should not be separated by an operator who has not read this finding.

2. **Two modulation-tests checks are unverified by run log.** `default_patch_gain_is_20_percent` and
   `default_patch_wet_dry_reads_its_own_registered_default` exist in source and their binary is built, but
   no `app/build/*.log` records their run. Per this audit's own evidentiary rule (source presence + most
   recent run log), these are NOT VERIFIED BY ME as passing now, though nothing suggests they fail.
   **Blocks neither execution nor delivery by itself** — it is a gap in what this pass can confirm, not a
   demonstrated failure — but Stage 3 (or whoever next runs the full suite) should re-run
   `froggers_modulation_tests` and capture its log so this scenario's Check: line is verified rather than
   merely present.

3. **`check_spec_checks_resolve.py` gate: wired but unverified by run log.** Exists, is wired into
   `app/Makefile`'s `test` target, but no log captures its own pass/fail for the current tree (only the
   C++ `TEST_CASE` binary output is logged). **Blocks neither execution nor delivery by itself** for the
   same reason as finding 2 — re-run `make test` in full and capture output before relying on this gate's
   current-tree status.

4. **Everything else cited by an explicit `Check:` line in the delta resolves to an existing test that is
   `[PASS]` in the most recent run log**, including every ADDED-requirement scenario, the Drive-page
   MODIFIED scenarios, and six of the seven wet/dry-master scenarios. No blocking defect on these.
