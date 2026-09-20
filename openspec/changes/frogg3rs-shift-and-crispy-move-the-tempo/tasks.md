# Tasks — `frogg3rs-shift-and-crispy-move-the-tempo`

Every new test is shown to fail with its production change reverted, by the
executor, before the task is reported done, and the report says so. Builds
run under `nice`, `-j2`, one at a time, and never two suites at once.

This change cannot run before the Sheaf change `shifted-turn-moves-the-tempo`
is in the pinned submodule commit. Task 1 says how to tell.

No task here depends on how far one detent moves the tempo. Every assertion
below holds at whatever value that constant takes, and any task that cannot
be written that way is not here.

- [ ] 1. Move the `External/Sheaf` pin to the commit carrying
      `shifted-turn-moves-the-tempo` on the `shifted-encoder-turns` branch.
      Check: `git -C External/Sheaf grep -c "TempoBpm" HEAD -- projects/synth/include/synth/MidiController.hpp`
      prints a non-zero count, and `git -C External/Sheaf status` is clean.
      If the Sheaf change has not landed, report that and stop.
- [ ] 2. Set `catalog.tempoAction = FroggersActions::kBpm` in
      `FroggersMidiCatalog()`, beside `catalog.encoderPressAction`.
      Check: NEW `catalog_names_the_bpm_action_as_its_tempo_action` in
      `app/FroggersMidiCatalogTests.cpp` passes: the catalog's tempo action
      is the BPM action's name, and the catalog entry it resolves to declares
      an analog range equal to `kFroggersBpmMin` and `kFroggersBpmMax`.
- [ ] 3. In `TwisterDeviceDefault()`, give the turn at `kFroggersCrispySlot`
      shifted job Tempo, found by slot as Crunchy's is, with no CC number
      written into the preset. The existing loop walks
      `config.encoderInput->turns` in position order — Crispy at position 14
      comes before Crunchy at position 15 — matches a slot, sets its
      `shiftedJob`, and `break`s. Adding a sibling check for Crispy inside
      that same loop without changing its control flow would stop the loop
      at Crispy's match and never reach Crunchy's, silently leaving
      Crunchy's shifted job unset. Write the loop so it assigns both slots —
      either by not breaking until both have been matched, or by two checks
      that do not short-circuit each other — and correct the header comment
      describing the preset, which says no other Twister encoder has a
      shifted job.
      Check: the existing
      `device_defaults_are_valid_and_address_exactly_the_documented_controls`
      passes with its shifted-turn count raised from one to two and each
      turn's job asserted by slot: Crunchy's Scene blend, Crispy's Tempo,
      every other turn and every push none, and no APC40 or Launchpad turn
      carrying one. The report additionally states, for the built preset,
      that both `kFroggersCrunchySlot` and `kFroggersCrispySlot` resolve a
      non-`None` `shiftedJob` — checked and printed explicitly, not inferred
      from the count above — so a loop that assigns one slot and silently
      drops the other cannot pass unnoticed.
- [ ] 4. Cover the knob itself. NEW
      `twister_shift_turns_crispys_knob_into_the_tempo` in
      `app/FroggersMidiCatalogTests.cpp`, written the way
      `twister_shift_turns_crunchys_knob_into_the_scene_blend` is: with a
      Twister row live and Shift held, a clockwise detent on Crispy's encoder
      raises the master clock's tempo and leaves Crispy's parameter value
      where it was; with Shift released, the same detent raises Crispy's value
      and leaves the tempo where it was.
      Check: that test passes, asserting the tempo rose and the parameter did
      not, never a tempo figure.
- [ ] 5. Cover both ends. NEW
      `twister_shifted_tempo_turn_stops_at_each_end_of_the_range` in
      `app/FroggersMidiCatalogTests.cpp`: from a tempo of 30, one
      counter-clockwise detent under Shift leaves it at 30, and the next
      clockwise detent raises it above 30; from a tempo of 300, one clockwise
      detent under Shift leaves it at 300.
      Check: that test passes. It reaches 30 and 300 by setting the tempo
      directly, so it needs no count of detents and no per-detent figure.
