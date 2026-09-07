# Delta — `froggers-modulation-slate`

## MODIFIED Requirements

### Requirement: Two randomize affordances
The app SHALL provide exactly two randomize affordances: **Randomize All** (global) and **Randomize Page** (per-page). Randomize All, pressed while a parameter page is active, SHALL randomize every page parameter value in every bank plus all first-level modulation depths, SHALL randomize the local Crispy control of at most two banks — chosen as distinct banks, with a count that is zero on about half of presses — SHALL leave the global Crunchy control untouched, and SHALL NOT descend to the second level. Randomize All, pressed while a first-level modulation detail grid is active, SHALL randomize that parameter's depths and SHALL also materialize and randomize their second-level depths. Randomize All pressed at the second level SHALL behave identically to Randomize Page. Randomize Page SHALL always randomize exactly what is displayed: on a parameter page, that bank's values including that bank's own Crispy, with no depths; on a modulation detail grid, that grid's depths only.

#### Scenario: The global press moves at most two banks' Crispy
- **WHEN** Randomize All is pressed while a parameter page is active
- **THEN** the number of banks whose local Crispy control changed is zero, one or two, never more
- **AND** the global Crunchy control is unchanged
- **WHEN** Randomize All is pressed many times
- **THEN** no Crispy changes on about half of those presses
- **AND** every one of the six banks is reached across the run

#### Scenario: The page press still owns its own Crispy
- **WHEN** Randomize Page is pressed on a parameter page
- **THEN** that bank's own Crispy control is randomized
- **AND** no other bank's Crispy control changes

#### Scenario: Crunchy is never randomized
- **WHEN** either randomize affordance is pressed, at any level
- **THEN** the global Crunchy control holds its value

### Requirement: Randomized source count is biased toward few, and depth storage is allocated once
The randomizer SHALL draw its source count geometrically, each count half as likely as the one below it, from a floor that depends on WHICH GESTURE was pressed and where: Randomize All at a drilled-in modulation level draws from a floor of one, so every such press draws at least one source and one source is the most likely outcome; every other randomize draw — Randomize All on a parameter page, and Randomize Page at any level — draws from a floor of zero, so about half of those calls draw no sources at all. Depth storage for a given source SHALL be allocated once, on first use, rather than accumulating additional storage across repeated randomization presses.

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
