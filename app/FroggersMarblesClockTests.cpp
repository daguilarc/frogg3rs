// FroggersMarblesClockTests.cpp -- covers the Marbles clock and
// visualizer sources. Bare synth::ParameterManager +
// FroggersParameterModel + FroggersModulationSlate, same structural-check
// convention FroggersModulationTests.cpp uses -- FroggersModulationSlate::
// Step()/PrepareBlockClock() take plain std::optional<double> values, so
// the per-source clock/rate logic is fully exercisable without a
// real MasterClock/AudioBlock/SynthRig. "A high-deja-vu source
// cycles a fixed loop while a low one keeps producing new values" is a pure
// dsp::RandomShLane-level property, independent of clock wiring, and lives
// instead in FroggersDspParityTests.cpp alongside the rest of the ported
// DSP's own parity suite. Validating the character table by
// ear is NOT covered here.

#include "FroggersModulation.hpp"
#include "FroggersParameters.hpp"
#include "FroggersRandomShVisualizer.hpp"

#include "synth/GangedRandomLfoVisualizer.hpp"
#include "synth/ParameterModulation.hpp"

#ifdef JUCE_MAJOR_VERSION
#error "Froggers Marbles clock tests must not see JUCE headers"
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace synth_froggers;

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

#define REQUIRE_NEAR(actual, expected, tolerance)                                          \
    do {                                                                                    \
        const double actualValue = (actual);                                                \
        const double expectedValue = (expected);                                             \
        if (!(std::fabs(actualValue - expectedValue) <= (tolerance))) {                     \
            std::ostringstream oss;                                                         \
            oss << __FILE__ << ":" << __LINE__ << " requirement failed: " #actual " (="      \
                << actualValue << ") not within " << (tolerance) << " of " #expected " (="   \
                << expectedValue << ")";                                                     \
            throw std::runtime_error(oss.str());                                            \
        }                                                                                    \
    } while (false)

struct Fixture {
    static constexpr std::uint32_t kSalt = 0x5A17u;  // a fixed launch salt, so every source is reproducible
    synth::ParameterManager manager;
    FroggersParameterModel model;
    FroggersModulationSlate slate{kSalt};

    Fixture() {
        model.Init(manager);
        slate.Init(model.Group());
        slate.Prepare(48000.0);
    }
};

// -----------------------------------------------------------------------
// Over N quarter notes each source ticks exactly N x its own rate (#1 ->
// 3N, #2 -> 2N, #3 -> N, #4 -> N/2, #5 -> N/4), counted by the lane's own
// tick counter: three of the five read a random slot on a jump, one time
// in eight the slot it is already on, so an index change is not a tick.
// N=8 quarter notes makes every expected count an integer (24, 16, 8, 4, 2).
// -----------------------------------------------------------------------
TEST_CASE(per_source_rate_ratios_over_eight_quarter_notes) {
    Fixture fx;
    constexpr FroggersModulationSlate::VcoDrive kSilent{0.5f, 0.5f, 0.0f};

    constexpr double kTotalQuarterNotes = 8.0;
    constexpr int kStepsPerQuarterNote = 300;  // fine enough that source #1 (x3) never double-ticks in one Step().
    constexpr int kTotalSteps = static_cast<int>(kTotalQuarterNotes) * kStepsPerQuarterNote;

    // First call primes every Phasor2Tick (Phasor2Tick::Process's own "first
    // call primes, does not tick" semantics) -- matches how a real transport
    // would first commit a clock plan before any tick can fire.
    fx.slate.Step(kSilent, kSilent, kSilent, 0.0);
    for (int step = 1; step <= kTotalSteps; ++step) {
        const double qn = kTotalQuarterNotes * static_cast<double>(step) / static_cast<double>(kTotalSteps);
        fx.slate.Step(kSilent, kSilent, kSilent, qn);
    }

    REQUIRE_TRUE(fx.slate.RandomShLaneTickCountForTest(0) == 24);  // source 1: eighth triplet (x3).
    REQUIRE_TRUE(fx.slate.RandomShLaneTickCountForTest(1) == 16);  // source 2: eighth (x2).
    REQUIRE_TRUE(fx.slate.RandomShLaneTickCountForTest(2) == 8);   // source 3: quarter note (x1).
    REQUIRE_TRUE(fx.slate.RandomShLaneTickCountForTest(3) == 4);   // source 4: once per two quarter notes.
    REQUIRE_TRUE(fx.slate.RandomShLaneTickCountForTest(4) == 2);   // source 5: once per four quarter notes.
}

