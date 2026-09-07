# Tasks — `frogg3rs-randomize-bounds-and-parked-repairs`

PLANNED ONLY. Not yet preflighted. Tier per task: **H** = mechanical, **S**
= decides what something means. Builds under `nice`, `-j2` at most.

Carried forward as already closed, not re-run: the halted change's preflight
citations (helper `app/FroggersModulation.hpp:1106`; call sites `:1306`,
`:1497`, `:1587`, `:1622`; `FroggersParameters.hpp:291`) and its BEFORE
histograms, measured on the unmodified tree:
level-0 `P(0)=49.9% mode=0 mean=0.926`,
level-1 `P(0)=48.2% mode=0 mean=1.024`,
level-2 `P(0)=52.9% mode=0 mean=0.934`.

## 0. Preflight

- [ ] 0.1 Every citation in `proposal.md` resolves at the current tree.
- [ ] 0.2 **Compare the plan against the ask, not only against the tree.**
      For every call site this change floors or unfloors, quote the operator
      instruction that covers it and name the enclosing function read from
      the file. A step whose target no instruction covers is rejected here.
      This is the check whose absence produced the superseded change.
- [ ] 0.3 Re-run §5 forward on every concept: `minimumSources`,
      `kMaxRandomizedCrispy`, the Crispy bank selection, and the geometric
      loop itself. FOUND vs CHANGED per concept, zeros included. Confirm no
      third place computes a source count or a floor.
- [ ] 0.4 OPERATOR: Crispy count is truncated geometric (50/25/25) or strict
      with the 3+ tail discarded (50/25/12.5). Truncated is proposed.
- [ ] 0.5 First run of every gate, recorded as the before line.

## 1. Correct the floor's scope — H

- [ ] 1.1 `app/FroggersModulation.hpp:1497` — remove the `1` argument so
      `RandomizePage`'s drilled-in branch returns to the default floor 0.
- [ ] 1.2 `app/FroggersModulationTests.cpp:838-843` — restore the original
      [35%, 65%] band and its comment from
      `git show a0a92cc:app/FroggersModulationTests.cpp`. Do not re-derive
      it; that test was correct before this work touched it.
- [ ] 1.3 KEEP the header-comment correction at `:801,804` (20% -> 50%,
      "~80%" -> 50%, "weighted table" -> geometric draw): those figures were
      stale against the code independently. Re-read after 1.2 and confirm the
      header and the band now agree.
- [ ] 1.4 Verify the floor reaches only Randomize All: `:1587` and `:1622`
      keep the `1`; `:1306` and `:1497` are on the default. Report all four
      with enclosing function names read from the file.
- [ ] 1.5 Gate. Control: `randomize_page_mod_detail_moves_the_display_on_the_expected_fraction_of_500_trials`
      passes with its ORIGINAL band and no edits. If it fails, the revert is
      incomplete — stop. The level-1 and level-2 histograms must still show
      P(0)=0 and mode 1, both being fed by Randomize All call sites; if
      either reverts to mode 0, the call-site map in 1.4 is wrong.

## 2. Randomize All draws up to two banks' Crispy — S

- [ ] 2.1 Write the test first and run it RED: over many Randomize All
      presses with a fixture-injected draw source, the number of banks whose
      Crispy moved is never 3+, is 0 on about half of presses, and every one
      of the six banks is reachable. Today the count is always 0 and no bank
      is reachable, so it must fail. A pass here voids the gate.
- [ ] 2.2 In `RandomizeAll`'s level-0 branch (`:1535`), draw the count with
      the capped coin loop and the banks with the distinct-index draw, then
      pass `includeCrispy` per bank. `RandomizeBankValues` (`:1263`) keeps
      its `bool` signature. Name the bound `kMaxRandomizedCrispy = 2`; do not
      inline the literal.
