# Axis 0.1 — the deciding measurement: single guard vs. per-term bounds

Preflight axis 0.1 (`tasks.md` task 0.1), run as a behavioural check before
execution, per the proposal's own framing. This is a MEASUREMENT record, not
an implementation; nothing below is landed. All edits were temporary, made
directly to `app/dsp/Delay.hpp` and a temporary probe appended to
`app/FroggersDspParityTests.cpp`, built with `nice -n 10 make -j2` against the
absolute binary path (rm'd before each rebuild), run for real, and reverted.

**Files touched during measurement, backed up first, restored exactly,
verified by md5 (`md5` command, byte-for-byte):**

```
MD5 (app/dsp/Delay.hpp) = 2090d9902bae29e67d7eb57a27991cef            (before, and after restore)
MD5 (app/FroggersDspParityTests.cpp) = 35b80ce13d6a4ffebae993663eabcddf  (before, and after restore)
```

These match the baseline recorded independently in
`research/adversarial-check-audit.md`, confirming this measurement started
from the same shared uncommitted tree every other concurrent auditor is
reading. (Mid-session, a concurrent auditor's own restore cycle reset both
files to this same baseline while this measurement was mid-edit — evidence
the tree is genuinely shared; re-applied the probe from scratch afterward and
re-verified.) `git status --short` at the end of this measurement:

```
 M app/FroggersDspParityTests.cpp
 M app/dsp/Delay.hpp
```

No residual diff beyond the pre-existing `M` state.

## Instrument

A temporary `TEST_CASE(temp_axis01_capacity_guard_probe)` appended to
`app/FroggersDspParityTests.cpp` (reverted, not landed). It fires a single
impulse through a real `dsp::StereoDelay::Process` and locates where it
reappears in the wet output — the same black-box technique
`MeasureReadAtLagSamples` (already in the shipped suite) and the prior
adversarial audit both use, extended here to read BOTH channels from **one**
shared stream of `Process()` calls per experiment (a first draft measured
each channel with an independent second call over the same `delay` object;
that silently fires a second, later impulse and advances `lfoPhase` an extra
`measureLen` samples between the two channel readings — invisible for a
stationary delay time, and wrong the moment modulation makes the read
time-dependent, which is exactly this axis's scenario 3; caught by the
symmetry check below, not assumed).

Three scenarios, matching the proposal/tasks.md's own cited points:

1. **Exact-capacity read** — `dtim=1.0, dwid=0.0, dmod=0.0`. `baseSeconds`
   equals `capacitySeconds` exactly at 48 kHz (`ExpMapCompute(0.001,2.0,1.0)
   == 2.0`, `capacity = min(96000, ceil(2.0*48000)) == 96000`). Correctness
   requirement: this read must land at exactly `capacity` samples, not fewer.
2. **Route one, the width term** — `dtim=0.99, dwid=1.0, dmod=0.0,
   widthBalance=1.0`. The proposal's own cited overrun point.
3. **Route two, the modulation term, worst phase** — `dtim=1.0, dwid=0.0,
   dmod=1.0`. `lfoPhase` advances by a fixed `lfoInc` (0.25 Hz default) with
   a single-subtract wrap every `Process()` call; that exact update rule was
   replicated offline (in the probe, in Python during derivation) to locate
   the call index where `sin(lfoPhase)` peaks (phase = pi/2, maximum positive
   `modSeconds`), confirmed at `bestSin=0.999999046`. A single impulse fires
   there.

**Positive control.** Scenario 3 run against the untouched baseline tree
(before any candidate edit) reproduces the adversarial audit's own recorded
figure independently:

```
[axis01 scenario3 mod-route worst, BASELINE/unguarded] capacity=96000 worstCall=239617 bestSin=0.999999046
  requested=103679.993 lagL=7454 peakAbsL=0.59375 lagR=7454 peakAbsR=0.59375
```

`requested=103679.993` matches `adversarial-check-audit.md`'s
`expectedUnclampedLagSamples=103679.993` exactly, and `measuredLagSamples=7454`
matches its `measuredLagSamples=7454` exactly (their run used only the right
channel; this one confirms the left channel independently and finds it
identical, as symmetry requires when `dwid=0`). `peakAbs` nonzero (0.59) is
the liveness check per §6.1 — a dead instrument reads 0, this doesn't. Had
the instrument been dead (e.g. a probe wiring bug), scenario 3 would read
`lag=0, peakAbs=0`, which is in fact what an earlier, buggy version of this
same probe printed before two real bugs in the probe itself were found and
fixed (documented below) — the dead-instrument case was observed, not merely
hypothesized.

**Two bugs found and fixed in the probe itself, recorded because they
produced literal wrong output before correction, per §0/§6.1 (a run whose
controlling quantity does not move is void, not negative):**

1. First attempt conflated an absolute warmup call-count with the
   measurement loop's own zero-based counter, so the impulse condition
   (`i == impulseIndex`) was never reached inside a shorter measurement
   window — no impulse ever fired, and both channels read exactly
   `lag=0, peakAbs=0`. Fixed by warming the object up to just before the
   worst-phase call, then passing `impulseIndex=0` to the measurement loop
   (the very next call is the worst-phase call).
2. Second attempt measured L and R with two sequential independent calls
   over the same `delay` object, silently running a second experiment
   starting from an already-advanced state (see instrument description
   above) — produced `lagL=7454, lagR=88533` for a case where `dwid=0`
   requires the two channels to be bit-identical by construction (same
   input history, same formula, `CrossFeedPair` is identity at `cross=0`).
   The asymmetry itself was the tell. Fixed by measuring both channels from
   one shared call stream.

## Candidate A — single guard, at `ReadAt` (covers both routes by
construction)

