## 1. Preflight

- [ ] 1.1 Enumerate by operand, case-insensitively, across `app/`, `openspec/`
      and the documents: `sceneBlend`, `AnalogMidiInConfig`, `analogRange`,
      `appActions`, `kBpm`, `MidiAppDeviceDefault`, `preconditions`,
      `KindSupport`, `analogs`, `LaunchControl`, `launch_control`.
      Report FOUND vs CHANGED per operand, zeros included.
- [ ] 1.2 Confirm by reading that no `openspec/specs/` or
      `External/Sheaf/openspec/specs/` requirement forbids a `Generic` device
      default carrying an analog section, and cite the one that permits it.
- [ ] 1.3 Confirm the APC40 entries' existing scene-blend and BPM assignments are
      unchanged by anything in this change, and record that disposition.
- [ ] 1.4 Baseline every `app/` gate with counts before touching anything — the
      eight scripts `app/Makefile` runs at `:176`, `:183`, `:191`, `:199`,
      `:208`, `:221`, `:228`, `:238` — plus `./app/build-launcher.sh`. Cap
      builds at `-j2` under `nice`.
- [ ] 1.5 §8.0 hygiene sweep over `app/` and `openspec/`. Name each directory.

## 2. Determine the Launch Control XL map

- [ ] 2.1 With the device on one pinned template, read what each of its eight
      faders transmits and record channel and CC per fader, plus the template
      that map belongs to. The deliverable is the map and the template
      identifier; a map recorded without its template is not usable.
- [ ] 2.2 Choose the fader for scene blend from that map, excluding fader 1, and
      state why the chosen one is free.

## 3. Documents

- [ ] 3.1 Rewrite `MANUAL.md:319-320` so the stuck-modifier recovery names the
      triggers that actually end a held modifier and says which are available on
      which host, and covers Hold Drill alongside Shift.
- [ ] 3.2 Coordinate every `MANUAL.md` edit with
      `frogg3rs-density-documents-and-spec`, which holds that file. Confirm it
      has released the file, or agree the edit with it, before writing.

## 4. Declared preconditions

- [ ] 4.1 Populate the Twister default's declarations from
      `app/FroggersMidiCatalog.hpp:14-24` and delete the comment those three
      settings came from.
- [ ] 4.2 Populate both APC40 mkII defaults' declarations from the same comment
      block, which carries the Track 1 caveat.
- [ ] 4.3 Render the declarations on the Controllers page where the preset is
      chosen.
- [ ] 4.4 Generate `MANUAL.md`'s per-device settings section from the
      declarations.
- [ ] 4.5 Add `app/check_docs_match_device_preconditions.py`, wire it into
      `app/Makefile` beside the eight existing checks, and prove it goes red by
      changing one declaration without changing the manual.

## 5. The Launch Control XL preset

- [ ] 5.1 Add the device default: `Generic` kind, the input and output aliases
      the device reports, and the analog section assigning scene blend to the
      fader chosen in 2.2.
- [ ] 5.2 Declare the template from 2.1 as a device-side precondition.
- [ ] 5.3 Check that the preset maps no Shift button and no shifted job.
- [ ] 5.4 Check that the chosen fader drives scene blend by the same path the
      APC40 crossfader uses, rather than by a parallel one.

## 6. Postflight

- [ ] 6.1 Re-run 1.1's enumeration against the diff.
- [ ] 6.2 Re-run every gate baselined in 1.4, naming which moved and which were
      carried forward.
- [ ] 6.3 Every scenario in the delta either has a check that passes now or says
      plainly it is not yet delivered and names what will deliver it.
- [ ] 6.4 Independent review with a fresh context.
- [ ] 6.5 Deliver: push to `main`, after Sheaf's `midi-controller-resilience`
      has landed the field this change populates. Verify clean trees at both
      levels and no submodule-pin dirt.
