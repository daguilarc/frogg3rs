// Mobile-emulated stacking assertion: mobile viewport stacks around a
// full-width encoder grid.
//
// Host-page CSS alone cannot restack this layout, because
// FroggersUiSurface.hpp lays the chrome and grid blocks out as one outer
// Row (`AppendLeftBlock`/`AppendRightBlock`, Weight(2)/Weight(4) siblings) that
// Sheaf's browser UI backend positions as absolutely-bounded, wire-managed
// DOM nodes an active render loop keeps rewriting.
// Per-block
// CSS transforms sidestep that instead of fighting it: they compose with (rather than override) those wire-managed
// properties, reasserted every render frame from the site shell
// (app/browser/site/mobile-stack.mjs, hooked in site-boot.mjs) -- see that
// file's own header comment for the full mechanism. TWO blocks stack --
// chrome above, grid full-width below it -- and Sheaf's own generic runtime
// sidebar (SIDEBAR_SELECTOR, helpers.mjs) is PLACED rather than stacked:
// inside the chrome block, under its Randomize/Reset column, beside the
// sliders. It is still never left beside the grid.
//
// SHARED SCALE: only the GRID is stretched to fill the viewport width.
// Every other stacked block renders at that SAME scale (mobile-stack.mjs's
// own comment has the full reasoning: independently stretching each block
// to full width made a narrow block balloon in height and pushed the grid
// far down the page). How wide a block ends up is therefore decided by its
// own declared design width, in the surface. The chrome block declares
// equal weight with the grid when narrow and so comes out the same width;
// Sheaf's own generic sidebar has no narrow variant of its own -- this
// surface cannot declare a weight for a tree Sheaf emits -- and so comes out
// narrower, sitting in the chrome block's own free space.
import { expect, test } from "@playwright/test";
import {
  BPM_SELECTOR,
  ENCODER_ROW_SELECTORS,
  LEFT_BLOCK_SELECTOR,
  NARROW_BUTTON_COLUMN_SELECTOR,
  RANDOMIZE_RESET_SELECTORS,
  RIGHT_BLOCK_SELECTOR,
  SIDEBAR_BUTTON_SELECTORS,
  SIDEBAR_SELECTOR,
  encoderGridBoundingBox,
  expectedStackedWidth,
  verticalOverlapPx,
  waitForSurfaceReady,
} from "./helpers.mjs";

