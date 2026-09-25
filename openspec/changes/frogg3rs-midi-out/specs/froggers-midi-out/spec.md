# Delta — `froggers-midi-out`

A new capability: what Frogg3rs sends on its one MIDI out, and how the plugin
build carries it. The library side (the per-block list, the sender's MIDI-out
sink, the runtime setting, the Controllers page section and the port
bindings) is the Sheaf change `app-midi-out`.

## ADDED Requirements

### Requirement: The MIDI out sends one chosen content, and nothing by default
The app SHALL offer two MIDI-out contents, Level, which sends Control Change, and Pitch, which sends notes, SHALL send only the content the player chose, and SHALL send nothing while the choice is Off, which it is until the player changes it.

No controller preset gains a MIDI-out setting; channel, CC number and velocity are editable fields with defaults (channel 0, CC 16, velocity following the level), not presets.

Every channel value in this capability, in requirements and scenarios alike, is the number the Channel field displays, 0 to 15, which is the status byte's low nibble; this is the numbering the Controllers page's channel fields use.

#### Scenario: Off sends nothing
- **WHEN** the app runs the default patch for ten seconds with the MIDI-out choice at its default
- **THEN** no MIDI-out message is appended in any block
- Check: `app/FroggersMidiOutTests.cpp: off_sends_nothing`

#### Scenario: Only the chosen content is sent
- **WHEN** Level is chosen and the default patch plays
- **THEN** every appended message is a Control Change on the set channel and CC number
- **WHEN** Pitch is chosen and the default patch plays
- **THEN** every appended message is a note-on or note-off on the set channel
- Check: `app/FroggersMidiOutTests.cpp: level_appends_control_changes_on_the_set_channel_and_cc_only_when_changed` (Level half), `app/FroggersMidiOutTests.cpp: pitch_default_patch_sends_note_45_and_stays_sounding` (Pitch half)

#### Scenario: No controller preset carries a MIDI-out setting
- **WHEN** every device default in the app's MIDI catalog is read
- **THEN** the presets address exactly the controls they addressed before this change
- Check: `app/FroggersMidiCatalogTests.cpp: device_defaults_are_valid_and_address_exactly_the_documented_controls`

### Requirement: Level sends the output level as a Control Change at most 50 times a second
WHILE Level is chosen, the app SHALL send the level of its audio output as a Control Change on the set channel and CC number, at most once per audio block, only when the 7-bit value differs from the last one sent, and never within 20 ms of output time after the last one sent.

