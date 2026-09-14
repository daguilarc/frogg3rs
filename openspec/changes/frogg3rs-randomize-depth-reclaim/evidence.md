# Evidence — how the cause was established

This is the canonical record of the debugging session that found this bug. It
is written to be re-read by someone who was not there, including the failed
attempts, because several of them are the reason the conclusions are narrow.

Session date: 2026-09-13. Deployed build under test: `buildId 989b01c8…` at
https://daguilarc.github.io/frogg3rs/ .

## The reported symptom

The operator reported that the browser build "crashes" after repeated
Randomize All: sound cuts out abruptly and never returns, the oscilloscope
keeps drawing, no console error appears, and the DSP meter does not look
especially high. The initial reading was that the synth was NaN-ing out.

## Conclusion

Not NaN. Repeated Randomize All materialises modulation-depth parameters that
are never released; per-block cost ratchets up until the audio callback is
consuming essentially all of its real-time budget, and the sound stops. What
the audio stack does past that point is NOT traced here: the callback was
still running when measured (a stopped callback would freeze the deadline
meter, and it instead varied and then decayed). The observable is "callback at
its budget, no sound". The
oscilloscope keeps drawing because the UI repaints from published state at
30 Hz on the message thread, which has no dependence on the audio callback
meeting its budget.

## The argument against NaN, and its limits

`CollectNeutralLocalParameters` recycles parameter slots
(`External/Sheaf/projects/synth/src/ParameterModulation.cpp:787-796` →
`Parameter::CollectNeutralChildren`, `:1089-1106` → `RecycleLocalParameter`,
`:768`). It touches no DSP state whatever: `delay_`, `reverb_`,
`filterChain_`, `audioVcos_` and `outputLimiter_` are `FroggersAppCore`
members and are not reachable from it.

If the silence were caused by poisoned DSP state — a NaN in a filter, delay or
reverb — then recycling parameter slots could not restore the audio. Audio did
return after a release, on the operator's own reproduction.

Three limits on how far that carries, none of them resolved here:

- The release is NOT inert with respect to a modulation-driven divergence: it
  sets `modulationDepths_[sourceIx] = nullptr`, severing routes. So it excludes
  a poisoned filter state; it does not exclude a divergence driven by
  modulation that severing the route would also stop. Failed attempt 4 below
  leaves that hypothesis untested, and this argument does not close it.
- The one audio-restoration observation is the browser drill-out, which does
  more than release: it unpins every visible cell, resets the view, clears the
  selection, and replays presses. Nothing isolates the release.
- No native measurement anywhere in this document shows AUDIO restored by a
  release. Measurements 1, 4 and 7 measure cost and counts.

So: the cost explanation is well supported on its own evidence, and the
arithmetic explanations are not excluded to the same standard. The change
rests on the former.

## Measurement 1 — native cost ratchet

`SynthRig<FroggersApp>`, 256-frame blocks, 200 blocks per measurement.

| state | us/block | vs baseline |
| --- | --- | --- |
| baseline | 3265.1 | 1.00x |
| after 50 Randomize All (4-block cadence) | 14801.7 | 4.53x |
| after Reset All | 16730.2 | 5.12x |
| after explicit reclaim, 1066 parameters collected | 3104.3 | 0.95x |

Reset All does not give the cost back. The reclaim gives all of it back.

Re-derived independently in a fresh context on the same tree, with parameter
counts attached:

| state | us/block | liveLocal | topLevel | freeSlots |
| --- | --- | --- | --- | --- |
| baseline | 1396.9 | 6 | 91 | 0 |
| after 50 Randomize All | 6176.8 | 1072 | 91 | 0 |
| after Reset All | 6297.8 | 1072 | 91 | 0 |
| after reclaim (1066 collected) | 1253.3 | 6 | 91 | 1066 |

Absolute microseconds differ between the two runs (different machine load);
the ratios and the 1066 figure reproduce exactly. Only ratios are relied on
anywhere in this change.

## Measurement 2 — which quantity actually drives the cost

`ParameterProcessingObserver` counters over 100 blocks:

| state | topLevelProcessLite | localRecursiveCompute | activeRouteVisits | us/blk |
| --- | --- | --- | --- | --- |
| baseline | 2,329,600 | 9,600 | 153,600 | 762.9 |
| after 50 Randomize All | 2,329,600 | 1,715,200 | 1,868,800 | 2659.7 |
| after Reset All | 2,329,600 | 1,715,200 | 153,600 | 2449.6 |
| after reclaim | 2,329,600 | 9,600 | 153,600 | 968.0 |

This discriminates the candidates:

