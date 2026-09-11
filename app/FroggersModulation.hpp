#pragma once

// synth_froggers::{FroggersModulationSlate, FroggersModulationDrillIn,
// RandomizeAll, RandomizePage, ApplyFroggersDefaultPatch} -- registers and
// steps the 15 modulation sources, in a fixed load-bearing order (below), and
// owns drill-in/randomize/reset for the modulation-depth grid. The VCO/EF DSP
// stepped below, which produces the six audio-rate/envelope-follower source
// values (slots 6-11), also feeds FroggersAppCore's RouteAudioSample(),
// which sums it to the real stereo output bus. app/dsp/*.hpp's DSP port is
// reused as-is; nothing here re-derives a Froggers formula.
//
// ============================================================================
// Registration, in a load-bearing order
// ============================================================================
// `StandardModulators` is NOT used: `kRandomCount = 4` is a
// hard `static constexpr` (include/synth/StandardModulators.hpp:27) with four
// hardcoded visualizers (:46-49,188-191), so it cannot yield six S&H sources,
// and with slots 0-5 already taken by the six Random S&H sources there is
// nowhere left to put its four randoms anyway. All 15 sources are registered
// directly via `Modulators::SetModulationSource`
// (src/ParameterModulation.cpp:552-572, reached here through
// `ParameterGroup::SetModulationSource`, the same public wrapper). Standalone
// Sheaf pieces reused: `NoiseModulatorProcessor` (DspNoise.hpp:48-54),
// `NoiseWaveformVisualizer`, `GangedRandomLfoVisualizer` (:247-248).
//
// Slot order -- SetModulationSource bounds-checks only
// and has no reservation registry, so registration order is load-bearing;
// each slot below is registered by its own explicit call (never a loop that
// could silently reorder), and a test asserts all 15 by identity:
//   0-5   Random S&H 1-6 (5 from the ported RandomShLane, #6 a Sheaf
//         GangedRandomLfoProcessor<1>)
//   6-8   VCO 1/2/3 audio out
//   9-11  VCO 1/2/3 envelope follower
//   12    Noise
//   13-14 external audio rate, external audio envelope follower
//
// ============================================================================
// Clock/rate wiring
// ============================================================================
// The five RandomShLane sources' `Increment()` (advance to a new held value)
// is driven by MasterClock ticks: `FroggersApp::ProcessBlock` computes
// the transport quarter-note position ONCE per sample via its own
// `TransportQuarterNotesAt()` helper (the same clock-read helper the ASR gate
// uses, reused here rather than re-derived) and passes
// that same `std::optional<double>` into `Step()`, which runs each of the
// five sources' own `synth::Phasor2Tick` (multiplier 3/2/1 for sources
// 1-3; sources 4 and 5 pre-scale `time` by 1/2 and 1/4 with multiplier 1
// -- the rate table below)
// and calls `Increment()` on a tick. Source #6 is NOT tick-driven:
// `PrepareBlockClock()`, called once per block from
// `FroggersApp::ProcessBlock` BEFORE the per-sample loop, recomputes its
// `GangedRandomLfoInput` from `block.clockPlan->QuarterNotesPerSample()` so
// one full move-cycle spans 16 quarter notes (four bars), clamping to
// a safe fallback (120 BPM-equivalent) when no clock plan is available so
// `GangedRandomLfoProcessor::Process`'s `ValidateRandomTimingConfig` never
// throws (`DspRandomLfo.hpp:26-30`).
//
// ============================================================================
// Source-value convention: normalized [0,1], matching Sheaf's own modulator
// sources
// ============================================================================
// Verified, not invented: `GangedRandomLfoProcessor`'s targets are drawn via
// `DefaultRandomDrawSource::Uniform01()` (DspRandomLfo.hpp:277-279,
// std::uniform_real_distribution<float>{0.0f,1.0f}) and
// `NoiseModulatorProcessor::Process()` writes `random_.UniformOpen01()`
// (DspNoise.hpp:69-71) -- both already [0,1]. `apps/braid-4/Braid4Core.hpp`
// renormalizes its own bipolar audio/LFO sources into [0,1] before
// registering them (`NormalizeMatrixOutput`, `0.5 + 0.5*clamp(x,-1,1)`,
// :420-422,367-378) rather than passing raw bipolar signal through. This
// port follows the same convention: `RandomShLane::Process()` and
// `VcoEnvelopeFollowers`/`SingleEnvelopeFollower` are already [0,1], and the
// two genuinely bipolar signals here (VCO audio, external audio)
// are renormalized with the identical `0.5 + 0.5*clamp(x,-1,1)` formula
// below (`NormalizeBipolarToUnit`).
//
// ============================================================================
// Drill-in level cap
// ============================================================================
// Sheaf's `Bank` has no level concept at all: one `Parameter* selected_`
// (ParameterModulation.hpp:661) plus one bool computed from it
// (`ShowingModulation()`, :2710-2712); `Bank::HandlePress` opens a
// modulation view for ANY non-selected pressed cell regardless of how deep
// the current view already is (`OpenModulationView`, :2813-2859), so it will
// happily descend to a third, fourth, ... level. `FroggersModulationDrillIn`
// below is the app-side level counter (0 = parameter grid, 1 = level-1
// mod-detail grid, 2 = level-2 depth grid) that refuses to forward a press
// that would open a third level, and otherwise defers entirely to Bank's
// native behavior for PRESSES (including `Deselect()`'s own
// full-exit-from-any-level semantics, unchanged and un-changeable -- it is
// Sheaf's, not this app's). `FroggersModulationDrillIn::Back()` does not
// simply forward that raw `Deselect()` call for every level -- from level 2
// it synthesizes a one-level pop app-side (`Deselect()` then re-press the
// remembered level-1 encoder id), landing back on the level-1 view instead
// of a full exit. See `Back()`'s own comment for the exact mechanism.
//
// ============================================================================
// Randomize: Sheaf remains the only mutator
// ============================================================================
// `Bank::RandomizeModulationDepths` and the `Modifier::Random`/`RandomMod`
// dispatch inside `Bank::HandlePress` (src/ParameterModulation.cpp:2633-2650,
// :2861-2913) are PRIVATE to `Bank` -- there is no public API to invoke them
// on an arbitrary parameter directly. The sanctioned public path (also how
// `ParameterManager::SelectBankForSlot`/`NavigateBankForSlot` themselves
// trigger a bank-wide randomize, :3490-3532) is: hold the desired modifier
// (`ParameterManager::SetRandomHeld`/`SetRandomModHeld`, both public), then
// dispatch a press at the target cell; `Bank::HandlePress`'s modifier branch
// (:2633-2642) applies `Modifier::Random`/`RandomMod` to that ONE cell's
// parameter and returns before touching `selected_`/level state at all. This
// governs DEPTH randomization below: the app "chooses the target set"
// (which cells get pressed, and in what view) while Sheaf performs every
// mutation. Nothing here reimplements `RandomizeModulationDepths`'s
// coin-flip loop or `RandomizeVisibleValue`.
//
// `Bank::ApplyModifierToTopLevel` (public) is deliberately NOT used for the
// parameter-page cases below: it applies to every cell in `topLevel_`
// unconditionally, including the shared Crunchy at slot 15, which this app
// excludes from randomize. Cell-by-cell press dispatch (via
// `FroggersModulationDrillIn::PressEncoder`, which never presses slot 15)
// is what makes the exclusion possible without touching Bank's private
// surface.
//
// The paragraph above does not hold for the VALUE path.
// `Parameter::RandomizeVisibleValue` (called by `Bank::ApplyModifierToParameter`
// under a held `Modifier::Random` press, src/ParameterModulation.cpp:1723-1731)
// computes its delta against `TargetValue(0)`, the MODULATION-RESOLVED value --
// so under live audio-rate modulation each press ratchets the commanded value
// into the [0,1] clamp (measured 20/20 at exactly 1.0000 after 5
// presses). `PressBankWithRandomValue` below does not press through
// `Bank::HandlePress`/`Modifier::Random` at all for the value write: it draws
// its own uniform value and commits it directly via `HandleSetAbsolute`
// (see that function's own comment). Depth randomization differs --
// `RandomizeParameterModulationDepths` above still calls Sheaf's
// `RandomizeVisibleValue` directly, which is correct because a freshly
// zeroed depth has no live modulation of its own to resolve against.

#include "FroggersParameters.hpp"
#include "FroggersRandomShVisualizer.hpp"
#include "dsp/EnvelopeFollowers.hpp"
#include "dsp/RandomShLane.hpp"
#include "dsp/Vco.hpp"

#include "synth/Color.hpp"
#include "synth/DspNoise.hpp"
#include "synth/DspPhasor2Tick.hpp"
#include "synth/DspRandomLfo.hpp"
#include "synth/GangedRandomLfoVisualizer.hpp"
#include "synth/NoiseWaveformVisualizer.hpp"
#include "synth/ParameterModulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <span>
#include <utility>
#include <vector>

namespace synth_froggers {

// The slate's fixed source order, named for readability at call sites.
enum FroggersModulatorSlot : std::size_t {
    kModSlotRandomSh1 = 0,
    kModSlotRandomSh2 = 1,
    kModSlotRandomSh3 = 2,
    kModSlotRandomSh4 = 3,
    kModSlotRandomSh5 = 4,
    kModSlotRandomSh6 = 5,
    kModSlotVco1Audio = 6,
    kModSlotVco2Audio = 7,
    kModSlotVco3Audio = 8,
    kModSlotVco1Ef = 9,
    kModSlotVco2Ef = 10,
    kModSlotVco3Ef = 11,
    kModSlotNoise = 12,
    kModSlotExternalAudio = 13,
    kModSlotExternalAudioEf = 14,
};

inline constexpr std::size_t kFroggersNumRandomShLanes = 5;  // sources 1-5; #6 is the GangedRandomLfo

// 0.5 + 0.5*clamp(x,-1,1): the same renormalization apps/braid-4 uses for its
// own bipolar audio/LFO modulation sources (Braid4Core.hpp:420-422), so a
// genuinely-bipolar signal (VCO audio, external audio) matches this
// framework's established [0,1] modulator-source convention (see this file's
// header comment).
inline float NormalizeBipolarToUnit(float bipolar)
{
    const float clamped = std::min(std::max(bipolar, -1.0f), 1.0f);
    return 0.5f + 0.5f * clamped;
}

// Bank::HandlePress's single-argument overload derives its own physical
// layout from `Bank::CompactPhysicalLayout()`, which returns only the
// encoder ids this bank has actually registered a parameter at
// (topLevel_.size(), src/ParameterModulation.cpp:2935-2941) -- 11 for every
// Froggers bank (9 page params + Crispy + Crunchy; this bank's slots 9-13
// are deliberately left empty). `Bank::OpenModulationView` requires
// `physicalLayout.size() >= numModulators + 1` = 16
// (ParameterModulation.cpp:2838-2841), so the single-arg overload throws
// ("modulation view has more modulators than slot depth positions") the
// first time any parameter here is pressed. The two-arg overload takes an
// EXPLICIT layout instead; `BankSlot::PhysicalEncoders()` (public) already
// holds the full 0-15 layout (`FroggersParameterModel::Init` adds all 16
// physical encoders to the shared slot before any bank registers a single
// parameter), so every press dispatch below uses that instead of the
// single-arg convenience overload.
inline std::span<const synth::PhysicalEncoderId> FullPhysicalLayout(synth::Bank& bank) {
    return bank.AssociatedSlot()->PhysicalEncoders();
}

// Owns the DSP behind this slate's 15 modulation sources and their
// registration. Constructed once, never moved (its `source6Visualizer_`
// member holds a reference into `gangedRandomLfo6_`'s own UiState, so this
// object's address must stay stable for its whole lifetime -- exactly the
// same convention `FroggersParameterModel`/`FroggersApp` already follow as
// plain, non-relocated members).
class FroggersModulationSlate {
public:
    // Every launch seeds the six random sources differently: one
    // std::random_device draw, taken once here, is mixed into each lane's
    // fixed seed by LaneSeed() and seeds source 6's own draw source. A test
    // that wants a reproducible slate passes the salt itself.
    FroggersModulationSlate() : FroggersModulationSlate(std::random_device{}()) {}

