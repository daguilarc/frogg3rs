// FroggersParameterModelTests.cpp -- validates the mono, 16-slot bank
// parameter model.
//
// Requires: parameter count/identity per bank (9 + Crispy@14 +
// Crunchy@15); Crispy/Crunchy slot identity stable across bank changes;
// per-bank colour reaching EncoderDrawState.baseColor; scene blend
// interpolating across banks and reaching the published ring state.
//
// Also a STOP-AND-ASK GATE validating the shared-Crunchy assumption:
// registering one Parameter at slot 15 in all six banks must resolve
// identically from every bank, move together when edited from any bank,
// publish as ONE parameter (not six), and drill in to the same target from
// any bank -- with no duplicate-registration/bank-ownership assertion firing.
// See the final test case's comment for how each check maps to a concrete
// assertion.
//
// The purely structural checks (identity, counts, colours-at-config-level)
// use a bare synth::ParameterManager + FroggersParameterModel directly --
// no Engine/SynthRig needed, since Bank::VisibleParameter() and per-parameter
// state are queryable without any audio-thread processing. The scene-blend
// and shared-Crunchy runtime checks (which need real message-bus press/turn
// routing and PopulateUIState's real throttle) use
// synth_rig::SynthRig<FroggersApp> (External/Sheaf's
// tests/support/SynthRig.hpp), same as FroggersHeadlessTests.cpp
// and FroggersMonoValidationTests.cpp.

#include "Froggers.hpp"
#include "FroggersParameters.hpp"
#include "dsp/Fuegoize.hpp"
#include "support/SynthRig.hpp"

#include "synth/EncoderDraw.hpp"
#include "synth/ParameterModulation.hpp"

#ifdef JUCE_MAJOR_VERSION
#error "Froggers parameter-model tests must not see JUCE headers"
#endif

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
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

synth::RuntimeDataPaths UseScratchRuntimeDataPaths(const char* testName) {
    const std::filesystem::path dataRoot =
        std::filesystem::temp_directory_path() / "froggers-parameter-model-tests" / testName;
    std::filesystem::remove_all(dataRoot);
    synth::RuntimeDataPaths paths = synth::RuntimeDataPaths::FromDataRoot(dataRoot);
    std::filesystem::create_directories(paths.patchesRoot);
    return paths;
}

// --- 4.5 check 1: parameter count/identity per bank ------------------------

TEST_CASE(bank_layouts_have_nine_named_parameters_plus_fixed_crispy_and_crunchy) {
    synth::ParameterManager manager;
    synth_froggers::FroggersParameterModel model;
    model.Init(manager);

    const auto& layouts = synth_froggers::FroggersBankLayouts();
    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
        synth::Bank& bank = model.BankAt(bankIx);
        const synth_froggers::FroggersBankLayout& layout = layouts[bankIx];

        for (std::size_t paramIx = 0; paramIx < synth_froggers::kFroggersParamsPerBank; ++paramIx) {
            synth::Parameter* param = bank.VisibleParameter(static_cast<synth::PhysicalEncoderId>(paramIx));
            REQUIRE_TRUE(param != nullptr);
            // Name() is bank-qualified ("<Bank> <label>") to satisfy
            // ParameterManager::RegisterParameter's global name-uniqueness
            // check (External/Sheaf/projects/synth/src/ParameterModulation.cpp:3069-3071) -- some page
            // labels genuinely repeat across banks (e.g. "Stereo width" on
            // both Reverb and Delay). ShortName() stays the authentic,
            // possibly-repeated, page-local label.
            REQUIRE_TRUE(param->Name() == std::string(layout.name) + " " + layout.params[paramIx].name);
            REQUIRE_TRUE(param->ShortName() == layout.params[paramIx].shortName);
            REQUIRE_TRUE(param == &model.PageParameter(bankIx, paramIx));
            REQUIRE_TRUE(param->BaseColor() == layout.color);
        }

        // Slots 9-13 are now ordinary page parameters too (grown from 9 to
        // 14 per bank); the loop above already covers them, so there is no
        // longer any "sparse, empty slots 9-13" region to assert on here.

        synth::Parameter* crispy = bank.VisibleParameter(
            static_cast<synth::PhysicalEncoderId>(synth_froggers::kFroggersCrispySlot));
        REQUIRE_TRUE(crispy == &model.Crispy(bankIx));
        REQUIRE_TRUE(crispy->BaseColor() == layout.color);  // Crispy takes the bank colour.

        synth::Parameter* crunchy = bank.VisibleParameter(
            static_cast<synth::PhysicalEncoderId>(synth_froggers::kFroggersCrunchySlot));
        REQUIRE_TRUE(crunchy == &model.Crunchy());
        REQUIRE_TRUE(crunchy->BaseColor() == synth_froggers::FroggersCrunchyColor());
    }

    // 91 top-level parameters total: 6 banks x 14 page params + 6 per-bank
    // Crispy + 1 SHARED Crunchy -- not 96, because Crunchy is one Parameter,
    // not six.
    REQUIRE_TRUE(manager.ParameterCount() == 91);
}

