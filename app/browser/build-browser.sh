#!/usr/bin/env bash
# Builds the frogg3rs browser (wasm) app through Sheaf's GENERIC browser
# pipeline (External/Sheaf/projects/synth/browser/src/build-browser-apps.mjs),
# out-of-tree: no Sheaf-side source changes.
#
# This cannot pass an out-of-tree
# `--output-root <repo>/app/browser/dist` directly. Empirically (and confirmed by
# reading build-browser-apps.mjs:147-152), that unconditionally throws
# "outputRoot must be a dedicated directory beneath dist/wasm": `wasmRoot`
# is always resolved from `browserRoot` (Sheaf's own
# projects/synth/browser/dist/wasm), independent of whatever --output-root
# value is given, so any path outside that tree is rejected before emcc
# ever runs. The Makefile's own fixture precedent
# (External/Sheaf/projects/synth/browser/Makefile:41,
# `--output-root dist/wasm/fixture-apps`) confirms this: it uses a path
# relative to and confined within browserRoot, never an out-of-tree one.
#
# The manifest and the app core are NOT the problem: with an in-tree
# --output-root this app compiles cleanly under emscripten 6.0.7 (verified
# 2026-08-18 -- the first-ever emscripten compile of this core, the
# std-only-portability probe, PASSED). So this script builds into an
# in-tree staging directory inside the Sheaf submodule (gitignored there:
# External/Sheaf/.gitignore:19) and then copies the two emitted artifacts
# plus the emissions report into this repo's
# app/browser/dist/wasm/apps/frogg3rs/{frogg3rs.js,frogg3rs.wasm}.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BROWSER_ROOT="$REPO_ROOT/External/Sheaf/projects/synth/browser"
MANIFEST="$REPO_ROOT/app/browser/frogg3rs-browser-apps.json"
STAGE_REL="dist/wasm/frogg3rs-out-of-tree-stage"
OUT_APPS_DIR="$REPO_ROOT/app/browser/dist/wasm/apps"
APP_ID="frogg3rs"

cd "$BROWSER_ROOT"

if [ ! -d node_modules ]; then
  npm install
fi
npm run build

node dist/src/build-browser-apps.mjs \
  --manifest "$MANIFEST" \
  --allowed-source-root "$REPO_ROOT/app" \
  --allowed-source-root "$REPO_ROOT/External/q/q_lib/include" \
  --allowed-source-root "$REPO_ROOT/External/infra/include" \
  --output-root "$STAGE_REL"

mkdir -p "$OUT_APPS_DIR"
rm -rf "$OUT_APPS_DIR/$APP_ID"
cp -R "$BROWSER_ROOT/$STAGE_REL/$APP_ID" "$OUT_APPS_DIR/$APP_ID"
cp "$BROWSER_ROOT/$STAGE_REL/emissions.json" "$OUT_APPS_DIR/emissions.json"

# The copied emissions.json still carries Sheaf's staging-relative artifact
# paths (STAGE_REL's basename, e.g. "frogg3rs-out-of-tree-stage/frogg3rs/
# frogg3rs.js") because it is a verbatim copy of the report Sheaf wrote for
# its OWN in-tree output root, not for the final app/browser/dist/ layout
# above. Downstream packaging reads this file, so rewrite the
# artifacts.{entry,wasm,pthreadWorker,wasmWorker,audioWorklet} fields to
# match the real final layout ("apps/<appId>/<appId>.{js,wasm}", relative
# to dist/wasm/ the same way Sheaf's own default-output-root convention
# reports it: build-browser-apps.mjs:175-184). Fail loudly (nonzero exit,
# uncaught exception) if the expected fields are missing rather than
# writing a half-patched report.
node -e '
const fs = require("fs");
const [file, appId] = process.argv.slice(1);
const data = JSON.parse(fs.readFileSync(file, "utf8"));
if (!Array.isArray(data.apps)) {
  throw new Error(`emissions.json at ${file} has no apps array`);
}
const app = data.apps.find((entry) => entry && entry.appId === appId);
if (!app) {
  throw new Error(`emissions.json at ${file} has no entry for appId ${appId}`);
}
if (!app.artifacts || typeof app.artifacts !== "object") {
  throw new Error(`emissions.json at ${file} entry ${appId} has no artifacts object`);
}
const required = ["entry", "wasm", "pthreadWorker", "wasmWorker", "audioWorklet"];
for (const key of required) {
  if (!(key in app.artifacts)) {
    throw new Error(`emissions.json at ${file} entry ${appId} is missing artifacts.${key}`);
  }
}
const entry = `apps/${appId}/${appId}.js`;
const wasm = `apps/${appId}/${appId}.wasm`;
app.artifacts.entry = entry;
app.artifacts.wasm = wasm;
app.artifacts.pthreadWorker = entry;
app.artifacts.wasmWorker = entry;
app.artifacts.audioWorklet = entry;
fs.writeFileSync(file, `${JSON.stringify(data, null, 2)}\n`);
' "$OUT_APPS_DIR/emissions.json" "$APP_ID"

echo "frogg3rs browser build complete:"
echo "  $OUT_APPS_DIR/$APP_ID/$APP_ID.js"
echo "  $OUT_APPS_DIR/$APP_ID/$APP_ID.wasm"
