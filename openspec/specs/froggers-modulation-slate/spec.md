# froggers-modulation-slate Specification

## Purpose
The fixed fifteen-source modulation slate plus Target/Back cell, app-owned registration (not Sheaf's fixed-count standard-modulator aggregate), a two-level drill-in cap, external-audio sources that stay present-but-inert when unavailable, and the two context-sensitive randomize affordances (Randomize All, Randomize Page).
## Requirements
### Requirement: Fifteen modulation sources plus a Target/Back cell
The app SHALL expose exactly fifteen modulation sources, filling the framework's fifteen modulator slots, with a sixteenth grid cell reserved for Target/Back. The slate SHALL be, **in this order**:

| Slot | Source |
|---|---|
| 1st–6th | Random S&H 1 through Random S&H 6 |
| 7th–9th | VCO 1, VCO 2, VCO 3 audio output |
| 10th–12th | VCO 1, VCO 2, VCO 3 envelope follower |
| 13th | Noise |
| 14th–15th | External audio — audio rate, then envelope follower |
| 16th cell | Target / Back |

The two external-audio sources SHALL be last before Target/Back, because they are the only sources that can become unavailable.

#### Scenario: Slate is complete and ordered
- **WHEN** the modulation detail grid is opened for any parameter
- **THEN** fifteen modulation source cells are present in exactly the order above
- **THEN** the sixteenth cell is Target/Back

### Requirement: The app owns modulator registration
The app SHALL register all fifteen modulation sources itself rather than delegating to the framework's standard-modulator aggregate, whose random count is fixed and cannot supply six sample-and-hold sources. Reusable framework pieces — the noise processor and the standard visualizers — SHALL still be used directly rather than reimplemented.

#### Scenario: No standard-modulator aggregate
- **WHEN** the app's modulator registration is inspected
- **THEN** the framework's standard-modulator aggregate is not used
- **THEN** its fixed-rate random walks and constant source do not appear in the slate

#### Scenario: Framework visualizers are still reused
- **WHEN** the noise and smooth-random sources are rendered
- **THEN** they use the framework's own noise and ganged-random visualizers
- **THEN** no equivalent is reimplemented in the app

### Requirement: Depth cells inherit source color and visualizer
Each modulation depth cell SHALL render in its modulation source's color and SHALL display that source's visualizer as an underlay, without per-cell app code.

#### Scenario: Stepped Random S&H cells show their loop visualizer
- **WHEN** the detail grid is open
- **THEN** each stepped Random S&H source's depth cell shows that source's remembered-loop visualizer
- **THEN** the depth cell's ring uses that source's color

#### Scenario: The smooth source keeps the framework visualizer
- **WHEN** the detail grid is open
- **THEN** the smooth Random S&H source's depth cell shows the framework's ganged-random visualizer
- **THEN** it is visually distinguishable from the stepped sources

### Requirement: Modulation drill-in is capped at two levels
The app SHALL permit drilling from a top-level parameter into its modulation depth grid, and from a depth cell into that depth parameter's own modulation — and SHALL refuse any deeper drill-in. The cap SHALL be enforced by the app, because the underlying framework provides no depth limit and no level stack.

#### Scenario: Second level is permitted
- **WHEN** the operator drills into a modulation depth cell from the detail grid
- **THEN** that depth parameter's own modulation view opens

#### Scenario: Third level is refused
- **WHEN** the operator attempts to drill in from a depth cell at the second level
- **THEN** no further modulation view opens
- **THEN** the current level remains the active editing context

#### Scenario: Target/Back exits to the parameter grid
- **WHEN** the operator activates Target/Back from any modulation level
- **THEN** the bank's parameter grid is restored
- **THEN** no intermediate modulation level is re-entered

Note: a one-level pop is deliberately **not** provided. The framework's deselect returns to the parameter grid, and the call that would re-open an intermediate level is not part of its public surface, so synthesizing a pop would mean working around a private API. Full exit from any level is the accepted behavior (design D5).

### Requirement: External-audio sources stay present but inert when unavailable
The two external-audio modulation sources SHALL connect only on the operator's
affirmative act, and declining SHALL be a real and default choice.

When no external audio input is routed, the two external-audio modulation
sources SHALL be marked **not connected**. They SHALL remain present in the
slate, SHALL be inert, SHALL render as disconnected, and SHALL NOT be
randomized. They SHALL NOT be hidden, and the slate SHALL NOT change size.

**Availability is defined by the host's affirmative routed signal, and a
host-opened device is not enough.** The app SHALL request one audio input
channel and SHALL derive the sources' connected state exclusively from the
host's routed signal — which reports routed only when the operator
affirmatively selected an input (device selection in the standalone and
browser; explicit bus routing in a DAW) — never from a channel or device
merely existing. A platform-default device the host opened unasked SHALL
derive not-routed and SHALL NOT connect the sources. The connected state SHALL
be written once per routing transition, from the host's change notification,
never recomputed per sample from channel presence.

**Requesting an input channel SHALL NOT open an input device.** A host SHALL
NOT open a capture device on the strength of an application's requested
channel count. It SHALL open one only when the operator has selected a device,
and SHALL then open the channels the application requested.

**Declining input SHALL be a real choice, and SHALL be the default.** Every
host that presents an input selection SHALL offer an explicit no-input option,
SHALL start on it, and SHALL treat any other selection as the operator's
affirmative act. A host SHALL NOT present "whatever the system provides" as an
input choice at all: an unnamed device that resolves to a real microphone is
indistinguishable from a choice nobody made. Output device selection is
unaffected and keeps its system-default entry.

**A persisted input selection SHALL NOT survive a change in what it means.**
When a host reads stored state written before declining became expressible, it
SHALL treat any input device name it finds as unset and start declined, because
a name recorded when there was no way to say no is not evidence that anyone
said yes. Stored output device selections are unaffected.

**A disconnected source SHALL render as a disabled control, not as nothing.**
While not connected, the two external-audio cells SHALL hold their grid
positions and SHALL draw a visibly de-emphasised encoder — carrying no value
readout and no modulation or gesture indicators, so it reads as present and
unavailable rather than as adjustable.

The de-emphasis SHALL be an explicit neutral colour rather than a dimming of
the source's own. A disconnected source publishes no colour, so there is
nothing to dim: scaling its published value leaves the cell drawn exactly as a
connected one would be. The disabled cell is drained of hue while a connected
cell carries its source's, which is what distinguishes them on screen.

A disabled cell draws no value arc, and that is not an omission: the arc is a
per-voice layer and a disconnected source publishes no voices. It SHALL NOT be
given a synthesised voice to draw one, which would report a value it does not
have.

They SHALL carry no press or drag action and no depth parameter while
disconnected, so there is no edit to accept. Visibility is a rendering choice
and SHALL NOT make a disconnected source reachable: nothing about the disabled
rendering may create an action, a parameter, or a randomization candidate.

The rendering SHALL be the surface's own choice rather than a change to the
shared encoder drawing, so that no other application's disconnected cells
change appearance.

#### Scenario: Disconnection is the inert state, not a removal
- **WHEN** no input is routed
- **THEN** both sources are marked not connected
- **THEN** their grid cells are still present, carrying no depth parameter
- **THEN** those cells carry no press or drag action

#### Scenario: A disconnected cell is drawn, and drawn as unavailable
- **WHEN** the modulation view shows a source that is not connected
- **THEN** that cell emits draw commands rather than none
- **AND** it is visibly de-emphasised against a connected cell in the same view,
  drawn in a neutral colour where the connected cell carries its source's
- **AND** it shows no value readout

#### Scenario: Drawing it does not make it reachable
- **WHEN** a disconnected cell is pressed or dragged
- **THEN** no action is dispatched and no depth parameter is created

#### Scenario: A host-opened default device does not count as routed
- **WHEN** the host has opened a platform-default input device without any operator selection
- **THEN** the routed signal reports not routed
- **THEN** both external-audio sources stay disconnected and contribute no modulation

#### Scenario: Requesting a channel opens no device
- **WHEN** an application requesting one input channel starts
- **THEN** the host has no input device open
- **THEN** the input diagnostic reports the requested count with zero active

#### Scenario: An upgraded install starts declined
- **WHEN** a host loads stored state written before no-input was a choice
- **AND** that state names an input device
- **THEN** the input selection reads as no input and no device is opened
- **THEN** the stored output device selection is still honored

#### Scenario: The operator can decline input, and starts declined
- **WHEN** a host that selects input devices is opened for the first time
- **THEN** the input selection reads as no input
- **THEN** both external-audio sources are disconnected
- **WHEN** the operator selects an actual device
- **THEN** that device is opened with the requested input channels
- **THEN** the routed signal reports routed
- **WHEN** the operator selects no input again
- **THEN** the input device is closed and the routed signal reports not routed

#### Scenario: Routing connects the sources in place
- **WHEN** the operator affirmatively routes an input and the host's routed signal reports routed
- **THEN** both sources are marked connected
- **THEN** their depth parameters materialize on next use, in the same cell positions

### Requirement: Randomization uses the framework's authority
The framework's own randomization entry point SHALL remain the sole mutator of randomized modulation-depth values. The app SHALL NOT implement a parallel randomization mutator, and SHALL NOT introduce a per-source randomizability flag beside the connection state the framework already consults. The app SHALL, however, choose the target set passed to a randomization call and the aggregate reach of that call.

#### Scenario: One randomization authority
- **WHEN** the app is inspected for randomization logic
- **THEN** every randomized modulation-depth value is written by the framework's mutator
- **THEN** the app's role is limited to selecting which targets and how much reach are passed into that call, never writing a randomized value itself

### Requirement: Two randomize affordances
The app SHALL provide exactly two randomize affordances: **Randomize All** (global) and **Randomize Page** (per-page). Randomize All, pressed while a parameter page is active, SHALL randomize every page parameter value in every bank plus all first-level modulation depths, SHALL randomize the local Crispy control of at most two banks — chosen as distinct banks, with a count that is zero on about half of presses — SHALL leave the global Crunchy control untouched, and SHALL NOT descend to the second level. Randomize All, pressed while a first-level modulation detail grid is active, SHALL randomize that parameter's depths and SHALL also materialize and randomize their second-level depths. Randomize All pressed at the second level SHALL behave identically to Randomize Page. Randomize Page SHALL always randomize exactly what is displayed: on a parameter page, that bank's values including that bank's own Crispy, with no depths; on a modulation detail grid, that grid's depths only.

#### Scenario: The global press moves at most two banks' Crispy
- **WHEN** Randomize All is pressed while a parameter page is active
- **THEN** the number of banks whose local Crispy control changed is zero, one or two, never more
- **AND** the global Crunchy control is unchanged
- **WHEN** Randomize All is pressed many times
- **THEN** no Crispy changes on about half of those presses
- **AND** every one of the six banks is reached across the run

#### Scenario: The page press still owns its own Crispy
- **WHEN** Randomize Page is pressed on a parameter page
- **THEN** that bank's own Crispy control is randomized
- **AND** no other bank's Crispy control changes

#### Scenario: Crunchy is never randomized
- **WHEN** either randomize affordance is pressed, at any level
- **THEN** the global Crunchy control holds its value

### Requirement: Randomized source count is biased toward few, and depth storage is allocated once
The randomizer SHALL draw its source count geometrically, each count half as likely as the one below it, from a floor that depends on WHICH GESTURE was pressed and where: Randomize All at a drilled-in modulation level draws from a floor of one, so every such press draws at least one source and one source is the most likely outcome; every other randomize draw — Randomize All on a parameter page, and Randomize Page at any level — draws from a floor of zero, so about half of those calls draw no sources at all. Depth storage for a given source SHALL be allocated once, on first use, rather than accumulating additional storage across repeated randomization presses.

A parameter the zero-floor draw leaves at no sources SHALL carry no modulation
depth and SHALL therefore show no modulation badge, so that a randomized bank
reads as a set of deliberate choices rather than as everything touched at once.

A drilled-in Randomize All press SHALL NOT be a no-op: the floor of one applies
both to the selected parameter's own depths and to the one-level descent that
materializes the level below it.

Randomize Page SHALL keep the zero floor at every level, because its contract is
to randomize exactly what is displayed and a floor is not part of that.

#### Scenario: Some parameters come out of a page randomize untouched
- **WHEN** Randomize All is pressed while a parameter page is active
- **THEN** about half the parameters carry no modulation depth
- **AND** those parameters show no modulation badge
- **AND** the remaining parameters carry at least one non-neutral depth

#### Scenario: A drilled-in Randomize All always moves something
- **WHEN** Randomize All is pressed while a modulation detail grid is active
- **THEN** the selected parameter carries at least one non-neutral depth, on every press
- **AND** one source is the most common outcome, two about half as often, three about half as often again
- **AND** each depth the press materializes is itself randomized under the same floor

#### Scenario: A drilled-in Randomize Page keeps the zero floor
- **WHEN** Randomize Page is pressed while a modulation detail grid is active
- **THEN** the draw is unchanged from a page-level draw: no sources on about half of presses

#### Scenario: Wide draws stay rare
- **WHEN** modulation depths are randomized repeatedly on a parameter page
- **THEN** four or more sources are affected on about one call in sixteen

### Requirement: The encoder grid owns its own colour language

The encoder grid SHALL define its own colours, including the colour that marks
a cell unavailable, rather than drawing them from the runtime configuration
pages' palette.

The two are separate visual systems addressed to different readers. A
configuration page is a form, read as text against a panel. The encoder grid is
an instrument surface, read as a field of illuminated cells at a glance. A
colour that reads as correctly de-emphasised in one is not the colour that reads
that way in the other, and a shared constant would make one of them wrong to
serve consistency no viewer experiences.

Where the two SHOULD agree, that is a palette decision taken once for both and
applied deliberately — not an import added at whichever site was being edited
when the mismatch was noticed.

#### Scenario: A disabled cell is not bound to the page palette
- **WHEN** the configuration pages' disabled colours change
- **THEN** the encoder grid's disabled cell colour is unaffected

#### Scenario: Neither palette is the other's source
- **WHEN** either surface's disabled colour is chosen
- **THEN** it is chosen for that surface's own reading conditions

