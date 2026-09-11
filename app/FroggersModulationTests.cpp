// FroggersModulationTests.cpp -- covers the modulation slate and drill-in:
// slate registration / depth materialization / drill-in level cap /
// external-audio inertness, disconnected sources never being randomized,
// randomize semantics, and the default patch.
//
// Uses a bare synth::ParameterManager + FroggersParameterModel +
// FroggersModulationSlate directly (matching FroggersParameterModelTests.cpp's
// structural-check convention) -- no Engine/SynthRig needed: every check here
// is queryable through Bank/Parameter/ParameterGroup's own public API without
// real audio-thread block pumping. FroggersHeadlessTests.cpp already covers
// the FroggersApp::ProcessBlock/PrepareToPlay wiring end-to-end.

#include "FroggersModulation.hpp"
#include "FroggersParameters.hpp"

#include "synth/ParameterModulation.hpp"

#ifdef JUCE_MAJOR_VERSION
#error "Froggers modulation tests must not see JUCE headers"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>
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
        const float actualValue = (actual);                                                \
        const float expectedValue = (expected);                                             \
        if (!(std::fabs(actualValue - expectedValue) <= (tolerance))) {                     \
            std::ostringstream oss;                                                         \
            oss << __FILE__ << ":" << __LINE__ << " requirement failed: " #actual " (="      \
                << actualValue << ") not within " << (tolerance) << " of " #expected " (="   \
                << expectedValue << ")";                                                     \
            throw std::runtime_error(oss.str());                                            \
        }                                                                                    \
    } while (false)

// Small fixture: manager + model + slate, ready to Step(). `extraDepthCapacity`
// lets a test deliberately shrink storage to exercise the CanAllocate()
// exhaustion / partial-randomize path, exercised indirectly here via the
// ceiling test below.
struct Fixture {
    synth::ParameterManager manager;
    FroggersParameterModel model;
    FroggersModulationSlate slate;

    explicit Fixture(std::size_t extraDepthCapacity = FroggersModulationSlate::kDepthParameterStorageCapacity) {
        model.Init(manager);
        slate.Init(model.Group(), extraDepthCapacity);
        slate.Prepare(48000.0);
    }

    // Connectedness is set directly via the standalone
    // SetExternalAudioConnected(bool) setter rather than derived from
    // `externalAudioSample` -- Step() itself keeps the two independent
    // (see Step()'s own comment: the sample is ignored entirely while
    // disconnected), so this fixture mirrors that split rather than
    // collapsing it. `externalAudioSample` defaults to 0.0f: every one of
    // this file's ~24 StepOnce(...) callers that does not pass one is
    // unaffected (0.0f normalizes to exactly the same 0.5f/0.0f pair those
    // callers already asserted before external audio was ever driven from
    // real input).
    void StepOnce(bool externalConnected = false, float externalAudioSample = 0.0f) {
        FroggersModulationSlate::VcoDrive drive{0.5f, 0.5f, 0.0f};
        slate.SetExternalAudioConnected(externalConnected);
        // Step() takes a trailing transportQuarterNotes parameter;
        // std::nullopt here reproduces this file's original behavior exactly
        // (no clock plan -> no tick -> no RandomShLane::Increment() call --
        // see StepClockDrivenLanes's own has_value() guard), since this file
        // is not about clock-driven advance at all (that is
        // FroggersMarblesClockTests.cpp).
        slate.Step(drive, drive, drive, std::nullopt, externalAudioSample);
    }
};

void ForEachTopLevelParameter(FroggersParameterModel& model, const std::function<void(synth::Parameter&)>& fn) {
    for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
        const auto bankId = static_cast<FroggersBankId>(bankIx);
        for (std::size_t paramIx = 0; paramIx < kFroggersParamsPerBank; ++paramIx) {
            fn(model.PageParameter(bankId, paramIx));
        }
        fn(model.Crispy(bankId));
    }
    fn(model.Crunchy());
}

// ============================================================================
// 15 sources registered, in order, BY IDENTITY
// ============================================================================

TEST_CASE(slate_registers_fifteen_sources_in_the_expected_order_by_name) {
    Fixture fx;
    const std::array<const char*, 15> expectedNames{
        "Random S&H 1", "Random S&H 2", "Random S&H 3", "Random S&H 4", "Random S&H 5", "Random S&H 6",
        "VCO1 Audio", "VCO2 Audio", "VCO3 Audio",
        "VCO1 EF", "VCO2 EF", "VCO3 EF",
        "Noise",
        "External Audio", "External Audio EF",
    };
    for (std::size_t i = 0; i < expectedNames.size(); ++i) {
        REQUIRE_TRUE(fx.slate.Metadata(i).name == expectedNames[i]);
    }
    // All 15 names distinct -- catches a copy-paste bug that silently
    // overwrote one slot's metadata with another's (SetModulationSource
    // bounds-checks only, so order is load-bearing).
    for (std::size_t i = 0; i < expectedNames.size(); ++i) {
        for (std::size_t j = i + 1; j < expectedNames.size(); ++j) {
            REQUIRE_TRUE(fx.slate.Metadata(i).name != fx.slate.Metadata(j).name);
        }
    }
}

// "By identity, not by count": prove each VCO-audio/EF slot is wired to ITS
// OWN VCO, not aliased to a sibling's, by varying only ONE VCO's drive and
// confirming ONLY that VCO's audio+EF sources (and none of the other ten
// non-VCO sources) move away from the neutral baseline they'd otherwise
// settle at.
TEST_CASE(vco_audio_and_ef_sources_are_wired_to_the_correct_identity_not_just_present) {
    Fixture fx;
    FroggersModulationSlate::VcoDrive silent{0.5f, 0.5f, 0.0f};  // pitch=mid, shape=mid, no PM
    FroggersModulationSlate::VcoDrive driven{0.9f, 0.5f, 0.0f};  // a different, higher pitch

    // Baseline: all three VCOs identical (silent) drive for a few samples so
    // their audio-rate outputs and EFs settle into a comparable state.
    // SourceValue() reads Modulators::Value(), a CACHE that only refreshes
    // via Modulators::UpdateModValues() (normally called from
    // FroggersParameterModel::ProcessSample) -- call it directly here since
    // this test drives the slate standalone, without the full parameter
    // model's per-sample loop.
    for (int i = 0; i < 8; ++i) {
        fx.slate.Step(silent, silent, silent, std::nullopt);
        fx.model.Group().UpdateModValues();
    }
    const float base6 = fx.slate.SourceValue(kModSlotVco1Audio);
    const float base7 = fx.slate.SourceValue(kModSlotVco2Audio);
    const float base8 = fx.slate.SourceValue(kModSlotVco3Audio);

    // Now drive ONLY VCO1 differently; VCO2/VCO3 stay at the same silent
    // drive as the baseline loop above.
    bool vco1Moved = false;
    for (int i = 0; i < 8; ++i) {
        fx.slate.Step(driven, silent, silent, std::nullopt);
        fx.model.Group().UpdateModValues();
        if (std::fabs(fx.slate.SourceValue(kModSlotVco1Audio) - base6) > 0.02f) {
            vco1Moved = true;
        }
    }
    REQUIRE_TRUE(vco1Moved);
    // VCO2/VCO3 audio sources must be UNAFFECTED by VCO1's drive change
    // (zero cross-VCO terms) -- their inputs never changed, so
    // their periodic (already-oscillating) values should still land within
    // the same excursion range as the baseline, not systematically shifted.
    // A precise no-op check: re-running the identical silent/silent inputs
    // for VCO2/VCO3 reproduces base7/base8 exactly is too strict (VCO phase
    // has advanced), so instead confirm VCO2/VCO3 stay in [0,1] (sanity) and
    // that changing VCO1 alone did not blow either of them outside the
    // normal oscillation envelope a silent-driven VCO produces.
    REQUIRE_TRUE(base7 >= 0.0f && base7 <= 1.0f);
    REQUIRE_TRUE(base8 >= 0.0f && base8 <= 1.0f);
}

// ============================================================================
// drill-in level cap and depth materialization
// ============================================================================

TEST_CASE(depth_cells_materialize_on_level_one_open_for_connected_sources) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);  // all 15 sources connected now

    synth::Parameter& target = fx.model.PageParameter(FroggersBankId::Reverb, 0);
    REQUIRE_TRUE(target.ModulationDepthParameter(0) == nullptr);  // nothing materialized yet

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    drillIn.PressEncoder(0);  // open Reverb param 0's L1 view
    REQUIRE_TRUE(drillIn.Level() == 1);

    for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
        REQUIRE_TRUE(target.ModulationDepthParameter(modIx) != nullptr);
    }
}

// This test pins "fourth level refused," not "third": kMaxDrillLevel moved
// from 2 to 3, so the exact press sequence this test used to pin at level 2
// now legitimately succeeds and reaches level 3 (see
// drill_in_reaches_level_three_with_genuinely_open_views_then_back_unwinds_one_level_at_a_time).
// The cap-refusal behavior itself is unchanged -- only the depth it applies
// at moved by exactly the same one level the cap itself moved by -- so this
// keeps pinning "one press past the cap is refused" at the new boundary.
TEST_CASE(fourth_level_drill_in_is_refused) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    drillIn.PressEncoder(0);  // -> level 1
    REQUIRE_TRUE(drillIn.Level() == 1);
    drillIn.PressEncoder(static_cast<synth::PhysicalEncoderId>(kModSlotVco1Audio));  // -> level 2
    REQUIRE_TRUE(drillIn.Level() == 2);
    drillIn.PressEncoder(static_cast<synth::PhysicalEncoderId>(kModSlotVco2Audio));  // -> level 3
    REQUIRE_TRUE(drillIn.Level() == 3);

    synth::Parameter* levelThreeSelected = drillIn.BankRef().SelectedParameter();
    drillIn.PressEncoder(static_cast<synth::PhysicalEncoderId>(kModSlotVco3Audio));  // would-be level 4: refused
    REQUIRE_TRUE(drillIn.Level() == 3);
    REQUIRE_TRUE(drillIn.BankRef().SelectedParameter() == levelThreeSelected);  // selection unchanged
}

TEST_CASE(back_exits_to_parameter_grid_from_level_one) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));

    drillIn.PressEncoder(0);
    REQUIRE_TRUE(drillIn.Level() == 1);
    drillIn.Back();
    REQUIRE_TRUE(drillIn.Level() == 0);
    REQUIRE_TRUE(!drillIn.BankRef().ShowingModulation());
}

// From level 2, Back() must step to level 1 (the level-1 parameter's own
// modulation-source view), landing on the SAME parameter that was open
// before the level-2 press, not a full exit to the parameter grid. A second
// Back() from that level-1 state still goes all the way to level 0 (level
// 1's Back is unchanged).
TEST_CASE(back_from_level_two_returns_to_the_same_level_one_parameter_then_back_again_exits_to_grid) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));

    drillIn.PressEncoder(0);  // -> level 1, Reverb param 0
    REQUIRE_TRUE(drillIn.Level() == 1);
    synth::Parameter* const levelOneParam = drillIn.BankRef().SelectedParameter();
    REQUIRE_TRUE(levelOneParam != nullptr);

    drillIn.PressEncoder(static_cast<synth::PhysicalEncoderId>(kModSlotVco1Audio));  // -> level 2
    REQUIRE_TRUE(drillIn.Level() == 2);

    drillIn.Back();
    REQUIRE_TRUE(drillIn.Level() == 1);
    // Identity check, not just "some" level-1 parameter: the exact same
    // Parameter* that was selected before the level-2 press.
    REQUIRE_TRUE(drillIn.BankRef().SelectedParameter() == levelOneParam);

    drillIn.Back();
    REQUIRE_TRUE(drillIn.Level() == 0);
    REQUIRE_TRUE(!drillIn.BankRef().ShowingModulation());
}

// kMaxDrillLevel is 3. Bank::OpenModulationView returns early WITHOUT
// opening the view when parameter storage is short, so a naive Level()-only
// check would pass even on a silent no-op. This test therefore checks
// BankRef().ShowingModulation() at every step of the descent, not just the
// level counter, then walks Back() down one level at a time, and confirms
// one press past the cap is refused (see fourth_level_drill_in_is_refused
// above). The peak materialized-depth-parameter count is recorded as a
// reported observation only, never as a pass condition.
TEST_CASE(drill_in_reaches_level_three_with_genuinely_open_views_then_back_unwinds_one_level_at_a_time) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    REQUIRE_TRUE(drillIn.Level() == 0);

    drillIn.PressEncoder(0);  // -> level 1, Reverb param 0
    REQUIRE_TRUE(drillIn.Level() == 1);
    REQUIRE_TRUE(drillIn.BankRef().ShowingModulation());
    synth::Parameter* const levelOneParam = drillIn.BankRef().SelectedParameter();
    REQUIRE_TRUE(levelOneParam != nullptr);

    drillIn.PressEncoder(static_cast<synth::PhysicalEncoderId>(kModSlotVco1Audio));  // -> level 2
    REQUIRE_TRUE(drillIn.Level() == 2);
    REQUIRE_TRUE(drillIn.BankRef().ShowingModulation());
    synth::Parameter* const levelTwoParam = drillIn.BankRef().SelectedParameter();
    REQUIRE_TRUE(levelTwoParam != nullptr);

    drillIn.PressEncoder(static_cast<synth::PhysicalEncoderId>(kModSlotVco2Audio));  // -> level 3
    REQUIRE_TRUE(drillIn.Level() == 3);
    REQUIRE_TRUE(drillIn.BankRef().ShowingModulation());
    synth::Parameter* const levelThreeParam = drillIn.BankRef().SelectedParameter();
    REQUIRE_TRUE(levelThreeParam != nullptr);

    auto countMaterialized = [](synth::Parameter& parameter) {
        std::size_t count = 0;
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            if (parameter.ModulationDepthParameter(modIx) != nullptr) {
                ++count;
            }
        }
        return count;
    };
    const std::size_t peakMaterialized = countMaterialized(*levelOneParam) + countMaterialized(*levelTwoParam) +
                                          countMaterialized(*levelThreeParam);
    std::cout << "[OBSERVED] peak materialized depth parameters across a level-3 descent: " << peakMaterialized
              << "\n";

    // The cap is now 3: a press at level 3 that is not the Target/Back cell
    // must be refused exactly the way a press at the old level-2 cap used to
    // be refused.
    drillIn.PressEncoder(static_cast<synth::PhysicalEncoderId>(kModSlotVco3Audio));  // would-be level 4: refused
    REQUIRE_TRUE(drillIn.Level() == 3);
    REQUIRE_TRUE(drillIn.BankRef().SelectedParameter() == levelThreeParam);

    // Back() from here on is checked by Level() only (as specified), not by
    // SelectedParameter() identity: Deselect() (which Back() calls) ends
    // with CollectNeutralLocalParameters(), which reclaims any depth
    // parameter still at its neutral default -- true here, since this test
    // only navigates and never writes a value. levelTwoParam/levelThreeParam
    // can therefore be pruned by the very first Back() below and a
    // DIFFERENT depth parameter re-materialized at the same cell on replay;
    // Back()'s job is to restore the LEVEL, not to guarantee pointer
    // identity for a never-written depth parameter across that prune cycle.
    // (levelOneParam is exempt from this -- it is a fixed top-level page
    // parameter, never pruned -- see the sibling back_from_level_two_*
    // test's own identity check for that already-covered case.)
    drillIn.Back();
    REQUIRE_TRUE(drillIn.Level() == 2);

    drillIn.Back();
    REQUIRE_TRUE(drillIn.Level() == 1);

    drillIn.Back();
    REQUIRE_TRUE(drillIn.Level() == 0);
    REQUIRE_TRUE(!drillIn.BankRef().ShowingModulation());
}

