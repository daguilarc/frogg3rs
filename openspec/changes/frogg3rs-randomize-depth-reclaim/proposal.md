# Finish the re-roll: release the modulation it rolls away

## Why

Randomize All is a re-roll, and the re-roll itself is correct. Measured across
twelve presses (`evidence.md` Measurement 6): the number of page parameters
carrying modulation holds around 42 and never climbs, and no stale assignment
survives a press.

What the press never does is let go of the storage for the sources the new roll
did not pick. The first half of the operation exists; the second half does not.

Nothing about that is visible. The values are right, the badges are right, the
patch sounds like a fresh roll. What grows is the live local depth-parameter
count, and per-block cost is proportional to it:

| presses | today | with the release |
| --- | --- | --- |
| 1 | 71 depths, 1.47x | 65 depths, 1.31x |
| 10 | 622 depths, 3.56x | 84 depths, 1.40x |
| 50 | 1072 depths, 4.72x | 73 depths, 1.41x |

~80 depths are what a roll uses. The rest are what earlier rolls rolled away
and nobody released.

On the deployed browser build, an operator who pressed Randomize All repeatedly
reached a state where the deadline meter read 98-101% and the sound had stopped
while the oscilloscope kept drawing. The meter is the fraction of the real-time
budget the audio callback consumed. **What happens past that point is not
traced and is not claimed here**: the callback was still running when measured
(a stopped callback would freeze the meter; it varied, then decayed). The
observable is a callback at its budget with no sound, and the release removes
the condition that puts it there. The oscilloscope keeps drawing because the UI
repaints from published state on the message thread, independent of the
callback's budget.

## Root cause

Cost is proportional to the number of live local depth parameters, reached
through the recursive `Compute()` descent at control rate. `evidence.md`
Measurement 2 rules out the alternatives by counter: the per-sample top-level
walk (`ProcessSamplePhase1` walks `topLevelParameters_`, and that count never
moves off 91), active route count (returns to baseline on Reset All while cost
does not), and storage footprint (batches stay allocated after a release, yet
cost returns to baseline).

Materialised depths persist until something releases them.
`ParameterGroup::CollectNeutralLocalParameters`, via
`Parameter::CollectNeutralChildren`, `ParameterGroup::RecycleLocalParameter`
and the `Parameter::CanRecycleLocal` gate, is that release. Sheaf calls it
wherever depths go neutral en masse: `Bank::Deselect`, `Bank::HandlePress`'s
Reset-modifier branch, `Bank::ApplyModifierToTopLevel`,
`ParameterManager::RevertAllToDefaults`, and patch load.

Froggers' randomize drain reaches none of those. Symbols are named rather than
line-cited because Task 3 edits one of these files and
`check-citations-resolve` forbids line citations into this tree.

## Scope: Randomize All is the only affordance that accumulates

Measured on today's tree, fifty presses each, from registered defaults:

| affordance | live local depths after 50 |
| --- | --- |
| Randomize All | 1072 |
| Randomize Page | 6 |
| Reset All | 6 |
| Reset Page | 6 |

Only Randomize All accumulates. The promoted spec says why Randomize Page does
not: it randomises exactly what is displayed, "with no depths". An earlier
draft asserted a scenario covering all four; it would have been green for three
of them before any implementation, and asserted nothing.

The release itself sits at the randomize drain, which both Randomize All and
Randomize Page reach; the reset drains do not. So the call also runs after a
Page press, where it is a neutrality scan over the fixed 91 top-level
parameters that finds nothing. That is stated rather than narrowed: gating it
strictly to the All branch would save an O(91) pointer walk and change code
whose controls have already been run, for no measured benefit.

## Why this MODIFIES the promoted requirement

`froggers-modulation-slate` carries "Depth storage for a given source SHALL be
allocated once, on first use, rather than accumulating additional storage
across repeated randomization presses." Its second clause is violated by
measured behaviour: 1072 depth parameters persist across fifty presses against
a last-press footprint of ~80.

The change that promoted it recorded the rationale that certified it — "the
persistent footprint is only the modulating depths of the last press ... the
spec's allocated-once clause is honoured" — and that is precisely the claim
this change measures as false. So the delta MODIFIES the requirement, restating
it in full and correcting the clause, rather than standing a second requirement
beside a violated one.

## Forward enumeration of every concept this change creates

| concept | FOUND | CHANGED |
| --- | --- | --- |
| `randomize_storm_holds_its_depth_working_set` | 0 outside this change | 1 created |
| `CollectNeutralLocalParameters` | 6 Sheaf call sites, 0 in `app/` production, 2 in `app/` test prose | 1 call site added |
| `LiveLocalParameterCount` | 0 in `app/` | 1 use added, in the check |
| `CanRecycleLocal` | 1 definition, 1 call site, both Sheaf | 0 |
| `kMaxDrillLevel` | 1 definition, uses in `app/` | 0 (comment corrected only) |

