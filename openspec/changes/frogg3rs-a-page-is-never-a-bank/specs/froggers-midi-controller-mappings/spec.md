# Delta — `froggers-midi-controller-mappings`

The Twister preset requirement's two side-button names change from Bank
Next/Bank Previous to Page Next/Page Previous — the buttons that step the
active page — with every other word, including the pending change
`frogg3rs-shift-and-crispy-move-the-tempo`'s own addition of Crispy's
shifted-tempo turn, carried forward unchanged. This delta is written against
that change's version of the requirement (base spec `spec.md` does not yet
carry Crispy's shifted turn; the pending change's own delta does), since it
is still open in `openspec/changes/frogg3rs-shift-and-crispy-move-the-tempo`
and touches disjoint words in the same requirement body — see this change's
`proposal.md`, "Overlap with the other active change."

The button ACTION IDENTIFIERS (`FroggersActions::kBankNext`/`kBankPrevious`,
renamed here to `kPageNext`/`kPagePrevious`) keep their serialized string
values (`"froggers.bank.next"`, `"froggers.bank.previous"`) exactly as
today; only the C++ symbol names and the button LABELS a player reads
change. See `proposal.md`'s persistence trace for why the wire values do not
move.

## MODIFIED Requirements

### Requirement: The MIDI Fighter Twister preset maps five buttons with shifted jobs and one Shift
The MIDI Fighter Twister preset SHALL map its six side buttons on channel 3 (channel 4 counted from 1) at CC 8 to 13 as: CC 8 Page Next, shifted Page Previous; CC 9 Play, shifted Stop; CC 10 Freeze, shifted Reset Page; CC 11 Scene 1, shifted Scene 2; CC 12 Randomize Page, shifted Randomize All; CC 13 Shift. Reset All and Record SHALL remain on screen and SHALL NOT be on the Twister's side buttons. The preset SHALL continue to require CC Hold on every side button, because the release is what ends Shift. The preset SHALL also give two encoder turns a shifted job: the turn that moves Crunchy, position 15 on channel 0, whose shifted job SHALL be Scene blend, so that while Shift is held a turn of that encoder moves the scene blend by one turn step per detent in the direction turned and Crunchy does not move; and the turn that moves Crispy, position 14 on channel 0, whose shifted job SHALL be Tempo, so that while Shift is held a turn of that encoder moves the tempo in the direction turned, clamped to 30 at the bottom and 300 at the top, and Crispy does not move. Crispy's shifted turn SHALL move the tempo by the same amount whichever parameter page is on screen, because the shifted job addresses the tempo directly and never Crispy's own page; unshifted, Crispy SHALL keep behaving as any other per-page control, scoped to whichever page is current. Once Shift is released each encoder SHALL move its own parameter again. The preset SHALL find both turns by their parameters' slots, not by CC numbers written into the preset. No other preset SHALL map a Shift button or a shifted job, on a button or on an encoder turn.

<!-- RESTATES-EXCEPT
are Bank Next, Play, Freeze
  keeps: are Page Next, Play, Freeze
shifted jobs Bank Previous, Stop
  keeps: shifted jobs Page Previous, Stop
switched with Bank Next
  keeps: switched with Page Next
-->

#### Scenario: The preset's side buttons are the eleven named jobs
- **WHEN** the Twister device default is read
- **THEN** its six system messages, in CC order, are Page Next, Play, Freeze, Scene 1 (value "0"), Randomize Page, and a held Shift, at channel 3 CC 8 to 13 with feedback off
- **AND** the first five carry shifted jobs Page Previous, Stop, Reset Page, Scene 2 (value "1") and Randomize All, each resolving against the catalog
- Check: `app/FroggersMidiCatalogTests.cpp: device_defaults_are_valid_and_address_exactly_the_documented_controls`

#### Scenario: Only the Twister carries Shift or shifted jobs
- **WHEN** every device default is read
- **THEN** no APC40 or Launchpad association has a shifted press or a Shift press
- **AND** no APC40 encoder turn has a shifted job
- **AND** the Twister's only encoder turns with a shifted job are the one at Crunchy's slot, whose job is Scene blend, and the one at Crispy's slot, whose job is Tempo
- Check: `app/FroggersMidiCatalogTests.cpp: device_defaults_are_valid_and_address_exactly_the_documented_controls`

#### Scenario: A missing release leaves Shift held until the next press and release
- **WHEN** a controller is unplugged while its Shift button is down
- **THEN** that controller's mappings stay shifted until a Shift press and release arrive
- **AND** the manual states this and the recovery
- Check: none. MANUAL.md's Shift subsection documents this; no automated check reads it.

#### Scenario: Shift turns Crunchy's knob into the scene blend
- **WHEN** a Twister row is live, Shift is held, and the encoder at Crunchy's slot is turned clockwise
- **THEN** the scene blend rises by one turn step per detent and Crunchy's value does not change
- **WHEN** Shift is released and the same encoder is turned clockwise
- **THEN** Crunchy's value rises and the scene blend does not change
- Check: `app/FroggersMidiCatalogTests.cpp: twister_shift_turns_crunchys_knob_into_the_scene_blend`

#### Scenario: Shift turns Crispy's knob into the tempo
- **WHEN** a Twister row is live, Shift is held, and the encoder at Crispy's slot is turned clockwise
- **THEN** the tempo rises and Crispy's value does not change
- **WHEN** Shift is released and the same encoder is turned clockwise
- **THEN** Crispy's value rises and the tempo does not change
- Check: `app/FroggersMidiCatalogTests.cpp: twister_shift_turns_crispys_knob_into_the_tempo`

#### Scenario: A shifted tempo turn stops at each end of the range
- **WHEN** the tempo is at 30, Shift is held, and the encoder at Crispy's slot is turned one detent counter-clockwise
- **THEN** the tempo stays at 30
- **WHEN** the same encoder is then turned one detent clockwise
- **THEN** the tempo rises above 30
- **WHEN** the tempo is at 300 and the encoder is turned one detent clockwise
- **THEN** the tempo stays at 300
- Check: `app/FroggersMidiCatalogTests.cpp: twister_shifted_tempo_turn_stops_at_each_end_of_the_range`

#### Scenario: A shifted tempo turn moves the tempo the same on every page
- **WHEN** a Twister row is live, Shift is held, and the encoder at Crispy's slot is turned clockwise one detent on the current page
- **THEN** the tempo rises by some amount
- **WHEN** the page is then switched with Page Next and the same encoder is turned clockwise one detent again under Shift
- **THEN** the tempo rises by that same amount
- **AND** turning Crispy unshifted on the new page leaves the previous page's own Crispy value where it was, as any other per-page control does
- Check: `app/FroggersMidiCatalogTests.cpp: twister_shifted_tempo_turn_moves_the_tempo_the_same_on_every_page`

#### Scenario: The Controllers page shows both shifted jobs
- **WHEN** a Twister row's encoder section is opened on the Controllers page
- **THEN** the turn at Crunchy's slot is its own row whose Shift field reads Scene Blend
- **AND** the turn at Crispy's slot is its own row whose Shift field reads BPM
- **AND** the other fourteen turns still read as one block
- Check: `app/FroggersControllersPageTests.cpp: twister_crunchy_turn_row_shows_its_shifted_scene_blend`
