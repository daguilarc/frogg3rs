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

#include "synth/ControllerWizard.hpp"
#include "synth/MidiAppCatalog.hpp"
#include "synth/MidiConfigViewModel.hpp"
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
#include <span>
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
// A gesture's own CC-driven value smooths toward its target rather than
// jumping (see the gesture's own analog input smoothing), so a reading
// taken shortly after moving the Gestures CC can sit a hair off its settled
// limit; GetRaw comparisons that follow a CC move use this instead of exact
// equality, while SceneCenter (a plain committed write, never smoothed)
// keeps exact comparisons throughout.
bool NearlyEqual(float a, float b, float epsilon = 1e-3f) {
    return std::fabs(a - b) < epsilon;
}

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
// bus drain for anything HandleAction only queued (a pushed MessageIn::
// Start/Stop/AppCommand the audio thread applies), plus the display atomics
// ProcessBlock() publishes at the end of that block.
constexpr std::size_t kSettleBlocks = 6;

using Rig = synth_rig::SynthRig<synth_froggers::FroggersApp>;

void PushAppAction(Rig& rig, std::size_t catalogIx, float value) {
    REQUIRE_TRUE(rig.Engine().MidiBus().Push(synth::MessageIn::AppAction(0, catalogIx, value)));
    rig.RunBlocks(kSettleBlocks);
}

