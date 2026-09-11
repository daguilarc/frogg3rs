#include "FroggersPluginProcessor.hpp"

// The real editor createEditor() below constructs, and the
// FroggersUiSurface downcast the constructor uses to call
// SetPluginHostMode() (see that call site's own comment).
#include "FroggersPluginEditor.hpp"
#include "FroggersUiSurface.hpp"
// Only FroggersManifest() (the app's own single-sourced identity, appId
// "frogg3rs") -- see ProductionDataPaths() below for why. JUCE-free itself
// (that file's own header comment), so this adds no new dependency weight.
#include "FroggersRegistration.hpp"
// BuildPatchJSON/LoadPatchJSON, PatchMessageIn/MessageOut, JsonArena --
// already transitively visible via synth/Engine.hpp's own include chain
// (Engine.hpp includes synth/ParameterModulation.hpp, which includes
// synth/Json.hpp, before its own synth/PatchPersistence.hpp include), named
// explicitly here since PumpStatePersistence() below names these types
// directly. See that method's own comment for why DAW session-state
// persistence reuses this format rather than inventing one.
#include "synth/PatchPersistence.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace frogg3rs_vst {

namespace {

// Mirrors app/FroggersMain.cpp's `FroggersMainApplication::initialise`'s dataRoot_/SheafPatchDataPathsForApp
// pair (same stable app id -- see that file's own header comment on why:
// "so existing saved patches ... are not orphaned"), so a patch saved from
// the standalone Frogg3rs app and one saved from this plugin land in, and
// load from, the same ~/Library/Sheaf/synth/sheaf-patch/patches/frogg3rs/
// directory. Reads the id from FroggersRegistration.hpp's own
// FroggersManifest().appId -- the app's existing single-sourced identity,
// already how FroggersApp registers with the launcher -- rather than
// restating "frogg3rs" as a second, independent literal here. app/
// FroggersMain.cpp's own pairing still carries its own separate literal
// (pre-existing, out of scope for this app/vst file to touch).
synth::RuntimeDataPaths ProductionDataPaths() {
    const std::filesystem::path dataRoot = synth_runtime::SheafUserApplicationDataRoot();
    return synth::SheafPatchDataPathsForApp(dataRoot, synth_froggers::FroggersManifest().appId);
}

// -- stable host-parameter IDs --------------------------------------
// Derived ONLY from structural facts of FroggersParameterModel that are
// fixed by design and documented as such in app/FroggersParameters.hpp,
// never from iteration order, display strings, or anything that shifts
// when the model grows:
//   - bankIx: FroggersBankId's own enum value (Audio=0 .. Reverb=5,
//     FroggersParameters.hpp) -- a named, documented identity per bank, not
//     an incidental loop index (it IS a loop index here too, but one that
//     is guaranteed to match FroggersBankId's own fixed assignment, since
//     FroggersParameterModel::Init() creates banks in exactly that order
//     and nothing else creates banks on this manager).
//   - position: the parameter's fixed 0-13 slot within its bank's
//     14-parameter row (kFroggersParamsPerBank -- a structural layout fact,
//     independent of that slot's current display name; the file's own
//     header comment cites a real precedent for this exact stability
//     requirement -- "Ph.mod 1" was renamed from "Phase mod N" with its
//     slot/default left unchanged), or the named kFroggersCrispySlot (14) /
//     kFroggersCrunchySlot (15) constants already used throughout that file
//     for exactly this purpose.
// A bank/slot rename, or a defaultValue/color tweak, changes none of these
// three inputs -- exactly the "stable across sessions and releases"
// requirement the governing spec states. Freeze is not part of the model at
// all (see HostParamEntry::Kind::kFreeze's own comment); "crunchy" needs no
// bank/slot qualifier since exactly one exists, globally, in the whole
// model (FroggersParameters.hpp).
juce::String HostParamStableId(std::size_t bankIx, std::size_t paramIx) {
    return "bank" + juce::String(static_cast<int>(bankIx)) + ".slot" + juce::String(static_cast<int>(paramIx));
}

juce::String HostParamStableIdCrispy(std::size_t bankIx) {
    return "bank" + juce::String(static_cast<int>(bankIx)) + ".crispy";
}

constexpr const char* kHostParamStableIdCrunchy = "crunchy";
constexpr const char* kHostParamStableIdFreeze = "freeze";

// DAW session-state persistence (PumpStatePersistence()): the patchName
// field of the shared "sheaf.synth.patch" envelope (BuildPatchJSON,
// synth/PatchPersistence.hpp). LoadPatchJSON only requires this field to be
// a string -- its content plays no role in whether a document loads -- so
// one fixed, descriptive name is enough; it never becomes a filename (this
// class never writes the snapshot to disk).
constexpr const char* kSessionStatePatchName = "daw-session";

// DAW session-state persistence: the Freeze latch sibling key (see
// getStateInformation()'s own header comment, FroggersPluginProcessor.hpp,
// for why it lives outside "parameterValues"). Named once here and reused
// at every read/write site so the two never drift apart.
constexpr const char* kSessionExtrasKey = "sessionExtras";
constexpr const char* kFreezeLatchedKey = "freezeLatched";
// Second sessionExtras sibling key: the operator's visible bank (the page
// FroggersUiSurface::CurrentBankIndex() reports as selected), so reopening
// a saved DAW project restores the page the operator was last on. Same
// object, same round trip, no new mechanism -- see this key's own read/
// write sites (PumpStatePersistence()) for the accessor/authority each
// direction uses.
constexpr const char* kVisibleBankIndexKey = "visibleBankIndex";
// Third sessionExtras sibling key: the operator's input-channel
// selection -- an index into FroggersPluginProcessor::
// ComputeInputOptionLabels()'s own return value (0 == "None"). Same object,
// same round trip, no new mechanism -- see this key's own read/write sites
// (PumpStatePersistence()) for the accessor/authority each direction uses,
// and ApplyInputSelection()'s own comment for why the restore side
// bounds-checks against the CURRENT bus rather than trusting the stored
// index. Missing key (a blob saved before this change existed) restores to
// 0 ("None"), the same default a fresh session already starts at -- opt-in
// audio is off until the operator affirmatively selects a channel again.
constexpr const char* kInputSelectionKey = "inputSelection";

// A present-but-wrong-typed value is treated the same as an absent one
// (left alone) rather than silently coerced to false -- IsNull() alone
// cannot tell the two apart, since JSON::BooleanValue() already folds
// "wrong type" into its own false fallback. Mirrors the strict, type-first
// checking synth::PatchPersistence.cpp's own IsBoolean() does for the rest
// of a patch document.
bool IsJsonBoolean(synth::JSON json) { return json.m_node != nullptr && json.m_node->m_type == synth::JsonType::Boolean; }
// Same strict, type-first treatment as IsJsonBoolean() above, for the
// bank-index sibling key: a missing or wrong-typed value is left alone
// rather than coerced to 0 via JSON::IntegerValue()'s own fallback.
bool IsJsonInteger(synth::JSON json) { return json.m_node != nullptr && json.m_node->m_type == synth::JsonType::Integer; }

}  // namespace

FroggersPluginProcessor::FroggersPluginProcessor()
    : FroggersPluginProcessor(ProductionDataPaths()) {}

