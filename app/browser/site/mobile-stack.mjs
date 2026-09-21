// Shell-side per-block transforms for the mobile stacked layout
// (froggers-web-host spec.md "Mobile viewport stacks around a full-width
// encoder grid": chrome block above, encoder-grid block below, spanning the
// full viewport width, with everything else placed above or below, never
// beside). Sheaf's own generic sidebar is placed rather than stacked: it
// goes inside the chrome block's footprint, under the Randomize/Reset
// column, where there is room for it beside the sliders. The grid is what
// spans the full viewport width; every other block shares ITS scale (see
// applyMobileStack's own comment) rather than being independently
// stretched to full width, so how wide a block renders is decided by the
// surface's own narrow topology, not here. The chrome block declares equal
// weight with the grid when narrow and so renders the same width
// (FroggersCellMap::kLeftBlockWeightNarrow); Sheaf's sidebar has no such
// variant and renders at its own smaller, design-proportional size,
// left-aligned under the grid. The
// input-mapping this mechanism relies on
// needed drag/press coordinate math in
// Sheaf's own ui.ts to be made transform-robust first. This file hooks
// `BrowserUiBackend`'s `renderFrame` method rather than fighting CSS at
// the stylesheet level, which cannot reflow the surface's own internal
// layout (see index.html's own header comment): ui.ts's `updateNode()`
// unconditionally clears EVERY protocol node's `transform` on every render
// frame (`element.style.transform = "";`), and only
// `fitSurface()`'s own later, root-only re-application survives that
// clear. Installing over `renderFrame` also reaches past that method's own
// public surface: the same patched function reads `this.dispatchBrowserAction`
// (see this file's own `dispatchViewportNarrow`) to report narrow/wide
// state to the C++ surface, even though `ui.ts` declares that constructor
// property `private readonly` -- TypeScript erases that at runtime, so the
// call works, and nothing else reaches this backend instance to dispatch
// through instead. A transform this shell applies directly to the
// wire-managed block elements would be wiped within one frame (~33ms)
// unless reasserted on that exact cadence -- so this module wraps the
// public `renderFrame` method and reruns its own application synchronously,
// in the same JS turn, right after Sheaf's own per-frame writes. It only
// ever sets host-owned CSS (`transform`/`transformOrigin` on the stacked
// block elements, `min-height`/`overflow` on the mount) and never touches a
// protocol node's wire-managed position/size (`left`/`top`/`width`/
// `height`) -- the "per-block CONTAINER transform" mechanism: it composes
// with the wire-managed properties instead of fighting them, which is what
// makes it survive where a plain-CSS approach could not.

// Legacy site's own mobile breakpoint
// (`window.matchMedia("(max-width: 720px)")`), carried forward as this
// shell's own threshold.
const NARROW_MAX_WIDTH = 720;
// Shell's own choice, not a wire value: vertical gap between each stacked
// block.
const STACK_GAP_PX = 12;

