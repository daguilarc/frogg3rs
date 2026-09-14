# Preflight 8 — auditor five, spec-versus-code axis (initial report + debate verdicts)



===== BLOCK 1 of 3 (timestamp: 2026-09-14T06:08:45.552Z) =====


Confirmed: slot indices 9-13 map exactly as the spec states (9→FeedbackDrive, 10→FeedbackTone, 11→ModRate, 12→WidthBalance, 13→Crush). Now let me confirm the widthSpread/capacity math precisely and check `ExpMapCompute`/`kMinDryLevel` definitions to nail the worked example.


===== BLOCK 2 of 3 (timestamp: 2026-09-14T06:12:03.182Z) =====


# Preflight Audit Five — Spec vs. Code Truth: Delay Bank

Scope read: `openspec/specs/froggers-sheaf-parameter-model/spec.md` (promoted), `openspec/changes/frogg3rs-delay-width-wysiwyg-repair/specs/froggers-sheaf-parameter-model/spec.md` (delta), `app/dsp/Delay.hpp`, `app/FroggersParameters.hpp` (slot table), `app/FroggersAppCore.hpp` (`RouteDelayBank`, lines ~1901–1935, for the actual slot→setter wiring), `app/FroggersDspParityTests.cpp` (existing Width Balance test).

## Slot layout and mechanism clauses (Delay bank scenario)

Slot table confirmed at `FroggersParameters.hpp:262-282` and wiring at `FroggersAppCore.hpp:1931-1935`: slots 9–13 = FbDr/FbTn/MdRt/WBal/Crsh exactly as both specs state; slots 0-1 = Wet/dry, Send; slots 2-8 = Delay time, Feedback, Stereo width, Freeze, Mod depth, Reverse blend, Diffusion, matching `DelayParams`'s field order (`dtim,dfbk,dwid,dfrz,dmod,drev,ddif`). **TRUE**, case 3 (true today, stays true; unaffected by this delta).

- **Feedback Drive** ("pre-gain... never to that saturator's output"): `Process()` — `PadeSaturator::Saturate(fbDrive * fbL)`. fbDrive multiplies the argument, not the result. **TRUE**, case 3.
- **Feedback Tone** ("one-pole lowpass ahead of the same in-loop saturator"): `fbL = fbToneL.Process(fbL)` precedes the `Saturate(...)` call. **TRUE**, case 3.
- **Mod Rate** ("the delay's own modulation LFO rate"): `SetModRate` recomputes `lfoInc`. **TRUE**, case 3.
- **Crush** ("applied to the feedback tap's repeats"): `crushL/R.Process(fbL/fbR)` runs on the feedback tap before tone/saturate, every repeat. **TRUE**, case 3.
- **Cross-feed weight stays within [0,1], convex combination**: pre-change, `cross = dwid*0.5*widthBalance` maxes at 0.5 (dwid=1, widthBalance=1) — inside [0,1]. Post-change, `cross` is a hardcoded `0.0f` — trivially inside [0,1]. **TRUE before and after.** Case 3.

## Finding 1 — Width Balance is not a ratio control; it never was

Promoted and delta spec (identical text): *"Width Balance (WBal), the ratio between the Width knob's time-offset spread and its cross-feed blend, independent of the Width knob's own value."*

