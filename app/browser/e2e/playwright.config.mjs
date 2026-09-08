// Playwright config for the site's own e2e harness -- a fresh file,
// chromium-only and workers capped, to stay within this machine's own
// resource limits.
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { defineConfig, devices } from "@playwright/test";

const host = "127.0.0.1";
const port = 8799;
// A second origin served the way GitHub Pages serves: no isolation headers at
// all. The site's service-worker shim is the only thing that isolates the page
// there, so a suite that only ever runs against the header-setting server
// cannot see a shim that has stopped working.
const pagesPort = 8800;

// Reuse an already-downloaded chromium build from the shared Playwright
// cache instead of triggering a second download
// (External/Sheaf/projects/synth/browser already pins @playwright/test
// ^1.60.0 / chromium-1223 on this machine).
if (!process.env.CI) {
  const cacheCandidates = [
    path.join(os.homedir(), "Library/Caches/ms-playwright"),
    path.join(os.homedir(), ".cache/ms-playwright"),
  ];
  for (const cacheDir of cacheCandidates) {
    if (!fs.existsSync(cacheDir)) continue;
    const hasChromium = fs.readdirSync(cacheDir).some((name) => name.startsWith("chromium"));
    if (!hasChromium) continue;
    process.env.PLAYWRIGHT_BROWSERS_PATH = cacheDir;
    break;
  }
}

const MOBILE_SPECS = [/link-roles\.spec\.mjs$/, /mobile-stacking\.spec\.mjs$/, /visibility\.spec\.mjs$/];
const DESKTOP_SPECS = [/link-roles\.spec\.mjs$/, /desktop-layout\.spec\.mjs$/, /visibility\.spec\.mjs$/, /blank-frame\.spec\.mjs$/, /audio-activation\.spec\.mjs$/, /midi-activation\.spec\.mjs$/, /recording\.spec\.mjs$/];
const PAGES_SPECS = [/blank-frame\.spec\.mjs$/, /first-visit-race\.spec\.mjs$/];
// Measured on this machine's Playwright Chromium (audio-devices.spec.mjs's
// own header comment carries the numbers): a plain launch's
// `enumerateDevices()` reports three placeholder entries with both `label`
// and `deviceId` empty, and granting the `microphone` permission alone does
// not change that. Only launching Chromium with `--use-fake-device-for-media-
// stream` (`--use-fake-ui-for-media-stream` alongside it, for the one test
// that would otherwise hit a permission prompt) yields populated entries. That
// changes what every spec running under it measures, so it is scoped to this
// project alone rather than added to `desktop`, which every other desktop spec
// still runs under unchanged.
const AUDIO_DEVICE_SPECS = [/audio-devices\.spec\.mjs$/];

export default defineConfig({
  testDir: ".",
  fullyParallel: false,
  forbidOnly: !!process.env.CI,
  retries: process.env.CI ? 1 : 0,
  workers: 1,
  timeout: 60_000,
  expect: { timeout: 20_000 },
  use: {
    baseURL: `http://${host}:${port}`,
    trace: "on-first-retry",
  },
  webServer: [
    {
      // Serves the ALREADY-assembled dist/site (built + packaged by earlier
      // CI/local-chain steps) -- this config never rebuilds it.
      command: `node ../serve-site.mjs --port ${port} --host ${host} ../dist/site`,
      url: `http://${host}:${port}/index.html`,
      reuseExistingServer: !process.env.CI,
      timeout: 60_000,
    },
    {
      command: `node ../serve-site.mjs --port ${pagesPort} --host ${host} --no-isolation-headers ../dist/site`,
      url: `http://${host}:${pagesPort}/index.html`,
      reuseExistingServer: !process.env.CI,
      timeout: 60_000,
    },
  ],
  projects: [
    {
      name: "mobile",
      testMatch: MOBILE_SPECS,
      // iPhone-class viewport for the mobile-emulated e2e project.
      use: {
        ...devices["Desktop Chrome"],
        viewport: { width: 390, height: 844 },
        userAgent:
          "Mozilla/5.0 (iPhone; CPU iPhone OS 16_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.0 Mobile/15E148 Safari/604.1",
        isMobile: true,
        hasTouch: true,
      },
    },
    {
      name: "desktop",
      testMatch: DESKTOP_SPECS,
      use: {
        ...devices["Desktop Chrome"],
        viewport: { width: 1280, height: 800 },
      },
    },
    {
      // The deployed configuration: no isolation headers from the server, so
      // the page renders only if the service-worker shim still supplies them.
      name: "pages",
      testMatch: PAGES_SPECS,
      use: {
        ...devices["Desktop Chrome"],
        viewport: { width: 1280, height: 800 },
        baseURL: `http://${host}:${pagesPort}`,
      },
    },
    {
      // Fake capture devices only for this project's own spec -- see
      // AUDIO_DEVICE_SPECS above for why this cannot live on "desktop".
      name: "audio-devices",
      testMatch: AUDIO_DEVICE_SPECS,
      use: {
        ...devices["Desktop Chrome"],
        viewport: { width: 1280, height: 800 },
        launchOptions: {
          args: ["--use-fake-device-for-media-stream", "--use-fake-ui-for-media-stream"],
        },
      },
    },
  ],
});
