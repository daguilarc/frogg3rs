#pragma once

// frogg3rs_vst::FroggersPluginProcessor -- the VST/AU plugin skeleton. This
// is the ONLY JUCE-facing wrapper over the JUCE-free app core
// (synth_froggers::FroggersApp,
// app/Froggers.hpp) -- every file under app/vst/ may see JUCE; the core
// under app/ and its check_no_juce gate (app/Makefile) stay untouched.
//
// Init/drive sequence traced from the two existing JUCE-facing callers of
// synth::Engine<App>, both read start-to-finish before writing this file:
//   - External/Sheaf/projects/synth/runtime/Runtime.hpp (the real JUCE host
//     shell FroggersMain.cpp's launcher session uses under the hood, via
//     synth_runtime::RuntimeShellSession -- External/Sheaf/projects/synth/runtime/Runtime.hpp:100-106 constructs
//     synth::Engine<App> with a NowMicros() timestamp provider; Start()
//     (:178-230) calls engine_.SetRuntimeDataPaths(...) THEN
//     engine_.Initialize() exactly once; audioDeviceAboutToStart (:594-599)
//     calls engine_.Prepare(sampleRate, blockSize) on every device
//     negotiation (not just once); audioDeviceIOCallbackWithContext (:591)
//     calls engine_.ProcessBlock(block, NowMicros()) every callback.
//   - External/Sheaf/projects/synth/tests/support/SynthRig.hpp, the
//     headless JUCE-free harness FroggersHeadlessTests.cpp already
//     drives FroggersApp through
//     (External/Sheaf/projects/synth/tests/support/SynthRig.hpp:60,93-94 constructor: Initialize() then Prepare() in
//     that order; RunOneBlockAt :513-539 builds a synth::AudioBlock sized
//     from FroggersAppCore::Config()'s numAudioInputs and calls
//     engine_.ProcessBlock(block, timestamp); StartAt/StopAt :174-184 push
//     synth::MessageIn::Start/Stop
//     on engine_.UiBus(), the exact message FroggersUiSurface::HandleAction's
//     Play/Stop branches push (app/FroggersUiSurface.hpp's `HandleAction`), paired
//     with FroggersApp::SetDesiredTransportRunning(true/false)
//     (FroggersUiSurface.hpp's `LatchThenTransport`) -- the UI-thread seam
//     PrepareToPlay() re-asserts across a MasterClock::Prepare() reset,
//     see FroggersAppCore.hpp's own PrepareToPlay() comment).
//
// This class mirrors Runtime.hpp's call order exactly (constructor:
// SetRuntimeDataPaths + Initialize(), once; prepareToPlay(): Prepare(),
// every call; processBlock(): ProcessBlock(), every callback) without any
// of Runtime.hpp's device-manager/MIDI-connection/window machinery -- none
// of that is needed to drive the core, so none of it is duplicated: the core
// is not blocked on launcher-session machinery to run headlessly --
// synth::Engine<App> is the seam, and it is already JUCE-free and driven
// exactly this way by SynthRig.hpp's own JUCE-free tests.
//
// Data path: reuses the SAME "frogg3rs" stable app id and shared
// ~/Library/Sheaf data root FroggersMain.cpp's direct-launch app uses
// (app/FroggersMain.cpp's `FroggersMainApplication::initialise` cites this reasoning: "so existing saved
// patches ... are not orphaned"), via the same tiny data-path helper
// (synth_runtime::SheafUserApplicationDataRoot(), HostDataPaths.cpp) rather
// than duplicating its logic -- that helper depends only on juce_core (a
// plugin dependency anyway) and owns no window/device/thread state, so
// reusing it does not pull launcher machinery into this class.
//
// Transport: FroggersAppCore's ASR gate stays closed (silence) until the
// transport is started via synth::MessageIn::Start + SetDesiredTransportRunning
// (FroggersAppCore.hpp's ProcessBlock()/TransportQuarterNotesAt() gating
// comment). This class adds no note handling: the MIDI buffer is accepted
// and ignored (see acceptsMidi() below).
//
// Host transport and tempo: the DAW is the transport AND tempo authority. Both producers
// below are driven from `getPlayHead()`, and both obey the SAME
// audio-thread-safety trace (see the header comment on
// pendingTransportEdge_ below for the full citation chain):
// `juce::AudioPlayHead::getPosition()` may ONLY be called from
// processBlock() (juce_AudioPlayHead.h's own doc comment: "undefined
// behaviour ... multithreading issues if it's not called on the audio
// thread"), while `engine_.UiBus()` (the same bus
// FroggersUiSurface::PushMessage/HandleAction write from the UI/message
// thread, FroggersUiSurface.hpp's `PushMessage`) is an SPSC ring buffer
// (MessageInBus::Push, External/Sheaf/projects/synth/src/ParameterModulation.cpp:3995-4005: an unsynchronized
// read-modify-write of `tail_`, safe for exactly one producer) already
// claimed by that same thread's Push calls in every other host of this
// surface -- so processBlock() must never call Push() itself, on pain of a
// second concurrent producer racing the first. The split used throughout
// this class: processBlock() (audio thread) ONLY reads the playhead and
// republishes what it saw into lock-free atomics; timerCallback() (message
// thread, wired via the private juce::Timer override below, see its own
// comment) is the ONLY thing that ever
// calls engine_.UiBus().Push() or engine_.RequestSyncConfiguration() from
// this class, reading those atomics to decide what to push.
//
// TestStartTransport()/TestStopTransport() below still exist purely for
// FroggersVstSmokeTest.cpp's existing headless "no host at all" smoke
// coverage; the REAL producer (host playhead edge-trigger + host-tempo
// clock-tick synthesis) is exercised by app/vst/FroggersVstHostTests.cpp
// via a fake AudioPlayHead and PumpMessageThreadForTest(), the
// deterministic stand-in for a real juce::Timer firing (a headless CTest
// binary runs no message loop, so juce::Timer callbacks never fire on
// their own -- see PumpMessageThreadForTest()'s own comment).
//
// Host parameters (frogg3rs-vst-host spec, "Parameters are external via a
// stable automation surface"): every user parameter of the six-bank model
// (FroggersParameterModel, app/FroggersParameters.hpp) is exposed as a
// juce::AudioProcessorParameter with a flat stable ID, bridged
// BIDIRECTIONALLY to FroggersParameterModel -- the app's single parameter
// authority -- plus Freeze (already wired via DispatchAction).
// SAME audio-thread/message-thread split as 6.1/6.3 above, extended, not
// replaced: processBlock() (audio thread) additionally publishes each
// parameter's current value into a per-parameter atomic UIState snapshot
// (Parameter::PopulateUIState(), the exact call synth::Engine's own
// ProcessBlock already makes for its throttled UI publish -- see
// processBlock()'s own comment for why this class makes its OWN,
// unthrottled, bank-selection-independent calls instead of reusing that
// one); timerCallback() (message thread, via PumpHostParameterBridge()) is
// the ONLY thing that reads those snapshots to notify the host, and the
// ONLY thing that pushes a host-driven write into the core (via the SAME
// engine_.UiBus().Push()/DispatchAction() seams 6.1/6.3/Freeze already use)
// -- processBlock() still never touches UiBus or DispatchAction itself. No
// plugin-side MIDI mapping or MIDI learn exists anywhere in this class (per
// the governing spec): DAW-side MIDI mapping reaches this instrument
// entirely through the ordinary host-parameter-automation surface below,
// the same way it would for any other automatable plugin parameter.

