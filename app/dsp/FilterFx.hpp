#pragma once

// synth_froggers::dsp::{PadeSaturator, ResonantBump, Comb, PureDelay,
// FilterFxChain} -- a **copy** of the cited Froggers formulas.
//
// Ported from:
//   - 08b5fd3:src/core/ResonantBump.hpp:44-71   RBJ peaking-biquad coefficients
//   - src/core/Comb.hpp:54              out = in + fb*sat(lp(delay[i-N]))
//   - src/core/Comb.hpp:63              N = 1/freq (GetDelaySamples)
//   - src/core/Comb.hpp:66-76           asymmetric +-1.1 feedback (GetFeedback --
//     THIS PORT DELIBERATELY DIVERGES to +-0.95; see GetFeedback's own comment
//     below)
//   - src/core/Comb.hpp:79-109          PureDelay (fractional interp :98-108;
//     the firmware file has no trailing newline after its closing `};`, so
//     `wc -l` reports 108 while the struct's real content runs through
//     line 109 -- confirmed by reading the file's raw bytes, not a citation
//     error)
//   - 08b5fd3:src/core/TanhSaturator.hpp:25-30  Pade rational approximation
//     x(27+x^2)/(27+9x^2), NOT std::tanh (formula :28, clamp :29)
//   - 08b5fd3:src/core/FroggersEngine.hpp:822-848 (ApplyOutputFx) -- specifically
//     the parallel branch :824-833 and serial branch :834-839. The reverb
//     stage and m_simFxInsert hook that follow (:840-847) are not part of
//     this port.
//
// ============================================================================
// ResonantBump and Comb each gain a
// `struct UIState : synth::TransferFunction` (the interface TYPE; the
// header is named DspTransferFunction.hpp,
// include/synth/DspTransferFunction.hpp:7,9,10), mirroring the exact
// in-struct placement Sheaf's own filters use (`OnePoleLowPass::UIState`,
// include/synth/DspFilters.hpp:22; `OnePoleHighPass::UIState`, :85;
// `ClassicStateVariableFilter::UIState`, :143) rather than a separate
// wrapper. As with app/dsp/Vco.hpp's own scope UIState, this is a
// deliberate, minimal widening of this file's "no Sheaf dependency"
// convention: `synth/DspTransferFunction.hpp` is a two-method abstract
// interface with no further transitive Sheaf includes beyond `<complex>`,
// header-only, no link dependency. `app/Makefile`'s DSP_TEST_BIN rule
// gains Sheaf's -I include path accordingly.
//
// The comb's saturator is the Pade rational approximation (PadeSaturator
// above), not tanh -- its small-signal gain (the derivative at x=0) is
// exactly 1.0 (d/dx [x(27+x^2)/(27+9x^2)] at x=0 = 27/27 = 1), so the
// linearised comb response below treats the saturator as an identity and
// reduces Comb::Process's feedback loop to an ordinary linear
// delay-plus-lowpass-in-feedback comb filter.

#include "DspMath.hpp"
#include "Limiter.hpp"
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

// 08b5fd3:src/core/ResonantBump.hpp:7-78. RBJ peaking EQ; coefficients at :44-71.
// The app's maximum peak-resonance gain, as a SHARED constant rather than a
// literal at each use. It lives here, beside the unit it bounds,
// because two places need to agree on it: FroggersAppCore's
// `peak.SetHeight(ExpMapCompute(1.0f, kMaxResonantBumpHeight, knob))` and the
// parity test that pins the resulting gain.
//
// It is a shared constant because the test that was supposed to pin this
// ceiling did not: it computed `ExpMapCompute(1.0f, 4.0f, 1.0f)` with its own
// hardcoded 4.0f and asserted the answer was 4.0f -- self-referential, and it
// passed unchanged when the app's real ceiling moved 4.0 -> 2.0. Anything
// asserting this value must read it from here, never retype it.
//
// Value history: 10.0 (firmware parity) -> 4.0 (gross overload) ->
// 2.0 (still too harsh when modulated, very close to blowout territory) ->
// 3.0 (2026-09-01: the pinned-comb launch state behind the 2.0 cap no
// longer exists, and the candidate measurement showed the limiter holds
// the worst case within 0.03 dB of the 2.0 ceiling's; ear-gated on the
// deployed site).
// See FroggersAppCore's own comment at the SetHeight
// call for why modulation is the case that governs this number.
inline constexpr float kMaxResonantBumpHeight = 3.0f;