const MOUNT_SELECTOR = "#synth-root"; // index.html:60, this shell's own id
// FroggersNodeIds::kLeftBlock / kRightBlock -- the outer split Row's two
// Weight(2)/Weight(4) siblings (FroggersUiSurface.hpp:113-114,464-467): the
// chrome block (scope/transport/scenes/blend/bpm) and the block holding the
// page chrome + 16-slot encoder grid.
const CHROME_BLOCK_SELECTOR = '[data-synth-node-id="froggers.layout.left"]';
const GRID_BLOCK_SELECTOR = '[data-synth-node-id="froggers.layout.right"]';
// RuntimePages.hpp:34 `NodeIds::kSidebarRoot = "runtime.sidebar.root"` --
// Sheaf's OWN generic runtime-chrome sidebar (the "Audio/Controllers/Sync/
// File" toolbar + CPU meter), a SIBLING of `froggers.root` under the
// composite `runtime.main.root` fitSurface actually scales
// (RuntimeMainComponent.hpp:197-214,212: `root.bounds = {..., appRootWidth
// + Layout::kSidebarWidth, appRootHeight}`, `root.children = {contentTree
// ...front().id, sidebarTree...front().id}`). It is "everything else" for
// the "chrome above, grid full-width, everything else above or
// below" rule. It is not a Froggers node and never can be, so the surface
// cannot place it -- placing it is this shell's job, and where it goes is
// PLACED_BELOW_SELECTOR's own comment below.
const SIDEBAR_SELECTOR = '[data-synth-node-id="runtime.sidebar.root"]';
// FroggersNodeIds::kLeftButtons -- the narrow chrome block's Randomize/Reset
// column. `AppendNarrowButtonColumn` declares it `Extent::Intrinsic()` on
// both axes specifically so this box ends where its last button ends, which
// makes it the surface's own statement of where the free space beside the
// sliders begins. Read live, never as an offset copied into this file.
const PLACED_BELOW_SELECTOR = '[data-synth-node-id="froggers.layout.left.buttons"]';

// The two blocks that stack, top to bottom, in this order. The sidebar is
// NOT one of them: it is measured with them (so a frame is still all-or-
// nothing across every block this file touches) but positioned relative to
// a box inside the first, not appended under the last.
const STACK_SELECTORS = [CHROME_BLOCK_SELECTOR, GRID_BLOCK_SELECTOR];
// Every block this file transforms, stacked or placed -- what a frame has to
// measure successfully before any of it is committed.
const MANAGED_SELECTORS = [...STACK_SELECTORS, SIDEBAR_SELECTOR];

function isNarrow(mount) {
  return mount.clientWidth > 0 && mount.clientWidth <= NARROW_MAX_WIDTH;
}

// FroggersActions::kViewportNarrow (FroggersUiSurface.hpp,
// `"froggers.viewport.narrow"`) -- the C++ surface's own narrow/wide
// signal, consumed by `HandleAction` to set `narrowViewport_` off
// `action.value == "1"`. "0" is dispatched for every other value,
// including wide.
const VIEWPORT_NARROW_ACTION = "froggers.viewport.narrow";
// Last value actually dispatched for VIEWPORT_NARROW_ACTION, so a
// narrow/wide state that hasn't changed since the previous frame is never
// redispatched -- renderFrame's own ~33ms cadence would otherwise flood
// the wasm app with a same-value action on every single frame while
// sitting still at one width. `null` (neither "0" nor "1") so the very
// first frame always dispatches once, regardless of which side it starts
// on.
let lastDispatchedNarrow = null;

// Reports the mount's narrow/wide state to the C++ surface off the SAME
// isNarrow() decision this file already uses for its own stacking
// transform -- never a second threshold or predicate. Called from inside
// the patched `renderFrame` (installMobileStack), where `this` is the
// `BrowserUiBackend` instance whose `dispatchBrowserAction` this reaches
// past the public API to call (see this file's own header comment).
function dispatchViewportNarrow(backend, mount) {
  const narrow = isNarrow(mount);
  const value = narrow ? "1" : "0";
  if (value === lastDispatchedNarrow) return;
  lastDispatchedNarrow = value;
  backend.dispatchBrowserAction({ name: VIEWPORT_NARROW_ACTION, value });
}

// Reads a block's WIRE-SET design-space extent -- the exact px string
// ui.ts's updateNode() writes from the node's resolved protocol bounds
// (ui.ts's `updateNode`: `element.style.width/height = node.bounds.width/height +
// "px"`). Read live every call; never a hardcoded copy of the interior
// layout numbers (the 900x712 design box, the two Froggers block ids, and
// the sidebar's own id are the only structural facts this file bakes in,
// all cited above).
function wireExtent(element) {
  const width = parseFloat(element.style.width);
  const height = parseFloat(element.style.height);
  return width > 0 && height > 0 ? { width, height } : null;
}

