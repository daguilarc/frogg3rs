# Tasks — `frogg3rs-random-sh-anomaly-gradient`

Preflighted 2026-09-06 (rulings in `proposal.md`). Tier per task: **H** =
mechanical (enumerate, substitute), **S** = decides what something means.
Builds under `nice`, `-j2` at most.

## 1. Preflight

- [x] 1.1 Every citation in `proposal.md` resolves at the current tree
      (re-cited where the first draft was off by a few lines).
- [x] 1.2 Enumerated by operand across the repo (External, build dirs
      excluded): `MakeSource` 23 (parity tests 12, lane header 6, slate 5:
      all rewritten by group 4), `spread` 50 (lane header 11, parity 6,
      marbles spec 2: this change; the rest are the Delay's stereo
      time-spread and prose "widespread": nothing needed), `quantizeLevels`
      7 (lane header only), `kFastCutoff` 8 (lane 5, FilterFx comments 2,
      parity comment 1: FilterFx comments reworded), `kSlowCutoff` 3 (lane
      2 deleted, FilterFx comment 1 reworded), `dejaVuKnob` 29 (lane 12,
      parity 5, clock test 1: this change; `src/core` 7 and firmware test 4:
      the firmware, untouched), `kRandomShSeeds` 6 (slate only),
      `randomSh6Output_` 3, `tickLane` 6 (slate only), `random walk` 2
      (modulation-slate spec: Sheaf's own walks, another meaning;
      DAISY_MANUAL: the firmware), `random-walk` 1 (marbles spec: the
      delta), `Random S&H` 37 (names only outside the marbles spec and
      MANUAL.md:131).
- [x] 1.3 `GangedRandomLfoVoice::Process` read: waiting holds the source,
      moving is `ShapedInterpolate(source, target, shape, progress)`, a
      blend of linear and raised-cosine `t`; monotone, so shaping the output
      is a monotone reshaping of the same path.
- [x] 1.4 `SetAlphaFromNatFreq`: `alpha = 1 - exp(-2 pi f)`; a time
      constant tau at fs gives `f = 1/(2 pi tau fs)`: 5 ms 6.63e-4, 20 ms
      1.66e-4, 100 ms 3.32e-5, 200 ms 1.66e-5 at 48 kHz.
- [x] 1.5 Before line recorded in `proposal.md` (Gates).

## 2. The measurement (finding first) — S

- [x] 2.1 `app/FroggersMarblesClockTests.cpp`: `random_sh_sources_rank_by_anomaly`.
      Drive one slate at 120 BPM and 48 kHz for 256 quarter notes,
      `PrepareBlockClock` once, `Step()` every sample, reading
      `RandomShLaneOutputForTest(i)` and `RandomSh6OutputForTest()` (both
      accessors added beside `CurrentGangedLfoInputForTest`); record mean
      |first difference| per sample and peak |first difference| per source.
      Print the six pairs. Assert both rankings 1 > 2 > 3 > 4 > 5 > 6
      strictly.
- [x] 2.2 Run it against today's constants: it must FAIL; record the printed
      six pairs as the before line. A pass here voids the gate.

## 3. Distribution shape, slew constants, constructor — S

- [x] 3.1 `RandomShLane.hpp`: `dsp::ShapeSpread(u01, spread)` per the
      proposal; `Process()` becomes shape, quantize, clamp, filter. Comment
      states the three checkpoints and the order's reason.
- [x] 3.2 Parity tests for the shape: spread 0.5 is the identity on 64
      uniform draws; spread 1.0 sends every draw to 0 or 1; spread 0 sends
      every draw to 0.5; mean absolute deviation from 0.5 is strictly
      ordered 0.25 < 0.5 < 0.75 on the same 64 draws.
- [x] 3.3 `lanes::SlewCutoff(tau)`, `kSlewReferenceSampleRate`, `kSlew5ms`,
      `kSlew20ms`, `kSlew100ms`, `kSlew200ms`; `kSlowCutoff` deleted;
      `FilterFx.hpp:706,733` comments reworded.
- [x] 3.4 Constructor `(seed, dejaVuKnob, cutoff, spread, quantizeLevels)`;
      `bagSize`, `stepChance`, `bias`, `size_`, `probability_`, `bias_` and
      the skip branch removed; `tickCount_`/`TickCount()` added; the struct
      comment rewritten as behaviour.
