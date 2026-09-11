// FroggersVstHostTests.cpp -- tests DAW-external transport, host tempo, and
// the plugin-mode editor row, extended with a bank-agreement test and an
// end-to-end plugin-mode-surface test through the real processor. CTest-
// wired via app/vst/CMakeLists.txt's add_test(), same "drive the REAL
// compiled FroggersPluginProcessor, not a second copy" shape as
// FroggersVstSmokeTest.cpp (links the same FroggersVst shared-code target).
// This binary does NOT test the real FroggersPluginEditor's own
// construction/destruction -- that needs a juce::Component, not just a
// juce::AudioProcessor, and is isolated in its own CTest binary instead
// (FroggersVstEditorTest.cpp's own header comment explains why).
//
// Six things this file needs proven, each its own section below:
//   1. Transport edges: a fake juce::AudioPlayHead drives
//      processBlock()'s real edge-detector; PumpMessageThreadForTest()
//      (timerCallback() itself, since a headless CTest binary runs no
//      message loop to fire a real juce::Timer -- see
//      FroggersPluginProcessor.hpp's own comment on that seam) drives the
//      real producer. Counted AT THE BUS (UiBusPendingCountForTest(),
//      engine_.UiBus().Size()) rather than inferred from eventual state, so
//      "exactly one message per transition, none while holding" is an
//      actual count, not a guess -- true for transitions the test spaces
//      out across pumps, as every case below does; pendingTransportEdge_ is
//      a single-slot coalescing atomic (FroggersPluginProcessor.hpp's own
//      comment on it), so transitions arriving faster than this class's
//      ~33ms (30Hz) pump rate collapse to the latest one, by design, not
//      tested here.
//   2. Freeze semantics: engaged via the real production seam
//      (PortableSurface().DispatchAction(kFreeze), the same call
//      FroggersUiSurface's Freeze button itself makes), mirroring the
//      DIRECTION of app/FroggersAudioRoutingTests.cpp's
//      freeze_alone_holds_the_ring_above_an_audible_floor_and_stops_the_
//      transport test -- transport reads stopped, FreezeLatched() true,
//      audio stays audible past stopping_transport_silences_...'s own
//      settle window -- on the DEFAULT patch rather than that test's
//      specially tuned self-sustaining delay/reverb ring: FroggersAppCore
//      forces `setGate(gateOpen || FreezeLatched())` while latched (that
//      file's own comment on why a single Freeze press both engages the
//      latch and stops the transport), so the envelope holds open
//      regardless of patch tuning -- the ring recipe exists to prove
//      something ELSE (recirculating energy persists), not to make the gate
//      stay open, so this simpler setup still exercises the actual property
//      needed here.
//   3. Tempo-follow: host bpm -> DisplayTempoBpm()/
//      TempoExternallyClocked(), RequestTempoBpm() rejected while slaved,
//      both directions (works before engaging, rejected while slaved, works
//      again after disengaging).
//   4. Surface row: app/FroggersUiSurface.hpp's own
//      FroggersUiSurface, attached context-free (mirrors
//      FroggersSurfaceTests.cpp's BuildFroggersTreeAt/FindNodeById --
//      app/vst/ cannot include that .cpp, a standalone binary with its own
//      main(), so the same technique is reproduced here rather than
//      imported) -- default mode byte-identical children, plugin mode
//      thinned to Freeze + label. The REAL default-mode identity proof is
//      running app/FroggersSurfaceTests.cpp and the rest of the app suite
//      UNCHANGED; the check here is a second, explicit, redundant
//      confirmation of the same property from this binary.
//   5. Host automation addressing: PumpHostParameterBridge()
//      (FroggersPluginProcessor.cpp) resolves every host write directly
//      against its own entry's bank (MessageIn::ParamSetAbsoluteOnBank),
//      never through the shared BankSlot's current selection -- five
//      TEST_CASEs below cover a non-visible-bank write landing there while
//      the operator's own page/tab/page-scoped-action stays put, two banks
//      written in one pump not oscillating the visible page, an open
//      modulation drilldown surviving a cross-bank write, a same-bank write
//      during a drilldown landing on the top-level parameter rather than
//      whatever depth cell the drilldown maps that same encoder to, and a
//      positive control proving the operator's OWN bank-select action does
//      move the page.
//   6. Plugin-mode surface tree end-to-end: the REAL processor's
//      surface (not a bare hand-built one) renders Play/Stop/Record absent,
//      Freeze + its "FREEZE" label present, and BPM as a display-only
//      StatusText while the host clock is engaged -- proving production
//      code actually calls SetPluginHostMode(true) (section 4's own test
//      never constructs a real processor, so it cannot see that call at
//      all), combined with the Tempo-follow section's own host-tempo-slaving.

#include "FroggersPluginProcessor.hpp"

#include "FroggersUiSurface.hpp"
#include "synth/PatchPersistence.hpp"
#include "synth/PortableUI.hpp"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

// Same self-contained TEST_CASE/REQUIRE_TRUE idiom every app/*Tests.cpp file
// already defines locally (e.g. app/FroggersSurfaceTests.cpp's `TestCase`) --
// mirrored here rather than shared via a new header, matching that
// per-file-self-contained convention.
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

#define TEST_CASE(name)                \
    void name();                       \
    Register reg_##name(#name, &name); \
    void name()

#define REQUIRE_TRUE(expr)                                                        \
    do {                                                                          \
        if (!(expr)) {                                                            \
            std::ostringstream oss;                                               \
            oss << __FILE__ << ":" << __LINE__ << " requirement failed: " #expr;  \
            throw std::runtime_error(oss.str());                                  \
        }                                                                         \
    } while (false)

// Same shape as FroggersVstSmokeTest.cpp's ScratchDataPaths(), parameterized
// by test name so each TEST_CASE's processor gets its own scratch root
// (several processors are constructed in this one binary; a shared root
// would let one test's patch state leak into another's).
synth::RuntimeDataPaths ScratchDataPaths(const char* testName) {
    const std::filesystem::path dataRoot =
        std::filesystem::temp_directory_path() / "froggers-vst-host-tests" / testName;
    std::filesystem::remove_all(dataRoot);
    synth::RuntimeDataPaths paths = synth::RuntimeDataPaths::FromDataRoot(dataRoot);
    std::filesystem::create_directories(paths.patchesRoot);
    return paths;
}

// A minimal juce::AudioPlayHead test double: exactly the two PositionInfo
// fields the transport/tempo bridge reads (isPlaying, bpm). getPosition()'s own JUCE doc
// comment restricts real callers to processBlock() (see
// FroggersPluginProcessor.cpp's own citation of that comment) -- this class
// makes no such promise itself, it is only ever driven from this test's own
// single thread, calling processBlock() directly, so that restriction is
// respected by construction.
class FakePlayHead final : public juce::AudioPlayHead {
public:
    void SetPlaying(bool playing) { info_.setIsPlaying(playing); }
    void SetBpm(double bpm) { info_.setBpm(juce::Optional<double>(bpm)); }
    void ClearBpm() { info_.setBpm(juce::Optional<double>()); }

    juce::Optional<PositionInfo> getPosition() const override { return info_; }

private:
    PositionInfo info_;
};

// -- 1. Transport edges -------------------------------------------------------
// run -> stop -> run: exactly one message per transition, none while
// holding, counted at the bus (UiBusPendingCountForTest()), not inferred.
TEST_CASE(transport_edges_run_stop_run_produce_exactly_one_message_per_transition) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("transport_edges"));
    FakePlayHead playHead;
    processor.setPlayHead(&playHead);
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    auto runBlock = [&] {
        buffer.clear();
        processor.processBlock(buffer, midi);
    };

    // First observation establishes the baseline (only on host play-state
    // TRANSITIONS) -- must NOT itself produce a message.
    playHead.SetPlaying(false);
    runBlock();
    processor.PumpMessageThreadForTest();
    REQUIRE_TRUE(processor.UiBusPendingCountForTest() == 0);

    // Holding "not playing" across several blocks: still nothing pending.
    runBlock();
    runBlock();
    processor.PumpMessageThreadForTest();
    REQUIRE_TRUE(processor.UiBusPendingCountForTest() == 0);

    // -- RUN (Stopped -> Playing transition #1) ------------------------------
    playHead.SetPlaying(true);
    runBlock();  // detects the edge, buffers it (does not push).
    processor.PumpMessageThreadForTest();  // pushes exactly the one Start.
    REQUIRE_TRUE(processor.UiBusPendingCountForTest() == 1);
    runBlock();  // drains+applies it.
    REQUIRE_TRUE(processor.UiBusPendingCountForTest() == 0);
    REQUIRE_TRUE(processor.ApplicationForTest().TransportRunning());

    // Holding "playing" across several more blocks + pumps: no new message.
    runBlock();
    processor.PumpMessageThreadForTest();
    REQUIRE_TRUE(processor.UiBusPendingCountForTest() == 0);
    runBlock();
    processor.PumpMessageThreadForTest();
    REQUIRE_TRUE(processor.UiBusPendingCountForTest() == 0);
    REQUIRE_TRUE(processor.ApplicationForTest().TransportRunning());

    // -- STOP (Playing -> Stopped transition) --------------------------------
    playHead.SetPlaying(false);
    runBlock();
    processor.PumpMessageThreadForTest();
    REQUIRE_TRUE(processor.UiBusPendingCountForTest() == 1);
    runBlock();
    REQUIRE_TRUE(processor.UiBusPendingCountForTest() == 0);
    REQUIRE_TRUE(!processor.ApplicationForTest().TransportRunning());

    // -- RUN again (Stopped -> Playing transition #2) ------------------------
    playHead.SetPlaying(true);
    runBlock();
    processor.PumpMessageThreadForTest();
    REQUIRE_TRUE(processor.UiBusPendingCountForTest() == 1);  // exactly one, not accumulated from before.
    runBlock();
    REQUIRE_TRUE(processor.UiBusPendingCountForTest() == 0);
    REQUIRE_TRUE(processor.ApplicationForTest().TransportRunning());

    processor.releaseResources();
    processor.setPlayHead(nullptr);

    std::cout << "  [6.1] run->stop->run: exactly one bus message per transition, none while holding.\n";
}

// Absent playhead (standalone hosting) must never synthesize a transport
// message: handled gracefully -- no messages.
TEST_CASE(transport_absent_playhead_produces_no_messages) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("transport_absent_playhead"));
    // No setPlayHead() call at all -- getPlayHead() returns nullptr, exactly
    // the standalone-hosting case.
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    for (int i = 0; i < 8; ++i) {
        buffer.clear();
        processor.processBlock(buffer, midi);
    }
    processor.PumpMessageThreadForTest();
    REQUIRE_TRUE(processor.UiBusPendingCountForTest() == 0);
    REQUIRE_TRUE(!processor.ApplicationForTest().TransportRunning());

    processor.releaseResources();
    std::cout << "  [6.1] absent playhead: no messages across 8 blocks.\n";
}

// -- 2. Freeze semantics -----------------------------------------------------
// Mirrors the DIRECTION of app/FroggersAudioRoutingTests.cpp's
// freeze_alone_holds_the_ring_above_an_audible_floor_and_stops_the_transport
// test -- see this file's own header comment for why the default patch
// already exercises the property under test (setGate(gateOpen ||
// FreezeLatched())).
TEST_CASE(freeze_via_production_seam_holds_audio_and_reads_stopped_like_t7_3a) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("freeze_semantics"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    auto runBlocksMeasuringPeak = [&](int blocks) {
        float peak = 0.0f;
        for (int i = 0; i < blocks; ++i) {
            buffer.clear();
            processor.processBlock(buffer, midi);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                const float* samples = buffer.getReadPointer(ch);
                for (int s = 0; s < buffer.getNumSamples(); ++s) {
                    peak = std::max(peak, std::fabs(samples[s]));
                }
            }
        }
        return peak;
    };

    // Real production Play seam (same one TestStartTransport() now routes
    // through) -- start the transport and let the default patch settle into
    // audible steady state, same ~1s budget FroggersVstSmokeTest.cpp uses.
    processor.TestStartTransport();
    const float settledPeak = runBlocksMeasuringPeak(188);
    // Same -60dBFS silence floor FroggersVstSmokeTest.cpp establishes (that
    // file's own kSilenceFloorLinear citation of
    // app/FroggersAudioRoutingTests.cpp's constant).
    constexpr float kSilenceFloorLinear = 1.0e-3f;
    REQUIRE_TRUE(settledPeak >= kSilenceFloorLinear);  // actually audible before trusting anything below.

    // -- Engage Freeze via the REAL production seam --------------------------
    // (PortableSurface().DispatchAction(kFreeze), the exact call the Freeze
    // button itself makes -- FroggersUiSurface.hpp's kFreeze branch: latch
    // engage + Stop push + SetDesiredTransportRunning(false), all in one
    // press.)
    processor.ApplicationForTest().PortableSurface().DispatchAction(
        synth::ui::Action::Named(synth_froggers::FroggersActions::kFreeze));
    // One block to drain+apply the Stop the Freeze press pushed.
    buffer.clear();
    processor.processBlock(buffer, midi);

    REQUIRE_TRUE(processor.ApplicationForTest().FreezeLatched());
    REQUIRE_TRUE(!processor.ApplicationForTest().TransportRunning());  // transport reads stopped.

    // Past the settle window a plain (unlatched) Stop would have silenced
    // within: the gate is forced open by FreezeLatched(), so output must
    // STAY audible.
    const float heldPeak = runBlocksMeasuringPeak(94);  // ~0.5s, same window FroggersVstSmokeTest.cpp's own pre-start check uses.
    REQUIRE_TRUE(heldPeak >= kSilenceFloorLinear);
    REQUIRE_TRUE(processor.ApplicationForTest().FreezeLatched());  // still latched -- nothing released it.
    REQUIRE_TRUE(!processor.ApplicationForTest().TransportRunning());  // still stopped.

    std::cout << "  [Freeze] settledPeak=" << settledPeak << " heldPeak(frozen)=" << heldPeak
              << " (>= silence floor " << kSilenceFloorLinear << ")\n";

    // Cleanup: release the latch and stop cleanly (does not restart
    // transport -- not asserted further here, out of this test's
    // scope).
    processor.releaseResources();
}

// -- 3. Tempo-follow -----------------------------------------------------------
TEST_CASE(host_tempo_follows_and_suppresses_user_requests_while_slaved_both_directions) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("tempo_follow"));
    FakePlayHead playHead;
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    auto runBlock = [&] {
        buffer.clear();
        processor.processBlock(buffer, midi);
    };

    // -- Positive control, direction 1: unslaved, user tempo works ----------
    // No playhead attached yet -- hostTempoValid_ is false by construction,
    // so this is the same "standalone hosting" state as
    // transport_absent_playhead_produces_no_messages above.
    REQUIRE_TRUE(!processor.ApplicationForTest().TempoExternallyClocked());
    processor.ApplicationForTest().RequestTempoBpm(90.0);
    runBlock();  // ProcessFrame() drains the pending request.
    REQUIRE_TRUE(std::fabs(processor.ApplicationForTest().DisplayTempoBpm() - 90.0) < 0.5);

    // -- Engage: host reports a playhead with a tempo -----------------------
    processor.setPlayHead(&playHead);
    playHead.SetPlaying(false);  // 6.3 is tempo-only; transport state is irrelevant here.
    playHead.SetBpm(140.0);
    runBlock();                              // processBlock observes bpm=140, republishes it.
    processor.PumpMessageThreadForTest();    // engages receiveClock, pushes tick #1.
    runBlock();                              // drains+applies tick #1 (establishes hasLastClock only).
    REQUIRE_TRUE(processor.ApplicationForTest().TempoExternallyClocked());  // receiveClock is now true.

    // A second, correctly-spaced tick is needed before activeBpm_ actually
    // reflects 140 (MasterClock::HandleExternalClock's own interval
    // estimator, see FroggersPluginProcessor.cpp's timerCallback() comment)
    // -- sleep past one tick interval (60e6/(24*140) ~= 17.9ms) so the next
    // pump's catch-up loop pushes ticks spaced by REAL elapsed time, then
    // pump+drain again.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    processor.PumpMessageThreadForTest();
    runBlock();

    const double displayedBpm = processor.ApplicationForTest().DisplayTempoBpm();
    std::cout << "  [6.3] host bpm=140 -> DisplayTempoBpm()=" << displayedBpm << "\n";
    REQUIRE_TRUE(std::fabs(displayedBpm - 140.0) < 5.0);
    REQUIRE_TRUE(processor.ApplicationForTest().TempoExternallyClocked());

    // -- User request suppressed while slaved --------------------------------
    processor.ApplicationForTest().RequestTempoBpm(200.0);
    runBlock();
    REQUIRE_TRUE(std::fabs(processor.ApplicationForTest().DisplayTempoBpm() - 200.0) > 1.0);  // NOT accepted.
    REQUIRE_TRUE(processor.ApplicationForTest().TempoExternallyClocked());

    // -- Positive control, direction 2: disengage, user tempo works again ---
    // The teardown concern: the host stops reporting a usable tempo
    // (here, no bpm at all) -- mirrors "the DAW vanishes."
    playHead.ClearBpm();
    runBlock();                            // hostTempoValid_ -> false.
    processor.PumpMessageThreadForTest();  // disengage: RequestSyncConfiguration({}).
    runBlock();                            // ApplySyncConfig applies the disengage.
    REQUIRE_TRUE(!processor.ApplicationForTest().TempoExternallyClocked());

    processor.ApplicationForTest().RequestTempoBpm(90.0);
    runBlock();
    REQUIRE_TRUE(std::fabs(processor.ApplicationForTest().DisplayTempoBpm() - 90.0) < 0.5);  // works again.

    processor.releaseResources();
    processor.setPlayHead(nullptr);
    std::cout << "  [6.3] user request suppressed while slaved; both positive controls pass.\n";
}

