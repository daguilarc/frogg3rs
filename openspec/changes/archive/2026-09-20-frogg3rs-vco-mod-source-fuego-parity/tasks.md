# Tasks — `frogg3rs-vco-mod-source-fuego-parity` (ARCHIVED — INVESTIGATION ONLY, NO CHANGE MADE)

This change implemented nothing and none of its tasks were executed. The behavioural
premise it was opened to check — that the six VCO-derived modulation sources never
receive a fuegoized knob value — was checked with a running probe (see `proposal.md`'s
"The probe" section) and found FALSE: they receive last sample's correctly fuegoized
value, every sample, without exception in the 4000-sample-settle-plus-40-sample-transient
run. The operator ruled that one-sample lag acceptable. There is no implementation task
here because there is no defect this investigation found to implement a fix for, and this
change is archived on that finding without any task below being carried out.

All three items below are left unchecked. Tasks 1 and 2 are DETERMINE/follow-on work
that never ran; archiving this change does not resolve them, and leaves the narrow
cold-start question (task 1) on record as unresolved, not silently dropped. Task 3 is
not a pending action — it records that this investigation found nothing to build — and
stays unchecked because no work under it was done or is needed; it is not a checkbox
this change left incomplete.

- [ ] 1. DETERMINE whether a host can deliver a `ProcessBlock` call with transport already
      running as the very first sample this plugin instance ever processes (before
      `parameters_.ProcessSample()` — and therefore `ApplyFuegoSeam()` — has run even
      once). `proposal.md`'s trace establishes this is the only sample on which
      `vcoDrive()` could read a truly un-fuegoized value (the plugin's raw `defaultValue`,
      not the previous sample's fuegoized cache); it does not establish whether real
      hosts ever present that ordering (e.g. a saved session reopened with transport
      already playing). Check: cite the specific host behaviour (JUCE's own
      `prepareToPlay`/`processBlock` contract, or an observed DAW's session-restore
      sequence) that settles whether this is reachable, not a guess about what "probably"
      happens; if it turns out to be reachable, this task's finding becomes the Why for a
      real follow-up proposal — a one-sample startup guard, scoped only to that first
      sample, is a plausible fix but is NOT proposed here since nothing yet establishes it
      is needed.
- [ ] 2. If task 1 finds the ordering is reachable AND that the resulting one-sample value
      can be audibly large (per `proposal.md`'s note on Crispy/Crunchy already being
      nonzero at that instant), write a new, separate proposal for the narrow fix —
      its own Impact list, its own tests (a test driving `FroggersAppCore`/`SynthRig`
      through exactly that ordering, proven to fail against today's code before any fix
      lands) — and take it through this project's normal preflight before any execution.
      If task 1 finds the ordering is unreachable, or reachable but inaudible, this
      item is closed with that finding recorded here and no follow-up proposal is
      written.
- [ ] 3. No task proposes building a shared construct for the audio and modulation paths.
      `proposal.md`'s trace already establishes `Parameter::CachedKnobValue()`/
      `ReplaceCachedKnobValue()` is that one shared construct today — one write site
      (`ApplyFuegoSeam()`), two read sites (`RouteAudioSample()`'s `knob()`, `vcoDrive()`)
      — so there is nothing to consolidate. If the operator has a different construct in
      mind (e.g. a typed accessor that makes "this is the fuegoized value" visible at
      each read site, rather than a bare `float` from `CachedKnobValue()`), that is a
      naming/readability preference, not a defect fix, and needs its own explicit ask
      before any task is written for it.
