# Adversarial audit of the four inherited checks (cross-feed decorrelation + three capacity checks)

Auditor axis: get a broken mechanism past `stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width`,
`stereo_delay_width_spread_never_reads_past_the_line_capacity`,
`stereo_delay_width_spread_bound_is_inert_away_from_capacity`, and
`stereo_delay_width_spread_bound_holds_across_the_reachable_grid`
(all in `app/FroggersDspParityTests.cpp`, checking `app/dsp/Delay.hpp`'s
`StereoDelay::Process`), both uncommitted (`git diff HEAD`).

Method: each attack below was mounted as a real edit to `app/dsp/Delay.hpp`
and/or a temporary probe `TEST_CASE` appended to
`app/FroggersDspParityTests.cpp`, built with
`nice -n 10 make -j2 <path>/build/froggers_dsp_parity_tests` (binary `rm`'d
first each time) and run for real. Every file was backed up before mutation
(`/tmp/attack_backup/*.orig`) and restored byte-for-byte afterward, verified
with `md5`. Baseline `md5`:
- `app/dsp/Delay.hpp` = `2090d9902bae29e67d7eb57a27991cef`
- `app/FroggersDspParityTests.cpp` = `35b80ce13d6a4ffebae993663eabcddf`

Both match after every attack's revert (checked after each mutation and
again at the end of this audit).

