# Results — commands and their output

Every figure this change asserts is reproduced here with the command that
produced it. A citation is a pointer; this file is the evidence.

## The check, red before the release existed

    $ ./app/build/froggers_audio_routing_tests
      [working set] after press 1: 71 live local depths; after press 50: 1072
    [FAIL] randomize_storm_holds_its_depth_working_set: ... 50 presses left 1072
           live local depths, above the ceiling of 150

That was the first form of the check, asserting the count after the last press.
It was replaced: see below.

## Why the first form was wrong

The release runs in the same frame as the randomize, so the count after a press
is a post-release trough. Firing the release only every Kth press:

    $ for K in 0 2 3 5 10 25; do PROBE_K=$K ./peak_probe; done
    K=0   first= 71 peak50=1072 final=1072
    K=2   first= 71 peak50= 284 final= 140
    K=3   first= 71 peak50= 327 final= 309
    K=5   first= 71 peak50= 441 final= 140
    K=10  first= 71 peak50= 652 final= 140
    K=25  first= 71 peak50= 962 final= 140

The final value is 140 for K = 1, 2, 5, 10 and 25 alike — it reports whether the
last press happened to release, not how much release survived. The peak
separates every case.

## Where the ceiling comes from

Peak across fifty presses, at twelve randomizer stream offsets, release firing
every press:

    $ for w in 0 1 2 3 5 7 11 17 23 31 47 63; do ./peak_offsets $w; done
    warm=0   first= 71 peak50= 214      warm=11  first=149 peak50= 214
    warm=1   first=153 peak50= 214      warm=17  first=137 peak50= 214
    warm=2   first=171 peak50= 214      warm=23  first=138 peak50= 199
    warm=3   first=159 peak50= 214      warm=31  first=164 peak50= 199
    warm=5   first=147 peak50= 214      warm=47  first=197 peak50= 199
    warm=7   first=182 peak50= 214      warm=63  first=125 peak50= 190

Correct peak is 190–214 across offsets. The tightest regression above is 284.
The ceiling of 250 sits ~14% above the widest correct peak and ~14% below the
closest failure. The peak is stable across offsets where the final value is not
(137–197), which is why the assertion is on the peak.

## Positive controls on the shipped check

    release removed          peak 1072  [FAIL] above the ceiling of 250
    release every 2nd press  peak  284  [FAIL] above the ceiling of 250
    shipping behaviour       peak  214  [PASS]

The floor has NO positive control. Inducing over-collection by reverting every
parameter to default before releasing did not move the peak off 214, because
`CanRecycleLocal` also requires `activeRouteCount_ == 0` and route state
protects a depth independently of its value. Recorded as a negative result: the
floor is unproven, and the direction it aims at is covered by the
pinned-or-sounding scenario instead.

## Suite, run by path

`make -C app test` cannot complete in any worktree of this repo:
`check-artifact-symbols-resolve` fails on a different change's artifacts, which
name `openspec/changes/frogg3rs-midi-controller-resilience/` — untracked in the
main checkout, so absent from every worktree. Pre-existing and unrelated to this
change. The suite was therefore run binary by binary:

    froggers_audio_routing_tests        exit=0
    froggers_controllers_page_tests     exit=0
    froggers_dsp_parity_tests           exit=0   186/186 tests passed
    froggers_headless_tests             exit=0
    froggers_marbles_clock_tests        exit=0   9/9 tests passed
    froggers_midi_catalog_tests         exit=0
    froggers_modulation_tests           exit=0   49/49 tests passed
    froggers_mono_validation_tests      exit=0
    froggers_parameter_model_tests      exit=0
    froggers_scope_advance_index_tests  exit=0
    froggers_surface_tests              exit=0
    froggers_visualizer_tests           exit=0

    TOTAL_PASS=395   TOTAL_FAIL=0

The main-checkout baseline before this change was 394 passes, 0 failures,
exit 0. The added case is the difference.

## Allocation on the audio thread, measured before and after

Global `operator new` override, counting across a fifty-press storm, built from
copies of `app/` at this commit and at its parent:

    diff before.csv after.csv
    -> one structural difference across all 50 presses:
       press=6 size=6144   (present after, absent before)
    grep -c "size=6144"  ->  after: 1   before: 0
    free_slots: press 5 = 93, press 6 = 97   (crosses the reserve of 96)
    peak free_slots across 50 presses = 156  (under the doubled capacity 192)

The pre-existing per-press allocation on that same callback, unchanged by this
change, is on the order of a hundred to several hundred calls per press from
`CreateLocalParameter`'s `make_unique`.

## A note on how these were verified

The adversarial probes built their cases as copies of `app/` under a scratchpad,
because their briefs forbade editing the repository. An adjudication pass that
received their findings with provenance stripped searched the repository for
those tests, did not find them, and flagged the results as possibly invented.
They were not: the artifacts exist and carry the scenarios described. Recorded
because the error was in how the findings were relayed, not in the findings —
anonymizing a report must not remove the pointer that lets the next reader
re-derive it.

## Known fragility of this check, recorded rather than fixed

The peak depends on one shared fixed-seed `std::mt19937` inside
`ParameterManager`, so unrelated code that consumes a different number of draws
upstream of Randomize All reshuffles every random decision the storm makes.
Measured by inserting single extra draws at four unrelated points, changing
nothing about the release: peaks of 209, 208, 199, 190, 190 against a baseline
of 214. All stayed inside the band, but the drift is tens of units and the
margin from 214 to the ceiling of 250 is 36.

So a future change touching randomizer draw counts anywhere upstream can turn
this red without touching the mechanism it names. Left as it is: widening the
ceiling buys margin against that at the cost of margin against the closest real
regression, which is 284. Recorded so a red here is read correctly rather than
treated as a mystery.

## One adversarial finding examined and rejected

A pass reported that moving the release to run BEFORE the randomize still
passes (peak 128), calling it the defect the code comment warns against. It is
not. The comment's ordering claim is about the release relative to the
parameter RECOMPUTE, not relative to the randomize, and it says in terms that
moving the release after the recompute would also be safe. The earlier
placement yields a lower peak, keeps the current roll's own depths, and still
prevents accumulation. The check passing there is correct behaviour.

Kept in the record because the finding was specific and plausible, and because
rejecting it rests on reading the comment rather than on preferring the
artifact already written.

## The release's gate, pinned by a check rather than by two agents failing to break it

    shipping              armed depth survived=yes  neutral depth collected=yes  [PASS]
    release removed       armed depth survived=yes  neutral depth collected=NO   [FAIL]

The armed-half branch was also seen to fire, on an earlier mis-scoped version of
the test that stormed the armed depth's own bank: Randomize All re-rolls that
bank's depths, so the armed depth was destroyed by the roll rather than taken by
the release. That was the instrument being wrong, not a finding, and it is why
the shipped test storms a different bank.