    explicit FroggersModulationSlate(std::uint32_t launchSalt)
        : randomShLanes_{
              dsp::lanes::MakeSource1(LaneSeed(0, launchSalt)),
              dsp::lanes::MakeSource2(LaneSeed(1, launchSalt)),
              dsp::lanes::MakeSource3(LaneSeed(2, launchSalt)),
              dsp::lanes::MakeSource4(LaneSeed(3, launchSalt)),
              dsp::lanes::MakeSource5(LaneSeed(4, launchSalt)),
          },
          gangedRandomLfo6_(launchSalt),
          noiseProcessor_(/*voiceCount=*/1),
          noiseVisualizer_(synth::Color::White),
          source6Visualizer_(gangedRandomLfo6_.UiState(), /*drawBackground=*/false),
          // The five X-style sources' own Visualizer, one per lane, each
          // bound to this instance's own
          // randomShLaneUiStates_[i] (populated each block by
          // PublishUiState()) -- same "constructed once, never moved"
          // convention as source6Visualizer_ above (Visualizer deletes
          // copy/move, so these cannot live in a std::array of value types;
          // see this class's own header comment on address stability).
          randomShLaneVisualizer1_(randomShLaneUiStates_[0], LaneColor(0)),
          randomShLaneVisualizer2_(randomShLaneUiStates_[1], LaneColor(1)),
          randomShLaneVisualizer3_(randomShLaneUiStates_[2], LaneColor(2)),
          randomShLaneVisualizer4_(randomShLaneUiStates_[3], LaneColor(3)),
          randomShLaneVisualizer5_(randomShLaneUiStates_[4], LaneColor(4)) {}

    FroggersModulationSlate(const FroggersModulationSlate&) = delete;
    FroggersModulationSlate& operator=(const FroggersModulationSlate&) = delete;

    // Registers all 15 sources, in the fixed slot order above, one explicit
    // call per slot.
    // Provisioned depth-parameter storage: worst case is every top-level
    // parameter carrying a depth for every modulation source --
    // FroggersParameterModel::kMaxParameters (96) *
    // FroggersParameterModel::kNumModulators (15) = 1440. Expressed as that
    // product (rather than a bare literal) so it cannot drift out of sync
    // with either ceiling again.
    static constexpr std::size_t kDepthParameterStorageCapacity =
        FroggersParameterModel::kMaxParameters * FroggersParameterModel::kNumModulators;

    // `extraDepthCapacity` defaults to the full provisioning need
    // (kDepthParameterStorageCapacity); tests pass a small override to
    // deterministically exercise the CanAllocate()-exhaustion / partial-
    // randomize detection without needing to actually drive 900+
    // real allocations.
    void Init(synth::ParameterGroup& group, std::size_t extraDepthCapacity = kDepthParameterStorageCapacity) {
        group_ = &group;
        RegisterSources();

        // This materialization ceiling (915 L1 depths, plus one focused
        // parameter's 225 L2 depths) exceeds the kMaxParameters=64 initial
        // batch (deliberately sized for only the 91 top-level parameters --
        // modulation-depth parameters are NOT sized for at that point, since
        // they ride ParameterGroup's own storage-batch request mechanism
        // instead). Provisions the extra capacity directly here (rather than
        // relying on the async ParameterStorageBatchNeeded /
        // ParameterMessageOutBus / Engine::MessageThreadTick path, which
        // exists for hosts that pump a message thread -- this app's tests
        // construct a bare ParameterManager with no such pump running).
        group_->AddParameterStorageBatch(synth::MakeParameterStorageBatch(
            group_->Config(), group_->GestureCount(), extraDepthCapacity));
    }

    // Sample-rate-dependent setup (VCO pitch mapping needs the real rate at
    // Process()-call time, not here; EF coefficients and the GangedRandomLfo
    // lookup table are the two pieces that must be prepared once up front).
    // This method's only production caller
    // is FroggersAppCore::PrepareToPlay() (`modulation_.Prepare(sampleRate)`,
    // its very first statement), which validates the host's sample rate
    // ONCE before any downstream use, including this call. So `sampleRate`
    // here is always already positive: a redundant `48000.0` fallback here
    // would be a THIRD fallback value, disagreeing with both the six
    // 44100.0 sites and Limiter's 1.0f, so this function carries none.
    void Prepare(double sampleRate) {
        sampleRate_ = sampleRate;
        vcoEnvelopeFollowers_.SetSampleRate(static_cast<float>(sampleRate_));
        externalAudioEf_.SetSampleRate(static_cast<float>(sampleRate_));
        gangedRandomLfo6_.Prepare(sampleRate_);
    }

    struct VcoDrive {
        float pitch01 = 0.5f;
        float shape01 = 0.5f;
        float phaseMod01 = 0.0f;
    };

    // Recomputes source #6's tempo-following
    // GangedRandomLfoInput. Called ONCE PER BLOCK from FroggersApp::ProcessBlock, BEFORE
    // the per-sample loop that calls Step() -- `quarterNotesPerSample` is
    // `block.clockPlan->QuarterNotesPerSample()` when a clock plan exists,
    // nullopt otherwise (a plan-independent rate, so this does not need the
    // per-sample transport-position optional Step() takes).
    void PrepareBlockClock(std::optional<double> quarterNotesPerSample) {
        // Fallback: MasterClock::kDefaultTempoBpm (120 BPM)'s quarter-note
        // duration, so the config below stays valid (Process() below throws
        // on invalid input, DspRandomLfo.hpp:26-30) even with no clock plan
        // at all (transport never started, or a rejected/zero-frame
        // callback) -- input to `GangedRandomLfoInput` must stay finite and
        // positive before it reaches the audio thread.
        constexpr double kFallbackQuarterNoteSeconds = 0.5;
        double quarterNoteSeconds = kFallbackQuarterNoteSeconds;
        if (quarterNotesPerSample.has_value() && std::isfinite(*quarterNotesPerSample) &&
            *quarterNotesPerSample > 0.0 && sampleRate_ > 0.0) {
            quarterNoteSeconds = 1.0 / (*quarterNotesPerSample * sampleRate_);
        }

        // One move spans 16 quarter notes (four bars).
        // Split between the "waiting" (holding still) and "moving"
        // (transitioning) phases at the SAME 2:1 ratio (and the same
        // sigma-to-mu fraction) the original fixed placeholder used
        // (waiting mu=3.0/sigma=0.5, moving mu=1.5/sigma=0.3) -- an
        // implementer choice for how the 16-QN total round splits, flagged
        // for by-ear tuning like this file's other constants,
        // not derived from any cited source.
        constexpr double kQuarterNotesPerMove = 16.0;
        const double totalRoundSeconds = kQuarterNotesPerMove * quarterNoteSeconds;
        const double waitingMuSeconds = totalRoundSeconds * (2.0 / 3.0);
        const double movingMuSeconds = totalRoundSeconds * (1.0 / 3.0);

        currentGangedLfoInput_ = synth::GangedRandomLfoInput{
            .waiting = {.muSeconds = waitingMuSeconds,
                        .sigmaSeconds = waitingMuSeconds * (0.5 / 3.0),
                        .internalSigmaHz = 0.1},
            .moving = {.muSeconds = movingMuSeconds,
                       .sigmaSeconds = movingMuSeconds * (0.3 / 1.5),
                       .internalSigmaHz = 0.15},
            .targetInternalSigma = 0.15f,
        };
    }

    // Called once per sample, BEFORE FroggersParameterModel::ProcessSample
    // (whose group_->UpdateModValues() call reads these sources' current
    // values through the pointers registered below) -- see FroggersApp's
    // ProcessBlock for the call order.
    //
    // `vco1`/`vco2`/`vco3` are each VCO's OWN Audio-bank pitch/shape/PM
    // knobs (post-fuego cached values from the PREVIOUS sample, since this
    // Step() runs before the current sample's fuego/modulation resolve --
    // see the class-level comment on the one-sample latency this implies
    // for any parameter a VCO-audio source itself modulates, e.g. the Audio
    // bank's cross-VCO pitch detents, kAudioPitchDetents further down this
    // file). Zero cross-VCO terms: each
    // Vco::Process call only ever reads its OWN three knobs.
    // `externalAudioSample` is this same sample's already-resolved, single-
    // channel input signal (bipolar, un-normalized) -- FroggersAppCore::
    // ProcessBlock reads it from the audio block's own input view and passes
    // it straight through with no cross-sample buffering here. Defaulted to
    // 0.0f so every caller that has no external-audio path to wire up (this
    // file's own tests, mostly) does not need to pass one -- the default
    // reproduces the exact silence this pair always saw before either was
    // ever driven.
    void Step(const VcoDrive& vco1, const VcoDrive& vco2, const VcoDrive& vco3,
              std::optional<double> transportQuarterNotes, float externalAudioSample = 0.0f) {
        // Tick each of the five X-style lanes at
        // its own rate against the SAME already-computed transport
        // quarter-note position FroggersApp::ProcessBlock's own
        // TransportQuarterNotesAt() helper produced for the ASR gate --
        // reusing that one value here, rather than re-deriving the
        // null-check/guard/Try-call sequence a second time. `Phasor2Tick::Input` rejects
        // `multiplier <= 0` (DspPhasor2Tick.hpp:17-22), so rate
        // MULTIPLICATIONS use `multiplier` (source 1 = 3, source 2 = 2,
        // source 3 = 1) and the two rate DIVISIONS (source 4 once per two
        // quarter notes, source 5 once per four) pre-scale `time` with
        // multiplier = 1.
        StepClockDrivenLanes(transportQuarterNotes);
        for (std::size_t i = 0; i < kFroggersNumRandomShLanes; ++i) {
            randomShLaneOutputs_[i] = randomShLanes_[i].Process();
        }

        // Random S&H 6 (Y-style, GangedRandomLfoProcessor<1>
        // mechanism resolution). `currentGangedLfoInput_` is recomputed once
        // per block by PrepareBlockClock() so this source is
        // tempo-proportional rather than tick-driven -- NOT
        // phase-locked to the quarter-note grid.
        gangedRandomLfo6_.Process(currentGangedLfoInput_);
        // The glide's output goes through the lanes' distribution shape at
        // kSource6Spread: Sheaf draws the targets uniformly and its input
        // carries no spread, so shaping the output bends the whole path
        // toward the centre slightly rather than only its endpoints (an
        // approximation, stated), and the source-6 visualizer draws the
        // unshaped path.
        randomSh6Output_ = dsp::ShapeSpread(gangedRandomLfo6_.Output(0), kSource6Spread);

        // VCO audio (slots 6-8): each VCO stepped from only its own knobs.
        // This modulation-preview slate has no
        // Ring Mod or PM-rate knob of its own (VcoDrive only carries
        // pitch01/shape01/phaseMod01), so phaseMod01 is passed again as the
        // rate argument -- reproducing the coupled rate-from-
        // depth behaviour this call always had -- and Ring Mod is held at
        // 0.0f (its own zero floor), so this source's output is unchanged.
        const float sr = static_cast<float>(sampleRate_);
        const float vco1Raw = vco1_.Process(vco1.pitch01, vco1.shape01, vco1.phaseMod01, vco1.phaseMod01, 0.0f, sr);
        const float vco2Raw = vco2_.Process(vco2.pitch01, vco2.shape01, vco2.phaseMod01, vco2.phaseMod01, 0.0f, sr);
        const float vco3Raw = vco3_.Process(vco3.pitch01, vco3.shape01, vco3.phaseMod01, vco3.phaseMod01, 0.0f, sr);
        vco1AudioSource_ = NormalizeBipolarToUnit(vco1Raw);
        vco2AudioSource_ = NormalizeBipolarToUnit(vco2Raw);
        vco3AudioSource_ = NormalizeBipolarToUnit(vco3Raw);

        // VCO envelope followers (slots 9-11); already [0,1].
        float efOut[dsp::VcoEnvelopeFollowers::kNumTaps];
        vcoEnvelopeFollowers_.Process(vco1Raw, vco2Raw, vco3Raw, efOut);
        vco1EfSource_ = efOut[0];
        vco2EfSource_ = efOut[1];
        vco3EfSource_ = efOut[2];

        // Noise (slot 12); already [0,1] (DspNoise.hpp).
        noiseProcessor_.Process();

        // External audio (slots 13-14). `.connected` is never written here:
        // SetExternalAudioConnected() (below) is the only writer, called
        // once at startup and once per routing transition by
        // FroggersAppCore -- never per sample, and never from this method.
        // While connected, `externalAudioSample` (this sample's already-
        // resolved, single-channel, bipolar input) drives both cells for
        // real: renormalized through the same NormalizeBipolarToUnit used
        // for VCO audio, and fed into externalAudioEf_ the same way
        // vcoEnvelopeFollowers_ is fed raw VCO output above. While
        // disconnected, both cells are pinned to the exact pair every
        // caller of this class already treats as their fixed inert value --
        // 0.5f == NormalizeBipolarToUnit(0.0f), 0.0f == a resting envelope
        // follower's own floor -- restored on every disconnected sample
        // rather than left holding whatever the last connected sample
        // produced, so a route that just went inert reads inert immediately,
        // not on some future edge. Either way the pair stays defined and
        // finite for Modulators::UpdateModValues() to dereference regardless
        // of connectedness.
        if (ExternalAudioConnected()) {
            externalAudioSource_ = NormalizeBipolarToUnit(externalAudioSample);
            externalAudioEfSource_ = externalAudioEf_.Process(externalAudioSample);
        } else {
            externalAudioSource_ = 0.5f;
            externalAudioEfSource_ = 0.0f;
        }
    }