- [x] 3.5 Gate: run together with 4.5 (the shape change alone leaves the
      old narrow-lane test red by design, so groups 3 and 4 are one
      commit, `4339756`).

## 4. The table — S

- [x] 4.1 `RandomShLane.hpp` factories take the proposal's table (period is
      the caller's; deja vu, spread, quantize, cutoff here). Each factory's
      comment is its row of the table in words.
- [x] 4.2 `FroggersModulation.hpp:532-536`: tick multipliers 3, 2, 1, and
      periods of 2 and 4 quarters for lanes 4 and 5 (`quarterNotes / 2.0`,
      `quarterNotes / 4.0`, multiplier 1); the rate comments at `:46-48`
      and `:526-531` updated.
- [x] 4.3 `FroggersModulation.hpp:392`: source 6's output through
      `ShapeSpread` at `kSource6Spread` 0.25, the approximation stated in
      one sentence; `RandomShLaneTickCountForTest(laneIx)` accessor.
- [x] 4.4 Existing tests that assert the old characters (parity `:1189`,
      `:1201`, `:1214`, `:4638-4680`; clock `:108`) are rewritten to assert
      the new table, one assertion per row per axis, no test deleted without
      its property restated; the clock test counts ticks (24, 16, 8, 4, 2
      over eight quarter notes).
- [x] 4.5 Gate: `nice make -C app -j2 test`; 2.1 now passes and its six
      printed pairs go into the ledger beside the before line.

## 5. Redrawing the bags — S

- [x] 5.1 `RandomShLane::Reseed(uint32_t)`: rebuilds `rgen_` and refills the
      slots; the constructor calls it. Parity test: after `Reseed` with a
      different seed at least one slot differs; with the same seed all eight
      match (control).
- [x] 5.2 Slate: `LaneSeed`, `ReseedRandomShLanes(salt)`, the salted
      constructor with the default delegating through `std::random_device`.
      `RandomizeAll(manager, drillIn, model, slate)` reseeds at level 0 from
      `manager.NextRandomIndex(1 << 32)`; `FroggersAppCore.hpp:725` passes
      `modulation_`; the 20 test call sites pass `fx.slate`. Test in
      `FroggersModulationTests.cpp`: `RandomizeAll` changes at least one
      lane's bag; `RandomizePage` does not (control); a drilled-in
      `RandomizeAll` does not.
- [x] 5.3 `gangedRandomLfo6_` built from the salt. Test: two
      default-constructed slates differ in at least one bag; two slates with
      the same salt match; the ranking test passes a fixed salt; the explicit-seed factory
      property is already `random_sh_same_seed_reconstructed_lane_matches`.
- [x] 5.4 Gate: `nice make -C app -j2 test`.

## 6. Spec and manual — S

- [x] 6.1 The delta in `specs/froggers-marbles-modulator/spec.md`: the
      MODIFIED and ADDED requirements per the proposal; `openspec validate
      frogg3rs-random-sh-anomaly-gradient --strict`.
- [x] 6.2 `MANUAL.md:131`: replace the one sentence with the table in words:
      one line per source saying its period, how often it takes a fresh
      value, how far a value can land, whether it snaps to levels, and how
      fast it moves; source 6 as a slow glide to a new value near the centre.
      `QUICK_DICT.md:15` unchanged (it lists names only).
- [x] 6.3 Gate: `openspec validate --all --strict`.

## 7. Verification and delivery

- [x] 7.1 Every gate in the table on the final tree; VST rebuilt and
      `ctest`.
- [x] 7.2 §5 against the diff (S, fresh context): `ShapeSpread`,
      `SlewCutoff` and the four constants, `Reseed`, `LaneSeed`,
      `TickCount`, and the ranking test grepped by operand for a second
      computation.
- [x] 7.3 Postflight (S, fresh context): implementation versus this text.
- [x] 7.4 Push to `main`; ledger with the before and after six pairs.
- [ ] 7.5 OPERATOR: on the desktop app, lane 1 sounds jittery and extreme,
      lane 6 drifts near the middle, Randomize All changes the locked
      phrases.

## Ledger

