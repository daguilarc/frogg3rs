# frogg3rs Quick Dict

Terse parameter glossary for the current **Frogg3rs — Sheaf app** (`app/`). Full guide → [`MANUAL.md`](MANUAL.md).
Daisy Field hardware firmware (frozen, different parameter model entirely) → [`DAISY_MANUAL.md`](DAISY_MANUAL.md).

Six pages — Audio, Envelope, Drive, Filter, Delay, Reverb — 16 slots each: 14 page parameters (slots
0–13) in the order below, a page-local Crispy (slot 14), and one shared global Crunchy (slot 15).

## Global

- **Crispy** (slot 14, per page) — Fuego (bit-scramble) applied to that page's own 14 parameters only; no-op at 0.
- **Crunchy** (slot 15, shared across all six pages) — Same fuego scramble applied to every parameter in every page, and to every page's own Crispy; no-op at 0.
- **Page select** — Six buttons (Audio/Envelope/Drive/Filter/Delay/Reverb) choose which page's slots are on screen; all six keep processing regardless.
- **Play / Stop** — Transport. No manual note-on: while running, the shared envelope gate auto-pulses open for the first half of every quarter note (at the BPM control's tempo) and closed for the second half. Stop forces a fast ~50 ms fade on all three voices and clears Delay/Reverb tails. Play's plate shows held colours while the transport runs.
- **Freeze / Record** — Freeze is a latch: engaging it stops the transport but holds the envelope gate open and keeps the delay recirculating, so the instrument drones while frozen. Releasing Freeze resumes the transport if it was running when Freeze engaged, the same as pressing Play; if the transport was already stopped, releasing just silences the drone. Record arms on one press and stops on the next (refusing to arm while the transport is stopped), offering the capture as `YYYY-MM-DD.wav`, up to 30 minutes.
- **Modulation assign** — Click a parameter's encoder (or a page's own Crispy) to open its 15-source depth view; each source has an independent signed depth (0 = off) and multiple non-zero sources sum. Sources: Random S&H 1–6, VCO1–3 Audio, VCO1–3 EF, Noise, External Audio, External Audio EF (the last two carry real signal only once an external input is connected — see MANUAL.md's Audio configuration section; until then they hold silent defaults, 0.5 and 0.0).

## Audio

- **VCO1 / VCO2 / VCO3** (slots 0–2) — Pitch, continuous 27.5 Hz–1.76 kHz (A0–A6) exponential; defaults 110/220/330 Hz.
- **Shape 1 / Shape 2 / Shape 3** (slots 3–5) — Per-VCO waveform morph, sine → saw → square.
- **Ph.mod 1 / Ph.mod 2 / Ph.mod 3** (slots 6–8) — Per-VCO phase-mod depth from that VCO's own fixed 2 Hz LFO; no cross-VCO coupling.
- **Ringmod 1 / Ringmod 2 / Ringmod 3** (slots 9–11) — Per-VCO ring mod against its own internal carrier (20 Hz–5 kHz); true zero at the floor.
- **Balance Y** (slot 12) — VCO3 share of the triangle mix; both Balance centres give equal thirds.
- **Balance X** (slot 13) — VCO1/VCO2 crossfade within the remaining mix; corners can solo and there is no floor.

## Envelope

- **Attack VCO1 / Attack VCO2 / Attack VCO3** (`A1`/`A2`/`A3`, slots 0/4/8) — Time to rise to full level on gate-open, exponential, 1 ms–250 ms.
- **Decay VCO1 / Decay VCO2 / Decay VCO3** (`D1`/`D2`/`D3`, slots 1/5/9) — Time to fall from Attack peak to Sustain level, exponential, 5 ms–1 s.
- **Sustain VCO1 / Sustain VCO2 / Sustain VCO3** (`S1`/`S2`/`S3`, slots 2/6/10) — Held level while gate is open; floored at 25%, default full.
- **Release VCO1 / Release VCO2 / Release VCO3** (`R1`/`R2`/`R3`, slots 3/7/11) — Time to fall to silence on gate-close, exponential, 5 ms–2.5 s.
- **Curve** (slot 12) — Reshapes all three voices' Attack/Decay/Release ramps from a straight line (default) toward the analog shape: fast start, flattening into the target.
- **Grace** (slot 13) — Minimum-hold before a gate-close is honored (0–1 s); no-op at default 0.

## Drive

- **Wet/Dry** (slot 0) — Dry/wet crossfade of the whole Drive chain, reaching fully wet at 1 (unlike Delay's and Reverb's own Wet/dry, which floor the dry signal at 30%).
- **Gain** (slot 1) — Input gain into the polynomial waveshaper, 1×–5×.
- **Shape** (slot 2) — Recomputes the waveshaper's coefficients; harmonic character.
- **SRR 1** (slot 3) — First sample-rate reducer stage; raising the knob increases the reduction, off at the bottom.
- **SRR 2** (slot 4) — Second sample-rate reducer stage, in series after SRR 1; same off-at-the-bottom mapping.
- **XOR** (slot 5) — 8-bit XOR mask on the sample; the mid-travel plateau strips the low end and leaves the top.
- **Bit depth** (slot 6) — How many low bits the digital reorganizer scrambles.
- **Fuzz** (slot 7) — Floored equal-power blend from sine-fold (bottom, the default) to tanh-style saturation (top); the held-back path sits about −22 dB at either extreme, and both paths sit at −3 dB at the centre. Sets how much of the folder reaches the output, and with it how far Feedback, Fold and Symmetry reach: at Fuzz's top a Symmetry sweep moves the output about 25 dB less than at Fuzz's bottom.
- **Phase** (slot 8) — Allpass on the wet signal before Wet/Dry; silent effect at Wet/Dry 0.
- **Anti-alias brightness** (`Anti-alias`, slot 9) — Crossfade between a clean oversampled shaper path and a gritty one; all grit at the top of travel (the default), clean at the bottom. At the page's own defaults, level holds through the travel, with the deepest point about 0.6 dB under the quieter end.
- **Feedback** (slot 10) — Folder output fed back into its own input, one sample later; 0 = no feedback. The coefficient stays below self-oscillation across the whole travel; the margin narrows and the ring's decay time lengthens toward the top. Wraps the folder alone, so raising Fuzz holds the folder back and reduces Feedback's reach.
- **Fold** (slot 11) — Sine-fold divisor, 16× down to 1× as the knob rises, so fold density rises with it; every position folds. At Fuzz's maximum the folder sits about 22 dB under the saturator, so Fold is quieter there: every position above the bottom still changes the sound.
- **Tone** (slot 12) — Low-pass at the end of the chain, ~800 Hz to bypass; bypass at default.
- **Symmetry** (slot 13) — Bipolar phase offset into the folder, centred at default (middle of travel = no offset, the two halves skew opposite sides of the wave); loudest with Fuzz low, and about 25 dB less effect at Fuzz's top. On a held or picked note this creates a DC offset, small at the page's default settings and up to about three-quarters of full scale at some other Gain/Shape/Fold combinations.

## Filter

Filter 1 → comb → Filter 2 → output limiter. The shared tracking reference is the Balance-weighted
mean of the three VCO pitch controls in semitones, relative to middle C (261.6 Hz, note 60).

- **Cutoff 1** (slot 0) — Main cutoff, 20 Hz–20 kHz exponential before tracking; default 20 kHz.
- **Res 1** (slot 1) — Main resonance, Q 0.5–5.5; higher values emphasize and ring at the cutoff.
- **Morph 1** (slot 2) — Continuous low-pass → band-pass → high-pass; default low-pass.
- **Keytrk 1** (slot 3) — Cutoff tracking, −100% to +100%; centre default is 0%.
- **Mix 1** (slot 4) — Linear dry → Filter 1, reaching fully filtered at 1; default 1.
- **Comb delay** (slot 5) — Pure delay ahead of the comb, 1–100 ms, bounded by buffer capacity.
- **Comb tune** (slot 6) — 100 Hz–10 kHz at middle C, tracking the shared reference one octave per octave; bounded by the delay buffer.
- **Comb FB** (slot 7) — Comb feedback, zero at centre, up to ±0.95; farther from centre rings longer.
- **Comb LP** (slot 8) — Loop low-pass, from four times tracked tune up to 20 kHz, with the lower end bounded at 20 kHz.
- **Comb drive** (slot 9) — Saturator gain, 0.25×–4×; unity at centre. Below unity the comb can ring louder.
- **Cutoff 2** (slot 10) — Peak/notch frequency, 20 Hz–20 kHz exponential; default 20 kHz.
- **Res 2** (slot 11) — Peak/notch resonance, Q 0.5–5.5; raises peak gain and ringing without compensation.
- **Morph 2** (slot 12) — Peak → notch, retaining low and high passbands in the middle; away-from-centre gain rises from about −3 dB to 0 dB. Default notch.
- **Mix 2** (slot 13) — Linear comb output → Filter 2, fully filtered at 1; default 1. Intermediate mixes can lose level through phase differences.

## Delay

- **Wet/dry** (slot 0) — Delay wet level continuing to Reverb; dry signal floored at 30% of its own level, the same floor Reverb's Wet/dry shares.
- **Send** (slot 1) — Signal sent into the delay line; 0 = bypass.
- **Delay time** (slot 2) — Base delay length, ~1 ms–2 s.
- **Feedback** (slot 3) — Repeat feedback, capped below 98%.
- **Stereo width** (slot 4) — Offsets the right tap's read time behind the left; the offset is the whole of the widening. Near the top of Delay time the spread is reduced automatically so the read stays inside the line.
- **Freeze** (slot 5) — Crossfades the feedback loop from the ordinary Feedback level toward full, lossless recirculation; at 0 the loop runs at the Feedback level.
- **Mod depth** (slot 6) — LFO wobble depth on delay time.
- **Reverse blend** (`Reverse`, slot 7) — Blends in a backward-travelling read of the delay line against the forward tap; 0 = off, 1 = fully reversed.
- **Diffusion** (slot 8) — Smears each repeat through a short allpass cascade, after the feedback write; exact bypass at 0. Same cascade as Reverb's Density (slot 7), applied to the repeats instead of the tank's own feed.
- **Feedback drive** (`FB drive`, slot 9) — Pre-gain into the feedback saturator, 0.25×–4×, unity at default.
- **Feedback tone** (`FB tone`, slot 10) — Low-pass inside the feedback loop, ~800 Hz to bypass; bypass at default.
- **Mod rate** (slot 11) — Delay-time LFO rate, 0.05–1.25 Hz.
- **Width balance** (`Width bal`, slot 12) — Scales the time offset Stereo width produces; full at the top of travel (the default), none at the bottom.
- **Crush** (slot 13) — Sample-rate reduction on the feedback tap only; off at default.

## Reverb

- **Wet/dry** (slot 0) — Reverb mix, dry signal floored at 30% of its own level.
- **Send** (slot 1) — Signal sent into the reverb tank; 0 = bypass, same job as Delay's Send.
- **Room size** (slot 2) — Both tank delay-line lengths.
- **Decay** (slot 3) — Tank feedback / tail length.
- **Pre-delay** (slot 4) — Time before input reaches the tank, 0.02 ms to about 85 ms.
- **Damping** (slot 5) — low-pass on the tank output; darker and quieter tail at higher knob. Each tank line filters independently, so Stereo width's spread holds at every Damping setting.
- **Stereo width** (slot 6) — Spread between the tank's two taps.
- **Density** (slot 7) — Decorrelates the signal reaching the tank through a short allpass cascade; exact bypass at 0. Same cascade as Delay's Diffusion (slot 8), applied ahead of the tank instead of to already-repeating signal.
- **Mod** (slot 8) — Sinusoidal wow depth on the tank's read taps, fixed 0.35 Hz rate.
- **Hold** (slot 9) — Pushes tank feedback close to self-oscillation without reaching it.
- **Tank drive** (slot 10) — Pre-gain into the tank's feedback saturator, 0.25×–4×, unity at default.
- **Grit** (slot 11) — Digital reorganizer (bit-scramble) on the tank's feedback taps; bypass at 0.
- **Tilt** (slot 12) — Bipolar tone shave on the final output around ~1 kHz; center = no change.
- **Tuned** (slot 13) — Static offset on the tank's delay lengths, ±300 samples; center = zero offset.