// --- 4.5 check 2: Crispy/Crunchy identity stable across bank changes -------

TEST_CASE(crispy_and_crunchy_identity_stable_when_active_bank_changes) {
    synth::ParameterManager manager;
    synth_froggers::FroggersParameterModel model;
    model.Init(manager);

    std::array<synth::Parameter*, synth_froggers::kFroggersBankCount> crispyPointers{};
    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
        // Simulate switching the active bank on the shared slot; identity of
        // slot 14/15 is a property of each Bank object, not of which bank is
        // currently selected, but exercise the switch anyway, as production
        // code does.
        model.Slot().SelectBank(&model.BankAt(bankIx));

        REQUIRE_TRUE(model.BankAt(bankIx).VisibleParameter(
                         static_cast<synth::PhysicalEncoderId>(synth_froggers::kFroggersCrunchySlot)) ==
                     &model.Crunchy());
        synth::Parameter* crispy = model.BankAt(bankIx).VisibleParameter(
            static_cast<synth::PhysicalEncoderId>(synth_froggers::kFroggersCrispySlot));
        REQUIRE_TRUE(crispy == &model.Crispy(bankIx));
        crispyPointers[bankIx] = crispy;
    }

    // Six distinct Crispy objects (unlike Crunchy, which is the same one).
    for (std::size_t i = 0; i < crispyPointers.size(); ++i) {
        for (std::size_t j = i + 1; j < crispyPointers.size(); ++j) {
            REQUIRE_TRUE(crispyPointers[i] != crispyPointers[j]);
        }
    }
}

// --- 4.5 check 3: per-bank colour reaches EncoderDrawState.baseColor -------

TEST_CASE(per_bank_colour_reaches_encoder_draw_state_base_color) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(64, UseScratchRuntimeDataPaths("bank_colour"));
    const auto& layouts = synth_froggers::FroggersBankLayouts();

    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
        rig.SelectBank(/*slotIx=*/0, bankIx);
        rig.RunBlocks(16);  // let SelectParamBank drain and a UI publish land.

        synth::ParameterManager::UIState& uiState = rig.UIState();
        const synth::BankSlot::UIState& slotState = uiState.slots[0];

        const auto pageDraw = synth::ui::EncoderDrawStateFromParameter(slotState.cells[0]);
        REQUIRE_TRUE(pageDraw.connected);
        REQUIRE_TRUE(pageDraw.baseColor == layouts[bankIx].color);

        const auto crispyDraw = synth::ui::EncoderDrawStateFromParameter(
            slotState.cells[synth_froggers::kFroggersCrispySlot]);
        REQUIRE_TRUE(crispyDraw.connected);
        REQUIRE_TRUE(crispyDraw.baseColor == layouts[bankIx].color);

        // The shared Crunchy has ONE fixed colour, the same in every
        // bank -- it cannot take on each bank's colour.
        const auto crunchyDraw = synth::ui::EncoderDrawStateFromParameter(
            slotState.cells[synth_froggers::kFroggersCrunchySlot]);
        REQUIRE_TRUE(crunchyDraw.connected);
        REQUIRE_TRUE(crunchyDraw.baseColor == synth_froggers::FroggersCrunchyColor());
    }
}

// --- 4.5 check 4: scene blend interpolates across banks, reaches the ring --

