// FroggersAudioRoutingTests.cpp -- the real audio path. Proves
// FroggersApp::ProcessBlock produces non-silent, finite stereo output for
// (a) a deliberately non-default patch and (b) the default patch
// untouched, while the master clock's transport is running -- explicitly
// stronger than the finiteness check in FroggersHeadlessTests.cpp,
// which silence also satisfies -- and produces silence while the transport
// is stopped, including immediately after Init()/PrepareToPlay() with the
// transport never started (the ASR gate follows the transport's
// quarter-note pulse, not any note-on/off or permanently-held gate). Also
// exercises the output-safety requirement directly: the Filter
// bank's Comb pushed to its self-oscillating feedback extreme, plus the
// Reverb bank's Hold pushed to its ceiling, must still leave the output
// finite and bounded.
//
// Runs via synth_rig::SynthRig<FroggersApp> (External/Sheaf's
// tests/support/SynthRig.hpp), same JUCE-free headless harness as
// FroggersHeadlessTests.cpp and FroggersParameterModelTests.cpp;
// wired into app/Makefile's `test` target (nice make -j2 test). The rig's
// transport starts Stopped by default (synth::ClockTransportState::Stopped)
// until `rig.StartAt(...)` is called, which is exactly the state this file's
// silence tests rely on.

#include "Froggers.hpp"
#include "FroggersModulation.hpp"
#include "FroggersParameters.hpp"
#include "support/SynthRig.hpp"

#ifdef JUCE_MAJOR_VERSION
#error "Froggers audio-routing tests must not see JUCE headers -- the app core must stay JUCE-free"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

struct TestCase {
    const char* name;
    void (*fn)();
};

std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Register {
    Register(const char* name, void (*fn)()) {
        Registry().push_back({name, fn});
    }
};

#define TEST_CASE(name)     \
    void name();            \
    Register reg_##name(#name, &name); \
    void name()

#define REQUIRE_TRUE(expr)                                                 \
    do {                                                                   \
        if (!(expr)) {                                                    \
            std::ostringstream oss;                                       \
            oss << __FILE__ << ":" << __LINE__ << " requirement failed: " #expr; \
            throw std::runtime_error(oss.str());                          \
        }                                                                  \
    } while (false)

// Fresh scratch runtime data paths per test, mirroring
// FroggersHeadlessTests.cpp's UseScratchRuntimeDataPaths, so startup patch
// loading never observes a shared production location or another test's
// data.
synth::RuntimeDataPaths UseScratchRuntimeDataPaths(const char* testName) {
    const std::filesystem::path dataRoot =
        std::filesystem::temp_directory_path() / "froggers-audio-routing-tests" / testName;
    std::filesystem::remove_all(dataRoot);
    synth::RuntimeDataPaths paths = synth::RuntimeDataPaths::FromDataRoot(dataRoot);
    std::filesystem::create_directories(paths.patchesRoot);
    return paths;
}

using Rig = synth_rig::SynthRig<synth_froggers::FroggersApp>;
namespace dsp = synth_froggers::dsp;

// SceneCenter writes are applied through a SMOOTHED periodic Compute
// (Parameter::ProcessSamplePhase1, alpha 0.0994 every 16 samples), so a patch
// is only ~81% applied one block after it is written. ComputeAllParameters()
// passes smoothTargetCenter=false and therefore converges exactly, in one
// call. Call this after the SceneCenter writes and before the first
// RunBlocks() whenever a test asserts on a patch rather than on a ramp.
//
// Route verified against FroggersAppCore.hpp: `context_` is private
// with no public accessor, so there is no `rig.Application().Context()` to
// call through. `TestParameterManager()` (FroggersAppCore.hpp, beside
// TestOutputLimiter()) is the narrow test-only accessor added for this.
inline void ApplyPatchNow(Rig& rig) {
    rig.Application().TestParameterManager().ComputeAllParameters();
}

// Peak absolute magnitude across every captured channel sample -- asserts
// some output sample exceeds a small epsilon in magnitude, which plain
// finiteness alone does not require.
float PeakAbs(const std::vector<Rig::OutputFrame>& frames) {
    float peak = 0.0f;
    for (const auto& frame : frames) {
        for (const float sample : frame.channels) {
            peak = std::max(peak, std::fabs(sample));
        }
    }
    return peak;
}

// Counts rising "gate opened" edges (channel 0's magnitude crossing above
// a small threshold from below) across captured output -- used to prove
// the ASR gate's cycle rate tracks tempo without needing to decode the
// exact envelope shape.
std::size_t CountRisingEdges(const std::vector<Rig::OutputFrame>& frames) {
    constexpr float kThreshold = 1.0e-3f;
    std::size_t count = 0;
    bool wasOpen = false;
    for (const auto& frame : frames) {
        const bool isOpen = std::fabs(frame.channels[0]) > kThreshold;
        if (isOpen && !wasOpen) {
            ++count;
        }
        wasOpen = isOpen;
    }
    return count;
}

void RequireFiniteStereo(const std::vector<Rig::OutputFrame>& frames) {
    REQUIRE_TRUE(!frames.empty());
    for (const auto& frame : frames) {
        REQUIRE_TRUE(frame.channels.size() == 2);  // Config() requests stereo out.
        for (const float sample : frame.channels) {
            REQUIRE_TRUE(std::isfinite(sample));
        }
    }
}

// The audible and inaudible fundamentals every band check in this file
// contrasts, and the linear silence floor they are compared against. Shared
// rather than re-typed per test: a band check that quietly drifts to a
// different triple stops being comparable with the others, and a floor that
// drifts stops matching ComputeSilenceSettleWindow's own
// kBandSilenceFloorLinear (-60 dBFS), which ComputeSilenceSettleWindow below
// returns as its own floor.
inline constexpr std::array<double, 3> kAudibleFundamentalsHz{110.0, 220.0, 330.0};
inline constexpr std::array<double, 3> kInaudibleFundamentalsHz{20.0, 40.0, 60.0};
inline constexpr float kBandSilenceFloorLinear = 1.0e-3f;

// Settle/check-
// window silence-measurement scaffolding, extracted after it appeared a
// third near-byte-identical time (the Grit stopped-state test) -- a
// concept repeating a third time is what turns two pre-existing
// near-identical blocks into duplication worth naming and sharing. Every caller only varies HOW
// LONG to wait before measuring (settleSeconds); sample rate, block size,
// the trailing check-window length, and the silence floor are the same
// fixed values at all 3 call sites (FroggersApp::Config() sets 48 kHz/
// 256-sample blocks, FroggersAppCore.hpp:196-197; -60 dBFS: 20*log10(x) =
// -60 -> x = 1.0e-3), so only settleSeconds is a parameter here.
//
// Deliberately does NOT also fold in the RunBlocks/ClearOutput/PeakAbs/
// REQUIRE_TRUE sequence that follows each call site -- that part is each
// test's own narrative and differs materially between callers (2 windows
// here, 3 windows -- one of them a distinct "afterStop" concept -- in the
// long-release test, an intervening Freeze-button press in the Freeze-latch
// test), so folding it in would gut those per-test assertions.
// Only the declaration block that derives checkWindowBlocks/settleLeadBlocks
// is shared.
struct SilenceSettleWindow {
    std::size_t settleLeadBlocks;
    std::size_t checkWindowBlocks;
    float silenceFloorLinear;
};

SilenceSettleWindow ComputeSilenceSettleWindow(double settleSeconds) {
    constexpr double kSampleRateHz = 48000.0;
    constexpr double kBlockSizeSamples = 256.0;
    constexpr double kCheckWindowSeconds = 0.02;  // ~20 ms trailing measurement window.
    const auto checkWindowBlocks =
        static_cast<std::size_t>(std::ceil((kCheckWindowSeconds * kSampleRateHz) / kBlockSizeSamples));
    const auto settleLeadBlocks =
        static_cast<std::size_t>(std::ceil((settleSeconds * kSampleRateHz) / kBlockSizeSamples)) -
        checkWindowBlocks;
    return {settleLeadBlocks, checkWindowBlocks, kBandSilenceFloorLinear};
}

// -----------------------------------------------------------------------
// Band-limited energy check (a mandatory "not RMS" assertion) -- a
// naive single-frequency Goertzel evaluated at arbitrary (non-bin-aligned)
// frequencies over a fixed-length window. Standard Goertzel recurrence
// (e.g. https://en.wikipedia.org/wiki/Goertzel_algorithm's "power" form):
//   coeff = 2*cos(2*pi*f/fs)
//   s0 = x[n] + coeff*s1 - s2 ; s2 = s1 ; s1 = s0            (per sample)
//   power = s1^2 + s2^2 - coeff*s1*s2                        (after N samples)
// This is exactly a length-N DFT correlation at frequency f, so it is valid
// for ANY f (not just fs*k/N bins) -- the "off-bin" cost is ordinary
// spectral leakage, irrelevant here since this test only needs "is there
// energy in this band", not a narrowband measurement.
double GoertzelPower(const std::vector<float>& samples, double freqHz, double sampleRateHz) {
    const double coeff = 2.0 * std::cos(2.0 * M_PI * freqHz / sampleRateHz);
    double s1 = 0.0, s2 = 0.0;
    for (const float sample : samples) {
        const double s0 = static_cast<double>(sample) + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return s1 * s1 + s2 * s2 - coeff * s1 * s2;
}

// `BandEnergy` (a summed sweep over freqStartHz..freqEndHz) used to live
// here; default_patch_has_audible_band_energy_above_150hz now sums
// `GoertzelPower` directly at specific fundamentals instead (see its own
// comment), leaving `BandEnergy` with no caller, so it was removed.

std::vector<float> ExtractChannel(const std::vector<Rig::OutputFrame>& frames, std::size_t channel) {
    std::vector<float> samples;
    samples.reserve(frames.size());
    for (const auto& frame : frames) {
        samples.push_back(frame.channels[channel]);
    }
    return samples;
}

// -----------------------------------------------------------------------
// The default patch must produce audio: a freshly started app
// should make sound with no user input. It sets three VCO Shapes, six
// cross-VCO modulation depths, and Drive to 20% -- all applied once from
// FroggersApp::Init() via ApplyFroggersDefaultPatch(), with no test-side
// parameter mutation here at all.
// -----------------------------------------------------------------------
TEST_CASE(default_patch_produces_non_silent_finite_audio) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("default_patch"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();

    // Confirm this really is the default patch before trusting the
    // energy assertion below to mean anything -- Gain (Drive bank, slot 1)
    // reads 20% of range, and at least one cross-VCO modulation depth is
    // materialized (ApplyFroggersDefaultPatch is called unconditionally
    // from FroggersApp::Init()).
    const synth::Parameter& driveParam = model.PageParameter(synth_froggers::FroggersBankId::Drive, 1);
    REQUIRE_TRUE(std::fabs(driveParam.SceneCenter(0) - 0.2f) < 1e-5f);
    const synth::Parameter& vco1Pitch = model.PageParameter(synth_froggers::FroggersBankId::Audio, 0);
    REQUIRE_TRUE(vco1Pitch.ModulationDepthParameter(synth_froggers::kModSlotVco2Audio) != nullptr);

    // The ASR gate follows the transport's quarter-note pulse, so the
    // transport must actually be running for any of this test's energy
    // assertions to mean anything.
    rig.StartAt(0);

    // Let scene-blend/fuego/modulation settle (kDefaultProcessLiteAlpha is a
    // ~1 kHz one-pole at 48 kHz, so a couple of blocks is generous margin),
    // then measure a fresh window of output.
    rig.RunBlocks(8);
    rig.ClearOutput();
    rig.RunBlocks(4);

    REQUIRE_TRUE(!rig.SawNaN());
    const auto& output = rig.Output();
    RequireFiniteStereo(output);

    constexpr float kEpsilon = 1.0e-4f;
    REQUIRE_TRUE(PeakAbs(output) > kEpsilon);
}

// -----------------------------------------------------------------------
// The default patch must be audible on laptop speakers,
// not merely nonzero. If All three Audio-bank pitch knobs (Audio bank slots
// 0-2) registered at the ordinary 0.0f default (FroggersParameters.hpp's own
// FroggersBankLayouts(), Audio row), then, since
// Vco::PitchToPhaseIncrement(0, sr) = ExpMapCompute(Vco::kPitchMinHz/sr, Vco::kPitchMaxHz/sr, 0)
// (app/dsp/Vco.hpp), every VCO would free-run at 20 Hz --
// inaudible on a MacBook Air, and NOT caught by a plain RMS/PeakAbs
// check (a 20 Hz tone is nonzero-RMS). This test instead asserts Goertzel
// power at the EXPECTED audible fundamentals (110/220/330 Hz) dominates
// power at the inaudible ones a 0.0f default would give (20/40/60 Hz) -- a
// ratio check needing
// no calibrated magic constant
// (see the assertion's own comment below for a prior, more fragile version).
// -----------------------------------------------------------------------
TEST_CASE(default_patch_has_audible_band_energy_above_150hz) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("default_patch_band_energy"));

    // The ASR gate only opens while the transport runs, so the transport
    // must be started for this to measure anything.
    rig.StartAt(0);

    // Let scene-blend/fuego/modulation settle, then capture a fresh window
    // long enough to resolve ~150 Hz content with a non-bin-aligned
    // Goertzel sweep (4096 samples @ 48 kHz = ~85 ms, comfortably inside
    // the ASR gate's open half of a 120 BPM quarter note -- 12000 samples --
    // started at rig.StartAt(0) above).
    rig.RunBlocks(8);
    rig.ClearOutput();
    rig.RunBlocks(16);

    REQUIRE_TRUE(!rig.SawNaN());
    const auto& output = rig.Output();
    RequireFiniteStereo(output);

    const std::vector<float> left = ExtractChannel(output, 0);
    // FroggersApp::Config() (app/FroggersAppCore.hpp:196) sets
    // preferredSampleRate = 48000.0, and this Rig() ctor call above passes
    // no override AudioSettings, so SynthRig negotiates that same rate
    // (SynthRig.hpp:68-70).
    constexpr double kSampleRateHz = 48000.0;

    // The prior version summed Goertzel power over a
    // 150 Hz-2000 Hz sweep and compared it to a calibrated constant
    // (1.0e5, ~5x margin from measured pre-fix/post-fix runs of ~2.0e4/
    // ~5.1e5) -- fragile because a future change to the Shape/Drive
    // defaults could drift the absolute figure with only a thin margin to
    // catch it. This version instead sums Goertzel power at the three
    // EXPECTED post-fix fundamentals (110/220/330 Hz, the Audio bank's
    // three VCOs at their ordinary 0.0f pitch default) and at the three
    // BROKEN pre-fix ones (20/40/60 Hz, what `Vco::PitchToPhaseIncrement`
    // collapses to without the fix) and asserts the expected fundamentals
    // dominate by a ratio, not an absolute threshold -- true regardless of
    // exactly how much total energy either patch config carries.

    constexpr std::array<double, 3> kBrokenFundamentalsHz = {20.0, 40.0, 60.0};

    double expectedPower = 0.0;
    for (const double freqHz : kAudibleFundamentalsHz) {
        expectedPower += GoertzelPower(left, freqHz, kSampleRateHz);
    }
    double brokenPower = 0.0;
    for (const double freqHz : kBrokenFundamentalsHz) {
        brokenPower += GoertzelPower(left, freqHz, kSampleRateHz);
    }

    // Ratio, not a magic constant: the previously measured ~25x separation
    // between pre-fix and post-fix band energy (see the comment this one
    // replaced) was over a whole 150 Hz-2000 Hz sweep, which includes
    // considerable harmonic leakage from the broken 20 Hz fundamentals
    // (saw/square harmonics from Audio bank slots 4-5's Shape defaults,
    // the default patch); measuring directly AT the fundamentals themselves (rather than
    // a swept band that partially captures both) should separate them by
    // at least as much. 10x is picked as a threshold comfortably inside
    // that ~25x figure -- large enough to unambiguously distinguish
    // "VCOs at 110/220/330 Hz" from "VCOs at 20/40/60 Hz", small enough to
    // leave real margin against incidental drift from future Shape/Drive
    // default changes (default-patch-tuned parameters this test does not own).
    constexpr double kMinimumFundamentalToBrokenRatio = 10.0;
    REQUIRE_TRUE(expectedPower > brokenPower * kMinimumFundamentalToBrokenRatio);
}

// -----------------------------------------------------------------------
// A deliberately non-default patch: nonzero Drive (well past
// the default patch's own 20%), a nonzero VCO level (the default patch
// leaves VCO pitch itself untouched, only Shape), and at least one active
// modulation depth. Also pushes the Filter/Reverb/Delay banks (which the
// default patch never touches) away from their neutral defaults,
// exercising more of the routing surface than the default-patch test
// above.
// -----------------------------------------------------------------------
TEST_CASE(non_default_patch_produces_non_silent_finite_audio) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("non_default_patch"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();

    synth::Parameter& vco1Pitch = model.PageParameter(synth_froggers::FroggersBankId::Audio, 0);
    // "At least one modulation depth active" -- the six cross-VCO depths
    // the default patch already materializes satisfy this
    // without any drill-in machinery here; asserted explicitly rather than
    // assumed.
    REQUIRE_TRUE(vco1Pitch.ModulationDepthParameter(synth_froggers::kModSlotVco2Audio) != nullptr);

    vco1Pitch.SceneCenter(0) = 0.5f;  // nonzero VCO level.
    model.PageParameter(synth_froggers::FroggersBankId::Drive, 1).SceneCenter(0) = 0.8f;    // nonzero Gain.
    model.PageParameter(synth_froggers::FroggersBankId::Filter, 12).SceneCenter(0) = 0.5f;   // Comb/Peak blend.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 0).SceneCenter(0) = 0.4f;   // Wet/dry.
    model.PageParameter(synth_froggers::FroggersBankId::Delay, 1).SceneCenter(0) = 0.4f;    // Send.
    model.PageParameter(synth_froggers::FroggersBankId::Delay, 0).SceneCenter(0) = 0.4f;    // Wet/dry.

    // Same as the default-patch test above -- the ASR gate only opens
    // while the transport runs.
    rig.StartAt(0);

    rig.RunBlocks(8);
    rig.ClearOutput();
    rig.RunBlocks(4);

    REQUIRE_TRUE(!rig.SawNaN());
    const auto& output = rig.Output();
    RequireFiniteStereo(output);

    constexpr float kEpsilon = 1.0e-4f;
    REQUIRE_TRUE(PeakAbs(output) > kEpsilon);
}

// -----------------------------------------------------------------------
// Push the Filter bank's Comb feedback to its self-oscillating extreme
// (knob=1.0 -> dsp::Comb::GetFeedback's +0.95 branch,
// deliberately below the firmware's +1.1, Comb.hpp:66-76) with the
// Comb/Peak blend turned fully toward Comb, and the Reverb bank's
// authored Hold control pushed to its ceiling -- the exact
// self-oscillating-comb scenario reachable by design -- and confirm the
// output stays finite and bounded.
// -----------------------------------------------------------------------
TEST_CASE(self_oscillating_comb_and_near_unity_reverb_hold_stays_finite_and_bounded) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("comb_self_oscillation"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();

    model.PageParameter(synth_froggers::FroggersBankId::Filter, 5).SceneCenter(0) = 1.0f;  // Comb feedback -> +0.95.
    model.PageParameter(synth_froggers::FroggersBankId::Filter, 12).SceneCenter(0) = 1.0f;  // Comb/Peak -> all comb.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 1).SceneCenter(0) = 1.0f;  // Send.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 9).SceneCenter(0) = 1.0f;  // Hold -> ceiling.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 0).SceneCenter(0) = 1.0f;  // Wet/dry fully wet.
    model.PageParameter(synth_froggers::FroggersBankId::Drive, 1).SceneCenter(0) = 1.0f;   // maximum Gain.

    // The transport must be running, or the ASR gate stays closed and this
    // scenario never actually drives the self-oscillating comb with real
    // signal.
    rig.StartAt(0);

    rig.RunBlocks(64);  // run long enough for any runaway growth to show up.

    REQUIRE_TRUE(!rig.SawNaN());
    const auto& output = rig.Output();
    RequireFiniteStereo(output);

    // Matches FroggersApp::SanitizeOutputSample's final clamp (1.0f ==
    // float full scale; +1e-3 headroom for float rounding at the exact
    // boundary) -- the point of this test is that output stays *bounded*,
    // not that it stays quiet.
    for (const auto& frame : output) {
        for (const float sample : frame.channels) {
            REQUIRE_TRUE(std::fabs(sample) <= 1.0f + 1.0e-3f);
        }
    }
}

// -----------------------------------------------------------------------
// The output clamp fix: the old ceiling
// (kMaxOutputMagnitude == 8.0f, +18 dBFS) let the device hard-clip into a
// square wave; the new ceiling (1.0f, float full scale) must never be
// exceeded even under the same deliberately overdriven patch the test above
// uses (self-oscillating Comb, Reverb Hold at its ceiling, maximum Drive),
// AND a normal-level (default) patch's output -- which never approaches
// either ceiling -- must be completely unaffected by the constant change:
// asserted here as "every sample stays under 1.0", which is only possible
// if the clamp never engages for it, i.e. passthrough is unchanged.
// -----------------------------------------------------------------------
TEST_CASE(output_clamp_bounds_overdriven_patch_to_full_scale) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("output_clamp_overdriven"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();

    model.PageParameter(synth_froggers::FroggersBankId::Filter, 5).SceneCenter(0) = 1.0f;  // Comb feedback -> +0.95.
    model.PageParameter(synth_froggers::FroggersBankId::Filter, 12).SceneCenter(0) = 1.0f;  // Comb/Peak -> all comb.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 1).SceneCenter(0) = 1.0f;  // Send.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 9).SceneCenter(0) = 1.0f;  // Hold -> ceiling.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 0).SceneCenter(0) = 1.0f;  // Wet/dry fully wet.
    model.PageParameter(synth_froggers::FroggersBankId::Drive, 1).SceneCenter(0) = 1.0f;   // maximum Gain.

    rig.StartAt(0);
    rig.RunBlocks(64);

    REQUIRE_TRUE(!rig.SawNaN());
    const auto& output = rig.Output();
    RequireFiniteStereo(output);
    for (const auto& frame : output) {
        for (const float sample : frame.channels) {
            REQUIRE_TRUE(std::fabs(sample) <= 1.0f + 1.0e-3f);
        }
    }
}

TEST_CASE(output_clamp_never_engages_for_normal_level_default_patch) {
    // A normal-level signal passes through the clamp bit-identical: since
    // SanitizeOutputSample's clamp is a no-op for any |x| already <=
    // kMaxOutputMagnitude, proving every sample of the untouched default
    // patch stays strictly under the NEW (tighter) 1.0f ceiling
    // proves the clamp never fires for it -- so this change could not have
    // altered a single one of its samples.
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("output_clamp_normal_level"));
    rig.StartAt(0);
    rig.RunBlocks(8);
    rig.ClearOutput();
    rig.RunBlocks(64);

    REQUIRE_TRUE(!rig.SawNaN());
    const auto& output = rig.Output();
    RequireFiniteStereo(output);
    for (const auto& frame : output) {
        for (const float sample : frame.channels) {
            REQUIRE_TRUE(std::fabs(sample) < 1.0f);
        }
    }
}

