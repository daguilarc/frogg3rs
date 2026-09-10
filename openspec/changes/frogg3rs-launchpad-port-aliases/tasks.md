# Tasks — `frogg3rs-launchpad-port-aliases`

## 1. The failing check

- [x] 1.1 Add `launchpad_presets_pair_with_the_port_names_a_host_reports` to
      `app/FroggersControllersPageTests.cpp`: build a `MidiDeviceList` holding,
      per Launchpad model, the MIDI port pair and the DAW port pair a host
      reports, plus the Twister's single-name pair; run
      `DiscoverControllerWizards` against the real catalog's registry; require
      one candidate per Launchpad preset bound to that model's MIDI ports, the
      Twister candidate as the positive control, and both DAW ports left
      unmatched. Run it and record that it fails on the Launchpad rows and
      passes on the Twister row. Done: the Twister row asserted its pair, then
      Launchpad X found no candidate -- FroggersControllersPageTests.cpp:380.

## 2. The aliases

- [x] 2.1 Give `LaunchpadDeviceDefault` separate input and output alias
      parameters and pass each preset's two lists at its call site.
- [x] 2.2 Put the host-reported name first in each list, keeping the three
      existing entries behind it on both sides.
- [x] 2.3 Rewrite the comment above the three presets: what was read from
      hardware, what was constructed, and which is which.
- [x] 2.4 Re-run the new case and the rest of `froggers_controllers_page_tests`.

## 3. Documents

- [x] 3.1 `MANUAL.md`: drop the "unconfirmed, bind by hand" caveat from the
      Mini MK3 section and say its ports were read from a connected unit; keep
      the caveat on the X and Pro MK3 sections.

## 4. Delivery

- [x] 4.1 `make -C app test` (capped at `-j2`): 13 binaries, 356 passes, 0
      failures, recipe exit 0. Sheaf's own gate was not re-run and did not need
      to be -- this change touches no library source, and `libsynth.a` was
      already up to date when the app gate rebuilt against it.
- [x] 4.2 Commit and push to `main`.
