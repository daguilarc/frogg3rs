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
- **Freeze / Record** — Freeze is a latch: engaging it stops the transport but holds the envelope gate open and keeps the delay recirculating, so the instrument drones until Freeze releases or Play is pressed. Record arms on one press and stops on the next (refusing to arm while the transport is stopped), offering the capture as `YYYY-MM-DD.wav`, up to 30 minutes.
- **Modulation assign** — Click a parameter's encoder (or a bank's own Crispy) to open its 15-source depth view; each source has an independent signed depth (0 = off) and multiple non-zero sources sum. Sources: Random S&H 1–6, VCO1–3 Audio, VCO1–3 EF, Noise, External Audio, External Audio EF (the last two carry real signal only once an external input is connected — see MANUAL.md's Audio configuration section; until then they hold silent defaults, 0.5 and 0.0).

## Audio

- **VCO1 / VCO2 / VCO3** (slots 0–2) — Pitch, 20 Hz–5 kHz exponential; defaults 110/220/330 Hz.
- **Shape 1 / Shape 2 / Shape 3** (slots 3–5) — Per-VCO waveform morph, sine → saw → square.
- **Ph.mod 1 / Ph.mod 2 / Ph.mod 3** (slots 6–8) — Per-VCO phase-mod depth from that VCO's own internal LFO; no cross-VCO coupling.
- **Ringmod 1 / Ringmod 2 / Ringmod 3** (slots 9–11) — Per-VCO ring mod against its own internal carrier (20 Hz–5 kHz); true zero at the floor.
- **PM rate** (slot 12) — One shared rate (2–20 Hz) for all three VCOs' phase-mod LFOs.
- **VCO balance** (slot 13) — Tilts mix emphasis VCO1 → VCO2 → VCO3; every VCO always keeps 10–80% of the mix.

## Envelope

- **Attack VCO1 / Attack VCO2 / Attack VCO3** (`A1`/`A2`/`A3`, slots 0/4/8) — Time to rise to full level on gate-open, exponential, 1 ms–250 ms.
- **Decay VCO1 / Decay VCO2 / Decay VCO3** (`D1`/`D2`/`D3`, slots 1/5/9) — Time to fall from Attack peak to Sustain level, exponential, 5 ms–1 s.
- **Sustain VCO1 / Sustain VCO2 / Sustain VCO3** (`S1`/`S2`/`S3`, slots 2/6/10) — Held level while gate is open; floored at 25%, default full.
- **Release VCO1 / Release VCO2 / Release VCO3** (`R1`/`R2`/`R3`, slots 3/7/11) — Time to fall to silence on gate-close, exponential, 5 ms–2.5 s.
- **Curve** (slot 12) — Reshapes all three voices' Attack/Decay/Release ramps from linear (default) to ease-in.
- **Grace** (slot 13) — Minimum-hold before a gate-close is honored (0–1 s); no-op at default 0.

## Filter

- **Peak freq** (slot 0) — Resonant peaking-EQ center frequency, 100 Hz–20 kHz.
- **Peak gain** (slot 1) — Peak boost height, up to about +9.5 dB (3×).
- **Peak Q** (slot 2) — Peak width/resonance.
- **Comb offset** (slot 3) — Short pure delay ahead of the comb, 1–100 ms.
- **Comb delay** (slot 4) — Comb filter pitch, 100 Hz–10 kHz.
- **Comb feedback** (`Comb FB`, slot 5) — Comb resonance, neutral at center, up to ±0.95 (always decays).
- **Comb LP** (slot 6) — Low-pass inside the comb feedback loop; darker when lower.
- **Comb drive** (slot 7) — Pre-gain into the comb's saturator, 0.25×–4×, unity at default.
- **Scoop mix** (slot 8) — Blends a resonant notch into the shared input feeding both the comb and peak paths; 0 = notch absent.
- **Scoop freq** (slot 9) — Notch's own center frequency, independent of Peak freq, 100 Hz–20 kHz.
- **Scoop width** (slot 10) — Notch's own Q, independent of Peak Q.
- **Scoop depth** (slot 11) — How deep the notch dips (0 = none, ~95% at max); independent of Scoop mix.
- **Comb/Peak** (slot 12) — Blend between peak and comb paths.
- **Topology** (slot 13) — Continuous morph, parallel (default) to series routing of comb into peak.

## Drive

- **Wet/Dry** (slot 0) — Dry/wet crossfade of the whole Drive chain; not capped, reaches fully wet.
- **Gain** (slot 1) — Input gain into the polynomial waveshaper, 1×–5×.
- **Shape** (slot 2) — Recomputes the waveshaper's coefficients; harmonic character.
- **SRR 1** (slot 3) — First sample-rate reducer stage; raising the knob increases the reduction, off at the bottom.
- **SRR 2** (slot 4) — Second sample-rate reducer stage, in series after SRR 1; same off-at-the-bottom mapping.
- **XOR** (slot 5) — 8-bit XOR mask on the sample; the mid-travel plateau strips the low end and leaves the top.
- **Bit depth** (slot 6) — How many low bits the digital reorganizer scrambles.
- **Fuzz** (slot 7) — Blend from sine-fold to tanh-style hard saturation.
- **Phase** (slot 8) — Allpass on the wet signal before Wet/Dry; silent effect at Wet/Dry 0.
- **Anti-alias brightness** (`Anti-alias`, slot 9) — Crossfade between a clean oversampled shaper path and today's gritty one; top of travel is what shipped before.
- **Link** (slot 10) — How strongly Gain amount skews Shape's coefficients.
- **Fold** (slot 11) — Sine-fold divisor, 1×–16×; lower folds harder.
- **Tone** (slot 12) — Low-pass at the end of the chain, ~800 Hz to bypass; bypass at default.
- **Waveshaper offset** (`Bias`, slot 13) — Small DC offset (±0.02) into the waveshaper, removed after; zero at default.

## Delay

- **Wet/dry** (slot 0) — Delay wet level continuing to Reverb; dry signal floored at 30% of its own level, the same floor Reverb's Wet/dry shares.
- **Send** (slot 1) — Signal sent into the delay line; 0 = bypass.
- **Delay time** (slot 2) — Base delay length, ~1 ms–2 s.
- **Feedback** (slot 3) — Repeat feedback, capped below 98%.
- **Stereo width** (slot 4) — Cross-feed/time-spread between L/R taps.
- **Freeze** (slot 5) — Crossfades the feedback loop from the ordinary Feedback level toward full, lossless recirculation; at 0 the loop runs at the Feedback level.
- **Mod depth** (slot 6) — LFO wobble depth on delay time.
- **Reverse blend** (`Reverse`, slot 7) — Blends in a backward-travelling read of the delay line against the forward tap; 0 = off, 1 = fully reversed.
- **Diffusion** (slot 8) — Smears each repeat through a short allpass cascade; exact bypass at 0.
- **Feedback drive** (`FB drive`, slot 9) — Pre-gain into the feedback saturator, 0.25×–4×, unity at default.
- **Feedback tone** (`FB tone`, slot 10) — Low-pass inside the feedback loop, ~800 Hz to bypass; bypass at default.
- **Mod rate** (slot 11) — Delay-time LFO rate, 0.05–1.25 Hz.
- **Width balance** (`Width bal`, slot 12) — Overall scalar on Stereo width's own spread; default reproduces original fixed behavior.
- **Crush** (slot 13) — Sample-rate reduction on the feedback tap only; off at default.

## Reverb

- **Wet/dry** (slot 0) — Reverb mix, dry signal floored at 30% of its own level.
- **Send** (slot 1) — Signal sent into the reverb tank; 0 = bypass, same job as Delay's Send.
- **Room size** (slot 2) — Both tank delay-line lengths.
- **Decay** (slot 3) — Tank feedback / tail length.
- **Pre-delay** (slot 4) — Time before input reaches the tank, 0.02 ms to about 85 ms.
- **Damping** (slot 5) — low-pass on the tank output; darker AND quieter tail at higher knob.
- **Stereo width** (slot 6) — Spread between the tank's two taps.
- **Diffusion** (slot 7) — Cross-feed between the tank's two lines.
- **Mod** (slot 8) — Sinusoidal wow depth on the tank's read taps, fixed 0.35 Hz rate.
- **Hold** (slot 9) — Pushes tank feedback toward, never to, self-oscillation.
- **Tank drive** (slot 10) — Pre-gain into the tank's feedback saturator, 0.25×–4×, unity at default.
- **Grit** (slot 11) — Digital reorganizer (bit-scramble) on the tank's feedback taps; bypass at 0.
- **Tilt** (slot 12) — Bipolar tone shave on the final output around ~1 kHz; center = no change.
- **Tuned** (slot 13) — Static offset on the tank's delay lengths, ±300 samples; center = zero offset.
