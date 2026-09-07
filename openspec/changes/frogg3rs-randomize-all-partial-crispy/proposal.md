# Proposal — `frogg3rs-randomize-all-partial-crispy`

**Created 2026-09-07. PLANNED ONLY: nothing executes before the operator's
go. Not yet preflighted.**

Paths are repo-root relative. Line numbers are 2026-09-07 reads of `main` at
`6e76316`.

## What the operator asked for

Randomize All should randomize at most two of the six per-bank Crispy knobs,
rather than none. The count is 0 to 2.

## What the tree does now

**Randomize All excludes Crispy on every bank.** `RandomizeAll`
(`app/FroggersModulation.hpp:1526`) loops the six banks (`:1535`, over
`kFroggersBankCount = 6`, `app/FroggersParameters.hpp:75`) and calls
`detail::RandomizeBankValues(manager, bank, /*includeCrispy=*/false)`
(`:1539`). `RandomizeBankValues` (`:1263-1271`) presses a random value into
each of the 14 page slots, then presses `kFroggersCrispySlot` (14) only when
the flag is set. Randomize Page passes `true` for the page it is on
(`:1486`).

**The stated reason is about all six at once, not about Crispy.** The flag's
own comment (`:1254-1262`): "Randomize All -> FALSE. Randomizing local Crispy
on all six pages at once is effectively randomizing global Crunchy, which
this app deliberately never randomizes. Doing it six times over reaches the
same place by another route." So the exclusion targets the aggregate. That a
single Crispy may be randomized is already settled by Randomize Page.

**What is asserted today.**
`app/FroggersModulationTests.cpp:717`
(`randomize_all_leaves_local_crispy_alone_but_randomize_page_moves_it`) runs
Randomize All eight times and requires Reverb's Crispy to hold its value,
then requires Randomize Page to move it. Its header comment (`:713-716`)
restates the aggregate rationale.
`openspec/specs/froggers-modulation-slate/spec.md:191` requires Randomize All
to "leave every bank's local Crispy control and the global Crunchy control
untouched", with the scenario at `:194`.

**The count and distinct-pick mechanisms already exist.**
`RandomizeParameterModulationDepths` (`:1106`) draws a geometric count —
`while (count < eligible.size() && manager.NextRandomCoin() >= 0.5f)
{ ++count; }` (`:1158-1160`) — then runs a partial Fisher-Yates over
`eligible` to draw that many DISTINCT indices (`:1162-1164`).
`NextRandomCoin()` and `NextRandomIndex(exclusiveMax)` are the manager's own
draws (`External/Sheaf/projects/synth/include/synth/ParameterModulation.hpp:885-886`),
which is what lets a fixture's injected index source govern them.

**A README sentence contradicts all of the above.** `README.md:67` says
"Because every **Crispy** knob is randomized by **Randomize All**, this is
functionally the same outcome", and builds the justification for excluding
Crunchy on it. Randomize All does not randomize any Crispy knob today. The
code, the spec and `MANUAL.md:162` all say the opposite. This is a live
defect independent of this change, found in this change's §8.0 sweep and
fixed inside it.

## Design

**Draw a count of 0 to 2, then randomize that many distinct banks' Crispy.**
The count uses the same coin the depth randomizer uses, capped at two:

    std::size_t crispyCount = 0;
    while (crispyCount < kMaxRandomizedCrispy && manager.NextRandomCoin() >= 0.5f) {
        ++crispyCount;
    }

with `kMaxRandomizedCrispy = 2`. That gives 50% none, 25% one, 25% two: the
tail mass that would have gone to three or more collects on two, because the
loop stops there. The banks are then drawn as distinct indices over
`kFroggersBankCount`, the same partial Fisher-Yates shape `:1162` already
uses, so the same bank is never picked twice.

`RandomizeBankValues`'s `bool includeCrispy` does not change shape. The
selection happens in `RandomizeAll`'s own loop, which decides the set of
banks up front and passes `true` for those and `false` for the rest. Nothing
else calls the helper differently, and Randomize Page keeps passing `true`.