TEST_CASE(scene_blend_interpolates_and_reaches_published_ring_state_across_banks) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(64, UseScratchRuntimeDataPaths("scene_blend"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();

    synth::Parameter& audioParam = model.PageParameter(synth_froggers::FroggersBankId::Audio, 0);    // VCO1
    synth::Parameter& reverbParam = model.PageParameter(synth_froggers::FroggersBankId::Reverb, 0);  // Wet/dry

    audioParam.SceneCenter(0) = 0.1f;
    audioParam.SceneCenter(1) = 0.9f;
    reverbParam.SceneCenter(0) = 0.3f;
    reverbParam.SceneCenter(1) = 0.7f;

    // Audio is the default active bank; make that explicit rather
    // than relying on the default.
    rig.SelectBank(/*slotIx=*/0, static_cast<std::size_t>(synth_froggers::FroggersBankId::Audio));
    rig.RunBlocks(4);

    // Reverb is deliberately NOT the active bank throughout this test: scene
    // blend is one manager-wide SceneState, so it must reach every
    // registered parameter's ring state at once, not just the visible bank's
    // (ParameterGroup::ProcessSamplePhase1 iterates ALL topLevelParameters_).
    // PopulateUIState publishes
    // UIDisplayCenter(v) -- reverbParam.UIDisplayCenter(0) reads the exact
    // value that publish would copy, without needing Reverb to be the active
    // bank on the one shared BankSlot.
    auto settleAndReadAudioRing = [&]() -> float {
        // uiDisplayCenterAlpha (~10 Hz at 48 kHz, ParameterModulation.hpp's
        // kDefaultUiDisplayCenterAlpha) needs roughly 14 blocks of 256
        // frames to settle a full-range jump within 1%; 64 gives generous
        // margin (rig.UIState() also forces an immediate synchronous
        // publish, sidestepping the uiPublishInterval_ throttle).
        rig.RunBlocks(64);
        synth::ParameterManager::UIState& uiState = rig.UIState();
        const synth::Parameter::UIState& cell = uiState.slots[0].cells[0];
        return synth::ui::EncoderDrawStateFromParameter(cell).voices.at(0).value;
    };

    rig.SetSceneBlend(0.0f);
    REQUIRE_NEAR(settleAndReadAudioRing(), 0.1f, 0.02f);
    REQUIRE_NEAR(reverbParam.UIDisplayCenter(0), 0.3f, 0.02f);

    rig.SetSceneBlend(1.0f);
    REQUIRE_NEAR(settleAndReadAudioRing(), 0.9f, 0.02f);
    REQUIRE_NEAR(reverbParam.UIDisplayCenter(0), 0.7f, 0.02f);

    rig.SetSceneBlend(0.5f);
    REQUIRE_NEAR(settleAndReadAudioRing(), 0.5f, 0.02f);
    REQUIRE_NEAR(reverbParam.UIDisplayCenter(0), 0.5f, 0.02f);
}

// --- 4.6 STOP-AND-ASK GATE: shared Crunchy validation ----------------------

TEST_CASE(shared_crunchy_resolves_and_moves_identically_from_any_bank) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(64, UseScratchRuntimeDataPaths("shared_crunchy"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    constexpr synth::PhysicalEncoderId kCrunchyPosition =
        static_cast<synth::PhysicalEncoderId>(synth_froggers::kFroggersCrunchySlot);

    // Check (a): "both/all banks resolve it" -- all six banks' slot-15 cell
    // is the identical Crunchy Parameter*.
    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
        REQUIRE_TRUE(model.BankAt(bankIx).VisibleParameter(kCrunchyPosition) == &model.Crunchy());
    }

    // Check (b): "editing it from any bank moves the same value" -- turn it
    // from bank Audio, then again from bank Reverb; the second edit
    // accumulates onto the first rather than resetting or diverging (scene
    // blend is 0 throughout, so HandleIncDec's ApplySceneDistribution adds
    // delta straight onto SceneCenter(0), External/Sheaf/projects/synth/src/ParameterModulation.cpp:376-378
    // in External/Sheaf).
    rig.SelectBank(0, static_cast<std::size_t>(synth_froggers::FroggersBankId::Audio));
    rig.RunBlocks(4);
    rig.Turn(0, kCrunchyPosition, 0.3f);
    rig.RunBlocks(16);
    const float afterAudioTurn = model.Crunchy().GetRaw(0);
    REQUIRE_NEAR(afterAudioTurn, 0.3f, 0.02f);

    rig.SelectBank(0, static_cast<std::size_t>(synth_froggers::FroggersBankId::Reverb));
    rig.RunBlocks(4);
    rig.Turn(0, kCrunchyPosition, 0.2f);
    rig.RunBlocks(16);
    const float afterReverbTurn = model.Crunchy().GetRaw(0);
    REQUIRE_NEAR(afterReverbTurn, afterAudioTurn + 0.2f, 0.02f);

    // Check (c): "PopulateUIState publishes one parameter, not six" -- the
    // manager's own parameter count is 91 (not 96; see the first test case),
    // and the value read through the UI-state cell at slot 15 is identical
    // regardless of which bank is currently active.
    REQUIRE_TRUE(rig.Engine().Manager().ParameterCount() == 91);

    // Let the uiDisplayCenter slew fully settle onto the post-turn value
    // BEFORE comparing snapshots across banks -- otherwise two reads taken
    // several blocks apart would legitimately differ simply because the
    // value is still slewing (not because of any per-bank duplication),
    // which would make a tight tolerance flaky rather than meaningful.
    rig.RunBlocks(4000);
    synth::ParameterManager::UIState& uiStateReverbActive = rig.UIState();
    const float crunchyValueViaReverb =
        synth::ui::EncoderDrawStateFromParameter(uiStateReverbActive.slots[0].cells[kCrunchyPosition])
            .voices.at(0)
            .value;

    rig.SelectBank(0, static_cast<std::size_t>(synth_froggers::FroggersBankId::Filter));
    rig.RunBlocks(4);  // just enough to drain the SelectParamBank message.
    synth::ParameterManager::UIState& uiStateFilterActive = rig.UIState();
    const float crunchyValueViaFilter =
        synth::ui::EncoderDrawStateFromParameter(uiStateFilterActive.slots[0].cells[kCrunchyPosition])
            .voices.at(0)
            .value;
    REQUIRE_NEAR(crunchyValueViaReverb, crunchyValueViaFilter, 0.001f);

    // Check (d): "drill-in from any bank targets the same parameter" -- press
    // Crunchy's cell from two different banks; each bank's OWN
    // Bank::selected_ ends up pointing at the identical shared Crunchy
    // Parameter*. A press on the UI bus (rig.Press) is this app's drill-in
    // action and never reaches the library's Bank::HandlePress selection, so
    // this model-level check presses the library's parameter manager
    // directly instead.
    rig.SelectBank(0, static_cast<std::size_t>(synth_froggers::FroggersBankId::Audio));
    rig.RunBlocks(4);
    rig.Engine().Manager().HandlePress(0, kCrunchyPosition);
    rig.RunBlocks(16);
    REQUIRE_TRUE(model.BankAt(synth_froggers::FroggersBankId::Audio).SelectedParameter() == &model.Crunchy());

    rig.SelectBank(0, static_cast<std::size_t>(synth_froggers::FroggersBankId::Delay));
    rig.RunBlocks(4);
    rig.Engine().Manager().HandlePress(0, kCrunchyPosition);
    rig.RunBlocks(16);
    REQUIRE_TRUE(model.BankAt(synth_froggers::FroggersBankId::Delay).SelectedParameter() == &model.Crunchy());

    // Check (e): "no duplicate-registration or bank-ownership assertion
    // fires" -- proven simply by this rig having constructed and run at all.
    // FroggersParameterModel::Init registers the identical Crunchy Parameter*
    // into all six banks' RegisterParameters calls; if Sheaf enforced
    // cross-bank ownership anywhere, that would have thrown out of
    // FroggersApp::Init (called from SynthRig's constructor via
    // Engine::Initialize()) before this test body ever ran.
}

// --- Fuego seam (model level) ------------
//
// The rendered-surface assertion (that the encoder ring itself paints the
// processed value) is not covered here -- these tests stop
// at the model boundary: Parameter::CachedKnobValue() (what the real DSP
// chain reads) and ParameterManager::UIState.values[v] (what the
// encoder draw path reads).
//
// Direct model-driving tests (no SynthRig) call FroggersParameterModel::
// ProcessSample() in a tight loop against a bare synth::ParameterManager --
// the same "no Engine/SynthRig needed" pattern the structural checks above
// use, since ProcessSample() only touches the ParameterGroup the model
// itself owns. Setting Parameter::SceneCenter(0) directly and letting the
// currentCenter_/targetCenter_ smoother cascade (processLiteAlpha ~=0.1227,
// targetCenterAlpha ~=0.0994 recomputed every 16 samples,
// ParameterModulation.hpp) settle over several thousand samples reaches the
// same steady state SynthRig's scene-blend test reaches over many blocks.

TEST_CASE(fuego_seam_transform_reaches_cached_knob_value_matching_dsp_stack) {
    synth::ParameterManager manager;
    synth_froggers::FroggersParameterModel model;
    model.Init(manager);

    synth::Parameter& page0 = model.PageParameter(synth_froggers::FroggersBankId::Audio, 0);
    synth::Parameter& crispy = model.Crispy(synth_froggers::FroggersBankId::Audio);
    synth::Parameter& crunchy = model.Crunchy();

    // Nonzero, non-trivial values so the cascade actually runs rather than
    // being bypassed by Fuegoize's `fuegKnob <= 0` identity shortcut.
    page0.SceneCenter(0) = 0.42f;
    crispy.SceneCenter(0) = 0.35f;
    crunchy.SceneCenter(0) = 0.60f;

    for (std::uint64_t sampleIx = 0; sampleIx < 4000; ++sampleIx) {
        model.ProcessSample(sampleIx);
    }

    // Loose sanity check that the smoother cascade actually settled near the
    // requested scene centers -- convergence precision isn't this test's
    // point, the transform is.
    REQUIRE_NEAR(page0.GetRaw(0), 0.42f, 0.01f);
    REQUIRE_NEAR(crispy.GetRaw(0), 0.35f, 0.01f);
    REQUIRE_NEAR(crunchy.GetRaw(0), 0.60f, 0.01f);

    const float rawPage0 = page0.GetRaw(0);
    const float rawCrispy = crispy.GetRaw(0);
    const float rawCrunchy = crunchy.GetRaw(0);

    // The fuego transform must reach the DSP -- read at the
    // model's DSP-facing seam: Parameter::CachedKnobValue() is exactly the
    // slot the real DSP chain reads each sample (FroggersAppCore::
    // RouteAudioSample's own `knob` lambda calls it directly) -- this is
    // the exact value production reads, not a proxy for a future consumer.
    // Independently recompute the expected
    // value via the same ported cascade (app/dsp/Fuegoize.hpp's FuegoStack)
    // from the settled raw inputs read a moment ago -- GetRaw() is
    // a pure read of currentCenter_/modulation state that hasn't changed
    // since ApplyFuegoSeam() last ran, so this recomputation and the value
    // actually written by the seam must match to floating-point precision.
    const float expected = synth_froggers::dsp::FuegoStack::ApplyMusicalRow(
        rawPage0, rawCrunchy, rawCrispy, /*row=*/0,
        static_cast<uint8_t>(synth_froggers::kFroggersCrispySlot));
    REQUIRE_NEAR(page0.CachedKnobValue(0), expected, 1e-4f);

    // And the seam actually moved the value (proves it is live, not a
    // no-op) -- with these inputs the ported cascade computes ~0.4749 vs a
    // raw 0.42 (verified independently against the ported formula).
    REQUIRE_TRUE(std::fabs(page0.CachedKnobValue(0) - rawPage0) > 1e-3f);
}

TEST_CASE(crispy_sweep_changes_sibling_parameters_on_its_own_bank_only) {
    synth::ParameterManager manager;
    synth_froggers::FroggersParameterModel model;
    model.Init(manager);

    // Global Crunchy stays at 0 (identity/bypass for the global stage) so
    // this test isolates Crispy's own effect cleanly -- the fuego cascade
    // still applies the per-bank Crispy stage even with Crunchy at 0 (the
    // musical row's second Fuegoize call is keyed by the Crunchy-warped
    // Crispy value, which is simply the raw Crispy value unchanged when
    // Crunchy is 0).
    synth::Parameter& audioRow0 = model.PageParameter(synth_froggers::FroggersBankId::Audio, 0);
    synth::Parameter& audioRow1 = model.PageParameter(synth_froggers::FroggersBankId::Audio, 1);
    synth::Parameter& audioCrispy = model.Crispy(synth_froggers::FroggersBankId::Audio);
    synth::Parameter& reverbRow0 = model.PageParameter(synth_froggers::FroggersBankId::Reverb, 0);
    synth::Parameter& reverbCrispy = model.Crispy(synth_froggers::FroggersBankId::Reverb);

    // Identical raw input on both banks' row 0 -- any difference in the
    // processed output between the two banks can only come from their
    // (different) per-bank Crispy values, not from the input itself.
    audioRow0.SceneCenter(0) = 0.42f;
    audioRow1.SceneCenter(0) = 0.42f;
    reverbRow0.SceneCenter(0) = 0.42f;
    audioCrispy.SceneCenter(0) = 0.7f;   // swept on
    reverbCrispy.SceneCenter(0) = 0.0f;  // left at neutral -- Reverb's control group

    for (std::uint64_t sampleIx = 0; sampleIx < 4000; ++sampleIx) {
        model.ProcessSample(sampleIx);
    }

    // Audio's own siblings (row 0 and row 1) both moved off their OWN raw
    // (settled) value -- one bank's Crispy affects every ordinary parameter
    // in that same bank. Compared against each parameter's own GetRaw()
    // (not the 0.42 setpoint) so this holds regardless of smoother settling
    // precision, matching the fuego_seam_transform_reaches_cached_knob_value_matching_dsp_stack
    // test's approach.
    REQUIRE_TRUE(std::fabs(audioRow0.CachedKnobValue(0) - audioRow0.GetRaw(0)) > 1e-2f);
    REQUIRE_TRUE(std::fabs(audioRow1.CachedKnobValue(0) - audioRow1.GetRaw(0)) > 1e-2f);

    // Reverb's row 0 -- its OWN bank's Crispy is left at 0, so (the fuego
    // cascade, with Crunchy also at 0) both Fuegoize stages bypass via the
    // `fuegKnob <= 0` identity shortcut -- CachedKnobValue must equal its
    // own raw value exactly, whatever that settled to: Audio's Crispy sweep
    // did not leak across banks.
    REQUIRE_NEAR(reverbRow0.CachedKnobValue(0), reverbRow0.GetRaw(0), 1e-4f);

    // And Audio's Crispy sweep is provably confined to Audio: the two
    // banks' row-0 outputs, which started from the identical raw input,
    // now differ specifically because of the per-bank Crispy value.
    REQUIRE_TRUE(std::fabs(audioRow0.CachedKnobValue(0) - reverbRow0.CachedKnobValue(0)) > 1e-2f);
}

TEST_CASE(global_crunchy_affects_all_banks) {
    synth::ParameterManager manager;
    synth_froggers::FroggersParameterModel model;
    model.Init(manager);

    synth::Parameter& crunchy = model.Crunchy();
    crunchy.SceneCenter(0) = 0.5f;

    // Identical raw input, zero Crispy, on every bank's row 0 -- so any
    // change is attributable to the ONE shared Crunchy control alone.
    std::array<synth::Parameter*, synth_froggers::kFroggersBankCount> row0{};
    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
        synth::Parameter& param = model.PageParameter(bankIx, 0);
        param.SceneCenter(0) = 0.42f;
        model.Crispy(bankIx).SceneCenter(0) = 0.0f;
        row0[bankIx] = &param;
    }

    for (std::uint64_t sampleIx = 0; sampleIx < 4000; ++sampleIx) {
        model.ProcessSample(sampleIx);
    }

    const float rawCrunchy = crunchy.GetRaw(0);
    REQUIRE_NEAR(rawCrunchy, 0.5f, 0.01f);  // loose sanity, not the point of this test

    // Every bank's row 0, computed independently from ITS OWN settled raw
    // value and the one shared, settled Crunchy raw value -- self-consistent
    // regardless of smoother settling precision (same technique as
    // fuego_seam_transform_reaches_cached_knob_value_matching_dsp_stack).
    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
        synth::Parameter& param = *row0[bankIx];
        const float rawParam = param.GetRaw(0);
        const float expected = synth_froggers::dsp::FuegoStack::ApplyMusicalRow(
            rawParam, rawCrunchy, /*crispyKnobPreFuego=*/0.0f, /*row=*/0,
            static_cast<uint8_t>(synth_froggers::kFroggersCrispySlot));
        REQUIRE_TRUE(std::fabs(expected - rawParam) > 1e-3f);  // sanity: Crunchy actually moves it
        REQUIRE_NEAR(param.CachedKnobValue(0), expected, 1e-4f);
    }
}

