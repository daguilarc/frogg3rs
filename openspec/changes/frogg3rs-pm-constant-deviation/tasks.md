# Tasks — `frogg3rs-pm-constant-deviation`

PLANNED ONLY. Not yet preflighted. Tier per task: **H** = mechanical, **S**
= decides what something means. Builds under `nice`, `-j2` at most.

## 1. Preflight

- [ ] 1.1 Every citation in `proposal.md` resolves at the current tree.
- [x] 1.2 `WrapPhase` is `p - floor(p)` (`app/dsp/DspMath.hpp:45-48`): folds
      any magnitude into [0, 1), so the floor's 1.5-cycle swing is safe.
- [ ] 1.3 Read the default patch's three VCO pitch values and compute the
      deviation in cents at each for today's 2 Hz floor; record them as the
      mechanism behind "VCO1 inaudible, VCO2 and VCO3 barely".
- [ ] 1.4 OPERATOR: Hz-constant (proposed) or cents-constant deviation. If
      cents, rewrite the Design section before group 2.
- [ ] 1.5 First run of the gates, recorded as the before line.

## 2. The measurement (finding first) — S

- [ ] 2.1 Cherry-pick the test from `ed956cd`
      (`pm_pitch_deviation_is_the_same_at_every_rate`) alone; run the parity
      binary: it must FAIL at knob 0 and 0.5 with 1.9 Hz and 5.9 Hz printed
      and pass at knob 1. A pass at knob 0 voids the gate.

## 3. The change — H

- [ ] 3.1 Cherry-pick the `Vco.hpp` hunk of `ed956cd` (the multiply in
      `StepPmLfo`); rewrite its two comment blocks so they state the rule
      (the audible quantity is the slope of the phase offset, so the LFO is
      scaled by `kPmLfoMaxHz / rate` and the deviation is
      `2 pi x kPmLfoDepth x kPmLfoMaxHz` at every rate) and name no earlier
      constant, no earlier behaviour, and no fix.
- [ ] 3.2 Gate: `nice make -C app -j2 test`; 2.1 passes with 18.8 Hz at
      all three knob positions; the numbers go into the ledger.

## 4. Spec and manual — S

- [ ] 4.1 The delta in `specs/froggers-vco-topology/spec.md`; `openspec
      validate frogg3rs-pm-constant-deviation --strict`.
- [ ] 4.2 `MANUAL.md`, present tense, no history. Ph.mod paragraph
      (`:366-368`): "...grows from a subtle vibrato into an increasingly
      warbly, FM-like wobble at full depth. The size of the wobble is set
      here and does not change with the rate." PM rate paragraph
      (`:376-378`): "one shared knob (2 Hz-20 Hz) setting how fast the
      phase-mod wobble runs for all three VCOs at once. Depth and rate are
      independent: this sets the speed for all three, and each VCO's own
      Phase mod knob sets how far its pitch swings." `QUICK_DICT.md:23`
      unchanged (it states the range only).
- [ ] 4.3 Gate: `openspec validate --all --strict`.

## 5. Verification and delivery

- [ ] 5.1 Every gate in the table on the final tree; VST rebuilt and
      `ctest`.
- [ ] 5.2 Postflight (S, fresh context): implementation versus this text.
- [ ] 5.3 Push to `main`; rebuild the three releases; ledger with the
      before and after deviation numbers.
- [ ] 5.4 OPERATOR: on the desktop app, PM rate at its floor with any
      Ph.mod knob up wobbles the pitch plainly on all three VCOs, and the
      top of the rate knob sounds as it did.
