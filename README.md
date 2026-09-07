# Frogg3rs

This is a synthesizer designed for experimental noise; it probably won't make regular music super easily. It has three oscillators, an envelope per oscillator, a
filter, a distortion stage, a delay and a reverb. It can be run as a desktop app, as a VST3
or Audio Unit plugin in a Digital Audio Workstation (DAW), and in a browser. Personally, I recommend using the web browser version or the plugin versions in a DAW. Since I won't bother paying for an Apple Developer account, and I don't have a computer running Windows, the desktop versions will not appear as automatically "trusted" by Mac or Windows operating systems. Apple now makes you go into System Settings to give permission to this un-trusted software to run on your computer, rather than in a pop-up window, which is kind of annoying. The plugins run on DAW software that was made by professional software developers so you won't need to deal with this janky stuff. Reaper is a free DAW that works pretty darn well, no need to worry about breaking the bank with Ableton Live.

Two instruments share this repository. **Frogg3rs**, the app this README describes, lives under
[`app/`](app/README.md) and runs as the desktop app, the plugins and the browser build. The original
**Froggers** firmware for the Daisy Field Eurorack module lives under `src/`, is frozen, and has its own
[`DAISY_MANUAL.md`](DAISY_MANUAL.md). They differ in more than where they run: the firmware runs an
external input through the oscillators as a ring mod (Solo outputs only that ring mod while input is
present; Guitar mixes the dry input with it), while the app never puts the input in the signal path and
offers it only as two modulation sources, the raw signal and its envelope follower.

Anyway, every parameter is an encoder knob on one of six
banks of sixteen. Most parameter ranges are exponential, and you can attenuate the modulation depth by turning the knob, so even heavily modulated parameters are still audibly playable.

These are some of the unique features in this digital synthesizer:

**Modulation goes three levels deep.** Any parameter can be modulated by any of
fifteen sources (the sixteenth slot in a modulation page is always the "back" button), and for the oscillators that includes themselves, so all oscillators can control themselves and/or each other. Six of the sources are random sample-and-hold
lanes, graded from fast and extreme to slow and centred, the last one gliding continuously. The rest are taken from the instrument: each oscillator's raw
audio-rate output, each oscillator's envelope follower, white noise, and
external audio with its own envelope follower. The envelope followers run at
10 ms attack and 50 ms release, so they move at a few Hz rather than at audio
rate, and one oscillator can drive any parameter either at audio rate or at that
slower envelope rate depending on which source you pick.

Each modulation depth parameter can go down carrying those same fifteen sources, and
so is the depth of that one. The amount by which one source modulates another can be modulated, and so can that modulation's own amount.

This compounds exponentially. One parameter has fifteen depth knobs. Each of those
fifteen has fifteen of its own, which is 225. Each of those 225 has fifteen
more, which is 3,375. There is no fourth level, so a single parameter sits on
top of 3,615 knobs. Across the instrument's 91 top-level parameters (six banks of
fourteen, six Crispy knobs and Crunchy) that is 1,365 possible first-level depths and
328,965 depths if every level were filled.



**Randomization is weighted to stay playable.** Randomize All draws new values
across the whole instrument, along with one level of modulation depths, and
randomizes the Crispy of up to two banks. Randomize Page draws exactly what is
on screen: a bank's values on a parameter page, or that view's depths inside a
modulation view. The number of sources a parameter picks up is a weighted
draw, each count half as likely as the one before it: half of all parameters
come out with no modulation at all, a quarter get one source, and four or
more is rare. Inside a modulation view, Randomize All draws from a floor of
one source instead, so every press there moves something.
A randomized patch comes out with some parameters moving and some holding still.

Randomize All covers one level at a time. To randomize the level below, open a
modulation view and press it again there. Because each modulation depth is a real parameter that has to be
materialized before it can hold a value, and the instrument provisions a pool of
1,440 of them (96 parameter slots times 15 sources) that every level draws on,
randomizing every level everywhere would mean materializing thousands of depths
per parameter. I'd rather not try that on my laptop for now.

