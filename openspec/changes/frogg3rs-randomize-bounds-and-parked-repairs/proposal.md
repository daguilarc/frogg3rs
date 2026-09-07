# Proposal — `frogg3rs-randomize-bounds-and-parked-repairs`

**Created 2026-09-07. Preflight revision 2026-09-07 (inline, this session).
Execution approved by the operator 2026-09-07: preflight, fix, execute,
postflight in a fresh context, then commit and push.**

Paths are repo-root relative. Line numbers are 2026-09-07 reads of the clean
tree at `main` (`dc367b5`), re-verified by the preflight.

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

The three superseded changes were deleted from `openspec/changes/` in
`429f988`; their text is reachable at `429f988^`.

## What the operator asked for

Two asks, quoted from the superseded proposals at `429f988^`:

- Floor: "At a drilled-in modulation level, **Randomize All** should never be
  a no-op."
- Crispy: "Randomize All should randomize at most two of the six per-bank
  Crispy knobs, rather than none. The count is 0 to 2."

And one instruction given during this preflight, 2026-09-07: the manual,
quick dict and readme have drifted from the current parameter set (Detune,
Color and Halo are documented and no longer exist); fix that drift, and put
the update of all three documents in this change's tasks.

## Impact

Directories this change edits; each is swept in group 0 and named in the
sweep report.

- `app/`: `FroggersModulation.hpp`, `FroggersModulationTests.cpp`, a new
  `check_docs_match_parameter_table.py` and its `Makefile` target.
- `app/dsp/`: read only, as the source of truth for the parameter reference.
- `openspec/changes/`: this change; `frogg3rs-omni-audit-repairs` (one delta
  directory deleted, then archived).
- `openspec/specs/`: `froggers-modulation-slate/` (the delta lands on
  archive); `pair-ar-vcv-time-range/` (deleted).
- repo root: `README.md`, `MANUAL.md`, `QUICK_DICT.md`.
- `External/Sheaf`: read only. Nothing in it is edited.

## The state of the working tree

Clean. The halted change's partially-executed code was reverted in
`404e7db` on 2026-09-07 rather than carried forward, so that execution
starts from the state this proposal describes rather than from a
half-applied earlier plan. That commit is the reference for what was undone;
it is not the starting point and must not be reapplied, because it contains
the over-scope below. Every line number in this proposal was re-resolved
against the reverted tree.

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
leave `:1303` and `:1494` at zero. The measured effect stands: level-1 and
level-2 both move from mode 0 to mode 1, P(0)=0, while level 0 keeps its
distribution (mode 0, P(0) about 50%). Level 0 is not byte-identical as a
sequence, because item 2's Crispy draw now consumes the RNG ahead of the
six-bank loop; its histogram is a fresh sample of the unchanged draw, and
the level-0 test's shape assertions are the check. The floor is a parameter of
`RandomizeParameterModulationDepths` with NO default: every one of the four
call sites passes its floor explicitly, and the two zero sites say why at
the call (preflight P4).

