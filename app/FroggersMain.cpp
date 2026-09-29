// Direct-launch entry point for the Frogg3rs build. Modelled on
// External/Sheaf/projects/synth/apps/sheaf-patch/Main.cpp: same
// window/session-owner plumbing, minus the "Select an app" picker. Registers
// and launches only Frogg3rs -- no Braid4Registration.hpp, no
// MiniAppRegistration.hpp, no Launcher.hpp -- so initialise() resolves the
// data root, creates the window, and launches Frogg3rs immediately.
//
// FroggersRegistration.hpp is included directly (the app dir is already on
// the include path via -I, see app/build-launcher.sh); the
// SHEAF_PATCH_EXTRA_APP_* macros are the picker build's mechanism and are
// not defined here.
//
// Data-path correctness: launches via the same
// synth::SheafPatchDataPathsForApp(dataRoot, "frogg3rs") helper the picker
// would have used (Launcher.hpp's ActivateApp), with the same "frogg3rs"
// appId as FroggersRegistration.hpp's FroggersManifest().appId, so existing
// saved patches under ~/Library/Sheaf/synth/sheaf-patch/patches/frogg3rs/
// are not orphaned.

#include "FroggersBundledDocs.hpp"
#include "FroggersRegistration.hpp"
#include "HostDataPaths.hpp"
#include "Shell.hpp"
#include "synth/AppRegistry.hpp"
#include "synth/ThreadId.hpp"

#include <juce_gui_extra/juce_gui_extra.h>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <vector>

namespace synth_frogg3rs_main {

class FroggersMainApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "Frogg3rs"; }
    const juce::String getApplicationVersion() override { return "0.1"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override {
        synth::SetCurrentThreadId(synth::ThreadId::Message);

        try {
            dataRoot_ = synth_runtime::SheafUserApplicationDataRoot();

            window_ = std::make_unique<MainWindow>("Frogg3rs");
#if JUCE_ANDROID
            // Wired before LaunchRegisteredApp below runs: its own first
            // ApplySafeAreaBounds() call (inside MainWindow::ShowContent)
            // fires this same resize callback, and activeSession_ is set
            // before that call for exactly this reason (see
            // LaunchRegisteredApp's own comment).
            window_->onViewportWidthChanged = [this](int width) { DispatchViewportWidth(width); };
#endif

            // Operator documentation ships with the app (froggers-sheaf-
            // runtime-app spec, "Operator documentation ships with the
            // app"): a native main-menu "Help" menu, entirely separate from
            // the app's own rendered surface (MainWindow's content, set
            // below) -- opening the manual/quick dictionary needs no new UI
            // inside FroggersUiSurface's node tree. Android gets neither
            // form: the app looks like the site (operator, 2026-09-29), no
            // menu bar at all, so helpMenu_ is never attached to a window
            // there.
#if JUCE_MAC
            juce::MenuBarModel::setMacMainMenu(&helpMenu_);
#elif !JUCE_ANDROID
            window_->setMenuBar(&helpMenu_);
#endif

            LaunchRegisteredApp<synth_froggers::FroggersApp>(
                synth::SheafPatchDataPathsForApp(dataRoot_, "frogg3rs"));
        } catch (const std::exception& e) {
            INFO("FroggersMainApplication::initialise failed: %s", e.what());
            setApplicationReturnValue(1);
            quit();
        }
    }

    void shutdown() override {
#if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu(nullptr);
#else
        // Guarded where the macOS call is not: setMacMainMenu is static and
        // safe whatever happened during startup, but this one dereferences
        // window_, and initialise() catches its own exceptions -- a throw
        // before the MainWindow is constructed leaves window_ null and still
        // reaches shutdown() on quit.
        if (window_ != nullptr) {
            window_->setMenuBar(nullptr);
        }
#endif
#if JUCE_ANDROID
        // Clears the kiosk-mode component before window_ is destroyed below:
        // Desktop::getKioskModeComponent() would otherwise dangle (same
        // ordering reason as the setMenuBar(nullptr) call above).
        juce::Desktop::getInstance().setKioskModeComponent(nullptr, false);
#endif
        window_.reset();
        activeSession_.reset();
    }

    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String&) override {}

