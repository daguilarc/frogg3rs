# Frogg3rs Manual

Operator manual for **Frogg3rs**, the Sheaf app — the current version of this instrument.

The same instrument core runs in four hosts:

- **Standalone** — a self-contained desktop app, with its own audio-device and
  MIDI-controller configuration (see MIDI controllers, below).
- **Browser build** — the same core running in a browser page.
- **VST3** and **AU plugin** — the same core loaded inside a DAW, where the host owns audio
  devices, transport and tempo.

Every parameter and every bank below is identical across all four; what differs between them is covered
in Audio configuration and MIDI controllers.

A different instrument, the frozen **Daisy Field hardware firmware**, shares this repository and is
documented separately, since its parameter model does not map onto this one: [`DAISY_MANUAL.md`](DAISY_MANUAL.md).

Terse one-line-per-parameter glossary for this app: [`QUICK_DICT.md`](QUICK_DICT.md).

## Release platforms

The desktop application releases for macOS and Windows. Each desktop release carries both: a macOS
disk image and a Windows zip holding the standalone executable.

The VST3 plugin releases for macOS and Windows. The Audio Unit releases for macOS only, because
the format exists only there.

### Opening a downloaded build

The first time you open Frogg3rs, your computer will refuse and warn you it cannot check the app for
malware. That is expected, and there is nothing wrong with the download. These builds are not
registered with Apple or Microsoft, so neither one recognises them.

You have to say yes once, and then it opens normally from then on. The steps are different on each
system.

**macOS.** Double-clicking shows **"Frogg3rs" Not Opened** — "Apple could not verify Frogg3rs is free
of malware..." — with a single **Done** button. That dialog will never offer a way to continue, no
matter how many times you open it. The permission lives elsewhere:

1. Click **Done** to dismiss the dialog. Dismissing it is what registers the blocked attempt.
2. Open **System Settings → Privacy & Security** and scroll to **Security**.
3. Click **Open Anyway** next to Frogg3rs, and authenticate.

The **Open Anyway** button only appears after a blocked attempt, and only for a short window
afterwards. If it is not there, double-click the app again to be blocked again, then go straight
back to Privacy & Security.

macOS remembers the choice, so later launches open normally. The same three steps apply the first
time a DAW loads the VST3 or the Audio Unit.

Control-clicking the app and choosing **Open** used to bypass this. macOS 15 removed that route for
apps in this state, so on current macOS the Privacy & Security panel is the only way through.

**Windows.** Opening the executable shows Microsoft Defender SmartScreen's blue **Windows protected
your PC** dialog, which names an unrecognised app and offers only **Don't run**. Click **More info**,
then **Run anyway**. Windows remembers this for that copy of the file; later launches open normally.

### Why the extra step is permanent

Removing these prompts takes an Apple Developer Program membership on macOS and an Authenticode
code-signing certificate on Windows. This project has neither, so expect these steps on every
release.

---

Six parameter banks — **Audio**, **Envelope**, **Filter**, **Drive**, **Delay**, **Reverb** — each with
16 encoder slots: 14 page parameters (slots 0–13), a bank-local **Crispy** (slot 14), and one shared
global **Crunchy** (slot 15, the same control in every bank). All six banks process audio every sample
regardless of which one is on screen — switching banks only changes what you're looking at.

## Global controls

### Crispy and Crunchy (fuego-ization)

Both controls apply the same bit-scramble ("fuego") to parameter values on their way to the DSP. What
they corrupt is the *values* other knobs are already set to, so what you hear depends on where those
other knobs are sitting.

A parameter's value is treated internally as an 8-bit number. The higher the fuego amount, the more of
that number's low bits get folded into an XOR/shift scramble keyed to the slot the parameter sits in.
At the knob's minimum, values pass through untouched. As the amount rises, small knob and modulation
moves stop being smooth: values snap between islands. Different slots on the same bank scramble
differently from each other, because the scramble pattern follows the slot index.

- **Crispy** (slot 14, one instance per bank, colored like that bank) scrambles only that bank's own 14
  page parameters (slots 0–13). It does not touch Crunchy, and it does not touch any other bank.
