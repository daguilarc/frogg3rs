This file is assembled from material reported across several passes of this
change's own preflight and measurement sessions, not produced by a single
dedicated measurement — see attribution on each excerpt below. Figures are
quoted verbatim and are not reconciled; where two passes give different
numbers for the same quantity, both are kept and the passage says so.

## Source: agent-ad785af990d2d388b (secondary finding, adversarial pass on the stimulus claim), as placed in `research/instrument-derivation.md`

**Secondary finding (not part of the claim, flagged per "say something"):** at dtim≈0.99, `rmsR` is nonzero at w=0.25/0.5/0.75 but zero at w=0 and w=1.0 — traced to `baseSeconds+widthSpread` exceeding the 2.0 s/96000-sample line capacity and wrapping modulo-capacity to an aliased (wrong) shorter lag rather than being clamped. This looks like a real edge-case gap in `StereoDelay`'s capacity handling for `dtim` near its top of travel, independent of this claim; I did not chase it further or touch any file under `app/`.

---

## Source: `proposal.md`, "A second defect in scope: `ReadAt` has no capacity clamp"

`dsp::StereoDelay::ReadAt` (`app/dsp/Delay.hpp:949`) computes
`floorPos % capacityI` with no check that the requested delay fits inside
`capacity` (`kMaxDelaySamples = 96000`, 2.0 s at 48 kHz, `Delay.hpp:250`). At
`dtim = 0.99`, `baseSeconds ≈ 1.8536 s` (`ExpMapCompute(0.001, 2.0, 0.99)`,
`Delay.hpp:669`), and `widthSpread` at full width and default Width balance
adds `≈0.6488 s` (`Delay.hpp:679`), so `timeR ≈ 2.5024 s` — past the 2.0 s
buffer. The modulo wraps that request to a short lag instead of clamping or
refusing it: the right channel reads roughly 0.5 s of delay when the knob
asks for about 2.5 s.

This is reachable from the shipping UI (`dtim` and `dwid` both travel to
1.0), it is in the file this change already edits, and after the repair
`widthSpread` is the only mechanism Stereo width drives — shipping a repair
that leaves the sole remaining mechanism broken over part of its travel is
worse than not repairing the cross-feed at all. Task 1.3 measures the fix
options (clamping the read against capacity vs. bounding `widthSpread` so
`timeR` always fits) and lands whichever the measurement supports; if they
differ audibly, spec conformance breaks the tie toward bounding
`widthSpread`, and task 1.3 states why. `research/INDEX.md` →
read-at-capacity.

---

## Source: `preflight-6.md` — auditor three, §9 checklist axis (N1 finding)

**N1 — Minor arithmetic imprecision in the `ReadAt` overflow illustration.** Recomputing `baseSeconds = ExpMapCompute(0.001, 2.0, 0.99)` and `widthSpread` at full width/default balance gives `timeR ≈ 2.5024 s`; the proposal states `≈2.506 s` (≈4 ms off, ~0.16%). Doesn't change the qualitative conclusion (timeR exceeds the 2.0 s capacity), and task 1.3 re-measures exactly rather than relying on this figure. **Nothing needed** — no dependent decision rests on the exact value.

---

## Source: `preflight-7.md` — auditor four, executability axis (N1 finding)

### N1 — timeR arithmetic off by ~0.16% — **CONFIRM the number, agree NOT-MATERIAL**

Checked independently: read `ExpMapCompute`'s exact source (`app/dsp/DspMath.hpp:52`, `min * pow(max/min, value)`) and `widthSpread`'s formula (`app/dsp/Delay.hpp:679`), recomputed `baseSeconds = ExpMapCompute(0.001, 2.0, 0.99) ≈ 1.853616 s`, `widthSpread ≈ 0.648765 s` at `dwid=1.0`/`widthBalance=1.0`, `timeR ≈ 2.502381 s` — matches their recomputation, versus the proposal's stated `≈2.506 s` (~0.15-0.16% relative difference). Checked whether this literal figure feeds any assertion or check anywhere in `tasks.md`: it does not — task 1.3 is instructed to independently MEASURE both fix options rather than build on this illustrative number, and no `Check:` embeds `2.506`. The qualitative conclusion driving task 1.3 (timeR overruns the 2.0 s capacity by roughly a quarter-second either way) is unaffected. NOT-MATERIAL confirmed.

