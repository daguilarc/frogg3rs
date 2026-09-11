// FroggersVstEditorTest.cpp -- plugin editor construction/destruction
// tests, extended to verify the action-handler seam
// (FroggersPluginEditor.hpp/.cpp's own "Refresh cadence" header comment) and
// a headless coordinate-transform guard. Isolated in its OWN CTest
// binary, deliberately separate from FroggersVstHostTests.cpp's cases: this
// is the ONE place in the app/vst/ test suite that constructs a real
// juce::Component (FroggersPluginEditor, via synth_juce::PortableComponent)
// rather than only a juce::AudioProcessor. A headless CTest process has no
// guarantee a JUCE GUI Component can be constructed/shown safely -- JUCE's
// GUI backend generally expects a live juce::MessageManager + a real
// windowing session for full operation, and this repo's OTHER app/vst/ test
// binaries (FroggersVstSmokeTest.cpp, FroggersVstHostTests.cpp) have never
// needed to find out, since they only ever construct AudioProcessors.
// Keeping this experiment in its own process means a hard crash/abort here
// (SIGSEGV/SIGABRT/etc) reports as exactly one failed CTest entry and never
// takes the other, already-solid processor-level cases down with it.
//
// juce::ScopedJuceInitialiser_GUI IS used here (main(), below) -- unlike
// this file's original cut, which tried the plain-construction path first
// and found it unnecessary for mere Component construction/resize. Test 2
// below needs juce::MessageManager::callAsync's posted lambda to actually be
// DELIVERED (via PumpPendingCallAsync(), this file's own helper -- see its
// comment for why that is not simply MessageManager::runDispatchLoopUntil()),
// a stronger requirement than construction alone; External/Sheaf/projects/
// synth/juce/PortableJuceBackendTests.cpp:1649 (Sheaf's own PortableComponent
// test binary, read-only precedent, NOT modified by this file) already
// establishes that this initialiser is safe and sufficient for real
// juce::Component use in this exact headless environment, so it is used
// here rather than re-discovered by trial and error.
//
// Three things this file proves, each its own TEST_CASE below:
//   1. Lifecycle: FroggersPluginEditor's constructor and
//      destructor both run to completion, twice in a row on the SAME
//      processor, including a resized() call at a NON-design size (the
//      scale-to-fit transform path) and one repaint-hook tick via the real
//      production timerCallback() seam. Does NOT prove absence of a leak
//      (needs a sanitizer/leak-checker run) or correct on-screen appearance
//      (needs a real window -- a real operator DAW smoke gate).
//   2. Action-handler wiring: dispatching an action
//      through the REAL processor.EditorSurface() -- the same surface
//      FroggersPluginEditor's constructor registers its action handler on
//      -- must eventually refresh the RENDERER (an actual rendered JUCE
//      control's own queryable state, not a spy on this test's own
//      dispatch call) through ONLY the callAsync-deferred action-handler
//      seam, never through processor.PumpMessageThreadForTest()
//      (== timerCallback(), the pre-fix 30Hz-only refresh path). Uses the
//      production froggers.scene.blend slider/action
//      (synth_froggers::FroggersNodeIds/FroggersActions::kSceneBlend,
//      app/FroggersUiSurface.hpp) because it is a real Slider node with a
//      publicly queryable juce::Slider::getValue() -- most of this surface's
//      transport controls are hand-painted Draw nodes with no queryable
//      value at all (see PortableJuceBackend.hpp's RetainedDrawComponent,
//      a private nested class not even nameable from outside).
//      Positive control: kSceneBlend's value is DSP-engine-
//      owned (ParameterModulation.cpp:3700 is the ONLY writer of
//      uiState->sceneBlend, published from inside Engine::ProcessBlock,
//      throttled to once every uiPublishInterval_ blocks -- Engine.hpp:
//      292-298 computes round(sampleRate / (uiFrameHz(30) * blockSize)) = 6
//      at this test's 48kHz/256 setup, FroggersAppCore.hpp's `Config` sets
//      uiFrameHz=30) -- so the test drains processBlock() in a bounded loop
//      and asserts the SURFACE's own BuildTree() actually observed the new
//      value BEFORE drawing any conclusion from the renderer's silence.
//      processBlock() itself never touches portableSurface_ or the 30Hz
//      editor timer (confirmed by reading FroggersPluginProcessor.cpp's
//      processBlock()/timerCallback() -- the repaint hook is invoked ONLY
//      from timerCallback()), so draining it cannot by itself explain a
//      renderer refresh.
//   3. Headless AffineTransform coordinate inversion: guards the exact bug
//      class FroggersPluginEditor::resized()'s scale-to-fit transform
//      (FroggersPluginEditor.cpp) opens up -- if juce::Component's
//      getLocalPoint()/getLocalArea() ever stopped correctly inverting a
//      non-1.0 AffineTransform::scale, drag/click coordinates on a
//      host-resized editor would land on the wrong control. Needs no
//      window/peer: two plain juce::Components in a parent/child
//      relationship, resolved purely as geometry.

