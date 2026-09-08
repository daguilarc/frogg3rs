# Tasks — `frogg3rs-randomize-bounds-and-parked-repairs`

Preflight revision 2026-09-07; executing. Tier per task: **H** = mechanical,
**S** = decides what something means. Builds under `nice`, `-j2` at most.

The working tree is clean: the halted change's partial execution was reverted
in `404e7db`, so this plan starts where it says it starts. All line numbers
below are reads of that tree at `dc367b5`, re-verified by the preflight.

Carried forward as already closed, not re-run: the helper is at
`app/FroggersModulation.hpp:1106`, its count loop at `:1158`, its four call
sites at `:1303` (`RandomizeBankLevel1Depths`), `:1494` (`RandomizePage`),
`:1584` and `:1619` (both `RandomizeAll`); `FroggersParameters.hpp:291`
(`kNumModulators = 15`). BEFORE histograms, re-measured by the preflight's
gate baseline on this tree (identical to the earlier record, so the fixture
RNG is seeded deterministically):
level-0 `P(0)=49.9% mode=0 mean=0.926`,
level-1 `P(0)=48.2% mode=0 mean=1.024`,
level-2 `P(0)=52.9% mode=0 mean=0.934`.

## 0. Hygiene — step zero (§8.0)

The sweep opens the change and what it finds is fixed inside it. The tree this
change touches is the proposal's Impact list. Name each as swept.

- [x] 0.1 Sweep those paths by CONCEPT, not by one spelling: "how many
      sources a randomize attaches", "which gesture randomizes Crispy", "what
      the randomize floor is", and (operator instruction) "which parameter is
      in which slot". Findings are the proposal's P5 and P10; each has a
      disposition in groups 2, 3, 4 and 8. Paths swept: `app/` (the two
      randomize files, the parameter table, the surface label table, the
      four check scripts and the `Makefile`), `app/dsp/` (read only),
      `openspec/changes/`, `openspec/specs/`, `README.md`, `MANUAL.md`,
      `QUICK_DICT.md`, `External/Sheaf` (read only: active changes and open
      PRs enumerated, no overlap).
- [x] 0.2 `openspec/specs/pair-ar-vcv-time-range/` — `openspec archive
      frogg3rs-omni-audit-repairs` aborts because that change's delta REMOVES
      all three of this spec's requirements, and a spec with zero requirements
      fails validation. pair-AR is gone from `src/core` (`58e232d`); the only
      remaining `app/` mentions are pinned `08b5fd3:` citations in comments,
      historical references to the reference commit, which stay. Delete the
      spec directory rather than leaving it empty — the inbound half of that
      deletion. Mentions enumerated by name and by path: the live spec
      itself; the delta; `frogg3rs-omni-audit-repairs/proposal.md:576` and
      `tasks.md:350` (both say the requirements were REMOVED — add to each,
      in one line, that the spec directory was deleted whole because openspec
      cannot archive a spec to zero requirements); this change's own text;
      `openspec/specs/mod-blend-semantics/spec.md:4,38` and
      `field-operator-doc-parity/spec.md:17` mention pair-AR as a concept, not
      this spec, and are governed by the omni-audit change's own deltas to
      those specs — nothing needed here.
      TRACED 2026-09-07; do exactly this, in this order:
        1. Delete `openspec/changes/frogg3rs-omni-audit-repairs/specs/pair-ar-vcv-time-range/`
           — the delta itself. Archive rebuilds the spec FROM the delta and
           validates the rebuilt result, so the delta is what produces the
           empty spec.
        2. Delete `openspec/specs/pair-ar-vcv-time-range/`.
        3. `openspec archive frogg3rs-omni-audit-repairs -y`. Expected:
           "Specs updated successfully", archived. DONE 2026-09-07: `Totals:
           + 3, ~ 3, - 1`, archived as `2026-09-07-frogg3rs-omni-audit-repairs`;
           `openspec validate --all --strict` 21 passed, 0 failed.
      Two things NOT to do, both tested and rejected:
        - Deleting only the live spec and archiving does NOT work — it aborts
          with the same error, because the rebuild comes from the delta and
          does not care whether the directory exists on disk.
        - `--skip-specs` DOES archive, and is a trap: it skips ALL spec
          updates, so that change's other six spec writes (including creating
          `frogg3rs-firmware-verification`) are silently discarded.