    // The cell stays pushed with a null parameter whenever this is false
    // (Sheaf's own OpenModulationView/EnsureModulationDepthParameter
    // behavior, ParameterModulation.cpp:2804-2806,2843-2852), so the slate
    // never changes size or order regardless of cabling. Called once at
    // startup and once per routing transition (FroggersAppCore.hpp's
    // Init()/ProcessFrame()) -- never per sample; a test wanting
    // connected=true calls it directly instead of routing through Step().
    void SetExternalAudioConnected(bool connected) {
        group_->GetModulators().Metadata(kModSlotExternalAudio).connected = connected;
        group_->GetModulators().Metadata(kModSlotExternalAudioEf).connected = connected;
    }

    bool ExternalAudioConnected() const {
        return group_->GetModulators().Metadata(kModSlotExternalAudio).connected;
    }

    // Publishes this block's visualizer-facing state -- the five
    // X-style lanes' UiState (their state matches the
    // bag) and source #6's GangedRandomLfo UiState (its own PublishUiState,
    // the same call apps/braid-4 and apps/miniapp make once per block for
    // their own StandardModulators instances). Called once per block from
    // FroggersApp::ProcessBlock, AFTER the per-sample loop (mirroring
    // Braid4Core::ProcessBlock's own end-of-block
    // scopeWriter_.Publish()/PopulateUIState()/PublishUiState() sequence).
    void PublishUiState() {
        for (std::size_t i = 0; i < kFroggersNumRandomShLanes; ++i) {
            randomShLanes_[i].PopulateUiState(randomShLaneUiStates_[i]);
        }
        gangedRandomLfo6_.PublishUiState();
    }

    // Test/inspection accessors (also usable by UI wiring).
    synth::ParameterGroup& Group() { return *group_; }
    float SourceValue(std::size_t modIx) const { return group_->GetModulators().Value(0, modIx); }
    const synth::ModulatorMetadata& Metadata(std::size_t modIx) const {
        return group_->GetModulators().Metadata(modIx);
    }
    // Reads externalAudioSource_/externalAudioEfSource_ directly, bypassing
    // Modulators::Value()'s own cache (SourceValue(), above) -- that cache
    // is only refreshed for CONNECTED sources (Modulators::UpdateModValues(),
    // ParameterModulation.cpp: `if (!metadata_[modIx].connected) continue;`),
    // so it is not a reliable way to observe these two members' own
    // disconnected-state restore in Step(): nothing downstream ever reads
    // the stale cache either, since a disconnected source's
    // ModulationDepthParameter() is always nullptr (RegisterSources()'s own
    // comment) -- but a test asserting the restore itself needs the members'
    // real current value, not last connected snapshot.
    float ExternalAudioSourceForTest() const { return externalAudioSource_; }
    float ExternalAudioEfSourceForTest() const { return externalAudioEfSource_; }
    // Reflects randomShLanes_[laneIx]'s current index/bag as of the
    // last PublishUiState() call -- lets a test observe lane advance without
    // reaching into this class's private DSP state.
    const dsp::RandomShLane::UiState& RandomShLaneUiState(std::size_t laneIx) const {
        return randomShLaneUiStates_.at(laneIx);
    }
    // Exposes PrepareBlockClock()'s own recomputed
    // config directly, so a test can check that source #6 is
    // tempo-proportional rather than counted in ticks, via the
    // muSeconds/tempo relationship
    // by formula rather than statistically waiting for real LFO rounds to
    // complete (which involves random normal draws and would be slow/flaky).
    const synth::GangedRandomLfoInput& CurrentGangedLfoInputForTest() const { return currentGangedLfoInput_; }
    // The five lanes' and source 6's current outputs, as registered with the
    // modulator group, so a test can measure them per sample.
    float RandomShLaneOutputForTest(std::size_t laneIx) const { return randomShLaneOutputs_[laneIx]; }
    float RandomSh6OutputForTest() const { return randomSh6Output_; }

    // Redraws every lane's bag from a new salt: the same mixing the
    // constructor uses, so a salt names one set of five bags whether it
    // arrives at launch or from Randomize All. The lanes keep their read
    // index and slew state, so the change glides in rather than clicking.
    void ReseedRandomShLanes(std::uint32_t salt) {
        for (std::size_t i = 0; i < kFroggersNumRandomShLanes; ++i) {
            randomShLanes_[i].Reseed(LaneSeed(i, salt));
        }
    }
    std::uint32_t RandomShLaneTickCountForTest(std::size_t laneIx) const { return randomShLanes_[laneIx].TickCount(); }

private:
    // One shared per-sample tick call, so a test suite can
    // exercise this without wiring up a real MasterClock/AudioBlock (a bare
    // std::optional<double> is exactly what FroggersApp::ProcessBlock's own
    // TransportQuarterNotesAt() helper already returns).
    void StepClockDrivenLanes(std::optional<double> transportQuarterNotes) {
        auto tickLane = [&](synth::Phasor2Tick& ticker, dsp::RandomShLane& lane, double time, int multiplier) {
            if (!transportQuarterNotes.has_value()) {
                return;  // transport not running / no clock plan: no tick, gate-closed-equivalent.
            }
            const synth::Phasor2Tick::Input input{time, multiplier};
            if (ticker.Process(input)) {
                lane.Increment();
            }
        };

        const double quarterNotes = transportQuarterNotes.value_or(0.0);
        // The rate table: #1 eighth triplet (x3), #2 eighth (x2), #3
        // quarter note (x1), #4 once per two quarter notes, #5 once per four
        // (the DIVISIONS pre-scale `time` rather than using `multiplier`,
        // per Phasor2Tick's own `multiplier <= 0` rejection,
        // DspPhasor2Tick.hpp:17-22). The period is the one axis of each
        // source's character set here; the rest are in RandomShLane.hpp's
        // factories, and the ordering is the same one: source 1 the
        // fastest, source 5 the slowest.
        tickLane(tick1_, randomShLanes_[0], quarterNotes, 3);
        tickLane(tick2_, randomShLanes_[1], quarterNotes, 2);
        tickLane(tick3_, randomShLanes_[2], quarterNotes, 1);
        tickLane(tick4_, randomShLanes_[3], quarterNotes / 2.0, 1);
        tickLane(tick5_, randomShLanes_[4], quarterNotes / 4.0, 1);
    }

    // Per-lane visualizer colors, shared between RegisterSources()
    // (ModulatorMetadata::sourceColor) and the constructor's visualizer
    // initializer list -- kept as one formula so the two never drift apart.
    static synth::Color LaneColor(std::size_t i) {
        return synth::Color::FromHsvDegrees(180.0f + static_cast<float>(i) * 30.0f, 0.65f, 0.95f);
    }

    void RegisterSources() {
        // Source colors: no shared "modulation source palette" file
        // exists for this 15-source set, so these are an implementer
        // default: one hue family
        // per source category, spaced for visual distinction, not verified
        // against a specific
        // palette table.
        // Each X-style lane's OWN Visualizer,
        // constructed once alongside this instance (see the constructor's
        // own comment).
        std::array<synth::ui::Visualizer*, kFroggersNumRandomShLanes> randomShLaneVisualizers{
            &randomShLaneVisualizer1_, &randomShLaneVisualizer2_, &randomShLaneVisualizer3_,
            &randomShLaneVisualizer4_, &randomShLaneVisualizer5_,
        };
        for (std::size_t i = 0; i < kFroggersNumRandomShLanes; ++i) {
            const std::array<float*, 1> src{&randomShLaneOutputs_[i]};
            group_->SetModulationSource(i, src,
                {kRandomShNames[i], kRandomShShortNames[i], LaneColor(i), randomShLaneVisualizers[i],
                 /*connected=*/true});
        }
        {
            // Mirrors Sheaf's own StandardModulators::Init, which
            // calls SetVoiceColor on every GangedRandomLfoProcessor it owns
            // before registering it (StandardModulators.hpp:126-130).
            // gangedRandomLfo6_ is this app's own standalone instance (not
            // one of StandardModulators' processors -- source #6 is resolved
            // outside that shared machinery), so nothing else
            // ever calls this -- without it, its one voice's UiState color
            // (what GangedRandomLfoVisualizer actually draws,
            // GangedRandomLfoVisualizer.hpp's `voice.color`) stays at
            // GangedRandomLfoAtomicColor's own default, Color::Grey
            // (DspRandomLfo.hpp:165), even though the ModulatorMetadata
            // below already carries the correct LaneColor(5).
            gangedRandomLfo6_.SetVoiceColor(0, LaneColor(5));
            const std::array<float*, 1> src{&randomSh6Output_};
            group_->SetModulationSource(kModSlotRandomSh6, src,
                {"Random S&H 6", "RndSH6", LaneColor(5), &source6Visualizer_, /*connected=*/true});
        }

        {
            const std::array<float*, 1> src{&vco1AudioSource_};
            group_->SetModulationSource(kModSlotVco1Audio, src,
                {"VCO1 Audio", "V1Aud", synth::Color::Red, nullptr, true});
        }
        {
            const std::array<float*, 1> src{&vco2AudioSource_};
            group_->SetModulationSource(kModSlotVco2Audio, src,
                {"VCO2 Audio", "V2Aud", synth::Color::Orange, nullptr, true});
        }
        {
            const std::array<float*, 1> src{&vco3AudioSource_};
            group_->SetModulationSource(kModSlotVco3Audio, src,
                {"VCO3 Audio", "V3Aud", synth::Color::Yellow, nullptr, true});
        }

        {
            const std::array<float*, 1> src{&vco1EfSource_};
            group_->SetModulationSource(kModSlotVco1Ef, src,
                {"VCO1 EF", "V1EF", synth::Color::Red.AdjustBrightness(0.55f), nullptr, true});
        }
        {
            const std::array<float*, 1> src{&vco2EfSource_};
            group_->SetModulationSource(kModSlotVco2Ef, src,
                {"VCO2 EF", "V2EF", synth::Color::Orange.AdjustBrightness(0.55f), nullptr, true});
        }
        {
            const std::array<float*, 1> src{&vco3EfSource_};
            group_->SetModulationSource(kModSlotVco3Ef, src,
                {"VCO3 EF", "V3EF", synth::Color::Yellow.AdjustBrightness(0.55f), nullptr, true});
        }

        // Reuses NoiseModulatorProcessor's own SourcePointers() span
        // directly -- it is already sized to its voiceCount (1), matching
        // this mono group's numVoices, so no manual pointer wiring is needed.
        group_->SetModulationSource(kModSlotNoise, noiseProcessor_.SourcePointers(),
            {"Noise", "Noise", synth::Color::White, &noiseVisualizer_, true});

        // External audio pair registered `connected = false` initially --
        // an unconditional starting point, immediately overwritten once
        // FroggersAppCore::Init() reads the host's actual routed-input state
        // (see that method's own comment) and on every later transition
        // (ProcessFrame()). Never true from this call alone.
        {
            const std::array<float*, 1> src{&externalAudioSource_};
            group_->SetModulationSource(kModSlotExternalAudio, src,
                {"External Audio", "ExtAud", synth::Color::Green, nullptr, false});
        }
        {
            const std::array<float*, 1> src{&externalAudioEfSource_};
            group_->SetModulationSource(kModSlotExternalAudioEf, src,
                {"External Audio EF", "ExtEF", synth::Color::Green.AdjustBrightness(0.55f), nullptr, false});
        }
    }

