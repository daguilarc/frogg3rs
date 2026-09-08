// FroggersSurfaceTests.cpp -- surface layout tests: layout bounds; the
// in-place drill-in swap; no overlap at the target window size; every
// encoder cell fully inside the grid region; and the encoder ring renders
// the fuegoized value with no visualizer-specific UI code. Also covers BPM
// normal/external-clock-slaved states and the "exactly two randomize
// controls, neither retired control present" requirement.
//
// Pure layout-math checks (bounds, no-overlap, cell containment) construct
// `FroggersPageLayout`/`FroggersEncoderGridLayout` directly -- no
// Engine/SynthRig needed, since that math has no dependency on live
// parameter state. The drill-in swap, encoder-ring, BPM, and
// randomize-control-inventory checks drive a real `synth_froggers::
// FroggersApp` through `synth_rig::SynthRig`, same convention as every other
// file's runtime tests (FroggersHeadlessTests.cpp, etc.) --
// dispatching actions through the actual `FroggersUiSurface` rather than
// reaching into FroggersApp/FroggersParameterModel directly, since the
// surface's own action routing (including the pending-atomic bridge
// FroggersAppCore.hpp's ProcessFrame() drains) is what's under test here.

#include "Froggers.hpp"
#include "FroggersParameters.hpp"
#include "FroggersUiSurface.hpp"
#include "support/SynthRig.hpp"

#include "synth/EncoderDraw.hpp"
#include "synth/MasterClock.hpp"
#include "synth/ParameterModulation.hpp"
#include "synth/PortableUI.hpp"

#ifdef JUCE_MAJOR_VERSION
#error "Froggers surface tests must not see JUCE headers"
#endif

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <set>
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

synth::RuntimeDataPaths UseScratchRuntimeDataPaths(const char* testName) {
    const std::filesystem::path dataRoot =
        std::filesystem::temp_directory_path() / "froggers-surface-tests" / testName;
    std::filesystem::remove_all(dataRoot);
    synth::RuntimeDataPaths paths = synth::RuntimeDataPaths::FromDataRoot(dataRoot);
    std::filesystem::create_directories(paths.patchesRoot);
    return paths;
}

bool Overlaps(synth::ui::Bounds a, synth::ui::Bounds b) {
    return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}

bool FullyInside(synth::ui::Bounds inner, synth::ui::Bounds outer) {
    return inner.x >= outer.x - 0.01f && inner.y >= outer.y - 0.01f &&
           inner.x + inner.width <= outer.x + outer.width + 0.01f &&
           inner.y + inner.height <= outer.y + outer.height + 0.01f;
}

std::set<std::string> NodeIds(const synth::ui::NodeTree& tree) {
    std::set<std::string> ids;
    for (const synth::ui::Node& node : tree.nodes) {
        ids.insert(node.id.value);
    }
    return ids;
}

bool HasButtonLabeled(const synth::ui::NodeTree& tree, const std::string& label) {
    for (const synth::ui::Node& node : tree.nodes) {
        if (node.kind == synth::ui::NodeKind::Button && node.label == label) {
            return true;
        }
    }
    return false;
}

const synth::ui::Node* FindNodeById(const synth::ui::NodeTree& tree, const std::string& id) {
    for (const synth::ui::Node& node : tree.nodes) {
        if (node.id.value == id) {
            return &node;
        }
    }
    return nullptr;
}

// The drill-level indicator is one more DrawCommand appended to its
// carrying node's own output, not a separate node -- so reading it back
// means scanning THAT node's drawCommands for its Kind::Text entry, rather
// than a FindNodeById lookup by an indicator-specific id.
//
// The carrying node's identity has moved over time (see AppendScopeCell's
// and AppendEncoderCell's own comments, FroggersUiSurface.hpp, for the full
// history): it used to always be kVcoScope, then briefly the Target/Back
// grid cell -- both are dead ends (unreachable, then mislabelled "BACK").
// These three helpers already took the node id as a parameter rather than
// assuming one fixed id, which is exactly what let the carrying node move
// again to its own dedicated header row (`FroggersNodeIds::kModulationHeader`,
// AppendModulationHeaderRow()) with no changes needed here.
//
// LAST match, not first: kept for the reason it was added -- an encoder
// cell (still used below by the Target/Back-cell REMOVAL guards, which now
// assert ABSENCE of any "BACK" text rather than presence of the old badge)
// can carry its own unrelated `AppendBadge` (EncoderDraw.hpp:607)
// `DrawCommand::Text` entries (e.g. "M1") for any connected, modulated
// parameter, so "the first Kind::Text command" on such a cell finds a badge
// label, not this file's own indicator. `kModulationHeader` itself never
// carries badges (it is not an encoder cell -- `AppendBadge` is only ever
// reached from `BuildEncoderDrawCommands`, which AppendModulationHeaderRow
// never calls), so last-vs-first is moot for its own carrying node, but
// stays general rather than special-cased per node id.
//
// The permanent overdraw regression guard
// (modulation_header_sits_below_bank_row_and_above_parameter_cells below)
// needs the indicator's INDEX within its node's command list, to check
// what -- if anything -- follows it; DrillBadgeTextCommand below only ever
// needed the command itself. Rather than let a second caller grow a
// second scan of the same list, this is the one scan site and
// DrillBadgeTextCommand is rewritten in terms of it (a third use of "find a
// node's Kind::Text command" must not become a third scan).
std::optional<std::size_t> DrillBadgeTextIndex(const synth::ui::NodeTree& tree, const std::string& nodeId) {
    const synth::ui::Node* node = FindNodeById(tree, nodeId);
    if (node == nullptr) {
        return std::nullopt;
    }
    std::optional<std::size_t> lastTextIndex;
    for (std::size_t ix = 0; ix < node->drawCommands.size(); ++ix) {
        if (node->drawCommands[ix].kind == synth::ui::DrawCommand::Kind::Text) {
            lastTextIndex = ix;
        }
    }
    return lastTextIndex;
}

// Returns the WHOLE command, not just `.text`, so a caller can also
// assert on the resolved TextStyle (size/colour) -- the operator's actual
// complaint was that the label rendered but did not READ as a header, so a
// content-only check can't cover the fix. `DrillBadgeText` below is the
// pre-existing `.text`-only accessor, rewritten in terms of this one so a
// second use of "find a node's Kind::Text command" does not become a
// second scan of the same list.
std::optional<synth::ui::DrawCommand> DrillBadgeTextCommand(const synth::ui::NodeTree& tree,
                                                            const std::string& nodeId) {
    const std::optional<std::size_t> index = DrillBadgeTextIndex(tree, nodeId);
    if (!index.has_value()) {
        return std::nullopt;
    }
    const synth::ui::Node* node = FindNodeById(tree, nodeId);
    return node != nullptr ? std::optional<synth::ui::DrawCommand>(node->drawCommands[*index]) : std::nullopt;
}

// Reused (2+ call sites) across every level checked by
// modulation_header_shown_only_while_drilled_in_and_matches_the_level below;
// isolates a distinct read from the raw draw-command list.
std::optional<std::string> DrillBadgeText(const synth::ui::NodeTree& tree, const std::string& nodeId) {
    const std::optional<synth::ui::DrawCommand> command = DrillBadgeTextCommand(tree, nodeId);
    return command.has_value() ? std::optional<std::string>(command->text) : std::nullopt;
}

// STEP 1 (2026-08-09) removal guards: "BACK L<N>" must never be drawn
// anywhere again -- not just absent from whichever node happened to carry
// it. Unlike DrillBadgeText above (which finds THE ONE indicator text on a
// node known to carry at most one), a Target/Back-style encoder cell can
// legitimately carry OTHER Text commands (modulator/gesture badges, e.g.
// "M1" -- see DrillBadgeTextIndex's own comment), so the removal guard
// cannot assert "this node has no Text command at all"; it has to check
// the CONTENT of whatever Text commands do exist. One low-level scan
// (CommandsContainText) shared by both the whole-tree and single-node
// forms below, so two callers do not duplicate the same loop.
bool CommandsContainText(const std::vector<synth::ui::DrawCommand>& commands, const std::string& needle) {
    for (const synth::ui::DrawCommand& command : commands) {
        if (command.kind == synth::ui::DrawCommand::Kind::Text && command.text.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// Scoped guard: the task's literal ask ("nothing reading 'BACK' is drawn on
// the Target/Back cell").
bool NodeDrawnTextContains(const synth::ui::NodeTree& tree, const std::string& nodeId, const std::string& needle) {
    const synth::ui::Node* node = FindNodeById(tree, nodeId);
    return node != nullptr && CommandsContainText(node->drawCommands, needle);
}

// Whole-tree guard: strictly stronger than the scoped one above -- the
// operator's actual complaint was that a "BACK" label showed up somewhere
// they were looking, not that it showed up on one specific pre-named node,
// so this closes off the same regression appearing on any OTHER node too.
bool AnyDrawnTextContains(const synth::ui::NodeTree& tree, const std::string& needle) {
    for (const synth::ui::Node& node : tree.nodes) {
        if (CommandsContainText(node.drawCommands, needle)) {
            return true;
        }
    }
    return false;
}

// A resolved node's `bounds` are PARENT-relative, not accumulated screen
// coordinates (PortableUI.hpp's coordinate contract): "every node's
// bounds are relative to its parent's origin," and a backend's rendered
// position is that node's own bounds "folded over the accumulated origins of
// its ancestor chain" (PortableUI.hpp:44-46; the JUCE backend's own fold is
// `PortableJuceBackend.hpp:737-753`'s `resolve()`). The rewritten
// geometry tests below compare nodes that do not share an immediate parent
// (e.g. an encoder cell several containers deep vs. the scope, or two
// encoder cells in different grid rows), so they need the SAME fold this
// helper performs by walking each node's parent chain (found by scanning
// every node's `children` list, since `Node` carries no parent pointer of
// its own) and summing each ancestor's own local offset.
synth::ui::Bounds AbsoluteBounds(const synth::ui::NodeTree& tree, const std::string& id) {
    std::map<std::string, std::string> parentOf;
    for (const synth::ui::Node& node : tree.nodes) {
        for (const synth::ui::NodeId& child : node.children) {
            parentOf[child.value] = node.id.value;
        }
    }
    const synth::ui::Node* node = FindNodeById(tree, id);
    if (node == nullptr) {
        return {};
    }
    synth::ui::Bounds bounds = node->bounds;
    std::string current = id;
    for (;;) {
        const auto found = parentOf.find(current);
        if (found == parentOf.end()) {
            break;
        }
        const synth::ui::Node* parent = FindNodeById(tree, found->second);
        if (parent == nullptr) {
            break;
        }
        bounds.x += parent->bounds.x;
        bounds.y += parent->bounds.y;
        current = found->second;
    }
    return bounds;
}

std::optional<std::size_t> FindNodeIndexById(const synth::ui::NodeTree& tree, const std::string& id) {
    for (std::size_t ix = 0; ix < tree.nodes.size(); ++ix) {
        if (tree.nodes[ix].id.value == id) {
            return ix;
        }
    }
    return std::nullopt;
}

// --- layout bounds, no overlap, cell containment -----------------------
//
// This replaced `FroggersPageLayout`'s hand-computed pixel `Bounds` (ContentArea/
// ScopeArea/GridArea, `FroggersEncoderGridLayout::BoundsForIndex`) with a
// declarative grid resolved by Sheaf's own layout engine. Every test below
// that used to call those functions directly now builds the REAL tree
// through a bare `FroggersUiSurface` (same convention `BuildBraid4TreeAt`
// uses in `External/Sheaf/projects/synth/tests/portable_ui_tests.cpp`) and
// reads the RESOLVED node bounds instead -- each test still pins its
// ORIGINAL property, just rewritten against the new mechanism instead of
// deleted.

// A context-free surface build at an arbitrary size: the layout claims below
// are about the resolver, not about any particular engine/parameter state
// (mirrors `BuildBraid4TreeAt`, portable_ui_tests.cpp:476-485).
synth::ui::NodeTree BuildFroggersTreeAt(float width, float height) {
    synth::RuntimeConfig config = synth_froggers::FroggersApp::Config();
    config.uiWidth = static_cast<int>(width);
    config.uiHeight = static_cast<int>(height);
    synth::AppContext context;
    context.config = &config;
    synth_froggers::FroggersUiSurface surface;
    surface.Attach(&context, nullptr);
    return surface.BuildTree();
}

synth::ui::NodeTree BuildFroggersTreeAtDefaultSize() {
    return BuildFroggersTreeAt(synth_froggers::FroggersPageLayout::kDefaultWidth,
                               synth_froggers::FroggersPageLayout::kDefaultHeight);
}

// try/catch around a resolve, exactly the pattern
// `tests/portable_ui_tests.cpp:1558-1568` and
// `tests/braid4_system_tests.cpp:476-485` use -- own local implementation
// since those files live under the read-only External/Sheaf submodule.
std::string FroggersResolutionDiagnostic(const std::function<void()>& build) {
    try {
        build();
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

TEST_CASE(root_and_content_bounds_match_default_config_size) {
    const synth::ui::Bounds root = synth_froggers::FroggersPageLayout::RootBounds(nullptr);
    REQUIRE_TRUE(root.width == synth_froggers::FroggersPageLayout::kDefaultWidth);
    REQUIRE_TRUE(root.height == synth_froggers::FroggersPageLayout::kDefaultHeight);

    // `ContentArea()` (a computed pixel Bounds) is gone
    // along with the auto-flow model it served -- what survives is the
    // property it existed to guarantee, that real content is inset from the
    // window edge by a nonzero margin. The left/right blocks (children of
    // the outer split Row, `FroggersNodeIds::kLayoutRoot`, whose own
    // `padding` IS `FroggersPageLayout::kMargin`) are that content now; their
    // ABSOLUTE bounds (AbsoluteBounds() above -- a resolved node's `bounds`
    // are parent-relative, not screen coordinates) must sit `kMargin` inside
    // the window on every edge they touch.
    const synth::ui::NodeTree tree = BuildFroggersTreeAtDefaultSize();
    const synth::ui::Bounds left = AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::kLeftBlock);
    const synth::ui::Bounds right = AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::kRightBlock);
    REQUIRE_TRUE(left.width > 0.0f && left.height > 0.0f);
    REQUIRE_TRUE(right.width > 0.0f && right.height > 0.0f);

    constexpr float kTolerance = 0.01f;
    REQUIRE_TRUE(std::fabs(left.x - synth_froggers::FroggersPageLayout::kMargin) < kTolerance);
    REQUIRE_TRUE(std::fabs(left.y - synth_froggers::FroggersPageLayout::kMargin) < kTolerance);
    REQUIRE_TRUE(std::fabs((root.width - (right.x + right.width)) - synth_froggers::FroggersPageLayout::kMargin) <
                 kTolerance);
    REQUIRE_TRUE(std::fabs((root.height - (left.y + left.height)) - synth_froggers::FroggersPageLayout::kMargin) <
                 kTolerance);
}

TEST_CASE(scope_and_grid_regions_do_not_overlap_at_target_window_size) {
    // the invariant survives verbatim (scope and every
    // encoder cell occupy disjoint regions, all inside the window); the
    // mechanism moves from computed pixel rectangles to the RESOLVED,
    // absolute-folded node bounds of the real tree.
    const synth::ui::NodeTree tree = BuildFroggersTreeAtDefaultSize();
    const synth::ui::Bounds root = synth_froggers::FroggersPageLayout::RootBounds(nullptr);
    const synth::ui::Bounds scope = AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::kVcoScope);
    REQUIRE_TRUE(scope.width > 0.0f && scope.height > 0.0f);
    REQUIRE_TRUE(FullyInside(scope, root));

    for (std::size_t ix = 0; ix < synth_froggers::FroggersEncoderGridLayout::kEncoderCount; ++ix) {
        const synth::ui::Bounds cell = AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::Encoder(ix));
        REQUIRE_TRUE(cell.width > 0.0f && cell.height > 0.0f);
        REQUIRE_TRUE(FullyInside(cell, root));
        REQUIRE_TRUE(!Overlaps(scope, cell));
    }
}

// (The operator's strongest complaint, verbatim: "it is taller than it is wide, which is to
// put it mildly, fucking stupid for visual UI. it should be at most a third
// of its current size."). pins the two hard requirements
// directly against the RESOLVED scope node -- wider than tall, and at most
// 1/3 of the OLD panel's area (340 wide x 528 tall -- the portrait column
// this replaced, long before this file's own `ScopeArea()` existed; the
// baseline is `FroggersPageLayout::kScopeWidth`, this file's one surviving
// definition of that historical width, and the old full content height,
// preserved here as a literal since the struct that computed it is gone). A
// regression back to a portrait panel, or one that merely shrinks without
// changing its aspect ratio, fails this test.
TEST_CASE(scope_area_is_wider_than_tall_and_at_most_a_third_of_its_old_area) {
    const synth::ui::NodeTree tree = BuildFroggersTreeAtDefaultSize();
    const synth::ui::Bounds scope = AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::kVcoScope);
    REQUIRE_TRUE(scope.width > 0.0f && scope.height > 0.0f);
    REQUIRE_TRUE(scope.width > scope.height);

    constexpr float kOldFullContentHeight = 528.0f;  // historical: the pre-rewrite content area's height.
    const float oldScopeArea = synth_froggers::FroggersPageLayout::kScopeWidth * kOldFullContentHeight;
    const float newScopeArea = scope.width * scope.height;
    REQUIRE_TRUE(newScopeArea <= oldScopeArea / 3.0f);
}

// POSITION REGRESSION GUARD (2026-07-29). Nothing pinned the scope's
// LOCATION, so a change asked to shrink it also moved it -- from a left-hand
// column to a full-width band across the top -- and every existing assertion
// still passed. Operator: "WHEN DID I ASK FOR YOU TO CHANGE THE LOCATION OF
// IT? i said just the height should change." the single
// most important guard in this file -- still pins scope-left/grid-right/
// grid-full-height, now against the resolved left/right block nodes.
TEST_CASE(scope_sits_in_a_left_column_with_the_grid_to_its_right) {
    const synth::ui::NodeTree tree = BuildFroggersTreeAtDefaultSize();
    const synth::ui::Bounds root = synth_froggers::FroggersPageLayout::RootBounds(nullptr);
    const synth::ui::Bounds left = AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::kLeftBlock);
    const synth::ui::Bounds right = AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::kRightBlock);
    const synth::ui::Bounds scope = AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::kVcoScope);
    REQUIRE_TRUE(left.width > 0.0f && right.width > 0.0f && scope.width > 0.0f);

    // The scope is a COLUMN, not a full-width band.
    REQUIRE_TRUE(scope.width < root.width);

    // The grid (right block) starts to the right of the left block's right
    // edge, and the left block does not start to the right of the right
    // block (i.e. they are not stacked).
    REQUIRE_TRUE(right.x >= left.x + left.width);
    REQUIRE_TRUE(right.x > left.x);

    // The grid (right block) spans the full content height -- it is BESIDE
    // the left column, not pushed down by anything in it.
    REQUIRE_TRUE(right.height >= left.height - 0.5f);

    // The scope itself sits inside the LEFT block, not the right.
    REQUIRE_TRUE(FullyInside(scope, left));
}

TEST_CASE(every_encoder_cell_lies_fully_inside_the_grid_region) {
    // "grid region" is now the right block (bank tabs,
    // the 16-slot grid, and Randomize share it per the CELL MAP), a superset
    // of the old grid-only region but still a meaningful containment check;
    // the harder property -- no two cells overlap -- is unchanged and still
    // checked pairwise.
    const synth::ui::NodeTree tree = BuildFroggersTreeAtDefaultSize();
    const synth::ui::Bounds right = AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::kRightBlock);
    REQUIRE_TRUE(right.width > 0.0f);

    std::vector<synth::ui::Bounds> cells;
    cells.reserve(synth_froggers::FroggersEncoderGridLayout::kEncoderCount);
    for (std::size_t ix = 0; ix < synth_froggers::FroggersEncoderGridLayout::kEncoderCount; ++ix) {
        const synth::ui::Bounds cell = AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::Encoder(ix));
        REQUIRE_TRUE(cell.width > 0.0f && cell.height > 0.0f);
        REQUIRE_TRUE(FullyInside(cell, right));
        cells.push_back(cell);
    }

    for (std::size_t a = 0; a < cells.size(); ++a) {
        for (std::size_t b = a + 1; b < cells.size(); ++b) {
            REQUIRE_TRUE(!Overlaps(cells[a], cells[b]));
        }
    }
}

// --- fit guards, replacing the deleted height cross-check --------------
//
// `declared_ui_height_matches_the_derived_required_extent` is gone along
// with its cross-check: nothing computes a required height any more --
// `config.uiHeight` is a plain initial window size kept equal to
// `FroggersPageLayout::kDefaultHeight` by hand, and the layout resolves
// against whatever root extent it is given
// (`FroggersPageLayout::RootBounds()`) -- and per its own finding that test
// could never actually fail even when its assumptions were already wrong
// (`FroggersAutoFlowedChromeModel::FlowedControls()` hardcoded a control
// list it never checked against the real tree). Its FUNCTION --
// proving the surface fits -- is taken over by these three, a strictly
// stronger guarantee: they resolve the real declarative grid and let
// `RequireContainerHoldsItsChildren` (PortableUILayout.hpp:267-316) itself
// report any overflow, at the three sizes the preflight gate pinned
// (900x632 default, 640x480 small, 1440x900 large) -- not an invented check.

// The default size literal here tracks
// `FroggersPageLayout::kDefaultHeight`/`FroggersAppCore::Config().uiHeight`
// BY HAND (a plain literal, same convention those two use with each other --
// see their own comments for why no cross-check test exists for that pair).
// 632 -> 712 with this task's window growth; the small/large cases below are
// deliberately untouched (arbitrary probe sizes, not tied to the default).
TEST_CASE(surface_resolves_without_overflow_at_the_default_window_size) {
    const std::string diagnostic = FroggersResolutionDiagnostic([] {
        BuildFroggersTreeAt(900.0f, 712.0f);
    });
    REQUIRE_TRUE(diagnostic.empty());
}

TEST_CASE(surface_resolves_without_overflow_at_a_small_window_size) {
    const std::string diagnostic = FroggersResolutionDiagnostic([] {
        BuildFroggersTreeAt(640.0f, 480.0f);
    });
    REQUIRE_TRUE(diagnostic.empty());
}

TEST_CASE(surface_resolves_without_overflow_at_a_large_window_size) {
    const std::string diagnostic = FroggersResolutionDiagnostic([] {
        BuildFroggersTreeAt(1440.0f, 900.0f);
    });
    REQUIRE_TRUE(diagnostic.empty());
}