// Tuning for
// `FilterFxChain::peakLimiter` below, a SECOND, independently-configured
// `dsp::OutputLimiter` instance (dsp/Limiter.hpp) inserted after the peak
// branch's own `1/height` scalar trim. That trim alone was measured to not
// close the gap under per-sample-random height modulation --
// the peak is a stateful 2-pole biquad whose stored energy survives a
// height DROP, and no per-sample SCALAR can retroactively scale away energy
// already in a filter's state; something with its own release can. These
// four constants are the ones that something needs, chosen BY MEASUREMENT
// (a scratch harness reproducing this file's own ResonantBump + trim-
// smoother math plus a candidate `dsp::OutputLimiter`, run over the exact
// adversarial pattern that measured a worst-case 1.669: per-sample-random
// `height` in [1, kMaxResonantBumpHeight], full-scale sine at the bump's
// resonant frequency, 500,000 trials across 10 fixed xorshift32 seeds,
// 50,000 samples/seed -- matching FroggersDspParityTests.cpp's own Pattern
// 2 idiom exactly, not a fresh methodology), never by analogy to the master
// limiter's own tuning (the comb-trim smoother's own constant was picked
// by analogy once and measured 80% wrong later).
//
//   - kPeakLimiterThreshold (0.7): BELOW the master output limiter's 0.9
//     (this stage must catch the peak's residual
//     BEFORE the master ever sees it) with comfortable margin, not just
//     barely under it; the sweep found overshoot fell smoothly as
//     threshold dropped from 0.9 toward 0.5, and 0.7 sits in the flat part
//     of that curve (0.65 -> 0.982783, 0.70 -> 0.990022, 0.75 -> 0.995622
//     worst-case, all at the attack below) -- low enough to leave real
//     headroom under the computed bound A, not so low it engages on every
//     ordinary moderate-height passage for no measured benefit.
//   - kPeakLimiterAttackSeconds (5 microseconds): the delicate number, and
//     NOT chosen by analogy to the master's 1ms. Per-sample-random height
//     modulation makes the worst-case transient a SINGLE-SAMPLE event (the
//     biquad's residual right after a height drop, still carrying energy
//     from the prior, higher height) -- a 1ms attack (a ~48-sample time
//     constant at 48kHz) cannot react within one sample, and measured
//     worst-case overshoot barely moved across the ENTIRE 1ms-500ms release
//     sweep at a 1ms attack (stuck at ~1.44-1.53, nowhere near bound A).
//     Sweeping attack alone (release fixed at 100ms, threshold fixed at
//     0.7) found the worst case crosses under 1.0 between 0.01ms
//     (1.028044) and 0.006ms (0.991957); 5 microseconds (0.990022 measured,
//     10 seeds x 50,000 samples) sits just past that crossing with a
//     working margin, and going faster still (2 microseconds: 0.988134)
//     buys almost nothing further -- the curve is flattening, not still
//     falling, so 5us is the measured knee, not an arbitrary small number.
//   - kPeakLimiterReleaseSeconds (100ms, matches the master): release
//     barely moves the worst-case-overshoot measurement at all once attack
//     is fast enough (0.005ms/thr=0.7: 0.991766 at 10ms release vs 0.989732
//     at 500ms release -- a 0.002 spread across 50x the release time), so
//     it cannot be chosen FROM that metric; it has to be chosen from the
//     risk that actually matters instead -- reintroducing pumping on a
//     slow-decaying residual. Measured that residual directly: a step
//     excitation at max Q (10) and max height (2.0) at this file's own
//     `freq = 0.05` (matches the existing peak-branch bound tests) decays
//     to -60dB in 566 samples (11.79ms at 48kHz) once the input stops.
//     100ms is ~8.5x that decay time -- comfortably slower than the
//     residual it is meant to ride OVER rather than chase sample-by-sample
//     (a release near or under 11.79ms would let the gain snap back up
//     mid-ring, audible as pumping on exactly the signal this stage exists
//     to tame), and it reuses a value this codebase has already accepted for
//     this exact "gain reduction that does not pump" job (the master's own
//     `kDefaultReleaseSeconds`, dsp/Limiter.hpp) rather than inventing a
//     second number where the measurement gave no reason to.
inline constexpr float kPeakLimiterThreshold = 0.7f;  // unchanged -- MEASURED (above), already strictly below kStageCeiling.
inline constexpr float kPeakLimiterCeiling = kStageCeiling;  // retargeted from kSharedCeiling.
// See dsp::OutputLimiter::kDefaultThreshold's own static_assert
// (dsp/Limiter.hpp) for why a negative headroom is catastrophic AND silent.
static_assert(kPeakLimiterThreshold < kPeakLimiterCeiling,
              "threshold must stay strictly below ceiling; a negative headroom turns "
              "DesiredMagnitude into an exponential amplifier -- see dsp/Limiter.hpp");
