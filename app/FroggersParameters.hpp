#pragma once

// synth_froggers::FroggersParameterModel builds the Sheaf-side parameter/bank
// graph: one ParameterManager, one mono ParameterGroup; six banks sharing
// one BankSlot, each bank holding one page's fourteen parameters
// at slots 0-13, a local Crispy at slot 14, and the single shared global
// Crunchy Parameter at slot 15 in every bank. See FroggersBankLayouts()
// below for the per-page parameter lists. The encoder ring shows the
// *processed* value -- the per-sample ProcessSamplePhase1/2 loop this class
// drives, wired into FroggersApp::ProcessBlock in Froggers.hpp, is exactly
// what makes SceneCenter/blend interpolation reach that published ring
// state.
//
// ApplyFuegoSeam() below is called from
// ProcessSample() between ParameterGroup::ProcessSamplePhase1() and
// ProcessSamplePhase2() -- the exact seam Braid uses for its own per-sample
// filtering (External/Sheaf/projects/synth/apps/braid-4/Braid4Core.hpp:457-459 ProcessParameterPhase1 ->
// FilterParameterCaches -> ProcessParameterPhase2, filtering implementation
// :569-627). This is the ONE fuego application point: see ApplyFuegoSeam()'s
// own comment.
//
// Scope:
//   * This file does not wire modulation sources. ParameterGroupConfig::
//     numModulators is set to 15 to match the slate's shape, but none of the
//     15 sources are SetModulationSource()'d here (that is
//     FroggersModulationSlate::RegisterSources()'s job) -- every slot's
//     ModulatorMetadata stays default {connected=false} until that class
//     runs, and Modulators::UpdateModValues() / Parameter's route-processing
//     are documented no-ops for unconnected slots
//     (External/Sheaf/projects/synth/src/ParameterModulation.cpp:576-577),
//     so driving the sample loop below is safe regardless of registration
//     order.
//   * This file does not build UI layout. FroggersUiSurface.hpp does that;
//     this header only builds the Sheaf-side parameter/bank graph.
//
// Per-parameter `defaultValue` is set explicitly below, per parameter, where
// the ordinary struct default of 0.0f would be inaudible or otherwise wrong
// (see each entry's own comment in FroggersBankLayouts()); `range` stays at
// ParameterConfig's own default (RangeKind::Unipolar) throughout.

#include "dsp/Fuegoize.hpp"

#include "synth/AppContext.hpp"
#include "synth/Color.hpp"
#include "synth/ParameterModulation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace synth_froggers {

// Which v2 page each bank corresponds to; also this bank's index into
// FroggersParameterModel's own bank array (and, since it is the only bank
// creator on this ParameterManager, ParameterManager::BankAt()'s index too).
enum class FroggersBankId : std::size_t {
    Audio = 0,
    Envelope = 1,
    Filter = 2,
    Drive = 3,
    Delay = 4,
    Reverb = 5,
};

// Audio-bank slot layout for a given VCO: pitch at its own index, shape
// at +3, phase mod at +6, ring mod at +9 -- one named grouping shared by
// ProcessBlock's vcoDrive lambda and RouteAudioSample's VCO loop below,
// rather than each hand-writing the same offsets.
enum class VcoSlotRole : std::size_t { Pitch = 0, Shape = 3, PhaseMod = 6, RingMod = 9 };
constexpr std::size_t AudioSlot(std::size_t vco, VcoSlotRole role) {
    return vco + static_cast<std::size_t>(role);
}

inline constexpr std::size_t kFroggersBankCount = 6;
inline constexpr std::size_t kFroggersParamsPerBank = 14;  // bank slots 0-13
inline constexpr std::size_t kFroggersSlotsPerBank = 16;  // bank slots 0-15
inline constexpr std::size_t kFroggersCrispySlot = 14;
inline constexpr std::size_t kFroggersCrunchySlot = 15;

struct FroggersParamSpec {
    const char* name;
    const char* shortName;
    // The ordinary struct default (0.0f) is a DSP-wiring concern per
    // parameter -- most page parameters leave it as-is; the entries below
    // that need a different value say why at their own call site.
    // dsp::VcoAdsrState/MixOscVoices treats the Envelope
    // bank's Sustain knob as a literal target level (0..1, clamped), not an
    // ExpMap-style knob with a nonzero floor like every other bank's
    // parameters -- so leaving it at the ordinary 0.0f default means the
    // ASR permanently holds at zero level regardless of gate state,
    // silencing every VCO. Since nothing in this app
    // introduces a note-on/gate gesture, and a freshly-started app
    // must make sound with no user input,
    // the three Sustain parameters below are given a nonzero
    // default (1.0f, full level) here; Attack/Release stay at the ordinary
    // 0.0f (their ExpMap floor, `VcoAdsrState::kMinTimeSeconds`, is already
    // a fast-but-nonzero time, so 0.0f there is an "instant on" default, not
    // a silencing one).
    float defaultValue = 0.0f;
};