// --- the two labelled sliders resolve to the same width ----------------

// The guard for that defect. Operator, on the rebuilt layout: "the
// scene slider is too wide and the bpm slider is too narrow. grid design
// fail." Neither slider declared a width at all, so each inherited whatever
// its container implied -- scene-blend filled a Column's cross axis while BPM
// split a Row's main axis with its label. Both now come from the single
// `kSliderWidthFraction` read once in `AppendLabelledSlider()`.
//
// This asserts the two sliders against EACH OTHER, never against a pixel
// literal. A hardcoded expected width would be two numbers kept in agreement
// by hand -- the same defect `uiHeight`'s hand-sync with `kDefaultHeight`
// (FroggersUiSurface.hpp) already carries, and it would keep passing if both
// sliders drifted together. Comparing them to each other is what actually
// pins "equal by construction."
//
// Checked at three window sizes because the width is a FRACTION of the left
// block: equality has to survive the block resizing, which is the property a
// fraction buys over a pixel count.
TEST_CASE(scene_blend_and_bpm_sliders_resolve_to_the_same_width) {
    const std::array<std::pair<float, float>, 3> sizes{{
        {synth_froggers::FroggersPageLayout::kDefaultWidth,
         synth_froggers::FroggersPageLayout::kDefaultHeight},
        {640.0f, 480.0f},
        {1440.0f, 900.0f},
    }};

    for (const auto& [width, height] : sizes) {
        const synth::ui::NodeTree tree = BuildFroggersTreeAt(width, height);
        const synth::ui::Bounds sceneBlend =
            AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::kSceneBlend);
        const synth::ui::Bounds bpm = AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::kBpm);

        REQUIRE_TRUE(sceneBlend.width > 0.0f);
        REQUIRE_TRUE(bpm.width > 0.0f);
        REQUIRE_TRUE(std::fabs(sceneBlend.width - bpm.width) < 0.5f);
        // Both are left-aligned in the same column, so equal width with equal
        // x means they genuinely occupy the same horizontal span -- not two
        // equal-width sliders sitting in different places.
        REQUIRE_TRUE(std::fabs(sceneBlend.x - bpm.x) < 0.5f);
    }
}

// --- bank buttons are Button nodes again --------------------------

// At the pinned Sheaf version,
// Draw/DrawInteractive nodes dispatch only on double-click
// (RetainedDrawComponent, PortableJuceBackend.hpp:549-555 -- no plain-click
// path), which cost single-click bank switching when bank buttons were
// briefly Draw nodes. Reverted back to plain
// `Button` nodes: this replaces the former
// bank_selection_renders_as_true_color_inversion_with_no_marker_character
// (which asserted Draw-node fill/text colour inversion, no longer
// applicable) with checks matching the operator's brief -- Button kind,
// action on `node.action` (not `doubleClickAction`), `node.selected` on the
// active bank only, exactly one bank selected, no marker character.
TEST_CASE(bank_buttons_are_button_kind_with_selected_flag_and_no_marker_character) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("bank_selection_button_kind"));
    rig.RunBlocks(4);
    rig.UIState();  // forces a synchronous publish (bank selection is throttled per Engine.hpp)

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    const synth::ui::NodeTree tree = surface.BuildTree();
    const auto& layouts = synth_froggers::FroggersBankLayouts();

    auto checkAllBanksAndReturnSelectedIx = [&](const synth::ui::NodeTree& checkedTree) -> std::size_t {
        std::size_t selectedCount = 0;
        std::size_t selectedIx = synth_froggers::kFroggersBankCount;
        for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
            const synth::ui::Node* node =
                FindNodeById(checkedTree, synth_froggers::FroggersNodeIds::BankButton(bankIx));
            REQUIRE_TRUE(node != nullptr);
            REQUIRE_TRUE(node->kind == synth::ui::NodeKind::Button);
            REQUIRE_TRUE(node->label == layouts[bankIx].name);
            REQUIRE_TRUE(node->label.find('*') == std::string::npos);
            // Button nodes carry their action directly on `node.action`
            // (Builder::Button, PortableUIBuilders.hpp:300-308) -- not
            // `doubleClickAction`, which only Draw/DrawInteractive nodes use.
            REQUIRE_TRUE(node->action.has_value());
            REQUIRE_TRUE(node->action->name == synth_froggers::FroggersActions::kBankSelect);
            REQUIRE_TRUE(node->action->value == std::to_string(bankIx));
            REQUIRE_TRUE(!node->doubleClickAction.has_value());
            if (node->selected) {
                ++selectedCount;
                selectedIx = bankIx;
            }
        }
        REQUIRE_TRUE(selectedCount == 1);
        return selectedIx;
    };

    // Bank 0 is the default active bank (FroggersParameterModel::Init()'s
    // own `slot_->SelectBank(banks_[0])`).
    REQUIRE_TRUE(checkAllBanksAndReturnSelectedIx(tree) == 0);

    // Selecting a different bank moves `node.selected` -- still exactly one
    // bank selected, and it is now bank 1.
    surface.DispatchAction(synth::ui::Action::WithValue(synth_froggers::FroggersActions::kBankSelect, "1"));
    rig.RunBlocks(4);
    rig.UIState();  // forces a synchronous publish
    const synth::ui::NodeTree afterTree = surface.BuildTree();
    REQUIRE_TRUE(checkAllBanksAndReturnSelectedIx(afterTree) == 1);
}

// --- 10.4/10.5: drill-in swaps the grid in place ----------------------------

TEST_CASE(drill_in_swaps_grid_in_place_scope_and_chrome_stay_put) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("drill_in_swap"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    const synth::ui::NodeTree before = surface.BuildTree();
    const std::set<std::string> idsBefore = NodeIds(before);

    // Region/chrome ids that must survive drill-in unchanged (10.4's "the
    // surrounding scope band and chrome stay put").
    REQUIRE_TRUE(idsBefore.count(synth_froggers::FroggersNodeIds::kRoot) == 1);
    REQUIRE_TRUE(idsBefore.count(synth_froggers::FroggersNodeIds::kVcoScope) == 1);
    REQUIRE_TRUE(idsBefore.count(synth_froggers::FroggersNodeIds::kRandomizeAll) == 1);
    REQUIRE_TRUE(idsBefore.count(synth_froggers::FroggersNodeIds::kRandomizePage) == 1);
    REQUIRE_TRUE(idsBefore.count(synth_froggers::FroggersNodeIds::Encoder(0)) == 1);

    REQUIRE_TRUE(!rig.UIState().slots[0].showingModulationView.load());

    // Press encoder 0 (Audio bank's VCO1 pitch, a page-level parameter) via
    // the surface's own action routing -- exercises the pending-atomic
    // bridge (FroggersAppCore::RequestEncoderPress -> ProcessFrame() ->
    // FroggersModulationDrillIn::PressEncoder), not a direct Bank call.
    surface.DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kEncoderPress, "0"));
    rig.RunBlocks(4);

    REQUIRE_TRUE(rig.UIState().slots[0].showingModulationView.load());

    const synth::ui::NodeTree after = surface.BuildTree();
    const std::set<std::string> idsAfter = NodeIds(after);

    // Same region/chrome ids present -- no separate window/page, no new
    // root, same node-id surface for everything the grid does not own.
    REQUIRE_TRUE(idsAfter.count(synth_froggers::FroggersNodeIds::kRoot) == 1);
    REQUIRE_TRUE(idsAfter.count(synth_froggers::FroggersNodeIds::kVcoScope) == 1);
    REQUIRE_TRUE(idsAfter.count(synth_froggers::FroggersNodeIds::kRandomizeAll) == 1);
    REQUIRE_TRUE(idsAfter.count(synth_froggers::FroggersNodeIds::kRandomizePage) == 1);
    // The Target/Back cell for the just-opened L1 view is the SAME grid
    // cell id (encoder 0 is now the Target/Back cell, since Bank::
    // OpenModulationView's cell layout puts the parameter itself last --
    // but this app's own drill-in always re-enters via whichever physical
    // encoder id was pressed's *position*, not a new id space). The grid
    // still renders ids 0-15 in the same region either way.
    REQUIRE_TRUE(idsAfter.count(synth_froggers::FroggersNodeIds::Encoder(0)) == 1);
    REQUIRE_TRUE(idsAfter.count(synth_froggers::FroggersNodeIds::Encoder(15)) == 1);

    // Back out: pressing the Target/Back cell (index 15 in a drilled view)
    // restores the parameter grid (10.4's "Return restores the parameter
    // grid" scenario) -- native Bank::Deselect() semantics via the same
    // PressEncoder path.
    surface.DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kEncoderPress, "15"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(!rig.UIState().slots[0].showingModulationView.load());
}

// Operator override 2026-07-29: "clicking on the page bank
// for the page we are on is the way the user should always be able to get to
// that page, even when they are in a modulation drilldown for a parameter on
// that page." `FroggersAppCore::ProcessFrame`'s RequestBankSelect handling
// used to guard the whole branch on `bankRequest != activeBankIx_`, making a
// same-bank click while drilled in a complete no-op. Fixed by resetting the
// drill-in (Back()-until-zero) when the requested bank equals the active
// bank AND the drill-in level is above 0, while still doing nothing at all
// when the requested bank equals the active bank and level is ALREADY 0 (the
// pre-existing, still-desired no-op).
TEST_CASE(clicking_the_active_bank_while_drilled_in_exits_to_the_top_level_grid) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("bank_select_exits_drilldown"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    const std::size_t activeBank = rig.Application().ActiveBankIndex();
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 0);

    // Drill to level 2 on the active bank via the surface's own action
    // routing -- same bridge (kEncoderPress -> ProcessFrame() ->
    // FroggersModulationDrillIn::PressEncoder) as the drill-in-swap test
    // above.
    surface.DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kEncoderPress, "0"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 1);
    surface.DispatchAction(synth::ui::Action::WithValue(
        synth_froggers::FroggersActions::kEncoderPress,
        std::to_string(static_cast<std::size_t>(synth_froggers::kModSlotVco1Audio))));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 2);

    // Click the SAME (already-active) bank button -- must exit the
    // drilldown back to the top-level parameter grid, not no-op.
    surface.DispatchAction(synth::ui::Action::WithValue(synth_froggers::FroggersActions::kBankSelect,
                                                          std::to_string(activeBank)));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 0);
    REQUIRE_TRUE(rig.Application().ActiveBankIndex() == activeBank);  // still the same bank, just exited

    // Existing no-op MUST be preserved: clicking the same bank while ALREADY
    // at level 0 must not disturb anything. There is no direct "was
    // drillIn_ reconstructed" observable, so this checks every state this
    // request path can touch: activeBankIx_, the drill-in's level, and its
    // selected parameter (null at level 0 either way) all stay exactly as
    // they were.
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().BankRef().SelectedParameter() == nullptr);
    surface.DispatchAction(synth::ui::Action::WithValue(synth_froggers::FroggersActions::kBankSelect,
                                                          std::to_string(activeBank)));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveBankIndex() == activeBank);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 0);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().BankRef().SelectedParameter() == nullptr);
}

// The operator's request, verbatim: "when we are in modulation drilldown levels ... headers
// ... 'Modulation Level 1' then 2 then 3." Level 0 shows no indicator; each
// deeper level shows the matching text, sourced from
// FroggersModulationDrillIn::Level() (never a hardcoded per-level string).
// Drills to level 3 with the SAME encoder-id sequence
// clicking_the_active_bank_while_drilled_in_exits_to_the_top_level_grid uses
// for level 1->2 (kEncoderPress "0" then kModSlotVco1Audio), extended one
// more press (kModSlotVco2Audio) to reach level 3 -- the identical sequence
// fourth_level_drill_in_is_refused (FroggersModulationTests.cpp) uses to
// reach the same depth, so this is a proven-valid path, not a guess.
//
// Operator regression report: "i still don't see a header
// label counting the drilldown levels") extended this same test rather than
// adding a parallel one: once drilled to level 1, also asserts the resolved
// TextStyle is an explicit, deliberate choice (not TextStyle{}'s own
// default), and that an opaque Fill (the backing band) immediately precedes
// the Text command in the carrying node's own draw list. Both properties
// carry forward unchanged below.
//
// RELOCATED TWICE since (see AppendScopeCell's and AppendModulationHeaderRow's
// own comments, FroggersUiSurface.hpp, for the full investigation): first to
// kVcoScope (its original placement -- unreachable, the operator never
// looks there while drilled in), then to the Target/Back grid cell
// (as a "BACK L<N>" badge) -- reachable, but
// rejected on SUBSTANCE, not visibility: "i don't know why you thought i
// wanted the header to be 'Back' and by the back button... nothing needs to
// be labeled 'back' there, that implementation sucks." STEP 1 (2026-08-09)
// moves it a second time, to its own dedicated, non-interactive header row
// (`FroggersNodeIds::kModulationHeader`). Same content/styling
// properties, same test identity (renamed to match), new carrying node --
// plus a new removal guard proving the Target/Back cell no longer carries
// this text either.
TEST_CASE(modulation_header_shown_only_while_drilled_in_and_matches_the_level) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("drill_level_header"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    const std::string headerId = synth_froggers::FroggersNodeIds::kModulationHeader;
    // frogg3rs-bank-carousel-arrows re-anchor: kModulationHeader is now a Row
    // wrapper (arrows at level 0, this title at level > 0), so the fill+text
    // commands this test pins live on the title CHILD, not on headerId
    // itself any more -- see kModulationHeaderTitle's own comment,
    // FroggersUiSurface.hpp. headerId itself is still used below for the
    // level-0 "row draws nothing of its own" check and is untouched by the
    // structural change -- its outer geometry is unchanged.
    const std::string titleId = synth_froggers::FroggersNodeIds::kModulationHeaderTitle;
    // Not the carrying node any more, but still the specific site the
    // rejected earlier attempt mislabelled "BACK" -- this test's own removal
    // guard below needs it.
    const std::string backId = synth_froggers::FroggersNodeIds::Encoder(
        synth_froggers::FroggersEncoderGridLayout::kEncoderCount - 1);

    // Positive control: print the level actually observed so an
    // "absent at level 0" pass can never be mistaken for a rig that simply
    // never drilled -- the level itself comes from the same real getter
    // (ActiveDrillIn().Level()) the levels-1/2/3 checks below rely on.
    const std::size_t levelBeforeDrilling = rig.Application().ActiveDrillIn().Level();
    std::cout << "[OBSERVED] drill level before any encoder press: " << levelBeforeDrilling << "\n";
    REQUIRE_TRUE(levelBeforeDrilling == 0);
    {
        // At level 0 the row is RESERVED (present, so sibling geometry
        // never jumps when entering/exiting a drilldown -- see
        // AppendModulationHeaderRow's own comment) but the ROW ITSELF must
        // carry no draw commands of its own (it is a container; its
        // level-0 children are the arrow pair, covered by
        // bank_carousel_arrows_are_centered_in_the_modulation_header_band_at_top_level
        // above).
        const synth::ui::NodeTree undrilledTree = surface.BuildTree();
        const synth::ui::Node* headerAtLevel0 = FindNodeById(undrilledTree, headerId);
        REQUIRE_TRUE(headerAtLevel0 != nullptr);
        // Pin the header row's level-0 children instead of a container tautology:
        // exactly [left spacer, prev arrow, next arrow, right spacer], no title.
        REQUIRE_TRUE(headerAtLevel0->children.size() == 4);
        REQUIRE_TRUE(headerAtLevel0->children[0].value == "froggers.layout.right.header.spacer.left");
        REQUIRE_TRUE(headerAtLevel0->children[1].value == synth_froggers::FroggersNodeIds::kBankPrevArrow);
        REQUIRE_TRUE(headerAtLevel0->children[2].value == synth_froggers::FroggersNodeIds::kBankNextArrow);
        REQUIRE_TRUE(headerAtLevel0->children[3].value == "froggers.layout.right.header.spacer.right");
        // kModulationHeaderTitle must not be a level-0 child (it exists only while drilled).
        for (const synth::ui::NodeId& child : headerAtLevel0->children) {
            REQUIRE_TRUE(child.value != titleId);
        }
    }

    surface.DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kEncoderPress, "0"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 1);
    const synth::ui::NodeTree drilledTree = surface.BuildTree();
    REQUIRE_TRUE(DrillBadgeText(drilledTree, titleId).value_or("") == "Modulation Level 1");
    // Removal guards: neither kVcoScope (the first rejected placement) nor
    // the Target/Back cell (the second) may carry this text any more. The
    // Target/Back check is a substring search, not "no Text command at
    // all" -- that cell is a real, connected parameter and can legitimately
    // carry unrelated modulator-badge text (e.g. "M1"; see
    // DrillBadgeTextIndex's own comment above).
    REQUIRE_TRUE(!DrillBadgeText(drilledTree, synth_froggers::FroggersNodeIds::kVcoScope).has_value());
    REQUIRE_TRUE(!NodeDrawnTextContains(drilledTree, backId, "BACK"));

    // Styling assertions, carried over to the new location -- the text
    // must not be a bare, default-styled DrawCommand::Text. The drawing
    // branch (AppendModulationHeaderRow, FroggersUiSurface.hpp) is
    // identical at every level > 0 (only the interpolated number changes,
    // already covered by the three "== Modulation Level <N>" checks in this
    // test), so checking the style and band once here is not a guess about
    // levels 2/3 -- it is the same code path they also execute.
    const std::optional<synth::ui::DrawCommand> headerCommand = DrillBadgeTextCommand(drilledTree, titleId);
    REQUIRE_TRUE(headerCommand.has_value());
    // Different from TextStyle{}'s own 14px default (PortableUI.hpp) -- the
    // concrete, testable form of "an explicit size, not a default."
    REQUIRE_TRUE(headerCommand->textStyle.size != synth::ui::TextStyle{}.size);

    const synth::ui::Node* headerNode = FindNodeById(drilledTree, titleId);
    REQUIRE_TRUE(headerNode != nullptr);
    // Exactly two commands (the backing Fill then the Text) -- a precise,
    // non-vacuous form of "nothing else is drawn on this row and nothing
    // follows the text," stronger than looping over a range that happens to
    // be empty (this row carries no badges -- it is not an encoder cell).
    REQUIRE_TRUE(headerNode->drawCommands.size() == 2);
    const std::optional<std::size_t> headerIndex = DrillBadgeTextIndex(drilledTree, titleId);
    REQUIRE_TRUE(headerIndex.has_value());
    REQUIRE_TRUE(*headerIndex == 1);  // the Text command is the LAST of the two
    const synth::ui::DrawCommand& bandBeforeText = headerNode->drawCommands[*headerIndex - 1];
    REQUIRE_TRUE(bandBeforeText.kind == synth::ui::DrawCommand::Kind::Fill &&
                 bandBeforeText.bounds.height == headerCommand->bounds.height);

    surface.DispatchAction(synth::ui::Action::WithValue(
        synth_froggers::FroggersActions::kEncoderPress,
        std::to_string(static_cast<std::size_t>(synth_froggers::kModSlotVco1Audio))));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 2);
    REQUIRE_TRUE(DrillBadgeText(surface.BuildTree(), titleId).value_or("") == "Modulation Level 2");

    surface.DispatchAction(synth::ui::Action::WithValue(
        synth_froggers::FroggersActions::kEncoderPress,
        std::to_string(static_cast<std::size_t>(synth_froggers::kModSlotVco2Audio))));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 3);
    REQUIRE_TRUE(DrillBadgeText(surface.BuildTree(), titleId).value_or("") == "Modulation Level 3");
}

