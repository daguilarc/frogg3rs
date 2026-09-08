// FroggersMidiCatalogTests.cpp -- proves the app's MIDI catalog
// (FroggersMidiCatalog.hpp) actually reaches the screen's own state: every
// catalog action, pushed as a MIDI message on Engine::MidiBus() and dispatched
// through the message thread exactly as a real controller would, moves the
// same observable the screen's own HandleAction branch moves; a MIDI encoder
// push drills in the same way the on-screen press does, and never opens the
// library's own modulation view; the catalog names every action the screen
// routes and nothing else; and the three device defaults validate against the
// library's per-kind support and address exactly the documented controls.
//
// Drives a real synth_froggers::FroggersApp through synth_rig::SynthRig, same
// convention as FroggersSurfaceTests.cpp -- pushing on the MIDI bus and
// letting the rig's own per-block message-thread tick carry the dispatch
// through, rather than calling FroggersUiSurface::HandleAction directly,
// since the MIDI catalog's index-based dispatch (Engine::MessageThreadTick)
// is what is under test here.

#include "Froggers.hpp"
#include "FroggersMidiCatalog.hpp"
#include "FroggersParameters.hpp"
#include "FroggersUiSurface.hpp"
#include "support/SynthRig.hpp"

#include "synth/MidiAppCatalog.hpp"
#include "synth/MidiController.hpp"
#include "synth/ParameterModulation.hpp"

#ifdef JUCE_MAJOR_VERSION
#error "Froggers MIDI catalog tests must not see JUCE headers"
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
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
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

// Same shape as REQUIRE_TRUE, but the failure text names the catalog entry
// (action and value) under test, so a failing entry reads e.g.
// "froggers.bank.select 3: ...".
void RequireForAction(const synth::MidiAppAction& entry, bool condition, const std::string& what) {
    if (!condition) {
        std::ostringstream oss;
        oss << entry.action << " " << entry.value << ": " << what;
        throw std::runtime_error(oss.str());
    }
}

// Looks up one device default by id, for the Launchpad-specific tests below
// that check one named preset at a time rather than walking the whole
// catalog by position. Throws (failing the calling test) when the id is
// missing, so a typo in a preset's id shows up at the lookup site.
const synth::MidiAppDeviceDefault& RequireDeviceDefault(const synth::MidiAppCatalog& catalog,
                                                          const std::string& id) {
    for (const synth::MidiAppDeviceDefault& device : catalog.deviceDefaults) {
        if (device.id == id) {
            return device;
        }
    }
    throw std::runtime_error("device default not found: " + id);
}

struct LaunchpadPresetId {
    const char* id;
    synth::LaunchpadController controller;
};

constexpr LaunchpadPresetId kLaunchpadPresetIds[] = {
    {"froggers.launchpad.x", synth::LaunchpadController::LaunchpadX},
    {"froggers.launchpad.promk3", synth::LaunchpadController::LaunchpadProMk3},
    {"froggers.launchpad.minimk3", synth::LaunchpadController::LaunchpadMiniMk3},
};

synth::RuntimeDataPaths UseScratchRuntimeDataPaths(const char* testName) {
    const std::filesystem::path dataRoot =
        std::filesystem::temp_directory_path() / "froggers-midi-catalog-tests" / testName;
    std::filesystem::remove_all(dataRoot);
    synth::RuntimeDataPaths paths = synth::RuntimeDataPaths::FromDataRoot(dataRoot);
    std::filesystem::create_directories(paths.patchesRoot);
    return paths;
}

// Generous margin for the two-hop settle a dispatched action needs: the
// message-thread tick that runs the app's HandleAction branch (immediate for
// a direct atomic write, e.g. FreezeLatched), then the following block's own
// ProcessFrame()/ProcessBlock() for anything HandleAction only queued (a
// pushed MessageIn::Start/Stop on the UI bus, or a pending Request* the
// audio thread drains), plus the display atomics ProcessBlock() publishes at
// the end of that block.
constexpr std::size_t kSettleBlocks = 6;

using Rig = synth_rig::SynthRig<synth_froggers::FroggersApp>;

void PushAppAction(Rig& rig, std::size_t catalogIx, float value) {
    REQUIRE_TRUE(rig.Engine().MidiBus().Push(synth::MessageIn::AppAction(0, catalogIx, value)));
    rig.RunBlocks(kSettleBlocks);
}

