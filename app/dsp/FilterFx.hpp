#pragma once

// Design inspired by Vital (Matt Tytel) for the filter-module controls.
// DSP uses the shared Sheaf SVF and the app's existing comb and limiter.

#include "DspMath.hpp"
#include "Limiter.hpp"
#include "Vco.hpp"
#include "VoiceEnvelope.hpp"
#include "synth/DspFilters.hpp"
#include "RecoveryTier.hpp"  // dsp::FiniteOnly/dsp::Magnitude, this struct's own ForEachStatefulUnit below.

#include "synth/DspTransferFunction.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstddef>

namespace synth_froggers::dsp {

namespace transfer_function_detail {

// Ensures self-oscillating comb feedback produces only finite plot
// values, with the displayed magnitude bounded rather than drawn out of
// frame: a self-oscillating comb (feedback up to +-0.95, Comb::GetFeedback
// above -- was +-1.1 before this port) can place this linearised
// system's loop gain arbitrarily close to
// (or, in principle, exactly on) a true unit-circle pole, which would make
// a bare `1/denominator` divide-by-(near)zero and stop being finite. This
// floors the denominator's MAGNITUDE (preserving its phase) before any
// division below, so every closed-form response this file computes is
// finite and bounded by construction -- not by coincidence of where sample
// points happen to land.
//
// This loop gain (`feedback * z^-N * Hlp(z)` in
// ComputeLinearizedTransferFunctionValue below) carries no combDrive term,
// and needs none at any combDrive setting: combDrive's compensation
// (multiply Saturate's argument, divide its output by the same factor --
// combDrive's own comment, below) keeps the saturator stage's small-signal
// gain at exactly 1.0 regardless of combDrive (this file's own header
// comment establishes Saturate's own derivative at 0 is 1.0; dividing by k
// the output of an argument scaled by that same k leaves the derivative at
// 0 unchanged: `d/dx[Saturate(k*x)/k]` at `x=0` is `Saturate'(0) == 1` for
// every `k > 0`), so this closed form's implicit "saturator is unity gain"
// treatment is exact at every drive setting, not only at combDrive's unity
// default.
inline std::complex<float> SafeDenominator(std::complex<float> denominator)
{
    constexpr float kMinMagnitude = 1.0e-3f;  // bounds the worst-case response to 1/kMinMagnitude = 1000x.
    const float magnitude = std::abs(denominator);
    if (!std::isfinite(magnitude) || magnitude < kMinMagnitude)
    {
        const float phase = (std::isfinite(magnitude) && magnitude > 0.0f) ? std::arg(denominator) : 0.0f;
        return std::polar(kMinMagnitude, phase);
    }
    return denominator;
}

}  // namespace transfer_function_detail

// 08b5fd3:src/core/TanhSaturator.hpp:25-30. Despite the firmware struct's name, this
// is a Pade rational approximation, not std::tanh -- pinned exactly,
// including the clamp. Compressive: `Saturate(y) <= y` for `y >= 0` (the
// rational part is `y * ratio` with `ratio = (27+y^2)/(27+9y^2) <= 1` for
// every real y, since `27+y^2 <= 27+9y^2` iff `0 <= 8y^2`; the clamp can
// only move the result closer to 0 from there) -- relied on by
// GetFeedback's own comment (below) and by combDrive's (Filter slot 7,
// below).
struct PadeSaturator
{
    static float Saturate(float input)
    {
        const float input2 = input * input;
        const float output = input * (27.0f + input2) / (27.0f + 9.0f * input2);
        return std::max(-1.0f, std::min(1.0f, output));
    }
};

// Page limiter configuration is retained; it runs after all three stages.
inline constexpr float kFilterOutputLimiterThreshold = 0.7f;
inline constexpr float kFilterOutputLimiterCeiling = kStageCeiling;
inline constexpr float kFilterOutputLimiterAttackSeconds = 5.0e-6f;
inline constexpr float kFilterOutputLimiterReleaseSeconds = 0.1f;
static_assert(kFilterOutputLimiterThreshold < kFilterOutputLimiterCeiling);

struct Comb
{
    static constexpr size_t kSize = 8192;