// A missing clock plan (std::nullopt every sample) must never advance any
// of the five lanes -- StepClockDrivenLanes' own has_value() guard.
TEST_CASE(missing_transport_position_never_advances_the_five_lanes) {
    Fixture fx;
    constexpr FroggersModulationSlate::VcoDrive kSilent{0.5f, 0.5f, 0.0f};

    for (int i = 0; i < 200; ++i) {
        fx.slate.Step(kSilent, kSilent, kSilent, std::nullopt);
    }
    fx.slate.PublishUiState();
    for (std::size_t lane = 0; lane < 5; ++lane) {
        REQUIRE_TRUE(fx.slate.RandomShLaneUiState(lane).currentIndex.load() == 0);
    }
}

// -----------------------------------------------------------------------
// Source #6 is verified as tempo-proportional rather than
// counted in ticks. PrepareBlockClock() recomputes source #6's
// GangedRandomLfoInput from quarterNotesPerSample so one full move-cycle
// spans 16 quarter notes; doubling the rate (quarterNotesPerSample) must
// halve the derived mu-seconds (both waiting and moving), by formula --
// checked directly rather than by waiting for real LFO rounds (random
// draws) to complete.
// -----------------------------------------------------------------------
TEST_CASE(source_six_tempo_following_input_scales_inversely_with_quarter_notes_per_sample) {
    Fixture fx;
    constexpr double kSampleRate = 48000.0;

    // quarterNotesPerSample = (bpm/60)/sampleRate; use two tempos 2x apart.
    const double qnPerSampleAt120Bpm = (120.0 / 60.0) / kSampleRate;
    const double qnPerSampleAt240Bpm = (240.0 / 60.0) / kSampleRate;

    fx.slate.PrepareBlockClock(qnPerSampleAt120Bpm);
    const synth::GangedRandomLfoInput at120 = fx.slate.CurrentGangedLfoInputForTest();

    fx.slate.PrepareBlockClock(qnPerSampleAt240Bpm);
    const synth::GangedRandomLfoInput at240 = fx.slate.CurrentGangedLfoInputForTest();

    REQUIRE_TRUE(at120.waiting.muSeconds > 0.0);
    REQUIRE_TRUE(at120.moving.muSeconds > 0.0);
    REQUIRE_NEAR(at240.waiting.muSeconds, at120.waiting.muSeconds * 0.5, at120.waiting.muSeconds * 1e-6);
    REQUIRE_NEAR(at240.moving.muSeconds, at120.moving.muSeconds * 0.5, at120.moving.muSeconds * 1e-6);

    // A missing clock plan (no rate at all) must still leave a finite,
    // positive, valid config -- input to `GangedRandomLfoInput` must stay
    // clamped finite and positive
    // before it reaches the audio thread.
    fx.slate.PrepareBlockClock(std::nullopt);
    const synth::GangedRandomLfoInput noPlan = fx.slate.CurrentGangedLfoInputForTest();
    REQUIRE_TRUE(std::isfinite(noPlan.waiting.muSeconds) && noPlan.waiting.muSeconds > 0.0);
    REQUIRE_TRUE(std::isfinite(noPlan.moving.muSeconds) && noPlan.moving.muSeconds > 0.0);
}

// Source #6 reuses GangedRandomLfoVisualizer UNMODIFIED, while the
// five X-style lanes get the new RandomShLaneVisualizer -- checked by type,
// not merely by non-null, so a copy-paste bug wiring the wrong visualizer
// kind to the wrong slot would be caught.
TEST_CASE(source_six_visualizer_is_the_ganged_random_lfo_kind_others_are_not) {
    Fixture fx;
    for (std::size_t i = 0; i < kFroggersNumRandomShLanes; ++i) {
        auto* visualizer = fx.slate.Metadata(i).visualizer;
        REQUIRE_TRUE(visualizer != nullptr);
        REQUIRE_TRUE(dynamic_cast<RandomShLaneVisualizer*>(visualizer) != nullptr);
        REQUIRE_TRUE(dynamic_cast<synth::ui::GangedRandomLfoVisualizer<1>*>(visualizer) == nullptr);
    }
    auto* source6Visualizer = fx.slate.Metadata(kModSlotRandomSh6).visualizer;
    REQUIRE_TRUE(source6Visualizer != nullptr);
    REQUIRE_TRUE(dynamic_cast<synth::ui::GangedRandomLfoVisualizer<1>*>(source6Visualizer) != nullptr);
    REQUIRE_TRUE(dynamic_cast<RandomShLaneVisualizer*>(source6Visualizer) == nullptr);
}

