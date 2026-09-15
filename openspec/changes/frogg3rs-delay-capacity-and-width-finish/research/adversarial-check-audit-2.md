# Adversarial audit of the four inherited checks -- second independent pass

Auditor axis: same as `research/adversarial-check-audit.md` (get a broken
mechanism past `stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width`,
`stereo_delay_width_spread_never_reads_past_the_line_capacity`,
`stereo_delay_width_spread_bound_is_inert_away_from_capacity`, and
`stereo_delay_width_spread_bound_holds_across_the_reachable_grid`, all in
`app/FroggersDspParityTests.cpp`, checking `app/dsp/Delay.hpp`'s
`StereoDelay::Process`), both uncommitted (`git diff HEAD`). This pass ran
independently of the first (`adversarial-check-audit.md`) and was told only
the goal, not that file's method. It derives its own attacks below, then
briefly notes where they relate to the first pass's four.

Method: read `app/dsp/Delay.hpp`'s `StereoDelay::Process` (the capacity guard
at the `widthSpread`/`maxSpreadSeconds` computation) and all four shipped
`TEST_CASE`s end-to-end before mutating anything. Backed up both files first
(`/tmp/attack_backup2/*.orig`), confirmed their md5 matched the first pass's
recorded baseline before any edit:
- `app/dsp/Delay.hpp` = `2090d9902bae29e67d7eb57a27991cef`
- `app/FroggersDspParityTests.cpp` = `35b80ce13d6a4ffebae993663eabcddf`

Both match again after every attack's revert (checked immediately after each
mutation and confirmed a final time at the end of this audit -- see the
`md5`/`git status --short` output at the bottom).

Built and ran for real via `nice -n 10 make -j2 "$(pwd)/build/froggers_dsp_parity_tests"`
from `app/`, `rm`ing the binary before each rebuild (`app/build/froggers_dsp_parity_tests`).

---

## Attack 5 (GOT THROUGH -- real, unmounted production gap): capacity guard silently inert whenever `widthBalance < 1.0`

**Enumeration that found it:** all three of the shipped checks that actually
call `StereoDelay::Process()` --
`stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width`,
`stereo_delay_width_spread_never_reads_past_the_line_capacity`, and
`stereo_delay_width_spread_bound_is_inert_away_from_capacity` -- run at
exactly ONE point on the `widthBalance` axis: 1.0 (the default-constructed
member for the first, an explicit `delay.SetWidthBalance(1.0f)` for the
other two). `widthBalance` is a real, independently-settable knob (Delay
slot 12, "Width balance" / "WBal", `StereoDelay::SetWidthBalance`) that
scales `widthSpreadRaw` directly in `Process()`:

```cpp
const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
...
const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds - 1.0f / sampleRate);
const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
```

The fourth check
(`stereo_delay_width_spread_bound_holds_across_the_reachable_grid`) DOES
sweep `widthBalance` across its full `[0,1]` grid -- but it never
instantiates `dsp::StereoDelay` or calls `.Process()` anywhere in its body
(confirmed by grep over its exact line range, `sed -n '10552,10597p' ... |
grep -n "StereoDelay\|\.Process(\|delay\."` -- the only hit is a comment).
It re-derives `widthSpreadRaw`/`maxSpreadSeconds`/`widthSpread` by hand from
`dsp::ExpMapCompute` and recomputes its own clamp with its own copy of the
`0.35f`/`1.0f / sampleRate` constants. It is a self-consistency check of its
own re-implementation of the formula, not a behavioral check of
`Process()`. So the `widthBalance` axis is swept only against a duplicate of
the guard, never against the guard's real code.

**Mutation:** gated the clamp on `widthBalance`, matching exactly the single
point (1.0) every Process()-calling check happens to pin:

```cpp
const float widthSpread = (widthBalance >= 0.999f) ? std::min(widthSpreadRaw, maxSpreadSeconds) : widthSpreadRaw;
```

i.e. the capacity guard is *completely disabled* the instant `widthBalance`
drops even slightly below 1.0 -- the exact class of regression this change
exists to prevent (an unclamped read past the line's capacity, wrapping to a
much shorter, wrong lag), reached through a different, real, user-facing
knob than the one (`dwid`) the shipped tests parametrize by.

Full-suite run with the mutation in place, filtered to the four checks:
```
[PASS] stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width
[PASS] stereo_delay_width_spread_never_reads_past_the_line_capacity
[PASS] stereo_delay_width_spread_bound_is_inert_away_from_capacity
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
```
All four green, guard fully disabled for any `widthBalance < 1.0`.

**Confirmed real and reachable** with a temporary probe `TEST_CASE`
(`adversarial_width_spread_widthbalance_below_one_reaches_overrun`, appended
directly after the shipped grid-sweep test so it could reuse
`MeasureReadAtLagSamples`), driving the mutated `Process()` through the
exact same public API a real host would use --
`delay.SetWidthBalance(0.5f)` (a legitimate mid-travel Width-balance
setting, not an edge value), `dtim=0.99`, `dwid=1.0`:

```
[ADVERSARIAL widthBalance<1 overrun] widthBalance=0.5 lagSamples=8544 capacity=96000 peakAbs=0.794186234
```

The knob asked for `baseSeconds + widthSpreadRaw` &asymp; 1.98s + 0.35s &asymp;
2.33s (&asymp;111840 samples) of delay; the actual measured lag was **8544
samples (&asymp;178ms)**, roughly 13x shorter than requested -- the same
wrap-to-a-short-lag failure Attack 1 in the first pass found through the
modulation term, found here through Width balance instead. `peakAbs=0.79`
confirms a real, non-degenerate peak (not a dead/silent measurement).

**Verdict:** GOT THROUGH. Real, currently-present coverage gap: any
`widthBalance` setting below 1.0 (the vast majority of the knob's own
travel) removes the capacity guard's protection entirely, and none of the
four shipped checks can see it because three of them never move off
`widthBalance=1.0` and the fourth never calls `Process()` at all.
**Blocks delivery** -- this is a second, independent way to reach the exact
overrun this change's own stated purpose is to close (the first being
Attack 1's `modSeconds` route), on a knob (`Width balance`) that ships and
is user-reachable today. Does not block execution of unrelated work.
Fix shape: either the fourth test must be rewritten to call the real
`StereoDelay::Process()` (via `MeasureReadAtLagSamples` or equivalent)
across its grid rather than recomputing the formula, or a fifth check must
sweep `widthBalance` through `Process()` directly the way the third check
sweeps `dtim`.

---

## Attack 6 (structural finding, no mutation needed -- reading is the command that settles it): the reachable-grid sweep tests its own formula, never the production code

This is the mechanism that let Attack 5 through, stated on its own because
it is a defect independent of any single mutation: a family that must stay
in sync (the guard's real implementation in `Process()`, and this test's
copy of it) is checked apart, per the brief's own instruction to name "each
place two things that must hold TOGETHER are checked apart."

`stereo_delay_width_spread_bound_holds_across_the_reachable_grid`'s entire
body (`app/FroggersDspParityTests.cpp:10552-10596` at the baseline commit)
never constructs a `dsp::StereoDelay` and never calls `.Process()`. It
reimplements the guard formula inline:

```cpp
const float widthSpreadRaw = dwid * baseSeconds * 0.35f * widthBalance;
const float capacitySeconds = static_cast<float>(capacity) / sr;
const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - 1.0f / sr);
const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
```

against a duplicated `capacity = 96000` constant and a duplicated `0.35f`
literal, then asserts only that ITS OWN recomputed `worstExcess <= 0.0` --
i.e. that the test's copy of the formula is internally consistent, which is
true by construction regardless of what `Process()` actually does. (Contrast
with the third check,
`stereo_delay_width_spread_bound_is_inert_away_from_capacity`, which DOES
call `Process()` and compares its measured output to a locally recomputed
expectation -- that one would catch a coefficient drift; the grid sweep does
not, because it never touches the thing it claims to sweep.)

Consequence, demonstrated by Attack 5 above: this test can be made to stay
green under *any* change to `Process()`'s actual guard logic that the other
three checks' single pinned points do not also happen to exercise -- it
supplies breadth of coverage in appearance (400^3 grid points, every knob
combination) while supplying zero coverage of the production code path for
any point that differs from what tests 1-3 already pin.

**Verdict:** structural gap, confirmed by reading (the grep above is the
settling command, reproducible without any mutation). Its consequence
(silently missing a real regression) is exactly what Attack 5 demonstrated
concretely. **Blocks delivery** together with Attack 5 -- fixing Attack 5's
specific `widthBalance` gap without also making this test call the real
`Process()` leaves the same class of drift (any future edit to the guard
formula that the three Process()-calling tests' pinned points don't happen
to probe) undetected again the next time the formula changes.

---

## Summary table (this pass)

| # | Attack class | Result | Blocks |
|---|---|---|---|
| 5 | Capacity guard disabled for any `widthBalance < 1.0` (all three Process()-calling checks pin `widthBalance == 1.0`) | **GOT THROUGH** (real, present-in-current-tree-reachable gap; confirmed with a real overrun via the public API) | delivery of this change |
| 6 | Grid-sweep test never calls `StereoDelay::Process()` -- checks its own formula copy, not production code | **Structural gap** (the mechanism that let #5 through; confirmed by reading, no mutation needed) | delivery of this change |

Relationship to the first pass's four attacks: Attack 5 here is a sibling of
that pass's Attack 1 (both are "capacity guard doesn't cover every term that
feeds the read-time request," reached through a different knob --
`widthBalance` here, `dmod`/Mod rate there). Attack 6 here is a distinct,
new finding (a coverage-instrument defect, not a DSP-mechanism defect) that
the first pass's four attacks did not report.

---

`git status --short` at the end of this audit, and the md5 of both edited
files, confirming no residual diff beyond what was already present
(`M`) before this audit started:
