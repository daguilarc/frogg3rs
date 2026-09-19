// Controllers-page setup and File-page patches both persist through the
// browser build's own IndexedDB-backed data root (mounted at "/data" by
// External/Sheaf/projects/synth/browser/src/persistence.ts's
// BrowserPersistence, synced via Emscripten's IDBFS): every Controllers-page
// Commit() writes a fresh runtime configuration file, and every confirmed
// patch save/load writes or reads a patch directory, under that same mount.
// This walks the page exactly as an operator's browser tab would --
// reloading between steps -- to prove that setup and those patches actually
// survive a real reload, not just an in-memory session.
//
// Runs under the "desktop" project only (playwright.config.mjs testMatch),
// alongside the other Controllers/File-adjacent specs. Each test() gets its
// own fresh, isolated BrowserContext (no storageState configured anywhere in
// this project), so each one starts from an empty Controllers page and an
// empty patch list with nothing to clear first.
import { expect, test } from "@playwright/test";
import { SIDEBAR_BUTTON_SELECTORS, SYNTH_ROOT_SELECTOR, waitForSurfaceReady } from "./helpers.mjs";

const CONTROLLERS_SELECTOR = SIDEBAR_BUTTON_SELECTORS[1];
const FILE_SELECTOR = SIDEBAR_BUTTON_SELECTORS[3];

// NodeIds::kAddPreset / kAddButton (ControllersPageUI.hpp). The combo's
// "custom" option value is NodeIds::kCustomPresetOptionId; ui.ts's ComboBox
// backend renders one real <select> per combo node, each <option>'s value
// set to the option's id (`new Option(option.label, option.id)`).
const ADD_PRESET_SELECT = '[data-synth-node-id="runtime.controllers.add_preset"] select';
const ADD_BUTTON = '[data-synth-node-id="runtime.controllers.add_button"]';

// MidiInstrumentConfig::AddController always appends, so on a fresh (empty)
// instrument the first row Add creates is always index 0 -- NodeIds::
// ControllerRow(0)/ControllerName(0) (ControllersPageUI.hpp). A Custom row's
// three sections (Encoders/SystemMessages/Analogs) all start expanded
// (HandleAddController's OpenAddedRow), so no disclosure/section-toggle
// click is needed before reaching its mapping controls.
const FIRST_ROW_NAME = '[data-synth-node-id="runtime.controllers.row.0.name"]';
// MidiConfigSection::SystemMessages == 1 (MidiConfigViewModel.hpp); its one
// addable group is header 0, "System" (ControllersLayout::AddableGroups) --
// the only section with no sibling group to disambiguate, so it is the
// simplest single mapping entry to add and check for.
// NodeIds::GroupAddSingle(0, SystemMessages, 0) / MappingRow(0, SystemMessages, 0).
const FIRST_ROW_SYSTEM_ADD =
  '[data-synth-node-id="runtime.controllers.row.0.section.1.body.header.0.add_single"]';
const FIRST_ROW_SYSTEM_ENTRY = '[data-synth-node-id="runtime.controllers.row.0.section.1.body.mapping.0"]';
// Which sections/rows are expanded (MidiControllerRowVM::configExpanded /
// MidiConfigViewModel::SectionExpanded) is view-model UI state, never part
// of the persisted MidiInstrumentConfig -- it starts collapsed on every
// fresh load (configExpanded's own "starts false" comment,
// MidiConfigViewModel.hpp) except immediately after Add, whose
// OpenAddedRow explicitly opens it. A reload always lands on the collapsed
// default, so re-opening a row already on the page needs both of its own
// toggles: NodeIds::ControllerDisclosure(0) and NodeIds::SectionToggle(0,
// SystemMessages). Neither dirties the runtime configuration, so neither
// needs commitAndAwaitPersistence.
const FIRST_ROW_DISCLOSURE = '[data-synth-node-id="runtime.controllers.row.0.disclosure"]';
const FIRST_ROW_SYSTEM_TOGGLE = '[data-synth-node-id="runtime.controllers.row.0.section.1.toggle"]';

// NodeIds::kFileSave/kFileBrowserSaveName/kFileBrowserList/kFileBrowserConfirm/
// kFilePatchName (RuntimePages.hpp). Clicking Save with no patch open yet
// (FilePageSurface::DispatchAction's `kFileSave && !hasCurrentPatch` branch)
// opens the same Save-As browser kFileSaveAs would.
const FILE_SAVE_BUTTON = '[data-synth-node-id="runtime.file.save"]';
const FILE_LOAD_BUTTON = '[data-synth-node-id="runtime.file.load"]';
const FILE_SAVE_NAME_INPUT = '[data-synth-node-id="runtime.file.browser.save_name"] input';
const FILE_BROWSER_LIST = '[data-synth-node-id="runtime.file.browser.list"]';
const FILE_BROWSER_CONFIRM = '[data-synth-node-id="runtime.file.browser.confirm"]';
const FILE_PATCH_NAME = '[data-synth-node-id="runtime.file.patch_name"]';

async function openSurface(page) {
  await page.goto("/");
  await waitForSurfaceReady(page);
}

function persistenceStatus(page) {
  return page.locator(SYNTH_ROOT_SELECTOR).getAttribute("data-synth-status");
}