#include "FroggersPluginEditor.hpp"
#include "FroggersPluginProcessor.hpp"
#include "FroggersUiSurface.hpp"
#include "synth/PortableUI.hpp"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#if JUCE_MAC
// See PumpPendingCallAsync()'s own comment below for why this is needed.
#include <CoreFoundation/CoreFoundation.h>
#elif JUCE_WINDOWS
// Same reason, other platform: the message pump primitive, reached directly.
// NOMINMAX and WIN32_LEAN_AND_MEAN keep windows.h from defining min/max as
// macros and from dragging in headers this file has no use for.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <chrono>
#endif

#include <cmath>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Same self-contained TEST_CASE/REQUIRE_TRUE idiom as this file's sibling
// FroggersVstHostTests.cpp (that file's own lines 93-96: reproduced
// per-file rather than shared via a new header, matching every app/*Tests.cpp
// file's existing convention).
struct TestCase {
    const char* name;
    void (*fn)();
};

std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Register {
    Register(const char* name, void (*fn)()) { Registry().push_back({name, fn}); }
};

#define TEST_CASE(name)                \
    void name();                       \
    Register reg_##name(#name, &name); \
    void name()

#define REQUIRE_TRUE(expr)                                                       \
    do {                                                                         \
        if (!(expr)) {                                                           \
            std::ostringstream oss;                                              \
            oss << __FILE__ << ":" << __LINE__ << " requirement failed: " #expr; \
            throw std::runtime_error(oss.str());                                 \
        }                                                                        \
    } while (false)

// Same shape as FroggersVstHostTests.cpp's ScratchDataPaths(), parameterized
// by test name so each TEST_CASE's processor gets its own scratch root.
synth::RuntimeDataPaths ScratchDataPaths(const char* testName) {
    const std::filesystem::path dataRoot =
        std::filesystem::temp_directory_path() / "froggers-vst-editor-test" / testName;
    std::filesystem::remove_all(dataRoot);
    synth::RuntimeDataPaths paths = synth::RuntimeDataPaths::FromDataRoot(dataRoot);
    std::filesystem::create_directories(paths.patchesRoot);
    return paths;
}

// Same BuildFroggersTreeAt/FindNodeById technique as FroggersVstHostTests.cpp
// (that file's own lines 500-512) -- reproduced here rather than shared,
// same per-file-self-contained convention.
const synth::ui::Node* FindNodeById(const synth::ui::NodeTree& tree, const std::string& id) {
    for (const synth::ui::Node& node : tree.nodes) {
        if (node.id.value == id) {
            return &node;
        }
    }
    return nullptr;
}