// -----------------------------------------------------------------------
// The acceptance test for "gain reduction, not per-sample waveshaping" --
// a signal entirely below OutputLimiter's 0.9 threshold must pass through
// Process() BIT-IDENTICAL (exact float `==`, not REQUIRE_NEAR). Drives the
// real per-app limiter instance directly via TestOutputLimiter() (this
// limiter's own test accessor), including values right at the threshold
// boundary and a long run, proving `envelope` never drifts off exactly
// 1.0f one sample at a time (see OutputLimiter::Process's own
// Sterbenz-lemma comment for why this must hold exactly, not just
// approximately).
// -----------------------------------------------------------------------
TEST_CASE(limiter_passes_below_threshold_signal_bit_identical) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("limiter_bit_identical"));
    auto& limiter = rig.Application().TestOutputLimiter();
    limiter.Reset();

    const float samples[] = {0.0f,      0.1f,       -0.1f,      0.5f,      -0.5f,     0.9f,
                              -0.9f,     0.899999f,  -0.899999f, 1.0e-6f,   -1.0e-6f};
    for (const float x : samples) {
        REQUIRE_TRUE(limiter.Process(x) == x);  // bit-for-bit, not approximate.
    }

    // A long run of below-threshold samples must never let `envelope` creep
    // off exactly 1.0f.
    for (int i = 0; i < 100000; ++i) {
        const float x = 0.8f * std::sin(static_cast<float>(i) * 0.01f);
        REQUIRE_TRUE(limiter.Process(x) == x);
    }
}

// -----------------------------------------------------------------------
// This test's own name says
// "reduces gain smoothly, not squared off" -- it does NOT say "never
// exceeds 1.0". That ceiling claim belongs to
// output_clamp_bounds_overdriven_patch_to_full_scale (:391 above), which
// exercises the real SanitizeOutputSample hard bound through the whole
// synth and already passes. This test instead exercises OutputLimiter
// directly via TestOutputLimiter(), bypassing that hard bound entirely, so
// it is only ever measuring the feed-forward limiter's OWN envelope
// tracking -- and a feed-forward limiter with no lookahead cannot promise
// an instantaneous ceiling: `envelope` (OutputLimiter::Process, above)
// one-pole-smooths toward each sample's targetGain rather than jumping to
// it, so on a fast-moving waveform the envelope can still be relaxing
// toward a lower target while `|x|` is already past its own local peak,
// letting `x * envelope` briefly overshoot the asymptote DesiredMagnitude
// approaches. Measured peak here is ~1.0274 for this tone -- a previous
// version of this test asserted `peak < 1.0f` directly against
// OutputLimiter output, which is what the ~1.0274 measurement contradicts;
// that assertion was wrong for what it exercises, not the limiter.
// The ceiling belongs to SanitizeOutputSample's hard bound, and gain
// reduction to this limiter -- this test now asserts exactly the
// gain-reduction claim: (1) the limited peak sits meaningfully below the
// unlimited input peak (kAmplitude, 1.5x) -- gain reduction actually
// happened; (2) consecutive near-peak output samples differ rather than
// repeating one clipped value -- the "not squared off" claim a hard clamp
// would violate; (3) a loose sanity bound of 1.05 (comfortably above the
// measured ~1.0274, comfortably below kAmplitude) to catch a gross
// regression -- e.g. a broken coefficient letting gain reduction stop
// working entirely -- without re-asserting the exact-ceiling claim this
// test does not own.
// -----------------------------------------------------------------------
TEST_CASE(limiter_reduces_gain_smoothly_not_squared_off) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("limiter_not_squared_off"));
    auto& limiter = rig.Application().TestOutputLimiter();
    limiter.Reset();

    constexpr float kSampleRate = 48000.0f;
    constexpr float kToneHz = 200.0f;
    constexpr float kAmplitude = 1.5f;  // well above 0.9 threshold, well below numeric-underflow territory.
    constexpr int kSamplesPerCycle = static_cast<int>(kSampleRate / kToneHz);
    constexpr int kWarmupCycles = 50;  // let the envelope settle into steady periodic tracking.

    auto toneAt = [&](int sampleIndex) {
        return kAmplitude * std::sin(2.0f * static_cast<float>(M_PI) * kToneHz *
                                      static_cast<float>(sampleIndex) / kSampleRate);
    };

    for (int i = 0; i < kWarmupCycles * kSamplesPerCycle; ++i) {
        limiter.Process(toneAt(i));
    }

    float peak = 0.0f;
    bool haveFirstNearPeak = false;
    float firstNearPeakValue = 0.0f;
    bool sawDistinctValuesNearPeak = false;
    for (int i = 0; i < kSamplesPerCycle; ++i) {
        const int sampleIndex = kWarmupCycles * kSamplesPerCycle + i;
        const float x = toneAt(sampleIndex);
        const float y = limiter.Process(x);
        peak = std::max(peak, std::fabs(y));
        if (std::fabs(x) > 1.4f) {  // near the sine's own peak, well above threshold.
            if (!haveFirstNearPeak) {
                firstNearPeakValue = y;
                haveFirstNearPeak = true;
            } else if (y != firstNearPeakValue) {
                sawDistinctValuesNearPeak = true;
            }
        }
    }
    REQUIRE_TRUE(haveFirstNearPeak);
    REQUIRE_TRUE(peak < kAmplitude - 0.3f);   // meaningfully below the unlimited 1.5x input peak: gain reduction happened.
    REQUIRE_TRUE(sawDistinctValuesNearPeak);  // NOT flattened to one repeated value near the peak (not squared off).
    REQUIRE_TRUE(peak < 1.05f);               // loose sanity bound (measured ~1.0274): catches gross failure, not a
                                               // hard ceiling -- see this test's header comment for why a
                                               // feed-forward limiter without lookahead cannot promise one; that
                                               // claim belongs to output_clamp_bounds_overdriven_patch_to_full_scale.
}

// -----------------------------------------------------------------------
// Re-homed from limiter_engages_on_overdriven_patch_and_stays_bounded
// (below), which bundled two properties -- "the limiter itself does real
// gain reduction, not a no-op" and "this particular hostile patch runs the
// master hot enough to need it". The second is the continuous-gain-
// reduction symptom (see overdriven_patch_stays_bounded's comment below)
// and was struck from the
// chain-level test. This property is independent of gain staging, so it
// moves here, driven directly via TestOutputLimiter() -- same convention as
// limiter_passes_below_threshold_signal_bit_identical (:448 above) and
// limiter_reduces_gain_smoothly_not_squared_off (:498 above).
// -----------------------------------------------------------------------
TEST_CASE(limiter_engages_and_envelope_drops_below_unity) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("limiter_engages_direct"));
    auto& limiter = rig.Application().TestOutputLimiter();
    limiter.Reset();

    // A steady tone well above the 0.9 threshold, run long enough for the
    // 1ms-attack envelope to settle away from its 1.0f rest state.
    constexpr float kSampleRate = 48000.0f;
    constexpr float kToneHz = 200.0f;
    constexpr float kAmplitude = 1.5f;
    constexpr int kSamplesPerCycle = static_cast<int>(kSampleRate / kToneHz);
    constexpr int kCycles = 20;
    for (int i = 0; i < kCycles * kSamplesPerCycle; ++i) {
        const float x = kAmplitude * std::sin(2.0f * static_cast<float>(M_PI) * kToneHz *
                                               static_cast<float>(i) / kSampleRate);
        limiter.Process(x);
    }

    REQUIRE_TRUE(limiter.envelope < 1.0f);  // the limiter genuinely engaged, not a no-op.
}

// -----------------------------------------------------------------------
// The same deliberately overdriven patch
// output_clamp_bounds_overdriven_patch_to_full_scale uses (self-oscillating
// Comb, Reverb Hold at ceiling, maximum Drive). This test used to also
// assert `minEnvelopeSeen < 0.999f` --
// "the master limiter engages on this patch". That assertion pins the master sitting in continuous gain reduction on an
// ordinary hostile patch, which is precisely what the per-stage headroom
// architecture (C = 0.80) exists to remove -- so it was destined to fail
// the moment the real fix landed. The "limiter genuinely does gain
// reduction" property it also asserted is independent and is re-homed to
// limiter_engages_and_envelope_drops_below_unity above. This test keeps
// only the boundedness claim, and its name (formerly
// limiter_engages_on_overdriven_patch_and_stays_bounded) was changed to
// match -- a test whose name claims a property it no longer checks is its
// own trap.
// -----------------------------------------------------------------------
TEST_CASE(overdriven_patch_stays_bounded) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("limiter_overdriven_engages"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();

    model.PageParameter(synth_froggers::FroggersBankId::Filter, 5).SceneCenter(0) = 1.0f;  // Comb feedback -> +0.95.
    model.PageParameter(synth_froggers::FroggersBankId::Filter, 12).SceneCenter(0) = 1.0f;  // Comb/Peak -> all comb.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 1).SceneCenter(0) = 1.0f;  // Send.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 9).SceneCenter(0) = 1.0f;  // Hold -> ceiling.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 0).SceneCenter(0) = 1.0f;  // Wet/dry fully wet.
    model.PageParameter(synth_froggers::FroggersBankId::Drive, 1).SceneCenter(0) = 1.0f;   // maximum Gain.

    rig.StartAt(0);
    // 256 blocks: the boundedness-stress window. No
    // longer sampled per-block -- that per-block loop existed only to track
    // `minEnvelopeSeen` for the engagement assertion removed above, so it
    // collapses to a single call.
    rig.RunBlocks(256);

    REQUIRE_TRUE(!rig.SawNaN());
    const auto& output = rig.Output();
    RequireFiniteStereo(output);
    for (const auto& frame : output) {
        for (const float sample : frame.channels) {
            REQUIRE_TRUE(std::fabs(sample) <= 1.0f + 1.0e-3f);
        }
    }
}

// -----------------------------------------------------------------------
// The master limiter is the BACKSTOP, not the gain-staging
// mechanism. With every stage bounded to C, a hostile patch must not
// engage it at all. This test is the only end-to-end proof of that; every
// other limiter test in this file measures one stage under synthetic
// input.
TEST_CASE(master_limiter_stays_at_unity_across_hostile_patch) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("b7_5_hostile"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();

    // The SAME hostile patch `overdriven_patch_stays_bounded` uses -- measured
    // to engage the master at block 101, min envelope 0.809, so it is
    // known-hostile by measurement rather than by assumption.
    model.PageParameter(synth_froggers::FroggersBankId::Filter, 5).SceneCenter(0) = 1.0f;  // Comb feedback -> +0.95
    model.PageParameter(synth_froggers::FroggersBankId::Filter, 12).SceneCenter(0) = 1.0f;  // Comb/Peak -> all comb
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 1).SceneCenter(0) = 1.0f;  // Send.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 9).SceneCenter(0) = 1.0f;  // Hold -> ceiling
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 0).SceneCenter(0) = 1.0f;  // fully wet
    model.PageParameter(synth_froggers::FroggersBankId::Drive, 1).SceneCenter(0) = 1.0f;   // maximum Gain
    // PLUS the operator's stated repro on top: Filter bank Crispy at max
    // scrambles all 8 bits of every Filter parameter per read. NOTE the
    // accessor -- Crispy is NOT in pageParameters_ (9 wide); it lives in its
    // own `crispy_` array (FroggersParameters.hpp:413-414), and
    // PageParameter(Filter, 14) throws std::out_of_range.
    model.Crispy(synth_froggers::FroggersBankId::Filter).SceneCenter(0) = 1.0f;
    // A SceneCenter write is only ~81% applied after one block
    // (Parameter::ProcessSamplePhase1's periodic smoothed Compute, alpha
    // 0.0994 every 16 samples). This test must measure the patch it declares,
    // not a ramp into it, so apply it exactly before the first block.
    ApplyPatchNow(rig);

    rig.StartAt(0);
    auto& limiter = rig.Application().TestOutputLimiter();

    float minEnvelopeSeen = 1.0f;
    for (int block = 0; block < 256; ++block) {
        rig.RunBlocks(1);
        minEnvelopeSeen = std::min(minEnvelopeSeen, limiter.envelope);
    }

    REQUIRE_TRUE(!rig.SawNaN());
    RequireFiniteStereo(rig.Output());
    // LIVENESS -- without this the assertion below passes on silence, which is
    // how the first version of this test passed while proving nothing. The
    // instrument must actually be sounding for "the master never engaged" to
    // mean anything.
    REQUIRE_TRUE(PeakAbs(rig.Output()) > 0.1f);
    // The property. Unity means the master never had to do anything.
    REQUIRE_TRUE(minEnvelopeSeen > 0.999f);
}

// -----------------------------------------------------------------------
// The test above proves the master stays at unity
// under STATIC knobs only. This change's own acceptance criterion is "all
// maxima, modulation live" -- measurement showed DriveBlendPhase's allpass
// coefficient (read fresh every sample from the Drive/Phase knob) hitting
// 50.5x under periodic phase/content coincidence, the largest known
// blowout path in the instrument, and no static-knob test exercises it.
// This is the SAME hostile patch as
// master_limiter_stays_at_unity_across_hostile_patch, PLUS deep audio-rate
// modulation on Drive slot 8 (Phase, that 50x path) and Filter slot 5 (Comb
// feedback, whose trim smoother was tuned against rand() sweeps, never real
// modulation). Deliberately kept as a SEPARATE
// test rather than folded into the one above: the two discriminate
// different failures (static gain staging vs. modulation-driven
// transients).
//
// SOURCE: kModSlotVco1Audio, not kModSlotNoise. First attempt used
// kModSlotNoise and measured minEnvelopeSeen=0.985726 -- statistically
// indistinguishable from the static test's 0.985796 -- which is a genuine
// end-to-end confirmation of the free-random-phase measurement's figure
// (1.002x, not the 50.5x figure), not a null result: that 50.5x figure is
// specifically from "periodic phase/content coincidence", and a
// per-sample-random noise source (NoiseModulatorProcessor::Process() ->
// random_.UniformOpen01(), DspNoise.hpp:69-71) structurally cannot produce
// periodic coincidence. kModSlotVco1Audio (vco1AudioSource_ =
// NormalizeBipolarToUnit(vco1Raw), FroggersModulation.hpp:384, registered
// at :535-536) IS periodic at the note frequency and locked to the note's
// period by construction -- it IS the note passing through DriveBlendPhase,
// so the coincidence that earlier measurement captured is exact rather
// than approximate. This is the corrected source for both
// modulated parameters below; kModSlotNoise is not used anywhere in this
// test.
//
// Route verified against synth::Parameter (ParameterModulation.hpp:499):
// `Parameter* EnsureModulationDepth(std::size_t modIx)` is public, returns
// nullptr at storage capacity (checked below, not dereferenced blindly), and
// PageParameter() returns synth::Parameter& (confirmed at
// FroggersModulation.hpp:1192,1207-1208's existing call sites), so it is
// reachable as PageParameter(...).EnsureModulationDepth(...) directly, no
// intermediate pointer needed. kModSlotVco1Audio (FroggersModulation.hpp:163,
// synth_froggers namespace) is registered `connected = true` unconditionally
// in ModulatorGroup's constructor (FroggersModulation.hpp:535-536), so it
// needs no extra wiring here. Depth centers are confirmed BIPOLAR by
// ModulationDepthTargetFromKnob (ParameterModulation.hpp:115-122): knob 0.5
// maps to bipolar 0 (zero depth), knob 1.0 maps to bipolar +1 (full
// positive) -- matching kNeutralModulationDepthCenter = 0.5f
// (FroggersModulation.hpp:832). SceneCenter(0) = 1.0f below is therefore
// full-positive depth, not a mid-scale value.
TEST_CASE(master_limiter_stays_at_unity_under_live_modulation) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("b7_5_live_mod"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();

    // Identical hostile patch to master_limiter_stays_at_unity_across_hostile_patch.
    model.PageParameter(synth_froggers::FroggersBankId::Filter, 5).SceneCenter(0) = 1.0f;  // Comb feedback -> +0.95
    model.PageParameter(synth_froggers::FroggersBankId::Filter, 12).SceneCenter(0) = 1.0f;  // Comb/Peak -> all comb
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 1).SceneCenter(0) = 1.0f;  // Send.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 9).SceneCenter(0) = 1.0f;  // Hold -> ceiling
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 0).SceneCenter(0) = 1.0f;  // fully wet
    model.PageParameter(synth_froggers::FroggersBankId::Drive, 1).SceneCenter(0) = 1.0f;   // maximum Gain
    model.Crispy(synth_froggers::FroggersBankId::Filter).SceneCenter(0) = 1.0f;

    // PLUS deep audio-rate modulation on the two parameters the evidence
    // names -- both from kModSlotVco1Audio (periodic, note-locked -- see the
    // route note above for why this replaces kModSlotNoise), depth at full
    // positive (1.0f, bipolar-neutral is 0.5f per the route note above).
    synth::Parameter* phaseDepth =
        model.PageParameter(synth_froggers::FroggersBankId::Drive, 8)
             .EnsureModulationDepth(synth_froggers::kModSlotVco1Audio);
    REQUIRE_TRUE(phaseDepth != nullptr);
    phaseDepth->SceneCenter(0) = 1.0f;

    synth::Parameter* combFeedbackDepth =
        model.PageParameter(synth_froggers::FroggersBankId::Filter, 5)
             .EnsureModulationDepth(synth_froggers::kModSlotVco1Audio);
    REQUIRE_TRUE(combFeedbackDepth != nullptr);
    combFeedbackDepth->SceneCenter(0) = 1.0f;

    // Apply the patch (including the modulation-depth SceneCenter
    // writes above) before the first block, exactly as the static test does.
    ApplyPatchNow(rig);

    rig.StartAt(0);
    auto& limiter = rig.Application().TestOutputLimiter();

    float minEnvelopeSeen = 1.0f;
    for (int block = 0; block < 256; ++block) {
        rig.RunBlocks(1);
        minEnvelopeSeen = std::min(minEnvelopeSeen, limiter.envelope);
    }

    REQUIRE_TRUE(!rig.SawNaN());
    RequireFiniteStereo(rig.Output());
    // LIVENESS -- same rationale as the static test above.
    REQUIRE_TRUE(PeakAbs(rig.Output()) > 0.1f);
    // The property. Unity means the master never had to do anything.
    REQUIRE_TRUE(minEnvelopeSeen > 0.999f);
}

// -----------------------------------------------------------------------
// The randomize storm test: the predecessor's failure rate was roughly 1
// in 7 Randomize All draws (permanent silence or a full-scale-exceeding
// blowout); this must show ZERO across at least 200 draws through the
// REAL engine path --
// `rig.Application().RequestRandomizeAll()` is the exact method
// `FroggersUiSurface::HandleAction` calls for the Randomize All button
// (FroggersUiSurface.hpp), consumed on the very next audio-thread
// ProcessFrame() (FroggersAppCore.hpp's `pendingRandomizeAll_`) -- the
// real call, not a shadow/copy of the DSP chain.
//
// Per draw: render a full second of audio (one full quarter-note gate
// cycle or more at any ordinary tempo, since the attack/release
// ceilings and RandomizeAll's own scope never touch the master clock's
// BPM -- only the six parameter banks), then assert (a) finite throughout,
// (b) never sustained above full scale (the limiter's own bound, with the
// same rounding headroom the other output-bound tests use), and (c) still
// audibly producing sound (PeakAbs over that window exceeds a small
// epsilon) -- "not permanently silenced" is exactly the predecessor's
// failure signature.
//
// Deliberately does NOT stop at the first failure (unlike REQUIRE_TRUE
// elsewhere in this file): tallies failures across all 200+ draws so a
// nonzero rate is reported precisely rather than only "it failed once
// somewhere" -- report a real finding rather than loosen the assertion.
// -----------------------------------------------------------------------
TEST_CASE(randomize_all_storm_test_never_blows_out_or_permanently_silences) {
    constexpr int kNumDraws = 200;
    constexpr double kSampleRateHz = 48000.0;
    constexpr double kBlockSize = 256.0;  // FroggersApp::Config().
    const std::size_t kBlocksPerSecond = static_cast<std::size_t>(std::ceil(kSampleRateHz / kBlockSize));
    constexpr float kOverScaleBound = 1.0f + 1.0e-3f;
    constexpr float kSilenceEpsilon = 1.0e-4f;

    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("randomize_all_storm"));
    rig.StartAt(0);
    rig.RunBlocks(8);  // establish a running steady state before the first draw.

    int nonFiniteFailures = 0;
    int overScaleFailures = 0;
    int permanentSilenceFailures = 0;

    for (int draw = 0; draw < kNumDraws; ++draw) {
        rig.Application().RequestRandomizeAll();
        rig.ClearOutput();
        rig.RunBlocks(kBlocksPerSecond);

        if (rig.SawNaN()) {
            ++nonFiniteFailures;
        }
        const auto& output = rig.Output();
        bool everyFinite = true;
        bool anyOverScale = false;
        float peak = 0.0f;
        for (const auto& frame : output) {
            for (const float sample : frame.channels) {
                if (!std::isfinite(sample)) {
                    everyFinite = false;
                    continue;
                }
                if (std::fabs(sample) > kOverScaleBound) {
                    anyOverScale = true;
                }
                peak = std::max(peak, std::fabs(sample));
            }
        }
        if (!everyFinite && !rig.SawNaN()) {
            ++nonFiniteFailures;  // belt-and-suspenders: SawNaN() covers the primary path, this covers any gap.
        }
        if (anyOverScale) {
            ++overScaleFailures;
        }
        if (peak <= kSilenceEpsilon) {
            ++permanentSilenceFailures;
        }
    }

    const int totalFailures = nonFiniteFailures + overScaleFailures + permanentSilenceFailures;
    if (totalFailures != 0) {
        std::ostringstream oss;
        oss << __FILE__ << ":" << __LINE__ << " randomize_all_storm_test_never_blows_out_or_permanently_silences: "
            << totalFailures << "/" << kNumDraws << " draws failed (" << nonFiniteFailures << " non-finite, "
            << overScaleFailures << " over full scale, " << permanentSilenceFailures << " permanently silent)";
        throw std::runtime_error(oss.str());
    }
}

