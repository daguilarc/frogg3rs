## Context

Device preconditions exist twice, as prose, with nothing joining them:
`app/FroggersMidiCatalog.hpp:14-19` for the reader of the catalogue, and
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
own artifacts are themselves still unexecuted:
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

**Sequencing, stated exactly.** Sheaf executes its own `midi-controller-resilience`
change to completion, through its own task 7.9, on its branch
`midi-resilience-merge` — the same submodule checkout this worktree already
carries. That includes tasks 6.1 (the field) and 6.2 (threading it through
`ControllerWizardDescriptor` and `MakeControllerWizardRegistry`), and group
3's task 3.1 (adds `HeldModifierClearSource`, which this change's own task
3.1 needs to name the manual's recoveries) — but "Sheaf has landed" means the
whole change is done, not just those three tasks, because the object this
change confirms against and pins is the commit Sheaf's own task 7.8 makes,
and that commit is only made once 7.1-7.7 have been reported (7.7 itself may
be reported *blocked* rather than complete — it depends on two other Sheaf
changes archiving first, which is outside either change's control this cycle
— see "Overlapping active changes" in proposal.md). Sheaf's own task 7.9
states that this change performs no push, opens no pull request, and moves
no pin in this cycle; the operator's later, separate rebase-and-merge step
pushes that work (or a rebased equivalent) to a remote, under whatever
branch name that step uses.

"Sheaf has landed" for this change therefore means, checked locally, never
against a remote: (1) `git -C External/Sheaf status --short` is empty and
`git -C External/Sheaf rev-parse HEAD` names the exact commit Sheaf's own
task 7.8 made, per that task's own stated tick convention; and (2) the Sheaf
test binaries that carry `declaredPreconditions` and `HeldModifierClearSource`
(Ceiling and EndpointOpen, on both the browser and the runtime binding) build
and pass, by name, at that commit — task 4.1 names the exact binaries and
cases. A declaration a comment could also spell, or a box ticked without the
work behind it, is not what this checks: a real test binary either builds and
its named case passes, or it does not, and neither outcome is satisfied by
text. None
of this is a claim about upstream, a pull request, or `main` on either
repository — it is a property of this local checkout's own object store.
Task 4.2 performs the pin advance from that same local checkout, not a
fetch, and pins exactly the commit task 4.1 confirmed — the coordinator's
task 7.8 commit, named by its SHA once confirmed, not whatever `HEAD`
happens to be at some earlier, partial point in Sheaf's own execution. Task
4.1 is the gate that says group 4 — and, under the same confirmation, task
3.1's manual rewrite — does not start before this. Because Sheaf executes to
completion before this change's group 4 begins, task 4.2's checkout of that
commit detaches nothing Sheaf still needs: no Sheaf task after 7.8 touches
that checkout, so the detach costs nothing this cycle (Risks, below, states
the general consequence for the record). The commit task 4.2 pins is, by
the same fact, reachable from no remote until the operator's later step
publishes it, and until then it exists in exactly one object store on this
machine — this worktree's own submodule store — which no step before that
publication may destroy (see Risks and Migration Plan below); that does not
change what "landed" means here, which is a property of this local
checkout, not of any remote.

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
`shift_->held = isPress` and returning — no message is pushed for
Shift's own press (`shift_` is a private member; Sheaf's group 3 later
composes a `HeldModifierState modifier;` into `ShiftState` and renames every
access to `shift_->modifier.held`, but the type name `ShiftState` itself is
unchanged, and no case this change adds constructs a
`SystemButtonMidiInProcessor` and reads that private field under either
spelling — see task 4.6). If Sheaf's group 3 has already landed by the time a
reader traces this paragraph, read `shift_->modifier.held` for `shift_->held`
throughout; the dispatch this paragraph describes is the same either way. If Shift's own CC Hold is the precondition that is unmet,
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
negates `shiftedPress` for the Twister's own buttons and for a
`{&generic, &ableton}` loop (`app/FroggersMidiCatalogTests.cpp:510`); a
separate case, `launchpad_defaults_positions_carry_their_own_controller`,
negates it again for the Launchpad loop (`:596`). `Type::Shift` itself is
asserted only positively, for the Twister's own Shift button (`:471`, `:474`)
— no case anywhere negates it, since the guarantee "no other device has a
Shift press" was never checked directly, only implied by `shiftedPress`'s own
absence. A future seventh default falls into none of these three coverage
sites. The check this change adds is one loop over
`catalog.deviceDefaults` asserting, for every entry whose id is not the
Twister's, that no association carries a `shiftedPress` and no
`press.type == MessageIn::Type::Shift` — generic over however many device
defaults the catalogue holds, now or later, with the positive control that
giving any non-Twister default a shifted press turns it red. This does not
depend on group 4's gate: it can run against today's six device defaults
immediately.