TEST_CASE(global_crunchy_itself_receives_no_fuego_stage) {
    synth::ParameterManager manager;
    synth_froggers::FroggersParameterModel model;
    model.Init(manager);

    synth::Parameter& crunchy = model.Crunchy();
    // A nonzero, non-edge value: were Crunchy mistakenly warped by itself
    // (self-referential fuego), Fuegoize(0.6, 0.6, row) would move it
    // measurably off 0.6 -- this is not a value that merely happens to be a
    // fixed point of the transform.
    crunchy.SceneCenter(0) = 0.6f;

    for (std::uint64_t sampleIx = 0; sampleIx < 4000; ++sampleIx) {
        model.ProcessSample(sampleIx);
    }

    // An inference, not a traced fact: global Crunchy receives NO fuego
    // stage at all -- its cached knob value must equal its own raw value
    // exactly, since ApplyFuegoSeam() never calls ReplaceCachedKnobValue()
    // on the Crunchy parameter.
    REQUIRE_TRUE(crunchy.CachedKnobValue(0) == crunchy.GetRaw(0));
}

// --- 5.5 (continued): the value published to UIState.values[v] is the
// fuegoized one. Needs a real synth_rig::SynthRig<FroggersApp> (not the bare
// ParameterManager the tests above use) to drive PopulateUIState the same
// way FroggersApp::ProcessBlock -> Engine does; rig.UIState() forces a
// synchronous publish (see the 4.5 scene-blend test's comment on
// uiPublishInterval_), so "pump enough blocks" here is about letting
// ProcessLitePhase2's UIDisplayCenter slew (uiDisplayCenterAlpha, an
// intentionally slow ~10 Hz one-pole) catch up to the fuegoized cached
// value, not about the publish throttle itself (External/Sheaf/projects/synth/include/synth/Engine.hpp:413).
TEST_CASE(fuegoized_value_is_published_to_ui_state_values) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(64, UseScratchRuntimeDataPaths("fuego_ui_publish"));
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();

    // Reverb bank param 0 (Wet/dry), NOT any Audio-bank parameter: the
    // default patch assigns live cross-VCO pitch
    // modulation depths onto the Audio bank's VCO1/2/3 pitch parameters
    // (slots 0-2), so those three no longer resolve to a pure,
    // un-modulated scene-center value the way this test's premise assumes.
    // Reverb param 0 carries no modulation-depth assignment in the default
    // patch, so it stays exactly scene-center + fuego, which is what this
    // test is actually about.
    synth::Parameter& page0 = model.PageParameter(synth_froggers::FroggersBankId::Reverb, 0);
    synth::Parameter& crispy = model.Crispy(synth_froggers::FroggersBankId::Reverb);
    synth::Parameter& crunchy = model.Crunchy();

    page0.SceneCenter(0) = 0.42f;
    crispy.SceneCenter(0) = 0.35f;
    crunchy.SceneCenter(0) = 0.60f;

    rig.SelectBank(/*slotIx=*/0, static_cast<std::size_t>(synth_froggers::FroggersBankId::Reverb));

    // uiDisplayCenterAlpha is a ~10 Hz one-pole (kDefaultUiDisplayCenterAlpha
    // ~=0.0013 in ParameterModulation.hpp) -- reaching within ~2% of a
    // full-scale jump needs roughly 3000 samples; 320 blocks x 256 frames
    // (Froggers' preferredBlockSize) is generous margin, matching the 4.5
    // scene-blend test's own convergence budget.
    rig.RunBlocks(320);

    const float rawPage0 = page0.GetRaw(0);
    const float rawCrispy = crispy.GetRaw(0);
    const float rawCrunchy = crunchy.GetRaw(0);
    const float expected = synth_froggers::dsp::FuegoStack::ApplyMusicalRow(
        rawPage0, rawCrunchy, rawCrispy, /*row=*/0,
        static_cast<uint8_t>(synth_froggers::kFroggersCrispySlot));

    synth::ParameterManager::UIState& uiState = rig.UIState();
    const synth::Parameter::UIState& cell = uiState.slots[0].cells[0];
    const float publishedValue = synth::ui::EncoderDrawStateFromParameter(cell).voices.at(0).value;

    // The published ring value is the fuegoized one, not the raw
    // scene-center value it started from.
    REQUIRE_NEAR(publishedValue, expected, 0.02f);
    REQUIRE_TRUE(std::fabs(publishedValue - rawPage0) > 0.01f);
}