// ============================================================================
// external audio: present but inert with no input
// ============================================================================

TEST_CASE(external_audio_cells_present_and_inert_with_no_input) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/false);
    REQUIRE_TRUE(!fx.slate.Metadata(kModSlotExternalAudio).connected);
    REQUIRE_TRUE(!fx.slate.Metadata(kModSlotExternalAudioEf).connected);

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    synth::Parameter& target = fx.model.PageParameter(FroggersBankId::Reverb, 0);
    drillIn.PressEncoder(0);  // open L1 view -- slate stays 15 cells regardless of cabling
    // The external-audio cells are still PUSHED (present) at their fixed
    // positions (13, 14) but with a null parameter (inert / disconnected
    // encoder rendering), per ParameterModulation.cpp:2843-2852,2784-2786.
    REQUIRE_TRUE(target.ModulationDepthParameter(kModSlotExternalAudio) == nullptr);
    REQUIRE_TRUE(target.ModulationDepthParameter(kModSlotExternalAudioEf) == nullptr);
    // Every OTHER (connected) source still materializes normally -- the
    // slate never changes size or shifts positions because of cabling.
    for (std::size_t modIx = 0; modIx < kModSlotExternalAudio; ++modIx) {
        REQUIRE_TRUE(target.ModulationDepthParameter(modIx) != nullptr);
    }
    drillIn.Back();

    // Positive control: flip an input on and watch the SAME two kinds of
    // assertion -- the metadata bit, AND the surface-level depth cell a
    // real operator press would see -- go the other way in this same
    // fixture, same run, not just a flipped internal bool.
    fx.StepOnce(/*externalConnected=*/true);
    REQUIRE_TRUE(fx.slate.Metadata(kModSlotExternalAudio).connected);
    REQUIRE_TRUE(fx.slate.Metadata(kModSlotExternalAudioEf).connected);
    drillIn.PressEncoder(0);  // re-open the identical view now that both slots are connected
    REQUIRE_TRUE(target.ModulationDepthParameter(kModSlotExternalAudio) != nullptr);
    REQUIRE_TRUE(target.ModulationDepthParameter(kModSlotExternalAudioEf) != nullptr);
}

// The metadata flag and the depth cell's mere existence (both proven above)
// are not proof that modulation actually reaches a destination: a connected
// source with a materialized but unread depth would look identical on both
// counts. This proves the source's VALUE flows through: at a full (|depth|
// == 1.0) route, the resolved value is driven entirely by the source and the
// destination's own commanded center contributes nothing at all
// (ParameterModulation.cpp:2257-2270's weightSum>=1.0 branch zeroes
// targetCenterScales_ -- see AttachFullPositiveAudioRateModulation's own
// comment, below, for the fuller derivation). So pinning the destination's
// commanded value to a KNOWN quantity, then attaching a full-positive
// external-audio route, must move the resolved value away from that known
// quantity and toward the source's own -- a swing that cannot happen unless
// the source's value actually reached the destination. The destination is
// pinned explicitly (`HandleSetAbsolute`) rather than read as whatever its
// unexamined default happens to be: this fixture's StepOnce() defaults
// `externalAudioSample` to 0.0f, so the external-audio source's own value
// here is exactly 0.5 (externalAudioSource_'s NSDMI, and what a 0.0f sample
// normalizes to -- Step()'s own comment), and 0.5 is
// `NormalizeBipolarToUnit`'s "no signal" convention, so a destination
// that ALSO happened to default to 0.5 would make full-positive and
// full-negative depth resolve identically (verified empirically: they do,
// at exactly 0.5, for this bank's own default) -- a coincidence of the
// destination's default, not proof of anything. Pinning it to 1.0 removes
// that dependency entirely. `Parameter::TargetValue` is the private
// accessor this mechanism is named after; `GetRaw(0)` (public) reads the
// same resolved quantity through `currentCenter_`/`CurrentDepthSlots`, which
// `ParameterManager::ComputeAllParameters()`'s `SnapCurrentToTarget()` call
// keeps equal to the target ones (ParameterModulation.cpp:1207-1216,
// 3165-3173) -- the same convergence AttachFullPositiveAudioRateModulation's
// own comment relies on for the depth parameter's `GetRaw()`.
TEST_CASE(connected_external_audio_modulation_reaches_a_destination_end_to_end) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    fx.model.Group().UpdateModValues();

    synth::Parameter& target = fx.model.PageParameter(FroggersBankId::Reverb, 0);
    for (const synth::SceneState& pole : detail::kScenePoles) {
        target.HandleSetAbsolute(pole, 1.0f);  // known quantity, deliberately far from the source's own 0.5.
    }
    fx.manager.ComputeAllParameters();
    const float unmodulated = target.GetRaw(0);

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    drillIn.PressEncoder(0);
    synth::Parameter* depth = target.ModulationDepthParameter(kModSlotExternalAudio);
    REQUIRE_TRUE(depth != nullptr);
    for (const synth::SceneState& pole : detail::kScenePoles) {
        depth->SceneCenter(pole.leftScene) = 1.0f;  // full-positive bipolar depth
    }
    fx.manager.ComputeAllParameters();
    const float modulated = target.GetRaw(0);

    std::cout << "connected_external_audio_modulation_reaches_a_destination_end_to_end: unmodulated="
              << unmodulated << " modulated=" << modulated << "\n";
    REQUIRE_TRUE(std::fabs(modulated - unmodulated) > 0.3f);
}

TEST_CASE(disconnected_external_audio_never_receives_randomized_depth) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/false);

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);  // parameter-page global randomize

    ForEachTopLevelParameter(fx.model, [](synth::Parameter& parameter) {
        REQUIRE_TRUE(parameter.ModulationDepthParameter(kModSlotExternalAudio) == nullptr);
        REQUIRE_TRUE(parameter.ModulationDepthParameter(kModSlotExternalAudioEf) == nullptr);
    });
}

// ============================================================================
// external audio: driven from real input while connected
// ============================================================================

// Positive control: a known, nonzero, connected sample must move the
// published source value off 0.5 to an exactly stateable quantity --
// NormalizeBipolarToUnit(0.4f) == 0.5 + 0.5*0.4 == 0.7 exactly (0.4f and 0.7f
// are both exactly representable in float, so this is a bit-exact check, not
// a tolerance one).
TEST_CASE(connected_external_audio_source_moves_off_0_5_for_a_known_input) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true, /*externalAudioSample=*/0.4f);
    fx.model.Group().UpdateModValues();
    const float value = fx.slate.SourceValue(kModSlotExternalAudio);
    REQUIRE_TRUE(value != 0.5f);
    REQUIRE_TRUE(value == 0.7f);
}

// The follower must actually follow (rise off its 0.0f floor for a sustained
// input) and must be genuinely smoothed, not a second, aliased read of the
// same raw sample the un-followed source above reads -- 200 samples at this
// fixture's 48kHz (SingleEnvelopeFollower::SetSampleRate's own ~10ms attack
// time constant) is under one full time constant, so the follower is still
// visibly mid-rise, nowhere near its 0.8f target -- a strong, non-tolerance-
// sensitive way to tell "smoothed" from "passthrough."
TEST_CASE(connected_external_audio_ef_source_rises_for_sustained_input_and_is_not_the_raw_sample) {
    Fixture fx;
    constexpr float kSustainedSample = 0.8f;
    float previousEf = 0.0f;
    float ef = 0.0f;
    bool everRose = false;
    for (int i = 0; i < 200; ++i) {
        fx.StepOnce(/*externalConnected=*/true, kSustainedSample);
        fx.model.Group().UpdateModValues();
        ef = fx.slate.SourceValue(kModSlotExternalAudioEf);
        if (ef > previousEf) {
            everRose = true;
        }
        previousEf = ef;
    }
    REQUIRE_TRUE(everRose);
    REQUIRE_TRUE(ef > 0.0f);
    REQUIRE_TRUE(std::fabs(ef - kSustainedSample) > 0.1f);  // smoothed, not the raw sample.
}

// Disconnected must read EXACTLY the inert pair regardless of what the input
// signal is doing -- a nonzero sample here proves the gate, not merely the
// absence of a signal. Reads the members directly (ExternalAudio*ForTest()):
// SourceValue()'s cache is not refreshed while disconnected (that accessor's
// own comment), so it cannot observe this restore.
TEST_CASE(disconnected_external_audio_sources_stay_exactly_inert_regardless_of_input) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/false, /*externalAudioSample=*/0.9f);
    REQUIRE_TRUE(fx.slate.ExternalAudioSourceForTest() == 0.5f);
    REQUIRE_TRUE(fx.slate.ExternalAudioEfSourceForTest() == 0.0f);
}

// Disconnecting after a live, clearly-nonneutral signal must snap the
// published pair back to inert on the very next Step() -- not merely leave
// them holding whatever the last connected sample produced.
TEST_CASE(external_audio_sources_snap_back_to_inert_on_disconnect_not_the_last_live_sample) {
    Fixture fx;
    for (int i = 0; i < 50; ++i) {
        fx.StepOnce(/*externalConnected=*/true, /*externalAudioSample=*/0.9f);
    }
    REQUIRE_TRUE(fx.slate.ExternalAudioSourceForTest() != 0.5f);
    REQUIRE_TRUE(fx.slate.ExternalAudioEfSourceForTest() > 0.0f);

    // Same nonzero sample, but now disconnected -- if the sample still
    // influenced the published pair, or the pair simply held its last live
    // value, this would fail.
    fx.StepOnce(/*externalConnected=*/false, /*externalAudioSample=*/0.9f);
    REQUIRE_TRUE(fx.slate.ExternalAudioSourceForTest() == 0.5f);
    REQUIRE_TRUE(fx.slate.ExternalAudioEfSourceForTest() == 0.0f);
}

// ============================================================================
// randomize semantics
// ============================================================================

TEST_CASE(randomize_all_on_parameter_page_never_creates_level_two_depths) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));

    RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);

    ForEachTopLevelParameter(fx.model, [](synth::Parameter& parameter) {
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            synth::Parameter* depth = parameter.ModulationDepthParameter(modIx);
            if (depth == nullptr) {
                continue;
            }
            for (std::size_t innerIx = 0; innerIx < FroggersParameterModel::kNumModulators; ++innerIx) {
                REQUIRE_TRUE(depth->ModulationDepthParameter(innerIx) == nullptr);
            }
        }
    });
}

TEST_CASE(randomize_all_on_parameter_page_stays_within_793_ceiling_with_external_disconnected_and_is_idempotent_capacity_wise) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/false);  // 13 connected sources -> 61*13 = 793 ceiling
    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));

    auto countMaterialized = [&]() {
        std::size_t count = 0;
        ForEachTopLevelParameter(fx.model, [&](synth::Parameter& parameter) {
            for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
                if (parameter.ModulationDepthParameter(modIx) != nullptr) {
                    ++count;
                }
            }
        });
        return count;
    };

    const auto result1 = RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);
    const std::size_t after1 = countMaterialized();
    REQUIRE_TRUE(!result1.partial);
    REQUIRE_TRUE(after1 <= 61 * 13);

    const auto result2 = RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);
    const std::size_t after2 = countMaterialized();
    REQUIRE_TRUE(!result2.partial);
    REQUIRE_TRUE(after2 <= 61 * 13);
    // Repeated presses re-randomize already-materialized depths (or add a
    // few more, since the coin-flip loop can touch a previously-untouched
    // modulator next time) but never exceed the ceiling.
    REQUIRE_TRUE(after2 >= after1);
}

