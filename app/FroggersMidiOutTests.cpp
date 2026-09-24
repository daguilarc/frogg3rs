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

#include <algorithm>
#include <array>
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

synth::AppMidiOutSettings PitchSetting(std::uint8_t channel,
                                       std::optional<std::uint8_t> fixedVelocity = std::nullopt) {
    synth::AppMidiOutSettings settings;
    settings.contentId = synth_froggers::kFroggersMidiOutContentPitchId;
    settings.channel = channel;
    settings.velocity = fixedVelocity;
    return settings;
}

// The knob-space delta that multiplies a VCO's frequency by `factor`,
// independent of its current value: dsp::Vco's pitch knob maps
// exponentially across [kPitchMinHz, kPitchMaxHz]
// (frequency = min * (max/min)^knob), so knob_new = knob_old +
// log(factor)/log(max/min) -- the exact inverse of
// FroggersAudioRoutingTests.cpp's own SetSelfSustainingRingPatch knob
// formula, generalized from an absolute target to a relative multiply. This
// is measure-q/QLatencyRange.cpp's own step: the three VCO pitch knobs
// raised by a factor of 1.5.
float PitchKnobRaiseDelta(float factor) {
    return std::log(factor) / std::log(synth_froggers::dsp::Vco::kPitchMaxHz /
                                        synth_froggers::dsp::Vco::kPitchMinHz);
}

// The knob value that tunes a VCO to an absolute frequency, the same
// exponential mapping's direct inverse (knob = log(f/min)/log(max/min)).
float PitchKnobForFrequency(float frequencyHz) {
    return std::log(frequencyHz / synth_froggers::dsp::Vco::kPitchMinHz) /
           std::log(synth_froggers::dsp::Vco::kPitchMaxHz / synth_froggers::dsp::Vco::kPitchMinHz);
}


// A note-on or note-off read from AppMidiOutEvents(), decoded once so every
// test below reads the same shape rather than re-decoding status bytes.
struct DecodedNoteEvent {
    bool isNoteOn = false;
    std::uint8_t channel = 0;
    std::uint8_t note = 0;
    std::uint8_t velocity = 0;
};

std::optional<DecodedNoteEvent> DecodeNoteEvent(const synth::AppMidiOutEvent& event) {
    const std::uint8_t kind = event.statusByte & 0xF0;
    if (kind != 0x90 && kind != 0x80) {
        return std::nullopt;
    }
    DecodedNoteEvent decoded;
    decoded.isNoteOn = kind == 0x90;
    decoded.channel = event.statusByte & 0x0F;
    decoded.note = event.data1;
    decoded.velocity = event.data2;
    return decoded;
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
    // Lower bound: the gain square wave forces a change every block, far
    // faster than the 20 ms gate, so a real 20 ms limiter sends close to the
    // ideal 50/s (46 observed) -- a limiter gating at some much wider
    // interval (e.g. once a second) would also read <= 50 and pass unnoticed
    // without this bound.
    REQUIRE_TRUE(ccCount >= 40);
}

// ---------------------------------------------------------------------------
// level_cc_value_matches_an_independently_computed_follower_level
// ---------------------------------------------------------------------------
// The Level CC's actual byte, checked against a known level rather than only
// "differs from the last one sent": replays the captured output audio
// through a FRESH dsp::SingleEnvelopeFollower (the same formula and
// SetSampleRate() call, an object of its own -- independent of production's
// own midiOutLevelFollower_) and asserts every sent CC equals
// round(127 * that independent level) at the sent event's own sample. A
// wrong scale (e.g. 254x instead of 127x) or a wrong speed (e.g. a missing
// SetSampleRate() call, which changes the level this independent replica
// converges to at any given sample) both show up as a mismatch.
TEST_CASE(level_cc_value_matches_an_independently_computed_follower_level) {
    Rig::AudioSettings settings48k128;
    settings48k128.sampleRate = 48000.0;
    settings48k128.blockSize = 128;
    Rig rig(/*patchPumpBudgetBlocks=*/64,
           UseScratchRuntimeDataPaths("level_cc_matches_independent_follower"), settings48k128);

    rig.Application().SetMidiOutSetting(LevelSetting(/*channel=*/0, /*ccNumber=*/16));
    rig.StartAt(0);

    synth_froggers::dsp::SingleEnvelopeFollower independentFollower;
    independentFollower.SetSampleRate(48000.0f);

    std::vector<float> independentLevelAtSample;
    struct SentCc { std::uint64_t sample; std::uint8_t value; };
    std::vector<SentCc> sentCcs;

    constexpr std::size_t kBlocksPerSecond = 48000 / 128;
    std::uint64_t blockStartSample = 0;
    for (std::size_t i = 0; i < kBlocksPerSecond; ++i) {
        rig.RunBlocks(1);
        for (const auto& frame : rig.Output()) {
            const float fold = 0.5f * (frame.channels[0] + frame.channels[1]);
            independentLevelAtSample.push_back(independentFollower.Process(fold));
        }
        rig.ClearOutput();
        const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
        for (const synth::AppMidiOutEvent& event : events) {
            REQUIRE_TRUE(event.statusByte == 0xB0);  // Control Change, channel 0.
            sentCcs.push_back({blockStartSample + event.frame, event.data2});
        }
        blockStartSample += 128;
    }

    REQUIRE_TRUE(sentCcs.size() > 1);  // the attack ramp sends more than the first-ever value.
    for (const SentCc& sent : sentCcs) {
        REQUIRE_TRUE(sent.sample < independentLevelAtSample.size());
        const int expected =
            std::clamp(static_cast<int>(std::lround(127.0f * independentLevelAtSample[sent.sample])), 0, 127);
        REQUIRE_TRUE(static_cast<int>(sent.value) == expected);
    }
}