test.describe("mobile stacking (phone-width layout)", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/");
    await waitForSurfaceReady(page);
  });

  test("the encoder grid spans the full viewport width", async ({ page }) => {
    const grid = await encoderGridBoundingBox(page);
    const viewport = page.viewportSize();
    expect(grid.width).toBeGreaterThanOrEqual(viewport.width * 0.9);
  });

  test("no other control renders beside the grid", async ({ page }) => {
    const rightBlockBox = await page.locator(RIGHT_BLOCK_SELECTOR).boundingBox();
    const leftBlockBox = await page.locator(LEFT_BLOCK_SELECTOR).boundingBox();
    // "Beside" means the two boxes share a vertical range (some y overlap)
    // while occupying disjoint horizontal ranges. Stacked above/below means
    // zero vertical overlap.
    expect(verticalOverlapPx(rightBlockBox, leftBlockBox)).toBe(0);
  });

  test("chrome renders at the grid's shared scale, and at the grid's own width", async ({ page }) => {
    // This test used to assert the opposite of its second half: that
    // chrome came out "well short of the grid's own full width". That was
    // a deliberate guard on the shell not stretching each block
    // independently, and it stayed correct for as long as the surface
    // declared the chrome block at half the grid's weight. The surface now
    // declares them EQUAL when narrow
    // (FroggersCellMap::kLeftBlockWeightNarrow), because the half-width
    // chrome block left the other half of a phone viewport empty with
    // nothing able to reach it. The shared-scale half below is unchanged
    // and still the thing that would catch an independent stretch: a block
    // stretched on its own would not land on the grid's scale times its
    // own wire width except by coincidence.
    const gridBox = await page.locator(RIGHT_BLOCK_SELECTOR).boundingBox();
    const chromeBox = await page.locator(LEFT_BLOCK_SELECTOR).boundingBox();

    expect(Math.abs(chromeBox.width - (await expectedStackedWidth(page, LEFT_BLOCK_SELECTOR)))).toBeLessThan(1.5); // within ~1.5px
    // Same width as the grid, within the delta's own 5%, so neither block
    // leaves a usable strip of the viewport empty beside it.
    expect(Math.abs(chromeBox.width - gridBox.width)).toBeLessThan(gridBox.width * 0.05);

    // Sits fully above the grid (zero vertical overlap, same "stacked not
    // beside" check the other tests here use) and does not spill past the
    // grid's own right edge (no horizontal overlap past its container).
    expect(verticalOverlapPx(chromeBox, gridBox)).toBe(0);
    expect(chromeBox.x + chromeBox.width).toBeLessThanOrEqual(gridBox.x + gridBox.width + 1);
  });

  test("the four Randomize/Reset buttons sit beside the sliders, inside the chrome block", async ({ page }) => {
    const chromeBox = await page.locator(LEFT_BLOCK_SELECTOR).boundingBox();
    const bpmBox = await page.locator(BPM_SELECTOR).boundingBox();
    const bpmCentre = bpmBox.x + bpmBox.width / 2;

    for (const selector of RANDOMIZE_RESET_SELECTORS) {
      const box = await page.locator(selector).boundingBox();
      expect(box, selector).not.toBeNull();
      // To the right of the BPM slider, and vertically within the chrome
      // block -- i.e. beside the sliders, in the strip that used to render
      // as empty viewport.
      expect(box.x + box.width / 2, selector).toBeGreaterThan(bpmCentre);
      const centreY = box.y + box.height / 2;
      expect(centreY, selector).toBeGreaterThanOrEqual(chromeBox.y);
      expect(centreY, selector).toBeLessThanOrEqual(chromeBox.y + chromeBox.height);
      // Inside the chrome block's own box, on both axes.
      expect(box.x, selector).toBeGreaterThanOrEqual(chromeBox.x - 1);
      expect(box.x + box.width, selector).toBeLessThanOrEqual(chromeBox.x + chromeBox.width + 1);
      // Sized to the label rather than to a share of the block width.
      expect(box.width, selector).toBeLessThan(chromeBox.width / 2);
    }
  });

  test("no Randomize or Reset button falls inside the encoder grid block", async ({ page }) => {
    // The layout this rejects: the four buttons hoisted into the encoder
    // COLUMN, above or below the encoder rows. That arrangement was built
    // once and turned down -- it pushes encoder rows past the fold, which
    // trades a control the operator touches occasionally for controls they
    // touch constantly. Every assertion here is about which block the
    // buttons are in, not about how high up the page they are, because
    // "above the encoder rows" is exactly what the rejected layout also
    // satisfied.
    const gridBox = await page.locator(RIGHT_BLOCK_SELECTOR).boundingBox();

    for (const selector of RANDOMIZE_RESET_SELECTORS) {
      const box = await page.locator(selector).boundingBox();
      const overlapsHorizontally = box.x < gridBox.x + gridBox.width && box.x + box.width > gridBox.x;
      const overlapsVertically = verticalOverlapPx(box, gridBox) > 0;
      expect(overlapsHorizontally && overlapsVertically, selector).toBe(false);
    }
  });

  test("each of the four buttons is emitted exactly once", async ({ page }) => {
    // The narrow layout MOVES these buttons; a shell that rearranged
    // emitted controls, or a surface that emitted a second narrow copy,
    // would leave two of each in the tree.
    for (const selector of RANDOMIZE_RESET_SELECTORS) {
      await expect(page.locator(selector), selector).toHaveCount(1);
    }
  });

  test("the first encoder row is reachable without scrolling", async ({ page }) => {
    // The chrome block spans the viewport when narrow, and the shell stacks
    // it above the grid. A chrome block that kept its full-page height
    // while doubling in width would take half again as much vertical space
    // and carry the whole encoder grid off the first screen -- the same
    // cost that got an earlier attempt at this layout turned down. The
    // surface declares a shorter chrome block when narrow
    // (FroggersCellMap::kLeftBlockCrossWeightNarrow) to hold this.
    const viewport = page.viewportSize();
    const row = await page.locator(ENCODER_ROW_SELECTORS[0]).boundingBox();
    expect(row.y).toBeGreaterThan(0);
    expect(row.y + row.height).toBeLessThanOrEqual(viewport.height);
  });

  test("a scroll offset survives the render loop", async ({ page }) => {
    // The published site could not be scrolled at all: an offset held for
    // about two frames and was then pinned back at the top, every frame,
    // so everything below the fold was unreachable. Nothing called a
    // scroll API. Sheaf's fitSurface writes the mount's `height` at the
    // end of every render frame, sized for its own un-stacked scale, and
    // mobile-stack.mjs used to reserve the stacked height on that same
    // property -- so between the two writes the document briefly fitted
    // the viewport, and the first layout read in that window made the
    // browser clamp the scroll offset to zero. The reservation is now a
    // `min-height`, which floors the used height above whatever fitSurface
    // writes, so the document is never momentarily short.
    //
    // Asserted after several animation frames, not immediately: a
    // single-frame check passes against the bug.
    const target = 200;
    await page.evaluate((y) => window.scrollTo(0, y), target);
    const offsets = await page.evaluate(async () => {
      const seen = [];
      for (let frame = 0; frame < 30; frame++) {
        await new Promise(requestAnimationFrame);
        seen.push(Math.round(window.scrollY));
      }
      return seen;
    });
    expect(Math.min(...offsets)).toBe(target);
  });

  test("controls below the fold are reachable by scrolling", async ({ page }) => {
    // The stacked layout is taller than a phone viewport by construction, so
    // the last encoder row starts below the fold. Bringing it into view is
    // the end-to-end version of the scroll assertion above: it fails both if
    // the page cannot scroll and if the mount clips the row away once it is
    // scrolled to.
    // This used to use the sidebar, which was the last stacked block. The
    // sidebar now sits inside the chrome block, above the fold, so it no
    // longer proves anything here -- the last encoder row is what is
    // genuinely below it.
    const viewport = page.viewportSize();
    const target = page.locator(ENCODER_ROW_SELECTORS[3]);
    const before = await target.boundingBox();
    expect(before.y).toBeGreaterThan(viewport.height); // positive control: it really is below the fold
    await target.scrollIntoViewIfNeeded();
    await expect(target).toBeInViewport({ timeout: 5_000 });
  });

  test("the runtime page buttons sit beside the sliders, under Randomize/Reset", async ({ page }) => {
    // This replaces "the sidebar stacks below the grid at the grid's shared
    // scale, not full width", which asserted the placement this test's
    // subject moved away from. Sheaf's sidebar used to be a third stacked
    // block under the encoder grid, which put four runtime page buttons a
    // whole page-scroll from everything else. It is now placed inside the
    // chrome block, under the Randomize/Reset column, where the column's
    // own intrinsic height leaves room for it beside the sliders.
    //
    // It is still Sheaf's block, with no narrow variant of its own -- the
    // frogg3rs surface has no weight to declare for a tree it does not
    // emit -- so the SHELL places it, and the shell derives the position
    // from the surface's own button-column box rather than from an offset
    // of its own. Hence the assertion against that box below.
    const chromeBox = await page.locator(LEFT_BLOCK_SELECTOR).boundingBox();
    const columnBox = await page.locator(NARROW_BUTTON_COLUMN_SELECTOR).boundingBox();
    const bpmBox = await page.locator(BPM_SELECTOR).boundingBox();
    const bpmCentre = bpmBox.x + bpmBox.width / 2;

    for (const selector of SIDEBAR_BUTTON_SELECTORS) {
      const box = await page.locator(selector).boundingBox();
      expect(box, selector).not.toBeNull();
      // Beside the sliders, not above or below them.
      expect(box.x + box.width / 2, selector).toBeGreaterThan(bpmCentre);
      // Under the Randomize/Reset column -- the clause that separates this
      // placement from the old one, which also satisfied "inside the chrome
      // block" for none of the right reasons.
      expect(box.y, selector).toBeGreaterThanOrEqual(columnBox.y + columnBox.height);
      // Inside the chrome block on both axes. The mount clips, so a button
      // past the block's bottom edge would be cut off, not merely misplaced.
      expect(box.x, selector).toBeGreaterThanOrEqual(chromeBox.x - 1);
      expect(box.y + box.height, selector).toBeLessThanOrEqual(chromeBox.y + chromeBox.height + 1);
    }
  });

  test("no runtime page button falls in or below the encoder grid", async ({ page }) => {
    const gridBox = await page.locator(RIGHT_BLOCK_SELECTOR).boundingBox();

    for (const selector of SIDEBAR_BUTTON_SELECTORS) {
      const box = await page.locator(selector).boundingBox();
      const overlapsHorizontally = box.x < gridBox.x + gridBox.width && box.x + box.width > gridBox.x;
      expect(overlapsHorizontally && verticalOverlapPx(box, gridBox) > 0, selector).toBe(false);
      expect(box.y, selector).toBeLessThan(gridBox.y);
    }
  });

  test("the audio page button is named for what the page does", async ({ page }) => {
    // The only assertion that the app's RuntimeConfig::audioPageTitle is
    // wired all the way through Sheaf to a rendered button, so it is the one
    // that fails if the submodule pin is left behind. "Audio" alone would be
    // this instrument's first parameter bank; the page selects the output
    // device as well as the input, so it is named for both.
    await expect(page.locator(SIDEBAR_BUTTON_SELECTORS[0])).toHaveText(/Audio I\/O/);
  });


  test("a transient measurement failure on one block does not disturb the other two", async ({ page }) => {
    // Regression test for a real bug this suite caught during development:
    // mobile-stack.mjs's
    // per-frame stacking pass is supposed to be atomic across the three
    // blocks -- if ONE block's measurement fails on a given frame (e.g.
    // Sheaf's sidebar surface reporting a transient zero-extent bounds
    // mid-resize), the OTHER two must stay exactly where they were, not
    // flash back toward their native/unstacked size for that frame.
    //
    // Forces the failure deterministically: overrides the sidebar
    // element's OWN `getBoundingClientRect` to report a degenerate
    // (zero-width) rect for a short window, and polls chrome/grid's real
    // boxes throughout that window (not just before/after) to catch a
    // transient revert a single before/after snapshot would miss.
    await expect
      .poll(async () => page.locator("#synth-root").evaluate((el) => el.style.minHeight), { timeout: 5_000 })
      .not.toBe("");
    const chromeBefore = await page.locator(LEFT_BLOCK_SELECTOR).boundingBox();
    const gridBefore = await page.locator(RIGHT_BLOCK_SELECTOR).boundingBox();

    const observed = await page.evaluate(
      async ({ chromeBefore, gridBefore, leftSelector, rightSelector, sidebarSelector }) => {
        const sidebar = document.querySelector(sidebarSelector);
        const original = sidebar.getBoundingClientRect.bind(sidebar);
        sidebar.getBoundingClientRect = () => ({
          width: 0, height: 0, top: 0, left: 0, right: 0, bottom: 0, x: 0, y: 0,
          toJSON() { return this; },
        });

        const chromeEl = document.querySelector(leftSelector);
        const gridEl = document.querySelector(rightSelector);
        let maxChromeDeltaW = 0;
        let maxGridDeltaW = 0;
        const deadline = performance.now() + 250; // several render frames' worth
        while (performance.now() < deadline) {
          maxChromeDeltaW = Math.max(maxChromeDeltaW, Math.abs(chromeEl.getBoundingClientRect().width - chromeBefore.width));
          maxGridDeltaW = Math.max(maxGridDeltaW, Math.abs(gridEl.getBoundingClientRect().width - gridBefore.width));
          await new Promise((resolve) => requestAnimationFrame(resolve));
        }

        sidebar.getBoundingClientRect = original;
        return { maxChromeDeltaW, maxGridDeltaW };
      },
      { chromeBefore, gridBefore, leftSelector: LEFT_BLOCK_SELECTOR, rightSelector: RIGHT_BLOCK_SELECTOR, sidebarSelector: SIDEBAR_SELECTOR },
    );

    // A genuine revert-to-native would move these by tens to hundreds of
    // px (chrome/grid's native, un-stacked widths are far below the
    // ~390px stacked target); a generous few-px tolerance still catches
    // that while giving normal sub-pixel layout/transform-matrix rounding
    // room to be sub-pixel.
    expect(observed.maxChromeDeltaW).toBeLessThan(2);
    expect(observed.maxGridDeltaW).toBeLessThan(2);

    // Confirms recovery too: the sidebar's own real getBoundingClientRect
    // is restored above, so the very next frame should re-settle it back
    // into the stack, at the grid's shared scale (not full width -- see
    // this file's own header comment).
    const expectedSidebarWidth = await expectedStackedWidth(page, SIDEBAR_SELECTOR);
    await expect
      .poll(async () => (await page.locator(SIDEBAR_SELECTOR).boundingBox()).width, { timeout: 5_000 })
      .toBeGreaterThan(expectedSidebarWidth * 0.9);
  });

  // Positive control: proves the drag/press input
  // mapping survives the per-block transform above, for one control in
  // EACH stacked block -- not just that the boxes land in the right
  // place, but that the app still genuinely reacts to input inside them.

  test("a button press in the chrome block still reaches the app", async ({ page }) => {
    // FroggersNodeIds::SceneButton(1) ("Scene 2") -- a plain click-dispatch
    // control (no pointerDragAction) in the chrome block. Selecting it
    // moves FroggersActions::kSceneBlend's own slider to that scene's
    // blend position, a real app-computed value (not something the client
    // guesses), rendered as the native range input's own value -- a
    // "drawn value text" positive control for a
    // plain click path.
    const blendInput = page.locator('[data-synth-node-id="froggers.scene.blend"] input');
    const before = await blendInput.inputValue();
    await page.locator('[data-synth-node-id="froggers.scene.1"]').click();
    await expect(blendInput).not.toHaveValue(before, { timeout: 5_000 });
  });

  test("an encoder drag in the grid block still reaches the app", async ({ page }) => {
    // FroggersNodeIds::Encoder(0) -- a Draw-kind node whose value is only
    // ever changed via pointerDragAction (ui.ts continuePointerDrag), the
    // exact code path a drag-input regression in the mobile-stack transform
    // would break. Playwright's
    // page.mouse.* does not reliably synthesize the pointerdown/pointermove
    // sequence Chromium's PointerEvent + setPointerCapture flow needs in
    // this headless run (reproduced with zero mobile-stack transforms
    // involved, on the desktop project too -- an environment/harness
    // characteristic, not a mobile-stack defect); dispatching real
    // PointerEvents directly against the element sidesteps that and is
    // what this test does. A successful drag repaints the encoder's own
    // canvas (its drawn value), which this asserts directly -- the
    // strongest available "rendered state change" signal, stronger than
    // the internal draggedSincePointerDown flag alone.
    const encoder = page.locator('[data-synth-node-id="froggers.encoder.0"]');
    const box = await encoder.boundingBox();
    const canvasDataUrl = () => page.evaluate(() => document.querySelector('[data-synth-node-id="froggers.encoder.0"] canvas')?.toDataURL());
    const before = await canvasDataUrl();

    const center = { x: box.x + box.width / 2, y: box.y + box.height / 2 };
    await encoder.dispatchEvent("pointerdown", { pointerId: 1, clientX: center.x, clientY: center.y, bubbles: true, isPrimary: true, button: 0 });
    for (let step = 1; step <= 10; step++) {
      await encoder.dispatchEvent("pointermove", {
        pointerId: 1,
        clientX: center.x + step * 8,
        clientY: center.y - step * 8,
        bubbles: true,
        isPrimary: true,
      });
    }
    await encoder.dispatchEvent("pointerup", { pointerId: 1, clientX: center.x + 80, clientY: center.y - 80, bubbles: true, isPrimary: true });

    await expect.poll(canvasDataUrl, { timeout: 5_000 }).not.toBe(before);
  });

  test("a touch drag on an encoder reaches pointerup without the browser ever cancelling it", async ({ page }) => {
    // `#synth-root [data-synth-node-kind="draw"] { touch-action: none; }`
    // (site.css) is what stops the browser from claiming a finger-drag on
    // an encoder canvas as its own page-scroll/pan gesture; without it, a
    // touch-type pointer sequence gets cut short after a move or two --
    // the browser fires `pointercancel` on the element (the pointer has
    // been handed off to native scrolling) and, if a capture was set,
    // `lostpointercapture` fires as part of that same cancellation, before
    // the drag ever reaches its own `pointerup`. Both are the actual
    // regression signal, not merely that a drag "still works" (the encoder
    // -drag test above already covers that with mouse-type pointers, which
    // `touch-action` never governs): a scroll-hijacked drag can still end
    // up moving the encoder's value.
    const encoder = page.locator('[data-synth-node-id="froggers.encoder.0"]');
    const box = await encoder.boundingBox();
    const center = { x: box.x + box.width / 2, y: box.y + box.height / 2 };

    await page.evaluate((selector) => {
      const element = document.querySelector(selector);
      window.__touchDragEventOrder = [];
      const record = (event) => window.__touchDragEventOrder.push(event.type);
      element.addEventListener("pointercancel", record);
      element.addEventListener("lostpointercapture", record);
      element.addEventListener("pointerup", record);
    }, '[data-synth-node-id="froggers.encoder.0"]');

    await encoder.dispatchEvent("pointerdown", {
      pointerId: 1,
      pointerType: "touch",
      clientX: center.x,
      clientY: center.y,
      bubbles: true,
      isPrimary: true,
      button: 0,
    });
    for (let step = 1; step <= 10; step++) {
      await encoder.dispatchEvent("pointermove", {
        pointerId: 1,
        pointerType: "touch",
        clientX: center.x + step * 8,
        clientY: center.y - step * 8,
        bubbles: true,
        isPrimary: true,
      });
    }
    await encoder.dispatchEvent("pointerup", {
      pointerId: 1,
      pointerType: "touch",
      clientX: center.x + 80,
      clientY: center.y - 80,
      bubbles: true,
      isPrimary: true,
    });

    const eventOrder = await page.evaluate(() => window.__touchDragEventOrder);
    expect(eventOrder).not.toContain("pointercancel");
    expect(eventOrder).toContain("pointerup");
    const lostCaptureIndex = eventOrder.indexOf("lostpointercapture");
    if (lostCaptureIndex !== -1) {
      expect(lostCaptureIndex).toBeGreaterThan(eventOrder.indexOf("pointerup"));
    }
  });
});
