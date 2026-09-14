## Context

Device preconditions exist twice, as prose, with nothing joining them:
`app/FroggersMidiCatalog.hpp:14-24` for the reader of the catalogue, and
`MANUAL.md:336-339` for the operator. They agree today. Nothing makes them
agree, and neither is shown at the moment a preset is chosen.

The analog path is already built. `AnalogMidiInConfig`
(`External/Sheaf/projects/synth/include/synth/MidiController.hpp:296-300`)
carries `gestures`, a first-class `sceneBlend` address, and `appActions`. Scene
blend is decoded and dispatched directly; BPM travels through an app-action row
with a declared range and a 0-1 to 30-300 rescale. Both are already assigned on
the APC40 mkII entries at `app/FroggersMidiCatalog.hpp:165-166` — scene blend to
the crossfader at channel 0 CC 15, BPM to the master fader at CC 14 — and
`MANUAL.md:348` describes exactly that.

What no device this operator owns can do is carry either.
`synth-midi-instrument` pins kind support: "twister: encoders, system messages;
launchpad: system messages only; generic: all sections". The Twister and all
three Launchpads therefore refuse an analog section by specification, and
neither has a fader to put one on. The Launch Control XL, which has eight, is
not in the catalogue.

## Goals / Non-Goals

**Goals:**
- A preset's device preconditions are one declaration that the catalogue, the
  Controllers page and the manual all read, with drift caught by a check.
- The manual's stuck-modifier recovery describes a recovery that exists.
- Scene blend is reachable on a fader of a device the operator owns.

**Non-Goals:**
- Any analog mechanism. One exists, and it is more direct for scene blend than
  for BPM.
- Enabling analog sections on the Twister or Launchpad kinds. Spec-pinned,
  and neither device has a fader.
- Held-modifier lifetime, mismatch reporting and template-change handling, which
  are Sheaf's `midi-controller-resilience`.

## Decisions

**Preconditions are declared data on the device default, and the manual is
generated from them.** A comment cannot be checked. Putting the declaration on
`MidiAppDeviceDefault` — the field Sheaf's change adds — makes the catalogue the
single source, lets the Controllers page render it where the choice is made, and
makes the manual a rendering rather than a second assertion. The check compares
the rendered section against the declarations and fails on divergence, which is
the guard `frogg3rs-midi-shift` did not have and whose absence cost the operator
three side buttons.

**The manual's recovery is rewritten to the triggers, not to a new promise.**
`MANUAL.md:319-320` currently says a stuck Shift clears when Shift is pressed and
released again. That is unreachable exactly when it is needed — the failure that
strands a profile is the Shift address ceasing to transmit. The replacement
states the four triggers Sheaf's change defines and says which are available on
which host, and covers Hold Drill, whose failure costs every encoder.

**The Launch Control XL is catalogued as `Generic`, not as a new kind.**
`Generic` already supports all sections by the pinned spec, so no Sheaf spec
change is needed and no new kind has to be threaded through validity, the
wizard, or the page. The alternative — a dedicated kind — would buy a tighter
default at the cost of a Sheaf spec change and a new enumerator in a family that
three consumers switch over.

**The template is a declared precondition, and the CC map is a task, not an
assertion.** The device's control map moves with its template: captures on
2026-09-09 show fader 1 sending CC 77 on channel 6 under one template and CC 77
on channel 2 under another. Writing a map from those captures would be asserting
a layout observed while the template was itself changing. The map is determined
by reading the device under a pinned template, and until then this change states
no CC numbers.

**Scene blend does not go on fader 1.** That control is tied to an unreproduced
SysEx flood on this device, characterised across the three preflight reports
carried in this change. Defaulting the one continuous control the operator asked
for onto the one control with a known unexplained fault would be choosing the
worst available address.

## Risks / Trade-offs

- **The manual is held by another change.** `frogg3rs-density-documents-and-spec`
  owns `MANUAL.md` and `QUICK_DICT.md` right now. → Sequence after it, or
  coordinate the two edits; never edit concurrently.
- **The declared-preconditions field belongs to Sheaf's change.** → This change
  populates and consumes it and lands after; it does not define the struct.
- **A generated manual section can flatten prose.** → Only the settings table is
  generated; the surrounding explanation stays hand-written.
- **The LCXL map is unknown until measured.** → No CC numbers are asserted
  anywhere in this change, and the task that determines them states what it must
  observe rather than leaving a blank.
- **A Generic-kind entry gives a looser default than a dedicated kind.** →
  Accepted; the tighter option costs a Sheaf spec change for no behaviour the
  operator asked for.

## Migration Plan

Additive. The preconditions field carries an empty default, so a preset that
declares nothing behaves as today. A new device default adds a catalogue entry
and changes no existing one. Patches store mappings, not device defaults, so a
patch saved before this change loads unchanged after it.

## Open Questions

Which fader and which template for the Launch Control XL. Determined by task
2.1; fader 1 is excluded on the grounds above.
