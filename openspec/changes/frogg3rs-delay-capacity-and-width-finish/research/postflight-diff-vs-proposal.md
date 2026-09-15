# Postflight: landed diff vs proposal.md and tasks.md Stage 1

Scope: `git diff HEAD -- app/`, the two untracked scripts, and the 22
tracked-but-deleted files. Stage 2 and Stage 3 tasks are out of scope.
`research/` was not read except by `ls` to check existence claims.

## Commands run, with output

`cd /Users/diegoaguilar-canabal/Desktop/frogg3rs && git rev-parse --is-inside-work-tree`
-> `true`

`git status --short | grep -c '^D '` -> `22` (matches proposal/tasks' "twenty-two
staged deletions under .../frogg3rs-delay-width-wysiwyg-repair/"; confirmed
every `D ` line is under that exact path).

`git status --short | grep '^??'` ->
```
?? app/check_delay_capacity_break_proofs.py
?? app/check_delay_capacity_parameters_are_swept.py
?? openspec/changes/
```
Matches proposal's Impact list for both scripts; the untracked
`frogg3rs-delay-capacity-and-width-finish/` directory itself is this change.

`git diff --stat HEAD -- app/`:
```
 app/FroggersAudioRoutingTests.cpp |  19 +-
 app/FroggersDspParityTests.cpp    | 413 +++++++++++++++++++++++++++++++++++++-
 app/Makefile                      |  30 ++-
 app/dsp/Delay.hpp                 | 128 ++++++++----
 4 files changed, 533 insertions(+), 57 deletions(-)
```
MANUAL.md, QUICK_DICT.md, frogg3rs.code-workspace (all named in the
proposal's Impact list, all Stage 2 tasks, all unchecked) are absent from
this stat — correctly not yet touched.

`git diff HEAD -- app/dsp/Delay.hpp` (full hunk read): confirms `cross =
0.0f`; the three ordered bounds (`baseSeconds = std::min(baseSecondsRaw,
capacitySeconds)`, `modSeconds = std::min(modSecondsRaw, maxModSeconds)`,
`widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds)` with
`maxSpreadSeconds` subtracting both `baseSeconds` and `modSeconds`); the
inclusive (`<=`) compile-gated asserts on `timeL`/`timeR` against
`capacitySeconds`; the index-domain asserts in `ReadAt`
(`idx0 < line.size()`, `idx1 < line.size()`) and `WriteSample`
(`writePos < line.size()`), all under `#if defined(FROGGERS_DSP_CHECKS)`.

`git diff HEAD -- app/Makefile`: confirms `CPPFLAGS` gains
`-DFROGGERS_DSP_CHECKS` (global, so applies to every binary the Makefile
builds); two new `.PHONY` targets `check-delay-capacity-parameters-are-swept`
and `check-delay-capacity-break-proofs`, both invoking the two untracked
scripts, both added to the `test:` prerequisite line.

`git diff HEAD -- app/FroggersAudioRoutingTests.cpp`: confirms the storm
test's peak tracking changed from one scalar `peak` to `peakPerChannel`,
failing on `std::any_of(... channelPeak <= kSilenceEpsilon)`.

`git diff HEAD -- app/FroggersDspParityTests.cpp` (full 476-line diff read;
`grep -c '^+TEST_CASE\|^-TEST_CASE'` -> 7): one rename
(`..._keeps_cross_in_0_1_and_spread_at_or_below_todays_max` ->
`..._keeps_spread_at_or_below_todays_max`, tautological `cross` REQUIRE_TRUE
removed) and 6 additions: the decorrelation test, two golden-vector
recaptures (`stereo_delay_cross_feed_reproduces_its_captured_output_exactly`,
`stereo_delay_freeze_at_default_reproduces_pinned_original_output...`), two
width-spread-capacity tests (`..._never_reads_past_the_line_capacity`,
`..._bound_is_inert_away_from_capacity`), the grid sweep
(`..._holds_across_the_reachable_grid`, `dtims[] = {0.0f, 0.5f, 0.9f, 0.99f,
1.0f}` matching task 1.9 exactly), and the random-walk test
(`stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks`,
4 sample rates x 500 points = 2000, `REQUIRE_TRUE(points == 2000)`). Also
confirms the `#if !defined(FROGGERS_DSP_CHECKS) #error` guard task 1.15
claims.

`cat app/check_delay_capacity_parameters_are_swept.py`: confirms
comment-stripping (`strip_comments`, honouring string/char literals via
character walk, not regex) before body selection, matching task 1.16
exactly.

`cat app/check_delay_capacity_break_proofs.py`: confirms the `BREAKS` table
has exactly 7 entries — `modulation-bound-removed`, `width-bound-removed`,
`modulation-dropped-from-width-budget`, `wrap-admits-capacity`,
`lines-allocated-short`, `off-grid-width-balance-headroom` (the cubic Width
balance headroom), `capacity-headroom-shortens-reads` (10ms) — matching task
1.17's list verbatim, and the docstring's explicit statement that the
base-bound break is excluded because it stays green, matching the task.

`ls openspec/changes/frogg3rs-delay-capacity-and-width-finish/research/`:
every research record a completed (`[x]`) Stage 1 task names by path exists:
`decorrelation-check-falsifiability.md`,
`wet-limiter-independence-image-shift.md`, `grid-widening-break-proof.md`,
`capacity-bound-break-proofs.md`, `process-capacity-assertion.md`,
`index-domain-checks.md`, `random-walk-capacity-test.md`,
`break-proof-gate.md`, `adversarial-postflight-a.md`,
`adversarial-postflight-b.md`, `adversarial-postflight-c.md`,
`adversarial-postflight-adjudication.md`. (Contents not read, per scope.)

`grep -rn "FROGGERS_DSP_CHECKS\|NDEBUG" app/standalone/build/...` and
`grep -rln "FROGGERS_DSP_CHECKS" app/browser app/vst`: standalone's CMake
flags carry `-DNDEBUG` and never `-DFROGGERS_DSP_CHECKS`; browser and vst
trees contain no reference to the macro at all. Confirms the landed
Makefile/Delay.hpp comments' claim that no shipping build passes the macro.

## Findings

None diverge. 20 claims checked (three capacity bounds and their order; the
fixed-zero cross-feed; the inclusive near-capacity compares; the liveness
(non-finite) checks; the four-parameter 96kHz grid check and its 1.9 dtim
values; the two golden-vector recaptures; the rewritten width-balance test
1.8; the per-channel storm-test check 1.4a; the two new scripts' wiring into
`app/Makefile` and into `test:`; the comment-stripping behavior of
`check_delay_capacity_parameters_are_swept.py`; the seven-entry break table
and base-bound exclusion in `check_delay_capacity_break_proofs.py`; the
`FROGGERS_DSP_CHECKS` compile gate and the `#error` guard; the index-domain
asserts in `ReadAt`/`WriteSample`; the random-walk test's four sample rates
and 2000-point count; the five-amended-comments claim; the 22 staged
deletions; the twelve named research records' existence; the "no shipping
build passes the macro" claim) — every one matches the diff or the
filesystem exactly, and nothing in the diff is unaccounted for by the
proposal or a completed Stage 1 task.
