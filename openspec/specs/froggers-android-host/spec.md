# froggers-android-host Specification

## Purpose
TBD - created by archiving change frogg3rs-android-app. Update Purpose after archive.
## Requirements
### Requirement: The Android app is Frogg3rs in Sheaf's runtime shell
The Android app SHALL be built from `app/FroggersMain.cpp` and the same Sheaf
runtime shell the desktop standalone presents, SHALL open directly into
Frogg3rs with no app picker, and SHALL carry the package name
io.github.daguilarc.frogg3rs.

#### Scenario: Opening the app opens Frogg3rs
- **WHEN** the installed app is launched
- **THEN** Frogg3rs opens with its audio device running and the runtime sidebar present
- Check: operator step — pass, frogg3rs-android-app task 4.7: on the frogg3rs-api35 emulator (debug APK) the session log, read via run-as, recorded "Audio prepared: 48000 Hz, 4800 frames, 0 in / 2 out", and dumpsys activity services showed the foreground service running (isForeground=true).

#### Scenario: The package is the registered one
- **WHEN** the built APK's badging is dumped
- **THEN** its package is io.github.daguilarc.frogg3rs
- Check: operator step — pass, frogg3rs-android-app task 4.6: aapt2 badging on both the debug and release APKs showed the registered package, and app/android/build-android.sh's own package-mismatch assertion printed "confirmed" on both builds.

### Requirement: Audio, MIDI and ring output keep running with the screen off
While the app is open, it SHALL run a foreground service of type
`mediaPlayback` that holds a partial wake lock, started from the visible
activity and stopped when the activity is destroyed, so that audio output,
MIDI input, the message-thread timer and MIDI output continue while the screen
is off and the device is idle. Closing the app SHALL stop the service.

