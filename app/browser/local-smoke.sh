#!/usr/bin/env bash
# Automated local validation of the assembled catalog+package
# site. CI-runnable (no browser required for this part): packages,
# renamed-origin-gates, serves the site on loopback with CORS, and runs
# Sheaf's OWN deployed-catalog validator
# (External/Sheaf/projects/synth/browser/src/validate-deployed-catalog.mjs)
# against it, proving cross-origin access, media types, decoded size, and
# content hash all verify -- entirely over loopback HTTP, which the
# validator explicitly allows for local tests, inside its own
# `validateDeployedCatalog`.
#
# This does NOT drive a browser or Sheaf's real launcher UI -- for that
# manual step (Sheaf's launcher genuinely loading and running frogg3rs
# locally), see app/browser/dev-harness/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$ROOT/../.." && pwd)"
VALIDATOR="$REPO_ROOT/External/Sheaf/projects/synth/browser/dist/src/validate-deployed-catalog.mjs"
SITE_DIR="$ROOT/dist/site"
CATALOG_JSON="$SITE_DIR/catalogs/daguilarc/catalog.json"
HOST=127.0.0.1
PORT=8787

echo "== 1/4: package + catalog =="
node "$ROOT/package-catalog.mjs"

echo "== 2/4: renamed-origin gate =="
"$ROOT/check-renamed-origin.sh" "$SITE_DIR"

echo "== 3/4: serve on loopback with CORS =="
CATALOG_VERSION="$(node -e '
  const fs = require("node:fs");
  console.log(JSON.parse(fs.readFileSync(process.argv[1], "utf8")).catalogVersion);
' "$CATALOG_JSON")"

node "$ROOT/serve-site.mjs" --port "$PORT" --host "$HOST" "$SITE_DIR" &
SERVER_PID=$!
cleanup() { kill "$SERVER_PID" 2>/dev/null || true; wait "$SERVER_PID" 2>/dev/null || true; }
trap cleanup EXIT

for _ in $(seq 1 50); do
  if curl -sf "http://$HOST:$PORT/catalogs/daguilarc/catalog.json" >/dev/null 2>&1; then
    break
  fi
  sleep 0.1
done

echo "== 4/4: Sheaf's deployed-catalog validator (loopback) =="
node "$VALIDATOR" \
  --catalog-url "http://$HOST:$PORT/catalogs/daguilarc/catalog.json" \
  --expected-catalog-version "$CATALOG_VERSION"

echo "local smoke: OK"
