# froggers-marbles-modulator Specification

## Purpose
Six Marbles-style Random S&H modulation sources with no source-level controls (five stepped with deja-vu loops, one smooth ganged-random source immune to deja-vu), advanced by the master-clock quarter-note pulse and shaped only through second-layer modulation, each rendering its character as a visualizer.
## Requirements
### Requirement: Six Marbles-style Random S&H sources
The app SHALL provide six random sample-and-hold modulation sources, labelled **Random S&H 1** through **Random S&H 6**, modelled on the Marbles random sampler, ordered so that a source's tendency to produce anomalous values falls with its number: the shorter a source's step period and the less it is slewed, the more often it takes a fresh value and the further from the centre that value can land. Five SHALL be stepped random voltages maintaining a remembered bag of eight stored values, with a fixed deja-vu character selecting, per step, between replaying those values, jumping among them, and overwriting them with fresh randoms. The sixth SHALL draw a fresh target and glide to it continuously, in the manner of Marbles' Y output, and is **not** affected by deja-vu.

#### Scenario: Locked deja-vu repeats a fixed loop
- **WHEN** a stepped source's deja-vu character is exactly the locked value and it is advanced repeatedly
- **THEN** the output cycles through the same remembered values
- **THEN** the cycle length matches that source's loop length

#### Scenario: Low deja-vu keeps producing new values
- **WHEN** a stepped source's deja-vu character is below the locked value and it is advanced repeatedly
- **THEN** stored values are progressively replaced with new randoms

#### Scenario: High deja-vu replays in a scrambled order
- **WHEN** a stepped source's deja-vu character is above the locked value and it is advanced repeatedly
- **THEN** no stored value is replaced
- **THEN** the read position jumps to a random slot with the probability that character sets

#### Scenario: The smooth source ignores deja-vu
- **WHEN** Random S&H 6 is active
- **THEN** its output glides between successive random targets rather than stepping
- **THEN** its behaviour does not change with deja-vu

#### Scenario: Smooth source movement duration follows tempo
- **WHEN** the master clock's tempo changes
- **THEN** Random S&H 6 recomputes its timing so that one wait-and-glide round spans sixteen quarter notes at the new tempo
- **THEN** its movement is tempo-proportional rather than locked to the quarter-note grid

#### Scenario: Activity and peak slope rank the sources
- **WHEN** every source is driven for the same span at one tempo and its output's mean absolute first difference per sample and its peak absolute first difference are measured
- **THEN** both quantities fall strictly from Random S&H 1 to Random S&H 6

### Requirement: The sources carry no source-level parameters
Random S&H sources SHALL expose no controls over their own behaviour — no loop length, deja-vu, spread, bias, rate, or slew control — and SHALL NOT occupy any bank slot. No dedicated page or additional bank SHALL be created for them. Each source's character SHALL be fixed at construction, chosen so the six differ usefully from one another.

This is distinct from their **modulation depth**, which is an ordinary bipolar encoder per target parameter, defaulting to neutral (no modulation). Those depth encoders are the operator's only direct control over a source, and modulating a depth is the second drill-in level.

#### Scenario: No Random S&H parameter appears on the grid
- **WHEN** every bank is enumerated
- **THEN** no bank contains a Random S&H source parameter
- **THEN** the number of banks is unchanged by the presence of these sources

#### Scenario: Depth is the control, and it starts off
- **WHEN** a parameter's modulation detail grid is opened for the first time
- **THEN** each Random S&H depth encoder reads neutral, contributing no modulation
- **THEN** the depth is bipolar, so the source can be applied in either direction

#### Scenario: Shaping happens through modulation
- **WHEN** the operator wants a source's effect to vary over time
- **THEN** they drill into that source's depth cell and modulate the depth itself
- **THEN** no control over the source's own behaviour exists anywhere

### Requirement: Stepped source character constants are new behavior
The sources' character constants SHALL be new behavior implemented for this capability, not carried over from the original Froggers random sampler. Spread SHALL be a distribution shape on the uniform draw with three fixed points: at 0.5 the draw is unchanged, at 1.0 every draw lands on an extreme, at 0 every draw lands on the centre; below 0.5 the shape contracts draws toward the centre and above 0.5 it expands them toward the extremes, without bounding any draw away from the extremes. A stepped source SHALL apply the shape before its level quantizer. The six sources' constants SHALL follow one table in which, from source 1 to source 6, the step period rises, the probability of a fresh value per step falls, spread falls from near-bimodal to centre-hugging, quantization coarsens toward source 1 and is absent from source 4 onward, and slew lengthens.

