# Preflight 3 — preflight on the first rewrite

# PREFLIGHT AUDIT — `frogg3rs-delay-width-wysiwyg-repair`

Read `omni-rule.md` in full. Read the rewritten `proposal.md`, `tasks.md`, and the (unchanged) `specs/froggers-sheaf-parameter-model/spec.md` in full. Read the relevant source (`app/dsp/Delay.hpp` Process() end to end, `app/dsp/DspMath.hpp::ExpMapCompute`, `app/dsp/Reverb.hpp` around its width blend and header comment), ran the three named gate scripts, `git log`/`git show` on the cited commits, and grepped `app/FroggersDspParityTests.cpp` and `app/FroggersAudioRoutingTests.cpp` for every pin touching `StereoDelay::Process`'s output. I did not edit any file.

## Findings 1–7 (own arithmetic / own citations)

**1. Finding 4's identity claim — `rms(side)/rms(mid) = sqrt((1-rho)/(1+rho))`.**
Derived from `mid=0.5(l+r)`, `side=0.5(l-r)`: `E[mid^2]=0.25(a^2+b^2+2*rho*a*b)`, `E[side^2]=0.25(a^2+b^2-2*rho*a*b)` where `a,b` are the two channels' RMS and `rho` their (raw, zero-mean) correlation. At `a=b`: ratio² = `(1-rho)/(1+rho)`. **Holds, exactly as stated.** At unequal levels: ratio² = `(1-rho*k)/(1+rho*k)` with `k=2ab/(a^2+b^2)` — **exactly the weighting the proposal states**, and `k<=1` by AM-GM, so it does push the ratio toward 1 regardless of `rho`. **One gap**: this derivation requires `rho` to be a *zero-mean* correlation (raw cross-moment normalized by RMS); with a DC-carrying pair, Pearson correlation (mean-subtracted) and this identity diverge. The proposal states the identity "exactly" without this caveat. Non-blocking: Finding 4's conclusion is "side/mid is not used as this change's instrument," and that conclusion is unaffected either way.

**2. Finding 4's back-calculation** (ratio 0.7663, rmsL 0.2448, rmsR 0.2123 → true rho ≈0.26). Computed `k=2ab/(a²+b²) = 0.9899`, `ratio²=0.5872`, solved `(1-rho·k)/(1+rho·k)=0.5872` → `rho·k=0.2601` → `rho≈0.2627`. **Reproduces "about 0.26."** The comparison figure "today's 0.034" is an external measurement I could not re-derive from the three given inputs alone (it isn't one of them); arithmetic on what *was* given checks out.

**3. Finding 5's numbers.** `ExpMapCompute(0.001, 2.0, 0.3) = 0.001·(2000)^0.3 = 0.001·9.7797 ≈ 0.0097797 s`. Matches "0.009779 s." `widthSpread_max = 1·0.0097797·0.35 = 0.0034229 s = 3.42 ms = 164.3 samples @48kHz`. **Both figures confirmed**, cited to `app/dsp/Delay.hpp:669,679` and `kMaxDelaySeconds` at the `StereoDelay` struct top.

**4. Finding 5's central reasoning.** White noise autocorrelation width ~1 sample is correct (it's a delta function). For band-limited noise of bandwidth `B`, autocorrelation width is order `1/B` (standard time–bandwidth reciprocity). At `B≈300 Hz`, `1/B≈3.3 ms` — the **same order** as the 3.42 ms max offset. **The reasoning holds and is not overstated** (it says "near," "same order," "a few hundred Hz," never an exact match). **This is the load-bearing claim and it survives arithmetic scrutiny — no loud rejection warranted here.**

**5. Finding 2.** `fbk=clamp(dfbk,0,0.98)=0` at `dfbk=0`; `freeze=clamp(dfrz,0,1)`, Freeze's registered default is `0.0f` (`app/FroggersParameters.hpp:264`, no third literal = default 0, matching Stereo width's own convention). `FreezeFeedback(0,0,false)=0+(1-0)*0=0`, so `fbEff=0` (`app/dsp/Delay.hpp:805-807`). The write (`:808-809`) is `fbEff * Saturate(...)` = 0 regardless of the crossed pair — confirmed. The wet tap (`lastWet.l/.r`, `:851-857`) is built from `diffusedL/diffusedR`, derived from `dL/dR` (`:683-684`, read pre-cross), **never** from `fed.a/fed.b` (`:726-728`, the crossed pair, which feeds only `WriteSample`). **Both halves of Finding 2 confirmed by line citation.**