struct FroggersBankLayout {
    FroggersBankId id;
    const char* name;
    synth::Color color;
    std::array<FroggersParamSpec, kFroggersParamsPerBank> params;
};

// Independently re-derived from
// b9a8199^:desktop-v2/Source/V2DesktopPageDisplayNames.hpp's forHostPageRow
// (:125-164): grid rows < 7 come from kHostRowGrid[gridPage][row]
// (:148-155, gridPage = hostPage-1 for hostPage in [1,4]), rows 7-9 from
// kExpansionTailRowLabels[gridPage][row-7] (:156-159). kHostRowGrid's row
// arrays are 8 wide with a trailing index-7 "Crispy" that forHostPageRow
// never reads (documented dead data at V2DesktopPageDisplayNames.hpp's
// comment above kAudioRowLabels, :95-103) -- NOT copied wholesale here.
// Audio (hostPage 0) instead comes from kAudioRowLabels (:101-103, 7
// entries: rows 0-5 used, row 6 is Audio's own dead local Crispy) with the
// three Shape (VCO morph) controls added as ordinary on-grid slots
// (they are a separate global axis in v2,
// b9a8199^:desktop-v2/Source/manifest/FroggersV2AppManifest.hpp:118-125,
// folded onto the grid here -- a different parameter from Drive's own
// "Shape" wavefolder control). Envelope (hostPage 5) comes from
// kEnvelopeRowLabels (:81-86, 10 entries: rows 0-8 used, row 9 is Envelope's
// dead local Crispy).
//
// Result, independently confirmed:
// Bank ORDER follows the signal path -- it
// deliberately does not match the v2 source material's own listed order.
// The parameter CONTENT of each bank matches that source material.
//   Audio    -- VCO1, VCO2, VCO3, Shape 1/2/3, Phase mod 1/2/3
//   Envelope -- Attack/Sustain/Release x VCO1, VCO2, VCO3
//   Filter   -- Peak freq, Peak gain, Peak Q, Comb offset, Comb delay,
//               Comb feedback, Comb LP, Comb drive, Scoop mix, Scoop freq,
//               Scoop width, Scoop depth, Comb/Peak, Topology (grouped by
//               stage; the v2 source's own order predates the grouping)
//   Drive    -- Wet/Dry, Gain, Shape, SRR 1, SRR 2, XOR, Bit depth, Fuzz,
//               Phase
//   Delay    -- Wet/dry, Send, Delay time, Feedback, Stereo width, Freeze,
//               Mod depth, Reverse blend, Diffusion
//   Reverb   -- Wet/dry, Send, Room size, Decay, Pre-delay, Damping,
//               Stereo width, Diffusion, Mod, Hold
inline const std::array<FroggersBankLayout, kFroggersBankCount>& FroggersBankLayouts() {
    static const std::array<FroggersBankLayout, kFroggersBankCount> layouts{{
        {FroggersBankId::Audio, "Audio", synth::Color::Red, {{
            // The ordinary 0.0f default maps (via
            // Vco::PitchToPhaseIncrement, app/dsp/Vco.hpp,
            // f = kPitchMinHz * (kPitchMaxHz/kPitchMinHz)^knob) to
            // kPitchMinHz Hz (20 Hz) -- inaudible on laptop speakers.
            // 0.3087/0.4343/0.5077 = ln(f/kPitchMinHz)/ln(kPitchMaxHz/kPitchMinHz)
            // for f = 109.97/220.02/329.96 Hz (verified against
            // PitchToPhaseIncrement above). Sustain specs below set the
            // precedent for a nonzero `defaultValue` at this same call site.
            {"VCO1", "VCO1", 0.3087f}, {"VCO2", "VCO2", 0.4343f}, {"VCO3", "VCO3", 0.5077f},
            {"Shape 1", "Shp1"}, {"Shape 2", "Shp2"}, {"Shape 3", "Shp3"},
            // Named "Ph.mod N", not "Phase mod N": three whitespace-separated
            // tokens don't compress
            // to one 14-segment line under the two-line splitter's
            // trailing-single-digit rule (FroggersUiSurface.hpp's
            // SplitFourteenSegmentLines) the way "word + index" names do --
            // see that function's own citation. Long `name` only;
            // `shortName`/slot/default all unchanged.
            {"Ph.mod 1", "PM1"}, {"Ph.mod 2", "PM2"}, {"Ph.mod 3", "PM3"},
            // Ring Mod: the ordinary
            // unset 0.0f default already sits at/below each Ring Mod knob's
            // own zero floor (dsp::Vco::kRingModFloor, 0.05f) -- a fresh
            // launch's ring-mod amount is exactly 0 (dsp::Vco::
            // RingModDepthScale(0.0f) == 0), so it sounds identical to
            // patches with no ring modulation at all. No explicit default
            // needed here.
            // Named "Ringmod N", not "Ring mod N" -- same reasoning as
            // "Ph.mod N" above. Long `name` only; `shortName`/slot/default
            // unchanged.
            {"Ringmod 1", "RM1"}, {"Ringmod 2", "RM2"}, {"Ringmod 3", "RM3"},
            // Unset (0.0f) default sits at this knob's exponential-map
            // minimum -- dsp::Vco::kPmLfoMinHz, the slowest rate the PM LFO
            // can run. It plays no part in silencing PM: that job belongs
            // entirely to the per-VCO PM depth knobs above, whose own
            // 0.0f default gates the offset to exactly zero regardless of
            // this rate.
            {"PM rate", "PMrt"},
            // 0.5f is this mapping's own centre (dsp::
            // ComputeVcoBalanceWeights, VoiceEnvelope.hpp), which gives
            // exactly equal weights (1/3, 1/3, 1/3) by construction --
            // matching a simple hardcoded equal-thirds average
            // exactly, so a fresh launch sounds balanced across all three VCOs.
            {"VCO balance", "VBal", 0.5f},
        }}},
        {FroggersBankId::Envelope, "Envelope", synth::Color::Green, {{
            {"Attack VCO1", "A1"}, {"Decay VCO1", "D1"}, {"Sustain VCO1", "S1", 1.0f}, {"Release VCO1", "R1"},
            {"Attack VCO2", "A2"}, {"Decay VCO2", "D2"}, {"Sustain VCO2", "S2", 1.0f}, {"Release VCO2", "R2"},
            {"Attack VCO3", "A3"}, {"Decay VCO3", "D3"}, {"Sustain VCO3", "S3", 1.0f}, {"Release VCO3", "R3"},
            {"Curve", "Curv"}, {"Grace", "Grac"},
        }}},
        {FroggersBankId::Filter, "Filter", synth::Color::Blue, {{
            {"Peak freq", "PkFreq"}, {"Peak gain", "PkGain"}, {"Peak Q", "PkQ"},
            {"Comb offset", "CmbOff"}, {"Comb delay", "CmbDly"},
            // 0.5f is this bipolar knob's centre: GetFeedback maps it to
            // exactly zero feedback (dsp/FilterFx.hpp), so a fresh
            // launch's comb passes signal through without ringing -- the
            // same centred-default treatment VCO balance has above.
            {"Comb feedback", "CmbFb", 0.5f},
            {"Comb LP", "CmbLP"},
            // Comb drive default 0.5f -> ExpMapCompute(0.25f, 4.0f, 0.5f)
            // == 0.25f * sqrt(16.0f) == 1.0f, unity gain (see
            // RouteAudioSample's own Filter slot 7 wiring) -- a fresh
            // launch matches the always-parallel, unscooped signal path.
            {"Comb drive", "CDrv", 0.5f}, {"Scoop mix", "ScMix"},
            // Scoop freq/width/depth defaults, chosen so a fresh launch
            // matches the always-parallel, unscooped signal path:
            //   Scoop freq/width 0.0f -> ExpMapCompute(min,max,0)==min,
            //     the SAME min the Peak freq/width knobs (slots 0/2) reach
            //     at their own current 0.0f defaults -- reproduces exactly
            //     what bumpFreq/bumpWidth give scoopNotch.
            //   Scoop depth 0.0f -> same default the existing Scoop
            //     parameter (slot 8, above) already carries.
            {"Scoop freq", "ScFq", 0.0f}, {"Scoop width", "ScWd", 0.0f}, {"Scoop depth", "ScDp", 0.0f},
            {"Comb/Peak", "Cmb/Pk"},
            // Topology default 0.0f -> FilterFxChain::Process's
            // topology==0, bit-identical to the always-parallel behaviour
            // a fresh launch matches.
            {"Topology", "Topo", 0.0f},
        }}},
        {FroggersBankId::Drive, "Drive", synth::Color::Orange, {{
            {"Wet/Dry", "Wet"}, {"Gain", "Gain"}, {"Shape", "Shape"}, {"SRR 1", "SRR1"},
            {"SRR 2", "SRR2"}, {"XOR", "XOR"}, {"Bit depth", "BitDp"}, {"Fuzz", "Fuzz"},
            // Default knob 0.86f -- MEASURED (dsp/Drive.hpp's
            // DriveBlendPhase::Process, Wet/Dry 0.25, Gain 0.5, 220 Hz): the
            // knob whose break frequency lands on 220 Hz itself, where the
            // allpass rotates its own input by exactly -90 degrees and the
            // wet path -- already close to antiphase with dry at this Gain
            // setting -- lands in quadrature with it instead of cancelling
            // (knob 0, today's old default) or reinforcing it (knob 1). Only
            // matters once Wet/Dry is off its own floor -- at Wet/Dry 0 this
            // stage returns dry exactly regardless of Phase, so the app's
            // registered default patch is unaffected by this default moving.
            {"Phase", "Phase", 0.86f},
            // Anti-alias brightness default knob 1.0f -- the crossfade's
            // ALL-GRIT end (dsp/Drive.hpp's Oversampler2x::SetAntiAliasBrightness,
            // cleanMix = 1 - knob^1.5), bit-identical to what shipped before the
            // knob was repurposed into a clean/grit crossfade, so no
            // existing preset changes until it is moved off this end.
            // Feedback default knob 0.0f -- SetFeedback's
            // `kMaxFeedbackCoefficient * knob` mapping reproduces exactly
            // 0.0f (no feedback) there -- see FrogBlock::SetFeedback's own
            // comment (dsp/Drive.hpp).
            {"Anti-alias brightness", "ABrt", 1.0f}, {"Feedback", "Fb", 0.0f},
            // Fold default knob 0.5f -- ExpMapCompute(16,1,·) reproduces
            // the fixed 4.0f divisor literal exactly at knob==0.5f -- see
            // SetFold's own comment (dsp/Drive.hpp).
            {"Fold", "Fold", 0.5f},
            // Default knob 1.0f -- ExpMapCompute(0.02,1.0,1.0) == 1.0
            // exactly, an exact-identity (bypass) alpha, see SetTone's own
            // comment (dsp/Drive.hpp).
            // Symmetry default knob 0.5f -- SetSymmetry's bipolar
            // `0.02f * (2*knob - 1)` mapping reproduces exactly 0.0f (no
            // offset) at the knob's own CENTRE, not its floor -- see
            // FrogBlock::SetSymmetry's own comment (dsp/Drive.hpp).
            {"Tone", "Tone", 1.0f}, {"Symmetry", "Sym", 0.5f},
        }}},
        {FroggersBankId::Delay, "Delay", synth::Color::Rgb(255, 105, 180), {{
            {"Wet/dry", "Wet"}, {"Send", "Send"}, {"Delay time", "DlyTm"},
            {"Feedback", "Fb"}, {"Stereo width", "Width"}, {"Freeze", "Frze"},
            {"Mod depth", "ModDp"}, {"Reverse blend", "Rev"}, {"Diffusion", "Diff"},
            // Default knob 0.5f -- FbDr's
            // ExpMapCompute(0.25,4,·) reproduces unity (1.0f) exactly at
            // 0.5f, MdRt's ExpMapCompute(0.05,1.25,·) reproduces 0.25Hz
            // exactly at 0.5f -- see each setter's own comment
            // (dsp/Delay.hpp).
            {"Feedback drive", "FbDr", 0.5f},
            // Default knob 1.0f -- FbTn's alpha reaches an exact
            // bypass (1.0f) at knob 1.0f (same idiom as Drive's Tone
            // above); WBal's identity map reproduces widthBalance == 1.0f,
            // i.e. fixed 0.35f/0.5f literals, exactly at knob 1.0f.
            {"Feedback tone", "FbTn", 1.0f}, {"Mod rate", "MdRt", 0.5f},
            {"Width balance", "WBal", 1.0f},
            // Default knob 0.0f -- SetCrush's own mapping gives
            // freq==1.01 (>= 1.0f) at knob 0.0f, SampleRateReducer's exact
            // bypass branch, i.e. no crushing.
            {"Crush", "Crsh", 0.0f},
        }}},
        {FroggersBankId::Reverb, "Reverb", synth::Color::Cyan, {{
            {"Wet/dry", "Wet"}, {"Send", "Send"}, {"Room size", "Room"}, {"Decay", "Decay"},
            {"Pre-delay", "PreDly"}, {"Damping", "Damp"}, {"Stereo width", "Width"},
            {"Diffusion", "Diff"}, {"Mod", "Mod"}, {"Hold", "Hold"},
            // Default knob 0.5f for
            // TkDv/Tilt/Tund -- TkDv's ExpMapCompute(0.25,4,·) reproduces
            // unity (1.0f) exactly, Tilt's centre crossfade weight is exactly
            // 0.0f, Tund's (2*knob-1) offset is exactly 0 samples -- all at
            // knob==0.5f -- see each constant's own comment (dsp/Reverb.hpp).
            // Grit defaults 0.0f -- dsp::DigitalReorganizer::SetFlip/SetHash
            // both reduce to flip==0/hashBits==0 at knob 0.0f, its own exact
            // bypass (same file).
            {"Tank drive", "TkDv", 0.5f}, {"Grit", "Grit", 0.0f},
            {"Tilt", "Tilt", 0.5f}, {"Tuned", "Tund", 0.5f},
        }}},
    }};
    return layouts;
}

