#pragma once

// synth_froggers::FroggersMidiCatalog -- the app's MIDI catalog: the
// actions a controller can dispatch (transport, randomize/reset, bank and
// scene selection, BPM), the library kinds the Controllers page keeps
// around for this app (parameter inc/dec, absolute set, push, scene blend,
// hold drill, shift), and the six device defaults offered from the
// Controllers page's Layout dropdown -- MIDI Fighter Twister, Akai APC40
// mkII (Generic), Akai APC40 mkII (Ableton), Launchpad X, Launchpad Pro
// MK3, and Launchpad Mini MK3. Choosing one of the six installs its
// mappings onto the selected slot; Custom leaves the slot's mappings
// untouched and editable by hand.
//
// Twister: the manual's Utility settings must match this default --
// every encoder set to relative (Enc 3FH/41H, not the factory absolute
// setting), all six side buttons set to CC Hold (127 on press, 0 on
// release, not the factory bank-switch behaviour on the middle pair), and
// Bank Side Buttons unchecked so the side buttons keep this default's CC
// addresses whatever Twister bank is lit. The six side buttons are five
// paired jobs -- a press and a shifted press held under Shift -- plus
// Shift itself: left column top to bottom Bank Next/Bank Previous,
// Play/Stop, Freeze/Reset Page; right column Scene 1/Scene 2, Randomize
// Page/Randomize All; Shift sits at the bottom right. CC Hold is required
// on every side button because the release is what ends Shift.
//
// APC40 mkII (Generic): the unit's eight device knobs follow whichever
// Track Select button is lit (track 1 = channel 0), so Track 1 must stay
// selected -- pressing another track moves those eight knobs to another
// channel and they stop responding to this default until Track 1 is
// pressed again. The Ableton default exists to avoid that caveat: it
// opens with a connect-time message that keeps the unit's sixteen knobs
// on channel 0 regardless of which track is selected. The cost is that
// every button LED on the unit is host-controlled in that mode, and this
// default sends none, so its buttons stay dark; Generic mode lights its
// own buttons and rings itself.

#include "FroggersParameters.hpp"
#include "FroggersUiSurface.hpp"

#include "synth/MidiAppCatalog.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace synth_froggers {

