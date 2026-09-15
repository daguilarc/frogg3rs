# Falsifiability of `stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width`

Task 1.3. The check (`app/FroggersDspParityTests.cpp`, currently lines
9517-9571) carries four `REQUIRE_TRUE` assertions per row of its width sweep:

1. liveness — `std::isfinite(rmsL) && rmsL != 0.0 && std::isfinite(rmsR) && rmsR != 0.0`
2. correlation bound — `corrAbs < 0.55`
3. level-balance bound — `balance < 0.06`
4. strict row-to-row ordering — `corrAbs < prevCorr`

Each was broken individually below, with the literal build and run output
recorded, then the file was restored to its original content (proven by md5)
and the check re-run to confirm it still passes.

All four breaks still fire. None is a finding of a dead assertion.

`REQUIRE_TRUE` throws `std::runtime_error` on the first failing expression in
the macro's own translation unit, tagged `__FILE__:__LINE__ requirement
failed: <expr text>`, so which assertion fired is read directly off the
exception text, not inferred.

## Method

All four breaks are made by editing `app/FroggersDspParityTests.cpp` only;
`app/dsp/Delay.hpp` was never edited for this task. Before starting:

```
$ md5 app/FroggersDspParityTests.cpp app/dsp/Delay.hpp
MD5 (app/FroggersDspParityTests.cpp) = 4e41acdc0efb0cdd1983e3d7b0baf0a7
MD5 (app/dsp/Delay.hpp) = 408cd3ea5c148092f8d57d86a54ecb34
```

A copy of the original `FroggersDspParityTests.cpp` was saved outside the
repo before any edit. Each break below was applied to a fresh copy of that
original (never stacked on a prior break), built with
`nice -n 10 make -j2 <absolute-path>/build/froggers_dsp_parity_tests` after
`rm`-ing the binary, then run in full. After each break the file was restored
from the saved original before the next break was applied.

Baseline, before any break, confirmed passing:

```
$ ./build/froggers_dsp_parity_tests | grep cross_feed_removal_decorrelates
[PASS] stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width
```

## Break 1 — liveness (must now defeat `std::isfinite`)

Task 1.2(a) rewrote the liveness gate to reject non-finite values directly
(`std::isfinite(rmsL) && rmsL != 0.0 && ...`), where the prior form
(`rmsL != 0.0`) does not catch a NaN — under IEEE-754, `NaN != 0.0` is `true`,
so the old gate would have passed a dead-non-finite channel. The break must
therefore defeat `std::isfinite`, not the old `!= 0.0` form, to be evidence for
what 1.2(a) actually changed.

Construction: poison the feedback loop's own input with one non-finite
sample, upstream of any per-row computation, so it has propagated through
`Delay::Process`'s feedback path (feedback = 0.7) for the full warmup window
before the first measured sample:

```cpp
noise[0] = std::numeric_limits<float>::quiet_NaN();  // TEMP BREAK 1.3a
```

inserted immediately after the noise buffer is filled, before the `measure`
lambda is defined.

Result — the liveness assertion (the isfinite/nonzero one) fires, and does so
before either bound is reached:

```
  [delay cross-feed removal] width=0.25 |corr|=1 balance=1
[FAIL] stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width: /Users/diegoaguilar-canabal/Desktop/frogg3rs/app/FroggersDspParityTests.cpp:9566 requirement failed: std::isfinite(rmsL) && rmsL != 0.0 && std::isfinite(rmsR) && rmsR != 0.0
```

This is a live break under the current code: it was not the case that the
break used to defeat only `!= 0.0`; the reported non-finite value was driven
straight into `std::isfinite` and caught there.

## Break 2 — correlation bound

Construction: leave the sweep's printed `width=` values as-is but pin the
knob value actually passed into `measure` to `0.0f`, so `Delay`'s own
width-driven decorrelation mechanism never engages while the signal itself
stays alive:

```cpp
const auto [corrAbs, balance, rmsL, rmsR] = measure(0.0f);  // TEMP BREAK 1.3b
```

Result — liveness passes (not reported as the failure), the correlation bound
fires next:

```
  [delay cross-feed removal] width=0.25 |corr|=1 balance=0