**The seventh default's exercise of the Add/Block and wizard path is
measured, not assumed.** Both `real_catalog_registers_one_descriptor_per_device_default`
and `real_catalog_defaults_generate_and_accept_adds_through_the_view_model`
(`app/FroggersControllersPageTests.cpp`) iterate `catalog.deviceDefaults`/
`registry` by their live size, so task 5.1's Generic, analog-only default
reaches both without an edit to either case. Built and run by path in a
detached worktree at this branch's own tip (`$M`), with `External/Sheaf`
checked out at the same commit this worktree's own gitlink pins, both
binaries deleted first, and only the seventh default added to
`app/FroggersMidiCatalog.hpp` (`git diff --stat` showed that one addition and
nothing else):

```
$ rm -f "$M/app/build/froggers_controllers_page_tests" "$M/app/build/froggers_midi_catalog_tests"
$ nice make -C "$M/app" -j2 "$M/app/build/froggers_controllers_page_tests" "$M/app/build/froggers_midi_catalog_tests"
$ "$M/app/build/froggers_controllers_page_tests"
[FAIL] real_catalog_registers_one_descriptor_per_device_default: /private/tmp/.../app/FroggersControllersPageTests.cpp:99 requirement failed: catalog.deviceDefaults.size() == 6
[PASS] real_catalog_defaults_generate_and_accept_adds_through_the_view_model
[PASS] twister_system_rows_carry_shift_editable_field_and_derived_choice_index
[PASS] launchpad_presets_pair_with_the_port_names_a_host_reports
EXIT=1
$ "$M/app/build/froggers_midi_catalog_tests"
[PASS] midi_app_action_walk_moves_the_state_the_screen_moves
[PASS] midi_encoder_push_drills_like_the_screen_press
[PASS] catalog_names_every_front_screen_action
[FAIL] device_defaults_are_valid_and_address_exactly_the_documented_controls: /private/tmp/.../app/FroggersMidiCatalogTests.cpp:399 requirement failed: catalog.deviceDefaults.size() == 6
[PASS] launchpad_defaults_open_sysex_is_programmer_mode
[PASS] launchpad_defaults_positions_carry_their_own_controller
[PASS] launchpad_defaults_pad_actions_resolve_against_the_catalog
[PASS] launchpad_defaults_bank_column_covers_every_bank
[PASS] launchpad_defaults_are_registered_with_expected_ids_and_kind
EXIT=1
```

The file paths in both `[FAIL]` lines print absolute, elided here as printed
(`REQUIRE_TRUE`, `app/FroggersMidiCatalogTests.cpp:72-79`, embeds `__FILE__`;
`app/Makefile` compiles every test source by its own absolute path, with no
prefix-map flag to shorten it) — a relative-looking path or a bare basename
in this block would not be this binary's own output. With the default
reverted and both binaries rebuilt from a clean baseline, both exit `0` (all
four `froggers_controllers_page_tests` cases and all nine
`froggers_midi_catalog_tests` cases `[PASS]`) — the two failures above are
attributable to the added default alone. The only failures are the two count
assertions task 5.5 already renames (`FroggersControllersPageTests.cpp:99`,
`FroggersMidiCatalogTests.cpp:399`; `:99`'s own `REQUIRE_TRUE` throws before
`:109`'s `registry.size() == 6` is ever reached, so that third assertion is
unexecuted in this run, not separately confirmed failing — task 5.5 renames
it on the same reasoning as the other two, since it reads the same `6`).
`real_catalog_defaults_generate_and_accept_adds_through_the_view_model`
passed with the seventh default present, driving it through
`MakeControllerWizard`, `GenerateCatalogSlots`, `ConfigForm`/`GenerateProfile`,
`AddController`, and every `AddSingle`/`AddBlock` call across the
Encoders/SystemMessages/Analogs sections — an analog-only `Generic` default
is accepted throughout, including its one real control (`AnalogGesture`,
`sceneBlend` at channel 8 / CC 77): `KindSupport(Generic)` supports all three
groups, so the Encoders/SystemMessages sections this default starts empty are
filled by `AddSingle`/`AddBlock` from scratch, with no refusal.
`launchpad_presets_pair_with_the_port_names_a_host_reports` also passed with
the seventh default present, exercising the catalogue-derived `registry` its
own case builds, though (task 5.5) it asserts against a local `kPortCount`
literal, not the catalogue's own size, so it needs no rename. So the
enumeration gap this paragraph closes is a citation gap, not a behavioural
one: nothing in the Add/Block or wizard path needed a change for the seventh
default, and task 5.5 names all three sites (the two count-bearing cases
above and `GenerateCatalogSlots`, which the first drives) the seventh default
exercises, so a future reader does not have to re-derive this measurement to
know they are covered. This measurement is not repeated at execution time;
task 5.5's own rename and this design's record of what passed and what failed
stand as its evidence.

