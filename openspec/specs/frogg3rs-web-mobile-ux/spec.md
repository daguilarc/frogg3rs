# frogg3rs-web-mobile-ux Specification

## Purpose
TBD - created by archiving change frogg3rs-windows-and-mobile. Update Purpose after archive.
## Requirements
### Requirement: Encoder drags work on mobile

`Draw` nodes in the browser surface SHALL declare `touch-action: none` so that touch drags on encoder knobs are not reinterpreted as browser scroll or pan gestures. Container rows, buttons, and non-canvas areas SHALL keep the default `touch-action` so the page remains scrollable by dragging there.

#### Scenario: A touch drag on an encoder reaches pointerup

- **WHEN** a finger touches an encoder knob and drags
- **THEN** the surface receives `pointerdown`, `pointermove`, and `pointerup`
- **AND** `pointercancel` does not fire, and `lostpointercapture` does not fire before `pointerup`

#### Scenario: Page scrolling still works on non-canvas areas

- **WHEN** a finger drags on the site header, footer, or gaps between controls
- **THEN** the page scrolls normally

### Requirement: The surface owns its own mobile topology

A mobile-only difference in what the surface emits SHALL be expressed as data
consumed by the surface's existing emission code — a row table, a block
weight, or a nested arrangement — selected by a flag that defaults to false,
rather than as a shell-side CSS or DOM rearrangement. A shell that moves,
clips, or duplicates controls THIS SURFACE emits makes the rendered tree
disagree with the surface that produced it, and the surface's own tests can no
longer tell what a viewer sees.

Blocks emitted by the RUNTIME rather than by this surface — Sheaf's sidebar is
the only one — are the shell's to place, because this surface's tree cannot
contain them. Where the shell places such a block relative to this surface's own
blocks, it SHALL derive the position from a box this surface emits, read live,
rather than from a hardcoded offset or a specific control's node id. A surface
that wants to reserve space for a runtime block SHALL do so by declaring the
extent of one of its own nodes, so the arrangement stays readable from the
surface's tree.

The narrow topology SHALL be free to differ in the OUTER split weights and in
a block's internal arrangement, not only in the order of rows within one
block. Where the shell derives a single scale from one block, a narrow
topology that leaves another SURFACE-EMITTED block narrower than the viewport
SHALL be treated as an incomplete topology rather than as a shell defect.

The flag SHALL be set only by the browser host. No desktop, standalone, or
plugin path SHALL set it.

#### Scenario: No duplicate controls at any width

- **WHEN** the surface renders at any viewport width
- **THEN** exactly one node exists for each of Randomize Page, Randomize All,
  Reset Page and Reset All
- **AND** the container carrying them is the only one: the wide layout's two
  rows and the narrow layout's column never both exist

#### Scenario: Other hosts are unaffected

- **WHEN** the standalone, VST, or AU host builds the surface
- **THEN** the narrow-viewport flag is false and the desktop layout is
  emitted, with the desktop split weights

#### Scenario: A narrow topology leaves no surface block short of the viewport

- **WHEN** the browser host sets the narrow-viewport flag
- **THEN** every block this surface emits renders at substantially the full
  viewport width under the shell's shared scale

#### Scenario: The runtime sidebar is placed from a surface-emitted box

- **WHEN** the shell places Sheaf's runtime sidebar at a phone-width viewport
- **THEN** its position is derived from the live rendered box of the surface's
  own narrow button column, not from a fixed offset

### Requirement: Playwright regression coverage

The browser e2e suite SHALL include a mobile-emulated test asserting both the drag behavior and the Randomize/Reset placement without starting audio.

#### Scenario: CI mobile UX test

- **WHEN** the browser e2e suite runs with mobile emulation
- **THEN** the mobile UX spec passes

### Requirement: Runtime page buttons sit beside the sliders on a phone

The Audio I/O, Controllers, Sync and File buttons SHALL render inside the chrome
block at a viewport width of 720px or less, below the Randomize and Reset
buttons and beside the sliders, rather than stacked below the encoder grid.

They SHALL fit within the chrome block's own height. The mount clips to that
height, so a runtime block placed past the block's bottom edge is cut off rather
than shown.

The runtime page whose name collides with this instrument's own Audio parameter
page SHALL be renamed, and the rename SHALL come from a host-supplied
configuration value rather than from the shell rewriting a rendered label, so
the rendered page and the tree that produced it agree.

#### Scenario: The runtime page buttons are beside the sliders

- **WHEN** the site loads at a viewport width of 720px or less
- **THEN** each of the four sidebar buttons has its horizontal centre to the
  RIGHT of the BPM slider's horizontal centre
- **AND** each falls inside the chrome block's bounding box
- **AND** each sits below the lowest Randomize or Reset button

#### Scenario: They are not below the grid any more

- **WHEN** the site loads at a viewport width of 720px or less
- **THEN** no sidebar button falls inside the encoder grid block's bounding box
- **AND** no sidebar button renders below the encoder grid block

#### Scenario: The audio page is named for what it does

- **WHEN** the sidebar renders in any host that sets the configuration value
- **THEN** the first page button reads "Audio I/O"
- **AND** a host that sets nothing still gets the runtime's own default name

### Requirement: The page scrolls, and a test proves it

On mobile-width viewports the published page SHALL scroll when its content is
taller than the viewport, and a scroll offset once set SHALL persist rather
than returning to the top on a later animation frame.

The suite SHALL assert this directly. The existing "page scrolling still
works" scenario has never had an assertion behind it, which is how a build
that cannot scroll at all reached the published site. An assertion SHALL
check the offset after several animation frames, not immediately, because the
offset survives the first frames and is lost afterwards.

#### Scenario: A scroll offset survives the render loop

- **WHEN** the page is scrolled down at a viewport width of 720px or less
- **AND** several animation frames elapse
- **THEN** the scroll offset is still where it was put

#### Scenario: Controls below the fold are reachable

- **WHEN** the content is taller than the viewport at a phone-width viewport
- **THEN** every emitted control can be brought into view by scrolling