// -----------------------------------------------------------------------
// This IS the mechanism that fixes the operator's "audio never comes
// back": before RecoverPoisonedUnitState()
// existed, nothing anywhere in FroggersAppCore.hpp ever cleared a unit's
// recursive state once poisoned (see RecoverPoisonedUnitState()'s own
// header comment for the full trace), so it persisted across every future
// block regardless of what the operator changed the knobs to. Injects a
// non-finite value directly into ONE unit's state (the Filter bank's peak
// ResonantBump -- the same kind of unit the scoopNotch self-oscillation bug
// actually hit) via the test accessors, and asserts (a) that
// unit recovers within a handful of blocks and (b) a DIFFERENT unit's state
// is completely untouched -- proving recovery is per-unit, not global (this
// class's own design decision: a global reset would cut the reverb tail and
// delay repeats every time one unrelated unit misbehaved).
// -----------------------------------------------------------------------
TEST_CASE(finiteness_recovery_resets_only_the_poisoned_unit_and_audio_recovers) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("finiteness_recovery"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    model.PageParameter(synth_froggers::FroggersBankId::Audio, 0).SceneCenter(0) = 0.5f;  // nonzero VCO level.
    model.PageParameter(synth_froggers::FroggersBankId::Drive, 1).SceneCenter(0) = 0.5f;  // nonzero Gain.
    rig.StartAt(0);

    // "Other unit untouched" marker: a sentinel written into the Comb's
    // delay line far enough from its write cursor (which starts at 0 and
    // only advances one slot per SAMPLE) that the small number of samples
    // this test runs can never reach it -- so if Reset() (which
    // std::fill()s the WHOLE array) is ever mistakenly called on the comb,
    // this slot goes from the sentinel to exactly 0.0f; if the comb is
    // truly untouched, it stays exactly the sentinel.
    dsp::Comb& comb = rig.Application().TestFilterComb();
    constexpr float kSentinel = 12345.6789f;
    constexpr std::size_t kSentinelIndex = 4000;  // << dsp::Comb::kSize (8192); far past any write cursor here.
    comb.delayLine[kSentinelIndex] = kSentinel;

    // Poison the peak ResonantBump's own recursive state directly.
    dsp::ResonantBump& peak = rig.Application().TestFilterPeak();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    peak.biquad.y1 = nan;
    peak.biquad.y2 = nan;
    REQUIRE_TRUE(!peak.StateFinite());

    // One block (256 samples @ 48kHz, FroggersAppCore::Config()) is enough
    // for RecoverPoisonedUnitState() to fire -- it runs once per block, and
    // Tier 1 fires immediately/unconditionally on the very first check that
    // observes non-finite state, with no sustained-window delay (only Tier
    // 2's magnitude check has one).
    rig.RunBlocks(1);

    REQUIRE_TRUE(peak.StateFinite());  // recovered.
    REQUIRE_TRUE(comb.delayLine[kSentinelIndex] == kSentinel);  // a DIFFERENT unit: untouched.

    // And audio genuinely recovers: run further blocks and confirm real,
    // non-silent, finite output -- not permanently masked to zero the way
    // SanitizeOutputSample alone (with no recovery underneath it) would
    // have left it.
    rig.RunBlocks(8);
    rig.ClearOutput();
    rig.RunBlocks(8);
    REQUIRE_TRUE(!rig.SawNaN());
    const auto& output = rig.Output();
    RequireFiniteStereo(output);
    REQUIRE_TRUE(PeakAbs(output) > 1.0e-4f);
}

// -----------------------------------------------------------------------
// Tier 2, magnitude recovery -- "sustained" defined and
// justified: a unit's state magnitude must exceed kMaxUnitStateMagnitude
// (100.0, derived beside the constant in FroggersAppCore.hpp) for at least
// kSustainedOverCeilingSeconds (0.01s == 10ms) of continuous real time,
// checked once per block, before Tier 2 resets it -- so a single block's
// transient excursion must NOT fire it, but two-or-more consecutive
// over-ceiling blocks (which already total >= 10ms at this rig's 256-
// sample/48kHz config: 256/48000 ~= 5.33ms/block, so block 1 alone is
// 5.33ms < 10ms, but block 1 + block 2 is 10.67ms >= 10ms) must.
// -----------------------------------------------------------------------
TEST_CASE(magnitude_recovery_ignores_a_single_block_transient) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("magnitude_recovery_transient"));
    dsp::Comb& comb = rig.Application().TestFilterComb();

    // A finite, over-ceiling (> 100.0) value far from the write cursor (see
    // the finiteness-recovery test above for why 4000 is safe) -- Tier 1
    // does not fire (it IS finite), so only Tier 2's sustained-window logic
    // is under test here.
    constexpr std::size_t kIndex = 4000;
    comb.delayLine[kIndex] = 500.0f;
    REQUIRE_TRUE(comb.StateFinite());
    REQUIRE_TRUE(comb.StateMagnitude() > 100.0f);

    // One over-ceiling block (~5.33ms) is strictly less than the 10ms
    // sustained window -- must NOT have fired yet.
    rig.RunBlocks(1);
    REQUIRE_TRUE(comb.delayLine[kIndex] == 500.0f);  // untouched: not reset.

    // The transient subsides (as a real one-sample spike would, decaying
    // within its own block) -- drop back under the ceiling before the
    // sustained window would have elapsed.
    comb.delayLine[kIndex] = 50.0f;  // finite, well under the 100.0 ceiling.
    rig.RunBlocks(4);
    // Still exactly 50.0f: the counter reset to 0 the instant magnitude
    // dropped back under the ceiling, so no amount of further (now-normal)
    // time can retroactively fire Tier 2 for this now-subsided transient.
    REQUIRE_TRUE(comb.delayLine[kIndex] == 50.0f);
}

TEST_CASE(magnitude_recovery_resets_after_sustained_over_ceiling_window) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("magnitude_recovery_sustained"));
    dsp::Comb& comb = rig.Application().TestFilterComb();

    constexpr std::size_t kIndex = 4000;
    comb.delayLine[kIndex] = 500.0f;

    // Block 1: ~5.33ms elapsed, still < 10ms -- not yet reset.
    rig.RunBlocks(1);
    REQUIRE_TRUE(comb.delayLine[kIndex] == 500.0f);

    // Block 2: ~10.67ms elapsed total, >= 10ms -- Tier 2 fires. Reset()
    // std::fill()s the WHOLE delay line to 0.0f, so the sentinel slot goes
    // to exactly 0.0f (not merely "no longer 500").
    rig.RunBlocks(1);
    REQUIRE_TRUE(comb.delayLine[kIndex] == 0.0f);
    REQUIRE_TRUE(comb.StateMagnitude() <= 100.0f);
}

// -----------------------------------------------------------------------
// Full-range endpoint sweep. No existing test drives every parameter to
// its endpoints; this drives every named parameter (slots 0-8,
// kFroggersParamsPerBank) plus each bank's own Crispy (slot 14,
// kFroggersCrispySlot) in all six banks, plus the shared Crunchy (slot 15,
// kFroggersCrunchySlot -- the SAME synth::Parameter object in every bank,
// so swept once, not six times), to 0.0, to
// 1.0, and back to a neutral 0.5, through the REAL engine path
// (rig.RunBlocks -- FroggersApp::ProcessBlock/RouteAudioSample, not a
// shadow/copy), asserting output stays finite at every single endpoint AND
// that the instrument is not left permanently silent once every parameter
// is back at a neutral value.
// -----------------------------------------------------------------------
TEST_CASE(full_range_endpoint_sweep_stays_finite_and_not_permanently_silenced) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("endpoint_sweep"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    rig.StartAt(0);
    rig.RunBlocks(4);  // establish a running steady state first.

    const synth_froggers::FroggersBankId banks[] = {
        synth_froggers::FroggersBankId::Audio,  synth_froggers::FroggersBankId::Envelope,
        synth_froggers::FroggersBankId::Filter, synth_froggers::FroggersBankId::Drive,
        synth_froggers::FroggersBankId::Delay,  synth_froggers::FroggersBankId::Reverb,
    };

    const auto sweepOneParam = [&](synth::Parameter& param) {
        for (const float endpoint : {0.0f, 1.0f}) {
            param.SceneCenter(0) = endpoint;
            rig.RunBlocks(2);
            REQUIRE_TRUE(!rig.SawNaN());
            RequireFiniteStereo(rig.Output());
            rig.ClearOutput();
        }
        param.SceneCenter(0) = 0.5f;  // neutral, nonzero/non-degenerate, before the next parameter.
        rig.RunBlocks(1);
        rig.ClearOutput();
    };

    for (const auto bank : banks) {
        for (std::size_t slot = 0; slot < synth_froggers::kFroggersParamsPerBank; ++slot) {
            sweepOneParam(model.PageParameter(bank, slot));
        }
        sweepOneParam(model.Crispy(bank));  // this bank's own Crispy (slot 14) -- own accessor, not PageParameter.
    }
    sweepOneParam(model.Crunchy());  // shared Crunchy (slot 15), swept once.

    // Not permanently silenced: with every parameter now left at a neutral
    // 0.5 (not the default, but nonzero/non-degenerate everywhere), the
    // instrument must still be capable of producing real, audible output --
    // proving the sweep (including whatever transient recovery it may have
    // triggered along the way) did not leave anything permanently poisoned.
    rig.RunBlocks(8);
    rig.ClearOutput();
    rig.RunBlocks(8);
    REQUIRE_TRUE(!rig.SawNaN());
    const auto& output = rig.Output();
    RequireFiniteStereo(output);
    REQUIRE_TRUE(PeakAbs(output) > 1.0e-4f);
}

// -----------------------------------------------------------------------
// The app is silent while the transport is stopped, regardless of any
// parameter setting, including
// immediately after Init()/PrepareToPlay() with the transport never
// started. Uses the same non-default patch as the test above (nonzero
// Drive, nonzero VCO level, an active modulation depth) specifically to
// prove silence is coming from the gate, not merely from an unexcited
// patch -- if the gate were stuck open this patch would be loud (see
// non_default_patch_produces_non_silent_finite_audio above).
// -----------------------------------------------------------------------
TEST_CASE(silent_while_transport_is_stopped) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("transport_stopped"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();

    synth::Parameter& vco1Pitch = model.PageParameter(synth_froggers::FroggersBankId::Audio, 0);
    REQUIRE_TRUE(vco1Pitch.ModulationDepthParameter(synth_froggers::kModSlotVco2Audio) != nullptr);

    vco1Pitch.SceneCenter(0) = 0.5f;  // nonzero VCO level.
    model.PageParameter(synth_froggers::FroggersBankId::Drive, 1).SceneCenter(0) = 0.8f;  // nonzero Gain.

    // Deliberately no rig.StartAt(...) -- the rig's transport starts
    // Stopped (MasterClock::Prepare() resets transportState_ to Stopped,
    // src/MasterClock.cpp:929) and stays there absent an explicit Start.
    rig.RunBlocks(12);

    REQUIRE_TRUE(!rig.SawNaN());
    const auto& output = rig.Output();
    RequireFiniteStereo(output);
    REQUIRE_TRUE(PeakAbs(output) == 0.0f);
}

// -----------------------------------------------------------------------
// A tempo-change-tracking test, in two parts.
//
// Part 1 asserts the thing this test is named for, directly: the
// transport's own quarter-note gate period halves when tempo doubles.
// Read straight from MasterClock::QuarterNotesPerSample() (a rate set by
// SetTempoBpm, independent of any envelope/audio rendering), the period in
// samples is deterministic arithmetic -- sampleRate * 60 / bpm -- so the
// doubling is exact to floating-point rounding, not merely "roughly".
//
// Part 2 keeps an audio-domain check, because a clock read alone does not
// prove the tempo change reaches anything audible -- CountRisingEdges
// still counts threshold crossings in the real rendered output. Its
// tempos are chosen so the ASR envelope can actually complete a full
// attack/decay/hold cycle before every gate transition, at BOTH tempos,
// with real headroom to the new ExpMap floors (VoiceEnvelope.hpp:
// kMinAttackSeconds 1ms, kMinDecaySeconds/kMinReleaseSeconds 5ms each):
//   - kBaseTempoBpm = 1500 -> quarter note = 1920 samples @ 48kHz, each
//     half = 20ms.
//   - kDoubledTempoBpm = 3000 -> quarter note = 960 samples, each half =
//     10ms -- the tighter of the two halves, and still the one that
//     matters: Attack (1ms floor) + Decay (5ms floor) = 6ms fits inside
//     the 10ms open half more than 1.6x over, so Decay always finishes
//     and Hold is reached at the exact sustain floor (0.25) before the
//     gate closes -- release then only has to fall 0.25 of the 5ms
//     release floor, ~1.25ms, comfortably inside (>2x over) even a
//     conservative ~4ms fall-from-sustain estimate, let alone the full
//     10ms closed half available. Both tempos land the SAME clean
//     attack->decay->hold->release shape per cycle, at 2x the cycle rate,
//     so CountRisingEdges' per-cycle multiplier (extra rising edges from
//     the audio oscillator itself, gated open during Hold) stays close to
//     constant across the doubling instead of collapsing the way it did
//     at 12000 BPM, where the 2.5ms closed half was shorter than the
//     release floor could complete in from a not-yet-settled level.
// Measured on this build at 128 blocks (32768 samples) per tempo:
// base=59, doubled=105 (ratio ~1.78). The bound below is pinned around
// that measurement, tight enough to fail on a collapse like the one this
// test caught (ratios well under 1.3), not loose enough to accept
// whatever the code happens to do.
// -----------------------------------------------------------------------
TEST_CASE(gate_period_tracks_tempo_change) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("tempo_tracks_gate"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    model.PageParameter(synth_froggers::FroggersBankId::Drive, 1).SceneCenter(0) = 0.8f;
    model.PageParameter(synth_froggers::FroggersBankId::Audio, 0).SceneCenter(0) = 0.5f;

    constexpr double kBaseTempoBpm = 1500.0;
    constexpr double kDoubledTempoBpm = kBaseTempoBpm * 2.0;

    // Part 1: the clock's own gate period, read directly, halves exactly.
    REQUIRE_TRUE(rig.Engine().Clock().SetTempoBpm(kBaseTempoBpm));
    const double basePeriodSamples = 1.0 / rig.Engine().Clock().QuarterNotesPerSample();
    rig.StartAt(0);
    rig.RunBlocks(128);
    const std::size_t baseTransitions = CountRisingEdges(rig.Output());

    rig.ClearOutput();
    REQUIRE_TRUE(rig.Engine().Clock().SetTempoBpm(kDoubledTempoBpm));
    const double doubledPeriodSamples = 1.0 / rig.Engine().Clock().QuarterNotesPerSample();
    rig.RunBlocks(128);
    const std::size_t doubledTransitions = CountRisingEdges(rig.Output());

    // basePeriodSamples/doubledPeriodSamples is exact rational arithmetic
    // (sampleRate*60/bpm for each), so this tolerance only absorbs
    // double-precision rounding, not any real slack in the relationship.
    REQUIRE_TRUE(std::fabs(basePeriodSamples - doubledPeriodSamples * 2.0) < 1.0e-6);

    // Part 2: the audio-domain proxy still moves with tempo, at tempos
    // where the envelope actually tracks (see this TEST_CASE's own
    // header comment for the headroom numbers).
    REQUIRE_TRUE(baseTransitions > 0);
    REQUIRE_TRUE(static_cast<double>(doubledTransitions) > static_cast<double>(baseTransitions) * 1.6);
    REQUIRE_TRUE(static_cast<double>(doubledTransitions) < static_cast<double>(baseTransitions) * 2.0);
}

// -----------------------------------------------------------------------
// A dedicated missing-clock-plan test: a zero-frame or rejected-commit
// callback (block.clockPlan == nullptr) does not fault and leaves the
// gate closed. Hand-builds a synth::AudioBlock with
// clockPlan == nullptr and calls FroggersApp::ProcessBlock directly
// (bypassing SynthRig's own per-block orchestration, which this one check
// does not need) -- output buffers are poisoned with a nonzero value first,
// so a silent gate is proven by an actual zero WRITE, not merely an
// untouched buffer.
// -----------------------------------------------------------------------
TEST_CASE(missing_clock_plan_does_not_fault_and_leaves_gate_closed) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("missing_clock_plan"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    model.PageParameter(synth_froggers::FroggersBankId::Drive, 1).SceneCenter(0) = 0.8f;
    model.PageParameter(synth_froggers::FroggersBankId::Audio, 0).SceneCenter(0) = 0.5f;
    // Deliberately no rig.StartAt(...) -- same "transport never started"
    // starting point as silent_while_transport_is_stopped above.

    constexpr std::size_t kFrames = 32;
    std::vector<float> outL(kFrames, 1.0f);
    std::vector<float> outR(kFrames, 1.0f);
    float* outPtrs[2] = {outL.data(), outR.data()};

    synth::AudioBlock block;
    block.inputs = nullptr;
    block.outputs = outPtrs;
    block.numInputChannels = 0;
    block.numOutputChannels = 2;
    block.numFrames = kFrames;
    block.startSample = 12345;
    block.clockPlan = nullptr;

    rig.Application().ProcessBlock(block);  // must not throw or crash.

    for (const float sample : outL) {
        REQUIRE_TRUE(sample == 0.0f);
    }
    for (const float sample : outR) {
        REQUIRE_TRUE(sample == 0.0f);
    }
}

// -----------------------------------------------------------------------
// Operator report: "Stop doesn't work" -- pressing Stop closes the ASR gate
// (silent_while_transport_is_stopped above) but the delay and
// reverb are feedback structures that keep ringing on their own: the Delay
// bank's feedback runs up to 0.98 (dsp::StereoDelay::Process's own clamp,
// Delay.hpp:135) and the Reverb bank's Hold control pushes its internal
// feedback arbitrarily close to 1.0 (dsp::Reverb::Process, Reverb.hpp:174).
// This pushes both to that self-sustaining extreme (mirroring the pattern
// self_oscillating_comb_and_near_unity_reverb_hold_stays_finite_and_bounded
// above uses to reach a self-sustaining state), confirms it is actually
// ringing, stops the transport mid-ring, and asserts the output falls below
// -60 dBFS within ~250 ms AND stays there for a further stretch -- "decayed
// once" is not "stopped".
// -----------------------------------------------------------------------
TEST_CASE(stopping_transport_silences_self_sustaining_delay_and_reverb) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("stop_silences_delay_reverb"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();

    synth::Parameter& vco1Pitch = model.PageParameter(synth_froggers::FroggersBankId::Audio, 0);
    vco1Pitch.SceneCenter(0) = 0.5f;  // nonzero VCO level, so there's signal to excite the tanks.
    model.PageParameter(synth_froggers::FroggersBankId::Drive, 1).SceneCenter(0) = 0.8f;  // nonzero Gain.

    // Delay bank slots: 1=Send, 3=Feedback, 0=Wet/dry. Feedback at 1.0
    // clamps to 0.98 inside StereoDelay::Process (Delay.hpp:818), the
    // near-unity extreme the defect report cites.
    model.PageParameter(synth_froggers::FroggersBankId::Delay, 1).SceneCenter(0) = 1.0f;  // Send.
    model.PageParameter(synth_froggers::FroggersBankId::Delay, 3).SceneCenter(0) = 1.0f;  // Feedback -> 0.98.
    model.PageParameter(synth_froggers::FroggersBankId::Delay, 0).SceneCenter(0) = 1.0f;  // Wet/dry.

    // Reverb bank: Send must be opened too, now that Reverb's tank is only
    // fed through it (Send defaults closed, like Delay's) -- without this
    // the reverb tank never gets excited and "establish self-sustaining
    // ringing in both tanks" below would only be true of Delay's.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 1).SceneCenter(0) = 1.0f;  // Send.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 9).SceneCenter(0) = 0.08f;  // Hold -> moderate.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 0).SceneCenter(0) = 1.0f;  // Wet/dry fully wet.

    rig.StartAt(0);
    std::uint64_t timestamp = 0;  // Mirrors SynthRig's own internal timestamp counter -- see
                                   // this test's comment below on why that tracking is valid.

    // Run long enough to firmly excite and establish self-sustaining ringing
    // in both tanks.
    constexpr std::size_t kExciteBlocks = 300;
    rig.RunBlocks(kExciteBlocks);
    timestamp += kExciteBlocks;
    REQUIRE_TRUE(!rig.SawNaN());

    // Confirm it's actually ringing loudly before trusting the "and stops"
    // half of this test to mean anything.
    rig.ClearOutput();
    constexpr std::size_t kConfirmRingBlocks = 40;
    rig.RunBlocks(kConfirmRingBlocks);
    timestamp += kConfirmRingBlocks;
    const auto& ringingOutput = rig.Output();
    RequireFiniteStereo(ringingOutput);
    const float ringingPeak = PeakAbs(ringingOutput);
    constexpr float kRingingFloorLinear = 1.0e-2f;  // ~ -40 dBFS.
    REQUIRE_TRUE(ringingPeak > kRingingFloorLinear);

    // Stop the transport mid-ring. SynthRig::StopAt pushes a MessageIn::Stop
    // carrying this timestamp onto the UI bus (SynthRig.hpp:176-178);
    // Engine::ProcessBlock drains that bus using each block's own sequential
    // timestamp (Engine.hpp:396, MessageInBus::Pop's `queue_[head].timestamp
    // > timestamp` gate, ParameterModulation.cpp:4013, which is inclusive of
    // equality) BEFORE committing that block's clock plan (Engine.hpp:402),
    // so passing exactly the running tally of blocks already executed via
    // RunBlocks (SynthRig::NextTimestamp() is a private monotonically
    // increasing counter RunBlocks draws from once per block, starting at 0,
    // and StartAt(0) above never drew from it) lines this Stop message up
    // with the very next block RunBlocks executes -- the same shape
    // miniapp_system_tests.cpp's own `rig.StopAt(3); rig.RunBlockAt(3);`
    // (miniapp_system_tests.cpp:1197-1198) relies on.
    rig.StopAt(timestamp);

    // "Falls below -60 dBFS within ~250 ms" means the output has REACHED
    // that floor by the 250 ms mark -- not that it has been below the floor
    // for the entire 250 ms leading up to it. There is a brief, fast-decaying
    // transient right at the stop instant (the ASR release ramps its last
    // few samples to zero over the release knob's own time constant, still
    // feeding a small amount of real signal into the just-reset delay/
    // reverb tanks) that is expected and not itself a bug. So this runs
    // most of the settle window unmeasured, then captures only a short
    // trailing window right at the ~250 ms mark -- and does the same thing again further
    // out for the "stays there" half, rather than taking one peak over the
    // whole span (which would also catch that brief opening transient and
    // never read as silent regardless of how well the fix works).
    // (FroggersApp::Config() sets 48 kHz/256-sample blocks,
    // FroggersAppCore.hpp:196-197). -60 dBFS: 20*log10(x) = -60 -> x = 1.0e-3.
    const auto [settleLeadBlocks, checkWindowBlocks, kBandSilenceFloorLinear] =
        ComputeSilenceSettleWindow(/*settleSeconds=*/0.25);

    rig.RunBlocks(settleLeadBlocks);
    timestamp += settleLeadBlocks;
    rig.ClearOutput();
    rig.RunBlocks(checkWindowBlocks);
    timestamp += checkWindowBlocks;

    REQUIRE_TRUE(!rig.SawNaN());
    const auto& settledOutput = rig.Output();
    RequireFiniteStereo(settledOutput);
    const float settledPeak = PeakAbs(settledOutput);
    REQUIRE_TRUE(settledPeak < kBandSilenceFloorLinear);

    // And it must STAY there -- not merely have decayed once. Run a further
    // stretch, then measure another short trailing window, confirming it's
    // still below the floor (and did not somehow build back up).
    rig.RunBlocks(settleLeadBlocks);
    rig.ClearOutput();
    rig.RunBlocks(checkWindowBlocks);

    REQUIRE_TRUE(!rig.SawNaN());
    const auto& staysSilentOutput = rig.Output();
    RequireFiniteStereo(staysSilentOutput);
    const float staysSilentPeak = PeakAbs(staysSilentOutput);
    REQUIRE_TRUE(staysSilentPeak < kBandSilenceFloorLinear);
}

