# Proposal — `frogg3rs-randomize-bounds-and-parked-repairs`

**Created 2026-09-07. PLANNED ONLY: nothing executes before the operator's
go. Not yet preflighted.**

Paths are repo-root relative. Line numbers are 2026-09-07 reads of the clean
tree at `main`.

## What this supersedes, and why one change

Three changes were left outstanding and one archive was blocked. They are
merged here because two of them edit the same function and the same spec
requirement, and shipping them separately means two passes over the same
call-site map with a rebase between:

| superseded | state when absorbed |
|---|---|
| `frogg3rs-drilled-in-randomize-floor` | HALTED mid-execution on an over-scoped plan (below). Its partial execution was reverted; the design survives here, corrected. |
| `frogg3rs-randomize-all-partial-crispy` | Proposal and spec delta only; never executed. |
| `frogg3rs-controllers-editor-add-and-columns` | One task, `5b.2`: a red browser e2e suite it did not cause and correctly refused to repair. |
| `frogg3rs-omni-audit-repairs` (archive) | Complete, but `openspec archive` aborts on it. |

## The state of the working tree

Clean. The halted change's partially-executed code was reverted on
2026-09-07 rather than carried forward, so that execution starts from the
state this proposal describes rather than from a half-applied earlier plan.
The reverted diff is kept at
`scratchpad/halted-floor-work.patch` for reference only; it is not the
starting point and must not be reapplied — it contains the over-scope below.
Every line number in this proposal was re-resolved against the reverted tree.

## Root defect carried in from the halted change

The floor change floors three call sites of
`detail::RandomizeParameterModulationDepths`
(`app/FroggersModulation.hpp:1106`). The operator's ask was, verbatim: "At a
drilled-in modulation level, **Randomize All** should never be a no-op."

| call site | enclosing function | in the ask? |
|---|---|---|
| `:1584` | `RandomizeAll`, drilled-in branch | yes |
| `:1619` | `RandomizeAll`, one-level descent | yes |
| `:1494` | **`RandomizePage`** (defined `:1482`) | **no** |
| `:1303` | `RandomizeBankLevel1Depths`, level 0 | no, correctly untouched |

A page-level press does not reach drilldown levels and nothing here changes
that; equally, a drilled-in **Page** press is not a drilled-in **All** press.
`RandomizePage`'s contract is to "randomize exactly what is displayed"
(`openspec/specs/froggers-modulation-slate/spec.md:191`), and a floor is not
part of it.

**What the over-scope caused.**
`randomize_page_mod_detail_moves_the_display_on_the_expected_fraction_of_500_trials`
(`app/FroggersModulationTests.cpp:812`) drives `RandomizePage` at drill level
1 — call site `:1494` — and asserted its moved-display rate in [35%, 65%].
That test was correct. Execution read its failure as a stale band, widened it
to >95%, and the halted change's own text was then amended to bless that as a
superseding finding. A correct test was edited to fit an unrequested
behaviour change.

**Why no audit caught it.** Preflight verified citations, call-site
completeness, arithmetic and a measured baseline. Postflight compared the
implementation against the proposal — and the implementation matched the
proposal exactly. Only the ASK disagreed, and no pass compared plan to ask.
§1's "a structural instruction is itself a claim" is the rule that applies:
"floor call sites 2, 3 and 4" claimed all three belong to the gesture the
operator named, and that claim was never tested. A test that fails BY
CONSTRUCTION is the loudest evidence a plan is wrong; it was read as a stale
assertion instead.

## What this change does

**1. Floor the drilled-in Randomize All.** Apply the floor to `:1584` and
`:1619` only — the two halves of one drilled-in Randomize All press — and
leave `:1303` and `:1494` on the default. The
measured effect stands: level-1 and level-2 both move from mode 0 to mode 1,
P(0)=0, while level 0 is byte-identical.

