# Postflight repetition audit -- diff under `app/`

Scope: `git diff HEAD -- app/` plus the untracked `app/check_delay_capacity_parameters_are_swept.py`
and `app/check_delay_capacity_break_proofs.py`. Question: what did writing this diff make
redundant that was not redundant before, and what does the diff itself duplicate. Every grep
below ran against `/Users/diegoaguilar-canabal/Desktop/frogg3rs/app`, case-insensitive.

## Diff contents (for reference)

`git diff HEAD -- app/` touches `app/FroggersAudioRoutingTests.cpp`, `app/FroggersDspParityTests.cpp`,
`app/Makefile`, `app/dsp/Delay.hpp` (775-line diff). Plus two new untracked files:
`app/check_delay_capacity_parameters_are_swept.py` (207 lines) and
`app/check_delay_capacity_break_proofs.py` (203 lines). Full diff captured at
`/private/tmp/claude-501/-Users-diegoaguilar-canabal-Desktop/68105815-8b90-42de-aa6a-51f093984b11/scratchpad/app_diff.patch`.

## Grep: FROGGERS_DSP_CHECKS

```
$ grep -rniI "FROGGERS_DSP_CHECKS" .
Makefile:22:# FROGGERS_DSP_CHECKS compiles the DSP headers' invariant assertions in.
Makefile:27:CPPFLAGS := -I$(SHEAF_SYNTH_DIR)/include -DFROGGERS_DSP_CHECKS
Makefile:256:# (compiled under FROGGERS_DSP_CHECKS) ran; it says nothing about whether
FroggersDspParityTests.cpp:35:// under FROGGERS_DSP_CHECKS as their oracle; app/Makefile passes it.
FroggersDspParityTests.cpp:37:#if !defined(FROGGERS_DSP_CHECKS)
FroggersDspParityTests.cpp:38:#error "FroggersDspParityTests.cpp requires -DFROGGERS_DSP_CHECKS ..."
check_delay_capacity_break_proofs.py:3:compares (dsp/Delay.hpp, compiled under FROGGERS_DSP_CHECKS) stop rejecting
dsp/Delay.hpp:708:        // Compiled only where FROGGERS_DSP_CHECKS is defined ...
dsp/Delay.hpp:713:#if defined(FROGGERS_DSP_CHECKS)
dsp/Delay.hpp:1002:#if defined(FROGGERS_DSP_CHECKS)
dsp/Delay.hpp:1012:#if defined(FROGGERS_DSP_CHECKS)
```
FOUND (whole tree): 1 symbol, 1 definition site (Makefile), 1 compile-time enforcement site
(FroggersDspParityTests.cpp #error), 3 guard sites (dsp/Delay.hpp asserts). All introduced by
this diff -- FOUND == diff occurrences, no pre-existing collision.

## Grep: capacity-in-samples formula (production vs. test recomputation)

```
$ grep -n "capacity = std::min(kMaxDelaySamples" dsp/Delay.hpp
546:        capacity = std::min(kMaxDelaySamples, static_cast<size_t>(std::ceil(kMaxDelaySeconds * sampleRate)));
$ grep -n "static_cast<size_t>(std::ceil(2.0f \* sr))" FroggersDspParityTests.cpp
10500:    const size_t capacity = static_cast<size_t>(std::ceil(2.0f * sr));   # width_spread_never_reads_past_the_line_capacity
10545:    const size_t capacity = static_cast<size_t>(std::ceil(2.0f * sr));   # width_spread_bound_is_inert_away_from_capacity
$ grep -n "kMaxDelaySamples,$" FroggersDspParityTests.cpp
10598:    const size_t capacity = std::min(dsp::StereoDelay::kMaxDelaySamples,   # width_spread_bound_holds_across_the_reachable_grid
```
FOUND (whole tree): 4 sites compute "delay-line capacity in samples" -- 1 production
(`SetSampleRate`, untouched by this diff) + 3 test recomputations, all new in this diff, in
**two different, non-unified forms**: two tests hardcode `2.0f` and omit the
`std::min(kMaxDelaySamples, ...)` clamp; the third (grid-sweep) reproduces the clamp exactly,
using the named `kMaxDelaySeconds`/`kMaxDelaySamples` constants, because it runs at 96 kHz where
the clamp has a real geometric surface (its own comment says so). Diff's own occurrences: 3
(all new). No production accessor (e.g. `Capacity()`) exists, so the tests have no way to avoid
recomputing; `capacity` is a private member.

**Ruling: partial violation.** The grid-sweep test's exact recomputation is deliberate and
justified in its own comment (it needs the real clamp at 96 kHz). The two simplified
recomputations are undocumented, diverge in form from the exact one, and are silently correct
only because they are exercised at 48 kHz (where `min(96000, ceil(2.0*48000)) == 96000`
regardless of the clamp). This is a family that must stay in sync (production's capacity
formula vs. 3 independent test copies) with no accessor and no drift check. **Breaks:** if
`SetSampleRate`'s clamp or constants change, the two simplified tests go silently stale (they
would keep computing a now-wrong "capacity" and their bound checks would pass or fail for the
wrong reason). **Blocks: neither** -- correct today, but flagged; the cheap fix is one shared
test helper mirroring `SetSampleRate`'s exact formula, used by all three tests, or a `Capacity()`
accessor on `StereoDelay`.

