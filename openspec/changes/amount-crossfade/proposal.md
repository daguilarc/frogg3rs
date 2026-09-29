# Proposal — `amount-crossfade`

## Why

A player modulating Crispy or Crunchy with the knob turned fully up hears
nothing and sees no modulation band: the one-way law delivered in
`one-way-amount-modulation` only adds on top of the knob and clamps at `1.0`,
so at knob `1.0` there is no room left (operator, 2026-09-29: "the modulation
doing nothing when the knob is at 1.0 is fucking idiotic"). The operator ruled
that Crispy and Crunchy use the crossfade instead.

## The law

For a `kOneWayAmount` target (Crispy in all six banks, and Crunchy):

`value = (1 − W)·knob + Σ d·u` when `W = Σ d ≤ 1`, and `value = Σ (d / W)·u`
when `W > 1`, where each depth `d` is the one-way depth (off to full, `0..1`,
`OneWayModulationDepthTargetFromKnob`), a route whose source is registered
`connected == false` has `d = 0`, and `u` is the source's stored value. This
is Sheaf's existing `kCrossfade` computation in `Parameter::ComputeAtDepth`,
restricted to non-negative depths. Modulation is heard at every knob position,
and the result never leaves `[0, 1]`. Real (sound) parameters keep the
attenuverter. The depth floor (`EnforceOneWayAmountFloor`) and the Randomize
remap are unchanged.

Worked values (the tests assert these):

- knob `0.3`, `d 0.6`, `u 0.8`: `0.4·0.3 + 0.48 = 0.60`; same route with its
  source disconnected: `0.3`.
- knob `0.3`, `d 0.5`/`u 1.0` and `d 0.3`/`u 0.0`: `0.2·0.3 + 0.5 = 0.56`.
- knob `0.2`, `d 0.5`/`u 1.0` and `d 0.4`/`u 1.0`: `W = 0.9`,
  `0.1·0.2 + 0.9 = 0.92`, depths stay `0.5` and `0.4`.
- knob `0.2`, `d 0.8`/`u 1.0` and `d 0.6`/`u 0.0`: `W = 1.4`,
  `0.8/1.4 = 0.5714286`, depths `0.5714286` and `0.4285714`.
- a `restsAtZero` route, knob `0`, `d 1.0`, `u 0.3`: `0.3`.
- knob `1.0`, `d 0.25`: `u 0` gives `0.75`, `u 1` gives `1.0`; the published
  min/max are `0.75`/`1.0` (the add-only law publishes `1.0`/`1.0`).
- frogg3rs: Crispy knob `0.1`, full depth, External Audio sample `0.6`
  (stored `0.8`): `0.8`; Crunchy knob `0.4`, full depth, connected silent
  External Audio (stored `0.5`): `0.5`.

## What changes

- Sheaf `Parameter::ComputeAtDepth`: a `kOneWayAmount` target skips the
  `kAttenuverter` block; its disconnected routes' depths are set to `0`; it
  then runs the existing crossfade code unchanged.
- Sheaf `projects/synth/tests/parameter_modulation_tests.cpp` and frogg3rs
  `app/FroggersModulationTests.cpp`: the expected values above.
- The promoted specs that state the add-only law (Sheaf
  `synth-parameter-modulation`, frogg3rs `froggers-sheaf-parameter-model`),
  and `MANUAL.md`'s sentence on Crispy/Crunchy modulation.

## Delivery

Sheaf commits on branch `attenuverter-blend-mode` (jvictor0/Sheaf#24);
frogg3rs `main` fast-forwarded; then `frogg3rs-android-app` is rebased onto
it before any further Android build.
