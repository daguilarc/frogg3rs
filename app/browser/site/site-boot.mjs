// Direct-load boot for the dedicated frogg3rs site. Mirrors
// Sheaf's own `launchCatalogApplication`
// (External/Sheaf/projects/synth/browser/src/main.ts, `launchCatalogApplication`) MINUS
// `SheafPatchLauncher`'s picker UI (`installSheafPatchLauncher`, never imported here):
// this page is dedicated to frogg3rs, so it loads the catalog, finds the
// one app, and boots it directly -- no catalog-browser list is ever
// rendered. Every imported symbol is Sheaf's own, unmodified, copied
// verbatim into ./sheaf-runtime/ by package-catalog.mjs's assembleSite();
// nothing here reimplements catalog loading, package verification (SHA-256
// digests -- package-loader.js's own `materializePackage`), or app
// bootstrapping (main.js's own `installSynthBrowserApp`).
//
// Supplies the `AudioContext` directly, through `audioOptions`, and passes
// no `activationLease`. The two are not interchangeable. A lease is a
// record that a user gesture has already happened: `ActivationLease.acquire`
// resumes its context and requests MIDI immediately (activation.ts), and a
// launcher that receives one starts audio and capture inside `start()`
// (main.ts). That is right for `SheafPatchLauncher`, whose lease is created
// inside its "Launch" button click. This page has no such click, so a lease
// acquired here would resume a context no one has interacted with -- which
// the browser will not complete -- and would claim an activation that has
// not occurred.
//
// Constructing a context is not starting one. A fresh `AudioContext` is
// suspended, and `startAudioWorklet` resumes the one it is given, so
// activation stays anchored to the first in-app UI action (main.ts's own
// `BrowserUiBackend` dispatch wiring calls `startUserActivation()` after
// every dispatched action) -- the app's own Play control, same as every
// other Sheaf host. No audio starts on load, for input or output:
// matches the e2e suite's "no audio start"
// requirement.
//
// The context has to come from somewhere, because microphone capture
// attaches to it specifically: `acquireInput` releases with
// `audioContextUnavailable` when `audioOptions.audioContext` is unset, so
// without this the Input device list is empty and Retry Input cannot help.
// Browser MIDI does not need a lease either: main.ts constructs
// `BrowserMidiManager` unconditionally in its constructor, but its dispatch
// wiring calls `BrowserMidiManager.startFromUserActivation()` only when the
// dispatched action is the sidebar Controllers action, not on every
// dispatched action the way it starts audio. At load, before any action
// dispatches, main.ts also asks the Permissions API whether Web MIDI with
// sysex is already granted, and starts MIDI through the same manager when
// it is, so a controller set up on an earlier visit works with no prompt.
// Either way MIDI is reachable without this page ever acquiring a lease.
//
// `launchCatalogApplication` also passes `runtimeClientFactory` and
// `frameIntervalMs`. Both are optional pass-throughs left undefined at
// Sheaf's own default call site, so omitting them here carries nothing.
import { CatalogClient } from "./sheaf-runtime/catalog-client.js";
import { runtimeIdentityForCatalogApp } from "./sheaf-runtime/catalog.js";
import { installSynthBrowserApp } from "./sheaf-runtime/main.js";
import { materializePackage } from "./sheaf-runtime/package-loader.js";
import { BrowserUiBackend } from "./sheaf-runtime/ui.js";
import { ANDROID_PHONE, IPHONE, classifyDevice } from "./device-class.mjs";
import { installViewportWidth } from "./viewport-width.mjs";

const APP_ID = "frogg3rs";

// Installs before the app boots so the very first render frame already
// reports the mount's width. See viewport-width.mjs's own header comment for
// the full mechanism.
installViewportWidth(BrowserUiBackend);

// Read once at module start: the user agent does not change over the page's
// lifetime, and both the footer treatment below and the wake lock wiring in
// boot() key off the same classification.
const deviceClass = classifyDevice();

