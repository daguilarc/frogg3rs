// Desktop-emulated layout sanity. Runs only under the
// "desktop" project (playwright.config.mjs testMatch). Proves the app
// actually boots and renders its real UI frame over the direct catalog
// load path (site-boot.mjs) with no picker/launcher UI ever shown, and
// that the surface renders at a sane, non-degenerate size on a wide
// viewport. Never starts audio -- no control is clicked here.
import { expect, test } from "@playwright/test";
import {
  ENCODER_ROW_SELECTORS,
  LEFT_BLOCK_SELECTOR,
  RIGHT_BLOCK_SELECTOR,
  SIDEBAR_SELECTOR,
  SURFACE_ROOT_SELECTOR,
  encoderGridBoundingBox,
  verticalOverlapPx,
  waitForSurfaceReady,
} from "./helpers.mjs";

test.describe("desktop layout sanity", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/");
    await waitForSurfaceReady(page);
  });

  test("no catalog-browser picker is ever shown", async ({ page }) => {
    // SheafPatchLauncher (the picker UI) always renders a
    // `.synth-launcher` shell with a `.synth-launcher__apps` list --
    // site-boot.mjs never imports or constructs that class at all, so
    // this must stay completely absent, at every point in the page
    // lifecycle, not just after boot.
    await expect(page.locator(".synth-launcher")).toHaveCount(0);
  });

  test("the surface renders at a sane, non-degenerate size", async ({ page }) => {
    const root = page.locator(SURFACE_ROOT_SELECTOR);
    await expect(root).toBeVisible();
    const box = await root.boundingBox();
    expect(box).not.toBeNull();
    expect(box.width).toBeGreaterThan(200);
    expect(box.height).toBeGreaterThan(200);
  });

  test("the encoder grid and the left chrome block both render", async ({ page }) => {
    const grid = await encoderGridBoundingBox(page);
    expect(grid).not.toBeNull();
    expect(grid.width).toBeGreaterThan(0);
    expect(grid.height).toBeGreaterThan(0);
    await expect(page.locator(LEFT_BLOCK_SELECTOR)).toBeVisible();
  });

  test("all four encoder-grid rows render at equal width (one coherent grid)", async ({ page }) => {
    const boxes = await Promise.all(ENCODER_ROW_SELECTORS.map((selector) => page.locator(selector).boundingBox()));
    for (const box of boxes) expect(box).not.toBeNull();
    const widths = boxes.map((box) => Math.round(box.width));
    expect(new Set(widths).size).toBe(1);
  });

  test("no audio starts on load", async ({ page }) => {
    // SynthBrowserApp only starts audio from a user gesture dispatched
    // through the app's own UI (main.ts's BrowserUiBackend dispatch
    // wiring); this suite never clicks anything, so the root's own status
    // dataset must never reach a "running"/"audio:online" state.
    const status = await page.locator("#synth-root").getAttribute("data-synth-status");
    expect(status ?? "").not.toContain("audio:online");
  });

  // viewport-width.mjs reports the mount's width from a ResizeObserver on
  // the same host element Sheaf's own fitSurface observes, not window
  // "resize" alone (viewport-width.mjs's own header comment) -- this drives
  // an actual live resize (starting wide, matching this describe block's
  // own project) rather than a fresh page load at a narrow viewport, so it
  // exercises that observer path specifically, in both directions.
  test("a live resize from wide to narrow engages the stack, and back restores wide layout", async ({ page }) => {
    const gridBefore = await page.locator(RIGHT_BLOCK_SELECTOR).boundingBox();
    const leftBefore = await page.locator(LEFT_BLOCK_SELECTOR).boundingBox();

    await page.setViewportSize({ width: 390, height: 844 });
    // Settled-narrow signal: the grid's own rendered box has reached near
    // the left edge (stacked, not beside chrome) and close to the new
    // viewport's width. A resize is reported asynchronously (the observer
    // callback, then a round trip through the wasm app's own next
    // BuildTree()), so this polls rather than reading once immediately
    // after setViewportSize() resolves.
    await expect
      .poll(
        async () => {
          const box = await page.locator(RIGHT_BLOCK_SELECTOR).boundingBox();
          return box.x < 50 && box.width >= 390 * 0.85;
        },
        { timeout: 5_000 },
      )
      .toBe(true);

    const chromeNarrow = await page.locator(LEFT_BLOCK_SELECTOR).boundingBox();
    const gridNarrow = await page.locator(RIGHT_BLOCK_SELECTOR).boundingBox();
    // Chrome stacks fully above the grid, at the same width -- the surface's
    // own narrow tree (FroggersPageLayout::ComputeNarrowBlockHeights()),
    // not independently stretched by this shell.
    expect(verticalOverlapPx(chromeNarrow, gridNarrow)).toBe(0);
    expect(Math.abs(chromeNarrow.width - gridNarrow.width)).toBeLessThan(gridNarrow.width * 0.05);

    const sidebarNarrow = await page.locator(SIDEBAR_SELECTOR).boundingBox();
    // Never beside the grid, at either placement, and inside the chrome
    // block -- resolved by Sheaf's RuntimeMainComponent against the
    // surface's own declared slot (mobile-stacking.spec.mjs's own header
    // comment has the full mechanism), not computed by this shell.
    expect(verticalOverlapPx(sidebarNarrow, gridNarrow)).toBe(0);
    expect(sidebarNarrow.y).toBeGreaterThanOrEqual(chromeNarrow.y - 1);
    expect(sidebarNarrow.y + sidebarNarrow.height).toBeLessThanOrEqual(chromeNarrow.y + chromeNarrow.height + 1);

    await page.setViewportSize({ width: 1280, height: 800 });
    // Wide layout matches the original exactly, once the resize has been
    // reported and the surface has rebuilt its wide (config-bounds) tree.
    await expect
      .poll(
        async () => {
          const box = await page.locator(RIGHT_BLOCK_SELECTOR).boundingBox();
          return Math.abs(box.width - gridBefore.width) < 1 && Math.abs(box.x - gridBefore.x) < 1;
        },
        { timeout: 5_000 },
      )
      .toBe(true);
    const gridAfter = await page.locator(RIGHT_BLOCK_SELECTOR).boundingBox();
    const leftAfter = await page.locator(LEFT_BLOCK_SELECTOR).boundingBox();
    expect(gridAfter).toEqual(gridBefore);
    expect(leftAfter).toEqual(leftBefore);
  });
});
