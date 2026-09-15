# Postflight — Stage 2 (tasks 2.1–2.6)

Scope: uncommitted working tree for `MANUAL.md`, `QUICK_DICT.md`,
`frogg3rs.code-workspace`, plus the staged deletion of `HANDOFF.md`, against
`tasks.md` 2.1–2.6 and `proposal.md`. `research/` not read for this pass
(instructed), except where `grep -rn HANDOFF` mechanically surfaced lines
inside it for the hygiene classification in axis 3 — those lines are quoted
verbatim below and not otherwise opened.

## Commands and literal output

### git status / diff scope

```
$ git status --short
D  HANDOFF.md
 M MANUAL.md
 M QUICK_DICT.md
 M frogg3rs.code-workspace
 M openspec/changes/frogg3rs-delay-capacity-and-width-finish/tasks.md
```

`tasks.md` is modified but is not one of the files this stage's diff covers
per the brief (`MANUAL.md`, `QUICK_DICT.md`, `frogg3rs.code-workspace`,
`HANDOFF.md`'s deletion) — not read further here.

`git diff HEAD -- MANUAL.md QUICK_DICT.md frogg3rs.code-workspace` (full text
reproduced, this is the diff axis 1 and axis 2 are checked against):

```diff
--- a/MANUAL.md
+++ b/MANUAL.md
@@ -691,11 +691,10 @@
 **Stereo width** (slot 4) — offsets when the right channel reads the delay line relative to the left;
 at 0 both taps read the same point in the line, and raising the knob spreads the right tap further
 behind the left in time. That time offset is the whole of the widening: the knob sets nothing in the
 feedback path, so Feedback (slot 3) and Freeze (slot 5) change how long the repeats last, not how
 wide they are. The offset never asks for more than the line holds; near the top of Delay time
 (slot 2) the spread shrinks so the right tap's read stays inside the line.
@@ -729,9 +728,9 @@
-**Width balance** (`Width bal`, slot 12) — an overall scalar on how strongly Stereo width's cross-feed
-and time-spread apply. Full strength at the top of travel (the default); turning it down narrows the
-stereo image Width can produce.
+**Width balance** (`Width bal`, slot 12) — scales the time offset Stereo width (slot 4) produces.
+Full strength at the top of travel (the default); turning it down narrows the spread Stereo width
+can produce, to none at the bottom.

--- a/QUICK_DICT.md
+++ b/QUICK_DICT.md
@@ -73,7 +73,7 @@
-- **Stereo width** (slot 4) — Offsets the right tap's read time behind the left; also sets a cross-feed weight that rides the feedback path, so it reaches the repeats once Feedback or Freeze leaves 0. At the page's defaults the widening is all time offset.
+- **Stereo width** (slot 4) — Offsets the right tap's read time behind the left. The offset is the whole of the widening; nothing rides the feedback path, and the read never leaves the line.
@@ -81,7 +81,7 @@
-- **Width balance** (`Width bal`, slot 12) — Overall scalar on Stereo width's own spread; default reproduces original fixed behavior.
+- **Width balance** (`Width bal`, slot 12) — Scales the time offset Stereo width produces; full at the top of travel (the default), none at the bottom.

--- a/frogg3rs.code-workspace
+++ b/frogg3rs.code-workspace
@@ -11,9 +11,7 @@
     "files.watcherExclude": {
       "**/node_modules/**": true,
-      "**/.emsdk/**": true,
-      "**/wasm/build/**": true,
-      "**/desktop/build/**": true
+      "**/.emsdk/**": true
     }
   }
 }
```

`git diff HEAD --stat -- HANDOFF.md`:
```
 HANDOFF.md | 182 -------------------------------------------------------------
 1 file changed, 182 deletions(-)
```
Full deletion, no partial hunk. `git show HEAD:HANDOFF.md` confirms the deleted
file was titled `# Handoff — frogg3rs-delay-width-wysiwyg-repair` and was about
that superseded change.

### Filesystem checks for the code

```
$ grep -n "class StereoDelay\|struct StereoDelay" app/dsp/Delay.hpp
255:struct StereoDelay
```

Relevant lines read directly from `app/dsp/Delay.hpp` (255–913), quoted in the
truth table below by line number.

### axis 3 — HANDOFF.md mentions

```
$ grep -rn "HANDOFF" . --exclude-dir=External --exclude-dir=node_modules --exclude-dir=.git
openspec/changes/frogg3rs-delay-capacity-and-width-finish/prior-preflight-adjudication.md:17:- Task 2.5's premise. `HANDOFF.md` item 4 already carries its correction and the
openspec/changes/frogg3rs-delay-capacity-and-width-finish/tasks.md:164:- [x] 2.5 `HANDOFF.md` removed. It was titled after a deleted change and
openspec/changes/frogg3rs-delay-capacity-and-width-finish/research/prior-art-crossfeed-separation.md:107:  This is the sentence quoted (slightly compressed) by the active proposal and HANDOFF.md.
openspec/changes/frogg3rs-delay-capacity-and-width-finish/research/prior-art-crossfeed-separation.md:113:- **`053e64c`**, 2026-09-13, "Leave the next session a handoff for the Delay width repair" — added `HANDOFF.md` (read in full above), records that six sessions in a row reached this defect and handed it forward without attempting a fix, and restates the same `04efe9c` ruling as settling *whether* it should be fixed (not *how*).
openspec/changes/frogg3rs-delay-capacity-and-width-finish/research/instrument-derivation.md:7:**Deliverable is this report. No file under `app/` was touched** — confirmed by `git diff --stat -- app/` showing no output. (`HANDOFF.md`, `proposal.md`, `tasks.md` show as modified in `git status` but those are pre-existing changes from before this task started; I only read them.)
openspec/changes/frogg3rs-delay-capacity-and-width-finish/research/postflight-hygiene.md:73:- `HANDOFF.md` (out of scope per the brief — Stage 2).
```

Classification — all six are record (history), none is a live pointer (a
mention expecting the file to still exist and be navigated to), none is in a
sibling change directory (`openspec/changes/` holds only `archive`,
`frogg3rs-delay-capacity-and-width-finish`, `frogg3rs-randomize-depth-reclaim`
— the superseded `frogg3rs-delay-width-wysiwyg-repair` directory HANDOFF.md
was titled after is itself already gone, consistent with task 2.5's premise):

| Hit | Classification |
|---|---|
| `prior-preflight-adjudication.md:17` | record (adjudication note citing HANDOFF.md's own item 4) |
| `tasks.md:164` | record (the task text itself, naming the deletion) |
| `research/prior-art-crossfeed-separation.md:107` | record (citation) |
| `research/prior-art-crossfeed-separation.md:113` | record (commit-history note) |
| `research/instrument-derivation.md:7` | record (scope note) |
| `research/postflight-hygiene.md:73` | record (prior postflight's out-of-scope note) |

Task 2.5 says "Its inbound mentions are all records under `research/`"; two of
the six (`prior-preflight-adjudication.md`, `tasks.md`) are records but sit at
the change directory's root, not under `research/`. Both are still history,
not live pointers, and nothing reads them as a path that must resolve — this
is a precision gap in the task's wording, not a divergence with a consequence.
Not filed as a finding (bears on neither axis).

### axis 3 — watcherExclude globs against the filesystem

```
$ find . -maxdepth 6 -type d -name node_modules
./.emsdk/node/22.16.0_64bit/lib/node_modules
./.emsdk/upstream/emscripten/node_modules  (+ nested)
./External/Sheaf/projects/synth/browser/node_modules
./app/browser/e2e/node_modules
./node-v22.16.0-darwin-arm64/lib/node_modules (+ nested)

$ ls -d .emsdk
.emsdk

$ find . -maxdepth 5 -type d -path "*wasm/build*"
(no output)

$ find . -maxdepth 5 -type d -path "*desktop/build*"
(no output)
```

`**/node_modules/**` resolves, including `External/Sheaf/projects/synth/browser/node_modules`
(Sheaf's browser install) and the vendored `node-v22.16.0-darwin-arm64` and
`.emsdk` node trees, matching task 2.3's stated rationale. `**/.emsdk/**`
resolves (the directory exists). `**/wasm/build/**` and `**/desktop/build/**`
resolve nowhere in the tree — confirmed stale, correctly removed. Post-diff
`files.watcherExclude` is exactly `{"**/node_modules/**": true, "**/.emsdk/**": true}`.

### axis 3 — docs gate

```
$ make -n check-docs-match-parameter-table   (from app/)
python3 /Users/diegoaguilar-canabal/Desktop/frogg3rs/app/check_docs_match_parameter_table.py "/Users/diegoaguilar-canabal/Desktop/frogg3rs/app"

$ python3 check_docs_match_parameter_table.py "/Users/diegoaguilar-canabal/Desktop/frogg3rs/app"
check-docs-match-parameter-table: OK - MANUAL.md 84 entries/0 failures; QUICK_DICT.md 84 entries/0 failures
EXIT_CODE=0
```

Gate passes on the working tree as-is.

## Axis 1 — diff against the plan (2.1–2.5)

- **2.1** (Stereo width entries say offset is the whole of the widening,
  nothing rides the feedback path, the read never leaves the line): both
  files' Stereo width entries carry all three claims. Matches.
- **2.2** (Width balance entries: scales the time offset Stereo width
  produces, full at default top of travel, none at bottom): both files'
  Width balance entries carry this. Matches.
- **2.3** (workspace `watcherExclude`: node_modules glob resolves, wasm/build
  and desktop/build were the only stale entries, removed): confirmed against
  the filesystem above — matches exactly, `.emsdk` also still resolves and is
  correctly retained.
- **2.4** (pre-stage diff check for foreign edits, stage only this change's
  hunks with `git add -p`, re-run before committing): nothing for
  `MANUAL.md`/`QUICK_DICT.md`/`frogg3rs.code-workspace` is staged yet (`git
  status` above shows all three as unstaged ` M`, only `HANDOFF.md`'s
  deletion is staged). Task 2.4 is marked `[x]` in `tasks.md`. Read against
  the stage's own mechanics text ("Every stage ends in one commit... after
  that stage's postflight") and 2.4's own instruction to "Re-run immediately
  before committing," staging is deferred to 2.6's commit step, which has not
  run — consistent, not a divergence. The diff contains only this change's
  hunks; no sign of foreign edits.
- **2.5** (HANDOFF.md removed; titled after a deleted change; inbound
  mentions are records, stay as history): removal confirmed full-file, no
  partial hunk. The predecessor `frogg3rs-delay-width-wysiwyg-repair`
  directory HANDOFF.md was titled after is absent from `openspec/changes/`,
  confirming "titled after a deleted change." Inbound-mentions claim checked
  in axis 3 above — true in substance (all six are history, none a live
  pointer), imprecise only in the "under `research/`" detail, not filed as a
  finding.

Diff contains nothing beyond what 2.1–2.3 and 2.5 name: MANUAL.md and
QUICK_DICT.md touch only the Stereo width and Width balance entries; the
workspace file touches only the two stale watcher globs; HANDOFF.md's removal
is a clean full-file deletion.

## Axis 2 — per-sentence truth table (documentation vs. `dsp::StereoDelay::Process`, `app/dsp/Delay.hpp`)

Authoritative slot list, `app/FroggersParameters.hpp:262–277` (Delay bank,
positional): Wet/dry(0), Send(1), Delay time(2), Feedback(3), Stereo
width(4), Freeze(5), Mod depth(6), Reverse blend(7), Diffusion(8), Feedback
drive(9), Feedback tone(10), Mod rate(11), Width balance(12), Crush(13).

| # | Sentence (file) | Ruling | Code basis |
|---|---|---|---|
| 1 | "That time offset is the whole of the widening: the knob sets nothing in the feedback path, so Feedback (slot 3) and Freeze (slot 5) change how long the repeats last, not how wide they are." (MANUAL, Stereo width) | TRUE | `Delay.hpp:759` `const float cross = 0.0f;` — fixed, not read from `p.dwid` or `widthBalance`. Feedback/Freeze feed only `fbk`/`freezeEff`/`fbEff` (`:768,844-846`), consumed only at `WriteSample` (`:847-848`), which sets loop persistence/level, not `timeL`/`timeR`. |
| 2 | "The offset never asks for more than the line holds; near the top of Delay time (slot 2) the spread shrinks so the right tap's read stays inside the line." (MANUAL, Stereo width) | TRUE | `:703-707`: `widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds)` where `maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds)` — as `baseSeconds` (set by Delay time, `p.dtim`) rises toward `capacitySeconds`, `maxSpreadSeconds` shrinks toward 0, clamping `widthSpread`. `timeR <= capacitySeconds` asserted at `:720-721` (`FROGGERS_DSP_CHECKS`) and guaranteed unconditionally by the `min()` chain regardless of that gate. |
| 3 | "scales the time offset Stereo width (slot 4) produces." (MANUAL, Width balance) | TRUE | `:703` `widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance` — `widthBalance` multiplies the same term `p.dwid` (Stereo width) drives. |
| 4 | "Full strength at the top of travel (the default); turning it down narrows the spread Stereo width can produce, to none at the bottom." (MANUAL, Width balance) | TRUE | `:315` default `widthBalance = 1.0f`; `SetWidthBalance` (`:645`) is an identity map, `widthBalance == knob01`; at `knob01 = 0`, `widthSpreadRaw = 0` regardless of `p.dwid`. |
| 5 | "Offsets the right tap's read time behind the left." (QUICK_DICT, Stereo width) | TRUE | `:707` `timeR = ... + widthSpread` (spread added only to the right tap's time), `:724-725` `dR = ReadAt(timeR, lineR)`. |
| 6 | "The offset is the whole of the widening; nothing rides the feedback path, and the read never leaves the line." (QUICK_DICT, Stereo width) | TRUE | Same basis as row 1 (`cross = 0.0f`) plus row 2 (`min()`-bounded `timeR <= capacitySeconds`). |
| 7 | "Scales the time offset Stereo width produces; full at the top of travel (the default), none at the bottom." (QUICK_DICT, Width balance) | TRUE | Same basis as rows 3–4. |

7 sentences checked, 7 TRUE, 0 FALSE.

Slot numbers named in the new sentences — Feedback(3), Freeze(5), Delay
time(2), Stereo width(4), Width balance(12) — all match the Delay bank's
positional slot list in `app/FroggersParameters.hpp:262-277` exactly, and the
`check-docs-match-parameter-table` gate (axis 3) independently confirms all
84 entries in each file (including these) against that same source, 0
failures.

## Findings

None. All three axes pass:

- Axis 1: diff matches tasks 2.1, 2.2, 2.3, 2.5 exactly; task 2.4's staging
  step is legitimately deferred to 2.6, not a divergence; diff contains
  nothing the tasks do not name.
- Axis 2: 7/7 added sentences TRUE against `dsp::StereoDelay::Process`; all
  named slot numbers correct against the Delay bank's authoritative list and
  the docs gate.
- Axis 3: all 6 `HANDOFF.md` mentions are records/history, none a live
  pointer, none in a sibling change directory; both surviving
  `watcherExclude` globs resolve on the filesystem and the two removed ones
  are confirmed stale; `check-docs-match-parameter-table` passes (84/84 both
  files, exit 0).

Task 2.6 (stage gate: suite green, commit, push) is not performed by this
postflight — this record only audits 2.1–2.5's diff and the standing docs
gate; the full suite (`make -k`, twelve binaries by path) was not re-run here
and is 2.6's own job.
