# Preflight 7 — auditor four, executability axis (initial report + debate verdicts)



===== BLOCK 1 of 3 (timestamp: 2026-09-14T05:32:16.156Z) =====


Confirmed — the gate collects "bullets" (dash-opening lines), consistent with task 3.2's claim that it doesn't compare requirement paragraph prose. Verified accurate. That's enough breadth on the secondary tasks; I have what I need to write the report.


===== BLOCK 2 of 3 (timestamp: 2026-09-14T05:33:11.034Z) =====


# Preflight Audit Two — Executability — `frogg3rs-delay-width-wysiwyg-repair`

Scope: mental execution of `tasks.md`, checking every task against the code it names, with primary focus on task 1.1's check design as directed. Edited nothing.

## Task-by-task executability

**1.1 (repair).** `cross = p.dwid * 0.5f * widthBalance` at `Delay.hpp:720`, `widthSpread` at `:679`, `SetWidthBalance` at `:635` — all verified by reading, line numbers current. The repair itself (`cross = 0.0f`) is a one-line, unambiguous edit. **1.1 (check) is where the guessing starts — see dedicated section below; this is blocking.**

**1.2 (Width balance ruling).** Executable. Since `widthSpread`'s formula is untouched by 1.1, `widthBalance = 1.0` trivially reproduces identical `widthSpread` values pre- and post-repair — that half is answerable by reading, not measurement. "Reproduces today's shipped voice" has no other free variable once `cross` is fixed at zero, so there's nothing left to invent. Non-blocking.

**1.3 (ReadAt capacity fix).** Executable. `ReadAt` at `:949`, the `% capacityI` wrap confirmed with no clamp. The task supplies the comparison method and an explicit "if they differ audibly, report both and do not choose" escape — no invented numeric threshold required. Non-blocking.

**1.4 (pin enumeration).** Executable. Confirmed `stereo_delay_cross_feed_reproduces_its_captured_output_exactly` at line 6978 (exact) and `stereo_delay_freeze_at_default_reproduces_pinned_original_output_through_real_process` at line 7815 (exact). Confirmed the second test's call to `MapRowsToDelayParams` uses `/*width=*/0.25f` with `feedback=0.6f` — a genuine candidate pin matching the proposal's own criterion. Confirmed `RoutedKnob(FroggersBankId::Delay, 4)` is width in production (`FroggersAppCore.hpp:1917`), matching `MapRowsToDelayParams`'s `widthKnob01` parameter position. Four operand passes are mechanical and well-scoped. Non-blocking.

**1.5 (provenance header).** Executable. Confirmed exactly one existing "REMOVED since" precedent (`Delay.hpp:28`) for the task to follow the shape of.

**1.6, 2.5, 3.7 (gates).** Confirmed live: `check_docs_match_parameter_table.py` reports "MANUAL.md 84 entries/0 failures; QUICK_DICT.md 84 entries/0 failures" today, matching task 2.2's baseline claim exactly. `check_spec_checks_resolve.py` fails on exactly the two named `frogg3rs-randomize-depth-reclaim` checks and nothing else. `git status --short` confirms `frogg3rs-midi-controller-resilience/` and `frogg3rs-randomize-depth-reclaim/` are both untracked, as the proposal requires. All verified, not merely asserted.

**2.1/2.2 (doc rewrites).** Confirmed MANUAL.md:692-697 and :732-734 currently say exactly what the proposal claims ("cross-feed weight ... rides the feedback path," "an overall scalar on how strongly Stereo width's cross-feed and time-spread apply") — the premise these tasks rewrite from is accurate.

**2.3 (workspace file).** Confirmed `frogg3rs.code-workspace` has exactly the two stale entries named and no `wasm/`/`desktop/` directories at root. Executable.

**2.4, 3.1, 3.3 (git-diff caution, grep-based repoint/resolve).** Mechanically well-specified; no blank to fill.

**3.2.** Confirmed `check_modified_requirements_restate_promoted.py`'s bullet collector keeps dash-opening lines only (its own docstring/`join_wrapped` logic), matching the task's claim that it can't be trusted to compare requirement prose.

## Non-blocking finding: the research/ directory is empty