FroggersPluginProcessor::FroggersPluginProcessor(synth::RuntimeDataPaths dataPathsForTest)
    // One OPTIONAL stereo input bus alongside the existing stereo output.
    // The third `withInput` argument (`isActivatedByDefault =
    // false`) declares the bus present but disabled until a host
    // explicitly enables it -- JUCE's own mechanism for a bus an
    // instrument still instantiates and still makes sound with left
    // disconnected. isBusesLayoutSupported() (below) accepts disabled(),
    // mono(), or stereo() for this bus, so a host may enable it at either
    // width; ComputeInputOptionLabels()/ResolveSelectedInputChannel() read
    // whatever the negotiated layout actually provides.
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), false)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , startTime_(std::chrono::steady_clock::now())
    , engine_([this] { return NowMicros(); }) {
    // Runtime.hpp Start() order (:223,230, this file's header comment):
    // SetRuntimeDataPaths() BEFORE Initialize() -- startup patch/config
    // discovery reads dataPaths_ during Initialize() (Engine.hpp:213-222's
    // own doc comment on step 8).
    engine_.SetRuntimeDataPaths(std::move(dataPathsForTest));
    // Wires this plugin's own InputRoutingSignal into the AppContext
    // BEFORE engine_.Initialize() below ever runs App::Init() -- mirrors
    // Runtime.hpp's constructor (`engine_.Context().inputRoutingSignal =
    // &inputRoutingSignal_;`, set before Start()/Initialize() can run
    // App::Init()), so FroggersAppCore::Init()'s context_->InputRouted()
    // read and context_->SetInputRoutedChangedCallback() registration see
    // this same live signal -- the one processorLayoutsChanged() (below)
    // publishes into -- rather than a null pointer (AppContext::
    // InputRouted() reads false when unset). inputRoutingSignal_ is
    // declared before engine_ in this class's header (see that member's
    // own comment) so it is already constructed here and outlives engine_
    // through teardown, including FroggersAppCore::~FroggersAppCore()'s
    // own unregistration through this same pointer.
    engine_.Context().inputRoutingSignal = &inputRoutingSignal_;
    // Called ONCE, in the constructor -- not in prepareToPlay(), which JUCE
    // may call repeatedly (sample-rate/block-size renegotiation). Mirrors
    // Runtime::Start() calling engine_.Initialize() once, before any audio
    // device exists, separately from the per-negotiation engine_.Prepare()
    // in audioDeviceAboutToStart (Runtime.hpp:230,594-599).
    engine_.Initialize();

    // This is SetPluginHostMode()'s first PRODUCTION
    // call site (app/FroggersUiSurface.hpp's own comment on that method
    // names this exact call: "The plugin host (the VST editor) calls
    // SetPluginHostMode(true) before/at attach time" -- previously
    // test-only, FroggersVstHostTests.cpp's BuildFroggersTree()). Called
    // here, right after engine_.Initialize() (i.e. as early as the surface
    // object exists at all), on the SAME instance EditorSurface() exposes
    // to the editor and PortableSurface() exposes to DispatchAction()
    // elsewhere in this class -- so every rebuild (BuildTree() reruns every
    // frame, that method's own comment) renders plugin mode from the very
    // first frame, with no separate opt-in the editor itself has to
    // remember to perform.
    //
    // SetPluginHostMode() is FroggersUiSurface-specific (a runtime flag
    // AppendTransportRow() reads, not part of the generic synth::ui::
    // Surface interface PortableSurface()/EditorSurface() expose) --
    // reaching it needs exactly one downcast. static_cast, not
    // dynamic_cast: engine_ is concretely synth::Engine<synth_froggers::
    // FroggersApp> (this class's own member type above), so
    // engine_.Application() is concretely FroggersApp&, and
    // FroggersApp::PortableSurface() (Froggers.hpp's `PortableSurface`) is defined as
    // `return ui_;` over its own declared-concrete-type member
    // (`FroggersUiSurface ui_;`, Froggers.hpp's `ui_`) -- so the object
    // PortableSurface() returns a reference to is ALWAYS, provably, a
    // FroggersUiSurface; a runtime check here would guard a branch that
    // cannot exist without an edit to Froggers.hpp itself -- protect only
    // against real edge cases; a proven-safe static relationship is this
    // codebase's own established idiom for exactly this shape, e.g.
    // app/FroggersMain.cpp's own comment on activeSession_'s concrete
    // type).
    static_cast<synth_froggers::FroggersUiSurface&>(engine_.Application().PortableSurface())
        .SetPluginHostMode(true);

    // The operator's input-channel selector, same downcast
    // reasoning and same instance as SetPluginHostMode() immediately above.
    // The callback is the surface's only write path back to this class
    // (the operator's tap, HandleAction's kInputSelect branch,
    // FroggersUiSurface.hpp): it hands back the NEXT option index the
    // surface just cycled to, and ApplyInputSelection() below is what
    // re-validates it, applies it, and publishes the resulting connected
    // state -- the surface itself never decides connectedness, only which
    // index the operator asked for next. Registered before the seed call
    // below so that call's own SetInputOptions() push has somewhere to land
    // (SetInputSelectionChangedCallback() itself never fires from a plain
    // registration).
    static_cast<synth_froggers::FroggersUiSurface&>(engine_.Application().PortableSurface())
        .SetInputSelectionChangedCallback([this](int selectionIndex) { ApplyInputSelection(selectionIndex); });
    // Seeds the surface with the bus's initial (disabled, per the
    // constructor's own BusesProperties comment above) option list -- just
    // "None" -- and publishes the matching connected=false into
    // inputRoutingSignal_, so the very first BuildTree() the editor ever
    // renders already shows the real bus shape rather than an empty
    // default. inputSelection_ is already 0 (its NSDMI) at this point, so
    // this call changes nothing about consent -- it only makes the already-
    // true "not connected" state visible to the surface and explicit in
    // inputRoutingSignal_ rather than assumed.
    ApplyInputSelection(inputSelection_);

    // FroggersParameterModel's Parameters (and
    // FroggersApp's production DispatchAction seam, for Freeze) only exist
    // once engine_.Initialize() has returned (app_.Init() runs inside it,
    // this file's header comment) -- so the host parameter inventory is
    // built HERE, once, immediately after, and nowhere else.
    BuildHostParameterInventory();

    // Seed the session-state cache synchronously so getStateInformation()
    // never returns empty state, even if a host calls it before this
    // class's timer has pumped even once. Safe ONLY here: no audio thread
    // is running yet at this point in construction, the same pre-audio,
    // single-threaded window engine_.Initialize() itself already relies on
    // for its own synchronous startup-patch drain (Engine.hpp's own
    // Initialize() comment, step 8) -- PumpStatePersistence()'s own comment
    // covers the steady-state, audio-thread-mediated refresh path this
    // seed bypasses here.
    {
        constexpr std::size_t kInitialArenaCapacity = synth::PatchSerializationContext{}.initialArenaCapacity;
        synth::JsonArena arena(kInitialArenaCapacity);
        synth::JSON root =
            synth::BuildPatchJSON(arena, kSessionStatePatchName, engine_.Manager(), synth::MidiInstrumentConfig{});
        if (!root.IsNull() && !arena.Failed()) {
            // Sibling key, attached after BuildPatchJSON returns (see
            // getStateInformation()'s own header comment in the .hpp) --
            // FreezeLatched() defaults false and no restore has happened
            // yet at this point in construction, so this seeds the cache
            // with the instrument's actual current latch state, the same
            // way engine_.Manager() above supplies its actual current
            // parameter values rather than assumed defaults. Arena
            // exhaustion here is handled the same way BuildPatchJSON's own
            // internal SetNew calls are (Json.hpp's own "null-tolerant"
            // build contract): SetNew silently drops the key instead of
            // corrupting the rest of the document, which degrades to
            // exactly the "no sessionExtras key" case restore already has
            // to handle.
            synth::JSON sessionExtras = arena.Object();
            sessionExtras.SetNew(kFreezeLatchedKey, arena.Boolean(engine_.Application().FreezeLatched()));
            // Same sibling-key treatment as kFreezeLatchedKey above, seeded
            // with the visible bank's own actual current value (0, the
            // default FroggersParameterModel::Init() selects, this early)
            // rather than an assumed constant -- see PumpStatePersistence()'s
            // steady-state write of this same key for the accessor this
            // mirrors.
            sessionExtras.SetNew(kVisibleBankIndexKey,
                                  arena.Integer(static_cast<std::int64_t>(synth_froggers::FroggersVisibleBankIndex(engine_.Context()))));
            // Same sibling-key treatment, seeded with inputSelection_'s own
            // actual current value (0, "None" -- ApplyInputSelection() has
            // already run once by this point in the constructor, above)
            // rather than an assumed constant.
            sessionExtras.SetNew(kInputSelectionKey, arena.Integer(static_cast<std::int64_t>(inputSelection_)));
            root.SetNew(kSessionExtrasKey, sessionExtras);
            if (char* dumped = root.Dumps(JSON_ENCODE_ANY)) {
                cachedStateJsonText_ = dumped;
                std::free(dumped);
            }
        }
    }

    // Pump engine_.MessageThreadTick() from a message-thread juce::Timer,
    // same as Sheaf Runtime.hpp (Runtime.hpp:343 starts it at
    // `config.uiFrameHz > 0 ? ... : 30`; this plugin has no equivalent
    // config knob, so 30Hz directly). Started once, here, in the
    // constructor -- mirrors Runtime::Start() calling startTimerHz() once
    // (Runtime.hpp:343), not per prepareToPlay().
    //
    // juce::Timer::startTimer() (which startTimerHz() calls) itself asserts
    // JUCE_ASSERT_MESSAGE_MANAGER_EXISTS in Debug builds
    // (JUCE's own juce_Timer.cpp, "If you're calling this before (or after)
    // the MessageManager is running, then you're not going to get any
    // timer callbacks!") -- true for every headless CTest binary in this
    // app/vst/ test suite (FroggersVstSmokeTest.cpp/
    // FroggersVstHostTests.cpp construct this class directly, no host, no
    // juce::MessageManager ever created). Guarded rather than left to trip:
    // a real host is guaranteed to have a MessageManager running before it
    // constructs any juce::AudioProcessor (every plugin-client entry point
    // runs on the message thread), so this guard is a no-op in production;
    // those same headless tests never need the REAL timer to fire anyway,
    // since PumpMessageThreadForTest() (this file's own header comment)
    // drives timerCallback() directly and deterministically.
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr) {
        startTimerHz(30);
    }
}

FroggersPluginProcessor::~FroggersPluginProcessor() {
    // juce::Timer::stopTimer()'s own doc comment: "No more timer callbacks
    // will be triggered after this method returns" -- true unconditionally
    // when called from the message thread (the expected case for plugin
    // teardown, and the same thread every timerCallback() runs on, so there
    // is no concurrent in-flight call to race here). Called explicitly,
    // before any member (in particular engine_, which timerCallback()
    // touches) begins tearing down.
    stopTimer();
}

// One FroggersPluginEditor per call, exactly the
// juce::AudioProcessor::createEditor() contract (a fresh juce::Component the
// HOST owns and destroys -- see juce_AudioProcessor.h's own doc comment).
// Defined here rather than inline in the header so the header itself never
// needs to include FroggersPluginEditor.hpp's own JUCE-GUI chain
// (PortableJuceBackend.hpp et al.) -- every OTHER app/vst/ translation unit
// that only needs FroggersPluginProcessor.hpp (FroggersVstSmokeTest.cpp,
// most of FroggersVstHostTests.cpp) stays exactly as GUI-free to compile as
// before.
juce::AudioProcessorEditor* FroggersPluginProcessor::createEditor() { return new FroggersPluginEditor(*this); }

void FroggersPluginProcessor::getStateInformation(juce::MemoryBlock& destData) {
    std::string text;
    {
        const std::lock_guard<std::mutex> lock(stateBlockMutex_);
        text = cachedStateJsonText_;
    }
    // Replaces destData wholesale rather than appending: JUCE's own
    // getStateInformation() doc comment does not promise the host passes
    // an empty block, and MemoryBlock::append() would silently corrupt any
    // pre-existing content into invalid JSON instead of overwriting it.
    destData = juce::MemoryBlock(text.data(), text.size());
}

void FroggersPluginProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (data == nullptr || sizeInBytes <= 0) {
        return;
    }
    std::string text(static_cast<const char*>(data), static_cast<std::size_t>(sizeInBytes));
    const std::lock_guard<std::mutex> lock(stateBlockMutex_);
    pendingRestoreJsonText_ = std::move(text);
}

std::uint64_t FroggersPluginProcessor::NowMicros() const {
    // Same derivation as Runtime.hpp's own NowMicros() (:986-990): a
    // monotonic microsecond counter relative to construction time, not
    // wall-clock time -- MasterClock/message-batching only need monotonic
    // ordering and real elapsed duration between calls, not an absolute
    // epoch.
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTime_).count());
}

void FroggersPluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    // Runtime.hpp's audioDeviceAboutToStart (:594-599): engine_.Prepare() is
    // called on EVERY negotiation, not just the first -- see
    // FroggersAppCore::PrepareToPlay()'s own comment on why re-preparing
    // mid-session is a real, expected event (and why it re-asserts a
    // previously-started transport via desiredTransportRunning_).
    engine_.Prepare(sampleRate, samplesPerBlock);

    // resolvedInputScratch_'s own comment (header): sized to the host's
    // negotiated block size here; processBlock() still re-checks its own
    // size defensively, since JUCE does not guarantee processBlock() never
    // exceeds it.
    resolvedInputScratch_.assign(static_cast<std::size_t>(std::max(0, samplesPerBlock)), 0.0f);
}

void FroggersPluginProcessor::releaseResources() {
    // No traced teardown for audio state itself: Runtime.hpp's
    // audioDeviceStopped() override (:614) only clears a diagnostic counter
    // (activeInputChannels_) that this plugin has no equivalent of (zero
    // input channels, by construction). synth::Engine/FroggersAppCore
    // expose no stop/release-type hook at all -- ProcessBlock's own
    // transport-gated ASR (FroggersAppCore.hpp) is what silences output on
    // transport stop, not a host-driven teardown call.
    //
    // This must not write hostClockEngaged_/hostTempoValid_ directly: JUCE
    // gives releaseResources() no thread guarantee at all
    // (juce_AudioProcessor.h's own declaration carries no thread
    // annotation, unlike processBlock()'s explicit audio-thread contract),
    // while this class's 30Hz Timer fires independently on the message
    // thread -- two plain writers on two possibly-different, unsynchronized
    // threads racing to decide the SAME engage/disengage state is a real
    // bug (could leave the clock re-engaged immediately after this call
    // thought it had disengaged it), not just an untidy one. Instead, this
    // call sets ONLY its own dedicated atomic request
    // (releaseResourcesSeen_) -- timerCallback() (message thread) remains
    // the sole reader/writer of hostClockEngaged_/nextHostClockTickMicros_/
    // hostTempoValid_, exactly like every other piece of transport/tempo
    // state that crosses threads in this class (see this file's own header
    // comment on the audio-thread/message-thread split).
    releaseResourcesSeen_.store(true, std::memory_order_release);
}

bool FroggersPluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // Bus posture: stereo output, plus ONE OPTIONAL input bus (the
    // constructor's BusesProperties above declares it, disabled by
    // default -- see that comment). A host may leave the input bus at
    // disabled() (the plugin still instantiates and makes sound), or
    // enable it at mono() or stereo(); anything wider, or a non-stereo
    // output, is rejected.
    const juce::AudioChannelSet mainInput = layouts.getMainInputChannelSet();
    if (mainInput != juce::AudioChannelSet::disabled() && mainInput != juce::AudioChannelSet::mono()
        && mainInput != juce::AudioChannelSet::stereo()) {
        return false;
    }
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

std::vector<std::string> FroggersPluginProcessor::ComputeInputOptionLabels() const {
    // Index 0 is always "None" -- the only option when the bus is disabled
    // or (defensively) reports zero channels, and required as the first
    // entry regardless of bus shape ("the only option is None, and the
    // control reads as unavailable rather than pretending to offer
    // something").
    std::vector<std::string> labels{"None"};
    const juce::AudioProcessor::Bus* inputBus = getBus(true, 0);
    if (inputBus == nullptr || !inputBus->isEnabled()) {
        return labels;
    }
    const int numChannels = inputBus->getNumberOfChannels();
    if (numChannels <= 0) {
        return labels;
    }
    // One entry per channel the bus CURRENTLY provides -- read from the
    // live layout (getCurrentLayout()), never assumed from the bus's
    // declared type, so this stays correct whichever layout the host
    // negotiates. Named via juce::AudioChannelSet's own
    // channel-type vocabulary, not an
    // invented "Ch<N>" scheme.
    const juce::AudioChannelSet layout = inputBus->getCurrentLayout();
    for (int channel = 0; channel < numChannels; ++channel) {
        labels.push_back(
            juce::AudioChannelSet::getAbbreviatedChannelTypeName(layout.getTypeOfChannel(channel)).toStdString());
    }
    // Sum, only when the bus provides more than one channel: two or more
    // means None, each channel, and their sum.
    if (numChannels > 1) {
        labels.push_back("Sum");
    }
    return labels;
}

void FroggersPluginProcessor::ApplyInputSelection(int selectionIndex) {
    const std::vector<std::string> labels = ComputeInputOptionLabels();
    const int optionCount = static_cast<int>(labels.size());
    // Re-validated against the CURRENT bus every time this runs -- a
    // selection the host has invalidated (a layout change removed the
    // channel it named, or a restored session named one this bus never
    // had) falls back to index 0 ("None") rather than reading a channel
    // that is gone. Never trusts `selectionIndex` merely because a caller
    // (the surface's tap handler, or a restored session blob) named it.
    inputSelection_ = (selectionIndex >= 0 && selectionIndex < optionCount) ? selectionIndex : 0;
    // Pushes the freshly-derived option list and the (possibly just
    // fallen-back) selection into the portable surface -- the ONLY writer
    // of the surface's rendered copy, so what the operator sees can never
    // lag what this method just validated.
    static_cast<synth_froggers::FroggersUiSurface&>(engine_.Application().PortableSurface())
        .SetInputOptions(labels, inputSelection_);
    // "Connected is consent, never channel presence": true only when the
    // OPERATOR has selected something other than
    // "None" -- never merely because the host left the bus enabled with
    // channels. The SAME seam the standalone host uses to report its own
    // routed-input state (Runtime.hpp's RefreshInputRoutedState()/
    // inputRoutingSignal_.Publish(bool)), not a second, plugin-private
    // mechanism. Publish() itself only invokes the registered changed-
    // callback when the value actually differs from its last-published one
    // (InputRoutingSignal::Publish(), AppContext.hpp) -- FroggersAppCore::
    // Init()'s context_->SetInputRoutedChangedCallback() registration
    // (app/FroggersAppCore.hpp) is what actually applies a real change,
    // once, on the next audio block (ProcessFrame(), that file's own
    // comment on why the apply is deferred off this, message-thread,
    // call).
    inputRoutingSignal_.Publish(inputSelection_ != 0);
}

bool FroggersPluginProcessor::ResolveSelectedInputChannel(const float* const* channels, int numChannels,
                                                            int selection, int numSamples, float* out) {
    if (selection <= 0 || numChannels <= 0 || numSamples <= 0) {
        return false;  // "None," or nothing to read from.
    }
    if (selection <= numChannels) {
        const float* source = channels[selection - 1];
        if (source == nullptr) {
            return false;
        }
        std::copy(source, source + numSamples, out);
        return true;
    }
    if (selection != numChannels + 1) {
        // Out of range for both "one specific channel" and "Sum" -- cannot
        // happen in production (ApplyInputSelection() re-validates
        // `inputSelection_` against a freshly-computed option list every
        // time either could have changed), but a test driving this
        // function directly with an inconsistent selection gets "nothing
        // resolved" rather than a read past `channels`.
        return false;
    }
    // Sum: ComputeInputOptionLabels() only ever appends this option once
    // numChannels > 1 -- reachable once a host enables the input bus at
    // stereo() -- kept general rather than special-cased to N==1, matching
    // that method's own generality.
    for (int s = 0; s < numSamples; ++s) {
        float sum = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            if (channels[ch] != nullptr) {
                sum += channels[ch][s];
            }
        }
        out[s] = sum;
    }
    return true;
}

void FroggersPluginProcessor::processorLayoutsChanged() {
    // Fires once per ACTUAL bus-layout change (JUCE's own AudioProcessor::
    // applyBusLayouts()/audioIOChanged() path calls this unconditionally
    // whenever the committed layout differs from the previous one -- a
    // no-op request, e.g. re-requesting the layout already in effect,
    // short-circuits before reaching here) -- never once per processBlock()
    // call, unlike isBusesLayoutSupported() (queried during negotiation,
    // before any layout is committed) or processBlock() itself (audio
    // thread, called every callback regardless of whether the layout
    // changed since the previous one). This is therefore the right, and
    // only, place to re-derive the option set and re-validate the current
    // selection against it: re-validate against the current bus on every
    // layout change and fall back to None.
    // Re-applying the SAME index the operator (or a restore) last chose is
    // deliberate: ApplyInputSelection() re-derives the option list fresh
    // from the bus every call, so a channel that no longer exists falls
    // back to None here even though the index itself did not change.
    ApplyInputSelection(inputSelection_);
}

void FroggersPluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    // The plugin does NOT consume notes: the buffer is accepted and
    // ignored. No note/MIDI wiring exists in FroggersAppCore's ProcessBlock
    // at all -- draining nothing out of `midiMessages` here is deliberate,
    // not an oversight.
    juce::ignoreUnused(midiMessages);

    // Teardown gap: a pure liveness heartbeat for
    // timerCallback()'s staleness detection -- incremented unconditionally,
    // every call, regardless of whether a playhead is present this block
    // (see processBlockCounter_'s own header comment for why this exists:
    // without it, a host that stops calling processBlock() at all -- no
    // releaseResources(), no more blocks, just silence -- would leave
    // hostTempoValid_/hostClockEngaged_ stuck at their last values forever).
    // Relaxed on both ends: this counter synchronizes no other data, it is
    // read only for inequality against a previously observed value.
    processBlockCounter_.fetch_add(1, std::memory_order_relaxed);

    // Read the host playhead HERE, and only here --
    // juce::AudioPlayHead::getPosition()'s own doc comment: "You can ONLY
    // call this from your processBlock() method! Calling it at other times
    // will produce undefined behaviour ... some hosts will almost certainly
    // have multithreading issues if it's not called on the audio thread."
    // This method therefore never pushes anything onto engine_.UiBus() or
    // calls engine_.RequestSyncConfiguration() itself -- it only republishes
    // what it observed into the lock-free atomics timerCallback() (message
    // thread) reads and acts on. See this file's header comment for the
    // full audio-thread-safety trace on why the split is drawn exactly
    // here.
    if (juce::AudioPlayHead* playHead = getPlayHead()) {
        if (const auto position = playHead->getPosition()) {
            // -- transport edge-trigger ---------------------------------
            const bool isPlayingNow = position->getIsPlaying();
            if (!haveHostPlayingBaseline_) {
                // The FIRST observation establishes the baseline rather than
                // firing: there is no prior state to TRANSITION from yet
                // (only on host play-state TRANSITIONS), so a plugin
                // instantiated mid-playback stays silent (matches this
                // class's own "silent by construction" default) until the
                // host's transport next actually toggles.
                haveHostPlayingBaseline_ = true;
                lastHostIsPlaying_ = isPlayingNow;
            } else if (isPlayingNow != lastHostIsPlaying_) {
                // Single-slot, coalescing store -- same "control-rate,
                // human-paced action" idiom FroggersAppCore::
                // RequestBankSelect/RequestEncoderPress already use
                // (FroggersAppCore.hpp's own comment on `RequestBankSelect`), applied here
                // because pushing directly from this thread is not an
                // option (see this file's header comment).
                pendingTransportEdge_.store(
                    static_cast<int>(isPlayingNow ? PendingTransportEdge::kStart : PendingTransportEdge::kStop),
                    std::memory_order_relaxed);
                lastHostIsPlaying_ = isPlayingNow;
            }

            // -- republish the host's reported tempo, if any -----------
            if (const auto bpm = position->getBpm()) {
                hostTempoBpm_.store(*bpm, std::memory_order_relaxed);
                hostTempoValid_.store(true, std::memory_order_release);
            } else {
                // Playhead present, but this block's position carries no
                // bpm -- collapse to the same "nothing usable" signal an
                // absent playhead produces below (timerCallback() cannot
                // and should not tell the two apart: either way there is no
                // host tempo to slave to right now).
                hostTempoValid_.store(false, std::memory_order_release);
            }
        } else {
            hostTempoValid_.store(false, std::memory_order_release);
        }
    } else {
        // No playhead at all (standalone hosting, or this file's own
        // no-host test constructions): handled gracefully -- no messages.
        // The transport baseline is deliberately left untouched (nothing to
        // compare against changed this block); tempo is marked not-usable,
        // which is also what drives the teardown disengage in
        // timerCallback() below.
        hostTempoValid_.store(false, std::memory_order_release);
    }

    const int numSamples = buffer.getNumSamples();

    // The optional input bus (see isBusesLayoutSupported()'s own comment,
    // above): getBusBuffer() reads the bus's ACTUAL negotiated channel
    // count directly from JUCE -- a zero-channel view when the host has
    // left the bus disabled, a one- or two-channel view when the host has
    // enabled it at mono() or stereo() -- rather than re-deriving it from
    // BusesProperties, since a host is free to (dis)connect the bus at any
    // point between construction and this call. Obtained BEFORE the
    // output write pointers below: this
    // bus and the output bus may share the same underlying buffer channels
    // (JUCE's standard in-place convention for buses of equal-or-narrower
    // input width), so the read pointers captured here must be taken while
    // they still point at genuine input samples, not samples this same
    // call has already overwritten with synth output.
    juce::AudioBuffer<float> inputBusBuffer = getBusBuffer(buffer, true, 0);
    const int numInputChannelsThisBlock = inputBusBuffer.getNumChannels();
    const float* const* inputPointers =
        numInputChannelsThisBlock > 0 ? inputBusBuffer.getArrayOfReadPointers() : nullptr;

    // Resolves the operator's own channel/Sum selection (inputSelection_,
    // ApplyInputSelection()'s own comment) down to exactly ONE logical
    // channel BEFORE `block` is built -- FroggersAppCore's own
    // numAudioInputs==1 config, so the core reads only channel 0 and must
    // never see, let alone choose among, the raw bus's own channels itself
    // (ResolveSelectedInputChannel()'s own header comment has the full
    // index-scheme trace). resolvedInputScratch_ is re-sized here rather
    // than trusted at its prepareToPlay()-time size, in case this callback
    // ever delivers more samples than that (that member's own comment).
    if (static_cast<std::size_t>(numSamples) > resolvedInputScratch_.size()) {
        resolvedInputScratch_.assign(static_cast<std::size_t>(numSamples), 0.0f);
    }
    const bool haveResolvedInput = ResolveSelectedInputChannel(
        inputPointers, numInputChannelsThisBlock, inputSelection_, numSamples, resolvedInputScratch_.data());
    const std::array<const float*, 1> resolvedInputChannelPointers{
        haveResolvedInput ? resolvedInputScratch_.data() : nullptr};

    std::array<float*, 2> outputPointers{buffer.getWritePointer(0), buffer.getWritePointer(1)};

    // Same AudioBlock shape the standalone host's real audio-device
    // callback builds (Runtime.hpp's audioDeviceIOCallbackWithContext):
    // `inputs` null-gated on an all-or-nothing zero-channel check rather
    // than an array of per-channel nulls. Unlike the raw bus above,
    // `inputs`/`numInputChannels` here describe the ALREADY-RESOLVED single
    // logical channel, not the bus's own channel count -- FroggersAppCore::
    // ProcessBlock() (see that method's own comment) reads this block's
    // input purely as the external-audio modulation sources' signal, at
    // channel 0, never to re-derive which host channel it came from.
    // numRequestedInputChannels is a different field: AppContext.hpp
    // documents it as "hosts set this explicitly from immutable
    // RuntimeConfig" -- the requested CEILING, independent of how many
    // channels this callback actually delivers -- and Engine::ProcessBlock
    // asserts it equals config_.numAudioInputs exactly (Engine.hpp:410), so
    // it is read from the engine's own already-negotiated config rather
    // than a second, independent literal that could drift out of sync with
    // it. startSample/clockPlan are OUT params the engine sets itself
    // during ProcessBlock (Engine.hpp:402-405) -- left default-constructed
    // here, exactly as SynthRig does.
    synth::AudioBlock block;
    block.inputs = haveResolvedInput ? resolvedInputChannelPointers.data() : nullptr;
    block.outputs = outputPointers.data();
    block.numInputChannels = haveResolvedInput ? 1 : 0;
    block.numOutputChannels = static_cast<int>(outputPointers.size());
    block.numFrames = static_cast<std::size_t>(numSamples);
    block.numRequestedInputChannels = engine_.Config().numAudioInputs;

    engine_.ProcessBlock(block, NowMicros());

    // Publish every host-exposed parameter's current
    // display value into its own atomic UIState snapshot, for
    // PumpHostParameterBridge() (message thread) to read. AFTER
    // engine_.ProcessBlock() (so this block's own ProcessSamplePhase1/2
    // slewing has already run), on THIS thread (the audio thread owns every
    // Parameter's internal uiDisplayCenters_/uiDisplaySpreadEnergies_ etc,
    // the non-atomic storage Parameter::PopulateUIState() reads -- exactly
    // the same audio-thread requirement synth::Engine's own ProcessBlock
    // already honors for its OWN throttled `manager_.PopulateUIState(...)`
    // call, Engine.hpp's own "7. throttled PopulateUIState every
    // uiPublishInterval_ blocks" step). This class calls
    // Parameter::PopulateUIState() directly, per parameter, UNTHROTTLED and
    // bank-selection-independent, rather than reusing that one: Engine's
    // own publish only reaches ParameterManager::UIState's
    // slots[0].cells[position], which reflects Bank::VisibleParameter(ix)
    // of whichever bank the shared BankSlot currently has SELECTED (traced
    // via BankSlot::HandleSetAbsolute/Bank::FindVisibleCell,
    // ParameterModulation.cpp) -- with one physical BankSlot shared by all
    // six banks, that path can only ever see ONE bank's 16 parameters at a
    // time, never all 91 simultaneously. Calling PopulateUIState()
    // per-Parameter instead sidesteps BankSlot/Bank entirely (every
    // Parameter in this group gets its ProcessSamplePhase1/2 slewing every
    // sample regardless of bank selection -- FroggersParameterModel::
    // ProcessSample() drives group_->ProcessSamplePhase1/2 for the WHOLE
    // group, not just the visible bank), so host readback for a bank that
    // is not currently "selected" stays live and correct. Cost is
    // negligible: a handful of relaxed atomic stores per parameter, once
    // per audio callback, no allocation -- see HostParamEntry::uiState's
    // own comment.
    for (HostParamEntry& entry : hostParams_) {
        if (entry.coreParam != nullptr) {
            entry.coreParam->PopulateUIState(*entry.uiState);
        }
    }

    // Engine::MessageThreadTick() (patch-response draining, serialization
    // arena growth, MIDI output processor pumps) is deliberately NOT called
    // from here -- it performs non-realtime-safe work (patch IO, heap
    // growth), exactly like Runtime.hpp, which drives it from a
    // message-thread juce::Timer (:975 call site), never the audio
    // callback. This plugin wires the same pump: see timerCallback() below
    // (started in the constructor via startTimerHz(30)).
}