std::vector<float> SnapshotAllParams(synth_froggers::FroggersApp& app) {
    std::vector<float> values;
    values.reserve(synth_froggers::kFroggersBankCount * synth_froggers::kFroggersParamsPerBank);
    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
        for (std::size_t slot = 0; slot < synth_froggers::kFroggersParamsPerBank; ++slot) {
            values.push_back(app.Parameters().PageParameter(bankIx, slot).SceneCenter(0));
        }
    }
    return values;
}

// ---------------------------------------------------------------------------
// midi_app_action_walk_moves_the_state_the_screen_moves
// ---------------------------------------------------------------------------
//
// Observables used per action (the same state HandleAction's own branch
// moves, read straight off the app/engine rather than re-deriving it):
//   Play              -- TransportRunning() true, FreezeLatched() false
//   Stop              -- TransportRunning() false
//   Freeze            -- FreezeLatched() toggles true (Stop, just before it
//                        in catalog order, always clears the latch first)
//   Record            -- after an inline Play push, RecordArmed() true, then
//                        false after a second Record push
//   Randomize All/Page -- some FroggersParameterModel value changes
//   Reset All/Page     -- every checked value returns to its startup-patch
//                        default (RandomizeAll/Page, earlier in the same
//                        catalog order, is what dirtied it)
//   Bank Previous/Next -- ActiveBankIndex() moves by -1/+1 mod bank count
//   Bank N              -- ActiveBankIndex() == N
//   Scene 1/2           -- Manager().Scene().blend reads 0.0/1.0
//   BPM                 -- DisplayTempoBpm() reads the midpoint of
//                          [kFroggersBpmMin, kFroggersBpmMax] for value 0.5
TEST_CASE(midi_app_action_walk_moves_the_state_the_screen_moves) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("app_action_walk"));
    rig.RunBlocks(4);

    synth_froggers::FroggersApp& app = rig.Application();
    const synth::MidiAppCatalog catalog = app.MidiCatalog();

    // Every startup-patch default, captured before any action runs, so the
    // Reset checks below compare against what a fresh launch actually shows.
    const std::vector<float> defaultValues = SnapshotAllParams(app);

    for (std::size_t ix = 0; ix < catalog.actions.size(); ++ix) {
        const synth::MidiAppAction& entry = catalog.actions[ix];

        if (entry.action == synth_froggers::FroggersActions::kPlay) {
            PushAppAction(rig, ix, 0.0f);
            RequireForAction(entry, app.TransportRunning(), "must start the transport");
            RequireForAction(entry, !app.FreezeLatched(), "must clear the Freeze latch");
        } else if (entry.action == synth_froggers::FroggersActions::kStop) {
            PushAppAction(rig, ix, 0.0f);
            RequireForAction(entry, !app.TransportRunning(), "must stop the transport");
        } else if (entry.action == synth_froggers::FroggersActions::kFreeze) {
            PushAppAction(rig, ix, 0.0f);
            RequireForAction(entry, app.FreezeLatched(), "must toggle the Freeze latch on");
        } else if (entry.action == synth_froggers::FroggersActions::kRecord) {
            const std::optional<std::size_t> playIx =
                synth::FindMidiAppAction(catalog, synth_froggers::FroggersActions::kPlay, "");
            REQUIRE_TRUE(playIx.has_value());
            PushAppAction(rig, *playIx, 0.0f);
            RequireForAction(entry, app.TransportRunning(), "Play must be running before Record is tested");

            PushAppAction(rig, ix, 0.0f);
            RequireForAction(entry, app.RecordArmed(), "first Record must arm recording");
            PushAppAction(rig, ix, 0.0f);
            RequireForAction(entry, !app.RecordArmed(), "second Record must stop recording");
        } else if (entry.action == synth_froggers::FroggersActions::kRandomizeAll ||
                   entry.action == synth_froggers::FroggersActions::kRandomizePage) {
            const std::vector<float> before = SnapshotAllParams(app);
            PushAppAction(rig, ix, 0.0f);
            const std::vector<float> after = SnapshotAllParams(app);
            bool anyDifference = false;
            for (std::size_t i = 0; i < before.size(); ++i) {
                if (std::fabs(before[i] - after[i]) > 1.0e-6f) {
                    anyDifference = true;
                    break;
                }
            }
            RequireForAction(entry, anyDifference, "must change at least one parameter value");
        } else if (entry.action == synth_froggers::FroggersActions::kResetAll) {
            float maxAbsDeltaBefore = 0.0f;
            {
                const std::vector<float> current = SnapshotAllParams(app);
                for (std::size_t i = 0; i < current.size(); ++i) {
                    maxAbsDeltaBefore = std::max(maxAbsDeltaBefore, std::fabs(current[i] - defaultValues[i]));
                }
            }
            RequireForAction(entry, maxAbsDeltaBefore > 0.01f,
                              "the preceding Randomize entries must have moved something away from default first");

            PushAppAction(rig, ix, 0.0f);
            float maxAbsDeltaAfter = 0.0f;
            {
                const std::vector<float> current = SnapshotAllParams(app);
                for (std::size_t i = 0; i < current.size(); ++i) {
                    maxAbsDeltaAfter = std::max(maxAbsDeltaAfter, std::fabs(current[i] - defaultValues[i]));
                }
            }
            RequireForAction(entry, maxAbsDeltaAfter < 1.0e-6f, "must return every bank to its default values");
        } else if (entry.action == synth_froggers::FroggersActions::kResetPage) {
            // Dirty the current bank again first -- a reset measured against
            // a page that never moved would prove nothing.
            const std::optional<std::size_t> randomizePageIx =
                synth::FindMidiAppAction(catalog, synth_froggers::FroggersActions::kRandomizePage, "");
            REQUIRE_TRUE(randomizePageIx.has_value());
            PushAppAction(rig, *randomizePageIx, 0.0f);

            const std::size_t bankIx = app.ActiveBankIndex();
            const std::size_t base = bankIx * synth_froggers::kFroggersParamsPerBank;
            float maxAbsDeltaBefore = 0.0f;
            for (std::size_t slot = 0; slot < synth_froggers::kFroggersParamsPerBank; ++slot) {
                maxAbsDeltaBefore = std::max(
                    maxAbsDeltaBefore,
                    std::fabs(app.Parameters().PageParameter(bankIx, slot).SceneCenter(0) - defaultValues[base + slot]));
            }
            RequireForAction(entry, maxAbsDeltaBefore > 0.01f, "Randomize Page must have moved something first");

            PushAppAction(rig, ix, 0.0f);
            float maxAbsDeltaAfter = 0.0f;
            for (std::size_t slot = 0; slot < synth_froggers::kFroggersParamsPerBank; ++slot) {
                maxAbsDeltaAfter = std::max(
                    maxAbsDeltaAfter,
                    std::fabs(app.Parameters().PageParameter(bankIx, slot).SceneCenter(0) - defaultValues[base + slot]));
            }
            RequireForAction(entry, maxAbsDeltaAfter < 1.0e-6f, "must return the current bank to its default values");
        } else if (entry.action == synth_froggers::FroggersActions::kBankPrevious) {
            const std::size_t before = app.ActiveBankIndex();
            PushAppAction(rig, ix, 0.0f);
            const std::size_t expected =
                (before + synth_froggers::kFroggersBankCount - 1) % synth_froggers::kFroggersBankCount;
            RequireForAction(entry, app.ActiveBankIndex() == expected, "must move to the previous bank");
        } else if (entry.action == synth_froggers::FroggersActions::kBankNext) {
            const std::size_t before = app.ActiveBankIndex();
            PushAppAction(rig, ix, 0.0f);
            const std::size_t expected = (before + 1) % synth_froggers::kFroggersBankCount;
            RequireForAction(entry, app.ActiveBankIndex() == expected, "must move to the next bank");
        } else if (entry.action == synth_froggers::FroggersActions::kBankSelect) {
            PushAppAction(rig, ix, 0.0f);
            const std::size_t expected = static_cast<std::size_t>(std::stoul(entry.value));
            RequireForAction(entry, app.ActiveBankIndex() == expected, "must select the named bank");
        } else if (entry.action == synth_froggers::FroggersActions::kSceneSelect) {
            PushAppAction(rig, ix, 0.0f);
            const float expected = entry.value == "0" ? 0.0f : 1.0f;
            RequireForAction(entry, std::fabs(rig.Engine().Manager().Scene().blend - expected) < 1.0e-6f,
                              "must set the scene blend to its named extreme");
        } else if (entry.action == synth_froggers::FroggersActions::kBpm) {
            PushAppAction(rig, ix, 0.5f);
            const double expected = static_cast<double>(synth_froggers::kFroggersBpmMin) +
                                    0.5 * static_cast<double>(synth_froggers::kFroggersBpmMax -
                                                              synth_froggers::kFroggersBpmMin);
            RequireForAction(entry, std::fabs(app.DisplayTempoBpm() - expected) < 0.5,
                              "must set the displayed tempo to the midpoint of its range");
        } else {
            PushAppAction(rig, ix, 0.0f);
            RequireForAction(entry, false, "no observable wired for this action in the test");
        }
    }
}

