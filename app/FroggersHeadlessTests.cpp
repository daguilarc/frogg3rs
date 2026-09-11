// FroggersHeadlessTests.cpp -- headless Init -> ProcessBlock
// test proving FroggersApp produces finite stereo output. Runs via
// synth_rig::SynthRig<FroggersApp> (External/Sheaf's
// tests/support/SynthRig.hpp), the same JUCE-free headless harness Sheaf's
// own miniapp/braid-4 system tests use (tests/miniapp_system_tests.cpp);
// wired into app/Makefile's `test` target (nice make -j2 test).
//
// This TU is part of the app core's build path (it only includes
// Froggers.hpp and Sheaf's JUCE-free rig/engine headers), so it doubles as a
// belt-and-suspenders JUCE guard alongside check_no_juce.cpp.

#include "Froggers.hpp"
#include "support/SynthRig.hpp"

#ifdef JUCE_MAJOR_VERSION
#error "Froggers headless tests must not see JUCE headers -- the app core must stay JUCE-free"
#endif

#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
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
// tests/miniapp_system_tests.cpp's UseScratchRuntimeDataPaths, so startup
// patch loading never observes a shared production location or another
// test's data.
synth::RuntimeDataPaths UseScratchRuntimeDataPaths(const char* testName) {
    const std::filesystem::path dataRoot =
        std::filesystem::temp_directory_path() / "froggers-headless-tests" / testName;
    std::filesystem::remove_all(dataRoot);
    synth::RuntimeDataPaths paths = synth::RuntimeDataPaths::FromDataRoot(dataRoot);
    std::filesystem::create_directories(paths.patchesRoot);
    return paths;
}

TEST_CASE(froggers_init_process_block_produces_finite_stereo_output) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64,
        UseScratchRuntimeDataPaths("init_process_block_finite"));

    // SynthRig's constructor already runs App::Config()/Init(&context_) via
    // Engine::Initialize()+Prepare(); pump several blocks of ProcessBlock and
    // inspect every captured sample. Config() requests one input channel, so
    // SynthRig allocates a real input buffer for it and passes a real
    // (silent, since nothing feeds it) `block.inputs` into every ProcessBlock
    // call below (tests/support/SynthRig.hpp: `numInputChannels_` is
    // `config.numAudioInputs`). SynthRig wires no routed-input signal into
    // its AppContext, so the external-audio modulation sources stay
    // disconnected throughout regardless (see the sibling test below) --
    // what this test proves is narrower: ProcessBlock produces finite stereo
    // output end-to-end through Init()/PrepareToPlay(), with a real input
    // channel present.
    rig.RunBlocks(4);

    REQUIRE_TRUE(!rig.SawNaN());

    const auto& output = rig.Output();
    REQUIRE_TRUE(!output.empty());
    for (const auto& frame : output) {
        REQUIRE_TRUE(frame.channels.size() == 2);  // Config() requests stereo out.
        for (const float sample : frame.channels) {
            REQUIRE_TRUE(std::isfinite(sample));
        }
    }
}

TEST_CASE(froggers_config_requests_one_audio_input_channel) {
    const synth::RuntimeConfig config = synth_froggers::FroggersApp::Config();
    REQUIRE_TRUE(config.numAudioInputs == 1);
}

