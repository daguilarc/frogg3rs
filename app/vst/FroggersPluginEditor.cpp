#include "FroggersPluginEditor.hpp"

#include "FroggersBundledDocs.hpp"
#include "FroggersPluginProcessor.hpp"
#include "FroggersUiSurface.hpp"

#include <algorithm>

namespace frogg3rs_vst {

namespace {

// Reuses FroggersPageLayout's own named constants
// (app/FroggersUiSurface.hpp) -- that struct's own comment names itself the
// single definition site for "the operator's approved design box" -- rather
// than a second 900.0f/712.0f literal pair here (the exact defect that
// struct's own predecessor, `RequiredHeight()`, was replaced for).
constexpr float kDesignWidth = synth_froggers::FroggersPageLayout::kDefaultWidth;
constexpr float kDesignHeight = synth_froggers::FroggersPageLayout::kDefaultHeight;

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
// margins a non-900:712 host window leaves around the uniformly-scaled
// surface (see resized() below) read as intentional background rather than
// a mismatched gap.
const juce::Colour kBackgroundColour(synth::kSurfaceBackground.r, synth::kSurfaceBackground.g,
                                      synth::kSurfaceBackground.b);

}  // namespace

FroggersPluginEditor::FroggersPluginEditor(FroggersPluginProcessor& processor)
    : juce::AudioProcessorEditor(processor), processor_(processor), portableSurface_(processor_.EditorSurface()) {
    addAndMakeVisible(portableSurface_);
    setResizable(true, true);
    setResizeLimits(static_cast<int>(kDesignWidth * kMinScale), static_cast<int>(kDesignHeight * kMinScale),
                     static_cast<int>(kDesignWidth * kMaxScale), static_cast<int>(kDesignHeight * kMaxScale));

    // Build the control tree once, immediately -- resized() (fired
    // synchronously by setSize() below) sizes/transforms portableSurface_,
    // but populating its CHILDREN the first time still needs an explicit
    // RefreshFromSurface() call, the same one-shot MainPane's own
    // constructor makes (Sheaf External/Sheaf/projects/synth/runtime/MainPane.hpp:47) rather than waiting
    // for this editor's first repaint-hook tick (up to ~33ms away at 30Hz,
    // and never arriving at all if the processor's own timer never started
    // -- see FroggersPluginProcessor's constructor's own MessageManager
    // guard).
    portableSurface_.RefreshFromSurface();
    setSize(static_cast<int>(kDesignWidth), static_cast<int>(kDesignHeight));

    // Mirrors synth_runtime::RuntimeShellSession's constructor
    // wiring a repaint hook into the SAME message-thread timer that already
    // drives Runtime<App>'s own per-tick work (Sheaf External/Sheaf/projects/synth/runtime/Shell.hpp:88,
    // External/Sheaf/projects/synth/runtime/Runtime.hpp:974-981) -- see
    // FroggersPluginProcessor::SetEditorRepaintHook's own comment for the
    // full precedent trace and the single-editor-at-a-time reasoning.
    processor_.SetEditorRepaintHook([this] { portableSurface_.RefreshFromSurface(); });

    // The action-handler seam -- see this file's
    // header comment, "Refresh cadence" section, for why this was missing
    // and what it fixes. Registered LAST in the constructor (after the
    // control tree already exists) so an action dispatched vanishingly
    // early can never target a not-yet-built tree.
    processor_.EditorSurface().SetActionHandler(
        [this](const synth::ui::Action&) { ScheduleDeferredRefresh(); });

    // Operator documentation ships with the plugin (froggers-sheaf-
    // runtime-app spec): a bundled-file corner button, independent of
    // FroggersUiSurface's own node tree -- see this member's declaration
    // in the header for why. Added LAST so it paints and hit-tests on top
    // of portableSurface_.
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
    // MUST both run before portableSurface_ (a member, destroyed
    // immediately after this body returns) tears down: a stray late timer
    // tick or a stray late deferred-refresh callAsync between "this
    // destructor started" and "the hooks are cleared" would otherwise call
    // back into a half-destroyed portableSurface_. Same ordering contract
    // synth_runtime::RuntimeShellSession::~RuntimeShellSession documents
    // (Sheaf External/Sheaf/projects/synth/runtime/Shell.hpp:91-95) for clearing
    // Runtime<App>::SetRepaintHook before its own ShellComponent member is
    // destroyed, applied here to both hooks this class registers. No race
    // to guard against beyond ordering, though: JUCE constructs/destroys
    // AudioProcessorEditors only on the message thread (its own contract),
    // FroggersPluginProcessor::timerCallback() only ever runs there too,
    // and action dispatch (and therefore ScheduleDeferredRefresh()) only
    // ever happens on the message thread as well -- so this destructor and
    // either callback can never actually be concurrent, only mis-ordered,
    // which clearing both here, first, prevents. (A pending callAsync from
    // ScheduleDeferredRefresh() is separately safe even if this ordering
    // were somehow violated: FlushDeferredRefresh() is invoked through a
    // juce::Component::SafePointer, which JUCE nulls out the moment this
    // Component is deleted -- belt-and-suspenders, not a substitute for
    // clearing the handler here.)
    processor_.EditorSurface().SetActionHandler({});
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
        // (External/Sheaf/projects/synth/runtime/MainPane.hpp:148-157): the message queue is shutting down: do
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
    portableSurface_.RefreshFromSurface();
}

void FroggersPluginEditor::paint(juce::Graphics& graphics) { graphics.fillAll(kBackgroundColour); }

void FroggersPluginEditor::resized() {
    // FroggersUiSurface's own layout is fixed at the design extent
    // regardless of this component's actual size (see this file's header
    // comment) -- portableSurface_ always resolves its node tree at exactly
    // kDesignWidth x kDesignHeight; only the VISUAL presentation (via the
    // transform below) adapts to whatever size the host gives this editor.
    portableSurface_.setBounds(0, 0, static_cast<int>(kDesignWidth), static_cast<int>(kDesignHeight));

    const float scaleX = kDesignWidth > 0.0f ? static_cast<float>(getWidth()) / kDesignWidth : 1.0f;
    const float scaleY = kDesignHeight > 0.0f ? static_cast<float>(getHeight()) / kDesignHeight : 1.0f;
    // Fit-inside (never crop), uniform on both axes -- the browser
    // precedent's own technique, see this file's header comment -- and
    // floored well above zero so a host that briefly assigns a degenerate
    // 0x0 bounds during setup still gets a valid, non-inverted transform
    // rather than a NaN/negative-scale one.
    const float scale = std::max(0.01f, std::min(scaleX, scaleY));

    const float offsetX = (static_cast<float>(getWidth()) - kDesignWidth * scale) * 0.5f;
    const float offsetY = (static_cast<float>(getHeight()) - kDesignHeight * scale) * 0.5f;
    portableSurface_.setTransform(juce::AffineTransform::scale(scale).translated(offsetX, offsetY));

    // Fixed corner overlay, in real (untransformed) editor pixels -- unlike
    // portableSurface_, this button is not part of the design-space surface
    // and does not scale with it.
    constexpr int kHelpButtonSize = 20;
    constexpr int kHelpButtonMargin = 4;
    helpButton_.setBounds(getWidth() - kHelpButtonSize - kHelpButtonMargin, kHelpButtonMargin, kHelpButtonSize,
                           kHelpButtonSize);
}

}  // namespace frogg3rs_vst