// Post-just-landed-fix defect: the test above
// (stopping_transport_silences_self_sustaining_delay_and_reverb) sets no
// Envelope-bank parameters, so every VcoAdsrState voice's Release sits at
// its ~0 default (FroggersParameters.hpp's Envelope layout gives Sustain an
// explicit 1.0f default but leaves Attack/Release at ParameterConfig's own
// 0.0f default) -- closing the gate on Stop snaps each voice to Idle almost
// immediately, so the one-shot delay_.ClearBuffers()/reverb_.Reset() at the
// running->stopped edge (FroggersAppCore.hpp's `runStopTeardown()`, the
// immediate AllIdle() branch) is never re-armed by anything and the test
// above is silent on arrival.
//
// This test instead pushes Envelope Release (bank rows 2/5/8, "Release
// VCO1-3" -- FroggersParameters.hpp:160-162) to 1.0, mapping through
// VcoAdsrState::mapRelease (VoiceEnvelope.hpp:94-98) to ~kMaxReleaseSeconds
// (2.5f, VoiceEnvelope.hpp:84) -- so closing the gate on Stop moves every
// voice to Stage::Release (VoiceEnvelope.hpp:139) and keeps it producing a
// slowly-decaying signal for ~2.5 real seconds, feeding delay_/reverb_ well
// after the one-shot reset already fired.
//
// Delay Time (row 0) is additionally pinned to 0.6 (baseSeconds =
// ExpMapCompute(0.001, 2.0, 0.6) ~= 0.0957s per delay cycle -- Delay.hpp:111)
// rather than left at its own 0.0f default (~0.001s/cycle, too short to
// demonstrate the bug within a boundable test window): with feedback
// clamped to 0.98 (Delay.hpp:135), decaying 60 dB unaided takes
// ln(1e-3)/ln(0.98) ~= 342 delay cycles, ~= 342 * 0.0957s ~= 32.7s here --
// squarely the "tens of seconds" the operator's Randomize All report
// describes -- so a settle window shortly after the ~2.5s
// release completes sits deep inside that unaided decay tail and cleanly
// discriminates the one-shot fix (still ringing there) from the
// keep-clearing-until-Idle fix (already silent there).
TEST_CASE(stopping_transport_silences_self_sustaining_delay_and_reverb_with_long_release) {
    Rig rig(/*patchPumpBudgetBlocks=*/64,
            UseScratchRuntimeDataPaths("stop_silences_delay_reverb_long_release"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();

    synth::Parameter& vco1Pitch = model.PageParameter(synth_froggers::FroggersBankId::Audio, 0);
    vco1Pitch.SceneCenter(0) = 0.5f;  // nonzero VCO level, so there's signal to excite the tanks.
    model.PageParameter(synth_froggers::FroggersBankId::Drive, 1).SceneCenter(0) = 0.8f;  // nonzero Gain.

    // Envelope bank: interleaved ADSR, slot = 4*vco + {0:Attack, 1:Decay,
    // 2:Sustain, 3:Release} (FroggersParameters.hpp:162-164), so rows 3/7/11
    // are Release VCO1-3. Sustain (rows 2/6/10) already defaults to 1.0f;
    // Attack (rows 0/4/8) stays at its fast ~0 default.
    model.PageParameter(synth_froggers::FroggersBankId::Envelope, 3).SceneCenter(0) = 1.0f;  // Release VCO1 -> ~2.5s.
    model.PageParameter(synth_froggers::FroggersBankId::Envelope, 7).SceneCenter(0) = 1.0f;  // Release VCO2 -> ~2.5s.
    model.PageParameter(synth_froggers::FroggersBankId::Envelope, 11).SceneCenter(0) = 1.0f;  // Release VCO3 -> ~2.5s.

    // Delay bank slots: 2=Time, 1=Send, 3=Feedback, 0=Wet/dry.
    model.PageParameter(synth_froggers::FroggersBankId::Delay, 2).SceneCenter(0) = 0.6f;  // Time -> ~96ms/cycle.
    model.PageParameter(synth_froggers::FroggersBankId::Delay, 1).SceneCenter(0) = 1.0f;  // Send.
    model.PageParameter(synth_froggers::FroggersBankId::Delay, 3).SceneCenter(0) = 1.0f;  // Feedback -> 0.98.
    model.PageParameter(synth_froggers::FroggersBankId::Delay, 0).SceneCenter(0) = 1.0f;  // Wet/dry.

    // Reverb bank: Send must be opened too, now that Reverb's tank is only
    // fed through it (Send defaults closed, like Delay's) -- see the
    // sibling test above's own comment on this.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 1).SceneCenter(0) = 1.0f;  // Send.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 9).SceneCenter(0) = 0.08f;  // Hold -> moderate.
    model.PageParameter(synth_froggers::FroggersBankId::Reverb, 0).SceneCenter(0) = 1.0f;  // Wet/dry fully wet.

    rig.StartAt(0);
    std::uint64_t timestamp = 0;

    // Run long enough to firmly excite and establish self-sustaining ringing
    // in both tanks (same shape as the sibling test above).
    constexpr std::size_t kExciteBlocks = 300;
    rig.RunBlocks(kExciteBlocks);
    timestamp += kExciteBlocks;
    REQUIRE_TRUE(!rig.SawNaN());

    rig.ClearOutput();
    constexpr std::size_t kConfirmRingBlocks = 40;
    rig.RunBlocks(kConfirmRingBlocks);
    timestamp += kConfirmRingBlocks;
    const auto& ringingOutput = rig.Output();
    RequireFiniteStereo(ringingOutput);
    const float ringingPeak = PeakAbs(ringingOutput);
    constexpr float kRingingFloorLinear = 1.0e-2f;  // ~ -40 dBFS.
    REQUIRE_TRUE(ringingPeak > kRingingFloorLinear);

    rig.StopAt(timestamp);

    // Settle window: 10.5s from the Stop instant -- generous enough that the
    // ~2.5s Release (VcoAdsrState::kMaxReleaseSeconds) has fully finished on
    // every voice (VcoAdsrState::AllIdle()) with roughly 8s to spare for block
    // granularity and the fix's final flush, but far short of the ~32.7s
    // unaided decay tail (see header comment above) the pre-fix one-shot
    // reset leaves behind -- so this window only passes once the delay/
    // reverb tanks are actively being kept clear through the whole release,
    // not merely once they've had "long enough" to ring out on their own.
    // kSampleRateHz/kBlockSizeSamples are kept local (not folded into
    // ComputeSilenceSettleWindow) because this test alone also reuses them
    // below for its own separate afterStopSeconds -> afterStopBlocks
    // conversion (a single-occurrence computation, not part of the 3x
    // duplicated settle-window concept ComputeSilenceSettleWindow() already
    // extracts).
    constexpr double kSampleRateHz = 48000.0;
    constexpr double kBlockSizeSamples = 256.0;
    const auto [settleLeadBlocks, checkWindowBlocks, kBandSilenceFloorLinear] =
        ComputeSilenceSettleWindow(/*settleSeconds=*/10.5);

    // Clear-once-at-AllIdle, not clear-every-block: this is the
    // assertion that actually distinguishes the two policies. Both the
    // old "wipe delay_/reverb_ every block for as long as any voice is
    // releasing" policy and the new "wipe once, only when AllIdle() first
    // turns true" policy leave the tanks (and thus the output, both banks
    // set fully wet above) silent by the 10.5s settle window checked below
    // -- that check alone cannot tell them apart. What it cannot see is
    // that the ~2.5s Release is supposed to keep ringing wet through
    // delay_/reverb_ the whole time it's live, same as it would with the
    // transport still running (RouteAudioSample() feeds them every sample
    // regardless of transport, ProcessBlock's own comment). Under the old
    // policy delay_/reverb_ would already have been wiped within the very
    // first post-stop block and stay wiped every block after, so the
    // output here would already be at/near the settled silence floor; the
    // new policy leaves the tanks alone until AllIdle(), so the release's
    // wet tail must still be clearly audible this soon after Stop, deep
    // inside the 2.5s Release and nowhere near AllIdle. 0.3s post-stop is
    // comfortably past the release's own fast opening transient (see the
    // sibling short-release test's comment on that) while still far short
    // of the ~2.5s it takes every voice to reach Stage::Idle.
    // This assertion is inverted from what it used to be, deliberately.
    //
    // It used to require that the release was STILL AUDIBLY RINGING 0.3s
    // after Stop, as proof that the delay/reverb tanks were not being wiped
    // every block during the release. That protection is now moot and the
    // behaviour it pinned is the bug the operator reported: "the stop button
    // doesnt actually stop all audio in all circumstances."
    //
    // A transport Stop now forces a ~50ms fade independent of the patch's
    // release knob (FroggersAppCore::RouteAudioSample's `releaseKnob` lambda),
    // so by 0.3s every voice has long since reached Stage::Idle, the
    // clear-at-AllIdle has fired, and the tanks are empty. The old
    // "don't wipe every block" concern only ever applied while a long release
    // was still running after Stop — which can no longer happen.
    constexpr double kAfterStopSeconds = 0.3;
    const auto afterStopBlocks =
        static_cast<std::size_t>(std::ceil((kAfterStopSeconds * kSampleRateHz) / kBlockSizeSamples));
    rig.RunBlocks(afterStopBlocks);
    timestamp += afterStopBlocks;
    rig.ClearOutput();
    rig.RunBlocks(checkWindowBlocks);
    timestamp += checkWindowBlocks;

    REQUIRE_TRUE(!rig.SawNaN());
    const auto& afterStopOutput = rig.Output();
    RequireFiniteStereo(afterStopOutput);
    const float afterStopPeak = PeakAbs(afterStopOutput);
    REQUIRE_TRUE(afterStopPeak < kBandSilenceFloorLinear);

    rig.RunBlocks(settleLeadBlocks);
    timestamp += settleLeadBlocks;
    rig.ClearOutput();
    rig.RunBlocks(checkWindowBlocks);
    timestamp += checkWindowBlocks;

    REQUIRE_TRUE(!rig.SawNaN());
    const auto& settledOutput = rig.Output();
    RequireFiniteStereo(settledOutput);
    const float settledPeak = PeakAbs(settledOutput);
    REQUIRE_TRUE(settledPeak < kBandSilenceFloorLinear);

    // And it must STAY there through the (short, near-unity-feedback-decay)
    // remainder -- not merely have decayed once right at the release's tail.
    rig.RunBlocks(settleLeadBlocks);
    rig.ClearOutput();
    rig.RunBlocks(checkWindowBlocks);

    REQUIRE_TRUE(!rig.SawNaN());
    const auto& staysSilentLongReleaseOutput = rig.Output();
    RequireFiniteStereo(staysSilentLongReleaseOutput);
    const float staysSilentLongReleasePeak = PeakAbs(staysSilentLongReleaseOutput);
    REQUIRE_TRUE(staysSilentLongReleasePeak < kBandSilenceFloorLinear);
}

// =========================================================================
// This test pins the stopped-transport override directly -- while the
// transport is stopped, the
// three drive pre-gains (Delay slot 9 "Feedback drive", Reverb slot 10
// "Tank drive", Filter slot 7 "Comb drive") and Freeze (Delay slot 5)
// resolve to their unity/zero effective values regardless of the commanded
// knob, WITHOUT writing to the parameter model, and resuming play restores
// the commanded mapping bit-exactly. Commanded values are deliberately
// pinned to each control's MAX (1.0f) -- the opposite extreme from the
// override's unity/zero target -- so a passing run cannot be a coincidence
// of the override happening to match a default. Comb drive/Feedback drive
// read directly off dsp::Comb::combDrive/dsp::StereoDelay::fbDrive
// (TestFilterComb()/TestDelay(), both already public members storing the
// POST-ExpMapCompute value); Reverb Tank drive and the Freeze knob have no
// such member (Reverb::Process computes tankDrive as a Process()-local, and
// DelayParams::dfrz lives on a RouteAudioSample()-local DelayParams), so
// this override needed TestLastReverbTankDriveKnobEffective()/
// TestLastDelayFreezeKnobEffective() (FroggersAppCore.hpp) added for this
// test.
//
// The Grit case reuses this SAME test rather than adding a new one: Grit
// (Reverb slot 11) joins the override, resolving to 0.0f (its exact
// bit-identical bypass by construction -- dsp::DigitalReorganizer at
// default, Reverb.hpp) -- same "no member to read back" situation as Tank
// drive/Freeze, so TestLastReverbGritKnobEffective() was added the same way.
// =========================================================================
TEST_CASE(stopped_transport_overrides_drive_and_freeze_to_unity_zero_and_resumes_bit_exact) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("stop_overrides_drive_freeze"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    using synth_froggers::FroggersBankId;

    model.PageParameter(FroggersBankId::Filter, 7).SceneCenter(0) = 1.0f;  // Comb drive commanded MAX.
    model.PageParameter(FroggersBankId::Delay, 9).SceneCenter(0) = 1.0f;    // Feedback drive commanded MAX.
    model.PageParameter(FroggersBankId::Reverb, 10).SceneCenter(0) = 1.0f;  // Tank drive commanded MAX.
    model.PageParameter(FroggersBankId::Delay, 5).SceneCenter(0) = 1.0f;    // Freeze commanded MAX.
    // Grit (Reverb slot 11) commanded MAX -- joins the same
    // stopped-state override this test already pins for the other four.
    model.PageParameter(FroggersBankId::Reverb, 11).SceneCenter(0) = 1.0f;  // Grit commanded MAX.
    ApplyPatchNow(rig);

    // Deliberately no rig.StartAt(...) -- transport starts Stopped by
    // default (this file's own header comment, and silent_while_transport_
    // is_stopped above relies on the same starting point). A few blocks so
    // RouteAudioSample() actually executes the commanded-max patch while
    // stopped.
    rig.RunBlocks(4);
    REQUIRE_TRUE(!rig.SawNaN());

    // Effective values: unity/zero, not the commanded max. dsp::Reverb::
    // TankDriveFromKnob is the SAME public static map Reverb::Process
    // itself calls -- reused here rather than re-derived, so this
    // assertion cannot silently drift from the real mapping if that range
    // is ever retuned (a test that retypes a production formula is a
    // second definition site of it).
    REQUIRE_TRUE(rig.Application().TestFilterComb().combDrive == 1.0f);
    REQUIRE_TRUE(rig.Application().TestDelay().fbDrive == 1.0f);
    REQUIRE_TRUE(rig.Application().TestLastReverbTankDriveKnobEffective() == 0.5f);
    REQUIRE_TRUE(dsp::Reverb::TankDriveFromKnob(rig.Application().TestLastReverbTankDriveKnobEffective()) == 1.0f);
    REQUIRE_TRUE(rig.Application().TestLastDelayFreezeKnobEffective() == 0.0f);
    // Grit's effective value reads 0.0f (its own exact bit-identical
    // bypass by construction, dsp/Reverb.hpp:526-527), not the commanded
    // MAX -- same override, joining the four above.
    REQUIRE_TRUE(rig.Application().TestLastReverbGritKnobEffective() == 0.0f);

    // WITHOUT writing to the parameter model: the commanded values set
    // above are exactly what was written, not silently rewritten to the
    // override's unity/zero.
    REQUIRE_TRUE(model.PageParameter(FroggersBankId::Filter, 7).CachedKnobValue(0) == 1.0f);
    REQUIRE_TRUE(model.PageParameter(FroggersBankId::Delay, 9).CachedKnobValue(0) == 1.0f);
    REQUIRE_TRUE(model.PageParameter(FroggersBankId::Reverb, 10).CachedKnobValue(0) == 1.0f);
    REQUIRE_TRUE(model.PageParameter(FroggersBankId::Delay, 5).CachedKnobValue(0) == 1.0f);
    REQUIRE_TRUE(model.PageParameter(FroggersBankId::Reverb, 11).CachedKnobValue(0) == 1.0f);

    // Resume play: the override stops applying and the never-touched
    // commanded MAX values reach the DSP units bit-exactly -- checked
    // against the real mapping formula's own value at knob==1.0, not
    // merely "no longer 1.0f/0.0f".
    rig.StartAt(4);
    rig.RunBlocks(4);
    REQUIRE_TRUE(!rig.SawNaN());

    const float kExpectedMaxDrive = dsp::ExpMapCompute(0.25f, 4.0f, 1.0f);  // same [0.25,4.0] map all three drive pre-gains share.
    REQUIRE_TRUE(rig.Application().TestFilterComb().combDrive == kExpectedMaxDrive);
    REQUIRE_TRUE(rig.Application().TestDelay().fbDrive == kExpectedMaxDrive);
    REQUIRE_TRUE(rig.Application().TestLastReverbTankDriveKnobEffective() == 1.0f);
    REQUIRE_TRUE(dsp::Reverb::TankDriveFromKnob(rig.Application().TestLastReverbTankDriveKnobEffective()) ==
                 kExpectedMaxDrive);
    REQUIRE_TRUE(rig.Application().TestLastDelayFreezeKnobEffective() == 1.0f);
    // Grit's commanded MAX (1.0f) reaches the DSP unit bit-exactly on
    // resume too -- the override was never a write to the parameter model,
    // so `stoppedKnob` just returns `knob(bank, slot)` unchanged the instant
    // `wasTransportRunning_` is true again, same as the other four.
    REQUIRE_TRUE(rig.Application().TestLastReverbGritKnobEffective() == 1.0f);
}

// =========================================================================
// The Comb low-pass's floor tracks 4x the comb's own frequency (so the
// low-pass never sits below the pitch it's filtering), clamped to the
// low-pass's fixed ceiling once that floor would otherwise run past it.
// Drives Comb delay across positions well below and at/past the point
// where the clamp engages, and at each position sweeps the LP knob
// end-to-end: the resulting low-pass alpha must never run backwards as
// the LP knob rises, at any Comb delay position, and the floor this test
// recomputes from the production combFreq formula must never exceed the
// ceiling.
// =========================================================================
TEST_CASE(comb_lowpass_alpha_monotone_across_comb_delay_positions) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("comb_lp_monotone"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    using synth_froggers::FroggersBankId;

    constexpr float kSampleRateHz = 48000.0f;  // Rig()'s default (App::Config().preferredSampleRate).
    constexpr std::array<float, 4> kCombDelayKnobs = {0.5f, 0.85f, 0.95f, 1.0f};
    constexpr std::array<float, 3> kLowPassKnobs = {0.0f, 0.5f, 1.0f};
    const float ceiling = 20000.0f / kSampleRateHz;

    for (const float cmbDly : kCombDelayKnobs) {
        // Same [100,10000]Hz map RouteAudioSample uses for combFreq
        // (FroggersAppCore.hpp), recomputed here so this assertion cannot
        // silently drift from the real mapping if that range is ever
        // retuned.
        const float combFreq = dsp::ExpMapCompute(100.0f / kSampleRateHz, 10000.0f / kSampleRateHz, cmbDly);
        REQUIRE_TRUE(std::min(4.0f * combFreq, ceiling) <= ceiling);

        float previousAlpha = -1.0f;
        for (const float lp : kLowPassKnobs) {
            model.PageParameter(FroggersBankId::Filter, 4).SceneCenter(0) = cmbDly;  // Comb delay.
            model.PageParameter(FroggersBankId::Filter, 6).SceneCenter(0) = lp;      // Comb LP.
            ApplyPatchNow(rig);
            rig.RunBlocks(4);
            REQUIRE_TRUE(!rig.SawNaN());

            const float alpha = rig.Application().TestFilterComb().filter.alpha;
            REQUIRE_TRUE(alpha >= previousAlpha);
            previousAlpha = alpha;
        }
    }
}

// =========================================================================
// Peak Q (Filter slot 2) and Scoop width (slot 10) share the same floor
// raise (0.1 -> 0.4), recomputed here from the production
// ExpMapCompute(0.4, 10.0, knob) call rather than typed-in decimals, so
// this cannot silently drift from the real mapping if that range is ever
// retuned.
// =========================================================================
TEST_CASE(filter_peak_q_and_scoop_width_floors_track_production_formula) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("filter_q_floors"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    using synth_froggers::FroggersBankId;

    for (const float knob : {0.0f, 1.0f}) {
        model.PageParameter(FroggersBankId::Filter, 2).SceneCenter(0) = knob;   // Peak Q.
        model.PageParameter(FroggersBankId::Filter, 10).SceneCenter(0) = knob;  // Scoop width.
        ApplyPatchNow(rig);
        rig.RunBlocks(4);
        REQUIRE_TRUE(!rig.SawNaN());

        const float expected = dsp::ExpMapCompute(0.4f, 10.0f, knob);
        REQUIRE_TRUE(rig.Application().TestFilterPeak().width == expected);
        REQUIRE_TRUE(rig.Application().TestFilterScoopNotch().width == expected);
    }
}

// =========================================================================
// Scoop depth (Filter slot 11) drives the notch's own height with a
// deliberate decreasing exponential (ExpMapCompute(1.0, 0.05, knob)):
// equal knob steps cut equal dB. Recomputes the expected height at each
// step from the production formula (never a typed-in decimal), then
// checks the resulting dB-per-quarter-step figure against the
// preflight-computed -6.505 dB. Scoop (slot 8) stays at its default 0.0
// throughout -- irrelevant to `.height`, which SetHeight sets directly
// from slot 11 alone, but pinned here so the fixture reads unambiguously
// as isolating this curve on its own.
// =========================================================================
TEST_CASE(filter_scoop_depth_curve_is_equal_db_per_quarter_with_exact_endpoints) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("filter_scoop_depth_curve"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    using synth_froggers::FroggersBankId;

    model.PageParameter(FroggersBankId::Filter, 8).SceneCenter(0) = 0.0f;  // Scoop -- default, irrelevant here.

    constexpr std::array<float, 5> kKnobs = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    std::array<float, 5> heights{};
    for (std::size_t i = 0; i < kKnobs.size(); ++i) {
        model.PageParameter(FroggersBankId::Filter, 11).SceneCenter(0) = kKnobs[i];  // Scoop depth.
        ApplyPatchNow(rig);
        rig.RunBlocks(4);
        REQUIRE_TRUE(!rig.SawNaN());
        heights[i] = rig.Application().TestFilterScoopNotch().height;
        REQUIRE_TRUE(heights[i] == dsp::ExpMapCompute(1.0f, 0.05f, kKnobs[i]));
    }

    REQUIRE_TRUE(heights[0] == 1.0f);   // exact bypass at the floor.
    REQUIRE_TRUE(heights[4] == 0.05f);  // exact at the ceiling.

    constexpr double kExpectedDbPerQuarter = -6.505;
    constexpr double kToleranceDb = 0.01;
    std::cout << "  [Scoop depth curve] heights at knob {0, .25, .5, .75, 1}: " << heights[0] << ", " << heights[1]
               << ", " << heights[2] << ", " << heights[3] << ", " << heights[4] << "\n";
    for (std::size_t i = 0; i + 1 < heights.size(); ++i) {
        const double stepDb =
            20.0 * std::log10(static_cast<double>(heights[i + 1]) / static_cast<double>(heights[i]));
        std::cout << "  [Scoop depth curve] quarter " << i << " -> " << i + 1 << ": " << stepDb << " dB\n";
        REQUIRE_TRUE(std::fabs(stepDb - kExpectedDbPerQuarter) < kToleranceDb);
    }
}

// =========================================================================
// The wiring swap: Scoop (Filter slot 8) drives the notch's wet/dry blend
// (scoopMix); Scoop depth (slot 11) drives the notch's own dip (SetHeight,
// pinned separately by the curve test above). Confirms the precondition
// via the same TestFilterScoopNotch().height accessor (so a partially-
// reverted swap -- this half right, the other wrong -- cannot slip past
// silently), then proves Scoop actually moves the output: with a real,
// confirmed dip in place, disabling the blend (Scoop at its floor) must
// differ audibly from applying it in full (Scoop at its ceiling). Scoop
// freq/width stay at their own defaults (100 Hz, Q 0.4 -- about a 3-octave
// bandwidth), reaching well into the 110 Hz fundamental this measures.
// =========================================================================
TEST_CASE(filter_scoop_slot_drives_wet_dry_blend_of_scoop_depths_notch) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("filter_scoop_blend_swap"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    using synth_froggers::FroggersBankId;

    model.PageParameter(FroggersBankId::Filter, 11).SceneCenter(0) = 1.0f;  // Scoop depth ceiling -- a real dip.
    ApplyPatchNow(rig);
    rig.RunBlocks(4);
    REQUIRE_TRUE(!rig.SawNaN());
    REQUIRE_TRUE(rig.Application().TestFilterScoopNotch().height == 0.05f);  // precondition: the dip landed.

    rig.StartAt(0);

    model.PageParameter(FroggersBankId::Filter, 8).SceneCenter(0) = 0.0f;  // Scoop floor -- blend disabled.
    ApplyPatchNow(rig);
    rig.RunBlocks(8);
    rig.ClearOutput();
    rig.RunBlocks(4);
    REQUIRE_TRUE(!rig.SawNaN());
    const double powerAtFloor = GoertzelPower(ExtractChannel(rig.Output(), 0), kAudibleFundamentalsHz[0], 48000.0);

    model.PageParameter(FroggersBankId::Filter, 8).SceneCenter(0) = 1.0f;  // Scoop ceiling -- blend fully applied.
    ApplyPatchNow(rig);
    rig.RunBlocks(8);
    rig.ClearOutput();
    rig.RunBlocks(4);
    REQUIRE_TRUE(!rig.SawNaN());
    const double powerAtCeiling = GoertzelPower(ExtractChannel(rig.Output(), 0), kAudibleFundamentalsHz[0], 48000.0);

    REQUIRE_TRUE(powerAtCeiling < 0.5 * powerAtFloor);
}

// =========================================================================
// Comb delay's tap is fractional now (Comb::Process, FilterFx.hpp): near
// the top of the Comb delay knob (slot 4), where delaySamples is smallest
// and a single whole-sample jump therefore swings pitch the most, adjacent
// FINE knob steps must move the achieved pitch by a small, smooth amount --
// not the old whole-sample stairstep. Recomputes combFreq from the
// production formula (same map comb_lowpass_alpha_monotone_across_comb_
// delay_positions above uses) and reads the actual routed delaySamples
// back via TestFilterComb(), so this also confirms the caller's clamp
// carries the float through without re-truncating it.
// =========================================================================
TEST_CASE(filter_comb_delay_fine_knob_steps_near_ceiling_avoid_whole_sample_pitch_jumps) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("comb_delay_fine_steps"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    using synth_froggers::FroggersBankId;

    auto delaySamplesAt = [&](float cmbDly) -> float {
        model.PageParameter(FroggersBankId::Filter, 4).SceneCenter(0) = cmbDly;  // Comb delay.
        ApplyPatchNow(rig);
        rig.RunBlocks(4);
        REQUIRE_TRUE(!rig.SawNaN());
        return rig.Application().TestFilterComb().delaySamples;
    };

    constexpr float kFineStep = 1.0e-4f;
    constexpr float kNearTop = 1.0f - 4.0f * kFineStep;
    const float dLow = delaySamplesAt(kNearTop);
    const float dHigh = delaySamplesAt(kNearTop + kFineStep);

    // Confirms the readback is genuinely fractional (the caller's clamp no
    // longer re-truncates it) directly, rather than only through the
    // pitch-jump margin below -- a re-truncated caller could otherwise pass
    // that margin trivially whenever these two particular knob values
    // happen to truncate to the same integer.
    REQUIRE_TRUE(dLow != std::floor(dLow));

    const float pitchStep = std::fabs(1.0f / dHigh - 1.0f / dLow);

    // The theoretical worst case the OLD truncating cast produced at this
    // same (smallest, near-ceiling) delay length: the pitch delta a single
    // WHOLE-sample change causes there.
    const float dFloor = std::floor(dLow);
    const float oldWholeSampleJump = std::fabs(1.0f / dFloor - 1.0f / (dFloor + 1.0f));

    std::cout << "  [Comb delay fine steps] delaySamples: " << dLow << " -> " << dHigh
              << "  fine-step pitch jump=" << pitchStep << "  old whole-sample jump=" << oldWholeSampleJump
              << "  ratio=" << (pitchStep / oldWholeSampleJump) << "\n";

    constexpr float kMarginFactor = 0.1f;  // measured well below this; see report.
    REQUIRE_TRUE(pitchStep < kMarginFactor * oldWholeSampleJump);
}

// =========================================================================
// This test pins the forced-release-on-Stop behavior directly, isolated
// from the ramp-progress-floor behavior -- Curve (Envelope
// slot 12) stays at its default 0.0f (the untouched linear ComputeRampStep
// path, no ramp-progress-floor arithmetic even runs), so a pass here can
// only be the forced-release behavior's doing. Attack VCO1 (slot 0) is
// pinned near its own
// ceiling (kMaxAttackSeconds, 0.25f) and the run is stopped well
// inside that 0.25s window, so the voice is still genuinely mid-Attack (not
// yet Hold) at the moment Stop lands. Grace (slot 13) is pinned to its own
// ceiling (kMaxGraceSeconds, 1.0f): WITHOUT the forced-release behavior, a
// pending release
// deliberately defers through Attack/Decay to Hold and only THEN starts
// this 1.0s countdown (VoiceEnvelope.hpp's own Grace comment) -- stage
// completion (>=0.4s remaining Attack + Decay) plus this 1.0s grace puts
// AllIdle, without the forced-release behavior, comfortably past 2s,
// matching this test's own "NOT
// stage-completion + grace (~2s+)" framing. The forced-release behavior
// forces Release
// immediately at the edge instead, bounded only by the existing ~50ms
// kStopFadeReleaseKnob fade -- AllIdle within the fade time (~0.1s).
// =========================================================================
TEST_CASE(stop_forces_release_from_mid_attack_bypassing_grace_and_stage_completion) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("stop_forces_release_mid_attack"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    using synth_froggers::FroggersBankId;

    model.PageParameter(FroggersBankId::Audio, 0).SceneCenter(0) = 0.5f;  // VCO1 pitch.
    model.PageParameter(FroggersBankId::Drive, 1).SceneCenter(0) = 0.8f;  // Gain.
    model.PageParameter(FroggersBankId::Envelope, 0).SceneCenter(0) = 1.0f;   // Attack VCO1: ceiling (~0.25s).
    model.PageParameter(FroggersBankId::Envelope, 13).SceneCenter(0) = 1.0f;  // Grace: ceiling (~1.0s).
    // Curve (slot 12) left at its 0.0f default -- explicit per this test's
    // own "isolated from the ramp-progress-floor behavior" framing, not
    // merely relying on the registered default silently.
    model.PageParameter(FroggersBankId::Envelope, 12).SceneCenter(0) = 0.0f;
    ApplyPatchNow(rig);

    rig.StartAt(0);
    std::uint64_t timestamp = 0;
    const std::size_t blockSize = 256;
    constexpr double kSampleRateHz = 48000.0;
    const auto secondsToBlocks = [&](double seconds) -> std::size_t {
        return static_cast<std::size_t>(std::ceil((seconds * kSampleRateHz) / static_cast<double>(blockSize)));
    };

    // 0.1s: well inside VCO1's 0.25s Attack ceiling, and well inside the
    // 120 BPM default tempo's first (open) gate half (0.25s -- MasterClock::
    // kDefaultTempoBpm), so the gate never closes/reopens under this voice
    // before Stop lands.
    rig.RunBlocks(secondsToBlocks(0.1));
    timestamp += secondsToBlocks(0.1);
    REQUIRE_TRUE(!rig.SawNaN());
    // Precondition this test needs: still genuinely non-idle (mid-Attack),
    // not already settled -- otherwise the AllIdle() check below would pass
    // vacuously regardless of the forced-release behavior.
    REQUIRE_TRUE(!rig.Application().TestAudioAdsr().AllIdle());

    rig.StopAt(timestamp);

    // The forced-release behavior's bound: AllIdle within the fade time,
    // generously budgeted to
    // 0.2s (comfortably above the ~50ms+wet-tail-irrelevant fade, since
    // AllIdle() only measures the ADSR stage, not delay/reverb) -- NOT the
    // pre-fix ~2s+ (stage completion + grace).
    rig.RunBlocks(secondsToBlocks(0.2));
    REQUIRE_TRUE(rig.Application().TestAudioAdsr().AllIdle());
}

// =========================================================================
// This section overturns an earlier claim -- "latching the
// Freeze button while stopped is a no-op on the audio" -- which is
// exactly backwards: the sustained-drive drone the button exists to
// reproduce only ever existed with the transport stopped, and
// earlier changes (forcing Release immediately on Stop, overriding the
// drive and freeze knobs to unity/zero while stopped, and the deleted
// freeze-latch-is-a-no-op test itself) together made it unreachable. The
// old test above
// (deleted) proved the latch was a no-op by engaging it AFTER Stop, onto
// an already-silenced instrument --
// which is still true (nothing to hold once already torn down) but was
// never the operator's scenario and does not exercise the
// latch-holds-through-stop or latch-release-while-stopped requirements at
// all.
// spec.md's own scenario order is latch-THEN-Stop; the three tests below
// follow that order instead.
//
// A further fix: the drone used to be reachable only via Freeze+Stop, and
// the latch stayed armed
// through Stop -- so a button labelled Stop could conditionally sustain
// instead of silence. Freeze is now SELF-CONTAINED: engaging it stops the
// transport itself (FroggersUiSurface.hpp's kFreeze branch pushes
// MessageIn::Stop and calls SetDesiredTransportRunning(false) on engage, the
// same as kStop), so BuildLatchedRingHeldAcrossStop below drives the drone
// through a single Freeze press -- no separate Stop press reaches it
// anymore -- and Stop's own branch now also disarms the latch
// unconditionally. Both handler branches are driven through the real
// DispatchAction -> HandleAction path below (PressFreeze/PressStop), never
// SetFreezeLatched() directly: a direct flag write bypasses the exact
// handler logic the Freeze-stops-the-transport and Stop-disarms-the-latch
// fixes add.
// =========================================================================

// Drives the Freeze/Stop transport BUTTONS through the real UI action path
// (DispatchAction -> FroggersUiSurface::HandleAction) rather than the app
// flags directly -- see the comment above about the Freeze-stops-the-
// transport and Stop-disarms-the-latch fixes for why that distinction
// matters for these particular two buttons.
void PressFreeze(Rig& rig) {
    rig.Application().PortableSurface().DispatchAction(
        synth::ui::Action::Named(synth_froggers::FroggersActions::kFreeze));
}

void PressStop(Rig& rig) {
    rig.Application().PortableSurface().DispatchAction(
        synth::ui::Action::Named(synth_froggers::FroggersActions::kStop));
}

// Drives Play through the same real action path, for the one handler
// branch (Play disarming the freeze latch) the tests above don't yet pin.
void PressPlay(Rig& rig) {
    rig.Application().PortableSurface().DispatchAction(
        synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));
}

// Shared setup for the freeze-holds-the-ring, latch-release, and
// encoder-edit-while-frozen tests below: the SAME self-sustaining-ring
// recipe as
// stopping_transport_silences_self_sustaining_delay_and_reverb above
// (feedback/hold pushed to their near-unity extremes, so there is real
// recirculating energy for the latch to hold), but with the Freeze BUTTON
// pressed WHILE RUNNING -- spec.md's "The Freeze button alone reaches the
// sustained drone" scenario. Leaves the rig stopped-and-latched, with
// the ring already confirmed audible immediately beforehand (the
// ringingPeak check below), so every caller starts from a genuinely live
// drone, not an assumed one.
// Same value the deleted freeze-latch-is-a-no-op test and
// stopping_transport_silences_self_
// sustaining_delay_and_reverb both used for "actually ringing" (~-40 dBFS)
// -- named distinctly from those tests' own LOCAL kRingingFloorLinear so
// this one file-scope constant can be shared by BuildLatchedRingHeldAcrossStop
// and every test below that uses it without colliding with those unrelated
// locals.
constexpr float kFrozenRingFloorLinear = 1.0e-2f;

std::uint64_t BuildLatchedRingHeldAcrossStop(Rig& rig) {
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    using synth_froggers::FroggersBankId;

    // Excites the ring at ~632 Hz regardless of where the pitch ceiling sits.
    const float kRingExcitePitchKnob =
        std::log(632.0f / dsp::Vco::kPitchMinHz) /
        std::log(dsp::Vco::kPitchMaxHz / dsp::Vco::kPitchMinHz);
    model.PageParameter(FroggersBankId::Audio, 0).SceneCenter(0) = kRingExcitePitchKnob;
    model.PageParameter(FroggersBankId::Drive, 1).SceneCenter(0) = 0.8f;
    model.PageParameter(FroggersBankId::Delay, 1).SceneCenter(0) = 1.0f;  // Send.
    model.PageParameter(FroggersBankId::Delay, 3).SceneCenter(0) = 1.0f;  // Feedback -> 0.98.
    model.PageParameter(FroggersBankId::Delay, 0).SceneCenter(0) = 1.0f;  // Wet/dry.
    // Reverb's own Send is deliberately left at its default-closed 0.0f here
    // -- this ring only needs to be Delay's own, and an open Send would let
    // Reverb's independently-sustaining tank (Hold below) dilute measurements
    // callers make against this ring's total peak (encoder_edit_while_frozen_
    // changes_the_output_measurably's own Delay-only reasoning, below).
    model.PageParameter(FroggersBankId::Reverb, 9).SceneCenter(0) = 0.08f;  // Hold -> moderate.
    model.PageParameter(FroggersBankId::Reverb, 0).SceneCenter(0) = 1.0f;   // Wet/dry fully wet.

    rig.StartAt(0);
    std::uint64_t timestamp = 0;

    constexpr std::size_t kExciteBlocks = 300;
    rig.RunBlocks(kExciteBlocks);
    timestamp += kExciteBlocks;
    REQUIRE_TRUE(!rig.SawNaN());

    rig.ClearOutput();
    constexpr std::size_t kConfirmRingBlocks = 40;
    rig.RunBlocks(kConfirmRingBlocks);
    timestamp += kConfirmRingBlocks;
    const float ringingPeak = PeakAbs(rig.Output());
    REQUIRE_TRUE(ringingPeak > kFrozenRingFloorLinear);  // actually ringing before trusting anything below.

    // With the Freeze-stops-the-transport fix, a single Freeze press now
    // both engages the latch AND stops the
    // transport (FroggersUiSurface.hpp's kFreeze branch) -- no separate Stop
    // press needed or wanted here anymore (an earlier version of this
    // helper
    // pushed SetFreezeLatched(true) directly, then a separate rig.StopAt(...);
    // both are wrong now).
    PressFreeze(rig);

    return timestamp;
}

// Freeze pressed while playing, with NO Stop press -- the
// transport reads
// stopped (TransportRunning() false) AND output stays above an audible
// floor PAST the bound an unlatched Stop must meet (stopping_transport_
// silences_self_sustaining_delay_and_reverb's own 0.25s settle window), the
// inverse of that test's own silence assertion. Checked twice (settle mark,
// then a further stretch) to prove the drone HOLDS rather than merely
// decaying slower. Was named ..._before_stop_...; BuildLatchedRingHeldAcrossStop
// no longer presses Stop at all (the Freeze-stops-the-transport fix), so
// the old name is now false --
// renamed rather than left describing a step this test no longer takes.
TEST_CASE(freeze_alone_holds_the_ring_above_an_audible_floor_and_stops_the_transport) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("freeze_alone_holds_ring_stops_transport"));
    BuildLatchedRingHeldAcrossStop(rig);

    const auto [settleLeadBlocks, checkWindowBlocks, kBandSilenceFloorLinear] =
        ComputeSilenceSettleWindow(/*settleSeconds=*/0.25);
    (void)kBandSilenceFloorLinear;  // Not the assertion here -- this test asserts the INVERSE.

    rig.RunBlocks(settleLeadBlocks);
    // The Freeze-stops-the-transport fix: the Freeze press itself (inside
    // BuildLatchedRingHeldAcrossStop,
    // no separate Stop press) must have stopped the transport -- this is
    // the "transport reads stopped" half of this test's two-part claim.
    REQUIRE_TRUE(!rig.Application().TransportRunning());
    rig.ClearOutput();
    rig.RunBlocks(checkWindowBlocks);
    REQUIRE_TRUE(!rig.SawNaN());
    const auto& heldOutput = rig.Output();
    RequireFiniteStereo(heldOutput);
    REQUIRE_TRUE(PeakAbs(heldOutput) > kFrozenRingFloorLinear);  // still audible, past the bound an unlatched Stop must meet.

    // And it must STAY held, not merely still be mid-decay at the first
    // checkpoint -- same "stays there" shape the silence tests use, proving
    // the opposite property.
    rig.RunBlocks(settleLeadBlocks);
    rig.ClearOutput();
    rig.RunBlocks(checkWindowBlocks);
    REQUIRE_TRUE(!rig.SawNaN());
    const auto& stillHeldOutput = rig.Output();
    RequireFiniteStereo(stillHeldOutput);
    REQUIRE_TRUE(PeakAbs(stillHeldOutput) > kFrozenRingFloorLinear);
    REQUIRE_TRUE(!rig.Application().TransportRunning());  // still stopped -- nothing restarted it.
}

