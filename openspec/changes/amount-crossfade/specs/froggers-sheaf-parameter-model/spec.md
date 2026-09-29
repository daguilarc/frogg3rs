# Delta — `froggers-sheaf-parameter-model`

Every clause the promoted requirement below already asserts about the depth
`Parameter`'s own storage, persistence, Reset, source registration, the floor,
Randomize, and the ring presentation is carried forward verbatim, as is every
scenario documenting one of those. Only the resolved-value LAW changes
(add-only to crossfade), so every scenario that stated the old add-only law's
own numbers is superseded outright by a new scenario stating the crossfade
law's numbers instead, rather than edited bullet-by-bullet in place; no
`RESTATES-EXCEPT` block is used, because no bullet is dropped from within any
scenario this delta still carries under its original title — only whole
superseded scenarios are dropped, which the promoted-requirement check reports
as a note, not a defect.

## MODIFIED Requirements

### Requirement: Crispy and Crunchy targets take one-way amount modulation, not the attenuverter's bipolar swing

Crispy (all six banks) and Crunchy SHALL be registered with `ParameterConfig`'s `modulationTargetKind` set to `kOneWayAmount` (Sheaf's spm-94), so every one of their modulation routes resolves under Sheaf's `kCrossfade` weighting (spm-9), restricted to non-negative depths, rather than under the requirement above's own attenuverter law: with `W = Σ_route depth[route]`, `value = clamp((1 − W) × knob + Σ_route depth[route] × u[route], [0, 1])` when `W ≤ 1`, and `value = clamp(Σ_route (depth[route] / W) × u[route], [0, 1])` when `W > 1`, where `depth[route] ∈ [0, 1]` (one-sided: off to full, with no negative half and no inverting position) and `u[route]` is that route's own registered source read as a plain amount — `u[route] = connected(route) ? modulatorSource[route] : 0` — REGARDLESS of that source's own `restsAtZero` registration: an LFO-like source's own stored value, which sweeps `[0,1]`, and an envelope follower's own stored value, which rises from `0`, are both read exactly as stored, with no bipolar recentring of either. This replaces the earlier add-only law (`value = clamp(knob + Σ_route depth[route] × u[route], [0, 1])`), which left no modulation headroom once the knob reached `1.0` and never let a source pull the result below the knob: under the crossfade law the knob's own commanded value keeps full weight only while `W = 0`, loses weight as any depth is assigned, and drops out entirely once `W` reaches `1` — at that point Crispy/Crunchy resolve to the source(s) directly, exactly as any other `kCrossfade` parameter would at full effective depth. This is still a deliberate asymmetry from the requirement above, not an oversight: a bipolar-metadata source at its own rest (stored `0.5`) still contributes to a Crispy/Crunchy route with no `restsAtZero` halving, where the identical source at the identical rest value contributes exactly `0` to a real (sound) parameter under the requirement above. `design.md` states and proves this substitution in full; this requirement states the outcome the player and a future maintainer need, not the derivation.

Every mechanism the requirement above shares with this one is unchanged by this requirement and is not restated here: the depth `Parameter`'s own storage (still raw `[0,1]`, still `RangeKind::Bipolar`, still defaulting to `kNeutralModulationDepthCenter`), `Parameter::ToValueJSON`/`LoadValuesFromJSON` (patch persistence), `Parameter::RevertToDefault`/`RevertAllToDefault` (Reset), and `ModulatorMetadata`/`RegisterSources()` (source registration) — `design.md`'s "What does not change" section traces each.

