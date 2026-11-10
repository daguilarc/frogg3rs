# Validation

Validated in the managed `codex/engine-192k-lean-model` worktree.

## Native results

| Suite | Passed | Failed |
| --- | ---: | ---: |
| Sheaf parameters and matrix bridge | 360 | 0 |
| Sheaf DSP and rate conversion | 106 | 0 |
| Froggers DSP | 209 | 0 |
| Froggers routing | 62 | 0 |
| Froggers surface and recording | 65 | 0 |
| Froggers headless | 9 | 0 |
| Froggers parameter model | 14 | 0 |
| Froggers modulation | 61 | 0 |
| Froggers scopes | 2 | 0 |
| Froggers visualizers | 4 | 0 |
| Froggers clocks | 9 | 0 |
| Plugin host scenarios | 66 | 0 |
| Plugin smoke / latency | 1 | 0 |

Total: **968 passed, zero failed** across these suites, plus two plugin documentation-bundling checks.

Follow-up verification reran the full 360-case Sheaf parameter suite, 84 headless/parameter/modulation core cases, and the compiled plugin tests after the raw-cache invalidation fix. The full core routing run remains recorded below.

The routing suite completed all 62 scenarios, including the 200-draw randomization storm, deep patch storage/load, stop/freeze behavior, recovery and reset comparisons. The recording suite confirmed a 48000 Hz WAV header. Nonaligned 37-frame callbacks verified four engine samples per matrix frame; the 96 kHz host case verified a 384 kHz engine and converted smoothing.

## Formal results

`make -C External/Sheaf/projects/synth formal-test` passed with Lean 4.32.0: 31 theorem declarations, four reviewed source fingerprints, and a fresh transitive axiom audit. Only `propext`, `Classical.choice` and `Quot.sound` are allowed. Source drift and an introduced unsupported axiom were deliberately tested and rejected.

These are rational behavioral-model proofs, not a verification of C++ floating-point execution. See `External/Sheaf/projects/synth/formal/README.md` and its correspondence manifest for exact boundaries. The recursive evaluator is retained with unchanged scalar-depth reuse; a full prepared-graph replacement is not claimed.

## Controls and measurements

- Removing source filtering failed the 47 kHz alias-rejection check; 1 kHz remained the positive control.
- Removing output interpolation failed the multivoice publication check.
- Reusing a stale scalar leaf after a scene-reference edit failed its invalidation regression. Restoring a raw knob cache also reproduced a stale leaf (0 instead of 0.8); that write now invalidates the image and the regression passes.
- Gating matrix processing while frozen failed the live-edit regression. The matched unchanged arms had zero RMS difference; changing delay mix produced approximately 0.60 RMS difference.
- FIR reconstruction initially exceeded full scale in the randomization storm. Moving the final hard bound after reconstruction resolved that failure; the complete storm now passes.
- One recovery probe originally lay within five blocks of the new engine write cursor. Moving the probe beyond that cursor travel preserved the test's intended recovery measurement.
- The moving-delay test now predicts the moving read cursor's impulse crossing rather than assuming the injection-time tap stays fixed.

A headless O2 benchmark rendered two seconds of the default patch at engine 192000 Hz / matrix 48000 Hz in approximately 1.14 seconds, with 591354 scalar-leaf cache hits and nonzero finite audio. This measures default-patch throughput on this Mac, not a deadline guarantee for arbitrary nested patches or other devices.

## Artifacts and checks

The standalone macOS bundle built successfully at `app/build-launcher/Frogg3rs.app`. Full plugin artifacts also built:

- `app/vst/build/FroggersVst_artefacts/Release/VST3/Frogg3rs.vst3`
- `app/vst/build/FroggersVst_artefacts/Release/AU/Frogg3rs.component`

All three bundles passed signature verification. The real compiled processor passed 66 plugin-host scenarios and the smoke test, including an explicit 36-host-sample latency assertion for the 143-engine-sample output FIR. Both plugin documentation-bundling tests passed. Plugins were not installed. Browser/mobile binaries, a real DAW and live device playback were not tested.

Incremental-build checks reproduced a missing dependency on `RateSeparatedParameters.hpp`: the parameter-test target previously stayed up to date after a virtual header change. The test and launcher dependency lists now include the bridge. Dry-run controls show an unchanged launcher stays up to date and a bridge-header change schedules recompilation.

The JUCE boundary, firmware-include boundary, parameter-table documentation, specification references, promoted-requirement consistency, planning-history and artifact-symbol checks passed. Both repository diffs passed whitespace checks.

Changes remain uncommitted in the parent worktree and its Sheaf submodule. The separate Desktop checkout's uncommitted density/randomization changes were preserved and are not included in this worktree's committed baseline.