// Proves the external-audio modulation sources' connected state does not
// follow channel presence, through the REAL Config()->Init()->ProcessBlock
// path (SynthRig), which FroggersModulationTests.cpp's own external-audio
// tests (e.g. external_audio_cells_present_and_inert_with_no_input) do not
// reach: that file drives FroggersModulationSlate::Step() directly against a
// bare Fixture, and never touches FroggersApp::Config() or ProcessBlock at
// all. This test checks three distinct things a single "not connected"
// assertion would blur together: (1) the app requests a real audio input
// channel -- so this is not vacuously true for lack of anything to be
// mistaken for routing; (2) both external-audio sources are still
// REGISTERED and PRESENT in the slate -- proven by their registered `name`,
// not merely a `false` `connected` bit, because a slot silently dropped from
// RegisterSources() instead of disconnected would ALSO read a
// default-constructed `connected == false` (synth::ModulatorMetadata's own
// default, ParameterModulation.hpp) and pass a connected-only check while
// actually being absent; and (3) both report connected == false, because
// SynthRig wires no routed-input signal into its AppContext at all --
// AppContext::InputRouted() reads false unconditionally under this rig, so
// this is the privacy property's degenerate case: a channel exists (this
// test's own (1) above) but nothing the app can read ever calls that
// routing, and the pair stays inert.
//
// Positive control (mandatory before trusting any "not connected"
// result): a rig that populates nothing would ALSO read every source as
// disconnected, so "false" alone proves nothing. Prints the total
// registered source count and reads a DIFFERENT, unconditionally-connected
// source (Random S&H 1, slot 0 -- registered `connected = true` in
// RegisterSources(), FroggersModulation.hpp) as `connected == true` in this
// SAME rig, SAME run, before the disconnected checks below -- demonstrating
// the metadata-reading mechanism itself is live, not uniformly false.
TEST_CASE(external_audio_sources_stay_registered_and_disconnected_with_a_channel_present_but_not_routed) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64,
        UseScratchRuntimeDataPaths("external_audio_registered_disconnected"));

    // (1) A real channel is requested, re-asserted here alongside its
    // slate-level consequence rather than trusted from the sibling test
    // above.
    const synth::RuntimeConfig config = synth_froggers::FroggersApp::Config();
    REQUIRE_TRUE(config.numAudioInputs == 1);

    // Transport RUNNING, real blocks pumped: this exercises the REAL
    // FroggersAppCore::PrepareToPlay()/ProcessBlock() wiring end-to-end, not
    // just RegisterSources()'s one-time Init() default in isolation. It also
    // proves externalAudioSource_/externalAudioEfSource_ (frozen at their
    // NSDMI defaults, FroggersModulation.hpp) stay defined and finite over
    // actual per-sample execution (SawNaN() below), which is exactly what
    // Modulators::UpdateModValues() needs every sample regardless of
    // connectedness.
    rig.StartAt(0);
    rig.RunBlocks(8);
    REQUIRE_TRUE(!rig.SawNaN());

    synth_froggers::FroggersModulationSlate& modulation = rig.Application().Modulation();

    // Positive control FIRST.
    std::cout << "external_audio_sources_stay_registered_and_disconnected_with_a_channel_present_but_not_routed: "
                 "total registered sources = "
              << synth_froggers::FroggersParameterModel::kNumModulators << "\n";
    REQUIRE_TRUE(synth_froggers::FroggersParameterModel::kNumModulators == 15);
    const synth::ModulatorMetadata& controlSource =
        modulation.Metadata(synth_froggers::kModSlotRandomSh1);
    std::cout << "  positive control -- slot " << synth_froggers::kModSlotRandomSh1 << " (\""
              << controlSource.name << "\") connected = " << std::boolalpha
              << controlSource.connected << "\n";
    REQUIRE_TRUE(controlSource.connected);  // proves the rig/metadata path can read true.

    // (2) Present: both external-audio slots were actually registered by
    // RegisterSources() -- their `name` is the exact string only that call
    // sets -- not silently dropped from the slate.
    const synth::ModulatorMetadata& externalAudio =
        modulation.Metadata(synth_froggers::kModSlotExternalAudio);
    const synth::ModulatorMetadata& externalAudioEf =
        modulation.Metadata(synth_froggers::kModSlotExternalAudioEf);
    REQUIRE_TRUE(externalAudio.name == "External Audio");
    REQUIRE_TRUE(externalAudioEf.name == "External Audio EF");

    // (3) Inert: both report connected == false -- a state the presence
    // checks above already established is not the same thing as absence.
    std::cout << "  external audio (slot " << synth_froggers::kModSlotExternalAudio
              << ") connected = " << std::boolalpha << externalAudio.connected << "\n";
    std::cout << "  external audio EF (slot " << synth_froggers::kModSlotExternalAudioEf
              << ") connected = " << std::boolalpha << externalAudioEf.connected << "\n";
    REQUIRE_TRUE(!externalAudio.connected);
    REQUIRE_TRUE(!externalAudioEf.connected);
}

