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

            // Operator documentation ships with the app (froggers-sheaf-
            // runtime-app spec, "Operator documentation ships with the
            // app"): a native macOS main-menu "Help" menu, entirely
            // separate from the app's own rendered surface (MainWindow's
            // content, set below) -- opening the manual/quick dictionary
            // needs no new UI inside FroggersUiSurface's node tree.
#if JUCE_MAC
            juce::MenuBarModel::setMacMainMenu(&helpMenu_);
#else
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
            setVisible(true);
        }

        void ShowContent(juce::Component& component, int width, int height) {
            setContentNonOwned(&component, false);
            setSize(width, height);
            centreWithSize(width, height);
            setVisible(true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
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
            // `MainPane::IntrinsicBounds()` (`External/Sheaf/projects/synth/runtime/Shell.hpp:86-87`), which
            // is `config.uiWidth + RuntimePages::Layout::kSidebarWidth` (96)
            // by `config.uiHeight` (`External/Sheaf/projects/synth/include/synth/RuntimeMainComponent.hpp:212-218`) --
            // the app surface PLUS the runtime sidebar that carries Audio /
            // Controllers / Sync / File.
            //
            // Sizing from `config` alone opens the window exactly 96px too
            // narrow and clips that sidebar off the right edge, where it stays
            // invisible until the user drags the window wider (a real
            // reported symptom: "i still had to resize the window to see the buttons
            // on the right hand side"). Sheaf ships two window paths and this
            // file was modelled on the wrong one: `apps/sheaf-patch/Main.cpp`
            // :87-99 sizes from `config` and has the same defect, while
            // `External/Sheaf/projects/synth/runtime/Shell.hpp:193-197` reads the intrinsic bounds and is
            // correct. Read the component's own size and both stay right even
            // if the sidebar width changes upstream.
            juce::Component& content = session->Component();
            window_->ShowContent(content, content.getWidth(), content.getHeight());
            activeSession_ = std::move(session);
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
            // `fileExport` is captured by value: the async completion fires
            // later (after the user picks a file), well after the engine's
            // own FileExport that produced this call has gone out of scope.
            chooser->launchAsync(flags, [chooser, fileExport](const juce::FileChooser& fc) {
                const juce::File file = fc.getResult();
                if (file == juce::File{}) {
                    return;  // Cancelled.
                }

                // Streamed to disk via
                // juce::FileOutputStream (not std::ofstream): the target
                // came back as a juce::File from the FileChooser, and this
                // stays in the same JUCE idiom as everything else in this
                // file rather than round-tripping through a raw path.
                file.deleteFile();
                juce::FileOutputStream stream(file);
                bool ok = stream.openedOk();
                if (ok) {
                    ok = stream.write(fileExport.bytes.data(), fileExport.bytes.size());
                    stream.flush();
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