// ---------------------------------------------------------------------------
// level_cc_gap_is_20ms_at_44100_and_96000hz
// ---------------------------------------------------------------------------
// A12: the 20 ms minimum gap (ceil(0.02 * sampleRate) frames) scaled by the
// actual configured rate, not the 48 kHz it happens to default to -- a
// hardcoded frame count would read half the real gap at 96 kHz and pass
// unnoticed at every test that only ever runs at 48 kHz.
void AssertLevelCcGapIsAbout20Ms(double sampleRate, const char* scratchName) {
    Rig::AudioSettings settings;
    settings.sampleRate = sampleRate;
    settings.blockSize = 128;
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths(scratchName), settings);

    rig.Application().SetMidiOutSetting(LevelSetting(/*channel=*/0, /*ccNumber=*/16));
    rig.StartAt(0);

    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    synth::Parameter& gain = model.PageParameter(synth_froggers::FroggersBankId::Drive, 1);

    const std::size_t blocksPerSecond = static_cast<std::size_t>(sampleRate / 128.0);
    std::optional<std::uint64_t> lastSentAbsoluteSample;
    std::uint64_t absoluteSample = 0;
    std::optional<std::uint64_t> minGapSamples;
    for (std::size_t i = 0; i < blocksPerSecond; ++i) {
        gain.SceneCenter(0) = (i % 2 == 0) ? 0.02f : 1.0f;
        rig.Application().TestParameterManager().ComputeAllParameters();
        rig.RunBlocks(1);
        absoluteSample += 128;
        const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
        REQUIRE_TRUE(events.Size() <= 1);
        if (events.Size() == 1) {
            const std::uint64_t sentAtSample = absoluteSample - 128 + events[0].frame;
            if (lastSentAbsoluteSample.has_value()) {
                const std::uint64_t gap = sentAtSample - *lastSentAbsoluteSample;
                minGapSamples = minGapSamples.has_value() ? std::min(*minGapSamples, gap) : gap;
            }
            lastSentAbsoluteSample = sentAtSample;
        }
    }
    REQUIRE_TRUE(minGapSamples.has_value());
    // The enforced minimum, and the same bound quantized up to the next
    // block boundary (sends only land at a block's last frame, so the
    // smallest OBSERVED gap can be up to one block period above it).
    const auto enforcedMinGapSamples = static_cast<std::uint64_t>(std::ceil(0.02 * sampleRate));
    REQUIRE_TRUE(*minGapSamples >= enforcedMinGapSamples);
    REQUIRE_TRUE(*minGapSamples < enforcedMinGapSamples + 128);
}

