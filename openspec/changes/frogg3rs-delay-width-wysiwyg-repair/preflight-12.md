# Preflight 12 — terminating executability check, second run

## YES

Tasks 1.1a and 1.1b can be executed exactly as written, without inventing a value, threshold, grid point, file, or decision.

## Simulated run — `dsp::StereoDelay::Process(bumpIn, p)` parameter by parameter

Reading `app/dsp/Delay.hpp:653-863` (`Process`'s full body):

| Parameter | Value set | Source |
|---|---|---|
| `bumpIn` | band-limited noise sample (50 Hz `OnePoleLowPass` over LCG burst, seed 20260913, amplitude `0.5f*(uniform[-1,1))`) | `proposal.md` "The instrument" — recurrence/seed/scaling match the existing `reverb_damping_filter_split_lowers_wet_leg_correlation_at_every_setting` idiom verbatim (`app/FroggersDspParityTests.cpp` ~9505, LCG `>>8 / 8388608.0f - 1.0f`); struct confirmed at `app/dsp/DspMath.hpp:86-106` |
| `p.dtim` | `0.3` | task 1.1a literal |
| `p.dsnd` (Send) | `1.0` | `proposal.md` "The check" — "Send... is pinned at 1.0 for every measurement in this document," with the `Delay.hpp:655` early-return degeneracy at default 0.0 named as the reason. **This is the repair to the prior audit's "Send unnamed" finding — verified present.** |
| `p.dfbk` (Feedback) | `0.7` | task 1.1a literal; `MapRowsToDelayParams` passes `feedbackKnob01` straight into `params.dfbk` with no curve (`Delay.hpp:1106` area), so "Feedback 0.7" is unambiguous |
| `p.dwid` (Stereo width) | grid `{0.0, 0.25, 0.5, 0.75, 1.0}` | task 1.1a literal, matching the spec's own prior five-point grid |
| `p.dfrz`, `p.dmod`, `p.drev`, `p.ddif` | `0.0` each | `app/FroggersParameters.hpp:262-283` — Freeze, Mod depth, Reverse blend, Diffusion rows all omit the third (`defaultValue`) field, defaulting to `0.0f` (struct default at `FroggersParameters.hpp:100`); registered default, so "needs no naming" per the check's own stated convention |
| `p.dfrzLatched` | `false` | `DelayParams`'s own field default (`Delay.hpp:151-154`, explicitly "NOT part of the row mapping") |
| `p.dmix` | irrelevant | not read anywhere inside `Process` — only `ToStereo` reads it, and the tap point is `Process`'s own return, pre-`ToStereo` |
| `widthBalance` | `1.0` via `SetWidthBalance(1.0f)` | `FroggersParameters.hpp:279` ("Width balance","WBal",1.0f) — registered default, no naming required |
| `fbDrive` | `0.5` via `SetFeedbackDrive(0.5f)` | `FroggersParameters.hpp:271` ("Feedback drive","FbDr",0.5f) |
| `fbToneL/R` | `1.0` via `SetFeedbackTone(1.0f)` | `FroggersParameters.hpp:277` ("Feedback tone","FbTn",1.0f) |
| `crushL/R` | `0.0` via `SetCrush(0.0f)` | `FroggersParameters.hpp:280` ("Crush","Crsh",0.0f) |
| `sampleRate` | `48000.0f` via `SetSampleRate` | the one convention every `StereoDelay` test in this file already uses (e.g. `app/FroggersDspParityTests.cpp:9219`) |

Every value traces to either the task text itself, `FroggersParameters.hpp`'s already-written registered-default table, or established precedent code already in the test file — none is invented.

**Liveness at every grid point:** at `dwid ∈ {0.25,0.5,0.75,1.0}`, `widthSpread = p.dwid*baseSeconds*0.35f*widthBalance > 0`, so `timeR ≠ timeL`, `ReadAt` reads two distinct history positions, `dL ≠ dR` almost surely under noise, and `rmsL`/`rmsR` are nonzero — a real `Correlation::Value()` computation, not the `den<=0 → 1.0` fallback at `FroggersDspParityTests.cpp:9500`. At `dwid = 0.0`, `timeL == timeR` so `dL == dR` and the fallback's `1.0` is a **genuine** correlation of an identical signal, not a dead-line artifact — exactly why 1.1a excludes width 0 from the max-taking. With Send pinned at 1.0, the `Delay.hpp:655` degenerate early-return never fires. **The instrument is live at every grid point the task sweeps.**

## Absence-diff against `git show HEAD:.../proposal.md` and `.../tasks.md`

HEAD is not a simple truncation of the current text — both files were substantially rewritten (net larger: 415→742 lines), so this is not literally "335→267," but per the brief HEAD is the earlier state and I diffed content, not line counts. One genuine, unrestored operational drop:

**Finding: the NaN/non-finite guard-gap warning was deleted and never restored.**
HEAD's `tasks.md` (deleted lines ~86-90) stated:
> "A flat +1 is the VOID signature. A `nan` is NOT the same finding and is not covered by `Correlation::Value()`'s guard, which catches a zero denominator only; a non-finite sample gives an infinite denominator, `Inf > 0.0` passes, and the division yields `nan` anyway. A task meeting a `nan` stops and reports it as unexplained."

I grepped current `proposal.md` and `tasks.md` for `nan|non-finite|infinite|isfinite` — zero hits. This warning is gone with nothing replacing it. It matters because a liveness gate written as `rmsL != 0.0f` (the natural reading of "liveness-gated on that row's `rmsL`/`rmsR` nonzero") is **true** for a NaN value under IEEE-754 (`NaN != 0.0` is `true`), so a broken/non-finite instrument could read as "live" and pass exactly the gate meant to catch a dead one. **This does not block 1.1a/1.1b**: under their specified bounded operating point (Send=1.0, `dfbk=0.7≤0.98` clamp, `PadeSaturator::Saturate` clamping every write to `±1`, per-channel `OutputLimiter`s on the wet tap), no NaN/Inf pathway is reachable — I traced every write in `Process` and found no unbounded operation. It **does block/weaken task A.3** (the adversarial pass against the landed check) and the check's own robustness the way `Correlation::Value()`'s zero-denominator note is supposed to: this class of evasion (drive an intermediate to Inf/NaN so a `!= 0` liveness check spuriously passes) is exactly what an adversarial reviewer should be told to try, and nothing in the current documents tells them to.

**Secondary observation (not a blocker for anything named, reported for completeness):** HEAD's deleted defect table (`git show HEAD:.../proposal.md`, "The defect, measured") records today's shipped law at Feedback 0.7, width 1.00 as `|corr| = 0.119325`. Current `proposal.md`'s "The check" section states "today's shipped law measures `0.370`" at the same nominal setting (Feedback 0.7, full width). Both claim Send opened to 1.0 and the same instrument. I did not reconcile which is right — neither figure is consumed by 1.1a/1.1b (1.1a computes its own bound fresh; the `0.370`/`0.240` pair is cited prose only, sourced to `research/INDEX.md`, which task 3.6 has not yet populated). This bears on task 3.6 (populating `research/INDEX.md` accurately) — a later stage — not on 1.1a/1.1b.

## 1.1a AND 1.1b ARE EXECUTABLE

No blockers to 1.1a or 1.1b themselves.

1. NaN/non-finite guard-gap (absence, restored nowhere) — blocks/weakens **task A.3** (adversarial pass), not 1.1a/1.1b. Recommend A.3's adversarial dispatch be told explicitly to try driving an intermediate non-finite and check whether the liveness gate as landed catches it.
2. Defect-table figure (`0.119325`) vs. current prose figure (`0.370`) for the same nominal setting — bears on **task 3.6** (research/INDEX.md accuracy), not on 1.1a/1.1b, since neither number is used as a check input.
