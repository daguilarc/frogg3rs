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

**Freeze** is a latch. Engaging it stops the transport and holds the envelope gate open, and keeps the
delay recirculating, so the instrument drones on with the transport stopped.
Releasing Freeze silences the instrument, the same teardown Stop triggers. Pressing Play disarms
Freeze and starts the transport.

**Record** arms a capture of what the operator hears; press again to stop it. A finished recording is
offered as a file named from today's date, `YYYY-MM-DD.wav` — the standalone through a save dialog on
that name in the Documents folder, with a warning before it overwrites an existing file of the same
name. Record refuses to arm while the transport is stopped: "Press Play before recording." shows
beneath the transport buttons until Play is pressed or a recording arms. A capture stops itself after
30 minutes and is offered at that moment with a note that it stopped at the limit.

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

External Audio and External Audio EF carry signal once an external input is connected — see Audio configuration and
MIDI controllers, below, for how each host connects one. Until then External Audio holds at 0.5 and
its envelope follower at 0.0, so neither one modulates anything.

### Randomize

Two randomize controls, both scoped to what is on screen.

**Randomize All** randomizes every page parameter in every bank, plus their first-level modulation
depths, and randomizes the Crispy value of zero, one or two of the six banks per press — never all six
at once, because scrambling all six together lands where randomizing Crunchy would. It never touches
Crunchy, and it does not descend into a depth's own sub-depths. Pressed while a modulation view is
open, it instead randomizes that parameter's depths and materializes and randomizes their second
level; every such press attaches at least one source, so every press moves something.

**Randomize Page** randomizes exactly what is on screen: on a parameter page, that bank's values
including its own Crispy and no depths; in a modulation view, that view's depths only.

Each press replaces the previous draw rather than adding to it — existing depths are cleared first, so
pressing twice does not accumulate more modulation than pressing once.

#### How many sources a randomize attaches

A randomized parameter does not get a depth on all 15 sources; how many it gets is a weighted draw.
Randomize All on a parameter page, and Randomize Page at any level, both draw from a floor of zero:

| sources attached | 0 | 1 | 2 | 3 | 4 | 5 or more |
|---|---|---|---|---|---|---|
| chance | 50% | 25% | 12.5% | 6.25% | 3.125% | 3.125% |

Each count is half as likely as the one before it. Half of all parameters come out carrying no
modulation at all, a quarter get one source, and four or more lands on roughly one draw in sixteen.

Randomize All pressed inside a modulation view draws from a floor of one instead, so every such press
moves something:

| sources attached | 1 | 2 | 3 | 4 or more |
|---|---|---|---|---|
| chance | 50% | 25% | 12.5% | about 1 in 16 |

Whatever the gesture or the floor, the sources chosen are always distinct, and only sources that are
currently connected are eligible — an unconnected External Audio source is never drawn.

The weights are shaped this way because a parameter pushed by many sources at once tends to sit near
its center — independent movements cancel each other out — and a patch where everything is modulated
by everything sounds uniformly busy. Parameters staying still is what gives a randomized patch its
contrast, and the ones that do move keep each source's contribution audible. The tail never closes, so
an occasional densely modulated parameter still happens.

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
granted. A stopped recording downloads as `YYYY-MM-DD.wav`.

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
scene blend (an analog control), BPM (an analog control, 30 to 300), **Hold Drill**, and **Shift**.
Buttons can be addressed by CC or by note number; analog controls by CC.

### Hold Drill

While a button mapped to Hold Drill is held, turning a knob drills into that knob's modulation
instead of changing its value, once per knob per hold; releasing the button makes every knob a plain
knob again. On an absolute knob, the first turn after release jumps to the knob's position.

### Shift

A button mapped to Shift is held rather than tapped. While it is held, any other button on the same
controller that has a shifted job assigned does that job instead of its ordinary one. The shifted job
is set in the Shift column on the button's own row on the Controllers page, editable per row and saved
with the patch like every other mapping. If the controller is unplugged while Shift is still held, its
buttons stay shifted until Shift is pressed and released again.

### MIDI Fighter Twister

The preset maps the 16 encoders (turn and push, with LED ring feedback) and the six side buttons, each
a press paired with a second job under Shift:

| Button | Press | Shift + press |
|---|---|---|
| Left top | Bank Next | Bank Previous |
| Left middle | Play | Stop |
| Left bottom | Freeze | Reset Page |
| Right top | Scene 1 | Scene 2 |
| Right middle | Randomize Page | Randomize All |
| Right bottom | Shift | (none) |

Utility settings the device needs, set in the Midi Fighter Utility: every encoder's sensitivity/mode to
"Enc 3FH/41H" (relative), all six side buttons to "CC Hold", and "Bank Side Buttons" unchecked, so the
side buttons keep sending CC 8 to 13 on channel 4 (channel 3 counted from 0) whatever bank the Twister
shows. CC Hold is also what lets the app see the Shift button's own release.

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

The preset matches the unit's MIDI ports, not its DAW ports. Those port names are taken from
Novation's manuals and have not been confirmed on a Launchpad X — if a connected one's MIDI in/out
read "(none)" after adding it, bind its ports by hand from the port selectors.

### Launchpad Pro MK3

The same pad map as Launchpad X, on the Pro MK3's own row of round buttons above the grid and column
beside it. The app switches the unit into programmer mode the same way when its output connects, and
its pads stay dark for the same reason. Its MIDI ports are matched the same unconfirmed way as
Launchpad X's; bind them by hand if they read "(none)".

### Launchpad Mini MK3

The same pad map and programmer-mode switch as Launchpad X. Its MIDI port names were read from a
connected unit, so adding this preset with the Mini MK3 plugged in binds its ports; its DAW ports are
left alone.

### Saving

The mappings are saved with each patch and restored when the patch loads, and they are also kept in
the runtime configuration that loads at launch; whichever loads last is what is active.

---

## Audio bank

Three independent oscillators. Each one's phase modulation and ring modulation run off its own
internal carrier, so nothing in this bank reads another VCO's phase or output directly. Mixing is a
three-way balance. Cross-oscillator routing lives in the modulation view, where each VCO's audio-rate
output is a source (Modulation assignment, above).

**VCO1 / VCO2 / VCO3** (slots 0–2) — each VCO's pitch, mapped exponentially from
20 Hz to 5 kHz. Default values land on 110 Hz, 220 Hz, and 330 Hz respectively, so a freshly launched app
already makes an audible chord with no knobs touched.

**Shape 1 / Shape 2 / Shape 3** (slots 3–5) — each VCO's own waveform morph: sine at the
bottom, through saw at the middle, to square at the top, blending continuously rather than switching.
Independent per VCO.

**Ph.mod 1 / Ph.mod 2 / Ph.mod 3** (slots 6–8) — each VCO's own phase-modulation depth, driven
by that VCO's own internal sine LFO (no cross-VCO modulation). Below a small floor near 0 it's silent;
above it, it grows from a subtle vibrato into an increasingly warbly, FM-like wobble at full depth. The
size of the wobble is set here and does not change with the rate. All three VCOs' LFOs share one rate
(see PM rate, slot 12).

**Ringmod 1 / Ringmod 2 / Ringmod 3** (slots 9–11) — each VCO ring-modulates against its *own*
internal carrier oscillator (20 Hz–5 kHz), never another VCO's signal. Has a genuine zero at the very
bottom of its travel — below a small floor, ring mod is completely off, not just quiet — then blends in
more of the metallic ring-modulated product as it's raised, fully replacing the dry tone at maximum.
Defaults to 0, so ring mod is off on a fresh app.

**PM rate** (slot 12) — one shared knob (2 Hz–20 Hz) setting how fast the phase-mod wobble runs
for all three VCOs at once. Depth and rate are independent: this sets the speed for all three, and each
VCO's own Phase mod knob sets how far its pitch swings.

**VCO balance** (slot 13) — a single tilt sweeping mix emphasis from VCO1 (bottom of travel)
through an even three-way split (center, the default) to VCO3 (top of travel). By construction, every
VCO always keeps at least 10% and never exceeds 80% of the mix — this knob can shift emphasis but can
never silence a VCO outright.

---

## Envelope bank

Per-voice Attack/Decay/Sustain/Release for each of the three VCOs, interleaved: slot = 4×VCO index +
{Attack, Decay, Sustain, Release}. So the slot order reads A1 D1 S1 R1, A2 D2 S2 R2, A3 D3 S3 R3, then
two knobs shared across all three voices.

