# Preflight 6 — auditor three, §9 checklist axis (initial report + debate verdicts)



===== BLOCK 1 of 2 (timestamp: 2026-09-14T05:34:56.635Z) =====


# Preflight Audit — `frogg3rs-delay-width-wysiwyg-repair`

Scope covered: `proposal.md`, `tasks.md`, `research/INDEX.md`, the unchanged `specs/froggers-sheaf-parameter-model/spec.md`, `HANDOFF.md`, `frogg3rs.code-workspace`, `MANUAL.md`/`QUICK_DICT.md` Delay entries, the cited code (`app/dsp/Delay.hpp`, `app/dsp/StereoField.hpp`), the cited tests (`app/FroggersDspParityTests.cpp`), the two held sibling changes, and all named gates (run live, not assumed).

## Blocking findings

**B1 — `research/` holds an index for six files that do not exist, and no task creates them.**
`research/INDEX.md` describes `cross-feed-widening.md`, `retired-mechanisms.md`, `prior-art-crossfeed-separation.md`, `instrument-derivation.md`, `side-mid-rejection.md`, `read-at-capacity.md`. `ls` of `research/` shows only `INDEX.md`. `grep` of `tasks.md` for `research/`/`INDEX.md`/`extract` returns nothing — no task in the plan ever creates these files. Every one of `proposal.md`'s six `research/INDEX.md → <topic>` pointers (the periodicity retraction, the 220 Hz frequency-sweep context, the prior-art citations, the corrected `dwid==0` divergence sample counts, the instrument-derivation rationale, the side/mid rejection identity, the `ReadAt` capacity trace) resolves to a table of contents for content that isn't in the tree and never will be under this plan. I confirmed no existing gate catches this either: `check_citations_resolve.py` only checks `path:line` citations inside `app/` C++ comments, not markdown pointers in `openspec/changes/**`. **What breaks:** the entire "Why zero, not a smaller fixed weight" rationale — the load-bearing argument for landing `cross = 0.0f` instead of some other law — rests on figures nobody can trace from the tree as delivered, permanently. This is exactly what §9 preflight exists to reject ("a proposal states only what has been read"). Blocks A.1 (this audit) and, downstream, task 1.2's ruling (which leans on the same design-document reasoning).
Disposition: **blocking** — either land the extraction pass as a real task gating Stage 1, or fold the load-bearing figures directly into `proposal.md` with resolvable citations and delete the dangling index.

