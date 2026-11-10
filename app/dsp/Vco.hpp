#pragma once

// synth_froggers::dsp::Vco -- a DSP port of Froggers' VCO, exposing Sheaf's
// VCO UIState. Pitch/modulation follow the cited Froggers formulas; saw and
// square now use Sheaf mipmapped tables. The original provenance follows --
// see the per-block citations below, each read directly from the firmware
// source before porting.
//
// Ported from:
//   - 08b5fd3:src/core/FroggersEngine.hpp:439-441   pitch 20 Hz-20 kHz exp map
//   - 08b5fd3:src/core/FroggersEngine.hpp:135-137   x_pmLfoMinHz/MaxHz/Depth
//   - 08b5fd3:src/core/FroggersEngine.hpp:147-148,150-165  PmDepthScale smoothstep
//   - 08b5fd3:src/core/FroggersEngine.hpp:706-712   StepIndependentPmLfo
//   - 08b5fd3:src/core/FroggersEngine.hpp:735-744   the m_simIndependentPm branch
//     (the depth multiply x_pmLfoDepth * PmDepthScale(knob) lives at this
//     CALLER, not inside StepIndependentPmLfo -- ported that way here too)
//   - 08b5fd3:src/core/VcoWaveEval.hpp:7-23         EvalWaveMorph sine->saw->square
//
// NOT ported (deliberately): the legacy `else` branch at
// 08b5fd3:src/core/FroggersEngine.hpp:746-755, which uses XCPL cross-coupling
// This struct has zero cross-VCO terms by construction -- it holds no
// reference to any other Vco instance, so "porting only the independent-PM
// branch" and "zero cross-VCO terms" are the same guarantee expressed one
// way: nothing here *could* reach another VCO's state.
//
// ============================================================================
// Sheaf's own `WavetableVco<Bits>`
// (External/Sheaf/projects/synth/include/synth/DspOscillators.hpp) is the
// cited shape this struct must conform to: `UIState{connected, scope,
// scopeChannel, scopeColor}`, `SetScopeWriterHolder()`, `SetScopeColor()`
// and `PopulateUIState()` -- `Process` is not part of the cited UIState
// surface. Sheaf's own reference
// struct embeds this directly alongside its DSP `Process()`, in the SAME
// file/class, rather than through a separate wrapper -- this port mirrors
// that placement exactly (same reasoning as FilterFx.hpp's ResonantBump/Comb
// UIState), which is why this otherwise-pure DSP file takes on a deliberate,
// Sheaf dependency: `synth/DspScope.hpp` (ScopeWriter/
// ScopeWriterHolder, header-only, no link dependency) and `synth/Color.hpp`/
// `synth/AtomicColor.hpp` (also header-only). This widens app/dsp/*.hpp's
// usual no-Sheaf-dependency convention deliberately, not incidentally;
// `app/Makefile`'s DSP_TEST_BIN rule gains Sheaf's -I include path
// accordingly (no link-time dependency: everything reached here is
// header-only inline). Mipmapped saw/square additionally reuse the header-only
// `synth/DspWavetable.hpp` generator and evaluator, prepared at construction.
//
// Deliberately NOT ported: `WavetableVco::Process`'s cycle-boundary
// RecordStart/marker bookkeeping, inside `WavetableVco::Process`
// (External/Sheaf/projects/synth/include/synth/DspOscillators.hpp). This struct
// does not write to the scope at all -- FroggersAppCore.hpp writes the
// gated sample to scopeWriterHolder_ after MixOscVoices runs (see
// SetScopeWriterHolder() below and RouteAudioSample() in
// FroggersAppCore.hpp) -- and that call site does not reproduce the
// marker/cycle-alignment bookkeeping either, so it stays unimplemented
// throughout, not silently dropped.

#include "DspMath.hpp"
#include "HalfbandDecimator.hpp"