std::vector<float> SnapshotAllParams(synth_froggers::FroggersApp& app) {
    std::vector<float> values;
    values.reserve(synth_froggers::kFroggersPageCount * synth_froggers::kFroggersParamsPerBank);
    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersPageCount; ++bankIx) {
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
//   Play              -- the engine's clock diagnostics read Running,
//                        FreezeLatched() false
//   Stop              -- the engine's clock diagnostics read Stopped
//   Freeze            -- FreezeLatched() toggles true (Stop, just before it
//                        in catalog order, always clears the latch first)
//   Record            -- after an inline Play push, RecordArmed() true, then
//                        false after a second Record push
//   Randomize All/Page -- some FroggersParameterModel value changes
//   Reset All/Page     -- every checked value returns to its startup-patch
//                        default (RandomizeAll/Page, earlier in the same
//                        catalog order, is what dirtied it)
//   Page Previous/Next -- ActivePageIndex() moves by -1/+1 mod page count
//   Page N              -- ActivePageIndex() == N
//   Scene 1/2           -- Manager().Scene().blend reads 0.0/1.0
//   BPM                 -- the engine's clock diagnostics read the midpoint
//                          of [kFroggersBpmMin, kFroggersBpmMax] for value
//                          0.5
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
            RequireForAction(entry, synth_froggers::FroggersTransportIsRunning(&rig.Engine().Context()),
                              "must start the transport");
            RequireForAction(entry, !app.FreezeLatched(), "must clear the Freeze latch");
        } else if (entry.action == synth_froggers::FroggersActions::kStop) {
            PushAppAction(rig, ix, 0.0f);
            RequireForAction(entry, !synth_froggers::FroggersTransportIsRunning(&rig.Engine().Context()),
                              "must stop the transport");
        } else if (entry.action == synth_froggers::FroggersActions::kFreeze) {
            PushAppAction(rig, ix, 0.0f);
            RequireForAction(entry, app.FreezeLatched(), "must toggle the Freeze latch on");
        } else if (entry.action == synth_froggers::FroggersActions::kRecord) {
            const std::optional<std::size_t> playIx =
                synth::FindMidiAppAction(catalog, synth_froggers::FroggersActions::kPlay, "");
            REQUIRE_TRUE(playIx.has_value());
            PushAppAction(rig, *playIx, 0.0f);
            RequireForAction(entry, synth_froggers::FroggersTransportIsRunning(&rig.Engine().Context()),
                              "Play must be running before Record is tested");

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
            RequireForAction(entry, maxAbsDeltaAfter < 1.0e-6f, "must return every page to its default values");
        } else if (entry.action == synth_froggers::FroggersActions::kResetPage) {
            // Dirty the current page again first -- a reset measured against
            // a page that never moved would prove nothing.
            const std::optional<std::size_t> randomizePageIx =
                synth::FindMidiAppAction(catalog, synth_froggers::FroggersActions::kRandomizePage, "");
            REQUIRE_TRUE(randomizePageIx.has_value());
            PushAppAction(rig, *randomizePageIx, 0.0f);

            const std::size_t bankIx = app.ActivePageIndex();
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
            RequireForAction(entry, maxAbsDeltaAfter < 1.0e-6f, "must return the current page to its default values");
        } else if (entry.action == synth_froggers::FroggersActions::kPagePrevious) {
            const std::size_t before = app.ActivePageIndex();
            PushAppAction(rig, ix, 0.0f);
            const std::size_t expected =
                (before + synth_froggers::kFroggersPageCount - 1) % synth_froggers::kFroggersPageCount;
            RequireForAction(entry, app.ActivePageIndex() == expected, "must move to the previous page");
        } else if (entry.action == synth_froggers::FroggersActions::kPageNext) {
            const std::size_t before = app.ActivePageIndex();
            PushAppAction(rig, ix, 0.0f);
            const std::size_t expected = (before + 1) % synth_froggers::kFroggersPageCount;
            RequireForAction(entry, app.ActivePageIndex() == expected, "must move to the next page");
        } else if (entry.action == synth_froggers::FroggersActions::kPageSelect) {
            PushAppAction(rig, ix, 0.0f);
            const std::size_t expected = static_cast<std::size_t>(std::stoul(entry.value));
            RequireForAction(entry, app.ActivePageIndex() == expected, "must select the named page");
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
            RequireForAction(entry,
                              std::fabs(rig.Engine().Context().clockDiagnostics->Snapshot().currentBpm - expected) <
                                  0.5,
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
            app.PortableSurface().DispatchAction(
                synth::ui::Action::WithValue(synth_froggers::FroggersActions::kEncoderPress, "3"));
            rig.RunBlocks(kSettleBlocks);
            REQUIRE_TRUE(app.DrillLevel() == expectedLevels[i]);
            screenSteps[i] = {app.DrillLevel(), app.ActiveDrillIn().BankRef().ShowingModulation()};
        }
    }

    for (std::size_t i = 0; i < midiSteps.size(); ++i) {
        std::ostringstream oss;
        oss << "step " << i << ": MIDI push gave (" << midiSteps[i].first << ", "
            << (midiSteps[i].second ? "true" : "false") << ") but the screen's own encoder press gave ("
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
        synth_froggers::FroggersActions::kPageSelect,    synth_froggers::FroggersActions::kPagePrevious,
        synth_froggers::FroggersActions::kPageNext,      synth_froggers::FroggersActions::kSceneSelect,
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
// catalog_names_the_bpm_action_as_its_tempo_action
// ---------------------------------------------------------------------------
TEST_CASE(catalog_names_the_bpm_action_as_its_tempo_action) {
    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();
    REQUIRE_TRUE(catalog.tempoAction == synth_froggers::FroggersActions::kBpm);

    const std::optional<std::size_t> tempoActionIx =
        synth::FindMidiAppAction(catalog, catalog.tempoAction, "");
    REQUIRE_TRUE(tempoActionIx.has_value());
    const synth::MidiAppAction& tempoAction = catalog.actions[*tempoActionIx];
    REQUIRE_TRUE(tempoAction.analogRange.has_value());
    REQUIRE_TRUE(tempoAction.analogRange->first == synth_froggers::kFroggersBpmMin);
    REQUIRE_TRUE(tempoAction.analogRange->second == synth_froggers::kFroggersBpmMax);
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

    // Exactly two Twister turns carry a shifted job: Crunchy's, Scene
    // blend, and Crispy's, Tempo. Every other turn and every push carries
    // none.
    std::size_t twisterShiftedTurnCount = 0;
    std::optional<synth::EncoderShiftedJob> crunchyShiftedJob;
    std::optional<synth::EncoderShiftedJob> crispyShiftedJob;
    for (const synth::EncoderMidiMapping& turn : twister.config.encoderInput->turns) {
        if (turn.position == synth_froggers::kFroggersCrunchySlot) {
            crunchyShiftedJob = turn.shiftedJob;
        } else if (turn.position == synth_froggers::kFroggersCrispySlot) {
            crispyShiftedJob = turn.shiftedJob;
        } else {
            REQUIRE_TRUE(turn.shiftedJob == synth::EncoderShiftedJob::None);
        }
        if (turn.shiftedJob != synth::EncoderShiftedJob::None) {
            ++twisterShiftedTurnCount;
        }
    }
    REQUIRE_TRUE(twisterShiftedTurnCount == 2);
    REQUIRE_TRUE(crunchyShiftedJob.has_value() && *crunchyShiftedJob == synth::EncoderShiftedJob::SceneBlend);
    REQUIRE_TRUE(crispyShiftedJob.has_value() && *crispyShiftedJob == synth::EncoderShiftedJob::TempoBpm);
    // Printed explicitly, not inferred from the count above: a loop that
    // assigns one slot and silently drops the other must not pass unnoticed.
    // What is printed is what the REQUIRE_TRUE calls above actually proved
    // for each slot -- its exact shifted job, not merely that it is set --
    // since a bare non-None check would be strictly weaker than that.
    std::cout << "Twister Crunchy slot (" << synth_froggers::kFroggersCrunchySlot
              << ") shiftedJob == SceneBlend: "
              << (*crunchyShiftedJob == synth::EncoderShiftedJob::SceneBlend) << "\n";
    std::cout << "Twister Crispy slot (" << synth_froggers::kFroggersCrispySlot
              << ") shiftedJob == TempoBpm: "
              << (*crispyShiftedJob == synth::EncoderShiftedJob::TempoBpm) << "\n";
    for (const synth::EncoderMidiMapping& push : twister.config.encoderInput->pushes) {
        REQUIRE_TRUE(push.shiftedJob == synth::EncoderShiftedJob::None);
    }

    REQUIRE_TRUE(!twister.config.analogInput.has_value());
    REQUIRE_TRUE(twister.config.openSysEx.empty());
    REQUIRE_TRUE(twister.config.systemMessages.size() == 6);

    const std::vector<std::string> twisterOrder = {
        synth_froggers::FroggersActions::kPageNext, synth_froggers::FroggersActions::kPlay,
        synth_froggers::FroggersActions::kFreeze, synth_froggers::FroggersActions::kSceneSelect,
        synth_froggers::FroggersActions::kRandomizePage,
    };
    const std::vector<std::string> twisterValues = {"", "", "", "0", ""};
    const std::vector<std::string> twisterShiftedAction = {
        synth_froggers::FroggersActions::kPagePrevious, synth_froggers::FroggersActions::kStop,
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
    expectedSystemMessages.insert({0, 97, kNote, synth_froggers::FroggersActions::kPagePrevious, ""});
    expectedSystemMessages.insert({0, 96, kNote, synth_froggers::FroggersActions::kPageNext, ""});
    expectedSystemMessages.insert({0, 62, kNote, synth_froggers::FroggersActions::kRandomizePage, ""});
    expectedSystemMessages.insert({0, 63, kNote, synth_froggers::FroggersActions::kRandomizeAll, ""});
    expectedSystemMessages.insert({0, 64, kNote, synth_froggers::FroggersActions::kResetPage, ""});
    expectedSystemMessages.insert({0, 65, kNote, synth_froggers::FroggersActions::kResetAll, ""});
    expectedSystemMessages.insert({0, 81, kNote, synth_froggers::FroggersActions::kFreeze, ""});
    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersPageCount; ++bankIx) {
        expectedSystemMessages.insert(
            {static_cast<int>(bankIx), 52, kNote, synth_froggers::FroggersActions::kPageSelect, std::to_string(bankIx)});
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
            REQUIRE_TRUE(mapping.shiftedJob == synth::EncoderShiftedJob::None);
        }
        for (std::size_t ix = 0; ix < 8; ++ix) {
            const synth::EncoderMidiMapping& mapping = device->config.encoderInput->turns[8 + ix];
            REQUIRE_TRUE(mapping.control.channel == 0);
            REQUIRE_TRUE(mapping.control.cc == static_cast<std::uint8_t>(16 + ix));
            REQUIRE_TRUE(mapping.position == 8 + ix);
            REQUIRE_TRUE(mapping.shiftedJob == synth::EncoderShiftedJob::None);
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
// twister_shift_turns_crunchys_knob_into_the_scene_blend
// ---------------------------------------------------------------------------
//
// Installs the real Twister device default as the app's own controller slot
// 0 and feeds it exactly the raw bytes a Twister sends (SendMidi reaches
// Engine::MidiInputProcessor(0), the same per-controller chain a connected
// device's port would feed): Shift down (channel 3 CC 13, 127), encoder 16
// turned clockwise (channel 0 CC 15, 65 -- decodes as +1 step, per
// EncoderMidiInProcessor::DecodeDelta's Signed7Bit ticks = value - 64), Shift
// up (0), then the same encoder turned clockwise again.
TEST_CASE(twister_shift_turns_crunchys_knob_into_the_scene_blend) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("twister_shift_crunchy"));
    rig.RunBlocks(4);
    synth_froggers::FroggersApp& app = rig.Application();

    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();
    const synth::MidiAppDeviceDefault& twister = RequireDeviceDefault(catalog, "froggers.twister");

    synth::MidiControllerSlot slot;
    slot.name = twister.id;
    slot.kind = twister.kind;
    slot.config = twister.config;
    synth::MidiInstrumentConfig instrument;
    REQUIRE_TRUE(instrument.AddController(std::move(slot)));
    rig.InstallInstrumentForTest(std::move(instrument));

    constexpr float kTurnStep = 1.0f / 128.0f;  // EncoderMidiInConfig's default turnStep
    const float blendBefore = rig.Engine().Manager().Scene().blend;
    const float crunchyBefore = app.Parameters().Crunchy().GetRaw(0);

    rig.SendMidi(0, synth::BasicMidi::CC(0, 3, 13, 127));  // Shift down
    rig.SendMidi(0, synth::BasicMidi::CC(0, 0, 15, 65));   // encoder 16 clockwise, shifted
    rig.RunBlocks(kSettleBlocks);

    const float blendAfterShiftedTurn = rig.Engine().Manager().Scene().blend;
    REQUIRE_TRUE(blendAfterShiftedTurn == std::clamp(blendBefore + kTurnStep, 0.0f, 1.0f));
    REQUIRE_TRUE(app.Parameters().Crunchy().GetRaw(0) == crunchyBefore);

    rig.SendMidi(0, synth::BasicMidi::CC(0, 3, 13, 0));   // Shift up
    rig.SendMidi(0, synth::BasicMidi::CC(0, 0, 15, 65));  // encoder 16 clockwise, unshifted
    rig.RunBlocks(kSettleBlocks);

    REQUIRE_TRUE(rig.Engine().Manager().Scene().blend == blendAfterShiftedTurn);
    REQUIRE_TRUE(app.Parameters().Crunchy().GetRaw(0) > crunchyBefore);
}

// ---------------------------------------------------------------------------
// twister_shift_turns_crispys_knob_into_the_tempo
// ---------------------------------------------------------------------------
//
// Same convention as twister_shift_turns_crunchys_knob_into_the_scene_blend,
// on the encoder that moves Crispy (position kFroggersCrispySlot, channel 0
// CC 14).
TEST_CASE(twister_shift_turns_crispys_knob_into_the_tempo) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("twister_shift_crispy"));
    rig.RunBlocks(4);
    synth_froggers::FroggersApp& app = rig.Application();

    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();
    const synth::MidiAppDeviceDefault& twister = RequireDeviceDefault(catalog, "froggers.twister");

    synth::MidiControllerSlot slot;
    slot.name = twister.id;
    slot.kind = twister.kind;
    slot.config = twister.config;
    synth::MidiInstrumentConfig instrument;
    REQUIRE_TRUE(instrument.AddController(std::move(slot)));
    rig.InstallInstrumentForTest(std::move(instrument));

    const double tempoBefore = rig.Engine().Clock().TempoBpm();
    const float crispyBefore =
        app.Parameters().Crispy(synth_froggers::FroggersBankId::Audio).GetRaw(0);

    rig.SendMidi(0, synth::BasicMidi::CC(0, 3, 13, 127));  // Shift down
    rig.SendMidi(0, synth::BasicMidi::CC(0, 0, 14, 65));   // encoder 15 clockwise, shifted
    rig.RunBlocks(kSettleBlocks);

    const double tempoAfterShiftedTurn = rig.Engine().Clock().TempoBpm();
    REQUIRE_TRUE(tempoAfterShiftedTurn > tempoBefore);
    REQUIRE_TRUE(app.Parameters().Crispy(synth_froggers::FroggersBankId::Audio).GetRaw(0) == crispyBefore);

    rig.SendMidi(0, synth::BasicMidi::CC(0, 3, 13, 0));   // Shift up
    rig.SendMidi(0, synth::BasicMidi::CC(0, 0, 14, 65));  // encoder 15 clockwise, unshifted
    rig.RunBlocks(kSettleBlocks);

    REQUIRE_TRUE(rig.Engine().Clock().TempoBpm() == tempoAfterShiftedTurn);
    REQUIRE_TRUE(app.Parameters().Crispy(synth_froggers::FroggersBankId::Audio).GetRaw(0) > crispyBefore);
}

