> Revised requirement approved after fresh-context review and implemented. Independent after-code reviewer A accepted the implementation, checks, manual and evidence; both independent final reviewers accepted delivery of main commit/push commands. See implementation.md.

## MODIFIED Requirements

### Requirement: Two randomize affordances
The app SHALL provide exactly two randomize affordances: Randomize All and Randomize Page. On a parameter page, Randomize All SHALL randomize every page parameter value across all six banks and its first-level modulation depths, SHALL randomize the local Crispy value of at most two distinct banks using the existing geometric count, SHALL leave Crunchy untouched, and SHALL NOT descend into second-level depths. In any modulation view, Randomize All SHALL randomize the selected parameter's depths and, below the existing maximum drill level, the next-level depths of its actually modulating depths, without changing the current view. Every source-count draw belonging to that press SHALL share its one selected density quartile. Randomize Page SHALL randomize only what is displayed: page values including that page's Crispy on a parameter page, or the selected parameter's depths in a modulation view; it SHALL NOT select or consume a density quartile.

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

#### Scenario: One quartile follows the drilled-in press
- **WHEN** Randomize All is pressed at any supported modulation-view level
- **THEN** the selected parameter and every next-level depth reached by that press use the same selected quartile
- **AND** descent visits only actually modulating parent depths and stops at the existing drill cap
- **AND** neither the selected view nor its level changes

#### Scenario: Randomize Page retains its own scope
- **WHEN** Randomize Page is pressed
- **THEN** only displayed values or depths are randomized, including the page's own Crispy value on a parameter page
- **AND** its modulation source counts retain the zero-floor geometric law and no quartile is drawn
- **AND** Crunchy is untouched

## REMOVED Requirements

### Requirement: Randomized source count is biased toward few, and depth storage is allocated once
**Reason**: Randomize All replaces the few-source bias with a shared fair density quartile. The complete replacement below retains Page's sparse law and the depth-storage obligations.
**Migration**: Replace this requirement with "Randomize All uses equal-probability density quartiles and preserves depth storage" below. Saved patch values and formats require no migration; update All-specific tests and manual together with implementation.

## ADDED Requirements

### Requirement: Randomize All uses equal-probability density quartiles and preserves depth storage
Each Randomize All invocation SHALL independently select one of four ordered density quartiles with probability one quarter each, with repetition permitted and no cycling, bag, history, stored setting or user control. That one selection SHALL apply to all modulation source-count draws made by the invocation, across banks and any drilled-in descent. Counts SHALL be sampled independently within that selected quartile of the allowed count distribution for each target. The minimum SHALL remain zero on parameter pages and one for drilled-in Randomize All when any source is eligible; the maximum SHALL be the target's connected-source count. Randomize Page SHALL retain its existing geometric source-count distribution from zero.

For a target with N eligible sources and N greater than zero, let m be the smaller of the gesture's minimum and N, K = N - m + 1, q the press's quartile in 0..3, and r an independent uniform integer in 0..K-1. The count SHALL be m + floor((q*K + r)/4). This defines four equal-probability quantile intervals without empty-band resampling: integer boundary counts can occur in adjacent quartiles when K is not divisible by four, and the marginal distribution over all four quartiles SHALL be uniform over m..N. If N is zero, the target SHALL receive no new sources and SHALL NOT draw an invalid count index. The press's quartile is still chosen once.

For every selected Randomize All route and each scene pole, the requested nominal base modulation depth, after its depth curve and before further modulation, SHALL be D=S*2^(-G), for bipolar targets, where G is an independent geometric random variable on nonnegative integers with P(G=k)=2^(-(k+1)), and S is an independent fair sign in {-1,+1}. For one-way targets, including drilled Crispy/Crunchy depths and their descendants, D SHALL instead equal positive2^(-G), without a sign draw; their scope and one-way behavior SHALL be preserved. These draws SHALL be independent of the count quartile and of other route/scene draws. The geometric law SHALL describe the effective depth after the existing depth curve, not the raw knob coordinate. The app SHALL convert into the framework coordinate and use its absolute edit API for unheld nominal commands, retaining the existing randomize mutator while a gesture is selected, without changing the DSP depth curve. Further submodulation and gesture blending may change the live output; they SHALL NOT be mistaken for the sampled base distribution. Randomize Page, top-level value randomization and Crispy's existing separate behavior SHALL retain their current laws. Existing held-gesture semantics SHALL take precedence over immediate scene-center assignment. No new minimum depth, tail resampling, product cutoff or tolerance change is authorized; ordinary finite numerical representation remains subject to fresh review before execution.

Sources selected for a target SHALL be distinct and connected. Each press SHALL clear depths within its existing scope before selecting the new set, preserving the same selected source membership in both scene poles and independently randomized depth values per pole through the framework mutator. Count selection SHALL NOT override held-gesture behavior, so a selected route is not a guarantee of a non-neutral depth, badge or audible change. Unselected neutral depth storage SHALL remain reclaimable; storage still used by a route, value or gesture SHALL remain protected.

Depth storage for a source SHALL be allocated on first use and reused where retained; unused depth storage SHALL be released under the existing lifecycle so repeated presses do not accumulate unused routes. The group's existing depth-storage low watermark SHALL remain tied to the launch-batch capacity. Saved or loaded patches SHALL receive their required storage before application. A press SHALL preserve existing detectable partial-allocation reporting and SHALL NOT deliberately truncate a selected count to make a denser draw fit. No new allocation cap or DSP law is introduced by the quartile choice.

