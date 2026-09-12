// Recording reaches the browser's own save flow. The capture buffer fills
// inside the AudioWorklet's process callback while the runtime is armed
// (main.ts's `offerDownload`, called once the runtime hands back the
// recorded bytes), the same worklet callback whose block counts Sheaf's own
// audio-flow.spec.ts already asserts render
// (External/Sheaf/projects/synth/browser/tests/audio-flow.spec.ts:904 and :987)
// -- this spec depends on
// that same rendering happening under Playwright's Chromium, and treats a
// downloaded, non-trivial RIFF/WAV file as proof the capture ran end to end
// rather than re-deriving worklet block counts itself.
//
// The refusal case is the negative control: with the transport never
// started, Record must show the app's own refusal text
// (FroggersAppCore.hpp's `kRecordRefusalReason`,
// FroggersUiSurface.hpp:2264's `transportNotice_ = app_->RecordRefusalReason()`)
// inside the left block rather than silently doing nothing, and the notice
// must clear once Play actually starts the transport.
import { expect, test } from "@playwright/test";
import { LEFT_BLOCK_SELECTOR, PLAY_SELECTOR, waitForSurfaceReady } from "./helpers.mjs";

const RECORD_SELECTOR = '[data-synth-node-id="froggers.transport.record"]';
const NOTICE_SELECTOR = '[data-synth-node-id="froggers.transport.notice"]';

function todayFileName() {
  const now = new Date();
  const year = now.getFullYear();
  const month = String(now.getMonth() + 1).padStart(2, "0");
  const day = String(now.getDate()).padStart(2, "0");
  return `${year}-${month}-${day}.wav`;
}

test.describe("recording", () => {
  test("a stopped recording downloads under today's date", async ({ page }) => {
    await page.goto("/");
    await waitForSurfaceReady(page);
    await page.locator(PLAY_SELECTOR).click();
    await page.locator(RECORD_SELECTOR).click();
    await page.waitForTimeout(1000);
    const downloadPromise = page.waitForEvent("download");
    await page.locator(RECORD_SELECTOR).click();
    const download = await downloadPromise;

    expect(download.suggestedFilename()).toBe(todayFileName());

    const filePath = await download.path();
    const fs = await import("node:fs/promises");
    const buffer = await fs.readFile(filePath);
    expect(buffer.length).toBeGreaterThan(44);
    expect(buffer.subarray(0, 4).toString("ascii")).toBe("RIFF");
  });

  test("Record with the transport stopped shows the notice", async ({ page }) => {
    await page.goto("/");
    await waitForSurfaceReady(page);
    await page.locator(RECORD_SELECTOR).click();

    const notice = page.locator(NOTICE_SELECTOR);
    await expect(notice).toHaveText("Press Play before recording.");

    const [noticeBox, leftBlockBox] = await Promise.all([
      notice.boundingBox(),
      page.locator(LEFT_BLOCK_SELECTOR).boundingBox(),
    ]);
    expect(noticeBox).not.toBeNull();
    expect(leftBlockBox).not.toBeNull();
    expect(noticeBox.x).toBeGreaterThanOrEqual(leftBlockBox.x);
    expect(noticeBox.y).toBeGreaterThanOrEqual(leftBlockBox.y);
    expect(noticeBox.x + noticeBox.width).toBeLessThanOrEqual(leftBlockBox.x + leftBlockBox.width);
    expect(noticeBox.y + noticeBox.height).toBeLessThanOrEqual(leftBlockBox.y + leftBlockBox.height);

    await page.locator(PLAY_SELECTOR).click();
    await expect(notice).toHaveCount(0);
  });
});
