# Correct the two engine/matrix conversion boundaries

Diego needs unwanted conversion artifacts reduced without losing SRR1/2, Delay Crush, exact static controls or the existing randomization laws. This change is scoped to those two conversion boundaries and their checks. It does not model the whole instrument or claim every nonlinear alias is removed.

## Final pipeline

Host input / engine modulation sources → independent fixed source low-pass histories → phase-zero editable matrix / Fuego seam → existing four-phase linear interpolation → independent low-pass root histories → bounded exact-static-target settlement → existing normalized DSP-cache setter → audio DSP → unchanged output FIR / output guard → host output.

The engine/matrix ratio remains4:1:192k/48k with the preferred48k host. Source histories freeze when transport stops; matrix edits and root histories continue. Recursive depths, voice/scene semantics and mode-two modulation-source randomization are unchanged. SRR1/2 and feedback-only Delay Crush retain the accepted build's algorithms, mappings and processing positions.

## Runtime design

DspRateConversion.hpp contains one authoritative30-value binary32 coefficient table: original unity-DC order11 elliptic, six SOS. Source instances and root batch share the same five-term float section-step helper. Configure initializes each actual section at its DC equilibrium using double intermediate gain and float histories; no512-sample priming approximation remains.

Root processing uses section-major independent x1/x2/y1/y2 arrays, configured offaudio. Process validates the lane span, processes in place without allocation, and retains scalar operation order. Configure gathers roots in parameter-major/voice-minor order. Each engine step gathers unchanged FOH inputs, filters the complete root batch, then publishes in the existing order. Sources remain scalar. No fastmath, transposed recurrence, coefficient rescaling, shared histories or constant-input processing shortcut is introduced.

Float IIR DC error broke exact static Freeze/Scoop requirements. A root's unchanged-target age starts0, resets0 on an exact phase-zero target change, and increments before eligibility once per engine sample, saturating4096. Only at age>=4096 and abs(double(filtered)-double(target))<=double(2e-5f) is exact target selected for publication. Otherwise filtered output is selected. Existing normalized clamping follows. Filters always continue; published correction is not fed back into histories. This is a bounded publication policy, not a guarantee that every arbitrary history settles by4096. The first sample of a new target counts1.

## Acceptance and mathematical scope

The actual stored-coefficient linear response is kernel-certified continuously over normalized passband[0,5/48] and stopband[1/8,1/2], including endpoints. Pass power lies in[.981,1.0001]; stop power<=.981e-9, hence stop/pass power<=1e-9. These are0–20k /24–96k at192k. The20–24k transition is intentionally excluded. All six denominators are strictly positive; all39Bernstein intervals are assembled and bound to the actual exported coefficients.

Native source probes require ±.1dB passband /−90dB stopband with paired endpoint phases and live signal controls. Root probes preserve the accepted FOH baseband and require each final image at48k−f,48k+f,96k−f to be≤−90dBc relative to the live final fundamental. This does not demand another90dB reduction of an already tiny original FOH image. Interior probe publication must equal independent scalar replay exactly and remain unclipped. Static settlement is inactive for those moving probes.

Settlement correction relative to existing normalized filtered publication is bounded by1/50000 in the abstract model. This pointwise statement is separate from the pure linear response certificate; no universal90dB nonlinear output or compiled-C++ refinement is claimed. Existing generic state/rounding lemmas remain conditional; obsolete actual four-section instances are removed. No new SRR or whole-engine model is added.

## Dependency graph and review

Fresh hygiene/measurements → structural specification → derived execution plan and independent pre-code review → fixed converter/bridge → batch runtime/native controls → bounded formal repair → exact-static publication repair → fresh final reviews → cleanup/build/launch of an experimental desktop for Diego. Runtime/model execution was serialized.

Impact: synth/include/synth converter and bridge; native tests; coefficient exporter/checker; explicit synth/Makefile and runtime/juce_build.mk header dependencies; formal proofs/generator/gate/docs; current conversion and prior engine-rate specification truth. Shared BiquadSection, output FIR, Braid-4, firmware, effect implementations and randomization implementations remain unchanged.

Independent tests compare scalar/batch against unchanged BiquadSection arithmetic for0/1/4/15/91lanes, reconfiguration/tails/permutations, and reject shared-helper corruption. Publication tests cover4095/4096, exact endpoints/interiors, tiny changes, reset and history continuity. Existing app Freeze/Scoop assertions remain unchanged. The200-draw per-channel silence/finite-output storm is retained, with a real mute control. Formal identity, coverage, attenuation, denominator and coefficient mutations must reject.

## Controlled test-build performance boundary

The initial80%wall-p99 target was not achieved by baseline or candidate and is not called passed. Paired headless measurements before the final settlement branch showed the batch candidate's median whole-callback CPU cost7–10% above baseline, with variable tails and deadline overruns. Timing is not a guarantee of device performance; one sample-and-hold lane did not change in the measured window. Busy/wait and actual output-zero controls expose instrument limits; the original attempted silent fixture did not mute and is not credited.

Fresh adjudication permits a controlled experimental desktop build after functional repair/review, with those limitations explicit. It does not establish overload-free operation or production readiness. No repeated identical benchmark loop is needed to make that user-authorized test artifact. Diego retains merge/push approval after testing. Clean old generated app/plugin build outputs before rebuilding and launch the new desktop.