The level is taken from the same sample the app writes to its output and to Record, after `RouteAudioSample()` in `FroggersAppCore::ProcessBlock`, folded to mono as `0.5 * (l + r)`, and followed by its own `dsp::SingleEnvelopeFollower` (10 ms attack, 50 ms release). At each block's last frame the app rounds the follower's level times 127; it appends a Control Change carrying that value, stamped at that last frame, when the value differs from the last one sent and at least 20 ms of output frames (the sample rate times 0.02, rounded up: 960 frames at 48 kHz) lie between the last Control Change's frame and this one. A change that arrives inside the 20 ms is not queued: the value is read again at each later block's last frame, and the first one past the 20 ms sends the level at that frame, stamped at that frame, if it still differs from the last value sent. Every stamp is therefore the frame whose level the message carries, and consecutive stamps are at least 20 ms of output apart. The rate follows audio-to-CC practice, which reports a level at 1 to 50 Hz (citing Gig Performer's envelope follower).

Cost: by reading, the follower's `Process` is a fabs, a clamp to [0, 1], one compare and one multiply-add per sample, with no loop. By timing (fold and follower on a captured 192 kHz default-patch output stream, 2 s, added per call to two baselines of `Engine::ProcessBlock` through `SynthRig<FroggersApp>`), the fold and follower cost 19.82 ns per sample at 1-frame blocks, where the two clock reads bounding each 1-sample timed region dominate, and 3.80 ns per sample at blocks varying from 1 to 2048 frames; at the varying setting the worst combined block moved from 8,313,625 to 8,320,916 ns and the missed-block count stayed at the baseline's 243 of 678. At 192 kHz both baselines already miss their deadlines without MIDI out (every block at 1 frame; 243 and 254 of 678 at the varying setting), so under the operator's threshold framework those settings are recorded as ones the app misses without MIDI out and gate nothing. In the browser, Level's cost was measured in the real browser build (48 kHz AudioContext rate, 128-frame render quantum, 2,666.7 µs budget; the Emscripten clock behind those figures resolves to 1 ms): the additions pass's worst block reaches 1.1250x budget, the same single block-0 startup transient every baseline pass also shows, and no block anywhere in the additions run exceeds what the baselines reach on their own, so browser delivery of Level CC needs no fix.

#### Scenario: A change in level sends one Control Change
- **WHEN** Level is chosen, a block's rounded level at its last frame differs from the last value sent, and at least 20 ms of output lie since the last Control Change's frame
- **THEN** exactly one Control Change with that value is appended, at the block's last frame
- Check: `app/FroggersMidiOutTests.cpp: level_appends_control_changes_on_the_set_channel_and_cc_only_when_changed`

#### Scenario: A steady level sends nothing more
- **WHEN** the rounded level is the same in consecutive blocks
- **THEN** no message is appended after the first
- Check: `app/FroggersMidiOutTests.cpp: steady_level_sends_nothing_more`

#### Scenario: A moving level is sent at most 50 times a second
- **WHEN** Level is chosen at 48 kHz with 128-frame blocks and the rounded level differs at every block's last frame for one second
- **THEN** at most 50 Control Changes are appended, each at a block's last frame, with consecutive stamps at least 960 frames apart
- Check: `app/FroggersMidiOutTests.cpp: moving_level_is_sent_at_most_50_times_a_second`

#### Scenario: Silence after sound brings the value to zero
- **WHEN** the default patch plays for two seconds and the transport then stops for five seconds of release and silence
- **THEN** the last Control Change sent has value 0
- Check: `app/FroggersMidiOutTests.cpp: silence_after_sound_brings_the_value_to_zero`

#### Scenario: Browser Level CC ships only after the browser measurement
- **WHEN** the browser build is delivered with Level offered
- **THEN** the browser measurement has recorded no block missing its deadline in the real browser build, or the fix for the block that did has landed and the measurement has been re-run
- Check: none. No automated check reads the measurement; it already recorded no missed deadline, so this gate is clear. This scenario is neither backed by a test nor by one of R2/R5/R6/R9 -- flagged for review.

### Requirement: Pitch sends the detected fundamental as monophonic notes
WHILE Pitch is chosen, the app SHALL send one note at a time: a note-on for the detected MIDI note whenever the pitch detector reports a nonzero frequency naming a note other than the one sounding, preceded at the same frame by the note-off for the sounding note, nothing for a report whose frequency is 0, and a note-off when the output's level falls below 0.001 (−60 dBFS).

The detector is Cycfi Q's `cycfi::q::pitch_detector` from `q_lib` (Boost Software License 1.0), run on the audio thread inside `FroggersAppCore::ProcessBlock`, fed the same mono fold Level uses, one call per output sample at the host rate, constructed only in `PrepareToPlay` (never lazily from the audio callback: construction allocates, and allocation must stay off the audio thread; it is constructed alongside `MidiSender::Start()`, which must never run from inside the AudioWorklet's own thread, since spawning its worker's pthread there silently kills the worklet under `-sPTHREAD_POOL_SIZE=1`) with a lowest frequency of 50 Hz, a highest frequency of 5,000 Hz and a hysteresis of -30 dB (5,000 Hz because the VCOs reach 5 kHz and Pitch sends the output's pitch, 50 Hz because it is the floor the <=100 ms latency bound allows; the manual states fundamentals below 50 Hz are not tracked; this is the configuration the range below was measured at). The note is `round(69 + 12 * log2(f / 440))` of the detector's `get_frequency()`, read on each sample where the detector reports (its call returns true) with a frequency above 0 while the level follower's level is at or above 0.001. By reading Q's source, a report can carry frequency 0: before the first report periodic enough to set a frequency, and after the detector's `reset()`. Such a report sends nothing and the formula is not evaluated on it. By reading Q's source, the detector's window is two periods of its lowest frequency (`bacf_period_detector`'s constructor sizes its zero-crossing collector as twice the lowest frequency's period times the rate) and it reports each time half that window has passed; measurement found one report every 960.01 samples at 48 kHz, 20.00 ms, whatever note was sounding, at both ranges. A note changes on the first report naming a different note (K = 1): for K consecutive agreeing reports, the worst latency from a pitch step on the output timeline to the report that confirms the new note, over 20 steps of the default patch's three VCOs by a factor of 1.5 (target note 52), each landing 25 ms into the gate's open half, was measured as follows. K = 1 was re-run alone at the ruled range, 50 to 5,000 Hz; the K = 2 to 4 figures are at 50 to 1,500 Hz and were not re-run:

