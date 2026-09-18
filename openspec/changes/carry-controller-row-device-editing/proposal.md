## Why

Sheaf's paired change finishes the Controllers-page row work: each row names
the device it is, the add row offers one Custom entry, and a row's connect-time
messages are editable. frogg3rs consumes that runtime library through its
`External/Sheaf` pin.

Two things in this tree are false because of it, and neither belongs to Sheaf,
which cannot edit frogg3rs's text.

The promoted requirement "The MIDI configuration page fits this application's
window in every state" still says the controller header's identity line carries
the device *kind*, that the page shows a controller's device kind by its display
name, and that the add row offers "a Custom entry per device kind and nothing
else". The shipped page renders the device — the preset's device name, or the
bound input when no wizard id resolves — and offers exactly one Custom entry.
Kind is no longer what identifies a row; the device is. A spec this change's
own delivery screenshots would contradict is corrected inside the change that
ships the behaviour, not deferred.

`app/FroggersMidiCatalog.hpp`'s header comment describes the add control as the
"Layout dropdown" where the page's caption is Preset, and says Custom leaves a
slot's mappings untouched and editable by hand, where Custom now adds an empty
Generic row.

## What Changes

- **Correct the promoted requirement** so it describes the row as it renders:
  identity is the row's name and its device label, the page shows the device
  rather than the kind, and the add row offers the application's presets
  followed by one Custom entry. Its "The row reads as its parts" scenario is
  corrected the same way. Every other clause and scenario in that requirement
  is carried through unchanged.
- **Correct `app/FroggersMidiCatalog.hpp`'s header comment** where it is false.
- **Bump the `External/Sheaf` pin** to the commit that carries Sheaf's
  implementation, and re-run this app's own Controllers-page and catalog test
  binaries against it.
- **Supply the app-side screenshots** for the shared delivery gate.

`MANUAL.md` also describes a Custom entry for each device kind, and is false for
the same reason. User documentation is reviewed as its own step after the code
diff, so the manual is corrected there rather than here; this change names it so
the step has it.

## Capabilities

### Modified Capabilities
- `froggers-sheaf-runtime-app`: the requirement above is corrected to the
  behaviour that ships. This removes a contradiction between the promoted spec
  and the code; it does not change what the app does.

The previously proposed addition to `froggers-midi-controller-mappings` is
dropped. It asserted that this app's tests keep passing when the pin moves and
that no source assumes a row-level pressure-mapping editor — a standing
obligation whose own scenario declared no check, and a second scenario nothing
could falsify. What actually enforces the first is the app's own suite, which
fails when a pin bump breaks it; a requirement restating that obligation adds
no check that could catch anything the suite does not.

Worth naming, because it is not true of this repository pair and this change
does not fix it: `app/check_spec_checks_resolve.py`, run from `app/Makefile`,
resolves `Check:` lines found in this repository's specs and indexes test cases
from both trees, but its spec scan never reads `External/Sheaf/openspec` — so a
`Check:` line written into a Sheaf spec is gated by nothing in either
repository. Closing that is its own change, not this one.

## Impact

- `openspec/specs/froggers-sheaf-runtime-app/spec.md`: one requirement and one
  scenario corrected.
- `app/FroggersMidiCatalog.hpp`: header comment corrected.
- `External/Sheaf`: pin bump.
- `app/FroggersControllersPageTests.cpp`, `app/FroggersMidiCatalogTests.cpp`:
  no source change expected; both are re-run against the new pin, since each
  links the Sheaf headers the paired change edits.
- `MANUAL.md`: named for the documentation step, not edited here.
- Depends on Sheaf's `finish-controller-row-device-editing` landing first.

## Delivery Gate

Shares Sheaf's gate. Before this merges to main, the operator reviews
screenshots of the real app's Controllers page in the six states that change
names: a resolved row's preset device name, an unresolved row's bound-input
label, the add row's Preset dropdown listing this app's devices plus one
library device per uncovered kind plus Custom, a row's Connect messages list, a
refused connect-message edit, and a row's expanded editor with no Pressure
mappings list.

That last state shows the absence only. A pressure mapping attached to a grid
cell is represented by the grid button itself, so a grid row holding one looks
identical to one that does not; its survival is carried by the grid tests, not
by an image.