#include "synth/AtomicColor.hpp"
#include "synth/Color.hpp"
#include "synth/DspScope.hpp"
#include "synth/DspWavetable.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <limits>

namespace synth_froggers::dsp {

// Prepared once during Vco construction, before any audio processing. Generate
// directly into static storage: the bank is large, while FFT scratch is local.
struct VcoWaveformBank
{
    synth::AdaptiveWavetable<12> saw;
    synth::AdaptiveWavetable<12> square;

    VcoWaveformBank()
    {
        for (std::size_t i = 0; i < synth::BasicWavetable<12>::kTableSize; ++i)
        {
            const float phase = static_cast<float>(i) / synth::BasicWavetable<12>::kTableSize;
            saw.m_waveTable.m_table[i] = 2.0f * phase - 1.0f;
            square.m_waveTable.m_table[i] = phase < 0.5f ? 1.0f : -1.0f;
        }
        for (auto* table : {&saw, &square}) table->Generate();
    }
};

inline const VcoWaveformBank& PreparedVcoWaveforms()
{
    static const VcoWaveformBank bank;
    return bank;
}

// Shape retains the firmware's sine->saw->square crossfades; the saw/square
// samples are now normalized, frequency-adaptive Sheaf waveforms.
inline float EvalWaveMorph(const VcoWaveformBank& bank, float phaseWrapped01, float morph,
                           float carrierHz, float maxHz)
{
    if (!std::isfinite(morph))
    {
        morph = 0.0f;
    }
    // Corrupted recursive phases must remain visible to recovery, without
    // reaching Sheaf's floating-to-index conversion. Its early frequency
    // rejection handles nonpositive carrier and ceilings below the carrier.
    if (!std::isfinite(phaseWrapped01) || !std::isfinite(carrierHz) || !std::isfinite(maxHz))
        return std::numeric_limits<float>::quiet_NaN();
    if (carrierHz > 0.0f && maxHz >= carrierHz)
    {
        const float ratio = maxHz / carrierHz;
        // size_t's exclusive upper bound is exactly a power of two; comparing
        // in long double avoids rounding SIZE_MAX upward before conversion.
        const long double indexLimit = std::ldexp(1.0L, std::numeric_limits<std::size_t>::digits);
        if (!std::isfinite(ratio) || static_cast<long double>(ratio) >= indexLimit)
            return std::numeric_limits<float>::quiet_NaN();
    }
    const float sine = Sine01(phaseWrapped01);
    const float saw = bank.saw.Evaluate(phaseWrapped01, carrierHz, maxHz);
    const float square = bank.square.Evaluate(phaseWrapped01, carrierHz, maxHz);
    if (morph <= 0.5f)
    {
        const float t = morph * 2.0f;
        return sine * (1.0f - t) + saw * t;
    }
    const float t = (morph - 0.5f) * 2.0f;
    return saw * (1.0f - t) + square * t;
}

// One Froggers VCO: pitch (exp map), morph (sine->saw->square), and an
// independent per-VCO PM sine LFO, gated to exactly zero depth at/below the
// knob's zero-off floor. Three of these, each fed its own knobs, reproduce
// the ported topology with zero cross-VCO terms.
struct Vco
{
    Vco() : waveforms(&PreparedVcoWaveforms()) {}

    // Const pointee, shared across every audio/modulation VCO. Construction
    // eagerly prepares it; Process never acquires a function-local static.
    const VcoWaveformBank* waveforms;
    // Verbatim member names and types, matching Sheaf's own `UIState` shape
    // (External/Sheaf/projects/synth/include/synth/DspOscillators.hpp).

    struct UIState
    {
        std::atomic<bool> connected{false};
        std::atomic<const synth::ScopeWriter*> scope{nullptr};
        std::atomic<std::size_t> scopeChannel{0};
        synth::AtomicColor scopeColor;
    };

