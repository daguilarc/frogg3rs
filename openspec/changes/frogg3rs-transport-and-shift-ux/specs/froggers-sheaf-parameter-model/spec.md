# Delta — `froggers-sheaf-parameter-model`

"A control's travel does not silently cost level" changes only in its
scenario's Check line, which names the limiter that caps the peak branch's
contribution: that limiter moves from the peak branch to the Filter page's
output. The requirement text and every other scenario clause are carried
forward word for word. The RESTATES-EXCEPT block under it names the one
promoted clause it changes, and `app/check_modified_requirements_restate_promoted.py`
checks it.

## MODIFIED Requirements

### Requirement: A control's travel does not silently cost level

A control whose name promises emphasis, boost or gain SHALL NOT cost output level silently: where its travel costs level that cannot be removed without degrading the output, the cost SHALL be stated where the control is described, and SHALL be pinned by a check measuring it. A cost that CAN be removed without degrading the output is removed instead of documented.

<!-- RESTATES-EXCEPT
any makeup compensating that sits ahead of `peakLimiter`
  keeps: The cost cannot be removed, only reduced
-->

#### Scenario: Peak gain's level cost is measured, stated, and pinned

- **WHEN** the Filter page's Peak gain is swept from its floor to its top, measured through the whole filter chain rather than through the resonant bump alone
- **THEN** the level at the resonant bump's own centre frequency stays flat across the whole travel
- **AND** the level at frequencies at least three octaves away falls at every step of that travel, which is the liveness control for the flat row and is asserted in the same case
- **AND** the manual and the quick dictionary each state what the control delivers at the output and what it costs, rather than quoting the bump's own centre gain
- Check: `app/FroggersDspParityTests.cpp`, `filter_bank_peak_gain_travel_measurement_at_and_away_from_resonance`, which drives the Filter bank through a replica of `RouteFilterBank`'s setter order at the registered defaults and pins both halves in one case: the level at the bump's own centre frequency stays flat across the whole travel, and the level on both rows at least three octaves away falls at every step and by several dB end to end, which is the flat row's liveness control. The manual and the quick dictionary each state what the control delivers at the output and what it costs. The cost cannot be removed, only reduced: the peak branch is divided by its own height, and any makeup compensating that sits ahead of the Filter page's output limiter, which every branch passes through after the Comb/Peak blend, whose ceiling is `kStageCeiling` and whose `OutputLimiter::DesiredMagnitude` asymptotes toward it without reaching it, so the branch's contribution is hard-capped whatever makeup precedes the limiter. An earlier wording of this requirement demanded that total level not fall at all, which is unsatisfiable at that placement. Every figure lives in the check rather than here, because a figure in prose cannot fail when it drifts.

## ADDED Requirements

### Requirement: The Filter page limits its output after the Comb/Peak blend

The Filter page SHALL limit its output with one limiter placed after the Comb/Peak blend, so the comb branch and the peak branch pass through the same limiter and neither reaches the pages after Filter unlimited, at every Comb/Peak, Topology, Comb feedback and Comb drive setting. Neither branch SHALL carry a limiter of its own ahead of the blend. The limiter's threshold SHALL sit below the master output limiter's threshold, and its ceiling SHALL be the per-stage ceiling every limiter other than the master's shares.

#### Scenario: A resonant comb is limited at the page's output

- **WHEN** the Comb/Peak blend sits at the comb end, comb feedback is at its maximum, Comb drive is at the bottom of its travel, and the input is a tone at the comb's own pitch
- **THEN** the blended signal ahead of the limiter exceeds the limiter's threshold, which is the liveness control for the clauses below
- **AND** the page's output is the limiter applied to the blended signal, sample for sample
- **AND** the page's output peak is lower than the same chain's with its limiter bypassed, in the same run
- Check: `app/FroggersDspParityTests.cpp`, `filter_fx_chain_limits_the_blended_output_so_a_resonant_comb_is_limited`, which drives a `FilterFxChain` with a below-unity-drive resonant comb, loud enough on the unlimited blend to exceed the limiter's threshold, and asserts that the shipped chain's output matches a fresh limiter applied to the unlimited blend sample for sample, and that the shipped peak is lower than the unlimited peak.

#### Scenario: Neither branch is limited ahead of the blend on a pinned comb

- **WHEN** Peak gain and comb feedback are both at their maximum, Topology puts the peak's input on the comb branch alone, and Peak frequency and Comb delay share the same registered-default pitch
- **THEN** the peak branch's own peak and the comb branch's own peak, each read before either reaches the blend, both exceed the limiter's threshold, which is the liveness control for the clause below
- **AND** a replica that applies no limiter to either branch, only to the blend, matches the chain's own output sample for sample
- Check: `app/FroggersDspParityTests.cpp`, `filter_fx_chain_limits_neither_branch_ahead_of_the_blend_on_a_pinned_comb`, which pins Peak gain and comb feedback at maximum, Topology at maximum so the peak's input is the comb branch alone, and Peak frequency and Comb delay at their shared registered-default pitch, then drives a full-scale sine at that pitch through both the shipped `FilterFxChain::Process` and a replica that applies the output limiter only to the blend, never to either branch alone, and asserts that the two match sample for sample, and that the peak branch's own peak and the comb branch's own peak, each read before the blend, both exceed the limiter's threshold.