    static constexpr std::array<const char*, kFroggersNumRandomShLanes> kRandomShNames{
        "Random S&H 1", "Random S&H 2", "Random S&H 3", "Random S&H 4", "Random S&H 5",
    };
    static constexpr std::array<const char*, kFroggersNumRandomShLanes> kRandomShShortNames{
        "RndSH1", "RndSH2", "RndSH3", "RndSH4", "RndSH5",
    };
    // Source 6 hugs the centre: the lanes' distribution shape applied to
    // the glide's output (see Step()).
    static constexpr float kSource6Spread = 0.25f;
    // Distinct per-lane seeds: each RGen is per-instance, so
    // distinct seeds are what actually gives five independent streams. A
    // salt (the launch draw, or Randomize All's) is mixed into each by
    // LaneSeed(), the one place a salt becomes five seeds.
    static constexpr std::array<uint32_t, kFroggersNumRandomShLanes> kRandomShSeeds{
        0x1a2b3c4du, 0x2b3c4d5eu, 0x3c4d5e6fu, 0x4d5e6f70u, 0x5e6f7081u,
    };
    static constexpr std::uint32_t LaneSeed(std::size_t laneIx, std::uint32_t salt) {
        return kRandomShSeeds[laneIx] ^ salt;
    }

    synth::ParameterGroup* group_ = nullptr;
    double sampleRate_ = 48000.0;

    dsp::Vco vco1_;
    dsp::Vco vco2_;
    dsp::Vco vco3_;
    dsp::VcoEnvelopeFollowers vcoEnvelopeFollowers_;
    // Follows the external-audio input while connected (Step(), above);
    // SetSampleRate() in Prepare() below gives it the same sample-rate-
    // correct attack/release coefficients vcoEnvelopeFollowers_ gets (the
    // struct's own raw defaults are only valid near ~2kHz, not this app's
    // 48kHz).
    dsp::SingleEnvelopeFollower externalAudioEf_;

    std::array<dsp::RandomShLane, kFroggersNumRandomShLanes> randomShLanes_;
    // One Phasor2Tick per X-style lane (the rate table above);
    // NOT one per bank/parameter -- each lane owns exactly its own ticker,
    // matching the "each source carries a fixed character" framing.
    synth::Phasor2Tick tick1_;
    synth::Phasor2Tick tick2_;
    synth::Phasor2Tick tick3_;
    synth::Phasor2Tick tick4_;
    synth::Phasor2Tick tick5_;
    // Visualizer-facing published state for the five lanes above
    // (declared before the Visualizer members below, which each hold a
    // pointer into one element -- see this class's own "constructed once,
    // never moved" address-stability convention).
    std::array<dsp::RandomShLane::UiState, kFroggersNumRandomShLanes> randomShLaneUiStates_;

    synth::GangedRandomLfoProcessor<1> gangedRandomLfo6_;
    // Source #6's tempo-following input, recomputed once per block
    // by PrepareBlockClock(). Default-initialized to a plausible fixed
    // shape, so a Step() call before the first
    // PrepareBlockClock() (should not happen in practice -- FroggersApp
    // calls PrepareBlockClock() before its per-sample loop every block --
    // but is a defensive, always-valid starting point, not a live musical
    // rate choice) still never throws.
    synth::GangedRandomLfoInput currentGangedLfoInput_{
        .waiting = {.muSeconds = 3.0, .sigmaSeconds = 0.5, .internalSigmaHz = 0.1},
        .moving = {.muSeconds = 1.5, .sigmaSeconds = 0.3, .internalSigmaHz = 0.15},
        .targetInternalSigma = 0.15f,
    };
    synth::NoiseModulatorProcessor noiseProcessor_;
    synth::ui::NoiseWaveformVisualizer noiseVisualizer_;
    synth::ui::GangedRandomLfoVisualizer<1> source6Visualizer_;
    // The five X-style lanes' own Visualizer --
    // individually-named members, not a std::array, because
    // synth::ui::Visualizer deletes copy/move (see the constructor's own
    // comment).
    RandomShLaneVisualizer randomShLaneVisualizer1_;
    RandomShLaneVisualizer randomShLaneVisualizer2_;
    RandomShLaneVisualizer randomShLaneVisualizer3_;
    RandomShLaneVisualizer randomShLaneVisualizer4_;
    RandomShLaneVisualizer randomShLaneVisualizer5_;

    float randomShLaneOutputs_[kFroggersNumRandomShLanes]{0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float randomSh6Output_ = 0.5f;
    float vco1AudioSource_ = 0.5f;
    float vco2AudioSource_ = 0.5f;
    float vco3AudioSource_ = 0.5f;
    float vco1EfSource_ = 0.0f;
    float vco2EfSource_ = 0.0f;
    float vco3EfSource_ = 0.0f;
    // NSDMI defaults double as the disconnected-state values Step() (above)
    // restores every sample while ExternalAudioConnected() is false, and as
    // the starting values before Prepare()/the first Step() call ever runs.
    float externalAudioSource_ = 0.5f;
    float externalAudioEfSource_ = 0.0f;
};

// ============================================================================
// Drill-in level cap
// ============================================================================
class FroggersModulationDrillIn {
public:
    // The ONE definition site of the drill-in maximum. PUBLIC because
    // `RandomizeAll` below reads it to decide whether a deeper level exists to
    // descend into -- it must not re-declare the number, and it is
    // the same fact this class already enforces in `PressEncoder`.
    static constexpr std::size_t kMaxDrillLevel = 3;

    explicit FroggersModulationDrillIn(synth::Bank& bank) : bank_(&bank) {}

    std::size_t Level() const { return level_; }
    synth::Bank& BankRef() { return *bank_; }

    // The app-side level counter. Levels below the cap are Sheaf's
    // native behavior (no app code); the ONLY thing added is refusing to
    // dispatch a press that would open one level deeper than the cap.
    void PressEncoder(synth::PhysicalEncoderId encoderId) {
        if (level_ >= kMaxDrillLevel) {
            // Level cap: only forward a press on the Target/Back cell
            // (the current selection's own cell) -- anything else would
            // call OpenModulationView for THIS level's depth cell's own
            // depths, i.e. one level past the cap, so it is refused: no
            // dispatch at all.
            if (bank_->SelectedParameter() == nullptr ||
                bank_->VisibleParameter(encoderId) != bank_->SelectedParameter()) {
                return;
            }
        }
        synth::Parameter* const selectedBefore = bank_->SelectedParameter();
        // Guards against the drilldown Back button going all the way back
        // instead of one level: capture, BEFORE dispatching the press, whether `encoderId` is the
        // Target/Back cell -- i.e. whether the pressed cell's own visible
        // parameter is the parameter already selected. This MUST be read
        // now, not after HandlePress() returns: a Target/Back press calls
        // Deselect() (below), which resets `visible_` to `topLevel_`, so
        // VisibleParameter(encoderId) would no longer reflect the
        // pre-press view afterward.
        //
        // This is the exact, and ONLY, distinguishing signal, verified by
        // reading Sheaf's complete Bank::HandlePress body (External/Sheaf/
        // projects/synth/src/ParameterModulation.cpp:2628-2650, pinned):
        // its one and only branch that sets `selected_` to nullptr is
        // `if (ShowingModulation() && cell->parameter == selected_) {
        // Deselect(); return; }` (:2643-2646) -- OpenModulationView
        // (:2813-2859) never nulls it, it either assigns a real parameter
        // or returns early on a storage shortfall leaving `selected_`
        // unchanged. `ShowingModulation()` is `selected_ != nullptr`
        // (:2710-2712), i.e. `selectedBefore != nullptr` here, and
        // `cell->parameter` for the pressed encoderId is exactly what
        // `bank_->VisibleParameter(encoderId)` reads (:2718-2721, same
        // FindVisibleCell lookup). So, given `selectedBefore != nullptr`,
        // HandlePress leaves `selectedAfter == nullptr` if and only if
        // `wasTargetBackPress` below is true -- an exact predicate, not a
        // heuristic. This also matches OpenModulationView's own
        // construction of the Target/Back cell (:2854-2857, the selected
        // parameter's own cell is always physicalLayout.back()) at every
        // drill level alike, since FullPhysicalLayout(*bank_) is the same
        // fixed 16-wide span on every call (this file's own comment above).
        const bool wasTargetBackPress =
            selectedBefore != nullptr && bank_->VisibleParameter(encoderId) == selectedBefore;
        bank_->HandlePress(encoderId, FullPhysicalLayout(*bank_));
        synth::Parameter* const selectedAfter = bank_->SelectedParameter();
        if (selectedAfter == nullptr) {
            if (wasTargetBackPress) {
                // The operator's actual Back gesture (every on-screen press,
                // Target/Back cell included, dispatches through here, never
                // through Back() directly -- see this class's own header
                // comment). Pop exactly ONE level by calling the existing
                // Back() mechanism -- reused as-is, not duplicated --
                // rather than the full `level_ = 0` reset below.
                // Back() reads the CURRENT `level_` to compute its target
                // depth, so this must run before `level_` is touched here;
                // it leaves `level_` at that one-shallower depth itself.
                Back();
            } else {
                // Genuine full clear: `selectedBefore` was already nullptr
                // (level 0, e.g. an empty-cell press), so `wasTargetBackPress`
                // is false and this is a same-value no-op. No path reaches this branch with
                // `wasTargetBackPress` false and `selectedBefore` non-null:
                // that would require HandlePress to null out `selected_`
                // some OTHER way, and the trace above establishes there is
                // no other way.
                level_ = 0;
            }
        } else if (selectedAfter != selectedBefore) {
            // One remembered encoder id per level (not just one for level 1):
            // remember the encoder id that opened THIS descent, so Back() can
            // re-open the same path one level at a time (Sheaf's Bank has no
            // one-level pop of its own -- Deselect() is always a full exit --
            // so the app synthesizes one by Deselect()-then-replaying the
            // remembered presses; see Back() below). `levelEncoders_[i]` is
            // therefore the encoder id that opens level i+1, which is why
            // this is recorded at index `level_` BEFORE the increment below.
            levelEncoders_[level_] = encoderId;
            level_ += 1;
        }
        // else: selection identity unchanged -- either a held modifier was
        // applied to the pressed cell (Bank::HandlePress's modifier branch
        // returns before touching selected_/level state), or the press hit
        // an empty cell. Level stays exactly where it was either way.
    }