// ============================================================================
// External-audio connected state follows the host's routed-input signal,
// never channel presence alone
// ============================================================================
// synth_rig::SynthRig does not wire a synth::InputRoutingSignal into the
// AppContext it builds (tests/support/SynthRig.hpp assigns nothing to
// `inputRoutingSignal`), so AppContext::InputRouted() reads false
// unconditionally under every SynthRig-based test above -- adequate for
// proving a channel's mere presence never flips connected (the sibling test
// above), but not for exercising a real routed transition. The two tests
// below construct FroggersAppCore directly against a hand-built AppContext
// that DOES wire a real, controllable synth::InputRoutingSignal -- the same
// class the JUCE and browser hosts publish through (AppContext.hpp) --
// bypassing SynthRig/Engine entirely: FroggersAppCore::Init() only
// dereferences `context->parameterManager`, and ProcessFrame() only touches
// `context->masterClock`/`uiBus` behind guards neither test below trips
// (both stay null here).

// Positive control across four states in one run: connected at
// Init() (already-routed-before-startup), still connected immediately after
// a transition is queued but before it is applied, disconnected once applied,
// and connected again after a second transition -- the same two assertions
// (immediately-after-Publish vs. after-ProcessFrame) flip each time.
TEST_CASE(external_audio_connected_follows_the_routed_signal_read_at_startup_and_on_transition) {
    synth::ParameterManager manager;
    synth::InputRoutingSignal signal;
    signal.Publish(true);  // already routed before Init() runs -- proves the startup read.

    synth::AppContext context;
    context.parameterManager = &manager;
    context.inputRoutingSignal = &signal;

    synth_froggers::FroggersAppCore app;
    app.Init(&context);

    REQUIRE_TRUE(app.Modulation().Metadata(synth_froggers::kModSlotExternalAudio).connected);
    REQUIRE_TRUE(app.Modulation().Metadata(synth_froggers::kModSlotExternalAudioEf).connected);

    // A later transition is queued on Publish() (the message thread in
    // production) but must NOT reach `connected` until ProcessFrame() (the
    // audio thread) drains it -- Modulators::UpdateModValues() reads
    // `connected` every sample, so an immediate write from the callback
    // would race it (see FroggersAppCore.hpp's Init()/ProcessFrame()
    // comments for the full trace).
    signal.Publish(false);
    REQUIRE_TRUE(app.Modulation().Metadata(synth_froggers::kModSlotExternalAudio).connected);  // not yet applied
    app.ProcessFrame();
    REQUIRE_TRUE(!app.Modulation().Metadata(synth_froggers::kModSlotExternalAudio).connected);
    REQUIRE_TRUE(!app.Modulation().Metadata(synth_froggers::kModSlotExternalAudioEf).connected);

    // And back -- the same assertion goes the other way a second time.
    signal.Publish(true);
    REQUIRE_TRUE(!app.Modulation().Metadata(synth_froggers::kModSlotExternalAudio).connected);  // still not yet applied
    app.ProcessFrame();
    REQUIRE_TRUE(app.Modulation().Metadata(synth_froggers::kModSlotExternalAudio).connected);
    REQUIRE_TRUE(app.Modulation().Metadata(synth_froggers::kModSlotExternalAudioEf).connected);
}

// The privacy property itself: a signal that has never been told "routed"
// -- exactly what a platform-default-opened device with an empty persisted
// selection derives (Runtime.hpp's RefreshInputRoutedState, JUCE-side and
// out of reach of this JUCE-free test, but its boolean contract is this
// synth::InputRoutingSignal) -- leaves the pair disconnected even though a
// real AppContext::inputRoutingSignal is wired (unlike the SynthRig-based
// test above, where no signal object exists at all).
TEST_CASE(external_audio_stays_disconnected_when_the_routed_signal_is_never_published_true) {
    synth::ParameterManager manager;
    synth::InputRoutingSignal signal;  // default-constructed: never told "routed".

    synth::AppContext context;
    context.parameterManager = &manager;
    context.inputRoutingSignal = &signal;

    synth_froggers::FroggersAppCore app;
    app.Init(&context);
    app.ProcessFrame();

    REQUIRE_TRUE(!app.Modulation().Metadata(synth_froggers::kModSlotExternalAudio).connected);
    REQUIRE_TRUE(!app.Modulation().Metadata(synth_froggers::kModSlotExternalAudioEf).connected);
}