**2. Randomize All draws up to two banks' Crispy.** Today Randomize All
excludes every bank's Crispy (`app/FroggersModulation.hpp:1539`,
`includeCrispy=false`) because randomizing all six at once lands where
randomizing global Crunchy would (the flag's own comment, `:1254-1262`). That
reasoning is about the aggregate; Randomize Page already randomizes one
bank's Crispy (`:1486`). Draw 0-2 distinct banks and randomize theirs.

**3. The two count draws share a shape and MUST NOT share their bounds.**
OPERATOR RULING 2026-09-07:

| draw | floor | cap | drawing zero means |
|---|---|---|---|
| Crispy banks | 0 | 2 | no bank's Crispy moved — wanted, and the common case |
| drilled-in sources | 1 | none | the press did nothing — the defect being removed |

Both are `while (count < bound && manager.NextRandomCoin() >= 0.5f) ++count;`
(`:1158-1160`). If they are ever collapsed into one helper it takes BOTH a
minimum and a maximum, and neither default may be borrowed from the other.

**4. Fix the documentation the floor invalidates.** `MANUAL.md:172-188`
presents one weighted table as universal ("Every parameter draws its own
source count"); `README.md:41-44` says the same in prose. Both are correct
for a page-level press and for Randomize Page at any level, and wrong for
Randomize All at a drilled-in level. `app/FroggersModulation.hpp:903` and `:1141`
both state `P(k) = 0.5^(k+1)` unconditionally.

**5. Unblock the `omni-audit-repairs` archive.** `openspec archive` aborts
because that change's delta REMOVES all three requirements from
`pair-ar-vcv-time-range`, and openspec rejects a spec with zero requirements.
The pair-AR envelope was deleted from `src/core` outright (`58e232d`) and is
gone — the only remaining mentions in `app/` are pinned `08b5fd3:` citations
in comments, which are historical references to the reference commit and
correctly stay. An emptied spec is not the right end state: the spec
directory is removed, which is the inbound half of that deletion (§8.0).

**6. Re-home the red browser e2e suite.** `frogg3rs-controllers-editor-add-and-columns`
task `5b.2` reports `External/Sheaf/projects/synth/browser/tests/fake-app.e2e.spec.ts`
at 10 failed / 7 passed, red since `c81727b9`, seven of them
`controller wizard` tests. That change established it did not cause the
breakage (its diff touches the file zero times; `git log -S` puts the
removals at `c81727b9`) and declined to repair it, correctly. The concrete
shape: `:541-542` drives `runtime.controllers.add_name` and
`runtime.controllers.add_kind`, and **neither control exists in
`External/Sheaf/projects/synth/include/synth/ControllersPageUI.hpp` any
more** — the add row is now one preset picker with a `"Custom (<kind>)"`
entry per kind (`:919,937`). Repair means expressing "add a hand-wired
Twister" as one preset selection, and deciding where naming happens now that
it is not part of adding. This change does NOT repair it — it carries the
finding so it stops being parked against a change that has nothing else open.

## Not in this change

- Repairing `fake-app.e2e.spec.ts`. Item 6 re-homes the report only; the
  repair is a design ruling on the wizard flow and needs its own change.
- Crunchy. It stays excluded from every randomize path.
- Randomize Page's behaviour, at any level. It keeps the zero floor, and its
  500-trial display test is the tripwire that proves the floor stayed out.
- The level-0 floor. It stays at zero; a floor there roughly doubles a
  page-level press's depth allocation (~78 to ~164 materialized depths),
  against a prior hand-tuned ladder at ~151 that was abandoned for storage
  pressure.

## Spec deltas

`froggers-modulation-slate`, two MODIFIED requirements — "Two randomize
affordances" (the Crispy count) and "Randomized source count is biased toward
few" (the floor, scoped to Randomize All).

## Gates

| gate | loads | control | run when |
|---|---|---|---|
| `nice make -C app -j2 test` | all of `app/` | the restored `:844` test green with its ORIGINAL band | after each group |
| `nice cmake --build app/vst/build -j2 && ctest --test-dir app/vst/build --output-on-failure` | `app/vst/` | the rebuild | at the end |
| `openspec validate --all --strict` | `openspec/` | archive of `omni-audit-repairs` succeeds | after the spec work |

## Delivery

Commits on `main`, no AI attribution, no pull request. Release tags move once
at the end, covering everything.