Implemented as: clamp the requested `seconds` argument to `capacitySeconds`
(not `capacitySeconds` minus any sample of headroom) at the top of
`ReadAt`, before it is converted to samples. Because both `ReadAt(timeL,
lineL)` and `ReadAt(timeR, lineR)` funnel through this one function, this is
a single call-site guard, not two. To isolate it, the existing per-term
`widthSpread` bound (`maxSpreadSeconds`) was neutralized for this run only
(`widthSpread = widthSpreadRaw`) — otherwise the pre-existing landed bound
would also be acting and the measurement would not isolate Candidate A.

```
[axis01 scenario1 exact-capacity]         capacity=96000 lagL=96000  peakAbsL=0.797590315 lagR=96000  peakAbsR=0.797590315
[axis01 scenario2 width-route]            capacity=96000 requestedRunbounded=120114.301   lagL=88974  peakAbsL=0.5546875   lagR=96000  peakAbsR=0.797590315
[axis01 scenario3 mod-route worst]        capacity=96000 worstCall=239617 bestSin=0.999999046 requested=103679.993 lagL=88551 peakAbsL=0.5703125 lagR=88551 peakAbsR=0.5703125
```

Reading: scenario 1 stays at exactly `96000` — the exact-capacity read
survives, unshortened. Scenario 2's right channel clamps from an unbounded
request of ~120114 samples to exactly `96000` (capacity), not `96000-100` or
any other conservative margin — zero headroom needed, matching scenario 1's
own finding that exactly `capacity` is safe. Scenario 3, both channels
(equal, as `dwid=0` requires): `88551`, no longer the catastrophic
14x-aliased `7454` — this is a legitimate value inside the modulation's own
natural range (`baseSeconds ± dmod*baseSeconds*0.08` = `96000 ± 7680`
samples = `[88320, 103680]`), i.e. wherever the clamped, continuously-varying
requested delay happens to re-align with the write pointer, not an aliasing
artifact. Route two is closed.

`188/191` local tests pass with the outer bound neutralized (the shipped
`stereo_delay_width_spread_never_reads_past_the_line_capacity` fails by
construction here, since it asserts `lagSamples < capacity` strictly and this
candidate reads exactly `capacity` — an expected, deliberate consequence of
isolating this candidate for measurement, not a defect in the candidate).

## Candidate B — per-term bounds (modSeconds bounded directly; widthSpread
bounded against the now-bounded modSeconds)

Implemented in `Process()`, `ReadAt` left untouched:

```cpp
const float modSecondsRaw = std::sin(lfoPhase) * p.dmod * baseSeconds * 0.08f;
const float capacitySeconds = static_cast<float>(capacity) / sampleRate;
const float maxModSeconds = capacitySeconds - baseSeconds;
const float modSeconds = std::min(modSecondsRaw, maxModSeconds);
const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
```

No headroom subtracted anywhere (no `- 1.0f/sampleRate`, no other margin):
scenario 1 below is the proof that none is needed.

```
[axis01 scenario1 exact-capacity]         capacity=96000 lagL=96000  peakAbsL=0.797590315 lagR=96000  peakAbsR=0.797590315
[axis01 scenario2 width-route]            capacity=96000 requestedRunbounded=120114.301   lagL=88974  peakAbsL=0.5546875   lagR=96000  peakAbsR=0.797590315
[axis01 scenario3 mod-route worst]        capacity=96000 worstCall=239617 bestSin=0.999999046 requested=103679.993 lagL=88551 peakAbsL=0.5703125 lagR=88551 peakAbsR=0.5703125
```

