# Proposal — `frogg3rs-vco-mod-source-fuego-parity` (ARCHIVED — INVESTIGATION ONLY, NO CHANGE MADE)

**This change was never implemented and was never preflighted for implementation.
It is archived as a closed investigation, not as delivered work.** It is based on
frogg3rs `main` at `4e9fb51`, worked read-only in the `transport-and-shift-ux`
worktree. No production file, test, or `MANUAL.md` was touched while writing it, and
none is touched by archiving it.

**Outcome.** The operator asked whether the six VCO-derived modulation sources receive
the fuegoized value of the knobs that drive them. A running probe (below) measured that
they do — one sample behind the audio path, on every sample after the first, never
un-fuegoized. Against the operator's own sharpened criterion for what would count as a
defect (a value the fuego seam has *never* touched), that one-sample lag is not a
defect, and the operator ruled it acceptable. No production code, test, or spec
requirement changes as a result of this investigation. This document and its probe's
output are kept because the trace and the measurement are worth having on record; the
change itself is archived unexecuted.

This draft supersedes the deleted `frogg3rs-vco-mod-source-envelope-tap-point` draft,
which was about a different question (post-envelope vs pre-envelope tapping for the VCO
audio-rate and envelope-follower modulation sources). That question is untouched here;
this draft is about a separate operator concern — whether the six VCO-derived modulation
sources receive the *fuegoized* value of the knobs that drive them, the same way the real
audio chain does.

## Why the operator raised this