inline constexpr float kPeakLimiterAttackSeconds = 5.0e-6f;   // 5 microseconds -- see comment above.
inline constexpr float kPeakLimiterReleaseSeconds = kSharedReleaseSeconds;  // shared; see Limiter.hpp.

struct ResonantBump
{
    // b0/b1/b2/a1/a2 (a0 already normalized to 1 by
    // UpdateCoefficients() below) are exactly what BiquadDf1::Process's
    // difference equation needs to reproduce the closed-form response --
    // capturing them directly (rather than freq/height/width) means
    // FrequencyResponse() reflects precisely the coefficients actually
    // driving Process(), with no risk of re-deriving them differently.
    struct UIState : synth::TransferFunction
    {
        std::atomic<float> b0{1.0f};
        std::atomic<float> b1{0.0f};
        std::atomic<float> b2{0.0f};
        std::atomic<float> a1{0.0f};
        std::atomic<float> a2{0.0f};

        float FrequencyResponse(float normalizedFrequency) const override
        {
            return std::abs(TransferFunctionValue(normalizedFrequency));
        }

        std::complex<float> TransferFunctionValue(float normalizedFrequency) const override
        {
            return ResonantBump::ComputeTransferFunctionValue(
                b0.load(), b1.load(), b2.load(), a1.load(), a2.load(), normalizedFrequency);
        }
    };

    BiquadDf1 biquad;

    float freq = 1000.0f;
    float height = 1.0f;
    float width = 1.0f;

    // Tier 2's per-unit
    // sustained-over-ceiling counter, owned here rather than in
    // FroggersAppCore -- see dsp::Vco::overCeilingSeconds's own comment
    // (app/dsp/Vco.hpp) for the full rationale. This struct has two
    // independent instances (FilterFxChain's peak/scoopNotch), each with
    // its own counter, same as any other member.
    float overCeilingSeconds = 0.0f;

    ResonantBump() { UpdateCoefficients(); }

    void SetFreq(float f)
    {
        freq = f;
        UpdateCoefficients();
    }

    void SetHeight(float h)
    {
        height = h;
        UpdateCoefficients();
    }

    void SetWidth(float w)
    {
        width = w;
        UpdateCoefficients();
    }

    // 08b5fd3:src/core/ResonantBump.hpp:44-71, verbatim formula.
    void UpdateCoefficients()
    {
        const float omega = 2.0f * static_cast<float>(M_PI) * freq;
        const float cosw = std::cos(omega);
        const float sinw = std::sin(omega);

        const float a = std::sqrt(height);
        const float q = width;
        const float alpha = sinw / (2.0f * q);

        const float a0 = 1.0f + alpha / a;
        const float a1 = -2.0f * cosw;
        const float a2 = 1.0f - alpha / a;
        const float b0 = 1.0f + alpha * a;
        const float b1 = -2.0f * cosw;
        const float b2 = 1.0f - alpha * a;

        biquad.b0 = b0 / a0;
        biquad.b1 = b1 / a0;
        biquad.b2 = b2 / a0;
        biquad.a1 = a1 / a0;
        biquad.a2 = a2 / a0;
    }