**The manual's recovery is rewritten to the triggers, not to a new promise.**
`MANUAL.md:319-320` currently says a stuck Shift clears when Shift is pressed and
released again. That is unreachable exactly when it is needed — the failure that
strands a profile is the Shift address ceasing to transmit. The replacement
states the triggers Sheaf's change's `HeldModifierClearSource` enumerates and
says which are available on which host, and covers Hold Drill, whose failure
costs every encoder.

**The drift check has two independent halves — declarations-vs-manual, and
recovery-text — with different dependencies, so they are authored and run at
different points, not folded into one task after the manual is already
rewritten.** The recovery half is pure text over `MANUAL.md`; it has no
dependency on Sheaf's field, so task 1.8 authors and runs it before task 3.1
touches anything. The drift half reads `MidiAppDeviceDefault::declaredPreconditions`,
which does not compile until the submodule pin lands it (tasks 4.1-4.2), and
compares against text task 4.7 generates; it is authored in task 4.8, after
both of those. Folding the two into one paragraph, and one task, is what let
an earlier version of this check ship with neither its rule nor its ordering
written down.

**Drift half — read the catalogue through the compiler, not through a text
parse of `FroggersMidiCatalog.hpp`.** Three earlier designs of this half each
parsed the header's source text — by device-setting substring, by function
name, by marker word — and each is porous in a way tracing this file exposes
directly:

- `Apc40AbletonDeviceDefault()` copy-constructs its return value from
  `Apc40GenericDeviceDefault()` and only overwrites `id`, `displayName`, and
  `config.openSysEx` (`app/FroggersMidiCatalog.hpp:182-187`); it never
  restates `declaredPreconditions` in its own body. A parse keyed to a
  function's own source text reads the Generic's declarations for both, and
  cannot see that the Ableton default's true list is empty.
- The three Launchpad factories (`LaunchpadXDeviceDefault()`,
  `LaunchpadProMk3DeviceDefault()`, `LaunchpadMiniMk3DeviceDefault()`,
  `app/FroggersMidiCatalog.hpp:265-292`) each call the shared
  `LaunchpadDeviceDefault()` helper, which takes no preconditions parameter at
  all. A by-function-name parse has no per-device text to read for three of
  seven devices, by the code's own structure, not by a gap in the parser.
- A parse anchored on the declared string's own text is a choice between two
  failure modes proven against this manual as it reads today: a prefix
  comparison (the substring before the first comma) cannot distinguish `"CC
  Hold"` from `"CC Hold, permanently"` — they share the same prefix — so the
  positive control the previous design specified for exactly this edit could
  not turn the check red; and a marker-word reverse check (`must`, `set to`,
  `unchecked`, `stay selected`, `Utility`) scores zero clause markers on five
  of today's six device subsections as written — `Twister` itself scores 3
  occurrences across two distinct markers (`unchecked` once, `Utility`
  twice), not zero, but that only proves *some* marker word is present
  somewhere in the Twister's subsection, not that the specific declared
  string deleted is missing (verified by direct count against the live
  manual: `APC40 Generic`, `APC40 Ableton`, and all three Launchpad
  subsections score 0 each), so it
  would stay silent if a real declared string were deleted from a device that
  states its precondition in different words, such as the APC40 Generic's
  "Keep Track 1 selected:".

None of these is a defect in how carefully the text was parsed — they are
the source text's actual shape: a shared factory function, a
copy-constructing variant, and free prose with no fixed vocabulary. A fourth
text-parsing design would need either a dedicated code path per factory-shape
this file uses (defeating the point of the shared Launchpad helper and the
Ableton copy-constructor existing at all, and with no guarantee a fifth shape
introduced later is covered) or a real C++ parser, which no check script in
this repository is or builds.

**The alternative this task adopts: an emitter, built and run, not a text
parse.** `app/check_docs_match_device_preconditions.py` builds a small
emitter binary against `app/FroggersMidiCatalog.hpp` — the same header
`app/FroggersMidiCatalogTests.cpp` already links against — whose `main()`
constructs `synth_froggers::FroggersMidiCatalog()` and prints each entry in
the live `catalog.deviceDefaults`, in order: its `id` and its
`declaredPreconditions` list, one line of a fixed, parseable format per
device. This reaches every device by construction: it reads the constructed
object at runtime, not the source that built it, so the shared Launchpad
helper and the Ableton copy-constructor are invisible to it — there is
nothing to miss, because there is no per-call-site logic to write. It also
gives the floor its count for free: the number of lines the emitter prints
*is* `catalog.deviceDefaults.size()`, the same source
`app/FroggersMidiCatalogTests.cpp:399`'s own count assertion reads, not a
number written into the check. That count is six today; it becomes seven once
task 5.1 appends the Launch Control XL, with no edit to the check either
time. Stating the count as "whatever the compiled catalogue holds," rather
than as a fixed number, is what keeps this paragraph correct on both sides of
task 5.1's own boundary without needing an edit when it runs.

