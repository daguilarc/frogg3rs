# Tasks — `frogg3rs-randomize-all-partial-crispy`

PLANNED ONLY. Not yet preflighted. Tier per task: **H** = mechanical, **S**
= decides what something means. Builds under `nice`, `-j2` at most.

## 1. Preflight

- [ ] 1.1 Every citation in `proposal.md` resolves at the current tree.
- [ ] 1.2 Re-read `frogg3rs-drilled-in-randomize-floor` at execution time.
      If it has executed, read the final `RandomizeParameterModulationDepths`
      signature and the count loop as they then stand, and decide whether the
      geometric draw is extracted once for both call sites or duplicated
      deliberately (§5). If it has not, this change must not pre-empt its
      design.
- [ ] 1.3 OPERATOR: truncated geometric (50/25/25, the proposed loop) or
      strict geometric with the 3+ tail discarded (50/25/12.5). If strict,
      rewrite the Design section before group 3.
- [ ] 1.4 Confirm no other randomize path calls `RandomizeBankValues` with a
      computed flag: today the only callers are `RandomizeAll` (`:1539`,
      false) and `RandomizePage` (`:1486`, true). Report FOUND vs CHANGED.
- [ ] 1.5 First run of the gates, recorded as the before line.

## 2. The measurement (finding first) — S

- [ ] 2.1 Rewrite
      `randomize_all_leaves_local_crispy_alone_but_randomize_page_moves_it`
      (`app/FroggersModulationTests.cpp:717`) as a DISTRIBUTION test over
      many Randomize All calls with a fixture-injected draw source: the
      number of banks whose Crispy moved is never 3 or more, is 0 on about
      half of calls, and every one of the six banks is reachable across the
      run. A single-draw assertion proves nothing here. Run it before the
      change: it must FAIL, because today the count is always 0 and no bank
      is ever reachable. A pass before the change voids the gate.
- [ ] 2.2 Keep the second half of that test intact: Randomize Page still
      moves the page's own Crispy.

## 3. The change — H

- [ ] 3.1 In `RandomizeAll`'s level-0 branch (`app/FroggersModulation.hpp:1535`),
      draw the count with the capped coin loop and the bank set with the
      distinct-index draw, then pass `includeCrispy` per bank.
      `RandomizeBankValues` (`:1263`) keeps its `bool` signature unchanged.
      Name the bound `kMaxRandomizedCrispy = 2`; do not inline the literal.
- [ ] 3.2 Rewrite the flag's comment (`:1254-1262`) and the test's header
      comment (`:713-716`) to state the rule as it then is — the aggregate of
      all six is what reproduces Crunchy, so the count is bounded below that
      — naming no earlier behaviour and no fix.
- [ ] 3.3 Gate: `nice make -C app -j2 test`; 2.1 passes; the observed
      distribution goes into the ledger.

## 4. Spec and docs — S

- [ ] 4.1 The delta in `specs/froggers-modulation-slate/spec.md`;
      `openspec validate frogg3rs-randomize-all-partial-crispy --strict`.
      Check the SHALL lands on the requirement's first body line.
- [ ] 4.2 `README.md:67` — the sentence asserting every Crispy knob is
      randomized by Randomize All is false today and only partly true after
      this change. Rewrite it and the Crunchy-exclusion reasoning built on
      it so both state what the code does. `README.md:41-43` gains the
      Crispy count.
- [ ] 4.3 `MANUAL.md:161-162` — "It leaves each bank's Crispy and the global
      Crunchy alone" becomes the at-most-two rule. Present tense, no history.
      Re-read `:94` and `:167` and confirm they stay true.
- [ ] 4.4 `QUICK_DICT.md` expected unchanged; confirm by reading, and say so.
- [ ] 4.5 Gate: `openspec validate --all --strict`.

## 5. Hygiene (§8.0, over the tree this change touches) — H

- [ ] 5.1 Sweep `app/FroggersModulation.hpp`'s randomize region,
      `app/FroggersModulationTests.cpp`'s Crispy tests,
      `openspec/specs/froggers-modulation-slate/`, and the randomize and
      Crispy/Crunchy sections of `README.md`, `MANUAL.md` and
      `QUICK_DICT.md`. Name each as swept; fix what it finds inside this
      change.
- [ ] 5.2 The `README.md:67` contradiction is already a finding of that
      sweep and is fixed by 4.2. Check whether the same false claim appears
      anywhere else — the sweep runs on the CONCEPT ("Randomize All
      randomizes Crispy"), not on one sentence's wording.

## 6. Verification and delivery

- [ ] 6.1 Every gate in the table on the final tree; VST rebuilt and `ctest`.
- [ ] 6.2 Postflight (S, fresh context): implementation versus this text.
- [ ] 6.3 One commit, no AI attribution; fast-forward push to `main`.
- [ ] 6.4 Move `frogg3rs_v2` and `frogg3rs_vst` to the new head and
      force-push; confirm Desktop Release, VST Plugin and Pages green.
- [ ] 6.5 OPERATOR: press Randomize All repeatedly. Some presses change no
      Crispy, some change one or two pages' bit-scramble character, and none
      re-rolls the whole instrument's crunch the way turning Crunchy does.
      If two still reads as a global change, the bound should be one.
