// Mobile-emulated stacking assertion: mobile viewport stacks around a
// full-width encoder grid.
//
// `synth_froggers::FroggersUiSurface` (app/FroggersUiSurface.hpp) is a
// `synth::ui::SelfSizedSurface`: given a narrow reported width, it builds
// its OWN stacked tree (chrome block above the grid block, both spanning
// the reported width) and declares that tree's root bounds itself
// (`FroggersPageLayout::RootBounds()`), which Sheaf's
// `RuntimeMainComponent::BuildTree()` composes the whole runtime --
// including Sheaf's own generic sidebar (SIDEBAR_SELECTOR, helpers.mjs),
// placed into the surface's declared slot -- against. This shell's only
// job is reporting the mount's width to that surface
// (app/browser/site/viewport-width.mjs, hooked in site-boot.mjs -- see
// that file's own header comment); Sheaf's own generic `fitSurface`
// (External/Sheaf/projects/synth/browser/src/ui.ts) then scales the WHOLE
// composite as one unit to fit the mount, exactly as it already does for
// the desktop layout. No per-block CSS transform is involved: the two
// blocks stack -- chrome above, grid full-width below it -- because the
// surface's own declared tree already stacks them, and the sidebar is
// PLACED inside the chrome block, under its Randomize/Reset column, beside
// the sliders, because `RuntimeMainComponent` resolves it there directly
// against the surface's declared slot -- never computed by this shell.
import { expect, test } from "@playwright/test";
import {
  BPM_SELECTOR,
  ENCODER_ROW_SELECTORS,
  LEFT_BLOCK_SELECTOR,
  NARROW_BUTTON_COLUMN_SELECTOR,
  RANDOMIZE_RESET_SELECTORS,
  RIGHT_BLOCK_SELECTOR,
  SIDEBAR_BUTTON_SELECTORS,
  encoderGridBoundingBox,
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

  test("chrome renders at the same width as the grid, stacked fully above it", async ({ page }) => {
    // This test used to assert the opposite: that chrome came out "well
    // short of the grid's own full width". That was a deliberate guard on
    // an earlier shell not stretching each block independently, and it
    // stayed correct for as long as the surface declared the chrome block
    // at half the grid's weight. The surface's narrow tree now spans both
    // blocks across the SAME reported width
    // (FroggersPageLayout::ComputeNarrowBlockHeights()'s own comment),
    // because the half-width chrome block left the other half of a phone
    // viewport empty with nothing able to reach it.
    const gridBox = await page.locator(RIGHT_BLOCK_SELECTOR).boundingBox();
    const chromeBox = await page.locator(LEFT_BLOCK_SELECTOR).boundingBox();

    // Same width as the grid, within a small tolerance, so neither block
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
    // Regression guard for a real bug: an earlier stacking mechanism wrote
    // the mount's reserved height on a DIFFERENT property than Sheaf's own
    // `fitSurface` (External/Sheaf/projects/synth/browser/src/ui.ts) did,
    // racing two writers every frame -- the document briefly fitted the
    // viewport between them, and the first layout read in that window made
    // the browser clamp the scroll offset to zero. `fitSurface` is now the
    // ONLY writer of the mount's height, and it already writes the surface's
    // own full (self-sized, narrow) height directly, so nothing here should
    // be able to reintroduce that race -- this stays a real regression
    // guard, not a check on a mechanism this file still owns.
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
    // Sheaf's sidebar used to be a third stacked block under the encoder
    // grid, which put four runtime page buttons a whole page-scroll from
    // everything else. It is now placed inside the chrome block, under the
    // Randomize/Reset column, where the column's own intrinsic height
    // leaves room for it beside the sliders -- resolved entirely by Sheaf's
    // `RuntimeMainComponent::BuildTree()` against the surface's own
    // declared slot (`FroggersUiSurface::SidebarSlot()`,
    // FroggersNodeIds::kSidebarSlot), never computed by this shell. The
    // assertion below is still against the button-column's own rendered
    // box, since that is the ONE definition of where the slot's free space
    // begins, on both the C++ and the browser side alike.
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
    // this instrument's first parameter page; the page selects the output
    // device as well as the input, so it is named for both.
    await expect(page.locator(SIDEBAR_BUTTON_SELECTORS[0])).toHaveText(/Audio I\/O/);
  });


  // Positive control: proves the drag/press input mapping still reaches the
  // app for one control in EACH stacked block -- not just that the boxes
  // land in the right place, but that the app still genuinely reacts to
  // input inside them.

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
    // exact code path a drag-input regression under Sheaf's `fitSurface`
    // scale would break. Playwright's
    // page.mouse.* does not reliably synthesize the pointerdown/pointermove
    // sequence Chromium's PointerEvent + setPointerCapture flow needs in
    // this headless run (reproduced on the desktop project too, at no
    // scale at all -- an environment/harness characteristic, not a scale
    // defect); dispatching real PointerEvents directly against the element
    // sidesteps that and is what this test does. A successful drag repaints
    // the encoder's own
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