**Attack VCO1 / Attack VCO2 / Attack VCO3** (`A1`/`A2`/`A3`, slots 0/4/8) — time for that VCO's level to
rise to full once the transport's envelope gate opens, mapped exponentially from 1 ms to 250 ms.
Defaults to the floor (fastest, essentially instant-on).

**Decay VCO1 / Decay VCO2 / Decay VCO3** (`D1`/`D2`/`D3`, slots 1/5/9) — time for that VCO's level to
fall from the Attack peak down to its Sustain level, mapped exponentially from 5 ms to 1 s.

**Sustain VCO1 / Sustain VCO2 / Sustain VCO3** (`S1`/`S2`/`S3`, slots 2/6/10) — the level held while the
gate stays open. Floored at 25% — it can never be modulated down to a true, silencing zero — and
defaults to full level (100%) so a freshly launched app makes sound without touching any knob.

**Release VCO1 / Release VCO2 / Release VCO3** (`R1`/`R2`/`R3`, slots 3/7/11) — time for that VCO's
level to fall to silence once the gate closes, mapped exponentially from 5 ms to 2.5 s. Defaults to the
floor (fastest). Stop always overrides this with a fast ~50 ms fade regardless of this knob's position.

**Curve** (slot 12) — shared across all three voices. Reshapes every Attack/Decay/Release
ramp from a straight linear ramp (bottom of travel, the default) toward an increasingly "slow start,
fast finish" ease-in curve at the top. Sustain is a level rather than a ramp, so it is unaffected.

**Grace** (slot 13) — shared across all three voices. A minimum-hold: once a note reaches
Sustain, Grace keeps it there for at least this long (0–1 s) before honoring a gate-close, so a very
short gate pulse can't cut a note off mid-way through its own Attack/Decay. At its default (0) a note
cuts to Release the instant the gate closes. Grace only delays *when* Release starts; it never changes
the length of Attack, Decay or Release themselves.

---

## Filter bank

Signal path (at the default Topology): a resonant notch filter ("Scoop") is blended into the input
first; that scooped signal feeds both a resonant peaking EQ ("peak") and, through a short pure delay, a
comb filter. Topology morphs how much the comb path feeds the peak path, from fully parallel to fully
in series, and Comb/Peak blends the peak and comb outputs together before this bank hands off to
Drive/Delay/Reverb downstream.

**Peak freq** (slot 0) — center frequency of the resonant peaking EQ, 100 Hz–20 kHz.

**Peak gain** (slot 1) — how far the peak stands above the rest of the signal. At the bottom of
travel the peak is flat. The peak path divides its own output by the same height it raises the peak
to, so turning this up holds the level at the peak's own center frequency and pulls the rest of the
signal down. With the bank at its defaults, which put the peak at 100 Hz, a full-scale tone at that
center frequency holds within 0.00025 dB across the whole travel, a tone at 1 kHz falls
6.04 dB, and a tone at 5 kHz falls 6.52 dB; a broadband source loses 7.10 dB of total level end to
end. The peak is shaped by attenuation, so reaching its full height costs that much level.

**Peak Q** (slot 2) — width/resonance of the peak: a wide, gentle bump at the bottom, a narrow,
ringing resonance at the top.

**Comb offset** (slot 3) — a short pure delay ahead of the comb (1 ms–100 ms). Smears the
comb's attack transient without changing the comb's own pitch.

**Comb delay** (slot 4) — the comb filter's own delay time, expressed as a pitch (100 Hz–10 kHz)
— this sets the comb's characteristic pitched ringing.

**Comb feedback** (`Comb FB`, slot 5) — comb resonance. Neutral (no resonance) at the exact center of
travel; pushing toward either end raises feedback up to ±0.95, where the comb rings almost
indefinitely — but always, eventually, decays; it can never truly self-sustain forever.

**Comb LP** (slot 6) — a low-pass filter inside the comb's own feedback loop. Turning it down
darkens/dampens the comb's repeats faster; turning it up brightens and sustains them longer, up to
20 kHz.

