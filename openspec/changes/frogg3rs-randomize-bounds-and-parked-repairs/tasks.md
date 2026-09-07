# Tasks — `frogg3rs-randomize-bounds-and-parked-repairs`

PLANNED ONLY. Not yet preflighted. Tier per task: **H** = mechanical, **S**
= decides what something means. Builds under `nice`, `-j2` at most.

The working tree is clean: the halted change's partial execution was reverted
so this plan starts where it says it starts. All line numbers below are reads
of the reverted tree.

Carried forward as already closed, not re-run: the helper is at
`app/FroggersModulation.hpp:1106`, its count loop at `:1158`, its four call
sites at `:1303` (`RandomizeBankLevel1Depths`), `:1494` (`RandomizePage`),
`:1584` and `:1619` (both `RandomizeAll`); `FroggersParameters.hpp:291`
(`kNumModulators = 15`). BEFORE histograms, measured on this same tree:
level-0 `P(0)=49.9% mode=0 mean=0.926`,
level-1 `P(0)=48.2% mode=0 mean=1.024`,
level-2 `P(0)=52.9% mode=0 mean=0.934`.

## 0. Hygiene — step zero (§8.0)

The sweep opens the change and what it finds is fixed inside it. The tree this
change touches: `app/FroggersModulation.hpp`, `app/FroggersModulationTests.cpp`,
`openspec/specs/froggers-modulation-slate/`,
`openspec/specs/pair-ar-vcv-time-range/`, and the randomize / Crispy / Crunchy
sections of `README.md`, `MANUAL.md` and `QUICK_DICT.md`. Name each as swept.

- [ ] 0.1 Sweep those paths. Search by CONCEPT, not by one spelling: the
      concepts are "how many sources a randomize attaches", "which gesture
      randomizes Crispy", and "what the randomize floor is". A claim can name
      none of the changed symbols and still rest on the number they produce —
      that is the miss that produced the superseded change, twice.
- [ ] 0.2 `openspec/specs/pair-ar-vcv-time-range/` — `openspec archive
      frogg3rs-omni-audit-repairs` aborts because that change's delta REMOVES
      all three of this spec's requirements, and a spec with zero requirements
      fails validation. pair-AR is gone from `src/core` (`58e232d`); the only
      remaining `app/` mentions are pinned `08b5fd3:` citations in comments,
      historical references to the reference commit, which stay. Delete the
      spec directory rather than leaving it empty — the inbound half of that
      deletion. Enumerate everything that MENTIONS it first, by name and by
      path, and fix each.
      UNTRACED, settle it before acting: `openspec archive` is what writes
      spec updates, so it may recreate this directory from the delta rather
      than honour its absence. Try deleting first and archiving second; if the
      archive recreates it or still aborts, archive first with `--skip-specs`
      and delete after, and record which order actually worked. Do not guess
      between the two — the failure reads as a tooling bug either way.
- [ ] 0.3 `README.md:67` — "Because every **Crispy** knob is randomized by
      **Randomize All**, this is functionally the same outcome" is FALSE
      today: Randomize All excludes every Crispy
      (`app/FroggersModulation.hpp:1539`), the spec says it SHALL leave them
      untouched, and `MANUAL.md:162` agrees. The Crunchy-exclusion argument is
      built on that false claim. Pre-existing, found by the sweep, not caused
      by this change. Group 4 updates the same sentence again for the new
      behaviour; do both in that order so the file never asserts something no
      version of the code did.
- [ ] 0.4 Re-run the archive after 0.2; confirm `openspec validate --all
      --strict` is clean and `frogg3rs-omni-audit-repairs` is archived.
- [ ] 0.5 Report the sweep: each path named, each finding with a disposition,
      including "nothing needed". A hit with no stated disposition is an open
      item, not a finding.

## 1. Preflight

- [ ] 1.1 Every citation in `proposal.md` resolves at the current tree.
- [ ] 1.2 **Compare the plan against the ask, not only against the tree.**
      For every call site this change floors or unfloors, quote the operator
      instruction that covers it and name the enclosing function read from
      the file. A step whose target no instruction covers is rejected here.
      This is the check whose absence produced the superseded change.