// ---------------------------------------------------------------------------
// midi_encoder_push_drills_like_the_screen_press
// ---------------------------------------------------------------------------
TEST_CASE(midi_encoder_push_drills_like_the_screen_press) {
    const std::array<std::size_t, 4> expectedLevels = {1, 2, 3, 3};

    // (DrillLevel(), ShowingModulation()) after each of the four presses --
    // recorded for both paths and compared step by step below, rather than
    // asserted against a fixed expectation, so the two paths are proven
    // identical rather than each merely matching a guess. Both are expected
    // to show the modulation view once drilled in (level > 0): that is the
    // correct, in-sync rendering signal, not the desync the catalog fixes
    // (the pre-fix defect was the view opening while DrillLevel() stayed at
    // 0, not the view being open at level > 0).
    using StepPair = std::pair<std::size_t, bool>;
    std::array<StepPair, 4> midiSteps{};
    std::array<StepPair, 4> screenSteps{};

    {
        Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("encoder_push_midi"));
        rig.RunBlocks(4);
        synth_froggers::FroggersApp& app = rig.Application();

        for (std::size_t i = 0; i < expectedLevels.size(); ++i) {
            REQUIRE_TRUE(rig.Engine().MidiBus().Push(synth::MessageIn::ParamPush(0, /*slotIx=*/0, /*position=*/3)));
            rig.RunBlocks(kSettleBlocks);
            REQUIRE_TRUE(app.DrillLevel() == expectedLevels[i]);
            midiSteps[i] = {app.DrillLevel(), app.ActiveDrillIn().BankRef().ShowingModulation()};
        }
    }

    {
        Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("encoder_press_direct"));
        rig.RunBlocks(4);
        synth_froggers::FroggersApp& app = rig.Application();

        for (std::size_t i = 0; i < expectedLevels.size(); ++i) {
            app.RequestEncoderPress(3);
            rig.RunBlocks(kSettleBlocks);
            REQUIRE_TRUE(app.DrillLevel() == expectedLevels[i]);
            screenSteps[i] = {app.DrillLevel(), app.ActiveDrillIn().BankRef().ShowingModulation()};
        }
    }

    for (std::size_t i = 0; i < midiSteps.size(); ++i) {
        std::ostringstream oss;
        oss << "step " << i << ": MIDI push gave (" << midiSteps[i].first << ", "
            << (midiSteps[i].second ? "true" : "false") << ") but the screen's own RequestEncoderPress gave ("
            << screenSteps[i].first << ", " << (screenSteps[i].second ? "true" : "false") << ")";
        if (midiSteps[i] != screenSteps[i]) {
            throw std::runtime_error(oss.str());
        }
    }
}

