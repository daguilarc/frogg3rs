// Browser MIDI is requested only when the dispatched UI action is the
// sidebar Controllers action, not on every dispatched action the way audio
// activation is. `SynthBrowserApp` constructs its `BrowserMidiManager`
// unconditionally in its constructor, and its `startUserActivation` calls
// `BrowserMidiManager.startFromUserActivation()` only when the dispatched
// action's name matches its own mirror of `Actions::kSidebarControllers`;
// any other action leaves MIDI at its current status. At load, before any
// action dispatches, `startMidiIfAlreadyGranted` asks the Permissions API
// whether Web MIDI with sysex is already granted, and starts MIDI through
// the same manager when it is -- the saved-grant path the second test below
// exercises by granting the permission before the page loads. Either path
// renders into the root's `data-synth-status`. The lease branch in `start()`
// is a separate, eager alternative for a launcher that already owns a
// gesture; this page never takes it.
//
// A lease must NOT be introduced to make this pass: acquiring one asserts a
// user gesture this page has not had, which is what broke the site before.
//
// Why the permission is granted explicitly: the manager asks for
// `requestMIDIAccess({ sysex: true })`, so Chromium requires the
// `midi-sysex` permission, NOT `midi` alone -- granting only `midi` still
// yields `midi:offline`.
//
// The permission is necessary and not sufficient. Whether `requestMIDIAccess`
// resolves at all depends on the host having a MIDI backend behind it: macOS
// has CoreMIDI and resolves, a Linux CI runner has nothing and rejects, and the
// grant is accepted identically either way. So the run measures that capability
// in the page and asserts the outcome that is honest for the machine it is on,
// rather than treating a missing backend as a broken code path.
//
// Neither outcome says a controller is attached. `online` means the access was
// granted and the manager started; a physical controller against the published
// site is the only proof of the rest, and remains an operator check.
import { expect, test } from "@playwright/test";
import { PLAY_SELECTOR, SIDEBAR_BUTTON_SELECTORS, SYNTH_ROOT_SELECTOR, waitForSurfaceReady } from "./helpers.mjs";

const CONTROLLERS_SELECTOR = SIDEBAR_BUTTON_SELECTORS[1];

async function openSurface(page) {
  await page.goto("/");
  await waitForSurfaceReady(page);
}

function currentStatus(page) {
  return page.locator(SYNTH_ROOT_SELECTOR).getAttribute("data-synth-status");
}

async function expectStatusToContain(page, substring) {
  await expect.poll(() => currentStatus(page), { timeout: 20_000 }).toContain(substring);
}

// Counts real `navigator.requestMIDIAccess` calls, wrapped before any page
// script runs so it sees exactly what the app's own dispatch wiring and
// load-time saved-grant start each call.
async function installMidiRequestCounter(page) {
  await page.addInitScript(() => {
    window.__midiRequestCount = 0;
    if (typeof navigator.requestMIDIAccess !== "function") return;
    const original = navigator.requestMIDIAccess.bind(navigator);
    navigator.requestMIDIAccess = (options) => {
      window.__midiRequestCount += 1;
      return original(options);
    };
  });
}

function midiRequestCount(page) {
  return page.evaluate(() => window.__midiRequestCount);
}

// Does this browser have a MIDI backend behind the permission? Asked with the
// same call and the same sysex option the manager uses, so a `true` here means
// the manager's own request can resolve.
async function webMidiResolves(page) {
  return page.evaluate(async () => {
    if (typeof navigator.requestMIDIAccess !== "function") return false;
    try {
      await navigator.requestMIDIAccess({ sysex: true });
      return true;
    } catch {
      return false;
    }
  });
}

test.describe("browser MIDI", () => {
  test("reports a MIDI status after the first in-app action", async ({ page }) => {
    await installMidiRequestCounter(page);
    await openSurface(page);
    // Play is outside the Controllers page: it starts audio, and leaves MIDI
    // at its current (never-requested) status.
    await page.locator(PLAY_SELECTOR).click();
    await expectStatusToContain(page, "audio:");
    expect(await midiRequestCount(page)).toBe(0);
    // Opening Controllers is what requests MIDI.
    await page.locator(CONTROLLERS_SELECTOR).click();
    await expect.poll(() => midiRequestCount(page), { timeout: 20_000 }).toBe(1);
    await expectStatusToContain(page, "midi:");
  });

  test("reports the MIDI state its host can actually reach", async ({ page, context }) => {
    await context.grantPermissions(["midi", "midi-sysex"]);
    await openSurface(page);
    const resolves = await webMidiResolves(page);
    // The saved-grant start at load already brings MIDI up before any click;
    // this action only needs to start audio so the status has something to
    // report at all. The assertion below is on the state the whole sequence
    // above leaves the app in, not on which of the two paths produced it.
    await page.locator(PLAY_SELECTOR).click();
    // Where access resolves, a granted permission must carry through to
    // `online` -- a regression that drops the grant fails here. Where it does
    // not resolve, `offline` is the correct report and claiming otherwise would
    // be the defect.
    await expectStatusToContain(page, resolves ? "midi:online" : "midi:offline");
  });
});
