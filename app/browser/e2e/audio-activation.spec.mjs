// Capture needs an `AudioContext` to attach to. `acquireInput`
// (External/Sheaf/projects/synth/browser/src/audio.ts) releases with
// `audioContextUnavailable` when `audioOptions.audioContext` is unset, before
// it ever reaches the browser's permission prompt -- so the Input device list
// is empty and Retry Input cannot help. The site supplies that context itself
// (site-boot.mjs), which is what this covers.
//
// The assertion is the ABSENCE of the old condition, not the presence of a
// working microphone. Headless Chromium never grants a real capture device, so
// "input reaches online" is not honestly assertable here; what is assertable is
// that activation no longer fails for the specific reason that no context was
// ever supplied. With a context present, `acquireInput` moves past that branch
// and fails, if it fails, for a permission-shaped reason instead.
//
// The text asserted on is the native runtime's own, recomputed from the numeric
// status code that crosses the wasm ABI (`BrowserAudioInputStatusText`,
// browser/BrowserAudioDevices.hpp) and rendered into the Audio page's
// status line (`NodeIds::kAudioStatusLine`, RuntimePages.hpp). The
// JS-side `"audio-context-unavailable"` diagnostic (`AudioInputStatusCode.audioContextUnavailable`, audio.ts) is
// internal `AudioBridge` state and is never rendered, so it is not a valid
// target.
import { expect, test } from "@playwright/test";
import {
  AUDIO_STATUS_LINE_SELECTOR,
  PLAY_SELECTOR,
  SIDEBAR_BUTTON_SELECTORS,
  waitForAudioOnline,
  waitForSurfaceReady,
} from "./helpers.mjs";

// The consent default is covered already: desktop-layout.spec.mjs asserts that
// nothing reaches `audio:online` without a click, which is the same property a
// second assertion here would restate. Supplying a context makes the choice
// reachable; it does not make the choice.
const AUDIO_CONTEXT_UNAVAILABLE_TEXT = "microphone requires the launch-owned AudioContext";

test.describe("audio activation", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/");
    await waitForSurfaceReady(page);
  });

  test("the Audio page does not report the missing-AudioContext condition after activation", async ({ page }) => {
    await page.locator(PLAY_SELECTOR).click();
    await waitForAudioOnline(page);
    await page.locator(SIDEBAR_BUTTON_SELECTORS[0]).click();
    await expect(page.locator(AUDIO_STATUS_LINE_SELECTOR)).not.toContainText(AUDIO_CONTEXT_UNAVAILABLE_TEXT);
  });
});
