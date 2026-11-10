// FroggersVstSmokeTest.cpp -- headless processBlock smoke test,
// CTest-wired via app/vst/CMakeLists.txt's add_test(). Constructs
// FroggersPluginProcessor directly (no host, no editor, no JUCE app/window),
// calls prepareToPlay(), then drives the exact seam
// FroggersUiSurface::HandleAction's Play/Stop buttons use
// (TestStartTransport()/TestStopTransport(), see FroggersPluginProcessor.hpp's
// header comment) -- the same synth::MessageIn::Start/Stop +
// SetDesiredTransportRunning pair SynthRig::StartAt/StopAt push for the app
// core's own test suite.
//
// A positive control: asserts output is (near-)silent BEFORE the
// transport is ever started, THEN asserts it is clearly non-silent after --
// so "nonzero after" cannot be trivially true (e.g. a broken gate that is
// simply always open). kSilenceFloorLinear below reuses the exact constant
// app/FroggersAudioRoutingTests.cpp already established for "silence" in
// this app (-60 dBFS, that file's ComputeSilenceSettleWindow/
// kSilenceFloorLinear).

#include "FroggersPluginProcessor.hpp"

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

namespace {

synth::RuntimeDataPaths ScratchDataPaths() {
    const std::filesystem::path dataRoot =
        std::filesystem::temp_directory_path() / "froggers-vst-smoke-test";
    std::filesystem::remove_all(dataRoot);
    synth::RuntimeDataPaths paths = synth::RuntimeDataPaths::FromDataRoot(dataRoot);
    std::filesystem::create_directories(paths.patchesRoot);
    return paths;
}

// Runs `blocks` blocks of silence-in/audio-out through the processor and
// returns the RMS of everything it wrote, across both output channels.
double RunBlocksAndMeasureRms(frogg3rs_vst::FroggersPluginProcessor& processor, int blockSize, std::size_t blocks) {
    double sumSquares = 0.0;
    std::size_t sampleCount = 0;

    for (std::size_t i = 0; i < blocks; ++i) {
        juce::AudioBuffer<float> buffer(2, blockSize);
        buffer.clear();
        juce::MidiBuffer midi;

        processor.processBlock(buffer, midi);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            const float* samples = buffer.getReadPointer(ch);
            for (int s = 0; s < buffer.getNumSamples(); ++s) {
                if (!std::isfinite(samples[s])) {
                    std::fprintf(stderr, "FroggersVstSmokeTest: non-finite sample at block %zu channel %d sample %d\n",
                                 i, ch, s);
                    std::exit(1);
                }
                sumSquares += static_cast<double>(samples[s]) * static_cast<double>(samples[s]);
                ++sampleCount;
            }
        }
    }

    return sampleCount > 0 ? std::sqrt(sumSquares / static_cast<double>(sampleCount)) : 0.0;
}

}  // namespace

int main() {
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;
    // Same constant app/FroggersAudioRoutingTests.cpp uses for "silence"
    // (-60 dBFS) -- see this file's own header comment.
    constexpr double kSilenceFloorLinear = 1.0e-3;

    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths());
    processor.setRateAndBufferSizeDetails(kSampleRate, kBlockSize);
    processor.prepareToPlay(kSampleRate, kBlockSize);
    // The 287-tap output FIR delays 143 engine samples: 35.75 host
    // frames at 4x. Report the next whole frame to the DAW.
    if (processor.getLatencySamples() != 36) {
        std::fprintf(stderr, "FroggersVstSmokeTest: FAIL reported latency %d, expected 36\n",
                     processor.getLatencySamples());
        return 1;
    }


    // Pre-start: the transport-gated ASR (FroggersAppCore.hpp) never opened,
    // so output must stay at/near silence -- ~0.5s, comfortably past any
    // startup transient.
    const double preStartRms = RunBlocksAndMeasureRms(processor, kBlockSize, /*blocks=*/94);
    if (preStartRms >= kSilenceFloorLinear) {
        std::fprintf(stderr,
                     "FroggersVstSmokeTest: FAIL pre-start RMS %.6f is not below the silence floor %.6f "
                     "(positive control failed -- transport gate may be open by default)\n",
                     preStartRms, kSilenceFloorLinear);
        return 1;
    }

    processor.TestStartTransport();

    // Post-start: ~1s so envelope/ADSR ramps and the default patch settle
    // into clearly audible steady state.
    const double postStartRms = RunBlocksAndMeasureRms(processor, kBlockSize, /*blocks=*/188);
    if (postStartRms < kSilenceFloorLinear) {
        std::fprintf(stderr,
                     "FroggersVstSmokeTest: FAIL post-start RMS %.6f did not clear the silence floor %.6f\n",
                     postStartRms, kSilenceFloorLinear);
        return 1;
    }

    processor.TestStopTransport();
    processor.releaseResources();

    std::fprintf(stderr, "FroggersVstSmokeTest: OK pre-start RMS=%.6f post-start RMS=%.6f\n", preStartRms,
                 postStartRms);
    return 0;
}
