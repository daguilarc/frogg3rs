# Delta — `froggers-sheaf-runtime-app`

Seven controls reach the audio thread through single-slot atomics that
lose count and order inside one message tick. Sheaf's UI bus already orders
every other control. The requirements below put the seven on that bus and
state the rule that keeps them there.

## ADDED Requirements

### Requirement: A press travels on the engine's UI bus
The app SHALL deliver every press that changes audio-thread state (Randomize All, Randomize Page, Reset All, Reset Page, page select, page previous, page next, encoder press) as a `MessageIn::AppCommand` on `AppContext::uiBus`, applied on the audio thread by `FroggersAppCore::ApplyAppCommand` in the block that pops it, in the order pushed, by the same drain that applies encoder turns and scene blend; Start, Continue, Stop and Clock remain the engine's realtime messages, lifted out of both buses and applied after both have drained. The app SHALL NOT hold such a press in an atomic, a flag or a queue of its own, and SHALL NOT hold the bus behind one. A press whose target depends on the current page (page previous, page next) SHALL resolve that target on the audio thread from the audio thread's own current page, never from the page the surface last displayed. A press that originates on a MIDI controller SHALL join the bus at the surface's action handler, the same point a click does.

#### Scenario: Presses keep their count and order inside one message tick
- **WHEN** two Back presses are dispatched inside one message tick from drill level 2
- **THEN** the drill is at level 0 after the next block
- **WHEN** two Page Next presses are dispatched inside one message tick from page 1
- **THEN** page 3 is shown after the next block
- **WHEN** Reset All then Randomize All are dispatched inside one message tick
- **THEN** the state after the next block is randomized; reversed, it is the launch state
- Check: not yet delivered; the change's task 3.1 adds the case to FroggersModulationTests.cpp

#### Scenario: The plugin's page restore is a dispatched press
- **WHEN** a host restores a session that names a visible page
- **THEN** the page is selected through the surface's action handler and shows after one block

### Requirement: Every cross-thread member is a command, a value or a publication
A cross-thread input that crosses as its latest value (the host's routed-input signal; the Freeze latch; the Record arm; the desired transport state) SHALL be one atomic written by the message thread for another thread to read, and its declaration SHALL say it is a value and which thread reads it; a press whose effect the message thread computes and that crosses as the resulting value is such a value. State the audio thread publishes for the UI to read (the drill level shown in the header; whether the last randomize drew short; the recorded frame count and truncation flag) SHALL be one atomic written by the audio thread, declared a publication. The app SHALL NOT route a value or a publication through the press bus, and SHALL NOT route a command through an atomic. A member outside these classes (the Record writer handshake the audio thread raises around a block) SHALL say so at its declaration.

#### Scenario: The classification is visible at the declaration
- **WHEN** the app core's cross-thread members are read
- **THEN** each is declared a command (on the bus), a value (an atomic the message thread writes), a publication (an atomic the audio thread writes), or the one named exception, and no member is two of these

### Requirement: The BPM slider is the catalog's tempo action
The BPM slider SHALL push `MessageIn::SetTempoBpmNormalized` with its value placed in the catalog's tempo range (`kFroggersBpmMin` to `kFroggersBpmMax`), the same route a controller's mapped tempo control and a shifted Twister turn already take, and SHALL read its displayed tempo, its external-clock state and the transport's running state from what the engine publishes through `AppContext`. The app SHALL NOT mirror those values in atomics of its own.

#### Scenario: A slider drag reaches the master clock in one block
- **WHEN** the slider is dragged to 300 on the rig
- **THEN** the engine's clock diagnostics read 300 after one block
- Check: not yet delivered; the change's task 3.2 adds the case to FroggersSurfaceTests.cpp

#### Scenario: Slaved, the slider is a read-only line and pushes nothing
- **WHEN** receive-clock is requested and a BPM action is dispatched
- **THEN** the surface renders the read-only tempo line and pushes no tempo message
