# Proposal — `frogg3rs-prose-claims-get-gates`

**Created 2026-09-11**, out of `frogg3rs-effect-page-hierarchy`'s postflight.
That change shipped correct code. What it also shipped, and what its audits
caught only because a fresh context went looking, was a set of claims that were
false and that nothing could have failed on:

- A spec scenario reading `Check: app/FroggersDspParityTests.cpp, the
  stage-independence case`. No such test existed, by name or by content. The
  requirement asserted the system did something, with nothing behind it.
- A second scenario naming the wrong file for tests that did exist.
- Four comments attributing `WetAuthorityFollower` to `Drive.hpp`. It is
  defined in `Limiter.hpp`.
- Thirteen comments carrying `D1`-`D10` design-list shorthand, in a file whose
  own task claimed planning-history labels had been stripped from it.

Each is prose. Nothing in the build reads prose, so each cost nothing to write
and stayed true-looking for the whole life of the change.

Set against that, the one claim of this kind that did NOT rot in that change is
the one with a script behind it. `check_docs_match_parameter_table.py` went from
27 failures to OK because it FAILED, in the gate, until the documents matched
the parameter table. Its own header says why it exists: two documents restate one
C++ table across a language boundary, and that is a family that drifts silently
unless something fails on the drift.

This change adds the something for three more such families.

## What the enumeration found

Counted mechanically, non-archived trees only.

**Citations.** `app/` carries 448 `path:line` citations in comments. Bucketed by
where the path actually resolves:

| bucket | count | disposition |
| --- | --- | --- |
| resolves under `External/Sheaf/` | 214 | LEFT ALONE. A pinned submodule; its lines are stable at the pin. |
| resolves under `app/` | 75 | SWEPT to symbol form. These rot on every edit to this tree. |
| already git-pinned (`08b5fd3:src/core/VcoWaveEval.hpp:7-23`) | 72 | LEFT ALONE. Names a commit, so it cannot rot. |
| resolves under `src/` | 65 | LEFT ALONE. Frozen firmware; the port's provenance. |
| resolves NOWHERE | 22 | SWEPT. Listed below. |

The first enumeration of this was wrong and is recorded here rather than
quietly corrected: its pattern was `[A-Za-z_]+\.(hpp|cpp)`, which truncates
`Braid4Core.hpp` to `Core.hpp` and `V2FuegoStack.hpp` to `FuegoStack.hpp`, and
it did not recognise the `sha:path` form at all. It reported 47 dangling
citations where there are 22, and it invented `Core.hpp` as a missing file when
the real citation was to Sheaf's `Braid4Core.hpp`. Grep the narrowest token the
concept cannot avoid sharing -- here the extension plus a path character class
that admits digits and hyphens.

The 22 that resolve nowhere, by path: `V2FuegoStack.hpp` (9),
`StereoDelay.hpp` (4), `V2EnvelopeFollowerBank.hpp` (3), `DelayState.hpp` (2),
`FroggersV2AppManifest.hpp` (1), `juce_Timer.cpp` (1), `MainComponent.cpp` (1),
`desktop/Source/MainComponent.cpp` (1). Most name the retired simulator, and
their surrounding prose says so honestly -- "the retired simulator's
V2FuegoStack.hpp:14-23". The reference is still unfollowable. The repo already
has the form that fixes this and uses it 72 times: pin the commit.

**Spec `Check:` lines.** 48 across the non-archived specs and changes -- 29 in
`openspec/specs/`, 13 in `frogg3rs-effect-page-hierarchy`, 6 in
`frogg3rs-midi-controller-resilience`. Every one is a claim that a named test
exists. Nothing resolves them.

**Planning-history labels.** **53 lines** under `app/`. The first count was 23,
and the preflight audit rejected both the number and the patterns that produced
it. Recorded here rather than silently corrected, because it is the same defect
twice in one proposal:

- `D[0-9]+` has **nine hits and zero true positives**. Every one is the
  Envelope bank's own D1/D2/D3 decay label (`FroggersParameters.hpp:190-192`,
  `FroggersUiSurface.hpp:820`, `FroggersSurfaceTests.cpp:1934`) -- a product
  name, not design-list shorthand. The real `D1`-`D10` comments this pattern was
  written for were all fixed earlier the same day, so the pattern can now only
  fire falsely, and a check that fails the build on legitimate content gets
  switched off. DROPPED.
- `this task's own` and `Task [A-Z0-9]+` are spellings, not operands. They miss
  `this task exists`, `this task fixes`, `this task targets`, `this task asks
  for`, `the task brief`, `the task report`, lowercase `task A/B/C`,
  `pre-Task-8` and `tasks 2.2-2.5`. Searching the operand instead --
  `\btasks?\b`, case-insensitive -- finds 49 lines, and **every one is a
  violation**. There is no legitimate use of the word in this tree: no task
  queue, no audio task, no thread task.