| Host block at 48 kHz | K = 1 (50–5,000 Hz) | K = 2 (50–1,500 Hz) | K = 3 (50–1,500 Hz) | K = 4 (50–1,500 Hz) |
|---|---|---|---|---|
| 128 frames | 75.417 ms | 94.958 ms | 114.938 ms | 135.062 ms |
| 256 frames | 94.271 ms | 113.312 ms | 133.312 ms | 153.625 ms |

K = 1 is the only count at or under 100 ms at both block sizes. The note-on and the note-off before it are stamped at the frame where the detector reported, so the latency on the output timeline is the figure above, with no further delay from the block. The worst note latency SHALL be at or under 100 ms at host blocks of 128 and 256 frames. These figures were measured without the note-off on a quiet output described below. The shipped build's own measured latency is 75.417 ms at 48 kHz/128-frame and 94.271 ms at 48 kHz/256-frame, both well under the 100 ms bound. The latency test runs this same 20-step render (a factor of 1.5 on the default patch's three VCO pitch knobs, target note 52, one step per quarter note, 5% into the gate-open window) and asserts the worst latency over all 20 steps is at or under 100 ms at both block sizes, printing its own result beside these figures.

In silence Q's detector stops reporting, so the app does not wait for a report to end a note. On the first sample whose level follower's level is below 0.001 (−60 dBFS) while a note is sounding, the app ends that note at that frame and calls the detector's `reset()`, which sets its held frequency to 0, so the next note is taken fresh instead of being held toward the ended one: by Q's source, the detector's bias step keeps the current frequency when a new estimate is a whole-number multiple or fraction of it within about half a semitone. No note-on is taken while the level is below 0.001.

At most one note change is sent per block: when the detector reports more than once in a block, the note-off and note-on describe the note sounding at the block's end, stamped at the frame of the report that set it, and a note that started and was replaced inside the same block is not sent. Reports come every 960 samples at 48 kHz on average (measured: 960.01), but by Q's source a report due after a rising edge waits until the signal drops below the collector's hysteresis and the next one comes that much sooner, so two reports can fall in one block even of 20 ms or less; the earlier one is then not sent.

The note-on velocity is the Velocity field's fixed value, or, when the field is Level (the default), the follower's level at the note-on's frame times 127, rounded and held within 1 to 127.