#### Scenario: A Twister turn is answered with the screen off
- **WHEN** the screen is off and the device is forced idle for two minutes
- **AND** a control change reaches the app every ten seconds on the ports a MIDI Fighter Twister row is bound to
- **THEN** each one is answered by that encoder's ring value on the row's output within 250 ms
- **AND** the app's audio output advances by the device sample rate per second of wall time, within 0.5%
- Check: operator step — pass in both cases (Frogg3rs in front when the screen goes off; Home pressed first), frogg3rs-android-app task 4.8 post the F4 fix: turns 1-11, ten seconds apart, were each answered within 55-230 ms by a ring message whose value changed, and audio output advanced within 0.0053%-0.0144% of the sample rate over the two-minute window (measured from the audio HAL's own write-count dump). The task's own control (stopping the service to show an unanswered turn) is not yet delivered: not measured on the emulator, since the service could not be detached without destroying the activity/task, so the answered-vs-unanswered contrast was not observed either way.

#### Scenario: Closing the app stops the service
- **WHEN** the app is removed from recent apps
- **THEN** no foreground service of the package remains
- Check: operator step — pass, frogg3rs-android-app task 4.8: after swiping the app away from recent apps, a process listing showed the app's process gone entirely and a foreground-service dump listed no entry for the package.

### Requirement: A Twister on Android is offered its preset
The MIDI Fighter Twister preset SHALL be offered and SHALL pair both ports on
Android from its existing desktop alias alone, and a device presenting
unnamed ports under an unrelated name SHALL NOT be offered it. Android/JUCE
report a USB Twister's ports as "DJ TechTools Midi Fighter Twister Output
Port 1" (the app's input from it) and "DJ TechTools Midi Fighter Twister
Input Port 1" (the app's output to it): the Twister firmware's USB strings
(manufacturer "DJ TechTools", product "Midi Fighter Twister", no jack
names), Android's name for a MIDI 1.0 USB device ("<manufacturer>
<product>", empty port names) and JUCE's name for an Android port ("<device
name> <port name>", "Output Port N" or "Input Port N" for an empty one).
Sheaf's `MatchesAnyAlias` accepts a reported name that ends with one of a
preset's aliases once a trailing " Output Port N"/" Input Port N" is
stripped (`phone-width-composition-and-record-permission`), so no
Twister-specific Android alias is carried in the catalog.

#### Scenario: The page pairs the Android names with the Twister preset
- **WHEN** a unit whose ports are named "DJ TechTools Midi Fighter Twister Output Port 1" and "DJ TechTools Midi Fighter Twister Input Port 1" is connected
- **THEN** the Controllers page offers the MIDI Fighter Twister preset for it and pairs both ports
- Check: `twister_preset_pairs_with_the_android_port_names` in `app/FroggersControllersPageTests.cpp` — pass (confirmed via the full `make -C app test` run recorded when F1/F2/F3 were committed, and unmodified by F5's later generalization of the matching rule).

#### Scenario: An unrelated device with unnamed ports is not offered the preset
- **WHEN** a unit whose ports are named "Other Maker Other Thing Output Port 1" and "Other Maker Other Thing Input Port 1" is connected
- **THEN** the Controllers page does not offer the MIDI Fighter Twister preset for it
- Check: `a_second_unnamed_port_device_is_not_offered_the_twister_preset` in `app/FroggersControllersPageTests.cpp` — pass, added by task F3 and reconfirmed green after F5's rule change (`make -C app test`).

#### Scenario: JUCE on Android reports those names
- **WHEN** a real USB Twister connects, presenting the device-level name "DJ TechTools Midi Fighter Twister" and no port names of its own
- **THEN** the app offers the MIDI Fighter Twister preset for it and binds both ports without a port chosen by hand
- Check: operator step — pass, frogg3rs-android-app tasks 4.8/F3, confirmed on the emulator against the virtual test device (its own ports named directly with the full strings above, since a MidiDeviceService-declared device does not receive Android's manufacturer-plus-product device-name concatenation the way a real USB device does): the Controllers page's preset dropdown showed "MIDI Fighter Twister" with no port chosen by hand, and adding it bound the row to the two Android port names above, both green/online. The real-USB-device case (no test-device workaround needed) remains unconfirmed by a connected unit.

### Requirement: The Android app lays out for a phone screen
The Android app SHALL fill the display inside its safe-area insets, SHALL
report its width to the surface on every resize, and SHALL show the surface's
narrow arrangement at 720 px or less, fitted to the window width and scrolled
vertically. A drag that starts on a control SHALL move that control and SHALL
NOT scroll.

#### Scenario: Portrait on a phone shows the narrow arrangement
- **WHEN** the app runs on a 412 by 915 dp portrait display
- **THEN** the chrome block sits above the encoder grid, both span the window width, and the runtime sidebar sits inside the chrome block
- **AND** no control lies under a system bar
- Check: operator step — pass, frogg3rs-android-app tasks 4.7 and F1: uiautomator's dump showed Randomize/Reset and the sidebar buttons in a column above the encoder grid (no overlap, about 36px margins), and after F1's fix a window-state dump showed the status/navigation bars hidden with no control drawn in that region.

#### Scenario: Dragging an encoder turns it
- **WHEN** a touch drag starts on an encoder
- **THEN** that encoder's value changes and the scroll position does not
- **AND** a touch drag that starts between controls scrolls
- Check: operator step — pass, frogg3rs-android-app task 4.7: a swipe starting exactly on the VCO1 knob changed only that knob's own pixel region (a 120x220 crop diff of 161539, chrome block's top row byte-identical before/after), and a swipe starting in the gap between knob columns moved the whole page as one scrolling viewport.

### Requirement: The Android app asks for the microphone when an input is chosen
The Android app SHALL request record permission before it opens an audio
input, SHALL keep output running and say so on the Audio page when permission
is refused, and SHALL add the `microphone` type to its foreground service once
permission is granted, so that the chosen input keeps working with the screen
off.

#### Scenario: Refused permission keeps the sound
- **WHEN** an input is chosen and record permission is refused
- **THEN** output keeps running and the Audio page says microphone access was not granted
- Check: operator step — pass, frogg3rs-android-app task 4.9, exercised through the persisted-input startup path (Runtime::Start, sar-43's other named target) since the on-screen input dropdown could not be operated by touch automation on this emulator: with record permission denied, the real "Allow Frogg3rs to record audio?" dialog appeared, was declined, the session log recorded "Audio prepared: 48000 Hz, 4800 frames, 0 in / 2 out", and the Audio I/O page read "Input requested 1 / active 0 - Microphone access was not granted." exactly.

#### Scenario: Granted permission opens the input
- **WHEN** an input is chosen and record permission is granted
- **THEN** the input opens and the foreground service carries the `microphone` type
- Check: operator step — pass, frogg3rs-android-app task 4.9, exercised through the same persisted-input path: with record permission granted and a relaunch, the session log recorded "Audio prepared: 48000 Hz, 4800 frames, 1 in / 2 out" and a services dump showed the foreground service carrying both the mediaPlayback and microphone types.

### Requirement: Record on Android saves where the user chooses
When a recording finishes, the Android app SHALL write it to the document the
user picks in Android's save screen, which JUCE returns as a content URL, and
SHALL show the same saved or failed message the desktop shows. Cancelling the
save screen SHALL save nothing and show nothing.

#### Scenario: A finished recording lands in the chosen document
- **WHEN** a recording is finished and saved into Documents with the offered name
- **THEN** a WAV file with that name exists there, starts with RIFF and is larger than its header
- **AND** the app says the recording was saved
- Check: operator step — pass, frogg3rs-android-app tasks 4.13/F2: after F2's URL-scheme fix, Play/Record/wait 6 s/Record/save into Documents keeping the offered name showed "Recording saved.", the file was 295724 bytes, and a hex dump confirmed it starts with the bytes RIFF. Reproduced red with F2's fix reverted: "Failed to write recording." and a 0-byte file.

### Requirement: The Android app shows only the app surface
The Android app's window SHALL show only the app surface, as the site does, with no menu bar, SHALL hide the status and navigation bars while it is in front, and every control it draws SHALL lie outside the display cutout.

#### Scenario: No menu bar and nothing under the system bars
- **WHEN** the app is open in portrait
- **THEN** no menu bar is drawn, the status and navigation bars are hidden, and no control lies in the display cutout
- **AND** a modulation view's Back target can be tapped
- Check: operator step — pass, frogg3rs-android-app task F1: a window-state dump showed the bars set to hide on swipe with a "Requested non-default-visibility types: statusBars navigationBars captionBar systemOverlays" line, no menu bar was drawn (screenshot), and tapping a modulation view's Back target (the already-selected page tab) returned to the page grid. An edge swipe revealed the bars transiently and they hid again on their own a few seconds later. Reproduced red with the fix reverted: bars shown by default, and the same Back tap left the modulation view open.

### Requirement: The Android app is released as an APK signed with the registered key
The Android APK SHALL be published on its own release, on the tag
frogg3rs_android, signed with the release key the package is registered with,
and verified before it is published. The release key SHALL come from the
GitHub Actions secrets FROGG3RS_RELEASE_KEYSTORE_B64 (the keystore, base64)
and FROGG3RS_RELEASE_KEYSTORE_PASSWORD (its password). No signing secret SHALL
be committed.
Every other build SHALL run the same signing and verification steps with a
key generated for that run. A push to `main` that touches only `openspec/` or
the repository README SHALL NOT start the Android build.

#### Scenario: The tag publishes a verified APK
- **WHEN** the frogg3rs_android tag is pushed
- **THEN** the release carries `Frogg3rs-android.apk` with exactly one signer, whose certificate is the release key's
- **AND** a missing signing secret fails the job instead of publishing
- Check: operator step — the frogg3rs_android tag's release job publishes Frogg3rs-android.apk, and `apksigner verify --print-certs` on the downloaded asset shows one signer whose SHA-256 is neither a generated CI key nor the Android debug key; frogg3rs main moves to the tagged commit only after that passes.

#### Scenario: Every code push signs and verifies
- **WHEN** a code push to `main` builds the APK
- **THEN** the signing and verification steps run with a key generated in the job, and the release is untouched
- Check: operator step — pass, frogg3rs-android-app task 4.11, confirmed on three separate pushes to branch android-app (CI runs 36520742614, 36533240387 and 36545759280): each build job signed with a key generated in the job and passed verification, and the release job reported "skipped" (not the release tag).