- [x] 0.3 `README.md:67` — "Because every **Crispy** knob is randomized by
      **Randomize All**, this is functionally the same outcome" is FALSE
      today: Randomize All excludes every Crispy
      (`app/FroggersModulation.hpp:1539`), the spec says it SHALL leave them
      untouched, and `MANUAL.md:162` agrees. Pre-existing, found by the sweep.
      Group 4 rewrites the same sentence for the new behaviour; 0.3 lands in
      the hygiene commit so no commit asserts something no version of the
      code did.
- [x] 0.4 After 0.2: `openspec validate --all --strict` clean and
      `frogg3rs-omni-audit-repairs` under `openspec/changes/archive/`.
- [ ] 0.5 Sweep report in the final message: each path named, each finding
      with a disposition, including "nothing needed".

## 1. Preflight — done 2026-09-07 (inline; findings are the proposal's P1-P12)

- [x] 1.1 Every citation in `proposal.md` resolves at `dc367b5`.
- [x] 1.2 Plan against ask: both asks quoted in the proposal; every floored
      or unfloored call site named with its enclosing function (P7).
- [x] 1.3 §5 forward on every concept, FOUND vs CHANGED (P4).
- [x] 1.4 Crispy count is the truncated geometric (P3).
- [x] 1.5 First run of every gate: `make -C app test` 12/12 binaries green
      on the clean tree (the before line); `openspec validate --all --strict`
      22 passed; VST `ctest` baseline is run by group 7 before the rebuild.

## 2. Floor the drilled-in Randomize All — H

- [x] 2.1 New `detail::DrawGeometricCount(synth::ParameterManager&, std::size_t minimum, std::size_t maximum)`
      in `app/FroggersModulation.hpp`, above `RandomizeParameterModulationDepths`:
      `count = minimum; while (count < maximum && manager.NextRandomCoin() >= 0.5f) ++count; return count;`
      No default on either bound. Its comment states the rule from proposal
      item 3: the two callers differ on both bounds and neither may be
      borrowed. New `detail::SwapInRandomPick(synth::ParameterManager&, std::span<std::size_t> pool, std::size_t i)`
      that does the three-line partial Fisher-Yates step from `:1165-1168`
      (`remaining = pool.size() - i; pick = i + NextRandomIndex(remaining);
      swap(pool[i], pool[pick])`) and returns `pool[i]`; the source loop
      calls it, keeping its materialize-and-break body exactly where it is.
- [x] 2.2 `RandomizeParameterModulationDepths` (`:1106`) gains
      `std::size_t minimumSources` as a third parameter, NO default. The
      count draw at `:1157-1160` becomes
      `DrawGeometricCount(manager, std::min(minimumSources, eligible.size()), eligible.size())`.
      The `eligible.empty()` early return stays ahead of it. Nothing else in
      the function changes.
- [x] 2.3 Pass `1` at `:1584` and `:1619` (the two halves of one drilled-in
      Randomize All press). Pass `0` at `:1303` and `:1494`, and say why at
      each: level 0 keeps the zero floor because a floor roughly doubles its
      depth allocation; Randomize Page keeps it because its contract is to
      randomize exactly what is displayed.
- [x] 2.4 `RequireGeometricCountDistribution` (`app/FroggersModulationTests.cpp:903`)
      gains a `floor` argument and asserts the shape from that floor: every
      bucket below `floor` exactly zero; `histogram[floor]` in [38%, 62%];
      mode == `floor`; P(count >= floor + 4) < 13%. THREE call sites: `:973`
      (level-0, floor 0), `:1044` (level-1, floor 1), `:1045` (level-2,
      floor 1).
- [x] 2.5 Rename the two tests whose names say `mode_two` (`:953`, `:986`) to
      name the distribution they assert. Correct the stale figures in the
      comment at `:801` ("20%"), `:804` ("~80%") and its "weighted table"
      phrase — all three. Rewrite `:787-789` (the app-side draw did not
      remove the no-op at level 0), `:895-896`, `:978-985` (the floor is now
      per gesture, so the level-1/2 test pins the floor-one shape, not
      "the same distribution at every level") and `:1049` (the helper is
      `RequireGeometricCountDistribution`).
- [x] 2.6 Positive control, both numbers reported. AFTER histograms beside the
      BEFORE ones above. level-0 MUST be unchanged (mode 0, P(0) about 50%);
      level-1 and level-2 MUST both show P(0)=0 and mode 1. If level-0 moved,
      the floor leaked into a zero site — stop.
      DONE 2026-09-07, AFTER: level-0 `P(0)=47.9% mode=0 mean=1.073` (a
      fresh sample of the unchanged draw: the Crispy draw now consumes the
      RNG ahead of the bank loop; the 2000-trial zero-rate test reads
      P(0)=48.65%); level-1 `P(0)=0% mode=1 mean=1.958`; level-2
      `P(0)=0% mode=1 mean=2.013`.