**Cost of the alternative, stated plainly.** The emitter costs one new small
binary, wired into `app/Makefile` beside the twelve existing test binaries,
plus a marker convention in `MANUAL.md` (below) that task 4.7 and 5.6 must
produce. A fourth text-parsing design would cost, at minimum, one dedicated
code path per factory-function shape this file uses today, permanently ahead
of whatever shape a future device default introduces, plus a marker-word
vocabulary already measured wrong against six of the seven subsections this
change ships. Reading the compiled catalogue is bounded and exact and does
not grow with the number of ways `FroggersMidiCatalog.hpp` is free to write a
device default; parsing its text does.

**Drift half — comparison unit: a delimited region, byte for byte, not a
substring search in free prose.** Tasks 4.7 and 5.6 wrap each device's
generated settings text in an HTML-comment marker pair keyed by that device's
own `id` — `<!-- declaredPreconditions:froggers.twister -->` ...
`<!-- /declaredPreconditions:froggers.twister -->` — one pair per entry in
`catalog.deviceDefaults`, including the three Launchpads and the Ableton
default, whose markers bound explicitly empty generated text (a stated
absence, not an unmarked one). The check renders the expected text for each
device directly from the emitter's own output (one declared string per line,
in list order, under a fixed heading) and compares it, byte for byte, against
what is actually inside that device's checked-in markers, matched by `id`.
Any difference — a hand-edit inside the markers, a stale value the emitter no
longer produces, a marker moved or missing — is reported as a mismatch naming
the device id. The floor requires exactly as many marker pairs in `MANUAL.md`
as the emitter reports devices; fewer fails outright, naming the missing
ids, rather than reporting agreement over whatever subset was found.

This reaches the whole declared string, not a prefix — `"CC Hold"` and `"CC
Hold, permanently"` differ as complete strings even though they share a
comma-terminated prefix — and it reaches the setting, the value, and where it
is set together, since the whole string travels from the emitter through the
comparison, which is what `spec.md`'s ADDED requirement asks the declaration
to carry.

What it does not find: a device-setting-shaped sentence written *outside* the
delimited region, elsewhere in the same subsection's hand-written prose.
`spec.md`'s "SHALL NOT depend on a device setting it does not declare" already
ships unchecked this cycle for a related reason (below); as cheap, disclosed
supplementary coverage for the same gap, the check also runs the marker-word
scan the earlier design specified — `must`, `set to`, `unchecked`, `stay
selected`, `Utility`, plus `keep`, `leave`, `off`, and `template` (imperative
forms this manual's own prose actually uses) — over each subsection's prose
*outside* its marker pair, and reports a WARNING, never a failure, when one of
those words appears there. This is stated plainly as a heuristic that can
miss phrasing nobody anticipated, not a rule the check enforces; the exact
region comparison above is the check's real assertion.

**Drift half — positive controls, both directions, against the built
mechanism.** (a) Declaration-edit direction: temporarily append a word to the
Twister's declared CC Hold string in `app/FroggersMidiCatalog.hpp` (e.g.
`"CC Hold, permanently"` in place of `"CC Hold"`), rebuild the emitter, and
confirm the check turns red — the checked-in region in `MANUAL.md` still
reads the old text, so the two no longer match byte for byte. (b) Manual-edit
direction: temporarily edit the text *inside* `MANUAL.md`'s Twister marker
pair (not merely somewhere in that subsection) to say "all six side buttons
set to CC Toggle" in place of "CC Hold", and confirm the check turns red
because the checked-in region no longer matches what the emitter currently
produces. Revert both immediately after confirming red; neither ships. Run
before task 4.7 has generated any marker pairs, the drift half instead fails
on the floor (zero marker pairs found against a compiled catalogue of six) —
this is the drift half's own "before" state, distinct from (a) and (b), and
task 4.8 records it too, so both halves' expected early results are written
down, not only the recovery half's.

**Recovery half — rule, floor, positive control (text-only; authored and run
early, in task 1.8).** The check parses the `### Shift` and `### Hold Drill`
subsections, heading-delimited, and applies three rules, all of which must
pass; if either heading is not found at all, the check fails naming the
missing heading, rather than vacuously passing an empty comparison.