/**
 * Every Controllers/File-page commit that touches the runtime configuration
 * or a patch file marks the mounted "/data" filesystem dirty; worker.ts's
 * `syncPersistenceIfRuntimeDirty` then debounces one `IDBFS.syncfs(false,
 * ...)` (persistence.ts's `scheduleSync`/`flush`, 100ms by default) and
 * reports "persistence pending" then "persistence succeeded" into the same
 * `data-synth-status` attribute audio/MIDI status already use
 * (main.ts's `renderStatus`). A `page.reload()` issued before that debounced
 * flush lands would lose the edit even though the UI already shows it, so
 * every mutating step below runs through this instead of a bare click,
 * giving the debounced flush a chance to land before anything reloads.
 *
 * This never fails the test itself on a stuck status: whether the flush
 * actually reached IndexedDB is exactly what the walk's own post-reload
 * assertions check, and a commit whose save is broken (ControllersPageSurface
 * ::Commit's `saveRuntimeConfiguration` call) never marks anything dirty at
 * all, so the status would otherwise hang here until the timeout on every
 * single step instead of surfacing once, at the reload, as a missing row.
 */
async function commitAndAwaitPersistence(page, act) {
  const before = await persistenceStatus(page);
  await act();
  const deadline = Date.now() + 10_000;
  while (Date.now() < deadline) {
    const status = await persistenceStatus(page);
    if (status !== before && status === "persistence succeeded") return;
    await page.waitForTimeout(50);
  }
}

async function addCustomController(page) {
  await page.locator(CONTROLLERS_SELECTOR).click();
  // Positive control for "index 0 is the row Add is about to create": no
  // controller row exists yet on this fresh instrument.
  await expect(page.locator(FIRST_ROW_NAME)).toHaveCount(0);
  await commitAndAwaitPersistence(page, async () => {
    await page.locator(ADD_PRESET_SELECT).selectOption("custom");
    await page.locator(ADD_BUTTON).click();
  });
  await expect(page.locator(FIRST_ROW_NAME)).toHaveText("Custom");
}

async function addSystemMappingEntry(page) {
  await commitAndAwaitPersistence(page, () => page.locator(FIRST_ROW_SYSTEM_ADD).click());
  await expect(page.locator(FIRST_ROW_SYSTEM_ENTRY)).toBeVisible();
}

async function savePatchAs(page, name) {
  await page.locator(FILE_SELECTOR).click();
  await commitAndAwaitPersistence(page, async () => {
    await page.locator(FILE_SAVE_BUTTON).click();
    await page.locator(FILE_SAVE_NAME_INPUT).fill(name);
    await page.locator(FILE_BROWSER_CONFIRM).click();
  });
  await expect(page.locator(FILE_PATCH_NAME)).toHaveText(name);
}

async function openPatch(page, name) {
  await page.locator(FILE_SELECTOR).click();
  await commitAndAwaitPersistence(page, async () => {
    await page.locator(FILE_LOAD_BUTTON).click();
    // FilePageBrowser::Confirm's Load branch requires a selected entry
    // (browser_.SelectedLoadPath()): a single click selects
    // (kFileBrowserSelect) without loading, so it must run before Confirm.
    await page.locator(FILE_BROWSER_LIST).getByText(name, { exact: true }).click();
    await page.locator(FILE_BROWSER_CONFIRM).click();
  });
  await expect(page.locator(FILE_PATCH_NAME)).toHaveText(name);
}

async function reloadAndOpenControllers(page) {
  await page.reload();
  await waitForSurfaceReady(page);
  await page.locator(CONTROLLERS_SELECTOR).click();
}

async function expandFirstRowSystemSection(page) {
  await page.locator(FIRST_ROW_DISCLOSURE).click();
  await page.locator(FIRST_ROW_SYSTEM_TOGGLE).click();
}

test.describe("Controllers page setup and patches survive a reload", () => {
  test("adding a Custom controller and one mapping entry survives a reload", async ({ page }) => {
    await openSurface(page);
    await addCustomController(page);
    await addSystemMappingEntry(page);

    await reloadAndOpenControllers(page);
    await expect(page.locator(FIRST_ROW_NAME)).toHaveText("Custom");
    await expandFirstRowSystemSection(page);
    await expect(page.locator(FIRST_ROW_SYSTEM_ENTRY)).toBeVisible();
  });

  test("a Custom controller added after saving a patch survives a reload", async ({ page }) => {
    await openSurface(page);
    await savePatchAs(page, "base-patch");
    await page.locator(CONTROLLERS_SELECTOR).click();
    await addCustomController(page);

    await reloadAndOpenControllers(page);
    await expect(page.locator(FIRST_ROW_NAME)).toHaveText("Custom");
  });

  test("opening a saved patch drops a Custom controller added afterward, across a reload", async ({ page }) => {
    await openSurface(page);
    await savePatchAs(page, "patch-p");
    await page.locator(CONTROLLERS_SELECTOR).click();
    await addCustomController(page);

    await openPatch(page, "patch-p");

    await reloadAndOpenControllers(page);
    await expect(page.locator(FIRST_ROW_NAME)).toHaveCount(0);
  });
});