// Measures ONE block's current (ancestor-scaled, own-transform-cleared)
// rect and wire extent, WITHOUT deciding anything about its final
// transform yet. Clearing `transform` here IS a DOM write (a block's own
// surface -- e.g. Sheaf's sidebar, rendered by its own, separate
// SidebarSurface -- can transiently report a degenerate extent mid-resize,
// so this write can turn out to have been for nothing); applyMobileStack
// is responsible for making the OVERALL operation atomic across every
// block it manages despite that -- see its own comment for how and why.
//
// IMPORTANT: this does NOT assume any particular ancestor is the element
// fitSurface() scales, and does not try to neutralize any ancestor's
// transform. Sheaf's generic runtime shell wraps the app's own tree in a
// further composite root (`runtime.main.root`, see SIDEBAR_SELECTOR's own
// comment above) that fitSurface actually scales -- confirmed empirically.
// Rather than
// special-case that ancestor (fragile if Sheaf's composite structure ever
// changes further), every measurement here is taken with the block's OWN
// transform cleared but every ancestor's transform left exactly as Sheaf
// set it, and the needed scale/translate (computed later, once every
// block has measured) is derived from the RATIO between the current
// (ancestor-scaled) rendered rect and the wire (design-space) extent --
// self-correcting for whatever scale any ancestor already contributes, at
// any depth, without needing to know or touch it.
function measureBlock(element) {
  const extent = wireExtent(element);
  if (!extent) return null;
  element.style.transform = "none";
  const rect = element.getBoundingClientRect();
  return rect.width > 0 ? { element, extent, rect } : null;
}

// Computes and applies ONE already-measured block's transform, using a
// SHARED `scale` (the same number for every block in the stack -- see
// applyMobileStack's own comment for why), left edge at `targetLeft`, top
// edge at `targetTop` (all real viewport px). Returns the block's own
// final on-screen height, for stacking the next block under it.
function applyStackedTransform(measurement, scale, targetLeft, targetTop) {
  const { element, extent, rect } = measurement;
  // A translate() inside an ancestor scale gets multiplied by that same
  // ancestor scale when painted (transforms compose down the tree), so a
  // raw "target minus current" delta would land short/long by exactly the
  // ancestor factor. `rect.width / extent.width` IS that ancestor factor:
  // rect.width already equals (ancestor scale x wire width), so dividing
  // it out recovers the ancestor's own contribution, self-correcting for
  // whatever scale any ancestor already applies, at any depth, without
  // needing to know or touch it.
  const ancestorScale = rect.width / extent.width;
  // The block's OWN needed scale, on top of the ancestor's contribution
  // above, so the TOTAL effective on-screen scale (ancestorScale x
  // ownScale) equals the shared `scale` passed in -- not each block's own
  // independent "fill the viewport" scale.
  const ownScale = scale / ancestorScale;
  const scaledHeight = scale * extent.height;

  element.style.transformOrigin = "0 0";
  element.style.transform =
    `translate(${(targetLeft - rect.left) / ancestorScale}px, ${(targetTop - rect.top) / ancestorScale}px) scale(${ownScale})`;
  return scaledHeight;
}

// Last successfully computed transform for each stacked block, keyed by
// its own SELECTOR (not the DOM element -- selectors are stable identity
// across frames even if Sheaf ever recreated a node's element; elements
// are not assumed to be). This is the fallback re-applied when a frame's
// measurement fails for ANY block -- and it has to be a cache like this,
// not a live read of `element.style.transform`: ui.ts's own `updateNode()`
// (part of the ORIGINAL `renderFrame()` this module wraps, which always
// runs BEFORE this module's own code, every frame) unconditionally clears
// EVERY node's `transform` to `""` before this module ever sees it -- so
// reading `element.style.transform` at the top of applyMobileStack always
// finds that wiped value, never the previous frame's real stacked
// transform. (An earlier version of this fix tried exactly that "read the
// live style, restore it on failure" approach; it looked correct but
// verifiably was not -- a forced-failure e2e test caught it reverting
// chrome/grid to something close to their NATIVE, un-stacked size even
// though the restore code ran, because what it captured and restored was
// already the cleared value, not the real one.)
// The mount's own stacked extent needs no equivalent cache. It is
// reserved with `min-height`, which nothing else on the page writes
// (ui.ts's `fitSurface` writes the mount's `height`, and `min-height`
// floors the used height above it), so a frame that measures badly and
// returns early simply leaves the previous frame's reservation standing.
const lastGoodTransform = new Map();