// Delivers a pending juce::MessageManager::callAsync() message without a
// real host's event loop. NOT juce::MessageManager::runDispatchLoopUntil():
// that method only exists when JUCE_MODAL_LOOPS_PERMITTED is set, and it
// defaults to 0 for every JUCE target that does not explicitly opt in
// (JUCE's own juce_PlatformDefs.h) -- this project never does,
// and flipping that flag would have to happen on FroggersVst, the SAME
// shared-code target the real VST3/AU plugin ships from, for a test-only
// need. Instead this pumps the exact underlying primitive
// runDispatchLoopUntil's own body uses
// (JUCE's own juce_MessageManager_mac.mm): a callAsync
// message is signalled onto the MessageQueue member's own captured run loop
// (JUCE's own juce_MessageQueue_mac.h: `CFRunLoopGetMain()` on
// macOS, captured once when JUCE's AppDelegate -- and its MessageQueue
// member -- are first constructed, inside
// MessageManager::doPlatformSpecificInitialisation()). This test binary
// never spawns a second thread, so its one and only thread (the one running
// main(), where juce::ScopedJuceInitialiser_GUI triggers that
// initialisation) IS the process's main thread, making CFRunLoopGetMain()
// and CFRunLoopGetCurrent() the same run loop here -- so pumping "the
// current run loop" reaches the right target.
//
// A SINGLE CFRunLoopRunInMode(..., true) call is NOT enough, though (this
// file's first cut tried exactly that, with a 0.2s budget, and the resulting
// test failed deterministically -- diagnosed by temporarily logging each
// call's CFRunLoopRunResult before landing this fix): `true` for
// returnAfterSourceHandled makes the call return as soon as it handles *any*
// ready source, not specifically JUCE's own -- and empirically, this
// headless process's default-mode run loop has some other source ready on
// effectively every entry (unidentified; irrelevant to the fix, since the
// loop below tolerates any number of unrelated sources firing first), so a
// lone short call reliably returns having handled THAT one instead. The fix
// mirrors runDispatchLoopUntil's OWN structure exactly: loop
// CFRunLoopRunInMode over a bounded TOTAL wall-clock budget rather than
// calling it once, so unrelated sources along the way cannot stop the pump
// before the target message's own turn comes up. 2 seconds comfortably
// exceeds the ~0.7s this took empirically in this environment; the loop
// still returns as soon as the budget is spent, never blocking indefinitely.
//
// Windows reaches the same primitive from the other side. JUCE delivers
// callAsync there by posting to the message-thread's own queue
// (juce_events/native/juce_MessageManager_windows.cpp: a hidden window
// created during doPlatformSpecificInitialisation, posted to with
// PostMessage), so draining that thread's queue with PeekMessage/Dispatch is
// what CFRunLoopRunInMode is on macOS. The same bounded-total-budget loop
// shape is kept for the same reason: one drain is not enough when unrelated
// messages are ready first, and the budget is what stops it blocking.
//
// FIRST ATTEMPT. The macOS half is verified by this suite having passed on
// macOS for as long as it has existed; nothing has ever run this half, and it
// cannot be exercised on the machine it was written on. A failure here is
// expected discovery about JUCE's Windows message delivery, not a regression
// in the wiring the test is actually about.
void PumpPendingCallAsync() {
#if JUCE_MAC
    const CFAbsoluteTime deadline = CFAbsoluteTimeGetCurrent() + 2.0;
    while (CFAbsoluteTimeGetCurrent() < deadline) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, true);
    }
#elif JUCE_WINDOWS
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        MSG message;
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
        Sleep(5);
    }
#else
    REQUIRE_TRUE(false && "PumpPendingCallAsync() has no implementation for this platform -- see its own comment");
#endif
}

