# Baseline gate — `frogg3rs-effect-page-hierarchy`, 2026-09-10

Recorded before any file in this change was touched, per task A.

## Commands run

```
nice -n 10 make -C app test -j2
```

Then, independently, every test binary under `app/build/` that `make test`
depends on, run directly by path:

```
nice -n 10 app/build/froggers_headless_tests
nice -n 10 app/build/froggers_mono_validation_tests
nice -n 10 app/build/froggers_dsp_parity_tests
nice -n 10 app/build/froggers_parameter_model_tests
nice -n 10 app/build/froggers_modulation_tests
nice -n 10 app/build/froggers_audio_routing_tests
nice -n 10 app/build/froggers_visualizer_tests
nice -n 10 app/build/froggers_scope_advance_index_tests
nice -n 10 app/build/froggers_marbles_clock_tests
nice -n 10 app/build/froggers_surface_tests
nice -n 10 app/build/froggers_midi_catalog_tests
nice -n 10 app/build/froggers_controllers_page_tests
```

(`app/build/` also holds `check_no_juce`, `froggers_skeleton`, and a
`debug_truncation.dSYM` bundle. `check_no_juce` is a check binary invoked by
the `test` target itself, not a standalone test; `froggers_skeleton` is not a
dependency of the `test` target and was not run.)

## Result: `make -C app test -j2`

`make` did not stop early — it ran to completion. Exit code 0.

| Check / binary | Result |
| --- | --- |
| check-no-juce | OK |
| check-microphone-usage | OK |
| check-catalog-covers-screen-actions | OK |
| check-docs-match-parameter-table | OK (MANUAL.md 84/0 failures; QUICK_DICT.md 84/0 failures) |
| check_no_firmware_includes | OK |
| froggers_headless_tests | PASS (8/8) |
| froggers_mono_validation_tests | PASS (4/4) |
| froggers_dsp_parity_tests | PASS (149/149) |
| froggers_parameter_model_tests | PASS (12/12) |
| froggers_modulation_tests | PASS (48/48) |
| froggers_audio_routing_tests | PASS (51/51) |
| froggers_visualizer_tests | PASS (4/4) |
| froggers_scope_advance_index_tests | PASS (2/2) |
| froggers_marbles_clock_tests | PASS (9/9) |
| froggers_surface_tests | PASS (56/56) |
| froggers_midi_catalog_tests | PASS (9/9) |
| froggers_controllers_page_tests | PASS (4/4) |

Total `[PASS]` lines across the whole `make test` run: 356. Zero `[FAIL]`
lines anywhere in the log.

## Result: each test binary run independently, by path

All twelve exited 0 and reproduced the same pass counts `make` reported, with
zero `[FAIL]` lines in any of them:

| Binary | Exit | PASS | FAIL |
| --- | --- | --- | --- |
| froggers_headless_tests | 0 | 8 | 0 |
| froggers_mono_validation_tests | 0 | 4 | 0 |
| froggers_dsp_parity_tests | 0 | 149 | 0 |
| froggers_parameter_model_tests | 0 | 12 | 0 |
| froggers_modulation_tests | 0 | 48 | 0 |
| froggers_audio_routing_tests | 0 | 51 | 0 |
| froggers_visualizer_tests | 0 | 4 | 0 |
| froggers_scope_advance_index_tests | 0 | 2 | 0 |
| froggers_marbles_clock_tests | 0 | 9 | 0 |
| froggers_surface_tests | 0 | 56 | 0 |
| froggers_midi_catalog_tests | 0 | 9 | 0 |
| froggers_controllers_page_tests | 0 | 4 | 0 |

Sum: 356 PASS, 0 FAIL — matches the combined `make test` run exactly.

## Summary

**Everything passes today.** 5 check scripts OK, 12 test binaries green,
356/356 individual `[PASS]` assertions, 0 failures, in both the combined
`make test` run and every binary run independently by path. The gate did not
stop early this time (nothing failed to stop at). There is no pre-existing
failure for later waves to inherit; any failure introduced from here forward
is new.