TEST_CASE(level_cc_gap_is_20ms_at_44100_and_96000hz) {
    AssertLevelCcGapIsAbout20Ms(44100.0, "level_cc_gap_44100");
    AssertLevelCcGapIsAbout20Ms(96000.0, "level_cc_gap_96000");
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

// ---------------------------------------------------------------------------
// pitch_detector_constructions_count_once_per_prepare
// ---------------------------------------------------------------------------
TEST_CASE(pitch_detector_constructions_count_once_per_prepare) {
    Rig rig(/*patchPumpBudgetBlocks=*/64,
           UseScratchRuntimeDataPaths("pitch_detector_constructions"));

    // SynthRig's constructor already ran Init() then Prepare() once, before
    // any ProcessBlock -- see this file's own header comment.
    REQUIRE_TRUE(rig.Application().PitchDetectorConstructions() == 1);

    rig.Application().SetMidiOutSetting(PitchSetting(/*channel=*/0));
    rig.StartAt(0);
    rig.RunBlocks(1000);

    REQUIRE_TRUE(rig.Application().PitchDetectorConstructions() == 1);
}

// ---------------------------------------------------------------------------
// pitch_default_patch_sends_note_45_and_stays_sounding
// ---------------------------------------------------------------------------
// Asserts the player's story on the default patch: over 60 s it sends 0
// exact-octave jumps and its first note-on is note 45. Also covers the
// "Only the chosen content is sent" scenario's Pitch half: every appended
// message across the run is a note-on or note-off on the set channel.
//
// A real render (below) shows the output briefly reports note 48 for
// ~43 ms around the 55.5 s mark (a release-tail transient, not an octave
// jump) before returning to 45, producing two note-offs and three note-ons
// in total -- an exact match to measure-q/report-shipped-rule.md Table 1's
// own recorded figure for this patch (3 note-ons, 0 octave jumps). This
// test asserts the first note-on, the octave-jump count, and the note
// sounding at the end (45); it does not assert zero note-offs, since a real
// render produces two.
TEST_CASE(pitch_default_patch_sends_note_45_and_stays_sounding) {
    Rig::AudioSettings settings48k128;
    settings48k128.sampleRate = 48000.0;
    settings48k128.blockSize = 128;
    Rig rig(/*patchPumpBudgetBlocks=*/64,
           UseScratchRuntimeDataPaths("pitch_default_patch_note_45"), settings48k128);

    rig.Application().SetMidiOutSetting(PitchSetting(/*channel=*/3));
    rig.StartAt(0);

    std::optional<std::uint8_t> firstNoteOn;
    std::optional<std::uint8_t> previousNoteOnNote;
    std::optional<std::uint8_t> soundingNote;
    std::size_t noteOnCount = 0;
    std::size_t noteOffCount = 0;
    std::size_t octaveJumpCount = 0;

    constexpr std::size_t kBlocksPerSecond = 48000 / 128;
    for (std::size_t i = 0; i < kBlocksPerSecond * 60; ++i) {
        rig.RunBlocks(1);
        const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
        REQUIRE_TRUE(events.Size() <= 2);  // at most a note-off and a note-on.
        for (const synth::AppMidiOutEvent& event : events) {
            const std::optional<DecodedNoteEvent> decoded = DecodeNoteEvent(event);
            REQUIRE_TRUE(decoded.has_value());  // only note-on/off is ever appended for Pitch.
            REQUIRE_TRUE(decoded->channel == 3);  // the set channel.
            if (decoded->isNoteOn) {
                ++noteOnCount;
                if (!firstNoteOn.has_value()) {
                    firstNoteOn = decoded->note;
                }
                if (previousNoteOnNote.has_value() &&
                    std::abs(static_cast<int>(decoded->note) - static_cast<int>(*previousNoteOnNote)) == 12) {
                    ++octaveJumpCount;
                }
                previousNoteOnNote = decoded->note;
                soundingNote = decoded->note;
            } else {
                ++noteOffCount;
                soundingNote = std::nullopt;
            }
        }
    }

    std::cout << "  [OBSERVED] default patch, 60 s: " << noteOnCount << " note-on(s), " << noteOffCount
              << " note-off(s), " << octaveJumpCount << " octave jump(s)\n";
    REQUIRE_TRUE(firstNoteOn.has_value());
    REQUIRE_TRUE(*firstNoteOn == 45);
    REQUIRE_TRUE(octaveJumpCount == 0);
    REQUIRE_TRUE(soundingNote.has_value());
    REQUIRE_TRUE(*soundingNote == 45);
}

// ---------------------------------------------------------------------------
// pitch_first_note_is_45_at_44100_and_96000hz
// ---------------------------------------------------------------------------
// A12: the detector is constructed at the actual configured sample rate,
// not a rate it happens to default to -- a hardcoded 48 kHz would shift
// every detected frequency (about +1.47 semitones at 44.1 kHz, an octave
// down at 96 kHz), so the default patch's first note would read as
// something other than 45.
void AssertPitchFirstNoteIs45(double sampleRate, const char* scratchName) {
    Rig::AudioSettings settings;
    settings.sampleRate = sampleRate;
    settings.blockSize = 128;
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths(scratchName), settings);

    rig.Application().SetMidiOutSetting(PitchSetting(/*channel=*/0));
    rig.StartAt(0);

    std::optional<std::uint8_t> firstNoteOn;
    const std::size_t blocksPerSecond = static_cast<std::size_t>(sampleRate / 128.0);
    for (std::size_t i = 0; i < blocksPerSecond && !firstNoteOn.has_value(); ++i) {
        rig.RunBlocks(1);
        const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
        for (const synth::AppMidiOutEvent& event : events) {
            const std::optional<DecodedNoteEvent> decoded = DecodeNoteEvent(event);
            if (decoded.has_value() && decoded->isNoteOn) {
                firstNoteOn = decoded->note;
            }
        }
    }
    REQUIRE_TRUE(firstNoteOn.has_value());
    REQUIRE_TRUE(*firstNoteOn == 45);
}

