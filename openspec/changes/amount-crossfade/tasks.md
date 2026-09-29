The executor's deliverable is a report; code changes are a side effect of it.
A conflict between this file and the code is reported and stops that task.
Every changed or new assertion is shown to fail with its production change
reverted. One build or suite at a time, C++ at -j2 under nice; Sheaf's
`test` target runs alone and its only expected reds are the two braid4
96 kHz deadline tests. Build frogg3rs test binaries by their absolute paths
(`make -C app "$(pwd)/app/build/<name>"`) or through the `test` target;
a relative target name matches no rule and rebuilds nothing.

- [ ] S1 Sheaf `Parameter::ComputeAtDepth`
      (`External/Sheaf/projects/synth/src/ParameterModulation.cpp`): as
      `proposal.md`'s "What changes" states. Check: in
      `External/Sheaf/projects/synth/tests/parameter_modulation_tests.cpp`,
      `one_way_amount_target_adds_reach_and_ignores_disconnected_sources`
      expects `0.60` and `0.3`;
      `one_way_amount_two_routes_crossfade_and_renormalize_past_the_top` expects `0.56`, `0.92` with depths `0.5`
      and `0.4`, and a new renormalized case `0.5714286` with depths
      `0.5714286` and `0.4285714`;
      `one_way_amount_ignores_restsatzero_and_reads_the_route_directly`
      still expects `0.3`; NEW
      `one_way_amount_band_is_visible_at_full_knob` expects `0.75` at
      source `0`, `1.0` at source `1`, and published min/max `0.75`/`1.0`
      (read through `PopulateUIState` with `processLiteAlpha = 1.0f`, as the
      existing min/max tests do). Red: with the change reverted, the first
      reads `0.78`, the second `0.8`, the band test publishes `1.0`/`1.0`.
      The full Sheaf `test` target passes apart from the two known reds.
      Commit on branch `attenuverter-blend-mode`.
- [ ] 2 frogg3rs `app/FroggersModulationTests.cpp`:
      `crispy_depth_crossfades_toward_the_source_and_the_floor_disables_it` expects `0.8` and, after the floor, `0.1`;
      `saved_crispy_depth_positive_keeps_depth_negative_and_neutral_load_off`
      expects `0.5 * (1 - 0.25) + 0.25 * 1.0 = 0.625` for the saved positive
      depth (knob `0.5`, depth knob `0.75`, source `1.0`) and `0.5` for the
      floored and neutral ones;
      `crunchy_full_depth_crossfades_to_the_source_at_its_own_rest` expects `0.5`. Needs: S1. Check: `make -C app test` green;
      red: with S1 reverted in the Sheaf working tree for the run and
      restored after (never committed), the three read `0.9`, `0.9` and `0.75`.
- [ ] 3 Specs and docs: restate, as MODIFIED requirements in this change's
      spec deltas, every promoted requirement in Sheaf
      `External/Sheaf/openspec/specs/synth-parameter-modulation/spec.md` and frogg3rs
      `openspec/specs/froggers-sheaf-parameter-model/spec.md` that states
      the add-only law for a `kOneWayAmount` target, with the law above and
      the worked values; Sheaf's delta lives at
      `External/Sheaf/openspec/changes/amount-crossfade/specs/`. `MANUAL.md`:
      the Crispy/Crunchy modulation sentence says what the depth does now,
      for a first-time reader, checked per changed sentence by a context
      that wrote none of it. Check: the four `app/check_*.py` validators and
      `openspec validate amount-crossfade --strict` in both trees pass.
