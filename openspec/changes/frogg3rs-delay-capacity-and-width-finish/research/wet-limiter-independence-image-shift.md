# Task 1.6 — does independent per-channel wet limiting on the decorrelated pair produce an image shift?

## The threshold, read from source

`app/dsp/Delay.hpp`:

```
inline constexpr float kDelayWetLimiterThreshold = 0.72f;
inline constexpr float kDelayWetLimiterCeiling = kStageCeiling;   // 0.80f, dsp/Limiter.hpp
```

`OutputLimiter::envelope` (`app/dsp/Limiter.hpp`) is a public field: the per-sample gain
multiplier, `1.0f` meaning no reduction. `StereoDelay::wetLimiterL`/`wetLimiterR` are public
members, so both the threshold and the live gain-reduction state are directly readable from a
test without touching production code.

## Step 1 — RMS the shipped decorrelation check's own instrument actually produces

The check is `stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width`
(`app/FroggersDspParityTests.cpp`). Its instrument, unchanged: band-limited noise (LCG seed
`20260913`, amplitude `0.5f*(uniform in [-1,1))`, through a 50 Hz one-pole lowpass), pinned at
`dtim=0.3, dsnd=1.0, dfbk=0.7, dfrz=0, dmod=0, dmix=1.0, drev=0, ddif=0`, 12000-sample warmup
discarded, 12000-sample RMS/correlation measured, tap = `Process`'s returned wet pair.

A temporary probe (added to `app/FroggersDspParityTests.cpp`, built, run, then the file was
restored byte-for-byte — see Restoration below) printed the RMS this exact instrument already
computes internally but never reports, at the check's own four grid widths:

```
TEMP-PROBE-1.6 width=0.25 rmsL=0.0204569181 rmsR=0.0225935735
TEMP-PROBE-1.6 width=0.5  rmsL=0.0204569181 rmsR=0.0209830341
TEMP-PROBE-1.6 width=0.75 rmsL=0.0204569181 rmsR=0.0210836385
TEMP-PROBE-1.6 width=1    rmsL=0.0204569181 rmsR=0.0213384352
```

Positive control for this step: the same run's `|corr|`/`balance` output reproduced the
proposal's own cited precedent figures exactly (`0.502056221` / `0.0496313829` at width 0.25,
against the proposal's `0.502056` / `0.049631`), proving the build and harness were live and
not printing stale or hardcoded numbers.

**Verdict of step 1:** RMS ~0.02–0.023, roughly 32–35x *below* the 0.72 threshold. The check's
own operating point cannot exercise the wet limiter at all, let alone asymmetrically — by
construction, as the task anticipated. A steady-state RMS this far under threshold means the
limiters sit at `envelope == 1.0` (no reduction) for the entire measurement in that check; this
was independently confirmed in step 2 below (every `amp=1` noise row shows `minEnvL=minEnvR=1`
exactly).

## Step 2 — this task's own operating point and statistic

Per the task, a 12000-sample steady-state RMS aggregate averages away exactly the kind of
event in question (one limiter engaging, briefly, while the other does not), so a new statistic
was used: **per-sample paired envelope divergence**, `|wetLimiterL.envelope − wetLimiterR.envelope|`,
read at the same sample index `i` from both limiters, maximized over the measurement window.
This is a transient, per-channel-paired quantity by construction — it is exactly zero unless the
two channels' gain reduction disagrees at some instant.

A temporary sweep (same probe, same file, restored after) searched for an operating point that
actually engages a limiter, using the same instrument shape (LCG noise, 50 Hz lowpass, 12000
warmup + 12000 measure) but varying `dtim`, `dfbk`, `dwid`, and input amplitude (`amp`, a
multiplier on the noise, `1.0` = the same nominal full-scale amplitude convention this file's
own tuning-harness comment uses for "sustained full-scale input"):