**Comb drive** (slot 7) — pre-gain (0.25×–4×) into the comb's own saturator; unity gain at the
center default. Raising it pushes the comb's ringing into harder, more distorted saturation. The
saturator's own ceiling on the comb's output level holds regardless.

**Scoop mix** (slot 8) — blends a resonant notch filter into the shared input that feeds both the comb
and peak paths. At the bottom of travel the notch is absent and Comb/Peak sees the unaffected signal;
raising it mixes in progressively more of the notched signal, up to fully replacing the input at
maximum. The notch's own dip depth is a separate control (Scoop depth, slot 11).

**Scoop freq** (slot 9) — the notch's own center frequency (100 Hz–20 kHz), independent of
Peak freq.

**Scoop width** (slot 10) — the notch's own Q/width, independent of Peak Q.

**Scoop depth** (slot 11) — how deep the notch dips (0 = no dip, up to roughly a 95% deep dip at
maximum). Independent of Scoop mix (slot 8), which sets how much of that notch reaches the signal at
all.

**Comb/Peak** (slot 12) — an equal-power blend between the peak path and the comb path. The
blend's travel is floored, so the held-back branch is never fully absent: about −22 dB down at
either extreme, exactly −3 dB — a true 50/50 — at the center of travel. Topology (slot 13) is the
separate control that morphs between parallel and series.