#include "Froggers.hpp"
#include "synth/AppContext.hpp"
#include "synth/AppRegistry.hpp"
#include "synth/Engine.hpp"
#include "synth/MasterClock.hpp"
#include "synth/ParameterModulation.hpp"

#include "HostDataPaths.hpp"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace frogg3rs_vst {

class FroggersPluginProcessor final : public juce::AudioProcessor, private juce::Timer {
public:
    // Production entry point (also what JUCE's generated createPluginFilter()
    // constructs, see FroggersPluginProcessor.cpp): resolves the shared
    // "frogg3rs" data root (see this file's own header comment).
    FroggersPluginProcessor();

    // Test-only entry point (5.2): lets the smoke test point the engine at a
    // scratch data root instead of the shared production one, the same
    // reason FroggersHeadlessTests.cpp's UseScratchRuntimeDataPaths() exists
    // -- a headless test must never read or write the operator's real
    // ~/Library/Sheaf state.
    explicit FroggersPluginProcessor(synth::RuntimeDataPaths dataPathsForTest);

    // Not defaulted: must stopTimer() before the rest of this object (in
    // particular engine_) tears down -- a juce::Timer's callback can fire on
    // the message thread right up until stopTimer() returns, and
    // timerCallback() below touches engine_.
    ~FroggersPluginProcessor() override;

    FroggersPluginProcessor(const FroggersPluginProcessor&) = delete;
    FroggersPluginProcessor& operator=(const FroggersPluginProcessor&) = delete;

    // --- juce::AudioProcessor ------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    // Fires once per actual bus-layout change (never once per
    // processBlock()) -- see this method's own comment in the .cpp for the
    // full JUCE-callback trace and why this, not processBlock() or
    // isBusesLayoutSupported(), is where "is an input actually routed in"
    // gets recomputed.
    void processorLayoutsChanged() override;

