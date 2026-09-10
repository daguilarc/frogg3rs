# Proposal — `frogg3rs-launchpad-variant`

**Created 2026-09-09.** The application half of Sheaf's
`launchpad-model-on-the-row`; paths are repo-root relative, and Sheaf paths
are relative to `External/Sheaf/projects/synth/`.

## Why

A Launchpad controller's model cannot be chosen on the Controllers page. A row
added from `Custom (Launchpad)` has no mappings to read a model from and
behaves as a Launchpad X; a row added from a model preset carries that model in
its pads and cannot be pointed at another unit. The operator asked for the
choice back: "in generic launchpad i also i should be able to select which one
somehow" (2026-09-09).

The page used to offer it. Sheaf's `185a7a09` removed the Variant combo along
with the row's preset combo, and named the defect: the Variant menu "had no
state behind it: it derived the model from the first mapping carrying a grid
position and rewrote existing mappings rather than recording a choice, so on a
row with no mappings it wrote nothing and read back Launchpad X."

This application's own spec was never brought in line with that removal.
`openspec/specs/froggers-sheaf-runtime-app/spec.md:294` still requires the
controller header's identity line to hold "name, device kind, Preset, and
Variant for a Launchpad", and its scenario at `:313` still requires a Twister
row to show "the Preset selector on the first line". Rendering the page's node
tree for an expanded Launchpad row today yields a disclosure button, two
endpoint combos, Delete, Release, the rename field and button, the section
toggle and the mapping cells — neither selector is there. Both clauses have
been false at every pin since `185a7a09`.

## What Changes

- `app/FroggersMidiCatalog.hpp`: each Launchpad preset records its model on the
  profile it installs, so a row added from a preset shows that model and stamps
  rows added to it the same way.
- `app/FroggersMidiCatalogTests.cpp`: the launchpad preset case requires the
  recorded model to match the model its pads carry.
- `openspec/specs/froggers-sheaf-runtime-app` (delta in this change): the
  identity line holds the Variant selector and no per-row preset selector, and
  the row-reads-as-its-parts scenario says so.
- The submodule pin moves to the Sheaf commit carrying
  `launchpad-model-on-the-row`.

## Impact

- Affected specs: `froggers-sheaf-runtime-app` (MODIFIED requirement).
- Affected code: `app/FroggersMidiCatalog.hpp`,
  `app/FroggersMidiCatalogTests.cpp`, and the submodule pin. The Variant
  control itself is Sheaf's; this repository supplies the presets that record a
  model and the spec that says the page offers the choice.
- Delivery: the Sheaf half first, as the next sequential pull request from the
  fork; then this half pushed to `main` with the pin move in the same commit.
