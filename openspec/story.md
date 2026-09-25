# Frogg3rs user story (ratified, main 2255303)

- frogg3rs `22553032155610cd57982b61c1dea31fcacd3c37` (the code matches `70db8bf`; only MANUAL.md and README.md differ)
- Sheaf `62829a4a694af78362fb8ef1cdecd88f16a3ed34`
- Paths with no prefix are relative to the frogg3rs repository root. Sheaf paths are written
  `Sheaf:<path under External/Sheaf/projects/synth>`.

Inputs merged: two draft stories (A, B), a control inventory (INV), the recorded MIDI-mapping
rulings (MR) and host-change rulings (HR), the operator's rulings given with the merge (OP), and
the operator's rulings of 2026-09-23 (SR). The operator ratified every step of the merged story as
written. This text changes only what SR rules.

## How to read this document

Every step has a stable ID, the draft steps it merges (`A:` / `B:`), a **Start** state, an **Action**
over the control's full range, and a **Result**. `[SWEEP]` means a program can check the result.
`[UX]` means only the operator can judge it. Each section header names the platforms: **S** is the
standalone app (macOS and Windows), **B** is the browser build, and **P** is the plugin (VST3 on
macOS and Windows, AU on macOS). A step's **Caps** line appears only where a limit applies or the
platforms differ.

Markers inside a step:

- **CODE**: the drafts disagreed with each other, or followed a spec that the code on main
  contradicts. The step states what main does and gives the evidence (a symbol and a file). Section
  2.2 lists every CODE resolution with the text that needs correcting.
- **RULED**: a recorded ruling (MR, HR) or an operator ruling (OP, SR) decides the step. Sections 2.1
  and 2.3 quote them.
- **fix required**: a ruling sets this behaviour and main does not deliver it yet, in the code, the
  manual, or both. Section 3.1 names the ruling, and section 5 quotes the manual text.
- **CHECK**: the result comes from reading the code and has not been run. It becomes a SWEEP task that
  runs the build and records what happens.
- **PENDING-PLG**: the separate investigation of controller MIDI in the plugin decides this step.

The bounds come from OP and SR. The story follows one user through the full range of every feature.
It covers one full modulation drill-down (MOD-01 to MOD-12, to the deepest level main allows) and one
level-1 randomization (RND-01). It has one branch per shipped controller default plus Custom, and no
WRLD.Bldr branch. Two controllers at once is one representative pair (SWX-01). The plugin branch
covers only what differs from the standalone. Recording is capped at 30 minutes.
`mod-blend-semantics`, `mod-rack-dual-midi-jacks` and planned features are left out. MIDI out
(pitch as notes) is a separate change. Loading from a Sheaf launcher catalog and hand-edited
configuration or patch files are outside the story.

---

# 1. The story

## 1.1 Getting the app and opening it the first time (S; P in 1.17)

Release artifacts: a macOS disk image, a Windows zip, VST3 for macOS and Windows, and AU for macOS.

### Branch BR-01: macOS standalone

- **INS-01** `[UX]` (A:INS-01; B:A-MAC-01) **Start:** the disk image has been downloaded and the app
  has never been opened. **Action:** double-click Frogg3rs. **Result:** the dialog **"Frogg3rs" Not
  Opened** appears with a single **Done** button, and it offers no way to continue.
- **INS-02** `[UX]` (A:INS-02; B:A-MAC-02) **Start:** INS-01. **Action:** press Done, open System
  Settings, then Privacy & Security, then Security, press **Open Anyway**, and authenticate.
  **Result:** the app opens. Open Anyway appears only for a short time after a blocked attempt;
  opening the app again brings it back. Control-click, then Open, does not bypass the block on
  macOS 15.
- **INS-03** `[UX]` (A:INS-03; B:A-MAC-03) **Start:** INS-02 done once. **Action:** quit and
  double-click again. **Result:** the app opens with no dialog.
- **INS-04** `[SWEEP]` (A:IO-04; B:A-MAC-04) **Start:** app open. **Action:** select an input device
  (IO-04). **Result:** macOS asks for microphone permission, because the bundle declares a
  microphone usage string.

### Branch BR-02: Windows standalone

- **INS-05** `[UX]` (A:INS-04; B:A-WIN-01) **Start:** the zip has been extracted and the executable
  has never run. **Action:** open it. **Result:** SmartScreen shows **Windows protected your PC** and
  offers only **Don't run**.
- **INS-06** `[UX]` (A:INS-05; B:A-WIN-02) **Start:** INS-05. **Action:** press **More info**, then
  **Run anyway**. **Result:** the app opens, and later launches of that copy open normally.
- **INS-07** `[SWEEP]` (B:A-WIN-03) **Start:** app open on Windows. **Action:** go through every
  control in this story. **Result:** every control is present, including the audio device pages.

## 1.2 Launch state (S)

- **LCH-01** `[SWEEP]` (A:LCH-01; B:S0, A-01) **Start:** first launch, with no patch and no runtime
  configuration. **Action:** open the app. **Result:** the transport is stopped and the app is
  silent. BPM reads 120. There are no controller rows. Audio I/O input reads No Input. Sync has every
  toggle off and PPQN 24. The Audio page is shown. **CODE:** A said 120 BPM and B said "not stated";
  `MasterClock::kDefaultTempoBpm = 120.0` (Sheaf:include/synth/MasterClock.hpp). A said "application
  default" controllers and B said none; Froggers writes no instrument config at Init, so the Engine's
  `defaultInstrumentConfig_` is empty (Sheaf:include/synth/Engine.hpp).
- **LCH-02** `[SWEEP]` (A:LCH-02; B:S0) **Start:** LCH-01. **Action:** read every encoder in scene 1,
  then move the blend to scene 2 and read them again. **Result:** the default patch. VCO1/2/3 sit at
  110/220/330 Hz. In scene 1, Shape 1/2/3 sit at 0/0.5/1. In scene 2 they are mirrored to 1/0.5/0.
  Drive Gain is at 20% in both scenes. Ringmod 1–3 are at 0. VCO balance is centred. Attack 1–3 are at
  the floor. Sustain 1–3 are at 100%. R1 is at the floor, R2 at about 0.9 s and R3 at about 110 ms.
  Curve is at the bottom and Grace at 0. Comb FB and Comb drive are centred and Topology is at the
  bottom. Drive Phase is at 0.86, Anti-alias at the top, Feedback at 0, Fold centred, Tone at the top
  and Symmetry centred. Delay Send is at 0, FB drive centred, FB tone at the top, Mod rate centred
  (0.25 Hz), Width bal at the top and Crush at 0. Reverb Send is at 0 with Tank drive, Tilt and Tuned
  centred. Every Crispy and Crunchy is at 0. **CODE:** neither draft gave the scene-2 mirror or the
  Drive Gain overlay. Evidence: `ApplyAudioBankOverlay` and `ApplyDriveBankOverlay`
  (`app/FroggersModulation.hpp`). INV lists Shape 2 and 3 at 0.0 and Gain at 0.0; those are the
  registered defaults before the overlay (section 3.3).
