#include "FroggersPluginEditor.hpp"

#include "FroggersBundledDocs.hpp"
#include "FroggersPluginProcessor.hpp"
#include "FroggersUiSurface.hpp"

#include <algorithm>

namespace frogg3rs_vst {

namespace {

// Resize bounds ("resizable per the surface's own sizing
// conventions"): neither the governing spec nor the design doc names a
// specific limit, only that the editor must be resizable and follow the
// browser's uniform-scale precedent (this file's own header comment) --
// half-to-double the design box is a plain, defensible range (small enough
// that a shrunk grid stays legible, generous enough to be useful on a large
// display) with no further requirement to derive it from.
constexpr float kMinScale = 0.5f;
constexpr float kMaxScale = 2.0f;

// Matches synth_juce::PortableComponent::paint()'s own empty-tree fallback
// fill (PortableJuceBackend.hpp -- `graphics.fillAll(juce::Colour(18, 20,
// 22))`), by inspection (Sheaf tracked file, read-only): so the letterbox
// margins a non-design-ratio host window leaves around the uniformly-scaled
// surface (see resized() below) read as intentional background rather than
// a mismatched gap.
const juce::Colour kBackgroundColour(synth::kSurfaceBackground.r, synth::kSurfaceBackground.g,
                                      synth::kSurfaceBackground.b);

}  // namespace

FroggersPluginEditor::FroggersPluginEditor(FroggersPluginProcessor& processor)
    : juce::AudioProcessorEditor(processor),
      processor_(processor),
      services_(processor_),
      mainComponent_(processor_.GetEngine().Application(), services_, FroggersPluginServices::PluginSidebarPages()),
      portableSurface_(mainComponent_) {
    addAndMakeVisible(portableSurface_);
    setResizable(true, true);
    const synth::ui::Bounds intrinsic = mainComponent_.IntrinsicBounds();
    setResizeLimits(static_cast<int>(intrinsic.width * kMinScale), static_cast<int>(intrinsic.height * kMinScale),
                     static_cast<int>(intrinsic.width * kMaxScale), static_cast<int>(intrinsic.height * kMaxScale));

    // Build the control tree once, immediately -- resized() (fired
    // synchronously by setSize() below) sizes/transforms portableSurface_,
    // but populating its CHILDREN the first time still needs an explicit
    // Refresh()+RefreshFromSurface() call, the same one-shot
    // `MainPane::RefreshOnTick()` the standalone's own constructor makes
    // (Sheaf External/Sheaf/projects/synth/runtime/MainPane.hpp) rather than waiting for this editor's
    // first repaint-hook tick (up to ~33ms away at 30Hz, and never arriving
    // at all if the processor's own timer never started -- see
    // FroggersPluginProcessor's constructor's own MessageManager guard).
    mainComponent_.Refresh();
    portableSurface_.RefreshFromSurface();
    setSize(static_cast<int>(intrinsic.width), static_cast<int>(intrinsic.height));

    // Mirrors synth_runtime::RuntimeShellSession's constructor
    // wiring a repaint hook into the SAME message-thread timer that already
    // drives Runtime<App>'s own per-tick work, inside
    // `RuntimeShellSession::RuntimeShellSession` (Sheaf External/Sheaf/projects/synth/runtime/Shell.hpp) and
    // `Runtime`'s own `timerCallback()` (External/Sheaf/projects/synth/runtime/Runtime.hpp) -- see
    // FroggersPluginProcessor::SetEditorRepaintHook's own comment for the
    // full precedent trace and the single-editor-at-a-time reasoning. Calls
    // Refresh() first (the same order MainPane::RefreshOnTick() uses) so the
    // Controllers/File snapshots and the sidebar's warning badge are current
    // before the renderer rebuilds from them.
    processor_.SetEditorRepaintHook([this] {
        mainComponent_.Refresh();
        portableSurface_.RefreshFromSurface();
    });

    // The action-handler seam -- see this file's header comment, "Refresh
    // cadence" section, for why BOTH handlers are registered and what each
    // one alone would miss. Registered LAST in the constructor (after the
    // control tree already exists) so an action dispatched vanishingly
    // early can never target a not-yet-built tree.
    mainComponent_.SetActionHandler([this](const synth::ui::Action&) { ScheduleDeferredRefresh(); });
    processor_.EditorSurface().SetActionHandler([this](const synth::ui::Action&) { ScheduleDeferredRefresh(); });

    // Operator documentation ships with the plugin (froggers-sheaf-
    // runtime-app spec): a bundled-file corner button, independent of the
    // composed node tree -- see this member's declaration in the header for
    // why. Added LAST so it paints and hit-tests on top of portableSurface_.
    helpButton_.onClick = [this] {
        juce::PopupMenu menu;
        menu.addItem(1, "Manual");
        menu.addItem(2, "Quick Dictionary");
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(helpButton_),
                            [](int result) {
                                if (result == 1) {
                                    frogg3rs_docs::OpenBundledDoc("MANUAL.md");
                                } else if (result == 2) {
                                    frogg3rs_docs::OpenBundledDoc("QUICK_DICT.md");
                                }
                            });
    };
    addAndMakeVisible(helpButton_);
}