void FroggersPluginProcessor::timerCallback() {
    // Tick order mirrors Sheaf Runtime.hpp's own timerCallback()
    // (Runtime.hpp:974-984): the engine's message-thread tick runs first.
    engine_.MessageThreadTick();

    // -- drain the pending host transport edge, if any -----------------
    // Routed through the SAME production seam the Play/Stop buttons use --
    // FroggersApp::PortableSurface() (Froggers.hpp's `PortableSurface`) returns the exact
    // FroggersUiSurface instance already Attach()-ed to this engine
    // (Froggers.hpp's `FroggersApp::Init`, run once during engine_.Initialize() above), so
    // DispatchAction() here runs the literal HandleAction kPlay/kStop
    // branches (FroggersUiSurface.hpp's `HandleAction`) -- including their
    // LatchThenTransport call, which disarms the latch and pushes the
    // transport message in happens-before order -- rather than a
    // hand-mirrored copy of that logic that could drift from it. No editor
    // is required for this: DispatchAction() does not touch anything
    // editor-owned.
    const int pendingEdge = pendingTransportEdge_.exchange(
        static_cast<int>(PendingTransportEdge::kNone), std::memory_order_relaxed);
    if (pendingEdge == static_cast<int>(PendingTransportEdge::kStart)) {
        engine_.Application().PortableSurface().DispatchAction(
            synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));
    } else if (pendingEdge == static_cast<int>(PendingTransportEdge::kStop)) {
        engine_.Application().PortableSurface().DispatchAction(
            synth::ui::Action::Named(synth_froggers::FroggersActions::kStop));
    }

    // -- host tempo via the existing external-clock slave path ---------
    // Trace: MasterClock::SetTempoBpm no-ops unconditionally
    // while `syncConfig_.receiveClock` is true (src/MasterClock.cpp:
    // 963-965) -- that suppression is a property of the flag, not of HOW
    // the tempo estimate itself gets updated. MasterClock::
    // HandleExternalClock (src/MasterClock.cpp:1097-1193) is the only path
    // that updates `activeBpm_` while slaved, and it derives that estimate
    // from the MEASURED INTERVAL between successive external-origin
    // MessageIn::Clock ticks (recoveredBpm = 60e6 / (ppqn *
    // filteredPeriodMicros), :1174-1177). So reaching "host tempo changes
    // reach the clock" AND "requests suppressed" through the SAME existing
    // mechanism requires SYNTHESIZING that tick stream -- calling
    // RequestTempoBpm(hostBpm) instead would funnel into the very
    // SetTempoBpm that no-ops while slaved, so it cannot be what updates
    // the tempo once slaved; the two candidate approaches are not actually
    // interchangeable once receiveClock is engaged, which is why this class
    // synthesizes ticks rather than requesting tempo directly.
    //
    // Zero core edits either way: RouteRealtimeBatch already routes any
    // Origin::ExternalMidi MessageIn::Clock to
    // MasterClock::HandleExternalClock (Engine.hpp:907-916), and
    // Engine::RequestSyncConfiguration is an existing lock-free atomic
    // request already applied once per block by ProcessBlock's own
    // ApplySyncConfig call (Engine.hpp:346-349, 489-495) -- both pre-date
    // this class. grep across app/ and the runtime shell turned up no
    // OTHER production caller of RequestSyncConfiguration at all: this
    // plugin is the first thing that ever engages external-clock slaving
    // outside a test rig (FroggersSurfaceTests.cpp's
    // `bpm_slider_is_read_only_and_shows_recovered_tempo_while_externally_clocked`'s SetSyncConfig is
    // a synchronous test-only bypass), so there is no existing "how
    // MIDI-clock slaving engages" production call site to defer to beyond
    // this mechanism itself.
    // Teardown gap: hostTempoValid_ on its own
    // only tells us what processBlock() last PUBLISHED -- if processBlock()
    // stops being called at all (host suspends/disables this track without
    // ever calling releaseResources()), that published value simply freezes
    // at whatever it last was and never goes false on its own. Detected via
    // processBlockCounter_ (a pure liveness heartbeat, see its own header
    // comment): if the counter has not moved since the last pump, this pump
    // measures how long it has been since it last DID move
    // (lastActivityMicros_, using the same NowMicros() clock as everywhere
    // else in this class); once that elapsed time reaches
    // kStaleActivityWindowMicros (~1 second -- see that constant's own
    // comment for why this uses a time-based window rather than a fixed
    // 3-pump count), the host is treated as
    // gone. At this class's own 30Hz pump rate (~33ms/pump) and a typical
    // real-world audio block far shorter than that (e.g. 256 samples @
    // 48kHz ~= 5.3ms), processBlock() ordinarily fires several times BETWEEN
    // two consecutive pumps whenever the host is genuinely still calling
    // it -- observing literally zero calls for a full second is far outside
    // normal scheduling jitter and reliably means processBlock() has
    // stopped being called at all.
    //
    // releaseResourcesSeen_ (see its own header
    // comment) is folded into the SAME staleness signal rather than
    // handled as a separate branch -- releaseResources() is simply a
    // stronger, immediate version of "the host is gone," so it forces
    // activityStale true on this pump without waiting out the window.
    const std::uint64_t nowMicros = NowMicros();
    const std::uint64_t observedBlockCounter = processBlockCounter_.load(std::memory_order_relaxed);
    bool activityStale = false;
    if (observedBlockCounter != lastObservedProcessBlockCounter_) {
        lastObservedProcessBlockCounter_ = observedBlockCounter;
        lastActivityMicros_ = nowMicros;
    } else {
        activityStale = (nowMicros - lastActivityMicros_) >= kStaleActivityWindowMicros;
    }
    if (releaseResourcesSeen_.exchange(false, std::memory_order_acq_rel)) {
        activityStale = true;
    }

    const bool hostTempoValid = hostTempoValid_.load(std::memory_order_acquire);
    const double hostTempoBpm = hostTempoBpm_.load(std::memory_order_relaxed);
    const bool hostTempoUsable = !activityStale && hostTempoValid && std::isfinite(hostTempoBpm) && hostTempoBpm > 0.0;

    if (hostTempoUsable) {
        // Reuses the SAME nowMicros captured above for the activity-staleness
        // check -- one NowMicros() call per pump, not two.
        if (!hostClockEngaged_) {
            // Engage. receiveTransport is deliberately left false (its
            // SyncConfig default): 6.1 above pushes plain Origin::Internal
            // Start/Stop -- exactly the Play/Stop buttons' own messages --
            // not Origin::ExternalMidi transport commands, so the
            // transport-arming state machine HandleExternalTransport would
            // add (ArmedStart/ArmedContinue, splice-crossing enumeration) is
            // neither wanted nor engaged here; only tempo is slaved.
            engine_.RequestSyncConfiguration(synth::SyncConfig{.receiveClock = true});
            hostClockEngaged_ = true;
            nextHostClockTickMicros_ = nowMicros;
        }

        // Standard MIDI clock rate: SyncConfig::ppqn's own default (24
        // pulses per quarter note, synth/MasterClock.hpp), READ rather than
        // restated as a literal so this can never silently drift from
        // Sheaf's own default if it ever changes. Ticks are timestamped
        // this-many-micros apart using the CURRENT host tempo, so
        // HandleExternalClock's interval-based estimator recovers the right
        // bpm even though every tick pushed in a given pump actually
        // arrives in one batch -- MessageInBus::Pop gates on the message's
        // OWN timestamp field, not wall-clock delivery time
        // (ParameterModulation.cpp:4007-4020), so a burst of correctly-
        // spaced-by-timestamp ticks estimates tempo exactly as a real,
        // evenly-spaced hardware clock would.
        constexpr int kHostClockPpqn = synth::SyncConfig{}.ppqn;
        const double tickIntervalMicros = 60'000'000.0 / (static_cast<double>(kHostClockPpqn) * hostTempoBpm);
        if (std::isfinite(tickIntervalMicros) && tickIntervalMicros > 0.0) {
            // Capped: if this timer went an unusually long time between
            // pumps (e.g. the host suspended the plugin), do not replay an
            // unbounded backlog of ticks in one call.
            constexpr int kMaxTicksPerPump = 64;
            int ticksPushed = 0;
            while (nextHostClockTickMicros_ <= nowMicros && ticksPushed < kMaxTicksPerPump) {
                engine_.UiBus().Push(synth::MessageIn::Clock(
                    nextHostClockTickMicros_, synth::MessageIn::Origin::ExternalMidi, /*externalControllerSlot=*/0));
                nextHostClockTickMicros_ += static_cast<std::uint64_t>(tickIntervalMicros);
                ++ticksPushed;
            }
            if (ticksPushed == kMaxTicksPerPump) {
                // Resynchronize to "now" rather than keep falling further
                // behind on the next pump too.
                nextHostClockTickMicros_ = nowMicros + static_cast<std::uint64_t>(tickIntervalMicros);
            }
        }
    } else if (hostClockEngaged_) {
        // -- Teardown -------------------------------------------------------
        // MasterClock's OWN external-source staleness check
        // (ExpireExternalSource/ClearExternalSource, src/MasterClock.cpp:
        // 350-393) is reactive, not ambient: it only runs from inside
        // AcceptExternalSource, itself only reached when ANOTHER clock/
        // transport message arrives (src/MasterClock.cpp:395-419) -- if the
        // host truly vanishes, no such message ever arrives again, so that
        // path alone would never fire. And even when it DOES fire, it only
        // resets `acquisitionState_`/`source_`, never `syncConfig_.
        // receiveClock` itself (ClearExternalSource, :350-367) -- so
        // TempoExternallyClocked() (FroggersAppCore.hpp's `ProcessBlock`, reads
        // SyncConfiguration().receiveClock directly) would stay stuck true,
        // permanently suppressing the BPM slider with no live source
        // driving it. There is no existing production disengage call site
        // to mirror (this plugin is the first production caller of
        // RequestSyncConfiguration at all) -- so this IS the disengage,
        // reached by EITHER of two independent triggers folded into
        // hostTempoUsable above (both handled entirely on this, the message
        // thread -- see hostClockEngaged_'s own header comment on why that
        // matters): (1) processBlock() itself observes the host reporting
        // no usable playhead/bpm any more, or (2) processBlockCounter_'s
        // staleness streak proves processBlock() has stopped being called
        // at all, whether or not releaseResources() ever runs.
        // releaseResourcesSeen_ additionally forces this branch immediately
        // rather than waiting out the staleness window, for the ordinary
        // "host cleanly tore the track down" case.
        engine_.RequestSyncConfiguration(synth::SyncConfig{});
        hostClockEngaged_ = false;
    }

    // -- Stable-ID host parameter surface, both directions -------------
    // Message-thread-only, same discipline as the transport/tempo handling
    // above: this is the ONLY place that pushes
    // MessageIn::ParamSetAbsoluteOnBank or calls DispatchAction(kFreeze) for
    // a host-driven write, and the ONLY place that reads the per-parameter
    // UIState snapshots processBlock() (audio thread) publishes into
    // hostParams_[i].uiState -- see PumpHostParameterBridge()'s own comment
    // for the full bidirectional trace.
    PumpHostParameterBridge();

    // DAW session-state persistence: independent of the host-parameter
    // bridge above (a different pair of buses, drained independently
    // inside engine_.ProcessBlock() -- see PumpStatePersistence()'s own
    // comment), so its position here relative to PumpHostParameterBridge()
    // does not affect correctness.
    PumpStatePersistence();

    // Repaint the editor, if one is open, LAST -- after every other
    // state mutation this pump makes above (message tick, transport, tempo,
    // parameter bridge) -- so a live editor's RefreshFromSurface() rebuilds
    // its tree from THIS pump's freshest state, never a stale mid-pump
    // snapshot. Zero cost when no editor is open (editorRepaintHook_ is
    // empty, SetEditorRepaintHook()'s own comment).
    if (editorRepaintHook_) {
        editorRepaintHook_();
    }
}