After Task 3, `ProcessFrame` holds two release paths: the new one, and the
pre-existing `while (drillIn_->Level() > 0) drillIn_->Back();` reaching
`Bank::Deselect`. Disposition: not duplicates. One releases after a roll, the
other after a view closes; neither is reachable from the other, and collapsing
them would tie a roll's storage lifetime to the drill-in view.

## Impact

- `app/FroggersAppCore.hpp` — the release; the stale drill-out comment.
- `app/FroggersModulation.hpp` — a stale `kMaxParameters` comment.
- `app/FroggersAudioRoutingTests.cpp` — the check.
- `app/FroggersModulationTests.cpp` — whatever Task 4's adversarial passes
  require. Not yet touched; the diff so far modifies three files, and this
  entry plus the spec below are the outstanding ones.
- `openspec/specs/froggers-modulation-slate` — the MODIFIED requirement.

The hygiene sweep covers both directories named above: `app/`, and the spec
directory. Its spec findings are
reported in Task 1 and deliberately not repaired here.

## Not in this change

- **The `recycledLocalSlots_` reservation — accepted explicitly, not
  overlooked.** The release can push past that vector's construction reserve of
  96 and reallocate on the audio thread. Measured across a fifty-press storm,
  before and after this change, counting allocations with a global `operator
  new` override: the change adds exactly ONE allocation, 6144 bytes, at press
  six, when the recycled count crosses 96 and the vector doubles to 192. It
  never recurs; the peak recycled count is 156. That same callback already
  performs on the order of a hundred to several hundred allocations per press
  from `CreateLocalParameter` constructing fresh parameters, identically with
  and without this change.

  Moving the release off the audio thread is not the fix: the parameter graph
  it touches has no locks and no atomics, and the audio thread reads the same
  child pointers every block, so a message-thread release would race
  `Compute()`. Making it safe would need a lock in the callback's hot path.

  The correct fix is the idiom this codebase already uses for the adjacent
  problem: `RequestParameterStorageBatchIfLow` has the audio thread FLAG a
  shortage and the message thread supply the storage, and
  `recycledLocalSlots_` does not follow it. That is a Sheaf-side change,
  sequenced behind the MIDI change's submodule pin, and it fixes the wider
  exposure too — a drill-out pushes ~999 slots onto that same vector from that
  same thread today, which this change does not create and does not worsen.

  What is accepted by shipping without it: one allocator call inside the
  real-time callback, once per session, on a path that already allocates
  heavily. What that risks is a stall on an allocator lock, which would surface
  as a single crackle during the first large randomize storm.
- **A browser deadline check.** The ratio it would assert was never measured in
  a browser; one post-reproduction reading exists and no first-press reading.
- **Storage-batch growth.** Total parameter count climbs 162 to 398 over a
  thousand presses while 138-156 recycled slots sit unused, so new batches are
  taken against a pool that already has capacity. Measured, bounded-looking,
  Sheaf-side allocation policy, and not created by this change.
- **The drill-in cap contradiction** Task 1's sweep found in the spec. A
  separate correctness question, not on this change's path.
- **Bounding randomize's reach.** At level 0 it randomises page values plus
  first-level depths; drilled in it reaches two levels, which the promoted spec
  requires. Level-0 presses alone take live depths 6 to 1072, so depth is not
  the lever.
- **Any manual change.** Re-rolling from a clean slate is what the app does and
  what the manual describes.

## Active changes

Read from `git worktree list` and `git status` rather than from memory, because
a name list decays under another session:

- Main tree, untracked: `frogg3rs-midi-controller-resilience`, this change.
- Main tree, modified: `frogg3rs-delay-width-wysiwyg-repair`, whose Impact
  names `app/dsp/Delay.hpp`, `app/FroggersDspParityTests.cpp`, `MANUAL.md`,
  `QUICK_DICT.md`.
- Worktrees that exist: `midi-controller-resilience` (locked) and this one.
  No others.

Sheaf's own tree carries unarchived changes; none is touched, because this
change no longer edits Sheaf. No file this change modifies collides with any of
them.

## Risks

- The release must not take a depth in use, on screen, or sounding.
  `CanRecycleLocal` requires a local id, zero view pins, zero active routes, no
  non-null children, and near-default state across both scene endpoints;
  `RecycleLocalParameter` re-checks the pin count. Task 4 attacks it rather
  than trusting it.
- Every cost figure is a ratio measured in one run. Per-block cost varies by
  more than 2x with machine load here, which is why the check asserts counts.