This requirement's own depth knob cannot be turned below off: Sheaf's `Parameter::EnforceOneWayAmountFloor` (Sheaf's spm-94) floors the stored knob value at `kNeutralModulationDepthCenter` on every write path Sheaf has — encoder tick, absolute set (UI and MIDI), patch load, and Reset/default — so turning left from off stays at off, a legacy patch's negative-encoded value loads as off with no migration, and a value already at or above off loads unchanged. This requirement does NOT change the depth cell's own ring presentation away from Sheaf's generic bipolar-shaped display: with the floor in place, that ring only ever fills from its own centre (off) rightward to full, which is the shipped presentation, decided, not a placeholder (`design.md`'s own "Decisions applied" section).

#### Scenario: A connected route crossfades with the knob, never subtracts
- **WHEN** Crispy or Crunchy's own knob is at `0.3` and one connected route has depth `0.6` and source value `0.8` (`W = 0.6`)
- **THEN** the resolved value is `(1 − 0.6) × 0.3 + 0.6 × 0.8 = 0.60`
- Check: Sheaf test `one_way_amount_target_adds_reach_and_ignores_disconnected_sources` asserts this same working point, knob 0.3 depth 0.6 source 0.8 resolving to 0.60; frogg3rs `app/FroggersModulationTests.cpp`'s own case is `crispy_depth_crossfades_toward_the_source_and_the_floor_disables_it`, at a different working point on the same law, knob 0.1 full depth source 0.8 resolving to 0.8.

#### Scenario: A disconnected source contributes nothing regardless of its own stored value or the assigned depth
- **WHEN** a route's own source is registered `connected = false`
- **THEN** that route's contribution is `0` at any depth, and Crispy/Crunchy's resolved value equals the knob's own value unchanged
- Check: not yet delivered by this change; frogg3rs task 1.2 adds this case.

#### Scenario: Several routes crossfade below the ceiling, unrenormalized
- **WHEN** Crispy or Crunchy's own knob is at `0.2` and two connected routes contribute depth `0.5`/source `1.0` and depth `0.4`/source `1.0` (`W = 0.9`)
- **THEN** the resolved value is `(1 − 0.9) × 0.2 + 0.5 × 1.0 + 0.4 × 1.0 = 0.92`, and neither route's own depth is divided or rescaled by the other's presence, since `W ≤ 1`
- Check: Sheaf test `one_way_amount_two_routes_crossfade_and_renormalize_past_the_top` asserts this same working point (its own `SubCeilingCarrier` case).

#### Scenario: A bipolar-metadata source at its own rest is reached directly once full depth crossfades the knob away
- **WHEN** Crunchy's own knob is at `0.4` and one bipolar-metadata source (rest value `0.5`, e.g. External Audio connected with a silent input) is assigned full depth `1.0` and held at its own rest (`W = 1.0`)
- **THEN** the knob drops out entirely (center scale `0`) and the resolved value is `1.0 × 0.5 = 0.5`, not `0.4` (the knob) and not `0.9` (the superseded add-only law's own value) — the named asymmetry from the requirement above (no `restsAtZero` halving) still applies, substituted into the crossfade law instead
- Check: frogg3rs test `crunchy_full_depth_crossfades_to_the_source_at_its_own_rest`; Sheaf test `one_way_amount_ignores_restsatzero_and_reads_the_route_directly` asserts the underlying law's own case at a different working point.

#### Scenario: An envelope-follower route is read directly, never halved
- **WHEN** a route registered `restsAtZero = true` is assigned depth `1.0` and its own stored source value is `0.3`
- **THEN** that route's contribution is `1.0 × 0.3 = 0.3`, not `0.15` — the requirement above's own `restsAtZero` halving does not apply to a one-way amount target
- Check: not yet delivered by this change; Sheaf task S1.4 adds this case.

#### Scenario: The depth knob's own curve keeps its positive-branch shape and floors the rest at off
- **WHEN** the one-way depth curve is evaluated directly at raw input `0.75` (previously encoding a positive bipolar depth), `0.25` (previously negative), or `0.5` (previously neutral, and this change's own default)
- **THEN** the resolved depth is, respectively, the SAME positive magnitude the old curve already produced at `0.75`, exactly `0` at `0.25`, and exactly `0` at `0.5`
- **AND** no patch migration or version marker is introduced — the same raw input is read differently only because Crispy/Crunchy's own `modulationTargetKind` selects a different curve
- Check: not yet delivered by this change; Sheaf task S1.2 adds the curve's own case.

#### Scenario: The depth knob's own stored value cannot be turned or loaded below off
- **WHEN** a Crispy/Crunchy depth is turned left past off with the encoder, set absolutely (UI or MIDI) to a value that would otherwise resolve below off, or loaded from a patch whose stored value is `0.25` (a legacy negative encoding)
- **THEN** the depth's own stored raw value reads exactly `0.5` (off) in every one of the three cases, not merely its resolved depth
- **AND** the same patch load with a stored value of `0.75` (a legacy positive encoding) leaves the stored value at `0.75`, unchanged
- **AND** no patch migration or version marker is introduced — the same floor construct that governs the encoder and MIDI also governs patch load
- Check: not yet delivered by this change; Sheaf task S1.5 adds the law's own case, and frogg3rs task 1.4 adds the patch-round-trip case.

#### Scenario: Randomize still attaches a live (nonzero) depth at the rate the manual advertises
- **WHEN** Randomize draws a fresh raw value for a newly-attached Crispy/Crunchy route
- **THEN** that draw is biased into the upper half of raw storage (`[0.5, 1.0]`) before being interpreted, so the attached route resolves to a nonzero depth except at the single zero-probability draw that lands exactly on `0.5`
- **AND** Crispy/Crunchy's own top-level VALUE (not a depth) is unaffected by this bias and still draws from the full range
- Check: not yet delivered by this change; Sheaf task S1.6 adds the law's own case, and frogg3rs task 1.5 adds the end-to-end case.

#### Scenario: Nested depth-of-a-depth inherits the one-way law
- **WHEN** a modulation view is opened on one of Crispy or Crunchy's own depths' own depth (the existing three-level limit)
- **THEN** that nested depth parameter resolves under the identical one-way law as its top-level ancestor, inherited by construction the same way the requirement above's own nested scenario is
- Check: not yet delivered by this change; Sheaf task S1.3 adds this case.

#### Scenario: Every other Sheaf app and every other Froggers parameter is unaffected
- **WHEN** `braid-4`, `miniapp`, and every Froggers page parameter are inspected
- **THEN** none of them sets `modulationTargetKind`, so all default to `kBipolar` and resolve exactly as the requirement above states, byte-for-byte unchanged by this requirement
- Check: not yet delivered by this change; Sheaf task S1.7's full `test` target run is the literal proof.