- The per-sample top-level walk is ruled out: `topLevelProcessLite` never
  moves, because `ParameterGroup::ProcessSamplePhase1` walks
  `topLevelParameters_` only, and that stays at 91 throughout.
- Active route count is ruled out: `activeRouteVisits` returns to baseline on
  Reset All while the cost does not.
- Storage footprint is ruled out: `freeSlots=1066` after the reclaim means the
  batches are still allocated, yet cost is back at baseline.

The only quantity that moves with the cost in all four states is
`localRecursiveCompute` — the recursive `Compute()` descent into materialised
local depth parameters, at control rate (1600 computes x 1072 depths =
1,715,200 exactly; 1600 x 6 = 9,600 exactly). **Cost is proportional to live
local depth-parameter count, through the recursive Compute descent.**

## Measurement 3 — the operator's reproduction, in the deployed browser

Real Chrome, deployed build, operator's own reproduction, reached by pressing
Randomize All repeatedly. Critically: no second instance of the app was
running during this measurement (an earlier attempt was contaminated — see
Failed attempt 5).

| state | DSP meter |
| --- | --- |
| reproduced bug, sound cut, oscilloscope still drawing | 98–101% |
| immediately after drill-out | 52% |
| +3.7 s | 45% |
| +5.7 s | 39% |
| +7.8 s | 37% |

The operator confirmed the sound returned. The meter decays over several
seconds rather than stepping, which is why an earlier check "a moment after"
the reclaim wrongly read as no recovery.

98–101% is the whole story: it is not a dramatic number, which is why it was
dismissed, but 100% of the real-time budget is a cliff, not a slope.

## Measurement 4 — the escape route that already exists

Driving only the real request API:

```
baseline                              us/blk= 746.2  liveLocal=   6  freeSlots=  0
after 50 Randomize All                us/blk=2966.6  liveLocal=1072  freeSlots=  0
after drill-in press (level 1)        us/blk=2987.7  liveLocal=1072  freeSlots=  0
after same-bank click (level 0)       us/blk=1163.9  liveLocal=  73  freeSlots=999
```

`RequestBankSelect` on the already-selected bank drains as
`while (drillIn_->Level() > 0) drillIn_->Back();`
(`app/FroggersAppCore.hpp`, the ProcessFrame drain);
`FroggersModulationDrillIn::Back()` calls `bank_->Deselect()`
(`app/FroggersModulation.hpp:878`); `Bank::Deselect` ends with
`CollectNeutralLocalParameters()`
(`External/Sheaf/projects/synth/src/ParameterModulation.cpp:2717`).

So the app already reclaims, on drill-out, and the operator's accidental
workaround — drill into a knob, click the bank tab you are already on — is
that path. It is undocumented, unreachable by intent, and 999 slots are pushed
onto a vector reserved for 96 (`:667`) from the audio thread when it fires.

`app/FroggersModulationTests.cpp:347-355` states this path in prose already.

## Measurement 5 — denormals, UNCONTROLLED and not relied upon

macOS/arm64 runs FZ=0 by default; the VST sets `juce::ScopedNoDenormals`
(`app/vst/FroggersPluginProcessor.cpp:553`); wasm has no flush-to-zero at all.

| | FZ=0 (wasm-like) | FZ=1 (VST-like) |
| --- | --- | --- |
| baseline | 1396.9 | 742.5 |
| after storm | 6176.8 (4.42x) | 2928.6 (3.88x) |
| after Reset All | 6297.8 — rises | 2685.8 — falls |

**This measurement is uncontrolled and nothing in the change depends on it.**
The Reset All direction it rested on — that reset makes cost slightly worse —
does not reproduce: across three repetitions on a loaded machine, cost fell
after Reset All twice and rose once, and the original 4.53x-to-5.12x delta sits
inside run-to-run noise. "If the controlling quantity did not move, the run is
VOID, not negative" -- so that is not a negative result, it is a void
one. The FZ=0/FZ=1 difference in absolute cost is real and large; the claim
built on the 2% reset delta is withdrawn.

## Failed attempts, and what each one invalidates

Recorded because each one produced a confident wrong answer first.

1. **"Nothing in this app ever calls the reclaim."** Produced by grepping
   `CollectNeutralLocalParameters` in `app/` and seeing only comments. False:
   the call chain runs through `Back()` → `Deselect()`. Searching by the
   symbol's spelling answered a narrower question than the one asked.

2. **"The output stays finite, so it is not NaN."** The tap used was
   `SynthRig`'s captured output, which is downstream of `GuardOutputSample`
   (`app/FroggersAppCore.hpp:2108-2116`), which returns `0.0f` for any
   non-finite sample. The same blindness had already been cited, correctly, as
   the reason the existing storm gate cannot fire. As a NaN test that run is
   void, not negative. The NaN question is instead settled by the argument at
   the top of this document.