// -- 3b. Teardown --------------------------------------------------------------
// Shared "get to a known-engaged state" fixture for the two teardown tests
// below -- factored out (unlike the tempo-follow test above, which predates
// this fix and is left as-is) since both new tests need the identical
// engage sequence.
struct EngagedClockFixture {
    frogg3rs_vst::FroggersPluginProcessor processor;
    FakePlayHead playHead;
    juce::AudioBuffer<float> buffer{2, 256};
    juce::MidiBuffer midi;

    explicit EngagedClockFixture(const char* scratchName) : processor(ScratchDataPaths(scratchName)) {
        processor.setRateAndBufferSizeDetails(48000.0, 256);
        processor.prepareToPlay(48000.0, 256);
    }

    void RunBlock() {
        buffer.clear();
        processor.processBlock(buffer, midi);
    }

    // Engages host-tempo slaving and confirms it before
    // returning -- both teardown tests below start from this known state.
    void EngageAt(double bpm) {
        processor.setPlayHead(&playHead);
        playHead.SetPlaying(false);
        playHead.SetBpm(bpm);
        RunBlock();
        processor.PumpMessageThreadForTest();
        RunBlock();
        REQUIRE_TRUE(processor.ApplicationForTest().TempoExternallyClocked());
    }
};

// Data race: releaseResources() must be able to
// disengage the clock ON ITS OWN, immediately, without relying on
// processBlockCounter_'s staleness streak ever accumulating -- proving that
// requires a scenario where the streak provably never reaches
// kStaleProcessBlockPumpThreshold, i.e. exactly one processBlock() call
// after releaseResources() (needed only so ApplySyncConfig() on the audio
// thread can apply the disengage request the very next pump makes -- not
// itself part of the trigger under test).
TEST_CASE(release_resources_alone_disengages_the_clock_with_no_further_process_block) {
    EngagedClockFixture fixture("teardown_release_resources");

    fixture.EngageAt(120.0);
    REQUIRE_TRUE(fixture.processor.ApplicationForTest().TempoExternallyClocked());

    fixture.processor.releaseResources();  // sets releaseResourcesSeen_ only.
    fixture.processor.PumpMessageThreadForTest();  // consumes it, forces disengage on THIS one pump.
    fixture.RunBlock();  // lets ApplySyncConfig() apply the disengage -- verification only.

    REQUIRE_TRUE(!fixture.processor.ApplicationForTest().TempoExternallyClocked());
    fixture.processor.setPlayHead(nullptr);
    std::cout << "  [teardown] releaseResources() alone disengaged the clock on the very next pump "
                 "(no staleness streak needed).\n";
}

// Teardown gap: if the host simply stops calling
// processBlock() at all -- no releaseResources() either -- the clock must
// still disengage once REAL elapsed time (not pump count -- see the
// comment on kStaleActivityWindowMicros,
// FroggersPluginProcessor.hpp) reaches kStaleActivityWindowMicros with the
// counter unmoved. A single settling pump first is needed for a robust test
// (not part of the scenario under test): EngageAt()'s own last
// processBlock() call happens AFTER its own single pump, so
// lastActivityMicros_ has not yet observed that last increment; one more
// pump here syncs it, so the test below starts its own timing from a clean
// baseline regardless of EngageAt()'s internal call pattern.
//
// Proves BOTH triggers independently: (1) a POSITIVE CONTROL well before
// the ~1s window elapses --
// a handful of quick pumps must NOT disengage the clock, proving this is
// genuinely time-gated rather than a fixed pump count in a different shape
// -- then (2) the actual time-based trigger, reached only after sleeping
// past the real window.
TEST_CASE(clock_disengages_after_the_time_based_staleness_window_when_process_block_simply_stops) {
    EngagedClockFixture fixture("teardown_staleness_window");

    fixture.EngageAt(120.0);
    REQUIRE_TRUE(fixture.processor.ApplicationForTest().TempoExternallyClocked());
    fixture.processor.PumpMessageThreadForTest();  // settle: sync lastActivityMicros_, see comment above.

    // -- Positive control: several quick pumps, no processBlock() in
    // between, well UNDER the ~1s window -- must still read engaged.
    for (int i = 0; i < 5; ++i) {
        fixture.processor.PumpMessageThreadForTest();
    }
    REQUIRE_TRUE(fixture.processor.ApplicationForTest().TempoExternallyClocked());

    // The scenario under test: NO processBlock() call anywhere from here on
    // -- only the message thread's own pump keeps running, exactly like a
    // host that suspends/disables the track without a clean teardown call.
    // Sleep past the real kStaleActivityWindowMicros (~1s) window, with
    // margin, before pumping again.
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    fixture.processor.PumpMessageThreadForTest();
    fixture.RunBlock();  // lets ApplySyncConfig() apply the disengage -- verification only.

    REQUIRE_TRUE(!fixture.processor.ApplicationForTest().TempoExternallyClocked());
    fixture.processor.setPlayHead(nullptr);
    std::cout << "  [teardown] clock disengaged after the ~1s time-based staleness window elapsed with zero "
                 "intervening processBlock() calls (and did NOT disengage on a handful of quick pumps well "
                 "under that window).\n";
}

// -- 4. Surface row -------------------------------------------------------------
// Context-free surface build, mirroring app/FroggersSurfaceTests.cpp's own
// BuildFroggersTreeAt/FindNodeById technique (that file's own lines 84-127,
// 309-318) -- reproduced here rather than included, since that file is a
// standalone binary with its own main() app/vst/ cannot link against.
const synth::ui::Node* FindNodeById(const synth::ui::NodeTree& tree, const std::string& id) {
    for (const synth::ui::Node& node : tree.nodes) {
        if (node.id.value == id) {
            return &node;
        }
    }
    return nullptr;
}

synth::ui::NodeTree BuildFroggersTree(bool pluginHostMode) {
    synth::RuntimeConfig config = synth_froggers::FroggersApp::Config();
    synth::AppContext context;
    context.config = &config;
    synth_froggers::FroggersUiSurface surface;
    surface.Attach(&context, nullptr);
    surface.SetPluginHostMode(pluginHostMode);
    return surface.BuildTree();
}

TEST_CASE(default_mode_transport_row_is_unchanged_play_stop_freeze_record) {
    const synth::ui::NodeTree tree = BuildFroggersTree(/*pluginHostMode=*/false);
    const synth::ui::Node* row = FindNodeById(tree, synth_froggers::FroggersNodeIds::kTransportRow);
    REQUIRE_TRUE(row != nullptr);
    REQUIRE_TRUE(row->children.size() == 4);
    REQUIRE_TRUE(row->children[0].value == synth_froggers::FroggersNodeIds::kPlay);
    REQUIRE_TRUE(row->children[1].value == synth_froggers::FroggersNodeIds::kStop);
    REQUIRE_TRUE(row->children[2].value == synth_froggers::FroggersNodeIds::kFreeze);
    REQUIRE_TRUE(row->children[3].value == synth_froggers::FroggersNodeIds::kRecord);
    REQUIRE_TRUE(FindNodeById(tree, synth_froggers::FroggersNodeIds::kFreezeLabel) == nullptr);
}

TEST_CASE(plugin_mode_transport_row_thins_to_freeze_and_label_only) {
    const synth::ui::NodeTree tree = BuildFroggersTree(/*pluginHostMode=*/true);
    const synth::ui::Node* row = FindNodeById(tree, synth_froggers::FroggersNodeIds::kTransportRow);
    REQUIRE_TRUE(row != nullptr);
    // A third child joined Freeze/its label in the freed row space -- the
    // plugin's own input-channel selection control.
    REQUIRE_TRUE(row->children.size() == 3);
    REQUIRE_TRUE(row->children[0].value == synth_froggers::FroggersNodeIds::kFreeze);
    REQUIRE_TRUE(row->children[1].value == synth_froggers::FroggersNodeIds::kFreezeLabel);
    REQUIRE_TRUE(row->children[2].value == synth_froggers::FroggersNodeIds::kInputSelect);

    REQUIRE_TRUE(FindNodeById(tree, synth_froggers::FroggersNodeIds::kPlay) == nullptr);
    REQUIRE_TRUE(FindNodeById(tree, synth_froggers::FroggersNodeIds::kStop) == nullptr);
    REQUIRE_TRUE(FindNodeById(tree, synth_froggers::FroggersNodeIds::kRecord) == nullptr);

    const synth::ui::Node* freeze = FindNodeById(tree, synth_froggers::FroggersNodeIds::kFreeze);
    REQUIRE_TRUE(freeze != nullptr);
    REQUIRE_TRUE(freeze->action.has_value() && freeze->action->name == synth_froggers::FroggersActions::kFreeze);

    const synth::ui::Node* label = FindNodeById(tree, synth_froggers::FroggersNodeIds::kFreezeLabel);
    REQUIRE_TRUE(label != nullptr);
    // Builder::Label() stores its text in Node::text, not Node::label
    // (PortableUIBuilders.hpp:184-190) -- Node::label is per-kind CAPTION
    // text for controls that render their own (buttons/sliders/etc, see
    // that struct's own comment); a Label node's displayed text is `text`.
    REQUIRE_TRUE(label->text == "FREEZE");

    // A bare-context surface (app_ == nullptr, this function's
    // own Attach() call) never had SetInputOptions() called on it, so it
    // renders its own default -- just "None" -- the same "unavailable"
    // reading a disabled bus produces (inputOptionLabels_'s own NSDMI
    // comment, FroggersUiSurface.hpp). Builder::Button() stores its display
    // text in Node::label (not Node::text, unlike a Label node -- see the
    // comment on the Freeze label just above for the reverse case).
    const synth::ui::Node* inputSelect = FindNodeById(tree, synth_froggers::FroggersNodeIds::kInputSelect);
    REQUIRE_TRUE(inputSelect != nullptr);
    REQUIRE_TRUE(inputSelect->action.has_value() &&
                 inputSelect->action->name == synth_froggers::FroggersActions::kInputSelect);
    REQUIRE_TRUE(inputSelect->label == "IN: NONE");
}

// -- 5. Stable-ID host parameter surface --------------------------------------
// Every helper below reconstructs the SAME structural facts
// FroggersPluginProcessor::BuildHostParameterInventory() itself uses
// (kFroggersBankCount/kFroggersParamsPerBank/kFroggersCrispySlot/
// kFroggersCrunchySlot, FroggersParameters.hpp) -- not a copy of that
// method's own string-building, but the same INPUTS, so a test failure here
// means the plugin's actual construction order/IDs disagree with the
// model's own enumeration, not that two independent guesses disagree with
// each other.

// The expected ordered stable-ID list this plugin's construction order
// produces: bank0.slot0..13, bank0.crispy, bank1.slot0..13, bank1.crispy,
// ..., bank5.crispy, crunchy, freeze. Computed from the model's own
// constants, never a hardcoded flat list -- see this section's own header
// comment on why (a hardcoded 92-entry snapshot would duplicate
// BuildHostParameterInventory()'s derivation and silently drift if the
// model ever grows).
std::vector<std::string> ExpectedHostParamIdsInOrder() {
    std::vector<std::string> ids;
    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
        for (std::size_t paramIx = 0; paramIx < synth_froggers::kFroggersParamsPerBank; ++paramIx) {
            ids.push_back("bank" + std::to_string(bankIx) + ".slot" + std::to_string(paramIx));
        }
        ids.push_back("bank" + std::to_string(bankIx) + ".crispy");
    }
    ids.push_back("crunchy");
    ids.push_back("freeze");
    return ids;
}

juce::AudioProcessorParameter* FindHostParamById(frogg3rs_vst::FroggersPluginProcessor& processor,
                                                 const std::string& id) {
    for (auto* p : processor.getParameters()) {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p)) {
            if (ranged->getParameterID().toStdString() == id) {
                return p;
            }
        }
    }
    return nullptr;
}

// -- Count: exposed parameter count equals the model's own enumeration ------
TEST_CASE(host_parameter_count_matches_the_models_own_enumeration) {
    // Computed from FroggersParameterModel's own constants (kFroggersBankCount
    // * kFroggersParamsPerBank page parameters + kFroggersBankCount Crispy +
    // 1 shared Crunchy), never a hardcoded literal, plus 1 for Freeze (not a
    // ParameterManager parameter at all -- see BuildHostParameterInventory()'s
    // own comment).
    const std::size_t expectedModelParamCount =
        synth_froggers::kFroggersBankCount * synth_froggers::kFroggersParamsPerBank
        + synth_froggers::kFroggersBankCount + 1;
    const std::size_t expectedHostParamCount = expectedModelParamCount + 1;

    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("param_count"));
    REQUIRE_TRUE(static_cast<std::size_t>(processor.getParameters().size()) == expectedHostParamCount);
    REQUIRE_TRUE(ExpectedHostParamIdsInOrder().size() == expectedHostParamCount);

    std::cout << "  [7.2] exposed host parameter count = " << processor.getParameters().size()
              << " (model enumeration: " << expectedModelParamCount << " + 1 Freeze = " << expectedHostParamCount
              << ").\n";
}

// -- Stable IDs: deterministic across construction, structurally sound ------
TEST_CASE(host_parameter_ids_are_stable_structural_and_identical_across_construction) {
    const std::vector<std::string> expectedIds = ExpectedHostParamIdsInOrder();

    frogg3rs_vst::FroggersPluginProcessor processorA(ScratchDataPaths("param_ids_a"));
    frogg3rs_vst::FroggersPluginProcessor processorB(ScratchDataPaths("param_ids_b"));

    REQUIRE_TRUE(static_cast<std::size_t>(processorA.getParameters().size()) == expectedIds.size());
    REQUIRE_TRUE(static_cast<std::size_t>(processorB.getParameters().size()) == expectedIds.size());

    std::vector<std::string> seen;
    for (std::size_t i = 0; i < expectedIds.size(); ++i) {
        auto* rangedA = dynamic_cast<juce::RangedAudioParameter*>(processorA.getParameters()[static_cast<int>(i)]);
        auto* rangedB = dynamic_cast<juce::RangedAudioParameter*>(processorB.getParameters()[static_cast<int>(i)]);
        REQUIRE_TRUE(rangedA != nullptr && rangedB != nullptr);

        const std::string idA = rangedA->getParameterID().toStdString();
        const std::string idB = rangedB->getParameterID().toStdString();

        // Two independently constructed processors produce the IDENTICAL
        // ordered ID list (the governing requirement to "construct the
        // processor twice").
        REQUIRE_TRUE(idA == idB);
        // ...and match the structural expectation derived from the model's
        // own constants (this section's header comment).
        REQUIRE_TRUE(idA == expectedIds[i]);

        // Structural invariant: every page-parameter/Crispy ID is qualified
        // by its bank ("bankN." prefix); Crunchy/Freeze are not (exactly
        // one of each exists, globally).
        if (idA != "crunchy" && idA != "freeze") {
            REQUIRE_TRUE(idA.rfind("bank", 0) == 0);
        }
        seen.push_back(idA);
    }

    // No duplicates -- every stable ID is unique.
    std::vector<std::string> sortedSeen = seen;
    std::sort(sortedSeen.begin(), sortedSeen.end());
    REQUIRE_TRUE(std::adjacent_find(sortedSeen.begin(), sortedSeen.end()) == sortedSeen.end());

    std::cout << "  [7.2] " << expectedIds.size()
              << " stable IDs identical across two independent constructions, all unique.\n";
}

// -- Automation round-trip: host write -> core follows -> host readback -----
TEST_CASE(host_write_round_trips_through_the_core_parameter_authority) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("param_roundtrip"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    auto runBlock = [&] {
        buffer.clear();
        processor.processBlock(buffer, midi);
    };
    // ~1.5s of pumps+blocks: enough for (a) the message-thread pump to push
    // the write and (b) Parameter's own ~10Hz uiDisplayCenterAlpha one-pole
    // slew (ParameterModulation.hpp) to settle to within float epsilon.
    auto pumpAndSettle = [&] {
        for (int i = 0; i < 45; ++i) {
            processor.PumpMessageThreadForTest();
            for (int b = 0; b < 7; ++b) {
                runBlock();
            }
        }
    };

    // "bank5.slot3" = Reverb bank ("Pre-delay", FroggersBankLayouts()'s
    // Reverb row), an arbitrary non-Audio/non-bank-0 target chosen
    // specifically so this test also exercises the SelectParamBank
    // pre-step (PumpHostParameterBridge()'s own comment) -- bank 0 is
    // already selected by default (FroggersParameterModel::Init()), so a
    // bank-0 target alone would never prove that step actually runs.
    juce::AudioProcessorParameter* target = FindHostParamById(processor, "bank5.slot3");
    REQUIRE_TRUE(target != nullptr);

    constexpr float kHostTarget = 0.75f;
    target->setValue(kHostTarget);  // simulates the host applying automation.
    pumpAndSettle();

    // Assert at the REAL parameter authority (FroggersParameterModel::
    // PageParameter(5, 3), the literal Parameter object FroggersBankLayouts
    // ()[5].params[3] registers) -- not this bridge's own shadow/uiState
    // mirror.
    synth::Parameter& coreParam = processor.ApplicationForTest().Parameters().PageParameter(5, 3);
    constexpr float kTolerance = 0.01f;
    REQUIRE_TRUE(std::fabs(coreParam.UIDisplayCenter(0) - kHostTarget) < kTolerance);

    // Host readback matches too -- the core->host half of this bridge.
    REQUIRE_TRUE(std::fabs(target->getValue() - kHostTarget) < kTolerance);

    std::cout << "  [7.2] host wrote bank5.slot3=" << kHostTarget
              << " -> core UIDisplayCenter=" << coreParam.UIDisplayCenter(0) << " -> host readback=" << target->getValue()
              << ".\n";

    processor.releaseResources();
}

