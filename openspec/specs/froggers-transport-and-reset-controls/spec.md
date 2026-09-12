# froggers-transport-and-reset-controls Specification

## Purpose
Transport (Play/Stop/Freeze) and Reset controls for the Froggers app: silencing guarantees on Stop, the Freeze latch's sustained-drone behavior, and reset semantics for parameters and depths.
## Requirements
### Requirement: Stop silences the instrument in bounded time, in every patch
Pressing Stop SHALL always silence the instrument: its output SHALL decay below audibility and every voice SHALL reach its idle state within a fixed bound measured in seconds, regardless of the parameter values, modulation depths, or modulation sources in effect — including any state reachable through Randomize All, and including while the Freeze latch is engaged, which Stop SHALL disarm as part of stopping. (⚠ AMENDED TWICE on 2026-08-17, both times by the operator exercising the built app. First version made the Freeze effect unreachable by any route. Second version reached it by making Stop-while-latched sustain instead of stop — which made a button labelled Stop conditionally mean "sustain", a trap. This version: the drone is reached by the FREEZE button alone, and Stop is unconditional. Operator ruling: Freeze stops the transport itself.) On the running-to-stopped edge, every non-idle voice SHALL enter Release immediately, bypassing the Grace minimum-hold and any in-progress Attack or Decay: Grace is a play-time musical guarantee and SHALL NOT delay transport stop (audit-added 2026-08-17, restoring the pre-Grace synchronous-force semantic that the Grace mechanism removed from Stop as a side effect). While the transport is stopped, the delay's feedback-drive, the reverb's tank-drive and the filter's comb-drive pre-gains SHALL resolve to unity, and the Freeze and reverb Grit parameters SHALL resolve to zero, without altering their commanded values, so that resuming play restores the patch bit-exactly. No stage inside a feedback loop SHALL be able to hold that loop's state up while the loop's input is silent: any in-loop stage whose local gain is unbounded resolves to its bypass value while stopped (measured 2026-08-17: reverb Grit re-amplifies a sub-audible tail into a bounded but non-decaying limit cycle). The sustained-drive character SHALL be reachable through the transport Freeze BUTTON ALONE, requiring no other control: engaging the latch SHALL itself stop the transport — reproducing the stopped-transport-plus-sustaining-audio state the effect only ever existed in — and SHALL suppress the entire stop-edge teardown (the forced release, the stopped-state effective-value overrides, and the stateful-unit clear), so the instrument holds its sounding state instead of silencing. Releasing the latch SHALL restore the teardown and silence the instrument within the same bound Stop guarantees; resuming audio then requires Play, since the transport is genuinely stopped. Parameter edits SHALL remain live while the latch is engaged: the drone responds to encoder changes exactly as the original accidental state did (operator ruling 2026-08-17, choosing faithful reproduction over a full state lock).

#### Scenario: Randomized patches stop like any other
- **WHEN** Randomize All has been applied any number of times and Stop is then pressed, with the Freeze latch in either state
- **THEN** the output peak falls below audibility within the bound
- **THEN** every voice reaches idle within the bound, so the stateful-unit clear actually fires

#### Scenario: A stage that cannot advance cannot hold a voice open
- **WHEN** the transport stops while any voice is in any non-idle stage, at any Curve and Grace setting
- **THEN** that voice enters Release on the stop edge, without waiting for its current stage or the Grace minimum-hold
- **THEN** that voice reaches idle within the bound
- **THEN** no envelope configuration exists whose ramp fails to complete within a small multiple of its knob time

#### Scenario: Stopping does not edit the patch
- **WHEN** the transport is stopped and later restarted
- **THEN** every commanded parameter value is bit-identical to its pre-stop value
- **THEN** the stop-time overrides were applied to the resolved values only

#### Scenario: The Freeze button alone reaches the sustained drone
- **WHEN** the transport is running and the Freeze button is pressed
- **THEN** the transport stops as part of engaging the latch
- **THEN** the instrument SUSTAINS its sounding state — the drone the accidental stop-state used to produce — instead of silencing
- **THEN** encoder edits continue to reach the audio, reshaping the sustained drone
- **WHEN** the Freeze button is pressed a second time
- **THEN** the instrument silences within the same bound Stop guarantees, and resuming audio requires Play