// End-to-end proof that the
// ComputeAllParameters() reseed inside FroggersAppCore::ProcessFrame()
// (FroggersAppCore.hpp) actually reaches the display through the REAL
// production path: RequestRandomizeAll() (UI/message thread) ->
// ProcessFrame() (audio thread, drained by synth::Engine once per block,
// before ProcessBlock() -- this class's own header comment) -> the reseed.
// FroggersModulationTests.cpp's own randomize tests call
// FroggersModulation.hpp's RandomizeAll()/RandomizePage() directly against a
// bare ParameterManager, which never exercises FroggersAppCore::
// ProcessFrame() at all -- this is the one place in the suite that proves
// the fix is wired into the real request-bridge path, not just correct in
// isolation.
TEST_CASE(randomize_all_request_through_process_frame_updates_the_display) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64,
        UseScratchRuntimeDataPaths("randomize_all_updates_display"));
    rig.RunBlocks(2);  // let Init()'s default patch / first ProcessFrame settle.

    rig.Application().RequestRandomizeAll();
    rig.RunBlocks(1);  // ProcessFrame() drains the request and reseeds, same block.

    // Randomize All (drill-in level 0) touches every top-level parameter
    // across all six banks -- find any depth it materialized
    // and confirm the DISPLAY (not just the commanded value) moved.
    constexpr float kNeutral = 0.5f;
    constexpr float kTolerance = 1e-4f;
    bool foundMovedDisplay = false;
    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount && !foundMovedDisplay; ++bankIx) {
        const auto bankId = static_cast<synth_froggers::FroggersBankId>(bankIx);
        for (std::size_t paramIx = 0; paramIx < synth_froggers::kFroggersParamsPerBank; ++paramIx) {
            synth::Parameter& parameter = rig.Application().Parameters().PageParameter(bankId, paramIx);
            for (std::size_t modIx = 0; modIx < synth_froggers::FroggersParameterModel::kNumModulators; ++modIx) {
                synth::Parameter* depth = parameter.ModulationDepthParameter(modIx);
                if (depth != nullptr && std::fabs(depth->UIDisplayCenter(0) - kNeutral) > kTolerance) {
                    foundMovedDisplay = true;
                    break;
                }
            }
            if (foundMovedDisplay) {
                break;
            }
        }
    }
    REQUIRE_TRUE(foundMovedDisplay);
}

// LastRandomizePartial() defaults false and stays false across an
// ordinary Randomize All request with ample storage headroom (the default
// FroggersModulationSlate::kDepthParameterStorageCapacity, 1200, is well
// above the 793-915 ceiling a full randomize can reach), proving the accessor is
// wired and does not false-positive on a healthy randomize.
TEST_CASE(randomize_all_with_ample_capacity_reports_not_partial) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64,
        UseScratchRuntimeDataPaths("randomize_all_not_partial"));
    rig.RunBlocks(2);

    REQUIRE_TRUE(!rig.Application().LastRandomizePartial());
    rig.Application().RequestRandomizeAll();
    rig.RunBlocks(1);
    REQUIRE_TRUE(!rig.Application().LastRandomizePartial());
}