#### Scenario: Selected All depths follow the geometric magnitude law
- **WHEN** Randomize All selects a route for ordinary unheld scene writes
- **THEN** each scene pole independently requests magnitude 1, 1/2, 1/4, ... of full effective depth with probabilities 1/2, 1/4, 1/8, ... and a fair independent sign for bipolar targets; one-way targets SHALL use positive sign only
- **AND** changing the source-count quartile does not change that depth law
- **AND** conversion through the existing framework curve preserves those effective magnitudes rather than applying the curve twice
- Delivery: implemented with runtime controls; independent after-code reviewer A accepted; both independent final reviewers accepted.

#### Scenario: All four faces are fair and repetitions are legal
- **WHEN** independent Randomize All presses choose their density quartiles
- **THEN** each of the four faces has probability one quarter and the same face can occur on consecutive presses
- **AND** only one quartile selection is made per invocation, regardless of the number of targeted parameters

#### Scenario: Fifteen connected sources give four familiar bands
- **WHEN** Randomize All is pressed on a parameter page with fifteen eligible sources
- **THEN** quartiles 0, 1, 2 and 3 draw counts respectively from 0..3, 4..7, 8..11 and 12..15, uniformly within the selected band
- **AND** each target independently draws its count while sharing the press's quartile

#### Scenario: Small and uneven eligible sets remain defined
- **WHEN** the number of allowed counts is not divisible by four, or fewer than four counts exist
- **THEN** the defined integer quantile mapping remains within the target's allowed bounds, permitting overlapping integer boundary counts without reselecting the quartile
- **AND** enumerating all equally likely q,r pairs gives every allowed count the same total multiplicity
- **WHEN** no connected source is eligible
- **THEN** no route is created and no zero-sized count draw is required

#### Scenario: Drilled-in Randomize All preserves its one-source floor
- **WHEN** Randomize All is pressed in a modulation view with eligible sources
- **THEN** the selected parameter and each reached next-level depth draw at least one source under the same quartile
- **AND** held-gesture semantics and the existing descent gate remain in force

#### Scenario: Randomize Page retains its sparse distribution
- **WHEN** Randomize Page is pressed in a modulation view
- **THEN** its source-count law remains geometric from zero, with no sources on half the mathematical draws
- **AND** it neither draws a quartile nor changes the scope of any later Randomize All press

#### Scenario: A zero-count target stays unassigned
- **WHEN** the count mapping selects zero for a target
- **THEN** that press creates no new route for that target, clears previous depths within the same existing scope, and retains the existing badge and reclamation semantics

#### Scenario: Repeated dense re-rolls retain only required storage
- **WHEN** the app processes fifty Randomize All presses at four blocks per press, including dense quartiles
- **THEN** the peak live local depth count is bounded by the retained routes and the existing release lifetime, not by accumulation of unused draws
- **AND** the check observes peak state and proves both that real routes were drawn and that disabling or delaying release is rejected
- Delivery: implemented after independent measurement/assertion acceptance, with disabled/delayed-release controls. Independent after-code reviewer A accepted; both independent final reviewers accepted. The old density-specific 120..250 bounds are not promoted here.

#### Scenario: Release retains used depths and takes unused ones
- **WHEN** one modulation depth is armed and another neutral, and randomization is pressed twenty-five times on another bank
- **THEN** the armed depth remains the same parameter and the neutral unused one is collected
- Existing check to rerun: `app/FroggersAudioRoutingTests.cpp: release_keeps_an_armed_depth_and_takes_a_neutral_one`.

#### Scenario: A dense page press after a drill session draws whole
- **WHEN** drilling has grown live depth storage past launch storage and Randomize All is pressed on a parameter page in its densest quartile
- **THEN** no target receives fewer routes than its count draw with correctly provisioned storage
- Existing check to adapt and rerun: `app/FroggersAudioRoutingTests.cpp: page_randomize_reports_no_partial_draw_after_a_drill_session_grows_depths_past_launch_storage`.

#### Scenario: Relaunch and running Load preserve grown patches
- **WHEN** a patch grown by dense randomization is reopened on the same data paths or loaded while running
- **THEN** every stored depth is provisioned before the first applying block, including a host state restore
- Existing checks to rerun: `app/FroggersAudioRoutingTests.cpp: a_relaunch_on_the_same_data_paths_opens_a_grown_patch_whole_before_the_first_block`; `app/FroggersAudioRoutingTests.cpp: a_running_load_of_a_grown_patch_applies_whole_with_storage_already_provisioned`; `app/vst/FroggersVstHostTests.cpp: state_information_restore_reopens_a_patch_grown_past_launch_storage_whole`.

#### Scenario: Same-tick Load and dense randomization remain whole
- **WHEN** a patch that fits is loaded in the same message tick as a dense Randomize All press
- **THEN** both patch application and the count draws are whole under existing provisioning
- Existing check to adapt and rerun: `app/FroggersAudioRoutingTests.cpp: a_small_patch_loaded_in_the_same_block_as_a_randomize_all_press_leaves_the_press_whole`.

Changed behavior and scenarios are implemented. Full modulation and the relevant full-audio storage checks passed; the full audio suite retains its previously established unrelated tempo failure. Independent after-code reviewer A accepted; both independent final reviewers accepted delivery of main commit/push commands.
