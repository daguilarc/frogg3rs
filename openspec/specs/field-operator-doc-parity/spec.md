# field-operator-doc-parity Specification

## Purpose

Keep Field hardware operator documentation (`DAISY_MANUAL.md`) aligned with engine external-mix behavior and FUEG semantics.
## Requirements
### Requirement: Field manual external mix documented plainly

`DAISY_MANUAL.md` SHALL describe external-input mix without product ring mod or FUEG/Crispy mix-topology language:

- Signal-flow ascii: parallel ring mod when external audio is present above the gate; VCO-only at `OLVL` when silent.
- No mix-topology table or FUEG morph between product and parallel.
- Audio page: external gate open → `(ext × VCO1 + ext × VCO2 + ext × VCO3) / 3`; FUEG does not shape external mix.
- Audio page exception: FUEG is fuegoizer plus PM3 depth only (no mix-topology bullet).

#### Scenario: Field signal flow

- **WHEN** reader opens `DAISY_MANUAL.md` signal-flow section
- **THEN** text does not mention FUEG continuum, product ring mod, or mix topology morph

#### Scenario: Field Audio FUEG row

- **WHEN** reader opens Audio page knob table row 8
- **THEN** FUEG is fuegoizer and PM3 depth only

### Requirement: Field manual documents button responsiveness expectations

`DAISY_MANUAL.md` SHALL state that SW1/SW2 and B1–B4 are polled on a fast path independent of OLED refresh, and that B2/B4 (Rand All / Rand All Mod) may complete over a few milliseconds while lighter buttons are immediate.

#### Scenario: Troubleshooting SW1/SW2

- **WHEN** reader opens SW1/SW2 troubleshooting in `DAISY_MANUAL.md`
- **THEN** text references control-loop responsiveness (not bootloader) and distinguishes dead switches (no LED) from slow OLED under load

