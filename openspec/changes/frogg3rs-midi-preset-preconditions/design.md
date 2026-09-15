## Context

Device preconditions exist twice, as prose, with nothing joining them:
`app/FroggersMidiCatalog.hpp:14-18` for the reader of the catalogue, and
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

No cataloged device can carry either today.
`synth-midi-instrument` pins kind support: "twister: encoders, system messages;
launchpad: system messages only; generic: all sections". The Twister and all
three Launchpads therefore refuse an analog section by specification, and
neither has a fader to put one on. The Launch Control XL, which has eight, is
not in the catalogue.

**The field this change populates does not exist yet.** `declaredPreconditions`
is added to `MidiAppDeviceDefault` by Sheaf's `midi-controller-resilience`
change (`synth-controller-wizards` requirement scw-6, tasks 6.1-6.2), whose
own artifacts still carry an unexecuted, operator-owned precondition of their
own (a divergent duplicate of that change in a sibling worktree — see Risks,
below, and proposal.md's overlap table) and are themselves still unexecuted:
`grep -rn declaredPreconditions
External/Sheaf/projects/synth/include/ External/Sheaf/projects/synth/src/
app/` finds nothing anywhere in either repository, and
`openspec validate midi-controller-resilience --strict` (run from
`External/Sheaf`) reports it valid without that meaning it has run. An
executor who reaches
this change's population tasks before that field exists gets a compile error.
Group 4 therefore opens with a gate (task 4.1) and the mechanical step that
closes it — advancing the submodule pin (task 4.2) — as its own numbered task,
because no other task in this change performs it.

**Sequencing, stated exactly.** Sheaf executes its own tasks 6.1 (the field)
and 6.2 (threading it through `ControllerWizardDescriptor` and
`MakeControllerWizardRegistry`) on its branch `midi-resilience-merge`, then its
own task 7.7 pushes that branch to the `fork` remote (`daguilarc/Sheaf`) and
does nothing else. "Sheaf has landed" for this change means exactly one
thing: this worktree's `External/Sheaf` submodule checkout is pinned to a
commit reachable from `fork/midi-resilience-merge` that carries the field —
verified by `grep -n declaredPreconditions
External/Sheaf/projects/synth/include/synth/MidiAppCatalog.hpp` returning a
real member, not by any claim about upstream, a pull request, or `main` on
either repository. Task 4.2 performs that pin advance; task 4.1 is the gate
that says group 4 does not start before it.

## Goals / Non-Goals

**Goals:**
- A preset's device preconditions are one declaration that the catalogue, the
  Controllers page and the manual all read, with drift caught by a check.
- The manual's stuck-modifier recovery describes a recovery that exists, for
  both Shift and Hold Drill, and the Twister's side buttons stay usable in
  their unshifted form even when CC Hold is unmet.
- Scene blend is reachable on a fader beyond the APC40 mkII: the Launch
  Control XL preset ships this cycle, scene blend on fader 1 (CC 77, channel
  8 counted from 0).

**Non-Goals:**
- Any analog mechanism. One exists, and it is more direct for scene blend than
  for BPM.
- Enabling analog sections on the Twister or Launchpad kinds. Spec-pinned,
  and neither device has a fader.
- Held-modifier lifetime, mismatch reporting and template-change handling, which
  are Sheaf's `midi-controller-resilience`.
- Measuring the Launch Control XL's fader CC/channel map on hardware. No
  Launch Control XL is attached to this machine (see proposal.md); the map is
  read from Ableton Live 12 Suite's own control-surface script for the device
  instead, per the operator's ruling.

## Decisions

**Preconditions are declared data on the device default, and the manual is
generated from them.** A comment cannot be checked. Putting the declaration in
`MidiAppDeviceDefault`'s `declaredPreconditions` — the field Sheaf's
`midi-controller-resilience` change adds via its `synth-controller-wizards`
requirement scw-6 — makes the catalogue the single source, lets the Controllers
page render it where the preset is offered (Sheaf's own `synth-runtime-ui`
requirement sru-64), and makes the manual a rendering rather than a second
assertion. The check compares the generated manual paragraph against the
declarations and fails on divergence — a gap this catalogue has not had before,
and whose absence cost the operator three side buttons.

**The APC40 Generic and Ableton defaults declare different preconditions,
because they have different ones.** Traced at `app/FroggersMidiCatalog.hpp:26-35`:
the Generic default requires Track 1 to stay selected, because the unit's eight
device knobs follow whichever Track Select button is lit. The Ableton default
"exists to avoid that caveat: it opens with a connect-time message that keeps
the unit's sixteen knobs on channel 0 regardless of which track is selected" —
and that message is sent by the app itself, automatically, via
`device.config.openSysEx` (`app/FroggersMidiCatalog.hpp:186`), not by anything
the operator sets on the device. `MANUAL.md`'s Ableton section
(`:353-357`) names no manual precondition either. So the Generic default
declares the Track 1 caveat, and the Ableton default declares an **empty**
list — which is worth doing explicitly, since it is the case scw-6's own third
scenario (a descriptor with no declared preconditions) exercises. Writing a
false Track-1 precondition onto the Ableton default — the one that exists
specifically to remove it — would be the exact WYSIWYG failure this pair of
changes exists to fix, introduced by the fix.

**Every side button's press keeps dispatching a job when CC Hold is unmet —
but which job depends on whether the unmet button is Shift itself.** Traced
at `External/Sheaf/projects/synth/src/MidiController.cpp:932-976`
(`SystemButtonMidiInProcessor::Process`): an ordinary or shifted press message
is dispatched (`PushStamped`) on the press edge — `isPress`, `midi.GetValue() >
0` — before the function ever inspects `association->release`. A button whose
own CC Hold is unmet on the device (so no matching release ever arrives, or
the addresses move) still fires a job on every press; nothing about dispatch
depends on that button's own release ever arriving.

That job is not always the button's *unshifted* one. `:964-971` handles a
press whose `association->press.type == MessageIn::Type::Shift` by setting
`shift_->modifier.held = isPress` and returning — no message is pushed for
Shift's own press. If Shift's own CC Hold is the precondition that is unmet,
its release never arrives, so `held` is set `true` on the press and never
cleared by a release; every subsequent press on the other five side buttons
then reads `:975`'s `shifted = shift_ != nullptr && shift_->held &&
association->shiftedPress.has_value()` as `true` and fires its **shifted**
job, not its ordinary one, until `held` is cleared by one of the other four
`HeldModifierClearSource` triggers (Rebuild, EndpointOpen, SecondPress on
Shift's own address, or Ceiling) — not by anything named "unshifted." Only
when the unmet precondition is on one of the *other* five buttons does that
button's own press keep firing its ordinary-or-shifted job exactly as Shift's
current state already dictates, indistinguishable from a button whose
precondition holds.

So the guarantee this change can actually state and check is: a side
button's press always fires *some* job (never silently drops), regardless of
whether that button's own CC Hold precondition is met. It is not that the
catalogue's five non-Shift buttons "remain usable in their unshifted form"
when Shift's own precondition is the one that is unmet — that case leaves
them shifted, recoverable only by the same triggers `MANUAL.md`'s rewritten
recovery text names. The requirement sentence and the scenario are worded to
the guarantee the code has, with the Shift-unmet case stated as its own
scenario rather than folded into a disjunction that would be satisfied either
way.

**"Only the Twister carries Shift or shifted jobs" needs a check that actually
tests every device, not three named ones.** The existing
`device_defaults_are_valid_and_address_exactly_the_documented_controls` case
only negates `shiftedPress`/`Type::Shift` for the Twister's own buttons, a
`{&generic, &ableton}` loop, and a separate Launchpad loop
(`app/FroggersMidiCatalogTests.cpp:510`, `:587-596`). A future seventh default
falls into none of them. The check this change adds is one loop over
`catalog.deviceDefaults` asserting, for every entry whose id is not the
Twister's, that no association carries a `shiftedPress` and no
`press.type == MessageIn::Type::Shift` — generic over however many device
defaults the catalogue holds, now or later, with the positive control that
giving any non-Twister default a shifted press turns it red. This does not
depend on group 4's gate: it can run against today's six device defaults
immediately.

**The manual's recovery is rewritten to the triggers, not to a new promise.**
`MANUAL.md:319-320` currently says a stuck Shift clears when Shift is pressed and
released again. That is unreachable exactly when it is needed — the failure that
strands a profile is the Shift address ceasing to transmit. The replacement
states the triggers Sheaf's change's `HeldModifierClearSource` enumerates and
says which are available on which host, and covers Hold Drill, whose failure
costs every encoder.

**The drift check has two independent halves — declarations-vs-manual, and
recovery-text — and each needs its own rule, floor and positive control.**
Folding them into one paragraph is what let the first version of this check
ship with neither written down; both are stated here before task 4.8 writes
the script, so the script has an assertion to implement rather than a blank
to fill.

**Drift half — correspondence rule.** For every entry in
`catalog.deviceDefaults` (`FroggersMidiCatalog()`'s initializer list,
currently seven: Twister, APC40 Generic, APC40 Ableton, Launch Control XL,
Launchpad X, Launchpad Pro MK3, Launchpad Mini MK3), the check reads that
device's `declaredPreconditions` list directly out of `app/FroggersMidiCatalog.hpp`'s
source — parsed by function name (`TwisterDeviceDefault()`,
`Apc40GenericDeviceDefault()`, `Apc40AbletonDeviceDefault()`,
`LaunchControlXlDeviceDefault()`, and the three Launchpad factories), the same
symbol-not-line-number discipline `check_docs_match_parameter_table.py`
already uses for `FroggersBankLayouts()` — and compares it against that
device's own subsection of `MANUAL.md`'s "MIDI controllers" section, delimited
by that device's own `###` heading (`### MIDI Fighter Twister`, `### Akai
APC40 mkII (Generic)`, `### Akai APC40 mkII (Ableton)`, `### Novation Launch
Control XL`, and the three Launchpad headings) up to the next `###` heading or
`---`. "Agree" means: every string in `declaredPreconditions` names a setting
(the substring before the first comma, e.g. "encoders set to relative") that
appears, case-insensitively, somewhere in that device's subsection prose; and
the subsection names no device-setting-shaped clause (a sentence containing
"must", "set to", "unchecked", "stay selected", or "Utility" outside the
device's own name) that does not correspond to a string in
`declaredPreconditions`. An empty `declaredPreconditions` list (Ableton, the
three Launchpads) agrees with a subsection that names none of those clause
markers.

**Drift half — floor.** The check must locate exactly seven device
subsections and seven `declaredPreconditions` lists before comparing
anything; if either parse finds fewer than `catalog.deviceDefaults.size()`
(read the same way `app/FroggersMidiCatalogTests.cpp`'s own count assertion
does, not hard-coded), the check fails outright with that count, rather than
reporting agreement over whatever subset it happened to find — a regex that
matches nothing must not read as "nothing disagreed."

**Drift half — positive controls, both directions.** (a) Declaration-edit
direction: temporarily append a word to the Twister's `declaredPreconditions`
entry for CC Hold (e.g. `"CC Hold, permanently"` in place of `"CC Hold"`) and
confirm the check turns red, because `MANUAL.md`'s Twister subsection no
longer contains that exact setting text. (b) Manual-edit direction:
temporarily edit `MANUAL.md`'s Twister subsection to say "all six side
buttons set to CC Toggle" in place of "CC Hold" and confirm the check turns
red, because the source's declared string no longer appears in the edited
prose. Both edits are reverted immediately after confirming red; neither
ships.

**Recovery half — rule.** The check parses the `### Shift` and `### Hold
Drill` subsections the same way (heading-delimited) and asserts each contains
at least one of a fixed set of multi-word recovery phrases that this change's
own rewrite (task 3.1) introduces — naming the specific action a phrase like
"selecting a different preset", "rebuilds the row's mapping",
"unplugging and reconnecting the controller", or "clears automatically after"
— and fails when none of those phrases is present. The set is multi-word and
names an action, not a bare noun: a single word like "unplugged" is not
sufficient, because the **current, unmodified** text already contains that
word as part of describing the *failure* ("If the controller is unplugged
while Shift is still held, its buttons stay shifted..."), not as a stated
recovery — a bare-word check would pass the very text this check exists to
reject. Requiring the specific action phrases task 3.1's rewrite actually uses
avoids that false pass; task 3.1's rewrite must use at least one of them
verbatim so the check and the prose agree by construction, not by luck.

**Recovery half — floor.** If the `### Shift` or `### Hold Drill` heading is
not found at all, the check fails naming the missing heading, rather than
vacuously passing an empty comparison.

**Recovery half — positive control, satisfiable against today's text.** The
control is running the check against the **current, unmodified**
`MANUAL.md:319-320` — "buttons stay shifted until Shift is pressed and
released again" — before task 3.1 touches it. That text contains none of the
recovery phrase set above (it contains the bare word "unplugged", which the
rule above deliberately does not accept alone, and "pressed and released
again", which is not in the set because it is exactly the mechanism that is
unavailable when the recovery is needed): the check must fail against it.
`MANUAL.md:308-313`'s Hold Drill subsection carries no recovery sentence at
all today — task 3.1 writes one — so the same control against Hold Drill
fails for the floor reason (heading found, no recovery phrase present) rather
than vacuously passing an absent paragraph. If either control does not turn
red, the check tests nothing, exactly as it did in the version this change
replaces (a false-positive check committed against
`check_spec_checks_resolve`'s check-name resolution rather than its
behaviour).

**The undeclared-dependency SHALL ships unchecked this cycle, stated as
such.** `spec.md`'s "A preset SHALL NOT depend on a device setting it does
not declare" cannot be verified by comparing generated text against
declarations — both sides are written from the same source, so a dependency
absent from both passes trivially. Checking it for real would mean tracing
every `MidiControllerSystemMessageAssociation`/`AnalogMidiInConfig` field a
device default populates back to a specific device-side setting and
confirming each one that matters is named in `declaredPreconditions` —
static analysis this repository has no mechanism for and this change does not
build one. It ships asserted by the same by-hand tracing task 4.3/4.4/5.2 use
to write each device's declarations from its own precondition prose in the
first place, not by a check that can fail. A future check would need to
enumerate, per device kind, which config fields correspond to a real
device-side setting.

**The Launch Control XL is catalogued as `Generic`, not as a new kind, and its
data is read from a third-party script.** `Generic` already supports all
sections by the pinned spec, so no Sheaf spec change is needed and no new kind
has to be threaded through validity, the wizard, or the page. The alternative
— a dedicated kind — would buy a tighter default at the cost of a Sheaf spec
change and a new enumerator in a family that three consumers switch over. That
decision stands independent of the map's source, below.

**The template is a declared precondition, and the CC map is read from
Ableton Live's control-surface script, not a vendor document.** The device's
control map moves with its template, so a map written without pinning the
template would assert a layout the next template change invalidates — the
template is a declared precondition the same way the Twister's Utility
settings are.

Neither vendor document states the map. Task 2.1 fetched and read Novation's
published *Launch Control XL Programmer's Reference Guide*, Version 2
(https://fael-downloads-prod.focusrite.com/customer/prod/downloads/launch_control_xl_programmer_s_reference_guide.pdf,
9 pages) and the companion *Getting Started Guide*
(https://fael-downloads-prod.focusrite.com/customer/prod/s3fs-public/downloads/Launch%20Control%20XL%20GSG%20v2.pdf,
7 pages) in full. The Programmer's Reference Guide documents the System
Exclusive `Change current template` message (`F0h 00h 20h 29h 02h 11h 77h
Template F7h`) and states, in "Launch Control XL MIDI Overview": "Launch
Control XL has 16 templates: 8 user templates, which can be modified, and 8
factory templates, which cannot. User templates occupy slots 00h-07h (0-7),
whereas factory templates occupy slots 08-0Fh (8-15)" — but its only indexed
control lists ("Set LEDs" / "Toggle button states") number knobs and buttons
for LED addressing, not faders, and carry no per-template CC values for
anything. The Getting Started Guide states the same 8-factory-templates fact
and, in "Template Switching and the Template Editor" → "Template Switching"
(page 5), gives the manual procedure — "To switch template press and hold
either the User or Factory template buttons. The bottom row of pads then will
light up, with the selected template brightly lit. Press pads 1-8 to select
template 1-8" — but neither document enumerates a single CC number for any
control. This finding stands.

Per the operator's ruling for this cycle, the map is read instead from the one
source on this Mac that encodes it: Ableton Live 12 Suite's own
control-surface script for the device, at
`/Applications/Ableton Live 12 Suite.app/Contents/App-Resources/MIDI Remote
Scripts/Launch_Control_XL/LaunchControlXL.pyc` (Ableton Live 12.4.3, read from
the application's own `Info.plist`, `CFBundleShortVersionString`). This is
evidence from a third-party control-surface script, not a vendor document,
because neither vendor document states a map at all (both URLs above); it is
asserted here because the operator ruled the preset ships this cycle
regardless.

The script's module-level constants `PREFIX_TEMPLATE_SYSEX` and
`LIVE_TEMPLATE_SYSEX` build exactly the `Change current template` message the
Programmer's Reference Guide documents: `F0 00 20 29 02 11 77 08 F7` —
template byte 8, the first of the 8 factory templates confirmed above — so the
script selects **factory template 1**. Its `LIVE_CHANNEL` constant is 8; the
Programmer's Reference Guide's "Device-to-Computer messages" section states
the device's own channel numbering is zero-indexed ("Buttons can output
either note messages or CC messages on a zero-indexed MIDI channel n"), which
is the corroboration that `LIVE_CHANNEL = 8` means channel 9 counted from 1,
not channel 8. The script's slider factory constructs each fader as a
`SliderElement` on `LIVE_CHANNEL`, and its fader list comprehension assigns CC
`77 + i` for `i` in `0..7` — CC 77 to CC 84, one per fader.

**Who read the script, by what command, and what it printed.** This is not a
vendor document, so the reading itself is the evidence and is recorded as
such rather than asserted as an already-known fact. The lead read
`LaunchControlXL.pyc` by disassembly, on this machine, on 2026-09-14 — not
this change's executor, who does not open anything under `/Applications`
(Non-Goals, above; see also the confirmation this reading still needs, task
5.4). The command and its literal output:

```
$ cd "/Applications/Ableton Live 12 Suite.app/Contents/App-Resources/MIDI Remote Scripts/Launch_Control_XL" && ~/.local/bin/python3.11 -c 'import marshal,dis; c=marshal.loads(open("LaunchControlXL.pyc","rb").read()[16:]); [print(f"{x.offset:4} {x.opname:12} {x.argrepr}") for x in dis.get_instructions(c) if x.opname in ("LOAD_CONST","LOAD_NAME","STORE_NAME","BUILD_TUPLE","BINARY_OP")]'
 222 LOAD_CONST     8
 224 STORE_NAME     LIVE_CHANNEL
 226 LOAD_CONST     (240, 0, 32, 41, 2, 17, 119)
 228 STORE_NAME     PREFIX_TEMPLATE_SYSEX
 230 LOAD_NAME      PREFIX_TEMPLATE_SYSEX
 232 LOAD_NAME      LIVE_CHANNEL
 234 LOAD_CONST     247
 236 BUILD_TUPLE
 238 BINARY_OP      +
 242 STORE_NAME     LIVE_TEMPLATE_SYSEX
```

and, from disassembling `make_slider` (script line 88) and the fader list
comprehension (script line 101): `SliderElement(MIDI_CC_TYPE, LIVE_CHANNEL,
identifier, name=...)` with identifiers `77 + i` for `i in range(8)`. The
`.pyc`'s bytecode gives `LIVE_CHANNEL = 8` and the template byte `8` directly;
the two paragraphs above corroborate what those two integers *mean*
(channel 9 counted from 1; the first of the 8 factory templates) against the
vendor's own Programmer's Reference Guide, which states the meaning of a
zero-indexed channel and of template slots 08h-0Fh but not the values
themselves. The CC numbers 77-84 rest on the disassembly alone — no vendor
document states them at all, factory-template or otherwise.

**What task 5.1's own check can and cannot prove.** The
`device_defaults_declare_their_preconditions`/analog-section case task 5.1
adds is a literal-against-literal assertion: it reads the CC/channel/template
values this change writes into `LaunchControlXlDeviceDefault()` and asserts
they equal the numbers stated here. It can catch a later transcription slip —
someone editing the catalogue entry to a different CC without updating this
citation — but it cannot catch a wrong reading of the device: if the
disassembly above is itself mistaken about what the hardware does, the check
passes anyway, because both sides of the comparison come from this same
reading. The CC/channel/template numbers this preset ships with rest on
exactly two things: the one disassembly recorded above, and the operator's
post-delivery hardware check (task 5.4) — nothing else in this change's
verification surface can reject a wrong value.

**Fader 1 (CC 77) carries scene blend.** The eight faders are electrically
identical and undistinguished by function in every source read for this
change — Ableton's own script and both vendor documents alike — so the first
(leftmost, lowest CC) is chosen rather than an arbitrary pick from the middle
of the row, the same "first distinguished control" convention already used
for the Twister's own paired side buttons. The template is declared as a
device precondition, "factory template 1," citing the Getting Started Guide's
"Template Switching" procedure above as where an operator sets it.

**The device default's `id` is `froggers.launchcontrolxl`.** Every existing
`id` is formed the same way — `"froggers."` plus the device name, lowercased
and stripped of spaces, with a `.`-separated variant suffix only where more
than one default shares a device name:

```
$ grep -n 'device.id = "\|"froggers\.launchpad' app/FroggersMidiCatalog.hpp
92:    device.id = "froggers.twister";
173:    device.id = "froggers.apc40.generic";
184:    device.id = "froggers.apc40.ableton";
266:    return LaunchpadDeviceDefault(synth::LaunchpadController::LaunchpadX, "froggers.launchpad.x", "Launchpad X",
276:        synth::LaunchpadController::LaunchpadProMk3, "froggers.launchpad.promk3", "Launchpad Pro MK3",
286:        synth::LaunchpadController::LaunchpadMiniMk3, "froggers.launchpad.minimk3", "Launchpad Mini MK3",
```

The convention alone underdetermines the literal token: `froggers.lcxl`,
`froggers.launchcontrol.xl` and `froggers.launchcontrolxl` are all consistent
with "lowercase the device name." The Twister is this catalogue's only other
single-variant device, and its id spells the device name in full with no
abbreviation and no dot (`froggers.twister`, not `froggers.tw` or
`froggers.mf.twister`); the dotted forms (`froggers.apc40.generic`,
`froggers.launchpad.x`) all carry a variant *after* the dot, distinguishing
multiple defaults for the same physical device family. The Launch Control XL
has exactly one default, the same shape as the Twister, not a variant family
— so `froggers.launchcontrolxl` follows the single-variant precedent, and
`froggers.launchcontrol.xl` is rejected because the dot would misread this as
a variant of a "Launch Control" family that does not exist, and
`froggers.lcxl` is rejected because no existing id abbreviates a device
name (`apc40` keeps the model number, `launchpad`/`twister` are spelled in
full).

**Hardware confirmation is a post-delivery operator check.** The Controllers
page cannot be driven from a unit test that proves a physical fader sends a
particular CC — that is a fact about the hardware, not the code, and no
Launch Control XL is attached to this machine (`ioreg -p IOUSB -w 0` lists
only a USB Hub, a Portable SSD T5, and a USB-to-DP/HDMI adapter). What the
code path can be shown to do, and what an operator can observe, is: moving
fader 1 changes the on-screen scene blend value, via
`AnalogMidiInProcessor::Process` (`External/Sheaf/projects/synth/src/MidiController.cpp:847`)
matching the incoming address against `AnalogMidiInConfig::sceneBlend` and
dispatching `MessageIn::SetSceneBlend`, rendered by `FroggersUiSurface.hpp`'s
read of `context_->uiState->sceneBlend` into the `FroggersNodeIds::kSceneBlend`
slider — the same path the APC40 crossfader already exercises in production
today. Task 5.4 states this as an operator step naming that observable and
that code path, not as an automated Check line, because nothing in this
repository can drive a real fader.

## Risks / Trade-offs

- **The manual is held by other active changes.** Task 1.2 brings this branch
  up to `main`, which is expected to remove `frogg3rs-delay-width-wysiwyg-repair`
  from this worktree entirely (already deleted on `main`, superseded as
  `frogg3rs-delay-capacity-and-width-finish`) — confirm with task 3.2's own
  check before relying on it. After that, `frogg3rs-delay-capacity-and-width-finish`
  (main checkout, task count re-measured, not frozen — see task 3.2) and
  `frogg3rs-density-documents-and-spec` (`midi-controller-resilience` worktree,
  0/14) hold `MANUAL.md`/`QUICK_DICT.md`'s Delay and Filter entries
  respectively. → Both are disjoint sections from this change's MIDI
  controllers section (`MANUAL.md:258-390`); diff-review before staging and
  never stage a whole-file `git add` while either is active.
- **Two divergent duplicates exist in a sibling worktree.** Untracked copies
  of `frogg3rs-midi-preset-preconditions` and, inside its own `External/Sheaf`
  checkout, `midi-controller-resilience` both sit in the
  `.claude/worktrees/midi-controller-resilience` worktree, each deltaing the
  same capability with different text than this worktree's copies. Re-verified
  directly during this repair pass — both are still live, not already removed
  by anyone. → This worktree's copies are authoritative; task 1.1 makes both
  duplicates' removal or reconciliation an operator precondition that must be
  satisfied before task 1.2 runs, since neither path is writable from this
  session's sandbox.
- **`declaredPreconditions` belongs to Sheaf's change and does not exist yet.**
  → Group 4 gates on the submodule pin landing a commit that carries it
  (tasks 4.1-4.2); it does not define the field and does not render it — the
  render is Sheaf's own `synth-runtime-ui` requirement sru-64.
- **A generated manual paragraph can flatten prose.** → Only the settings
  paragraph is generated; the surrounding explanation stays hand-written.
- **The LCXL map's source is a third-party control-surface script, not a
  vendor document.** → Both vendor documents were read in full and state no
  map (see above); Ableton Live 12 Suite's script is cited by path and Live
  version, and the post-delivery operator check on hardware (task 5.4) is
  what confirms it in practice.
- **A Generic-kind entry gives a looser default than a dedicated kind.** →
  Accepted; the tighter option costs a Sheaf spec change for no behaviour the
  operator asked for.

## Migration Plan

Additive. `declaredPreconditions` carries an empty default, so a preset that
declares nothing behaves as today. Group 4 does not run until the submodule
pin lands the field (tasks 4.1-4.2); until then this change's only executable
work is the manual rewrite (group 3) and the two device-generic checks that do
not depend on the field (the Twister-only-Shift rewrite and the
unshifted-usability case). Group 5's Launch Control XL default also waits on
that same pin (its declared-preconditions population, task 5.2). The Launch
Control XL device default adds a catalogue entry and changes no existing one.
Patches store mappings, not device defaults, so a patch saved before this
change loads unchanged after it.

## Open Questions

None held by this change's own execution path. The Launch Control XL's fader
CC/channel map is answered by Ableton Live 12 Suite's control-surface script
(see above); confirming that map on a physical device remains a post-delivery
operator step (task 5.4), not a question this change's artifacts can resolve
further.