TEST_CASE(pitch_first_note_is_45_at_44100_and_96000hz) {
    AssertPitchFirstNoteIs45(44100.0, "pitch_note45_44100");
    AssertPitchFirstNoteIs45(96000.0, "pitch_note45_96000");
}

// ---------------------------------------------------------------------------
// pitch_tracks_the_ruled_range_edges
// ---------------------------------------------------------------------------
// A13: the ruled 50-5,000 Hz range is exercised past the default patch's own
// 110 Hz -- a low edge inside 50-100 Hz and a note above 1 kHz -- so a
// narrower detector range (e.g. a 100 Hz floor or a 1,000 Hz ceiling) is
// caught even though the default patch's own note still detects fine.
void AssertPitchTracksFrequency(float targetHz, int expectedNote, const char* scratchName) {
    Rig::AudioSettings settings48k128;
    settings48k128.sampleRate = 48000.0;
    settings48k128.blockSize = 128;
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths(scratchName), settings48k128);

    rig.Application().SetMidiOutSetting(PitchSetting(/*channel=*/0));

    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    const float knob = PitchKnobForFrequency(targetHz);
    for (std::size_t vco = 0; vco < 3; ++vco) {
        model.PageParameter(synth_froggers::FroggersBankId::Audio,
                            synth_froggers::AudioSlot(vco, synth_froggers::VcoSlotRole::Pitch))
            .SceneCenter(0) = knob;
    }
    rig.StartAt(0);

    std::optional<std::uint8_t> firstNoteOn;
    constexpr std::size_t kBlocksPerSecond = 48000 / 128;
    for (std::size_t i = 0; i < kBlocksPerSecond * 2 && !firstNoteOn.has_value(); ++i) {
        rig.RunBlocks(1);
        const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
        for (const synth::AppMidiOutEvent& event : events) {
            const std::optional<DecodedNoteEvent> decoded = DecodeNoteEvent(event);
            if (decoded.has_value() && decoded->isNoteOn) {
                firstNoteOn = decoded->note;
            }
        }
    }
    REQUIRE_TRUE(firstNoteOn.has_value());
    REQUIRE_TRUE(*firstNoteOn == expectedNote);
}

TEST_CASE(pitch_tracks_the_ruled_range_edges) {
    AssertPitchTracksFrequency(65.406f, 36, "pitch_range_low_edge");    // C2, inside 50-100 Hz.
    AssertPitchTracksFrequency(1567.98f, 91, "pitch_range_above_1khz");  // G6, above 1 kHz.
}

// The Pitch note the 20-step schedule below raises the default patch's
// three VCOs to, measure-q/QShippedRule.cpp Item 2's own target.
constexpr int kPitchStepTargetNote = 52;