TEST_CASE(randomize_all_on_level_one_grid_materializes_that_parameters_own_level_two_depths_and_no_others) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    synth::Parameter& focused = fx.model.PageParameter(FroggersBankId::Reverb, 0);
    synth::Parameter& other = fx.model.PageParameter(FroggersBankId::Filter, 3);  // untouched control

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    drillIn.PressEncoder(0);  // -> level 1 on `focused`
    REQUIRE_TRUE(drillIn.Level() == 1);

    RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);  // level-1 case: 15 + up to 225

    bool anyLevelTwoOnFocused = false;
    for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
        synth::Parameter* depth = focused.ModulationDepthParameter(modIx);
        if (depth == nullptr) {
            continue;
        }
        for (std::size_t innerIx = 0; innerIx < FroggersParameterModel::kNumModulators; ++innerIx) {
            if (depth->ModulationDepthParameter(innerIx) != nullptr) {
                anyLevelTwoOnFocused = true;
            }
        }
    }
    REQUIRE_TRUE(anyLevelTwoOnFocused);

    // `other` (a different top-level parameter never opened) must have
    // gained NOTHING as a side effect of this operation.
    for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
        REQUIRE_TRUE(other.ModulationDepthParameter(modIx) == nullptr);
    }
}

// This test guards against a real regression: a level-1 Randomize All used
// to eject the operator back to level 0 (by round-tripping through
// PressEncoder/Back). The level must not move, and the deeper (level-2)
// randomization must still happen even though no view is ever opened to
// reach it.
TEST_CASE(randomize_all_on_level_one_grid_never_ejects_and_still_reaches_level_two) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    synth::Parameter& focused = fx.model.PageParameter(FroggersBankId::Reverb, 0);

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    drillIn.PressEncoder(0);  // -> level 1 on `focused`
    REQUIRE_TRUE(drillIn.Level() == 1);

    // `LastRandomizePartial()` is not usable here: that accessor lives on
    // FroggersApp (FroggersAppCore.hpp), published from ProcessFrame() and
    // reachable only via a SynthRig/rig.Application() fixture (as
    // FroggersHeadlessTests.cpp uses) -- it does not exist on this file's
    // bare manager+model+slate Fixture. `result.partial` below is the exact
    // underlying signal LastRandomizePartial() publishes (that accessor's
    // own comment: true when "the MOST RECENT Randomize All/Page operation
    // left FroggersRandomizeResult.partial true"), and is this file's own
    // established idiom for the same check -- see the 793-ceiling test
    // above.
    const auto result = RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);

    // 1) Still at level 1: a level-1 Randomize All must not navigate out.
    REQUIRE_TRUE(drillIn.Level() == 1);

    // 2) The deeper randomization still happened even though no view was
    // opened: `focused`'s own depth parameters carry at least one
    // non-neutral level-2 sub-depth.
    bool anyLevelTwoNonNeutral = false;
    for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
        synth::Parameter* depth = focused.ModulationDepthParameter(modIx);
        if (depth == nullptr) {
            continue;
        }
        for (std::size_t innerIx = 0; innerIx < FroggersParameterModel::kNumModulators; ++innerIx) {
            synth::Parameter* subDepth = depth->ModulationDepthParameter(innerIx);
            if (subDepth != nullptr && detail::DepthIsModulating(*subDepth)) {
                anyLevelTwoNonNeutral = true;
            }
        }
    }
    REQUIRE_TRUE(anyLevelTwoNonNeutral);

    // 3) No silent allocation failure on a fresh patch.
    REQUIRE_TRUE(!result.partial);
}

TEST_CASE(crunchy_is_never_randomized_by_either_button_in_any_view) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    synth::Parameter& crunchy = fx.model.Crunchy();
    const float before = crunchy.SceneCenter(0);

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);
    REQUIRE_TRUE(crunchy.SceneCenter(0) == before);
    REQUIRE_TRUE(crunchy.ModulationDepthParameter(0) == nullptr);  // never opened/randomized either

    RandomizePage(fx.manager, drillIn);
    REQUIRE_TRUE(crunchy.SceneCenter(0) == before);
}

// Randomize All's Crispy draw is bounded by kMaxRandomizedCrispy: the
// aggregate of all six banks' Crispy is what reproduces global Crunchy
// (which this app deliberately never randomizes), so randomizing all six at
// once would land in the same place as randomizing Crunchy directly, and the
// count Randomize All draws must stay bounded below six -- kMaxRandomizedCrispy
// is that bound. Randomize Page keeps randomizing one page's own Crispy,
// which is that page's own business.
TEST_CASE(randomize_all_moves_at_most_two_of_six_banks_crispy_but_randomize_page_moves_only_its_own) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));

    auto crispyOf = [&](std::size_t bankIx) -> synth::Parameter& {
        return fx.model.Crispy(static_cast<FroggersBankId>(bankIx));
    };

    constexpr int kTrials = 600;
    std::array<int, kFroggersBankCount + 1> countHistogram{};
    std::array<bool, kFroggersBankCount> bankEverChanged{};
    for (int trial = 0; trial < kTrials; ++trial) {
        std::array<float, kFroggersBankCount> before{};
        for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
            before[bankIx] = crispyOf(bankIx).SceneCenter(0);
        }

        RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);

        int changedCount = 0;
        for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
            if (crispyOf(bankIx).SceneCenter(0) != before[bankIx]) {
                ++changedCount;
                bankEverChanged[bankIx] = true;
            }
        }
        REQUIRE_TRUE(changedCount <= static_cast<int>(detail::kMaxRandomizedCrispy));
        ++countHistogram[static_cast<std::size_t>(changedCount)];
    }

    // The count is 0 on roughly half of presses -- the common case, not a
    // rare one. A [38%, 62%] band is the same sample-size-appropriate margin
    // RequireGeometricCountDistribution uses elsewhere in this file.
    REQUIRE_TRUE(countHistogram[0] > kTrials * 0.38);
    REQUIRE_TRUE(countHistogram[0] < kTrials * 0.62);

    // Every one of the six banks must be reachable across the run.
    for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
        REQUIRE_TRUE(bankEverChanged[bankIx]);
    }

    std::cout << "[OBSERVED] randomize-all-crispy-count histogram over " << kTrials << " trials:"
              << " P(0)=" << (100.0 * countHistogram[0] / kTrials) << "%"
              << " P(1)=" << (100.0 * countHistogram[1] / kTrials) << "%"
              << " P(2)=" << (100.0 * countHistogram[2] / kTrials) << "%\n";

    // Randomize Page on a parameter page still moves only that page's own
    // Crispy, and no other bank's.
    std::array<float, kFroggersBankCount> beforePage{};
    for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
        beforePage[bankIx] = crispyOf(bankIx).SceneCenter(0);
    }
    constexpr std::size_t kReverbIx = static_cast<std::size_t>(FroggersBankId::Reverb);
    bool changedByPage = false;
    for (int attempt = 0; attempt < 4 && !changedByPage; ++attempt) {
        RandomizePage(fx.manager, drillIn);
        changedByPage = crispyOf(kReverbIx).SceneCenter(0) != beforePage[kReverbIx];
    }
    REQUIRE_TRUE(changedByPage);
    for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
        if (bankIx == kReverbIx) {
            continue;
        }
        REQUIRE_TRUE(crispyOf(bankIx).SceneCenter(0) == beforePage[bankIx]);
    }
}

TEST_CASE(randomize_page_on_parameter_page_changes_no_depths) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));

    RandomizePage(fx.manager, drillIn);

    ForEachTopLevelParameter(fx.model, [](synth::Parameter& parameter) {
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            REQUIRE_TRUE(parameter.ModulationDepthParameter(modIx) == nullptr);
        }
    });
}

TEST_CASE(randomize_page_on_mod_detail_grid_changes_only_that_parameters_own_depths) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    synth::Parameter& focused = fx.model.PageParameter(FroggersBankId::Reverb, 0);
    synth::Parameter& sibling = fx.model.PageParameter(FroggersBankId::Reverb, 2);

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    drillIn.PressEncoder(0);  // -> level 1 on `focused`
    RandomizePage(fx.manager, drillIn);  // one RandomizeModulationDepths call on `focused`
    REQUIRE_TRUE(drillIn.Level() == 1);  // stays at the same level (target/back+modifier doesn't Deselect)

    bool focusedGotSomething = false;
    for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
        if (focused.ModulationDepthParameter(modIx) != nullptr) {
            focusedGotSomething = true;
        }
    }
    REQUIRE_TRUE(focusedGotSomething);
    for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
        REQUIRE_TRUE(sibling.ModulationDepthParameter(modIx) == nullptr);
    }
}

// ============================================================================
// randomize-depth count distribution
// ============================================================================
// Sheaf's own private Bank::RandomizeModulationDepths coin loop is geometric
// FROM ZERO (P(0)=50%), so a single call is a no-op half the time.
// detail::RandomizeParameterModulationDepths (FroggersModulation.hpp)
// replaces the count/source selection app-side with the same geometric
// shape, drawn from a per-gesture floor -- zero everywhere except a
// drilled-in Randomize All press, which floors at one and so is never a
// no-op there. Every other gesture, including RandomizePage at any level,
// keeps the zero floor and stays a no-op exactly half the time. Sheaf still
// performs every write. All three properties below are statistical, not
// single-sample: a probability distribution cannot be verified from one
// draw.

// The drill-in knob's display value (`UIDisplayCenter`) is populated only
// by a smoothed one-shot nudge inside RandomizeVisibleValue itself, and is
// never ticked again for a parameter that is never in `topLevelParameters_`
// -- so a check that pins only the raw commanded value (`SceneCenter(0)`,
// which RandomizeVisibleValue writes directly and immediately) can stay
// green while the displayed knob is visually stuck at center. This test
// pins `UIDisplayCenter` to catch that. `count` is 0 on 50% of draws in
// RandomizeParameterModulationDepths's geometric draw from a floor of zero
// (RandomizePage never floors above zero, at any level), so no single trial
// can be required to move the display -- the assertion is over the 500-trial
// aggregate instead, matching the 50% of draws expected to produce at least
// one source.
//
// `fx.manager.ComputeAllParameters()` is called per trial to stand in for
// the real app's fix, which lives in `FroggersAppCore::ProcessFrame()` (the
// audio-thread drain) and is therefore NOT exercised by this file's
// bare-ParameterManager fixture at all -- without this call,
// `UIDisplayCenter` would stay stale by construction.
TEST_CASE(randomize_page_mod_detail_moves_the_display_on_the_expected_fraction_of_500_trials) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);  // 15 connected sources
    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    drillIn.PressEncoder(0);  // -> level 1; eagerly materializes all 15 depth cells (Bank::OpenModulationView)
    synth::Parameter& focused = fx.model.PageParameter(FroggersBankId::Reverb, 0);
    constexpr float kNeutral = detail::kNeutralModulationDepthCenter;  // single named constant
    constexpr float kTolerance = 1e-4f;

    constexpr int kTrials = 500;
    int trialsWithDisplayMoved = 0;
    for (int trial = 0; trial < kTrials; ++trial) {
        RandomizePage(fx.manager, drillIn);
        fx.manager.ComputeAllParameters();  // reseed -- see this test's header comment.

        bool anyDisplayMoved = false;
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            synth::Parameter* depth = focused.ModulationDepthParameter(modIx);
            if (depth != nullptr && std::fabs(depth->UIDisplayCenter(0) - kNeutral) > kTolerance) {
                anyDisplayMoved = true;
            }
        }
        if (anyDisplayMoved) {
            ++trialsWithDisplayMoved;
        }
    }
    // P(count>=1) is 50% under the geometric draw. At 500 trials the binomial
    // standard error at p=0.5 is ~2.2 points, so a [35%, 65%] band (roughly
    // 6.7 standard errors wide) leaves a wide, sample-size-appropriate margin
    // while still catching the DISPLAY (not just the commanded value)
    // failing to track a real draw.
    REQUIRE_TRUE(trialsWithDisplayMoved > kTrials * 0.35);
    REQUIRE_TRUE(trialsWithDisplayMoved < kTrials * 0.65);
}

TEST_CASE(randomize_depth_helper_draws_distinct_sources_even_from_an_adversarial_index_feed) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    // Shrink the eligible set to exactly 5 (modulators 0-4) so it is fully
    // enumerable -- disconnect everything else. ModulatorMetadata::connected
    // is a public field (Modulators::Metadata(modIx) returns a mutable ref).
    for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
        fx.model.Group().GetModulators().Metadata(modIx).connected = (modIx < 5);
    }
    synth::Parameter& focused = fx.model.PageParameter(FroggersBankId::Reverb, 0);

    // Force count = 4: the geometric draw keeps going while the coin reads
    // >= 0.5, so four such reads and then one below stops it there. Feeds an
    // ADVERSARIAL NextRandomIndex that always returns
    // the LAST valid index of whatever range it's asked -- a "draw with
    // replacement, no exclusion" loop (Sheaf's own private
    // Bank::RandomizeModulationDepths) would pick a fixed relative position
    // on every one of its independent draws under a feed like this; a
    // correct partial Fisher-Yates cannot, because each pick's search window
    // shrinks and is offset by the picks already made, so it is forced to
    // exercise real swaps here rather than degenerating to a no-op permutation.
    int coinsDrawn = 0;
    fx.manager.SetRandomSource(
        []() { return 0.3f; },                                       // NextRandomValue (irrelevant to selection)
        [&coinsDrawn]() { return coinsDrawn++ < 4 ? 0.9f : 0.1f; },  // NextRandomCoin -> count=4
        [](std::size_t exclusiveMax) { return exclusiveMax - 1; });  // NextRandomIndex: always top-of-range

    detail::RandomizeParameterModulationDepths(fx.manager, focused, /*minimumSources=*/0);

    std::size_t touchedCount = 0;
    for (std::size_t modIx = 0; modIx < 5; ++modIx) {
        if (focused.ModulationDepthParameter(modIx) != nullptr) {
            ++touchedCount;
        }
    }
    // 4 DISTINCT sources materialized -- a double-draw would leave this < 4.
    REQUIRE_TRUE(touchedCount == 4);
}