// The latch-release-while-stopped scenario is still live under the
// current handling: Freeze pressed again tears
// down and silences, with the transport staying stopped. A second Freeze
// press, releasing the latch while stopped, is the escape hatch out of the
// drone -- it must silence within the SAME bound an unlatched Stop
// guarantees. Now driven
// through PressFreeze() (DispatchAction -> HandleAction) rather than a
// direct SetFreezeLatched(false) call, per the rule that these
// tests exercise the real handler, not the flag.
TEST_CASE(freeze_latch_release_while_stopped_silences_within_the_bound) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("freeze_latch_release_while_stopped_silences"));
    BuildLatchedRingHeldAcrossStop(rig);

    const auto [settleLeadBlocks, checkWindowBlocks, kBandSilenceFloorLinear] =
        ComputeSilenceSettleWindow(/*settleSeconds=*/0.25);

    // Confirm the drone is genuinely still held immediately before
    // releasing the latch -- the positive control this test's silence
    // claim below needs: a "silences" result means nothing if there was
    // nothing sounding to silence.
    rig.RunBlocks(settleLeadBlocks);
    rig.ClearOutput();
    rig.RunBlocks(checkWindowBlocks);
    REQUIRE_TRUE(!rig.SawNaN());
    REQUIRE_TRUE(PeakAbs(rig.Output()) > kFrozenRingFloorLinear);

    // Release the latch with a second Freeze press -- still stopped, no
    // Play in between. Releasing the latch does not start the transport.
    PressFreeze(rig);

    rig.RunBlocks(settleLeadBlocks);
    rig.ClearOutput();
    rig.RunBlocks(checkWindowBlocks);
    REQUIRE_TRUE(!rig.SawNaN());
    const auto& silencedOutput = rig.Output();
    RequireFiniteStereo(silencedOutput);
    REQUIRE_TRUE(PeakAbs(silencedOutput) < kBandSilenceFloorLinear);
    REQUIRE_TRUE(!rig.Application().TransportRunning());  // release never starts the transport.
}

// This pins the behaviour the earlier handling got backwards -- Stop,
// pressed while
// Freeze is engaged and the drone is sustaining, must disarm the latch AND
// silence within the bound, exactly as any other Stop (spec.md's "Stop
// always means stop" scenario). The earlier handling used to SUSTAIN
// instead;
// this is the direct regression test for that bug, distinct from the
// latch-release-while-stopped test above (which exits the drone via a
// second Freeze press, not Stop).
TEST_CASE(stop_disarms_the_latch_and_silences_the_held_drone_within_the_bound) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("stop_disarms_latch_silences_drone"));
    BuildLatchedRingHeldAcrossStop(rig);

    const auto [settleLeadBlocks, checkWindowBlocks, kBandSilenceFloorLinear] =
        ComputeSilenceSettleWindow(/*settleSeconds=*/0.25);

    // Positive control: the drone is genuinely live immediately before
    // Stop.
    rig.RunBlocks(settleLeadBlocks);
    rig.ClearOutput();
    rig.RunBlocks(checkWindowBlocks);
    REQUIRE_TRUE(!rig.SawNaN());
    REQUIRE_TRUE(PeakAbs(rig.Output()) > kFrozenRingFloorLinear);
    REQUIRE_TRUE(rig.Application().FreezeLatched());  // still latched right before Stop.

    PressStop(rig);

    rig.RunBlocks(settleLeadBlocks);
    rig.ClearOutput();
    rig.RunBlocks(checkWindowBlocks);
    REQUIRE_TRUE(!rig.SawNaN());
    const auto& silencedOutput = rig.Output();
    RequireFiniteStereo(silencedOutput);
    REQUIRE_TRUE(PeakAbs(silencedOutput) < kBandSilenceFloorLinear);
    REQUIRE_TRUE(!rig.Application().FreezeLatched());  // Stop disarms the latch.
    REQUIRE_TRUE(!rig.Application().TransportRunning());
}

