# field-operator-doc-parity

## ADDED Requirements

### Requirement: A claim that names something is resolvable mechanically

A written claim that names a test, a file or a symbol SHALL be resolvable by a check that runs in the gate, because prose is the cheapest place in this repository to assert something false and the only place nothing fails on it. Where no automated check exists for a scenario, the specification SHALL say so in those words rather than naming a check that does not exist.

#### Scenario: A spec names a test that does not exist

- **WHEN** a `Check:` line names a test case or a file
- **THEN** the gate fails unless that test case is defined under `app/` or that path exists
- **AND** a `Check:` that begins `none` or `operator step` passes, so an honest gap stays sayable
- Check: `app/check_spec_checks_resolve.py`

#### Scenario: A comment cites a file that resolves nowhere

- **WHEN** a comment under `app/` cites a path and line
- **THEN** the gate fails unless that path resolves under `app/`, `External/Sheaf/` or `src/`, or the citation carries a git commit pin
- Check: `app/check_citations_resolve.py`

#### Scenario: A comment carries a planning label instead of behaviour

- **WHEN** a comment or string literal under `app/` names a task, item, group, design-list entry or rule section
- **THEN** the gate fails
- Check: `app/check_no_planning_history.py`

#### Scenario: A citation into this tree names a symbol rather than a line

- **WHEN** a comment under `app/` cites another file in `app/`
- **THEN** it names the symbol it is pointing at rather than a line number, because a line number into a tree under edit is wrong by the next commit
- **AND** the gate fails on a surviving `app/`-internal citation that still carries a trailing line number
- Check: `app/check_citations_resolve.py`
