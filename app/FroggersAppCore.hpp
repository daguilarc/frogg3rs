#pragma once

// synth_froggers::FroggersAppCore -- the app's Core/UI/outer-composition
// split, following Braid 4's own shape (apps/braid-4/Braid4Core.hpp +
// Braid4UI.hpp + Braid4.hpp).
//
// This class contains the `FroggersApp` state (Config/Init/PrepareToPlay/
// ProcessBlock/RouteAudioSample and every existing accessor) plus the
// UI-thread -> audio-thread request bridge described below. The composed
// `synth_froggers::FroggersApp` (still that exact name, so every existing
// test TU's `#include "Froggers.hpp"` / `synth_froggers::FroggersApp` keeps
// working unchanged) lives in Froggers.hpp as a thin wrapper: `class
// FroggersApp : public FroggersAppCore` that adds only the
// `FroggersUiSurface` member and `PortableSurface()` -- exactly Braid4.hpp's
// own shape.
//
// ============================================================================
// Why a request bridge exists
// ============================================================================
// `synth::AppContext` documents `parameterManager`/`masterClock` as
// audio-thread-owned once the engine is running (AppContext.hpp's own
// thread-role comments); the sanctioned way for UI code (message thread) to
// influence them is the existing `MessageInBus* uiBus` (message thread
// produces, audio thread consumes inside `Engine::ProcessBlock`'s
// `DrainMessageBus` call, itself before `app_.ProcessBlock()`).
//
// That generic bus is exactly right for encoder DRAG (`MessageIn::
// ParamIncDec`/`ParamSetAbsolute`), scene select/blend, and transport Start/
// Stop -- `Bank::HandleTick`/`HandleSetAbsolute` only look up the currently
// visible cell and never touch `selected_`/level state (verified by reading
// `src/ParameterModulation.cpp`'s `Bank::HandleTick`/`HandleSetAbsolute`),
// so this surface pushes those four kinds of action straight onto
// `context_->uiBus`, the same way `apps/braid-4/Braid4UI.hpp` does.
//
// It is NOT right for an encoder PRESS (drill-in), Randomize All/Page, or
// the BPM slider, for two different reasons:
//   - Encoder press MUST go through `FroggersModulationDrillIn::PressEncoder`
//     (FroggersModulation.hpp) rather than a generic
//     `MessageIn::ParamPush`, because that class is the ONLY thing enforcing
//     this app's 2-level drill-in cap -- Sheaf's own `Bank` has no level
//     concept at all (FroggersModulation.hpp's own header comment) and would
//     happily let a generic press descend to a third, fourth, ... level.
//   - Randomize All/Page (`FroggersModulation.hpp`'s `RandomizeAll`/
//     `RandomizePage`) and the BPM slider (`MasterClock::SetTempoBpm`/
//     `TempoBpm`, both audio-thread-owned per `AppContext.hpp`) mutate
//     audio-thread-owned state directly, with no existing generic
//     `MessageIn` shape for either of them.
//
// Crunchy no longer routes through this bridge (operator 2026-07-27): the
// chrome-band Crunchy slider that used to call `RequestCrunchy()` here is
// removed (it duplicated the real control at bank slot 15 -- see
// FroggersUiSurface.hpp's own header comment). Crunchy is a
// `Parameter` shared across all six banks, but at slot 15 it is addressed
// exactly like any other bank parameter (generic encoder press/drag over
// `context_->uiBus`), so it never needed a pending-atomic request of its
// own -- the chrome slider was the only thing that did, purely because it
// bypassed the bank/slot addressing scheme entirely.
//
// The fix used throughout this file is the same one already established by
// this app's clock-driven Marbles source for "audio-thread state written
// once per block, safely observed cross-thread": small, single-slot
// pending-request atomics that the UI/message thread WRITES (`Request*`
// methods below) and
// that `ProcessFrame()` (detected via `AppConcepts.hpp`'s `HasProcessFrame`
// concept, invoked by `synth::Engine` once per block, after message drains
// and before `ProcessBlock()` -- i.e. on the audio thread) DRAINS and
// applies. Display-direction state (the master clock's active tempo,
// whether it is externally slaved) is published the same way in reverse,
// once per block from `ProcessBlock()`'s existing end-of-block publish
// section.

#include "FroggersModulation.hpp"
#include "FroggersParameters.hpp"
#include "FroggersTransferFunctionVisualizer.hpp"

#include "dsp/Delay.hpp"
#include "dsp/Drive.hpp"
#include "dsp/DspMath.hpp"
#include "dsp/FilterFx.hpp"
#include "dsp/Limiter.hpp"
#include "dsp/RecoveryTier.hpp"
#include "dsp/Reverb.hpp"
#include "dsp/Vco.hpp"
#include "dsp/VoiceEnvelope.hpp"