// No sequence of Freeze/Stop presses may leave the
// instrument sounding after a Stop -- exercises the three sequences
// spec.md names at minimum, each on a fresh rig with the same
// self-sustaining
// recipe BuildLatchedRingHeldAcrossStop's ring uses (inlined here rather
// than reusing that helper, since two of the three sequences below need a
// DIFFERENT press order than the helper's own "Freeze once" shape).
void RequireSilentAfter(Rig& rig, const char* label) {
    const auto [settleLeadBlocks, checkWindowBlocks, kBandSilenceFloorLinear] =
        ComputeSilenceSettleWindow(/*settleSeconds=*/0.25);
    rig.RunBlocks(settleLeadBlocks);
    rig.ClearOutput();
    rig.RunBlocks(checkWindowBlocks);
    REQUIRE_TRUE(!rig.SawNaN());
    const auto& output = rig.Output();
    RequireFiniteStereo(output);
    const float peak = PeakAbs(output);
    std::cout << "Freeze/Stop sequence " << label << ": peak after final Stop=" << peak << "\n";
    REQUIRE_TRUE(peak < kBandSilenceFloorLinear);
    REQUIRE_TRUE(!rig.Application().FreezeLatched());
    REQUIRE_TRUE(!rig.Application().TransportRunning());
}

TEST_CASE(no_freeze_stop_press_sequence_leaves_the_instrument_sounding_after_stop) {
    // Sequence 1: Freeze -> Stop (engage the drone, then Stop over it).
    {
        Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("freeze_stop_sequence_1"));
        BuildLatchedRingHeldAcrossStop(rig);  // presses Freeze once, confirms the ring first.
        PressStop(rig);
        RequireSilentAfter(rig, "Freeze->Stop");
    }
    // Sequence 2: Freeze -> Freeze -> Stop (engage, release via a second
    // Freeze press, then Stop on an already-unlatched-and-silent instrument
    // -- Stop must still be a no-op-safe unconditional silence, not assume
    // something is left to tear down).
    {
        Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("freeze_stop_sequence_2"));
        BuildLatchedRingHeldAcrossStop(rig);
        PressFreeze(rig);  // release.
        PressStop(rig);
        RequireSilentAfter(rig, "Freeze->Freeze->Stop");
    }
    // Sequence 3: Stop -> Freeze -> Stop (Stop while already unlatched and
    // playing, then Freeze re-engages and re-stops the transport, holding a
    // fresh drone, then a second Stop must disarm and silence it again).
    {
        Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("freeze_stop_sequence_3"));
        synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
        using synth_froggers::FroggersBankId;
        model.PageParameter(FroggersBankId::Audio, 0).SceneCenter(0) = 0.5f;
        model.PageParameter(FroggersBankId::Drive, 1).SceneCenter(0) = 0.8f;
        model.PageParameter(FroggersBankId::Delay, 1).SceneCenter(0) = 1.0f;
        model.PageParameter(FroggersBankId::Delay, 3).SceneCenter(0) = 1.0f;
        model.PageParameter(FroggersBankId::Delay, 0).SceneCenter(0) = 1.0f;
        model.PageParameter(FroggersBankId::Reverb, 1).SceneCenter(0) = 1.0f;  // Send.
        model.PageParameter(FroggersBankId::Reverb, 9).SceneCenter(0) = 0.08f;
        model.PageParameter(FroggersBankId::Reverb, 0).SceneCenter(0) = 1.0f;

        rig.StartAt(0);
        rig.RunBlocks(300);
        REQUIRE_TRUE(!rig.SawNaN());

        PressStop(rig);  // Stop while unlatched and playing: must silence, same as any other Stop.
        RequireSilentAfter(rig, "Stop (first, unlatched)");

        // Play again, re-build the ring, then Freeze re-engages and
        // re-stops the transport, holding a fresh drone.
        rig.StartAt(0);
        rig.RunBlocks(300);
        REQUIRE_TRUE(!rig.SawNaN());
        rig.ClearOutput();
        rig.RunBlocks(40);
        REQUIRE_TRUE(PeakAbs(rig.Output()) > kFrozenRingFloorLinear);  // positive control: really ringing again.
        PressFreeze(rig);
        REQUIRE_TRUE(rig.Application().FreezeLatched());

        PressStop(rig);
        RequireSilentAfter(rig, "Stop->Freeze->Stop (final)");
    }
}

// Releasing Freeze must NOT restart the transport --
// the operator resumes with Play, not by releasing the latch.
// (Operator-reported 2026-08-17, found in the built app: "why does
// clicking play
// not de-select freeze".) Play disarms the latch for the same reason Stop
// does -- and more urgently, because a latched Freeze holds the voice gate
// open unconditionally (FroggersAppCore's `setGate(gateOpen ||
// FreezeLatched())`). Starting the transport with the latch still engaged
// would run the sequencer with every voice pinned sustaining and the delay
// still at its latch overdrive, so Play would not actually return the
// instrument to playing.
//
// This was the single behaviour the Freeze-stops-the-transport and
// Stop-disarms-the-latch fix's own tests did not pin: the
// Play branch was written last, by hand, and every other Freeze/Stop
// sequence had a test while this one did not. Without this case, deleting
// `SetFreezeLatched(false)` from the kPlay handler leaves the whole suite
// green.
TEST_CASE(play_disarms_the_freeze_latch_and_returns_the_voice_gate_to_the_transport) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("play_disarms_freeze_latch"));
    BuildLatchedRingHeldAcrossStop(rig);

    rig.RunBlocks(4);
    // Positive control: the preconditions this test needs must actually
    // hold before Play is pressed, or "the latch cleared" would be
    // provable by an instrument that was never latched in the first place.
    REQUIRE_TRUE(rig.Application().FreezeLatched());
    REQUIRE_TRUE(!rig.Application().TransportRunning());

    PressPlay(rig);
    rig.RunBlocks(4);

    REQUIRE_TRUE(!rig.Application().FreezeLatched());   // the fix under test.
    REQUIRE_TRUE(rig.Application().TransportRunning());  // Play still starts the transport.
    REQUIRE_TRUE(!rig.SawNaN());
}

TEST_CASE(releasing_freeze_does_not_restart_the_transport) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("releasing_freeze_does_not_restart_transport"));
    BuildLatchedRingHeldAcrossStop(rig);

    rig.RunBlocks(4);
    REQUIRE_TRUE(!rig.Application().TransportRunning());  // Freeze engage stopped it.

    PressFreeze(rig);  // release.
    rig.RunBlocks(4);
    REQUIRE_TRUE(!rig.Application().FreezeLatched());
    REQUIRE_TRUE(!rig.Application().TransportRunning());  // still stopped -- release did not restart it.
}

// Parameter edits stay live while frozen -- an encoder edit made
// while the drone is held must change the output measurably, with a
// positive control proving the drone was live immediately before the edit
// (a null result from an already-dead instrument would be void). Edits
// Delay Wet/dry (bank Delay slot 0, DelayParams::dmix) from fully wet to
// fully dry -- a post-gain crossfade applied every sample to the delay's
// own (already self-sustaining) output, so its effect on an ALREADY-
// ringing signal is immediate, not dependent on new input reaching the
// tank.
TEST_CASE(encoder_edit_while_frozen_changes_the_output_measurably) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("encoder_edit_while_frozen_changes_output"));
    BuildLatchedRingHeldAcrossStop(rig);
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    using synth_froggers::FroggersBankId;

    const auto [settleLeadBlocks, checkWindowBlocks, kBandSilenceFloorLinear] =
        ComputeSilenceSettleWindow(/*settleSeconds=*/0.25);
    (void)kBandSilenceFloorLinear;

    rig.RunBlocks(settleLeadBlocks);
    rig.ClearOutput();
    rig.RunBlocks(checkWindowBlocks);
    REQUIRE_TRUE(!rig.SawNaN());
    const float peakBeforeEdit = PeakAbs(rig.Output());
    // Positive control: the drone must be genuinely audible right here,
    // immediately before the edit below, or a measured change (or lack of
    // one) proves nothing.
    REQUIRE_TRUE(peakBeforeEdit > kFrozenRingFloorLinear);

    // The "encoder edit" -- still latched, still stopped, no Play in
    // between. Deliberately NOT using ApplyPatchNow/ComputeAllParameters()
    // here (same reasoning as patch_change_still_reaches_dsp_while_
    // transport_stopped's own comment, above in this file): that helper
    // writes CachedKnobValue() directly, bypassing parameters_.
    // ProcessSample()'s smoothed per-sample Compute entirely -- so it would
    // still pass even if THIS task's actual claim (ProcessSample() itself
    // stays ungated by transport state) were broken. A plain SceneCenter
    // write exercises the real path an operator's encoder turn uses.
    model.PageParameter(FroggersBankId::Delay, 0).SceneCenter(0) = 0.0f;  // Wet/dry: fully wet -> fully dry.

    // The periodic smoothed Compute (alpha 0.0994 every 16 samples,
    // this file's own ApplyPatchNow comment) converges geometrically;
    // patch_change_still_reaches_dsp_while_transport_stopped's own comment
    // works the math for 50 blocks x 256 samples -- comfortably converged
    // well under float precision. Run that many BEFORE measuring so the
    // "after" window reads the settled value, not a mid-crossfade one.
    constexpr std::size_t kConvergeBlocks = 50;
    rig.RunBlocks(kConvergeBlocks);

    rig.ClearOutput();
    rig.RunBlocks(checkWindowBlocks);
    REQUIRE_TRUE(!rig.SawNaN());
    const auto& afterEditOutput = rig.Output();
    RequireFiniteStereo(afterEditOutput);
    const float peakAfterEdit = PeakAbs(afterEditOutput);

    // Measurable, not merely different in the noise: a self-sustaining ring
    // is not a pure tone, so consecutive non-overlapping windows differ by
    // a small amount even with NO edit at all -- measured directly (with
    // parameters_.ProcessSample() deliberately gated behind
    // `transportRunningNow`, so the edit above could not reach the DSP):
    // diff == 0.00627667 over this same 50-block gap. The real edit (Mix
    // 1.0 -> 0.0 collapsing delayOut toward the near-silent dry signal,
    // dsp/Delay.hpp's ToReverbMono) measures diff == 0.537887 -- ~86x that
    // noise floor. kMeasurableChangeLinear sits an order of magnitude above
    // the measured noise floor and comfortably below the measured true
    // effect, so this cannot pass on drift alone.
    const float peakDiff = std::fabs(peakBeforeEdit - peakAfterEdit);
    std::cout << "encoder-edit-while-frozen: peakBeforeEdit=" << peakBeforeEdit
              << " peakAfterEdit=" << peakAfterEdit << " diff=" << peakDiff << "\n";
    constexpr float kMeasurableChangeLinear = 0.05f;  // noise floor 0.0063, true effect 0.538 (both measured, comment above).
    REQUIRE_TRUE(peakDiff > kMeasurableChangeLinear);
}

// =========================================================================
// This pins the operator-ordered gate on modulation_.Step() --
// NOT the fix for a static DC seed that could never have been removed by
// freezing modulation (see FroggersAppCore.hpp's own comment at the call
// site for why). This proves the actual behaviour change: with
// the transport stopped, a genuinely free-running modulation source holds
// its last value instead of continuing to update. Uses kModSlotNoise, one
// of the 8 slots the gate actually stops (kModSlotRandomSh6, kModSlotVco1/
// 2/3Audio, kModSlotVco1/2/3Ef, kModSlotNoise) -- the simplest of the 8, a
// fresh synth::NoiseModulatorProcessor::Process() draw (random_.
// UniformOpen01(), FroggersModulation.hpp) every Step() call, needing no
// patch/knob setup to prove liveness.
//
// Deliberately lives here, not in FroggersModulationTests.cpp: that file's
// own Fixture calls FroggersModulationSlate::Step() directly (its own
// header comment: "no Engine/SynthRig needed"), bypassing
// FroggersAppCore::ProcessBlock entirely -- exactly where this gate lives --
// so it structurally cannot observe this change. This file already drives
// ProcessBlock through the real synth_rig::SynthRig<FroggersApp> path (see
// e.g. the stopping_transport_silences_self_sustaining_delay_and_reverb
// tests above), so it is the correct home, following the same TEST_CASE/
// REQUIRE_TRUE convention FroggersModulationTests.cpp also uses (each test
// file in this directory defines its own copy; there is no shared header).
//
// A negative result (holds while stopped) requires a positive control
// (the SAME rig, SAME source, proven to move while running) in the SAME
// test, or the held-value read is meaningless. Both numbers are printed,
// not just asserted.
TEST_CASE(free_running_modulation_source_holds_while_stopped_with_positive_control) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("s1a2_source_holds_while_stopped"));
    const auto noiseValue = [&] {
        return rig.Application().Modulation().SourceValue(synth_froggers::kModSlotNoise);
    };

    // --- Positive control: transport RUNNING, sampled at every block
    // boundary (RunBlocks(1) chunking is bit-exact with an un-chunked call --
    // the same fact this file's own
    //  master_limiter_stays_at_unity_under_live_modulation
    // test above both already rely on). kModSlotNoise is a fresh open-(0,1)
    // draw every Step() call, so any nonzero spread over 40 independent
    // draws (short of an astronomically unlikely all-equal run) proves
    // liveness.
    rig.StartAt(0);
    std::uint64_t timestamp = 0;
    constexpr std::size_t kLiveBlocks = 40;
    float liveMin = std::numeric_limits<float>::infinity();
    float liveMax = -std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < kLiveBlocks; ++i) {
        rig.RunBlocks(1);
        ++timestamp;
        const float v = noiseValue();
        liveMin = std::min(liveMin, v);
        liveMax = std::max(liveMax, v);
    }
    std::cout << "Modulation gate positive control -- transport RUNNING, kModSlotNoise, " << kLiveBlocks
              << " block-boundary samples: min=" << liveMin << " max=" << liveMax
              << " range=" << (liveMax - liveMin) << "\n";
    // A void-liveness threshold of 1e-3f:
    // It is comfortably above float noise, and a
    // uniform-(0,1) draw over 40 samples spans on the order of the full
    // [0,1) range in practice, so this margin is not close.
    constexpr float kLivenessThreshold = 1.0e-3f;
    REQUIRE_TRUE((liveMax - liveMin) > kLivenessThreshold);

    // --- Stop the transport; the source must now hold. ---
    rig.StopAt(timestamp);
    rig.RunBlocks(1);  // One block past the Stop message's own drain boundary.
    ++timestamp;

    const float heldValue = noiseValue();
    constexpr std::size_t kHeldBlocks = 40;
    for (std::size_t i = 0; i < kHeldBlocks; ++i) {
        rig.RunBlocks(1);
        REQUIRE_TRUE(noiseValue() == heldValue);
    }
    std::cout << "Modulation gate held value -- transport STOPPED, kModSlotNoise, constant at " << heldValue
              << " across " << kHeldBlocks << " further block-boundary samples\n";
}

// Regression check for the modulation gate above: `parameters_.ProcessSample()`
// stays UNGATED by
// design (FroggersAppCore.hpp's own comment on the Step() call site) --
// gating it would freeze knob edits and Randomize All until Play, a worse
// bug than the one the modulation gate fixes. This proves that stays true:
// a raw
// SceneCenter write (the "commanded value" convention FroggersModulation
// Tests.cpp/FroggersParameterModelTests.cpp also use) still reaches
// CachedKnobValue() -- the value RouteAudioSample's knob()/vcoDrive actually
// read -- through the ordinary ProcessBlock path, with the transport never
// started at all (this rig's default state, per this file's own header
// comment: "The rig's transport starts Stopped by default").
// The Delay bank's Wet/dry crossfades the dry signal against a wet path that
// Send feeds, and Send defaults to zero (dsp/Delay.hpp's DelayParams). So at
// Wet/dry maximum the crossfade lands on a path carrying nothing and the
// instrument disappears -- a mute knob wearing a mix label, and the first
// thing a new listener meets, because the default patch is what ships.
//
// Measured at the output rather than at the mix value: the claim being made
// is that the instrument is still audible, and a mix value cannot say that.
// The ceiling half of the same requirement the Reverb bank already carries.
// Read off what the DSP was handed, not off the constant it came from.
// Both Width controls were mathematically inert before the fold moved to the
// output, and provably so rather than approximately: the reverb summed
// wetL+wetR, and with mid == 0.5(aOut+bOut) that sum is 2*mid at EVERY width,
// so the knob could not change a single sample. The delay's cross-feed
// cancels in a sum for the same reason. Carrying the pair is what turns both
// into controls, so this is the test that says they now do something.
//
// Measured as a channel DIFFERENCE, not as a change in peak: a control that
// only moved the level would pass a peak test while still producing no image.
float ChannelDifference(const std::vector<Rig::OutputFrame>& frames) {
    const std::vector<float> left = ExtractChannel(frames, 0);
    const std::vector<float> right = ExtractChannel(frames, 1);
    float worst = 0.0f;
    const std::size_t n = std::min(left.size(), right.size());
    for (std::size_t ix = 0; ix < n; ++ix) {
        worst = std::max(worst, std::fabs(left[ix] - right[ix]));
    }
    return worst;
}

TEST_CASE(reverb_width_produces_a_stereo_image) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("reverb_width_image"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    using synth_froggers::FroggersBankId;

    // Fully wet, so the tank's own pair is what reaches the output, and Width
    // pushed away from centre. Send must be open too -- it defaults closed,
    // like Delay's own Send, and a closed Send leaves the tank unfed, which
    // would read as no image rather than an inert Width.
    model.PageParameter(FroggersBankId::Reverb, 1).SceneCenter(0) = 1.0f;  // Send.
    model.PageParameter(FroggersBankId::Reverb, 0).SceneCenter(0) = 1.0f;  // Wet/dry.
    model.PageParameter(FroggersBankId::Reverb, 6).SceneCenter(0) = 1.0f;  // Stereo width.
    ApplyPatchNow(rig);
    // StartAt(0), matching the default-patch audibility test above: the ASR
    // gate follows the transport's quarter-note pulse, and starting further
    // in leaves the window this samples silent -- in which case a zero
    // channel difference would mean nothing at all.
    rig.StartAt(0);
    rig.RunBlocks(16);
    rig.ClearOutput();
    rig.RunBlocks(8);
    REQUIRE_TRUE(!rig.SawNaN());
    RequireFiniteStereo(rig.Output());
    // The instrument must actually be sounding, or "the channels do not
    // differ" is a statement about silence.
    REQUIRE_TRUE(PeakAbs(rig.Output()) > 1.0e-4f);
    const float wide = ChannelDifference(rig.Output());

    // POSITIVE CONTROL: the same patch with Width at centre must NOT produce
    // a difference. Without this, "the channels differ" could be true of a
    // build that simply decorrelated them for some unrelated reason.
    model.PageParameter(FroggersBankId::Reverb, 6).SceneCenter(0) = 0.0f;
    ApplyPatchNow(rig);
    rig.RunBlocks(16);
    rig.ClearOutput();
    rig.RunBlocks(8);
    const float centred = ChannelDifference(rig.Output());

    std::cout << "  [reverb width] width 1.0 -> channel diff " << wide
              << ", width 0.0 -> " << centred << "\n";
    constexpr float kAudibleDifference = 1.0e-4f;
    REQUIRE_TRUE(wide > kAudibleDifference);
    REQUIRE_TRUE(wide > centred);
}

TEST_CASE(delay_stereo_width_produces_a_stereo_image) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("delay_width_image"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    using synth_froggers::FroggersBankId;

    // Send must be up: the delay line is what carries the image, and it is
    // fed only through Send, which defaults to zero.
    model.PageParameter(FroggersBankId::Delay, 1).SceneCenter(0) = 0.8f;   // Send.
    model.PageParameter(FroggersBankId::Delay, 3).SceneCenter(0) = 0.7f;   // Feedback.
    model.PageParameter(FroggersBankId::Delay, 0).SceneCenter(0) = 1.0f;   // Wet/dry.
    model.PageParameter(FroggersBankId::Delay, 4).SceneCenter(0) = 1.0f;   // Stereo width.
    ApplyPatchNow(rig);
    // StartAt(0), matching the default-patch audibility test above: the ASR
    // gate follows the transport's quarter-note pulse, and starting further
    // in leaves the window this samples silent -- in which case a zero
    // channel difference would mean nothing at all.
    rig.StartAt(0);
    rig.RunBlocks(16);
    rig.ClearOutput();
    rig.RunBlocks(8);
    REQUIRE_TRUE(!rig.SawNaN());
    RequireFiniteStereo(rig.Output());
    // The instrument must actually be sounding, or "the channels do not
    // differ" is a statement about silence.
    REQUIRE_TRUE(PeakAbs(rig.Output()) > 1.0e-4f);
    const float wide = ChannelDifference(rig.Output());

    std::cout << "  [delay width] Send 0.8, width 1.0 -> channel diff " << wide << "\n";
    constexpr float kAudibleDifference = 1.0e-4f;
    REQUIRE_TRUE(wide > kAudibleDifference);
}

TEST_CASE(delay_wet_mix_ceiling_leaves_dry_signal_and_keeps_its_full_travel) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("delay_wet_ceiling"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    using synth_froggers::FroggersBankId;

    model.PageParameter(FroggersBankId::Delay, 0).SceneCenter(0) = 1.0f;  // Wet/dry commanded MAX.
    ApplyPatchNow(rig);
    rig.StartAt(120.0);
    rig.RunBlocks(4);
    REQUIRE_TRUE(!rig.SawNaN());

    const float mixAtMax = rig.Application().TestLastDelayWetMixEffective();
    // dsp::StereoDelay::ToStereo blends with the same equal-power law Reverb
    // does (`dry*cos(theta) + wet*sin(theta)`, theta = mix*acos(kMinDryLevel),
    // both scaled further by WetAuthority() in [0,1] before reaching that
    // formula), so `cos(mixAtMax*thetaMax)` is a lower bound on the true dry
    // share for the same reason the Reverb ceiling test's own comment gives
    // (cos decreasing over [0, thetaMax], actual mix <= mixAtMax). No more
    // knob-level ceiling scales `mixAtMax` down from 1.0 -- the floor now
    // lives entirely in thetaMax (dsp::kMinDryLevel's own comment,
    // Limiter.hpp) -- so at full authority this lower bound is also the
    // exact floor: mixAtMax == 1.0 puts theta at exactly thetaMax.
    const float thetaMax = std::acos(synth_froggers::dsp::kMinDryLevel);
    const float dryShare = std::cos(mixAtMax * thetaMax);
    std::cout << "  [delay wet ceiling] knob 1.0 -> mix " << mixAtMax << ", dry share (lower bound) " << dryShare
              << "\n";
    // Same float-representation epsilon as the Reverb ceiling: 0.3 has no
    // exact binary form, so the floor stores as 0.30000001 -- the epsilon
    // documents that this is float representation slop, not slack in the
    // requirement, in either direction.
    REQUIRE_TRUE(dryShare >= 0.30f - 1e-6f);

    // The ceiling is on the mapped value, not the knob range, so half the
    // knob is half the ceiling.
    model.PageParameter(FroggersBankId::Delay, 0).SceneCenter(0) = 0.5f;
    ApplyPatchNow(rig);
    rig.RunBlocks(4);
    const float mixAtHalf = rig.Application().TestLastDelayWetMixEffective();
    REQUIRE_TRUE(std::fabs(mixAtHalf - mixAtMax * 0.5f) < 1e-5f);

    // POSITIVE CONTROL: the knob really did move, so a high dry share is a
    // ceiling doing its job rather than a control that never left zero.
    REQUIRE_TRUE(mixAtMax > 0.0f);
    model.PageParameter(FroggersBankId::Delay, 0).SceneCenter(0) = 0.0f;
    ApplyPatchNow(rig);
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().TestLastDelayWetMixEffective() == 0.0f);
}