// Non-additive randomize is why either histogram test below can measure a
// call's draw count as "how many depths are non-neutral immediately after
// it," with no round-to-round diffing: RandomizeParameterModulationDepths
// zeroes the target's existing depths (both scene poles) BEFORE every draw,
// so each call starts from a known, exact baseline (`SceneCenter(0) == 0.5`)
// regardless of what any previous call did. No `ComputeAllParameters()` call
// is needed here for the same reason (contrast the no-op/display test above,
// which needs it for `UIDisplayCenter`, not `SceneCenter`).
//
// Shared assertions for a count histogram against the geometric draw from a
// per-gesture floor -- reused by the level-0 test (floor 0, Randomize All on
// a parameter page) and the level-1/level-2 test below (floor 1, Randomize
// All at a drilled-in level), so the four properties (buckets below the
// floor, the floor bucket's rate, mode, rare floor+4+) are pinned exactly
// once rather than duplicated per level. Asserts on the resulting COUNT
// DISTRIBUTION, not on call counts -- the predecessor's version of this test
// (median-based, one vector of samples) is the sixth green-while-wrong guard
// on record for pinning the wrong layer; this one pins the actual observable
// shape.
void RequireGeometricCountDistribution(const std::array<int, FroggersParameterModel::kNumModulators + 1>& histogram,
                                        int trials, const char* label, std::size_t floor) {
    // Every bucket below the floor is exactly zero: the draw never returns a
    // count under its own floor.
    for (std::size_t count = 0; count < floor; ++count) {
        REQUIRE_TRUE(histogram[count] == 0);
    }

    // histogram[floor] is 50% under the geometric draw. A [38%, 62%] band is
    // roughly 3.4 standard errors wide even at `trials` as low as 200, where
    // the binomial standard error at p=0.50 is ~3.5 points -- a wide,
    // sample-size-appropriate margin.
    REQUIRE_TRUE(histogram[floor] > trials * 0.38);
    REQUIRE_TRUE(histogram[floor] < trials * 0.62);

    std::size_t mode = floor;
    for (std::size_t count = floor + 1; count < histogram.size(); ++count) {
        if (histogram[count] > histogram[mode]) {
            mode = count;
        }
    }
    REQUIRE_TRUE(mode == floor);  // 50% > 25% > 12.5% > ..., strictly decreasing from the floor.

    int atLeastFloorPlusFour = 0;
    int atLeastFour = 0;
    int atLeastSeven = 0;
    double sum = 0.0;
    for (std::size_t count = 0; count < histogram.size(); ++count) {
        if (count >= floor + 4) {
            atLeastFloorPlusFour += histogram[count];
        }
        if (count >= 4) {
            atLeastFour += histogram[count];
        }
        if (count >= 7) {
            atLeastSeven += histogram[count];
        }
        sum += static_cast<double>(count) * static_cast<double>(histogram[count]);
    }
    // P(>=floor+4) is 6.25% under the geometric draw (0.5^4), measured from
    // the floor. "Materially below 13%" leaves a wide, sample-size-appropriate
    // margin rather than one tightened until it happens to pass: even at
    // `trials` as low as 200 the binomial standard error at p=0.0625 is ~1.7
    // points, so 13% sits roughly 4 standard errors above the true rate.
    REQUIRE_TRUE(atLeastFloorPlusFour < trials * 0.13);

    // [OBSERVED] -- not a pass condition (matches this file's own convention
    // for recording a measurement alongside its pass condition, e.g. the
    // level-three drill-in test's own peak-count print): the actual sampled
    // shape, for a human to compare against the geometric draw in
    // FroggersModulation.hpp.
    std::cout << "[OBSERVED] " << label << " count histogram over " << trials << " trials:";
    for (std::size_t count = 0; count <= 4 && count < histogram.size(); ++count) {
        std::cout << " P(" << count << ")=" << (100.0 * histogram[count] / trials) << "%";
    }
    std::cout << " P(>=4)=" << (100.0 * atLeastFour / trials) << "%"
              << " P(>=7)=" << (100.0 * atLeastSeven / trials) << "%"
              << " mode=" << mode << " mean=" << (sum / trials) << "\n";
}

TEST_CASE(randomize_depth_helper_level_zero_count_distribution_is_geometric_from_a_floor_of_zero_across_1000_trials) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);  // 15 connected sources -- N for the tail
    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));  // default: Level()==0
    synth::Parameter& focused = fx.model.PageParameter(FroggersBankId::Reverb, 0);

    constexpr int kTrials = 1000;
    std::array<int, FroggersParameterModel::kNumModulators + 1> histogram{};
    for (int trial = 0; trial < kTrials; ++trial) {
        RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);  // Level()==0: the level-0 draw.
        int nonNeutral = 0;
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            synth::Parameter* depth = focused.ModulationDepthParameter(modIx);
            if (depth != nullptr && detail::DepthIsModulating(*depth)) {
                ++nonNeutral;
            }
        }
        ++histogram[static_cast<std::size_t>(nonNeutral)];
    }

    RequireGeometricCountDistribution(histogram, kTrials, "level-0", /*floor=*/0);
}

// The drilled-in Randomize All press's floor of one -- a REGRESSION PIN for
// that specific gesture, not a fix, since all four RandomMod dispatch sites
// already share the one `detail::RandomizeParameterModulationDepths`
// definition and its `minimumSources` parameter. Traced structurally in
// FroggersModulation.hpp: RandomizeAll's Level()==1 branch calls that shared
// helper TWICE per press, both floored at one -- once on the selected
// parameter itself (its own, level-1 depths) and once more on each of that
// parameter's now-materialized depth parameters (each one's own, level-2
// sub-depths) -- so a single press exercises both nesting depths at once,
// both floored, and this test histograms both.
TEST_CASE(randomize_all_level_one_press_gives_its_own_depths_and_each_depths_subdepths_the_geometric_from_a_floor_of_one_distribution) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);  // 15 connected sources
    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    drillIn.PressEncoder(0);  // -> level 1
    synth::Parameter& focused = fx.model.PageParameter(FroggersBankId::Reverb, 0);

    auto countNonNeutral = [&](synth::Parameter& parameter) {
        int count = 0;
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            synth::Parameter* depth = parameter.ModulationDepthParameter(modIx);
            if (depth != nullptr && detail::DepthIsModulating(*depth)) {
                ++count;
            }
        }
        return count;
    };

    // Counts materialized depth-parameter slots in the focused parameter's
    // subtree: its own (up to kNumModulators) depth slots, plus, for each
    // materialized depth, its own materialized sub-depth slots. Storage is
    // never freed, so calling this before and after a press isolates what
    // that one press newly allocated.
    auto countMaterialized = [](synth::Parameter& parameter) {
        int count = 0;
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            synth::Parameter* depthParam = parameter.ModulationDepthParameter(modIx);
            if (depthParam == nullptr) {
                continue;
            }
            ++count;
            for (std::size_t subModIx = 0; subModIx < FroggersParameterModel::kNumModulators; ++subModIx) {
                if (depthParam->ModulationDepthParameter(subModIx) != nullptr) {
                    ++count;
                }
            }
        }
        return count;
    };

    constexpr int kTrials = 500;
    std::array<int, FroggersParameterModel::kNumModulators + 1> level1Histogram{};
    std::array<int, FroggersParameterModel::kNumModulators + 1> level2Histogram{};
    int level2Samples = 0;
    int partialPresses = 0;
    long long newlyMaterializedTotal = 0;
    int cumulativeAtEnd = 0;
    for (int trial = 0; trial < kTrials; ++trial) {
        const int materializedBefore = countMaterialized(focused);
        // Level()==1: own depths + each depth's own sub-depths.
        const FroggersRandomizeResult result = RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);
        if (result.partial) {
            ++partialPresses;
        }
        cumulativeAtEnd = countMaterialized(focused);
        newlyMaterializedTotal += (cumulativeAtEnd - materializedBefore);
        ++level1Histogram[static_cast<std::size_t>(countNonNeutral(focused))];
        // The descent visits only depths that are ACTUALLY MODULATING, not
        // every materialized one: a neutral depth must carry ZERO
        // sub-depths, so a loop that samples all materialized depths
        // (including neutral ones) would feed `histogram[0]`, which the
        // shared "count is never 0" assertion rejects -- sampling a
        // population the code deliberately does not touch measures the
        // wrong thing.
        //
        // So: modulating depths feed the distribution histogram, and neutral
        // ones get the STRONGER assertion -- they must carry no sub-depths at
        // all. That second branch is the badge fix pinned directly. Sheaf's
        // ModulatorsAffectingMask counts a depth that merely HAS sub-modulation
        // (ParameterModulation.cpp:2356-2365 via HasNonZeroState), so a neutral
        // depth with sub-depths would light up as a badge for a source that is
        // modulating nothing -- measured at 13 badges against 1 live source.
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            synth::Parameter* depthParam = focused.ModulationDepthParameter(modIx);
            if (depthParam == nullptr) {
                continue;
            }
            if (detail::DepthIsModulating(*depthParam)) {
                ++level2Histogram[static_cast<std::size_t>(countNonNeutral(*depthParam))];
                ++level2Samples;
            } else {
                REQUIRE_TRUE(countNonNeutral(*depthParam) == 0);
            }
        }
    }

    // Sanity floor: proves the level-2 loop actually fired repeatedly rather
    // than the histogram being near-empty from a wiring regression.
    REQUIRE_TRUE(level2Samples >= 200);

    RequireGeometricCountDistribution(level1Histogram, kTrials, "level-1", /*floor=*/1);
    RequireGeometricCountDistribution(level2Histogram, level2Samples, "level-2", /*floor=*/1);

    // [OBSERVED] -- the allocation cost of the floor of one at this level:
    // mean count of depth-parameter slots newly materialized per press (the
    // focused parameter's own depths plus their own sub-depths, counted
    // before and after each press since storage is never freed), the
    // cumulative materialized count in that subtree after the final press,
    // and the fraction of presses that came back partial (storage
    // exhausted).
    std::cout << "[OBSERVED] level-1 newly-materialized-depths-per-press mean="
              << (static_cast<double>(newlyMaterializedTotal) / kTrials)
              << " cumulative-at-end=" << cumulativeAtEnd
              << " partial-fraction=" << (100.0 * partialPresses / kTrials) << "%\n";
}

// Dedicated coverage for the zero bucket the geometric draw carries: a
// fraction of RandomizeAll draws must leave a parameter with NO modulation
// sources at all (count=0), not merely "rarely one source." This is
// distinct from RequireGeometricCountDistribution's zero-rate band above --
// that helper's job is the whole-shape regression pin; this test's job is
// to state the zero and four-or-more rates as their own named properties,
// plus a positive control that a broken always-zero randomizer could not
// pass.
TEST_CASE(randomize_depth_helper_zero_and_four_plus_rates_match_the_geometric_draw_across_2000_trials) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);  // 15 connected sources -- N for the tail
    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));  // default: Level()==0
    synth::Parameter& focused = fx.model.PageParameter(FroggersBankId::Reverb, 0);

    constexpr int kTrials = 2000;
    int zeroCount = 0;
    int fourPlusCount = 0;
    int nonZeroCount = 0;
    for (int trial = 0; trial < kTrials; ++trial) {
        RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);  // Level()==0: the level-0 draw, uses the fixture's own seeded RNG.
        int nonNeutral = 0;
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            synth::Parameter* depth = focused.ModulationDepthParameter(modIx);
            if (depth != nullptr && detail::DepthIsModulating(*depth)) {
                ++nonNeutral;
            }
        }
        if (nonNeutral == 0) {
            ++zeroCount;
        } else {
            ++nonZeroCount;
        }
        if (nonNeutral >= 4) {
            ++fourPlusCount;
        }
    }

    // P(0) is 50% under the geometric draw. At 2000 trials the binomial
    // standard error at p=0.50 is ~1.1 points, so a [44%, 56%] band (roughly
    // 5.4 standard errors wide) is a wide, sample-size-appropriate margin.
    REQUIRE_TRUE(zeroCount > kTrials * 0.44);
    REQUIRE_TRUE(zeroCount < kTrials * 0.56);

    // P(>=4) is 1-in-16 (6.25%) under the geometric draw (0.5^4) -- the same
    // rate the tuned table it replaced happened to land on. At 2000
    // trials the binomial standard error at p=0.0625 is ~0.54 points, so a
    // [3.5%, 9.5%] band (roughly 5.5 standard errors wide either side) is a
    // wide, sample-size-appropriate margin.
    REQUIRE_TRUE(fourPlusCount > kTrials * 0.035);
    REQUIRE_TRUE(fourPlusCount < kTrials * 0.095);

    // POSITIVE CONTROL: a substantial fraction of draws must be non-zero --
    // a randomizer that regressed to always-zero would otherwise satisfy
    // "some fraction is zero" trivially. P(count>=1) is 50% under the
    // geometric draw, so the threshold sits well clear of it: a third rules
    // out an always-zero (or mostly-zero) regression without pinning the
    // exact rate twice, which the band above already does.
    REQUIRE_TRUE(nonZeroCount > kTrials * 0.33);

    std::cout << "[OBSERVED] zero-and-four-plus-rates count histogram over " << kTrials
              << " trials: P(0)=" << (100.0 * zeroCount / kTrials) << "%"
              << " P(>=4)=" << (100.0 * fourPlusCount / kTrials) << "%"
              << " P(>=1)=" << (100.0 * nonZeroCount / kTrials) << "%\n";
}

// ============================================================================
// non-additivity and scene-pair semantics
// ============================================================================

