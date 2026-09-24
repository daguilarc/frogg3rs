// The Controllers page's Audio to MIDI section lists this app's MIDI-out
// contents exactly as its catalog declares them (app/FroggersMidiCatalog.hpp)
// -- Off (the runtime's own entry), then Level and Pitch in catalog order.
// Proves the browser build reaches the same catalog the standalone
// (app/FroggersMidiCatalogTests.cpp) and the plugin
// (app/vst/FroggersVstHostTests.cpp) already prove theirs does.
import { expect, test } from "@playwright/test";
import { SIDEBAR_BUTTON_SELECTORS, waitForSurfaceReady } from "./helpers.mjs";

const CONTROLLERS_SELECTOR = SIDEBAR_BUTTON_SELECTORS[1];
// synth::NodeIds::kAppMidiOutSends (ControllersPageUI.hpp); ui.ts's
// ComboBox backend nests one real <select> inside the node's own element,
// same convention as every other combo this suite reads (see
// controllers-persistence.spec.mjs's ADD_PRESET_SELECT).
const APP_MIDI_OUT_SENDS_SELECT = '[data-synth-node-id="runtime.controllers.app_midi_out.sends"] select';

test.describe("Controllers page Audio to MIDI Sends lists this app's catalog", () => {
  test("the Sends combo offers Off, Level and Pitch in that order", async ({ page }) => {
    await page.goto("/");
    await waitForSurfaceReady(page);
    await page.locator(CONTROLLERS_SELECTOR).click();

    const select = page.locator(APP_MIDI_OUT_SENDS_SELECT);
    await expect(select).toBeVisible();
    const optionValues = await select.locator("option").evaluateAll((options) => options.map((o) => o.value));
    expect(optionValues).toEqual(["off", "level", "pitch"]);
  });
});
