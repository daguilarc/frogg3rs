#pragma once

// synth_froggers::dsp -- the stereo-field primitives `dsp::StereoDelay`
// (dsp/Delay.hpp) and `dsp::Reverb` (dsp/Reverb.hpp) both build on:
//
//   - CrossFeedPair: one weighted cross-feed shared by
//     `dsp::StereoDelay::Process` and `dsp::Reverb::Process`. Both stages
//     compute the same weighted average of two channel reads and its
//     mirror -- `a*(1-cross) + b*cross` and `b*(1-cross) + a*cross`.
//   - SchroederAllpassSection and DelayDiffuser: the per-channel allpass
//     cascade `dsp::StereoDelay`'s own Diffusion control drives (moved here
//     from dsp/Delay.hpp so this header does not depend back on it -- see
//     each struct's own comment below).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace synth_froggers::dsp {

// The crossed pair `CrossFeedPair` returns. Generic field names (`a`/`b`,
// not `l`/`r`) because a caller's two arguments are not always a left/right
// pair -- `dsp::Reverb::Process` passes its two tank-line reads, and reaches
// its own pre-existing zero-weight behavior only by passing them transposed
// (see that call site's own comment, dsp/Reverb.hpp).
struct CrossedPair
{
    float a;
    float b;
};

// `cross` is expected in [0, 0.5]: 0 leaves both arguments unchanged
// (`{a, b}`, an exact identity -- the zero multiply guarantees it rather
// than relying on the arithmetic to happen to land there), and 0.5 mixes
// them in equal measure. Which physical signal a caller passes as `a`
// versus `b` decides whether zero reads as identity or as a full swap --
// see each call site's own comment.
inline CrossedPair CrossFeedPair(float a, float b, float cross)
{
    return {a * (1.0f - cross) + b * cross, b * (1.0f - cross) + a * cross};
}

// (Diffusion, Delay slot 8, dsp::DelayParams::ddif's own comment,
// dsp/Delay.hpp): one Schroeder allpass section with an
// M-sample delay, M configured at runtime (up to `Capacity` samples, fixed
// at compile time so the audio path never allocates).
//
// Recurrence and coefficient-sign convention are the SAME as
// DriveBlendPhase's one-sample allpass (Drive.hpp:
// `phased = -a*wet + allpassX1 + a*allpassY1; allpassX1 = wet;
// allpassY1 = phased;`), generalized from one-sample registers
// (allpassX1/allpassY1) to M-sample circular buffers (xHistory/yHistory
// below) -- reuse the RECURRENCE FORM, not the one-sample STATE: a
// straight copy of DriveBlendPhase's one-
// sample memory produces frequency-dependent phase rotation (a phaser),
// not time smearing, so diffusion needs real per-section delay instead.
template <std::size_t Capacity>
struct SchroederAllpassSection
{
    // xHistory[pos]/yHistory[pos] hold x[n-M]/y[n-M] the instant Process()
    // reads them (read-before-write, the same ring-buffer idiom
    // StereoDelay's own ReadAt/WriteSample pair uses on lineL/lineR in
    // dsp/Delay.hpp, just backed by a fixed-capacity array instead of a
    // heap vector).
    std::array<float, Capacity> xHistory{};
    std::array<float, Capacity> yHistory{};
    std::size_t pos = 0;
    std::size_t m = 1;  // current delay length in samples; set by Configure(), clamped to [1, Capacity].

    // Recomputes M from a base delay time (seconds) and a sample rate --
    // called only from DelayDiffuser::SetSampleRate (never per-sample), so
    // the rounding/clamp here are not audio-path cost. Also clears this
    // section's history: a rate change invalidates old buffer alignment
    // either way, matching StereoDelay::SetSampleRate's own lineL/lineR
    // re-assignment in dsp/Delay.hpp.
    void Configure(float sampleRateHz, float baseSeconds)
    {
        long long rounded = std::lround(static_cast<double>(baseSeconds) * static_cast<double>(sampleRateHz));
        if (rounded < 1)
        {
            rounded = 1;
        }
        if (rounded > static_cast<long long>(Capacity))
        {
            // Defensive only -- this unit is sized for sample rates up to
            // 192kHz (see DelayDiffuser's own capacity comments below);
            // clamping here keeps every buffer access in-bounds even if a
            // caller supplies more, rather than relying on that ceiling
            // never being crossed.
            rounded = static_cast<long long>(Capacity);
        }
        m = static_cast<std::size_t>(rounded);
        Reset();
    }