**Numerically identical to Candidate A in every scenario measured.** This is
expected, not a coincidence to explain away: both candidates compute the same
final bound on the total requested read time (`capacitySeconds`, zero
headroom) and differ only in WHERE that bound is applied — downstream at the
shared `ReadAt` call site (A) vs. upstream at each contributing term (B). For
any single combination of knobs the two are the same total clamp, so the
observable (the actual read time) is the same. `188/191` local tests pass
here too, for the same reason as Candidate A (the shipped near-capacity check
does not yet accept an exact-`capacity` read; that check's own bound is
task 1.2(b)'s job, not this axis's).

## What distinguishes them: the promoted spec sentence

`openspec/specs/froggers-sheaf-parameter-model/spec.md`, Delay bank scenario,
slot 12 (Width Balance), read directly:

> **THEN** the time-offset spread this balance produces never lengthens a
> read tap beyond the delay buffer's own capacity

The grammatical subject is **the spread itself** — `widthSpread`, the
quantity Width Balance actually scales — not the read time as a whole and not
whatever `ReadAt` is asked to do with it.

- **Candidate A** (guard at `ReadAt`) leaves `widthSpreadRaw` computed and
  assigned to `widthSpread` with no bound of its own — in the isolated
  measurement above, `widthSpread` at scenario 2's settings is the full
  unbounded `~0.6488s` raw value; only the downstream call to `ReadAt`
  prevents that from producing an out-of-range READ. The spread computation
  itself still "lengthens" past what the sentence promises; only the
  consequence is intercepted one call later. **The sentence stays false**
  under this candidate, for the same reason `preflight-10.md`'s adjudicated
  finding (C6, carried in `research/read-at-capacity.md`) already gives:
  "Clamping downstream inside `ReadAt` leaves the spread's own computed value
  unbounded ... only bounding `widthSpread` upstream makes the sentence
  literally true of the quantity it names."
- **Candidate B** bounds `widthSpread` at its own computation site, against
  the (separately, also directly bounded) `modSeconds` term. The spread
  itself is now the bounded quantity the sentence describes.
  **The sentence becomes literally true** under this candidate.

Candidate B also needs its own separate bound on `modSeconds` to close route
two — the sentence names only the spread, and correctly: `modSeconds` is not
"the spread," so closing route two does not, by itself, make or break this
particular sentence. It is a second, independent per-term bound alongside the
existing one, not a rewording of what the sentence already asserts.

## Answer written into task 1.1

**Mechanism: per-term bounds (Candidate B).** Bound `modSeconds` directly so
`baseSeconds + modSeconds` never exceeds `capacitySeconds` (closes route two,
into both `timeL` and `timeR`), then bound `widthSpread` against the
already-bounded `baseSeconds + modSeconds` so `timeR` never exceeds
`capacitySeconds` (closes route one). Both bounds use **zero headroom** —
clamp to `capacitySeconds` exactly, not `capacitySeconds` minus any margin.

**What headroom the mechanism needs, and what measurement establishes it:**
zero. Scenario 1 above measures a request of exactly `capacity` samples
(`dtim=1.0, dwid=0, dmod=0`) reading back at exactly `capacity` samples with a
non-degenerate nonzero peak (`peakAbs=0.7976`), both under the untouched
baseline and under both candidates. The line's write happens after its read
within the same `Process()` call, so the sample from exactly one full lap ago
is still present when the read for that lap executes — there is nothing to
reserve a sample against. The `capacity - 1` figure currently in the tree
(`- 1.0f / sampleRate` in the landed `maxSpreadSeconds`) is one sample more
conservative than this measurement supports and is not carried forward.

**Which way `stereo_delay_width_spread_bound_holds_across_the_reachable_grid`'s
baseline moves:** from `capacity - 1` to `capacity` (task 1.2(b)'s job to
land; also needs to start exercising `modSeconds`, which it currently omits,
and to call production rather than recomputing the formula inline — pre-
existing holes this axis does not repair, only names).

**What assertion would catch a guard that is too conservative** (task 0.1 /
1.2(b)): the shipped near-capacity check's tolerance window,
`REQUIRE_TRUE(lagSamples > capacity - 100)`, absorbs up to 99 samples of
excess headroom by construction (`adversarial-check-audit.md`, Attack 4,
confirmed there at both 50 and 99 samples). A tolerance built from this
axis's own zero-headroom finding closes that gap: assert the measured lag at
the reachable worst point (`dtim=1.0, dwid=1.0, widthBalance=1.0`, and now
also `dmod` swept since route two shares the same bound machinery) sits
within roughly one linear-interpolation sample of `capacity` exactly —
`REQUIRE_NEAR(lagSamples, capacity, 1.0)` rather than a 100-sample one-sided
window — so a guard that regresses to 2+ samples of unnecessary
conservatism fails immediately instead of passing silently the way the
current window allows for anything up to 99.

**Both candidates close both routes and preserve the exact-capacity read;**
they diverge only on the spec-literal question above, which is what selects
Candidate B.