You may fork this repo if you wish to bias the randomization differently, or do even more randomization all at once. I
weighted this after testing and adjusting to taste; your taste may differ.

**Crispy and Crunchy** knobs control a bit-scrambling function, which corrupts
parameter values on their way to the DSP. This is basically like applying distortion to all the knobs, not just sounds.
**Crunchy** is a single global knob shared by the whole instrument; each bank has its own local **Crispy**, which scrambles only that bank's parameters. They cascade: Crunchy warps every value, Crunchy also warps
the bank's Crispy knob itself, and that warped Crispy is then applied on top of
the already-warped value. At zero (fully counter-clockwise, 7 o'clock) both do nothing. Turned up, knob moves stop
being smooth and values snap between newly crispy-crunchy islands. This works on human knob-turning as well as parameters patched through modulation sources.

**Crunchy** is never randomized: only you can turn it and set its modulation depths. **Randomize All** randomizes the Crispy of at most two of the six banks per press, never all six at once, because scrambling all six together lands where randomizing Crunchy would. **Randomize Page** randomizes the Crispy of the bank on screen.

Full parameter reference: [`MANUAL.md`](MANUAL.md).

**Two scenes** preserve two different values for each parameter in Scene 1 and Scene 2. Sliding between them slides each parameter value gradually between its Scene 1 and 2 values. Randomization puts different values on each scene, so really you get two different outcomes for each parameter with one roll of the dice, and you can then play around in a continuous spectrum between them.

**Current development is the Sheaf-based app under [`app/`](app/README.md)** — see
[Frogg3rs — Sheaf app](#frogg3rs--sheaf-app-current-development) below. `src/` is the **Daisy Field**
hardware firmware; it builds as two variants, `FroggersSolo` and `FroggersGuitar` (see
[`DAISY_MANUAL.md`](DAISY_MANUAL.md)), and its tests run with `make firmware-test`.

- **Sheaf app docs:** [`app/README.md`](app/README.md) — build instructions, Sheaf submodule pin, status
- **Manual:** [`MANUAL.md`](MANUAL.md) — global controls and all six parameter banks for the current
  Sheaf app
- **Daisy Field manual:** [`DAISY_MANUAL.md`](DAISY_MANUAL.md) — the Eurorack firmware (pages,
  buttons, modulation workflow, safe flash sequence)
- **Firmware tests:** run `make firmware-test` to build and run the Daisy firmware's test suite.
- **Quick Dict:** [`QUICK_DICT.md`](QUICK_DICT.md) — terse, slot-ordered parameter glossary for the
  current Sheaf app
- **License:** MIT — see [`LICENSE`](LICENSE) (copyright JoYoFresh and Diego Aguilar-Canabal)

## Frogg3rs — Sheaf app (current development)

Froggers' DSP on [Sheaf](https://github.com/jvictor0/Sheaf)'s parameter and UI framework. Sheaf is a
pinned submodule at `External/Sheaf`; [`app/README.md`](app/README.md) records the pinned commit.

Build the playable app from the repo root:

```sh
./app/build-launcher.sh
```

It writes `app/build-launcher/Frogg3rs.app` and signs it. The script caps itself at `-j2` and runs
`nice`, which an 8-core machine needs; raising it can freeze the host.

Change proposals and specs live under [`openspec/`](openspec/).

Parameter reference for this app: [`MANUAL.md`](MANUAL.md) (global controls, then all six banks —
Audio, Envelope, Filter, Drive, Delay, Reverb — parameter by parameter) and [`QUICK_DICT.md`](QUICK_DICT.md)
(the same six banks, one line per parameter).

MIDI controllers: the standalone and browser builds have a Controllers page where every front-screen
control can be mapped, with ready-made presets for the MIDI Fighter Twister, the Akai APC40 mkII, and
three Launchpad models. The manual's [MIDI controllers](MANUAL.md#midi-controllers) section describes
the page, the presets and the device settings each controller needs.
