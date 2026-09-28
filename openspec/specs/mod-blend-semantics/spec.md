# mod-blend-semantics Specification

## Purpose
Standardize modulated parameter blending as depth crossfade via ModMgr::Modulate across page rows, Delay sidecar, and pair-AR targets.
## Requirements
### Requirement: Modulation uses crossfade blend not attenuator multiply

The host engine SHALL compute each modulated parameter's effective value (0–1) as:

`effective = clamp(base × (1 − depth) + modSource × depth, 0, 1)`

where `base` is the stored knob value, `depth` is mod amount, and `modSource` is the current sample on mod bus index `modIndex`. This is a **crossfade**, not `base × modSource` (a multiplicative attenuator).

All sim hosts SHALL implement this via `ModMgr::Modulate` (not duplicated inline crossfade math) for page rows and Delay sidecar rows.

This requirement governs the firmware host engine only (`src/core/ModMgr.hpp`, consumed by `src/core/Parameter.hpp`'s `GetPreFuegoization`, `src/core/FroggersEngine.hpp`, and `src/core/Page.hpp`) — the Daisy Field hardware pedal's own modulation resolution, which `modulation-depth-attenuverter` does not edit: ruled by the operator on 2026-09-28, firmware under `src/` does not change. Frogg3rs's Sheaf-based app engine (`app/`, `External/Sheaf/projects/synth`) does not use `ModMgr::Modulate` at all (confirmed: no reference to `ModMgr` exists outside `src/` and `test/firmware/`) and resolves modulation through Sheaf's own `Parameter::ComputeAtDepth`/`GetRaw` instead, per `froggers-sheaf-parameter-model` — whose scope note that Sheaf's `ParameterManager` is "the sole parameter authority" for the app build already established these as two independent implementations before this change. As of `modulation-depth-attenuverter`, Froggers' app-side `ParameterGroup` uses a different law (attenuverter, not crossfade) than this requirement describes, with no carve-out for Crispy or Crunchy (also ruled by the operator on 2026-09-28: those two are ordinary attenuverter targets like any other Froggers parameter); the two engines are not required to agree, and a player's hardware pedal and desktop/VST/browser build can sound different at the same depth setting as a result — this is the ruled outcome, not an oversight.

#### Scenario: Zero depth returns base only

- **WHEN** a parameter has mod source assigned and mod depth = 0
- **THEN** effective value equals stored base regardless of mod bus level

#### Scenario: Full depth returns mod source only

- **WHEN** mod depth = 1.0, base = 0.25, and mod bus sample = 0.80
- **THEN** effective value ≈ 0.80

#### Scenario: Half depth crossfades

- **WHEN** mod depth = 0.5, base = 0.0, and mod bus sample = 1.0
- **THEN** effective value ≈ 0.5

### Requirement: Modulation applied before fuegoization

For fuego-enabled parameters (page rows 1–7, Delay rows 1–7), the pipeline SHALL be:

1. Mod crossfade on stored base
2. Fuego bit-scramble using effective Crispy amount (mod crossfade on Crispy when assigned)

Fuego SHALL NOT apply to Crispy itself.

#### Scenario: Mod then fuego on page row

- **WHEN** Audio row 1 has LFO mod at depth 1.0 and Crispy > 0
- **THEN** DSP receives fuego-scrambled LFO value, not fuego-scrambled base then modulated

#### Scenario: Modulated Crispy drives scramble intensity

- **WHEN** Crispy has VCO Envelope mod at depth 1.0 on Audio page
- **THEN** fuego mask on rows 1–7 follows the modulated Crispy level

### Requirement: Base knob and mod depth are separate stored values

When mod source index ≠ 255, the host SHALL retain base independently of mod depth. Unpatching restores base display.

#### Scenario: Depth edit preserves base

- **WHEN** base = 0.30, mod assigned, depth set to 0.8
- **THEN** stored base remains 0.30

#### Scenario: Unpatch restores base display

- **WHEN** mod source cleared after modulation was active
- **THEN** UI returns stored base, not last effective value

### Requirement: External CV mod sources gated by presence

For mod indices 0–3, `ModMgr::Modulate` SHALL return base unchanged when `m_externalCvActive[index]` is false. Delay sidecar SHALL use `ModMgr::Modulate` so this gating applies equally.

#### Scenario: Inactive external CV ignored on Delay row

- **WHEN** MIDI CC 1 assigned to Delay Send with depth > 0 but CC input inactive
- **THEN** effective Send equals stored base

### Requirement: First mod assignment default depth on page rows

When `SetPageModSource` assigns mod to a page-row parameter with prior depth below epsilon, depth SHALL initialize to **0.5**.

#### Scenario: Fresh page patch defaults to half depth

- **WHEN** mod newly assigned to page row with depth ≈ 0
- **THEN** depth becomes 0.5