// ---------------------------------------------------------------------------
// catalog_names_every_front_screen_action
// ---------------------------------------------------------------------------
TEST_CASE(catalog_names_every_front_screen_action) {
    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();

    std::set<std::string> catalogActionNames;
    for (const synth::MidiAppAction& entry : catalog.actions) {
        catalogActionNames.insert(entry.action);
    }

    // Every FroggersActions constant, minus the five that are not
    // MIDI-mappable actions -- each with the reason it is excluded:
    std::set<std::string> screenActionNames = {
        synth_froggers::FroggersActions::kPlay,          synth_froggers::FroggersActions::kStop,
        synth_froggers::FroggersActions::kFreeze,        synth_froggers::FroggersActions::kRecord,
        synth_froggers::FroggersActions::kRandomizeAll,  synth_froggers::FroggersActions::kRandomizePage,
        synth_froggers::FroggersActions::kResetAll,      synth_froggers::FroggersActions::kResetPage,
        synth_froggers::FroggersActions::kBankSelect,    synth_froggers::FroggersActions::kBankPrevious,
        synth_froggers::FroggersActions::kBankNext,      synth_froggers::FroggersActions::kSceneSelect,
        synth_froggers::FroggersActions::kBpm,
    };
    // kSceneBlend      -- offered as the library's own analog Scene Blend kind, not an app action.
    // kEncoderPress    -- the catalog's own encoderPressAction, not a listed action.
    // kEncoderDrag     -- the screen's mouse route into the library's ParamIncDec.
    // kInputSelect     -- the plugin host's input picker, not offered.
    // kViewportNarrow  -- a browser shell flag, not a control.
    REQUIRE_TRUE(screenActionNames.count(synth_froggers::FroggersActions::kSceneBlend) == 0);
    REQUIRE_TRUE(screenActionNames.count(synth_froggers::FroggersActions::kEncoderPress) == 0);
    REQUIRE_TRUE(screenActionNames.count(synth_froggers::FroggersActions::kEncoderDrag) == 0);
    REQUIRE_TRUE(screenActionNames.count(synth_froggers::FroggersActions::kInputSelect) == 0);
    REQUIRE_TRUE(screenActionNames.count(synth_froggers::FroggersActions::kViewportNarrow) == 0);

    REQUIRE_TRUE(catalogActionNames == screenActionNames);
    REQUIRE_TRUE(catalog.encoderPressAction == synth_froggers::FroggersActions::kEncoderPress);
}

