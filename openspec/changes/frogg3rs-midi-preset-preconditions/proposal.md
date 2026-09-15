## Why

Two gaps, both found by running a preset against the hardware it names.

**A preset's device-side requirements are prose with no mechanism behind them.**
The Twister preset needs three settings written into the device's own flash —
encoders relative, all six side buttons on CC Hold, "Bank Side Buttons"
unchecked. They are stated at `MANUAL.md:336-339` and again at
`app/FroggersMidiCatalog.hpp:14-18`. Nothing ties the two together, nothing
shows them where the preset is chosen, and nothing notices when they are unmet.
An operator whose Twister had "Bank Side Buttons" on lost three of six side
buttons: the device moved their CC addresses, and the app reported nothing. The
recovery the manual offers at `MANUAL.md:319-320` — press and release Shift
again — was unreachable, because Shift was one of the addresses that had moved.

**Scene blend and BPM are already assignable to faders, and the Launch Control
XL is not catalogued.** `AnalogMidiInConfig::sceneBlend`
(`External/Sheaf/projects/synth/include/synth/MidiController.hpp:298`) is a
first-class field, decoded more directly than BPM, and both are already
defaulted to faders on the APC40 mkII entries — scene blend to the crossfader at
channel 0 CC 15, BPM to the master fader at CC 14
(`app/FroggersMidiCatalog.hpp:165-166`, `MANUAL.md:348`). The catalogue's other
four devices — the Twister and all three Launchpads — cannot carry either:
`synth-midi-instrument` pins kind support as "twister: encoders, system
messages; launchpad: system messages only", so both kinds refuse analog
sections outright. A Novation Launch Control XL, which has eight faders, is
not in the catalogue at all. The mechanism is not missing; the hardware entry
is — and this delivery adds it (see "The Launch Control XL fader map" below).

## What Changes

- A preset carries its device-side preconditions as declared data
  (`declaredPreconditions`) on its device default rather than as a comment,
  and `MANUAL.md`'s per-device settings paragraph is generated from the same
  declarations, with a check that fails when the two disagree. Where a
  preset's declarations are shown is Sheaf's own `synth-runtime-ui`
  requirement sru-64, not this change's concern.
- `MANUAL.md:319-320`'s stuck-Shift recovery is rewritten against the triggers
  Sheaf's `midi-controller-resilience` change's `HeldModifierClearSource`
  enumerates, and covers Hold Drill, which has the same lifetime and a worse
  failure. The Twister requirement is widened so every side button's press
  keeps dispatching a job — never silently dropped — when that button's own
  CC Hold precondition is unmet; true today of the dispatch code and now
  asserted. This does not mean the other five buttons stay in their
  unshifted form when the unmet precondition is on Shift itself — that case
  leaves them shifted until another trigger clears Shift, which is exactly
  why the recovery text above exists (design.md states both cases).