    // Back pops exactly ONE level, from any depth -- Sheaf's Bank::Deselect()
    // is a full, unconditional exit (no Sheaf change, no one-level pop
    // added there), so the one-level pop is synthesized app-side: Deselect()
    // all the way out, then replay the remembered presses (`levelEncoders_`,
    // set by PressEncoder above) that walk back down to exactly one level
    // short of where Back() was called, at any depth. `target` is captured BEFORE `level_` is reset
    // to 0 below, and uses the `(level_ == 0) ? 0 : level_ - 1` form because
    // `level_ - 1` on an unsigned zero would wrap instead of staying at 0.
    void Back() {
        const std::size_t target = (level_ == 0) ? 0 : level_ - 1;
        bank_->Deselect();
        level_ = 0;
        for (std::size_t i = 0; i < target; ++i) {
            PressEncoder(levelEncoders_[i]);
        }
    }

private:
    synth::Bank* bank_;
    std::size_t level_ = 0;
    // One remembered encoder id per
    // level of the current drill-in path -- `levelEncoders_[i]` is the
    // encoder id that opens level i+1. See PressEncoder's own comment for
    // when each entry is recorded, and Back() for how they are replayed.
    std::array<synth::PhysicalEncoderId, kMaxDrillLevel> levelEncoders_{};
};

// ============================================================================
// Randomize
// ============================================================================
// Depth randomize does not dispatch a held Modifier::RandomMod press into
// Bank::HandlePress, which calls the PRIVATE Bank::RandomizeModulationDepths
// (src/ParameterModulation.cpp:2901-2933). That function's own loop --
//     while (manager_->NextRandomCoin() < 0.5f) { ... one depth touched ... }
// (:2894) -- is a geometric distribution over the count of depths touched
// STARTING AT ZERO: P(k) = 0.5^(k+1) for k = 0, 1, 2, ..., mean 1.0. A single
// press typically changed only ~1 depth, and was a complete no-op exactly
// 50% of the time. That constant is Sheaf's, private, and out of scope to
// change upstream (src/ParameterModulation.cpp:2914).
//
// `detail::RandomizeParameterModulationDepths` below (used by all four call
// sites: RandomizeBankLevel1Depths, RandomizePage's drill-in branch, and
// both RandomizeAll drill-in branches) replaces this with an APP-SIDE count/
// source selection -- a geometric draw from a per-gesture floor (zero for
// Randomize All on a parameter page and for Randomize Page at any level; one
// for Randomize All at a drilled-in level -- see DrawGeometricCount's and
// that function's own comments) -- while Sheaf
// still performs every actual write (`Parameter::EnsureModulationDepth` +
// `Parameter::RandomizeVisibleValue`, the same two calls
// `Bank::RandomizeModulationDepths` itself makes internally). This is an
// "app chooses the target set, Sheaf does every write" split, not a
// violation of Sheaf's ownership of the actual writes -- see that function's
// own header comment for the full
// derivation. NEVER a no-op at a drilled-in Randomize All press (the floor of
// one guarantees at least 1 connected source is touched when one exists);
// every other gesture keeps its own zero floor, so a no-op stays possible and
// common there. Never re-draws the same source twice (Sheaf's own
// loop could; this one uses a partial Fisher-Yates over the connected set).
//
// The OTHER knob this app owns: Randomize All's aggregate
// reach also comes from how many PARAMETERS it presses (84 page parameters --
// six banks of fourteen -- for the parameter-page case) -- a future
// maintainer who wants Randomize All to feel like "more" or "fewer" changes
// has two independent levers (the per-gesture floor and cap the geometric
// draw above uses, or the parameter set
// RandomizeAll/RandomizeBankLevel1Depths iterates over).
struct FroggersRandomizeResult {
    // EnsureModulationDepthParameter's CanAllocate() failure must
    // surface as a detectable partial randomize, not fail silently. True
    // when `ParameterGroup::CanAllocate()` (public) was observed false
    // immediately before a press that could have needed to materialize a
    // new depth parameter during this operation.
    bool partial = false;
};

namespace detail {

// The only reliable externally-observable signal that
// Bank::EnsureModulationDepthParameter's private CanAllocate() early return
// (ParameterModulation.cpp:2811-2813) is ABOUT to fire for the next press:
// `ParameterGroup::CanAllocate()` (public) is already false. Checking a
// per-parameter "does every connected modulator have a materialized depth"
// count instead would be wrong -- the draw below is geometric from a
// per-gesture floor (zero everywhere except a drilled-in Randomize All
// press, which floors at one), so a HEALTHY randomize leaves a parameter with
// no depths at all half the time wherever the floor is zero, and most of its
// 15 possible depths untouched nearly always regardless of the floor; that is
// normal, not a partial randomize. Only "no more storage was available to
// give" is.
inline bool CapacityExhausted(const synth::ParameterGroup& group) {
    return !group.CanAllocate();
}

// Randomize All/Page is
// "agnostic to scene-slider position" -- every depth write below targets one
// of these two FIXED scene poles directly, never the live (possibly
// mid-blend) `manager.Scene()`. leftScene==rightScene on each, so
// `ApplySceneDistribution`'s own `&left==&right` special case
// (ParameterModulation.cpp:369-374) applies the write to exactly that one
// scene index regardless of blend -- the same pattern
// `ApplyFroggersDefaultPatch`'s own `kScene` constant already relies on for
// pole 0 (this file, further down). Matches `FroggersParameterModel::
// kNumScenes == 2` and the app's fixed `SetSceneEndpoints(0, 1)`
// (FroggersParameters.hpp; scene index 0 = "Scene 1", 1 = "Scene 2").
inline constexpr synth::SceneState kScenePole0{0, 0, 0.0f};
inline constexpr synth::SceneState kScenePole1{1, 1, 0.0f};

// The single definition site for "how many scene poles exist." Both
// ZeroExistingModulationDepths and RandomizeParameterModulationDepths below
// iterate this array, rather than hardcoding kScenePole0/kScenePole1 as two
// separate consecutive statements each, so a future change to
// `FroggersParameterModel::kNumScenes` trips the static_assert right below
// (a build break) instead of silently leaving both call sites still
// handling exactly two poles. Size is deduced from the initializer list
// (currently 2, matching kScenePole0/kScenePole1 above) and cross-checked
// against kNumScenes, the actual authority (FroggersParameters.hpp).
inline constexpr std::array kScenePoles{kScenePole0, kScenePole1};
static_assert(kScenePoles.size() == FroggersParameterModel::kNumScenes,
              "kScenePoles must enumerate exactly kNumScenes scene poles");

// The bipolar-neutral commanded value, named once. Sheaf's own
// `kNeutralModulationDepthCenter` (ParameterModulation.cpp:258) is
// file-private and not reachable from here, so this is a legitimate
// separate definition -- but it must exist exactly once on this side, not
// as a bare `0.5f` repeated at every call site: depths are
// `RangeKind::Bipolar`, so 0.5, not 0.0, is neutral. Referenced by
// ZeroExistingModulationDepths below and by FroggersModulationTests.cpp.
inline constexpr float kNeutralModulationDepthCenter = 0.5f;
// Matches Sheaf's own `kModulationNeutralTolerance`
// (External/Sheaf/projects/synth/src/ParameterModulation.cpp:256), which is
// the tolerance `Parameter::HasNonZeroState()` -- and therefore the badge
// criterion `ModulatorsAffectingMask()` -- uses to decide neutrality. Pinned
// to the same value deliberately: `DepthIsModulating` below has to agree with
// what the UI will actually draw, and that constant is file-local to Sheaf's
// .cpp so it cannot be referenced directly. If Sheaf's value ever moves, this
// one moves with it.
inline constexpr float kModulationNeutralEpsilon = 0.000001f;

// Non-additive randomize, scene-pair semantics: zeroes
// `parameter`'s EXISTING (already-materialized) modulation depths at BOTH
// scene poles, immediately before RandomizeParameterModulationDepths below
// draws a fresh set. Only touches depths that are already materialized --
// `ModulationDepthParameter` (unlike `EnsureModulationDepth`) never
// allocates, so an unmaterialized source (never touched, or already at
// storage capacity) is left alone rather than burning a slot on a write that
// would be a no-op anyway (an unmaterialized depth has no Parameter to carry
// a nonzero commanded value in the first place). `kNeutralModulationDepthCenter`
// is the bipolar neutral commanded value (see its own comment above).
// A raw `SceneCenter` write (not `RandomizeVisibleValue`/`HandleSetAbsolute`)
// is deliberate: it is the exact commanded-value write that
// `sceneCenters_` needs, with no dependency on the depth's own
// (currently stale) `targetCenter_`, and the single `ComputeAllParameters()`
// call the randomize operation makes at the end resyncs
// `currentCenter_`/`targetCenter_`/the UI display for every parameter this
// touches -- including these zeroed ones -- in one pass, so nothing here
// needs to re-converge anything itself.
// Is this depth parameter ACTUALLY modulating anything -- i.e. does it carry a
// non-neutral commanded value in either scene pole?
//
// Needed because "a depth parameter EXISTS" and "a source is modulating this
// parameter" are different facts, and drilling in conflates them: opening a
// modulation view eagerly materializes one depth per CONNECTED source (13 of
// 15 on the measured patch), regardless of whether randomize selected any of
// them. RandomizeAll below must descend only into the ones that are really
// modulating, for two reasons:
//
//  1. Semantics: a neutral depth modulates nothing, so sub-modulating IT is
//     meaningless work on a value that has no audible effect.
//  2. It is directly visible. Sheaf's badge criterion is
//     `Parameter::ModulatorsAffectingMask()` (External/Sheaf/projects/synth/
//     src/ParameterModulation.cpp:2356-2365), which sets a bit when the depth
//     is non-null AND `HasNonZeroState()`. `HasNonZeroState()` (:2367) returns
//     true if the depth's OWN `currentDepths_`/`targetDepths_` are non-zero --
//     so a depth counts as "affecting" merely because it has SUB-modulation,
//     even when its own value is dead neutral. Writing sub-depths to every
//     materialized depth therefore lit up all 13 as badges when only ~3
//     sources were modulating (measured: badge 13, non-neutral 1).
//     The badge was reporting CONNECTEDNESS, not modulation.
//
// `HasNonZeroState()` itself is private in Sheaf and unreachable from here, so
// this reads the commanded values the app itself writes -- the same
// `kNeutralModulationDepthCenter` / `kScenePoles` idiom
// ZeroExistingModulationDepths below uses, so the two agree by construction.
// Per-scene form: is this depth non-neutral in ONE specific scene? Separate
// from the any-scene form below because two distinct properties are asserted
// across this codebase's tests and they must not be conflated:
//   - "is this source modulating at all" (any pole) -- what the badge shows,
//     and what RandomizeAll's descent gates on;
//   - "is this source modulating in scene N" (this one) -- used to compare the
//     two poles against each other, to assert the badge/depth symmetry
//     property ("randomizing only scene 0 lights the badge in both scenes
//     while scene 1 reads zero"). Expressing THAT with the any-pole form would
//     make its own assertion trivially true and silently gut the test.
// Both share this one neutral-value/epsilon comparison, so the threshold that
// decides "neutral" has exactly one definition site.
inline bool DepthIsModulatingInScene(const synth::Parameter& depth, std::size_t sceneIx) {
    return std::fabs(depth.SceneCenter(sceneIx) - kNeutralModulationDepthCenter) >
           kModulationNeutralEpsilon;
}

inline bool DepthIsModulating(const synth::Parameter& depth) {
    for (const synth::SceneState& pole : kScenePoles) {
        if (DepthIsModulatingInScene(depth, pole.leftScene)) {
            return true;
        }
    }
    return false;
}

inline void ZeroExistingModulationDepths(synth::Parameter& parameter) {
    for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
        synth::Parameter* depth = parameter.ModulationDepthParameter(modIx);
        if (depth == nullptr) {
            continue;
        }
        for (const synth::SceneState& pole : kScenePoles) {
            depth->SceneCenter(pole.leftScene) = kNeutralModulationDepthCenter;
        }
    }
}

// Two independent geometric-coin draws share this one loop shape --
// RandomizeParameterModulationDepths' own source-count draw below and
// RandomizeAll's Crispy-bank-count draw further down this file -- but they
// differ on BOTH bounds: the source draw's floor is 0 for every gesture
// except a drilled-in Randomize All press (floor 1) and has no cap besides
// the connected-source count, while the Crispy draw floors at 0 always and
// caps at kMaxRandomizedCrispy. Neither caller's bound may be borrowed from
// the other's, so both are required arguments with no default.
inline std::size_t DrawGeometricCount(synth::ParameterManager& manager, std::size_t minimum,
                                       std::size_t maximum) {
    std::size_t count = minimum;
    while (count < maximum && manager.NextRandomCoin() >= 0.5f) {
        ++count;
    }
    return count;
}

// The partial-Fisher-Yates step RandomizeParameterModulationDepths' own
// source draw performs, and the Crispy-bank draw further down this file
// reuses unchanged: swaps a random not-yet-picked element of `pool` into
// slot `i` and returns it, so calling this for i = 0, 1, ..., count-1 draws
// `count` DISTINCT elements without replacement.
inline std::size_t SwapInRandomPick(synth::ParameterManager& manager, std::span<std::size_t> pool, std::size_t i) {
    const std::size_t remaining = pool.size() - i;
    const std::size_t pick = i + manager.NextRandomIndex(remaining);
    std::swap(pool[i], pool[pick]);
    return pool[i];
}

// The shared count/source-selection helper used by all
// four RandomMod dispatch sites in this file (RandomizeBankLevel1Depths,
// RandomizePage's drill-in branch, and both RandomizeAll drill-in branches).
// Chooses a COUNT of `parameter`'s connected modulation sources using the
// geometric draw below (see its own comment, right above it), draws that
// many DISTINCT sources
// (partial Fisher-Yates -- Sheaf's
// own private Bank::RandomizeModulationDepths loop can independently redraw
// the same source twice, which this does not reproduce), and
// for each chosen source calls the exact same two public calls Sheaf's own
// loop makes internally: `Parameter::EnsureModulationDepth` then
// `Parameter::RandomizeVisibleValue` (ParameterModulation.cpp:2926-2931).
// Sheaf performs every write; only the count and the source set are chosen
// here. Does NOT dispatch through Bank::HandlePress at
// all -- no press, no modifier-hold, no selection/level state touched -- so
// callers do not need `parameter` to be the bank's currently selected/open
// one.
//
// Returns true (a "partial" randomize, matching CapacityExhausted's meaning
// everywhere else in this file) when storage was already exhausted before
// this call, OR when `EnsureModulationDepth` returns null mid-loop (storage
// ran out while this call was materializing depths) -- the same "stop and
// report partial" convention `ApplyAudioPitchDetent`
// already uses for the identical null-return case.
inline bool RandomizeParameterModulationDepths(synth::ParameterManager& manager, synth::Parameter& parameter,
                                                std::size_t minimumSources) {
    synth::ParameterGroup& group = parameter.Group();
    bool partial = CapacityExhausted(group);

    // Non-additive (Option A): zero this parameter's own existing
    // depths, BOTH scene poles, before drawing the fresh set below, so each
    // randomize draws fresh depths rather than accumulating on top of the
    // previous draw.
    // Scope note: this function is the single call site every RandomizeAll/
    // RandomizePage branch below routes through per parameter, so "zero in
    // scope, then draw" naturally becomes "zero ALL depths this operation
    // touches" for Randomize All (RandomizeBankLevel1Depths calls this once
    // per top-level parameter, across every bank) and "zero only that page's
    // parameter's depths" for Randomize Page (RandomizePage's level-1/2
    // branch calls this exactly once, on the one selected parameter) -- no
    // separate wide "zero everything up front" pass is needed, since one
    // parameter's depths never affect another's.
    ZeroExistingModulationDepths(parameter);

    // Built once per call (not cached across calls: `connected` changes at
    // runtime, e.g. external-audio cabling) and reserved to the modulator
    // count -- O(15), trivial even at Randomize All's 84 calls.
    const std::span<const synth::ModulatorMetadata> metadata = group.GetModulators().Metadata();
    std::vector<std::size_t> eligible;
    eligible.reserve(metadata.size());
    for (std::size_t modIx = 0; modIx < metadata.size(); ++modIx) {
        if (metadata[modIx].connected) {
            eligible.push_back(modIx);
        }
    }
    if (eligible.empty()) {
        return partial;
    }

    // How many sources this parameter gets: a geometric draw from a
    // per-gesture floor (`minimumSources`, supplied by the caller -- see
    // DrawGeometricCount's own comment on why neither this bound nor the
    // Crispy-bank draw's may be borrowed from the other). From a floor of
    // zero it is 50% none, 25% one, 12.5% two, and so on; from a floor of one
    // it is 50% one, 25% two, 12.5% three, and so on. Keep flipping while the
    // coin says keep going; the count is how many times it said so, added to
    // the floor.
    //
    // This is Sheaf's own distribution rather than a table tuned on top of
    // it. A hand-tuned ladder used to sit here, and the arithmetic justifying
    // its six thresholds was longer than the code it explained. What the app
    // still needs from this function is the DISTINCT-source draw below, which
    // Sheaf's own loop does not give -- it can redraw one source twice, and a
    // count means nothing if the draw can collapse it. The count itself needed
    // no opinion of its own.
    //
    // Most parameters getting nothing is the point: at 84 parameters a
    // Randomize All materializes about 84 depths rather than the ~151 the old
    // mean of 1.80 produced, which is both sparser to listen to and half the
    // pressure on the storage a partial randomize reports running out of.
    const std::size_t count = DrawGeometricCount(manager, std::min(minimumSources, eligible.size()), eligible.size());

    // Partial Fisher-Yates over `eligible`: draws `count` DISTINCT source
    // indices (see this function's header comment on why "distinct" matters
    // here, unlike Sheaf's own loop). The materialize-and-break body stays
    // here unchanged; only the swap-and-pick step moves into the shared
    // helper, so the RNG call order is unaffected.
    for (std::size_t i = 0; i < count; ++i) {
        synth::Parameter* depth = parameter.EnsureModulationDepth(SwapInRandomPick(manager, eligible, i));
        if (depth == nullptr) {
            partial = true;
            break;  // storage exhausted mid-call -- partial, not a silent short-count.
        }
        // The SAME chosen source (`eligible[i]`)
        // gets an INDEPENDENT random value in EACH scene pole -- identical
        // source membership (so the badge, true if ANY scene is nonzero,
        // agrees with what every scene actually holds), different values per
        // pole (so blending sweeps between two distinct modulation states).
        // Deliberately NOT `manager.Scene()` (the live, possibly mid-blend
        // scene) -- see kScenePole0/kScenePole1's own comment. Iterates
        // kScenePoles rather than naming each pole in its own statement --
        // see that array's own comment.
        for (const synth::SceneState& pole : kScenePoles) {
            depth->RandomizeVisibleValue(pole, manager.NextRandomValue());
        }
    }
    return partial;
}

// A single-cell
// VALUE randomize of whatever parameter is currently VISIBLE at `encoderId`
// on `bank` -- `bank.VisibleParameter(encoderId)` is exactly
// `FindVisibleCell(encoderId)->parameter` (src/ParameterModulation.cpp:2729-
// 2732), the same lookup `Bank::HandlePress`'s modifier branch uses
// internally, so this targets the identical cell a press-based path
// would (top-level or drilled-in, whichever is visible), with no dependency
// on whether this Bank is the slot's currently *selected* one.
//
// Dispatching a press under a held `Modifier::Random`, which
// routes into Sheaf's `Parameter::RandomizeVisibleValue`
// (ParameterModulation.cpp:1723-1731), does not work here: that function deltas against
// `TargetValue(0)`, the MODULATION-RESOLVED value, so under live audio-rate
// modulation repeated presses ratchet the commanded value into the [0,1]
// clamp instead of landing the drawn value (measured 20/20 at
// exactly 1.0000 after 5 presses on a modulated Freeze). Sheaf is pinned and
// untouched, so the fix is here: draw ONE
// uniform value and commit it directly as the COMMANDED value via
// `HandleSetAbsolute` (`sceneCenters_`, no dependency on resolved/modulated
// state) to BOTH scene poles -- the "Randomize
// All/Page is agnostic to scene-slider position" convention (`kScenePoles`'s
// own comment above), the same idiom `RandomizeParameterModulationDepths`
// and `ApplyBankDefaultPatch` already use elsewhere in this file. One
// draw, not one per pole (unlike the depth path): "a draw lands the drawn
// value" is the whole fix, and a single shared draw is what makes that
// literally true regardless of scene-slider position.
// `manager.NextRandomValue()` is the same RNG source Sheaf's own
// `RandomizeVisibleValue` call site draws from (`Bank::ApplyModifierToParameter`,
// ParameterModulation.cpp:2893), so seeded/reproducible randomize is
// unaffected.
//
// `ParameterManager::SetRandomHeld` is deliberately no longer called here --
// traced, not assumed. Its only readers are: (1) `Bank::HandlePress`'s
// modifier branch, which this function no longer calls; (2)
// `ParameterManager::HandleSetAbsolute(slotIx, position, ...)`'s
// GetCurrentModifier()-gate, an overload this app never calls (app-side
// absolute/encoder input goes through `MessageIn::ParamIncDec` instead,
// FroggersUiSurface.hpp's `FroggersUiSurface::HandleAction`); (3) `SelectBankForSlot`/`NavigateBankForSlot`'s
// bank-wide-randomize branch, not invoked from inside this function; (4) the
// `randomHeld` mirror `PopulateUIState` publishes for UI rendering -- this
// app never reads `RandomHeld()` or that mirror anywhere (no "modifier held"
// indicator exists in this app), and never wires Sheaf's `MidiController`
// (which is the only other reader, for hardware LED feedback). With every
// reachable reader ruled out, holding it around this function's now-direct
// write would be a no-op; it is genuinely dead on this path, not merely
// unused by convention, and is removed rather than kept "just in case."
inline void PressBankWithRandomValue(synth::ParameterManager& manager, synth::Bank& bank,
                                     synth::PhysicalEncoderId encoderId) {
    synth::Parameter* parameter = bank.VisibleParameter(encoderId);
    if (parameter == nullptr) {
        return;
    }
    const float drawn = manager.NextRandomValue();
    for (const synth::SceneState& pole : kScenePoles) {
        parameter->HandleSetAbsolute(pole, drawn);
    }
}

// Randomize All draws 0 to this many of the six banks' Crispy per press --
// the operator's own bound, not derived. Randomizing all six banks' Crispy at
// once is what reproduces randomizing global Crunchy directly (which this app
// deliberately never randomizes), so the count Randomize All draws must stay
// bounded below six; two is the chosen bound. Read by RandomizeAll's level-0
// branch and by the test that pins this shape.
inline constexpr std::size_t kMaxRandomizedCrispy = 2;

// Randomizes one bank's 14 page-parameter values, and its Crispy (encoder 14)
// when `includeCrispy` says so -- NEVER Crunchy (encoder 15). Presses `bank`
// directly (see PressBankWithRandomValue); no level state is touched or
// required.
// `includeCrispy` exists because the two callers pick which banks' Crispy
// moves differently:
//
//   Randomize Page -> always TRUE.  One page's local Crispy is that page's
//                      own business.
//   Randomize All  -> TRUE for at most kMaxRandomizedCrispy of the six banks,
//                      drawn fresh per press (see that constant's own
//                      comment). Randomizing all six at once would land in
//                      the same place as randomizing global Crunchy, which
//                      this app deliberately never randomizes, so the count
//                      stays bounded below six.
inline void RandomizeBankValues(synth::ParameterManager& manager, synth::Bank& bank,
                                bool includeCrispy) {
    for (synth::PhysicalEncoderId e = 0; e < kFroggersParamsPerBank; ++e) {
        PressBankWithRandomValue(manager, bank, e);
    }
    if (includeCrispy) {
        PressBankWithRandomValue(manager, bank, kFroggersCrispySlot);
    }
}

// Randomizes one bank's 14 page parameters' LEVEL-1 depths -- and only those.
// Crispy (encoder 14) is deliberately EXCLUDED here, as is Crunchy (encoder
// 15), so unlike RandomizeBankValues above there is no `includeCrispy` knob:
// that function randomizes VALUES and its two callers differ on Crispy, while
// this one randomizes DEPTHS and its sole caller (RandomizeAll's level-0
// branch, which runs it once per bank) always wants Crispy out. The why is
// spelled out at this function's tail.
// Does not press through Bank::HandlePress at all --
// `bank.VisibleParameter(paramIx)` reads the top-level parameter directly
// (this bank is never drilled into here, so `visible_ == topLevel_`, per
// PressBankWithRandomValue's own header comment on Bank-owned state), and
// RandomizeParameterModulationDepths is called on it directly (it derives
// the parameter's group itself via `parameter.Group()`, which is every page
// parameter's SAME group -- FroggersParameterModel's one mono
// ParameterGroup -- so no separate group parameter is needed here anymore).
inline bool RandomizeBankLevel1Depths(synth::ParameterManager& manager, synth::Bank& bank) {
    bool partial = false;
    // Each call's result is hoisted into its own named local BEFORE
    // combining (the same short-circuit hazard FroggersAppCore::ProcessFrame()
    // guards against for its own Randomize All/Page dispatch), so every
    // iteration of this loop always runs its randomize call -- `partial =
    // RandomizeParameterModulationDepths(...) || partial` reads fine but is
    // only correct because the call sits on the left of `||`; swapping the
    // combine order (or a future edit that does) would short-circuit once
    // `partial` went true and skip randomizing every remaining page parameter.
    for (std::size_t paramIx = 0; paramIx < kFroggersParamsPerBank; ++paramIx) {
        synth::Parameter* param = bank.VisibleParameter(static_cast<synth::PhysicalEncoderId>(paramIx));
        if (param == nullptr) {
            continue;  // defensive: every page-parameter slot is always registered in practice.
        }
        // Floor 0: this runs once per top-level parameter across all six
        // banks from Randomize All's level-0 branch, and a floor here
        // roughly doubles the whole press's materialized-depth allocation
        // (see RandomizeParameterModulationDepths' own count comment) --
        // level 0 keeps the zero floor deliberately.
        const bool paramPartial = RandomizeParameterModulationDepths(manager, *param, /*minimumSources=*/0);
        partial = partial || paramPartial;
    }
    // Crispy's DEPTHS stay excluded here regardless of whether this press
    // drew this bank's Crispy VALUE (see kMaxRandomizedCrispy's own comment):
    // this function runs once per bank from Randomize All, and depths are
    // simply out of Randomize All's scope for Crispy, full stop. Randomize
    // Page reaches a single page's Crispy depths through the ordinary
    // drill-in path, which is unaffected.
    return partial;
}

// ----------------------------------------------------------------------------
// The default patch: ONE bank-addressable definition, consumed by both a
// fresh launch (ApplyFroggersDefaultPatch, further down this file) and Reset
// (ResetPage/ResetAll below). A parameter's default is its own registered
// ParameterConfig default (FroggersBankLayouts()'s defaultValue field --
// the same source FroggersParameterModel::Init() reads when first
// constructing it) unless one of the two per-bank overlays below replaces
// it; everything not named here is 0.0f, that field's own struct default.
// ----------------------------------------------------------------------------

// The six cross-VCO pitch-modulation detents the Audio bank's default patch
// gives its three pitch parameters (slots 0-2), sourced from the VCO
// audio-rate slate entries (indices 6-8):
//   VCO1 pitch (slot 0) <- +1 detent from VCO2 audio (7), VCO3 audio (8)
//   VCO2 pitch (slot 1) <- -1 detent from VCO1 audio (6), VCO3 audio (8)
//   VCO3 pitch (slot 2) <- +1 detent from VCO1 audio (6), VCO2 audio (7)
// One shared table rather than six separate call-site literals, so
// RestoreAudioPitchDetentsFor below can find "does this specific depth carry
// an override" without a second copy of the same six facts.
struct AudioPitchDetentSpec {
    std::size_t targetParamIx;
    std::size_t modIx;
    float sign;
};
inline constexpr std::array<AudioPitchDetentSpec, 6> kAudioPitchDetents{{
    {0, kModSlotVco2Audio, +1.0f}, {0, kModSlotVco3Audio, +1.0f},
    {1, kModSlotVco1Audio, -1.0f}, {1, kModSlotVco3Audio, -1.0f},
    {2, kModSlotVco1Audio, +1.0f}, {2, kModSlotVco2Audio, +1.0f},
}};

// One relative-encoder detent's worth of modulation depth. The surface's
// real encoder resolution is not established elsewhere in this codebase, so
// this is an explicit placeholder quantum (1/100, a common relative-encoder
// resolution) rather than a value derived from a cited source -- revisit if
// a real encoder resolution is established later.
inline constexpr float kPlaceholderEncoderDetent = 1.0f / 100.0f;

// Applies exactly one cross-VCO pitch detent: neutralizes the depth first
// (so this lands the same result whether the depth is being materialized
// for the first time -- a fresh launch -- or already carries a different
// value -- Reset), then applies the signed detent as a relative increment
// from that known base, via the same HandleIncDec call this file's
// randomize helpers use elsewhere. Storage exhaustion (EnsureModulationDepth
// returning null) leaves the depth unmaterialized rather than forcing a
// write, the same convention this file uses everywhere else CanAllocate()
// can fail.
inline void ApplyAudioPitchDetent(FroggersParameterModel& model, const AudioPitchDetentSpec& spec) {
    synth::Parameter& target = model.PageParameter(FroggersBankId::Audio, spec.targetParamIx);
    synth::Parameter* depth = target.EnsureModulationDepth(spec.modIx);
    if (depth == nullptr) {
        return;
    }
    for (const synth::SceneState& pole : kScenePoles) {
        depth->SceneCenter(pole.leftScene) = kNeutralModulationDepthCenter;
        depth->HandleIncDec(pole, spec.sign * kPlaceholderEncoderDetent);
    }
}

// If `parameter` is one of the Audio bank's three pitch parameters, applies
// whichever of kAudioPitchDetents targets it. A no-op for every other
// parameter -- the loop below simply never matches -- which is what lets
// Reset's drilled-in branch call this unconditionally on whatever parameter
// is currently selected, at any level, with no separate identity gate.
inline void RestoreAudioPitchDetentsFor(FroggersParameterModel& model, synth::Parameter& parameter) {
    for (const AudioPitchDetentSpec& spec : kAudioPitchDetents) {
        if (&model.PageParameter(FroggersBankId::Audio, spec.targetParamIx) == &parameter) {
            ApplyAudioPitchDetent(model, spec);
        }
    }
}

// Scene 1 (pole 0) keeps sine/saw/square = 0.0/0.5/1.0 -- Audio bank slots
// 3-5, confirmed against EvalWaveMorph's sine->saw (0-0.5) / saw->square
// (0.5-1.0) crossfade (app/dsp/Vco.hpp's EvalWaveMorph, ported from
// 08b5fd3:src/core/VcoWaveEval.hpp:7-23). Scene 2 (pole 1) gets the MIRROR of that
// same value, `1.0 - shape` -- written as an expression of the scene-1
// value, not as three more hardcoded literals, so the mirror relationship is
// what the reader sees. VCO2's 0.5 (saw) is its own mirror and is therefore
// unchanged in scene 2 too, without needing a special case. The six pitch
// detents ride alongside, identical in both scene poles (same sources, same
// signs, same magnitude).
inline void ApplyAudioBankOverlay(FroggersParameterModel& model) {
    constexpr std::array<float, 3> kScene1VcoShapes{0.0f, 0.5f, 1.0f};
    for (std::size_t vcoIx = 0; vcoIx < kScene1VcoShapes.size(); ++vcoIx) {
        const float scene1Shape = kScene1VcoShapes[vcoIx];
        synth::Parameter& shapeParam = model.PageParameter(FroggersBankId::Audio, AudioSlot(vcoIx, VcoSlotRole::Shape));
        shapeParam.HandleSetAbsolute(kScenePole0, scene1Shape);
        shapeParam.HandleSetAbsolute(kScenePole1, 1.0f - scene1Shape);
    }
    for (const AudioPitchDetentSpec& spec : kAudioPitchDetents) {
        ApplyAudioPitchDetent(model, spec);
    }
}

// Gain (Drive bank, slot 1) = 20% of its range, identical in both scene
// poles -- only the Audio bank's shapes differ between poles. Wet/Dry (slot
// 0) is left at its own registered default (0.0f, fully dry) by the loop
// above this overlay runs after -- the instrument ships bypassed on the
// Drive page, driven at a nonzero Gain.
inline void ApplyDriveBankOverlay(FroggersParameterModel& model) {
    for (const synth::SceneState& pole : kScenePoles) {
        model.PageParameter(FroggersBankId::Drive, 1).HandleSetAbsolute(pole, 0.2f);
    }
}

// Applies bank `bankId`'s own slice of the default patch: every one of its
// kFroggersParamsPerBank page parameters and its Crispy, both scene poles,
// set to that parameter's own registered default -- then the bank's own
// overlay on top where one exists (Audio, Drive).
inline void ApplyBankDefaultPatch(FroggersParameterModel& model, FroggersBankId bankId) {
    const FroggersBankLayout& layout = FroggersBankLayouts()[static_cast<std::size_t>(bankId)];
    for (std::size_t paramIx = 0; paramIx < kFroggersParamsPerBank; ++paramIx) {
        synth::Parameter& param = model.PageParameter(bankId, paramIx);
        for (const synth::SceneState& pole : kScenePoles) {
            param.HandleSetAbsolute(pole, layout.params[paramIx].defaultValue);
        }
    }
    for (const synth::SceneState& pole : kScenePoles) {
        model.Crispy(bankId).HandleSetAbsolute(pole, 0.0f);
    }
    if (bankId == FroggersBankId::Audio) {
        ApplyAudioBankOverlay(model);
    } else if (bankId == FroggersBankId::Drive) {
        ApplyDriveBankOverlay(model);
    }
}

// Crunchy is a single Parameter registered before the per-bank loop and
// shared into every bank's slot 15 (FroggersParameters.hpp's `FroggersParameterModel::Init`,
// reached here through FroggersParameterModel::Crunchy()) -- it belongs to
// no one bank, so it is not part of ApplyBankDefaultPatch above; this is its
// own default-patch slice, the "plus the globals" a bank-addressable default
// patch still needs.
inline void ApplyCrunchyDefaultPatch(FroggersParameterModel& model) {
    for (const synth::SceneState& pole : kScenePoles) {
        model.Crunchy().HandleSetAbsolute(pole, 0.0f);
    }
}

// Reset counterpart to ApplyBankDefaultPatch: clears whatever pre-existing
// (possibly Randomize-dirtied) depths this bank's page parameters and
// Crispy carry, THEN applies ApplyBankDefaultPatch on top -- so any depth
// ApplyBankDefaultPatch's own overlay materializes (the Audio bank's cross-
// VCO detents) is the last write and survives, while every other depth lands
// on neutral exactly as ApplyBankDefaultPatch already leaves a freshly
// constructed instance.
inline void ResetBankToDefaultPatch(FroggersParameterModel& model, FroggersBankId bankId) {
    for (std::size_t paramIx = 0; paramIx < kFroggersParamsPerBank; ++paramIx) {
        ZeroExistingModulationDepths(model.PageParameter(bankId, paramIx));
    }
    ZeroExistingModulationDepths(model.Crispy(bankId));
    ApplyBankDefaultPatch(model, bankId);
}

// Reset counterpart to ApplyCrunchyDefaultPatch, mirroring
// ResetBankToDefaultPatch's own clear-then-apply shape for the one global
// the default patch also needs.
inline void ResetGlobalCrunchyToDefaultPatch(FroggersParameterModel& model) {
    ZeroExistingModulationDepths(model.Crunchy());
    ApplyCrunchyDefaultPatch(model);
}

}  // namespace detail

// Randomize Page -- "randomize exactly what is displayed."
//   - parameter page (drillIn.Level()==0): this bank's 14 values + Crispy,
//     excluding Crunchy; no depths.
//   - level-1 grid (Level()==1): one RandomizeParameterModulationDepths call
//     on the selected parameter (the L1 parameter's own 15 depths).
//   - level-2 grid (Level()==2): one RandomizeParameterModulationDepths call
//     on the selected (depth) parameter's own 15 depths.
inline FroggersRandomizeResult RandomizePage(synth::ParameterManager& manager, FroggersModulationDrillIn& drillIn) {
    if (drillIn.Level() == 0) {
        // Randomize Page: this page's own Crispy IS included -- see the flag's
        // comment on RandomizeBankValues for why the two callers differ.
        detail::RandomizeBankValues(manager, drillIn.BankRef(), /*includeCrispy=*/true);
        return {};
    }
    // Level 1 or 2: one RandomizeParameterModulationDepths call (the
    // count/source-selection helper above) on whichever parameter is currently
    // selected -- not a Target/Back-cell press; the helper is
    // called on the selected parameter directly. Floor 0: Randomize Page's
    // contract is to randomize exactly what is displayed, and a floor is not
    // part of that.
    synth::Parameter& selected = *drillIn.BankRef().SelectedParameter();
    const bool partial = detail::RandomizeParameterModulationDepths(manager, selected, /*minimumSources=*/0);
    return {partial};
}

// Randomize All -- context-sensitive by view, "wider and deeper."
//   - parameter page (Level()==0): every top-level parameter ACROSS ALL SIX
//     BANKS -- value + level-1 depths, Crunchy excluded. Also randomizes the
//     Crispy VALUE of at most kMaxRandomizedCrispy of the six banks (0 to 2,
//     drawn fresh each press -- see that constant's own comment for why not
//     all six).
//     Never descends to level 2. Each bank is pressed directly via its own
//     `Bank&` (PressBankWithRandomValue), independent of which bank the
//     BankSlot currently displays, so the active/displayed bank is left
//     completely undisturbed by this global operation.
//   - ANY drilled-in grid (Level() >= 1): the ONE selected parameter's own
//     depths, PLUS one more RandomizeParameterModulationDepths call on each of
//     those depths that is ACTUALLY MODULATING (detail::DepthIsModulating) --
//     i.e. randomizing at level N also randomizes level N+1, at every N, gated
//     only by kMaxDrillLevel: "level 1 randomize all
//     should affect level 2 randomization, level 2 randomize all should affect
//     level 3." There is no per-level branch here: a per-level branch that
//     descended at level 1 only and let levels 2/3 fall through to
//     RandomizePage would be a hardcoded special case of exactly the kind
//     this class's Back() avoids.
//
//     Two things this deliberately does NOT do. It never opens a view --
//     RandomizeParameterModulationDepths takes `Parameter&` and reads no view
//     state -- so the level counter never moves, avoiding a bug where
//     Randomize All would eject the player out of the drilled-in view back to
//     level 0. And it
//     skips neutral depths rather than descending into every materialized one:
//     a neutral depth modulates nothing, and sub-modulating it made the
//     drilled parameter report 13 badges against 1 live source, because
//     Sheaf's badge criterion counts a depth that merely HAS sub-modulation.
//     See detail::DepthIsModulating's own comment.
inline FroggersRandomizeResult RandomizeAll(synth::ParameterManager& manager, FroggersModulationDrillIn& drillIn,
                                            FroggersParameterModel& model, FroggersModulationSlate& slate) {
    if (drillIn.Level() == 0) {
        bool partial = false;
        // Draw how many of the six banks' Crispy this press randomizes (0 to
        // kMaxRandomizedCrispy -- see that constant's own comment for why 2,
        // not all 6), then which banks: a partial Fisher-Yates over a 0..5
        // pool of bank indices, the same distinct-pick idiom
        // RandomizeParameterModulationDepths' own source draw uses, reused
        // here via the same SwapInRandomPick step rather than reimplemented.
        // Drawn before the six-bank loop below so every bank's own
        // NextRandomCoin()/NextRandomIndex() sequence inside that loop is
        // unaffected by how many banks this press's Crispy draw touches.
        const std::size_t crispyCount = detail::DrawGeometricCount(manager, 0, detail::kMaxRandomizedCrispy);
        std::array<std::size_t, kFroggersBankCount> crispyPool{};
        for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
            crispyPool[bankIx] = bankIx;
        }
        std::array<bool, kFroggersBankCount> crispyDrawn{};
        for (std::size_t i = 0; i < crispyCount; ++i) {
            crispyDrawn[detail::SwapInRandomPick(manager, crispyPool, i)] = true;
        }

        // Hoisted for the same reason as RandomizeBankLevel1Depths's own
        // loop above (the same short-circuit hazard FroggersAppCore::ProcessFrame()
        // guards against) -- this loop is
        // ACROSS ALL SIX BANKS, so a short-circuited `||` here would skip
        // randomizing every remaining bank once one had already gone partial.
        for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
            const auto bankId = static_cast<FroggersBankId>(bankIx);
            synth::Bank& bank = model.BankAt(bankId);
            // Randomize All: this bank's Crispy VALUE is randomized only if
            // this press drew it above; Crunchy stays excluded regardless.
            detail::RandomizeBankValues(manager, bank, /*includeCrispy=*/crispyDrawn[bankIx]);
            const bool bankPartial = detail::RandomizeBankLevel1Depths(manager, bank);
            partial = partial || bankPartial;
        }
        // The gesture that re-rolls the whole patch also re-rolls the five
        // stepped sources' held values, so a locked phrase is a new phrase
        // afterwards. The salt comes from the manager's own generator, so a
        // fixture's injected index source governs it like every other draw.
        // Drilled-in levels below randomize one parameter's depths and
        // leave the bags alone.
        slate.ReseedRandomShLanes(static_cast<std::uint32_t>(manager.NextRandomIndex(std::size_t{1} << 32)));
        return {partial};
    }

    // ONE rule for every drilled-in level: "level 1
    // randomize all should affect level 2 randomization, level 2 randomize all
    // should affect level 3." There is no per-level branching: at ANY
    // drilled-in level, randomize the selected parameter's own depths, then
    // descend exactly one level, guarded by kMaxDrillLevel. Raising the cap
    // again needs no edit here. A `Level() == 1`-only branch with
    // levels 2 and 3 falling through to RandomizePage would make the descent a
    // hardcoded level-1 special case -- the same shape Back() avoids for its
    // own per-level replay.
    //
    // Level 0 already returned above, so this is every drilled-in level. No
    // guard condition and no enclosing block: a redundant `Level() >= 1` test
    // would only add an unreachable tail the compiler still demands a return
    // for.
    //
    // This deliberately does NOT Back() out to level 0 to look up the selected
    // parameter's
    // encoder id, PressEncoder back in, then PressEncoder/Back once per depth
    // parameter to reach the next level -- that round trip's only permanent
    // effect would be driving the level counter to 0, ejecting the player out
    // of the drilled-in view. None
    // of it is needed: RandomizeParameterModulationDepths takes `Parameter&`
    // directly, reads eligibility from `group.GetModulators().Metadata()`, and
    // calls EnsureModulationDepth itself -- nothing in it reads view state. So
    // this only ever calls that helper, never the view, and the level never
    // moves. (Bank::Deselect(), which such a round trip would call via Back(),
    // always prunes
    // the transient eager materialization back to whatever was actually
    // written, so avoiding the round trip leaves the steady-state count
    // unchanged; what it avoids is the wasted allocate-then-prune churn.)
    synth::Parameter& selectedParam = *drillIn.BankRef().SelectedParameter();
    // Floor 1: a drilled-in Randomize All press must never be a no-op.
    bool partial = detail::RandomizeParameterModulationDepths(manager, selectedParam, /*minimumSources=*/1);
    // Descend one level -- but ONLY if a deeper level exists to descend into.
    // Read from the drill-in's own single definition site rather than
    // re-testing a literal.
    if (drillIn.Level() < FroggersModulationDrillIn::kMaxDrillLevel) {
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            synth::Parameter* depthParam = selectedParam.ModulationDepthParameter(modIx);
            if (depthParam == nullptr) {
                continue;  // not connected, or the draw above hit CanAllocate()==false
            }
            // Descend only into depths that are ACTUALLY modulating. A
            // materialized-but-neutral depth modulates nothing, so
            // sub-modulating it is meaningless -- and it is what made the
            // drilled parameter show 13 badges when 1 source was live. See
            // DepthIsModulating's own comment for the badge mechanism.
            if (!detail::DepthIsModulating(*depthParam)) {
                // CLEAR it rather than merely skipping. Randomize is
                // non-additive: each press zeroes the selected
                // parameter's own depths before drawing, so a source picked
                // last press is neutral this press. But
                // ZeroExistingModulationDepths DOES NOT RECURSE -- it clears
                // this parameter's depths, not those depths' own sub-depths.
                // Skipping alone would therefore leave the sub-depths a
                // PREVIOUS press wrote sitting on a now-neutral depth, and
                // Sheaf's badge criterion counts a depth that merely HAS
                // sub-modulation -- so the badge would stay lit for a source
                // that is modulating nothing, which is the very defect this
                // rule exists to remove.
                detail::ZeroExistingModulationDepths(*depthParam);
                continue;
            }
            // Hoisted for the same reason (the same short-circuit hazard
            // FroggersAppCore::ProcessFrame() guards against)
            // -- this loop is the depth-parameter sweep, so a short-circuited
            // `||` here would skip randomizing every remaining depth parameter.
            // Floor 1: the other half of the same drilled-in Randomize All
            // press -- never a no-op on a modulating depth either.
            const bool depthPartial = detail::RandomizeParameterModulationDepths(manager, *depthParam, /*minimumSources=*/1);
            partial = partial || depthPartial;
        }
    }
    return {partial};

}