Code (`Delay.hpp`, pre-change): `cross = p.dwid * 0.5f * widthBalance`; `widthSpread = p.dwid * baseSeconds * 0.35f * widthBalance`. Both terms carry `widthBalance` as an identical linear multiplier. Algebraically, `widthSpread / cross = 0.35*baseSeconds / 0.5 = 0.7*baseSeconds` — **independent of `widthBalance` entirely**, not just of `dwid`. Turning WBal does not change the ratio between the two mechanisms at all; it scales both together (0 → nothing, 1 → today's full 0.35/0.5 literals). This is a joint amount knob, not a balance/ratio knob.

The only existing test touching this, `stereo_delay_width_balance_mapping_keeps_cross_in_0_1_and_spread_at_or_below_todays_max` (`FroggersDspParityTests.cpp:6935`), does not test a ratio either — it only checks `cross∈[0,1]` and `spread ≤ today's ceiling`, by recomputing the identical formula inline. It cannot distinguish "ratio control" from "joint scale control."

**Verdict: FALSE. Case 1 — always false, promoted wrongly**, pre-existing and independent of this delta.

**This delta makes it worse, not better.** The change sets `cross` to a hardcoded `0.0f`, permanently decoupled from `widthBalance`. Post-change, WBal has *zero* effect on any cross-feed (there is none to balance), so the "ratio between spread and its cross-feed blend" framing becomes not just inaccurate but structurally impossible to satisfy — there is no second mechanism left to hold a ratio against. The delta's own restated Delay-bank scenario (delta lines ~287–305) is carried forward **verbatim**, unedited, so archival will promote text describing a mechanism the delta's own code change eliminates.

**Consequence if not fixed:** anything (manual, automation preset, future modulation routing) that reasons about WBal as trading spread against cross-feed will misdescribe the control to the operator — exactly the WYSIWYG defect this change is named for repairing, reintroduced in the very requirement text this change touches.

**§8.0 scope:** `app/dsp/Delay.hpp`'s `cross` line and this exact spec paragraph are both directly edited/restated by this change — squarely in this change's Impact. This is not a someday-list item.

**Blocks: ARCHIVAL.** Does not block executing the `cross → 0.0f` code edit itself, but the delta's WBal scenario text must be corrected (either to describe WBal honestly as scaling `widthSpread` alone, or the mechanism redesigned) before this spec-delta is promoted. Additionally: `stereo_delay_width_balance_mapping_keeps_cross_in_0_1_and_spread_at_or_below_todays_max` should be updated or retired as part of this change's own diff — it duplicates the exact formula being deleted from `Process()`, will keep "passing" forever post-change since it never calls `Process()`, and becomes a test that cannot fail (the class of defect flagged elsewhere in this codebase's own history).

## Finding 2 — Width-spread capacity overrun (the cited worked example, confirmed independently)

Promoted and delta spec (identical text): *"the time-offset spread this balance produces never lengthens a read tap beyond the delay buffer's own capacity."*

Traced: `baseSeconds = ExpMapCompute(0.001f, kMaxDelaySeconds=2.0f, p.dtim)`, so at `dtim=1`, `baseSeconds = 2.0f` exactly. `SetSampleRate`: `capacity = min(kMaxDelaySamples=96000, ceil(2.0*sampleRate))` — at 48 kHz this is exactly 96000 samples = 2.0 s, i.e. `baseSeconds`'s own maximum already consumes the *entire* buffer with zero headroom. `widthSpread = p.dwid * baseSeconds * 0.35f * widthBalance` reaches `1.0 * 2.0 * 0.35 * 1.0 = 0.7f` at `dwid=1, widthBalance=1` (today's default). `timeR = baseSeconds + modSeconds + widthSpread` therefore reaches `2.0 + 0.7 = 2.7s` at minimum (before any modulation), i.e. **0.7 s / 33600 samples beyond `capacity`**, purely from `widthSpread`, with `dmod=0`.

`ReadAt` does not clamp or fail on this: `idx0I = floorPos % capacityI` wraps modulo `capacity`, silently aliasing the read to a position that does **not** correspond to the requested delay time — it reads whatever sample happens to sit `(requested_samples mod capacity)` back, not "no such history." This is exactly the buffer-capacity violation the clause forbids.

No `Check:` line exists anywhere in either spec version for this clause (the whole "Delay bank holds fourteen parameters" scenario carries no `Check:` citations at all, unlike the Drive/Reverb scenarios), and the existing `stereo_delay_width_balance_mapping_...` test (above) never calls `ReadAt`/`Process`, so nothing has ever exercised this invariant.

**Verdict: FALSE. Case 1 — always false, promoted wrongly**, pre-existing, confirmed independently of the other auditor's flag.

**Not touched by this delta**: the change description states `widthSpread` is "untouched," which the code (still showing the original formula) corroborates — the cross removal is orthogonal to this bug. The task brief itself discloses a pending second task to repair `ReadAt`, "deliberately open" between clamping the read or bounding `widthSpread."

**§8.0 scope:** `Delay.hpp` and this spec paragraph are both directly in this change's Impact — this is not a someday-list item; the change's own framing (a second task within the same change) is the compliant way to sequence it, per §8.0's "lands where it stops being load-bearing."

**Blocks: ARCHIVAL**, not current execution — provided the second task actually lands, and the promoted text is re-verified true, before this spec-delta is archived. If the second task is deferred to a *separate future change* rather than a task inside this one, that would be a §8.0 violation (hygiene found inside a change's own touched files scheduled as a someday-list) and should be rejected outright; the brief's wording is ambiguous on this point and should be confirmed with whoever owns `tasks.md`.

**Interaction with the stereo-image requirement (untested presupposition):** the delta's added scenario "A stereo-image control widens across its whole travel" is honestly marked `REFUTED for the Delay page` / pending its own check "once it lands," which is correctly hedged — no violation there. But the delta's own diagnosis frames the fix purely as removing cross-feed re-correlation; it does not connect this to the capacity overrun above. If `dtim` and `dwid` are both driven high, `ReadAt`'s aliasing (Finding 2) could reintroduce a correlation anomaly independent of the cross-feed fix, at the top of the travel — meaning the eventual monotonic-decorrelation check for Delay **presupposes Finding 2 is also fixed**, not just the cross-feed. Whoever writes that check should sweep `dtim` to its own maximum concurrently with `dwid`, not only `dwid` alone at a fixed low `dtim`, or a passing check will not actually cover the region where these two defects interact.

## Unaffected Delay-bank clauses, briefly

Wet/dry, Send, Freeze, Reverse blend, Diffusion mechanism descriptions: none are touched by this delta's code diff (`cross` assignment only) or its own text diff (Delay-bank scenario is restated verbatim apart from the two issues above). Read against current `Delay.hpp`, nothing else contradicts them. **Case 3** for all, not examined further per the "confirm/refute other auditors" split of labor — no new evidence found that they are false.

## PREFLIGHT REJECTS

1. **Width Balance "ratio" clause is false today and is not corrected by this delta's own diff of the same code/spec it edits.** Fix: either restate the Delay-bank Width Balance clause to describe what the code actually does post-change (a single scalar on `widthSpread`, no cross-feed left to balance against), or redesign the mechanism so a "ratio" genuinely exists. Blocks archival.
2. **Stale self-test (`stereo_delay_width_balance_mapping_keeps_cross_in_0_1_and_spread_at_or_below_todays_max`) becomes a test that cannot fail once `cross` is hardcoded**, since it never calls `Process()` and re-derives the deleted formula independently. Update or retire it in this same change. Blocks archival (and arguably execution, since it is the direct object of this change's own diff).
3. **Buffer-capacity overrun clause is false today, uncovered by any check, and explicitly deferred to a second task.** Confirm that task is inside this change's own `tasks.md` (not a separate future change) before promoting the unchanged clause text at archival; re-verify the clause against the fix once it lands. Blocks archival, not current execution.
4. (Non-blocking, flagged for whoever writes the pending Delay stereo-width check) The eventual monotonic-decorrelation check should sweep `dtim` alongside `dwid`, since Finding 3's aliasing could defeat monotonicity at the high end of both knobs even after the cross-feed fix lands.


===== BLOCK 3 of 3 (timestamp: 2026-09-14T06:18:37.005Z) =====


## H1 — CONFIRM

Verified directly against `Delay.hpp`. `SetWidthBalance`'s doc comment reads: *"Because both new weights in Process() are `<literal> * widthBalance * p.dwid` and widthBalance never exceeds 1.0f... (a) cross-feed stays in [0,1]: max is 0.5*1.0*1.0 == 0.5..."* — a derivation that requires `cross` to still be `dwid*0.5*widthBalance`. Once `cross = 0.0f` (task 1.1b), that premise is gone: there is no "second weight," and the stated derivation of bound (a) describes arithmetic that no longer runs. Checked task text for coverage: 1.1b's own scope is narrowly "cross = 0.0f... decoupled from p.dwid and widthBalance," with no instruction to touch this comment; 1.2 is scoped to a judgment call on whether WBal's remaining behavior is a defect, not to comment upkeep; 1.5 is scoped to the file's top provenance header only. None of the three closes this. **Confirmed as written — no task owns it.**

## H2 — CONFIRM

Same file, the comment immediately above the `cross =` line: *"half of a width blend that widthBalance then scales further."* This is the same premise as H1 (a two-stage width blend that widthBalance scales), stated a second time at the call site. It fails for the identical reason and is covered by the identical (non-)coverage in 1.1b/1.2/1.5. **Confirmed — and this is the same root defect as H1 surfacing in two places, not two independent defects:** both comments assert the pre-repair two-weight coupling; a single edit removing or rewriting both, done as one unit when the `cross=` line changes, closes both.

## H3 — CONFIRM

Verified the rationale text: per-channel (not linked/max-driven) `wetLimiterL`/`wetLimiterR` is justified specifically because *"`dwid`'s cross-feed... already keeps L and R close to each other in practice, so the two channels rarely diverge enough for one limiter engaging alone to read as an image shift."* This repair's entire purpose is to make L/R decorrelate for real (that's what fixes the REFUTED stereo-widening scenario). Post-repair, `fbL`/`fbR` come from `ReadAt(timeL,...)`/`ReadAt(timeR,...)` with no cross-mix at all (`cross=0` makes `CrossFeedPair` an exact identity), so L and R are two independent taps of the same source at different delay times — free to diverge far more under transient material than the pre-repair, cross-fed pair ever could. I checked whether 1.1a/1.1b's own check would catch a resulting artifact: the tap point is post-limiter (`Process`'s returned `DelayWetPair`), but the assertions are `|corr|` and `|rmsL-rmsR|/(rmsL+rmsR)` over a **12000-sample steady-state measure window** — an aggregate. A brief per-channel limiter-engagement asymmetry at a transient onset (exactly the case one channel's tap catches a transient a few ms before the other's) can read as an audible image blip while barely moving a quarter-second RMS average or aggregate correlation. Nothing in 1.1a/1.1b/A.3 targets transient-resolved per-channel limiter divergence specifically (A.3's adversarial pass targets panning-instead-of-widening, a different failure shape). **Confirmed as a real, previously-untraced behavioural premise this change invalidates, with no task currently re-verifying it.**

## H4 — CONFIRM

Verified `stereo_delay_width_balance_mapping_keeps_cross_in_0_1_and_spread_at_or_below_todays_max` (`FroggersDspParityTests.cpp:6935`): it calls only `delay.SetWidthBalance(...)` and recomputes `cross`/`spread` locally from `delay.widthBalance`; it never calls `delay.Process(...)`. Task 1.4's enumeration criterion is explicitly "every `TEST_CASE` that reaches `StereoDelay::Process`" — this test does not reach `Process` by construction, so 1.4's sweep will never even consider it, let alone flag it as invalidated by the repair. **Confirmed: this is a criterion blind spot, not a missed hit within the criterion's own search space.**

## Note on H4 vs. my own Finding 2 — same test, two distinct, both-needed framings

Not the same defect — two levels of one root cause:

- **Mine (object-level):** the test itself is unfalsifiable *right now*, today, independent of any task-list wording — it duplicates the formula being deleted, so it can never disagree with production. The needed action is concrete: update or retire this one test.
- **H4 (process-level):** the *mechanism* meant to catch exactly this class of drift (task 1.4's enumeration) is scoped in a way that structurally cannot see it, or any other test shaped like it (present or future). The needed action is different: widen 1.4's criterion to also catch tests that reimplement a formula independently of `Process`.

An executor needs both. Fixing only the test (mine) resolves this instance but leaves 1.4 exactly as blind for the next occurrence — no mechanism protects the family, which is the specific failure the omni-rule's "a family that must stay in sync gets a check, not an enumeration" principle warns against. Fixing only the criterion (H4) without knowing this concrete instance exists risks widening it without a positive control: per §6.1, a widened criterion needs one known-bad case to prove it now catches something it didn't before, and this test is exactly that case. So: H4 supplies the durable fix, mine supplies the required positive control for validating that fix. Report both together to the executor, not either alone.

## Note on dtim-sweep: does one measurement cover Finding 2 and H3?

Independent mechanisms, correlated region, not one measurement:

- **Mechanism:** Finding 2 is a memory/indexing correctness failure — `ReadAt` aliases to the wrong sample once requested delay time exceeds `capacity`. H3 is an envelope-follower/limiter divergence — two independent limiters reacting differently to two independently-timed taps. Fixing one (e.g., clamping `widthSpread` per task 1.3) does nothing to bound the other; limiter divergence can occur at delay times well inside `capacity`, as soon as `timeL`/`timeR` differ enough to catch different transient content.
- **Shared driving parameter:** `widthSpread = dwid * baseSeconds * 0.35 * widthBalance`, and `baseSeconds` scales with `dtim`. Both concerns get worse as `dtim` rises, because both the overrun margin and the absolute L/R time-offset grow with `baseSeconds`. Task 1.1a's fixed `dtim = 0.3` sits far from where either manifests strongly.
- **Conclusion:** one **broadened grid** (sweeping `dtim` toward its maximum alongside `dwid`, not `dwid` alone at `dtim=0.3`) is a *shared precondition* for exposing either defect, but it is not sufficient by itself — each needs its own metric layered on top of that grid: a correctness/aliasing check (compare `ReadAt`'s returned sample against what an unbounded buffer would return, or verify capacity is never exceeded) for Finding 2, and a transient-resolved (not 12000-sample-aggregate) per-channel gain-reduction comparison for H3. Recommend both ride the same widened grid as two separate assertions, not one merged one.

## Do my own findings change?

Yes, on two points, after reading `tasks.md` (which I had not read before, and should have per §1 — the change's own task breakdown is exactly the artifact that resolves the ambiguity I flagged):

- **My Finding 2 (capacity overrun) is resolved on the open question I raised.** I asked whether "the second task" was inside this change or deferred to a future one. Task 1.3 confirms it is inside this change, scoped almost exactly as the brief described ("clamping... against bounding widthSpread... land whichever the measurement supports"). This satisfies §8.0. I withdraw the "confirm this is in-scope" ask; my finding narrows to a residual gap I hadn't previously named: no task in stage 3 explicitly ties 1.3's landed fix back to the Delay-bank scenario's capacity SHALL-clause text or gives it a `Check:` citation (the scenario currently carries none for any of its bullets). That should be added to 3.1–3.5's scope or reported as missing before archival.
- **My Finding 1 (Width Balance "ratio" mischaracterization) is already scoped by task 1.2**, more directly than I credited. 1.2 explicitly names the post-1.1b state ("widthBalance scales only widthSpread, the same term Stereo width itself already scales") and requires a judgment call on whether that is a second WYSIWYG defect — this is my finding, reframed as a design question rather than a spec-text audit. My algebraic result (the ratio `spread/cross = 0.7*baseSeconds` is invariant to `widthBalance` even in **today's shipped code**, before this change) sharpens 1.2's mandate: this was never a real ratio control, which 1.2's executor should treat as evidence, not rediscover from scratch. What remains unaddressed even by 1.2: task 2.2 updates the *manual/quick-dict* prose per 1.2's ruling, but no task instructs updating the **spec-delta's own Width Balance clause** in `specs/froggers-sheaf-parameter-model/spec.md` to match whatever 1.2 decides. That gap stands and should block archival (stage 3) unless 3.2/3.5 are read to cover it explicitly.

Everything else in my original report (slot layout, FbDr/FbTn/MdRt/Crsh, cross-feed [0,1] bound, the honestly-hedged stereo-image scenario) is unaffected by this exchange.