// ---------------------------------------------------------------------------
// twister_shifted_tempo_turn_stops_at_each_end_of_the_range
// ---------------------------------------------------------------------------
TEST_CASE(twister_shifted_tempo_turn_stops_at_each_end_of_the_range) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("twister_shift_tempo_range"));
    rig.RunBlocks(4);

    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();
    const synth::MidiAppDeviceDefault& twister = RequireDeviceDefault(catalog, "froggers.twister");

    synth::MidiControllerSlot slot;
    slot.name = twister.id;
    slot.kind = twister.kind;
    slot.config = twister.config;
    synth::MidiInstrumentConfig instrument;
    REQUIRE_TRUE(instrument.AddController(std::move(slot)));
    rig.InstallInstrumentForTest(std::move(instrument));

    REQUIRE_TRUE(rig.Engine().Clock().SetTempoBpm(synth_froggers::kFroggersBpmMin));
    rig.SendMidi(0, synth::BasicMidi::CC(0, 3, 13, 127));  // Shift down
    rig.SendMidi(0, synth::BasicMidi::CC(0, 0, 14, 63));   // encoder 15 counter-clockwise, shifted
    rig.RunBlocks(kSettleBlocks);
    REQUIRE_TRUE(rig.Engine().Clock().TempoBpm() == synth_froggers::kFroggersBpmMin);

    rig.SendMidi(0, synth::BasicMidi::CC(0, 0, 14, 65));  // encoder 15 clockwise, shifted
    rig.RunBlocks(kSettleBlocks);
    REQUIRE_TRUE(rig.Engine().Clock().TempoBpm() > synth_froggers::kFroggersBpmMin);

    REQUIRE_TRUE(rig.Engine().Clock().SetTempoBpm(synth_froggers::kFroggersBpmMax));
    rig.SendMidi(0, synth::BasicMidi::CC(0, 0, 14, 65));  // encoder 15 clockwise, shifted
    rig.RunBlocks(kSettleBlocks);
    REQUIRE_TRUE(rig.Engine().Clock().TempoBpm() == synth_froggers::kFroggersBpmMax);
}

// ---------------------------------------------------------------------------
// twister_shifted_tempo_turn_moves_the_tempo_the_same_on_every_page
// ---------------------------------------------------------------------------
TEST_CASE(twister_shifted_tempo_turn_moves_the_tempo_the_same_on_every_page) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("twister_shift_tempo_pages"));
    rig.RunBlocks(4);
    synth_froggers::FroggersApp& app = rig.Application();

    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();
    const synth::MidiAppDeviceDefault& twister = RequireDeviceDefault(catalog, "froggers.twister");

    synth::MidiControllerSlot slot;
    slot.name = twister.id;
    slot.kind = twister.kind;
    slot.config = twister.config;
    synth::MidiInstrumentConfig instrument;
    REQUIRE_TRUE(instrument.AddController(std::move(slot)));
    rig.InstallInstrumentForTest(std::move(instrument));

    const double tempoBefore = rig.Engine().Clock().TempoBpm();
    const float audioCrispyBefore =
        app.Parameters().Crispy(synth_froggers::FroggersBankId::Audio).GetRaw(0);

    rig.SendMidi(0, synth::BasicMidi::CC(0, 3, 13, 127));  // Shift down
    rig.SendMidi(0, synth::BasicMidi::CC(0, 0, 14, 65));   // encoder 15 clockwise, shifted
    rig.RunBlocks(kSettleBlocks);
    const double firstRise = rig.Engine().Clock().TempoBpm() - tempoBefore;
    REQUIRE_TRUE(firstRise > 0.0);

    rig.SendMidi(0, synth::BasicMidi::CC(0, 3, 13, 0));  // Shift up before switching pages
    rig.RunBlocks(kSettleBlocks);

    const double tempoBeforeSecondTurn = rig.Engine().Clock().TempoBpm();
    rig.SendMidi(0, synth::BasicMidi::CC(0, 3, 8, 127));  // Page Next, unshifted
    rig.RunBlocks(kSettleBlocks);

    rig.SendMidi(0, synth::BasicMidi::CC(0, 3, 13, 127));  // Shift down
    rig.SendMidi(0, synth::BasicMidi::CC(0, 0, 14, 65));   // encoder 15 clockwise, shifted
    rig.RunBlocks(kSettleBlocks);
    const double secondRise = rig.Engine().Clock().TempoBpm() - tempoBeforeSecondTurn;
    REQUIRE_TRUE(std::fabs(secondRise - firstRise) < 1e-6);

    rig.SendMidi(0, synth::BasicMidi::CC(0, 3, 13, 0));  // Shift up
    rig.SendMidi(0, synth::BasicMidi::CC(0, 0, 14, 65));  // encoder 15 clockwise, unshifted, new page
    rig.RunBlocks(kSettleBlocks);

    REQUIRE_TRUE(app.Parameters().Crispy(synth_froggers::FroggersBankId::Audio).GetRaw(0) == audioCrispyBefore);
}

