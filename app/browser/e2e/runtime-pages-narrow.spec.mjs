// Sheaf's runtime pages (Audio I/O, Controllers, Sync, File) at a narrow
// (phone) viewport. Each page is Sheaf's own fixed-content-bounds tree
// (RuntimePages.hpp/ControllersPageUI.hpp), composed by
// `RuntimeMainComponent::BuildTree()` beside Sheaf's sidebar into one
// composite root (`runtime.main.root`) that Sheaf's own generic
// `fitSurface` (External/Sheaf/projects/synth/browser/src/ui.ts) scales as
// a single unit to fit the mount -- the SAME scale-to-fit mechanism that
// makes the frogg3rs app's own narrow, self-sized tree work (see
// mobile-stacking.spec.mjs's own header comment). A page's own content is
// not narrow-aware the way FroggersUiSurface is, so this suite's job is
// narrower: prove that scale-to-fit alone is enough to keep every page
// usable at a phone width -- rendered, on screen, and never wider than the
// mount -- and that returning to the app afterward still gets the app's
// own narrow layout back.
import { expect, test } from "@playwright/test";
import {
  BOOT_ERROR_DETAIL_SELECTOR,
  SIDEBAR_BUTTON_SELECTORS,
  SYNTH_ROOT_SELECTOR,
  encoderGridBoundingBox,
  waitForSurfaceReady,
} from "./helpers.mjs";

// RuntimeMainComponent.hpp's own composite root -- the ONE node Sheaf's
// `fitSurface` actually applies its `transform: scale(...)` to, whichever
// page (or the app itself) is currently shown.
const RUNTIME_MAIN_ROOT_SELECTOR = '[data-synth-node-id="runtime.main.root"]';

// RuntimePages.hpp's/ControllersPageUI.hpp's own root/back node ids, in
// SIDEBAR_BUTTON_SELECTORS' own order (helpers.mjs: Audio, Controllers,
// Sync, File).
const RUNTIME_PAGES = [
  { open: SIDEBAR_BUTTON_SELECTORS[0], root: '[data-synth-node-id="runtime.audio.root"]', back: '[data-synth-node-id="runtime.audio.back"]' },
  { open: SIDEBAR_BUTTON_SELECTORS[1], root: '[data-synth-node-id="runtime.controllers.root"]', back: '[data-synth-node-id="runtime.controllers.back"]' },
  { open: SIDEBAR_BUTTON_SELECTORS[2], root: '[data-synth-node-id="runtime.sync.root"]', back: '[data-synth-node-id="runtime.sync.back"]' },
  { open: SIDEBAR_BUTTON_SELECTORS[3], root: '[data-synth-node-id="runtime.file.root"]', back: '[data-synth-node-id="runtime.file.back"]' },
];

test.describe("runtime pages at a narrow viewport", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/");
    await waitForSurfaceReady(page);
  });

  test("Audio I/O, Controllers, Sync and File each render usably, then the app's narrow layout returns", async ({ page }) => {
    const pageErrors = [];
    page.on("pageerror", (error) => pageErrors.push(error));

    for (const runtimePage of RUNTIME_PAGES) {
      await page.locator(runtimePage.open).click();

      // The page's own root renders and genuinely intersects the viewport
      // (a real IntersectionObserver, which resolves against ancestor
      // clipping -- see helpers.mjs's own note on why this is stronger
      // than a bounding-box check alone).
      await expect(page.locator(runtimePage.root), runtimePage.root).toBeInViewport({ timeout: 5_000 });

      // The scaled composite never renders wider than the mount itself --
      // fitSurface's own `Math.min(1, availableWidth / surfaceWidth)`
      // guarantees this by construction, so a failure here means that
      // guarantee broke, not that this page's own content misbehaved.
      const mountBox = await page.locator(SYNTH_ROOT_SELECTOR).boundingBox();
      const scaledRootBox = await page.locator(RUNTIME_MAIN_ROOT_SELECTOR).boundingBox();
      expect(scaledRootBox.width, runtimePage.root).toBeLessThanOrEqual(mountBox.width + 1);

      // No boot-error notice and no uncaught page error surfaced while
      // this page was open.
      await expect(page.locator(BOOT_ERROR_DETAIL_SELECTOR)).toHaveCount(0);
      expect(pageErrors, runtimePage.root).toHaveLength(0);

      await page.locator(runtimePage.back).click();
    }

    // Back in the app, at the same narrow viewport: the encoder grid spans
    // the viewport width again, exactly like mobile-stacking.spec.mjs's own
    // "the encoder grid spans the full viewport width" -- the app's own
    // narrow layout was not left behind by having visited every runtime
    // page.
    const grid = await encoderGridBoundingBox(page);
    const viewport = page.viewportSize();
    expect(grid.width).toBeGreaterThanOrEqual(viewport.width * 0.9);
  });
});
