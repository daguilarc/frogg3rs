# Preflight 11 — terminating executability check, first run

## Answer to the first question: NO

Tasks 1.1a and 1.1b as written cannot be executed without inventing a value.

**The guess an executor would have to make:** neither task, nor `proposal.md`'s "The check" / "The instrument" sections, ever names a value for Send (`DelayParams::dsnd`). `dsp::StereoDelay::Process` (`app/dsp/Delay.hpp:655`) opens with `if (p.dsnd <= 0.0001f || capacity == 0) { lastWet = {}; ...; return lastWet; }` — Send's registered default is closed (0.0, confirmed at `app/FroggersParameters.hpp:263` where the Delay bank's `{"Send","Send"}` tuple carries no override, and stated directly in the spec text this same change carries: "Reverb and Delay read as transparent at rest because their Sends default closed," `specs/froggers-sheaf-parameter-model/spec.md:329`). An existing test proves the consequence directly: `stereo_delay_send_at_or_below_threshold_returns_silence_and_freezes_state` (`app/FroggersDspParityTests.cpp:5552`) asserts `dsnd == 0` returns `wet.l == 0.0f && wet.r == 0.0f` for 500 samples regardless of every other knob.

So run 1.1a's "pinned instrument" exactly as written and every grid point is dead: `rmsL == rmsR == 0` at every row (not only width 0), and `Correlation::Value()`'s zero-denominator fallback (`app/FroggersDspParityTests.cpp:9500`) reads every row as `|corr| = 1.0`. That contradicts the proposal's own cited prior measurement ("the repaired law measures `|corr| = 0.240`... today's shipped law measures `0.370`" at Feedback 0.7 full width, `proposal.md:161`), which could only have been produced with Send open — meaning whoever ran that measurement fed the delay, and simply never wrote the value into either document. An executor following only what 1.1a/1.1b name would either (a) reproduce a dead instrument that cannot set a discriminating bound, contradicting the sanity-check step that requires "at least one grid point exceeds it," or (b) silently pick a Send value (1.0? 0.8?) to make the instrument work — inventing exactly the kind of unstated decision §2 and the "no invented value" mechanics rule forbid. `SetSampleRate(48000.0f)` has the same gap but is not a blocker: it is unambiguous boilerplate (every test in the file uses it, and the 48000 constant is already baked into the pinned instrument's own alpha formula), unlike Send, which is a genuine knob with no single value implied by anything named.

This blocks 1.1a directly (its own MEASURE step produces no real numbers) and 1.1b directly (its landed check reuses "the pinned instrument" at the same operating point).

## The ten repairs

1. **Landed** — Stereo width grid `{0.0,0.25,0.5,0.75,1.0}` and width-0 exclusion, `tasks.md:60-73`. Matches the cited spec grid (`spec.md:106-111`) verbatim. No contradiction.
2. **Landed** — stop-and-report gate on `<...>` placeholders, `tasks.md:87-94`. No contradiction; consistent with 1.1a's own instruction to "leave no blank" (`tasks.md:85-86`).
3. **Landed** — pure nonzero per-channel gain-scale construction, `tasks.md:122-130`. Checked the math: Pearson `|corr|` is invariant under independent nonzero per-channel scaling (`corr(gL·X, gR·Y) = sign(gL·gR)·corr(X,Y)`), so the construction does what it claims — it moves the level-balance ratio while leaving `repairedCorr` untouched. No contradiction.
4. **Landed** — tie-breaker toward bounding `widthSpread`, `tasks.md:138-153`. The quoted spec clause matches `spec.md:299-301` verbatim, including "never lengthens a read tap." No contradiction.
5. **Landed** — `tasks.md:154-180` names `stereo_delay_width_balance_mapping_keeps_cross_in_0_1_and_spread_at_or_below_todays_max` (confirmed to exist at `app/FroggersDspParityTests.cpp:6935`, confirmed it never calls `Process`) and adds the "reimplements a formula" pass. No contradiction.
6. **Landed** — task 3.1 widened to the THEN clause, `tasks.md:302-327`, conditional on 1.1a's ordering finding — consistent with 1.1b's own conditional language (`tasks.md:109-111`). No contradiction.
7. **Landed** — two more stale comments in task 1.5 (`SetWidthBalance`'s doc comment, and the comment before `cross`'s assignment), `tasks.md:189-201`. Verified both citations against source exactly: `Delay.hpp:617-634` and `Delay.hpp:714-720` — quoted text matches current file content verbatim. No contradiction.
8. **Landed** — new task 1.5a, `tasks.md:202-241`. Citations verified: `wetLimiterL`/`wetLimiterR` at `Delay.hpp:271-272`, `kDelayWetLimiterThreshold` at `Delay.hpp:122` (value `0.72f`, matches). It inherits the same missing-Send gap as 1.1a/1.1b for its own RMS measurement step, but that is the same root defect, not a new one.
9. **Landed** — new task 3.1a, `tasks.md:328-348`. Verified the algebra: `widthSpread/cross = (dwid·baseSeconds·0.35·wb)/(dwid·0.5·wb) = 0.7·baseSeconds`, independent of `wb` — matches the task's claim exactly, and is scoped to pre-repair code so it isn't broken by `cross` becoming a fixed 0 later. No contradiction.
10. **Landed, with a cosmetic inconsistency** — the Mechanics section and `proposal.md` use the full path `openspec/changes/frogg3rs-randomize-depth-reclaim/` (`tasks.md:18-19`), while the three stage-gate items (1.6, 2.6, 3.8) use the shortened `frogg3rs-randomize-depth-reclaim/` (`tasks.md:244, 293, 383-384`). Both correctly identify the same directory (a path containing the longer form also contains the shorter), so this does not break gate evaluation, but it is a textual drift between the rule's two statements. **Blocks nothing** for 1.1a/1.1b or later stages — noted only because the prompt asked to check for exactly this class of drift.

## Verdict

**NOT EXECUTABLE**

Blockers:
1. Task 1.1a's "pinned instrument" never names a value for Send (`DelayParams::dsnd`), whose registered default (0.0, closed) makes `dsp::StereoDelay::Process` (`app/dsp/Delay.hpp:655`) return a dead `{0,0}` pair unconditionally, at every grid point, before any correlation or level-balance figure can be computed. An executor cannot set the check's two bounds without inventing this value. This same gap blocks task 1.1b, whose landed check reuses the identical unstated operating point.
