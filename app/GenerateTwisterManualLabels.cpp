// GenerateTwisterManualLabels.cpp -- writes assets/manual/twister-controls.json,
// the facts app/render_twister_manual_diagrams.mjs draws MANUAL.md's two MIDI
// Fighter Twister diagrams from, and app/check_twister_manual_diagrams_drift.py
// checks MANUAL.md's side-button table against.
//
// The preset itself comes through the library's own persistence serializer
// (synth::ToJSON on the MidiControllerProfileConfig FroggersMidiCatalog()
// installs), embedded here as "preset" so the render script can confirm it
// recognizes every field on an encoder turn or push -- this file never
// re-types a control address by hand, and a later change to that mapping
// fails the render instead of going unlabelled.
//
// Alongside the raw preset, this program writes the two label tables the
// render script and the drift check both read rather than re-deriving:
// "encoders" (16 rows, one per physical grid position, each carrying what its
// turn moves, what it moves instead while Shift is held, and what its push
// does) and "sideButtons" (6 rows, one per
// physical place on the unit, derived from each button's own CC rather than
// its position in the catalog's array). Every label comes from data the
// preset or FroggersMidiCatalog() already carries -- an app action's display
// name from catalog.actions, and a control-state job's (Shift's) display name
// from Sheaf's own MakeUISystemMessageChoices/FindUISystemMessageChoice,
// the same lookup the Controllers page's own forms use instead of duplicating
// its labels. A press or shifted press this program cannot resolve through
// either path stops the program with an error naming it, rather than
// guessing.
//
// The two page slots whose identity is the same regardless of which bank is
// open -- Crispy and Crunchy -- are named directly; the other fourteen are
// named by slot number alone ("Slot N"), since what a numbered slot does
// depends on which bank the player has open, and the manual's own bank
// sections (not this diagram) say what a slot does in each one.
//
// Usage: GenerateTwisterManualLabels <output-path>

#include "FroggersMidiCatalog.hpp"

#include "synth/Json.hpp"
#include "synth/MidiConfigViewModel.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {

// JSON::Dumps() has no pretty-printing option of its own (Json.hpp's own
// header comment: it targets non-realtime handoff code, not human review) --
// this reformats its compact output by brace depth so the committed file is
// readable and diffable.
std::string PrettyPrint(const char* compact) {
    std::string out;
    int depth = 0;
    bool inString = false;
    for (const char* p = compact; *p != '\0'; ++p) {
        const char c = *p;
        if (inString) {
            out.push_back(c);
            if (c == '\\' && *(p + 1) != '\0') {
                out.push_back(*(++p));
                continue;
            }
            if (c == '"') {
                inString = false;
            }
            continue;
        }
        switch (c) {
            case '"':
                inString = true;
                out.push_back(c);
                break;
            case '{':
            case '[': {
                out.push_back(c);
                const char next = *(p + 1);
                if (next == '}' || next == ']') {
                    out.push_back(*(++p));
                    break;
                }
                ++depth;
                out.push_back('\n');
                out.append(static_cast<std::size_t>(depth) * 2, ' ');
                break;
            }
            case '}':
            case ']':
                --depth;
                out.push_back('\n');
                out.append(static_cast<std::size_t>(depth) * 2, ' ');
                out.push_back(c);
                break;
            case ',':
                out.push_back(c);
                out.push_back('\n');
                out.append(static_cast<std::size_t>(depth) * 2, ' ');
                break;
            case ':':
                out.push_back(c);
                out.push_back(' ');
                break;
            default:
                out.push_back(c);
        }
    }
    out.push_back('\n');
    return out;
}