// ---------------------------------------------------------------------------
// twister_row_mapped_to_gesture_1_fires_the_gesture_on_the_rig
// ---------------------------------------------------------------------------
//
// The Twister default's six side buttons are all assigned; this repurposes
// the Shift button (channel 3 CC 13) as Hold Gesture Select for gesture 1
// through the same view-model route BuildHeldGestureButtonFixture uses for
// a Custom controller's rows, so this exercises the real preset-editing
// path a Twister row would take on this app. Holding it selects gesture 1
// (never gesture 0, which a gestureIx defaulted to 0 would also satisfy),
// releasing it deselects. Fails with the row left unmapped (message kind
// reverted to its Shift default): the CC then does nothing to
// SelectedGestureMask().
TEST_CASE(twister_row_mapped_to_gesture_1_fires_the_gesture_on_the_rig) {
    using Field = synth::MidiMappingRowVM::Field;

    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();
    const std::vector<synth::UISystemMessageChoice> messageCatalog = synth::MakeUISystemMessageChoices(catalog);
    const auto holdGestureSelectIt = std::find_if(
        messageCatalog.begin(), messageCatalog.end(),
        [](const synth::UISystemMessageChoice& choice) {
            return choice.message == synth::UISystemMessage::HoldGestureSelect;
        });
    REQUIRE_TRUE(holdGestureSelectIt != messageCatalog.end());
    const double holdGestureSelectMessageIx =
        static_cast<double>(std::distance(messageCatalog.begin(), holdGestureSelectIt));

    const synth::MidiAppDeviceDefault& twister = RequireDeviceDefault(catalog, "froggers.twister");
    synth::MidiControllerSlot slot;
    slot.name = twister.id;
    slot.kind = twister.kind;
    slot.config = twister.config;
    synth::MidiInstrumentConfig instrument;
    REQUIRE_TRUE(instrument.AddController(std::move(slot)));

    synth::MidiConnectionState connection;
    connection.controllers.push_back({});

    synth::MidiConfigViewModel vm;
    vm.SetMessageCatalog(messageCatalog);
    vm.Rebuild(instrument, connection);

    // Row 5 is the Shift button (channel 3, CC 13) in TwisterDeviceDefault's
    // own systemMessages order (FroggersMidiCatalog.hpp). Both edits below
    // apply to this same open row presentation -- Rebuild() updates the
    // model's persisted snapshot but does not reshape an already-open row,
    // so the second edit runs against the same vm without an intervening
    // Rebuild.
    constexpr std::size_t kShiftRowIx = 5;
    std::string reason;
    synth::MidiInstrumentConfig withKind;
    REQUIRE_TRUE(vm.ApplyMappingEdit(0, synth::MidiConfigSection::SystemMessages, kShiftRowIx, Field::MessageKind,
                                      holdGestureSelectMessageIx, withKind, &reason));

    synth::MidiInstrumentConfig withGesture1;
    REQUIRE_TRUE(vm.ApplyMappingEdit(0, synth::MidiConfigSection::SystemMessages, kShiftRowIx, Field::MessageArg,
                                      1.0, withGesture1, &reason));

    // Found by address rather than by kShiftRowIx: ApplyMappingEdit writes
    // its output config with edited rows reordered ahead of untouched
    // stock ones, so the row's position in `withGesture1` is not
    // kShiftRowIx itself.
    const auto& outputRows = withGesture1.controllers[0].config.systemMessages;
    const auto rowIt = std::find_if(outputRows.begin(), outputRows.end(),
                                     [](const synth::MidiControllerSystemMessageAssociation& candidate) {
                                         return candidate.control.has_value() && candidate.control->channel == 3 &&
                                                candidate.control->cc == 13;
                                     });
    REQUIRE_TRUE(rowIt != outputRows.end());
    const synth::MidiControllerSystemMessageAssociation& row = *rowIt;
    REQUIRE_TRUE(row.press.gestureIx == 1);

    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("twister_gesture_1"));
    rig.RunBlocks(4);
    rig.InstallInstrumentForTest(withGesture1);

    REQUIRE_TRUE(rig.Engine().Manager().SelectedGestureMask() == 0u);
    rig.SendMidi(0, synth::BasicMidi::CC(0, 3, 13, 127));
    rig.RunBlocks(kSettleBlocks);
    REQUIRE_TRUE(rig.Engine().Manager().SelectedGestureMask() == (synth::GestureMask{1} << 1));

    rig.SendMidi(0, synth::BasicMidi::CC(0, 3, 13, 0));
    rig.RunBlocks(kSettleBlocks);
    REQUIRE_TRUE(rig.Engine().Manager().SelectedGestureMask() == 0u);
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
// launchpad_defaults_page_column_covers_every_page
// ---------------------------------------------------------------------------
TEST_CASE(launchpad_defaults_page_column_covers_every_page) {
    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();

    for (const LaunchpadPresetId& preset : kLaunchpadPresetIds) {
        const synth::MidiAppDeviceDefault& device = RequireDeviceDefault(catalog, preset.id);
        std::size_t pagePadCount = 0;
        for (const synth::MidiControllerSystemMessageAssociation& assoc : device.config.systemMessages) {
            if (assoc.appAction != synth_froggers::FroggersActions::kPageSelect) {
                continue;
            }
            REQUIRE_TRUE(assoc.launchpadPosition.has_value());
            REQUIRE_TRUE(assoc.launchpadPosition->x == 8);
            REQUIRE_TRUE(assoc.launchpadPosition->y == static_cast<int>(pagePadCount));
            REQUIRE_TRUE(assoc.appActionValue == std::to_string(pagePadCount));
            ++pagePadCount;
        }
        REQUIRE_TRUE(pagePadCount == synth_froggers::kFroggersPageCount);
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
        // The model the profile records is what the Controllers page's Variant
        // selector shows and what a row added to this preset is stamped from,
        // so a preset that left it at the default would read as a Launchpad X
        // row however its pads are addressed.
        REQUIRE_TRUE(device.config.launchpadModel == preset.controller);
    }
    REQUIRE_TRUE(RequireDeviceDefault(catalog, "froggers.launchpad.x").displayName == "Launchpad X");
    REQUIRE_TRUE(RequireDeviceDefault(catalog, "froggers.launchpad.promk3").displayName == "Launchpad Pro MK3");
    REQUIRE_TRUE(RequireDeviceDefault(catalog, "froggers.launchpad.minimk3").displayName == "Launchpad Mini MK3");
}

// Builds a Custom (Generic-kind) controller with one System row (Hold
// Gesture Select, gesture 0) and one Analogs Gestures row (gesture 0),
// through the view model's own AddSingle/ApplyMappingEdit route -- the same
// production path the Controllers page's own "+" buttons and combo edits
// dispatch. Returns the two rows' own channel/cc addresses (read back from
// the built config, not assumed), so a caller can drive them with
// SynthRig::SendMidi.
struct HeldGestureButtonFixture {
    synth::MidiInstrumentConfig instrument;
    synth::MidiControlAddress systemAddress;   // Hold Gesture Select row
    synth::MidiControlAddress analogAddress;   // Gestures row (gesture 0)
};

