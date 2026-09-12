// Shared selectors/helpers for the site e2e suite. Node ids
// mirror `FroggersNodeIds` in app/FroggersUiSurface.hpp exactly, which is what
// Sheaf's browser UI backend publishes as
// each element's `data-synth-node-id` (External/Sheaf/projects/synth/
// browser/src/ui.ts, `updateNode()`).
import { expect } from "@playwright/test";

// FroggersNodeIds::kLeftBlock -- scope, transport, scenes, scene-blend, bpm.
export const LEFT_BLOCK_SELECTOR = '[data-synth-node-id="froggers.layout.left"]';
// FroggersNodeIds::kRightBlock -- the bank chrome + 16-slot encoder grid
// (FroggersUiSurface.hpp:114).
export const RIGHT_BLOCK_SELECTOR = '[data-synth-node-id="froggers.layout.right"]';
// FroggersNodeIds::EncoderRow(0..3) -- the four 4-wide slices that make up
// the 16-slot (4x4) encoder grid (FroggersUiSurface.hpp:201-206).
export const ENCODER_ROW_SELECTORS = [0, 1, 2, 3].map((row) => `[data-synth-node-id="froggers.layout.right.row.${row}"]`);
export const SYNTH_ROOT_SELECTOR = "#synth-root";
// FroggersNodeIds::kRandomizePage/kRandomizeAll/kResetPage/kResetAll
// (FroggersUiSurface.hpp:148-151), in the order
// FroggersCellMap::kRandomizeResetButtons lists them. The wide layout puts
// them in two rows below the encoder grid; the narrow layout puts the same
// four in a column inside the chrome block, beside the sliders.
export const RANDOMIZE_RESET_SELECTORS = [
  "froggers.randomize.page",
  "froggers.randomize.all",
  "froggers.reset.page",
  "froggers.reset.all",
].map((id) => `[data-synth-node-id="${id}"]`);
// FroggersNodeIds::kBpm -- the lowest of the chrome block's two sliders,
// and the one the narrow button column has to sit to the right of.
export const BPM_SELECTOR = '[data-synth-node-id="froggers.bpm"]';
// FroggersNodeIds::kLeftButtons -- the narrow chrome block's Randomize/Reset
// column, whose box is what the shell places Sheaf's sidebar under.
export const NARROW_BUTTON_COLUMN_SELECTOR = '[data-synth-node-id="froggers.layout.left.buttons"]';
// Sheaf's own runtime page buttons (RuntimePages.hpp:37-41), in the order the
// sidebar stacks them. The first one's LABEL is the app's own
// (RuntimeConfig::audioPageTitle); its node id is the runtime's and does not
// change with the rename.
export const SIDEBAR_BUTTON_SELECTORS = [
  "runtime.sidebar.audio",
  "runtime.sidebar.controllers",
  "runtime.sidebar.sync",
  "runtime.sidebar.file",
].map((id) => `[data-synth-node-id="${id}"]`);
export const SURFACE_ROOT_SELECTOR = '[data-synth-node-id="froggers.root"]';
// RuntimePages.hpp:34 `NodeIds::kSidebarRoot` -- Sheaf's own generic
// runtime-chrome sidebar (Audio/Controllers/Sync/File + CPU meter), a
// sibling of `froggers.root` under the composite `runtime.main.root`
// Sheaf's fitSurface actually scales (RuntimeMainComponent.hpp:197-214).
// Not a FroggersUiSurface node, but the mobile stack includes it as
// a third stacked block, alongside everything else above or
// below the grid.
export const SIDEBAR_SELECTOR = '[data-synth-node-id="runtime.sidebar.root"]';
// FroggersNodeIds::kPlay (FroggersUiSurface.hpp:222) -- a plain
// click-dispatch transport control (ControlStyle::action, not a drag
// action), the same "reaches the app" path the mobile-stacking suite's
// own scene-button test already exercises for a different control.
export const PLAY_SELECTOR = '[data-synth-node-id="froggers.transport.play"]';
// FroggersNodeIds::Encoder(ix) for the first slot of each of the 4 encoder
// rows (ix = row * 4 -- FroggersUiSurface.hpp:466-469's own
// `{RightKind::EncoderRow, 0/4/8/12}` firstEncoderIndex values), one
// canvas per row: the same per-row sampling convention
// ENCODER_ROW_SELECTORS above already uses, rather than all 16. Each
// encoder is a Draw-kind node, which Sheaf's browser UI backend gives
// exactly one <canvas> child (ui.ts's `createNode`, the NodeKind.Draw
// case) and paints into via a 2D context (ui.ts's `paintDrawCommands`,
// `canvas.getContext("2d")`).
export const ENCODER_CANVAS_SELECTORS = [0, 4, 8, 12].map(
  (ix) => `[data-synth-node-id="froggers.encoder.${ix}"] canvas`,
);
// RuntimePages.hpp:57 `NodeIds::kAudioStatusLine` -- the Audio page's own
// requested/active input line (`ComposeBrowserAudioStatusLine`,
// BrowserAudioDevices.hpp), the DOM's only rendering of the native
// `BrowserAudioInputStatusText` for the current `BrowserAudioInputStatus`.
// Only present once the Audio page has been opened (SIDEBAR_BUTTON_SELECTORS[0])
// and only non-empty once the app has actually requested input channels and
// has a current diagnostic to show (RuntimePages.hpp's own `if
// (!snapshot.statusLineText.empty())` guard around this node).
export const AUDIO_STATUS_LINE_SELECTOR = '[data-synth-node-id="runtime.audio.status_line"]';
// RuntimePages.hpp:52 `NodeIds::kAudioInput` -- the Audio page's Input device
// combo box. Only present once the app has requested at least one input
// channel (`AudioPageSnapshot.showInputCombo`, BrowserAudioDevices.hpp's
// `BuildBrowserAudioSnapshot`) and the Audio page has been opened
// (SIDEBAR_BUTTON_SELECTORS[0]). ui.ts's ComboBox case nests a plain
// `<select>` inside the node's own element and keeps its `<option>` list
// synced to the native NodeTree's ComboBox options (ui.ts's `updateNode`), so
// this reaches the `<select>` the operator actually chooses a device from,
// not the wrapper element the node id alone would select.
export const AUDIO_INPUT_SELECT_SELECTOR = '[data-synth-node-id="runtime.audio.input"] select';
// The `Allow Microphone` button (RuntimePages.hpp's kAudioInputPermission).
// Rendered only when the page enumerates input devices it cannot name, which
// is what a page holding no capture permission sees.
// A Button node's own element IS the <button> (ui.ts renders NodeKind.Button
// as one and stamps the id on it), so this addresses the control directly.
export const AUDIO_INPUT_PERMISSION_SELECTOR =
  '[data-synth-node-id="runtime.audio.input.permission"]';