    // The comb's *linearised* response (saturator
    // treated as identity, see this file's header comment) is a feedback
    // loop of "delay by delaySamples, then one-pole lowpass, then scale by
    // feedback" around unity input gain:
    //   Y(z) = X(z) + feedback * z^-N * Hlp(z) * Y(z)
    //   H(z) = Y(z)/X(z) = 1 / (1 - feedback * z^-N * Hlp(z))
    // -- captured state is exactly what that closed form needs: the
    // feedback coefficient, the lowpass's alpha (`OnePoleLowPass::alpha`, the
    // same state Sheaf's own `OnePoleLowPass::UIState` captures,
    // External/Sheaf/projects/synth/include/synth/DspFilters.hpp), and the integer delay length.
    struct UIState : synth::TransferFunction
    {
        std::atomic<float> feedback{0.0f};
        std::atomic<float> lowPassAlpha{0.0f};
        std::atomic<std::size_t> delaySamples{0};

        float FrequencyResponse(float normalizedFrequency) const override
        {
            return std::abs(TransferFunctionValue(normalizedFrequency));
        }

        std::complex<float> TransferFunctionValue(float normalizedFrequency) const override
        {
            return Comb::ComputeLinearizedTransferFunctionValue(
                feedback.load(), lowPassAlpha.load(), delaySamples.load(), normalizedFrequency);
        }
    };

    OnePoleLowPass filter;
    FixedWatchedBuffer<kSize> delayLine;
    size_t index = 0;
    // Fractional: Process() below reads between the two adjacent integer
    // taps by this value's fractional part (PureDelay's own frac/idx0/idx1
    // idiom, reused verbatim) -- an exact integer value still reads a
    // single sample bit-identically, since the interpolation weight is
    // zero exactly at an integer delay.
    float delaySamples = 0.0f;
    float feedback = 0.0f;
    PadeSaturator saturator;

    // (Filter slot 7, "Comb drive", "CDrv"): knob-driven saturation-depth
    // control. `combDrive` multiplies the SATURATOR'S ARGUMENT, and the
    // SAME factor divides its OUTPUT (see Process() below) -- COMPENSATED,
    // not a bare pre-gain on the argument alone: an uncompensated pre-gain
    // spends the knob's bottom half (combDrive < 1) merely weakening the
    // loop before any saturation character appears, and only reaches the
    // saturator's knee in its top half (combDrive > ~2) -- the "useless
    // until the very end" behaviour this compensation replaces.
    //
    // NOT a post-saturator multiply with no matching divide either
    // (`feedback * combDrive * Saturate(x)`): that widens the per-sample
    // bound to `|input| + combDrive*|feedback|`, unboundedly worse at high
    // drive, and never drives the saturator past its knee at all -- a pure
    // post-gain, no saturation-character change, the opposite of what a
    // drive control is for.
    //
    // Dividing the saturator's OUTPUT by the same `combDrive` that scaled
    // its ARGUMENT still leaves an absolute `|feedback|/combDrive` ceiling
    // at the low-drive end (combDrive < 1), and that ceiling DOES exceed
    // the old `|feedback|` bound: `PadeSaturator::Saturate` is compressive,
    // `Saturate(y) <= y` for `y >= 0` (Saturate's own comment, above; the
    // same fact GetFeedback's comment below relies on), so
    // `Saturate(combDrive*x)/combDrive <= x` for `x >= 0` -- but `x`
    // (`filter.Process(tapped)`, the delayed signal it is computed from) is
    // not itself bounded to +-1, only the pre-divide `Saturate` output is,
    // so this inequality does not keep the compensated fed-back term under
    // |feedback|. Measured directly on `dsp::Comb`, with a 100-sample delay,
    // the low-pass fully open, feedback 0.95, and a full-scale sine at the
    // comb's own pitch: at Comb drive 0.25 the fed-back term reaches 2.9222,
    // well past |feedback|'s own 0.95 -- a statement about the fed-back term's
    // own absolute LEVEL, not its decay. The loop's per-pass decay still
    // stays governed by `feedback` alone at every combDrive, and
    // GetFeedback's geometric ring-time law (below) still holds true
    // across this knob's whole travel: `Saturate(combDrive*x)/combDrive <=
    // x` holds for every `x >= 0` regardless of `combDrive`, so the
    // fed-back term's ratio to the signal it was computed from never
    // exceeds `feedback`, even where its absolute level does.
    // combDrive is always > 0 (`ExpMapCompute(0.25, 4.0, knob)`,
    // FroggersAppCore.hpp: a positive base raised to any real power stays
    // positive), so the divide is always defined.
    //
    // At combDrive == 1.0 -- this field's own default, the knob's centre,
    // and the transport-stopped override's target (RouteAudioSample's own
    // Filter slot 7 wiring) -- `Saturate(x)/1.0f` reproduces the
    // pre-compensation form bit-for-bit (IEEE divide-by-1.0 introduces no
    // rounding for any finite operand).
    float combDrive = 1.0f;

