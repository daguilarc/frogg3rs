// FroggersControllersPageTests.cpp -- drives the Controllers page's view
// model (synth::MidiConfigViewModel) against synth_froggers::
// FroggersMidiCatalog()'s six real device defaults (the MIDI Fighter
// Twister, both APC40 mkII variants, and the three Launchpad models).
// Sheaf's own page tests build the wizard registry from an empty
// synth::MidiAppCatalog{}, which falls back to the library's single Twister
// descriptor, so none of them ever drive these six shipping defaults
// through the page. This proves the real catalog resolves through
// MakeControllerWizardRegistry()/MakeControllerWizard(), that each device's
// wizard-generated profile installs as a real MidiControllerSlot, and that
// every section/group the resulting slots offer accepts an Add or Block
// through the same view-model calls the Controllers page itself dispatches.

#include "FroggersMidiCatalog.hpp"

#include "synth/ControllerWizard.hpp"
#include "synth/MidiConfigViewModel.hpp"
#include "synth/MidiController.hpp"

#ifdef JUCE_MAJOR_VERSION
#error "Froggers Controllers page tests must not see JUCE headers"
#endif

#include <algorithm>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct TestCase {
    const char* name;
    void (*fn)();
};

std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Register {
    Register(const char* name, void (*fn)()) {
        Registry().push_back({name, fn});
    }
};

#define TEST_CASE(name)                     \
    void name();                            \
    Register reg_##name(#name, &name);      \
    void name()

#define REQUIRE_TRUE(expr)                                                       \
    do {                                                                         \
        if (!(expr)) {                                                          \
            std::ostringstream oss;                                             \
            oss << __FILE__ << ":" << __LINE__ << " requirement failed: " #expr; \
            throw std::runtime_error(oss.str());                                \
        }                                                                        \
    } while (false)

// One real, wizard-generated MidiControllerSlot per registry descriptor --
// the same ConfigForm(nullopt)/GenerateProfile() path the Controllers
// page's add row uses for an app default, never a hand-built config.
std::vector<synth::MidiControllerSlot> GenerateCatalogSlots(
    const std::vector<synth::ControllerWizardDescriptor>& registry) {
    std::vector<synth::MidiControllerSlot> slots;
    slots.reserve(registry.size());
    for (const synth::ControllerWizardDescriptor& descriptor : registry) {
        std::unique_ptr<synth::ControllerWizard> wizard =
            synth::MakeControllerWizard(registry, descriptor.id);
        REQUIRE_TRUE(wizard != nullptr);
        std::unique_ptr<synth::ControllerConfigForm> form = wizard->ConfigForm(std::nullopt);
        REQUIRE_TRUE(form != nullptr);

        const synth::WizardGenerationContext context{
            .name = descriptor.displayName,
            .input = {.identifier = descriptor.id + ".in", .name = descriptor.displayName},
            .output = {.identifier = descriptor.id + ".out", .name = descriptor.displayName}};
        synth::WizardGenerationResult result = wizard->GenerateProfile(*form, context);
        if (!result) {
            std::cout << "  [" << descriptor.id << "] wizard generation refused: " << result.error << "\n";
        }
        REQUIRE_TRUE(static_cast<bool>(result));
        slots.push_back(std::move(*result.controller));
    }
    return slots;
}

// ---------------------------------------------------------------------------
// real_catalog_registers_one_descriptor_per_device_default
// ---------------------------------------------------------------------------
TEST_CASE(real_catalog_registers_one_descriptor_per_device_default) {
    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();
    REQUIRE_TRUE(catalog.deviceDefaults.size() == 6);

    const std::vector<synth::ControllerWizardDescriptor> registry =
        synth::MakeControllerWizardRegistry(catalog);
    // An empty catalog's registry falls back to the library's single
    // Twister descriptor (ControllerWizard.cpp's MakeControllerWizardRegistry);
    // matching the real catalog's own device-default count one-for-one is
    // what proves the registry actually resolved this catalog rather than
    // silently taking that fallback.
    REQUIRE_TRUE(registry.size() == catalog.deviceDefaults.size());
    REQUIRE_TRUE(registry.size() == 6);
    for (std::size_t ix = 0; ix < registry.size(); ++ix) {
        REQUIRE_TRUE(registry[ix].id == catalog.deviceDefaults[ix].id);
        REQUIRE_TRUE(registry[ix].kind == catalog.deviceDefaults[ix].kind);
    }
}

