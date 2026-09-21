# froggers-app-surface-layout Specification

## Purpose
The portable Froggers surface (dual VCO scope band + global chrome, sixteen-slot encoder grid, in-place modulation-detail swap) ported from the v2 layout design, built entirely with Sheaf's portable UI builder so the same description drives both the desktop and browser hosts.

## Requirements
### Requirement: Portable surface built with Sheaf UI
The Froggers surface SHALL be constructed entirely with Sheaf's portable UI builder. No JUCE component from the legacy desktop trees SHALL be reused.

#### Scenario: Surface is portable
- **WHEN** the app renders in either the desktop or browser host
- **THEN** the same portable surface description drives both
- **THEN** no legacy JUCE panel class participates

### Requirement: Scope band with global chrome
The surface SHALL present a band containing the VCO scope panels alongside transport and global controls: the **Randomize All** control, scene controls, and a tempo (BPM) slider positioned immediately beside the scene slider, so the two sliders sit together. The **Randomize Page** control SHALL NOT appear in this band; it SHALL instead appear in the per-page header. Global Crunchy SHALL NOT appear in this band either (corrected 2026-07-27 — operator: "why is there a fucking slider for crunchy between the randomize buttons, i never asked for that. It duplicates bank slot 15"); Crunchy's only control surface is bank slot 15 in the encoder grid (see the "Sixteen-slot encoder grid" requirement below), reachable like any other page parameter and, as a deliberately accepted trade-off, unreachable while a modulation view is open (slot 15 is then Target/Back).

#### Scenario: Scopes and chrome share the band
- **WHEN** the surface is displayed
- **THEN** the VCO scope panels are visible
- **THEN** transport, Randomize All, scene, and tempo controls are reachable without leaving the main view
- **THEN** no global Crunchy control appears in this band

#### Scenario: Tempo slider sets and displays the master clock tempo
- **WHEN** the operator adjusts the tempo slider
- **THEN** the master clock's tempo is set to the slider's value
- **THEN** the slider displays the active tempo

#### Scenario: External MIDI clock makes the tempo slider read-only
- **WHEN** the host is slaved to an external MIDI clock
- **THEN** the tempo slider becomes read-only and inert, and no longer accepts input
- **THEN** the slider displays the recovered external tempo instead

#### Scenario: Exactly two randomize controls exist in the whole surface
- **WHEN** the entire surface is enumerated for randomize controls
- **THEN** exactly two exist: Randomize All in the global chrome band and Randomize Page in the per-page header
- **THEN** no other randomize control appears anywhere

### Requirement: No dedicated waveform-randomize or manual random-source controls
The surface SHALL provide no dedicated waveform-randomize control and no manual random-source step/resample control. The former is retired because the waveform Shape controls are now ordinary page parameters covered by Randomize Page. The latter is retired because the random sources are driven by the master clock rather than by manual stepping.

#### Scenario: Neither control appears anywhere
- **WHEN** the entire surface is enumerated for controls
- **THEN** no dedicated waveform-randomize control exists
- **THEN** no manual random-source step/resample control exists

### Requirement: Bank selector with direct selection
The surface SHALL provide direct selection among pages. Arrow-based paging SHALL NOT be the primary navigation. Exactly one page SHALL be active at a time, with a single authority for that selection.