#### Scenario: Stop always means stop
- **WHEN** the Freeze latch is engaged and sustaining the drone, and Stop is pressed
- **THEN** the latch is disarmed
- **THEN** the instrument silences within the bound, exactly as any other Stop
- **THEN** no sequence of Freeze and Stop presses leaves the instrument sounding after Stop

### Requirement: Reset restores the default patch
THE Reset controls SHALL revert to the instrument's fresh-launch
default patch — the values a first launch presents, which are 0 for
most parameters but not all — never to a flat all-zeros state no
launch ever shows. Reset All SHALL be global: every bank's page
parameters, every parameter's modulation depths, every bank's local
Crispy, and the shared global Crunchy all revert to their defaults.
Reset Page SHALL revert the currently shown bank's slice of that same
default patch, including that bank's Crispy, and SHALL NOT touch other
banks. From a drilled-in modulation grid, reset SHALL revert the
selected parameter's modulation depths to their default-patch values.
The default patch SHALL have a single definition shared by launch,
reset, and New, so the three can never drift apart.

Equality with a fresh launch SHALL be evaluated over which parameters and
modulation depths EXIST as well as the values they carry. A depth parameter that
was materialized by an operation and left at a neutral value is not equal to one
that was never materialized.

Equality SHALL further be observable in the instrument's AUDIO OUTPUT, not only
in its stored values. A reset that leaves every stored value correct while the
instrument goes on sounding differently has not restored the default patch. This
clause exists because parameter-level equality and audible behaviour are not
interchangeable evidence: stored equality has held while the instrument
audibly did not decay.

#### Scenario: Reset All lands exactly on a fresh launch
- **WHEN** the operator has changed parameters, depths, Crispy, and
  Crunchy — including via Randomize All — and presses Reset All
- **THEN** the instrument's entire state equals a fresh launch's
  default patch, field for field, in both scene poles
- **THEN** the set of materialized modulation depth parameters equals a fresh
  launch's set, with no extra depths left over from the operations before it
- **THEN** Crispy on every bank and global Crunchy are at their
  default values

#### Scenario: Reset All restores the envelope's decay, not only its values

- **WHEN** the operator presses Randomize All, and then presses Reset All on a
  later block than the randomize landed on
- **THEN** the instrument decays to silence on the same schedule a fresh launch
  does, measured in the audible band rather than as a broadband level
- **THEN** this holds however many blocks separate the two presses, so that a
  reset arriving on the same block as the randomize is not the only case that
  works

#### Scenario: Reset Page restores that page's defaults, not zeros
- **WHEN** the Audio page's parameters have been edited and Reset Page
  is pressed while the Audio page is shown
- **THEN** the Audio bank's parameters return to their default-patch
  values — including the non-zero VCO shape defaults and the default
  cross-VCO pitch modulation depths — and the bank's Crispy returns to
  its default
- **THEN** every other bank's state is untouched

#### Scenario: One definition of the default patch
- **WHEN** the default patch is changed in a future edit
- **THEN** launch, reset, and New all present the changed defaults, because
  all three read the same single definition

### Requirement: New returns the instrument to its fresh-launch state

New SHALL leave the instrument in the same state a fresh launch presents,
including every modulation depth the launch state carries. New is reached
through the runtime's File page rather than the app's own surface, but it
restores the app's state and is governed here alongside Reset.

Restoring the launch state SHALL NOT be reconstructible from per-parameter
registration alone. A parameter's registered default is a single value on one
parameter and cannot express a modulation depth, which is a relationship between
a target parameter and a source slot. Any path claiming to restore the default
state SHALL therefore restore it from the state the application actually
established at startup, not from registration.

#### Scenario: New restores the cross-oscillator modulation depths

- **WHEN** the operator presses New
- **THEN** the cross-oscillator pitch modulation depths are present at the same
  values a fresh launch shows, not absent and not neutral
