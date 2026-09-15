# Retired mechanisms — measurement transcripts

## Source: agent-a907e0197cbf52e08 — fixed-coupling constant measurements

I've completed the measurement task. Addressing the coordinator's corrections first, then the full report.

**Acknowledging receipt of both coordinator messages.** I widened the lag windows, ran the raw pre-limiter and full-scale balance variants, and ran down the 1408/2552/980 identification analytically and empirically rather than reporting a null. Findings below, including a correction to my own earlier framing (the K≥0.15 "match today's law" framing was wrong, per point 3 — replaced with the raw curves).

## Deliverable scope

No file under `app/` was touched. All work is in the scratchpad (`/private/tmp/claude-501/.../scratchpad/t14const/`): `Delay_probe_t14.hpp`, `measure_K.cpp`, `measure_K` (binary), `measure_K_output.txt`.

## 1. Harness diff

`Delay_probe_t14.hpp` = a byte-identical copy of the prior round's `Delay_probe_t12.hpp` (diffed, empty), which itself diffs against today's `app/dsp/Delay.hpp` as exactly: `crossProbe` (replaces `cross = p.dwid*0.5f*widthBalance`), `inRFactor` (left at 1.0 throughout, per instructions), and the lineR write's `inSignal * inRFactor` — nothing else.

I added one further scratch-only instrumentation pair on top of that, needed to answer constraint 2 correctly (see below): `probeWriteL`/`probeWriteR`, mirroring the exact value each `WriteSample` call stores, so the loop's own pre-limiter content is readable. Diff against the t12 copy:
```
+ float probeWriteL = 0.0f;
+ float probeWriteR = 0.0f;
...
-  WriteSample(inSignal*(1-freezeEff)+fbEff*Saturate(fbDrive*fbL), lineL);
-  WriteSample((inSignal*inRFactor)*(1-freezeEff)+fbEff*Saturate(fbDrive*fbR), lineR);
+  probeWriteL = inSignal*(1-freezeEff)+fbEff*Saturate(fbDrive*fbL);
+  probeWriteR = (inSignal*inRFactor)*(1-freezeEff)+fbEff*Saturate(fbDrive*fbR);
+  WriteSample(probeWriteL, lineL);
+  WriteSample(probeWriteR, lineR);
```
Read-only mirrors; nothing feeds back into `Process()`. `dsp::CrossFeedPair` was not touched.

## 2. Candidate grid

