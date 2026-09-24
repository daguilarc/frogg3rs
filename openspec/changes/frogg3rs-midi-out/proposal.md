# Proposal — `frogg3rs-midi-out`

This change gives Frogg3rs one MIDI out that sends either the output level as
a Control Change or the output's pitch as monophonic notes, set on the
Controllers page in the standalone and browser builds and on the instrument's
surface in the plugin builds. It depends on the Sheaf change `app-midi-out`,
which adds the per-block MIDI-out list, the sender's MIDI-out sink, the runtime
setting, the Controllers page section and the port bindings. It was written
against frogg3rs `main` at `2255303`, which pins Sheaf `62829a4a`. By the
operator's ruling, midi-out is developed and pushed on its own branches
(frogg3rs `midi-out`, Sheaf `app-midi-out`); it is rebased onto main as main
stands when the operator says the other sessions' work on main is done —
frogg3rs `main` for this branch, Sheaf's upstream `main` for its Sheaf branch
(task 0) — and open changes nobody is working on are not waited for.

The evidence directory cited below is
`/Users/diegoaguilar-canabal/.claude/projects/-Users-diegoaguilar-canabal-Desktop/fca76ec7-1fa9-41b0-854c-718d9051aa6c/evidence/`.

## Why

The operator's story, in their rulings:

1. Frogg3rs gets ONE MIDI out, carrying the app's audio output.
2. The user picks what it sends: the output level as a continuous CC, or the
   output's pitch as MIDI notes, tracked monophonically. It sends one or the
   other, never both. 36–100 ms of pitch latency is acceptable.
3. MIDI out is off by default and ships with no presets.
4. It is wanted on the standalone, VST3, AU and browser builds, but only where
   feasibility is shown.

Today nothing leaves the app as MIDI except controller feedback, clock and
transport, and the plugin declares no MIDI output at all (feasibility claim
C6; Frogg3rs adjudication adj-M, M3):

```
$ grep -n "producesMidi\|acceptsMidi" app/vst/FroggersPluginProcessor.hpp
193:    bool acceptsMidi() const override { return true; }
194:    bool producesMidi() const override { return false; }
$ grep -n "IS_SYNTH TRUE\|NEEDS_MIDI" app/vst/CMakeLists.txt
130:    IS_SYNTH TRUE
134:    NEEDS_MIDI_INPUT TRUE
```

## What Changes

- **Two contents, Off by default.** The app's MIDI catalog lists Level, which
  sends one Control Change, and Pitch, which sends monophonic notes. Off is the
  default; channel 0, CC 16 and velocity following the level are the editable
  fields' defaults.
- **Level.** A new envelope follower on the output's mono fold; a Control
  Change at a block's last frame when its rounded value changed and at least
  20 ms of output have passed since the last one.
- **Pitch.** Cycfi Q's pitch detector, added with its infra dependency as
  submodules under `External/`, runs on the audio thread on the same mono
  fold, one call per sample; each note change is stamped at the frame the
  detector reported it. K = 1. A report whose frequency is 0 sends nothing.
  Velocity from the level or a fixed value; a note-off when the output's
  level falls below 0.001 (−60 dBFS), when Pitch stops, or when the channel
  changes.
- **Plugin.** One declared MIDI output, the host's input cleared every block,
  the app's messages written at their frames; a MIDI button and three fields on
  the plugin-mode surface, saved in the host project.
- **Manual.** An Audio to MIDI section naming only the hosts and browsers the
  operator's runs confirmed, and the Sync page's Send clock and Send transport.

## Story steps each requirement serves

Steps are numbered as in Why above. The spec states behaviour only; this table
carries the story citations.