- **Crunchy** (slot 15) is a single shared parameter — the literal same value — wired into all six
  banks at once. It scrambles every page parameter in every bank, *and* it scrambles every bank's own
  Crispy value before that Crispy value is used to scramble its bank (Crunchy stacks underneath Crispy,
  never the other way around). Crunchy itself receives no scramble.
- Both default to 0 (no-op). Turning up Crunchy alone is a fast way to add grit everywhere at once
  without touching six separate Crispy knobs.

### Bank selection

Six named buttons — Audio, Envelope, Filter, Drive, Delay, Reverb — pick which bank's 16 slots populate
the encoder grid on screen. Every bank keeps processing audio whichever one is selected; the buttons
change what you can see and edit.

### Transport and the envelope gate

**Play** / **Stop** control the master clock's transport. Starting the transport is what plays notes:
while it runs, the shared envelope gate driving all three VCOs' Attack/Decay/Sustain/Release stages
(Envelope bank) pulses automatically — **open for the first half of every quarter note, closed for the
second half** — at whatever tempo the BPM control (30–300 BPM) is set to. That re-triggers the
envelopes on every beat. This app takes no MIDI note input.

**Stop** fades all three voices out over about 50 ms whatever the Release knobs are set to, so Stop
always reads as immediate, and clears the Delay/Reverb tails once every voice has gone silent.

### Modulation assignment

Click any parameter's encoder — a page parameter or a bank's own Crispy (Crunchy is excluded) — to open
a modulation view for that one parameter. It exposes 15 modulation sources, each with its own signed
depth. A depth of 0 means that source is off for this parameter. Turning a source's depth changes how
hard that source pushes the target, and depths on the same parameter sum together. Click the
parameter's encoder again, or the back target in the corner, to leave the modulation view.

The view fills the same 4×4 grid the parameters use: 15 sources and, in the last slot, the way back
out.

| | | | |
|---|---|---|---|
| Random S&H 1 | Random S&H 2 | Random S&H 3 | Random S&H 4 |
| Random S&H 5 | Random S&H 6 | VCO1 Audio | VCO2 Audio |
| VCO3 Audio | VCO1 EF | VCO2 EF | VCO3 EF |
| Noise | External Audio | External Audio EF | **[Back]** |

The six Random S&H sources are random values ordered so that each one lands on an extreme value less
often than the one before it:

- **Random S&H 1** takes a fresh value three times per quarter note, nearly always at the top or bottom
  of its range (about one step in sixteen rests at the centre), and moves to it instantly.
- **Random S&H 2** steps twice per quarter note, taking a fresh value on half its steps and repeating a
  held one on the rest; values favour the extremes and snap to five levels; each step smooths over
  about 5 ms.
- **Random S&H 3** steps once per quarter note through eight held values in random order, snapped to
  eight levels; each step smooths over about 20 ms.
- **Random S&H 4** steps once per two quarter notes through a four-bar phrase of eight held values,
  skipping to a random one on half its steps; values are unsnapped and lean slightly outward; each step
  smooths over about 100 ms.
- **Random S&H 5** steps once per four quarter notes through an eight-bar phrase in order, with a stray
  jump about once in fifty steps; values hug the centre; each step smooths over about 200 ms.
- **Random S&H 6** glides to a new value near the centre once every sixteen quarter notes, holding
  still for two thirds of that time and gliding for the rest.

Randomize All redraws the held values of the five stepped sources, and every launch starts them from a
different set. The VCO Audio sources are each oscillator's raw signal at audio rate. The EF sources are each oscillator's slow envelope
follower. Noise is broadband.

External Audio and External Audio EF carry signal once an external input is connected — see Audio and
MIDI configuration, below, for how each host connects one. Until then External Audio holds at 0.5 and
its envelope follower at 0.0, so neither one modulates anything.

### Randomize

Two randomize controls, both scoped to what is on screen.

**Randomize All** randomizes every page parameter in every bank, plus their first-level modulation
depths. It leaves each bank's Crispy and the global Crunchy alone, and does not descend into a depth's
own sub-depths. Pressed while a modulation view is open, it randomizes that parameter's depths and also
materializes and randomizes their second level.

**Randomize Page** randomizes exactly what is on screen: on a parameter page, that bank's values
including its Crispy and no depths; in a modulation view, that view's depths only.