// Footer: on an Android phone the desktop-app download link becomes the
// Android release link; on an iPhone there is no APK to offer, so the link
// and the separator right after it are removed outright. Every other device
// class (including desktop) leaves the footer exactly as index.html wrote
// it.
(function applyDeviceClassToFooter() {
  const downloadLink = document.querySelector('[data-site-link="download"]');
  if (!downloadLink) return;
  if (deviceClass === ANDROID_PHONE) {
    downloadLink.textContent = "Download Android app";
    downloadLink.href = "https://github.com/daguilarc/frogg3rs/releases/tag/frogg3rs_android";
  } else if (deviceClass === IPHONE) {
    const separator = downloadLink.nextElementSibling;
    downloadLink.remove();
    separator?.remove();
  }
})();

// Screen wake lock, held only while the AudioContext driving the app is
// actually running and the page is visible -- a phone left playing with the
// screen off is the whole point of the Android app, and a browser tab
// should behave the same way while it can (task 3.3). Re-evaluated on the
// context's own `statechange` and on the page's `visibilitychange`; every
// refusal (a denied permission, or `navigator.wakeLock` not existing at
// all, which throws synchronously on the property access below) is caught
// here so it never reaches the page's `unhandledrejection` backstop further
// down this file.
let wakeLockSentinel = null;

async function updateWakeLock(audioContext) {
  try {
    const shouldHold = audioContext.state === "running" && document.visibilityState === "visible";
    if (shouldHold && !wakeLockSentinel) {
      wakeLockSentinel = await navigator.wakeLock.request("screen");
      // The platform itself can release a held lock outside any event this
      // file listens for (e.g. the OS reclaiming it) -- forgetting the
      // stale sentinel here is what lets the next re-evaluation request a
      // fresh one instead of believing one is still held.
      wakeLockSentinel.addEventListener("release", () => {
        wakeLockSentinel = null;
      });
    } else if (!shouldHold && wakeLockSentinel) {
      const sentinel = wakeLockSentinel;
      wakeLockSentinel = null;
      await sentinel.release();
    }
  } catch {
    wakeLockSentinel = null;
  }
}

function wireScreenWakeLock(audioContext) {
  audioContext.addEventListener("statechange", () => void updateWakeLock(audioContext));
  document.addEventListener("visibilitychange", () => void updateWakeLock(audioContext));
}

// A failed boot must SAY so on the page. The data attribute alone left a
// visitor (and the operator, debugging remotely) staring at a blank frame
// between the header and footer with the reason readable only from the
// console or a DOM inspector.
function fail(root, error) {
  const message = error instanceof Error ? error.message : "frogg3rs failed to start";
  root.dataset.synthStatus = message;
  root.style.height = "";
  const notice = document.createElement("div");
  notice.className = "boot-error";
  notice.setAttribute("role", "alert");
  const heading = document.createElement("p");
  heading.className = "boot-error-heading";
  heading.textContent = "frogg3rs could not start in this browser.";
  const detail = document.createElement("p");
  detail.className = "boot-error-detail";
  detail.textContent = message;
  notice.append(heading, detail);
  root.replaceChildren(notice);
  console.error("frogg3rs boot failed", error);
}

async function boot(root) {
  const sourcesUrl = root.dataset.synthCatalogSources ?? "./catalog-sources.json";
  const client = new CatalogClient({ sourcesUrl });
  const result = await client.loadSources({ cacheMode: "default" });
  const app = result.apps.find((candidate) => candidate.appId === APP_ID);
  if (!app) {
    const detail = result.diagnostics
      .map((d) => `${d.catalogUrl}: ${d.status}${d.message ? ` (${d.message})` : ""}`)
      .join("; ");
    throw new Error(detail || `${APP_ID} is not present in the catalog`);
  }

  let materialized;
  const audioContext = new AudioContext();
  if (deviceClass === ANDROID_PHONE || deviceClass === IPHONE) wireScreenWakeLock(audioContext);
  try {
    materialized = await materializePackage(app);
    await installSynthBrowserApp(root, {
      module: materialized,
      runtimeVersions: {
        abiVersion: app.browser.abiVersion,
        uiProtocolVersion: app.browser.uiProtocolVersion,
        runtimeConfigVersion: app.browser.runtimeConfigVersion,
      },
      runtimeIdentity: runtimeIdentityForCatalogApp(app),
      audioOptions: { audioContext },
      disposeModule: () => materialized.dispose(),
    });
  } catch (error) {
    materialized?.dispose();
    void audioContext.close().catch(() => {});
    throw error;
  }
}

