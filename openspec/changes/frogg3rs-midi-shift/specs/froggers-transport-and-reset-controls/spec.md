# Delta — `froggers-transport-and-reset-controls`

## ADDED Requirements

### Requirement: A stopped recording is offered for saving under today's date on every host that records
WHEN a recording stops with captured audio, THE app SHALL encode it as a WAV file, name it with the current local date in the form `YYYY-MM-DD` and a `.wav` extension, computed when the recording stops, and hand it to the runtime as a file export carrying the name, the media type, the bytes and, when the capture hit its length limit, a note saying so. A capture the length limit stops SHALL be handed over the same way without a stop press, and arming a new take SHALL hand over an unsaved one first. The app SHALL do this the same way on every host; how the file is saved is the host's: THE standalone SHALL open its save dialog on that name in the user's documents folder, keep its overwrite warning, and show the note in its completion message; THE browser build SHALL offer the file as a download under that name. A recording SHALL never be captured and then discarded for want of a host that saves it.

#### Scenario: The app queues one named export per stopped recording
- **WHEN** Play, Record and Record are dispatched with the transport running
- **THEN** exactly one file export is pending, named for today with `.wav`, whose bytes begin with the RIFF header and decode to the captured frame count
- **AND** taking it leaves none pending
- Check: `FroggersSurfaceTests.cpp: record_action_stop_with_data_queues_one_named_wav_export`

#### Scenario: A truncated capture carries its note and needs no stop press
- **WHEN** a capture stops at its length limit
- **THEN** the engine's next tick hands the host one export whose note says the recording stopped at the limit
- **AND** arming a new take before that tick hands it over first
- Check: `FroggersSurfaceTests.cpp: truncated_capture_queues_its_export_without_a_stop_press`, `arming_after_an_unpolled_truncated_capture_flushes_it_first`

#### Scenario: The standalone dialog opens on today's date
- **WHEN** the operator presses Record, Play, then Record on the standalone
- **THEN** the save dialog's file name is today's date followed by `.wav`
- Check: operator step with `date +%Y-%m-%d` (no automated check builds the dialog)

#### Scenario: The browser downloads the recording
- **WHEN** Play, Record and Record are clicked in the browser build
- **THEN** the page offers a download whose suggested name is today's date followed by `.wav` and whose body is a WAV file
- Check: `app/browser/e2e/recording.spec.mjs`

### Requirement: A refused Record says why, on screen, on every host
WHEN Record is pressed while the transport is stopped, THE app SHALL show the refusal ("Press Play before recording.") as a label beneath its own transport plates, on every host that shows Record, and SHALL clear it when a recording arms or Play is pressed. The app SHALL NOT depend on a host-registered callback or a modal dialog to show it.

#### Scenario: The notice appears and clears
- **WHEN** Record is dispatched with the transport stopped
- **THEN** the surface tree holds a transport notice reading "Press Play before recording." beneath the transport plates, inside the transport cell, with the plates where they were
- **WHEN** Play is dispatched
- **THEN** the notice is gone
- Check: `FroggersSurfaceTests.cpp: record_action_refused_while_stopped_shows_the_transport_notice`

#### Scenario: The browser shows it
- **WHEN** Record is clicked in the browser build with the transport stopped
- **THEN** the transport notice node reads "Press Play before recording."
- Check: `app/browser/e2e/recording.spec.mjs`
