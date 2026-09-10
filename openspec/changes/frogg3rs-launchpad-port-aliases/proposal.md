# Proposal — `frogg3rs-launchpad-port-aliases`

**Created 2026-09-09.** Paths are repo-root relative. Line numbers are
2026-09-09 reads of `main` at `40b417c`; the working tree is clean apart from
this change's own directory. Sheaf is the submodule at `External/Sheaf`,
pinned at `ddb14693`; Sheaf paths below are relative to
`External/Sheaf/projects/synth/`.

## What the operator reported

> launchpad has a slightly different sysex depending on pro, minimk3 or x. i
> think the config page wont let me select mini, i suspect this is a bug in
> the minimk3 preset. plz fix

## Root cause

A connected Launchpad is never offered on the Controllers page, because none
of the three presets' port aliases equal the name the host reports for the
unit's ports.

Alias matching is whole-string, case-insensitive equality:
`MatchesAnyAlias` (`src/ControllerWizard.cpp:228`) compares each alias to the
endpoint name through `CaseInsensitiveEquals` (`:214`), and
`MatchingUnclaimedEndpoints` (`:280`) admits only endpoints that match that
way. `DiscoverControllerWizards` (`:954`) pairs an input and an output per
descriptor from those matches, and everything else falls to
`unmatchedInputs`/`unmatchedOutputs`.

The names the app compares against come from
`juce::MidiInput::getAvailableDevices()` (`runtime/MidiConnectionManager.hpp:118`).
In the JUCE this application is built against (8.0.12), that call resolves
through the UMP endpoint layer
(`~/JUCE/modules/juce_audio_devices/midi_io/juce_MidiDevices.cpp:198`), and the
name is built in `computeInfo`
(`~/JUCE/modules/juce_audio_devices/midi_io/juce_MidiDeviceListConnectionBroadcaster.cpp:151`)
as the endpoint's name, a space, and the group's block name -- for a
CoreMIDI unit, the device's name followed by the port's name.

Read on this Mac with the operator's Launchpad Mini MK3 connected, by calling
`getAvailableDevices()` through the application's own compiled JUCE objects
(`app/build-launcher/juce_audio_devices.o` and the rest of its module set),
the names the page compares against are:

| direction | name the app sees | what it is |
| --- | --- | --- |
| input | `Launchpad Mini MK3 LPMiniMK3 MIDI Out` | the unit's MIDI port |
| input | `Launchpad Mini MK3 LPMiniMK3 DAW Out` | the unit's DAW port |
| output | `Launchpad Mini MK3 LPMiniMK3 MIDI In` | the unit's MIDI port |
| output | `Launchpad Mini MK3 LPMiniMK3 DAW In` | the unit's DAW port |
| both | `Midi Fighter Twister` | the Twister, one name per direction |

The direction word belongs to the port and is the opposite word on each side:
the unit's MIDI Out is what the application opens as an input.

The preset offers `LPMiniMK3 MIDI`, `Launchpad Mini MK3 LPMiniMK3 MIDI` and
`Launchpad Mini MK3` (`app/FroggersMidiCatalog.hpp:271`), the same list for
both directions (`:240-241`). Every entry is missing the trailing `In`/`Out`
the endpoint name carries, so none is equal to either name. One list for both
directions cannot be right for a unit whose two endpoints are named
differently: the direction word is part of the name, and it is the opposite
word on each side.

Behavioural check, run before this proposal was written. Discovery against
those live names, with the operator's whole device list:

```
shipping aliases:            1 paired (MIDI Fighter Twister); Mini MK3 unmatched
mini aliases set to the
real per-direction names:    2 paired (Twister + Launchpad Mini MK3)
```

The Twister pairing is the positive control: it matches because its endpoint
name is exactly its device name on both sides, which is why the same
whole-string rule has never failed for it. Launchpad X and Pro MK3 carry the
identical alias shape (`:257`, `:264`), so neither pairs either.

## What is not the cause