// Chromium (and other browsers) dispatch a genuine `window` `error` event
// for "ResizeObserver loop completed with undelivered notifications." /
// "ResizeObserver loop limit exceeded" -- a well-known, benign, spec-
// sanctioned notice (not a real thrown Error: `event.error` is null,
// `event.message` carries the text) that fires whenever a ResizeObserver
// callback itself triggers layout work that would need yet another
// observation in the same frame. This app has TWO independent
// ResizeObservers on the same `#synth-root` element -- Sheaf's own
// `fitSurface`-triggering one (ui.ts) and viewport-width.mjs's own
// (installViewportWidth's own comment on why) -- which is exactly the
// reactive-layout pattern that provokes this notice. Empirically confirmed
// harmless and frequent here (caught by this file's own e2e suite going red
// the moment the backstop below started treating it as fatal): DO NOT treat
// it as a boot failure.
const BENIGN_WINDOW_ERROR_MESSAGES = new Set([
  "ResizeObserver loop completed with undelivered notifications.",
  "ResizeObserver loop limit exceeded",
]);

const root = document.querySelector("#synth-root");
if (root) {
  // coi-serviceworker.js's page branch (loaded before this module,
  // classic script) may still owe an isolation attempt that ends in a
  // page reload -- see that file's own header comment. Booting, or
  // wiring up this module's own error backstop, before that attempt
  // settles is exactly the race that used to surface a false
  // "SharedArrayBuffer transfer requires self.crossOriginIsolated"
  // failure on a perfectly capable browser: the pthread pool would start
  // (and throw) before the reload it needed had a chance to land. When
  // the shim never ran at all (the server already sent real isolation
  // headers), `window.frogg3rsCoiAttempt` is never created, and awaiting
  // `undefined` resolves on the next microtask -- i.e. behaves exactly as
  // today.
  void (async () => {
    await window.frogg3rsCoiAttempt?.settled;

    void boot(root).catch((error) => fail(root, error));

    // Backstop for failures OUTSIDE boot()'s own promise chain (e.g. an
    // uncaught error from code that runs later, on its own timer/rAF/event
    // callback, never awaited by boot() itself). Does NOT double-fire for
    // boot()'s own errors: a promise with an attached .catch() (above) is
    // "handled" and never also raises `unhandledrejection`.
    //
    // Does NOT, and structurally cannot, catch a static import failure of
    // this module's OWN `./sheaf-runtime/*.js` imports at the top of this
    // file: per ES module semantics, a module's top-level code (including
    // these two addEventListener calls) never runs at all if any of its own
    // static imports fails to resolve, no matter where in the file a
    // listener is registered -- so a handler living in this file cannot be
    // the backstop for THIS file's own import failing. That specific case
    // (the exact scenario that produces a silent blank page) needs a
    // handler that does not depend on this module having loaded in the
    // first place; see index.html's own early inline <script>, registered
    // before this module's <script type="module"> tag, for that layer.
    window.addEventListener("error", (event) => {
      if (BENIGN_WINDOW_ERROR_MESSAGES.has(event.message)) return;
      fail(root, event.error instanceof Error ? event.error : new Error(event.message || "frogg3rs failed to start"));
    });
    window.addEventListener("unhandledrejection", (event) => {
      fail(root, event.reason);
    });
  })();
}