// -- Reverse direction: a core-side change is reflected to the host --------
TEST_CASE(core_side_randomize_is_reflected_to_every_host_parameter) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("param_reverse"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    auto runBlock = [&] {
        buffer.clear();
        processor.processBlock(buffer, midi);
    };
    auto pumpAndSettle = [&] {
        for (int i = 0; i < 45; ++i) {
            processor.PumpMessageThreadForTest();
            for (int b = 0; b < 7; ++b) {
                runBlock();
            }
        }
    };

    pumpAndSettle();  // let construction-time defaults fully mirror before snapshotting.

    std::vector<float> before(static_cast<std::size_t>(processor.getParameters().size()));
    for (int i = 0; i < processor.getParameters().size(); ++i) {
        before[static_cast<std::size_t>(i)] = processor.getParameters()[i]->getValue();
    }

    // Real production "the app itself changed the value" seam -- Randomize
    // All (FroggersAppCore::RequestRandomizeAll(), the SAME Request*/
    // pending*_ audio-thread bridge the real Randomize button drives via
    // FroggersUiSurface.hpp's kRandomizeAll branch), not a mock of the
    // bridge under test. ParameterManager's own random source is
    // fixed-seeded (`std::mt19937 randomEngine_{0x51EA5EEDu}`,
    // ParameterModulation.hpp), so this is deterministic, not flaky.
    processor.ApplicationForTest().RequestRandomizeAll();
    pumpAndSettle();

    int changedCount = 0;
    for (int i = 0; i < processor.getParameters().size(); ++i) {
        if (std::fabs(processor.getParameters()[i]->getValue() - before[static_cast<std::size_t>(i)]) > 0.01f) {
            ++changedCount;
        }
    }
    REQUIRE_TRUE(changedCount > 0);  // Randomize actually moved something.

    // Every host parameter mirrors the real parameter authority -- walked
    // in the SAME construction order this plugin itself uses, so each
    // index lines up with a specific, known core Parameter without needing
    // to parse IDs back apart.
    synth_froggers::FroggersParameterModel& model = processor.ApplicationForTest().Parameters();
    int hostIx = 0;
    constexpr float kTolerance = 0.01f;
    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
        for (std::size_t paramIx = 0; paramIx < synth_froggers::kFroggersParamsPerBank; ++paramIx) {
            const float hostValue = processor.getParameters()[hostIx++]->getValue();
            REQUIRE_TRUE(std::fabs(hostValue - model.PageParameter(bankIx, paramIx).UIDisplayCenter(0)) < kTolerance);
        }
        const float hostCrispy = processor.getParameters()[hostIx++]->getValue();
        REQUIRE_TRUE(std::fabs(hostCrispy - model.Crispy(bankIx).UIDisplayCenter(0)) < kTolerance);
    }
    const float hostCrunchy = processor.getParameters()[hostIx++]->getValue();
    REQUIRE_TRUE(std::fabs(hostCrunchy - model.Crunchy().UIDisplayCenter(0)) < kTolerance);
    // hostIx now points at Freeze -- RandomizeAll does not touch it
    // (transport-only latch), not asserted further here.

    std::cout << "  [7.2] RandomizeAll moved " << changedCount
              << "/" << processor.getParameters().size() << " host parameters; every one mirrors the core.\n";

    processor.releaseResources();
}

// -- No-feedback: bounded notifications, with a positive control -----------
struct CountingParamListener : juce::AudioProcessorParameter::Listener {
    int count = 0;
    void parameterValueChanged(int, float) override { ++count; }
    void parameterGestureChanged(int, bool) override {}
};

TEST_CASE(host_write_produces_a_bounded_number_of_notifications_not_an_endless_loop) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("param_no_feedback"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    auto runBlock = [&] {
        buffer.clear();
        processor.processBlock(buffer, midi);
    };
    auto pumpAndSettle = [&] {
        for (int i = 0; i < 45; ++i) {
            processor.PumpMessageThreadForTest();
            for (int b = 0; b < 7; ++b) {
                runBlock();
            }
        }
    };

    juce::AudioProcessorParameter* target = FindHostParamById(processor, "bank2.slot5");
    REQUIRE_TRUE(target != nullptr);

    // Let the core's own startup convergence finish BEFORE attaching the
    // listener: a parameter with a non-zero registered default slews from
    // the smoother's zero start over the first blocks, and the bridge
    // mirrors that movement as real notifications. The steady-state control
    // below is about the settled state, whatever any parameter's default is.
    pumpAndSettle();

    // setValueNotifyingHost() is the ONLY thing that fires
    // parameterValueChanged() (juce_AudioProcessorParameter.cpp: plain
    // setValue() -- what the test below calls to simulate the host's own
    // write -- does NOT notify listeners), so this listener counts
    // PumpHostParameterBridge()'s own core->host notifications exclusively,
    // never the simulated host write itself.
    CountingParamListener listener;
    target->addListener(&listener);

    // -- Positive control, part 1: steady state produces zero notifications.
    pumpAndSettle();
    REQUIRE_TRUE(listener.count == 0);

    // -- Simulated host write --------------------------------------------
    target->setValue(0.42f);
    pumpAndSettle();  // Parameter's own slew converges; several notifies may fire while it does (not a bug).
    const int countAfterSettle = listener.count;
    // Positive control, part 2: the guard can fire at all -- something WAS
    // mirrored while the core converged toward the host's target, proving
    // this is not just an always-suppress no-op.
    REQUIRE_TRUE(countAfterSettle > 0);

    // -- Bounded, not endless: well past convergence, the count must be
    // FLAT -- no further notifies once the core has genuinely settled
    // (Parameter's uiDisplayCenterAlpha one-pole reaches a true, bit-exact
    // fixed point in finite time on floating-point hardware -- see
    // PumpHostParameterBridge()'s own comment).
    pumpAndSettle();
    REQUIRE_TRUE(listener.count == countAfterSettle);

    std::cout << "  [7.2] host write -> " << countAfterSettle
              << " notifications while settling, then flat (0 more) over a further ~1.5s of pumping.\n";

    target->removeListener(&listener);
    processor.releaseResources();
}

// -- 5. Host automation addressing: bank-scoped writes, never through the
// operator's own visible page --------------------------------------------
// PumpHostParameterBridge() (FroggersPluginProcessor.cpp) resolves a host
// write directly against its own entry's bank (MessageIn::
// ParamSetAbsoluteOnBank -> synth::Bank::HandleSetAbsoluteOnTopLevel, see
// that method's own header comment). It never selects a bank on the shared
// BankSlot and never touches FroggersAppCore::activeBankIx_/drillIn_ at
// all. This proves the write reaches its target bank's real parameter
// while the operator's own view -- ActiveBankIndex(), the rendered/
// selected bank tab, and a page-scoped action (Reset Page) -- all stay on
// the bank the OPERATOR is looking at, never the one automation last wrote.
TEST_CASE(host_automation_in_a_non_visible_bank_lands_there_and_leaves_the_operators_page_untouched) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("bank_agreement"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    auto runBlock = [&] {
        buffer.clear();
        processor.processBlock(buffer, midi);
    };
    // Same "~1.5s settle" shape as the 7.2 round-trip tests above.
    auto pumpAndSettle = [&] {
        for (int i = 0; i < 45; ++i) {
            processor.PumpMessageThreadForTest();
            for (int b = 0; b < 7; ++b) {
                runBlock();
            }
        }
    };

    // FroggersParameterModel::Init() selects bank 0 by default
    // (`slot_->SelectBank(banks_[0])`).
    REQUIRE_TRUE(processor.ApplicationForTest().ActiveBankIndex() == 0);

    // Bank 3, slot 2 -- an arbitrary non-visible target. Bank 0 (the
    // operator's own, active-by-default bank), slot 6 ("Ph.mod 1") is also
    // driven, so the Reset-Page check below has an unambiguous before/after
    // on the bank that action must actually affect. Slot 6 is deliberately
    // NOT one of the Audio bank's slots 0-2/3-5 (ApplyAudioBankOverlay,
    // FroggersModulation.hpp) -- those carry a default-patch modulation
    // overlay of their own, so their post-Reset UIDisplayCenter is not a
    // stable, single constant to assert against.
    constexpr std::size_t kTargetBank = 3;
    constexpr std::size_t kTargetSlot = 2;
    constexpr std::size_t kOperatorSlot = 6;
    juce::AudioProcessorParameter* target = FindHostParamById(processor, "bank3.slot2");
    juce::AudioProcessorParameter* operatorBankTarget = FindHostParamById(processor, "bank0.slot6");
    REQUIRE_TRUE(target != nullptr);
    REQUIRE_TRUE(operatorBankTarget != nullptr);

    constexpr float kHostTarget = 0.81f;
    constexpr float kOperatorBankHostTarget = 0.65f;
    target->setValue(kHostTarget);
    operatorBankTarget->setValue(kOperatorBankHostTarget);
    pumpAndSettle();

    constexpr float kTolerance = 0.01f;
    synth::Parameter& targetCoreParam =
        processor.ApplicationForTest().Parameters().PageParameter(kTargetBank, kTargetSlot);
    synth::Parameter& operatorBankParam =
        processor.ApplicationForTest().Parameters().PageParameter(0, kOperatorSlot);
    REQUIRE_TRUE(std::fabs(targetCoreParam.UIDisplayCenter(0) - kHostTarget) < kTolerance);  // the write landed.
    REQUIRE_TRUE(std::fabs(operatorBankParam.UIDisplayCenter(0) - kOperatorBankHostTarget) < kTolerance);

    // -- The operator's own bank never moved --------------------------------
    REQUIRE_TRUE(processor.ApplicationForTest().ActiveBankIndex() == 0);

    // -- The REAL selection authority (BankSlot::SelectedBank(), published
    // via ParameterManager::PopulateUIState to uiState->banks[].selected)
    // agrees too, and this is SPECIFICALLY what the editor renders:
    // FroggersUiSurface::CurrentBankIndex() (app/FroggersUiSurface.hpp:
    // 1872-1882) reads this live state directly, never ActiveBankIndex().
    // Read via the SAME BuildTree() call the editor's PortableComponent
    // makes every refresh.
    const synth::ui::NodeTree tree = processor.ApplicationForTest().PortableSurface().BuildTree();
    const synth::ui::Node* targetBankTab =
        FindNodeById(tree, synth_froggers::FroggersNodeIds::BankButton(kTargetBank));
    REQUIRE_TRUE(targetBankTab != nullptr);
    REQUIRE_TRUE(!targetBankTab->selected);  // never flipped to the automated bank.
    const synth::ui::Node* startingBankTab = FindNodeById(tree, synth_froggers::FroggersNodeIds::BankButton(0));
    REQUIRE_TRUE(startingBankTab != nullptr);
    REQUIRE_TRUE(startingBankTab->selected);  // still the operator's own bank.

    // -- Page-scoped action (Reset Page) targets the OPERATOR's bank -------
    // RandomizePage/ResetPage both act on *drillIn_ (FroggersAppCore.hpp:
    // 692-694/708-709), which only ever moves via RequestBankSelect() -- no
    // longer called anywhere on the host-automation path -- so it stays
    // bound to the real active bank (0) regardless of where automation last
    // wrote. At drill level 0, ResetPage reverts the whole bank via
    // ResetBankToDefaultPatch -> ApplyBankDefaultPatch (FroggersModulation.hpp),
    // which sets each page parameter back to its OWN registered
    // FroggersParamSpec::defaultValue -- read directly from that same
    // in-domain source below, not re-derived or assumed to be 0.0.
    const float kResetPageTarget = synth_froggers::FroggersBankLayouts()[0].params[kOperatorSlot].defaultValue;
    processor.ApplicationForTest().RequestResetPage();
    pumpAndSettle();

    REQUIRE_TRUE(std::fabs(operatorBankParam.UIDisplayCenter(0) - kResetPageTarget) < kTolerance);  // reset bank 0.
    REQUIRE_TRUE(std::fabs(targetCoreParam.UIDisplayCenter(0) - kHostTarget) < kTolerance);  // bank 3 untouched.
    REQUIRE_TRUE(std::fabs(target->getValue() - kHostTarget) < kTolerance);  // host readback still agrees.

    std::cout << "  [host automation] wrote bank " << kTargetBank << " slot " << kTargetSlot
              << " -> ActiveBankIndex() stayed " << processor.ApplicationForTest().ActiveBankIndex()
              << ", and RequestResetPage() reset THAT bank's own parameter (-> "
              << operatorBankParam.UIDisplayCenter(0) << ") while leaving bank " << kTargetBank << " at "
              << targetCoreParam.UIDisplayCenter(0) << ".\n";

    processor.releaseResources();
}

// -- Two banks automated in the same pump must not oscillate the visible
// page -- there is no bank-select message on this path at all any more
// (see PumpHostParameterBridge()'s own header comment), so there is
// nothing left here that could move it, no matter how many banks a single
// pump's worth of host writes touches.
TEST_CASE(host_automation_of_two_banks_in_one_pump_does_not_move_the_visible_page) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("bank_no_oscillate"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    auto runBlock = [&] {
        buffer.clear();
        processor.processBlock(buffer, midi);
    };
    auto pumpAndSettle = [&] {
        for (int i = 0; i < 45; ++i) {
            processor.PumpMessageThreadForTest();
            for (int b = 0; b < 7; ++b) {
                runBlock();
            }
        }
    };

    REQUIRE_TRUE(processor.ApplicationForTest().ActiveBankIndex() == 0);

    juce::AudioProcessorParameter* targetA = FindHostParamById(processor, "bank2.slot1");
    juce::AudioProcessorParameter* targetB = FindHostParamById(processor, "bank4.slot6");
    REQUIRE_TRUE(targetA != nullptr);
    REQUIRE_TRUE(targetB != nullptr);

    constexpr float kTargetAValue = 0.33f;
    constexpr float kTargetBValue = 0.77f;
    // Both host writes are visible to PumpHostParameterBridge() the SAME
    // pump -- it walks all of hostParams_ in one pass, so this is exactly
    // the "two different banks in one pump" case.
    targetA->setValue(kTargetAValue);
    targetB->setValue(kTargetBValue);
    pumpAndSettle();

    constexpr float kTolerance = 0.01f;
    synth::Parameter& coreA = processor.ApplicationForTest().Parameters().PageParameter(2, 1);
    synth::Parameter& coreB = processor.ApplicationForTest().Parameters().PageParameter(4, 6);
    REQUIRE_TRUE(std::fabs(coreA.UIDisplayCenter(0) - kTargetAValue) < kTolerance);  // both writes landed.
    REQUIRE_TRUE(std::fabs(coreB.UIDisplayCenter(0) - kTargetBValue) < kTolerance);

    // -- Neither write, nor their combination, moved the visible page ------
    REQUIRE_TRUE(processor.ApplicationForTest().ActiveBankIndex() == 0);
    const synth::ui::NodeTree tree = processor.ApplicationForTest().PortableSurface().BuildTree();
    REQUIRE_TRUE(FindNodeById(tree, synth_froggers::FroggersNodeIds::BankButton(0))->selected);
    REQUIRE_TRUE(!FindNodeById(tree, synth_froggers::FroggersNodeIds::BankButton(2))->selected);
    REQUIRE_TRUE(!FindNodeById(tree, synth_froggers::FroggersNodeIds::BankButton(4))->selected);

    std::cout << "  [no oscillation] bank2.slot1=" << coreA.UIDisplayCenter(0) << ", bank4.slot6="
              << coreB.UIDisplayCenter(0) << ", ActiveBankIndex() stayed "
              << processor.ApplicationForTest().ActiveBankIndex() << " throughout.\n";

    processor.releaseResources();
}

