# Instrument derivation — measurement transcripts

## Source: agent-a308d9ee02ca50453 — stimulus design and the offset-alone measurement

## Report — Task 1.1

**Deliverable is this report. No file under `app/` was touched** — confirmed by `git diff --stat -- app/` showing no output. (`HANDOFF.md`, `proposal.md`, `tasks.md` show as modified in `git status` but those are pre-existing changes from before this task started; I only read them.)

### Harness verification

Diffed `/private/tmp/.../scratchpad/t12measure/Delay_probe_t12.hpp` against today's `/Users/diegoaguilar-canabal/Desktop/frogg3rs/app/dsp/Delay.hpp`. The only differences are exactly the two probe fields and the two lines reading them, as claimed:

```
+    float crossProbe = 0.0f;   // added field
+    float inRFactor = 1.0f;    // added field
...
-        const float cross = p.dwid * 0.5f * widthBalance;
+        const float cross = crossProbe;
...
-        WriteSample(inSignal * (1.0f - freezeEff) + fbEff * PadeSaturator::Saturate(fbDrive * fbR), lineR);
+        WriteSample((inSignal * inRFactor) * (1.0f - freezeEff) + fbEff * PadeSaturator::Saturate(fbDrive * fbR), lineR);
```

Trusted. Built and reused unmodified via `-I t12measure`.

### Finding 5's arithmetic, verified myself

`baseSeconds = ExpMapCompute(0.001, 2.0, 0.3) = 0.001 * (2000)^0.3 = 0.009779 s` (computed both by hand and printed by the harness). `widthSpread` max (dwid=1, widthBalance=1) = `0.009779 * 0.35 = 0.003423 s = 3.4228 ms = 164.29 samples @ 48 kHz`. Matches Finding 5's cited numbers.

### (a) Stimulus design and graded-vs-saturated proof