HeldGestureButtonFixture BuildHeldGestureButtonFixture() {
    using RowGroup = synth::MidiMappingRowVM::RowGroup;
    using Field = synth::MidiMappingRowVM::Field;

    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();
    const std::vector<synth::UISystemMessageChoice> messageCatalog = synth::MakeUISystemMessageChoices(catalog);
    const auto holdGestureSelectIt = std::find_if(
        messageCatalog.begin(), messageCatalog.end(),
        [](const synth::UISystemMessageChoice& choice) {
            return choice.message == synth::UISystemMessage::HoldGestureSelect;
        });
    if (holdGestureSelectIt == messageCatalog.end()) {
        throw std::runtime_error("catalog offers no Hold Gesture Select choice");
    }
    const double holdGestureSelectMessageIx =
        static_cast<double>(std::distance(messageCatalog.begin(), holdGestureSelectIt));

    synth::MidiInstrumentConfig instrument;
    synth::MidiControllerSlot slot;
    slot.name = "custom";
    slot.kind = synth::MidiProfileKind::Generic;
    if (!instrument.AddController(std::move(slot))) {
        throw std::runtime_error("could not add a Custom controller");
    }
    synth::MidiConnectionState connection;
    connection.controllers.push_back({});

    synth::MidiConfigViewModel vm;
    vm.SetMessageCatalog(messageCatalog);
    vm.Rebuild(instrument, connection);

    // A System row, Hold Gesture Select, gesture 0 (the row's own default
    // argument -- see a_gesture_row_past_the_app_gestures_is_refused above).
    synth::MidiInstrumentConfig afterSystemAdd;
    std::string reason;
    if (!vm.AddSingle(0, synth::MidiConfigSection::SystemMessages, RowGroup::System, afterSystemAdd, &reason)) {
        throw std::runtime_error("AddSingle(System) refused: " + reason);
    }
    vm.SetMessageCatalog(messageCatalog);
    vm.Rebuild(afterSystemAdd, connection);

    synth::MidiInstrumentConfig withKind;
    if (!vm.ApplyMappingEdit(0, synth::MidiConfigSection::SystemMessages, 0, Field::MessageKind,
                             holdGestureSelectMessageIx, withKind, &reason)) {
        throw std::runtime_error("ApplyMappingEdit(MessageKind) refused: " + reason);
    }
    vm.SetMessageCatalog(messageCatalog);
    vm.Rebuild(withKind, connection);

    // An Analogs Gestures row, gesture 0, on a different CC. The System and
    // Analog sections each pick their own row's default address
    // independently (NextFreeGenericAddress/NextFreeCc, MidiConfigViewModel.cpp),
    // starting from channel 0 cc 0 in both -- so a fresh Generic controller's
    // first row of each kind collides on the same wire address; moved to
    // channel 1 here so SynthRig::SendMidi can address each row on its own.
    synth::MidiInstrumentConfig withAnalogAdded;
    if (!vm.AddSingle(0, synth::MidiConfigSection::Analogs, RowGroup::AnalogGesture, withAnalogAdded, &reason)) {
        throw std::runtime_error("AddSingle(AnalogGesture) refused: " + reason);
    }
    vm.SetMessageCatalog(messageCatalog);
    vm.Rebuild(withAnalogAdded, connection);

    synth::MidiInstrumentConfig withAnalog;
    if (!vm.ApplyMappingEdit(0, synth::MidiConfigSection::Analogs, 0, Field::Channel, 1.0, withAnalog, &reason)) {
        throw std::runtime_error("ApplyMappingEdit(Analog Channel) refused: " + reason);
    }

    const synth::MidiControllerSystemMessageAssociation& systemRow = withAnalog.controllers[0].config.systemMessages[0];
    if (!systemRow.control.has_value()) {
        throw std::runtime_error("Hold Gesture Select row has no control address");
    }
    const synth::AnalogMidiMapping& analogRow = withAnalog.controllers[0].config.analogInput->gestures[0];
    if (analogRow.gestureIx != 0) {
        throw std::runtime_error("Analogs Gestures row did not default to gesture 0");
    }

    HeldGestureButtonFixture fixture;
    fixture.instrument = std::move(withAnalog);
    fixture.systemAddress = *systemRow.control;
    fixture.analogAddress = analogRow.control;
    return fixture;
}