/**
 * Applies (while narrow) or clears (while wide) the per-block stack
 * transform. Idempotent and safe to call every render frame and on every
 * resize. At wide viewports this is a true no-op: it touches nothing, so
 * Sheaf's own unmodified fitSurface/updateNode output is exactly what
 * reaches the screen (matches "wide viewports: hands off").
 *
 * Applies ATOMICALLY across every block it manages -- the two stacked
 * blocks and the placed sidebar alike: measures all of them first
 * (measureBlock), and only commits a NEW transform to any of them if EVERY
 * one measured successfully. If even one is transiently
 * unmeasurable (e.g. Sheaf's sidebar surface mid-rebuild reporting a
 * momentary zero-extent bounds during a resize -- observed in practice,
 * and forced deterministically in the e2e regression test for this),
 * every element that measureBlock already cleared to `transform: none`
 * earlier in this same pass is put back to its own last KNOWN-GOOD
 * stacked transform (lastGoodTransform, see its own comment), the
 * mount's own reservation is left as the previous frame set it, and this
 * frame is otherwise a no-op -- the
 * PREVIOUS frame's fully-correct stacked state is what's left standing,
 * not one block reverted to its native position while the other two stay
 * stacked. Everything in this function runs synchronously (no `await`/rAF
 * inside it), so the browser cannot paint an intermediate state between
 * "some elements cleared" and "all restored" (or, on success, "all
 * re-stacked") -- only the state present when this function RETURNS is
 * ever visible. The next render frame (~33ms later) retries from scratch
 * either way.
 */