// STEP 1 (operator, 2026-08-09, fourth session on the same complaint -- see
// AppendModulationHeaderRow's own comment, FroggersUiSurface.hpp, for the
// full history: the header first sat on kVcoScope, unreachable; a later attempt
// relocation-1 moved it to the Target/Back cell as a "BACK L<N>" badge,
// which the operator rejected on SUBSTANCE, not visibility -- "i don't know
// why you thought i wanted the header to be 'Back' and by the back button,
// instead of a HEADER above all the modulation parameters, below the bank
// button row?? ... nothing needs to be labeled 'back' there"). Their spec is
// unambiguous and geometric: a header BAR spanning the grid's width, BELOW
// the bank tabs row, ABOVE the first row of parameter cells. This test
// computes (not eyeballs) exactly that claim against the real resolved
// tree, replacing the previous containment-only guard
// (drill_back_badge_resolves_inside_the_grid_region_the_operator_actually_sees,
// which only proved the old badge was somewhere inside the right block --
// true of the rejected Target/Back placement too, and therefore
// insufficient on its own to distinguish an accepted fix from a rejected
// one).
//
// Three positive controls, so this cannot pass against
// coincidentally-empty geometry: the bank tabs row and a populated
// parameter cell (kModSlotRandomSh6, "Random S&H 6" -- the one modulation
// source registered `/*connected=*/true` unconditionally,
// FroggersModulation.hpp:549-550, so it is guaranteed to be a live,
// rendering cell in the drilled-in grid with no patch setup needed) both
// resolve to real, populated, in-region geometry, AND the header itself is
// checked for its actual "Modulation Level 1" text, not merely for having
// nonzero bounds.
TEST_CASE(modulation_header_sits_below_bank_row_and_above_parameter_cells) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("drill_header_position"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    surface.DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kEncoderPress, "0"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 1);  // genuinely drilled in, not assumed

    const synth::ui::NodeTree tree = surface.BuildTree();
    const synth::ui::Bounds gridRegion = AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::kRightBlock);
    REQUIRE_TRUE(gridRegion.width > 0.0f && gridRegion.height > 0.0f);

    // Positive control 1: the bank tabs row is real, in-region geometry.
    const synth::ui::Bounds bankRow = AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::kBankTabsRow);
    REQUIRE_TRUE(bankRow.width > 0.0f && bankRow.height > 0.0f);
    REQUIRE_TRUE(FullyInside(bankRow, gridRegion));

    // Positive control 2: a cell the operator demonstrably DOES see while
    // drilled in. Its own nonzero command count is real, load-bearing
    // evidence, not an assumption -- AppendEncoderCell emits zero ring
    // commands for a hidden/disconnected cell (BuildEncoderDrawCommands
    // returns {} when !state.connected, EncoderDraw.hpp:653-656).
    const std::string sourceId = synth_froggers::FroggersNodeIds::Encoder(
        static_cast<std::size_t>(synth_froggers::kModSlotRandomSh6));
    const synth::ui::Node* sourceRing = FindNodeById(tree, sourceId);
    REQUIRE_TRUE(sourceRing != nullptr);
    REQUIRE_TRUE(sourceRing->drawCommands.size() > 0);
    const synth::ui::Bounds sourceBounds = AbsoluteBounds(tree, sourceId);
    REQUIRE_TRUE(FullyInside(sourceBounds, gridRegion));

    // The header itself, folded to ABSOLUTE (screen) coordinates. Bounds are
    // still pinned by the OUTER row's own id (kModulationHeader) -- this id's
    // geometry is unchanged by the frogg3rs-bank-carousel-arrows restructure.
    // Its title TEXT, however, now
    // lives on the child kModulationHeaderTitle (the row's own draw commands
    // are empty; it is a container), so the content check below is
    // re-anchored there.
    const std::string headerId = synth_froggers::FroggersNodeIds::kModulationHeader;
    const synth::ui::Bounds header = AbsoluteBounds(tree, headerId);
    REQUIRE_TRUE(header.width > 0.0f && header.height > 0.0f);
    REQUIRE_TRUE(FullyInside(header, gridRegion));
    // Positive control 3: the region checked is a REAL, populated header,
    // not a coincidentally passing empty rectangle.
    REQUIRE_TRUE(DrillBadgeText(tree, synth_froggers::FroggersNodeIds::kModulationHeaderTitle).value_or("") ==
                 "Modulation Level 1");

    // Every parameter cell's own absolute bounds -- checked against all 16,
    // not just whichever row happens to be topmost, so "above EVERY
    // modulation-parameter cell" cannot pass by only having checked one.
    std::vector<synth::ui::Bounds> cellBounds;
    cellBounds.reserve(synth_froggers::FroggersEncoderGridLayout::kEncoderCount);
    for (std::size_t ix = 0; ix < synth_froggers::FroggersEncoderGridLayout::kEncoderCount; ++ix) {
        cellBounds.push_back(AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::Encoder(ix)));
    }

    std::cout << "[OBSERVED] gridRegion={" << gridRegion.x << "," << gridRegion.y << "," << gridRegion.width << ","
              << gridRegion.height << "} bankRow={" << bankRow.x << "," << bankRow.y << "," << bankRow.width << ","
              << bankRow.height << "} header={" << header.x << "," << header.y << "," << header.width << ","
              << header.height << "} firstParamCell={" << cellBounds.front().x << "," << cellBounds.front().y << ","
              << cellBounds.front().width << "," << cellBounds.front().height << "} sourceBounds={" << sourceBounds.x
              << "," << sourceBounds.y << "," << sourceBounds.width << "," << sourceBounds.height << "}\n";

    // The containment/position claims this whole test exists to encode,
    // computed (FullyInside, already used throughout this file for exactly
    // this kind of check), not eyeballed from a screenshot.
    constexpr float kTolerance = 0.01f;
    // BELOW the bank button row.
    REQUIRE_TRUE(header.y + kTolerance >= bankRow.y + bankRow.height);
    // ABOVE every modulation-parameter cell.
    for (const synth::ui::Bounds& cell : cellBounds) {
        REQUIRE_TRUE(header.y + header.height <= cell.y + kTolerance);
    }

    // Removal guards. The operator's actual complaint was about a "back"
    // label appearing where they were looking, not about one specific
    // pre-named node, so this checks both: scoped exactly to the task's own
    // wording (the Target/Back cell) and, strictly stronger, the whole tree.
    const std::string backId = synth_froggers::FroggersNodeIds::Encoder(
        synth_froggers::FroggersEncoderGridLayout::kEncoderCount - 1);
    REQUIRE_TRUE(!NodeDrawnTextContains(tree, backId, "BACK"));
    REQUIRE_TRUE(!AnyDrawnTextContains(tree, "BACK"));
}

// At drill
// level 0 the modulation-header band emits a centered back/forward arrow
// pair instead of drawing nothing. This is the STRUCTURE/geometry half only
// -- a separate mechanism wires the actions up to actual bank switching; this test
// pins that the pair renders, carries the right action names, and sits
// centered and fully inside the band, all independent of whatever
// HandleAction eventually does with those actions.
TEST_CASE(bank_carousel_arrows_are_centered_in_the_modulation_header_band_at_top_level) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("bank_carousel_arrows_centered"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    // Positive control: confirm we are genuinely at the top level
    // (arrows are only specified there), not assuming a fresh rig's default.
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 0);

    const synth::ui::NodeTree tree = surface.BuildTree();
    const synth::ui::Bounds band = AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::kModulationHeader);
    REQUIRE_TRUE(band.width > 0.0f && band.height > 0.0f);

    const synth::ui::Node* prevNode = FindNodeById(tree, synth_froggers::FroggersNodeIds::kBankPrevArrow);
    const synth::ui::Node* nextNode = FindNodeById(tree, synth_froggers::FroggersNodeIds::kBankNextArrow);
    REQUIRE_TRUE(prevNode != nullptr);
    REQUIRE_TRUE(nextNode != nullptr);
    // Positive control: the arrows actually draw something (plate + glyph),
    // not a coincidentally-empty pair of zero-size nodes.
    REQUIRE_TRUE(prevNode->drawCommands.size() > 0);
    REQUIRE_TRUE(nextNode->drawCommands.size() > 0);
    REQUIRE_TRUE(prevNode->action.has_value());
    REQUIRE_TRUE(prevNode->action->name == synth_froggers::FroggersActions::kBankPrevious);
    REQUIRE_TRUE(nextNode->action.has_value());
    REQUIRE_TRUE(nextNode->action->name == synth_froggers::FroggersActions::kBankNext);

    const synth::ui::Bounds prevBounds = AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::kBankPrevArrow);
    const synth::ui::Bounds nextBounds = AbsoluteBounds(tree, synth_froggers::FroggersNodeIds::kBankNextArrow);
    REQUIRE_TRUE(FullyInside(prevBounds, band));
    REQUIRE_TRUE(FullyInside(nextBounds, band));
    REQUIRE_TRUE(prevBounds.x < nextBounds.x);  // prev sits left of next

    // Pair-midpoint == band-midpoint, computed (not eyeballed), same
    // FullyInside/AbsoluteBounds idiom every other geometry test in this file
    // uses.
    constexpr float kTolerance = 0.01f;
    const float pairLeft = prevBounds.x;
    const float pairRight = nextBounds.x + nextBounds.width;
    const float pairMidpoint = (pairLeft + pairRight) * 0.5f;
    const float bandMidpoint = band.x + band.width * 0.5f;
    std::cout << "[OBSERVED] band={" << band.x << "," << band.width << "} pair={" << pairLeft << "," << pairRight
              << "} pairMid=" << pairMidpoint << " bandMid=" << bandMidpoint << "\n";
    REQUIRE_TRUE(std::fabs(pairMidpoint - bandMidpoint) <= kTolerance);
}

// The band's own outer geometry -- "outer geometry identical in both drill
// states" -- must not move by even a pixel between drill states, and the child
// structure genuinely SWITCHES (arrows only at level 0, the title only while
// drilled) rather than merely hiding one side. Complements the existing
// modulation_header_shown_only_while_drilled_in_and_matches_the_level /
// modulation_header_sits_below_bank_row_and_above_parameter_cells tests
// above, which this change re-anchors onto kModulationHeaderTitle for their
// own draw-content checks (see that constant's own comment,
// FroggersUiSurface.hpp) since kModulationHeader is no longer the leaf that
// carries the title's draw commands.
TEST_CASE(modulation_header_band_bounds_are_identical_across_drill_states_and_arrows_vanish_while_drilled) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("bank_carousel_arrows_drilled"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    const std::string headerId = synth_froggers::FroggersNodeIds::kModulationHeader;

    const synth::ui::NodeTree undrilledTree = surface.BuildTree();
    const synth::ui::Bounds bandAtLevel0 = AbsoluteBounds(undrilledTree, headerId);
    REQUIRE_TRUE(bandAtLevel0.width > 0.0f && bandAtLevel0.height > 0.0f);
    // At level 0, no title node -- the child structure switches, it does not
    // merely hide one side while both exist.
    REQUIRE_TRUE(FindNodeById(undrilledTree, synth_froggers::FroggersNodeIds::kModulationHeaderTitle) == nullptr);

    surface.DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kEncoderPress, "0"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 1);  // genuinely drilled, not assumed

    const synth::ui::NodeTree drilledTree = surface.BuildTree();
    const synth::ui::Bounds bandAtLevel1 = AbsoluteBounds(drilledTree, headerId);

    // Band bounds byte-identical (to a tight float tolerance) pre/post
    // drill-in -- the same declarative Px(26)/Weight(1) LayoutOptions resolve
    // the same way regardless of which children the row emits.
    constexpr float kTolerance = 0.01f;
    REQUIRE_TRUE(std::fabs(bandAtLevel0.x - bandAtLevel1.x) <= kTolerance);
    REQUIRE_TRUE(std::fabs(bandAtLevel0.y - bandAtLevel1.y) <= kTolerance);
    REQUIRE_TRUE(std::fabs(bandAtLevel0.width - bandAtLevel1.width) <= kTolerance);
    REQUIRE_TRUE(std::fabs(bandAtLevel0.height - bandAtLevel1.height) <= kTolerance);

    // While drilled, neither arrow node exists at all -- no hit target, no
    // draw commands, nothing for a synthetic dispatch to even find
    // (HandleAction's own gate is a second, independent layer this test
    // does not exercise).
    REQUIRE_TRUE(FindNodeById(drilledTree, synth_froggers::FroggersNodeIds::kBankPrevArrow) == nullptr);
    REQUIRE_TRUE(FindNodeById(drilledTree, synth_froggers::FroggersNodeIds::kBankNextArrow) == nullptr);

    // The title child renders today's exact fill+text commands, now under
    // its own id.
    REQUIRE_TRUE(DrillBadgeText(drilledTree, synth_froggers::FroggersNodeIds::kModulationHeaderTitle)
                     .value_or("") == "Modulation Level 1");
    const synth::ui::Node* titleNode =
        FindNodeById(drilledTree, synth_froggers::FroggersNodeIds::kModulationHeaderTitle);
    REQUIRE_TRUE(titleNode != nullptr);
    REQUIRE_TRUE(titleNode->drawCommands.size() == 2);
    REQUIRE_TRUE(titleNode->drawCommands[0].kind == synth::ui::DrawCommand::Kind::Fill);
    REQUIRE_TRUE(titleNode->drawCommands[1].kind == synth::ui::DrawCommand::Kind::Text);
}

// Wires the arrow actions through the same single selection authority
// (RequestBankSelect) the bank buttons use, so the highlight must follow
// an arrow-driven step identically to a button-driven one -- same
// checkAllBanksAndReturnSelectedIx idiom as
// bank_buttons_are_button_kind_with_selected_flag_and_no_marker_character
// above (:565-612), reused here as a local lambda since that one is scoped
// to its own TEST_CASE. Steps forward through every bank via kBankNext,
// asserting exactly one bank is ever selected and the index advances by
// exactly one per click, then continues one more click past the last bank to
// pin the 5->0 wrap this same design section requires.
TEST_CASE(bank_carousel_next_arrow_action_steps_the_active_bank_with_wrap_and_highlight_following) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("bank_carousel_next_steps_and_wraps"));
    rig.RunBlocks(4);
    rig.UIState();  // forces a synchronous publish (bank selection is throttled per Engine.hpp)

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 0);

    auto checkAllBanksAndReturnSelectedIx = [&]() -> std::size_t {
        const synth::ui::NodeTree tree = surface.BuildTree();
        std::size_t selectedCount = 0;
        std::size_t selectedIx = synth_froggers::kFroggersBankCount;
        for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
            const synth::ui::Node* node =
                FindNodeById(tree, synth_froggers::FroggersNodeIds::BankButton(bankIx));
            REQUIRE_TRUE(node != nullptr);
            if (node->selected) {
                ++selectedCount;
                selectedIx = bankIx;
            }
        }
        REQUIRE_TRUE(selectedCount == 1);
        return selectedIx;
    };

    // Bank 0 is the default active bank.
    REQUIRE_TRUE(checkAllBanksAndReturnSelectedIx() == 0);

    // Step forward through banks 1..5, one kBankNext click each -- highlight
    // follows every single step, not just the final one.
    for (std::size_t expectedIx = 1; expectedIx < synth_froggers::kFroggersBankCount; ++expectedIx) {
        surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kBankNext));
        rig.RunBlocks(4);
        rig.UIState();
        REQUIRE_TRUE(checkAllBanksAndReturnSelectedIx() == expectedIx);
    }

    // One more click from the LAST bank (5) wraps to the first (0).
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kBankNext));
    rig.RunBlocks(4);
    rig.UIState();
    REQUIRE_TRUE(checkAllBanksAndReturnSelectedIx() == 0);
}

// The back arrow's own wrap direction -- "previous from 0 -> 5" -- gets a
// separate TEST_CASE from the forward-stepping one above since it exercises
// the OTHER action name and the OTHER wrap edge -- one kBankPrevious click
// from the default bank (0) must land on the LAST bank (5), with exactly
// one bank highlighted.
TEST_CASE(bank_carousel_previous_arrow_action_wraps_from_first_bank_to_last) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("bank_carousel_previous_wraps"));
    rig.RunBlocks(4);
    rig.UIState();

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 0);
    REQUIRE_TRUE(rig.Application().ActiveBankIndex() == 0);

    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kBankPrevious));
    rig.RunBlocks(4);
    rig.UIState();

    const synth::ui::NodeTree tree = surface.BuildTree();
    std::size_t selectedCount = 0;
    std::size_t selectedIx = synth_froggers::kFroggersBankCount;
    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
        const synth::ui::Node* node = FindNodeById(tree, synth_froggers::FroggersNodeIds::BankButton(bankIx));
        REQUIRE_TRUE(node != nullptr);
        if (node->selected) {
            ++selectedCount;
            selectedIx = bankIx;
        }
    }
    REQUIRE_TRUE(selectedCount == 1);
    REQUIRE_TRUE(selectedIx == synth_froggers::kFroggersBankCount - 1);
    REQUIRE_TRUE(rig.Application().ActiveBankIndex() == synth_froggers::kFroggersBankCount - 1);
}

// MANDATORY preflight finding: pins the 2.1 drill-level-0 gate itself, not
// just the arrow nodes' absence. `HandleAction` (FroggersUiSurface.hpp)
// matches on action NAME with no node-presence check, so hiding the
// arrow nodes while drilled (already covered by
// modulation_header_band_bounds_are_identical_across_drill_states_and_arrows_vanish_while_drilled
// above) is not sufficient on its own -- a synthetic dispatch of kBankNext
// while drilled must still be rejected by HandleAction itself. Drills in via
// the real kEncoderPress gesture (same proven-valid path as the other
// drill-in tests in this file), then dispatches kBankNext directly through
// the surface (bypassing the tree/node layer entirely, exactly like a
// synthetic/malicious dispatch would), and asserts NEITHER the active bank
// NOR the drill level moved.
TEST_CASE(bank_carousel_arrow_actions_are_rejected_while_drilled_in) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("bank_carousel_arrows_no_op_while_drilled"));
    rig.RunBlocks(4);
    rig.UIState();

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 0);
    const std::size_t activeBankBeforeDrill = rig.Application().ActiveBankIndex();

    surface.DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kEncoderPress, "0"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 1);  // genuinely drilled, not assumed
    REQUIRE_TRUE(rig.Application().ActiveBankIndex() == activeBankBeforeDrill);

    // Positive control: no arrow node exists in the drilled tree at all --
    // a real click has nothing to hit.
    const synth::ui::NodeTree drilledTree = surface.BuildTree();
    REQUIRE_TRUE(FindNodeById(drilledTree, synth_froggers::FroggersNodeIds::kBankPrevArrow) == nullptr);
    REQUIRE_TRUE(FindNodeById(drilledTree, synth_froggers::FroggersNodeIds::kBankNextArrow) == nullptr);

    // The gate itself: a SYNTHETIC dispatch (surface.DispatchAction called
    // directly, not routed through any node/hit-test) of kBankNext must
    // change neither the active bank nor the drill level --
    // an ungated branch would accept this and, via the ProcessFrame drain's
    // reconstruct-drillIn_-on-bank-change behaviour
    // (FroggersAppCore.hpp:627-641), silently exit the drill.
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kBankNext));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveBankIndex() == activeBankBeforeDrill);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 1);

    // Same pin for kBankPrevious, the other action this gate must cover.
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kBankPrevious));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveBankIndex() == activeBankBeforeDrill);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 1);
}

// Operator regression report, 2026-08-07: "the drilldown back button
// still doesn't go one back, it goes all the way back") -- traced to
// FroggersModulationDrillIn::PressEncoder (FroggersModulation.hpp): every
// on-screen press, including the Target/Back cell, dispatches through
// kEncoderPress -> PressEncoder, never through .Back() directly (see that
// class's own header comment), so a test calling .Back() directly (as
// FroggersModulationTests.cpp's back_* tests do) cannot catch a regression
// in the operator's actual gesture -- that gap is why the bug shipped as
// "landed" in 49ce9af/9d0802c despite Back() itself being correct and
// already tested. This test drives the REAL gesture: dispatches
// kEncoderPress on physical encoder 15 (the Target/Back cell --
// Bank::OpenModulationView always places the selected parameter's own cell
// at physicalLayout.back(), and FullPhysicalLayout is the same fixed
// 16-wide 0-15 span at every level, so 15 is the Target/Back cell at every
// depth -- see drill_in_swaps_grid_in_place_scope_and_chrome_stay_put
// above's identical level-1 case) through the surface's own action routing,
// exactly as a real click would, and checks it pops exactly ONE level per
// press, from level 3 down to 0.
TEST_CASE(pressing_target_back_cell_through_the_surface_pops_exactly_one_drill_level_at_a_time) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("target_back_pops_one_level"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 0);

    // Drill to level 3 -- the identical proven-valid encoder-id sequence
    // modulation_header_shown_only_while_drilled_in_and_matches_the_level
    // and fourth_level_drill_in_is_refused (FroggersModulationTests.cpp) use.
    surface.DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kEncoderPress, "0"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 1);

    surface.DispatchAction(synth::ui::Action::WithValue(
        synth_froggers::FroggersActions::kEncoderPress,
        std::to_string(static_cast<std::size_t>(synth_froggers::kModSlotVco1Audio))));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 2);

    surface.DispatchAction(synth::ui::Action::WithValue(
        synth_froggers::FroggersActions::kEncoderPress,
        std::to_string(static_cast<std::size_t>(synth_froggers::kModSlotVco2Audio))));
    rig.RunBlocks(4);

    // Positive control: prove the descent actually REACHED level 3
    // before testing that a Target/Back press pops from it -- a pop that
    // starts from a level the rig never reached would prove nothing.
    const std::size_t reachedLevel = rig.Application().ActiveDrillIn().Level();
    std::cout << "[OBSERVED] drill level reached before Target/Back presses: " << reachedLevel << "\n";
    REQUIRE_TRUE(reachedLevel == 3);

    // The operator's ACTUAL gesture, level 3 -> 2: kEncoderPress on the
    // Target/Back cell, not a direct .Back() call. Before this fix,
    // PressEncoder collapsed straight to level 0 here (the reported bug).
    surface.DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kEncoderPress, "15"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 2);  // ONE level back, not zero

    // Level 2 -> 1.
    surface.DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kEncoderPress, "15"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 1);

    // Level 1 -> 0, exiting the modulation view entirely.
    surface.DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kEncoderPress, "15"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 0);
    REQUIRE_TRUE(!rig.Application().ActiveDrillIn().BankRef().ShowingModulation());
}

// --- 10.5: the encoder ring renders the fuegoized value, never rawKnobValue --

// The ring matches Braid -- processed value only.
// `EncoderDrawStateFromParameter` (EncoderDraw.hpp:306-364) reads
// `Parameter::UIState.values[]` (the UIDisplayCenter chain) and its return
// type, `synth::ui::EncoderDrawState`/`EncoderVoiceDrawState`
// (EncoderDraw.hpp:279-304), has NO rawKnobValue field at all -- there is no
// way for FroggersUiSurface's one call site (AppendEncoderGrid(), the only
// place this surface reads a Parameter::UIState) to read it even by
// accident. This test proves the runtime consequence: after fuego
// materially changes a parameter's value, the rendered ring reflects that
// changed value, not the pre-fuego scene center.
TEST_CASE(encoder_ring_renders_fuegoized_value_not_raw_scene_center) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("encoder_ring_fuego"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();

    constexpr std::size_t kCrunchyPosition = synth_froggers::kFroggersCrunchySlot;  // 15
    constexpr std::size_t kPageParamPosition = 0;                                    // Audio VCO1 pitch

    // Move Crunchy and the page parameter through the surface's own action
    // routing -- both via the generic encoder-drag message path now (the
    // chrome-band Crunchy slider/action was removed operator 2026-07-27,
    // Crunchy is
    // reachable only via grid slot 15 now, addressed exactly like any other
    // bank parameter) -- both end-to-end through FroggersUiSurface, not by
    // calling FroggersParameterModel directly.
    // A large delta (rather than a small "one tick" turn) so Crunchy lands
    // at its range boundary regardless of whatever value the default patch
    // left it at -- reliably away from a bypass-adjacent value, the same
    // guarantee the old absolute-set-to-0.8 dispatch gave for free.
    surface.DispatchAction(synth::ui::Action::WithValue(
        synth_froggers::FroggersActions::kEncoderDrag,
        synth_froggers::FormatFroggersEncoderDrag(kCrunchyPosition, 5.0f)));
    surface.DispatchAction(synth::ui::Action::WithValue(
        synth_froggers::FroggersActions::kEncoderDrag,
        synth_froggers::FormatFroggersEncoderDrag(kPageParamPosition, 0.6f)));

    // Convergence pump: enough blocks for
    // ProcessLitePhase2's UIDisplayCenter slew to catch up to the fuegoized
    // cached value (see FroggersParameterModelTests.cpp's own
    // fuego_seam_transform_reaches_cached_knob_value_matching_dsp_stack for
    // the same convention/block count).
    rig.RunBlocks(320);

    synth::Parameter& page0 = model.PageParameter(synth_froggers::FroggersBankId::Audio, kPageParamPosition);
    const float rawSceneCenter = page0.GetRaw(0);

    const synth::ui::NodeTree tree = surface.BuildTree();
    (void)tree;  // BuildTree() must not throw/crash while values are non-neutral.

    synth::ParameterManager::UIState& uiState = rig.UIState();  // forces a synchronous publish
    const synth::Parameter::UIState& cell = uiState.slots[0].cells[kPageParamPosition];
    const float renderedRingValue = synth::ui::EncoderDrawStateFromParameter(cell).voices.at(0).value;

    // The fuegoized/UI-displayed value must differ from the raw scene
    // center by more than a rounding margin -- proving fuego is
    // visible on the ring exactly the way this test's traced chain says it must
    // be, with no separate "raw ghost tick" anywhere.
    REQUIRE_TRUE(std::fabs(renderedRingValue - rawSceneCenter) > 0.02f);
}