| Requirement (`froggers-midi-out`) | Story steps |
|---|---|
| The MIDI out sends one chosen content, and nothing by default | 1; 2 ("one or the other, never both"); 3 ("off by default and ships with no presets") |
| Level sends the output level as a Control Change at most 50 times a second | 2 ("the output level as a continuous CC") |
| Pitch sends the detected fundamental as monophonic notes | 2 ("the output's pitch as MIDI notes, tracked monophonically"; "36–100 ms of pitch latency is acceptable") |
| The MIDI-out path runs on the audio thread and misses no audio deadline | 4 ("only where feasibility is shown") |
| No note is left sounding | 2 (a held note no longer follows the output's pitch) |
| The plugin offers one MIDI output carrying only the app's messages | 1; 4 (VST3 and AU) |
| The plugin player sets the MIDI out on the instrument's own surface | 2 and 3 in the plugin builds; 4 (the shared configuration must not switch the plugin on) |
| The manual says where the MIDI out works and what it sends | 4 ("only where feasibility is shown") |

The Sheaf requirements this change depends on (`app-midi-out`) are mapped in
that change's proposal. Its rule that a bad `midiOut` entry resets only itself
serves the ratified story's requirement that the controller setup persists
across relaunch (story items CTL-17 and QR-05, in the operator's ratified
story, which a separate change is committing to this repository).

## Rulings folded in

These were settled by the operator (relayed by the coordinator) and refine
the rules named beside each:

- Delivery gates (omni rule §7, dependencies stated before each step): plugin
  delivery waits on R2 and R7; browser delivery waits on R5 and R6.
- Threshold framework (omni rule §2, a figure carries the run that produced
  it): a finding is a missed audio block deadline, which is an audible
  dropout; a worst block below the budget is reported and gates nothing.
  Level CC was first shown feasible on the standalone by R4 at 48 and 96 kHz
  with 64 and 256 frames (the 48 kHz, 256-frame worst block at 14.9% of the
  budget), with its operation count from the code reading, C13. M1, now
  recorded from measure-q/report-range.md, timed the follower and the pitch
  detector together at 48 and 192 kHz, where this machine misses the
  deadlines without MIDI out only at 192 kHz; browser Level and Pitch are
  recorded feasible by M6, the same measurement in the real browser build.
- Channel, CC number and note velocity are editable fields, not presets:
  channel 0, CC 16 (General Purpose Controller 1), velocity from the output
  level at note-on, scaled to 1–127.
- Channel (`app-midi-out`, Q1): the MIDI-out section's Channel field is the
  same construct the controller rows' channel fields use, and displays the
  same numbering they do. It stays stored 0–15, and the plugin surface's
  Channel field here follows the same convention.
- Title (`app-midi-out`, Q1): the section is titled "Audio to MIDI", saying
  what it sends. The controller rows' "MIDI out" caption is untouched.
  Wherever the section is named (the manual, spec scenarios) it is named by
  that title. The plugin's MIDI button labels ("MIDI: OFF", "MIDI: LEVEL",
  "MIDI: PITCH") name the button's state, not the section, and stay.
- The MIDI out port carries only the audio-output messages; clock and
  transport never reach it (`app-midi-out`, smi-11).
- The port picker's page and the setting's host scoping follow the code; both
  are recorded under Structural decisions with their evidence.
- The pitch detector (coordinator ruling, relaying the operator's approval of
  the download; replaces the earlier design ruling that ran YIN on a thread
  of its own over a decimated ring, and refines the Pitch requirement and
  omni rule §7, dependencies stated before each step). Pitch detection uses
  Cycfi Q's pitch detector from Q's q_lib (Boost Software License 1.0) at
  commit `0920556c6eacb0091634310950f0c0ff5d66434a`, with its infra
  dependency at `2dff97a4b107eced78e426152f5001a2331cb1cf` (MIT), added to
  frogg3rs as submodules under `External/`, both license texts shipping with
  them. It runs on the audio thread on the mono fold. The reason, from the
  coordinator's research: YIN's window cannot meet the 100 ms bound, and Q
  detects within about two periods (Cycfi Research) and runs per sample.
  Measured support: the YIN design's own latency run
  (`measure-pitch/m3_output.txt`) gave a worst of 159.979 ms at K = 1 at both
  128 and 256 frames; by reading Q's source, its window is two periods of
  the detector's lowest frequency (Q's bacf_period_detector constructor), and
  measure-q measured a report every 20.000 ms at 48 kHz with a 50 Hz lowest
  frequency. This removes the YIN tracker, the analysis thread, the ring, the
  decimator and its shared Butterworth design, the evidence directory's
  `pitch-tracker/` headers, the browser's second-thread question (M5) and its
  fallback slices, the pacing that served the analysis thread, and M2. From
  the earlier ruling, the mono fold computed once per sample and read by
  Record, the mono device write, the follower and the detector stays, as does
  Pitch's presence decided per build in `kFroggersOffersPitch`.
- K = 1 (coordinator ruling): the only K whose worst latency is at or under
  100 ms at both 128- and 256-frame blocks (M3, from measure-q part 1: 75.417
  and 94.271 ms at K = 1; 94.958 and 113.312 ms at K = 2).