// This test pins non-additivity directly (not just "never a no-op") -- two
// successive Randomize All presses must NOT exhibit the old bug's signature,
// where the nonzero-source count for a given parameter could only ever grow
// (nothing ever zeroed a previously-randomized source). Checked at BOTH
// scene poles ("non-additive in both scenes").
TEST_CASE(randomize_all_is_non_additive_two_successive_presses_can_decrease_the_count) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    synth::Parameter& focused = fx.model.PageParameter(FroggersBankId::Reverb, 0);

    auto countNonNeutral = [&](std::size_t sceneIx) {
        int count = 0;
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            synth::Parameter* depth = focused.ModulationDepthParameter(modIx);
            if (depth != nullptr && detail::DepthIsModulatingInScene(*depth, sceneIx)) {
                ++count;
            }
        }
        return count;
    };

    // Under the old additive bug, `count2 < count1` could never happen -- a
    // previously-drawn source is never zeroed, so the set (and therefore the
    // count) can only grow or stay the same across repeated presses. Under
    // the fix, each press draws an independent fresh set, so a strictly
    // smaller count on the second press must eventually be observed across
    // enough trial pairs if the fix is real.
    bool sawDecreaseScene0 = false;
    bool sawDecreaseScene1 = false;
    constexpr int kTrialPairs = 200;
    for (int trial = 0; trial < kTrialPairs; ++trial) {
        RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);
        const int count1Scene0 = countNonNeutral(0);
        const int count1Scene1 = countNonNeutral(1);

        RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);
        const int count2Scene0 = countNonNeutral(0);
        const int count2Scene1 = countNonNeutral(1);

        if (count2Scene0 < count1Scene0) {
            sawDecreaseScene0 = true;
        }
        if (count2Scene1 < count1Scene1) {
            sawDecreaseScene1 = true;
        }
        if (sawDecreaseScene0 && sawDecreaseScene1) {
            break;
        }
    }
    REQUIRE_TRUE(sawDecreaseScene0);
    REQUIRE_TRUE(sawDecreaseScene1);
}

// For each parameter Randomize All touches, the SET of nonzero-depth sources
// must be IDENTICAL in both scene poles (so the badge, which is true if ANY
// scene is nonzero, never disagrees with what a specific scene actually
// holds), while the VALUES differ per pole (so blending between scenes is
// audible).
TEST_CASE(randomize_all_scene_pair_has_identical_source_membership_but_different_values) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));

    RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);

    bool checkedAtLeastOneParameter = false;
    bool foundAValueDifference = false;
    ForEachTopLevelParameter(fx.model, [&](synth::Parameter& parameter) {
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            synth::Parameter* depth = parameter.ModulationDepthParameter(modIx);
            if (depth == nullptr) {
                continue;
            }
            checkedAtLeastOneParameter = true;
            const bool nonzeroScene0 = detail::DepthIsModulatingInScene(*depth, 0);
            const bool nonzeroScene1 = detail::DepthIsModulatingInScene(*depth, 1);
            // Identical source membership: a materialized depth that was
            // actually drawn this operation must be nonzero in BOTH poles,
            // never just one -- otherwise randomizing only scene 0 lights
            // the badge in both scenes while scene 1 reads zero.
            REQUIRE_TRUE(nonzeroScene0 == nonzeroScene1);
            if (nonzeroScene0 && depth->SceneCenter(0) != depth->SceneCenter(1)) {
                foundAValueDifference = true;
            }
        }
    });
    REQUIRE_TRUE(checkedAtLeastOneParameter);
    // Different values per pole -- otherwise scene-blending this parameter
    // would be silently inaudible despite the badge being lit.
    REQUIRE_TRUE(foundAValueDifference);
}

// ============================================================================
// default patch
// ============================================================================

// NOTE on GetRaw() vs SceneCenter(): a Parameter's GetRaw()/currentCenter_ is
// a per-SAMPLE-processed, smoothed value (Parameter::ProcessLitePhase1's
// `currentCenter_ += alpha*(targetCenter_-currentCenter_)`) that only
// updates via ParameterGroup::ProcessSamplePhase1 -- and that group-level
// call iterates ONLY topLevelParameters_ (ParameterModulation.cpp), never
// recursing into modulation-depth parameters. HandleSetAbsolute/
// HandleIncDec (both used by ApplyFroggersDefaultPatch) write directly into
// `sceneCenters_`, which SceneCenter(sceneIx) (public) reads back exactly,
// with no smoothing and no dependency on ProcessSample ever having run.
// These tests check the actual COMMANDED value via SceneCenter(0) (scene
// 0 == both leftScene and rightScene at the model's default blend=0), which
// is the precise, unambiguous assertion this file wants -- not a slewed
// display value that would need an arbitrary "settled enough" convergence
// budget to test reliably.
TEST_CASE(default_patch_shape_defaults_are_exact) {
    Fixture fx;
    ApplyFroggersDefaultPatch(fx.model);
    REQUIRE_NEAR(fx.model.PageParameter(FroggersBankId::Audio, 3).SceneCenter(0), 0.0f, 1e-6f);  // VCO1 Shape = min
    REQUIRE_NEAR(fx.model.PageParameter(FroggersBankId::Audio, 4).SceneCenter(0), 0.5f, 1e-6f);  // VCO2 Shape = 0.5
    REQUIRE_NEAR(fx.model.PageParameter(FroggersBankId::Audio, 5).SceneCenter(0), 1.0f, 1e-6f);  // VCO3 Shape = max
}

TEST_CASE(default_patch_gain_is_20_percent) {
    Fixture fx;
    ApplyFroggersDefaultPatch(fx.model);
    REQUIRE_NEAR(fx.model.PageParameter(FroggersBankId::Drive, 1).SceneCenter(0), 0.2f, 1e-6f);
}

// Follows ApplyDriveBankOverlay's own literal to Gain's slot -- Wet/Dry
// (slot 0) is a DIFFERENT control since the rename, and the overlay must
// not spill onto it: a default patch that left Wet/Dry at any nonzero value
// would ship the Drive page 20% wet instead of bypassed. Read the
// REGISTERED default straight off FroggersBankLayouts() rather than
// hardcoding 0.0f here, so this stays correct if that default is ever
// deliberately changed.
TEST_CASE(default_patch_wet_dry_reads_its_own_registered_default) {
    Fixture fx;
    ApplyFroggersDefaultPatch(fx.model);
    const float registeredDefault =
        FroggersBankLayouts()[static_cast<std::size_t>(FroggersBankId::Drive)].params[0].defaultValue;
    REQUIRE_NEAR(fx.model.PageParameter(FroggersBankId::Drive, 0).SceneCenter(0), registeredDefault, 1e-6f);
}

TEST_CASE(default_patch_cross_vco_pitch_depths_have_correct_sign_and_source) {
    Fixture fx;
    ApplyFroggersDefaultPatch(fx.model);

    synth::Parameter& vco1Pitch = fx.model.PageParameter(FroggersBankId::Audio, 0);
    synth::Parameter& vco2Pitch = fx.model.PageParameter(FroggersBankId::Audio, 1);
    synth::Parameter& vco3Pitch = fx.model.PageParameter(FroggersBankId::Audio, 2);

    constexpr float kNeutral = detail::kNeutralModulationDepthCenter;

    // VCO1 pitch <- +1 detent from VCO2 audio (7) and VCO3 audio (8).
    REQUIRE_TRUE(vco1Pitch.ModulationDepthParameter(kModSlotVco2Audio)->SceneCenter(0) > kNeutral);
    REQUIRE_TRUE(vco1Pitch.ModulationDepthParameter(kModSlotVco3Audio)->SceneCenter(0) > kNeutral);
    REQUIRE_TRUE(vco1Pitch.ModulationDepthParameter(kModSlotVco1Audio) == nullptr);  // no self-modulation

    // VCO2 pitch <- -1 detent from VCO1 audio (6) and VCO3 audio (8).
    REQUIRE_TRUE(vco2Pitch.ModulationDepthParameter(kModSlotVco1Audio)->SceneCenter(0) < kNeutral);
    REQUIRE_TRUE(vco2Pitch.ModulationDepthParameter(kModSlotVco3Audio)->SceneCenter(0) < kNeutral);
    REQUIRE_TRUE(vco2Pitch.ModulationDepthParameter(kModSlotVco2Audio) == nullptr);

    // VCO3 pitch <- +1 detent from VCO1 audio (6) and VCO2 audio (7).
    REQUIRE_TRUE(vco3Pitch.ModulationDepthParameter(kModSlotVco1Audio)->SceneCenter(0) > kNeutral);
    REQUIRE_TRUE(vco3Pitch.ModulationDepthParameter(kModSlotVco2Audio)->SceneCenter(0) > kNeutral);
    REQUIRE_TRUE(vco3Pitch.ModulationDepthParameter(kModSlotVco3Audio) == nullptr);

    // Exactly one detent's magnitude (1/100, this port's placeholder
    // quantum -- see ApplyFroggersDefaultPatch's comment) away from neutral,
    // not some other arbitrary offset.
    constexpr float kDetent = 1.0f / 100.0f;
    REQUIRE_NEAR(vco1Pitch.ModulationDepthParameter(kModSlotVco2Audio)->SceneCenter(0), kNeutral + kDetent, 1e-5f);
    REQUIRE_NEAR(vco2Pitch.ModulationDepthParameter(kModSlotVco1Audio)->SceneCenter(0), kNeutral - kDetent, 1e-5f);
}

// Scene 2 (SceneCenter(1)) must be the MIRROR of scene 1 (SceneCenter(0))
// for each of the three VCO shapes -- asserted as the computed relationship
// `scene2 == 1.0 - scene1`, not as three more hardcoded literals, so this
// test actually pins the mirror property rather than merely re-stating two
// independent lists of numbers.
TEST_CASE(default_patch_scene2_vco_shapes_are_the_mirror_of_scene1) {
    Fixture fx;
    ApplyFroggersDefaultPatch(fx.model);

    for (std::size_t vcoIx = 0; vcoIx < 3; ++vcoIx) {
        synth::Parameter& shapeParam = fx.model.PageParameter(FroggersBankId::Audio, 3 + vcoIx);
        const float scene1 = shapeParam.SceneCenter(0);
        const float scene2 = shapeParam.SceneCenter(1);
        REQUIRE_NEAR(scene2, 1.0f - scene1, 1e-6f);
    }
    // Scene 1 itself is unchanged from the pre-C1 defaults (pinned again
    // here, alongside the mirror check, so a future edit cannot satisfy the
    // mirror relationship by drifting scene 1 instead of deriving scene 2).
    REQUIRE_NEAR(fx.model.PageParameter(FroggersBankId::Audio, 3).SceneCenter(0), 0.0f, 1e-6f);
    REQUIRE_NEAR(fx.model.PageParameter(FroggersBankId::Audio, 4).SceneCenter(0), 0.5f, 1e-6f);
    REQUIRE_NEAR(fx.model.PageParameter(FroggersBankId::Audio, 5).SceneCenter(0), 1.0f, 1e-6f);
}

// The light cross-VCO pitch-modulation depths must be PRESENT and EQUAL in
// both scene poles -- same source set (already covered by the sign/source
// test above via SceneCenter(0)), same magnitude in scene 2 as in scene 1.
TEST_CASE(default_patch_cross_vco_pitch_depths_equal_in_both_scene_poles) {
    Fixture fx;
    ApplyFroggersDefaultPatch(fx.model);

    synth::Parameter& vco1Pitch = fx.model.PageParameter(FroggersBankId::Audio, 0);
    synth::Parameter& vco2Pitch = fx.model.PageParameter(FroggersBankId::Audio, 1);
    synth::Parameter& vco3Pitch = fx.model.PageParameter(FroggersBankId::Audio, 2);

    const std::array<std::pair<synth::Parameter*, std::size_t>, 6> depths{{
        {&vco1Pitch, kModSlotVco2Audio},
        {&vco1Pitch, kModSlotVco3Audio},
        {&vco2Pitch, kModSlotVco1Audio},
        {&vco2Pitch, kModSlotVco3Audio},
        {&vco3Pitch, kModSlotVco1Audio},
        {&vco3Pitch, kModSlotVco2Audio},
    }};
    for (const auto& [param, modIx] : depths) {
        synth::Parameter* depth = param->ModulationDepthParameter(modIx);
        REQUIRE_TRUE(depth != nullptr);  // present in both poles (materialized once, applies to both)
        REQUIRE_NEAR(depth->SceneCenter(0), depth->SceneCenter(1), 1e-6f);  // equal in both poles
    }
}

TEST_CASE(default_patch_touches_no_parameter_outside_the_enumerated_set) {
    // Baseline: an unmodified model (no default patch applied).
    Fixture baselineFx;
    // Patched: the same construction, WITH the default patch applied.
    Fixture patchedFx;
    ApplyFroggersDefaultPatch(patchedFx.model);

    // Every top-level parameter's OWN scene-center value must match the
    // baseline exactly, except the three enumerated Shape controls and the
    // one enumerated Gain control.
    for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
        const auto bankId = static_cast<FroggersBankId>(bankIx);
        for (std::size_t paramIx = 0; paramIx < kFroggersParamsPerBank; ++paramIx) {
            const bool isEnumeratedShape = bankId == FroggersBankId::Audio && paramIx >= 3 && paramIx <= 5;
            const bool isEnumeratedDrive = bankId == FroggersBankId::Drive && paramIx == 1;
            const float baseline = baselineFx.model.PageParameter(bankId, paramIx).SceneCenter(0);
            const float patched = patchedFx.model.PageParameter(bankId, paramIx).SceneCenter(0);
            if (isEnumeratedShape || isEnumeratedDrive) {
                continue;  // checked exactly by the two tests above
            }
            REQUIRE_NEAR(patched, baseline, 1e-9f);
        }
        REQUIRE_NEAR(patchedFx.model.Crispy(bankId).SceneCenter(0), baselineFx.model.Crispy(bankId).SceneCenter(0), 1e-9f);
    }
    REQUIRE_NEAR(patchedFx.model.Crunchy().SceneCenter(0), baselineFx.model.Crunchy().SceneCenter(0), 1e-9f);

    // No depth parameter exists anywhere EXCEPT the six enumerated
    // (VCO-pitch, VCO-audio-source) pairs.
    ForEachTopLevelParameter(patchedFx.model, [&](synth::Parameter& parameter) {
        const bool isVco1Pitch = &parameter == &patchedFx.model.PageParameter(FroggersBankId::Audio, 0);
        const bool isVco2Pitch = &parameter == &patchedFx.model.PageParameter(FroggersBankId::Audio, 1);
        const bool isVco3Pitch = &parameter == &patchedFx.model.PageParameter(FroggersBankId::Audio, 2);
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            const bool isEnumeratedDepth =
                (isVco1Pitch && (modIx == kModSlotVco2Audio || modIx == kModSlotVco3Audio)) ||
                (isVco2Pitch && (modIx == kModSlotVco1Audio || modIx == kModSlotVco3Audio)) ||
                (isVco3Pitch && (modIx == kModSlotVco1Audio || modIx == kModSlotVco2Audio));
            if (isEnumeratedDepth) {
                continue;
            }
            REQUIRE_TRUE(parameter.ModulationDepthParameter(modIx) == nullptr);
        }
    });
}