- [ ] 6. Cover pages. NEW
      `twister_shifted_tempo_turn_moves_the_tempo_the_same_on_every_page` in
      `app/FroggersMidiCatalogTests.cpp`: with a Twister row live, hold Shift
      and turn the encoder at Crispy's slot clockwise one detent on the
      current page and read the tempo's rise; switch the page with Bank Next;
      hold Shift and turn the same encoder clockwise one detent again and
      read the tempo's rise a second time; then, unshifted, turn the same
      encoder on the new page and confirm the previous page's own Crispy
      value is unaffected.
      Check: that test passes, asserting the two rises are equal to each
      other and that the new page's unshifted turn leaves the earlier page's
      Crispy value where it was. The assertion is written against the
      equality of the two rises, never a per-detent figure, so it holds
      whatever value `kTempoBpmPerEncoderDetent` receives.
- [ ] 7. Update `twister_crunchy_turn_row_shows_its_shifted_scene_blend` in
      `app/FroggersControllersPageTests.cpp` for the second shifted turn: the
      encoder section reads as one block of fourteen turns plus Crispy's row,
      whose Shift field reads BPM, and Crunchy's row, whose Shift field reads
      Scene Blend; the existing field edit on Crunchy's row still commits and
      leaves its shifted job alone.
      Check: that test passes with the block count still 1 and two individual
      turn rows, each read back by `EncoderTurnShiftedJobIndex`.
      `twister_row_saved_before_the_shifted_turn_gains_it_on_restore`, in the
      same file, is updated too: its `shiftedBefore` count of the old row's
      pre-Restore shifted turns moves from 1 to 2, and the post-Restore
      section it already checks reads both `EncoderTurnShiftedJobIndex`
      values back, not only Crunchy's.
- [ ] 8. Correct the comment in `app/GenerateTwisterManualLabels.cpp` that
      names the shifted-job catalog's entries as "(none)" or "Scene Blend".
      Check: the comment names the catalog without listing entries that no
      longer make the whole list, and the program still builds.
- [ ] 9. MANUAL.md: in "What can be mapped", say BPM is driven by an analog
      control or by a shifted encoder turn, keeping the 30 to 300 range; in
      the MIDI Fighter Twister subsection, replace the Crunchy-only paragraph
      with one naming both shifted turns and what each does, and say that no
      other Twister encoder has one; in the paragraph that follows it, about
      a Twister row added from the preset before this version, rewrite the
      sentence "Pressing Restore installs Shift + Crunchy and replaces edits
      made to that row." to say Restore installs both shifted turns and
      replaces edits made to that row — this is the one sentence
      `frogg3rs-row-says-it-differs-from-its-preset` leaves untouched for
      this change to own; and write the shift diagram's alt text to describe
      what the regenerated picture shows.
      Check: the subsection names Crispy and Crunchy and no other encoder;
      `grep -n "installs Shift + Crunchy and replaces" MANUAL.md` prints
      nothing and the rewritten sentence names both shifted turns; the alt
      text names both knobs' shifted readings.
- [ ] 10. Regenerate the manual's Twister artifacts: `make manual-diagrams`,
      then commit `assets/manual/twister-controls.json` and both PNGs.
      Check: `make check-twister-manual-diagrams-drift` passes, and
      `twister-controls.json` shows `"shiftedTurn": "BPM"` at position 14 and
      `"shiftedTurn": "Scene Blend"` at position 15.
- [ ] 11. Run the full app suite and the host suites CI runs, reading the
      workflows for which those are, by running every test binary by path
      after `make test` stops at the carried deadline tests.
      Check: pass and fail counts reported per binary as measured; every
      failure is either fixed here or shown to fail identically at `424d7c8`
      with the old pin. A test named red in this report is reported, never
      edited.