// ---------------------------------------------------------------------------
// held_gesture_button_collects_knobs_and_the_gesture_control_moves_them
// ---------------------------------------------------------------------------
//
// Through the production route (BuildHeldGestureButtonFixture, above): a
// System row bound to Hold Gesture Select for gesture 0, and an Analogs
// Gestures row bound to gesture 0. Knobs A and B (bank Audio, positions 0
// and 1) are turned twice each through the UI bus while the button is held
// and the gesture value is at 1.0 (Gestures CC 127) -- the first turn only
// arms membership (Parameter::HandleIncDec returns after arming, so it
// moves nothing), the second turn moves the gesture's own target since the
// gesture's weight is 1.0 there. Knob C (position 2) is never turned and
// never joins. Fails with the catalog's HoldGestureSelect line removed
// (BuildHeldGestureButtonFixture then throws, since AddSingle/
// ApplyMappingEdit can never reach that message kind).
TEST_CASE(held_gesture_button_collects_knobs_and_the_gesture_control_moves_them) {
    const HeldGestureButtonFixture fixture = BuildHeldGestureButtonFixture();

    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("held_gesture_button"));
    rig.RunBlocks(4);
    rig.InstallInstrumentForTest(fixture.instrument);

    synth_froggers::FroggersApp& app = rig.Application();
    // Envelope, not Audio: the Audio bank's default patch carries six
    // factory cross-VCO pitch-modulation depths (LCH-03), which would make
    // GetRaw drift on its own and defeat a settled reading; Envelope's page
    // parameters carry no default depths.
    constexpr std::size_t kBankIx = 1;  // Envelope
    constexpr std::size_t kSlotIx = 0;  // SynthRig::Turn's physical slot -- Frogg3rs has one.
    rig.SelectBank(kSlotIx, kBankIx);
    rig.RunBlocks(4);
    synth::Parameter& knobA = app.Parameters().PageParameter(kBankIx, 0);
    synth::Parameter& knobB = app.Parameters().PageParameter(kBankIx, 1);
    synth::Parameter& knobC = app.Parameters().PageParameter(kBankIx, 2);

    // Extra settle margin after moving the Gestures CC: the gesture's own
    // value smooths toward its new target rather than jumping, so a reading
    // taken right at kSettleBlocks can still be a hair short of the limit.
    constexpr std::size_t kGestureSettleBlocks = 60;

    // SceneCenter is the unweighted base a turn commands; GetRaw is what the
    // screen reads -- SceneCenter blended with the gesture target by the
    // gesture's own weight (Parameter::ComputeRawCenter). At weight 1.0
    // (Gestures CC 127) a member's GetRaw tracks its gesture target and
    // SceneCenter never moves; at weight 0.0 (CC 0) GetRaw collapses back to
    // SceneCenter.
    const float preA = knobA.GetRaw(0);
    const float preB = knobB.GetRaw(0);
    const float preC = knobC.GetRaw(0);
    REQUIRE_TRUE(knobA.SceneCenter(0) == preA);

    rig.SendMidi(0, synth::BasicMidi::CC(0, fixture.analogAddress.channel, fixture.analogAddress.cc, 127));
    rig.SendMidi(0, synth::BasicMidi::CC(0, fixture.systemAddress.channel, fixture.systemAddress.cc, 127));
    rig.RunBlocks(kGestureSettleBlocks);
    REQUIRE_TRUE(rig.Engine().Manager().SelectedGestureMask() == 1u);

    rig.Turn(kSlotIx, 0, 0.1f);  // A: arms, moves nothing
    rig.RunBlocks(kSettleBlocks);
    REQUIRE_TRUE(knobA.GestureActive(0, 0));
    REQUIRE_TRUE(NearlyEqual(knobA.GetRaw(0), preA));
    rig.Turn(kSlotIx, 0, 0.1f);  // A: second turn, weight 1.0 -> moves the gesture target only
    rig.RunBlocks(kSettleBlocks);

    rig.Turn(kSlotIx, 1, 0.1f);  // B: arms, moves nothing
    rig.RunBlocks(kSettleBlocks);
    rig.Turn(kSlotIx, 1, 0.1f);  // B: second turn
    rig.RunBlocks(kSettleBlocks);

    rig.SendMidi(0, synth::BasicMidi::CC(0, fixture.systemAddress.channel, fixture.systemAddress.cc, 0));  // release
    rig.RunBlocks(kSettleBlocks);

    const float aAt127 = knobA.GetRaw(0);
    const float bAt127 = knobB.GetRaw(0);
    REQUIRE_TRUE(!NearlyEqual(aAt127, preA));
    REQUIRE_TRUE(!NearlyEqual(bAt127, preB));
    REQUIRE_TRUE(NearlyEqual(knobC.GetRaw(0), preC));
    // Neither member's own base ever moved -- every turn's delta went to
    // the gesture target instead, since the gesture's weight was 1.0
    // throughout both members' turns.
    REQUIRE_TRUE(knobA.SceneCenter(0) == preA);
    REQUIRE_TRUE(knobB.SceneCenter(0) == preB);

    rig.SendMidi(0, synth::BasicMidi::CC(0, fixture.analogAddress.channel, fixture.analogAddress.cc, 0));
    rig.RunBlocks(kGestureSettleBlocks);
    REQUIRE_TRUE(NearlyEqual(knobA.GetRaw(0), preA));
    REQUIRE_TRUE(NearlyEqual(knobB.GetRaw(0), preB));
    REQUIRE_TRUE(NearlyEqual(knobC.GetRaw(0), preC));

    // With the button released and the gesture back at full weight, A moves
    // again -- membership, once joined, is shared by every later turn
    // whether the button is held or not.
    rig.SendMidi(0, synth::BasicMidi::CC(0, fixture.analogAddress.channel, fixture.analogAddress.cc, 127));
    rig.RunBlocks(kGestureSettleBlocks);
    rig.Turn(kSlotIx, 0, 0.1f);
    rig.RunBlocks(kSettleBlocks);
    REQUIRE_TRUE(!NearlyEqual(knobA.GetRaw(0), aAt127));
    REQUIRE_TRUE(knobA.SceneCenter(0) == preA);

    rig.SendMidi(0, synth::BasicMidi::CC(0, fixture.analogAddress.channel, fixture.analogAddress.cc, 0));
    rig.RunBlocks(kGestureSettleBlocks);
    REQUIRE_TRUE(NearlyEqual(knobA.GetRaw(0), preA));
}

// ---------------------------------------------------------------------------
// randomize_with_a_gesture_button_held_adds_what_it_writes
// ---------------------------------------------------------------------------
//
// Through the same held-button setup as the case above, then
// synth_froggers::detail::RandomizeParameterModulationDepths -- the actual
// production function a modulation view's own Randomize All calls per
// visible parameter -- on knob A directly. A depth it materializes is a
// brand-new Parameter whose first-ever write is this randomize call, so
// Parameter::HandleIncDec's "first write only arms" rule (exercised above)
// applies to it identically: the drawn depths join gesture 0 and their own
// SceneCenter stays neutral (0.0f). This case goes red if the drawn depths
// are written through SceneCenter (a plain assignment) instead of through
// RandomizeVisibleValue/HandleIncDec's arm-then-share route.
TEST_CASE(randomize_with_a_gesture_button_held_adds_what_it_writes) {
    const HeldGestureButtonFixture fixture = BuildHeldGestureButtonFixture();

    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("randomize_gesture_held"));
    rig.RunBlocks(4);
    rig.InstallInstrumentForTest(fixture.instrument);

    synth_froggers::FroggersApp& app = rig.Application();
    constexpr std::size_t kBankIx = 1;  // Envelope
    constexpr std::size_t kSlotIx = 0;
    rig.SelectBank(kSlotIx, kBankIx);
    rig.RunBlocks(4);
    synth::Parameter& knobA = app.Parameters().PageParameter(kBankIx, 0);
    REQUIRE_TRUE(!knobA.GestureActive(0, 0));

    rig.SendMidi(0, synth::BasicMidi::CC(0, fixture.analogAddress.channel, fixture.analogAddress.cc, 127));
    rig.SendMidi(0, synth::BasicMidi::CC(0, fixture.systemAddress.channel, fixture.systemAddress.cc, 127));
    rig.RunBlocks(kSettleBlocks);
    REQUIRE_TRUE(rig.Engine().Manager().SelectedGestureMask() == 1u);

    const bool partial = synth_froggers::detail::RandomizeParameterModulationDepths(rig.Engine().Manager(), knobA,
                                                                                   /*minimumSources=*/1);
    REQUIRE_TRUE(!partial);
    rig.RunBlocks(kSettleBlocks);

    const std::span<const synth::ModulatorMetadata> metadata = knobA.Group().GetModulators().Metadata();
    std::size_t materialized = 0;
    for (std::size_t modIx = 0; modIx < metadata.size(); ++modIx) {
        synth::Parameter* depth = knobA.ModulationDepthParameter(modIx);
        if (depth == nullptr) {
            continue;
        }
        ++materialized;
        // The randomize's own draw is this depth's first-ever write: it
        // joins gesture 0 and stays neutral
        // (synth_froggers::detail::kNeutralModulationDepthCenter, a depth's
        // own "no effect" center) in both scenes -- the arm-only rule, not a
        // value change.
        REQUIRE_TRUE(depth->GestureActive(0, 0));
        REQUIRE_TRUE(depth->GestureActive(1, 0));
        REQUIRE_TRUE(depth->SceneCenter(0) == synth_froggers::detail::kNeutralModulationDepthCenter);
        REQUIRE_TRUE(depth->SceneCenter(1) == synth_froggers::detail::kNeutralModulationDepthCenter);
    }
    REQUIRE_TRUE(materialized > 0);

    // The gesture as a whole moved nothing on knob A itself: the randomize
    // targeted the depths, not knob A's own base.
    REQUIRE_TRUE(!knobA.GestureActive(0, 0));
}