    // The real portable-surface editor (FroggersPluginEditor.hpp)
    // -- defined out-of-line in the .cpp so this header does not need to
    // include the JUCE-GUI-heavy PortableJuceBackend.hpp chain, and so
    // FroggersPluginEditor.hpp (which itself needs this class's full
    // definition, for EditorSurface()/SetEditorRepaintHook() below) does not
    // have to be included before this class is complete. hasEditor() is
    // true now that createEditor() returns a real editor -- every host
    // still automates/reads back every parameter below via its own generic
    // UI too, this just adds the portable-surface UI on top for hosts that
    // show it.
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Frogg3rs"; }
    // NEEDS_MIDI_INPUT TRUE (CMakeLists.txt) declares MIDI input accepted;
    // acceptsMidi() must agree. The buffer is ignored every block (see
    // processBlock()): a synth that declares MIDI input accepted is fine
    // with no note wiring.
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // DAW session-state persistence. Round-trips the same portable
    // "sheaf.synth.patch" JSON representation the standalone app and the
    // browser host already use for saved patches (BuildPatchJSON/
    // LoadPatchJSON over ParameterManager::ParameterValuesToJSON/
    // LoadParameterValuesFromJSON, synth/PatchPersistence.hpp) -- not a
    // second, plugin-private format. Both directions apply through the
    // parameter authority (ParameterManager, reached via
    // engine_.Context().patchInputBus/patchOutputBus) rather than through
    // this class's host-parameter bridge, and both stay purely in memory:
    // neither direction reads or writes the shared "frogg3rs" patches
    // directory this class's own data root points at (ProductionDataPaths(),
    // FroggersPluginProcessor.cpp) or any other filesystem location. See
    // PumpStatePersistence()'s own comment (in the .cpp) for the full
    // mechanism, including why both directions are necessarily asynchronous
    // and how getStateInformation() still returns synchronously despite
    // that.
    //
    // The Freeze latch is not a ParameterManager parameter (see
    // HostParamEntry::Kind::kFreeze's own comment) so it has no place in
    // ParameterValuesToJSON -- Sheaf's own patch format has no concept of
    // it. It is still user-visible, hand-toggled, host-automatable state, so
    // it is carried as a sibling "sessionExtras" object next to
    // "parameterValues" at the top of the same JSON document -- a key
    // LoadPatchJSON simply never looks at, so this needs no Sheaf change and
    // never disturbs a standalone-saved patch file (this plugin's session
    // blob and a standalone patch file share the format, not a saved
    // instance). PumpStatePersistence()'s own comment covers both
    // directions.
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- Test seam (retained) ---------------------------------------------
    // Dispatches the exact same kPlay/kStop actions
    // (FroggersUiSurface.hpp's `HandleAction`, via
    // engine_.Application().PortableSurface().DispatchAction()) the real
    // Play/Stop buttons dispatch -- see this file's header comment and
    // timerCallback()'s own comment for why this goes through
    // DispatchAction rather than hand-mirroring HandleAction's message
    // sequence. Not reachable from any host UI (no editor); exists solely
    // so FroggersVstSmokeTest.cpp can drive the core with no host/playhead
    // at all, the same way SynthRig::StartAt/StopAt do for the app core's
    // own test suite. Safe to call from any single thread that is not
    // concurrently calling processBlock() -- like every other UiBus
    // producer, it is not safe to call from two threads at once (see this
    // file's header comment on the SPSC contract); the smoke test drives it
    // single-threaded, so this holds.
    void TestStartTransport();
    void TestStopTransport();

    // --- 6.4 test seam ----------------------------------------------------
    // A headless CTest binary runs no JUCE message loop, so a real
    // juce::Timer started via startTimerHz() never fires on its own --
    // juce_events' dispatch loop is what calls timerCallback(), and nothing
    // in a plain `int main()` pumps it. Rather than stand up a
    // juce::MessageManager + a run-until-idle loop in the test binary (real,
    // but adds real flake risk: how long is "long enough" to wait for a
    // timer that fires every ~33ms is itself a race), this exposes
    // timerCallback() itself as the deterministic pump: production calls it
    // from the real Timer; app/vst/FroggersVstHostTests.cpp calls it
    // directly, synchronously, after driving processBlock() with a fake
    // playhead. Same function either way -- this is not a second, weaker
    // code path, just a second caller.
    void PumpMessageThreadForTest() { timerCallback(); }