    // Tier 2's per-unit sustained-over-
    // ceiling counter, owned here rather than in FroggersAppCore -- see
    // dsp::Vco::overCeilingSeconds's own comment (app/dsp/Vco.hpp) for the
    // full rationale.
    float overCeilingSeconds = 0.0f;

    void SetFeedback(float fb) { feedback = fb; }
    void SetCutoffAlpha(float cutoff) { filter.alpha = cutoff; }
    void SetDrive(float drive) { combDrive = drive; }

    // src/core/Comb.hpp:54, verbatim: out = in + fb*sat(lp(delay[i-N])),
    // with `combDrive` inserted on Saturate's ARGUMENT and dividing its
    // OUTPUT -- the compensated form; see combDrive's own declaration
    // comment above for why both placements are load-bearing.
    float Process(float input)
    {
        // PureDelay's own frac/idx0/idx1 idiom (FilterFx.hpp's `PureDelay::Process`),
        // reused verbatim: the two adjacent taps, linearly interpolated by
        // delaySamples's fractional part. The weight is exactly zero at an
        // integer delaySamples (lowExact is then an exact integer, so
        // frac==0 and idx0 alone survives), so an integer delay reproduces
        // the old single-tap read bit-for-bit. The lerp is a convex
        // combination (both weights >= 0 and sum to 1), so the interpolated
        // tap never leaves the range the two neighbor samples bound --
        // every bound argument that relied on the old single-tap read
        // still holds.
        const float lowExact = static_cast<float>(index) + static_cast<float>(kSize) - delaySamples;
        const float frac = lowExact - std::floor(lowExact);
        const size_t idx0 = static_cast<size_t>(lowExact) % kSize;
        const size_t idx1 = (idx0 + 1) % kSize;
        const float tapped = delayLine[idx0] * (1.0f - frac) + delayLine[idx1] * frac;
        const float x = filter.Process(tapped);
        const float output = input + feedback * (PadeSaturator::Saturate(combDrive * x) / combDrive);
        delayLine.Write(index, output);
        index = (index + 1) % kSize;
        return output;
    }

    // The closed form derived in this struct's header comment.
    // `Hlp(z) = alpha / (1 - (1-alpha) z^-1)` matches
    // OnePoleLowPass::Process's own recurrence
    // (`output = alpha*input + (1-alpha)*output`, app/dsp/DspMath.hpp)
    // exactly. `SafeDenominator` (this file's top) keeps every division
    // finite and bounded even when feedback*Hlp's magnitude approaches or
    // exceeds 1 (a genuinely self-oscillating configuration).
    static std::complex<float> ComputeLinearizedTransferFunctionValue(
        float feedbackValue, float lowPassAlpha, size_t delaySamplesValue, float normalizedFrequency)
    {
        const float omega = 2.0f * static_cast<float>(M_PI) * normalizedFrequency;
        const std::complex<float> zInv(std::cos(omega), -std::sin(omega));
        const std::complex<float> lowPassResponse =
            std::complex<float>(lowPassAlpha, 0.0f) /
            transfer_function_detail::SafeDenominator(
                std::complex<float>(1.0f, 0.0f) - (1.0f - lowPassAlpha) * zInv);
        // z^-N = e^{-j*N*omega}; computed directly from N*omega (exact, no
        // accumulated per-sample multiplication error) since |zInv| == 1.
        const float omegaN = omega * static_cast<float>(delaySamplesValue);
        const std::complex<float> zInvN(std::cos(omegaN), -std::sin(omegaN));
        const std::complex<float> loopGain = feedbackValue * zInvN * lowPassResponse;
        return std::complex<float>(1.0f, 0.0f) /
               transfer_function_detail::SafeDenominator(std::complex<float>(1.0f, 0.0f) - loopGain);
    }