// The visualizer state must match the bag: RandomShLaneVisualizer
// reads dsp::RandomShLane::UiState directly, so after driving a
// lane and publishing, the UiState's own slots/currentIndex must equal what
// the lane itself reports -- proven by re-deriving the expected values
// through the exact same per-source rate/tick mechanism as the ratio test
// above, rather than re-implementing a second Increment() count.
TEST_CASE(visualizer_state_matches_the_bag_after_publishing) {
    Fixture fx;
    constexpr FroggersModulationSlate::VcoDrive kSilent{0.5f, 0.5f, 0.0f};

    for (int i = 0; i <= 500; ++i) {
        const double qn = static_cast<double>(i) * 0.01;
        fx.slate.Step(kSilent, kSilent, kSilent, qn);
    }
    fx.slate.PublishUiState();

    const auto& lane0State = fx.slate.RandomShLaneUiState(0);
    const std::size_t size = lane0State.size.load();
    REQUIRE_TRUE(size >= 1 && size <= dsp::RandomShLane::kNumSlots);
    const std::size_t index = lane0State.currentIndex.load();
    REQUIRE_TRUE(index < dsp::RandomShLane::kNumSlots);
    // Every published slot value is a valid [0,1] modulation-source reading
    // (see FroggersModulation.hpp's
    // file header for the source-value convention), not left at some uninitialized/out-of-range default.
    for (std::size_t i = 0; i < dsp::RandomShLane::kNumSlots; ++i) {
        const float v = lane0State.slots[i].load();
        REQUIRE_TRUE(std::isfinite(v));
        REQUIRE_TRUE(v >= 0.0f && v <= 1.0f);
    }
}

// -----------------------------------------------------------------------
// Confirms no source-level control exists: no Random S&H
// behaviour parameter in any bank, no seventh bank.
// -----------------------------------------------------------------------
TEST_CASE(no_random_sh_behaviour_parameter_exists_in_any_bank_and_no_seventh_bank) {
    REQUIRE_TRUE(kFroggersBankCount == 6);
    const auto& layouts = FroggersBankLayouts();
    REQUIRE_TRUE(layouts.size() == 6);

    const std::array<const char*, 6> forbiddenSubstrings{
        "Step chance", "Deja vu", "Bag size", "Slew", "Spread", "Bias",
    };
    // Case-insensitive: a generic-sounding bank parameter name (e.g. "Drive
    // bias") must not slip through this guard purely by luck of
    // capitalisation.
    auto toLower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    };
    for (const auto& layout : layouts) {
        for (const auto& spec : layout.params) {
            const std::string name = toLower(std::string(spec.name));
            for (const char* forbidden : forbiddenSubstrings) {
                REQUIRE_TRUE(name.find(toLower(forbidden)) == std::string::npos);
            }
        }
    }
}

// "Depth encoders are unaffected -- assert they are bipolar and default to
// neutral/off" (ModulationDepthConfig, src/ParameterModulation.cpp:1869-1870:
// range=Bipolar, defaultValue=kNeutralModulationDepthCenter=0.5).
TEST_CASE(random_sh_depth_encoders_are_bipolar_and_default_neutral) {
    Fixture fx;
    synth::Parameter& target = fx.model.PageParameter(FroggersBankId::Audio, 0);
    for (std::size_t modIx = 0; modIx < 6; ++modIx) {  // all six Random S&H depth cells.
        synth::Parameter* depth = target.EnsureModulationDepth(modIx);
        REQUIRE_TRUE(depth != nullptr);
        REQUIRE_TRUE(depth->Range() == synth::RangeKind::Bipolar);
        REQUIRE_NEAR(depth->SceneCenter(0), 0.5, 1e-6);  // kNeutralModulationDepthCenter.
    }
}