**B2 — Task 1.1's check margin is specified as self-referential, which tasks.md's own rule forbids.**
Task 1.1: "measure `todayCorr - repairedCorr` at every grid point to set the margin... then assert `todayCorr - repairedCorr > <measured margin>` at every point." If the margin is derived from the same measured gaps (e.g., their minimum, the natural reading), the assertion at the defining point becomes "gap > gap," which is false by construction. No derivation rule (how far below the measured minimum the margin must sit) is given. **What breaks:** the executor cannot write the check as literally specified without inventing an unstated fudge factor — precisely the "blank filled toward green" the brief's preamble calls out by name, and `tasks.md`'s own Mechanics section states the rule this violates: *"No task hands an executor a check without its assertion already written."* The same gap exists for "the level-balance companion... stays under a stated bound." Different executors picking different fudge sizes silently changes how discriminating the regression pin is, and nothing later in the plan would catch that drift (A.3's adversarial pass tests two named evasions, not margin-tightness).
Disposition: **blocking** for task 1.1 — pin one sentence naming the margin-derivation rule (e.g., "assert `>=` the minimum measured gap" or "margin := minimum measured gap minus a stated epsilon") before an executor writes the check.

**B3 — `HANDOFF.md` is not just stale, it asserts a claim the rewrite just proved false, and no task cleans it up.**
Confirmed via `git status` (`M HANDOFF.md`, already modified in this session) and diff against `proposal.md`. `HANDOFF.md`'s "Do not re-derive these" item 4 states: *"At `dwid == 0` the cross-feed is a **no-op at any weight**, confirmed bitwise."* `proposal.md`'s own new "A correction to the record" section identifies this exact reasoning as **false** in floating point (`CrossFeedPair(x,x,K)` landing on `x` is "a per-value accident, not a guarantee"). `proposal.md`'s Impact section states the hygiene sweep covers "the repository root's documents," which includes `HANDOFF.md` — yet no Stage 2 or Stage 3 task names it. **What breaks:** a document inside the change's own declared sweep scope carries an affirmatively wrong claim about the defect this change repairs, with no task to fix or remove it; once Stage 3 archives the change directory, `HANDOFF.md` persists at the repo root, permanently wrong, orphaned from the corrected text that moved into `archive/`. This is a sweep-completeness violation under §8.0 ("what the sweep finds is fixed INSIDE that change"), not a mere prose staleness note.
Disposition: **blocking** for task 3.6 (archival) — add a task to delete or correct `HANDOFF.md` before archival. Per this audit's brief, I have not repaired it myself.

## Non-blocking findings (verified, dispositioned)

**N1 — Minor arithmetic imprecision in the `ReadAt` overflow illustration.** Recomputing `baseSeconds = ExpMapCompute(0.001, 2.0, 0.99)` and `widthSpread` at full width/default balance gives `timeR ≈ 2.5024 s`; the proposal states `≈2.506 s` (≈4 ms off, ~0.16%). Doesn't change the qualitative conclusion (timeR exceeds the 2.0 s capacity), and task 1.3 re-measures exactly rather than relying on this figure. **Nothing needed** — no dependent decision rests on the exact value.

**N2 — Verified accurate, no action needed:**
- All code line citations are exact: `Delay.hpp:720` (`cross`), `:679` (`widthSpread`), `:635` (`SetWidthBalance`), `:949` (`ReadAt`), `:250` (`kMaxDelaySamples`); `StereoField.hpp:39` (`CrossFeedPair`).
- Both test citations are exact and match their described behavior: `stereo_delay_cross_feed_reproduces_its_captured_output_exactly` (`:6978`, self-capture, as described) and `stereo_delay_freeze_at_default_reproduces_pinned_original_output_through_real_process` (`:7815`, `width=0.25`/`feedback=0.6`, matching the "candidate pin" criterion exactly).
- The reverb precedent citation (`reverb_damping_filter_split_lowers_wet_leg_correlation_at_every_setting`, `:9531`) and its `REQUIRE_TRUE(sharedCorr - splitCorr > 0.25)` assertion are verified verbatim.
- Gate baseline matches exactly as briefed: ran `check-docs-match-parameter-table`, `check-spec-checks-resolve`, `check-citations-resolve`, `check-modified-requirements-restate-promoted`, `check-no-planning-history`, `check-artifact-symbols-resolve`, `check-no-firmware-includes`, `check-microphone-usage`, `check-catalog-covers-screen-actions` live. Only failure: `check-spec-checks-resolve` on exactly the two named `frogg3rs-randomize-depth-reclaim` checks. `check-docs-match-parameter-table`: 84/84 entries, 0 failures, both documents, matching task 2.2's claim exactly.
- `check-modified-requirements-restate-promoted`'s "8 declared edit(s)" figure is fully and exactly attributable to this change's own two `RESTATES-EXCEPT` blocks (counted by hand: 1 + 7 = 8); the one flagged "promoted scenario no longer restated" belongs to the held `frogg3rs-midi-controller-resilience` change, not this one.
- The two transitional `NOTE (pending frogg3rs-delay-width-wysiwyg-repair delivery)` markers in the promoted spec: confirmed exactly two, matching task 3.4.
- `frogg3rs.code-workspace`'s two stale `watcherExclude` entries confirmed stale (`wasm/`, `desktop/` don't exist at repo root); the other two entries resolve.
- `MANUAL.md`/`QUICK_DICT.md`'s current Stereo width and Width balance entries read exactly as `proposal.md`/`tasks.md` describe them (pre-repair text, needing the described rewrite).
- No overlap between this change and either held sibling change (`frogg3rs-midi-controller-resilience`, `frogg3rs-randomize-depth-reclaim`) on `app/dsp/Delay.hpp`, `StereoField.hpp`, or `FroggersDspParityTests.cpp` — only `MANUAL.md`/`QUICK_DICT.md`, already covered by task 2.4's hunk-level staging discipline.
- `openspec validate frogg3rs-delay-width-wysiwyg-repair` passes; task count via `openspec list --json` (21) matches manual count of `tasks.md` checkboxes.
- Widened enumeration check for task 1.4: `dwid`/`MapRowsToDelayParams`/router-slot-4 references also appear in `FroggersAudioRoutingTests.cpp` and `FroggersSurfaceTests.cpp`, not only `FroggersDspParityTests.cpp` — but task 1.4's own design (full 12-binary before/after `[PASS]`/`[FAIL]` diff) catches any pin regardless of file, so this is not a gap.
- Operator-ruling citation (`04efe9c`) verified to exist with matching substance.