**2. Randomize All draws up to two banks' Crispy.** Today Randomize All
excludes every bank's Crispy (`app/FroggersModulation.hpp:1539`,
`includeCrispy=false`) because randomizing all six at once lands where
randomizing global Crunchy would (the flag's own comment, `:1254-1262`). That
reasoning is about the aggregate; Randomize Page already randomizes one
bank's Crispy (`:1486`). Draw 0-2 distinct banks and randomize their Crispy
VALUE; Crispy DEPTHS stay excluded from Randomize All as before
(`RandomizeBankLevel1Depths`, `:1305-1309`). The distinct-bank pick reuses
the partial Fisher-Yates step the source draw already performs
(`:1165-1168`), through one shared step helper (preflight P4).

**3. The two count draws share a shape and MUST NOT share their bounds.**
OPERATOR RULING 2026-09-07:

| draw | floor | cap | drawing zero means |
|---|---|---|---|
| Crispy banks | 0 | 2 | no bank's Crispy moved — wanted, and the common case |
| drilled-in sources | 1 | none (the eligible count) | the press did nothing — the defect being removed |

Both are `while (count < bound && manager.NextRandomCoin() >= 0.5f) ++count;`
(`:1158-1160`). Under §5 two instances of that loop are one helper:
`detail::DrawGeometricCount(manager, minimum, maximum)`, with BOTH bounds
explicit at every call and no default on either, so neither draw can borrow
the other's bound. The Crispy draw is the truncated form: with a cap of two,
P(0)=50%, P(1)=25%, P(2)=25% (task 1.4 in the superseded text proposed
truncated; nothing in the ask distinguishes the two forms, and the truncated
form is the loop as written, so it stands and is recorded here).

**4. Fix the documentation the floor invalidates.** `MANUAL.md:172-188`
presents one weighted table as universal ("Every parameter draws its own
source count"); `README.md:41-48` says the same in prose. Both are correct
for a page-level press and for Randomize Page at any level, and wrong for
Randomize All at a drilled-in level. In `app/FroggersModulation.hpp` the
P(k) = 0.5^(k+1) claim stands unconditionally at `:903`, `:945-947` and
`:1141`; the sweep also found `:911` ("app-owned weighted table"), `:919`
("NEVER a no-op", false today: P(0) is 50%), `:923-927` ("count table above",
"61 for the parameter-page case" — it is 84) and `:1500` ("Crispy included",
false today) — all pre-existing, all fixed here (preflight P5).

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

**7. Repair the parameter reference, and keep it repaired.** The operator's
mid-preflight instruction. The mechanical name-by-slot diff of
`app/FroggersParameters.hpp:154-275` against `MANUAL.md` and `QUICK_DICT.md`
(preflight P10) finds:

- Filter bank: the code order is Peak freq, Peak gain, Peak Q, Comb offset,
  Comb delay, Comb feedback, Comb LP, Comb drive, Scoop mix, Scoop freq,
  Scoop width, Scoop depth, Comb/Peak, Topology (`:196-222`). Both documents
  give a different order — every one of the 14 slot numbers is wrong in
  both — and call slot 8 "Scoop".
- Delay bank: slots 4, 7 and 8 are Freeze, Reverse blend and Diffusion
  (`:242-243`; `app/dsp/Delay.hpp:181-193`, `:1270-1292`). Both documents
  describe Detune, Color and Halo, which no longer exist.
- Rendered labels differ from the table's `shortName` at seven slots
  (`app/FroggersUiSurface.hpp:816-825`, e.g. "Comb FB", "Anti-alias",
  "Reverse", "FB drive"); which one the manual's backticked label should show
  is settled by the enumeration in task 8.1, which reads what `shortName`
  is used for.
- Audio, Envelope, Drive and Reverb: names and slots match in both documents
  (Audio and Envelope use grouped entries). Ranges and behaviour claims are
  checked per entry by task 8.1.

Two documents restating one C++ table across a language boundary is a
family that must stay in sync; §5 says that gets a check that fails on
drift, not an enumeration. `app/check_docs_match_parameter_table.py`, run
from `make -C app test`, parses every bold parameter entry in the bank
sections of `MANUAL.md` and `QUICK_DICT.md` and compares name, label and
slot against the table in `FroggersParameters.hpp`; it is proven by breaking
it once. It needs `python3`, which this machine has and the existing checks
do not use; the `app` test target is a local gate, not run by any workflow
(`.github/workflows/` invokes `make firmware-test`, `ctest` for the VST,
and the Pages build only).

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
- Crispy depths under Randomize All. Only the Crispy VALUE of the drawn banks
  is randomized; `RandomizeBankLevel1Depths` keeps excluding Crispy's depths.
- The manual's non-parameter sections (release platforms, audio
  configuration, MIDI controllers). Item 7 covers the parameter reference:
  the six bank sections, Crispy and Crunchy, and the readme's parameter
  claims.

## Preflight 2026-09-07

Inline (the Impact names five directories). Every citation above was
re-read at `dc367b5`; all resolve. Findings and dispositions:

- **P1** `scratchpad/halted-floor-work.patch` did not exist at the repo path
  the text gave. Replaced by the revert commit `404e7db`, which is what the
  sentence was pointing at.
- **P2** Task 7.3 said "the uncommitted floor work in the tree ... lands with
  it". The tree is clean (`git status`); the sentence described the state
  before `404e7db`. Removed.
- **P3** Task 1.4 asked the operator to choose between the truncated and the
  strict-with-tail-discarded Crispy count. The loop item 3 already specifies
  IS the truncated form; recorded as the ruling, question closed.
- **P4** §5 forward on every concept the plan creates, by operand
  (`NextRandomCoin`, `NextRandomIndex`, `includeCrispy`, `kFroggersCrispySlot`):

  | concept | FOUND today | after this change |
  |---|---|---|
  | geometric coin loop (`NextRandomCoin() >= 0.5f`) | 1, `:1158` | 1 definition (`DrawGeometricCount`), 2 calls |
  | partial Fisher-Yates step (`i + NextRandomIndex(remaining)`, swap) | 1, `:1165-1168` | 1 definition (a step helper), 2 loops calling it |
  | `minimumSources` | 0 | 1 parameter, 4 explicit call sites |
  | `kMaxRandomizedCrispy` | 0 | 1 definition, read by the draw and by the test |
  | Crispy bank selection | 0 | 1, in `RandomizeAll`'s level-0 branch |
  | other `NextRandomIndex` use | 1, `:1549` (the S&H reseed salt) | unchanged; not a distinct-index draw |

  The step helper rather than a whole-loop helper: the source loop
  interleaves each pick with `EnsureModulationDepth` and breaks on null
  (`:1170-1174`), so a loop helper would either move the picks ahead of the
  materialization, changing the RNG sequence every seeded test depends on,
  or take a callback. The three-line step is the shared operand.
- **P5** Stale comments the sweep found beyond the two the plan named, all
  fixed in groups 2-4: `app/FroggersModulation.hpp:911`, `:919`, `:923-927`,
  `:945-947`, `:1305-1309` (Crispy depths "for the same reason its value is"),
  `:1500`; `app/FroggersModulationTests.cpp:787-789` (implies the app-side
  draw removed the no-op; level 0 keeps P(0)=50%), `:895-896`, `:978-985`
  ("the same distribution must apply at EVERY level" — false once the floor
  is per gesture), `:1049` (names a helper `RequireModeTwoCountDistribution`
  that does not exist).
- **P6** `randomize_all_leaves_local_crispy_alone_but_randomize_page_moves_it`
  (`:717-745`) asserts the behaviour item 2 removes; the plan only named its
  header. It is REPLACED by task 3.1's test, which keeps its Randomize Page
  half.
- **P7** Plan against ask (task 1.2): `:1584` and `:1619` are both inside
  `RandomizeAll` (`:1526-1625`), after the `Level() == 0` return at `:1550`,
  so both are the drilled-in press the floor ask names. `:1539` is
  `RandomizeAll`'s level-0 branch, the "Randomize All" the Crispy ask names.
  `:1303` and `:1494` are named by no ask and stay at zero.
- **P8** The proposal had no Impact section; §8.0's sweep is defined over
  one. Added.
- **P9** Other active changes: `frogg3rs-omni-audit-repairs` is complete
  (0 open tasks) and is archived by group 0. `External/Sheaf` has eight
  active changes and five open PRs (#9-#13); grep for "randomi" hits two,
  both incidental (a gesture category in `bank-addressed-absolute-write`,
  a randomized simulation in `rework-controllers-block-editing`). No overlap.
- **P10** The operator's parameter-reference instruction; item 7 and group 8.
- **P11** Behavioural premise check (§6.1): the gate baseline on the clean
  tree reproduces the recorded BEFORE histograms exactly (level-0 P(0)=49.9%
  mean 0.926; level-1 48.2%/1.024; level-2 52.9%/0.934), so the instrument
  is live and the fixture RNG is seeded deterministically.
- **P12** The subagent brief for the code executor carries the four call
  sites, both asks and the tripwire test by name; the executor may not read
  scope from anything else.

## Spec deltas

`froggers-modulation-slate`, two MODIFIED requirements — "Two randomize
affordances" (the Crispy count) and "Randomized source count is biased toward
few" (the floor, scoped to Randomize All).

## Gates

| gate | loads | control | run when |
|---|---|---|---|
| `nice make -C app -j2 test` | all of `app/`, plus the new docs check | the `:812` display test green with its ORIGINAL band; the docs check red when one entry is broken | after each group |
| `nice cmake --build app/vst/build -j2 && ctest --test-dir app/vst/build --output-on-failure` | `app/vst/` | the rebuild | at the end |
| `openspec validate --all --strict` | `openspec/` | archive of `omni-audit-repairs` succeeds | after the spec work |

## Delivery

Commits on `main`, no AI attribution, no pull request. Release tags move once
at the end, covering everything.
