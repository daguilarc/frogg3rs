// Link-role regression: site
// links carry the legacy roles under the new repository references. Runs
// under the mobile, desktop and android projects (playwright.config.mjs)
// since link presence and target correctness do not depend on viewport.
// Never starts audio -- no control is clicked here at all.
import { expect, test } from "@playwright/test";

// Unaffected by device class (task 3.2 only ever touches the download
// role) -- present, and at this href, on every project.
const COMMON_ROLES = [
  { role: "plugin", hrefPrefix: "https://github.com/daguilarc/frogg3rs/releases/tag/frogg3rs_vst" },
  { role: "license", hrefPrefix: "https://github.com/daguilarc/frogg3rs/blob/main/LICENSE" },
  { role: "manual", hrefPrefix: "https://github.com/daguilarc/frogg3rs/blob/main/MANUAL.md" },
];

// The download role's href (and whether it exists at all) is gated by
// site-boot.mjs's device-class treatment (task 3.2): absent under "mobile"
// (an iPhone user agent), the desktop release under "desktop", the Android
// release under "android". download-link.spec.mjs covers the resulting
// text and the separator's fate at finer grain; this file only asserts
// which project resolves to which role.
const DOWNLOAD_HREF_BY_PROJECT = {
  desktop: "https://github.com/daguilarc/frogg3rs/releases/tag/frogg3rs_v2",
  android: "https://github.com/daguilarc/frogg3rs/releases/tag/frogg3rs_android",
};

test.describe("site link roles", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/");
  });

  for (const { role, hrefPrefix } of COMMON_ROLES) {
    test(`${role} link is present and resolves to the new origin`, async ({ page }) => {
      const link = page.locator(`[data-site-link="${role}"]`);
      await expect(link).toHaveCount(1);
      await expect(link).toHaveAttribute("href", hrefPrefix);
    });
  }

  test("the download role resolves per project", async ({ page }, testInfo) => {
    const expectedHref = DOWNLOAD_HREF_BY_PROJECT[testInfo.project.name];
    const link = page.locator('[data-site-link="download"]');
    if (expectedHref === undefined) {
      await expect(link).toHaveCount(0);
      return;
    }
    await expect(link).toHaveCount(1);
    await expect(link).toHaveAttribute("href", expectedHref);
  });

  test("no link targets the old repository name", async ({ page }) => {
    const hrefs = await page.locator("a[href]").evaluateAll((anchors) => anchors.map((a) => a.getAttribute("href")));
    expect(hrefs.length).toBeGreaterThan(0);
    for (const href of hrefs) {
      expect(href).not.toContain("FroggersTiga");
    }
  });

  test("every non-download link role is present exactly once", async ({ page }) => {
    for (const { role } of COMMON_ROLES) {
      await expect(page.locator(`[data-site-link="${role}"]`)).toHaveCount(1);
    }
  });
});