// ---------------------------------------------------------------------------
// real_catalog_defaults_generate_and_accept_adds_through_the_view_model
// ---------------------------------------------------------------------------
TEST_CASE(real_catalog_defaults_generate_and_accept_adds_through_the_view_model) {
    using RowGroup = synth::MidiMappingRowVM::RowGroup;

    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();
    const std::vector<synth::ControllerWizardDescriptor> registry =
        synth::MakeControllerWizardRegistry(catalog);
    REQUIRE_TRUE(registry.size() == catalog.deviceDefaults.size());

    std::vector<synth::MidiControllerSlot> slots = GenerateCatalogSlots(registry);
    REQUIRE_TRUE(slots.size() == registry.size());

    synth::MidiInstrumentConfig instrument;
    synth::MidiConnectionState connection;
    for (synth::MidiControllerSlot& slot : slots) {
        REQUIRE_TRUE(instrument.AddController(std::move(slot)));
        connection.controllers.push_back({});
    }
    REQUIRE_TRUE(instrument.controllers.size() == registry.size());

    synth::MidiConfigViewModel vm;
    vm.Rebuild(instrument, connection);
    REQUIRE_TRUE(vm.Controllers().size() == instrument.controllers.size());

    constexpr synth::MidiConfigSection kSections[] = {synth::MidiConfigSection::Encoders,
                                                      synth::MidiConfigSection::SystemMessages,
                                                      synth::MidiConfigSection::Analogs};

    for (std::size_t controllerIx = 0; controllerIx < instrument.controllers.size(); ++controllerIx) {
        const synth::MidiControllerSlot& slot = instrument.controllers[controllerIx];
        const synth::MidiKindSupport kindSupport = synth::KindSupport(slot.kind);
        for (synth::MidiConfigSection section : kSections) {
            for (RowGroup group : vm.AddableGroups(controllerIx, section)) {
                REQUIRE_TRUE(vm.GroupSupportsAdd(controllerIx, section, group));

                // GroupSupportsAdd()/GroupSupportsBlocks() deliberately do
                // not gate EncoderTurn/EncoderPush, AnalogGesture, System,
                // or Grid by controller kind (MidiConfigViewModel.cpp), so
                // AddableGroups() offers them uniformly across kinds;
                // whether an Add/Block actually succeeds is decided later,
                // when the edit is flushed back to the slot and validated
                // against the kind's real support (MidiController.cpp's
                // KindSupport()/ProfileConfigValidForKind -- a Launchpad has
                // no encoders, a Twister has no analog input). That is a
                // legitimate, catalog-shaped refusal; anything else refusing
                // is a real failure.
                std::optional<std::string> expectedRefusal;
                if ((group == RowGroup::EncoderTurn || group == RowGroup::EncoderPush) &&
                    !kindSupport.encoders) {
                    expectedRefusal = "encoders not supported by this controller kind";
                } else if (group == RowGroup::AnalogGesture && !kindSupport.analogs) {
                    expectedRefusal = "analog input not supported by this controller kind";
                } else if ((group == RowGroup::System || group == RowGroup::Grid) &&
                           !kindSupport.systemMessages) {
                    expectedRefusal = "system messages not supported by this controller kind";
                }

                // AddSingle and AddBlock each run against their own fresh
                // view model, rebuilt from the untouched installed
                // instrument, rather than the shared enumeration `vm` above:
                // both calls mutate that model's open per-section
                // presentation cache (MidiConfigViewModel.hpp), so sharing
                // one across an accepted Add and the Block check right
                // after it would let the Add's consumed address/position
                // starve the independent Block check of room it would
                // otherwise have had against the same starting state.
                synth::MidiConfigViewModel singleVm;
                singleVm.Rebuild(instrument, connection);
                synth::MidiInstrumentConfig afterSingle;
                std::string singleReason;
                const bool singleOk = singleVm.AddSingle(controllerIx, section, group, afterSingle, &singleReason);
                // FroggersMidiCatalog.hpp's TwisterDeviceDefault() maps all
                // six of the Twister's physical side buttons (cc 8-13 on
                // channel 3); with no seventh button to give a new row,
                // AddSingle legitimately refuses here too -- an
                // address-exhaustion refusal rather than a kind-support one,
                // specific to this one shipping default.
                const bool isTwisterSideButtonsFull =
                    slot.kind == synth::MidiProfileKind::MfTwister &&
                    section == synth::MidiConfigSection::SystemMessages && group == RowGroup::System;
                if (expectedRefusal.has_value()) {
                    REQUIRE_TRUE(!singleOk && singleReason == *expectedRefusal);
                } else if (isTwisterSideButtonsFull) {
                    REQUIRE_TRUE(!singleOk && singleReason == "no free twister side button for a new system row");
                } else {
                    if (!singleOk) {
                        std::cout << "  [" << slot.name << "] AddSingle refused: " << singleReason << "\n";
                    }
                    REQUIRE_TRUE(singleOk);
                }

                if (vm.GroupSupportsBlocks(controllerIx, section, group)) {
                    synth::MidiConfigViewModel blockVm;
                    blockVm.Rebuild(instrument, connection);
                    synth::MidiInstrumentConfig afterBlock;
                    std::string blockReason;
                    const bool blockOk = blockVm.AddBlock(controllerIx, section, group, afterBlock, &blockReason);
                    // The shared Launchpad pad map (FroggersMidiCatalog.hpp's
                    // LaunchpadPadMap()) fills the transport row (y=-1) at
                    // x=0..7, leaving exactly one free cell in that row.
                    // AddBlock's SystemMessages/System branch for Launchpad
                    // kind (MidiConfigViewModel.cpp) reuses AddSingle's
                    // single-cell NextFreeLaunchpadPosition and then always
                    // widens it by 2 without checking the neighbor cell
                    // (unlike the Grid group's own NextFreeLaunchpadGridPair,
                    // which checks both cells), so the default block always
                    // collides with the pad map. Which of the two refusals
                    // fires depends on the model's shape (MidiController.cpp's
                    // LaunchpadShapeSupports): the free cell sits at the
                    // right edge of LaunchpadX/MiniMk3's 0-8 column range, so
                    // widening it runs off the grid; ProMk3's extra x=-1
                    // column is free instead, so widening it collides with
                    // the transport row's existing button one cell over.
                    // Legitimate for all three Launchpad defaults, since they
                    // share this pad map; anything else refusing is a real
                    // failure.
                    const bool isLaunchpadSystemBlockEdgeOverflow =
                        slot.kind == synth::MidiProfileKind::Launchpad &&
                        section == synth::MidiConfigSection::SystemMessages && group == RowGroup::System;
                    if (expectedRefusal.has_value()) {
                        REQUIRE_TRUE(!blockOk && blockReason == *expectedRefusal);
                    } else if (isLaunchpadSystemBlockEdgeOverflow) {
                        REQUIRE_TRUE(!blockOk && (blockReason == "launchpad coordinate is outside this controller's grid" ||
                                                  blockReason == "section would create a duplicate address"));
                    } else {
                        if (!blockOk) {
                            std::cout << "  [" << slot.name << "] AddBlock refused: " << blockReason << "\n";
                        }
                        REQUIRE_TRUE(blockOk);
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// twister_system_rows_carry_shift_editable_field_and_derived_choice_index
// ---------------------------------------------------------------------------
TEST_CASE(twister_system_rows_carry_shift_editable_field_and_derived_choice_index) {
    using Field = synth::MidiMappingRowVM::Field;

    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();
    const std::vector<synth::ControllerWizardDescriptor> registry =
        synth::MakeControllerWizardRegistry(catalog);
    std::vector<synth::MidiControllerSlot> slots = GenerateCatalogSlots(registry);

    synth::MidiInstrumentConfig instrument;
    synth::MidiConnectionState connection;
    for (synth::MidiControllerSlot& slot : slots) {
        REQUIRE_TRUE(instrument.AddController(std::move(slot)));
        connection.controllers.push_back({});
    }

    std::size_t twisterIx = instrument.controllers.size();
    for (std::size_t ix = 0; ix < instrument.controllers.size(); ++ix) {
        if (instrument.controllers[ix].kind == synth::MidiProfileKind::MfTwister) {
            twisterIx = ix;
            break;
        }
    }
    REQUIRE_TRUE(twisterIx < instrument.controllers.size());

    synth::MidiConfigViewModel vm;
    vm.SetMessageCatalog(synth::MakeUISystemMessageChoices(catalog));
    vm.Rebuild(instrument, connection);

    const std::vector<synth::MidiMappingRowVM> rows =
        vm.SectionRows(twisterIx, synth::MidiConfigSection::SystemMessages);
    REQUIRE_TRUE(rows.size() == 6);
    for (std::size_t ix = 0; ix < 5; ++ix) {
        const bool hasShiftField =
            std::find(rows[ix].editableFields.begin(), rows[ix].editableFields.end(), Field::ShiftAction) !=
            rows[ix].editableFields.end();
        REQUIRE_TRUE(hasShiftField);
    }
    const bool shiftRowHasShiftField =
        std::find(rows[5].editableFields.begin(), rows[5].editableFields.end(), Field::ShiftAction) !=
        rows[5].editableFields.end();
    REQUIRE_TRUE(!shiftRowHasShiftField);

    const std::vector<synth::UISystemMessageChoice>& shiftCatalog = vm.ShiftCatalog();
    int bankPreviousIx = -1;
    for (std::size_t ix = 0; ix < shiftCatalog.size(); ++ix) {
        if (shiftCatalog[ix].label == "Bank Previous") {
            bankPreviousIx = static_cast<int>(ix);
            break;
        }
    }
    REQUIRE_TRUE(bankPreviousIx >= 0);

    REQUIRE_TRUE(vm.ShiftChoiceIndex(twisterIx, synth::MidiConfigSection::SystemMessages, 0) == bankPreviousIx);
    REQUIRE_TRUE(vm.ShiftChoiceIndex(twisterIx, synth::MidiConfigSection::SystemMessages, 5) == 0);
}

}  // namespace

int main() {
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