Fuegoization (Crispy/Crunchy's bit-scramble, `MANUAL.md:75-93`) is what a knob's value
*becomes* as it leaves the knob, before it reaches anything downstream — the audio chain,
phase modulation, ring modulation, all of it. The operator's position, going in: the six
VCO-derived modulation sources (VCO1/2/3 Audio and VCO1/2/3 EF, slots 6–11 of the
15-source modulation grid) are fed from `FroggersAppCore`'s `vcoDrive()` lambda
(`app/FroggersAppCore.hpp:1180–1186`), which reads
`parameters_.PageParameter(...).CachedKnobValue(0)`, and `modulation_.Step()` runs
*before* `parameters_.ProcessSample()` in the same sample
(`app/FroggersAppCore.hpp:1241,1244`). The operator read that ordering as meaning these
six sources never see the fuegoized value at all, and asked for the audio and modulation
paths — envelope-follower sources included — to be served by one shared construct so they
cannot drift apart.

## What the trace and a running probe actually show

**The premise, checked against the sharpened criterion the operator later gave this
draft** ("a one-sample-stale value is fine; the only real defect would be a value the
fuego seam has *never* touched") **is false.** The six VCO-derived modulation sources DO
receive a fuegoized value on every sample after the first — one sample later than the
audio chain, never un-fuegoized.

**Why the ordering the operator flagged does not mean what it looks like it means.**
`Parameter::CachedKnobValue()`/`ReplaceCachedKnobValue()` (`External/Sheaf/projects/
synth/include/synth/ParameterModulation.hpp:478,487`) is ONE storage slot per parameter.
`FroggersParameterModel::ApplyFuegoSeam()` (`app/FroggersParameters.hpp:565–612`) is the
ONE place anything ever writes to it with a fuegoized value — it reads
`CachedKnobValue()`, transforms it through `FuegoStack::ApplyMusicalRow`/`ApplyGlobal`
(`app/dsp/Fuegoize.hpp:80–96`), and writes the result back into that same slot via
`ReplaceCachedKnobValue()` (`app/FroggersParameters.hpp:605-609`), confirmed by reading
both functions side by side — there is no second cached-value slot and no second write
path. `FroggersAppCore::RouteAudioSample()`'s `knob()`/`RoutedKnob()`
(`app/FroggersAppCore.hpp:1556-1557`) and `vcoDrive()`
(`app/FroggersAppCore.hpp:1180-1186`) both read that identical slot through the identical
accessor, `CachedKnobValue()`. **The two consumers are already single-sourced** — they are
not two paths that happen to agree today and could silently diverge tomorrow; they are one
accessor called from two call sites.

What the ordering (`modulation_.Step()` before `parameters_.ProcessSample()`, every
sample, `app/FroggersAppCore.hpp:1241,1244`) actually produces is a one-sample latency for
`vcoDrive()`'s read specifically, not an un-fuegoized read: `vcoDrive()` reads
`CachedKnobValue()` before this sample's `ApplyFuegoSeam()` has run, so it sees whatever
`ApplyFuegoSeam()` wrote on the *previous* sample. `RouteAudioSample()` reads the same
slot after this sample's `ApplyFuegoSeam()` has run, so it sees this sample's own value.
This ordering is not arbitrary: `modulation_.Step()` must run before
`parameters_.ProcessSample()` because `ProcessSample()`'s first action is
`group_->UpdateModValues()`, which routes this sample's modulation-source outputs into
every parameter's own modulation depth for this same sample — the mod sources have to
exist before the parameters that consume them are computed. The existing in-code comment
at `app/FroggersAppCore.hpp:1173-1179` already names this as "standard for any modulation
graph with a cycle," and the probe below confirms it is exactly a one-sample latency, not
a stale-forever value.

### The probe (run, not read)

Source: `/private/tmp/claude-501/-Users-diegoaguilar-canabal-Desktop/
fc01622f-2f10-4779-8ccb-9e7483b6620e/scratchpad/fuego_mod_probe.cpp` (compiled and run
from this worktree against the already-built `External/Sheaf/projects/synth/build/
libsynth.a`; deleted after this draft was written, per the instruction that a throwaway
probe does not ship). It uses the exact same bare-`ParameterManager` +
`FroggersParameterModel` convention `FroggersParameterModelTests.cpp`'s own
`fuego_seam_transform_reaches_cached_knob_value_matching_dsp_stack` test uses (no
Engine/SynthRig needed — `ApplyFuegoSeam()` only touches the `ParameterGroup` the model
itself owns), and reproduces `FroggersAppCore::ProcessBlock`'s own per-sample order
exactly: read `CachedKnobValue()` (what `vcoDrive()` would read this sample) → call
`model.ProcessSample()` (Phase1 writes raw → `ApplyFuegoSeam()` overwrites with
fuegoized → Phase2) → read `CachedKnobValue()` again (what `RouteAudioSample()`'s
`knob()` reads this same sample). Crispy/Crunchy are set to 0.35/0.60 — the same pair the
existing test already proved moves a 0.42 raw value to ~0.4749, large and obvious, not
rounding noise.

Literal output:

```
STARTUP  CachedKnobValue(before any ProcessSample)=0.308700  defaultValue-equivalent GetRaw=0.308700
SETTLED  raw page0=0.420000 crispy=0.350000 crunchy=0.599999
steady         modConsumed=0.474902  prevAudioConsumed=0.474902  rawThisSample=0.420000  expectedFuegoThisSample=0.474902  audioConsumed=0.474902
steady         modConsumed=0.474902  prevAudioConsumed=0.474902  rawThisSample=0.420000  expectedFuegoThisSample=0.474902  audioConsumed=0.474902
steady         modConsumed=0.474902  prevAudioConsumed=0.474902  rawThisSample=0.420000  expectedFuegoThisSample=0.474902  audioConsumed=0.474902
after-move     modConsumed=0.474902  prevAudioConsumed=0.474902  rawThisSample=0.420000  expectedFuegoThisSample=0.474902  audioConsumed=0.474902
   ... (raw does not move yet -- SceneCenter only reaches targetCenter_ via
        Compute(), gated on sampleIndex % 16 == 0, then currentCenter_ chases
        targetCenter_ at processLiteAlpha per sample -- 16 near-identical
        lines omitted, all with modConsumed == prevAudioConsumed exactly)
after-move     modConsumed=0.474902  prevAudioConsumed=0.474902  rawThisSample=0.416096  expectedFuegoThisSample=0.416096  audioConsumed=0.416096
after-move     modConsumed=0.416096  prevAudioConsumed=0.416096  rawThisSample=0.412672  expectedFuegoThisSample=0.396985  audioConsumed=0.396985
after-move     modConsumed=0.396985  prevAudioConsumed=0.396985  rawThisSample=0.409667  expectedFuegoThisSample=0.464569  audioConsumed=0.464569
after-move     modConsumed=0.464569  prevAudioConsumed=0.464569  rawThisSample=0.407031  expectedFuegoThisSample=0.387423  audioConsumed=0.387423
after-move     modConsumed=0.387423  prevAudioConsumed=0.387423  rawThisSample=0.404719  expectedFuegoThisSample=0.385111  audioConsumed=0.385111
after-move     modConsumed=0.385111  prevAudioConsumed=0.385111  rawThisSample=0.402690  expectedFuegoThisSample=0.445828  audioConsumed=0.445828
after-move     modConsumed=0.445828  prevAudioConsumed=0.445828  rawThisSample=0.400910  expectedFuegoThisSample=0.444048  audioConsumed=0.444048
after-move     modConsumed=0.444048  prevAudioConsumed=0.444048  rawThisSample=0.399349  expectedFuegoThisSample=0.497388  audioConsumed=0.497388
after-move     modConsumed=0.497388  prevAudioConsumed=0.497388  rawThisSample=0.397979  expectedFuegoThisSample=0.496018  audioConsumed=0.496018
after-move     modConsumed=0.496018  prevAudioConsumed=0.496018  rawThisSample=0.396777  expectedFuegoThisSample=0.494817  audioConsumed=0.494817
after-move     modConsumed=0.494817  prevAudioConsumed=0.494817  rawThisSample=0.395723  expectedFuegoThisSample=0.431017  audioConsumed=0.431017
   ... (transient continues moving every sample; every remaining line still
        satisfies modConsumed == prevAudioConsumed and audioConsumed ==
        expectedFuegoThisSample exactly, to the printed precision, through
        the full 40-sample run)
```

**Reading it:** at every single sample, in steady state and through the entire
knob-move transient (raw itself moving nonlinearly under the scramble, since
fuegoization is not monotonic near its own islands), two things hold without exception:
`audioConsumed == expectedFuegoThisSample` (the audio chain gets exactly this sample's
correct fuegoized value), and `modConsumed == prevAudioConsumed` (the modulation slate
gets exactly *last* sample's correct fuegoized value — never the raw value, never
anything the seam has not touched). This is outcome 2 of the three the operator's
sharpened criterion named: **fuegoized, one sample behind the audio path, and by the
operator's own stated criterion that is not a defect.**

**The one real exception, and how narrow it is.** The `STARTUP` line shows that before
this `FroggersParameterModel` instance's very first `ProcessSample()` call ever runs,
`CachedKnobValue()` returns the raw `defaultValue` — `ApplyFuegoSeam()` has not run yet,
so nothing has fuegoized it. In `FroggersAppCore`, `parameters_.ProcessSample()` runs
**unconditionally every sample**, regardless of transport state
(`app/FroggersAppCore.hpp:1244`, outside the `if (transportRunningNow)` block at
1229-1242, per that block's own comment at 1197-1202: "`parameters_.ProcessSample()`
below stays UNGATED on purpose"). `vcoDrive()` is only ever evaluated *inside* that same
`if (transportRunningNow)` block. So for `vcoDrive()` to observe a never-fuegoized value,
transport would have to already be running on the literal first `ProcessBlock` sample
this plugin instance ever processes — before `parameters_.ProcessSample()` has had a
single earlier call to fuegoize anything. Whether a host can actually deliver that (a
project that opens with transport already playing, feeding the plugin's very first
callback) is a reachability question this draft does not resolve; see Tasks. Even in that
case, the consequence is bounded to one sample's worth of six modulation sources reading
each `defaultValue` un-fuegoized instead of `ApplyFuegoSeam`'s output of it. Whether that
one sample is audibly different from the correct value depends on Crispy/Crunchy's own
state at that instant: `Fuegoize()` is an identity transform whenever its own `fuegKnob <=
0` (`app/dsp/Fuegoize.hpp:47-50`), so if Crispy/Crunchy have not yet been turned up when
that first sample runs, there is no difference to hear; if a host restores a saved session
with Crispy/Crunchy already nonzero and transport already running on the very first
callback, the one-sample difference could be as large as the scramble itself produces
anywhere else — not sustained, but not necessarily small either. This is not measured
here; it is named for the reachability task below to settle.

**Envelope-follower sources have the identical status, not a separate one.** Both source
families are computed from the *same* `vco1Raw`/`vco2Raw`/`vco3Raw` locals inside
`FroggersModulationSlate::Step()` (`app/FroggersModulation.hpp:418-430`):
`vco{1,2,3}_.Process(vco{n}.pitch01, vco{n}.shape01, vco{n}.phaseMod01, ...)` produces the
raw values, `NormalizeBipolarToUnit()` feeds the Audio sources, and
`dsp::VcoEnvelopeFollowers::Process(vco1Raw, vco2Raw, vco3Raw, efOut)` feeds the EF
sources from that identical input, two lines later. `vco{n}.pitch01/shape01/phaseMod01`
all come from the same `VcoDrive` struct `vcoDrive()` builds from `CachedKnobValue()`
(`app/FroggersAppCore.hpp:1180-1186`). There is no separate tap for the EF sources to
diverge through — whatever is true of the Audio sources' input is true of the EF sources'
input, verified by reading the same three lines the probe's steady-state/transient
numbers already exercise (the EF sources add their own further one-pole smoothing on top,
which is a separate, already-understood, out-of-scope behavior — see below).

## What is proposed

**No production change.** The premise that motivated one — that the modulation sources
receive a value the fuego seam has never touched — does not hold. `Parameter::
CachedKnobValue()`/`ReplaceCachedKnobValue()` already is the one shared construct: a
single write site (`ApplyFuegoSeam()`) and two read sites (`RouteAudioSample()`'s
`knob()`, `vcoDrive()`) that read the identical slot through the identical accessor.
Building a new shared type or wrapper to "single-source" two things that are already
single-sourced would add a layer with nothing to consolidate — there is no second
definition of "apply fuego to a knob value" anywhere in this codebase (checked: `grep -n
"ApplyMusicalRow\|ApplyGlobal" app/*.hpp` finds calls only inside `ApplyFuegoSeam()`
itself and its own test/parity files). This draft's answer to the operator's "should the
audio and modulation paths share one construct" question is: **they already do, and no
change is proposed here.**

The only open item is the narrow cold-start reachability question above, named as a
DETERMINE task, not an implementation task, because right now nothing establishes whether
it can happen on a real host or only in a synthetic harness.

## What is deliberately left alone

- **Envelope tap point** — whether the VCO Audio/EF modulation sources should read a
  post-envelope-gated value instead of the free-running mod-source VCOs' own raw output
  (the subject of the now-deleted `frogg3rs-vco-mod-source-envelope-tap-point` draft) is
  untouched. It is a different question from "is the value fuegoized" — the probe and
  trace above only establish what happens to the *fuegoization* of the knobs feeding the
  mod-source VCOs, not what those VCOs are otherwise built from. Not entangled: nothing in
  this draft's trace touches `VcoAdsrState`, `GatedVoices`, or the audio-chain envelope.
- **Oscillator phase alignment** between the mod-source VCOs and the real `audioVcos_` —
  untouched; this draft never reads or compares phase state.
- **Ring modulation** — the mod-source VCOs hold Ring Mod at a fixed `0.0f`
  (`app/FroggersModulation.hpp:415`, unrelated to this draft) and `vcoDrive()` never reads
  the `VcoSlotRole::RingMod` slot at all (`app/FroggersAppCore.hpp:1180-1186` only builds
  `Pitch`/`Shape`/`PhaseMod`) — not entangled with the fuego-parity question, since the
  question here is about knob values reaching a consumer, and Ring Mod is never one of the
  three knobs `vcoDrive()` reads.
- **The EF's own fixed 10 ms/50 ms time constants** — untouched, unrelated to whether
  their *input* is fuegoized.

## Capabilities

None modified — no code changes are proposed.

## Impact (of this draft itself)

- `openspec/changes/frogg3rs-vco-mod-source-fuego-parity/` (this directory) — new,
  replaces the deleted `frogg3rs-vco-mod-source-envelope-tap-point/` draft.
- No other file in the tree is touched by this draft.
- Files read to build the trace (all read-only): `app/FroggersAppCore.hpp`,
  `app/FroggersParameters.hpp`, `app/dsp/Fuegoize.hpp`, `app/FroggersModulation.hpp`,
  `app/FroggersParameterModelTests.cpp` (as the probe's structural template),
  `External/Sheaf/projects/synth/include/synth/ParameterModulation.hpp`,
  `External/Sheaf/projects/synth/src/ParameterModulation.cpp`, `MANUAL.md`.
- Probe compiled and run, then deleted: `/private/tmp/claude-501/
  -Users-diegoaguilar-canabal-Desktop/fc01622f-2f10-4779-8ccb-9e7483b6620e/scratchpad/
  fuego_mod_probe.cpp` (and its binary). Not part of this repo at any point.

## Delivery

None — no production code, test, or spec text is delivered by this change; it is
archived as a closed investigation. Its one open item, tasks.md's DETERMINE task on
cold-start reachability, was never executed and is not resolved by archiving this
change — it is left open, on the record, in case the operator wants it picked up later
as its own small DETERMINE-only task. Archiving this change is not a ruling on that
question either way.
