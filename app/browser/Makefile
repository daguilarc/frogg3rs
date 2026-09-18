# Dedicated, CI-runnable build-level test for the frogg3rs browser (wasm)
# build. Deliberately separate from app/Makefile: `cd app && make test`
# (its own binaries' pass count, plus check_no_juce) must stay untouched by
# this group. Run this one explicitly via `cd app/browser && make test` (or
# `make -C app/browser test`).
.PHONY: build test package check-renamed-origin determinism-test package-test serve local-smoke

build:
	./build-browser.sh

test: build
	@test -s dist/wasm/apps/frogg3rs/frogg3rs.js || { echo "frogg3rs.js missing or empty" >&2; exit 1; }
	@test -s dist/wasm/apps/frogg3rs/frogg3rs.wasm || { echo "frogg3rs.wasm missing or empty" >&2; exit 1; }
	@echo "frogg3rs browser build-level test: OK ($$(wc -c < dist/wasm/apps/frogg3rs/frogg3rs.js) bytes .js, $$(wc -c < dist/wasm/apps/frogg3rs/frogg3rs.wasm) bytes .wasm)"

# --- Package + catalog + local validation (sibling targets;
# deliberately not folded into `test` above, which stays a pure build-level
# assertion per the header comment). ---

# Assembles dist/site/catalogs/daguilarc/{catalog.json,packages/...} from
# the already-built dist/wasm/apps/frogg3rs artifacts.
package:
	node package-catalog.mjs

# RENAMED-ORIGIN GATE (CI-runnable): fails if the assembled package+catalog
# output contains the retired app identity string "FroggersTiga".
check-renamed-origin: package
	./check-renamed-origin.sh dist/site

# Proves the content-addressed build id is a pure function of artifact
# bytes (same inputs -> same id; changed inputs -> changed id). Does not
# rebuild wasm; operates on copies of the existing dist/wasm/apps/frogg3rs
# output.
determinism-test:
	node test-package-determinism.mjs

# Full CI-runnable local proof for 4.1: determinism, package, renamed-origin
# gate, serve on loopback with CORS, and validate with Sheaf's own
# deployed-catalog validator (local-smoke.sh runs package + gate + serve +
# validate as one sequence).
package-test: determinism-test
	./local-smoke.sh

local-smoke: package-test

# Serves the assembled site on http://127.0.0.1:8787/ (catalog at
# /catalogs/daguilarc/catalog.json) with permissive CORS and correct media
# types. Foreground process; Ctrl-C to stop.
serve: package
	node serve-site.mjs --port 8787 dist/site