    void PopulateUIState(UIState& state) const
    {
        state.feedback.store(feedback);
        state.lowPassAlpha.store(filter.alpha);
        state.delaySamples.store(delaySamples);
    }

    // (Per-unit recovery, app/FroggersAppCore.hpp): zeros only the
    // comb's own recursive state -- the delay line, its write index, and
    // the one-pole lowpass's output -- NOT feedback/delaySamples (config,
    // reassigned every block from the Filter page's knobs by
    // RouteAudioSample) and NOT filter.alpha (same: reassigned by
    // SetCutoffAlpha every block). Mirrors dsp::Reverb::Reset()'s own
    // "clears state, does not reconfigure" convention (Reverb.hpp).
    void Reset()
    {
        filter.output = 0.0f;
        delayLine.Clear();
        index = 0;
        overCeilingSeconds = 0.0f;
    }

    // (Tier 1/Tier 2 recovery). The whole recirculating line is state a
    // poisoned sample could sit in; its own counts answer for it in constant
    // time (dsp::WatchedBuffer, dsp/RecoveryTier.hpp), where a walk of all
    // 8192 samples once per audio callback cost a phone most of its budget.
    bool StateFinite() const { return std::isfinite(filter.output) && delayLine.AllFinite(); }

    // Tier 2's question for this unit (dsp::StateOverCeiling): exactly
    // StateMagnitude() > dsp::kMaxUnitStateMagnitude, from the line's counts.
    bool StateOverCeiling() const
    {
        return std::fabs(filter.output) > kMaxUnitStateMagnitude || delayLine.AnyOverCeiling();
    }

    float StateMagnitude() const
    {
        float magnitude = std::fabs(filter.output);
        for (size_t i = 0; i < kSize; ++i)
        {
            magnitude = std::max(magnitude, std::fabs(delayLine[i]));
        }
        return magnitude;
    }

    // src/core/Comb.hpp:61-64 (GetDelaySamples): N = 1/freq.
    static float GetDelaySamples(float freq) { return 1.0f / freq; }

    // src/core/Comb.hpp:66-76 (GetFeedback): asymmetric feedback, magnitude
    // scaled by a 0..1 curve built from ExpMapCompute.
    //
    // DELIBERATE PARITY DIVERGENCE (same treatment as Fuegoize.hpp's own
    // divergence note): the firmware scales by +-1.1. This port
    // scales by +-0.95 instead. The predecessor's analysis claimed |fb| > 1
    // makes the comb diverge
    // exponentially -- that is FALSE. `Process()` above is
    // `out = in + fb*Saturate(lp(delayed))`, and `PadeSaturator::Saturate`
    // sits INSIDE the feedback path, so at Comb drive 1 (no compensation)
    // the fed-back term can never exceed |fb|*1.0 no matter how large |fb|
    // is -- there is no exponential divergence possible here. Below Comb
    // drive 1 the compensated form (combDrive's own comment, above) divides
    // the saturator's clamped output by combDrive, widening the ceiling to
    // |fb|/combDrive (measured directly on `dsp::Comb`, feedback 0.95, a
    // 100-sample delay and a full-scale sine at the comb's own pitch: Comb
    // drive 0.25 reaches a fed-back term of 2.9222) -- still finite, so
    // still no exponential
    // divergence, but no longer bounded to |fb|*1.0.
    // What |fb| > 1 actually does is hold the
    // loop in permanent, undecaying self-oscillation pinned at the
    // saturator's limit: `Reset()`-free silence never arrives, because each
    // pass regenerates (>=1.0)*saturator-bounded-output faster than it can
    // decay. |fb| < 1 is the mathematical definition of a loop that DOES
    // decay once its input stops (each pass multiplies by a strictly
    // sub-unity factor -- see `PadeSaturator::Saturate`'s own comment: it is
    // compressive, `Saturate(y) <= y` for `y >= 0`, so `|fb|*Saturate(y) <=
    // |fb|*y` and the sequence is geometric with ratio `<= |fb| < 1`).
    // 0.95 keeps that decay slow enough to still ring for a long, musical
    // time while guaranteeing it eventually reaches silence. Parity was
    // explicitly deprioritized here -- this is the intended outcome, not a
    // defect -- do not restore 1.1. The feedback gap -- one minus the
    // magnitude -- falls geometrically across each half, so equal knob
    // steps multiply ring time by equal ratios, and the center and both
    // rails map to exactly the values the previous curve produced.
    static float GetFeedback(float knob)
    {
        constexpr float kMaxFeedbackMagnitude = 0.95f;
        if (knob < 0.5f)
        {
            return -(1.0f - ExpMapCompute(1.0f, 1.0f - kMaxFeedbackMagnitude, 2.0f * (0.5f - knob)));
        }
        return 1.0f - ExpMapCompute(1.0f, 1.0f - kMaxFeedbackMagnitude, 2.0f * (knob - 0.5f));
    }
};

// src/core/Comb.hpp:79-109. Fractional interpolation at :98-108.
struct PureDelay
{
    static constexpr size_t kSize = 32768;

    float delayLine[kSize]{};
    size_t index = 0;
    float delaySamples = 0.0f;

    void SetDelaySeconds(float seconds, float sampleRate) { delaySamples = seconds * sampleRate; }

    float Process(float input)
    {
        delayLine[index] = input;
        const float lowExact = static_cast<float>(index) + static_cast<float>(kSize) - delaySamples;
        const float frac = lowExact - std::floor(lowExact);
        const size_t idx0 = static_cast<size_t>(lowExact) % kSize;
        const size_t idx1 = (idx0 + 1) % kSize;
        const float output = delayLine[idx0] * (1.0f - frac) + delayLine[idx1] * frac;
        index = (index + 1) % kSize;
        return output;
    }
};

// 08b5fd3:src/core/FroggersEngine.hpp:822-848 (ApplyOutputFx), the parallel (:824-833) and
// serial (:834-839) comb/peak/scoop routing only -- the reverb blend and
// m_simFxInsert hook that follow in the firmware function are out of scope.
// Both filters use the shared Sheaf integrator. Only the app recovery timer
// is added here; filter state and its reset contract remain owned by Sheaf.
struct RecoverableSvf : synth::ClassicStateVariableFilter
{
    float overCeilingSeconds = 0.0f;
    void Reset() { synth::ClassicStateVariableFilter::Reset(); overCeilingSeconds = 0.0f; }
};

struct FilterKeyTracking
{
    float semitoneOffset = 0.0f;

    void Update(const std::array<float, 3>& pitchKnobs, float balanceX, float balanceY, float sampleRate)
    {
        float weights[3];
        ComputeVcoBalanceWeights(balanceX, balanceY, weights[0], weights[1], weights[2]);
        float key = 0.0f;
        for (std::size_t i = 0; i < pitchKnobs.size(); ++i)
        {
            const float hz = Vco::PitchToPhaseIncrement(pitchKnobs[i], sampleRate) * sampleRate;
            key += weights[i] * (69.0f + 12.0f * std::log2(hz / 440.0f));
        }
        semitoneOffset = key - 60.0f;
    }

    float Track(float referenceHz, float amount = 1.0f) const
    {
        return referenceHz * std::exp2(amount * semitoneOffset / 12.0f);
    }
};

inline float LinearFilterMix(float dry, float wet, float mix)
{
    if (mix == 0.0f) return dry;
    if (mix == 1.0f) return wet;
    return dry + mix * (wet - dry);
}

struct PeakNotchWeights { float pass; float band; };
inline PeakNotchWeights ComputePeakNotchWeights(float morph)
{
    const float pass = std::sqrt(0.5f * morph * morph + 0.5f);
    return {pass, 2.0f * pass * (1.0f - morph)};
}

inline float CombinePeakNotch(const synth::ClassicStateVariableFilter& filter, float morph)
{
    const auto weights = ComputePeakNotchWeights(morph);
    return weights.pass * (filter.LowPass() + filter.HighPass()) + weights.band * filter.BandPass();
}

struct PeakNotchUiState : synth::TransferFunction
{
    std::atomic<float> cutoff{0.0f};
    std::atomic<float> resonance{0.5f};
    std::atomic<float> morph{1.0f};