TEST_CASE(delay_wet_mix_at_maximum_leaves_the_default_patch_audible) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("delay_wet_audible"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    using synth_froggers::FroggersBankId;

    // Send is left at its default. That is the whole point: this is the patch
    // the instrument ships with, not one contrived to have an empty delay line.
    const synth::Parameter& send = model.PageParameter(FroggersBankId::Delay, 1);
    const float sendDefault = send.SceneCenter(0);

    model.PageParameter(FroggersBankId::Delay, 0).SceneCenter(0) = 1.0f;  // Wet/dry commanded MAX.
    ApplyPatchNow(rig);
    rig.StartAt(0);
    rig.RunBlocks(8);
    rig.ClearOutput();
    rig.RunBlocks(4);

    REQUIRE_TRUE(!rig.SawNaN());
    const auto& output = rig.Output();
    RequireFiniteStereo(output);
    constexpr float kEpsilon = 1.0e-4f;
    const float peakAtMaxWet = PeakAbs(output);
    std::cout << "  [delay wet/dry] Send default " << sendDefault << ", Wet/dry 1.0 -> peak "
              << peakAtMaxWet << "\n";

    // POSITIVE CONTROL: the same rig with Wet/dry at zero must be audible, so
    // a silent result above is the Wet/dry removing the instrument rather
    // than a rig that was never making sound in the first place.
    model.PageParameter(FroggersBankId::Delay, 0).SceneCenter(0) = 0.0f;
    ApplyPatchNow(rig);
    rig.RunBlocks(8);
    rig.ClearOutput();
    rig.RunBlocks(4);
    const float peakAtDry = PeakAbs(rig.Output());
    std::cout << "  [delay wet/dry] Wet/dry 0.0 -> peak " << peakAtDry << "\n";
    REQUIRE_TRUE(peakAtDry > kEpsilon);

    REQUIRE_TRUE(peakAtMaxWet > kEpsilon);
}

TEST_CASE(reverb_wet_mix_always_leaves_at_least_thirty_percent_dry) {
    // The property the dry floor exists to guarantee, read off what the DSP
    // was actually handed rather than off the constant it was computed from.
    // Reverb blends `dry*cos(theta) + wet*sin(theta)`, theta =
    // mix*acos(kMinDryLevel) -- an equal-power law (dsp/Reverb.hpp's
    // `mixedL`/`mixedR` comment), not the linear `(1-mix)*dry + mix*wet`
    // this pin used to assert. `mix` itself is
    // `mixKnob01 * wetAuthority.Authority()`, and Authority() is in [0,1], so
    // the mix actually reaching the crossfade is never more than `mixAtMax`
    // below (the pre-authority, knob-mapped value TestLastReverbWetMixEffective
    // reports, no longer scaled down by any knob-level constant -- the floor
    // now lives entirely in thetaMax). cos is monotonically DECREASING over
    // [0, thetaMax], so `cos(mixAtMax*thetaMax)` is a valid lower bound on
    // the true dry share no matter how much authority the wet path has
    // actually earned by the time this measurement runs.
    //
    // A test that asserted `dsp::kMinDryLevel == 0.30f` would restate the
    // edit and would keep passing if the floor were later applied to the
    // wrong thing, or stopped being applied at all.
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("reverb_wet_ceiling"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    using synth_froggers::FroggersBankId;

    model.PageParameter(FroggersBankId::Reverb, 0).SceneCenter(0) = 1.0f;  // Wet/dry commanded MAX.
    ApplyPatchNow(rig);
    rig.StartAt(120.0);
    rig.RunBlocks(4);
    REQUIRE_TRUE(!rig.SawNaN());

    const float mixAtMax = rig.Application().TestLastReverbWetMixEffective();
    const float thetaMax = std::acos(synth_froggers::dsp::kMinDryLevel);
    const float dryShare = std::cos(mixAtMax * thetaMax);
    std::cout << "  [reverb wet ceiling] knob 1.0 -> mix " << mixAtMax << ", dry share (lower bound) " << dryShare
              << "\n";
    REQUIRE_TRUE(dryShare >= 0.30f - 1e-6f);

    // The control still sweeps its whole travel: the ceiling is on the mapped
    // value, not on the knob's range, so half the knob is half the ceiling.
    model.PageParameter(FroggersBankId::Reverb, 0).SceneCenter(0) = 0.5f;
    ApplyPatchNow(rig);
    rig.RunBlocks(4);
    const float mixAtHalf = rig.Application().TestLastReverbWetMixEffective();
    REQUIRE_TRUE(std::fabs(mixAtHalf - mixAtMax * 0.5f) < 1e-5f);

    // POSITIVE CONTROL: the instrument really was asked for maximum wetness,
    // so "the dry share is high" is a ceiling doing its job and not a knob
    // that never moved.
    REQUIRE_TRUE(mixAtMax > 0.0f);
    model.PageParameter(FroggersBankId::Reverb, 0).SceneCenter(0) = 0.0f;
    ApplyPatchNow(rig);
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().TestLastReverbWetMixEffective() == 0.0f);
}

TEST_CASE(patch_change_still_reaches_dsp_while_transport_stopped) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("s1a2_patch_reaches_dsp_while_stopped"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    synth::Parameter& driveGain = model.PageParameter(synth_froggers::FroggersBankId::Drive, 1);

    const float before = driveGain.CachedKnobValue(0);
    constexpr float kTarget = 0.8f;
    driveGain.SceneCenter(0) = kTarget;

    // The periodic smoothed Compute (Parameter::ProcessSamplePhase1,
    // alpha 0.0994 every 16 samples -- this file's own ApplyPatchNow comment
    // above) converges geometrically: residual after k updates is
    // (1-0.0994)^k. 50 blocks x 256 samples / 16 samples-per-update = 800
    // updates; (0.9006)^800 underflows float precision many times over.
    // Deliberately NOT using ApplyPatchNow/ComputeAllParameters() here --
    // the point of this test is to exercise the real per-sample path
    // (parameters_.ProcessSample(), called every sample regardless of
    // transport state) rather than the direct one-shot convergence helper.
    constexpr std::size_t kBlocks = 50;
    rig.RunBlocks(kBlocks);

    const float after = driveGain.CachedKnobValue(0);
    std::cout << "Patch-while-stopped regression check -- Drive gain CachedKnobValue(0): before=" << before
              << " target=" << kTarget << " after=" << after << " (transport never started)\n";
    REQUIRE_TRUE(!rig.SawNaN());
    // The write started meaningfully far from target (catches a vacuous
    // "already there" pass) and converged onto it (catches the regression
    // this test exists to rule out: parameters_.ProcessSample() silently
    // gated alongside modulation_.Step()).
    REQUIRE_TRUE(std::fabs(before - kTarget) > 0.1f);
    REQUIRE_TRUE(std::fabs(after - kTarget) < 1.0e-4f);
}

// -----------------------------------------------------------------------
// Where the held energy sits after randomize-then-reset.
//
// A pristine instrument decays to silence; one that has been randomized and
// then reset holds a large level indefinitely. A single broadband number
// cannot say whether that held level is a tone the envelope failed to
// close, or a DC/subsonic offset latched somewhere downstream -- which is
// the same reason this file mandates a band-limited check over RMS
// (GoertzelPower above; a 20 Hz tone is inaudible and still nonzero-RMS,
// per default_patch_has_audible_band_energy_above_150hz's own comment).
// This splits the measurement into the audible fundamentals that test
// asserts on (110/220/330 Hz) and the inaudible ones it contrasts against
// (20/40/60 Hz), per window.
//
// Four operations, a fresh rig each so no arm inherits another's state:
// nothing, randomize only, reset only, and randomize-then-reset. The last
// runs twice, because both requests drain in a FIXED order inside one
// ProcessFrame() -- RandomizeAll, then ResetAll, then
// ComputeAllParameters() guarded by `if (randomizeRan)`
// (FroggersAppCore.hpp) -- so issuing both before a single block runs
// ComputeAllParameters() AFTER the reset, while issuing them a block apart
// does not. Those are different experiments; only the split one is what
// pressing two buttons produces.
//
// This test REPORTS; it does not assert a diagnosis. Its only pass
// condition is the positive control: every arm must be audibly live in its
// first window. An arm silent throughout would match a decayed pristine one
// in the late windows while proving nothing.
// -----------------------------------------------------------------------

// GoertzelPower() returns a length-N DFT magnitude squared, which for a
// sinusoid of amplitude A is (A*N/2)^2. Converting back to A keeps the
// reported numbers on the same linear scale as this file's silence floor
// (kBandSilenceFloorLinear, ComputeSilenceSettleWindow above) instead of an
// N-dependent power the reader has to undo. Powers are summed before the
// conversion (the same way default_patch_has_audible_band_energy_above_150hz
// sums them), which makes the result the root-sum-square of the individual
// component amplitudes.
double BandAmplitude(const std::vector<float>& samples, const std::array<double, 3>& freqsHz,
                     double sampleRateHz) {
    double power = 0.0;
    for (const double freqHz : freqsHz) {
        power += std::max(GoertzelPower(samples, freqHz, sampleRateHz), 0.0);
    }
    return 2.0 * std::sqrt(power) / static_cast<double>(samples.size());
}

enum class EnvelopeArm { Nothing, RandomizeOnly, ResetOnly, RandomizeThenResetSplit, RandomizeThenResetSameBlock };

struct EnvelopeWindow {
    double audible;
    double subsonic;
};

// One arm, start to finish. Settles the rig into a running steady state
// first (the same 8 blocks randomize_all_storm_test_never_blows_out... uses
// before its first draw), applies the operation, warms up, then measures.
std::vector<EnvelopeWindow> MeasureEnvelopeArm(EnvelopeArm arm, const char* scratchName) {
    constexpr double kSampleRateHz = 48000.0;
    constexpr std::size_t kSettleBlocks = 8;
    constexpr std::size_t kWarmUpBlocks = 24;
    constexpr std::size_t kWindows = 12;
    constexpr std::size_t kBlocksPerWindow = 4;

    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths(scratchName));
    rig.StartAt(0);
    rig.RunBlocks(kSettleBlocks);

    switch (arm) {
        case EnvelopeArm::Nothing:
            break;
        case EnvelopeArm::RandomizeOnly:
            rig.Application().RequestRandomizeAll();
            rig.RunBlocks(1);
            break;
        case EnvelopeArm::ResetOnly:
            rig.Application().RequestResetAll();
            rig.RunBlocks(1);
            break;
        case EnvelopeArm::RandomizeThenResetSplit:
            rig.Application().RequestRandomizeAll();
            rig.RunBlocks(1);  // drains randomize alone, so the reset block has randomizeRan false.
            rig.Application().RequestResetAll();
            rig.RunBlocks(1);
            break;
        case EnvelopeArm::RandomizeThenResetSameBlock:
            rig.Application().RequestRandomizeAll();
            rig.Application().RequestResetAll();
            rig.RunBlocks(1);  // both drain here, so ComputeAllParameters() runs after ResetAll.
            break;
    }

    rig.RunBlocks(kWarmUpBlocks);

    std::vector<EnvelopeWindow> windows;
    windows.reserve(kWindows);
    for (std::size_t window = 0; window < kWindows; ++window) {
        rig.ClearOutput();
        rig.RunBlocks(kBlocksPerWindow);
        const std::vector<float> samples = ExtractChannel(rig.Output(), 0);
        windows.push_back({BandAmplitude(samples, kAudibleFundamentalsHz, kSampleRateHz),
                           BandAmplitude(samples, kInaudibleFundamentalsHz, kSampleRateHz)});
    }
    return windows;
}

TEST_CASE(randomize_then_reset_hold_is_reported_per_band_against_a_pristine_decay) {
    const std::array<std::pair<EnvelopeArm, const char*>, 5> kArms{{
        {EnvelopeArm::Nothing, "A_nothing"},
        {EnvelopeArm::RandomizeOnly, "B_randomize_only"},
        {EnvelopeArm::ResetOnly, "C_reset_only"},
        {EnvelopeArm::RandomizeThenResetSplit, "D_split"},
        {EnvelopeArm::RandomizeThenResetSameBlock, "D_same_block"},
    }};

    for (const auto& [arm, name] : kArms) {
        const std::vector<EnvelopeWindow> windows = MeasureEnvelopeArm(arm, name);

        std::cout << "  [envelope arm " << name << "] audible(110/220/330) | subsonic(20/40/60)\n";
        for (std::size_t ix = 0; ix < windows.size(); ++ix) {
            std::cout << "    w" << ix << " audible=" << windows[ix].audible
                      << " subsonic=" << windows[ix].subsonic << "\n";
        }

        // Positive control: the instrument was live in this arm. Without it a
        // permanently silent arm reads as a clean decay.
        REQUIRE_TRUE(windows.front().audible > kBandSilenceFloorLinear);
    }
}


// -----------------------------------------------------------------------
// The single-draw result above (D_split holds, D_same_block decays) is one
// randomize draw per arm, and Randomize All draws from an RNG -- so a single
// pair cannot separate "the extra ComputeAllParameters() is what matters"
// from "those two draws happened to differ". This repeats each arm over many
// draws, fresh rig per draw so no draw inherits the last one's state, and
// reports how many held.
//
// The measurement window is the LAST 4-block window of the same geometry the
// per-band test above uses -- 24 warm-up blocks, then 11 windows discarded,
// then one measured -- which is the point where the pristine arm there read
// ~1e-11, ten orders under the floor. Window length is load-bearing: the ASR
// gate follows the transport's quarter-note pulse, so a window long enough to
// contain the next gate opening reads "still audible" for EVERY arm and the
// comparison collapses. A 48-block single window does exactly that.
//
// Hence the pristine control arm below. It is not decoration: it is the only
// thing that distinguishes "the split arm holds" from "this instrument cannot
// report silence at all".
// -----------------------------------------------------------------------
enum class ResetDrawArm { Pristine, Split, SameBlock };

TEST_CASE(pristine_and_reset_arms_compared_over_many_draws_with_a_silence_capable_instrument) {
    constexpr double kSampleRateHz = 48000.0;
    constexpr int kDraws = 12;
    constexpr std::size_t kWarmUpBlocks = 24;
    constexpr std::size_t kDiscardedWindows = 11;
    constexpr std::size_t kBlocksPerWindow = 4;

    const auto countHeldDraws = [&](ResetDrawArm arm, const char* scratchName) {
        int held = 0;
        for (int draw = 0; draw < kDraws; ++draw) {
            Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths(scratchName));
            rig.StartAt(0);
            rig.RunBlocks(8);

            if (arm != ResetDrawArm::Pristine) {
                rig.Application().RequestRandomizeAll();
                if (arm == ResetDrawArm::Split) {
                    // Drains randomize alone, so the reset block sees
                    // randomizeRan false and ComputeAllParameters() does not run.
                    rig.RunBlocks(1);
                }
                rig.Application().RequestResetAll();
                rig.RunBlocks(1);
            }

            rig.RunBlocks(kWarmUpBlocks + kDiscardedWindows * kBlocksPerWindow);
            rig.ClearOutput();
            rig.RunBlocks(kBlocksPerWindow);
            const double audible = BandAmplitude(ExtractChannel(rig.Output(), 0), kAudibleFundamentalsHz, kSampleRateHz);
            if (audible > kBandSilenceFloorLinear) {
                ++held;
            }
        }
        return held;
    };

    const int pristineHeld = countHeldDraws(ResetDrawArm::Pristine, "draws_pristine");
    const int splitHeld = countHeldDraws(ResetDrawArm::Split, "draws_split");
    const int sameHeld = countHeldDraws(ResetDrawArm::SameBlock, "draws_same_block");

    std::cout << "  [reset draws] of " << kDraws << " draws, still audible in the final window: "
              << "pristine=" << pristineHeld << "  split(no ComputeAllParameters after reset)=" << splitHeld
              << "  same-block(with it)=" << sameHeld << "\n";

    // The instrument can report silence. Without this, a nonzero count means
    // nothing -- it is what a dead measurement returns. This arm involves no
    // randomize and is bit-identical run to run, so it is safe to assert.
    REQUIRE_TRUE(pristineHeld == 0);
    // The randomize arms are NOT asserted, because their counts depend on the
    // envelope mapping rather than on the reset defect. With Grace's pre-
    // exponential linear map the split arm held 12 of 12; with the current
    // mapping it reads 0 of 12 whether or not the reset reseeds, because the
    // shorter holds decay inside the measurement window. Randomize itself is
    // deterministic -- an earlier reading of these counts as run-to-run noise
    // was withdrawn after ten controlled runs showed zero variance within a
    // fixed build. The counts are printed as configuration-dependent context;
    // the reset defect is asserted by the +0-block transient check instead,
    // which no envelope mapping can mask.
}


// -----------------------------------------------------------------------
// What "New" actually restores, and why it is not the default patch.
//
// New goes PatchManager::NewPatch() -> PatchMessageIn::RevertAllToDefault()
// -> ParameterManager::RevertAllToDefaults() (PatchPersistence.cpp:546) ->
// Parameter::RevertAllToDefault() per parameter, which sets each center to
// its REGISTERED config_.defaultValue and zeroes every modulation depth
// (ParameterModulation.cpp:1772 onward: currentDepths_/targetDepths_ filled
// with 0, activeRouteCount_ = 0, recursing into modulationDepths_).
//
// The centers survive that unchanged, because ApplyBankDefaultPatch writes
// the SAME registered layout.params[ix].defaultValue. What does not survive
// is the Audio bank's overlay: ApplyBankDefaultPatch additionally calls
// ApplyAudioBankOverlay, materializing the six cross-VCO pitch detents
// (detail::kAudioPitchDetents) as modulation DEPTHS -- exactly what a revert
// zeroes. No app-side hook re-applies the default patch afterwards, so the
// three VCOs land in unison and the instrument audibly changes with no saved
// patch anywhere on disk.
//
// Reset All does not have this problem: ResetBankToDefaultPatch clears the
// depths and then re-applies ApplyBankDefaultPatch on top, so the overlay is
// the last write (see that function's own comment). This test pins the
// asymmetry -- New wipes the detents, Reset All restores them -- so a future
// change to either path has to keep it or break a check.
// -----------------------------------------------------------------------
// A detent that is not materialized at all reads as nullopt, which is what
// New actually leaves behind -- the depth PARAMETER is reclaimed, not merely
// set back to neutral. Collapsing that into a float would hide the
// difference between "zeroed" and "gone", which is the whole finding.
std::array<std::optional<float>, 6> ReadAudioPitchDetents(synth_froggers::FroggersParameterModel& model) {
    std::array<std::optional<float>, 6> centers{};
    for (std::size_t ix = 0; ix < synth_froggers::detail::kAudioPitchDetents.size(); ++ix) {
        const auto& spec = synth_froggers::detail::kAudioPitchDetents[ix];
        const synth::Parameter& target =
            model.PageParameter(synth_froggers::FroggersBankId::Audio, spec.targetParamIx);
        const synth::Parameter* depth = target.ModulationDepthParameter(spec.modIx);
        centers[ix] = (depth == nullptr) ? std::nullopt : std::optional<float>(depth->SceneCenter(0));
    }
    return centers;
}

void PrintDetents(const char* label, const std::array<std::optional<float>, 6>& centers) {
    std::cout << "    " << label << ":";
    for (const std::optional<float>& center : centers) {
        if (center.has_value()) {
            std::cout << " " << *center;
        } else {
            std::cout << " (not materialized)";
        }
    }
    std::cout << "\n";
}

TEST_CASE(new_and_reset_all_both_restore_the_cross_vco_pitch_detents) {
    constexpr double kSampleRateHz = 48000.0;
    constexpr float kNeutralDepth = 0.5f;
    constexpr std::size_t kDrainBlocks = 16;
    constexpr std::size_t kWindowBlocks = 4;

    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("new_patch_detents"));
    rig.StartAt(0);
    rig.RunBlocks(8);

    const auto measureAudible = [&]() {
        rig.ClearOutput();
        rig.RunBlocks(kWindowBlocks);
        return BandAmplitude(ExtractChannel(rig.Output(), 0), kAudibleFundamentalsHz, kSampleRateHz);
    };

    auto& model = rig.Application().Parameters();

    const std::array<std::optional<float>, 6> fresh = ReadAudioPitchDetents(model);
    const double freshAudible = measureAudible();

    // The real production path -- the same PatchManager call the File page's
    // New button reaches (BrowserRuntimeMainServices.hpp's callbacks.newPatch).
    rig.Engine().Patches().NewPatch();
    rig.RunBlocks(kDrainBlocks);
    const std::array<std::optional<float>, 6> afterNew = ReadAudioPitchDetents(model);
    const double afterNewAudible = measureAudible();

    rig.Application().RequestResetAll();
    rig.RunBlocks(kDrainBlocks);
    const std::array<std::optional<float>, 6> afterReset = ReadAudioPitchDetents(model);
    const double afterResetAudible = measureAudible();

    std::cout << "  [cross-VCO pitch detents] neutral is " << kNeutralDepth << "\n";
    PrintDetents("fresh          ", fresh);
    PrintDetents("after New      ", afterNew);
    PrintDetents("after Reset All", afterReset);
    // Printed, deliberately NOT compared: each of these is a 4-block window
    // taken at whatever phase of the transport-driven ASR gate the preceding
    // block count happens to land on, so the three are not measurements of
    // the same thing. Comparing them would read a gate phase as a patch
    // difference. The detent rows above are the finding; these are context.
    std::cout << "    audible band at uncontrolled gate phase (not comparable): fresh=" << freshAudible
              << "  afterNew=" << afterNewAudible << "  afterReset=" << afterResetAudible << "\n";

    const auto materializedOffNeutral = [&](const std::array<std::optional<float>, 6>& centers) {
        return std::all_of(centers.begin(), centers.end(), [&](const std::optional<float>& center) {
            return center.has_value() && std::fabs(*center - kNeutralDepth) > 1.0e-6f;
        });
    };
    const auto noneMaterialized = [&](const std::array<std::optional<float>, 6>& centers) {
        return std::none_of(centers.begin(), centers.end(),
                            [](const std::optional<float>& center) { return center.has_value(); });
    };

    // Positive control: a fresh launch really does carry all six detents off
    // neutral, or "New restored them" could not be told from "they were never
    // disturbed".
    REQUIRE_TRUE(materializedOffNeutral(fresh));

    // The drift check the "single definition shared by launch, reset and New"
    // clause actually needs. Asserting only that each state is materialized and
    // off neutral would pass if New restored 0.52 where launch gives 0.51 --
    // three paths agreeing that a detent EXISTS is not the same as three paths
    // agreeing what it IS. Equality is what fails when any one of them drifts.
    const auto sameAsFresh = [&](const std::array<std::optional<float>, 6>& other) {
        for (std::size_t ix = 0; ix < fresh.size(); ++ix) {
            if (fresh[ix].has_value() != other[ix].has_value()) {
                return false;
            }
            if (fresh[ix].has_value() && std::fabs(*fresh[ix] - *other[ix]) > 1.0e-6f) {
                return false;
            }
        }
        return true;
    };
    REQUIRE_TRUE(sameAsFresh(afterNew));
    REQUIRE_TRUE(sameAsFresh(afterReset));
    // New reverts every parameter to its registered default, which cannot
    // express a depth -- so the app's own revert hook re-applies the default
    // patch and the detents come back. Before that hook existed, all six were
    // reclaimed outright and the three oscillators landed in unison.
    REQUIRE_TRUE(materializedOffNeutral(afterNew));
    // Reset All materializes and re-applies all six.
    REQUIRE_TRUE(materializedOffNeutral(afterReset));
}

