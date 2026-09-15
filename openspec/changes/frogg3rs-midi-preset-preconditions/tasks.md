## 1. Preflight

- [ ] 1.1 **Precondition, already satisfied — confirm before anything else in
      this list runs.** A sibling worktree at
      `.claude/worktrees/midi-controller-resilience/` once carried a fully
      divergent, untracked copy of this change (`openspec/changes/frogg3rs-midi-preset-preconditions/`,
      differing in `proposal.md`, `design.md`, `tasks.md` and the spec delta)
      and, inside its own `External/Sheaf` checkout, a divergent untracked
      copy of `External/Sheaf/openspec/changes/midi-controller-resilience/`
      (no `synth-controller-wizards` delta, no `scw-6` requirement at all).
      That worktree and its branch no longer exist. **The executor confirms
      this by running, from the main checkout,**
      `ls .claude/worktrees/midi-controller-resilience/openspec/changes/frogg3rs-midi-preset-preconditions`
      (must return `No such file or directory`) and
      `git branch --list worktree-midi-controller-resilience` (must return no
      output) — do not proceed past this task on the strength of this note; run
      the commands yourself. Both duplicates were, before the worktree was
      removed, md5-identical to the snapshots folded into this repository's own
      commit `6e77142` ("Collect the MIDI resilience work into one worktree")
      and Sheaf's commit `caae5c2` ("Carry the MIDI controller resilience
      change into the submodule"), so nothing in either duplicate was lost.
- [ ] 1.2 **Confirm this branch is at `main`'s tip** before baselining, so
      1.6's baseline is not stale before this change's own diff starts. Check:
      `git log --oneline HEAD..main | wc -l` → `0`. If this is nonzero when
      this task runs, STOP and report rather than baselining against a stale
      tree: this task's claim is that the branch is caught up as of the time
      it is checked, not a promise that `main` cannot move further afterward.
      While at `main`'s tip, `openspec/changes/frogg3rs-delay-width-wysiwyg-repair/`
      does not exist in this tree (that change shipped and was archived as
      `frogg3rs-delay-capacity-and-width-finish`) — confirm with
      `ls openspec/changes/frogg3rs-delay-width-wysiwyg-repair` returning "No
      such file or directory" — and `python3 app/check_artifact_symbols_resolve.py app`
      reports `check-artifact-symbols-resolve: OK - 6 artifact file(s) resolve,
      1 name(s) declared as not yet created`. If either check disagrees because
      `main` has moved since this was written, STOP and report the actual state
      rather than resolving by guesswork.
- [ ] 1.3 Enumerate by operand, case-insensitively, across `app/`, `openspec/`
      and the documents: `sceneBlend`, `AnalogMidiInConfig`, `analogRange`,
      `appActions`, `kBpm`, `MidiAppDeviceDefault`, `declaredPreconditions`,
      `preconditions`, `KindSupport`, `analogs`, `LaunchControl`.
      Report FOUND vs CHANGED per operand, zeros included.
- [ ] 1.4 Confirm by reading that no `openspec/specs/` or
      `External/Sheaf/openspec/specs/` requirement forbids a `Generic` device
      default carrying an analog section, and cite the one that permits it.
- [ ] 1.5 Confirm the APC40 entries' existing scene-blend and BPM assignments are
      unchanged by anything in this change, and record that disposition.
- [ ] 1.6 Baseline every gate `app/Makefile`'s `test:` target runs, read from
      the Makefile's own `check-*` prerequisite list and its test-binary
      variables rather than copied here (the list drifts: an earlier version
      of this task named eight scripts by line number and missed two, and the
      branch reaching `main`'s tip added two more). This baseline runs AFTER
      task 1.2 confirms the branch is at `main`'s tip, against that tree.
      Record, with its exit code, all twelve `check-*` prerequisites
      (`check-no-juce`, `check-no-firmware-includes`, `check-microphone-usage`,
      `check-catalog-covers-screen-actions`, `check-docs-match-parameter-table`,
      `check-spec-checks-resolve`, `check-citations-resolve`,
      `check-modified-requirements-restate-promoted`, `check-no-planning-history`,
      `check-artifact-symbols-resolve`, `check-delay-capacity-parameters-are-swept`,
      `check-delay-capacity-break-proofs` — read from `app/Makefile`'s `test:`
      line itself, not copied here, since the list drifts) and the twelve test
      binaries the `test:` target links (`$(TEST_BIN)`, `$(MONO_VALIDATION_BIN)`,
      `$(DSP_TEST_BIN)`, `$(PARAMETER_MODEL_BIN)`, `$(MODULATION_BIN)`,
      `$(AUDIO_ROUTING_BIN)`, `$(VISUALIZER_BIN)`, `$(SCOPE_ADVANCE_INDEX_BIN)`,
      `$(MARBLES_CLOCK_BIN)`, `$(SURFACE_BIN)`, `$(MIDI_CATALOG_BIN)`,
      `$(CONTROLLERS_PAGE_BIN)`). Ten of the twelve `check-*` targets run
      without compiling anything; run by path directly against this tree, all
      ten report `OK`: `check-no-firmware-includes`, `check-microphone-usage`,
      `check-catalog-covers-screen-actions`, `check-docs-match-parameter-table`,
      `check-spec-checks-resolve`, `check-citations-resolve`,
      `check-no-planning-history`, `check-artifact-symbols-resolve` (`OK - 6
      artifact file(s) resolve, 1 name(s) declared as not yet created`),
      `check-delay-capacity-parameters-are-swept`, and
      `check-modified-requirements-restate-promoted` (`OK - 5 MODIFIED
      requirement(s), 134 promoted clause(s) restated or declared, 9 declared
      edit(s), 1 promoted scenario(s) no longer restated`, exit `0`) — this
      last one also prints an informational `NOTE` naming the one promoted
      scenario this change's own spec delta does not fully restate; read that
      script's own header before treating a `NOTE` as a failure. The remaining
      two, `check-no-juce` and `check-delay-capacity-break-proofs`, compile
      code under `$(CXX)`; re-run them, and the twelve test binaries, and
      `./app/build-launcher.sh`, only when actually building this change,
      capped at `-j2` under `nice`, and record their exit codes here.
      `make -C app test` does not necessarily run any of the twelve binaries
      if any `check-*` prerequisite is red — GNU Make 3.81 aborts a linear
      prerequisite list at the first failing one — so if any check is red when
      this task runs, run each of the twelve binaries by path directly, note
      which ran and which did not, and say so explicitly rather than
      describing `make -C app test` as this change's green gate.
- [ ] 1.7 §8.0 hygiene sweep over `app/` and `openspec/`. Name each directory.
      (`openspec/changes/frogg3rs-delay-width-wysiwyg-repair/` does not exist
      in this tree — confirmed by task 1.2 — and
      `check-artifact-symbols-resolve` reports OK, so no dangling citation to
      it remains. If this sweep, re-run at execution time, nonetheless finds a
      reference to that removed pre-split directory, or to the also-removed
      `openspec/changes/frogg3rs-midi-controller-resilience/` (confirmed
      absent), or to the archived `RESEARCH2-drive-delay.md` path — this sweep
      is where it is repaired: such a reference is this change's own §8.0
      obligation because this change is that removed change's app-half
      successor for exactly the coordination concern it names —
      MANUAL.md/QUICK_DICT.md editing conflicts — so repoint it to this
      change's own directory, `openspec/changes/frogg3rs-midi-preset-preconditions/`,
      not to Sheaf's `midi-controller-resilience`, which is the engine half
      and does not touch those documents.)

## 2. The Launch Control XL fader map

- [ ] 2.1 Read Novation's published Programmer's Reference Guide for the
      device: *Launch Control XL Programmer's Reference Guide*, Version 2
      (https://fael-downloads-prod.focusrite.com/customer/prod/downloads/launch_control_xl_programmer_s_reference_guide.pdf)
      and the *Getting Started Guide*
      (https://fael-downloads-prod.focusrite.com/customer/prod/s3fs-public/downloads/Launch%20Control%20XL%20GSG%20v2.pdf).
      **Done, and the result is negative.** Both documents were read in full (9
      and 7 pages). They describe the LED-lighting protocol (the
      Launchpad-compatible note/CC addressing and the device's own System
      Exclusive `Set LEDs` / `Toggle button states` / `Change current
      template` messages) and state that the 8 factory templates "output a
      fixed set of MIDI CCs (pots, LED colours and mode buttons) and Notes
      (Pads)" — but neither document enumerates those fixed CC numbers or
      channels for any control, including the faders, on any template. This
      finding stands. The map is read instead from the one source on this Mac
      that encodes it: Ableton Live 12 Suite's own control-surface script for
      the device (Live 12.4.3). That reading is not this task's or this
      task's executor's: it was performed once, by disassembly, by the lead,
      on 2026-09-14, and design.md's "Who read the script, by what command,
      and what it printed" carries the literal command and its output —
      consult that section rather than treating factory template 1 and CC 77
      to CC 84 (channel 8 counted from 0, channel 9 counted from 1) as
      self-evident here. This task does not re-open or re-disassemble the
      file; it is satisfied by citing that existing reading, not by producing
      a new one.
- [ ] 2.2 Choose the fader for scene blend: fader 1, CC 77, channel 8 counted
      from 0. The eight faders are undistinguished by function in every
      source read for this change, so the first is chosen rather than an
      arbitrary pick from the row's middle (design.md carries the reasoning).

## 3. Documents

- [ ] 3.1 Rewrite `MANUAL.md:319-320` (Shift) and add a matching recovery
      sentence to `MANUAL.md:308-313` (Hold Drill, which has none today), each
      naming which of `HeldModifierClearSource`'s five triggers are available
      on which host — traced from Sheaf's `midi-controller-resilience`
      design.md, not invented here: **Rebuild** (selecting a different preset,
      or otherwise causing the row's mapping to rebuild) is available on every
      host. **Ceiling** (an automatic elapsed-time clear, 30 seconds) is
      available on every host, because `Engine::MessageThreadTick` — the
      pump that evaluates it — runs on all three (standalone's `Runtime.hpp`
      timer, the browser's `setInterval`, and the plugin's own JUCE timer).
      **EndpointOpen** (unplugging and reconnecting the controller) is
      available in the standalone and browser builds, which manage their own
      MIDI ports, and NOT in the plugin, which takes MIDI through host
      automation and opens no ports of its own (design.md: "A host that
      embeds neither ... has the other four triggers and not this one").
      **Release** and **SecondPress** both require the same button's own
      address to transmit again — the two triggers that are unavailable
      exactly when this recovery is needed, so they are not offered as the
      recovery, only named as what ordinarily ends a hold. Write the recovery
      as an explicit list of actions using multi-word phrasing (e.g.
      "selecting a different preset", "unplugging and reconnecting the
      controller", "clears automatically after 30 seconds"), not a bare
      mention of "unplugged" — task 4.8's check matches on these specific
      phrases, and a bare word is exactly what the current, unmodified text
      already has without stating a recovery (design.md's recovery-half rule
      explains why). Do not describe the plugin as having the reconnect
      route.
- [ ] 3.2 Coordinate every `MANUAL.md` edit with `frogg3rs-delay-capacity-and-width-finish`
      (present in this worktree's own `openspec/changes/`, since this branch
      is at `main`'s tip; task count moves between sessions — re-count with
      `grep -c '^- \[x\]'`/`'^- \[ \]'` on its `tasks.md` rather than trusting
      a figure recorded here). Traced directly on the main checkout, not
      assumed: `git -C <main checkout> status --short MANUAL.md QUICK_DICT.md`
      returns nothing — neither file carries an uncommitted edit there as of
      this writing; re-run that check rather than relying on this note, since
      another session can change it at any time.
      `openspec/changes/frogg3rs-delay-width-wysiwyg-repair` does not exist in
      this tree (that change shipped and was archived as
      `frogg3rs-delay-capacity-and-width-finish`) — confirm with
      `ls openspec/changes/frogg3rs-delay-width-wysiwyg-repair` returning "No
      such file or directory" — so it is not a separate coordination party.
      `frogg3rs-delay-capacity-and-width-finish`'s Delay bank section
      (`MANUAL.md:678-742`) is disjoint from this change's `MANUAL.md:258-390`.
      Diff-review `MANUAL.md` section-by-section before staging; never stage a
      whole-file `git add` while it is active. `QUICK_DICT.md` carries no MIDI
      content for this change to edit (`grep -in "midi\|shift\|hold drill\|twister\|
      apc40\|launchpad" QUICK_DICT.md` returns nothing) — this change does not
      touch it.

## 4. Declared preconditions

- [ ] 4.1 **Gate.** Do not start the rest of this group until
      `External/Sheaf`'s submodule checkout is at a commit, reachable from
      Sheaf's `fork/midi-resilience-merge` (its own task 7.7 delivers this
      branch there and nothing else), that has executed Sheaf's tasks 6.1 (adds
      `declaredPreconditions` to `MidiAppDeviceDefault`,
      `include/synth/MidiAppCatalog.hpp:31-38`) and 6.2 (threads it through
      `ControllerWizardDescriptor` and `MakeControllerWizardRegistry`). Confirm
      with `grep -n declaredPreconditions
      External/Sheaf/projects/synth/include/synth/MidiAppCatalog.hpp` — it
      must return a real member; today it returns nothing. The branch name is
      only how this task locates a candidate commit; it is not what task 4.2
      pins. Once this gate is satisfied, record the exact commit SHA the grep
      above was run against — that SHA, not the branch name, is what 4.2
      checks out and what 4.2's own commit message names.
- [ ] 4.2 **Submodule pin advance.** `git -C External/Sheaf fetch fork && git -C
      External/Sheaf checkout <the commit 4.1 confirmed> && git add
      External/Sheaf`, naming the exact commit SHA in the commit message. This
      is the only task in this change that moves the pin; nothing else does.
- [ ] 4.3 Populate the Twister default's `declaredPreconditions` from the
      precondition sentences at `app/FroggersMidiCatalog.hpp:14-18` (relative
      encoders, all six side buttons on CC Hold, "Bank Side Buttons"
      unchecked), and reword the surviving comment at `:19-24` (the
      button-layout description, which is not a precondition and is not
      deleted) so it does not restate the declared settings and states CC
      Hold's reason as "the promptest of the triggers that end a held
      modifier" rather than "what ends Shift." Add a
      device_defaults_declare_their_preconditions case to
      `app/FroggersMidiCatalogTests.cpp` asserting the Twister's three
      declarations.
- [ ] 4.4 Populate both APC40 mkII defaults' `declaredPreconditions`, which are
      **not the same list**: the Generic default declares the Track 1 caveat
      from `app/FroggersMidiCatalog.hpp:26-31`; the Ableton default declares an
      **empty list**, because its connect-time SysEx message
      (`app/FroggersMidiCatalog.hpp:186`) is sent by the app automatically and
      removes that caveat, and `MANUAL.md:353-357` names no manual precondition
      for it. Add both cases to the same
      device_defaults_declare_their_preconditions case in
      `app/FroggersMidiCatalogTests.cpp`, including the empty-list assertion
      for Ableton.
- [ ] 4.5 Add one loop-based case to `app/FroggersMidiCatalogTests.cpp`
      asserting, for every entry in `catalog.deviceDefaults` whose id is not
      the Twister's, that no association carries a `shiftedPress` and no
      association has `press.type == synth::MessageIn::Type::Shift`. Prove the
      positive control: giving any non-Twister default a shifted press must
      turn this case red. Does not depend on 4.1/4.2's gate: it reads
      `synth_froggers::FroggersMidiCatalog()`'s live `catalog.deviceDefaults`
      each time the binary runs, not a list frozen at authoring time, so it
      covers whatever the catalogue holds at test-run time — six today,
      automatically seven once task 5.1 appends the Launch Control XL, with no
      further edit to this case (see task 5.3).
- [ ] 4.6 Add two cases to `app/FroggersMidiCatalogTests.cpp`, both against
      `SystemButtonMidiInProcessor::Process`
      (`External/Sheaf/projects/synth/src/MidiController.cpp:932-976`), which
      dispatches on the press edge before it ever inspects
      `association->release` — neither depends on 4.1/4.2's gate. (a) A
      non-Shift Twister side button's press message, with no paired release
      ever arriving, still dispatches its currently-applicable job (ordinary
      or shifted, per whatever Shift's own held state is at the time) — the
      job is never silently dropped. (b) When the button whose press-with-no-release
      is the Shift button itself, `shift_->modifier.held` is left `true`
      (`:969`) and every subsequent press on the other five side buttons then
      dispatches its **shifted** job (`:975`), not its ordinary one, until
      cleared by one of `HeldModifierClearSource`'s other four triggers — this
      case asserts the shifted dispatch, not an unshifted one, and exists so
      the requirement's guarantee is checked for both the button whose own
      precondition is unmet and for the other five when Shift's is. Do not
      assert or assume what a real Twister transmits when CC Hold is off the
      device (no artifact in this change or Sheaf's states that); both cases
      construct the press-with-no-release input directly, the same input
      shape a mismatched device precondition happens to be capable of
      producing, without a hardware claim about which device states produce
      it.
- [ ] 4.7 Generate, from the declarations, the per-device settings prose for
      every device default that receives one at this point in the sequence:
      the Twister's (`MANUAL.md:336-339`), the APC40 Generic's Track-1 caveat
      (currently prose inside `:341-351`), and the APC40 Ableton's explicit
      empty-preconditions statement (`:353-357`, which today names no
      precondition at all — this task makes that silence an intentional,
      generated statement rather than an accident). The Launch Control XL's
      section is written by task 5.6, once its default exists, and is subject
      to the same generation and the same check (task 4.8) — it is not a
      fifth device this task populates now. The three Launchpad sections
      generate nothing (their declared-preconditions lists are empty and
      their sections name no device-setting-shaped sentence today); task 4.8's
      check confirms that agreement too, not just the four with content.
      Name each device's section by its own `###` heading
      (`### MIDI Fighter Twister`, `### Akai APC40 mkII (Generic)`,
      `### Akai APC40 mkII (Ableton)`), not by the line numbers above, which
      this task's own edits shift.
- [ ] 4.8 Create NEW `app/check_docs_match_device_preconditions.py`,
      implementing exactly the rule, floor and positive controls design.md's
      "The drift check has two independent halves" states — read that section
      before writing this script; nothing here restates it. In outline: (1)
      the drift half parses `declaredPreconditions` from
      `app/FroggersMidiCatalog.hpp` per device-default factory function and
      each device's `MANUAL.md` subsection by heading, requires finding all
      seven devices on both sides before comparing anything, and fails on any
      per-device mismatch; (2) the recovery half parses the `### Shift` and
      `### Hold Drill` subsections and fails unless each contains at least one
      of the specific multi-word recovery phrases task 3.1's rewrite uses.
      Run and record, before task 3.1 or 4.3-4.7 change anything: against the
      current, unmodified `MANUAL.md:319-320` (only "pressed and released
      again," plus the bare word "unplugged" describing the failure, not a
      recovery), the recovery half must fail; against the current
      `MANUAL.md:308-313` (no recovery sentence at all), it must also fail,
      for the "heading found, no phrase present" reason. Prove both drift-half
      positive controls design.md states (a temporary edit to a declared
      string, and a temporary edit to the matching manual sentence, each
      independently turning the check red) and revert both edits immediately
      after confirming red. `spec.md`'s "SHALL NOT depend on a device setting
      it does not declare" ships unchecked this cycle — design.md's final
      paragraph states why; this task does not attempt a check for it. Wire
      this script into `app/Makefile` beside the twelve existing `check-*`
      targets.

## 5. The Launch Control XL preset

Task 2.1 found a source for the fader map (Ableton Live 12 Suite's
control-surface script), so these tasks execute in this delivery. Task 5.2's
declared-preconditions population depends on the same submodule-pin gate as
group 4 (tasks 4.1-4.2), because it writes into the same
device_defaults_declare_their_preconditions case group 4 adds.

- [ ] 5.1 Add the device default: id `froggers.launchcontrolxl` (design.md,
      "The device default's `id` is `froggers.launchcontrolxl`," carries the
      grep against the six existing ids and the reasoning), `Generic` kind,
      input and output aliases `{"Launch Control XL"}` (Programmer's
      Reference Guide, "Launch Control XL MIDI Overview": "Launch Control XL
      has a single MIDI port named 'Launch Control XL n', where n is the
      device ID of your unit (not shown for device ID 1)" — unconfirmed on
      hardware, the same caveat MANUAL.md's Launchpad X and Pro MK3 sections
      carry), and an analog section assigning scene blend to CC 77, channel 8
      counted from 0 (task 2.2). The case this task adds is a
      literal-against-literal assertion (design.md, "What task 5.1's own
      check can and cannot prove") — it catches a later transcription slip,
      not a wrong reading of the device.
- [ ] 5.2 **Gated on 4.1-4.2**, the same submodule pin group 4's own
      declarations wait on. Declare factory template 1 as a
      `declaredPreconditions` entry, citing the Getting Started Guide's
      "Template Switching" procedure (page 5) as where an operator sets it,
      and add its assertions to the device_defaults_declare_their_preconditions
      case.
- [ ] 5.3 Once this default exists (5.1 has run), task 4.5's loop-based case
      needs no edit to cover it: that case iterates `catalog.deviceDefaults`
      at test-run time, so the same compiled case now also asserts, of the
      Launch Control XL entry, that it carries no shifted press or Shift
      press. No separate check is written for this device specifically — only
      re-running the existing `FroggersMidiCatalogTests` binary after 5.1 is
      needed to exercise it against the seventh entry.
- [ ] 5.4 Once the default exists (5.1-5.2), this is an **operator step**, not
      a build-time check: on the live browser site, select the Launch Control
      XL preset, move fader 1, and observe the on-screen scene blend value
      move — via `AnalogMidiInProcessor::Process`
      (`External/Sheaf/projects/synth/src/MidiController.cpp:847`) matching
      the incoming CC against `AnalogMidiInConfig::sceneBlend` and dispatching
      `MessageIn::SetSceneBlend`, rendered by `FroggersUiSurface.hpp`'s read of
      `context_->uiState->sceneBlend` into the `FroggersNodeIds::kSceneBlend`
      slider — the same code path the APC40 crossfader already exercises in
      production. There is no way to drive a physical fader from this
      repository's test binaries; this is what makes the observation
      meaningful rather than what a unit test would additionally prove.
- [ ] 5.5 When the default is added, name and update the three
      catalogue-count assertions this changes: `app/FroggersMidiCatalogTests.cpp:399`
      (`catalog.deviceDefaults.size() == 6`), `app/FroggersControllersPageTests.cpp:99`
      (`catalog.deviceDefaults.size() == 6`), and
      `app/FroggersControllersPageTests.cpp:109` (`registry.size() == 6`) —
      all three become `== 7`. Append the new entry after the Ableton default
      and before the Launchpads in `catalog.deviceDefaults`'s initializer list
      (`app/FroggersMidiCatalog.hpp:329-336`), not inserted elsewhere, because
      `real_catalog_registers_one_descriptor_per_device_default` pairs
      `registry[ix]` positionally against `catalog.deviceDefaults[ix]`. At the
      same time, update the two file-header comments that enumerate the
      catalogue's defaults by name and count: `app/FroggersMidiCatalog.hpp:6-12`
      ("the six device defaults offered from the Controllers page's Layout
      dropdown -- MIDI Fighter Twister, ...") and
      `app/FroggersControllersPageTests.cpp:1-4` ("FroggersMidiCatalog()'s six
      real device defaults"), both to seven, naming the Launch Control XL.
- [ ] 5.6 Add MANUAL.md's Launch Control XL section (after "Akai APC40 mkII
      (Ableton)" at `:353-357`, before "Launchpad X" at `:359`, matching the
      device-default order) and add "Novation Launch Control XL" to the
      Overview's Preset selector list (`:264-265`). Name fader 1 as scene
      blend and factory template 1 as the device precondition, citing the
      Getting Started Guide's "Template Switching" procedure as where it is
      set. This paragraph is subject to the same drift check task 4.8 adds.
      Also reword `MANUAL.md:271-272` ("A newly connected Twister, APC40, or
      Launchpad is also offered through the page's configure flow.") so it
      does not enumerate device families by name — the configure flow's list
      is built from `catalog.deviceDefaults` (`ControllerWizard.cpp:937-940`,
      `:962-963`; `real_catalog_registers_one_descriptor_per_device_default`
      already pins one descriptor per device default), so naming three
      families reads as exhaustive and a fourth (this change's own Launch
      Control XL) already falsifies it before this change even ships;
      describe the set as every cataloged device rather than by an
      enumeration this and future changes would have to keep editing. Also
      update `README.md:110-112` ("ready-made presets for the MIDI Fighter
      Twister, the Akai APC40 mkII, and three Launchpad models") to include
      the Launch Control XL, the same drift this section and the two header
      comments (task 5.5) exist to fix.

## 6. Postflight

- [ ] 6.1 Re-run 1.3's enumeration against the diff.
- [ ] 6.2 Re-run 1.7's §8.0 hygiene sweep against the change's final diff,
      naming each directory again.
- [ ] 6.3 Re-run every gate baselined in 1.6 — the twelve `check-*`
      prerequisites and the twelve test binaries, against the tree as this
      change leaves it — and report which moved and which were carried
      forward, measured now, not assumed from 1.6's record. This change adds
      one target (`check-docs-match-device-preconditions`) to the twelve;
      state what each of the other twelve reports, rather than asserting in
      advance that none of them moved. Task 1.2 confirms `check-artifact-symbols-resolve` is green
      as of the branch reaching `main`'s tip; this change's own new citations
      (design.md, tasks.md, the spec delta)
      must not have reopened it — confirm by re-running the check, not by
      inference. This change does not certify forward any failure it did not
      itself cause; a failure whose cause is outside this change's own
      directory and outside the causes 1.2/1.7 already own is a new fact,
      reported by name and location, not folded into "carried forward."
- [ ] 6.4 Every scenario in the delta either has a check that passes now, or
      says plainly it is not yet delivered and names what will deliver it.
- [ ] 6.5 Independent review with a fresh context.
- [ ] 6.6 **Deliver: push branch `worktree-midi-resilience` to `origin` and
      nothing else** — no push to `main`, no pull request, no submodule-pin
      change on any `main`. This delivery ships groups 1, 3, 4 and 5, once
      group 4's gate (4.1-4.2) is satisfied — the submodule checkout pinned to
      a Sheaf `fork/midi-resilience-merge` commit that carries
      `declaredPreconditions`, which group 5's own declared-preconditions
      population (task 5.2) also depends on. The operator performs the
      rebase and merge of `worktree-midi-resilience` into `main` afterward;
      this task does not do that.