// -- 1. Lifecycle --------------------------------------------------------------
// What this DOES prove, if it passes: FroggersPluginEditor's constructor and
// destructor both run to completion, twice in a row on the SAME processor
// (the governing requirement: "constructed/destroyed repeatedly by hosts;
// must not leak or double-register"), including a resized() call at a
// NON-design size (so the scale-to-fit transform path runs at least once)
// and one repaint-hook tick via the real production timerCallback() seam.
// What it does NOT prove: absence of a leak (needs a sanitizer/leak-checker
// run, out of this binary's own scope) or correct on-screen appearance
// (needs a real window -- a real operator DAW smoke gate).
TEST_CASE(editor_constructs_and_destructs_cleanly_twice_headless) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("lifecycle"));

    {
        frogg3rs_vst::FroggersPluginEditor editor(processor);
        // Exercise resized()'s scale-to-fit transform path at a size other
        // than the 900x712 design default (constructor already exercised
        // the design-size path via its own setSize() call).
        editor.setSize(1200, 900);
        // Drive the real production repaint-hook seam once while the editor
        // is alive (FroggersPluginProcessor::timerCallback(), via the same
        // deterministic test pump every other app/vst/ test uses).
        processor.PumpMessageThreadForTest();
    }  // ~FroggersPluginEditor: must clear both the repaint hook and the
       // action handler (see FroggersPluginEditor.cpp's destructor comment).

    // Second construct/destruct cycle on the SAME processor -- the lifecycle
    // requirement above. If either hook were left dangling from the
    // first editor, this would not necessarily crash (both are single-slot,
    // so the second editor's hooks just silently replace stale ones) --
    // what THIS run proves is that a second cycle completes cleanly with no
    // crash/hang, not the absence of that narrower defect.
    {
        frogg3rs_vst::FroggersPluginEditor editor2(processor);
        processor.PumpMessageThreadForTest();
    }

    processor.releaseResources();
    std::cout << "  editor constructs/destructs cleanly, twice, on one processor.\n";
}