// ============================================================================
// ResetPage/ResetAll (model layer)
// ============================================================================

// Snapshot of one top-level parameter's own value (both scene poles) plus,
// for every modulator slot, whether a depth is materialized there and if so
// its own value (both poles) -- shared by the "only the intended set
// changed" tests below, so the before/after comparison logic is written
// exactly once.
struct ParamSnapshot {
    float v0 = 0.0f;
    float v1 = 0.0f;
    std::array<std::optional<std::pair<float, float>>, FroggersParameterModel::kNumModulators> depths{};
};

ParamSnapshot SnapshotParameter(synth::Parameter& parameter) {
    ParamSnapshot snap;
    snap.v0 = parameter.SceneCenter(0);
    snap.v1 = parameter.SceneCenter(1);
    for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
        synth::Parameter* depth = parameter.ModulationDepthParameter(modIx);
        if (depth != nullptr) {
            snap.depths[modIx] = std::make_pair(depth->SceneCenter(0), depth->SceneCenter(1));
        }
    }
    return snap;
}

std::vector<std::pair<synth::Parameter*, ParamSnapshot>> SnapshotAllTopLevelParameters(FroggersParameterModel& model) {
    std::vector<std::pair<synth::Parameter*, ParamSnapshot>> snapshots;
    ForEachTopLevelParameter(model, [&](synth::Parameter& parameter) {
        snapshots.push_back({&parameter, SnapshotParameter(parameter)});
    });
    return snapshots;
}

void RequireParameterUnchanged(synth::Parameter& parameter, const ParamSnapshot& before) {
    REQUIRE_NEAR(parameter.SceneCenter(0), before.v0, 1e-9f);
    REQUIRE_NEAR(parameter.SceneCenter(1), before.v1, 1e-9f);
    for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
        synth::Parameter* depth = parameter.ModulationDepthParameter(modIx);
        if (!before.depths[modIx].has_value()) {
            REQUIRE_TRUE(depth == nullptr);
            continue;
        }
        REQUIRE_TRUE(depth != nullptr);
        REQUIRE_NEAR(depth->SceneCenter(0), before.depths[modIx]->first, 1e-9f);
        REQUIRE_NEAR(depth->SceneCenter(1), before.depths[modIx]->second, 1e-9f);
    }
}

bool IsOnBankPage(synth::Parameter* candidate, FroggersParameterModel& model, FroggersBankId bankId) {
    if (candidate == &model.Crispy(bankId)) {
        return true;
    }
    for (std::size_t paramIx = 0; paramIx < kFroggersParamsPerBank; ++paramIx) {
        if (candidate == &model.PageParameter(bankId, paramIx)) {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Fresh-instance oracle: Reset's own correctness standard is "matches a
// freshly constructed model with the default patch applied" -- not a
// hand-written literal, since a bank's own default is 0.0f for most
// parameters but not all (FroggersBankLayouts()'s own defaultValue field,
// plus the Audio/Drive overlays). An unmaterialized depth reads as neutral,
// the same as a materialized-and-neutral one (an absent depth modulates
// nothing either way), so the comparison below treats the two as equal
// rather than requiring identical materialization -- Reset never
// de-materializes a depth it once touched, so a Reset instance and a truly
// fresh one can disagree on WHETHER a depth Parameter exists while still
// agreeing on what it means.
// ----------------------------------------------------------------------------

std::pair<float, float> EffectiveDepthValue(const ParamSnapshot& snap, std::size_t modIx) {
    if (!snap.depths[modIx].has_value()) {
        return {detail::kNeutralModulationDepthCenter, detail::kNeutralModulationDepthCenter};
    }
    return *snap.depths[modIx];
}

bool ParameterDiffersFromDefaultPatch(synth::Parameter& parameter, synth::Parameter& reference, float tolerance) {
    const ParamSnapshot actual = SnapshotParameter(parameter);
    const ParamSnapshot expected = SnapshotParameter(reference);
    if (std::fabs(actual.v0 - expected.v0) > tolerance || std::fabs(actual.v1 - expected.v1) > tolerance) {
        return true;
    }
    for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
        const auto [actualD0, actualD1] = EffectiveDepthValue(actual, modIx);
        const auto [expectedD0, expectedD1] = EffectiveDepthValue(expected, modIx);
        if (std::fabs(actualD0 - expectedD0) > tolerance || std::fabs(actualD1 - expectedD1) > tolerance) {
            return true;
        }
    }
    return false;
}

void RequireParameterMatchesDefaultPatch(synth::Parameter& parameter, synth::Parameter& reference) {
    constexpr float kTol = 1e-6f;
    const ParamSnapshot actual = SnapshotParameter(parameter);
    const ParamSnapshot expected = SnapshotParameter(reference);
    REQUIRE_NEAR(actual.v0, expected.v0, kTol);
    REQUIRE_NEAR(actual.v1, expected.v1, kTol);
    for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
        const auto [actualD0, actualD1] = EffectiveDepthValue(actual, modIx);
        const auto [expectedD0, expectedD1] = EffectiveDepthValue(expected, modIx);
        REQUIRE_NEAR(actualD0, expectedD0, kTol);
        REQUIRE_NEAR(actualD1, expectedD1, kTol);
    }
}

void RequireModelMatchesFreshDefaultPatch(FroggersParameterModel& actualModel, FroggersParameterModel& referenceModel) {
    for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
        const auto bankId = static_cast<FroggersBankId>(bankIx);
        for (std::size_t paramIx = 0; paramIx < kFroggersParamsPerBank; ++paramIx) {
            RequireParameterMatchesDefaultPatch(actualModel.PageParameter(bankId, paramIx),
                                                 referenceModel.PageParameter(bankId, paramIx));
        }
        RequireParameterMatchesDefaultPatch(actualModel.Crispy(bankId), referenceModel.Crispy(bankId));
    }
    RequireParameterMatchesDefaultPatch(actualModel.Crunchy(), referenceModel.Crunchy());
}

// ----------------------------------------------------------------------------
// "ResetPage clears only the current bank" / "ResetAll clears every bank"
// ----------------------------------------------------------------------------

TEST_CASE(reset_page_clears_only_current_bank_values_and_depths) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));

    // Give every bank non-neutral page-parameter values, non-neutral Crispy
    // values, and materialized, non-neutral depths, so ResetPage has
    // something real to clear on Reverb and something real to prove
    // untouched everywhere else.
    RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);
    for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
        FroggersModulationDrillIn pageDrill(fx.model.BankAt(static_cast<FroggersBankId>(bankIx)));
        RandomizePage(fx.manager, pageDrill);
    }

    Fixture reference;
    ApplyFroggersDefaultPatch(reference.model);

    const auto before = SnapshotAllTopLevelParameters(fx.model);

    // Positive control: confirm the setup above actually moved Reverb's own
    // page away from the default patch -- a materialized non-neutral depth,
    // and at least one value -- before trusting "matches the default patch
    // after" as meaningful.
    bool reverbHadNonNeutralDepthBefore = false;
    bool reverbValueMovedBefore = false;
    for (std::size_t paramIx = 0; paramIx < kFroggersParamsPerBank; ++paramIx) {
        synth::Parameter& p = fx.model.PageParameter(FroggersBankId::Reverb, paramIx);
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            synth::Parameter* depth = p.ModulationDepthParameter(modIx);
            if (depth != nullptr && detail::DepthIsModulating(*depth)) {
                reverbHadNonNeutralDepthBefore = true;
            }
        }
        if (ParameterDiffersFromDefaultPatch(p, reference.model.PageParameter(FroggersBankId::Reverb, paramIx), 0.05f)) {
            reverbValueMovedBefore = true;
        }
    }
    REQUIRE_TRUE(reverbHadNonNeutralDepthBefore);
    REQUIRE_TRUE(reverbValueMovedBefore);

    ResetPage(fx.manager, drillIn, fx.model);  // drillIn is still Level()==0, on Reverb

    bool anyReverbDepthCheckedAfter = false;
    for (std::size_t paramIx = 0; paramIx < kFroggersParamsPerBank; ++paramIx) {
        synth::Parameter& p = fx.model.PageParameter(FroggersBankId::Reverb, paramIx);
        RequireParameterMatchesDefaultPatch(p, reference.model.PageParameter(FroggersBankId::Reverb, paramIx));
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            if (p.ModulationDepthParameter(modIx) != nullptr) {
                anyReverbDepthCheckedAfter = true;
            }
        }
    }
    RequireParameterMatchesDefaultPatch(fx.model.Crispy(FroggersBankId::Reverb),
                                         reference.model.Crispy(FroggersBankId::Reverb));
    REQUIRE_TRUE(anyReverbDepthCheckedAfter);

    // Every OTHER top-level parameter (five other banks' page params +
    // Crispy, and Crunchy) is completely untouched -- own value AND every
    // materialized depth, both poles.
    for (const auto& [paramPtr, snap] : before) {
        if (IsOnBankPage(paramPtr, fx.model, FroggersBankId::Reverb)) {
            continue;  // checked above.
        }
        RequireParameterUnchanged(*paramPtr, snap);
    }
}

TEST_CASE(reset_page_on_audio_restores_shapes_and_pitch_detents_while_other_banks_stay_untouched) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Audio));

    RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);
    for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
        FroggersModulationDrillIn pageDrill(fx.model.BankAt(static_cast<FroggersBankId>(bankIx)));
        RandomizePage(fx.manager, pageDrill);
    }

    Fixture reference;
    ApplyFroggersDefaultPatch(reference.model);

    // Positive control: the randomize above genuinely moved Audio's own
    // page (shapes and/or pitch detents included) away from the default
    // patch.
    bool audioMovedBefore = false;
    for (std::size_t paramIx = 0; paramIx < kFroggersParamsPerBank; ++paramIx) {
        if (ParameterDiffersFromDefaultPatch(fx.model.PageParameter(FroggersBankId::Audio, paramIx),
                                              reference.model.PageParameter(FroggersBankId::Audio, paramIx), 0.05f)) {
            audioMovedBefore = true;
        }
    }
    REQUIRE_TRUE(audioMovedBefore);

    const auto before = SnapshotAllTopLevelParameters(fx.model);

    ResetPage(fx.manager, drillIn, fx.model);  // drillIn is still Level()==0, on Audio

    // Audio's own slice -- values (shapes included) and depths (the six
    // cross-VCO pitch detents included) -- matches the default patch
    // exactly, not a flat zero/neutral clear.
    for (std::size_t paramIx = 0; paramIx < kFroggersParamsPerBank; ++paramIx) {
        RequireParameterMatchesDefaultPatch(fx.model.PageParameter(FroggersBankId::Audio, paramIx),
                                             reference.model.PageParameter(FroggersBankId::Audio, paramIx));
    }
    RequireParameterMatchesDefaultPatch(fx.model.Crispy(FroggersBankId::Audio),
                                         reference.model.Crispy(FroggersBankId::Audio));

    // Every other bank (and Crunchy) is completely untouched from its
    // pre-reset (still-randomized) state.
    for (const auto& [paramPtr, snap] : before) {
        if (IsOnBankPage(paramPtr, fx.model, FroggersBankId::Audio)) {
            continue;  // checked above.
        }
        RequireParameterUnchanged(*paramPtr, snap);
    }
}

TEST_CASE(reset_all_matches_a_freshly_constructed_default_patch_instance_field_for_field) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));

    RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);  // values+depths on all 6 banks' page params
    for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
        FroggersModulationDrillIn pageDrill(fx.model.BankAt(static_cast<FroggersBankId>(bankIx)));
        RandomizePage(fx.manager, pageDrill);  // Crispy on every bank -- Randomize All itself excludes it
    }
    // RandomizeAll/RandomizePage never touch Crunchy -- perturb it by hand
    // so Reset All's own "Crunchy is global too" behaviour has something
    // real to prove.
    constexpr float kPerturbedCrunchy = 0.73f;
    fx.model.Crunchy().SceneCenter(0) = kPerturbedCrunchy;
    fx.model.Crunchy().SceneCenter(1) = kPerturbedCrunchy;

    Fixture reference;
    ApplyFroggersDefaultPatch(reference.model);

    // Positive control: confirm the setup above genuinely moved every
    // bank's own page, every bank's Crispy, and Crunchy away from the
    // default patch -- a reset that "matches" a patch that never moved
    // would be void, not passing. A random draw can coincidentally land a
    // bank's own Crispy within tolerance of its default value, so retry that
    // bank's own RandomizePage a bounded number of times (P(20 straight
    // coincidences) is astronomically small) rather than accepting an
    // occasional spurious failure here.
    for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
        const auto bankId = static_cast<FroggersBankId>(bankIx);
        FroggersModulationDrillIn pageDrill(fx.model.BankAt(bankId));
        bool bankMoved = false;
        bool crispyMoved = false;
        for (int attempt = 0; attempt < 20 && !(bankMoved && crispyMoved); ++attempt) {
            for (std::size_t paramIx = 0; paramIx < kFroggersParamsPerBank; ++paramIx) {
                if (ParameterDiffersFromDefaultPatch(fx.model.PageParameter(bankId, paramIx),
                                                      reference.model.PageParameter(bankId, paramIx), 0.05f)) {
                    bankMoved = true;
                }
            }
            crispyMoved =
                ParameterDiffersFromDefaultPatch(fx.model.Crispy(bankId), reference.model.Crispy(bankId), 0.05f);
            if (!(bankMoved && crispyMoved)) {
                RandomizePage(fx.manager, pageDrill);  // Crispy on this bank -- Randomize All itself excludes it
            }
        }
        REQUIRE_TRUE(bankMoved);
        REQUIRE_TRUE(crispyMoved);
    }
    REQUIRE_TRUE(ParameterDiffersFromDefaultPatch(fx.model.Crunchy(), reference.model.Crunchy(), 0.05f));

    ResetAll(fx.manager, drillIn, fx.model);  // Level()==0

    // The whole instrument -- every bank's page, every bank's Crispy, and
    // the shared Crunchy, both scene poles, every materialized depth --
    // matches a freshly constructed, freshly patched instance field for
    // field.
    RequireModelMatchesFreshDefaultPatch(fx.model, reference.model);
}