FroggersPluginEditor::~FroggersPluginEditor() {
    // MUST all run before mainComponent_/portableSurface_ (members, destroyed
    // immediately after this body returns) tear down: a stray late timer
    // tick or a stray late deferred-refresh callAsync between "this
    // destructor started" and "the hooks are cleared" would otherwise call
    // back into a half-destroyed mainComponent_/portableSurface_. Same
    // ordering contract synth_runtime::RuntimeShellSession::~RuntimeShellSession
    // documents (Sheaf External/Sheaf/projects/synth/runtime/Shell.hpp) for clearing
    // Runtime<App>::SetRepaintHook before its own ShellComponent member is
    // destroyed, applied here to the repaint hook and to both action
    // handlers this class registers. No race to guard against beyond
    // ordering, though: JUCE constructs/destroys AudioProcessorEditors only
    // on the message thread (its own contract), FroggersPluginProcessor::
    // timerCallback() only ever runs there too, and action dispatch (and
    // therefore ScheduleDeferredRefresh()) only ever happens on the message
    // thread as well -- so this destructor and either callback can never
    // actually be concurrent, only mis-ordered, which clearing all three
    // here, first, prevents. (A pending callAsync from
    // ScheduleDeferredRefresh() is separately safe even if this ordering
    // were somehow violated: FlushDeferredRefresh() is invoked through a
    // juce::Component::SafePointer, which JUCE nulls out the moment this
    // Component is deleted -- belt-and-suspenders, not a substitute for
    // clearing the handlers here.)
    processor_.EditorSurface().SetActionHandler({});
    mainComponent_.SetActionHandler({});
    processor_.SetEditorRepaintHook({});
}

void FroggersPluginEditor::ScheduleDeferredRefresh() {
    if (deferredRefreshPending_) {
        return;  // Coalesce: a refresh is already queued and will see the latest state when it runs.
    }
    deferredRefreshPending_ = true;
    juce::Component::SafePointer<FroggersPluginEditor> safeThis(this);
    if (!juce::MessageManager::callAsync([safeThis] {
            if (safeThis != nullptr) {
                safeThis->FlushDeferredRefresh();
            }
        })) {
        // Same guard MainPane::RefreshRendererAfterAction uses
        // (External/Sheaf/projects/synth/runtime/MainPane.hpp): the message queue is shutting down: do
        // not synchronously rebuild controls inside the active JUCE
        // callback, just drop the pending flag so a later action (if any)
        // can try again.
        deferredRefreshPending_ = false;
    }
}

void FroggersPluginEditor::FlushDeferredRefresh() {
    if (!deferredRefreshPending_) {
        return;
    }
    deferredRefreshPending_ = false;
    mainComponent_.Refresh();
    portableSurface_.RefreshFromSurface();
}

void FroggersPluginEditor::paint(juce::Graphics& graphics) { graphics.fillAll(kBackgroundColour); }

void FroggersPluginEditor::resized() {
    // The composed tree's own layout is fixed at its intrinsic extent
    // regardless of this component's actual size (see this file's header
    // comment) -- portableSurface_ always resolves its node tree at exactly
    // that extent; only the VISUAL presentation (via the transform below)
    // adapts to whatever size the host gives this editor.
    const synth::ui::Bounds intrinsic = mainComponent_.IntrinsicBounds();
    portableSurface_.setBounds(0, 0, static_cast<int>(intrinsic.width), static_cast<int>(intrinsic.height));

    const float scaleX = intrinsic.width > 0.0f ? static_cast<float>(getWidth()) / intrinsic.width : 1.0f;
    const float scaleY = intrinsic.height > 0.0f ? static_cast<float>(getHeight()) / intrinsic.height : 1.0f;
    // Fit-inside (never crop), uniform on both axes -- the browser
    // precedent's own technique, see this file's header comment -- and
    // floored well above zero so a host that briefly assigns a degenerate
    // 0x0 bounds during setup still gets a valid, non-inverted transform
    // rather than a NaN/negative-scale one.
    const float scale = std::max(0.01f, std::min(scaleX, scaleY));

    const float offsetX = (static_cast<float>(getWidth()) - intrinsic.width * scale) * 0.5f;
    const float offsetY = (static_cast<float>(getHeight()) - intrinsic.height * scale) * 0.5f;
    const juce::AffineTransform transform = juce::AffineTransform::scale(scale).translated(offsetX, offsetY);
    portableSurface_.setTransform(transform);

    // The ? button keeps its
    // 20-pixel size, sits in the sidebar column directly below the last
    // declared row, centred in the column, mapped through the SAME
    // transform as portableSurface_ above -- so it stays visually attached
    // to the sidebar, covering no sidebar entry and no app control, at
    // every host-chosen size (see this file's header comment).
    constexpr float kHelpButtonSize = 20.0f;
    constexpr float kHelpButtonMargin = 4.0f;
    const float appRootWidth = intrinsic.width - synth::runtime_ui::Layout::kSidebarWidth;
    const float sidebarRowsBottom =
        synth::runtime_ui::Layout::kSidebarButtonHeight *
        static_cast<float>(synth::runtime_ui::DeclaredSidebarRowCount(FroggersPluginServices::PluginSidebarPages()));
    const juce::Rectangle<float> designBounds(
        appRootWidth + (synth::runtime_ui::Layout::kSidebarWidth - kHelpButtonSize) * 0.5f,
        sidebarRowsBottom + kHelpButtonMargin, kHelpButtonSize, kHelpButtonSize);
    helpButton_.setBounds(designBounds.transformedBy(transform).toNearestInt());
}

}  // namespace frogg3rs_vst
