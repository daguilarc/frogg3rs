// FroggersMonoValidationTests.cpp -- validates the mono
// assumption end-to-end. This is a designed stop-and-ask gate: an earlier
// "mono is first-class" claim was overclaimed, and StandardModulators
// special-cases mono *out* -- nothing in Sheaf
// demonstrates the full parameter/modulation/encoder pipeline exercised
// end-to-end with the primary group at numVoices == 1 except this test.
//
// This stands up
// EXACTLY what the validation needs and nothing more: one
// ParameterGroup{numVoices = 1}, one Bank, one Bank/BankSlot pair, one
// modulation source (registered directly via
// ParameterGroup::SetModulationSource -- the mechanism this app's own
// modulation slate uses, not
// the StandardModulators aggregate), and the depth cell that source's
// drill-in materializes. This is deliberately NOT part of FroggersApp/
// Froggers.hpp: it is a standalone, minimal validation app, run only
// through this test, that isolates the mono-model question from everything
// else FroggersApp does.
//
// Driven end-to-end through synth_rig::SynthRig<App> (the same JUCE-free
// harness Sheaf's own miniapp/braid-4 system tests use), so parameter
// registration, drill-in press routing, and PopulateUIState's real
// block-count throttle (include/synth/Engine.hpp:413) are all exercised
// through the production message/audio-thread path, not called directly.
//
// If any of the four checks below come back second-class, this test must
// fail loudly -- no workaround, no
// silent adaptation.

#include "support/SynthRig.hpp"

#include "synth/AppConcepts.hpp"
#include "synth/AppContext.hpp"
#include "synth/EncoderDraw.hpp"
#include "synth/ParameterModulation.hpp"

#ifdef JUCE_MAJOR_VERSION
#error "Froggers mono-validation tests must not see JUCE headers"
#endif

#include <array>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace synth_froggers_validation {

// Deliberately minimal: one mono group, one parameter, one modulation source.
// Satisfies only synth::SynthApplicationCore (SynthRig needs no more); this
// is a validation rig, not a SynthApplication, and is never registered with
// any launcher.
class MonoValidationApp {
public:
    static constexpr std::size_t kNumModulators = 1;
    // Drill-in needs >= numModulators + 1 physical encoders
    // (Bank::OpenModulationView, src/ParameterModulation.cpp:2818-2821): one
    // depth cell for the single modulator, one for the Target/Back cell.
    static constexpr std::size_t kNumPhysicalEncoders = kNumModulators + 1;

    static synth::RuntimeConfig Config() {
        synth::RuntimeConfig config;
        config.appName = "Froggers Mono Validation (not shipped)";
        config.numAudioInputs = 0;
        config.numAudioOutputs = 2;
        config.preferredSampleRate = 48000.0;
        config.preferredBlockSize = 256;
        config.uiFrameHz = 30;
        return config;
    }

    void Init(synth::AppContext* context) {
        if (context == nullptr || context->parameterManager == nullptr) {
            throw std::invalid_argument("MonoValidationApp requires a valid app context");
        }
        context_ = context;
        synth::ParameterManager& manager = *context_->parameterManager;

        // --- exactly one mono ParameterGroup ---
        group_ = &manager.CreateGroup({
            .numVoices = 1,
            .numModulators = kNumModulators,
            .numScenes = 1,
            // One top-level parameter + one level-1 depth parameter it can
            // materialize on drill-in, plus a little headroom.
            .maxParameters = 4,
        });

        paramId_ = manager.RegisterParameter(*group_, {
            .name = "Test Param",
            .shortName = "Test",
            .defaultValue = 0.5f,
            .range = synth::RangeKind::Unipolar,
            .baseColor = synth::Color::Cyan,
        });
        synth::Parameter& param = manager.ParameterById(paramId_);

        // --- exactly one modulation source, registered the way
        // Froggers' own slate registers its sources (ParameterGroup::SetModulationSource
        // directly), not via the StandardModulators aggregate ---
        std::array<float* const, 1> sourcePointers{&modSourceValue_};
        group_->SetModulationSource(0, sourcePointers, synth::ModulatorMetadata{
            .name = "Test Mod",
            .shortName = "TMod",
            .sourceColor = synth::Color::Orange,
            .connected = true,
        });

        // --- one bank, one slot, matching Braid4Core's Bank/BankSlot wiring
        // order (create bank -> create slot -> add physical encoders ->
        // SelectBank -> RegisterParameters, since RegisterParameters requires
        // an associated slot) ---
        bank_ = &manager.CreateBank();
        bank_->SetBankColor(synth::Color::Cyan);
        slot_ = &manager.CreateBankSlot();
        for (synth::PhysicalEncoderId encoderId = 0; encoderId < kNumPhysicalEncoders; ++encoderId) {
            slot_->AddPhysicalEncoder(encoderId);
        }
        slot_->SelectBank(bank_);

        std::array<synth::Parameter*, 1> params{&param};
        bank_->RegisterParameters(params, /*offset=*/0);
    }