namespace {

// A momentary control that fires one app action on press and nothing on
// release. The dispatched action's catalog index is resolved by the
// engine at runtime, not stored here. When shiftedAction is non-empty, the
// button fires that second action instead while a Shift button on the
// same controller is held.
inline synth::MidiControllerSystemMessageAssociation AppActionButton(synth::MidiControlAddress address,
                                                                       std::string action, std::string value,
                                                                       std::string shiftedAction = {},
                                                                       std::string shiftedValue = {}) {
    synth::MidiControllerSystemMessageAssociation association;
    association.control = address;
    association.press = synth::MessageIn::AppAction(0, 0, 0.0f);
    association.release = std::nullopt;
    association.outputFeedback = false;
    association.appAction = std::move(action);
    association.appActionValue = std::move(value);
    if (!shiftedAction.empty()) {
        association.shiftedPress = synth::MessageIn::AppAction(0, 0, 0.0f);
        association.shiftedAppAction = std::move(shiftedAction);
        association.shiftedAppActionValue = std::move(shiftedValue);
    }
    return association;
}

// A momentary control that holds a state while pressed and releases it on
// lift: Hold Drill on the APC40, Shift on the Twister. `kind` is one of
// those two; each call site passes it as a literal.
inline synth::MidiControllerSystemMessageAssociation HeldButton(synth::MidiControlAddress address,
                                                                  synth::UISystemMessage kind) {
    const bool drill = kind == synth::UISystemMessage::HoldDrill;
    synth::MidiControllerSystemMessageAssociation association;
    association.control = address;
    association.press = drill ? synth::MessageIn::HoldDrill(0, true) : synth::MessageIn::Shift(0, true);
    association.release = drill ? synth::MessageIn::HoldDrill(0, false) : synth::MessageIn::Shift(0, false);
    association.outputFeedback = false;
    return association;
}

inline synth::MidiAppDeviceDefault TwisterDeviceDefault() {
    synth::MidiAppDeviceDefault device;
    device.id = "froggers.twister";
    device.displayName = "MIDI Fighter Twister";
    device.kind = synth::MidiProfileKind::MfTwister;
    // The library's own descriptor alias (ControllerWizard.cpp's
    // file-local kMfTwisterAlias, not exported from a header).
    device.inputAliases = {"Midi Fighter Twister"};
    device.outputAliases = {"Midi Fighter Twister"};

    synth::MidiControllerProfileConfig config;
    config.encoderInput = synth::EncoderMidiInConfig::TwisterDefault(0);
    config.encoderOutput = synth::EncoderMidiOutConfig::TwisterDefault(0);
    config.systemMessages = {
        AppActionButton(synth::MidiControlAddress{.channel = 3, .cc = 8}, FroggersActions::kBankNext, "",
                         FroggersActions::kBankPrevious, ""),
        AppActionButton(synth::MidiControlAddress{.channel = 3, .cc = 9}, FroggersActions::kPlay, "",
                         FroggersActions::kStop, ""),
        AppActionButton(synth::MidiControlAddress{.channel = 3, .cc = 10}, FroggersActions::kFreeze, "",
                         FroggersActions::kResetPage, ""),
        AppActionButton(synth::MidiControlAddress{.channel = 3, .cc = 11}, FroggersActions::kSceneSelect, "0",
                         FroggersActions::kSceneSelect, "1"),
        AppActionButton(synth::MidiControlAddress{.channel = 3, .cc = 12}, FroggersActions::kRandomizePage, "",
                         FroggersActions::kRandomizeAll, ""),
        HeldButton(synth::MidiControlAddress{.channel = 3, .cc = 13}, synth::UISystemMessage::Shift),
    };
    device.config = std::move(config);
    return device;
}

// The two APC40 mkII defaults share this control table; the Ableton
// default is built from the Generic one, adding only the connect-time
// SysEx message, so the two tables stay one definition.
inline synth::MidiControllerProfileConfig Apc40BaseConfig() {
    std::vector<synth::EncoderMidiMapping> turns;
    for (std::size_t ix = 0; ix < 8; ++ix) {
        turns.push_back({.control = {.channel = 0, .cc = static_cast<std::uint8_t>(48 + ix)},
                         .slotIx = 0,
                         .position = ix});
    }
    for (std::size_t ix = 0; ix < 8; ++ix) {
        turns.push_back({.control = {.channel = 0, .cc = static_cast<std::uint8_t>(16 + ix)},
                         .slotIx = 0,
                         .position = 8 + ix});
    }

    auto note = [](std::uint8_t number) {
        return synth::MidiControlAddress{.channel = 0, .cc = number, .type = synth::MidiControlType::Note};
    };
    std::vector<synth::MidiControllerSystemMessageAssociation> messages;
    messages.push_back(HeldButton(note(98), synth::UISystemMessage::HoldDrill));
    messages.push_back(AppActionButton(note(91), FroggersActions::kPlay, ""));
    messages.push_back(AppActionButton(note(92), FroggersActions::kStop, ""));
    messages.push_back(AppActionButton(note(93), FroggersActions::kRecord, ""));
    messages.push_back(AppActionButton(note(82), FroggersActions::kSceneSelect, "0"));
    messages.push_back(AppActionButton(note(83), FroggersActions::kSceneSelect, "1"));
    messages.push_back(AppActionButton(note(97), FroggersActions::kBankPrevious, ""));
    messages.push_back(AppActionButton(note(96), FroggersActions::kBankNext, ""));
    messages.push_back(AppActionButton(note(62), FroggersActions::kRandomizePage, ""));
    messages.push_back(AppActionButton(note(63), FroggersActions::kRandomizeAll, ""));
    messages.push_back(AppActionButton(note(64), FroggersActions::kResetPage, ""));
    messages.push_back(AppActionButton(note(65), FroggersActions::kResetAll, ""));
    messages.push_back(AppActionButton(note(81), FroggersActions::kFreeze, ""));
    for (std::size_t ix = 0; ix < kFroggersBankCount; ++ix) {
        messages.push_back(AppActionButton(
            synth::MidiControlAddress{.channel = static_cast<std::uint8_t>(ix),
                                      .cc = 52,
                                      .type = synth::MidiControlType::Note},
            FroggersActions::kBankSelect, std::to_string(ix)));
    }

    synth::MidiControllerProfileConfig config;
    config.encoderInput = synth::EncoderMidiInConfig{.mode = synth::EncoderMode::Absolute, .turns = std::move(turns)};
    config.systemMessages = std::move(messages);
    config.analogInput = synth::AnalogMidiInConfig{
        .sceneBlend = synth::MidiControlAddress{.channel = 0, .cc = 15},
        .appActions = {{synth::MidiControlAddress{.channel = 0, .cc = 14}, FroggersActions::kBpm, ""}},
    };
    return config;
}

inline synth::MidiAppDeviceDefault Apc40GenericDeviceDefault() {
    synth::MidiAppDeviceDefault device;
    device.id = "froggers.apc40.generic";
    device.displayName = "Akai APC40 mkII (Generic)";
    device.kind = synth::MidiProfileKind::Generic;
    device.inputAliases = {"APC40 mkII"};
    device.outputAliases = {"APC40 mkII"};
    device.config = Apc40BaseConfig();
    return device;
}

inline synth::MidiAppDeviceDefault Apc40AbletonDeviceDefault() {
    synth::MidiAppDeviceDefault device = Apc40GenericDeviceDefault();
    device.id = "froggers.apc40.ableton";
    device.displayName = "Akai APC40 mkII (Ableton)";
    device.config.openSysEx = {{0xF0, 0x47, 0x7F, 0x29, 0x60, 0x00, 0x04, 0x41, 0x09, 0x07, 0x01, 0xF7}};
    return device;
}

// A momentary control that fires one app action on press and nothing on
// release, addressed by Launchpad grid position rather than a MIDI control
// address -- the pad-addressed sibling of AppActionButton above.
inline synth::MidiControllerSystemMessageAssociation LaunchpadPadButton(synth::LaunchpadController controller,
                                                                         int x, int y, std::string action,
                                                                         std::string value) {
    synth::MidiControllerSystemMessageAssociation association;
    association.launchpadPosition = synth::LaunchpadGridPosition{.controller = controller, .x = x, .y = y};
    association.press = synth::MessageIn::AppAction(0, 0, 0.0f);
    association.release = std::nullopt;
    association.outputFeedback = false;
    association.appAction = std::move(action);
    association.appActionValue = std::move(value);
    return association;
}

// The transport/randomize/reset/scene row above the pad grid, plus the bank
// column beside it -- identical across all three Launchpad models, so this
// one table is shared by every LaunchpadDeviceDefault() call below. The 8x8
// pad grid itself (x 0..7, y 0..7) is left unmapped.
inline std::vector<synth::MidiControllerSystemMessageAssociation> LaunchpadPadMap(
    synth::LaunchpadController controller) {
    std::vector<synth::MidiControllerSystemMessageAssociation> associations = {
        LaunchpadPadButton(controller, 0, -1, FroggersActions::kPlay, ""),
        LaunchpadPadButton(controller, 1, -1, FroggersActions::kStop, ""),
        LaunchpadPadButton(controller, 2, -1, FroggersActions::kFreeze, ""),
        LaunchpadPadButton(controller, 3, -1, FroggersActions::kRecord, ""),
        LaunchpadPadButton(controller, 4, -1, FroggersActions::kSceneSelect, "0"),
        LaunchpadPadButton(controller, 5, -1, FroggersActions::kSceneSelect, "1"),
        LaunchpadPadButton(controller, 6, -1, FroggersActions::kRandomizePage, ""),
        LaunchpadPadButton(controller, 7, -1, FroggersActions::kResetPage, ""),
    };
    for (std::size_t ix = 0; ix < kFroggersBankCount; ++ix) {
        associations.push_back(LaunchpadPadButton(controller, 8, static_cast<int>(ix), FroggersActions::kBankSelect,
                                                    std::to_string(ix)));
    }
    return associations;
}

// The three Launchpad presets below differ only in the model, the
// descriptor id, the display name, the port aliases, and the
// programmer-mode SysEx -- everything else (kind, pad map) is shared, so
// one helper builds every descriptor. The aliases are per direction: a
// Launchpad's two endpoints carry the direction word in their names, and it
// is the opposite word on each side, so the application's input is the
// unit's MIDI Out.
inline synth::MidiAppDeviceDefault LaunchpadDeviceDefault(synth::LaunchpadController controller, std::string id,
                                                           std::string displayName,
                                                           std::vector<std::string> inputAliases,
                                                           std::vector<std::string> outputAliases,
                                                           std::vector<std::uint8_t> programmerModeSysEx) {
    synth::MidiAppDeviceDefault device;
    device.id = std::move(id);
    device.displayName = std::move(displayName);
    device.kind = synth::MidiProfileKind::Launchpad;
    device.inputAliases = std::move(inputAliases);
    device.outputAliases = std::move(outputAliases);

    synth::MidiControllerProfileConfig config;
    config.systemMessages = LaunchpadPadMap(controller);
    config.openSysEx = {std::move(programmerModeSysEx)};
    device.config = std::move(config);
    return device;
}

// The MIDI interface, not the DAW interface: the manuals state Programmer
// Mode uses the MIDI interface, and no alias here names a DAW port. A JUCE
// host reports each port as the device name, a space, and the port name, so
// the name to match is "<product> <interface name> In" or "... Out" -- the
// application's input being the unit's MIDI Out. The Mini MK3's two names
// were read from a connected unit; the other two models' are the same
// construction with each model's own interface name and are unconfirmed on
// hardware, which is why each list keeps the shorter forms behind the full
// one for a host that reports one of those instead.
inline synth::MidiAppDeviceDefault LaunchpadXDeviceDefault() {
    return LaunchpadDeviceDefault(synth::LaunchpadController::LaunchpadX, "froggers.launchpad.x", "Launchpad X",
                                   {"Launchpad X LPX MIDI Out", "LPX MIDI", "Launchpad X LPX MIDI",
                                    "Launchpad X"},
                                   {"Launchpad X LPX MIDI In", "LPX MIDI", "Launchpad X LPX MIDI",
                                    "Launchpad X"},
                                   {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0C, 0x0E, 0x01, 0xF7});
}

inline synth::MidiAppDeviceDefault LaunchpadProMk3DeviceDefault() {
    return LaunchpadDeviceDefault(
        synth::LaunchpadController::LaunchpadProMk3, "froggers.launchpad.promk3", "Launchpad Pro MK3",
        {"Launchpad Pro MK3 LPProMK3 MIDI Out", "LPProMK3 MIDI", "Launchpad Pro MK3 LPProMK3 MIDI",
         "Launchpad Pro MK3"},
        {"Launchpad Pro MK3 LPProMK3 MIDI In", "LPProMK3 MIDI", "Launchpad Pro MK3 LPProMK3 MIDI",
         "Launchpad Pro MK3"},
        {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0E, 0x00, 0x11, 0x00, 0x00, 0xF7});
}

inline synth::MidiAppDeviceDefault LaunchpadMiniMk3DeviceDefault() {
    return LaunchpadDeviceDefault(
        synth::LaunchpadController::LaunchpadMiniMk3, "froggers.launchpad.minimk3", "Launchpad Mini MK3",
        {"Launchpad Mini MK3 LPMiniMK3 MIDI Out", "LPMiniMK3 MIDI",
         "Launchpad Mini MK3 LPMiniMK3 MIDI", "Launchpad Mini MK3"},
        {"Launchpad Mini MK3 LPMiniMK3 MIDI In", "LPMiniMK3 MIDI",
         "Launchpad Mini MK3 LPMiniMK3 MIDI", "Launchpad Mini MK3"},
        {0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0x01, 0xF7});
}

}  // namespace