- [x] 2.7 Control on the untouched gesture:
      `randomize_page_mod_detail_moves_the_display_on_the_expected_fraction_of_500_trials`
      (`:812`, band at `:843-844`) must pass UNCHANGED, with no edit to it.
      It drives `RandomizePage` at drill level 1 — call site `:1494`. If it
      fails, the floor reached a gesture the ask did not name; stop and
      re-read 2.3. This test is the tripwire for the defect that superseded
      the previous change.
- [x] 2.8 Measure the allocation cost in the level-1 test (`:986`): add an
      `[OBSERVED]` line, in the file's existing convention, with the mean
      number of materialized depths per press on the focused parameter plus
      its sub-depths, and the fraction of presses that returned `partial`.
      If `partial` becomes common rather than rare, that is a finding this
      text does not resolve — report it and stop.
      DONE 2026-09-07 (measured as a per-press delta, since depth storage is
      never freed): newly materialized depths per press mean 0.45 over 500
      presses; within the open view the focused parameter's subtree is fully
      materialized (240 of 240 slots) by the end of the run; partial 0%.
      Traced 2026-09-07 at the operator's question: this is a within-view
      transient. Depth cells are pinned while the view is open; on Back,
      `Bank::Deselect` (`External/Sheaf/projects/synth/src/ParameterModulation.cpp:2704-2718`)
      unpins them and runs `CollectNeutralLocalParameters`, which recycles
      every unpinned neutral depth and sub-depth back to the pool
      (`CollectNeutralChildren`, `CanRecycleLocal`). The persistent
      footprint is only the modulating depths of the last press. The floor
      of one doubles that footprint and fills a view's subtree sooner; it
      changes nothing about reclaim, and the spec's allocated-once clause is
      honoured.
- [x] 2.9 Gate: `nice make -C app -j2 test`, every binary by path.

## 3. Randomize All draws up to two banks' Crispy — S

- [x] 3.1 Replace `randomize_all_leaves_local_crispy_alone_but_randomize_page_moves_it`
      (`:717-745`) with a test written first and run RED: over 600
      Randomize All presses on a parameter page with the fixture's seeded
      RNG, comparing each bank's Crispy `SceneCenter(0)` before and after
      every press, the number of banks whose Crispy moved is never above
      `detail::kMaxRandomizedCrispy`, is 0 on [38%, 62%] of presses, and
      every one of the six banks moves at least once across the run. Keep
      the old test's second half: Randomize Page on a parameter page moves
      that bank's Crispy and no other bank's. Today the count is always 0
      and no bank is reachable, so it must fail. A pass here voids the gate.
- [x] 3.2 In `RandomizeAll`'s level-0 branch (`:1528-1541`), before the bank
      loop: `count = DrawGeometricCount(manager, 0, kMaxRandomizedCrispy)`,
      then a `std::array<std::size_t, kFroggersBankCount>` pool of bank
      indices with `SwapInRandomPick` called `count` times, marking the drawn
      banks; the loop passes `includeCrispy` per bank from that mark.
      `RandomizeBankValues` (`:1263`) keeps its `bool` signature. Name the
      bound `inline constexpr std::size_t kMaxRandomizedCrispy = 2;` in
      `detail`; do not inline the literal, and the test reads the constant.
- [x] 3.3 Rewrite the flag comment (`:1254-1262`), `RandomizeAll`'s header
      line `:1500` ("Crispy included"), the `RandomizeBankLevel1Depths` tail
      (`:1305-1309`, Crispy depths "for the same reason its value is") and
      the new test's header to state the rule as it then is: the aggregate
      of all six is what reproduces Crunchy, so the count is bounded below
      it; Crispy depths stay out of Randomize All. Present tense, no earlier
      behaviour, no fix narrative.
- [x] 3.4 Gate; the observed Crispy-count distribution goes into the ledger.
      DONE 2026-09-07: over 600 presses `P(0)=48.8% P(1)=26.5% P(2)=24.7%`,
      never above two, every bank reached. RED control: with the draw
      reverted the new test failed at `countHistogram[0] < kTrials * 0.62`
      (600/600 zero). Gate after groups 2 and 3: 12/12 binaries green.