    // pmRateKnob01 in [0,1] maps exponentially across [kPmLfoMinHz,
    // kPmLfoMaxHz] (StepPmLfo below): kPmLfoMinHz is the slowest rate the
    // knob reaches, kPmLfoMaxHz the fastest, and because the map between
    // them is exponential rather than linear, the knob's middle position
    // is their geometric mean -- sqrt(kPmLfoMinHz * kPmLfoMaxHz) -- not
    // the arithmetic average, so moving either endpoint also moves where
    // the middle of the knob's travel lands. kPmLfoMaxHz and kPmLfoDepth
    // match the parity reference exactly (08b5fd3:src/core/FroggersEngine.hpp:136-137,
    // x_pmLfoMaxHz/x_pmLfoDepth). kPmLfoMinHz does not: the reference's
    // x_pmLfoMinHz (0.05f, 08b5fd3:src/core/FroggersEngine.hpp:135) is a ~20-second cycle,
    // slow enough to double as a second off switch. That job already
    // belongs entirely to each VCO's own PM depth knob, which gates the
    // offset to exactly zero at/below kPmLfoFloor (PmDepthScale below) --
    // so this floor's only purpose is to bound how slow an AUDIBLE rate
    // is allowed to get, never to silence it. A cycle within a few
    // seconds reads as motion; one that takes tens of seconds reads as
    // drift.
    // What the ear hears from phase modulation is the pitch deviation, the
    // slope of the phase offset: 2 pi x (offset amplitude) x rate Hz, which
    // carries no carrier term -- the same Hz of deviation on every VCO, and
    // so more cents on a low carrier than on a high one. StepPmLfo scales
    // the LFO by kPmLfoMaxHz / rate, which holds that slope constant: the
    // phase swing grows as the rate falls, and the peak deviation is
    // 2 pi x kPmLfoDepth x kPmLfoMaxHz = 18.8 Hz at full depth at every
    // position of the rate knob. The rate knob sets only how fast the
    // wobble runs, the depth knobs only how far it goes.
    static constexpr float kPmLfoMinHz = 2.0f;  // 0.5 s/cycle at the floor.
    static constexpr float kPmLfoMaxHz = 20.0f;
    static constexpr float kPmLfoDepth = 0.15f;  // cycles of phase swing at kPmLfoMaxHz; more below it

    // 08b5fd3:src/core/FroggersEngine.hpp:147-148 (x_pmLfoFloor/x_pmLfoRampWidth).
    static constexpr float kPmLfoFloor = 0.02f;
    static constexpr float kPmLfoRampWidth = 0.08f;

    // Pitch spans six octaves continuously from A0 (27.5 Hz) to A6 (1760 Hz),
    // with equal knob movements producing equal frequency ratios.
    static constexpr float kPitchMinHz = 27.5f;
    static constexpr float kPitchMaxHz = 1760.0f;

    // The internal ring-mod carrier's own frequency range (Ring Mod, Audio
    // slots 9-11) -- its own named constants, not a reuse of
    // kPitchMinHz/kPitchMaxHz above. 20 Hz-5000 Hz covers sub-audio "throb" through the
    // classic clangy ring-mod register while staying well below Nyquist at
    // every sample rate this app supports, so the carrier itself never
    // folds into noise before the ring-modulated product does.
    static constexpr float kRingModMinHz = 20.0f;
    static constexpr float kRingModMaxHz = 5000.0f;

    // Ring Mod's OWN zero-off floor/ramp -- a separate pair of
    // constants from kPmLfoFloor/kPmLfoRampWidth (not merely a separate
    // call), since a bare product carrier is a much more drastic audible
    // change at any nonzero depth than PM's phase offset, so a slightly
    // higher floor and wider ramp keeps the bottom of the knob's travel
    // forgiving.
    static constexpr float kRingModFloor = 0.05f;
    static constexpr float kRingModRampWidth = 0.10f;