// Crunchy is a single fixed global-control colour in every bank (it
// cannot take on each bank's colour -- it is one shared Parameter with one
// baseColor). Distinct from all six bank colours above.
inline synth::Color FroggersCrunchyColor() { return synth::Color::Yellow; }

class FroggersParameterModel {
public:
    // ParameterGroupConfig{numVoices=1, numModulators=15,
    // numScenes=..., maxParameters=...} (External/Sheaf/projects/synth/include/synth/ParameterModulation.hpp:195-198).
    static constexpr std::size_t kNumVoices = 1;
    static constexpr std::size_t kNumModulators = 15;  // The slate's source count (FroggersModulationSlate registers all 15).
    // Two scenes, matching apps/braid-4's convention (External/Sheaf/projects/synth/apps/braid-4/Braid4Core.hpp:125,137)
    // and the single scene-blend slider the surface uses for
    // the chrome band -- one scene *pair*.
    //
    // The COUNT (two scenes, one
    // blend) still matches Braid 4, but the SCENE BUTTONS' behaviour no
    // longer does. Braid 4's own S1/S2 (apps/braid-4/Braid4UiModel.hpp:
    // 402-404) dispatch `SetLessSelectedScene`, which reassigns which stored
    // scene occupies the less-weighted endpoint and never touches the
    // blend -- at either blend extreme, pressing the button that already
    // owns the dominant endpoint does nothing audible. This app's own
    // buttons (FroggersUiSurface.hpp's AppendChromeBand/HandleAction) push
    // `MessageIn::SetSceneBlend(0.0f/1.0f)` directly instead, so pressing
    // either one is always audible. This is a deliberate product decision,
    // not an oversight -- do not "restore" Braid 4's convention here.
    static constexpr std::size_t kNumScenes = 2;
    // Sized for the 91 top-level parameters this class itself registers (6
    // banks x 14 page
    // parameters + 6 per-bank Crispy + 1 shared Crunchy = 91) plus a little
    // slack. Modulation-depth parameters (up to 1365 level-1, 91 x 15, plus
    // more at deeper levels) are deliberately NOT sized for here -- that growth
    // rides ParameterGroup's own storage-batch request mechanism
    // (RequestParameterStorageBatch / ParameterMessageOutBus, or the direct
    // provisioning FroggersModulationSlate::Init() does) instead; out of
    // this class's own "parameter/bank model only" scope.
    static constexpr std::size_t kMaxParameters = 96;

