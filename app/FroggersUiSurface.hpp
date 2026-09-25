#pragma once

// synth_froggers::FroggersUiSurface -- the app's surface layout
// (spec `specs/froggers-app-surface-layout/spec.md`), built on Sheaf's
// portable layout engine (`PortableUILayout.hpp`/`PortableUIMetrics.hpp`)
// rather than hand-rolled pixel arithmetic.
//
// Topology: sheaf supplies the class idioms but this app owns its own
// topology. This surface is declared as ONE grid -- encoders and chrome
// alike are grid citizens with cell positions, not a scope/grid region plus
// a separately auto-flowed chrome band. `FroggersCellMap` below is the
// topology, expressed as data (a 6-row-by-6-column table); the mechanism --
// how a cell becomes real geometry -- is the Sheaf idiom
// Braid 4's own app uses for its grids (`apps/braid-4/Braid4UiModel.hpp`'s
// `EmitBraid4CellGrid`/`Braid4CellLayout`): a `Column` of `Row`s, weighted
// `Extent::Weight(n)` cells (n>1 expresses a span), and Draw nodes whose
// commands are built from the RESOLVED bounds a `DrawFactory` receives, not
// from pixel math this file computes ahead of time. `EmitBraid4CellGrid`
// itself lives in an app-specific Sheaf header this app must not include
// (cross-app coupling into another app's file) -- this file writes its own
// equivalent, same convention `FroggersParseSize`/`FroggersParseFloat` below
// already follow for Braid4UiModel.hpp's parse-helper pattern.
//
// `StandardAppLayout` (`PortableUIStandardLayout.hpp`) is NOT used: it is
// Braid4's OWN topology (an empty second-visualizer slot does not collapse,
// see `StandardAppLayout::Emit` in
// External/Sheaf/projects/synth/include/synth/PortableUIStandardLayout.hpp), not a neutral scaffold, and this
// app's topology is its own, not Braid4's.
//
// Window size: the surface still resolves against
// `context->config->uiWidth/uiHeight` (a fixed, compiled-in size) rather
// than a live window extent -- making the layout track the ACTUAL window
// requires an upstream shell change (`RuntimeMainComponent::BuildTree()`
// composes the sidebar from `App::Config().uiWidth`, not a live extent, so a
// resizable surface here would desync from it). Everything internal to this
// surface is nonetheless fully declarative, so adopting a live extent later
// is a change to `FroggersPageLayout::RootBounds()`'s source, not a
// redesign.
//
// Bounds note: `synth::ui::Builder`'s Button/Slider/Toggle/
// ComboBox/TextField/StatusText node kinds take no explicit `Bounds` --
// placement comes entirely from each node's own `LayoutOptions` (`main`/
// `cross` extents), resolved by the engine against whatever region its
// container was given. Every region this surface renders -- the scope
// panel, the encoder grid, Play/Stop's plates -- is an in-flow cell with a
// declared `LayoutOptions`; the transport plates' own fixed
// `Extent::Px(28)` size (see `kTransportPlateSize`) is a
// `LayoutOptions`-expressed size too, not an `explicitBounds` out-of-flow
// declaration.
//
// Crunchy has no dedicated chrome slider: it duplicates bank slot 15, so it
// is reachable only via the encoder grid's slot 15, addressed exactly like
// any other bank parameter (Crunchy is unreachable while a modulation view
// is open, since slot 15 is then Target/Back). Crunchy (slot 15) is GLOBAL
// -- one shared `Parameter` aliased into all six banks
// (`FroggersParameters.hpp`'s `FroggersParameterModel::Init`) carrying its own
// fixed Yellow rather than the bank colour, and excluded from randomize
// dispatch (`FroggersModulation.hpp`'s `RandomizeAll`); an encoder press on it
// opens its modulation view like any other slot. That colour already
// flows through `Parameter::UIState.color` into
// `EncoderDrawStateFromParameter` with no special-casing needed here -- this
// file's one encoder-cell code path renders slot 14 (Crispy, per-bank
// colour) and slot 15 (Crunchy, fixed Yellow) identically; the colour
// difference is data, not branching.
//
// Threading note: see FroggersAppCore.hpp's own header comment for the full
// reasoning. Every control this surface dispatches -- encoder drag, scene
// select/blend, transport Start/Stop, the eight presses (encoder press, page
// select and its two carousel arrows, Randomize All/Page, Reset All/Page)
// and the BPM slider -- is pushed onto `context_->uiBus` as a
// `synth::MessageIn`, applied on the audio thread in the order pushed by the
// same drain that applies every other message. The eight presses carry the
// app's own `FroggersCommand` number as `MessageIn::AppCommand`; the BPM
// slider carries `MessageIn::SetTempoBpmNormalized`.

#include "FroggersAppCore.hpp"

#include "synth/EncoderDraw.hpp"
#include "synth/MasterClock.hpp"
#include "synth/ParameterModulation.hpp"
#include "synth/PortableUI.hpp"
#include "synth/PortableUIBuilders.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace synth_froggers {

namespace FroggersNodeIds {

inline constexpr const char* kRoot = "froggers.root";
// The single outer split Row (left block | right block, per FroggersCellMap
// below) and the two blocks themselves.
inline constexpr const char* kLayoutRoot = "froggers.layout.root";
inline constexpr const char* kLeftBlock = "froggers.layout.left";
inline constexpr const char* kRightBlock = "froggers.layout.right";
// The chrome block's two inner columns, emitted only in narrow mode
// (AppendLeftBlock below): the kLeftRows stack, and the column of
// Randomize/Reset buttons that sits beside it. In wide mode kLeftBlock is
// itself that stack and neither id exists.
inline constexpr const char* kLeftStack = "froggers.layout.left.stack";
inline constexpr const char* kLeftButtons = "froggers.layout.left.buttons";

inline constexpr const char* kPlay = "froggers.transport.play";
inline constexpr const char* kStop = "froggers.transport.stop";
// The Freeze transport BUTTON's own node id -- distinct from the Freeze
// ENCODER (Delay bank slot 5, FroggersParameters.hpp), which has no id here
// because encoder cells are addressed by grid index
// (FroggersNodeIds::Encoder), not by name.
inline constexpr const char* kFreeze = "froggers.transport.freeze";
// The Record transport BUTTON's own node id -- fourth child of the
// transport row, beside Play/Stop/Freeze.
inline constexpr const char* kRecord = "froggers.transport.record";
// The label after the Record plate carrying Record's refusal text while it
// applies -- see FroggersUiSurface's transportNotice_ member.
inline constexpr const char* kTransportNotice = "froggers.transport.notice";
// The "FREEZE" text label that sits beside kFreeze -- emitted ONLY when this
// surface is attached in plugin-host mode
// (FroggersUiSurface::SetPluginHostMode(true), see that method's own
// comment); never emitted in the default/standalone/browser mode this file
// otherwise renders. Naming convention: sibling label id under the SAME
// dotted namespace as the control it labels
// (froggers.transport.freeze -> froggers.transport.freeze.label), mirroring
// kBpmLabel's own "<control>.label" suffix off kBpm above.
inline constexpr const char* kFreezeLabel = "froggers.transport.freeze.label";
// The plugin's own input-channel selection control -- a single Button, its
// own label TEXT the currently selected option (e.g. "IN: NONE"), that
// cycles to the next option on tap. Emitted ONLY in plugin-host mode (same
// gate as kFreezeLabel above), in the row space Play/Stop/Record no longer
// occupy there -- there is no standalone equivalent: the standalone's own
// input-device selector lives on Sheaf's audio runtime page, which this
// file does not render (see this file's header comment, "Where it lives is
// the only real difference from the standalone").
inline constexpr const char* kInputSelect = "froggers.transport.input";
// Row 3 of the left block (FroggersCellMap): Play | Stop | Freeze | Record.
inline constexpr const char* kTransportRow = "froggers.layout.left.transport";
// The transport cell, the plates row over the notice line.
inline constexpr const char* kTransportStack = "froggers.layout.left.transport.stack";
// Row 4 of the left block: Scene 1 | Scene 2.
inline constexpr const char* kScenesRow = "froggers.layout.left.scenes";

inline constexpr const char* kRandomizeAll = "froggers.randomize.all";
inline constexpr const char* kRandomizePage = "froggers.randomize.page";
inline constexpr const char* kResetAll = "froggers.reset.all";
inline constexpr const char* kResetPage = "froggers.reset.page";
// Row 6 of the right block: Randomize Page | Randomize All (moved out of the
// page-header group by the CELL MAP -- see AppendRandomizeRow()'s comment).
inline constexpr const char* kRandomizeRow = "froggers.layout.right.randomize";
// Row 7, directly below Randomize, same two-halves weighting.
inline constexpr const char* kResetRow = "froggers.layout.right.reset";
// Row 1 of the right block: the six page-select tabs.
// The string value is a stored wire identifier (a node id a saved layout
// or test can reference) and does not follow the symbol's "Page" name.
inline constexpr const char* kPageTabsRow = "froggers.layout.right.banks";
// A dedicated header ROW, inserted between the page tabs row and the first
// encoder row -- see AppendModulationHeaderRow()'s own comment for why a
// separate row carries the drill-level indicator, and
// FroggersCellMap::RightKind::Header for its place in the topology table.
inline constexpr const char* kModulationHeader = "froggers.layout.right.header";
// Children of kModulationHeader at level 0 (the arrow pair) and level > 0
// (the title, now a distinct child rather than the row's own leaf content --
// see AppendModulationHeaderRow()'s own comment for the full child-structure
// switch).
// Both string values are stored wire identifiers -- a saved controller
// row's action string -- and do not follow the symbols' "Page" name.
inline constexpr const char* kPagePrevArrow = "froggers.bank.prev";
inline constexpr const char* kPageNextArrow = "froggers.bank.next";
inline constexpr const char* kModulationHeaderTitle = "froggers.layout.right.header.title";

inline constexpr const char* kSceneBlend = "froggers.scene.blend";
// Row 5 of the left block: the Scene-blend slider with its label BELOW it,
// a hand-rolled Label rather than a `ControlStyle::caption` -- see
// AppendSceneBlendGroup()'s own comment for why.
inline constexpr const char* kSceneBlendGroup = "froggers.scene.blend.group";
inline constexpr const char* kSceneBlendLabel = "froggers.scene.blend.label";

inline constexpr const char* kBpm = "froggers.bpm";
// Row 6 of the left block: the BPM slider with its label BELOW it, the same
// shape as row 5 -- see AppendLabelledSlider()'s own comment for why both
// labels sit below rather than trailing their sliders.
inline constexpr const char* kBpmGroup = "froggers.bpm.group";
// A hand-rolled Label node rather than `ControlStyle::caption`, exactly like
// scene-blend's (kSceneBlendLabel above). Both sit BELOW their slider.
// `Builder::FinishControl` (External/Sheaf/projects/synth/include/synth/PortableUIBuilders.hpp) always wraps a
// caption and its control together in one implicit `Row`;
// `CaptionPlacement::After` only reorders the caption after the control
// inside that row -- it never stacks them, so a label sitting below its
// slider cannot be a caption. Both stay hand-rolled TOGETHER.
inline constexpr const char* kBpmLabel = "froggers.bpm.label";

inline constexpr const char* kVcoScope = "froggers.scope.vco";

// The string value is a stored wire identifier (a node id a saved layout
// or test can reference) and does not follow the function's "Page" name.
inline std::string PageButton(std::size_t bankIx) {
    return "froggers.bank." + std::to_string(bankIx);
}

inline std::string SceneButton(std::size_t sceneIx) {
    return "froggers.scene." + std::to_string(sceneIx);
}

inline std::string Encoder(std::size_t ix) {
    return "froggers.encoder." + std::to_string(ix);
}

// One Row per 4-wide slice of the 16-slot encoder grid (rows 2-5 of the
// right block, FroggersCellMap): row 0 = slots 0-3, row 1 = slots 4-7,
// etc.
inline std::string EncoderRow(std::size_t row) {
    return "froggers.layout.right.row." + std::to_string(row);
}

}  // namespace FroggersNodeIds

namespace FroggersActions {

inline constexpr const char* kPlay = "froggers.transport.play";
inline constexpr const char* kStop = "froggers.transport.stop";
inline constexpr const char* kFreeze = "froggers.transport.freeze";
inline constexpr const char* kRecord = "froggers.transport.record";
inline constexpr const char* kRandomizeAll = "froggers.randomize.all";
inline constexpr const char* kRandomizePage = "froggers.randomize.page";
inline constexpr const char* kResetAll = "froggers.reset.all";
inline constexpr const char* kResetPage = "froggers.reset.page";
// The string value is a stored wire identifier -- a saved controller row's
// action string -- and does not follow the symbol's "Page" name.
inline constexpr const char* kPageSelect = "froggers.bank.select";
// The secondary arrow-pair navigation beside direct page selection above --
// routed through the same single selection authority (HandleAction, not this
// file's own concern here). Both string values below are stored wire
// identifiers and do not follow the symbols' "Page" name.
inline constexpr const char* kPagePrevious = "froggers.bank.previous";
inline constexpr const char* kPageNext = "froggers.bank.next";
inline constexpr const char* kSceneSelect = "froggers.scene.select";
inline constexpr const char* kSceneBlend = "froggers.scene.blend";
inline constexpr const char* kBpm = "froggers.bpm";
inline constexpr const char* kEncoderPress = "froggers.encoder.press";
inline constexpr const char* kEncoderDrag = "froggers.encoder.drag";
// Cycles the plugin's input-channel selection to the next option in the
// surface's own currently-rendered list (None -> channel 1 -> ... -> Sum ->
// None). See FroggersNodeIds::kInputSelect above and HandleAction's own
// branch for the exact cycle/callback mechanics.
inline constexpr const char* kInputSelect = "froggers.transport.input";
// Dispatched by a browser shell to report its own viewport width, not
// something this surface measures itself. Selects the narrow topology:
// equal outer split weights, and the Randomize/Reset buttons emitted
// beside the sliders in the chrome block rather than below the encoder
// grid. "1" is narrow; every other value, including wide, is not.
inline constexpr const char* kViewportNarrow = "froggers.viewport.narrow";

}  // namespace FroggersActions

// The BPM slider's range, shared with the MIDI catalog.
inline constexpr float kFroggersBpmMin = 30.0f;
inline constexpr float kFroggersBpmMax = 300.0f;

// The surface's own extent and design tokens. Nothing here computes a pixel
// `Bounds` for the scope/grid regions by hand -- placement comes from the
// declarative `LayoutOptions` each node carries (Bounds note above). Some
// tokens below feed layout directly (kMargin/kGap, the outer grid's own
// padding/gap); others (kScopeWidth/kScopeHeight) are consumed only by
// FroggersSurfaceTests.cpp's ratio guard, which checks the RESOLVED layout
// against them -- they remain this file's one definition of the scope's
// target proportions rather than a duplicate literal in the test.
struct FroggersPageLayout {
    static constexpr float kDefaultWidth = 900.0f;
    // A plain literal, matching `FroggersAppCore::Config().uiHeight` (see
    // that file's own comment): the window's initial height, kept in
    // hand-sync with this value, not a derived cross-check.
    //
    // Tall enough that the encoder ring does not shrink: the label band adds
    // space below each encoder rather than taking it from the ring, exactly
    // `FroggersEncoderGridLayout::kLabelBandHeight` (20px) times the 4
    // encoder rows (+80px total) -- see that struct's own comment for the
    // exact-by-construction row-height arithmetic this pays for (page
    // tabs/randomize/reset stay one row height; each encoder row alone
    // carries the extra kLabelBandHeight). `FroggersAppCore::Config()`'s own
    // `config.uiHeight` is the SAME number, hand-synced (see that file's own
    // comment on why it cannot be derived FROM this one) -- kept in sync
    // here, not verified by any cross-check test: such a test can pass even
    // when both sides are already wrong.
    static constexpr float kDefaultHeight = 712.0f;

    // The outer split Row's own padding (inset from the window edge) and the
    // gap between the left/right blocks and between each block's own stacked
    // rows.
    static constexpr float kMargin = 16.0f;
    static constexpr float kGap = 14.0f;