// The boot-error panel's own detail paragraph, painted by either
// site-boot.mjs's `fail()` or index.html's inline `renderBootError()`
// (both use the same `.boot-error-detail` class -- see those files' own
// header comments). Its presence/absence is the shared "did the app show
// a boot failure" predicate for both the first-visit race regression and
// its negative control.
export const BOOT_ERROR_DETAIL_SELECTOR = ".boot-error-detail";

/**
 * Waits for the app to have rendered at least one real UI frame (proof the
 * catalog loaded, the package materialized, and the wasm module started --
 * NOT proof audio started, which never happens in this suite: nothing here
 * ever clicks Play).
 */
export async function waitForSurfaceReady(page) {
  await page.locator(ENCODER_ROW_SELECTORS[0]).waitFor({ state: "attached", timeout: 45_000 });
}

/**
 * The union bounding box of the four encoder-row elements, i.e. the
 * 16-slot grid's own rendered extent.
 */
export async function encoderGridBoundingBox(page) {
  const boxes = await Promise.all(ENCODER_ROW_SELECTORS.map((selector) => page.locator(selector).boundingBox()));
  const present = boxes.filter((box) => box !== null);
  if (present.length === 0) return null;
  const left = Math.min(...present.map((box) => box.x));
  const top = Math.min(...present.map((box) => box.y));
  const right = Math.max(...present.map((box) => box.x + box.width));
  const bottom = Math.max(...present.map((box) => box.y + box.height));
  return { x: left, y: top, width: right - left, height: bottom - top };
}

/**
 * A block's own wire-set (design-space) width, read live from the DOM the
 * same way mobile-stack.mjs itself derives it (mobile-stack.mjs's own
 * `wireExtent`) -- never a hardcoded design width number.
 */
export async function wireWidth(page, selector) {
  return page.locator(selector).evaluate((el) => parseFloat(el.style.width));
}

/**
 * The ONE shared scale mobile-stack.mjs applies to every stacked block
 * when narrow (that file's own "ONE shared scale for the WHOLE stack"
 * comment): viewport width over the grid block's own live wire width.
 */
export async function gridSharedScale(page) {
  const viewport = page.viewportSize();
  const gridWire = await wireWidth(page, RIGHT_BLOCK_SELECTOR);
  return viewport.width / gridWire;
}

/**
 * A stacked block's expected on-screen width at the grid's shared scale
 * (gridSharedScale(page) * that block's own live wire width) -- the
 * formula both the chrome and sidebar width assertions need, since
 * mobile-stack.mjs applies the same shared scale to every stacked block.
 */
export async function expectedStackedWidth(page, selector) {
  const [scale, wire] = await Promise.all([gridSharedScale(page), wireWidth(page, selector)]);
  return scale * wire;
}