// -- 2. Action-handler wiring ---------------------------------------------------
// See this file's own header comment's "Action-handler wiring" section for the full trace and the
// positive-control reasoning. Short version: dispatch a real action through
// the real surface, prove the surface's OWN state actually changed (so a
// later renderer mismatch cannot be blamed on the action never landing),
// then prove the RENDERER only catches up once the JUCE message loop is
// pumped -- never merely because the action was dispatched, and never
// through processor.PumpMessageThreadForTest().
TEST_CASE(dispatching_an_action_refreshes_the_renderer_through_the_action_handler_with_no_timer_tick) {
    frogg3rs_vst::FroggersPluginProcessor processor(ScratchDataPaths("action_handler_refresh"));
    frogg3rs_vst::FroggersPluginEditor editor(processor);

    // Neutralize the OTHER path that can also call RefreshFromSurface() --
    // FroggersPluginEditor's own constructor ALSO wires the pre-existing 30Hz
    // repaint hook (FroggersPluginProcessor::SetEditorRepaintHook), which
    // this class's real juce::Timer drives independently of any action
    // dispatch. This test's own juce::ScopedJuceInitialiser_GUI (main(),
    // below) means juce::MessageManager::getInstanceWithoutCreating() is
    // non-null when `processor` above is constructed, so
    // FroggersPluginProcessor's constructor DOES start that real timer here
    // (unlike FroggersVstHostTests.cpp/FroggersVstSmokeTest.cpp, which never
    // construct a MessageManager at all, so that guard is false there) --
    // and PumpPendingCallAsync() below pumps the run loop for up to two real
    // seconds, comfortably long enough for that ~33ms-period timer to fire
    // and refresh the renderer through the repaint-hook path ON ITS OWN,
    // even with the action-handler seam under test entirely disconnected.
    // Caught empirically: with the action-handler wiring temporarily
    // reverted (non-vacuousness proof, verification pass), this test still
    // PASSED before this line was added -- the real timer alone was enough.
    // Clearing the repaint hook here removes that confound, so a renderer
    // refresh observed below can only be explained by the action-handler/
    // callAsync seam this test exists to guard.
    processor.SetEditorRepaintHook({});

    // Real audio-callback lifecycle -- needed ONLY so processBlock() below
    // can drain the scene-blend message into uiState (see header comment).
    // Same 48kHz/256-sample shape FroggersVstHostTests.cpp's own cases use.
    processor.setRateAndBufferSizeDetails(48000.0, 256);
    processor.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;

    synth_juce::PortableComponent& renderer = editor.RendererForTest();
    const auto rendererSliderValue = [&renderer]() -> double {
        auto* slider =
            dynamic_cast<juce::Slider*>(renderer.FindByNodeId(synth_froggers::FroggersNodeIds::kSceneBlend));
        REQUIRE_TRUE(slider != nullptr);
        return slider->getValue();
    };
    const auto surfaceValue = [&processor]() -> float {
        const synth::ui::NodeTree tree = processor.EditorSurface().BuildTree();
        const synth::ui::Node* node = FindNodeById(tree, synth_froggers::FroggersNodeIds::kSceneBlend);
        REQUIRE_TRUE(node != nullptr);
        return node->value;
    };

    const double baseline = rendererSliderValue();
    // Displayed range is [1.0, 2.0] (AppendSceneBlendGroup(),
    // app/FroggersUiSurface.hpp) -- pick whichever extreme differs from the
    // baseline so the test cannot pass by accident if the baseline already
    // sits at one end.
    const bool baselineNearHigh = std::abs(baseline - 2.0) < 0.05;
    const char* targetLiteral = baselineNearHigh ? "1.0" : "2.0";
    const double target = baselineNearHigh ? 1.0 : 2.0;
    REQUIRE_TRUE(std::abs(baseline - target) > 0.05);

    // The real production seam: the SAME surface instance
    // FroggersPluginEditor's constructor registered its action handler on
    // (FroggersPluginProcessor::EditorSurface() returns
    // engine_.Application().PortableSurface(), a single shared instance).
    processor.EditorSurface().DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kSceneBlend, targetLiteral));

    // -- Positive control ------------------------------------------------------
    // Prove the dispatched value actually propagated into the surface's own
    // live state before drawing any conclusion from the renderer's silence.
    // Bounded to comfortably clear the computed 6-block publish throttle
    // (see header comment); processBlock() cannot itself explain a renderer
    // refresh (it never touches portableSurface_ or the 30Hz editor timer).
    bool publishedToSurface = false;
    for (int i = 0; i < 32 && !publishedToSurface; ++i) {
        buffer.clear();
        processor.processBlock(buffer, midi);
        publishedToSurface = std::abs(static_cast<double>(surfaceValue()) - target) < 0.01;
    }
    REQUIRE_TRUE(publishedToSurface);

    // The RENDERER must NOT have moved yet -- RefreshFromSurface() only ever
    // runs from FlushDeferredRefresh(), posted via callAsync above and not
    // yet pumped. This is the crux: the surface already agrees with target
    // (just proved), so if the renderer already agreed too, that could only
    // mean something refreshed it synchronously or via a path this test
    // never touched -- suspicious either way.
    REQUIRE_TRUE(std::abs(rendererSliderValue() - baseline) < 0.01);
    REQUIRE_TRUE(std::abs(rendererSliderValue() - target) > 0.05);

    // Pump the underlying run loop DIRECTLY -- NOT
    // processor.PumpMessageThreadForTest() (== timerCallback(), the pre-fix
    // 30Hz-only refresh path this test must not rely on). The only thing
    // that can deliver the pending callAsync here is the action-handler
    // wiring under test. See PumpPendingCallAsync()'s own comment (below)
    // for why this is not simply MessageManager::runDispatchLoopUntil().
    PumpPendingCallAsync();

    REQUIRE_TRUE(std::abs(rendererSliderValue() - target) < 0.01);

    processor.releaseResources();
    std::cout << "  dispatched action refreshed the renderer via the action-handler/callAsync "
                 "seam alone, with processor.PumpMessageThreadForTest() never called.\n";
}

