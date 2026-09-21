# Delta — `froggers-vst-host`

One requirement disentangles two senses of "bank" it used side by side:
which of the six pages the editor is showing (a display-selection concept,
restated here as "page," matching `FroggersActions`'s own `kResetPage`/
`kRandomizePage`) and a parameter's own model address, "bank and slot" (an
addressing concept, `FroggersBankId`-based, left as "bank" — see
`proposal.md`'s scope decision). The requirement's own title already used
"the operator's view," never "bank"; its body no longer needs the word twice
for two different things.

## MODIFIED Requirements

### Requirement: Automation does not steal the operator's view
THE plugin SHALL deliver an automated parameter's value to that
parameter's own bank and slot regardless of which page the editor is
currently showing. The editor's visible page SHALL follow operator
selection only: automation SHALL NOT move it, for a single lane or for
simultaneous lanes, and SHALL NOT close a modulation view the operator has
open.

#### Scenario: Simultaneous cross-bank lanes
- **WHEN** two automation lanes drive parameters in two different banks
  at once
- **THEN** each value lands on its own bank's parameter
- **AND** the editor's visible page does not move

#### Scenario: The operator is drilled into an automated bank
- **WHEN** the operator has a modulation view open on a page and a lane
  automates a parameter on that same page
- **THEN** the value lands on that parameter
- **AND** the open modulation view is neither closed nor written to

#### Scenario: Operator selection still moves the page
- **WHEN** the operator selects a different page
- **THEN** the editor's visible page follows that selection

<!-- RESTATES-EXCEPT
in that same bank
  keeps: automates a parameter
selects a different bank
  keeps: selects a different page
-->