**Topology** (slot 13) — a continuous morph between running the comb path and the peak **in
parallel** (bottom of travel, the default — the comb's output does not reach the peak) and running
them fully **in series** (top of travel — the comb's output becomes the peak's input, so the peak
shapes an already comb-colored signal). Every value in between blends smoothly.

---

## Drive bank

Signal path: an oversampled polynomial waveshaper (with a sine-fold/tanh-fuzz blend) → digital
reorganizer (bit XOR + bit-scramble) → two sample-rate reducers in series → a tone low-pass → dry/wet
Wet/Dry with an allpass Phase stage on the wet side.

Wet/Dry is the page's master and Gain is what makes distortion as opposed to crushing, which is why
those two sit first. No knob switches the whole page off except Wet/Dry: the bit and rate manglers
act on a signal at any level, so crushing a quiet signal still crushes it.

**Wet/Dry** (slot 0) — crossfades the dry (pre-Gain) signal against the fully processed Drive
chain output. 0 = dry only, untouched by everything below and bit-for-bit identical to the input;
1 = fully wet. Unlike Delay's and Reverb's own wet/dry controls, this one is not capped — it reaches
fully wet, because a distortion that replaces its source is a sound you ask for by name.

The crossfade is equal-power rather than linear. That matters here more than on most mixes: the
wet path's fundamental is partly out of phase with the dry one, and which way it leans changes as
Gain moves, so a linear crossfade thinned the sound over the knob's first quarter instead of
fading the distortion in. Worst dip across the page's range is now about 1 dB, where it was
just over 4 dB.

**Gain** (slot 1) — input gain into the polynomial waveshaper (1×–5×). Higher gain pushes the
shaper into denser, more extreme harmonic territory.

**Shape** (slot 2) — recomputes the waveshaper's five polynomial coefficients along a
space-filling curve, continuously changing its harmonic character. (Distinct from the Audio bank's
per-VCO waveform Shape knobs — same name, different control.)

**SRR 1** (slot 3) — first sample-rate-reducer stage. Raising the knob increases the reduction —
heavier, stair-stepped decimation; off at the bottom (the same mapping as Delay's Crush, slot 13).

**SRR 2** (slot 4) — a second, identical reducer stage running in series right after SRR 1, for a
second layer of decimation; same off-at-the-bottom mapping as SRR 1.

**XOR** (slot 5) — an 8-bit XOR mask applied to the (quantized) sample, producing bit-flip
glitching. 0 = no flip.

The middle of the travel is not a quiet spot, though it reads as one: from roughly 0.3 to 0.7 the
mask strips about 16 dB out of everything below 1 kHz while leaving the top octaves where they
were. Overall level barely moves. Body gone, fizz intact — most obvious into a filter. The knob is
also mirror-symmetric: a setting and its opposite differ by a polarity flip, which is inaudible at
full wet and audible at partial Wet/Dry, where the sign sums against the dry signal.

**Bit depth** (slot 6) — how many of the sample's low bits the digital reorganizer scrambles.
0 = untouched; higher values add progressively harsher low-bit digital noise. The knob is mapped
onto the bit counts that actually do something, so the first audible step arrives just off the
floor rather than a fifth of the way up.

**Fuzz** (slot 7) — blends between the sine-folded wet path (bottom of travel, the default) and a
tanh-style saturator (top of travel) inside the waveshaper stage. The saturator's curve is smooth
up to an input of 3 and clamps above that. The gain ahead of it drives well past 3, so the level
reaching the curve is what makes the top of the knob sound hard.

The blend is floored the same way Comb/Peak's is, so both paths stay in the sound at every
position: the held-back one sits about −22 dB at either extreme, and the two meet at −3 dB — a
true 50/50 — at the center of travel. At the default the saturator is the held-back path.

Fuzz therefore sets how much of the folder reaches the output, and Feedback, Fold and Symmetry all
work inside that folder. At the top of Fuzz's travel their reach shrinks: on a 220 Hz tone at full
wet, sweeping Symmetry end to end moves the output about 25 dB less than the same sweep does with
Fuzz at the bottom, and Feedback and Fold are held back by the same blend. What Fold still moves up
there is under Fold below.

**Phase** (slot 8) — a first-order allpass filter on the wet signal, applied *before* the Wet/Dry
crossfade above. At Wet/Dry 0 this has no audible effect at all, since dry passes through unfiltered
regardless of this knob's position.

An allpass does not change level on its own, so everything you hear from this knob is how the
rotated wet signal sums against the dry one. That makes its range conditional rather than fixed: at
a partial Wet/Dry on a bass note the top of the travel lifts the sum by a couple of dB, while on a
mid note with the shaper driven hard no position changes the tone measurably. The knob is mapped
through the allpass's own corner frequency, so equal turns move the audible band by comparable
amounts across the whole travel.

**Anti-alias brightness** (`Anti-alias`, slot 9) — crossfades between a clean, heavily
oversampled shaper path and a grittier one. The top of the travel — the default — is all grit,
with the clean path multiplied out of the mix entirely; the bottom is the clean path.

Turned down, it removes the metallic ring that hard shaping puts on higher notes: partials that
belong to no key, a different one per semitone, so a line played up the keyboard changes character
note to note instead of transposing. Measured at a 1.5 kHz tone, the clean end is about 23 dB
cleaner than the grit end.

It runs out at the top of the range, and deliberately so. It cleans up to roughly A6; above about
2 kHz the harmonics that would need cleaning have already passed the oversampled domain's own
ceiling, where no filter in this path can reach them. Expect no change at all on a bass note —
nothing folds down there to remove. The in-band level moves a little across the sweep: the
fundamental drops about 2.5 dB from the clean end to the grit end.

The crossfade holds its level through the middle of the travel at the page's own defaults, where the
deepest point of the sweep sits about 0.6 dB under the quieter of the two ends. That figure belongs
to those defaults.

**Feedback** (slot 10) — the amount of the folder's own output fed back into its own input,
one sample later. At 0 (the default) the folder reduces to a plain sine fold with no feedback at
all. Turning it up makes the fold resonate and sing back on itself instead of simply getting
louder or grittier; the coefficient stays below the point where that resonance turns into
self-sustaining oscillation across the whole travel, so a struck note's own resonance always dies
away once the note does, at every setting. The margin shrinks toward the top of the travel, so the
ring after a note stops takes noticeably longer to die out up there than it does lower down the
knob, even though it always eventually reaches silence.

The loop wraps the folder alone, so Fuzz sets how much of it is audible: turning Fuzz up holds the
folder back behind the saturator, and Feedback's reach goes with it.

**Fold** (slot 11) — divisor inside the sine-fold stage. The divisor falls from 16× to 1× as the
knob is turned up, so more of the signal's swing wraps through the fold each cycle and fold density
RISES with the knob. Every position folds; the bottom of the travel folds least.

At Fuzz's maximum the folder sits about 22 dB under the saturator, so Fold is quieter up there
without going inert: on a 220 Hz tone at full wet with Gain and Shape at their page defaults, every
position above the bottom of Fold's travel still changes the sound.

**Tone** (slot 12) — a low-pass filter at the end of the Drive chain, on the driven signal that
Wet/Dry mixes against the dry. Fully open at the top of travel (the default), and progressively darker as
it is turned down, to roughly an 800 Hz cutoff at the bottom.

**Symmetry** (slot 13) — shifts the folder's own input by a phase offset, bipolar around a centred
default: the middle of the travel is no offset, and the two halves skew opposite sides of the wave
to fold first. It is loudest with Fuzz low, where the folder carries most of the sound; at Fuzz's
top the same end-to-end sweep moves the output about 25 dB less.

Silence in always produces silence out, at any Symmetry setting. A held or picked note is not
silence, though, and skewing which half of a wave folds first is a genuine asymmetry, so a driven
signal comes out with a DC offset. At the page's default settings the offset stays small, under two
hundredths of full scale at either end of the travel. At some combinations of Gain, Shape and Fold
away from those defaults it grows much larger, up to around three-quarters of full scale.

### "Feedback" across the instrument

Five controls carry the name Feedback, across three banks. Three of them set how much of a stage's
own output returns to that same stage's input; the other two shape what travels around Delay's
loop on each pass:

- **Filter bank, Comb feedback** — sets how much of the comb delay line's output returns to the
  comb delay line's own input.
- **Drive bank, Feedback** — sets how much of the wavefolder's output returns to the wavefolder's
  own input, one sample later.
- **Delay bank, Feedback** — sets how much of each repeat returns to the delay line's own input.
- **Delay bank, Feedback drive** — sets the pre-gain into the saturator that sits in Delay's
  feedback loop, and with it how hard each pass is driven.
- **Delay bank, Feedback tone** — sets the cutoff of the low-pass inside Delay's feedback loop, and
  with it how much treble each pass keeps.

Reverb's tank runs on one feedback coefficient, and two controls set it: Decay places it between
0.1 and 0.98, and Hold moves it from wherever Decay left it toward 1, always stopping short.

---

## Delay bank

A stereo delay effect, positioned after Filter and before Reverb in the actual audio chain.

**Wet/dry** (slot 0) — how much of the delay's wet output continues on toward Reverb. The dry signal
never drops below 30% of its own level, the same floor Reverb's own Wet/dry shares. At 0, with Send
also at its default-closed 0, the delay line is never fed and this stage is transparent.

**Send** (slot 1) — how much signal is sent into the delay line at all. At 0, this stage produces
no output — an exact bypass.

**Delay time** (slot 2) — base delay length, roughly 1 ms–2 s, exponential.

**Feedback** (slot 3) — how much of each repeat feeds back for another pass, clamped below 100%
(98% max) so repeats always eventually die out even at maximum.

**Stereo width** (slot 4) — cross-feed and time-spread between the left/right taps. At 0 the two
channels behave almost identically; higher values spread the taps further apart in time and blend them
into each other less.

**Freeze** (slot 5) — crossfades the delay's feedback loop from its ordinary level toward full,
lossless recirculation. Raising it both lets more of each repeat feed back, up to unity gain, and
reduces how much new input enters the loop, so at maximum the loop holds whatever was already inside
it and takes in nothing new. At 0 the loop runs at the ordinary Feedback (slot 3) level.

**Mod depth** (slot 6) — amount of a slow LFO wobble on the delay time itself — chorus/vibrato
motion on the repeats.

**Reverse blend** (`Reverse`, slot 7) — blends in a second, backward-travelling read of the same delay
line against the ordinary forward-reading tap, per channel. At 0 only the forward tap is heard;
raising it mixes in increasing amounts of reversed playback, up to fully reversed repeats at maximum. The backward read sweeps continuously across roughly one delay-time of buffered
history and loops with a short crossfade to avoid an audible click at the wrap point.

**Diffusion** (slot 8) — smears each repeat through a cascade of three short allpass sections applied
once per repeat, after the feedback write and before the output limiter. At 0 this is an exact bypass;
raising it progressively blurs the sharp attack of each repeat into a smoother, more diffuse tail, up
to a comfortably stable maximum.

**Feedback drive** (`FB drive`, slot 9) — pre-gain (0.25×–4×, unity at the center default) into the
feedback path's saturator. Raising it drives the repeats into more obvious saturation. The saturator's
own ceiling holds regardless.

**Feedback tone** (`FB tone`, slot 10) — a low-pass filter inside the feedback loop, so successive
repeats get progressively darker as this is turned down. Fully open at the top of travel (the default),
to roughly an 800 Hz cutoff at the bottom — the same range as the Drive bank's Tone. Because it sits in
the loop, the darkening compounds: each repeat passes the filter again.

**Mod rate** (slot 11) — rate of the delay-time LFO whose depth Mod depth (slot 6) sets
(0.05 Hz–1.25 Hz). 0.25 Hz at the center default.

**Width balance** (`Width bal`, slot 12) — an overall scalar on how strongly Stereo width's cross-feed
and time-spread apply. Full strength at the top of travel (the default); turning it down narrows the
stereo image Width can produce.

**Crush** (slot 13) — a sample-rate reducer on the feedback tap only, so the repeats get
progressively more bit-crushed as this is raised. Off at its default of 0; the dry signal is never
crushed.

---

## Reverb bank

The signal actually reaches this bank last, after Audio/Envelope, Drive, Filter, and Delay have all
already processed it.

**Wet/dry** (slot 0) — reverb mix. The dry signal never drops below 30% of its own level, even at
maximum, so it always remains audible. At 0, with Send also at its default-closed 0, the tank is
never fed and this stage is transparent.

**Send** (slot 1) — how much signal is sent into the reverb tank at all. At 0, the tank receives
nothing and this stage produces no output — an exact bypass, the same job Delay's own Send does.

**Room size** (slot 2) — sets both of the tank's internal delay-line lengths; larger room means
longer, more spacious-sounding reflections.

**Decay** (slot 3) — feedback amount inside the tank — tail length. Longer tails at higher
settings.

**Pre-delay** (slot 4) — time before the input reaches the tank at all, separating a clean dry
transient from the onset of the reverb tail. Roughly 0.02 ms up to about 85 ms at 48 kHz, the
ceiling being as much as the pre-delay line can hold; the top of the travel is far enough out to
hear the dry hit and its tail as two separate events.

**Damping** (slot 5) — a low-pass on the tank's output. Turning it UP darkens the tail; turning
it down brightens it, up to roughly a 1.7 kHz cutoff at the bottom of travel. The dark end is
floored at about 150 Hz, so the tail keeps some top even at maximum.

It also makes the tail quieter, and that is the filter doing its job rather than a fault: a
low-pass removes the energy sitting above its corner, so how much level it takes depends entirely
on the material. Across the full travel it costs about 10 dB on a broad, bright source and about
half a decibel on a low sine. Nothing compensates for that on purpose — a fixed makeup set for one
of those two is wrong for the other, and it would turn Damping into a volume control on the
material it currently leaves alone. Use **Send** to put the level back.

**Stereo width** (slot 6) — spread between the tank's two internal taps in the final left/right
output.

**Diffusion** (slot 7) — cross-feed between the tank's two internal lines. Higher values smear
the two lines into each other more.

**Mod** (slot 8) — depth of a slow sinusoidal wow on the tank's read taps, for chorus-y
movement in the tail, at a fixed rate (0.35 Hz). 0 = no movement.

**Hold** (slot 9) — pushes the tank's internal feedback coefficient toward, but never quite to,
self-oscillation — indefinitely extending the tail's sustain without ever letting it hang forever. At 0
it adds nothing beyond ordinary Decay.

**Tank drive** (slot 10) — pre-gain (0.25×–4×, unity at the center default) into the tank's own
feedback saturator, for more obvious saturation on the tail as it is raised.

**Grit** (slot 11) — routes the tank's feedback taps through the same bit-scramble/XOR digital
reorganizer the Drive bank uses, adding digital grit to the tail. Off at 0.

**Tilt** (slot 12) — a bipolar tone control on the final reverb output, crossfading between a
darker (lowpass-emphasized) tail and a brighter (highpass-emphasized) tail around a fixed ~1 kHz
corner. No change at the center default.

**Tuned** (slot 13) — a static offset on the tank's delay-line lengths, up to ±300 samples
around whatever Room size set, tuning the tank's own resonance by hand. Zero offset at the center
default.

---