    // The scope reads as a wide landscape strip, not a tall portrait column
    // -- width comfortably exceeds height. The scope's cell is
    // weight-resolved against whatever window the surface builds against,
    // not sized from these pixels directly -- but FroggersSurfaceTests.cpp's
    // ratio guard checks the resolved cell against this exact baseline, so
    // these stay this file's one definition site for the target proportions
    // rather than a second copy in the test.
    static constexpr float kScopeWidth = 340.0f;
    static constexpr float kScopeHeight = 64.0f;

    // The ONE declared width for every labelled slider in the left block.
    // Scene blend and BPM would otherwise disagree: a Column sizes its
    // slider's CROSS axis (horizontal) so an undeclared width fills the
    // block, while a Row sizes its MAIN axis (horizontal) so an undeclared
    // width splits with the label instead -- two different default widths
    // for the same-looking control. Declaring one width here, read once in
    // AppendLabelledSlider(), makes width a property of the control rather
    // than a side effect of which container emits it.
    //
    // Deliberately a FRACTION, not a pixel count. `Extent::Fraction(f)`
    // resolves as `contentExtent * f` (ResolveCrossExtent,
    // External/Sheaf/projects/synth/include/synth/PortableUILayout.hpp's `ResolveCrossExtent`,
    // Fraction case, with `ClampExtent` applying Min/Max), so this tracks the left block's real
    // resolved width and keeps working if that ever changes. A pixel width
    // would have to be re-tuned for each.
    //
    // Sliders are equal to each other BY CONSTRUCTION because this is the
    // single definition site, read once in AppendLabelledSlider(). Do not add
    // a second per-control width: two values kept in agreement by hand is the
    // same defect `uiHeight`'s hand-sync with `kDefaultHeight` (above)
    // already carries.
    static constexpr float kSliderWidthFraction = 0.8f;

    static synth::ui::Bounds RootBounds(const synth::AppContext* context) {
        const float width = context != nullptr && context->config != nullptr
                                 ? static_cast<float>(context->config->uiWidth)
                                 : kDefaultWidth;
        const float height = context != nullptr && context->config != nullptr
                                  ? static_cast<float>(context->config->uiHeight)
                                  : kDefaultHeight;
        return {0.0f, 0.0f, width, height};
    }
};

// The 16-slot grid topology, slots 0-15 laid out 4x4 -- `kColumns`/`kRows`/
// `kEncoderCount` are the slot topology (static_assert-tied to
// `kFroggersSlotsPerBank`). Cells are in-flow grid cells the layout engine
// sizes (see AppendEncoderRow() below); the row/column mapping (`ix /
// kColumns`, `ix % kColumns`) is the loop shape AppendEncoderRow() below
// walks.
struct FroggersEncoderGridLayout {
    static constexpr std::size_t kColumns = 4;
    static constexpr std::size_t kRows = 4;
    static constexpr std::size_t kEncoderCount = kColumns * kRows;
    // The gap between encoder cells within a row and between encoder rows --
    // its own distinct structural role vs. `FroggersPageLayout::kGap`, which
    // separates the left/right blocks and each block's own top-level rows.
    static constexpr float kGap = 8.0f;

    // The measured value (`BuildFroggersTreeAtDefaultSize()`'s resolved
    // Encoder(0) bounds, cross-checked against `AllocateExtents`' documented
    // formula -- kRightBlock content height 600px at the pre-existing
    // 900x632 window, minus the modulation header's fixed 26px and 7
    // inter-row gaps of 14 (98), leaves 476, split 7 ways across PageTabs +
    // the 4 EncoderRows + Randomize + Reset, all Weight(1.0) at the time of
    // measurement: 476/7 = 68.0 exactly).
    //
    // VERIFIED (not eyeballed): measured directly against a built tree and
    // matches the analytic formula above bit-for-bit, so the ring math below
    // is built on a confirmed figure rather than an assumed one.
    static constexpr float kUnchangedRowHeight = 68.0f;

    // The label band strictly BELOW the ring, ~20px -- added space, not
    // space taken from the ring.
    static constexpr float kLabelBandHeight = 20.0f;

    // Each encoder row's own resolved height: the
    // ring-bearing portion (unchanged) plus the new label band. Deliberately
    // the SUM of the two constants above, never a separately hand-typed
    // literal -- AppendEncoderRow passes this as an `Extent::Weight` value
    // (not a later multiply) specifically so `AllocateExtents`
    // (`AllocateExtents`, External/Sheaf/projects/synth/include/synth/PortableUILayout.hpp)'s `remaining * weight / totalWeight`
    // resolves EXACTLY (bit-for-bit, no float rounding) at the default
    // window: `kDefaultHeight` above is chosen so that `remaining` there
    // exactly equals the total weight (PageTabs/Randomize/Reset at
    // kUnchangedRowHeight=68 each + 4 EncoderRows at kGrownRowHeight=88
    // each = 556), which makes every `remaining * weight / totalWeight`
    // division reduce to the exact input weight (a property of IEEE754
    // correctly-rounded division over exactly-representable integer
    // operands, not an approximation) -- see AppendEncoderCell's own
    // comment for how this feeds the ring's byte-identical sub-extent.
    static constexpr float kGrownRowHeight = kUnchangedRowHeight + kLabelBandHeight;
};

static_assert(FroggersEncoderGridLayout::kEncoderCount == kFroggersSlotsPerBank,
              "the grid must render exactly the 16 physical encoder slots FroggersParameterModel wires up");

// The operator-approved topology, kept as PURE DATA -- no builder calls, no
// layout math -- separate from the emission code that interprets it
// (AppendLeftBlock()/AppendRightBlock() below). This stays the one
// definition site for "what goes where", including the narrow-viewport
// variant: the narrow topology differs in the outer split weights
// (kLeftBlockWeightNarrow/kRightBlockWeightNarrow) and in which block the
// four Randomize/Reset buttons land in, and both of those are values here,
// read by the same emission code rather than a forked builder path.
//
// The left and right columns are two INDEPENDENT stacked Columns (siblings
// under the outer split Row, AppendLeftBlock()/AppendRightBlock()), not one
// shared grid: each has its own row count and its own gap total, so a given
// row index does not land at the same y-coordinate in both (the left
// column's 5 rows split 6 weight-units across 4 gaps; the right column's,
// since the header row was added, splits 6 weight-units plus one
// fixed-height row across 6 gaps). The two tables below are listed side by
// side for readability, not
// because their rows align pixel-for-pixel.
//
//   LEFT (L1-L2, 5 rows, weights sum to 6):
//   1 | Scope (weight 2, spans rows 1-2's worth of height)
//   2 | Play | Stop
//   3 | Scene 1 | Scene 2
//   4 | Scene blend (label below)
//   5 | BPM (label below)
//   Rows 4 and 5 are the same shape on purpose: both labels
//   below, both sliders one declared width -- see AppendLabelledSlider() for
//   why.
//
//   RIGHT (E1-E4, 7 rows -- the header row added row 2, all others renumbered
//   down by one from the table before it existed):
//   1 | Page tabs x6 (span E1-E4)
//   2 | Modulation header (span E1-E4, fixed height, empty at drill level 0)
//   3 | slot 0 | slot 1 | slot 2 | slot 3
//   4 | slot 4 | slot 5 | slot 6 | slot 7
//   5 | slot 8 | slot 9 | slot 10 | slot 11
//   6 | slot 12 | slot 13 | slot 14 CRIS | slot 15 CRNC
//   7 | Randomize page (span 2) | Randomize all (span 2)
//   8 | Reset page (span 2) | Reset all (span 2)
//
//   NARROW (a phone, where the browser shell stacks the two blocks
//   vertically instead of placing them side by side): the two blocks carry
//   equal weight so each spans the viewport, rows 7 and 8 above are not
//   emitted, and their four buttons become a second column inside the LEFT
//   block, beside the Scope/Transport/Scenes/blend/BPM stack:
//   L | Scope        | Randomize page
//     | Play | Stop  | Randomize all
//     | Scene 1 | 2  | Reset page
//     | Scene blend  | Reset all
//     | BPM          |
struct FroggersCellMap {
    enum class LeftKind { Scope, Transport, Scenes, SceneBlend, Bpm };
    enum class RightKind { PageTabs, Header, EncoderRow, Randomize, Reset };

    struct LeftRow {
        LeftKind kind;
        // Vertical share of the left column's 6 row-units (a span, exactly
        // like a horizontal `Extent::Weight` span within a row -- the Scope
        // row is 2 units tall, matching rows 1-2 of the table above).
        float rowWeight;
    };
    struct RightRow {
        RightKind kind;
        // Meaningful only for RightKind::EncoderRow: the first of the 4
        // consecutive encoder slot indices this row renders.
        std::size_t firstEncoderIndex;
    };
    struct ButtonCell {
        const char* id;
        const char* label;
        const char* action;
    };

    static constexpr std::array<LeftRow, 5> kLeftRows = {{
        {LeftKind::Scope, 2.0f},
        {LeftKind::Transport, 1.0f},
        {LeftKind::Scenes, 1.0f},
        {LeftKind::SceneBlend, 1.0f},
        {LeftKind::Bpm, 1.0f},
    }};

    // Extent 7 -> 8 for the Reset row. This is a FIXED-extent
    // std::array, so appending a row means changing the count too, not just
    // adding an initializer.
    static constexpr std::array<RightRow, 8> kRightRows = {{
        {RightKind::PageTabs, 0},
        {RightKind::Header, 0},
        {RightKind::EncoderRow, 0},
        {RightKind::EncoderRow, 4},
        {RightKind::EncoderRow, 8},
        {RightKind::EncoderRow, 12},
        {RightKind::Randomize, 0},
        {RightKind::Reset, 0},
    }};

    // The four Randomize/Reset buttons, in operator order. ONE definition
    // site for their identity, because two layouts emit them: the wide
    // layout as two rows of two below the encoder grid (AppendRandomizeRow
    // / AppendResetRow), the narrow layout as one column beside the
    // sliders (AppendNarrowButtonColumn). A second list would be the same
    // four ids, labels and actions written twice.
    static constexpr std::array<ButtonCell, 4> kRandomizeResetButtons = {{
        {FroggersNodeIds::kRandomizePage, "Randomize Page", FroggersActions::kRandomizePage},
        {FroggersNodeIds::kRandomizeAll, "Randomize All", FroggersActions::kRandomizeAll},
        {FroggersNodeIds::kResetPage, "Reset Page", FroggersActions::kResetPage},
        {FroggersNodeIds::kResetAll, "Reset All", FroggersActions::kResetAll},
    }};

    // The outer split Row's weights (L1+L2 = 2 units, E1-E4 = 4 units,
    // matching the table's 6-column width exactly).
    static constexpr float kLeftBlockWeight = 2.0f;
    static constexpr float kRightBlockWeight = 4.0f;
    // The same split at a narrow viewport, where the two blocks are stacked
    // vertically by the browser shell rather than placed side by side. The
    // shell derives ONE scale from the grid block and applies it to every
    // stacked block (app/browser/site/mobile-stack.mjs, `sharedScale`), so
    // a chrome block narrower than the grid block renders narrower than the
    // viewport with the remainder left empty. Equal weights make the two
    // blocks the same width in design space and therefore the same width on
    // screen, which is what puts usable room beside the sliders.
    static constexpr float kLeftBlockWeightNarrow = 3.0f;
    static constexpr float kRightBlockWeightNarrow = 3.0f;
    // How tall the chrome block is when narrow, as a fraction of the outer
    // Row's height. Widening the block without shortening it would keep
    // laying its five rows out over the whole page height, spreading the
    // same controls over half again as much space and pushing the encoder
    // grid -- stacked underneath it by the shell -- clean off the first
    // screen. The ratio of the two weights above keeps the block's design
    // AREA the same as it widens, so its rows stay at the density they are
    // drawn for and the grid starts at the same position as in the wide
    // layout.
    static constexpr float kLeftBlockCrossWeightNarrow = kLeftBlockWeight / kLeftBlockWeightNarrow;
};

// Play/Stop as coloured icons: Play is a green triangle, Stop a red square,
// each on the same neutral chrome plate. Built from exactly Sheaf's existing
// portable primitives (verified present and painted in both the JUCE and
// browser backends): `DrawCommand::FillRoundedRect` for the plate,
// `DrawCommand::FillPolygon` for the Play triangle, `DrawCommand::Fill(Bounds,
// Color)` for the Stop square. Commands are authored against the
// node-LOCAL (0,0,width,height) box (PortableUI.hpp's coordinate contract),
// which the layout engine resolves from this node's own
// `Extent::Px(kTransportPlateSize)` declaration -- the DrawFactory signature
// `vector<DrawCommand>(Bounds)` receives a 28x28 box, so this inset-fraction
// arithmetic works regardless of how that box was produced.
//
// The icon is inset to a fixed FRACTION of the plate rather than a fixed
// pixel amount so it scales with the square and lands at ~55-60% of the
// plate with even padding on all sides, and the plate uses a muted chrome
// colour (RGB 57/106/127) instead of stark white so it sits in the dark
// instrument face instead of glaring out of it.
inline constexpr synth::Color kTransportPlateColor = synth::Color::Rgb(57, 106, 127);
// Record's own glyph/armed-plate colour -- a dark red, distinct from Stop's
// plain synth::Color::Red square sharing this same row.
inline constexpr synth::Color kRecordColor = synth::Color::Rgb(139, 0, 0);
inline constexpr float kTransportIconFraction = 0.575f;  // ~55-60% of the plate
inline constexpr float kTransportPlateSize = 28.0f;      // the transport plate's fixed size, both axes

// Six builders below (Play/Stop/Freeze/Record/PagePrevArrow/PageNextArrow)
// each opened with the identical rounded-rect plate plus inset-box
// arithmetic. Factored to one shared helper -- the plate `DrawCommand` and
// the inset `Bounds` every caller derives its own glyph geometry from.
// Byte-identical output is preserved: the inset `Bounds` computed here is
// LITERALLY the same expression BuildStopDrawCommands/BuildRecordDrawCommands
// already built inline (`square`/`circleBounds` below), and the four
// polygon-glyph builders derive left/top directly from `inset.x`/`inset.y`
// (again the same expression, `bounds.x + insetX` / `bounds.y + insetY`) --
// only right/bottom move from `bounds.x + bounds.width - insetX` to
// `inset.x + inset.width`, algebraically identical and, per this file's own
// test suite (FroggersSurfaceTests.cpp's `PointsClose`/`BoundsClose`, 0.01-0.02f
// tolerance, never bit-exact), not something any test distinguishes.
struct PlateAndInsetBox {
    synth::ui::DrawCommand plate;
    synth::ui::Bounds inset;
};

inline PlateAndInsetBox BuildPlateAndInsetBox(synth::ui::Bounds bounds, synth::Color plateColor) {
    constexpr float kCornerRadius = 4.0f;
    const float insetX = bounds.width * (1.0f - kTransportIconFraction) * 0.5f;
    const float insetY = bounds.height * (1.0f - kTransportIconFraction) * 0.5f;
    return PlateAndInsetBox{
        synth::ui::DrawCommand::FillRoundedRect(bounds, kCornerRadius, plateColor),
        synth::ui::Bounds{
            bounds.x + insetX,
            bounds.y + insetY,
            std::max(0.0f, bounds.width - insetX * 2.0f),
            std::max(0.0f, bounds.height - insetY * 2.0f),
        },
    };
}

// `running` swaps the plate and glyph colours outright while the transport
// runs -- the same genuine colour EXCHANGE BuildFreezeDrawCommands' own
// `latched` and BuildRecordDrawCommands' own `armed` make, not a brightness
// bump, so the plate says whether the instrument is playing.
inline std::vector<synth::ui::DrawCommand> BuildPlayDrawCommands(synth::ui::Bounds bounds, bool running) {
    const synth::Color plateColor = running ? synth::Color::Green : kTransportPlateColor;
    const synth::Color glyphColor = running ? kTransportPlateColor : synth::Color::Green;
    std::vector<synth::ui::DrawCommand> commands;
    const PlateAndInsetBox plate = BuildPlateAndInsetBox(bounds, plateColor);
    commands.push_back(plate.plate);
    const float left = plate.inset.x;
    const float right = plate.inset.x + plate.inset.width;
    const float top = plate.inset.y;
    const float bottom = plate.inset.y + plate.inset.height;
    commands.push_back(synth::ui::DrawCommand::FillPolygon(
        {
            synth::ui::Point{left, top},
            synth::ui::Point{left, bottom},
            synth::ui::Point{right, (top + bottom) * 0.5f},
        },
        glyphColor));
    return commands;
}

