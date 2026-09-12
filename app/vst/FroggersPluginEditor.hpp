#pragma once

// frogg3rs_vst::FroggersPluginEditor -- the plugin editor hosting the
// portable surface (spec froggers-vst-host, "Editor hosts the portable
// surface": the plugin editor
// SHALL render the SAME portable app surface the standalone launcher
// renders -- minus DAW-owned chrome -- through the SAME portable renderer,
// so surface improvements reach the plugin without a parallel UI).
//
// ============================================================================
// Render-host seam trace (read start-to-finish before writing this
// file) -- cites the ACTUAL files, not a summary of them:
// ============================================================================
//   - app/FroggersMain.cpp's launcher session builds a
//     synth_runtime::RuntimeShellSession<FroggersApp> (Sheaf
//     External/Sheaf/projects/synth/runtime/Shell.hpp:73-108), which owns a synth_runtime::Runtime<App>
//     (runtime/Runtime.hpp -- the AudioDeviceManager/MIDI-connection-manager/
//     window/timer machinery a STANDALONE app needs) PLUS a
//     synth_runtime::ShellComponent<App> (External/Sheaf/projects/synth/runtime/Shell.hpp:51-71), itself a thin
//     juce::Component host for ONE synth_runtime::MainPane<App>
//     (runtime/MainPane.hpp).
//   - MainPane<App>'s constructor (External/Sheaf/projects/synth/runtime/MainPane.hpp:36-48) builds THREE things:
//     a JuceRuntimeMainServices<App> (device-manager/MIDI-connection glue
//     the Audio/Controllers/Sync/File SIDEBAR pages need), a
//     synth::runtime_ui::RuntimeMainComponent<App, Services>
//     (`mainComponent_` -- itself a synth::ui::Surface implementation that
//     COMPOSES the wrapped app's own surface tree together with the
//     sidebar's, per that class's own BuildTree()), and a
//     synth_juce::PortableComponent (`renderer_(mainComponent_)`,
//     External/Sheaf/projects/synth/juce/PortableJuceBackend.hpp:212-224)
//     -- the actual JUCE-side renderer that walks a synth::ui::NodeTree and
//     creates/lays out real juce::Component controls for it.
//   - THE SEPARABILITY FINDING: synth_juce::PortableComponent's constructor
//     (External/Sheaf/projects/synth/juce/PortableJuceBackend.hpp:221-224, `explicit PortableComponent(synth::
//     ui::Surface& surface)`) takes EXACTLY ONE dependency: a bare
//     `synth::ui::Surface&`. Nothing in that class's ~1450 lines references
//     Runtime<App>, MainPane<App>, RuntimeMainComponent, or the sidebar --
//     it only ever calls `surface.BuildTree()` (RefreshFromSurface(),
//     :226-233) and `surface.DispatchAction(...)` (DispatchBackendAction(),
//     :817-824), both members of the GENERIC synth::ui::Surface interface
//     (Sheaf External/Sheaf/projects/synth/include/synth/PortableUI.hpp:280-289). MainPane hands it
//     `mainComponent_` (the sidebar-COMPOSED surface) only because that is
//     what the STANDALONE launcher wants rendered -- not because
//     PortableComponent requires that particular Surface implementation.
//   - synth_froggers::FroggersApp::PortableSurface() (app/Froggers.hpp's `PortableSurface`)
//     already returns a plain `synth::ui::Surface&` over `ui_`, a
//     `FroggersUiSurface` member (Froggers.hpp's `ui_`) -- the SAME instance
//     FroggersPluginProcessor's own DispatchAction()/TestStartTransport()/
//     PumpHostParameterBridge() calls already drive (see that file's own
//     header comment).
//   - CONCLUSION: this editor constructs a synth_juce::PortableComponent
//     DIRECTLY over `processor.EditorSurface()` -- the bare app surface, NO
//     RuntimeMainComponent wrapper, NO sidebar, NO Runtime<App>, NO
//     JuceRuntimeMainServices, NO device/MIDI-connection machinery. The
//     renderer IS separable from the full runtime session: this class is
//     the proof. The binding BLOCKED-escalation condition ("the
//     renderer is NOT separable from the runtime session") did not trigger.
//   - What this editor deliberately does NOT construct, and why each is
//     correctly excluded rather than merely omitted: Runtime<App> (owns the
//     AudioDeviceManager/window/MIDI-connection machinery -- the DAW itself
//     owns audio devices for a plugin, governing spec: "no audio-device
//     page"); MainPane<App>/RuntimeMainComponent (the sidebar and its
//     Audio/Controllers/Sync/File pages -- same reason, plus "no internal
//     transport controls" the sidebar has no bearing on either way); and
//     ShellComponent (a thin MainPane host with nothing left for it to
//     wrap once MainPane itself is excluded).
//   - Plugin-mode chrome exclusion (Play/Stop/Record suppressed, Freeze
//     "FREEZE"-labelled) is NOT implemented in this file at all --
//     FroggersUiSurface::SetPluginHostMode(true)
//     (app/FroggersUiSurface.hpp) is called ONCE, in
//     FroggersPluginProcessor's OWN constructor (see that file's own
//     comment for the exact call site and the static_cast justification),
//     on the SAME surface
//     instance this editor renders. This editor just renders whatever tree
//     BuildTree() hands back, unconditionally -- exactly what "so surface
//     improvements reach the plugin without a parallel UI" requires: zero
//     plugin-specific UI branching lives in this class.
//
// ============================================================================
// Refresh cadence: the action-handler seam
// ============================================================================
// A REAL bug in the first cut of this file: it registered ONLY the 30Hz
// repaint hook (FroggersPluginProcessor::SetEditorRepaintHook) and never
// called `processor.EditorSurface().SetActionHandler(...)` at all. Every
// dispatched action (an encoder drag, a click) still applies to the CORE
// immediately (FroggersUiSurface::DispatchAction calls HandleAction()
// synchronously, app/FroggersUiSurface.hpp's `DispatchAction`), but with no action
// handler registered, this editor's OWN redraw was capped at the fixed 30Hz
// timer tick -- visibly stepped dragging, not the smooth per-action tracking
// the SAME surface gives the standalone launcher, whose MainPane wires
// exactly this seam (Sheaf External/Sheaf/projects/synth/runtime/MainPane.hpp:36-48, `mainComponent_.
// SetActionHandler(...)`) and refreshes from it, not (only) from a timer.
//
// FroggersUiSurface::SetActionHandler (app/FroggersUiSurface.hpp's `SetActionHandler`)
// is part of the generic synth::ui::Surface interface (`ActionHandler`,
// Sheaf External/Sheaf/projects/synth/include/synth/PortableUI.hpp:282-289) -- EditorSurface() already
// returns that interface, so registering needs no new accessor.
//
// Safety split, replicated from MainPane (External/Sheaf/projects/synth/runtime/MainPane.hpp:113-159,
// RefreshRendererAfterAction/NeedsDeferredRendererRefresh/callAsync/
// FlushDeferredRendererRefresh), NOT called synchronously from the handler:
// DispatchAction runs INSIDE the currently-firing juce::Component callback
// (e.g. RetainedDrawComponent::mouseDrag, PortableJuceBackend.hpp) --
// refreshing synchronously there means RebuildControls() could destroy the
// very component whose own callback is still on the call stack, if the
// dispatched action ever changes which nodes exist (a drill-in/out, e.g.).
// This class deliberately does NOT reimplement MainPane's conditional
// NeedsDeferredRendererRefresh(action) classification --
// RuntimeMainComponent::NeedsDeferredDispatch
// (External/Sheaf/projects/synth/include/synth/RuntimeMainComponent.hpp:235-238) is `IsControllersAction(action.name) &&
// controllersSurface_.NeedsDeferredDispatch(action)` -- tied to
// RuntimeMainComponent's OWN sidebar/Controllers-page composition, which
// FroggersUiSurface (a bare, single, non-composed Surface) has no
// equivalent of. Every action is deferred, UNCONDITIONALLY, via the exact
// same juce::MessageManager::callAsync + single-pending-flag coalescing
// idiom MainPane uses (so a fast mouse-drag posting many actions per second
// schedules at most ONE pending refresh, not one per action) -- correct by
// construction (never destroys a live callback's own component) rather than
// correct only as long as FroggersUiSurface happens not to need deferral
// today; callAsync typically services on the very next idle message-loop
// slot, still far faster than the 30Hz ceiling this fix replaces.
//
// ============================================================================
// Sizing ("resizable per the surface's own sizing conventions")
// ============================================================================
// FroggersUiSurface resolves its OWN internal layout against a fixed,
// compiled-in 900x712 (synth_froggers::FroggersPageLayout::kDefaultWidth/
// kDefaultHeight, app/FroggersUiSurface.hpp -- that struct's own header
// comment: "a fixed, compiled-in size ... making the layout track the
// ACTUAL window requires an upstream shell change"), NEVER a
// live window extent -- so resizing THIS editor's JUCE window can never, by
// itself, make FroggersUiSurface relayout; the standalone launcher sidesteps
// this by sizing its OWN window from the content's fixed intrinsic size
// (app/FroggersMain.cpp's own header comment on `IntrinsicBounds()`) rather
// than the other way around.
//
// The browser host (app/browser/**, read-only precedent, NOT modified by
// this file) solved the identical problem for viewport-adaptive sizing:
// app/browser/site/mobile-stack.mjs's own header comment says the
// composite "runtime.main.root fitSurface actually scales" -- i.e. a
// uniform CSS `transform: scale(...)` applied to the surface's rendered
// root, leaving the surface's OWN internal (wire-space) layout completely
// untouched. This class applies the exact same technique in JUCE terms:
// portableSurface_ always stays sized at the fixed design extent
// (900x712), and resized() computes a uniform, aspect-preserving,
// centered juce::AffineTransform::scale(...) to fit whatever size the host
// gives this editor, applied via Component::setTransform() -- which JUCE
// propagates through the whole rendered subtree for both PAINTING and
// MOUSE-EVENT hit-testing, so encoder drag/click keeps working correctly
// at any host-chosen size.