// ============================================================================
// Reset Page/Reset All: siblings of RandomizePage/RandomizeAll above,
// mirroring their own drillIn.Level() branching and enumeration -- except
// that at Level()==0 both revert to the default patch (see above) rather
// than to Randomize's own drawn values, and Reset All additionally reaches
// the global Crunchy, which Randomize All never touches. Reset is fully
// deterministic (no manager.NextRandom*() calls), so unlike Randomize there
// is no partial/capacity-exhaustion outcome to report; both return void.
// ============================================================================

// Reset Page.
//   - parameter page (drillIn.Level()==0): this bank's own slice of the
//     default patch -- its kFroggersParamsPerBank values, its Crispy, and
//     (on the Audio bank) its shape/pitch-detent overlay -- via
//     ResetBankToDefaultPatch. Crunchy belongs to no one bank, so Reset Page
//     leaves it untouched.
//   - level-1/level-2 grid (Level()==1 or 2): the SAME set
//     RandomizeParameterModulationDepths would act on from that view -- the
//     selected parameter's own depth children -- reset to their default-
//     patch value: neutral, except the depths RestoreAudioPitchDetentsFor
//     restores when `selected` is one of the Audio bank's pitch parameters.
//     `selected`'s own value is deliberately left untouched here, matching
//     Randomize exactly: RandomizeParameterModulationDepths never writes to
//     `parameter` itself, only to its depth children. This also matters at
//     level 2, where `selected` IS itself a depth parameter -- forcing it to
//     a value directly here would reintroduce the "0.0 is not off" trap
//     through a different call site.
inline void ResetPage(synth::ParameterManager& /*manager*/, FroggersModulationDrillIn& drillIn,
                       FroggersParameterModel& model) {
    if (drillIn.Level() == 0) {
        for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
            if (&model.BankAt(bankIx) == &drillIn.BankRef()) {
                detail::ResetBankToDefaultPatch(model, static_cast<FroggersBankId>(bankIx));
                break;
            }
        }
        return;
    }
    synth::Parameter& selected = *drillIn.BankRef().SelectedParameter();
    detail::ZeroExistingModulationDepths(selected);
    detail::RestoreAudioPitchDetentsFor(model, selected);
}