// --- Dropped frame around the encoder (operator screenshot, ring/frame
// collision) ------------------------------------------------------------

// Operator screenshot: the parameter card's rounded-rect frame outline
// visibly crossed the encoder's own modulation ring. Diagnosed as a geometry
// collision entirely inside Sheaf's BuildEncoderDrawCommands (EncoderDraw.hpp)
// between the frame (gated by EncoderDrawState::wantsFrame, EncoderDraw.hpp:
// 293, default true) and the ring/arc layers -- see AppendEncoderCell's own
// comment (FroggersUiSurface.hpp) for the full citation. This test pins the
// RENDERED consequence -- no CELL-SPANNING StrokeRoundedRect command reaches
// the encoder node -- rather than the intermediate EncoderDrawState field,
// because that field is consumed inside the Draw callback's closure and
// never surfaces on synth::ui::Node: the draw-command list is what is
// actually observable through this test surface (NodeTree/FindNodeById),
// same convention DrillBadgeTextCommand above uses to read a node's own
// draw list.
//
// NOT "any StrokeRoundedRect": a later pass (2026-08-08), after wiring
// wantsFrame=false actually made this test inspect a populated cell (see
// the "pre-existing gap" comment below), found it still red. Root cause was
// this test, not the fix: AppendBadge (EncoderDraw.hpp:586-608, the
// modulator/gesture badge chips, e.g. "M1"/"M2") emits its own
// unconditional StrokeRoundedRect outline, entirely unrelated to
// wantsFrame, and legitimate chrome the operator never asked to remove --
// grep confirms AppendEncoderCell (FroggersUiSurface.hpp) is the
// ONLY call site into BuildEncoderDrawCommands this app's build reaches
// (braid-4/miniapp call sites are unrelated apps), and it
// sets `state.wantsFrame = false` unconditionally for every cell, before
// that one call, so the
// wantsFrame-gated rect (EncoderDraw.hpp:691-698) is provably dead code on
// this path -- what was still firing was the badge outline.
//
// Geometry tells the two apart cleanly. Traced against a live encoder(0)
// cell built by this exact test (136.333x88.333, reproducible bit-for-bit
// across repeated runs -- the default patch's modulator routing is fixed,
// not randomized): the card frame would be `{bounds.x+1, bounds.y+1,
// bounds.width-2, bounds.height-2}` off a `bounds` inset from the node
// extent by a flat 4px on each side (EncoderDraw.hpp:658-664, 691-698) --
// 126.33x78.33, i.e. 92.7%/88.7% of the cell's own extent. A badge side
// length is `radius * badgeLengthFraction`, `radius = min(bounds.width,
// bounds.height) * 0.43 * 0.72`, `badgeLengthFraction =
// 1/sqrt(1 + total*total/4)` (EncoderGeometry::GetBadgePosition,
// EncoderDraw.hpp:249-275), which is LARGEST at total==1
// (badgeLengthFraction ~= 0.894) -- an upper bound of ~27.7% of the cell's
// SMALLER dimension at any cell size, since every factor is a fixed
// fraction of a cell dimension, never a flat pixel inset. The two
// StrokeRoundedRect commands this exact cell emits (two modulator badges,
// total==2) land at 17.5866x17.5866 -- 12.9% width / 19.9% height of the
// cell -- recomputing bit-for-bit from that formula (centerX=68.1667,
// radius=24.8712, badgeLengthFraction=1/sqrt(2), badge 0 at x=50.5801,
// badge 1 at x=68.1667, both y=24.1701). A "> half the cell in both
// dimensions" threshold sits with wide margin on both sides of that gap
// (badges top out near 28%, the frame sits at 75%+ even on a
// pathologically small cell) and is what `spansCell` below checks -- so a
// badge chip can never trip `sawFrame`, while the real frame always would.
TEST_CASE(encoder_cell_never_emits_a_frame_draw_command) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("encoder_no_frame"));
    rig.RunBlocks(4);
    // Pre-existing gap found while verifying this test (it was never
    // confirmed against a real build -- see this task's own history):
    // without this call, slots[0].cellCapacity stays 0 and
    // AppendEncoderCell's `state` stays default-constructed (disconnected,
    // zero voices), so BuildEncoderDrawCommands legitimately emits NOTHING
    // and the assertions below pass/fail vacuously rather than against real
    // rendered state. Same "forces a synchronous publish" call
    // encoder_ring_renders_fuegoized_value_not_raw_scene_center already uses
    // (SynthRig::UIState(), tests/support/SynthRig.hpp) -- VERIFIED this
    // makes sawBody/sawRing observe real commands (drawCommands 0 -> 77).
    // Confirmed by rebuilding+running this one binary in isolation.
    rig.UIState();

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    const synth::ui::NodeTree tree = surface.BuildTree();

    const synth::ui::Node* encoder = FindNodeById(tree, synth_froggers::FroggersNodeIds::Encoder(0));
    REQUIRE_TRUE(encoder != nullptr);

    // A card frame spans (near) the whole cell; a badge chip is small and
    // offset -- see this test's own header comment for the traced geometry
    // and the numbers that put wide margin on both sides of this threshold.
    constexpr float kFrameSpanFraction = 0.5f;

    bool sawFrame = false;
    bool sawBadgeOutline = false;
    bool sawBody = false;
    bool sawRing = false;
    for (const synth::ui::DrawCommand& command : encoder->drawCommands) {
        if (command.kind == synth::ui::DrawCommand::Kind::StrokeRoundedRect) {
            const bool spansCell = command.bounds.width > encoder->bounds.width * kFrameSpanFraction &&
                                    command.bounds.height > encoder->bounds.height * kFrameSpanFraction;
            // A badge chip's own outline (AppendBadge, EncoderDraw.hpp:602)
            // is legitimate chrome, not the operator's complaint, and must
            // NOT be caught here -- only a cell-spanning stroke counts as
            // the card frame.
            if (spansCell) {
                sawFrame = true;
            } else {
                sawBadgeOutline = true;
            }
        }
        if (command.kind == synth::ui::DrawCommand::Kind::FillEllipse) {
            sawBody = true;
        }
        if (command.kind == synth::ui::DrawCommand::Kind::Arc) {
            sawRing = true;
        }
    }

    // Positive control: prove this cell is genuinely connected and
    // populated (body + ring commands actually present) so the "no frame"
    // assertion below cannot pass by accident against an empty command
    // list. This encoder is a top-level bank parameter (never a
    // disconnected modulation-source cell), so it was always connected and
    // populated regardless -- but see
    // disconnected_modulation_source_draws_a_dimmed_disabled_cell_not_a_blank_one
    // below for the now-populated disconnected case (AppendEncoderCell no
    // longer emits an empty command vector for a disconnected cell either).
    std::cout << "[OBSERVED] encoder(0) drawCommands: " << encoder->drawCommands.size()
              << ", body(FillEllipse) seen=" << sawBody << ", ring(Arc) seen=" << sawRing
              << ", badge outline(s) seen=" << sawBadgeOutline << "\n";
    REQUIRE_TRUE(sawBody);
    REQUIRE_TRUE(sawRing);

    // Second positive control, specific to the size-based split
    // above: the default patch wires modulators onto this cell (Audio
    // bank's VCO1 pitch), so this cell DOES emit badge-chip
    // StrokeRoundedRect commands every run. Without this, "no frame" below
    // could pass vacuously against a cell that never emits a
    // StrokeRoundedRect of ANY kind -- which would prove nothing about
    // whether `spansCell` actually keeps badges out of `sawFrame`, exactly
    // the vacuous-pass failure mode this test's own history already
    // produced once (see the "pre-existing gap" comment above).
    REQUIRE_TRUE(sawBadgeOutline);

    // The actual fix: no CELL-SPANNING stroke-rounded-rect (the card frame)
    // reaches the rendered encoder cell.
    REQUIRE_TRUE(!sawFrame);
}

// --- Disconnected modulation-source cells draw disabled, not blank ------

// SynthRig never wires a real host input-routing signal (AppContext::
// inputRoutingSignal stays null the whole run -- grep confirms nothing
// under tests/support sets it), so `ExternalAudioConnected()` stays false
// for this rig's entire lifetime with no way for a test to flip it
// (FroggersModulation.hpp's own comment: "External audio pair registered
// `connected = false` initially ... immediately overwritten once
// FroggersAppCore::Init() reads the host's actual routed-input state" --
// that host state never arrives here). That makes the External Audio
// modulation source (kModSlotExternalAudio) a reliably, permanently
// disconnected REAL cell once drilled into a page parameter's modulation
// view -- exactly the case AppendEncoderCell's `disabledCell` branch
// (FroggersUiSurface.hpp) now draws instead of leaving blank.
TEST_CASE(disconnected_modulation_source_draws_a_dimmed_disabled_cell_not_a_blank_one) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("disconnected_source_disabled_cell"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    // Drill into page parameter 0's modulation view -- same bridge/sequence
    // drill_in_swaps_grid_in_place_scope_and_chrome_stay_put and
    // modulation_header_sits_below_bank_row_and_above_parameter_cells above
    // already use.
    surface.DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kEncoderPress, "0"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().ActiveDrillIn().Level() == 1);  // genuinely drilled in, not assumed

    synth::ParameterManager::UIState& uiState = rig.UIState();  // forces a synchronous publish

    // Positive control: the source really is disconnected, not merely
    // assumed to be -- everything below would pass vacuously against a
    // cell that happened to already be connected.
    const synth::Parameter::UIState& externalAudioCell =
        uiState.slots[0].cells[synth_froggers::kModSlotExternalAudio];
    const synth::ui::EncoderDrawState externalAudioState =
        synth::ui::EncoderDrawStateFromParameter(externalAudioCell);
    REQUIRE_TRUE(!externalAudioState.connected);

    const synth::ui::NodeTree tree = surface.BuildTree();
    const std::string disconnectedId =
        synth_froggers::FroggersNodeIds::Encoder(static_cast<std::size_t>(synth_froggers::kModSlotExternalAudio));
    const synth::ui::Node* disconnectedNode = FindNodeById(tree, disconnectedId);
    REQUIRE_TRUE(disconnectedNode != nullptr);

    bool sawBody = false;
    bool sawRing = false;
    bool sawValueGlyph = false;
    for (const synth::ui::DrawCommand& command : disconnectedNode->drawCommands) {
        if (command.kind == synth::ui::DrawCommand::Kind::FillEllipse) {
            sawBody = true;
        }
        if (command.kind == synth::ui::DrawCommand::Kind::Arc) {
            sawRing = true;
        }
        // The 14-segment label glyphs are the ONLY FillPolygon commands
        // BuildEncoderDrawCommands ever emits (FourteenSegment::
        // AppendCharacter, EncoderDraw.hpp:488-531) -- every other layer
        // (body, ring, badges, frame) uses a different DrawCommand kind, so
        // this is a direct, count-independent test of "no readout drawn"
        // rather than an inference from a command total.
        if (command.kind == synth::ui::DrawCommand::Kind::FillPolygon) {
            sawValueGlyph = true;
        }
    }
    // (a) Real draw commands, not an empty vector.
    REQUIRE_TRUE(!disconnectedNode->drawCommands.empty());
    REQUIRE_TRUE(sawBody);
    // No value arc, and that is correct rather than missing: every Arc
    // BuildEncoderDrawCommands emits comes from a per-voice layer
    // (EncoderDraw.hpp:736-760, indexing state.voices), and a disconnected
    // source publishes no voices. A disabled cell shows the knob body and
    // frame with no value drawn on it. Do not "fix" this by synthesising a
    // voice: an arc on an unavailable source would be reporting a value it
    // does not have.
    REQUIRE_TRUE(!sawRing);
    // (c) No value readout.
    REQUIRE_TRUE(!sawValueGlyph);

    // (d) Still unreachable: no press or drag action reaches a source with
    // no signal.
    REQUIRE_TRUE(!disconnectedNode->action.has_value());
    REQUIRE_TRUE(!disconnectedNode->pointerDragAction.has_value());

    // (b) Visibly de-emphasised against a CONNECTED cell in the same view,
    // compared by COLOUR rather than by command count. The reference has to
    // be a real connected source: building one from this cell's own state
    // with `connected` forced true is degenerate, because the slate
    // publishes `Color::Off` for a disconnected source, so that "connected"
    // reference is black and nothing can read as dimmer than it.
    // The body outline (StrokeEllipse, `ScaleAlpha(state.baseColor, 0.9f)`,
    // EncoderDraw.hpp:687) is a pure function of `baseColor`, so it isolates
    // colour from geometry.
    const std::string connectedId =
        synth_froggers::FroggersNodeIds::Encoder(static_cast<std::size_t>(synth_froggers::kModSlotVco1Audio));
    const synth::ui::Node* connectedNode = FindNodeById(tree, connectedId);
    REQUIRE_TRUE(connectedNode != nullptr);

    const auto FindBodyStrokeColor =
        [](const std::vector<synth::ui::DrawCommand>& commands) -> std::optional<synth::Color> {
        for (const synth::ui::DrawCommand& command : commands) {
            if (command.kind == synth::ui::DrawCommand::Kind::StrokeEllipse) {
                return command.color;
            }
        }
        return std::nullopt;
    };
    const std::optional<synth::Color> disabledStroke = FindBodyStrokeColor(disconnectedNode->drawCommands);
    const std::optional<synth::Color> connectedStroke = FindBodyStrokeColor(connectedNode->drawCommands);
    REQUIRE_TRUE(disabledStroke.has_value());
    REQUIRE_TRUE(connectedStroke.has_value());
    REQUIRE_TRUE(!(*disabledStroke == *connectedStroke));
    // Drawn at all: a disconnected source publishes `Color::Off`, so an
    // outline still at zero would mean the disabled colour never applied.
    REQUIRE_TRUE(disabledStroke->r > 0 || disabledStroke->g > 0 || disabledStroke->b > 0);
    // Drained of colour, which is what reads as unavailable: the disabled
    // outline is neutral, while a connected source carries its own hue.
    const auto Chroma = [](const synth::Color& color) {
        const int hi = std::max({color.r, color.g, color.b});
        const int lo = std::min({color.r, color.g, color.b});
        return hi - lo;
    };
    constexpr int kNeutralTolerance = 16;
    REQUIRE_TRUE(Chroma(*disabledStroke) <= kNeutralTolerance);

    // (e) Other cells in the same view are unaffected: a genuinely
    // connected source in the same drilled-in grid keeps its action/drag
    // reachability and still draws its own value readout.
    REQUIRE_TRUE(connectedNode->action.has_value());
    REQUIRE_TRUE(connectedNode->pointerDragAction.has_value());
    bool connectedSawValueGlyph = false;
    for (const synth::ui::DrawCommand& command : connectedNode->drawCommands) {
        if (command.kind == synth::ui::DrawCommand::Kind::FillPolygon) {
            connectedSawValueGlyph = true;
        }
    }
    REQUIRE_TRUE(connectedSawValueGlyph);
}

// --- Single-row labels, ------
// --- never over the ring -----------------------------------------------

// Recomputes AppendEncoderCell's own label-band geometry
// (FroggersUiSurface.hpp) from a cell's own measured LOCAL extent (draw
// commands are node-local, PortableUI.hpp's own coordinate contract --
// `encoder->bounds.width/height` is this node's resolved size, the same
// convention every pre-existing geometry test in this file already uses),
// then builds the EXACT commands production would emit for a given
// approved label by calling the SAME public helper
// (`BuildEncoderLabelRowCommands`) production calls -- so what follows is a
// comparison against production's own code path, not a second
// hand-written copy of its arithmetic.
std::vector<synth::ui::DrawCommand> ExpectedEncoderLabelBandCommands(synth::Color bankBaseColor,
                                                                     const std::string& approvedLabel,
                                                                     float cellWidth, float cellHeight) {
    const float ringHeight =
        std::max(0.0f, cellHeight - synth_froggers::FroggersEncoderGridLayout::kLabelBandHeight);
    const float bandTop = ringHeight;
    const float bandHeight = std::max(0.0f, cellHeight - bandTop);
    const float plateWidth = cellWidth * 0.94f;
    const float plateLeft = (cellWidth - plateWidth) * 0.5f;
    const synth::ui::Bounds rowBounds{plateLeft, bandTop, plateWidth, bandHeight};

    const synth::Color onColor = synth::Brighten(bankBaseColor, 0.45f);
    const synth::Color offColor = synth::Color::Rgb(
        synth::kSurfaceBackground.r + 4, synth::kSurfaceBackground.g + 6, synth::kSurfaceBackground.b + 6);

    std::vector<synth::ui::DrawCommand> expected;
    expected.push_back(
        synth::ui::DrawCommand::FillRoundedRect(rowBounds, bandHeight * 0.15f, synth::kSurfaceBackground));
    const std::vector<synth::ui::DrawCommand> row = synth_froggers::BuildEncoderLabelRowCommands(
        approvedLabel, rowBounds, onColor, offColor, synth_froggers::kApprovedLabelGridColumns);
    expected.insert(expected.end(), row.begin(), row.end());
    return expected;
}

bool BoundsClose(synth::ui::Bounds a, synth::ui::Bounds b, float eps = 0.02f) {
    return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.width - b.width) < eps &&
           std::fabs(a.height - b.height) < eps;
}

bool PointsClose(const std::vector<synth::ui::Point>& a, const std::vector<synth::ui::Point>& b,
                 float eps = 0.02f) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::fabs(a[i].x - b[i].x) >= eps || std::fabs(a[i].y - b[i].y) >= eps) {
            return false;
        }
    }
    return true;
}

bool DrawCommandsMatch(const synth::ui::DrawCommand& a, const synth::ui::DrawCommand& b) {
    return a.kind == b.kind && a.color == b.color && BoundsClose(a.bounds, b.bounds) &&
           std::fabs(a.cornerRadius - b.cornerRadius) < 0.02f && PointsClose(a.points, b.points);
}

// This app always appends its label-band commands LAST (after
// BuildEncoderDrawCommands' own, trailing-block-stripped output) -- see
// AppendEncoderCell's own comment -- so the deterministic slice to compare
// is the trailing `expected.size()` commands, not a guess.
bool TrailingCommandsMatch(const std::vector<synth::ui::DrawCommand>& actual,
                           const std::vector<synth::ui::DrawCommand>& expected) {
    if (actual.size() < expected.size()) {
        return false;
    }
    const std::size_t offset = actual.size() - expected.size();
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (!DrawCommandsMatch(actual[offset + i], expected[i])) {
            return false;
        }
    }
    return true;
}

// Envelope-bank cells emit exactly the single-row short-name
// block; a truncation-class cell (Filter slot 3, "Comb offset") emits its
// approved long label in full -- both checked by command count/geometry
// (dpDotCount, and a full trailing-command match against
// `ExpectedEncoderLabelBandCommands`) against the approved list, never by
// inspecting pixels.
TEST_CASE(envelope_short_forms_and_filter_slot3_long_label_render_single_row_verbatim) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("label_row_content"));
    rig.RunBlocks(4);
    rig.UIState();  // forces a synchronous publish

    synth::ui::Surface& surface = rig.Application().PortableSurface();

    const auto checkCell = [&](std::size_t bankIx, std::size_t slot, const char* approvedLabel) {
        surface.DispatchAction(
            synth::ui::Action::WithValue(synth_froggers::FroggersActions::kBankSelect, std::to_string(bankIx)));
        rig.RunBlocks(4);
        rig.UIState();

        const synth::ui::NodeTree tree = surface.BuildTree();
        const synth::ui::Node* encoder = FindNodeById(tree, synth_froggers::FroggersNodeIds::Encoder(slot));
        REQUIRE_TRUE(encoder != nullptr);

        std::size_t dpDotCount = 0;
        for (const synth::ui::DrawCommand& command : encoder->drawCommands) {
            if (command.kind == synth::ui::DrawCommand::Kind::FillEllipse && command.bounds.width < 5.0f) {
                ++dpDotCount;
            }
        }
        // Single row, kApprovedLabelGridColumns dots -- NOT the rejected
        // 2-row grid's 2x that count, regardless of label length.
        REQUIRE_TRUE(dpDotCount == static_cast<std::size_t>(synth_froggers::kApprovedLabelGridColumns));

        const synth::Color bankColor = synth_froggers::FroggersBankLayouts()[bankIx].color;
        const std::vector<synth::ui::DrawCommand> expected = ExpectedEncoderLabelBandCommands(
            bankColor, approvedLabel, encoder->bounds.width, encoder->bounds.height);
        REQUIRE_TRUE(TrailingCommandsMatch(encoder->drawCommands, expected));
        std::cout << "[OBSERVED] bank " << bankIx << " slot " << slot << " (\"" << approvedLabel
                  << "\"): dpDotCount=" << dpDotCount << ", trailing commands match expected\n";
    };

    // Envelope (bank 1) slot 0 -- canonical short form "A1" (operator
    // ruling: the short form IS the name here).
    checkCell(1, 0, "A1");
    // Filter (bank 2) slot 3 -- the truncation-class long name "Comb
    // offset" (the exact site the predecessor's rejected work clipped to
    // "CMBO"), now required to render in full.
    checkCell(2, 3, "Comb offset");
}