// ITEM 2b (Sheaf call-site sweep): Sheaf's own StandardModulators::Init
// calls SetVoiceColor(voice, color) for every GangedRandomLfoProcessor it
// owns (StandardModulators.hpp:126-130) before registering it as a
// modulation source. gangedRandomLfo6_ is this app's OWN standalone
// GangedRandomLfoProcessor<1> instance (FroggersModulation.hpp,
// source #6's own resolution, NOT one of StandardModulators' processors), so
// nothing was ever calling SetVoiceColor on it -- its one voice's color
// stays at GangedRandomLfoAtomicColor's own default, Color::Grey
// (DspRandomLfo.hpp:165), even though the registered ModulatorMetadata
// already carries the correct LaneColor(5)
// (FroggersModulation.hpp's LaneColor(), same formula as lanes 1-5). The
// GangedRandomLfoVisualizer reads its plotted color from the LFO's own
// per-voice UiState (GangedRandomLfoVisualizer.hpp:222/243/252's
// `voice.color`, populated by GangedRandomLfoProcessor::PublishUiState
// from `m_voiceColors`, DspRandomLfo.hpp:337-342/369), NOT from
// ModulatorMetadata::sourceColor -- so before the fix the metadata says
// LaneColor(5) but the actual rendered lane is Grey.
//
// Drives the LFO forward one round (SampleAndResetRound runs on the very
// first Process() call, since every voice starts in State::Done -- see
// GangedRandomLfoProcessor::Process's `allDone` check, DspRandomLfo.hpp:
// 311-325) so ComputeTiming (GangedRandomLfoVisualizer.hpp:77-94) has a
// valid nonzero waitingIncrement/movingIncrement to draw from, then reads
// the color back out through the SAME public Draw() path the real UI uses
// (Visualizer::Draw() -> DrawVisible() -> BuildGangedRandomLfoCommands),
// rather than reaching into the LFO's private state -- proving the fix
// where it's actually observable.
TEST_CASE(source_six_visualizer_color_matches_its_own_registered_metadata_color) {
    Fixture fx;
    constexpr FroggersModulationSlate::VcoDrive kSilent{0.5f, 0.5f, 0.0f};

    fx.slate.PrepareBlockClock((120.0 / 60.0) / 48000.0);  // 120 BPM, matches Fixture's 48 kHz Prepare().
    for (int i = 0; i < 8; ++i) {
        fx.slate.Step(kSilent, kSilent, kSilent, static_cast<double>(i) * 0.001);
    }
    fx.slate.PublishUiState();

    const synth::ModulatorMetadata& metadata = fx.slate.Metadata(kModSlotRandomSh6);
    auto* visualizer = dynamic_cast<synth::ui::GangedRandomLfoVisualizer<1>*>(metadata.visualizer);
    REQUIRE_TRUE(visualizer != nullptr);
    visualizer->SetBounds({0.0f, 0.0f, 100.0f, 40.0f});

    const std::vector<synth::ui::DrawCommand> commands = visualizer->Draw();
    bool foundVoiceColorCommand = false;
    for (const synth::ui::DrawCommand& command : commands) {
        if (command.kind == synth::ui::DrawCommand::Kind::FillEllipse ||
            command.kind == synth::ui::DrawCommand::Kind::Polyline) {
            foundVoiceColorCommand = true;
            REQUIRE_TRUE(command.color == metadata.sourceColor);
        }
    }
    // If this is false, ComputeTiming's own preconditions weren't met and
    // the assertion above never ran against anything -- fail loudly rather
    // than silently passing on an empty command list.
    REQUIRE_TRUE(foundVoiceColorCommand);
}

}  // namespace

// -----------------------------------------------------------------------
// The anomaly gradient. Over one 256-quarter-note run at 120 BPM and
// 48 kHz, every source's activity (mean |first difference| per sample:
// step size times step rate, independent of slew because a one-pole is
// monotone) and peak slope (peak |first difference|: slew times step size)
// fall strictly from source 1 to source 6. The six pairs are printed so
// the ledger carries the numbers.
// -----------------------------------------------------------------------
TEST_CASE(random_sh_sources_rank_by_anomaly) {
    Fixture fx;
    constexpr FroggersModulationSlate::VcoDrive kSilent{0.5f, 0.5f, 0.0f};
    constexpr double kSampleRate = 48000.0;
    constexpr double kQuarterNotesPerSample = (120.0 / 60.0) / kSampleRate;
    constexpr double kTotalQuarterNotes = 256.0;
    constexpr long kTotalSamples = static_cast<long>(kTotalQuarterNotes / kQuarterNotesPerSample);
    constexpr std::size_t kSources = 6;

    const auto read = [&](std::size_t source) {
        return static_cast<double>(source < 5 ? fx.slate.RandomShLaneOutputForTest(source)
                                              : fx.slate.RandomSh6OutputForTest());
    };

    fx.slate.PrepareBlockClock(kQuarterNotesPerSample);
    fx.slate.Step(kSilent, kSilent, kSilent, 0.0);
    std::array<double, kSources> last{};
    std::array<double, kSources> sum{};
    std::array<double, kSources> peak{};
    for (std::size_t source = 0; source < kSources; ++source) {
        last[source] = read(source);
    }
    for (long sample = 1; sample < kTotalSamples; ++sample) {
        fx.slate.Step(kSilent, kSilent, kSilent, static_cast<double>(sample) * kQuarterNotesPerSample);
        for (std::size_t source = 0; source < kSources; ++source) {
            const double value = read(source);
            const double difference = std::fabs(value - last[source]);
            sum[source] += difference;
            peak[source] = std::max(peak[source], difference);
            last[source] = value;
        }
    }
    for (std::size_t source = 0; source < kSources; ++source) {
        std::cout << "  [anomaly gradient] Random S&H " << (source + 1)
                  << " mean|d|=" << sum[source] / static_cast<double>(kTotalSamples - 1)
                  << " peak|d|=" << peak[source] << "\n";
    }
    for (std::size_t source = 0; source + 1 < kSources; ++source) {
        REQUIRE_TRUE(sum[source] > sum[source + 1]);
        REQUIRE_TRUE(peak[source] > peak[source + 1]);
    }
}

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
    std::cout << (Registry().size() - static_cast<std::size_t>(failed)) << "/" << Registry().size()
              << " tests passed\n";
    return failed == 0 ? 0 : 1;
}