    // Public (not private, unlike the activity/counter state it gates)
    // specifically so app/vst/FroggersVstHostTests.cpp's staleness-teardown
    // test can drive exactly this window rather than hardcoding a second
    // copy of the number that could silently drift from timerCallback()'s
    // own.
    //
    // Uses a TIME-based window rather than a fixed pump count: a fixed
    // count is an unvalidated heuristic -- a scheduling-jitter spike, or a
    // host that legitimately pauses processBlock() delivery for a while but
    // is still very much alive (briefly idling a track, a slow buffer-size
    // renegotiation, etc.), would trip the exact same "host is gone"
    // disengage as a real teardown, producing a visible tempo-display blip
    // (disengage, then immediately re-engage on the next usable block) for
    // no real host departure. Uses the SAME NowMicros() clock this class
    // already uses everywhere else (startTime_-relative monotonic
    // microseconds -- see that member's own comment), widened to ~1 second:
    // far outside ordinary pump-scheduling jitter (a full second is ~30
    // missed 30Hz pumps in a row, not a handful), while still disengaging
    // within one second of an actual teardown -- "prompt" is enough here
    // because nothing audio-critical depends on this deadline: this only
    // gates the host-tempo *display* slave, not audio output silencing,
    // which is FroggersAppCore's own transport-gated ASR, unaffected by
    // this window. The immediate releaseResourcesSeen_ path (below) remains
    // the primary, fast teardown trigger for the ordinary "host cleanly
    // tore the track down" case; this window only covers the case
    // releaseResources() is never called at all (see
    // releaseResourcesSeen_'s own comment).
    static constexpr std::uint64_t kStaleActivityWindowMicros = 1'000'000;

    // Test-only accessors (6.4): the real consumer/producer state
    // app/vst/FroggersVstHostTests.cpp asserts against, rather than a
    // second, weaker copy of it. ApplicationForTest() is the same
    // FroggersApp& TestStartTransport()/timerCallback() already drive
    // (DisplayTempoBpm()/TempoExternallyClocked()/FreezeLatched()/
    // TransportRunning()/RequestTempoBpm() -- all real, existing
    // FroggersAppCore API, see that file's own comments). UiBusPendingCount
    // ForTest() reads engine_.UiBus().Size() (MessageInBus::Size(),
    // External/Sheaf/projects/synth/include/synth/ParameterModulation.hpp:1022) -- the actual SPSC ring buffer 6.1/6.3
    // push onto, letting a test count messages AT THE BUS, without draining
    // them (draining only happens inside
    // engine_.ProcessBlock(), i.e. only when the test itself calls
    // processBlock() again).
    synth_froggers::FroggersApp& ApplicationForTest() { return engine_.Application(); }
    // Not const: engine_.UiBus() itself has no const overload (Engine.hpp).
    std::size_t UiBusPendingCountForTest() { return engine_.UiBus().Size(); }

    // Test-only accessor: the canonical input-selection index a
    // test asserts against, rather than reaching into the portable
    // surface's own rendered copy (SetInputOptions()'s own comment,
    // FroggersUiSurface.hpp, on why that copy exists at all). 0 is always
    // "None"; see ComputeInputOptionLabels()'s own comment for what the
    // rest of the index space means.
    int InputSelectionForTest() const { return inputSelection_; }

    // Test-only accessor: the SAME option list ApplyInputSelection() just
    // pushed to the portable surface (ComputeInputOptionLabels() is
    // private, and reaching into the surface's own rendered copy would be
    // the weaker, indirect check InputSelectionForTest()'s own comment
    // above already rejects for the selection index). Lets a test assert
    // the option list's actual contents -- not just its length -- for a
    // given live bus shape.
    std::vector<std::string> InputOptionLabelsForTest() const { return ComputeInputOptionLabels(); }

    // Resolves one block's operator-selected input into exactly ONE logical
    // channel -- FroggersAppCore::Config()'s own numAudioInputs==1, so the
    // core must never see, let alone choose among, the raw bus's own
    // channels. Mirrors ComputeInputOptionLabels()'s own index scheme:
    // `selection` <= 0 or `numChannels` <= 0 is None (nothing resolved,
    // returns false, `out` untouched); 1..numChannels picks that one
    // channel verbatim (a straight copy, no mixing); numChannels+1 is Sum,
    // the per-sample total across every channel -- present in the option
    // list (and therefore reachable here) only when numChannels > 1. `out`
    // must hold at least `numSamples` floats. processBlock() (below) is the
    // one production caller, feeding it the real bus's channel pointers and
    // `inputSelection_`; public and static (stateless, no `this`) so
    // FroggersVstHostTests.cpp can also drive it directly with synthetic
    // multi-channel arrays. Stays general (never special-cased to a fixed
    // channel count) so it keeps matching ComputeInputOptionLabels()'s own
    // generality across whatever width the bus negotiates.
    static bool ResolveSelectedInputChannel(const float* const* channels, int numChannels, int selection,
                                             int numSamples, float* out);

    // The host-parameter surface tests need no new test-only accessors beyond what's already
    // public: every exposed host parameter is a plain
    // juce::AudioProcessorParameter reachable via the standard
    // juce::AudioProcessor::getParameters() (public JUCE API, this class
    // adds nothing on top of it), and its ground truth on the core side is
    // ApplicationForTest().Parameters() (FroggersAppCore::Parameters(),
    // already public) -- see BuildHostParameterInventory()'s own comment
    // for the exact stable-ID scheme a test reconstructs to find a specific
    // parameter, and PumpHostParameterBridge()'s own comment for why a
    // juce::AudioProcessorParameter::Listener (standard JUCE, added by the
    // test itself) is the right tool to observe this bridge's
    // setValueNotifyingHost() traffic rather than a bespoke counter here.