export function applyMobileStack() {
  const mount = document.querySelector(MOUNT_SELECTOR);
  if (!mount) return;

  if (!isNarrow(mount)) {
    // Wide viewport: clear any of OUR OWN prior overrides (a no-op unless
    // the viewport just crossed narrow->wide) and touch nothing else --
    // Sheaf's own renderFrame call this runs after already left the
    // correct wide-mode state on every node it owns.
    //
    // Deliberately does NOT clear lastGoodTransform here.
    // `mount.clientWidth` can transiently read a stale "still wide"
    // value for a frame or two while an active resize is settling (layout
    // catching up with a just-issued viewport change), so `isNarrow()`
    // reaching `false` for one frame mid-transition does not reliably mean
    // "the user is actually at a wide viewport now" -- an e2e regression
    // test caught this: clearing the caches on that stale read left a
    // SUBSEQUENT transient measurement failure (see applyStackedTransform's
    // own atomicity comment) with nothing to restore from, reverting one
    // block to its native size for a frame even though the OTHER two
    // blocks (whose own measurements didn't fail that frame) looked fine.
    // Stale cache entries are harmless while genuinely wide (nothing here
    // reads them at all in that state); the only cost of leaving them is
    // that the FIRST transient failure after a later re-entry into narrow
    // mode could fall back to a slightly-stale (previous viewport width's)
    // transform for one frame instead of "no transform" -- strictly better,
    // not worse, and self-corrects on the very next successful frame.
    // `min-height` and `overflow` are this shell's own properties and are
    // cleared here; the mount's `height` is Sheaf's, written by fitSurface
    // (browser/src/ui.ts, `root.style.height = surfaceHeight *
    // surfaceScale`) and never touched by this module at any width. The
    // mount's only child is absolutely positioned, so a mount whose height
    // this shell blanked would compute to zero and clip the whole surface
    // away.
    mount.style.minHeight = "";
    mount.style.overflow = "";
    return;
  }

  const elements = MANAGED_SELECTORS.map((selector) => document.querySelector(selector));
  if (elements.some((element) => !element)) return; // not all rendered yet

  const measurements = elements.map(measureBlock);
  if (measurements.some((measurement) => measurement === null)) {
    // Restore every block to its own cached last-good transform (leaving
    // it cleared if this stack has never successfully applied yet --
    // nothing better to fall back to). The mount's reservation is left
    // exactly as the previous frame set it -- see lastGoodTransform's own
    // comment for why the mount needs no cache of its own.
    MANAGED_SELECTORS.forEach((selector, index) => {
      const cached = lastGoodTransform.get(selector);
      if (!cached) return;
      elements[index].style.transformOrigin = cached.transformOrigin;
      elements[index].style.transform = cached.transform;
    });
    return;
  }

  const viewportWidth = mount.clientWidth;
  const mountRect = mount.getBoundingClientRect();

  // ONE shared scale for the WHOLE stack: each block used to be stretched
  // to its OWN full viewport width, which made a block render at whatever
  // scale its own design width happened to need -- ballooning a narrow
  // block's height and pushing the grid far down the page. Deriving ONE
  // scale from the grid block's own live wire width and applying it to
  // every stacked block keeps the grid spanning the full viewport width
  // (unchanged requirement) and makes every other block's rendered width a
  // direct read-out of its own declared design width. That is what lets
  // the surface decide the narrow layout: the chrome block declares equal
  // weight with the grid when narrow (FroggersCellMap, the
  // kViewportNarrow action this file dispatches) and therefore renders the
  // same width, while Sheaf's own sidebar, which has no narrow variant,
  // renders at its own smaller, design-proportional size.
  const gridIndex = MANAGED_SELECTORS.indexOf(GRID_BLOCK_SELECTOR);
  const sharedScale = viewportWidth / measurements[gridIndex].extent.width;

  let top = mountRect.top;
  let totalHeight = 0;
  STACK_SELECTORS.forEach((selector, index) => {
    // Left-aligned (not centered): every block starts at the same left
    // edge as the mount/grid, including chrome even though it renders at
    // its own declared width -- simpler, predictable, and keeps everything
    // flush against one shared edge rather than each block needing its own
    // centering offset.
    const measurement = measurements[index];
    const scaledHeight = applyStackedTransform(measurement, sharedScale, mountRect.left, top);
    lastGoodTransform.set(selector, {
      transform: measurement.element.style.transform,
      transformOrigin: measurement.element.style.transformOrigin,
    });
    top += scaledHeight + STACK_GAP_PX;
    totalHeight += scaledHeight + STACK_GAP_PX;
  });
  totalHeight -= STACK_GAP_PX; // no trailing gap after the last stacked block

  // Sheaf's sidebar goes in the free space beside the sliders, under the
  // chrome block's Randomize/Reset column, instead of under the grid where
  // it was a whole page-scroll away from everything else.
  //
  // READ ORDER IS THE WHOLE TRICK HERE. That column is a DESCENDANT of the
  // chrome block, so its rendered rect only means anything once the chrome
  // block's own transform is applied -- which the loop above has just
  // done. Reading it during the measurement pass would give the
  // untransformed position instead, because measureBlock deliberately
  // clears each block's transform before measuring. Still one synchronous
  // pass, so the all-or-nothing guarantee above is untouched.
  const sidebar = measurements[MANAGED_SELECTORS.indexOf(SIDEBAR_SELECTOR)];
  const placedBelow = document.querySelector(PLACED_BELOW_SELECTOR);
  const sidebarHeight = sharedScale * sidebar.extent.height;
  // Fits only if it lands inside the chrome block. The mount clips
  // (`overflow: hidden` below), so a sidebar hanging past the block's
  // bottom edge would be CUT OFF rather than visibly overflowing, which is
  // the kind of failure nobody notices until a control is missing. Both
  // heights are live: Sheaf sizes the sidebar at 40px per row and grows it
  // by a row if the app ever registers a page of its own
  // (RuntimePages.hpp's SidebarRootBounds), so this is checked rather than
  // settled once by arithmetic.
  const chromeBottom = mountRect.top + sharedScale * measurements[0].extent.height;
  const anchor = placedBelow === null ? null : placedBelow.getBoundingClientRect();
  const placedTop = anchor === null ? 0 : anchor.bottom + STACK_GAP_PX;
  if (anchor !== null && placedTop + sidebarHeight <= chromeBottom) {
    applyStackedTransform(sidebar, sharedScale, anchor.left, placedTop);
  } else {
    // Either the narrow column is not in the tree yet -- the wide topology
    // rendering at a narrow mount, for the one frame after a resize before
    // the surface emits its narrow tree -- or it is there but the sidebar
    // no longer fits beside the sliders. Stack it under the grid, where
    // this shell always used to put it: a longer page, but nothing clipped
    // away.
    applyStackedTransform(sidebar, sharedScale, mountRect.left, top);
    totalHeight += STACK_GAP_PX + sidebarHeight;
  }
  lastGoodTransform.set(SIDEBAR_SELECTOR, {
    transform: sidebar.element.style.transform,
    transformOrigin: sidebar.element.style.transformOrigin,
  });

  // fitSurface's own mount height (sized for its own, un-stacked scale) no
  // longer applies; reserve exactly the stacked content's own extent so the
  // shell's own footer chrome sits right below it, and clip whatever of
  // Sheaf's own composite tree (e.g. its own outer margin) would otherwise
  // peek out under `overflow: visible`. That extent is chrome plus grid
  // while the sidebar sits inside the chrome block, and gains the sidebar
  // on the frames where it falls back to being stacked under the grid.
  //
  // Reserved as `min-height`, NOT as `height`, and that choice is what
  // makes the page scrollable. fitSurface writes the mount's `height` at
  // the end of every renderFrame -- 279px at a 390px viewport, against a
  // ~1242px stack -- so a shell that reserved the stack on `height` too
  // would leave that short value standing for the rest of the frame,
  // until this function overwrote it. Anything that reads layout in that
  // window (this function's own `mount.clientWidth` does, on its very
  // first line) forces the browser to lay the page out while the document
  // fits the viewport, and a document that fits the viewport has a
  // maximum scroll offset of zero, so any scroll offset is clamped to the
  // top. Putting the tall value back afterwards does not restore the
  // clamped offset, and no scroll API is involved, so nothing watching
  // `scrollTo`/`scrollTop` can see it happen. `min-height` floors the used
  // height above whatever fitSurface writes, so the document is never
  // momentarily short on any path -- including Sheaf's own
  // ResizeObserver, which calls fitSurface a whole frame before this
  // module's rAF-deferred follow-up runs.
  mount.style.minHeight = `${totalHeight}px`;
  mount.style.overflow = "hidden";
}