// No label draw command's bounds intersect the ring's drawn arc,
// in ANY cell of ALL SIX banks -- computable from the command list. This is
// the property the predecessor's rejected work violated (measured: ~95% of
// the lower semicircle covered).
//
// Distinguishes a cell's RING-kind commands (everything
// `BuildEncoderDrawCommands` itself can emit into the ring's own
// sub-extent: body/highlight ellipses, the outline stroke, modulation/
// switch arcs, and modulator badge chip outlines) from this app's own
// LABEL-band commands (the plate `FillRoundedRect` and the single-row
// 14-segment glyphs/decimal-dots `BuildEncoderLabelRowCommands` appends --
// kinds `BuildEncoderDrawCommands` never emits itself: it has no
// `FillPolygon` call site of its own outside the trailing label block
// this app strips; `AppendBadge` DOES emit `FillRoundedRect` -- see the
// NOTE below, which is the accurate account and governs the classifier
// (corrected 2026-08-17: this sentence previously claimed `AppendBadge`
// used `StrokeRoundedRect`, contradicting that NOTE three lines down).
// A small
// dot-sized `FillEllipse` (width < 5px) is a decimal point (this app's
// addition); a big one is the ring's own body/highlight fill.
// NOTE: `AppendBadge` (EncoderDraw.hpp) emits its OWN `FillRoundedRect`
// pair (chip background + chip foreground), so `FillRoundedRect` alone
// does not distinguish a ring-area badge chip from this app's own label
// plate -- both need the same size split `encoder_cell_never_emits_a_frame_
// draw_command`'s own header comment already establishes for
// StrokeRoundedRect (badges top out near 28% of the cell's smaller
// dimension; this app's label plate is 94% of the FULL cell width, ~128px
// at the measured cell size -- wide margin on both sides of 50px).
bool IsRingKindCommand(const synth::ui::DrawCommand& command) {
    switch (command.kind) {
        case synth::ui::DrawCommand::Kind::Arc:
        case synth::ui::DrawCommand::Kind::StrokeEllipse:
        case synth::ui::DrawCommand::Kind::StrokeRoundedRect:
            return true;
        case synth::ui::DrawCommand::Kind::FillEllipse:
            return command.bounds.width >= 5.0f;
        case synth::ui::DrawCommand::Kind::FillRoundedRect:
            return command.bounds.width < 50.0f;  // a badge chip, not this app's own label plate.
        default:
            return false;
    }
}

bool IsLabelBandCommand(const synth::ui::DrawCommand& command) {
    switch (command.kind) {
        case synth::ui::DrawCommand::Kind::FillPolygon:
            return true;
        case synth::ui::DrawCommand::Kind::FillEllipse:
            return command.bounds.width < 5.0f;
        case synth::ui::DrawCommand::Kind::FillRoundedRect:
            return command.bounds.width >= 50.0f;  // this app's own label plate.
        default:
            return false;
    }
}

// `DrawCommand::FillPolygon`'s static factory (PortableUI.hpp) never
// populates `.bounds` -- only `.points` -- so a command's Y-extent has to
// come from whichever field the command's own KIND actually sets.
float CommandMinY(const synth::ui::DrawCommand& command) {
    if (command.kind == synth::ui::DrawCommand::Kind::FillPolygon) {
        float minY = std::numeric_limits<float>::infinity();
        for (const synth::ui::Point& point : command.points) {
            minY = std::min(minY, point.y);
        }
        return minY;
    }
    return command.bounds.y;
}

float CommandMaxY(const synth::ui::DrawCommand& command) {
    if (command.kind == synth::ui::DrawCommand::Kind::FillPolygon) {
        float maxY = -std::numeric_limits<float>::infinity();
        for (const synth::ui::Point& point : command.points) {
            maxY = std::max(maxY, point.y);
        }
        return maxY;
    }
    return command.bounds.y + command.bounds.height;
}

TEST_CASE(no_label_command_intersects_the_ring_in_any_cell_of_any_bank) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("label_never_over_ring"));
    rig.RunBlocks(4);
    rig.UIState();

    synth::ui::Surface& surface = rig.Application().PortableSurface();

    std::size_t cellsChecked = 0;
    std::size_t ringCommandsSeen = 0;
    std::size_t labelCommandsSeen = 0;

    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
        surface.DispatchAction(
            synth::ui::Action::WithValue(synth_froggers::FroggersActions::kBankSelect, std::to_string(bankIx)));
        rig.RunBlocks(4);
        rig.UIState();

        const synth::ui::NodeTree tree = surface.BuildTree();
        for (std::size_t ix = 0; ix < synth_froggers::FroggersEncoderGridLayout::kEncoderCount; ++ix) {
            const synth::ui::Node* encoder = FindNodeById(tree, synth_froggers::FroggersNodeIds::Encoder(ix));
            REQUIRE_TRUE(encoder != nullptr);

            float ringMaxY = -std::numeric_limits<float>::infinity();
            float labelMinY = std::numeric_limits<float>::infinity();
            for (const synth::ui::DrawCommand& command : encoder->drawCommands) {
                if (IsRingKindCommand(command)) {
                    ringMaxY = std::max(ringMaxY, CommandMaxY(command));
                    ++ringCommandsSeen;
                } else if (IsLabelBandCommand(command)) {
                    labelMinY = std::min(labelMinY, CommandMinY(command));
                    ++labelCommandsSeen;
                }
            }

            // Positive control: every one of the 96 (6 banks x 16 slots)
            // cells swept here is a real, connected, always-visible
            // page/global parameter (never the modulation-view's hidden
            // case), so both counts must be real numbers or the check
            // below would pass vacuously against an empty command list.
            REQUIRE_TRUE(std::isfinite(ringMaxY));
            REQUIRE_TRUE(std::isfinite(labelMinY));
            REQUIRE_TRUE(labelMinY >= ringMaxY - 0.01f);
            ++cellsChecked;
        }
    }

    std::cout << "[OBSERVED] label-vs-ring sweep: " << cellsChecked << " cells, " << ringCommandsSeen
              << " ring commands, " << labelCommandsSeen << " label commands, all clear\n";
    REQUIRE_TRUE(cellsChecked ==
                 synth_froggers::kFroggersBankCount * synth_froggers::FroggersEncoderGridLayout::kEncoderCount);
}

// Every rendered label matches the approved label table VERBATIM, so a later
// rename cannot silently reintroduce a truncation. Covers all 86 entries.
// Deliberately an INDEPENDENT copy of the approved label table (not a read of
// `FroggersApprovedLabels()`, production's own copy) -- comparing
// production's rendering against production's own table would not catch
// production reading the WRONG source (e.g. reverting to
// `FroggersParamSpec::name`, which is exactly what the predecessor did)
// or a typo in that table; this is the independent oracle that table itself
// is.
TEST_CASE(every_rendered_label_matches_the_approved_list_verbatim) {
    static const std::array<std::array<const char*, synth_froggers::kFroggersParamsPerBank>,
                            synth_froggers::kFroggersBankCount>
        kExpectedApprovedLabels{{
            {{"VCO1", "VCO2", "VCO3", "Shape 1", "Shape 2", "Shape 3", "Ph.mod 1", "Ph.mod 2", "Ph.mod 3",
              "Ringmod 1", "Ringmod 2", "Ringmod 3", "PM rate", "VCO balance"}},
            {{"A1", "D1", "S1", "R1", "A2", "D2", "S2", "R2", "A3", "D3", "S3", "R3", "Curve", "Grace"}},
            {{"Peak freq", "Peak gain", "Peak Q", "Comb offset", "Comb delay", "Comb FB", "Comb LP",
              "Comb drive", "Scoop mix", "Scoop freq", "Scoop width", "Scoop depth", "Comb/Peak", "Topology"}},
            {{"Drive", "Shape", "SRR 1", "SRR 2", "XOR", "Bit depth", "Fuzz", "Blend", "Phase", "Anti-alias",
              "Link", "Fold", "Tone", "Bias"}},
            {{"Delay time", "Send", "Feedback", "Stereo width", "Freeze", "Mod depth", "Wet mix", "Reverse",
              "Diffusion", "FB drive", "FB tone", "Mod rate", "Width bal", "Crush"}},
            {{"Wet/dry", "Room size", "Decay", "Pre-delay", "Damping", "Stereo width", "Diffusion",
              "Mod depth", "Hold", "Mod rate", "Tank drive", "Grit", "Tilt", "Tuned"}},
        }};
    constexpr const char* kExpectedCrispy = "Crispy";
    constexpr const char* kExpectedCrunchy = "Crunchy";

    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("label_verbatim_sweep"));
    rig.RunBlocks(4);
    rig.UIState();

    synth::ui::Surface& surface = rig.Application().PortableSurface();

    std::size_t entriesChecked = 0;
    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
        surface.DispatchAction(
            synth::ui::Action::WithValue(synth_froggers::FroggersActions::kBankSelect, std::to_string(bankIx)));
        rig.RunBlocks(4);
        rig.UIState();

        const synth::ui::NodeTree tree = surface.BuildTree();
        const synth::Color bankColor = synth_froggers::FroggersBankLayouts()[bankIx].color;

        for (std::size_t slot = 0; slot < synth_froggers::kFroggersSlotsPerBank; ++slot) {
            const synth::ui::Node* encoder = FindNodeById(tree, synth_froggers::FroggersNodeIds::Encoder(slot));
            REQUIRE_TRUE(encoder != nullptr);

            const char* expectedLabel = slot < synth_froggers::kFroggersParamsPerBank
                                             ? kExpectedApprovedLabels[bankIx][slot]
                                             : (slot == synth_froggers::kFroggersCrispySlot ? kExpectedCrispy
                                                                                             : kExpectedCrunchy);
            // Crunchy (slot 15) is ONE shared Parameter with its OWN fixed
            // colour (`FroggersCrunchyColor()`, Yellow -- FroggersParameters.hpp),
            // distinct from every bank's own colour; Crispy (slot 14) and
            // every page parameter (slots 0-13) DO take the bank's colour
            // (FroggersParameters.hpp's own registration, `.baseColor =
            // layout.color`).
            const synth::Color slotColor =
                slot == synth_froggers::kFroggersCrunchySlot ? synth_froggers::FroggersCrunchyColor() : bankColor;
            const std::vector<synth::ui::DrawCommand> expected = ExpectedEncoderLabelBandCommands(
                slotColor, expectedLabel, encoder->bounds.width, encoder->bounds.height);
            REQUIRE_TRUE(TrailingCommandsMatch(encoder->drawCommands, expected));
            ++entriesChecked;
        }
    }

    std::cout << "[OBSERVED] verbatim sweep: " << entriesChecked << " (bank,slot) entries matched (6 banks x 16 slots, "
              << "covering all 86 distinct approved labels)\n";
    REQUIRE_TRUE(entriesChecked ==
                 synth_froggers::kFroggersBankCount * synth_froggers::kFroggersSlotsPerBank);
}

// Structural guard against a future label-table edit silently overflowing the
// single-row grid: proves the thing that isn't structurally guaranteed --
// that no approved label is longer than `kApprovedLabelGridColumns` -- over
// every one of the 86 entries, not a hand-picked sample. Supersedes
// `every_parameter_label_fits_the_two_line_grid` (the old 2-row/10-column
// grid this task retires along with `SplitFourteenSegmentLines`).
TEST_CASE(every_approved_label_fits_the_single_row_grid) {
    std::size_t longest = 0;
    std::string longestText;

    const auto checkOne = [&](const char* label) {
        const std::size_t len = std::string_view(label).size();
        REQUIRE_TRUE(static_cast<int>(len) <= synth_froggers::kApprovedLabelGridColumns);
        if (len > longest) {
            longest = len;
            longestText = label;
        }
    };

    for (const auto& bankRow : synth_froggers::FroggersApprovedLabels()) {
        for (const char* label : bankRow) {
            checkOne(label);
        }
    }
    checkOne(synth_froggers::FroggersApprovedGlobalLabel(synth_froggers::kFroggersCrispySlot));
    checkOne(synth_froggers::FroggersApprovedGlobalLabel(synth_froggers::kFroggersCrunchySlot));

    std::cout << "[OBSERVED] longest approved label: \"" << longestText << "\" (" << longest << " chars)\n";
    // The task brief's own claim, checked rather than assumed: the longest
    // of all 86 approved entries ("Stereo width", Delay/Reverb) is exactly
    // kApprovedLabelGridColumns characters.
    REQUIRE_TRUE(longest == static_cast<std::size_t>(synth_froggers::kApprovedLabelGridColumns));
}

// --- 3.5: scene buttons toggle the blend to its extremes --------------------

// S1/S2's old behaviour (`SetLessSelectedScene`) reassigned
// which stored scene occupied the less-weighted endpoint and never moved
// the blend, so clicking did nothing audible at either blend extreme. This
// test drives the fix end-to-end through the real surface/message-bus path
// (not by calling ParameterManager directly): pressing each relabelled
// "Scene 1"/"Scene 2" button must push a `MessageIn::SetSceneBlend` that
// actually lands at the correct extreme -- 0.0 for Scene 1 (leftScene,
// FroggersParameters.hpp's fixed `SetSceneEndpoints(0, 1)`), 1.0 for
// Scene 2 (rightScene).
TEST_CASE(scene_buttons_push_scene_blend_to_the_correct_extremes) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("scene_buttons_blend"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    const synth::ui::NodeTree tree = surface.BuildTree();

    // Relabelled -- no lingering "S1"/"S2".
    REQUIRE_TRUE(HasButtonLabeled(tree, "Scene 1"));
    REQUIRE_TRUE(HasButtonLabeled(tree, "Scene 2"));
    REQUIRE_TRUE(!HasButtonLabeled(tree, "S1"));
    REQUIRE_TRUE(!HasButtonLabeled(tree, "S2"));

    const synth::ui::Node* blendNode = FindNodeById(tree, synth_froggers::FroggersNodeIds::kSceneBlend);
    REQUIRE_TRUE(blendNode != nullptr);
    REQUIRE_TRUE(blendNode->kind == synth::ui::NodeKind::Slider);
    REQUIRE_TRUE(blendNode->label == "Scene blend");

    // Model default is blend=0.0 (SceneState{}'s own default, matching
    // FroggersParameters.hpp's own comment on Init()).
    REQUIRE_TRUE(std::fabs(rig.UIState().sceneBlend.load() - 0.0f) < 0.001f);

    // Scene 2 (index 1) -> blend must move to the 1.0 extreme.
    surface.DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kSceneSelect, "1"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(std::fabs(rig.UIState().sceneBlend.load() - 1.0f) < 0.001f);

    // Scene 1 (index 0) -> blend must move back to the 0.0 extreme --
    // proving the button is a real toggle, not a one-shot.
    surface.DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kSceneSelect, "0"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(std::fabs(rig.UIState().sceneBlend.load() - 0.0f) < 0.001f);
}

// The Scene blend label sits BELOW its slider. This supersedes an earlier
// caption for scene-blend: a `ControlStyle::caption` can only lead, so
// scene-blend uses a hand-rolled label, placed under the slider.
//
// `NodeKind::Slider` routes `node.label` to `juce::Slider::setName()` only
// (PortableJuceBackend.hpp:1229-1232) -- no `juce::Label` is attached, so
// nothing ever draws it; some adjacent Label node is required regardless of
// which mechanism produces it. That mechanism was once
// `ControlStyle::caption` (a sibling Label BEFORE the control); scene-blend
// now uses a hand-rolled Label node
// (`FroggersNodeIds::kSceneBlendLabel`) placed AFTER the slider, because a
// caption can only lead and the operator wants it below/trailing here.
//
// NOTE: an earlier test enumeration classified this test as
// UNAFFECTED, which does not hold once the CELL MAP amendment is applied --
// see FroggersUiSurface.hpp's `AppendSceneBlendGroup()` comment for the same
// note. The amendment is the more specific, more recently affirmed
// instruction and governs; this rewrite is flagged in the task report as a
// place the traced table was wrong.
//
// This test asserts the Label NODE exists, carries the expected text, and
// sits immediately AFTER the Slider in `tree.nodes` order. It does NOT and
// CANNOT prove the text is actually painted on screen -- that requires a
// human looking at the running app; a previous task was closed on exactly
// that false equivalence (asserting the label field was set) and this
// comment exists so it isn't repeated.
TEST_CASE(scene_blend_slider_has_an_adjacent_label_node_carrying_its_text) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("scene_blend_label"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    const synth::ui::NodeTree tree = surface.BuildTree();

    const synth::ui::Node* labelNode = FindNodeById(tree, synth_froggers::FroggersNodeIds::kSceneBlendLabel);
    REQUIRE_TRUE(labelNode != nullptr);
    REQUIRE_TRUE(labelNode->kind == synth::ui::NodeKind::Label);
    REQUIRE_TRUE(labelNode->text == "Scene blend");

    const std::optional<std::size_t> labelIx =
        FindNodeIndexById(tree, synth_froggers::FroggersNodeIds::kSceneBlendLabel);
    const std::optional<std::size_t> sliderIx =
        FindNodeIndexById(tree, synth_froggers::FroggersNodeIds::kSceneBlend);
    REQUIRE_TRUE(labelIx.has_value());
    REQUIRE_TRUE(sliderIx.has_value());
    // The label FOLLOWS its slider in node order, which in the Column both
    // rows now use (AppendLabelledSlider()) renders it BELOW the
    // slider -- the opposite order from the retired caption, which
    // always led. Row 6 asserts the identical relationship; the two rows are
    // deliberately the same shape now.
    REQUIRE_TRUE(*labelIx == *sliderIx + 1);
}

// Operator follow-up (2026-08-17): the scene-blend slider must PRESENT
// 1.0-2.0 to match the "Scene 1"/"Scene 2" buttons (unchanged),
// while Sheaf's own `SceneState.blend` stays 0..1 -- see
// FroggersUiSurface.hpp's `kSceneBlendDisplayOffset` for the full trace.
// This asserts the slider node's declared bounds and presented value are
// offset from the underlying blend, at both extremes.
TEST_CASE(scene_blend_slider_presents_1_to_2_while_the_underlying_blend_stays_0_to_1) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("scene_blend_display_range"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();

    // Model default is blend=0.0 -- the slider must declare 1.0/2.0 bounds
    // and present 1.0 (blend + offset), not the raw 0.0 blend.
    const synth::ui::NodeTree treeAtDefault = surface.BuildTree();
    const synth::ui::Node* blendNodeAtDefault =
        FindNodeById(treeAtDefault, synth_froggers::FroggersNodeIds::kSceneBlend);
    REQUIRE_TRUE(blendNodeAtDefault != nullptr);
    REQUIRE_TRUE(blendNodeAtDefault->minValue == 1.0f);
    REQUIRE_TRUE(blendNodeAtDefault->maxValue == 2.0f);
    REQUIRE_TRUE(std::fabs(blendNodeAtDefault->value - 1.0f) < 0.001f);

    // Push the underlying blend to its other extreme via the Scene 2
    // button (unaffected by this change) and confirm the
    // presented value follows with the same +1 offset -- 1.0 blend presents
    // as 2.0.
    surface.DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kSceneSelect, "1"));
    rig.RunBlocks(4);
    const synth::ui::NodeTree treeAtOtherExtreme = surface.BuildTree();
    const synth::ui::Node* blendNodeAtOtherExtreme =
        FindNodeById(treeAtOtherExtreme, synth_froggers::FroggersNodeIds::kSceneBlend);
    REQUIRE_TRUE(blendNodeAtOtherExtreme != nullptr);
    REQUIRE_TRUE(std::fabs(blendNodeAtOtherExtreme->value - 2.0f) < 0.001f);
}

// A `kSceneBlend` action's value is now the DISPLAYED 1.0-2.0 reading;
// HandleAction() must subtract the same offset back out (and clamp) so the
// message it pushes still carries Sheaf's own 0..1 blend. Round-trips
// through the real HandleAction() path (DispatchAction -> message bus ->
// UIState), not by calling the offset arithmetic directly.
TEST_CASE(scene_blend_slider_action_subtracts_the_display_offset_and_clamps) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("scene_blend_action_offset"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();

    // A displayed "1.5" must land at blend 0.5 (1.5 - kSceneBlendDisplayOffset).
    surface.DispatchAction(synth::ui::Action::WithValue(synth_froggers::FroggersActions::kSceneBlend, "1.5"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(std::fabs(rig.UIState().sceneBlend.load() - 0.5f) < 0.001f);

    // The two displayed extremes must land exactly on Sheaf's own blend
    // extremes: 1.0 -> 0.0, 2.0 -> 1.0.
    surface.DispatchAction(synth::ui::Action::WithValue(synth_froggers::FroggersActions::kSceneBlend, "1.0"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(std::fabs(rig.UIState().sceneBlend.load() - 0.0f) < 0.001f);

    surface.DispatchAction(synth::ui::Action::WithValue(synth_froggers::FroggersActions::kSceneBlend, "2.0"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(std::fabs(rig.UIState().sceneBlend.load() - 1.0f) < 0.001f);
}

// --- 10.6: BPM slider normal vs external-clock-slaved states ----------------

TEST_CASE(bpm_slider_writes_and_displays_tempo_in_normal_state) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("bpm_normal"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    surface.DispatchAction(synth::ui::Action::WithValue(synth_froggers::FroggersActions::kBpm, "137.0"));
    rig.RunBlocks(4);

    REQUIRE_TRUE(!rig.Application().TempoExternallyClocked());
    REQUIRE_TRUE(std::fabs(rig.Application().DisplayTempoBpm() - 137.0) < 0.5);

    // The rendered tree carries an interactive BPM Slider (not a StatusText)
    // while not slaved.
    const synth::ui::NodeTree tree = surface.BuildTree();
    bool foundSlider = false;
    for (const synth::ui::Node& node : tree.nodes) {
        if (node.id.value == synth_froggers::FroggersNodeIds::kBpm) {
            REQUIRE_TRUE(node.kind == synth::ui::NodeKind::Slider);
            foundSlider = true;
        }
    }
    REQUIRE_TRUE(foundSlider);
}

TEST_CASE(bpm_slider_is_read_only_and_shows_recovered_tempo_while_externally_clocked) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("bpm_external"));
    rig.RunBlocks(4);

    // Set a known manual tempo first, then slave to external MIDI clock.
    synth::ui::Surface& surface = rig.Application().PortableSurface();
    surface.DispatchAction(synth::ui::Action::WithValue(synth_froggers::FroggersActions::kBpm, "100.0"));
    rig.RunBlocks(4);

    REQUIRE_TRUE(rig.SetSyncConfig(synth::SyncConfig{.receiveClock = true}));
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.Application().TempoExternallyClocked());

    // SetTempoBpm returns false and does nothing while slaved
    // (src/MasterClock.cpp:963-965) -- attempting to set 222 must not move
    // the active tempo, and the surface must not even forward the request
    // (FroggersUiSurface's own belt-and-suspenders guard).
    const double tempoBeforeAttempt = rig.Application().DisplayTempoBpm();
    surface.DispatchAction(synth::ui::Action::WithValue(synth_froggers::FroggersActions::kBpm, "222.0"));
    rig.RunBlocks(4);
    REQUIRE_TRUE(std::fabs(rig.Application().DisplayTempoBpm() - tempoBeforeAttempt) < 0.5);
    REQUIRE_TRUE(std::fabs(rig.Application().DisplayTempoBpm() - 222.0) > 1.0);

    // The rendered tree carries a read-only StatusText, not an interactive
    // Slider, while slaved -- "read-only/inert... no longer accepts input."
    const synth::ui::NodeTree tree = surface.BuildTree();
    bool foundStatusText = false;
    for (const synth::ui::Node& node : tree.nodes) {
        if (node.id.value == synth_froggers::FroggersNodeIds::kBpm) {
            REQUIRE_TRUE(node.kind == synth::ui::NodeKind::StatusText);
            foundStatusText = true;
        }
    }
    REQUIRE_TRUE(foundStatusText);
}

// This test used
// to be `bpm_label_indicates_no_effect_while_transport_is_stopped`, pinning
// a "BPM (no effect while stopped)" annotation that switched in and out with
// transport state. That annotation was never requested -- an agent invented
// it "to improve discoverability" -- and, because
// chrome is auto-flowed by control width (FroggersAutoFlowedChromeModel),
// it re-flowed every neighbouring control on every Play/Stop -- operator:
// "a really stupid feature I never asked for, and it changes the alignment
// of nearby labels." Reverted to a constant "BPM"; this test now asserts
// the label does NOT change across transport state transitions, i.e. the
// exact regression this correction must not reintroduce.
TEST_CASE(bpm_label_is_constant_across_transport_state) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("bpm_stopped_label"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();

    // Transport is stopped by default (the rig never presses Play) --
    // the label must already read the plain constant, and the slider must
    // still be interactive (setting a tempo ahead of pressing Play is
    // legitimate).
    REQUIRE_TRUE(!rig.Application().TransportRunning());
    {
        const synth::ui::NodeTree tree = surface.BuildTree();
        const synth::ui::Node* bpmNode = FindNodeById(tree, synth_froggers::FroggersNodeIds::kBpm);
        REQUIRE_TRUE(bpmNode != nullptr);
        REQUIRE_TRUE(bpmNode->kind == synth::ui::NodeKind::Slider);
        REQUIRE_TRUE(bpmNode->label == "BPM");
    }

    // Once running, the label must be unchanged.
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));
    rig.RunBlocks(8);
    REQUIRE_TRUE(rig.Application().TransportRunning());
    {
        const synth::ui::NodeTree tree = surface.BuildTree();
        const synth::ui::Node* bpmNode = FindNodeById(tree, synth_froggers::FroggersNodeIds::kBpm);
        REQUIRE_TRUE(bpmNode != nullptr);
        REQUIRE_TRUE(bpmNode->kind == synth::ui::NodeKind::Slider);
        REQUIRE_TRUE(bpmNode->label == "BPM");
    }

    // Stop again -- still unchanged.
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kStop));
    rig.RunBlocks(4);
    REQUIRE_TRUE(!rig.Application().TransportRunning());
    {
        const synth::ui::NodeTree tree = surface.BuildTree();
        const synth::ui::Node* bpmNode = FindNodeById(tree, synth_froggers::FroggersNodeIds::kBpm);
        REQUIRE_TRUE(bpmNode != nullptr);
        REQUIRE_TRUE(bpmNode->label == "BPM");
    }
}