    static constexpr int kOversampleFactor = 4;
    // Two 2:1 stages: 4x -> 2x -> base rate. Physical FIR group delay is
    // 23/4 + 23/2 = 17.25 base samples; output is not latency-compensated.
    std::array<HalfbandDecimator2x, 2> antiAliasFilter;

    float carrierPhase = 0.0f;
    float pmLfoPhase = 0.0f;
    // This VCO's own internal ring-mod carrier phase -- stepped the
    // same way carrierPhase is (WrapPhase(phase + increment)), never derived
    // from or read by any other Vco instance.
    float ringCarrierPhase = 0.0f;

    // The running count of consecutive seconds this unit's state has stayed
    // over dsp::kMaxUnitStateMagnitude (dsp/RecoveryTier.hpp; checked by
    // app/FroggersAppCore.hpp's RecoverUnitIfNeeded), owned HERE rather than in FroggersAppCore -- it
    // is this unit's own recovery bookkeeping, not shared state, so it
    // belongs with the rest of this struct's state rather than in a
    // parallel member on the enumerating parent, which would need a second,
    // address-keyed lookup table to pair a unit back to its own counter
    // once the enumeration went hierarchical.
    float overCeilingSeconds = 0.0f;

    // 08b5fd3:src/core/FroggersEngine.hpp:439-441 -- one VCO's pitch knob (0..1) mapped
    // exponentially across kPitchMinHz-kPitchMaxHz, expressed as a phase
    // increment (cycles/sample: freq/sampleRate) so Process() only
    // adds-and-wraps.
    static float PitchToPhaseIncrement(float pitchKnob01, float sampleRate)
    {
        return ExpMapCompute(kPitchMinHz / sampleRate, kPitchMaxHz / sampleRate, pitchKnob01);
    }

    // 08b5fd3:src/core/FroggersEngine.hpp:150-165 (PmDepthScale). Thresholds at :147-148.
    // Body lives in dsp::TrueZeroDepthTaper (DspMath.hpp), shared with any
    // other knob needing the same true-zero taper shape with its own
    // floor/ramp width.
    static float PmDepthScale(float pmKnob01)
    {
        return TrueZeroDepthTaper(pmKnob01, kPmLfoFloor, kPmLfoRampWidth);
    }

    // This VCO's own ring-mod depth taper uses TrueZeroDepthTaper with Ring
    // Mod's own floor/ramp (kRingModFloor/kRingModRampWidth above) -- NOT
    // kPmLfoFloor/kPmLfoRampWidth, no second copy of the taper itself.
    static float RingModDepthScale(float ringModKnob01)
    {
        return TrueZeroDepthTaper(ringModKnob01, kRingModFloor, kRingModRampWidth);
    }

    // Ring-mod carrier's phase increment -- same ExpMapCompute
    // shape PitchToPhaseIncrement uses for pitch, over kRingModMinHz/MaxHz.
    static float RingModPhaseIncrement(float ringModKnob01, float sampleRate)
    {
        return ExpMapCompute(kRingModMinHz / sampleRate, kRingModMaxHz / sampleRate, ringModKnob01);
    }

    // 08b5fd3:src/core/FroggersEngine.hpp:706-712 (StepIndependentPmLfo) -- advances this
    // VCO's own PM LFO by one sample; returns its PRE-advance sine value,
    // scaled so the pitch deviation it produces does not fall with the rate.
    // The RATE argument is the shared PM-rate knob (Audio slot 12, one knob
    // feeding all three VCOs' StepPmLfo calls), decoupled from the per-VCO
    // PM depth knob that drives only PmDepthScale -- pmRateKnob01 maps
    // exponentially to [kPmLfoMinHz, kPmLfoMaxHz].
    float StepPmLfo(float pmRateKnob01, float sampleRate)
    {
        const float hz = ExpMapCompute(kPmLfoMinHz, kPmLfoMaxHz, pmRateKnob01);
        // Scaled so the pitch deviation does not fall with the rate (see
        // kPmLfoMinHz above): unity at the top of the knob, ten at the floor.
        const float lfoValue = Sine01(pmLfoPhase) * (kPmLfoMaxHz / hz);
        pmLfoPhase = WrapPhase(pmLfoPhase + hz / sampleRate);
        return lfoValue;
    }