**6. Finding 6.** `lfoPhase` updated once (`:670-674`), `modSeconds` computed once from it (`:675`), and added into **both** `timeL` (`:680`) and `timeR` (`:681`). **Confirmed exactly as stated.**

**7. The requirement over-reach — Reverb's Stereo width "not chosen on design grounds."** `app/dsp/Reverb.hpp:807-813`: `width = widthKnob01; // :460, direct passthrough` — a verbatim firmware port — and the comment at `:810-813` states plainly: "wetL/wetR used to be summed here, which made the Width control above mathematically inert... Keeping the pair is what makes it a control." Commit `43856d1` ("Stereo is plumbing, not computation, and both Width knobs are inert") confirms the only local decision was *not summing* to avoid inertness, not a chosen mid/side design. **The claim is supported, not overstated.**

## §9 checklist, run mechanically

**§1 trace completeness — constraint map items 1–6, all six checked by reading:**
- Item 1 (CrossFeedPair shared): confirmed — `Delay.hpp:52` and `Reverb.hpp:91,705` both reference/call it; Reverb's tank call passes a **literal 0.0** weight (`Reverb.hpp:705`), matching spec.md's "fixed coupling" claim.
- Item 2 (`cross_feed_pair_is_identity_...`): confirmed — calls `dsp::CrossFeedPair` directly, never touches `StereoDelay`.
- Item 3 (recapture target, not a constraint): confirmed by the test's own comment ("captured by running this same fixture against `dsp::StereoDelay::Process` before that de-duplication").
- Item 4 (no firmware parity): confirmed — `find src -iname "*Delay*"` returns nothing; `git show f236915^:sim/StereoDelay.hpp` line 87 is `const float cross = p.dwid * 0.5f;` verbatim.
- Item 5 (no-op at dwid==0): structurally confirmed by reading (`widthSpread=0` ⇒ `timeL==timeR`; `CrossFeedPair(x,x,w)={x,x}` for any `w`). The "confirmed bitwise over 3200 samples" clause is an unread runtime claim by me specifically, but it is backed by sound structural reasoning I did verify and by the 394/0 baseline; non-blocking.
- **Item 6 (SEVEN pins) — DEFECT.** I enumerated independently (all `TEST_CASE`s constructing `DelayParams`/routing through the Delay bank with **both** `dwid≠0` and the feedback path live — `dfbk≠0`, `dfrz≠0`, or `dfrzLatched`, since Finding 2 shows the cross-feed is inert otherwise). The constraint map's list (Freeze family, `stereo_delay_default_knob_values_reproduce_original_output_exactly`, `stereo_delay_width_balance_mapping_...`, the diffusion cases, the reverse-blend cases, `delay_stereo_width_produces_a_stereo_image`) **omits three qualifying tests**, none named anywhere in `proposal.md` or `tasks.md`:
  - `delay_feedback_loop_stays_bounded_at_max_feedback` (`dfbk=1.0`, `dwid=0.3`, `FroggersDspParityTests.cpp:5726`)
  - `delay_wet_output_stays_at_or_below_limiter_ceiling_at_max_feedback` (`dfbk=1.0`, `dwid=0.3`, `:5786`)
  - `stereo_delay_feedback_drive_at_maximum_does_not_raise_the_per_sample_bound` (`dfbk=1.0`, `dwid=0.3`, `:7070/7079/7080`)
  
  All three assert only loose bounds (`|wet| <= bound+eps`), which a convexity-preserving repair (spec.md's own requirement: cross-feed weight stays in `[0,1]`) will very likely continue to satisfy — so the *risk* of breakage is low — but the enumeration itself, which the proposal states as complete ("Enumerate all of them before editing. Do not discover any of them from a red build"), **is not complete**. **DEFECT, non-blocking for task 1.1 (measurement only), blocking for tasks 1.2/1.4** — the constraint map must be corrected before the repair or its recapture lands. Disposition: report to coordinator for a proposal.md correction; task 1.4's own instruction to "enumerate every other pin... FIRST" independently at execution time is a partial safety net but does not excuse the proposal's own inaccurate claim.

**A proposal states only what has been read:** no other unverified behavioral claims found beyond the two already flagged (Finding 4's unstated zero-mean caveat, non-blocking; item 5's "3200 samples" runtime claim, non-blocking given structural support + green baseline).

**§5 enumeration on every named concept the plan creates:**
- "Band-limited noise stimulus" (generator): **FOUND=0** existing generator anywhere in `app/` — genuinely new, no duplication.
- "Band-limited energy check" hits at `FroggersAudioRoutingTests.cpp:199,2752` are a **different, pre-existing concept** (measuring energy in a band, not generating a band-limited stimulus) — correctly distinct, no overlap.
- "Pearson correlation of the wet pair" / `struct Correlation`: **FOUND=1**, `app/FroggersDspParityTests.cpp:9487` — exists exactly as claimed; proposal correctly reuses it rather than duplicating.
- "Level-balance companion assertion" (`|rmsL-rmsR|/(rmsL+rmsR)`): **FOUND=0** anywhere in `app/` — genuinely new, nothing to de-duplicate against.
- No orphaned scratch code from the three measurement rounds was found in the tree (`git status --short` clean apart from this change's own two modified files and the two held directories).

**Every `Check:` naming a test resolves:** ran `python3 app/check_spec_checks_resolve.py app` — exactly the two pre-established failures (`frogg3rs-randomize-depth-reclaim` spec lines 15, 21), **zero attributable to this change's own spec delta**. Also ran `check_docs_match_parameter_table.py app` → `OK - MANUAL.md 84/0, QUICK_DICT.md 84/0` (matches proposal's own citation). Also ran `check_modified_requirements_restate_promoted.py app` → `OK`, with one `NOTE` attributable to the unrelated held `frogg3rs-midi-controller-resilience` change, not this one.

**Enumerate other active changes, overlap per change:**
- `frogg3rs-randomize-depth-reclaim` (untracked, held): **0 code overlap** beyond the already-named gate exclusion.
- Sheaf submodule (9 active openspec changes: `bank-addressed-absolute-write`, `app-midi-catalog`, `shorten-deadline-readout-window`, `shift-and-file-export`, `ui-state-before-audio`, `launchpad-model-on-the-row`, `fix-out-of-tree-app-gaps`, `rework-controllers-block-editing`, `browser-slider-value-readout`, `fix-task-analyzer-plan-derived-tasks`): **0 overlap** — grepped for `Delay.hpp`, `Stereo width`, `CrossFeedPair`, `StereoDelay`, `froggers-sheaf-parameter-model` across all of them, zero hits. Different app entirely.
- `frogg3rs-midi-controller-resilience` (untracked, held): **DEFECT, not previously surfaced.** Its own `tasks.md` (7.1, 7.3) plans edits to `MANUAL.md:319-320/336-339` and `QUICK_DICT.md` (MIDI stuck-Shift text and settings table) — **different sections** of the same two files this change's Stage 2 (2.1/2.2) edits, but the **same files**, in what appears to be one shared working tree with no worktree isolation between sessions. Since `git add MANUAL.md` (as Stage 2's commit will do) stages the *entire* file's working-tree state, a concurrently-active midi-controller-resilience session with uncommitted edits to those same files would be silently swept into this change's commit. This is not mitigated by the existing "never `git add -A`" rule, because `MANUAL.md`/`QUICK_DICT.md` are paths this change legitimately names. **Non-blocking for Stage 1; must be named and mitigated (e.g., diff-review the two files before staging, or confirm the other session is dormant) before Stage 2 executes.**
  - Also found, informationally: `frogg3rs-midi-controller-resilience/preflight-3.md:246` cites a stale git-status snapshot naming `frogg3rs-stereo-field-and-density` (a superseded ancestor of this change) as holding `Delay.hpp`/`Reverb.hpp`/etc. modified — current `git status` shows no such modifications. That document belongs to the other session; not ours to edit, but worth relaying since it may be misleading that session too.