**Stimulus:** one causal pass of `dsp::OnePoleLowPass` (`app/dsp/DspMath.hpp`, the primitive `FilterFx.hpp`'s own filters are built from) applied to the same seed-20260913 LCG white-noise burst used everywhere else in this lineage (`lcg = lcg*1664525+1013904223`, amplitude `0.5*(uniform in [-1,1))`). Cutoff **50 Hz** via `SetAlphaFromNatFreq(50/48000)` → `alpha=0.006524`, `b=1-alpha=0.993476`.

**Bandwidth justification:** `OnePoleLowPass`'s recurrence is an AR(1) process; its autocorrelation at lag `k` is `b^k`, so its e-folding width is `-1/ln(b) = 152.8 samples` — the same order as `widthSpread`'s 164.29-sample maximum, per Finding 5's own criterion. Confirmed empirically (independent of the delay class) by measuring the filtered signal's own autocorrelation vs. lag, settle=2000 samples:

```
lag   band-limited   white(control)
0     1.000000       1.000000
20    0.901836       0.006209
41    0.806618       0.008371
62    0.715096      -0.002366
82    0.632504      -0.007683
103   0.554513      -0.002859
123   0.494476       0.006119
144   0.437450      -0.001653
164   0.378462       0.001352
```

**Proof via the actual probe** — read-time offset alone (`crossProbe=0`, `inRFactor=1`, `Feedback=0.0`, `dtim=0.3`), 9-point grid, tap = `Process`'s returned `DelayWetPair` (post-Diffusion, post-wet-limiter):

```
w      offset(smp)  corr(band-lim)  corr(white,ctrl)
0.000    0.00         1.000000        1.000000
0.125   20.54         0.882637       -0.006832
0.250   41.07         0.775363        0.016821
0.375   61.61         0.667589       -0.005698
0.500   82.15         0.571328       -0.005610
0.625  102.68         0.490283       -0.009388
0.750  123.22         0.438770        0.016937
0.875  143.76         0.395813        0.009718
1.000  164.29         0.345059       -0.004234
```

Band-limited row falls strictly monotonically across the entire grid, 1.000 → 0.345. White-noise control collapses to its noise floor (≈±0.01) by the first non-zero step and wanders with no direction thereafter — reproducing exactly the "collapses at step one" symptom every prior session measured under white noise. **The stimulus design works**; the graded row is not the saturated row.

### (b) Full grid — does the offset alone widen monotonically once cross is retired?

Grid: `w ∈ {0.0,0.25,0.5,0.75,1.0}`, Feedback `∈ {0.0,0.35,0.7}`, `dtim=0.3`, `dsnd=1.0` (both moved off registered `0.0f` defaults), `inRFactor=1.0` fixed both laws, band-limited stimulus from (a). Tap point stated beside every figure: `Process`'s returned `DelayWetPair`, post-Diffusion, post-wet-limiter.

```
Feedback=0.00, law=cross-retired (crossProbe=0.0)
w      corr        rmsL        rmsR        balance
0.000  1.000000    0.016875    0.016875    0.000000
0.250  0.775363    0.016875    0.016902    0.000804
0.500  0.571328    0.016875    0.016942    0.001968
0.750  0.438770    0.016875    0.017033    0.004641
1.000  0.345059    0.016875    0.017009    0.003961

Feedback=0.00, law=today (crossProbe=0.5*w)
w      corr        rmsL        rmsR        balance
0.000  1.000000    0.016875    0.016875    0.000000
0.250  0.775363    0.016875    0.016902    0.000804
0.500  0.571328    0.016875    0.016942    0.001968
0.750  0.438770    0.016875    0.017033    0.004641
1.000  0.345059    0.016875    0.017009    0.003961
(bit-identical to retired at Feedback 0 — confirms Finding 2: fbEff=0 nulls the cross-feed's write regardless of crossProbe's value)

Feedback=0.35, law=cross-retired (crossProbe=0.0)
w      corr        rmsL        rmsR        balance
0.000  1.000000    0.017494    0.017494    0.000000
0.250  0.732214    0.017494    0.017628    0.003822
0.500  0.517134    0.017494    0.017531    0.001044
0.750  0.386137    0.017494    0.017505    0.000307
1.000  0.304592    0.017494    0.017615    0.003439

Feedback=0.35, law=today (crossProbe=0.5*w)
w      corr        rmsL        rmsR        balance
0.000  1.000000    0.017494    0.017494    0.000000
0.250  0.746584    0.017456    0.017537    0.002314
0.500  0.547649    0.017320    0.017382    0.001766
0.750  0.420471    0.017132    0.017297    0.004785
1.000  0.342938    0.017074    0.017277    0.005888

Feedback=0.70, law=cross-retired (crossProbe=0.0)
w      corr        rmsL        rmsR        balance
0.000  1.000000    0.020457    0.020457    0.000000
0.250  0.502056    0.020457    0.022594    0.049631
0.500  0.312225    0.020457    0.020983    0.012696
0.750  0.271011    0.020457    0.021084    0.015087
1.000  0.240065    0.020457    0.021338    0.021091

Feedback=0.70, law=today (crossProbe=0.5*w)
w      corr        rmsL        rmsR        balance
0.000  1.000000    0.020457    0.020457    0.000000
0.250  0.625328    0.020411    0.021360    0.022721
0.500  0.467788    0.019603    0.019671    0.001723
0.750  0.391410    0.018750    0.018846    0.002539
1.000  0.369539    0.018682    0.018778    0.002566
```

All figures finite; no `nan` encountered anywhere.

**Positive control** (`w==0`, must read exactly `1.000000`):
```
law=retired dfbk=0.00 w=0.000 corr=1.000000 (control OK)
law=today   dfbk=0.00 w=0.000 corr=1.000000 (control OK)
law=retired dfbk=0.35 w=0.000 corr=1.000000 (control OK)
law=today   dfbk=0.35 w=0.000 corr=1.000000 (control OK)
law=retired dfbk=0.70 w=0.000 corr=1.000000 (control OK)
law=today   dfbk=0.70 w=0.000 corr=1.000000 (control OK)
```
All six read exactly `1.000000`. Rig is not void — every row's `rmsL`/`rmsR` are nonzero and none is a flat `+1` across a whole row.

**Level balance:** stays under 5% at every point (worst case 0.0496 at Feedback 0.7, `w=0.25`, retired law), well short of Finding 3's muting/panning signature (`rmsR=0` or 1.15–1.67x imbalance). No law reads as panning-disguised-as-width.

### Plain one-line answer to (b)

**YES** — with `crossProbe` retired to `0.0` and `inRFactor` held at `1.0`, the read-time offset alone produces a wet-pair correlation that falls strictly monotonically point-to-point across Stereo width's whole travel at every measured Feedback row (0.0, 0.35, 0.7), under the band-limited stimulus that (a) proved has dynamic range across that travel; the level-balance figure stays under 5% throughout, so this is not panning in disguise.

One nuance worth carrying into 1.2, reported plainly rather than softened: today's shipped law (`crossProbe=0.5*w`) is *also* monotonically decreasing at all three Feedback rows measured here, because `widthSpread` grows alongside `crossProbe` in production rather than being held fixed as Finding 1's earlier round tested it — but at every nonzero grid point the retired-cross law reaches a lower (more decorrelated / wider) value than today's law, e.g. at Feedback 0.7, `w=1.0`: retired `0.240` vs. today `0.370`. Retiring the cross-feed does not just avoid breaking monotonicity — it strictly out-widens today's law at every point measured, with no level-balance cost.

---

## Source: agent-ad785af990d2d388b — adversarial pass on the stimulus claim

## Report: Adversarial falsification attempt on the StereoDelay width/correlation claim

**Setup.** Built a fresh, independent harness (`/private/tmp/.../scratchpad/adv-a/measure.cpp`) that includes the **real, unmodified** `app/dsp/Delay.hpp` (read-only; nothing under `app/` touched), compiled with the exact command given (`clang++ -std=c++20 -O2 -I.../External/Sheaf/projects/synth/include -I.../app/dsp`, `nice -n 10`). I did not reuse the pre-existing `t12measure` harness's probe copy — that copy exists to decouple cross-feed/feed-asymmetry (a different, unrelated task's question), and the claim under test here only needs stock `Process()` at `dfbk=0` (its DelayParams default), so testing against the real header is the more direct adversarial route. First step was reproducing the reported baseline exactly (1.000000 → 0.775 → 0.571 → 0.439 → 0.345 at 50Hz/seed 20260913/dtim=0.3) to validate the rig — it matched digit-for-digit, so the harness is trustworthy.