Baseline run (unmodified tree) — all four checks green:
```
[PASS] stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width
[PASS] stereo_delay_width_spread_never_reads_past_the_line_capacity
[PASS] stereo_delay_width_spread_bound_is_inert_away_from_capacity
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
```
(Two unrelated pre-existing failures also present at baseline —
`stereo_delay_cross_feed_reproduces_its_captured_output_exactly` and
`stereo_delay_freeze_at_default_reproduces_pinned_original_output_through_real_process`
— both predate this audit's edits, out of scope for this axis, not touched.)

---

## Attack 1 (GOT THROUGH — real, unmounted production gap): modulation term overruns capacity, uncaught

**Mechanism, not introduced by me — present in the current uncommitted
`Delay.hpp` as-is.** `Process()`'s capacity guard only bounds `widthSpread`:

```cpp
const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds - 1.0f / sampleRate);
const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
float timeL = std::max(0.001f, baseSeconds + modSeconds);
float timeR = std::max(0.001f, baseSeconds + modSeconds + widthSpread);
```

`timeL` (and the `baseSeconds + modSeconds` part of `timeR`) is `modSeconds`
added with **no bound of its own**. At `dtim = 1.0`, `baseSeconds ==
kMaxDelaySeconds == capacitySeconds` exactly (2.0s == 96000 samples at
48kHz). Any nonzero `modSeconds` (driven by `dmod`/Mod rate, independent of
`dwid`/Stereo width) then pushes the requested read past what the line
holds — the exact same "wraps to a much shorter lag" failure this change
exists to fix for `widthSpread`, but reached through the modulation term
with `dwid` pinned at 0 (width mechanism fully inert).

None of the three shipped capacity tests ever sets `p.dmod` away from its
`DelayParams` default of `0.0f`, so none of them can see this.

**Probe** (temporary `TEST_CASE`, unmodified production `Delay.hpp`):
mirrors `lfoPhase`'s own public advance/wrap formula outside the unit
under test purely to locate the call index where `sin(lfoPhase)` peaks,
fires a single impulse there with `dtim=1.0, dwid=0.0, dmod=1.0`, and
measures where the impulse reappears in the output (same technique the
shipped `MeasureReadAtLagSamples` uses).

Literal output:
```
[ADVERSARIAL modulation overrun] bestSin=0.999999046 expectedUnclampedLagSamples=103679.993 capacitySamples=96000 measuredLagSamples=7454 measuredPeakIndex=7454 peakAbs=0.6015625
```

The knob asked for ~103680 samples (~2.16s) of delay; the line actually
returned the impulse at **7454 samples (~155ms)** — a ~14x-shorter,
completely wrong lag, silently. `peakAbs=0.60` confirms this was a real,
non-degenerate peak, not measurement noise.

**Verdict:** GOT THROUGH. This is a live gap in the shipped capacity
coverage, not a hypothetical — the buggy behavior exists in the current
tree right now and all three capacity tests stay green.
**Blocks delivery** (a real, user-reachable capacity overrun the change's
own stated purpose is to close, left open by knob combinations the shipped
tests don't sweep — Mod rate x Delay time, independent of Stereo width).
Does not block execution of unrelated work, but should block this change's
own delivery until either the guard covers `modSeconds` fully-independently
of `widthSpread`'s own bound, or a fourth test sweeps `dmod` the way the
grid-sweep test already sweeps `dtim`/`dwid`/`widthBalance`.

---

## Attack 2 (CAUGHT): width as a pure per-channel gain pan instead of a decorrelating read offset

Per the assignment's explicit attack class: mutated `Process()` so
`widthSpread` is computed but never applied to the read time (`timeL ==
timeR` always), and Stereo width instead scales `dL`/`dR` by a strictly
positive, never-crossing-zero per-channel gain (`dL *= 1-0.3*dwid*wb`,
`dR *= 1+0.3*dwid*wb`) after the (now-identical) reads.

Literal output at the first grid point (`width=0.25`):
```
[delay cross-feed removal] width=0.25 |corr|=0.980831035 balance=0.110130206
[FAIL] ...decorrelates_the_feedback_pair_across_width: ...corrAbs < 0.55
```

**Verdict:** CAUGHT immediately, as the correlation bound's own design
predicts — Pearson correlation is invariant to a positive per-channel
scale, so a pure pan sits at `|corr| ~= 0.98`, nowhere near the `< 0.55`
bound. (It also fails `balance < 0.06` at 0.11, and separately breaks all
three capacity tests since `widthSpread` is dead — those are incidental to
this specific mutation's intent, not what makes the decorrelation check
itself work.) No delivery/execution impact — this is a negative result:
the check does what its own header comment claims for this exact class of
regression.

---

## Attack 3 (CAUGHT overall, but the liveness REQUIRE itself is defeated): non-finite channel

Per the assignment's explicit attack class: mutated `Process()` to divide
`dR` by `(1.0f - p.dwid)` right after the reads — a plausible "wrong
operator" regression that goes non-finite exactly at `dwid == 1.0`, one of
the sweep's own four grid points.

First mutation attempt (full sweep) failed earlier, at `width=0.25`
(amplification, not yet non-finite), before ever reaching `width=1.0`,
because `REQUIRE_TRUE` aborts the `TEST_CASE` on its first failing
assertion — so the sweep's own later grid points are unreachable once an
earlier one fails. To see the `dwid==1.0` non-finite case in isolation, a
second temporary probe ran only that grid point:

```
[ADVERSARIAL liveness] sawNonFiniteChannelSample=1 rmsL=0.699362362 rmsR=nan livenessGate(rmsL!=0&&rmsR!=0)=1 corrAbs=1 (corrAbs<0.55 -> 0) balance=1 (balance<0.06 -> 0)
```

**What this shows:** `rmsR` is `nan`. The liveness assertion
(`REQUIRE_TRUE(rmsL != 0.0 && rmsR != 0.0)`) reads **true** — `livenessGate=1`
— because IEEE-754 `NaN != 0.0` is true. The gate that exists specifically
to catch "a silently dead measurement... reading as in bounds for the
wrong reason" (the test's own comment) reads a *poisoned* channel as more
comfortably alive than a merely-quiet one: a channel that actually went to
zero would trip this same `!= 0.0` check and fail loudly, while a channel
that went to NaN sails through it. This is the "passes MORE comfortably
when the thing it measures is dead" pattern named in the brief.

The `TEST_CASE` as a whole still fails, but only because
`Correlation::Value()` and the `balance` computation both have their own
independent NaN-safety fallback (`den > 0.0 ? ... : 1.0`, and
`(rmsL+rmsR) > 0.0 ? ... : 1.0`) that happens to produce a value outside
each bound. That is a coincidence of two unrelated defensive ternaries, not
a property of the liveness check — if either downstream bound were ever
loosened (e.g. `< 1.5` instead of `< 0.55`) independently of the liveness
check being fixed, a non-finite channel would pass the whole test.

**Verdict:** GOT THROUGH at the level of the specific liveness assertion;
the `TEST_CASE` as a whole still fails today via two unrelated fallback
paths. **Blocks execution** in the narrow sense that this specific
`REQUIRE_TRUE` line does not do what its comment says it does and should
be `std::isfinite(rmsL) && rmsL != 0.0 && std::isfinite(rmsR) && rmsR != 0.0`
(or check `wet.l`/`wet.r` per-sample the way `StateFinite()` elsewhere in
this file already does) rather than relying on the correlation/balance
bounds to catch non-finite values by accident. Does not block delivery of
the underlying capacity fix itself (no width/capacity mechanism is at
fault here — the injected division was a hypothetical mutation, not
something present in the real diff), but the liveness assertion's wording
should not ship making a claim it does not keep.

---

## Attack 4 (GOT THROUGH): capacity guard made too conservative, hidden by the tolerance window

Per the assignment's explicit attack class: widened the guard's headroom
from `1.0f / sampleRate` (1 sample, the real value) to a much larger
number, simulating a guard that shortens legitimate reads that were never
actually an overrun.

At **50 samples** of headroom (49 samples more conservative than
necessary):
```
[PASS] stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width
[PASS] stereo_delay_width_spread_never_reads_past_the_line_capacity
[PASS] stereo_delay_width_spread_bound_is_inert_away_from_capacity
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
```

Pushed to the edge of the shipped test's own tolerance — **99 samples** of
headroom (`REQUIRE_TRUE(lagSamples > capacity - 100)` is the exact bound
in `stereo_delay_width_spread_never_reads_past_the_line_capacity`):
```
[PASS] stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width
[PASS] stereo_delay_width_spread_never_reads_past_the_line_capacity
[PASS] stereo_delay_width_spread_bound_is_inert_away_from_capacity
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
```

Both still fully green. The reachable-grid sweep only asserts
`worstExcess <= 0.0` (an overrun-direction-only check — it has no lower
bound), the "inert away from capacity" companion test runs at `dtim=0.3`
(nowhere near the edge this guard exists to protect, so it never exercises
the change at all), and the near-capacity test's own 100-sample tolerance
window is exactly what absorbs a guard up to 99 samples too conservative.

**Verdict:** GOT THROUGH, confirmed at two points (50 and 99 samples of
excess conservatism), both fully undetected by all four shipped checks.
**Blocks neither execution nor delivery of the CURRENT diff** — the real
production guard is exactly 1 sample, correct — but it is a real coverage
gap: a future edit that regresses the headroom constant anywhere in
`[2, 99]` samples too conservative (silently shortening every near-max-time
+ near-max-width read by that many samples, an audible pitch/time error at
the top of the control's travel) would ship green. The 100-sample
tolerance should be tightened to something close to the intended 1-sample
margin (e.g. `> capacity - 4`) if this test is meant to also guard against
excess conservatism, not just against overrun.

---

## Summary table

| # | Attack class | Result | Blocks |
|---|---|---|---|
| 1 | Capacity regression via the modulation term, not width | **GOT THROUGH** (real, present in current tree) | delivery of this change |
| 2 | Width as a pure per-channel gain pan (correlation scale-invariance) | Caught | neither (negative result) |
| 3 | Non-finite intermediate vs. the liveness gate | Liveness assertion itself defeated; whole test still fails via two unrelated NaN-safe fallbacks | execution (fix the assertion's wording/logic before this change ships) |
| 4 | Capacity guard 1-99 samples too conservative | **GOT THROUGH** (up to 99 samples, both shipped capacity tests' tolerance absorbs it) | neither today (current guard is correct at 1 sample); flags a coverage gap for any future regression in that range |

`git status --short` at the end of this audit (see the auditor's own
final report) shows no residual diff in `app/dsp/Delay.hpp` or
`app/FroggersDspParityTests.cpp` beyond what was already `M`odified before
this audit started — every mutation above was reverted and verified by
`md5` immediately after its run.