// -- An open modulation drilldown survives a cross-bank host write --------
// The operator's own bank (0) is drilled into one of its own parameters; a
// host write lands in a DIFFERENT bank (3). Since that write never selects
// a bank and never touches FroggersAppCore::drillIn_ (see
// PumpHostParameterBridge()'s own header comment), the open drilldown must
// still be open, on the same selected parameter, afterward.
TEST_CASE(open_modulation_drilldown_survives_a_cross_bank_host_write) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("drilldown_survives_cross_bank"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    auto runBlock = [&] {
        buffer.clear();
        processor.processBlock(buffer, midi);
    };
    auto pumpAndSettle = [&] {
        for (int i = 0; i < 45; ++i) {
            processor.PumpMessageThreadForTest();
            for (int b = 0; b < 7; ++b) {
                runBlock();
            }
        }
    };

    REQUIRE_TRUE(processor.ApplicationForTest().ActiveBankIndex() == 0);

    // Drill into bank 0's own slot 5 -- the real operator press seam
    // (FroggersAppCore::RequestEncoderPress(), drained by ProcessFrame() on
    // the very next block; see FroggersModulation.hpp's
    // FroggersModulationDrillIn::PressEncoder()).
    constexpr std::size_t kDrilledSlot = 5;
    processor.ApplicationForTest().RequestEncoderPress(kDrilledSlot);
    runBlock();

    synth::Bank& bank0 = processor.ApplicationForTest().ActiveDrillIn().BankRef();
    synth::Parameter& drilledParam = processor.ApplicationForTest().Parameters().PageParameter(0, kDrilledSlot);
    REQUIRE_TRUE(bank0.ShowingModulation());
    REQUIRE_TRUE(bank0.SelectedParameter() == &drilledParam);
    REQUIRE_TRUE(processor.ApplicationForTest().ActiveDrillIn().Level() == 1);

    // -- Cross-bank host write ----------------------------------------------
    juce::AudioProcessorParameter* target = FindHostParamById(processor, "bank3.slot2");
    REQUIRE_TRUE(target != nullptr);
    constexpr float kHostTarget = 0.81f;
    target->setValue(kHostTarget);
    pumpAndSettle();

    constexpr float kTolerance = 0.01f;
    synth::Parameter& targetCoreParam = processor.ApplicationForTest().Parameters().PageParameter(3, 2);
    REQUIRE_TRUE(std::fabs(targetCoreParam.UIDisplayCenter(0) - kHostTarget) < kTolerance);  // the write landed.

    // -- The drilldown is untouched -----------------------------------------
    REQUIRE_TRUE(processor.ApplicationForTest().ActiveBankIndex() == 0);
    REQUIRE_TRUE(bank0.ShowingModulation());
    REQUIRE_TRUE(bank0.SelectedParameter() == &drilledParam);
    REQUIRE_TRUE(processor.ApplicationForTest().ActiveDrillIn().Level() == 1);

    std::cout << "  [drilldown survives] bank 3 slot 2 automated to " << kHostTarget
              << "; bank 0's drilldown on slot " << kDrilledSlot << " is still open.\n";

    processor.releaseResources();
}

// -- THE IMPORTANT CASE: a same-bank host write during a drilldown must
// land on the TOP-LEVEL parameter, never on the modulation-depth cell the
// SAME physical encoder maps to while drilled in --------------------------
// The operator's own bank (0) is drilled into slot 5's modulation view;
// that view remaps EVERY physical encoder (Bank::OpenModulationView,
// External/Sheaf/projects/synth/src/ParameterModulation.cpp:2833-2879) to a
// modulation-depth cell, including slot 0 -- the position this test then
// automates. Removing the MessageIn::SelectParamBank push -- it
// used to force the bank's visible page back to top level as a side effect
// -- puts this case at risk: MessageIn::ParamSetAbsoluteOnBank must resolve
// against bankIx's own TOP-LEVEL mapping (synth::Bank::
// HandleSetAbsoluteOnTopLevel) regardless of what the bank currently has
// visible, or the write would silently land on whatever depth cell slot 0
// happens to show instead.
TEST_CASE(host_automation_of_the_viewed_banks_own_parameter_lands_on_top_level_not_the_drilldowns_depth_cell) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("same_bank_write_during_drilldown"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    auto runBlock = [&] {
        buffer.clear();
        processor.processBlock(buffer, midi);
    };
    auto pumpAndSettle = [&] {
        for (int i = 0; i < 45; ++i) {
            processor.PumpMessageThreadForTest();
            for (int b = 0; b < 7; ++b) {
                runBlock();
            }
        }
    };

    REQUIRE_TRUE(processor.ApplicationForTest().ActiveBankIndex() == 0);

    // Drill into slot 5 -- distinct from slot 0, the position this test
    // writes below, so top-level and depth-cell targets are unambiguously
    // different parameters.
    constexpr std::size_t kDrilledSlot = 5;
    processor.ApplicationForTest().RequestEncoderPress(kDrilledSlot);
    runBlock();

    synth::Bank& bank0 = processor.ApplicationForTest().ActiveDrillIn().BankRef();
    synth::Parameter& drilledParam = processor.ApplicationForTest().Parameters().PageParameter(0, kDrilledSlot);
    REQUIRE_TRUE(bank0.ShowingModulation());
    REQUIRE_TRUE(bank0.SelectedParameter() == &drilledParam);

    // Slot 0's own physical encoder now shows the drilled-in parameter's
    // modulation-depth cell for modulator 0 (Random S&H 1 -- registered
    // `connected=true` unconditionally, FroggersModulation.hpp's
    // RegisterSources(), so this cell is always materialized, never null).
    synth::Parameter* depthCellAtSlot0 = drilledParam.ModulationDepthParameter(0);
    REQUIRE_TRUE(depthCellAtSlot0 != nullptr);
    REQUIRE_TRUE(bank0.VisibleParameter(0) == depthCellAtSlot0);
    const float depthBefore = depthCellAtSlot0->UIDisplayCenter(0);

    synth::Parameter& topLevelSlot0Param = processor.ApplicationForTest().Parameters().PageParameter(0, 0);
    const float topLevelBefore = topLevelSlot0Param.UIDisplayCenter(0);

    // -- Automate slot 0, in THIS same, currently-drilled-into bank --------
    juce::AudioProcessorParameter* target = FindHostParamById(processor, "bank0.slot0");
    REQUIRE_TRUE(target != nullptr);
    constexpr float kHostTarget = 0.81f;
    // An unambiguous positive control: the target is far
    // from both the top-level parameter's and the depth cell's own current
    // values, so whichever one actually moved is unmistakable.
    REQUIRE_TRUE(std::fabs(topLevelBefore - kHostTarget) > 0.1f);
    REQUIRE_TRUE(std::fabs(depthBefore - kHostTarget) > 0.1f);
    target->setValue(kHostTarget);
    pumpAndSettle();

    constexpr float kTolerance = 0.01f;
    // The write landed on the TOP-LEVEL parameter...
    REQUIRE_TRUE(std::fabs(topLevelSlot0Param.UIDisplayCenter(0) - kHostTarget) < kTolerance);
    // ...and the depth cell the drilldown shows at that same physical
    // position is untouched.
    REQUIRE_TRUE(std::fabs(depthCellAtSlot0->UIDisplayCenter(0) - depthBefore) < kTolerance);
    // The drilldown itself is still exactly where it was.
    REQUIRE_TRUE(bank0.ShowingModulation());
    REQUIRE_TRUE(bank0.SelectedParameter() == &drilledParam);

    std::cout << "  [top-level not depth-cell] bank0.slot0 automated to " << kHostTarget << " -> top-level="
              << topLevelSlot0Param.UIDisplayCenter(0) << " (target), depth cell stayed "
              << depthCellAtSlot0->UIDisplayCenter(0) << " (was " << depthBefore << ").\n";

    processor.releaseResources();
}

// -- POSITIVE CONTROL: the operator's OWN bank-select action does move the
// visible page -- without this, every "the page did not move" assertion
// above would prove only that nothing moves anything.
TEST_CASE(operator_selecting_a_bank_does_move_the_visible_page) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("operator_bank_select_moves_page"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    auto runBlock = [&] {
        buffer.clear();
        processor.processBlock(buffer, midi);
    };

    REQUIRE_TRUE(processor.ApplicationForTest().ActiveBankIndex() == 0);

    constexpr std::size_t kOperatorTargetBank = 4;
    // The exact same public seam FroggersUiSurface.hpp's own bank buttons
    // call (app/FroggersUiSurface.hpp's `HandleAction`).
    processor.ApplicationForTest().RequestBankSelect(kOperatorTargetBank);
    runBlock();  // ProcessFrame() drains the pending request -- ActiveBankIndex() itself moves this block.
    REQUIRE_TRUE(processor.ApplicationForTest().ActiveBankIndex() == kOperatorTargetBank);

    // BuildTree() below reads synth::ParameterManager::UIState, which
    // Engine::ProcessBlock() only re-publishes every uiPublishInterval_
    // blocks (Engine.hpp's own UI-state publish throttle) -- run a handful
    // more blocks so at least one publish has definitely landed before
    // reading it.
    for (int i = 0; i < 10; ++i) {
        runBlock();
    }
    const synth::ui::NodeTree tree = processor.ApplicationForTest().PortableSurface().BuildTree();
    REQUIRE_TRUE(FindNodeById(tree, synth_froggers::FroggersNodeIds::BankButton(kOperatorTargetBank))->selected);
    REQUIRE_TRUE(!FindNodeById(tree, synth_froggers::FroggersNodeIds::BankButton(0))->selected);

    std::cout << "  [positive control] RequestBankSelect(" << kOperatorTargetBank << ") -> ActiveBankIndex()="
              << processor.ApplicationForTest().ActiveBankIndex() << ".\n";

    processor.releaseResources();
}

// -- 6. Plugin-mode surface tree, end-to-end through the REAL processor
// ----------------------------------------------------------------------------
// The existing plugin_mode_transport_row_thins_to_freeze_and_label_only
// test above already proves FroggersUiSurface's OWN branching logic is
// correct GIVEN pluginHostMode_ == true, using a bare, hand-constructed
// surface THIS FILE sets the flag on directly (BuildFroggersTree()). What
// that test cannot prove -- because it never constructs a real
// FroggersPluginProcessor at all -- is that PRODUCTION actually calls
// SetPluginHostMode(true) anywhere (FroggersPluginProcessor.cpp's
// constructor is its first production caller). This test drives the REAL
// processor and reads the REAL engine_.Application().PortableSurface() --
// the exact instance the editor's PortableComponent renders -- to close
// that gap, and combines it with an ENGAGED host clock (EngagedClockFixture,
// already defined above for the teardown tests) to additionally prove the
// BPM control's "display-only while slaved" rendering (governed entirely by
// TempoExternallyClocked(), AppendBpmControl() -- app/FroggersUiSurface.hpp:
// 1272-1300 -- independent of pluginHostMode_ itself) still works correctly
// for a plugin-hosted surface. Extends, not duplicates, the branching-logic
// coverage above.
TEST_CASE(production_processor_surface_is_plugin_mode_with_bpm_display_only_while_host_tempo_engaged) {
    EngagedClockFixture fixture("plugin_mode_surface_tree");
    fixture.EngageAt(128.0);

    const synth::ui::NodeTree tree = fixture.processor.ApplicationForTest().PortableSurface().BuildTree();

    // -- Plugin-mode row shape: belt-and-suspenders confirmation of the
    // branching-logic proof above, now through the REAL production object
    // graph.
    const synth::ui::Node* row = FindNodeById(tree, synth_froggers::FroggersNodeIds::kTransportRow);
    REQUIRE_TRUE(row != nullptr);
    // A third child joined Freeze/its label -- see
    // plugin_mode_transport_row_thins_to_freeze_and_label_only's own
    // updated comment for why.
    REQUIRE_TRUE(row->children.size() == 3);
    REQUIRE_TRUE(row->children[0].value == synth_froggers::FroggersNodeIds::kFreeze);
    REQUIRE_TRUE(row->children[1].value == synth_froggers::FroggersNodeIds::kFreezeLabel);
    REQUIRE_TRUE(row->children[2].value == synth_froggers::FroggersNodeIds::kInputSelect);
    REQUIRE_TRUE(FindNodeById(tree, synth_froggers::FroggersNodeIds::kPlay) == nullptr);
    REQUIRE_TRUE(FindNodeById(tree, synth_froggers::FroggersNodeIds::kStop) == nullptr);
    REQUIRE_TRUE(FindNodeById(tree, synth_froggers::FroggersNodeIds::kRecord) == nullptr);
    const synth::ui::Node* freezeLabel = FindNodeById(tree, synth_froggers::FroggersNodeIds::kFreezeLabel);
    REQUIRE_TRUE(freezeLabel != nullptr && freezeLabel->text == "FREEZE");

    // -- BPM display-only while host-tempo-slaved (the governing explicit
    // requirement) -- AppendBpmControl() emits a StatusText (not a Slider)
    // and no adjacent kBpmLabel while TempoExternallyClocked() is true.
    const synth::ui::Node* bpm = FindNodeById(tree, synth_froggers::FroggersNodeIds::kBpm);
    REQUIRE_TRUE(bpm != nullptr);
    REQUIRE_TRUE(bpm->kind == synth::ui::NodeKind::StatusText);
    REQUIRE_TRUE(FindNodeById(tree, synth_froggers::FroggersNodeIds::kBpmLabel) == nullptr);

    fixture.processor.releaseResources();
    fixture.processor.setPlayHead(nullptr);
    std::cout << "  [8.2] production processor's surface: plugin-mode transport row + BPM StatusText \""
              << bpm->text << "\" while host-tempo-slaved.\n";
}

// -- Pre-audio editor rendering ----------------------------------------------
// A DAW may open a plugin's editor before ever calling processBlock() -- the
// operator drops the plugin on a track and looks at it before pressing
// play. Parameter controls must already show their real values at that
// moment, not blank/default-looking ones. The seam that makes this possible
// is engine_.MessageThreadTick()'s own claim-and-publish of
// ParameterManager::UIState (the pinned External/Sheaf Engine.hpp), which
// runs from this plugin's timerCallback() -- the SAME function
// PumpMessageThreadForTest() drives -- independently of
// processBlock()'s own (audio-thread-only) publish. This test never calls
// processBlock() at all.
TEST_CASE(editor_surface_renders_real_parameter_values_before_any_process_block_call) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("pre_audio_render"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);

    // Positive control: push a page parameter to a value distinctly away
    // from both its own registered default AND the grid's background-arc
    // midpoint (0.5, drawn for every cell regardless of state) -- directly
    // on the core Parameter object, via Parameter::HandleSetAbsolute (the
    // SAME call a host-automation write or an operator's encoder turn
    // ultimately makes, ParameterModulation.cpp) followed by directly
    // driving Parameter::ProcessSample() itself, NOT
    // processor.processBlock(): this settles UIDisplayCenter()'s own
    // one-pole slew toward the new target entirely off the audio pump, the
    // exact technique the pinned External/Sheaf commit's own pre-audio-
    // population test uses (engine_tests.cpp,
    // engine_message_thread_populates_ui_state_before_audio_ever_runs:
    // "with NO engine.ProcessBlock(...) call anywhere in this test").
    // Bank 0 is already the visible/active bank by default (no bank-select
    // pump needed). Slot 6 ("Ph.mod 1"), not slots 0-2/3-5: those carry the
    // Audio bank's own default-patch modulation overlay
    // (ApplyAudioBankOverlay, FroggersModulation.hpp -- see this file's own
    // host_automation_in_a_non_visible_bank_lands_there_and_leaves_the_
    // operators_page_untouched comment on why that makes their
    // UIDisplayCenter not a single stable constant to target).
    constexpr std::size_t kBank = 0;
    constexpr std::size_t kSlot = 6;
    constexpr float kNonDefaultTarget = 0.8f;
    synth::Parameter& probe = processor.ApplicationForTest().Parameters().PageParameter(kBank, kSlot);
    const float defaultValue = synth_froggers::FroggersBankLayouts()[kBank].params[kSlot].defaultValue;
    REQUIRE_TRUE(std::fabs(kNonDefaultTarget - defaultValue) > 0.2f);   // distinctly non-default.
    REQUIRE_TRUE(std::fabs(kNonDefaultTarget - 0.5f) > 0.2f);           // distinctly off the background arc too.
    probe.HandleSetAbsolute(probe.Group().Manager().Scene(), kNonDefaultTarget);
    constexpr std::uint64_t kSettleSamples = 20000;
    for (std::uint64_t sample = 0; sample < kSettleSamples; ++sample) {
        probe.ProcessSample(sample);
    }
    constexpr float kTolerance = 0.01f;
    REQUIRE_TRUE(std::fabs(probe.UIDisplayCenter(0) - kNonDefaultTarget) < kTolerance);  // settled pre-audio; sanity only.

    // The only pump this test ever runs -- no processBlock() call anywhere
    // in this test. PumpMessageThreadForTest() is timerCallback() itself
    // (this file's own header comment's "Transport edges" section), which fires
    // engine_.MessageThreadTick() first, the claim-and-publish seam under
    // test here.
    processor.PumpMessageThreadForTest();

    // The rendered tree: the SAME BuildTree() call the editor's
    // PortableComponent makes every refresh (see
    // host_automation_in_a_non_visible_bank_lands_there_and_leaves_the_
    // operators_page_untouched's own comment on this).
    const synth::ui::NodeTree tree = processor.EditorSurface().BuildTree();
    const synth::ui::Node* encoder = FindNodeById(tree, synth_froggers::FroggersNodeIds::Encoder(kSlot));
    REQUIRE_TRUE(encoder != nullptr);
    // AppendEncoderCell()'s Draw lambda returns an EMPTY command list for a
    // disconnected cell (BuildEncoderDrawCommands' own early return,
    // EncoderDraw.hpp) -- i.e. exactly the blank surface a never-populated
    // UIState renders. Real content here is already evidence the cell
    // published as connected, not merely that a node with this id exists.
    REQUIRE_TRUE(!encoder->drawCommands.empty());

    // Real VALUE, not just presence: the ring's motion-indicator layers
    // (AppendMotionIndicator, EncoderDraw.hpp) each draw a thin Arc
    // SYMMETRIC around the current value via
    // EncoderGeometry::ValueToArcAngle(), a pure linear map -- so the
    // midpoint of any one layer's start/end angle is exactly
    // ValueToArcAngle(value), independent of cell size or display spread
    // (the settle loop above already drove that residual toward 0 too, the
    // same one-pole tracking UIDisplayCenter() itself converges through).
    // Counting these proves the rendered ring reflects kNonDefaultTarget
    // specifically, not a stale or blank state that happened to still
    // produce SOME arcs.
    const float expectedAngle = synth::ui::EncoderGeometry::ValueToArcAngle(kNonDefaultTarget);
    int matchingArcs = 0;
    for (const synth::ui::DrawCommand& command : encoder->drawCommands) {
        if (command.kind != synth::ui::DrawCommand::Kind::Arc) {
            continue;
        }
        const float midAngle = (command.startRadians + command.endRadians) * 0.5f;
        if (std::fabs(midAngle - expectedAngle) < 1e-3f) {
            ++matchingArcs;
        }
    }
    // Five arcs land on the value's own angle with no active modulation
    // depths (this cell's own state): the min/max range arc collapses to a
    // single point AT the value (currentMinValues_==currentMaxValues_==
    // value, EncoderDraw.hpp's own AppendArcWithSwitchGaps call for it),
    // plus AppendMotionIndicator's four layers (outline/outer/mid/core),
    // all drawn at the identical start/end angle pair. The grid's own
    // full-circle background track (0..1) never matches (its own midpoint
    // is angle(0.5), asserted distinctly off above).
    REQUIRE_TRUE(matchingArcs == 5);

    std::cout << "  [pre-audio render] bank0.slot6 set to " << kNonDefaultTarget << " (default " << defaultValue
              << ") pre-audio; one message-thread pump alone rendered " << matchingArcs
              << " ring arcs at the matching angle, with no processBlock() call.\n";

    processor.releaseResources();
}

