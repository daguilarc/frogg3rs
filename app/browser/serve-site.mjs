#!/usr/bin/env node
// Local serving path. Serves one or
// more directories on loopback HTTP with permissive CORS and correct
// media types (wasm -> application/wasm, js -> text/javascript), matching
// sbac-7 (External/Sheaf/openspec/specs/synth-browser-app-catalog/spec.md:95-106)
// and the froggers-browser-package spec's "Public hosting
// suitable for cross-origin loading" requirement.
//
// Reuses Sheaf's generic `contentTypeForPath`
// (External/Sheaf/projects/synth/browser/src/static-server.mjs) verbatim
// for the media-type mapping -- not reimplemented. Sheaf's
// `createStaticServer` itself is NOT reused as-is: its static roots and
// published-site root are hardcoded relative to Sheaf's OWN browser/
// checkout (dist/, packages/, public/ siblings of that file), so it can't
// serve an arbitrary directory in this repo without forking it; only its
// exported, generic `contentTypeForPath` travels cleanly.
//
// Usage:
//   node serve-site.mjs [--port N] [--host H] [MOUNT=DIR ...] [DEFAULT_DIR]
//
// A bare DEFAULT_DIR (no "=") is served at "/". Each "PREFIX=DIR" mounts
// DIR at "/PREFIX/". Longest prefix wins. With no arguments, serves
// dist/site at "/".
import { createReadStream } from "node:fs";
import { stat } from "node:fs/promises";
import { createServer } from "node:http";
import path from "node:path";
import process from "node:process";
import { fileURLToPath } from "node:url";

const REPO_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "..");
const BROWSER_ROOT = path.join(REPO_ROOT, "app", "browser");
const SHEAF_BROWSER_DIST_SRC = path.join(
  REPO_ROOT, "External", "Sheaf", "projects", "synth", "browser", "dist", "src",
);

const { contentTypeForPath } = await import(path.join(SHEAF_BROWSER_DIST_SRC, "static-server.mjs"));

function parseArguments(argv) {
  let isolationHeaders = true;
  let port = 8787;
  let host = "127.0.0.1";
  const mounts = [];
  let defaultDir;
  for (let index = 0; index < argv.length; index += 1) {
    const value = argv[index];
    if (value === "--port") {
      const raw = argv[++index];
      const parsed = Number(raw);
      if (!Number.isInteger(parsed) || parsed < 0 || parsed > 65535) {
        throw new Error(`--port must be an integer 0-65535, got ${JSON.stringify(raw)}`);
      }
      port = parsed;
      continue;
    }
    if (value === "--host") { host = argv[++index]; continue; }
    // Serves without the isolation headers, the way GitHub Pages does -- it
    // cannot set custom headers at all. The site's service-worker shim is the
    // only thing that isolates the page there, and a suite that always runs
    // against the headers below can never exercise that path.
    if (value === "--no-isolation-headers") { isolationHeaders = false; continue; }
    const equals = value.indexOf("=");
    if (equals > 0) {
      mounts.push({ prefix: `/${value.slice(0, equals).replace(/^\/+|\/+$/g, "")}/`, root: path.resolve(value.slice(equals + 1)) });
    } else if (defaultDir === undefined) {
      defaultDir = path.resolve(value);
    } else {
      throw new Error(`unexpected argument ${value}`);
    }
  }
  if (defaultDir === undefined) defaultDir = path.join(BROWSER_ROOT, "dist", "site");
  mounts.sort((left, right) => right.prefix.length - left.prefix.length);
  return { port, host, mounts, defaultDir, isolationHeaders };
}

function resolveFile(pathname, root, prefix = "/") {
  let relative = pathname.slice(prefix.length);
  if (relative === "" || relative.endsWith("/")) relative += "index.html";
  if (relative.includes("\0") || relative.split("/").includes("..")) return undefined;
  const filename = path.resolve(root, relative);
  if (filename !== root && !filename.startsWith(`${root}${path.sep}`)) return undefined;
  return filename;
}