void FroggersPluginProcessor::TestStartTransport() {
    // Routed through DispatchAction (the production seam, see
    // timerCallback()'s own comment) rather than pushing
    // MessageIn::Start/SetDesiredTransportRunning directly: a direct push
    // would miss the Freeze-latch disarm the real Play button's
    // LatchThenTransport call performs (FroggersUiSurface.hpp's `HandleAction`),
    // and hand-mirroring that fix a
    // second time would let this seam and the real transport-edge-trigger
    // producer independently drift from HandleAction's actual kPlay
    // branch.
    engine_.Application().PortableSurface().DispatchAction(
        synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));
}

void FroggersPluginProcessor::TestStopTransport() {
    // See TestStartTransport()'s own comment.
    engine_.Application().PortableSurface().DispatchAction(
        synth::ui::Action::Named(synth_froggers::FroggersActions::kStop));
}

// -- stable-ID host parameter surface -----------------------------------
void FroggersPluginProcessor::BuildHostParameterInventory() {
    synth_froggers::FroggersParameterModel& model = engine_.Application().Parameters();
    const auto& layouts = synth_froggers::FroggersBankLayouts();

    // Sized from the model's OWN enumeration constants, never a hardcoded
    // literal (the governing spec's own "Count" requirement) -- 6
    // banks * 14 page parameters + 6 per-bank Crispy + 1 shared Crunchy + 1
    // Freeze.
    hostParams_.reserve(synth_froggers::kFroggersBankCount * synth_froggers::kFroggersParamsPerBank
                         + synth_froggers::kFroggersBankCount + 1 + 1);

    // JUCE's own versionHint doc comment (juce_ParameterID.h): "Influences
    // parameter ordering in Audio Unit plugins" -- a monotonically
    // increasing hint over this SAME stable construction order is the JUCE
    // convention for it; it plays no role in this class's own identity or
    // addressing (parameterID/bankIx/position do that), so it is not part
    // of the stability contract HostParamStableId()'s own comment makes.
    int versionHint = 1;

    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
        const synth_froggers::FroggersBankLayout& layout = layouts[bankIx];

        for (std::size_t paramIx = 0; paramIx < synth_froggers::kFroggersParamsPerBank; ++paramIx) {
            synth::Parameter& coreParam = model.PageParameter(bankIx, paramIx);

            HostParamEntry entry;
            entry.kind = HostParamEntry::Kind::kPageParam;
            entry.coreParam = &coreParam;
            entry.bankIx = bankIx;
            entry.position = paramIx;
            entry.uiState = std::make_unique<synth::Parameter::UIState>();
            // voiceCapacity=1 (FroggersParameterModel::kNumVoices; this is a
            // mono model); this bridge needs no modulator/gesture color info, so
            // both those capacities are 0 -- Parameter::UIState::Configure()
            // (ParameterModulation.cpp) accepts 0 for either with no error,
            // it simply allocates zero-length arrays for them.
            entry.uiState->Configure(/*voiceCapacity=*/1, /*modulatorColorCapacity=*/0, /*gestureColorCapacity=*/0);
            // Matches the REAL starting value FroggersParameterModel::Init()
            // seeds this exact Parameter with (RegisterParameter's own
            // `.defaultValue = spec.defaultValue`, FroggersParameters.hpp)
            // -- read from the SAME FroggersParamSpec that call reads, not
            // re-guessed.
            const float defaultValue = layout.params[paramIx].defaultValue;
            entry.shadowNormalized = defaultValue;

            // name = coreParam.Name(): the qualified "<Bank> <Param>"
            // display name FroggersParameterModel::Init() itself computes
            // and registers (`qualifiedName = layout.name + " " +
            // spec.name`, FroggersParameters.hpp) -- read directly from the
            // live Parameter, not re-derived, so it can never drift from
            // the model's own display-name authority.
            auto* juceParam = new juce::AudioParameterFloat(
                juce::ParameterID(HostParamStableId(bankIx, paramIx), versionHint++), juce::String(coreParam.Name()),
                juce::NormalisableRange<float>(0.0f, 1.0f), defaultValue);
            entry.juceParam = juceParam;
            addParameter(juceParam);
            hostParams_.push_back(std::move(entry));
        }

        // Crispy (position kFroggersCrispySlot==14): per-bank, so
        // still needs bank selection like an ordinary page parameter.
        {
            synth::Parameter& coreParam = model.Crispy(bankIx);

            HostParamEntry entry;
            entry.kind = HostParamEntry::Kind::kCrispy;
            entry.coreParam = &coreParam;
            entry.bankIx = bankIx;
            entry.position = synth_froggers::kFroggersCrispySlot;
            entry.uiState = std::make_unique<synth::Parameter::UIState>();
            entry.uiState->Configure(1, 0, 0);
            // Crispy's own RegisterParameter call never sets `.defaultValue`
            // (FroggersParameters.hpp), leaving ParameterConfig's own
            // default (0.0f) -- matched here, not re-guessed.
            constexpr float kCrispyDefaultValue = 0.0f;
            entry.shadowNormalized = kCrispyDefaultValue;

            auto* juceParam = new juce::AudioParameterFloat(
                juce::ParameterID(HostParamStableIdCrispy(bankIx), versionHint++), juce::String(coreParam.Name()),
                juce::NormalisableRange<float>(0.0f, 1.0f), kCrispyDefaultValue);
            entry.juceParam = juceParam;
            addParameter(juceParam);
            hostParams_.push_back(std::move(entry));
        }
    }

    // Crunchy (position kFroggersCrunchySlot==15): ONE Parameter object,
    // the SAME pointer registered at slot 15 in every bank
    // (FroggersParameters.hpp) -- so unlike page parameters/Crispy, writing
    // it needs no bank selection at all: whichever bank the shared BankSlot
    // currently has selected, position 15 always resolves to this same
    // Parameter*.
    {
        synth::Parameter& coreParam = model.Crunchy();

        HostParamEntry entry;
        entry.kind = HostParamEntry::Kind::kCrunchy;
        entry.coreParam = &coreParam;
        entry.bankIx = 0;  // Arbitrary: this Parameter* is at position 15 in every bank's own top-level mapping.
        entry.position = synth_froggers::kFroggersCrunchySlot;
        entry.uiState = std::make_unique<synth::Parameter::UIState>();
        entry.uiState->Configure(1, 0, 0);
        // Crunchy's own RegisterParameter call never sets `.defaultValue`
        // either (FroggersParameters.hpp) -- same 0.0f default as Crispy.
        constexpr float kCrunchyDefaultValue = 0.0f;
        entry.shadowNormalized = kCrunchyDefaultValue;

        auto* juceParam = new juce::AudioParameterFloat(
            juce::ParameterID(kHostParamStableIdCrunchy, versionHint++), juce::String(coreParam.Name()),
            juce::NormalisableRange<float>(0.0f, 1.0f), kCrunchyDefaultValue);
        entry.juceParam = juceParam;
        addParameter(juceParam);
        hostParams_.push_back(std::move(entry));
    }

    // Freeze: NOT a ParameterManager parameter -- kFreeze is wired via
    // DispatchAction, so the host parameter must drive the same production
    // seam and preserve SetFreezeLatched semantics. coreParam/uiState stay
    // null; PumpHostParameterBridge()'s Freeze branch reads
    // FroggersAppCore::FreezeLatched() (already an atomic, acquire-load
    // getter -- FroggersAppCore.hpp -- safe cross-thread with no
    // PopulateUIState snapshot needed) and writes via
    // PortableSurface().DispatchAction(kFreeze), the EXACT seam the
    // transport edge-trigger and this file's own TestStartTransport()
    // already use, and the exact seam the real Freeze button itself uses
    // (FroggersUiSurface.hpp's kFreeze branch). No plugin-side MIDI
    // mapping/learn is introduced anywhere by this -- Freeze becomes
    // automatable/host-MIDI-mappable purely by being an ordinary
    // juce::AudioProcessorParameter, exactly like every other parameter
    // above.
    {
        HostParamEntry entry;
        entry.kind = HostParamEntry::Kind::kFreeze;
        // FreezeLatched() defaults false (FroggersAppCore.hpp) -- matched
        // here, not re-guessed.
        entry.shadowNormalized = 0.0f;

        auto* juceParam = new juce::AudioParameterBool(
            juce::ParameterID(kHostParamStableIdFreeze, versionHint++), "Freeze", /*defaultValue=*/false);
        entry.juceParam = juceParam;
        addParameter(juceParam);
        hostParams_.push_back(std::move(entry));
    }
}