#include "synth/PortableUI.hpp"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PortableJuceBackend.hpp"

namespace frogg3rs_vst {

class FroggersPluginProcessor;

class FroggersPluginEditor final : public juce::AudioProcessorEditor {
public:
    explicit FroggersPluginEditor(FroggersPluginProcessor& processor);
    // MUST clear BOTH the processor's editor-repaint hook AND the surface's
    // action handler before portableSurface_ (a member, torn down right
    // after this body returns) is destroyed -- see the .cpp definition's
    // own comment for the full ordering contract, which mirrors
    // synth_runtime::RuntimeShellSession::~RuntimeShellSession (Sheaf
    // External/Sheaf/projects/synth/runtime/Shell.hpp:91-95) exactly for both hooks.
    ~FroggersPluginEditor() override;

    FroggersPluginEditor(const FroggersPluginEditor&) = delete;
    FroggersPluginEditor& operator=(const FroggersPluginEditor&) = delete;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

    // Test-only: exposes the actual renderer so a test can assert on
    // OBSERVABLE RENDERED control state (e.g. FindByNodeId(id) cast to the
    // concrete juce::Component subtype, then its own public getter) rather
    // than on the surface's live state, which updates SYNCHRONOUSLY inside
    // DispatchAction (FroggersUiSurface::DispatchAction, HandleAction() then
    // the registered handler) and therefore cannot distinguish "the action
    // applied" from "the renderer actually refreshed" -- exactly the
    // distinction the action-handler-wiring test needs. Named for its
    // test-only caller, matching this repo's existing
    // PumpMessageThreadForTest()/ApplicationForTest() convention
    // (FroggersPluginProcessor.hpp).
    synth_juce::PortableComponent& RendererForTest() { return portableSurface_; }

private:
    // Registered as processor_.EditorSurface()'s action handler (this file's
    // header comment, "Refresh cadence" section): schedules AT MOST ONE
    // pending juce::MessageManager::callAsync refresh at a time, mirroring
    // MainPane::RefreshRendererAfterAction/FlushDeferredRendererRefresh
    // (Sheaf External/Sheaf/projects/synth/runtime/MainPane.hpp:134-159) minus its conditional
    // NeedsDeferredRendererRefresh classification (unconditional defer,
    // justified in this file's header comment).
    void ScheduleDeferredRefresh();
    void FlushDeferredRefresh();