---

## Source: `preflight-8.md` — auditor five, spec-versus-code axis (Finding 2)

## Finding 2 — Width-spread capacity overrun (the cited worked example, confirmed independently)

Promoted and delta spec (identical text): *"the time-offset spread this balance produces never lengthens a read tap beyond the delay buffer's own capacity."*

Traced: `baseSeconds = ExpMapCompute(0.001f, kMaxDelaySeconds=2.0f, p.dtim)`, so at `dtim=1`, `baseSeconds = 2.0f` exactly. `SetSampleRate`: `capacity = min(kMaxDelaySamples=96000, ceil(2.0*sampleRate))` — at 48 kHz this is exactly 96000 samples = 2.0 s, i.e. `baseSeconds`'s own maximum already consumes the *entire* buffer with zero headroom. `widthSpread = p.dwid * baseSeconds * 0.35f * widthBalance` reaches `1.0 * 2.0 * 0.35 * 1.0 = 0.7f` at `dwid=1, widthBalance=1` (today's default). `timeR = baseSeconds + modSeconds + widthSpread` therefore reaches `2.0 + 0.7 = 2.7s` at minimum (before any modulation), i.e. **0.7 s / 33600 samples beyond `capacity`**, purely from `widthSpread`, with `dmod=0`.

`ReadAt` does not clamp or fail on this: `idx0I = floorPos % capacityI` wraps modulo `capacity`, silently aliasing the read to a position that does **not** correspond to the requested delay time — it reads whatever sample happens to sit `(requested_samples mod capacity)` back, not "no such history." This is exactly the buffer-capacity violation the clause forbids.

No `Check:` line exists anywhere in either spec version for this clause (the whole "Delay bank holds fourteen parameters" scenario carries no `Check:` citations at all, unlike the Drive/Reverb scenarios), and the existing `stereo_delay_width_balance_mapping_...` test (above) never calls `ReadAt`/`Process`, so nothing has ever exercised this invariant.

**Verdict: FALSE. Case 1 — always false, promoted wrongly**, pre-existing, confirmed independently of the other auditor's flag.

---

## Source: `preflight-9.md` — auditor six, hygiene and blast-radius axis (J2 finding)

## J2 — capacity clause (blocking archival)

**CONFIRMED**, arithmetic verified independently: `ExpMapCompute(min,max,v) = min·pow(max/min, v)` (`DspMath.hpp:52-55`), so at `dtim=1.0`, `baseSeconds = 0.001·pow(2000,1) = 2.0` exactly; `capacity = min(96000, ceil(2.0·48000)) = 96000` samples = 2.0 s exactly (`Delay.hpp:537`). Zero headroom before width is even added, matching the proposal's own `dtim=0.99` figure (`≈1.8536 s` base, `+0.6488 s` spread `→ ≈2.5024 s`) as a less extreme point on the same curve. I also confirmed by direct read that the Delay bank scenario in `spec.md` carries no `Check:` line at all (consistent with the "no Check by convention" note under the *Reverb* scenario, so this isn't a Delay-specific gap in that regard — but it doesn't rescue the false clause).

One layer J2 didn't state, which sharpens it further: task 1.3 offers two candidate fixes — "clamping the requested delay against capacity inside `ReadAt`" or "bounding `widthSpread` so `timeR` never exceeds capacity" — and explicitly allows landing either, or reporting both unresolved. Only the second option makes the spec sentence ("the time-offset **spread** ... never lengthens a read tap beyond capacity") literally true, since the sentence attributes the safety property to the spread computation itself, not to a downstream read-side clamp. If the clamp-in-`ReadAt` option ships, the spread still computes an out-of-range value and this sentence stays false regardless. Task 3.2's generic hand-check has no cue to notice that the two fix options aren't equivalent for this exact sentence. **Blocks archival**, same reasoning as J1: real, currently-false, and only nominally covered by a generic sweep that isn't pointed at it.

---

## Source: `preflight-10.md` — adjudication of the converged finding set (C6 finding)

## C6 — TRUE (both parts)