The pattern that ships is `\btasks?\b`, `\bitem [0-9]`, `\bgroup [0-9]`,
`\bpacket [0-9]`, `§[0-9]`, case-insensitive, over comments and string
literals. 53 lines, no known false positive.

The standing rule that comments explain behaviour and never which planning
artifact produced them is recorded in `CLAUDE.md`. It has been swept by hand
before, and the history says what that was worth. Counting the same operand
pattern over `app/`'s sources at three points:

| point | date | lines |
| --- | --- | --- |
| `fb1a737`, "Remove planning-process labels from the remaining comments" | 2026-08-20 | **201** |
| `81ee8cf`, before this change | 2026-09-11 | 53 |
| after this change | 2026-09-11 | 0 |

A commit whose own title says it removed *the remaining* labels left 201 of them
in place. Not through carelessness -- it swept the labels someone had noticed,
which is what a human sweep can do. Three further weeks of incidental cleanups,
including thirteen removed by hand earlier today, brought it to 53. Searching by
the operand instead of by the spellings anyone remembered brought it to 0, and
the check is what keeps it there.

That is the whole argument for this change, stated as a measurement rather than
a principle: a completed enumeration protects the pass that ran it, and nothing
after.

## What Changes

Three check scripts, joining the five the `test` target already runs. Each is
proven to fail by breaking it once before being wired in -- a check nobody has
seen go red is a check nobody knows runs.

- `app/check_spec_checks_resolve.py`. Parses every `Check:` line out of
  `openspec/specs/**/spec.md` and non-archived `openspec/changes/**/specs/**/spec.md`
  -- spec files only, never a whole change directory, or it reads the `Check:`
  lines QUOTED inside that change's own preflight and postflight reports and
  fails on text nobody is asserting. Extracts the test names and paths it names, and fails if a name matches no `TEST_CASE` in
  `app/*.cpp` and no file on disk. An honest gap stays sayable: a line whose
  text begins `none` or `operator step` is accepted and counted, so "no
  automated check builds the dialog" remains a legal answer and a silent
  fiction does not.
- `app/check_no_planning_history.py`. Fails on planning-history labels in
  comments and string literals under `app/`, by the operand patterns above.
  One allow-list entry with its reason recorded: a `TEMP-BREAK` note records how
  a figure was measured, which is method rather than history.
- `app/check_citations_resolve.py`. Fails on a `path:line` citation in `app/`
  whose path resolves nowhere and which carries no git pin. Scoped by the
  buckets above: `External/Sheaf/`, `src/` and git-pinned citations are out of
  scope by construction, and the reason is recorded in the script.

One sweep, over the two buckets that rot:

- The 22 unresolvable citations get a git pin where the commit is findable, or
  lose the false path where it is not.
- The 75 `app/`-internal citations move from `File.hpp:NNN` to `File.hpp`'s
  `Symbol`. A line number into a tree this change edits is wrong by the next
  commit; a symbol name is not. This is what `omni-rule.md` §1 now asks for.

## Why the two excluded buckets are excluded

Stated rather than left to inference, because the next reader will wonder.

`External/Sheaf/` citations name a submodule pinned at a commit, so their lines
are as stable as a git pin already. `src/` citations are the Daisy firmware
port's provenance -- the tree is frozen, and the whole value of the citation is
that it says which line of the original a formula was copied from. Converting
either to symbols would destroy provenance to fix a rot that cannot happen.

## How the scripts are invoked

All five existing check scripts take exactly one argument, `$(APP_DIR)`,
absolute, so they do not care about the caller's working directory; they signal
with `name: OK - ...` or `name: FAIL - ...` and a nonzero exit that stops the
`test` target. Two of the three new scripts need to read above `app/`. They keep
the same one-argument signature and derive the repository root as that
argument's parent, rather than inventing a second convention for two callers.

## What the postflight changed

The first postflight pass is the reason the shipped checks differ from the ones
proposed, and the headline finding is worth stating where a reader will see it:
**`check_spec_checks_resolve.py` as first written passed the exact bug it exists
to catch.** A `Check:` naming a real file beside a fictional case satisfied it,
because it accepted a line when any token resolved -- a leniency adopted hours
earlier to avoid false positives, which reopened the hole. Two rules replaced
it: evidence tokens are told from sentence tokens by shape, and naming an `app/`
test file now requires naming a case inside it. `tasks.md` group 10 carries the
other three findings, including that the three scripts had already drifted apart
in their copies of one tree walk before any of them shipped.

## Impact

- Affected specs: `field-operator-doc-parity`, which is where this repo keeps
  its rules about documents and their checks.
- Affected code: three new scripts under `app/`, `app/Makefile`'s `test`
  target, and comments across `app/` for the sweep.
- Baseline: 371 PASS / 0 FAIL at `814a703`, measured today. The sweep touches
  comments only, so any movement in that number is a defect in this change.
- Risk: the checks are new code that has never run. Each gets its invocations
  traced against how the five existing check scripts are invoked from the same
  Makefile, and each is broken deliberately to prove it goes red.
