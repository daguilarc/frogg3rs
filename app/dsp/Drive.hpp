#pragma once

// synth_froggers::dsp::{PolynomialDrive, SampleRateReducer,
// DigitalReorganizer, Oversampler2x, FrogBlock, DriveBlendPhase} -- a
// **copy** of the cited Froggers formulas -- read directly from
// the firmware source before porting, not from memory.
//
// Ported (7 of the Drive page's 9 params -- Drive, Shape [a wavefolder,
// different from the Audio bank's VCO-morph Shape], SRR 1,
// SRR 2, XOR, Bit depth, Fuzz) from:
//   - src/core/PolynomialDrive.hpp
//       PolynomialDrive::SetGain/:34   Drive knob -> ExpMap(1,5,knob)
//       PolynomialDrive::SetCoefs/:38-66  Shape knob -> 5-coefficient
//         space-filling-curve wavefolder (uses the CURRENT gain TARGET,
//         not the smoothed value, at :40 `m_gain.m_target`)
//       PolynomialDrive::Process/:23-30   the polynomial evaluation itself
//       SampleRateReducer (whole file)    SRR 1 / SRR 2
//       DigitalReorganizer/:125-163       XOR (SetFlip) / Bit depth
//         (SetHash) and its bit-scramble Process
//       Oversampler2x/:69-123             2x oversample + anti-alias wrap
//         around the polynomial-drive + fuzz stage
//       FrogBlock/:165-203                the whole chain's order
//   - 08b5fd3:src/core/FroggersEngine.hpp:81-85,92    member declarations (m_srr1,
//     m_srr2, m_fuzz, m_digr, m_hash, m_frogBlock)
//   - src/core/FroggersEngine.hpp:151      RuntimeParam smoothing-rate
//     application (confirms these are ordinary smoothed knobs; smoothing
//     itself is NOT ported -- see note below)
//   - 08b5fd3:src/core/FroggersEngine.hpp:483-490  param wiring:
//       :483 SRR 1 = 1e-2 + ZeroedExp(10, 1 - GetParam(2))
//       :484 SRR 2 = 1e-2 + ZeroedExp(10, 1 - GetParam(3))
//       :485 XOR (m_digr) = GetParam(4), direct passthrough
//       :486 Bit depth (m_hash) = GetParam(5), direct passthrough
//       :487 Fuzz = GetParam(6), direct passthrough
//       :489 Drive = PolynomialDrive::SetGain(GetParam(0))
//       :490 Shape = PolynomialDrive::SetCoefs(GetParam(1))
//   - 08b5fd3:src/core/FroggersEngine.hpp:569-573  block-rate setter calls
//     (SetFreq/SetFlip/SetHash/fuzz assignment) confirming these five feed
//     FrogBlock's members directly by name.
//   - 08b5fd3:src/core/FroggersEngine.hpp:641-647  the Drive page's InitParam order
//     (GAIN, SHAPE, SRR1, SRR2, DIGR, HASH, FUZZ), confirming param indices
//     0-6 map to Drive/Shape/SRR1/SRR2/XOR/BitDepth/Fuzz in that order.
//   - 08b5fd3:src/core/FroggersEngine.hpp:872      `m_frogBlock.Process(chainIn)`,
//     confirming FrogBlock is the whole unit's entry point.
//
// TanhSaturator<false> reduces to PadeSaturator: FrogBlock's fuzz path
// (src/core/PolynomialDrive.hpp:195) reads `m_tanhSaturator.Process(out)`, where
// m_tanhSaturator's input gain is set exactly once, in FrogBlock's own
// constructor (`m_tanhSaturator.SetInputGain(1.0f)`, src/core/PolynomialDrive.hpp:184)
// and never touched again anywhere in FroggersEngine.hpp (confirmed by
// grep) -- so `TanhSaturator<false>::Process(x)` always evaluates
// `Saturate(1.0f * x)` with Normalize=false, i.e. exactly
// `dsp::PadeSaturator::Saturate(x)` (already ported in FilterFx.hpp, same
// Pade formula and clamp as 08b5fd3:src/core/TanhSaturator.hpp:25-30). Reused rather than
// re-defined.
//
// Smoothing NOT ported: the same convention as Reverb.hpp/Vco.hpp --
// FroggersEngine.hpp reads Drive/Shape/SRR1/SRR2/XOR/BitDepth/Fuzz through
// RuntimeParam (one-pole knob smoothing); that smoothing is parameter-model
// infrastructure owned by Sheaf's parameter model, not DSP. Callers here
// pass already-resolved 0..1 knob values.
//
// NOT ported (deliberately): Wet/Dry (formerly Blend) and Phase, the
// original firmware's params 7 and 8. `m_driveParams->GetParam(7)`/
// `GetParam(8)` are never read anywhere in FroggersEngine.hpp -- confirmed
// by grep -- so there is no formula to pin. They are newly authored below
// (DriveBlendPhase), clearly marked, with behavioral (not parity) tests.
// Live at this app's Drive-bank slots 0 and 8 (FroggersParameters.hpp) --
// Wet/Dry moved off the firmware's own slot 7 when the Drive page's
// controls were renumbered; Phase did not move.

#include "DspMath.hpp"
#include "FilterFx.hpp"  // reuse dsp::PadeSaturator (see note above)
#include "Limiter.hpp"   // reuse dsp::OutputLimiter (see DriveBlendPhase below)
#include "RecoveryTier.hpp"  // dsp::FiniteOnly/dsp::Magnitude, this file's own ForEachStatefulUnit calls below
                              // (already reachable transitively via FilterFx.hpp; included directly since this
                              // file names both types itself, not just through FilterFx.hpp's own use of them).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace synth_froggers::dsp {

// src/core/PolynomialDrive.hpp:12-67.
struct PolynomialDrive
{
    float gain = 1.0f;
    float coefs[5] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    // src/core/PolynomialDrive.hpp:32-36 (SetGain).
    void SetGain(float gainKnob01) { gain = ExpMapCompute(1.0f, 5.0f, gainKnob01); }

    // src/core/PolynomialDrive.hpp:38-66 (SetCoefs). Uses the CURRENT `gain` (the
    // firmware code's m_gain.m_target, i.e. the un-smoothed target) -- call
    // SetGain before SetCoefs, same order as 08b5fd3:src/core/FroggersEngine.hpp:489-490.
    //
    // `kGainCouplingScalar` is a fixed coupling constant, not a knob:
    // `coefs[1]`/`coefs[3]` gain `kGainCouplingScalar * (computedGain -
    // 1.0f)`, and that coupling is not a control a player can hear as
    // anything but a second Shape/Gain trim, so it stays fixed rather than
    // exposed on the panel. 0.25f reproduces every existing coefficient
    // parity pin and the default-reproduction test exactly.
    void SetCoefs(float shapeKnob01)
    {
        const float computedGain = gain;
        const float coefsKnob = ZeroedExpCompute(30.0f, shapeKnob01);
        constexpr float kGainCouplingScalar = 0.25f;

        coefs[0] = 1.0f + 10.0f * Sine01(coefsKnob * 1.0f);
        coefs[1] = 10.0f * Sine01(coefsKnob * 1.618f + kGainCouplingScalar * (computedGain - 1.0f));
        coefs[2] = 10.0f * Sine01(coefsKnob * 2.718f);
        coefs[3] = 10.0f * Sine01(coefsKnob * 3.141f + kGainCouplingScalar * (computedGain - 1.0f));
        coefs[4] = 10.0f * Sine01(coefsKnob * 4.669f);
    }

    // src/core/PolynomialDrive.hpp:23-30 (Process).
    float Process(float input) const
    {
        const float input2 = input * input;
        const float input3 = input2 * input;
        const float input4 = input3 * input;
        const float input5 = input3 * input2;
        return gain * (input * coefs[0] + input2 * coefs[1] + input3 * coefs[2]
                        + input4 * coefs[3] + input5 * coefs[4]);
    }