## Verdict

**PREFLIGHT REJECTS**

1. B1 — `research/` citations are dangling; no task will ever resolve them. Blocks A.1 and the design rationale behind Stage 1.
2. B2 — Task 1.1's check margin is self-referential as written; violates `tasks.md`'s own no-blank-checks rule. Blocks task 1.1.
3. B3 — `HANDOFF.md` is inside this change's own declared sweep scope, is affirmatively wrong (not just stale), and no task addresses it before archival. Blocks task 3.6.


===== BLOCK 2 of 2 (timestamp: 2026-09-14T05:39:31.740Z) =====


## Verification performed before answering

For D1 I read both call sites end-to-end rather than trusting either auditor's characterization:

- `app/dsp/Reverb.hpp`: `valA/valB` are the raw tap reads. The feedback write (`lineA[indexA] = aIn; lineB[indexB] = bIn;`) is computed from `aIn/bIn`, which derive from `tankFeed` and the **cross-fed** `aFb/bFb` — never from `aOut/bOut`. Damping (`dampFilterA.Process(valA)`, `dampFilterB.Process(valB)`) runs on the raw taps and only feeds `wetL/wetR`, which are never written back into `lineA/lineB`. So which damping regime you pick has zero effect on what the tank line ever contains — the "shared" and "split" regimes are two independent post-hoc computations over one shared, regime-invariant tap stream, which is exactly why the test reconstructs both from one real `Process()` call by also peeking `rv.indexA`/`rv.lineA` from outside.
- `app/dsp/Delay.hpp`: `cross` (the value under test) feeds `fed = CrossFeedPair(dL, dR, cross)` → `fbL/fbR`, and those are exactly what gets saturated and written into `lineL/lineR` at `:808-809`. Whichever `cross` law runs changes the line content itself, from the very next sample onward — the two regimes are not two views of one stream, they're two different trajectories.
- Access specifiers: `grep -n "private:"` on `Delay.hpp` shows `private:` at line 924 with `lineL`, `lineR`, `writePos` declared at 1086-1088, after it, no `public:` in between — confirmed private. `Reverb.hpp`'s `struct Reverb` has no access specifier at all, so its `lineA/lineB/indexA/indexB/wetL/wetR` are public by default (default struct access) — confirmed public, and this is exactly what the cited test exploits.

For D2/D3 I grepped `proposal.md`, `tasks.md`, and `research/INDEX.md` for `rmsL`, `rmsR`, `level.balance`, `stated bound` — the only hits are the two documents' own wording; no number appears anywhere, and no instruction says "measure X to set the level-balance bound" (unlike the correlation margin, which at least has "first measure... to set the margin").

For D4 I re-checked every one of the six `research/INDEX.md → <topic>` citations against `proposal.md`'s own body text.

## D1 — CONFIRM (blocking, task 1.1)

Verified independently above: Reverb's damping split is a genuine post-tap, non-recirculating operation reconstructible from public state peeked alongside one real call; StereoDelay's cross-feed is written directly into the private, recirculating `lineL/lineR` it also reads from. The precedent does not transfer — `todayCorr` and `repairedCorr` are not two readings of one run, they are two different line-content trajectories, and `lineL/lineR/writePos` are private so there's no way to peek an alternate trajectory from outside even if you wanted to. Task 1.1 says only "measure... from the same noise run" and names no mechanism for getting two live regimes out of one recirculating, privately-owned buffer. Their three listed ways out are accurate and each conflicts with something else the proposal states (a runtime toggle contradicts "cross becomes a fixed 0.0f"; duplicating the recirculating formula in the test creates a second definition site of `Process()`'s feedback math that must stay in sync forever; freezing today's values turns a live discriminating measurement into a regression pin, a different and weaker instrument than the proposal describes). None of the three is named in the task. **Confirmed blocking on task 1.1** — my own B2 already flagged that task 1.1's check isn't fully specified; this is a more fundamental version of the same underlying gap (the check may not be constructible as literally worded at all, independent of the margin question).

## D2 — CONFIRM (blocking, task 1.1)