inline std::vector<synth::ui::DrawCommand> BuildStopDrawCommands(synth::ui::Bounds bounds) {
    std::vector<synth::ui::DrawCommand> commands;
    const PlateAndInsetBox plate = BuildPlateAndInsetBox(bounds, kTransportPlateColor);
    commands.push_back(plate.plate);
    commands.push_back(synth::ui::DrawCommand::Fill(plate.inset, synth::Color::Red));
    return commands;
}

// Freeze, third transport plate beside Play/Stop -- same plate-plus-glyph idiom as
// BuildPlayDrawCommands/BuildStopDrawCommands above (rounded-rect plate,
// inset glyph at kTransportIconFraction). BuildPlayDrawCommands takes a
// `running` flag with the same swap-on-true shape as this function's own
// `latched`; BuildStopDrawCommands alone takes no state flag -- Stop has no
// held state, a stopped transport being Play's own idle state. A diamond
// glyph, visually distinct from Play's triangle and Stop's square.
//
// WHY A Draw NODE AND NOT A Button: `StateColourFor` renders
// `ControlStyle::selected` as `brighter(0.14f)` on
// the background and `TextColourForNode` (External/Sheaf/projects/synth/juce/PortableJuceBackend.hpp)
// branches on `enabled` only -- text colour never changes on selection, so a
// genuine colour INVERSION is not available from the library's own
// selected-state handling. A Draw node emits its own commands, so the
// inversion below is free and needs no upstream change.
//
// When `latched` is true the plate and glyph colours SWAP outright (a real
// exchange, not a brightness bump) -- this is the whole reason Freeze is a
// Draw node rather than a Button relying on `selected`.
inline std::vector<synth::ui::DrawCommand> BuildFreezeDrawCommands(synth::ui::Bounds bounds, bool latched) {
    const synth::Color plateColor = latched ? synth::Color::Cyan : kTransportPlateColor;
    const synth::Color glyphColor = latched ? kTransportPlateColor : synth::Color::Cyan;
    std::vector<synth::ui::DrawCommand> commands;
    const PlateAndInsetBox plate = BuildPlateAndInsetBox(bounds, plateColor);
    commands.push_back(plate.plate);
    const float left = plate.inset.x;
    const float right = plate.inset.x + plate.inset.width;
    const float top = plate.inset.y;
    const float bottom = plate.inset.y + plate.inset.height;
    const float midX = (left + right) * 0.5f;
    const float midY = (top + bottom) * 0.5f;
    commands.push_back(synth::ui::DrawCommand::FillPolygon(
        {
            synth::ui::Point{midX, top},
            synth::ui::Point{right, midY},
            synth::ui::Point{midX, bottom},
            synth::ui::Point{left, midY},
        },
        glyphColor));
    return commands;
}

// Record, fourth transport plate beside Play/Stop/Freeze -- same
// plate-plus-glyph idiom as BuildFreezeDrawCommands
// above (rounded-rect plate, inset glyph at kTransportIconFraction, a
// genuine colour EXCHANGE while armed, not a brightness tweak -- see that
// function's own comment for why a Draw node is what makes the exchange
// free). A filled circle glyph (DrawCommand::FillEllipse over a square inset
// box, same inset box BuildStopDrawCommands already computes for its
// square), visually distinct from Play's triangle, Stop's square, and
// Freeze's diamond. Dark red normally; armed swaps plate and glyph outright,
// same as Freeze's own cyan/plate-colour exchange.
inline std::vector<synth::ui::DrawCommand> BuildRecordDrawCommands(synth::ui::Bounds bounds, bool armed) {
    const synth::Color plateColor = armed ? kRecordColor : kTransportPlateColor;
    const synth::Color glyphColor = armed ? kTransportPlateColor : kRecordColor;
    std::vector<synth::ui::DrawCommand> commands;
    const PlateAndInsetBox plate = BuildPlateAndInsetBox(bounds, plateColor);
    commands.push_back(plate.plate);
    commands.push_back(synth::ui::DrawCommand::FillEllipse(plate.inset, glyphColor));
    return commands;
}

// The page-carousel back/forward arrow pair, AppendModulationHeaderRow's
// level-0 children. Same
// plate-plus-glyph idiom as the four builders above (rounded-rect plate,
// glyph inset at `kTransportIconFraction`) -- but plain triangles with no
// state to invert (no latched/armed toggle, unlike Freeze/Record), so no
// second colour-swapped variant is needed. The plate colour is
// `kTransportPlateColor`, the SAME neutral chrome colour every transport
// plate above uses; the glyph is `synth::Color::White`, matching the header
// band's own title text colour (`kModulationHeaderTextStyle`,
// AppendModulationHeaderRow's own comment) rather than any one transport
// plate's glyph colour -- Play/Stop/Freeze/Record's glyph colours
// (Green/Red/Cyan/dark-red) each encode that control's own state semantics,
// which this plain nav pair does not carry.
//
// The forward glyph (BuildPageNextArrowDrawCommands) is the exact same
// apex-right triangle shape as BuildPlayDrawCommands' own glyph above,
// reused verbatim; the back glyph (BuildPagePrevArrowDrawCommands) mirrors
// it (apex left, base right).
inline std::vector<synth::ui::DrawCommand> BuildPagePrevArrowDrawCommands(synth::ui::Bounds bounds) {
    std::vector<synth::ui::DrawCommand> commands;
    const PlateAndInsetBox plate = BuildPlateAndInsetBox(bounds, kTransportPlateColor);
    commands.push_back(plate.plate);
    const float left = plate.inset.x;
    const float right = plate.inset.x + plate.inset.width;
    const float top = plate.inset.y;
    const float bottom = plate.inset.y + plate.inset.height;
    commands.push_back(synth::ui::DrawCommand::FillPolygon(
        {
            synth::ui::Point{right, top},
            synth::ui::Point{right, bottom},
            synth::ui::Point{left, (top + bottom) * 0.5f},
        },
        synth::Color::White));
    return commands;
}

inline std::vector<synth::ui::DrawCommand> BuildPageNextArrowDrawCommands(synth::ui::Bounds bounds) {
    std::vector<synth::ui::DrawCommand> commands;
    const PlateAndInsetBox plate = BuildPlateAndInsetBox(bounds, kTransportPlateColor);
    commands.push_back(plate.plate);
    const float left = plate.inset.x;
    const float right = plate.inset.x + plate.inset.width;
    const float top = plate.inset.y;
    const float bottom = plate.inset.y + plate.inset.height;
    commands.push_back(synth::ui::DrawCommand::FillPolygon(
        {
            synth::ui::Point{left, top},
            synth::ui::Point{left, bottom},
            synth::ui::Point{right, (top + bottom) * 0.5f},
        },
        synth::Color::White));
    return commands;
}

// Small parse helpers (own implementation, following Braid4UiModel.hpp's
// ParseSize/ParseFloat *pattern* -- this ports the pattern, not the
// implementation: Braid4UiModel.hpp itself lives under the read-only
// External/Sheaf submodule).
// FroggersTransportIsRunning() and FroggersExternallyClocked() live in
// FroggersAppCore.hpp (ArmRecording() needs the former too); this file
// reads the transport state and clock-slaving state through them only.
inline std::size_t FroggersParseSize(const std::string& value, std::size_t fallback) {
    if (value.empty()) {
        return fallback;
    }
    try {
        std::size_t consumed = 0;
        const unsigned long parsed = std::stoul(value, &consumed, 10);
        return consumed == value.size() ? static_cast<std::size_t>(parsed) : fallback;
    } catch (...) {
        return fallback;
    }
}

inline float FroggersParseFloat(const std::string& value, float fallback) {
    if (value.empty()) {
        return fallback;
    }
    try {
        std::size_t consumed = 0;
        const float parsed = std::stof(value, &consumed);
        return consumed == value.size() ? parsed : fallback;
    } catch (...) {
        return fallback;
    }
}

// Encoder drag actions carry both the grid position and the delta, encoded
// as "position:delta" (no separate slotIx field -- unlike Braid4, this app
// has exactly one BankSlot, always slot 0).
inline std::string FormatFroggersEncoderDrag(std::size_t position, float delta) {
    return std::to_string(position) + ":" + std::to_string(delta);
}

inline bool ParseFroggersEncoderDrag(const std::string& value, std::size_t& position, float& delta) {
    const std::size_t separator = value.find(':');
    if (separator == std::string::npos) {
        return false;
    }
    const std::size_t parsedPosition =
        FroggersParseSize(value.substr(0, separator), std::numeric_limits<std::size_t>::max());
    const float parsedDelta =
        FroggersParseFloat(value.substr(separator + 1), std::numeric_limits<float>::quiet_NaN());
    if (parsedPosition == std::numeric_limits<std::size_t>::max() || !std::isfinite(parsedDelta)) {
        return false;
    }
    position = parsedPosition;
    delta = parsedDelta;
    return true;
}

inline std::string FormatFroggersBpm(double bpm) {
    std::ostringstream oss;
    oss.precision(1);
    oss << std::fixed << bpm;
    return oss.str();
}

// The label actually RENDERED in each encoder cell, kept deliberately
// SEPARATE from `FroggersParamSpec::name`/`shortName`
// (FroggersParameters.hpp): the approved rendering here wins for RENDERING
// ONLY -- the seven marked shortenings below (e.g. "Comb feedback" -> "Comb
// FB") must not rename the REGISTERED parameter, whose `name`/`shortName`
// stay load-bearing for ParameterManager's own name space and other
// consumers. Indexed [bankIx][slot] for slots 0-13, in
// `FroggersBankLayouts()`'s own bank order (FroggersBankId::Audio=0 ...
// Reverb=5); the Envelope row (index 1) uses its canonical short forms
// verbatim (short names are acceptable there), every other row is the
// readable long name except the seven marked shortenings, called out per
// row below. Slots 14/15 (Crispy/Crunchy) are handled by
// `FroggersApprovedGlobalLabel` below, not this table -- they are not
// per-bank `FroggersParamSpec` entries (Crispy is six separate per-bank
// Parameter objects that all render the same word; Crunchy is one shared
// Parameter across all six banks).
inline const std::array<std::array<const char*, kFroggersParamsPerBank>, kFroggersPageCount>&
FroggersApprovedLabels() {
    static const std::array<std::array<const char*, kFroggersParamsPerBank>, kFroggersPageCount> labels{{
        {{"VCO1", "VCO2", "VCO3", "Shape 1", "Shape 2", "Shape 3", "Ph.mod 1", "Ph.mod 2", "Ph.mod 3",
          "Ringmod 1", "Ringmod 2", "Ringmod 3", "PM rate", "VCO balance"}},
        // Envelope -- canonical short forms, not a truncation: the short
        // form IS the name here.
        {{"A1", "D1", "S1", "R1", "A2", "D2", "S2", "R2", "A3", "D3", "S3", "R3", "Curve", "Grace"}},
        // Filter -- slot 5 shortened ("Comb feedback" -> "Comb FB").
        {{"Peak freq", "Peak gain", "Peak Q", "Comb offset", "Comb delay", "Comb FB", "Comb LP",
          "Comb drive", "Scoop mix", "Scoop freq", "Scoop width", "Scoop depth", "Comb/Peak", "Topology"}},
        // Drive -- slot 9 shortened ("Anti-alias brightness" -> "Anti-alias").
        // Slots 10 and 13 render their full registered names ("Feedback",
        // "Symmetry") verbatim, same as every other unshortened slot on
        // this row.
        {{"Wet/Dry", "Gain", "Shape", "SRR 1", "SRR 2", "XOR", "Bit depth", "Fuzz", "Phase",
          "Anti-alias", "Feedback", "Fold", "Tone", "Symmetry"}},
        // Delay -- slot 7 ("Reverse blend" -> "Reverse"), slot 9
        // ("Feedback drive" -> "FB drive"), slot 10 ("Feedback tone" ->
        // "FB tone"), slot 12 ("Width balance" -> "Width bal").
        {{"Wet/dry", "Send", "Delay time", "Feedback", "Stereo width", "Freeze", "Mod depth",
          "Reverse", "Diffusion", "FB drive", "FB tone", "Mod rate", "Width bal", "Crush"}},
        // Reverb -- no shortenings.
        {{"Wet/dry", "Send", "Room size", "Decay", "Pre-delay", "Damping", "Stereo width",
          "Density", "Mod", "Hold", "Tank drive", "Grit", "Tilt", "Tuned"}},
    }};
    return labels;
}

// labels.md's "Global" row: one rendered word regardless of which bank's
// local Crispy this is (all six read the same word; Crunchy is the one
// shared Parameter). Deliberately independent of Crunchy's registered
// `shortName` ("Crnchy", FroggersParameters.hpp) -- labels.md wins for
// RENDERING, per this table's own header comment.
inline const char* FroggersApprovedGlobalLabel(std::size_t slot) {
    return slot == kFroggersCrispySlot ? "Crispy" : "Crunchy";
}

// The single-row 14-segment label grid's column count, sized to hold the
// longest approved label. Verified (not assumed) by FroggersSurfaceTests.cpp's
// own
// `every_approved_label_fits_the_single_row_grid`: the longest of all 86
// entries in `FroggersApprovedLabels()`/`FroggersApprovedGlobalLabel` is
// "Stereo width" (Delay slot 4 and Reverb slot 6), 12 characters including
// the space.
inline constexpr int kApprovedLabelGridColumns = 12;

