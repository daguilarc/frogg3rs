# froggers-modulation-slate

## MODIFIED Requirements

### Requirement: Randomized source count is biased toward few, and depth storage is allocated once
The randomizer SHALL draw its source count geometrically, each count half as likely as the one below it, from a floor that depends on WHICH GESTURE was pressed and where: Randomize All at a drilled-in modulation level draws from a floor of one, so every such press draws at least one source and one source is the most likely outcome; every other randomize draw — Randomize All on a parameter page, and Randomize Page at any level — draws from a floor of zero, so about half of those calls draw no sources at all. Depth storage for a given source SHALL be allocated once, on first use, rather than reallocated on each press; and storage for a source the current roll did not pick SHALL be released as part of the press, so that repeated randomization presses do not accumulate depth parameters no current source assignment accounts for.

A parameter the zero-floor draw leaves at no sources SHALL carry no modulation
depth and SHALL therefore show no modulation badge, so that a randomized bank
reads as a set of deliberate choices rather than as everything touched at once.

A drilled-in Randomize All press SHALL NOT be a no-op: the floor of one applies
both to the selected parameter's own depths and to the one-level descent that
materializes the level below it.

Randomize Page SHALL keep the zero floor at every level, because its contract is
to randomize exactly what is displayed and a floor is not part of that.

#### Scenario: Some parameters come out of a page randomize untouched
- **WHEN** Randomize All is pressed while a parameter page is active
- **THEN** about half the parameters carry no modulation depth
- **AND** those parameters show no modulation badge
- **AND** the remaining parameters carry at least one non-neutral depth

#### Scenario: A drilled-in Randomize All always moves something
- **WHEN** Randomize All is pressed while a modulation detail grid is active
- **THEN** the selected parameter carries at least one non-neutral depth, on every press
- **AND** one source is the most common outcome, two about half as often, three about half as often again
- **AND** each depth the press materializes is itself randomized under the same floor

#### Scenario: A drilled-in Randomize Page keeps the zero floor
- **WHEN** Randomize Page is pressed while a modulation detail grid is active
- **THEN** the draw is unchanged from a page-level draw: no sources on about half of presses

#### Scenario: Wide draws stay rare
- **WHEN** modulation depths are randomized repeatedly on a parameter page
- **THEN** four or more sources are affected on about one call in sixteen

#### Scenario: Fifty re-rolls hold the working set a single re-roll needs
- **WHEN** the app is driven at its registered defaults and Randomize All is pressed fifty times at a cadence of four blocks per press
- **THEN** the PEAK live local depth-parameter count observed across those presses is at most 250, rather than the 1072 that fifty presses accumulate today
- **AND** the assertion is on that peak rather than on the count after the last press, because the release runs in the same frame as the randomize, so the count read after a press is a post-release trough that reads the same whether the release fires every press or every twenty-fifth
- **AND** that peak is at least 120, so a release that took depths the current roll still uses could not make this pass with a dead instrument
- **AND** both bounds are counts rather than timings, because the count is deterministic while per-block cost on this hardware varies by more than 2x with machine load
- Check: `app/FroggersAudioRoutingTests.cpp`'s `randomize_storm_holds_its_depth_working_set`, which asserts the PEAK live depth count across the storm rather than the value after the last press -- that value is a post-release trough and reads 140 whether the release fires every press or every twenty-fifth. Delivered and passing at a peak of 214 against a ceiling of 250; red at 1072 with the release removed and at 284 with it firing every second press. Both of those controls were run. Results in `openspec/changes/frogg3rs-randomize-depth-reclaim/results.md`.

#### Scenario: The release keeps what the roll is using and takes what it is not
- **WHEN** one modulation depth on a parameter carries a non-neutral value and another on the same parameter is left at its neutral default, and a randomize is pressed twenty-five times on a different bank
- **THEN** the depth carrying a value is still the same parameter afterwards, and the one left neutral has been collected
- **AND** both halves are asserted, so a release that stopped running fails on the neutral depth and a release whose gate loosened fails on the armed one
- **AND** the storm is scoped to a different bank than the armed depth, because a randomize covering that bank would re-roll the depth itself and say nothing about what the release may take
- Check: `app/FroggersAudioRoutingTests.cpp`'s `release_keeps_an_armed_depth_and_takes_a_neutral_one`. Passing; red with the release removed, on the neutral half.