    // Attaches the Filter bank's bump/comb
    // transfer-function visualizers as underlays via
    // `ParameterConfig::visualizer`'s per-parameter underlay
    // mechanism (distinct from
    // `ModulatorMetadata::visualizer`, which is for modulation
    // SOURCES, not ordinary page parameters). Both default to nullptr so
    // every existing bare-ParameterManager test fixture (which does not care
    // about visualizer wiring) is unaffected. The two live DSP objects these
    // visualizers sample (FroggersApp's `filterChain_.peak`/`.comb`) are
    // owned by FroggersApp, a different object than this class -- so the
    // visualizer objects themselves are FroggersApp's own members,
    // constructed before FroggersApp::Init() calls this method, and simply
    // passed in here rather than owned by this class.
    void Init(synth::ParameterManager& manager, synth::ui::Visualizer* peakVisualizer = nullptr,
              synth::ui::Visualizer* combVisualizer = nullptr) {
        manager_ = &manager;
        group_ = &manager.CreateGroup({
            .numVoices = kNumVoices,
            .numModulators = kNumModulators,
            .numScenes = kNumScenes,
            .maxParameters = kMaxParameters,
        });

        // Crunchy is ONE Parameter, created before any bank so
        // the identical pointer can be registered into all six banks below.
        const synth::ParameterId crunchyId = manager.RegisterParameter(*group_, synth::ParameterConfig{
            .name = "Crunchy",
            .shortName = "Crnchy",
            .baseColor = FroggersCrunchyColor(),
        });
        crunchy_ = &manager.ParameterById(crunchyId);

        // One shared BankSlot, 16 physical encoders (slots 0-15),
        // matching Braid4Core's wiring order (create slot -> add physical
        // encoders before any Bank::RegisterParameters call, since that call
        // requires an associated slot with a full physical layout already
        // present -- External/Sheaf/projects/synth/src/ParameterModulation.cpp:2559-2566 in
        // External/Sheaf).
        slot_ = &manager.CreateBankSlot();
        for (synth::PhysicalEncoderId encoderId = 0; encoderId < kFroggersSlotsPerBank; ++encoderId) {
            slot_->AddPhysicalEncoder(encoderId);
        }

        const auto& layouts = FroggersBankLayouts();
        for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
            const FroggersBankLayout& layout = layouts[bankIx];

            synth::Bank& bank = manager.CreateBank();
            bank.SetBankColor(layout.color);
            // BankSlot::SelectBank associates a bank with this slot the
            // first time it is selected (Bank::AssociateSlot, idempotent for
            // repeated association with the same slot -- it throws only on
            // a *different* slot, External/Sheaf/projects/synth/src/ParameterModulation.cpp:2776-2781)
            // without permanently making it the *active* bank -- the final
            // SelectBank call after this loop sets the real default.
            slot_->SelectBank(&bank);

            // Fourteen page parameters at offset 0, this bank's
            // colour on every one of them (Bank::BankColor() itself renders
            // nothing; encoder colour reads
            // only ParameterConfig::baseColor).
            //
            // STRUCTURAL FACT: ParameterManager::RegisterParameter
            // enforces GLOBAL name uniqueness across the whole manager
            // (`parameterNames_`, External/Sheaf/projects/synth/src/ParameterModulation.cpp:3069-3071 in
            // External/Sheaf) -- a stricter check than Bank::RegisterParameters's
            // own per-call-only duplicate check
            // (:2572-2578). The per-page labels are page-LOCAL in the
            // original product and genuinely repeat across pages ("Stereo
            // width" is both a Reverb and a Delay row; "Wet/dry" is both a
            // Reverb and a Delay row) -- confirmed by the independent
            // re-derivation above, not an invented collision. A verbatim
            // Parameter::Name() per spec.name would throw "duplicate
            // parameter name" the second page registers "Stereo width" or
            // "Wet/dry". Resolution: qualify the internal, global-namespace
            // Name() with the bank name ("Reverb Stereo width" / "Delay
            // Stereo width"), while leaving ShortName() -- the field the
            // encoder grid actually renders (External/Sheaf/projects/synth/include/synth/EncoderDraw.hpp:322) -- as the
            // authentic, page-local, possibly-repeated label the original
            // product uses. This changes no on-screen text and no
            // parameter's identity/semantics; it only disambiguates the
            // internal name Sheaf uses for its own bookkeeping (and, later,
            // patch JSON keys).
            std::array<synth::Parameter*, kFroggersParamsPerBank> pageParams{};
            for (std::size_t paramIx = 0; paramIx < kFroggersParamsPerBank; ++paramIx) {
                const FroggersParamSpec& spec = layout.params[paramIx];
                const std::string qualifiedName = std::string(layout.name) + " " + spec.name;
                // Filter bank paramIx 0-2 are "Peak freq/Peak
                // gain/Peak Q" (this bank's ResonantBump) and paramIx 4-6
                // are "Comb delay/Comb feedback/Comb LP" (this bank's Comb)
                // -- see FroggersBankLayouts()'s own Filter row above. Comb
                // offset (3, the PureDelay ahead of the comb -- a timing
                // control, not part of either curve's own shape),
                // Comb/Peak (12, a blend) and Scoop (8, a second
                // ResonantBump instance) deliberately
                // get no transfer-function underlay here.
                synth::ui::Visualizer* visualizer = nullptr;
                if (layout.id == FroggersBankId::Filter) {
                    if (paramIx <= 2) {
                        visualizer = peakVisualizer;
                    } else if (paramIx >= 4 && paramIx <= 6) {
                        visualizer = combVisualizer;
                    }
                }
                const synth::ParameterId id = manager.RegisterParameter(*group_, synth::ParameterConfig{
                    .name = qualifiedName,
                    .shortName = spec.shortName,
                    .defaultValue = spec.defaultValue,
                    .baseColor = layout.color,
                    .visualizer = visualizer,
                });
                pageParams[paramIx] = &manager.ParameterById(id);
            }
            bank.RegisterParameters(pageParams, /*offset=*/0);

            // Local Crispy at slot 14, this bank's colour
            // (Crispy is per-bank and DOES take the bank colour, unlike
            // Crunchy). Named "<Bank> Crispy" for global uniqueness across
            // the ParameterManager's flat name space -- the six banks each
            // have their own Crispy Parameter object (unlike Crunchy).
            const std::string crispyName = std::string(layout.name) + " Crispy";
            const synth::ParameterId crispyId = manager.RegisterParameter(*group_, synth::ParameterConfig{
                .name = crispyName,
                .shortName = "Crispy",
                .baseColor = layout.color,
            });
            synth::Parameter* crispyParam = &manager.ParameterById(crispyId);
            std::array<synth::Parameter*, 1> crispyArr{crispyParam};
            bank.RegisterParameters(crispyArr, kFroggersCrispySlot);

            // The SAME Crunchy Parameter* at slot 15 in every
            // bank. Bank::RegisterParameters's duplicate-visible-name check
            // only looks within the span passed to a single call
            // (External/Sheaf/projects/synth/src/ParameterModulation.cpp:2572-2578 in External/Sheaf), so
            // one call per bank registering this shared pointer never
            // collides with itself, and there is no Parameter->Bank
            // back-pointer anywhere to object to the same Parameter
            // appearing in six banks.
            std::array<synth::Parameter*, 1> crunchyArr{crunchy_};
            bank.RegisterParameters(crunchyArr, kFroggersCrunchySlot);

            banks_[bankIx] = &bank;
            crispy_[bankIx] = crispyParam;
            for (std::size_t paramIx = 0; paramIx < kFroggersParamsPerBank; ++paramIx) {
                pageParameters_[bankIx][paramIx] = pageParams[paramIx];
            }
        }