- **A Launch Control XL device default is added** — `Generic` kind, which the
  spec already permits analog sections, with scene blend on fader 1 (CC 77,
  channel 8 counted from 0 — channel 9 counted from 1) and factory template 1
  declared as a device precondition, id `froggers.launchcontrolxl`. Neither
  vendor document states the device's fader CC/channel map; the lead read it
  by disassembling Ableton Live 12 Suite's own control-surface script for the
  device instead (design.md, "Who read the script, by what command, and what
  it printed," carries the command and its literal output). See "The Launch
  Control XL fader map" below.

## Capabilities

### Modified Capabilities
- `froggers-midi-controller-mappings`: the Twister requirement's "the release is
  what ends Shift" sentence becomes one of the triggers Sheaf's
  `HeldModifierClearSource` enumerates, gains an unshifted-usability guarantee,
  and its preconditions become declared data. Two requirements are added — one
  for declared preconditions generally, one for the Launch Control XL preset.

## The Launch Control XL fader map

Task 2.1 fetched and read Novation's published Programmer's Reference Guide for
the device in full — *Launch Control XL Programmer's Reference Guide*, Version
2
(https://fael-downloads-prod.focusrite.com/customer/prod/downloads/launch_control_xl_programmer_s_reference_guide.pdf,
9 pages) — and the companion *Getting Started Guide*
(https://fael-downloads-prod.focusrite.com/customer/prod/s3fs-public/downloads/Launch%20Control%20XL%20GSG%20v2.pdf,
7 pages). Both documents describe the device's LED-lighting and System
Exclusive protocol (`Set LEDs`, `Toggle button states`, `Change current
template`) and state plainly that "8 factory templates are available. These
output a fixed set of MIDI CCs (pots, LED colours and mode buttons) and Notes
(Pads)" (Getting Started Guide, "Template Switching and the Template Editor" →
"Factory Templates") — but neither document enumerates those fixed CC numbers
or channels for any control, including the faders, on any template. The
Programmer's Reference Guide's only indexed control lists (page 7, "Set LEDs" /
"Toggle button states") number knobs and buttons for LED addressing and do not
cover faders at all. Read even more narrowly: the Getting Started Guide's own
enumeration of what a factory template controls ("pots, LED colours and mode
buttons ... and Notes (Pads)") does not name faders as a category at all, so
neither vendor document states that a fader's CC assignment changes with the
selected template in the first place — only that pots, buttons and pads do.
Whether the eight faders are template-scoped like the pots, or fixed
regardless of template, is answered by neither vendor document; it rests
entirely on Ableton's own control-surface script choosing to select factory
template 1 before constructing its sliders (design.md, "The template is a
declared precondition"), which this change reads as the script author's own
evidence that the fader CCs are template-scoped, not as a vendor statement.
This finding stands.

Per the operator's ruling for this cycle, the map is read instead from the one
source on this Mac that encodes it: Ableton Live 12 Suite's own control-surface
script for the device, at `/Applications/Ableton Live 12 Suite.app/Contents/App-Resources/MIDI
Remote Scripts/Launch_Control_XL/LaunchControlXL.pyc` (Ableton Live 12.4.3,
read from the application's own `Info.plist`). That script's module-level
constants build the same `Change current template` message the Programmer's
Reference Guide documents (`F0h 00h 20h 29h 02h 11h 77h Template F7h`) with a
template byte of 8 — the first of the 8 factory templates, per the
Programmer's Reference Guide's own "Launch Control XL MIDI Overview": "User
templates occupy slots 00h-07h (0-7), whereas factory templates occupy slots
08-0Fh (8-15)." Its channel constant is 8, and the Programmer's Reference
Guide's "Device-to-Computer messages" section states the device's own MIDI
channel is zero-indexed, so that channel is 9 counted from 1. Its
fader-building code assigns consecutive CC numbers starting at 77, one per
fader, giving CC 77 to CC 84 across the eight faders. design.md, "The template
is a declared precondition," carries the full citation and the reasoning for
reading this map from a third-party script rather than a vendor document.

**Fader 1 (CC 77) carries scene blend.** The eight faders are electrically
identical and undistinguished by function in every source read for this
change; the first is chosen rather than an arbitrary pick from the row's
middle. **Factory template 1** is declared as a device precondition, the same
way the Twister's Utility settings are, because the control map moves with the
template; the Getting Started Guide's "Template Switching and the Template
Editor" → "Template Switching" section (page 5) is where an operator sets it:
"press and hold either the User or Factory template buttons... Press pads 1-8
to select template 1-8."

Consequently:

- The catalogue's device-default count moves from six to seven. The three
  catalogue-count assertions this changes — `app/FroggersMidiCatalogTests.cpp:399`,
  `app/FroggersControllersPageTests.cpp:99`, and
  `app/FroggersControllersPageTests.cpp:109` — are named in tasks.md, task 5.5.
- Hardware confirmation is a post-delivery operator check, not a build-time
  test: no Launch Control XL is attached to this machine (`ioreg -p IOUSB -w
  0` lists only a USB Hub, a Portable SSD T5, and a USB-to-DP/HDMI adapter),
  and no unit test can drive a physical fader. On the live browser site,
  selecting the preset and moving fader 1 should move the on-screen scene
  blend value, by the same `AnalogMidiInProcessor::Process` address-match
  dispatch the APC40 crossfader already exercises in production (design.md
  names the exact symbols). Task 5.4 states this.

Everything else in "What Changes" — the declared-preconditions mechanism, the
Twister and APC40 population, and the manual rewrite and unshifted-usability
guarantee — has no hardware or documentation dependency and is delivered the
same way.

## Overlapping active changes

Re-derived directly: `git worktree list`, then `git status --short` and
`ls openspec/changes/` in each checkout it names.

```
$ git worktree list
/Users/diegoaguilar-canabal/Desktop/frogg3rs                                            634d292 [main]
/Users/diegoaguilar-canabal/Desktop/frogg3rs/.claude/worktrees/midi-resilience          a66f651 [worktree-midi-resilience]
/Users/diegoaguilar-canabal/Desktop/frogg3rs/.claude/worktrees/randomize-depth-reclaim  db65167 [worktree-randomize-depth-reclaim]
```

The sibling worktree `.claude/worktrees/midi-controller-resilience` and its
branch `worktree-midi-controller-resilience` no longer exist (see task 1.1);
the change it held, `frogg3rs-density-documents-and-spec`, went with it and
exists in no checkout.

| checkout | `openspec/changes/` holds | overlap with this change | disposition |
| --- | --- | --- | --- |
| **this worktree** (`midi-resilience`) | `frogg3rs-delay-capacity-and-width-finish`, `frogg3rs-midi-preset-preconditions` (this change), `frogg3rs-randomize-depth-reclaim` | `frogg3rs-delay-capacity-and-width-finish`'s Delay bank section (`MANUAL.md:678-742`) | Disjoint section: this change's MIDI controllers section is `MANUAL.md:258-390`. No line overlap. Diff-review `MANUAL.md` before staging; never stage a whole-file `git add` while it is active. `frogg3rs-randomize-depth-reclaim` touches neither `MANUAL.md`, `QUICK_DICT.md`, `app/FroggersMidiCatalog*`, nor `app/Makefile` (checked by name against its `proposal.md`); no overlap. |
| **main checkout** | `frogg3rs-delay-capacity-and-width-finish`, `frogg3rs-randomize-depth-reclaim` (this change, `frogg3rs-midi-preset-preconditions`, has not been delivered to `main`) | `git -C <main checkout> status --short` shows uncommitted edits inside `frogg3rs-delay-capacity-and-width-finish`'s own directory and to `app/FroggersDspParityTests.cpp`, `app/check_delay_capacity_break_proofs.py`, and `openspec/specs/froggers-sheaf-parameter-model/spec.md` — none of it touches `MANUAL.md`, `QUICK_DICT.md`, `app/FroggersMidiCatalog*`, or `app/Makefile`. | Same disjoint-section reasoning as above; this row is in a different checkout so no working-tree collision is possible from here, but the eventual merge must diff-review `MANUAL.md`/`QUICK_DICT.md` rather than take either side wholesale. |
| **`randomize-depth-reclaim` worktree** | `frogg3rs-delay-capacity-and-width-finish`, `frogg3rs-randomize-depth-reclaim` | None found by name against `MANUAL.md`, `QUICK_DICT.md`, `app/FroggersMidiCatalog*`, `app/Makefile`. | No action. |

Three Sheaf changes matter, read from `External/Sheaf/openspec/changes/`:

| change | state | overlap | disposition |
| --- | --- | --- | --- |
| `midi-controller-resilience` | Its own artifacts carry a repair pass against a preflight adjudication; still **unexecuted** — `declaredPreconditions` exists in no source file yet (`grep -rn declaredPreconditions External/Sheaf/projects/synth/include/ External/Sheaf/projects/synth/src/ app/` returns nothing) and `openspec validate midi-controller-resilience --strict` reports it valid. Adds `declaredPreconditions` to `MidiAppDeviceDefault` (`synth-controller-wizards` requirement scw-6, tasks 6.1-6.2) and renders it on the Controllers page (`synth-runtime-ui` requirement sru-64, task 6.3), AND adds `HeldModifierClearSource`/`HeldModifierState` (`synth-midi-instrument`, group 3, task 3.1) — this change's own task 3.1 manual rewrite depends on that second symbol, not only on `declaredPreconditions`. The sibling worktree its own task 2.4 names no longer exists (see this change's task 1.1), so no divergent copy of `midi-controller-resilience` remains anywhere to reconcile. Its own task 7.7 performs no push, opens no pull request, and moves no pin in this cycle — the operator's later, separate rebase-and-merge step does that, under whatever branch name it uses; until then the commit this change's own task 4.2 pins is reachable from no remote. | This change's group 4 (declared-preconditions population) and task 3.1 (manual rewrite) cannot proceed until both symbols exist here. See task 4.1's gate. | This change populates `declaredPreconditions` for its own device defaults and starts group 4, and task 3.1, only after the submodule checkout confirms both symbols (task 4.1) and the pin is advanced (task 4.2). It does not define either field and does not render `declaredPreconditions`. |
| `app-midi-catalog` | 26/28 done, PR #13 | Owns `MidiAppDeviceDefault` at `projects/synth/include/synth/MidiAppCatalog.hpp:31-38` | Neither this change nor `midi-controller-resilience` redefines the struct; both land above #13. |
| `shift-and-file-export` | All 29 tasks checked (`grep -c '^\- \[x\]' .../shift-and-file-export/tasks.md` → 29, `'^\- \[ \]'` → 0); not yet archived, so still present in `External/Sheaf/openspec/changes/`. Already delivered `ShiftState`/`shift_` and the field's current spelling, `shift_->held`, into the tree both this change and `midi-controller-resilience` build on (`grep -n "struct ShiftState\|shift_ =" External/Sheaf/projects/synth/include/synth/MidiController.hpp` confirms it is live). Owns requirement smi-16, which `midi-controller-resilience`'s own MODIFIED requirements also amend. | This change's design.md and task 4.6 trace `shift_->held` at `MidiController.cpp:964-971` — code this change shipped, not `midi-controller-resilience`'s. `midi-controller-resilience`'s own task 3.1 renames this to `shift_->modifier.held`; design.md and task 4.6 name both spellings for that reason. | No `app/`-level file this table's coordination concern covers (`MANUAL.md`, `QUICK_DICT.md`, `app/FroggersMidiCatalog*`, `app/Makefile`) is touched by `shift-and-file-export`; the only overlap is the citation-spelling one already handled above. |

## Impact

- `app/FroggersMidiCatalog.hpp` — the precondition sentences at `:14-18`
  promoted to declarations on each device default; the surrounding
  button-layout comment at `:19-24` kept but reworded so it no longer restates
  the declared settings and states CC Hold's reason as "the promptest of the
  triggers that end a held modifier" rather than "what ends Shift"; the APC40
  caveat comment at `:26-35` split between the Generic and Ableton defaults;
  the APC40 entries' existing analog defaults at `:165-166` left as they are;
  **the file header comment at `:6-12`**, which enumerates the catalogue's
  "six device defaults" by name, is updated to seven, naming the Launch
  Control XL; a new `LaunchControlXlDeviceDefault()` (`Generic` kind, id
  `froggers.launchcontrolxl`, scene blend on CC 77 channel 8) is appended to
  `catalog.deviceDefaults`'s initializer list after the Ableton default and
  before the Launchpads.
- `app/FroggersMidiCatalogTests.cpp` and `app/FroggersControllersPageTests.cpp`
  — both gain a device_defaults_declare_their_preconditions (or equivalent)
  case, and `FroggersMidiCatalogTests.cpp` gains the rewritten Twister-only
  Shift check and the unshifted-usability cases (ordinary-precondition-unmet
  and Shift-precondition-unmet, stated separately); the Launch Control XL
  default is added to the same declared-preconditions case, and the
  catalogue-count assertions at `app/FroggersMidiCatalogTests.cpp:399`,
  `app/FroggersControllersPageTests.cpp:99`, and
  `app/FroggersControllersPageTests.cpp:109` move from six to seven.
  **`app/FroggersControllersPageTests.cpp`'s own file header comment**
  carries the count twice, at `:1-4` ("six real device defaults") and again
  at `:7` ("so none of them ever drive these six shipping defaults") — both,
  not only the first, are updated to seven, naming the Launch Control XL
  alongside the Twister and the two APC40 variants.
- `app/Makefile` — a new `check-docs-match-device-preconditions` target joins
  the twelve `check-*` prerequisites `test:` already runs (read from the Makefile
  itself, not copied here, since the list drifts). The underlying script is
  authored in two steps: task 1.8 creates it with only the recovery half
  (text-only, run before the manual is rewritten); task 4.8 extends it with
  the drift half (which needs `declaredPreconditions` to compile) once the
  submodule pin lands.
- A new small emitter binary under `app/` (task 4.8), built against
  `app/FroggersMidiCatalog.hpp` the way `app/FroggersMidiCatalogTests.cpp`
  already is, that the drift half runs to read `declaredPreconditions` off
  the compiled catalogue rather than by parsing the header's source text.
- `MANUAL.md:319-320` — the recovery text; `:308-313` (Hold Drill) gets the
  same recovery treatment; `:336-339` — the settings paragraph, which becomes
  generated; `:341-351` and `:353-357` — the APC40 Generic/Ableton sections,
  which the split precondition declarations must stay consistent with; the
  Overview's Preset selector list and a new Launch Control XL section are
  added, naming fader 1 and factory template 1; **`:271-272`**, the
  Overview's "A newly connected Twister, APC40, or Launchpad is also offered"
  sentence, is reworded so it does not enumerate device families where the
  set the configure flow offers is open-ended (a seventh family, the Launch
  Control XL, joins it this cycle; naming families at all invites the same
  drift the catalogue header comments are being fixed for).
- `README.md:110-112` — "ready-made presets for the MIDI Fighter Twister, the
  Akai APC40 mkII, and three Launchpad models" is updated to include the
  Launch Control XL, the same enumeration-drift class as the two header
  comments and `MANUAL.md:271-272` above.
- `openspec/specs/froggers-midi-controller-mappings/spec.md` — the promoted
  capability this change deltas (unedited by this change; updated only when
  archived).
- A new check script under `app/`.

§8.0's sweep covers `app/`, `openspec/`, and the documents named above. It does
not cover `External/Sheaf`, which the Sheaf change sweeps.

## What this change does not do

It adds no analog mechanism, because one exists and is already more first-class
than BPM's. It does not enable analog sections on the Twister or Launchpad
kinds: those are pinned by `synth-midi-instrument`, neither device has a fader,
and changing them would be a Sheaf spec change serving no hardware.
