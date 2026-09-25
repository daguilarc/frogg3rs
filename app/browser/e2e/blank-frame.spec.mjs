// Blank-frame regression. A holistic, screen-truth backstop for
// visibility.spec.mjs's own header comment's defect class: instead of
// asking any specific element whether IT thinks it is visible, this reads
// the actual rendered pixels a visitor would see. Two independent samples,
// both required to pass: the whole band between header and footer (catches
// a total blackout of the app surface) and the encoder grid's own region
// alone (catches a blank restricted to the grid -- Sheaf's own button and
// slider chrome carries hardcoded CSS fill and border colours distinct
// from the page background, synth-browser.css lines 61-66 and 100-130, so a
// regression that blanks every encoder canvas while leaving transport/
// scene controls rendering normally would still find color variety in,
// and pass, a whole-band-only sample). Desktop project only
// (playwright.config.mjs): the wide viewport is the scope for this
// specific check.
import { expect, test } from "@playwright/test";
import { encoderGridBoundingBox, waitForSurfaceReady } from "./helpers.mjs";

test.describe("blank frame", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/");
    await waitForSurfaceReady(page);
  });

  test("the rendered frame is not one uniform color, in the header-to-footer band or in the encoder grid alone", async ({ page }) => {
    // Bounds the first sample to strictly BETWEEN the site shell's own
    // header and footer chrome (index.html's `.site-header` / `.site-links`),
    // which render unconditionally regardless of the app surface's own
    // state. Sampling the full viewport instead would risk a false pass
    // from the header title's or footer links' own text -- real color
    // variety that has nothing to do with the surface -- even when the
    // surface itself is a uniform blank, which would silently defeat
    // this exact check.
    const headerBox = await page.locator(".site-header").boundingBox();
    const footerBox = await page.locator(".site-links").boundingBox();
    const viewport = page.viewportSize();
    const bandTop = Math.ceil(headerBox.y + headerBox.height);
    const bandBottom = Math.floor(footerBox.y);
    expect(bandBottom).toBeGreaterThan(bandTop); // sanity: there is a real band to sample

    // The second sample is bounded to the encoder grid's own region --
    // nothing but encoder canvases renders there, so unlike the band
    // above it shares no pass with the left chrome block's own button/
    // slider colours (see this file's header comment).
    const gridBox = await encoderGridBoundingBox(page);
    expect(gridBox).not.toBeNull();

    const screenshot = await page.screenshot();
    // Decoded and sampled IN the page (native <img>/<canvas> PNG decode)
    // rather than via a Node-side PNG parser -- no extra dependency, and
    // the data: URL image is same-origin so the canvas is never tainted.
    const [bandColors, gridColors] = await page.evaluate(
      async ({ base64, band, grid }) => {
        const image = new Image();
        image.src = `data:image/png;base64,${base64}`;
        await image.decode();
        const canvas = document.createElement("canvas");
        canvas.width = image.naturalWidth;
        canvas.height = image.naturalHeight;
        const context = canvas.getContext("2d");
        context.drawImage(image, 0, 0);

        const columns = 12;
        const rows = 12;
        const sampleRect = (rect) => {
          const samples = [];
          for (let row = 0; row < rows; row++) {
            for (let column = 0; column < columns; column++) {
              const x = Math.floor(rect.left + ((column + 0.5) * (rect.right - rect.left)) / columns);
              const y = Math.floor(rect.top + ((row + 0.5) * (rect.bottom - rect.top)) / rows);
              samples.push(Array.from(context.getImageData(x, y, 1, 1).data).join(","));
            }
          }
          return samples;
        };
        return [sampleRect(band), sampleRect(grid)];
      },
      {
        base64: screenshot.toString("base64"),
        band: { left: 0, top: bandTop, right: viewport.width, bottom: bandBottom },
        grid: { left: gridBox.x, top: gridBox.y, right: gridBox.x + gridBox.width, bottom: gridBox.y + gridBox.height },
      },
    );

    expect(new Set(bandColors).size).toBeGreaterThan(1);
    expect(new Set(gridColors).size).toBeGreaterThan(1);
  });
});