// Reset All -- global, at every level.
//   - parameter page (Level()==0): every bank's own slice of the default
//     patch (ResetBankToDefaultPatch, the same helper Reset Page uses for
//     its one bank), PLUS the single shared global Crunchy
//     (ResetGlobalCrunchyToDefaultPatch). Neither the per-bank Crispy
//     carve-out nor the never-touch-Crunchy rule Randomize All applies to
//     itself carries over to Reset: Reset All reverts the WHOLE patch, and
//     Crunchy is part of it. Each bank is reached directly via
//     `model.BankAt(bankId)` (RandomizeAll's own idiom), independent of
//     which bank the BankSlot currently displays.
//   - ANY drilled-in grid (Level() >= 1): the selected parameter's own depth
//     children reset to their default-patch value (RestoreAudioPitchDetentsFor,
//     as above), PLUS -- mirroring RandomizeAll's own recursive one-level
//     descent, gated the same way by kMaxDrillLevel -- each of THOSE depths'
//     own depth children reset to neutral too (no default-patch override
//     ever reaches a second level deep, so grandchildren are always plain
//     neutral). Unlike RandomizeAll, this does NOT gate the descent on
//     detail::DepthIsModulating first: that gate exists to skip meaningless
//     RANDOM draws on a depth that modulates nothing, which does not apply
//     to a deterministic clear -- ZeroExistingModulationDepths is already a
//     safe no-op on a depth that has no materialized children of its own.
inline void ResetAll(synth::ParameterManager& /*manager*/, FroggersModulationDrillIn& drillIn,
                      FroggersParameterModel& model) {
    if (drillIn.Level() == 0) {
        for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
            detail::ResetBankToDefaultPatch(model, static_cast<FroggersBankId>(bankIx));
        }
        detail::ResetGlobalCrunchyToDefaultPatch(model);
        return;
    }

    synth::Parameter& selectedParam = *drillIn.BankRef().SelectedParameter();
    detail::ZeroExistingModulationDepths(selectedParam);
    detail::RestoreAudioPitchDetentsFor(model, selectedParam);
    if (drillIn.Level() < FroggersModulationDrillIn::kMaxDrillLevel) {
        for (std::size_t modIx = 0; modIx < FroggersParameterModel::kNumModulators; ++modIx) {
            synth::Parameter* depthParam = selectedParam.ModulationDepthParameter(modIx);
            if (depthParam == nullptr) {
                continue;  // not connected, or never materialized.
            }
            detail::ZeroExistingModulationDepths(*depthParam);
        }
    }
}

// ============================================================================
// The default patch, applied once on a fresh launch.
// ============================================================================
// Requires slate indices 6-8 (VCO audio sources) to already be registered
// (the Audio bank's own overlay materializes real modulation depths sourced
// from them), hence this must run AFTER FroggersModulationSlate::Init. Every
// bank's own slice (detail::ApplyBankDefaultPatch, defined above alongside
// Reset -- the same function both consume) plus the one global, Crunchy.
inline void ApplyFroggersDefaultPatch(FroggersParameterModel& model) {
    for (std::size_t bankIx = 0; bankIx < kFroggersBankCount; ++bankIx) {
        detail::ApplyBankDefaultPatch(model, static_cast<FroggersBankId>(bankIx));
    }
    detail::ApplyCrunchyDefaultPatch(model);
}

}  // namespace synth_froggers