**Mechanism traced first** (`Delay.hpp:683-684`, `:720`, `:808-809`): at `dfbk=0`, `fbEff=0`, so both lines are written the *identical* `inSignal` every sample — cross-feed (`Delay.hpp:720`) has zero effect on the wet output in this regime. The wet pair reduces exactly to two reads of the *same* stimulus at two fixed lags (`timeL`, `timeR`), so `corr(wet.l, wet.r)` **is** the stimulus's autocorrelation function evaluated at `lag = widthSpread(w)`. For a one-pole IIR-filtered white noise input this is the textbook AR(1) result `rho^lag`, `rho = exp(-2π·fc/fs)` — confirmed numerically (theory predicts ≈0.342 at w=1, measured 0.345). So the claim's core mechanism is real, not folklore.

**Angles tried, and outcome of each:**

1. **Seed sensitivity** (8 seeds, matched cutoff/dtim/grid): shape (monotonic 1.0 → ~0.23–0.43) held for every seed. **Survived.**
2. **Fine grid** (41 points instead of 5): strictly monotonic decreasing at every point, no hidden non-monotonic wiggle the coarse grid could have concealed. **Survived.**
3. **Cutoff sweep** (2 Hz–5000 Hz at fixed dtim=0.3): confirms a genuine "sweet spot" — roughly 20–150 Hz gives graded, monotonic dynamic range; below it (2–10 Hz) correlation stays compressed near 1 (0.81–0.96, no real range); above ~250 Hz it collapses to the white-noise floor by the first grid point. This is exactly the shape the "matching" theory predicts. **Survived** (bound: ~20–150 Hz at this dtim/grid, i.e. not unique to 50 Hz but not arbitrary either).
4. **Sample rate** (44.1/48/88.2/96 kHz, cutoff/dtim held in Hz/seconds): same qualitative curve at all four. **Survived.**
5. **Delay-time (dtim) sweep at the fixed 50 Hz cutoff**: short dtim (0.02–0.1) never leaves the near-1 compressed regime (0.87–0.97) — no genuine dynamic range without retuning. Long dtim (0.5, 0.7) collapses to the floor almost immediately. **dtim=0.9 and 0.99 produced a flat `corr=1.000000` across the *entire* width row with `rmsL=rmsR=0.000000`** — the exact "unfed/dead line" signature the brief warned about: `Correlation::Value()`'s void fallback (den==0 → hardcoded 1.0), not a real measurement. Root cause: `timeL` at dtim=0.9/0.99 (≈45k–89k samples) exceeds the 24,000 total samples run, so the read lands in never-written buffer.
6. **Re-matching the cutoff correctly for other delay times** (holding `fc·baseSeconds` constant, the literal "matching" principle, plus enlarging warmup so the line is provably filled — this is the fair test of the claim's generalization): dtim=0.05/0.10/0.30 reproduce the same smooth monotonic shape fine. But **dtim=0.6 is already only marginally clean, and dtim=0.8/0.95 are non-monotonic and go negative** (e.g. dtim=0.95: 1.0, 0.029, 0.150, −0.040, −0.266) even with warmup enlarged well past the dead-line threshold. Root cause: the "matched" correlation time scales with delay time too, so the fixed 12,000-sample measurement window (adequate at dtim=0.3, giving ~78 independent correlation-lengths) shrinks to under one correlation-length at dtim≥0.8, so the number reported there is measurement noise, not signal.
7. **Measurement-length sensitivity at the baseline (matched) settings**: cutting `measure` from 12000 to 1000 already breaks monotonicity (1.0, 0.615, 0.320, 0.359, 0.469 — rises after w=0.5) at otherwise-unchanged, non-extreme settings. So the reported smoothness is partly a function of the specific sample count chosen, not solely of the cutoff/lag matching.
8. **Feedback>0** (outside the claim's literal scope but reachable on the same control page): dfbk=0.9 and 0.98 produce non-monotonic curves (e.g. dfbk=0.98: 1.0, 0.248, 0.265, 0.275, 0.583 — rises at w=1). Flagged as a boundary the claim doesn't cover, not counted in the verdict.
9. **Warmup sensitivity at baseline settings**: 0 through 48,000 samples all agree to ~2 decimal places — warmup itself isn't fragile at the reported settings. **Survived** at dtim=0.3.

**Secondary finding (not part of the claim, flagged per "say something"):** at dtim≈0.99, `rmsR` is nonzero at w=0.25/0.5/0.75 but zero at w=0 and w=1.0 — traced to `baseSeconds+widthSpread` exceeding the 2.0 s/96000-sample line capacity and wrapping modulo-capacity to an aliased (wrong) shorter lag rather than being clamped. This looks like a real edge-case gap in `StereoDelay`'s capacity handling for `dtim` near its top of travel, independent of this claim; I did not chase it further or touch any file under `app/`.

**Verdict:** the claim's mechanism (autocorrelation-matching) is real and survives seed, grid, cutoff-choice, and sample-rate attacks with the bounds above. But the "not an artifact of ... delay time" clause is **CLAIM FALSIFIED**: at the same cutoff/seed/protocol, `dtim=0.9` (or `0.99`) yields a flat `corr(l,r)=1.000000` across the whole Stereo-width grid with `rmsL=rmsR=0.000000` — a void/dead-line artifact reported as if it were "no dynamic range," not a real correlation. And even when the cutoff is re-derived correctly to "match" a different delay time (the claim's own stated mechanism, applied properly), the fixed 12,000-sample measurement window used in the report does not generalize — it becomes noise-dominated and non-monotonic at dtim ≥ 0.8, meaning the reported measurement's sample count is itself an unstated, delay-time-dependent precondition the claim omits.