    float Process(float input) { return biquad.Process(input); }

    // BiquadDf1's own difference equation --
    // y[n] = b0 x[n] + b1 x[n-1] + b2 x[n-2] - a1 y[n-1] - a2 y[n-2]
    // (DspMath.hpp's BiquadDf1::Process) -- gives the standard direct-form-1
    // closed form H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2),
    // evaluated at z = e^{j*2*pi*normalizedFrequency} (normalizedFrequency in
    // cycles/sample, matching this struct's own `freq` convention).
    static std::complex<float> ComputeTransferFunctionValue(
        float b0Value, float b1Value, float b2Value, float a1Value, float a2Value, float normalizedFrequency)
    {
        const float omega = 2.0f * static_cast<float>(M_PI) * normalizedFrequency;
        const std::complex<float> zInv(std::cos(omega), -std::sin(omega));
        const std::complex<float> zInv2 = zInv * zInv;
        const std::complex<float> numerator = b0Value + b1Value * zInv + b2Value * zInv2;
        const std::complex<float> denominator = std::complex<float>(1.0f, 0.0f) + a1Value * zInv + a2Value * zInv2;
        return numerator / transfer_function_detail::SafeDenominator(denominator);
    }

    void PopulateUIState(UIState& state) const
    {
        state.b0.store(biquad.b0);
        state.b1.store(biquad.b1);
        state.b2.store(biquad.b2);
        state.a1.store(biquad.a1);
        state.a2.store(biquad.a2);
    }

    // (Per-unit recovery, app/FroggersAppCore.hpp): zeros only
    // biquad's recursive x1/x2/y1/y2 history -- NOT b0/b1/b2/a1/a2 (those
    // are coefficients, recomputed from freq/height/width by
    // UpdateCoefficients() on the very next SetFreq/SetHeight/SetWidth call,
    // not signal state) and NOT freq/height/width themselves (tuning, set by
    // the caller every block regardless). This is exactly the recovery
    // scoopNotch's own self-oscillation bug (see FroggersAppCore.hpp's
    // RouteAudioSample comment) needed: the biquad's y1/y2 can diverge to
    // non-finite from a bad coefficient set and then stay non-finite forever
    // afterward even once freq/height/width return to sane values, because
    // the difference equation (DspMath.hpp's BiquadDf1::Process) keeps
    // feeding y1/y2 back into every future output.
    void Reset()
    {
        biquad.x1 = 0.0f;
        biquad.x2 = 0.0f;
        biquad.y1 = 0.0f;
        biquad.y2 = 0.0f;
        overCeilingSeconds = 0.0f;
    }

    // (Tier 1/Tier 2 recovery): checks recursive state only
    // (x1/x2/y1/y2), not the coefficients -- a coefficient set computed from
    // a momentarily extreme freq/height/width is not itself "poisoned",
    // only a diverged y1/y2 that keeps propagating regardless of the
    // coefficients is.
    bool StateFinite() const
    {
        return std::isfinite(biquad.x1) && std::isfinite(biquad.x2) && std::isfinite(biquad.y1) &&
               std::isfinite(biquad.y2);
    }

    float StateMagnitude() const
    {
        return std::max({std::fabs(biquad.x1), std::fabs(biquad.x2), std::fabs(biquad.y1), std::fabs(biquad.y2)});
    }
};