// -- DAW session-state persistence -------------------------------------------
// Three properties, each its own TEST_CASE below:
//   1. Round trip: host-automated values survive getStateInformation() ->
//      setStateInformation() on a FRESH processor, at both the parameter
//      authority and the host-parameter readback.
//   2. Parameter-model growth: a stored document with fewer keys than the
//      current model leaves the missing ones at default; a stored document
//      with a key the current model does not recognize is tolerated, not
//      rejected.
//   3. No write-through: a save/restore cycle never touches the shared
//      data root's patches directory, with a positive control proving the
//      detector used below actually can see a write into that directory.

// Drives `processor` through enough message-thread pumps and audio blocks
// for a host-parameter write (or a session-state restore, which lands on
// the SAME per-block patch-bus drain, DrainPatchInputBus() inside
// engine_.ProcessBlock()) to fully apply and settle -- same budget as the
// existing host_write_round_trips_through_the_core_parameter_authority
// test above, reused rather than re-derived.
void PumpAndSettle(frogg3rs_vst::FroggersPluginProcessor& processor, juce::AudioBuffer<float>& buffer,
                   juce::MidiBuffer& midi) {
    for (int i = 0; i < 45; ++i) {
        processor.PumpMessageThreadForTest();
        for (int b = 0; b < 7; ++b) {
            buffer.clear();
            processor.processBlock(buffer, midi);
        }
    }
}

// Parses `patchJsonText` (a full "sheaf.synth.patch" document, e.g. from
// getStateInformation()) and returns a document with the SAME schema/
// patchName whose parameterValues object carries only the keys named in
// `keepNames` -- every other real parameter this build knows about is
// simply absent, standing in for "a session saved before those parameters
// existed." Each kept value is copied by reference from the original
// document (both live in the same fresh arena here), so its internal shape
// is exactly what a real save produces -- never hand-rolled.
std::string BuildPatchTextWithOnlyTheseParameterKeys(const std::string& patchJsonText,
                                                     const std::vector<std::string>& keepNames) {
    synth::JsonArena arena(256 * 1024);
    synth::JSON original = arena.Loads(patchJsonText.c_str());
    REQUIRE_TRUE(!original.IsNull());
    synth::JSON originalValues = original.Get("parameterValues");
    REQUIRE_TRUE(!originalValues.IsNull());

    synth::JSON trimmedValues = arena.Object();
    for (const std::string& name : keepNames) {
        synth::JSON value = originalValues.Get(name.c_str());
        REQUIRE_TRUE(!value.IsNull());
        trimmedValues.SetNew(name.c_str(), value);
    }

    // A fresh top-level object, not `original` with parameterValues
    // overwritten in place: JSON::SetNew() always APPENDS a member
    // (Json.hpp's own implementation -- no existing-key search), so
    // calling it again for a key `original` already has (parameterValues
    // itself) would leave TWO same-named members on one object rather than
    // replacing the first -- and JSON::Get() returns the FIRST match, so
    // the untrimmed original would keep winning silently. Every field
    // below is set exactly once, so no such duplicate can arise here.
    synth::JSON result = arena.Object();
    result.SetNew("schema", original.Get("schema"));
    result.SetNew("schemaVersion", original.Get("schemaVersion"));
    result.SetNew("patchName", original.Get("patchName"));
    result.SetNew("parameterValues", trimmedValues);

    char* dumped = result.Dumps(JSON_ENCODE_ANY);
    REQUIRE_TRUE(dumped != nullptr);
    std::string text(dumped);
    std::free(dumped);
    return text;
}

// Parses `patchJsonText` and returns a document identical to it except for
// one extra parameterValues key no real parameter is ever named --
// standing in for "a session saved by a build with a bank/slot this one
// predates." The extra value's shape does not matter: ParameterManager::
// LoadParameterValuesFromJSON (ParameterModulation.cpp) looks a key up by
// name first and, on a miss, skips straight to the next member without
// ever reading the value.
std::string BuildPatchTextWithAnExtraUnknownParameterKey(const std::string& patchJsonText) {
    synth::JsonArena arena(256 * 1024);
    synth::JSON root = arena.Loads(patchJsonText.c_str());
    REQUIRE_TRUE(!root.IsNull());
    synth::JSON values = root.Get("parameterValues");
    REQUIRE_TRUE(!values.IsNull());
    values.SetNew("Bank9000 Future Slot 99", arena.Object());

    char* dumped = root.Dumps(JSON_ENCODE_ANY);
    REQUIRE_TRUE(dumped != nullptr);
    std::string result(dumped);
    std::free(dumped);
    return result;
}

// Parses `patchJsonText` and returns an equivalent document with no
// "sessionExtras" key at all -- standing in for a session saved by a build
// that predates that key, as opposed to one that has it. Built the same
// "fresh top-level object, each field set exactly once" way as
// BuildPatchTextWithOnlyTheseParameterKeys above, for the same reason (SetNew
// always appends; overwriting an existing key in place would leave a
// duplicate rather than replacing it).
std::string BuildPatchTextWithoutSessionExtras(const std::string& patchJsonText) {
    synth::JsonArena arena(256 * 1024);
    synth::JSON original = arena.Loads(patchJsonText.c_str());
    REQUIRE_TRUE(!original.IsNull());

    synth::JSON result = arena.Object();
    result.SetNew("schema", original.Get("schema"));
    result.SetNew("schemaVersion", original.Get("schemaVersion"));
    result.SetNew("patchName", original.Get("patchName"));
    result.SetNew("parameterValues", original.Get("parameterValues"));

    char* dumped = result.Dumps(JSON_ENCODE_ANY);
    REQUIRE_TRUE(dumped != nullptr);
    std::string text(dumped);
    std::free(dumped);
    return text;
}

// Builds a source processor on its own scratch root, moves one host
// parameter to `target`, settles, saves, and returns the saved state with
// "sessionExtras" stripped out -- shared setup for the two legacy-blob tests
// below, which differ only in what the RESTORE target's Freeze latch starts
// at. The caller already knows `hostParamId`/`target` (it chose them), so it
// asserts the parameter round trip itself; this only builds the blob.
std::string BuildLegacyBlobWithOneParameterMoved(const char* scratchName, const char* hostParamId, float target) {
    frogg3rs_vst::FroggersPluginProcessor source(ScratchDataPaths(scratchName));
    source.setRateAndBufferSizeDetails(48000.0, 256);
    source.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> sourceBuffer(2, 256);
    juce::MidiBuffer midi;

    juce::AudioProcessorParameter* hostParam = FindHostParamById(source, hostParamId);
    REQUIRE_TRUE(hostParam != nullptr);
    hostParam->setValue(target);
    PumpAndSettle(source, sourceBuffer, midi);

    juce::MemoryBlock fullState;
    source.getStateInformation(fullState);
    source.releaseResources();
    const std::string fullText(static_cast<const char*>(fullState.getData()), fullState.getSize());
    return BuildPatchTextWithoutSessionExtras(fullText);
}

// Every regular file under `root`, as "path:size:mtime" lines, sorted and
// joined -- two snapshots compare equal iff the tree is the same set of
// files with the same content-relevant metadata (a content change without
// a size change is not a case any code path under test here can produce:
// every write in this file is a whole-file create, never an in-place
// edit).
std::string SnapshotDirectoryTree(const std::filesystem::path& root) {
    std::vector<std::string> entries;
    if (std::filesystem::exists(root)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            // static_cast to long long: file_time_type's duration rep is a
            // 128-bit integer on this platform (libc++), which has no
            // operator<< overload -- a 64-bit truncation is exactly as
            // sufficient here as the full width, since this value is only
            // ever compared for equality within one test run, never parsed
            // back or compared across runs.
            std::ostringstream oss;
            oss << entry.path().string() << ":" << entry.file_size() << ":"
                << static_cast<long long>(entry.last_write_time().time_since_epoch().count());
            entries.push_back(oss.str());
        }
    }
    std::sort(entries.begin(), entries.end());
    std::ostringstream combined;
    for (const std::string& line : entries) {
        combined << line << "\n";
    }
    return combined.str();
}

// -- 1. Round trip ------------------------------------------------------------
TEST_CASE(state_information_round_trips_through_a_fresh_processor) {
    frogg3rs_vst::FroggersPluginProcessor source(ScratchDataPaths("state_roundtrip_source"));
    source.setRateAndBufferSizeDetails(48000.0, 256);
    source.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> sourceBuffer(2, 256);
    juce::MidiBuffer midi;

    // Two distinct, non-default targets on two different banks, written
    // through the SAME host-automation path a DAW would use (also
    // exercises the SelectParamBank pre-step, like the existing
    // host_write_round_trips_through_the_core_parameter_authority test
    // above).
    juce::AudioProcessorParameter* targetA = FindHostParamById(source, "bank1.slot4");
    juce::AudioProcessorParameter* targetB = FindHostParamById(source, "bank4.crispy");
    REQUIRE_TRUE(targetA != nullptr && targetB != nullptr);
    constexpr float kTargetA = 0.85f;
    constexpr float kTargetB = 0.2f;
    targetA->setValue(kTargetA);
    targetB->setValue(kTargetB);
    PumpAndSettle(source, sourceBuffer, midi);

    // Assert at the authority BEFORE saving -- so the restore assertion
    // below is against a known, real value, not against the host write's
    // own target (which would pass even if getStateInformation() captured
    // something unrelated to the authority).
    synth_froggers::FroggersParameterModel& sourceModel = source.ApplicationForTest().Parameters();
    constexpr float kTolerance = 0.01f;
    REQUIRE_TRUE(std::fabs(sourceModel.PageParameter(1, 4).UIDisplayCenter(0) - kTargetA) < kTolerance);
    REQUIRE_TRUE(std::fabs(sourceModel.Crispy(4).UIDisplayCenter(0) - kTargetB) < kTolerance);

    juce::MemoryBlock state;
    source.getStateInformation(state);
    REQUIRE_TRUE(state.getSize() > 0);
    source.releaseResources();

    // A FRESH processor, its own scratch root -- a restore must not depend
    // on any process-wide state left behind by `source`.
    frogg3rs_vst::FroggersPluginProcessor fresh(ScratchDataPaths("state_roundtrip_restore"));
    fresh.setRateAndBufferSizeDetails(48000.0, 256);
    fresh.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> freshBuffer(2, 256);
    synth_froggers::FroggersParameterModel& freshModel = fresh.ApplicationForTest().Parameters();

    // Defaults confirmed different from the saved targets FIRST, so the
    // restore assertion below cannot pass by accident (e.g. a no-op
    // setStateInformation() that leaves defaults in place).
    REQUIRE_TRUE(std::fabs(freshModel.PageParameter(1, 4).UIDisplayCenter(0) - kTargetA) > 0.05f);

    fresh.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    PumpAndSettle(fresh, freshBuffer, midi);

    // The authority itself restored -- not this bridge's own shadow/uiState
    // mirror (same "assert at the real Parameter, not the bridge" idiom the
    // existing host-write round-trip test above uses).
    REQUIRE_TRUE(std::fabs(freshModel.PageParameter(1, 4).UIDisplayCenter(0) - kTargetA) < kTolerance);
    REQUIRE_TRUE(std::fabs(freshModel.Crispy(4).UIDisplayCenter(0) - kTargetB) < kTolerance);

    // The host parameters read back the SAME restored values -- the bridge
    // relays the restored authority (an ordinary core-side change, from its
    // own point of view), it is never written directly (see
    // PumpStatePersistence()'s own comment, FroggersPluginProcessor.cpp).
    juce::AudioProcessorParameter* freshTargetA = FindHostParamById(fresh, "bank1.slot4");
    juce::AudioProcessorParameter* freshTargetB = FindHostParamById(fresh, "bank4.crispy");
    REQUIRE_TRUE(freshTargetA != nullptr && freshTargetB != nullptr);
    REQUIRE_TRUE(std::fabs(freshTargetA->getValue() - kTargetA) < kTolerance);
    REQUIRE_TRUE(std::fabs(freshTargetB->getValue() - kTargetB) < kTolerance);

    fresh.releaseResources();

    std::cout << "  [state] round trip: bank1.slot4=" << kTargetA << ", bank4.crispy=" << kTargetB
              << " survived getStateInformation() -> setStateInformation() on a fresh processor, at the "
                 "authority and the host readback.\n";
}

// -- 2. Parameter-model growth -------------------------------------------------
TEST_CASE(state_information_restore_defaults_parameters_missing_from_a_smaller_stored_model) {
    frogg3rs_vst::FroggersPluginProcessor source(ScratchDataPaths("state_smaller_source"));
    source.setRateAndBufferSizeDetails(48000.0, 256);
    source.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> sourceBuffer(2, 256);
    juce::MidiBuffer midi;
    PumpAndSettle(source, sourceBuffer, midi);  // let construction-time defaults fully mirror first.

    synth_froggers::FroggersParameterModel& sourceModel = source.ApplicationForTest().Parameters();
    const std::string keptName = sourceModel.PageParameter(0, 0).Name();
    const float droppedDefault = sourceModel.PageParameter(0, 1).UIDisplayCenter(0);

    juce::AudioProcessorParameter* keptHostParam = FindHostParamById(source, "bank0.slot0");
    juce::AudioProcessorParameter* droppedHostParam = FindHostParamById(source, "bank0.slot1");
    REQUIRE_TRUE(keptHostParam != nullptr && droppedHostParam != nullptr);
    constexpr float kKeptTarget = 0.9f;
    constexpr float kDroppedTarget = 0.1f;
    keptHostParam->setValue(kKeptTarget);
    droppedHostParam->setValue(kDroppedTarget);
    PumpAndSettle(source, sourceBuffer, midi);
    // Confirm the to-be-dropped parameter actually moved, so its later
    // reversion to default is a real, observable change, not a no-op.
    REQUIRE_TRUE(std::fabs(sourceModel.PageParameter(0, 1).UIDisplayCenter(0) - droppedDefault) > 0.05f);

    juce::MemoryBlock fullState;
    source.getStateInformation(fullState);
    source.releaseResources();
    const std::string fullText(static_cast<const char*>(fullState.getData()), fullState.getSize());
    const std::string trimmedText = BuildPatchTextWithOnlyTheseParameterKeys(fullText, {keptName});

    frogg3rs_vst::FroggersPluginProcessor fresh(ScratchDataPaths("state_smaller_restore"));
    fresh.setRateAndBufferSizeDetails(48000.0, 256);
    fresh.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> freshBuffer(2, 256);
    fresh.setStateInformation(trimmedText.data(), static_cast<int>(trimmedText.size()));
    PumpAndSettle(fresh, freshBuffer, midi);

    synth_froggers::FroggersParameterModel& freshModel = fresh.ApplicationForTest().Parameters();
    constexpr float kTolerance = 0.01f;
    // The key present in the trimmed document restores...
    REQUIRE_TRUE(std::fabs(freshModel.PageParameter(0, 0).UIDisplayCenter(0) - kKeptTarget) < kTolerance);
    // ...the key ABSENT from it -- simulating a parameter that did not
    // exist yet when this "session" was saved -- keeps its default rather
    // than erroring or carrying over an unrelated value.
    REQUIRE_TRUE(std::fabs(freshModel.PageParameter(0, 1).UIDisplayCenter(0) - droppedDefault) < kTolerance);

    fresh.releaseResources();

    std::cout << "  [state] stored model smaller than current: kept key restored to " << kKeptTarget
              << ", key missing from the stored document defaulted to " << droppedDefault << ".\n";
}

