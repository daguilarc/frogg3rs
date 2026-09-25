#!/usr/bin/env node
// Assembles the frogg3rs browser build
// (app/browser/dist/wasm/apps/frogg3rs, produced by build-browser.sh) into
// a spec-conformant, content-addressed, self-hosted package + catalog.
//
// Governing specs:
//   openspec/specs/froggers-browser-package/spec.md
//     - "Schema-conformant catalog document"/"Permanent publisher identity":
//       appId "frogg3rs", publisher identity is this
//       project's own, never Sheaf's
//     - "Registry listing requires no Sheaf source change": built entirely out-of-tree
//     - "Immutable, content-addressed package" (build id derived from
//       artifact content)
//   External/Sheaf/openspec/specs/synth-browser-app-catalog/spec.md
//     - sbac-2: catalog schema, media types, SHA-256 digests
//     - sbac-3: global id is "<publisher-id>/<app-id>"
//     - sbac-7: CORS package sidecar materialization
//     - sbac-10: first-party catalog + package, local dev source
//
// DOES NOT REIMPLEMENT Sheaf's generic package/catalog tooling. Reused
// directly (read-only imports, no Sheaf source edited):
//   External/Sheaf/projects/synth/browser/src/package-contract.mjs
//     `assemblePackage` -- inventories the emission directory, validates
//     the 5 required artifact roles (entry/wasm/pthreadWorker/wasmWorker/
//     audioWorklet, aliasing allowed), computes each file's media type +
//     size + SHA-256, and derives `buildId` as a SHA-256 over every file's
//     {path, mediaType, size, sha256} plus the role mapping -- i.e. purely
//     a function of artifact content, exactly the "content-derived build
//     identifier" the spec requires. We do NOT recompute any of this by
//     hand.
//   External/Sheaf/projects/synth/browser/src/catalog.js (compiled from
//     catalog.ts) `parseCatalog` -- the schema-v1 validator
//     (External/Sheaf/projects/synth/browser/docs/catalog-schema-v1.md).
//     This *is* Sheaf's own catalog validator, preferred here over
//     reimplementing schema checks; parseCatalog is the same function
//     Sheaf's own first-party catalog builder
//     (Sheaf's browser/src/build-first-party-catalog.mjs) and its CatalogClient
//     (Sheaf's browser/src/catalog-client.ts) call.
//
// NOT reused: build-first-party-catalog.mjs / publishSite. Both are
// Sheaf's OWN first-party plumbing: they read Sheaf's own
// `first-party-apps.json` (schema requires header/cppType/includeDirs --
// an app-build manifest, not a catalog-publication concern) with
// `allowedSourceRoots` defaulted to a directory *inside* the Sheaf
// checkout, and `publishSite` additionally emits a full generic launcher
// site (index.html, rollback pages, runtime modules) that
// froggers-browser-package/spec.md's "Registry listing requires no Sheaf
// source change" + External/Sheaf/openspec/specs/synth-browser-app-catalog/spec.md's
// "Pages artifact excludes launcher authority" explicitly say we must NOT ship (GitHub Pages is a publisher-only
// catalog/package origin here, not a launcher). We are a third-party
// publisher assembling our OWN catalog; `assemblePackage` + `parseCatalog`
// are the generic building blocks meant for exactly that, and are used
// via the CLI-equivalent shape of package-app.mjs (same
// assemblePackage call, same argument shape) plus a hand-built catalog
// object validated with parseCatalog before being written.
//
// PUBLISHER IDENTITY (permanent once published -- spec "Permanent
// publisher identity", froggers-browser-package/spec.md):
// publisher.id = "daguilarc", publisher.name = "daguilarc".
//
// This script generates a fresh package and catalog under
// app/browser/dist/site/ (gitignored) from the current build on every
// deploy.
import { createHash } from "node:crypto";
import { cp, mkdir, mkdtemp, readdir, readFile, rename, rm, writeFile } from "node:fs/promises";
import path from "node:path";
import process from "node:process";
import { fileURLToPath } from "node:url";