inline synth::MidiAppCatalog FroggersMidiCatalog() {
    synth::MidiAppCatalog catalog;
    catalog.actions = {
        {FroggersActions::kPlay, "", "Play", std::nullopt},
        {FroggersActions::kStop, "", "Stop", std::nullopt},
        {FroggersActions::kFreeze, "", "Freeze", std::nullopt},
        {FroggersActions::kRecord, "", "Record", std::nullopt},
        {FroggersActions::kRandomizeAll, "", "Randomize All", std::nullopt},
        {FroggersActions::kRandomizePage, "", "Randomize Page", std::nullopt},
        {FroggersActions::kResetAll, "", "Reset All", std::nullopt},
        {FroggersActions::kResetPage, "", "Reset Page", std::nullopt},
        {FroggersActions::kBankPrevious, "", "Bank Previous", std::nullopt},
        {FroggersActions::kBankNext, "", "Bank Next", std::nullopt},
    };
    for (std::size_t ix = 0; ix < kFroggersBankCount; ++ix) {
        catalog.actions.push_back(
            {FroggersActions::kBankSelect, std::to_string(ix), "Bank " + std::to_string(ix + 1), std::nullopt});
    }
    catalog.actions.push_back({FroggersActions::kSceneSelect, "0", "Scene 1", std::nullopt});
    catalog.actions.push_back({FroggersActions::kSceneSelect, "1", "Scene 2", std::nullopt});
    catalog.actions.push_back(
        {FroggersActions::kBpm, "", "BPM", std::make_pair(kFroggersBpmMin, kFroggersBpmMax)});

    catalog.libraryKinds = {
        synth::UISystemMessage::ParamIncDec,
        synth::UISystemMessage::ParamSetAbsolute,
        synth::UISystemMessage::ParamPush,
        synth::UISystemMessage::SetSceneBlend,
        synth::UISystemMessage::HoldDrill,
        synth::UISystemMessage::Shift,
    };
    catalog.encoderPressAction = FroggersActions::kEncoderPress;
    catalog.patchCarriesMappings = true;
    catalog.deviceDefaults = {
        TwisterDeviceDefault(),
        Apc40GenericDeviceDefault(),
        Apc40AbletonDeviceDefault(),
        LaunchpadXDeviceDefault(),
        LaunchpadProMk3DeviceDefault(),
        LaunchpadMiniMk3DeviceDefault(),
    };
    return catalog;
}

}  // namespace synth_froggers