- **LCH-03** `[SWEEP]` (A:LCH-03; B:A-02) **Start:** LCH-01. **Action:** open VCO1's, then VCO2's,
  then VCO3's modulation view. **Result:** each pitch carries two cross-VCO audio depths of one detent
  (0.01): VCO1 +1 from VCO2 Audio and VCO3 Audio, VCO2 −1 from VCO1 Audio and VCO3 Audio, and VCO3 +1
  from VCO1 Audio and VCO2 Audio. These are ordinary depths that can be removed. Every other depth
  reads neutral. **CODE** (A's OTR-13): `kAudioPitchDetents` and `kPlaceholderEncoderDetent`
  (`app/FroggersModulation.hpp`). The manual does not mention these depths (section 2.2, item
  9).
- **LCH-04** `[UX]` (A:LCH-04; B:T-01) **Start:** LCH-01. **Action:** press Play. **Result:** a chord
  at 110/220/330 Hz sounds, retriggered on every quarter note.

## 1.3 Surface, pages and navigation (S, B, P)

- **SUR-01** `[SWEEP]` (A:SUR-01; B:A-00) **Start:** LCH-01. **Action:** look at the window.
  **Result:** a band holds the VCO scope panels, the Play, Stop, Freeze and Record plates, Randomize
  All, Scene 1, Scene 2, the scene blend slider and the BPM slider beside it. The band has no Crunchy
  control. Below it are six page buttons, back and forward arrows, the 4×4 encoder grid, Randomize
  Page in the page header, Reset Page, and Reset All. The runtime sidebar holds Audio I/O,
  Controllers, Sync, File and the load readout. **Caps:** P shows no sidebar and a different
  transport row (PLG-04).
- **SUR-02** `[SWEEP]` (A:SUR-02; B:N-01) **Start:** running, Audio page. **Action:** press Audio,
  Envelope, Filter, Drive, Delay and Reverb in turn. **Result:** slots 0–13 hold that page's
  parameters, slot 14 its own Crispy and slot 15 the shared Crunchy. Exactly one page button is lit.
  The page's colour applies to the grid, and Crunchy keeps one fixed colour. The sound does not
  change, because every page processes audio all the time.
- **SUR-03** `[SWEEP]` (A:SUR-03; B:N-02) **Start:** Audio page. **Action:** press the forward arrow
  six times. **Result:** the pages go Audio, Envelope, Filter, Drive, Delay, Reverb and wrap to Audio.
- **SUR-04** `[SWEEP]` (A:SUR-04; B:N-03) **Start:** Audio page. **Action:** press the back arrow
  once. **Result:** the page wraps to Reverb.
- **SUR-05** `[SWEEP]` (A:SUR-05; B:N-06) **Start:** any page. **Action:** drag an encoder right and
  up, then left and down, leaving the knob mid-drag. **Result:** the value follows the drag
  (Sheaf spec sprs-4 gives `(dx − dy) × 0.0025` per pixel, with moves under 0.001 held until they add
  up). The ring shows the Crispy- and Crunchy-processed (fuego) value with no raw-value ghost. A drag
  keeps its knob until release.
- **SUR-06** `[UX]` (A:SUR-06; B:N-04) **Start:** any page. **Action:** read every label on all six
  pages. **Result:** a canonical short name (such as `A1`) renders short. A truncated short name
  renders in full (such as `Comb offset`). No label covers the ring.
- **SUR-07** `[SWEEP]` (A:SUR-07; B:N-08) **Start:** running. **Action:** watch the scope band.
  **Result:** each VCO panel shows a live trace.
- **SUR-08** `[SWEEP]` (A:SUR-08; B:N-05, FI-00, FI-04) **Start:** Filter page. **Action:** move Peak
  freq, Peak gain, Peak Q, Comb delay and Comb FB. **Result:** a frequency-response curve is drawn
  under each filter encoder behind a dimmed body. It follows the change and stays in frame at extreme
  Comb FB. The parameters are grouped: Peak (slots 0–2), Comb (3–7), Scoop (8–11), then Comb/Peak and
  Topology.
- **SUR-09** `[SWEEP]` (B:N-07) **Start:** running. **Action:** open Audio I/O, Controllers, Sync and
  File in turn, and press Back on each. **Result:** each page replaces the instrument view, and Back
  restores it with its state intact.

## 1.4 Transport, Freeze and tempo (S, B; P in 1.17)

- **TRN-01** `[SWEEP]` (A:TRN-01; B:T-01) **Start:** stopped. **Action:** press Play. **Result:** the
  gate is open for the first half of every quarter note and closed for the second half, and it
  retriggers all three envelopes. The Play plate shows its held colours.
- **TRN-02** `[SWEEP]` (A:TRN-03; B:T-02) **Start:** running. **Action:** drag BPM from 30 to 300 and
  back. **Result:** the pulse follows. The stepped Random S&H sources step at fixed multiples of the
  new quarter note, without drift or reset. Random S&H 6 retimes so one round spans 16 quarter notes.
  **Caps:** S and B 30–300 in steps of 1.0. P follows host tempo (PLG-06).
- **TRN-03** `[SWEEP]` (A:TRN-02; B:T-03) **Start:** running, R2 at the top (2.5 s), Grace raised.
  **Action:** press Stop. **Result:** all voices fade in about 50 ms whatever Release and Grace are.
  Delay and Reverb tails clear once every voice is silent, and the Play plate goes idle.
- **TRN-04** `[SWEEP]` (A:TRN-10; B:T-04) **Start:** running, after any number of Randomize All
  presses. **Action:** press Stop. **Result:** the output falls below audibility within the Stop
  bound.
- **TRN-05** `[SWEEP]` (A:TRN-02; B:T-05) **Start:** stopped after edits. **Action:** press Play.
  **Result:** every stored value is bit-identical to its value before the stop.
- **TRN-06** `[SWEEP]` (B:T-06) **Start:** stopped. **Action:** turn Delay FB drive, Reverb Tank drive,
  Filter Comb drive, Delay Freeze (slot 5) and Reverb Grit. **Result:** nothing is audible while
  stopped. Each knob keeps its value, and the value takes effect on Play.
- **TRN-07** `[SWEEP]` (B:T-07) **Start:** running. **Action:** press Play again. **Result:** the
  quarter-note grid restarts from 0 (the gate re-phases). **CODE, CHECK:** B said "not stated". The
  internal transport path sets `nextTransportQuarterNotes_ = 0.0` and increments `transportEpoch_` on
  every Start (`MasterClock` in Sheaf:src/MasterClock.cpp).

### Branch BR-03: Freeze engaged while running

- **TRN-08** `[SWEEP]` (A:TRN-04; B:T-FR-01) **Start:** running. **Action:** press Freeze.
  **Result:** the transport stops, the gate is held open and the delay keeps recirculating, so the
  instrument drones. The Play plate goes idle and the Freeze plate shows its latch. **CODE** (A's
  OTR-3): `audioAdsr_.setGate(gateOpen || FreezeLatched())` (`app/FroggersAppCore.hpp`).
- **TRN-09** `[SWEEP]` (A:TRN-05; B:T-FR-02) **Start:** TRN-08. **Action:** turn encoders on several
  pages. **Result:** the drone changes live.
- **TRN-10** `[SWEEP]` (A:TRN-06; B:T-FR-03) **Start:** TRN-08. **Action:** press Freeze again.
  **Result:** the latch releases and the transport runs as if Play were pressed.

### Branch BR-04: Freeze engaged while stopped

- **TRN-11** `[SWEEP]` (A:TRN-07; B:T-FS-01, T-FS-02) **Start:** stopped and silent. **Action:** press
  Freeze, then press it again. **Result:** the first press holds an audible drone even from silence,
  because the latch opens the gate. The second press silences the instrument within the Stop bound,
  and the transport stays stopped. **CODE:** A started from silence and B from ringing notes; the
  latch opens the gate in both cases (`FroggersUiSurface::HandleAction`, branch `kFreeze`,
  `freezeEngagedWhileTransportRunning_`).

### Rejoin

- **TRN-12** `[SWEEP]` (A:TRN-08, TRN-09; B:T-FR-04, T-FR-05) **Start:** frozen, from either branch.
  **Action:** press Stop, and in a second run press Play. **Result:** Stop disarms the latch and
  silences within the Stop bound. Play disarms the latch and runs. No sequence of Freeze and Stop
  leaves sound after Stop.

## 1.5 Recording (S, B; P has no Record)

- **REC-01** `[SWEEP]` (A:REC-01; B:REC-01) **Start:** stopped. **Action:** press Record. **Result:**
  it refuses, and "Press Play before recording." appears beneath the transport plates.
- **REC-02** `[SWEEP]` (A:REC-02; B:REC-02) **Start:** REC-01. **Action:** press Play. **Result:** the
  notice clears.
- **REC-03** `[SWEEP]` (A:REC-03; B:REC-03) **Start:** running. **Action:** press Record. **Result:**
  the Record plate shows armed and capture begins.
- **REC-04** `[SWEEP]` (A:REC-04; B:REC-04, REC-05) **Start:** REC-03, with no `YYYY-MM-DD.wav` in
  Documents. **Action:** press Record, then accept the dialog. **Result:** a "Save Recording" dialog
  opens in Documents on today's `YYYY-MM-DD.wav`. Accepting writes the file and shows "Recording
  saved." The file is 16-bit PCM mono (`EncodeWavPcm16Mono`, `app/FroggersAppCore.hpp`). **Caps:**
  B downloads the file instead (WEB-11). A second Record press with no captured frames queues nothing.
- **REC-05** `[SWEEP]` (A:REC-05; B:REC-06, REC-07) **Start:** REC-03, with today's file already
  present. **Action:** press Record, keep the name, confirm, and in a second run decline the warning.
  **Result:** a warning appears before the overwrite, and confirming replaces the file. **CHECK:** the
  JUCE `warnAboutOverwriting` native dialog decides what declining does; neither draft states it.
- **REC-06** `[SWEEP]` (A:REC-06; B:REC-08) **Start:** the REC-04 dialog is open. **Action:** press
  Cancel. **Result:** nothing is written, and that take is not offered again. **CODE:** the chooser
  callback returns on an empty result (`RegisterFileExportHandler`, `app/FroggersMain.cpp`).
- **REC-07** `[SWEEP]` (A:REC-07; B:REC-10) **Start:** REC-03. **Action:** let it run for 30 minutes.
  **Result:** the capture stops itself and is offered at once. The completion message carries a note
  that it stopped at the limit. **Caps:** S and B 30 minutes (`kMaxRecordSeconds = 30*60`), about
  345 MB of buffer at 48 kHz. **RULED** (OP).
- **REC-08** `[SWEEP]` (A:REC-08; B:REC-11) **Start:** a take stopped at the limit and not yet handed
  over. **Action:** press Record. **Result:** the stopped take is offered first, then the new take
  arms (`QueueTruncatedExportIfPending`).
- **REC-09** `[SWEEP]` (B:REC-12) **Start:** REC-03. **Action:** press Stop, and in a second run press
  Freeze. **Result:** capture continues, recording the silence or the drone, until Record is pressed
  or the limit is reached. **CODE, CHECK:** the `kStop` and `kFreeze` branches do not touch the
  recording state (`FroggersUiSurface::HandleAction`).

## 1.6 Parameter pages: the full range of every knob (S, B, P)

Unless a step says otherwise, **Start** is the default patch, running, with the step's page shown.
**Action** sweeps the knob from the bottom of its travel to the top. All 91 parameters are unipolar,
0.0–1.0 internally (INV §1).

### Audio page

- **AUD-00** `[SWEEP]` (A:AUD-00; B:AU-00) VCO1 (slot 0). **Result:** pitch sweeps 20 Hz to 5 kHz
  exponentially, from the 110 Hz default. **CODE** (A's OTR-1 and B's OTR-1): `Vco::kPitchMaxHz =
  5000.0f` (`app/dsp/Vco.hpp`).
- **AUD-01** `[SWEEP]` (A:AUD-01; B:AU-01) VCO2, as AUD-00, from 220 Hz.
- **AUD-02** `[SWEEP]` (A:AUD-02; B:AU-02) VCO3, as AUD-00, from 330 Hz.
- **AUD-03** `[SWEEP]` (A:AUD-03; B:AU-03) Shape 1. **Result:** VCO1 morphs continuously from sine to
  saw at the middle to square at the top, and the scope shows the change. Only VCO1 changes.
- **AUD-04** `[SWEEP]` (A:AUD-04; B:AU-04) Shape 2, as AUD-03 on VCO2.
- **AUD-05** `[SWEEP]` (A:AUD-05; B:AU-05) Shape 3, as AUD-03 on VCO3.
- **AUD-06** `[SWEEP]` (A:AUD-06; B:AU-06) Ph.mod 1. **Result:** nothing below a small floor, then
  vibrato that grows to an FM-like wobble from VCO1's own LFO. The swing size does not depend on PM
  rate, and only VCO1 moves.
- **AUD-07** `[SWEEP]` (A:AUD-07; B:AU-07) Ph.mod 2, as AUD-06 on VCO2.
- **AUD-08** `[SWEEP]` (A:AUD-08; B:AU-08) Ph.mod 3, as AUD-06 on VCO3.
- **AUD-09** `[SWEEP]` (A:AUD-09; B:AU-09) Ringmod 1. **Result:** exactly off below the floor (0.05).
  Raising the knob does two things at once: it raises the ring amount until the ring product fully
  replaces VCO1's dry tone at the top, and it sweeps VCO1's own internal carrier from 20 Hz to 5 kHz.
  No other VCO is read. **CODE** (A's OTR-15): in `Vco::Process`, one knob feeds both
  `RingModDepthScale` and `RingModPhaseIncrement` (`kRingModMinHz` 20 to `kRingModMaxHz` 5000).
- **AUD-10** `[SWEEP]` (A:AUD-10; B:AU-10) Ringmod 2, as AUD-09 on VCO2.
- **AUD-11** `[SWEEP]` (A:AUD-11; B:AU-11) Ringmod 3, as AUD-09 on VCO3.
- **AUD-12** `[SWEEP]` (A:AUD-12; B:AU-12) PM rate, 2 Hz to 20 Hz. **Start:** Ph.mod 1 at mid, Ph.mod 2
  and 3 at 0. **Result:** VCO1's wobble speeds up with the same swing size (within a few percent). The
  minimum still cycles audibly. VCO2 and VCO3 stay unmodulated. **RULED** (HR, PM floor set to 2 Hz).
- **AUD-13** `[SWEEP]` (A:AUD-13; B:AU-13) VCO balance. **Result:** emphasis moves from VCO1 through
  an equal three-way split at the centre (the default) to VCO3. Each VCO's weight stays between 10%
  and 80%, and the weights sum to 1.

### Envelope page

- **ENV-00** `[SWEEP]` (A:ENV-00, ENV-04, ENV-08; B:EN-00/04/08) A1, A2 and A3 (slots 0, 4 and 8), 1 ms
  to 250 ms exponentially, from the floor default. **Result:** only that VCO's rise time changes, by
  equal ratios per equal move.
- **ENV-01** `[SWEEP]` (A:ENV-01, ENV-05, ENV-09; B:EN-01/05/09) D1, D2 and D3 (slots 1, 5 and 9), 5 ms
  to 1 s. **Result:** the fall to Sustain follows.
- **ENV-02** `[SWEEP]` (A:ENV-02, ENV-06, ENV-10; B:EN-02/06/10) S1, S2 and S3 (slots 2, 6 and 10), 25%
  to 100%. **Result:** the held level follows and never goes below 25%, even under modulation.
- **ENV-03** `[SWEEP]` (A:ENV-03, ENV-07, ENV-11; B:EN-03/07/11) R1, R2 and R3 (slots 3, 7 and 11),
  5 ms to 2.5 s. **Result:** the tail follows. Stop still fades in about 50 ms.
- **ENV-12** `[SWEEP]` (A:ENV-12; B:EN-12) Curve. **Result:** every Attack, Decay and Release ramp
  morphs from straight (bit-identical to linear at the bottom) to the analog shape. Every stage
  completes within about 2.5× its linear time, and at most 2.75× once quantisation is counted.
  Sustain is unaffected. **CODE:** A wrote 2.75× and B wrote 2.5×. Both come from the same sentence in
  `froggers-sheaf-parameter-model` ("two and a half times ... no more than 2.75").
- **ENV-13** `[SWEEP]` (A:ENV-13; B:EN-13) Grace, 0 to 1 s, with a true zero. **Start:** A1 and D1 long,
  BPM high. **Result:** at 0, Release starts the moment the gate closes. When raised, a note that has
  reached Sustain holds at least this long. Attack, Decay and Release lengths do not change, and Stop
  ignores Grace.

### Filter page

- **FIL-00** `[SWEEP]` (A:FIL-00; B:FI-00) Peak freq, 100 Hz to 20 kHz. **Result:** the peak moves.
- **FIL-01** `[SWEEP]` (A:FIL-01; B:FI-01) Peak gain. **Result:** flat at the bottom. At page
  defaults, a 100 Hz tone holds within 0.0000097 dB, 1 kHz falls 5.27 dB, 5 kHz falls 5.75 dB, and a
  broadband source loses 6.72 dB.
- **FIL-02** `[UX]` (A:FIL-02; B:FI-02) Peak Q. **Result:** a wide bump becomes a narrow ringing
  resonance.
- **FIL-03** `[SWEEP]` (A:FIL-03; B:FI-03) Comb offset, 1 ms to 100 ms. **Result:** the comb's attack
  smears, and its pitch does not change.
- **FIL-04** `[SWEEP]` (A:FIL-04; B:FI-04) Comb delay, 100 Hz to 10 kHz. **Result:** the comb's ring
  pitch follows.
- **FIL-05** `[SWEEP]` (A:FIL-05; B:FI-05) Comb FB, bipolar, from the centre to each end. **Result:**
  no resonance at the centre, rising to ±0.95 at the ends. Ring time grows by equal ratios per step,
  and the ring always decays.
- **FIL-06** `[UX]` (A:FIL-06; B:FI-06) Comb LP, up to 20 kHz. **Result:** turning down darkens and
  shortens the repeats; turning up brightens and sustains them.
- **FIL-07** `[SWEEP]` (A:FIL-07; B:FI-07) Comb drive, 0.25× to 4×, unity at the centre. **Result:**
  above the centre the comb saturates harder. Below the centre it can ring louder, and the page
  limiter holds the level.
- **FIL-08** `[SWEEP]` (A:FIL-08; B:FI-08) Scoop mix. **Result:** no notch at the bottom, rising to a
  notched signal that fully replaces the input at the top.
- **FIL-09** `[SWEEP]` (A:FIL-09; B:FI-09) Scoop freq, 100 Hz to 20 kHz, independent of Peak freq.
- **FIL-10** `[UX]` (A:FIL-10; B:FI-10) Scoop width, independent of Peak Q.
- **FIL-11** `[SWEEP]` (A:FIL-11; B:FI-11) Scoop depth, from no dip to about a 95% dip.
- **FIL-12** `[SWEEP]` (A:FIL-12; B:FI-12) Comb/Peak, an equal-power blend. **Result:** the held-back
  branch sits about −22 dB at each end, and each branch is at −3 dB at the centre.
- **FIL-13** `[SWEEP]` (A:FIL-13; B:FI-13) Topology. **Result:** parallel at the bottom, series at the
  top, and continuous in between.
- **FIL-14** `[SWEEP]` (B:FI-15) **Start:** Comb/Peak at the comb end, Comb FB at max, Comb drive at
  the bottom. **Action:** play a tone at the comb pitch. **Result:** the one limiter after the blend
  holds the level.

### Drive page

- **DRV-00** `[SWEEP]` (A:DRV-00; B:DR-00) Wet/Dry, 0 to 1. **Result:** at 0 the output matches the
  input sample for sample; at 1 it is fully wet. The crossfade is equal-power with a worst dip of
  about 1 dB.
- **DRV-01** `[SWEEP]` (A:DRV-01; B:DR-01) Gain, 1× to 5×, at Wet/Dry 1. **Result:** distortion grows
  denser, and Gain alone distorts with every mangler at its floor.
- **DRV-02** `[UX]` (A:DRV-02; B:DR-02) Shape. **Result:** the harmonic character changes
  continuously.
- **DRV-03** `[SWEEP]` (A:DRV-03; B:DR-03) SRR 1. **Result:** off at the bottom, then heavier
  decimation, at any Gain.
- **DRV-04** `[SWEEP]` (A:DRV-04; B:DR-04) SRR 2. **Result:** a second decimation stage after SRR 1.
- **DRV-05** `[SWEEP]` (A:DRV-05; B:DR-05) XOR. **Result:** no flip at 0. Between about 0.3 and 0.7,
  about 16 dB below 1 kHz is removed while the level barely moves. A setting and its mirror differ
  only in polarity.
- **DRV-06** `[SWEEP]` (A:DRV-06; B:DR-06) Bit depth. **Result:** untouched at 0; the first audible
  step comes within the first hundredth of travel.
- **DRV-07** `[SWEEP]` (A:DRV-07; B:DR-07) Fuzz. **Result:** folder at the bottom, saturator at the
  top, with the held-back path about −22 dB at each end. With Gain raised, the top reads cleaner.
- **DRV-08** `[SWEEP]` (A:DRV-08; B:DR-08) Phase. **Result:** no effect at Wet/Dry 0. At partial
  Wet/Dry on a bass note, the top lifts the sum by a couple of dB, and the change is progressive.
- **DRV-09** `[SWEEP]` (A:DRV-09; B:DR-09) Anti-alias, from the top (grit) to the bottom (clean).
  **Result:** about 23 dB cleaner at 1.5 kHz, with no change on a bass note and about 2.5 dB of
  fundamental change.
- **DRV-10** `[SWEEP]` (A:DRV-10; B:DR-10) Feedback. **Result:** a plain fold at 0; raised, the fold
  resonates and the ring always dies.
- **DRV-11** `[SWEEP]` (A:DRV-11; B:DR-11) Fold. **Result:** the fold divisor goes from 16× down to 1×.
  Every position folds, and the bottom folds least. At maximum Fuzz the fold is quieter but every
  position still changes the sound. **CODE** (A's OTR-2): `SetFold` maps with `ExpMapCompute(16, 1, ·)`
  (`app/dsp/Drive.hpp`), so there is no true zero.
- **DRV-12** `[SWEEP]` (A:DRV-12; B:DR-12) Tone, from the top (open) to about 800 Hz at the bottom.
- **DRV-13** `[SWEEP]` (A:DRV-13; B:DR-13) Symmetry, bipolar. **Result:** the two halves of the wave
  skew opposite ways. The effect is largest with Fuzz low. Silence in gives silence out. The DC
  offset stays under 0.02 at page defaults and reaches about ¾ of full scale at some
  Gain/Shape/Fold settings.

### Delay page

- **DLY-00** `[SWEEP]` (A:DLY-00; B:DE-00, DE-00b) Wet/dry. **Result:** with Send at 0 it has no
  effect. With Send fed, more of the delay reaches Reverb, and the dry signal stays at 30% or more.
- **DLY-01** `[SWEEP]` (A:DLY-01; B:DE-01) Send. **Result:** 0 is an exact bypass. Turning Send to 0
  while echoes sound fades them over about 100 ms.
- **DLY-02** `[SWEEP]` (A:DLY-02; B:DE-02) Delay time, about 1 ms to 2 s, exponentially.
- **DLY-03** `[SWEEP]` (A:DLY-03; B:DE-03) Feedback, up to 98%. **Result:** repeats always die out.
- **DLY-04** `[SWEEP]` (A:DLY-04; B:DE-04) Stereo width. **Result:** at 0, left and right are
  identical. Raising it delays the right tap further, and left/right correlation falls. Near the top
  of Delay time the spread shrinks.
- **DLY-05** `[SWEEP]` (A:DLY-05; B:DE-05) Freeze (slot 5). **Result:** ordinary feedback at 0; at the
  top the loop is lossless and takes in nothing new.
- **DLY-06** `[UX]` (A:DLY-06; B:DE-06) Mod depth. **Result:** a chorus or vibrato on the repeats.
- **DLY-07** `[UX]` (A:DLY-07; B:DE-07) Reverse. **Result:** forward at 0, fully reversed at the top,
  with no click.
- **DLY-08** `[SWEEP]` (A:DLY-08; B:DE-08) Diffusion. **Result:** an exact bypass at 0, then blurred
  attacks.
- **DLY-09** `[SWEEP]` (A:DLY-09; B:DE-09) FB drive, 0.25× to 4×, unity at the centre. **Result:** the
  ceiling holds.
- **DLY-10** `[SWEEP]` (A:DLY-10; B:DE-10) FB tone, from open down to about 800 Hz. **Result:** each
  pass is darker than the last.
- **DLY-11** `[SWEEP]` (A:DLY-11; B:DE-11) Mod rate, 0.05 Hz to 1.25 Hz, with 0.25 Hz at the centre.
- **DLY-12** `[SWEEP]` (A:DLY-12; B:DE-12) Width bal. **Start:** Stereo width raised. **Result:** full
  spread at the top, none at the bottom. The knob scales only the time-offset spread, and the
  cross-feed is fixed at 0. **CODE** (A's OTR-14 and B's OTR-12): `SetWidthBalance` and `widthSpreadRaw`
  (`app/dsp/Delay.hpp`).
- **DLY-13** `[SWEEP]` (A:DLY-13; B:DE-13) Crush. **Result:** off at 0; the repeats are crushed and
  the dry signal never is.

### Reverb page

- **REV-00** `[SWEEP]` (A:REV-00; B:RV-00, RV-00b) Wet/dry. **Result:** no effect with Send at 0. When
  fed, the dry signal stays at 30% or more.
- **REV-01** `[SWEEP]` (A:REV-01; B:RV-01) Send. **Result:** 0 is an exact bypass.
- **REV-02** `[UX]` (A:REV-02; B:RV-02) Room size. **Result:** a larger space.
- **REV-03** `[SWEEP]` (A:REV-03; B:RV-03) Decay, tank feedback from 0.1 to 0.98.
- **REV-04** `[SWEEP]` (A:REV-04; B:RV-04) Pre-delay, about 0.02 ms to about 85 ms at 48 kHz.
  **Result:** at the top, the hit and the tail are heard as two events, and no two adjacent positions
  sound the same.
- **REV-05** `[SWEEP]` (A:REV-05; B:RV-05) Damping, from about 1.7 kHz at the bottom to a floor near
  150 Hz. **Result:** about 10 dB quieter on bright broadband material and about 0.5 dB on a low sine.
  The stereo image is unchanged.
- **REV-06** `[SWEEP]` (A:REV-06; B:RV-06) Stereo width. **Result:** left/right correlation falls.
- **REV-07** `[UX]` (A:REV-07; B:RV-07) Density. **Result:** an exact bypass at 0; the first quarter of
  travel already makes the repeats audible.
- **REV-08** `[UX]` (A:REV-08; B:RV-08) Mod. **Result:** a 0.35 Hz wow.
- **REV-09** `[SWEEP]` (A:REV-09; B:RV-09) Hold. **Result:** the tail approaches self-oscillation but
  never reaches it.
- **REV-10** `[SWEEP]` (A:REV-10; B:RV-10) Tank drive, 0.25× to 4×, unity at the centre.
- **REV-11** `[UX]` (A:REV-11; B:RV-11) Grit. **Result:** off at 0, then grit in the tail.
- **REV-12** `[SWEEP]` (A:REV-12; B:RV-12) Tilt, bipolar around about 1 kHz, with no change at the
  centre.
- **REV-13** `[SWEEP]` (A:REV-13; B:RV-13) Tuned, ±300 samples, with 0 at the centre.

### Crispy and Crunchy (fuego)

- **FUE-01** `[SWEEP]` (A:FUE-01..06; B:CR-01) Crispy (slot 14) on each of the six pages, 0 to max,
  then switch pages. **Result:** nothing changes at 0. As it rises, that page's 14 values snap between
  islands, with each slot scrambling differently, and the rings show the scrambled values. No other
  page and not Crunchy change. At max the result is finite and fixed. Each Crispy takes its page's
  colour.
- **FUE-02** `[SWEEP]` (A:FUE-07; B:CR-02) Crunchy (slot 15), 0 to max, from any page. **Result:**
  every page parameter and every Crispy is scrambled, and Crunchy's own value never is. The same
  Crunchy value shows on every page.
- **FUE-03** `[SWEEP]` (A:FUE-08) **Start:** Audio Crispy at mid. **Action:** raise Crunchy.
  **Result:** the Audio Crispy ring moves to its scrambled value.

## 1.7 Modulation: one full drill-down (S, B, P)

Main has three modulation levels. Level 1 is a parameter's view, level 2 is a depth's own view, and
level 3 is that depth's depth. A fourth is refused. **CODE:** both drafts followed
`froggers-modulation-slate` ("capped at two levels"; Back "exits to the parameter grid"). Evidence:
`FroggersModulationDrillIn::kMaxDrillLevel = 3` and `Back()` (`app/FroggersModulation.hpp`), and
the tests `fourth_level_drill_in_is_refused` and
`back_from_level_two_returns_to_the_same_level_one_parameter_then_back_again_exits_to_grid`
(`app/FroggersModulationTests.cpp`). The test comment reads "kMaxDrillLevel moved from 2 to 3".

- **MOD-01** `[SWEEP]` (A:MOD-01; B:M-01) **Start:** Audio page, running. **Action:** click VCO1's
  encoder once. **Result:** VCO1's modulation view replaces the grid. Row 1 holds Random S&H 1–4. Row 2
  holds S&H 5–6, VCO1 Audio and VCO2 Audio. Row 3 holds VCO3 Audio and VCO1–3 EF. Row 4 holds Noise,
  External Audio, External Audio EF and Back. The arrows disappear and a level title shows. Stepped
  S&H cells show the loop visualizer, and S&H 6 shows the ganged-random visualizer. **CODE** (A's
  OTR-12): a single click drills in, because the cell's `action` is `kEncoderPress`, not a
  double-click action (`AppendEncoderCell`, `app/FroggersUiSurface.hpp`).
- **MOD-02** `[SWEEP]` (A:MOD-02; B:M-02) **Start:** MOD-01, no input. **Action:** click and drag
  External Audio and External Audio EF. **Result:** both are grey with no readout and no arc, and
  nothing happens. Their outputs hold at 0.5 and 0.0.
- **MOD-03** `[SWEEP]` (A:MOD-03..15; B:M-03, M-15, M-16) **Start:** MOD-01. **Action:** turn each of
  the 13 connected depths from 0 to −1 to +1 and back to 0. **Result:** each source is off at 0 and
  pushes VCO1's pitch in the matching direction. The rings show the reachable range, and VCO1 carries
  a badge after Back. Each source's character: S&H 1 steps three times per quarter, nearly always to
  an extreme, instantly. S&H 2 steps twice per quarter across five levels with about 5 ms smoothing.
  S&H 3 steps once per quarter across eight values with about 20 ms. S&H 4 steps once per two quarters
  over a four-bar phrase with about 100 ms. S&H 5 steps once per four quarters over an eight-bar phrase
  with about 200 ms. S&H 6 glides near the centre every 16 quarters. VCO Audio is the raw signal at
  audio rate. VCO EF rises fast and falls slower. Noise is broadband and redraws every frame.
- **MOD-04** `[SWEEP]` (A:MOD-16; B:M-03) **Start:** S&H 3 and VCO2 Audio both at +0.8. **Result:** the
  depths sum. Above an absolute sum of 1 they are scaled down together.
- **MOD-05** `[SWEEP]` (A:MOD-17; B:M-04) **Start:** MOD-04. **Action:** click the S&H 3 depth cell.
  **Result:** level 2 opens with the same layout.
- **MOD-06** `[SWEEP]` (A:MOD-18; B:M-05) **Start:** MOD-05. **Action:** turn S&H 5's depth from −1 to
  +1. **Result:** the level-1 depth now moves with S&H 5.
- **MOD-07** `[SWEEP]` (A:MOD-19; B:M-06) **Start:** MOD-06. **Action:** click the S&H 5 depth cell.
  **Result:** level 3 opens. **CODE** (the drafts said "refused").
- **MOD-08** `[SWEEP]` (new from CODE; A:MOD-19; B:M-06) **Start:** MOD-07. **Action:** click any depth
  cell. **Result:** it is refused; level 3 stays and the selection is unchanged.
- **MOD-09** `[SWEEP]` (A:MOD-20; B:M-07, M-08) **Start:** MOD-07. **Action:** click Back, three times.
  **Result:** each press goes up one level: to level 2, to level 1 on the same parameter, then to the
  grid. Clicking the parameter's own encoder in the Back cell does the same. **CODE** (the drafts said
  one Back exits to the grid).
- **MOD-10** `[SWEEP]` (A:MOD-22; B:M-09) **Start:** a view at any level. **Action:** press a different
  page button, and in a second run press the current page's button. **Result:** a different page
  closes the view and shows that page's grid. The current page's button exits to its grid from any
  level (`FroggersCommand` and `ApplyAppCommand` handling, `app/FroggersAppCore.hpp`). The on-screen
  arrows are not shown in a view. A mapped Page Previous or Page Next does nothing in a view
  (`ApplyAppCommand`'s gate on the drill level).
- **MOD-11** `[SWEEP]` (A:MOD-21, MOD-23; B:M-10) **Start:** Filter page. **Action:** drill into this
  page's Crispy, then into Crunchy, set a depth, go Back, switch to Reverb, and drill into Crunchy.
  **Result:** it is the same Crunchy with the same depth. While any view is open, Crunchy's knob
  cannot be reached, because slot 15 is Back.
- **MOD-12** `[SWEEP]` (A:MOD-24; B:M-11) **Start:** an input connected (IO-04, WEB-09 or PLG-07).
  **Action:** turn External Audio and External Audio EF from −1 to +1. **Result:** the cells are
  coloured. External Audio modulates at audio rate, and the EF follows the input level.

## 1.8 Randomize and Reset (S, B, P)

- **RND-01** `[SWEEP]` (A:RND-01; B:R-01) The one level-1 randomization. **Start:** a parameter page.
  **Action:** press Randomize All. **Result:** every page parameter on all six pages is redrawn
  uniformly. Level-1 depths are cleared, then redrawn from a floor of 0: 0 sources 50%, 1 25%, 2 12.5%,
  3 6.25%, 4 3.125%, 5 or more 3.125%. The sources are distinct and connected only, and no level-2
  depths are drawn. Parameters with no depth carry no badge. The Crispy of 0, 1 or 2 pages changes
  (`kMaxRandomizedCrispy = 2`), and Crunchy never does. The five stepped S&H sources reseed.
  **Caps:** see section 4, depth storage.
- **RND-02** `[SWEEP]` (A:RND-02) **Start:** RND-01. **Action:** press it again. **Result:** the new
  draw replaces the old; nothing accumulates.
- **RND-03** `[SWEEP]` (A:RND-03; B:R-02) **Start:** a parameter page. **Action:** press Randomize
  Page. **Result:** that page's 14 values and its Crispy change. No depths, no other page and no
  Crunchy change, and the S&H phrases stay.
- **RND-04** `[SWEEP]` (A:RND-04; B:M-13) **Start:** level 1, 2 or 3. **Action:** press Randomize
  Page. **Result:** only that view's depths change, drawn from a floor of 0.
- **RND-05** `[SWEEP]` (A:RND-05; B:M-12) **Start:** level 1. **Action:** press Randomize All.
  **Result:** that parameter's depths are redrawn from a floor of 1 (1 source 50%, 2 25%, 3 12.5%, 4 or
  more about 1 in 16). Each depth that is modulating also gets its own depths drawn from a floor of 1.
  Every press moves something.
- **RND-06** `[SWEEP]` (A:RND-06; B:M-14) **Start:** level 2, then level 3. **Action:** press
  Randomize All. **Result:** at level 2, same as RND-05 one level down: floor 1, and the next level is
  drawn. At level 3, floor 1 with no further level. **CODE** (A's OTR-4 and B's OTR-2):
  `RandomizeAll`, whose `Level() >= 1` branch uses `minimumSources = 1` and descends while `Level() <
  kMaxDrillLevel`.
- **RND-07** `[SWEEP]` (A:RND-07; B:M-12) **Start:** no input. **Action:** press Randomize All 50
  times. **Result:** the External sources are never drawn.
- **RST-01** `[SWEEP]` (A:RST-01; B:R-03) **Start:** after RND-01 and edits to depths, Crispy and
  Crunchy. **Action:** press Reset All on a parameter page. **Result:** the whole state equals LCH-02
  and LCH-03 in both scenes, including which depths exist, with every Crispy and Crunchy at 0.
  **RULED** (HR, "Reset All is global").
- **RST-02** `[SWEEP]` (A:RST-02; B:R-04) **Start:** every page edited. **Action:** press Reset Page on
  Audio. **Result:** the Audio page returns to its default patch, including shapes, the six detents
  and its Crispy. No other page changes.
- **RST-03** `[SWEEP]` (A:RST-03; B:R-05) **Start:** a view with depths set. **Action:** press Reset
  Page. **Result:** the selected parameter's depths return to default: neutral, except the Audio
  pitch detents.
- **RST-04** `[SWEEP]` (A:RST-04; B:R-05) **Start:** a view at level 1. **Action:** press Reset All.
  **Result:** the selected parameter's depths return to default, and the next level down under each
  of them is zeroed. Nothing outside that parameter changes. **RULED** (SR R1, option a: keep main).
  The manual does not describe this yet: **fix required** (MANUAL, section 5).

## 1.9 Scenes and tempo (S, B; P in 1.17)

- **SCN-01** `[SWEEP]` (A:SCN-01; B:SC-01) **Start:** the two scenes differ (the default shapes
  differ). **Action:** drag the scene blend slider end to end. **Result:** every parameter
  interpolates, and the rings follow. The slider reads 1.0 to 2.0 (`kSceneBlendDisplayOffset = 1.0`,
  `app/FroggersUiSurface.hpp`).
- **SCN-02** `[SWEEP]` (A:SCN-02; B:SC-02) **Start:** the blend at one end, then mid-way. **Action:**
  turn a knob. **Result:** at an end, only that scene changes. Mid-way, the edit is shared.
- **SCN-03** `[SWEEP]` (A:SCN-03; B:SC-03) **Start:** any blend. **Action:** press Scene 2, then
  Scene 1. **Result:** Scene 2 moves the blend to 1 (the slider reads 2.0), and Scene 1 moves it to 0
  (the slider reads 1.0). **CODE** (A's OTR-11 and B's OTR-10): `HandleAction` `kSceneSelect` pushes
  `SetSceneBlend(sceneIx == 0 ? 0 : 1)`. `FroggersParameters.hpp` records this as a deliberate
  departure from Braid 4's spm-17 behaviour.
- **BPM-01** `[SWEEP]` (A:BPM-01) **Start:** stopped. **Action:** set 30, then 300, then try beyond
  both. **Result:** the value stays within 30–300 (`kFroggersBpmMin`/`kFroggersBpmMax`, step 1.0).

## 1.10 Audio I/O page (S; B in 1.18; P has none)

- **IO-01** `[UX]` (A:IO-01) **Action:** open Audio I/O. **Result:** it is titled "Audio I/O" and the
  sidebar stays.
- **IO-02** `[SWEEP]` (A:IO-02; B:IO-01) **Action:** pick each output, including System Default.
  **Result:** audio moves to that device, and the page shows it with its status. B adds, from
  sar-15, that the transport restarts and the Play plate follows. **CHECK** that restart.
- **IO-03** `[SWEEP]` (A:IO-03; B:IO-02) **Start:** fresh install. **Action:** read Input device.
  **Result:** No Input is selected, no device is open, the External sources are disconnected, and
  named devices are listed. There is no "system provides" choice.
- **IO-04** `[SWEEP]` (A:IO-04; B:IO-03) **Action:** pick a named input. **Result:** it opens one
  channel, and the External sources connect (MOD-12).
- **IO-05** `[SWEEP]` (A:IO-05; B:IO-04) **Action:** pick No Input. **Result:** the device closes and
  the sources go inert and grey.
- **IO-06** `[UX]` (A:IO-06; B:IO-05) **Start:** capture failing. **Action:** press Retry Input.
  **Result:** capture retries, and output keeps running.
- **IO-07** `[SWEEP]` (A:IO-07; B:IO-06) **Action:** press Back. **Result:** the runtime configuration
  is saved (`RuntimePageBackSavesConfiguration`).
- **IO-08** `[UX]` (A:IO-08; B:Q-06) **Start:** the saved output is unplugged. **Action:** relaunch.
  **Result:** the default device runs, and the status names the missing device.
- **IO-09** `[SWEEP]` (B:Q-09) **Start:** a configuration from before No Input existed, naming an
  input. **Action:** launch. **Result:** Input reads No Input, and the output choice is kept.

## 1.11 Sync page (S, B)

- **SYN-01** `[SWEEP]` (A:SYN-01; B:SY-01) **Start:** fresh install. **Action:** open Sync.
  **Result:** four toggles are shown, **Send clock**, **Receive clock**, **Send transport** and
  **Receive transport**, all off, with PPQN at 24. Read-only BPM, lock state, source, output latency
  and the ignored, late and dropped counters are shown. **CODE** (A's OTR-9 and B's OTR-8): the labels
  are at `RuntimePages.hpp` 858, 863, 868 and 873. The manual names only the two receive toggles
  (section 2.2).
- **SYN-02** `[SWEEP]` (A:SYN-02; B:SY-02) **Action:** type PPQN 1, 960, 0 and 961. **Result:** 1 and
  960 are accepted. 0 and 961 are refused inline and the old value is kept. Any value other than 24
  shows a note that peers must match.
- **SYN-03** `[SWEEP]` (A:SYN-03; B:T-MC-01, T-MC-02) **Start:** a controller row's MIDI input carries
  Timing Clock. **Action:** turn Receive clock on, then drag BPM. **Result:** the state reaches Locked
  with the controller's name and the recovered BPM. BPM becomes "BPM `<value>` (external clock)" and
  ignores drags. The S&H sources step on external quarter notes.
- **SYN-04** `[SWEEP]` (A:SYN-04; B:T-MC-03) **Action:** stop the clock. **Result:** after the timeout
  the state is FreeRun at the last tempo, and the transport keeps running.
- **SYN-05** `[SWEEP]` (A:SYN-06; B:T-MC-04, T-MC-05) **Action:** with Receive transport on, send Start
  and then Stop; repeat with it off. **Result:** on, Start arms the transport, the next clock starts
  it, Stop stops it, and the plate follows. Off, both are ignored, and the on-screen Play and Stop
  still work.
- **SYN-06** `[SWEEP]` (A:SYN-06; B:SY-04) **Action:** turn on Send clock and Send transport.
  **Result:** clock and transport bytes go out to the open controller outputs. **CODE:** the controls
  ship (see SYN-01).
- **SYN-07** `[SWEEP]` (A:SYN-05, SYN-07; B:T-MC-06, Q-07) **Action:** turn Receive clock off, press
  Back, quit and relaunch. **Result:** the last manual BPM is editable again, and the sync settings
  persist in the runtime configuration, not in patches.

## 1.12 Controllers page: common to every row (S, B; P has none)

- **CTL-01** `[SWEEP]` (A:CTL-01; B:C-01) **Start:** nothing connected. **Action:** open Controllers.
  **Result:** "No connected controller is waiting to be set up" is shown. The add row has a
  **Preset** selector (MIDI Fighter Twister, Akai APC40 mkII (Generic), Akai APC40 mkII (Ableton),
  Launchpad X, Launchpad Pro MK3, Launchpad Mini MK3, Custom) and **Add**. A legend names the online,
  offline and not-set dots. **RULED** (MR 2026-09-01 and 2026-09-16: the dropdown lists the devices
  plus Custom and replaces Reconfigure; 2026-09-23: WRLD.Bldr removed). Main still offers WRLD.Bldr:
  **fix required** (section 3.1, D-01).
- **CTL-02** `[SWEEP]` (A:CTL-02; B:C-02) **Start:** a recognised device connected and unused.
  **Result:** "Available controllers" lists it with every matching preset (an APC40 shows both).
  Unrecognised ports and half-present devices appear under "Other inputs" and "Other outputs". The
  selector starts on the first waiting device's first preset.
- **CTL-03** `[SWEEP]` (A:CTL-03) **Start:** CTL-02 with the page closed. **Result:** the sidebar's
  Controllers entry shows a warning marker until the device is used (`kSidebarControllersWarning`).
  The manual is silent (section 2.2).
- **CTL-04** `[SWEEP]` (A:CTL-04; B:C-03) **Action:** press Add without touching the selector.
  **Result:** a row with the preset's full mapping is added, its ports are bound, every section is
  open, and the marker clears.
- **CTL-05** `[SWEEP]` (B:C-04) **Start:** no matching device. **Action:** pick a preset and press
  Add. **Result:** the row's ports read "(none)".
- **CTL-06** `[SWEEP]` (A:CTL-05; B:C-03) **Action:** add the same preset again. **Result:** the row is
  named with the smallest free suffix (for example "MIDI Fighter Twister 2").
- **CTL-07** `[SWEEP]` (A:CTL-06; B:C-05) **Action:** read a row. **Result:** line 1 shows the
  disclosure arrow, the name and the device label (the preset's name, else the bound MIDI input, else
  "(none)"). A Launchpad row also shows a **Model** selector (captioned "Variant" on main, the label
  defect section 2.1 e rules this audit fixes). Line 2 shows dots before **MIDI in**
  and **MIDI out**, then **Delete**. **RULED** (MR 2026-09-16: the device label replaces Kind).
- **CTL-08** `[SWEEP]` (A:CTL-07; B:C-06) **Action:** open the editor, type in **Name** and press
  **Rename**. **Result:** the row is renamed with its sections still open. A duplicate name is
  refused.
- **CTL-09** `[SWEEP]` (A:CTL-08; B:C-10) **Action:** edit a mapping on a preset row, then set it back
  by hand. **Result:** a third line appears, "This row differs from its preset. Restore replaces its
  mappings and discards any edits.", with **Restore**. It disappears when the mapping is set back.
- **CTL-10** `[SWEEP]` (A:CTL-09; B:C-10) **Action:** diverge the row again and press Restore.
  **Result:** the preset's mappings return, and the name and ports stay.
- **CTL-11** `[SWEEP]` (A:CTL-10; B:C-08) **Action:** type CC 200. **Result:** "Refused: last cc must
  be an integer 0-127", and the field reverts.
- **CTL-12** `[SWEEP]` (A:CTL-11, CTL-12; B:C-09) **Action:** set a block's end below its start, or a
  grid X/Y outside the device. **Result:** the value stays and "Warning: " plus the reason appears. It
  is not saved until fixed. The status line does not name the row.
- **CTL-13** `[SWEEP]` (A:CTL-15; B:C-07) **Action:** pick devices in MIDI in and MIDI out.
  **Result:** they are stored and opened.
- **CTL-14** `[SWEEP]` (A:CTL-13; B:C-07) **Action:** unplug, then replug. **Result:** the dots go
  offline, and after replugging the row reconnects within one poll and resends its feedback.
- **CTL-15** `[SWEEP]` (A:CTL-16; B:C-11) **Action:** press Delete. **Result:** the row goes, its
  ports close, and the device returns to "Available controllers".
- **CTL-16** `[SWEEP]` (A:CTL-17; B:C-12) **Start:** an old configuration holding a released row.
  **Result:** the row shows its name, device, a **Released** badge, its stored ports and Delete. It
  has no arrow, editor or rename. **RULED** (MR 2026-09-01: no Reconfigure, Blacklist, Configure,
  Ignore or wizard). Main has none of those actions (`Actions` list in
  `Sheaf:include/synth/ControllersPageUI.hpp`), and it renders blacklisted records with the Released
  badge.
- **CTL-17** `[SWEEP]` (A:CTL-18; B:Q-05) **Action:** edit, then quit without pressing Back and
  without saving a patch, and relaunch. **Result:** the setup is as the page last had it. **RULED**
  (MR 2026-09-18). **CODE:** `RuntimePageBackSavesConfiguration` excludes Controllers because "Controllers
  saves every committed edit as it is made".
- **CTL-18** `[SWEEP]` (A:HLD-01; B:C-13) **Start:** a row with Hold Drill mapped. **Action:** hold it,
  turn knob A twice and knob B once, release, then turn an absolute knob. **Result:** A and B each
  drill in once per hold. After release, knobs are plain again, and the absolute knob's first turn
  jumps to its position.
- **CTL-19** `[SWEEP]` (A:SHF-01; B:C-14) **Start:** a row with Shift mapped. **Action:** hold Shift
  and use every shifted job, then add Hold Drill and turn a knob, then unplug while Shift is held and
  replug. **Result:** shifted jobs fire. With Hold Drill held, the knob drills. The controller stays
  shifted until Shift is pressed and released again. Shift has no on-screen control.
- **CTL-20** `[SWEEP]` (B:C-15; A:CUS-08) **Action:** open every section of a Custom row. **Result:**
  the offered targets are encoder turns and pushes, Play, Stop, Freeze, Record, Randomize All,
  Randomize Page, Reset All, Reset Page, Page 1–6, Page Previous, Page Next, Scene 1, Scene 2, scene
  blend, BPM, Hold Drill, Hold Gesture Select and Shift. Clock, Continue and the plugin IN: button are not offered.
  **RULED** (MR 2026-09-01).
- **CTL-21** `[SWEEP]` (A:*-PATCH; B:F-16, F-17, F-18) Run once in each branch of 1.13. **Start:** a
  row for this branch's device is live, and a patch was saved while a different device's row was the
  only row. **Action:** open that patch from the File page. **Result:** the patch's sound and
  controller setup apply (the other device's row replaces the live one), and `config.json` is saved.
  A patch saved before patches carried mappings applies sound only. A Shift column set to Reset Page
  survives the save and load. **RULED** (MR 2026-09-01: "Saving a patch file saves the MIDI mappings";
  2026-09-18: "Opening a patch from the File page applies its sound and setup and saves config.json").
  **CODE:** `catalog.patchCarriesMappings = true` (`FroggersMidiCatalog`) and the `pendingPatchInstrument_`
  handling (`Sheaf:include/synth/Engine.hpp`).

### Connect messages (SR D-02: document)

- **CON-01** `[SWEEP]` (new from SR D-02) **Start:** an Akai APC40 mkII (Ableton) row, a Launchpad row
  and a Custom row. **Action:** open each row's editor. **Result:** every row shows a **Connect
  messages** section with an **Add** button. The APC40 (Ableton) row lists
  `F0 47 7F 29 60 00 04 41 09 07 01 F7`, a Launchpad row lists its programmer-mode message, and the
  Custom row lists none. Each message shows as space-separated uppercase hex. **fix required**
  (MANUAL). **CODE:** `Apc40AbletonDeviceDefault` and `LaunchpadDeviceDefault` set `config.openSysEx`
  (`app/FroggersMidiCatalog.hpp`); the section is built on every row whatever its kind
  (`FormatSysExHex`; "Connect messages" in Sheaf:include/synth/ControllersPageUI.hpp).
- **CON-02** `[SWEEP]` (new from SR D-02) **Start:** CON-01, on the Custom row. **Action:** press
  **Add**, type `F0 00 7F F7` into the new **Message** field, then type `F0 80 F7`, then `ZZ`, then
  press **x**. **Result:** Add appends `F0 F7`. `F0 00 7F F7` is stored. `F0 80 F7` is refused with
  "Refused: connect message must be one SysEx message: leading F0, trailing F7, data bytes 00-7F", and
  `ZZ` with "Refused: connect message must be hex byte pairs, e.g. F0 00 7F F7"; each refusal keeps the
  stored message. **x** deletes the message. Each committed edit is saved as it is made (CTL-17).
  **fix required** (MANUAL). **CODE:** `AddConnectMessage`, `SetConnectMessage` and
  `DeleteConnectMessage` (Sheaf:src/MidiConfigViewModel.cpp).
- **CON-03** `[SWEEP]` (new from SR D-02) **Start:** a row with connect messages and a bound MIDI out.
  **Action:** connect the output, then unplug and replug the device. **Result:** the row sends its
  connect messages, in order, to its MIDI out when that output connects (APA-01, LPX-01). **CHECK**
  the replug: `OpenSysExMidiOutProcessor` sends once after each `Reset()` (Sheaf:src/MidiController.cpp).
  **fix required** (MANUAL).

### Offline port option (SR D-04: document)

- **PRT-01** `[SWEEP]` (new from SR D-04) **Start:** a row whose bound MIDI in device is unplugged,
  so its dot reads offline. **Action:** open its **MIDI in** selector and keep the entry carrying the
  stored port's name; in a second run choose a connected device; in a third choose **(none)**. Repeat
  on **MIDI out**. **Result:** the selector lists **(none)**, every connected device, and the stored
  port's name, which is selected. Keeping that entry changes nothing, so the row reconnects when the
  device returns (CTL-14). Choosing a device binds the row to it, and **(none)** clears the port.
  **fix required** (MANUAL). **CODE:** `BuildEndpointOptions` appends `kEndpointOfflineOptionId`
  ("keep_offline") while the port is offline, and the port handler returns on it
  (Sheaf:include/synth/ControllersPageUI.hpp).

### Gestures (SR D-03: reachable over MIDI)

A gesture is "an arbitrary set of actions the user groups together into one midi control" (SR). A
separate trace determines how. These steps state only what the user sees.

- **GES-01** `[SWEEP]` (new from SR D-03) **Start:** a controller row. **Action:** group a set of
  actions of the user's choosing into one gesture, and assign the gesture one MIDI control on that
  controller. **Result:** the row shows the gesture with its MIDI control. **fix
  required** (code and MANUAL).
- **GES-02** `[SWEEP]` (new from SR D-03) **Start:** GES-01. **Action:** operate the gesture's MIDI
  control. **Result:** every action in the gesture happens, from that one control. **fix required.**
- **GES-03** `[SWEEP]` (new from SR D-03) **Start:** GES-01. **Action:** save a patch, quit and
  relaunch, then open the saved patch from the File page; then change which knobs belong to the
  gesture, and quit and relaunch without saving. **Result:** the gesture's rows are kept, the same as
  the row's other mappings (CTL-17, CTL-21). Which knobs belong to the gesture, and the setting it
  takes each one to, are part of the patch, where Sheaf stores them: the relaunch and the opened patch
  bring back the gesture as it was saved (QR-01, FILE-07), and the change made after the last save is
  lost on relaunch like any other unsaved edit (QR-03). **RULED** (SR 2026-09-23, gestures follow
  Sheaf). **fix required.**
- **GES-04** `[SWEEP]` (new from SR 2026-09-23) **Start:** GES-01, with knob A in the gesture.
  **Action:** press Reset Page on A's page; build the gesture again, then press Reset All on a
  parameter page. **Result:** after each reset, the gesture's control no longer moves A, and the
  gesture's rows are kept. In a modulation view, Reset Page and Reset All take the depths they reset
  out of every gesture in the same way. **RULED** (SR 2026-09-23: Sheaf's reset of a parameter takes
  it out of every gesture). **fix required.**

## 1.13 One branch per shipped controller default, plus Custom (S, B)

Each branch starts from the main story with the previous controller still plugged in and configured.
Froggers addresses one Twister device-bank. Parameter-bank means a Froggers page.

### Branch BR-14: MIDI Fighter Twister

- **TWI-01** `[UX]` (A:TWI-01; B:C-TW-01) **Action:** in the Midi Fighter Utility, set every encoder to
  "Enc 3FH/41H", all six side buttons to "CC Hold", and uncheck "Bank Side Buttons". **Result:** the
  side buttons send CC 8–13 on channel 4 (3 counted from 0) on any device-bank, and the Shift release
  reaches the app.
- **TWI-02** `[SWEEP]` (A:TWI-SW; B:C-SW-01) **Action:** unplug the previous controller, plug in the
  Twister, pick **MIDI Fighter Twister** and press Add. **Result:** the old row goes offline and stays,
  and the new row binds both ports. If only one port is present, both read "(none)" and the user binds
  the present one.
- **TWI-03** `[SWEEP]` (A:TWI-02; B:C-TW-02) **Action:** turn all 16 encoders on every parameter-bank.
  **Result:** knobs 1–14 move slots 0–13 of the shown page, knob 15 moves Crispy and knob 16 moves
  Crunchy. The LED rings follow.
- **TWI-04** `[SWEEP]` (A:TWI-03; B:C-TW-02) **Action:** push each encoder, then push knob 16 in a view.
  **Result:** a push drills in like a click. Knob 16 in a view goes up one level (MOD-09). **CODE:** A
  said it returns to the grid.
- **TWI-05** `[SWEEP]` (A:TWI-04..08; B:C-TW-03) **Action:** press the five side buttons unshifted.
  **Result:** left top is Page Next (parameter-bank, wrapping Reverb to Audio, and doing nothing in a
  view), left middle Play, left bottom Freeze, right top Scene 1 (blend to 0), and right middle
  Randomize Page.
- **TWI-06** `[SWEEP]` (A:TWI-09..13; B:C-TW-04) **Action:** hold Shift (right bottom) and press the
  other five. **Result:** Page Previous, Stop, Reset Page, Scene 2 (blend to 1) and Randomize All.
- **TWI-07** `[SWEEP]` (A:TWI-14; B:C-TW-05) **Action:** hold Shift and turn knob 16 both ways, then
  release and turn. **Result:** held, the scene blend moves one step per detent and Crunchy stays.
  Released, Crunchy moves.
- **TWI-08** `[SWEEP]` (A:TWI-15; B:C-TW-06) **Start:** Shift held, 30 BPM. **Action:** turn knob 15
  one detent down, then up, then at 300 up, then change pages under Shift and turn, then release and
  turn. **Result:** the tempo moves 1.0 BPM per detent, clamped to 30–300, identically on every page,
  and Crispy does not move. Released, only the current page's Crispy moves. **RULED** (MR 2026-09-19).
- **TWI-09** `[SWEEP]` (A:TWI-16; B:C-TW-07, C-TW-09) **Action:** open Encoders. **Result:** the
  Crunchy turn is its own row with Shift set to "Scene Blend", and the Crispy turn is its own row with
  "BPM". The other 14 turns form one block. Record and Reset All are not on the Twister.
- **TWI-10** `[UX]` (A:TWI-18; B:C-TW-08) **Start:** a Twister row made from the preset before this
  version. **Result:** it reports that it differs, and Restore installs both shifted turns. A row made
  before the preset existed, or a released row, has no Restore: delete it and add the preset again.

### Branch BR-15: Akai APC40 mkII (Generic)

- **APG-01** `[SWEEP]` (A:APG-SW; B:C-APG-01) **Action:** plug in the APC40, pick the Generic preset,
  and press Add. **Result:** nothing is sent to the unit, and the unit lights its own buttons.
- **APG-02** `[SWEEP]` (A:APG-01, APG-02; B:C-APG-02, C-APG-04) **Start:** Track 1 selected. **Action:**
  turn track knobs 1–8 and device knobs 1–8 end to end. **Result:** they reach encoders 1–8 and 9–16
  as absolute encoders.
- **APG-03** `[SWEEP]` (A:APG-03; B:C-APG-05) **Action:** press another Track Select, then Track 1.
  **Result:** the device knobs leave encoders 9–16, then return.
- **APG-04** `[SWEEP]` (A:APG-04..11; B:C-APG-03) **Action:** press SHIFT (hold and turn), PLAY, STOP,
  RECORD, SCENE LAUNCH 1 and 2, LEFT and RIGHT, DEVICE ON/OFF, DEVICE LOCK, CLIP/DEVICE VIEW, DETAIL
  VIEW, STOP ALL CLIPS, and the CLIP STOP buttons under tracks 1–6. **Result:** Hold Drill, Play,
  Stop, Record, Scene 1 and 2, Page Previous and Next (no-op in a view), Randomize Page, Randomize All,
  Reset Page, Reset All (RST-04 in a view), Freeze, and Page 1–6. There are no shifted jobs.
- **APG-05** `[SWEEP]` (A:APG-12; B:C-APG-03) **Action:** move the crossfader and then the master
  fader end to end. **Result:** the blend goes from 0 to 1 and BPM from 30 to 300.

### Branch BR-16: Akai APC40 mkII (Ableton)

- **APA-01** `[SWEEP]` (A:APA-SW; B:C-APA-01) **Action:** pick the Ableton preset and press Add.
  **Result:** the connect-time SysEx switches the unit to Ableton mode, and every button LED is left
  to the host (dark). **RULED** (MR 2026-09-01: unconfirmed on hardware).
- **APA-02** `[SWEEP]` (A:APA-01) **Action:** repeat APG-02 to APG-05. **Result:** the jobs are the same.
- **APA-03** `[SWEEP]` (A:APA-02; B:C-APA-01) **Action:** press other Track Selects and turn the device
  knobs. **Result:** they stay on encoders 9–16. This is MR's recorded operator check.

### Branch BR-17: Launchpad X

- **LPX-01** `[SWEEP]` (A:LPX-SW; B:C-LPX-01) **Action:** pick Launchpad X and press Add. **Result:**
  the row is added. If a port reads "(none)" (port names are unconfirmed), bind it by hand. Programmer
  mode is sent on output connect, and the pads stay dark.
- **LPX-02** `[SWEEP]` (A:LPX-01..03; B:C-LPX-01) **Action:** press the top row, the right column and
  the 8×8 grid. **Result:** the top row, left to right, is Play, Stop, Freeze, Record, Scene 1, Scene 2,
  Randomize Page and Reset Page. The right column's top six are Page 1–6. The grid does nothing.
- **LPX-03** `[UX]` (A:LPX-04) The unit has no encoders, so knobs are set on screen or on another
  controller.
- **LPX-04** `[SWEEP]` (A:LPX-05; B:C-LPX-01) **Action:** open the editor. **Result:** only System
  Messages is offered.
- **LPX-05** `[SWEEP]` (A:LPX-06; B:C-LP-02) **Action:** set **Model** to Launchpad X, Pro MK3 and
  Mini MK3 in turn. **Result:** the row retargets its pads to that model (`kLaunchpadVariants`).
  **RULED** (MR 2026-09-15): the selector is permitted, and its label is a WYSIWYG defect (section 2.1,
  item e).

### Branch BR-18: Launchpad Pro MK3

- **LPP-01** `[SWEEP]` (A:LPP-SW..03; B:C-LPP-01) The same as LPX-01 to LPX-05 on the Pro MK3's own top
  row and side column.

### Branch BR-19: Launchpad Mini MK3

- **LPM-01** `[SWEEP]` (A:LPM-SW, LPM-01; B:C-LPM-01) **Start:** ports "Launchpad Mini MK3 LPMiniMK3
  MIDI Out" and "... MIDI In". **Action:** pick Mini MK3 and press Add. **Result:** both ports bind
  automatically. The map, programmer mode and dark pads are the same as the Launchpad X.
- **LPM-02** `[SWEEP]` (A:LPM-02; B:C-LPM-01) **Result:** the DAW ports pair with no preset and are
  listed under "Other inputs" and "Other outputs".
- **LPM-03** `[SWEEP]` (A:LPM-03; B:C-LP-02) Model, as LPX-05.

### Branch BR-20: Custom

- **CUS-01** `[SWEEP]` (A:CUS-01; B:C-CU-01) **Action:** pick Custom and press Add, twice. **Result:**
  "Custom" and "Custom 2" are added, with no mappings, ports "(none)" and sections open. They never
  show Restore.
- **CUS-02** `[SWEEP]` (A:CUS-03; B:C-CU-02) **Action:** bind MIDI in and MIDI out. **Result:** the
  device label becomes the MIDI input's name.
- **CUS-03** `[SWEEP]` (A:CUS-04; B:C-CU-06, C-CU-07) **Action:** set Encoder mode to signed 7-bit,
  direction-only, then absolute, and change Step. **Result:** each mode applies live. Step ("Step
  (relative modes only)") is kept but applies only to the relative modes. These config rows have no
  delete.
- **CUS-04** `[SWEEP]` (A:CUS-05; B:C-CU-03) **Action:** press **+** on an empty Turn group and set
  channel, CC, slot and position. **Result:** the mapping is added, and turning the knob moves that
  slot.
- **CUS-05** `[SWEEP]` (A:CUS-06; B:C-CU-04) **Action:** press **+B** on Turn and set a start CC and an
  end CC (exclusive). **Result:** one mapping is added per CC.
- **CUS-06** `[SWEEP]` (A:CUS-07; B:C-CU-05, C-CU-08) **Action:** add a Push row, switch it to Note,
  and enter note 60. **Result:** it drills in and shows 60 as a number. CC and Note rows never merge
  into one block.
- **CUS-07** `[SWEEP]` (A:CUS-08; B:C-CU-09) **Action:** in System Messages, add one row for every
  button job in CTL-20, one of them by Note, and one **+B** run of page selects. **Result:** each
  button does its job.
- **CUS-08** `[SWEEP]` (A:CUS-09; B:C-CU-10) **Action:** set a row's Shift column to Reset Page, map
  Shift, then hold it and press. **Result:** Reset Page fires.
- **CUS-09** `[SWEEP]` (A:CUS-10) **Action:** Hold Drill, as CTL-18.
- **CUS-10** `[SWEEP]` (A:CUS-11; B:C-CU-11) **Action:** in Analogs, set the **Scene blend** row's CC
  and add an **App actions** row for BPM, then move both end to end. **Result:** the blend goes from 0
  to 1 and BPM from 30 to 300. **CODE:** A used an App action row and B a Gesture row for the blend.
  Scene blend is its own config-level row (`RowGroup::AnalogSceneBlend`, "config-level, never
  addable", `Sheaf:include/synth/MidiConfigViewModel.hpp`). The Gestures group is D-03 in section 3.1.
- **CUS-11** `[SWEEP]` (A:CUS-12, CUS-13; B:C-CU-12) **Action:** delete a row and a block, add two
  adjacent compatible rows, then collapse and reopen. **Result:** exactly those are deleted, and the
  block goes in one commit. The two rows show as one block after reopening.
- **CUS-12** `[SWEEP]` (A:CUS-14) **Action:** map a turn by Note. **Result:** refused; turns and
  analogs are CC only.
- **CUS-13** `[SWEEP]` (B:C-CU-13) **Action:** expand three rows with many mappings. **Result:**
  everything is reachable by scrolling and fits the width.

### Rejoin: two controllers at once

- **SWX-01** `[SWEEP]` (A:SWX-01; B:C-SW-02, C-SW-03) **Start:** two rows, both connected. **Action:**
  use both, then replug one. **Result:** each device uses its own ports and feedback, and Shift
  belongs to its own controller. Replugging one does not disturb the other. **RULED** (SR R2: one
  representative pair; "this should work on any midi device", and the Custom branch covers arbitrary
  devices).

## 1.14 File page and patches (S, B; P has none)

- **FILE-01** `[SWEEP]` (A:FILE-01; B:F-01) **Action:** open File. **Result:** it shows a header, the
  patches root, **New**, **Save**, **Save As** and **Load**, and a status area. There is no Revert.
  **RULED** (MR 2026-09-18: "Revert is REMOVED"). **CODE:** `RuntimePages.hpp` `NodeIds` has no
  `kFileRevert`.
- **FILE-02** `[SWEEP]` (A:FILE-02; B:F-02) **Start:** no current patch. **Action:** press Save.
  **Result:** the Save As browser opens.
- **FILE-03** `[SWEEP]` (A:FILE-03; B:F-03) **Action:** Save As with a new name. **Result:** the
  directory is created with its first version, and the header shows the name.
- **FILE-04** `[SWEEP]` (A:FILE-04; B:F-04) **Action:** type an existing name. **Result:** an inline
  "already exists" message, and nothing is written.
- **FILE-05** `[SWEEP]` (A:FILE-05; B:F-05) **Action:** double-click an existing row in Save As.
  **Result:** a new version is added to that patch.
- **FILE-06** `[SWEEP]` (A:FILE-06; B:F-06) **Action:** press Save three times. **Result:** three new
  versions; none is overwritten, and Versions lists them newest first.
- **FILE-07** `[SWEEP]` (A:FILE-07; B:F-09) **Action:** press Load and confirm a row, or double-click
  it. **Result:** the latest version loads, applying sound and setup (CTL-21).
- **FILE-08** `[SWEEP]` (A:FILE-08; B:F-10) **Action:** double-click an older version. **Result:** that
  exact version loads.
- **FILE-09** `[SWEEP]` (B:F-11) **Action:** Load an empty or corrupt patch. **Result:** a failure is
  reported and nothing changes (spp-6). **CHECK.**
- **FILE-10** `[SWEEP]` (A:FILE-09; B:F-08) **Action:** press Cancel in either browser. **Result:**
  nothing happens.
- **FILE-11** `[SWEEP]` (A:FILE-10; B:F-07) **Action:** type a name with `..` or an absolute path.
  **Result:** rejected; nothing outside `patches/` is touched, and no OS file picker appears.
- **FILE-12** `[SWEEP]` (A:FILE-11; B:F-14) **Action:** press New. **Result:** the state equals LCH-02
  and LCH-03, and there is no current patch.
- **FILE-13** `[SWEEP]` (A:FILE-14; B:F-15) **Action:** press Back on File. **Result:** the runtime
  configuration is not written.

## 1.15 Quitting and relaunching (S)

- **QR-01** `[SWEEP]` (A:QR-01; B:Q-02, Q-03) **Start:** an older version of A was opened after a newer
  B was saved. **Action:** relaunch. **Result:** the version of A that was opened comes back, with
  sound only, and the controller setup comes from `config.json`. **RULED** (MR 2026-09-18). **CODE:**
  `lastPatchVersionRecord_` with `applyInstrument=false` (`Sheaf:include/synth/Engine.hpp`).
- **QR-02** `[SWEEP]` (A:QR-02; B:Q-04) **Start:** New, nothing saved. **Action:** relaunch.
  **Result:** New's defaults. **CODE:** New records an empty version (`RecordPatchVersionAndSave`).
- **QR-03** `[SWEEP]` (A:QR-03; B:Q-01) **Start:** edits after the last save. **Action:** relaunch.
  **Result:** the edits are lost.
- **QR-04** `[SWEEP]` (new from CODE) **Start:** the recorded version file was deleted. **Action:**
  relaunch. **Result:** the newest saved version loads, sound only, and is re-recorded.
- **QR-05** `[SWEEP]` (A:QR-04; B:Q-05, Q-06, Q-07) **Action:** relaunch. **Result:** the controller
  setup, the audio devices and sync are as last saved.
- **QR-06** `[SWEEP]` (A:QR-05; B:Q-08) **Action:** relaunch twice. **Result:** the stepped S&H
  sources start from different values.
- **QR-07** `[SWEEP]` (A:QR-06) **Start:** a configured controller unplugged. **Action:** relaunch,
  then plug it in. **Result:** the row starts offline and connects on a later poll.

## 1.16 Documentation and load readout

- **DOC-01** `[SWEEP]` (A:DOC-01; B:A-03) **Start:** offline. **Action:** open **Help**, then
  **Manual**, then **Quick Dictionary** (the macOS main menu, or the window menu bar on Windows).
  **Result:** both open, matching that build (`HelpMenuModel`, `app/FroggersMain.cpp`). **Caps:**
  P uses the "?" button (PLG-13). B uses the site links (WEB-01).
- **LOAD-01** `[SWEEP]` (A:LOAD-01; B:A-04) **Action:** watch the readout through a spike.
  **Result:** a whole percent, which holds the spike briefly and then releases. Three digits fit.
  **Caps:** S and B only. **CODE:** A listed P, but P has no sidebar (PLG-04).

## 1.17 Plugin: what differs (P)

- **PLG-01** `[UX]` (A:PLG-01; B:P-01) macOS first DAW load of the VST3 or AU. **Result:** the steps
  are the same as INS-01 to INS-03.
- **PLG-02** `[UX]` (A:PLG-02; B:P-02) Windows first load of the unsigned VST3. **Result:** the release
  notes explain it. What Windows shows is not stated. **CHECK.**
- **PLG-03** `[SWEEP]` (A:PLG-03; B:P-03) **Action:** scan and instantiate. **Result:** a stereo output
  and one optional stereo input bus. It instantiates either way, and the AU is macOS only.
- **PLG-04** `[SWEEP]` (A:PLG-04, PLG-11, PLG-12; B:P-04, P-17) **Action:** open the editor.
  **Result:** the row shows Freeze with a "FREEZE" label and **IN:**, and no Play, Stop or Record.
  There is no sidebar at all: no Audio I/O, Controllers, Sync, File or load readout. A **?** button
  sits top right. **CODE:** A and B expected a File page. The editor builds a `PortableComponent`
  over the bare surface with "NO sidebar" (`FroggersPluginEditor.hpp` header). `SetPluginHostMode`
  hides Play, Stop and Record (`FroggersUiSurface.hpp`, `kTransportRow` builder).
- **PLG-05** `[SWEEP]` (A:PLG-05; B:P-05) **Action:** start and stop the DAW transport. **Result:** the
  app follows, once per host transition.
- **PLG-06** `[SWEEP]` (A:PLG-08; B:P-06) **Action:** change the DAW tempo, drag BPM, then use a host
  that reports no tempo. **Result:** with a host tempo, BPM reads "BPM `<value>` (external clock)" and
  ignores drags. With no usable host tempo, the sync config clears and the slider is editable.
  **CODE** (B's OTR-11): `hostTempoUsable` and `RequestSyncConfiguration(SyncConfig{})`
  (`app/vst/FroggersPluginProcessor.cpp`).
- **PLG-07** `[SWEEP]` (A:PLG-06, PLG-07; B:P-11..14) **Action:** press IN: repeatedly with audio
  routed, then remove the chosen channel or disable the bus. **Result:** it cycles None (the
  default), each channel, and Sum when there are two or more channels. Anything other than None
  connects External Audio and EF. A channel that disappears falls back to None. With the bus
  disabled, only None is offered.
- **PLG-08** `[SWEEP]` (A:PLG-09; B:P-07, P-08, P-09) **Action:** list, automate and DAW-MIDI-learn
  the host parameters, and press Freeze with the host playing and stopped. **Result:** there are 92
  parameters with stable IDs: 84 page parameters, 6 Crispy, Crunchy and Freeze. Each follows and reads
  back. Freeze latches independently of the host transport. Depths, scene blend, BPM and page are not
  host parameters.
- **PLG-09** `[SWEEP]` (A:PLG-10; B:P-10) **Action:** automate two pages while a third is shown, with a
  view open. **Result:** each value lands on its own parameter, the page does not move, and the view
  is neither closed nor written. **RULED** (HR, "Cross-bank automation = option (iii)").
- **PLG-10** `[SWEEP]` (A:PLG-13; B:P-15) **Action:** edit, set IN:, save, close and reopen the
  project. **Result:** the values and the IN: choice return. Standalone patches are untouched. A
  project older than the IN: setting opens with input off. New parameters keep their defaults.
- **PLG-11** `[UX]` (B:P-19) manual expectation. **Action:** close and reopen the editor. **Result:**
  the page, values and open view are as they were.
- **PLG-12** `[UX]` (B:P-20) manual expectation. **Action:** insert a second instance. **Result:** each
  instance has its own state.
- **PLG-13** `[SWEEP]` (A:PLG-14; B:P-18) **Action:** press **?**, then Manual, then Quick Dictionary,
  offline, on each format. **Result:** both open (`helpButton_`, `FroggersPluginEditor.cpp`).
- **PLG-14** `[UX]` (A:PLG-15; B:P-intro) **Action:** Randomize, Reset, scenes, every page and
  modulation. **Result:** the same as S.
- **PLG-15** `[SWEEP]` (B:P-16) **Action:** look for Record. **Result:** there is none; the DAW
  records.
- **PLG-16** `[SWEEP]` (A:PLG-16; B:P-C-01) **Action:** look for a Controllers page, Preset or Custom.
  **Result:** none (PLG-04).
- **PLG-17** `[SWEEP]` (A:PLG-17; B:P-C-02) **Start:** a Twister, APC40 or Launchpad routed through the
  DAW to the track. **Action:** use every control with no DAW mapping. **Result:** manual: nothing
  moves. **PENDING-PLG.** Lead only: `processBlock` calls `juce::ignoreUnused(midiMessages)`.
- **PLG-18** `[SWEEP]` (A:PLG-18; B:P-C-04, P-C-06) **Action:** DAW-MIDI-learn knobs to host
  parameters, including relative encoders. **Result:** each moves one fixed parameter. How a relative
  stream moves it is up to the DAW. **PENDING-PLG.**
- **PLG-19** `[SWEEP]` (A:PLG-19; B:P-C-05) **Action:** try mapping Page, Randomize, Reset, Scene,
  blend, BPM, push, Hold Drill and Shift. **Result:** they are not host parameters (PLG-08).
  **PENDING-PLG** for any other route.
- **PLG-20** `[SWEEP]` (A:PLG-20; B:P-C-03) **Action:** watch the controller. **Result:** manual
  expectation: no LED feedback, no APC40 Ableton SysEx, no Launchpad programmer mode, and no clock
  out. **PENDING-PLG.**
- **PLG-21** `[UX]` (B:P-C-07) **Action:** save and reopen the project with DAW mappings. **Result:**
  manual expectation: the DAW restores its own mappings.
- **PLG-22** `[SWEEP]` (A:PLG-21) **Action:** open a standalone patch. **Result:** not possible, because
  P has no File page (PLG-04). **CODE.**

## 1.18 Browser build: what differs (B)

- **WEB-01** `[SWEEP]` (A:WEB-01; B:B-04) **Action:** visit the site. **Result:** the logo, and links
  to the desktop app and the plugin (each to its own release), the licence and the manual.
- **WEB-02** `[SWEEP]` (A:WEB-02; B:B-02) **Start:** first visit, before the service worker controls
  the page. **Result:** no failure panel before the one reload. After it, an isolation failure or a
  failed registration shows the panel with the error.
- **WEB-03** `[SWEEP]` (A:WEB-03; B:B-01) **Action:** load and touch nothing. **Result:** every encoder
  shows its name and value, there is no audio, and no prompt has appeared.
- **WEB-04** `[SWEEP]` (A:WEB-04; B:B-03) **Action:** the first in-app action. **Result:** audio starts.
  **CODE:** A said Play and B said any first action; activation piggybacks on the first UI action
  (`app/browser/site/site-boot.mjs`).
- **WEB-05** `[SWEEP]` (A:WEB-05; B:B-05) **Action:** BPM from 30 to 300, and every S step above.
  **Result:** the same as S.
- **WEB-06** `[SWEEP]` (A:WEB-06; B:B-07) **Action:** allow, then deny, Web MIDI with SysEx.
  **Result:** allowed, controllers work as in 1.12 and 1.13 with polling reconnect. Denied, audio and
  the UI run, rows read offline, and the status names the missing SysEx permission.
- **WEB-07** `[SWEEP]` (A:WEB-07; B:B-06) **Action:** open Audio I/O. **Result:** Output lists
  **System Default** plus any output the browser exposes by name. **CODE:** both drafts said System
  Default only; `BuildBrowserAudioSnapshot` adds named outputs
  (`Sheaf:include/synth/browser/BrowserAudioDevices.hpp`). **CHECK** that a named output is actually
  used.
- **WEB-08** `[SWEEP]` (A:WEB-08; B:B-09, B-10) **Start:** no microphone permission. **Action:** press
  **Allow Microphone**, then grant, and in another run deny. **Result:** granted, Input lists devices
  by name, the selection stays on No Input, and no stream is left open. Denied, the page reports it.
  The button shows only while inputs are unnamed (`showInputPermissionRequest`). **CODE** (A's OTR-10
  and B's OTR-9): the label is at `RuntimePages.hpp` 991. The manual calls it Retry Input.
- **WEB-09** `[SWEEP]` (A:WEB-09; B:B-11) **Action:** select an input. **Result:** the External sources
  connect (MOD-12). **Caps:** up to 32 input channels (`kMaxBrowserInputChannels`); one is requested.
- **WEB-10** `[SWEEP]` (A:WEB-10) **Start:** capture denied or ended. **Action:** press **Retry Input**.
  **Result:** capture is retried without a restart.
- **WEB-11** `[SWEEP]` (A:WEB-11; B:REC-09) **Action:** Play, Record, Record. **Result:** the browser
  downloads `YYYY-MM-DD.wav`. REC-01 and the 30-minute limit apply.
- **WEB-12** `[SWEEP]` (A:WEB-12; B:B-08) **Action:** Save As, Save, Load, then reload. **Result:**
  patches persist in browser storage, and the File status reports sync as pending, succeeded or
  failed. After a reload, configuration and patches are restored before the engine starts.
- **WEB-13** `[SWEEP]` (A:WEB-13; B:B-P-01) **Start:** 720 px or narrower. **Result:** the grid and the
  chrome span the width. Randomize and Reset sit inside the chrome, right of BPM, each at most half
  its width. Audio I/O, Controllers, Sync and File sit below them. The first encoder row is visible,
  and each button exists once.
- **WEB-14** `[SWEEP]` (A:WEB-14; B:B-P-02, B-P-03) **Start:** WEB-13. **Action:** touch-drag an
  encoder, then drag a gap, then wait. **Result:** the encoder turns without scrolling, gaps scroll
  the page, and the scroll position holds.
- **WEB-15** `[SWEEP]` (A:WEB-15; B:B-W-01) **Start:** wider than 720 px. **Result:** Randomize and
  Reset sit below the grid.
- **WEB-16** `[UX]` (A:WEB-16) **Action:** read the Preset selector while it shows "MIDI Fighter
  Twister". **Result:** the text is clipped inside its box.

## 1.19 Steps removed by ruling

| ID | Ruling |
|---|---|
| WEB-17 (B:B-L-01, loading from a Sheaf launcher catalog) | SR R3, option (b): "this is sheaf's problem, we don't care." |

No other step is removed. The WRLD.Bldr ruling removes no step, because the merged story already had
no WRLD.Bldr branch. The MIDI-out ruling removes no step, because no step covers pitch as notes.

---

# 2. Rulings

## 2.1 Answered by a recorded ruling or an operator ruling given with the merge (7)

a. **Does a patch carry the controller setup?** (A:OTR-5; B:OTR-3; decides CTL-21, FILE-07, TWI/APG/
   APA/LPX/LPP/LPM/CUS-PATCH.) MR 2026-09-01: "Saving a patch file saves the MIDI mappings". MR
   2026-09-18: "A patch file keeps sound AND controller setup ... Opening a patch from the File page
   applies its sound and setup and saves `config.json`." The Sheaf texts spp-2 and sar-8 are stale.
b. **What a relaunch opens.** (A:OTR-6; B:OTR-4; decides QR-01, QR-02.) MR 2026-09-18: "On relaunch the
   app reopens the patch VERSION last opened or saved (a version file, not the folder's newest)". The
   sar-8 text ("lexicographically greatest") is stale.
c. **When controller edits are saved.** (B:OTR-5; decides CTL-17.) MR 2026-09-18: "`config.json`, which
   the Controllers page saves on every edit". The sru-12 text is stale for Controllers.
d. **Lifecycle controls on the Controllers page.** (A:OTR-7; B:OTR-6; decides CTL-01, CTL-16.) MR
   2026-09-01: the dropdown "REPLACES the wizard's Reconfigure path". MR 2026-09-16: "The Controllers
   page add dropdown lists devices plus one plain Custom". The code has no Reconfigure, Blacklist,
   Configure, Ignore or wizard action. The Released badge and the sidebar marker ship.
e. **Launchpad Variant selector.** (A:OTR-8; B:OTR-7; decides CTL-07, LPX-05, LPM-03, LPP-01.) MR
   2026-09-15: "a row names its device, and the page can change it. This reverses the 2026-09-03
   corollary", which overrides the spec's "SHALL NOT offer a second control". The same ruling says the
   label is wrong: "The existing Variant control is a WYSIWYG violation". The step stays because the
   selector ships. The label is a defect for the audit to fix.
f. **Revert.** (A:FILE-12 and its note; B:F-12, F-13, BR-REVERT; decides FILE-01.) MR 2026-09-18:
   "Revert is REMOVED". The code agrees (no `kFileRevert`). The sru-6 and spp-6 texts are stale.
g. **WRLD.Bldr.** (A header; B counts.) MR 2026-09-23, OP and SR: "removed entirely", with no branch or
   step. Main still offers it (D-01).

Also applied inside steps, although no draft raised them as questions: HR's PM floor (AUD-12), HR's
cross-bank automation (PLG-09), HR's "Reset All is global" at the grid (RST-01), MR's Shift-and-Crispy
tempo rule (TWI-08), MR's APC40 Ableton hardware check (APA-01, APA-03), MR's mapping targets (CTL-20),
and OP's 30-minute cap (REC-07).

## 2.2 Resolved by the code on main (the story describes main as it ships; correct the text named)

| # | Drafts | Main does | Evidence | Text to correct |
|---|---|---|---|---|
| 1 | A:OTR-1, B:OTR-1 | pitch 20 Hz–5 kHz | `Vco::kPitchMaxHz` | spec froggers-vco-topology ("20 kHz") |
| 2 | A:OTR-2 | Fold has no true zero | `SetFold` `ExpMapCompute(16,1,·)` | spec froggers-vco-topology (fold "true zero") |
| 3 | A:OTR-3 | Freeze drones while stopped | `setGate(gateOpen \|\| FreezeLatched())` | spec froggers-vco-topology "silent regardless" |
| 4 | A:OTR-4, B:OTR-2 | Randomize All uses floor 1 at every view level and descends one level | `RandomizeAll` | spec froggers-modulation-slate (level 2 = Randomize Page) |
| 5 | A:OTR-9, B:OTR-8 | the Sync page has Send clock and Send transport | `RuntimePages.hpp:858,868` | MANUAL (lists the receive toggles only) |
| 6 | A:OTR-10, B:OTR-9 | browser input: No Input plus named devices, via **Allow Microphone** | `BuildBrowserAudioSnapshot`; `RuntimePages.hpp:991` | MANUAL ("Retry Input"); Sheaf sar-30 |
| 7 | A:OTR-11, B:OTR-10 | Scene 1/2 set the blend to 0/1 | `HandleAction` `kSceneSelect` | MANUAL (never says) |
| 8 | A:OTR-12 | a single click drills in | `cellStyle.action = kEncoderPress` | none (sru-20 describes miniapp) |
| 9 | A:OTR-13 | six ±0.01 cross-VCO depths at launch | `kAudioPitchDetents` | MANUAL (silent) |
| 10 | A:OTR-14, B:OTR-12 | Width bal scales the spread only; cross-feed is 0 | `SetWidthBalance` | spec froggers-sheaf-parameter-model (ratio clause) |
| 11 | A:OTR-15 | the Ringmod knob sets amount and carrier together | `Vco::Process` | MANUAL and spec each state half |
| 12 | B:OTR-11 | plugin BPM is editable when the host reports no tempo | `hostTempoUsable` | spec froggers-vst-host (unconditional) |
| 13 | both (spec) | three modulation levels; Back goes up one level; the current page button fully exits | `kMaxDrillLevel = 3`; `Back()`; `FroggersCommand`; `ApplyAppCommand` | spec froggers-modulation-slate (two levels, full exit); MANUAL (Back) |
| 14 | A:WEB-07, B:B-06 | browser output lists named devices too | `BuildBrowserAudioSnapshot` | Sheaf sar-30 |
| 15 | A:ENV-12 vs B:EN-12 | stages take about 2.5× nominal, at most 2.75× | one spec sentence | none |
| 16 | A:LCH-01 vs B:S1 | launch BPM is 120 | `kDefaultTempoBpm` | none |
| 17 | A:LCH-01 vs B:S0 | first launch has no controller rows | empty `defaultInstrumentConfig_` | none |
| 18 | A:CUS-11 vs B:C-CU-11 | scene blend is a config-level Analogs row | `RowGroup::AnalogSceneBlend` | none |
| 19 | A:PLG-11,12, LOAD-01; B:P-17 | the plugin has no sidebar pages | `FroggersPluginEditor.hpp` header | MANUAL (silent about plugin pages) |
| 20 | A:TRN-07 vs B:T-FS-01 | Freeze from silence drones | `kFreeze` branch | none |
| 21 | A:TWI-03 | the Twister knob-16 push goes up one level | `Back()` | none |
| 22 | B:T-07 | Play while running restarts the grid (CHECK) | MasterClock internal Start | none |
| 23 | B:REC-12 | Stop and Freeze do not end a capture (CHECK) | `HandleAction` | none |
| 24 | A:REC-06, B:REC-08 | Cancel discards the take | `RegisterFileExportHandler` | none |

## 2.3 Resolved by operator ruling, 2026-09-23 (SR)

**R1. Reset All pressed in a modulation view.** Option (a): keep main. "this is incorrect, the manual
says the correct way to do it, which is already happening." In a view, Reset All resets the selected
parameter's depths to default and zeroes the next level under each of them (RST-04). This decides
RST-04, APG-04 (DETAIL VIEW in a view), APA-02, CUS-07 (the Reset All row), PLG-14, and the Reset All
half of CTL-20. The header comment "Reset All -- global, at every level." above `ResetAll`
(`app/FroggersModulation.hpp`) contradicts the code below it and stays a false-comment BUG. The
manual gains the text in section 5, item 4.

**R2. Two controllers at once.** Option (a): one representative pair. "this should work on any midi
device"; the Custom branch covers arbitrary devices. SWX-01 stays one step.

**R3. Loading from a Sheaf launcher catalog.** Option (b): "this is sheaf's problem, we don't care."
WEB-17 is removed (section 1.19).

**WRLD.Bldr.** Removed entirely, with no branch or step (D-01).

**Pitch as notes for MIDI out.** Monophonic, with 36–100 ms acceptable. MIDI out is a separate change
and is not part of this story.

**Hand-edited configuration and patch files.** Out of the story; no fix.

**Gestures follow Sheaf (2026-09-23).** "there is no reason to depart from Sheaf on this." Sheaf
stores which parameters belong to a gesture, and the setting the gesture takes each one to, on each
parameter per scene, and writes both into a patch (`Parameter::ToValueJSON`). Sheaf's reset of a
parameter takes it out of every gesture in the scenes it resets (`Parameter::ResetSceneToDefault`).
This decides GES-03 and GES-04. The GES-01 and CTL-20 wordings stand: Sheaf's Gestures row shows the
gesture and its control.

**Materialized modulation depths are growable (2026-09-23).** "option 2 match[es] plausible user
stories while option 1 excludes many plausible user stories". No cap on live depths (section 4).

**Randomize never stops short (2026-09-23).** Randomize All re-sets every modulation depth and
parameter; it is not additive. A press zeroes a knob's existing depths and draws new ones in one
frame; the release that follows in the same frame frees none of them, because it reads centres the
recompute after it has not yet updated, so they are freed by a later frame's release, and only the
message thread adds storage, so a press can need old plus new slots at once. "this is a good bug to
fix ... within the scope of this change." **fix required**: a press never needs more slots than it
holds (section 4). (SR
2026-09-23: "i don't see why reset or randomize should ever be included in the same batching"). This replaces the earlier C1 ruling, which cited MANUAL.md 176–181; those lines describe
how deep Randomize All reaches and say nothing about storage.

For the operator's information: the recorded Twister device-bank fork (MR 2026-09-15, "OPEN FORK",
options A, B and C) does not change any step, because main ships branch A (TWI-01 declares the
precondition). If (C) lands, it rewrites TWI-01.

---

# 3. Inventory diff

## 3.1 Controls in the code with no step in the merged story, as ruled

| ID | Control | Symbol and pointer | Ruling (SR) | Resolution |
|---|---|---|---|---|
| D-01 | **WRLD.Bldr** in the Preset dropdown | `synth::MakeControllerWizardRegistry` appends `library.wrldbldr` (Sheaf:include/synth/ControllerWizard.hpp:135); pinned by `app/FroggersControllersPageTests.cpp` (`registry.back().id == "library.wrldbldr"`) | removed entirely (MR 2026-09-23) | **fix required**: remove it from the dropdown, its test pin, and MANUAL.md 269–282 (section 5, item 5). CTL-01 lists the ruled dropdown. No step. |
| D-02 | **Connect messages** section on every row (add, edit and delete a hex SysEx) | `Actions::kConnectMessageAdd/Commit/Delete` (Sheaf:include/synth/ControllersPageUI.hpp:280-282); section "Connect messages" (same file, ~2856) | document in MANUAL.md | CON-01 to CON-03, **fix required** (MANUAL). Section 5, item 1. |
| D-03 | Analogs **Gestures** row group (+ and +B) | `MidiMappingRowVM::RowGroup::AnalogGesture` (Sheaf:include/synth/MidiConfigViewModel.hpp:215); caption at ControllersPageUI.hpp:524; drives `FroggersParameterModel::kNumGestures = 8` (`app/FroggersParameters.hpp`) | make gestures reachable over MIDI: "a gesture is just an arbitrary set of actions the user groups together into one midi control" | GES-01 to GES-03, **fix required** (code and MANUAL). A separate trace determines how. Section 5, item 3. |
| D-04 | Port selector option showing the stored port while offline | `kEndpointOfflineOptionId = "keep_offline"` (ControllersPageUI.hpp:37, :865) | document | PRT-01, **fix required** (MANUAL). Section 5, item 2. |
| D-05 | File browser **Parent** id | `NodeIds::kFileBrowserParent` (Sheaf:include/synth/RuntimePages.hpp:101, :242) | delete | No step: the id is never rendered, and `RuntimePagesJuceTests.cpp:287` asserts it is absent. |

## 3.2 Draft steps with no control in the code (these do not ship)

| ID | Draft steps | What is missing | Pointer |
|---|---|---|---|
| X-01 | A:FILE-12; B:F-12, F-13, BR-REVERT-* | the Revert button | no `kFileRevert` in `RuntimePages.hpp` `NodeIds`; ruled removed (2.1 f) |
| X-02 | A:CTL-14; B:OTR-6 list | Reconfigure, Blacklist, Configure, Ignore, and the Configuration Wizard action | `Actions` list, ControllersPageUI.hpp:265-282; ruled (2.1 d) |
| X-03 | B:A-SP-01, A-SP-02 (BR-SHEAF-PATCH) | launching frogg3rs from the Sheaf Patch desktop launcher | `app/FroggersMain.cpp` header: launches "only Frogg3rs -- no ... Launcher.hpp"; `apps/sheaf-patch` has no Froggers reference |
| X-04 | A:PLG-11; B:P-17 | a File page (patches) in the plugin | `FroggersPluginEditor.hpp` header, "NO sidebar" |
| X-05 | A:LOAD-01 (P column) | a load readout in the plugin | same |
| X-06 | A:PLG-21 | opening a standalone patch in the plugin | same |
| X-07 | A:MOD-19, MOD-20; B:M-06, M-07 | a refusal at the third level, and Back going straight to the grid | `kMaxDrillLevel = 3`, `Back()`; replaced by MOD-07 to MOD-09 |

## 3.3 Corrections to the control inventory, found while settling matches

- §1: Shape 2 and 3 and Drive Gain are listed at 0.0. The default patch sets them to 0.5, 1.0 and 0.2
  (`ApplyAudioBankOverlay`, `ApplyDriveBankOverlay`), and Drive Phase is 0.86.
- §4: Stop is listed as present on the plugin. It is hidden (`if (!pluginHostMode)` covers Play and
  Stop).
- §5 and §11: the MIDI catalog and the Controllers page are listed on standalone and plugin, and not
  on browser. The plugin has no sidebar. The browser has the page through Web MIDI (MANUAL; sbw-5).
- §6: the Preset dropdown is listed as "6 device defaults + Custom". Main also carries WRLD.Bldr
  (D-01), which is ruled removed.
- §2 and §3: "up to 3 levels" is correct, and both drafts disagree with it (2.2 #13).
- The Audio I/O page controls are not enumerated: Output, Input, Retry Input (`kAudioInputRetry`),
  Allow Microphone (`kAudioInputPermission`), and Back. The Help and "?" documentation openers are
  also missing.

---

# 4. Growable quantities and the code's own limits

No caps are imposed (SR, C1–C7 withdrawn as questions). The O(1) change makes cost independent of
each growable count below. The thresholds for any cost finding (SR): an audio block that misses its
deadline (a dropout) and a UI tick over 33 ms (visible lag) are findings. A cost below measurement
noise is dropped. A cost between the two is reported as numbers.

### Growable, with no limit in the code

| Quantity | S | B | P |
|---|---|---|---|
| Controller rows | unbounded | unbounded | n/a |
| Mapping rows per controller | unbounded | unbounded | n/a |
| Connect messages per row | unbounded | unbounded | n/a |
| Knobs in a gesture | any number, up to every knob and depth; Sheaf stores membership on each parameter (SR 2026-09-23) | same | n/a |
| Materialized modulation depths | 1440 slots provisioned at launch (`kDepthParameterStorageCapacity`, 96 parameters × 15 sources); the message thread adds more as they run low (`ParameterGroup::RequestParameterStorageBatchIfLow`, `Engine::MessageThreadTick`). A Randomize press can need a knob's old depths and its new ones at once; when no slot is free, the knob gets fewer sources than it drew (`partial` in `RandomizeParameterModulationDepths`, recorded by `LastRandomizePartial`, shown nowhere). **fix required** (SR 2026-09-23): the launch constant is the watermark; a patch applies only with its storage, at startup and running | same | same |
| Patches | disk | browser storage quota | n/a |
| Versions per patch | every Save adds one | same | n/a |
| Same-name suffix | smallest free number | same | n/a |

### The code's own limits

| Quantity | S | B | P | Symbol |
|---|---|---|---|---|
| Recording length | 30 min, then offered | 30 min, then downloaded | none (the DAW records) | `kMaxRecordSeconds` (OP) |
| Recording buffer | ~345 MB at 48 kHz, 16-bit mono WAV | same | n/a | `ArmRecording` comment |
| Tempo | 30–300, step 1.0 | 30–300 | host tempo, or 30–300 without one | `kFroggersBpmMin/Max` |
| PPQN | 1–960 | 1–960 | n/a (no Sync page) | `SyncConfig::IsValid` |
| Modulation levels | 3 | 3 | 3 | `kMaxDrillLevel` |
| Sources per view | 15 (13 until an input connects) | 15 | 15 | `kNumModulators` |
| Crispy pages per Randomize All | 0–2 | 0–2 | 0–2 | `kMaxRandomizedCrispy` |
| Scenes | 2 | 2 | 2 | `kNumScenes` |
| Gesture faders on main | 8, reached only through the Analogs Gestures rows (D-03) | 8 | 8 | `kNumGestures` |
| Host parameters | n/a | n/a | 92 | PLG-08 |
| Input channels | 1 requested | 1 requested, at most 32 | stereo bus: None, each channel, or Sum | `kMaxBrowserInputChannels` |
| MIDI output sinks | 8; a sink index of 8 or more is ignored | same | n/a | `MidiSender::kMaxSinks` |
| Absolute-feedback routes | 4096; a route past that is not tracked | same | n/a | `AbsoluteFeedbackCoordinator::kMaxRoutes` |

---

# 5. Manual changes the rulings imply

Line numbers are MANUAL.md at 2255303, which is byte-identical to the snapshot's MANUAL.md.

1. **Connect messages (SR D-02; CON-01 to CON-03).** Add a subsection after "### Editing a field"
   (after line 349):

   > ### Connect messages
   >
   > Every row's editor, Custom included, has a **Connect messages** section. Each entry is one SysEx
   > message the row sends to its MIDI out when that output connects. A preset that switches its
   > device into a mode installs its switch message here: **Akai APC40 mkII (Ableton)** carries
   > `F0 47 7F 29 60 00 04 41 09 07 01 F7`, and each Launchpad preset carries its programmer-mode
   > message. **Add** appends an empty message, `F0 F7`. Type a message into its **Message** field
   > as hex byte pairs, for example `F0 00 7F F7`. A message must be one SysEx message: `F0` first,
   > `F7` last, and data bytes `00` to `7F` between them. Any other entry is refused, and the stored
   > message stays. The **x** beside a message deletes it.

2. **The offline port option (SR D-04; PRT-01).** Add after line 303 ("...online, offline, not
   set."):

   > While a row's device is unplugged, its **MIDI in** or **MIDI out** selector lists **(none)**,
   > every connected device, and the stored port's name, which stays selected. Leaving that entry
   > selected keeps the port, and the row reconnects when the device returns. Choosing a connected
   > device binds the row to it, and choosing **(none)** clears the port.

3. **Gestures (SR D-03; GES-01 to GES-03).** In "What can be mapped", change lines 328–329 from
   "**Hold Drill**, and **Shift**." to "**Hold Drill**, **Shift**, and gestures (see Gestures,
   below)." Add a subsection after "### Shift" (after line 364):

   > ### Gestures
   >
   > A gesture is a set of actions the player chooses, grouped into one MIDI control. Operating that
   > control does every action in the set.
   >
   > To build one, map a button to **Hold Gesture Select** with a gesture number from 0 to 7, and map
   > one control to the same gesture number on an Analogs **Gestures** row. While the button is held,
   > turning a knob, on the controller or on screen, adds that knob to the gesture, in the scene the
   > blend is on (both scenes while the blend sits between them). The first turn of an endless knob, or
   > the first drag of an on-screen knob, only adds it and moves nothing; a control that sends absolute
   > positions adds the knob and sets it in the same move. From then on, every endless turn or drag of a
   > knob in the gesture, with the button held or not, is shared between the knob's own value and the
   > setting the gesture takes it to, by the gesture control's position: at the top it moves only that
   > setting, at the bottom only the knob. Moving the gesture's control takes every knob in the gesture
   > from its own value toward its setting. A Randomize pressed while the button is held adds every knob
   > it re-sets to the gesture, and the depths it draws on that press stay neutral, since the first
   > write to a depth only adds it to the gesture.
   >
   > The gesture's mappings are saved with the row's other mappings. Which knobs belong to the gesture,
   > and the setting it takes each one to, are part of the patch: saving a patch keeps them, opening a
   > patch brings back the ones it was saved with, and quitting without saving loses any change to them,
   > like any other unsaved edit. Reset Page takes that page's knobs out of every gesture, and Reset All
   > on a parameter page takes every knob out. In a modulation view, the depths Reset returns to their
   > launch values leave their gestures too.

   SR 2026-09-23: gestures follow Sheaf, so this text states Sheaf's behaviour as Frogg3rs exposes it.

4. **Reset All inside a modulation view (SR R1; RST-04).** The manual names Reset All only as a
   mappable control (line 327) and as the APC40's DETAIL VIEW job (line 409). It has no Reset
   subsection. Add one after the Randomize subsection (after line 215):

   > ### Reset
   >
   > **Reset All** on a parameter page returns the whole patch to its launch state: every page's
   > values, every modulation depth, every page's Crispy, and Crunchy. Pressed while a modulation
   > view is open, it instead returns that parameter's depths to their launch values and clears the
   > depths one level below them, where that level exists. Nothing outside that parameter changes.
   >
   > **Reset Page** on a parameter page returns that page's values and its own Crispy to their
   > launch state and leaves every other page and Crunchy alone. In a modulation view, it returns
   > that view's depths to their launch values.

   The R1 ruling covers only the second sentence of the Reset All paragraph. The other sentences
   restate RST-01 to RST-03 (HR "Reset All is global", and `froggers-transport-and-reset-controls`),
   because the manual has no Reset subsection to hold the new sentence.

5. **WRLD.Bldr's removal (MR 2026-09-23; D-01).** Three corrections in "Overview":
   - Lines 269–271: replace "a port no preset recognizes, a recognized device missing its other port,
     or a WRLD.Bldr, whose preset recognizes no port name at all." with "a port no preset recognizes,
     or a recognized device missing its other port."
   - Lines 273–275: replace "**Launchpad Pro MK3**, **Launchpad Mini MK3**, **WRLD.Bldr**, or
     **Custom**" with "**Launchpad Pro MK3**, **Launchpad Mini MK3**, or **Custom**".
   - Lines 280–282: delete "The WRLD.Bldr preset recognizes no port name, so a WRLD.Bldr always
     appears under "Other inputs" and "Other outputs" rather than as a waiting device; after adding
     its preset, the player picks its ports on the row."

---
