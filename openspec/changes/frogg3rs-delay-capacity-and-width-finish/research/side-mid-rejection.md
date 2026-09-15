# Side/mid rejection — measurement transcript

## Source: agent-aee27a231268ce89a — side/mid ratio measurements

# Report: Task 1.3 — side/mid energy ratio, three laws, three Feedback rows

**No production change.** Nothing under `app/` was touched (confirmed: `Delay.hpp` mtime predates this session, no newer files under `app/`, `app/build/` untouched). All artifacts are in `/private/tmp/claude-501/-Users-diegoaguilar-canabal-Desktop/9f45ca23-7367-4d5e-b9b9-5371f8f0488e/scratchpad/t13sidemid/`: `measure_harness_t13.cpp`, built binary `measure_harness_t13`, and `measure_output_t13.txt`.

## Harness verification (required before trusting it)

`diff` of `.../scratchpad/t12measure/Delay_probe_t12.hpp` against today's `/Users/diegoaguilar-canabal/Desktop/frogg3rs/app/dsp/Delay.hpp` — the only differences are exactly the two probe fields plus the two lines that read them, nothing else:

```
296a297,302
> // SCRATCH PROBE ... float crossProbe = 0.0f;
304a310
> // SCRATCH PROBE ... float inRFactor = 1.0f;
720c733
< const float cross = p.dwid * 0.5f * widthBalance;
---
> const float cross = crossProbe;
809c822
< WriteSample(inSignal * (1.0f - freezeEff) + fbEff * PadeSaturator::Saturate(fbDrive * fbR), lineR);
---
> WriteSample((inSignal * inRFactor) * (1.0f - freezeEff) + fbEff * PadeSaturator::Saturate(fbDrive * fbR), lineR);
```

Reused verbatim, unmodified, for this task's harness. `widthBalance` in-class default confirmed at `Delay.hpp:296` (`1.0f`), never touched. `dsp::CrossFeedPair` (`StereoField.hpp:39`) confirmed as `{a*(1-cross)+b*cross, b*(1-cross)+a*cross}` — not edited.

Build: `clang++ -std=c++20 -O2 -I.../External/Sheaf/projects/synth/include -I.../app/dsp` (plus `-I t12measure` for the probe header), `nice -n 10`, single-threaded compile — clean, no warnings.

## Mechanism trace that changes what the numbers mean

`cross`/`crossProbe` is read only into `fed = CrossFeedPair(dL, dR, cross)`, whose outputs `fbL/fbR` feed **only** the recirculating write (`fbEff * Saturate(fbDrive*fbL/fbR)`, `Delay.hpp:808-809`). The direct wet output (`dL`/`dR` → Diffusion → wet limiter → `lastWet`) never reads `fed`. So `crossProbe` can only reach the output by first reaching the delay line through feedback. `inRFactor`, by contrast, multiplies `inSignal` directly in the **same** write expression, ungated by `fbEff`. This asymmetry is the answer to Q6 and it is empirically confirmed below (L3 and L4 — which differ only in `crossProbe` — produce numerically identical rows at Feedback 0.0).

## Q3 — bit-identical at w==0 (all PASS)

3200 samples, bitwise `==`, 3 Feedback rows × 3 laws — all 9 combinations PASS, including L4 at every row. This empirically confirms L4's load-bearing claim: even though L4's `crossProbe=1.0` is a full swap (`CrossFeedPair` returns `{dR,dL}`), bit-exact match to L0 (`cross=0`, true identity) over the whole run is only possible if `dL==dR` held at every sample — algebraically this follows by induction (both lines start at 0, `timeL==timeR` since `widthSpread=0` at `w=0`, and at `w=0` `inRFactor=1.0` so both lines are always written the identical value regardless of the swap, since a swap of two equal numbers is a no-op). Verified, not assumed.

## Q6 — Feedback 0.0 structural availability (confirmed by data, not just algebra)

At `dfbk=0.0`: `fbk=0`, `freeze` defaults to 0 → `fbEff = FreezeFeedback(0,0,false) = 0` exactly, so the feedback-write term is multiplied by exactly `0.0f` regardless of `crossProbe`. **Empirical proof**: L3 (`crossProbe=w`) and L4 (`crossProbe=1.0` constant) produce byte-for-byte identical rows across the entire width grid at Feedback 0.0 (see table below) despite wildly different `crossProbe` values — the routing weight is provably inert there. The only thing that moves at Feedback 0.0 is `inRFactor` (present in L3/L4, absent from L0), and it is **not** a width mechanism — see Q4.

