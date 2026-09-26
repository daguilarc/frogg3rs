#pragma once

// frogg3rs_vst::FroggersPluginServices -- satisfies
// synth::runtime_ui::RuntimeMainServices (RuntimeMainComponent.hpp) for the
// plugin editor, the same role JuceRuntimeMainServices plays for the
// standalone and BrowserRuntimeMainServices plays for the browser (both
// read start-to-finish before writing this file). Controllers and File are
// the only pages the plugin's sidebar declares (FroggersPluginEditor.hpp,
// RuntimeSidebarPages), so this class wires those two for real and leaves
// Audio, Sync and the deadline readout as no-ops: the RuntimeMainServices
// concept still requires those methods regardless of which pages a host
// declares, so a no-op implementation is what "not shown" means here, not
// an exemption from the concept.
//
// Controllers: built on synth::runtime_ui::ControllersPageBinding<EngineType>
// (Sheaf ControllersPageBinding.hpp), the same shared construct the
// standalone and browser hosts use, over the processor's own engine and
// MidiConnectionManager (FroggersPluginProcessor::GetEngine()/
// MidiConnections()) -- a controller row opens its own MIDI ports in the
// plugin exactly as in the standalone, so the binding's
// device-list/discovery-cache plumbing is unchanged from either host. A
// commit (Add/Edit/Remove/Delete a row) tells the host its state changed
// and answers true unconditionally: the plugin keeps no runtime
// configuration to fail saving, so the page must never show a save failure
// for a write that was never attempted.
//
// File: built on synth::runtime_ui::MakeEngineFileCallbacks (Sheaf
// RuntimeFileService.hpp) over the same engine, exactly as both other
// hosts bind it -- New/Save/Save As/Load reach the standalone's own patches
// folder and never the plugin's own (nonexistent) runtime configuration.
// onCommand notifies the host after New and Load answer Ok, and after Save
// As or its overwrite answers Pending; a plain Save does not notify.

#include "FroggersPluginProcessor.hpp"

#include "synth/ControllersPageBinding.hpp"
#include "synth/ControllersPageUI.hpp"
#include "synth/RuntimeFileService.hpp"
#include "synth/RuntimePages.hpp"

#include <functional>
#include <string_view>

namespace frogg3rs_vst {

class FroggersPluginServices final {
public:
    explicit FroggersPluginServices(FroggersPluginProcessor& processor)
        : processor_(processor),
          controllersBinding_(processor_.GetEngine()),
          fileService_(synth::runtime_ui::MakeEngineFileCallbacks(
              processor_.GetEngine(),
              [this](const char* action, const synth::PatchCommandResult& result) {
                  NotifyAfterFileCommand(action, result);
              })) {
        // Mirrors JuceRuntimeMainServices::SetMidiProcessorsRebuiltHook's own
        // wiring over Runtime<App>: every MIDI-processor rebuild, not just
        // this page's own edits, leaves the Controllers page's device
        // discovery and instrument snapshot stale until the next Refresh().
        processor_.SetMidiProcessorsRebuiltHook([this] { controllersBinding_.MarkInstrumentRebuilt(); });
    }

    ~FroggersPluginServices() { processor_.SetMidiProcessorsRebuiltHook({}); }

    FroggersPluginServices(const FroggersPluginServices&) = delete;
    FroggersPluginServices& operator=(const FroggersPluginServices&) = delete;

    synth::runtime_ui::ControllersPageCallbacks MakeControllersCallbacks(std::function<void()> onBack) {
        return controllersBinding_.MakeCallbacks(
            std::move(onBack), [this] { return processor_.MidiConnections().State(); },
            [this] {
                processor_.NotifyHostOfNonParameterChange();
                return true;
            });
    }

    void RefreshAudio(synth::runtime_ui::AudioPageSnapshot&) {}
    void DispatchAudio(const synth::ui::Action&) {}

    void RefreshFile(synth::runtime_ui::FilePageSnapshot& snapshot) { fileService_.Refresh(snapshot); }
    void DispatchFile(const synth::ui::Action& action) { fileService_.Dispatch(action); }

    void RefreshControllers(synth::runtime_ui::ControllersPageSurface& surface) {
        synth_runtime::FeedControllersDeviceList(controllersBinding_, processor_.MidiConnections());
        controllersBinding_.Refresh(surface);
    }

    synth::SyncConfig SnapshotSyncConfiguration() { return {}; }
    void RefreshSyncStatus(synth::runtime_ui::SyncPageStatus&) {}
    bool CommitSyncConfiguration(const synth::SyncConfig&) { return false; }

    float DeadlineSamplePercent() const { return 0.0f; }

    void SaveRuntimeConfiguration() {}

private:
    void NotifyAfterFileCommand(const char* action, const synth::PatchCommandResult& result) {
        const std::string_view name(action);
        using synth::PatchCommandStatus;
        const bool newOrLoadOk =
            (name == "NewPatch" || name == "LoadPatch") && result.status == PatchCommandStatus::Ok;
        const bool saveAsPending = (name == "SavePatchAs" || name == "SavePatchAsOverwrite") &&
                                    result.status == PatchCommandStatus::Pending;
        if (newOrLoadOk || saveAsPending) {
            processor_.NotifyHostOfNonParameterChange();
        }
    }

    FroggersPluginProcessor& processor_;
    synth::runtime_ui::ControllersPageBinding<synth::Engine<synth_froggers::FroggersApp>> controllersBinding_;
    synth::runtime_ui::RuntimeFileService fileService_;
};

}  // namespace frogg3rs_vst
