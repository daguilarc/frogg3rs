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
      removed, folded into this branch by the commit
      `git log --all --oneline | grep -i "Collect the MIDI resilience"` names
      (subject "Collect the MIDI resilience work into one worktree"), and, in
      the submodule, by the commit
      `git -C External/Sheaf log --all --oneline | grep -i "Carry the MIDI
      controller resilience"` names (subject "Carry the MIDI controller
      resilience change into the submodule"). Identify both by that subject
      search, not by a hex id recorded here: confirm each is this branch's
      own ancestor with `git branch -a --contains <the commit the search
      above found>` (must name `worktree-midi-resilience`) and, for the
      submodule, `git -C External/Sheaf branch -a --contains <that commit>`
      (must name `midi-resilience-merge`) — a hex id written into this task
      is reachable from no ref by the next rebase this branch or its
      submodule checkout undergoes, even though the fold itself is permanent
      history; the subject line is not. If either subject search returns no
      commit, or the containment check against the commit it finds names no
      branch, that is a different, unresolved state — STOP and report it
      rather than guessing which id is current.
- [ ] 1.2 **Confirm this branch is at `main`'s tip** before baselining, so
      1.6's baseline is not stale before this change's own diff starts. First
      confirm `main` itself still resolves: `git rev-parse --verify main`
      exits `0` — a renamed or deleted local `main` makes the count below read
      `0` vacuously (ancestry of nothing), which is a worse, different state
      than being caught up; STOP and report if this does not resolve, rather
      than trusting the count that follows it. Then check
      `git merge-base --is-ancestor main HEAD; echo $?` → `0`
      (equivalently, and naming which commits are missing when it is
      nonzero: `git log --oneline HEAD..main | wc -l` → `0`). If `main` is
      not an ancestor of `HEAD`, `main` has moved since this branch last
      rebased from it — a live tree this change's own artifacts do not
      control (see task 3.2 for the same fact about the coordination party
      `main` currently carries). This is a coordinator step, not a STOP:
      **the coordinator** runs `git rebase main` on this branch, resolves any
      conflict, and re-runs both checks above until each reports `0` /
      exits `0`; the executor does not perform the rebase itself and does not
      baseline against a `main` this branch has not yet absorbed. Once both
      checks pass, `openspec/changes/frogg3rs-delay-width-wysiwyg-repair/`
      does not exist in this tree (that change shipped and was archived as
      `frogg3rs-delay-capacity-and-width-finish`) — confirm with
      `ls openspec/changes/frogg3rs-delay-width-wysiwyg-repair` returning "No
      such file or directory" — and `python3 app/check_artifact_symbols_resolve.py app`
      exits `0` and prints `check-artifact-symbols-resolve: OK -`, with a
      "name(s) declared as not yet created" count that is not itself a
      pass/fail condition (it counts backticked *occurrences* of this
      change's own forward-declared names, not distinct names — `app/check_docs_match_device_preconditions.py`
      alone is named by task 1.8's own "Create NEW" phrasing, again by task
      4.8, and a third time by this very parenthetical, since a citation
      naming the script also counts as an occurrence; the count moves
      whenever this file's own wording does, including this sentence's own
      edits, which is why it is not recorded as a fixed number here either;
      only a nonzero count of *unresolvable* names,
      which the script reports as `FAIL` with a nonzero exit code, is a
      failure). Recording a specific number here for either script would be
      the same frozen-literal defect this task's own second half already
      guards against for `main`: the number of open `openspec/changes/`
      directories elsewhere in the tree changes it independent of anything
      this change does. If either check disagrees because `main` has moved
      since this was written, or because this change's own artifacts (not
      `main`) introduce a new unresolvable name, STOP and report the actual
      state — naming which of the two causes it is — rather than resolving by
      guesswork.
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
      ten must exit `0`. Read each one's actual invocation — its interpreter
      and any positional arguments — from its own recipe in `app/Makefile`
      rather than assuming a uniform `python3 <script> "$(APP_DIR)"` shape:
      three of the ten (`check-no-firmware-includes`, `check-microphone-usage`,
      `check-catalog-covers-screen-actions`) run under `bash`, not `python3`,
      the second names a script file (`check_microphone_usage_strings.sh`)
      this task does not otherwise give, and the third takes five positional
      action-name arguments (`kSceneBlend kEncoderPress kEncoderDrag
      kInputSelect kViewportNarrow`) that live only in the Makefile recipe and
      exits nonzero without them; the remaining seven
      (`check-docs-match-parameter-table`, `check-spec-checks-resolve`,
      `check-citations-resolve`, `check-no-planning-history`,
      `check-artifact-symbols-resolve`, `check-delay-capacity-parameters-are-swept`,
      `check-modified-requirements-restate-promoted`) run under `python3` with
      only `"$(APP_DIR)"` as their argument. Naming the recipe as the source of
      truth here, rather than restating each invocation, is what keeps this
      task correct if a script gains an argument later: `check-artifact-symbols-resolve` (exit `0`
      and an `OK -` line; see task 1.2 for why its "name(s) declared as not
      yet created" count is not itself recorded as a fixed number here),
      `check-delay-capacity-parameters-are-swept`, and
      `check-modified-requirements-restate-promoted` (exit `0` and an `OK -`
      line naming some number of MODIFIED requirements, promoted clauses,
      and declared edits — not recorded as fixed figures here, because this
      script's counts sum over every `openspec/changes/` directory open in
      the tree at run time, not only this change's own, and the number of
      other open changes is outside this change's control; re-run it rather
      than trusting a number written down before this task runs) — this
      last one also prints an informational `NOTE` naming the one promoted
      scenario this change's own spec delta does not fully restate; read that
      script's own header before treating a `NOTE` as a failure.
      `check-spec-checks-resolve` prints its own summary line in the form
      `check-spec-checks-resolve: OK - <N> Check reference(s) resolved, <M>
      declared as having no automated check`, counted over every
      `openspec/specs/` requirement in the tree, not only this change's own —
      record that exact line verbatim here as this task's baseline (at this
      writing: `check-spec-checks-resolve: OK - 55 Check reference(s)
      resolved, 18 declared as having no automated check`; re-run rather
      than trusting this parenthetical, since another change's own delta can
      move both figures before this task executes); task 6.6
      compares its own post-rewrite run of the same script against the `<N>`
      and `<M>` recorded here, so a number copied instead of measured at this
      point makes that later comparison meaningless. The remaining
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
- [ ] 1.8 **Create NEW `app/check_docs_match_device_preconditions.py` with its
      recovery half now, and run it, before task 3.1 or anything in group 4 changes
      `MANUAL.md` or the catalogue.** This half is pure text over `MANUAL.md`
      — it has no dependency on `declaredPreconditions` or the submodule pin
      (group 4's gate), so nothing here waits for it. Implement exactly the
      recovery-half rule design.md's "Recovery half — rule, floor, positive
      control" section states — three rules, all must pass, over the
      `### Shift` and `### Hold Drill` subsections, heading-delimited;
      fail naming the missing heading if either is not found at all:
      (1) **presence** — each subsection contains, for every trigger this
      change's own recovery text offers (Rebuild, Ceiling, and EndpointOpen,
      all three, unconditionally — `MANUAL.md` is one document read across
      every host, not partitioned by host, so no trigger is excused from this
      subsection on a per-host basis; the plugin exclusion is a separate,
      additional obligation, rule 3 below, not a condition on this rule), at
      least one of that trigger's own multi-word phrases, verbatim, as task 3.1's rewrite
      introduces them: Rebuild as "selecting a different preset" or
      "rebuilds the row's mapping"; Ceiling as "clears automatically after";
      EndpointOpen as "unplugging and reconnecting the controller". A
      subsection missing any one of these three phrase-families fails rule
      1 by name — a single phrase is not sufficient, because task 3.1's own
      instruction is to name all of them, not to pick one; (2) **absence**
      — neither subsection contains "pressed and released again" or any
      other sentence naming the modifier's own button as what clears it —
      implemented mechanically as literal-substring absence of "pressed and
      released again" (the exact current text, with no trailing comma); the
      broader instruction is a drafting rule for task 3.1's own prose, not a
      second mechanical pattern, because "any other sentence naming the
      modifier's own button" has no fixed substring to match and none is
      invented here; (3) **per-host coverage** — if a subsection contains
      any reconnect-shaped phrase (matched case-insensitively against
      `reconnect`, so "unplugging and reconnecting the controller",
      "reconnecting it", or any other phrasing built on the same word all
      trigger it, not only the one literal task 3.1 is instructed to write),
      that same subsection also contains the literal substring "not
      available in the plugin." Run it now, against the current, unmodified
      text, and record all results here: against `MANUAL.md:319-320`
      ("buttons stay shifted until Shift is pressed and released again."),
      rule 1 must fail (none of the three phrase-families is present, only
      the bare word "unplugged" describing the failure and "pressed and
      released again"); against that same unmodified sentence with all
      three presence phrases appended but "pressed and released again" left
      in place, rule 2 must fail — proving the absence rule actually rejects
      the sentence it exists to reject, not only the presence rule against
      different text; against `MANUAL.md:308-313` (no recovery sentence at
      all today), rule 1 must fail for the "heading found, no phrase
      present" reason; against a constructed subsection stating "unplugging
      and reconnecting the controller" with no "not available in the
      plugin" phrase anywhere in it, rule 3 must fail. If any of these four
      does not fail, the check tests nothing and this task is not done. The
      check does not assert a
      numeric duration for the Ceiling trigger's "clears automatically
      after" phrase — design.md's "The recovery text states no numeric
      duration for the Ceiling trigger" states why. Do not add the drift half here —
      that half reads `declaredPreconditions`, which does not exist until
      group 4's gate (4.1-4.2) lands it, and is added by task 4.8, which
      extends this same file rather than creating a second one. Wire only
      this half into `app/Makefile` for now; task 4.8 adds the drift half's
      own wiring once it exists.
      **This wires a check into `test:` that is knowingly red until task 3.1
      rewrites `MANUAL.md`** (rule 1 fails against the current, unmodified
      text by this same task's own design, above) — for the whole window from
      this task through task 3.1, `make -C app test` aborts at this new
      prerequisite before reaching any binary after it in the linear list,
      exactly as task 1.6 already states for any red `check-*` prerequisite.
      Task 1.6's own instruction carries forward through this window, not
      only at the moment 1.6 itself runs: for as long as this check is
      red, run each of the twelve test binaries by path directly whenever
      this change's own state needs confirming in that window, note which ran
      and which did not, and do not describe `make -C app test` as this
      change's own green gate until task 3.1 closes it.

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

- [ ] 3.1 **Gate.** This rewrite documents recoveries Sheaf's group 3 ("Held
      modifier lifetime") creates, not group 6 — do not start until Sheaf's
      `midi-controller-resilience` change is complete through its own task
      7.9, the same condition task 4.1 confirms (see task 4.1 for the exact
      commands: a clean tree at the commit Sheaf's own task 7.8 made, and the
      named Sheaf test binaries — covering `declaredPreconditions` and
      `HeldModifierClearSource`'s Ceiling and EndpointOpen triggers — built
      and passing at that commit, not a declaration or a ticked box a comment
      could also satisfy). This is the same
      submodule checkout and the same confirmation task 4.1 performs;
      confirming it once is sufficient, and this task does not require its
      own, separate submodule advance beyond what task 4.2 performs. Then:
      rewrite `MANUAL.md:319-320`
      (Shift) and add a matching recovery
      sentence to `MANUAL.md:308-313` (Hold Drill, which has none today), each
      naming which of `HeldModifierClearSource`'s five triggers are available
      on which host — traced from Sheaf's `midi-controller-resilience`
      design.md, not invented here: **Rebuild** (selecting a different preset,
      or otherwise causing the row's mapping to rebuild) is available on every
      host. **Ceiling** (an automatic elapsed-time clear) is
      available on every host, because `Engine::MessageThreadTick` — the
      pump that evaluates it — runs on all three (standalone's `Runtime.hpp`
      timer, the browser's `setInterval`, and the plugin's own JUCE timer);
      describe it as "clears automatically after" an elapsed time, WITHOUT
      stating a number of seconds — the duration the constant `grep -n
      kHeldModifierCeilingMicros External/Sheaf/openspec/changes/midi-controller-resilience/design.md`
      names (currently 30 seconds) is explicitly a reversible default that
      Sheaf's own
      change can tune before it ships, and design.md's "The recovery text
      states no numeric duration for the Ceiling trigger" explains why
      freezing whatever number is current into this manual would be the same
      class of stale figure this change avoids elsewhere by citing a command
      instead of a number (see, for instance, task 1.1's commit citations and
      task 3.2's coordination-party check).
      **EndpointOpen** (unplugging and reconnecting the controller) is
      available in the standalone and browser builds, which manage their own
      MIDI ports, and NOT in the plugin, which takes MIDI through host
      automation and opens no ports of its own — the binding that exists in
      code, per Sheaf's `midi-controller-resilience` design.md, "Clear both
      modifiers on endpoint open at the two bindings, and say where": "A host
      that embeds neither — the frogg3rs VST embeds no `MidiConnectionManager`
      at all — has the other four triggers and not this one, which the
      requirement states rather than assuming." This is the only host
      distinction EndpointOpen carries: the phrase itself is required in both
      manual subsections unconditionally (task 1.8's rule 1, below), and the
      plugin exclusion is a separate, additional sentence in the same
      subsection wherever the reconnect phrase appears (task 1.8's rule 3).
      **Release** and **SecondPress** both require the same button's own
      address to transmit again — the two triggers that are unavailable
      exactly when this recovery is needed, so they are not offered as the
      recovery, only named as what ordinarily ends a hold. Write the recovery
      as an explicit list of actions using multi-word phrasing (e.g.
      "selecting a different preset", "unplugging and reconnecting the
      controller", "clears automatically after" with no number attached), not
      a bare
      mention of "unplugged" — task 1.8's check (already authored and proven
      red against today's text before this task runs) matches on these
      specific
      phrases, and a bare word is exactly what the current, unmodified text
      already has without stating a recovery (design.md's recovery-half rule
      explains why). Also do not carry forward the sentence task 1.8's check
      rejects — "pressed and released again," or any other wording that
      names the modifier's own button as what clears it — the rewrite
      replaces that sentence, it does not add beside it. Do not describe the
      plugin as having the reconnect route: wherever this rewrite states
      "unplugging and reconnecting the controller" for a subsection, that
      same subsection must also contain the literal phrase "not available in
      the plugin," naming the plugin specifically (e.g. "...unplugging and
      reconnecting the controller, on the standalone and browser builds —
      not available in the plugin, which takes MIDI through host automation
      and opens no ports of its own") — task 1.8's own third check rule
      fails a subsection that offers the reconnect phrase without this
      exclusion.
- [ ] 3.2 Coordinate every `MANUAL.md` edit with whatever change currently
      holds it. `ls /Users/diegoaguilar-canabal/Desktop/frogg3rs/openspec/changes/`
      names the live coordination party at the time this task runs — at this
      writing it names `frogg3rs-envelope-curve-direction`, whose Delivery
      section commits to one commit and a push to `main` (no pull request),
      after which it messages this worktree's own session that `main` is
      final so it can rebase (task 1.2 performs that rebase; a push this
      branch has not yet absorbed is what task 1.2's rebase step exists for,
      not a defect this task reports). Because the set of open changes on
      `main` is not fixed, do not assume the name recorded here is still the
      live party; re-run the `ls` and read whichever directory it names.
      Traced directly on the main checkout, not assumed:
      `git -C /Users/diegoaguilar-canabal/Desktop/frogg3rs status --short MANUAL.md QUICK_DICT.md`
      — two dispositions, not one:
      - **Empty output.** No coordination party currently holds an
        uncommitted edit to either file; proceed, then diff-review
        `MANUAL.md` section-by-section before staging and never stage a
        whole-file `git add` while any change named by the `ls` above could
        still be editing it.
      - **Non-empty output.** Read
        `git -C /Users/diegoaguilar-canabal/Desktop/frogg3rs diff -U0 MANUAL.md QUICK_DICT.md`
        and confirm every changed hunk's line range is disjoint from this
        change's own `MANUAL.md:258-390` (this change touches no
        `QUICK_DICT.md` line at all — see below). Disjoint ranges are safe
        to proceed against, diff-reviewed the same way; a hunk that overlaps
        `MANUAL.md:258-390` is a real collision — STOP and report the
        overlapping range rather than merging past it. (Measured at this
        writing against `frogg3rs-envelope-curve-direction`'s own edit:
        `git -C /Users/diegoaguilar-canabal/Desktop/frogg3rs diff -U0 MANUAL.md | grep '^@@'`
        reports `@@ -452,3 +452,6 @@` and `@@ -579,2 +582,6 @@`, both
        disjoint from `:258-390`; re-run rather than trusting this
        parenthetical, since the party and its edit can both change before
        this task executes.)
      `QUICK_DICT.md` carries no MIDI content for this change to edit
      (`grep -in "midi\|shift\|hold drill\|twister\|apc40\|launchpad" QUICK_DICT.md`
      returns nothing) — this change does not touch it, so any coordination
      party's edit to that file, whatever it changes, cannot collide with
      this change's own edits by construction.

## 4. Declared preconditions

- [ ] 4.1 **Gate.** Do not start the rest of this group — **except tasks 4.5
      and 4.6, which do not depend on this gate and may run before it, as
      each states at its own site** — until Sheaf's
      `midi-controller-resilience` change — executed in `External/Sheaf`'s
      submodule checkout, this worktree's own copy, on branch
      `midi-resilience-merge` — is **complete through its own task 7.9**, not
      merely through the three tasks (6.1, 6.2, and group 3's task 3.1) that
      happen to add the two symbols this group needs. Completeness is a
      property of a commit, not of the working tree, because the working
      tree and a commit are different objects and only a commit is what a
      submodule gitlink can name. Confirm the following against the checkout
      as it actually stands:
      1. `git -C External/Sheaf status --short` reports nothing (a clean
         tree — the same condition Sheaf's own task 7.8 confirms before its
         coordinator commits), and `git -C External/Sheaf rev-parse HEAD`
         names the exact commit Sheaf's own task 7.8 made (its coordinator
         ticks 7.7, 7.8 and 7.9 themselves in the working tree before
         committing, per that task's own stated convention) — record this
         SHA; it is what 4.2 checks out and what 4.2's own commit message
         names.
      2. Build and run, by path, every Sheaf test binary that carries a case
         this change's own group 4 or task 3.1 depends on, and confirm each
         named case reports `[PASS]` (or the equivalent this repository's own
         test runner uses) at that commit — an assertion in a real test
         binary that this gate's own current, unmet state already turns red
         (today none of these files or cases exist, so the build itself
         fails; that failure is this gate's own positive control, not a
         second one to construct) is what actually confirms delivered
         behaviour, not a declaration or a comment a text probe cannot tell
         apart from real work:
         - `$(ENGINE_TEST_BIN)` (`projects/synth/tests/engine_tests.cpp`):
           `HeldModifierCeilingIsWallClockNotPumpCount`,
           `HeldModifierCeilingDoesNotFireBeforeItElapses` (Ceiling).
         - `$(BROWSER_MIDI_BRIDGE_TEST_BIN)`
           (`projects/synth/tests/browser_midi_bridge_tests.cpp`):
           `EndpointOpenClearsHeldModifierThroughTheAbiEntryPoint`,
           `ReconcileAloneDoesNotClearAHeldModifier` (EndpointOpen, browser
           binding).
         - The `projects/synth/apps/miniapp/Makefile` `test` target's own
           `MidiConnectionManagerReconcileTests.cpp`:
           `EndpointOpenClearsHeldModifierAtRuntimeBinding` (EndpointOpen,
           runtime binding — this is the miniapp target specifically; the
           top-level `projects/synth` `test` target does not build this
           file).
         - `$(CONTROLLER_WIZARD_TEST_BIN)`
           (`projects/synth/tests/controller_wizard_tests.cpp`):
           `AppDeviceDefaultCarriesItsDeclaredPreconditionsOntoItsDescriptor`
           (`declaredPreconditions` itself, the field this group populates).
         Each of these binaries and Makefile variables is Sheaf's own, named
         by its own tasks (group 3 for the first three, group 6 for the
         fourth) — read that repository's own `Makefile`s for the exact
         invocation rather than assuming a uniform build command, the same
         discipline task 1.6 applies to this repository's own gates. If any
         binary fails to build, or builds but any named case does not report
         `[PASS]`, this gate does not pass, regardless of what the tasks.md
         box count or the commit's own tick state says: a ticked box is not
         evidence of delivered behaviour, and this gate no longer reads one.
      This gate does not name or depend on any remote branch or a push:
      Sheaf's own task 7.9 states that this change performs no push, opens no
      pull request, and moves no pin in this cycle (its later,
      operator-driven rebase-and-merge step does that, under whatever branch
      name that step uses). Per the operator's decision that this delivery
      cycle ends at a push and nothing more (design.md and task 6.7), the
      commit this gate confirms and 4.2 pins is reachable from no remote
      until the operator's later merge — that is expected, not a defect this
      task can fix — and until then it exists in exactly one object store on
      this machine (see task 4.2).
- [ ] 4.2 **Submodule pin advance — the last task in this change that leaves
      a lasting change on `External/Sheaf`'s checkout.** (Task 4.6's positive
      control temporarily edits a file in that checkout and reverts it before
      the case is left passing; that revert-before-leaving is exactly what
      keeps it from being a second such task — this task's own pin is the
      only edit to that checkout that survives past the task that makes it.)
      `git -C External/Sheaf checkout <the
      commit 4.1 confirmed> && git add External/Sheaf`, naming the exact
      commit SHA in the commit message — no `fetch` is needed for *staging*
      this pin, since 4.1 confirmed the commit from this worktree's own
      already-present `Sheaf` checkout, not from a remote ref (a fetch is
      needed later, by whoever resolves the pushed gitlink, which is the
      operator's own merge step, not this task). This checkout detaches
      `External/Sheaf`'s `HEAD` from `midi-resilience-merge`; that is the
      normal, expected state for a pinned submodule gitlink (it is not a
      moving branch reference), the `midi-resilience-merge` branch ref itself
      is untouched by this checkout, and nothing here needs to "return" the
      submodule to it. Because task 4.1 confirmed Sheaf's change is complete
      through its own task 7.9 before this task runs, no Sheaf task still
      needs this checkout on its branch — the detach costs nothing this
      cycle, unlike a mid-change pin, which would leave a later Sheaf commit
      landing on a detached `HEAD` its own branch ref never sees. This
      is the only task in this change that moves the pin; nothing else does.
      **The commit this stages is reachable from no remote until the
      operator's later merge, and until then exists in exactly one object
      store on this machine** — this worktree's own submodule store,
      `.git/worktrees/midi-resilience/modules/External/Sheaf` (confirm with
      `cat External/Sheaf/.git`); the main checkout's own submodule store,
      `.git/modules/External/Sheaf`, cannot resolve this commit
      (`git -C /Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf
      cat-file -e <the pinned SHA>` exits non-zero). **No step before the
      operator's merge may remove this worktree or otherwise destroy that
      object store** — see task 6.7 for the same statement at delivery time.
      **Re-run `python3 app/check_citations_resolve.py app` immediately after
      this pin advance and before continuing.** That script verifies a cited
      line number falls within the pinned tree's current file length; it
      cannot tell a citation that still happens to be in range from one that
      now points at different code, so a citation into `External/Sheaf` this
      change's own artifacts wrote against the *old* pin — this change's own
      `design.md` and task 4.6's citations of
      `External/Sheaf/projects/synth/src/MidiController.cpp:932-976`,
      `:964-971`, `:969` and `:975` foremost among them, since Sheaf's own
      task 3.1 renames and moves code around those exact lines — needs a
      by-hand re-check against the new pin, not only the script's exit code.
      Re-read each such citation against the freshly pinned commit and
      correct any that moved before task 4.6 or anything later relies on
      them.
- [ ] 4.3 Populate the Twister default's `declaredPreconditions` from the
      precondition sentences at `app/FroggersMidiCatalog.hpp:14-19` (relative
      encoders, all six side buttons on CC Hold, "Bank Side Buttons"
      unchecked — the third precondition's own sentence ends mid-line at
      `:19`, "...addresses whatever Twister bank is lit."), and reword the
      surviving comment, which begins on that same line 19 ("The six side
      buttons are five paired jobs...") and runs to `:24` (the button-layout
      description, which is not a precondition and is not
      deleted) so it does not restate the declared settings and states CC
      Hold's reason as "what makes every side button address CC
      127-on-press/0-on-release instead of the factory bank-switch behaviour
      the middle pair defaults to; for Shift specifically, that release is
      also the promptest of the triggers that end a held modifier" rather
      than "what ends Shift." Add a
      device_defaults_declare_their_preconditions case to
      `app/FroggersMidiCatalogTests.cpp` asserting the Twister's three
      declarations.
- [ ] 4.4 Populate both APC40 mkII defaults' `declaredPreconditions`, which are
      **not the same list**: the Generic default declares the Track 1 caveat
      from the comment block at `app/FroggersMidiCatalog.hpp:26-35` (the
      Generic default's own sentence ends mid-line at `:30`, "...pressed
      again."; the Ableton default's sentence begins on that same line and
      runs to `:35` — read only the Generic half for this default's
      declaration); the Ableton default declares an
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
      positive control specifically on the **last** entry in
      `catalog.deviceDefaults` at the time this task runs (today, Launchpad
      Mini MK3) — not an arbitrary non-Twister default — since a loop bug that
      drops the final iteration is exactly the shape a control on a middle
      entry would miss. Task 5.3 re-proves this same control against the
      Launch Control XL specifically, once it exists, because that device is
      not yet in the catalogue for this task to target. Giving the targeted
      default a shifted press must
      turn this case red, then revert. Does not depend on 4.1/4.2's gate: it reads
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
      is the Shift button itself, `shift_->held` is left `true`
      (`:969`, current pinned-tree spelling — Sheaf's group 3 renames this to
      `shift_->modifier.held` once its task 3.1 lands, a private member no
      case outside `MidiController.cpp` can name either way; this case does
      not construct a `SystemButtonMidiInProcessor` and inspect its private
      state under either spelling, so it is unaffected by the rename) and
      every subsequent press on the other five side buttons then
      dispatches its **shifted** job (`:975`), not its ordinary one, until
      cleared by one of `HeldModifierClearSource`'s other four triggers — this
      case asserts the shifted dispatch on the message bus, never the private
      `held` flag under either of its spellings, and exists so
      the requirement's guarantee is checked for both the button whose own
      precondition is unmet and for the other five when Shift's is. Do not
      assert or assume what a real Twister transmits when CC Hold is off the
      device (no artifact in this change or Sheaf's states that); both cases
      construct the press-with-no-release input directly, the same input
      shape a mismatched device precondition happens to be capable of
      producing, without a hardware claim about which device states produce
      it. Prove two positive controls for both cases, because
      `SystemButtonMidiInProcessor` is a stateful object and one control
      alone defends only this task's own prescribed implementation, not the
      scenario's actual claim: (1) temporarily gate the `isPress` dispatch in
      `SystemButtonMidiInProcessor::Process` on `association->release.has_value()`
      (i.e. require a release before dispatching) and confirm both cases turn
      red, then revert — without this, both cases are green on the
      unmodified tree with no demonstrated failure mode against the shape
      this task prescribes; (2) add a test-local wrapper of
      `SystemButtonMidiInProcessor` (`MidiController.hpp:387` declares it
      `final`, so no subclass route exists — the wrapper composes an
      instance rather than inheriting from it) that stashes a press whose
      release has
      not yet arrived and replays it only from a later `Process()` call that
      carries the matching release, and confirm both cases turn red against
      it, then discard the wrapper — it exists only to drive this control,
      not to ship. Control (1) alone cannot distinguish "no implementation
      can defer dispatch to a later release" from "this task's own
      prescribed implementation does not defer it": a stateful processor can
      buffer a pending press internally and dispatch it only once a matching
      release arrives, which control (1)'s config-field gate never
      constructs and control (2) does. Both controls, not either alone, are
      what this task's own `Check:` lines cite as regression tripwires
      against the shape this task ships — control (2) is the one that
      exercises the scenario's own wording (dispatch appears on the message
      bus immediately after the press, before any release is sent), and
      control (1) is the cheaper, narrower one against this task's exact
      diff.
- [ ] 4.7 Generate, from the declarations, the per-device settings prose for
      every device default that receives one at this point in the sequence:
      the Twister's (`MANUAL.md:336-339`), the APC40 Generic's Track-1 caveat
      (currently prose inside `:341-351`), and the APC40 Ableton's explicit
      empty-preconditions statement (`:353-357`, which today names no
      precondition at all — this task makes that silence an intentional,
      generated statement rather than an accident). Wrap the generated
      sentence(s) for each of these three devices, AND a generated
      (necessarily empty) region for each of the three Launchpad defaults, in
      an HTML-comment marker pair keyed by that device's own `id` —
      `<!-- declaredPreconditions:froggers.twister -->` ...
      `<!-- /declaredPreconditions:froggers.twister -->`, one pair per entry
      in `catalog.deviceDefaults` that exists at this point (six; the seventh,
      the Launch Control XL, is task 5.6's) — so task 4.8's drift half has an
      exact, delimited region to compare against rather than free prose. The
      Launch Control XL's own marker pair and section are written by task 5.6,
      once its default exists, and are subject to the same generation and the
      same check (task 4.8) — it is not a fifth device this task populates
      now. The three Launchpad sections' marker pairs bound explicitly empty
      generated text (their declared-preconditions lists are empty); task
      4.8's check confirms that agreement too, not just the three with
      content.
      Name each device's section by its own `###` heading
      (`### MIDI Fighter Twister`, `### Akai APC40 mkII (Generic)`,
      `### Akai APC40 mkII (Ableton)`), not by the line numbers above, which
      this task's own edits shift; the marker pair, keyed by `id`, is what
      task 4.8 actually matches on, not the heading text.
- [ ] 4.8 Extend `app/check_docs_match_device_preconditions.py` (task 1.8
      created it with only the recovery half) with the drift half,
      implementing exactly the rule, floor and positive controls design.md's
      "The drift check has two independent halves" section states — read that
      section before writing this script; nothing here restates it. In
      outline: add a small emitter binary, named by a new `app/Makefile`
      variable (`DEVICE_PRECONDITIONS_EMITTER_BIN`, following the existing
      `$(MIDI_CATALOG_BIN)`/`$(CONTROLLERS_PAGE_BIN)` naming and build-rule
      shape — one `.cpp` source, linked against `$(SHEAF_LIB)` the same way),
      built against
      `app/FroggersMidiCatalog.hpp` the way `app/FroggersMidiCatalogTests.cpp`
      already is, that prints each entry in the live `catalog.deviceDefaults`
      (its `id` and its `declaredPreconditions` list) to stdout in the fixed,
      parseable format design.md's "the alternative this task adopts"
      section describes — this is the drift
      half's only source for both the expected values and the floor count
      (`len(deviceDefaults)`, not a hard-coded seven: today six, seven once
      task 5.1 runs). `check-docs-match-device-preconditions`'s own
      `app/Makefile` recipe depends on `$(DEVICE_PRECONDITIONS_EMITTER_BIN)`
      as a prerequisite, the way `check-no-juce` depends on
      `$(NO_JUCE_CHECK)` — this is the first `check-*` target in this
      repository needing a build, so it is wired as a dependency, not run
      standalone before the check script; the script itself invokes the
      built binary by its `$(BUILD_DIR)`-relative path as a subprocess and
      parses its stdout, the same way it reads `MANUAL.md`'s marker regions,
      rather than re-implementing any part of the catalogue in Python. The check regenerates each device's expected settings
      text from the emitter's output and compares it byte-for-byte against
      the `<!-- declaredPreconditions:<id> -->` ... `<!-- /declaredPreconditions:<id> -->`
      region task 4.7 (and 5.6) wrapped in `MANUAL.md`, requiring exactly one
      marker pair per compiled device default before comparing anything, and
      failing on any mismatch, naming the device id and what differed. Run and
      record, now, against the tree as it stands after task 4.7 (six devices,
      six marker pairs, no Launch Control XL yet): the drift half must pass.
      Also record what it does before task 4.7 runs (already true, but state
      the observed fact here): zero marker pairs against a compiled catalogue
      of six fails on the floor, naming the missing ids — the drift half's own
      "before" state, distinct from the two positive controls below. Prove
      both drift-half positive controls design.md states: (a) temporarily
      append a word to the Twister's declared CC Hold string in
      `app/FroggersMidiCatalog.hpp` (e.g. `"CC Hold, permanently"`), rebuild
      the emitter, and confirm the check turns red because the checked-in
      region still reads the old text; (b) temporarily edit the text inside
      `MANUAL.md`'s Twister marker pair itself (not merely somewhere in that
      subsection) to say "all six side buttons set to CC Toggle" in place of
      "CC Hold", and confirm the check turns red because the checked-in region
      no longer matches what the emitter currently produces. Revert both
      immediately after confirming red; neither ships. `spec.md`'s "SHALL NOT
      depend on a device setting it does not declare" ships unchecked this
      cycle — design.md's final paragraph on it states why — but add the
      best-effort WARNING scan design.md's rewritten section describes (the
      five-word marker set plus `keep`/`leave`/`off`/`template`, run over each
      subsection's prose *outside* its marker pair, reported as a warning, not
      a failure) as cheap, disclosed-as-a-heuristic coverage for it. Wire the
      now-complete script into `app/Makefile` beside the twelve existing
      `check-*` targets, replacing task 1.8's recovery-half-only wiring.

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
      needed to exercise it against the seventh entry. As this task's own
      positive control (task 4.5's own control, proven earlier, targeted the
      then-last entry, Launchpad Mini MK3 — it could not target a device that
      did not exist yet), temporarily give the Launch Control XL default
      itself a shifted press or a `Type::Shift` press, confirm the loop case
      turns red, then revert. Without this, nothing in this change's own
      verification surface ever gives a shifted press specifically to the
      device this group adds.
- [ ] 5.4 Once the default exists (5.1-5.2) **and, separately, once the
      operator's later rebase-and-merge has published the site this step
      observes** (task 6.7: the branch this delivery pushes carries a
      submodule pin reachable from no remote until that merge, and
      `.github/workflows/pages.yml` builds the live site from `main` on
      push), this is an **operator step**, not
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
      meaningful rather than what a unit test would additionally prove. Doing
      this against a preview/staging build before the merge would still
      exercise the code path, but would not confirm the value this catalogue
      entry ships to the published site, since the submodule pin the preview
      would use is the same not-yet-merged one.
- [ ] 5.5 When the default is added, name and update the three
      catalogue-count assertions this changes: `app/FroggersMidiCatalogTests.cpp:399`
      (`catalog.deviceDefaults.size() == 6`), `app/FroggersControllersPageTests.cpp:99`
      (`catalog.deviceDefaults.size() == 6`), and
      `app/FroggersControllersPageTests.cpp:109` (`registry.size() == 6`) —
      all three become `== 7`. Append the new entry after the Ableton default
      and before the Launchpads in `catalog.deviceDefaults`'s initializer list
      (`app/FroggersMidiCatalog.hpp:329-336`), not inserted elsewhere, because
      `real_catalog_registers_one_descriptor_per_device_default` pairs
      `registry[ix]` positionally against `catalog.deviceDefaults[ix]`, and,
      more specifically, `FroggersMidiCatalogTests.cpp:416-418` binds
      `deviceDefaults[0]`/`[1]`/`[2]` to the Twister/Generic/Ableton defaults
      by fixed index — appending after the Ableton default and before the
      Launchpads is what keeps those three indices unchanged; inserting
      anywhere before index 2 would shift them and fail that binding, not
      only the registry pairing.
      This change's own device_defaults_declare_their_preconditions case
      (tasks 4.3-4.4, 5.2) and
      `real_catalog_defaults_generate_and_accept_adds_through_the_view_model`
      (`app/FroggersControllersPageTests.cpp`) both iterate
      `catalog.deviceDefaults`/`registry` by their live size, not a literal
      count, so the seventh default is exercised by the wizard, `GenerateProfile`,
      `AddController`, and every `AddSingle`/`AddBlock` call across the
      Encoders/SystemMessages/Analogs sections — including
      `GenerateCatalogSlots`, which that case drives — with no edit needed to
      either case beyond what tasks 4.3-5.2 already make. A third case also
      consumes the live catalogue without needing an edit here:
      `launchpad_presets_pair_with_the_port_names_a_host_reports` builds a
      catalogue-derived `registry`, but asserts against a local `kPortCount`
      array literal, not against `catalog.deviceDefaults`'s own size, so the
      seventh default reaches it too without becoming a fourth count this
      task would need to rename. Name all three here because none is a count
      assertion this task changes, and none was
      previously named anywhere in this change's own artifacts even though
      the seventh default exercises all three. At the same time, update the two file-header comments that enumerate the
      catalogue's defaults by name and count: `app/FroggersMidiCatalog.hpp:6-12`
      ("the six device defaults offered from the Controllers page's Layout
      dropdown -- MIDI Fighter Twister, ...") and, in
      `app/FroggersControllersPageTests.cpp`'s header, BOTH count statements —
      not only `:1-4` ("FroggersMidiCatalog()'s six real device defaults") but
      also `:7` ("so none of them ever drive these six shipping defaults"),
      the same "six" repeated a second time in the same comment block — both
      to seven, naming the Launch Control XL. Two more prose sites state the
      same count and are not build-verified assertions, so nothing else
      would ever catch them going stale: `app/Makefile:127` ("...
      `synth_froggers::FroggersMidiCatalog()`'s six real device defaults...")
      and `app/FroggersMidiCatalogTests.cpp:8` ("...and the three device
      defaults validate against the library's per-kind support...") — the
      latter is **already false today**, having drifted to "three" while its
      own `:399` assertion reads `6`; update both to "seven" (and to name the
      Launch Control XL where the surrounding sentence names the other
      devices), fixing the pre-existing drift at `FroggersMidiCatalogTests.cpp:8`
      as part of this same edit rather than leaving one wrong count for
      another cycle to find.
- [ ] 5.6 Add MANUAL.md's Launch Control XL section (after "Akai APC40 mkII
      (Ableton)" at `:353-357`, before "Launchpad X" at `:359`, matching the
      device-default order), wrapped in its own
      `<!-- declaredPreconditions:froggers.launchcontrolxl -->` marker pair
      the same way task 4.7 wraps the other six, and add "Novation Launch
      Control XL" to the
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
      one target (`check-docs-match-device-preconditions`, first wired by task
      1.8 with only the recovery half, extended by task 4.8) to the twelve;
      state what each of the other twelve reports, rather than asserting in
      advance that none of them moved. Task 1.2 confirms `check-artifact-symbols-resolve` is green
      as of the branch reaching `main`'s tip; this change's own new citations
      (design.md, tasks.md, the spec delta)
      must not have reopened it — confirm by re-running the check, not by
      inference. This change does not certify forward any failure it did not
      itself cause; a failure whose cause is outside this change's own
      directory and outside the causes 1.2, 1.7, AND 4.2's own submodule pin
      advance already own is a new fact,
      reported by name and location, not folded into "carried forward" — a
      test that regresses only because the pinned Sheaf commit changed
      underneath it is this change's own effect, not an unrelated one, and is
      reported as such rather than as background noise.
- [ ] 6.4 Every scenario in the delta either has a check that passes now, or
      says plainly it is not yet delivered and names what will deliver it.
- [ ] 6.5 Independent review with a fresh context.
- [ ] 6.6 **Rewrite every promoted `Check: not yet delivered` line before
      archiving — no unrewritten deferral survives into the permanent
      promoted spec.** `grep -hE '^[[:space:]]*-[[:space:]]*Check:' openspec/changes/frogg3rs-midi-preset-preconditions/specs/*/spec.md
      | grep -c 'not yet delivered'` names the count to rewrite (measure it
      now; do not carry forward a number recorded before this task runs). By
      the time this task runs, every task this change's own deferred `Check:`
      lines name (1.8, 4.3, 4.4, 4.5, 4.6(a), 4.6(b), 4.8, 5.1, 5.2) is
      ticked, so each such line is rewritten to name the test that now
      exists in the form `app/check_spec_checks_resolve.py` already accepts
      elsewhere in this repository — a repo-relative path indexed verbatim
      from the tree, plus the real case name it defines, after a colon (a
      bare basename is not accepted); the same form this delta's own
      already-resolved lines already use — not left reading
      "not yet delivered" against a task that is in fact done, which is
      false the moment it is ticked and would be promoted false into
      `openspec/specs/` at archive. This is this repository's own
      obligation, parallel to what Sheaf's task 1.9 rule 2(b) mechanises for
      its own deltas; frogg3rs's `check_spec_checks_resolve.py` does not
      short-circuit on the "not yet delivered" prefix the way Sheaf's does
      (its own header comment states why), so a line left unrewritten here
      is not caught by that script alone — this task is the mechanism, and
      the check below is what proves it ran.
      **Check:** `grep -hE '^[[:space:]]*-[[:space:]]*Check:' openspec/changes/frogg3rs-midi-preset-preconditions/specs/*/spec.md
      | grep -c 'not yet delivered'` reports `0` (whitespace-anchored, not
      `^- Check:`, so an indented deferred line cannot escape this count the
      way it escapes a stricter anchor), and
      `python3 app/check_spec_checks_resolve.py app` exits `0`. Verify by hand,
      for each of the nine rewritten lines, that the test it now names is the
      one the delivering task actually added — the file exists, the case name
      is defined in it, and it is the case that task's own diff introduced,
      not a pre-existing or unrelated one the script's own aggregate
      resolution cannot distinguish by name alone; this is not a second
      script, it is the same by-hand correctness the executor already owes
      for writing the line in the first place. No `<M>`/`<N>` count is
      asserted here: an aggregate figure over every `Check:` line in the tree
      can rise or fall for reasons this task does not cause (another change's
      own delta, a rewrite elsewhere), so it is not this task's own check to
      carry, and the two conditions above — the literal deferred-line count
      and the script's own exit code — are what a real break turns red.
      **Positive control, stated as the archive step's own precondition:**
      leave exactly one `not yet delivered` line unrewritten and confirm
      `grep -hE '^[[:space:]]*-[[:space:]]*Check:' openspec/changes/frogg3rs-midi-preset-preconditions/specs/*/spec.md
      | grep 'not yet delivered'` prints that line — its output must be
      **empty** before 6.7's archive runs; a nonempty output is 6.7's own STOP
      condition, not merely a warning here. Revert immediately after
      confirming the control fires.
- [ ] 6.7 **Deliver, in this order: postflight (6.1-6.5) passes → documentation
      hygiene this change owes → 6.6's Check-line rewrite passes →
      `openspec archive frogg3rs-midi-preset-preconditions -y`
      → commit → push branch `worktree-midi-resilience` to `origin` and
      nothing else.** No push to `main`, no pull request, no submodule-pin
      change on any `main`, no further rebase. This delivery ships groups 1, 3,
      4 and 5, once task 4.1's gate is satisfied — Sheaf's change confirmed
      complete through its own task 7.9 (from this worktree's own
      `External/Sheaf` checkout, not from any remote ref), carrying both
      `declaredPreconditions` (Sheaf group 6)
      and `HeldModifierClearSource` (Sheaf group 3) in the commit task 7.8
      made, and pinned by task 4.2 —
      which group 5's own declared-preconditions population (task 5.2) also
      depends on, and which task 3.1's manual rewrite (group 3) depends on
      too, under the same confirmation, not earlier. The `-y` flag skips
      `openspec archive`'s interactive confirmation prompts — it does not
      skip validation, and this task does not add `--skip-specs` or
      `--no-validate`. It is used here for one known, disclosed reason: task
      5.4 is by design an operator step sequenced after this delivery's own
      push, so it is unticked (`- [ ]`) when this task runs, and `openspec
      archive` would otherwise stop on an interactive "incomplete tasks
      remain, continue?" prompt for that single, expected exception. The tool
      itself never names an unticked task — its progress counter is a count,
      `total - completed`, printed (human mode only, and only without
      `--json`) as "Warning: N incomplete task(s) found. Continuing due to
      `--yes` flag." — so the check this task actually performs is run
      **before** invoking `openspec archive` at all, not read off its output
      afterward: `grep -n '^- \[ \] ' openspec/changes/frogg3rs-midi-preset-preconditions/tasks.md`
      must list exactly two lines, `5.4` and `6.7` (this task itself, unticked
      while it runs) — no other task number. If it lists a third, STOP and
      report which, before running the archive command at all. If this task
      runs interactively (no `--json`) and the tool prints its own "N
      incomplete task(s) found" line, confirm `N` equals `2` — the count
      above, not a name, but cross-checked against the enumerated set just
      read, not trusted alone. If this task runs non-interactively
      (`--json`), disclose plainly: with `--yes` set, the tool's JSON output
      carries no count and no name for this condition at all (verified
      against this repository's own installed `openspec archive`
      implementation) — the `grep -n` above, run immediately before the
      command, is the only verification available in that mode, not a
      fallback to a tool output that will not appear. Either mode, `-y`/`--yes`
      also answers a second, later prompt ("Proceed with spec updates?")
      silently, with no output naming it either — expected here, since this
      change's own delta does carry spec updates to apply, not a second
      condition to STOP on. A validation failure is reported by the tool
      itself, loudly, in either mode, and remains its own STOP.
      **The commit this pin
      advances to, and so the commit `worktree-midi-resilience` carries once
      pushed, is reachable from no remote until the operator's own later step,
      and until then exists in exactly one object store on this machine**
      (Sheaf's task 7.9 states the same fact from its own side; see task 4.2
      for the exact commands) — `git branch -r
      --contains <that commit>` against both `fork` and `origin` returns
      nothing today, and will keep returning nothing after this task's own
      push, exactly as it does for the Sheaf commit itself. **This worktree
      must not be removed, and `External/Sheaf`'s submodule object store
      inside it must not be destroyed, before the operator's later merge
      publishes both repositories' commits together** — `git worktree remove`
      of a checkout with a submodule destroys that submodule's object store
      irrecoverably; treat this as an absolute constraint on every worktree
      and submodule checkout this delivery depends on, not only a risk to
      note. A fresh clone or a
      CI checkout of `worktree-midi-resilience` (`.github/workflows/pages.yml`
      triggers on push to `main`, not to this branch, so this push alone does
      not run it) cannot resolve `External/Sheaf`'s gitlink until the
      operator's later rebase-and-merge publishes both repositories' commits
      together — that is expected under the operator's own ruling for this
      cycle, not a defect this task can fix, and task 5.4's live-site operator check is sequenced after
      that merge for exactly this reason. Archiving in this repository moves
      the change directory out of the tracked tree
      (`openspec/changes/archive/` is gitignored, `.gitignore:31`, zero
      tracked files today — confirm with `git check-ignore -v
      openspec/changes/archive/anything` (matches `.gitignore:31`) and
      `git ls-files openspec/changes/archive` returning nothing — so the
      archived copy lives only on this machine,
      not in the pushed commit); this is expected, not a loss to fix. Sheaf's
      own `midi-controller-resilience` change is not archived in this
      cycle regardless of what its own task 7.7 reports — that repository's
      archive is the operator's own upstream concern, sequenced after the
      two Sheaf changes 7.7 itself names, not this task's to perform or wait
      for. The
      operator performs the
      rebase and merge of `worktree-midi-resilience` into `main` afterward;
      this task does not do that, and does not wait for it.
