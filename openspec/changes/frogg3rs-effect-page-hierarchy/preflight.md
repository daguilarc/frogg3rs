# Preflight — `frogg3rs-effect-page-hierarchy`

Run inline (§9: large change — Impact names nine files across four
directories, the plan rests on behavioural premises throughout, and it moves
definition sites with ~150 call sites).

## Verdict: REJECT, on four findings. Three are mechanical. One is a false
## premise that a measurement settled against the proposal.

## What the proposal got right

Checked, not assumed:

- **Every file:line citation resolves.** `FroggersAppCore.hpp:1692-1713`,
  `Drive.hpp:690`, `:681`, `:180`, `:96`, `:115`, `:390`, `Reverb.hpp:388`,
  `:596`, `Delay.hpp:1061`, `FroggersAppCore.hpp:1601`,
  `FroggersParameters.hpp:76`, `DAISY_MANUAL.md:310-319` — all verified
  verbatim.
- **The measurement campaign reproduces.** Driving `dsp::FrogBlock` with every
  control at its floor or stated neutral, 220 Hz at 0.5, 48 kHz:

  | Fold knob | measured gain | measured THD | proposal's figure |
  | --- | --- | --- | --- |
  | 0.50 (today's default) | +3.25 dB | 2.673% | +3.25 dB, 2.67% |
  | 1.00 (proposed clean end) | -8.16 dB | 0.161% | 0.16% |
  | 0.00 (positive control) | +4.94 dB | 118.573% | 118.6% |

  The control spans 118% to 0.16% THD, so the instrument is live. The
  proposal's numbers are real.
- **Mod rate really is inert at Mod depth 0.** Not a structural guess:
  `modOffset = modDepthKnob01 * kModMaxOffsetSamples * Sine01(modLfoPhase)`
  (`app/dsp/Reverb.hpp:483`) is a literal multiplication by zero. The
  argument for freeing that slot holds.
- **Pre-delay really is 1..100 samples** (`Reverb.hpp:388-391`, then
  `round(preNorm * sampleRate)` at `:453`). 0.02-2.08 ms at 48 kHz, as claimed.
- **`dsp::BiquadDf1` exists** (`app/dsp/DspMath.hpp:137`), so "no new
  primitive" holds.
- **The counterintuitive alias claim is corroborated by the code.**
  `SetAntiAliasBrightness` sets the corner in *cycles/sample*
  (`Drive.hpp:180-183`), so the corner scales with the oversampled rate and
  never gets relatively steeper. That is exactly why raising the factor alone
  changes nothing, as the proposal measured.

## Finding 1 — BLOCKER. The fold normalization's safety claim is false.

The proposal states, of normalizing the sine fold:

> A scalar cannot change harmonic character [...] so this changes the page's
> level, never its sound's shape.

That holds only if nothing level-dependent follows the fold. Something does.
`FrogBlock::Process` (`app/dsp/Drive.hpp:474-487`) runs the fold *inside* the
oversampled lambda and then feeds `digitalReorganizer.Process` at the output
rate. `DigitalReorganizer::Mangle` maps the sample onto a **fixed absolute
8-bit grid** — `(input + 1.0f) * 128.0f`, rounded and clamped to 0..255 — and
XORs and scrambles bits of that code. Halve the level and a different set of
bit patterns is hit entirely.

Measured. A gain change ahead of the reorganizer, compared against the same
signal after removing the best-fit single scalar — so a pure level change
would read as a large negative number:

| reorganizer setting | residual after gain match | reading |
| --- | --- | --- |
| both manglers at their floors | **-140.5 dB** | level only (positive control) |
| XOR engaged (0.30) | **-6.0 dB** | reshaped |
| Bit depth engaged (0.75) | **-4.7 dB** | reshaped |
| both engaged | **-10.2 dB** | reshaped |

The floor case is the positive control: at the manglers' floors the stage is a
proven exact bypass, the instrument reports -140 dB, so it can register "level
only" when that is the truth. It does not, once a mangler is on. A residual
5-10 dB under the signal is not a subtlety; it is a different sound.

So tasks 3c.2 and 3c.3 change the character of the Drive page's signature
crush stages at every setting where they are engaged. The proposal's **goal**
survives — Fold 1.0 measures -8.16 dB and the normalization is +8.11 dB, so
the page really does land at unity and transparent at rest — but "nothing
else on the page moves" is wrong, and it is the sentence the operator would
have relied on.

This is a taste call, not an arithmetic one: whether a changed bit-crusher
character is an acceptable price for transparency at rest. It belongs to the
operator.

## Finding 2 — BLOCKER. Task 3.4 names a harness that does not exist.

> 3.4 Measure the block cost at the chosen factor against the app's own
> deadline harness

There is no deadline harness in this app. Established by searching for the
invocation across `app/` by operand — `deadline`, `benchmark`, `chrono`,
`steady_clock`, `high_resolution`, `elapsed` — the only hits are a date stamp
in `FroggersAppCore.hpp:561` and `std::this_thread::sleep_for` in the VST host
tests. The 96 kHz deadline tests that exist are Sheaf's, in a different tree.
Task 3.4 cannot be executed as written.

The gate it wants is very likely satisfiable. Timed here, best-of-three warm,
48 kHz in 128-sample blocks (budget 2.67 ms per block):

| path | ns/sample | share of budget |
| --- | --- | --- |
| Drive shaper today (internal 2x) | 21.5 | 0.10% |
| a 4x-shaped proxy + an 8th-order cascade | 25.7 | 0.12% |

The proxy is not the real implementation, so 1.19x is a floor on the true
ratio rather than the ratio. The usable conclusion is the magnitude: the Drive
shaper spends about a tenth of a percent of the audio budget, so doubling or
quadrupling it stays far inside the deadline. Cost is not the risk here; the
missing harness is.

(The first run of this measurement was **void, not negative** — the heavier
path timed *faster*, which is impossible for strictly more work. Cold start.
Re-run warm, the ordering became physical. The void run is not quoted.)

## Finding 3 — BLOCKER. The spec delta contradicts promoted requirements and
## carries no MODIFIED section.

The change's delta has only `## ADDED Requirements`. These already-promoted
requirements in `openspec/specs/froggers-sheaf-parameter-model/spec.md` say
the opposite of what the change ships:

| line | promoted text | what the change does |
| --- | --- | --- |
| `:105` | Drive "slots 0-8 are unchanged from the Drive bank's existing nine parameters" | moves Wet/Dry to slot 0, renumbering 0-8 |
| `:120` | Delay, same sentence | moves Wet/dry to 0 and Send to 1 |
| `:137` | Reverb, same sentence | inserts Send at slot 1 |
| `:138` | Reverb "slot 9 is Mod Rate (short name `MdRt`)" | collapses Mod Rate away |
| `:312` | Reverb at max wet, "the dry signal's contribution to this stage's output is at least 40%" | `kMaxWetMix` 0.6 -> 0.7 leaves 30% (later replaced by `dsp::kMinDryLevel = 0.30f`, applied to the equal-power crossfade's angle rather than the knob -- same 30% floor, different mechanism) |
| `:326`, `:341` | Delay's control called "Wet mix" | renamed "Wet/dry" |

`## MODIFIED Requirements` is established precedent in this repo
(`openspec/changes/archive/2026-07-28-frogg3rs-gui-and-dsp-robustness/` uses
it). Note that `openspec validate --strict` **passes** on the change as it
stands, so the tool will not catch this — archiving would leave the promoted
spec asserting a slot layout and a dry floor the shipped code contradicts.

## Finding 4 — BLOCKER. A default-patch overlay silently re-targets on the
## renumber.

`app/FroggersModulation.hpp:1461` hardcodes the default patch:

    model.PageParameter(FroggersBankId::Drive, 0).HandleSetAbsolute(pole, 0.2f);

with the comment above it reading "Drive (Drive bank, slot 0) = 20% of its
range". Today slot 0 is the gain. After the move, slot 0 is Wet/Dry — so the
instrument would ship its default patch **20% wet** rather than bypassed, and
Gain would silently fall back to its registered default.

Twenty percent lands inside the notch the proposal's own section 1 measures as
the worst part of that travel (-2.6 dB, thinner and quieter). The proposal's
"What Changes" list does not name this site, and its Impact section says the
default patch is "re-measured rather than assumed unchanged" without naming
what would move it.

## Mechanical defects in the artifacts

- `tasks.md` has two tasks numbered **0.2** (lines 9 and 16).
- `proposal.md`'s OPERATOR items are numbered **0, 2, 1, 2** — a duplicate and
  out of order (lines 533, 541, 548, 553).
- `proposal.md:531` says "Two calls left" above four items, all four ANSWERED.
- `tasks.md` group 3d jumps 3d.0 -> 3d.5, with no 3d.1-3d.4.

## Not blocking, but owed a decision

Task 3d.7 requires Reverb's mix to gain Delay's authority scaling as "one
shared definition, not a second copy". `WetAuthority()` is today a method on
`dsp::StereoDelay` reading that struct's own `wetLevel` follower
(`app/dsp/Delay.hpp:1053-1065`). Sharing it means extracting the follower into
a unit both stages own. Neither the proposal nor the task names where that
unit would live, so an executor cannot execute the step deterministically.

## §5 forward enumeration — concepts this change creates

| concept | already in the tree? | disposition |
| --- | --- | --- |
| Butterworth / cascaded decimation lowpass | none | genuinely new; `dsp::BiquadDf1` (`DspMath.hpp:137`) is the primitive to build it from, as planned |
| oversampling above 2x | none | `Oversampler2x` is fixed 2x and is the extension point, not a duplicate |
| a `Send` parameter | Delay has one (slot 1) | Reverb genuinely lacks one |
| wet-level authority scaling | Delay only (`Delay.hpp:1053-1065`) | Reverb genuinely lacks it; see the open decision below |
| a max-wet ceiling | **exists** — was `kMaxWetMix` (`FroggersAppCore.hpp:1601`), applied to Delay `:1909` and Reverb `:1973`; now `dsp::kMinDryLevel` (`app/dsp/Limiter.hpp`), applied inside `StereoDelay::ToStereo`/`Reverb::Process` to the equal-power crossfade's own angle | Drive's Blend is the only unfloored mix, exactly as the proposal states |
| unity-gain normalization of a waveshaper | none in `app/dsp/` | see Finding 1 |
| quantized-count knob mapping | none in `app/dsp/` | new |
| a knob map in ms rather than samples | only the `PreDelayNormFromKnob` bug | no sibling bugs; the other sampleRate-divided maps (`Vco.hpp`) are correctly unitted |

One correction to note against a subagent's report: it concluded no wet ceiling
exists anywhere. It does — verified above. Its other suggestion, reusing
`TanhSaturator<Normalize>` for the fold, is unusable: that type lives in
`src/core/`, the frozen firmware tree, and `app/dsp/DspMath.hpp:7` states this
layer must never include anything under `src/`.

## Undocumented parameter couplings found while enumerating

The proposal treats Reverb's Mod rate/Mod depth coupling as the argument for
collapsing that pair. The same shape exists elsewhere and is documented
nowhere: Audio's PM rate under PM depth, Filter's comb feedback under comb
drive, Delay's own Mod rate under Mod depth, and Delay's feedback tone/Crush
under Feedback. Not this change's business to fix, but if the collapse
argument is good for Reverb it is an open question why Delay keeps its own
identical pair — which this change moves without touching.

## §8.0 hygiene, over the directories the Impact names

Swept `app/`, `app/dsp/`, the root documents, and `openspec/`.

On this change's own path, so fixable inside it:

- **15 comments carrying planning history** in `app/` and `app/dsp/` —
  `Item N` / `ITEM N` labels, plus design-section codes like `F.2b` in
  `FroggersSurfaceTests.cpp`. Two sit in files this change edits:
  `app/dsp/Drive.hpp:678` (`// Item 3 fix:`) and
  `app/FroggersAppCore.hpp:1695` (`// D2 (Drive slot 10, "Link")`). The
  standing rule is that comments explain behaviour, never planning history.
- `openspec/specs/field-button-input-latency/spec.md:52` cites `src/FroggersTiga`;
  the real directories are `src/FroggersSolo` and `src/FroggersGuitar`.

Found but outside the Impact's directories — reported, not silently folded in:

- `.git/hooks/pre-commit` is a no-op: it guards on `sim/check_operator_docs_sync.sh`
  and `sim/` does not exist in this tree.
- `package.json:2` still names the project `"froggers-tiga"`.
- `app/.gitignore:4` cites the same stale path and a dangling `tasks.md 12.4`.
- `node-v22.16.0-darwin-arm64/` (178 MB) is untracked and referenced nowhere.
- `build/manifest/` is orphaned session scratch, and
  `build/manifest/froggers-v2-manifest-report.md` documents a Sequencer the app
  no longer ships.
- `openspec/.sessions/marbles-mod-led-level-meter-progress.md` is a progress log
  orphaned from an already-archived change.

## Other active changes

No collision. The three other frogg3rs changes are delivered to `main` and
touch unrelated regions of the one shared file. Sheaf's ten active changes and
seven open PRs are runtime/UI/MIDI plumbing, not DSP parameter work. There are
no open PRs on frogg3rs.

---

## Resolution, 2026-09-10

All four findings closed in the artifacts before execution.

1. **The fold.** Withdrawn, by §5 rather than by taste. Section 7's argument
   held the Drive page to a property neither sibling has; the family's actual
   rule is that the master returns dry exactly at its floor, which Blend
   already does at -240 dB. `app/dsp/Drive.hpp`'s fold, Fold's default and the
   fold's parity pin are untouched, so the crush stages do not move. Section 7
   rewritten, the two "What Changes" bullets replaced with the withdrawal and
   its measurement, task group 4 records it.
2. **The cost gate.** Task 3.4 restated: measure the real 4x path and record
   the figure, no harness added, no gate. §7 — a gate defending against an
   overrun traced to be impossible is a branch to remove.
3. **The spec delta.** `## MODIFIED Requirements` added, carrying the corrected
   "One sixteen-slot bank per Froggers page" and "A wet/dry control cannot
   remove the instrument" requirements: the three banks' slot layouts, Reverb's
   slot 9, the dry floor at 30%, and Delay's control renamed throughout.
4. **The default-patch overlay.** Named in Impact and given task 5.2, with a
   scenario of its own in the delta, since neither check script reaches it.

Also fixed: duplicate task ids, OPERATOR items renumbered 1-5, the "two calls
left" line above four answered items, and the 3d numbering gap.

Per §9, this pass rewrote the proposal, which means the proposal is now
unaudited. Postflight runs in a fresh context and reads the revision, not the
draft it replaced.

---

# Preflight, second pass — 2026-09-11, fresh context

The pass above ran INLINE, in the context that wrote the proposal, and says so
in its own header. §9 requires a context that did not write the document, and
its closing line records that rewriting the proposal left the revision
unaudited. This pass is that missing one: a session opened by the operator,
which read the artifacts for the first time.

Scope: the work still outstanding (`tasks.md` groups 9.2, 10.2, 12 and 13) plus
every claim the artifacts make about behaviour that the outstanding work rests
on. The already-implemented groups are postflight's subject, not this pass's.

## Verdict: REJECT group 12 entirely, on measurement. Five findings.

All five are one defect wearing five faces: the deficit group 12 exists to fix
was measured on a quantity other than the one it is named after, and the
structure it proposes to fix was read backwards.

## Finding 1 — BLOCKER. Group 12's deficit table does not reproduce, because
## it does not measure the wet leg.

Task 12.1 states:

> Damping moves the wet leg's broadband level 6.4 dB across its travel
> (-9.15 / -10.59 / -7.72 / -5.44 / -4.21 dB at knob 0 / .25 / .5 / .75 / 1.0)

Measured here, `rv.wetL`/`rv.wetR` — the actual wet leg, the signal the
crossfade's `wetGain` multiplies (`app/dsp/Reverb.hpp:734-735`) — against the
input RMS that fed it, Reverb at its registered defaults with Send open,
48 kHz, one second after a quarter-second settle:

| source | damp 0.00 | 0.25 | 0.50 | 0.75 | 1.00 | spread |
| --- | --- | --- | --- | --- | --- | --- |
| white noise | -8.67 | -11.19 | -13.70 | -16.21 | -18.64 | **9.98 dB** |
| 440 Hz sine | +1.21 | +1.04 | +0.51 | -0.85 | -3.53 | 4.74 dB |
| 110 Hz sine | -2.01 | -2.03 | -2.06 | -2.18 | -2.53 | **0.52 dB** |
| plucked | -10.88 | -11.83 | -13.66 | -15.93 | -18.08 | 7.20 dB |

Strictly monotonic, and 2.50 dB per step on noise — which is exactly
`10*log10` of the alpha map's own ratio per step, `DampAlphaFromKnob` being
`ExpMapCompute(0.02, 0.2, 1 - knob)` (`Reverb.hpp:454`). A one-pole passes
white-noise power `a/(2-a)`; the measurement is the textbook figure.

The task's table is neither monotonic nor of that size. Its shape is
reproduced exactly by measuring something else — **the stage's full mixed
output at Wet/dry maximum, with the dry floor still summed in**:

| source | damp 0.00 | 0.25 | 0.50 | 0.75 | 1.00 |
| --- | --- | --- | --- | --- | --- |
| full output, noise, mix 1.0 | -7.37 | -8.47 | -9.23 | -8.30 | **-4.20** |
| task 12.1's table | -9.15 | -10.59 | -7.72 | -5.44 | **-4.21** |

Same non-monotonic shape, and the last cell agrees to a hundredth of a dB.
The quantity group 12 is built on is the output of a crossfade that still has
`kMinDryLevel` worth of dry signal in it, labelled as the wet leg. That is why
it reads as a hump rather than a slope: the dry floor's contribution sums
against a wet path whose correlation with it changes across the sweep.

POSITIVE CONTROL for this instrument, same rig, same run: Decay moves the same
figure 6.94 dB (-9.95 to -3.01), while Diffusion moves it 0.04 dB and Room size
0.03 dB across full travel. The proposal's own claim that those two move it
"under half a decibel" reproduces exactly. The instrument separates a control
that moves this quantity from one that does not.

## Finding 2 — BLOCKER. Damping's registered default is 0.0, not 0.4.

Task 12.1: "Damping's registered default of 0.4 sits near the worst point."

`app/FroggersParameters.hpp:277-289` registers the Reverb bank. `{"Damping",
"Damp"}` carries no third field, so it takes `FroggersParamSpec`'s ordinary
`0.0f` (`:84-86`). Only Tank drive, Grit, Tilt and Tuned carry explicit
defaults in that bank, and there is no Reverb overlay: `FroggersModulation.hpp`
defines `ApplyAudioBankOverlay` and `ApplyDriveBankOverlay` and no third
(`:1446`, `:1464`, applied at `:1486-1488`).

The default is knob 0, which Finding 1's table shows is the LOUDEST point of
the travel, not a point near the worst. The sentence inverts the fact it is
built on.

## Finding 3 — BLOCKER. The two filters are swapped. Reverb's damping is
## outside the feedback loop; Delay's tone is inside it.

`proposal.md` §11: "The damping filter sits inside the feedback path, so its
loss compounds per circulation."

It does not. `Reverb::Process` reads the tank at `Reverb.hpp:592-593`
(`valA`/`valB`), forms the feedback taps from those raw reads at `:604-605`
(`aFb`/`bFb`), and writes them back at `:667-668`. The damping filter runs at
`:669-671`, on `valA`/`valB`, producing `aOut`/`bOut`, which go only to the
width mix and out. Nothing damped is ever fed back. It is a static one-pole on
the tank's output — a tone shave, applied once.

The description is accurate about the OTHER page. `StereoDelay`'s `fbToneL/R`
run at `Delay.hpp:894-895`, on `fbL`/`fbR`, the feedback taps themselves, and
that file's own comment states it: "This one sits inside the feedback loop, so
its filter is applied on every pass and its darkening compounds across
repeats" (`:752-754`).

This is not a wording slip. It is the premise task 12.2 inherits, and it
inverts that task's own safety rule.

## Finding 4 — BLOCKER. No static term derived from the coefficient can meet
## task 12.1's pass criterion, and every one breaks a case that works today.

Task 12.1 asks for a STATIC compensation derived from the filter's own
coefficient, applied to the tank's wet output, with the criterion "level flat
within about 1.5 dB across Damping's travel".

The compensation such a term can express is `g(a) = (P(a_ref)/P(a))^(e/2)` with
`P(a) = a/(2-a)`, the one-pole's white-noise power gain, normalised so the
registered default stays bit-identical. Spread across Damping's travel after
applying it, same rig as Finding 1:

| law | noise | 440 Hz | 110 Hz | plucked |
| --- | --- | --- | --- | --- |
| none (ships today) | 9.98 | 4.74 | **0.52** | 7.20 |
| e = 1.00 (full power) | **0.44** | 5.82 | 9.90 | 3.21 |
| e = 0.75 | 2.17 | 3.85 | 7.29 | **1.21** |
| e = 0.50 | 4.77 | 1.96 | 4.69 | 2.40 |

No law meets 1.5 dB on more than one source, and none meets it on two at once.
Worse, the 110 Hz column: that source is nearly flat TODAY at 0.52 dB, and
every compensation makes it four to nineteen times worse. The fix would take a
case that already satisfies its own pass criterion and break it.

The reason is structural, not a matter of picking a better exponent. A static
scalar cannot undo a frequency-dependent gain, and Damping's loss is
frequency-dependent by construction — it is a lowpass with a corner sweeping
1704 Hz down to 154 Hz (`alpha` 0.2 to 0.02 at 48 kHz). How much level it
removes depends entirely on where the signal's energy sits relative to that
corner, which is why the same sweep costs 9.98 dB on noise and 0.52 dB on a
110 Hz tone. The only compensations that track it are signal-dependent, and
task 11.3 already measured a level follower and rejected it for audible
breathing.

## Finding 5 — BLOCKER. Task 12.2 is forbidden by task 12.1's own rule, once
## Finding 3's structure is read the right way round.

Task 12.1 states the constraint: compensate "downstream of the recursion --
never inside the feedback path, where the loop gain is the decay time and a
change there alters RT60 and can destabilise the tank." That is correct.

Task 12.2 then asks for "the same treatment for Delay's `SetFeedbackTone`".
Delay's tone filter IS inside the feedback path (Finding 3). A makeup gain
there changes the loop gain, which is the delay's decay time, and this is the
loop whose unbounded version this codebase already had to fix with a saturator.
Putting the gain outside the loop instead compensates nothing, because the loss
it is meant to answer compounds per repeat while a static output gain does not.

The third call site, Drive, is named in the task as one whose "term computes to
about zero and its output must be proven not to move" — a call site that does
nothing, added to a shared helper to make it shared. §4's 2-of-4 is not met by
a use that is provably inert.

## Resolution of group 12

**12.1 and 12.2 are WITHDRAWN on measurement**, and recorded the way groups 4
and 11.3 are: the measurement that killed them stays, so the next reader does
not re-propose them.

The withdrawal has a positive statement behind it, not just a failure. Damping
removes broadband energy because it is a lowpass, and this change already
records that exact reason as grounds for excluding a whole bank — task 12.2's
own text excludes the Filter bank because "removing spectral energy is a
filter's entire function." Damping is a filter. What the measurement adds is
that its level change is not a fixed cost to be cancelled but a
signal-dependent one, small on the low content a reverb tail is mostly made of
(0.52 dB at 110 Hz) and large only on broadband material.

What the operator loses time to is therefore not compensated, it is
DOCUMENTED — which is the answer §11 already took from Valhalla and then did
not apply here: "The level change is explained to the player and a control that
adds gain is pointed at, rather than hidden by a makeup stage." Reverb now has
exactly such a control, `Send` at slot 1, added by this same change.

## 12.3 — the dry floor: measured, and the floor STAYS

12.3 asked for the full-wet level at `kMinDryLevel` 0.30 against 0.0, on the
grounds that the floor "exists because the pages went quiet at the wet end,
which is now known to have been the linear crossfade plus a Damping default
parked near its worst point."

The second half of that is false twice over: the default is 0.0 (Finding 2),
and it sits at the loudest end of the travel (Finding 1), not near the worst.

Measured anyway, by building `app/dsp/` twice with the constant at 0.30 and at
0.0. Reverb at its registered defaults, Send open, Wet/dry at maximum, output
against the dry input:

| source | floor 0.30 (ships) | floor 0.0 | cost of removing the floor |
| --- | --- | --- | --- |
| white noise | -7.37 dB | -9.93 dB | 2.56 dB quieter |
| 440 Hz sine | -0.88 dB | -0.04 dB | 0.84 dB louder |
| plucked | -1.97 dB | -3.14 dB | 1.17 dB quieter |

So the level argument either way is small, and on a sine it points the other
way. That is the whole of what a measurement can settle here.

What it does not settle is the ruling itself, and the ruling is not a level
threshold. `kMinDryLevel`'s own comment records it verbatim: operator
2026-07-29, "clamp the reverb wetness down, it's too fucking quiet", tightened
2026-08-26. The same comment identifies the character half — "the linear law
cancelled the dry signal away, leaving behind a quiet, uncorrelated wet path" —
and removing the floor restores exactly that condition, since at mix 1.0 with
no floor the dry signal is gone and only the uncorrelated wet path remains.
The equal-power law fixed the level sag; it did not put a dry signal back at
the top of the travel, because nothing can except a floor.

And the question was already put and already answered inside this change:
OPERATOR item 1, ANSWERED 2026-09-10, "the cap stays on Delay and Reverb only."
Eleven days is not a reason to re-ask, and the argument offered for re-asking
is the one Findings 1 and 2 just refuted.

**The floor stays at 0.30.** The numbers above are recorded so the operator can
reopen it on a listen if they want to; nothing here is a reason to.

## Finding 6 — the proposal asserts the opposite of what it ships

`proposal.md` §11, closing the Valhalla passage:

> Across all three products the level question is answered the same way: with a
> control the player can reach. Never with a hidden makeup stage, and never by
> capping how wet the mix can go. That is why this change carries neither.

This change carries `dsp::kMinDryLevel = 0.30f`, which is a cap on how wet the
mix can go, described as exactly that everywhere else in the same document —
OPERATOR item 1, the What Changes list, the Impact section and the spec delta.
The sentence is a leftover from the draft in which the Damping compensation was
the answer instead. Corrected in the revision: the change carries no hidden
makeup stage, and it does carry a dry floor, for the recorded reason.

## What this pass checked and found sound

- The equal-power crossfade's constant-power claim: `0.300^2 + sin(acos(0.300))^2`
  is 1.0, as `kMinDryLevel`'s comment states.
- Diffusion and Room size move the wet leg under half a decibel, as §11 claims
  (0.04 and 0.03 dB measured).
- `WetAuthorityFollower` saturates at `kWetAuthorityFullLevel` 0.05
  (`Limiter.hpp:161`), so it is not silently normalising the levels measured
  above; every figure here was taken with Authority at 1.0.
- Reverb's damping filter is one shared instance processing both channels in
  sequence, which is the faithful port (`Reverb.hpp:213-216`), not a defect
  introduced here.