// A minimal repro of the loud-stuck regime, kept as a suite test so
// the regression it guards against cannot silently come back. Curve
// (Envelope slot 12) at exactly 1.0, Grace (slot 13) active, VCO1's own
// Decay/Sustain set so the ease-in Decay's slow start lingers near peak
// (the loud-stuck regime) -- everything else default. Without a progress
// floor in ComputeRampStep, this configuration
// held output flat at 0.939 at t+10s post-Stop,
// AllIdle()==0 at every checkpoint through t+5s -- the voice never reached
// Idle, so FroggersAppCore's delay/reverb clear (gated on AllIdle()) never
// fired, and the instrument never actually stopped after Stop. This test
// pins the fix: AllIdle() within 2.0s of Stop, and output peak below 1e-4
// within 2.5s of Stop.
TEST_CASE(stop_silences_curve_one_grace_active_voice_within_bound) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64,
        UseScratchRuntimeDataPaths("stop_silences_curve_one_grace_active"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    using synth_froggers::FroggersBankId;

    model.PageParameter(FroggersBankId::Audio, 0).SceneCenter(0) = 0.5f;   // VCO1 pitch.
    model.PageParameter(FroggersBankId::Drive, 1).SceneCenter(0) = 0.8f;   // Gain.
    // Same recipe as the loud-stuck regime above:
    // mid Decay + audible Sustain on VCO1 (ease-in Decay's slow start keeps
    // level lingering near peak -- default fast Decay or silent Sustain
    // would fail to reproduce the bug for the WRONG reason), Curve at
    // exactly 1.0 (the value that would leave ComputeRampStep's per-sample
    // progress unbounded without its progress floor), Grace active (a pending release defers
    // through Attack/Decay instead of forcing Release immediately -- the
    // condition needed to expose the stuck-mid-Decay regime).
    // Attack/Release/VCO2/VCO3 stay at their registered defaults, exactly
    // as the repro leaves them.
    model.PageParameter(FroggersBankId::Envelope, 1).SceneCenter(0) = 0.5f;   // Decay VCO1: mid.
    model.PageParameter(FroggersBankId::Envelope, 2).SceneCenter(0) = 0.7f;   // Sustain VCO1: audible.
    model.PageParameter(FroggersBankId::Envelope, 12).SceneCenter(0) = 1.0f;  // Curve: exactly 1.0.
    model.PageParameter(FroggersBankId::Envelope, 13).SceneCenter(0) = 0.5f;  // Grace: active.
    rig.Application().TestParameterManager().ComputeAllParameters();

    rig.StartAt(0);
    std::uint64_t timestamp = 0;
    const std::size_t blockSize = 256;
    constexpr double kSampleRateHz = 48000.0;
    const auto secondsToBlocks = [&](double seconds) -> std::size_t {
        return static_cast<std::size_t>(std::ceil((seconds * kSampleRateHz) / static_cast<double>(blockSize)));
    };

    rig.RunBlocks(secondsToBlocks(2.0));
    timestamp += secondsToBlocks(2.0);

    rig.ClearOutput();
    rig.RunBlocks(secondsToBlocks(0.1));
    timestamp += secondsToBlocks(0.1);
    float prestopPeak = 0.0f;
    for (const auto& frame : rig.Output()) {
        for (float sample : frame.channels) prestopPeak = std::max(prestopPeak, std::fabs(sample));
    }
    // Liveness gate (a positive control): the arm must actually
    // be audible before Stop, or a silent-by-accident run would make the
    // post-Stop assertions below vacuous.
    REQUIRE_TRUE(prestopPeak > 0.05f);

    rig.StopAt(timestamp);

    // AllIdle() within 2.0s of Stop -- the ADSR side of the fix: every
    // voice actually reaches Stage::Idle (was: stuck mid-Decay forever,
    // AllIdle()==0 through t+5s pre-fix).
    rig.RunBlocks(secondsToBlocks(2.0));
    REQUIRE_TRUE(rig.Application().TestAudioAdsr().AllIdle());

    // Silence within 2.5s of Stop -- the audible consequence: once AllIdle()
    // fires, FroggersAppCore's delay/reverb clear can run and the wet tail
    // finishes decaying (was: output held flat at 0.939 at t+10s pre-fix).
    rig.ClearOutput();
    rig.RunBlocks(secondsToBlocks(0.5));
    float postStopPeak = 0.0f;
    for (const auto& frame : rig.Output()) {
        for (float sample : frame.channels) postStopPeak = std::max(postStopPeak, std::fabs(sample));
    }
    REQUIRE_TRUE(!rig.SawNaN());
    REQUIRE_TRUE(postStopPeak < 1.0e-4f);
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