Each press replaces the previous draw rather than adding to it — existing depths are cleared first, so
pressing twice does not accumulate more modulation than pressing once.

#### How many sources a randomize attaches

A randomized parameter does not get a depth on all 15 sources. Every parameter draws its own source
count, and the draw is weighted:

| sources attached | 0 | 1 | 2 | 3 | 4 | 5 or more |
|---|---|---|---|---|---|---|
| chance | 50% | 25% | 12.5% | 6.25% | 3.125% | 3.125% |

Each count is half as likely as the one before it. Half of all parameters come out carrying no
modulation at all, a quarter get one source, and four or more lands on roughly one draw in sixteen.
Whatever the count, the sources chosen are always distinct, and only sources that are currently
connected are eligible — an unconnected External Audio source is never drawn.

The weights are shaped this way because a parameter pushed by many sources at once tends to sit near
its center — independent movements cancel each other out — and a patch where everything is modulated
by everything sounds uniformly busy. Half the parameters staying still is what gives a randomized
patch its contrast, and the ones that do move keep each source's contribution audible. The tail never
closes, so an occasional densely modulated parameter still happens.

---

## Audio configuration

### Standalone

An **Audio I/O** page (reached from the app's sidebar) offers **Output device** and **Input device**
selectors listing the machine's own audio devices, plus a **Retry Input** button if capture fails. It is
named Audio I/O rather than Audio so it is not read as the Audio parameter bank. A
**Controllers** page maps an external MIDI controller to this app's own controls; see MIDI controllers, below. A **Sync** page lets the transport slave to incoming MIDI clock (**Receive
clock**, **Receive transport** toggles, a **PPQN** field 1–960); while slaved, the BPM control (Global
controls, above) becomes a read-only status display instead of an editable slider.

### Plugin (VST3 / AU)

The DAW owns audio devices, transport and tempo.

**Transport and tempo** follow the host. The plugin's own surface shows only Freeze (labeled "FREEZE")
where the standalone shows Play, Stop, Freeze, and Record. Whenever the host reports a tempo, the BPM
control becomes a read-only display, "BPM `<value>` (external clock)", the same display the standalone
shows while slaved to incoming MIDI clock.

**MIDI** reaches this instrument entirely through host-parameter automation. Every parameter — each
bank's 14 page parameters, its own Crispy, the one shared Crunchy, and Freeze — is exposed to the host as
a standard automatable plugin parameter, so a DAW's own MIDI-learn/CC-mapping targets one of these the
same way it would target any other plugin parameter. The plugin accepts the host's MIDI buffer but does
not read it itself.

**Input audio** is opt-in. The plugin has one optional stereo input bus, disabled until the host routes
into it. On the plugin's own surface, an **IN:** button beside Freeze cycles through **None** (the
default), each channel the host's bus currently provides, and, once the bus carries two or more channels,
their **Sum**. Selecting anything other than None is what connects External Audio and External Audio EF
(Global controls, above) — routing the DAW's bus into the plugin is not itself enough; the operator must
select an input here.

### Browser build

The transport runs on its own internal clock, the same as the standalone (Play, Stop, Record, and an
editable BPM slider). Audio input requires the browser's own microphone permission, granted through a
**Retry Input** action; nothing is captured, and External Audio stays silent, until that permission is
granted.

---

## MIDI controllers

### Overview

The Controllers page exists in the standalone and browser builds (the plugin takes MIDI through host
automation, as the Plugin subsection says). The add row at the bottom offers a **Preset** selector —
**MIDI Fighter Twister**, **Akai APC40 mkII (Generic)**, **Akai APC40 mkII (Ableton)**,
**Launchpad X**, **Launchpad Pro MK3**, **Launchpad Mini MK3**, or a **Custom** entry for each
device kind — and an **Add** button. Choosing a named preset and pressing Add installs a new row
carrying that preset's complete mapping; choosing Custom installs an empty, unbound row of that
device kind. A row keeps the identity of the preset that created it for as long as the row exists,
even after its mappings are edited by hand; if a row's mappings later diverge from what its preset
installs, a **Restore** button appears on the row, and pressing it reinstalls the preset's mappings
without renaming the row, changing its ports, or releasing it. A newly connected Twister, APC40, or
Launchpad is also offered through the page's configure flow.

