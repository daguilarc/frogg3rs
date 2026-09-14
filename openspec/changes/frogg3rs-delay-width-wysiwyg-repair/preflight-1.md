# Preflight 1 — opening §8.0 hygiene sweep

Confirmed: both HELD directories are untracked (`??`), consistent with the proposal's description. No changes were made to the working tree at any point in this sweep (read-only throughout).

## Opening Hygiene Sweep (§8.0) — `frogg3rs-delay-width-wysiwyg-repair`

### Tree 1: `app/` — searched for uninvoked scripts/gates, dead headers, stale test-count claims

- Enumerated every `app/check_*.py` (7) and every `.sh`/`.py`/`.cpp`-gate script (11 total, plus `app/browser/`'s scripts and `.mjs` e2e specs). Second pass by bare-name grep (not path) across the whole tree, excluding build/vendor dirs.
- All resolve to an invocation: `check_no_juce.cpp`, `check_no_firmware_includes.sh`, `check_microphone_usage_strings.sh`, `check_docs_match_parameter_table.py`, `check_spec_checks_resolve.py`, `check_citations_resolve.py`, `check_modified_requirements_restate_promoted.py`, `check_no_planning_history.py`, `check_artifact_symbols_resolve.py`, `check_catalog_covers_screen_actions.sh` — all invoked from `app/Makefile` (cited line numbers in the transcript above). `check_common.py` is invoked indirectly, imported by `check_no_planning_history.py` and `check_citations_resolve.py`.
- `app/browser/build-browser.sh`, `check-renamed-origin.sh`, `local-smoke.sh`, `package-catalog.mjs`, `test-package-determinism.mjs`, and the `app/browser/e2e/*.spec.mjs` suite — all invoked from `app/browser/Makefile` and/or `.github/workflows/pages.yml`.
- `app/build-launcher.sh` — invoked from `.github/workflows/desktop-release.yml` and named in `README.md`/`app/README.md`.
- Checked four more `.hpp` headers for orphaning (`FroggersRandomShVisualizer.hpp`, `FroggersTransferFunctionVisualizer.hpp`, `FroggersBundledDocs.hpp`, `FroggersRegistration.hpp`) — all have live consumers.
- `app/README.md`'s claim of twelve test binaries verified against `app/Makefile`'s `test:` target and the twelve `*Tests.cpp` files on disk — matches, nothing needed.
- **Disposition: 0 findings needing change in `app/`.**

### Tree 2: `openspec/specs/`

- All 20 spec directories enumerated. Resolved every `Check:` citation in `froggers-sheaf-parameter-model/spec.md` (13 lines) against `app/FroggersDspParityTests.cpp`, `app/FroggersModulationTests.cpp`, `app/check_docs_match_parameter_table.py` — all resolve.
- The two `NOTE (pending frogg3rs-delay-width-wysiwyg-repair delivery)` transitional pointers (lines 119, 579) correctly name this in-flight change; not stale while it's still open.
- **Disposition: 0 findings needing change.**

### Tree 3: `openspec/changes/` (including `archive/`)

- Listed all hidden files (`find -name ".*"`) since a glob on `openspec/*` misses them: found `openspec/.sessions/` (see finding #1 below) and every archived change's `.openspec.yaml`; no other hidden files.
- Enumerated the 3 non-archive change directories: `frogg3rs-delay-width-wysiwyg-repair` (this change), and the two HELD ones.
- Resolved every `Check:` citation (28 distinct test names) in this change's own delta spec (`.../frogg3rs-delay-width-wysiwyg-repair/specs/froggers-sheaf-parameter-model/spec.md`) against `app/FroggersDspParityTests.cpp` — all resolve.
- Verified `tasks.md` and the proposal's cited research doc (`archive/2026-08-18-frogg3rs-post-expansion-consolidation/research/RESEARCH2-drive-delay.md`) both exist.
- Scanned `archive/`'s 106 subdirectories' filenames for correspondence/scratch/junk shape — all fit the established archived-change artifact pattern (proposal/tasks/design/postflight/audit records); this is the accepted, gitignored "working record" per the repo's own `.gitignore` comment, not hygiene debt.
- **Disposition: 0 findings needing change**, apart from the carried-forward `.sessions` item below.

### Tree 4: Repository root documents

- Checked `README.md`, `MANUAL.md`, `QUICK_DICT.md`, `HANDOFF.md`, `DAISY_MANUAL.md`, `frogg3rs.code-workspace`, `LICENSE`, `Makefile`, `package.json`, `.gitignore`, `.gitmodules`.
- Grepped for stale product references (`vcv`, `sequencer`, `gesture`, `desktop-v2`) — only one hit, "gesture" in `MANUAL.md:205`, which is live prose about randomization gestures, not the deleted gestures feature. Nothing needed.
- Extracted every backtick-quoted path/link across the five `.md` files and verified each resolves; two apparent misses were both legitimate (`omni-rule.md` resolves one level up at Desktop scope; `tasks.md` is shorthand for the sibling file in the change directory just named, and it exists).
- `README.md` and `HANDOFF.md` read current and accurate to the shipped product; no stale claims found.
- **Disposition: 0 findings needing change**, apart from the carried-forward `frogg3rs.code-workspace` item below.

### The three carried-forward findings — CONFIRMED

1. **`openspec/.sessions/marbles-mod-led-level-meter-progress.md` is orphaned — CONFIRMED.** Repo-wide grep for the bare filename found only this change's own `proposal.md` (documenting the finding) — no script, doc, or Makefile invokes it. A second pass grepping for the directory token `.sessions` found matches only inside the unrelated vendored `External/Sheaf/` tree and this same proposal.md. **IN SCOPE** is not applicable — it sits under `openspec/` but outside this change's actual edits; classified OUT OF SCOPE below per the proposal's own prior disposition.

2. **`node-v22.16.0-darwin-arm64/` and `build/manifest/` protected only via `.git/info/exclude` — CONFIRMED.** `git check-ignore -v` on each returns `.git/info/exclude:10:node-v*/` and `.git/info/exclude:27:build/manifest/` respectively — neither pattern exists in the tracked root `.gitignore`. Confirmed this protection would not survive a fresh clone.

3. **`frogg3rs.code-workspace` excludes two nonexistent directories — CONFIRMED.** Its `files.watcherExclude` lists `**/wasm/build/**` and `**/desktop/build/**`; `ls -d wasm desktop` at repo root both report "No such file or directory." (Its other two entries, `**/node_modules/**` and `**/.emsdk/**`, do resolve — `.emsdk/` exists — so only the wasm/desktop pair is stale.)

### Additional observation (bonus, not one of the three carried-forward items)

- `.cursorignore` at the repo root also has stale entries (`wasm/build/`, `desktop/build/`, `desktop/dist/`, `desktop/FroggersTigaPlugin_artefacts/`, `!vcv/`/`!vcv/**` — `vcv/` doesn't exist either) — same drift pattern as finding #3. However `.cursorignore` is itself listed in `.git/info/exclude` (confirmed via `git status --ignored=matching`) — it is untracked, local-only editor tooling, the same category as `.claude/`/`.cursor/`/`.vscode/` that the root `.gitignore`'s own comment calls "not part of the product." **OUT OF SCOPE**: not a tracked/shipped artifact.

### Consolidated findings list

| Finding | Classification | Reason |
|---|---|---|
| `openspec/.sessions/marbles-mod-led-level-meter-progress.md` orphaned | **OUT OF SCOPE** | Not on a path this change edits (Impact lists `openspec/specs/`, `openspec/changes/` non-archive work, root docs, `app/`); it's leftover from an *archived* change with no relation to delay-width/WYSIWYG. Reported per the proposal's own carry-forward, not fixed here. |
| `node-v22.16.0-darwin-arm64/`, `build/manifest/` gitignore-fragility | **OUT OF SCOPE** | Root-level vendored/build-output hygiene unrelated to the delay/width repair; not a path this change touches. |
| `frogg3rs.code-workspace` stale excludes (`wasm/build`, `desktop/build`) | **OUT OF SCOPE** | Root document, but unrelated to the Delay/Width-balance repair this change makes; the change's Impact does not name `frogg3rs.code-workspace` as an edited file. |
| `.cursorignore` stale excludes (bonus finding) | **OUT OF SCOPE** | Untracked, local-only tooling file (excluded via `.git/info/exclude`), not part of the shipped repo. |

None of the four findings sit on a path this change's Impact section names as edited (`app/dsp/Delay.hpp`, `app/FroggersDspParityTests.cpp`, `MANUAL.md`, `QUICK_DICT.md`) or gates it exercises, so none qualify as IN SCOPE under §8.0's "fixed inside the touching change" rule — they are pre-existing debt on adjacent, untouched paths, correctly left for a separate bounded dispatch.

### HELD directories (enumerated, not swept for findings, not editable by this change)

- `openspec/changes/frogg3rs-midi-controller-resilience/` — untracked (`git status` shows `??`), contains `design.md`, `preflight.md`, `preflight-2.md`, `preflight-3.md`, `proposal.md`, `tasks.md`, `specs/`. Owned by another concurrent session.
- `openspec/changes/frogg3rs-randomize-depth-reclaim/` — untracked (`??`), contains `proposal.md`, `tasks.md`, `specs/`. Owned by another concurrent session.

### FOUND vs CHANGED

- **FOUND:** 4 (the 3 carried-forward findings, confirmed true, plus 1 bonus `.cursorignore` observation of the same class).
- **CHANGED:** 0. No edits were made to any file; this was a read-only report as instructed. All 4 findings are classified OUT OF SCOPE for this change and are reported for the coordinator to route to a separate bounded dispatch, consistent with the proposal's own existing "Hygiene found outside this change's Impact, unfixed and reported" note.