// Label-visibility fix (2026-07-28), BPM half -- see
// `scene_blend_slider_has_an_adjacent_label_node_carrying_its_text`'s
// comment for the full trace of why the Slider's own label never draws and
// what an adjacent Label node does/doesn't prove. This asserts the caption
// Label node exists and reads the constant "BPM" text in both transport
// states (the "(no effect while stopped)"
// annotation this test used to track is gone, see
// `bpm_label_is_constant_across_transport_state`'s comment for why).
//
// NEITHER this pair NOR its scene-blend neighbour is a `ControlStyle::
// caption`, and for the same single reason: `Builder::FinishControl`
// (PortableUIBuilders.hpp:428-465) always emits a caption BEFORE its control
// with no option to place it after, and since that change BOTH
// labels sit BELOW their slider. Filed upstream (caption
// placement); when it lands, both collapse into captions together.
//
// THE HISTORY HERE IS THE POINT, because this comment has twice asserted the
// opposite. An earlier change converted scene-blend to a caption and left
// this one hand-rolled, citing the standing trailing-label instruction as a
// second live cause; an implementer following that change literally flipped
// this label's order and flagged it rather than absorbing it, and the flip
// was reverted. Then the cell-map rewrite moved scene-blend's label BELOW its slider,
// and a later change moved this one below to match, retiring it outright.
//
// That instruction's stated reason was that a LEADING label sat between the two sliders
// and read as labelling the wrong control; trailing was merely the only
// alternative available while both labels shared a horizontal band. A label
// directly beneath its own control cannot be misread that way, so both-below
// serves that concern better than trailing did. The asymmetry it protected
// was a means, not the goal -- and an earlier draft of that change missed exactly
// that, keeping trailing and inventing a placement parameter to honour its
// literal words. An instruction's rationale is part of the instruction: when
// the rationale dies, the instruction is up for re-derivation rather than
// mechanical preservation.
TEST_CASE(bpm_slider_has_an_adjacent_label_node_with_the_constant_bpm_text) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("bpm_adjacent_label"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();

    auto checkAdjacentLabel = [&](const std::string& expectedText) {
        const synth::ui::NodeTree tree = surface.BuildTree();
        const synth::ui::Node* labelNode =
            FindNodeById(tree, synth_froggers::FroggersNodeIds::kBpmLabel);
        REQUIRE_TRUE(labelNode != nullptr);
        REQUIRE_TRUE(labelNode->kind == synth::ui::NodeKind::Label);
        REQUIRE_TRUE(labelNode->text == expectedText);

        const std::optional<std::size_t> labelIx =
            FindNodeIndexById(tree, synth_froggers::FroggersNodeIds::kBpmLabel);
        const std::optional<std::size_t> sliderIx =
            FindNodeIndexById(tree, synth_froggers::FroggersNodeIds::kBpm);
        REQUIRE_TRUE(labelIx.has_value());
        REQUIRE_TRUE(sliderIx.has_value());
        // The label FOLLOWS its slider in node order, which inside the Column
        // AppendLabelledSlider() emits renders it BELOW the slider -- the
        // same relationship the scene-blend row asserts. Keep this pinned to
        // the ORDER, not merely to the label's existence: an ordering
        // assertion is what caught an earlier change silently moving this label once
        // already, and order is what distinguishes "labels its own control"
        // from "labels its neighbour's."
        REQUIRE_TRUE(*labelIx == *sliderIx + 1);
    };

    // Stopped by default -- the Label must already read the plain constant.
    REQUIRE_TRUE(!rig.Application().TransportRunning());
    checkAdjacentLabel("BPM");

    // Running -- unchanged.
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));
    rig.RunBlocks(8);
    REQUIRE_TRUE(rig.Application().TransportRunning());
    checkAdjacentLabel("BPM");
}

// --- Exactly two randomize controls; neither retired control ------

// ---------------------------------------------------------------------
// Reset Page / Reset All.
// ---------------------------------------------------------------------

TEST_CASE(reset_row_sits_below_randomize_with_two_equal_halves) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("reset_row_layout"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    const synth::ui::NodeTree tree = surface.BuildTree();

    REQUIRE_TRUE(HasButtonLabeled(tree, "Reset Page"));
    REQUIRE_TRUE(HasButtonLabeled(tree, "Reset All"));

    // "Below them, same size" is defined BY the Randomize row,
    // not re-derived: the Reset row must carry the same two-halves
    // weighting. Assert the row exists and that Reset's own buttons come
    // after Randomize's in the tree's emission order, which is the row
    // order kRightRows declares.
    std::size_t randomizeAllIx = tree.nodes.size();
    std::size_t resetPageIx = tree.nodes.size();
    std::size_t resetAllIx = tree.nodes.size();
    for (std::size_t i = 0; i < tree.nodes.size(); ++i) {
        const synth::ui::Node& node = tree.nodes[i];
        if (node.kind != synth::ui::NodeKind::Button) {
            continue;
        }
        if (node.label == "Randomize All") { randomizeAllIx = i; }
        if (node.label == "Reset Page") { resetPageIx = i; }
        if (node.label == "Reset All") { resetAllIx = i; }
    }
    REQUIRE_TRUE(randomizeAllIx < tree.nodes.size());
    REQUIRE_TRUE(resetPageIx < tree.nodes.size());
    REQUIRE_TRUE(resetAllIx < tree.nodes.size());
    REQUIRE_TRUE(resetPageIx > randomizeAllIx);  // below, not above.
    REQUIRE_TRUE(resetAllIx > resetPageIx);      // Page then All, matching Randomize's own order.

    std::cout << "  [reset row order] RandomizeAll@" << randomizeAllIx << " ResetPage@" << resetPageIx
              << " ResetAll@" << resetAllIx << "\n";
}

TEST_CASE(randomize_reset_sit_beside_the_sliders_in_a_narrow_viewport) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("narrow_viewport_buttons"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();

    // The layout this test exists to REJECT is the one the operator turned
    // down: the four buttons anywhere inside the encoder column, whether
    // above the encoder rows or below them. The wide default puts them
    // below, inside the right block, so it doubles as the positive control
    // -- a pass below means they MOVED, not that they were never there.
    const synth::ui::NodeTree wide = surface.BuildTree();
    const synth::ui::Bounds wideChrome = AbsoluteBounds(wide, synth_froggers::FroggersNodeIds::kLeftBlock);
    const synth::ui::Bounds wideGrid = AbsoluteBounds(wide, synth_froggers::FroggersNodeIds::kRightBlock);
    REQUIRE_TRUE(wideChrome.width > 0.0f && wideGrid.width > 0.0f);
    for (const synth_froggers::FroggersCellMap::ButtonCell& button :
         synth_froggers::FroggersCellMap::kRandomizeResetButtons) {
        const synth::ui::Bounds box = AbsoluteBounds(wide, button.id);
        REQUIRE_TRUE(box.width > 0.0f && box.height > 0.0f);
        REQUIRE_TRUE(FullyInside(box, wideGrid));
        REQUIRE_TRUE(!Overlaps(box, wideChrome));
    }
    // The wide split is the 2-of-6 / 4-of-6 one, i.e. the chrome block is
    // materially narrower. Recorded here because the narrow assertion below
    // is "these two are now the same width", which means nothing without a
    // measured before.
    REQUIRE_TRUE(wideChrome.width < wideGrid.width * 0.75f);

    // The browser shell reports its own viewport width through this action;
    // dispatching it here exercises the same path the shell takes rather
    // than reaching past it to the flag.
    surface.DispatchAction(
        synth::ui::Action::WithValue(synth_froggers::FroggersActions::kViewportNarrow, "1"));
    rig.RunBlocks(4);

    const synth::ui::NodeTree narrow = surface.BuildTree();
    const synth::ui::Bounds chrome = AbsoluteBounds(narrow, synth_froggers::FroggersNodeIds::kLeftBlock);
    const synth::ui::Bounds grid = AbsoluteBounds(narrow, synth_froggers::FroggersNodeIds::kRightBlock);
    REQUIRE_TRUE(chrome.width > 0.0f && grid.width > 0.0f);

    // The shell derives ONE scale from the grid block and applies it to
    // every stacked block, so equal design widths here are what makes the
    // chrome block span the viewport on a phone instead of half of it.
    REQUIRE_TRUE(std::fabs(chrome.width - grid.width) <= grid.width * 0.05f);

    const synth::ui::Bounds bpm = AbsoluteBounds(narrow, synth_froggers::FroggersNodeIds::kBpm);
    REQUIRE_TRUE(bpm.width > 0.0f);
    for (const synth_froggers::FroggersCellMap::ButtonCell& button :
         synth_froggers::FroggersCellMap::kRandomizeResetButtons) {
        const synth::ui::Bounds box = AbsoluteBounds(narrow, button.id);
        REQUIRE_TRUE(box.width > 0.0f && box.height > 0.0f);
        // Inside the chrome block and clear of the encoder column.
        REQUIRE_TRUE(FullyInside(box, chrome));
        REQUIRE_TRUE(!Overlaps(box, grid));
        // Beside the sliders, not above or below them.
        REQUIRE_TRUE(box.x + box.width * 0.5f > bpm.x + bpm.width * 0.5f);
        // Sized to the label rather than to a share of the block.
        REQUIRE_TRUE(box.width < chrome.width * 0.5f);

        // Moved, not copied: a shell that rearranged emitted controls would
        // leave the originals behind as well.
        std::size_t occurrences = 0;
        for (const synth::ui::Node& node : narrow.nodes) {
            if (node.id == button.id) {
                ++occurrences;
            }
        }
        REQUIRE_TRUE(occurrences == 1);
    }

    // The column's own box has to END at its last button, not run to the
    // bottom of the block. The browser shell reads exactly this box to find
    // where the space beside the sliders begins, and places Sheaf's runtime
    // sidebar there -- a block this surface cannot contain. A column that
    // filled the block would put that sidebar off the bottom of the chrome
    // block, where the mount's own clipping would hide it rather than show
    // it in the wrong place.
    const synth::ui::Bounds buttonColumn =
        AbsoluteBounds(narrow, synth_froggers::FroggersNodeIds::kLeftButtons);
    const synth::ui::Bounds lastButton = AbsoluteBounds(
        narrow, synth_froggers::FroggersCellMap::kRandomizeResetButtons.back().id);
    REQUIRE_TRUE(buttonColumn.height > 0.0f && lastButton.height > 0.0f);
    REQUIRE_TRUE(std::fabs((buttonColumn.y + buttonColumn.height) -
                           (lastButton.y + lastButton.height)) < 0.01f);
    // Four buttons and the three gaps between them, and nothing else.
    const float expectedColumnHeight =
        4.0f * lastButton.height + 3.0f * synth_froggers::FroggersPageLayout::kGap;
    REQUIRE_TRUE(std::fabs(buttonColumn.height - expectedColumnHeight) < 0.01f);
    // And it leaves room under it, inside the block, for what the shell puts
    // there. Compared against the chrome block rather than against a number,
    // so this stays meaningful if either extent is ever re-declared.
    REQUIRE_TRUE(buttonColumn.y + buttonColumn.height < chrome.y + chrome.height);

    const synth::ui::Bounds randomizePage =
        AbsoluteBounds(narrow, synth_froggers::FroggersNodeIds::kRandomizePage);
    std::cout << "  [narrow chrome] chrome width " << wideChrome.width << " -> " << chrome.width
              << " (grid " << wideGrid.width << " -> " << grid.width << "), Randomize Page "
              << randomizePage.width << "x" << randomizePage.height << " at x=" << randomizePage.x
              << ", button column " << buttonColumn.height << " tall leaving "
              << (chrome.y + chrome.height) - (buttonColumn.y + buttonColumn.height)
              << " under it\n";
}

TEST_CASE(reset_all_clears_values_and_neutralises_depths_end_to_end) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("reset_all_end_to_end"));
    rig.RunBlocks(4);

    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();
    synth::ui::Surface& surface = rig.Application().PortableSurface();

    // A fresh launch's own default patch is not all-zero (e.g. the Audio
    // bank's VCO shapes, its cross-VCO pitch detents, and Drive all start
    // away from zero) -- so the correct post-reset target is each
    // parameter's own default-patch value, not a literal 0.0. Snapshot it
    // here, right after construction and before anything dirties it, so the
    // comparisons below check against what a fresh launch actually shows,
    // not a hand-written expectation.
    std::vector<float> defaultValues;
    defaultValues.reserve(synth_froggers::kFroggersBankCount * synth_froggers::kFroggersParamsPerBank);
    for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
        for (std::size_t slot = 0; slot < synth_froggers::kFroggersParamsPerBank; ++slot) {
            defaultValues.push_back(model.PageParameter(bankIx, slot).SceneCenter(0));
        }
    }

    // Randomize first, so there is genuinely something to clear -- a reset
    // measured against a patch that never moved would prove nothing: the
    // instrument has to be live for the null result to mean anything.
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kRandomizeAll));
    rig.RunBlocks(8);

    float maxAbsDeltaBefore = 0.0f;
    {
        std::size_t ix = 0;
        for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
            for (std::size_t slot = 0; slot < synth_froggers::kFroggersParamsPerBank; ++slot) {
                maxAbsDeltaBefore = std::max(
                    maxAbsDeltaBefore, std::fabs(model.PageParameter(bankIx, slot).SceneCenter(0) - defaultValues[ix]));
                ++ix;
            }
        }
    }

    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kResetAll));
    rig.RunBlocks(8);

    float maxAbsDeltaAfter = 0.0f;
    {
        std::size_t ix = 0;
        for (std::size_t bankIx = 0; bankIx < synth_froggers::kFroggersBankCount; ++bankIx) {
            for (std::size_t slot = 0; slot < synth_froggers::kFroggersParamsPerBank; ++slot) {
                maxAbsDeltaAfter = std::max(
                    maxAbsDeltaAfter, std::fabs(model.PageParameter(bankIx, slot).SceneCenter(0) - defaultValues[ix]));
                ++ix;
            }
        }
    }

    std::cout << "  [reset end-to-end] max |value - default| across all 6 banks x 14 slots: after randomize="
              << maxAbsDeltaBefore << "  after reset=" << maxAbsDeltaAfter << "\n";
    REQUIRE_TRUE(maxAbsDeltaBefore > 0.05f);   // the randomize actually moved something away from default...
    REQUIRE_TRUE(maxAbsDeltaAfter < 1.0e-6f);  // ...and the reset brought every one of them back, through the real UI path.
}

TEST_CASE(exactly_two_randomize_controls_and_no_retired_controls_anywhere) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("randomize_inventory"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    const synth::ui::NodeTree tree = surface.BuildTree();

    std::size_t randomizeButtonCount = 0;
    for (const synth::ui::Node& node : tree.nodes) {
        if (node.kind != synth::ui::NodeKind::Button) {
            continue;
        }
        if (node.label.find("Randomize") != std::string::npos || node.label.find("Rand") != std::string::npos) {
            ++randomizeButtonCount;
        }
    }
    REQUIRE_TRUE(randomizeButtonCount == 2);
    REQUIRE_TRUE(HasButtonLabeled(tree, "Randomize All"));
    REQUIRE_TRUE(HasButtonLabeled(tree, "Randomize Page"));

    // Retired controls: "Rand waveforms" and "Rand Resample"
    // must not appear anywhere.
    REQUIRE_TRUE(!HasButtonLabeled(tree, "Rand waveforms"));
    REQUIRE_TRUE(!HasButtonLabeled(tree, "Rand Resample"));
}

// --- 10.2: Play and Stop exist and gate the transport -----------------------

TEST_CASE(play_and_stop_controls_exist_and_gate_the_transport) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("play_stop"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    const synth::ui::NodeTree tree = surface.BuildTree();
    // F.2b/F.2e (2026-08-03, Sheaf pin 77a3019e): Play/Stop are real
    // draw-command controls again -- a rounded plate plus a `Color::Green`
    // triangle (Play) / `Color::Red` square (Stop), via
    // `BuildPlayDrawCommands`/`BuildStopDrawCommands`
    // (FroggersUiSurface.hpp) -- replacing the EMOJI-glyph Button nodes
    // (UI-rework ITEM 4/B.4, 2026-07-29) that were themselves a workaround
    // for `Draw` nodes needing a double click and `Node` having no colour
    // field. Both causes are gone at this pin (plain click via
    // `ControlStyle::action`; a `Draw` node's own commands always carried
    // colour, which is why the emoji workaround chose emoji in the first
    // place -- see the removed comment's own reasoning). Assert by id: Draw
    // kind, drawCommands contains the plate then the coloured icon in the
    // real Sheaf colours (not a text/emoji substitute), the action on
    // `node.action` (not `doubleClickAction`, which this file never sets),
    // and a resolved square extent (Draw has no intrinsic size --
    // `layout.main`/`cross` were given explicitly, see BuildTree()'s own
    // comment -- so a collapsed 0x0 node would mean that wiring broke).
    const synth::ui::Node* playNode = FindNodeById(tree, synth_froggers::FroggersNodeIds::kPlay);
    const synth::ui::Node* stopNode = FindNodeById(tree, synth_froggers::FroggersNodeIds::kStop);
    REQUIRE_TRUE(playNode != nullptr);
    REQUIRE_TRUE(playNode->kind == synth::ui::NodeKind::Draw);
    REQUIRE_TRUE(playNode->bounds.width == synth_froggers::kTransportPlateSize);
    REQUIRE_TRUE(playNode->bounds.height == synth_froggers::kTransportPlateSize);
    REQUIRE_TRUE(playNode->drawCommands.size() == 2);
    REQUIRE_TRUE(playNode->drawCommands[0].kind == synth::ui::DrawCommand::Kind::FillRoundedRect);
    REQUIRE_TRUE(playNode->drawCommands[0].color == synth_froggers::kTransportPlateColor);
    REQUIRE_TRUE(playNode->drawCommands[1].kind == synth::ui::DrawCommand::Kind::FillPolygon);
    REQUIRE_TRUE(playNode->drawCommands[1].color == synth::Color::Green);
    REQUIRE_TRUE(playNode->action.has_value() &&
                 playNode->action->name == synth_froggers::FroggersActions::kPlay);
    REQUIRE_TRUE(!playNode->doubleClickAction.has_value());

    REQUIRE_TRUE(stopNode != nullptr);
    REQUIRE_TRUE(stopNode->kind == synth::ui::NodeKind::Draw);
    REQUIRE_TRUE(stopNode->bounds.width == synth_froggers::kTransportPlateSize);
    REQUIRE_TRUE(stopNode->bounds.height == synth_froggers::kTransportPlateSize);
    REQUIRE_TRUE(stopNode->drawCommands.size() == 2);
    REQUIRE_TRUE(stopNode->drawCommands[0].kind == synth::ui::DrawCommand::Kind::FillRoundedRect);
    REQUIRE_TRUE(stopNode->drawCommands[0].color == synth_froggers::kTransportPlateColor);
    REQUIRE_TRUE(stopNode->drawCommands[1].kind == synth::ui::DrawCommand::Kind::Fill);
    REQUIRE_TRUE(stopNode->drawCommands[1].color == synth::Color::Red);
    REQUIRE_TRUE(stopNode->action.has_value() &&
                 stopNode->action->name == synth_froggers::FroggersActions::kStop);
    REQUIRE_TRUE(!stopNode->doubleClickAction.has_value());

    // Non-default patch (the default patch is already applied at
    // Init()) + transport stopped (the rig's own default state) -> silent,
    // matching FroggersAudioRoutingTests.cpp's own
    // silent_while_transport_is_stopped -- re-asserted here specifically
    // through the surface's own Play/Stop actions.
    rig.RunBlocks(8);
    rig.ClearOutput();
    rig.RunBlocks(4);
    REQUIRE_TRUE(rig.OutputPeak() == 0.0f);

    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));
    rig.RunBlocks(8);
    rig.ClearOutput();
    rig.RunBlocks(8);
    REQUIRE_TRUE(rig.OutputPeak() > 0.0f);

    // Stop closes the ASR gate immediately (audioAdsr_.setGate(false) every
    // sample once transportQuarterNotes has no value -- FroggersAppCore::
    // ProcessBlock), but the Filter/Delay/Reverb chain's own decay tails
    // (self-oscillating comb feedback, reverb Hold) legitimately keep
    // ringing for a while after the gate closes -- that is correct DSP
    // behavior, not a surface-wiring defect, so this does not assert
    // instant re-silence the way the never-started case above does. What IS
    // asserted: Stop is dispatchable through the surface without fault, and
    // sending it stops the gate from admitting further NEW energy -- the
    // playing peak observed immediately after Play (above) is not
    // re-verified to persist or grow once Stop has been sent.
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kStop));
    rig.RunBlocks(4);
}

