# Cross-feed widening — measurement transcripts

## Source: agent-aab988ce7a8f0ba26 — whether the cross-feed can widen at all (Task 1.1)

## Report — Task 1.1: Can the cross-feed widen this signal at all?

### 1. One-line answer

**No.** Across the full documented weight range `cross ∈ [0, 1]`, every value only ever *re*-correlates the wet pair relative to its own `cross == 0` baseline — never below it — reaching exact mono at 0.5 in the open loop and, once the feedback loop recirculates it, rising monotonically all the way to `cross == 1.0` (Feedback 0.7: baseline −0.0023 → 0.2544). At **Feedback 0.0 the effect is stronger than "doesn't widen": `cross` has *zero* effect on the wet pair at all** — every grid value, including the negative ones, reproduced the identical correlation to 15 decimal digits (`-0.005610245849400`). Only **negative** weights — outside `CrossFeedPair`'s documented `[0, 0.5]` domain, where it forms a difference instead of a blend — can decorrelate the pair below its own baseline, and only once they cross roughly −0.35 in the closed loop (before that they hover near baseline). This is exactly the finding the proposal predicted from the arithmetic; no combination of the tested knobs falsified it.

### 2. Diff of scratch copy vs. `app/dsp/Delay.hpp`

Scratch copy at `/private/tmp/claude-501/.../scratchpad/t11/Delay_probe.hpp`. `dsp::CrossFeedPair` (`app/dsp/StereoField.hpp`) was not touched.

```diff
--- app/dsp/Delay.hpp
+++ Delay_probe.hpp
@@ -294,6 +294,12 @@
     // today's 0.35f/0.5f literals exactly (see Process() below and
     // SetWidthBalance).
     float widthBalance = 1.0f;
+
+    // SCRATCH PROBE (task 1.1 harness only, not shipped): overrides the
+    // cross-feed weight in Process() below, decoupling it from
+    // p.dwid/widthBalance so the harness can hold widthSpread fixed while
+    // sweeping cross on its own.
+    float crossProbe = 0.0f;
 
     // (Delay slot 13, "Crush" / "Crsh"): feedback-tap crush, reused
     // dsp::SampleRateReducer AS-IS (Drive.hpp) rather than a hand-rolled
@@ -717,7 +723,7 @@
         // cross-feed's own bound -- half of a width blend that widthBalance
         // then scales further, not a quantity shared with any other call
         // site's cross-feed weight.
-        const float cross = p.dwid * 0.5f * widthBalance;
+        const float cross = crossProbe;
         // dsp::CrossFeedPair (dsp/StereoField.hpp) is identity on its first
         // argument at cross == 0, which is exactly this call's own zero-cross
         // behavior (fbL == dL, fbR == dR) -- passed in the same order the
```

The only functional line changed is the `cross` assignment; the added field is the probe it reads.

### 3. Grid, tap point, knobs moved off default, full tables

**Sample rate** 48000 Hz. **Noise burst**: LCG seed `20260913`, same idiom as `reverb_damping_filter_split_lowers_wet_leg_correlation_at_every_setting` (`app/FroggersDspParityTests.cpp` ~9532), 12000-sample warmup discarded, 12000-sample measure window. **Correlation accumulator**: `struct Correlation` reused verbatim from `app/FroggersDspParityTests.cpp:9487`.

**Grid for `cross`:** `{0.0, 0.1, 0.2, 0.25, 0.3, 0.4, 0.5, 0.6, 0.75, 0.9, 1.0}` plus negative `{-0.5, -0.25, -0.1}` (and, for characterizing the negative regime, an extra finer scan reported separately below).