1. **Presence, as a conjunction over all three triggers, unconditionally, not
   a disjunction over any one.** Each subsection contains, for every one of
   the three triggers this change's own recovery text offers — Rebuild,
   Ceiling and EndpointOpen, all three, regardless of host — at least one of
   that trigger's own multi-word phrases, verbatim, as task 3.1's rewrite
   introduces them: Rebuild as "selecting a different preset" or "rebuilds
   the row's mapping"; Ceiling as "clears automatically after"; EndpointOpen
   as "unplugging and reconnecting the controller". `MANUAL.md` is one
   document read by every host's operator, not partitioned by host, so the
   presence rule does not conditionally excuse a host from naming
   EndpointOpen; what varies by host is only whether the same subsection
   must also exclude the plugin (rule 3, below), never whether the phrase
   itself is required. A subsection missing any one of these three families
   fails rule 1 by name for the missing ones — an earlier, disjunctive form
   of this rule accepted a manual naming a single trigger even though
   `spec.md`'s scenario says "the triggers" (plural) and task 3.1 is
   instructed to name all three per subsection; the conjunction is what
   actually enforces that instruction. The phrases are
   multi-word and name an action, not a bare noun: a single word like
   "unplugged" is not sufficient, because the **current, unmodified** text
   already contains that word as part of describing the *failure* ("If the
   controller is unplugged while Shift is still held, its buttons stay
   shifted..."), not as a stated recovery — a bare-word check would pass the
   very text this check exists to reject.
2. **Absence.** Neither subsection contains "pressed and released again" (or
   any sentence naming the modifier's own button — Shift's own button for
   the Shift subsection, Hold Drill's own button for that subsection — as
   what clears it), because that is exactly the recovery `spec.md`'s
   scenario forbids: one that depends on the same address whose failure to
   transmit is what strands the modifier in the first place. Implemented
   mechanically as literal-substring absence of "pressed and released
   again" (no trailing comma — the exact current text ends in a period, not
   a comma, and a comma appended to the matched phrase would match nothing
   in it); the broader "any other sentence naming the modifier's own
   button" is a drafting rule for task 3.1's own prose, not a second
   mechanical pattern — it names no fixed substring, and none is invented
   here. Presence alone accepts the **current**
   `MANUAL.md:319-320` text unmodified, plus one appended sentence
   containing every presence phrase, because nothing in a presence-only rule
   reads the rest of the subsection — the absence rule is what makes
   `MANUAL.md:319-320`'s own current sentence itself a positive control (see
   below), not only a hypothetical one.
3. **Per-host coverage, triggered on any reconnect-shaped phrase, not one
   exact literal.** If a subsection contains a case-insensitive match for
   `reconnect` (so "unplugging and reconnecting the controller",
   "reconnecting it", or any other phrasing built on the same word all
   trigger this rule, not only the one literal task 3.1 is instructed to
   write), the same subsection must also contain the literal substring "not
   available in the plugin" — task 3.1's rewrite states EndpointOpen is
   unavailable in the plugin build (design.md,
   above), and task 3.1 is separately instructed "do not describe the
   plugin as having the reconnect route"; nothing before this rule checked
   that instruction was followed. A subsection that mentions reconnecting
   the controller without also excluding the plugin fails, whatever
   phrasing it uses for the reconnect itself.

This half has no dependency on `declaredPreconditions` or the submodule pin,
so task 1.8 authors it and runs it against the **current, unmodified**
manual before task 3.1 changes anything, and both controls below must turn
red before task 3.1 exists to make them pass, not after — if either does
not, the check tests nothing, exactly as it did in the version this change
replaces (a false-positive check committed against
`check_spec_checks_resolve`'s check-name resolution rather than its
behaviour):