// --- Freeze button ----------------------------------------------------
//
// A third Draw node beside Stop. Latching it OVERRIDES the Freeze encoder's
// own clamp rather than merely shortcutting the encoder to its
// clamped maximum -- see dsp/Delay.hpp's DelayParams::dfrzLatched comment
// and FroggersAppCore.hpp's own wiring comment for the full mapping.

TEST_CASE(transport_row_has_freeze_as_third_child_beside_play_and_stop) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("freeze_row_children"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    const synth::ui::NodeTree tree = surface.BuildTree();

    const synth::ui::Node* rowNode = FindNodeById(tree, synth_froggers::FroggersNodeIds::kTransportRow);
    REQUIRE_TRUE(rowNode != nullptr);
    // Now 4 (Play, Stop, Freeze, Record) -- see
    // transport_row_has_record_as_fourth_child_beside_play_stop_and_freeze
    // below for the dedicated Record-child assertion; this test still only
    // re-checks the three buttons it originally covered.
    REQUIRE_TRUE(rowNode->children.size() == 4);
    REQUIRE_TRUE(rowNode->children[0].value == synth_froggers::FroggersNodeIds::kPlay);
    REQUIRE_TRUE(rowNode->children[1].value == synth_froggers::FroggersNodeIds::kStop);
    REQUIRE_TRUE(rowNode->children[2].value == synth_froggers::FroggersNodeIds::kFreeze);

    const char* transportIds[3] = {synth_froggers::FroggersNodeIds::kPlay, synth_froggers::FroggersNodeIds::kStop,
                                    synth_froggers::FroggersNodeIds::kFreeze};
    for (const char* id : transportIds) {
        const synth::ui::Node* node = FindNodeById(tree, id);
        REQUIRE_TRUE(node != nullptr);
        REQUIRE_TRUE(node->kind == synth::ui::NodeKind::Draw);
        REQUIRE_TRUE(node->bounds.width == synth_froggers::kTransportPlateSize);
        REQUIRE_TRUE(node->bounds.height == synth_froggers::kTransportPlateSize);
    }

    const synth::ui::Node* freezeNode = FindNodeById(tree, synth_froggers::FroggersNodeIds::kFreeze);
    REQUIRE_TRUE(freezeNode->action.has_value() &&
                 freezeNode->action->name == synth_froggers::FroggersActions::kFreeze);
    REQUIRE_TRUE(!freezeNode->doubleClickAction.has_value());
}

TEST_CASE(freeze_action_toggles_the_latch_and_a_second_click_releases_it) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("freeze_toggle_latch"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    REQUIRE_TRUE(!rig.Application().FreezeLatched());  // default: unlatched.

    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kFreeze));
    REQUIRE_TRUE(rig.Application().FreezeLatched());  // one click latches.

    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kFreeze));
    REQUIRE_TRUE(!rig.Application().FreezeLatched());  // a second click releases.
}

// A pure function of (bounds, latched) -- no rig needed. Asserts the
// EXCHANGE, not merely that the two command lists differ (a brightness
// tweak would also satisfy "differ", which is exactly the thing this
// control exists to avoid).
TEST_CASE(freeze_draw_commands_genuinely_invert_plate_and_glyph_colours) {
    const synth::ui::Bounds bounds{0.0f, 0.0f, synth_froggers::kTransportPlateSize,
                                    synth_froggers::kTransportPlateSize};
    const std::vector<synth::ui::DrawCommand> unlatched = synth_froggers::BuildFreezeDrawCommands(bounds, false);
    const std::vector<synth::ui::DrawCommand> latched = synth_froggers::BuildFreezeDrawCommands(bounds, true);

    REQUIRE_TRUE(unlatched.size() == 2);
    REQUIRE_TRUE(latched.size() == 2);
    REQUIRE_TRUE(unlatched[0].kind == synth::ui::DrawCommand::Kind::FillRoundedRect);
    REQUIRE_TRUE(unlatched[1].kind == synth::ui::DrawCommand::Kind::FillPolygon);
    REQUIRE_TRUE(latched[0].kind == synth::ui::DrawCommand::Kind::FillRoundedRect);
    REQUIRE_TRUE(latched[1].kind == synth::ui::DrawCommand::Kind::FillPolygon);

    REQUIRE_TRUE(unlatched[0].color == synth_froggers::kTransportPlateColor);
    REQUIRE_TRUE(unlatched[1].color == synth::Color::Cyan);
    REQUIRE_TRUE(latched[0].color == synth::Color::Cyan);
    REQUIRE_TRUE(latched[1].color == synth_froggers::kTransportPlateColor);
    // The exchange itself, asserted directly: latched's plate is unlatched's
    // glyph colour and vice versa.
    REQUIRE_TRUE(latched[0].color == unlatched[1].color);
    REQUIRE_TRUE(latched[1].color == unlatched[0].color);
    REQUIRE_TRUE(unlatched[0].color != unlatched[1].color);  // the two colours are genuinely distinct to begin with.
}

// Reused by freeze_latched_grows_the_recirculating_level_beyond_unlatched_end_to_end
// below at every measurement point (called repeatedly,
// isolates a distinct transformation stage, prevents repeating the same
// per-block sampling loop). Deliberately NOT dsp::StereoDelay::StateMagnitude()
// (used elsewhere in this file): that scans the WHOLE lineL/lineR buffers
// (96000 samples at 48kHz -- StereoDelay::kMaxDelaySamples, dsp/Delay.hpp),
// but at dtim==0 the ACTIVE round trip only touches the ~48 samples right
// behind the write head each cycle, so a measurement window far shorter
// than the full 96000-sample capacity leaves the rest of the buffer
// (including loud content from priming, long before divergence) unrevisited
// and StateMagnitude() keeps reporting that stale max regardless of what
// the active loop is currently doing -- confirmed empirically: an earlier
// version of this test measured StateMagnitude() and read the SAME primed
// value back for the unlatched rig no matter how it should have decayed.
// GetLastWet() is the delay's own most-recently-computed wet output --
// exactly "the recirculating level" this test needs, immune to stale
// history elsewhere in the buffer. Sampled once per block across `blocks`
// blocks (not one `RunBlocks(blocks)` call) so a single unlucky
// near-zero-crossing sample cannot be mistaken for the level.
float PeakDelayWetMagnitude(synth_rig::SynthRig<synth_froggers::FroggersApp>& rig, std::size_t blocks) {
    float peak = 0.0f;
    for (std::size_t i = 0; i < blocks; ++i) {
        rig.RunBlocks(1);
        const auto wet = rig.Application().TestDelay().GetLastWet();
        peak = std::max(peak, std::max(std::fabs(wet.l), std::fabs(wet.r)));
    }
    return peak;
}

// Reused by freeze_latched_grows_the_recirculating_level_beyond_unlatched_end_to_end
// below for BOTH its rigs (2 uses, isolates the "apply
// a real patch and build up real recirculating energy" transformation stage)
// -- so the only thing that can differ between the two rigs afterward is
// whether SetFreezeLatched(true) was ever called on one of them. Returns
// the peak wet level reached during priming (a positive control for
// the caller: proof the instrument was actually live before divergence).
float ApplyFreezeEndToEndPatchAndPrime(synth_rig::SynthRig<synth_froggers::FroggersApp>& rig) {
    synth_froggers::FroggersParameterModel& model = rig.Application().Parameters();

    // A real, moderate input signal -- same idiom as
    // FroggersAudioRoutingTests.cpp's own
    // stopping_transport_silences_self_sustaining_delay_and_reverb ("nonzero
    // VCO level, so there's signal to excite the tanks").
    model.PageParameter(synth_froggers::FroggersBankId::Audio, 0).SceneCenter(0) = 0.5f;  // VCO1 level.
    model.PageParameter(synth_froggers::FroggersBankId::Drive, 0).SceneCenter(0) = 0.8f;  // Drive.

    // Delay bank, rows per dsp::MapRowsToDelayParams's own comment
    // (dsp/Delay.hpp): 0=Time, 1=Send, 2=Feedback, 3=Width, 4=Freeze,
    // 5=Mod, 9=Feedback Drive. Feedback Drive stays at its own neutral
    // (knob 0.5 -> fbDrive==1.0, dsp::StereoDelay::SetFeedbackDrive's own
    // comment) during priming -- only raised to "above centre" at the
    // moment of divergence, alongside the Freeze encoder, matching this
    // test's own header comment.
    model.PageParameter(synth_froggers::FroggersBankId::Delay, 0).SceneCenter(0) = 0.0f;  // Time -> shortest round trip.
    model.PageParameter(synth_froggers::FroggersBankId::Delay, 1).SceneCenter(0) = 1.0f;  // Send.
    model.PageParameter(synth_froggers::FroggersBankId::Delay, 2).SceneCenter(0) = 0.6f;  // Feedback.
    model.PageParameter(synth_froggers::FroggersBankId::Delay, 3).SceneCenter(0) = 0.0f;  // Width -> no cross-feed smear.
    model.PageParameter(synth_froggers::FroggersBankId::Delay, 5).SceneCenter(0) = 0.0f;  // Mod -> no LFO smear.
    model.PageParameter(synth_froggers::FroggersBankId::Delay, 9).SceneCenter(0) = 0.5f;  // Feedback Drive, neutral for now.

    rig.StartAt(0);
    // Freeze encoder stays at its own default (0) throughout priming:
    // freezeEff==1 blocks ALL new input the instant dfrz
    // reaches 1, latched or not (dsp/Delay.hpp) -- priming needs real
    // signal actually entering the line.
    constexpr std::size_t kPrimeBlocks = 40;
    return PeakDelayWetMagnitude(rig, kPrimeBlocks);
}

// "This is the test that proves the override reached the DSP through the
// real path rather than the DSP behaving correctly in isolation"
// -- unlike FroggersDspParityTests.cpp's own freeze-latch tests
// (e.g. stereo_delay_freeze_latched_grows_the_loop_measurably_beyond_unlatched_full_freeze,
// which drives a bare dsp::StereoDelay/DelayParams by hand), this drives
// audio through the real FroggersAppCore::ProcessBlock/RouteAudioSample
// path via two independent, identically-primed rigs that diverge only in
// whether SetFreezeLatched(true) was called.
TEST_CASE(freeze_latched_grows_the_recirculating_level_beyond_unlatched_end_to_end) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> latchedRig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("freeze_e2e_latched"));
    synth_rig::SynthRig<synth_froggers::FroggersApp> unlatchedRig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("freeze_e2e_unlatched"));

    // Positive control: confirm priming actually put a real,
    // non-negligible level into both delay lines before trusting anything
    // measured after divergence -- a null result out of a dead instrument
    // would be void, not negative.
    const float primedLatched = ApplyFreezeEndToEndPatchAndPrime(latchedRig);
    const float primedUnlatched = ApplyFreezeEndToEndPatchAndPrime(unlatchedRig);
    REQUIRE_TRUE(primedLatched > 1.0e-4f);
    REQUIRE_TRUE(primedUnlatched > 1.0e-4f);

    // Divergence: BOTH rigs get the SAME driving quantities -- Feedback
    // Drive above centre and the Freeze encoder at maximum (this test's own
    // required setting) -- applied identically to
    // both. Only the BUTTON differs: latchedRig's is pressed, unlatchedRig's
    // never is (stays false, the default).
    synth_froggers::FroggersParameterModel& latchedModel = latchedRig.Application().Parameters();
    latchedModel.PageParameter(synth_froggers::FroggersBankId::Delay, 9).SceneCenter(0) =
        1.0f;  // Feedback Drive -> 4.0, above centre.
    latchedModel.PageParameter(synth_froggers::FroggersBankId::Delay, 4).SceneCenter(0) = 1.0f;  // Freeze encoder, max.
    latchedRig.Application().SetFreezeLatched(true);  // the override under test.

    synth_froggers::FroggersParameterModel& unlatchedModel = unlatchedRig.Application().Parameters();
    unlatchedModel.PageParameter(synth_froggers::FroggersBankId::Delay, 9).SceneCenter(0) =
        1.0f;  // SAME Feedback Drive.
    unlatchedModel.PageParameter(synth_froggers::FroggersBankId::Delay, 4).SceneCenter(0) =
        1.0f;  // SAME Freeze encoder.
    // unlatchedRig.Application().SetFreezeLatched(...) is never called --
    // The clamped encoder value prevails instead.

    constexpr std::size_t kMeasureBlocks = 16;
    const float latchedLevel = PeakDelayWetMagnitude(latchedRig, kMeasureBlocks);
    const float unlatchedLevel = PeakDelayWetMagnitude(unlatchedRig, kMeasureBlocks);

    REQUIRE_TRUE(!latchedRig.SawNaN());
    REQUIRE_TRUE(!unlatchedRig.SawNaN());
    REQUIRE_TRUE(std::isfinite(latchedLevel) && std::isfinite(unlatchedLevel));

    std::cout << "  [OBSERVED] Feedback Drive knob=1.0 (fbDrive=4.0), Freeze encoder=1.0 (max) in both "
                 "rigs; primed recirculating level latched-rig="
              << primedLatched << " unlatched-rig=" << primedUnlatched << "; swept to, after " << kMeasureBlocks
              << " post-divergence blocks: latched=" << latchedLevel << " unlatched=" << unlatchedLevel << "\n";

    // Retuned here. This asserted `> 2x`, a threshold that only made sense
    // under the retired product-clamp mapping (latched 4.0 vs unlatched
    // 1.0). Freeze's mapping no longer mentions fbDrive
    // (dsp::StereoDelay::FreezeFeedback), so both branches are multiplied by
    // the SAME fbDrive afterward and the ratio between them is
    // kFreezeLatchOverdrive / 1.0 -- 1.05 -- at every Drive setting.
    // Keeping `> 2x` would assert the old coupling back into existence.
    //
    // What this test is FOR is the end-to-end wire: that the transport
    // latch reaches the DSP through the real path at all. That is a
    // strictly-greater relation, and the level difference compounds over
    // the measured blocks, so the margin is comfortably above float noise
    // without pinning a ratio the operator may retune by ear.
    REQUIRE_TRUE(latchedLevel > unlatchedLevel);
    std::cout << "  [OBSERVED] latched/unlatched level ratio="
              << (latchedLevel / unlatchedLevel) << "\n";
}

// -----------------------------------------------------------------------
// Regression test for the real-Runtime "Play produces no audio" bug
// (diagnosed while investigating a report that the app was silent in the
// real `sheaf-patch` launcher despite all headless tests passing).
//
// Root cause: `synth::Engine<App>::Prepare()` calls
// `MasterClock::Prepare()` unconditionally
// (External/Sheaf/projects/synth/src/MasterClock.cpp:929 resets
// `transportState_` to `Stopped`) BEFORE calling this app's
// `PrepareToPlay()` hook. `synth::Engine::Prepare()` is not a one-time
// startup call in the real app: `synth_runtime::Runtime<App>::
// audioDeviceAboutToStart` (External/Sheaf/projects/synth/runtime/
// Runtime.hpp) re-invokes it on EVERY audio-device renegotiation --
// verified against a real session's own log
// (~/Library/Sheaf/synth/sheaf-patch/logs/): three separate "Audio
// prepared" lines fired before any user interaction at all during one
// real run, one of them renegotiating the block size from 256 to 512
// frames, and three more after the user manually switched output devices
// while troubleshooting. Every one of those silently re-closes the
// transport-gated ASR with no visual indication and no automatic
// recovery -- a user who pressed Play and then hit (or caused) any such
// renegotiation gets permanent silence.
//
// No previous test ever drove this: every existing audio-producing test
// (this file's own play_and_stop_controls_exist_and_gate_the_transport
// included) starts the transport once and never calls Engine::Prepare()
// again. This test reproduces the exact real-world sequence -- dispatch
// Play through the real FroggersUiSurface (not rig.StartAt), THEN call
// synth::Engine::Prepare() a second time (exactly what
// audioDeviceAboutToStart does on a device renegotiation) -- at the
// exact sample rate/block size the real device negotiated in that
// session (44100 Hz / 512 frames), and asserts audio survives it.
// -----------------------------------------------------------------------
TEST_CASE(transport_survives_audio_device_reprepare_after_play) {
    synth_rig::SynthRig<synth_froggers::FroggersApp>::AudioSettings realDeviceSettings;
    realDeviceSettings.sampleRate = 44100.0;
    realDeviceSettings.blockSize = 512;
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("reprepare_after_play"), realDeviceSettings);
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();

    // Same real Play path as play_and_stop_controls_exist_and_gate_the_transport.
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));
    rig.RunBlocks(8);
    rig.ClearOutput();
    rig.RunBlocks(8);
    REQUIRE_TRUE(rig.OutputPeak() > 0.0f);
    REQUIRE_TRUE(rig.Engine().Clock().TransportState() == synth::ClockTransportState::Running);

    // The event no test has ever driven: a benign audio-device
    // renegotiation, exactly synth_runtime::Runtime<App>::
    // audioDeviceAboutToStart's own call (Runtime.hpp), at the SAME
    // sample rate/block size (a device switch, not a rate change).
    rig.Engine().Prepare(realDeviceSettings.sampleRate, realDeviceSettings.blockSize);

    // Before the fix: MasterClock::Prepare() alone resets TransportState()
    // to Stopped here and the app stays silent forever with no further
    // user action able to recover it (short of pressing Play again, which
    // the real UI gives the user no reason to do -- nothing indicates the
    // transport ever stopped). After the fix: FroggersAppCore::
    // PrepareToPlay() re-asserts the user's last explicit Play request.
    rig.ClearOutput();
    rig.RunBlocks(8);
    REQUIRE_TRUE(rig.OutputPeak() > 0.0f);
    REQUIRE_TRUE(rig.Engine().Clock().TransportState() == synth::ClockTransportState::Running);

    // Randomize actually changes the output (part of the reported
    // symptom: "randomizing changes nothing"), even across this
    // renegotiation.
    const float peakBeforeRandomize = rig.OutputPeak();
    (void)peakBeforeRandomize;
    std::vector<float> samplesBefore;
    for (const auto& frame : rig.Output()) {
        samplesBefore.insert(samplesBefore.end(), frame.channels.begin(), frame.channels.end());
    }
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kRandomizeAll));
    rig.RunBlocks(4);
    rig.ClearOutput();
    rig.RunBlocks(8);
    REQUIRE_TRUE(rig.OutputPeak() > 0.0f);
    std::vector<float> samplesAfter;
    for (const auto& frame : rig.Output()) {
        samplesAfter.insert(samplesAfter.end(), frame.channels.begin(), frame.channels.end());
    }
    REQUIRE_TRUE(samplesBefore.size() == samplesAfter.size());
    bool anyDifference = false;
    for (std::size_t ix = 0; ix < samplesBefore.size(); ++ix) {
        if (std::fabs(samplesBefore[ix] - samplesAfter[ix]) > 1.0e-6f) {
            anyDifference = true;
            break;
        }
    }
    REQUIRE_TRUE(anyDifference);

    // An explicit Stop's intent must also survive a reprepare -- the fix
    // must not turn the transport into an unstoppable always-on gate.
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kStop));
    rig.RunBlocks(4);
    rig.Engine().Prepare(realDeviceSettings.sampleRate, realDeviceSettings.blockSize);
    REQUIRE_TRUE(rig.Engine().Clock().TransportState() == synth::ClockTransportState::Stopped);
}

// --- Record capture, app core only -----------------------------
//
// FroggersAppCore's own bounded mono capture buffer -- arm/stop API, refusal
// with a reason while the transport is stopped, truncation at capacity. No
// UI/JUCE/file-writing involved (covered separately below); these tests drive the
// core's public API directly through rig.Application(), same convention as
// the other runtime tests in this file.

TEST_CASE(record_arm_refuses_when_transport_stopped) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("record_refusal"));
    rig.RunBlocks(4);

    synth_froggers::FroggersApp& app = rig.Application();
    // Rig's own default state: transport stopped, never Played.
    REQUIRE_TRUE(!app.ArmRecording());
    REQUIRE_TRUE(app.RecordRefusalReason() != nullptr);
    REQUIRE_TRUE(std::string(app.RecordRefusalReason()) == "Press Play before recording.");

    rig.RunBlocks(8);
    REQUIRE_TRUE(app.RecordedFrameCount() == 0);
}

TEST_CASE(record_captures_audio_while_playing) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("record_capture"));
    rig.RunBlocks(4);

    // Same real Play path as play_and_stop_controls_exist_and_gate_the_transport above.
    synth::ui::Surface& surface = rig.Application().PortableSurface();
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));
    rig.RunBlocks(8);

    synth_froggers::FroggersApp& app = rig.Application();
    // Generous override (well above anything this test could run past) so
    // the capture-while-playing measurement below cannot itself truncate --
    // truncation is a separate, dedicated test below.
    REQUIRE_TRUE(app.ArmRecording(/*capacityFramesOverride=*/200000));
    REQUIRE_TRUE(app.RecordRefusalReason() == nullptr);

    rig.ClearOutput();
    rig.RunBlocks(8);
    const std::uint64_t framesTransportRan = static_cast<std::uint64_t>(rig.Output().size());

    REQUIRE_TRUE(app.RecordedFrameCount() > 0);
    REQUIRE_TRUE(app.RecordedFrameCount() == framesTransportRan);
    REQUIRE_TRUE(!app.RecordingTruncated());

    // A silent capture proves nothing -- report the max |sample|
    // actually observed, not just that the count moved.
    float maxSample = 0.0f;
    for (float s : app.RecordedAudio()) {
        maxSample = std::max(maxSample, std::fabs(s));
    }
    std::cout << "  [capture] frames captured=" << app.RecordedFrameCount()
              << "  max|sample|=" << maxSample << "\n";
    REQUIRE_TRUE(maxSample > 0.0f);

    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kStop));
    rig.RunBlocks(4);
}