The surface SHALL additionally provide a back/forward arrow pair as secondary navigation, horizontally centered within the band between the page row and the encoder grid (the modulation-header row's reserved space, whose outer geometry SHALL NOT change in any state). The back arrow SHALL step the active page to the previous index and the forward arrow to the next, wrapping at both ends, routed through the same single selection authority as direct selection; the page-button highlight SHALL reflect an arrow-driven change identically to a button-driven one. WHILE a modulation drill-in is active, the arrows SHALL NOT render and SHALL NOT accept input, and the band SHALL render its drill-level title exactly as before this change.

#### Scenario: Direct page selection
- **WHEN** the operator selects a page
- **THEN** that page's parameters populate the encoder grid
- **THEN** no second, divergent page-selection state exists

#### Scenario: Forward arrow steps and wraps
- **WHEN** the operator clicks the forward arrow repeatedly from the first page
- **THEN** the active page advances one index per click, the highlight following each step
- **THEN** a click on the last page wraps the selection to the first

#### Scenario: Back arrow steps and wraps
- **WHEN** the operator clicks the back arrow on the first page
- **THEN** the selection wraps to the last page, with exactly one page highlighted

#### Scenario: Arrows yield to the drill-in title
- **WHEN** a modulation drill-in is active
- **THEN** the band shows the drill-level title with no arrow rendered and no arrow hit target
- **THEN** the band's bounds are identical to its bounds at the top level

### Requirement: Sixteen-slot encoder grid with in-place modulation swap
The surface SHALL render the active page as a sixteen-slot encoder grid indexed `0..15`, with the local Crispy control at slot index 14 and global Crunchy at slot index 15 in every page, and empty cells wherever the page has no parameter. Entering a modulation view SHALL replace the grid contents in place with the modulation detail cells, occupying the same region; it SHALL NOT open a separate window or push a new page.

#### Scenario: Empty cells render as empty
- **WHEN** the active page uses fewer than fourteen parameter slots
- **THEN** the unused cells render as empty
- **THEN** Crispy and Crunchy still occupy slot indices 14 and 15

#### Scenario: Drill-in swaps in place
- **WHEN** the operator opens a parameter's modulation view
- **THEN** the modulation detail cells occupy the same grid region
- **THEN** the surrounding scope band and chrome remain in place

#### Scenario: Return restores the parameter grid
- **WHEN** the operator activates Target/Back from any modulation level
- **THEN** the page's parameter grid is restored in the same region

### Requirement: Layout integrity at the target window size
The surface SHALL lay out without overlap or clipping at the target window size, and its regions SHALL be verified by automated bounds tests.

#### Scenario: No overlapping regions
- **WHEN** the surface is laid out at the target size
- **THEN** the scope band, chrome, page selector, and encoder grid regions do not overlap
- **THEN** every encoder cell lies fully within the grid region

### Requirement: Encoder labels are legible, natural, and never obscure the encoder
Each encoder cell SHALL render its parameter's NATURAL display label — the short form where that form is the parameter's canonical name (`A1`, `D1`, `S1`, `R1` and their siblings), the readable full name where the short form is merely a truncation (`Comb offset`, never only `CmbOff`) — per an operator-approved label list covering every parameter of every page; neither blanket expansion nor blanket abbreviation satisfies this requirement (corrected 2026-08-17 after the first draft of this requirement mandated short names universally and the operator rejected it). No label rendering SHALL overlap the encoder's ring: label draw commands occupy vertical space disjoint from the ring's drawn arc in every cell of every page. The operator's on-screen confirmation is the acceptance criterion, exercised on a proposed mock — including the full label list — BEFORE the change is built and again on the built result.

The label band's plate SHALL match the surface background. The plate stays
opaque — it is what keeps the glyphs legible when a visualizer underlay runs
under the band — but on a cell with nothing behind it the band is
indistinguishable from the background around it. The surface background SHALL
have a single named definition, read by the window's root fill, the encoder
cell fill, and the label plate, so the three cannot drift apart. Unlit ghost
segments derive their colour from the plate's, not from a second literal. The
badge chips on the encoder rings keep their own chip colour; a chip sits on
the knob, not on the surface, and is deliberately visible.

#### Scenario: Canonical short names render short
- **WHEN** a parameter's short form is its canonical name on the approved list
- **THEN** the cell renders that short form in the native single-row idiom
- **THEN** no expanded treatment is applied to it

#### Scenario: Truncations never stand alone
- **WHEN** a parameter's short form is a truncation rather than a name
- **THEN** the cell renders the readable label the approved list assigns it
- **THEN** that label is fully legible without cut-off characters, in space that does not overlap the ring's drawn arc

#### Scenario: The ring is never covered
- **WHEN** any cell of any page is rendered, with or without an expanded label
- **THEN** no label draw command's bounds intersect the ring's circle

#### Scenario: The operator gate runs before the build
- **WHEN** a change to label rendering or cell geometry is proposed
- **THEN** the operator sees and approves a mock of the geometry before implementation begins
- **THEN** the built result is confirmed on screen by the operator before the change is considered done

#### Scenario: Label plates sit flush on the background

- **WHEN** an encoder cell with no visualizer underlay renders
- **THEN** the pixels of its label plate match the surface background, and
  the band reads as glyphs on the surface, not as a lighter strip

#### Scenario: Glyphs stay legible over an underlay

- **WHEN** a visualizer underlay is visible on a cell
- **THEN** the plate occludes the underlay across the whole label band, and
  the glyphs render on the plate rather than on the moving trace

#### Scenario: One definition of the surface background

- **WHEN** the surface background colour is changed in a future edit
- **THEN** the root fill, the encoder cell fill, and the label plate all
  present the changed colour, because all three read the same single
  definition

### Requirement: The Filter page groups its controls by stage

The Filter page SHALL present its controls grouped by the stage they
belong to, whatever slot indices carry them: the Peak stage's frequency,
gain, and Q together; the Comb stage's offset, delay, feedback, lowpass,
and drive together; the Scoop stage's blend, frequency, width, and depth
together; and the two routing controls — Comb/Peak and Topology — at the
end of the page's own controls, before the page's own Crispy and the global
Crunchy, which keep their fixed positions. Whether grouping is achieved by
re-slotting or by a display-order mapping is the implementation's decision,
made only after the patch persistence format's addressing is read; saved
patches SHALL keep meaning what they meant.

#### Scenario: Stages read as clusters

- **WHEN** the operator opens the Filter page
- **THEN** every Peak control is adjacent to the other Peak controls, every
  Comb control adjacent to the other Comb controls, and every Scoop
  control adjacent to the other Scoop controls
- **THEN** Comb/Peak and Topology sit at the end of the page's own
  controls, with Crispy and Crunchy unmoved after them

#### Scenario: Saved patches survive the regrouping

- **WHEN** a patch saved before the regrouping is loaded after it
- **THEN** every parameter carries the value it was saved with, applied to
  the control it was saved from