// ---------------------------------------------------------------------------
// pitch_step_confirms_within_100ms
// ---------------------------------------------------------------------------
// The requirement's own <=100 ms latency bound, measured the way
// measure-q/QShippedRule.cpp Item 2 measured it (the same render
// measure-q/report-shipped-rule.md Table 2's 75.417 ms/94.271 ms figures
// come from): 20 up/back steps of the default patch's three VCO pitch
// knobs, a factor of 1.5, one every quarter note (0.5 s at the default
// 120 bpm) and landing 5% into that quarter's own gate-open window, worst
// latency taken over all 20 "up" transitions to the real MIDI-out note-on
// for note 52.
void AssertPitchStepConfirmsWithin100Ms(int blockSize, double& outWorstLatencyMs) {
    Rig::AudioSettings settings;
    settings.sampleRate = 48000.0;
    settings.blockSize = blockSize;
    Rig rig(/*patchPumpBudgetBlocks=*/64,
           UseScratchRuntimeDataPaths(blockSize == 128 ? "pitch_step_128" : "pitch_step_256"), settings);

    rig.Application().SetMidiOutSetting(PitchSetting(/*channel=*/0));
    rig.StartAt(0);

    const double quarterNoteSamples = 1.0 / rig.Engine().Clock().QuarterNotesPerSample();
    const std::size_t offsetFrames = static_cast<std::size_t>(0.05 * quarterNoteSamples);
    const double totalSeconds = 0.5 * 41.0 + 1.0;
    const std::size_t totalFrames = static_cast<std::size_t>(48000.0 * totalSeconds);

    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    const float delta = PitchKnobRaiseDelta(1.5f);
    // The default patch's own VCO1/2/3 pitch knobs, read once: each step
    // below sets an ABSOLUTE value relative to this fixed baseline (raised
    // or back to it), not a cumulative add, so 20 up/back cycles land on
    // exactly the same two values every time.
    std::array<float, 3> baseline{};
    for (std::size_t vco = 0; vco < 3; ++vco) {
        baseline[vco] = model.PageParameter(
                             synth_froggers::FroggersBankId::Audio,
                             synth_froggers::AudioSlot(vco, synth_froggers::VcoSlotRole::Pitch))
                            .SceneCenter(0);
    }

    std::vector<std::uint64_t> raisedTransitionSamples;
    std::vector<std::uint64_t> targetNoteOnSamples;

    std::size_t framesRun = 0;
    std::uint64_t blockStartSample = 0;
    std::size_t nextStepIx = 0;
    bool raised = false;
    while (framesRun < totalFrames) {
        if (nextStepIx < 40) {
            const std::size_t targetFrame =
                static_cast<std::size_t>(static_cast<double>(nextStepIx) * quarterNoteSamples) + offsetFrames;
            if (framesRun >= targetFrame) {
                raised = !raised;
                for (std::size_t vco = 0; vco < 3; ++vco) {
                    model.PageParameter(
                             synth_froggers::FroggersBankId::Audio,
                             synth_froggers::AudioSlot(vco, synth_froggers::VcoSlotRole::Pitch))
                        .SceneCenter(0) = baseline[vco] + (raised ? delta : 0.0f);
                }
                if (raised) {
                    raisedTransitionSamples.push_back(framesRun);
                }
                ++nextStepIx;
            }
        }
        rig.RunBlocks(1);
        const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
        for (const synth::AppMidiOutEvent& event : events) {
            const std::optional<DecodedNoteEvent> decoded = DecodeNoteEvent(event);
            if (decoded.has_value() && decoded->isNoteOn && decoded->note == kPitchStepTargetNote) {
                targetNoteOnSamples.push_back(blockStartSample + event.frame);
            }
        }
        blockStartSample += static_cast<std::uint64_t>(blockSize);
        framesRun += static_cast<std::size_t>(blockSize);
    }

    REQUIRE_TRUE(raisedTransitionSamples.size() == 20);

    std::uint64_t worstLatencyFrames = 0;
    std::size_t measuredTransitions = 0;
    for (std::uint64_t transitionSample : raisedTransitionSamples) {
        const auto found = std::find_if(targetNoteOnSamples.begin(), targetNoteOnSamples.end(),
                                        [&](std::uint64_t sample) { return sample >= transitionSample; });
        REQUIRE_TRUE(found != targetNoteOnSamples.end());
        const std::uint64_t latencyFrames = *found - transitionSample;
        worstLatencyFrames = std::max(worstLatencyFrames, latencyFrames);
        ++measuredTransitions;
    }

    REQUIRE_TRUE(measuredTransitions == 20);
    REQUIRE_TRUE(worstLatencyFrames <= 4800);  // 100 ms at 48 kHz.
    outWorstLatencyMs = static_cast<double>(worstLatencyFrames) * 1000.0 / 48000.0;
}

TEST_CASE(pitch_step_confirms_within_100ms_at_128_and_256_frames) {
    double worst128 = 0.0;
    double worst256 = 0.0;
    AssertPitchStepConfirmsWithin100Ms(128, worst128);
    AssertPitchStepConfirmsWithin100Ms(256, worst256);
    // measure-q/report-shipped-rule.md Table 2's own figures for this exact
    // render: 75.417 ms at 128-frame, 94.271 ms at 256-frame.
    std::cout << "  [pitch latency] 128-frame: " << worst128
              << " ms (shipped-rule reference 75.417 ms); 256-frame: " << worst256
              << " ms (shipped-rule reference 94.271 ms)\n";
}

