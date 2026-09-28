#pragma once

// frogg3rs_vst::FroggersPluginEditor -- the plugin editor hosting the
// portable surface composed with the runtime sidebar (spec froggers-vst-host,
// "Editor hosts the portable surface" and "The sidebar holds Controllers and
// File"): the plugin editor SHALL render the same portable app surface the
// standalone launcher renders, composed with the runtime sidebar through the
// same runtime main component the standalone and browser use, declaring the
// Controllers and File pages only.
//
// ============================================================================
// Render-host seam trace (read start-to-finish before writing this
// file) -- cites the ACTUAL files, not a summary of them:
// ============================================================================
//   - app/FroggersMain.cpp's launcher session builds a
//     synth_runtime::RuntimeShellSession<FroggersApp> (Sheaf
//     External/Sheaf/projects/synth/runtime/Shell.hpp), which owns a synth_runtime::Runtime<App>
//     (runtime/Runtime.hpp -- the AudioDeviceManager/MIDI-connection-manager/
//     window/timer machinery a STANDALONE app needs) PLUS a
//     synth_runtime::ShellComponent<App> (External/Sheaf/projects/synth/runtime/Shell.hpp), itself a thin
//     juce::Component host for ONE synth_runtime::MainPane<App>
//     (runtime/MainPane.hpp).
//   - MainPane<App>'s constructor (External/Sheaf/projects/synth/runtime/MainPane.hpp) builds THREE things:
//     a JuceRuntimeMainServices<App> (device-manager/MIDI-connection glue
//     the Audio/Controllers/Sync/File SIDEBAR pages need), a
//     synth::runtime_ui::RuntimeMainComponent<App, Services>
//     (`mainComponent_` -- itself a synth::ui::Surface implementation that
//     COMPOSES the wrapped app's own surface tree together with the
//     sidebar's, per that class's own BuildTree()), and a
//     synth_juce::PortableComponent (`renderer_(mainComponent_)`,
//     External/Sheaf/projects/synth/juce/PortableJuceBackend.hpp)
//     -- the actual JUCE-side renderer that walks a synth::ui::NodeTree and
//     creates/lays out real juce::Component controls for it.
//   - THIS EDITOR follows the SAME shape as MainPane, minus the pieces the
//     DAW itself owns: FroggersPluginServices (FroggersPluginServices.hpp)
//     is this editor's own Services -- Controllers and File bound to the
//     processor's engine and MidiConnectionManager, Audio/Sync/deadline
//     no-ops -- a synth::runtime_ui::RuntimeMainComponent<FroggersApp,
//     FroggersPluginServices> declaring only Controllers and File
//     (RuntimeSidebarPages), and a synth_juce::PortableComponent
//     over that component. No Runtime<App> (the DAW owns audio devices --
//     governing spec: "no audio-device page"), no MainPane/ShellComponent
//     (this class IS the plugin's own thin host, JUCE's own
//     AudioProcessorEditor rather than a plain juce::Component).
//   - Plugin-mode chrome exclusion (Play/Stop/Record suppressed, Freeze
//     "FREEZE"-labelled) is NOT implemented in this file at all --
//     FroggersUiSurface::SetPluginHostMode(true)
//     (app/FroggersUiSurface.hpp) is called ONCE, in
//     FroggersPluginProcessor's OWN constructor (see that file's own
//     comment for the exact call site and the static_cast justification),
//     on the surface instance the composed tree embeds. This editor just
//     renders whatever tree RuntimeMainComponent::BuildTree() hands back,
//     unconditionally -- exactly what "so surface improvements reach the
//     plugin without a parallel UI" requires: zero plugin-specific UI
//     branching lives in this class.
//
// ============================================================================
// Refresh cadence: two action handlers, one deferred refresh
// ============================================================================
// A dispatched action can reach the composed tree by two routes:
// mainComponent_'s OWN surfaces (the sidebar, Controllers and File pages --
// RuntimeMainComponent::DispatchAction routes these itself and then always
// calls its own actionHandler_) or the wrapped app surface
// (FroggersUiSurface, reached either directly through processor_.
// EditorSurface() -- an encoder drag or a controller-row action pushed from
// MidiConnectionManager's forwarding processor -- or through
// RuntimeMainComponent::DispatchAction's own "not a runtime action" fallback,
// which calls app_.PortableSurface().DispatchAction(action), i.e. the same
// FroggersUiSurface, so FroggersUiSurface's own outerHandler_ fires too).
// Registering the SAME deferred-refresh callback on both -- mainComponent_.
// SetActionHandler(...) and processor_.EditorSurface().SetActionHandler(...)
// -- covers both routes: an action that reaches only one of the two paths (a sidebar click,
// or an encoder drag delivered straight to the app surface with no editor
// open) still schedules exactly one refresh. A fast mouse-drag or a rapid
// controller message can post the SAME callback twice in one message-loop
// turn; ScheduleDeferredRefresh()'s single-slot coalescing (below) makes the
// second call a no-op, so this never double-refreshes, only ever
// double-schedules a refresh that already coalesces to one.
//
// DispatchAction runs INSIDE the currently-firing juce::Component callback
// (e.g. RetainedDrawComponent::mouseDrag, PortableJuceBackend.hpp) --
// refreshing synchronously there means RebuildControls() could destroy the
// very component whose own callback is still on the call stack, if the
// dispatched action ever changes which nodes exist (a drill-in/out, a page
// navigation, e.g.). So every action is deferred, UNCONDITIONALLY, via
// juce::MessageManager::callAsync + a single-pending-flag coalescing idiom
// (mirroring MainPane's own RefreshRendererAfterAction/
// FlushDeferredRendererRefresh, Sheaf External/Sheaf/projects/synth/runtime/MainPane.hpp, minus its
// conditional NeedsDeferredRendererRefresh classification -- this class has
// no equivalent notion of which actions are safe to refresh synchronously,
// so it defers all of them) -- correct by construction (never destroys a
// live callback's own component).
//
// ============================================================================
// Sizing ("resizable per the surface's own sizing conventions")
// ============================================================================
// The composed tree (app surface plus sidebar) resolves at a fixed,
// compiled-in extent -- RuntimeMainComponent::IntrinsicBounds(), the app's
// own 900x712 (synth_froggers::FroggersPageLayout::kDefaultWidth/
// kDefaultHeight) plus the sidebar's 96-pixel column -- NEVER a live window
// extent, the same reasoning FroggersUiSurface's own layout carries (that
// struct's own header comment: "making the layout track the ACTUAL window
// requires an upstream shell change"). Resizing THIS editor's JUCE window
// can never, by itself, make the composed tree relayout.
//
// The browser host (app/browser/**, read-only precedent, NOT modified by
// this file) solved the identical problem for viewport-adaptive sizing:
// app/browser/site/viewport-width.mjs's own header comment says the
// composite "runtime.main.root fitSurface actually scales" -- i.e. a
// uniform CSS `transform: scale(...)` applied to the surface's rendered
// root, leaving the surface's OWN internal (wire-space) layout completely
// untouched. This class applies the exact same technique in JUCE terms:
// portableSurface_ always stays sized at the fixed design extent
// (IntrinsicBounds()), and resized() computes a uniform, aspect-preserving,
// centered juce::AffineTransform::scale(...) to fit whatever size the host
// gives this editor, applied via Component::setTransform() -- which JUCE
// propagates through the whole rendered subtree for both PAINTING and
// MOUSE-EVENT hit-testing, so encoder drag/click and sidebar/page clicks
// keep working correctly at any host-chosen size.
//
// The ? button is anchored to
// the sidebar column in the SAME design-space coordinates BuildSidebarTree
// resolves the sidebar into, then mapped through the SAME transform as
// portableSurface_ -- so it stays visually attached to the sidebar, below
// its last declared row, at every host-chosen size.