        // Default active bank -- a starting
        // selection; the surface control that actually drives bank
        // selection lives in FroggersUiSurface.hpp.
        slot_->SelectBank(banks_[0]);

        // Scenes wired, two endpoints matching kNumScenes above.
        // Blend defaults to 0.0 (pure left/scene-0), matching
        // ParameterManager::UIState's own default (External/Sheaf/projects/synth/include/synth/ParameterModulation.hpp:769).
        manager.SetSceneEndpoints(0, 1);
    }

    // Drives every top-level parameter's per-sample scene-blend
    // interpolation and UI-display slew (the change reaches the
    // published ring state), plus the fuego seam
    // between the two group-wide phases -- mirroring Braid's
    // ProcessParameterPhase1() -> FilterParameterCaches() ->
    // ProcessParameterPhase2() (External/Sheaf/projects/synth/apps/braid-4/Braid4Core.hpp:457-459). Safe to call every
    // sample regardless of how many of the 15 modulation sources are
    // connected,
    // because UpdateModValues()/route processing are no-ops for unconnected
    // modulator slots.
    void ProcessSample(std::uint64_t sampleIndex) {
        group_->UpdateModValues();
        group_->ProcessSamplePhase1(sampleIndex);
        ApplyFuegoSeam();
        group_->ProcessSamplePhase2();
    }

    synth::ParameterGroup& Group() { return *group_; }
    const synth::ParameterGroup& Group() const { return *group_; }
    synth::BankSlot& Slot() { return *slot_; }
    synth::Bank& BankAt(std::size_t bankIx) { return *banks_.at(bankIx); }
    synth::Bank& BankAt(FroggersBankId id) { return BankAt(static_cast<std::size_t>(id)); }
    synth::Parameter& PageParameter(std::size_t bankIx, std::size_t paramIx) {
        return *pageParameters_.at(bankIx).at(paramIx);
    }
    synth::Parameter& PageParameter(FroggersBankId bankId, std::size_t paramIx) {
        return PageParameter(static_cast<std::size_t>(bankId), paramIx);
    }
    synth::Parameter& Crispy(std::size_t bankIx) { return *crispy_.at(bankIx); }
    synth::Parameter& Crispy(FroggersBankId bankId) { return Crispy(static_cast<std::size_t>(bankId)); }
    synth::Parameter& Crunchy() { return *crunchy_; }