// Builds the single-row 14-segment label block for one encoder cell's label
// band -- ONE function, called from AppendEncoderCell's own Draw lambda AND
// directly from FroggersSurfaceTests.cpp's verbatim/non-intersection guards,
// so those tests compare against the exact commands production emits, not a
// second hand-written copy of this centering/padding arithmetic. `label` is
// uppercased here (the display is uppercase-only) and centered by
// left-padding with spaces to `columns` -- `BuildFourteenSegmentCommands`
// (EncoderDraw.hpp) already
// left-aligns and pads the TRAILING side with spaces on its own, so
// centering only needs the leading pad computed here.
inline std::vector<synth::ui::DrawCommand> BuildEncoderLabelRowCommands(std::string_view label,
                                                                        synth::ui::Bounds rowBounds,
                                                                        synth::Color onColor,
                                                                        synth::Color offColor, int columns) {
    std::string upper;
    upper.reserve(label.size());
    for (char c : label) {
        upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    const int len = std::min(columns, static_cast<int>(upper.size()));
    const int leadingSpaces = (columns - len) / 2;
    const std::string padded = std::string(static_cast<std::size_t>(leadingSpaces), ' ') + upper.substr(0, static_cast<std::size_t>(len));
    return synth::ui::BuildFourteenSegmentCommands(padded, rowBounds, onColor, offColor, columns);
}

class FroggersUiSurface final : public synth::ui::Surface {
public:
    void Attach(synth::AppContext* context, FroggersAppCore* app) {
        context_ = context;
        app_ = app;
    }

    // A runtime host-capability flag, defaulted false -- the desktop and
    // browser hosts never call this, so their rendered tree is unaffected by
    // it. The plugin host (the VST editor) calls SetPluginHostMode(true)
    // before/at attach time so
    // AppendTransportRow() below thins Play | Stop | Freeze | Record down to
    // Freeze | "FREEZE" label -- the plugin spec's binding requirement
    // (Play/Stop/Record are meaningless when the DAW is transport authority;
    // only Freeze survives as a plugin-reachable control).
    //
    // Deliberately a plain runtime setter on the surface instance, NOT a
    // new FroggersCellMap::LeftKind row-table entry (contrast the CELL
    // MAP's own convention, kLeftRows above, ~line 442): kLeftRows is ONE
    // compile-time static array shared by every host that constructs this
    // class -- baking a per-host variant into that table would make the
    // STANDALONE host's row shape depend on a flag it never touches, when
    // what actually varies here is which CHILDREN one already-declared row
    // (Transport) emits, not which rows exist at all. This follows the same
    // "read live state fresh every rebuild" idiom AppendTransportRow
    // already uses for app_->FreezeLatched() (this class rebuilds its
    // tree every frame) -- pluginHostMode_ is just another per-rebuild-read
    // runtime flag, owned by the surface instance rather than by app_
    // (it is a HOST fact, not an application-state fact, so it does not
    // belong on FroggersAppCore either).
    void SetPluginHostMode(bool pluginHostMode) { pluginHostMode_ = pluginHostMode; }
    bool PluginHostMode() const { return pluginHostMode_; }

    // The plugin's own input-channel selection control -- another
    // host-only fact owned directly by the
    // surface instance, same reasoning as pluginHostMode_ above (a host
    // fact, this file has no concept of a JUCE audio bus to derive it from
    // itself). `labels[0]` is always "None"; the plugin
    // (FroggersPluginProcessor::ComputeInputOptionLabels(), ONLY writer of
    // this list) derives the rest from its own live bus and calls this
    // whenever that bus's shape might have changed -- construction, a host
    // layout change, or a restored session. `selection` is an index into
    // `labels`, clamped into range here (never trusted from the caller):
    // this is the ONLY entry point that can move inputSelection_ WITHOUT
    // going through the operator's own tap (HandleAction's kInputSelect
    // branch below, the ONLY other writer), so a plugin-side re-validation
    // (a layout change that removed the selected channel) always lands here
    // too, not a second path.
    void SetInputOptions(std::vector<std::string> labels, int selection) {
        inputOptionLabels_ = std::move(labels);
        const int count = static_cast<int>(inputOptionLabels_.size());
        inputSelection_ = (count > 0 && selection >= 0 && selection < count) ? selection : 0;
    }

    // Message-thread only (same contract as every other write path in this
    // class): registers the callback HandleAction's kInputSelect branch
    // invokes with the NEXT index it just cycled to, whenever the operator
    // taps the control. The plugin is the one that decides whether that
    // index is actually valid and what it means for consent -- see this
    // callback's registration site (FroggersPluginProcessor's constructor)
    // for why re-validation belongs there, not here.
    void SetInputSelectionChangedCallback(std::function<void(int)> callback) {
        inputSelectionChangedCallback_ = std::move(callback);
    }

    synth::ui::NodeTree BuildTree() override {
        const synth::ui::Bounds root = FroggersPageLayout::RootBounds(context_);

        synth::ui::Builder builder;
        builder.Root(FroggersNodeIds::kRoot, root);
        // The on-canvas "Frogg3rs Synth" title label is removed --
        // `config.appName` (FroggersAppCore.hpp's `Config`) and
        // `FroggersManifest().displayName` (FroggersRegistration.hpp's `FroggersManifest`)
        // already cover launcher/window-title naming. The freed space is
        // left for a future logo,
        // deferred pending upstream `DrawCommand::Image`.

        // ONE outer split Row -- left block (Weight(2): scope,
        // transport, scenes, scene-blend, BPM) beside right block
        // (Weight(4): page tabs, the 16-slot encoder grid, randomize) --
        // matching the CELL MAP's 2-of-6 vs 4-of-6 column split. Outer
        // padding/gap are this file's own design tokens
        // (FroggersPageLayout::kMargin/kGap), not upstream defaults.
        synth::ui::LayoutOptions outerLayout;
        outerLayout.main = synth::ui::Extent::Weight(1.0f);
        outerLayout.cross = synth::ui::Extent::Weight(1.0f);
        outerLayout.padding = FroggersPageLayout::kMargin;
        outerLayout.gap = FroggersPageLayout::kGap;
        builder.Row(FroggersNodeIds::kLayoutRoot, outerLayout, [this](synth::ui::Builder& b) {
            AppendLeftBlock(b);
            AppendRightBlock(b);
        });

        return builder.Build(root);
    }

    void SetActionHandler(ActionHandler handler) override {
        outerHandler_ = std::move(handler);
    }

    void DispatchAction(const synth::ui::Action& action) override {
        HandleAction(action);
        if (outerHandler_) {
            outerHandler_(action);
        }
    }

private:
    // Display-only offset for the scene-blend slider: the Scene 1/Scene 2
    // buttons read 1/2 (AppendScenesRow() above, DO NOT CHANGE), while
    // Sheaf's own `SceneState.blend` -- clamped 0..1 inside Sheaf -- must
    // stay 0..1. Rather than relabel the buttons to 0/1, the slider PRESENTS
    // 1.0-2.0 so it agrees with the buttons; the underlying message still
    // carries 0..1. Both call sites (AppendSceneBlendGroup()'s
    // `b.Slider(...)` below and HandleAction()'s `kSceneBlend` branch) apply
    // the SAME offset, so it is this one named constant rather than two bare
    // `1.0f`s that would have to be kept in agreement by hand.
    static constexpr float kSceneBlendDisplayOffset = 1.0f;

    // -- Left block (FroggersCellMap, columns L1-L2) ------------------

    void AppendLeftBlock(synth::ui::Builder& builder) const {
        synth::ui::LayoutOptions blockLayout;
        blockLayout.main = synth::ui::Extent::Weight(narrowViewport_ ? FroggersCellMap::kLeftBlockWeightNarrow
                                                                     : FroggersCellMap::kLeftBlockWeight);
        blockLayout.cross = synth::ui::Extent::Weight(
            narrowViewport_ ? FroggersCellMap::kLeftBlockCrossWeightNarrow : 1.0f);
        blockLayout.padding = 0.0f;
        blockLayout.gap = FroggersPageLayout::kGap;
        if (!narrowViewport_) {
            builder.Column(FroggersNodeIds::kLeftBlock, blockLayout, [this](synth::ui::Builder& b) {
                AppendLeftRows(b);
            });
            return;
        }
        // Narrow: the block is a Row of two Columns instead of one Column.
        // The kLeftRows stack keeps its own shape and takes the width left
        // over; the Randomize/Reset buttons take the rest, filling what
        // would otherwise be empty viewport beside this block at a phone
        // width.
        builder.Row(FroggersNodeIds::kLeftBlock, blockLayout, [this](synth::ui::Builder& b) {
            synth::ui::LayoutOptions stackLayout;
            stackLayout.main = synth::ui::Extent::Weight(1.0f);
            stackLayout.cross = synth::ui::Extent::Weight(1.0f);
            stackLayout.padding = 0.0f;
            stackLayout.gap = FroggersPageLayout::kGap;
            b.Column(FroggersNodeIds::kLeftStack, stackLayout, [this](synth::ui::Builder& c) {
                AppendLeftRows(c);
            });
            AppendNarrowButtonColumn(b);
        });
    }

    // The kLeftRows stack, emitted into whichever Column carries it: the
    // chrome block itself when wide, the block's left-hand inner column
    // when narrow.
    void AppendLeftRows(synth::ui::Builder& builder) const {
        for (const FroggersCellMap::LeftRow& row : FroggersCellMap::kLeftRows) {
            AppendLeftRow(builder, row);
        }
    }

    // The narrow chrome block's second column: the same four buttons the
    // wide layout puts in two rows below the encoder grid
    // (FroggersCellMap::kRandomizeResetButtons, the one definition site of
    // their identity), stacked instead beside the oscilloscope and the
    // sliders.
    //
    // Both axes Intrinsic, which is what "sized to the label" means here.
    // Inside a Column the MAIN axis is the height and the CROSS axis is the
    // width (PortableUILayout.hpp's `MainAxisFor`: Vertical for anything
    // that is not a Row), so it is `cross` that has to be Intrinsic for a
    // short label to produce a narrow button -- `main` alone would size the
    // height and leave the width untouched. A Button's intrinsic width is
    // its label width with a 72px floor (PortableUIMetrics.hpp's
    // `IntrinsicFor`), so four of them cost one label's width here, not a
    // share of the block.
    void AppendNarrowButtonColumn(synth::ui::Builder& builder) const {
        synth::ui::LayoutOptions columnLayout;
        // Intrinsic along the parent Row's main axis: the column is exactly
        // as wide as its widest button, leaving everything else to the stack.
        columnLayout.main = synth::ui::Extent::Intrinsic();
        // Intrinsic on the cross axis too, so the column's own box ENDS where
        // its last button ends rather than running to the bottom of the
        // block. The browser shell reads that box to find where the space
        // below these buttons begins, and places Sheaf's runtime sidebar
        // there (app/browser/site/mobile-stack.mjs) -- a block this surface
        // cannot contain, since Sheaf emits it as a separate tree. Declaring
        // the extent here is what keeps that arrangement readable from this
        // surface instead of living as an offset in the shell.
        // The parent is a Row, so its cross axis is vertical, which is this
        // Column's own MAIN axis -- and a container's intrinsic extent sums
        // its children plus gaps only along its main axis
        // (PortableUILayout.hpp's IntrinsicForNode). Those two axes coinciding
        // is why this resolves to the four buttons plus three gaps.
        columnLayout.cross = synth::ui::Extent::Intrinsic();
        columnLayout.padding = 0.0f;
        columnLayout.gap = FroggersPageLayout::kGap;
        builder.Column(FroggersNodeIds::kLeftButtons, columnLayout, [](synth::ui::Builder& b) {
            for (const FroggersCellMap::ButtonCell& button : FroggersCellMap::kRandomizeResetButtons) {
                synth::ui::ControlStyle style{};
                style.layout.main = synth::ui::Extent::Intrinsic();
                style.layout.cross = synth::ui::Extent::Intrinsic();
                b.Button(button.id, button.label, synth::ui::Action::Named(button.action), style);
            }
        });
    }

    void AppendLeftRow(synth::ui::Builder& builder, const FroggersCellMap::LeftRow& row) const {
        switch (row.kind) {
            case FroggersCellMap::LeftKind::Scope:
                AppendScopeCell(builder, row.rowWeight);
                return;
            case FroggersCellMap::LeftKind::Transport:
                AppendTransportRow(builder, row.rowWeight);
                return;
            case FroggersCellMap::LeftKind::Scenes:
                AppendScenesRow(builder, row.rowWeight);
                return;
            case FroggersCellMap::LeftKind::SceneBlend:
                AppendSceneBlendGroup(builder, row.rowWeight);
                return;
            case FroggersCellMap::LeftKind::Bpm:
                AppendBpmGroup(builder, row.rowWeight);
                return;
        }
    }

    // The VCO scope panel. Its bounds are not known until the layout
    // resolves (it is now an in-flow, weight-sized cell, not a
    // hand-computed pixel rectangle), so the DrawFactory form is used
    // exactly like Braid4UI.hpp's own encoder-visualizer-underlay pattern:
    // the factory receives the RESOLVED extent and sets it on the
    // visualizer at that point.
    //
    // This node carries no drill-level indicator. The scope's cell sits in
    // the CELL MAP's LEFT block (columns L1-L2) -- physically
    // separate from the 16-slot grid, which lives in the RIGHT block
    // (columns E1-E4) and is the only thing that changes content when
    // drill-in changes. A gap (FroggersPageLayout::kGap) separates the two
    // blocks; they never meet, so a header painted on this node would
    // not sit next to the grid it would be describing. Computed (not
    // eyeballed): FroggersSurfaceTests.cpp's
    // modulation_header_sits_below_page_row_and_above_parameter_cells,
    // scope_and_grid_regions_do_not_overlap_at_target_window_size, and
    // scope_sits_in_a_left_column_with_the_grid_to_its_right all check these
    // resolved bounds directly.
    //
    // The drill-level indicator is not on the Target/Back encoder cell
    // either (AppendEncoderCell): a badge there would conflate two separate
    // facts -- the current drill depth, and "this cell exits one level" --
    // into one label attached to a control whose job is only the second one.
    //
    // The indicator is instead a dedicated header ROW
    // (FroggersNodeIds::kModulationHeader, AppendModulationHeaderRow()
    // below) spanning the right block's full width, between the page tabs
    // row and the first row of parameter cells -- not attached to any button
    // or cell. This node (kVcoScope) and the Target/Back encoder cell both
    // keep no copy of this text: the drill level renders in exactly one
    // place.
    void AppendScopeCell(synth::ui::Builder& builder, float rowWeight) const {
        synth::ui::LayoutOptions layout;
        layout.main = synth::ui::Extent::Weight(rowWeight);
        layout.cross = synth::ui::Extent::Weight(1.0f);
        // The cell is always emitted (matching Braid4UI.hpp's own scope-cell
        // idiom, `EmitScopeCell`): only the DATA depends on `app_`, not
        // whether the node exists, so a bare-context resolve (no app
        // attached -- FroggersSurfaceTests.cpp's layout-only tests) still
        // sees a real `kVcoScope` cell with a real resolved extent, just
        // empty draw commands.
        FroggersAppCore* app = app_;
        builder.Draw(FroggersNodeIds::kVcoScope, layout,
                     [app](synth::ui::Bounds extent) -> std::vector<synth::ui::DrawCommand> {
                         if (app == nullptr) {
                             return {};
                         }
                         synth::ui::Visualizer& vcoScope = app->VcoScopeVisualizer();
                         vcoScope.SetBounds(extent);
                         return vcoScope.Draw();
                     });
    }

    // Row 3: Play | Stop, rendered as `Draw` nodes so each plate can paint
    // its own coloured icon. `kTransportPlateSize` is a fixed `Extent::Px`
    // size on both axes, so the plates stay a 28x28 square regardless of the
    // row's resolved width.
    // "IN: <current option, upper-cased>" -- same all-caps
    // convention kFreezeLabel's "FREEZE" already uses beside it. Falls back
    // to inputOptionLabels_[0] ("None") if inputSelection_ is somehow out
    // of range (it never should be -- SetInputOptions()/HandleAction's own
    // kInputSelect branch both clamp it -- this is a display-only guard
    // against reading past the vector, not a second validation path).
    std::string InputSelectButtonLabel() const {
        const std::size_t index = (inputSelection_ >= 0 && static_cast<std::size_t>(inputSelection_) < inputOptionLabels_.size())
                                       ? static_cast<std::size_t>(inputSelection_)
                                       : std::size_t{0};
        std::string label = "IN: ";
        for (char c : inputOptionLabels_[index]) {
            label.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
        return label;
    }

    void AppendTransportRow(synth::ui::Builder& builder, float rowWeight) const {
        // The transport cell is a Column: the plates Row on top, sized to
        // its own fixed plate height (Px(kTransportPlateSize)) rather than
        // filling the cell, with Record's refusal notice appended below it
        // as a second child only when non-empty -- see
        // FroggersNodeIds::kTransportStack's own comment. The cell's own
        // layout (main = Weight(rowWeight)) sits on this outer stack, not on
        // the plates Row.
        synth::ui::LayoutOptions stackLayout;
        stackLayout.main = synth::ui::Extent::Weight(rowWeight);
        stackLayout.cross = synth::ui::Extent::Weight(1.0f);
        stackLayout.padding = 0.0f;
        stackLayout.gap = FroggersPageLayout::kGap;

        // The plates Row's own layout inside the stack -- sized to the
        // fixed plate height rather than the cell's full weighted share,
        // now that the stack above carries that share.
        synth::ui::LayoutOptions rowLayout;
        rowLayout.main = synth::ui::Extent::Px(kTransportPlateSize);
        rowLayout.cross = synth::ui::Extent::Weight(1.0f);
        rowLayout.padding = 0.0f;
        rowLayout.gap = FroggersPageLayout::kGap;
        // The Freeze plate's draw factory needs to read `app_`'s
        // CURRENT latch state on every rebuild (the tree rebuilds every
        // frame, this file's header comment) -- captured into a local first,
        // same idiom AppendScopeCell above uses for kVcoScope, so the
        // now-const-context row-builder lambda below can capture it too.
        FroggersAppCore* app = app_;
        // The Play plate's draw factory reads the live transport state the
        // same way -- captured for the same reason.
        const synth::AppContext* context = context_;
        // Captured by value into the row-builder lambda, same reason `app`
        // is -- read fresh every rebuild, but this one never
        // actually changes mid-session (a host does not switch modes after
        // attaching), so it is really just "the value this render pass
        // uses," not a live cross-thread read like app_->FreezeLatched().
        const bool pluginHostMode = pluginHostMode_;
        // The input-select Button's own rendered text -- computed
        // fresh every rebuild (this file's header comment: the tree
        // rebuilds every frame) from inputOptionLabels_/inputSelection_,
        // the same "read live state fresh, capture the read INTO the
        // lambda by value" idiom `app`/`pluginHostMode` just above already
        // use, since the row-builder lambda below is not a member function
        // and cannot read `this` implicitly.
        const std::string inputSelectLabel = InputSelectButtonLabel();
        // Read fresh into the column-builder lambda every rebuild, same
        // idiom as inputSelectLabel just above -- read by the COLUMN lambda
        // now (not the row lambda), since the notice Label moved out to
        // become the stack's own second child, a sibling of the plates Row
        // rather than a child inside it.
        const std::string transportNotice = transportNotice_;

        builder.Column(FroggersNodeIds::kTransportStack, stackLayout,
                       [app, context, pluginHostMode, inputSelectLabel, transportNotice, rowLayout](synth::ui::Builder& col) {
          col.Row(FroggersNodeIds::kTransportRow, rowLayout,
                    [app, context, pluginHostMode, inputSelectLabel](synth::ui::Builder& b) {
            // In plugin mode, Play, Stop, and Record are not rendered; the
            // Freeze button stays and gains a "FREEZE" text label beside it
            // in the freed row space. pluginHostMode_ defaults false, so
            // this branch is never taken by the standalone/desktop/browser
            // hosts -- they always render the full Play/Stop/Freeze/Record
            // sequence below.
            if (!pluginHostMode) {
                synth::ui::ControlStyle playStyle{};
                playStyle.action = synth::ui::Action::Named(FroggersActions::kPlay);
                playStyle.layout.main = synth::ui::Extent::Px(kTransportPlateSize);
                playStyle.layout.cross = synth::ui::Extent::Px(kTransportPlateSize);
                // A capturing lambda wrapping BuildPlayDrawCommands (it
                // takes a `running` bool the DrawFactory signature --
                // Bounds only -- has no room for), same idiom the Freeze
                // lambda just below uses for its own `latched` bool. Reads
                // FroggersTransportIsRunning() fresh on every call rather
                // than a value cached at click time, so the plate follows
                // the transport on the very next rebuild, whichever route
                // started or stopped it.
                b.Draw(
                    FroggersNodeIds::kPlay,
                    [context](synth::ui::Bounds bounds) {
                        return BuildPlayDrawCommands(bounds, FroggersTransportIsRunning(context));
                    },
                    playStyle);

                synth::ui::ControlStyle stopStyle{};
                stopStyle.action = synth::ui::Action::Named(FroggersActions::kStop);
                stopStyle.layout.main = synth::ui::Extent::Px(kTransportPlateSize);
                stopStyle.layout.cross = synth::ui::Extent::Px(kTransportPlateSize);
                b.Draw(FroggersNodeIds::kStop, BuildStopDrawCommands, stopStyle);
            }

            // Freeze, same 28px plate idiom as Play/Stop -- present in BOTH
            // modes (the Freeze button stays even when Play/Stop/Record are
            // hidden). A capturing
            // lambda wrapping BuildFreezeDrawCommands (it takes a `latched`
            // bool the DrawFactory signature -- Bounds only -- has no room
            // for), same style the encoder-cell `b.Draw(encoderId, [state,
            // ...](Bounds){...}, ...)` call (AppendEncoderCell) already uses
            // for its own per-frame captured state. Reads
            // `app->FreezeLatched()` fresh on every call rather than a value
            // cached at click time, so a click updates the drawn colours on
            // the very next rebuild.
            synth::ui::ControlStyle freezeStyle{};
            freezeStyle.action = synth::ui::Action::Named(FroggersActions::kFreeze);
            freezeStyle.layout.main = synth::ui::Extent::Px(kTransportPlateSize);
            freezeStyle.layout.cross = synth::ui::Extent::Px(kTransportPlateSize);
            b.Draw(
                FroggersNodeIds::kFreeze,
                [app](synth::ui::Bounds bounds) {
                    return BuildFreezeDrawCommands(bounds, app != nullptr && app->FreezeLatched());
                },
                freezeStyle);

            if (pluginHostMode) {
                // The "FREEZE" text label, beside the Freeze plate, in the
                // row space Play/Stop/Record no longer occupy -- same
                // hand-rolled Label idiom kBpmLabel/
                // kSceneBlendLabel use (AppendBpmControl() above,
                // `builder.Label(id, text, ControlStyle{})`), just placed
                // beside its control (a Row's in-flow next child) instead
                // of below it (those two sit in a Column).
                b.Label(FroggersNodeIds::kFreezeLabel, "FREEZE", synth::ui::ControlStyle{});

                // The input-channel selection control: a plain Button (not
                // a Draw plate, unlike Freeze/
                // Record -- it needs no latched/armed colour inversion,
                // just its own current-selection TEXT, which a Button's
                // `label` already carries) whose tap cycles to the surface's
                // next option (HandleAction's kInputSelect branch, below).
                // Intrinsic width -- unlike the fixed 28px transport plates,
                // its label's length varies with the option text ("IN:
                // NONE" vs "IN: SUM").
                synth::ui::ControlStyle inputSelectStyle{};
                inputSelectStyle.layout.cross = synth::ui::Extent::Intrinsic();
                b.Button(FroggersNodeIds::kInputSelect, inputSelectLabel,
                         synth::ui::Action::Named(FroggersActions::kInputSelect), inputSelectStyle);
            } else {
                // Record, fourth child, same 28px plate idiom and same
                // "read live state fresh every rebuild" lambda shape as
                // Freeze just above -- app->RecordArmed() rather than
                // app->FreezeLatched(). Not rendered in plugin-host mode.
                synth::ui::ControlStyle recordStyle{};
                recordStyle.action = synth::ui::Action::Named(FroggersActions::kRecord);
                recordStyle.layout.main = synth::ui::Extent::Px(kTransportPlateSize);
                recordStyle.layout.cross = synth::ui::Extent::Px(kTransportPlateSize);
                b.Draw(
                    FroggersNodeIds::kRecord,
                    [app](synth::ui::Bounds bounds) {
                        return BuildRecordDrawCommands(bounds, app != nullptr && app->RecordArmed());
                    },
                    recordStyle);
            }
          });

          // Record's refusal text, shown below the plates row until a
          // recording arms or Play is pressed -- same hand-rolled Label
          // idiom kFreezeLabel above uses, now the stack's own second
          // child (a sibling of the plates Row) rather than a child
          // inside it.
          if (!transportNotice.empty()) {
              col.Label(FroggersNodeIds::kTransportNotice, transportNotice, synth::ui::ControlStyle{});
          }
        });
    }

    // Row 4: Scene 1 | Scene 2, each taking half the row (Weight(1), an
    // intrinsic cross size so the button does not stretch to the row's full
    // resolved height).
    //
    // "Scene 1"/"Scene 2" are a TOGGLE between the scene-blend extremes, not
    // a re-assignment of which stored scene occupies the less-weighted
    // endpoint -- see HandleAction() below for the full trace.
    void AppendScenesRow(synth::ui::Builder& builder, float rowWeight) const {
        synth::ui::LayoutOptions rowLayout;
        rowLayout.main = synth::ui::Extent::Weight(rowWeight);
        rowLayout.cross = synth::ui::Extent::Weight(1.0f);
        rowLayout.padding = 0.0f;
        rowLayout.gap = FroggersPageLayout::kGap;
        builder.Row(FroggersNodeIds::kScenesRow, rowLayout, [](synth::ui::Builder& b) {
            for (std::size_t sceneIx = 0; sceneIx < 2; ++sceneIx) {
                synth::ui::ControlStyle style{};
                style.layout.main = synth::ui::Extent::Weight(1.0f);
                style.layout.cross = synth::ui::Extent::Intrinsic();
                b.Button(FroggersNodeIds::SceneButton(sceneIx), "Scene " + std::to_string(sceneIx + 1),
                         synth::ui::Action::WithValue(FroggersActions::kSceneSelect, std::to_string(sceneIx)),
                         style);
            }
        });
    }

    // Row 5: the Scene-blend slider with its label BELOW it -- a hand-rolled
    // Label rather than a `ControlStyle::caption`, since caption can only
    // lead. Row 6 (BPM) has the identical shape: its label sits below too.
    // ROWS 5 AND 6 SHARE THIS ONE EMITTER.
    //
    // One container kind (`Column`), one declared slider width, called twice.
    // There is deliberately **no placement parameter and no branch on
    // container kind**: both labels sit BELOW their slider. Two separate
    // emitters would let the two sliders drift into different containers (a
    // Column vs. a Row) with different default widths, so "how wide is this
    // slider" would be answered by which container it happened to live in
    // rather than by a declared property.
    //
    // The two labels are visually symmetric (both below) on purpose -- do
    // not make BPM's label trail its slider while scene-blend's stays below;
    // a trailing label reads as belonging to whichever control sits next to
    // it, which is why both are placed directly beneath their own slider
    // instead.
    //
    // `emitControl` receives the shared slider style, so the width is read
    // from its single definition site exactly once, here, while each caller
    // keeps its own control logic -- BPM has a read-only StatusText state
    // this row must not flatten away.
    void AppendLabelledSlider(
        synth::ui::Builder& builder,
        float rowWeight,
        const char* groupId,
        const std::function<void(synth::ui::Builder&, const synth::ui::ControlStyle&)>& emitControl) const {
        synth::ui::LayoutOptions groupLayout;
        groupLayout.main = synth::ui::Extent::Weight(rowWeight);
        groupLayout.cross = synth::ui::Extent::Weight(1.0f);
        groupLayout.padding = 0.0f;
        groupLayout.gap = FroggersPageLayout::kGap;

        // In a Column the CROSS axis is the horizontal one, so this is where
        // the shared width lands. See kSliderWidthFraction's own comment for
        // why it is a fraction rather than a pixel count.
        synth::ui::ControlStyle sliderStyle;
        sliderStyle.layout.cross = synth::ui::Extent::Fraction(FroggersPageLayout::kSliderWidthFraction);

        builder.Column(groupId, groupLayout, [&emitControl, &sliderStyle](synth::ui::Builder& b) {
            emitControl(b, sliderStyle);
        });
    }

    void AppendSceneBlendGroup(synth::ui::Builder& builder, float rowWeight) const {
        const float sceneBlend = context_ != nullptr && context_->uiState != nullptr
                                      ? context_->uiState->sceneBlend.load(std::memory_order_relaxed)
                                      : 0.0f;
        AppendLabelledSlider(
            builder, rowWeight, FroggersNodeIds::kSceneBlendGroup,
            [sceneBlend](synth::ui::Builder& b, const synth::ui::ControlStyle& sliderStyle) {
                // Presented value/range are the blend shifted by
                // kSceneBlendDisplayOffset (this class's own display-only
                // constant, see its declaration above) so the slider reads
                // 1.0-2.0 to match the Scene 1/Scene 2 buttons; the message
                // this produces still carries 0..1 -- see HandleAction()'s
                // `kSceneBlend` branch, which subtracts the same offset back
                // out.
                b.Slider(FroggersNodeIds::kSceneBlend, "Scene blend", sceneBlend + kSceneBlendDisplayOffset,
                         kSceneBlendDisplayOffset, 1.0f + kSceneBlendDisplayOffset, 0.001f,
                         synth::ui::Action::Named(FroggersActions::kSceneBlend), sliderStyle);
                // `NodeKind::Slider` routes
                // `node.label` to `juce::Slider::setName()` only, inside
                // `CreateControlForNode`
                // (External/Sheaf/projects/synth/juce/PortableJuceBackend.hpp) -- no `juce::Label` is
                // attached, so the slider's own label argument never draws;
                // this adjacent Label node is what actually renders the text.
                b.Label(FroggersNodeIds::kSceneBlendLabel, "Scene blend", synth::ui::ControlStyle{});
            });
    }

    // Row 6: the BPM slider with its label BELOW it, exactly like row 5 --
    // see AppendLabelledSlider()'s trailing-label supersession note. Still a
    // read-only
    // StatusText while slaved to external MIDI clock.
    void AppendBpmGroup(synth::ui::Builder& builder, float rowWeight) const {
        AppendLabelledSlider(builder, rowWeight, FroggersNodeIds::kBpmGroup,
                             [this](synth::ui::Builder& b, const synth::ui::ControlStyle& sliderStyle) {
                                 AppendBpmControl(b, sliderStyle);
                             });
    }

    // Read-only/inert (a StatusText) while slaved to external MIDI clock, an
    // interactive Slider otherwise (see `MasterClock::TempoBpm`/`QuarterNotesPerSample`,
    // External/Sheaf/projects/synth/include/synth/MasterClock.hpp, and the no-op guard in
    // `MasterClock::SetTempoBpm` and the estimate `MasterClock::HandleExternalClock` writes,
    // both in External/Sheaf/projects/synth/src/MasterClock.cpp). Both states
    // display TempoBpm(); its container is this row's own group
    // (AppendBpmGroup() above).
    void AppendBpmControl(synth::ui::Builder& builder, const synth::ui::ControlStyle& sliderStyle) const {
        const double tempoBpm = context_ != nullptr && context_->clockDiagnostics != nullptr
                                     ? context_->clockDiagnostics->Snapshot().currentBpm
                                     : synth::MasterClock::kDefaultTempoBpm;
        const bool externallyClocked = FroggersExternallyClocked(context_);
        if (externallyClocked) {
            // Takes the same declared width as the interactive slider it
            // replaces, so the row does not change shape when the clock is
            // slaved. There is no adjacent Label in this state -- the status
            // text names itself.
            builder.StatusText(FroggersNodeIds::kBpm, "BPM " + FormatFroggersBpm(tempoBpm) + " (external clock)",
                               sliderStyle);
            return;
        }
        // The control genuinely IS labelled "BPM" -- the
        // transport-state-dependent "(no effect while stopped)" annotation
        // was never requested and is not to be reintroduced without asking
        // first.
        constexpr const char* kLabel = "BPM";
        // Still a hand-rolled adjacent Label rather than
        // `ControlStyle::caption`, for the same reason as scene-blend's:
        // `Builder::FinishControl` (External/Sheaf/projects/synth/include/synth/PortableUIBuilders.hpp) always
        // wraps a caption and its control together in one implicit `Row`;
        // `CaptionPlacement::After` only reorders the caption after
        // the control inside that row -- it never stacks them, and both
        // labels now sit BELOW theirs. They stay hand-rolled TOGETHER,
        // which is the point.
        builder.Slider(FroggersNodeIds::kBpm, kLabel, static_cast<float>(tempoBpm), kFroggersBpmMin,
                       kFroggersBpmMax, 1.0f, synth::ui::Action::Named(FroggersActions::kBpm), sliderStyle);
        builder.Label(FroggersNodeIds::kBpmLabel, kLabel, synth::ui::ControlStyle{});
    }

    // -- Right block (FroggersCellMap, columns E1-E4) -----------------

    void AppendRightBlock(synth::ui::Builder& builder) const {
        synth::ui::LayoutOptions blockLayout;
        blockLayout.main = synth::ui::Extent::Weight(narrowViewport_ ? FroggersCellMap::kRightBlockWeightNarrow
                                                                     : FroggersCellMap::kRightBlockWeight);
        blockLayout.cross = synth::ui::Extent::Weight(1.0f);
        blockLayout.padding = 0.0f;
        blockLayout.gap = FroggersPageLayout::kGap;
        builder.Column(FroggersNodeIds::kRightBlock, blockLayout, [this](synth::ui::Builder& b) {
            for (const FroggersCellMap::RightRow& row : FroggersCellMap::kRightRows) {
                AppendRightRow(b, row);
            }
        });
    }

    void AppendRightRow(synth::ui::Builder& builder, const FroggersCellMap::RightRow& row) const {
        switch (row.kind) {
            case FroggersCellMap::RightKind::PageTabs:
                AppendPageTabsRow(builder);
                return;
            case FroggersCellMap::RightKind::Header:
                AppendModulationHeaderRow(builder);
                return;
            case FroggersCellMap::RightKind::EncoderRow:
                AppendEncoderRow(builder, row.firstEncoderIndex);
                return;
            // Narrow moves these two rows into the chrome block
            // (AppendNarrowButtonColumn), so the encoder column emits
            // neither and the four buttons exist exactly once at every
            // width. Skipping them here rather than selecting a second row
            // table keeps kRightRows the one description of this column.
            case FroggersCellMap::RightKind::Randomize:
                if (narrowViewport_) {
                    return;
                }
                AppendRandomizeRow(builder);
                return;
            case FroggersCellMap::RightKind::Reset:
                if (narrowViewport_) {
                    return;
                }
                AppendResetRow(builder);
                return;
        }
    }

    // Row 1: the six page-select tabs, LOOPED from `FroggersBankLayouts()`
    // (single source of truth for bank identity/order,
    // app/FroggersParameters.hpp), not a second hand-written list.
    //
    // Plain `Button` nodes with the action supplied directly, so a single
    // click switches pages. `node.selected` for the active page comes from
    // `ControlStyle::selected`.
    void AppendPageTabsRow(synth::ui::Builder& builder) const {
        synth::ui::LayoutOptions rowLayout;
        // `kUnchangedRowHeight` (68, not the encoder rows' 88) --
        // paired with AppendEncoderRow's own change above so this row
        // stays pixel-identical to today at the default window while the
        // 4 encoder rows alone grow by kLabelBandHeight. See
        // FroggersEncoderGridLayout's own comment for the exact-division
        // property this relies on.
        rowLayout.main = synth::ui::Extent::Weight(FroggersEncoderGridLayout::kUnchangedRowHeight);
        rowLayout.cross = synth::ui::Extent::Weight(1.0f);
        rowLayout.padding = 0.0f;
        rowLayout.gap = FroggersPageLayout::kGap;
        const auto& layouts = FroggersBankLayouts();
        builder.Row(FroggersNodeIds::kPageTabsRow, rowLayout, [this, &layouts](synth::ui::Builder& b) {
            for (std::size_t bankIx = 0; bankIx < kFroggersPageCount; ++bankIx) {
                synth::ui::ControlStyle style{};
                style.selected = PageSelected(bankIx);
                style.layout.main = synth::ui::Extent::Weight(1.0f);
                style.layout.cross = synth::ui::Extent::Intrinsic();
                b.Button(FroggersNodeIds::PageButton(bankIx), layouts[bankIx].name,
                         synth::ui::Action::WithValue(FroggersActions::kPageSelect, std::to_string(bankIx)), style);
            }
        });
    }

    // Row 2: a dedicated fixed-height header row (Extent::Px, the same
    // sizing idiom the transport plates use for kTransportPlateSize) between
    // the page tabs row and the first row of parameter cells, spanning the
    // right block's full width -- not attached to any button or cell.
    //
    // Inserting one more FIXED-size sibling changes no row/column WEIGHT
    // value: every pre-existing Weight(1.0) right-block row stays
    // Weight(1.0), and kLeftBlockWeight/kRightBlockWeight/
    // kSliderWidthFraction are untouched. It leaves less remaining space for
    // the six weighted rows to divide (`AllocateExtents`,
    // External/Sheaf/projects/synth/include/synth/PortableUILayout.hpp: `remaining =
    // contentExtent - totalGaps - nonWeighted`, then split by
    // weight) -- an arithmetic CONSEQUENCE of the insertion, not a declared
    // change. At the current 900x712 window: kRightBlock content is 680px
    // tall; 8 rows cost 7 gaps of 14 (98) plus this row's fixed 26px,
    // leaving 556 -- exactly the total weight (3 unchanged rows at 68 + 4
    // encoder rows at 88), so every row resolves to its declared height
    // bit-exactly. Nothing moves horizontally, no row is reordered, and the
    // encoder grid's own 4-column-per-row internal structure (ids, order,
    // weights) is untouched. Verified in FroggersSurfaceTests.cpp's
    // modulation_header_sits_below_page_row_and_above_parameter_cells, which
    // computes the resolved bounds rather than asserting this comment's
    // arithmetic.
    //
    // CONTENT: "Modulation Level <N>" (N = FroggersModulationDrillIn::
    // Level() via app_->DrillLevel(), never a hardcoded per-level string),
    // drawn ONLY while drilled in (level > 0) -- an empty Draw at level 0.
    // The row's own SPACE is always reserved (constant Px height regardless
    // of drill state): the same "always emit the node, sometimes with empty
    // commands, so sibling geometry never jumps" idiom AppendEncoderCell
    // already uses for a hidden slot in the modulation view (see that
    // method's own comment) -- entering/exiting a drilldown never reflows
    // the page tabs or the parameter grid by even one pixel; only this
    // row's own content changes.
    static constexpr float kModulationHeaderRowHeight = 26.0f;
    static constexpr synth::Color kModulationHeaderBandColor = synth::Color::Rgb(32, 38, 44);
    // Centered: a full-width title bar reads as a header when its text is
    // centered, rather than as a label squeezed into a corner.
    static constexpr synth::ui::TextStyle kModulationHeaderTextStyle{
        20.0f, synth::Color::Rgb(255, 255, 255), synth::ui::TextAlign::Center};

    // The row's OUTER geometry (id kModulationHeader,
    // Px(kModulationHeaderRowHeight) main / Weight(1) cross) is identical in
    // both drill states -- only the CHILDREN switch. Level 0 emits a
    // centered back/forward arrow pair; level > 0 emits the single
    // full-width title child (kModulationHeaderTitle) carrying the fill+text
    // commands for "Modulation Level <N>".
    void AppendModulationHeaderRow(synth::ui::Builder& builder) const {
        synth::ui::LayoutOptions layout;
        layout.main = synth::ui::Extent::Px(kModulationHeaderRowHeight);
        layout.cross = synth::ui::Extent::Weight(1.0f);
        // Container defaults (padding=12/gap=8, PortableUILayout.hpp's
        // kSpacing) would eat most of a 26px-tall band and misplace the
        // arrow pair -- explicit zero padding, same idiom AppendTransportRow
        // already uses for its own fixed-height row (`rowLayout.padding =
        // 0.0f`, above); the row's own `gap` is the pair's documented
        // separation (FroggersPageLayout::kGap).
        layout.padding = 0.0f;
        layout.gap = FroggersPageLayout::kGap;
        const std::size_t drillLevel = app_ != nullptr ? app_->DrillLevel() : 0;
        builder.Row(FroggersNodeIds::kModulationHeader, layout, [drillLevel](synth::ui::Builder& b) {
            if (drillLevel == 0) {
                // [spacer Weight(1)][prev Px][next Px][spacer Weight(1)]:
                // two equal-weight spacers centre the fixed-size pair
                // regardless of the band's resolved width -- the row's own
                // uniform `gap` (set above) applies symmetrically on every
                // side of the pair, so the pair's midpoint lands on the
                // band's midpoint by construction (verified by
                // page_carousel_arrows_are_centered_in_the_modulation_header_band_at_top_level,
                // FroggersSurfaceTests.cpp). Spacers are empty Draw nodes --
                // the same "always emit the node, sometimes with empty
                // commands" idiom AppendEncoderCell already uses for a
                // hidden grid slot -- with ad hoc suffixed ids, the same
                // convention `encoderId + ".visualizer"` already uses
                // elsewhere in this file rather than new named constants.
                synth::ui::LayoutOptions spacerLayout;
                spacerLayout.main = synth::ui::Extent::Weight(1.0f);
                const auto emptyDraw = [](synth::ui::Bounds) -> std::vector<synth::ui::DrawCommand> {
                    return {};
                };
                b.Draw(std::string(FroggersNodeIds::kModulationHeader) + ".spacer.left", spacerLayout, emptyDraw);

                synth::ui::ControlStyle prevStyle{};
                prevStyle.action = synth::ui::Action::Named(FroggersActions::kPagePrevious);
                prevStyle.layout.main = synth::ui::Extent::Px(kModulationHeaderRowHeight);
                prevStyle.layout.cross = synth::ui::Extent::Px(kModulationHeaderRowHeight);
                b.Draw(FroggersNodeIds::kPagePrevArrow, BuildPagePrevArrowDrawCommands, prevStyle);

                synth::ui::ControlStyle nextStyle{};
                nextStyle.action = synth::ui::Action::Named(FroggersActions::kPageNext);
                nextStyle.layout.main = synth::ui::Extent::Px(kModulationHeaderRowHeight);
                nextStyle.layout.cross = synth::ui::Extent::Px(kModulationHeaderRowHeight);
                b.Draw(FroggersNodeIds::kPageNextArrow, BuildPageNextArrowDrawCommands, nextStyle);

                b.Draw(std::string(FroggersNodeIds::kModulationHeader) + ".spacer.right", spacerLayout, emptyDraw);
                return;
            }

            synth::ui::LayoutOptions titleLayout;
            titleLayout.main = synth::ui::Extent::Weight(1.0f);
            b.Draw(FroggersNodeIds::kModulationHeaderTitle, titleLayout,
                   [drillLevel](synth::ui::Bounds extent) -> std::vector<synth::ui::DrawCommand> {
                       std::vector<synth::ui::DrawCommand> commands;
                       commands.push_back(synth::ui::DrawCommand::Fill(
                           synth::ui::Bounds{0.0f, 0.0f, extent.width, extent.height},
                           kModulationHeaderBandColor));
                       commands.push_back(synth::ui::DrawCommand::Text(
                           synth::ui::Bounds{0.0f, 0.0f, extent.width, extent.height},
                           "Modulation Level " + std::to_string(drillLevel), kModulationHeaderTextStyle));
                       return commands;
                   });
        });
    }

    // Rows 3-6: the 16-slot grid, 4 slots per row -- LOOPED over
    // `FroggersEncoderGridLayout::kColumns`, using the row/col mapping
    // `firstEncoderIndex / kColumns` (in spirit: `ix / kColumns`/
    // `ix % kColumns`).
    void AppendEncoderRow(synth::ui::Builder& builder, std::size_t firstEncoderIndex) const {
        const std::size_t row = firstEncoderIndex / FroggersEncoderGridLayout::kColumns;
        synth::ui::LayoutOptions rowLayout;
        // `kGrownRowHeight` (88, not the sibling rows' 68) -- see
        // that constant's own comment for why a weight VALUE equal to the
        // target px height, alongside `AppendPageTabsRow`/
        // `AppendTwoButtonRow`'s matching change below, resolves this
        // row's height EXACTLY at the default window, growing only the
        // encoder rows and leaving page tabs/Randomize/Reset pixel-
        // identical to today.
        rowLayout.main = synth::ui::Extent::Weight(FroggersEncoderGridLayout::kGrownRowHeight);
        rowLayout.cross = synth::ui::Extent::Weight(1.0f);
        rowLayout.padding = 0.0f;
        rowLayout.gap = FroggersEncoderGridLayout::kGap;
        builder.Row(FroggersNodeIds::EncoderRow(row), rowLayout, [this, firstEncoderIndex](synth::ui::Builder& b) {
            for (std::size_t column = 0; column < FroggersEncoderGridLayout::kColumns; ++column) {
                AppendEncoderCell(b, firstEncoderIndex + column);
            }
        });
    }

    // One encoder cell. Reads the SAME
    // `context_->uiState->slots[0]` snapshot whether it currently holds the
    // parameter grid or a drilled-in modulation-detail grid (Bank::
    // OpenModulationView/Deselect swap `visible_`'s contents; this surface
    // has no branch of its own for "which grid" -- see the drill-in note
    // below) -- and it is the ONLY place this surface reads a
    // `Parameter::UIState`.
    //
    // IMPORTANT: this loop must NOT
    // re-derive slot->parameter from `FroggersBankLayouts()`/
    // `PageParameter()`/`Crispy()`/`Crunchy()` -- those are
    // construction-time accessors that do not reflect modulation drill-in
    // substitution. `context_->uiState->slots[0].cells[ix]` already reflects
    // `Bank::VisibleParameter(ix)` (BankSlot::PopulateUIState publishes
    // exactly that), so this file already satisfies that requirement and did
    // not need to change to do so.
    //
    // A disconnected cell still holds its place in the grid (Braid4UI.hpp's
    // own EmitEncoderCell idiom): in this weight-resolved grid, omitting the
    // node entirely would let its siblings' weights redistribute and shift
    // position, silently RESEQUENCING the remaining cells on every drill-in
    // change. Always emitting the node keeps the grid geometry stable and is
    // the established Sheaf idiom this surface's own header comment points
    // at `Braid4UI::EmitEncoderCell`
    // (External/Sheaf/projects/synth/apps/braid-4/Braid4UI.hpp).
    // What that node draws is a separate question: a disconnected cell draws
    // a dimmed, disabled encoder (below) rather than nothing, so an
    // unavailable source reads as present-but-inert instead of a blank gap
    // in the grid. It stays unreachable either way -- no press/drag action,
    // no visualizer underlay -- while a disconnected source has no signal to
    // press, drag, or underlay.
    //
    // This cell carries no drill-level text. The drill-level indicator is a
    // dedicated header ROW spanning the grid's width, between the page tabs
    // row and the first parameter row -- see AppendModulationHeaderRow() and
    // FroggersNodeIds::kModulationHeader -- not a badge on this cell (its
    // job is only to go back a level; see AppendScopeCell's own comment
    // above for why the indicator lives in neither place). Verified:
    // FroggersSurfaceTests.cpp's
    // modulation_header_sits_below_page_row_and_above_parameter_cells
    // asserts nothing reading "BACK" is drawn anywhere in the tree.

    void AppendEncoderCell(synth::ui::Builder& builder, std::size_t ix) const {
        const bool showingModulationView =
            context_ != nullptr && context_->uiState != nullptr && context_->uiState->slotCapacity > 0 &&
            context_->uiState->slots[0].showingModulationView.load(std::memory_order_relaxed);

        synth::ui::EncoderDrawState state{};
        synth::ui::Visualizer* visualizer = nullptr;
        if (context_ != nullptr && context_->uiState != nullptr && context_->uiState->slotCapacity > 0) {
            const synth::BankSlot::UIState& slotState = context_->uiState->slots[0];
            if (ix < slotState.cellCapacity) {
                // `EncoderDrawStateFromParameter` reads only
                // `Parameter::UIState.values[]` (the post-fuego,
                // post-modulation published display center) -- never
                // `.rawKnobValue`.
                state = synth::ui::EncoderDrawStateFromParameter(slotState.cells[ix]);
                visualizer = slotState.cells[ix].visualizer.load(std::memory_order_relaxed);
            }
        }
        // Two separate flags: what this cell draws, and what it can be
        // reached by (press/drag/underlay) -- a disconnected cell draws a
        // dimmed disabled encoder rather than nothing, so the two need
        // independent names. `unreachableWhileDisconnected` covers
        // reachability: a disconnected source has no signal, so it stays
        // unreachable and underlay-free whether or not it is being drilled
        // into. `disabledCell` names the drawing decision -- true for every
        // disconnected cell, in or out of the modulation view, since
        // `BuildEncoderDrawCommands` itself early-returns `{}` for
        // `!connected` (External/Sheaf/projects/synth/include/synth/EncoderDraw.hpp) and this app
        // overrides that with a dimmed render instead.
        const bool unreachableWhileDisconnected = showingModulationView && !state.connected;
        const bool disabledCell = !state.connected;
        state.hasVisualizerUnderlay =
            !unreachableWhileDisconnected && visualizer != nullptr && visualizer->Visible();

        // No frame is drawn: the parameter card's frame outline and the
        // encoder's own modulation ring are both emitted by ONE Sheaf
        // function, BuildEncoderDrawCommands
        // (External/Sheaf/projects/synth/include/synth/EncoderDraw.hpp), from geometry this app does not
        // own: the ring's radius is `baseRadius = min(bounds.width,
        // bounds.height) * 0.43f` (inside `BuildEncoderDrawCommands`,
        // External/Sheaf/projects/synth/include/synth/EncoderDraw.hpp) while the frame is
        // `bounds` inset by a fixed 1px with a 6px corner radius
        // (also inside `BuildEncoderDrawCommands`,
        // External/Sheaf/projects/synth/include/synth/EncoderDraw.hpp) -- two different linear functions of the
        // SAME cell bounds this app supplies, so at this app's actual cell
        // size they collide (measured: ring outer edge 38.057px from centre
        // vs. frame inner edge 36.257px, a 1.80px overlap). The two edges'
        // slopes (0.43 vs. 0.5) mean a SMALLER cell only widens this
        // collision, never closes it, so shrinking the bounds this app
        // passes into BuildEncoderDrawCommands is not a usable fix. The one
        // app-facing lever is `EncoderDrawState::wantsFrame`
        // (External/Sheaf/projects/synth/include/synth/EncoderDraw.hpp, defaults true) -- the same field Sheaf's own
        // MiniAppUI.hpp sets per-visualizer (External/Sheaf/projects/synth/apps/miniapp/MiniAppUI.hpp).
        // Set once, unconditionally, for every encoder cell -- this is the
        // surface's one call site into BuildEncoderDrawCommands (below) --
        // not per-bank or per-cell.
        state.wantsFrame = false;

        const std::string encoderId = FroggersNodeIds::Encoder(ix);
        if (!unreachableWhileDisconnected && visualizer != nullptr && visualizer->Visible()) {
            // Bump/comb transfer-function underlays and
            // modulation-source underlays render here automatically. The
            // underlay is deferred to the resolved bounds of its SIBLING
            // encoder cell via `overlayOf`
            // (External/Sheaf/projects/synth/include/synth/PortableUILayout.hpp) -- the same mechanism Braid4UI.hpp's own
            // EmitEncoderCell uses, needed here because the cell's own
            // bounds are not known until the layout resolves.
            synth::ui::LayoutOptions underlayLayout;
            underlayLayout.overlayOf = encoderId;
            builder.Draw(encoderId + ".visualizer", underlayLayout, [visualizer](synth::ui::Bounds extent) {
                visualizer->SetBounds(extent);
                return visualizer->Draw();
            });
        }

        // The drill-in press dispatches from `ControlStyle::action` (plain
        // click) and the drag from the separate `pointerDragAction` field --
        // no conflict, no post-Build() patch. Neither is set while
        // unreachableWhileDisconnected: a
        // disconnected cell in the modulation view is inert -- it still
        // draws, as a dimmed disabled encoder (below), but has no signal
        // to press, drag, or underlay.
        // `Draw` has no case in `metrics::IntrinsicFor`
        // (External/Sheaf/projects/synth/include/synth/PortableUIMetrics.hpp, `default: {0,0,0,0}`) -- an in-flow
        // Draw node needs an explicit `layout.main` or it resolves to zero
        // size (this file's header comment makes the same point about the
        // transport plates). `Weight(1)` makes the cell fill its equal share
        // of the row, exactly `Braid4CellLayout()`'s own square-cell idiom
        // (`cross` stays the library default `Weight(1)` too, filling the
        // row's height).
        synth::ui::ControlStyle cellStyle{};
        cellStyle.layout.main = synth::ui::Extent::Weight(1.0f);
        if (!unreachableWhileDisconnected) {
            cellStyle.action = synth::ui::Action::WithValue(FroggersActions::kEncoderPress, std::to_string(ix));
            cellStyle.pointerDragAction =
                synth::ui::Action::WithValue(FroggersActions::kEncoderDrag, FormatFroggersEncoderDrag(ix, 0.0f));
        }
        // A SINGLE-ROW 14-segment strip, sized to the longest approved label
        // (`kApprovedLabelGridColumns`), living ENTIRELY in the ~20px of
        // cell height added below the ring
        // (`FroggersEncoderGridLayout::kLabelBandHeight`) -- never inside
        // the ring's own sub-extent, so it structurally cannot intersect
        // the ring's drawn arc. `EncoderDrawState` has no field
        // to change Sheaf's own trailing block, but `BuildEncoderDrawCommands`
        // returns its command vector BY VALUE with that block appended LAST
        // (inside `BuildEncoderDrawCommands`,
        // External/Sheaf/projects/synth/include/synth/EncoderDraw.hpp) and `BuildFourteenSegmentCommands` is
        // public/inline -- so this app strips Sheaf's trailing block by its
        // exact, deterministic size and appends its own single-row strip
        // instead.
        const std::size_t bankIx = CurrentPageIndex();
        builder.Draw(
            encoderId,
            [state, disabledCell, bankIx, ix, showingModulationView](synth::ui::Bounds extent) {
                // `extent` is THIS cell's full resolved bounds, now
                // `FroggersEncoderGridLayout::kLabelBandHeight` px TALLER
                // than the ring alone needs (that constant is exactly how
                // much `AppendEncoderRow` grew each encoder row by). Handing
                // `BuildEncoderDrawCommands` the FULL extent would grow the
                // ring too (it derives `baseRadius`/`centerY` from
                // `min(width,height)` of whatever extent it is given,
                // External/Sheaf/projects/synth/include/synth/EncoderDraw.hpp) -- so this app instead hands it a
                // SUB-extent, anchored at the cell's own top-left origin
                // (unaffected by growth, since the label band is added
                // BELOW, not above), whose height is `extent.height` minus
                // the label band -- at the default window this resolves to
                // EXACTLY `kUnchangedRowHeight` (68.0px, bit-for-bit: see
                // `FroggersEncoderGridLayout`'s own comment for why the row
                // weights make this an exact division, not an
                // approximation), reproducing today's ring geometry
                // byte-identically. At other window sizes the ring still
                // scales with the window, just `kLabelBandHeight` narrower
                // than the full cell -- the SAME resize behaviour as before
                // the label band was added, offset by a constant. Both the
                // disabled and connected branches below share this same
                // sub-extent -- a disabled cell's ring occupies the same
                // geometry, just dimmer.
                const synth::ui::Bounds ringExtent{
                    0.0f,
                    0.0f,
                    extent.width,
                    std::max(0.0f, extent.height - FroggersEncoderGridLayout::kLabelBandHeight),
                };

                // Sheaf's trailing block size is exactly
                // kSheafLabelCommandsPerChar (15: 14 AppendCharacter
                // segment polygons -- 4 horizontal/G-bar + 6 vertical + 4
                // diagonal, in `AppendCharacter`,
                // External/Sheaf/projects/synth/include/synth/EncoderDraw.hpp, every one of which
                // returns a non-empty polygon at any real display size --
                // plus 1 unconditional decimal-point FillEllipse,
                // also in `AppendCharacter`,
                // External/Sheaf/projects/synth/include/synth/EncoderDraw.hpp) times kSheafLabelDefaultChars
                // (4, BuildFourteenSegmentCommands's own numChars default,
                // External/Sheaf/projects/synth/include/synth/EncoderDraw.hpp, `BuildFourteenSegmentCommands`)
                // = 60 commands, always, whenever
                // `BuildEncoderDrawCommands` is handed a `connected` state
                // -- both branches below force `connected` true (the
                // connected branch already has it; the disabled branch
                // forces it on a copy, see below), so both strip this same
                // trailing block by the same exact, guarded size. Guarded
                // (size >= that) so a cell that somehow emitted fewer
                // commands can never underflow.
                constexpr std::size_t kSheafLabelCommandsPerChar = 15;
                constexpr std::size_t kSheafLabelDefaultChars = 4;
                constexpr std::size_t kSheafLabelCommandCount =
                    kSheafLabelCommandsPerChar * kSheafLabelDefaultChars;

                if (disabledCell) {
                    // A disconnected source is inert and has nothing to
                    // report. Build a dimmed COPY of `state` -- never
                    // mutate the captured original, since it is not
                    // reused after this branch returns -- with `connected`
                    // forced true so `BuildEncoderDrawCommands` draws the
                    // ring instead of early-returning `{}`
                    // (External/Sheaf/projects/synth/include/synth/EncoderDraw.hpp). `AdjustBrightness` is this
                    // app's existing dimming idiom
                    // (FroggersModulation.hpp's `FroggersModulationSlate::RegisterSources` et al.). An unavailable
                    // source cannot be affecting anything, so both
                    // indicator bitmasks AND the colour vectors they index
                    // into are cleared together -- leaving stale colours
                    // published while clearing only the masks would still
                    // satisfy `BuildEncoderDrawCommands`'s own bounds
                    // assert (External/Sheaf/projects/synth/include/synth/EncoderDraw.hpp), but a disabled source
                    // must not still look wired to specific modulators or
                    // gestures.
                    // A disconnected source has no colour to dim. The slate
                    // publishes `Color::Off` for it (Rgb(0,0,0)), and
                    // `AdjustBrightness` darkens toward black, so scaling it
                    // is a no-op and the cell would render identically to a
                    // connected one. The disabled cell therefore carries an
                    // explicit muted grey: the body stroke is drawn from
                    // `baseColor` (EncoderDraw.hpp's own
                    // `ScaleAlpha(state.baseColor, 0.9f)`), so this is what
                    // makes it visible as a control and flat enough to read
                    // as unavailable.
                    // Deliberately NOT `pagestyle::kDisabledText` or
                    // `kDisabledButton` (External/Sheaf/projects/synth/include/synth/RuntimePageStyle.hpp). Those
                    // are the runtime chrome's palette for configuration
                    // pages; the encoder grid is a separate visual system
                    // coloured per modulation source, and this surface
                    // references that palette nowhere else. Reusing one of
                    // them here would couple the grid to the config pages'
                    // colour language to avoid repeating a grey. If the two
                    // ever should agree, that is a palette decision made
                    // once for both, not an include added here.
                    const synth::Color kDisabledCellColor = synth::Color::Rgb(90, 96, 100);
                    synth::ui::EncoderDrawState disabledState = state;
                    disabledState.connected = true;
                    disabledState.baseColor = kDisabledCellColor;
                    disabledState.modulatorsAffectingMask = 0;
                    disabledState.gesturesAffectingMask = 0;
                    disabledState.modulatorColors.clear();
                    disabledState.gestureColors.clear();

                    std::vector<synth::ui::DrawCommand> commands =
                        synth::ui::BuildEncoderDrawCommands(disabledState, ringExtent);
                    // No value readout on a disabled cell: strip Sheaf's
                    // trailing label block by the same exact, guarded size
                    // the connected branch strips below, and append
                    // nothing in its place -- an unavailable source has no
                    // value or label to show.
                    if (commands.size() >= kSheafLabelCommandCount) {
                        commands.resize(commands.size() - kSheafLabelCommandCount);
                    }
                    return commands;
                }

                std::vector<synth::ui::DrawCommand> commands = synth::ui::BuildEncoderDrawCommands(state, ringExtent);
                if (commands.size() >= kSheafLabelCommandCount) {
                    commands.resize(commands.size() - kSheafLabelCommandCount);
                }

                // The label band: everything below the ring's own
                // sub-extent (`ringExtent.height`), i.e. exactly the added
                // `kLabelBandHeight` strip -- by construction this can never
                // reach up into `ringExtent`'s own [0, ringExtent.height)
                // span, which is the whole of what `BuildEncoderDrawCommands`
                // was given to draw into.
                const float bandTop = ringExtent.height;
                const float bandHeight = std::max(0.0f, extent.height - bandTop);
                const float plateWidth = extent.width * 0.94f;
                const float plateLeft = (extent.width - plateWidth) * 0.5f;
                const synth::ui::Bounds rowBounds{plateLeft, bandTop, plateWidth, bandHeight};

                const synth::Color cellColor = state.baseColor;
                const synth::Color onColor = synth::Brighten(cellColor, 0.45f);
                const synth::Color offColor = synth::Color::Rgb(
                    synth::kSurfaceBackground.r + 4, synth::kSurfaceBackground.g + 6,
                    synth::kSurfaceBackground.b + 6);
                // The plate matches the surface background so the label
                // band reads as glyphs sitting directly on the surface,
                // not a separate chip. It stays opaque because visualizer
                // underlays cover the whole cell, label band included.
                const synth::Color plateColor = synth::kSurfaceBackground;
                commands.push_back(synth::ui::DrawCommand::FillRoundedRect(
                    rowBounds, bandHeight * 0.15f, plateColor));

                // labels.md is the authority for what RENDERS here,
                // not `FroggersParamSpec::name`/`shortName`
                // (FroggersApprovedLabels()'s own header comment). Slots
                // 14/15 (Crispy/Crunchy) have no page spec, so they read
                // `FroggersApprovedGlobalLabel` instead. A modulation
                // drill-in view ALSO substitutes a DIFFERENT Parameter into
                // this same physical slot index (Bank::OpenModulationView,
                // External/Sheaf/projects/synth/src/ParameterModulation.cpp -- BankSlot::
                // PopulateUIState's cells[] then reflects that substituted
                // parameter, not the bank's own page layout; this is
                // exactly why this file's own AppendEncoderCell header
                // comment says this loop must NOT re-derive slot->parameter
                // from FroggersBankLayouts()) -- looking the approved list
                // up by (bank,slot) in that case would silently mislabel
                // the substituted control, and that substituted parameter
                // is outside labels.md's 86-entry scope in any case. This
                // one case falls back to the live `state.shortLabel`
                // (EncoderDrawStateFromParameter's own source).
                std::string approvedLabel;
                if (!showingModulationView && bankIx < kFroggersPageCount) {
                    approvedLabel = (ix < kFroggersParamsPerBank) ? FroggersApprovedLabels()[bankIx][ix]
                                                                   : FroggersApprovedGlobalLabel(ix);
                } else {
                    approvedLabel = state.shortLabel;
                }
                std::vector<synth::ui::DrawCommand> rowCommands = BuildEncoderLabelRowCommands(
                    approvedLabel, rowBounds, onColor, offColor, kApprovedLabelGridColumns);
                commands.insert(commands.end(), rowCommands.begin(), rowCommands.end());

                return commands;
            },
            cellStyle);
    }

    // Row 7: Randomize Page | Randomize All, each spanning 2 of the 4
    // encoder columns (`Extent::Weight(2)`, matching the encoder rows'
    // per-column `Weight(1)` unit so the two rows visually align). Row 1 is
    // page tabs only (AppendPageTabsRow() above); Randomize Page and
    // Randomize All sit together in this last row.
    // Exactly one Randomize All control exists anywhere in this surface.
    // The ONE two-half-width-buttons row builder: Randomize and Reset both
    // route through it, so the "same size halves" property is structural --
    // the two rows cannot drift apart.
    void AppendTwoButtonRow(synth::ui::Builder& builder, const char* rowId,
                            const FroggersCellMap::ButtonCell& left,
                            const FroggersCellMap::ButtonCell& right) const {
        synth::ui::LayoutOptions rowLayout;
        // `kUnchangedRowHeight`, same reasoning as AppendPageTabsRow
        // above -- Randomize and Reset (this method's two callers) both
        // stay pixel-identical to today at the default window.
        rowLayout.main = synth::ui::Extent::Weight(FroggersEncoderGridLayout::kUnchangedRowHeight);
        rowLayout.cross = synth::ui::Extent::Weight(1.0f);
        rowLayout.padding = 0.0f;
        rowLayout.gap = FroggersEncoderGridLayout::kGap;
        builder.Row(rowId, rowLayout, [&](synth::ui::Builder& b) {
            synth::ui::ControlStyle leftStyle{};
            leftStyle.layout.main = synth::ui::Extent::Weight(2.0f);
            leftStyle.layout.cross = synth::ui::Extent::Intrinsic();
            b.Button(left.id, left.label, synth::ui::Action::Named(left.action), leftStyle);

            synth::ui::ControlStyle rightStyle{};
            rightStyle.layout.main = synth::ui::Extent::Weight(2.0f);
            rightStyle.layout.cross = synth::ui::Extent::Intrinsic();
            b.Button(right.id, right.label, synth::ui::Action::Named(right.action), rightStyle);
        });
    }

    void AppendRandomizeRow(synth::ui::Builder& builder) const {
        AppendTwoButtonRow(builder, FroggersNodeIds::kRandomizeRow,
                           FroggersCellMap::kRandomizeResetButtons[0],
                           FroggersCellMap::kRandomizeResetButtons[1]);
    }

    // Row 7, directly below Randomize. Deliberately the SAME shape as
    // AppendRandomizeRow above -- row Weight(1), two Buttons at Weight(2) of
    // four weight-units, i.e. two equal halves, same gap. Reset is
    // Randomize's exact inverse in scope, so the two rows are each other's
    // visual twin.
    void AppendResetRow(synth::ui::Builder& builder) const {
        AppendTwoButtonRow(builder, FroggersNodeIds::kResetRow,
                           FroggersCellMap::kRandomizeResetButtons[2],
                           FroggersCellMap::kRandomizeResetButtons[3]);
    }

    bool PageSelected(std::size_t bankIx) const {
        if (context_ == nullptr || context_->uiState == nullptr || bankIx >= context_->uiState->bankCapacity) {
            return bankIx == 0;
        }
        return context_->uiState->banks[bankIx].selected.load(std::memory_order_relaxed);
    }

    // Which page (index into FroggersBankLayouts(), same order banks are
    // created in -- FroggersParameters.hpp's Init() loop -- and the same
    // order `uiState->banks[]` is populated in, by bank index against
    // `ParameterManager::PopulateUIState`'s own `banks_` in
    // ParameterModulation.cpp) is currently
    // selected, for AppendEncoderCell's label-source lookup below. Same
    // default (0) PageSelected() above already uses when uiState isn't
    // ready yet.
    std::size_t CurrentPageIndex() const {
        return context_ == nullptr ? 0 : FroggersVisiblePageIndex(*context_);
    }

    void HandleAction(const synth::ui::Action& action) {
        if (app_ == nullptr) {
            return;
        }

        // Generic, safe over the existing uiBus (see this file's header
        // comment): transport, scene select/blend, encoder drag.
        if (action.name == FroggersActions::kPlay) {
            // Play disarms the Freeze latch: a
            // latched Freeze holds the voice gate OPEN unconditionally
            // (FroggersAppCore's `setGate(gateOpen || FreezeLatched())`),
            // so starting the transport with the latch still engaged would
            // run the sequencer while every voice was pinned sustaining,
            // and leave the delay frozen at its latch overdrive -- Play
            // would not actually return the instrument to playing. See
            // LatchThenTransport's own comment for the happens-before
            // ordering this relies on.
            StartTransport();
            return;
        }
        if (action.name == FroggersActions::kStop) {
            // A later Stop always means stop, regardless of latch state.
            // See LatchThenTransport's own comment for the happens-before
            // ordering this relies on.
            LatchThenTransport(false, synth::MessageIn::Stop(NowMicros()), false);
            return;
        }
        if (action.name == FroggersActions::kFreeze) {
            // Freeze is self-contained: engaging it stops the transport
            // itself ("The Freeze button alone reaches the
            // sustained drone" is the governing requirement), so the drone needs no separate
            // Stop press and a later Stop always means stop (see the kStop
            // branch above).
            //
            // ENGAGE (latch false -> true): same as the kStop branch above,
            // including SetDesiredTransportRunning(false) -- see
            // LatchThenTransport's own comment for why both the ordering
            // and the recorded intent matter here. Records whether the
            // transport was actually running right before it stops, in
            // freezeEngagedWhileTransportRunning_ below, for RELEASE to read.
            //
            // RELEASE (latch true -> false): returns the transport to where
            // it was when Freeze engaged. If it was running,
            // StartTransport() runs -- the same call the kPlay branch above
            // makes, so releasing Freeze is pressing Play. If it was
            // already stopped, this only clears the latch;
            // FroggersAppCore's existing "latch released while already
            // stopped" edge (`latchReleasedWhileStopped`, FroggersAppCore.hpp)
            // is what notices the plain atomic flip and runs the teardown
            // that silences the held drone.
            const bool engaging = !app_->FreezeLatched();
            if (engaging) {
                freezeEngagedWhileTransportRunning_ = FroggersTransportIsRunning(context_);
                LatchThenTransport(true, synth::MessageIn::Stop(NowMicros()), false);
            } else if (freezeEngagedWhileTransportRunning_) {
                StartTransport();
            } else {
                app_->SetFreezeLatched(false);
            }
            return;
        }
        if (action.name == FroggersActions::kRecord) {
            // Unlike Freeze's plain latch flip, Record can REFUSE (transport
            // stopped -- FroggersAppCore::
            // ArmRecording's own comment) and produces a result the host
            // cares about (captured audio to export). A refusal shows as
            // this surface's own transport notice (transportNotice_,
            // cleared once a recording arms or Play is pressed); a
            // finished capture with data is queued as a file export
            // (FroggersAppCore::QueueRecordingExport) for the engine's
            // installed handler to pick up -- a direct message-thread call,
            // same as kFreeze above and unlike the presses below, which
            // travel to the audio thread as a `MessageIn::AppCommand`.
            if (!app_->RecordArmed()) {
                if (!app_->ArmRecording()) {
                    transportNotice_ = app_->RecordRefusalReason();
                } else {
                    transportNotice_.clear();
                }
                return;
            }
            app_->StopRecording();
            if (app_->RecordedFrameCount() > 0) {
                app_->QueueRecordingExport();
            }
            return;
        }
        if (action.name == FroggersActions::kSceneSelect) {
            // Scene 1/Scene 2 now toggle the blend to its extremes rather
            // than reassigning a stored-scene endpoint. Verified:
            // FroggersParameters.hpp wires `manager.SetSceneEndpoints(0, 1)`
            // once at Init() (fixed for this app's lifetime), and
            // Parameter::ComputeRawCenter's blend arithmetic
            // (External/Sheaf/projects/synth/src/ParameterModulation.cpp)
            // is `SceneCenter(leftScene) * (1-blend) + SceneCenter(rightScene)
            // * blend` -- blend 0.0 is pure leftScene (scene index 0), blend
            // 1.0 is pure rightScene (scene index 1). So scene index 0
            // ("Scene 1") -> blend 0.0, scene index 1 ("Scene 2") -> blend
            // 1.0, matching the button's own ordinal exactly.
            const std::size_t sceneIx = FroggersParseSize(action.value, 0);
            const float blend = sceneIx == 0 ? 0.0f : 1.0f;
            PushMessage(synth::MessageIn::SetSceneBlend(NowMicros(), blend));
            return;
        }
        if (action.name == FroggersActions::kSceneBlend) {
            // action.value carries the DISPLAYED 1.0-2.0 reading (see
            // AppendSceneBlendGroup()'s `b.Slider(...)` and
            // kSceneBlendDisplayOffset's own comment above), so the fallback
            // fed to FroggersParseFloat must be in that same displayed
            // domain -- kSceneBlendDisplayOffset (the bottom of the
            // displayed range, i.e. "no offset applied"), not a pre-offset
            // 0.0f. 0.0f would be a displayed-domain value that only happens
            // to land on the right blend (0.0, Scene 1's extreme) after the
            // subtract-and-clamp below coincidentally rescues it;
            // kSceneBlendDisplayOffset is the value that is actually correct
            // in the domain being parsed, not merely one that clamps to the
            // same place. Subtract the offset back out and clamp to [0,1]:
            // the message must still carry Sheaf's own 0..1 blend, and a
            // slider edge value (or a malformed action) must never push an
            // out-of-range blend.
            const float displayed = FroggersParseFloat(action.value, kSceneBlendDisplayOffset);
            const float blend = std::clamp(displayed - kSceneBlendDisplayOffset, 0.0f, 1.0f);
            PushMessage(synth::MessageIn::SetSceneBlend(NowMicros(), blend));
            return;
        }
        if (action.name == FroggersActions::kInputSelect) {
            // Cycles to the next option in the list this surface was last
            // handed (SetInputOptions(), above) -- None -> channel 1 -> ...
            // -> Sum -> None. Applied to inputSelection_ directly (a live
            // render-state write, the same "surface owns this fact, the
            // plugin listens for changes" split pluginHostMode_ established
            // above) and handed to the plugin's callback so IT can
            // re-validate against the actual bus and decide consent --
            // this method never decides connectedness itself.
            const int count = static_cast<int>(inputOptionLabels_.size());
            if (count > 0) {
                const int next = (inputSelection_ + 1) % count;
                inputSelection_ = next;
                if (inputSelectionChangedCallback_) {
                    inputSelectionChangedCallback_(next);
                }
            }
            return;
        }
        if (action.name == FroggersActions::kEncoderDrag) {
            std::size_t position = 0;
            float delta = 0.0f;
            if (!ParseFroggersEncoderDrag(action.value, position, delta) || std::fabs(delta) < 0.0001f) {
                return;
            }
            PushMessage(synth::MessageIn::ParamIncDec(NowMicros(), /*slotIx=*/0, position, delta));
            return;
        }

        // The eight presses that travel as synth::MessageIn::AppCommand --
        // encoder press (drill-in cap), page select and its two carousel
        // arrows, Randomize All/Page, Reset All/Page -- mapped from their
        // action name to the app's own command number through one table.
        // The arrows do not resolve their own target or gate on
        // DrillLevel() here: FroggersAppCore::ApplyAppCommand does both, from
        // its own current page, on the audio thread. FroggersParseSize on an
        // action with no value (every command but encoder press and page
        // select) returns its 0 fallback, which every one of those commands
        // ignores, so one parse serves all eight.
        static constexpr struct {
            const char* actionName;
            FroggersCommand command;
        } kAppCommandTable[] = {
            {FroggersActions::kEncoderPress, FroggersCommand::kEncoderPress},
            {FroggersActions::kPageSelect, FroggersCommand::kPageSelect},
            {FroggersActions::kPagePrevious, FroggersCommand::kPagePrevious},
            {FroggersActions::kPageNext, FroggersCommand::kPageNext},
            {FroggersActions::kRandomizeAll, FroggersCommand::kRandomizeAll},
            {FroggersActions::kRandomizePage, FroggersCommand::kRandomizePage},
            {FroggersActions::kResetAll, FroggersCommand::kResetAll},
            {FroggersActions::kResetPage, FroggersCommand::kResetPage},
        };
        for (const auto& entry : kAppCommandTable) {
            if (action.name == entry.actionName) {
                PushMessage(synth::MessageIn::AppCommand(
                    NowMicros(), static_cast<std::size_t>(entry.command),
                    static_cast<float>(FroggersParseSize(action.value, 0))));
                return;
            }
        }
        if (action.name == FroggersActions::kBpm) {
            // Belt-and-suspenders -- the slider itself renders as a
            // non-interactive StatusText while slaved (AppendBpmControl(),
            // above), so this action should not even be reachable then; the
            // guard here means the audio-thread's own no-op
            // (MasterClock::SetTempoBpm's `syncConfig_.receiveClock` check)
            // is not the ONLY thing preventing a stray request from an
            // out-of-date rendered tree. Reads the same requested sync
            // configuration the audio thread reads (AppContext::
            // syncConfiguration), never a mirror this surface keeps.
            if (context_ != nullptr && context_->syncConfiguration && !FroggersExternallyClocked(context_)) {
                const float bpm = FroggersParseFloat(action.value, 120.0f);
                const float normalized = (bpm - kFroggersBpmMin) / (kFroggersBpmMax - kFroggersBpmMin);
                PushMessage(synth::MessageIn::SetTempoBpmNormalized(NowMicros(), normalized));
            }
            return;
        }
        if (action.name == FroggersActions::kViewportNarrow) {
            // A UI-only flag, same as pluginHostMode_ -- no PushMessage,
            // the audio thread has no concept of viewport width.
            narrowViewport_ = (action.value == "1");
            return;
        }
    }

    void PushMessage(const synth::MessageIn& message) {
        if (context_ != nullptr && context_->uiBus != nullptr) {
            context_->uiBus->Push(message);
        }
    }

    // Disarm the Freeze latch BEFORE pushing the transport message, not
    // after -- the two reach the audio thread by different paths
    // (freezeLatched_ is a plain release/acquire atomic FreezeLatched()
    // re-reads every sample; the MessageIn travels through MessageInBus's
    // ring buffer, drained once per block by Engine::DrainMessageBus()
    // BEFORE FroggersAppCore::ProcessBlock's per-sample transport-edge check
    // runs). Writing the latch first in this thread's program order, before
    // the Push() whose internal fetch_add is the release half of that ring
    // buffer's release/acquire pair, makes the write happen-before the
    // corresponding acquire load in MessageInBus::Pop -- and therefore
    // happen-before every later read on the audio thread this sample. This
    // order is deliberate, not cosmetic. SetDesiredTransportRunning records
    // the intent behind the push so a later audio-device renegotiation
    // (which resets MasterClock::TransportState() to Stopped out from under
    // the app, with no message of its own) can re-assert it.
    void LatchThenTransport(bool latched, synth::MessageIn message, bool running) {
        app_->SetFreezeLatched(latched);
        PushMessage(message);
        app_->SetDesiredTransportRunning(running);
    }

    // Starts the transport: disarms the Freeze latch, pushes the same
    // MessageIn::Start every Play press pushes, records the desired-running
    // intent, and clears the transport notice -- the one definition of
    // "start the transport" the kPlay branch above and the Freeze release
    // branch above both call, so the two can never drift apart in what
    // starting means.
    void StartTransport() {
        LatchThenTransport(false, synth::MessageIn::Start(NowMicros()), true);
        transportNotice_.clear();
    }

    std::uint64_t NowMicros() const {
        if (context_ != nullptr && context_->now) {
            return context_->now();
        }
        return fallbackTimestamp_++;
    }

    synth::AppContext* context_ = nullptr;
    FroggersAppCore* app_ = nullptr;
    ActionHandler outerHandler_;
    // See SetPluginHostMode()'s own comment. Defaults false, so every
    // existing construction of this class -- default constructed, this flag
    // never touched -- renders exactly as before.
    bool pluginHostMode_ = false;
    // Set only by kViewportNarrow, which a browser shell dispatches to
    // report its own viewport width. Purely a rendering choice -- which
    // outer split weights AppendLeftBlock/AppendRightBlock declare, and
    // whether the chrome block is a plain stack or a stack beside a button
    // column -- so it never reaches the audio thread, same as
    // pluginHostMode_ above. Defaults false, so every existing
    // construction of this class (default constructed, no such action ever
    // dispatched) renders exactly as before.
    bool narrowViewport_ = false;
    // The refusal text shown in the transport row until a recording arms or
    // the transport starts. Set by HandleAction's kRecord branch and
    // cleared by StartTransport() (the kPlay branch, and the Freeze release
    // that resumes playing), read fresh into the transport row lambda every
    // rebuild, same per-frame idiom as pluginHostMode_/narrowViewport_
    // above.
    std::string transportNotice_;
    // Whether the transport was running the moment Freeze last engaged the
    // latch (HandleAction's kFreeze branch, ENGAGE side) -- read on the
    // matching RELEASE so releasing Freeze can return the transport to
    // where it was: StartTransport() if it was running, a plain latch clear
    // if it was already stopped. Meaningless while the latch is
    // disengaged; only ever read immediately after `engaging` reads false.
    bool freezeEngagedWhileTransportRunning_ = false;
    // See SetInputOptions()'s own comment. Defaults to just "None" (index
    // 0), the same "unavailable" reading a disabled/zero-channel bus
    // produces -- so a plugin-host construction that has not yet called
    // SetInputOptions() (there is no such window in production; only a
    // bare-context surface, e.g. FroggersSurfaceTests.cpp's layout-only
    // tests, would ever observe this default) renders the control exactly
    // as if the bus were disabled, never as if something were already
    // selected.
    std::vector<std::string> inputOptionLabels_{"None"};
    int inputSelection_ = 0;
    std::function<void(int)> inputSelectionChangedCallback_;
    mutable std::uint64_t fallbackTimestamp_ = 1;
};

}  // namespace synth_froggers