    // Deliberately no offset/bias input here: shifting this stage's own
    // input by a DC offset and subtracting that offset's own output back
    // out before the signal reaches the folder would cancel exactly the
    // asymmetry a Symmetry control needs the folder to see (Sine01 wraps
    // phase, so a DC offset downstream of such a subtraction is a phase
    // rotation, not a duty-cycle skew -- see
    // dsp::FrogBlock::symmetryOffsetCycles below). The offset is injected
    // directly at the folder's own input instead, in FrogBlock::Process.
};

// src/core/PolynomialDrive.hpp:69-123 (Oversampler2x), using dsp::OnePoleLowPass
// (DspMath.hpp) as the anti-alias filter -- identical to OPLowPassFilter.
struct Oversampler2x
{
    float prevInput = 0.0f;
    bool firstSample = true;
    OnePoleLowPass antiAlias;

    // Tier 2's per-unit sustained-over-
    // ceiling counter, owned here rather than in FroggersAppCore -- see
    // dsp::Vco::overCeilingSeconds's own comment (Vco.hpp) for the full
    // rationale.
    float overCeilingSeconds = 0.0f;

    // -----------------------------------------------------------------
    // AUTHORED, not ported: the clean path. The one-pole `antiAlias`
    // above sits at 30.7-47.9 kHz at this stage's own 96 kHz rate --
    // above the 24 kHz Nyquist of the final output, so it is nearly
    // transparent to the very content it exists to remove (measured: the
    // knob moves a driven 3 kHz tone's alias-to-signal ratio by 0.01 dB
    // across its whole range). A one-pole corner cannot fix that at any
    // setting; what does is a steeper filter with more oversampled
    // headroom to work with. This adds a second path -- the SAME
    // `processFunc` run at 4x instead of 2x, decimated through an 8th-
    // order Butterworth built from four cascaded dsp::BiquadDf1 sections
    // (the existing primitive; no new filter type) -- and crossfades it
    // against the one-pole path above, so the control keeps its name and
    // starts actually doing the job the name promises.
    //
    // Standard cascaded-biquad Butterworth design (not specific to this
    // codebase): an N-th order Butterworth lowpass factors into N/2
    // second-order sections sharing one cutoff, each at its own quality
    // factor Q_k = 1 / (2*cos(theta_k)), theta_k = (2k-1)*pi/(2N) for
    // k = 1..N/2 -- the analog Butterworth pole angles. Each section is
    // then the ordinary bilinear-transformed RBJ/Audio-EQ-Cookbook
    // lowpass biquad at that section's own Q, computed once in
    // ConfigureCleanFilter() below.
    // 4x clears the fold-images up to about 1.5 kHz, which is the range this
    // page is played in; above roughly 2 kHz the harmonics that matter have
    // passed this domain's own Nyquist, where no decimation filter reaches
    // them, so a higher factor buys the top of the range and nothing below it.
    static constexpr int kCleanOversampleFactor = 4;
    static constexpr int kCleanFilterOrder = 8;         // four cascaded BiquadDf1 sections.
    // 21 kHz at an assumed 48 kHz base rate, expressed the way this file
    // expresses every other un-Configure()'d cutoff -- cycles/sample at
    // the rate it actually runs at (here, the 4x-oversampled rate), same
    // convention as `antiAlias`'s own fixed 0.4 above:
    // 21000 / (48000 * 4) == 0.109375.
    static constexpr float kCleanCutoffCyclesPerSample = 0.109375f;

    std::array<BiquadDf1, kCleanFilterOrder / 2> cleanFilter;

    // 0.0 (default, matching a raw `Oversampler2x over;` that never calls
    // SetAntiAliasBrightness -- e.g. the existing
    // oversampler2x_first_sample_processes_twice_then_interpolates parity
    // test) is ALL grit: the crossfade below reduces to
    // `gritOutput*1 + cleanOutput*0`, bit-identical to the one-pole path
    // alone regardless of what the clean path computed, since multiplying
    // by exactly 0.0f and adding the result changes nothing in IEEE 754
    // as long as the clean path stays finite (it does -- see
    // ConfigureCleanFilter/Reset).
    float cleanMix = 0.0f;

    Oversampler2x()
    {
        antiAlias.SetAlphaFromNatFreq(0.4f);
        ConfigureCleanFilter();
    }

    void ConfigureCleanFilter()
    {
        constexpr float kTwoPi = 6.28318530717958647692f;
        constexpr float kPi = 3.14159265358979323846f;
        const float w0 = kTwoPi * kCleanCutoffCyclesPerSample;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        for (std::size_t stage = 0; stage < cleanFilter.size(); ++stage)
        {
            const float k = static_cast<float>(stage + 1);
            const float theta = (2.0f * k - 1.0f) * kPi / (2.0f * static_cast<float>(kCleanFilterOrder));
            const float q = 1.0f / (2.0f * std::cos(theta));
            const float alpha = sinw0 / (2.0f * q);
            const float b0 = (1.0f - cosw0) * 0.5f;
            const float b1 = 1.0f - cosw0;
            const float b2 = (1.0f - cosw0) * 0.5f;
            const float a0 = 1.0f + alpha;
            const float a1 = -2.0f * cosw0;
            const float a2 = 1.0f - alpha;
            BiquadDf1& section = cleanFilter[stage];
            section.b0 = b0 / a0;
            section.b1 = b1 / a0;
            section.b2 = b2 / a0;
            section.a1 = a1 / a0;
            section.a2 = a2 / a0;
        }
    }

    float ProcessCleanFilter(float x)
    {
        float y = x;
        for (BiquadDf1& section : cleanFilter)
        {
            y = section.Process(y);
        }
        return y;
    }

    // (Drive slot 9, "Anti-alias brightness" / "ABrt"): the knob is
    // repurposed from a one-pole brightness trim (which could not reach
    // the aliasing band, see class comment above) into a clean-to-grit
    // crossfade. knob01 == 1 (this control's new default,
    // FroggersParameters.hpp) is ALL grit -- bit-identical to what shipped
    // before this change, `cleanMix` at its own default of 0.0 -- and
    // knob01 == 0 is the fully clean 4x path.
    //
    // `kAntiAliasKnobExponent` warps the knob before it reaches `cleanMix`,
    // replacing a plain `cleanMix = 1 - knob01`. MEASURED (a standalone
    // sweep against this file's own Oversampler2x/PolynomialDrive, 1487 Hz,
    // Hann-windowed loudest-inharmonic-partial vs. fundamental, same
    // methodology as FroggersDspParityTests.cpp's
    // drive_anti_alias_crossfade_falls_monotonically_and_the_old_one_pole_
    // barely_moved_it): the plain map left alias reduction bunched at the
    // END of the travel closest to fully clean (quarter-turn reductions
    // -3.16 / -4.11 / -6.55 / -9.45 dB, each turn doing visibly more work
    // than the last). Exponent 1.5 spreads that far more evenly
    // (-4.65 / -5.96 / -7.70 / -4.96 dB) without moving either endpoint:
    // `1 - knob01^p` still reaches exactly 0.0 at knob01 == 1 and exactly
    // 1.0 at knob01 == 0 for any p, so both bit-identity claims this
    // control already carries (all-grit default, fully-clean floor) are
    // unaffected by this remap.
    static constexpr float kAntiAliasKnobExponent = 1.5f;
    void SetAntiAliasBrightness(float knob01) { cleanMix = 1.0f - std::pow(knob01, kAntiAliasKnobExponent); }

