// FroggersMidiOutTests.cpp -- what the app sends on its one MIDI out.
//
// Runs via synth_rig::SynthRig<FroggersApp> (External/Sheaf's
// tests/support/SynthRig.hpp), the same JUCE-free headless harness
// FroggersAudioRoutingTests.cpp uses; wired into app/Makefile's `test`
// target (nice make -j2 test). SynthRig's own constructor calls
// Engine::Initialize() with no hook to call EnableAppMidiOutRouting()
// first (that call must precede Initialize()), so these tests configure
// the MIDI-out setting through FroggersAppCore::SetMidiOutSetting()
// directly -- the same app entry point the plugin calls (see that
// method's own comment) -- rather than through Engine::SetAppMidiOutConfig,
// and read appended events through Engine::AppMidiOutEvents(), which
// Engine::ProcessBlock populates every block regardless of whether engine
// routing is enabled.

#include "Froggers.hpp"
#include "FroggersParameters.hpp"
#include "support/SynthRig.hpp"

#ifdef JUCE_MAJOR_VERSION
#error "Froggers MIDI-out tests must not see JUCE headers -- the app core must stay JUCE-free"
#endif

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
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

#define TEST_CASE(name)                     \
    void name();                            \
    Register reg_##name(#name, &name);      \
    void name()

#define REQUIRE_TRUE(expr)                                                       \
    do {                                                                         \
        if (!(expr)) {                                                          \
            std::ostringstream oss;                                             \
            oss << __FILE__ << ":" << __LINE__ << " requirement failed: " #expr; \
            throw std::runtime_error(oss.str());                                \
        }                                                                        \
    } while (false)

synth::RuntimeDataPaths UseScratchRuntimeDataPaths(const char* testName) {
    const std::filesystem::path dataRoot =
        std::filesystem::temp_directory_path() / "froggers-midi-out-tests" / testName;
    std::filesystem::remove_all(dataRoot);
    synth::RuntimeDataPaths paths = synth::RuntimeDataPaths::FromDataRoot(dataRoot);
    std::filesystem::create_directories(paths.patchesRoot);
    return paths;
}

using Rig = synth_rig::SynthRig<synth_froggers::FroggersApp>;

synth::AppMidiOutSettings LevelSetting(std::uint8_t channel, std::uint8_t ccNumber) {
    synth::AppMidiOutSettings settings;
    settings.contentId = synth_froggers::kFroggersMidiOutContentLevelId;
    settings.channel = channel;
    settings.ccNumber = ccNumber;
    return settings;
}

// ---------------------------------------------------------------------------
// off_sends_nothing
// ---------------------------------------------------------------------------
// Placed beside Level's own tests: with the setting at its default (Off),
// nothing is ever appended, including across the send path Level adds
// below -- a build whose default content were Level, instead of Off, would
// append Control Changes throughout this run.
TEST_CASE(off_sends_nothing) {
    Rig::AudioSettings settings48k128;
    settings48k128.sampleRate = 48000.0;
    settings48k128.blockSize = 128;
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("off_sends_nothing"), settings48k128);

    REQUIRE_TRUE(rig.Application().MidiOutContent() == synth_froggers::FroggersMidiOutContent::Off);

    rig.StartAt(0);
    // Ten seconds at 48 kHz / 128-frame blocks.
    constexpr std::size_t kBlocksPerSecond = 48000 / 128;
    for (std::size_t i = 0; i < kBlocksPerSecond * 10; ++i) {
        rig.RunBlocks(1);
        REQUIRE_TRUE(rig.Engine().AppMidiOutEvents().Size() == 0);
    }
}

// ---------------------------------------------------------------------------
// level_appends_control_changes_on_the_set_channel_and_cc_only_when_changed
// ---------------------------------------------------------------------------
// Covers "A change in level sends one Control Change" and the "Only the
// chosen content is sent" scenario's Level half: every appended message,
// across the whole run, is a Control Change on the set channel and CC
// number, at the block's last frame, and only when its value differs from
// the last one sent.
TEST_CASE(level_appends_control_changes_on_the_set_channel_and_cc_only_when_changed) {
    Rig::AudioSettings settings48k128;
    settings48k128.sampleRate = 48000.0;
    settings48k128.blockSize = 128;
    Rig rig(/*patchPumpBudgetBlocks=*/64,
           UseScratchRuntimeDataPaths("level_appends_cc_on_change"), settings48k128);

    rig.Application().SetMidiOutSetting(LevelSetting(/*channel=*/2, /*ccNumber=*/20));
    rig.StartAt(0);

    std::optional<int> lastSeenValue;
    std::size_t ccCount = 0;
    // The attack ramp from silence up to the default patch's steady output
    // guarantees at least one real change.
    for (std::size_t i = 0; i < 200; ++i) {
        rig.RunBlocks(1);
        const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
        REQUIRE_TRUE(events.Size() <= 1);  // at most one CC per block.
        if (events.Size() == 1) {
            ++ccCount;
            REQUIRE_TRUE(events[0].frame == 127);  // the block's last frame.
            REQUIRE_TRUE(events[0].statusByte == 0xB2);  // Control Change, channel 2.
            REQUIRE_TRUE(events[0].data1 == 20);  // the set CC number.
            REQUIRE_TRUE(events[0].data2 <= 127);
            REQUIRE_TRUE(!lastSeenValue.has_value() || *lastSeenValue != events[0].data2);
            lastSeenValue = events[0].data2;
        }
    }
    REQUIRE_TRUE(ccCount > 0);  // the attack ramp actually produced a change.
}