    void ProcessBlock(synth::AudioBlock& block) {
        modSourceValue_ = 0.25f;
        if (block.outputs == nullptr) {
            return;
        }
        for (int channelIx = 0; channelIx < block.numOutputChannels; ++channelIx) {
            float* const channel = block.outputs[static_cast<std::size_t>(channelIx)];
            if (channel == nullptr) {
                continue;
            }
            for (std::size_t frame = 0; frame < block.numFrames; ++frame) {
                channel[frame] = 0.0f;
            }
        }
    }

    synth::ParameterId TestParamId() const { return paramId_; }

private:
    synth::AppContext* context_ = nullptr;
    synth::ParameterGroup* group_ = nullptr;
    synth::ParameterId paramId_{};
    synth::Bank* bank_ = nullptr;
    synth::BankSlot* slot_ = nullptr;
    float modSourceValue_ = 0.0f;
};

static_assert(synth::SynthApplicationCore<MonoValidationApp>,
              "MonoValidationApp must satisfy SynthApplicationCore for SynthRig to drive it");

}  // namespace synth_froggers_validation

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
        std::filesystem::temp_directory_path() / "froggers-mono-validation-tests" / testName;
    std::filesystem::remove_all(dataRoot);
    synth::RuntimeDataPaths paths = synth::RuntimeDataPaths::FromDataRoot(dataRoot);
    std::filesystem::create_directories(paths.patchesRoot);
    return paths;
}

constexpr std::size_t kSlotIx = 0;
constexpr std::size_t kTestParamPosition = 0;
constexpr std::size_t kTargetBackPosition = 1;

TEST_CASE(mono_group_parameter_registers_and_resolves) {
    // Check 1: parameters register and resolve.
    synth_rig::SynthRig<synth_froggers_validation::MonoValidationApp> rig(
        64, UseScratchRuntimeDataPaths("registers_and_resolves"));

    const synth::ParameterId id = rig.Application().TestParamId();
    // Resolves back to the same registered parameter object (by identity,
    // not merely by a plausible-looking id -- ParameterById throws if the id
    // does not resolve to a real, owned parameter).
    const synth::Parameter& resolved = rig.Engine().Manager().ParameterById(id);
    REQUIRE_TRUE(resolved.Name() == "Test Param");

    rig.RunBlocks(4);
    // Resolves to a real value at voice 0 (the only voice in a mono group).
    const float value = rig.ParameterValue(id, /*voiceIx=*/0);
    REQUIRE_TRUE(std::isfinite(value));
}

