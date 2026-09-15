# Postflight hygiene — this change's own final diff

Scope: `git diff HEAD -- app/`; untracked `app/check_delay_capacity_parameters_are_swept.py`
and `app/check_delay_capacity_break_proofs.py`; the staged deletions under
`openspec/changes/frogg3rs-delay-width-wysiwyg-repair/`; the untracked
`openspec/changes/frogg3rs-delay-capacity-and-width-finish/` (research/ checked
for unresolved references only, not style).

## Commands with output

```
$ cd /Users/diegoaguilar-canabal/Desktop/frogg3rs && git status --short | grep '^D '
```
22 files under `openspec/changes/frogg3rs-delay-width-wysiwyg-repair/` staged deleted,
already absent from the working tree (`ls` on the directory: No such file or directory).

```
$ git diff HEAD -- app/Makefile
```
Adds `-DFROGGERS_DSP_CHECKS` to `CPPFLAGS`; adds `check-delay-capacity-break-proofs`
to `.PHONY` but NOT `check-delay-capacity-parameters-are-swept`; adds both new
`check-delay-capacity-*` recipes; adds both to the `test` prerequisite list.

```
$ grep -n "^\.PHONY" app/Makefile
```
`152:.PHONY: ... check-artifact-symbols-resolve check-delay-capacity-break-proofs test force`
— `check-delay-capacity-parameters-are-swept` is absent from this line.

```
$ python3 app/check_no_planning_history.py app
check-no-planning-history: OK - 59 files, no planning-history references
EXIT=0

$ python3 app/check_delay_capacity_parameters_are_swept.py app
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes
more than one value across the capacity-surface checks
EXIT=0

$ make -n check-citations-resolve check-artifact-symbols-resolve
python3 .../check_citations_resolve.py ".../app"
python3 .../check_artifact_symbols_resolve.py ".../app"

$ python3 app/check_citations_resolve.py app
check-citations-resolve: OK - 95 commit-pinned, 277 into pinned or frozen trees,
0 unresolvable, 0 line-numbered into this tree, 0 split across two lines

$ python3 app/check_artifact_symbols_resolve.py app
check-artifact-symbols-resolve: OK - 0 artifact file(s) resolve, 0 name(s)
declared as not yet created
```
All four prose gates pass. No path under `openspec/changes/frogg3rs-randomize-depth-reclaim/`
is reported (that directory is not present in the current tree, so the change's
own exclusion rule is moot here).

```
$ for f in app/check_*.py app/check_*.sh; do grep -c "$(basename "$f")" app/Makefile; done
```
Every `check_*` script under `app/` (including both new ones) is invoked from
`app/Makefile` exactly once. No orphaned gate.

```
$ python3 -c 'text=open("app/dsp/Delay.hpp").read(); ...count each BREAKS old_text...'
```
All 7 anchor strings in `check_delay_capacity_break_proofs.py`'s `BREAKS` table
occur exactly once in the current `app/dsp/Delay.hpp` — the mutation gate's
anchors have not drifted from the landed source.

```
$ grep -rn "frogg3rs-delay-width-wysiwyg-repair" . --exclude-dir=External --exclude-dir=node_modules
```
Hits, beyond the deleted directory's own former contents:
- `HANDOFF.md` (out of scope per the brief — Stage 2).
- `openspec/specs/froggers-sheaf-parameter-model/spec.md:128,186` — the LIVE,
  promoted spec — two NOTE lines say "pending `frogg3rs-delay-width-wysiwyg-repair`
  delivery" and one cites `openspec/changes/frogg3rs-delay-width-wysiwyg-repair/tasks.md`
  by path. That path no longer resolves once the staged deletion lands.
- `openspec/changes/frogg3rs-delay-capacity-and-width-finish/{proposal.md,tasks.md,
  prior-preflight-adjudication.md,research/INDEX.md,research/adversarial-check-audit-3.md,
  research/prior-art-crossfeed-separation.md,research/retired-mechanisms.md,
  specs/froggers-sheaf-parameter-model/spec.md:111,128,186}` — expected: this
  change's own provenance record of what it supersedes. `specs/.../spec.md:111`
  is not just provenance: its `Check:` prose still describes the OLD mechanism
  ("computes its cross-feed weight as half the width knob ... REFUTED for the
  Delay page") and still promises `frogg3rs-delay-width-wysiwyg-repair` will
  land the fix, but `app/dsp/Delay.hpp`'s landed diff already sets `cross = 0.0f`
  unconditionally — the fix this change's own diff delivers under a different
  change name. `tasks.md` open task 3.1 already names exactly this repoint as
  outstanding.
- No hits in `openspec/changes/frogg3rs-midi-controller-resilience/` or
  `openspec/changes/frogg3rs-randomize-depth-reclaim/` — neither directory
  exists in the current tree, so nothing to rule on for either.

```
$ grep -no 'preflight-[0-9]*\.md' openspec/changes/frogg3rs-delay-capacity-and-width-finish/research/*.md
```
`research/read-at-capacity.md` cites `preflight-6.md` through `preflight-10.md`
by bare name; `research/axis-0.1-capacity-guard-measurement.md` cites
`preflight-10.md`. Both resolve only inside the now-deleted
`frogg3rs-delay-width-wysiwyg-repair/` directory. `tasks.md` open task 3.7a
already names this as outstanding. `check_citations_resolve.py` does not catch
either the NOTE-prose mentions above or these bare-name research citations —
consistent with task 3.7a's own note that no gate scans `openspec/changes/**/research/`.

## Findings

1. **`app/dsp/Delay.hpp:302-303`, the "Width balance" (slot 12) field comment,
   is stale.** It reads "default 1.0f reproduces today's 0.35f/0.5f literals
   exactly" but the landed diff fixes the cross-feed weight at `0.0f` unconditionally
   (Process(), `const float cross = 0.0f;`) — Width balance no longer touches
   any 0.5f literal at all, only the 0.35f width-spread term. This comment sits
   outside every hunk in the diff and was not one of the "five amended comments"
   the proposal lists as landed. BLOCKS DELIVERY: a reader of this struct's own
   field declaration is told a false thing about what the knob still does.

2. **`.PHONY` omits `check-delay-capacity-parameters-are-swept`.** The diff adds
   two new phony targets but lists only `check-delay-capacity-break-proofs` in
   `.PHONY` (Makefile:152); the other new target still runs correctly (no file
   of that name exists in `app/` to collide with) and `make test` still reaches
   it. BLOCKS NEITHER — breaks nothing today, but is an inconsistency against
   the pattern this diff itself half-applies (every other `check-*` target,
   including its own sibling, is declared phony).

3. **The live, promoted spec (`openspec/specs/froggers-sheaf-parameter-model/spec.md:128,186`)
   references the change directory this diff deletes**, once by path
   (`openspec/changes/frogg3rs-delay-width-wysiwyg-repair/tasks.md`). Once the
   staged deletion lands, that path no longer resolves and the promoted spec
   still reads as though delivery of the cross-feed repair is pending under a
   change name that no longer exists in the tree — while the repair is in fact
   already landed in this diff's own `app/dsp/Delay.hpp`. This is already
   tracked as this change's own open task 3.5 ("Repoint the promoted spec's two
   transitional NOTEs"), not yet done. BLOCKS DELIVERY of this change's stage
   gate (task 3.9) as the plan itself already schedules 3.5 ahead of it — not a
   new obligation, confirmed still open and still true against the tree as it
   stands now.

4. **This change's own `specs/froggers-sheaf-parameter-model/spec.md:111`**
   (the delta that task 3.8 will promote) carries the same staleness one level
   deeper: its `Check:` prose describes the pre-repair cross-feed formula
   (`p.dwid * 0.5f * widthBalance`) and a `REFUTED for the Delay page` verdict
   that the now-landed `cross = 0.0f` fix and the new
   `stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width`
   test (added by this diff) supersede. Tracked as this change's own open task
   3.1 ("repoint its Check: onto the landed test, correct the diagnosis"), not
   yet done. BLOCKS DELIVERY of the stage gate for the same reason as (3).

5. **Two research files cite deleted `preflight-N.md` files by bare name**
   (`research/read-at-capacity.md` → preflight-6 through -10;
   `research/axis-0.1-capacity-guard-measurement.md` → preflight-10), resolving
   only inside the now-deleted `frogg3rs-delay-width-wysiwyg-repair/` directory.
   Tracked as this change's own open task 3.7a, not yet done. No automated gate
   catches this class of reference. BLOCKS DELIVERY of the stage gate, per the
   plan's own scheduling of 3.7a ahead of the archival step (3.8).

## What does not block

- Both new gates (`check-delay-capacity-parameters-are-swept`,
  `check-delay-capacity-break-proofs`) are invoked from `app/Makefile` and from
  `test`; no orphaned `check_*` script exists under `app/`.
- The break-proof gate's mutation anchors all match the landed
  `app/dsp/Delay.hpp` verbatim, exactly once each — no drift.
- The four prose gates in scope (`check_no_planning_history.py`,
  `check_delay_capacity_parameters_are_swept.py`, `check_citations_resolve.py`,
  `check_artifact_symbols_resolve.py`) all pass on the tree as it stands.
- `app/FroggersDspParityTests.cpp` and `app/FroggersAudioRoutingTests.cpp`
  diffs (golden-vector recapture, width-balance test rewrite, per-channel
  storm-test silence check, the new decorrelation/grid/random-walk tests) carry
  comments consistent with what they now check; no staleness found there.
- The rest of `app/dsp/Delay.hpp`'s touched comments (SetWidthBalance, the
  per-channel-limiter rationale, the new capacity-bound comments, ReadAt/
  WriteSample assertion comments) are internally consistent with the landed
  code.
