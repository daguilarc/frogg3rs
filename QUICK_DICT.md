# frogg3rs Quick Dict

Terse parameter glossary for the current **Frogg3rs — Sheaf app** (`app/`). Full guide → [`MANUAL.md`](MANUAL.md).
Daisy Field hardware firmware (frozen, different parameter model entirely) → [`DAISY_MANUAL.md`](DAISY_MANUAL.md).

Six banks — Audio, Envelope, Filter, Drive, Delay, Reverb — 16 slots each: 14 page parameters (slots
0–13) in the order below, a bank-local Crispy (slot 14), and one shared global Crunchy (slot 15).

## Global

- **Crispy** (slot 14, per bank) — Fuego (bit-scramble) applied to that bank's own 14 parameters only; no-op at 0.
- **Crunchy** (slot 15, shared across all six banks) — Same fuego scramble applied to every parameter in every bank, and to every bank's own Crispy; no-op at 0.
- **Bank select** — Six buttons (Audio/Envelope/Filter/Drive/Delay/Reverb) choose which bank's slots are on screen; all six keep processing regardless.
- **Play / Stop** — Transport. No manual note-on: while running, the shared envelope gate auto-pulses open for the first half of every quarter note (at the BPM control's tempo) and closed for the second half. Stop forces a fast ~50 ms fade on all three voices and clears Delay/Reverb tails.
- **Modulation assign** — Click a parameter's encoder (or a bank's own Crispy) to open its 15-source depth view; each source has an independent signed depth (0 = off) and multiple non-zero sources sum. Sources: Random S&H 1–6, VCO1–3 Audio, VCO1–3 EF, Noise, External Audio, External Audio EF (the last two carry real signal only once an external input is connected — see MANUAL.md's Audio configuration section; until then they hold silent defaults, 0.5 and 0.0).

## Audio

- **VCO1 / VCO2 / VCO3** (slots 0–2) — Pitch, 20 Hz–5 kHz exponential; defaults 110/220/330 Hz.
- **Shape 1 / 2 / 3** (slots 3–5) — Per-VCO waveform morph, sine → saw → square.
- **Phase mod 1 / 2 / 3** (slots 6–8) — Per-VCO phase-mod depth from that VCO's own internal LFO; no cross-VCO coupling.
- **Ring mod 1 / 2 / 3** (slots 9–11) — Per-VCO ring mod against its own internal carrier (20 Hz–5 kHz); true zero at the floor.
- **PM rate** (slot 12) — One shared rate (2–20 Hz) for all three VCOs' phase-mod LFOs.
- **VCO balance** (slot 13) — Tilts mix emphasis VCO1 → VCO2 → VCO3; every VCO always keeps 10–80% of the mix.

## Envelope

- **Attack VCO1/2/3** (slots 0/4/8) — Time to rise to full level on gate-open, exponential, 1 ms–250 ms.
- **Decay VCO1/2/3** (slots 1/5/9) — Time to fall from Attack peak to Sustain level, exponential, 5 ms–1 s.
- **Sustain VCO1/2/3** (slots 2/6/10) — Held level while gate is open; floored at 25%, default full.
- **Release VCO1/2/3** (slots 3/7/11) — Time to fall to silence on gate-close, exponential, 5 ms–2.5 s.
- **Curve** (slot 12) — Reshapes all three voices' Attack/Decay/Release ramps from linear (default) to ease-in.
- **Grace** (slot 13) — Minimum-hold before a gate-close is honored (0–1 s); no-op at default 0.

## Filter

- **Comb offset** (slot 0) — Short pure delay ahead of the comb, 1–100 ms.
- **Peak freq** (slot 1) — Resonant peaking-EQ center frequency, 100 Hz–20 kHz.
- **Peak gain** (slot 2) — Peak boost height, up to +6 dB.
- **Peak Q** (slot 3) — Peak width/resonance.
- **Comb delay** (slot 4) — Comb filter pitch, 100 Hz–10 kHz.
- **Comb feedback** (slot 5) — Comb resonance, neutral at center, up to ±0.95 (always decays).
- **Comb LP** (slot 6) — Low-pass inside the comb feedback loop; darker when lower.
- **Comb/Peak** (slot 7) — Blend between peak and comb paths.
- **Scoop** (slot 8) — Depth of the resonant notch dip (0 = none, ~95% at max).
- **Topology** (slot 9) — Continuous morph, parallel (default) to series routing of comb into peak.
- **Scoop freq** (slot 10) — Notch's own center frequency, independent of Peak freq, 100 Hz–20 kHz.
- **Scoop width** (slot 11) — Notch's own Q, independent of Peak Q.
- **Comb drive** (slot 12) — Pre-gain into the comb's saturator, 0.25×–4×, unity at default.
- **Scoop depth** (slot 13) — How much of the notch is blended into the output; independent of Scoop's own height.

## Drive

- **Drive** (slot 0) — Input gain into the polynomial waveshaper, 1×–5×.
- **Shape** (slot 1) — Recomputes the waveshaper's coefficients; harmonic character.
- **SRR 1** (slot 2) — First sample-rate reducer stage.
- **SRR 2** (slot 3) — Second sample-rate reducer stage, in series after SRR 1.
- **XOR** (slot 4) — 8-bit XOR mask on the sample.
- **Bit depth** (slot 5) — How many low bits the digital reorganizer scrambles.
- **Fuzz** (slot 6) — Blend from sine-fold to tanh-style hard saturation.
- **Blend** (slot 7) — Dry/wet crossfade of the whole Drive chain.
- **Phase** (slot 8) — Allpass on the wet signal before Blend; silent effect at Blend 0.
- **Anti-alias brightness** (slot 9) — Oversampler anti-alias filter cutoff trim.
- **Link** (slot 10) — How strongly Drive amount skews Shape's coefficients.
- **Fold** (slot 11) — Sine-fold divisor, 1×–16×; lower folds harder.
- **Tone** (slot 12) — Low-pass at the end of the chain, ~800 Hz to bypass; bypass at default.
- **Waveshaper offset** (slot 13, `Bias`) — Small DC offset (±0.02) into the waveshaper, removed after; zero at default.

## Delay

- **Delay time** (slot 0) — Base delay length, ~1 ms–2 s.
- **Send** (slot 1) — Signal sent into the delay line; 0 = bypass.
- **Feedback** (slot 2) — Repeat feedback, capped below 98%.
- **Stereo width** (slot 3) — Cross-feed/time-spread between L/R taps.
- **Detune** (slot 4) — Pitches L/R repeats apart, up to ±50 cents.
- **Mod depth** (slot 5) — LFO wobble depth on delay time.
- **Wet mix** (slot 6) — Delay wet level continuing to Reverb.
- **Color** (slot 7) — Folds into/biases Detune.
- **Halo** (slot 8) — Folds into/biases Mod depth.
- **Feedback drive** (slot 9) — Pre-gain into the feedback saturator, 0.25×–4×, unity at default.
- **Feedback tone** (slot 10) — Low-pass inside the feedback loop, ~800 Hz to bypass; bypass at default.
- **Mod rate** (slot 11) — Delay-time LFO rate, 0.05–1.25 Hz.
- **Width balance** (slot 12) — Overall scalar on Stereo width's own spread; default reproduces original fixed behavior.
- **Crush** (slot 13) — Sample-rate reduction on the feedback tap only; off at default.

## Reverb

- **Wet/dry** (slot 0) — Reverb mix, capped at 70% wet.
- **Room size** (slot 1) — Both tank delay-line lengths.
- **Decay** (slot 2) — Tank feedback / tail length.
- **Pre-delay** (slot 3) — Time before input reaches the tank.
- **Damping** (slot 4) — HF loss in feedback; darker tail at higher knob.
- **Stereo width** (slot 5) — Spread between the tank's two taps.
- **Diffusion** (slot 6) — Cross-feed between the tank's two lines.
- **Mod depth** (slot 7) — Sinusoidal wow depth on the tank's read taps.
- **Hold** (slot 8) — Pushes tank feedback toward, never to, self-oscillation.
- **Mod rate** (slot 9) — Mod depth LFO rate, 0.07–1.75 Hz.
- **Tank drive** (slot 10) — Pre-gain into the tank's feedback saturator, 0.25×–4×, unity at default.
- **Grit** (slot 11) — Digital reorganizer (bit-scramble) on the tank's feedback taps; bypass at 0.
- **Tilt** (slot 12) — Bipolar tone shave on the final output around ~1 kHz; center = no change.
- **Tuned** (slot 13) — Static offset on the tank's delay lengths, ±300 samples; center = zero offset.
