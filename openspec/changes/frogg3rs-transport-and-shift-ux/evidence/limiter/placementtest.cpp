// Prototype of the proposed placement test, built against cur/ (shipped
// placement) and moved/ (limiter after the blend) to show it is red on the
// first and green on the second.
#include "dsp/FilterFx.hpp"
#include <cmath>
#include <cstdio>
int main() {
    namespace dsp = synth_froggers::dsp;
    constexpr float kSr = 48000.0f;
    constexpr int kN = 100;
    auto setup = [&](dsp::FilterFxChain& c) {
        c.Configure(kSr);
        c.pureDelay.delaySamples = 1.0f;
        c.comb.delaySamples = static_cast<float>(kN);
        c.comb.SetFeedback(0.95f);
        c.comb.SetCutoffAlpha(1.0f);
        c.comb.SetDrive(0.25f);
        // The Filter bank's registered defaults, as RouteFilterBank maps them.
        c.peak.SetFreq(100.0f / kSr);
        c.peak.SetWidth(0.4f);
        c.peak.SetHeight(1.0f);
        c.scoopNotch.SetFreq(100.0f / kSr);
        c.scoopNotch.SetWidth(0.4f);
        c.scoopNotch.SetHeight(1.0f);
    };
    dsp::FilterFxChain shipped, open;
    setup(shipped);
    setup(open);
    open.peakLimiter.Configure(kSr, 1.0e6f, 2.0e6f, dsp::kPeakLimiterAttackSeconds, dsp::kPeakLimiterReleaseSeconds);
    dsp::OutputLimiter ref;
    ref.Configure(kSr, dsp::kPeakLimiterThreshold, dsp::kPeakLimiterCeiling, dsp::kPeakLimiterAttackSeconds,
                  dsp::kPeakLimiterReleaseSeconds);
    float openPeak = 0, shippedPeak = 0, refEnvMin = 1, shippedEnvMin = 1;
    int mismatches = 0;
    for (int i = 0; i < 2 * static_cast<int>(kSr); ++i) {
        const float in = 0.25f * std::sin(2.0f * 3.14159265f * static_cast<float>(i % kN) / kN);
        const float a = shipped.Process(in, 0.0f, 1.0f, 0.0f);
        const float b = open.Process(in, 0.0f, 1.0f, 0.0f);
        const float r = ref.Process(b);
        openPeak = std::fmax(openPeak, std::fabs(b));
        shippedPeak = std::fmax(shippedPeak, std::fabs(a));
        refEnvMin = std::fmin(refEnvMin, ref.envelope);
        shippedEnvMin = std::fmin(shippedEnvMin, shipped.peakLimiter.envelope);
        if (a != r) ++mismatches;
    }
    std::printf("unlimited peak %.4f (threshold %.2f) | shipped peak %.4f | reference envelope min %.4f | shipped limiter envelope min %.4f | samples where shipped != limiter(blend) %d of %d -> %s\n",
                openPeak, dsp::kPeakLimiterThreshold, shippedPeak, refEnvMin, shippedEnvMin, mismatches, 2 * static_cast<int>(kSr),
                (openPeak > dsp::kPeakLimiterThreshold && mismatches == 0 && shippedPeak < openPeak) ? "GREEN" : "RED");
    return 0;
}