TEST_CASE(record_truncates_at_capacity_and_stops_growing) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("record_truncation"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));
    rig.RunBlocks(8);

    synth_froggers::FroggersApp& app = rig.Application();
    constexpr std::uint64_t kCapacityFrames = 256;
    REQUIRE_TRUE(app.ArmRecording(/*capacityFramesOverride=*/static_cast<std::size_t>(kCapacityFrames)));

    // Run well past the tiny capacity (FroggersAppCore::Config()'s
    // preferredBlockSize is 256, so a single block already fills it exactly
    // without yet flagging truncation -- the flag only sets on the first
    // frame that finds the buffer already full, i.e. early in the next
    // block; 16 blocks is a wide margin past that).
    rig.RunBlocks(16);
    REQUIRE_TRUE(app.RecordedFrameCount() == kCapacityFrames);
    REQUIRE_TRUE(app.RecordingTruncated());

    // Further blocks must not grow the count -- capture has stopped.
    rig.RunBlocks(8);
    REQUIRE_TRUE(app.RecordedFrameCount() == kCapacityFrames);

    std::cout << "  [truncation] capacity=" << kCapacityFrames
              << "  recordedFrameCount=" << app.RecordedFrameCount()
              << "  truncated=" << (app.RecordingTruncated() ? "true" : "false") << "\n";

    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kStop));
    rig.RunBlocks(4);
}

// --- Record button + WAV export --------------------------------
//
// The transport-row Record button (fourth Draw child beside Play/Stop/
// Freeze), its HandleAction wiring to ArmRecording()/StopRecording() plus
// the surface's own transport notice (transportNotice_, shown as a Label
// beside the Record plate) and the core's queued file export
// (QueueRecordingExport/TakePendingFileExport, FroggersAppCore.hpp), and
// the pure std:: WAV encoder (EncodeWavPcm16Mono), moved into the core
// specifically so it is testable headlessly here -- no binary this
// Makefile builds compiles FroggersMain.cpp (verified by reading
// app/Makefile: every target's source list stops at Main.cpp/
// FroggersHeadlessTests.cpp/.../FroggersSurfaceTests.cpp -- FroggersMain.cpp
// appears nowhere in it; it is only ever compiled by app/build-launcher.sh's
// separate `make -C .../sheaf-patch APP_SOURCES=.../FroggersMain.cpp` call).

TEST_CASE(transport_row_has_record_as_fourth_child_beside_play_stop_and_freeze) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("record_row_children"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    const synth::ui::NodeTree tree = surface.BuildTree();

    const synth::ui::Node* rowNode = FindNodeById(tree, synth_froggers::FroggersNodeIds::kTransportRow);
    REQUIRE_TRUE(rowNode != nullptr);
    REQUIRE_TRUE(rowNode->children.size() == 4);
    REQUIRE_TRUE(rowNode->children[0].value == synth_froggers::FroggersNodeIds::kPlay);
    REQUIRE_TRUE(rowNode->children[1].value == synth_froggers::FroggersNodeIds::kStop);
    REQUIRE_TRUE(rowNode->children[2].value == synth_froggers::FroggersNodeIds::kFreeze);
    REQUIRE_TRUE(rowNode->children[3].value == synth_froggers::FroggersNodeIds::kRecord);

    const synth::ui::Node* recordNode = FindNodeById(tree, synth_froggers::FroggersNodeIds::kRecord);
    REQUIRE_TRUE(recordNode != nullptr);
    REQUIRE_TRUE(recordNode->kind == synth::ui::NodeKind::Draw);
    REQUIRE_TRUE(recordNode->bounds.width == synth_froggers::kTransportPlateSize);
    REQUIRE_TRUE(recordNode->bounds.height == synth_froggers::kTransportPlateSize);
    REQUIRE_TRUE(recordNode->action.has_value() &&
                 recordNode->action->name == synth_froggers::FroggersActions::kRecord);
    REQUIRE_TRUE(!recordNode->doubleClickAction.has_value());
}

TEST_CASE(record_action_arms_while_playing_and_stops_with_captured_frames) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("record_action_toggle"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    synth_froggers::FroggersApp& app = rig.Application();

    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));
    rig.RunBlocks(8);

    REQUIRE_TRUE(!app.RecordArmed());  // default: not armed.
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kRecord));
    REQUIRE_TRUE(app.RecordArmed());  // one dispatch arms it.

    rig.RunBlocks(8);
    REQUIRE_TRUE(app.RecordedFrameCount() > 0);

    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kRecord));
    REQUIRE_TRUE(!app.RecordArmed());            // a second dispatch stops it.
    REQUIRE_TRUE(app.RecordedFrameCount() > 0);  // captured data survives the stop.

    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kStop));
    rig.RunBlocks(4);
}

TEST_CASE(record_action_refused_while_stopped_shows_the_transport_notice) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("record_refusal_notice"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    synth_froggers::FroggersApp& app = rig.Application();

    // Before the Record dispatch, the built tree carries no notice node at
    // all -- not merely an empty one.
    const synth::ui::NodeTree beforeTree = surface.BuildTree();
    REQUIRE_TRUE(FindNodeById(beforeTree, synth_froggers::FroggersNodeIds::kTransportNotice) == nullptr);

    // With no notice, the four plates' resolved bounds are exactly what
    // they were on `main` before AppendTransportRow's cell became a Column
    // (froggers.layout.left.transport.stack) wrapping the plates Row over
    // the notice line -- read from that pre-Column build at the same
    // default 900x712 config SynthRig itself uses (FroggersAppCore::
    // Config()), so the restructure moved nothing the operator can see.
    const synth::ui::Bounds playBoundsBefore =
        AbsoluteBounds(beforeTree, synth_froggers::FroggersNodeIds::kPlay);
    const synth::ui::Bounds stopBoundsBefore =
        AbsoluteBounds(beforeTree, synth_froggers::FroggersNodeIds::kStop);
    const synth::ui::Bounds freezeBoundsBefore =
        AbsoluteBounds(beforeTree, synth_froggers::FroggersNodeIds::kFreeze);
    const synth::ui::Bounds recordBoundsBefore =
        AbsoluteBounds(beforeTree, synth_froggers::FroggersNodeIds::kRecord);
    REQUIRE_TRUE(playBoundsBefore.x == 16.0f && playBoundsBefore.y == 238.0f &&
                 playBoundsBefore.width == 28.0f && playBoundsBefore.height == 28.0f);
    REQUIRE_TRUE(stopBoundsBefore.x == 58.0f && stopBoundsBefore.y == 238.0f &&
                 stopBoundsBefore.width == 28.0f && stopBoundsBefore.height == 28.0f);
    REQUIRE_TRUE(freezeBoundsBefore.x == 100.0f && freezeBoundsBefore.y == 238.0f &&
                 freezeBoundsBefore.width == 28.0f && freezeBoundsBefore.height == 28.0f);
    REQUIRE_TRUE(recordBoundsBefore.x == 142.0f && recordBoundsBefore.y == 238.0f &&
                 recordBoundsBefore.width == 28.0f && recordBoundsBefore.height == 28.0f);

    // Rig's own default state: transport stopped, never Played.
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kRecord));
    REQUIRE_TRUE(!app.RecordArmed());

    const synth::ui::NodeTree refusedTree = surface.BuildTree();
    const synth::ui::Node* noticeNode =
        FindNodeById(refusedTree, synth_froggers::FroggersNodeIds::kTransportNotice);
    REQUIRE_TRUE(noticeNode != nullptr);
    REQUIRE_TRUE(noticeNode->kind == synth::ui::NodeKind::Label);
    REQUIRE_TRUE(noticeNode->text == "Press Play before recording.");

    // The notice's resolved (absolute) bounds lie inside the transport
    // stack's (froggers.layout.left.transport.stack, the Column wrapping
    // the plates Row and the notice line -- not the plates Row itself,
    // which the notice sits BELOW as the stack's second child), in this
    // wide-layout default (narrowViewport_ defaults false).
    const synth::ui::Bounds noticeBounds =
        AbsoluteBounds(refusedTree, synth_froggers::FroggersNodeIds::kTransportNotice);
    const synth::ui::Bounds transportStackBounds =
        AbsoluteBounds(refusedTree, synth_froggers::FroggersNodeIds::kTransportStack);
    REQUIRE_TRUE(FullyInside(noticeBounds, transportStackBounds));

    // A Play dispatch clears the notice -- the node is gone again.
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));
    const synth::ui::NodeTree afterPlayTree = surface.BuildTree();
    REQUIRE_TRUE(FindNodeById(afterPlayTree, synth_froggers::FroggersNodeIds::kTransportNotice) == nullptr);

    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kStop));
    rig.RunBlocks(4);
}

TEST_CASE(record_action_stop_with_data_queues_one_named_wav_export) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("record_wav_export"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    synth_froggers::FroggersApp& app = rig.Application();

    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));
    rig.RunBlocks(8);
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kRecord));
    rig.RunBlocks(8);
    REQUIRE_TRUE(app.RecordedFrameCount() > 0);

    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kRecord));

    const std::optional<synth::FileExport> fileExport = app.TakePendingFileExport();
    REQUIRE_TRUE(fileExport.has_value());
    REQUIRE_TRUE(fileExport->mediaType == "audio/wav");
    REQUIRE_TRUE(fileExport->bytes.size() >= 4 && fileExport->bytes[0] == 'R' && fileExport->bytes[1] == 'I' &&
                 fileExport->bytes[2] == 'F' && fileExport->bytes[3] == 'F');
    // EncodeWavPcm16Mono's canonical 44-byte header, 16-bit mono samples.
    REQUIRE_TRUE(fileExport->bytes.size() >= 44);
    REQUIRE_TRUE((fileExport->bytes.size() - 44) / 2 == app.RecordedFrameCount());
    REQUIRE_TRUE(fileExport->note.empty());

    static const std::regex kFileNamePattern("^\\d{4}-\\d{2}-\\d{2}\\.wav$");
    std::cout << "  [wav export] fileName=" << fileExport->fileName << "\n";
    REQUIRE_TRUE(std::regex_match(fileExport->fileName, kFileNamePattern));

    // A second take, with no further Record stop, yields none.
    REQUIRE_TRUE(!app.TakePendingFileExport().has_value());

    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kStop));
    rig.RunBlocks(4);
}

TEST_CASE(truncated_capture_queues_its_export_without_a_stop_press) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("record_wav_export_truncated"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));
    rig.RunBlocks(8);

    synth_froggers::FroggersApp& app = rig.Application();
    constexpr std::uint64_t kCapacityFrames = 256;

    // Observe the export the way a real host does -- through the engine's
    // own per-tick drain (Engine::MessageThreadTick, Engine.hpp:622-636),
    // which hands every queued export to whatever handler is installed here
    // and drops it otherwise. Installed before arming so the drain that
    // follows the cap has somewhere to deliver it.
    std::vector<synth::FileExport> exportedFiles;
    rig.Engine().SetFileExportHandler(
        [&exportedFiles](synth::FileExport fileExport) { exportedFiles.push_back(std::move(fileExport)); });

    REQUIRE_TRUE(app.ArmRecording(/*capacityFramesOverride=*/static_cast<std::size_t>(kCapacityFrames)));

    // Run well past the tiny capacity, same margin
    // record_truncates_at_capacity_and_stops_growing above uses. The audio
    // thread disarms itself the instant it hits the cap (FroggersAppCore::
    // ProcessBlock, :1186-1194) -- no Record dispatch follows here, so the
    // handler above is only ever reached through the engine's own per-tick
    // TakePendingFileExport() poll (Engine.hpp:628), driven automatically by
    // every RunBlocks() call's MessageThreadTick.
    rig.RunBlocks(16);
    REQUIRE_TRUE(app.RecordedFrameCount() == kCapacityFrames);
    REQUIRE_TRUE(app.RecordingTruncated());
    REQUIRE_TRUE(!app.RecordArmed());

    REQUIRE_TRUE(exportedFiles.size() == 1);
    const synth::FileExport& fileExport = exportedFiles.front();
    REQUIRE_TRUE(fileExport.note == "stopped at the 30-minute limit");
    REQUIRE_TRUE(fileExport.mediaType == "audio/wav");
    REQUIRE_TRUE(fileExport.bytes.size() >= 44);
    REQUIRE_TRUE((fileExport.bytes.size() - 44) / 2 == kCapacityFrames);

    static const std::regex kFileNamePattern("^\\d{4}-\\d{2}-\\d{2}\\.wav$");
    std::cout << "  [truncated export via engine tick] fileName=" << fileExport.fileName << "\n";
    REQUIRE_TRUE(std::regex_match(fileExport.fileName, kFileNamePattern));

    // Further ticks, with nothing new queued, must not hand the same
    // capture to the handler a second time.
    rig.RunBlocks(8);
    REQUIRE_TRUE(exportedFiles.size() == 1);

    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kStop));
    rig.RunBlocks(4);
}

TEST_CASE(arming_after_an_unpolled_truncated_capture_flushes_it_first) {
    synth_rig::SynthRig<synth_froggers::FroggersApp> rig(
        /*patchPumpBudgetBlocks=*/64, UseScratchRuntimeDataPaths("record_unpolled_truncation_flush"));
    rig.RunBlocks(4);

    synth::ui::Surface& surface = rig.Application().PortableSurface();
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));
    // One ticked block is enough to land the transport-running state the
    // direct, un-ticked blocks below need -- no further tick may run before
    // the capture is filled (see that loop's own comment).
    rig.RunBlocks(1);

    synth_froggers::FroggersApp& app = rig.Application();
    constexpr std::uint64_t kCapacityFrames = 256;
    REQUIRE_TRUE(app.ArmRecording(/*capacityFramesOverride=*/static_cast<std::size_t>(kCapacityFrames)));

    // Truncate the capture without ever ticking the message thread.
    // RunBlocks()/RunOneBlockAt() always pair a ProcessBlock with a
    // MessageThreadTick (SynthRig.hpp:~520-540), and that tick alone would
    // drain and drop the truncated export (no handler installed in this
    // test) before ArmRecording()'s own flush below ever gets a chance to
    // run. synth::Engine<App>::ProcessBlock() -- the call RunOneBlockAt
    // makes, minus the tick that follows it there -- is public and reachable
    // via rig.Engine(); the block it needs is built here from
    // FroggersApp::Config() rather than the rig's own block buffers, which
    // are private.
    const synth::RuntimeConfig config = synth_froggers::FroggersApp::Config();
    const std::size_t blockFrames = static_cast<std::size_t>(config.preferredBlockSize);
    std::vector<std::vector<float>> inputBuffers(static_cast<std::size_t>(config.numAudioInputs),
                                                  std::vector<float>(blockFrames, 0.0f));
    std::vector<std::vector<float>> outputBuffers(static_cast<std::size_t>(config.numAudioOutputs),
                                                   std::vector<float>(blockFrames, 0.0f));
    std::vector<const float*> inputPointers(inputBuffers.size());
    std::vector<float*> outputPointers(outputBuffers.size());
    for (std::size_t ch = 0; ch < inputPointers.size(); ++ch) {
        inputPointers[ch] = inputBuffers[ch].data();
    }
    for (std::size_t ch = 0; ch < outputPointers.size(); ++ch) {
        outputPointers[ch] = outputBuffers[ch].data();
    }

    // preferredBlockSize == kCapacityFrames, so the first direct block only
    // fills the buffer exactly full; the truncation flag itself only sets
    // on the first frame that finds it already full, early in the next
    // block -- same wide margin record_truncates_at_capacity_and_stops_growing
    // above uses, just driven without a tick.
    std::uint64_t directTimestamp = 1;
    for (int i = 0; i < 16; ++i) {
        synth::AudioBlock block;
        block.inputs = inputPointers.empty() ? nullptr : inputPointers.data();
        block.outputs = outputPointers.empty() ? nullptr : outputPointers.data();
        block.numInputChannels = config.numAudioInputs;
        block.numOutputChannels = config.numAudioOutputs;
        block.numFrames = blockFrames;
        block.numRequestedInputChannels = config.numAudioInputs;
        rig.Engine().ProcessBlock(block, directTimestamp++);
    }
    REQUIRE_TRUE(app.RecordedFrameCount() == kCapacityFrames);
    REQUIRE_TRUE(app.RecordingTruncated());
    REQUIRE_TRUE(!app.RecordArmed());

    // The transport is still running from the Play dispatch above (no Stop
    // sent), but dispatch Play again here so the test does not depend on
    // that state having survived unchanged -- ArmRecording() below refuses
    // outright while stopped.
    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));

    // ArmRecording() flushes the still-unpolled truncated capture's export
    // before its own recordBuffer_.assign() would otherwise wipe it out --
    // see QueueTruncatedExportIfPending()'s own comment (FroggersAppCore.hpp).
    REQUIRE_TRUE(app.ArmRecording(/*capacityFramesOverride=*/static_cast<std::size_t>(kCapacityFrames)));
    REQUIRE_TRUE(app.RecordArmed());

    const std::optional<synth::FileExport> fileExport = app.TakePendingFileExport();
    REQUIRE_TRUE(fileExport.has_value());
    REQUIRE_TRUE(fileExport->note == "stopped at the 30-minute limit");
    REQUIRE_TRUE((fileExport->bytes.size() - 44) / 2 == kCapacityFrames);

    // A further take, with nothing new queued, yields none.
    REQUIRE_TRUE(!app.TakePendingFileExport().has_value());

    surface.DispatchAction(synth::ui::Action::Named(synth_froggers::FroggersActions::kStop));
    rig.RunBlocks(4);
}

// A pure function of (bounds, armed) -- no rig needed. Asserts the EXCHANGE,
// not merely that the two command lists differ, same reasoning as
// freeze_draw_commands_genuinely_invert_plate_and_glyph_colours above.
TEST_CASE(record_draw_commands_genuinely_invert_plate_and_glyph_colours) {
    const synth::ui::Bounds bounds{0.0f, 0.0f, synth_froggers::kTransportPlateSize,
                                    synth_froggers::kTransportPlateSize};
    const std::vector<synth::ui::DrawCommand> unarmed = synth_froggers::BuildRecordDrawCommands(bounds, false);
    const std::vector<synth::ui::DrawCommand> armed = synth_froggers::BuildRecordDrawCommands(bounds, true);

    REQUIRE_TRUE(unarmed.size() == 2);
    REQUIRE_TRUE(armed.size() == 2);
    REQUIRE_TRUE(unarmed[0].kind == synth::ui::DrawCommand::Kind::FillRoundedRect);
    REQUIRE_TRUE(unarmed[1].kind == synth::ui::DrawCommand::Kind::FillEllipse);
    REQUIRE_TRUE(armed[0].kind == synth::ui::DrawCommand::Kind::FillRoundedRect);
    REQUIRE_TRUE(armed[1].kind == synth::ui::DrawCommand::Kind::FillEllipse);

    REQUIRE_TRUE(unarmed[0].color == synth_froggers::kTransportPlateColor);
    REQUIRE_TRUE(unarmed[1].color == synth_froggers::kRecordColor);
    REQUIRE_TRUE(armed[0].color == synth_froggers::kRecordColor);
    REQUIRE_TRUE(armed[1].color == synth_froggers::kTransportPlateColor);
    // The exchange itself, asserted directly: armed's plate is unarmed's
    // glyph colour and vice versa.
    REQUIRE_TRUE(armed[0].color == unarmed[1].color);
    REQUIRE_TRUE(armed[1].color == unarmed[0].color);
    REQUIRE_TRUE(unarmed[0].color != unarmed[1].color);  // the two colours are genuinely distinct to begin with.
}

// Pure function of (samples, sampleRate) -- no rig needed. A known 64-sample
// ramp across [-0.5, +0.5] at 48000 Hz, decoded back by hand (not via any
// WAV-reading library this app has) to check the header fields land at their
// canonical byte offsets and sample 0 round-trips within one 16-bit LSB.
TEST_CASE(wav_encoding_produces_a_correct_pcm16_header_and_round_trips_sample_zero) {
    constexpr std::size_t kNumSamples = 64;
    constexpr float kSampleRate = 48000.0f;
    std::vector<float> ramp(kNumSamples);
    for (std::size_t ix = 0; ix < kNumSamples; ++ix) {
        ramp[ix] = -0.5f + (static_cast<float>(ix) / static_cast<float>(kNumSamples - 1)) * 1.0f;
    }

    const std::vector<std::uint8_t> wav = synth_froggers::EncodeWavPcm16Mono(ramp, kSampleRate);

    REQUIRE_TRUE(wav.size() == 44 + 2 * kNumSamples);
    REQUIRE_TRUE(wav[0] == 'R' && wav[1] == 'I' && wav[2] == 'F' && wav[3] == 'F');
    REQUIRE_TRUE(wav[8] == 'W' && wav[9] == 'A' && wav[10] == 'V' && wav[11] == 'E');
    REQUIRE_TRUE(wav[12] == 'f' && wav[13] == 'm' && wav[14] == 't' && wav[15] == ' ');
    REQUIRE_TRUE(wav[36] == 'd' && wav[37] == 'a' && wav[38] == 't' && wav[39] == 'a');

    const auto readU32 = [&wav](std::size_t offset) {
        return static_cast<std::uint32_t>(wav[offset]) | (static_cast<std::uint32_t>(wav[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(wav[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(wav[offset + 3]) << 24);
    };
    const auto readU16 = [&wav](std::size_t offset) {
        return static_cast<std::uint16_t>(static_cast<std::uint16_t>(wav[offset]) |
                                          static_cast<std::uint16_t>(wav[offset + 1] << 8));
    };

    REQUIRE_TRUE(readU32(24) == 48000);            // fmt chunk sample rate field, little-endian read.
    REQUIRE_TRUE(readU16(22) == 1);                // channels.
    REQUIRE_TRUE(readU16(34) == 16);                // bits per sample.
    REQUIRE_TRUE(readU32(40) == 2 * kNumSamples);  // data chunk size.

    const std::int16_t sample0 = static_cast<std::int16_t>(readU16(44));
    const float decoded0 = static_cast<float>(sample0) / 32767.0f;
    // Report the actual read-back values, not just that the
    // assertions passed.
    std::cout << "  [WAV header] sampleRateField=" << readU32(24) << "  dataChunkSize=" << readU32(40)
              << "  sample0 input=" << ramp[0] << "  sample0 decoded=" << decoded0 << "\n";
    REQUIRE_TRUE(std::fabs(decoded0 - ramp[0]) <= (1.0f / 32767.0f) + 1.0e-6f);
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