// ---------------------------------------------------------------------------
// steady_level_sends_nothing_more
// ---------------------------------------------------------------------------
TEST_CASE(steady_level_sends_nothing_more) {
    Rig::AudioSettings settings48k128;
    settings48k128.sampleRate = 48000.0;
    settings48k128.blockSize = 128;
    Rig rig(/*patchPumpBudgetBlocks=*/64,
           UseScratchRuntimeDataPaths("steady_level_sends_nothing_more"), settings48k128);

    // Transport never started: output, and therefore the mono fold and the
    // follower's level, is exactly and steadily 0 every sample.
    rig.Application().SetMidiOutSetting(LevelSetting(/*channel=*/0, /*ccNumber=*/16));

    rig.RunBlocks(1);
    const synth::AppMidiOutEventList& firstEvents = rig.Engine().AppMidiOutEvents();
    REQUIRE_TRUE(firstEvents.Size() == 1);  // the first-ever message always sends.
    REQUIRE_TRUE(firstEvents[0].data2 == 0);

    // Rounded level stays 0 in every following block (steady): nothing more
    // is appended after the first.
    for (std::size_t i = 0; i < 100; ++i) {
        rig.RunBlocks(1);
        REQUIRE_TRUE(rig.Engine().AppMidiOutEvents().Size() == 0);
    }
}

// ---------------------------------------------------------------------------
// moving_level_is_sent_at_most_50_times_a_second
// ---------------------------------------------------------------------------
// Drives Gain (Drive bank, slot 1) between two extremes every single block
// via direct SceneCenter writes (ComputeAllParameters converges each write
// exactly, in one call -- see FroggersAudioRoutingTests.cpp's ApplyPatchNow
// comment), which forces the rounded output level to differ at every
// block's last frame -- the scenario's own precondition, driven
// deterministically rather than left to incidental DSP dynamics.
TEST_CASE(moving_level_is_sent_at_most_50_times_a_second) {
    Rig::AudioSettings settings48k128;
    settings48k128.sampleRate = 48000.0;
    settings48k128.blockSize = 128;
    Rig rig(/*patchPumpBudgetBlocks=*/64,
           UseScratchRuntimeDataPaths("moving_level_50_a_second"), settings48k128);

    rig.Application().SetMidiOutSetting(LevelSetting(/*channel=*/0, /*ccNumber=*/16));
    rig.StartAt(0);

    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    synth::Parameter& gain = model.PageParameter(synth_froggers::FroggersBankId::Drive, 1);

    constexpr std::size_t kBlocksPerSecond = 48000 / 128;  // 375 at 128 frames.
    std::size_t ccCount = 0;
    std::optional<std::uint64_t> lastSentAbsoluteSample;
    std::uint64_t absoluteSample = 0;
    for (std::size_t i = 0; i < kBlocksPerSecond; ++i) {
        gain.SceneCenter(0) = (i % 2 == 0) ? 0.02f : 1.0f;
        rig.Application().TestParameterManager().ComputeAllParameters();
        rig.RunBlocks(1);
        absoluteSample += 128;
        const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
        REQUIRE_TRUE(events.Size() <= 1);
        if (events.Size() == 1) {
            const std::uint64_t sentAtSample = absoluteSample - 128 + events[0].frame;
            if (lastSentAbsoluteSample.has_value()) {
                REQUIRE_TRUE(sentAtSample - *lastSentAbsoluteSample >= 960);
            }
            lastSentAbsoluteSample = sentAtSample;
            ++ccCount;
        }
    }
    REQUIRE_TRUE(ccCount <= 50);
}

// ---------------------------------------------------------------------------
// silence_after_sound_brings_the_value_to_zero
// ---------------------------------------------------------------------------
TEST_CASE(silence_after_sound_brings_the_value_to_zero) {
    Rig::AudioSettings settings48k128;
    settings48k128.sampleRate = 48000.0;
    settings48k128.blockSize = 128;
    Rig rig(/*patchPumpBudgetBlocks=*/64,
           UseScratchRuntimeDataPaths("silence_after_sound_zero"), settings48k128);

    rig.Application().SetMidiOutSetting(LevelSetting(/*channel=*/0, /*ccNumber=*/16));
    rig.StartAt(0);

    constexpr std::size_t kBlocksPerSecond = 48000 / 128;
    std::optional<std::uint8_t> lastValueSent;
    for (std::size_t i = 0; i < kBlocksPerSecond * 2; ++i) {
        rig.RunBlocks(1);
        const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
        if (events.Size() == 1) {
            lastValueSent = events[0].data2;
        }
    }
    rig.StopAt(0);
    for (std::size_t i = 0; i < kBlocksPerSecond * 5; ++i) {
        rig.RunBlocks(1);
        const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
        if (events.Size() == 1) {
            lastValueSent = events[0].data2;
        }
    }
    REQUIRE_TRUE(lastValueSent.has_value());
    REQUIRE_TRUE(*lastValueSent == 0);
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