```
dtim=0.3  dfbk=0.7 dwid=1 amp=0.5  -> minEnvL=1        minEnvR=1        (never engages — this is the shipped check's own point)
dtim=0.3  dfbk=1   dwid=1 amp=1    -> minEnvL=1        minEnvR=1
dtim=0.1  dfbk=1   dwid=1 amp=1    -> minEnvL=1        minEnvR=1
dtim=0    dfbk=1   dwid=1 amp=1    -> minEnvL=1        minEnvR=1        (band-limited noise; NOT the DC case below)
dtim=0    dfbk=1   dwid=1 amp=1.25 -> minEnvL=1        minEnvR=1
dtim=0    dfbk=1   dwid=1 amp=1.5  -> minEnvL=1        minEnvR=1
dtim=0    dfbk=1   dwid=1 amp=2    -> minEnvL=0.938590 minEnvR=0.960578  maxEnvDivergence=0.0558550954
dtim=0.1  dfbk=1   dwid=1 amp=3    -> minEnvL=0.905255 minEnvR=0.958767  maxEnvDivergence=0.0726916790
dtim=0.1  dfbk=1   dwid=0.5 amp=3  -> minEnvL=0.905255 minEnvR=0.962013  maxEnvDivergence=0.0749757886
dtim=0.1  dfbk=1   dwid=0.25 amp=3 -> minEnvL=0.905255 minEnvR=0.969454  maxEnvDivergence=0.0644116402
dtim=0.3  dfbk=1   dwid=1 amp=3    -> minEnvL=1        minEnvR=0.951987  maxEnvDivergence=0.0480134487
```

`dfbk` is the *knob*; `Feedback` internally clamps toward `~0.98` at the knob's top end (this
file's own header comment on the wet-limiter tuning derivation) — consistent with these being
the hottest reachable feedback settings, not an invented internal value.

At the shipped instrument's own amplitude convention (`amp=1`, i.e. nominal full scale into the
lowpass, same as the decorrelation check), **the wet limiter never engages at all** — `minEnv`
reads exactly `1.0` at every `dtim`/`dwid` tried, including `dtim=0` (this file's own comment
identifies `dtim=0` as the worst-case fastest round trip for this exact limiter). Engagement
only began once the input was driven to roughly 2x that nominal amplitude.

### Isolating the mechanism: width is the variable that produces the divergence, not just "hot signal"

A direct control at the engaging operating point, width forced to zero:

```
dtim=0 dfbk=1 dwid=0 amp=2 -> minEnvL=0.938589692 minEnvR=0.938589692 maxEnvDivergence=0 (exact)
dtim=0 dfbk=1 dwid=1 amp=2 -> minEnvL=0.938589692 minEnvR=0.960578382 maxEnvDivergence=0.0558550954
```

Both rows drive the *same* hot signal through the *same* feedback loop at the *same* engagement
depth on the L channel (`minEnvL` is bit-identical, `0.938589692`, in both rows — the amplitude
and feedback alone decide how hard the limiter is pushed). The divergence is exactly zero when
`dwid=0` and non-zero the instant `dwid>0` reintroduces the read-time offset between channels.
This is the positive control the omni rule requires for a "does not happen" claim inverted into
a "does happen, and here is what would have shown it not happening": had this been an artifact
of the stimulus or the limiter alone (not of the decorrelation this change introduces),
`dwid=0` would have shown the same non-zero divergence. It does not — divergence tracks `dwid`
exactly as the killed reason predicted it would once cross-feed stopped holding the channels
together.

At the peak-divergence sample for `dtim=0, dwid=1, amp=2`:

```
envL=0.942760885 envR=0.998615980  wetL=0.778742075 wetR=0.681546986
dB swing (envL/envR) = 20*log10(0.942760885/0.998615980) = -0.499939263 dB
```

and for `dtim=0.1, dwid=1, amp=3`:

```
envL=0.905256391 envR=0.977948070  wetL=0.778569162 wetR=0.571126103
dB swing = -0.670883775 dB
```

So the measured effect, where it occurs, is a momentary (single-sample-resolution, tracked
across a 12000-sample window) interchannel gain difference of roughly 0.5–0.7 dB, attributable
specifically to one limiter's envelope sitting deeper in gain reduction than the other's at the
same instant — the per-channel independence the file's surviving reason (avoiding whole-stage
ducking) is explicitly trading for.

## Verdict