**Verdict: nothing engineered as a width mechanism (crossProbe) is reachable at Feedback 0.0, for any law.** L0 has no other lever, so L0 is flat/noise-only at Feedback 0.0. L3/L4's apparent movement at Feedback 0.0 is a forward-path panning artifact, not stereo width.

## Full grid — side/mid, correlation, per-channel RMS (Tap point: `Process`'s returned `DelayWetPair`, post-Diffusion, post-wet-limiter)

`w` grid {0, .25, .5, .75, 1}; ratio = rms(side)/rms(mid); corr = L/R correlation.

**Feedback = 0.00**
| law | w=0 | 0.25 | 0.5 | 0.75 | 1.0 |
|---|---|---|---|---|---|
| L0 ratio | 0 | 0.9832 | 1.0055 | 0.9831 | 1.0041 |
| L0 corr | 1.000 | 0.0168 | -0.0056 | 0.0169 | -0.0042 |
| L0 rmsL/rmsR | .2103/.2103 | .2103/.2072 | .2103/.2083 | .2103/.2134 | .2103/.2226 |
| L3 ratio | 0 | 0.9840 | 1.0044 | 0.9919 | 1.0000 |
| L3 rmsL/rmsR | .2103/.2103 | .2103/.1554 | .2103/.1041 | .2103/.0533 | .2103/**0.0000** |
| L4 ratio | 0 | 0.9840 | 1.0044 | 0.9919 | 1.0000 |
| L4 rmsL/rmsR | .2103/.2103 | .2103/.1554 | .2103/.1041 | .2103/.0533 | .2103/**0.0000** |

L3 and L4 rows are identical at every column (proves Q6). At `w=1.0`, right channel goes to exact silence for both L3 and L4 — `inRFactor=0` and `fbEff=0` means lineR is written `0.0f` forever.

**Feedback = 0.35**
| law | w=0 | 0.25 | 0.5 | 0.75 | 1.0 |
|---|---|---|---|---|---|
| L0 ratio | 0 | 0.9819 | 0.9923 | 0.9711 | 0.9813 |
| L0 rmsL/rmsR | .2197/.2197 | .2176/.2147 | .2163/.2146 | .2154/.2184 | .2157/.2280 |
| L3 ratio | 0 | 0.9783 | 0.9798 | 0.9676 | 0.9915 |
| L3 rmsL/rmsR | .2197/.2197 | .2159/.1603 | .2135/.1102 | .2117/.0715 | .2113/.0631 |
| L4 ratio | 0 | 0.9133 | 0.9387 | 0.9547 | 0.9915 |
| L4 rmsL/rmsR | .2197/.2197 | .2163/.1674 | .2139/.1211 | .2120/.0818 | .2113/.0631 |

**Feedback = 0.70**
| law | w=0 | 0.25 | 0.5 | 0.75 | 1.0 |
|---|---|---|---|---|---|
| L0 ratio | 0 | 0.9659 | 0.9605 | 0.9201 | 0.8965 |
| L0 rmsL/rmsR | .2517/.2517 | .2446/.2437 | .2393/.2385 | .2354/.2384 | .2371/.2490 |
| L3 ratio | 0 | 0.9422 | 0.8961 | 0.8822 | 0.9829 |
| L3 rmsL/rmsR | .2517/.2517 | .2372/.1805 | .2271/.1341 | .2215/.1163 | .2269/.1361 |
| L4 ratio | 0 | 0.7663 | 0.8029 | 0.8765 | 0.9829 |
| L4 rmsL/rmsR | .2517/.2517 | .2448/.2123 | .2368/.1733 | .2299/.1458 | .2269/.1361 |

## Positive control

`rms(side)` at `w==0.0`, every law/Feedback row: **exactly `0.000000000`**, with `rms(mid)` clearly nonzero (0.210–0.252) at every one of the 9 rows — printed and verified, not a VOID signature. Rig is live.

## Answers to the six questions

**Q1 — does side/mid ratio RISE point to point, per law/row?**
- Feedback 0.0: no law rises meaningfully. All three jump near ~0.98–1.01 at the first step off zero and then oscillate in a narrow band with no monotonic trend — the same "saturates immediately, no dynamic range left" signature the prior round found with correlation, now reproduced in the instrument that was supposed to avoid it.
- Feedback 0.35: L0 is flat/oscillating (0.982→0.992→0.971→0.981, no trend). L3 is flat/slightly dipping then a partial recovery (0.978→0.980→0.968→0.992, not monotonic). **L4 rises monotonically**: 0.913→0.939→0.955→0.992.
- Feedback 0.70: **L0 falls monotonically**: 0.966→0.960→0.920→0.896. L3 dips then partially recovers (0.942→0.896→0.882→0.983, not monotonic). **L4 rises monotonically**: 0.766→0.803→0.876→0.983.

L4 is the only law that rises monotonically at both nonzero Feedback rows, and the rise strengthens with Feedback (mild at 0.35, pronounced at 0.70) — consistent with a feedback-path mechanism that "engages gradually," matching the reason three Feedback rows were used.

**Q2 — L0 at Feedback 0.7 as width rises: FALLS.** From 0.9659 (w=0.25) to 0.8965 (w=1.0), a monotonic ~7.2% relative narrowing across the travel. This is the pinned defect number: today's law measurably narrows the stereo image at high Feedback as Stereo width is turned up.

**Q3 — bit-identical at w==0: PASS for all 9 (law × Feedback-row) combinations**, including L4 specifically, empirically confirming the swap-of-equal-values identity claim (see mechanism trace above).

**Q4 — per-channel RMS / panning check.** L0 keeps `rmsL` and `rmsR` close across the whole grid (within ~5% at Feedback 0.7, e.g. 0.2371 vs 0.2490 at w=1.0) — its weak "widening" is not achieved by panning. **L3 and L4 both show real, disqualifying panning**: at Feedback 0.0, `rmsR` decays linearly with `(1-w)` down to **exact silence** at `w=1.0` (0.2103/0.0000) — the "width" there is 100% one-channel muting, not stereo image widening. At Feedback 0.7 the effect is less severe but still substantial: L4's `rmsL/rmsR` ratio grows from 1.15 (w=0.25) to 1.67 (w=1.0) — the right channel stays 40–47% quieter than the left across the width range, even where the side/mid ratio rises cleanly. The rise in side/mid for L3/L4 is confounded with genuine loudness panning at every Feedback row measured, worst at low Feedback.

**Q5 — write bound**: `dfbk=1.0→0.98`, `dsnd=1.0`, `w=1.0`, 3000 samples, bound = 1.98. Worst observed: L0 `|wet.l|=|wet.r|=0.800002`; L3/L4 `|wet.l|=0.800002`, `|wet.r|=0.794331`. All well inside the 1.98 bound, all finite. No law raises the bound — none disqualified by this question.

**Q6 — Feedback 0.0 structural availability: confirmed nothing engineered as width can reach output.** `crossProbe` is provably inert there (L3 and L4 rows identical despite different `crossProbe` values). `inRFactor` (L3/L4 only) does move at Feedback 0.0, but what it produces is one-channel attenuation/panning (culminating in exact right-channel silence at `w=1.0`), not stereo widening. L0 has no lever at all at Feedback 0.0 and shows only widthSpread's own noise-floor oscillation. Honest finding: at Feedback 0.0, nothing tested here widens the image; L3/L4's apparent Feedback-0.0 "movement" is a panning artifact, not width.

## Correlation vs side/mid — does the new instrument do better?

At Feedback 0.0, side/mid reproduces exactly the same failure mode correlation showed previously: an immediate jump near its ceiling (~0.98–1.01) at the first nonzero width step, then noise-band oscillation with no usable dynamic range — side/mid does **not** rescue Feedback 0.0 as a discriminating row. At Feedback 0.35 and especially 0.70, side/mid does show real dynamic range and a genuine monotonic signal for L4 (and the L0 narrowing defect) that correlation's floor obscured — so the instrument swap is a real improvement at the nonzero-feedback rows, but it is not a universal fix: the two instruments agree Feedback 0.0 carries no usable width signal for any of these laws.

## Disqualifications

- **L0**: not disqualified by any hard question (Q3/Q5 pass, Q4 shows no panning), but Q1/Q2 document its defect directly — it narrows (falls) at Feedback 0.7 as width rises, which is the reason this whole investigation exists.
- **L3**: disqualified by **Q4** — full right-channel silence at Feedback 0.0/`w=1.0` (pure panning, not widening), and non-monotonic (dip-then-recover) side/mid behavior at both nonzero Feedback rows per **Q1**, so it does not deliver a clean "rises with width" law either.
- **L4**: passes Q3 (identity claim verified) and Q5 (write bound unaffected); shows the cleanest monotonic Q1 rise at both nonzero Feedback rows and is the specific defect-repair candidate for Q2's narrowing. It is **not clean under Q4**: it shares L3's exact-silence panning artifact at Feedback 0.0/`w=1.0`, and even at Feedback 0.7 carries a persistent ~1.15–1.67× L-over-R loudness imbalance across the width range that any side/mid rise is confounded with.

No law is recommended and none is implemented, per the task's instructions — this is measurement only.