// ---------------------------------------------------------------------------
// pitch_quiet_output_ends_the_note
// ---------------------------------------------------------------------------
TEST_CASE(pitch_quiet_output_ends_the_note) {
    Rig::AudioSettings settings48k128;
    settings48k128.sampleRate = 48000.0;
    settings48k128.blockSize = 128;
    Rig rig(/*patchPumpBudgetBlocks=*/64,
           UseScratchRuntimeDataPaths("pitch_quiet_output_ends_note"), settings48k128);

    rig.Application().SetMidiOutSetting(PitchSetting(/*channel=*/0));
    rig.StartAt(0);

    constexpr std::size_t kBlocksPerSecond = 48000 / 128;
    for (std::size_t i = 0; i < kBlocksPerSecond * 2; ++i) {
        rig.RunBlocks(1);
    }
    REQUIRE_TRUE(rig.Application().MidiOutContent() == synth_froggers::FroggersMidiOutContent::Pitch);

    rig.StopAt(0);

    bool sawNoteOffAfterStop = false;
    bool sawNoteOnAfterNoteOff = false;
    for (std::size_t i = 0; i < kBlocksPerSecond * 30; ++i) {
        rig.RunBlocks(1);
        const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
        for (const synth::AppMidiOutEvent& event : events) {
            const std::optional<DecodedNoteEvent> decoded = DecodeNoteEvent(event);
            REQUIRE_TRUE(decoded.has_value());
            if (decoded->isNoteOn) {
                if (sawNoteOffAfterStop) {
                    sawNoteOnAfterNoteOff = true;
                }
            } else {
                sawNoteOffAfterStop = true;
            }
        }
        if (sawNoteOffAfterStop) {
            break;
        }
    }
    REQUIRE_TRUE(sawNoteOffAfterStop);
    REQUIRE_TRUE(!sawNoteOnAfterNoteOff);

    // No note-on follows for the rest of the (up to) 30 s render window.
    for (std::size_t i = 0; i < kBlocksPerSecond * 28; ++i) {
        rig.RunBlocks(1);
        const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
        for (const synth::AppMidiOutEvent& event : events) {
            const std::optional<DecodedNoteEvent> decoded = DecodeNoteEvent(event);
            REQUIRE_TRUE(decoded.has_value());
            REQUIRE_TRUE(!decoded->isNoteOn);
        }
    }
}

// ---------------------------------------------------------------------------
// pitch_velocity_follows_the_level_unless_fixed
// ---------------------------------------------------------------------------
TEST_CASE(pitch_velocity_follows_the_level_unless_fixed) {
    // Fixed: every note-on's velocity is the fixed value, regardless of the
    // real follower level.
    {
        Rig::AudioSettings settings48k128;
        settings48k128.sampleRate = 48000.0;
        settings48k128.blockSize = 128;
        Rig rig(/*patchPumpBudgetBlocks=*/64,
               UseScratchRuntimeDataPaths("pitch_velocity_fixed"), settings48k128);
        rig.Application().SetMidiOutSetting(PitchSetting(/*channel=*/0, /*fixedVelocity=*/90));
        rig.StartAt(0);

        bool sawNoteOn = false;
        constexpr std::size_t kBlocksPerSecond = 48000 / 128;
        for (std::size_t i = 0; i < kBlocksPerSecond * 2; ++i) {
            rig.RunBlocks(1);
            const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
            for (const synth::AppMidiOutEvent& event : events) {
                const std::optional<DecodedNoteEvent> decoded = DecodeNoteEvent(event);
                if (decoded.has_value() && decoded->isNoteOn) {
                    sawNoteOn = true;
                    REQUIRE_TRUE(decoded->velocity == 90);
                }
            }
        }
        REQUIRE_TRUE(sawNoteOn);
    }

    // Level (the default): a note-on's velocity is round(127*level), held
    // within 1 to 127, where `level` is checked against an INDEPENDENT
    // dsp::SingleEnvelopeFollower replica fed the captured output audio
    // (the same technique level_cc_value_matches_an_independently_computed_
    // follower_level uses) rather than reading back production's own
    // captured value -- a formula that writes the wrong source into that
    // captured value would go unnoticed by a check that reads the same
    // corrupted value back.
    {
        Rig::AudioSettings settings48k128;
        settings48k128.sampleRate = 48000.0;
        settings48k128.blockSize = 128;
        Rig rig(/*patchPumpBudgetBlocks=*/64,
               UseScratchRuntimeDataPaths("pitch_velocity_level"), settings48k128);
        rig.Application().SetMidiOutSetting(PitchSetting(/*channel=*/0));
        rig.StartAt(0);

        synth_froggers::dsp::SingleEnvelopeFollower independentFollower;
        independentFollower.SetSampleRate(48000.0f);
        std::vector<float> independentLevelAtSample;

        bool sawNoteOn = false;
        constexpr std::size_t kBlocksPerSecond = 48000 / 128;
        std::uint64_t blockStartSample = 0;
        for (std::size_t i = 0; i < kBlocksPerSecond * 2; ++i) {
            rig.RunBlocks(1);
            for (const auto& frame : rig.Output()) {
                const float fold = 0.5f * (frame.channels[0] + frame.channels[1]);
                independentLevelAtSample.push_back(independentFollower.Process(fold));
            }
            rig.ClearOutput();
            const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
            for (const synth::AppMidiOutEvent& event : events) {
                const std::optional<DecodedNoteEvent> decoded = DecodeNoteEvent(event);
                if (decoded.has_value() && decoded->isNoteOn) {
                    sawNoteOn = true;
                    const std::uint64_t sample = blockStartSample + event.frame;
                    REQUIRE_TRUE(sample < independentLevelAtSample.size());
                    const int expected = std::clamp(
                        static_cast<int>(std::lround(127.0f * independentLevelAtSample[sample])), 1, 127);
                    REQUIRE_TRUE(decoded->velocity == expected);
                }
            }
            blockStartSample += 128;
        }
        REQUIRE_TRUE(sawNoteOn);
    }
}