#include "synth/PortableUI.hpp"
#include "synth/RuntimeMainComponent.hpp"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "FroggersPluginServices.hpp"
#include "PortableJuceBackend.hpp"

namespace frogg3rs_vst {

class FroggersPluginEditor final : public juce::AudioProcessorEditor {
public:
    explicit FroggersPluginEditor(FroggersPluginProcessor& processor);
    // MUST clear BOTH the processor's editor-repaint hook AND both action
    // handlers (mainComponent_'s and the app surface's) before mainComponent_
    // and portableSurface_ (members, destroyed right after this body
    // returns) tear down -- see the .cpp definition's own comment for the
    // full ordering contract, which mirrors
    // synth_runtime::RuntimeShellSession::~RuntimeShellSession (Sheaf
    // External/Sheaf/projects/synth/runtime/Shell.hpp) exactly for the repaint hook, extended
    // to the second action handler this class registers.
    ~FroggersPluginEditor() override;

    FroggersPluginEditor(const FroggersPluginEditor&) = delete;
    FroggersPluginEditor& operator=(const FroggersPluginEditor&) = delete;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

    // Test-only: exposes the actual renderer so a test can assert on
    // OBSERVABLE RENDERED control state (e.g. FindByNodeId(id) cast to the
    // concrete juce::Component subtype, then its own public getter) rather
    // than on the surface's live state, which updates SYNCHRONOUSLY inside
    // DispatchAction and therefore cannot distinguish "the action applied"
    // from "the renderer actually refreshed" -- exactly the distinction the
    // action-handler-wiring test needs. Named for its test-only caller,
    // matching this repo's existing PumpMessageThreadForTest()/
    // ApplicationForTest() convention (FroggersPluginProcessor.hpp).
    synth_juce::PortableComponent& RendererForTest() { return portableSurface_; }