    template <typename ProcessFunc>
    float Process(float input, ProcessFunc processFunc)
    {
        // The one-pole path -- UNCHANGED from before this knob was
        // repurposed, so that cleanMix == 0.0 (the constructor's own
        // default, and knob01 == 1's mapped value) reproduces it exactly.
        float gritOutput;
        if (firstSample)
        {
            const float output1 = processFunc(input);
            const float output2 = processFunc(input);
            antiAlias.Process(output1);
            gritOutput = antiAlias.Process(output2);
        }
        else
        {
            const float interpolated = (prevInput + input) * 0.5f;
            const float output1 = processFunc(interpolated);
            const float output2 = processFunc(input);
            antiAlias.Process(output1);
            gritOutput = antiAlias.Process(output2);
        }

        // The clean path: kCleanOversampleFactor evaluations of the SAME
        // processFunc per input sample, linearly interpolated the same
        // way the one-pole path above interpolates for its own two
        // (first-sample: the same input repeated, matching the one-pole
        // path's own first-sample idiom above), decimated by keeping only
        // the last of each group's filtered outputs.
        float cleanOutput = 0.0f;
        if (firstSample)
        {
            for (int i = 0; i < kCleanOversampleFactor; ++i)
            {
                cleanOutput = ProcessCleanFilter(processFunc(input));
            }
        }
        else
        {
            for (int i = 1; i <= kCleanOversampleFactor; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(kCleanOversampleFactor);
                const float interpolated = prevInput + (input - prevInput) * t;
                cleanOutput = ProcessCleanFilter(processFunc(interpolated));
            }
        }

        prevInput = input;
        firstSample = false;
        // Equal-power crossfade, not linear -- the same law
        // `dsp::EqualPowerWetDry` gives every master mix on the instrument
        // (Limiter.hpp), reused directly rather than a second copy: it
        // serves this call site exactly, with a zero floor (this crossfade
        // reaches both fully-grit and fully-clean, so no page-specific
        // minimum applies). `cleanMix <= 0.0f` returns exactly
        // `{dry=1, wet=0}`, so `cleanMix == 0.0` (this struct's own
        // constructor default, and knob01 == 1's mapped value) still
        // reproduces the grit-only path bit-for-bit.
        const WetDryGains gains = EqualPowerWetDry(cleanMix, 0.0f);
        return gritOutput * gains.dry + cleanOutput * gains.wet;
    }

    // (Per-unit recovery, app/FroggersAppCore.hpp): zeros only the
    // recursive state -- prevInput (the interpolation history shared by
    // both paths), antiAlias.output (the grit path's one-pole state), and
    // every clean-filter section's own history -- and rearms firstSample
    // so the very next Process() call re-enters the "first sample" branch
    // rather than interpolating against a just-zeroed prevInput as if it
    // were real history. NOT touched: antiAlias.alpha and the clean
    // filter's own coefficients, set once and never reconfigured
    // per-block, so clearing them would be a tuning change, not a state
    // clear.
    void Reset()
    {
        prevInput = 0.0f;
        firstSample = true;
        antiAlias.output = 0.0f;
        for (BiquadDf1& section : cleanFilter)
        {
            section.x1 = 0.0f;
            section.x2 = 0.0f;
            section.y1 = 0.0f;
            section.y2 = 0.0f;
        }
        overCeilingSeconds = 0.0f;
    }

    bool StateFinite() const
    {
        if (!std::isfinite(prevInput) || !std::isfinite(antiAlias.output))
        {
            return false;
        }
        for (const BiquadDf1& section : cleanFilter)
        {
            if (!std::isfinite(section.x1) || !std::isfinite(section.x2) || !std::isfinite(section.y1) ||
                !std::isfinite(section.y2))
            {
                return false;
            }
        }
        return true;
    }
    float StateMagnitude() const
    {
        float magnitude = std::max(std::fabs(prevInput), std::fabs(antiAlias.output));
        for (const BiquadDf1& section : cleanFilter)
        {
            magnitude = std::max(magnitude, std::fabs(section.x1));
            magnitude = std::max(magnitude, std::fabs(section.x2));
            magnitude = std::max(magnitude, std::fabs(section.y1));
            magnitude = std::max(magnitude, std::fabs(section.y2));
        }
        return magnitude;
    }
};

// SampleRateReducer.hpp (whole file), verbatim.
struct SampleRateReducer
{
    float freq = 0.0f;
    float phase = 0.0f;
    float output = 0.0f;

    // Tier 2's per-unit sustained-over-
    // ceiling counter, owned here rather than in FroggersAppCore -- see
    // dsp::Vco::overCeilingSeconds's own comment (Vco.hpp) for the full
    // rationale. This struct has two independent instances (sampleRateReducer1/2
    // on dsp::FrogBlock), each with its own counter, same as any other member.
    float overCeilingSeconds = 0.0f;

    void SetFreq(float f) { freq = f; }

    float Process(float input)
    {
        if (freq >= 1.0f)
        {
            return input;
        }
        if (freq <= 0.0f)
        {
            return output;
        }
        phase += freq;
        if (phase >= 1.0f)
        {
            phase = phase - std::floor(phase);
            output = input;
        }
        return output;
    }

    // (Per-unit recovery, app/FroggersAppCore.hpp): zeros only
    // phase/output, the recursive sample-and-hold state -- NOT freq, which
    // is config reassigned every block by the caller's SetFreq(), not
    // signal state.
    void Reset()
    {
        phase = 0.0f;
        output = 0.0f;
        overCeilingSeconds = 0.0f;
    }

    bool StateFinite() const { return std::isfinite(phase) && std::isfinite(output); }
    float StateMagnitude() const { return std::max(std::fabs(phase), std::fabs(output)); }
};

// src/core/PolynomialDrive.hpp:125-163 (DigitalReorganizer). NOTE: :138's
// `std::round(inputUp)` assigned to a uint8_t is float-to-integer
// narrowing that is well-defined only while `inputUp` (== (input+1)*128)
// stays within [0,255] -- i.e. input in roughly [-1, 0.9921875]. At
// input==1.0 exactly, inputUp==256 and the cast is undefined behavior in
// the firmware source too (confirmed by reading src/core/PolynomialDrive.hpp:135-151
// directly).
//
// FIX, NOT A REPRODUCTION: unlike
// the fuegoize UB (the retired simulator's Fuegoize.hpp), which is carried forward
// because the firmware tree also contains a *correct* reference (the
// firmware's 08b5fd3:src/core/Parameter.hpp:142) to port instead, there is no such correct
// reference here -- both src/core/PolynomialDrive.hpp:138 in the `src/core/`
// tree and this port hit the same undefined cast at input==1.0. This is
// newly written code this app owns, and reproducing UB has no parity
// value, so this port clamps the rounded value to the uint8_t-representable
// range [0,255] *before* the narrowing cast, then computes `inputRemainder`
// against that same clamped value (not the raw `inputUp`). Two
// consequences: (1) everywhere the original expression was well-defined
// (`inputUp` in [0,255], i.e. input in [-1, 0.9921875]) the clamp is a
// no-op, so `round(inputUp)` is already in range and behavior is bit-for-
// bit unchanged; (2) at the boundary and beyond (input >= ~0.9921875, or
// input < -1 -- reachable in practice, since PolynomialDrive's output
// upstream of this stage is not amplitude-bounded to [-1,1]) the clamp
// saturates the integer part while the remainder still carries the
// leftover delta, so with flip==0 and hashBits==0 (the pass-through
// configuration) `Process(1.0f)` still reconstructs to exactly `1.0f`
// rather than invoking UB -- see the regression test at input==1.0 in
// FroggersDspParityTests.cpp.
//
// DELIBERATE PARITY DIVERGENCE #2 (same class of divergence as
// dsp::Comb::GetFeedback's +-1.1 -> +-0.95 above (FilterFx.hpp) and the
// resonant-peak ceiling's 10x -> 4x -> 2x (FilterFx.hpp's
// kMaxResonantBumpHeight), each carrying its own in-code note):
//
// `Process(0.0f)` -- digital silence -- is NOT silent. At input==0,
// `inputUp` is exactly 128 and `inputRemainder` is exactly 0, but the XOR by
// `flip` and the mask-gated bit scramble below still run, so the result is
// nonzero for any `flip != 0` (exactly -1.0 at flip==128) and also nonzero
// at flip==0 when hashBits==8. This stage sits upstream of three recursive
// loops (dsp::Comb, dsp::StereoDelay, dsp::Reverb -- the last with no
// in-loop saturator), which amplify that seed without bound -- "Stop
// doesn't stop," measured and traced end-to-end.
//
// PLAINLY: the firmware (src/core/PolynomialDrive.hpp:125-163) has
// this exact same f(0) != 0 behaviour -- it is a property of the original
// bit-scramble math, not a porting error -- and no DC blocker or highpass
// exists anywhere in this signal chain (src/core/ and app/ both
// checked). On real hardware, an analog output stage AC-couples a DC offset
// away for free, so this was inaudible on the original instrument; this
// port has no such output stage, so without a fix nothing downstream ever
// removes it.
//
// FIX: the scramble is factored into `Mangle()` below (identical math, no
// behaviour change by itself), and `Process` returns
// `Mangle(input, flip, hashBits) - Mangle(0.0f, flip, hashBits)` -- a DC
// block anchored to this stage's own silent-input response, computed fresh
// on every call rather than cached (`flip`/`hashBits` are public fields
// that can be assigned directly, bypassing SetFlip()/SetHash(), so a
// correction cached only inside those setters would go stale the moment
// they were bypassed). At the pass-through configuration (flip==0,
// hashBits==0), `Mangle(0.0f, 0, 0) == 0.0f` exactly, so the correction
// term is exactly zero there and this fix is a no-op: `Process(1.0f) ==
// 1.0f` (the input-clamp fix's own regression test, above) and every parity
// case at default flip/hash are unaffected. `FroggersDspParityTests.cpp`'s
// digital_reorganizer_process_matches_bit_scramble_formula pins `Process()`
// against a hand-transcribed replica of this formula at nonzero flip/hash;
// it is re-asserted against this corrected behaviour (see that test's own
// comment), never deleted.
struct DigitalReorganizer
{
    uint8_t flip = 0;
    uint8_t hashBits = 0;

