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

The original implementation was checkpointed in local parent and Sheaf commits before rebasing. The separate Desktop checkout was preserved.

## Rebase and operator correction (2026-11-09)

Rebased cleanly onto `46291acd`, fetched from the Froggers remote. Main did not
change the Sheaf gitlink. The source-count quartile policy in that main commit
was a product-intent error: the operator clarified that quartiles choose how many
parameters receive modulation. Selected parameters retain a source-count mode of
two, with geometric falloff in both directions. The current code, manual, native
checks and Lean model implement that separation. See
`openspec/changes/randomization-coverage-source-bias/`.

Post-correction checks:

- 49 Lean theorem declarations, five reviewed source fingerprints and fresh axiom audit passed.
- 361 Sheaf parameter tests, 72 modulation tests, nine headless and 14 parameter-model tests passed.
- The final optimized code passed all 200 actual-audio Randomize All stress draws: finite, bounded and non-silent on both channels.
- Reintroducing dense source-count quartiles failed the source-weight/coverage regressions; ignoring target coverage failed the coverage regression. These controls were compiled in temporary copies, not the production tree.
- Twelve repeated randomizations each rendered 0.501 seconds of finite audible audio. After profiling and exact scalar-curve reuse/absent-inactive-route skipping, measured render times were approximately 0.36–0.42 seconds, versus roughly 0.48–0.59 seconds before those optimizations. These are throughput measurements, not a universal deadline guarantee.
- Source boundaries, documentation, specification references, promoted clauses, artifact symbols and whitespace checks passed.

All 62 distinct routing scenarios are covered by the completed broad run and
subsequent targeted reruns. The corrected, final optimized code reran the 200-draw
storm, all 34 scenarios after the old run's stopping point, and the corrected
same-tick Load pressure fixture. The earlier broad run covers the other completed
routing cases. The obsolete broad process was terminated before operator testing;
no stress or storage test remains running.

The same-tick Load fixture now explicitly creates retained storage pressure instead
of relying on random growth to reach a particular capacity. Its assertion remains
unchanged. The rerun measured 1521 available slots for an 81-depth patch, leaving
exactly one 1440-slot launch batch; Load applied in the same block, all 1092 forced
routes were written, all 75 saved page-depth paths were present, and no partial draw
was reported. This setup change does not change production allocation behavior.

Build hygiene removed this worktree's old `app/build`, `app/build-launcher`, and
`app/vst/build` outputs (approximately 840 MB) before rebuilding the core tests
and desktop app. The corrected desktop bundle passed signature verification and
was launched from this exact worktree. Plugin artifacts were removed and have
not been rebuilt for this correction. No plugins were installed.

The operator confirmed the corrected desktop build “works great.” Merge and push
remain on hold pending explicit approval. All changes are saved locally.

## Formal source-filter alias bounds (2026-11-09)

Superseded: this section and everything after it describe the earlier
four-section source filter. The converter, its certificate and its checks are
now those of change `conversion-alias-rejection`.

Added 20 real-valued aliasing theorems: the full gate now verifies **69 theorem
declarations and seven reviewed sources**, with only the same three allowed
standard Lean axioms. Mathlib v4.32.0 and its transitive dependencies are pinned
in the Sheaf Lake manifest. No globally installed Lean packages are required.

A fresh C++ exporter checks the exact bit patterns of all 20 configured source
filter coefficients and their exact real embeddings. The proofs derive the
biquad squared-magnitude polynomial, verify each concrete stage certificate,
combine the cascade bounds, and connect continuous frequency bands using real
trigonometric inequalities. At a 192 kHz engine, 47..96 kHz power gain is bounded
by 1/4000000 and 0..1 kHz power gain is at least 1/2. Relative amplitude gain is
therefore at most 1/1000 (at least 60 dB rejection). The 47 kHz to 1 kHz sampling
fold is proved explicitly.