### Reading a controller row

Each row is two lines. The first line shows the disclosure arrow, the controller's name, and its
device (MF Twister, Generic, Launchpad). The second line holds a status dot before each of the
**MIDI in** and **MIDI out** port selectors, then **Delete**; **Restore**, on a row created from a
preset whose mappings no longer match it; and **Release**, on a row created from a preset the
current build still recognises, once both ports are bound. A legend above the first row names the
dot colours: online, offline, not set.

A released row shows its name, device, and a **Released** badge on the first line, and its stored
MIDI in/out ports, **Configure** (when its preset still resolves), and **Reclaim** on the second
line. It has no disclosure arrow and no live editor.

### Renaming

Open a row's editor with the disclosure arrow at its left. The editor's first line is a **Name**
field and a **Rename** button. Renaming leaves the row open with its sections as they were. A
released row has no editor: reclaim it before renaming it.

### Adding a controller

The add row at the bottom is a **Preset** selector and an **Add** button. Pressing Add without
touching the selector adds the preset the selector displays. The new row takes its preset's name,
with a number appended when that name is taken, and its ports bind to a connected device matching
the preset. With no such device its ports read "(none)".

### What can be mapped

Every front-screen control. Encoder turns (relative or absolute), encoder pushes (which drill into a
knob's modulation exactly like an on-screen press), Play, Stop, Freeze, Record, Randomize All,
Randomize Page, Reset All, Reset Page, Bank 1 to 6, Bank Previous, Bank Next, Scene 1, Scene 2, the
scene blend (an analog control), BPM (an analog control, 30 to 300), and **Hold Drill**. Buttons can
be addressed by CC or by note number; analog controls by CC.

### Hold Drill

While a button mapped to Hold Drill is held, turning a knob drills into that knob's modulation
instead of changing its value, once per knob per hold; releasing the button makes every knob a plain
knob again. On an absolute knob, the first turn after release jumps to the knob's position.

### MIDI Fighter Twister

The preset maps the 16 encoders (turn and push, with LED ring feedback) and the six side buttons:
left side top to bottom Bank Previous, Bank Next, Randomize Page; right side top to bottom Randomize
All, Reset Page, Reset All. Utility settings the device needs, set in the Midi Fighter Utility:
every encoder's sensitivity/mode to "Enc 3FH/41H" (relative), all six side buttons to "CC Hold", and
"Bank Side Buttons" unchecked, so the side buttons keep sending CC 8 to 13 on channel 4 (channel 3
counted from 0) whatever bank the Twister shows.

### Akai APC40 mkII (Generic)

The unit powers up in this mode; nothing is sent to it. Top-row track knobs 1 to 8 are encoders 1 to
8, device knobs 1 to 8 are encoders 9 to 16, SHIFT is Hold Drill, PLAY/STOP/RECORD are
Play/Stop/Record, SCENE LAUNCH 1 and 2 are Scene 1 and 2, the LEFT and RIGHT arrows are Bank
Previous and Bank Next, DEVICE ON/OFF is Randomize Page, DEVICE LOCK is Randomize All, CLIP/DEVICE
VIEW is Reset Page, DETAIL VIEW is Reset All, STOP ALL CLIPS is Freeze, the CLIP STOP buttons under
tracks 1 to 6 are Bank 1 to 6, the crossfader is the scene blend and the master fader is BPM. Keep
Track 1 selected: the device knobs follow the selected track, and after pressing another Track
Select button they stop reaching encoders 9 to 16 until Track 1 is pressed again. The unit lights
its own buttons in this mode.

### Akai APC40 mkII (Ableton)

The same mapping, and the app switches the unit into Ableton mode when its output connects, so the
device knobs stay on encoders 9 to 16 whatever track is selected. In this mode every button light is
controlled by the host and this app sends none, so the buttons stay dark.

### Launchpad X

The preset maps the row of round buttons above the 8x8 pad grid and the column of round buttons to
its right; the 8x8 grid itself is left unmapped. Top row, left to right: Play, Stop, Freeze, Record,
Scene 1, Scene 2, Randomize Page, Reset Page. Right column, top six buttons: Bank 1 through Bank 6.
The app switches the unit into programmer mode when its output connects; in that mode every pad's
light is controlled by the host and this app sends none, so the pads stay dark.

The ports this preset matches are taken from Novation's manuals, not confirmed on a unit — if a
connected Launchpad X's MIDI in/out read "(none)" after adding it, bind its ports by hand from the
port selectors.

### Launchpad Pro MK3

The same pad map as Launchpad X, on the Pro MK3's own row of round buttons above the grid and column
beside it. The app switches the unit into programmer mode the same way when its output connects, and
its pads stay dark for the same reason. Its ports are matched the same unconfirmed way as Launchpad
X's; bind them by hand if they read "(none)".

### Launchpad Mini MK3

The same pad map and programmer-mode switch as Launchpad X. Its ports are matched the same
unconfirmed way as Launchpad X's; bind them by hand if they read "(none)".

### Saving

The mappings are saved with each patch and restored when the patch loads, and they are also kept in
the runtime configuration that loads at launch; whichever loads last is what is active.

---

## Audio bank

Three independent oscillators. Each one's phase modulation and ring modulation run off its own
internal carrier, so nothing in this bank reads another VCO's phase or output directly. Mixing is a
three-way balance. Cross-oscillator routing lives in the modulation view, where each VCO's audio-rate
output is a source (Modulation assignment, above).

**VCO1 / VCO2 / VCO3** (`VCO1`/`VCO2`/`VCO3`, slots 0–2) — each VCO's pitch, mapped exponentially from
20 Hz to 20 kHz. Default values land on 110 Hz, 220 Hz, and 330 Hz respectively, so a freshly launched app
already makes an audible chord with no knobs touched.

**Shape 1 / 2 / 3** (`Shp1`/`Shp2`/`Shp3`, slots 3–5) — each VCO's own waveform morph: sine at the
bottom, through saw at the middle, to square at the top, blending continuously rather than switching.
Independent per VCO.

**Phase mod 1 / 2 / 3** (`PM1`/`PM2`/`PM3`, slots 6–8) — each VCO's own phase-modulation depth, driven
by that VCO's own internal sine LFO (no cross-VCO modulation). Below a small floor near 0 it's silent;
above it, it grows from a subtle vibrato into an increasingly warbly, FM-like wobble at full depth. All
three VCOs' LFOs share one rate (see PM rate, slot 12).

**Ring mod 1 / 2 / 3** (`RM1`/`RM2`/`RM3`, slots 9–11) — each VCO ring-modulates against its *own*
internal carrier oscillator (20 Hz–5 kHz), never another VCO's signal. Has a genuine zero at the very
bottom of its travel — below a small floor, ring mod is completely off, not just quiet — then blends in
more of the metallic ring-modulated product as it's raised, fully replacing the dry tone at maximum.
Defaults to 0, so ring mod is off on a fresh app.

**PM rate** (`PMrt`, slot 12) — one shared knob (0.05 Hz–20 Hz) setting the phase-mod LFO rate for all
three VCOs at once. Depth and rate are independent: this sets the rate for all three, and each VCO's
own Phase mod knob sets its depth.

**VCO balance** (`VBal`, slot 13) — a single tilt sweeping mix emphasis from VCO1 (bottom of travel)
through an even three-way split (center, the default) to VCO3 (top of travel). By construction, every
VCO always keeps at least 10% and never exceeds 80% of the mix — this knob can shift emphasis but can
never silence a VCO outright.

---

## Envelope bank

Per-voice Attack/Decay/Sustain/Release for each of the three VCOs, interleaved: slot = 4×VCO index +
{Attack, Decay, Sustain, Release}. So the slot order reads A1 D1 S1 R1, A2 D2 S2 R2, A3 D3 S3 R3, then
two knobs shared across all three voices.

**Attack VCO1/2/3** (`A1`/`A2`/`A3`, slots 0/4/8) — time for that VCO's level to rise to full once the
transport's envelope gate opens, mapped exponentially from 1 ms to 250 ms. Defaults to the floor
(fastest, essentially instant-on).

**Decay VCO1/2/3** (`D1`/`D2`/`D3`, slots 1/5/9) — time for that VCO's level to fall from the
Attack peak down to its Sustain level, mapped exponentially from 5 ms to 1 s.

**Sustain VCO1/2/3** (`S1`/`S2`/`S3`, slots 2/6/10) — the level held while the gate stays open. Floored
at 25% — it can never be modulated down to a true, silencing zero — and defaults to full level (100%) so
a freshly launched app makes sound without touching any knob.

**Release VCO1/2/3** (`R1`/`R2`/`R3`, slots 3/7/11) — time for that VCO's level to fall to silence once
the gate closes, mapped exponentially from 5 ms to 2.5 s. Defaults to the floor (fastest). Stop always
overrides this with a fast ~50 ms fade regardless of this knob's position.

**Curve** (`Curv`, slot 12) — shared across all three voices. Reshapes every Attack/Decay/Release
ramp from a straight linear ramp (bottom of travel, the default) toward an increasingly "slow start,
fast finish" ease-in curve at the top. Sustain is a level rather than a ramp, so it is unaffected.

**Grace** (`Grac`, slot 13) — shared across all three voices. A minimum-hold: once a note reaches
Sustain, Grace keeps it there for at least this long (0–1 s) before honoring a gate-close, so a very
short gate pulse can't cut a note off mid-way through its own Attack/Decay. At its default (0) a note
cuts to Release the instant the gate closes. Grace only delays *when* Release starts; it never changes
the length of Attack, Decay or Release themselves.

---

## Filter bank

Signal path (at the default Topology): input feeds a resonant peaking EQ ("peak") and, in parallel, a
short pure delay into a comb filter; the two paths blend, then a resonant notch ("Scoop") is dipped in
before this bank hands off to Drive/Delay/Reverb downstream (see the Reverb bank's opening note for the
actual per-sample processing order, which differs from this bank's own visual left-to-right slot order).

**Comb offset** (`CmbOff`, slot 0) — a short pure delay ahead of the comb (1 ms–100 ms). Smears the
comb's attack transient without changing the comb's own pitch.

**Peak freq** (`PkFreq`, slot 1) — center frequency of the resonant peaking EQ, 100 Hz–20 kHz.

**Peak gain** (`PkGain`, slot 2) — height of that peak's boost. Flat (no boost) at the bottom of travel;
up to +6 dB (2×) at the top.

**Peak Q** (`PkQ`, slot 3) — width/resonance of the peak: a wide, gentle bump at the bottom, a narrow,
ringing resonance at the top.

**Comb delay** (`CmbDly`, slot 4) — the comb filter's own delay time, expressed as a pitch (100 Hz–10 kHz)
— this sets the comb's characteristic pitched ringing.

**Comb feedback** (`CmbFb`, slot 5) — comb resonance. Neutral (no resonance) at the exact center of
travel; pushing toward either end raises feedback up to ±0.95, where the comb rings almost
indefinitely — but always, eventually, decays; it can never truly self-sustain forever.

**Comb LP** (`CmbLP`, slot 6) — a low-pass filter inside the comb's own feedback loop. Turning it down
darkens/dampens the comb's repeats faster; turning it up brightens and sustains them longer, up to
20 kHz.

**Comb/Peak** (`Cmb/Pk`, slot 7) — blend between the peak path and the comb path (0 = pure peak, 1 =
pure comb).

**Scoop** (`Scoop`, slot 8) — how deep the resonant notch dips (0 = no dip, up to roughly a 95% deep dip
at maximum). This is the notch's own height/depth; whether the notch is audible in the output at all is
a separate control (Scoop depth, slot 13).

**Topology** (`Topo`, slot 9) — a continuous morph between running the comb path and the peak **in
parallel** (bottom of travel, the default — the comb's output does not reach the peak) and running
them fully **in series** (top of travel — the comb's output becomes the peak's input, so the peak
shapes an already comb-colored signal). Every value in between blends smoothly.

**Scoop freq** (`ScFq`, slot 10) — the notch's own center frequency (100 Hz–20 kHz), independent of
Peak freq.

**Scoop width** (`ScWd`, slot 11) — the notch's own Q/width, independent of Peak Q.

**Comb drive** (`CDrv`, slot 12) — pre-gain (0.25×–4×) into the comb's own saturator; unity gain at the
center default. Raising it pushes the comb's ringing into harder, more distorted saturation. The
saturator's own ceiling on the comb's output level holds regardless.

**Scoop depth** (`ScDp`, slot 13) — how much of the notch is blended into this bank's final output,
independent of the notch's own height (Scoop, slot 8). At 0 the notch is inaudible; at 1 it is fully
applied.

---

## Drive bank

Signal path: 2×-oversampled polynomial waveshaper (with a sine-fold/tanh-fuzz blend) → digital
reorganizer (bit XOR + bit-scramble) → two sample-rate reducers in series → a tone low-pass → dry/wet
Blend with an allpass Phase stage on the wet side.

**Drive** (`Drive`, slot 0) — input gain into the polynomial waveshaper (1×–5×). Higher drive pushes the
shaper into denser, more extreme harmonic territory.

**Shape** (`Shape`, slot 1) — recomputes the waveshaper's five polynomial coefficients along a
space-filling curve, continuously changing its harmonic character. (Distinct from the Audio bank's
per-VCO waveform Shape knobs — same name, different control.)

**SRR 1** (`SRR1`, slot 2) — first sample-rate-reducer stage. Lower knob values mean more reduction —
heavier, stair-stepped decimation.

**SRR 2** (`SRR2`, slot 3) — a second, identical reducer stage running in series right after SRR 1, for a
second layer of decimation.

**XOR** (`XOR`, slot 4) — an 8-bit XOR mask applied to the (quantized) sample, producing bit-flip
glitching. 0 = no flip.

**Bit depth** (`BitDp`, slot 5) — how many of the sample's low bits the digital reorganizer scrambles.
0 = untouched; higher values add progressively harsher low-bit digital noise.

**Fuzz** (`Fuzz`, slot 6) — blends between the sine-folded wet path (bottom of travel) and a
tanh-style hard saturator (top of travel) inside the waveshaper stage.

**Blend** (`Blend`, slot 7) — crossfades the dry (pre-Drive) signal against the fully processed Drive
chain output. 0 = dry only, untouched by everything above; 1 = fully wet.

**Phase** (`Phase`, slot 8) — a first-order allpass filter on the wet signal, applied *before* the Blend
crossfade above. At Blend 0 this has no audible effect at all, since dry passes through unfiltered
regardless of this knob's position.

**Anti-alias brightness** (`ABrt`, slot 9) — trims the cutoff of the oversampler's anti-alias filter
within a narrow range. A brightness adjustment, small either way.

**Link** (`Link`, slot 10) — how strongly the Drive knob's amount skews Shape's coefficients. Turning
it up makes Drive pull Shape's harmonic character along with it; turning it down decouples them.

**Fold** (`Fold`, slot 11) — divisor inside the sine-fold stage (1×–16×). A lower divisor folds harder;
a higher divisor folds more gently.

**Tone** (`Tone`, slot 12) — a low-pass filter at the end of the Drive chain, on the driven signal that
Blend mixes against the dry. Fully open at the top of travel (the default), and progressively darker as
it is turned down, to roughly an 800 Hz cutoff at the bottom.

**Waveshaper offset** (`Bias`, slot 13) — shifts the waveshaper's input by a small DC offset (up to
±0.02) before shaping, then removes the same offset from the output afterward. This biases the shaping
asymmetrically without adding audible DC. Zero offset at the center default.

---

## Delay bank

A stereo delay effect, positioned after Filter and before Reverb in the actual audio chain.

**Delay time** (`DlyTm`, slot 0) — base delay length, roughly 1 ms–2 s, exponential.

**Send** (`Send`, slot 1) — how much signal is sent into the delay line at all. At 0, this stage produces
no output — an exact bypass.

**Feedback** (`Fb`, slot 2) — how much of each repeat feeds back for another pass, clamped below 100%
(98% max) so repeats always eventually die out even at maximum.

**Stereo width** (`Width`, slot 3) — cross-feed and time-spread between the left/right taps. At 0 the two
channels behave almost identically; higher values spread the taps further apart in time and blend them
into each other less.

**Detune** (`Detune`, slot 4) — pitches the left and right repeats apart from each other, up to ±50
cents, for a chorus-y, slightly out-of-tune stereo repeat.

**Mod depth** (`ModDp`, slot 5) — amount of a slow LFO wobble on the delay time itself — chorus/vibrato
motion on the repeats.

**Wet mix** (`WetMx`, slot 6) — how much of the delay's wet output continues on toward Reverb.

**Color** (`Color`, slot 7) — folds into and biases Detune (averaged 50/50 with Detune's own value).

**Halo** (`Halo`, slot 8) — folds into and biases Mod depth (averaged 50/50 with Mod depth's own value).

**Feedback drive** (`FbDr`, slot 9) — pre-gain (0.25×–4×, unity at the center default) into the
feedback path's saturator. Raising it drives the repeats into more obvious saturation. The saturator's
own ceiling holds regardless.

**Feedback tone** (`FbTn`, slot 10) — a low-pass filter inside the feedback loop, so successive repeats
get progressively darker as this is turned down. Fully open at the top of travel (the default), to
roughly an 800 Hz cutoff at the bottom — the same range as the Drive bank's Tone. Because it sits in
the loop, the darkening compounds: each repeat passes the filter again.

**Mod rate** (`MdRt`, slot 11) — rate of the delay-time LFO whose depth Mod depth (slot 5) sets
(0.05 Hz–1.25 Hz). 0.25 Hz at the center default.

**Width balance** (`WBal`, slot 12) — an overall scalar on how strongly Stereo width's cross-feed and
time-spread apply. Full strength at the top of travel (the default); turning it down narrows the
stereo image Width can produce.

**Crush** (`Crsh`, slot 13) — a sample-rate reducer on the feedback tap only, so the repeats get
progressively more bit-crushed as this is raised. Off at its default of 0; the dry signal is never
crushed.

---

## Reverb bank

The signal actually reaches this bank last, after Audio/Envelope, Drive, Filter, and Delay have all
already processed it.

**Wet/dry** (`Wet`, slot 0) — reverb mix, internally capped at 70% wet so at least 30% dry always remains
audible even at maximum.

**Room size** (`Room`, slot 1) — sets both of the tank's internal delay-line lengths; larger room means
longer, more spacious-sounding reflections.

**Decay** (`Decay`, slot 2) — feedback amount inside the tank — tail length. Longer tails at higher
settings.

**Pre-delay** (`PreDly`, slot 3) — time before the input reaches the tank at all, separating a clean dry
transient from the onset of the reverb tail.

**Damping** (`Damp`, slot 4) — high-frequency loss in the feedback path. Turning it UP darkens the
tail; turning it down brightens it, up to roughly a 1.7 kHz damping cutoff at the bottom of travel.
The dark end is floored at about 150 Hz, so the tail keeps some top even at maximum.

**Stereo width** (`Width`, slot 5) — spread between the tank's two internal taps in the final left/right
output.

**Diffusion** (`Diff`, slot 6) — cross-feed between the tank's two internal lines. Higher values smear
the two lines into each other more.

**Mod depth** (`ModDp`, slot 7) — depth of a slow sinusoidal wow on the tank's read taps, for chorus-y
movement in the tail. 0 = no movement.

**Hold** (`Hold`, slot 8) — pushes the tank's internal feedback coefficient toward, but never quite to,
self-oscillation — indefinitely extending the tail's sustain without ever letting it hang forever. At 0
it adds nothing beyond ordinary Decay.

**Mod rate** (`MdRt`, slot 9) — rate of the Mod depth LFO (0.07 Hz–1.75 Hz). 0.35 Hz at the center
default.

**Tank drive** (`TkDv`, slot 10) — pre-gain (0.25×–4×, unity at the center default) into the tank's own
feedback saturator, for more obvious saturation on the tail as it is raised.

**Grit** (`Grit`, slot 11) — routes the tank's feedback taps through the same bit-scramble/XOR digital
reorganizer the Drive bank uses, adding digital grit to the tail. Off at 0.

**Tilt** (`Tilt`, slot 12) — a bipolar tone control on the final reverb output, crossfading between a
darker (lowpass-emphasized) tail and a brighter (highpass-emphasized) tail around a fixed ~1 kHz
corner. No change at the center default.

**Tuned** (`Tund`, slot 13) — a static offset on the tank's delay-line lengths, up to ±300 samples
around whatever Room size set, tuning the tank's own resonance by hand. Zero offset at the center
default.

---