**Knobs moved off their registered `0.0f` default, and why:**
- **Send (`dsnd`) → 1.0**: at its registered default the wet path is never fed (Process's own `p.dsnd <= 0.0001f` early return) — a flat +1 VOID reading, not a measurement.
- **Stereo width (`dwid`) → 0.5, held fixed for every row**: at its registered default both `widthSpread` and `cross` are exactly zero (a provable no-op per the proposal's constraint map item 5), useless for this question. 0.5 is a plain mid-travel value, clear of the `dwid==0` no-op and the `dwid==1` edge, giving `widthSpread = 0.001711 s = 82.15 samples` — large enough to be a real desync, not a sub-sample rounding artifact.
- **Delay time (`dtim`) → 0.3**: also registered `0.0f` by default. Reused rather than invented — it is the exact value `stereo_delay_default_knob_values_reproduce_original_output_exactly` already uses. `dtim=0.0` gives the *shortest* reachable delay (~48 samples), an edge case already covered by the golden-vector test; 0.3 gives `baseSeconds = 0.009779 s = 469.41 samples`, a plain mid value that keeps `widthSpread` (and hence the tap desync) unambiguous.
- **`cross` itself** is driven directly by the scratch probe, not by `dwid`/`widthBalance` — that is the whole point of the decoupling.
- **Feedback (`dfbk`)**: the two required rows, 0.0 and 0.7, per the task.

**Knobs left at their registered defaults, and why it doesn't matter here:** Wet/dry (`dmix=0.0`, unused by `Process` — only `ToStereo`, which this harness never calls), Freeze (`dfrz=0.0`), Mod depth (`dmod=0.0` — deliberately kept at default so the LFO does not perturb `baseSeconds`/`widthSpread` sample-to-sample, which would violate "widthSpread held fixed"), Reverse blend (`drev=0.0`), Diffusion (`ddif=0.0`, provably identity at 0 per the header's own comment), Width balance (`widthBalance` = in-class default 1.0, `SetWidthBalance` never called), Feedback drive/tone, Mod rate, Crush (all left at their bypass in-class defaults — none is read anywhere near `cross`).

**Tap points:**
- **Open loop**: `dL`/`dR` from a standalone `OpenLoopTapLine` that replicates `StereoDelay::ReadAt`/`WriteSample`'s exact interpolation/wrap arithmetic against a line holding only `noise[n]*dsnd` — no feedback, no diffusion, no limiter, no reverse. `dsp::CrossFeedPair` (the real, unmodified helper) is applied directly to that `(dL, dR)` pair, once per grid point.
- **Closed loop**: the scratch-probed `StereoDelay::Process`'s **returned `DelayWetPair`**, i.e. post-Diffusion, post-wet-limiter — the same object the file's own `Process()` comment calls `lastWet`.

**1. Open-loop table** (`cross` applied directly to the two time-shifted taps, no recirculation):

| cross | corr(A,B) | finite |
|---|---|---|
| 0.0 | −0.005610 | yes |
| 0.1 | 0.214176 | yes |
| 0.2 | 0.466227 | yes |
| 0.25 | 0.596416 | yes |
| 0.3 | 0.721475 | yes |
| 0.4 | 0.922249 | yes |
| 0.5 | 1.000000 | yes |
| 0.6 | 0.922249 | yes |
| 0.75 | 0.596416 | yes |
| 0.9 | 0.214176 | yes |
| 1.0 | −0.005610 | yes |
| **−0.5** | **−0.603597** | yes (maxAbsSample 0.9246) |
| **−0.25** | **−0.389401** | yes (maxAbsSample 0.6993) |
| **−0.1** | **−0.185759** | yes (maxAbsSample 0.5696) |

The positive-weight row is an exact, symmetric hump around 0.5 (`corr(w) == corr(1-w)`, algebraically forced by `CrossFeedPair`'s symmetry) that never dips below the `cross==0` baseline. Every negative weight pushes correlation **below** that baseline, into negative (anti-correlated) territory, with no non-finite or out-of-[-1,1] values.

**2. Closed loop, Feedback 0.0** (`dwid=0.5` fixed, `dtim=0.3`, `dsnd=1.0`):

Every one of the 14 grid points (11 positive + 3 negative) produced the **bit-identical** correlation `-0.005610245849400` (verified to 15 decimal digits with a separate build). This is the arithmetic reason: at `dfbk=0.0`, `fbEff = FreezeFeedback(0,0,false) = 0`, so the write is `inSignal*(1-0) + 0*Saturate(...)` — the cross-fed term (`fed.a`/`fed.b` → `fbL`/`fbR`) is multiplied by zero before it ever reaches the line, and the wet output (`dL`/`dR` → diffusion → limiter) never reads `fed.a`/`fed.b` at all. `cross` genuinely cannot reach the output through this path when Feedback is at its registered default.

**3. Closed loop, Feedback 0.7** (same fixed geometry):

| cross | corr(l,r) | finite |
|---|---|---|
| 0.0 | −0.002322 | yes |
| 0.1 | 0.007220 | yes |
| 0.2 | 0.027009 | yes |
| 0.25 | 0.040074 | yes |
| 0.3 | 0.054775 | yes |
| 0.4 | 0.087581 | yes |
| 0.5 | 0.122544 | yes |
| 0.6 | 0.157176 | yes |
| 0.75 | 0.204078 | yes |
| 0.9 | 0.239901 | yes |
| 1.0 | 0.254405 | yes |
| **−0.5** | **−0.536917** | yes |
| **−0.25** | **0.008855** | yes |
| **−0.1** | **−0.001605** | yes |

Positive-weight correlation rises **monotonically** across the entire `[0,1]` grid (unlike the open-loop symmetric hump — see surprise #1 below), never falling below the `cross==0` baseline anywhere in the documented range.

**4. Negative-weight fine scan, Feedback 0.7** (extra characterization beyond the required 3 points, same fixed geometry):

| cross | corr | finite |
|---|---|---|
| 0.0 | −0.002322 | yes |
| −0.05 | −0.003139 | yes |
| −0.1 | −0.001605 | yes |
| −0.15 | 0.001800 | yes |
| −0.2 | 0.005987 | yes |
| −0.25 | 0.008855 | yes |
| −0.3 | 0.004686 | yes |
| −0.35 | −0.029316 | yes |
| −0.4 | −0.152830 | yes |
| −0.45 | −0.376303 | yes |
| −0.5 | −0.536917 | yes |
| −0.6 | −0.648184 | yes |
| −0.75 | −0.676208 | yes |
| −1.0 | −0.708121 | yes |

No `nan`, no non-finite value, and no out-of-[-1,1] correlation anywhere in any table above. `CrossFeedPair` is a purely linear, unconditionally finite combination, and the loop's own `PadeSaturator::Saturate` bounds every write before it re-enters the line, so nothing blows up even at `cross = -1.0`.

### 4. Positive control

Held `cross == 0` (identity, no cross-feed at all — open loop) and swept `dwid` so **`widthSpread`** (in samples) is the only thing moving, to prove the correlation instrument is live before trusting any "cross doesn't move it" reading:

| dwid | widthSpread (samples) | corr(dL,dR) |
|---|---|---|
| 0.0 | 0.00 | 1.000000 |
| 0.25 | 41.07 | 0.016821 |
| 0.5 | 82.15 | −0.005610 |
| 0.75 | 123.22 | 0.016937 |
| 1.0 | 164.29 | −0.004234 |

The controlling quantity is `widthSpread`, and it visibly moves the reading: from a trivial `1.0` at `dwid==0` (identity — the taps are literally the same read) to values scattered near zero once the taps desynchronize by tens of samples of broadband noise. The instrument registers movement; it is not dead. (This also explains *why* the open-loop `cross==0` baseline is near zero rather than near 1 at `dwid=0.5`: a fixed 82-sample offset between two reads of broadband noise is already close to fully decorrelated before any cross-feed is applied — `cross`'s whole job at that point is pulling two already-near-independent signals back together.)

### 5. Surprises / things not fully settled

- **The open-loop hump and the closed-loop monotone climb disagree in shape.** Single-shot `CrossFeedPair` algebra is symmetric about 0.5 (`corr(w) == corr(1-w)`), so in the open loop `cross=0.9` reproduces `cross=0.1`'s correlation exactly. In the closed loop at Feedback 0.7, correlation instead rises monotonically all the way from `cross=0` to `cross=1.0` with no fold-back. This is expected once you account for repeated recirculation (the cross-feed is reapplied every round trip to content that itself already passed through it), but I did not derive the closed-form reason the fold-back specifically disappears rather than merely flattening — I'm reporting the measured fact rather than a full derivation.
- **The negative-weight response in the closed loop is not monotonic near zero.** From `cross=0` down to about `-0.3`, correlation barely moves and even wobbles slightly positive (`-0.15` → `+0.0018`, `-0.25` → `+0.0089`) before falling off a cliff past roughly `-0.35` into strong anti-correlation, saturating near `-0.7` by `cross=-1.0`. I did not chase down why the transition sits specifically near `-0.3` to `-0.35` (a resonance/eigenvalue effect of the recirculating 2×2 cross-feed-plus-feedback map is my best guess, not a checked one) — flagging this rather than smoothing it into "negative weights decorrelate," which is true only past that threshold.
- Everything else came out exactly as the arithmetic in the proposal predicted, including the bit-exact Feedback-0.0 null result, which was a stronger and cleaner confirmation than I expected going in.

No production file was edited. `app/dsp/Delay.hpp` and `app/FroggersDspParityTests.cpp` are untouched; the only artifacts are `Delay_probe.hpp`, `probe_harness.cpp`, `verify_fb0.cpp`, `verify_neg.cpp`, and their compiled binaries, all under `/private/tmp/claude-501/-Users-diegoaguilar-canabal-Desktop/9f45ca23-7367-4d5e-b9b9-5371f8f0488e/scratchpad/t11/`.

---

## Source: agent-a5cfc8e6071d4d75b — routing-law measurements (Task 1.2: asymmetric input feed + cross weight)

## Report — Task 1.2: does asymmetric input feed + cross weight widen where symmetric feed cannot?

**Scope discipline honored: no file under `/Users/diegoaguilar-canabal/Desktop/frogg3rs/app/` was touched.** All work happened in the scratchpad at `/private/tmp/claude-501/-Users-diegoaguilar-canabal-Desktop/9f45ca23-7367-4d5e-b9b9-5371f8f0488e/scratchpad/t12measure/`: `Delay_probe_t12.hpp` (a further-modified copy of task 1.1's `Delay_probe.hpp`) and `measure_harness.cpp`.

### Prior artifact verified, not trusted blind

Diffed `t11/Delay_probe.hpp` against today's `app/dsp/Delay.hpp`. The only differences are exactly what its own comment claims: a new `float crossProbe = 0.0f;` field, and `const float cross = crossProbe;` replacing `const float cross = p.dwid * 0.5f * widthBalance;` at the `Process()` call site. Nothing else differs. I built `Delay_probe_t12.hpp` on top of that verified base.

### What I added to the scratch copy (full diff vs. today's `app/dsp/Delay.hpp`)

```
296a297,302
> (crossProbe field — carried over from t11, unchanged)
297a304,310
>     // SCRATCH PROBE (task 1.2/L1-L3 harness only, not shipped): multiplies
>     // the write-time input signal fed to lineR only, decoupling the
>     // feed-symmetry question (this task's L0-L3) from crossProbe above.
>     // Default 1.0f reproduces today's symmetric `inSignal` write to both
>     // lines exactly (see WriteSample calls below).
>     float inRFactor = 1.0f;
720c733
<         const float cross = p.dwid * 0.5f * widthBalance;
---
>         const float cross = crossProbe;
809c822
<         WriteSample(inSignal * (1.0f - freezeEff) + fbEff * PadeSaturator::Saturate(fbDrive * fbR), lineR);
---
>         WriteSample((inSignal * inRFactor) * (1.0f - freezeEff) + fbEff * PadeSaturator::Saturate(fbDrive * fbR), lineR);
```

`widthSpread`'s formula (`Delay.hpp:679`) is untouched, and `p.dwid` is always set to the real grid value `w` in every law, so widthSpread moves exactly as production for all four laws. `dsp::CrossFeedPair` (`app/dsp/StereoField.hpp:39`) was not edited.

**Law definitions used** (`w` = `p.dwid`, `widthBalance` = 1.0 in-class default, held fixed throughout):
- L0: `inRFactor=1.0`, `crossProbe = 0.5*w`
- L1: `inRFactor=1.0-w`, `crossProbe = 0.0`
- L2: `inRFactor=1.0-w`, `crossProbe = 0.5*w`
- L3: `inRFactor=1.0-w`, `crossProbe = w`

Build: `clang++ -std=c++20 -O2 -I<repo>/External/Sheaf/projects/synth/include -I<repo>/app/dsp measure_harness.cpp -o measure_harness` (the second `-I` was needed because `Delay.hpp` includes `"DspMath.hpp"`, which lives in `app/dsp`, not in the Sheaf include tree — t11 apparently used the same second include path). Single translation unit, so `-j2` does not apply; run under `nice -n 10`.

### Q1/Q4 — correlation and per-channel RMS, tap point = `Process()`'s returned `DelayWetPair` (post-Diffusion, post-wet-limiter)

**Feedback = 0.0**

| w | L0 corr | L1 corr | L2 corr | L3 corr | rmsL (all laws) | rmsR L0 | rmsR L1/L2/L3 |
|---|---|---|---|---|---|---|---|
|0.00|1.000000|1.000000|1.000000|1.000000|0.210336|0.210336|0.210336|
|0.25|0.016821|0.016821|0.016821|0.016821|0.210336|0.207170|0.155377|
|0.50|-0.005610|-0.005610|-0.005610|-0.005610|0.210336|0.208287|0.104144|
|0.75|0.016937|0.016937|0.016937|0.016937|0.210336|0.213390|0.053347|
|1.00|-0.004234|**1.000000 (VOID)**|**1.000000 (VOID)**|**1.000000 (VOID)**|0.210336|0.222644|0.000000|

**Feedback = 0.7**

| w | L0 corr | L1 corr | L2 corr | L3 corr |
|---|---|---|---|---|
|0.00|1.000000|1.000000|1.000000|1.000000|
|0.25|0.034467|0.022613|0.035302|0.061478|
|0.50|0.040074|-0.002506|0.044237|0.124507|
|0.75|0.082838|0.005074|0.077895|0.151233|
|1.00|0.108780|**1.000000 (VOID)**|0.056873|0.019491|

RMS at Feedback = 0.7:

| w | L0 rmsL/rmsR | L1 rmsL/rmsR | L2 rmsL/rmsR | L3 rmsL/rmsR |
|---|---|---|---|---|
|0.00|0.251675/0.251675|0.251675/0.251675|0.251675/0.251675|0.251675/0.251675|
|0.25|0.244579/0.243685|0.251675/0.195507|0.244345/0.185293|0.237228/0.180490|
|0.50|0.239262/0.238471|0.251675/0.132423|0.236326/0.125552|0.227070/0.134092|
|0.75|0.235427/0.238446|0.251675/0.067426|0.228218/0.082511|0.221454/0.116285|
|1.00|0.237081/0.248991|0.251675/0.000000|0.222517/0.073418|0.226870/0.136116|

**Mechanism found, not assumed:** at Feedback 0.0, `fbEff` is 0, so `crossProbe` never reaches `WriteSample` at all (matches the background note). The only thing that can move correlation is `widthSpread`'s time offset — identical across all four laws, since correlation is invariant to a positive scalar multiplier on one channel (`inRFactor` scaling). That is why L1/L2/L3's corr rows are bit-for-bit identical to L0's at Feedback 0.0 for w<1: **the feed-asymmetry manipulation is provably inert for correlation whenever feedback is off**, confirmed by the numbers, not inferred.

**Q1 answer per law/row — does correlation fall point to point across the five-point travel?**
- L0 (Fb 0.0): No. Falls 0→0.25, rises 0.25→0.5, falls 0.5→0.75, rises→1.0.
- L0 (Fb 0.7): No. Falls once (0→0.25), then rises at every remaining step (0.0345→0.0401→0.0828→0.1088).
- L1 (Fb 0.0): No — identical to L0 through w=0.75, then **VOID** at w=1.0 (rmsR=0, dead line: R never received input for the whole run since `inRFactor=0` held from warmup).
- L1 (Fb 0.7): No — falls twice, rises once, then **VOID** at w=1.0 for the same reason.
- L2 (Fb 0.0): No — identical to L1's row (cross is inert at Fb 0.0).
- L2 (Fb 0.7): No — falls once, rises twice, falls once (0.0779→0.0569).
- L3 (Fb 0.0): No — identical to L1/L2's row.
- L3 (Fb 0.7): No — falls once, rises twice, falls once (0.1512→0.0195).

No law falls monotonically at every step in either Feedback row. The only fall common to every row is the trivial crash from the w=0 identity (corr=1.0) to the first nonzero point; past that, correlation is non-monotonic and at Feedback 0.7 mostly **rises** with `w` for L0/L2/L3 — the same "cross-feed re-correlates rather than decorrelates" effect task 1.1 already measured. L3 (full ping-pong) is actually **more correlated than L0** at w=0.25/0.5/0.75 under Feedback 0.7 (e.g. 0.1245 vs 0.0401 at w=0.5), only dropping below L0 at the very top of travel (w=1.0: 0.0195 vs 0.1088).

**Q4 answer per law — is a widening reading actually a pan?**
- L1: yes, unambiguously. `rmsR` falls monotonically and lands at **exactly 0.000000** at w=1.0 (both Feedback rows) — total silence in the right channel, not decorrelation. Any "improved" correlation number in that row is the VOID signature, not a finding.
- L2: `rmsR` at Feedback 0.7 falls from 0.2517 to 0.0734 while `rmsL` stays near 0.22-0.24 — R ends at ~33% of L's level. The lower correlation it shows at w=1.0 (0.0569 vs L0's 0.1088) is largely a level imbalance, not a genuinely decorrelated equal-loudness pair.
- L3: less extreme but still lopsided — at w=1.0, Feedback 0.7, `rmsR`=0.136 vs `rmsL`=0.227 (R ~60% of L). Better balance than L1/L2, but still a real imbalance, and it comes with worse (higher) correlation than L0 through most of the travel.
- L0: stays balanced throughout (`rmsL`/`rmsR` within ~5% of each other at every grid point, both Feedback rows) — as expected, since it never touches feed symmetry.

### Positive control

`corr(l,r)` at `w==0.0` for every law × Feedback row: all eight read **exactly 1.000000** (printed value, not rounded display). This is the named, movable quantity the task specifies; all eight passed, so the rig's identity behavior at w=0 is trustworthy going into the rest of the sweep.

### Q2 — bit-identical to L0 at `w==0.0`, bitwise `==`, 3200 samples

| Law | dfbk=0.0 | dfbk=0.7 |
|---|---|---|
| L0 | PASS | PASS |
| L1 | PASS | PASS |
| L2 | PASS | PASS |
| L3 | PASS | PASS |

All eight bitwise-identical over the full 3200-sample run. **No law is disqualified by Q2.** (Expected mechanistically: at `w=0`, every `ConfigureLaw` branch collapses to `inRFactor=1.0`, `crossProbe=0.0`, so all four laws execute the identical arithmetic.)

### Q3 — per-sample write bound, `dfbk=1.0`(→0.98 clamp), `dsnd=1.0`, `w=1.0`, sustained full-scale input (constant 1.0), 3000 samples, `fbDrive` left at its default 1.0x (unity)

Bound = |inSignal| + fbk = 1.9800.

| Law | worst \|wet.l\| | worst \|wet.r\| | finite | within bound |
|---|---|---|---|---|
| L0 | 0.800002 | 0.800002 | yes | yes |
| L1 | 0.800006 | 0.000000 | yes | yes |
| L2 | 0.800000 | 0.788738 | yes | yes |
| L3 | 0.800002 | 0.794331 | yes | yes |

All four stay far under the 1.98 bound (worst observed ~0.80, consistent with the wet limiter's own ~0.9-1.0 ceiling from the existing test suite), all finite — **no NaN encountered, no unexplained result to stop on. No law is disqualified by Q3.**

### Final verdict

**Disqualified (by Q2 or Q3): none.** All four laws pass the bit-identical-default gate and the per-sample bound gate.

**Does any law actually widen?** No — this is the finding, not a gap in the measurement. None of L1, L2, or L3 delivers a genuinely widened (decorrelated **and** comparably loud) wet pair anywhere on the five-point grid:
- L1 (feed asymmetry, no cross) is inert for correlation at Feedback 0.0 and mostly inert-to-noisy at Feedback 0.7, and its apparent "improvement" at the top of travel is a dead right channel (VOID), not decorrelation — it pans, it does not widen.
- L2 (feed asymmetry + today's cross) shows a real correlation improvement over L0 only at the extreme top of travel (w=1.0, Feedback 0.7), but that same setting carries a 3:1 level imbalance between channels — again panning more than widening.
- L3 (full ping-pong) is mostly **worse** (more correlated) than today's law (L0) through the middle of the travel at Feedback 0.7, and only pulls ahead at w=1.0, still with a real (if smaller) level imbalance.
- L0 (today) stays balanced but, as task 1.1 already established, cross-feed does not decorrelate — this task adds that the story does not change when the read-time widthSpread offset is allowed to move alongside it.

No law is recommended and none was implemented; this is a measurement report only.

## Task 1.1a — the repaired law's grid, and the bounds it sets

Measured with `cross = 0.0f` applied locally and uncommitted in
`dsp::StereoDelay::Process`, against the pinned instrument (band-limited
noise, 50 Hz `OnePoleLowPass` over the seed-20260913 LCG burst, `dtim = 0.3`,
Send 1.0, Feedback 0.7, 12000-sample warmup discarded, 12000-sample measure,
tap point `Process`'s returned `DelayWetPair`). `rmsL` and `rmsR` are nonzero
at every row, so each `|corr|` is a real computation rather than
`Correlation::Value()`'s zero-denominator fallback.

```
width,|corr|,balance
0.00,1.000000,0.000000
0.25,0.502056,0.049631
0.50,0.312225,0.012696
0.75,0.271011,0.015087
1.00,0.240065,0.021091
```

Width 0 is excluded from the maxima below: there `timeL == timeR`, so the two
taps are the same read and the 1.0 is a genuine correlation of an identical
signal. Folding it in would force the correlation bound to at least 1.0, which
no grid point could then exceed.

Across width > 0, `|corr|` falls strictly at every step. The level-balance
ratio does not, and nothing asserts that it should.

The bounds: correlation `0.55`, above a measured maximum of `0.502056` at
width 0.25. Level balance `0.06`, above a measured maximum of `0.049631`, also
at width 0.25. The level-balance margin is the wider of the two in relative
terms because that ratio is the smaller and noisier quantity.

### The positive control

The identical instrument with `cross` restored to `p.dwid * 0.5f * widthBalance`:

```
width,|corr|,balance
0.00,1.000000,0.000000
0.25,0.625328,0.022721
0.50,0.467788,0.001723
0.75,0.391410,0.002539
1.00,0.369539,0.002566
```

Width 0.25 reads `0.625328`, above the `0.55` bound, so the bound
discriminates between the two laws rather than passing both. The full-width
figures reproduce the two values this document already records from the
earlier sessions — `0.240` repaired and `0.370` shipped — which corroborates
the harness against work that did not produce it.