    // The bit-mangle itself -- unchanged from the original formula (the
    // input clamp above still applies); factored out of Process() purely so
    // the divergence note above can call it twice: once at the real input,
    // once at silence. Reused (2 call sites) and isolates a distinct
    // transformation stage. One definition, so neither call site in
    // Process() below copies this expression.
    static float Mangle(float input, uint8_t flip, uint8_t hashBits)
    {
        const float inputUp = (input + 1.0f) * 128.0f;
        const float roundedUp = std::round(inputUp);
        const float clampedUp = std::min(std::max(roundedUp, 0.0f), 255.0f);
        uint8_t inputInt = static_cast<uint8_t>(clampedUp);
        const float inputRemainder = inputUp - clampedUp;

        inputInt = static_cast<uint8_t>(inputInt ^ flip);
        const uint8_t mask = static_cast<uint8_t>((1 << hashBits) - 1);
        uint8_t lowerBits = static_cast<uint8_t>(inputInt & mask);

        lowerBits = static_cast<uint8_t>(lowerBits ^ ((lowerBits << 3) & mask));
        lowerBits = static_cast<uint8_t>(lowerBits ^ ((lowerBits >> 5) & mask));
        lowerBits = static_cast<uint8_t>(lowerBits ^ ((lowerBits << 1) & mask));

        inputInt = static_cast<uint8_t>((inputInt & ~mask) | lowerBits);

        return (static_cast<float>(inputInt) + inputRemainder) / 128.0f - 1.0f;
    }

    // DC-blocked at this stage's own silent-input response -- see the
    // divergence note above this struct. Computed fresh every sample, never
    // cached (flip/hashBits are public and may be reassigned directly,
    // bypassing SetFlip()/SetHash()).
    float Process(float input) const
    {
        return Mangle(input, flip, hashBits) - Mangle(0.0f, flip, hashBits);
    }

    void SetFlip(float flipKnob01) { flip = static_cast<uint8_t>(flipKnob01 * 255.0f); }  // :154-157, truncates

    // AUTHORED remap, not the ported :159-162 formula (kept only in this
    // comment for the record: `round(hashKnob01 * 8)`, nine positions
    // 0..8). That formula wasted its own first fifth: hashBits == 1 masks
    // exactly one bit, and Mangle's three shift-XOR steps
    // (lowerBits << 3, >> 5, << 1, each masked back to the live bits)
    // cancel completely for a one-bit mask -- shifting a single bit by 3
    // or left by 1 always carries it outside a 1-bit mask before the AND,
    // and shifting right by 5 always underflows to zero -- so hashBits == 1
    // measures bit-identical to hashBits == 0 (`Process()` unchanged to
    // float precision), not merely quiet. hashBits == 0 stays reachable
    // (silence has to stay reachable), but the count the DEFAULT knob
    // (0.0f, FroggersParameters.hpp) produces is unchanged, and every other
    // position now maps onto a count that actually scrambles something:
    // 2..8, geometrically enough of the range that knob 0.19 -- today's
    // first audible position -- becomes reachable within the first
    // hundredth instead.
    void SetHash(float hashKnob01)
    {
        constexpr float kOffFloor = 0.01f;
        if (hashKnob01 <= kOffFloor)
        {
            hashBits = 0;
            return;
        }
        constexpr uint8_t kFirstActingBitCount = 2;
        constexpr uint8_t kLastBitCount = 8;
        const float travel = (hashKnob01 - kOffFloor) / (1.0f - kOffFloor);
        const float span = static_cast<float>(kLastBitCount - kFirstActingBitCount);
        hashBits = static_cast<uint8_t>(kFirstActingBitCount + std::round(travel * span));
    }
};

// 08b5fd3:src/core/TanhSaturator.hpp:25-30 already ported as
// dsp::PadeSaturator (FilterFx.hpp) -- reused directly, see file-header note.

// src/core/PolynomialDrive.hpp:165-203 (FrogBlock).
struct FrogBlock
{
    PolynomialDrive polynomialDrive;
    SampleRateReducer sampleRateReducer1;
    SampleRateReducer sampleRateReducer2;
    DigitalReorganizer digitalReorganizer;
    Oversampler2x oversampler;
    float fuzz = 0.0f;

    // (Drive slot 11, "Fold"): knob-mapped
    // divisor for the sinIn fold below, replacing the hardcoded 4.0f.
    // Default 4.0f -- matches today's literal for any instance that never
    // calls SetFold (e.g. frog_block_process_matches_manual_chain_replica,
    // which pins the OLD hardcoded `out / 4.0f` formula directly).
    float foldDivisor = 4.0f;

    // (Drive slot 12, "Tone"): post-chain
    // low-pass. Default alpha 1.0f (bypass -- see SetTone) so an instance
    // that never calls SetTone (same parity test as above) still processes
    // as an exact identity, matching today's FrogBlock exactly.
    OnePoleLowPass tone{1.0f};

    // (Drive slot 13, "Symmetry"): default 0.0f (no offset) -- see
    // SetSymmetry below. Applied directly at the FOLDER's own input, in
    // phase units, rather than at the polynomial's -- see SetSymmetry and
    // Process() below for why.
    float symmetryOffsetCycles = 0.0f;

    // (Drive slot 10, "Feedback"): the fraction of the
    // folder's own PREVIOUS output fed back into its own input -- see
    // SetFeedback and Process() below. Default 0.0f: no feedback, the
    // folder reduces to today's plain `Sine01(out / foldDivisor)`.
    float feedbackCoefficient = 0.0f;