- The Mini MK3's programmer-mode SysEx is correct.
  `{0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0x01, 0xF7}` (`:272`) carries
  the Mini MK3 product byte `0x0D` that the library's own
  `LaunchpadProductByte` is tested against
  (`tests/parameter_modulation_tests.cpp:7217`).
- The preset is registered and installs.
  `MakeControllerWizardRegistry` returns all six device defaults, the add
  row's Preset combo lists `Launchpad Mini MK3` as its sixth entry, and
  `InstallDescriptorProfile` generates the slot (14 system messages, the
  9-byte open SysEx) which `MidiInstrumentConfig::AddController` accepts.
  Verified by rendering the page's own node tree.
- Binding by hand works. The endpoint combos list every enumerated port
  (`BuildEndpointOptions`, `include/synth/ControllersPageUI.hpp:829`), and a
  slot with hand-bound Launchpad ports is accepted.

## What Changes

- `app/FroggersMidiCatalog.hpp`: `LaunchpadDeviceDefault` takes an input alias
  list and an output alias list rather than one list used twice, and each
  Launchpad preset names the host-reported form per direction ahead of the
  shorter forms it already carries:
  - Launchpad X: input `Launchpad X LPX MIDI Out`, output
    `Launchpad X LPX MIDI In`.
  - Launchpad Pro MK3: input `Launchpad Pro MK3 LPProMK3 MIDI Out`, output
    `Launchpad Pro MK3 LPProMK3 MIDI In`.
  - Launchpad Mini MK3: input `Launchpad Mini MK3 LPMiniMK3 MIDI Out`,
    output `Launchpad Mini MK3 LPMiniMK3 MIDI In`.
  The existing three entries stay on both sides, so a host that reports a
  shorter name still pairs.
  The comment at `:250-253` is rewritten: the Mini MK3's names are read from
  the connected unit through the application's own JUCE, the other two are the
  same construction with the model's own interface name and are still
  unconfirmed -- what is unknown for them is the port name their firmware
  reports, not how the host assembles it.
- `app/FroggersControllersPageTests.cpp`: a new case drives
  `DiscoverControllerWizards` against a device list holding the names a host
  reports for all three Launchpads, the Twister, and the DAW ports beside the
  MIDI ports, and requires each Launchpad preset to pair with its unit's MIDI
  port and with neither DAW port. The Mini MK3 row of that table is the
  hardware-read one; the X and Pro MK3 rows are constructed the same way and
  are named as unconfirmed in the test's own comment.
- `MANUAL.md:359-381`: the Mini MK3 section drops the "unconfirmed, bind by
  hand" caveat; the X and Pro MK3 sections keep it and say what confirmed it
  for the Mini.
- `openspec/specs/froggers-sheaf-runtime-app` gains one requirement (delta in
  this change): a preset's port aliases are per direction and include the name
  the host reports.

## Impact

- Affected specs: `froggers-sheaf-runtime-app` (ADDED requirement).
- Affected code: `app/FroggersMidiCatalog.hpp`,
  `app/FroggersControllersPageTests.cpp`. Nothing else reads the alias values:
  a sweep of `app/`, the root documents and `openspec/` found the alias
  strings at 7 lines, all in `app/FroggersMidiCatalog.hpp` (`:97`, `:98`,
  `:176`, `:177`, `:257`, `:264`, `:271`), and no test asserting any literal.
- Affected documents: `MANUAL.md` (the three Launchpad subsections).
- Not touched: the Twister and APC40 aliases. The Twister's is confirmed by
  the same live read; no APC40 was connected to read, and its endpoint naming
  is not what this change is about.
- Not touched: Sheaf. The matcher's whole-string rule stays as it is; a
  looser rule would change pairing for every application built on the
  library, and would risk pairing a Launchpad's DAW port, which is not the
  port Programmer Mode uses.
- Browser host: unverified. Web MIDI port names could not be read in this
  session; the shorter aliases remain for that reason.
- Delivery: one commit pushed to `main`, this repository's convention.