**A plan that changes leaves its other artifacts stale — `HANDOFF.md`:** **BLOCKING DEFECT.** `HANDOFF.md`'s "The open question, which is task 1.1" section states verbatim the pre-rewrite framing — "Can the cross-feed widen this signal at all? ... which re-correlates, reaching exact mono at weight 0.5" — that `proposal.md`'s own "READ THIS FIRST" explicitly says was **refuted** by three measurement rounds ("Do not re-run these rounds"). The rewritten `tasks.md` task 1.1 asks a different, corrected question (does the *read-time offset alone*, under a *corrected band-limited stimulus*, widen monotonically). `HANDOFF.md` was not updated after the rewrite and directly contradicts it, telling a fresh reader to go re-open a question already closed — precisely the failure mode both `HANDOFF.md` and the proposal warn against. Must be rewritten to match the current task 1.1 before Stage 1 is dispatched.

**A task handing an executor a check without its assertion written — read all of stages 1–3:** Only task 1.2 carries an intentional blank (the law/bandwidth/`dtim`/Feedback values/bound, pending 1.1's results). Unlike the previous audit's finding #2, the blank is now **fully enumerated** ("naming, explicitly: the law, the stimulus bandwidth, the `dtim`, the Feedback values each asserted row uses, the monotonicity the image assertion demands, and the numeric bound") and gated ("Do not dispatch 1.2 with this paragraph unfilled"). **Previous finding #2 is resolved.** No other task in stages 1–3 hands off an unfilled check.

**Scope consistency, Impact vs. tasks.md:** Every file Impact names as edited (`Delay.hpp`, `FroggersDspParityTests.cpp`, `MANUAL.md`, `QUICK_DICT.md`, `frogg3rs.code-workspace`) is referenced by a task (1.2/1.4/1.5, 1.4, 2.1, 2.1, 2.3 respectively); nothing in Impact is unreferenced. Verified `frogg3rs.code-workspace`'s `files.watcherExclude` directly: 4 entries, `**/wasm/build/**` and `**/desktop/build/**` are stale (`wasm/`, `desktop/` do not exist at repo root — confirmed), the other two (`**/node_modules/**`, `**/.emsdk/**`) do resolve (`.emsdk/` at root; `node_modules` nested under `node-v22.16.0-darwin-arm64/lib/`, matched by the `**` glob). **Task 2.3's claim is accurate, nothing needed.** No task touches `Reverb.hpp` or `StereoField.hpp` (explicitly forbidden in 1.2); consistent with Impact's out-of-scope declaration.

**Previous audit's finding #1 (the seventh pin):** the specific test (`delay_stereo_width_produces_a_stereo_image`) is now named, resolving that finding's literal ask — but see the SEVEN-count defect above: the broader completeness claim is still short by at least 3.

## Disposition summary

| Item | Disposition |
|---|---|
| Findings 1,2,3,5,6,7 | Verified correct by arithmetic/citation — nothing needed |
| Finding 4 identity/back-calc | Correct arithmetic; unstated zero-mean caveat — non-blocking, note for the record |
| Finding 5 stimulus reasoning | **Sound** — the change's central premise holds |
| Constraint map items 1–5 | Verified by reading — nothing needed |
| Constraint map item 6 (SEVEN pins) | **DEFECT** — undercounts by ≥3 named tests; blocking for 1.2/1.4, not for 1.1 |
| §5 concept enumeration | All FOUND/CHANGED counts checked — nothing needed |
| `Check:` resolution / gates | All three gates run; match claimed state — nothing needed |
| Active-change overlap: Sheaf | 0 overlap — nothing needed |
| Active-change overlap: randomize-depth-reclaim | 0 overlap beyond known exclusion — nothing needed |
| Active-change overlap: midi-controller-resilience | **DEFECT** — unflagged MANUAL.md/QUICK_DICT.md file-level collision risk; blocking for Stage 2, not Stage 1 |
| `HANDOFF.md` staleness | **BLOCKING DEFECT** — restates the refuted framing as the open question |
| Task 1.2 blank | Previously flagged, now correctly enumerated and gated — resolved |
| Impact vs. tasks.md scope | Consistent — nothing needed |
| `frogg3rs.code-workspace` claim (2.3) | Verified accurate — nothing needed |

## Verdict

**PREFLIGHT REJECTS**, with defects:

1. **BLOCKING** — `HANDOFF.md` restates the pre-rewrite, refuted framing of the open question as if it were still live, directly contradicting `proposal.md`'s own "Do not re-run these rounds." Must be rewritten to match the current task 1.1 before Stage 1 is dispatched.
2. **BLOCKING for tasks 1.2/1.4 (not for 1.1)** — constraint map item 6's "SEVEN pins" enumeration is incomplete: `delay_feedback_loop_stays_bounded_at_max_feedback`, `delay_wet_output_stays_at_or_below_limiter_ceiling_at_max_feedback`, and `stereo_delay_feedback_drive_at_maximum_does_not_raise_the_per_sample_bound` all run with the cross-feed live (`dwid≠0`, `dfbk≠0`) and are named nowhere in this change. Correct the constraint map before the repair or recapture lands.
3. **BLOCKING for Stage 2 (not Stage 1)** — `frogg3rs-midi-controller-resilience`'s planned edits to `MANUAL.md`/`QUICK_DICT.md` share files (different sections) with this change's Stage 2 in what appears to be one shared working tree; name and mitigate the staging risk before 2.1/2.2/2.4 commit.
4. **Non-blocking** — Finding 4's identity claim needs a zero-mean caveat for full rigor; does not change the finding's conclusion (side/mid is not the instrument).

Task 1.1 itself (measurement only, no production change, self-contained methodology) is not blocked by defects 2 or 3 and may proceed once defect 1 is corrected. Finding 5's stimulus reasoning — the premise the whole rewrite depends on — checks out arithmetically and is not the cause of this rejection.