private:
    // "Help" main-menu entries for the two documents this app bundles at
    // build time (see this file's own comment at the setMacMainMenu /
    // setMenuBar call sites, and FroggersBundledDocs.hpp's header comment
    // for how the bundled file is located and opened).
    class HelpMenuModel final : public juce::MenuBarModel {
    public:
        juce::StringArray getMenuBarNames() override { return {"Help"}; }

        juce::PopupMenu getMenuForIndex(int /*topLevelMenuIndex*/, const juce::String& /*menuName*/) override {
            juce::PopupMenu menu;
            menu.addItem(1, "Manual");
            menu.addItem(2, "Quick Dictionary");
            return menu;
        }

        void menuItemSelected(int menuItemId, int /*topLevelMenuIndex*/) override {
            if (menuItemId == 1) {
                frogg3rs_docs::OpenBundledDoc("MANUAL.md");
            } else if (menuItemId == 2) {
                frogg3rs_docs::OpenBundledDoc("QUICK_DICT.md");
            }
        }
    };

    class MainWindow final : public juce::DocumentWindow {
    public:
        explicit MainWindow(juce::String name)
            : DocumentWindow(std::move(name), juce::Colours::black, DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setResizable(true, true);
#if JUCE_ANDROID
            // Target SDK 35 draws edge to edge: fill the display's user area
            // (design.md, "Window, docs, Record"). setFullScreen(true) also
            // creates this window's native peer if it doesn't exist yet
            // (setKioskModeComponent below asserts on a component with no
            // peer -- "Only components that are already on the desktop can
            // be put into kiosk mode!", juce_Desktop.cpp -- so this call must
            // run first).
            setFullScreen(true);

            // Operator, 2026-09-29: like an instrument app, hide the status
            // and navigation bars while Frogg3rs is in front, immersive and
            // sticky (an edge swipe shows them briefly, then they hide
            // again), rather than only insetting content inside them.
            // juce::Desktop's kiosk-mode component is JUCE's own Android
            // mechanism for exactly this: setKioskModeComponent() calls
            // through to AndroidComponentPeer::setFullScreen() again
            // (Desktop::setKioskComponent, juce_Windowing_android.cpp),
            // which this time hides the bars because
            // Desktop::getKioskModeComponent() is now non-null
            // (isKioskModeComponent() / shouldNavBarsBeHidden(), same file);
            // its own ComponentPeerView.setSystemUiVisibilityCompat sets
            // exactly SYSTEM_UI_FLAG_HIDE_NAVIGATION | SYSTEM_UI_FLAG_FULLSCREEN
            // | SYSTEM_UI_FLAG_IMMERSIVE_STICKY when hiding -- Android's own
            // "immersive sticky" mode, whose documented behaviour is that
            // same edge-swipe-reveals-then-hides-again shape.
            // allowMenusAndBars=false is what hides the bars (JUCE's own
            // naming is inverted: true keeps them).
            juce::Desktop::getInstance().setKioskModeComponent(this, false);

            // Pulls in the safe-area insets so the surface never sits under
            // the display CUTOUT specifically -- hiding the status/
            // navigation bars above does not move content out of a cutout
            // (a notch/hole-punch can sit inside the drawable area even with
            // both bars hidden). `config.uiWidth`/`uiHeight` (used
            // elsewhere, see ShowContent below) name a DESKTOP window size
            // and are not applied here.
            ApplySafeAreaBounds();
#endif
            setVisible(true);
        }

        void ShowContent(juce::Component& component, int width, int height) {
            setContentNonOwned(&component, false);
#if JUCE_ANDROID
            juce::ignoreUnused(width, height);
            ApplySafeAreaBounds();
#else
            setSize(width, height);
            centreWithSize(width, height);
#endif
            setVisible(true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

#if JUCE_ANDROID
        // Fires on every resize (rotation, insets changing, multi-window),
        // not just the two explicit ApplySafeAreaBounds() call sites above:
        // whatever the window's own width ends up being, the running app's
        // surface hears about it. `onViewportWidthChanged` is wired by
        // FroggersMainApplication once `activeSession_` exists (see
        // LaunchRegisteredApp below), so it is set before either
        // ApplySafeAreaBounds() call above can fire it.
        void resized() override {
            DocumentWindow::resized();
            if (onViewportWidthChanged) {
                onViewportWidthChanged(getWidth());
            }
        }

        // Both ApplySafeAreaBounds() call sites above (the constructor and
        // ShowContent) run before Android has ever delivered real window
        // insets to the activity's decor view: juce_Windowing_android.cpp's
        // Displays::findDisplays() reads them via
        // `decorView.getRootWindowInsets()`, which returns null until the
        // view is attached and has gone through a layout pass, so
        // `display->safeAreaInsets` is still the zero-inset default at both
        // calls. The real insets arrive later, asynchronously, through the
        // decor view's OnApplyWindowInsetsListener (installed by JUCE's
        // AndroidComponentPeer) and a couple of other startup callbacks
        // (onActivityStarted, onLayoutChange) -- every one of them calls
        // ComponentPeer::forceDisplayUpdate(), which calls
        // Desktop::getInstance().displays->refresh(). Displays::refresh()
        // re-reads the insets and, when they differ from the stale snapshot
        // (verified in juce_Displays.cpp: safeAreaInsets is one of the
        // fields `refresh()` diffs), calls handleScreenSizeChange() on every
        // ComponentPeer, which calls `component.parentSizeChanged()`
        // unconditionally -- before comparing old and new peer bounds, so it
        // fires even though our window's own bounds (already set to the
        // full, un-inset userArea) haven't changed and `resized()` above
        // never runs. ResizableWindow::parentSizeChanged() (DocumentWindow's
        // base) only acts when this window has a parent Component, which a
        // top-level desktop window never does, so overriding it here is safe
        // and is JUCE's actual notification path for "the display's insets
        // just became known/changed" on Android, not a poll or guess.
        void parentSizeChanged() override {
            DocumentWindow::parentSizeChanged();
            ApplySafeAreaBounds();
        }

        std::function<void(int)> onViewportWidthChanged;

    private:
        void ApplySafeAreaBounds() {
            if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
                setBounds(display->safeAreaInsets.subtractedFrom(display->userArea));
            }
        }
#endif
    };

    template <synth::SynthApplication App>
    void LaunchRegisteredApp(synth::RuntimeDataPaths paths) {
        try {
            // Holds the CONCRETE session (not the
            // type-erased synth_runtime::RuntimeSessionOwner, which exposes
            // only Component()) so RegisterFileExportHandler below can
            // reach the running engine via GetRuntime().GetEngine()
            // (Shell.hpp/Runtime.hpp/Engine.hpp) to register the file-export
            // handler that saves a finished Record capture.
            auto session = std::make_unique<synth_runtime::RuntimeShellSession<App>>(std::move(paths));
            const synth::RuntimeConfig config = App::Config();

            window_->setName(juce::String(config.appName));
            // Size the window from the SHELL COMPONENT, not from
            // `config.uiWidth/uiHeight`. The session sizes its component to
            // `MainPane::IntrinsicBounds()` (`External/Sheaf/projects/synth/runtime/Shell.hpp`), which
            // is `config.uiWidth + RuntimePages::Layout::kSidebarWidth` (96)
            // by `config.uiHeight`, computed in `RuntimeMainComponent::BuildTree`
            // (`External/Sheaf/projects/synth/include/synth/RuntimeMainComponent.hpp`) --
            // the app surface PLUS the runtime sidebar that carries Audio /
            // Controllers / Sync / File.
            //
            // Sizing from `config` alone opens the window exactly 96px too
            // narrow and clips that sidebar off the right edge, where it stays
            // invisible until the user drags the window wider (a real
            // reported symptom: "i still had to resize the window to see the buttons
            // on the right hand side"). `External/Sheaf/projects/synth/runtime/Shell.hpp`'s
            // own `RuntimeShellSession::MainWindow` constructor reads the intrinsic
            // bounds the same way and is correct. Read the component's own size and both stay right even
            // if the sidebar width changes upstream.
            juce::Component& content = session->Component();
            // activeSession_ is set BEFORE ShowContent, not after: under
            // JUCE_ANDROID, ShowContent's own ApplySafeAreaBounds() call
            // resizes the window immediately and, through
            // MainWindow::resized(), fires onViewportWidthChanged() before
            // ShowContent returns -- DispatchViewportWidth needs
            // activeSession_ already set to reach the surface for that very
            // first width. Moving `session` does not invalidate `content`:
            // it is a reference to the component the session OWNS, not to
            // the session object itself.
            activeSession_ = std::move(session);
            window_->ShowContent(content, content.getWidth(), content.getHeight());
            // activeSession_'s declared type is concretely
            // RuntimeShellSession<synth_froggers::FroggersApp> (see this
            // method's own comment above), regardless of this method's own
            // App template parameter -- valid because this file only ever
            // instantiates LaunchRegisteredApp with
            // synth_froggers::FroggersApp (see this file's own header
            // comment: "Registers and launches only Frogg3rs").
            RegisterFileExportHandler(activeSession_->GetRuntime().GetEngine());
        } catch (const std::exception& e) {
            INFO("FroggersMainApplication::LaunchRegisteredApp failed: %s", e.what());
        }
    }

#if JUCE_ANDROID
    // The running app's surface, reached the same way
    // RegisterFileExportHandler above reaches the engine
    // (activeSession_->GetRuntime().GetEngine()), one step further to the
    // App instance Engine::Application() returns and its own
    // PortableSurface() (synth::ui::Surface&, Froggers.hpp) -- the same
    // accessor RuntimeMainComponent::DispatchAction and
    // PortableJuceBackend::DispatchBackendAction call to reach it. A resize
    // that fires before activeSession_ exists (there is none: see
    // LaunchRegisteredApp's own comment on ordering) is ignored rather than
    // crashing.
    void DispatchViewportWidth(int width) {
        if (activeSession_ == nullptr) {
            return;
        }
        activeSession_->GetRuntime().GetEngine().Application().PortableSurface().DispatchAction(
            synth::ui::Action::WithValue(synth_froggers::FroggersActions::kViewportWidth, std::to_string(width)));
    }
#endif

    // Registers the JUCE-side behaviour for the engine's file-export seam
    // (Engine::SetFileExportHandler -- see that method's own comment). This
    // is the ONLY place in the app that JUCE dialog/file-write code for
    // Record lives -- FroggersAppCore.hpp names and encodes the finished
    // recording (QueueRecordingExport) without ever seeing JUCE
    // (check-no-juce), and FroggersUiSurface.hpp shows Record's own refusal
    // itself (its transportNotice_) rather than routing it through here.
    void RegisterFileExportHandler(synth::Engine<synth_froggers::FroggersApp>& engine) {
        engine.SetFileExportHandler([](synth::FileExport fileExport) {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Save Recording",
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                    .getChildFile(fileExport.fileName),
                "*.wav");
            const int flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                               juce::FileBrowserComponent::warnAboutOverwriting;
            // `chooser` is captured into its own completion callback (JUCE's
            // documented FileChooser lifetime contract: the object must
            // outlive the dialog), so it self-destructs once this fires.
            // `fileExport` is moved into the inner lambda's own capture,
            // not copied: the async completion fires later (after the user
            // picks a file), well after the engine's own FileExport that
            // produced this call has gone out of scope, and nothing here
            // reads the outer `fileExport` again once the chooser launches.
            chooser->launchAsync(flags, [chooser, fileExport = std::move(fileExport)](const juce::FileChooser& fc) {
                // getResult() (a juce::File) is empty on Android: its save
                // screen (JUCE_ANDROID's CREATE_DOCUMENT chooser,
                // juce_FileChooser_android.cpp) returns a content URL, and
                // FileChooser::getResults() keeps only local files -- so
                // getURLResult() is read instead everywhere, and every
                // platform's own chooser result (a plain file:// URL off
                // macOS/Windows/Linux) still satisfies the scheme=="file"
                // check below.
                const juce::URL url = fc.getURLResult();
                if (url.isEmpty()) {
                    return;  // Cancelled.
                }

                // Branch on the URL's SCHEME, not juce::URL::isLocalFile():
                // on Android, isLocalFile() also returns true for a
                // content:// URI when juce_Files_android.cpp's
                // AndroidContentUriResolver::getLocalFileFromContentUri can
                // GUESS a raw filesystem path from it -- for
                // com.android.externalstorage.documents (the Documents save
                // screen's own provider) that guess is
                // "/storage/emulated/0/<subpath>", a path this app has no
                // write access to under scoped storage (target SDK 29+):
                // file.deleteFile() and FileOutputStream's open both fail
                // silently against it, leaving the save screen's own 0-byte
                // placeholder document untouched and producing exactly
                // "Failed to write recording." -- traced with a temporary
                // diagnostic build that logged url.isLocalFile()==true and
                // url.getScheme()=="content" side by side for this same
                // save. A local save dialog's result is always a file://
                // URL (this file's own header comment on getURLResult, and
                // juce::URL's File constructor, juce_URL.cpp), so
                // scheme=="file" is exactly the platforms this branch
                // already ran on, and content:// (Android's save screen)
                // now always takes the createOutputStream() branch instead
                // of JUCE's local-path guess.
                std::unique_ptr<juce::OutputStream> stream;
                bool ok = false;
                if (url.getScheme() == "file") {
                    const juce::File file = url.getLocalFile();
                    file.deleteFile();
                    auto fileStream = std::make_unique<juce::FileOutputStream>(file);
                    ok = fileStream->openedOk();
                    stream = std::move(fileStream);
                } else {
                    stream = url.createOutputStream();
                    ok = stream != nullptr;
                }

                if (ok) {
                    ok = stream->write(fileExport.bytes.data(), fileExport.bytes.size());
                    stream->flush();
#if JUCE_ANDROID
                    INFO("F2diag: write ok=%d bytes=%zu", (int)ok, fileExport.bytes.size());
#endif
                }

                juce::String message =
                    ok ? juce::String("Recording saved.") : juce::String("Failed to write recording.");
                if (ok && !fileExport.note.empty()) {
                    message += " (" + juce::String(fileExport.note) + ")";
                }
                juce::AlertWindow::showMessageBoxAsync(
                    ok ? juce::AlertWindow::InfoIcon : juce::AlertWindow::WarningIcon, "Recording", message);
            });
        });
    }

    std::filesystem::path dataRoot_;
    // Declared BEFORE window_ so it is destroyed AFTER it (member
    // destruction is declaration-reverse): the Help menu -- set via
    // setMacMainMenu(&helpMenu_) on macOS or window_->setMenuBar(&helpMenu_)
    // elsewhere -- is cleared in shutdown() before window_.reset() runs, but
    // this ordering is a second belt-and-suspenders guard against the main
    // menu ever outliving the model object it points to, on either
    // platform.
    HelpMenuModel helpMenu_;
    std::unique_ptr<MainWindow> window_;
    std::unique_ptr<synth_runtime::RuntimeShellSession<synth_froggers::FroggersApp>> activeSession_;
};

}  // namespace synth_frogg3rs_main

START_JUCE_APPLICATION(synth_frogg3rs_main::FroggersMainApplication)