    FroggersPluginProcessor& processor_;
    // Constructed directly over processor_.EditorSurface() -- see this
    // file's header comment for the full render-host seam trace. Declared
    // AFTER processor_ (member destruction order is declaration-reverse:
    // this is destroyed BEFORE processor_ itself could go away, which
    // matters only if a future edit ever changed processor_'s own
    // lifetime relative to this editor's -- today the host always
    // outlives the editor by construction, JUCE's own AudioProcessorEditor
    // contract).
    synth_juce::PortableComponent portableSurface_;
    // Message-thread-owned (ScheduleDeferredRefresh()/FlushDeferredRefresh()
    // both only ever run there -- action dispatch happens on the message
    // thread, the same thread callAsync's posted lambda runs on). Same
    // single-slot coalescing role as MainPane's own
    // deferredRendererRefreshPending_.
    bool deferredRefreshPending_ = false;

    // Operator documentation ships with the plugin (froggers-sheaf-
    // runtime-app spec, "Operator documentation ships with the app"): a
    // small corner button, entirely outside FroggersUiSurface's own node
    // tree, that opens the manual/quick dictionary this plugin bundles at
    // build time (see FroggersBundledDocs.hpp). Added and made visible
    // AFTER portableSurface_ in the .cpp so it sits on top of it in
    // z-order and reliably receives its own clicks.
    juce::TextButton helpButton_{"?"};
};

}  // namespace frogg3rs_vst