    // One complete oscillator step for callers that already run an
    // oversampled engine. The host-rate Process wrapper uses the same step.
    float ProcessEngineSample(float pitchKnob01, float morphKnob01, float pmKnob01,
                              float pmRateKnob01, float ringModKnob01, float sampleRate)
    {
        const float phaseIncrement = PitchToPhaseIncrement(pitchKnob01, sampleRate);
        // 08b5fd3:src/core/FroggersEngine.hpp:741-743 -- the depth multiply is at the caller of
        // StepIndependentPmLfo, not inside it; ported the same way.
        const float pmOffset = kPmLfoDepth * PmDepthScale(pmKnob01) * StepPmLfo(pmRateKnob01, sampleRate);
        const float modulatedPhase = WrapPhase(carrierPhase + pmOffset);
        const float dry = EvalWaveMorph(*waveforms, modulatedPhase, morphKnob01,
                                        phaseIncrement * sampleRate, 0.5f * sampleRate);

        // This SAME VCO's own internal ring-mod carrier -- stepped
        // every sample regardless of depth (same practice as pmLfoPhase
        // above, so raising the knob never causes a phase jump), multiplied
        // against `dry` (the pre-ASR-gate wave-morph output -- this struct
        // has no ASR gate of its own; that happens later in
        // dsp::MixOscVoices/VcoAdsrState::apply, so "pre-gate" is the only
        // choice available here and the one this port makes). Blended by
        // RingModDepthScale's taper amount so the bottom of the knob is an
        // exact identity and the top is a full ring-mod product; convex
        // blend is bounded before filtering. The causal lowpass can overshoot;
        // do not clip its output, which would create new harmonics.
        const float ringCarrier = Sine01(ringCarrierPhase);
        ringCarrierPhase = WrapPhase(ringCarrierPhase + RingModPhaseIncrement(ringModKnob01, sampleRate));
        const float ringAmount = RingModDepthScale(ringModKnob01);
        const float mixed = dry * (1.0f - ringAmount) + dry * ringCarrier * ringAmount;

        carrierPhase = WrapPhase(carrierPhase + phaseIncrement);
        return mixed;
    }

    // 08b5fd3:src/core/FroggersEngine.hpp:735-744, the m_simIndependentPm branch, plus Ring
    // Mod and PM-rate decoupling on top. pmKnob01 drives ONLY this VCO's
    // own PM depth (PmDepthScale); pmRateKnob01 is the shared Audio-slot-12
    // rate knob; ringModKnob01 is this VCO's own Ring Mod knob (Audio slot
    // 9/10/11).
    float Process(float pitchKnob01, float morphKnob01, float pmKnob01, float pmRateKnob01, float ringModKnob01,
                  float sampleRate)
    {
        const float internalSampleRate = sampleRate * kOversampleFactor;
        std::array<float, kOversampleFactor> internalOutput{};
        // Hold all five normalized controls through four complete oscillator
        // steps. Decimate the complete PM/ring result through two halfbands.
        for (int substep = 0; substep < kOversampleFactor; ++substep)
            internalOutput[static_cast<std::size_t>(substep)] = ProcessEngineSample(
                pitchKnob01, morphKnob01, pmKnob01, pmRateKnob01, ringModKnob01, internalSampleRate);
        const float middleFirst = antiAliasFilter[0].Process(internalOutput[0], internalOutput[1]);
        const float middleSecond = antiAliasFilter[0].Process(internalOutput[2], internalOutput[3]);
        const float output = antiAliasFilter[1].Process(middleFirst, middleSecond);

        // This struct does not write to the scope. The `output` returned
        // here is pre-gate: it is nonzero whatever the ASR gate is doing,
        // because the gate (dsp::MixOscVoices/VcoAdsrState::apply,
        // VoiceEnvelope.hpp) runs at the caller, only after all three
        // Vco::Process() calls have returned (FroggersAppCore.hpp's
        // RouteAudioSample()). Writing it to the scope from here would show
        // motion with the transport stopped, so the scope write lives at
        // that call site instead, after the gate, using the per-VCO
        // ScopeWriterHolder members FroggersAppCore owns directly.
        // `scopeWriterHolder_`/`SetScopeWriterHolder()` here serve only
        // `PopulateUIState()` below, which tells the UI which
        // ScopeWriter/channel to poll, not when to write a sample.
        return output;
    }