// src/core/Comb.hpp:9-77.
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
    // feedback coefficient, the lowpass's alpha (OnePoleLowPass::alpha, the
    // same state Sheaf's own OnePoleLowPass::UIState captures,
    // DspFilters.hpp:22-23), and the integer delay length.
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
    float delayLine[kSize]{};
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
    // its ARGUMENT does not reopen that same concern from the low-drive
    // end (combDrive < 1, where an absolute `|feedback|/combDrive` ceiling
    // alone would exceed the old `|feedback|` bound):
    // `PadeSaturator::Saturate` is compressive, `Saturate(y) <= y` for
    // `y >= 0` (Saturate's own comment, above; the same fact GetFeedback's
    // comment below relies on), so `Saturate(combDrive*x)/combDrive <= x`
    // for `x >= 0` (divide the compressive inequality through by
    // combDrive > 0; the odd-symmetric case mirrors it for x < 0) -- the
    // compensated fed-back term never exceeds `x`
    // (`filter.Process(tapped)`, the delayed signal it is computed from),
    // so the loop's per-pass decay stays governed by
    // `feedback` alone, exactly as before, at EVERY combDrive setting: this
    // is what keeps GetFeedback's geometric ring-time law (below) true
    // across this knob's whole travel, not just at the unity default.
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
        // PureDelay's own frac/idx0/idx1 idiom (FilterFx.hpp:598-608),
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
        delayLine[index] = output;
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
    // reassigned every block from the Filter bank's knobs by
    // RouteAudioSample) and NOT filter.alpha (same: reassigned by
    // SetCutoffAlpha every block). Mirrors dsp::Reverb::Reset()'s own
    // "clears state, does not reconfigure" convention (Reverb.hpp).
    void Reset()
    {
        filter.output = 0.0f;
        std::fill(delayLine, delayLine + kSize, 0.0f);
        index = 0;
        overCeilingSeconds = 0.0f;
    }

    // (Tier 1/Tier 2 recovery). O(kSize) per call by
    // necessity (the entire recirculating delay line is state a poisoned
    // sample could be sitting in) -- called once per block (not per
    // sample), so this is 8192 float compares per block per Comb instance,
    // negligible next to a typical audio block's own per-sample DSP cost.
    bool StateFinite() const
    {
        if (!std::isfinite(filter.output))
        {
            return false;
        }
        for (size_t i = 0; i < kSize; ++i)
        {
            if (!std::isfinite(delayLine[i]))
            {
                return false;
            }
        }
        return true;
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
    // sits INSIDE the feedback path, so the fed-back term can never exceed
    // |fb|*1.0 no matter how large |fb| is -- there is no exponential
    // divergence possible here. What |fb| > 1 actually does is hold the
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
    static constexpr size_t kSize = 8192;

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
struct FilterFxChain
{
    ResonantBump peak;
    ResonantBump scoopNotch;
    Comb comb;
    PureDelay pureDelay;

    // Smooths the comb branch's exact output trim computed in
    // Process() below. `fb` (Comb::feedback) is set from
    // `Comb::SetFeedback`, called from `RouteAudioSample()` once per SAMPLE.
    // Smoothing the TRIM value here (never `fb` itself, which would change
    // the filter's behaviour, not just its level) erases the step every
    // `SetFeedback` call would otherwise put directly into the output level,
    // including the abrupt jumps a Crispy/randomize scramble produces.
    OnePoleLowPass combTrimSmoother;

    // Mirrors combTrimSmoother exactly, but smooths the
    // PEAK branch's exact output trim `1/height` computed in Process()
    // below, so `A * height / height == A` (the peak's own bound is
    // `|peak| <= A * height`). `height` (ResonantBump::height,
    // set via `SetHeight`) is refreshed from `RouteAudioSample()` once per
    // SAMPLE the same way `fb` is (audio-rate modulation reaches this
    // parameter's maximum continuously, not just as an edge case) -- so
    // this needs the exact
    // same glide as the comb trim for the exact same reason, not a separate
    // constant chosen by fresh analogy. Smooths the TRIM, never `height`
    // itself (that would change the filter, not just its level).
    OnePoleLowPass peakTrimSmoother;

    // The peak branch's OWN limiter, inserted AFTER peakTrimSmoother's
    // scalar trim above, not instead of it. The scalar
    // trim alone was measured to not bound the peak branch under per-sample-random
    // height modulation (worst-case 1.669 trimmed vs 1.819 untrimmed,
    // against an ideal of 1.0, 500k trials/10 seeds) because the peak is a
    // stateful 2-pole biquad -- its stored energy survives a height DROP,
    // and a same-instant scalar cannot retroactively remove energy already
    // in the filter's state. A limiter can, because it has its own release
    // and therefore its own memory. Tuned independently from the master
    // output limiter via the four `kPeakLimiter*` constants above this
    // struct (by measurement, not by analogy -- see their own comment).
    // NOT applied to `filterOut`/the composite: the comb branch is already
    // provably bounded by its own comb-trim smoothing above
    // (the saturator sits INSIDE that loop), so limiting the composite
    // would compress a signal that does not need it and colour the comb's
    // sound for no measured benefit -- the measurement traced the offender
    // to the peak specifically, so only the peak gets treated.
    OutputLimiter peakLimiter;

    // This struct's own contribution to the "every stateful unit in
    // the audio path" enumeration -- lists ONLY the members declared above
    // (not pureDelay/combTrimSmoother/peakTrimSmoother, which
    // RecoverPoisonedUnitState never watched either -- see
    // app/FroggersAppCore.hpp's own RecoverPoisonedUnitState comment for
    // which units that was and why). Composed, not re-listed, by
    // FroggersAppCore::ForEachStatefulUnit below.
    template <typename Visitor>
    void ForEachStatefulUnit(Visitor&& visit)
    {
        visit(comb, Magnitude{});
        visit(peak, Magnitude{});
        visit(scoopNotch, Magnitude{});
        visit(peakLimiter, FiniteOnly{});
    }

    FilterFxChain()
    {
        // Sample-rate-independent knob-glide idiom this codebase already
        // uses when a DSP unit has no sample-rate handle to convert a real
        // Hz cutoff into alpha (RandomShLane.hpp's SetAlphaFromNatFreq
        // call and its slew constants) -- FilterFxChain
        // has no SetSampleRate() of its own, so a fixed cycles/sample cutoff
        // is the only option without threading a new parameter down from
        // FroggersAppCore.
        //
        // This was 0.01 cycles/sample --
        // chosen by analogy to RandomShLane's own glide constants, under the
        // (wrong, see combTrimSmoother's own comment above) assumption that
        // `fb` only steps once per BLOCK. It actually steps once per SAMPLE,
        // so under fast audio-rate modulation of this parameter (a 100%-
        // depth Noise source being the worst realistic case) the trim can
        // lag far enough behind a fast upward swing that the comb branch
        // goes measurably UNDER-trimmed during the lag -- pinned by
        // FroggersDspParityTests.cpp's TEST_CASE
        // "comb_branch_output_stays_at_or_below_computed_bound_under_audio_
        // rate_feedback_modulation" (name split across lines here only for
        // width; it is one identifier), which failed outright at 0.01 (measured
        // worst-case overshoot over the intended bound: ~0.80 for a hard
        // step from rest to max feedback, ~0.53 for a continuous full-range
        // audio-rate sweep, both against the single-sample-delay comb
        // configuration that test and the static-feedback bound test above
        // it both use to isolate loop gain). A scratch sweep of the constant
        // found overshoot reaches exactly 0.0 (to measurement precision,
        // tens of thousands of trials across multiple seeds) at glide >=
        // ~0.33 cycles/sample; 0.45 below sits comfortably above that
        // crossover while staying under `OnePoleLowPass::kMaxCutoff` (0.499,
        // the hard clamp) -- fast enough that this is barely still
        // "smoothing" in the RandomShLane-glide sense (its own kFastCutoff
        // is 0.45, "near-instant"), which is the honest consequence of the
        // premise correction above, not a separate design choice: once `fb`
        // is known to step every sample rather than every block, matching
        // its own update rate is what the safety bound requires.
        // Shared by peakTrimSmoother below rather than a second literal
        // -- both trims are refreshed at the same per-sample cadence
        // (`fb`/`height` are both set from `RouteAudioSample()` once per
        // sample) so this fix applies identically to both.
        constexpr float kTrimGlideCyclesPerSample = 0.45f;
        combTrimSmoother.SetAlphaFromNatFreq(kTrimGlideCyclesPerSample);
        combTrimSmoother.output = 1.0f;  // unity at fb=0 -- matches the untrimmed branch (no initial fade-in).
        peakTrimSmoother.SetAlphaFromNatFreq(kTrimGlideCyclesPerSample);
        peakTrimSmoother.output = 1.0f;  // unity at height=1 (the minimum) -- matches the untrimmed branch.

        // `peakLimiter`'s attack/release coefficients ARE sample-rate-
        // dependent (unlike the two smoothers above), so unlike them it
        // cannot be fully configured here without a sample rate this
        // constructor never receives. Configure() below (called from
        // FroggersAppCore::PrepareToPlay(), mirroring
        // outputLimiter_.Configure(sampleRate_) there) supplies the real
        // one; this assumed-48kHz call just gives a direct
        // `FilterFxChain chain;` instantiation (e.g. a test that never
        // calls Configure()) the intended threshold/attack/release rather
        // than dsp::OutputLimiter's own generic master-limiter defaults
        // (0.9/1ms/100ms), which would silently under-tune this branch.
        // 48kHz matches FroggersAppCore's own `sampleRate_` field default.
        constexpr float kDefaultAssumedSampleRate = 48000.0f;
        peakLimiter.Configure(kDefaultAssumedSampleRate, kPeakLimiterThreshold, kPeakLimiterCeiling,
                               kPeakLimiterAttackSeconds, kPeakLimiterReleaseSeconds);
    }

    // Sample-rate-dependent configuration for `peakLimiter`, separate
    // from the constructor above because the real sample rate is only known
    // once FroggersAppCore::PrepareToPlay() runs. Mirrors
    // `outputLimiter_.Configure(sampleRate_)`'s own call site there.
    void Configure(float sampleRate)
    {
        peakLimiter.Configure(sampleRate, kPeakLimiterThreshold, kPeakLimiterCeiling, kPeakLimiterAttackSeconds,
                               kPeakLimiterReleaseSeconds);
    }

    // combPeakBlend/scoopMix are precomputed 0..1 control values (the
    // firmware engine derives them from smoothed knob reads via RuntimeParam,
    // which is parameter-smoothing infrastructure, not a DSP unit in scope
    // here -- callers pass the already-smoothed 0..1 value).
    //
    // `scoopMix` (Filter slot 8, "Scoop mix") shapes the chain's shared INPUT,
    // not its output: `scoopedIn` below feeds BOTH the comb and peak
    // branches, so the peak's resonance lives ON TOP of whatever material
    // the scoop leaves behind and survives any Scoop setting. This used to
    // be a second ResonantBump (`scoopNotch`) wired as a post-blend series
    // dip over the whole mixed signal, so at overlapping centers dialing
    // Scoop up subtracted exactly what dialing Peak up had just added --
    // "more knob, less filter." Moving the scoop upstream of both branches
    // removes that cancellation. Depth/Freq/Width (Filter slots 9-11) keep
    // their own independent maps, unchanged; scoopNotch has no visualizer
    // wiring either way (only peak/comb UIStates populate).
    //
    // `topology` (Filter slot 13, "Topology"/"Topo") replaces the old
    // `bool useParallel`. The old `useParallel == false` branch (:834-839,
    // `pureDelay -> comb -> peak` with no trims/limiter/blend, ignoring
    // combPeakBlend and scoopMix entirely) was DEAD CODE -- the only
    // production call site always passed `true` -- and has been deleted
    // rather than kept beside a morph. The
    // surviving path is the former `useParallel == true` branch, continuous
    // morphed by `topology` in [0,1] on ONLY the peak stage's input:
    //   peakIn = scoopedIn * (1 - topology) + combPath * topology
    // Every other computation (combTrim, peakTrim, peakLimiter, the
    // Comb/Peak blend) stays exactly as it was and stays
    // in force at every topology value. At `topology == 0`,
    // `peakIn == scoopedIn * 1.0f + combPath * 0.0f == scoopedIn`
    // bit-for-bit (IEEE multiply-by-1 and multiply-by-0/add-0 are exact for
    // finite operands), reproducing the old `useParallel == true` branch's
    // `peak.Process(scoopedIn)` call exactly -- so this is a strict
    // superset, not a behaviour change, at the default topology (and, at
    // the also-default scoopMix == 0, `scoopedIn == input` bit-for-bit too,
    // by the same IEEE argument -- so launch, where both default to 0, is
    // unaffected all the way through). Every stateful unit (comb, peak,
    // pureDelay, scoopNotch, combTrimSmoother, peakTrimSmoother,
    // peakLimiter) is still processed exactly once per sample, in the same
    // order as before. The Comb/Peak
    // blend below is equal-power across a floored `combPeakBlend` range of
    // 0.05..0.95: at either extreme the held-back branch is still present
    // at `sin(0.025*pi)` gain (~0.0785, about -22 dB), so neither branch is
    // ever fully absent.
    float Process(float input, float topology, float combPeakBlend, float scoopMix)
    {
        // Scoop shapes the chain's shared input, feeding both branches
        // below (see this method's own header comment for why). PLAIN
        // LINEAR blend, same IEEE argument as the topology morph a few
        // lines down: at scoopMix == 0 this is exactly `input` bit-for-bit.
        const float scoopedIn = input * (1.0f - scoopMix) + scoopNotch.Process(input) * scoopMix;
        const float combRaw = comb.Process(pureDelay.Process(scoopedIn));
        // Exact output trim `1/(1+|fb|)` on the comb branch
        // ONLY, before the blend with peakPath below. `|comb| <= A +
        // |fb|` (PadeSaturator bounds the fed-back term to +-1,
        // Comb::Process above), so this normalizes the worst case
        // (A=0, i.e. the comb's own decaying tail with no input) to
        // exactly 1.0 at |fb| == kMaxFeedbackMagnitude (0.95), while
        // leaving +3.5 dB of bloom across the rest of the feedback
        // travel. A scalar on a branch output moves level, not
        // frequency response -- the comb's peaks/notches/ring/decay
        // are unchanged. `fb` is read as a MAGNITUDE (GetFeedback is
        // asymmetric +-0.95) directly from Comb's own state, not
        // threaded as a new parameter.
        const float rawCombTrim = 1.0f / (1.0f + std::fabs(comb.feedback));
        const float combTrim = combTrimSmoother.Process(rawCombTrim);
        const float combPath = combRaw * combTrim;
        // Topology morph: at topology==0 this is exactly `scoopedIn` (see
        // this method's own header comment for the bit-identity argument).
        const float peakIn = scoopedIn * (1.0f - topology) + combPath * topology;
        // Exact output trim `1/height` on the peak branch, the path
        // the comb trim above deliberately left uncompensated ("one
        // variable at a
        // time"). `|peakRaw| <= A * height` (RBJ peaking biquad, centre
        // gain == height), so this normalizes the worst
        // case (input at the bump's own resonant frequency) to exactly
        // A, matching the comb trim's treatment of its own worst case.
        // `height >= 1.0` always (`ExpMapCompute(1, kMaxResonantBumpHeight,
        // knob)`), so the divisor can never be zero -- no guard needed
        // for an unreachable case.
        const float peakRaw = peak.Process(peakIn);
        const float rawPeakTrim = 1.0f / peak.height;
        const float peakTrim = peakTrimSmoother.Process(rawPeakTrim);
        const float peakTrimmed = peakRaw * peakTrim;
        // The scalar trim above cannot fully bound the peak branch
        // on its own (see peakLimiter's declaration
        // comment above and the kPeakLimiter* tuning comment near this
        // file's top) because the biquad's stored energy survives a
        // height DROP and a same-instant scalar cannot retroactively
        // remove energy already in its state. `peakLimiter` is
        // inserted HERE -- after the trim, before the blend with
        // combPath below -- so it catches exactly the residual the
        // trim leaves behind, not instead of the trim (the trim stays;
        // this is additive).
        const float peakPath = peakLimiter.Process(peakTrimmed);
        const float flooredBlend = 0.05f + 0.90f * combPeakBlend;
        const float halfPi = 0.5f * static_cast<float>(M_PI);
        const float mixed = peakPath * std::cos(flooredBlend * halfPi) + combPath * std::sin(flooredBlend * halfPi);
        return mixed;
    }
};

}  // namespace synth_froggers::dsp