- [ ] 1.3 Re-run §5 forward on every concept: `minimumSources`,
      `kMaxRandomizedCrispy`, the Crispy bank selection, and the geometric
      loop itself. FOUND vs CHANGED per concept, zeros included. Confirm no
      third place computes a source count or a floor.
- [ ] 1.4 OPERATOR: Crispy count is truncated geometric (50/25/25) or strict
      with the 3+ tail discarded (50/25/12.5). Truncated is proposed.
- [ ] 1.5 First run of every gate, recorded as the before line.

## 2. Floor the drilled-in Randomize All — H

- [ ] 2.1 `detail::RandomizeParameterModulationDepths`
      (`app/FroggersModulation.hpp:1106`) gains `std::size_t minimumSources = 0`
      as a third parameter. The count draw at `:1158` becomes
      `std::size_t count = std::min(minimumSources, eligible.size());` before
      the existing while loop. The `eligible.empty()` early return stays ahead
      of it. Nothing else in the function changes.
- [ ] 2.2 Pass `1` at `:1584` (`RandomizeAll`'s drilled-in branch) and `:1619`
      (`RandomizeAll`'s one-level descent) — the two halves of one drilled-in
      press. Leave `:1303` (`RandomizeBankLevel1Depths`, level 0) and `:1494`
      (`RandomizePage`, any level) on the default, and say why at each: level 0
      keeps the zero floor because a floor roughly doubles its depth
      allocation; Randomize Page keeps it because its contract is to
      randomize exactly what is displayed.
- [ ] 2.3 `RequireGeometricCountDistribution` (`app/FroggersModulationTests.cpp:903`)
      gains a floor argument and asserts the shape from that floor: every
      bucket below the floor exactly zero, mode at the floor, each count about
      half the one below. THREE call sites: `:973` (level-0, floor 0), `:1044`
      (level-1, floor 1), `:1045` (level-2, floor 1).
- [ ] 2.4 Rename the two tests whose names say `mode_two` (`:953`, `:986`) to
      name the distribution they assert, and correct the stale figures in the
      comment at `:801` ("20%"), `:804` ("~80%") and its "weighted table"
      phrase — all three, or the sentence is half-corrected.
- [ ] 2.5 Positive control, both numbers reported. AFTER histograms beside the
      BEFORE ones above. level-0 MUST be unchanged (mode 0, P(0) about 50%);
      level-1 and level-2 MUST both show P(0)=0 and mode 1. If level-0 moved,
      the floor leaked into the shared default — stop.
- [ ] 2.6 Control on the untouched gesture:
      `randomize_page_mod_detail_moves_the_display_on_the_expected_fraction_of_500_trials`
      (`:812`, band at `:843-844`) must pass UNCHANGED, with no edit to it.
      It drives `RandomizePage` at drill level 1 — call site `:1494`. If it
      fails, the floor reached a gesture the ask did not name; stop and
      re-read 2.2. This test is the tripwire for the defect that superseded
      the previous change.
- [ ] 2.7 Measure the allocation cost: depths materialized per drilled-in
      press before and after, and how often `partial` is reported. If
      `partial` becomes common rather than rare, that is a finding this text
      does not resolve — report it and stop.
- [ ] 2.8 Gate: `nice make -C app -j2 test`, every binary by path.

## 3. Randomize All draws up to two banks' Crispy — S

- [ ] 3.1 Write the test first and run it RED: over many Randomize All
      presses with a fixture-injected draw source, the number of banks whose
      Crispy moved is never 3+, is 0 on about half of presses, and every one
      of the six banks is reachable. Today the count is always 0 and no bank
      is reachable, so it must fail. A pass here voids the gate.
- [ ] 3.2 In `RandomizeAll`'s level-0 branch (`:1535`), draw the count with
      the capped coin loop and the banks with the distinct-index draw, then
      pass `includeCrispy` per bank. `RandomizeBankValues` (`:1263`) keeps
      its `bool` signature. Name the bound `kMaxRandomizedCrispy = 2`; do not
      inline the literal.
- [ ] 3.3 Rewrite the flag comment (`:1254-1262`) and the Crispy test's
      header (`:713-716`) to state the rule as it then is: the aggregate of
      all six is what reproduces Crunchy, so the count is bounded below it.
      Present tense, no earlier behaviour, no fix narrative.
- [ ] 3.4 Gate; the observed distribution goes into the ledger.

## 4. Documentation this change invalidates — S

Distinct from group 0: these are wrong BECAUSE of this change, not before it.

- [ ] 4.1 `MANUAL.md:172-188` — the weighted table is presented as universal.
      It is correct for a page-level press and for Randomize Page at any
      level, and wrong for Randomize All at a drilled-in level, where zero
      becomes impossible and the distribution shifts up one. State both and
      say which gesture each governs.
- [ ] 4.2 `README.md:41-44` — same claim in prose, same fix, shorter.
- [ ] 4.3 `README.md:67` and `:41-43` — after 0.3 has made the sentence true
      of today's code, update it again for the new behaviour: Randomize All
      moves at most two banks' Crispy, so the Crunchy-exclusion reasoning
      needs restating, not just the count.
- [ ] 4.4 `MANUAL.md:161-162` — "It leaves each bank's Crispy and the global
      Crunchy alone" becomes the at-most-two rule.
- [ ] 4.5 `app/FroggersModulation.hpp:903` and `:1141` — both state
      `P(k) = 0.5^(k+1)` unconditionally. State the floor at both; fixing one
      leaves the other contradicting it.
- [ ] 4.6 `QUICK_DICT.md` expected unchanged; confirm by reading and say so.

## 5. Spec — S

- [ ] 5.1 The merged delta under `specs/froggers-modulation-slate/`: both
      MODIFIED requirements. The floor requirement must say Randomize All at
      a drilled-in level, NOT "a drilled-in level" generally — the halted
      change's delta carried the same over-scope as its code.
- [ ] 5.2 `openspec validate frogg3rs-randomize-bounds-and-parked-repairs
      --strict` and `--all --strict`. Every SHALL on a requirement's FIRST
      body line.

## 6. Re-home the red browser e2e suite — S

- [ ] 6.1 Carry the finding into this change's record, so it stops being
      parked against a change that had nothing else open.
      `External/Sheaf/projects/synth/browser/tests/fake-app.e2e.spec.ts` is
      10 failed / 7 passed, red since `c81727b9`; seven are
      `controller wizard` tests; `:541-542` drives
      `runtime.controllers.add_name` and `runtime.controllers.add_kind`,
      neither of which exists in `ControllersPageUI.hpp` any more — the add
      row is one preset picker with a `"Custom (<kind>)"` entry per kind
      (`:919,937`).
- [ ] 6.2 NOT REPAIRED HERE. Repair is a design ruling on the wizard flow —
      how "add a hand-wired Twister" is expressed as one preset selection,
      and where naming happens now that it is not part of adding. It needs
      its own change and an operator decision. Do not attempt it under this
      one.

## 7. Verification and delivery

- [ ] 7.1 Every gate in the table on the final tree; VST rebuilt and `ctest`.
- [ ] 7.2 Postflight (S, fresh context). Its brief must include 1.2's
      question — does the implementation do what the operator asked, not just
      what the plan said.
- [ ] 7.3 Commits on `main`, no AI attribution. The uncommitted floor work in
      the tree is part of this change now and lands with it.
- [ ] 7.4 Move `frogg3rs_v2` and `frogg3rs_vst` to the new head and
      force-push; confirm Desktop Release, VST Plugin and Pages green.
- [ ] 7.5 OPERATOR: drill into a parameter, press Randomize All repeatedly —
      every press moves something. On a parameter page, press Randomize All
      repeatedly — about half the parameters stay still, and some presses
      change one or two pages' Crispy while none re-rolls the whole
      instrument's crunch. Randomize Page at a drilled-in level is unchanged
      from before all of this.