// --- Envelope short names survive EncoderDraw's 4-char truncation
// ---------------------------------------------------------------------------
// EncoderDraw renders UpperShortLabel(shortLabel) with a hard 4-char cap
// (External/Sheaf/projects/synth/include/synth/EncoderDraw.hpp:571-582). The envelope bank's short names are the only
// place the VCO-distinguishing digit lives, so this asserts both that every
// short name already fits in 4 chars (nothing silently truncated) and that
// the three VCO variants of each stage (Attack/Sustain/Release) stay
// distinct from one another after running through the exact same truncation
// function EncoderDraw applies.
TEST_CASE(envelope_bank_short_names_survive_four_char_truncation_distinctly) {
    const auto& layouts = synth_froggers::FroggersBankLayouts();
    const synth_froggers::FroggersBankLayout* envelope = nullptr;
    for (const auto& layout : layouts) {
        if (layout.id == synth_froggers::FroggersBankId::Envelope) {
            envelope = &layout;
            break;
        }
    }
    REQUIRE_TRUE(envelope != nullptr);

    for (const auto& spec : envelope->params) {
        REQUIRE_TRUE(std::string(spec.shortName).size() <= 4);
        // UpperShortLabel must be a no-op (aside from case) at <=4 chars:
        // length through the real truncation function must equal the raw
        // name's length -- i.e. nothing gets cut.
        REQUIRE_TRUE(synth::ui::UpperShortLabel(spec.shortName).size() ==
                     std::string(spec.shortName).size());
    }

    // Layout order within each bank is Attack/Sustain/Release x VCO1/2/3
    // (paramIx 0-2 = VCO1, 3-5 = VCO2, 6-8 = VCO3), so paramIx % 3 groups by
    // stage (0=Attack, 1=Sustain, 2=Release) across the three VCOs.
    for (std::size_t stage = 0; stage < 3; ++stage) {
        std::array<std::string, 3> truncated{};
        for (std::size_t vco = 0; vco < 3; ++vco) {
            const std::size_t paramIx = vco * 3 + stage;
            truncated[vco] = synth::ui::UpperShortLabel(envelope->params[paramIx].shortName);
        }
        REQUIRE_TRUE(truncated[0] != truncated[1]);
        REQUIRE_TRUE(truncated[0] != truncated[2]);
        REQUIRE_TRUE(truncated[1] != truncated[2]);
    }
}