#### Scenario: Spread's three fixed points hold
- **WHEN** a set of uniform draws is shaped at spread 0.5, 1.0 and 0
- **THEN** at 0.5 every draw is unchanged
- **THEN** at 1.0 every draw is 0 or 1
- **THEN** at 0 every draw is 0.5
- **THEN** the mean absolute deviation from the centre at 0.25 is below that at 0.5, which is below that at 0.75

#### Scenario: Every axis is monotone in the source number
- **WHEN** the six sources' character constants are enumerated
- **THEN** period, fresh-value probability, spread, quantization and slew are each monotone from source 1 to source 6

### Requirement: Sources advance on the master clock quarter-note pulse
Every stepped source SHALL derive its rate from the master clock's quarter-note pulse, rather than from a free-running internal timer, each at its own fixed multiple or division of that pulse. These per-source rates SHALL be fixed: Random S&H 1 advances three times per quarter note (eighth-note triplets); Random S&H 2 advances twice per quarter note (eighth notes); Random S&H 3 advances once per quarter note; Random S&H 4 advances once per two quarter notes; Random S&H 5 advances once per four quarter notes; Random S&H 6 is smooth and is not stepped at all, per its own requirement.

#### Scenario: Advances track the clock
- **WHEN** the transport runs for a known number of quarter notes
- **THEN** each stepped source advances exactly that number multiplied by its own fixed rate ratio

#### Scenario: A bar means four quarter notes
- **WHEN** the master clock supplies quarter notes and no time signature
- **THEN** this capability defines one bar as four quarter notes
- **THEN** every "bar" referenced in a source's character means four quarter notes

#### Scenario: Tempo change tracks immediately
- **WHEN** the tempo is changed
- **THEN** the advance rate follows the new tempo without drift or reset

#### Scenario: External MIDI clock drives advancement
- **WHEN** the host is synchronized to an external MIDI clock
- **THEN** the sources advance on that external clock's quarter notes

#### Scenario: A missing clock plan is handled
- **WHEN** a processing block arrives with no clock plan
- **THEN** the sources do not advance and do not fault

### Requirement: Sources render their character as a visualizer
Each source SHALL provide a visualizer attached to the modulation source itself, so it appears as the underlay on every depth cell derived from that source. Stepped sources SHALL render their remembered values as a waveform across the loop, indicating the currently active position. The smooth source SHALL use the framework's existing ganged-random visualizer rather than a new one.

#### Scenario: Stepped visualizer matches the remembered values
- **WHEN** a stepped source's remembered values change
- **THEN** the rendered waveform reflects the new values
- **THEN** the indicated position matches the currently active value

#### Scenario: Smooth source reuses the framework visualizer
- **WHEN** Random S&H 6 is rendered
- **THEN** it uses the framework's ganged-random visualizer unmodified

#### Scenario: Depth cells show the visualizer
- **WHEN** a modulation detail grid is open
- **THEN** each Random S&H depth cell shows that source's visualizer as an underlay

### Requirement: Randomize All redraws the locked bags
Randomize All SHALL redraw every stepped source's bag of stored values from fresh randoms, so the locked and scrambled sources play a new phrase after the gesture, and the sources SHALL be seeded differently on each launch while a source built from an explicit seed remains reproducible.

#### Scenario: Randomize All changes a locked phrase
- **WHEN** a locked source's stored values are recorded and Randomize All is pressed
- **THEN** at least one stored value differs afterwards

#### Scenario: Randomize Page leaves the bags alone
- **WHEN** a locked source's stored values are recorded and Randomize Page is pressed
- **THEN** every stored value is unchanged

#### Scenario: Launches differ, explicit seeds do not
- **WHEN** two modulation slates are constructed in one process
- **THEN** at least one source's stored values differ between them
- **AND** two sources built from the same explicit seed hold identical values

