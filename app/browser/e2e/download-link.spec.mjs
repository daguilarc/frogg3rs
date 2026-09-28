// Footer download-link coverage for the device-gated behaviour task 3.2
// adds to site-boot.mjs (via device-class.mjs's classification): the
// generic desktop-app download link becomes the Android release on an
// Android phone, and is removed together with the separator right after it
// on an iPhone. Runs under android, mobile (iPhone -- see
// playwright.config.mjs's own userAgent) and desktop.
// link-roles.spec.mjs's own "the download role resolves per project" test
// covers the same href per project at a lighter grain (presence/absence +
// href only); this file is the dedicated coverage for the exact link text
// and for the separator's fate.
import { expect, test } from "@playwright/test";

const EXPECTATIONS = {
  android: {
    text: "Download Android app",
    href: "https://github.com/daguilarc/frogg3rs/releases/tag/frogg3rs_android",
  },
  desktop: {
    text: "Download the desktop app",
    href: "https://github.com/daguilarc/frogg3rs/releases/tag/frogg3rs_v2",
  },
  // "mobile" (iPhone) is deliberately absent: no download link exists there.
};

test.describe("footer download link", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/");
  });

  test("resolves for this project's device class", async ({ page }, testInfo) => {
    const expectation = EXPECTATIONS[testInfo.project.name];
    const link = page.locator('[data-site-link="download"]');
    if (!expectation) {
      await expect(link).toHaveCount(0);
      return;
    }
    await expect(link).toHaveText(expectation.text);
    await expect(link).toHaveAttribute("href", expectation.href);
  });

  test("the separator after the download link is removed together with it", async ({ page }, testInfo) => {
    const expectation = EXPECTATIONS[testInfo.project.name];
    const linkCount = await page.locator(".site-links a").count();
    const separatorCount = await page.locator('.site-links span[aria-hidden="true"]').count();
    if (!expectation) {
      // download link gone: plugin, license, manual (3 links, 2 separators).
      expect(linkCount).toBe(3);
      expect(separatorCount).toBe(2);
    } else {
      // download, plugin, license, manual (4 links, 3 separators).
      expect(linkCount).toBe(4);
      expect(separatorCount).toBe(3);
    }
  });
});