Part 1: `ExpMapCompute(min,max,v) = min*(max/min)^v` (`app/dsp/DspMath.hpp:52-55`); at `dtim=1.0`, `ExpMapCompute(0.001,2.0,1.0)=2.0` exactly (verified: `0.001*(2000)^1.0=2.0`). `capacity = min(kMaxDelaySamples, ceil(kMaxDelaySeconds*sampleRate))` (`Delay.hpp:537`) = 96000 at 48kHz = exactly `kMaxDelaySeconds` in seconds. So `baseSeconds` equals capacity **exactly** at `dtim=1.0`, and any nonzero `widthSpread` (any nonzero `dwid`) pushes `timeR` past capacity — a stronger, more general case than the `dtim=0.99` example `proposal.md` uses. Part 2: the promoted THEN clause (`spec.md:140-141`) reads "the time-offset spread this balance produces never lengthens a read tap beyond the delay buffer's own capacity" — its grammatical subject is the *spread itself*. Clamping downstream inside `ReadAt` (`Delay.hpp:949`) leaves the spread's own computed value unbounded (still requesting more than capacity); only bounding `widthSpread` upstream makes the sentence literally true of the quantity it names. **Consequence:** blocks EXECUTION of task 1.3 as scoped — its tie-break rule ("if they differ audibly, report both and do not choose") has no clause for spec-literal conformance, so an executor could correctly pick the audibly-better option while still leaving the promoted sentence false. Needs an explicit tie-breaker or the sentence itself corrected.

## The second route, measured through running code

The material above traces the width term into `timeR`. It is not the only route
into `ReadAt`'s unguarded modulo, and the change that recorded it did not know
that.

`capacity` equals `kMaxDelaySeconds` exactly at 48 kHz, so `baseSeconds` alone
reaches exactly capacity at `dtim = 1.0` with no headroom for any other term.
Reading exactly `capacity` is CORRECT and must not be clamped away: the write
happens after the read within the same `Process` call, so `line[writePos]` still
holds the sample from exactly one full lap ago. Measured, not argued — at
`dtim = 1.0, dwid = 0.0` the true lag comes back as exactly 96000 samples:

```
dtim=1.0 dwid=0.0 (no width contribution at all): capacity=96000 actualLagLSamples=96000 actualLagLSeconds=2.000000 peakAbsL=0.797590
```

An earlier working criterion of `capacity - 1` was one sample too conservative,
and a clamp built to it would shorten this correct read.

### The modulation term, into `timeL`

`modSeconds = sin(lfoPhase) * p.dmod * baseSeconds * 0.08` is added to BOTH
`timeL` and `timeR`, and is signed. Nothing clamps `timeL` at all. At
`dtim = 1.0` and `dmod = 1.0` — both ordinary knob endpoints — `modSeconds`
swings about ±0.16 s around a `baseSeconds` already equal to capacity, so the
positive half of every LFO cycle asks for more than the buffer holds.

Measured through real `Process`, with Width, Feedback, Freeze, Reverse and
Diffusion all zeroed so only this route is live, injecting a single-sample
impulse at 24 points across one LFO period:

```
[mod route probe] k=49000 phase=1.60352125 requestedSamples=103675.888 measuredLagSamples=7389 peakMag=0.5859375
[mod route probe] WORST: k=49000 phase=1.60352125 requestedSamples=103675.888 measuredLagSamples=7389
```

A request of about 103676 samples reads back at 7389 — off by essentially one
full lap. Across the sweep, `requestedSamples` runs from about 88324 at the
trough to about 103676 near phase pi/2, exceeding capacity for roughly half of
every LFO cycle. The defect is periodic, not a one-off spike: the echo collapses
from a ~2.1 s repeat to a ~0.15 s slapback and back, twice per cycle, while the
knobs themselves move smoothly.

### Nothing else reaches it

`timeL` is built only from `baseSeconds` (driven solely by `p.dtim`) and
`modSeconds` (driven by `p.dmod` and `baseSeconds`). No other `DelayParams`
field and no other member of `StereoDelay` feeds it; `p.dwid` reaches `timeR`
only. That enumeration is what makes "two routes" a closed claim rather than
"two found so far".

### Why the width-only bound is not the repair

Bounding the width spread so `timeR` fits makes the promoted spec sentence about
the spread literally true, and does not touch the modulation route. A capacity
repair guarding one of two routes into the same unguarded modulo is a partial
repair. The superseded change presented it as the whole of the second defect
because this route had not been found.