- Against `MANUAL.md:319-320` as it reads today ("buttons stay shifted until
  Shift is pressed and released again.") the check must fail on rule 1
  (presence) — none of the three phrase-families is present, only the bare
  word "unplugged" and "pressed and released again". It must **also** fail on
  rule 2 (absence) once all three presence phrases are appended to this same
  unmodified sentence without removing "pressed and released again" —
  presence alone would accept `MANUAL.md:319-320` verbatim plus one appended
  sentence naming every trigger, and rule 2 is
  what this repository's own current text is used to prove red before
  task 3.1 removes the sentence rule 2 targets.
- Against `MANUAL.md:308-313` (no recovery sentence at all today) the check
  must fail for the "heading found, no phrase present" reason (rule 1).
- Against a hypothetical rewrite that states "unplugging and reconnecting
  the controller" without also stating "not available in the plugin"
  anywhere in the same subsection, the check must fail on rule 3.

Both controls are run before task 3.1 exists to make the rules pass, not
after — a check whose positive control cannot be demonstrated against text
that predates the fix proves nothing. `spec.md`'s Check line for the
scenario this half backs names task 1.8 and states this same three-rule
set, not a second, differently-worded one.

**The recovery text states no numeric duration for the Ceiling trigger.**
An earlier draft of task 3.1 had the manual state the Ceiling's duration in
seconds, read at authoring time from `External/Sheaf`'s own design.md
default (`kHeldModifierCeilingMicros`, currently 30 seconds). That default is
explicitly Sheaf's own change's to tune before it ships (its own Risks
section calls it reversible), and freezing whatever number is current into
this repository's manual text would go stale the next time Sheaf's change
adjusts it, with nothing in either repository's check surface positioned to
notice — the same class of frozen figure this change avoids elsewhere by
citing a command instead of a number. The recovery text names "clears automatically after" as the
trigger phrase (rule 1, above) without a number attached, describing the
Ceiling as an automatic elapsed-time clear rather than committing to a
duration this repository does not own and cannot re-verify at manual-render
time. Task 3.1 states this explicitly; the recovery-half check's phrase set
does not include or require any numeric text.

**The undeclared-dependency SHALL ships unchecked this cycle, stated as
such.** `spec.md`'s "A preset SHALL NOT depend on a device setting it does
not declare" cannot be verified by comparing generated text against
declarations — both sides are written from the same source, so a dependency
absent from both passes trivially. The drift half's best-effort WARNING scan
(above) is a cheap, disclosed heuristic over prose *outside* the generated
region, not a substitute: it can miss phrasing it has no word for and proves
nothing when it finds nothing. Checking the SHALL for real would mean tracing
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
control. Narrower still: the Getting Started Guide's own enumeration of what a
factory template controls ("pots, LED colours and mode buttons ... and Notes
(Pads)") does not name faders as a category, so neither document states that
fader CCs are template-scoped at all, only that pots, buttons and pads are.
That the faders move with the template rests entirely on the Ableton script's
own choice to select factory template 1 before constructing its sliders
(below) — read here as the script author's evidence, not a vendor statement.
This finding stands.

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
script selects **factory template 1**. Its `LIVE_CHANNEL` constant is 8. The
Programmer's Reference Guide's "Device-to-Computer messages" section states
one sentence about zero-indexing, and it is scoped to buttons, not faders or
the device generally: "Buttons can output either note messages or CC messages
on a zero-indexed MIDI channel n." No sentence in either document states
channel numbering for faders or pots specifically. What corroborates
`LIVE_CHANNEL = 8` meaning channel 9 counted from 1 is the Guide's own
internal consistency, not a fader-scoped statement: every message table in
the document — `Bnh`/`176+n` throughout Computer-to-Device Messages, the
System Exclusive `Template` byte, the buttons' own zero-indexed channel above
— uses the same `n`-is-zero-indexed convention with no exception carved out
for any one control type, so reading the fader's channel constant the same
way is consistent with the whole document, not derived from a sentence that
names faders. The script's slider factory constructs each fader as a
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

`dis.get_instructions` disassembles one code object and does not descend
into the nested code objects a `def` or a comprehension compiles to — the
module-level command above is filtered to `LOAD_CONST`/`LOAD_NAME`/
`STORE_NAME`/`BUILD_TUPLE`/`BINARY_OP` at module scope only, so it cannot by
itself show the fader CC numbers, which are built inside `make_slider` (a
nested `def`) and a list comprehension (its own nested code object under
every Python version this reading used). Reading those requires
disassembling each nested code object individually — a recursive walk over
`co_consts` looking for nested code objects, since `dis.get_instructions`
does not descend on its own — which is a second,
separate reading, recorded here in the same form as the module-level one —
same interpreter, `~/.local/bin/python3.11` (`~/.local/bin/python3.11 -V`
reports `Python 3.11.14` on this machine; the reading was run under this
pinned interpreter specifically, not the system `python3`, which
`python3 -V` reports as `Python 3.13.5` on this machine and would
inline a comprehension into its enclosing code object under PEP 709,
producing a different, misleadingly complete instruction stream at module
scope — not run for this reading):

```
$ cd "/Applications/Ableton Live 12 Suite.app/Contents/App-Resources/MIDI Remote Scripts/Launch_Control_XL" && ~/.local/bin/python3.11 -c '
import marshal, dis
def walk(code):
    if code.co_name in ("make_slider", "<listcomp>"):
        consts = [c for c in code.co_consts if not hasattr(c, "co_code")]
        print(f"== {code.co_name} (script line {code.co_firstlineno}) consts={consts}")
        for x in dis.get_instructions(code):
            print(f"{x.offset:5} {x.opname:14} {x.argrepr}")
    for c in code.co_consts:
        if hasattr(c, "co_code"):
            walk(c)
c = marshal.loads(open("LaunchControlXL.pyc", "rb").read()[16:])
walk(c)
'
== make_slider (script line 88) consts=[None, ('name',)]
   [elided: offset 0, a RESUME instruction]
     2 LOAD_GLOBAL    NULL + SliderElement
    14 LOAD_GLOBAL    MIDI_CC_TYPE
    26 LOAD_GLOBAL    LIVE_CHANNEL
    38 LOAD_FAST      identifier
    40 LOAD_FAST      name
    42 KW_NAMES
    44 PRECALL
    48 CALL
    58 RETURN_VALUE
== <listcomp> (script line 101) consts=[77, 'Volume_%d', 1]
     0 COPY_FREE_VARS
   [elided: offset 2, a RESUME instruction]
     4 BUILD_LIST
     6 LOAD_FAST      .0
     8 FOR_ITER       to 56
    10 STORE_FAST     i
    12 PUSH_NULL
    14 LOAD_DEREF     make_slider
    16 LOAD_CONST     77
    18 LOAD_FAST      i
    20 BINARY_OP      +
    24 LOAD_CONST     'Volume_%d'
    26 LOAD_FAST      i
    28 LOAD_CONST     1
    30 BINARY_OP      +
    34 BINARY_OP      %
    38 PRECALL
    42 CALL
    52 LIST_APPEND
    54 JUMP_BACKWARD  to 8
    56 RETURN_VALUE
```

Both instructions are now marked as elided rather than silently missing: this
section as it previously read dropped each function's own `RESUME` line (the
offset gap — `0`→`2` in `make_slider`, `0`→`4` in `<listcomp>`, both left
intact) with no elision mark, which reads as an oversight since one other
elision in this design (the three encoder-row comprehensions, above) is
marked. `RESUME` carries no operand relevant to this reading (it is CPython's
own interpreter-entry marker on every code object, present identically on
both), so this reading does not re-run the disassembly to recover its exact
printed form; it marks the gap explicitly instead. No other instruction in
this section is omitted.

(The three encoder-row comprehensions at script lines 93, 94 and 98, over the
same `consts=[13, ...]`/`consts=[29, ...]`/`consts=[49, ...]` shape calling
`make_encoder` instead of `make_slider`, are part of the same recorded
disassembly and are not reproduced here since this requirement concerns the
faders only.) `make_slider` itself loads `LIVE_CHANNEL` by name (`LOAD_GLOBAL
LIVE_CHANNEL`, offset 26) rather than a literal, so the fader's channel is
the same module-level constant read above, not a second value; the
`<listcomp>` calls `make_slider` with `LOAD_CONST 77` plus the loop index
`i`, one call per fader — CC 77 to CC 84 across the eight faders, labelled
`Volume_%d`, which this change reads as the CC map for the row of faders and
assigns fader 1 (`i = 0`, CC 77) to scene blend.

This reading rests on one unread premise: `make_slider`'s own body constructs
`SliderElement(MIDI_CC_TYPE, LIVE_CHANNEL, identifier, name)` positionally,
and this disassembly does not descend into `SliderElement.__init__` itself —
only Ableton's own `_Framework` module defines it — so which positional
argument `SliderElement.__init__` treats as the MIDI channel and which as the
CC number is read from `make_slider`'s call-site convention (channel before
identifier, matching every other control factory in this script), not from a
disassembly of `__init__`'s own parameter order. The shipped `CC 77 / channel
8` mapping rests on that premise being right; it is stated here as unread, not
corroborated.

The `.pyc`'s bytecode gives `LIVE_CHANNEL = 8` and the template byte `8`
directly; the two paragraphs above corroborate what that one constant *means*
in its two roles — the fader's channel (channel 9 counted from 1) and the
`Change current template` message's own channel byte, since `LIVE_CHANNEL` is
loaded once by name and reused in both the SysEx tuple and the slider
factory, not two integers occupying two purposes — against the vendor's own
Programmer's Reference Guide, which states the meaning of a zero-indexed
channel and of template slots 08h-0Fh but not the values themselves. The CC
numbers 77-84 rest on the disassembly alone — no vendor document states them
at all, factory-template or otherwise.

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
Launch Control XL is attached to this machine: `ioreg -p IOUSB -w 0` prints
two host controllers (`AppleT8112USBXHCI`) and no peripheral device beneath
either, which is consistent with nothing being attached but does not by
itself distinguish that from "this command enumerates nothing on this
machine regardless" — `system_profiler SPUSBDataType`, the control that
would draw that distinction, itself prints no output at all on this machine,
so it corroborates nothing either way; the disposition (no Launch Control XL
attached) rests on there being no third-party peripheral entry in either
command's output, not on a device-count claim neither command actually
supports. What the
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

- **The manual can be held by another active change on `main` at any time.**
  Whichever change `ls /Users/diegoaguilar-canabal/Desktop/frogg3rs/openspec/changes/`
  names is the live party to check against, re-derived, not assumed — see
  task 3.2 for the exact command and its two dispositions (empty vs.
  non-empty `git status --short MANUAL.md QUICK_DICT.md` on the main
  checkout). At this writing the live party is `frogg3rs-envelope-curve-direction`,
  whose own edit (`git diff -U0 MANUAL.md | grep '^@@'` → `@@ -452,3 +452,6 @@`
  and `@@ -579,2 +582,6 @@`) is disjoint from this change's MIDI controllers
  section (`MANUAL.md:258-390`) and touches no `QUICK_DICT.md` line this
  change cares about. → Diff-review before staging and never stage a
  whole-file `git add` while any change named by that `ls` could still be
  editing either file; a hunk that does overlap `:258-390` is task 3.2's own
  STOP condition, not a silent merge.
- **`declaredPreconditions` belongs to Sheaf's change and does not exist yet,
  and neither does `HeldModifierClearSource`.** → Group 4, and task 3.1's
  manual rewrite, gate on the submodule checkout carrying both symbols (tasks
  4.1-4.2); this change does not define either field and does not render
  `declaredPreconditions` — the render is Sheaf's own `synth-runtime-ui`
  requirement sru-64.
- **The commit task 4.2 pins is reachable from no remote until the operator's
  own later step, and until then exists in exactly one object store on this
  machine.** → Sheaf's own task 7.9 states that this change performs no
  push, opens no pull request, and moves no pin in this cycle; the operator
  has decided this delivery cycle ends at a push of `worktree-midi-resilience`
  and nothing more, so an unreachable pin until that operator's later merge
  is expected, not a defect. Stated plainly rather than implied: a fresh
  clone or a CI checkout of the branch this change's own task 6.7 pushes
  (`worktree-midi-resilience`) cannot resolve `External/Sheaf`'s gitlink until
  the operator's later rebase-and-merge publishes both commits together —
  and, more than that, the pinned commit exists in exactly one object store
  on this machine, `.git/worktrees/midi-resilience/modules/External/Sheaf`
  (Sheaf's own task 7.9 states the same fact from its side); the main
  checkout's own submodule store, `.git/modules/External/Sheaf`, cannot
  resolve it (`git -C
  /Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf cat-file -e
  <sha>` exits non-zero for this commit). **No step before the operator's
  merge may remove this worktree or otherwise destroy that object store** —
  `git worktree remove` of a checkout with a submodule destroys that
  submodule's object store irrecoverably; treat this as an absolute
  constraint on this worktree and its submodule checkout, not only a risk to
  note.
  `.github/workflows/pages.yml` checks out with `submodules: recursive` on
  push to `main`, not to this branch, so that workflow does not even run
  against this push — the merge is the point at which the pin must resolve,
  and task 5.4's live-site operator check is sequenced after it for that
  reason.
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
declares nothing behaves as today. Group 4, AND task 3.1's manual rewrite in
group 3, do not run until the submodule checkout confirms both
`declaredPreconditions` and `HeldModifierClearSource` (task 4.1) and the pin
is advanced (task 4.2) — group 3's manual rewrite documents recoveries
`HeldModifierClearSource` creates, so it is not executable ahead of that
confirmation either. Until that
confirmation passes, this change's only executable work is: the two
device-generic checks that do not depend on either field (the
Twister-only-Shift rewrite, task 4.5, and the unshifted-usability case, task
4.6) and the check script's recovery half (task 1.8), all of which read
today's tree directly and assert nothing about `declaredPreconditions` or
`HeldModifierClearSource`. Group 5's Launch Control XL default also waits on
the same confirmation (its declared-preconditions population, task 5.2; task
5.1 itself, the bare catalogue entry with no declared preconditions, does
not). The Launch
Control XL device default adds a catalogue entry and changes no existing one.
Patches store mappings, not device defaults, so a patch saved before this
change loads unchanged after it. The commit this change's own delivery (task
6.7) pushes carries a submodule pin reachable from no remote until the
operator's later merge (Risks, above); that does not block this change's own
delivery step, which is a push and nothing more, but it does block anything
that needs the pin published — task 5.4's live-site operator check is
sequenced after that merge for exactly this reason.

## Open Questions

None held by this change's own execution path. The Launch Control XL's fader
CC/channel map is answered by Ableton Live 12 Suite's control-surface script
(see above); confirming that map on a physical device remains a post-delivery
operator step (task 5.4), not a question this change's artifacts can resolve
further.