    // The folder's own (DC-corrected) output from the previous call, held
    // for exactly one sample so `feedbackCoefficient` has something to feed
    // back INTO the next call's phase argument -- the one-sample delay a
    // feedback loop needs to be a loop rather than an algebraic
    // self-reference. A dedicated struct rather than a bare float purely so
    // it can join the ForEachStatefulUnit/RecoverPoisonedUnitState
    // enumeration below, which visits TYPES carrying their own
    // Reset()/StateFinite()/StateMagnitude(), the same shape
    // SampleRateReducer/Oversampler2x already are.
    //
    // Bounded to [-2, 2] in ordinary operation, because it is always
    // assigned the difference of two Sine01 calls (see Process() below),
    // each of which never leaves [-1, 1] regardless of its argument's
    // magnitude -- this keeps the feedback loop's amplitude bounded no
    // matter how large `feedbackCoefficient` or the signal driving it gets.
    // Boundedness alone does not make the loop STABLE, though -- see
    // `kMaxFeedbackCoefficient`'s own comment for the analysis that does.
    // This state still needs the ordinary finite-state guard below: a
    // NaN/Inf reaching `out` upstream would propagate through Sine01 into
    // this state and then regenerate itself forever afterward (Sine01(NaN)
    // is NaN), the same "poisoned state that never recovers on its own"
    // shape every other recursive unit in this file already carries a guard
    // for.
    struct FolderFeedbackState
    {
        float value = 0.0f;
        void Reset() { value = 0.0f; }
        bool StateFinite() const { return std::isfinite(value); }
        float StateMagnitude() const { return std::fabs(value); }
    };
    FolderFeedbackState folderFeedback;

    // This struct's own contribution to the "every stateful unit in the
    // audio path" enumeration -- lists ONLY the members declared above that
    // RecoverPoisonedUnitState ever watched (not polynomialDrive/
    // digitalReorganizer/fuzz -- see app/FroggersAppCore.hpp's own
    // RecoverPoisonedUnitState comment for which units that was and why).
    // `folderFeedback` joins the list at Tier 1 (FiniteOnly) rather than
    // Tier 2 (Magnitude): it has no meaningful "sustained over ceiling"
    // reading of its own (its value is always within [-1, 1] by
    // construction whenever it is finite at all), so only the finite-state
    // guard applies.
    // Composed, not re-listed, by FroggersAppCore::ForEachStatefulUnit below.
    template <typename Visitor>
    void ForEachStatefulUnit(Visitor&& visit)
    {
        visit(sampleRateReducer1, Magnitude{});
        visit(sampleRateReducer2, Magnitude{});
        visit(oversampler, Magnitude{});
        visit(folderFeedback, FiniteOnly{});
    }

    // Knob rises -> divisor FALLS, from 16.0 at knob 0 down to 1.0 at knob
    // 1 -- `out / foldDivisor` is what actually enters the folder below, so
    // a smaller divisor sends more of `out`'s own swing through Sine01's
    // wrap per cycle, i.e. more folds. Divisor 1.0 (knob 1) is maximum
    // folding, not "no folding"; there is no knob position that turns
    // folding off.
    // Strictly positive by construction regardless of which argument is
    // larger (ExpMapCompute's own `min * pow(max/min, value)`, and a
    // positive `min` raised to any finite power stays strictly positive,
    // so the divisor can never reach or cross zero) -- crucial, since
    // `out / 0` would be +-inf, and Sine01's own
    // `phase - std::floor(phase)` turns that into NaN, which this codebase
    // has already been silenced permanently by once. Default knob 0.5f
    // reproduces exactly 4.0f either way the endpoints are named:
    // ExpMapCompute(16,1,0.5) == sqrt(16) == 4 (16 == 4^2 by choice of
    // range, same trick as SetAntiAliasBrightness above).
    void SetFold(float foldKnob01) { foldDivisor = ExpMapCompute(16.0f, 1.0f, foldKnob01); }

    // Alpha fed directly (Reverb.hpp's own damping-filter idiom -- "the
    // ExpMap output IS the alpha", not run through SetAlphaFromNatFreq).
    // The range, and why its floor is where it is, live with the map itself
    // (DspMath.hpp's ToneAlphaFromKnob) rather than here: the Delay bank's
    // Feedback tone is the same control and reads the same function.
    void SetTone(float toneKnob01) { tone.alpha = ToneAlphaFromKnob(toneKnob01); }

    // Symmetry offset, in PHASE units (cycles), injected directly at the
    // FOLDER's own input rather than at the polynomial's -- Sine01 wraps
    // phase, so a DC offset placed there (the old Bias's placement) is a
    // phase ROTATION, and phase is circular: a full-cycle offset is
    // bit-identical to no offset at all (measured,
    // `sum|Sine01(x) - Sine01(x+1.0)|` over 4800 samples reads 0.0005,
    // float noise).
    //
    // BIPOLAR, centred on the knob's own midpoint: knob 0.5 reproduces
    // offset == 0.0f exactly, matching no offset at all, and the two halves
    // of the travel skew the wave in opposite directions from there. A
    // one-directional (0 to positive) mapping looks appealing on a bare
    // sine folder, where the asymmetry it produces rises monotonically with
    // the offset -- but through this chain the folder shares the output
    // with `PolynomialDrive`'s own even harmonics, whose sign swings with
    // Gain, so a one-directional offset that starts at zero REVERSES which
    // way the wave skews as Gain changes: the higher end of its travel can
    // read as LESS asymmetric than a setting closer to its floor. Centring
    // the travel does not remove that interaction, but it means the control
    // moves through it symmetrically from both sides rather than sitting
    // pinned at one edge of it.
    //
    // Bounded to +-0.02 cycles -- measured (across Symmetry x Shape x Gain x
    // Fold x input amplitude) as the widest bound at which the signed
    // asymmetry statistic never reverses direction anywhere in that grid;
    // widening it re-admits the same reversal this bound exists to avoid,
    // in exchange for more travel. Do not widen it without re-measuring.
    // Also, coincidentally, the same range the ported Bias control already
    // used, so this keeps that range rather than choosing a new one.
    //
    // Deliberately NOT scaled by `foldDivisor` here: the offset is already
    // expressed directly in the same phase units Sine01's argument is in,
    // so adding it AFTER the `out / foldDivisor` divide in Process() below
    // is exactly what does the scaling -- it lands as the same number of cycles
    // regardless of what Fold's own divisor is currently set to, which is
    // what keeps Fold from changing what Symmetry means.
    void SetSymmetry(float symmetryKnob01) { symmetryOffsetCycles = 0.02f * (2.0f * symmetryKnob01 - 1.0f); }

    // The maximum feedback coefficient the knob can reach. With the input
    // silent, `out` is exactly 0 (every term of PolynomialDrive::Process
    // multiplies a positive power of its input), so the folder's own
    // recursion reduces to `x[n+1] = Sine01(symmetryOffsetCycles +
    // feedbackCoefficient * x[n])`, i.e. `sin(2*pi*(c + a*x[n]))`. That map's
    // local slope at its own fixed point never exceeds `2*pi*a` in
    // magnitude (`|cos| <= 1`, reaching exactly 1 -- the worst case -- at
    // `c == 0`, Symmetry off), so the fixed point is stable only while
    // `a < 1 / (2*pi)`; past it the loop does not diverge (Sine01 stays in
    // [-1, 1] regardless), but it also does not decay -- it latches onto a
    // nonzero fixed point, a period-2 cycle, or broadband chaos depending on
    // how far past the bound `a` sits. `kFeedbackStabilityMargin` reuses the
    // exact pole-margin factor `DriveBlendPhase::kPhaseCoeffMargin` already
    // applies to a different pole in this file (0.98) rather than a fresh
    // round number, so the coefficient sits proportionally as far inside the
    // stable region at every Symmetry setting as that other pole sits inside
    // the unit circle.
    static constexpr float kFeedbackStabilityMargin = 0.98f;
    static constexpr float kTwoPi = 6.28318530717958647692f;
    static constexpr float kMaxFeedbackCoefficient = kFeedbackStabilityMargin / kTwoPi;
    // Default knob 0.0f reproduces feedbackCoefficient == 0.0f exactly --
    // no feedback, folderFeedback.value's own contribution to `sinIn` below
    // multiplies out to zero regardless of its history.
    void SetFeedback(float feedbackKnob01) { feedbackCoefficient = kMaxFeedbackCoefficient * feedbackKnob01; }