    // -- Editor render-host seam ---------------------------------------------
    // The exact synth::ui::Surface& FroggersPluginEditor renders through
    // synth_juce::PortableComponent -- the SAME instance DispatchAction()/
    // TestStartTransport()/PumpHostParameterBridge() already drive via
    // Application().PortableSurface() (Froggers.hpp's `PortableSurface`), so the editor
    // observes/drives the live production surface with no second copy and
    // no new plumbing. Named for its real caller (NOT "...ForTest()", unlike
    // ApplicationForTest() above) because this IS the production accessor;
    // FroggersVstHostTests.cpp's own tests use ApplicationForTest().
    // PortableSurface() instead, by the same naming logic in reverse.
    synth::ui::Surface& EditorSurface() { return engine_.Application().PortableSurface(); }

    // Mirrors synth_runtime::Runtime<App>::SetRepaintHook (Sheaf
    // External/Sheaf/projects/synth/runtime/Runtime.hpp:500) -- the SAME single-slot "the message-thread
    // timer that already drives per-tick work also drives a UI repaint"
    // idiom this class's own timerCallback() follows for
    // engine_.MessageThreadTick() (see this file's header comment, "Tick
    // order mirrors Sheaf Runtime.hpp's own timerCallback()"). Exactly one
    // editor can be open at a time (JUCE's own AudioProcessorEditor
    // contract: a host calls createEditor() again only after the previous
    // editor has been destroyed), so a single slot -- not a list -- is the
    // right shape, same as Runtime<App>'s own repaintHook_. The editor
    // registers this in its constructor and MUST clear it (pass {}) in its
    // destructor BEFORE any of its own members (in particular the
    // PortableComponent this hook calls into) finish tearing down -- same
    // ordering contract synth_runtime::RuntimeShellSession's destructor
    // documents for this exact hook shape (External/Sheaf/projects/synth/runtime/Shell.hpp:91-95). Plain
    // std::function, no atomic: both the write (the editor's ctor/dtor) and
    // the read (timerCallback(), below) happen only ever on the message
    // thread -- editors are host-constructed/destroyed on the message
    // thread by JUCE's own contract, and timerCallback() only ever runs
    // there too, so the two can never interleave, only be ordered (which
    // the editor's own destructor comment covers).
    void SetEditorRepaintHook(std::function<void()> hook) { editorRepaintHook_ = std::move(hook); }

private:
    // juce::Timer override (private per the private-inheritance idiom
    // Sheaf's own Runtime.hpp uses, `class Runtime : private
    // juce::AudioIODeviceCallback, private juce::Timer` -- External/Sheaf/projects/synth/runtime/Runtime.hpp:100).
    // Pumps engine_.MessageThreadTick() every tick, same call Runtime.hpp's
    // own timerCallback() makes first (External/Sheaf/projects/synth/runtime/Runtime.hpp:975) -- non-realtime-safe
    // work (patch IO, serialization arena growth, MIDI-out processor pumps)
    // that must never run on the audio thread. Also the ONLY place this
    // class calls
    // engine_.UiBus().Push() or engine_.RequestSyncConfiguration() -- see
    // this file's header comment on why processBlock() itself must not.
    void timerCallback() override;

    // -- host transport edge-trigger -----------------------------------
    // processBlock() (audio thread) is the only place allowed to call
    // getPlayHead()->getPosition() (juce_AudioPlayHead.h's own doc comment:
    // "You can ONLY call this from your processBlock() method!"), so it is
    // also the only place that can DETECT a host play-state transition. It
    // may not PUSH the resulting Start/Stop message itself (see this file's
    // header comment) -- so it records at most one pending edge here, a
    // single-slot atomic exactly like FroggersAppCore's own
    // pendingBankSelect_/pendingEncoderPress_ idiom
    // (FroggersAppCore.hpp's own comment on `RequestBankSelect`: "a single-slot pending
    // request; a later write ... simply coalesces (acceptable: ...
    // control-rate, human-paced actions, never a data stream)" -- a host
    // transport toggle is exactly that kind of action). timerCallback()
    // claims it with exchange() and, if non-empty, pushes the mirrored
    // Play/Stop-button message sequence.
    enum class PendingTransportEdge : int { kNone = 0, kStart = 1, kStop = 2 };
    std::atomic<int> pendingTransportEdge_{static_cast<int>(PendingTransportEdge::kNone)};
    // Audio-thread-owned (processBlock only): the last host play-state this
    // object observed, and whether it has observed one yet at all. Not
    // published anywhere -- only processBlock() ever reads or writes these,
    // so no atomics are needed for them (contrast pendingTransportEdge_
    // above, which crosses to the message thread).
    bool haveHostPlayingBaseline_ = false;
    bool lastHostIsPlaying_ = false;

