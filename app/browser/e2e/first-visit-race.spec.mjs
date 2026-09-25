// Permanent regression test for the first-visit COI boot race. On a genuine
// first visit to the deployed (no-isolation-headers)
// configuration, coi-serviceworker.js's page branch kicks off an async
// register/ready/reload sequence while site-boot.mjs's module boot races it
// independently; a slow-enough machine (or, deterministically here, CPU
// throttling) lets the wasm module's pthread pool start before the reload
// lands, which throws on the un-isolated context and paints the boot-error
// panel on a perfectly capable browser. This file asserts that race can no
// longer reach the panel.
//
// Each Playwright `test()` gets a brand-new browser context (and therefore
// empty sessionStorage) by default, so every invocation below IS a first
// visit -- no explicit sessionStorage.clear() is needed, and none is done,
// because doing so INSIDE a test would require a page already loaded, by
// which point the race under test has already run once.
import { test, expect } from "@playwright/test";
import { BOOT_ERROR_DETAIL_SELECTOR, waitForSurfaceReady } from "./helpers.mjs";

test.describe("first-visit boot race", () => {
  test("a throttled first visit never shows the boot-error panel and still reaches a rendered surface", async ({ page, context }) => {
    const client = await context.newCDPSession(page);
    await client.send("Emulation.setCPUThrottlingRate", { rate: 20 });
    await page.goto("/");
    await waitForSurfaceReady(page);
    await expect(page.locator(BOOT_ERROR_DETAIL_SELECTOR)).toHaveCount(0);
  });
});

// The other half of the suppression, and the one that protects the panel.
// Suppressing the boot-error report while an isolation attempt is owed is
// only safe because every path where no reload is coming settles the
// attempt instead. If it ever stops settling, the page goes silently
// blank forever -- strictly worse than showing a false panel, because a
// visitor is left with no reason at all. Registration
// failing is the cleanest way to reach that path: coi-serviceworker.js's
// register().catch() settles, site-boot.mjs proceeds, the un-isolated
// SharedArrayBuffer transfer throws for real, and the panel MUST appear.
test("a page whose service worker cannot register still reports the failure", async ({ page }) => {
  await page.addInitScript(() => {
    navigator.serviceWorker.register = () =>
      Promise.reject(new Error("registration blocked for test"));
  });
  await page.goto("/");
  await expect(page.locator(BOOT_ERROR_DETAIL_SELECTOR)).toBeVisible({ timeout: 30_000 });
});
