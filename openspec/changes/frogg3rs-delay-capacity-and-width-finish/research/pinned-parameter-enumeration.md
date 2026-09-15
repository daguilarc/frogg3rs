# Every parameter the Delay checks hold fixed

Three adversarial passes each found the same defect one instance at a time —
Mod depth pinned at 0, Width balance pinned at 1.0, sample rate pinned at
48000 — and each cost a full audit round. This enumeration exists so the class
closes once. Task 1.4a builds its gate from this list.

## What the checks can set

`DelayParams` carries `dtim, dsnd, dfbk, dwid, dfrz, dfrzLatched, dmod, dmix,
drev, ddif`. Object state arrives only through `SetSampleRate`,
`SetFeedbackDrive`, `SetFeedbackTone`, `SetModRate`, `SetWidthBalance` and
`SetCrush`. All fourteen map one-to-one onto the Delay bank's fourteen UI slots,
so every one is a knob an operator can move.

Sample rate is host-reachable, not an app constant: `PrepareToPlay` takes it
from the host and passes it to `SetSampleRate`.

## The capacity surface

The width spread's bound is a function of exactly five of them: sample rate,
`dtim`, `dwid`, `widthBalance`, and `dmod` through the modulation offset.

Across the three capacity checks:

| | sample rate | dtim | dwid | widthBalance | dmod |
|---|---|---|---|---|---|
| never-reads-past-capacity | PINNED 48000 | PINNED 0.99 | PINNED 1.0 | PINNED 1.0 | PINNED 0, unset |
| bound-is-inert-away | PINNED 48000 | PINNED 0.3 | PINNED 1.0 | PINNED 1.0 | PINNED 0, unset |
| bound-holds-across-grid | PINNED 48000 | SWEPT, 401 steps | SWEPT, 401 steps | SWEPT, 401 steps | ABSENT FROM THE FORMULA |

Of 39 sample-rate definitions among the Delay checks, 38 read `48000.0f` and one
reads `20000.0f`. None sweeps it.

The grid check is the widest of the three and never constructs a `StereoDelay`
at all — it re-derives the production arithmetic by hand, and that re-derivation
drops the modulation term the production bound subtracts. So Mod depth is not
merely pinned across the capacity surface; in the one check that sweeps
anything, it is absent from the model.

## The remaining hiding places

1. **Sample rate.** Host-reachable. The capacity clamp engages only above
   48000, so at the pinned value the guard has no surface at all.
2. **Width balance.** Pinned at 1.0 in both checks that drive a real object.
   Swept only in the formula replica, and never jointly with a nonzero
   modulation depth.
3. **Mod depth.** Pinned at its default in every capacity check and absent from
   the grid check's formula.

Also pinned at their defaults in both real-object capacity checks, though none
feeds the read-position arithmetic: Feedback, Freeze and its latch, Reverse
blend, Diffusion, Feedback drive, Feedback tone, Mod rate, Crush.

## One combination nothing has ever computed

No check, real-object or replica, drives Delay time, Stereo width, Width balance
and Mod depth to their reachable maxima together. Each is individually
reachable from the shipping UI.