// Sensitivity check on the comparison above: perturbs one of the six
// detents Reset All just restored, through the same SceneCenter write the
// grid and sweep tests below use to set parameter state, by far more than
// the comparison's own tolerance, then confirms the same element-wise
// comparison reports the perturbed state as different from a fresh launch.
TEST_CASE(perturbed_reset_detent_fails_the_launch_equality_check) {
    constexpr std::size_t kDrainBlocks = 16;
    constexpr float kPerturbationDelta = 0.05f;  // far above the 1.0e-6f tolerance below.

    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("perturbed_reset_detent"));
    rig.StartAt(0);
    rig.RunBlocks(8);

    auto& model = rig.Application().Parameters();
    const std::array<std::optional<float>, 6> fresh = ReadAudioPitchDetents(model);

    rig.Application().RequestResetAll();
    rig.RunBlocks(kDrainBlocks);

    const auto& perturbedSpec = synth_froggers::detail::kAudioPitchDetents[0];
    synth::Parameter& perturbedTarget =
        model.PageParameter(synth_froggers::FroggersBankId::Audio, perturbedSpec.targetParamIx);
    synth::Parameter* perturbedDepth = perturbedTarget.ModulationDepthParameter(perturbedSpec.modIx);
    REQUIRE_TRUE(perturbedDepth != nullptr);
    perturbedDepth->SceneCenter(0) += kPerturbationDelta;

    const std::array<std::optional<float>, 6> drifted = ReadAudioPitchDetents(model);

    // Same element-wise comparison used above, applied here to the perturbed
    // capture instead of the genuine one.
    const auto sameAsFresh = [&](const std::array<std::optional<float>, 6>& other) {
        for (std::size_t ix = 0; ix < fresh.size(); ++ix) {
            if (fresh[ix].has_value() != other[ix].has_value()) {
                return false;
            }
            if (fresh[ix].has_value() && std::fabs(*fresh[ix] - *other[ix]) > 1.0e-6f) {
                return false;
            }
        }
        return true;
    };

    REQUIRE_TRUE(!sameAsFresh(drifted));
}


// -----------------------------------------------------------------------
// Re-arming the reset reproduction against the landed Grace/Curve mapping.
//
// The reset defect is that a reset draining on a later block than the
// randomize never reaches the ComputeAllParameters() reseed, which
// ProcessFrame() runs only under `if (randomizeRan)` (FroggersAppCore.hpp).
// With the older linear Grace map and unbounded Curve ramps that showed up as
// a held level across the whole measurement window. The exponential Grace map
// and the duration-linear Curve warp shorten the holds enough that an ordinary
// randomize draw now decays inside that window, so the symptom stops
// reproducing at the knob values a draw happens to pick.
//
// The defect is untouched by that. The INSTRUMENT stopped reaching it. This
// sweeps Curve x Grace explicitly, set AFTER the operation, and reports which
// combinations still separate a reset arm from a pristine one.
//
// Deliberately does NOT call ComputeAllParameters() after writing the knobs:
// that is the very call whose absence is under test, and invoking it here
// would reseed the state the measurement is trying to observe.
//
// Both halves are required at a usable grid point. An arm that holds while
// pristine also holds proves nothing -- that is a knob setting that sustains
// the instrument, not one that exposes the reset gap.
// -----------------------------------------------------------------------
TEST_CASE(reset_reproduction_re_armed_across_the_curve_and_grace_grid) {
    constexpr double kSampleRateHz = 48000.0;
    constexpr std::size_t kEnvelopeCurveSlot = 12;  // FroggersParameters.hpp's Envelope row.
    constexpr std::size_t kEnvelopeGraceSlot = 13;
    constexpr std::size_t kWarmUpBlocks = 24;
    constexpr std::size_t kDiscardedWindows = 11;
    constexpr std::size_t kBlocksPerWindow = 4;
    const float kCurveGrid[] = {0.0f, 0.5f, 1.0f};
    const float kGraceGrid[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    const auto runOne = [&](bool doResetArm, float curve, float grace, const char* scratchName) {
        Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths(scratchName));
        rig.StartAt(0);
        rig.RunBlocks(8);

        if (doResetArm) {
            rig.Application().RequestRandomizeAll();
            rig.RunBlocks(1);  // randomize drains alone, so the reset block has randomizeRan false.
            rig.Application().RequestResetAll();
            rig.RunBlocks(1);
        }

        auto& model = rig.Application().Parameters();
        for (std::size_t sceneIx = 0; sceneIx < synth_froggers::FroggersParameterModel::kNumScenes; ++sceneIx) {
            model.PageParameter(synth_froggers::FroggersBankId::Envelope, kEnvelopeCurveSlot)
                .SceneCenter(sceneIx) = curve;
            model.PageParameter(synth_froggers::FroggersBankId::Envelope, kEnvelopeGraceSlot)
                .SceneCenter(sceneIx) = grace;
        }

        rig.RunBlocks(kWarmUpBlocks + kDiscardedWindows * kBlocksPerWindow);
        rig.ClearOutput();
        rig.RunBlocks(kBlocksPerWindow);
        return BandAmplitude(ExtractChannel(rig.Output(), 0), kAudibleFundamentalsHz, kSampleRateHz);
    };

    std::cout << "  [re-arm grid] curve/grace -> pristine | reset-split  (held = above "
              << kBandSilenceFloorLinear << ")\n";
    int usableGridPoints = 0;
    for (const float curve : kCurveGrid) {
        for (const float grace : kGraceGrid) {
            const double pristine = runOne(/*doResetArm=*/false, curve, grace, "rearm_pristine");
            const double resetArm = runOne(/*doResetArm=*/true, curve, grace, "rearm_reset");
            const bool pristineHeld = pristine > kBandSilenceFloorLinear;
            const bool resetHeld = resetArm > kBandSilenceFloorLinear;
            const bool usable = resetHeld && !pristineHeld;
            if (usable) {
                ++usableGridPoints;
            }
            std::cout << "    curve=" << curve << " grace=" << grace
                      << "  pristine=" << pristine << (pristineHeld ? " HELD" : " decayed")
                      << "  reset=" << resetArm << (resetHeld ? " HELD" : " decayed")
                      << (usable ? "   <-- USABLE" : "") << "\n";
        }
    }
    std::cout << "  [re-arm grid] usable grid points: " << usableGridPoints << "\n";

    // Reported, not asserted. Whether any grid point re-arms the reproduction is
    // the finding this test exists to produce; asserting a count here would turn
    // an open question into a requirement before it has been answered.
}


// -----------------------------------------------------------------------
// What differs between a reset that reseeds and one that does not.
//
// The two arms are identical except for whether ComputeAllParameters() runs
// after ResetAll: issuing both requests before one block drains them together
// so `randomizeRan` is still true and the reseed fires (FroggersAppCore.hpp),
// while a block in between leaves the reset block with randomizeRan false.
// Measured with the pre-mapping envelope, the first decays like a pristine
// instrument and the second holds indefinitely.
//
// Rather than guess which field carries that difference, this walks EVERY
// parameter in both arms -- all six banks' page slots, each bank's Crispy, the
// global Crunchy, and every modulation depth descendant recursively -- and
// reports the fields that disagree. An earlier version of this test walked
// only page slots against their own SceneCenter and found nothing, because it
// enumerated 62 of the depths this walk reaches and compared an arm against
// itself rather than against the other arm.
// -----------------------------------------------------------------------
// Every public observable on synth::Parameter, enumerated from its accessor
// list rather than chosen. An earlier version of this walk captured only
// SceneCenter/CurrentCenter/TargetCenter and reported "no difference" between
// two arms that audibly differ -- the modulation-APPLICATION state below is
// exactly what changes the DSP output without moving any center value.
struct ParamSnapshot {
    std::string path;
    float sceneCenter;
    float currentCenter;
    float targetCenter;
    std::size_t activeRoutes;
    float currentCenterScale;
    float targetCenterScale;
    float currentNormalizationOffset;
    float targetNormalizationOffset;
    std::array<float, synth_froggers::FroggersParameterModel::kNumModulators> currentDepths;
    std::array<float, synth_froggers::FroggersParameterModel::kNumModulators> targetDepths;
};

void SnapshotParameterTree(const synth::Parameter& param, const std::string& path,
                           std::vector<ParamSnapshot>& out) {
    ParamSnapshot snap{};
    snap.path = path;
    snap.sceneCenter = param.SceneCenter(0);
    snap.currentCenter = param.CurrentCenter();
    snap.targetCenter = param.TargetCenter();
    snap.activeRoutes = param.ActiveRouteCount();
    snap.currentCenterScale = param.CurrentCenterScale(0);   // monophonic: one voice.
    snap.targetCenterScale = param.TargetCenterScale(0);
    snap.currentNormalizationOffset = param.CurrentNormalizationOffset(0);
    snap.targetNormalizationOffset = param.TargetNormalizationOffset(0);
    for (std::size_t srcIx = 0; srcIx < synth_froggers::FroggersParameterModel::kNumModulators; ++srcIx) {
        snap.currentDepths[srcIx] = param.CurrentDepthForSource(0, srcIx);
        snap.targetDepths[srcIx] = param.TargetDepthForSource(0, srcIx);
    }
    out.push_back(snap);
    for (std::size_t modIx = 0; modIx < synth_froggers::FroggersParameterModel::kNumModulators; ++modIx) {
        const synth::Parameter* depth = param.ModulationDepthParameter(modIx);
        if (depth != nullptr) {
            SnapshotParameterTree(*depth, path + ".d" + std::to_string(modIx), out);
        }
    }
}

std::vector<ParamSnapshot> SnapshotWholeModel(synth_froggers::FroggersParameterModel& model) {
    std::vector<ParamSnapshot> out;
    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
        const auto bank = static_cast<synth_froggers::FroggersBankId>(bankIx);
        for (std::size_t slotIx = 0; slotIx < synth_froggers::kFroggersParamsPerBank; ++slotIx) {
            SnapshotParameterTree(model.PageParameter(bank, slotIx),
                                  "b" + std::to_string(bankIx) + ".s" + std::to_string(slotIx), out);
        }
        SnapshotParameterTree(model.Crispy(bank), "b" + std::to_string(bankIx) + ".crispy", out);
    }
    SnapshotParameterTree(model.Crunchy(), "crunchy", out);
    return out;
}

TEST_CASE(reseeded_and_unreseeded_reset_are_compared_field_by_field) {
    constexpr std::size_t kDrainBlocks = 32;
    constexpr float kTolerance = 1.0e-4f;

    const auto runArm = [&](bool sameBlock, const char* scratchName) {
        Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths(scratchName));
        rig.StartAt(0);
        rig.RunBlocks(8);
        rig.Application().RequestRandomizeAll();
        if (!sameBlock) {
            rig.RunBlocks(1);
        }
        rig.Application().RequestResetAll();
        rig.RunBlocks(1);
        rig.RunBlocks(kDrainBlocks);
        return SnapshotWholeModel(rig.Application().Parameters());
    };

    const std::vector<ParamSnapshot> reseeded = runArm(/*sameBlock=*/true, "arm_reseeded");
    const std::vector<ParamSnapshot> unreseeded = runArm(/*sameBlock=*/false, "arm_unreseeded");

    std::cout << "  [reset arms] parameters walked: reseeded=" << reseeded.size()
              << "  unreseeded=" << unreseeded.size() << "\n";

    std::size_t differing = 0;
    const std::size_t common = std::min(reseeded.size(), unreseeded.size());
    for (std::size_t ix = 0; ix < common; ++ix) {
        const ParamSnapshot& a = reseeded[ix];
        const ParamSnapshot& b = unreseeded[ix];
        if (a.path != b.path) {
            std::cout << "    tree shape diverges at index " << ix << ": " << a.path << " vs " << b.path << "\n";
            ++differing;
            break;
        }
        std::string why;
        const auto note = [&](const char* field, float lhs, float rhs) {
            if (std::fabs(lhs - rhs) > kTolerance) {
                why += std::string(" ") + field + "=" + std::to_string(lhs) + "/" + std::to_string(rhs);
            }
        };
        note("scene", a.sceneCenter, b.sceneCenter);
        note("current", a.currentCenter, b.currentCenter);
        note("target", a.targetCenter, b.targetCenter);
        note("ccScale", a.currentCenterScale, b.currentCenterScale);
        note("tcScale", a.targetCenterScale, b.targetCenterScale);
        note("cNorm", a.currentNormalizationOffset, b.currentNormalizationOffset);
        note("tNorm", a.targetNormalizationOffset, b.targetNormalizationOffset);
        if (a.activeRoutes != b.activeRoutes) {
            why += " routes=" + std::to_string(a.activeRoutes) + "/" + std::to_string(b.activeRoutes);
        }
        for (std::size_t srcIx = 0; srcIx < a.currentDepths.size(); ++srcIx) {
            note(("cDepth" + std::to_string(srcIx)).c_str(), a.currentDepths[srcIx], b.currentDepths[srcIx]);
            note(("tDepth" + std::to_string(srcIx)).c_str(), a.targetDepths[srcIx], b.targetDepths[srcIx]);
        }
        if (!why.empty()) {
            ++differing;
            if (differing <= 12) {
                std::cout << "    " << a.path << " " << why << "\n";
            }
        }
    }
    std::cout << "  [reset arms] fields differing (reseeded/unreseeded): " << differing
              << (differing > 12 ? "  (first 12 shown)" : "") << "\n";

    // Positive control: the walk actually reached parameters in both arms.
    REQUIRE_TRUE(reseeded.size() > 100 && unreseeded.size() > 100);
    // Reported, not asserted. Which fields differ is the finding; asserting a
    // count would fix an answer before the mechanism is named.
}


// -----------------------------------------------------------------------
// Does the un-reseeded reset differ TRANSIENTLY, before the smoothed path
// converges?
//
// The full-surface walk above finds the two arms identical 32 blocks after the
// reset -- every center, center scale, normalization offset, route count and
// per-source depth. Yet with the pre-mapping envelope one arm holds audibly and
// the other decays. If the end states match, the only remaining place for the
// difference to live is the blocks in between: ComputeAllParameters() snaps
// current to target immediately, while the per-sample path walks there over
// several blocks, so an un-reseeded reset spends that walk still driving the
// DSP with the values randomize drew.
//
// Samples the same observables immediately after the reset block and at +1,
// +2, +4, +8 blocks, and reports the block at which the arms converge. A
// nonzero count early that falls to zero later is the transient; zero
// throughout would rule the parameter model out entirely and put the
// difference in DSP unit state.
// -----------------------------------------------------------------------
TEST_CASE(the_two_reset_arms_are_compared_while_the_smoothed_path_is_still_walking) {
    constexpr float kTolerance = 1.0e-4f;
    const std::size_t kProbeBlocks[] = {0, 1, 2, 4, 8, 16};

    const auto armAt = [&](bool sameBlock, std::size_t extraBlocks, const char* scratchName) {
        Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths(scratchName));
        rig.StartAt(0);
        rig.RunBlocks(8);
        rig.Application().RequestRandomizeAll();
        if (!sameBlock) {
            rig.RunBlocks(1);
        }
        rig.Application().RequestResetAll();
        rig.RunBlocks(1);
        if (extraBlocks > 0) {
            rig.RunBlocks(extraBlocks);
        }
        return SnapshotWholeModel(rig.Application().Parameters());
    };

    std::size_t atResetBlock = 0;
    std::size_t walkedPerArm = 0;
    std::cout << "  [reset transient] blocks-after-reset -> fields differing between arms\n";
    for (const std::size_t probe : kProbeBlocks) {
        const std::vector<ParamSnapshot> a = armAt(true, probe, "transient_reseeded");
        const std::vector<ParamSnapshot> b = armAt(false, probe, "transient_unreseeded");
        std::size_t differing = 0;
        float worst = 0.0f;
        const std::size_t common = std::min(a.size(), b.size());
        for (std::size_t ix = 0; ix < common; ++ix) {
            float d = 0.0f;
            d = std::max(d, std::fabs(a[ix].currentCenter - b[ix].currentCenter));
            d = std::max(d, std::fabs(a[ix].currentCenterScale - b[ix].currentCenterScale));
            d = std::max(d, std::fabs(a[ix].currentNormalizationOffset - b[ix].currentNormalizationOffset));
            for (std::size_t srcIx = 0; srcIx < a[ix].currentDepths.size(); ++srcIx) {
                d = std::max(d, std::fabs(a[ix].currentDepths[srcIx] - b[ix].currentDepths[srcIx]));
            }
            if (d > kTolerance) {
                ++differing;
                worst = std::max(worst, d);
            }
        }
        std::cout << "    +" << probe << " blocks: differing=" << differing << "  worst=" << worst << "\n";
        if (probe == 0) {
            atResetBlock = differing;
            walkedPerArm = std::min(a.size(), b.size());
        }
    }

    // Positive control: the walk actually reached the model in both arms. Without
    // it, `atResetBlock == 0` is also what an empty walk returns, and the check
    // would pass by measuring nothing. Its sibling above carries the same guard.
    REQUIRE_TRUE(walkedPerArm > 100);
    // The reset block itself is the whole defect: a reset that has not reseeded
    // leaves the computed values walking toward what it commanded while the DSP
    // is still driven by the outgoing patch. Asserted at +0 rather than at a
    // settled block because the arms converge either way by +8 -- a check taken
    // after convergence passes whether or not the fix is present, which is how
    // every existing parameter-level test missed this.
    //
    // Independent of the Grace/Curve mapping, unlike an audio-level check: that
    // mapping governs how long any patch sustains and moves both arms together,
    // so no knob setting separates them (see the re-arm grid above).
    REQUIRE_TRUE(atResetBlock == 0);
}


// -----------------------------------------------------------------------
// Does a fast parameter sweep latch the instrument, with no Randomize and no
// Reset anywhere in it?
//
// Reseeding after Reset removes one TRIGGER: the window where the DSP is
// driven with values partway between an outgoing patch and an incoming one.
// It does not establish that such a window is the only way in. If a unit
// latches -- the delay's near-unity feedback, the reverb's Hold, the
// self-oscillating comb -- then every other fast sweep is an unprotected
// trigger: patch load, New, a scene blend, host automation. None of those
// pass through the reseed.
//
// This drives the sweep directly through SceneCenter writes, which no reseed
// covers, so the smoothed path walks exactly as it did before the fix. If the
// instrument decays afterwards, there is no latch and the transient
// explanation is complete. If it holds while a pristine arm decays, the
// reseed is a mitigation rather than a cure.
// -----------------------------------------------------------------------
TEST_CASE(a_fast_parameter_sweep_with_no_reset_does_not_latch_the_instrument) {
    constexpr double kSampleRateHz = 48000.0;
    constexpr std::size_t kSweepHeldBlocks = 8;   // the same order as the reset transient's walk.
    constexpr std::size_t kWarmUpBlocks = 24;
    constexpr std::size_t kDiscardedWindows = 11;
    constexpr std::size_t kBlocksPerWindow = 4;

    const auto run = [&](bool doSweep, const char* scratchName) {
        Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths(scratchName));
        rig.StartAt(0);
        rig.RunBlocks(8);

        if (doSweep) {
            auto& model = rig.Application().Parameters();
            // Every page parameter to its ceiling: a deliberately maximal
            // excursion, so a negative result is not "the sweep was too gentle".
            for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
                const auto bank = static_cast<synth_froggers::FroggersBankId>(bankIx);
                for (std::size_t slotIx = 0; slotIx < synth_froggers::kFroggersParamsPerBank; ++slotIx) {
                    for (std::size_t sceneIx = 0;
                         sceneIx < synth_froggers::FroggersParameterModel::kNumScenes; ++sceneIx) {
                        model.PageParameter(bank, slotIx).SceneCenter(sceneIx) = 1.0f;
                    }
                }
            }
            rig.RunBlocks(kSweepHeldBlocks);
            // Back to the launch patch, through the same single definition
            // launch itself uses. No Reset, so nothing reseeds.
            synth_froggers::ApplyFroggersDefaultPatch(model);
        }

        rig.RunBlocks(kWarmUpBlocks + kDiscardedWindows * kBlocksPerWindow);
        rig.ClearOutput();
        rig.RunBlocks(kBlocksPerWindow);
        return BandAmplitude(ExtractChannel(rig.Output(), 0), kAudibleFundamentalsHz, kSampleRateHz);
    };

    // The control that makes a quiet swept arm mean something: Freeze is a
    // deliberate, documented hold, driven through the real UI action path. If
    // this arm did NOT read as held, the rig or the measurement window could
    // not report a hold at all, and "the sweep did not latch" would be a
    // property of the instrument rather than of the instrument under test.
    const auto runLatchedControl = [&]() {
        Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("latch_control_frozen"));
        rig.StartAt(0);
        rig.RunBlocks(8);
        PressFreeze(rig);
        rig.RunBlocks(kWarmUpBlocks + kDiscardedWindows * kBlocksPerWindow);
        rig.ClearOutput();
        rig.RunBlocks(kBlocksPerWindow);
        return BandAmplitude(ExtractChannel(rig.Output(), 0), kAudibleFundamentalsHz, kSampleRateHz);
    };

    const double pristine = run(/*doSweep=*/false, "latch_pristine");
    const double swept = run(/*doSweep=*/true, "latch_swept");
    const double frozen = runLatchedControl();

    std::cout << "  [latch probe] pristine=" << pristine << "  after sweep+restore=" << swept
              << "  frozen(control)=" << frozen << "  (floor " << kBandSilenceFloorLinear << ")\n";

    // Can this measurement report silence?
    REQUIRE_TRUE(pristine < kBandSilenceFloorLinear);
    // Can it report a hold? Without this, the assertion below passes for a rig
    // that could never have shown one.
    REQUIRE_TRUE(frozen > kBandSilenceFloorLinear);
    // The question: does a sweep the reseed never covers leave the instrument
    // sounding after it has been returned to the launch patch?
    REQUIRE_TRUE(swept < kBandSilenceFloorLinear);
}


}  // namespace

int main() {
    int failed = 0;
    for (const auto& test : Registry()) {
        try {
            test.fn();
            std::cout << "[PASS] " << test.name << "\n";
        } catch (const std::exception& ex) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << ": " << ex.what() << "\n";
        }
    }
    return failed == 0 ? 0 : 1;
}