// -- 3. Headless AffineTransform coordinate inversion -------------------------
// Guards the drag-coordinate bug class FroggersPluginEditor::resized()'s
// scale-to-fit transform opens up (see this file's header comment's
// "Headless AffineTransform coordinate inversion" section).
// Mirrors that method's own shape exactly: a design-space child fixed at
// position (0,0), transformed into a differently-sized parent via
// AffineTransform::scale(...).translated(...). Ground truth is computed via
// AffineTransform's OWN inversion (transform.inverted()), an independent
// codepath from Component's hierarchy-walking getLocalPoint()/getLocalArea()
// -- so this checks that two independently-implemented JUCE mechanisms
// agree, not that a hand-derived formula matches whatever the code happens
// to do.
TEST_CASE(component_get_local_point_and_area_invert_a_non_unit_affine_scale_headlessly) {
    juce::Component parent;
    parent.setBounds(0, 0, 1200, 900);

    juce::Component child;
    parent.addAndMakeVisible(child);
    // Same shape as portableSurface_.setBounds(0, 0, kDesignWidth,
    // kDesignHeight) in FroggersPluginEditor::resized(): position (0,0), so
    // the child's own local coordinates coincide with its pre-transform
    // parent-relative coordinates.
    child.setBounds(0, 0, 900, 712);

    // A non-1.0, non-trivial scale/offset pair, same shape as resized()'s
    // own computed AffineTransform::scale(scale).translated(offsetX,
    // offsetY) -- scale within FroggersPluginEditor's own [0.5x, 2.0x]
    // resize-limit range (kMinScale/kMaxScale, FroggersPluginEditor.cpp).
    const juce::AffineTransform transform = juce::AffineTransform::scale(0.6f).translated(45.0f, 30.0f);
    child.setTransform(transform);
    const juce::AffineTransform inverse = transform.inverted();

    // -- getLocalPoint: no window/peer needed, pure hierarchy geometry. -----
    const juce::Point<float> editorPoint(300.0f, 200.0f);
    const juce::Point<float> expectedPoint = editorPoint.transformedBy(inverse);
    const juce::Point<float> actualPoint = child.getLocalPoint(&parent, editorPoint);
    REQUIRE_TRUE(std::abs(actualPoint.x - expectedPoint.x) < 1e-3f);
    REQUIRE_TRUE(std::abs(actualPoint.y - expectedPoint.y) < 1e-3f);
    // Non-vacuousness: the point actually moved under inversion (a 1.0-scale
    // identity transform would make this assertion trivially true even with
    // a broken inversion).
    REQUIRE_TRUE(std::abs(actualPoint.x - editorPoint.x) > 1.0f);

    // -- getLocalArea: same inversion, over a rectangle. A pure scale+
    // translate keeps a rectangle a rectangle (no rotation/shear), so
    // getLocalArea's own "smallest rectangle containing the transformed
    // area" fallback (its doc comment) is exact here, not an approximation.
    const juce::Rectangle<float> editorArea(300.0f, 200.0f, 60.0f, 40.0f);
    const juce::Rectangle<float> expectedArea = editorArea.transformedBy(inverse);
    const juce::Rectangle<float> actualArea = child.getLocalArea(&parent, editorArea);
    REQUIRE_TRUE(std::abs(actualArea.getX() - expectedArea.getX()) < 1e-3f);
    REQUIRE_TRUE(std::abs(actualArea.getY() - expectedArea.getY()) < 1e-3f);
    REQUIRE_TRUE(std::abs(actualArea.getWidth() - expectedArea.getWidth()) < 1e-3f);
    REQUIRE_TRUE(std::abs(actualArea.getHeight() - expectedArea.getHeight()) < 1e-3f);
    REQUIRE_TRUE(std::abs(actualArea.getWidth() - editorArea.getWidth()) > 1.0f);

    std::cout << "  [transform] getLocalPoint/getLocalArea invert a 0.6x-scaled, offset AffineTransform "
                 "exactly, headlessly.\n";
}

}  // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI juce;

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