    // -- host tempo via external-clock slaving -------------------------
    // Same audio-thread/message-thread split as 6.1 above: processBlock()
    // republishes what it read from the playhead into these atomics;
    // timerCallback() is the only reader, and the only thing that acts on
    // them (engine_.RequestSyncConfiguration()/engine_.UiBus().Push()).
    // hostTempoValid_ covers BOTH "no playhead at all" and "playhead present
    // but this host block reported no bpm" -- see processBlock()'s own
    // comment for why those collapse to the same "nothing usable this
    // block" signal down here.
    std::atomic<bool> hostTempoValid_{false};
    std::atomic<double> hostTempoBpm_{synth::MasterClock::kDefaultTempoBpm};
    // Message-thread-owned -- timerCallback() is the SOLE writer of
    // hostClockEngaged_/nextHostClockTickMicros_:
    // releaseResources() used to write hostClockEngaged_ directly, but JUCE
    // gives releaseResources() no thread guarantee at all
    // (juce_AudioProcessor.h's own declaration carries no thread
    // annotation, unlike processBlock()'s explicit "audio thread" contract)
    // while this class's 30Hz Timer fires independently on the message
    // thread -- two plain-bool writers on two different, unsynchronized
    // threads is a real data race (not merely a logical one), and could
    // leave the clock re-engaged immediately after releaseResources()
    // thought it had disengaged it. Fixed by routing BOTH triggers (the
    // Timer's own staleness detection AND releaseResources()) through one
    // atomic REQUEST (releaseResourcesSeen_ below) that only ever gates
    // what timerCallback() itself decides -- the alternative (making
    // hostClockEngaged_ itself atomic) would still leave two independent
    // threads racing to decide WHETHER to engage/disengage even if each
    // individual flag flip were data-race-free; funneling through a single
    // decision-maker removes the race at the decision level, not just at
    // the storage level.
    bool hostClockEngaged_ = false;
    std::uint64_t nextHostClockTickMicros_ = 0;

    // releaseResources() (any/unspecified thread,
    // see hostClockEngaged_'s own comment) sets ONLY this atomic; it never
    // touches hostClockEngaged_/nextHostClockTickMicros_/hostTempoValid_
    // itself. timerCallback() (message thread, sole owner of the fields
    // above) claims it with exchange() and treats it as an immediate
    // "treat the host as stale right now" signal -- same effect as the
    // staleness counter below reaching its threshold, just without waiting
    // for it.
    std::atomic<bool> releaseResourcesSeen_{false};

    // Teardown gap: the "disengage once
    // hostTempoUsable goes false" logic silently assumed processBlock()
    // keeps being called at all -- if a host suspends/disables this track
    // WITHOUT calling releaseResources(), processBlock() simply stops, so
    // hostTempoValid_/hostTempoBpm_ are never refreshed and stay stuck at
    // their last (possibly "usable") values forever, so hostClockEngaged_
    // never disengages either. processBlock() increments
    // processBlockCounter_ unconditionally, every call, as a pure liveness
    // heartbeat (it synchronizes no other data, so relaxed ordering is
    // enough on both ends); timerCallback() compares it against the value
    // it last observed to detect "processBlock has not run since the
    // previous pump" and, once kStaleActivityWindowMicros of REAL elapsed
    // time (not pump count) has passed with the counter unmoved, treats the
    // host as gone -- see kStaleActivityWindowMicros's own comment and
    // timerCallback()'s own comment for the full justification.
    std::atomic<std::uint64_t> processBlockCounter_{0};
    // Message-thread-owned (timerCallback only). lastObservedProcessBlockCounter_
    // is the last processBlockCounter_ value this pump observed CHANGE;
    // lastActivityMicros_ is the NowMicros() timestamp of the pump that last
    // observed that change -- i.e. "how long ago did processBlock() last
    // actually run," replacing the old fixed pump-count streak
    // (staleProcessBlockPumpStreak_, removed).
    // Zero-initialized: at construction time NowMicros() is itself ~0 (it is
    // relative to startTime_, set in the same constructor), so the very
    // first pump's elapsed-time computation starts from "just built," not
    // "already stale" -- same grace period the old streak-based version got
    // for free from starting its counter at 0.
    std::uint64_t lastObservedProcessBlockCounter_ = 0;
    std::uint64_t lastActivityMicros_ = 0;

    // -- stable-ID host parameter surface -------------------------------
    // One entry per exposed juce::AudioProcessorParameter: the 6*14 page
    // parameters + 6 per-bank Crispy + 1 shared Crunchy the model's own
    // enumeration produces (FroggersParameterModel -- see
    // BuildHostParameterInventory()'s own comment for how this is computed,
    // never hardcoded), plus one more for Freeze (not a ParameterManager
    // parameter at all -- see the Freeze branch of that method and of
    // PumpHostParameterBridge()).
    struct HostParamEntry {
        enum class Kind { kPageParam, kCrispy, kCrunchy, kFreeze };

