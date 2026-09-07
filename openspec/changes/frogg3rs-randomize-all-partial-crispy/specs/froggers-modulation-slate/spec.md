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
