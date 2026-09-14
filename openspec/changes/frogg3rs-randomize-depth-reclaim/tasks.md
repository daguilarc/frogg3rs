# Tasks

## 1. Hygiene sweep — every directory Impact names

"Every change opens with a sweep of the tree it touches, and what the sweep
finds is fixed INSIDE that change. The sweep covers EVERY directory the
change's own Impact names, submodules included; name each one swept. And it
runs AGAIN, against the change's own final diff."

`app/` and `openspec/specs/froggers-modulation-slate`. Already found, all
verified against the shipping code:

- `app/FroggersAppCore.hpp`, the drill-out drain comment: "bounded to at most 2
  iterations (the level cap)". `kMaxDrillLevel` is 3.
- `app/FroggersModulation.hpp`: "the kMaxParameters=64 initial batch".
  `kMaxParameters` is 96.
- `openspec/specs/froggers-modulation-slate`, purpose line: "a two-level
  drill-in cap"; and "Requirement: Modulation drill-in is capped at two levels"
  with its "Third level is refused" scenario. The cap is 3, and the shipping
  test is named `fourth_level_drill_in_is_refused`.

The spec ones are a promoted requirement contradicted by shipping behaviour.
They are NOT repaired inside this change: that is a separate correctness
question about what the cap should be, and folding it in would put two
unrelated subjects in one change. Report them, and open a change for them.
They are not on this change's path — nothing here reads the drill cap — and
folding them in would put two unrelated subjects in one change.

NOT FILED AS A CHANGE HERE, and an earlier draft of this task said "open a
change for them" without doing so. Correcting the promoted requirement means
deciding whether the cap should be two or three and restating the requirement
and its scenarios against that answer; that is a correctness question with its
own trace, not a line edit. What is delivered here is the finding and its
evidence: the promoted purpose line and requirement say two, the shipping
constant says three, and the shipping test is named for a fourth level being
refused.

Fix the two code comments. Re-run the sweep against this change's final diff.

## 2. The check, red first

`randomize_storm_holds_its_depth_working_set` in
`app/FroggersAudioRoutingTests.cpp`: press Randomize All fifty times at a
four-block cadence; track the PEAK live local depth-parameter count across
those presses and assert it is at most 250 and at least 120.

The floor sits on the FIRST press, not on a stormed count. Once Task 3 lands
the release fires in the same `ProcessFrame` as the randomize, so no test
driving the real request API can observe a pre-release count; a guard asserting
a large stormed count would be unsatisfiable against the fix it guards. The
first-press floor stays live: a release that takes too much drives press 1
toward the baseline of 6 and trips it.

Only Randomize All is covered. Measured on today's tree, Randomize Page, Reset
All and Reset Page never move the live local depth count off 6, with or without
a release — so a scenario covering all four would be green for three of them
before any implementation, and would assert nothing.

Run it on today's tree. Record the red output before Task 3.

## 3. Release the depths at the Randomize All drain

`app/FroggersAppCore.hpp`, at the end of the randomize/reset drain in
`ProcessFrame`. Randomize All is the only affordance measured to accumulate.

Name every symbol introduced in the diff, and enumerate each by operand
before committing, reporting count FOUND versus count CHANGED, zeros
included. "Replaced 3" hides "of 5".

Note the disposition `ProcessFrame` will then hold two release paths: this one,
and the pre-existing `while (drillIn_->Level() > 0) drillIn_->Back();` which
reaches `Bank::Deselect`. They are not duplicates — one releases after a roll,
the other after a view closes — and neither is reachable from the other.

## 4. Adversarial passes on what the release may take

The release decides what it may free, and "for anything whose job is to REJECT
-- a check, a guard, a validator, a filter -- the demand is adversarial:
someone who did not build it tries to get past it and reports what got through.
Send MORE THAN ONE where the thing matters -- each reports only the evasions it
thought of, so a single clean report bounds nothing."

So: at least two fresh contexts, each given a different starting point, neither
told what the other found. Cases to expect them to reach, not to hand them: drilled in at each level; firing in the same
block as the press that materialised the depth; randomize and reset draining on
one edge; a patch load racing the drain; a depth non-neutral on one scene
endpoint only; a widened neutrality tolerance, which makes the count check pass
by a WIDER margin while the release starts taking audibly modulating depths.

Report what gets through; add cases for whatever survives.

## 5. Gate and postflight

`make -C app test` against the baseline measured on today's tree with these
artifacts present: **394 passes, 0 failures, exit 0**.

Then postflight in a context that did not write the proposal, comparing
implementation against proposal only, on separate axes rather than one broad
reading: "one delegate diffs landed code against the proposal, one runs the
adversarial pass against the checks that shipped, one re-runs the enumeration
against the DIFF."

## Not in this change

- The `recycledLocalSlots_` reservation. It is reserved to `maxParameters` and
  a drill-out already pushes ~999 onto it from the audio thread today. Both
  sites considered here were wrong: the construction site cannot see runtime
  batches, and `AddParameterStorageBatch` runs on the message thread and would
  reallocate a vector the audio thread mutates. It is a pre-existing hazard in
  a submodule, it needs its own trace, and it is not created by this change.
- A browser deadline check. The ratio it would assert was never measured in a
  browser; only a single post-reproduction reading exists. Measure first.