- **THEN** the three oscillators are not left in unison

#### Scenario: New, Reset All and launch agree

- **WHEN** the instrument's state is captured after a fresh launch, after New,
  and after Randomize All followed by Reset All
- **THEN** all three states are identical, both in which parameters and
  modulation depths exist and in the values they carry

#### Scenario: Drift in any one of the three paths is caught

- **WHEN** any one of launch, Reset All, or New is changed so that it no longer
  produces the same state as the other two
- **THEN** a check fails

Reset and New drift are caught by direct checks: the three-way detent
equality, and a sensitivity case that perturbs a Reset-restored value and
asserts the comparison reports it. Launch drift has no dedicated check and is
covered as near-vacuous: the wider suite asserts against the default patch
throughout, so a drifted launch fails broadly rather than through one named
check.

### Requirement: A stopped recording is offered for saving under today's date on every host that records
WHEN a recording stops with captured audio, THE app SHALL encode it as a WAV file, name it with the current local date in the form `YYYY-MM-DD` and a `.wav` extension, computed when the recording stops, and hand it to the runtime as a file export carrying the name, the media type, the bytes and, when the capture hit its length limit, a note saying so. A capture the length limit stops SHALL be handed over the same way without a stop press, and arming a new take SHALL hand over an unsaved one first. The app SHALL do this the same way on every host; how the file is saved is the host's: THE standalone SHALL open its save dialog on that name in the user's documents folder, keep its overwrite warning, and show the note in its completion message; THE browser build SHALL offer the file as a download under that name. A recording SHALL never be captured and then discarded for want of a host that saves it.

#### Scenario: The app queues one named export per stopped recording
- **WHEN** Play, Record and Record are dispatched with the transport running
- **THEN** exactly one file export is pending, named for today with `.wav`, whose bytes begin with the RIFF header and decode to the captured frame count
- **AND** taking it leaves none pending
- Check: `app/FroggersSurfaceTests.cpp: record_action_stop_with_data_queues_one_named_wav_export`

#### Scenario: A truncated capture carries its note and needs no stop press
- **WHEN** a capture stops at its length limit
- **THEN** the engine's next tick hands the host one export whose note says the recording stopped at the limit
- **AND** arming a new take before that tick hands it over first
- Check: `app/FroggersSurfaceTests.cpp: truncated_capture_queues_its_export_without_a_stop_press`, `arming_after_an_unpolled_truncated_capture_flushes_it_first`

#### Scenario: The standalone dialog opens on today's date
- **WHEN** the operator presses Record, Play, then Record on the standalone
- **THEN** the save dialog's file name is today's date followed by `.wav`
- Check: operator step with `date +%Y-%m-%d` (no automated check builds the dialog)

#### Scenario: The browser downloads the recording
- **WHEN** Play, Record and Record are clicked in the browser build
- **THEN** the page offers a download whose suggested name is today's date followed by `.wav` and whose body is a WAV file
- Check: `app/browser/e2e/recording.spec.mjs`, `a stopped recording downloads under today's date`

### Requirement: A refused Record says why, on screen, on every host
WHEN Record is pressed while the transport is stopped, THE app SHALL show the refusal ("Press Play before recording.") as a label beneath its own transport plates, on every host that shows Record, and SHALL clear it when a recording arms or Play is pressed. The app SHALL NOT depend on a host-registered callback or a modal dialog to show it.

#### Scenario: The notice appears and clears
- **WHEN** Record is dispatched with the transport stopped
- **THEN** the surface tree holds a transport notice reading "Press Play before recording." beneath the transport plates, inside the transport cell, with the plates where they were
- **WHEN** Play is dispatched
- **THEN** the notice is gone
- Check: `app/FroggersSurfaceTests.cpp: record_action_refused_while_stopped_shows_the_transport_notice`

#### Scenario: The browser shows it
- **WHEN** Record is clicked in the browser build with the transport stopped
- **THEN** the transport notice node reads "Press Play before recording."
- Check: `app/browser/e2e/recording.spec.mjs`, `Record with the transport stopped shows the notice`