3. **Static maximum-feedback sweep.** Delay and Reverb loop parameters driven
   to their reachable extremes produced no divergence. Wrong test: the
   hypothesis under examination was audio-rate modulation of those parameters,
   which is a time-varying system, not an extreme frozen one.

4. **Audio-rate modulation routed at depth — three void runs.** Attempts to
   route an audio-rate source onto Delay Feedback / Feedback drive / Freeze /
   Reverb Decay / Tank drive produced bit-identical output peaks across every
   distinct target, which is the signature of an input that never landed.
   Controls confirmed it: the depth read back as not materialised in one
   attempt, and as exactly its neutral default (0.5000) in another. A depth
   cell cannot be driven from outside the app's own slot → selected-bank
   routing, and a side-constructed drill-in does not participate in it.
   **The audio-rate-instability hypothesis is untested, not refuted.**

5. **Contaminated browser measurement.** A first reading of the operator's tab
   showed DSP 102–120% and a drop to 61% after drill-out. A second instance of
   the app — opened by the debugging session itself — was running in the same
   browser throughout, so that reading includes contention introduced by the
   measurement. Superseded by Measurement 3, taken with the second instance
   closed and verified silent. The lesson is recorded because the contaminated
   numbers were reported as evidence before the confound was noticed.

6. **`innerText` used to detect drill state.** Reported "not drilled in"
   against a screenshot plainly showing `Modulation Level 1`, because that
   text is drawn on canvas rather than in the DOM. Screenshots are the
   reliable instrument for this app's view state.

## What is still open

- Whether audio-rate modulation of a feedback loop can diverge on its own is
  untested (Failed attempt 4). It is not needed to explain the reported bug,
  and it is not claimed either way here.
- Randomize All randomises one level when pressed at level 0. Pressed while
  drilled in it reaches TWO levels — it randomises the selected parameter's
  depths and then each depth it materialises — which the promoted spec already
  requires ("each depth the press materializes is itself randomized under the
  same floor"). An earlier draft of this document said the operator's depth-3
  trees could only have been built by deliberate per-level presses; that is
  wrong, one drilled-in press reaches depth 3 from level 1. The conclusion is
  unchanged: bounding randomize's reach is NOT proposed, because level-0
  presses alone take live depths from 6 to 1072.


## Measurement 6 — the re-roll itself is correct

Counting, after each press, how many of the 84 page parameters carry at least
one non-neutral modulation depth, and how many non-neutral depth cells exist:

| press | params carrying modulation | non-neutral cells |
| --- | --- | --- |
| 0 | 3 | 6 |
| 1 | 36 | 65 |
| 2 | 44 | 93 |
| 4 | 43 | 82 |
| 7 | 48 | 121 |
| 12 | 41 | 82 |

No growth. The figure fluctuates around 42 parameters and ~80 cells across
twelve presses. **Randomize All already re-rolls from a clean slate**: values
are redrawn, and no stale assignment survives a press. The promoted badge
requirement holds.

This was run to test the opposite prediction — that accumulation would leave
stale badges and violate the promoted requirement — and it refuted it. The
defect is therefore narrower than that: the values re-roll, the storage does
not get released.

It also cross-checks Measurement 4 from an independent direction. A release
leaves 73 live depths; a roll independently uses ~80 non-neutral cells. Two
probes written for different purposes agree that a release leaves precisely
what the current roll uses.

## Measurement 7 — the fix, simulated

Two arms, identical presses. Arm A is today. Arm B calls the release after
every press. Cost is a ratio against each arm's own baseline.

| presses | A: today | B: release in the re-roll |
| --- | --- | --- |
| 1 | 71 depths, 1.47x | 65 depths, 1.31x |
| 5 | 355 depths, 2.24x | 81 depths, 1.36x |
| 10 | 622 depths, 3.56x | 84 depths, 1.40x |
| 25 | 943 depths, 4.25x | 76 depths, 1.50x |
| 50 | 1072 depths, 4.72x | 73 depths, 1.41x |

Arm B is flat in both columns. The ratchet does not exist there. The residual
1.4x is the honest cost of a randomized patch, which carries ~80 active depths
where the default patch carries 6.

This is the change's positive control: it demonstrates the proposed operation
produces the proposed outcome, before any production code is written. It also
sets the check thresholds in `tasks.md` — first-press floor 30 against a
measured 65, fiftieth-press ceiling 150 against a measured 73 and an observed
peak of 128 across the storm.