TEST_CASE(state_information_restore_ignores_parameter_keys_the_current_model_does_not_know) {
    frogg3rs_vst::FroggersPluginProcessor source(ScratchDataPaths("state_larger_source"));
    source.setRateAndBufferSizeDetails(48000.0, 256);
    source.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> sourceBuffer(2, 256);
    juce::MidiBuffer midi;

    juce::AudioProcessorParameter* hostParam = FindHostParamById(source, "bank2.slot9");
    REQUIRE_TRUE(hostParam != nullptr);
    constexpr float kTarget = 0.65f;
    hostParam->setValue(kTarget);
    PumpAndSettle(source, sourceBuffer, midi);

    juce::MemoryBlock fullState;
    source.getStateInformation(fullState);
    source.releaseResources();
    const std::string fullText(static_cast<const char*>(fullState.getData()), fullState.getSize());
    const std::string augmentedText = BuildPatchTextWithAnExtraUnknownParameterKey(fullText);

    frogg3rs_vst::FroggersPluginProcessor fresh(ScratchDataPaths("state_larger_restore"));
    fresh.setRateAndBufferSizeDetails(48000.0, 256);
    fresh.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> freshBuffer(2, 256);
    fresh.setStateInformation(augmentedText.data(), static_cast<int>(augmentedText.size()));
    PumpAndSettle(fresh, freshBuffer, midi);

    // No crash, no rejected load -- and the real key restores correctly
    // despite sitting alongside a key this build has never heard of.
    synth_froggers::FroggersParameterModel& freshModel = fresh.ApplicationForTest().Parameters();
    REQUIRE_TRUE(std::fabs(freshModel.PageParameter(2, 9).UIDisplayCenter(0) - kTarget) < 0.01f);

    fresh.releaseResources();

    std::cout << "  [state] stored model larger than current: unknown extra parameter key tolerated, known key "
                 "still restored to "
              << kTarget << ".\n";
}

// -- 3. No write-through -------------------------------------------------------
TEST_CASE(state_information_save_and_restore_never_write_the_shared_data_root) {
    const synth::RuntimeDataPaths paths = ScratchDataPaths("state_no_write_through");
    frogg3rs_vst::FroggersPluginProcessor processor(paths);
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    PumpAndSettle(processor, buffer, midi);

    const std::string before = SnapshotDirectoryTree(paths.dataRoot);

    juce::AudioProcessorParameter* target = FindHostParamById(processor, "bank3.slot6");
    REQUIRE_TRUE(target != nullptr);
    target->setValue(0.42f);
    PumpAndSettle(processor, buffer, midi);

    // Save and restore several times over, with many PumpStatePersistence()
    // pumps along the way (each issues/consumes its own SerializeToJSON/
    // LoadFromJSON round trip) -- the same repeated exercise a long DAW
    // editing session produces.
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    for (int i = 0; i < 5; ++i) {
        processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        PumpAndSettle(processor, buffer, midi);
        processor.getStateInformation(state);
    }

    const std::string after = SnapshotDirectoryTree(paths.dataRoot);
    REQUIRE_TRUE(before == after);

    // Positive control: prove SnapshotDirectoryTree() could have detected
    // a write, by deliberately writing a file into the SAME directory this
    // test just proved untouched -- so "no difference" above is not simply
    // because this detector cannot see writes at all.
    {
        std::ofstream control(paths.patchesRoot / "control-write.json", std::ios::binary);
        control << "{}";
        REQUIRE_TRUE(static_cast<bool>(control));
    }
    const std::string afterControlWrite = SnapshotDirectoryTree(paths.dataRoot);
    REQUIRE_TRUE(afterControlWrite != after);

    processor.releaseResources();

    std::cout << "  [state] " << paths.dataRoot.string()
              << " byte-for-byte unchanged across 6 save/restore cycles; the same detector DID flag a "
                 "deliberate control write into it.\n";
}

// -- 4. Freeze latch persistence -----------------------------------------------
// The Freeze latch is not a ParameterManager parameter (BuildHostParameterInventory()'s
// own comment, FroggersPluginProcessor.cpp), so it takes no part in the three
// tests above -- it is carried separately, as a "sessionExtras" sibling key,
// restored through the Freeze host parameter and PumpHostParameterBridge()'s
// own existing comparison rather than a second dispatch path (see
// PumpStatePersistence()'s own comment). Proven here as two independent
// properties: restoring an explicit target actually moves the latch, in
// EITHER direction (a single always-true or always-false case cannot rule out
// "coincidentally landed on it anyway"); and restoring a blob with no
// sessionExtras key at all leaves whatever the latch already was alone,
// whichever way that was.
TEST_CASE(state_information_round_trips_the_freeze_latch_when_engaged) {
    frogg3rs_vst::FroggersPluginProcessor source(ScratchDataPaths("state_freeze_engaged_source"));
    source.setRateAndBufferSizeDetails(48000.0, 256);
    source.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> sourceBuffer(2, 256);
    juce::MidiBuffer midi;

    // Engaged via the "freeze" host parameter -- the same host-automation
    // door host_write_round_trips_through_the_core_parameter_authority above
    // uses for an ordinary parameter -- rather than DispatchAction()
    // directly, so this also proves the saved state reflects what a DAW's
    // own generic parameter view would show.
    juce::AudioProcessorParameter* sourceFreeze = FindHostParamById(source, "freeze");
    REQUIRE_TRUE(sourceFreeze != nullptr);
    sourceFreeze->setValue(1.0f);
    PumpAndSettle(source, sourceBuffer, midi);
    REQUIRE_TRUE(source.ApplicationForTest().FreezeLatched());

    juce::MemoryBlock state;
    source.getStateInformation(state);
    REQUIRE_TRUE(state.getSize() > 0);
    source.releaseResources();

    // A FRESH processor, its own scratch root -- same discipline as the
    // parameter round-trip test above.
    frogg3rs_vst::FroggersPluginProcessor fresh(ScratchDataPaths("state_freeze_engaged_restore"));
    fresh.setRateAndBufferSizeDetails(48000.0, 256);
    fresh.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> freshBuffer(2, 256);

    // Confirmed disengaged FIRST -- a fresh processor's own real default --
    // so the restore assertion below cannot pass merely because disengaged
    // happens to already be where it started.
    REQUIRE_TRUE(!fresh.ApplicationForTest().FreezeLatched());

    fresh.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    PumpAndSettle(fresh, freshBuffer, midi);

    REQUIRE_TRUE(fresh.ApplicationForTest().FreezeLatched());
    juce::AudioProcessorParameter* freshFreeze = FindHostParamById(fresh, "freeze");
    REQUIRE_TRUE(freshFreeze != nullptr);
    REQUIRE_TRUE(freshFreeze->getValue() >= 0.5f);

    fresh.releaseResources();

    std::cout << "  [state] freeze latch engaged: survived getStateInformation() -> setStateInformation() on a "
                 "fresh processor, at the authority and the host readback.\n";
}

TEST_CASE(state_information_restore_disengages_the_latch_on_a_target_that_started_engaged) {
    frogg3rs_vst::FroggersPluginProcessor source(ScratchDataPaths("state_freeze_disengaged_source"));
    source.setRateAndBufferSizeDetails(48000.0, 256);
    source.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> sourceBuffer(2, 256);
    juce::MidiBuffer midi;
    PumpAndSettle(source, sourceBuffer, midi);  // settle before saving, same discipline as the smaller-model test.
    REQUIRE_TRUE(!source.ApplicationForTest().FreezeLatched());  // disengaged -- the state this test saves.

    juce::MemoryBlock state;
    source.getStateInformation(state);
    REQUIRE_TRUE(state.getSize() > 0);
    source.releaseResources();

    frogg3rs_vst::FroggersPluginProcessor target(ScratchDataPaths("state_freeze_disengaged_restore"));
    target.setRateAndBufferSizeDetails(48000.0, 256);
    target.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> targetBuffer(2, 256);

    // Bring the TARGET to the OPPOSITE state before restoring: this is what
    // rules out "restored correctly" being indistinguishable from
    // "coincidentally already there" for the disengaged case, the same way
    // starting the target at its own true default already ruled it out for
    // the engaged case above -- a fresh processor's own default is already
    // disengaged, so proving THIS direction needs a target that starts
    // engaged instead.
    juce::AudioProcessorParameter* targetFreeze = FindHostParamById(target, "freeze");
    REQUIRE_TRUE(targetFreeze != nullptr);
    targetFreeze->setValue(1.0f);
    PumpAndSettle(target, targetBuffer, midi);
    REQUIRE_TRUE(target.ApplicationForTest().FreezeLatched());

    target.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    PumpAndSettle(target, targetBuffer, midi);

    REQUIRE_TRUE(!target.ApplicationForTest().FreezeLatched());
    REQUIRE_TRUE(targetFreeze->getValue() < 0.5f);

    target.releaseResources();

    std::cout << "  [state] freeze latch disengaged: restoring into a processor whose latch was engaged actually "
                 "moved it to disengaged, at the authority and the host readback.\n";
}

TEST_CASE(state_information_legacy_blob_without_session_extras_leaves_a_disengaged_latch_disengaged) {
    constexpr float kTarget = 0.55f;
    const std::string legacyText =
        BuildLegacyBlobWithOneParameterMoved("state_freeze_legacy_source_a", "bank0.slot2", kTarget);
    REQUIRE_TRUE(legacyText.find("sessionExtras") == std::string::npos);

    frogg3rs_vst::FroggersPluginProcessor target(ScratchDataPaths("state_freeze_legacy_restore_a"));
    target.setRateAndBufferSizeDetails(48000.0, 256);
    target.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> targetBuffer(2, 256);
    juce::MidiBuffer midi;

    REQUIRE_TRUE(!target.ApplicationForTest().FreezeLatched());  // fresh processor's own default: disengaged.
    target.setStateInformation(legacyText.data(), static_cast<int>(legacyText.size()));
    PumpAndSettle(target, targetBuffer, midi);

    // Still disengaged -- a blob with no sessionExtras key does not force it
    // either way.
    REQUIRE_TRUE(!target.ApplicationForTest().FreezeLatched());
    // The parameter portion of a legacy blob still restores normally.
    REQUIRE_TRUE(std::fabs(target.ApplicationForTest().Parameters().PageParameter(0, 2).UIDisplayCenter(0) - kTarget)
                 < 0.01f);
    target.releaseResources();

    std::cout << "  [state] legacy blob (no sessionExtras), latch started disengaged: parameter restored to "
              << kTarget << ", latch left disengaged.\n";
}

TEST_CASE(state_information_legacy_blob_without_session_extras_leaves_an_engaged_latch_engaged) {
    constexpr float kTarget = 0.35f;
    const std::string legacyText =
        BuildLegacyBlobWithOneParameterMoved("state_freeze_legacy_source_b", "bank0.slot3", kTarget);
    REQUIRE_TRUE(legacyText.find("sessionExtras") == std::string::npos);

    frogg3rs_vst::FroggersPluginProcessor target(ScratchDataPaths("state_freeze_legacy_restore_b"));
    target.setRateAndBufferSizeDetails(48000.0, 256);
    target.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> targetBuffer(2, 256);
    juce::MidiBuffer midi;

    // Engage the latch on the TARGET first -- the case a naive "absent key
    // means disengage" bug would get wrong while the test above (already
    // disengaged) could not catch, since disengaged-stays-disengaged looks
    // the same whether the restore truly does nothing or silently forces
    // false.
    juce::AudioProcessorParameter* targetFreeze = FindHostParamById(target, "freeze");
    REQUIRE_TRUE(targetFreeze != nullptr);
    targetFreeze->setValue(1.0f);
    PumpAndSettle(target, targetBuffer, midi);
    REQUIRE_TRUE(target.ApplicationForTest().FreezeLatched());

    target.setStateInformation(legacyText.data(), static_cast<int>(legacyText.size()));
    PumpAndSettle(target, targetBuffer, midi);

    REQUIRE_TRUE(target.ApplicationForTest().FreezeLatched());  // still engaged -- undisturbed, not forced off.
    REQUIRE_TRUE(std::fabs(target.ApplicationForTest().Parameters().PageParameter(0, 3).UIDisplayCenter(0) - kTarget)
                 < 0.01f);
    target.releaseResources();

    std::cout << "  [state] legacy blob (no sessionExtras), latch started engaged: parameter restored to " << kTarget
              << ", latch left engaged (not forced off).\n";
}

// Parses `patchJsonText` and returns an equivalent document whose
// sessionExtras object has its "visibleBankIndex" member replaced with
// `bankIndexValue`, keeping "freezeLatched" untouched -- standing in for a
// host project file naming a bank this build's restore path must
// bounds-check rather than trust. Same "fresh top-level object, each field
// set exactly once" construction as the other Build...() helpers above, for
// the same SetNew-always-appends reason.
std::string BuildPatchTextWithVisibleBankIndexOverridden(const std::string& patchJsonText,
                                                          std::int64_t bankIndexValue) {
    synth::JsonArena arena(256 * 1024);
    synth::JSON original = arena.Loads(patchJsonText.c_str());
    REQUIRE_TRUE(!original.IsNull());
    synth::JSON originalExtras = original.Get("sessionExtras");
    REQUIRE_TRUE(!originalExtras.IsNull());
    synth::JSON freezeLatched = originalExtras.Get("freezeLatched");
    REQUIRE_TRUE(!freezeLatched.IsNull());

    synth::JSON newExtras = arena.Object();
    newExtras.SetNew("freezeLatched", freezeLatched);
    newExtras.SetNew("visibleBankIndex", arena.Integer(bankIndexValue));

    synth::JSON result = arena.Object();
    result.SetNew("schema", original.Get("schema"));
    result.SetNew("schemaVersion", original.Get("schemaVersion"));
    result.SetNew("patchName", original.Get("patchName"));
    result.SetNew("parameterValues", original.Get("parameterValues"));
    result.SetNew("sessionExtras", newExtras);

    char* dumped = result.Dumps(JSON_ENCODE_ANY);
    REQUIRE_TRUE(dumped != nullptr);
    std::string text(dumped);
    std::free(dumped);
    return text;
}

// Parses `patchJsonText` and returns an equivalent document whose
// sessionExtras object keeps ONLY "freezeLatched", dropping
// "visibleBankIndex" entirely -- standing in for a session saved by a build
// that has the Freeze-latch sibling key but predates the bank-index one
// (sessionExtras exists, but this specific key does not), as opposed to
// BuildPatchTextWithoutSessionExtras above, which drops the whole object.
std::string BuildPatchTextWithSessionExtrasKeepingOnlyFreezeLatched(const std::string& patchJsonText) {
    synth::JsonArena arena(256 * 1024);
    synth::JSON original = arena.Loads(patchJsonText.c_str());
    REQUIRE_TRUE(!original.IsNull());
    synth::JSON originalExtras = original.Get("sessionExtras");
    REQUIRE_TRUE(!originalExtras.IsNull());
    synth::JSON freezeLatched = originalExtras.Get("freezeLatched");
    REQUIRE_TRUE(!freezeLatched.IsNull());

    synth::JSON trimmedExtras = arena.Object();
    trimmedExtras.SetNew("freezeLatched", freezeLatched);

    synth::JSON result = arena.Object();
    result.SetNew("schema", original.Get("schema"));
    result.SetNew("schemaVersion", original.Get("schemaVersion"));
    result.SetNew("patchName", original.Get("patchName"));
    result.SetNew("parameterValues", original.Get("parameterValues"));
    result.SetNew("sessionExtras", trimmedExtras);

    char* dumped = result.Dumps(JSON_ENCODE_ANY);
    REQUIRE_TRUE(dumped != nullptr);
    std::string text(dumped);
    std::free(dumped);
    return text;
}

// -- Visible bank persistence --------------------------------------------
// Same sessionExtras sibling-key round trip as the Freeze latch tests
// above, for the "visibleBankIndex" key (FroggersPluginProcessor.cpp's own
// PumpStatePersistence() comment).