// ---------------------------------------------------------------------------
// gesture_is_part_of_the_patch
// ---------------------------------------------------------------------------
//
// Builds gesture 0 with knobs A and B as members (same held-button, double-
// turn sequence as held_gesture_button_..., above), saves through the File
// page's own route (SynthRig::SavePatchAs, the same PatchManager call the
// page's Save dispatches -- SynthRig.hpp's own header comment), and opens
// it through the File page's Load (SynthRig::LoadPatch) in a second, fresh
// rig that never saw this instrument. Checked directly against
// Parameter::GestureActive/GestureValue -- the two fields
// Parameter::ToValueJSON writes into a patch (gesture-trace.md, "What it
// stores") -- rather than a live MIDI reading, so the assertion is exactly
// what the patch file carries, independent of the fresh rig's own (absent)
// controller mapping.
TEST_CASE(gesture_is_part_of_the_patch) {
    const HeldGestureButtonFixture fixture = BuildHeldGestureButtonFixture();

    const std::filesystem::path patchDir =
        std::filesystem::temp_directory_path() / "froggers-midi-catalog-tests" / "gesture_is_part_of_the_patch";
    std::filesystem::remove_all(patchDir);  // SavePatchAs requires patchDir to not already exist.

    Rig builder(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("gesture_is_part_of_the_patch_builder"));
    builder.RunBlocks(4);
    builder.InstallInstrumentForTest(fixture.instrument);

    constexpr std::size_t kBankIx = 1;  // Envelope
    constexpr std::size_t kSlotIx = 0;
    builder.SelectBank(kSlotIx, kBankIx);
    builder.RunBlocks(4);
    synth::Parameter& knobA = builder.Application().Parameters().PageParameter(kBankIx, 0);
    synth::Parameter& knobB = builder.Application().Parameters().PageParameter(kBankIx, 1);

    builder.SendMidi(0, synth::BasicMidi::CC(0, fixture.systemAddress.channel, fixture.systemAddress.cc, 127));
    builder.RunBlocks(kSettleBlocks);
    REQUIRE_TRUE(builder.Engine().Manager().SelectedGestureMask() == 1u);

    builder.Turn(kSlotIx, 0, 0.1f);
    builder.RunBlocks(kSettleBlocks);
    builder.Turn(kSlotIx, 0, 0.1f);
    builder.RunBlocks(kSettleBlocks);
    builder.Turn(kSlotIx, 1, 0.1f);
    builder.RunBlocks(kSettleBlocks);
    builder.Turn(kSlotIx, 1, 0.1f);
    builder.RunBlocks(kSettleBlocks);

    builder.SendMidi(0, synth::BasicMidi::CC(0, fixture.systemAddress.channel, fixture.systemAddress.cc, 0));
    builder.RunBlocks(kSettleBlocks);

    REQUIRE_TRUE(knobA.GestureActive(0, 0));
    REQUIRE_TRUE(knobB.GestureActive(0, 0));
    const float gestureValueA = knobA.GestureValue(0, 0);
    const float gestureValueB = knobB.GestureValue(0, 0);

    const synth_rig::RigPatchStatus saveStatus = builder.SavePatchAs(patchDir);
    REQUIRE_TRUE(saveStatus == synth_rig::RigPatchStatus::Written);
    const std::optional<std::filesystem::path> versionFile = synth::LatestPatchVersion(patchDir);
    REQUIRE_TRUE(versionFile.has_value());

    Rig opener(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("gesture_is_part_of_the_patch_opener"));
    opener.RunBlocks(4);
    const synth_rig::RigPatchStatus loadStatus = opener.LoadPatch(*versionFile);
    REQUIRE_TRUE(loadStatus == synth_rig::RigPatchStatus::Ok);

    synth::Parameter& openedA = opener.Application().Parameters().PageParameter(kBankIx, 0);
    synth::Parameter& openedB = opener.Application().Parameters().PageParameter(kBankIx, 1);
    REQUIRE_TRUE(openedA.GestureActive(0, 0));
    REQUIRE_TRUE(openedB.GestureActive(0, 0));
    REQUIRE_TRUE(openedA.GestureValue(0, 0) == gestureValueA);
    REQUIRE_TRUE(openedB.GestureValue(0, 0) == gestureValueB);
}

// ---------------------------------------------------------------------------
// gesture_colours_are_distinct_visible_and_not_modulator_colours
// ---------------------------------------------------------------------------
//
// Reads the eight colours FroggersParameterModel::Init actually installs
// (through the real ParameterManager::GestureMetadataAt accessor, not the
// FroggersGestureColor() helper's own return values, so this exercises the
// wiring, not just the palette table). Fails with the assignment loop in
// app/FroggersParameters.hpp removed, since every gesture then keeps
// GestureMetadata::gestureColor's own default, Color::Off, which collapses
// every "pairwise distinct" and "none is Off" case at once.
TEST_CASE(gesture_colours_are_distinct_visible_and_not_modulator_colours) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("gesture_colours"));
    rig.RunBlocks(4);

    synth::ParameterManager& manager = rig.Engine().Manager();
    std::array<synth::Color, synth_froggers::FroggersParameterModel::kNumGestures> colors{};
    for (std::size_t gestureIx = 0; gestureIx < colors.size(); ++gestureIx) {
        colors[gestureIx] = manager.GestureMetadataAt(gestureIx).gestureColor;
        REQUIRE_TRUE(colors[gestureIx] != synth::Color::Off);
        REQUIRE_TRUE(colors[gestureIx] != synth::kSurfaceBackground);
    }
    for (std::size_t i = 0; i < colors.size(); ++i) {
        for (std::size_t j = i + 1; j < colors.size(); ++j) {
            REQUIRE_TRUE(colors[i] != colors[j]);
        }
    }

    // The one group Froggers creates (app.Parameters().Init's own comment);
    // any parameter reaches the same modulator metadata.
    synth::Parameter& anyParameter = rig.Application().Parameters().PageParameter(0, 0);
    const std::span<const synth::ModulatorMetadata> metadata = anyParameter.Group().GetModulators().Metadata();
    for (const synth::Color& gestureColor : colors) {
        for (const synth::ModulatorMetadata& source : metadata) {
            REQUIRE_TRUE(gestureColor != source.sourceColor);
        }
    }
}