    // Test-only: the ? button's own real-editor-pixel bounds, the same
    // coordinate space getLocalArea() maps a rendered sidebar node's bounds
    // into (see FroggersVstEditorTest.cpp's own sidebar-overlap check) --
    // helpButton_ itself is private, unlike portableSurface_ above, because
    // nothing but its bounds is ever asserted on.
    juce::Rectangle<int> HelpButtonBoundsForTest() const { return helpButton_.getBounds(); }

private:
    // Registered as BOTH mainComponent_'s and processor_.EditorSurface()'s
    // action handler (this file's header comment, "Refresh cadence" section):
    // schedules AT MOST ONE pending juce::MessageManager::callAsync refresh
    // at a time, mirroring MainPane::RefreshRendererAfterAction/
    // FlushDeferredRendererRefresh (Sheaf External/Sheaf/projects/synth/runtime/MainPane.hpp) minus its
    // conditional NeedsDeferredRendererRefresh classification (unconditional
    // defer, justified in this file's header comment).
    void ScheduleDeferredRefresh();
    void FlushDeferredRefresh();

    FroggersPluginProcessor& processor_;
    // Declared in dependency/teardown order: services_ is what mainComponent_
    // is built over (its Controllers-page binding and file service must
    // outlive it), and mainComponent_ is the synth::ui::Surface& portableSurface_
    // renders (it must outlive the renderer). Reverse-declaration-order
    // destruction tears these down portableSurface_, then mainComponent_,
    // then services_ -- the opposite of construction, and the only safe
    // order.
    FroggersPluginServices services_;
    synth::runtime_ui::RuntimeMainComponent<synth_froggers::FroggersApp, FroggersPluginServices> mainComponent_;
    synth_juce::PortableComponent portableSurface_;
    // Message-thread-owned (ScheduleDeferredRefresh()/FlushDeferredRefresh()
    // both only ever run there -- action dispatch happens on the message
    // thread, the same thread callAsync's posted lambda runs on). Same
    // single-slot coalescing role as MainPane's own
    // deferredRendererRefreshPending_.
    bool deferredRefreshPending_ = false;

    // Operator documentation ships with the plugin (froggers-sheaf-
    // runtime-app spec, "Operator documentation ships with the app"): a
    // small button anchored to the sidebar column, entirely outside the
    // composed node tree, that opens the manual/quick dictionary this
    // plugin bundles at build time (see FroggersBundledDocs.hpp). Added and
    // made visible AFTER portableSurface_ in the .cpp so it sits on top of
    // it in z-order and reliably receives its own clicks.
    juce::TextButton helpButton_{"?"};
};

}  // namespace frogg3rs_vst