// POSITIVE CONTROL folded into this test itself (not a separate case, same
// as the Freeze latch round trip above already assumes its OWN positive
// control from operator_selecting_a_bank_does_move_the_visible_page):
// the restored bank is asserted to DIFFER from the default (0) before it is
// asserted to equal the operator's actual target, so a restore that quietly
// no-ops (leaving the fresh processor at its own default) cannot pass this
// test by coincidence.
TEST_CASE(state_information_round_trips_the_visible_bank_when_non_default) {
    frogg3rs_vst::FroggersPluginProcessor source(ScratchDataPaths("state_bank_source"));
    source.setRateAndBufferSizeDetails(48000.0, 256);
    source.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> sourceBuffer(2, 256);
    juce::MidiBuffer midi;

    constexpr std::size_t kOperatorBank = 4;
    REQUIRE_TRUE(source.ApplicationForTest().ActiveBankIndex() == 0);
    // The exact same public seam FroggersUiSurface.hpp's own bank buttons
    // call (app/FroggersUiSurface.hpp's `HandleAction`) -- the OPERATOR
    // selecting a page, not a direct MessageIn::SelectParamBank push.
    source.ApplicationForTest().RequestBankSelect(kOperatorBank);
    PumpAndSettle(source, sourceBuffer, midi);
    REQUIRE_TRUE(source.ApplicationForTest().ActiveBankIndex() == kOperatorBank);

    juce::MemoryBlock state;
    source.getStateInformation(state);
    REQUIRE_TRUE(state.getSize() > 0);
    source.releaseResources();

    // A FRESH processor, its own scratch root -- same discipline as the
    // parameter/freeze-latch round-trip tests above.
    frogg3rs_vst::FroggersPluginProcessor fresh(ScratchDataPaths("state_bank_restore"));
    fresh.setRateAndBufferSizeDetails(48000.0, 256);
    fresh.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> freshBuffer(2, 256);

    // Confirmed at the default FIRST -- a fresh processor's own real
    // default -- so the restore assertion below cannot pass merely because
    // the target bank happens to already be where it started.
    REQUIRE_TRUE(fresh.ApplicationForTest().ActiveBankIndex() == 0);

    fresh.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    PumpAndSettle(fresh, freshBuffer, midi);

    const std::size_t restoredBankIx = fresh.ApplicationForTest().ActiveBankIndex();
    REQUIRE_TRUE(restoredBankIx != 0);  // positive control -- see this test's own header comment.
    REQUIRE_TRUE(restoredBankIx == kOperatorBank);

    fresh.releaseResources();

    std::cout << "  [state] visible bank round trip: operator selected bank " << kOperatorBank
              << " -> survived getStateInformation() -> setStateInformation() on a fresh processor, "
                 "ActiveBankIndex()="
              << restoredBankIx << ".\n";
}

TEST_CASE(state_information_restore_clamps_an_out_of_range_saved_bank_to_the_default_page) {
    frogg3rs_vst::FroggersPluginProcessor source(ScratchDataPaths("state_bank_oob_source"));
    source.setRateAndBufferSizeDetails(48000.0, 256);
    source.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> sourceBuffer(2, 256);
    juce::MidiBuffer midi;
    PumpAndSettle(source, sourceBuffer, midi);  // settle before saving, same discipline as the smaller-model test.

    juce::MemoryBlock fullState;
    source.getStateInformation(fullState);
    REQUIRE_TRUE(fullState.getSize() > 0);
    source.releaseResources();
    const std::string fullText(static_cast<const char*>(fullState.getData()), fullState.getSize());

    // Far past kFroggersBankCount (6) -- a host project file naming a bank
    // this build (or any plausible future one) does not have.
    constexpr std::int64_t kOutOfRangeBank = 999;
    const std::string tamperedText = BuildPatchTextWithVisibleBankIndexOverridden(fullText, kOutOfRangeBank);

    frogg3rs_vst::FroggersPluginProcessor target(ScratchDataPaths("state_bank_oob_restore"));
    target.setRateAndBufferSizeDetails(48000.0, 256);
    target.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> targetBuffer(2, 256);

    target.setStateInformation(tamperedText.data(), static_cast<int>(tamperedText.size()));
    PumpAndSettle(target, targetBuffer, midi);  // must not crash.

    REQUIRE_TRUE(target.ApplicationForTest().ActiveBankIndex() == 0);  // falls back to the default page.

    target.releaseResources();

    std::cout << "  [state] out-of-range saved bank index " << kOutOfRangeBank
              << " did not crash and landed on the default page (ActiveBankIndex()=0).\n";
}

TEST_CASE(state_information_session_extras_without_bank_key_restores_freeze_latch_and_leaves_the_page_at_default) {
    frogg3rs_vst::FroggersPluginProcessor source(ScratchDataPaths("state_bank_missing_key_source"));
    source.setRateAndBufferSizeDetails(48000.0, 256);
    source.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> sourceBuffer(2, 256);
    juce::MidiBuffer midi;

    // Engage the freeze latch AND move to a non-default bank on the
    // SOURCE, so the trimmed blob below carries a real engaged-freeze value
    // while still lacking a bank key entirely -- proving the missing key,
    // not merely an absent/false freeze value, is what this test exercises.
    juce::AudioProcessorParameter* sourceFreeze = FindHostParamById(source, "freeze");
    REQUIRE_TRUE(sourceFreeze != nullptr);
    sourceFreeze->setValue(1.0f);
    constexpr std::size_t kSourceBank = 3;
    source.ApplicationForTest().RequestBankSelect(kSourceBank);
    PumpAndSettle(source, sourceBuffer, midi);
    REQUIRE_TRUE(source.ApplicationForTest().FreezeLatched());
    REQUIRE_TRUE(source.ApplicationForTest().ActiveBankIndex() == kSourceBank);

    juce::MemoryBlock fullState;
    source.getStateInformation(fullState);
    REQUIRE_TRUE(fullState.getSize() > 0);
    source.releaseResources();
    const std::string fullText(static_cast<const char*>(fullState.getData()), fullState.getSize());
    const std::string trimmedText = BuildPatchTextWithSessionExtrasKeepingOnlyFreezeLatched(fullText);
    REQUIRE_TRUE(trimmedText.find("sessionExtras") != std::string::npos);
    REQUIRE_TRUE(trimmedText.find("visibleBankIndex") == std::string::npos);

    frogg3rs_vst::FroggersPluginProcessor target(ScratchDataPaths("state_bank_missing_key_restore"));
    target.setRateAndBufferSizeDetails(48000.0, 256);
    target.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> targetBuffer(2, 256);

    REQUIRE_TRUE(!target.ApplicationForTest().FreezeLatched());
    REQUIRE_TRUE(target.ApplicationForTest().ActiveBankIndex() == 0);

    target.setStateInformation(trimmedText.data(), static_cast<int>(trimmedText.size()));
    PumpAndSettle(target, targetBuffer, midi);

    // The Freeze latch restores exactly as it did before this key existed.
    REQUIRE_TRUE(target.ApplicationForTest().FreezeLatched());
    // A blob with sessionExtras but no bank key is a no-op for the page --
    // left at its default, never forced there by some OTHER path either.
    REQUIRE_TRUE(target.ApplicationForTest().ActiveBankIndex() == 0);

    target.releaseResources();

    std::cout << "  [state] sessionExtras present, bank key absent: freeze latch restored to engaged, page left at "
                 "its default (ActiveBankIndex()=0).\n";
}

// -- 7. Optional audio input bus and its input-channel selection ------------
// Same -60dBFS silence floor and ~1s settle budget every other audio-
// producing test in this file uses (see the Freeze section's own citation
// of FroggersVstSmokeTest.cpp's kSilenceFloorLinear). Each TEST_CASE below
// gets its own fresh processor/scratch root (ScratchDataPaths(), this
// file's own convention) rather than sharing one across cases.
TEST_CASE(input_bus_absent_by_default_and_the_plugin_still_produces_audio) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("input_bus_absent"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);

    // Bus posture straight out of construction (isBusesLayoutSupported()'s
    // own comment, FroggersPluginProcessor.cpp): the optional input bus is
    // declared but disabled until a host opts in.
    const juce::AudioProcessor::Bus* inputBus = processor.getBus(true, 0);
    REQUIRE_TRUE(inputBus != nullptr);
    REQUIRE_TRUE(!inputBus->isEnabled());

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    auto runBlocksMeasuringPeak = [&](int blocks) {
        float peak = 0.0f;
        for (int i = 0; i < blocks; ++i) {
            buffer.clear();
            processor.processBlock(buffer, midi);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                const float* samples = buffer.getReadPointer(ch);
                for (int s = 0; s < buffer.getNumSamples(); ++s) {
                    peak = std::max(peak, std::fabs(samples[s]));
                }
            }
        }
        return peak;
    };

    processor.TestStartTransport();
    const float settledPeak = runBlocksMeasuringPeak(188);
    constexpr float kSilenceFloorLinear = 1.0e-3f;
    REQUIRE_TRUE(settledPeak >= kSilenceFloorLinear);  // "optional" -- disabled input never silences the instrument.

    // Left exactly as it started: producing audio never implicitly enables
    // the bus.
    REQUIRE_TRUE(!inputBus->isEnabled());

    processor.releaseResources();
    std::cout << "  [input bus] absent by default, settledPeak=" << settledPeak << " (>= silence floor "
              << kSilenceFloorLinear << ").\n";
}

TEST_CASE(input_bus_present_when_a_host_enables_it_and_the_plugin_still_produces_audio) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("input_bus_present"));
    juce::AudioProcessor::Bus* inputBus = processor.getBus(true, 0);
    REQUIRE_TRUE(inputBus != nullptr);
    REQUIRE_TRUE(inputBus->enable(true));  // host connects the optional bus before negotiating the session.
    REQUIRE_TRUE(inputBus->isEnabled());
    REQUIRE_TRUE(inputBus->getNumberOfChannels() == 2);  // stereo -- the bus's own declared default layout.

    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    auto runBlocksMeasuringPeak = [&](int blocks) {
        float peak = 0.0f;
        for (int i = 0; i < blocks; ++i) {
            buffer.clear();
            processor.processBlock(buffer, midi);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                const float* samples = buffer.getReadPointer(ch);
                for (int s = 0; s < buffer.getNumSamples(); ++s) {
                    peak = std::max(peak, std::fabs(samples[s]));
                }
            }
        }
        return peak;
    };

    processor.TestStartTransport();
    const float settledPeak = runBlocksMeasuringPeak(188);
    constexpr float kSilenceFloorLinear = 1.0e-3f;
    REQUIRE_TRUE(settledPeak >= kSilenceFloorLinear);  // the instrument still makes sound with the bus connected.

    processor.releaseResources();
    std::cout << "  [input bus] present (host-enabled), settledPeak=" << settledPeak << " (>= silence floor "
              << kSilenceFloorLinear << ").\n";
}

// Dispatches the exact same kInputSelect action the rendered control
// dispatches on a tap (FroggersUiSurface.hpp's own HandleAction branch),
// through the SAME production seam TestStartTransport()/TestStopTransport()
// use (EditorSurface().DispatchAction()) -- never a hand-mirrored write to
// inputSelection_, which this file has no access to anyway (private).
void DispatchInputSelect(frogg3rs_vst::FroggersPluginProcessor& processor) {
    processor.EditorSurface().DispatchAction(
        synth::ui::Action::Named(synth_froggers::FroggersActions::kInputSelect));
}

TEST_CASE(input_bus_selecting_a_channel_connects_the_external_sources) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("input_bus_select_channel"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;

    // Positive control: the sources must NOT already read connected before
    // this test does anything -- otherwise the assertion below would pass
    // even if the whole derivation were dead code.
    REQUIRE_TRUE(!processor.ApplicationForTest().Modulation().ExternalAudioConnected());
    REQUIRE_TRUE(processor.InputSelectionForTest() == 0);  // "None" -- the default.

    // The host connects the bus (processorLayoutsChanged() fires
    // synchronously off enable(), re-deriving the option list to
    // ["None", <channel>] -- see that method's own comment) -- bus
    // enablement ALONE is not consent, so connected must still read false
    // here.
    juce::AudioProcessor::Bus* inputBus = processor.getBus(true, 0);
    REQUIRE_TRUE(inputBus != nullptr);
    REQUIRE_TRUE(inputBus->enable(true));
    REQUIRE_TRUE(!processor.ApplicationForTest().Modulation().ExternalAudioConnected());

    // The OPERATOR's own affirmative act: a tap on the rendered
    // input-select control, cycling None -> the bus's first channel.
    DispatchInputSelect(processor);
    REQUIRE_TRUE(processor.InputSelectionForTest() == 1);

    // One block: ApplyInputSelection() already published the change
    // (invoked synchronously by the dispatched action's callback, above);
    // this block is what FroggersAppCore::ProcessFrame() (audio thread,
    // once per block) uses to apply the queued transition -- see that
    // method's own comment.
    buffer.clear();
    processor.processBlock(buffer, midi);

    REQUIRE_TRUE(processor.ApplicationForTest().Modulation().ExternalAudioConnected());

    processor.releaseResources();
    std::cout << "  [input bus] selecting the bus's first channel: external sources report connected (was NOT "
                 "connected beforehand).\n";
}

TEST_CASE(input_bus_returning_to_none_makes_the_external_sources_inert_again) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("input_bus_return_to_none"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;

    // Enabled at mono() specifically (rather than the bus's own stereo
    // default) so the option list is exactly ["None", <channel>] and a
    // single further tap cycles straight back to "None" -- the property
    // this test is actually about; the fuller stereo option cycle (None,
    // both channels, Sum) is covered separately by the stereo-bus tests
    // below.
    juce::AudioProcessor::Bus* inputBus = processor.getBus(true, 0);
    REQUIRE_TRUE(inputBus != nullptr);
    REQUIRE_TRUE(inputBus->setCurrentLayout(juce::AudioChannelSet::mono()));
    DispatchInputSelect(processor);  // None -> the bus's first channel.
    REQUIRE_TRUE(processor.InputSelectionForTest() == 1);
    buffer.clear();
    processor.processBlock(buffer, midi);

    // Positive control: must actually BE connected before returning to
    // None means anything -- otherwise the final assertion below would
    // pass even if the None branch were never wired at all.
    REQUIRE_TRUE(processor.ApplicationForTest().Modulation().ExternalAudioConnected());

    // A second tap: the only other option (with one channel on the bus) is
    // "None", so this cycles straight back to it.
    DispatchInputSelect(processor);
    REQUIRE_TRUE(processor.InputSelectionForTest() == 0);
    buffer.clear();
    processor.processBlock(buffer, midi);

    REQUIRE_TRUE(!processor.ApplicationForTest().Modulation().ExternalAudioConnected());

    processor.releaseResources();
    std::cout << "  [input bus] returning to None: external sources are inert again (WERE connected "
                 "beforehand).\n";
}

// The defect this whole design exists to prevent
// ("Connected is consent, never channel presence"): a host may enable the
// optional input bus on its own, with nothing routed in and no operator
// selection -- e.g. a DAW that activates every optional bus on
// instantiation. Deriving "connected" from bus-enabled-with-channels
// (rather than from the selection) would report connected here anyway and
// let the external-audio sources start taking randomization from silence.
TEST_CASE(input_bus_host_enabled_with_no_selection_leaves_external_sources_inert) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("input_bus_enabled_no_selection"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;

    // The host enables the bus -- and delivers real, nonzero samples into
    // it, so a passing assertion below cannot be credited to "there was
    // never anything to read" instead of to the connected gate actually
    // holding.
    juce::AudioProcessor::Bus* inputBus = processor.getBus(true, 0);
    REQUIRE_TRUE(inputBus != nullptr);
    REQUIRE_TRUE(inputBus->enable(true));
    REQUIRE_TRUE(processor.InputSelectionForTest() == 0);  // the operator selected nothing.

    buffer.clear();
    juce::AudioBuffer<float> hostInputView = processor.getBusBuffer(buffer, true, 0);
    REQUIRE_TRUE(hostInputView.getNumChannels() == 2);  // stereo -- the bus's own declared default layout.
    hostInputView.setSample(0, 0, 0.25f);
    processor.processBlock(buffer, midi);

    REQUIRE_TRUE(!processor.ApplicationForTest().Modulation().ExternalAudioConnected());
    REQUIRE_TRUE(processor.InputSelectionForTest() == 0);  // still None -- enablement alone never moves it.

    processor.releaseResources();
    std::cout << "  [input bus] host-enabled bus, no operator selection: external sources stayed inert.\n";
}

TEST_CASE(input_bus_layout_change_removing_the_selected_channel_falls_back_to_none) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("input_bus_layout_change_removes_channel"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;

    juce::AudioProcessor::Bus* inputBus = processor.getBus(true, 0);
    REQUIRE_TRUE(inputBus != nullptr);
    REQUIRE_TRUE(inputBus->enable(true));
    DispatchInputSelect(processor);  // None -> the bus's first channel.
    REQUIRE_TRUE(processor.InputSelectionForTest() == 1);
    buffer.clear();
    processor.processBlock(buffer, midi);
    REQUIRE_TRUE(processor.ApplicationForTest().Modulation().ExternalAudioConnected());  // positive control.

    // The host changes the layout mid-session, disabling the bus outright
    // -- the channel index 1 named is now gone. processorLayoutsChanged()
    // (fired synchronously by enable(): "Re-validate against the current
    // bus on every layout change") re-derives the
    // option list (now just "None") and re-validates the selection against
    // it.
    REQUIRE_TRUE(inputBus->enable(false));
    REQUIRE_TRUE(processor.InputSelectionForTest() == 0);  // fell back, never left pointing at a gone channel.

    buffer.clear();
    processor.processBlock(buffer, midi);
    REQUIRE_TRUE(!processor.ApplicationForTest().Modulation().ExternalAudioConnected());

    processor.releaseResources();
    std::cout << "  [input bus] layout change removed the selected channel: selection fell back to None.\n";
}