// ---------------------------------------------------------------------------
// pitch_several_reports_in_one_block_send_one_change
// ---------------------------------------------------------------------------
// Phase modulation at maximum (all three VCOs), 48 kHz / 2048-frame blocks:
// dense enough pitch content that several of Q's ~20 ms-apart reports
// regularly land inside one 2048-frame (~42.7 ms) block and disagree.
// TestPitchNoteChangesLastBlock() (a test-only counter of how many times
// ProcessBlock's own per-sample loop reassigned the sounding note this
// block, reset every block) finds those blocks from outside; the appended
// events are then checked against the requirement: at most a note-off and a
// note-on, the note-on naming whatever note was ACTUALLY sounding at the
// block's end (which TestPitchNoteChangesLastBlock() alone cannot say, so
// this also cross-checks against the following block's own start state,
// implicitly continuous with this block's note-on).
TEST_CASE(pitch_several_reports_in_one_block_send_one_change) {
    Rig::AudioSettings settings;
    settings.sampleRate = 48000.0;
    settings.blockSize = 2048;
    Rig rig(/*patchPumpBudgetBlocks=*/64,
           UseScratchRuntimeDataPaths("pitch_several_reports_one_block"), settings);
    rig.Application().SetMidiOutSetting(PitchSetting(/*channel=*/0));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    for (std::size_t vco = 0; vco < 3; ++vco) {
        model
            .PageParameter(synth_froggers::FroggersBankId::Audio,
                           synth_froggers::AudioSlot(vco, synth_froggers::VcoSlotRole::PhaseMod))
            .SceneCenter(0) = 1.0f;
    }
    rig.Application().TestParameterManager().ComputeAllParameters();
    rig.StartAt(0);

    const std::size_t blocksPerSecond = 48000 / 2048;
    std::size_t severalReportBlocksSeen = 0;
    std::size_t twoEventBlocksSeen = 0;
    // Tracked from the events themselves (not read back from production's
    // own soundingPitchNote_): whatever the LAST note-on named, across every
    // block so far -- so a note-on for a different note, with no preceding
    // note-off for this one, is caught on ANY block, not only the ones this
    // patch happens to also report 2+ times in.
    std::optional<std::uint8_t> lastKnownSoundingNote;
    for (std::size_t i = 0; i < blocksPerSecond * 60; ++i) {
        rig.RunBlocks(1);
        const std::size_t changes = rig.Application().TestPitchNoteChangesLastBlock();
        const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
        REQUIRE_TRUE(events.Size() <= 2);  // at most a note-off and a note-on, regardless of `changes`.
        if (changes >= 2) {
            ++severalReportBlocksSeen;
        }
        if (events.Size() == 2) {
            ++twoEventBlocksSeen;
            const std::optional<DecodedNoteEvent> first = DecodeNoteEvent(events[0]);
            const std::optional<DecodedNoteEvent> second = DecodeNoteEvent(events[1]);
            REQUIRE_TRUE(first.has_value() && !first->isNoteOn);   // note-off first.
            REQUIRE_TRUE(second.has_value() && second->isNoteOn);  // note-on second.
            REQUIRE_TRUE(first->note != second->note);  // a genuine change, not a same-note replay.
            REQUIRE_TRUE(events[0].frame == events[1].frame);  // both at the later report's frame.
            REQUIRE_TRUE(lastKnownSoundingNote.has_value() && *lastKnownSoundingNote == first->note);
            lastKnownSoundingNote = second->note;
        } else if (events.Size() == 1) {
            const std::optional<DecodedNoteEvent> only = DecodeNoteEvent(events[0]);
            REQUIRE_TRUE(only.has_value());
            if (only->isNoteOn) {
                // A note-on with nothing else this block: only valid when no
                // note was already sounding -- a genuine change instead
                // needs the note-off too (a poly synth downstream would
                // otherwise stack a stuck note from the one this dropped).
                REQUIRE_TRUE(!lastKnownSoundingNote.has_value());
                lastKnownSoundingNote = only->note;
            } else {
                REQUIRE_TRUE(lastKnownSoundingNote.has_value() && *lastKnownSoundingNote == only->note);
                lastKnownSoundingNote = std::nullopt;
            }
        }
    }
    // Positive controls: this patch really does produce blocks where several
    // reports land and disagree, and blocks where that disagreement reaches
    // the MIDI-out pair -- otherwise the assertions above never ran.
    REQUIRE_TRUE(severalReportBlocksSeen > 0);
    REQUIRE_TRUE(twoEventBlocksSeen > 0);
}