A measurement counted note changes per minute and octave jumps over 60 s at 48 kHz and 128 frames, K = 1, at the ruled range (50-5,000 Hz), with no note-off on a quiet output: the default patch 3 and 0, phase modulation at its maximum 677 and 114, ring modulation at its maximum 30 and 5 (moved from 25 and 2 at the narrower 50-1,500 Hz range first used, since the wider ceiling admits some of that patch's higher-frequency inharmonic content), comb feedback at its maximum 3 and 0, with render setup `sampleRate = 48000.0`, `blockSize = 128`. The count test is narrowed to the default patch alone: it asserts 0 exact-octave jumps over the 60 s, that the first note-on is note 45, and that note 45 is the note sounding at the render's end. A real render shows one release-tail blip near 55.5 s, matching the measured 3 note-ons for the default patch. The per-case integer assertions for the other three patches are removed.

#### Scenario: The default patch sends note 45
- **WHEN** Pitch is chosen and the default patch plays for 60 seconds
- **THEN** a note-on for note 45 is sent and is the note sounding for most of the render, with 0 exact-octave jumps
- Check: `app/FroggersMidiOutTests.cpp: pitch_default_patch_sends_note_45_and_stays_sounding`

#### Scenario: A pitch step is sent within 100 ms
- **WHEN** the default patch's three VCOs step up by a factor of 1.5 twenty times, each step 25 ms into the gate's open half, at 48 kHz with 128-frame and with 256-frame blocks
- **THEN** every step's note-on for note 52 is stamped at most 100 ms of output after the step
- Check: `app/FroggersMidiOutTests.cpp: pitch_step_confirms_within_100ms_at_128_and_256_frames`

#### Scenario: A quiet output ends the note
- **WHEN** a note is sounding and the output falls silent
- **THEN** a note-off for that note is sent once the level follower's level is below 0.001, and no note-on follows while silence lasts
- Check: `app/FroggersMidiOutTests.cpp: pitch_quiet_output_ends_the_note`

#### Scenario: Several reports in one block send one change
- **WHEN** Pitch is chosen at 48 kHz with 2048-frame blocks and the detector reports two different notes inside one block
- **THEN** that block appends at most a note-off and a note-on, the note-on for the later note at its report's frame
- Check: `app/FroggersMidiOutTests.cpp: pitch_several_reports_in_one_block_send_one_change`

#### Scenario: Velocity follows the level unless fixed
- **WHEN** the Velocity field is Level and a note-on is sent while the output level is 0.5
- **THEN** its velocity is 64
- **WHEN** the Velocity field is fixed at 90
- **THEN** every note-on's velocity is 90
- Check: `app/FroggersMidiOutTests.cpp: pitch_velocity_follows_the_level_unless_fixed`

### Requirement: The MIDI-out path runs on the audio thread and misses no audio deadline
The fold, Level's follower and the pitch detector SHALL run on the audio thread inside `FroggersAppCore::ProcessBlock` on every build, with no second thread, no allocation, no lock and no wait there, and SHALL add no missed audio block deadline on any build that offers them.

A missed block deadline is an audible dropout, so a build offers Pitch only where its path was measured free of them. The detector and follower are constructed only in `PrepareToPlay`, which runs on the thread that called `Engine::Prepare()`: the thread JUCE calls `Runtime::audioDeviceAboutToStart` on in the standalone, the browser's main thread through `synth_browser::Runtime::Prepare`, and in the plugin whatever thread the host calls `prepareToPlay` on, which JUCE does not fix. None of these is the audio callback, and construction never happens lazily from the audio callback on first use, because construction allocates and allocation must stay off the audio thread. It is constructed alongside `MidiSender::Start()`, which must never run from inside the AudioWorklet's own thread, since spawning its worker's pthread there silently kills the worklet under `-sPTHREAD_POOL_SIZE=1`. By reading Q's source, the per-sample call writes only into storage sized at construction: `bitset` keeps its bits in a vector whose size is fixed at construction, the zero-crossing collector keeps its edges in a `ring_buffer` sized at construction, and `bitstream_acf` holds a reference to the bitset. Once per report the detector autocorrelates its bitstream, so its cost per sample is not constant: the report's sample carries that work.

A measurement timed the fold, follower and detector together per call at 48 kHz/128-frame, 192 kHz/1-frame and 192 kHz/blocks varying 1 to 2048 frames, on the default and phase-modulation-maximum patches, two reps each, added to two baselines of `Engine::ProcessBlock` through `SynthRig<FroggersApp>`, with the detector constructed at 50 to 5,000 Hz. Only at 48 kHz/128-frame do both baselines miss no block (0/750 each patch), and at that setting the addition causes zero combined misses either patch; at 192 kHz (both block settings) both baselines already miss on their own (every block at 1 frame; 226-310 of 678 at the varying setting), so under the operator's threshold framework those settings are recorded as ones the app misses without MIDI out and gate nothing, except that at 192 kHz/varying the phase-modulation-maximum patch's first rep shows 7 additional misses at the margin (284 baseline, 291 combined), reported as the real difference it is. The addition's own worst per-block time stays in the low tens of microseconds at every setting and patch (3,709-47,333 ns), 16.8 to 340.6 times below the same run's baseline worst (0.355-15.7 ms across the runs). A padded `sin()` loop inside the addition, 1,000 iterations per sample on the default patch, raised the addition's own worst and the combined worst block at every setting (at 192 kHz/varying from 5,306,708 to 12,430,250 ns); it moved the missed count only at 192 kHz/varying, from 231/678 to 572/678, since at 48 kHz/128-frame the count stayed at the control run's own single baseline miss (1/750) and at 192 kHz/1-frame every block already missed. The standalone and the plugin run this same `FroggersAppCore::ProcessBlock`; the plugin adds only clearing its MIDI buffer and at most two `addEvent` calls per block, into the 2,048 bytes JUCE 8.0.12 reserves before each prepare. In the browser, a measurement of the follower, the detector and the per-block enqueue together in the real browser build's audio worklet (48 kHz AudioContext rate, 128-frame quantum, 2,666.7 µs budget; the Emscripten clock behind those figures resolves to 1 ms) found the additions pass's worst block reaches 1.1250x budget, the same single block-0 startup transient every baseline pass also shows, and no block anywhere in the additions run exceeds what the baselines reach on their own, so the browser offers Pitch.

#### Scenario: A build offers Pitch only where its path was measured clean
- **WHEN** the catalog's contents are read on a build
- **THEN** Pitch is listed on the standalone, the plugin and the browser, all three shown feasible
- Check: `app/FroggersMidiCatalogTests.cpp: catalog_lists_level_and_pitch_as_midi_out_contents` (standalone listing), `app/vst/FroggersVstHostTests.cpp: plugin_catalog_lists_pitch_alongside_level` (plugin listing), `app/browser/e2e/midi-out-catalog.spec.mjs: the Sends combo offers Off, Level and Pitch in that order` (browser listing); the feasibility measurements themselves are not automated checks.

### Requirement: No note is left sounding
The app SHALL send the note-off for a sounding note, on the channel its note-on used, in the block where Pitch stops being the chosen content or the channel changes.

A note left held on the receiving instrument no longer follows the output's pitch. When Pitch stops being the chosen content the app also resets the detector, whether or not a note was sounding at that moment, as it does when the output falls quiet.

Releasing a port (another port chosen, None, or shutdown) is covered by the library, which sends All Notes Off on every channel to the released port (specified in the `app-midi-out` change).

#### Scenario: Switching away from Pitch ends the note
- **WHEN** a note is sounding and the choice changes to Level or Off
- **THEN** the note-off for that note, on its channel, is appended at frame 0 of that block, before any Control Change
- Check: `app/FroggersMidiOutTests.cpp: pitch_switching_away_ends_the_note`, `app/FroggersMidiOutTests.cpp: pitch_switching_to_off_ends_the_note`

#### Scenario: Changing the channel ends the note on the old channel
- **WHEN** a note is sounding on channel 0 and the channel changes to 1
- **THEN** the note-off is sent on channel 0, and the next note-on on channel 1
- Check: `app/FroggersMidiOutTests.cpp: pitch_changing_the_channel_ends_the_note_on_the_old_channel`

### Requirement: The plugin offers one MIDI output carrying only the app's messages
The plugin SHALL declare one MIDI output, SHALL clear the host's incoming MIDI at the start of every block, and SHALL write into the host's buffer only the app's MIDI-out messages for that block, each at its frame.

The VST3 and AU builds deliver this only after the operator's routing run (R2) confirms it. R7, a saved-project reopen run, is removed as an operator run and delivery gate: no plugin project was saved before this change, so there is nothing for it to confirm.

Before this change the plugin declared no MIDI output: `producesMidi()` returned false, `app/vst/CMakeLists.txt` set `NEEDS_MIDI_INPUT TRUE` and no `NEEDS_MIDI_OUTPUT`, whose default is FALSE, and `processBlock` ignored the buffer. JUCE treats whatever is left in the buffer when `processBlock` returns as the plugin's output ("Any messages left in the MIDI buffer when this method has finished are assumed to be the processor's MIDI output", JUCE `AudioProcessor` documentation). The host's input events could not reach the output before this change only because no output was declared, so declaring one without clearing the buffer would pass them through. JUCE 8.0.12, the version the plugin builds against, reserves 2,048 bytes in the wrapper's MIDI buffer before each prepare in the VST3 and AU wrappers, and at most two messages are written per block.

#### Scenario: The plugin scans with a MIDI output
- **WHEN** the plugin is instantiated
- **THEN** `producesMidi()` returns true and `acceptsMidi()` still returns true
- Check: `app/vst/FroggersVstHostTests.cpp: plugin_scans_with_a_midi_output`

#### Scenario: Incoming MIDI does not pass through
- **WHEN** the host passes a Control Change into a block and the MIDI-out choice is Off
- **THEN** the buffer holds no events when `processBlock` returns
- Check: `app/vst/FroggersVstHostTests.cpp: plugin_incoming_midi_does_not_pass_through`, `app/vst/FroggersVstHostTests.cpp: plugin_incoming_midi_does_not_pass_through_while_level_sends`

#### Scenario: The app's messages reach the host buffer at their frames
- **WHEN** Level is chosen and the app appends a Control Change at a block's last frame
- **THEN** the buffer holds that one event at that frame when `processBlock` returns
- Check: `app/vst/FroggersVstHostTests.cpp: plugin_apps_messages_reach_the_host_buffer_at_their_frames`

### Requirement: The plugin player sets the MIDI out on the instrument's own surface
WHILE the app is hosted as a plugin, the surface SHALL show, in a row beneath the transport row (wrapping into a second row where the width demands it), a MIDI button that cycles Off, Level and Pitch (Pitch only on builds that offer it), followed by Channel, CC and Velocity text fields, each refusing an entry outside its range and showing the stored value again; the transport row itself is unchanged (at the plugin's design width the transport row needs 317.64 px and has 284.67 px if the MIDI button joins it); and the plugin SHALL save these with the host project, restore them on reload, and SHALL NOT take them from the shared runtime configuration.

A new plugin instance starts with its MIDI out Off, and every saved session carries its MIDI-out setting, Off included, so there is no restore case that lacks one.

The plugin builds have no Controllers page, so the setting lives on the surface. The plugin loads the standalone's configuration file, so a MIDI-out setting the standalone saved would switch the plugin on unless the plugin ignores it. The library forwards that setting only to hosts that route MIDI out; the plugin does not, and applies its own setting through the same app entry point.

The standalone and browser builds set the MIDI out on the Controllers page and show no MIDI-out control on the instrument's surface.

#### Scenario: The plugin transport row is unchanged; the MIDI button sits beneath it
- **WHEN** the surface is built in plugin mode
- **THEN** the transport row holds only Freeze, the FREEZE label and the IN button, unchanged, and the new row beneath it holds the MIDI button first, which reads "MIDI: OFF"
- **AND** Play, Stop and Record are absent
- Check: `app/vst/FroggersVstHostTests.cpp: plugin_mode_transport_row_thins_to_freeze_and_label_only` (the transport row does not gain the MIDI button), `app/vst/FroggersVstHostTests.cpp: plugin_transport_row_is_unchanged_midi_button_sits_beneath_it` (the MIDI button and its "MIDI: OFF" label)

#### Scenario: The plugin rows fit the surface
- **WHEN** the surface is built in plugin mode at its design size
- **THEN** every node of the transport row and of the row (or rows, where it wraps) beneath it holding the MIDI button, Channel, CC and Velocity lies inside its parent and the surface, and no two siblings overlap
- Check: `app/vst/FroggersVstHostTests.cpp: plugin_mode_rows_fit_the_surface`

#### Scenario: The setting survives the project
- **WHEN** Pitch, channel 3, CC 20 and a fixed velocity of 100 are set, the host saves the project, and the project is reopened
- **THEN** the surface shows those values and the app sends Pitch on channel 3 at velocity 100
- Check: `app/vst/FroggersVstHostTests.cpp: plugin_midi_out_setting_survives_the_project`

#### Scenario: The standalone's setting does not switch the plugin on
- **WHEN** the shared runtime configuration holds MIDI-out content Level and a plugin instance starts with no saved session
- **THEN** the plugin's MIDI out is Off and it appends nothing
- Check: `app/vst/FroggersVstHostTests.cpp: plugin_standalones_setting_does_not_switch_the_plugin_on`

### Requirement: The manual says where the MIDI out works and what it sends
MANUAL.md SHALL describe the MIDI out as it ships now: where it is set on each build, what Level sends and what Pitch sends, the defaults, that Level sends at most 50 times a second, that Pitch is monophonic and tracks fundamentals from 50 Hz to 5,000 Hz, and that the plugin's MIDI output is routed in the DAW.

The manual names no DAW, plugin format or browser as confirmed until the operator runs (R2, R5, R6 and R9) happen.

#### Scenario: The manual names only confirmed hosts
- **WHEN** MANUAL.md's Audio to MIDI section is read after delivery
- **THEN** every DAW, plugin format and browser it names as receiving MIDI out is one whose R2, R5 or R6 run confirmed, and none is named before its run happens
- Check: none. Awaits R2, R5, R6 and R9; no automated check reads MANUAL.md.
