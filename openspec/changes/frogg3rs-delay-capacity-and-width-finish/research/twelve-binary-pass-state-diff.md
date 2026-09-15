# Before: the twelve binaries at HEAD 0260ce9 with the six changed sources checked out from HEAD

HEAD: `0260ce931e383482f77691fed3e5de20cdf63b49` (`0260ce9 Settle the Delay width mechanism and record the evidence for it`).

The working tree carries six modified sources (`app/dsp/Delay.hpp`, `app/FroggersDspParityTests.cpp`, `app/FroggersAudioRoutingTests.cpp`, `app/Makefile`, `app/check_common.py`, `app/check_docs_match_parameter_table.py`). This run temporarily checked those six out to their HEAD content, built and ran the twelve test binaries, then restored the working-tree content byte for byte. No commit, stash, add, or reset was used; the only git command run was the one `git checkout HEAD -- <six paths>` written into the procedure.

## Twelve-binary results (HEAD sources, six files reverted)

| Binary | Exit code | Summary line | Fail count | Failing tests |
|---|---|---|---|---|
| froggers_headless_tests | 0 | `[PASS] stop_silences_curve_one_grace_active_voice_within_bound` (no aggregate count printed; 8 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |
| froggers_mono_validation_tests | 0 | `[PASS] mono_group_drill_in_materializes_depth_parameter` (4 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |
| froggers_dsp_parity_tests | 0 | `186/186 tests passed` | 0 | none |
| froggers_parameter_model_tests | 0 | `[PASS] configure_processing_timing_is_wired_at_96khz_prepare` (12 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |
| froggers_modulation_tests | 0 | `49/49 tests passed` | 0 | none |
| froggers_audio_routing_tests | 0 | `[PASS] a_fast_parameter_sweep_with_no_reset_does_not_latch_the_instrument` (51 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |
| froggers_visualizer_tests | 0 | `[PASS] silent_comb_visualizer_plots_flat_unity_response` (4 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |
| froggers_scope_advance_index_tests | 0 | `[PASS] vco_scope_published_index_advances_across_successive_blocks` (2 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |
| froggers_marbles_clock_tests | 0 | `9/9 tests passed` | 0 | none |
| froggers_surface_tests | 0 | `[PASS] wav_encoding_produces_a_correct_pcm16_header_and_round_trips_sample_zero` (56 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |
| froggers_midi_catalog_tests | 0 | `[PASS] launchpad_defaults_are_registered_with_expected_ids_and_kind` (9 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |
| froggers_controllers_page_tests | 0 | `[PASS] launchpad_presets_pair_with_the_port_names_a_host_reports` (4 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |

All twelve binaries exit 0 with zero `[FAIL]` lines. Binaries that print an aggregate line (`dsp_parity`, `modulation`, `marbles_clock`) show it; the other nine binaries print only per-test `[PASS]`/`[FAIL]` lines with no printed aggregate, so their summary line is the last `[PASS]` line, and the fail count is the grep of `[FAIL]` across the whole log (zero in every case).

The controlling quantity moved between the two build passes: `app/build/froggers_dsp_parity_tests` was `440568` bytes on the tree before checkout, `421992` bytes after HEAD's `Delay.hpp`/`FroggersDspParityTests.cpp` replaced the working-tree sources and the binary was rebuilt from a removed state, and back to `440568` bytes once the working-tree sources were restored and rebuilt again — confirming the rm-before-rebuild guard was not a no-op.

## Commands and output

### Step 1 — snapshot

```
$ md5 app/dsp/Delay.hpp app/FroggersDspParityTests.cpp app/FroggersAudioRoutingTests.cpp app/Makefile app/check_common.py app/check_docs_match_parameter_table.py
MD5 (app/dsp/Delay.hpp) = a0fa9d738fd5a7fd14831bfcfec8f560
MD5 (app/FroggersDspParityTests.cpp) = 92c605312f97319687f09e51885656f8
MD5 (app/FroggersAudioRoutingTests.cpp) = d2d0e9a7c09f8c31a63b744e80f1f342
MD5 (app/Makefile) = 445791795c6e86adafa9e236ef4ddec4
MD5 (app/check_common.py) = d72bcc55487bb0c536b36d2408ef33d9
MD5 (app/check_docs_match_parameter_table.py) = 06722ed08cf37a45f5c7c99553a77f86
```

Copies in `$S/before-state/` hashed identically (same six lines, same hashes).

```
$ git status --short
 M app/FroggersAudioRoutingTests.cpp
 M app/FroggersDspParityTests.cpp
 M app/Makefile
 M app/check_common.py
 M app/check_docs_match_parameter_table.py
 M app/dsp/Delay.hpp
D  openspec/changes/frogg3rs-delay-width-wysiwyg-repair/preflight-1.md
... (11 more preflight-N.md D lines, proposal.md, tasks.md, and 6 research/*.md D lines — 22 D total)
?? app/check_delay_capacity_break_proofs.py
?? app/check_delay_capacity_parameters_are_swept.py
?? openspec/changes/
```

Six ` M`, twenty-two `D `, three `??` — matches expectation.

### Step 2 — before-state sources

```
$ git checkout HEAD -- app/dsp/Delay.hpp app/FroggersDspParityTests.cpp app/FroggersAudioRoutingTests.cpp app/Makefile app/check_common.py app/check_docs_match_parameter_table.py
$ git status --short
D  openspec/changes/frogg3rs-delay-width-wysiwyg-repair/preflight-1.md
... (same 22 D lines)
?? app/check_delay_capacity_break_proofs.py
?? app/check_delay_capacity_parameters_are_swept.py
?? openspec/changes/
```

The six ` M` lines are gone; the 22 `D ` and 3 `??` lines are unchanged.

### Step 4 — restore

```
$ md5 app/dsp/Delay.hpp app/FroggersDspParityTests.cpp app/FroggersAudioRoutingTests.cpp app/Makefile app/check_common.py app/check_docs_match_parameter_table.py
MD5 (app/dsp/Delay.hpp) = a0fa9d738fd5a7fd14831bfcfec8f560
MD5 (app/FroggersDspParityTests.cpp) = 92c605312f97319687f09e51885656f8
MD5 (app/FroggersAudioRoutingTests.cpp) = d2d0e9a7c09f8c31a63b744e80f1f342
MD5 (app/Makefile) = 445791795c6e86adafa9e236ef4ddec4
MD5 (app/check_common.py) = d72bcc55487bb0c536b36d2408ef33d9
MD5 (app/check_docs_match_parameter_table.py) = 06722ed08cf37a45f5c7c99553a77f86
```

Identical to step 1's hashes, all six.

```
$ git status --short
 M app/FroggersAudioRoutingTests.cpp
 M app/FroggersDspParityTests.cpp
 M app/Makefile
 M app/check_common.py
 M app/check_docs_match_parameter_table.py
 M app/dsp/Delay.hpp
D  openspec/changes/frogg3rs-delay-width-wysiwyg-repair/preflight-1.md
... (same 22 D lines)
?? app/check_delay_capacity_break_proofs.py
?? app/check_delay_capacity_parameters_are_swept.py
?? openspec/changes/
```

Equal to step 1's `git status --short` output line for line (6 M / 22 D / 3 ??).

### Step 5 — rebuild the tree's binaries

The twelve binaries were `rm -f`'d and rebuilt with `nice -n 10 make -k -j2 <twelve absolute paths>` against the restored working-tree sources, matching `app/build/` to the tree again. Not run (measured later by the stage gate).

## After

Measured by the stage gate run on the final tree, 2026-09-14.

### Command

`cd app && rm -f build/froggers_* build/check_no_juce && nice -n 10 make -k -j2 test > build/stage-gate.log 2>&1` — `MAKE_EXIT=2`. `make -k` continues past a failing target, so it still builds every one of the twelve binaries, `check-no-juce`, and the seven-run `check-delay-capacity-break-proofs` gate; only the top-level `test` recipe is not remade, because it depends on the failing `check-artifact-symbols-resolve` target. The twelve binaries below are run directly by absolute path as a result.

### Gate table

| Gate | Result |
|---|---|
| check_no_firmware_includes | OK |
| check-microphone-usage | OK |
| check-catalog-covers-screen-actions | OK |
| check-docs-match-parameter-table | OK |
| check-citations-resolve | OK |
| check-modified-requirements-restate-promoted | OK |
| check-no-planning-history | OK |
| check-spec-checks-resolve | OK |
| check-delay-capacity-parameters-are-swept | OK |
| check-artifact-symbols-resolve | FAIL — 6 unresolvable names; 2 under `openspec/changes/frogg3rs-randomize-depth-reclaim/`, 4 elsewhere (detail below) |
| check-no-juce | OK |
| check-delay-capacity-break-proofs | OK — 7/7 PASS (`modulation-bound-removed` 40.63s, `width-bound-removed` 39.72s, `modulation-dropped-from-width-budget` 39.82s, `wrap-admits-capacity` 4.14s, `lines-allocated-short` 3.94s, `off-grid-width-balance-headroom` 40.59s, `capacity-headroom-shortens-reads` 39.94s), total 208.77s |

`check-artifact-symbols-resolve` detail, one line per unresolvable name:

- `proposal.md:134` → `openspec/changes/frogg3rs-randomize-depth-reclaim/` — path is under that change's directory.
- `tasks.md:11` → `openspec/changes/frogg3rs-delay-width-wysiwyg-repair/` — different change directory.
- `tasks.md:129` → `strip_comments` names no test case and no symbol in this tree.
- `tasks.md:153` → `node_modules` names no test case and no symbol in this tree.
- `tasks.md:17` → `openspec/changes/frogg3rs-randomize-depth-reclaim/` — path is under that change's directory.
- `tasks.md:185` → `openspec/changes/frogg3rs-delay-width-wysiwyg-repair/tasks.md` — different change directory.

### Twelve-binary table

| Binary | Exit | Summary | Fail count | Failing tests |
|---|---|---|---|---|
| froggers_headless_tests | 0 | `[PASS] stop_silences_curve_one_grace_active_voice_within_bound` (8 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |
| froggers_mono_validation_tests | 0 | `[PASS] mono_group_drill_in_materializes_depth_parameter` (4 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |
| froggers_dsp_parity_tests | 0 | `191/191 tests passed` | 0 | none |
| froggers_parameter_model_tests | 0 | `[PASS] configure_processing_timing_is_wired_at_96khz_prepare` (12 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |
| froggers_modulation_tests | 0 | `49/49 tests passed` | 0 | none |
| froggers_audio_routing_tests | 0 | `[PASS] a_fast_parameter_sweep_with_no_reset_does_not_latch_the_instrument` (51 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |
| froggers_visualizer_tests | 0 | `[PASS] silent_comb_visualizer_plots_flat_unity_response` (4 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |
| froggers_scope_advance_index_tests | 0 | `[PASS] vco_scope_published_index_advances_across_successive_blocks` (2 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |
| froggers_marbles_clock_tests | 0 | `9/9 tests passed` | 0 | none |
| froggers_surface_tests | 0 | `[PASS] wav_encoding_produces_a_correct_pcm16_header_and_round_trips_sample_zero` (56 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |
| froggers_midi_catalog_tests | 0 | `[PASS] launchpad_defaults_are_registered_with_expected_ids_and_kind` (9 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |
| froggers_controllers_page_tests | 0 | `[PASS] launchpad_presets_pair_with_the_port_names_a_host_reports` (4 `[PASS]` lines, 0 `[FAIL]`) | 0 | none |

All twelve binaries postdate the newest compiled source (`app/dsp/Delay.hpp`, 19:00:01); binary mtimes run 19:27:17 through 19:28:08.

### Diff

Eleven of the twelve binaries hold the same pass count as the Before table, all still exit 0 with zero `[FAIL]` lines: froggers_headless_tests stays at 8 `[PASS]`, froggers_mono_validation_tests at 4, froggers_parameter_model_tests at 12, froggers_modulation_tests at 49/49, froggers_audio_routing_tests at 51, froggers_visualizer_tests at 4, froggers_scope_advance_index_tests at 2, froggers_marbles_clock_tests at 9/9, froggers_surface_tests at 56, froggers_midi_catalog_tests at 9, froggers_controllers_page_tests at 4. froggers_dsp_parity_tests moves from `186/186` to `191/191`, a net gain of five passing tests. `git diff --cached -- app/FroggersDspParityTests.cpp | grep '^+TEST_CASE'` shows six added test cases: `stereo_delay_width_balance_mapping_keeps_spread_at_or_below_todays_max`, `stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width`, `stereo_delay_width_spread_never_reads_past_the_line_capacity`, `stereo_delay_width_spread_bound_is_inert_away_from_capacity`, `stereo_delay_width_spread_bound_holds_across_the_reachable_grid`, `stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks`. The matching `-TEST_CASE` line shows one of the 186 removed by rename, `stereo_delay_width_balance_mapping_keeps_cross_in_0_1_and_spread_at_or_below_todays_max`, accounting for the six added minus one renamed equalling the five-test net gain.