// ---------------------------------------------------------------------------
// pitch_switching_away_ends_the_note
// ---------------------------------------------------------------------------
TEST_CASE(pitch_switching_away_ends_the_note) {
    Rig::AudioSettings settings48k128;
    settings48k128.sampleRate = 48000.0;
    settings48k128.blockSize = 128;
    Rig rig(/*patchPumpBudgetBlocks=*/64,
           UseScratchRuntimeDataPaths("pitch_switching_away_ends_note"), settings48k128);

    rig.Application().SetMidiOutSetting(PitchSetting(/*channel=*/5));
    rig.StartAt(0);
    constexpr std::size_t kBlocksPerSecond = 48000 / 128;
    for (std::size_t i = 0; i < kBlocksPerSecond; ++i) {
        rig.RunBlocks(1);
    }
    REQUIRE_TRUE(rig.Application().MidiOutContent() == synth_froggers::FroggersMidiOutContent::Pitch);

    rig.Application().SetMidiOutSetting(LevelSetting(/*channel=*/5, /*ccNumber=*/16));
    rig.RunBlocks(1);
    const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
    REQUIRE_TRUE(events.Size() >= 1);
    const std::optional<DecodedNoteEvent> first = DecodeNoteEvent(events[0]);
    REQUIRE_TRUE(first.has_value());
    REQUIRE_TRUE(!first->isNoteOn);        // note-off, before any other message.
    REQUIRE_TRUE(first->note == 45);       // the note that was sounding.
    REQUIRE_TRUE(first->channel == 5);     // the channel it was sounding on.
    REQUIRE_TRUE(events[0].frame == 0);    // frame 0.
}

// ---------------------------------------------------------------------------
// pitch_changing_the_channel_ends_the_note_on_the_old_channel
// ---------------------------------------------------------------------------
TEST_CASE(pitch_changing_the_channel_ends_the_note_on_the_old_channel) {
    Rig::AudioSettings settings48k128;
    settings48k128.sampleRate = 48000.0;
    settings48k128.blockSize = 128;
    Rig rig(/*patchPumpBudgetBlocks=*/64,
           UseScratchRuntimeDataPaths("pitch_changing_channel_ends_note"), settings48k128);

    rig.Application().SetMidiOutSetting(PitchSetting(/*channel=*/0));
    rig.StartAt(0);
    constexpr std::size_t kBlocksPerSecond = 48000 / 128;
    for (std::size_t i = 0; i < kBlocksPerSecond; ++i) {
        rig.RunBlocks(1);
    }
    REQUIRE_TRUE(rig.Application().MidiOutContent() == synth_froggers::FroggersMidiOutContent::Pitch);

    rig.Application().SetMidiOutSetting(PitchSetting(/*channel=*/1));
    rig.RunBlocks(1);
    const synth::AppMidiOutEventList& firstEvents = rig.Engine().AppMidiOutEvents();
    REQUIRE_TRUE(firstEvents.Size() >= 1);
    const std::optional<DecodedNoteEvent> off = DecodeNoteEvent(firstEvents[0]);
    REQUIRE_TRUE(off.has_value());
    REQUIRE_TRUE(!off->isNoteOn);
    REQUIRE_TRUE(off->note == 45);
    REQUIRE_TRUE(off->channel == 0);  // sent on the OLD channel.
    REQUIRE_TRUE(firstEvents[0].frame == 0);

    // The next note-on -- possibly later in this SAME block (a fresh report
    // can land right after the block-start note-off, since the per-block
    // bookkeeping above already reads no note as sounding), or in a later
    // one -- is on the new channel.
    bool sawNextNoteOn = false;
    for (std::size_t eventIx = 1; eventIx < firstEvents.Size(); ++eventIx) {
        const std::optional<DecodedNoteEvent> decoded = DecodeNoteEvent(firstEvents[eventIx]);
        if (decoded.has_value() && decoded->isNoteOn) {
            sawNextNoteOn = true;
            REQUIRE_TRUE(decoded->channel == 1);
        }
    }
    for (std::size_t i = 0; i < kBlocksPerSecond && !sawNextNoteOn; ++i) {
        rig.RunBlocks(1);
        const synth::AppMidiOutEventList& events = rig.Engine().AppMidiOutEvents();
        for (const synth::AppMidiOutEvent& event : events) {
            const std::optional<DecodedNoteEvent> decoded = DecodeNoteEvent(event);
            if (decoded.has_value() && decoded->isNoteOn) {
                sawNextNoteOn = true;
                REQUIRE_TRUE(decoded->channel == 1);
            }
        }
    }
    REQUIRE_TRUE(sawNextNoteOn);
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
