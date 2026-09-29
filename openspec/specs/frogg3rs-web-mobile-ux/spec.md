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
weight, or a nested arrangement — selected by the width a host reports, rather
than as a shell-side CSS or DOM rearrangement. A shell that moves, clips, or
duplicates controls THIS SURFACE emits makes the rendered tree disagree with
the surface that produced it, and the surface's own tests can no longer tell
what a viewer sees.

<!-- RESTATES-EXCEPT
the browser host sets the narrow-viewport flag
  keeps: none
substantially the full
  keeps: every block this surface emits
the shell places Sheaf's runtime sidebar at a phone-width viewport
  keeps: Sheaf's runtime sidebar at a phone-width viewport
live rendered box
  keeps: not from a fixed offset
the narrow-viewport flag is false
  keeps: the desktop layout is emitted, with the desktop split weights
-->

Blocks emitted by the RUNTIME rather than by this surface — Sheaf's sidebar is
the only one — are placed by the runtime at a slot this surface declares,
because this surface's tree cannot contain them. The slot SHALL be a node this
surface emits, so the arrangement stays readable from the surface's tree,
rather than a hardcoded offset or a specific control's node id.

The narrow topology SHALL be free to differ in the outer arrangement — blocks
stacked in a column rather than split side by side — and in a block's internal
arrangement, not only in the order of rows within one block. A narrow topology
that leaves a surface-emitted block narrower than the root it is laid out in
SHALL be treated as an incomplete topology.

The surface SHALL be narrow when a host reports a width greater than 0 and at
most 720 px, and wide otherwise. Only the browser host and the Android host
SHALL report a width. No desktop standalone or plugin path SHALL report one.

#### Scenario: No duplicate controls at any width
- **WHEN** the surface renders at any viewport width
- **THEN** exactly one node exists for each of Randomize Page, Randomize All,
  Reset Page and Reset All
- **AND** the container carrying them is the only one: the wide layout's two
  rows and the narrow layout's column never both exist
- Check: the one-node-per-button assertion in `narrow_layout_stacks_chrome_above_the_grid`, `app/FroggersSurfaceTests.cpp` — pass (confirmed via the full `make -C app test` runs recorded when F1/F2/F3 were committed).

#### Scenario: Other hosts are unaffected
- **WHEN** the standalone, VST, or AU host builds the surface
- **THEN** no width is reported, the surface is wide, and the desktop layout is
  emitted, with the desktop split weights
- Check: `a_width_above_the_narrow_limit_keeps_the_desktop_layout` in `app/FroggersSurfaceTests.cpp` — pass (confirmed via the full `make -C app test` runs recorded when F1/F2/F3 were committed).

#### Scenario: A narrow topology leaves no surface block short of the viewport
- **WHEN** a host reports a width of 720 px or less
- **THEN** every block this surface emits spans the root width less the
  surface's own margins
- Check: `narrow_layout_stacks_chrome_above_the_grid` in `app/FroggersSurfaceTests.cpp` — pass (confirmed via the full `make -C app test` runs recorded when F1/F2/F3 were committed).

#### Scenario: The runtime sidebar is placed from a surface-emitted box
- **WHEN** the runtime places Sheaf's runtime sidebar at a phone-width viewport
- **THEN** its position is the slot node the surface emits under its own
  narrow button column, not from a fixed offset
- Check: `narrow_sidebar_slot_sits_under_the_buttons` in `app/FroggersSurfaceTests.cpp` — pass (confirmed via the full `make -C app test` runs recorded when F1/F2/F3 were committed).

#### Scenario: The reported width chooses the topology
- **WHEN** a host reports 720 px, then 721 px
- **THEN** the first emits the narrow topology and the second the desktop one
- Check: `a_width_above_the_narrow_limit_keeps_the_desktop_layout` in `app/FroggersSurfaceTests.cpp` — pass (confirmed via the full `make -C app test` runs recorded when F1/F2/F3 were committed).

### Requirement: Playwright regression coverage

The browser e2e suite SHALL include a mobile-emulated test asserting both the drag behavior and the Randomize/Reset placement without starting audio.

#### Scenario: CI mobile UX test

- **WHEN** the browser e2e suite runs with mobile emulation
- **THEN** the mobile UX spec passes

### Requirement: Runtime page buttons sit beside the sliders on a phone

The Audio I/O, Controllers, Sync and File buttons SHALL render inside the chrome
block at a viewport width of 720px or less, below the Randomize and Reset
buttons and beside the sliders, rather than stacked below the encoder grid.

They SHALL fit within the chrome block's own height. They sit in a slot node the
surface emits inside that block, and the runtime refuses to compose a sidebar
that does not fit its slot, so a sidebar larger than the space under the buttons
fails composition rather than overlapping the encoder grid.

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

### Requirement: Every runtime page opens at phone width
At a phone width, THE published site and the Android app SHALL open Audio I/O,
Controllers, Sync and File from the sidebar. An open runtime page SHALL keep its
own configured size with the sidebar at its right edge, fitted to the screen
width by the host, and returning to the app SHALL restore the stacked
arrangement.

#### Scenario: Each page opens in the phone browser
- **WHEN** the site at 390 px wide opens Audio I/O, Controllers, Sync and File in turn
- **THEN** each page renders within the viewport with no boot-error notice
- **AND** returning to the app puts the encoder grid back at the full viewport width
- Check: `app/browser/e2e/runtime-pages-narrow.spec.mjs`, `Audio I/O, Controllers, Sync and File each render usably, then the app's narrow layout returns`.

#### Scenario: Each page opens in the Android app
- **WHEN** the Android app in portrait opens Audio I/O, Controllers, Sync and File in turn
- **THEN** each page shows and the app keeps running
- Check: operator step — pass, frogg3rs-android-app task 4.7: Audio I/O, Controllers, Sync and File each opened via the sidebar showed page-specific content (e.g. "Input device"/"Output device", "Available controllers", sync fields, "Patch root: ..."), with the process alive 10 s+ after each and no fatal/exception line in the device log.