// PumpHostParameterBridge() -- message-thread-only (called from
// timerCallback(), never processBlock() -- see this file's header comment
// on the audio-thread/message-thread split). One pass over hostParams_ per
// pump, both directions, per entry:
//
//   host -> core: if the host has moved this parameter since the shadow
//   last agreed with it (`juceParam->getValue() != entry.shadowNormalized`
//   beyond a small float epsilon), push exactly the write a real host
//   automation event or MIDI-mapped-by-the-DAW controller move would
//   produce -- MessageIn::ParamSetAbsoluteOnBank at this entry's
//   (bankIx, slotIx=0, position). This resolves directly against bankIx's
//   own top-level parameter mapping (synth::Bank::HandleSetAbsoluteOnTopLevel,
//   via ParameterManager::HandleSetAbsoluteOnBank) -- it never touches the
//   shared BankSlot's selection, and never depends on which bank is
//   currently selected or how deep that bank is drilled into a modulation
//   view. A host write therefore always lands on the entry's own
//   parameter, whether or not that entry's bank happens to be the one an
//   operator is currently looking at, and never moves the visible bank or
//   disturbs an open drilldown.
//
//   This is pushed onto engine_.UiBus() (the message bus) rather than
//   applied through the audio-thread Request*/pending*_ bridge
//   (FroggersAppCore::RequestBankSelect() and friends, applied inside
//   ProcessFrame()) because engine_.ProcessBlock() drains the message bus
//   BEFORE running ProcessFrame() (Engine.hpp's own binding step order): a
//   Request* write queued this pump would not apply until the block AFTER
//   the one a message-bus write applies in, landing one block late relative
//   to a value meant to take effect immediately.
//
//   Crunchy's bankIx is arbitrary (HostParamEntry::bankIx's own comment):
//   the same Parameter object is registered at position 15 in every bank's
//   top-level mapping, so any bankIx resolves it correctly.
//
//   core -> host: reached only when the host did NOT just write this
//   parameter THIS pump (host writes always win a same-pump race, and
//   `continue` past the core-check below, in the per-entry branches above,
//   deliberately skip it -- the message this pump just pushed has not been
//   applied to the core yet, so reading uiState/FreezeLatched() THIS pump
//   would still see the OLD value and could otherwise echo it straight
//   back over the host's fresh write). If the core's published value
//   (entry.uiState->values[0], or FreezeLatched() for kFreeze) differs from
//   the shadow, this is a genuine core-side change (randomize, a scene
//   change, or -- ordinarily -- the settling tail of an EARLIER host write
//   still slewing toward its target via Parameter's own ~10Hz
//   uiDisplayCenterAlpha one-pole, ParameterModulation.hpp) --
//   setValueNotifyingHost() relays it and the shadow is updated to match.
//
//   Feedback-guard: the single shadowNormalized per entry is what makes
//   this safe from an endless notify loop. The moment this method itself
//   calls setValueNotifyingHost(coreValue), it also sets
//   shadowNormalized = coreValue -- so on the VERY NEXT pump,
//   juceParam->getValue() (unchanged since nothing else wrote it) still
//   equals shadowNormalized, the host-check finds no diff, and only the
//   core-check runs; that check only fires again once the core's ACTUAL
//   published value has moved again. Because Parameter's own slew is a
//   one-pole IIR converging on floating-point hardware, it reaches a true,
//   bit-exact fixed point in finite time (the update term shrinks below the
//   float ULP at that magnitude and the stored value stops changing bit for
//   bit) -- so this is not merely "unlikely to loop forever," it provably
//   goes to a fixed count of notifies and stops.
//   FroggersVstHostTests.cpp's
//   host_write_produces_a_bounded_number_of_notifications_not_an_endless_loop
//   proves this by pumping well past that settle window and asserting the
//   notify count has gone flat, plus a positive control (a genuine
//   core-side change DOES still notify) proving the guard discriminates
//   rather than just suppressing everything.
void FroggersPluginProcessor::PumpHostParameterBridge() {
    constexpr float kEpsilon = 1.0e-5f;

    for (HostParamEntry& entry : hostParams_) {
        if (entry.kind == HostParamEntry::Kind::kFreeze) {
            // Not only the host writes entry.juceParam between one pass of
            // this loop and the next: PumpStatePersistence() also writes it
            // directly, to feed a DAW session restore through this exact
            // comparison instead of dispatching a second time on its own --
            // see that method's own comment.
            const bool hostTarget = entry.juceParam->getValue() >= 0.5f;
            const bool shadowBool = entry.shadowNormalized >= 0.5f;
            if (hostTarget != shadowBool) {
                // host -> core: DispatchAction(kFreeze) TOGGLES
                // (FroggersUiSurface.hpp's own kFreeze branch: `engaging =
                // !app_->FreezeLatched(); engaging ?
                // LatchThenTransport(true, ...) : app_->SetFreezeLatched(false)`)
                // -- converted here into "set to this absolute target" by
                // only dispatching when the toggle would actually move the
                // latch to match what the host asked for.
                if (hostTarget != engine_.Application().FreezeLatched()) {
                    engine_.Application().PortableSurface().DispatchAction(
                        synth::ui::Action::Named(synth_froggers::FroggersActions::kFreeze));
                }
                entry.shadowNormalized = hostTarget ? 1.0f : 0.0f;
                continue;
            }

            const bool coreValue = engine_.Application().FreezeLatched();
            if (coreValue != shadowBool) {
                entry.juceParam->setValueNotifyingHost(coreValue ? 1.0f : 0.0f);
                entry.shadowNormalized = coreValue ? 1.0f : 0.0f;
            }
            continue;
        }

        const float hostValue = entry.juceParam->getValue();
        if (std::fabs(hostValue - entry.shadowNormalized) > kEpsilon) {
            // host -> core: resolves directly against entry.bankIx's own
            // top-level mapping (see this method's own header comment) --
            // never touches the shared BankSlot's selection.
            engine_.UiBus().Push(synth::MessageIn::ParamSetAbsoluteOnBank(
                NowMicros(), entry.bankIx, /*slotIx=*/0, entry.position, hostValue));
            entry.shadowNormalized = hostValue;
            continue;
        }

        // core -> host (only reached when the host did not just write this
        // parameter this same pump -- see this method's own header
        // comment).
        const float coreValue = entry.uiState->values[0].load(std::memory_order_relaxed);
        if (std::fabs(coreValue - entry.shadowNormalized) > kEpsilon) {
            entry.juceParam->setValueNotifyingHost(coreValue);
            entry.shadowNormalized = coreValue;
        }
    }
}