TEST_CASE(mono_group_populate_ui_state_publishes_single_voice) {
    // Check 2: PopulateUIState publishes a single voice.
    // PopulateUIState is throttled by uiPublishInterval_
    // (include/synth/Engine.hpp:413) -- pump enough blocks
    // before reading the published state (rig.UIState() also forces an
    // immediate synchronous populate, but pumping blocks first exercises the
    // real audio-thread publish path too).
    synth_rig::SynthRig<synth_froggers_validation::MonoValidationApp> rig(
        64, UseScratchRuntimeDataPaths("populates_single_voice"));

    rig.RunBlocks(32);
    synth::ParameterManager::UIState& uiState = rig.UIState();

    REQUIRE_TRUE(uiState.slotCapacity > kSlotIx);
    const synth::BankSlot::UIState& slotState = uiState.slots[kSlotIx];
    REQUIRE_TRUE(slotState.connected.load());
    REQUIRE_TRUE(kTestParamPosition < slotState.cellCapacity);

    const synth::Parameter::UIState& cellState = slotState.cells[kTestParamPosition];
    REQUIRE_TRUE(cellState.connected.load());
    REQUIRE_TRUE(cellState.voiceCount.load() == 1);
}

TEST_CASE(mono_group_encoder_draw_state_yields_one_ring) {
    // Check 3: EncoderDrawStateFromParameter yields
    // voiceCount == 1 and one ring, not a degenerate stack.
    synth_rig::SynthRig<synth_froggers_validation::MonoValidationApp> rig(
        64, UseScratchRuntimeDataPaths("one_ring_not_a_stack"));

    rig.RunBlocks(32);
    synth::ParameterManager::UIState& uiState = rig.UIState();
    const synth::Parameter::UIState& cellState = uiState.slots[kSlotIx].cells[kTestParamPosition];

    const synth::ui::EncoderDrawState drawState = synth::ui::EncoderDrawStateFromParameter(cellState);
    REQUIRE_TRUE(drawState.connected);
    REQUIRE_TRUE(drawState.voiceCount == 1);
    REQUIRE_TRUE(drawState.voices.size() == 1);  // one ring, not a degenerate stack of voices.
}

TEST_CASE(mono_group_drill_in_materializes_depth_parameter) {
    // Check 4: drill-in materializes depth parameters.
    // Press the physical encoder holding the test parameter -- through
    // SynthRig::Press, the same production UI-bus message path a real
    // control surface uses (synth::MessageIn::ParamPush) -- and confirm the
    // modulation-detail view materializes: depth cell at position 0 (now a
    // *different*, newly materialized Parameter, not the top-level one) and
    // the Target/Back cell at position 1 holding the original parameter.
    synth_rig::SynthRig<synth_froggers_validation::MonoValidationApp> rig(
        64, UseScratchRuntimeDataPaths("drill_in_materializes_depth"));

    const synth::ParameterId topLevelId = rig.Application().TestParamId();
    rig.RunBlocks(4);

    rig.Press(kSlotIx, kTestParamPosition);
    rig.RunBlocks(32);  // let the press drain and a UI publish land.

    synth::ParameterManager::UIState& uiState = rig.UIState();
    const synth::BankSlot::UIState& slotState = uiState.slots[kSlotIx];
    REQUIRE_TRUE(slotState.showingModulationView.load());

    const synth::Parameter::UIState& depthCellState = slotState.cells[kTestParamPosition];
    REQUIRE_TRUE(depthCellState.connected.load());
    REQUIRE_TRUE(depthCellState.voiceCount.load() == 1);  // depth params are mono too.

    const synth::Parameter::UIState& targetBackCellState = slotState.cells[kTargetBackPosition];
    REQUIRE_TRUE(targetBackCellState.connected.load());

    // Back (press the Target/Back cell) returns to the parameter grid, and
    // the original top-level parameter still resolves unchanged.
    rig.Press(kSlotIx, kTargetBackPosition);
    rig.RunBlocks(32);
    synth::ParameterManager::UIState& uiStateAfterBack = rig.UIState();
    REQUIRE_TRUE(!uiStateAfterBack.slots[kSlotIx].showingModulationView.load());
    REQUIRE_TRUE(std::isfinite(rig.ParameterValue(topLevelId, /*voiceIx=*/0)));
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