private:
    // The ONE fuego application
    // point -- called exactly once per sample from ProcessSample(), between
    // group_->ProcessSamplePhase1() (which just wrote each parameter's raw,
    // modulated value into its cached-knob slot via ProcessLitePhase1's
    // `currentKnobValues_[v] = GetRaw(v)`) and group_->ProcessSamplePhase2()
    // (which slews UIDisplayCenter toward whatever the cache holds now).
    // Mirrors Braid's FilterParameterCaches() (External/Sheaf/projects/synth/apps/braid-4/Braid4Core.hpp:569-627): read
    // Parameter::CachedKnobValue(), transform, write back with
    // Parameter::ReplaceCachedKnobValue() -- so the fuegoized value is what
    // both a future DSP consumer and the UI-display slew inherit, with
    // no separate write path.
    //
    // Row identity: each parameter's 0-based slot
    // index within its 16-slot bank -- page params are rows 0-13, Crispy is
    // row kFroggersCrispySlot (14) in every bank. Crunchy is the shared
    // global control (not itself keyed to a "row" here).
    //
    // Voice index is always 0 -- this app's mono model (kNumVoices == 1).
    void ApplyFuegoSeam() {
        constexpr std::size_t kVoiceIx = 0;

        // Global
        // Crunchy receives NO fuego stage at all. It is the source of the
        // warp, and warping Crunchy by Crunchy is self-referential; the
        // cascade (the retired simulator's f236915^:sim/V2FuegoStack.hpp:9-23) only defines the treatment of
        // ordinary parameters (both stages) and of a Crispy control (global
        // stage only, src/core/Page.hpp:203-207) -- it says nothing about
        // Crunchy itself now that it is on the grid at slot 15. This
        // is an INFERENCE from the v2 Crispy-row precedent, not a traced
        // fact. Implemented as
        // specified: simply never call ReplaceCachedKnobValue() on
        // crunchy_ here, so its cached value stays exactly what
        // ProcessLitePhase1 already wrote (the raw, unwarped value).
        const float globalCrunchy = crunchy_->CachedKnobValue(kVoiceIx);

        for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
            synth::Parameter& crispy = *crispy_[bankIx];
            const float crispyPreFuego = crispy.CachedKnobValue(kVoiceIx);

            // A Crispy control receives ONLY the global stage --
            // it is itself Crunchy-warped before use as the per-bank
            // cascade key below (the retired simulator's f236915^:sim/V2FuegoStack.hpp:9-23's ApplyGlobal
            // call on crispyKnobPreFuego; v2 wiring proof at
            // src/core/Page.hpp:203-207). This is also exactly the value a
            // DSP/UI consumer of the Crispy parameter itself would read.
            const float crispyAfterCrunchy = synth_froggers::dsp::FuegoStack::ApplyGlobal(
                crispyPreFuego, globalCrunchy, static_cast<uint8_t>(kFroggersCrispySlot));
            crispy.ReplaceCachedKnobValue(kVoiceIx, crispyAfterCrunchy);

            // Ordinary (page) parameters get both stages -- global
            // Crunchy then this bank's Crispy -- via the full musical-row
            // cascade (the retired simulator's f236915^:sim/V2FuegoStack.hpp:14-23). Passing crispyPreFuego
            // (not crispyAfterCrunchy) matches ApplyMusicalRow's own
            // signature: it re-derives the Crunchy-warped Crispy value
            // internally from the pre-fuego knob, identically to the
            // crispy.ReplaceCachedKnobValue() call just above.
            for (std::size_t paramIx = 0; paramIx < kFroggersParamsPerBank; ++paramIx) {
                synth::Parameter& param = *pageParameters_[bankIx][paramIx];
                const float raw = param.CachedKnobValue(kVoiceIx);
                const float fuegoized = synth_froggers::dsp::FuegoStack::ApplyMusicalRow(
                    raw, globalCrunchy, crispyPreFuego,
                    static_cast<uint8_t>(paramIx), static_cast<uint8_t>(kFroggersCrispySlot));
                param.ReplaceCachedKnobValue(kVoiceIx, fuegoized);
            }
        }
    }

    synth::ParameterManager* manager_ = nullptr;
    synth::ParameterGroup* group_ = nullptr;
    synth::BankSlot* slot_ = nullptr;
    synth::Parameter* crunchy_ = nullptr;
    std::array<synth::Bank*, kFroggersBankCount> banks_{};
    std::array<synth::Parameter*, kFroggersBankCount> crispy_{};
    std::array<std::array<synth::Parameter*, kFroggersParamsPerBank>, kFroggersBankCount> pageParameters_{};
};

}  // namespace synth_froggers