// ---------------------------------------------------------------------------
// device_defaults_are_valid_and_address_exactly_the_documented_controls
// ---------------------------------------------------------------------------
TEST_CASE(device_defaults_are_valid_and_address_exactly_the_documented_controls) {
    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();
    REQUIRE_TRUE(catalog.deviceDefaults.size() == 6);

    // Every default must validate against the same per-kind validator the
    // runtime uses (MidiInstrumentConfig::AddController's own check).
    for (const synth::MidiAppDeviceDefault& device : catalog.deviceDefaults) {
        synth::MidiControllerSlot slot;
        slot.name = device.id;
        slot.kind = device.kind;
        slot.config = device.config;
        std::string reason;
        const bool valid = synth::SlotValidForKind(slot, &reason);
        if (!valid) {
            std::cout << "  [" << device.id << "] validator reason: " << reason << "\n";
        }
        REQUIRE_TRUE(valid);
    }

    const synth::MidiAppDeviceDefault& twister = catalog.deviceDefaults[0];
    const synth::MidiAppDeviceDefault& generic = catalog.deviceDefaults[1];
    const synth::MidiAppDeviceDefault& ableton = catalog.deviceDefaults[2];

    REQUIRE_TRUE(twister.id == "froggers.twister");
    REQUIRE_TRUE(generic.id == "froggers.apc40.generic");
    REQUIRE_TRUE(ableton.id == "froggers.apc40.ableton");
    REQUIRE_TRUE(twister.id != generic.id);
    REQUIRE_TRUE(twister.id != ableton.id);
    REQUIRE_TRUE(generic.id != ableton.id);

    // --- Twister -----------------------------------------------------
    REQUIRE_TRUE(twister.kind == synth::MidiProfileKind::MfTwister);
    REQUIRE_TRUE(twister.config.encoderInput.has_value());
    REQUIRE_TRUE(twister.config.encoderInput->turns.size() == 16);
    REQUIRE_TRUE(twister.config.encoderInput->pushes.size() == 16);
    REQUIRE_TRUE(!twister.config.analogInput.has_value());
    REQUIRE_TRUE(twister.config.openSysEx.empty());
    REQUIRE_TRUE(twister.config.systemMessages.size() == 6);

    const std::vector<std::string> twisterOrder = {
        synth_froggers::FroggersActions::kBankNext, synth_froggers::FroggersActions::kPlay,
        synth_froggers::FroggersActions::kFreeze, synth_froggers::FroggersActions::kSceneSelect,
        synth_froggers::FroggersActions::kRandomizePage,
    };
    const std::vector<std::string> twisterValues = {"", "", "", "0", ""};
    const std::vector<std::string> twisterShiftedAction = {
        synth_froggers::FroggersActions::kBankPrevious, synth_froggers::FroggersActions::kStop,
        synth_froggers::FroggersActions::kResetPage, synth_froggers::FroggersActions::kSceneSelect,
        synth_froggers::FroggersActions::kRandomizeAll,
    };
    const std::vector<std::string> twisterShiftedValue = {"", "", "", "1", ""};
    for (std::size_t ix = 0; ix < twisterOrder.size(); ++ix) {
        const synth::MidiControllerSystemMessageAssociation& assoc = twister.config.systemMessages[ix];
        REQUIRE_TRUE(assoc.control.has_value());
        REQUIRE_TRUE(assoc.control->channel == 3);
        REQUIRE_TRUE(assoc.control->cc == static_cast<std::uint8_t>(8 + ix));
        REQUIRE_TRUE(assoc.control->type == synth::MidiControlType::Cc);
        REQUIRE_TRUE(assoc.appAction == twisterOrder[ix]);
        REQUIRE_TRUE(assoc.appActionValue == twisterValues[ix]);
        REQUIRE_TRUE(assoc.outputFeedback == false);
        REQUIRE_TRUE(assoc.shiftedPress.has_value());
        REQUIRE_TRUE(assoc.shiftedPress->type == synth::MessageIn::Type::AppAction);
        REQUIRE_TRUE(assoc.shiftedAppAction == twisterShiftedAction[ix]);
        REQUIRE_TRUE(assoc.shiftedAppActionValue == twisterShiftedValue[ix]);
        REQUIRE_TRUE(synth::FindMidiAppAction(catalog, assoc.appAction, assoc.appActionValue).has_value());
        REQUIRE_TRUE(
            synth::FindMidiAppAction(catalog, assoc.shiftedAppAction, assoc.shiftedAppActionValue).has_value());
    }

    const synth::MidiControllerSystemMessageAssociation& twisterShift = twister.config.systemMessages[5];
    REQUIRE_TRUE(twisterShift.control.has_value());
    REQUIRE_TRUE(twisterShift.control->channel == 3);
    REQUIRE_TRUE(twisterShift.control->cc == 13);
    REQUIRE_TRUE(twisterShift.control->type == synth::MidiControlType::Cc);
    REQUIRE_TRUE(twisterShift.press.type == synth::MessageIn::Type::Shift);
    REQUIRE_TRUE(twisterShift.press.boolValue == true);
    REQUIRE_TRUE(twisterShift.release.has_value());
    REQUIRE_TRUE(twisterShift.release->type == synth::MessageIn::Type::Shift);
    REQUIRE_TRUE(twisterShift.release->boolValue == false);
    REQUIRE_TRUE(!twisterShift.shiftedPress.has_value());
    REQUIRE_TRUE(twisterShift.outputFeedback == false);

    REQUIRE_TRUE(std::count(catalog.libraryKinds.begin(), catalog.libraryKinds.end(),
                            synth::UISystemMessage::Shift) == 1);

    // --- the two APC40 defaults ---------------------------------------
    REQUIRE_TRUE(generic.kind == synth::MidiProfileKind::Generic);
    REQUIRE_TRUE(ableton.kind == synth::MidiProfileKind::Generic);
    REQUIRE_TRUE(generic.inputAliases == ableton.inputAliases);
    REQUIRE_TRUE(generic.outputAliases == ableton.outputAliases);

    using ControlTuple = std::tuple<int, int, int, std::string, std::string>;
    const int kNote = static_cast<int>(synth::MidiControlType::Note);

    std::set<ControlTuple> expectedSystemMessages;
    expectedSystemMessages.insert({0, 98, kNote, "", ""});  // SHIFT -> Hold Drill (a library kind, no app action).
    expectedSystemMessages.insert({0, 91, kNote, synth_froggers::FroggersActions::kPlay, ""});
    expectedSystemMessages.insert({0, 92, kNote, synth_froggers::FroggersActions::kStop, ""});
    expectedSystemMessages.insert({0, 93, kNote, synth_froggers::FroggersActions::kRecord, ""});
    expectedSystemMessages.insert({0, 82, kNote, synth_froggers::FroggersActions::kSceneSelect, "0"});
    expectedSystemMessages.insert({0, 83, kNote, synth_froggers::FroggersActions::kSceneSelect, "1"});
    expectedSystemMessages.insert({0, 97, kNote, synth_froggers::FroggersActions::kBankPrevious, ""});
    expectedSystemMessages.insert({0, 96, kNote, synth_froggers::FroggersActions::kBankNext, ""});
    expectedSystemMessages.insert({0, 62, kNote, synth_froggers::FroggersActions::kRandomizePage, ""});
    expectedSystemMessages.insert({0, 63, kNote, synth_froggers::FroggersActions::kRandomizeAll, ""});
    expectedSystemMessages.insert({0, 64, kNote, synth_froggers::FroggersActions::kResetPage, ""});
    expectedSystemMessages.insert({0, 65, kNote, synth_froggers::FroggersActions::kResetAll, ""});
    expectedSystemMessages.insert({0, 81, kNote, synth_froggers::FroggersActions::kFreeze, ""});
    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
        expectedSystemMessages.insert(
            {static_cast<int>(bankIx), 52, kNote, synth_froggers::FroggersActions::kBankSelect, std::to_string(bankIx)});
    }

    for (const synth::MidiAppDeviceDefault* device : {&generic, &ableton}) {
        REQUIRE_TRUE(device->config.encoderInput.has_value());
        REQUIRE_TRUE(device->config.encoderInput->mode == synth::EncoderMode::Absolute);
        REQUIRE_TRUE(device->config.encoderInput->turns.size() == 16);
        REQUIRE_TRUE(device->config.encoderInput->pushes.empty());
        for (std::size_t ix = 0; ix < 8; ++ix) {
            const synth::EncoderMidiMapping& mapping = device->config.encoderInput->turns[ix];
            REQUIRE_TRUE(mapping.control.channel == 0);
            REQUIRE_TRUE(mapping.control.cc == static_cast<std::uint8_t>(48 + ix));
            REQUIRE_TRUE(mapping.position == ix);
        }
        for (std::size_t ix = 0; ix < 8; ++ix) {
            const synth::EncoderMidiMapping& mapping = device->config.encoderInput->turns[8 + ix];
            REQUIRE_TRUE(mapping.control.channel == 0);
            REQUIRE_TRUE(mapping.control.cc == static_cast<std::uint8_t>(16 + ix));
            REQUIRE_TRUE(mapping.position == 8 + ix);
        }

        REQUIRE_TRUE(device->config.analogInput.has_value());
        REQUIRE_TRUE(device->config.analogInput->sceneBlend.has_value());
        REQUIRE_TRUE(device->config.analogInput->sceneBlend->channel == 0);
        REQUIRE_TRUE(device->config.analogInput->sceneBlend->cc == 15);
        REQUIRE_TRUE(device->config.analogInput->appActions.size() == 1);
        REQUIRE_TRUE(device->config.analogInput->appActions[0].control.channel == 0);
        REQUIRE_TRUE(device->config.analogInput->appActions[0].control.cc == 14);
        REQUIRE_TRUE(device->config.analogInput->appActions[0].appAction == synth_froggers::FroggersActions::kBpm);

        REQUIRE_TRUE(device->config.systemMessages.size() == 19);
        std::set<ControlTuple> actual;
        bool foundHoldDrill = false;
        for (const synth::MidiControllerSystemMessageAssociation& assoc : device->config.systemMessages) {
            REQUIRE_TRUE(assoc.control.has_value());
            REQUIRE_TRUE(assoc.control->type == synth::MidiControlType::Note);
            REQUIRE_TRUE(!assoc.shiftedPress.has_value());
            actual.insert({assoc.control->channel, assoc.control->cc, static_cast<int>(assoc.control->type),
                           assoc.appAction, assoc.appActionValue});
            if (assoc.control->channel == 0 && assoc.control->cc == 98) {
                foundHoldDrill = true;
                REQUIRE_TRUE(assoc.press.type == synth::MessageIn::Type::HoldDrill);
                REQUIRE_TRUE(assoc.press.boolValue == true);
                REQUIRE_TRUE(assoc.release.has_value());
                REQUIRE_TRUE(assoc.release->type == synth::MessageIn::Type::HoldDrill);
                REQUIRE_TRUE(assoc.release->boolValue == false);
            }
        }
        REQUIRE_TRUE(foundHoldDrill);
        REQUIRE_TRUE(actual == expectedSystemMessages);
    }

    REQUIRE_TRUE(generic.config.openSysEx.empty());
    const std::vector<std::vector<std::uint8_t>> expectedSysEx = {
        {0xF0, 0x47, 0x7F, 0x29, 0x60, 0x00, 0x04, 0x41, 0x09, 0x07, 0x01, 0xF7}};
    REQUIRE_TRUE(ableton.config.openSysEx == expectedSysEx);
}

