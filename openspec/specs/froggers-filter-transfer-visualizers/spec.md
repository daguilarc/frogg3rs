# froggers-filter-transfer-visualizers Specification

## Purpose
The Filter page draws the main filter, comb and peak/notch frequency responses behind their controls.
## Requirements
### Requirement: A visualizer plots filter frequency response
The app SHALL provide a visualizer that samples a transfer function across the audible frequency range and renders it as a polyline, using Sheaf's portable draw commands. It SHALL be attachable as an underlay to filter parameters.

#### Scenario: State-variable filter response is rendered
- **WHEN** a state-variable filter's parameters are displayed
- **THEN** a response curve is drawn showing its peak, notch or slope at the configured frequency

#### Scenario: Comb response is rendered
- **WHEN** the comb filter's parameters are displayed
- **THEN** a response curve is drawn showing its periodic peak structure

#### Scenario: Extreme settings remain renderable
- **WHEN** the comb feedback is set near or beyond self-oscillation
- **THEN** the plotted curve contains only finite values
- **THEN** the visualizer bounds the displayed magnitude rather than drawing out of frame

### Requirement: Filter visualizers follow the app's visualizer conventions
Filter response visualizers SHALL be implemented as standard Sheaf visualizers and SHALL be dimmed-behind-the-knob underlays consistent with other parameter visualizers, not bespoke UI panels.

#### Scenario: Underlay renders beneath the encoder
- **WHEN** a filter parameter with a response underlay is displayed
- **THEN** the response curve renders beneath the encoder body
- **THEN** the encoder body is dimmed so the underlay remains visible

### Requirement: Filter stages expose a transfer function

The main filter, the peak/notch filter and the comb filter SHALL each expose Sheaf's transfer-function interface, reporting frequency response derived from their current coefficients. `synth::ClassicStateVariableFilter::UIState` already provides it for the low-pass, band-pass and high-pass blend; the peak-to-notch blend is not a mode of that class, so its response is the same weighted sum of low-pass, band-pass and high-pass responses that the peak/notch blend law applies to the filter's states, built from the class's static `TransferFunctionValue` at blends -1, 0 and +1.

#### Scenario: Response follows parameter changes

- **WHEN** a state-variable filter's cutoff, resonance or blend is changed
- **THEN** its reported frequency response changes correspondingly
- Check: not yet delivered; execution task E5.

#### Scenario: Comb response reflects delay and feedback

- **WHEN** the comb filter's delay or feedback is changed
- **THEN** its reported response changes its peak spacing or peak sharpness accordingly
- Check: not yet delivered; execution task E5 carries this promoted scenario forward unchanged in meaning.