## Grep: width-spread weight formula `dwid * baseSeconds * 0.35f * widthBalance`

```
$ grep -n "baseSeconds \* 0.35f" FroggersDspParityTests.cpp dsp/Delay.hpp
FroggersDspParityTests.cpp:6959:  const float spread = dwid * baseSeconds * 0.35f * delay.widthBalance;      # pre-existing, untouched by diff
FroggersDspParityTests.cpp:10559: const float widthSpread = p.dwid * baseSeconds * 0.35f * 1.0f;              # new
FroggersDspParityTests.cpp:10670: const float widthSpreadRaw = dwid * baseSeconds * 0.35f * wb;               # new
dsp/Delay.hpp:697:      const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;      # production, modified by diff
```
FOUND: 4 sites (1 production + 3 test). Diff's own new occurrences: 2 (the two new test sites;
the 3rd test site at :6959 predates this diff).

**Ruling: deliberate, not a violation.** These are test-side oracles: each computes an expected
value from the known formula, then compares it against a lag *measured* by actually running
`dsp::StereoDelay::Process()` -- a real behavioural check, not two production sites computing the
same thing. The grid-sweep test's own comment argues this explicitly: "rather than recomputing
the bound formula on its own with no call into production at all: a family that must stay in
sync ... checked apart is how a broken guard can pass" -- i.e. the diff already reasoned about
exactly this risk and designed around it by measuring real output. Read and verified: `measure`
in the grid-sweep test does call `delay.Process` for the actual lag; the formula only builds
`expectedLagSamples`. Minor, non-blocking note: the formula itself is inlined 3 times across
tests rather than through one shared test helper (e.g. `ExpectedWidthSpreadSeconds(dwid,
baseSeconds, wb)`), which would satisfy the helper rule (3 reuses, isolates a stage). Not required
to ship.

## Grep: old cross-feed formula, confirming clean removal

```
$ grep -rniI "dwid \* 0\.5f\|0\.5f \* widthBalance\|dwid\*0\.5f" .
(no output)
```
FOUND: 0. The diff replaces `const float cross = p.dwid * 0.5f * widthBalance;` with a fixed
`const float cross = 0.0f;` in `dsp/Delay.hpp` and updates every dependent comment and pinned
test value (`stereo_delay_cross_feed_reproduces_its_captured_output_exactly`,
`stereo_delay_freeze_at_default_reproduces_pinned_original_output...`, the width-balance-mapping
test). No leftover reference to the removed formula anywhere in the tree. Clean; no violation.

## Grep: python `read()` helper

