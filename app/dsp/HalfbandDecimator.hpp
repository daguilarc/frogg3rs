#pragma once

#include <array>
#include <cstddef>

namespace synth_froggers::dsp {

// Fixed 47-tap linear-phase halfband, processing one ordered pair per output.
// Remez bands [0,.4,.6,1], desired [1,0], Nyquist=1; odd taps are zero
// except the .5 center, and the even taps sum to .5. Symmetric pairs avoid
// multiplying zeros or calculating samples discarded by 2:1 decimation.
struct HalfbandDecimator2x {
    static constexpr int kTaps = 47;
    inline static constexpr std::array<float, 12> kEvenCoefficients{
        -0.000197734612849f, 0.00057653207159f, -0.00135168859744f,
        0.00272882921695f, -0.00498790476891f, 0.00849969769225f,
        -0.0137894554475f, 0.0217155600507f, -0.0339845498113f,
        0.0549533148596f, -0.100673935111f, 0.316511334457f
    };
    std::array<float, kTaps> history{};
    int writeIndex = 0;

    void Reset() {
        history.fill(0.0f);
        writeIndex = 0;
    }

    float Process(float first, float second) {
        history[writeIndex] = first;
        if (++writeIndex == kTaps) writeIndex = 0;
        history[writeIndex] = second;
        const int newest = writeIndex;
        if (++writeIndex == kTaps) writeIndex = 0;
        const auto atLag = [&](int lag) {
            int index = newest - lag;
            if (index < 0) index += kTaps;
            return history[static_cast<std::size_t>(index)];
        };
        float output = .5f * atLag(23);
        for (int pair = 0; pair < 12; ++pair) {
            output += kEvenCoefficients[static_cast<std::size_t>(pair)] *
                      (atLag(2 * pair) + atLag(46 - 2 * pair));
        }
        return output;
    }
};

} // namespace synth_froggers::dsp