**Why two does not reproduce the aggregate.** The rationale being relaxed is
that six simultaneous Crispy draws land in the same place as one Crunchy
draw. Two of six leaves four banks holding their values, so the global
bit-scramble character is not re-rolled; two pages change their own. Whether
that is ALSO true by ear is not derivable from the code and is the operator
check, not a claim this proposal makes.

**Decision reserved for the operator:** the truncated draw above (50/25/25)
versus a strict geometric one where the three-or-more tail is discarded
rather than collected (50/25/12.5, and 12.5% redrawn or treated as zero). The
truncated form is proposed because it is the existing loop with a smaller
bound and no new branch. Note that the spec's own geometric requirement
(`openspec/specs/froggers-modulation-slate/spec.md:201`, "The randomizer
SHALL draw its source count geometrically, each count half as likely as the
one below it") is scoped to the modulation SOURCE count, not to this bank
count, so the truncation does not contradict it — but the two draws now share
a shape and the spec should say which requirement owns which.

## Overlaps with active changes

`frogg3rs-drilled-in-randomize-floor` (PLANNED ONLY, 15 open tasks) changes
`RandomizeParameterModulationDepths` to take a `minimumSources` argument and
start the same count loop from it. **That change governs the count helper.**
If it lands first, this change reads the final signature before touching
anything nearby; if this lands first, that change rebases onto it. Either
way the two must not grow independent geometric-count code: if both are in
flight when this executes, the shared draw is extracted once and both call
it (§5). Note that change's proposal cites the helper at `:867`; it is at
`:1106` today, so its own line numbers need re-reading before it executes —
reported, not fixed here, since it is that change's artifact.

The other six active changes were checked for `randomize`/`crispy`:
`controllers-editor-add-and-columns`, `controllers-page-name-in-the-editor`,
`controllers-page-row-controls`, `guitar-and-solo-variants`,
`omni-audit-repairs`, `random-sh-anomaly-gradient` — none has an open task
touching `RandomizeAll`, `RandomizeBankValues`, the Crispy slot, or the
modulation-slate spec.

## Not in this change

- Crunchy. It stays excluded from every randomize path, as it is today.
- Randomize Page's behaviour. It keeps randomizing its own page's Crispy.
- The modulation-source count draw or its distribution.
- Archiving the three active changes whose only open items are operator
  checks (`guitar-and-solo-variants`, `omni-audit-repairs`,
  `random-sh-anomaly-gradient`). Reported to the operator, not this
  change's to close.

## Spec deltas

`froggers-modulation-slate`: MODIFIED "Two randomize affordances" — Randomize
All randomizes at most two banks' Crispy instead of none, and its scenario
follows.

## Docs

- `README.md:67`: the false "every Crispy knob is randomized by Randomize
  All" sentence, and the Crunchy-exclusion reasoning built on it.
- `README.md:41-43`: the Randomize All description gains the Crispy count.
- `MANUAL.md:161-162`: "It leaves each bank's Crispy and the global Crunchy
  alone" becomes the at-most-two rule; `:167` (Randomize Page) unchanged.
- `MANUAL.md:94`: "without touching six separate Crispy knobs" describes
  Crunchy and stays true; re-read at execution.
- `QUICK_DICT.md:11-12` state ranges and semantics only, no randomize
  behaviour — expected unchanged, confirmed at execution.

## Gates

| gate | loads | control | run when |
|---|---|---|---|
| `nice make -C app -j2 test` | all of `app/` | the rewritten Crispy test red before the change | after group 2 |
| `nice cmake --build app/vst/build -j2 && ctest --test-dir app/vst/build --output-on-failure` | `app/vst/` | the rebuild | at the end |
| `openspec validate --all --strict` | `openspec/` | — | after the spec delta |

## Delivery

One commit, no AI attribution, fast-forward push to `main`, then the release
tags moved as usual; no pull request.