- Q-R: Pitch's range (coordinator ruling, replacing the open question this
  proposal carried). The detector's lowest frequency stays 50 Hz and its
  highest frequency is 5,000 Hz: 5,000 Hz because the VCOs reach 5 kHz and
  Pitch sends the output's pitch, and 50 Hz because it is the lowest floor
  the <=100 ms latency ruling allows. The manual says notes below 50 Hz are
  not tracked. `measure-q/report-range.md` re-ran the latency, octave-count
  and cost measurements at this range: K = 1 latency is unchanged (75.417 and
  94.271 ms, identical to the original 50-1,500 Hz run, since Q's window and
  report cadence are set by the lowest frequency alone, unchanged at 50 Hz);
  default-patch, phase-modulation-maximum and comb-feedback-maximum note and
  octave counts are unchanged, and ring-modulation-maximum moved from 25
  changes/2 octave jumps to 30/5, since the wider 5,000 Hz ceiling admits
  some of that patch's higher-frequency inharmonic content the narrower
  range excluded. Every figure this proposal and `tasks.md` carried at
  50-1,500 Hz is replaced by `report-range.md`'s figures at this range.
- Level rate (coordinator ruling, from audio-to-CC practice, which reports a
  level at 1 to 50 Hz, for example Gig Performer's envelope follower): a
  Control Change is sent at most once per 20 ms and only when the 7-bit value
  changed. Its frame stamp is the frame whose level it carries: the value is
  read at each block's last frame, a change inside the 20 ms is not queued,
  and the first block end past the 20 ms sends the level at that frame,
  stamped there, if it still differs.
- Measurements (coordinator ruling): M1 and M3 are recorded from measure-q,
  reconfirmed at the Q-R range by `measure-q/report-range.md`; M4
  (`app-midi-out`) from measure-native-2's final run; M5 is withdrawn, since
  no second thread is needed; M6 is recorded from `measure-m6/report.md`: no
  block misses its deadline except worklet callback 0, a startup transient
  every baseline also shows, and browser Level and Pitch are shown feasible.
  No measurement remains open.
- The sender and the detector start only on the main thread. `MidiSender::Start()`
  must never run inside the AudioWorklet scope (coordinator ruling, from M6):
  spawning its worker's pthread there is not a safe call under
  `-sPTHREAD_POOL_SIZE=1` and silently stops the worklet. Constructing the
  pitch detector runs on the main thread too, but for its own reason:
  construction allocates, and allocation must stay off the audio thread; Q's
  headers spawn no thread, so the detector was never itself the cause of the
  worklet stopping. M6's first build recorded zero worklet callbacks because
  both were triggered lazily on first use from inside the worklet thread,
  which also spawned `MidiSender::Start()`'s pthread there; the rerun, with
  both moved to run during prepare on the main thread, is confirmed clean.
  Task 6's detector is constructed in
  `PrepareToPlay`, which runs wherever `Engine::Prepare()` is called: from
  `Runtime::audioDeviceAboutToStart` in the standalone, from
  `synth_browser::Runtime::Prepare` on the browser's main thread, and from the
  plugin's `prepareToPlay`, whose thread JUCE leaves to the host. None of
  these is the audio callback. Task 6's check reads a construction count
  after `PrepareToPlay` and before any block, so it fails against a detector
  constructed lazily on the first `ProcessBlock`.
- Offline render and host-block delay (the earlier rulings Q-C and Q-B). Q-C
  ran the analysis inline on the processing thread while the host reports
  non-realtime rendering, because the analysis otherwise ran on its own
  thread; the detector now runs on the processing thread in every mode, so a
  bounce sends the notes live playback would with no separate path, and
  nothing of Q-C remains. Q-B split the delay of reading a thread's result
  once per host block from the tracker's own latency; the detector now
  reports on a sample and the note is stamped at that frame, so there is no
  block-read delay and the split is gone, with the manual sentence that a
  larger buffer adds up to its own length. What stays from Q-B: the latency
  is measured at host blocks of 128 and 256 frames and both figures are
  reported, and the manual names 128–256 frames as the buffer sizes the
  latency was measured at.
- The manual (coordinator ruling): it says the default patch's release tails
  make its tracked pitch move (`measure-floor/report.md`), and that Pitch is
  monophonic. measure-floor traced the tails: for about the last 100 ms of
  each closed gate half the output is VCO2 alone at 220 Hz, note 57. Over the
  same patch, measure-q counted 3 note changes and no octave jumps in 60 s at
  K = 1; by reading Q's source, its detector's bias step keeps the current
  frequency when a new estimate is a whole-number multiple or fraction of it
  within about half a semitone. No run recorded which notes those 3 changes
  were. Task 10 therefore writes that the tails move the output's pitch and
  gives the counts task 6's count test measured on the shipped code; it names
  no note held through the tails.
- Unmeasured behavioural premises are measurement tasks that run first inside
  the before-code audit, before approval, each naming its deciding quantity
  and what the design does for every answer. M6, the last such premise in
  both changes, is now recorded (above); no measurement remains open.

## Data flow, per build

**Tap, every build.** `FroggersAppCore::ProcessBlock` (`app/FroggersAppCore.hpp`)
computes each output sample with `RouteAudioSample()`, which returns
`SanitizeOutputSample(reverbOut)`; the same `sample` is written to the device
and, while Record is armed, to `recordBuffer_` as `0.5f * (sample.l + sample.r)`.
`ProcessBlock` forms that mono fold twice today, for Record and for a mono
device; the change computes it once per sample and every reader uses it. Each
sample the fold feeds a new `dsp::SingleEnvelopeFollower`
(`app/dsp/EnvelopeFollowers.hpp`) and a new instance of Q's pitch detector (cycfi::q::pitch_detector),
whatever the content, so both are current whenever a content is chosen. While
Pitch is chosen, a sample where the detector reports a nonzero frequency, with
the follower's level at or above 0.001, sets the sounding note at that frame;
a report whose frequency is 0 changes nothing; and the first sample whose
level is below 0.001 while a note sounds ends that note and resets the
detector. At
the end of the block the app appends at most two messages to the block's
MIDI-out list (`app-midi-out`, sar-37): for Level, a Control Change at the
block's last frame when the rounded level changed and at least 20 ms of output
have passed since the last one; for Pitch, a note-off and a note-on stamped at
the frame where the block's final note was set.

**Standalone** (`app/FroggersMain.cpp` hosts Sheaf's runtime). Sheaf's
`Runtime::audioDeviceIOCallbackWithContext` → `Engine::ProcessBlock` →
`FroggersAppCore::ProcessBlock` → the engine turns each appended entry into an
app channel message due at its frame's output time and calls
`MidiSender::TryEnqueue` → the sender's worker delivers it only to the MIDI-out
sink → `synth_juce::MidiOutputHandler::SendScheduled` → the port chosen on the
Controllers page.

**Browser** (`app/browser/build-browser.sh` builds through Sheaf's
`build-browser-apps.mjs`). The worklet callback in Sheaf's `BrowserRuntime.hpp` →
`ProcessAudioWorkletPlanarBlock()` → `Engine::ProcessBlock` → the app → the same enqueue → `BrowserMidiBridge`'s
MIDI-out `OutputSink` → `midi.ts` `drainOutputsNow` → `port.send(bytes,
dueTimeMicros / 1000)` on the chosen `MIDIOutput`. The detector runs inside the
worklet callback like every other sample's work; the browser needs no second
thread for it. Q's headers reach this build through the manifest's
`includeDirs` and one `--allowed-source-root` argument per Q include
directory in `app/browser/build-browser.sh`, which `pages.yml` runs (task 2).

**VST3 and AU.** `FroggersPluginProcessor::processBlock`
(`app/vst/FroggersPluginProcessor.cpp`) clears `midiMessages`, builds its
`AudioBlock` and calls `engine_.ProcessBlock` (routing is not enabled in this
host), then reads the engine's MIDI-out list and calls
`midiMessages.addEvent(bytes, 3, frame)` for each entry. JUCE's wrappers hand
the buffer to the host: VST3 through the wrapper's MidiEventList::pluginToHostEventList
when `JucePlugin_ProducesMidiOutput` is set, AU through `pushMidiOutput`
(adj-M, M3, JUCE 8.0.12 sources). Realtime and offline rendering run the same
path.

**Setting.** Standalone and browser: the Controllers page edits the runtime
configuration's `midiOut`; the engine hands the content id, channel, CC number
and velocity to `FroggersAppCore` on the message thread through the app-context
callback, once during `Engine::Initialize` and again on each change; the app
core resolves the id to a content index and packs all four into one atomic
32-bit pending slot, which the audio thread takes at the start of the next
block, the shape `pendingExternalAudioRouted_` already has. Plugin: the surface's MIDI button and fields call
the same app-core entry point directly, and the plugin saves them in its
session extras beside `inputSelection`, through one function both
session-extras writing sites call.

## Structural decisions

- **The port picker is on the Controllers page.** In the browser, Web MIDI is
  requested only from the Controllers sidebar action or from a grant the browser
  already holds (active Sheaf change `gate-browser-midi-on-controllers`). A
  picker on the Audio I/O page would list no ports for a player who never
  opened Controllers. The standalone uses the same page so the two builds
  match.
- **The setting is host-scoped.** The plugin loads the standalone's own
  configuration file: `ProductionDataPaths()` in the plugin resolves the same
  root as `app/FroggersMain.cpp` (adj-M, S3). The library forwards `midiOut` to
  the app only in hosts that enable MIDI-out routing, and the plugin does not.
  The plugin's own setting lives in its session extras, the same place its
  input selection lives (`kSessionExtrasKey`, `kInputSelectionKey` in
  `FroggersPluginProcessor.cpp`).
- **The plugin surface follows the IN button's pattern.** The plugin has no
  Controllers page (adj-M, M2); its only host-specific control today is the IN
  button in the plugin-mode transport row. The MIDI button sits beside it, and
  the three text fields sit in a row beneath it. The MIDI button is the second
  plugin-mode cycling picker, so both it and the IN button run on one NEW
  `CyclingHostPicker`; their actions join `kInputSelect` in the catalog
  coverage check's exclusion list, since the catalog does not offer
  host-only controls.
- **One producer, per-host consumers.** The app appends to the block's list in
  every build and never names a sink; the engine routes it in standalone and
  browser, and the plugin copies it into the host buffer.
- **The level follower is a new instance.** `externalAudioEf_` follows the
  external input, not the output, so the MIDI out gets its own
  `SingleEnvelopeFollower` with the same coefficients.
- **The pitch detector runs on the audio thread, per sample.** R4 measured a
  whole YIN analysis inside the block at 126–141% of the block budget, which
  is why the earlier design moved YIN to a thread of its own. Q's detector
  instead does a zero-crossing step per sample and, once per report, an
  autocorrelation over a bitstream of its window (Q's bacf_period_detector);
  measure-q/report-range.md Table 3 timed the fold, follower and detector's
  combined share, at the ruled range, as low as 3,709 ns and as high as
  47,333 ns of a block's own worst time, one to three orders of magnitude
  below the baseline engine call's own worst-case stalls at every setting
  measured. It is constructed only in `PrepareToPlay`, never lazily from the
  audio callback on first use, because construction allocates and allocation
  must stay off the audio thread. It is constructed alongside
  `MidiSender::Start()`, which M6 found must never run from inside the
  AudioWorklet's own thread, since spawning its worker's pthread there
  silently kills the worklet under `-sPTHREAD_POOL_SIZE=1` (coordinator
  ruling). Q's detector has
  no default constructor, so the member is an optional that `PrepareToPlay`
  emplaces (task 6). Its per-sample call writes only into storage sized at
  construction (Q's bitset and the zero-crossing collector's ring buffer), so
  the audio thread neither allocates nor waits.
- **Notes are stamped where the detector reported.** Because the detector
  runs per sample, the frame of each report is known, and the note is due at
  that frame's output time. The latency on the output timeline is then the
  latency measure-q measured, with no block-read delay added.
- **At most one note change per block.** The detector reports every 960
  samples at 48 kHz on average (measure-q: 960.01), but by reading Q's
  zero-crossing collector a report that falls due after a rising edge waits
  until the signal drops below the collector's hysteresis, and the next
  report comes that many samples sooner, so two reports can fall in one
  block even of 20 ms or less. The
  block sends only the note sounding at its end, stamped where it was set,
  keeping the MIDI-out list at the two entries sar-37 sizes it for; a note
  set and replaced inside one block is not sent.
- **A lost pitch is the output going quiet** (coordinator ruling). In silence
  Q's detector stops reporting, so a note rule that waited for a report
  saying "no pitch" would hold the last note forever. The app ends the
  sounding note on the first sample whose follower level is below 0.001
  (−60 dBFS) and calls the detector's reset, which sets its held frequency to
  0, so the next note is not biased toward the ended one. A note-on is taken
  only while the level is at or above 0.001, since below it the same rule
  would end the note on the next sample. By reading Q's source, a report can
  carry frequency 0 (before the first report periodic enough to set one, and
  after that reset); such a report sends nothing, as measure-q's harness
  skipped it, and the note formula is never evaluated on it.
- **The shipped detector's arguments and note rule are both measured.** The
  constructor arguments are the ones measure-q used. The note-off on a quiet
  output, with its reset, was measured in `measure-q/report-shipped-rule.md`:
  per-case note-on and octave-jump counts (default patch 3 and 0; phase
  modulation at maximum 677 and 114; ring modulation at maximum 30 and 5;
  comb feedback at maximum 3 and 0); worst latencies of 75.417 ms at
  48 kHz/128-frame and 94.271 ms at 48 kHz/256-frame; render setup with
  `sampleRate = 48000.0`, `blockSize = 128` (or 256). Task 6's count and
  latency tests run the shipped path and assert these numbers, and a
  difference is shown by the test and reported, never absorbed. Post-stop
  decay: the default patch's level falls below 0.001 at 0.2823 s after
  transport stop, per `measure-q/report-shipped-rule.md` item 3.
- **The plugin's MIDI button reads the catalog.** Its options are Off and the
  catalog's MIDI-out contents, so Pitch's presence per build is decided once,
  in `kFroggersOffersPitch`, which the standalone and plugin share and the
  browser sets apart under `__EMSCRIPTEN__`. Its three fields check entries with the same
  `ParseAppMidiOut*` functions the Controllers page and the configuration
  loader use (`app-midi-out`).

## Evidence for the measured premises

From the MIDI-out feasibility runs. Their reports and probes were kept in
session scratchpads that were cleared when the machine restarted; the lines
below were quoted from them before that:

```
R4 follower, 48 kHz / 256 frames:
  baseline_run1: max_ns=762167  baseline_run2: max_ns=740000  spread=22167
  with_follower: max_ns=795917  (within_spread=false, +33750ns vs higher baseline)
R4 in-block tracker, 48 kHz / 256 frames:
  analysis_block_max_ns=7546791 other_block_max_ns=1712416
  analysis_block_max_as_fraction_of_block=1.415023
R8: $HOME/JUCE is JUCE 8.0.12 (29396c22); juce_audio_plugin_client_VST3.cpp:3901 and
    juce_audio_plugin_client_AU_1.mm:276 call ensureSize (2048)
```

R4's other three follower settings stayed within their spread. R4 ran on an
Apple M2 (arm64); the feasibility note on subnormal release tails applies to
x86 hosts without flush-to-zero and was not exercised.

From the evidence directory, kept on disk:

```
measure-q/qlatencyrange_output.txt (48 kHz, pitch_detector(50 Hz, 5000 Hz, -30 dB), K=1 only, run exit 0):
  128-frame: observed report cadence: avg 960.01 samples (20.000 ms) between reports
  128-frame: K=1 worstLatencyMs=75.417 (measured 20/20 transitions)
  256-frame: K=1 worstLatencyMs=94.271 (measured 20/20 transitions)
  positive control (added 10.000ms delay): delta=10.000ms at both block sizes
  default patch  K=1:changes/min=3.0 octaveJumps=0
  phase modulation at maximum  K=1:changes/min=677.0 octaveJumps=114
  ring modulation at maximum  K=1:changes/min=30.0 octaveJumps=5
measure-q/qcostrange_output.txt (range 50-5000 Hz, 2 s/pass, run exit 0 after the fixedBlockFrames fix):
  48 kHz/128-frame, default (rep1): combined worst=1,090,791 ns  budget=2,666,667 ns  combined missed 0/750
  192 kHz/varying 1..2048, default (rep1): combined worst=5,007,750 ns  budget=10,666,667 ns (@2048fr)  combined missed 231/678, same as the baseline's 231/678
  positive control (padSinIterations=1000, 192 kHz/varying): combinedWorstNs moves from 5,306,708 to 12,430,250; combinedMissed from 231/678 to 572/678
measure-floor/report.md: default patch, gate closed, VCO2's 220 Hz tail alone
  for about the last 100 ms of each closed half (a110 < 0.007, r(1/220) 0.96-0.98)
measure-pitch/m3_output.txt (the YIN design, superseded):
  K=1 worstLatencyMs=159.979 at 128 and at 256 frames
measure-m6/report.md (browser worklet, 48 kHz AudioContext rate, 128-frame render quantum, 2,666.7 µs budget; the Emscripten clock these figures come from resolves to 1 ms, so every block time below is a multiple of 1000 µs):
  both baselines and the additions pass each show one miss, all the same block-0 startup transient (3000.0 µs, 1.1250x budget); no other block in any of the seven passes reaches it
  additions pass: additionSampleCalls=345,600  additionBlockCalls=2,700, both exactly the expected per-block/per-sample counts
  positive control: 200 sin/sample leaves the worst block unmoved at 3000.0 µs; 2000 sin/sample moves it to 6000.0 µs (2.2500x budget)
```

The full tables are in `tasks.md` under M1 and M3.

## Operator runs that gate delivery

R2 and R7 gate plugin delivery; R5 and R6 gate browser delivery; R9 gates
standalone delivery, being the one run of the whole standalone chain (routing
enabled in `Runtime::Start`, the port-changed callback, the Controllers-page
port choice, and `MidiOutputHandler::SendScheduled` to a real port), which no
automated test covers. They need
real hosts and real browser visibility and cannot run headless. Their
confirming and clearing results are in `tasks.md`, taken from the feasibility
verification.

## Functions and classes this change touches

- `FroggersAppCore`: `ProcessBlock`, `Init`, `PrepareToPlay`, a new setter
  for the MIDI-out setting, a new follower member, a new
  member holding Q's pitch detector (cycfi::q::pitch_detector), the note
  state and the Level stamp state.
- `FroggersMidiCatalog()`: the two contents; NEW `kFroggersOffersPitch`.
- `FroggersUiSurface`: the plugin-mode transport row in `BuildTree`,
  `HandleAction`, new node ids and actions, NEW `CyclingHostPicker` holding
  both the IN button's and the MIDI button's options, selection and
  callback.
- `FroggersPluginProcessor`: `processBlock`, `producesMidi`, the constructor's
  callback registration, NEW `BuildSessionExtras` called by the constructor's
  session-state seed and `PumpStatePersistence` (the two sites that write
  session extras), and the restore path that reads `kInputSelectionKey`.
- `app/Makefile`: the Q include paths, the new test file's build entry, its
  binary in `test`'s prerequisites and recipe, and the new actions in
  `check-catalog-covers-screen-actions`' exclusion list.
- `app/vst/CMakeLists.txt`: `NEEDS_MIDI_OUTPUT TRUE`; the Q include paths.
- `app/standalone/CMakeLists.txt`, `app/browser/frogg3rs-browser-apps.json`:
  the Q include paths.
- `app/browser/build-browser.sh`: one `--allowed-source-root` per Q include
  directory. `app/build-launcher.sh`: the Q include directories passed to
  Sheaf's `apps/sheaf-patch/Makefile` through the variable `app-midi-out`
  adds for them.
- `.gitmodules`, NEW submodules `External/q` and `External/infra`, NEW
  `External/infra-LICENSE.txt`.
- `MANUAL.md`: an Audio to MIDI section; the Standalone paragraph's Sync page
  sentence, which omits Send clock and Send transport (feasibility claim C15).
- Tests: `app/vst/FroggersVstHostTests.cpp`, a new app-level MIDI-out test file
  beside `app/FroggersAudioRoutingTests.cpp` built and run by `app/Makefile`,
  `app/FroggersMidiCatalogTests.cpp`, and a new browser spec in
  `app/browser/e2e/` reading the Controllers page's Sends options.

## Capabilities

### New Capabilities
- `froggers-midi-out`: what the MIDI out sends, the audio-thread deadline
  requirement, note-off rules, the plugin's output and surface controls, and
  the manual.

## Impact

- `app/FroggersAppCore.hpp`, `app/FroggersMidiCatalog.hpp`,
  `app/dsp/EnvelopeFollowers.hpp` (its header comment),
  `app/FroggersUiSurface.hpp`, `app/vst/FroggersPluginProcessor.hpp`,
  `app/vst/FroggersPluginProcessor.cpp`, `app/vst/CMakeLists.txt`,
  `app/standalone/CMakeLists.txt`, `app/browser/frogg3rs-browser-apps.json`,
  `app/browser/build-browser.sh`, `app/build-launcher.sh`, `MANUAL.md`.
- `.gitmodules`, `External/q` (Cycfi Q at `0920556c`, Boost Software License
  1.0, its `LICENSE` in its tree), `External/infra` (Cycfi infra at
  `2dff97a4`, MIT), `External/infra-LICENSE.txt`. CI's recursive checkouts
  also clone Q's nested `infra` and infra's three `external/` submodules;
  nothing includes them.
- Tests: `app/vst/FroggersVstHostTests.cpp`, `app/FroggersMidiCatalogTests.cpp`,
  NEW `app/FroggersMidiOutTests.cpp`, its build and run entries in
  `app/Makefile`, and one new spec in `app/browser/e2e/`.
- The submodule pin at `External/Sheaf`, moved to the commit carrying
  `app-midi-out`.
- Hygiene in touched files: `processBlock`'s header comment and the
  `acceptsMidi()` comment in `FroggersPluginProcessor.hpp` say the buffer is
  ignored every block, and the `NEEDS_MIDI_INPUT` comment in `CMakeLists.txt`
  says `processBlock()` ignores the MIDI buffer entirely; the class comment in
  `FroggersPluginProcessor.hpp` says the buffer is accepted and ignored; and
  `dsp::SingleEnvelopeFollower`'s header says it feeds only the external-audio
  modulation source, and its `SetSampleRate` comment calls its caller chain
  single. This change makes all six false. MANUAL.md's "The plugin's own
  surface shows only Freeze" sentence also stops describing the plugin's
  transport row, which gains the MIDI button (task 10). The `releaseResources` comment's "(zero input channels, by
  construction)" is already false (adj-M, S4). Moving the IN button onto
  `CyclingHostPicker` makes false the comments in `app/FroggersUiSurface.hpp`
  on `SetInputOptions`, `InputSelectButtonLabel`, `AppendTransportRow`'s
  input-select capture, `HandleAction`'s `kInputSelect` branch and the
  `inputOptionLabels_` member, and the comment in
  `app/vst/FroggersVstHostTests.cpp` that cites `inputOptionLabels_`'s
  comment (task 11); the new actions make `app/Makefile`'s "except the five"
  above `check-catalog-covers-screen-actions` false (task 9).
- No longer touched, after the Q ruling: `app/dsp/Drive.hpp`
  (`Oversampler2x::ConfigureCleanFilter` keeps its own Butterworth design, as
  there is no second instance), and no new header in `app/dsp/`.

## Overlap with active changes

Landing order (operator ruling): midi-out is developed and pushed on its own
branches, frogg3rs `midi-out` and Sheaf `app-midi-out`. It is rebased onto
main as main stands when the operator says the other sessions' work on main
is done — frogg3rs `main` for this branch, Sheaf's upstream `main` for its
Sheaf branch (task 0) — and the conflicts below are resolved on this side.
Open changes nobody is working on are not waited for.

Task 0 waits on that word.

- `frogg3rs-o1-audit` (worktree `.claude/worktrees/o1-audit`). None of its
  tasks is ticked, but its worktree holds uncommitted edits in five files this
  change also edits: `app/FroggersAppCore.hpp`, `app/FroggersUiSurface.hpp`,
  `app/vst/FroggersPluginProcessor.cpp`, `app/vst/FroggersVstHostTests.cpp`
  and `app/FroggersAudioRoutingTests.cpp` (`git -C .claude/worktrees/o1-audit
  status --short`). Shared sites:
  - Its task 1.3 rewrites comments in `app/FroggersUiSurface.hpp` (three),
    `app/vst/FroggersVstHostTests.cpp`, `app/vst/FroggersPluginProcessor.cpp`,
    `app/FroggersAppCore.hpp` and `app/FroggersAudioRoutingTests.cpp`; tasks 9
    and 11 here edit comments in the first two.
  - Its task 5.1 changes Record's capture in `FroggersAppCore::ProcessBlock`
    and `FroggersAppCore::ArmRecording`, beside this change's tap and the
    Record write task 5 moves onto the shared mono fold.
  - Its task 6.5, if its operator run confirms, changes
    `FroggersAppCore::PrepareToPlay`, where task 6 constructs the detector.
  - Its task 7.3 cites `juce::ignoreUnused(midiMessages)` in `processBlock`,
    which task 8 replaces by clearing the buffer (the plugin still takes no
    controller MIDI, so 7.3's reasoning holds with that citation updated); it
    also touches `FroggersMidiCatalog()` and `app/FroggersMidiCatalogTests.cpp`
    (task 3) and `MANUAL.md` (task 10).
  - Its task 4.9 rewrites the `releaseResources` "(zero input channels, by
    construction)" sentence that task 11 here also names; since it lands
    first, task 11 leaves that sentence as 4.9 wrote it.
  - Its tasks 3.1 and 3.2 make `check-citations-resolve` refuse a
    line-numbered citation into `External/Sheaf`; comments this change writes
    name symbols, never line numbers.
- `app-o1-audit` (same worktree, Sheaf): see `app-midi-out`'s overlap
  section; this change reads sar-37, smi-20 and sru-71, which are numbered
  apart from it.
- `ui-state-before-audio` (Sheaf, postflight tasks 2.1 to 2.3 open): its code
  sits in `Engine::ProcessBlock`, which `app-midi-out` task 4 edits; see that
  change's overlap section.

## Delivery

Pushed to `main` on `daguilarc/frogg3rs`, never as a pull request, once this
branch has been rebased onto `main` as it stands when the operator says the
other sessions' work on main is done (task 0), and after the Sheaf change is
pushed to the fork and the pin moves with it. The standalone
ships once the implementation passes and R9 confirms. The plugin builds ship
only after R2 and R7; the browser build only after R5 and R6. M6 recorded no
missed deadline, so browser Level and Pitch ship with the browser build once
R5 and R6 clear. A build whose gate clears ships without the part that
failed, as the operator rules at that point.

The worktree `.claude/worktrees/midi-out` belongs to the operator; no task
removes, moves or rebases it beyond task 0's rebase, which the operator
ruled.