/**
 * Vertical overlap in px between two bounding boxes (0 when one renders
 * entirely above/below the other) -- the "stacked, not beside" predicate
 * the mobile-stacking and desktop-layout suites both need. Direction-
 * agnostic (only reports overlap, not which box is on top); callers that
 * know the stacking order from construction (as every current caller
 * does) get "A is above B" for free when this returns 0.
 */
export function verticalOverlapPx(boxA, boxB) {
  return Math.max(0, Math.min(boxA.y + boxA.height, boxB.y + boxB.height) - Math.max(boxA.y, boxB.y));
}

/**
 * Waits for the app's own post-click audio activation to finish
 * (main.ts's `startUserActivation`, dispatched after every UI action --
 * main.ts's `startUserActivation` call site -- which renders
 * `audio:${started ? "online" : ...}` into
 * the root's `data-synth-status` (main.ts's `setStatus`), the same attribute the
 * desktop-layout suite's own "no audio starts on load" test reads).
 */
export async function waitForAudioOnline(page) {
  await expect
    .poll(async () => page.locator(SYNTH_ROOT_SELECTOR).getAttribute("data-synth-status"), { timeout: 20_000 })
    .toContain("audio:online");
}

/**
 * True if the given <canvas> element's OWN backing store holds at least
 * one non-transparent pixel -- read directly via getImageData, not from a
 * screenshot, so this answers "did paint() ever draw into this canvas"
 * (ui.ts's `paintDrawCommands`) independent of whether the canvas is currently
 * on-screen. Deliberately scans every pixel rather than a fixed sample
 * point: encoders draw a filled ring roughly centered in their own
 * bounds (EncoderDraw.hpp's `BuildEncoderDrawCommands`), but nothing here
 * should assume a specific coordinate is inside it.
 */
export async function canvasHasPaintedPixels(page, canvasSelector) {
  return page.evaluate((selector) => {
    const canvas = document.querySelector(selector);
    if (!(canvas instanceof HTMLCanvasElement) || canvas.width === 0 || canvas.height === 0) return false;
    const context = canvas.getContext("2d");
    if (!context) return false;
    const { data } = context.getImageData(0, 0, canvas.width, canvas.height);
    for (let index = 3; index < data.length; index += 4) {
      if (data[index] !== 0) return true; // alpha channel of any pixel
    }
    return false;
  }, canvasSelector);
}

/**
 * Asserts ONE encoder canvas is both genuinely on-screen and actually
 * painted -- two independent, both-required signals, not one:
 *  - toBeInViewport() polls a real IntersectionObserver, which resolves
 *    intersection against every ancestor's own clipping (unlike
 *    boundingBox()/getBoundingClientRect(), which reports an element's
 *    own box regardless of whether an ancestor clips it away -- see
 *    mobile-stack.mjs's own header comment for the concrete bug this
 *    distinction guards against: a collapsed, overflow:hidden ancestor
 *    that clips an otherwise-correctly-sized, correctly-painted child
 *    completely out of view).
 *  - canvasHasPaintedPixels reads the canvas's own backing store, which
 *    an ancestor's clipping does not touch at all -- a canvas that was
 *    simply never painted into would still report itself "in viewport"
 *    (its own box is genuinely on-screen), so this half is the ONLY
 *    signal that would catch that separate failure mode.
 */
export async function expectEncoderCanvasVisible(page, canvasSelector) {
  // Scrolled to first, because the stacked narrow layout is taller than a
  // phone viewport: the encoder grid sits below the chrome block, so the
  // lower encoder rows are legitimately off the initial fold. "Reachable
  // and painted" is the guarantee here; "on screen without scrolling" is
  // asserted at the wider viewports, by the desktop project.
  // This does NOT blunt the clipping guard above: a collapsed,
  // overflow:hidden ancestor clips its child away at every scroll offset,
  // so scrolling cannot bring such a child into the viewport and the
  // assertion below still fails.
  // The scroll is INSIDE the retry: once audio is running, renderFrame
  // re-applies the stacked transforms every frame, which can move a
  // just-scrolled-to row back out from under the viewport. Scrolling once
  // and then waiting only works while nothing re-lays-out underneath.
  // Still toBeInViewport (IntersectionObserver), never a hand-rolled
  // getBoundingClientRect check -- see this helper's own note above on why
  // that distinction is the whole point.
  await expect(async () => {
    await page.locator(canvasSelector).scrollIntoViewIfNeeded();
    await expect(page.locator(canvasSelector), canvasSelector).toBeInViewport({ timeout: 2_000 });
  }, canvasSelector).toPass({ timeout: 20_000 });
  await expect.poll(() => canvasHasPaintedPixels(page, canvasSelector), { timeout: 10_000 }).toBe(true);
}