// ---------------------------------------------------------------------------
// launchpad_defaults_open_sysex_is_programmer_mode
// ---------------------------------------------------------------------------
TEST_CASE(launchpad_defaults_open_sysex_is_programmer_mode) {
    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();

    const std::vector<std::vector<std::uint8_t>> expectedX = {
        {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0C, 0x0E, 0x01, 0xF7}};
    REQUIRE_TRUE(RequireDeviceDefault(catalog, "froggers.launchpad.x").config.openSysEx == expectedX);

    const std::vector<std::vector<std::uint8_t>> expectedProMk3 = {
        {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0E, 0x00, 0x11, 0x00, 0x00, 0xF7}};
    REQUIRE_TRUE(RequireDeviceDefault(catalog, "froggers.launchpad.promk3").config.openSysEx == expectedProMk3);

    const std::vector<std::vector<std::uint8_t>> expectedMiniMk3 = {
        {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0x01, 0xF7}};
    REQUIRE_TRUE(RequireDeviceDefault(catalog, "froggers.launchpad.minimk3").config.openSysEx == expectedMiniMk3);
}

// ---------------------------------------------------------------------------
// launchpad_defaults_positions_carry_their_own_controller
// ---------------------------------------------------------------------------
TEST_CASE(launchpad_defaults_positions_carry_their_own_controller) {
    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();

    for (const LaunchpadPresetId& preset : kLaunchpadPresetIds) {
        const synth::MidiAppDeviceDefault& device = RequireDeviceDefault(catalog, preset.id);
        REQUIRE_TRUE(!device.config.systemMessages.empty());
        for (const synth::MidiControllerSystemMessageAssociation& assoc : device.config.systemMessages) {
            REQUIRE_TRUE(assoc.launchpadPosition.has_value());
            REQUIRE_TRUE(assoc.launchpadPosition->controller == preset.controller);
            REQUIRE_TRUE(!assoc.shiftedPress.has_value());
        }
    }
}