// ---------------------------------------------------------------------------
// a_knob_in_a_gesture_draws_its_gesture_dot
// ---------------------------------------------------------------------------
//
// Joins knob A to gesture 0 directly (Parameter::SetGestureActive, the same
// flag Parameter::HandleIncDec's arm step sets), then builds the parameter
// page's own draw commands the production route builds
// (Parameter::UIState -> synth::ui::EncoderDrawStateFromParameter ->
// synth::ui::BuildEncoderDrawCommands, FroggersUiSurface.hpp's own
// AppendEncoderCell), and asserts one of them (the gesture badge
// AppendBadge appends, EncoderDraw.hpp) is filled in gesture 0's own colour
// (its r/g/b -- AppendBadge scales the badge's own alpha by 0.9, so alpha
// is not compared). Fails with the colour assignment removed: gesture 0's
// colour is then Color::Off (0,0,0,255), the same r/g/b as the badge's own
// black stroke and near-black shell, but never as its OWN FillRoundedRect
// (see AppendBadge), so no command actually matches Off either -- proving
// the assertion needs a real, non-default colour, not merely "some colour."
TEST_CASE(a_knob_in_a_gesture_draws_its_gesture_dot) {
    Rig rig(/*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("gesture_badge_draw"));
    rig.RunBlocks(4);

    constexpr std::size_t kBankIx = 1;  // Envelope
    constexpr std::size_t kSlotIx = 0;
    rig.SelectBank(kSlotIx, kBankIx);
    rig.RunBlocks(4);

    synth::Parameter& knobA = rig.Application().Parameters().PageParameter(kBankIx, 0);
    knobA.SetGestureActive(0, 0, true);
    knobA.SetGestureActive(1, 0, true);
    rig.RunBlocks(kSettleBlocks);

    const synth::ParameterManager::UIState& uiState = rig.UIState();
    const synth::Parameter::UIState& cell = uiState.slots[kSlotIx].cells[0];
    const synth::ui::EncoderDrawState drawState = synth::ui::EncoderDrawStateFromParameter(cell);
    REQUIRE_TRUE(drawState.gesturesAffectingMask != 0);
    REQUIRE_TRUE(!drawState.gestureColors.empty());
    const synth::Color gesture0Color = drawState.gestureColors[0];
    REQUIRE_TRUE(gesture0Color != synth::Color::Off);

    const synth::ui::Bounds nodeExtent{0.0f, 0.0f, 120.0f, 120.0f};
    const std::vector<synth::ui::DrawCommand> commands = synth::ui::BuildEncoderDrawCommands(drawState, nodeExtent);
    REQUIRE_TRUE(!commands.empty());

    bool foundGestureBadge = false;
    for (const synth::ui::DrawCommand& command : commands) {
        if (command.color.r == gesture0Color.r && command.color.g == gesture0Color.g &&
            command.color.b == gesture0Color.b) {
            foundGestureBadge = true;
            break;
        }
    }
    REQUIRE_TRUE(foundGestureBadge);
}

// ---------------------------------------------------------------------------
// catalog_offers_hold_gesture_select
// ---------------------------------------------------------------------------
//
// FroggersControllersPageTests.cpp's own convention for a System row's
// offered targets: SetMessageCatalog(MakeUISystemMessageChoices(catalog))
// against a real synth::MidiConfigViewModel, then read the choices back.
// Fails with the FroggersMidiCatalog.hpp libraryKinds line above removed,
// since HoldGestureSelect is then absent from every catalog.libraryKinds
// entry MakeUISystemMessageChoices draws from.
TEST_CASE(catalog_offers_hold_gesture_select) {
    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();

    synth::MidiConfigViewModel vm;
    vm.SetMessageCatalog(synth::MakeUISystemMessageChoices(catalog));

    const std::vector<synth::UISystemMessageChoice>& choices = vm.MessageCatalog();
    const bool offersHoldGestureSelect =
        std::find_if(choices.begin(), choices.end(), [](const synth::UISystemMessageChoice& choice) {
            return choice.message == synth::UISystemMessage::HoldGestureSelect;
        }) != choices.end();
    REQUIRE_TRUE(offersHoldGestureSelect);
}

// ---------------------------------------------------------------------------
// a_gesture_row_past_the_app_gestures_is_refused
// ---------------------------------------------------------------------------
//
// Sheaf 3.2's cap (viewmodel_tests.cpp's GestureFieldPastTheAppsGesturesIsRefused
// / GestureCountCapRefusesABlockAnAddAndAHoldGestureSelectArgument), driven
// through Frogg3rs's own gesture count instead of a hand-picked one: 7.3(b)'s
// record showed gesture 8 accepted with no count set, so this proves the app
// actually wires FroggersParameterModel::kNumGestures into the view model
// that refuses it now that Sheaf 3.2 has landed.
TEST_CASE(a_gesture_row_past_the_app_gestures_is_refused) {
    using RowGroup = synth::MidiMappingRowVM::RowGroup;
    using Field = synth::MidiMappingRowVM::Field;

    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();
    const std::vector<synth::UISystemMessageChoice> messageCatalog = synth::MakeUISystemMessageChoices(catalog);
    const auto holdGestureSelectIx = std::find_if(
        messageCatalog.begin(), messageCatalog.end(),
        [](const synth::UISystemMessageChoice& choice) {
            return choice.message == synth::UISystemMessage::HoldGestureSelect;
        });
    REQUIRE_TRUE(holdGestureSelectIx != messageCatalog.end());
    const double holdGestureSelectMessageIx =
        static_cast<double>(std::distance(messageCatalog.begin(), holdGestureSelectIx));

    // (a) A Gestures row's GestureIx field refuses 8.
    {
        synth::MidiInstrumentConfig instrument;
        synth::MidiControllerSlot slot;
        slot.name = "custom";
        slot.kind = synth::MidiProfileKind::Generic;
        synth::AnalogMidiInConfig analog;
        analog.gestures.push_back(synth::AnalogMidiMapping{
            .control = synth::MidiControlAddress{.channel = 0, .cc = 0}, .gestureIx = 0});
        slot.config.analogInput = analog;
        REQUIRE_TRUE(instrument.AddController(std::move(slot)));
        synth::MidiConnectionState connection;
        connection.controllers.push_back({});

        synth::MidiConfigViewModel vm;
        vm.Rebuild(instrument, connection);
        vm.SetGestureCount(synth_froggers::FroggersParameterModel::kNumGestures);

        synth::MidiInstrumentConfig out;
        std::string reason;
        REQUIRE_TRUE(!vm.ApplyMappingEdit(0, synth::MidiConfigSection::Analogs, 0, Field::GestureIx, 8.0, out,
                                          &reason));
        REQUIRE_TRUE(reason == "gesture must be an integer 0-7");
        REQUIRE_TRUE(out.controllers.empty());
        REQUIRE_TRUE(instrument.controllers[0].config.analogInput->gestures[0].gestureIx == 0);
    }

    // (b) A Hold Gesture Select row's argument refuses 8.
    {
        synth::MidiInstrumentConfig instrument;
        synth::MidiControllerSlot slot;
        slot.name = "custom";
        slot.kind = synth::MidiProfileKind::Generic;
        REQUIRE_TRUE(instrument.AddController(std::move(slot)));
        synth::MidiConnectionState connection;
        connection.controllers.push_back({});

        synth::MidiConfigViewModel vm;
        vm.SetMessageCatalog(messageCatalog);
        vm.Rebuild(instrument, connection);

        synth::MidiInstrumentConfig afterAdd;
        std::string reason;
        REQUIRE_TRUE(vm.AddSingle(0, synth::MidiConfigSection::SystemMessages, RowGroup::System, afterAdd, &reason));
        vm.SetMessageCatalog(messageCatalog);
        vm.Rebuild(afterAdd, connection);

        synth::MidiInstrumentConfig withKind;
        REQUIRE_TRUE(vm.ApplyMappingEdit(0, synth::MidiConfigSection::SystemMessages, 0, Field::MessageKind,
                                         holdGestureSelectMessageIx, withKind, &reason));
        vm.SetMessageCatalog(messageCatalog);
        vm.Rebuild(withKind, connection);
        vm.SetGestureCount(synth_froggers::FroggersParameterModel::kNumGestures);

        synth::MidiInstrumentConfig out;
        REQUIRE_TRUE(!vm.ApplyMappingEdit(0, synth::MidiConfigSection::SystemMessages, 0, Field::MessageArg, 8.0, out,
                                          &reason));
        REQUIRE_TRUE(reason == "gesture must be an integer 0-7");
        REQUIRE_TRUE(withKind.controllers[0].config.systemMessages[0].press.gestureIx == 0);
    }
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
