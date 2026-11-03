# Add Sheaf mipmapped VCO waveforms

## Why
Diego requests frequency-adaptive saw and square waveforms using the existing Sheaf dependency, supplementing the shipped 4× oscillator processing and halfband FIR decimation while preserving Shape and modulation topology. His current instruction authorizes proposal repair, execution and execution audit.

## What Changes
- Add one immutable shared bank containing two Sheaf AdaptiveWavetable<12> objects, generated in place and prepared during Vco construction before processing.
- Replace direct saw/square evaluation with Sheaf lookup selected by carrier Hz and half of the 4× internal sample rate. Keep analytic sine, ascending saw polarity, positive-first-half square, and the existing two Shape crossfades.
- Pass the prepared bank explicitly into waveform evaluation; Process reads its already prepared bank without a lazy initialization call.
- Guard nonfinite lookup operands before Sheaf's floating-to-index conversion, propagating a nonfinite signal for existing recovery instead of indexing with NaN.
- Preserve four complete PM/waveform/ring substeps, held controls, both halfbands, phase ordering, pitch/defaults, modulation floors, reset and recovery.

## Impact
Implementation: app/dsp/Vco.hpp, app/FroggersDspParityTests.cpp, and the existing VCO audio identity test in app/FroggersModulationTests.cpp; update the waveform provenance comment in app/FroggersModulation.hpp if its ported-formula wording becomes false. Planning artifacts: this proposal, design.md and tasks.md. Reuse External/Sheaf headers without submodule changes. The existing spec delta is retained. Firmware src/core/VcoWaveEval.hpp remains a separate implementation and is outside this app change. No control, patch format, release, UI or manual change.

## Non-goals
No 2× reduction, filter redesign, CPU-saving or alias-free claim, listening gate, broad spectral comparison, large render matrix, dependency installation, commit, push or release. Sheaf normalization and level interpolation are accepted; exact saw/square samples change. Sonic improvement and performance magnitude are not acceptance claims.

## Authority and dependencies
Read /Users/diegoaguilar-canabal/Desktop/omni-rule.md in full. Its chain is “OpenSpec (structural decomposition, at the threshold below) → proposal (the derived plan) → execution.” Existing spec delta → this proposal/design → independent before-code audit → execution and controls → independent after-code audit → delivery. The user's latest instruction supersedes the earlier proposal-only stop. Execute the dependency sequence in tasks.md after the independent audit accepts this repaired plan.