Task 1.1's liveness clause reads "liveness (`rmsL`, `rmsR` nonzero) on every row" — one unqualified pair, not clearly bound to *both* whatever computation produces `todayCorr` and whatever produces `repairedCorr`. `Correlation::Value()`'s documented fallback (hardcoded `1.0` on a zero denominator) is exactly the failure mode proposal.md itself warns about generically ("every check using it also asserts rmsL and rmsR nonzero, or it can pass vacuously") — but task 1.1 doesn't say the liveness check must independently cover the today-law side. If whatever mechanism backs `todayCorr` (per D1, necessarily some separate/duplicated computation) goes silent, `todayCorr` reads as `1.0`, which is very likely *larger* than a live `repairedCorr`, so `todayCorr - repairedCorr` grows rather than collapses — the assertion gets easier to pass, not harder, on exactly the failure this instrument is known to be vulnerable to. **Confirmed blocking on task 1.1.**

## D3 — CONFIRM (blocking, task 1.1)

Grepped and found no numeric bound anywhere in `proposal.md`, `tasks.md`, or `research/INDEX.md` for the level-balance companion. This is a plainer instance of the same family as my B2, but worth confirming as distinct because it's *less* scaffolded than the correlation margin: the margin at least has "first measure... to set the margin" language; the level-balance bound has none — not even an instruction to measure one. As written, `tasks.md`'s own stated rule ("No task hands an executor a check without its assertion already written") is violated outright here, not just ambiguously. **Confirmed blocking on task 1.1.**

## D4 — CONFIRM their verdict (revises my B1)

I checked exactly what they say they checked: whether every figure a task needs is duplicated inline. It is. Systematically, for each of the six `research/INDEX.md → <topic>` pointers, the specific number(s) also appear verbatim in `proposal.md`'s own prose:
- cross-feed-widening → the `dwid==0` divergence figures ("950-1035 samples," which weights hold/drift) are inline in "A correction to the record."
- retired-mechanisms → 1408/2552 vs 980/980, `|corr|=0.019` at 220 Hz, the 0.00032–0.99998/mean-0.64 sweep, the 0.125-weight comparison — all inline in "Why zero."
- prior-art-crossfeed-separation → Logic Pro/Bitwig/Valhalla citations — inline in the same section.
- instrument-derivation → the 152.8-vs-164.29-sample figures, the `dtim≥0.8`/1000-sample failure modes — inline in "The instrument."
- side-mid-rejection → the `sqrt((1-rho)/(1+rho))` identity — inline in "Side/mid energy ratio is not used."
- read-at-capacity → the baseSeconds/widthSpread/timeR trace — inline in "A second defect in scope."

And no task (1.1 through 3.7) instructs anyone to open `research/INDEX.md` or any child file — each task cites `proposal.md`'s own section names instead. So my original "what breaks" (task 1.1/1.2's rationale is untraceable) does not hold: nothing in task execution is gated on these files existing.

**I am revising B1 from blocking to non-blocking.** What still genuinely breaks, narrower than I first claimed: `research/INDEX.md`'s own header text is a false claim in the tree right now ("Its files are extracted verbatim from the measurement sessions... by a pass separate from the one that wrote proposal.md" — that pass never happened), and the six citation pointers in `proposal.md` are dead links with no gate that would ever catch them (`check_citations_resolve.py` only scans `app/` comments, confirmed by reading its docstring). That's real but it's a hygiene/consistency defect under §8.0 ("stale... that no longer resolve to what they claim"), not an execution blocker, since the numbers a reader actually needs are sitting right next to each citation in the same document.

## Net effect on B1–B3

- **B1: REVISED — non-blocking.** Confirmed via the inline-duplication check above. Disposition unchanged in kind (still worth fixing before archival, since `research/INDEX.md` ships as part of this change and asserts something false about itself) but no longer gates any stage or task.
- **B2: STANDS, and is now subsumed/sharpened by D1–D3.** All four findings (mine and theirs) are the same root defect — task 1.1 hands an executor a check whose construction and thresholds aren't fully written down — but D1–D3 are more concrete and more severe than my original framing: D1 says the check's core comparison may not be constructible as worded at all; D2 and D3 name two specific unguarded/unset values. Recommend treating D1, D2, D3, and my original B2 as one combined blocking item against task 1.1, to be resolved together before an executor touches it.
- **B3: UNCHANGED.** Neither auditor's findings bear on `HANDOFF.md`; it stands as originally reported (blocking, task 3.6).