K ∈ {0.05, 0.10, 0.15, 0.20, 0.25, 0.30, 0.35, 0.40, 0.45}, step 0.05 across the open interval (0, 0.5). K=0 and K=0.5 are carried only as **references** (both independently disqualified by the task's own framing), plus a dynamic "today's law" reference row (`cross = w*0.5`) for sanity-checking the harness.

## 3. Positive control (§6.1)

At w=0.0, |corr| reads exactly **1.000000** for every law × every feedback row (0.00/0.35/0.70), with rmsL/rmsR nonzero every time (ruling out the VOID signature). All controls passed.

## 4. Bit-exactness — lead finding, refutes an assumed premise

**The claim "every candidate preserves the registered default bit-exactly at any K" is false.** At dwid=0, over 3200 samples:

| dfbk | K values that PASS (bit-identical) | K values that FAIL |
|---|---|---|
| 0.00 | all (0.05–0.50, today's law) | none |
| 0.35 | 0.05, 0.10, 0.20, 0.25, 0.30, 0.45, 0.50 | **0.15** (mismatch @1036), **0.35** (@977), **0.40** (@1031) |
| 0.70 | 0.05, 0.10, 0.20, 0.25, 0.30, 0.45, 0.50 | **0.15** (@1035), **0.35** (@943), **0.40** (@952) |

Mechanism, verified rather than assumed: at dwid=0, dL==dR bit-exactly (widthSpread=0), so `CrossFeedPair(a,a,K) = a*(1-K)+a*K` — but this is two separate float32 multiplies and an add, not `a*1.0`; whether `(1-K)+K` rounds to exactly 1.0f in float32 is a per-value accident, not a general guarantee (only K=0 is guaranteed exact, by the zero-multiply). At dfbk=0.00 there is no feedback loop (fbEff=0), so nothing recirculates and the accident never surfaces regardless of K. At nonzero feedback, the tiny per-sample rounding residual recirculates through ~950–1035 samples of feedback before becoming visible as a bit-level divergence — a slow-onset failure a shorter test would have missed, which is exactly why the task specified ≥3000 samples.

**New hard constraint this creates:** bit-exactness of the registered default is per-exact-K, not inferable from neighboring grid values or from the algebra. Whichever K is ultimately chosen must be individually verified bit-exact at dwid=0 across the feedback range before being treated as a "free" default-preserving change.

## 5. Constraint 1 — width must widen

Every law tested (K=0 through K=0.5, today's law) shows |corr| **monotonically non-increasing** across the width grid {0, 0.25, 0.5, 0.75, 1.0}, at all three feedback rows, on band-limited noise. rmsL/rmsR nonzero throughout (no void signature). This holds even at K=0.5 — width still narrows |corr| from 1.0 down to ~0.35–0.37, just from a much higher floor than smaller K. **Constraint 1 is satisfied by every candidate in the grid.**

## 6. Constraint 2 — coupling

**Lead metric: cross-correlation peak between wetL and wetR** (impulse response, dfbk=0.70, w=0.25, lag search widened to ±3200 samples after your correction — the earlier ±1500 window was too narrow and I should not have reported a null from it):

| K | 0.00 | 0.05 | 0.10 | 0.15 | 0.20 | 0.25 | 0.30 | 0.35 | 0.40 | 0.45 | 0.50 | today's law |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| peak value | 0.648 | 0.716 | 0.777 | 0.830 | 0.875 | 0.913 | 0.943 | 0.966 | 0.982 | 0.991 | 0.995 | 0.804 |

Curve is smooth and monotonic, no sharp knee — increments shrink continuously (K=0→0.05: +0.068; 0.05→0.10: +0.061; … 0.40→0.45: +0.009; 0.45→0.50: +0.004), an asymptotic approach toward the K=0.5 ceiling. Today's law's own value (0.804) sits between K=0.10 and K=0.15. Peak lag is 41 samples (= timeR−timeL) for every K>0, i.e. coupling manifests as R becoming a delayed copy of L's content, not a lag shift. **Per your point 3, I am not selecting a "recovered with margin" cutoff — this is the full curve for that rule to be applied to.**

**Same-channel dominant-lag flag (global argmax of each channel's own autocorrelation) is uninformative, as you flagged**: it reads DECOUPLED (469 vs 510) for every K from 0 through 0.45 *and* for today's law, flipping to "coupled" (469/469) only at exactly K=0.5. It doesn't discriminate anywhere in the candidate range and should not be read as a negative result about the candidates.

**Identification of the cited 1408/2552/980** (ran down per your point 2, not left as a disagreement):
- Analytic: at dtim=0.30, w=0.25, `dsp::ExpMapCompute` gives timeL=469.41 samples, timeR=510.48 samples. **timeL+timeR = 979.89** (cited 980, 0.01% off). **3×timeL = 1408.22** (cited 1408, 0.016% off). **5×timeR = 2552.40** (cited 2552, 0.016% off). Too precise to be coincidence.
- Empirically confirmed for K=0: normalized same-channel autocorrelation at the exact cited lags (dfbk=0.70, w=0.25) —

  | K=0.000 | lag=469 | lag=510 | lag=980 | lag=1408 | lag=2552 |
  |---|---|---|---|---|---|
  | channel L | 0.561 | 0.000 | 0.000 | **0.256** | 0.000 |
  | channel R | 0.000 | 0.553 | 0.000 | 0.000 | **0.122** |

  This is a genuine, real, non-noise secondary peak in each channel's own signal — L rings at its own fundamental (469) *and* a real, measurable 3rd-harmonic feature at 1408; R at its fundamental (510) *and* a real 5th-harmonic at 2552 — with R showing exactly zero at L's frequencies and vice versa (fully decoupled channels, confirming the finding qualitatively). My earlier global-argmax search simply reported the numerically larger fundamental instead of these real secondary peaks; that was a choice-of-statistic gap, not a window-truncation gap or a fabricated non-reproduction. **1408/2552 reproduce.**
- Partially confirmed for today's law's cited 980/980: same diagnostic at K=today's-law shows only a weak signature at lag=980 (0.058–0.059 on both channels) — present, but not clearly "dominant" in this harness's own signal at this operating point. The analytic match (979.89 vs 980) is compelling, but I cannot independently certify 980 as a *prominent* feature here, only that it exists faintly. I'm reporting this as partially resolved rather than fully closing it.

**Channel-level balance |rmsL−rmsR|** — measured three ways, all at the cited operating point (dfbk=1.00→0.98, w=0.25):

| law | post-limiter (2b) | pre-limiter raw (2b-raw) | full-scale sustained (2b-fullscale) |
|---|---|---|---|
| K=0.000 | 0.005675 | 0.007331 | 0.000000 |
| K=0.050 | 0.006814 | 0.007611 | 0.000000 |
| K=0.100 | 0.004420 | 0.004825 | 0.000000 |
| K=0.150 | 0.002787 | 0.003003 | 0.000000 |
| K=0.200 | 0.001766 | 0.001880 | 0.000000 |
| K=0.250 | 0.001114 | 0.001173 | 0.000000 |
| K=0.300 | 0.000688 | 0.000718 | 0.000000 |
| K=0.350 | 0.000403 | 0.000420 | 0.000000 |
| K=0.400 | 0.000210 | 0.000222 | 0.000000 |
| K=0.450 | 0.000078 | 0.000090 | 0.000000 |
| K=0.500 | 0.000013 | 0.000000 | 0.000000 |
| today's law | 0.003508 | 0.003803 | 0.000000 |

Two hypotheses for the cited 0.093/0.012 (16x my own gap of ~1.6x on band-limited noise) were tested and **ruled out**: (a) tap point — post-limiter vs pre-limiter raw are nearly identical here, the limiter never engages at these levels (~0.05–0.07 RMS, well under its 0.72 threshold); (b) full-scale sustained stimulus — a constant DC-like input drives both channels to an identical steady state by symmetry (diff exactly 0.000000), ruling that out cleanly. **I did not find the operating condition or exact metric definition that reproduces 0.093/0.012 in the time available; reporting this as unresolved, with the two ruled-out hypotheses stated, rather than papering over it or promoting a different metric to replace it.** The qualitative direction (K=0 worse than today's law, monotonically improving with K) holds in both my metrics.

## 7. Constraint 3 — mono-collapse ceiling

Fine sweep, dfbk=0.70, band-limited noise, no threshold assumed:

| K | \|corr\| w=1.00 | \|corr\| w=0.25 |
|---|---|---|
| 0.025 | 0.242 | 0.532 |
| 0.100 | 0.253 | 0.605 |
| 0.200 | 0.274 | 0.676 |
| 0.300 | 0.302 | 0.726 |
| 0.400 | 0.335 | 0.762 |
| 0.475 | 0.361 | 0.783 |

No candidate shows a literal flat-narrow collapse across the width grid (that's specific to K=0.5's internal write, per §6). The content-coupling curve from §6 (0.648→0.995) is the more direct signature of "how close to the K=0.5 mono-write defect the internal content is" — at K=0.45 the internal content is already 99.1% of the way to K=0.5's near-total mono value. **No numeric ceiling was given in the task, so none is asserted here — this is the curve for that judgment to be applied to.**

## 8. Constraint 4 — per-sample write bound

Bound = |inSignal|+fbk = 1.98. Worst observed across every candidate, every width, both channels: ≈0.800001–0.800006 — roughly 40% of the bound, essentially flat across all K. This is structural: `Saturate()` clamps its argument to ±1 regardless of the cross weight, so cross (K) cannot affect this bound. **All candidates pass, trivially and for a provable reason, not a coincidence of the grid.**

## 9. Constraint 5 — tonal material

Sustained 220Hz sine, amplitude 1.0, dtim=0.30, w=1.00:

| K | dfbk=0.35 | dfbk=0.70 | dfbk=0.90 | dfbk=1.00 |
|---|---|---|---|---|
| 0.05 | 0.381 | 0.774 | 0.933 | 0.973 |
| 0.15 | 0.299 | 0.638 | 0.814 | 0.875 |
| 0.25 | 0.208 | 0.453 | 0.597 | 0.654 |
| 0.35 | 0.114 | 0.249 | 0.326 | 0.357 |
| 0.45 | 0.023 | 0.062 | 0.082 | 0.089 |
| 0.50 | 0.0189 | 0.0188 | 0.0188 | 0.0188 |
| today's law | 0.0189 | 0.0188 | 0.0188 | 0.0188 |

Today's law and K=0.5 are identical here by construction (today's law's own cross at w=1.0 is exactly 0.5). **This means the only K that reproduces today's ~0.019 tonal figure is K=0.5 — which constraint 3 independently disqualifies.** Every K<0.5 costs a real, monotonically-growing tonal-decorrelation penalty (1.2x at K=0.45/dfbk=0.35 up to ~50x at K=0.05/dfbk=1.00) with no numeric "not much worse" bound given to test against — reported, not adjudicated. Separately: my own K=0 range here (0.42–0.99) only partially overlaps the cited "0.94–0.99" (matches at the two highest-feedback rows, not at 0.35/0.70) — an open, unresolved discrepancy distinct from constraint 2's now-largely-resolved ones; I did not find the alternate quantity for this one.

## Final section

No candidate in the grid satisfies all five constraints without a stated, unresolved cost:
- **Constraints 1 and 4 are satisfied by every candidate**, unconditionally.
- **Constraint 2's own curve is reported in full** (cross-correlation peak 0.648→0.995, balance diff shrinking geometrically toward 0); no knee is picked, per your instruction.
- **Constraint 3 has no stated numeric ceiling**; the content-coupling curve is the data for applying one.
- **Constraint 5 has no stated numeric bound**; only K=0.5 reaches today's law's tonal figure, and K=0.5 is disqualified by constraint 3 — this is a structural conflict between constraints 3 and 5 that no K strictly between 0 and 0.5 resolves. That is a complete, legitimate finding on its own, not something for me to compromise past.
- **Bit-exactness at the registered default is not a free property of any K** — it must be verified individually for whichever exact value is chosen; three of nine grid candidates (0.15, 0.35, 0.40) fail it at nonzero feedback despite passing the algebraic argument.

The choice of K, including whether the constraint-3/constraint-5 tension is acceptable at any point, is not made here.

---

## Source: agent-a72dded46d8895a78 — adversarial pass on the tonal benchmark

## Report: Adversarial test of the 0.019 |corr| claim

**Verdict: CLAIM FALSIFIED.** At the cited operating point (dtim=0.30, w=1.00, tap = `Process`'s returned `DelayWetPair`), `|corr|` is a rapidly oscillating function of tone frequency ranging from **0.00032 to 0.99998** (essentially the full possible range), with median ≈0.71 and mean ≈0.64 across all four `dfbk` values. The cited 220 Hz figure (reproduced almost exactly: 0.0189–0.0190 across `dfbk` ∈ {0.35,0.70,0.90,1.00}) sits in the bottom ~1% of tested frequencies — it lands almost exactly on a trough of the oscillation, not on a typical or stable value.

### Harness verification
Diffed the supplied scratch header against today's `app/dsp/Delay.hpp` (60992 bytes, unmodified by me). The diff is exactly and only:
- adds `crossProbe` (default 0.0f) and replaces `const float cross = p.dwid * 0.5f * widthBalance;` with `const float cross = crossProbe;` inside `Process()`.
- adds `inRFactor` (default 1.0f, left untouched here) multiplying only lineR's write-time input, and mirrors the pre-limiter write values into `probeWriteL/R` (unused by this test).
- no other line differs; `dsp::CrossFeedPair` itself is untouched.

To reproduce the shipped law at w=1.00 exactly, I set `crossProbe = 0.5f` (since `p.dwid*0.5f*widthBalance == 1.00*0.5f*1.0f == 0.5f` at this operating point) and left `inRFactor = 1.0f`. Sanity check against the claim: `crossProbe=0.5`, dtim=0.30, dwid=1.00, dsnd=1.0 (moved off its 0.0 default — required or `Process` early-returns and the wet pair is dead), amplitude 0.5 (matches this codebase's own "sustained tone" convention, e.g. `app/FroggersDspParityTests.cpp`'s "input amplitude 0.5, 220 Hz" comments), sample rate 48000 Hz, all other knobs (`dfrz`, `dmod`, `drev`, `ddif`, `widthBalance`) at their registered defaults, gave `|corr|` = 0.018977 / 0.018971 / 0.018956 / 0.018945 for `dfbk` = 0.35/0.70/0.90/1.00 — matches the cited ~0.019 to 4 decimal places.

Build: `clang++ -std=c++20 -O2 -I<repo>/External/Sheaf/projects/synth/include -I<repo>/app/dsp`, run under `nice -n 10`, entirely in scratchpad `adv-c/`. Never touched `app/` or `app/build/`.

### Frequency grid
Linear 20 Hz–20000 Hz in 20 Hz steps (1000 points). Rationale: since `cross=0.5` makes `CrossFeedPair` write bit-identical content to both delay lines (fed.a and fed.b are IEEE-754-identical by commutativity of `+`), lineL and lineR are the same signal, and the two reads differ only by the read-time offset `widthSpread = dwid*baseSeconds*0.35*widthBalance ≈ 3.42 ms` at dtim=0.30. `openspec/changes/frogg3rs-delay-width-wysiwyg-repair/proposal.md` itself already notes "correlation of two taps of one sine is `cos(2πfτ)`, periodic" — predicted oscillation period in frequency ≈ 1/τ ≈ 292 Hz. A 20 Hz step gives ~14-15 samples per predicted cycle across the whole audible span (~68 cycles) — dense enough to resolve it if present. It was present.

### Steady-state method
Per (frequency, `dfbk`, cross-weight) point: ran in blocks of ≥20 tone periods (clamped to [4800, 9600] samples), tracking block RMS of the returned wet pair; declared steady state once 3 consecutive blocks changed RMS by <0.1%, then measured `|corr|` (using the codebase's own `struct Correlation` from `app/FroggersDspParityTests.cpp:9487`, reproduced verbatim) over 20 further blocks (hundreds of tone periods). Convergence never approached the 10-second/480000-sample cap (observed range 19200–67200 samples to converge, mean 27943) — every point genuinely settled, none was cut off mid-transient. `rmsL`/`rmsR` were checked nonzero at every point (min ~0.27, none near 0) — no VOID readings. No NaNs encountered anywhere in 8000 measured points (4 `dfbk` × 1000 freqs × 2 cross weights).

### Full results, shipped law (`cross=0.5`, reproducing `p.dwid*0.5*widthBalance` at w=1.00)

| dfbk | min\|corr\| | max\|corr\| | mean | p10 | p25 | p50 | p75 | p90 | value at 220 Hz |
|---|---|---|---|---|---|---|---|---|---|
| 0.35 | 0.000324 @ 9420 Hz | 0.999984 @ 3360 Hz | 0.6369 | 0.1571 | 0.3835 | 0.7082 | 0.9241 | 0.9884 | 0.018940 |
| 0.70 | 0.000324 @ 9420 Hz | 0.999984 @ 3360 Hz | 0.6369 | 0.1571 | 0.3835 | 0.7082 | 0.9241 | 0.9883 | 0.018933 |
| 0.90 | 0.000324 @ 9420 Hz | 0.999984 @ 3360 Hz | 0.6368 | 0.1571 | 0.3835 | 0.7082 | 0.9241 | 0.9881 | 0.018919 |
| 1.00 | 0.000324 @ 9420 Hz | 0.999983 @ 16200 Hz | 0.6368 | 0.1571 | 0.3835 | 0.7082 | 0.9240 | 0.9880 | 0.018908 |

Only 1.0% of the 1000 tested frequencies fall within ±0.01 of 0.019, at every `dfbk`.

Local window around 220 Hz (`dfbk=0.35`) confirms a smooth, fast oscillation, not noise:
```
100Hz 0.548  120Hz 0.847  140Hz 0.991  160Hz 0.956  180Hz 0.746
200Hz 0.400  220Hz 0.019  240Hz 0.434  260Hz 0.770  280Hz 0.966
300Hz 0.986  320Hz 0.826  340Hz 0.516  360Hz 0.112  380Hz 0.313  400Hz 0.681
```
220 Hz sits precisely in a narrow trough between two much higher plateaus — the choice of exactly 220 Hz, not the shipped law's general behavior, produces the low figure.

### K=0.125 comparison (same grid, `crossProbe=0.125`, everything else identical)
Fraction of tested frequencies where the shipped law (`cross=0.5`) performs **worse** (higher `|corr|`) than the fixed `K=0.125` weight:

| dfbk | fraction shipped worse than K=0.125 |
|---|---|
| 0.35 | 474/1000 = 47.4% |
| 0.70 | 456/1000 = 45.6% |
| 0.90 | 440/1000 = 44.0% |
| 1.00 | 432/1000 = 43.2% |

K=0.125's own curve has essentially the same range and shape (min≈0.001–0.006, max≈0.99997, mean≈0.637) — frequency, not the cross-feed weight, dominates this pathology at any fixed weight, which is itself evidence against treating any single-frequency reading as characteristic of "the law."

### Bottom line
CLAIM FALSIFIED. Range measured: `|corr|` ∈ [0.00032, 0.99998] across 20 Hz–20 kHz at every tested `dfbk`, mean ≈0.64, median ≈0.71. The cited 0.019 is not a stable, general property of the shipped law on tonal material — it is one narrow trough of a frequency-periodic function (period ≈290 Hz, exactly consistent with the two-tap phase-offset mechanism the task described), and 220 Hz happens to land almost exactly on it. A replacement law should not be benchmarked against 0.019 as if it were representative; a fair benchmark would need to characterize the whole oscillation (e.g. its envelope/mean/percentiles) rather than one frequency sample.

Files: harness at `/private/tmp/claude-501/-Users-diegoaguilar-canabal-Desktop/9f45ca23-7367-4d5e-b9b9-5371f8f0488e/scratchpad/adv-c/sweep.cpp` (and `sanity.cpp`), results at `.../scratchpad/adv-c/sweep_results.csv`, probe header verified at `.../scratchpad/t14const/Delay_probe_t14.hpp`. `app/dsp/Delay.hpp` was read-only throughout.

---

## Source: agent-a5359c3a492e4442e — adversarial pass on the repair-improves-width claim, plus its retraction under debate (periodicity argument and its retraction)



===== BLOCK 1 of 2 (timestamp: 2026-09-14T04:37:56.735Z) =====


# Adversary Report — `dsp::StereoDelay::Process` cross-feed retirement

## Setup verification
Diffed `t12measure/Delay_probe_t12.hpp` against `app/dsp/Delay.hpp` myself (`/private/tmp/.../scratchpad/adv-b/delay_diff.txt`): the only differences are the two new fields (`crossProbe`, `inRFactor`) and their two read sites (`cross = crossProbe` instead of `p.dwid*0.5f*widthBalance`; `lineR`'s write scaled by `inRFactor`). `inRFactor` held at `1.0f` throughout — confirmed by inspection of every call in my harness. Harness: `/private/tmp/.../scratchpad/adv-b/adv_harness.cpp`, compiled with the specified command under `nice -n 10`, output in `adv_output.txt` in the same directory. I did not touch anything under `app/`.

## Angles I chose, and why
The prior scratch work I found alongside the pointed-to harness (`t11/`, `t11b/`) had already run one stimulus (50Hz-lowpassed noise) at one delay time (dtim=0.3). To attack independently rather than rubber-stamp that work, I picked angles it hadn't covered:

1. **Re-derive and independently reconfirm the `dfbk=0` edge case analytically and empirically** (own seed, 3 delay times, 3 widths) — `FreezeFeedback(fbk,0,false)==fbk` bit-exactly means at Feedback=0 the cross-feed weight never reaches the write, so the two laws must be bit-identical.
2. **A genuinely different stimulus class: a sustained 220Hz sine tone**, not noise — because a plain tonal signal is what "stereo width" claims are usually judged against by ear, and a pure delay of periodic content behaves very differently from a pure delay of noise.
3. **A wider Feedback grid pushed to the 0.98 clamp** with a different noise seed/cutoff than the prior work, to see if the level-balance cost keeps growing.
4. **An impulse-response probe**: T60 decay time and a tail-tonality/period-matching metric, to check the "tail length" and "character of repeats" costs the task explicitly asked about.
5. **Direct read of `OutputLimiter::envelope`** (public field) for `wetLimiterL/R` under a hot transient, to test the exact asymmetric-ducking risk the file's own header comment names as the reason the limiters are per-channel.

## What I found

**(0) `dfbk=0` is a real hole in "at every Feedback setting."** Independently confirmed bit-identical output between retired and today's law across 3 delay times × 3 widths × my own white-noise seed, and this reproduces again as exact `TIE`s throughout the sine-tone grid's `dfbk=0.00` rows. At Feedback=0 the proposal produces **zero** change, not "more decorrelated." The claim's universal quantifier over "every Feedback setting" is false at the literal boundary.

**(1) Under a tonal stimulus, the claim reverses outright — not just ties.** At `dtim=0.30`, `w=1.00` (top of Stereo width's travel, squarely "above zero"), a 220Hz sine tone gives:
- `dfbk=0.35`: retired corr=-0.494, today corr=+0.019
- `dfbk=0.70`: retired corr=-0.939, today corr=+0.019
- `dfbk=0.90`: retired corr=-0.993, today corr=+0.019
- `dfbk=1.00`: retired corr=-0.954, today corr=+0.019

Today's shipped law achieves near-perfect decorrelation (~0.02) at max width/high feedback; the proposed law collapses to near-total correlation (image reads as mono/phase-inverted, not wide). This also reproduces at `dtim=0.05`, `w=1.00` for `dfbk` in {0.35,0.70,0.90,1.00}: retired 0.85–0.93 vs today's flat 0.76. Mechanism: with cross-feed retired, the only decorrelation left is a pure time offset between the two read pointers, and correlation of a phase-shifted periodic signal is `cos(Δφ)` — it can land anywhere in `[-1,1]` depending on the (delay-time, width, tone-period) combination. Cross-feed genuinely blends different content between channels and decorrelates tonal material far more reliably. This is a clean, reproducible reversal at a non-degenerate, in-range setting, not a cherry-picked single sample. Ran the same grid with dtim=0.70 and most (not all — `dfbk=0.35,w=1.00` ties near zero either way) rows do favor "retired," so the reversal is dtim/frequency-dependent, which is itself evidence against a universal law.

**(2) Channel level-balance cost is real, reproducible, and grows toward the feedback clamp.** Reconfirmed independently with a different noise seed/cutoff (120Hz) and a finer feedback grid up to the 0.98 clamp: the RMS-balance figure for "retired" is worse than "today's law" in essentially every row from `dfbk=0.70` through `1.00` (only one exception cell, `dfbk=0.90,w=0.50`), and the gap widens as feedback rises — up to ~8x worse balance at `dfbk=1.00,w=0.25` (0.093 vs 0.012). The sine-tone grid corroborates this at `dtim=0.30`/`0.05`. This directly falsifies "with no channel level-balance cost" for feedback settings from moderate to maximum.

**(3) Tail length: no cost found.** Impulse-response T60-to-(-60dB) was bit-identical between retired and today across every `(dfbk, w)` combination tried (both channels, `T60L=T60R=11` samples in every row) — cross-feed is a linear, energy-preserving mix, so it doesn't change the loop's decay rate. This component of the claim survives.

**(4) Tail character/tonality: a real, unmeasured-by-correlation cost.** In the impulse tail, today's law makes both channels' dominant resonance lock to the **same** lag (e.g. both channels peak at lag 980 at `dfbk=0.70,w=0.25`), while retiring cross-feed lets L and R ring at **independent, generally different** lags (e.g. 1408 vs 2552 at the same setting). Peak magnitudes are similar (~0.99 either way — not "more/less tonal" overall), but the two channels become two uncoupled single-line combs with unrelated resonant periods once cross-feed is gone. That reads as two echoes going out of sync/detuned across channels, not a clean stereo spread — exactly the "what a listener hears as width" gap the task asked me to attack. The correlation figure doesn't show this because it's about the coherence of *which* periodicity each channel locks onto, not the instantaneous linear correlation.

**(5) Limiter-asymmetry probe: suggestive but confounded, flagging rather than claiming.** At `dtim=0.0` the `widthSpread` term is numerically negligible regardless of `w` (baseSeconds is tiny), so my width sweep didn't actually vary the thing I meant to test — retired showed a flat, width-independent max L/R envelope gap (0.3436) larger than today's (0.328→0.248, shrinking with width), consistent with the file's own comment's worry, but I don't trust this specific run enough to assert it; it needs a non-degenerate delay time to be a real test, and I didn't spend further budget re-running it correctly. I'm reporting the confound rather than passing this off as confirmation.

**No degenerate results.** Across ~250+ grid points (sections 0–2) no `NaN`/non-finite outputs and no zero-RMS ("dead line") rows were flagged by my harness's checks.

## Verdicts

**(1) FALSIFIED** — breaks on two independent grounds: (a) `dfbk=0` produces bit-identical output (not "more decorrelated," just unchanged), violating "at every Feedback setting"; (b) under a sustained 220Hz tone at `dtim=0.30, w=1.00, dfbk∈{0.35,0.70,0.90,1.00}`, the proposed law is dramatically *less* decorrelated than today's shipped law (e.g. corr -0.993 vs +0.019 at `dfbk=0.90`), violating "at every point of Stereo width's travel above zero"; and (c) the channel level-balance figure is reproducibly and increasingly worse under the proposed law from `dfbk≈0.7` to the clamp, violating "with no channel level-balance cost."

**(2) FALSIFIED** — independent of part (1)'s breaks, the tail-tonality probe shows the proposed law decouples the two channels' resonant repeat-rate (different dominant lags per channel) where today's law keeps them locked to a shared periodicity; that is a real, audible change in "character of the repeats" that a correlation number does not capture and that plausibly reads as a defect (out-of-sync echoes) rather than a wider image, on top of the demonstrated correlation reversals and level-balance cost above.


===== BLOCK 2 of 2 (timestamp: 2026-09-14T05:01:24.435Z) =====


## Adjudication: cross-channel resonance-lag measurement

**Bottom line up front:** their 469/510 is correct and real, but it doesn't test what my number claimed to test. My original 1408/2552 vs 980/980 comparison has a real defect and I'm retracting the specific numbers. A cleaner, structurally-guaranteed re-measurement supports the qualitative finding underneath it (today's law couples the channels' periodicities; the retired law provably does not) — but the "dominant lag" framing was the wrong metric, and I'm replacing it.

### 1. Exact method behind my original 1408/2552 and 980/980

Signal: `StereoDelay::Process`'s full returned `DelayWetPair` (post-diffusion, post-wet-limiter — everything a listener would hear), driven by a single unit impulse at n=0 then zero, for 40000 samples, at `dtim=0.30, dwid=0.25, dfbk=0.70`, `crossProbe=0.0` ("retired") vs `crossProbe=0.125` (`=w*0.5`, "today"). `outL`/`outR` cast to `double` per sample.

"Dominant lag" = `argmax` over `lag ∈ [100, 4000]`, stepping by 4, of `|Pearson autocorrelation(x[i], x[i+lag])|`, computed **only over samples 20000–39999** (the second half of the buffer) — one scalar Pearson correlation per candidate lag, whichever lag scores highest wins, independently for L and R. That's the whole method; no windowing, no FFT.

### 2. Re-run, plus timeL/timeR directly

Re-running that exact method now reproduces my original numbers exactly: retired → L=1408 (val 0.9986), R=2552 (val 0.9955); today → L=980 (val 0.9994), R=980 (val 0.9990). Not a transcription error.

Direct computation: `baseSeconds = ExpMapCompute(0.001, 2.0, 0.30) = 0.009779s`; `timeL = baseSeconds = 469.41 samples`; `timeR = baseSeconds + 0.25·baseSeconds·0.35 = 510.48 samples`; `timeL+timeR = 979.89`. **Their arithmetic is right, and it exposes a real problem**, though not quite the one they framed: I checked directly, and 980 is not literally `timeL+timeR` computed anywhere in the signal path — it's the argmax landing near that value in the deep tail. But that closeness is not coincidence either (see below). More importantly: block-peak amplitude in my tail window (samples 20000–24000) is already down to **3.4e-8 (retired) / 3.5e-9 (today)** relative to an initial peak of ~0.6 — 140–170 dB down. The samples there are still legitimate float32 arithmetic (not denormal garbage), but they are the *asymptotic remnant* of a comb filter that has been through 40+ passes of a one-pole tone filter, which progressively narrows which spectral teeth of the comb survive. My argmax was searching that asymptotic remnant, not "the" resonance.

**The defect, confirmed directly:** I printed `|autocorrelation|` at the exact fundamental (469/470), 2nd harmonic (939/940), and 3rd harmonic (1408/1409) on the **full** 40000-sample retired-law signal. All three are comparable magnitude (0.54–0.66) — because a single un-cross-fed comb driven by one impulse has *exactly zero* autocorrelation at every non-multiple of its period and *comparably-sized, non-decaying-in-relative-terms* autocorrelation at *every* multiple of it. A coarse-stride global-argmax over a signal shaped like that isn't measuring "the resonant period" — it's picking whichever harmonic best aligns to an integer lag under the chosen window/stride, which is a quantization artifact, not physics. I verified the instability directly: three different reasonable choices on the *identical* retired-law signal gave three different "winners" — 470 (fundamental, via a first-peak scan on the full signal), 939 (2nd harmonic, via argmax on the full signal restricted to lag≤1200), and 1408 (3rd harmonic, via argmax on the tail restricted to lag≤4000, my original method). None of these is wrong; all are the same period's harmonics. **This is the specific defect in my method: global-argmax lag search is not a well-posed "dominant period" estimator for a signal that is structurally a pure comb, and restricting to the deep, filtered tail made the harmonic selection even more window-dependent, not less.**

### 3. Reproducing 469/510, and what actually distinguishes the two laws

I reproduced their number: using a "first local maximum above a small threshold, scanning lag upward from 0" search (a pitch-period-style method, not a global argmax) on the full signal, restricted to lag≤1200, I get **470/511 at `crossProbe=0` and at `crossProbe=0.0625`** — matching their 469/510 to within 1 sample, and matching it "for every cross weight tested" in that low range, holding `dwid` (hence `timeL`/`timeR`) fixed and varying `crossProbe` alone.

**What their method is doing:** a channel's own read-time period is the first and largest peak encountered scanning outward from lag 0, in *both* laws, because cross-feed only adds a smaller secondary correction — it doesn't relocate the primary near-field peak at these cross weights. So 469/510 is real, but it is close to invariant to cross-feed *by construction* for the same reason the very first echo is (§ established already: cross only touches the recirculating write, and a channel's own fundamental dominates its own near-field spectrum regardless of how much of the other channel leaks in at these weights). **Their measurement and mine were of different quantities**: theirs is "where does each channel's own dominant near-field peak sit" (expected to be cross-invariant, and correctly found to be so); mine was "which lag wins a coarse global argmax in an already-decayed comb tail" (ill-posed, as shown above). Neither number, by itself, settles whether coupling occurs — theirs isn't built to detect it, and mine measured a harmonic-selection artifact instead of it.

**The clean test, run directly:** whether channel L's autocorrelation has *any* energy at R's own fundamental lag (510), and vice versa (R at 469) — holding `dwid=0.25` fixed, sweeping only `crossProbe`:

```
cross=0.0000  acL(lag=510)=0.000000  acR(lag=469)=0.000000
cross=0.0625  acL(lag=510)=0.067995  acR(lag=469)=0.039494
cross=0.1250  acL(lag=510)=0.149181  acR(lag=469)=0.081931
cross=0.1875  acL(lag=510)=0.240047  acR(lag=469)=0.125460
cross=0.2500  acL(lag=510)=0.335921  acR(lag=469)=0.168555
cross=0.3750  acL(lag=510)=0.520147  acR(lag=469)=0.248759
cross=0.5000  acL(lag=510)=0.660413  acR(lag=469)=0.315642
```

At `cross=0` this is **exactly 0.000000**, not approximately — a mathematical certainty, not a measurement: when `cross=0`, `fbL=dL` and `fbR=dR` exactly (from `CrossedPair(dL,dR,0)`), so L's recirculating content can never contain any component driven by R's line, and L's whole impulse response is a pure period-469.41 train with, provably, zero energy at any non-multiple of that period. It then grows smoothly and monotonically with cross weight. This is the correct, robust replacement for my original claim.

One more honest disclosure: past `cross≈0.1875` at this same operating point, the first-local-max search collapses to lag=41 for both channels — a much shorter periodicity takes over as the first prominent peak once cross weight grows past roughly today's-law value. I haven't chased down the mechanism (plausibly an interaction between cross-mixing and the tone-filter/saturator producing a faster oscillatory mode) and I'm not claiming to understand it; flagging it rather than omitting it.

### Verdict on this sub-argument

**RETRACTED, with the specific defect named:** the numbers 1408/2552 ("decoupled") and 980/980 ("shared periodicity") are artifacts of running a coarse global-argmax lag search over the deep, tone-filtered tail of a signal that is structurally a pure comb — a class of signal for which that search is not a stable descriptor, as shown by getting three different "winners" (469, 939, 1408) from three reasonable window/stride choices on the same retired-law signal. Their 469/510 is correct but measures a different, largely cross-invariant quantity (each channel's own near-field dominant period) and does not itself settle the coupling question either way.

**HELD, on different evidence:** the qualitative claim that today's shipped law couples the two channels' recirculating periodicities while the retired law categorically does not is still true, and now rests on a structurally-guaranteed measurement rather than an argmax: cross-channel leakage (autocorrelation of one channel at the other's own fundamental lag) is exactly zero at `cross=0` by construction and grows monotonically and substantially (0 → 0.66) as cross-feed is restored. This is weaker in one respect than my original framing (it's a leakage/coupling-strength statement, not a "the channels ring at unrelated pitches" statement) but it is the part of finding (4) in my prior report that survives, and it should replace the retracted lag numbers wherever that report is used as evidence for a cost of retiring the cross-feed.