        Kind kind = Kind::kPageParam;
        // Non-owning: juce::AudioProcessor::addParameter() (called once, in
        // BuildHostParameterInventory()) takes ownership and destroys this
        // along with every other parameter when the processor itself is
        // destroyed -- same lifetime contract as every other
        // juce::AudioProcessorParameter this codebase adds.
        juce::RangedAudioParameter* juceParam = nullptr;
        // Null only for kind == kFreeze -- Freeze is not a ParameterManager
        // parameter (see PumpHostParameterBridge()'s own comment on why its
        // bridge goes through DispatchAction(kFreeze)/FreezeLatched()
        // instead of this pointer).
        synth::Parameter* coreParam = nullptr;

        // Addressing for the host -> core write direction (see
        // PumpHostParameterBridge()'s own comment): bankIx/position identify
        // WHICH Parameter this entry targets, resolved directly against
        // bankIx's own top-level mapping (MessageIn::ParamSetAbsoluteOnBank)
        // regardless of which bank the shared BankSlot currently has
        // selected, or how deep that bank is drilled into a modulation
        // view (slotIx is always 0 -- FroggersParameterModel::Init() creates
        // exactly one BankSlot; it contributes only the encoder layout used
        // to resolve position). kCrunchy's bankIx is arbitrary (0): the SAME
        // Parameter object is registered at position 15 in every bank's own
        // top-level mapping, so any bankIx resolves it. kFreeze needs
        // neither field -- it bypasses the BankSlot/ParameterManager surface
        // entirely.
        std::size_t bankIx = 0;
        std::size_t position = 0;

        // Audio-thread-published (processBlock(), every block, via
        // Parameter::PopulateUIState() -- see processBlock()'s own comment)
        // / message-thread-read (PumpHostParameterBridge()) snapshot of
        // this parameter's current, post-fuego, post-modulation display
        // value -- Parameter::UIState::values[] is an atomic array
        // (ParameterModulation.hpp), safe for exactly this cross-thread
        // read/publish split. Null only for kind == kFreeze (FreezeLatched()
        // is already its own atomic -- no UIState needed).
        std::unique_ptr<synth::Parameter::UIState> uiState;

        // Message-thread-owned: the value BOTH sides last agreed on (either
        // "the host wrote this and we relayed it" or "the core settled to
        // this and we notified the host of it"). This is the single piece
        // of state PumpHostParameterBridge()'s feedback guard is built
        // around -- see that method's own comment for the full two-phase
        // per-parameter-per-pump algorithm.
        float shadowNormalized = 0.0f;
    };

    // Built once, in the constructor, right after engine_.Initialize() (the
    // point FroggersParameterModel's 91 Parameters and FroggersApp's
    // production surface first exist) -- see that method's own comment for
    // the full derivation.
    void BuildHostParameterInventory();
    // Message-thread-only (called from timerCallback(), see this file's
    // header comment on the audio-thread/message-thread split this class
    // maintains): both directions of the bridge, one
    // pass over hostParams_ per pump. See this method's own comment (in the
    // .cpp) for the full bidirectional trace and feedback-guard design.
    void PumpHostParameterBridge();

    // Message-thread-only (called from timerCallback(), same discipline as
    // PumpHostParameterBridge() above): both directions of DAW
    // session-state persistence. See this method's own comment (in the
    // .cpp) for the full trace.
    void PumpStatePersistence();

    std::vector<HostParamEntry> hostParams_;

    // See SetEditorRepaintHook()'s own comment above for the full
    // precedent trace. Empty (default-constructed, falsy) whenever no
    // editor is open -- the `if (editorRepaintHook_)` check in
    // timerCallback() then costs one branch and no repaint work.
    std::function<void()> editorRepaintHook_;

    // -- DAW session-state persistence ---------------------------------------
    // getStateInformation()/setStateInformation() carry no JUCE thread
    // guarantee (juce_AudioProcessor.h's own declaration carries no thread
    // annotation for either, unlike processBlock()'s explicit audio-thread
    // contract -- the same asymmetry releaseResources() has, see that
    // method's own comment above), so a host may call them from any thread,
    // concurrently with timerCallback() (always the message thread).
    // stateBlockMutex_ guards every field below that both a host-calling
    // thread and the message thread could otherwise touch at once -- the
    // same reason Engine.hpp's own audioDeviceStateMutex_ exists, for its
    // own occasional, non-realtime, cross-thread fields.
    std::mutex stateBlockMutex_;