    // y[n] = -a*x[n] + x[n-M] + a*y[n-M] -- Drive.hpp's recurrence,
    // M-sample memory instead of one-sample (class header comment above).
    float Process(float x, float a)
    {
        const float xDelayed = xHistory[pos];
        const float yDelayed = yHistory[pos];
        const float y = -a * x + xDelayed + a * yDelayed;
        xHistory[pos] = x;
        yHistory[pos] = y;
        pos = (pos + 1 >= m) ? 0 : pos + 1;
        return y;
    }

    void Reset()
    {
        std::fill(xHistory.begin(), xHistory.end(), 0.0f);
        std::fill(yHistory.begin(), yHistory.end(), 0.0f);
        pos = 0;
    }

    bool StateFinite() const
    {
        for (const float v : xHistory)
        {
            if (!std::isfinite(v))
            {
                return false;
            }
        }
        for (const float v : yHistory)
        {
            if (!std::isfinite(v))
            {
                return false;
            }
        }
        return true;
    }

    float StateMagnitude() const
    {
        float magnitude = 0.0f;
        for (const float v : xHistory)
        {
            magnitude = std::max(magnitude, std::fabs(v));
        }
        for (const float v : yHistory)
        {
            magnitude = std::max(magnitude, std::fabs(v));
        }
        return magnitude;
    }
};

// One channel's diffuser -- three SchroederAllpassSection cascades
// at non-harmonic base times (4.7ms/12.3ms/21.1ms) so the sections do not
// reinforce one another. One instance per
// channel (dsp::StereoDelay::diffuserL/diffuserR, dsp/Delay.hpp), matching
// that struct's own per-channel-instance idiom (wetLimiterL/R, fbToneL/R,
// crushL/R) rather than one shared instance driven by e.g. max(|L|,|R|).
struct DelayDiffuser
{
    static constexpr float kSection1BaseSeconds = 0.0047f;  // 4.7 ms.
    static constexpr float kSection2BaseSeconds = 0.0123f;  // 12.3 ms.
    static constexpr float kSection3BaseSeconds = 0.0211f;  // 21.1 ms.

    // Capacities sized to each section's OWN 192kHz maximum, not all to the
    // longest section's.
    static constexpr std::size_t kSection1Capacity = 1024;  // 4.7ms@192kHz = 902.4 samples; headroom to a round cap.
    static constexpr std::size_t kSection2Capacity = 2560;  // 12.3ms@192kHz = 2361.6 samples; own maximum, not 4096.
    static constexpr std::size_t kSection3Capacity =
        4096;  // 21.1ms@192kHz ~= 4051.2 samples; a safe cap for the longest section.

    SchroederAllpassSection<kSection1Capacity> section1;
    SchroederAllpassSection<kSection2Capacity> section2;
    SchroederAllpassSection<kSection3Capacity> section3;

    void SetSampleRate(float sampleRateHz)
    {
        section1.Configure(sampleRateHz, kSection1BaseSeconds);
        section2.Configure(sampleRateHz, kSection2BaseSeconds);
        section3.Configure(sampleRateHz, kSection3BaseSeconds);
    }

    // Cascaded: section1's output feeds section2's input feeds section3's
    // input, all three driven by the SAME coefficient `a`
    // (dsp::StereoDelay's own ddif*kDiffusionCoeffScale mapping computes `a`
    // once per sample -- see StereoDelay::ApplyDiffusion, dsp/Delay.hpp).
    float Process(float x, float a)
    {
        return section3.Process(section2.Process(section1.Process(x, a), a), a);
    }

    void Reset()
    {
        section1.Reset();
        section2.Reset();
        section3.Reset();
    }

    bool StateFinite() const
    {
        return section1.StateFinite() && section2.StateFinite() && section3.StateFinite();
    }

    float StateMagnitude() const
    {
        return std::max(section1.StateMagnitude(), std::max(section2.StateMagnitude(), section3.StateMagnitude()));
    }
};

}  // namespace synth_froggers::dsp
