# Validation record

## Reproducible repository checks

```
make -C External/Sheaf/projects/synth build/parameter_modulation_tests build/dsp_tests
External/Sheaf/projects/synth/build/parameter_modulation_tests
External/Sheaf/projects/synth/build/dsp_tests
make -C External/Sheaf/projects/synth formal-test   # needs elan; lean-toolchain pins Lean v4.32.0
make -C app test
```

`.github/workflows/parameter-model.yml` runs the first four on every commit that
changes the Sheaf pin. `make formal-test` also checks the exported coefficient
bits, the pinned theorem statements in `formal/requirements.json`, the absence
of `sorry`/`admit`/`native_decide`/`axiom` anywhere in the model, that every
contract cites audited proofs, and that the C++ settlement constants match the
Lean publication law (binary32 `2e-5f` = 1.99999995e-5 ≤ 1/50000; 4096 samples).

## Results (2026-11-11, final tree)

- Native parameter suite 375/375, including the source seam probes now spanning
  24–96 kHz (weakest: 40 kHz at −92.0 dB; 96 kHz at −115.4 dB) and the new
  per-voice/held/DC-start source history test.
- `make -C app test` exit 0 (482 passing cases, none failing), including the
  unchanged Freeze/Scoop assertions and the 200-draw Randomize All storm.
- Formal gate: 180 theorem declarations, 11 reviewed sources, axioms limited to
  propext, Classical.choice and Quot.sound.

## Independent review

Two fresh-context reviewers read the diff without producer notes.

- Spec versus code: the landed code matched the proposal; SRR1/2, Delay Crush
  and randomization files are untouched; static targets settled exactly in a
  200k-case sweep; nothing on the audio thread allocates or locks. Its
  findings (invalid spec delta, stale prior-change text, temporary-file
  citations) are fixed in this tree.
- Check attack: every gate stayed green under a 55–96 kHz source leak, shared
  source histories, history advancing while held, a zero-started source filter,
  a 50× settlement tolerance, weakened or narrowed Lean statements, unlisted
  `lemma`s, and no CI. Each is now caught: the probe set, the source history
  test, the settlement cross-check, statement pins, the widened enumeration and
  the CI workflow respectively. A one-ulp coefficient change is caught only by
  the formal gate, which CI now runs.

## Operator test

Diego listened to a clean desktop build of this tree on 2026-11-11, reported
that it works great, and approved merge and push.

## Performance limitations

Before the settlement guard, a headless paired diagnostic (O3, 168 live routes,
84 moving roots, 48 kHz / 256 frames) measured median whole-callback CPU 7–10%
above the previous converter, with wall-time tails past the 5.33 ms deadline for
both baseline and candidate. The final code was not re-timed; the operator's
listening test on the desktop build is the evidence that it runs, not a
guarantee for slower devices or smaller buffers. Android was not rebuilt.