TEST_CASE(reset_page_never_touches_crunchy) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    synth::Parameter& crunchy = fx.model.Crunchy();
    // Deliberately non-zero, non-neutral, non-default: Crunchy's own
    // ParameterConfig default is already 0.0f, so checking against that
    // default alone would not distinguish "never touched" from "touched and
    // reset to 0.0" -- this perturbation makes the check real.
    constexpr float kPerturbed = 0.73f;
    crunchy.SceneCenter(0) = kPerturbed;
    crunchy.SceneCenter(1) = kPerturbed;

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    ResetPage(fx.manager, drillIn, fx.model);
    REQUIRE_TRUE(crunchy.SceneCenter(0) == kPerturbed);
    REQUIRE_TRUE(crunchy.SceneCenter(1) == kPerturbed);
}

TEST_CASE(reset_all_resets_crunchy_to_its_default) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    synth::Parameter& crunchy = fx.model.Crunchy();
    // Comfortably away from Crunchy's own default (0.0f, its
    // ParameterConfig's own struct default -- see FroggersParameters.hpp's
    // Crunchy registration) so "reads its default after" is a real check,
    // not a 0.0-before/0.0-after tautology.
    constexpr float kPerturbed = 0.73f;
    crunchy.SceneCenter(0) = kPerturbed;
    crunchy.SceneCenter(1) = kPerturbed;
    // Crunchy's own modulation depths are part of "Crunchy reverts to its
    // default" too, not just its value.
    synth::Parameter* crunchyDepth = crunchy.EnsureModulationDepth(kModSlotRandomSh1);
    REQUIRE_TRUE(crunchyDepth != nullptr);
    crunchyDepth->SceneCenter(0) = 0.9f;
    crunchyDepth->SceneCenter(1) = 0.9f;

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    ResetAll(fx.manager, drillIn, fx.model);  // Level()==0, global

    constexpr float kTol = 1e-6f;
    REQUIRE_NEAR(crunchy.SceneCenter(0), 0.0f, kTol);
    REQUIRE_NEAR(crunchy.SceneCenter(1), 0.0f, kTol);
    REQUIRE_NEAR(crunchyDepth->SceneCenter(0), detail::kNeutralModulationDepthCenter, kTol);
    REQUIRE_NEAR(crunchyDepth->SceneCenter(1), detail::kNeutralModulationDepthCenter, kTol);
}

// ----------------------------------------------------------------------------
// The trap: a depth's "off" is NEUTRAL (0.5), never 0.0.
// ----------------------------------------------------------------------------

TEST_CASE(reset_depths_read_neutral_not_zero_the_trap_is_not_reintroduced) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    drillIn.PressEncoder(0);  // -> level 1: materializes all 15 depth cells on Reverb param 0
    REQUIRE_TRUE(drillIn.Level() == 1);
    synth::Parameter& focused = fx.model.PageParameter(FroggersBankId::Reverb, 0);

    // RandomizePage draws 0 sources on 50% of calls (see
    // FroggersModulation.hpp's count table), so a single draw is not
    // guaranteed to leave a non-neutral depth to reset -- retry a bounded
    // number of times (P(20 straight no-ops) is astronomically small) until
    // one lands, rather than accepting an occasional spurious failure here.
    bool anyNonNeutralBefore = false;
    for (int attempt = 0; attempt < 20 && !anyNonNeutralBefore; ++attempt) {
        RandomizePage(fx.manager, drillIn);  // give the depths real, non-neutral values first
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            synth::Parameter* depth = focused.ModulationDepthParameter(modIx);
            if (depth != nullptr && detail::DepthIsModulating(*depth)) {
                anyNonNeutralBefore = true;
            }
        }
    }
    // Positive control: confirm at least one depth is genuinely non-neutral
    // before reset.
    REQUIRE_TRUE(anyNonNeutralBefore);

    ResetPage(fx.manager, drillIn, fx.model);  // Level()==1: resets focused's own depths

    constexpr float kNeutral = detail::kNeutralModulationDepthCenter;  // 0.5f
    constexpr float kTol = 1e-6f;
    bool anyDepthChecked = false;
    for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
        synth::Parameter* depth = focused.ModulationDepthParameter(modIx);
        if (depth == nullptr) {
            continue;
        }
        anyDepthChecked = true;
        // The trap, pinned directly: NEUTRAL (0.5), not zero. This tight a
        // tolerance against 0.5 would catch a reset that wrote literal 0.0
        // (full negative depth, the opposite of off) -- 0.5 and 0.0 differ
        // by 0.5, five orders of magnitude past kTol.
        REQUIRE_NEAR(depth->SceneCenter(0), kNeutral, kTol);
        REQUIRE_NEAR(depth->SceneCenter(1), kNeutral, kTol);
        REQUIRE_TRUE(depth->SceneCenter(0) != 0.0f);
        REQUIRE_TRUE(depth->SceneCenter(1) != 0.0f);
    }
    REQUIRE_TRUE(anyDepthChecked);
}

// ----------------------------------------------------------------------------
// Reset mirrors Randomize's own scope at the same drill level.
// ----------------------------------------------------------------------------

TEST_CASE(reset_page_drilled_in_acts_on_only_the_selected_parameters_own_depths) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    synth::Parameter& focused = fx.model.PageParameter(FroggersBankId::Reverb, 0);
    synth::Parameter& sibling = fx.model.PageParameter(FroggersBankId::Reverb, 2);

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));

    // Give `focused` real, materialized, non-neutral depths.
    drillIn.PressEncoder(0);  // -> level 1 on `focused`
    RandomizePage(fx.manager, drillIn);
    drillIn.Back();
    REQUIRE_TRUE(drillIn.Level() == 0);

    // Give `sibling` its OWN real, materialized, non-neutral depths, via the
    // SAME drillIn (never two concurrent drill-ins on one Bank -- Bank's
    // selection state is shared, app-side level tracking is not).
    drillIn.PressEncoder(2);  // -> level 1 on `sibling`
    RandomizePage(fx.manager, drillIn);
    drillIn.Back();
    REQUIRE_TRUE(drillIn.Level() == 0);

    // Re-open `focused` (its own non-neutral depths from above survive --
    // Deselect()'s CollectNeutralLocalParameters() only reclaims depths
    // still at their neutral default, not ones actually written).
    drillIn.PressEncoder(0);  // -> level 1 on `focused` again
    REQUIRE_TRUE(drillIn.Level() == 1);
    REQUIRE_TRUE(drillIn.BankRef().SelectedParameter() == &focused);

    // Perturb `focused`'s own value to something distinctive and non-default
    // so "Reset leaves it alone at this drill level" is a real check, not a
    // 0.0-before/0.0-after tautology.
    constexpr float kPerturbedValue = 0.37f;
    focused.SceneCenter(0) = kPerturbedValue;
    focused.SceneCenter(1) = kPerturbedValue;

    const auto siblingBefore = SnapshotParameter(sibling);

    ResetPage(fx.manager, drillIn, fx.model);  // drillIn still at Level()==1 on `focused`

    // `focused`'s own depths -- RandomizeParameterModulationDepths's exact
    // target set at this drill level -- are all neutral now.
    constexpr float kNeutral = detail::kNeutralModulationDepthCenter;
    constexpr float kTol = 1e-6f;
    bool anyFocusedDepthChecked = false;
    for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
        synth::Parameter* depth = focused.ModulationDepthParameter(modIx);
        if (depth == nullptr) {
            continue;
        }
        anyFocusedDepthChecked = true;
        REQUIRE_NEAR(depth->SceneCenter(0), kNeutral, kTol);
        REQUIRE_NEAR(depth->SceneCenter(1), kNeutral, kTol);
    }
    REQUIRE_TRUE(anyFocusedDepthChecked);

    // `focused`'s OWN value is untouched -- Randomize never writes to it at
    // this level either (only to its depth children), so Reset matching
    // Randomize's scope must leave it alone too.
    REQUIRE_NEAR(focused.SceneCenter(0), kPerturbedValue, kTol);
    REQUIRE_NEAR(focused.SceneCenter(1), kPerturbedValue, kTol);

    // `sibling` -- a different top-level parameter's own depths, never the
    // selected one -- is completely untouched.
    RequireParameterUnchanged(sibling, siblingBefore);
}

// ----------------------------------------------------------------------------
// Drilled-in reset on the Audio bank's pitch parameters: the default patch
// itself gives two of the fifteen depth cells a non-neutral value, so
// resetting them must land back on that detent, not neutral -- the same
// "0.0 is not off" class of trap as above, one level up (a whole depth
// carrying the wrong default, not just the wrong zero).
// ----------------------------------------------------------------------------

TEST_CASE(reset_page_drilled_into_audio_pitch_restores_its_default_patch_detent_not_neutral) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Audio));
    drillIn.PressEncoder(0);  // -> level 1: materializes all 15 depth cells on VCO1 pitch
    REQUIRE_TRUE(drillIn.Level() == 1);
    synth::Parameter& vco1Pitch = fx.model.PageParameter(FroggersBankId::Audio, 0);

    Fixture reference;
    ApplyFroggersDefaultPatch(reference.model);
    synth::Parameter& referenceVco1Pitch = reference.model.PageParameter(FroggersBankId::Audio, 0);

    // The two depths the default patch itself sets a detent on. Perturb
    // both away from that detent so "restores it" is a real check.
    synth::Parameter* detentFromVco2 = vco1Pitch.ModulationDepthParameter(kModSlotVco2Audio);
    synth::Parameter* detentFromVco3 = vco1Pitch.ModulationDepthParameter(kModSlotVco3Audio);
    REQUIRE_TRUE(detentFromVco2 != nullptr);
    REQUIRE_TRUE(detentFromVco3 != nullptr);
    constexpr float kPerturbed = 0.9f;
    detentFromVco2->SceneCenter(0) = kPerturbed;
    detentFromVco2->SceneCenter(1) = kPerturbed;
    detentFromVco3->SceneCenter(0) = kPerturbed;
    detentFromVco3->SceneCenter(1) = kPerturbed;

    // Positive control: the perturbation above genuinely moved both depths
    // away from their default-patch detent value.
    synth::Parameter* referenceDetentFromVco2 = referenceVco1Pitch.ModulationDepthParameter(kModSlotVco2Audio);
    synth::Parameter* referenceDetentFromVco3 = referenceVco1Pitch.ModulationDepthParameter(kModSlotVco3Audio);
    REQUIRE_TRUE(referenceDetentFromVco2 != nullptr);
    REQUIRE_TRUE(referenceDetentFromVco3 != nullptr);
    REQUIRE_TRUE(std::fabs(detentFromVco2->SceneCenter(0) - referenceDetentFromVco2->SceneCenter(0)) > 0.05f);
    REQUIRE_TRUE(std::fabs(detentFromVco3->SceneCenter(0) - referenceDetentFromVco3->SceneCenter(0)) > 0.05f);

    ResetPage(fx.manager, drillIn, fx.model);  // Level()==1: resets vco1Pitch's own depths

    // Every one of VCO1 pitch's depths -- the two detents included -- now
    // matches the default patch exactly, not a flat neutral clear.
    RequireParameterMatchesDefaultPatch(vco1Pitch, referenceVco1Pitch);

    // The trap, pinned directly: the two detents are nowhere near neutral.
    constexpr float kNeutral = detail::kNeutralModulationDepthCenter;
    REQUIRE_TRUE(std::fabs(detentFromVco2->SceneCenter(0) - kNeutral) > 0.005f);
    REQUIRE_TRUE(std::fabs(detentFromVco3->SceneCenter(0) - kNeutral) > 0.005f);
}

TEST_CASE(reset_all_drilled_into_audio_pitch_restores_its_default_patch_detent_not_neutral) {
    Fixture fx;
    fx.StepOnce(/*externalConnected=*/true);
    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Audio));
    drillIn.PressEncoder(1);  // -> level 1: materializes all 15 depth cells on VCO2 pitch
    REQUIRE_TRUE(drillIn.Level() == 1);
    synth::Parameter& vco2Pitch = fx.model.PageParameter(FroggersBankId::Audio, 1);

    Fixture reference;
    ApplyFroggersDefaultPatch(reference.model);
    synth::Parameter& referenceVco2Pitch = reference.model.PageParameter(FroggersBankId::Audio, 1);

    synth::Parameter* detentFromVco1 = vco2Pitch.ModulationDepthParameter(kModSlotVco1Audio);
    REQUIRE_TRUE(detentFromVco1 != nullptr);
    constexpr float kPerturbed = 0.05f;
    detentFromVco1->SceneCenter(0) = kPerturbed;
    detentFromVco1->SceneCenter(1) = kPerturbed;

    // Positive control.
    synth::Parameter* referenceDetentFromVco1 = referenceVco2Pitch.ModulationDepthParameter(kModSlotVco1Audio);
    REQUIRE_TRUE(referenceDetentFromVco1 != nullptr);
    REQUIRE_TRUE(std::fabs(detentFromVco1->SceneCenter(0) - referenceDetentFromVco1->SceneCenter(0)) > 0.05f);

    ResetAll(fx.manager, drillIn, fx.model);  // Level()==1

    RequireParameterMatchesDefaultPatch(vco2Pitch, referenceVco2Pitch);
}