    // src/core/PolynomialDrive.hpp:187-202 (FrogBlock::Process), same order
    // as before, with three changes: the polynomial no longer sees a
    // biased input (Symmetry moved downstream, see SetSymmetry above);
    // `sinIn` gains the Symmetry offset and the folder's own one-sample-
    // delayed feedback; and the Fold/Fuzz blend is now a floored
    // equal-power crossfade (`dsp::FlooredEqualPowerBlend`, the same law
    // `FilterFxChain::Process` uses for its own Comb/Peak blend,
    // dsp/Limiter.hpp) rather than a linear one that could reach a hard zero
    // on the folder's leg -- see this struct's own class-level notes above
    // for why. `foldDivisor` still replaces
    // the old literal divisor, and the tone stage is still appended after
    // the ported chain.
    float Process(float input)
    {
        // The same floored equal-power law FilterFxChain::Process uses for
        // its own Comb/Peak blend, single-sourced as
        // `dsp::FlooredEqualPowerBlend` (dsp/Limiter.hpp) rather than a
        // second copy: neither leg of a blend built this way is ever fully
        // silent -- at either extreme the held-back leg still sits at
        // `sin(0.05*halfPi)` gain (about -22 dB), so Fold can no longer be
        // multiplied by exactly zero the way the old linear blend let Fuzz
        // do at its own maximum.
        const FloorBlendGains fuzzBlendGains = FlooredEqualPowerBlend(fuzz);
        const float folderGain = fuzzBlendGains.legA;
        const float saturatorGain = fuzzBlendGains.legB;

        float output = oversampler.Process(input, [this, folderGain, saturatorGain](float in) -> float {
            const float out = polynomialDrive.Process(in);
            // Feedback re-enters at the FOLDER's own input alone, not the
            // polynomial's -- Sine01's output is bounded to [-1, 1]
            // regardless of its argument's magnitude, so no matter how
            // large `feedbackCoefficient` or `out` gets, this term can only
            // ever add at most `feedbackCoefficient` cycles of phase, never
            // an unbounded amount of amplitude. Boundedness alone does not
            // keep the loop from self-oscillating, though -- a bounded map
            // can still settle on a nonzero fixed point, a period-2 cycle or
            // broadband chaos instead of decaying, which is exactly what an
            // unmargined coefficient does (see `kMaxFeedbackCoefficient`'s
            // own comment above for the analysis that bounds it against
            // that, not merely against runaway amplitude).
            const float sinIn =
                out / foldDivisor + symmetryOffsetCycles + feedbackCoefficient * folderFeedback.value;
            // Sine01's response to the Symmetry offset alone (zero drive,
            // zero feedback contribution) is not silence, so a phase offset
            // sitting ahead of it would otherwise leave a DC term at the
            // output whenever the folder is driven -- the same
            // response-to-nothing anchor `DigitalReorganizer::Process`
            // already subtracts above (`Mangle(input) - Mangle(0)`),
            // applied here instead of before the fold, which is what lets
            // the offset still reach Sine01 as a genuine phase shift (see
            // SetSymmetry's own comment for why it has to). Subtracting it
            // from the value fed back, not only from this call's own
            // output, makes silence a fixed point of the feedback recursion
            // itself at every Symmetry setting: with `out` and `folded`
            // both driven to 0, `Sine01(symmetryOffsetCycles) -
            // Sine01(symmetryOffsetCycles) == 0` exactly, regardless of
            // `feedbackCoefficient`, so a silenced input decays to true
            // silence rather than to the offset's own quiescent level.
            const float folded = Sine01(sinIn) - Sine01(symmetryOffsetCycles);
            folderFeedback.value = folded;
            const float saturated = PadeSaturator::Saturate(out);
            return folded * folderGain + saturated * saturatorGain;
        });

        output = digitalReorganizer.Process(output);
        output = sampleRateReducer1.Process(output);
        output = sampleRateReducer2.Process(output);
        output = tone.Process(output);
        return output;
    }
};

// -------------------------------------------------------------------------
// Authored, NOT ported: Wet/Dry (formerly Blend) and Phase, Drive page
// slots 0/8. No Froggers original exists (see file header) -- design
// rationale below.
//
// Wet/Dry crossfades the dry input against the driven (FrogBlock) signal --
// the common "parallel drive" pattern that keeps the raw input available
// underneath the processed tone. Phase applies a first-order allpass to
// the wet signal before the blend; the coefficient is mapped from the knob
// into (-0.98, 0.98) -- STRICTLY inside the unit circle, not [-1, 1] --
// pairing an allpass with a dry/wet blend is the standard way parallel-
// drive designs avoid comb-filtering / phase-cancellation artefacts when
// the two paths recombine. At the neutral default (blendKnob01 == 0),
// Process() returns `dry` exactly regardless of phaseKnob01 or wet, so this
// authored stage never disturbs the seven ported params -- callers can
// drive the ported FrogBlock alone and ignore this struct entirely.
//
// An earlier revision of this struct mapped the knob to the CLOSED interval
// [-1, 1] and claimed that "keeps a first-order allpass unconditionally
// stable (energy-preserving) for any input." That claim is false at the
// endpoints. Process()'s recurrence is
// `y[n] = -a*x[n] + x[n-1] + a*y[n-1]` -- a first-order allpass whose pole
// sits at `z = a`. Stability requires `|a| < 1` STRICTLY; `|a| = 1` places
// the pole exactly ON the unit circle, where the homogeneous response
// (`y[n] = a*y[n-1]` once the input stops exciting it) neither grows nor
// decays -- it rings forever at constant amplitude. Both endpoints were
// reachable (phaseKnob01 clamps to [0,1] inclusive, matching every other
// knob in this app), and phaseKnob01 DEFAULTS to 0, i.e. a == -1 ships by
// default: every fresh instance of this app started with its pole sitting
// on the unit circle, state that never decays once excited. Fixed by
// scaling the coefficient strictly inside the unit circle (0.98, the same
// margin dsp::Reverb's own Decay/Hold ceiling uses, Reverb.hpp's
// DecayFeedbackFromKnob) --
// this leaves the audible sweep essentially unchanged (0.98 vs. 1.0 shifts
// the allpass's frequency-dependent group delay by a negligible amount at
// every phaseKnob01 value) while guaranteeing every pole strictly inside
// the unit circle, so the state provably decays instead of only "usually"
// decaying. This is NOT the kind of clamp this codebase generally avoids --
// the discouraged pattern guards an UNREACHABLE zero divisor; here `|a| == 1` is both
// reachable and the shipped default, so the fix changes real, exercised
// behavior rather than adding dead code.
// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
// The allpass coefficient `a` below was read fresh from the
// Phase knob every sample with NO smoothing. A FIXED-coefficient allpass is
// unity-gain; a TIME-VARYING one is not -- measured, with a bounded
// +-1 `wet` input: 1.002 under free random phase, 4.15 under full-bank
// per-sample-random modulation (`RouteAudioSample()` refreshes every knob
// read every sample -- this is not an edge case, it is what audio-rate noise
// modulation of this knob does continuously), and 50.5 under periodic
// phase/content coincidence (an LFO landing near the note's own period),
// bounded and plateauing but the largest blowout path measured in the
// instrument -- 50x dwarfs the comb's ~1.95 and the peak's 1.669.
//
// TWO-PART FIX, both measured against a standalone harness reproducing
// this struct's exact math (not assumed by analogy -- the comb-trim
// smoother's own glide constant was picked by analogy and measured 80%
// wrong):
//
// 1. SMOOTH THE COEFFICIENT (root cause, tried first). A
//    one-pole (`coeffSmoother` below, `dsp::OnePoleLowPass` reused, not a
//    new smoother) on `a` makes the allpass quasi-static, restoring
//    near-unity gain. Swept 0.0005-0.5 cycles/sample against two
//    adversarial patterns: (A) phaseKnob01 AND wet both redrawn uniform
//    per-sample-random (the "full-bank" case) and (B) a periodic
//    phase/content coincidence (wet a sine, phaseKnob01 a synced
//    square-ish LFO at a rational-multiple frequency) that reproduces
//    that same periodic-coincidence mechanism -- found up to 61.2x unsmoothed
//    (exceeds the recorded 50.5x; verified bounded/plateauing to 20M
//    samples, matching that earlier plateau finding, not divergent).
//    Result: smoothing alone drives pattern B to ~1.0x at any glide <=
//    ~0.05 (the periodic mechanism needs `a` to track the content in sync;
//    a lagging coefficient breaks the lock). Pattern A -- persistent full-
//    range per-sample redraws, not a one-off transient -- has a genuine
//    residual floor smoothing cannot close: minimized (10 seeds x 1M
//    samples/glide) at ~0.0035 cycles/sample (worst 1.405x), WORSE both
//    slower (a slow-moving coefficient dwelling near the +-0.98 pole for
//    many samples re-creates pattern B's own reinforcement, 1e-5 cyc/samp
//    -> 1.86x) and faster (approaches the unsmoothed case, 0.45 cyc/samp
//    -> 3.96x) -- so 0.0035 is a genuine measured minimum, not a monotone
//    tradeoff. Same class of fix as the comb-trim smoother's trim smoothing;
//    costs nothing tonally, since an unsmoothed per-sample-random coefficient was never
//    a musical control (static-Phase output is unchanged, see
//    FroggersDspParityTests.cpp's static-neutrality pin).
// 2. LIMIT THE STAGE OUTPUT (only because step 1 proved insufficient --
//    pattern A's ~1.4-1.7x residual, confirmed against 20 seeds x 2M
//    samples at the chosen glide, does not close on its own; same
//    structural finding as the peak branch, where a per-sample
//    scalar trim alone plateaued at 1.669x and needed its own limiter).
//    `dsp::OutputLimiter` (Limiter.hpp), five-argument `Configure()`.
//    Threshold/attack/release swept together (thresholds 0.7-0.9,
//    attacks 1us-1ms) against both patterns above (10-20 seeds x
//    500k-2M samples): attack must be MICROSECONDS, not the ~1ms
//    originally guessed for "sustained" excess -- mechanism-shape predicted
//    attack wrong twice already;
//    measurement is what actually decides it, every stage so far has
//    needed microseconds. 1000x slower (1ms) leaves pattern A at 1.39x,
//    barely better than smoothing alone; 2us reaches 0.990x. Threshold
//    0.7 (below the master's 0.9, same headroom logic as the peak
//    branch's kPeakLimiterThreshold) with `kSharedCeiling`/
//    `kSharedReleaseSeconds` (Limiter.hpp, reused rather than
//    re-declared) rounds out the tuning. FINAL, measured:
//    pattern A worst 0.990x (20 seeds x 2M samples), pattern B worst
//    0.896x (20M samples) -- both at or below the ~1.0 bound FrogBlock's
//    own output already respects, so this stage no longer
//    forces the master limiter to engage on its own account.
// -------------------------------------------------------------------------
struct DriveBlendPhase
{
    // Measured minimum of the pattern-A sweep above; NOT chosen by analogy
    // to FilterFxChain's kTrimGlideCyclesPerSample (0.45, a different
    // problem -- smoothing an output-level TRIM, not a recursive filter's
    // own pole -- and measured far too fast here, see class comment).
    static constexpr float kPhaseCoeffGlideCyclesPerSample = 0.0035f;