    std::complex<float> TransferFunctionValue(float frequency) const override
    {
        const float c = cutoff.load(), q = resonance.load();
        const auto weights = ComputePeakNotchWeights(morph.load());
        using Svf = synth::ClassicStateVariableFilter;
        return weights.pass * (Svf::TransferFunctionValue(c, q, -1.0f, frequency)
                              + Svf::TransferFunctionValue(c, q, 1.0f, frequency))
            + weights.band * Svf::TransferFunctionValue(c, q, 0.0f, frequency);
    }

    float FrequencyResponse(float frequency) const override
    {
        return std::abs(TransferFunctionValue(frequency));
    }
};

struct FilterFxChain
{
    RecoverableSvf mainFilter;
    RecoverableSvf peakFilter;
    Comb comb;
    PureDelay pureDelay;
    FilterKeyTracking keyTracking;
    synth::ClassicStateVariableFilter::Input mainInput, peakInput;
    float mainMix = 1.0f;
    float peakMix = 1.0f;
    float peakMorph = 1.0f;
    OutputLimiter outputLimiter;

    // Audio-thread taps for production-path measurements and headless tests.
    struct StageOutputs { float mainWet = 0, main = 0, comb = 0, peakWet = 0, peak = 0; } outputs;

#ifdef FROGGERS_DSP_CHECKS
    // Test builds measure the production path without injecting an audio
    // source or changing its controls. Shipping builds carry no counters.
    struct Metrics {
        std::size_t samples = 0, nonfiniteSamples = 0;
        double inputEnergy = 0, outputEnergy = 0, limiterReductionDb = 0;
        float mainState = 0, combState = 0, peakState = 0;
    } metrics;
#endif

    FilterFxChain() { Configure(48000.0f); }

    void Configure(float sampleRate)
    {
        outputLimiter.Configure(sampleRate, kFilterOutputLimiterThreshold, kFilterOutputLimiterCeiling,
            kFilterOutputLimiterAttackSeconds, kFilterOutputLimiterReleaseSeconds);
    }

    template <typename Visitor>
    void ForEachStatefulUnit(Visitor&& visit)
    {
        visit(mainFilter, Magnitude{});
        visit(comb, Magnitude{});
        visit(peakFilter, Magnitude{});
        visit(outputLimiter, FiniteOnly{});
    }

    float Process(float input)
    {
        mainInput.value = input;
        outputs.mainWet = mainFilter.Process(mainInput);
        outputs.main = LinearFilterMix(input, outputs.mainWet, mainMix);
        outputs.comb = comb.Process(pureDelay.Process(outputs.main));
        peakInput.value = outputs.comb;
        peakFilter.Process(peakInput);
        outputs.peakWet = CombinePeakNotch(peakFilter, peakMorph);
        outputs.peak = LinearFilterMix(outputs.comb, outputs.peakWet, peakMix);
        const float output = outputLimiter.Process(outputs.peak);
#ifdef FROGGERS_DSP_CHECKS
        ++metrics.samples;
        metrics.nonfiniteSamples += !std::isfinite(output);
        metrics.inputEnergy += static_cast<double>(input) * input;
        metrics.outputEnergy += static_cast<double>(output) * output;
        metrics.limiterReductionDb += -20.0 * std::log10(static_cast<double>(outputLimiter.envelope));
        metrics.mainState = std::max(metrics.mainState, mainFilter.StateMagnitude());
        metrics.combState = std::max(metrics.combState, std::max(std::fabs(outputs.comb), std::fabs(comb.filter.output)));
        metrics.peakState = std::max(metrics.peakState, peakFilter.StateMagnitude());
#endif
        return output;
    }

    void PopulatePeakUiState(PeakNotchUiState& state) const
    {
        state.cutoff.store(peakFilter.m_cutoff);
        state.resonance.store(peakFilter.m_resonance);
        state.morph.store(peakMorph);
    }
};

}  // namespace synth_froggers::dsp