// ---------------------------------------------------------------------------
// launchpad_defaults_pad_actions_resolve_against_the_catalog
// ---------------------------------------------------------------------------
TEST_CASE(launchpad_defaults_pad_actions_resolve_against_the_catalog) {
    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();

    for (const LaunchpadPresetId& preset : kLaunchpadPresetIds) {
        const synth::MidiAppDeviceDefault& device = RequireDeviceDefault(catalog, preset.id);
        for (const synth::MidiControllerSystemMessageAssociation& assoc : device.config.systemMessages) {
            const std::optional<std::size_t> ix =
                synth::FindMidiAppAction(catalog, assoc.appAction, assoc.appActionValue);
            if (!ix.has_value()) {
                std::ostringstream oss;
                oss << preset.id << ": pad action does not resolve against the catalog: \"" << assoc.appAction
                    << "\" \"" << assoc.appActionValue << "\"";
                throw std::runtime_error(oss.str());
            }
        }
    }
}

// ---------------------------------------------------------------------------
// launchpad_defaults_bank_column_covers_every_bank
// ---------------------------------------------------------------------------
TEST_CASE(launchpad_defaults_bank_column_covers_every_bank) {
    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();

    for (const LaunchpadPresetId& preset : kLaunchpadPresetIds) {
        const synth::MidiAppDeviceDefault& device = RequireDeviceDefault(catalog, preset.id);
        std::size_t bankPadCount = 0;
        for (const synth::MidiControllerSystemMessageAssociation& assoc : device.config.systemMessages) {
            if (assoc.appAction != synth_froggers::FroggersActions::kBankSelect) {
                continue;
            }
            REQUIRE_TRUE(assoc.launchpadPosition.has_value());
            REQUIRE_TRUE(assoc.launchpadPosition->x == 8);
            REQUIRE_TRUE(assoc.launchpadPosition->y == static_cast<int>(bankPadCount));
            REQUIRE_TRUE(assoc.appActionValue == std::to_string(bankPadCount));
            ++bankPadCount;
        }
        REQUIRE_TRUE(bankPadCount == synth_froggers::kFroggersBankCount);
    }
}

// ---------------------------------------------------------------------------
// launchpad_defaults_are_registered_with_expected_ids_and_kind
// ---------------------------------------------------------------------------
TEST_CASE(launchpad_defaults_are_registered_with_expected_ids_and_kind) {
    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();

    for (const LaunchpadPresetId& preset : kLaunchpadPresetIds) {
        const synth::MidiAppDeviceDefault& device = RequireDeviceDefault(catalog, preset.id);
        REQUIRE_TRUE(device.kind == synth::MidiProfileKind::Launchpad);
    }
    REQUIRE_TRUE(RequireDeviceDefault(catalog, "froggers.launchpad.x").displayName == "Launchpad X");
    REQUIRE_TRUE(RequireDeviceDefault(catalog, "froggers.launchpad.promk3").displayName == "Launchpad Pro MK3");
    REQUIRE_TRUE(RequireDeviceDefault(catalog, "froggers.launchpad.minimk3").displayName == "Launchpad Mini MK3");
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