// --- ConfigureProcessingTiming must be wired at PrepareToPlay -----
//
// Sheaf's parameter-smoothing constants (kDefaultProcessLiteAlpha,
// kDefaultTargetComputeIntervalSamples, kDefaultUiDisplayCenterAlpha,
// kDefaultUiDisplaySpreadAlpha -- External/Sheaf/projects/synth/include/synth/ParameterModulation.hpp:170-174) are
// defined at a 48 kHz reference (their own comments), and
// ParameterGroupConfig initialises to exactly those raw values
// (External/Sheaf/projects/synth/include/synth/ParameterModulation.hpp:199-203) -- ConfigureProcessingTiming
// (External/Sheaf/projects/synth/src/ParameterModulation.cpp:859-865) is the only thing that ever replaces
// them. Braid 4 calls it from its sample-rate hook, converting each
// constant with ConvertOnePoleAlpha/ConvertSampleInterval against its own
// internal (oversampled) rate (External/Sheaf/projects/synth/apps/braid-4/Braid4Core.hpp:207-220). This app has no
// oversampling at the parameter tier (FroggersAppCore.hpp's own
// PrepareToPlay sets sampleRate_ straight from the host rate, no
// internalSampleRate_ concept anywhere in this class), so the fix converts
// against the host sample rate directly.
//
// At the plain 48 kHz default (every other test in this file), a missing
// call and a correctly-wired call are indistinguishable (48/48 == 1, every
// ConvertOnePoleAlpha/ConvertSampleInterval call is a no-op) -- so this test
// prepares the app at 96 kHz instead, where a missing call leaves the raw
// 48 kHz-reference constants in place (2x too slow in real time) and a
// correctly-wired call converts them to run at half the raw alpha/interval,
// i.e. this asserts the converted values differ from the unconverted
// defaults, not merely that they are "close to something."
TEST_CASE(configure_processing_timing_is_wired_at_96khz_prepare) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        64, UseScratchRuntimeDataPaths("configure_processing_timing_96khz"),
        synth_rig::SynthRig<synth_froggers::FroggersApp>::AudioSettings{
            .sampleRate = 96000.0, .blockSize = 256});

    const synth::ParameterGroupConfig& config = rig.Application().Parameters().Group().Config();

    // Expected values: the exact conversion Braid 4's own PrepareToPlay
    // performs (External/Sheaf/projects/synth/apps/braid-4/Braid4Core.hpp:207-217), substituting the host rate
    // (96000.0) for Braid 4's internalSampleRate_ per this app's own
    // no-oversampling-at-parameter-tier design.
    const float expectedProcessLiteAlpha =
        synth::ConvertOnePoleAlpha(synth::kDefaultProcessLiteAlpha, 48000.0, 96000.0);
    const std::size_t expectedTargetComputeIntervalSamples =
        synth::ConvertSampleInterval(synth::kDefaultTargetComputeIntervalSamples, 48000.0, 96000.0);
    const float expectedUiDisplayCenterAlpha =
        synth::ConvertOnePoleAlpha(synth::kDefaultUiDisplayCenterAlpha, 48000.0, 96000.0);
    const float expectedUiDisplaySpreadAlpha =
        synth::ConvertOnePoleAlpha(synth::kDefaultUiDisplaySpreadAlpha, 48000.0, 96000.0);

    // Sanity: at a 2x rate, ConvertOnePoleAlpha must actually move the
    // alpha away from the raw 48 kHz-reference constant -- otherwise this
    // test would pass whether or not the app ever calls
    // ConfigureProcessingTiming at all.
    REQUIRE_TRUE(std::fabs(expectedProcessLiteAlpha - synth::kDefaultProcessLiteAlpha) > 1.0e-4f);
    REQUIRE_TRUE(expectedTargetComputeIntervalSamples != synth::kDefaultTargetComputeIntervalSamples);

    REQUIRE_NEAR(config.processLiteAlpha, expectedProcessLiteAlpha, 1.0e-6f);
    REQUIRE_TRUE(config.targetComputeIntervalSamples == expectedTargetComputeIntervalSamples);
    REQUIRE_NEAR(config.uiDisplayCenterAlpha, expectedUiDisplayCenterAlpha, 1.0e-6f);
    REQUIRE_NEAR(config.uiDisplaySpreadAlpha, expectedUiDisplaySpreadAlpha, 1.0e-6f);

    // targetCenterAlpha is deliberately NOT part of
    // synth::ParameterProcessingTiming (External/Sheaf/projects/synth/include/synth/ParameterModulation.hpp:179-184) --
    // ConfigureProcessingTiming never touches it, so it must stay at its
    // own untouched default regardless of host sample rate.
    REQUIRE_NEAR(config.targetCenterAlpha, synth::kDefaultTargetCenterAlpha, 1.0e-6f);
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