## 4. Documentation this change invalidates — S

Distinct from group 0: these are wrong BECAUSE of this change, not before it.

- [x] 4.1 `MANUAL.md:172-188` — the weighted table is presented as universal.
      It is correct for a page-level press and for Randomize Page at any
      level, and wrong for Randomize All at a drilled-in level, where zero
      becomes impossible and the distribution shifts up one. State both and
      say which gesture each governs.
- [x] 4.2 `README.md:41-48` — same claim in prose, same fix, shorter.
- [x] 4.3 `README.md:67-68` — after 0.3 has made the sentence true of
      today's code, update it again for the new behaviour: Randomize All
      moves at most two banks' Crispy, so the Crunchy-exclusion reasoning
      needs restating, not just the count.
- [x] 4.4 `MANUAL.md:161-162` — "It leaves each bank's Crispy and the global
      Crunchy alone" becomes the at-most-two rule.
- [x] 4.5 `app/FroggersModulation.hpp:903`, `:945-947` and `:1141` state
      `P(k) = 0.5^(k+1)` unconditionally: state the floor at each. `:911`
      ("app-owned weighted table"), `:919` ("NEVER a no-op") and `:923-927`
      ("count table above", "61") are rewritten to the geometric draw with
      its per-gesture floor and the real parameter count (84).
      DONE 2026-09-07 by the code executor; `:903` stays as it is, because
      it describes Sheaf's own private coin loop, which this change does not
      touch (postflight confirmed every other site).
- [x] 4.6 `QUICK_DICT.md` randomize claims: none found by the sweep (its
      Crispy/Crunchy entries describe the scramble, not randomization);
      confirmed by reading. Its parameter drift is group 8.

## 5. Spec — S

- [x] 5.1 The merged delta under `specs/froggers-modulation-slate/`: both
      MODIFIED requirements. The floor requirement must say Randomize All at
      a drilled-in level, NOT "a drilled-in level" generally — the halted
      change's delta carried the same over-scope as its code. Written;
      re-read 2026-09-07 against the diff: the Crispy scenario matches the
      draw (zero, one or two distinct banks; zero on about half of presses;
      every bank reachable), the floor scenarios match the four call sites
      (one at both drilled-in Randomize All calls, zero at Randomize Page and
      at level 0). Postflight re-reads it independently.
- [x] 5.2 `openspec validate frogg3rs-randomize-bounds-and-parked-repairs
      --strict` and `--all --strict`. Every SHALL on a requirement's FIRST
      body line. DONE 2026-09-07: the change is valid; `--all --strict` 21
      passed, 0 failed after the archive.

## 6. Re-home the red browser e2e suite — S

- [x] 6.1 Carried into this change's record (proposal item 6).
      `External/Sheaf/projects/synth/browser/tests/fake-app.e2e.spec.ts` is
      10 failed / 7 passed, red since `c81727b9`; seven are
      `controller wizard` tests; `:541-542` drives
      `runtime.controllers.add_name` and `runtime.controllers.add_kind`,
      neither of which exists in `ControllersPageUI.hpp` any more — the add
      row is one preset picker with a `"Custom (<kind>)"` entry per kind
      (`:919,937`). Re-verified by the preflight: both ids are absent from
      that header.
- [x] 6.2 NOT REPAIRED HERE. Repair is a design ruling on the wizard flow —
      how "add a hand-wired Twister" is expressed as one preset selection,
      and where naming happens now that it is not part of adding. It needs
      its own change and an operator decision.

## 7. Verification and delivery

- [x] 7.1 Every gate in the table on the final tree; VST `ctest` before and
      after the rebuild. VST DONE 2026-09-07: `ctest` 5/5 on the old build,
      then the rebuild against the final header recompiled 11 units and
      `ctest` 5/5 again, both exit 0. The app gate and `openspec validate`
      are re-run on the final tree after group 8. DONE: the full app gate
      ran after every code and document edit (12/12 binaries, every check
      OK, exit 0); the lead's later wording edits touched only `MANUAL.md`
      and `QUICK_DICT.md`, which the docs check reads (re-run green) and
      which the VST and standalone bundles copy as resources
      (`app/vst/CMakeLists.txt:157-158,187`,
      `app/standalone/CMakeLists.txt:182-185`), so the VST build and `ctest`
      were re-run after them as well: 5/5, exit 0;
      `openspec validate --all --strict` 22 passed (the count includes a
      foreign untracked change, `frogg3rs-midi-shift`, not part of this
      work).