Ranking test `random_sh_sources_rank_by_anomaly` (256 quarter notes at 120
BPM, 48 kHz), sources 1 to 6:

| line | mean abs first difference per sample | peak abs first difference |
|---|---|---|
| before (old table, fixed seeds, source 6 one random draw) | 8.71e-6, 2.26e-5, 1.54e-5, 1.48e-5, 4.31e-6, 1.07e-6 | 0.386, 0.836, 0.183, 0.941, 0.0122, 1.54e-5 |
| after (new table, salt 0x5A17) | 6.28e-5, 3.63e-5, 1.49e-5, 8.30e-6, 3.37e-6, 5.87e-7 | 0.941, 4.16e-3, 8.92e-4, 2.01e-4, 8.68e-5, 1.35e-5 |

Before: red on `sum[0] > sum[1]` (positive control). After: both orders
strict; the smallest ratio between neighbours is 1.8x (mean, 4 to 3... 3 to
4 at 1.8x) and 6.4x (peak, 5 to 6).

Gates after group 5: `make -C app test` exit 0; parity 147/147 (five tests
replaced by five), modulation 48/48 (two added), marbles clock 9/9 (one
added); `openspec validate --all --strict` 27 passed, 0 failed; VST rebuilt
(`cmake --build app/vst/build -j2` exit 0) and `ctest` `100% tests passed,
0 tests failed out of 5`.
Commits: `4339756` (groups 2 to 4, one commit: the shape change alone
leaves the old narrow-lane test red), group 5 in the commit above this
ledger's entry.

Postflight (fresh context, Sonnet, 2026-09-06): 2 text divergences, both
fixed in `proposal.md` (the redundant clamp; `FilterFx.hpp:733` names only
`kFastCutoff`); no stale comment for any of the ten old-table terms; FOUND
== CHANGED for every new concept. Scenario coverage it asked for:

| scenario | check |
|---|---|
| Locked deja-vu repeats a fixed loop | `random_sh_locked_deja_vu_replays_the_bag_as_a_fixed_loop` (parity), added after postflight; no source sits at exactly 0.5 |
| Low deja-vu keeps producing new values | `random_sh_rows_take_fresh_values_and_jump_as_the_table_says`, lanes 1 and 2 |
| High deja-vu replays in a scrambled order | same test, lanes 3 to 5 |
| The smooth source ignores deja-vu | source 6 has no deja-vu input; "glides rather than steps" is the ranking test's `peak[5] < peak[4]` |
| Smooth source movement duration follows tempo | `source_six_tempo_following_input_scales_inversely_with_quarter_notes_per_sample` (unchanged) |
| Activity and peak slope rank the sources | `random_sh_sources_rank_by_anomaly` |
| Spread's three fixed points hold | `shape_spread_fixed_points_and_deviation_ordering` |
| Every axis is monotone in the source number | period: the tick-count ordering loop in `per_source_rate_ratios_over_eight_quarter_notes`; slew: the alpha ordering in `random_sh_rows_slew_as_the_table_says`; quantization: the exact grid per row in `random_sh_rows_snap_to_the_table_grid`; spread: the per-row values are constructor arguments not readable from outside, so the aggregate is the ranking test |
| Advances track the clock | `per_source_rate_ratios_over_eight_quarter_notes` |
| A bar means four quarter notes | definitional: lane 4's eight steps at two quarters are the "four-bar phrase" in the factory comment and MANUAL.md; no runtime state |
| Tempo change tracks immediately / External MIDI clock / missing clock plan | unchanged scenarios, unchanged tests (`missing_transport_position_never_advances_the_five_lanes`; the clock tests in FroggersAudioRoutingTests.cpp) |
| Randomize All changes a locked phrase / Randomize Page leaves the bags alone | `randomize_all_redraws_the_bags_and_page_and_drilled_in_randomize_do_not` (modulation) |
| Launches differ, explicit seeds do not | `launches_seed_the_bags_differently_and_the_same_salt_reproduces_them` (modulation) and `random_sh_same_seed_reconstructed_lane_matches` (parity) |

After the two post-postflight tests: parity 148/148 and marbles clock 9/9
rebuilt and run by path; every other binary carried forward from the group
5 run (their inputs did not move); VST carried forward (no `app/vst` or
header edit after its build).