const REPO_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "..");
const BROWSER_ROOT = path.join(REPO_ROOT, "app", "browser");
const SHEAF_BROWSER_ROOT = path.join(REPO_ROOT, "External", "Sheaf", "projects", "synth", "browser");
const SHEAF_BROWSER_DIST_SRC = path.join(SHEAF_BROWSER_ROOT, "dist", "src");
// The public site shell's own source (index.html + minimal
// css/js, app/browser/site/) -- staged verbatim into outputRoot below,
// alongside unmodified copies of Sheaf's own compiled browser runtime
// (dist/src/*.js -- the .mjs siblings in that directory are Node-only
// CLI/build tooling, e.g. package-app.mjs/publish-site.mjs/
// build-browser-apps.mjs, never fetched by a browser and deliberately
// excluded) and its public stylesheet (public/synth-browser.css). Nothing
// here reimplements or edits any Sheaf source; this is a copy step only.
const SITE_SOURCE_ROOT = path.join(BROWSER_ROOT, "site");
const SHEAF_BROWSER_PUBLIC = path.join(SHEAF_BROWSER_ROOT, "public");

const { assemblePackage } = await import(
  path.join(SHEAF_BROWSER_DIST_SRC, "package-contract.mjs")
);
const { parseCatalog } = await import(path.join(SHEAF_BROWSER_DIST_SRC, "catalog.js"));

// Permanent once published -- see header comment above.
const PUBLISHER = Object.freeze({ id: "daguilarc", name: "daguilarc" });
const APP_ID = "frogg3rs";
const APP_DISPLAY = Object.freeze({
  appId: APP_ID,
  displayName: "Frogg3rs Synth",
  author: "daguilarc",
  category: "synth",
});

const EMISSIONS_PATH = path.join(BROWSER_ROOT, "dist", "wasm", "apps", "emissions.json");

// Used only as the resolution base for local parseCatalog schema
// validation below; the catalog.json content itself carries no absolute
// URLs (every file/entry path is catalog-relative, per Sheaf's
// catalog-schema-v1.md, "Paths are normalized catalog-relative paths"),
// so this value does not get baked into any published artifact and does
// not decide, or need to match, the eventual production hosting origin
// (that origin/base-path is decided at cutover time, not fixed by this
// script).
// The exact string is arbitrary (any HTTPS or loopback URL satisfies
// parseCatalog's trustedCatalogUrl check) and never reaches published
// bytes -- it exists solely to give the validator something to resolve
// relative paths against, not to declare a real hosting location.
const CATALOG_VALIDATION_BASE_URL = "https://daguilarc.github.io/placeholder/catalogs/daguilarc/catalog.json";

// emissions.json's artifact paths are relative to dist/wasm/apps/ (e.g.
// "apps/frogg3rs/frogg3rs.js" -- see build-browser.sh's own rewrite step),
// but assemblePackage's sourceDirectory is the app's own dedicated emission
// directory (dist/wasm/apps/frogg3rs/) so it can assert "unexpected emitted
// artifacts not named by a required role" over exactly that directory's
// contents. Strip the "apps/<appId>/" prefix to get paths relative to
// sourceDirectory -- the same normalization Sheaf's
// build-first-party-catalog.mjs performs for the identical reason.
async function readArtifactRoles(appId) {
  const raw = JSON.parse(await readFile(EMISSIONS_PATH, "utf8"));
  if (!Array.isArray(raw.apps)) throw new Error(`emissions.json at ${EMISSIONS_PATH} has no apps array`);
  const app = raw.apps.find((entry) => entry && entry.appId === appId);
  if (!app) throw new Error(`emissions.json at ${EMISSIONS_PATH} has no entry for appId ${appId}`);
  if (!app.artifacts || typeof app.artifacts !== "object") {
    throw new Error(`emissions.json entry ${appId} has no artifacts object`);
  }
  const prefix = `apps/${appId}/`;
  const stripped = {};
  for (const [role, value] of Object.entries(app.artifacts)) {
    if (typeof value !== "string" || !value.startsWith(prefix)) {
      throw new Error(`emissions.json entry ${appId} artifacts.${role} (${String(value)}) is not within ${prefix}`);
    }
    stripped[role] = value.slice(prefix.length);
  }
  return stripped;
}