- [x] 7.2 Postflight (S, fresh context). Its brief includes 1.2's question —
      does the implementation do what the operator asked, not just what the
      plan said — and group 8's: does every rewritten entry match the table
      and the DSP source it cites.
- [x] 7.3 Commits on `main`, no AI attribution: hygiene (0.2, 0.3), code
      (2, 3), documentation (4, 8), spec (5), in that order.
      DONE 2026-09-07: `121bb51` hygiene, `e1b61ed` code, `45b3193`
      documents and the drift check, `fd825b3` this change's record (the
      spec delta lands with the record; README's 0.3 and 4.3 edits share
      one file and land in the documents commit, after the code commit, so
      no commit asserts a behaviour its code lacks). Pushed to `main`.
      Postflight (fresh context) verdict: PASS WITH FINDINGS — a stale 4.5
      checkbox and an inexact gate-input claim, both corrected above; core
      code, tripwire test, spec backing and hygiene confirmed.
- [x] 7.4 Move `frogg3rs_v2` and `frogg3rs_vst` to the new head and
      force-push; confirm Desktop Release, VST Plugin and Pages green.
      DONE 2026-09-07: both tags at `fd825b3`; Desktop Release, VST Plugin
      (on `main` and on the tag), GitHub Pages and both Firmware tests runs
      completed with success.
- [ ] 7.5 OPERATOR: drill into a parameter, press Randomize All repeatedly —
      every press moves something. On a parameter page, press Randomize All
      repeatedly — about half the parameters stay still, and some presses
      change one or two pages' Crispy while none re-rolls the whole
      instrument's crunch. Randomize Page at a drilled-in level is unchanged
      from before all of this.

## 8. Parameter reference drift — S (operator instruction 2026-09-07)

- [x] 8.1 (report: 86 entries checked, 54 clean, 32 with drift — 11 Filter
      slot numbers, 7 names, 6 labels, 4 ranges or behaviours, 3 entries
      for parameters that do not exist, 1 signal-path paragraph; every
      verdict cited and the six most consequential re-read by the lead
      against source) Enumerate every entry: for all 84 page parameters plus Crispy and
      Crunchy, compare `app/FroggersParameters.hpp:154-275` (name,
      `shortName`, slot), `app/FroggersUiSurface.hpp:816-825` (rendered
      label) and the DSP source of each range or behaviour claim against
      the entry in `MANUAL.md` and in `QUICK_DICT.md`; one verdict per entry
      with a file:line, "nothing needed" included. Settle what `shortName`
      is used for, so the manual's backticked label is the one the operator
      can see. Report: `scratchpad/doc-drift-report.md` (session
      scratchpad, not the repo).
- [x] 8.2 Rewrite the bank sections of `MANUAL.md` (`:350-632`) and
      `QUICK_DICT.md`, and any parameter claim in `README.md`, from that
      report: correct slot numbers, names and labels; replace the Detune,
      Color and Halo entries with Freeze, Reverse blend and Diffusion,
      described from `app/dsp/Delay.hpp`; rename "Scoop" to "Scoop mix";
      correct every range the report refutes. Plain present tense, say what
      the control does, no history.
- [x] 8.3 `app/check_docs_match_parameter_table.py`, invoked from a new
      `check-docs-match-parameter-table` target that `make -C app test`
      depends on, beside the existing checks (`Makefile:175-189, 261-265`).
      It parses the six bank tables from `FroggersParameters.hpp` and every
      bold entry `**Name** (\`Label\`, slot N)` — including the grouped forms
      "A / B / C (…, slots i–j)" and "A / B / C (…, slots a/b/c)" — in the
      bank sections of `MANUAL.md` and `QUICK_DICT.md`, and fails, naming
      the entry, when a name, label or slot disagrees with the table or a
      table entry has no document entry. Proven by breaking it once: change
      one slot number in `QUICK_DICT.md`, see the check go red with that
      entry's name, restore, see green. Report both outputs.
      DONE 2026-09-07: green reads `MANUAL.md 84 entries/0 failures;
      QUICK_DICT.md 84 entries/0 failures`; with one QUICK_DICT slot changed
      it read `QUICK_DICT.md: Filter: 'Peak gain' is shown at slot 9, the
      table has it at slot 1` and `make` exited 1; restored, green again.
- [x] 8.4 Gate: `nice make -C app -j2 test` green including the new check.