// PumpStatePersistence() -- message-thread-only (called from
// timerCallback(), same discipline as PumpHostParameterBridge() above --
// see this file's header comment on the audio-thread/message-thread
// split). Both directions of DAW session-state persistence, pushed/popped
// directly on engine_.Context().patchInputBus/patchOutputBus:
// AppContext.hpp's own comment documents the contract ("producer: message
// thread" / "consumer: audio thread" for the input bus, the reverse for
// the output bus) -- the same buses PatchManager (engine_.Patches()) would
// use for the standalone's on-disk Save/Load Patch feature, but this class
// never calls PatchManager, and nothing else in this plugin does either
// (Froggers' own portable UI exposes no patch save/load surface,
// app/FroggersUiSurface.hpp has no such action) -- so PatchManager's own
// ProcessResponses() (called unconditionally every
// engine_.MessageThreadTick(), above) never has a pending save of its own
// and therefore never touches patchOutputBus. This method is that bus's
// only consumer, and this class is patchInputBus's only producer; sharing
// either with PatchManager would let one side silently steal a response
// meant for the other (MessageOutBus::Pop() is a plain dequeue -- whichever
// side pops a message first is the only side that ever sees it).
//
// Restore: a JUCE host may call setStateInformation() from any thread (no
// thread annotation on that declaration, juce_AudioProcessor.h -- the same
// asymmetry releaseResources() has, see this file's header comment on
// stateBlockMutex_), so it cannot safely push onto patchInputBus itself
// (the single-producer contract every other push in this class already
// honors). It deposits the raw bytes into pendingRestoreJsonText_ instead;
// this method claims that deposit and is the one that actually parses and
// pushes it, as a LoadFromJSON patch message -- applied by
// DrainPatchInputBus() inside the next engine_.ProcessBlock() (audio
// thread), exactly like every other host-driven core write in this class
// (MessageIn::ParamSetAbsolute et al., PumpHostParameterBridge()). Applying
// the restored values directly to the authority this way, rather than
// writing this class's host-parameter juce::AudioProcessorParameters/
// shadowNormalized shadows, is what keeps this from fighting
// PumpHostParameterBridge()'s feedback guard: the restored values simply
// look like an ordinary core-side change on a later pump (the exact case
// that guard already discriminates and relays, see that method's own
// comment) rather than a second, competing write path. A malformed deposit
// (fails to parse) is dropped -- re-parsing the identical bytes next pump
// could not succeed either; an incompatible-but-parseable one (fails
// LoadPatchJSON's own schema/shape check, ValidPatchRoot in
// PatchPersistence.cpp) is still pushed and applied by ApplyPatchMessage,
// which reports InvalidJSON and leaves the authority untouched -- the same
// "invalid document is a no-op" contract LoadPatchJSON documents. A push
// that fails only because patchInputBus is momentarily full is transient,
// so the deposit is put back for the next pump to retry rather than lost.
//
// Snapshot: ApplyPatchMessage's SerializeToJSON handler calls
// BuildPatchJSON(..., manager, ...), reading the SAME ParameterManager the
// audio thread mutates every sample. synth/Json.hpp's own header comment
// is what makes building that JSON safe to run ON the audio thread (an
// arena bump-allocator, no system allocator call) but explicitly reserves
// Dumps() ("intended for non-realtime handoff code") for elsewhere -- so
// this method requests a snapshot, and only turns the response into text
// (Dumps()) once it comes back here, on the message thread. At most one
// request is ever outstanding (pendingStateSnapshotRequestId_) -- required
// because, unlike PatchManager's own single-pending-save gate
// (PatchSerializationContext::arena's own doc comment), nothing else
// enforces this for a direct bus producer, and the response's document
// aliases engine_'s shared serialization arena non-owningly: a second
// request before the first is fully consumed would let its arena Reset()
// clobber the still-unread first response. cachedStateJsonText_ is
// therefore never more than about one pump interval stale -- the same
// bound PumpHostParameterBridge() already accepts for host-parameter
// readback.
//
// Freeze latch, both directions (see getStateInformation()'s own header
// comment in the .hpp for why this lives outside "parameterValues"):
//   snapshot -- the sessionExtras object is attached directly to the
//   response's own JSON tree, using its own aliased arena, strictly
//   between popping the response and clearing
//   pendingStateSnapshotRequestId_ -- i.e. the same window in which the
//   single-outstanding-request gate above already guarantees the audio
//   thread cannot be touching that arena (it will not process another
//   SerializeToJSON, and so will not Reset() this arena again, until this
//   method issues a new request, which only happens once this window has
//   closed). FreezeLatched() is read fresh at attach time rather than
//   threaded through the request/response round trip itself, which only
//   widens the value's staleness bound from "current" to "current as of
//   the last bus hop" -- already within the "about one pump interval
//   stale" bound this whole cache accepts.
//   restore -- deliberately does NOT call DispatchAction(kFreeze) itself.
//   It writes the target into the Freeze host parameter's own JUCE value
//   (setValueNotifyingHost(), the same call PumpHostParameterBridge()'s
//   own core->host direction uses to reflect a non-host-originated change)
//   without touching that entry's shadowNormalized -- so, from
//   PumpHostParameterBridge()'s point of view on ITS next pass, this looks
//   exactly like a host automation write that has not been relayed yet,
//   and its existing kFreeze branch (compare against FreezeLatched(),
//   DispatchAction(kFreeze) only on a real difference) does the actual
//   dispatch. One extra pump of latency versus dispatching here directly,
//   the same bound every other host-parameter round trip in this class
//   already carries -- traded for not needing a second place that decides
//   when Freeze should toggle.
void FroggersPluginProcessor::PumpStatePersistence() {
    std::optional<std::string> restoreText;
    {
        const std::lock_guard<std::mutex> lock(stateBlockMutex_);
        restoreText = std::move(pendingRestoreJsonText_);
        pendingRestoreJsonText_.reset();
    }
    if (restoreText.has_value()) {
        constexpr std::size_t kInitialArenaCapacity = synth::PatchSerializationContext{}.initialArenaCapacity;
        auto arena = std::make_shared<synth::JsonArena>(kInitialArenaCapacity);
        synth::JSON root = arena->Loads(restoreText->c_str());
        while (root.IsNull() && arena->Failed()) {
            arena->GrowAndReset();
            root = arena->Loads(restoreText->c_str());
        }
        if (!root.IsNull()) {
            const bool pushed = engine_.Context().patchInputBus->Push(
                synth::PatchMessageIn::LoadFromJSON(synth::JsonDocument{.arena = arena, .root = root}));
            if (pushed) {
                // Paired with the parameter-authority restore above rather
                // than applied independently: if the push above had failed
                // instead, this would retry next pump alongside it (the
                // else branch below), so the two halves of one restore
                // never land on different pumps. Reading root below, after
                // handing a copy of it to the bus, is safe unconditionally
                // (not just while the bus happens to still be unconsumed):
                // Get()/BooleanValue() never write to the arena, and
                // nothing else ever will either -- the audio thread only
                // ever reads this same document too (LoadPatchJSON copies
                // values out into ParameterManager, it never mutates the
                // JSON tree it was handed) -- so this is two readers over
                // already-built, henceforth-immutable nodes, never a
                // reader racing a writer. A blob saved before this key
                // existed has no "sessionExtras" object at all -- Get() on
                // a missing key returns a null JSON, IsJsonBoolean() rejects
                // it (both a missing key and a present-but-wrong-typed
                // one), and the latch is left exactly as it was.
                const synth::JSON freezeLatchedJson = root.Get(kSessionExtrasKey).Get(kFreezeLatchedKey);
                if (IsJsonBoolean(freezeLatchedJson)) {
                    const bool targetFreezeLatched = freezeLatchedJson.BooleanValue();
                    for (HostParamEntry& entry : hostParams_) {
                        if (entry.kind == HostParamEntry::Kind::kFreeze) {
                            entry.juceParam->setValueNotifyingHost(targetFreezeLatched ? 1.0f : 0.0f);
                            break;
                        }
                    }
                }
                // Same missing-or-wrong-typed-is-a-no-op treatment as the
                // Freeze latch above -- a blob saved before this key existed
                // (or with sessionExtras but no bank key) leaves the visible
                // bank exactly where FroggersParameterModel::Init() already
                // put it (bank 0). Unlike the Freeze latch, this does NOT
                // write a host parameter's JUCE value directly: the visible
                // page is not a host-automatable parameter, and
                // FroggersAppCore::RequestBankSelect() (the same public seam
                // FroggersUiSurface.hpp's own bank buttons call) is the only
                // authority that also reconstructs drillIn_ for the restored
                // bank -- pushing MessageIn::SelectParamBank or writing
                // activeBankIx_/drillIn_ directly would bypass that
                // reconstruction. A saved index a host project can name that
                // this build no longer has (kFroggersBankCount shrank, or
                // the blob is corrupt/hostile) is bounds-checked HERE,
                // before ever reaching RequestBankSelect(), rather than
                // trusted blind: ProcessFrame()'s own internal bankRequest
                // check (FroggersAppCore.hpp) only guards against a
                // negative/too-large `int` after a std::size_t round trip,
                // which a negative int64_t here could already have
                // aliased into a large positive std::size_t before ever
                // reaching that check.
                const synth::JSON visibleBankIndexJson = root.Get(kSessionExtrasKey).Get(kVisibleBankIndexKey);
                if (IsJsonInteger(visibleBankIndexJson)) {
                    const std::int64_t requestedBankIx = visibleBankIndexJson.IntegerValue();
                    if (requestedBankIx >= 0 &&
                        static_cast<std::uint64_t>(requestedBankIx) < synth_froggers::kFroggersBankCount) {
                        engine_.Application().RequestBankSelect(static_cast<std::size_t>(requestedBankIx));
                    }
                }
                // Same missing-or-wrong-typed-is-a-no-op treatment as the two
                // keys above -- a blob saved before this key existed (or
                // with sessionExtras but no input-selection key) leaves
                // inputSelection_ exactly where it already is (0, "None" --
                // its NSDMI, unchanged by construction unless the operator
                // or a valid restore sets it), which is exactly the "opt-in
                // audio defaults off" contract requires. Bounds-checked HERE, against a FRESH
                // ComputeInputOptionLabels() (the CURRENT bus, which a
                // project reopened on a different host/layout may not
                // match), before ever reaching ApplyInputSelection() --
                // same discipline as the visible-bank-index check just
                // above, and the same reason: a value this document names
                // is a claim from a document, never a fact about the
                // currently running host, and a claim that does not fit is
                // dropped rather than trusted.
                const synth::JSON inputSelectionJson = root.Get(kSessionExtrasKey).Get(kInputSelectionKey);
                if (IsJsonInteger(inputSelectionJson)) {
                    const std::int64_t requestedInputSelection = inputSelectionJson.IntegerValue();
                    const std::vector<std::string> labels = ComputeInputOptionLabels();
                    if (requestedInputSelection >= 0 &&
                        static_cast<std::uint64_t>(requestedInputSelection) < labels.size()) {
                        ApplyInputSelection(static_cast<int>(requestedInputSelection));
                    }
                }
            } else {
                const std::lock_guard<std::mutex> lock(stateBlockMutex_);
                pendingRestoreJsonText_ = std::move(*restoreText);
            }
        }
    }

    synth::MessageOut response;
    while (engine_.Context().patchOutputBus->Pop(response)) {
        if (response.type != synth::MessageOut::Type::SerializedJSON || !pendingStateSnapshotRequestId_.has_value() ||
            response.requestId != *pendingStateSnapshotRequestId_) {
            continue;
        }
        // Sibling key, attached to the core's own response tree using its
        // own (aliased) arena before dumping -- see this method's own
        // header comment for why this specific window (after the pop,
        // before pendingStateSnapshotRequestId_ is cleared below) is safe:
        // the single-outstanding-request gate means the audio thread
        // cannot be touching this arena again until this method itself
        // issues a new request, which happens later, further down this
        // function, only after this reset() below has run.
        synth::JsonArena& responseArena = *response.document.arena;
        synth::JSON sessionExtras = responseArena.Object();
        sessionExtras.SetNew(kFreezeLatchedKey, responseArena.Boolean(engine_.Application().FreezeLatched()));
        // Same sibling-key treatment, read fresh at attach time from the
        // SAME live selection state the editor itself renders from --
        // FroggersVisibleBankIndex(), which the editor's own
        // CurrentBankIndex() also delegates to -- never FroggersAppCore::
        // ActiveBankIndex(), which can differ from the visible page while a
        // host automation write is in flight (see that accessor's own
        // comment). Safe to read here, off the audio thread, for the same
        // reason the editor's own BuildTree() reads the same uiState from
        // the message thread every refresh.
        sessionExtras.SetNew(kVisibleBankIndexKey,
                              responseArena.Integer(static_cast<std::int64_t>(synth_froggers::FroggersVisibleBankIndex(engine_.Context()))));
        // Same sibling-key treatment, read fresh from inputSelection_ --
        // this class's own single write path is ApplyInputSelection(), so
        // there is nothing to go stale between transitions the way a
        // polled value could.
        sessionExtras.SetNew(kInputSelectionKey, responseArena.Integer(static_cast<std::int64_t>(inputSelection_)));
        response.document.root.SetNew(kSessionExtrasKey, sessionExtras);

        if (char* dumped = response.document.root.Dumps(JSON_ENCODE_ANY)) {
            std::string text(dumped);
            std::free(dumped);
            const std::lock_guard<std::mutex> lock(stateBlockMutex_);
            cachedStateJsonText_ = std::move(text);
        }
        pendingStateSnapshotRequestId_.reset();
    }

    if (!pendingStateSnapshotRequestId_.has_value()) {
        const std::uint64_t requestId = nextStateRequestId_++;
        if (engine_.Context().patchInputBus->Push(
                synth::PatchMessageIn::SerializeToJSON(requestId, kSessionStatePatchName))) {
            pendingStateSnapshotRequestId_ = requestId;
        }
    }
}

}  // namespace frogg3rs_vst

// JUCE plugin-client entry point (juce_audio_plugin_client, every format
// wrapper calls createPluginFilterOfType() -> this): production processor
// only, never the test constructor overload.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new frogg3rs_vst::FroggersPluginProcessor();
}