    // `SetScopeWriterHolder`/`SetScopeColor`/`PopulateUIState`,
    // same shape as WavetableVco's (External/Sheaf/projects/synth/include/synth/DspOscillators.hpp).
    void SetScopeWriterHolder(synth::ScopeWriterHolder* holder)
    {
        scopeWriterHolder_ = holder;
    }

    void SetScopeColor(synth::Color color)
    {
        scopeColor_ = color;
    }

    void PopulateUIState(UIState& state) const
    {
        state.scopeColor.Store(scopeColor_);
        const bool connected = scopeWriterHolder_ != nullptr && scopeWriterHolder_->Writer() != nullptr;
        state.connected.store(connected);
        state.scope.store(connected ? scopeWriterHolder_->Writer() : nullptr);
        state.scopeChannel.store(connected ? scopeWriterHolder_->FlatChan() : 0);
    }

    // Per-unit recovery (see app/FroggersAppCore.hpp): zeros this
    // VCO's three running phases and both FIR histories -- does NOT touch
    // scopeWriterHolder_/scopeColor_ (wiring, not signal state) or any knob
    // input, since those are recomputed fresh from the caller's arguments on
    // the very next Process() call regardless. Both phases are ordinarily
    // kept in [0,1) by WrapPhase, but `carrierPhase + pmOffset` (the
    // phase-offset addition inside Process() above) can latch non-finite if
    // pmOffset itself ever is (WrapPhase's `floor` of a NaN is NaN, and NaN
    // then propagates through every subsequent WrapPhase() forever) --
    // Reset() is this unit's only way back.
    void Reset()
    {
        carrierPhase = 0.0f;
        pmLfoPhase = 0.0f;
        ringCarrierPhase = 0.0f;
        overCeilingSeconds = 0.0f;
        for (HalfbandDecimator2x& section : antiAliasFilter)
        {
            section.Reset();
        }
    }

    // Read by Tier 1/Tier 2 recovery: phases and filter histories are
    // recursive state; immutable coefficients are excluded.
    bool StateFinite() const
    {
        if (!std::isfinite(carrierPhase) || !std::isfinite(pmLfoPhase) || !std::isfinite(ringCarrierPhase))
        {
            return false;
        }
        for (const HalfbandDecimator2x& section : antiAliasFilter)
        {
            for (float sample : section.history)
            {
                if (!std::isfinite(sample)) return false;
            }
        }
        return true;
    }
    float StateMagnitude() const
    {
        float magnitude = std::max({std::fabs(carrierPhase), std::fabs(pmLfoPhase), std::fabs(ringCarrierPhase)});
        for (const HalfbandDecimator2x& section : antiAliasFilter)
        {
            for (float sample : section.history)
            {
                magnitude = std::max(magnitude, std::fabs(sample));
            }
        }
        return magnitude;
    }

private:
    synth::Color scopeColor_ = synth::Color::Cyan;
    synth::ScopeWriterHolder* scopeWriterHolder_ = nullptr;
};

}  // namespace synth_froggers::dsp