// Resolves a press (or shifted press) to the text the Controllers page itself
// would show for it: an app-action label from the app's own catalog, or one
// of Sheaf's own display names (UISystemMessageCatalog()'s "Shift"/"Hold
// Drill" and the rest) -- the same lookup Sheaf's own forms use instead of
// duplicating its labels. Stops the program if the message's own type has no
// counterpart to look up, or the lookup finds nothing, rather than showing a
// placeholder for a job it cannot actually name.
std::string ResolveButtonLabel(synth::JsonArena& arena, const std::vector<synth::UISystemMessageChoice>& uiChoices,
                                const synth::MessageIn& message, const std::string& appAction,
                                const std::string& appActionValue) {
    synth::UISystemMessage kind;
    if (message.type == synth::MessageIn::Type::AppAction) {
        kind = synth::UISystemMessage::AppAction;
    } else if (message.type == synth::MessageIn::Type::Shift) {
        kind = synth::UISystemMessage::Shift;
    } else {
        // ToJSON already knows how to spell every MessageIn::Type as text
        // (the same text the preset itself would carry); reuse that instead
        // of a second name table just for this error.
        const char* typeName = synth::ToJSON(arena, message).Get("type").StringValue();
        std::fprintf(stderr,
                     "GenerateTwisterManualLabels: no display name for side-button message type \"%s\" -- teach "
                     "ResolveButtonLabel about it\n",
                     typeName != nullptr ? typeName : "?");
        std::exit(1);
    }
    const synth::UISystemMessageChoice* choice = synth::FindUISystemMessageChoice(kind, uiChoices, appAction, appActionValue);
    if (choice == nullptr) {
        std::fprintf(stderr,
                     "GenerateTwisterManualLabels: no display name for side-button action \"%s\" (value \"%s\")\n",
                     appAction.c_str(), appActionValue.c_str());
        std::exit(1);
    }
    return choice->label;
}

// The randomize note belongs to the two randomize jobs themselves -- Randomize
// Page and Randomize All are the one job that leaves Crunchy out
// (FroggersModulationTests.cpp's
// crunchy_is_never_randomized_by_either_button_in_any_view) -- so it is
// decided per job, from that job's own action name, never from which button
// or which of its two presses (plain or shifted) happens to carry it.
std::string NoteForAction(const std::string& action) {
    if (action == synth_froggers::FroggersActions::kRandomizePage || action == synth_froggers::FroggersActions::kRandomizeAll) {
        return " (never Crunchy)";
    }
    return "";
}

struct SidePlacement {
    std::size_t index;   // 0..5, "sideButtons" array order: left top to bottom, then right top to bottom
    std::string place;   // "Left top" .. "Right bottom", MANUAL.md's own table wording
};