- [ ] 2.3 Rewrite the flag comment (`:1254-1262`) and the Crispy test's
      header (`:713-716`) to state the rule as it then is: the aggregate of
      all six is what reproduces Crunchy, so the count is bounded below it.
      Present tense, no earlier behaviour, no fix narrative.
- [ ] 2.4 Gate; the observed distribution goes into the ledger.

## 3. Documentation the floor invalidates — S

- [ ] 3.1 `MANUAL.md:172-188` — the weighted table is presented as universal.
      It is correct for a page-level press and for Randomize Page at any
      level, and wrong for Randomize All at a drilled-in level, where zero
      becomes impossible and the distribution shifts up one. State both and
      say which gesture each governs.
- [ ] 3.2 `README.md:41-44` — same claim in prose, same fix, shorter.
- [ ] 3.3 `README.md:67` — "Because every **Crispy** knob is randomized by
      **Randomize All**" is false today and only partly true after group 2.
      Rewrite it and the Crunchy-exclusion reasoning built on it.
      `README.md:41-43` gains the Crispy count.
- [ ] 3.4 `MANUAL.md:161-162` — "It leaves each bank's Crispy and the global
      Crunchy alone" becomes the at-most-two rule.
- [ ] 3.5 `app/FroggersModulation.hpp:1140-1143` — the helper's doc comment
      states `P(k) = 0.5^(k+1)` unconditionally. State the floor.
- [ ] 3.6 `QUICK_DICT.md` expected unchanged; confirm by reading and say so.

## 4. Spec — S

- [ ] 4.1 The merged delta under `specs/froggers-modulation-slate/`: both
      MODIFIED requirements. The floor requirement must say Randomize All at
      a drilled-in level, NOT "a drilled-in level" generally — the halted
      change's delta had the same over-scope as its code.
- [ ] 4.2 `openspec validate frogg3rs-randomize-bounds-and-parked-repairs --strict`
      and `--all --strict`. Every SHALL on a requirement's FIRST body line.

## 5. Unblock the omni-audit-repairs archive — H

- [ ] 5.1 `openspec archive frogg3rs-omni-audit-repairs -y` aborts: its delta
      REMOVES all three requirements from `pair-ar-vcv-time-range`, and a
      spec with zero requirements fails validation. Confirm pair-AR is gone
      from `src/core` (it is) and that the only `app/` mentions are pinned
      `08b5fd3:` citations in comments (they are).
- [ ] 5.2 Remove `openspec/specs/pair-ar-vcv-time-range/` rather than leaving
      it empty — the inbound half of the deletion (§8.0). Enumerate
      everything that MENTIONS that spec first, by name and by path, and fix
      each; a dangling spec reference is the defect this step exists to
      avoid.
- [ ] 5.3 Re-run the archive; confirm it completes and
      `openspec validate --all --strict` is clean.

## 6. Re-home the red browser e2e suite — S

- [ ] 6.1 Carry `5b.2`'s finding into this change's record and close it on
      `frogg3rs-controllers-editor-add-and-columns`, so that change stops
      being held open by a defect it did not cause and cannot own.
      `External/Sheaf/projects/synth/browser/tests/fake-app.e2e.spec.ts` is
      10 failed / 7 passed, red since `c81727b9`; seven are
      `controller wizard` tests; `:541-542` drives
      `runtime.controllers.add_name` and `runtime.controllers.add_kind`,
      neither of which exists in `ControllersPageUI.hpp` any more (the add
      row is one preset picker with a `"Custom (<kind>)"` entry per kind,
      `:919,937`).
- [ ] 6.2 NOT REPAIRED HERE. Repair is a design ruling on the wizard flow —
      how "add a hand-wired Twister" is expressed as one preset selection,
      and where naming happens now that it is not part of adding. It needs
      its own change and an operator decision. Do not attempt it under this
      one.

## 7. Verification and delivery

- [ ] 7.1 Every gate in the table on the final tree; VST rebuilt and `ctest`.
- [ ] 7.2 Postflight (S, fresh context). Its brief must include 0.2's
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