// rAF-debounced apply, plus a short burst of follow-up ticks -- catches
// both a secondary layout shift the resize itself can trigger (e.g. a
// scrollbar appearing/disappearing changing `mount.clientWidth` again
// after the first pass already ran) AND ui.ts's OWN internal
// `ResizeObserver` on this same element (see installMobileStack's own
// comment): that observer calls `fitSurface()` directly, independent of
// `renderFrame()`, so it is NOT something this module's `renderFrame`
// patch alone can order against -- it can fire in the same
// resize-observation batch as this callback, in EITHER order (browsers do
// not guarantee inter-observer callback order), so a single follow-up
// tick is not reliably enough margin. `renderFrame()`'s own ~33ms cadence
// self-heals any single miss on its own, but that isn't sufficient during
// this active window (a resize burst can legitimately outlast one frame,
// and this shell would rather actively re-assert than rely on the app's
// own frame cadence alone). FOLLOW_UP_TICKS spans several rAF ticks
// (~16ms each, well over two full ui.ts render-frame intervals) so any
// interleaving with ui.ts's own resize-triggered fitSurface() call is
// re-corrected well within the same user-perceived resize gesture.
const FOLLOW_UP_TICKS = 8;
let applyScheduled = false;
function scheduleApply() {
  if (applyScheduled) return;
  applyScheduled = true;
  let remaining = FOLLOW_UP_TICKS;
  const tick = () => {
    applyMobileStack();
    remaining -= 1;
    if (remaining > 0) requestAnimationFrame(tick);
    else applyScheduled = false;
  };
  requestAnimationFrame(tick);
}

