# field-button-input-latency Specification

## Purpose

Daisy Field firmware control-loop architecture for responsive tactile switches and randomize buttons under audio load: fast input polling decoupled from OLED refresh, queued heavy randomize, and toolchain parity with proto Froggers.

Hardware diagnostic findings (e.g. SW1 stuck-input on a specific unit) are recorded in `DAISY_MANUAL.md`'s Troubleshooting section and are out of scope for latency acceptance on affected hardware.
## Requirements
### Requirement: Fast control poll decoupled from OLED refresh

Field firmware SHALL sample tactile switches (SW1/SW2), keyboard edges (B1–B5, A1–A8), and ADC knobs on a fast main-loop path that is not blocked by a full SSD1306 redraw every iteration. OLED updates SHALL be throttled (maximum ~30 full frames per second) or driven by a dirty flag after page/param changes.

#### Scenario: SW1 polled while OLED would block

- **WHEN** the main loop would previously spend longer than 10 ms in `UpdateScreen()`
- **THEN** `ProcessAllControls()` still runs at least once before the next full OLED refresh

#### Scenario: Page change marks dirty

- **WHEN** SW1 or SW2 triggers `PagePrevious` or `PageNext`
- **THEN** the OLED refresh occurs on the next allowed frame boundary without delaying switch edge detection

### Requirement: Heavy randomize is queued

Field firmware SHALL enqueue `RandomizeAllPages` (B2) and `RandomizeAllPagesMod` (B4) on rising edge and apply them incrementally across main-loop iterations. The control poll loop SHALL NOT run either operation synchronously to completion in a single iteration. Duplicate pending Rand-All requests SHALL coalesce to one pending mutation.

#### Scenario: B2 under audio load

- **WHEN** the user presses B2 while `FroggersEngine` is processing a full block
- **THEN** SW1/SW2 remain responsive within the same session (no multi-hundred-ms poll gap attributable to Rand All)

#### Scenario: B1 remains immediate

- **WHEN** the user presses B1 (randomize current page knobs)
- **THEN** current-page knob values update in the same main-loop iteration (no queue)

### Requirement: SW1 and SW2 work during mod-assign

Field firmware SHALL process SW1/SW2 page switches regardless of `PageManager::m_modIndex`. A page switch SHALL exit mod-assign mode per existing `PageNext`/`PagePrevious` behavior.

#### Scenario: SW1 while A1 held

- **WHEN** the user holds A1 (mod-assign) and presses SW1
- **THEN** the page changes and mod-assign ends

### Requirement: Toolchain parity with proto Froggers baseline

Firmware builds SHALL use `APP_TYPE=BOOT_NONE`, `OPT_LEVEL=-Os`, `USE_LTO=1`, and Arm GNU Toolchain 14.3.rel1 as documented in `src/mk/config.mk`. No switch to bootloader app types is required for this capability.

#### Scenario: Release build flags

- **WHEN** `make` runs in `src/FroggersSolo` or `src/FroggersGuitar` without overrides
- **THEN** the effective compile flags are the ones `src/mk/config.mk` sets (no accidental `-O0`, no `BOOT_SRAM` default)

### Requirement: Acceptance bench for button latency

Release verification SHALL include a manual bench: rapid SW1/SW2 taps (≥5 presses in 2 s) with audio playing and knobs mid-travel; page title on OLED and SW tactile LEDs SHALL track presses without sustained unresponsiveness (>200 ms dead window).

#### Scenario: Full-load SW test

- **WHEN** audio is playing with external input gated or VCO mix active and user taps SW1 repeatedly
- **THEN** at least 4 of 5 presses change the visible page name on OLED within one throttle frame of the press

### Requirement: Parameter smoothing is not moved to block rate

`FroggersEngine` SHALL continue to apply parameter updates once per sample.
`RuntimeParam`'s one-pole advances one step per call with its alpha derived from
a 1 kHz natural frequency at the sample rate, so calling it once per block would
change the smoothing time by the block size.

At the Field's block size of 48 and rate of 48 kHz, a block-rate call happens at
1 kHz, and a 1 kHz cutoff cannot be expressed at a 1 kHz update rate:
`OPLowPassFilter::SetAlphaFromNatFreq` clamps at `x_maxCutoff` 0.499. Measured
time to 90% of a step is 0.375 ms per sample, 18.0 ms per block at the same
alpha, and 1.0 ms per block re-derived — the last by clamping the request away
rather than honouring it.

The separable cost was the coefficient recompute, not the smoother advance, and
it is removed by the requirement below instead.

#### Scenario: Smoothing time is unchanged by this work
- **WHEN** a parameter is stepped to a new target
- **THEN** it reaches 90% of the step in the same number of samples as before

### Requirement: Per-sample filter coefficients are computed once

`FroggersEngine` SHALL compute each output filter's coefficients at most once
per sample, and SHALL read each parameter smoother at most once per sample.

A filter whose output no sample reads SHALL NOT be configured at all.

#### Scenario: One coefficient recompute per sample
- **WHEN** a sample is processed
- **THEN** `ResonantBump::UpdateCoefficients` runs once, not six times

#### Scenario: Each smoother advances once per sample
- **WHEN** a sample is processed
- **THEN** no `RuntimeParam` has `Process()` called on it more than once

### Requirement: LED transmission is throttled

LED state SHALL be computed every poll and transmitted on change or at a bounded
rate, so that I2C traffic does not scale with poll rate.

#### Scenario: Static LEDs do not transmit every poll
- **WHEN** no LED value changes between polls
- **THEN** no transmission is issued for those polls

### Requirement: A silent reverb costs no reverb processing

On builds that HAVE a reverb page, reverb processing SHALL be skipped while the
mix rests at zero, with distinct enter and exit thresholds so a knob resting at
the boundary does not alternate between states.

Froggers Guitar has no reverb page and this requirement does not apply to it.

#### Scenario: Zero mix skips the reverb
- **WHEN** the reverb mix is at zero on a Solo build
- **THEN** reverb processing does not run for that sample

#### Scenario: A knob at the threshold does not chatter
- **WHEN** the mix sits exactly at the bypass boundary
- **THEN** the state does not alternate between processed and skipped

### Requirement: Improvement claims carry a measured quantity

A latency or freezing fix SHALL NOT be reported as delivered on the strength of
the edit alone. The controlling quantity SHALL be named, measured before and
after, and both numbers reported. Where the quantity did not move, the result is
void rather than negative.

#### Scenario: A fix with no moved number
- **WHEN** a headroom change is made and no measured quantity moves
- **THEN** the change is reported as unproven rather than as an improvement

