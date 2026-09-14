# Preflight 4 — preflight on the repairs to that rewrite

# Preflight Audit — `frogg3rs-delay-width-wysiwyg-repair` (repair-of-repairs pass)

Read `omni-rule.md` in full first. Independent enumeration performed by running commands directly against the tree (no reliance on any prior session's report). Scratchpad script used: a small Python pass over `app/FroggersDspParityTests.cpp` matching `TEST_CASE` boundaries against `.dwid=`, `.dfbk=`, `.dfrz=`, `dfrzLatched`, `.Process(` — cross-checked by a second, differently-shaped search on `MapRowsToDelayParams(` call sites and a third on `RequestRandomizeAll()`.

## Repair 1 — proposal.md constraint map item 6 (the criterion)

**My independent enumeration, for diffing against the document's list:**

`app/FroggersDspParityTests.cpp` (dwid nonzero AND feedback path live AND reaches `Process`):
1. `delay_feedback_loop_stays_bounded_at_max_feedback`
2. `delay_wet_output_stays_at_or_below_limiter_ceiling_at_max_feedback`
3. `stereo_delay_feedback_drive_at_maximum_does_not_raise_the_per_sample_bound`
4. `stereo_delay_default_knob_values_reproduce_original_output_exactly`
5. `stereo_delay_diffusion_at_maximum_differs_measurably_from_no_diffusion`
6. `stereo_delay_diffusion_state_tracks_the_signal_while_ddif_is_zero`
7. `stereo_delay_diffusion_midpoint_differs_from_both_endpoints`
8. `stereo_delay_diffusion_does_not_raise_wet_output_beyond_limiter_ceiling`
9. `stereo_delay_reverse_blend_midpoint_differs_from_both_endpoints`
10. `stereo_delay_reverse_state_tracks_the_signal_while_drev_is_zero`
11. `stereo_delay_reverse_blend_does_not_raise_wet_output_beyond_limiter_ceiling`
12. `stereo_delay_cross_feed_reproduces_its_captured_output_exactly` (non-literal, `p.dwid = c.dwid`)
13. **`stereo_delay_diffusion_at_default_zero_is_bit_identical_to_no_diffusion`** — line 7174, `MapRowsToDelayParams(/*feedback=*/0.4f, /*width=*/0.25f, ...)`, calls `.Process()` 2000x. **Not in the document's list.**
14. **`stereo_delay_reverse_blend_at_default_zero_is_bit_identical_to_no_reverse_blend`** — line 7413, same pattern (`feedback=0.4`, `width=0.25`). **Not in the document's list.**
15. **`stereo_delay_freeze_at_default_reproduces_pinned_original_output_through_real_process`** — line 7850, `feedback=0.6`, `width=0.25`, 500-sample loop calling `.Process()`. **Not in the document's list.**

`app/FroggersAudioRoutingTests.cpp` (production router, `FroggersBankId::Delay, 4`):
16. `delay_stereo_width_produces_a_stereo_image` — matches the document.

Correctly excluded by the document's own disposition (verified): `stereo_delay_width_balance_mapping_keeps_cross_in_0_1_and_spread_at_or_below_todays_max` never calls `Process` — confirmed by reading it; it only computes `cross`/`spread` arithmetically. Good disposition, not a defect.

**(a) Is the criterion correct?** The *criterion's wording* ("reaches `Process` with `dwid` nonzero AND feedback live") is right. The *enumeration method* the document specifies to answer it — "literal assignments to `dwid`, non-literal assignments to `dwid`, and the production router's literal `FroggersBankId::Delay, 4`" — is not exhaustive, and I confirmed a concrete miss, not a hypothetical one:

- **A fourth assignment channel exists and is untested by either pass**: `dsp::MapRowsToDelayParams(...)` is called by name/positional argument at 6 sites in `FroggersDspParityTests.cpp` (grepped separately as a distinct operand). Three of them (items 13–15 above) pass a nonzero `width` argument with feedback/freeze live and then call `.Process()` in a loop — no `.dwid` token appears anywhere in these call sites, so neither of the document's two `dwid` passes can find them.
- **A fifth channel, lower materiality but real**: `RequestRandomizeAll()` drives every bank's knobs (`app/FroggersModulation.hpp:1585`, `detail::RandomizeBankValues`) including Delay's Width/Feedback/Freeze, through the real production router, then runs audio blocks. On a continuous draw, `dwid==0` exactly has probability ~0. Confirmed reaching `Process` with dwid nonzero and feedback live: `randomize_all_storm_test_never_blows_out_or_permanently_silences` (200 draws), `randomize_then_reset_hold_is_reported_per_band_against_a_pristine_decay`, `pristine_and_reset_arms_compared_over_many_draws_with_a_silence_capable_instrument`, `reset_reproduction_re_armed_across_the_curve_and_grace_grid`, `reseeded_and_unreseeded_reset_are_compared_field_by_field`, `the_two_reset_arms_are_compared_while_the_smoothed_path_is_still_walking` (all `app/FroggersAudioRoutingTests.cpp`), `randomize_all_request_through_process_frame_updates_the_display`, `randomize_all_with_ample_capacity_reports_not_partial` (`app/FroggersHeadlessTests.cpp`). These assert generic invariants (finite/bounded/display-moved), not width-specific behavior, so they are weak pins — but they structurally satisfy the criterion as written and are omitted.
- **Freeze-alone opening the path with width nonzero**: not found anywhere (confirmed by the same sweep) — this matches the document's own disclosed gap ("The Freeze half... is traced but pinned by nothing"), so that omission is honest, not silent.

**(b) Does the found list match?** No. Items 13–15 are real, deterministic, `Process`-calling fixtures the criterion returns and the list omits — the same class of thing already in the list (parity/regression pins), found via a search pattern the document didn't run. This is not a materiality-borderline miss like the RNG class; these are exactly the kind of pin task 1.4 exists to protect.

**(c) Is a criterion the right form, and could a gate do this mechanically?** Criterion-over-count is the right call in principle (the preamble's own rule). But a *grep-based* criterion inherits grep's blind spot: it can only enumerate operand spellings someone thought to search, and this file has at least two more input channels (constructor-argument, RNG) than the document tried. A **mechanically stronger deliverable exists and is cheap**: instrument `StereoDelay::Process` (debug build only, compiled out otherwise) to record, per invocation, whether `dwid!=0 && (dfbk!=0 || dfrz!=0 || dfrzLatched)`, tag the record with the active `TEST_CASE` name (the test runner already knows this), and dump the set at the end of a run. That finds every channel — literal, variable, constructor-argument, RNG — with no operand-spelling risk, and it can be re-run and diffed on every future change to this file, closing exactly the "count rots" problem the document is trying to solve. I recommend this over another grep pass.

## Repair 2 — Finding 4's zero-mean caveat

The caveat's math checks out: with `side=(L−R)/2`, `mid=(L+R)/2`, `rms(side)^2/rms(mid)^2 = (E[L²]−2E[LR]+E[R²])/(E[L²]+2E[LR]+E[R²])` reduces to `(1−ρ)/(1+ρ)` only when `E[L]=E[R]=0`, because that's the step that lets `E[L²]=Var(L)` and `E[LR]=Cov(L,R)`. A nonzero mean breaks exactly this step — the caveat is correctly stated and does not contradict the surrounding text (Finding 4's conclusion — don't use side/mid — holds either way, and the caveat doesn't claim otherwise).

**Is DC reachable at the tap point?** Traced `StereoDelay::Process` end to end (`app/dsp/Delay.hpp:653-862`): `ReadAt` (interpolated buffer read) → optional reverse-blend (linear mix) → cross-feed (linear mix) → feedback write through `Saturate`/tone-filter/crush (odd-ish nonlinearities, none DC-blocking) → Diffusion (allpass sections, unity gain at all frequencies **including DC**) → `wetLimiter` (envelope-following gain scalar, not a DC blocker). **No stage in this chain removes DC.** And the fixed instrument (band-limited noise) generically carries nonzero *windowed* mean even when its infinite-time expectation is zero — that's the defining behavior of low-frequency-heavy noise. So **the caveat is load-bearing, not pedantic**, and the document should say so explicitly rather than leaving it general. Non-blocking in practice only because the *actual* chosen statistic (Pearson correlation, `struct Correlation`) is mean-subtracted by definition and therefore immune — but that connection isn't stated, and the audit brief specifically asked for it. Recommend one added sentence: "the wet pair here can carry DC (traced, no DC-blocking stage in `Process`); this is why the instrument uses Pearson correlation rather than the raw identity."

## Repair 3 — tasks.md task 2.4 (shared-tree hazard)

First traced whether "same working tree" is even true, since `.claude/worktrees/midi-controller-resilience/` exists as a separate, locked git worktree on branch `worktree-midi-controller-resilience`. Checked its content: that worktree's own `openspec/changes/` holds `frogg3rs-density-documents-and-spec` (a superseded predecessor gone from main) and `frogg3rs-midi-preset-preconditions` (a different name) — **not** `frogg3rs-midi-controller-resilience`. The live held change's actual files sit as the untracked directory directly in the main tree (confirmed via `git log --oneline --all -- openspec/changes/frogg3rs-midi-controller-resilience/tasks.md` returning nothing — genuinely never committed anywhere). So task 2.4's "same working tree" premise **is correct**; the nested worktree is unrelated stale debris outside this change's Impact (flagging as a hygiene item below, not a defect of this change).

Given that, judging the mitigation itself:
- **(a) Already-staged foreign edit**: plain `git diff` (no `--cached`, no `HEAD`) compares working tree to the *index*. If the other session already ran its own `git add MANUAL.md`, its hunks are in the index and **`git diff` will show nothing for them** — the mitigation is blind to exactly this case. Confirmed gap.
- **(b) Commit landing between review and this stage's own commit**: nothing in task 2.4 re-checks immediately before `git commit`; the review and the stage's commit are separated by the rest of stage 2's work. This is a real, if narrow, TOCTOU window, unaddressed.
- **(c) Stronger mechanism exists**: `git diff HEAD -- MANUAL.md QUICK_DICT.md` (catches staged+unstaged) reviewed via `git add -p` (stages per-hunk at the same moment as the read, rather than read-then-blindly-`git add`-the-whole-file) closes (a). A re-run of the same check immediately before `git commit` (not just once, earlier in the stage) narrows (b). Recommend both edits to task 2.4.

Renumbering check: grepped `2.4`, `2.5`, `task 2.` across `proposal.md`, `tasks.md`, `HANDOFF.md`, and `specs/froggers-sheaf-parameter-model/spec.md`. Only one cross-reference exists (`HANDOFF.md:129`, "task 2.4") and it correctly points at the *current* 2.4 (the hazard task). No dangling old-numbering found.

## Repair 4 — HANDOFF.md (rewritten whole)

Read in full against `proposal.md`/`tasks.md`. Structurally consistent: the six "do not re-derive" items map onto constraint-map items 1, 3, 4, 5, 6, and the separate "parent defect" section (item 2 — the `cross_feed_pair_is_identity...` test — is omitted from this list, but that item is purely informational in the proposal, not an instruction, so its absence isn't a contradiction). The Findings restatement (1–6) matches the proposal's Findings without introducing new figures — good adherence to "a figure is not written into prose."

One claim I traced and confirmed: "The parallel defect in the reverb tank was repaired in the same lineage" — verified via `git log`/`git show` (commits `04efe9c`, `43856d1`) and `app/dsp/Reverb.hpp:810`'s own comment ("wetL/wetR used to be summed here, which made the Width control above [inert]") — the Reverb fix genuinely shipped. Not a defect.

One claim I could **not** verify and flag as an unsupported historical assertion: "Every audit rejection in this lineage hit prose... Not one ever rejected DSP code." The five superseded predecessor changes (`frogg3rs-wysiwyg-controls`, `-wysiwyg-finish`, `-wysiwyg-deliver`, `-stereo-field-and-density`, `-density-documents-and-spec`) exist nowhere in the working tree or `openspec/changes/archive/` — not even in git history as directories (confirmed by `git log --all -- <path>` returning results only for their `proposal.md` edits, not any recorded rejection). This sweeping claim about the whole lineage's rejection history is untraceable from source and reads as an assertion "no possible check could refute or confirm cheaply" here — per §9, that's not a caveat, it's an unverified claim. Non-blocking (it's framing prose, not a task instruction), but should be softened or removed.

## The four consistency checks

**Internal consistency after the edits — found one blocking defect.** `tasks.md` task 1.4 (line 127) still reads: *"The proposal's constraint map item 6 names SEVEN, including `delay_stereo_width_produces_a_stereo_image`..."* — but the **repaired** `proposal.md` constraint-map item 6 no longer names a count at all; it explicitly replaces "six" and "seven" with a criterion and a found-list stated to be "a floor, not a ceiling" (and, per Repair 1 above, the true count is at least 15-16, not seven). This is a direct, confirmed contradiction between two artifacts of this same change — the exact "family that drifted apart" failure mode the omni-rule preamble describes. Grepped case-insensitively for "seven"/"six pin" a second time across both files to confirm this is the only stale occurrence — it is.

Everything else checked clean: Finding-number citations in tasks.md (1.1→Findings 5/6, 1.2→Finding 2, 1.3→Finding 1) all match their content; no other task-number citations found anywhere that could be stale.

**Impact vs. tasks.** Every file Impact names (`app/dsp/Delay.hpp`, `app/FroggersDspParityTests.cpp`, `MANUAL.md`, `QUICK_DICT.md`, `frogg3rs.code-workspace`) is touched by a named task (1.2/1.5, 1.2/1.4, 2.1/2.2, 2.1/2.2, 2.3), and no task touches anything outside Impact. `Reverb.hpp`/`StereoField.hpp` are named out-of-scope in Impact and reinforced by an explicit "do not edit" in task 1.2. Clean.

**Gates** (run directly):
- `check_spec_checks_resolve.py`: **FAIL**, exit 1, exactly 2 unresolvable names, both in `frogg3rs-randomize-depth-reclaim/specs/froggers-modulation-slate/spec.md` (`randomize_storm_cost_returns_to_baseline_after_reset`, `reclaim_does_not_recycle_a_pinned_drill_in_depth`) — matches the known exclusion exactly, count has not grown.
- `check_docs_match_parameter_table.py`: OK, MANUAL.md 84/0, QUICK_DICT.md 84/0.
- `check_modified_requirements_restate_promoted.py`: exit 0 (pass). Prints one NOTE about `frogg3rs-midi-controller-resilience`'s own spec delta (a scenario not restated) — that's the other held change's own content, not this change's; the "4 MODIFIED requirement(s)" total is 3 (ours) + 1 (theirs), consistent.
- `check_no_planning_history.py`: OK, 57 files, none.
- `check_citations_resolve.py`: OK, 0 unresolvable.
- `openspec validate frogg3rs-delay-width-wysiwyg-repair` and `openspec validate --changes`: all three changes (including the two held ones) valid.

No gate shows a defect attributable to this change.

## Hygiene observation outside this change's Impact (not blocking)

`.claude/worktrees/midi-controller-resilience/` is a separate, **locked** git worktree whose checked-out branch (`worktree-midi-controller-resilience`) is stale relative to `main` (its `MANUAL.md` predates commit `b05e83a`) and whose own `openspec/changes/` content doesn't match its directory name (`frogg3rs-density-documents-and-spec`, a directory gone from main, and `frogg3rs-midi-preset-preconditions`, an unrelated name). It sits under `.claude/`, which this change's Impact doesn't name, so it's not this change's to fix — flagging for the operator's awareness only.

---

**PREFLIGHT REJECTS**

1. **BLOCKING — Stage 1, task 1.4.** `tasks.md:127` still asserts "constraint map item 6 names SEVEN," directly contradicting the repaired `proposal.md` constraint map item 6 (criterion-based, explicitly "a floor, not a ceiling," true count ≥15). Fix: update task 1.4's text to match the repaired proposal before Stage 1 executes.
2. **BLOCKING — Stage 1, task 1.4.** The enumeration method proposal.md item 6 and tasks.md 1.4 both specify (two `dwid`-assignment passes + one production-router literal pass) is confirmed incomplete: 3 deterministic, `Process`-calling `TEST_CASE`s reach `Process` with `dwid` nonzero and feedback live via `MapRowsToDelayParams(...)` constructor arguments and are omitted from both documents — `stereo_delay_diffusion_at_default_zero_is_bit_identical_to_no_diffusion`, `stereo_delay_reverse_blend_at_default_zero_is_bit_identical_to_no_reverse_blend`, `stereo_delay_freeze_at_default_reproduces_pinned_original_output_through_real_process`. If task 1.4 runs the stated method as written, it will not find them. Fix: add a fourth operand pass (search `MapRowsToDelayParams(` call sites) before task 1.4 executes, or replace the enumeration with the mechanical instrumentation approach described under Repair 1(c).

Non-blocking, for the record and for whoever next touches these documents:
3. The same three-pass method also misses 8 RNG-driven `TEST_CASE`s (`RequestRandomizeAll()`-based) that structurally satisfy the criterion but assert generic invariants unlikely to be sensitive to the repair; covered independently by the stage's suite-green gate regardless.
4. Repair 2's caveat is correct but incomplete: it should state that DC is in fact reachable at the tap point (traced, no DC-blocking stage in `Process`) and that this is why Pearson correlation, not the raw identity, is the actual instrument.
5. HANDOFF.md's claim that no rejection in this lineage ever hit DSP code is untraceable from the current tree/history and should be softened.
6. Task 2.4's mitigation should use `git diff HEAD --` (or `git add -p`) instead of plain `git diff`, and re-check immediately before the stage's commit, to close the staged-edit and TOCTOU gaps identified above.
