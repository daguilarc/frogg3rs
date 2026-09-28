// Visibility regression suite. Every existing assertion in
// this e2e suite measures GEOMETRY (boundingBox() / getBoundingClientRect,
// see helpers.mjs and mobile-stacking.spec.mjs), and geometry reports a
// full, correct box for an element that is actually clipped to
// invisibility by an ancestor -- exactly how a real defect (an earlier
// per-block stacking mechanism's wide branch briefly clearing
// `mount.style.height`, which Sheaf's own fitSurface owns) once produced a
// blank surface at every wide viewport while every geometry assertion kept
// passing. The defect class is broader than
// that one bug: an element can have correct geometry and be invisible via
// ancestor `height: 0` + `overflow: hidden`, `display: none`, a zero-size
// canvas, or a canvas that is sized but never painted. Each assertion
// below measures actual rendered/painted state instead: the mount's own
// real extent, genuine viewport intersection (via a real
// IntersectionObserver, which resolves against ancestor clipping the way
// boundingBox() never does), and live canvas pixel content.
//
// Runs under BOTH the mobile and desktop projects (playwright.config.mjs):
// the mount-height check needs coverage at both narrow and wide viewports
// by the nature of what it proves, and none of this file's other
// assertions are viewport-dependent, the same reasoning link-roles.spec.mjs
// already uses to run under both projects.
import { expect, test } from "@playwright/test";
import {
  ENCODER_CANVAS_SELECTORS,
  PLAY_SELECTOR,
  SURFACE_ROOT_SELECTOR,
  SYNTH_ROOT_SELECTOR,
  expectEncoderCanvasVisible,
  waitForAudioOnline,
  waitForSurfaceReady,
} from "./helpers.mjs";

test.describe("surface visibility", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/");
    await waitForSurfaceReady(page);
  });

  test("the mount reaches a non-zero rendered height", async ({ page }) => {
    // The MOUNT's own box, not a descendant's. A descendant absolutely
    // positioned inside it (e.g. the surface root, checked separately
    // below) reports its own full, correct box even while the mount
    // itself has collapsed to zero height -- this is the mount's real
    // rendered extent, the exact property the historical defect zeroed.
    const box = await page.locator(SYNTH_ROOT_SELECTOR).boundingBox();
    expect(box).not.toBeNull();
    expect(box.height).toBeGreaterThan(0);
  });

  test("the surface root actually intersects the viewport", async ({ page }) => {
    // toBeInViewport() (default ratio 0: any positive intersection)
    // polls a real IntersectionObserver, which computes its intersection
    // rectangle against every ancestor's own clipping -- unlike
    // boundingBox()/getBoundingClientRect(), which reports an element's
    // own box regardless of whether an ancestor clips it away.
    await expect(page.locator(SURFACE_ROOT_SELECTOR)).toBeInViewport();
  });

  test("a sample of encoder canvases are painted and visible before Play is ever clicked", async ({ page }) => {
    // Pre-Play painting is a real guarantee at the pinned Sheaf commit
    // (`ui-state-before-audio`, "populate UI state before the audio pump
    // has ever run"): encoder cells paint names/values with no click and
    // no Play. Nothing here clicks anything.
    for (const selector of ENCODER_CANVAS_SELECTORS) {
      await expectEncoderCanvasVisible(page, selector);
    }
  });

  test("a sample of encoder canvases are painted and visible after audio starts", async ({ page }) => {
    await page.locator(PLAY_SELECTOR).click();
    await waitForAudioOnline(page);
    for (const selector of ENCODER_CANVAS_SELECTORS) {
      await expectEncoderCanvasVisible(page, selector);
    }
  });

  test("the header logo image resolves", async ({ page }) => {
    // Checks the decoded image's own natural dimensions, not merely that
    // the <img> element is present: an element whose `src` failed to load
    // still exists in the DOM (rendered as the browser's broken-image
    // icon) and would pass an existence-only or even a boundingBox()-based
    // check, so only the browser's own decoded-image state actually
    // proves the file shipped.
    const logo = page.locator(".site-logo");
    await expect.poll(() => logo.evaluate((img) => img.naturalWidth), { timeout: 10_000 }).toBeGreaterThan(0);
    await expect.poll(() => logo.evaluate((img) => img.naturalHeight), { timeout: 10_000 }).toBeGreaterThan(0);
  });
});
