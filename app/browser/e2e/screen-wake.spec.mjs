// Screen wake lock coverage for the device-gated behaviour task 3.3 adds to
// site-boot.mjs: a wake lock is requested while the AudioContext driving
// the app is running and the page is visible, released when either stops
// holding, and a request denial never reaches the page's own
// `unhandledrejection` handler (helpers.mjs's BOOT_ERROR_DETAIL_SELECTOR,
// which only that handler paints). Runs under android, mobile (iPhone) and
// desktop.
//
// Playwright's headless Chromium never actually hides a page for opening or
// activating a second tab (a probe found document.visibilityState staying
// "visible" with no visibilitychange event at all), so hiding is simulated
// here instead of a real tab switch: the init script below makes
// document.visibilityState report a value this file controls, and the
// "simulated hide and show" step below drives it through the same sequence
// a real hide performs -- release the held sentinel (the Screen Wake Lock
// spec's own automatic release on a hidden document), then dispatch
// visibilitychange -- for hidden, then again for visible. The real hide on
// Android is 3.5's power-key run, not this suite.
import { expect, test } from "@playwright/test";
import { BOOT_ERROR_DETAIL_SELECTOR, PLAY_SELECTOR, waitForAudioOnline, waitForSurfaceReady } from "./helpers.mjs";

/**
 * Replaces navigator.wakeLock with a recorder before the page's own scripts
 * run: every call to request() is logged (regardless of outcome), and
 * either refuses (matching a real denial) or returns a stand-in sentinel
 * whose own "release" event fires its registered listeners, the same
 * contract a real WakeLockSentinel offers.
 */
async function installWakeLockRecorder(page, { refuse = false } = {}) {
  await page.addInitScript((refuseRequests) => {
    window.__wakeLockRequests = [];
    let visibility = document.visibilityState;
    Object.defineProperty(document, "visibilityState", {
      configurable: true,
      get: () => visibility,
    });
    window.__setVisibility = (value) => {
      visibility = value;
      document.dispatchEvent(new Event("visibilitychange"));
    };

    let lastSentinel = null;
    Object.defineProperty(navigator, "wakeLock", {
      configurable: true,
      value: {
        request: (type) => {
          window.__wakeLockRequests.push(type);
          if (refuseRequests) return Promise.reject(new DOMException("denied", "NotAllowedError"));
          const listeners = new Set();
          const sentinel = {
            released: false,
            type,
            addEventListener: (name, handler) => {
              if (name === "release") listeners.add(handler);
            },
            removeEventListener: (name, handler) => listeners.delete(handler),
            release() {
              if (this.released) return Promise.resolve();
              this.released = true;
              for (const handler of listeners) handler();
              return Promise.resolve();
            },
          };
          lastSentinel = sentinel;
          return Promise.resolve(sentinel);
        },
      },
    });
    window.__releaseLastWakeLockSentinel = () => {
      if (lastSentinel) lastSentinel.release();
    };
  }, refuse);
}

async function wakeLockRequestCount(page) {
  return page.evaluate(() => window.__wakeLockRequests.length);
}

test.describe("screen wake lock", () => {
  test("one request once the context runs, a new one after a simulated hide and show", async ({ page }, testInfo) => {
    await installWakeLockRecorder(page);
    await page.goto("/");
    await waitForSurfaceReady(page);

    await page.locator(PLAY_SELECTOR).click();
    await waitForAudioOnline(page);

    // Only android and mobile (iPhone) are gated on -- see
    // device-class.mjs's classification and site-boot.mjs's own use of it;
    // desktop never wires a wake lock at all.
    const expectsWakeLock = testInfo.project.name !== "desktop";
    await expect.poll(() => wakeLockRequestCount(page)).toBe(expectsWakeLock ? 1 : 0);

    await page.evaluate(() => {
      window.__setVisibility("hidden");
      window.__releaseLastWakeLockSentinel();
    });
    await page.evaluate(() => window.__setVisibility("visible"));

    await expect.poll(() => wakeLockRequestCount(page)).toBe(expectsWakeLock ? 2 : 0);

    await expect(page.locator(BOOT_ERROR_DETAIL_SELECTOR)).toHaveCount(0);
  });

  test("a refused wake lock never surfaces a boot-error notice", async ({ page }, testInfo) => {
    test.skip(testInfo.project.name === "desktop", "desktop never requests a wake lock (see the companion test)");
    await installWakeLockRecorder(page, { refuse: true });
    await page.goto("/");
    await waitForSurfaceReady(page);

    await page.locator(PLAY_SELECTOR).click();
    await waitForAudioOnline(page);

    await expect.poll(() => wakeLockRequestCount(page)).toBeGreaterThan(0);
    await expect(page.locator(BOOT_ERROR_DETAIL_SELECTOR)).toHaveCount(0);
  });
});