**Measured, and it does — but only once the wet path is driven well past the amplitude this
change's own decorrelation check ever exercises, and past what this task traced upstream of
Delay.** Under the exact instrument family the shipped check uses (LCG noise, 50 Hz lowpass,
12000+12000 warmup/measure, feedback pinned at its knob maximum), independent per-channel
limiting on the decorrelated (`dwid>0`) pair produces a real, non-zero, per-sample-paired
envelope divergence — confirmed absent (exactly `0.0`) under the identical stimulus with
`dwid=0`, isolating decorrelation as the cause — translating to on the order of 0.5–0.7 dB of
momentary interchannel level difference. It does not occur at the nominal full-scale amplitude
convention this same probe and the shipped check both use (`amp=1`): the limiter's `envelope`
stays pinned at exactly `1.0` (no reduction at all) at every `dtim`/`dwid` combination tried at
that amplitude, matching the RMS finding in Step 1 that the shipped check's operating point
never comes close to threshold.

Whether ordinary use ever drives Delay's input (`filterOut` in `FroggersAppCore.hpp`) to ~2–3x
that nominal amplitude was not traced in this task — it is out of scope for a task that must not
touch `dsp::CrossFeedPair`, `Reverb.hpp`, or `StereoField.hpp` and was scoped to the wet-limiter
question alone. `app/dsp/Delay.hpp`'s own header comment on the wet-limiter tuning derivation
already anticipates hot content reaching this stage — sustained full-scale input alone drives the
raw (pre-limiter) wet tap to "its ~1.96 per-sample bound" — and the Filter bank's Comb drive
(`app/FroggersAppCore.hpp`, `RouteFilterBank`) alone provides up to 4x pre-gain ahead of Delay,
so amplitudes above nominal full scale reaching this stage are plausible, not invented; this task
did not measure how far above nominal `filterOut` actually reaches under a full worst-case knob
combination, and that is a gap a reader should not treat as closed.

**Recommendation, not landed here:** if a permanent check is wanted, it needs its own pinned
operating point separate from the decorrelation check's (something in the neighborhood of the
`dtim=0, dfbk=1.0 (knob), dwid>0, amp≈2x nominal` region measured above) and a transient,
per-sample-paired statistic like `maxEnvDivergence` above — a steady-state RMS/correlation
aggregate, by this task's own step 1, cannot see this risk at all. This is a description of a
possible follow-on check, not a task this proposal authorizes landing.

## Positive controls, summarized

1. Step 1's `|corr|`/`balance` output reproduced the proposal's own precedent figures
   (`0.502056`/`0.049631`) exactly, proving the harness was live, not stale.
2. `dwid=0` at the same hot amplitude/feedback that produces divergence at `dwid>0` produces
   `maxEnvDivergence == 0.0` exactly and bit-identical `envL`/`envR`, isolating decorrelation
   (not the limiter or the stimulus alone) as the cause.
3. Sub-threshold amplitudes (`amp` in `{1, 1.25, 1.5}`) at the same `dtim=0, dwid=1, dfbk=1`
   settings that engage at `amp=2` show `minEnvL=minEnvR=1` exactly — the statistic is capable
   of reading zero, so its non-zero readings above are not a saturated or always-on instrument.

## Restoration

All measurement used a temporary probe added to `app/FroggersDspParityTests.cpp` (a new
throwaway `TEST_CASE`, plus one added `std::cout` line inside the existing decorrelation test to
expose its already-computed `rmsL`/`rmsR`). No other file was edited. The file was backed up
before editing and restored from that backup afterward:

```
before:   MD5 (app/FroggersDspParityTests.cpp) = 523a39fc8f0ae58c0af6d2e5e85136cf
after:    MD5 (app/FroggersDspParityTests.cpp) = 523a39fc8f0ae58c0af6d2e5e85136cf
```

`app/dsp/Delay.hpp`, `app/dsp/Limiter.hpp`, `dsp::CrossFeedPair`, `app/dsp/Reverb.hpp`, and
`app/dsp/StereoField.hpp` were read only, never edited. `app/build/froggers_dsp_parity_tests`
was rebuilt against the restored source after this task's probes, confirming the binary and
source are back in sync with the pre-task tree (190/190 cases in that binary passed against the
restored file).
