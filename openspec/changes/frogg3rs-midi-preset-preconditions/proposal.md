## Why

Two gaps, both found by running a preset against the hardware it names.

**A preset's device-side requirements are prose with no mechanism behind them.**
The Twister preset needs three settings written into the device's own flash —
encoders relative, all six side buttons on CC Hold, "Bank Side Buttons"
unchecked. They are stated at `MANUAL.md:336-339` and again at
`app/FroggersMidiCatalog.hpp:14-24`. Nothing ties the two together, nothing
shows them where the preset is chosen, and nothing notices when they are unmet.
An operator whose Twister had "Bank Side Buttons" on lost three of six side
buttons: the device moved their CC addresses, and the app reported nothing. The
recovery the manual offers at `MANUAL.md:319-320` — press and release Shift
again — was unreachable, because Shift was one of the addresses that had moved.

**Scene blend and BPM are already assignable to faders, on no device this
operator owns.** `AnalogMidiInConfig::sceneBlend`
(`External/Sheaf/projects/synth/include/synth/MidiController.hpp:298`) is a
first-class field, decoded more directly than BPM, and both are already
defaulted to faders on the APC40 mkII entries — scene blend to the crossfader at
channel 0 CC 15, BPM to the master fader at CC 14
(`app/FroggersMidiCatalog.hpp:165-166`, `MANUAL.md:348`). The catalogue's other
five devices cannot carry either: `synth-midi-instrument` pins kind support as
"twister: encoders, system messages; launchpad: system messages only", so both
kinds refuse analog sections outright. The operator's only device with faders,
a Novation Launch Control XL, is not in the catalogue at all. The mechanism is
not missing; the hardware entry is.

## What Changes

- A preset carries its device-side preconditions as declared data on its device
  default rather than as a comment. The Controllers page shows them where the
  preset is chosen, and `MANUAL.md`'s per-device settings table is generated
  from the same declarations, with a check that fails when the two disagree.
- `MANUAL.md:319-320`'s stuck-Shift recovery is rewritten against the four
  triggers the Sheaf change gives a held modifier, and covers Hold Drill, which
  has the same lifetime and a worse failure.
- **A Launch Control XL device default is added**, of `Generic` kind, which the
  spec already permits analog sections. Scene blend is defaulted to one of its
  faders, and the template the CC map depends on is declared as a precondition
  the same way the Twister's Utility settings are.

## Capabilities

### Modified Capabilities
- `froggers-midi-controller-mappings`: the Twister requirement's "the release is
  what ends Shift" sentence becomes one of four triggers, and its preconditions
  become declared data. Two requirements are added — one for declared
  preconditions generally, one for the Launch Control XL preset.

## Overlapping active changes

Two changes are active in this repository, and one in the submodule matters.

| change | state | overlap | disposition |
| --- | --- | --- | --- |
| `frogg3rs-density-documents-and-spec` | 0/14, created 2026-09-13 | owns `MANUAL.md` and `QUICK_DICT.md` for the density chain's close-out, and holds them modified in the main checkout | This change edits `MANUAL.md` in two places. Sequence after it releases the file, or coordinate the two edits explicitly; do not edit it concurrently. |
| Sheaf `midi-controller-resilience` | artifacts complete, not executed | adds the declared-preconditions field to `MidiAppDeviceDefault` and bounds held-modifier lifetime | This change populates the field and depends on its shape; it lands after. The manual rewrite in task 3.1 states the four triggers that change defines. |
| Sheaf `app-midi-catalog` | 26/28, PR #13 | owns `MidiAppDeviceDefault` at `MidiAppCatalog.hpp:31-38` | Neither this change nor the Sheaf one redefines the struct; both land above #13. |

## Impact

- `app/FroggersMidiCatalog.hpp` — the precondition comment at `:14-24` promoted
  to declarations on each device default; the APC40 entries' existing analog
  defaults at `:165-166` left as they are; a new Launch Control XL entry.
- `MANUAL.md:319-320` — the recovery text; `:336-339` — the settings table,
  which becomes generated.
- `openspec/specs/froggers-midi-controller-mappings/spec.md` — the promoted
  capability this change deltas.
- A new check script under `app/`, joining the eight `app/Makefile` already runs
  at `:176`, `:183`, `:191`, `:199`, `:208`, `:221`, `:228` and `:238`.

§8.0's sweep covers `app/`, `openspec/`, and the documents named above. It does
not cover `External/Sheaf`, which the Sheaf change sweeps.

## What this change does not do

It adds no analog mechanism, because one exists and is already more first-class
than BPM's. It does not enable analog sections on the Twister or Launchpad
kinds: those are pinned by `synth-midi-instrument`, neither device has a fader,
and changing them would be a Sheaf spec change serving no hardware.

## Open question

Which Launch Control XL fader carries scene blend by default, and on which
template. The CC map moves with the template — captures on 2026-09-09 show
fader 1 sending CC 77 on channel 6 under one template and CC 77 on channel 2
under another — so the template is a declared precondition and the map is
determined by task 2.1 rather than asserted here. Fader 1 is excluded: it is the
control tied to an unreproduced SysEx flood on that device, recorded in this
change's `preflight.md`, `preflight-2.md` and `preflight-3.md`.
