## ADDED Requirements

### Requirement: Parameter coverage and source counts are separate draws
Randomize All SHALL select one independent fair quartile q in 0..3 per press.
On a parameter page, its coverage population SHALL be the 84 ordinary parameters
across all six banks. For T eligible targets it SHALL select
floor((q*(T+1)+r)/4) distinct targets, where r is independently uniform in 0..T.
It SHALL clear previous depths for every target in scope, including unselected
ones. Quartiles SHALL NOT determine source count per selected target.

Conditional on target selection and N>0 connected sources, source count c in
1..N SHALL have weight 2^(-abs(c-min(2,N))), normalized over that finite range.
The mode SHALL therefore be two whenever N>=2. Counts at successive steps away
from the mode SHALL halve in weight. N=0 SHALL create no routes. Selected sources
SHALL be distinct and connected. Source count SHALL be independent per target,
of coverage quartile and of independent depth magnitude/sign draws.

In a modulation view, the selected parameter SHALL receive the same source-count
law, with a one-source minimum when connected sources exist. Below the drill cap,
the quartile SHALL choose coverage among its actually modulating depth parameters;
unselected descendants SHALL be cleared. The view and cap SHALL be preserved.
Randomize Page SHALL retain its existing sparse zero-floor count/value laws.
Geometric depth magnitudes, positive-only one-way amounts, fair bipolar signs,
held-gesture mutators, partial-allocation reporting, storage reclamation and
Crispy/Crunchy exclusions SHALL remain in force.

#### Scenario: Dense coverage does not make dense source counts common
- **WHEN** the same independently scripted two-source draw is used in each quartile
- **THEN** the number of modulated parameters varies with coverage and every selected parameter still receives two sources
- Check: `app/FroggersModulationTests.cpp: quartile_controls_target_coverage_not_sources_per_target`.

#### Scenario: Source counts retain their two-sided geometric law
- **WHEN** every sampler entropy ticket is enumerated for each connected-source count from one through fifteen
- **THEN** two has maximum multiplicity when available and weights halve at each step away from it
- Check: `app/FroggersModulationTests.cpp: modal_source_count_has_exact_geometric_ticket_weights`.

#### Scenario: Drilled coverage remains within the current view
- **WHEN** Randomize All is pressed at any supported drill level
- **THEN** source counts retain their independent law, deeper target coverage follows the quartile, and no further level is reached beyond the cap
- Check: `app/FroggersModulationTests.cpp: randomize_all_coverage_respects_every_drill_level_and_cap`.

#### Scenario: Repeated presses preserve working audio
- **WHEN** 200 Randomize All draws each process one second of actual app audio
- **THEN** output remains finite, within full scale and audibly nonzero on both channels
- Check: `app/FroggersAudioRoutingTests.cpp: randomize_all_storm_test_never_blows_out_or_permanently_silences`.