/**
 * Wires applyMobileStack() to run after every render frame -- fighting
 * ui.ts's own per-frame style resets, see this file's header comment --
 * and on resize, so a narrow<->wide crossing doesn't have to wait for the
 * next wasm-driven frame tick. Call once, before booting the app. Patches
 * the SHARED prototype method (not a specific instance): site-boot.mjs
 * never gets a direct handle to the `BrowserUiBackend`
 * `installSynthBrowserApp` constructs internally, but every instance looks
 * up `renderFrame` through the same prototype at call time, so this covers
 * it regardless.
 *
 * Resize detection uses a `ResizeObserver` on the SAME host element Sheaf
 * itself observes for `fitSurface` (ui.ts's constructor, `this.resizeObserver =
 * new ResizeObserver(() => this.fitSurface()); this.resizeObserver.
 * observe(root);` where `root` is the mount passed into
 * `BrowserUiBackend`'s constructor -- `#synth-root`, same as
 * MOUNT_SELECTOR here) rather than `window`'s `resize` event alone: a
 * `window.resize` doesn't fire for every layout change that resizes the
 * mount (e.g. this shell's own responsive chrome reflowing without the
 * outer window changing size at all), so observing the exact element
 * Sheaf's own fitSurface reacts to is what makes this react to every
 * resize fitSurface itself reacts to, not just window-level ones. It does
 * NOT, on its own, guarantee running strictly after that same resize's
 * `fitSurface()` call -- two independent `ResizeObserver` instances on one
 * element have no browser-guaranteed callback ordering -- which is exactly
 * why `scheduleApply`'s multi-tick follow-up burst exists (see its own
 * comment): order-safety here comes from repeatedly re-asserting shortly
 * after the resize, not from winning a single race.
 */
export function installMobileStack(BrowserUiBackend) {
  const originalRenderFrame = BrowserUiBackend.prototype.renderFrame;
  BrowserUiBackend.prototype.renderFrame = function patchedRenderFrame(...args) {
    const result = originalRenderFrame.apply(this, args);
    applyMobileStack();
    // Reports narrow/wide state to the C++ surface every frame, but only
    // actually dispatches on a change (dispatchViewportNarrow's own
    // comment) -- `this` is the `BrowserUiBackend` instance `renderFrame`
    // was called on, which is what makes `this.dispatchBrowserAction`
    // reachable here despite it not being public API.
    const mount = document.querySelector(MOUNT_SELECTOR);
    if (mount) dispatchViewportNarrow(this, mount);
    return result;
  };

  const mount = document.querySelector(MOUNT_SELECTOR);
  if (mount) {
    const resizeObserver = new ResizeObserver(scheduleApply);
    resizeObserver.observe(mount);
  }
  // Kept as a belt-and-suspenders trigger (orientation change on some
  // engines resizes the layout viewport without necessarily resizing the
  // mount element on the very same tick the OS event fires) -- cheap,
  // idempotent, and scheduleApply() itself is debounced.
  window.addEventListener("orientationchange", scheduleApply);
}
