#pragma once

// synth_froggers::dsp -- shared math substrate for the DSP port. This
// header is a **copy**, not an include, of four small firmware-tree utility
// headers that
// several ported units (Vco.hpp, RandomShLane.hpp, FilterFx.hpp) all need.
// It must never #include anything under src/ -- see
// check_no_firmware_includes.sh, which enforces that mechanically for every
// file under app/.
//
// Sources copied (read directly, not from memory):
//   - src/core/SDDSine.hpp              (Sine01, below)
//   - src/core/PhaseUtils.hpp:49-52,87-90 (ExpMapCompute, ZeroedExpCompute)
//   - src/core/OPLowPassFilter.hpp      (OnePoleLowPass)
//   - src/core/BiquadSection.hpp:30-38  (BiquadDf1::Process only; the port
//     needs no highpass path, so SetCoefficients's isHighPass branch is not
//     carried -- ResonantBump only ever uses the peaking-EQ coefficient math
//     in FilterFx.hpp, computed there directly)

#include <algorithm>
#include <cassert>
#include <cmath>

namespace synth_froggers::dsp {

// One sample of a two-channel signal. The delay and the reverb both compute a
// pair internally and used to sum it on the next line; carrying this instead
// is what lets a stereo image reach the device, and it is the same shape both
// stages already had, not a new one.
struct StereoSample
{
    float l = 0.0f;
    float r = 0.0f;
};

// -- src/core/SDDSine.hpp, verbatim formula -------------------------------
inline float Sine01(float phase)
{
    const float wrappedPhase = phase - std::floor(phase);
    constexpr float kTwoPi = 6.28318530717958647692f;
    return std::sin(kTwoPi * wrappedPhase);
}

// -- 08b5fd3:src/core/FroggersEngine.hpp:177-180 (WrapPhase) --------------
inline float WrapPhase(float p)
{
    return p - std::floor(p);
}

// -- src/core/PhaseUtils.hpp:49-52 (ExpParam::Compute) --------------------
// min * (max/min)^value
inline float ExpMapCompute(float min, float max, float value)
{
    return min * std::pow(max / min, value);
}

// -- src/core/PhaseUtils.hpp:87-90 (ZeroedExpParam::Compute) --------------
// (base^value - 1) / (base - 1)
inline float ZeroedExpCompute(float base, float value)
{
    return (std::pow(base, value) - 1.0f) / (base - 1.0f);
}

// -- 08b5fd3:src/core/FroggersEngine.hpp:150-165 (Vco::PmDepthScale), generalized -
// true-zero depth taper: exactly 0 at/below `floor`, a smoothstep
// t*t*(3-2t) ramp up to 1 across `rampWidth` above `floor`, then 1. Shared
// by any knob that needs PmDepthScale's exact shape with its own floor/
// ramp width (see Vco::PmDepthScale, which now calls this with
// kPmLfoFloor/kPmLfoRampWidth).
inline float TrueZeroDepthTaper(float knob01, float floor, float rampWidth)
{
    if (knob01 <= floor)
    {
        return 0.0f;
    }
    const float rampTop = floor + rampWidth;
    if (knob01 >= rampTop)
    {
        return 1.0f;
    }
    const float t = (knob01 - floor) / rampWidth;
    return t * t * (3.0f - 2.0f * t);
}

// -- src/core/OPLowPassFilter.hpp, verbatim formula -----------------------
struct OnePoleLowPass
{
    static constexpr float kMaxCutoff = 0.499f;

    float alpha = 0.0f;
    float output = 0.0f;

    float Process(float input)
    {
        output = alpha * input + (1.0f - alpha) * output;
        return output;
    }

    void SetAlphaFromNatFreq(float cyclesPerSample)
    {
        cyclesPerSample = std::min(kMaxCutoff, cyclesPerSample);
        assert(cyclesPerSample > 0.0f);
        constexpr float kTwoPi = 6.28318530717958647692f;
        alpha = 1.0f - std::exp(-kTwoPi * cyclesPerSample);
    }
};

// The knob-to-coefficient map every TONE control shares: a post-stage
// low-pass whose knob top is exact bypass. Two controls use it -- the Drive
// bank's Tone (FrogBlock::SetTone) and the Delay bank's Feedback tone
// (StereoDelay::SetFeedbackTone) -- and they are the same control in two
// places, not two ranges that happen to agree, so this is the one definition
// site rather than the same expression written twice.
//
// Knob 1.0 gives alpha exactly 1.0, which makes OnePoleLowPass::Process an
// exact identity (output = 1.0*input + 0.0*output). A tone control at its
// default has to remove nothing exactly, not almost.
//
// Knob 0.0 gives 0.1, a cutoff near 805 Hz at 48kHz. The floor is not there
// to bound the filter but to bound the DRAW: randomization takes each
// parameter uniformly across its travel while this mapping is geometric, so
// half of all draws land below the range's geometric mean. At the 0.02 both
// controls were authored with, that mean was 0.141 -- about 1165 Hz -- and
// the darkest reachable setting was 154 Hz, which is a mute rather than a
// tone. One decade puts the median draw near 2.9 kHz instead.
//
// Reverb's damping filter deliberately does NOT use this: its range is
// (0.02, 0.2) and its knob is inverted, because it darkens a tail rather
// than shaping a signal and never fully opens. Same filter, different
// control.
inline float ToneAlphaFromKnob(float toneKnob01)
{
    return ExpMapCompute(0.1f, 1.0f, toneKnob01);
}

// -- src/core/BiquadSection.hpp:30-38 (Process only, Direct Form 1) -------
struct BiquadDf1
{
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float x1 = 0.0f;
    float x2 = 0.0f;
    float y1 = 0.0f;
    float y2 = 0.0f;

    float Process(float input)
    {
        const float out = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = input;
        y2 = y1;
        y1 = out;
        return out;
    }
};

}  // namespace synth_froggers::dsp
