// Reports the mount's own width, in CSS px, to the C++ surface -- nothing
// else. `synth_froggers::FroggersUiSurface` is now a
// `synth::ui::SelfSizedSurface` (app/FroggersUiSurface.hpp): given a narrow
// width it builds its OWN stacked tree (chrome block above the grid block,
// both spanning the reported width) and declares that tree's root bounds
// through `RootBounds()`, which `RuntimeMainComponent::BuildTree()` composes
// the whole runtime (including Sheaf's own sidebar, placed into the surface's
// declared slot) against. Sheaf's own generic `fitSurface()`
// (External/Sheaf/projects/synth/browser/src/ui.ts) then scales that WHOLE
// composite root down to fit the mount's real width, exactly as it already
// does for the desktop, wide-layout composite -- no per-block CSS transform,
// measurement, or mount `min-height` reservation is needed here any more,
// because there is no longer a fixed-aspect wide layout that a uniform
// scale-to-fit would visibly distort at a narrow width. This file replaces
// mobile-stack.mjs, which used to carry all of that now-unnecessary
// machinery.
//
// This file hooks `BrowserUiBackend`'s `renderFrame` method rather than
// polling on a timer: `renderFrame` already runs on every rendered frame
// (~33ms cadence), which is more than prompt enough for a width report the
// surface only reads on its next `BuildTree()`, and `this` inside the
// patched method is the `BrowserUiBackend` instance whose
// `dispatchBrowserAction` this reaches past that method's own public surface
// to call (the same reach `mobile-stack.mjs` used, since site-boot.mjs never
// gets a direct handle to the instance `installSynthBrowserApp` constructs
// internally -- `installViewportWidth` below patches the SHARED prototype,
// before any instance exists, for the same reason).

const MOUNT_SELECTOR = "#synth-root"; // this shell's own <main id="synth-root"> in app/browser/site/index.html

// FroggersActions::kViewportWidth (FroggersUiSurface.hpp,
// `"froggers.viewport.width"`) -- the C++ surface's own reported-width
// signal, consumed by `HandleAction` to set `viewportWidth_`.
const VIEWPORT_WIDTH_ACTION = "froggers.viewport.width";

// Last width actually dispatched, so an unchanged width is never
// redispatched on every single frame while sitting still at one size.
// `null` so the very first frame always dispatches once.
let lastDispatchedWidth = null;

// The most recent `BrowserUiBackend` instance seen through the patched
// `renderFrame` below -- there is exactly one live instance at a time (one
// app boots per page), and `installViewportWidth` patches the shared
// prototype before any instance exists, so this is the only way a
// ResizeObserver callback (which carries no `this` of its own) can reach
// `dispatchBrowserAction`. `null` until the first frame renders; a resize
// that fires before then is still caught by that first frame's own report.
let lastBackend = null;

// Reports the mount's current width, off the exact same `clientWidth` Sheaf's
// own `fitSurface()` reads to scale the composite root -- never a second,
// independently measured number.
function dispatchViewportWidth(backend, mount) {
  const width = mount.clientWidth;
  if (width === lastDispatchedWidth) return;
  lastDispatchedWidth = width;
  backend.dispatchBrowserAction({ name: VIEWPORT_WIDTH_ACTION, value: String(width) });
}

/**
 * Wires a viewport-width report to run after every render frame and
 * immediately on resize, rather than waiting for the next one. Call once,
 * before booting the app -- see this file's own header comment for why the
 * shared prototype is patched rather than a specific instance.
 */
export function installViewportWidth(BrowserUiBackend) {
  const originalRenderFrame = BrowserUiBackend.prototype.renderFrame;
  BrowserUiBackend.prototype.renderFrame = function patchedRenderFrame(...args) {
    const result = originalRenderFrame.apply(this, args);
    lastBackend = this;
    const mount = document.querySelector(MOUNT_SELECTOR);
    if (mount) dispatchViewportWidth(this, mount);
    return result;
  };

  const mount = document.querySelector(MOUNT_SELECTOR);
  if (mount) {
    const resizeObserver = new ResizeObserver(() => {
      if (lastBackend) dispatchViewportWidth(lastBackend, mount);
    });
    resizeObserver.observe(mount);
  }
  // Kept as a belt-and-suspenders trigger (orientation change on some
  // engines resizes the layout viewport without necessarily resizing the
  // mount element on the very same tick the OS event fires) -- cheap and
  // idempotent (dispatchViewportWidth is itself a no-op when the width has
  // not actually changed).
  window.addEventListener("orientationchange", () => {
    const current = document.querySelector(MOUNT_SELECTOR);
    if (current && lastBackend) dispatchViewportWidth(lastBackend, current);
  });
}