TEST_CASE(state_information_round_trips_the_input_selection_when_a_channel_is_chosen) {
    frogg3rs_vst::FroggersPluginProcessor source(ScratchDataPaths("state_input_selection_source"));
    source.setRateAndBufferSizeDetails(48000.0, 256);
    source.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> sourceBuffer(2, 256);
    juce::MidiBuffer midi;

    juce::AudioProcessor::Bus* sourceInputBus = source.getBus(true, 0);
    REQUIRE_TRUE(sourceInputBus != nullptr);
    REQUIRE_TRUE(sourceInputBus->enable(true));
    DispatchInputSelect(source);  // None -> the bus's first channel.
    REQUIRE_TRUE(source.InputSelectionForTest() == 1);
    PumpAndSettle(source, sourceBuffer, midi);
    REQUIRE_TRUE(source.ApplicationForTest().Modulation().ExternalAudioConnected());

    juce::MemoryBlock state;
    source.getStateInformation(state);
    REQUIRE_TRUE(state.getSize() > 0);
    source.releaseResources();

    // A FRESH processor, its own scratch root -- same discipline as the
    // parameter/freeze-latch/bank-index round-trip tests above. Its bus is
    // enabled to the SAME shape the source's was: a restore only ever
    // re-validates against the CURRENT bus (ApplyInputSelection()'s own
    // comment), so a target whose bus does not yet match cannot exercise a
    // successful restore -- that is state_information_restore_clamps_an_
    // out_of_range_saved_bank_to_the_default_page's own shape of case
    // (a mismatched target), covered for the analogous bank key already.
    frogg3rs_vst::FroggersPluginProcessor fresh(ScratchDataPaths("state_input_selection_restore"));
    juce::AudioProcessor::Bus* freshInputBus = fresh.getBus(true, 0);
    REQUIRE_TRUE(freshInputBus != nullptr);
    REQUIRE_TRUE(freshInputBus->enable(true));
    fresh.setRateAndBufferSizeDetails(48000.0, 256);
    fresh.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> freshBuffer(2, 256);

    // Confirmed at "None" FIRST -- a fresh processor's own real default --
    // so the restore assertion below cannot pass merely because the
    // selected channel happens to already be where it started.
    REQUIRE_TRUE(fresh.InputSelectionForTest() == 0);
    REQUIRE_TRUE(!fresh.ApplicationForTest().Modulation().ExternalAudioConnected());

    fresh.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    PumpAndSettle(fresh, freshBuffer, midi);

    REQUIRE_TRUE(fresh.InputSelectionForTest() == 1);
    REQUIRE_TRUE(fresh.ApplicationForTest().Modulation().ExternalAudioConnected());

    fresh.releaseResources();

    std::cout << "  [state] input selection round trip: operator selected the bus's first channel -> survived "
                 "getStateInformation() -> setStateInformation() on a fresh processor, InputSelectionForTest()="
              << fresh.InputSelectionForTest() << ".\n";
}

TEST_CASE(state_information_legacy_blob_without_input_selection_key_restores_to_none) {
    frogg3rs_vst::FroggersPluginProcessor source(ScratchDataPaths("state_input_selection_missing_key_source"));
    source.setRateAndBufferSizeDetails(48000.0, 256);
    source.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> sourceBuffer(2, 256);
    juce::MidiBuffer midi;
    PumpAndSettle(source, sourceBuffer, midi);  // settle before saving, same discipline as the freeze/bank tests.

    juce::MemoryBlock fullState;
    source.getStateInformation(fullState);
    REQUIRE_TRUE(fullState.getSize() > 0);
    source.releaseResources();
    const std::string fullText(static_cast<const char*>(fullState.getData()), fullState.getSize());
    // Reuses the SAME trimming helper the bank-index "missing key" test
    // above uses: it keeps only "freezeLatched" in sessionExtras, so the
    // trimmed blob has no "inputSelection" key either -- exactly "a blob
    // saved before this change existed."
    const std::string trimmedText = BuildPatchTextWithSessionExtrasKeepingOnlyFreezeLatched(fullText);
    REQUIRE_TRUE(trimmedText.find("sessionExtras") != std::string::npos);
    REQUIRE_TRUE(trimmedText.find("inputSelection") == std::string::npos);

    // The target's bus is enabled (unlike a fresh processor's own default)
    // so a MISTAKEN read of some other key as a selection, or a bug that
    // defaulted to "select whatever exists," would have a real channel to
    // land on and be caught here -- the missing key must produce "None" on
    // its own, not merely because there was nothing else it could have
    // been.
    frogg3rs_vst::FroggersPluginProcessor target(ScratchDataPaths("state_input_selection_missing_key_restore"));
    juce::AudioProcessor::Bus* targetInputBus = target.getBus(true, 0);
    REQUIRE_TRUE(targetInputBus != nullptr);
    REQUIRE_TRUE(targetInputBus->enable(true));
    target.setRateAndBufferSizeDetails(48000.0, 256);
    target.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> targetBuffer(2, 256);

    target.setStateInformation(trimmedText.data(), static_cast<int>(trimmedText.size()));
    PumpAndSettle(target, targetBuffer, midi);

    REQUIRE_TRUE(target.InputSelectionForTest() == 0);
    REQUIRE_TRUE(!target.ApplicationForTest().Modulation().ExternalAudioConnected());

    target.releaseResources();

    std::cout << "  [state] sessionExtras present, inputSelection key absent: restored to None on a bus that DID "
                 "have a channel available.\n";
}

// A signal on the bus's own one channel, with that channel selected, must
// actually reach the external-audio source -- not just flip `connected`
// (already covered above). Constant across the whole block so the block's
// LAST sample (what Step() leaves the source holding once the block ends)
// is a known, stateable quantity: NormalizeBipolarToUnit(0.6f) == 0.8f
// exactly (both 0.6f and 0.8f are exactly representable in float).
TEST_CASE(input_bus_selected_channel_signal_reaches_the_external_audio_source) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("input_bus_selected_channel_signal"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);

    juce::AudioProcessor::Bus* inputBus = processor.getBus(true, 0);
    REQUIRE_TRUE(inputBus != nullptr);
    REQUIRE_TRUE(inputBus->enable(true));
    DispatchInputSelect(processor);  // None -> the bus's first channel.
    REQUIRE_TRUE(processor.InputSelectionForTest() == 1);

    // Step() only runs while the transport is running (FroggersAppCore::
    // ProcessBlock()'s own transportRunningNow gate) -- without this, the
    // source would never move off its inert default and this test would
    // prove nothing.
    processor.TestStartTransport();

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    buffer.clear();
    juce::AudioBuffer<float> hostInputView = processor.getBusBuffer(buffer, true, 0);
    REQUIRE_TRUE(hostInputView.getNumChannels() == 2);  // stereo -- the bus's own declared default layout.
    constexpr float kDrivenSample = 0.6f;
    for (int s = 0; s < hostInputView.getNumSamples(); ++s) {
        hostInputView.setSample(0, s, kDrivenSample);
    }
    processor.processBlock(buffer, midi);

    REQUIRE_TRUE(processor.ApplicationForTest().Modulation().ExternalAudioConnected());  // positive control.
    const float sourceValue = processor.ApplicationForTest().Modulation().ExternalAudioSourceForTest();
    REQUIRE_TRUE(std::fabs(sourceValue - 0.8f) < 1.0e-4f);

    processor.releaseResources();
    std::cout << "  [input bus] selected channel: driven sample=" << kDrivenSample
              << " -> external-audio source=" << sourceValue << " (expected 0.8).\n";
}

// ResolveSelectedInputChannel()'s own generality -- Sum and "one specific
// channel of several" -- exercised directly against synthetic 2-channel
// arrays, isolating the resolution algorithm itself from bus/processBlock
// plumbing (the section below drives the same paths through a real stereo
// bus and a real processBlock() call instead).
TEST_CASE(resolve_selected_input_channel_sums_two_channels_when_sum_is_selected) {
    constexpr int kNumSamples = 8;
    std::array<float, kNumSamples> channel0{};
    std::array<float, kNumSamples> channel1{};
    for (int s = 0; s < kNumSamples; ++s) {
        channel0[static_cast<std::size_t>(s)] = 0.1f;
        channel1[static_cast<std::size_t>(s)] = 0.2f;
    }
    const std::array<const float*, 2> channels{channel0.data(), channel1.data()};
    std::array<float, kNumSamples> out{};

    // Sum is index numChannels+1 == 3 (ComputeInputOptionLabels()'s own
    // scheme: 0 None, 1..2 one per channel, 3 Sum).
    const bool resolved = frogg3rs_vst::FroggersPluginProcessor::ResolveSelectedInputChannel(
        channels.data(), 2, /*selection=*/3, kNumSamples, out.data());
    REQUIRE_TRUE(resolved);
    for (int s = 0; s < kNumSamples; ++s) {
        REQUIRE_TRUE(std::fabs(out[static_cast<std::size_t>(s)] - 0.3f) < 1.0e-6f);  // 0.1 + 0.2.
    }
    std::cout << "  [resolve] Sum over a synthetic 2-channel array: 0.1 + 0.2 = " << out[0] << ".\n";
}

TEST_CASE(resolve_selected_input_channel_selecting_the_other_channel_reads_the_other_one) {
    constexpr int kNumSamples = 4;
    std::array<float, kNumSamples> channel0{0.11f, 0.12f, 0.13f, 0.14f};
    std::array<float, kNumSamples> channel1{0.21f, 0.22f, 0.23f, 0.24f};
    const std::array<const float*, 2> channels{channel0.data(), channel1.data()};
    std::array<float, kNumSamples> out{};

    // Selection 1 -> channel0, selection 2 -> channel1 (ComputeInputOptionLabels()'s
    // own 1-based scheme).
    REQUIRE_TRUE(frogg3rs_vst::FroggersPluginProcessor::ResolveSelectedInputChannel(
        channels.data(), 2, /*selection=*/1, kNumSamples, out.data()));
    for (int s = 0; s < kNumSamples; ++s) {
        REQUIRE_TRUE(out[static_cast<std::size_t>(s)] == channel0[static_cast<std::size_t>(s)]);
    }

    REQUIRE_TRUE(frogg3rs_vst::FroggersPluginProcessor::ResolveSelectedInputChannel(
        channels.data(), 2, /*selection=*/2, kNumSamples, out.data()));
    for (int s = 0; s < kNumSamples; ++s) {
        REQUIRE_TRUE(out[static_cast<std::size_t>(s)] == channel1[static_cast<std::size_t>(s)]);
    }
    std::cout << "  [resolve] selection 1 read channel0 verbatim, selection 2 read channel1 verbatim.\n";
}

// The bus was widened from mono-only to mono-or-stereo (isBusesLayoutSupported()'s
// own comment, FroggersPluginProcessor.cpp) precisely so the selection
// machinery above stops being unreachable code past index 1. The four
// TEST_CASEs below exercise that width directly: (a)/(b) the bus's own
// layout negotiation, (c) the option list a real stereo bus produces, (d)
// the second-channel and Sum reads driven through a real processBlock().
TEST_CASE(input_bus_can_be_enabled_as_stereo_and_the_plugin_accepts_it) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("input_bus_stereo_accept"));
    juce::AudioProcessor::Bus* inputBus = processor.getBus(true, 0);
    REQUIRE_TRUE(inputBus != nullptr);

    REQUIRE_TRUE(inputBus->setCurrentLayout(juce::AudioChannelSet::stereo()));
    REQUIRE_TRUE(inputBus->isEnabled());
    REQUIRE_TRUE(inputBus->getNumberOfChannels() == 2);

    // A layout isBusesLayoutSupported() must still reject: wider than
    // stereo, even with the output left at its own required stereo --
    // proves the widening stopped at stereo rather than opening the bus to
    // anything.
    juce::AudioProcessor::BusesLayout quadInputRejected;
    quadInputRejected.inputBuses.add(juce::AudioChannelSet::quadraphonic());
    quadInputRejected.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE_TRUE(!processor.checkBusesLayoutSupported(quadInputRejected));

    std::cout << "  [input bus] setCurrentLayout(stereo()) accepted, getNumberOfChannels()="
              << inputBus->getNumberOfChannels() << "; quadraphonic input still rejected.\n";
}

TEST_CASE(input_bus_mono_and_disabled_are_still_accepted) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("input_bus_mono_and_disabled_accept"));
    juce::AudioProcessor::Bus* inputBus = processor.getBus(true, 0);
    REQUIRE_TRUE(inputBus != nullptr);

    REQUIRE_TRUE(inputBus->setCurrentLayout(juce::AudioChannelSet::mono()));
    REQUIRE_TRUE(inputBus->isEnabled());
    REQUIRE_TRUE(inputBus->getNumberOfChannels() == 1);

    REQUIRE_TRUE(inputBus->setCurrentLayout(juce::AudioChannelSet::disabled()));
    REQUIRE_TRUE(!inputBus->isEnabled());

    std::cout << "  [input bus] mono() and disabled() both still accepted after the stereo widening.\n";
}

TEST_CASE(input_bus_stereo_options_are_none_both_channels_and_sum) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("input_bus_stereo_options"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);

    juce::AudioProcessor::Bus* inputBus = processor.getBus(true, 0);
    REQUIRE_TRUE(inputBus != nullptr);
    REQUIRE_TRUE(inputBus->setCurrentLayout(juce::AudioChannelSet::stereo()));
    REQUIRE_TRUE(inputBus->getNumberOfChannels() == 2);

    // Asserting the actual contents, not just the count -- a bug that
    // dropped or reordered an entry (e.g. Sum before the second channel)
    // would pass a length-only check.
    const std::vector<std::string> labels = processor.InputOptionLabelsForTest();
    REQUIRE_TRUE(labels.size() == 4);
    REQUIRE_TRUE(labels[0] == "None");
    REQUIRE_TRUE(labels[1] == "L");
    REQUIRE_TRUE(labels[2] == "R");
    REQUIRE_TRUE(labels[3] == "Sum");

    processor.releaseResources();
    std::cout << "  [input bus] stereo option labels: [" << labels[0] << ", " << labels[1] << ", " << labels[2]
              << ", " << labels[3] << "].\n";
}

TEST_CASE(input_bus_stereo_second_channel_and_sum_reach_the_external_audio_source) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("input_bus_stereo_second_and_sum"));
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);

    juce::AudioProcessor::Bus* inputBus = processor.getBus(true, 0);
    REQUIRE_TRUE(inputBus != nullptr);
    REQUIRE_TRUE(inputBus->setCurrentLayout(juce::AudioChannelSet::stereo()));
    REQUIRE_TRUE(inputBus->getNumberOfChannels() == 2);

    processor.TestStartTransport();  // Step() only runs while the transport is running.

    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;

    // Driven values deliberately UNEQUAL -- the positive control below (the
    // second channel's read must differ from the first's) cannot pass by
    // coincidence.
    constexpr float kChannel0Sample = 0.6f;   // NormalizeBipolarToUnit(0.6f) == 0.8f.
    constexpr float kChannel1Sample = -0.2f;  // NormalizeBipolarToUnit(-0.2f) == 0.4f.
    REQUIRE_TRUE(kChannel0Sample != kChannel1Sample);

    // None -> L (index 1): read directly ahead of the real target below, as
    // a positive control that a passing read of index 2 is actually reading
    // the SECOND channel and not just re-reading the first.
    DispatchInputSelect(processor);
    REQUIRE_TRUE(processor.InputSelectionForTest() == 1);
    buffer.clear();
    juce::AudioBuffer<float> firstChannelView = processor.getBusBuffer(buffer, true, 0);
    for (int s = 0; s < firstChannelView.getNumSamples(); ++s) {
        firstChannelView.setSample(0, s, kChannel0Sample);
        firstChannelView.setSample(1, s, kChannel1Sample);
    }
    processor.processBlock(buffer, midi);
    const float firstChannelValue = processor.ApplicationForTest().Modulation().ExternalAudioSourceForTest();
    REQUIRE_TRUE(std::fabs(firstChannelValue - 0.8f) < 1.0e-4f);

    // L -> R (index 2): must read the SECOND channel's signal, not the
    // first's.
    DispatchInputSelect(processor);
    REQUIRE_TRUE(processor.InputSelectionForTest() == 2);
    buffer.clear();
    juce::AudioBuffer<float> secondChannelView = processor.getBusBuffer(buffer, true, 0);
    for (int s = 0; s < secondChannelView.getNumSamples(); ++s) {
        secondChannelView.setSample(0, s, kChannel0Sample);
        secondChannelView.setSample(1, s, kChannel1Sample);
    }
    processor.processBlock(buffer, midi);
    const float secondChannelValue = processor.ApplicationForTest().Modulation().ExternalAudioSourceForTest();
    REQUIRE_TRUE(std::fabs(secondChannelValue - 0.4f) < 1.0e-4f);
    // Positive control: cannot pass by coincidentally reading channel 0.
    REQUIRE_TRUE(std::fabs(secondChannelValue - firstChannelValue) > 1.0e-3f);

    // R -> Sum (index 3): must read the sum of BOTH channels.
    DispatchInputSelect(processor);
    REQUIRE_TRUE(processor.InputSelectionForTest() == 3);
    buffer.clear();
    juce::AudioBuffer<float> sumView = processor.getBusBuffer(buffer, true, 0);
    for (int s = 0; s < sumView.getNumSamples(); ++s) {
        sumView.setSample(0, s, kChannel0Sample);
        sumView.setSample(1, s, kChannel1Sample);
    }
    processor.processBlock(buffer, midi);
    const float sumValue = processor.ApplicationForTest().Modulation().ExternalAudioSourceForTest();
    // Raw sum 0.6 + (-0.2) == 0.4, normalized: 0.5 + 0.5 * 0.4 == 0.7.
    REQUIRE_TRUE(std::fabs(sumValue - 0.7f) < 1.0e-4f);

    processor.releaseResources();
    std::cout << "  [input bus] stereo: channel1=" << firstChannelValue << " (expected 0.8), channel2="
              << secondChannelValue << " (expected 0.4), sum=" << sumValue << " (expected 0.7).\n";
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
