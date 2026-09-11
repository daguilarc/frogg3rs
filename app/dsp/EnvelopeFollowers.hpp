#pragma once

// synth_froggers::dsp::VcoEnvelopeFollowers -- a **copy** of the cited
// Froggers formula.
//
// Ported from the retired simulator's V2EnvelopeFollowerBank.hpp:
//   - attack 0.01 s / release 0.05 s, coeff = 1 - exp(-1/(t*sr))  (:22-25)
//   - the five per-block targets                                  (:30-35)
//
// Only taps 3, 4, 5 (|v1|, |v2|, |v3| -- array slots 0-2 of the frozen
// bank's 5-wide kNumTaps) are ported: those are the ones that feed the
// modulation slate (slots 9-11, "VCO 1/2/3 envelope follower"). Taps 6-7 in
// the frozen bank (targets[3] = |v1+v2|*0.5, targets[4] = |v2+v3|*0.5, the
// two PAIR envelope followers) are superseded by the modulation slate and
// are deliberately NOT ported -- porting them would create modulation
// sources nothing in this app consumes.

#include <algorithm>
#include <cmath>

namespace synth_froggers::dsp {

struct VcoEnvelopeFollowers
{
    static constexpr int kNumTaps = 3;

    float levels[kNumTaps]{};
    float attackCoeff = 0.05f;
    float releaseCoeff = 0.01f;

    // The retired simulator's f236915^:sim/V2EnvelopeFollowerBank.hpp:19-26 (setSampleRate). This struct's
    // only production caller is FroggersModulationSlate::Prepare(), itself
    // only ever called from FroggersAppCore::PrepareToPlay(), which
    // validates the host's sample rate ONCE before any downstream use --
    // so `sampleRate` here is always already positive; a defensive
    // `44100.0f` fallback would be unreachable, which is why none is
    // present.
    void SetSampleRate(float sampleRate)
    {
        constexpr float kAttackSeconds = 0.01f;
        constexpr float kReleaseSeconds = 0.05f;
        attackCoeff = 1.0f - std::exp(-1.0f / (kAttackSeconds * sampleRate));
        releaseCoeff = 1.0f - std::exp(-1.0f / (kReleaseSeconds * sampleRate));
    }

    // The retired simulator's f236915^:sim/V2EnvelopeFollowerBank.hpp:28-45 (Process), restricted to taps
    // 3-5 (|v1|, |v2|, |v3|); the pair-sum taps 6-7 are not computed at all.
    // Returns {|v1| follower, |v2| follower, |v3| follower}.
    void Process(float v1, float v2, float v3, float (&out)[kNumTaps])
    {
        const float targets[kNumTaps] = {std::fabs(v1), std::fabs(v2), std::fabs(v3)};
        for (int i = 0; i < kNumTaps; ++i)
        {
            const float target = std::min(std::max(targets[i], 0.0f), 1.0f);
            const float coeff = (target > levels[i]) ? attackCoeff : releaseCoeff;
            levels[i] += (target - levels[i]) * coeff;
            out[i] = levels[i];
        }
    }
};

// Feeds the "external audio envelope follower" modulation source (slot 14).
// The external-audio source is a single channel, so it needs exactly one of
// VcoEnvelopeFollowers's three identical per-tap formulas, not all three --
// this is that same formula (the retired simulator's f236915^:sim/V2EnvelopeFollowerBank.hpp:19-35),
// generalized to one channel instead of duplicating VcoEnvelopeFollowers's
// 3-wide array for a single tap or wastefully feeding one signal into all
// three of its lanes.
struct SingleEnvelopeFollower
{
    float level = 0.0f;
    float attackCoeff = 0.05f;
    float releaseCoeff = 0.01f;

    // Same single-caller chain as VcoEnvelopeFollowers::SetSampleRate
    // above (this app's other production caller of this struct), rooted at
    // the same sample-rate-validating PrepareToPlay() -- a `44100.0f`
    // re-guard would be unreachable for the same reason.
    void SetSampleRate(float sampleRate)
    {
        constexpr float kAttackSeconds = 0.01f;
        constexpr float kReleaseSeconds = 0.05f;
        attackCoeff = 1.0f - std::exp(-1.0f / (kAttackSeconds * sampleRate));
        releaseCoeff = 1.0f - std::exp(-1.0f / (kReleaseSeconds * sampleRate));
    }

    float Process(float v)
    {
        const float target = std::min(std::max(std::fabs(v), 0.0f), 1.0f);
        const float coeff = (target > level) ? attackCoeff : releaseCoeff;
        level += (target - level) * coeff;
        return level;
    }
};

}  // namespace synth_froggers::dsp