    // Measured tuning (class comment): below the master's 0.9 threshold,
    // same headroom logic as the peak branch's kPeakLimiterThreshold.
    static constexpr float kOutputLimiterThreshold = 0.7f;
    // This limiter is Configure()'d against kStageCeiling (see
    // the outputLimiter.Configure(...) call below, retargeted off
    // kSharedCeiling), so that is the ceiling the invariant is
    // checked against -- unchanged threshold (0.7, MEASURED, class comment
    // above), already strictly below kStageCeiling (0.80). See
    // dsp::OutputLimiter::kDefaultThreshold's own static_assert
    // (dsp/Limiter.hpp) for why a negative headroom is both catastrophic and
    // invisible to every finiteness guard in the suite.
    static_assert(kOutputLimiterThreshold < kStageCeiling,
                  "threshold must stay strictly below ceiling; a negative headroom turns "
                  "DesiredMagnitude into an exponential amplifier -- see dsp/Limiter.hpp");
    static constexpr float kOutputLimiterAttackSeconds = 2.0e-6f;  // 2 microseconds -- measured, see class comment.

    // Both endpoints of the Phase knob's coefficient range -- same 0.98
    // margin the coefficient mapping used directly before it was rerouted
    // through the break frequency (see CoeffFromBreakFreq/BreakFreqFromCoeff
    // below), so the allpass's pole stays exactly as far inside the unit
    // circle as it always has.
    static constexpr float kPhaseCoeffMargin = 0.98f;

    float allpassX1 = 0.0f;
    float allpassY1 = 0.0f;

    // The break frequencies (cycles/sample) phaseKnob01 == 0 and
    // phaseKnob01 == 1 reach -- derived from kPhaseCoeffMargin in
    // Configure() below rather than hardcoded, so a future change to the
    // margin re-derives these rather than going stale next to it.
    float phaseBreakFreqAtNegativeMargin = 0.0f;
    float phaseBreakFreqAtPositiveMargin = 0.0f;

    // Tier 2's per-unit sustained-over-
    // ceiling counter, owned here rather than in FroggersAppCore -- see
    // dsp::Vco::overCeilingSeconds's own comment (Vco.hpp) for the full
    // rationale.
    float overCeilingSeconds = 0.0f;

    // Smooths the allpass coefficient `a`, not the Phase knob or `wet`
    // themselves -- the excess is specifically a time-varying POLE, so the
    // coefficient feeding the recurrence is what must be made quasi-static
    // (class comment, fix 1).
    OnePoleLowPass coeffSmoother;

    // Class comment, fix 2: catches the residual smoothing alone cannot
    // close. Attack/release are sample-rate-dependent, so -- same reason
    // FilterFxChain::peakLimiter/Reverb::wetLimiter/Delay's wetLimiterL/R
    // all need their own Configure(sampleRate) -- this type gets one too,
    // called from the constructor (assumed-48kHz default, matching
    // FilterFxChain's/Reverb's own constructor-time Configure()) AND from
    // FroggersAppCore::PrepareToPlay() once the real host rate is known
    // (driveBlendPhase_.Configure(sampleRate_), mirroring
    // filterChain_.Configure(sampleRate_)/reverb_.Configure(sampleRate_)
    // there).
    OutputLimiter outputLimiter;

    DriveBlendPhase()
    {
        // Same assumed-48kHz constructor-time default FilterFxChain's/
        // Reverb's own constructors use for their sample-rate-dependent
        // limiters, for the identical reason (a direct `DriveBlendPhase bp;`
        // instantiation -- e.g. a test -- that never calls Configure() still
        // gets a harmless, finite tuning rather than an unconfigured one).
        constexpr float kDefaultAssumedSampleRate = 48000.0f;
        Configure(kDefaultAssumedSampleRate);
    }

    // The allpass `H(z) = (z^-1 - a) / (1 - a*z^-1)` this struct evaluates
    // has unit magnitude at every frequency and crosses -90 degrees of phase
    // at one frequency per value of `a` -- its "break frequency". Solving
    // H(e^{j*2*pi*fb}) = -j for `a` gives a closed form:
    // `a = tan(pi/4 - pi*fb)`, monotonically decreasing from +1 at fb == 0 to
    // -1 at fb == 0.5 cycles/sample. This is the inverse of
    // BreakFreqFromCoeff below, so the two must be kept in the same
    // convention (fb in cycles/sample, `a` this struct's own coefficient)
    // if either one changes.
    static float CoeffFromBreakFreq(float breakFreqCyclesPerSample)
    {
        constexpr float kQuarterCircle = 0.78539816339744830962f;  // pi/4
        constexpr float kTwoPi = 6.28318530717958647692f;
        return std::tan(kQuarterCircle - 0.5f * kTwoPi * breakFreqCyclesPerSample);
    }