// The Twister sends the six side buttons' CC down two columns -- CC 8 to 10
// down the left column from the top, CC 11 to 13 down the right column from
// the top (MANUAL.md's Utility paragraph, and this app's own
// FroggersMidiCatalog.hpp header comment) -- so a button's physical place
// comes from its own CC, not from where it sits in the catalog's array.
SidePlacement PlaceForCC(std::uint8_t cc) {
    if (cc < 8 || cc > 13) {
        std::fprintf(stderr, "GenerateTwisterManualLabels: side button CC %d is outside the Twister's 8-13 range\n",
                     static_cast<int>(cc));
        std::exit(1);
    }
    static const char* const kRowNames[3] = {"top", "middle", "bottom"};
    const bool right = cc >= 11;
    const std::size_t row = static_cast<std::size_t>(cc - 8) % 3;
    std::string place = right ? "Right " : "Left ";
    place += kRowNames[row];
    return {(right ? 3u : 0u) + row, place};
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <output-path>\n", argv[0]);
        return 2;
    }

    const synth::MidiAppCatalog catalog = synth_froggers::FroggersMidiCatalog();
    const std::vector<synth::UISystemMessageChoice> uiChoices = synth::MakeUISystemMessageChoices(catalog);

    const synth::MidiAppDeviceDefault* twister = nullptr;
    for (const synth::MidiAppDeviceDefault& device : catalog.deviceDefaults) {
        if (device.id == "froggers.twister") {
            twister = &device;
            break;
        }
    }
    if (twister == nullptr) {
        std::fprintf(stderr,
                     "GenerateTwisterManualLabels: no \"froggers.twister\" device default in "
                     "FroggersMidiCatalog()\n");
        return 1;
    }
    if (!twister->config.encoderInput.has_value()) {
        std::fprintf(stderr, "GenerateTwisterManualLabels: %s has no encoderInput\n", twister->id.c_str());
        return 1;
    }

    synth::JsonArena arena(synth::JsonArena::kDefaultCapacity);
    synth::JSON root = arena.Object();
    root.SetNew("displayName", arena.String(twister->displayName.c_str()));

    // --- Encoders: 16 rows keyed by physical grid position ------------------
    constexpr std::size_t kEncoderCount = 16;
    const std::vector<synth::EncoderMidiMapping>& turns = twister->config.encoderInput->turns;
    const std::vector<synth::EncoderMidiMapping>& pushes = twister->config.encoderInput->pushes;
    if (turns.size() != kEncoderCount || pushes.size() != kEncoderCount) {
        std::fprintf(stderr,
                     "GenerateTwisterManualLabels: expected %zu encoder turns and pushes, found %zu turns / %zu "
                     "pushes\n",
                     kEncoderCount, turns.size(), pushes.size());
        return 1;
    }

    std::array<bool, kEncoderCount> turnSeen{};
    std::array<std::string, kEncoderCount> turnLabels;
    // The turn's shifted job, in the same label vocabulary the Controllers
    // page shows (EncoderShiftedJobCatalog(): "(none)" or "Scene Blend").
    std::array<std::string, kEncoderCount> turnShiftedJobLabels;
    for (const synth::EncoderMidiMapping& turn : turns) {
        if (turn.position >= kEncoderCount || turnSeen[turn.position]) {
            std::fprintf(stderr, "GenerateTwisterManualLabels: encoder turn position %zu is out of range or duplicated\n",
                         turn.position);
            return 1;
        }
        turnSeen[turn.position] = true;
        // The slot a turn moves is that same position: the preset assigns
        // each of the 16 encoders the parameter slot with its own number
        // (Sheaf's RowMajorInputDefault), so the grid cell and the slot it
        // edits are one value.
        if (turn.position == synth_froggers::kFroggersCrispySlot) {
            turnLabels[turn.position] = "Crispy";
        } else if (turn.position == synth_froggers::kFroggersCrunchySlot) {
            turnLabels[turn.position] = "Crunchy";
        } else {
            turnLabels[turn.position] = "Slot " + std::to_string(turn.position);
        }
        const std::vector<std::string>& shiftedJobCatalog = synth::EncoderShiftedJobCatalog();
        const std::size_t shiftedJobIx = static_cast<std::size_t>(turn.shiftedJob);
        if (shiftedJobIx >= shiftedJobCatalog.size()) {
            std::fprintf(stderr, "GenerateTwisterManualLabels: encoder turn at position %zu has an unknown shifted job\n",
                         turn.position);
            return 1;
        }
        turnShiftedJobLabels[turn.position] = shiftedJobCatalog[shiftedJobIx];
    }
    for (std::size_t position = 0; position < kEncoderCount; ++position) {
        if (!turnSeen[position]) {
            std::fprintf(stderr, "GenerateTwisterManualLabels: no encoder turn mapped to position %zu\n", position);
            return 1;
        }
    }

    std::array<bool, kEncoderCount> pushSeen{};
    for (const synth::EncoderMidiMapping& push : pushes) {
        if (push.position >= kEncoderCount || pushSeen[push.position]) {
            std::fprintf(stderr, "GenerateTwisterManualLabels: encoder push position %zu is out of range or duplicated\n",
                         push.position);
            return 1;
        }
        pushSeen[push.position] = true;
    }
    for (std::size_t position = 0; position < kEncoderCount; ++position) {
        if (!pushSeen[position]) {
            std::fprintf(stderr, "GenerateTwisterManualLabels: no encoder push mapped to position %zu\n", position);
            return 1;
        }
    }

    synth::JSON encoders = arena.Array();
    for (std::size_t position = 0; position < kEncoderCount; ++position) {
        synth::JSON entry = arena.Object();
        entry.SetNew("position", arena.Integer(static_cast<std::int64_t>(position)));
        entry.SetNew("turn", arena.String(turnLabels[position].c_str()));
        // What this turn does while Shift is held: "(none)" (the ordinary
        // turn keeps its job) or the shifted job's own Controllers-page
        // label, e.g. "Scene Blend".
        entry.SetNew("shiftedTurn", arena.String(turnShiftedJobLabels[position].c_str()));
        // Every encoder push drills into its own slot's modulation, exactly
        // like an on-screen press (MANUAL.md's "What can be mapped"); the
        // mapping carries no separate job field for this, so the fact drawn
        // here is confirmed above only by a push actually being mapped to
        // this position.
        entry.SetNew("push", arena.String("Drill"));
        encoders.Append(entry);
    }
    root.SetNew("encoders", encoders);

    // --- Side buttons: 6 rows keyed by physical place, from each button's CC
    if (twister->config.systemMessages.size() != 6) {
        std::fprintf(stderr, "GenerateTwisterManualLabels: expected 6 side buttons, found %zu\n",
                     twister->config.systemMessages.size());
        return 1;
    }
    std::array<synth::JSON, 6> sideButtonSlots;
    std::array<bool, 6> sideButtonSeen{};
    for (const synth::MidiControllerSystemMessageAssociation& button : twister->config.systemMessages) {
        if (!button.control.has_value()) {
            std::fprintf(stderr, "GenerateTwisterManualLabels: a Twister side button has no CC address\n");
            return 1;
        }
        const SidePlacement placement = PlaceForCC(button.control->cc);
        if (sideButtonSeen[placement.index]) {
            std::fprintf(stderr, "GenerateTwisterManualLabels: two side buttons place at \"%s\"\n", placement.place.c_str());
            return 1;
        }
        sideButtonSeen[placement.index] = true;

        synth::JSON entry = arena.Object();
        entry.SetNew("place", arena.String(placement.place.c_str()));
        entry.SetNew("isShift", arena.Boolean(button.press.type == synth::MessageIn::Type::Shift));
        entry.SetNew("press", arena.String(ResolveButtonLabel(arena, uiChoices, button.press, button.appAction,
                                                               button.appActionValue)
                                                .c_str()));
        entry.SetNew("pressNote", arena.String(NoteForAction(button.appAction).c_str()));
        if (button.shiftedPress.has_value()) {
            entry.SetNew("shiftedPress",
                         arena.String(ResolveButtonLabel(arena, uiChoices, *button.shiftedPress, button.shiftedAppAction,
                                                          button.shiftedAppActionValue)
                                          .c_str()));
            entry.SetNew("shiftedPressNote", arena.String(NoteForAction(button.shiftedAppAction).c_str()));
        } else {
            // MANUAL.md's own table wording for a button with no shifted job.
            entry.SetNew("shiftedPress", arena.String("(none)"));
            entry.SetNew("shiftedPressNote", arena.String(""));
        }
        sideButtonSlots[placement.index] = entry;
    }
    synth::JSON sideButtons = arena.Array();
    for (std::size_t index = 0; index < 6; ++index) {
        sideButtons.Append(sideButtonSlots[index]);
    }
    root.SetNew("sideButtons", sideButtons);

    // The raw preset, through the library's own persistence serializer --
    // kept alongside the resolved rows above so render_twister_manual_diagrams.mjs
    // can confirm it recognizes every field on an encoder turn or push,
    // catching a field a later change adds before it goes unlabelled.
    root.SetNew("preset", synth::ToJSON(arena, twister->config));

    char* compact = root.Dumps(0);
    if (compact == nullptr) {
        std::fprintf(stderr, "GenerateTwisterManualLabels: JSON serialization failed\n");
        return 1;
    }
    const std::string pretty = PrettyPrint(compact);
    std::free(compact);

    std::ofstream out(argv[1], std::ios::trunc | std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "GenerateTwisterManualLabels: cannot open %s for writing\n", argv[1]);
        return 1;
    }
    out << pretty;
    if (!out) {
        std::fprintf(stderr, "GenerateTwisterManualLabels: write to %s failed\n", argv[1]);
        return 1;
    }
    return 0;
}
