## Why

`9d4a320` ("Carry the Controllers-page device label work and pin Sheaf")
carried Sheaf's `8e56c779` into this tree: it bumped the `External/Sheaf` pin
and updated `app/FroggersControllersPageTests.cpp` and `app/Makefile`'s
comment to match the registry's added library device. Sheaf's own commit
shipped that device label alongside a row-level pressure-mapping editor it
admits is defective, three untested refusal paths, a width check that only
measures Sheaf's own library device names, and a browser e2e spec asserting a
label the row no longer shows. Sheaf's paired change,
`finish-controller-row-device-editing`, removes the pressure-mapping editor,
closes the test gaps, and fixes what they find; none of that touches
`app/FroggersMidiCatalog.hpp` or frogg3rs's own device defaults.

This change is what frogg3rs does once Sheaf's change lands: bump the pin
again, confirm frogg3rs's own Controllers-page and catalog tests still pass
against the new Sheaf commit, and supply the operator's screenshots of the
real app for the shared delivery gate.

## What Changes

- **Bump the Sheaf pin** to `finish-controller-row-device-editing`'s landed
  commit once that change's own tasks and gate are done.
- **Re-run frogg3rs's own suite** against the new pin:
  `app/FroggersControllersPageTests.cpp` and
  `app/FroggersMidiCatalogTests.cpp`, since neither references the removed
  pressure-mapping API directly but both link against
  `MakeControllerWizardRegistry` and `ControllersPageUI.hpp`, which do
  change.
- **Supply the app-side half of the shared delivery gate**: frogg3rs is the
  real, shipping app; the operator's screenshot review names states best
  captured here rather than in Sheaf's harness.

## Capabilities

### New Capabilities
None.

### Modified Capabilities
- `froggers-midi-controller-mappings`: adds the requirement that frogg3rs's
  own catalog tests track Sheaf's row-editing contract across a pin bump,
  and that no frogg3rs source assumes the row-level pressure-mapping editor
  Sheaf's paired change removes. Nothing here previously described that
  editor, the device label, or the width check; this is additive text
  carrying a dependency's fix forward, not a change to frogg3rs's own
  documented behavior.

## Impact

- `External/Sheaf`: pin bump only.
- `app/FroggersControllersPageTests.cpp`, `app/FroggersMidiCatalogTests.cpp`:
  no source change expected; both are re-run against the new pin as a
  regression check, since MIDI_CATALOG_BIN and CONTROLLERS_PAGE_BIN both link
  the Sheaf headers the paired change edits.
- Depends on Sheaf's `finish-controller-row-device-editing` landing first;
  this change's own task 1 cannot start until that one's tasks and delivery
  gate are done.

## Delivery Gate

Shares Sheaf's delivery gate. Before this merges to main, the operator
reviews screenshots of the real frogg3rs app's Controllers page in the six
states Sheaf's paired change names: a resolved row's preset device label, an
unresolved row's bound-input label, the add row's Preset dropdown (frogg3rs's
own six devices plus the one library device for the kind its catalog does
not cover, plus Custom), a row's Connect messages list, that list after a
refused edit, and a row's expanded editor with no Pressure mappings list
under it.