    // Inverse of CoeffFromBreakFreq: the break frequency (cycles/sample)
    // at which this allpass's phase crosses -90 degrees for a given `a`.
    static float BreakFreqFromCoeff(float a)
    {
        constexpr float kQuarterCircle = 0.78539816339744830962f;  // pi/4
        constexpr float kPi = 3.14159265358979323846f;
        return (kQuarterCircle - std::atan(a)) / kPi;
    }

    void Configure(float sampleRate)
    {
        coeffSmoother.SetAlphaFromNatFreq(kPhaseCoeffGlideCyclesPerSample);
        // Seeded to `a` at phaseKnob01 == 0 (the knob's own default, class
        // header note above) so a fresh instance produces no startup
        // transient -- same idiom as FilterFxChain's combTrimSmoother/
        // peakTrimSmoother seeding their `.output` to the untrimmed value
        // at each trim's own default knob position. `coeffSmoother`'s glide
        // is sample-rate-INdependent (SetAlphaFromNatFreq takes cycles/
        // sample, same idiom FilterFxChain's own trim smoothers use, class
        // header note), so re-running it here on a later Configure() call
        // is a harmless no-op re-derivation, not a behaviour change.
        coeffSmoother.output = -0.98f;
        // The two break frequencies phaseKnob01's endpoints reach --
        // derived from the SAME +-0.98 margin the coefficient mapping used
        // to reach directly, so both endpoints stay exactly as far inside
        // the unit circle as before. Computed once here rather than per
        // sample -- Process() below only takes the atan-free direction
        // (CoeffFromBreakFreq), and does it every sample because
        // phaseKnob01 is not assumed static (see class header comment on
        // audio-rate modulation of this knob).
        phaseBreakFreqAtNegativeMargin = BreakFreqFromCoeff(-kPhaseCoeffMargin);  // near Nyquist -> a == -0.98
        phaseBreakFreqAtPositiveMargin = BreakFreqFromCoeff(kPhaseCoeffMargin);   // near DC -> a == +0.98
        // Retargeted from kSharedCeiling to kStageCeiling -- see the
        // static_assert above.
        outputLimiter.Configure(sampleRate, kOutputLimiterThreshold, kStageCeiling, kOutputLimiterAttackSeconds,
                                 kSharedReleaseSeconds);
    }

    float Process(float dry, float wet, float blendKnob01, float phaseKnob01)
    {
        // Mapped through the allpass's own break frequency rather than
        // linearly through `a`: `a` swept linearly leaves the break
        // frequency pinned near Nyquist across nearly the whole knob (a
        // linear-in-`a` sweep is a sweep in tan-space, which is nearly flat
        // away from its own asymptotes), so a low tone barely rotates until
        // the last tenth of the travel -- measured, at 220 Hz the first
        // three quarters moved the blended output by 0.08% combined.
        // Sweeping the break frequency itself geometrically (equal knob
        // steps, equal ratio of frequency, same convention as every other
        // ExpMapCompute-mapped knob in this file) from the +-0.98 margin's
        // near-Nyquist endpoint down to its near-DC endpoint, with the knob
        // itself pre-warped by a square root so the low end of that
        // sweep -- where a bass note's own phase actually moves -- gets
        // more than a sliver of the travel, spreads the audible action
        // across the whole knob instead of concentrating it at one end.
        // Both endpoints are unchanged from before (phaseKnob01 == 0 ->
        // a == -0.98, phaseKnob01 == 1 -> a == 0.98), so the pole margin
        // that keeps the allpass strictly inside the unit circle is
        // preserved exactly, just reached along a different path.
        const float knobWarped = std::sqrt(phaseKnob01);
        const float breakFreqRatio = phaseBreakFreqAtPositiveMargin / phaseBreakFreqAtNegativeMargin;
        const float breakFreq = phaseBreakFreqAtNegativeMargin * std::pow(breakFreqRatio, knobWarped);
        const float aTarget = CoeffFromBreakFreq(breakFreq);
        // Smooth the coefficient itself, not the knob input --
        // see class header comment for the measurement that picked this glide.
        const float a = coeffSmoother.Process(aTarget);
        const float phased = -a * wet + allpassX1 + a * allpassY1;
        allpassX1 = wet;
        allpassY1 = phased;
        // Equal-power crossfade, not linear: a linear
        // `dry*(1-blend) + phased*blend` only holds level when the two legs
        // are correlated, and here they are not -- the wet path's
        // fundamental is partly ANTI-correlated with dry, with a sign that
        // flips across the Drive knob (measured correlation of the wet
        // render against dry: -0.22 at Gain 0.25, -0.35 at Gain 0.50, +0.02
        // at Gain 0.75, -0.085 at Gain 1.00), so no fixed polarity and no
        // fixed Phase setting removes the resulting notch. cos/sin quadrant
        // weights preserve power regardless of that correlation and measure
        // much flatter across the same travel: worst dip anywhere across
        // 110/220/440/880 Hz and Gain 0.25/0.50/0.75/1.00, Phase at its 0.86
        // default, is -4.10 dB for the linear law above versus -1.09 dB for
        // this one (0.00 dB at 11 of the 16 points).
        //
        // Both ends are special-cased rather than left to std::cos/std::sin:
        // blendKnob01 == 0 has to return `dry` bit-for-bit (an existing pin
        // depends on it) and blendKnob01 == 1 has to reach `phased` with no
        // dry leakage (this page's Wet/Dry, unlike Delay's and Reverb's, is
        // deliberately uncapped). Checked directly rather than assumed:
        // theta == 0 lands std::cos/std::sin on exactly 1.0f/0.0f here, but
        // theta == pi/2 does not land std::cos on exactly 0.0f (it measures
        // -4.37e-8f), which would otherwise leak a trace of dry into a
        // nominally fully-wet output.
        float blended;
        if (blendKnob01 <= 0.0f)
        {
            blended = dry;
        }
        else if (blendKnob01 >= 1.0f)
        {
            blended = phased;
        }
        else
        {
            // Same law the floored pages use (dsp::EqualPowerWetDry,
            // Limiter.hpp); this page passes a zero floor, which is the whole
            // of the difference between them. The two endpoints above stay
            // exact cases here rather than going through the helper, because
            // an uncapped travel reaches pi/2, where std::cos does not land
            // on an exact 0.0f.
            const WetDryGains gains = EqualPowerWetDry(blendKnob01, 0.0f);
            blended = dry * gains.dry + phased * gains.wet;
        }
        // Catches the residual smoothing alone cannot close.
        return outputLimiter.Process(blended);
    }

    // (Per-unit recovery, app/FroggersAppCore.hpp): zeros only the
    // allpass's own recursive history plus the two new stateful units this
    // fix adds (coeffSmoother/outputLimiter), each reset to the same
    // quiescent value their own type's Reset()/constructor already defines
    // as "no history" -- the coefficient smoother back to the default-knob
    // seed above, the limiter's envelope back to unity (OutputLimiter::Reset()).
    void Reset()
    {
        allpassX1 = 0.0f;
        allpassY1 = 0.0f;
        coeffSmoother.output = -0.98f;
        outputLimiter.Reset();
        overCeilingSeconds = 0.0f;
    }

    bool StateFinite() const
    {
        return std::isfinite(allpassX1) && std::isfinite(allpassY1) && std::isfinite(coeffSmoother.output) &&
               outputLimiter.StateFinite();
    }
    float StateMagnitude() const
    {
        return std::max(std::max(std::fabs(allpassX1), std::fabs(allpassY1)), std::fabs(coeffSmoother.output));
    }
};

}  // namespace synth_froggers::dsp
