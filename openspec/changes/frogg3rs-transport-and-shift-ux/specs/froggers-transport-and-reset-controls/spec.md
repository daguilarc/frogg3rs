# Delta — `froggers-transport-and-reset-controls`

In "Stop silences the instrument in bounded time, in every patch", the Freeze
release clause and the "pressed a second time" step of "The Freeze button
alone reaches the sustained drone" change, and two scenarios are added; every
other clause and scenario is carried forward word for word. The
RESTATES-EXCEPT block under the requirement names the promoted clause it
changes, and `app/check_modified_requirements_restate_promoted.py` checks it.

## MODIFIED Requirements

### Requirement: Stop silences the instrument in bounded time, in every patch
Pressing Stop SHALL always silence the instrument: its output SHALL decay below audibility and every voice SHALL reach its idle state within a fixed bound measured in seconds, regardless of the parameter values, modulation depths, or modulation sources in effect — including any state reachable through Randomize All, and including while the Freeze latch is engaged, which Stop SHALL disarm as part of stopping. (⚠ AMENDED TWICE on 2026-08-17, both times by the operator exercising the built app. First version made the Freeze effect unreachable by any route. Second version reached it by making Stop-while-latched sustain instead of stop — which made a button labelled Stop conditionally mean "sustain", a trap. This version: the drone is reached by the FREEZE button alone, and Stop is unconditional. Operator ruling: Freeze stops the transport itself.) On the running-to-stopped edge, every non-idle voice SHALL enter Release immediately, bypassing the Grace minimum-hold and any in-progress Attack or Decay: Grace is a play-time musical guarantee and SHALL NOT delay transport stop (audit-added 2026-08-17, restoring the pre-Grace synchronous-force semantic that the Grace mechanism removed from Stop as a side effect). While the transport is stopped, the delay's feedback-drive, the reverb's tank-drive and the filter's comb-drive pre-gains SHALL resolve to unity, and the Freeze and reverb Grit parameters SHALL resolve to zero, without altering their commanded values, so that resuming play restores the patch bit-exactly. No stage inside a feedback loop SHALL be able to hold that loop's state up while the loop's input is silent: any in-loop stage whose local gain is unbounded resolves to its bypass value while stopped (measured 2026-08-17: reverb Grit re-amplifies a sub-audible tail into a bounded but non-decaying limit cycle). The sustained-drive character SHALL be reachable through the transport Freeze BUTTON ALONE, requiring no other control: engaging the latch SHALL itself stop the transport — reproducing the stopped-transport-plus-sustaining-audio state the effect only ever existed in — and SHALL suppress the entire stop-edge teardown (the forced release, the stopped-state effective-value overrides, and the stateful-unit clear), so the instrument holds its sounding state instead of silencing. Releasing the latch with a second Freeze press SHALL return the transport to the state it was in when the latch engaged: if the transport was running, releasing SHALL start it exactly as pressing Play does; if it was stopped, releasing SHALL restore the teardown and silence the instrument within the same bound Stop guarantees, and the transport SHALL stay stopped. Parameter edits SHALL remain live while the latch is engaged: the drone responds to encoder changes exactly as the original accidental state did (operator ruling 2026-08-17, choosing faithful reproduction over a full state lock).

<!-- RESTATES-EXCEPT
resuming audio requires Play
  keeps: silences within the same bound Stop guarantees
-->

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
- **THEN** the latch is disarmed and the transport runs again, exactly as if Play had been pressed
- Check: `app/FroggersAudioRoutingTests.cpp: releasing_freeze_resumes_the_transport_it_stopped`

#### Scenario: Stop always means stop
- **WHEN** the Freeze latch is engaged and sustaining the drone, and Stop is pressed
- **THEN** the latch is disarmed
- **THEN** the instrument silences within the bound, exactly as any other Stop
- **THEN** no sequence of Freeze and Stop presses leaves the instrument sounding after Stop

#### Scenario: Releasing a Freeze engaged while stopped silences and stays stopped
- **WHEN** the transport is stopped, the Freeze button is pressed and the drone it holds is audible, and the Freeze button is pressed a second time
- **THEN** the instrument silences within the same bound Stop guarantees
- **THEN** the transport is still stopped
- Check: `app/FroggersAudioRoutingTests.cpp: freeze_engaged_while_stopped_releases_to_silence_within_the_bound`

#### Scenario: Play and Stop while frozen still decide the transport
- **WHEN** the transport is running, the Freeze button is pressed, and then Play is pressed
- **THEN** the latch is disarmed and the transport runs
- **WHEN** the transport is running, the Freeze button is pressed, and then Stop is pressed
- **THEN** the latch is disarmed and the instrument silences within the bound, with the transport stopped
- Check: `app/FroggersAudioRoutingTests.cpp: play_disarms_the_freeze_latch_and_returns_the_voice_gate_to_the_transport`, `no_freeze_stop_press_sequence_leaves_the_instrument_sounding_after_stop`

## ADDED Requirements

### Requirement: The Play plate shows whether the transport is running
The Play plate SHALL show a held state whenever the transport is running and SHALL show its idle state whenever the transport is stopped, on every host that shows Play. The held state SHALL swap the plate and glyph colours, the same way the Freeze plate shows its latch and the Record plate shows that it is armed. The state SHALL be read from the transport itself on every rebuild, so it follows every route that starts or stops the transport: the Play, Stop and Freeze buttons, their MIDI mappings, a MIDI Start or Stop message, and the transport restart after an audio-device change.

#### Scenario: Play is held while the transport runs
- **WHEN** Play is pressed and the transport is running
- **THEN** the Play plate's draw commands swap its plate and glyph colours
- Check: `app/FroggersSurfaceTests.cpp: play_plate_is_held_while_the_transport_runs`

#### Scenario: Stop and Freeze release the Play plate
- **WHEN** the transport is running and Stop is pressed
- **THEN** the Play plate shows its idle colours
- **WHEN** the transport is running and Freeze is pressed
- **THEN** the Play plate shows its idle colours while the drone holds
- **WHEN** Freeze is then pressed a second time
- **THEN** the Play plate shows its held colours again
- Check: `app/FroggersSurfaceTests.cpp: play_plate_is_held_while_the_transport_runs`

#### Scenario: The held colours are a real exchange
- **WHEN** the Play plate's draw commands are built held and idle
- **THEN** the held plate colour is the idle glyph colour and the held glyph colour is the idle plate colour
- Check: `app/FroggersSurfaceTests.cpp: play_draw_commands_swap_plate_and_glyph_colours_while_running`