function fileFor(requestUrl, { mounts, defaultDir }) {
  let pathname;
  try {
    pathname = decodeURIComponent(new URL(requestUrl ?? "/", "http://localhost").pathname);
  } catch {
    return undefined;
  }
  for (const { prefix, root } of mounts) {
    // A request for the mount's bare prefix with no trailing slash (e.g.
    // "/catalogs/daguilarc") must match too, not just
    // "/catalogs/daguilarc/" -- resolveFile's own empty-relative-path
    // handling already treats that the same as the slash form (falls
    // through to index.html).
    if (pathname === prefix.slice(0, -1) || pathname.startsWith(prefix)) return resolveFile(pathname, root, prefix);
  }
  return resolveFile(pathname, defaultDir, "/");
}

export function createSiteServer(config) {
  return createServer(async (request, response) => {
    // Permissive CORS (sbac-7) PLUS the cross-origin-isolated launcher's
    // COOP/COEP/Permissions-Policy trio (mirroring Sheaf's own
    // static-server.mjs `isolated: true` default, static-server.mjs
    // :134-138). The comment this replaced argued this server must NOT
    // claim isolation headers because "this repo is a catalog/package
    // origin, not the launcher" (sbac-11) -- true when this script's only
    // job was local catalog+package validation, but the site
    // shell (index.html + site-boot.mjs) now boots frogg3rs directly, i.e.
    // THIS origin is also the launcher for its own published site. That is
    // not optional: frogg3rs.js is compiled with Emscripten pthreads
    // (confirmed present at app/browser/dist/wasm/apps/frogg3rs/
    // frogg3rs.js) and unconditionally starts its pthread pool on module
    // load (not just on audio start) -- with no crossOriginIsolated
    // context, that pool creation itself throws
    // (`DataCloneError: ... SharedArrayBuffer transfer requires
    // self.crossOriginIsolated`) before any UI renders, empirically
    // confirmed serving this same dist/site output without these headers.
    // CORS stays exactly as before: COOP/COEP
    // govern how *this* origin's own documents/navigations behave, not who
    // may fetch the catalog/package JSON and binaries cross-origin, so a
    // third-party launcher consuming this origin's catalog is unaffected.
    // `no-store` because this server exists to look at a freshly rebuilt
    // dist/site: without it the browser keeps the previous build's ES
    // modules and serves a MIX of cached and fresh ones after a repackage,
    // which fails the boot outright -- a confirmed real failure mode: page header
    // and footer rendered, surface never did, and the stale layout module
    // flickered on resize. Production hosting sets its own caching; this
    // header is a development-server concern only.
    const headers = {
      "Access-Control-Allow-Origin": "*",
      "Cache-Control": "no-store",
      ...(config.isolationHeaders === false ? {} : {
        "Cross-Origin-Resource-Policy": "cross-origin",
        "Cross-Origin-Opener-Policy": "same-origin",
        "Cross-Origin-Embedder-Policy": "require-corp",
      }),
      "Permissions-Policy": "midi=(self), microphone=(self)",
    };
    if (request.method === "OPTIONS") { response.writeHead(204, headers).end(); return; }
    const filename = fileFor(request.url, config);
    if (!filename) { response.writeHead(404, headers).end(); return; }
    try {
      if (!(await stat(filename)).isFile()) throw new Error("not a file");
      response.writeHead(200, { ...headers, "Content-Type": contentTypeForPath(filename) });
      createReadStream(filename).pipe(response);
    } catch {
      response.writeHead(404, headers).end();
    }
  });
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const config = parseArguments(process.argv.slice(2));
  const server = createSiteServer(config);
  server.listen(config.port, config.host, () => {
    console.log(`serving http://${config.host}:${config.port}/  ->  ${path.relative(REPO_ROOT, config.defaultDir)}`);
    for (const { prefix, root } of config.mounts) {
      console.log(`serving http://${config.host}:${config.port}${prefix}  ->  ${path.relative(REPO_ROOT, root)}`);
    }
  });
}