    // Guarded by stateBlockMutex_. The most recently completed full-fidelity
    // session snapshot, already serialized to JSON text --
    // getStateInformation() copies this out and returns immediately; it
    // never blocks waiting for a fresh one. Seeded synchronously in the
    // constructor (safe pre-audio, mirroring engine_.Initialize()'s own
    // synchronous startup patch drain) and refreshed roughly once per
    // PumpStatePersistence() pump thereafter, so it is never more than
    // about one pump interval stale -- the same consistency bound
    // PumpHostParameterBridge() already accepts for host-parameter
    // readback.
    std::string cachedStateJsonText_;

    // Guarded by stateBlockMutex_. A restore setStateInformation() deposited
    // but PumpStatePersistence() -- the sole legitimate
    // engine_.Context().patchInputBus producer, see that method's own
    // comment -- has not yet claimed. Cleared once claimed.
    std::optional<std::string> pendingRestoreJsonText_;

    // Message-thread-owned (PumpStatePersistence() only): tracks a
    // SerializeToJSON request this class itself issued but has not yet
    // consumed the response for, so a second one is never issued while one
    // is outstanding. nextStateRequestId_ is this class's own monotonically
    // increasing request-ID source for these requests.
    std::optional<std::uint64_t> pendingStateSnapshotRequestId_;
    std::uint64_t nextStateRequestId_ = 1;

    std::uint64_t NowMicros() const;

    // This plugin's own storage for the "is an input actually
    // routed in" signal FroggersAppCore reads via synth::AppContext::
    // InputRouted()/SetInputRoutedChangedCallback() -- the exact same
    // synth::InputRoutingSignal type and seam the standalone host's
    // Runtime.hpp uses (its own `inputRoutingSignal_` member, wired the
    // same way -- see the constructor's own comment in the .cpp).
    // Declared BEFORE engine_ (not after): C++ destroys members in reverse
    // declaration order, and FroggersAppCore::~FroggersAppCore() (run
    // during engine_'s own destruction) unregisters its callback through
    // the pointer wired into engine_.Context().inputRoutingSignal --
    // inputRoutingSignal_ must therefore still be alive when engine_ tears
    // down, i.e. destroyed AFTER it, i.e. declared BEFORE it here.
    synth::InputRoutingSignal inputRoutingSignal_;

    // Derives this plugin's input-channel option list from the
    // LIVE JUCE bus (isEnabled()/getNumberOfChannels()/getCurrentLayout(),
    // never a cached copy) -- index 0 is always "NONE"; indices 1..N are
    // one entry per channel the bus currently provides (name via
    // juce::AudioChannelSet); index N+1 is "SUM", present only when N > 1.
    // Called fresh every time the option set might have changed -- never
    // cached across a layout change, the exact discipline required ("never
    // read a channel because a stored value names it").
    std::vector<std::string> ComputeInputOptionLabels() const;

    // The single write path for the operator's input-channel
    // selection, called from three places that must all funnel through the
    // SAME re-validation -- the constructor (seeds "None" against the
    // bus's initial, disabled shape), processorLayoutsChanged() (re-derives
    // the option list against the bus's NEW shape and re-validates the
    // CURRENT selection against it), and the portable surface's own
    // input-select action (the operator's tap, cycling to the next option
    // it last rendered). `selectionIndex` is bounds-checked against a
    // FRESH ComputeInputOptionLabels() here, not trusted from the caller --
    // an index that no longer names a real option (a layout change removed
    // it, or a restored session named one the current bus does not have)
    // falls back to index 0 ("None"), never left pointing at nothing.
    // Publishes the resulting connected state -- selection != None, NEVER
    // bus-enabled-with-channels -- into inputRoutingSignal_ (avoiding the
    // phantom-input defect a stale, unrevalidated selection would cause),
    // and pushes the freshly-derived labels/selection into the portable
    // surface so the rendered control never lags what was just computed.
    void ApplyInputSelection(int selectionIndex);

    // This plugin's own canonical copy of the operator's
    // input-channel selection -- an index into ComputeInputOptionLabels()'s
    // own return value (0 == "None", the default). Read directly by
    // PumpStatePersistence()'s snapshot side and InputSelectionForTest();
    // written ONLY by ApplyInputSelection() above, so no path can set it to
    // an unvalidated value.
    int inputSelection_ = 0;

    // Scratch storage for ResolveSelectedInputChannel()'s output -- this
    // block's single resolved input channel, built fresh every processBlock()
    // call before `block` is constructed. Sized in prepareToPlay() (to the
    // host's negotiated block size) and defensively re-sized in
    // processBlock() itself if a host ever delivers more samples than that
    // (JUCE's own contract does not guarantee processBlock() never exceeds
    // the size prepareToPlay() announced, only that it usually does not).
    std::vector<float> resolvedInputScratch_;

    std::chrono::steady_clock::time_point startTime_;
    synth::Engine<synth_froggers::FroggersApp> engine_;
};

}  // namespace frogg3rs_vst
