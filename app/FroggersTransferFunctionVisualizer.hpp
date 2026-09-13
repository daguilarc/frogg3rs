#pragma once

// synth_froggers::TransferFunctionVisualizer -- one
// Visualizer sampling any synth::TransferFunction's FrequencyResponse()
// into a DrawCommand::Polyline (External/Sheaf/projects/synth/include/synth/PortableUI.hpp:119), log-frequency
// axis. Generic over EITHER ResonantBump::UIState or Comb::UIState
// (app/dsp/FilterFx.hpp) -- both derive from synth::TransferFunction,
// so one Visualizer implementation covers both the bump's peak/notch and
// the comb's periodic response.
//
// A self-oscillating comb's feedback can drive the displayed magnitude
// arbitrarily high, so it must stay bounded rather than drawn out of frame:
// FilterFx.hpp's closed forms are
// already finite by construction (SafeDenominator floors the
// denominator's magnitude), but a finite-and-huge value could still land
// far outside this Visualizer's own drawing bounds. Plotting in dB and
// clamping to a fixed [`minDb_`, `maxDb_`] window (-40 dB to 40 dB by
// default) before mapping to pixel-y is what keeps the polyline inside
// GetBounds() regardless of how extreme the underlying (finite) response
// gets.

#include "synth/DspTransferFunction.hpp"
#include "synth/Color.hpp"
#include "synth/PortableUI.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace synth_froggers {

class TransferFunctionVisualizer final : public synth::ui::Visualizer {
public:
    // minFreqNormalized/maxFreqNormalized are in cycles/sample (the same
    // convention ResonantBump::freq / Comb's delay-derived frequency
    // already use elsewhere in this app, e.g. FroggersApp::RouteAudioSample's
    // ExpMapCompute(20/sr, 20000/sr, ...) calls). Defaults span from a
    // generic near-DC anchor to Nyquist so this class needs no sample-rate
    // dependency of its own.
    TransferFunctionVisualizer(const synth::TransferFunction& transferFunction, synth::Color color,
                                float minFreqNormalized = 1.0e-4f, float maxFreqNormalized = 0.5f,
                                std::size_t numSamples = 128, float minDb = -40.0f, float maxDb = 40.0f)
        : transferFunction_(&transferFunction),
          color_(color),
          minFreqNormalized_(minFreqNormalized),
          maxFreqNormalized_(maxFreqNormalized),
          numSamples_(numSamples),
          minDb_(minDb),
          maxDb_(maxDb) {}

protected:
    std::vector<synth::ui::DrawCommand> DrawVisible() const override {
        const synth::ui::Bounds bounds = GetBounds();
        const float logMin = std::log10(std::max(minFreqNormalized_, 1.0e-6f));
        const float logMax = std::log10(std::max(maxFreqNormalized_, minFreqNormalized_ * 2.0f));

        std::vector<synth::ui::Point> points;
        points.reserve(numSamples_);
        for (std::size_t i = 0; i < numSamples_; ++i) {
            const float t =
                numSamples_ > 1 ? static_cast<float>(i) / static_cast<float>(numSamples_ - 1) : 0.0f;
            const float logFreq = logMin + (logMax - logMin) * t;
            const float freqNormalized = std::pow(10.0f, logFreq);
            const float magnitude = transferFunction_->FrequencyResponse(freqNormalized);
            // Belt-and-suspenders: FilterFx.hpp's closed forms are already
            // finite by construction (SafeDenominator), but this Visualizer
            // must never plot a NaN/Inf regardless of which TransferFunction
            // it is pointed at.
            const float safeMagnitude =
                std::isfinite(magnitude) ? std::max(magnitude, 1.0e-6f) : 1.0e-6f;
            const float db = 20.0f * std::log10(safeMagnitude);
            const float clampedDb = std::clamp(db, minDb_, maxDb_);
            const float yFrac = (clampedDb - minDb_) / (maxDb_ - minDb_);

            const float x = bounds.x + bounds.width * t;
            const float y = bounds.y + bounds.height * (1.0f - yFrac);
            points.push_back({x, y});
        }

        std::vector<synth::ui::DrawCommand> commands;
        commands.push_back(synth::ui::DrawCommand::Polyline(points, color_, 1.4f));
        return commands;
    }

private:
    const synth::TransferFunction* transferFunction_;
    synth::Color color_;
    float minFreqNormalized_;
    float maxFreqNormalized_;
    std::size_t numSamples_;
    float minDb_;
    float maxDb_;
};

}  // namespace synth_froggers