[FAIL] stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width: /Users/diegoaguilar-canabal/Desktop/frogg3rs/app/FroggersDspParityTests.cpp:9566 requirement failed: corrAbs < 0.55
```

(Line 9566 here is the correlation-bound `REQUIRE_TRUE` — this file has one
fewer inserted line than break 1's copy, so the same source line now holds a
different assertion; the expression text in the exception, not the line
number alone, is what identifies which one fired.)

## Break 3 — level-balance bound (isolated from correlation)

Correlation is scale-invariant to a positive per-channel gain, so a pure
nonzero gain scale applied to one channel at the tap trips the balance bound
without tripping the correlation bound first, per the task's own constraint.

Construction: scale the right channel by a constant factor at the point
where each sample is taken from `wet` into the accumulators, leaving the
left channel and everything upstream (the `Delay::Process` call itself)
untouched:

```cpp
const double r = static_cast<double>(wet.r) * 3.0;  // TEMP BREAK 1.3c
```

Result — correlation stays under its own bound (0.502 < 0.55) and is not the
assertion that fires; the balance bound fires:

```
  [delay cross-feed removal] width=0.25 |corr|=0.502056221 balance=0.536322177
[FAIL] stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width: /Users/diegoaguilar-canabal/Desktop/frogg3rs/app/FroggersDspParityTests.cpp:9567 requirement failed: balance < 0.06
```

## Break 4 — strict row-to-row ordering (leaving both bounds satisfied)

Per the task's constraint, reversing the sweep order leaves both the
correlation and balance bounds satisfied at every row (the same four
`(width, corr, balance)` values occur, just visited in the opposite order),
while breaking only the strict-decrease ordering, since correlation is known
(from the passing baseline) to decrease monotonically as width increases.

Construction: reverse the `widths` array only, touching neither `measure`
nor the accumulation:

```cpp
const float widths[] = {1.00f, 0.75f, 0.50f, 0.25f};  // TEMP BREAK 1.3d
```

Result — both rows shown satisfy the correlation bound (0.240, 0.271, both
< 0.55) and the balance bound (0.021, 0.015, both < 0.06); the ordering
assertion is the one that fires, on the second row, because correlation rose
from 0.240 to 0.271 while the sweep now runs width descending:

```
  [delay cross-feed removal] width=1 |corr|=0.240064955 balance=0.0210912708
  [delay cross-feed removal] width=0.75 |corr|=0.27101055 balance=0.0150869517
[FAIL] stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width: /Users/diegoaguilar-canabal/Desktop/frogg3rs/app/FroggersDspParityTests.cpp:9568 requirement failed: corrAbs < prevCorr
```

## Restore and re-verify

`app/FroggersDspParityTests.cpp` was restored from the saved original after
the last break; `app/dsp/Delay.hpp` was never touched by this task. Both
confirmed byte-identical to their pre-task state by md5:

```
$ md5 app/FroggersDspParityTests.cpp <saved original>
MD5 (app/FroggersDspParityTests.cpp) = 4e41acdc0efb0cdd1983e3d7b0baf0a7
MD5 (<saved original>) = 4e41acdc0efb0cdd1983e3d7b0baf0a7
$ md5 app/dsp/Delay.hpp <saved original>
MD5 (app/dsp/Delay.hpp) = 408cd3ea5c148092f8d57d86a54ecb34
MD5 (<saved original>) = 408cd3ea5c148092f8d57d86a54ecb34
```

Rebuilt (binary `rm`-ed first, `nice -n 10 make -j2`) and run in full:

```
[PASS] stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width
...
188/190 tests passed
```

The two failures in that run are pre-existing and not produced by this task —
the two golden-vector pins that moved when `cross` became `0.0f`:

```
[FAIL] stereo_delay_cross_feed_reproduces_its_captured_output_exactly: .../FroggersDspParityTests.cpp:7009 requirement failed: wet.l == c.expectedL
[FAIL] stereo_delay_freeze_at_default_reproduces_pinned_original_output_through_real_process: .../FroggersDspParityTests.cpp:7865 requirement failed: lastWet.l (-0.110614) ~= -0.11269673f (-0.112697), eps=0
```

## Finding

No break failed to fire. All four assertions in the current, task-1.2(a)-rewritten
check are individually falsifiable, each by a break that targets it alone
without a bystander assertion executing first.