`research/INDEX.md` describes six measurement files (`cross-feed-widening.md`, `retired-mechanisms.md`, `prior-art-crossfeed-separation.md`, `instrument-derivation.md`, `side-mid-rejection.md`, `read-at-capacity.md`) as already "extracted verbatim from the measurement sessions... by a pass separate from the one that wrote proposal.md." None of those six files exist — the directory holds only `INDEX.md` itself. Every `research/INDEX.md → <topic>` citation in `proposal.md` (7+ of them) is currently a dangling pointer. **What breaks:** the provenance trail INDEX.md claims to provide doesn't exist, so a figure questioned mid-execution or later has no backing record to open. **Non-blocking for the numbered tasks** because every figure a task actually needs is duplicated inline in `proposal.md` or already promoted in `spec.md` — I checked each task's dependency and none requires opening a missing file. Still worth fixing before this ships, since it's exactly the kind of provenance gap §1/§9 call out.

---

## Task 1.1's check: does it discriminate, and would it go red on revert?

**Short answer: cannot tell from the task text, and that is the defect.** The proposal correctly defused the trap named in my brief — it doesn't assert bare monotonicity, it asserts the relative measure `todayCorr − repairedCorr > margin`, which is the right shape to avoid "shipped law also falls monotonically." But the task never specifies **how `todayCorr` is computed**, and I traced why that is not a filled-in detail but an open structural question:

- `Correlation`'s two inputs must come from somewhere. `repairedCorr` can come straight from the real, post-repair `dsp::StereoDelay::Process` — task's own "Tap point" (`Process`'s returned `DelayWetPair`) supports that reading.
- `todayCorr` has no such source. After 1.1 lands, **no code anywhere computes `cross = p.dwid * 0.5f * widthBalance` any more** — that's the whole point of the repair. So a live measurement of "today's law" cannot come from calling `Process()`.
- The proposal cites its own precedent for "the same shape": `reverb_damping_filter_split_lowers_wet_leg_correlation_at_every_setting` (`FroggersDspParityTests.cpp:9531`). I read it. Its "shared" (retired) regime is reconstructed by **peeking already-public state** (`rv.lineA`/`lineB`/`indexA`/`indexB`) and reapplying a small standalone filter — this works only because Reverb's damping-filter choice is a **pure post-tap operation that never feeds back into the recirculating lines** (the test's own comment: "never feeds back into lineA/lineB").
- `StereoDelay`'s cross-feed is the opposite: `cross` sets `fbL`/`fbR` which are then **written back into the recirculating `lineL`/`lineR`** (`Delay.hpp:726-728`, `:808-809`). At Feedback 0.7 (the task's own operating point) the two laws' recirculating content diverges sample by sample. I also confirmed `lineL`, `lineR`, `writePos`, and `ReadAt` are **private** (`Delay.hpp:924` on) — unlike Reverb's public delay-line members, there is nothing to peek even if the compounding problem didn't exist.
- Net: reconstructing "today's law" requires either (a) an unauthorized structural addition to `StereoDelay` (a runtime-selectable cross-feed, contradicting the repair's own text that `cross` becomes "a fixed `0.0f`"), or (b) a from-scratch, ~90-line duplicate of `Process()`'s whole recirculating pipeline live in the test (directly against this codebase's own repeatedly-stated anti-duplication convention), or (c) freezing `todayCorr` as literal constants captured once against the pre-repair code (the file's own "MEASURED FIRST, then written to match" convention, used by the two golden-vector tests at 6978/7815) — which works, but is nowhere instructed, and changes what "from the same noise run" means from a live dual-computation to a live-vs-frozen comparison.

None of (a)/(b)/(c) is named in the task. The choice materially changes whether the standing regression check tracks current "shipped" behavior or a frozen snapshot of it, and (b) — the option closest to a literal reading of "compute both laws" — is also the one most likely to be picked under time pressure and is a real duplication/drift risk. This is a **guess the task forces on the executor**, not a detail it left to discretion.

**Blocking. Blocks task 1.1** (and downstream: 1.4's "every test whose state changes must be on an expected list," 3.1's repoint, and A.3's adversarial pass all inherit whatever the check turns out to be).

## §6.3 accepting-path enumeration on the check as specified

1. **Vacuous-pass via `Correlation::Value()`'s hardcoded `1.0`.** If `todayCorr`'s computation (whichever of (a)/(b)/(c) above gets chosen) has a bug that silences the signal it's fed, `Value()` returns `1.0` — which *inflates* `todayCorr − repairedCorr`, making the assertion pass *more* easily, not less. The task requires liveness (`rmsL`, `rmsR` nonzero) "on every row" but never says liveness must be asserted on **both** `todayCorr`'s and `repairedCorr`'s own rms independently. A single liveness check tied only to the real-production `repairedCorr` path leaves exactly the failure mode this proposal itself calls out (proposal's own instrument note) undetected on the harder-to-get-right side. **Blocks 1.1.**
2. **The level-balance bound is never stated.** Proposal: "stays under a stated bound" — no number appears anywhere in `proposal.md`, `tasks.md`, or `research/INDEX.md` (confirmed by reading all three). Unlike the margin, which task 1.1 explicitly says to measure first, there is no parallel "measure this bound first" instruction. `tasks.md`'s own mechanics section forbids exactly this: "No task hands an executor a check without its assertion already written... never invented to fill a blank." **Blocking, blocks 1.1.**
3. **Margin looseness.** "the margin set by task 1.1's own measurement" doesn't say whether it's the exact minimum measured gap or the minimum with slack. A generously slack margin tolerates a future partial regression (a repair that shrinks but doesn't zero `cross`) without going red. Non-blocking — ordinary executor discretion, and the instruction to measure first bounds how far off it can be.
4. **`dwid = 0` exemption.** Correctly named as void/not-asserted in the proposal. Risk is only in an over-broad implementation of the exemption silently dropping other grid points; nothing in the task invites that. Non-blocking.
5. **The pan-law adversarial construction (A.3 / 1.1's own control).** As specified this is a real, targeted control against a real evasion class (pans-instead-of-widens) and is answerable without invention. Non-blocking.

Findings 1 and 2 are two independent, concrete "must guess" defects layered on top of the structural one above; all three land on the same task.

## PREFLIGHT REJECTS

Numbered defects, blocking unless marked otherwise:

1. Task 1.1's check does not specify how `todayCorr` is obtained once the repair removes the shipped law from production code entirely; the reverb precedent it cites as "the same shape" does not transfer because Reverb's regime-swap is post-tap/non-feeding-back while Delay's cross-feed is written back into the recirculating lines it also reads from. Blocks 1.1, and transitively 1.4, 3.1, A.3.
2. Task 1.1's liveness requirement is not specified as applying independently to whichever computation backs `todayCorr`, so a broken/dead reconstruction of "today's law" would read as `1.0` (`Correlation::Value()`'s zero-denominator default) and inflate the pass margin rather than trip it. Blocks 1.1.
3. The level-balance companion's bound is asserted but never stated or scheduled for measurement anywhere in the artifacts, contrary to `tasks.md`'s own rule against blanks filled by invention. Blocks 1.1.
4. (Non-blocking) `research/` holds only `INDEX.md`; the six files it indexes and that `proposal.md` cites throughout do not exist, leaving every such citation dangling. Does not block any task's literal execution because the needed figures are duplicated inline, but should be fixed before this ships.


===== BLOCK 3 of 3 (timestamp: 2026-09-14T05:39:01.133Z) =====


## Verdicts

### B1 — research/ files never created, no gate catches it — **CONFIRM (blocking)**

Checked independently: `grep -ni "research/\|INDEX.md\|extract" tasks.md` returns nothing — no task in the plan ever creates, populates, or even mentions the six indexed files. Checked `check_citations_resolve.py`'s Makefile invocation (`app/Makefile:208`, `python3 check_citations_resolve.py "$(APP_DIR)"`) and its own docstring: it resolves `path:line` citations inside `app/` C++ comments only. It is never invoked against `openspec/` and has no markdown-pointer logic at all. Their evidence closes a gap my own D4 didn't check.

**This changes my materiality judgment — I withdraw the non-blocking call.** My original reasoning ("every figure a task needs is duplicated inline") answered too narrow a question: whether an executor can literally perform each task without opening the files. It didn't ask whether the gap ever closes. It doesn't: nothing creates the files, nothing flags their absence, and task 3.6 archives the change with that permanent gap intact. The inline text in `proposal.md` also isn't a substitute — "range 0.00032–0.99998, mean 0.64," "43-47% of frequencies," "1408/2552 vs 980/980" are stated *conclusions*, not the underlying sweep data; nobody can re-derive or spot-check them, ever, once archived. `INDEX.md`'s own claim that these files are "extracted verbatim from the measurement sessions... by a pass separate from the one that wrote proposal.md and tasks.md" is then simply false. That's a direct hit on the omni-rule's TRACE-DON'T-ASSERT and PROVENANCE-IS-NEVER-VERIFICATION sections, for the load-bearing justification of the central design decision (zero vs. a small fixed weight). Reclassifying my **D4 as blocking, blocks task 3.6** — either the files get written before archival, or the citations are stripped and the actual sweep data (not just conclusions) is inlined.

### B2 — margin is self-referential — **CONFIRM (blocking, task 1.1)**

Checked the exact task text again: "First measure `todayCorr - repairedCorr` at every grid point to set the margin... then assert `todayCorr - repairedCorr > <measured margin>` at every point." Under the natural reading — margin := the minimum of the measured gaps, which is the only value "set the margin" naturally produces without a further rule — the assertion at the grid point that produced that minimum reduces to `min > min`, false by construction. No rule is given for how far below the minimum the margin must sit. This holds up: the borrowed reverb precedent (`REQUIRE_TRUE(sharedCorr - splitCorr > 0.25)`) uses a fixed literal chosen with presumed headroom, never a live recomputation of its own minimum — the task doesn't say to reproduce *that* part of the shape, only the comparison shape itself.

**This is independent of my D1, not subsumed by it, and doesn't subsume it either.** D1 says there is no source for `todayCorr`'s *value* at all once production only implements the repaired law. B2 says that even granting valid gap numbers at every point, the stated margin-derivation is self-defeating. Fixing D1 (finding a way to compute `todayCorr`) does nothing to fix B2's arithmetic, and a correct margin rule does nothing to manufacture a source for `todayCorr`. Both must be resolved independently for task 1.1's check to exist at all. I'll also note B2 sharpens my own D3 (level-balance bound unstated): D3 flagged that bound as simply never stated; B2 shows the margin, which at least has a "measure first" instruction, still lands on a self-refuting reading — the level-balance bound has no measurement instruction *and* no derivation rule, which is the same defect one step further undressed.

### B3 — HANDOFF.md ships a claim the change's own correction refutes — **CONFIRM (blocking, task 3.6)**

Checked: `HANDOFF.md` exists at repo root; lines 82-84 read verbatim "At `dwid == 0` the cross-feed is a **no-op at any weight**, confirmed bitwise, so any law agreeing with the current one there costs nothing at the registered default." Checked `proposal.md`'s "A correction to the record" section, which explicitly refutes this: "An earlier draft of this change claimed the cross-feed is a no-op at `dwid == 0` at any weight... That reasoning is false in floating point... weights 0.15, 0.35 and 0.40 diverge from bit-exact output after roughly 950-1035 samples." Checked `proposal.md`'s Impact section: "Directories swept: ... the repository root's documents..." — `HANDOFF.md` is squarely inside that named scope. Checked `tasks.md` and `proposal.md` end to end for "HANDOFF": zero hits in either. So the sweep is promised in Impact but never turned into a task anywhere in Stage 1, 2, or 3, and 3.6 (archive) would finalize a document at the repo root that flatly contradicts the correction sitting in the same change. CONFIRM, blocking 3.6 until a task fixes or retires the claim.

### N1 — timeR arithmetic off by ~0.16% — **CONFIRM the number, agree NOT-MATERIAL**

Checked independently: read `ExpMapCompute`'s exact source (`app/dsp/DspMath.hpp:52`, `min * pow(max/min, value)`) and `widthSpread`'s formula (`app/dsp/Delay.hpp:679`), recomputed `baseSeconds = ExpMapCompute(0.001, 2.0, 0.99) ≈ 1.853616 s`, `widthSpread ≈ 0.648765 s` at `dwid=1.0`/`widthBalance=1.0`, `timeR ≈ 2.502381 s` — matches their recomputation, versus the proposal's stated `≈2.506 s` (~0.15-0.16% relative difference). Checked whether this literal figure feeds any assertion or check anywhere in `tasks.md`: it does not — task 1.3 is instructed to independently MEASURE both fix options rather than build on this illustrative number, and no `Check:` embeds `2.506`. The qualitative conclusion driving task 1.3 (timeR overruns the 2.0 s capacity by roughly a quarter-second either way) is unaffected. NOT-MATERIAL confirmed.

## Net effect on my report

- D1, D2, D3 stand unchanged.
- **D4 reclassified non-blocking → blocking**, per B1's evidence, blocking task 3.6.
- Two new blocking defects adopted: B2 (blocks 1.1, independent of D1) and B3 (blocks 3.6, alongside upgraded D4).
- N1 adopted as a confirmed, non-blocking correction.

Verdict stands **PREFLIGHT REJECTS**, now on five blocking defects: D1, D2, D3, B2 (task 1.1 — four independent reasons the check as specified cannot be built as written), plus D4/B1 and B3 (task 3.6 — two independent reasons archival would finalize permanent, uncaught defects).