#include "synth/AppConcepts.hpp"
#include "synth/AppContext.hpp"
#include "synth/Color.hpp"
#include "synth/DspScope.hpp"
#include "synth/MasterClock.hpp"
#include "synth/PortableScopeVisualizer.hpp"
#include "synth/PortableUI.hpp"
#include "synth/PortableUIBuilders.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>       // QueueRecordingExport's local-date file name.
#include <cmath>
#include <cstdio>   // Stop diagnostic (stopDiagBlocks_): std::fprintf.
#include <cstdlib>  // Stop diagnostic (stopDiagEnabled_): std::getenv.
#include <cstddef>
#include <cstdint>      // EncodeWavPcm16Mono's byte/sample types.
#include <ctime>        // QueueRecordingExport's local-date file name.
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace synth_froggers {

// The bank the editor is showing: whichever one the published UI-state
// snapshot marks selected (`AppContext::uiState->banks[].selected`, written
// by ParameterManager::PopulateUIState). Read from that snapshot and never
// from BankSlot::SelectedBank(), a plain non-atomic pointer the audio thread
// writes with no synchronization, unsafe for message-thread callers.
inline std::size_t FroggersVisibleBankIndex(const synth::AppContext& context) {
    if (context.uiState == nullptr) {
        return 0;
    }
    for (std::size_t bankIx = 0; bankIx < context.uiState->bankCapacity; ++bankIx) {
        if (context.uiState->banks[bankIx].selected.load(std::memory_order_relaxed)) {
            return bankIx;
        }
    }
    return 0;
}

// Forward-declared so QueueRecordingExport() (below, inside the class) can
// call it -- the definition (a pure std:: WAV encoder) sits after the class,
// beside its own tests' idiom of exercising it standalone; see its own
// comment there.
inline std::vector<std::uint8_t> EncodeWavPcm16Mono(std::span<const float> samples, float sampleRate);

class FroggersAppCore {
public:
    // The ONLY hand-written constructor work this class needs -- everything
    // else is default member initializers. Reserves the VCO scope's three
    // channels (one per VCO) and wires each VCO's ScopeWriterHolder/color,
    // then constructs the two transfer-function Visualizers over this
    // instance's own peakUiState_/combUiState_ (UIState members, populated
    // once per block in ProcessBlock()).
    // `synth::ui::ScopeVisualizer`/`synth_froggers::TransferFunctionVisualizer`
    // have no default constructor, so this class cannot rely on implicit
    // default construction.
    FroggersAppCore()
        : vco1ScopeHolder_(vcoScopeWriter_.ReserveChans(1)),
          vco2ScopeHolder_(vcoScopeWriter_.ReserveChans(1)),
          vco3ScopeHolder_(vcoScopeWriter_.ReserveChans(1)),
          vcoScopeVisualizer_(
              std::array<dsp::Vco::UIState*, 3>{&vco1ScopeUiState_, &vco2ScopeUiState_, &vco3ScopeUiState_},
              // Sizing verdict: the defaults below are correct for this app;
              // no change needed.
              //   - Read window 512 samples across a 340px-wide panel
              //     (FroggersUiSurface's kScopeWidth) is ~1.5 samples per
              //     pixel -- slightly oversampled, which is the right side to
              //     err on: the polyline builder decimates, it cannot invent
              //     detail. At 48kHz, 512 samples = 10.7ms, so the lowest
              //     default voice (110Hz, 9.1ms period) shows ~1.2 cycles and
              //     the highest (330Hz) ~3.5. That is a readable window.
              //   - ScopeWriter's default ring is maxFrames=4096 (DspScope.hpp),
              //     8x the read window and 85ms of history, with 3 of its 16
              //     channels reserved. Ample.
              //   - Braid 4's kScopeFrames = 6'553'600 is NOT a target to
              //     match: that sizes a marker-aligned long-window display it
              //     hand-builds. Copying the number without the display it
              //     serves would just allocate ~400MB for nothing.
              /*minY=*/-1.0f, /*maxY=*/1.0f, /*numSamples=*/512, /*drawMarkers=*/true),
          peakVisualizer_(peakUiState_, synth::Color::Blue),
          combVisualizer_(combUiState_, synth::Color::Blue.AdjustBrightness(0.7f)) {
        audioVcos_[0].SetScopeWriterHolder(&vco1ScopeHolder_);
        audioVcos_[1].SetScopeWriterHolder(&vco2ScopeHolder_);
        audioVcos_[2].SetScopeWriterHolder(&vco3ScopeHolder_);
        // These were previously Red/Orange/Yellow -- three hues that collapse
        // together under red-green colour blindness (protanopia/deuteranopia), an operator
        // report from the running build. `synth::Color` has `Cyan` and
        // `Yellow` (External/Sheaf/projects/synth/include/synth/Color.hpp)
        // but no `Pink`/`Magenta` (verified by grep of that file), so pink
        // is `Color::Rgb(255, 105, 180)` -- bright against the panel's dark
        // background and separated from cyan/yellow in both blue channel
        // and luminance. Do NOT restore Red/Orange/Yellow.
        audioVcos_[0].SetScopeColor(synth::Color::Cyan);
        audioVcos_[1].SetScopeColor(synth::Color::Rgb(255, 105, 180));  // pink -- no synth::Color::Pink/Magenta exists.
        audioVcos_[2].SetScopeColor(synth::Color::Yellow);
    }

    // Unregisters the routed-input callback Init() below registers, if any.
    // `context_->inputRoutingSignal` is host-owned and can outlive this
    // object (it is declared to survive exactly that -- see Runtime.hpp's
    // own member-ordering comment), so a still-registered callback capturing
    // `this` would dangle the moment this destructor finishes; clearing it
    // here removes that registration first.
    ~FroggersAppCore() {
        if (context_ != nullptr) {
            context_->SetInputRoutedChangedCallback({});
        }
    }

    static synth::RuntimeConfig Config() {
        synth::RuntimeConfig config;
        config.appName = "Frogg3rs Synth";
        // The runtime's own first sidebar page is called "Audio", and so is
        // this instrument's first parameter bank, so the two buttons read as
        // the same thing on screen and are not. The page selects the output
        // device AND the input device, so it is named for both rather than
        // for the input alone. Set for every host: the bank is called Audio
        // in the standalone as much as in the browser.
        config.audioPageTitle = "Audio I/O";
        // One input channel is requested. That alone never means an input is
        // actually available to the operator: requesting any nonzero count
        // also opens a platform-default input device at launch, before any
        // device selection is made -- on this machine, the built-in
        // microphone, unasked (Runtime.hpp's `initialiseWithDefaultDevices`
        // call). The external-audio modulation sources
        // (`kModSlotExternalAudio`/`kModSlotExternalAudioEf`,
        // FroggersModulation.hpp) never treat channel presence as consent:
        // `Init()` below subscribes their `connected` state to the host's
        // routed-input signal instead (`synth::AppContext::InputRouted()`/
        // `SetInputRoutedChangedCallback()`), which reads true only while an
        // operator-selected input device is open and current, and stays
        // false for a platform-default device nobody chose.
        config.numAudioInputs = 1;
        config.numAudioOutputs = 2;
        config.preferredSampleRate = 48000.0;
        config.preferredBlockSize = 256;
        config.uiWidth = 900;
        // This literal is just the window's INITIAL size, not a derived
        // cross-check. Sheaf's own layout engine resolves one declarative
        // grid's regions against whatever root extent it is given
        // (`FroggersPageLayout::RootBounds()`), so there is nothing for
        // this literal to be derived FROM.
        // 632 -> 712, +80px (4 encoder rows x
        // `synth_froggers::FroggersEncoderGridLayout::kLabelBandHeight`,
        // 20px each -- FroggersUiSurface.hpp), so the label band added below
        // each encoder ring has somewhere to live without shrinking the ring
        // or the window's other rows -- the operator's own ruling was "do
        // not shrink the encoder ring" / "space you should have ADDED below
        // each encoder". This is simply a plain, hand-picked initial window
        // size, matching `synth_froggers::FroggersPageLayout::kDefaultHeight`
        // (FroggersUiSurface.hpp) by hand, not derivation (that file's own
        // comment explains why no cross-check test exists for this pair --
        // such a test can pass even when both sides are already wrong).
        // `FroggersSurfaceTests.cpp`'s fit-guard tests assert the surface
        // actually fits its declared grid at this size (and at a smaller and
        // a larger one), which is a stronger guarantee than a cross-check
        // ever was -- the old test could never fail even when wrong.
        config.uiHeight = 712;
        config.uiFrameHz = 30;
        return config;
    }

    void Init(synth::AppContext* context) {
        if (context == nullptr || context->parameterManager == nullptr) {
            throw std::invalid_argument("FroggersApp requires a valid app context");
        }
        context_ = context;
        // Builds the six 16-slot parameter banks (Crispy@14/Crunchy@15 in
        // every bank), the shared BankSlot, and the mono ParameterGroup +
        // scenes. See FroggersParameters.hpp.
        // peakVisualizer_/combVisualizer_ are constructed by this class's own
        // constructor (see its comment), ahead of Init() always running --
        // passed in so FroggersParameterModel can attach them as the Filter
        // bank's Peak/Comb parameter underlays.
        parameters_.Init(*context->parameterManager, &peakVisualizer_, &combVisualizer_);

        // Registers all 15 modulation sources -- see FroggersModulation.hpp
        // for the full list and ordering.
        modulation_.Init(parameters_.Group());

        // Drives the external-audio modulation sources' connected state from
        // the host's routed-input signal: true only while an
        // operator-selected input device is open and current (see
        // synth::AppContext::InputRouted()'s own doc comment), never from
        // channel presence alone. The callback runs on the message thread
        // (InputRoutingSignal::Publish's contract) and only ever queues the
        // new value -- ProcessFrame() below (audio thread, once per block)
        // is what actually applies it to the slate's metadata, because
        // Modulators::UpdateModValues() reads that same metadata every
        // sample on the audio thread and a direct write from here would
        // race it.
        context_->SetInputRoutedChangedCallback([this](bool routed) {
            pendingExternalAudioRouted_.store(routed ? 1 : 0, std::memory_order_release);
        });
        // Applied directly, not through the pending atomic above: nothing
        // else touches modulation_'s metadata until the audio thread starts
        // running, so the signal's current value can be read and applied
        // here, once, without racing anything.
        modulation_.SetExternalAudioConnected(context_->InputRouted());

        // Applies the default patch once, on first start, now that slate
        // indices 6-8 (VCO audio sources) exist.
        ApplyFroggersDefaultPatch(parameters_);

        // The single active-bank authority plus its own drill-in level
        // tracker, constructed against bank 0 -- the same default active
        // selection FroggersParameterModel::Init() already made via
        // `slot_->SelectBank(banks_[0])`.
        drillIn_.emplace(parameters_.BankAt(activeBankIx_));
    }

    // Sample-rate-dependent modulation-slate setup: detected and called
    // automatically by synth::Engine via the optional HasPrepareToPlay hook
    // (AppConcepts.hpp:28-32) once the host negotiates a real sample rate.
    // Everything else in this class is sample-rate-independent at Init()
    // time.
    // The fallback used when the host hands this hook a non-positive sample
    // rate. See PrepareToPlay()'s own comment for why that is a real
    // possibility, not a hypothetical -- this is the ONE place that
    // possibility is handled.
    static constexpr double kFallbackSampleRateHz = 44100.0;

    void PrepareToPlay(double sampleRate, int /*blockSize*/) {
        // `synth::Engine::Prepare()` (External/Sheaf/
        // projects/synth/include/synth/Engine.hpp:289-311) guards
        // `sampleRate > 0.0 && blockSize > 0` before its OWN two uses
        // (MasterClock::Prepare, the uiPublishInterval_ computation) but
        // forwards this hook's `sampleRate`/`blockSize` UNCONDITIONALLY --
        // Sheaf's own engine treats a non-positive rate as real enough to
        // guard twice, then hands the raw value to the app hook regardless.
        // The real host origin is `synth_runtime::Runtime<App>::
        // audioDeviceAboutToStart` (External/Sheaf/projects/synth/runtime/
        // Runtime.hpp:580-593): `double sampleRate =
        // device->getCurrentSampleRate();` straight into `engine_.Prepare(
        // sampleRate, blockSize)`, no validation of its own -- a live
        // `juce::AudioIODevice` query, not a compile-time constant. A
        // non-positive rate is therefore genuinely reachable here, so it is
        // validated ONCE, before any use (including ConfigureProcessingTiming
        // below, which used to read the raw parameter directly), rather than
        // re-guarded downstream. This makes every per-unit guard this same
        // condition used to need (dsp/EnvelopeFollowers.hpp,
        // dsp/VoiceEnvelope.hpp, dsp/Delay.hpp, dsp/Limiter.hpp,
        // FroggersModulationSlate::Prepare) unreachable; each of those was
        // deleted rather than left duplicating this check with its own
        // fallback (44100.0, 1.0f via std::max, and 48000.0 all disagreed).
        // 44100.0 is the value that survives: a standard audio rate, unlike
        // Limiter's old 1.0f (which would have collapsed attack/release to
        // near-instant, not "a limiter", had it ever actually fired).
        if (!(sampleRate > 0.0)) {
            sampleRate = kFallbackSampleRateHz;
        }
        modulation_.Prepare(sampleRate);

        // The audio-path DSP units below are a
        // SEPARATE set of instances from modulation_'s own private VCOs
        // (FroggersModulation.hpp's vco1_/vco2_/vco3_, which exist solely to
        // produce the six VCO-audio-rate modulation-source values and are
        // never summed to the output bus -- see that header's own scope
        // note). These need their own sample-rate-dependent setup.
        sampleRate_ = static_cast<float>(sampleRate);
        audioAdsr_.init(sampleRate_);

        // Sheaf's parameter-smoothing constants
        // (kDefaultProcessLiteAlpha/kDefaultTargetComputeIntervalSamples/
        // kDefaultUiDisplayCenterAlpha/kDefaultUiDisplaySpreadAlpha,
        // ParameterModulation.hpp:170-174) are defined at a 48 kHz
        // reference and ParameterGroupConfig starts out holding exactly
        // those raw values (ParameterModulation.hpp:199-203) until
        // ConfigureProcessingTiming replaces them
        // (ParameterModulation.cpp:859-865) -- otherwise knob glide,
        // modulation-depth smoothing, and UI-display slew all run at the
        // wrong real-time rate at any host rate other than 48 kHz. Mirrors
        // Braid 4's own PrepareToPlay (Braid4Core.hpp:205-219), which
        // converts against internalSampleRate_ (its oversampled internal
        // parameter-tier rate); this app has no such oversampling at the
        // parameter tier (parameters_/modulation_ share the single mono
        // ParameterGroup below, driven once per sample at the host rate --
        // see modulation_.Init(parameters_.Group()) in Init()), so this
        // converts against the host `sampleRate` directly instead. One
        // call covers modulation_ too: modulation_.Init(parameters_.Group())
        // (Init(), above) hands it the SAME ParameterGroup instance, not a
        // separate one.
        parameters_.Group().ConfigureProcessingTiming(synth::ParameterProcessingTiming{
            .processLiteAlpha =
                synth::ConvertOnePoleAlpha(synth::kDefaultProcessLiteAlpha, 48000.0, sampleRate),
            .targetComputeIntervalSamples = synth::ConvertSampleInterval(
                synth::kDefaultTargetComputeIntervalSamples, 48000.0, sampleRate),
            .uiDisplayCenterAlpha =
                synth::ConvertOnePoleAlpha(synth::kDefaultUiDisplayCenterAlpha, 48000.0, sampleRate),
            .uiDisplaySpreadAlpha =
                synth::ConvertOnePoleAlpha(synth::kDefaultUiDisplaySpreadAlpha, 48000.0, sampleRate),
        });
        // The ASR gate is not forced permanently on -- it is driven per-sample in
        // ProcessBlock() from the master clock's transport quarter-note
        // pulse (see TransportQuarterNotesAt()/the gate computation there).
        // audioAdsr_.init() above already leaves the gate low
        // (VcoAdsrState::init() sets m_gateHigh = false, Stage::Idle), which
        // is the correct starting state for "silent until the transport
        // runs."
        delay_.SetSampleRate(sampleRate_);  // Also (re)configures wetLimiterL/R's coeffs, see dsp/Delay.hpp.
        outputLimiter_.Configure(sampleRate_);  // The limiter's attack/release coefficients are sample-rate-dependent.
        filterChain_.Configure(sampleRate_);  // Peak-branch limiter's own coeffs, same reason.
        reverb_.Configure(sampleRate_);  // Reverb's own wetLimiter coeffs, same reason.
        driveBlendPhase_.Configure(sampleRate_);  // This stage's own outputLimiter coeffs, same reason.

        // Root-cause fix (a robustness gap found while diagnosing "Play
        // produces no audio in the real Runtime"): `synth::Engine::Prepare()`
        // calls `MasterClock::Prepare()` UNCONDITIONALLY before this hook
        // runs (Engine.hpp's own Prepare(), which calls
        // `masterClock_.Prepare(sampleRate, blockSize)` first, then this
        // method) -- and `MasterClock::Prepare()` unconditionally resets
        // `transportState_` to `Stopped`
        // (External/Sheaf/projects/synth/src/MasterClock.cpp:929), with no
        // regard for whether the transport was already `Running`.
        // `synth::Engine::Prepare()` is not a one-time startup call: JUCE's
        // `synth_runtime::Runtime<App>::audioDeviceAboutToStart` (Runtime.hpp)
        // re-invokes it on EVERY audio-device renegotiation -- not just user
        // device switches (`ApplyAudioDeviceSelection`/
        // `ApplyAudioDeviceInputSelection`), but also spontaneous ones during
        // ordinary startup (verified against a real session's own log,
        // ~/Library/Sheaf/synth/sheaf-patch/logs/: three separate "Audio
        // prepared" lines fired before any user interaction at all, one of
        // them renegotiating the block size from 256 to 512 frames). Each
        // one silently re-closes the transport-gated ASR with no visual
        // indication and no automatic recovery -- a user who pressed Play
        // and then hit (or caused, e.g. by switching output devices while
        // troubleshooting) any such renegotiation gets permanent silence
        // with no way to know Play needs pressing again. Verified
        // reproducible headlessly: `synth::Engine::Prepare()` called a
        // second time after a `MessageIn::Start` mid-session drops
        // `TransportState()` back to `Stopped` and output back to silence,
        // exactly matching the reported symptom.
        //
        // Fix: track the surface's last explicit Play/Stop request
        // independently of `MasterClock::TransportState()` (which this same
        // Prepare() call may have just clobbered), and re-push the SAME
        // `MessageIn::Start` the Play button itself pushes
        // (`FroggersUiSurface::HandleAction`, via the sanctioned
        // message-thread-producer/audio-thread-consumer `uiBus`) so the very
        // next `ProcessBlock` (audio thread, which always drains `uiBus`
        // before touching the clock plan) restores `Running` before another
        // sample is produced. `PrepareToPlay()` runs on the thread that
        // called `Engine::Prepare()` -- the message thread for every runtime
        // device-renegotiation path -- so pushing onto `uiBus` here is the
        // same safe, already-relied-upon cross-thread path `FroggersUiSurface`
        // uses, not a new one.
        if (desiredTransportRunning_.load(std::memory_order_acquire) && context_ != nullptr &&
            context_->uiBus != nullptr) {
            const std::uint64_t timestamp = context_->now ? context_->now() : 0;
            context_->uiBus->Push(synth::MessageIn::Start(timestamp));
        }
    }

    // Records the surface's last explicit Play/Stop request so
    // PrepareToPlay() can re-assert it across a `MasterClock::Prepare()`
    // reset triggered by an audio-device renegotiation (see
    // PrepareToPlay()'s own comment). Called from
    // `FroggersUiSurface::HandleAction` alongside its existing `uiBus` push
    // -- this does not change Play/Stop's own behavior, it only lets a LATER
    // renegotiation event recover it.
    void SetDesiredTransportRunning(bool running) {
        desiredTransportRunning_.store(running, std::memory_order_release);
    }

    // The Freeze transport button's
    // latch -- toggled by FroggersUiSurface::HandleAction's kFreeze branch
    // (UI/message thread) and read every sample inside RouteAudioSample()
    // (audio thread, below) to set DelayParams::dfrzLatched before it
    // reaches dsp::StereoDelay::Process(). Same release/acquire pairing as
    // SetDesiredTransportRunning()/its own read just above -- a direct
    // UI-thread-write, audio-thread-read flag, not the separate "pending
    // request -> published display value" two-hop some of the atomics below
    // use.
    void SetFreezeLatched(bool latched) {
        freezeLatched_.store(latched, std::memory_order_release);
    }
    bool FreezeLatched() const { return freezeLatched_.load(std::memory_order_acquire); }

    // The ONE gate every stop-edge
    // teardown site reads -- ForceReleaseAll(), the six `stoppedKnob`
    // overrides (all through ONE lambda instance, RouteAudioSample() below),
    // and both the immediate and deferred `ForEachStatefulUnit(Reset)`
    // clears (ProcessBlock() below) -- instead of each re-deriving its own
    // `!running`/`!FreezeLatched()` check. `running` is the
    // transport's ACTUAL state at the call site: pass the freshly computed
    // local where the edge-detect block below has one (it reads this BEFORE
    // updating `wasTransportRunning_`, so the member would still be stale),
    // or `wasTransportRunning_` itself everywhere later in the same sample,
    // where it is already current (see that member's own comment).
    //
    // Stopped-and-unlatched is the only condition the teardown runs under;
    // while FreezeLatched() is true the instrument holds its sounding state
    // instead of tearing down. This is the operator's deliberate route to
    // the sustained-drive drone the Freeze button exists to reproduce -- the
    // effect only ever existed with the transport stopped, so suppressing the teardown here
    // is what makes it reachable again, not a bug being reintroduced.
    bool TransportTeardownActive(bool running) const { return !running && !FreezeLatched(); }

    // Arms/stops a bounded mono capture of
    // what the operator hears. UI-thread call. Refuses while the transport
    // is stopped -- v1 precedent for the wording (desktop/Source/
    // MainComponent.cpp:201, reference only, not ported): exactly "Press
    // Play before recording." via RecordRefusalReason() below. All
    // allocation happens here, on the UI thread, never on the audio thread
    // -- `capacityFramesOverride` lets tests bound the buffer far below the
    // real kMaxRecordSeconds*sampleRate_ cap (~345 MB at 30 minutes/48kHz);
    // production callers pass the default (0).
    bool ArmRecording(std::size_t capacityFramesOverride = 0) {
        if (!TransportRunning()) {
            recordRefusalReason_ = kRecordRefusalReason;
            return false;
        }
        // A Record press that beats the engine's own per-tick
        // TakePendingFileExport() poll (see that method's own comment) would
        // otherwise have this call's recordBuffer_.assign() below wipe out a
        // truncated capture that was never exported -- flush it first so a
        // fresh take never wipes an unsaved one.
        QueueTruncatedExportIfPending();
        const std::size_t capacityFrames = capacityFramesOverride != 0
            ? capacityFramesOverride
            : static_cast<std::size_t>(kMaxRecordSeconds * sampleRate_);
        recordBuffer_.assign(capacityFrames, 0.0f);
        recordFrames_.store(0, std::memory_order_release);
        recordTruncated_.store(false, std::memory_order_release);
        recordRefusalReason_ = nullptr;
        captureExported_ = false;
        recordArmed_.store(true, std::memory_order_release);
        return true;
    }
    // Reason the LAST ArmRecording() call was refused, or nullptr if it
    // succeeded (or has never been called).
    const char* RecordRefusalReason() const { return recordRefusalReason_; }

    // UI-thread call to end a capture -- clears the armed flag only.
    // Captured data stays readable afterward via RecordedAudio()/
    // RecordedFrameCount() below (v1 precedent for the truncation wording,
    // reference only: "Recording stopped at the 30-minute limit.",
    // desktop/Source/MainComponent.cpp:219).
    void StopRecording() { recordArmed_.store(false, std::memory_order_release); }

    // Read-back for the capture (UI thread). Safe to call while still armed
    // (reads whatever has been written so far).
    std::span<const float> RecordedAudio() const {
        return std::span<const float>(recordBuffer_.data(), RecordedFrameCount());
    }
    std::uint64_t RecordedFrameCount() const { return recordFrames_.load(std::memory_order_acquire); }
    bool RecordingTruncated() const { return recordTruncated_.load(std::memory_order_acquire); }
    float RecordSampleRate() const { return sampleRate_; }

    // Whether a capture is currently armed -- the
    // surface's transport-row Record glyph and HandleAction both need this
    // (BuildRecordDrawCommands' colour swap, and the toggle branch's
    // arm-vs-stop decision), same one-line-acquire-load shape as
    // FreezeLatched() above.
    bool RecordArmed() const { return recordArmed_.load(std::memory_order_acquire); }

    // Queues the just-stopped recording as a file export -- called by the
    // SURFACE (FroggersUiSurface::HandleAction) once it decides a stop
    // counted as "finished with data," never internally by StopRecording()
    // itself. UI-thread call: encodes the captured samples to a WAV byte
    // stream, names the file from today's local date, and stores it for the
    // host's engine to pick up and hand to whatever it installed through
    // Engine::SetFileExportHandler -- this class only names and encodes the
    // file; the host decides how (and whether) to save it.
    void QueueRecordingExport() {
        const std::vector<float> samples(RecordedAudio().begin(), RecordedAudio().end());
        std::vector<std::uint8_t> bytes = EncodeWavPcm16Mono(samples, RecordSampleRate());

        const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char dateBuffer[16] = {};
        std::strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", std::localtime(&now));

        synth::FileExport fileExport;
        fileExport.fileName = std::string(dateBuffer) + ".wav";
        fileExport.mediaType = "audio/wav";
        fileExport.bytes = std::move(bytes);
        fileExport.note = RecordingTruncated() ? "stopped at the 30-minute limit" : "";
        pendingExport_ = std::move(fileExport);
        captureExported_ = true;
    }
    // Satisfies synth::HasFileExports -- the engine calls this once per
    // message-thread tick and hands whatever comes back to the handler the
    // host installed. Flushes a truncated capture first (see
    // QueueTruncatedExportIfPending()'s own comment): the audio thread
    // disarms on its own the instant it hits the cap, so no Stop/Record
    // press ever follows to queue that capture's export -- this poll is the
    // only place left that can pick it up.
    std::optional<synth::FileExport> TakePendingFileExport() {
        QueueTruncatedExportIfPending();
        std::optional<synth::FileExport> fileExport = std::move(pendingExport_);
        pendingExport_.reset();
        return fileExport;
    }

    // The surface's request API
    // -- called from FroggersUiSurface::DispatchAction (UI/message thread).
    // Each is a single-slot pending request; a later write before the audio
    // thread drains the previous one simply coalesces (acceptable: these are
    // all control-rate, human-paced actions, never a data stream). See this
    // file's header comment for why each one needs to be a request rather
    // than a direct call.
    void RequestBankSelect(std::size_t bankIx) {
        pendingBankSelect_.store(static_cast<int>(bankIx), std::memory_order_release);
    }
    void RequestEncoderPress(std::size_t encoderId) {
        pendingEncoderPress_.store(static_cast<int>(encoderId), std::memory_order_release);
    }
    void RequestRandomizeAll() { pendingRandomizeAll_.store(true, std::memory_order_release); }
    void RequestRandomizePage() { pendingRandomizePage_.store(true, std::memory_order_release); }
    // Reset Page / Reset All: same UI-thread -> audio-thread request
    // idiom as the two Randomize flags above, deliberately not a new one.
    void RequestResetAll() { pendingResetAll_.store(true, std::memory_order_release); }
    void RequestResetPage() { pendingResetPage_.store(true, std::memory_order_release); }
    // A negative sentinel means "no
    // pending request." `MasterClock::SetTempoBpm` itself already no-ops
    // (returns false) while slaved to external MIDI clock
    // (src/MasterClock.cpp:963-965) -- ProcessFrame() below still calls it
    // unconditionally when a request is pending; the surface's own
    // DispatchAction additionally never enqueues a request while slaved (see
    // FroggersUiSurface.hpp), so this is a belt-and-suspenders no-op, not the
    // sole guard.
    void RequestTempoBpm(double bpm) { pendingTempoBpmRequest_.store(bpm, std::memory_order_release); }

    // Display-direction reads for the surface (UI/message
    // thread), published once per block from ProcessBlock()'s existing
    // end-of-block section below.
    double DisplayTempoBpm() const { return tempoDisplayBpm_.load(std::memory_order_acquire); }
    bool TempoExternallyClocked() const { return tempoExternallyClocked_.load(std::memory_order_acquire); }
    // Published alongside tempoDisplayBpm_/
    // tempoExternallyClocked_ below, same cross-thread contract -- lets the
    // surface (message thread) know whether the transport is currently
    // running without reading `context_->masterClock` directly (audio-
    // thread-owned per AppContext.hpp). Used only to annotate the BPM
    // control, which has zero audible effect while the gate is closed
    // outright (ProcessBlock()'s own comment on `gateOpen`) -- no wiring
    // change, this is a read-only discoverability aid.
    bool TransportRunning() const { return transportRunningDisplay_.load(std::memory_order_acquire); }

    // Published once per block alongside
    // transportRunningDisplay_ above, same cross-thread contract -- lets the
    // surface (message thread) read the current modulation drill-in level
    // (FroggersModulationDrillIn::Level(), audio-thread-owned via drillIn_)
    // for its "Modulation Level N" header without touching drillIn_ directly
    // from the wrong thread.
    std::size_t DrillLevel() const { return drillLevelDisplay_.load(std::memory_order_acquire); }

    // True when
    // the MOST RECENT Randomize All/Page operation left
    // `FroggersRandomizeResult.partial` true -- i.e. `EnsureModulationDepth`
    // hit `!group_.CanAllocate()` (Sheaf, ParameterModulation.cpp:1825-1827)
    // and stopped that operation short of drawing its full chosen set.
    // Published from ProcessFrame() (audio thread) alongside the
    // ComputeAllParameters() reseed below, same cross-thread contract as
    // TransportRunning() above -- makes a silent partial randomize
    // observable to tests, without inventing any new UI element the operator
    // did not ask for. Any operator-visible logging must read this atomic
    // from the UI thread -- ProcessFrame() itself never logs (fprintf on
    // the audio thread can allocate/lock/block, which is a dropout risk).
    bool LastRandomizePartial() const { return lastRandomizePartial_.load(std::memory_order_acquire); }

    // Detected via AppConcepts.hpp's
    // HasProcessFrame concept; synth::Engine invokes this once per block,
    // after message drains and before ProcessBlock() (AppConcepts.hpp's own
    // comment on the hook's placement) -- exactly the audio-thread window
    // the pending-request atomics above need to be applied in.
    // Sheaf's optional revert hook (AppConcepts.hpp's HasRestoreStartupState),
    // invoked right after a patch revert has rebuilt every parameter from its
    // REGISTERED default.
    //
    // That rebuild is correct for everything registration can express, and
    // registration cannot express a modulation depth. This app's startup state
    // is not only registered defaults: ApplyFroggersDefaultPatch materializes
    // the Audio bank's cross-VCO pitch detents as real modulation depths
    // (FroggersModulation.hpp's ApplyAudioBankOverlay), and a revert zeroes
    // every depth and then reclaims the neutral ones, so those six parameters
    // stop existing and the three oscillators land in unison.
    //
    // Re-applies the SAME function a fresh launch runs, not a snapshot of what
    // it produced, so launch, Reset All and New cannot drift apart -- the one
    // definition each of them reaches is ApplyFroggersDefaultPatch.
    //
    // Reseeds afterwards for the reason the reset drain below documents: a
    // patch write leaves the computed values walking toward it over several
    // blocks, and depth children are never reached by the per-sample path at
    // all.
    void RestoreStartupState() {
        ApplyFroggersDefaultPatch(parameters_);
        if (context_ != nullptr && context_->parameterManager != nullptr) {
            context_->parameterManager->ComputeAllParameters();
        }
    }

    void ProcessFrame() {
        // Applies the most recent routed-input transition queued by Init()'s
        // callback (message thread), if any -- at most once per block, never
        // per sample, and always before ProcessBlock() reads it below.
        const int routedRequest = pendingExternalAudioRouted_.exchange(-1, std::memory_order_acq_rel);
        if (routedRequest >= 0) {
            modulation_.SetExternalAudioConnected(routedRequest != 0);
        }

        const int bankRequest = pendingBankSelect_.exchange(-1, std::memory_order_acq_rel);
        if (bankRequest >= 0 && static_cast<std::size_t>(bankRequest) < kFroggersBankCount) {
            if (static_cast<std::size_t>(bankRequest) != activeBankIx_) {
                activeBankIx_ = static_cast<std::size_t>(bankRequest);
                parameters_.Slot().SelectBank(&parameters_.BankAt(activeBankIx_));
                // `BankSlot::SelectBank` Deselect()s the OUTGOING bank
                // (src/ParameterModulation.cpp:2944-2951 in External/Sheaf), so
                // a freshly-constructed drillIn_ (level_ starts at 0) for the
                // INCOMING bank is always consistent with that bank's real
                // state: either it was never drilled into, or it was
                // Deselect()ed the last time it was left active -- both are
                // real level 0. This is why exactly one drillIn_ instance,
                // reconstructed on every switch, never desyncs from six
                // persistent per-bank instances would risk.
                drillIn_.emplace(parameters_.BankAt(activeBankIx_));
            } else if (drillIn_->Level() > 0) {
                // Clicking the bank you are ALREADY viewing must still be
                // able to back a modulation drilldown all the way out to
                // that bank's top-level parameter grid -- "clicking on the
                // page bank for the page we are on is the way the user
                // should always be able to get to that page, even when they
                // are in a modulation drilldown for a parameter on that
                // page." Back()-until-zero reaches the same "full
                // Deselect(), level 0" state a genuine bank switch produces
                // above, without reconstructing drillIn_ (same Bank&, no
                // need) -- bounded to at most 2 iterations (the level cap).
                // The `Level() > 0` guard is what keeps the pre-existing
                // no-op preserved for a same-bank click that is ALREADY at
                // level 0: nothing in this branch runs, so activeBankIx_/
                // drillIn_ are left completely undisturbed, same as before
                // this fix (rebuilding identical state would be wasted work
                // for no behavior change).
                while (drillIn_->Level() > 0) {
                    drillIn_->Back();
                }
            }
        }

        const int pressRequest = pendingEncoderPress_.exchange(-1, std::memory_order_acq_rel);
        if (pressRequest >= 0 && pressRequest < static_cast<int>(kFroggersSlotsPerBank)) {
            drillIn_->PressEncoder(static_cast<synth::PhysicalEncoderId>(pressRequest));
        }

        if (context_ != nullptr && context_->parameterManager != nullptr) {
            // Both branches below
            // return a `FroggersRandomizeResult` whose `.partial` flag used
            // to be discarded entirely -- captured into `anyPartial` and
            // published below instead.
            bool randomizeRan = false;
            bool anyPartial = false;
            // Each call's result is hoisted into its own named local
            // BEFORE combining, so both RandomizeAll/RandomizePage always run
            // when their request is pending -- `anyPartial = a.partial ||
            // anyPartial` reads fine but is only correct because the call
            // sits on the left of `||`; swapping the combine order (or a
            // future edit that does) would short-circuit and skip the second
            // call whenever the first's `.partial` was already true.
            if (pendingRandomizeAll_.exchange(false, std::memory_order_acq_rel)) {
                const bool allPartial = RandomizeAll(*context_->parameterManager, *drillIn_, parameters_, modulation_).partial;
                anyPartial = anyPartial || allPartial;
                randomizeRan = true;
            }
            if (pendingRandomizePage_.exchange(false, std::memory_order_acq_rel)) {
                const bool pagePartial = RandomizePage(*context_->parameterManager, *drillIn_).partial;
                anyPartial = anyPartial || pagePartial;
                randomizeRan = true;
            }
            // Reset drains here, beside Randomize, so the two are serviced on
            // the same audio-thread edge and in the same order every block.
            // Reset does not surface a partial/capacity outcome the way
            // Randomize does: it can materialize a handful of the Audio
            // bank's own default-patch depths (ResetBankToDefaultPatch ->
            // ApplyBankDefaultPatch -> EnsureModulationDepth), but never on a
            // scale where reporting a partial reset to the UI would be
            // meaningful -- see FroggersModulation.hpp's ResetPage/ResetAll.
            bool resetRan = false;
            if (pendingResetAll_.exchange(false, std::memory_order_acq_rel)) {
                ResetAll(*context_->parameterManager, *drillIn_, parameters_);
                resetRan = true;
            }
            if (pendingResetPage_.exchange(false, std::memory_order_acq_rel)) {
                ResetPage(*context_->parameterManager, *drillIn_, parameters_);
                resetRan = true;
            }
            if (randomizeRan) {
                // Surfaces a partial randomize -- observable to tests via
                // LastRandomizePartial(). Not a per-parameter/per-press
                // signal (the randomize helper can be called dozens of times
                // per operation); one publish per Randomize All/Page press
                // that actually ran short. No log is emitted here --
                // ProcessFrame() runs on the audio thread (this method's own
                // header comment), and any operator-visible logging must
                // instead read this atomic from the UI thread.
                lastRandomizePartial_.store(anyPartial, std::memory_order_release);
            }
            // ONE reseed covering both drains above, for two different reasons.
            //
            // Randomize: `Parameter::RandomizeVisibleValue` writes
            // `sceneCenters_` (the commanded value) directly and immediately,
            // but the drill-in knob reads `uiDisplayCenters_`, which a depth
            // parameter only ever gets seeded into via a smoothed, one-shot
            // nudge inside RandomizeVisibleValue itself (Sheaf, pinned). The
            // per-sample loop never touches it again, because depth parameters
            // are not in `topLevelParameters_`.
            //
            // Reset: it writes `sceneCenters_` and returns; the per-sample
            // path then WALKS the computed values there over several blocks
            // (~81% of the remaining distance per block -- see
            // FroggersAudioRoutingTests.cpp's own note on that rate). During
            // that walk the DSP is driven with values partway between what
            // Randomize drew and what Reset commanded, so a reset landing on a
            // later block than a randomize left the instrument audibly running
            // on the outgoing patch after it had been told to stop. Measured at
            // 84 parameters still differing at the reset block and converging
            // by 8 blocks. `ComputeAllParameters()` snaps current to target
            // outright, so the window never exists.
            //
            // Depth children make it permanent rather than merely slow: they
            // are not in `topLevelParameters_`, so the per-sample path never
            // reaches them at all and only this call ever reseeds them.
            //
            // Called ONCE here, after every branch above has made its writes
            // for this frame, never per-parameter and never from the UI
            // thread: ProcessFrame() only ever runs on the audio thread (this
            // method's own header comment; `synth::Engine` invokes it once per
            // block, after message drains and before ProcessBlock()), and
            // `ComputeAllParameters()` (public, ParameterModulation.hpp:809)
            // is a full, non-lock-free graph traversal that `ParameterManager`
            // requires to run there (ParameterModulation.hpp:484-485). It
            // reseeds every parameter including depth children -- ComputeAtDepth's
            // recursionDepth_>0 branch takes the instant snap-and-seed path,
            // not the smoothed one.
            if (randomizeRan || resetRan) {
                context_->parameterManager->ComputeAllParameters();
            }
        }

        const double tempoRequest = pendingTempoBpmRequest_.exchange(-1.0, std::memory_order_acq_rel);
        if (tempoRequest >= 0.0 && context_ != nullptr && context_->masterClock != nullptr) {
            context_->masterClock->SetTempoBpm(tempoRequest);
        }
    }

    // Drives the per-sample parameter-model (scene-blend, fuego), then steps
    // the DSP behind the 15 modulation sources every sample (this class's
    // modulation_ member) BEFORE parameters_.ProcessSample() so its
    // group_->UpdateModValues() call reads freshly-updated values through
    // the pointers registered in FroggersModulationSlate::Init. Routes the
    // now-resolved (post-fuego, post-modulation) parameter values into the
    // real DSP chain and sums it to the stereo output bus.
    void ProcessBlock(synth::AudioBlock& block) {
        // External audio (slots 13/14) connectedness is never derived from
        // `block.inputs` itself: whether a channel is present in the block
        // is a different question from whether the operator actually routed
        // something in, and this method never conflates the two. The
        // sources' `connected` state is driven entirely from the host's
        // routed-input signal (see Init()'s and ProcessFrame()'s own
        // comments, above). `block.inputs` IS read below, once per sample,
        // purely as the SIGNAL those sources step from while connected --
        // reached only through `AudioInputView` (channel 0, this app's own
        // `numAudioInputs == 1`), never as a second connectedness check.
        // `InputView()` is cheap (a plain struct built from already-clamped
        // fields, no per-call work) but still hoisted out of the per-sample
        // loop below, same as `hasOutputs` just under it, since both are
        // loop-invariant for the whole block.
        const synth::AudioInputView externalAudioInputView = block.InputView();

        // Source #6's tempo-following recompute
        // happens ONCE PER BLOCK, not per sample, from the block's clock-plan rate (independent of transport
        // running/stopped -- a rate, not a position). A null clock plan
        // falls back to a safe default inside PrepareBlockClock() itself.
        std::optional<double> quarterNotesPerSample;
        if (block.clockPlan != nullptr) {
            quarterNotesPerSample = block.clockPlan->QuarterNotesPerSample();
        }
        modulation_.PrepareBlockClock(quarterNotesPerSample);

        // `block.outputs` is `AudioBlock`'s own field
        // (External/Sheaf/projects/synth/include/synth/AppContext.hpp:183-211)
        // -- set once for the whole callback, never reassigned inside this
        // function -- so re-testing it every frame below was re-evaluating a
        // loop-invariant up to 48,000x/second. Hoisted here, once per block.
        const bool hasOutputs = block.outputs != nullptr;

        for (std::size_t frame = 0; frame < block.numFrames; ++frame) {
            const std::uint64_t absoluteOutputSample = block.startSample + frame;

            // The ASR gate follows
            // the master clock's transport quarter-note pulse -- see
            // TransportQuarterNotesAt()'s own comment for the citation chain
            // (apps/miniapp/MiniAppCore.hpp's gate idiom, adapted to the
            // containment-safe Try accessor). Gated open for the first half
            // of every quarter note, closed for the second half; closed
            // outright whenever the transport isn't running or the block has
            // no clock plan.
            const std::optional<double> transportQuarterNotes =
                TransportQuarterNotesAt(block, absoluteOutputSample);
            bool gateOpen = false;
            if (transportQuarterNotes.has_value()) {
                const double phase = *transportQuarterNotes - std::floor(*transportQuarterNotes);
                gateOpen = phase >= 0.0 && phase < 0.5;
            }
            // Measured in the built app: the drone "lasts only a few
            // seconds" if only the stop-edge teardown is suppressed -- that
            // alone is NOT sufficient to hold the drone, and this line is
            // why: the gate closes whenever the transport is not running, so engaging
            // Freeze -- which stops the transport (HandleAction's kFreeze
            // branch) -- let every voice release NATURALLY and reach Idle
            // within its release time, leaving only a decaying delay/reverb
            // tail. The teardown gate only ever skipped the FORCED release,
            // the stopped-state overrides and the buffer clear; it never
            // touched this.
            //
            // The accidental state this button reproduces was sustained by
            // voices that never reached Idle (curve~1 stuck envelopes)
            // CONTINUOUSLY exciting the chain -- that is what
            // made it a drone rather than a tail. A ramp bound elsewhere removed
            // that mechanism deliberately and permanently, so the latch has
            // to hold the voices open itself: while Freeze is engaged the
            // gate stays OPEN, voices sit at their sustain level, and the
            // chain keeps being fed for as long as the operator holds the
            // latch. Releasing it drops the gate back to the transport's own
            // answer (false while stopped), so the voices release and the
            // teardown silences the instrument -- the escape hatch is
            // unchanged.
            audioAdsr_.setGate(gateOpen || FreezeLatched());

            // Stop-transport reset (see wasTransportRunning_'s own comment):
            // `transportQuarterNotes.has_value()` above is already exactly
            // "transport running AND this sample is contained in a committed
            // plan" (TransportQuarterNotesAt()'s own comment), so the
            // running->stopped edge is read off that same already-computed
            // value rather than a second clock query. This runs on the
            // audio thread, inside the same per-sample loop that owns
            // delay_/reverb_ (RouteAudioSample(), below, is the one and only
            // caller of both), so there is no data race with the UI thread's
            // kStop handler (FroggersUiSurface.hpp's HandleAction), which
            // only pushes a MessageIn::Stop and never touches DSP state
            // directly. Resets every
            // unit ForEachStatefulUnit() enumerates (all 14 -- see that
            // method's own comment), not just delay_/reverb_. The comment
            // this replaces claimed "VCOs/filters/drive do not [self-
            // sustain]" as the reason to skip them; a direct measurement
            // found that claim false for the
            // comb (a recirculating delay line, 6.7s T60 at its longest
            // delay) and driveBlendPhase_ (a recursive allpass) -- both are
            // upstream of delay_/reverb_ and both have Reset(). The tag each
            // unit carries is ignored here on purpose: Stop is a hard
            // silence request, not a fault-recovery decision, so every unit
            // gets the same unconditional Reset() regardless of its
            // Recovery tier.
            const bool transportRunningNow = transportQuarterNotes.has_value();
            // DIAGNOSTIC: open the logging window on the Stop edge. The app
            // is the only place the bug reproduces -- two separate
            // measurements both found Stop working in the harness, and the
            // swept-knob one refuted the parametric hypothesis live -- so the remaining
            // question is which of these two the real failure is, and that can
            // only be read from a real session:
            //   pending stays true / AllIdle stays false -> the flush NEVER
            //     FIRES, and the bug is in the gate, not in what gets reset;
            //   flush fires (pending false) but peak stays up -> something
            //     REGENERATES after a full 14-unit reset with no input, and
            //     the only ungated energy source left is modulation.
            if (stopDiagEnabled_ && wasTransportRunning_ && !transportRunningNow) {
                stopDiagBlocks_ = kStopDiagBlocks;
            }
            // Releasing the Freeze
            // latch while the transport is already stopped is a SECOND
            // "stop edge" -- symmetric to the running->stopped edge just
            // below, keyed on the latch's own transition instead of the
            // transport's, and following the SAME idiom
            // (`wasTransportRunning_`'s edge check just below, and its own
            // comment) rather than inventing a new one. Read before
            // `wasFreezeLatched_` is updated at the bottom of this same
            // per-sample iteration, so this holds THIS sample's latch
            // state, not the previous one's.
            const bool freezeLatchedNow = FreezeLatched();
            const bool latchReleasedWhileStopped =
                wasFreezeLatched_ && !freezeLatchedNow && !transportRunningNow;
            // The stop-edge
            // teardown action itself, shared by BOTH edges capable of
            // firing it below (the running->stopped edge, and the latch-
            // released-while-stopped edge) so the two can never drift apart
            // in what "teardown" means. One lambda instance per sample,
            // cheap (no heap capture; `this` only).
            // Resets every stateful unit and clears the pending-clear flag --
            // the one clear this policy owes, shared by both the
            // already-idle-at-the-stop-edge site and the deferred-clear site
            // below so the two can never drift apart in what "clear" means.
            const auto clearDelayReverb = [this]() {
                ForEachStatefulUnit([](auto& unit, auto) { unit.Reset(); });
                delayReverbClearPending_ = false;
            };
            const auto runStopTeardown = [this, &clearDelayReverb]() {
                // Forces
                // every non-idle voice into Release right at the edge,
                // BEFORE the AllIdle() check below reads its post-edge
                // state. `setGate(gateOpen)` above already ran for this
                // sample and, on play-time gate-close, only marks
                // m_releasePending -- it does NOT itself move a voice out
                // of Attack/Decay/Hold, so without this call a voice mid-
                // Attack/Decay at the stop instant would sit there through
                // the Grace minimum-hold and/or stage completion (up to
                // ~9s at kMaxAttack/Decay/GraceSeconds each) before ever
                // reaching Release, let alone Idle -- exactly the
                // "Stop doesn't stop" symptom this app used to have.
                // ForceReleaseAll() (VoiceEnvelope.hpp) is
                // idempotent on an already-idle voice (leaves it Idle), so
                // this is safe to call unconditionally on every edge here,
                // including the "gate already closed, every voice already
                // released naturally" case the comment below describes --
                // in that case every voice is already Idle and this is a
                // no-op, so `audioAdsr_.AllIdle()` just below still reads
                // true exactly as before this call existed. In the more
                // common case (a voice non-idle at the edge), this call
                // moves it to Release, so AllIdle() below correctly reads
                // false and the deferred-clear branch (delayReverbClearPending_
                // = true) arms -- a forced-Release voice is NOT idle at the
                // edge, so it still needs its (now ~50ms, kStopFadeReleaseKnob
                // below) release tail to finish before delay_/reverb_ can be
                // cleared; that ordering is unchanged.
                audioAdsr_.ForceReleaseAll();
                // Revised from an old "clear every block
                // while releasing" policy: a releasing voice is still
                // musically live and still feeding delay_/reverb_
                // (RouteAudioSample(), below, runs every sample regardless
                // of transport) -- it is supposed to keep ringing through
                // them wet, exactly as it would if the transport were still
                // running. So this edge does NOT clear unconditionally.
                // `audioAdsr_.setGate(gateOpen)` above already ran for this
                // sample, so AllIdle() here already reflects the post-edge
                // stage: false if that gate transition just occurred on a
                // live voice (setGate(false) no longer
                // forces Stage::Release synchronously -- it marks
                // m_releasePending and stepVoice() resolves it, see
                // VoiceEnvelope.hpp's own comments -- but a voice that was
                // Attack/Decay/Hold the sample before this edge is still
                // Attack/Decay/Hold immediately after it, i.e. still not
                // Idle, so this branch's "not Idle" conclusion holds
                // regardless of which mechanism moves it toward Release),
                // true only if the gate was already closed *before* this
                // edge (e.g. the transport stopped during the closed half of
                // the ASR gate cycle) and every voice had already finished
                // its Release
                // naturally.
                if (audioAdsr_.AllIdle()) {
                    // Nothing is left to inject further energy into
                    // delay_/reverb_ (or anything upstream of them) -- gate
                    // closed, every voice Idle -- so one clear right here is
                    // permanent. This is the "already idle at the stop edge"
                    // case: with no future Release to wait for, the
                    // pending-clear-on-AllIdle logic below would never get a
                    // false->true transition to fire on, so the clear has to
                    // happen here instead.
                    clearDelayReverb();
                } else {
                    // Still releasing: defer. Arms the sample-accurate watch
                    // below for the one clear this policy owes, fired the
                    // instant AllIdle() first turns true while still
                    // stopped.
                    delayReverbClearPending_ = true;
                }
            };
            if (wasTransportRunning_ && !transportRunningNow) {
                if (TransportTeardownActive(transportRunningNow)) {
                    runStopTeardown();
                }
                // Else -- FreezeLatched() is true, so NONE of
                // runStopTeardown() above runs. The instrument holds its
                // sounding state instead of tearing down; see
                // TransportTeardownActive()'s own comment for why this is
                // the deliberate exception, not a regression.
            } else if (latchReleasedWhileStopped) {
                // The latch's own release, while already stopped, is
                // the escape hatch out of the drone the edge above just
                // held open -- run the exact same teardown that edge would
                // have run had the latch not suppressed it. (No
                // TransportTeardownActive() call needed here: this branch's
                // own condition already implies !transportRunningNow &&
                // !FreezeLatched(), i.e. the gate is true by construction.)
                runStopTeardown();
            } else if (delayReverbClearPending_) {
                if (transportRunningNow) {
                    // Transport resumed mid-release: this is now a
                    // legitimately playing delay/reverb, not a stale
                    // release tail -- cancel instead of wiping it.
                    delayReverbClearPending_ = false;
                } else if (audioAdsr_.AllIdle() && TransportTeardownActive(transportRunningNow)) {
                    // This is the deferred-clear site -- the easy miss this
                    // gate exists to cover. AllIdle() alone used to be
                    // sufficient here. It no longer is -- if the latch got
                    // engaged during this release-tail window (armed while
                    // unlatched, then latched before AllIdle() turned true),
                    // this must NOT clear; the instrument keeps holding
                    // until the latch itself releases (the branch above).
                    // The moment every voice reaches Idle while still
                    // stopped AND unlatched: the release can no longer
                    // inject anything into delay_/reverb_ (or anything
                    // upstream of them), so this single clear is permanent.
                    // Checked per-sample (cheap: AllIdle() is just three
                    // stage comparisons) so the expensive part (every
                    // unit's own Reset(), O(its own state size) each) still
                    // runs exactly once for the whole release, not once per
                    // block for up to kMaxReleaseSeconds (2.5s,
                    // VoiceEnvelope.hpp:84) the way the old policy did.
                    clearDelayReverb();
                }
            }
            wasTransportRunning_ = transportRunningNow;
            wasFreezeLatched_ = freezeLatchedNow;

            // VCO audio-rate modulation sources (slots 6-8) are
            // driven from each VCO's own Audio-bank pitch/shape/PM knobs --
            // the PREVIOUS sample's post-fuego cached value (this Step()
            // call runs before this sample's own fuego/modulation resolve;
            // see FroggersModulationSlate::Step's comment on the resulting
            // one-sample latency, standard for any modulation graph with a
            // cycle -- e.g. the cross-VCO pitch default).
            const auto vcoDrive = [this](std::size_t paramIx) {
                return FroggersModulationSlate::VcoDrive{
                    parameters_.PageParameter(FroggersBankId::Audio, AudioSlot(paramIx, VcoSlotRole::Pitch)).CachedKnobValue(0),
                    parameters_.PageParameter(FroggersBankId::Audio, AudioSlot(paramIx, VcoSlotRole::Shape)).CachedKnobValue(0),
                    parameters_.PageParameter(FroggersBankId::Audio, AudioSlot(paramIx, VcoSlotRole::PhaseMod)).CachedKnobValue(0),
                };
            };
            // Operator-ordered, not the DigitalReorganizer DC-seed fix --
            // that fix was a static DC seed inside DigitalReorganizer, a pure
            // function of frozen knob state (fixed separately); freezing
            // modulation could never have removed it. This gate is the operator's own ruling
            // instead, verbatim: "no, modulation should not free-run
            // while stopped lol. come on."
            //
            // Gated on `transportRunningNow`, already computed above (this
            // same `transportQuarterNotes.has_value()`) for the ASR gate and
            // the stop-edge reset logic -- reused here, not a second clock
            // query or null-check sequence. `parameters_.ProcessSample()`
            // below stays UNGATED on purpose: a SceneCenter write only
            // reaches the DSP through its own periodic smoothed Compute
            // (Parameter::ProcessSamplePhase1), so gating that call too would
            // freeze knob edits and Randomize All until Play. Only the
            // modulation SOURCES stop here.
            //
            // The genuinely free-running set this stops is 8 slots --
            // kModSlotRandomSh6, kModSlotVco1/2/3Audio, kModSlotVco1/2/3Ef,
            // kModSlotNoise -- every one written unconditionally inside
            // Step()'s own body. Random S&H lanes 1-5 are deliberately NOT
            // named here: StepClockDrivenLanes's own has_value() guard
            // already stops their Increment() regardless of this outer gate
            // (RandomShLane::Process() itself still glides a further few
            // samples toward an already-fixed target, then holds bit-exact --
            // a transient, not free-running; an earlier draft of this task
            // named the lanes here instead, which was wrong). Sources HOLD
            // rather than reset: RegisterSources() hands Sheaf raw pointers
            // to the member variables Step() writes, and
            // Modulators::UpdateModValues() (inside parameters_.
            // ProcessSample(), still called every sample below) dereferences
            // those same pointers regardless of this gate -- there is no
            // neutral-reset path, so skipping Step() simply leaves them at
            // whatever they last held. modulation_.PublishUiState(), once
            // per block after this loop, republishes that same held state,
            // so a visualizer shows a held value, not darkness.
            //
            // `transportQuarterNotes` is still passed into Step() below --
            // reused from the ASR gate's own
            // already-computed value rather than re-derived a second time,
            // for StepClockDrivenLanes's own tick-phase arithmetic -- see
            // Step()'s own comment.
            if (transportRunningNow) {
                // `SampleOrSilence(0, frame)` reads this frame's logical
                // channel zero -- the plugin has already resolved the
                // operator's channel/Sum selection down to that one channel
                // before this call ever sees the block (FroggersPluginProcessor::
                // processBlock()'s own comment); the standalone host's
                // device callback delivers its one requested channel there
                // directly. Silence (0.0f) whenever no channel is active,
                // matching this call's disconnected-source behavior anyway
                // (Step() ignores the sample entirely while
                // !ExternalAudioConnected()).
                const float externalAudioSample = externalAudioInputView.SampleOrSilence(0, frame);
                modulation_.Step(vcoDrive(0), vcoDrive(1), vcoDrive(2), transportQuarterNotes, externalAudioSample);
            }

            parameters_.ProcessSample(absoluteOutputSample);

            // Routes this sample's post-fuego, post-modulation
            // parameter values into the ported DSP chain and sums the (mono,
            // The chain's own output is a pair. Folding to the device happens
            // in the write loop below and only where the device is mono; the
            // middle of the chain no longer collapses, which is what lets the
            // delay's and the reverb's Width controls reach a listener.
            const dsp::StereoSample sample = RouteAudioSample();

            // DIAGNOSTIC (2026-08-07), OFF unless FROGG3RS_STOP_DIAG is set
            // in the environment -- see stopDiagBlocks_'s own comment. Tracks
            // this block's output peak only while the diagnostic window is
            // open, so a normal run pays one already-loaded bool test.
            if (stopDiagBlocks_ > 0) {
                stopDiagPeak_ = std::max(stopDiagPeak_,
                                         std::max(std::fabs(sample.l), std::fabs(sample.r)));
            }

            // Capture hook -- post-limiter, and still a mono buffer. The chain
            // is stereo now, so what is recorded is the same fold a mono
            // device gets rather than one channel of a pair, which would
            // silently drop half the signal from every recording.
            if (recordArmed_.load(std::memory_order_acquire) && transportRunningNow) {
                const std::uint64_t recordedSoFar = recordFrames_.load(std::memory_order_acquire);
                if (recordedSoFar < recordBuffer_.size()) {
                    recordBuffer_[recordedSoFar] = 0.5f * (sample.l + sample.r);
                    recordFrames_.store(recordedSoFar + 1, std::memory_order_release);
                } else {
                    recordTruncated_.store(true, std::memory_order_release);
                    recordArmed_.store(false, std::memory_order_release);
                }
            }

            if (hasOutputs) {
                // Unlike `block.outputs`
                // above, an individual `block.outputs[channelIx]` is NOT
                // provably non-null by contract. `AudioBlock::outputs` is
                // `float* const*` -- "Channel counts are the device's actual
                // counts" (AppContext.hpp:92-93) says nothing about every
                // slot in that count being populated, and Sheaf's own two
                // reference apps that consume this exact contract both guard
                // the identical way: `apps/braid-4/Braid4Core.hpp:678-689`
                // checks `block.outputs[0]`/`[1]`/`[channel] != nullptr`
                // individually even after already checking `block.outputs ==
                // nullptr`, and `apps/miniapp/MiniAppCore.hpp:358-363` does
                // `if (out == nullptr) { continue; }` per channel in the same
                // shape as here. Two independent call sites in Sheaf's own
                // codebase treating per-channel null as real is affirmative
                // evidence, not just an unproven possibility -- so this check
                // is real and stays, unlike the outer one above.
                // One channel is a mono device, and folding is what it needs:
                // the sum, not the left channel, or half the signal goes
                // missing. Two or more channels alternate L/R, so a stereo
                // device gets the image and a surround one gets the pair
                // repeated across its pairs rather than silence past the
                // second channel.
                const float monoFold = block.numOutputChannels == 1
                                           ? 0.5f * (sample.l + sample.r)
                                           : 0.0f;
                for (int channelIx = 0; channelIx < block.numOutputChannels; ++channelIx) {
                    float* const channel = block.outputs[static_cast<std::size_t>(channelIx)];
                    if (channel != nullptr) {
                        channel[frame] = block.numOutputChannels == 1
                                             ? monoFold
                                             : (channelIx % 2 == 0 ? sample.l : sample.r);
                    }
                }
            }

            // Moves the scope ring-buffer cursor once
            // per sample -- Sheaf's ScopeWriter needs both Write() (called
            // per-sample inside RouteAudioSample(), above, on the POST-gate
            // values -- moved off
            // dsp::Vco::Process() itself, see that struct's own comment) and
            // AdvanceIndex() (index_ += amount, DspScope.hpp:126-128).
            // Mirrors Braid 4's own placement: AdvanceIndex() runs at the
            // end of its per-sample work, after that sample's audio/matrix
            // outputs are computed and published but before the per-sample
            // function returns (Braid4Core.hpp:487, immediately preceding
            // RecordInternalIndex()+return). Here the equivalent slot is
            // the end of this per-frame loop's body, after this sample's
            // output has been computed and written.
            vcoScopeWriter_.AdvanceIndex();
        }

        // No once-per-block clearing step here anymore -- the single
        // clear this policy owes (fired either at the running->stopped edge
        // if voices were already Idle, or the instant AllIdle() first turns
        // true afterward) is now detected and performed sample-accurately
        // inside the per-sample loop above, at the same place the
        // running->stopped edge itself is detected. See that block's own
        // comment.

        // Tier 1/Tier 2 per-unit recovery: see
        // RecoverPoisonedUnitState()'s own header comment for the full
        // design/trace. Runs once per block, after the per-sample loop, over
        // every unit with a Reset() -- this IS the mechanism that
        // fixes "audio never comes back": before this call existed, nothing
        // anywhere in this file ever reset audioVcos_[0]/[1]/[2]/
        // driveBlendPhase_/drive_'s sub-units/filterChain_'s sub-units, so a
        // poisoned recursive state in any of them (e.g. a biquad whose y1/y2
        // went non-finite, or diverged to an extreme finite magnitude) would
        // keep feeding forward into every future sample forever, regardless
        // of what the operator changed the knobs to afterward -- only
        // SanitizeOutputSample's clamp/zero at the very end of the chain
        // masked the symptom, it never cleared the cause.
        RecoverPoisonedUnitState(block.numFrames);

        // DIAGNOSTIC: one line per block while the post-Stop window is
        // open. Once per BLOCK, never per sample, and entirely skipped unless
        // FROGG3RS_STOP_DIAG is set. It does write to stderr from the audio
        // thread, which is a real-time violation and exactly why it is
        // env-gated and not on by default -- it exists to catch a bug that has
        // never reproduced in the test harness.
        if (stopDiagBlocks_ > 0) {
            --stopDiagBlocks_;
            std::fprintf(stderr, "F3DIAG block=%d allIdle=%d clearPending=%d peak=%.9g\n",
                         kStopDiagBlocks - stopDiagBlocks_, audioAdsr_.AllIdle() ? 1 : 0,
                         delayReverbClearPending_ ? 1 : 0, static_cast<double>(stopDiagPeak_));
            stopDiagPeak_ = 0.0f;
        }

        // Publishes this block's UI-facing state, once per
        // block after the per-sample loop -- the same end-of-ProcessBlock
        // placement apps/braid-4's own ProcessBlock uses for its
        // scopeWriter_.Publish()/PopulateUIState()/PublishUiState() sequence
        // (Braid4Core.hpp:253-263).
        vcoScopeWriter_.Publish();
        audioVcos_[0].PopulateUIState(vco1ScopeUiState_);
        audioVcos_[1].PopulateUIState(vco2ScopeUiState_);
        audioVcos_[2].PopulateUIState(vco3ScopeUiState_);
        filterChain_.peak.PopulateUIState(peakUiState_);
        filterChain_.comb.PopulateUIState(combUiState_);
        modulation_.PublishUiState();

        // Publish the current drill-in level for the surface's header --
        // same once-per-block cross-thread publish shape as every other
        // display atomic in this section (see DrillLevel()'s own comment).
        drillLevelDisplay_.store(drillIn_->Level(), std::memory_order_release);

        // Publishes the master clock's active tempo/external-slave
        // state for the surface to read cross-thread (see this file's
        // header comment).
        if (context_ != nullptr && context_->masterClock != nullptr) {
            tempoDisplayBpm_.store(context_->masterClock->TempoBpm(), std::memory_order_release);
            tempoExternallyClocked_.store(context_->masterClock->SyncConfiguration().receiveClock,
                                          std::memory_order_release);
            // Same publish-once-per-block pattern as
            // the two stores above, for TransportRunning()'s cross-thread read.
            transportRunningDisplay_.store(
                context_->masterClock->TransportState() == synth::ClockTransportState::Running,
                std::memory_order_release);
        }
    }

    // Test/inspection access to the VCO scope plumbing.
    synth::ui::Visualizer& VcoScopeVisualizer() { return vcoScopeVisualizer_; }
    const dsp::Vco::UIState& VcoScopeUiState(std::size_t vcoIx) const {
        switch (vcoIx) {
            case 0: return vco1ScopeUiState_;
            case 1: return vco2ScopeUiState_;
            default: return vco3ScopeUiState_;
        }
    }

    // Test/inspection access to the transfer-function visualizers.
    synth::ui::Visualizer& PeakVisualizer() { return peakVisualizer_; }
    synth::ui::Visualizer& CombVisualizer() { return combVisualizer_; }

    // Test/inspection access to the parameter/bank model (also used
    // to wire fuego/modulation/UI).
    FroggersParameterModel& Parameters() { return parameters_; }

    // Test/inspection access to the modulation slate.
    FroggersModulationSlate& Modulation() { return modulation_; }

    // Test/inspection access to this class's own bank/drill-in bookkeeping.
    std::size_t ActiveBankIndex() const { return activeBankIx_; }
    FroggersModulationDrillIn& ActiveDrillIn() { return *drillIn_; }

    // Test/inspection access to tasks 2.2-2.5's per-unit recovery targets --
    // the exact dsp:: unit instances RecoverPoisonedUnitState() watches, so
    // tests can inject a poisoned (non-finite or over-ceiling) state
    // directly into ONE unit and observe (a) that unit's own recovery
    // firing through the real ProcessBlock()/RunBlocks() path and (b) every
    // OTHER unit's state staying untouched -- same "Test/inspection access"
    // convention as VcoScopeUiState()/PeakVisualizer() above.
    dsp::Vco& TestAudioVco(std::size_t ix) {
        switch (ix) {
            case 0: return audioVcos_[0];
            case 1: return audioVcos_[1];
            default: return audioVcos_[2];
        }
    }
    dsp::DriveBlendPhase& TestDriveBlendPhase() { return driveBlendPhase_; }
    // Read-only voice-state
    // probe, same convention as TestDelay()/TestReverb() -- measurement, not control.
    const dsp::VcoAdsrState& TestAudioAdsr() const { return audioAdsr_; }
    dsp::Oversampler2x& TestDriveOversampler() { return drive_.oversampler; }
    // Same convention as the accessors above -- added because a
    // silent-chain measurement case needs to read the live `flip`/
    // `hashBits` SetFlip()/SetHash() actually resolved to (Drive.hpp's
    // truncating/rounding casts), not assume the SceneCenter knob argument
    // maps the way the caller expects. Read-only; does not change behaviour.
    dsp::DigitalReorganizer& TestDriveDigitalReorganizer() { return drive_.digitalReorganizer; }
    dsp::SampleRateReducer& TestSampleRateReducer(std::size_t ix) {
        return ix == 0 ? drive_.sampleRateReducer1 : drive_.sampleRateReducer2;
    }
    dsp::ResonantBump& TestFilterPeak() { return filterChain_.peak; }
    dsp::ResonantBump& TestFilterScoopNotch() { return filterChain_.scoopNotch; }
    dsp::Comb& TestFilterComb() { return filterChain_.comb; }
    // Test/inspection access to the peak branch's OWN limiter instance
    // (`FilterFxChain::peakLimiter`, `dsp/FilterFx.hpp`) -- same convention
    // as `TestOutputLimiter()` above, but for the independently-tuned
    // second instance rather than the master.
    dsp::OutputLimiter& TestFilterPeakLimiter() { return filterChain_.peakLimiter; }
    // Test/inspection access to the MASTER output limiter, same
    // convention as the accessors above -- lets tests call Process()/
    // Reset() directly and read `envelope` without going through the full
    // RouteAudioSample chain, e.g. to prove the bit-identical-passthrough
    // acceptance test. `auto&` kept as-is after that change (`dsp::OutputLimiter` is
    // now a public type, so `dsp::OutputLimiter&` would spell fine too, but
    // every other TestXxx() accessor above already returns `dsp::Xxx&` by
    // deduction-free convention -- `auto&` here is simply consistent with
    // those, not a workaround for privacy anymore). Distinct from the peak
    // branch's OWN limiter instance, which has its own accessor
    // (`TestFilterPeakLimiter()`, mirroring `TestFilterPeak()` above).
    auto& TestOutputLimiter() { return outputLimiter_; }

    // Test/inspection access
    // to `delay_`/`reverb_`, same convention as the accessors above --
    // added because the Stop-flush measurement harness needs to read their
    // internal state magnitude directly (their delay lines/tanks, not just
    // the finiteness `RecoverIfNonFinite` already checks) to tell "cleared
    // once and stayed clear" apart from "cleared once, then refilled by a
    // still-ringing upstream unit." Read-only; does not change either
    // unit's behaviour.
    dsp::StereoDelay& TestDelay() { return delay_; }
    dsp::Reverb& TestReverb() { return reverb_; }

    // Test/inspection
    // access to the resolved Freeze-KNOB (dsp::DelayParams::dfrz) and
    // Reverb Tank-drive knob values RouteAudioSample() (below) last used,
    // same "TestXxx() convention read-only, measurement not control" as
    // TestDelay()/TestReverb() above. Neither value has a home on delay_/
    // reverb_ themselves to read back after the fact: `delayParams` is
    // RouteAudioSample()-local (constructed fresh every sample, see the
    // dfrzLatched comment below), and Reverb::Process computes `tankDrive`
    // from its knob argument as a purely local (dsp/Reverb.hpp,
    // TankDriveFromKnob) -- unlike Filter's comb.combDrive and Delay's own
    // fbDrive, which are members already reachable via TestFilterComb()/
    // TestDelay() above. Comb drive and Feedback drive tests read those
    // directly instead of adding parallel accessors here.
    float TestLastDelayFreezeKnobEffective() const { return lastDelayFreezeKnobEffective_; }
    float TestLastReverbTankDriveKnobEffective() const { return lastReverbTankDriveKnobEffective_; }
    // Same rationale as TestLastReverbTankDriveKnobEffective()
    // immediately above -- Reverb::Process computes `gritKnob01`'s use
    // (DigitalReorganizer::Mangle) as a purely local, no member to read
    // back after the fact.
    float TestLastReverbGritKnobEffective() const { return lastReverbGritKnobEffective_; }
    // The wet/dry mix actually handed to Reverb::Process, after the ceiling
    // this file applies to the knob. Same rationale as the two accessors
    // above: the ceiling is applied inline in the Process() argument list,
    // so nothing reads it back afterwards. A test asserting the ceiling
    // against this reads what the DSP was really given, rather than
    // restating the constant it was computed from.
    float TestLastReverbWetMixEffective() const { return lastReverbWetMixEffective_; }

    // The mapped Delay wet mix actually handed to MapRowsToDelayParams, read
    // the same way and for the same reason as the Reverb one above: a test
    // that asserted the constant would restate the edit rather than check it.
    float TestLastDelayWetMixEffective() const { return lastDelayWetMixEffective_; }

    // Test/inspection access to the actual `DelayParams::dfrzLatched`
    // value RouteAudioSample() (below) last handed to `delay_.Process()` --
    // same "TestXxx() convention" as the accessors above. `delayParams`
    // itself is a RouteAudioSample()-local (constructed fresh every sample),
    // so there is no other route for a test to confirm the wire at that call
    // site actually ran, as opposed to merely re-reading FreezeLatched()'s
    // own atomic a second time.

    // Test/inspection access to the live synth::ParameterManager, same
    // convention as TestOutputLimiter() above. `context_` (below) is
    // private and there is no other public route to it; this is the
    // narrowest accessor that reaches ComputeAllParameters(), which
    // FroggersAudioRoutingTests.cpp's ApplyPatchNow() needs to make a
    // SceneCenter write converge exactly before the first RunBlocks()
    // (the convergence's own settling rule).
    synth::ParameterManager& TestParameterManager() { return *context_->parameterManager; }

private:
    synth::AppContext* context_ = nullptr;

    // The single clock-read call
    // site for this class. Returns the transport quarter-note position at
    // `absoluteOutputSample`, or nullopt when the transport isn't running or
    // the committed plan doesn't contain the sample. Null-checking
    // `block.clockPlan` (`AppContext.hpp:197`) is necessary but not
    // sufficient for containment, so this calls the containment-safe
    // `TryTransportQuarterNotesAt` (`MasterClock.hpp:200`) rather than the
    // precondition-carrying `TransportQuarterNotesAt` (`:198`, precondition
    // `Contains(...)`, `:192-198`) -- the same shape
    // `apps/miniapp/MiniAppCore.hpp`'s own ADSR-gate idiom follows (guard
    // `:323-324`, phase derivation `:325-327`, duty-cycle assignment `:328`),
    // substituting the Try accessor for miniapp's unchecked one. Factored
    // into its own method (rather than inlined at the gate's one call site in
    // ProcessBlock) so the master-clock-driven Marbles advance
    // can share this exact read instead of re-deriving the
    // null-check/guard/Try-call sequence a second time.
    static std::optional<double> TransportQuarterNotesAt(const synth::AudioBlock& block,
                                                          std::uint64_t absoluteOutputSample) {
        if (block.clockPlan == nullptr ||
            block.clockPlan->TransportState() != synth::ClockTransportState::Running) {
            return std::nullopt;
        }
        return block.clockPlan->TryTransportQuarterNotesAt(static_cast<double>(absoluteOutputSample));
    }

    float RoutedKnob(FroggersBankId bank, std::size_t ix) {
        return parameters_.PageParameter(bank, ix).CachedKnobValue(0);
    }

    // Stop-must-stop fix (operator bug report 2026-07-29: "the stop button
    // doesnt actually stop all audio in all circumstances"). Closing the
    // transport gate puts every voice into Stage::Release honouring the
    // PATCH's release knob -- up to kMaxReleaseSeconds. So Stop used to
    // begin a multi-second fade during which the voices kept re-exciting
    // the delay and reverb, and ProcessBlock's clear-at-AllIdle could not
    // fire until that fade finished. Lowering kMaxReleaseSeconds does not
    // fix this; even 5s reads as "Stop is broken".
    //
    // While the transport is STOPPED, substitute a fast release. The
    // operator's own release setting is untouched and applies normally the
    // moment the transport runs again -- this only overrides the knob on
    // the stop path. ~50ms is short enough to read as immediate and long
    // enough to avoid a click; an instantaneous cut would click.
    //
    // Derived from VcoAdsrState's own mapping rather than hardcoded, so it
    // stays a true 50ms if kMaxReleaseSeconds/kMinReleaseSeconds are ever
    // retuned again. mapRelease is now dsp::ExpMapCompute (deliberate
    // parity divergence, VoiceEnvelope.hpp), so this is that map's
    // inverse:
    //   mapRelease(k) = min * (max/min)^k
    //   k = ln(target/min) / ln(max/min)
    // `static const`, not `constexpr`: std::log is not a constant
    // expression on this toolchain, so the inversion is computed once on
    // this function's first call (function-local static init, no data
    // race -- audio processing is single-threaded) rather than every
    // sample, unlike the old linear inverse this replaces, which the
    // compiler folded to a literal at compile time.
    //
    // `wasTransportRunning_` is written earlier in this same per-sample
    // iteration (ProcessBlock's edge check, above this call), so it holds
    // THIS sample's transport state, not the previous one's.
    //
    // While the transport is
    // STOPPED, override effective knob values for the release knob
    // (releaseKnob, below -- already existed) PLUS the three drive
    // pre-gains (Delay slot 9 "Feedback drive", Reverb slot 10 "Tank
    // drive", Filter slot 7 "Comb drive" -- each maps through the SAME
    // ExpMapCompute(0.25,4,knob) idiom, unity at knob==0.5, see each
    // unit's own SetFeedbackDrive()/TankDriveFromKnob()/this file's
    // Filter-slot-7 comment for the citation) and Freeze (Delay slot
    // 5, dsp::DelayParams::dfrz, resolves to 0.0f) -- WITHOUT writing to
    // the parameter model, exactly as kStopFadeReleaseKnob's release
    // override above never writes the Release knob. Play resumes
    // bit-identical because the commanded value was never touched: the
    // instant `wasTransportRunning_` is true again, `stoppedKnob` below
    // just returns `knob(bank, slot)` unchanged.
    //
    // Reverb's Grit (slot 11) joins
    // the same list -- a third mechanism found by direct measurement.
    // With W1+W2 in place the pass-D 20-trial repro
    // still reproduced 20/20 -- measured cause: Grit's stage
    // (dsp::DigitalReorganizer::Mangle, dsp/Drive.hpp:363-382) sits
    // INSIDE the tank feedback path ahead of the loop's own
    // PadeSaturator::Saturate bound, and Mangle's 8-bit-bucket
    // quantize-then-XOR has UNBOUNDED local gain across a bucket
    // boundary, so at Hold-high (fb~=0.999) the ordinary sub-audible
    // release tail gets re-amplified every round trip into a
    // saturator-bounded but never-decaying limit cycle (control,
    // isolated dsp::Reverb: one 0.01 seed then exact zero for 6.25s --
    // locks at 0.306814 forever at the drawn Grit 0.8094; decays to
    // 1.98e-7 at Grit 0). Grit 0.0f is its own exact bit-identical
    // bypass by construction (Mangle(x,0,0) - Mangle(0,0,0) == x,
    // dsp/Reverb.hpp:526-527), so this override is silent by
    // construction too, same as the others.
    //
    // One shared lookup, not a sixth independent ternary --
    // `stoppedKnob(bank, slot, stoppedValue)` generalizes the
    // `wasTransportRunning_ ? knob(...) : override` shape the old
    // inlined `releaseKnob` used to hardcode to any (bank, slot,
    // override) triple, and `releaseKnob` itself is now defined in
    // terms of it below, so all SIX stopped-state overrides (release
    // x3 voices [already one call site via releaseKnob's own slot
    // argument], the three drive pre-gains, Freeze, Grit) render
    // through this ONE lambda instance.
    //
    // The condition below reads
    // TransportTeardownActive(wasTransportRunning_) -- the SAME gate
    // ForceReleaseAll() and both ForEachStatefulUnit(Reset) clears read
    // in ProcessBlock() above -- instead of the bare `wasTransportRunning_`
    // this used to test. `wasTransportRunning_` is current for THIS
    // sample already (ProcessBlock's edge-detect block updates it
    // before RouteAudioSample() runs, see that member's own comment),
    // so passing it through is exactly "is the transport actually
    // running", and the gate itself folds in "...or held open by the
    // latch": while FreezeLatched() is true and the transport is
    // stopped, these six overrides stay OFF (return the live knob, as
    // if still running) so the drone's above-unity drives and nonzero
    // Grit survive the stop instead of being forced to their
    // bypass/unity values.
    static constexpr float kStopUnityDriveKnob = 0.5f;  // ExpMapCompute(0.25,4,0.5) == 1.0 (unity) -- shared by all three drive pre-gains.
    static constexpr float kStopFreezeKnob = 0.0f;
    static constexpr float kStopGritKnob = 0.0f;  // Mangle(x,0,0) - Mangle(0,0,0) == x (dsp/Reverb.hpp:526-527) -- exact bit-identical bypass, same as Grit's own registered default.
    // The dry floor shared by Delay's and Reverb's wet/dry crossfades is
    // `dsp::kMinDryLevel` (dsp/Limiter.hpp) -- see that declaration for the
    // equal-power law it feeds and the guarantee it makes. It is applied
    // inside `dsp::StereoDelay::ToStereo`/`dsp::Reverb::Process`, AFTER
    // `WetAuthority()` has scaled the routed knob there, not here: this
    // class only routes the raw 0..1 knob down to those two calls (see
    // RouteDelayBank/RouteReverbBank below).

    float StoppedKnob(FroggersBankId bank, std::size_t slot, float stoppedValue) {
        return TransportTeardownActive(wasTransportRunning_) ? stoppedValue : RoutedKnob(bank, slot);
    }

    float ReleaseKnob(std::size_t slot) {
        constexpr float kStopFadeSeconds = 0.05f;
        static const float kStopFadeReleaseKnob =
            std::log(kStopFadeSeconds / dsp::VcoAdsrState::kMinReleaseSeconds) /
            std::log(dsp::VcoAdsrState::kMaxReleaseSeconds / dsp::VcoAdsrState::kMinReleaseSeconds);
        return StoppedKnob(FroggersBankId::Envelope, slot, kStopFadeReleaseKnob);
    }

    // -- Audio bank -> 3x dsp::Vco ---------------------------
    // Slots: VCO pitch 0-2, Shape (morph) 3-5, Phase mod 6-8 -- same
    // (paramIx, +3, +6) grouping ProcessBlock's own vcoDrive lambda uses
    // above for the modulation slate's separate VCO instances.
    //
    // Collapsed from three structurally identical statements
    // (audioVco1_/2_/3_, differing only by index 0,3,6 / 1,4,7 / 2,5,8)
    // into a loop over audioVcos_. Per-index order is preserved exactly
    // (i=0 is the old audioVco1_/slots 0,3,6; i=1 is audioVco2_/slots
    // 1,4,7; i=2 is audioVco3_/slots 2,5,8), and each dsp::Vco instance's
    // Process() call only ever reads its OWN persistent state
    // (carrierPhase/pmLfoPhase) plus this call's own arguments -- no
    // cross-VCO term exists (Vco.hpp's own header comment) -- so the
    // iteration order across i cannot change any instance's output.
    // Ring Mod slots 9-11 + PM rate slot 12:
    // slot 12 (PMrt) is read ONCE here and passed identically to all
    // three Process() calls -- deliberately "ONE knob shared
    // across all three VCOs' StepPmLfo calls, not three per-VCO rates".
    std::array<float, 3> RouteAudioBank() {
        const float pmRateKnob = RoutedKnob(FroggersBankId::Audio, 12);
        std::array<float, 3> vcoOut{};
        for (std::size_t i = 0; i < audioVcos_.size(); ++i) {
            vcoOut[i] = audioVcos_[i].Process(RoutedKnob(FroggersBankId::Audio, AudioSlot(i, VcoSlotRole::Pitch)),
                                               RoutedKnob(FroggersBankId::Audio, AudioSlot(i, VcoSlotRole::Shape)),
                                               RoutedKnob(FroggersBankId::Audio, AudioSlot(i, VcoSlotRole::PhaseMod)),
                                               pmRateKnob,
                                               RoutedKnob(FroggersBankId::Audio, AudioSlot(i, VcoSlotRole::RingMod)),
                                               sampleRate_);
        }
        return vcoOut;
    }

    // -- Envelope bank -> ASR + voice mix --------------------
    // Slots: Attack/Sustain/Release x VCO1-3, 0-8 in that order --
    // matches dsp::MixOscVoices's parameter order exactly.
    //
    // `gatedVoices` receives the POST-gate per-voice values via
    // MixOscVoices's out-parameter (VoiceEnvelope.hpp) -- written to the
    // scope just below instead of the pre-gate raw VCO output
    // Vco::Process() used to write (see that struct's own comment for
    // why). This is the single call site the out-parameter was added
    // for; do not re-apply `adsr.apply` anywhere else.
    float RouteEnvelopeBank(std::array<float, 3> vcoOut) {
        dsp::GatedVoices gatedVoices;
        // Envelope bank slots are interleaved ADSR, slot = 4*vco +
        // {0:Attack, 1:Decay, 2:Sustain, 3:Release} (FroggersParameters.hpp's
        // Envelope layout), plus slot 12 Curve and slot 13 Grace, shared
        // across all three voices. MixOscVoices now reads Decay (slots
        // 1/5/9), Curve (slot 12) and Grace (slot 13) too -- see
        // VoiceEnvelope.hpp's own task A/B/C comments for the ramp-shape and
        // deferred-release mechanisms these drive.
        // Task C: Audio slot 13 (VBal) drives the mixed-average's crossfade
        // weights (dsp::ComputeVcoBalanceWeights); outGated's raw per-voice
        // values (scope tap below) are unaffected by balance.
        const float chainIn = dsp::MixOscVoices(
            audioAdsr_, vcoOut[0], vcoOut[1], vcoOut[2],
            RoutedKnob(FroggersBankId::Envelope, 0), RoutedKnob(FroggersBankId::Envelope, 1), RoutedKnob(FroggersBankId::Envelope, 2), ReleaseKnob(3),
            RoutedKnob(FroggersBankId::Envelope, 4), RoutedKnob(FroggersBankId::Envelope, 5), RoutedKnob(FroggersBankId::Envelope, 6), ReleaseKnob(7),
            RoutedKnob(FroggersBankId::Envelope, 8), RoutedKnob(FroggersBankId::Envelope, 9), RoutedKnob(FroggersBankId::Envelope, 10), ReleaseKnob(11),
            RoutedKnob(FroggersBankId::Envelope, 12), RoutedKnob(FroggersBankId::Envelope, 13),
            RoutedKnob(FroggersBankId::Audio, 13), &gatedVoices);

        // Post-gate scope tap: writes the GATED values, not
        // the raw pre-gate VCO output -- so the scope stays flat until the
        // transport's ASR gate has actually opened at least one voice.
        vco1ScopeHolder_.Write(gatedVoices.v1);
        vco2ScopeHolder_.Write(gatedVoices.v2);
        vco3ScopeHolder_.Write(gatedVoices.v3);

        return chainIn;
    }

    // -- Drive bank -> dsp::FrogBlock + DriveBlendPhase ------
    // FroggersEngine.hpp:290-297 order: Gain (SetGain) before Shape
    // (SetCoefs, which reads the just-set gain target -- Drive.hpp's own
    // comment), then SRR1/SRR2/XOR/BitDepth/Fuzz; Wet/Dry/Phase (slots 0, 8)
    // are the authored DriveBlendPhase stage, crossfading dry (chainIn)
    // against wet (FrogBlock's output).
    float RouteDriveBank(float chainIn) {
        drive_.polynomialDrive.SetGain(RoutedKnob(FroggersBankId::Drive, 1));
        // Link (Drive slot 10) must precede SetCoefs, which reads `link`.
        drive_.polynomialDrive.SetLink(RoutedKnob(FroggersBankId::Drive, 10));
        drive_.polynomialDrive.SetCoefs(RoutedKnob(FroggersBankId::Drive, 2));
        drive_.sampleRateReducer1.SetFreq(
            1e-2f + dsp::ZeroedExpCompute(10.0f, 1.0f - RoutedKnob(FroggersBankId::Drive, 3)));
        drive_.sampleRateReducer2.SetFreq(
            1e-2f + dsp::ZeroedExpCompute(10.0f, 1.0f - RoutedKnob(FroggersBankId::Drive, 4)));
        drive_.digitalReorganizer.SetFlip(RoutedKnob(FroggersBankId::Drive, 5));
        drive_.digitalReorganizer.SetHash(RoutedKnob(FroggersBankId::Drive, 6));
        drive_.fuzz = RoutedKnob(FroggersBankId::Drive, 7);
        // -- Drive slots 9, 11-13 -----
        drive_.oversampler.SetAntiAliasBrightness(RoutedKnob(FroggersBankId::Drive, 9));
        drive_.SetFold(RoutedKnob(FroggersBankId::Drive, 11));
        drive_.SetTone(RoutedKnob(FroggersBankId::Drive, 12));
        drive_.SetBias(RoutedKnob(FroggersBankId::Drive, 13));
        const float driveWet = drive_.Process(chainIn);
        // Wet/Dry carries no dry floor -- unlike Delay's and Reverb's wet
        // controls, this crossfade is allowed to reach fully wet (see
        // dsp::kMinDryLevel's own comment, dsp/Limiter.hpp, for why the
        // other two pages are floored and this one is not).
        const float driveOut = driveBlendPhase_.Process(
            chainIn, driveWet, RoutedKnob(FroggersBankId::Drive, 0), RoutedKnob(FroggersBankId::Drive, 8));
        return driveOut;
    }

    // -- Filter bank -> dsp::FilterFxChain -------------------
    // FroggersEngine.hpp:272,274-288 mapping (Comb offset -> pureDelay,
    // Peak freq/gain/Q -> ResonantBump, Comb delay/feedback/LP -> Comb,
    // Comb/Peak -> blend, Scoop -> scoopMix). The old `useParallel` bool
    // (which used to mirror `SetUseV2FilterParallel(UsesV2Fuego(hostKind))`,
    // always `true` for this app) has
    // been replaced by the Filter slot-13 "Topology" knob, a continuous
    // morph -- see FilterFxChain::Process's own comment (FilterFx.hpp).
    // Clamped to `PureDelay::kSize` so a very high host sample rate
    // cannot push `SetDelaySeconds` past the ported unit's fixed-size
    // ring buffer (defensive; PureDelay itself is not
    // modified).
    float RouteFilterBank(float driveOut) {
        const float combOffsetSeconds = std::min(
            dsp::ExpMapCompute(0.001f, 0.1f, RoutedKnob(FroggersBankId::Filter, 3)),
            static_cast<float>(dsp::PureDelay::kSize - 1) / sampleRate_);
        filterChain_.pureDelay.SetDelaySeconds(combOffsetSeconds, sampleRate_);
        // 08b5fd3:src/core/FroggersEngine.hpp:561-562: the scoop notch shares the peak bump's
        // freq and width verbatim -- hoisted into locals because BOTH
        // ResonantBumps consume them.
        // Floor raised from 20 Hz to 100 Hz: this is a peak, not a cutoff
        // (traced against ResonantBump above), so a low draw idled below
        // audibility rather than merely softening it -- inert, not tame. The
        // knob is a modulation TARGET (see this function's own resonant-peak
        // ceiling comment below), so a randomized depth visits this floor
        // regularly, not only when an operator dials there. Moves the
        // bottom decile from about 40 Hz to about 170 Hz.
        const float bumpFreq =
            dsp::ExpMapCompute(100.0f / sampleRate_, 20000.0f / sampleRate_, RoutedKnob(FroggersBankId::Filter, 0));
        // Floor raised from 0.1 to 0.4: at Q 0.4 the RBJ peaking biquad's
        // bandwidth (ResonantBump::UpdateCoefficients's `alpha =
        // sin(w0)/(2*Q)`, FilterFx.hpp) is about 3 octaves -- broad, but
        // still a discernible peak, not the flat pass-through the old
        // floor could sit at.
        const float bumpWidth = dsp::ExpMapCompute(0.4f, 10.0f, RoutedKnob(FroggersBankId::Filter, 2));
        filterChain_.peak.SetFreq(bumpFreq);
        // Ceiling lowered
        // from 10x (+20 dB) to 4x (+12 dB). An audible resonant peak does
        // not need a 20 dB multiplier sitting on top of a comb that (with
        // its feedback now bounded below unity) can still ring for seconds -- 10x was the primary gain
        // offender in the operator's blowout (multiplying the comb's
        // saturator-pinned output before the
        // limiter). scoopNotch (below) has its own independent freq/width
        // knobs (Filter slots 9/10) but its own height is a DIP
        // (max(0.05, 1-0.95*scoop)), not a gain, so it is unaffected and
        // untouched.
        // Ceiling 4.0f (+12 dB), NOT the firmware's 10.0f
        // (+20 dB). A pinned self-oscillating comb through a 20 dB peak is what put
        // ~20x full scale into the output stage. Deliberate divergence from the port
        // source -- parity deprioritised by operator decision 2026-07-28.
        // Lowered again 2026-07-29, operator on hearing it: 4x "is still too
        // harsh when modulated/randomized ... gets very close to blowout
        // territory anyway". 10x -> 4x fixed the gross overload; 4x -> 2x
        // targets the harshness that remains.
        //
        // Why modulation is the case that matters: the knob is a modulation
        // TARGET, so a randomized depth sweeps it to maximum regularly rather
        // than only when the operator dials it there. The comb feeding this
        // stage is bounded near |in| + 0.95 (~2 at full scale), so 4x handed
        // the output stage ~8 -- about 9x over the limiter's 0.9 threshold,
        // which means the limiter rides hard and continuously, and heavy
        // sustained gain reduction is itself the harshness. 2x (+6 dB) roughly
        // halves how hard it has to work while still being an audible
        // resonant peak.
        //
        // If it is STILL harsh, the next lever is the comb feedback (0.95)
        // that feeds it, not this ceiling -- past a point, lowering this just
        // makes the resonance inaudible.
        filterChain_.peak.SetHeight(
            dsp::ExpMapCompute(1.0f, dsp::kMaxResonantBumpHeight, RoutedKnob(FroggersBankId::Filter, 1)));
        filterChain_.peak.SetWidth(bumpWidth);
        // 08b5fd3:src/core/FroggersEngine.hpp:561-564, restored 2026-07-27. These three
        // setters were dropped in the port even though `FilterFxChain`
        // kept the scoop blend, so `scoopNotch` ran forever on
        // `ResonantBump`'s default `freq = 1000.0f` (FilterFx.hpp:133) --
        // an unnormalized value in a cycles/sample convention, i.e.
        // thousands of times past Nyquist, giving a marginally stable
        // biquad. Under a self-oscillating comb (Randomize All pins the
        // feedback near its -0.95 extreme) its state diverged to non-finite,
        // and `SanitizeOutputSample` then masked every later sample to
        // 0.0f: permanent silence with no recovery. Note `scoopNotch`
        // processes unconditionally, so a poisoned state reaches the output
        // even at `scoopMix == 0` -- IEEE `NaN * 0` is `NaN`.
        // Height is a DIP, not a gain: max(0.05, 1 - 0.95 * scoop).
        // Filter slots 9/10 ("Scoop freq"/"Scoop width",
        // "ScFq"/"ScWd"): the scoop notch used to share the peak bump's
        // freq/width verbatim (bumpFreq/bumpWidth above); it now gets its
        // own independent knobs, mapped with the SAME shape (same
        // ExpMapCompute min/max) so the two controls behave consistently
        // with one another.
        // Floor raised from 20 Hz to 100 Hz, same reasoning as bumpFreq's own
        // comment above (an independent knob, same inert-below-audibility
        // failure). Moves the bottom decile from about 40 Hz to about
        // 170 Hz.
        const float scoopFreq =
            dsp::ExpMapCompute(100.0f / sampleRate_, 20000.0f / sampleRate_, RoutedKnob(FroggersBankId::Filter, 9));
        // Same floor raise (0.1 -> 0.4) and reasoning as bumpWidth's own
        // comment above.
        const float scoopWidth = dsp::ExpMapCompute(0.4f, 10.0f, RoutedKnob(FroggersBankId::Filter, 10));
        filterChain_.scoopNotch.SetFreq(scoopFreq);
        filterChain_.scoopNotch.SetWidth(scoopWidth);
        // Deliberate decreasing exponential map (base 0.05 < 1 is
        // well-defined): equal knob steps cut equal dB, running from an
        // exact 1.0 (no dip) at the floor to an exact 0.05 at the ceiling.
        // Scoop depth (Filter slot 11) drives the notch's own dip;
        // how much of that dip reaches the output is Scoop (slot 8, the
        // wet/dry blend below).
        filterChain_.scoopNotch.SetHeight(
            dsp::ExpMapCompute(1.0f, 0.05f, RoutedKnob(FroggersBankId::Filter, 11)));
        // Floor raised from 20 Hz to 100 Hz, same inert-below-audibility
        // reasoning as bumpFreq/scoopFreq above. Ceiling here is 10 kHz, not
        // 20 kHz, so this moves the bottom decile from about 37 Hz to about
        // 159 Hz. NOTE this floor also raises the comb low-pass knob's own
        // floor below (cmlp derives its floor as 4x this frequency) from
        // 80 Hz to 400 Hz at minimum comb frequency -- a range change to a
        // control nobody asked to change, accepted on the same reasoning (an
        // 80 Hz low-pass on a comb is inaudible) but stated here so the
        // coupling is not a surprise later.
        const float combFreq =
            dsp::ExpMapCompute(100.0f / sampleRate_, 10000.0f / sampleRate_, RoutedKnob(FroggersBankId::Filter, 4));
        // Fractional now (Comb::delaySamples, FilterFx.hpp) -- no truncating
        // cast here; the clamp keeps the same [1, kSize-1] bounds as floats.
        filterChain_.comb.delaySamples =
            std::min(static_cast<float>(dsp::Comb::kSize - 1), std::max(1.0f, dsp::Comb::GetDelaySamples(combFreq)));
        filterChain_.comb.SetFeedback(dsp::Comb::GetFeedback(RoutedKnob(FroggersBankId::Filter, 5)));
        // Comb low-pass floor derives from combFreq above (4x it), so it
        // moved too when combFreq's own floor was raised -- see combFreq's
        // comment just above. The low-pass runs from just above the comb's
        // own pitch up to fully open, so the floor is clamped to the
        // ceiling: once the comb sits high enough that 4x its frequency is
        // already past fully open, the range degenerates to fully open --
        // the knob goes inert there, it never reverses direction.
        const float cmlpCeiling = 20000.0f / sampleRate_;
        const float cmlp = dsp::ExpMapCompute(std::min(4.0f * combFreq, cmlpCeiling), cmlpCeiling,
                                               RoutedKnob(FroggersBankId::Filter, 6));
        // FroggersEngine.hpp:245-247 (Alpha): 1 - exp(-2*pi*natFreq).
        filterChain_.comb.SetCutoffAlpha(1.0f - std::exp(-2.0f * static_cast<float>(M_PI) * cmlp));
        // Task C (Filter slot 7, "Comb drive", "CDrv"): knob-driven
        // pre-gain on the comb saturator's argument (dsp::Comb::Process,
        // FilterFx.hpp), exponential map (dsp::ExpMapCompute, same shape
        // used throughout this method), range 0.25x-4.0x so unity (1.0x)
        // is reachable exactly at knob==0.5 -- ExpMapCompute(0.25, 4.0,
        // 0.5) == 0.25 * sqrt(16) == 1.0. The Filter slot-7 default
        // (FroggersParameters.hpp) is set to 0.5f for exactly this reason
        // (Task E).
        // stoppedKnob (defined above, beside kStopFadeReleaseKnob)
        // overrides this to kStopUnityDriveKnob (0.5, i.e. unity post-map)
        // while the transport is stopped, regardless of the commanded knob.
        filterChain_.comb.SetDrive(
            dsp::ExpMapCompute(0.25f, 4.0f, StoppedKnob(FroggersBankId::Filter, 7, kStopUnityDriveKnob)));
        // Task A (Filter slot 13, "Topology", "Topo"): continuous morph
        // replacing the old `useParallel` bool -- see FilterFxChain::Process's
        // own comment (FilterFx.hpp) for why topology==0 is bit-identical to
        // the old always-true `useParallel` behaviour.
        // Scoop (Filter slot 8) is the notch's wet/dry blend into the
        // chain's shared input, ahead of both the Comb and Peak stages
        // (FilterFxChain::Process, FilterFx.hpp) -- SetHeight above reads
        // Scoop depth (slot 11) instead, the notch's own dip.
        const float filterOut =
            filterChain_.Process(driveOut, RoutedKnob(FroggersBankId::Filter, 13), RoutedKnob(FroggersBankId::Filter, 12),
                                  RoutedKnob(FroggersBankId::Filter, 8));
        return filterOut;
    }

    // -- Delay bank -> dsp::StereoDelay ---------------------
    // Positioned exactly where the firmware engine's `m_simFxInsert` hook
    // sits: FroggersEngine.hpp:595-598, between the filter chain
    // (08b5fd3:src/core/FroggersEngine.hpp:824-839) and Reverb (FroggersEngine.hpp:599-618) -- confirmed by the retired
    // simulator's `WasmSimHost`, which wired this exact ported unit's
    // frozen counterpart (`DelayState::processInsert`) into that hook.
    // `processInsert`'s own shape is `delay.process(bumpIn, params)` then
    // `delay.toReverbMono(bumpIn, wet, params.dmix)`, reproduced
    // identically below.
    // The Freeze KNOB (Delay bank slot 5, dsp::DelayParams::dfrz) goes
    // through stoppedKnob too -- kStopFreezeKnob (0.0f) while stopped,
    // regardless of the commanded knob -- distinct from the Freeze
    // BUTTON's latch (dfrzLatched, set just below: that latch stays a no-op
    // on the audio while stopped, since this override already zeroes dfrz
    // and every voice is forced into Release, so FreezeFeedback's
    // above-unity overdrive has nothing
    // left feeding it). lastDelayFreezeKnobEffective_ records the
    // resolved value for TestLastDelayFreezeKnobEffective() -- dfrz has
    // no other test-readable home since `delayParams` is local to this
    // function, freshly constructed every sample (this file's own
    // dfrzLatched comment, below).
    dsp::StereoSample RouteDelayBank(float filterOut) {
        const float delayFreezeKnobEffective = StoppedKnob(FroggersBankId::Delay, 5, kStopFreezeKnob);
        lastDelayFreezeKnobEffective_ = delayFreezeKnobEffective;
        // The routed knob, passed down unscaled: dsp::kMinDryLevel's dry
        // floor (dsp/Limiter.hpp, same constant the Reverb bank uses below)
        // is applied inside dsp::StereoDelay::ToStereo, to theta AFTER
        // WetAuthority() has scaled this value there -- not here, and not
        // by multiplying the knob. Delay needs a second protection Reverb
        // does not: its wet path is fed only through Send, which defaults
        // to zero, so ToStereo also scales the mix by what that path
        // actually holds. The dry floor bounds a fed path; the authority
        // handles an unfed one.
        const float delayWetMixEffective = RoutedKnob(FroggersBankId::Delay, 0);
        lastDelayWetMixEffective_ = delayWetMixEffective;
        dsp::DelayParams delayParams = dsp::MapRowsToDelayParams(
            RoutedKnob(FroggersBankId::Delay, 2), RoutedKnob(FroggersBankId::Delay, 1), RoutedKnob(FroggersBankId::Delay, 3),
            RoutedKnob(FroggersBankId::Delay, 4), delayFreezeKnobEffective, RoutedKnob(FroggersBankId::Delay, 6),
            delayWetMixEffective, RoutedKnob(FroggersBankId::Delay, 7), RoutedKnob(FroggersBankId::Delay, 8));
        // The Freeze BUTTON's override, applied where the
        // freeze mapping resolves the encoder's value -- MapRowsToDelayParams
        // just above sets `dfrz` from the clamped encoder alone and
        // never touches `dfrzLatched`, which dsp::StereoDelay::Process()
        // (dsp/Delay.hpp) already honours as a SINK but nothing
        // else ever sets as a SOURCE. Not folded into
        // MapRowsToDelayParams itself: dfrzLatched is not a row (this
        // file's own DelayParams::dfrzLatched comment).
        delayParams.dfrzLatched = FreezeLatched();
        // -- Delay slots 9-13 ---------------
        // stoppedKnob overrides Feedback drive to kStopUnityDriveKnob
        // while stopped, same idiom as the Filter comb drive above.
        delay_.SetFeedbackDrive(StoppedKnob(FroggersBankId::Delay, 9, kStopUnityDriveKnob));
        delay_.SetFeedbackTone(RoutedKnob(FroggersBankId::Delay, 10));
        delay_.SetModRate(RoutedKnob(FroggersBankId::Delay, 11));
        delay_.SetWidthBalance(RoutedKnob(FroggersBankId::Delay, 12));
        delay_.SetCrush(RoutedKnob(FroggersBankId::Delay, 13));
        const dsp::DelayWetPair delayWet = delay_.Process(filterOut, delayParams);
        const dsp::StereoSample delayOut = delay_.ToStereo(filterOut, delayWet, delayParams.dmix);
        return delayOut;
    }

    // -- Reverb bank -> dsp::Reverb --------------------------
    // Last stage, matching FroggersEngine.hpp:617-618's wet/dry blend
    // (folded into Reverb::Process's own return -- see that struct's
    // header comment).
    // The dry floor is dsp::kMinDryLevel (dsp/Limiter.hpp), shared with the
    // Delay bank; see its declaration for why the value is what it is.
    // Reverb's blend, and the floor's application to it, both live in
    // dsp/Reverb.hpp, folded into Process's own return -- this class passes
    // the routed knob straight through, unscaled.
    // stoppedKnob overrides Tank drive (slot 10) to
    // kStopUnityDriveKnob while stopped, same idiom as the Filter comb
    // drive and Delay feedback drive above. Reverb::Process computes
    // its own `tankDrive` from this knob as a LOCAL (dsp/Reverb.hpp,
    // TankDriveFromKnob), so there is no member to read back after the
    // fact -- lastReverbTankDriveKnobEffective_ records the resolved
    // knob value passed in here for TestLastReverbTankDriveKnobEffective().
    dsp::StereoSample RouteReverbBank(dsp::StereoSample delayOut) {
        const float reverbTankDriveKnobEffective = StoppedKnob(FroggersBankId::Reverb, 10, kStopUnityDriveKnob);
        lastReverbTankDriveKnobEffective_ = reverbTankDriveKnobEffective;
        // stoppedKnob overrides Grit (slot 11) to kStopGritKnob
        // (0.0f) while stopped, same idiom as the Tank drive override
        // immediately above -- see the comment above
        // kStopFadeReleaseKnob for why (measured: Grit is the
        // third mechanism keeping the tank sustained after Stop).
        // lastReverbGritKnobEffective_ records the resolved knob value for
        // TestLastReverbGritKnobEffective(), same convention as
        // lastReverbTankDriveKnobEffective_ immediately above (Reverb::
        // Process has no member to read gritKnob01's use back from either).
        const float reverbGritKnobEffective = StoppedKnob(FroggersBankId::Reverb, 11, kStopGritKnob);
        lastReverbGritKnobEffective_ = reverbGritKnobEffective;
        // Reverb slots 9-13 (Hold/Tank
        // drive/Grit/Tilt/Tuned) -- passed as Process()'s own trailing
        // arguments, mirroring how slot 8 (Mod) is already
        // wired here, rather than via separate Set*() calls (dsp::Reverb's
        // own established convention differs from dsp::StereoDelay's, which
        // does use separate setters -- see dsp/Reverb.hpp's own comment on
        // each of these).
        const float reverbWetMixEffective = RoutedKnob(FroggersBankId::Reverb, 0);
        lastReverbWetMixEffective_ = reverbWetMixEffective;
        // Reverb slot 8 ("Mod") is the collapsed Mod depth/Mod rate control
        // -- Mod rate no longer has a live knob of its own (dsp::Reverb::
        // Process's modRateKnob01 parameter is [[maybe_unused]] now; see its
        // own comment), so the fixed 0.5f default is passed explicitly here
        // rather than reading Reverb slot 9, which is Hold now.
        const dsp::StereoSample reverbOut = reverb_.Process(
            delayOut,
            reverbWetMixEffective,
            RoutedKnob(FroggersBankId::Reverb, 2), RoutedKnob(FroggersBankId::Reverb, 3),
            RoutedKnob(FroggersBankId::Reverb, 4), RoutedKnob(FroggersBankId::Reverb, 5), RoutedKnob(FroggersBankId::Reverb, 6),
            RoutedKnob(FroggersBankId::Reverb, 7), sampleRate_,
            RoutedKnob(FroggersBankId::Reverb, 8), RoutedKnob(FroggersBankId::Reverb, 9),
            /*modRateKnob01=*/0.5f, reverbTankDriveKnobEffective, reverbGritKnobEffective,
            RoutedKnob(FroggersBankId::Reverb, 12), RoutedKnob(FroggersBankId::Reverb, 13),
            /*sendKnob01=*/RoutedKnob(FroggersBankId::Reverb, 1));
        return reverbOut;
    }

    // The real audio path -- Audio/Envelope
    // banks -> 3x dsp::Vco + dsp::MixOscVoices ->
    // Drive bank -> dsp::FrogBlock + dsp::DriveBlendPhase ->
    // Filter bank -> dsp::FilterFxChain -> Delay bank ->
    // dsp::StereoDelay -> Reverb bank -> dsp::Reverb.
    //
    // Ordering proof (verified by reading the cited source, not assumed):
    // `Parameter::GetRaw()` (External/Sheaf's
    // projects/synth/src/ParameterModulation.cpp:1207-1215) sums the
    // scene-blended center with `Modulators::ApplyActive()` -- i.e.
    // modulation-depth routing is already baked in there. `Parameter::
    // ProcessLitePhase1()` (:1459-1461) writes `currentKnobValues_[v] =
    // GetRaw(v)`, and `ParameterGroup::ProcessSamplePhase1()` calls that for
    // every parameter (:867-870) -- so by the time `FroggersParameterModel::
    // ApplyFuegoSeam()` runs (between Phase1 and Phase2, FroggersParameters.
    // hpp), `Parameter::CachedKnobValue()` already reflects modulation, and
    // ApplyFuegoSeam() then overwrites it with the fuegoized value via
    // `ReplaceCachedKnobValue()`. `ProcessSamplePhase2()` ->
    // `ProcessLitePhase2()` (:1471-1479) only slews `uiDisplayCenters_` from
    // `currentKnobValues_` and never rewrites the latter, so the cache is
    // unaffected by Phase2. Every `CachedKnobValue()` read below -- taken
    // after `parameters_.ProcessSample()` has returned for this sample -- is
    // therefore post-modulation AND post-fuego, exactly as required; if this
    // were not achievable as structured, the fallback was to stop and report
    // rather than reading raw values, which is why this citation chain
    // exists.
    dsp::StereoSample RouteAudioSample() {
        const std::array<float, 3> vcoOut = RouteAudioBank();
        const float chainIn = RouteEnvelopeBank(vcoOut);
        const float driveOut = RouteDriveBank(chainIn);
        const float filterOut = RouteFilterBank(driveOut);
        const dsp::StereoSample delayOut = RouteDelayBank(filterOut);
        const dsp::StereoSample reverbOut = RouteReverbBank(delayOut);
        return SanitizeOutputSample(reverbOut);
    }

    // Output-stage limiter.
    // Supersedes the old unconditional hard clamp (see
    // `SanitizeOutputSample`'s own comment for the historical
    // record) -- the
    // `frogg3rs-dsp-recovery` spec requirement forbidding soft-knee
    // limiting is superseded along with it.
    //
    // GAIN REDUCTION, not per-sample waveshaping: `Process()` computes ONE
    // scalar gain per sample from a smoothed envelope of the input's own
    // instantaneous magnitude and multiplies the sample by it, rather than
    // reshaping the sample the way a saturator/waveshaper would. Below
    // `threshold`, the target gain is mathematically exactly 1.0f (see
    // `DesiredMagnitude()`), and `1.0f * x == x` is an IEEE-754 identity
    // (exact, no rounding) -- so once `envelope` itself settles to exactly
    // 1.0f (its `Reset()`/initial value, never disturbed while every sample
    // stays under threshold, per the Sterbenz-lemma argument in the
    // struct's own `Process()` comment), passthrough is bit-identical. That
    // is this stage's own acceptance test.
    //
    // The struct itself now lives in `dsp::OutputLimiter`
    // (`dsp/Limiter.hpp`), not here -- it was a PRIVATE nested type until
    // a SECOND, independently-tuned instance was needed on the Filter bank's
    // peak branch (`FilterFxChain::peakLimiter`, `dsp/FilterFx.hpp`), which
    // needed the type reachable from a header below this one in the include
    // graph (this class includes `dsp/FilterFx.hpp`, never the reverse --
    // see `dsp/Limiter.hpp`'s own header comment for the full reasoning).
    // `outputLimiter_` below keeps its exact field name, its exact call
    // site (`outputLimiter_.Configure(sampleRate_)` in `PrepareToPlay()`,
    // unchanged), and its exact tuning (the single-argument `Configure()`
    // overload pins the master's original threshold/ceiling/attack/release,
    // `dsp/Limiter.hpp`'s own header comment) -- this move is behaviour-
    // neutral, not a retune.

    // This was the
    // final, unconditional hard clamp before a sample reached
    // `synth::AudioBlock::outputs`. In float audio, 1.0 IS full scale
    // (0 dBFS) -- the original 8.0f ceiling let this app hand the output
    // device a signal at +18 dBFS, which the device then hard-clipped into
    // a square wave (the operator-reported "blows the audio out"). Kept
    // here as the historical record of that fix (superseded by the
    // `OutputLimiter` above); the clamp itself no longer
    // runs -- `SanitizeOutputSample()` below now hands off to
    // `outputLimiter_.Process()` instead of `std::clamp`.
    //
    // The clamp alone was NOT recovery: it silently capped whatever the
    // upstream chain produced, sample by sample, forever, while a poisoned
    // recursive state upstream (e.g. a biquad or comb whose state went
    // non-finite or diverged) kept producing bad values underneath it.
    // Genuine recovery -- detecting and resetting the poisoned unit itself
    // -- is `RecoverPoisonedUnitState()` below, which the
    // limiter cooperates with but does not replace: the Filter bank's Comb
    // can self-oscillate (its feedback curve is asymmetric +-0.95 -- the
    // comb's sub-unity feedback bound, `dsp::Comb::GetFeedback` in
    // FilterFx.hpp -- see that function's own divergence-note comment) and the Reverb bank's
    // authored Hold control pushes its internal feedback
    // coefficient toward, but strictly below, 1.0, so both are ordinarily
    // bounded by construction -- the limiter is the safety net for when
    // "ordinarily" stops holding, not the primary defense (the sub-unity feedback bounds are).
    // Per-channel guards, then ONE linked limiter pass over the pair. The
    // guards are per-channel because a non-finite or subnormal value is a
    // property of the sample, not of the frame; the limiter is linked
    // because two independent envelopes would duck the channels differently
    // and swing the image. With l == r this is bit-identical to what the
    // mono path produced.
    dsp::StereoSample SanitizeOutputSample(dsp::StereoSample x) {
        x.l = GuardOutputSample(x.l);
        x.r = GuardOutputSample(x.r);
        constexpr float kMakeUpGain = 1.0f / dsp::kStageCeiling;  // 1.25x, +1.9 dB.
        const dsp::StereoSample limited = outputLimiter_.Process(x);
        return {std::clamp(limited.l * kMakeUpGain, -1.0f, 1.0f),
                std::clamp(limited.r * kMakeUpGain, -1.0f, 1.0f)};
    }

    static float GuardOutputSample(float x) {
        if (!std::isfinite(x)) {
            return 0.0f;
        }
        if (x != 0.0f && std::fabs(x) < std::numeric_limits<float>::min()) {
            return 0.0f;  // flush subnormal/denormal to zero.
        }
        return x;
    }

    // Limiter, then a hard bound: output is limited, then bounded.
    // `outputLimiter_` is a
    // feed-forward one-pole gain rider with no lookahead: its gain
    // reduction is computed from the signal it has already passed, so
    // it necessarily lags a fast-changing input and cannot itself
    // guarantee a ceiling -- measured, not assumed: a steady 1.5x
    // 200 Hz tone settled at a stable periodic peak of 1.027426 with
    // the gain envelope already converged at 0.685245, and the
    // self-oscillating-comb + near-unity-reverb-hold patch produced a
    // raw 1.560059 with the envelope at 0.62-0.64. Lookahead would fix
    // that lag but was rejected -- it adds latency to a live
    // instrument. So gain reduction does the work here, and the
    // std::clamp below only catches the residual overshoot the
    // limiter's lag lets through -- typically a few percent, which is
    // inaudible. This is NOT a reinstatement of the defect the old
    // unconditional 8x-full-scale clamp above caused (that produced a
    // guaranteed square wave); clamping a residual after gain
    // reduction has already brought the signal near 1.0 is a
    // different, harmless thing.
    //
    // Make-up gain: an earlier fix narrowed every per-stage limiter's
    // ceiling to dsp::kStageCeiling (0.80, dsp/Limiter.hpp) so the
    // master stops riding continuously below unity -- which also means
    // a fully-driven chain now arrives here around 0.80 instead of
    // 1.0. Restore that headroom AFTER the master limiter -- its
    // threshold/headroom math (DesiredMagnitude, dsp/Limiter.hpp) is
    // calibrated in absolute terms, so scaling BEFORE it would
    // silently retune every stage's budget -- and BEFORE the trailing
    // std::clamp, which stays as the residual-overshoot net. Derived
    // from kStageCeiling rather than hardcoded so the two constants
    // cannot drift apart.

    // The ceiling is DERIVED,
    // not measured, and re-derived here rather than re-tuned by feel.
    // Re-derived 2026-07-29 after the feedback bounds tightened both inputs below;
    // the ceiling constant itself is UNCHANGED (100.0 was
    // already comfortably above the tighter figures too, so there was
    // nothing to retune):
    //   - The filter chain's input is bounded to +-1.0 by
    //     `PadeSaturator::Saturate` (FilterFx.hpp:94-99, `std::max(-1.0f,
    //     std::min(1.0f, output))`) before it ever reaches a recursive
    //     stage this recovery watches.
    //   - `ResonantBump`'s peak gain is `A^2 == height`, `height ==
    //     dsp::ExpMapCompute(1.0f, 4.0f, knob)` (FroggersAppCore.hpp's own
    //     RouteAudioSample, Filter bank wiring -- the filter-bank gain bound) -- at most 4x for
    //     any reachable knob value (was 10x).
    //   - `scoopNotch`'s height is a DIP, not a gain: `max(0.05, 1 - 0.95 *
    //     scoop)` in [0.05, 1] -- adds no gain at all, unaffected by the filter-bank gain bound.
    //   - So the largest legitimate magnitude this chain can produce is
    //     roughly 4 (ResonantBump's peak gain), maybe ~8-10 with ringing
    //     (Comb's now sub-unity +-0.95 feedback -- a decaying loop,
    //     not a compounding one, but still capable of several round trips'
    //     worth of buildup before it settles). 100.0 remains a full 10x-25x
    //     above ANY of that -- comfortably above legitimate ringing,
    //     comfortably below float overflow (3.4e38, so 100.0 is ~3.4e36x
    //     below it) -- and because divergence under recursive feedback is
    //     exponential, a REAL fault crosses from "normal" to "past 100" in
    //     milliseconds, not minutes, so this ceiling is never mistaken for
    //     a slow legitimate swell.
    // DO NOT retune this constant without re-deriving it from the above.
    static constexpr float kMaxUnitStateMagnitude = 100.0f;

    // "Sustained": a unit's state magnitude
    // must stay above kMaxUnitStateMagnitude for at least this much REAL
    // TIME, not merely "the last block-end snapshot", before it is treated
    // as a genuine divergence rather than a transient. Tracked in seconds
    // (not a block count) so the definition does not silently change shape
    // with block size -- app/Makefile's own tests span both blockSize==1
    // and blockSize==256/512
    // (everything else). 10ms is deliberately short: the ceiling's own
    // derivation above establishes that a real fault's exponential
    // divergence crosses from normal into "past 100" in milliseconds, so it
    // will still read as over-ceiling several block-boundaries later almost
    // certainly (its magnitude keeps growing, it does not hover exactly at
    // the crossing point) -- while a single loud transient that peaks once
    // and decays within a block will already be back under the ceiling by
    // the very next block-end snapshot, resetting this unit's counter to
    // zero before 10ms of continuous over-ceiling readings can ever
    // accumulate. 10ms is also short enough that, on the rare occasion a
    // real fault DOES take this path, the operator hears at most a brief
    // click before recovery, not an audible dropout.
    static constexpr float kSustainedOverCeilingSeconds = 0.01f;

    // One unit's worth of Tier 1 (finiteness) + Tier 2
    // (sustained magnitude) recovery, called once per block per unit from
    // RecoverPoisonedUnitState() below. `Unit` is any of the dsp:: structs
    // that carry `StateFinite()`/`StateMagnitude()`/`Reset()` (dsp::Vco,
    // dsp::ResonantBump, dsp::Comb, dsp::Oversampler2x,
    // dsp::SampleRateReducer, dsp::DriveBlendPhase) -- templated rather than
    // duplicated 10 times over, since the recovery POLICY (finiteness first,
    // then a sustained-magnitude watch) is identical for every one of them
    // and only the concrete unit type differs.
    //
    // Tier 1 fires immediately and unconditionally resets the sustained-
    // magnitude counter too (a unit that just went non-finite has no
    // meaningful prior magnitude history worth preserving). Tier 2 only
    // evaluates once Tier 1 has confirmed the state IS finite -- a
    // non-finite StateMagnitude() read (e.g. NaN) would never compare
    // `>` the ceiling, silently defeating Tier 2, so Tier 1 must run first.
    template <typename Unit>
    void RecoverUnitIfNeeded(Unit& unit, float& overCeilingSeconds, std::size_t blockFrames) {
        if (!unit.StateFinite()) {
            unit.Reset();
            overCeilingSeconds = 0.0f;
            return;
        }
        if (unit.StateMagnitude() > kMaxUnitStateMagnitude) {
            overCeilingSeconds += static_cast<float>(blockFrames) / sampleRate_;
            if (overCeilingSeconds >= kSustainedOverCeilingSeconds) {
                unit.Reset();
                overCeilingSeconds = 0.0f;
            }
        } else {
            overCeilingSeconds = 0.0f;
        }
    }

    // Tier 1 ONLY -- no magnitude/Tier 2 counter. Used for
    // `delay_`/`reverb_`: both are exposed to the exact same "a poisoned
    // sample from an upstream unit permanently latches this unit's own
    // state non-finite" failure Tier 1 exists to fix (see
    // dsp::Reverb::StateFinite()'s and dsp::StereoDelay::StateFinite()'s
    // own comments) -- BUT they deliberately do NOT get Tier 2's
    // sustained-magnitude watch, because kMaxUnitStateMagnitude's derivation
    // above is specific to the Filter-chain-bounded units below (input
    // bounded to ~10-30 by construction); `delay_`'s feedback (up to 0.98)
    // and `reverb_`'s authored Hold (up to 0.999) are BIBO-stable feedback
    // loops that can LEGITIMATELY settle to a much larger-but-finite steady
    // state under sustained loud input (roughly input/(1-feedback), which
    // at reverb_'s extreme Hold alone is already >> 100) -- applying the
    // same ceiling there would misfire on a genuinely loud, stable, musical
    // tail, not just a fault.
    template <typename Unit>
    void RecoverIfNonFinite(Unit& unit) {
        if (!unit.StateFinite()) {
            unit.Reset();
        }
    }

    // Tasks 2.2-2.5 (design: per-unit, never global -- see this class's
    // header comment on why a global reset is wrong: it would cut the
    // reverb tail and delay repeats every time one unrelated filter
    // misbehaved). Called once per block, after ProcessBlock()'s per-sample
    // loop (RecoverUnitIfNeeded() above needs `block.numFrames` for Tier 2's
    // time accounting, hence taking it as a parameter rather than being
    // folded into the per-sample loop itself).
    //
    // The unit set this
    // watches is no longer listed here directly -- it walks
    // ForEachStatefulUnit() below, the single definition site (see that
    // method's own comment). Tier 2's per-unit sustained-over-ceiling
    // counter now lives ON each Magnitude-tagged unit itself
    // (`unit.overCeilingSeconds`, dsp::Vco/dsp::ResonantBump/dsp::Comb/
    // dsp::Oversampler2x/dsp::SampleRateReducer/dsp::DriveBlendPhase's own
    // member -- see dsp::Vco::overCeilingSeconds's own comment, Vco.hpp,
    // for why it moved there instead of staying a parallel member here) --
    // so no lookup/adapter is needed to pair a visited unit back to its own
    // counter; the visitor just reads it directly off whichever unit it was
    // called with.
    //
    // The tier passed to `visit` is a COMPILE-TIME tag type
    // (dsp::FiniteOnly/dsp::Magnitude, dsp/RecoveryTier.hpp), not a runtime
    // enum -- `if constexpr` below is what makes that necessary: it is the
    // only way to guard RecoverUnitIfNeeded() (which requires
    // `unit.StateMagnitude()`) from being instantiated for FiniteOnly-tagged
    // units that never defined that method (`outputLimiter_`,
    // `filterChain_.peakLimiter`) -- a runtime `if` on an enum value does
    // NOT prevent instantiation, since the compiler still has to type-check
    // both branches' bodies for every concrete `unit` type the generic
    // lambda below gets called with.
    //
    // Tier 1 ONLY (RecoverIfNonFinite, FiniteOnly-tagged units): `delay_`/
    // `reverb_`/`outputLimiter_`/`filterChain_.peakLimiter`, reusing their
    // EXISTING Reset()/ClearBuffers() (StereoDelay::Reset() is a thin alias
    // for ClearBuffers(), added so the same generic call works uniformly --
    // see that method's own comment) rather than adding duplicates. For
    // `delay_`/`reverb_`, this is IN
    // ADDITION to their existing, separate, transport-edge-gated reset
    // (`wasTransportRunning_`/`delayReverbClearPending_` above, which exists
    // for a different reason -- silencing self-sustaining feedback on Stop,
    // not fault recovery). The two do not conflict: Reset()/ClearBuffers()
    // is idempotent, and in NORMAL operation `delay_`/`reverb_` never
    // actually go non-finite on their own (both are BIBO-stable by
    // construction, per the comment above) -- Tier 1 only ever fires here
    // when a genuine upstream fault cascaded a non-finite sample into them,
    // exactly the case with no other recovery path. `outputLimiter_`'s and
    // `filterChain_.peakLimiter`'s envelopes get the same Tier-1-only
    // treatment: a finite input always keeps `envelope` in (0, 1] by
    // construction (dsp::OutputLimiter's own comment), so there is no
    // analogous "legitimately very large but finite" case Tier 2 would need
    // to tolerate.
    void RecoverPoisonedUnitState(std::size_t blockFrames) {
        ForEachStatefulUnit([this, blockFrames](auto& unit, auto tier) {
            if constexpr (std::is_same_v<decltype(tier), dsp::Magnitude>) {
                RecoverUnitIfNeeded(unit, unit.overCeilingSeconds, blockFrames);
            } else {
                RecoverIfNonFinite(unit);
            }
        });
    }

    FroggersParameterModel parameters_;
    FroggersModulationSlate modulation_;

    // The real audio path's own DSP state (sample-rate-dependent
    // members are (re)initialized in PrepareToPlay()). Deliberately separate
    // instances from modulation_'s private VCOs -- see this class's
    // PrepareToPlay() comment.
    float sampleRate_ = 48000.0f;
    // Collapsed from three separately-named dsp::Vco audioVco1_/
    // audioVco2_/audioVco3_ members into one std::array -- dsp::Vco is
    // trivially copy/move constructible/assignable (all members are float,
    // a trivially-copyable synth::Color, and a raw pointer; verified by a
    // standalone compile+run before this refactor), which is what makes
    // std::array<dsp::Vco, 3> construction/element-access valid here.
    // Index 0/1/2 map to the old audioVco1_/audioVco2_/audioVco3_ identities
    // respectively, everywhere this class uses them (constructor scope
    // wiring, PopulateUIState, TestAudioVco, RouteAudioSample,
    // ForEachStatefulUnit).
    std::array<dsp::Vco, 3> audioVcos_;
    dsp::VcoAdsrState audioAdsr_;
    dsp::FrogBlock drive_;
    dsp::DriveBlendPhase driveBlendPhase_;
    dsp::FilterFxChain filterChain_;
    dsp::StereoDelay delay_;
    dsp::Reverb reverb_;
    // RouteAudioSample()'s last-resolved effective values for the
    // Freeze knob (dsp::DelayParams::dfrz), Reverb Tank-drive knob, and
    // Reverb Grit knob -- written every sample, read only by
    // TestLastDelayFreezeKnobEffective()/TestLastReverbTankDriveKnobEffective()/
    // TestLastReverbGritKnobEffective() above (test-only; production
    // code never reads these back). Default values match each knob's own
    // registered default (0.0f for Freeze, 0.5f/unity for Tank drive, 0.0f
    // for Grit) so an un-run instance reads a sane value rather than
    // 0-initialized noise.
    float lastDelayFreezeKnobEffective_ = 0.0f;
    float lastReverbTankDriveKnobEffective_ = 0.5f;
    float lastReverbGritKnobEffective_ = 0.0f;
    float lastReverbWetMixEffective_ = 0.0f;
    float lastDelayWetMixEffective_ = 0.0f;
    // The output-stage limiter's own per-sample
    // gain-envelope state -- see the comment above `SanitizeOutputSample()`
    // for the full design rationale, and `dsp::OutputLimiter`'s own
    // definition (`dsp/Limiter.hpp`, moved out separately) for the struct itself.
    dsp::OutputLimiter outputLimiter_;

    // This class's own contribution to the "every stateful
    // unit in the audio path" enumeration, COMPOSED from the members
    // declared above rather than re-listing dsp::FrogBlock's/
    // dsp::FilterFxChain's own sub-units here a second time (see those
    // structs' own ForEachStatefulUnit, dsp/Drive.hpp, dsp/FilterFx.hpp).
    // This is the SINGLE definition site both RecoverPoisonedUnitState
    // (below) and the Stop flush (ProcessBlock, above) call -- the fix here
    // was that the Stop flush used to be a SECOND,
    // independent, truncated definition of this same concept (2 units, only
    // delay_/reverb_) instead of calling this one. The tier passed to
    // `visit` is DECLARED here, never inferred from the unit's interface
    // (dsp::FiniteOnly's/dsp::Magnitude's own comment, dsp/RecoveryTier.hpp,
    // explains why). These are compile-time tag
    // TYPES, not a runtime enum value -- see RecoverPoisonedUnitState()'s
    // own comment below for why that distinction is load-bearing. Per-unit
    // Tier-2 sustained-over-ceiling bookkeeping now lives ON each Magnitude-tagged unit itself
    // (`unit.overCeilingSeconds`) rather than as ten parallel members here,
    // so unlike the first version of this method there is nothing else this
    // class needs to own alongside the unit references themselves.
    template <typename Visitor>
    void ForEachStatefulUnit(Visitor&& visit)
    {
        // Collapsed from three visit(audioVcoN_, ...) lines into a
        // loop over audioVcos_ -- same 3 dsp::Vco instances, same order
        // (index 0/1/2 == old audioVco1_/2_/3_), same dsp::Magnitude tag,
        // same visitor. Count/order/tag of every OTHER unit this method
        // enumerates is unchanged (nothing below this loop was touched), so
        // the total enumeration is still all 14 units in the same order.
        for (auto& vco : audioVcos_) {
            visit(vco, dsp::Magnitude{});
        }
        visit(driveBlendPhase_, dsp::Magnitude{});
        drive_.ForEachStatefulUnit(visit);
        filterChain_.ForEachStatefulUnit(visit);
        visit(delay_, dsp::FiniteOnly{});
        visit(reverb_, dsp::FiniteOnly{});
        visit(outputLimiter_, dsp::FiniteOnly{});
    }

    // A capture that hits its cap disarms itself on the audio thread the
    // instant it happens (recordArmed_.store(false) inside ProcessBlock's
    // per-sample loop above, :1186-1194) -- no Stop/Record press ever
    // follows it, so nothing else would ever call QueueRecordingExport()
    // for that capture. TakePendingFileExport() and ArmRecording() both
    // call this first instead: the engine polls TakePendingFileExport()
    // once per message-thread tick regardless of any UI press, so that
    // poll alone is enough to pick a truncated capture up; a Record press
    // that beats the poll goes through ArmRecording(), so this call there
    // flushes the old capture before recordBuffer_.assign() wipes it out
    // from under it. captureExported_ keeps a capture from being queued
    // twice across repeated calls from either caller.
    void QueueTruncatedExportIfPending() {
        if (RecordingTruncated() && RecordedFrameCount() > 0 && !captureExported_ && !RecordArmed()) {
            QueueRecordingExport();
        }
    }

    // Stop-transport reset (operator report "Stop doesn't work" -- the ASR
    // gate closes on Stop but delay_/reverb_ are feedback structures that
    // self-sustain: StereoDelay feedback runs up to 0.98, Reverb Hold pushes
    // its feedback arbitrarily close to 1.0). Remembers the PREVIOUS frame's
    // transport-running state so ProcessBlock can detect the exact
    // running->stopped edge and reset both units, once, on the audio thread
    // that owns them -- see ProcessBlock's own comment at the gate/edge
    // computation for why this is not done from the UI thread's kStop
    // handler (FroggersUiSurface.hpp's HandleAction) instead.
    bool wasTransportRunning_ = false;

    // The PREVIOUS sample's Freeze-
    // latch state, same idiom as `wasTransportRunning_` just above --
    // remembered so ProcessBlock can detect the latch's own
    // latched->unlatched edge while the transport is already stopped (the
    // "release the latch" escape hatch out of the sustained drone) instead
    // of only the transport's running->stopped edge.
    bool wasFreezeLatched_ = false;

    // True while stopped and a clear is still owed --
    // armed either at the running->stopped edge (if a voice was still
    // releasing there) or left false (if the edge's own AllIdle() check
    // already fired the clear directly). Stays true, without re-clearing
    // every block, until either AllIdle() first turns true (fires the one
    // owed clear) or the transport resumes (cancels it, since a resumed
    // delay/reverb is legitimately playing and must not be wiped) -- see
    // ProcessBlock's per-sample loop, where the running->stopped edge is
    // detected, for the full logic.
    bool delayReverbClearPending_ = false;

    // One ScopeWriter, ReserveChans(1) per VCO
    // returning a ScopeWriterHolder -- declared before the holders below
    // since the constructor's init list calls vcoScopeWriter_.ReserveChans()
    // to construct them (member init order follows declaration order, not
    // ctor init-list order).
    synth::ScopeWriter vcoScopeWriter_;
    synth::ScopeWriterHolder vco1ScopeHolder_;
    synth::ScopeWriterHolder vco2ScopeHolder_;
    synth::ScopeWriterHolder vco3ScopeHolder_;
    // Populated once per block (ProcessBlock(), after the
    // per-sample loop) via dsp::Vco::PopulateUIState().
    dsp::Vco::UIState vco1ScopeUiState_;
    dsp::Vco::UIState vco2ScopeUiState_;
    dsp::Vco::UIState vco3ScopeUiState_;
    // The ScopeVisualizer<UIState> instance itself -- this is the
    // "no app-level drawing code" half (VcoScopeVisualizer() above
    // exposes it for the surface to place).
    synth::ui::ScopeVisualizer<dsp::Vco::UIState> vcoScopeVisualizer_;

    // Populated once per block from filterChain_'s
    // live peak/comb, same convention as the VCO UIStates above.
    dsp::ResonantBump::UIState peakUiState_;
    dsp::Comb::UIState combUiState_;
    // One TransferFunctionVisualizer per filter, attached as
    // the Filter bank's Peak/Comb parameter underlays via
    // parameters_.Init()'s peakVisualizer_/combVisualizer_ arguments --
    // rendered automatically as grid-cell underlays, not
    // placed into the surface's own tree directly.
    TransferFunctionVisualizer peakVisualizer_;
    TransferFunctionVisualizer combVisualizer_;

    // The UI-thread -> audio-thread request
    // bridge (see this file's header comment) plus the audio-thread-only
    // active-bank/drill-in bookkeeping it drives.
    std::atomic<int> pendingBankSelect_{-1};
    std::atomic<int> pendingEncoderPress_{-1};
    std::atomic<bool> pendingRandomizeAll_{false};
    std::atomic<bool> pendingRandomizePage_{false};
    // Reset's own request flags, beside the Randomize pair they mirror.
    std::atomic<bool> pendingResetAll_{false};
    std::atomic<bool> pendingResetPage_{false};
    std::atomic<double> pendingTempoBpmRequest_{-1.0};
    // Queued by Init()'s routed-input-changed callback (message thread);
    // drained by ProcessFrame() (audio thread), same -1-sentinel/exchange
    // idiom as pendingBankSelect_/pendingEncoderPress_ above. -1 = no
    // pending transition, 0 = not routed, 1 = routed.
    std::atomic<int> pendingExternalAudioRouted_{-1};

    std::atomic<double> tempoDisplayBpm_{synth::MasterClock::kDefaultTempoBpm};
    std::atomic<bool> tempoExternallyClocked_{false};
    // Published once per block, same contract as the
    // two atomics above -- see TransportRunning()'s own comment.
    std::atomic<bool> transportRunningDisplay_{false};
    // Published once per block from ProcessBlock() -- see DrillLevel()'s
    // own comment.
    std::atomic<std::size_t> drillLevelDisplay_{0};
    // Published once per Randomize All/Page press from ProcessFrame() --
    // see LastRandomizePartial()'s own comment.
    std::atomic<bool> lastRandomizePartial_{false};

    // Robustness measure (see PrepareToPlay()'s own comment): the surface's
    // last explicit Play(true)/Stop(false) request, independent of
    // `MasterClock::TransportState()` -- defaults false so a fresh app (or a
    // headless rig that never presses Play) stays silent exactly as before.
    std::atomic<bool> desiredTransportRunning_{false};

    // The Freeze transport button's latch (see SetFreezeLatched()/
    // FreezeLatched() above) -- defaults false so a fresh app's Delay stays
    // on the ordinary clamped-encoder path until the operator
    // latches it.
    std::atomic<bool> freezeLatched_{false};

    // A bounded mono capture buffer of what the
    // operator hears (post-limiter, pre-channel-fanout -- see the capture
    // hook inside ProcessBlock()'s per-sample loop below). recordArmed_ is a
    // direct UI-thread-write, audio-thread-read (and audio-thread-
    // clearable, on truncation) flag -- same release/acquire pairing as
    // desiredTransportRunning_/freezeLatched_ above, not the coalescing
    // "pending request" two-hop the Request*/pending*_ atomics above use.
    // All allocation happens once, in ArmRecording() (UI thread, above);
    // the audio thread only ever appends to an already-sized recordBuffer_
    // or clears a flag, never allocates.
    std::vector<float> recordBuffer_;
    std::atomic<bool> recordArmed_{false};
    std::atomic<std::uint64_t> recordFrames_{0};
    std::atomic<bool> recordTruncated_{false};
    static constexpr float kMaxRecordSeconds = 30.0f * 60.0f;
    // UI-thread-only (written/read only from ArmRecording()/
    // RecordRefusalReason() above, never touched by the audio thread) -- no
    // atomic needed.
    static constexpr const char* kRecordRefusalReason = "Press Play before recording.";
    const char* recordRefusalReason_ = nullptr;

    // Storage for QueueRecordingExport()'s output, drained by
    // TakePendingFileExport() -- message-thread-only, same as
    // recordRefusalReason_ just above (never touched by the audio thread).
    std::optional<synth::FileExport> pendingExport_;
    // Whether the CURRENT capture (the one ArmRecording() most recently
    // set up) has already been queued for export -- set by
    // QueueRecordingExport(), cleared by ArmRecording() for the next
    // capture. Lets QueueTruncatedExportIfPending() (below) queue a
    // cap-triggered truncation's export exactly once, even though both
    // TakePendingFileExport() and ArmRecording() call it on every
    // invocation. Message-thread-only, same as pendingExport_ above.
    bool captureExported_ = false;

    // DIAGNOSTIC (2026-08-07). "Stop does not stop" has never
    // reproduced in the test harness: one measurement found a clean decay on a static
    // patch, and another refuted the parametric-oscillation hypothesis with the
    // modulated comb-feedback knob proven to sweep its full range while output
    // held at exactly 0. The operator still hears it in the real app, so the
    // harness is not reaching the real condition and the measurement has to
    // happen in a real session. Enabled ONLY by setting FROGG3RS_STOP_DIAG in
    // the environment; a normal launch reads the env var once at construction
    // and then pays a single bool test per block.
    //
    // ~46s at 48kHz/512 -- long enough to cover the operator's "over a minute"
    // report without unbounded logging.
    static constexpr int kStopDiagBlocks = 4096;
    const bool stopDiagEnabled_ = std::getenv("FROGG3RS_STOP_DIAG") != nullptr;
    int stopDiagBlocks_ = 0;
    float stopDiagPeak_ = 0.0f;

    std::size_t activeBankIx_ = 0;
    std::optional<FroggersModulationDrillIn> drillIn_;
};

// A pure std:: WAV encoder, kept in the no-JUCE core (check-no-juce, this
// file's own header comment) so it is testable headlessly -- no test binary
// compiles FroggersMain.cpp, the only place a juce::AudioFormatWriter could
// otherwise have done this -- and so the host stays a thin
// dialog-plus-byte-stream layer that calls this and writes exactly what it
// returns. Canonical 44-byte PCM header (RIFF/WAVE/fmt /data), mono, 16-bit
// signed little-endian samples, scaled by 32767 (not 32768) so +1.0f
// round-trips exactly and the encoder does not need a separate rule for the
// one negative value 32768 would otherwise reach -- the same convention
// most hand-rolled PCM16 encoders use.
inline std::vector<std::uint8_t> EncodeWavPcm16Mono(std::span<const float> samples, float sampleRate) {
    constexpr std::uint16_t kNumChannels = 1;
    constexpr std::uint16_t kBitsPerSample = 16;
    constexpr std::uint16_t kBytesPerSample = kBitsPerSample / 8;

    const auto sampleRateHz = static_cast<std::uint32_t>(sampleRate > 0.0f ? sampleRate : 0.0f);
    const auto dataSize = static_cast<std::uint32_t>(samples.size() * kBytesPerSample);
    const std::uint32_t byteRate = sampleRateHz * kNumChannels * kBytesPerSample;
    const auto blockAlign = static_cast<std::uint16_t>(kNumChannels * kBytesPerSample);

    std::vector<std::uint8_t> bytes;
    bytes.reserve(44 + dataSize);
    const auto appendTag = [&bytes](const char* fourCc) { bytes.insert(bytes.end(), fourCc, fourCc + 4); };
    const auto appendU32 = [&bytes](std::uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xff));
        }
    };
    const auto appendU16 = [&bytes](std::uint16_t value) {
        bytes.push_back(static_cast<std::uint8_t>(value & 0xff));
        bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
    };

    appendTag("RIFF");
    appendU32(36 + dataSize);  // ChunkSize: everything after this field.
    appendTag("WAVE");
    appendTag("fmt ");
    appendU32(16);  // Subchunk1Size -- 16 for PCM.
    appendU16(1);   // AudioFormat -- 1 for integer PCM.
    appendU16(kNumChannels);
    appendU32(sampleRateHz);
    appendU32(byteRate);
    appendU16(blockAlign);
    appendU16(kBitsPerSample);
    appendTag("data");
    appendU32(dataSize);

    for (float sample : samples) {
        const float clamped = std::clamp(sample, -1.0f, 1.0f);
        const auto pcm = static_cast<std::int16_t>(
            std::clamp(std::lround(clamped * 32767.0f), static_cast<long>(-32768), static_cast<long>(32767)));
        appendU16(static_cast<std::uint16_t>(pcm));
    }
    return bytes;
}

}  // namespace synth_froggers