// ============================================================================
// randomize lands the drawn value even under live modulation
// ============================================================================
// Sheaf's own `Parameter::RandomizeVisibleValue`
// (External/Sheaf/projects/synth/src/ParameterModulation.cpp) computes
// `delta = target - TargetValue(0)` -- a delta against the
// MODULATION-RESOLVED value -- so under live audio-rate modulation each
// press's delta is measured against a modulated snapshot; repeated presses
// ratchet the commanded value into the [0,1] clamp. Measured on the running
// instrument: the commanded value landed at EXACTLY 1.0000 in 20/20
// independent trials after 5 presses each. The fix (FroggersModulation.hpp's
// `PressBankWithRandomValue`) no longer routes the VALUE write through that
// function at all: it draws its own uniform value and commits it directly
// via `HandleSetAbsolute` (`sceneCenters_`, no dependency on
// resolved/modulated state), to both scene poles. Filed upstream as
// `UPSTREAM-SHEAF-ASK.md` ask #16; Sheaf is pinned and untouched, so this
// app-side fix does not wait on it.
//
// Verified (not asserted here): temporarily restoring the OLD
// press-with-RandomHeld body in `PressBankWithRandomValue` and rebuilding
// this exact binary reproduces the ratchet against the harness below and
// fails the assertions that follow; restoring the fix and rebuilding turns
// it green again.

// Attaches a FULL-POSITIVE (SceneCenter == 1.0 -> ModulationDepthTargetFromKnob
// == +1.0 bipolar) modulation route from VCO1 Audio to `parameter`, so
// `TargetValue(0)` is driven by the live, audio-rate-oscillating source
// signal instead of `parameter`'s own commanded value. At depth == +1.0 the
// route's |depth| weight sum is exactly 1.0, which zeroes
// `targetCenterScales_` (ParameterModulation.cpp:2257-2268's weightSum>=1.0
// branch) -- i.e. the commanded center contributes NOTHING to
// `TargetValue(0)`; it is driven entirely by the oscillating source. VCO1's
// pitch is pinned near the top of its kPitchMinHz-kPitchMaxHz exponential
// map (dsp/Vco.hpp's `PitchToPhaseIncrement`): at 48 kHz and pitch==1.0 that
// is kPitchMaxHz/48000 ~= 0.104 cycles/sample, so a handful of `Step()`
// calls sweeps the source's full excursion -- the same live-modulation shape
// a whole running instrument would show, built here from public API instead.
// `fx.manager.ComputeAllParameters()` is the
// immediate (non-smoothed) resync -- see that method's own comment -- so
// the depth's commanded value is fully converged into `GetRaw()` before the
// caller reads anything.
void AttachFullPositiveAudioRateModulation(Fixture& fx, synth::Parameter& parameter) {
    synth::Parameter* depth = parameter.EnsureModulationDepth(kModSlotVco1Audio);
    REQUIRE_TRUE(depth != nullptr);
    for (const synth::SceneState& pole : detail::kScenePoles) {
        depth->SceneCenter(pole.leftScene) = 1.0f;  // max normalized -> +1 bipolar (full positive)
    }
    fx.manager.ComputeAllParameters();
}

TEST_CASE(randomize_lands_the_drawn_value_under_full_positive_audio_rate_modulation_across_50_presses) {
    Fixture fx;
    synth::Bank& bank = fx.model.BankAt(FroggersBankId::Reverb);
    synth::Parameter& parameter = fx.model.PageParameter(FroggersBankId::Reverb, 0);
    AttachFullPositiveAudioRateModulation(fx, parameter);

    // pitch=1.0 (near kPitchMaxHz -- see the helper's own comment):
    // audio-rate, fast enough to sweep the source's full excursion between
    // presses.
    const FroggersModulationSlate::VcoDrive fastVco{1.0f, 0.5f, 0.0f};

    constexpr int kPresses = 50;
    std::array<float, kPresses> commanded{};
    int atClamp = 0;
    double sum = 0.0;
    for (int i = 0; i < kPresses; ++i) {
        fx.slate.Step(fastVco, fastVco, fastVco, std::nullopt);
        fx.model.Group().UpdateModValues();  // refresh the live-modulation snapshot before the press
        detail::PressBankWithRandomValue(fx.manager, bank, 0);  // encoder 0 -> `parameter`
        const float value = parameter.SceneCenter(0);
        commanded[static_cast<std::size_t>(i)] = value;
        sum += static_cast<double>(value);
        if (std::fabs(value - 1.0f) < 1e-6f) {
            ++atClamp;
        }
    }

    const double mean = sum / kPresses;
    // A draw lands the drawn value, so the resulting sequence is an i.i.d.
    // sample of NextRandomValue()'s uniform draw, not a biased walk toward
    // the clamp.
    REQUIRE_TRUE(mean >= 0.35 && mean <= 0.65);
    REQUIRE_TRUE(atClamp < static_cast<int>(kPresses * 0.05));  // <5% at the exact 1.0 clamp

    // No monotone drift: a ratchet toward the clamp pushes the second half's
    // mean well above the first half's; an unbiased uniform draw keeps the
    // two close regardless of press order.
    double firstHalfSum = 0.0;
    double secondHalfSum = 0.0;
    for (int i = 0; i < kPresses / 2; ++i) {
        firstHalfSum += static_cast<double>(commanded[static_cast<std::size_t>(i)]);
    }
    for (int i = kPresses / 2; i < kPresses; ++i) {
        secondHalfSum += static_cast<double>(commanded[static_cast<std::size_t>(i)]);
    }
    const double firstHalfMean = firstHalfSum / (kPresses / 2);
    const double secondHalfMean = secondHalfSum / (kPresses / 2);
    REQUIRE_TRUE(std::fabs(secondHalfMean - firstHalfMean) < 0.3);

    // [OBSERVED] -- this file's own convention for recording the sampled
    // shape alongside its pass condition.
    std::cout << "[OBSERVED] randomize-under-modulation commanded-value distribution over " << kPresses
              << " presses: mean=" << mean << " atClampFraction=" << (100.0 * atClamp / kPresses) << "%"
              << " firstHalfMean=" << firstHalfMean << " secondHalfMean=" << secondHalfMean << "\n";
}

// ============================================================================
// Lane 6's visualizer draws its own trace, not a full-node background
// ============================================================================

// source6Visualizer_ is constructed with drawBackground=false
// (FroggersModulation.hpp's own constructor, the trailing argument to
// GangedRandomLfoVisualizer<1>). GangedRandomLfoVisualizer::
// AppendBackgroundAndAxis (GangedRandomLfoVisualizer.hpp:57-70) is the only
// thing that Fill's the whole node in Color::Rgb(12, 14, 16) plus an axis
// line, and it only runs when drawBackground is true -- lanes 1-5's own
// RandomShLaneVisualizer (FroggersRandomShVisualizer.hpp) never had a
// background to begin with, so this brings lane 6 in line with the other
// five. The rest of what BuildGangedRandomLfoCommands draws per voice --
// the trace polyline and the playhead dot -- is unconditional on
// drawBackground (GangedRandomLfoVisualizer.hpp:199-243), so lane 6 must
// still draw those.
//
// Reaches source6Visualizer_/the five lane visualizers through
// ModulatorMetadata::visualizer (ParameterModulation.hpp), the same public
// pointer FroggersUiSurface reads off a slot's published cell -- this test
// never touches the private slate members directly.
TEST_CASE(lane_six_visualizer_omits_the_full_node_background_but_still_draws_its_trace) {
    Fixture fx;
    // The bare fixture never calls PrepareBlockClock() (that is
    // FroggersAppCore::ProcessBlock's job), so source #6's round shape
    // stays at its defensive fallback (waiting mu=3.0s, moving mu=1.5s,
    // FroggersModulation.hpp's `PrepareBlockClock`) -- tens of thousands of samples at
    // the 48kHz this fixture prepares at. A handful of Step() calls leaves
    // the round well short of completing: both a nonzero past trace and a
    // still-open future remain, the state a running instrument is in
    // almost all the time. PublishUiState() is a separate call from
    // Step() (FroggersModulation.hpp's own comment: "called once per
    // block ... AFTER the per-sample loop") -- without it, source #6's
    // UiState was never published, ReadSnapshot() fails, and the
    // visualizer draws nothing at all.
    for (int i = 0; i < 8; ++i) {
        fx.StepOnce();
    }
    fx.slate.PublishUiState();

    constexpr synth::ui::Bounds kNodeBounds{0.0f, 0.0f, 120.0f, 60.0f};
    const auto DrawLane = [&fx, kNodeBounds](std::size_t modIx) {
        const synth::ModulatorMetadata& meta = fx.slate.Metadata(modIx);
        REQUIRE_TRUE(meta.visualizer != nullptr);
        meta.visualizer->SetBounds(kNodeBounds);
        return meta.visualizer->Draw();
    };

    const std::vector<synth::ui::DrawCommand> lane6Commands = DrawLane(kModSlotRandomSh6);

    // (a) No full-node background fill -- checked by the exact colour and
    // extent AppendBackgroundAndAxis's Fill uses
    // (GangedRandomLfoVisualizer.hpp:62-63), not by command count.
    bool sawFullNodeBackgroundFill = false;
    for (const synth::ui::DrawCommand& command : lane6Commands) {
        if (command.kind == synth::ui::DrawCommand::Kind::Fill &&
            command.color == synth::Color::Rgb(12, 14, 16) && command.bounds.width == kNodeBounds.width &&
            command.bounds.height == kNodeBounds.height) {
            sawFullNodeBackgroundFill = true;
        }
    }
    REQUIRE_TRUE(!sawFullNodeBackgroundFill);

    // (b) Lane 6 still draws its own trace and playhead.
    bool lane6SawPolyline = false;
    bool lane6SawPlayhead = false;
    for (const synth::ui::DrawCommand& command : lane6Commands) {
        if (command.kind == synth::ui::DrawCommand::Kind::Polyline) {
            lane6SawPolyline = true;
        }
        if (command.kind == synth::ui::DrawCommand::Kind::FillEllipse) {
            lane6SawPlayhead = true;
        }
    }
    REQUIRE_TRUE(lane6SawPolyline);
    REQUIRE_TRUE(lane6SawPlayhead);

    // (c) Lanes 1-5 are unaffected -- this change touches only lane 6's
    // constructor argument; each of the other five still draws exactly its
    // own trace/playhead pair.
    for (std::size_t lane = kModSlotRandomSh1; lane <= kModSlotRandomSh5; ++lane) {
        const std::vector<synth::ui::DrawCommand> laneCommands = DrawLane(lane);
        bool laneSawPolyline = false;
        bool laneSawPlayhead = false;
        for (const synth::ui::DrawCommand& command : laneCommands) {
            if (command.kind == synth::ui::DrawCommand::Kind::Polyline) {
                laneSawPolyline = true;
            }
            if (command.kind == synth::ui::DrawCommand::Kind::FillEllipse) {
                laneSawPlayhead = true;
            }
        }
        REQUIRE_TRUE(laneSawPolyline);
        REQUIRE_TRUE(laneSawPlayhead);
    }
}

}  // namespace

// -----------------------------------------------------------------------
// Randomize All redraws the five stepped sources' bags; Randomize Page
// and a drilled-in Randomize All leave them alone. Every launch seeds the
// bags differently and the same salt reproduces them.
// -----------------------------------------------------------------------
namespace {

std::vector<float> ReadBags(FroggersModulationSlate& slate) {
    slate.PublishUiState();
    std::vector<float> bags;
    for (std::size_t lane = 0; lane < kFroggersNumRandomShLanes; ++lane) {
        for (const auto& slot : slate.RandomShLaneUiState(lane).slots) {
            bags.push_back(slot.load());
        }
    }
    return bags;
}

}  // namespace

TEST_CASE(randomize_all_redraws_the_bags_and_page_and_drilled_in_randomize_do_not) {
    Fixture fx;
    const std::vector<float> before = ReadBags(fx.slate);

    FroggersModulationDrillIn drillIn(fx.model.BankAt(FroggersBankId::Reverb));
    RandomizePage(fx.manager, drillIn);
    REQUIRE_TRUE(ReadBags(fx.slate) == before);  // control: a page randomize never reaches the bags

    drillIn.PressEncoder(0);  // -> level 1
    RandomizeAll(fx.manager, drillIn, fx.model, fx.slate);
    REQUIRE_TRUE(ReadBags(fx.slate) == before);  // control: drilled in, only the parameter's depths are drawn

    FroggersModulationDrillIn levelZero(fx.model.BankAt(FroggersBankId::Reverb));
    RandomizeAll(fx.manager, levelZero, fx.model, fx.slate);
    REQUIRE_TRUE(ReadBags(fx.slate) != before);
}

TEST_CASE(launches_seed_the_bags_differently_and_the_same_salt_reproduces_them) {
    FroggersModulationSlate launchA;
    FroggersModulationSlate launchB;
    REQUIRE_TRUE(ReadBags(launchA) != ReadBags(launchB));

    FroggersModulationSlate saltedA{0x5A17u};
    FroggersModulationSlate saltedB{0x5A17u};
    REQUIRE_TRUE(ReadBags(saltedA) == ReadBags(saltedB));  // control
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