The native parameter suite passed **362 cases**, including the existing actual
binary32 time-domain alias test and a new 8193-point-per-band frequency-response
cross-check. A one-bit coefficient snapshot drift was rejected by the export
gate. Doubling the first stage numerator in a temporary proof copy caused Lean
to reject the attenuation certificate. Both negative controls failed as intended.

These are exact-real, fixed-coefficient steady-state response proofs. They do not
bound per-sample IEEE error, transients or pole convergence, the transition band
just above 24 kHz, nonlinear DSP, oscillator generation, root imaging, the output
FIR or whole-synth aliasing. The bridge cutoff expression was factored into a
named constant for shared runtime/export use without changing its numeric value.
The already-tested desktop app remains open; no desktop or plugin rebuild was
needed for this proof-only behavior extension. See
`External/Sheaf/projects/synth/formal/ALIASING.md` for full correspondence boundaries.

## State, rounding and composition extension (2026-11-10)

Historical validation below describes the previous four-section converter. Its
concrete energy/512-sample priming claims are superseded by conversion-alias-rejection
Stage C: six-section response, conditional generic state algebra and DC-equilibrium
Reset. Current contracts and gate are in `External/Sheaf/projects/synth/formal/`;
the dated measurements below are not validation of the replacement converter.

The fresh gate now verifies **139 theorem declarations and 10 reviewed sources**,
with only the same three standard Lean axioms. The native coefficient exporter
checks **20 source-filter coefficients plus all 287 stored binary32 FIR taps**.

Added full direct-form-I memory and its relation to the feedback recurrence,
two-sample input-history alignment, concrete positive energy certificates,
exponential initial-history decay, convergence of homogeneous energy, and bounded
adversarial disturbance accumulation. Mathematical nearest-even binary32/binary64
models include gradual subnormals, FTZ/DAZ allowances, primitive/local/state budgets
and the actual mixed-precision FIR operation pattern under explicit finite guards.
The FIR has certified L1 gain at most 9/4 and finite history; final clipping is
proved nonexpansive for value error. These are guarded mathematical contracts,
not compiler/hardware refinement proofs or an all-patch spectral guarantee.

Native verification passed **685 cases**: 366 Sheaf parameter/filter, 108 Sheaf
DSP, and 211 Froggers DSP scenarios. This includes actual ARM64 FPCR FTZ/DAZ
checks with environment restoration, exact state-copy and tap-order checks,
512-sample four-section priming, and the existing time/frequency alias tests.
Measured peak local biquad residual was about 7.87054e-8 (guarded budget about
4.29153e-6); peak FIR mixed-precision error was about 5.15049e-8.

The nonlinear extension proves within-cell error preservation for a fixed code,
explicit offset-jump budgets for cell/control-code crossings, and two-stage warp
composition. It also proves a discontinuity witness for the actual maximum-Fuego,
row-zero integer transformation. Native adjacent float inputs differed by
4.65661287e-10 but their outputs differed by 0.0823529437. The bypass control
preserved the inputs; 36,720 within-cell mask/row/cell pairs passed. Therefore the
linear filter guarantee cannot simply be carried through the bit warp without
those guards/jump terms. This does not assert non-Lipschitzness of the finite
binary32 domain or formally establish a full audio-output counterexample.

Negative controls rejected an unstable stage coefficient, a one-bit output FIR
snapshot drift, and FE_TOWARDZERO in the nearest-even native check. The reviewed
contracts and detailed limits are in
`External/Sheaf/projects/synth/formal/STATE_AND_ROUNDING.md`.

No production DSP behavior was changed; this extension adds models, coefficient
correspondence, tests and documentation. The already approved desktop sound was
preserved. No desktop/plugin rebuild, install, merge or push was performed.

Source-boundary/documentation checks and both whitespace checks passed. The spec
check markers were normalized to the parser's plain `Check:` form; all 136 case
references now resolve, including the state/arithmetic cases above.
