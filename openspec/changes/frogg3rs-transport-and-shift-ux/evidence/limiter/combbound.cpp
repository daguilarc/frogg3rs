// Settles whether the comb branch obeys `|comb| <= A + |fb|` (FilterFxChain::Process's
// comb-trim comment) at every Comb drive, by running the shipped dsp::Comb from
// the worktree's app/dsp unchanged.
//
// Grid: integer delay N = 100 samples (comb pitch 480 Hz at 48 kHz), comb
// low-pass fully open (alpha 1.0, so the loop's filter passes the tap
// unchanged), feedback +0.95 (the maximum GetFeedback returns), Comb drive
// k in {0.25 (knob 0), 0.5, 1.0 (knob centre), 2.0, 4.0 (knob top)}, input a
// sine at the comb's own pitch with amplitude A in {0.25, 1.0}, 2 s of input
// then 1 s of silence. Tap: Comb::Process's return (combRaw), and the
// fed-back term `output - input`.

#include "dsp/FilterFx.hpp"

#include <cmath>
#include <cstdio>

int main() {
    namespace dsp = synth_froggers::dsp;
    constexpr int kN = 100;
    constexpr int kSr = 48000;
    constexpr float kFb = 0.95f;
    std::printf("%6s %5s | %10s %10s | %12s | %12s | %s\n", "drive", "A", "max|comb|", "A+|fb|", "max|fedback|",
                "max|tail|", "bound holds");
    for (float drive : {0.25f, 0.5f, 1.0f, 2.0f, 4.0f}) {
        for (float amp : {0.25f, 1.0f}) {
            dsp::Comb comb;
            comb.delaySamples = static_cast<float>(kN);
            comb.SetFeedback(kFb);
            comb.SetCutoffAlpha(1.0f);
            comb.SetDrive(drive);
            float maxOut = 0.0f, maxFed = 0.0f, maxTail = 0.0f;
            for (int i = 0; i < 3 * kSr; ++i) {
                const float in = i < 2 * kSr ? amp * std::sin(2.0f * 3.14159265f * static_cast<float>(i % kN) / kN) : 0.0f;
                const float out = comb.Process(in);
                maxOut = std::max(maxOut, std::fabs(out));
                maxFed = std::max(maxFed, std::fabs(out - in));
                if (i >= 2 * kSr) maxTail = std::max(maxTail, std::fabs(out));
            }
            const float bound = amp + kFb;
            std::printf("%6.2f %5.2f | %10.4f %10.4f | %12.4f | %12.4f | %s\n", drive, amp, maxOut, bound, maxFed, maxTail,
                        maxOut <= bound ? "yes" : "NO");
        }
    }
    return 0;
}