// Copies the site shell (app/browser/site/*, flat -- index.html, site.css,
// site-boot.mjs, catalog-sources.json) to the top of stagingRoot, plus
// verbatim runtime copies staged under ./sheaf-runtime/ (Sheaf's compiled
// browser JS, dist/src/*.js only) and ./sheaf-public/ (its stylesheet) --
// exactly the two directories site/index.html's <link>/<script> tags and
// site-boot.mjs's relative imports expect alongside it.
async function stageSiteShell(stagingRoot) {
  const siteEntries = await readdir(SITE_SOURCE_ROOT, { withFileTypes: true });
  await Promise.all(
    siteEntries
      .filter((entry) => entry.isFile())
      .map((entry) => cp(path.join(SITE_SOURCE_ROOT, entry.name), path.join(stagingRoot, entry.name))),
  );

  const runtimeDir = path.join(stagingRoot, "sheaf-runtime");
  await mkdir(runtimeDir, { recursive: true });
  const runtimeEntries = await readdir(SHEAF_BROWSER_DIST_SRC, { withFileTypes: true });
  await Promise.all(
    runtimeEntries
      .filter((entry) => entry.isFile() && entry.name.endsWith(".js"))
      .map((entry) => cp(path.join(SHEAF_BROWSER_DIST_SRC, entry.name), path.join(runtimeDir, entry.name))),
  );

  const publicDir = path.join(stagingRoot, "sheaf-public");
  await mkdir(publicDir, { recursive: true });
  await cp(
    path.join(SHEAF_BROWSER_PUBLIC, "synth-browser.css"),
    path.join(publicDir, "synth-browser.css"),
  );
}

function catalogVersionFor(publisherId, packageRecords) {
  const identities = packageRecords.map(({ appId, buildId }) => ({ appId, buildId }));
  const digest = createHash("sha256").update(JSON.stringify(identities)).digest("hex");
  return `${publisherId}-${digest}`;
}

/**
 * Assembles the immutable package (via Sheaf's generic assemblePackage)
 * and a schema-v1 catalog.json (validated via Sheaf's generic
 * parseCatalog) under outputRoot/catalogs/<publisher-id>/, atomically
 * replacing any previous contents of outputRoot.
 */
export async function assembleSite({
  outputRoot = path.join(BROWSER_ROOT, "dist", "site"),
  sourceDirectory = path.join(BROWSER_ROOT, "dist", "wasm", "apps", APP_ID),
  appId = APP_ID,
} = {}) {
  const artifacts = await readArtifactRoles(appId);

  await mkdir(path.dirname(outputRoot), { recursive: true });
  const stagingRoot = await mkdtemp(
    path.join(path.dirname(outputRoot), `.${path.basename(outputRoot)}.stage-`),
  );
  try {
    const catalogRoot = path.join(stagingRoot, "catalogs", PUBLISHER.id);
    await mkdir(catalogRoot, { recursive: true });

    const packageRecord = await assemblePackage({
      appId,
      sourceDirectory,
      outputDirectory: catalogRoot,
      artifacts,
    });

    const catalog = {
      schemaVersion: 1,
      catalogVersion: catalogVersionFor(PUBLISHER.id, [packageRecord]),
      publisher: PUBLISHER,
      apps: [{
        appId: APP_DISPLAY.appId,
        displayName: APP_DISPLAY.displayName,
        author: APP_DISPLAY.author,
        category: APP_DISPLAY.category,
        buildId: packageRecord.buildId,
        browser: packageRecord.browser,
      }],
    };

    // Throws on any schema violation -- this IS Sheaf's own validator,
    // not a hand-rolled check.
    parseCatalog(catalog, CATALOG_VALIDATION_BASE_URL);

    await writeFile(path.join(catalogRoot, "catalog.json"), `${JSON.stringify(catalog, null, 2)}\n`);

    // The public site shell (index.html + minimal css/js) plus
    // verbatim Sheaf runtime/stylesheet copies, staged alongside the
    // catalog+package output above.
    await stageSiteShell(stagingRoot);

    await rm(outputRoot, { recursive: true, force: true });
    await mkdir(path.dirname(outputRoot), { recursive: true });
    await rename(stagingRoot, outputRoot);
    return Object.freeze({ outputRoot, catalog: Object.freeze(catalog), packageRecord });
  } finally {
    await rm(stagingRoot, { recursive: true, force: true });
  }
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const { outputRoot, catalog, packageRecord } = await assembleSite();
  console.log(`Assembled frogg3rs site at ${path.relative(REPO_ROOT, outputRoot)}`);
  console.log(`  publisher:      ${catalog.publisher.id}`);
  console.log(`  appId:          ${packageRecord.appId}`);
  console.log(`  buildId:        ${packageRecord.buildId}`);
  console.log(`  catalogVersion: ${catalog.catalogVersion}`);
  console.log(`  files:          ${packageRecord.browser.files.length}`);
}