```
$ grep -n "^def read(" check_*.py
check_common.py:41:def read(path):
check_delay_capacity_break_proofs.py:109:def read(path):
$ sed -n '41,43p' check_common.py
def read(path):
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        return fh.read()
$ sed -n '109,111p' check_delay_capacity_break_proofs.py
def read(path):
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        return fh.read()
$ grep -n "check_common" check_*.py
check_artifact_symbols_resolve.py:106:from check_common import path_index, read, walk_sources
check_citations_resolve.py:40:from check_common import path_index, walk_all, walk_sources
check_delay_capacity_parameters_are_swept.py:36:from check_common import read
check_modified_requirements_restate_promoted.py:119:from check_common import read
check_spec_checks_resolve.py:89:from check_common import path_index, read, walk_all, walk_sources
check_no_planning_history.py:35:from check_common import walk_sources
```
FOUND: 2 definitions, byte-identical body. Every other check script in `app/` (5 of them,
including this diff's own sibling `check_delay_capacity_parameters_are_swept.py`) imports `read`
from `check_common` instead of redefining it. Diff's own occurrences: 1 new duplicate definition
(in `check_delay_capacity_break_proofs.py`).

**Ruling: VIOLATION.** Identical logic, 2 occurrences, one of them written in this diff, in a
script sitting right next to `check_common.py` whose own docstring says: "Three scripts each
walked the tree with their own copy of the same exclusion list, and the copies had already
drifted apart before any of them shipped ... it is worth not committing in the very change that
exists to stop it." `check_delay_capacity_break_proofs.py` does not use `check_common`'s tree-walk
helpers (it copies one known directory, not a filtered walk), but it does need `read()`, and
reimplements it instead of importing it. **Breaks:** a future edit to `check_common.read` (e.g.
adding a size guard or a different error-handling policy) silently does not apply to this script.
**Blocks delivery** -- one-line mechanical fix (`from check_common import read`, delete the local
def), directly on this diff's own path, exactly the failure `check_common.py` exists to prevent.

## Grep: python `strip_comments()` (C++ comment stripping)

```
$ grep -n "def strip_comments" check_*.py
check_delay_capacity_parameters_are_swept.py:80:def strip_comments(text):
check_docs_match_parameter_table.py:32:def strip_comments(text):
```
`check_docs_match_parameter_table.py` (pre-existing, untouched by this diff):
```python
def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text
```
`check_delay_capacity_parameters_are_swept.py` (new, this diff): a ~40-line character walk that
preserves string/char literals and explicitly documents why: "a regex alternation of
string/comment patterns is exactly the shape that starts matching a `//` or `/*` sitting inside a
string literal instead of skipping it."

FOUND: 2 implementations of the same concept (strip C++ `//` and `/* */` comments from source
text) at different robustness levels. Diff's own new occurrence: 1.

**Ruling: violation by the narrowest-operand standard** (same transformation, different
expression -- exactly what Rule 5 says to search for). The new implementation's own docstring
argues the old one is unsafe against comment-like text inside string literals, but the diff does
not fix or consolidate the old one into `check_common.py`, which is the file whose own charter is
"one place to correct them." **Breaks:** a MANUAL.md/QUICK_DICT.md parameter name or comment
containing `//` or `/*` inside a quoted label would be mis-stripped by the old regex version and
not by the new one -- two check scripts can now disagree about what "the source, minus comments"
means. **Blocks: neither** -- the older implementation predates this diff and is outside its
blast radius (`check_docs_match_parameter_table.py` is not part of `git diff HEAD -- app/`);
flagged as a missed consolidation opportunity, not a defect this diff introduced into a file it
owns.

## Grep: TEST_CASE name/body parsing

```
$ grep -n "TEST_CASE" check_spec_checks_resolve.py check_delay_capacity_parameters_are_swept.py
check_spec_checks_resolve.py:95:TEST_CASE = re.compile(r"TEST_CASE\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)")
check_delay_capacity_parameters_are_swept.py:42:TEST_CASE_RE = re.compile(r"TEST_CASE\(([A-Za-z0-9_]+)\)\s*\{")
```
FOUND: 2 regex declarations that both start `TEST_CASE\(`. Diff's own new occurrence: 1.

**Ruling: not a violation.** `check_spec_checks_resolve.py` extracts only test *names* (for
citation resolution against spec `Check:` lines); `check_delay_capacity_parameters_are_swept.py`
needs full test *bodies* (brace-matched, to inspect literal values assigned inside), which the
other script has no counterpart for. The shared substring is a one-line regex prefix, not a
transformation stage; the actual non-trivial logic (`find_test_case_bodies`'s brace counting) is
unique to the new script. Does not clear the Rule 4 helper bar (not reused, and the "isolates a
stage" criterion applies to the body-extraction machinery, which is not shared).

## Grep: LCG PRNG step

```
$ grep -n "1664525u + 1013904223u" FroggersDspParityTests.cpp
3331, 9444, 9549, 9632, 9747, 9808 (altLcg), 9886, 10738
```
8 occurrences total in the file. Diff's own new occurrences: 2 (line ~9549 area inside
`stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width`'s noise generator,
and line 10738 inside `stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks`'s
`next01`). The other 6 sites predate this diff.

**Ruling: violation, pre-existing and extended, not originated, by this diff.** The identical
one-line LCG step (`lcg = lcg * 1664525u + 1013904223u;`) is inlined ad hoc at every one of 8
call sites rather than through one shared `NextLcg`/RNG helper -- already 2+ occurrences (6) before
this diff, already a Rule 5 violation on its own. This diff had two fresh call sites and had the
opportunity to introduce a shared helper; it matched the ambient (already-duplicated) idiom
instead. **Breaks:** none immediately -- the formula is a fixed, well-known constant (Numerical
Recipes LCG) unlikely to need to change in lockstep. **Blocks: neither** -- pre-existing,
repo-wide, outside this diff's narrow scope to fix unilaterally; named here because the diff
added to it rather than reducing it.

## Grep: peak-scan-after-impulse loop (`MeasureReadAtLagSamples` vs. inlined `step` copy)

```
$ grep -n "MeasureReadAtLagSamples\|peakAbs = std::fabs(wet.r)" FroggersDspParityTests.cpp
10474: static long long MeasureReadAtLagSamples(...)          # definition
10481:     if (i > impulseIndex && std::fabs(wet.r) > peakAbs) {
10522:     MeasureReadAtLagSamples(delay, p, impulseIndex, capacity + 5000, &peakAbs);   # call 1
10565:     MeasureReadAtLagSamples(delay, p, impulseIndex, capacity, &peakAbs);          # call 2
10680:                         if (i > 0 && std::fabs(wet.r) > peakAbs) {                # inlined copy, call 3 site
```
FOUND: helper defined once, reused by 2 tests (satisfies Rule 4's "2+ uses"); a 3rd test
(`stereo_delay_width_spread_bound_holds_across_the_reachable_grid`) inlines its own copy of the
same peak-scan loop body instead of calling the helper. Diff's own occurrences: 1 def + 3 uses (2
direct, 1 duplicated inline).

**Ruling: deliberate, justified in-code.** The grid-sweep test's own comment states it inlines
the scan "so this test can also track its own copy of the LFO phase alongside each call" -- the
shared helper's signature (`delay`, `p`, `impulseIndex`, `measureLen`) has no hook for interleaving
an externally-tracked phase variable across thousands of grid points sharing one warmed-up
`StereoDelay` object, which the grid test needs (it must know where production's private
`lfoPhase` sits to advance to the sin-peak position before each impulse). Verified by reading:
`MeasureReadAtLagSamples` does not expose or accept a per-step callback. Not a violation, though a
lower-cost fix (giving the helper an optional per-step callback) was available and not taken --
minor, non-blocking.

## Grep: liveness threshold literal `0.01f`

```
$ grep -n "peakAbs > 0.01f" FroggersDspParityTests.cpp
10526, 10567, 10690
```
FOUND: 3 identical literal comparisons, all new in this diff. Cosmetic: Rule 7 extracts O(1)
checks only at 3+ uses, which this now meets. **Blocks: neither** -- a named
`kLagProbeLivenessFloor` constant would be marginally clearer; not required.

## Grep: `#if defined(FROGGERS_DSP_CHECKS)` assert guards in dsp/Delay.hpp

```
$ grep -n "#if defined(FROGGERS_DSP_CHECKS)" dsp/Delay.hpp
713, 1002, 1012
```
FOUND: 3 sites, each guarding a different invariant (`Process`'s `timeL/timeR <= capacitySeconds`,
`ReadAt`'s `idx0/idx1 < line.size()`, `WriteSample`'s `writePos < line.size()`). All new. **Ruling:
not a violation** -- boilerplate compile-guard repeated around 3 distinct runtime checks on 3
distinct variables; not "identical logic" in the Rule 5 sense (a shared macro would only shrink
`#if`/`#endif` text, not eliminate duplicated behaviour).

## The two new python gates against each other and against `check_common.py`

- `check_delay_capacity_parameters_are_swept.py` imports `read` from `check_common` correctly; it
  needs no tree walk (operates on one fixed path, `FroggersDspParityTests.cpp`), so it does not
  duplicate `walk_sources`/`walk_all`/`EXCLUDED_DIRS`.
- `check_delay_capacity_break_proofs.py` copies one known directory (`app_dir/dsp`) via
  `shutil.copytree`, not a filtered walk, so it too does not need `EXCLUDED_DIRS` -- but it does
  duplicate `read()` (see above), the one violation between these two files and `check_common.py`.
- The two new scripts share no logic with each other: one parses C++ test bodies via regex/brace
  matching, the other mutates a header and shells out to a compiler. Their common shape (a `NAME`
  constant, `main()` with positional argv, `"{NAME}: OK/FAIL -- ..."` print format) matches the
  existing convention already used by every other `check_*.py` in `app/` -- consistent, not
  duplicated freshly between just these two.

## Summary table -- concepts touched, FOUND vs. diff's own occurrences

| Concept (operand) | FOUND (tree) | Diff's own | Ruling |
|---|---:|---:|---|
| `FROGGERS_DSP_CHECKS` | 8 sites / 1 symbol | 8 (all new) | not a violation |
| capacity-in-samples formula | 4 | 3 (2 simplified, 1 exact) | partial violation, non-blocking |
| width-spread weight formula | 4 | 2 | deliberate oracle, non-blocking |
| old cross-feed formula | 0 | 0 (removed) | clean removal |
| python `read()` | 2 | 1 | **violation, blocks delivery** |
| python `strip_comments()` | 2 | 1 | violation, non-blocking (out of blast radius) |
| `TEST_CASE\(` regex | 2 | 1 | not a violation |
| LCG step | 8 | 2 | violation, pre-existing, extended not originated |
| peak-scan-after-impulse loop | 1 helper + 1 inline copy | both new | deliberate, non-blocking |
| `peakAbs > 0.01f` | 3 | 3 | cosmetic, non-blocking |
| `#if FROGGERS_DSP_CHECKS` assert guard | 3 | 3 | not a violation |

15 concepts enumerated total (11 above with FOUND>1 or otherwise notable, plus `kSilenceEpsilon`,
`kOverScaleBound`, `peakPerChannel`/`anyChannelPermanentlySilent`, `Correlation` struct reuse --
each FOUND=1 new-definition-and-single-use or clean reuse, no repetition, not tabled above).

## Findings

1. **`check_delay_capacity_break_proofs.py` redefines `read()` instead of importing it from
   `check_common.py`.** Identical logic (2 occurrences, byte-identical body), one written by this
   diff, in the one file whose own docstring exists specifically to stop this pattern. Breaks: a
   future change to `check_common.read`'s behaviour silently does not reach this script. **Blocks
   delivery.** Fix: `from check_common import read` (line 36 in the sibling script shows the exact
   form), delete the local `def read` at lines 109-111.

2. **Two of the three new capacity-bound tests recompute the delay line's capacity-in-samples
   using a simplified, undocumented form** (`static_cast<size_t>(std::ceil(2.0f * sr))`, omitting
   the `std::min(kMaxDelaySamples, ...)` clamp and the named constant) that diverges from both
   production's `SetSampleRate` and this diff's own third test (which reproduces the clamp
   exactly, with a comment explaining why). Breaks: if `SetSampleRate`'s formula changes, these two
   tests silently go stale. Blocks neither now (numerically correct at the 48 kHz they run at).
   Not required to ship; recommend one shared test-side capacity helper for all three.

3. **`check_delay_capacity_parameters_are_swept.py` introduces a second, more-robust
   `strip_comments()` implementation** rather than fixing or promoting the existing one in
   `check_docs_match_parameter_table.py` into `check_common.py`. Breaks: two check scripts can now
   disagree about what "comments" means for text containing `//`/`/*` inside string literals.
   Blocks neither -- the older implementation sits in a file outside this diff's blast radius.

4. **The diff adds 2 more inline copies of the pre-existing, already-duplicated LCG PRNG step**
   (8 total sites now, none behind a shared helper) rather than introducing one. Blocks neither --
   pre-existing, repo-wide convention this diff did not originate, only extended.

No finding blocks execution. One finding (the `read()` duplication) blocks delivery and is a
one-line mechanical fix on this diff's own new file. The width-spread and capacity-recomputation
patterns in the new tests are legitimate test-measures-production-behaviour oracles, not
production-side duplication, with one partial exception (finding 2) that is non-blocking but real.
