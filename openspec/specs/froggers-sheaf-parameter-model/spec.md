# froggers-sheaf-parameter-model Specification

## Purpose
Monophonic Sheaf parameter/bank model for Froggers: one sixteen-slot bank per Froggers page, fixed Crispy/Crunchy slots at indices 14/15, Shape controls registered as ordinary bank parameters, per-bank colors, Sheaf scene state, and a defined non-neutral initial patch — with Sheaf's `ParameterManager` as the sole parameter authority.
## Requirements
### Requirement: Monophonic Sheaf parameter model
All Froggers parameters SHALL be registered through Sheaf's `ParameterManager` into `ParameterGroup`s configured with `numVoices = 1`. Froggers value scaling SHALL be preserved for every ported parameter. No bespoke parameter, inventory, or randomization model SHALL be introduced beside Sheaf's.

#### Scenario: Groups are monophonic
- **WHEN** the app initializes its parameter groups
- **THEN** every group reports `numVoices == 1`
- **THEN** each encoder renders a single value ring rather than stacked per-voice arcs

#### Scenario: Single authority
- **WHEN** the app is inspected for parameter state
- **THEN** exactly one `ParameterManager` and one `BankSlot` exist
- **THEN** no parallel parameter table, page-state, or randomization mutator exists

### Requirement: One sixteen-slot bank per Froggers page
Each existing Froggers page SHALL become exactly one bank of sixteen parameter slots. Pages SHALL NOT be
merged. A bank's own parameters SHALL occupy the leading slots; remaining parameter slots MAY be empty,
or MAY hold additional named parameters where a bank's slate has been explicitly decided and expanded.

#### Scenario: Page identity is preserved
- **WHEN** the banks are enumerated
- **THEN** there is one bank per original Froggers page
- **THEN** each bank contains that page's parameters and no other page's

#### Scenario: Sparse banks are valid
- **WHEN** a bank has fewer parameters than available slots
- **THEN** the unused slots render as empty
- **THEN** the occupied slots keep their positions rather than being renumbered

#### Scenario: The Audio bank holds fourteen parameters, complete
- **WHEN** the Audio bank is enumerated
- **THEN** it holds fourteen named parameters at slot indices 0 through 13, not nine
- **THEN** slots 0-8 are unchanged from the Audio bank's existing nine parameters, without every one of
  the nine necessarily originating as a page row
- **THEN** three of the original nine are the VCO Shape controls rather than page rows
- **THEN** slots 9 through 11 are Ring Mod (short names `RM1`, `RM2`, `RM3`), one per VCO — each VCO's own
  ring modulator carrying an internal carrier generated inside that VCO's own ring-mod stage, never a
  signal from another VCO
- **THEN** each Ring Mod knob's resolved value sets its own VCO's internal carrier frequency across an
  audio-rate range, mapped the same exponential way the Audio bank's existing pitch knobs already map
  pitch, and each VCO's own signal is multiplied by its own carrier's output
- **THEN** the bottom of each Ring Mod knob's travel is a true zero position, gating that VCO's ring-mod
  amount to exactly zero and ramping smoothly out of it, so the knob is continuous and the VCO can be heard
  unmodulated, per the shared ramp the `froggers-vco-topology` delta requires
- **THEN** each Ring Mod knob defaults to a position at or below that zero floor, so the instrument at its
  defaults sounds exactly as it did before Ring Mod existed
- **THEN** slot 12 is PM Rate (short name `PMrt`), the phase-modulation LFO's own rate, shared across all
  three VCOs and independent of the phase-modulation depth the existing Phase-mod knobs already control
- **THEN** slot 13 is VCO Balance (short name `VBal`), a single tilt sweeping mix emphasis across VCO1
  through VCO3, replacing the fixed equal-thirds average with a constant-total-gain crossfade, subject to
  the floor and cap the "VCO Balance keeps every VCO in the mix" scenario below requires

#### Scenario: VCO Balance keeps every VCO in the mix
- **WHEN** the VCO Balance crossfade weights are computed for any knob position
- **THEN** the three weights SHALL sum to exactly 1
- **THEN** each weight SHALL stay within the range 0.10 to 0.80 inclusive
- **THEN** no knob position reduces any VCO's weight to 0, and no knob position raises any VCO's weight to
  1.0
- **THEN** the three-VCO, 10%-floor arithmetic caps any single VCO's weight at 1 minus two floors, i.e.
  0.80, so the floor and the cap are the same constraint expressed from opposite ends

#### Scenario: The Envelope bank holds fourteen parameters in interleaved ADSR order
- **WHEN** the Envelope bank is enumerated
- **THEN** it holds fourteen named parameters at slot indices 0 through 13, not nine
- **THEN** slots 0-3 are Attack VCO1, Decay VCO1, Sustain VCO1, Release VCO1 (short names A1, D1, S1, R1)
- **THEN** slots 4-7 are Attack VCO2, Decay VCO2, Sustain VCO2, Release VCO2 (short names A2, D2, S2, R2)
- **THEN** slots 8-11 are Attack VCO3, Decay VCO3, Sustain VCO3, Release VCO3 (short names A3, D3, S3, R3)
- **THEN** slot 12 is Curve, applying to all three voices' Attack/Decay/Release ramp shape
- **THEN** slot 13 is Grace, a minimum Hold duration so a short gate cannot clip a note before its
  envelope completes Attack and Decay
- **THEN** each voice's Attack ramps to a peak, Decay then falls from that peak to the voice's own
  Sustain target level, and Hold sustains at that level exactly as it does today

#### Scenario: The Filter bank holds fourteen parameters, grouped by stage

- **WHEN** the Filter bank is enumerated
- **THEN** it holds fourteen named parameters at slot indices 0 through 13, grouped by the stage they
  belong to, with saved patches unaffected because persistence addresses parameters by name
- **THEN** slots 0-2 are the Peak stage: Peak Freq (`PkFreq`), Peak Gain (`PkGain`), Peak Q (`PkQ`)
- **THEN** slots 3-7 are the Comb stage: Comb Offset (`CmbOff`), Comb Delay (`CmbDly`), Comb Feedback
  (`CmbFb`), Comb LP (`CmbLP`), and Comb Drive (`CDrv`) — a saturation-depth control whose drive factor
  multiplies the in-loop saturator's input while the same factor divides its output, so the loop's
  small-signal gain is unity at every setting, the fed-back term never exceeds the delayed signal it is
  computed from, and the loop's decay argument is preserved at every drive
- **THEN** slots 8-11 are the Scoop stage: Scoop Mix (`ScMix`, the notch's wet/dry blend into the chain's
  shared input, ahead of both the Comb and Peak stages), Scoop Freq (`ScFq`, the notch's own center
  frequency, independent of the Peak stage's), Scoop Width (`ScWd`, the notch's own width, independent
  of the Peak stage's), Scoop Depth (`ScDp`, the notch's own dip depth, independent of how much of it
  reaches the chain's input)
- **THEN** slots 12-13 are the routing pair: Comb/Peak (`Cmb/Pk`) and Topology (`Topo`) — Topology a
  continuous morph of the Comb and Peak stages from parallel at one end to series at the other, with no
  switched positions anywhere in its travel; at its minimum the Peak stage reads the chain's scooped
  input, the same input the Comb stage reads, and at its maximum it reads the Comb stage's output
  instead, with the Comb/Peak blend and every output trim and limiter staying in force at every
  position of both controls
- **THEN** the bank's Crispy and the global Crunchy keep slots 14 and 15

#### Scenario: The Drive bank holds fourteen parameters, complete
- **WHEN** the Drive bank is enumerated
- **THEN** it holds fourteen named parameters at slot indices 0 through 13, not nine
- **THEN** slot 0 is Wet/Dry (short name `Wet`), the page's master, and slot 1 is Gain
  (short name `Gain`), the polynomial waveshaper's own input gain
- **THEN** slots 2-8 carry the remaining seven of the bank's original nine parameters, in their
  established order
- **THEN** slot 9 is Anti-Alias Brightness (short name `ABrt`), the oversampling anti-alias filter's own
  cutoff
- **THEN** slot 10 is Feedback (short name `Fb`), the amount of the folder's own output
  returned to the folder's own input, one oversampled sample later
- **THEN** slot 11 is Fold (short name `Fold`), the pre-fold scale ahead of the sine-fold stage,
  independent of the Drive knob's own gain
- **THEN** slot 12 is Tone (short name `Tone`), a post-chain one-pole lowpass applied after every other
  Drive stage
- **THEN** slot 13 is Symmetry (short name `Sym`), an offset injected at the folder's own input in phase
  units, anchored so that silence-in still produces silence-out
- NOTE (pending `frogg3rs-stereo-field-and-density` delivery): slots 10 and 13 are updated ahead of that change's
  own archival, because Link and Bias no longer exist in the tree and a promoted scenario naming them
  would record a bank the code does not have. That change's spec-delta group restates this requirement's full
  scenario set at delivery, including the bound and centred default its Symmetry law carries.

#### Scenario: The Delay bank holds fourteen parameters, complete
- **WHEN** the Delay bank is enumerated
- **THEN** it holds fourteen named parameters at slot indices 0 through 13, not nine
- **THEN** slot 0 is Wet/dry (short name `Wet`), named exactly as the Reverb bank names its own,
  and slot 1 is Send, the page's feed into its wet path
- **THEN** slots 2-8 carry the remaining seven of the bank's original nine parameters, in their
  established order
- **THEN** slot 9 is Feedback Drive (short name `FbDr`), a pre-gain applied to the input of the feedback
  loop's own in-loop saturator, never to that saturator's output
- **THEN** slot 10 is Feedback Tone (short name `FbTn`), a one-pole lowpass damping the feedback tap
  ahead of the same in-loop saturator
- **THEN** slot 11 is Mod Rate (short name `MdRt`), the delay's own modulation LFO rate
- **THEN** slot 12 is Width Balance (short name `WBal`), the ratio between the Width knob's time-offset
  spread and its cross-feed blend, independent of the Width knob's own value
- **THEN** the cross-feed weight this balance produces stays within 0 to 1 inclusive at every knob position,
  so the left/right feedback pair stays a convex combination of the two delay-line reads
- **THEN** the time-offset spread this balance produces never lengthens a read tap beyond the delay
  buffer's own capacity
- **THEN** slot 13 is Crush (short name `Crsh`), a bitcrush stage applied to the feedback tap's repeats

#### Scenario: The Reverb bank holds fourteen parameters, complete
- **WHEN** the Reverb bank is enumerated
- **THEN** it holds fourteen named parameters at slot indices 0 through 13, not nine
- **THEN** slot 0 is Wet/dry (short name `Wet`) and slot 1 is Send, the page's feed into its tank
- **THEN** slots 2-8 carry the bank's remaining original parameters, with Mod depth and Mod rate
  collapsed into one Mod control so that Send's slot is found without cutting a capability
- **THEN** slot 9 is Hold (short name `Hold`), displaced there by Send taking slot 1 and the modulation pair collapsing to one control
- **THEN** slot 10 is Tank Drive (short name `TkDv`), a pre-gain applied to the input of the tank feedback
  path's own in-loop saturator, never to that saturator's output
- **THEN** slot 11 is Grit (short name `Grit`), the tank feedback path routed through a bit-scramble
  stage ahead of that same in-loop saturator
- **THEN** slot 12 is Tilt (short name `Tilt`), a bipolar post-tank tone shave applied before the
  existing wet limiter
- **THEN** slot 13 is Tuned (short name `Tund`), the tank's own delay-line lengths driven directly by
  this parameter's resolved value, with no pitch tracker

### Requirement: Fixed global control slots
Bank slots SHALL be indexed from zero (`0..15`). In every bank, the local **Crispy** control SHALL occupy slot index **14** and the global **Crunchy** control SHALL occupy slot index **15**. These positions SHALL be identical across all banks and SHALL NOT change when the active bank changes.

#### Scenario: Global controls never move
- **WHEN** the operator switches from one bank to another
- **THEN** Crispy remains at slot index 14
- **THEN** Crunchy remains at slot index 15

#### Scenario: Crunchy is one global control
- **WHEN** Crunchy is adjusted from any bank
- **THEN** the same single global value changes
- **THEN** no per-bank copy of Crunchy exists

### Requirement: Waveform Shape controls are ordinary bank slots
The three VCO waveform **Shape** controls SHALL be registered as ordinary parameters in the Audio bank, not as a separate global axis outside the grid.

#### Scenario: Shape appears on the grid
- **WHEN** the Audio bank is displayed
- **THEN** the three Shape controls occupy ordinary encoder slots
- **THEN** they are addressable, modulatable, and randomizable like any other parameter

### Requirement: Each bank carries its own color
Every bank SHALL have a distinct bank color. That color SHALL be realized by giving every one of that bank's parameters that color; declaring a color on the bank alone, without propagating it to the bank's parameters, SHALL NOT render anything. Because the global Crunchy control is a single shared parameter appearing in all six banks, it SHALL carry one fixed color rather than taking on each bank's color.

#### Scenario: Banks are visually distinguishable
- **WHEN** the operator switches banks
- **THEN** the encoder grid renders in that bank's distinct color
- **THEN** that color is present because it has been applied to every parameter in the bank, not merely declared on the bank

#### Scenario: Crunchy keeps one fixed color across banks
- **WHEN** the operator switches from one bank to another
- **THEN** the global Crunchy control's color does not change to match the newly active bank
- **THEN** Crunchy renders in its own fixed color in every bank

### Requirement: Global Crunchy is one shared parameter, not six copies
The global Crunchy control SHALL be a single shared parameter occupying slot index 15 in all six banks. It SHALL NOT be six independent per-bank copies.

#### Scenario: Adjusting Crunchy from any bank moves the same value
- **WHEN** Crunchy is adjusted while any bank is active
- **THEN** the same single underlying value changes
- **THEN** switching to a different bank shows that same changed value at slot index 15

#### Scenario: Crunchy is published once
- **WHEN** the app's published parameters are enumerated
- **THEN** Crunchy appears once, not once per bank
- **THEN** no per-bank Crunchy duplicate exists

#### Scenario: Drilling into Crunchy targets the same parameter from any bank
- **WHEN** the operator drills into Crunchy's modulation from any bank
- **THEN** the same single Crunchy parameter is the drill-in target
- **THEN** modulation applied from one bank's Crunchy cell affects the identical parameter reachable from every other bank

### Requirement: Scenes
The app SHALL support Sheaf scene state (scene selection and blend) through the Sheaf parameter model.

#### Scenario: Scene blend applies across banks
- **WHEN** the operator changes scene blend
- **THEN** parameter values interpolate between scene endpoints
- **THEN** the change is reflected on the rendered encoder rings

### Requirement: Defined initial patch
The app SHALL ship a defined initial patch: a small, enumerated set of parameters carries non-neutral starting values, and every other parameter starts at its ordinary default.

#### Scenario: Waveform Shape controls start at fixed points
- **WHEN** the app starts for the first time
- **THEN** the first oscillator's Shape control reads its minimum value
- **THEN** the second oscillator's Shape control reads its midpoint value
- **THEN** the third oscillator's Shape control reads its maximum value

#### Scenario: Cross-oscillator modulation depths are present at minimal depth
- **WHEN** the app starts for the first time
- **THEN** a defined set of cross-oscillator modulation depth assignments is already present
- **THEN** each of those assignments sits at the smallest non-zero depth available

#### Scenario: No other parameter departs from its ordinary default
- **WHEN** the app starts for the first time
- **THEN** every parameter outside the enumerated initial-patch set reads its ordinary default value

### Requirement: Bank-slate growth is safe for existing saved patches by construction
A patch saved before a bank's occupied parameter slots grow SHALL continue to load every parameter it
named at its own previously-saved value, whether that growth added new parameters or reassigned existing
parameters to different slot indices within the same bank. Parameter identity for the purpose of saving
and loading SHALL be the parameter's own name, never its bank-slot position, so that slot reassignment
cannot cause one parameter's stored value to be silently applied to a different parameter.

#### Scenario: Reordering an occupied bank's existing slots does not swap values
- **WHEN** a bank's existing named parameters are reassigned to different slot indices within that bank
- **THEN** a patch saved before the reassignment still applies each parameter's stored value to that same
  parameter, not to whatever parameter now occupies its old slot index

#### Scenario: A newly added parameter loads at its ordinary default from an older patch
- **WHEN** a patch saved before a bank gained a new parameter is loaded
- **THEN** the new parameter is not present in that patch's saved data
- **THEN** the new parameter reads its own ordinary default value, exactly as any other parameter absent
  from a loaded patch already does

#### Scenario: Modulation depth assignments follow their own source, not the target's slot
- **WHEN** a bank's target parameter is reassigned to a different slot index
- **THEN** any modulation depth already assigned to that parameter from a given source keeps that same
  source's assignment
- **THEN** this holds because modulation depth is stored per modulation-source index, not per target
  slot index

### Requirement: A newly exposed hardcoded value defaults to the value it replaces
A new parameter SHALL default to the value that was hardcoded before it existed, whenever that parameter's whole purpose is to expose an existing hardcoded literal, so that exposing the literal does not change how the instrument sounds at its own defaults. Where the value being replaced was derived from another parameter at runtime rather than fixed, the new parameter SHALL default to whatever that derivation produces at the other parameter's own default.

#### Scenario: An unlocked literal's default reproduces today's sound
- **WHEN** a parameter is added whose purpose is to expose a value that is hardcoded today
- **THEN** its default value maps to that same hardcoded value
- **THEN** the instrument at its defaults sounds exactly as it did before the parameter existed

#### Scenario: A value derived at runtime defaults to what that derivation produces
- **WHEN** a new parameter replaces a value that was previously computed from another parameter, so no fixed
  default reproduces the old behaviour across that other parameter's whole range
- **THEN** the new parameter defaults to the value that derivation produces at the other parameter's default
- **THEN** the tracking itself is not reproduced, which is the point of decoupling them

### Requirement: Envelope ramps complete in bounded time at every Curve setting
Every envelope stage SHALL complete within a small fixed multiple of its knob-mapped duration at every Curve setting including the maximum, at every supported sample rate. The Curve control SHALL shape a ramp's trajectory, never its reachability: no Curve value may reduce a ramp's per-sample progress below a fixed fraction of its linear step. At Curve's zero default the ramp SHALL be bit-identical to the linear ramp.

#### Scenario: Maximum Curve still completes
- **WHEN** any stage runs at the maximum Curve setting with any knob time, at any supported sample rate
- **THEN** it completes within the fixed multiple of the knob-mapped duration
- **THEN** the observed worst-case multiple is reported by the test that guards this, not assumed

#### Scenario: A pending release is never stranded
- **WHEN** a release is pending while Grace is active and the voice is in any stage, at any Curve setting including the maximum
- **THEN** the voice reaches Release within a bounded time: the bounded completion of its remaining Attack/Decay plus the Grace minimum-hold
- **THEN** Grace's minimum-hold guarantee is preserved unchanged — a short gate still completes Attack and Decay before Release begins (audit-corrected 2026-08-17: the first draft forced Release at Grace expiry from any stage, which would have clipped legitimate notes mid-attack during play, contradicting the approved Grace requirement; transport Stop's immediate release is specified in `froggers-transport-and-reset-controls`, not here)

### Requirement: A randomize draw lands the drawn value
A randomize operation on a parameter's value SHALL result in a commanded value equal to the drawn uniform value, regardless of any live modulation on the parameter at the instant of the draw. Repeated randomize operations SHALL NOT drift the commanded value toward either clamp: the landed values' distribution follows the draw, not the modulation.

#### Scenario: Randomize under audio-rate modulation stays uniform
- **WHEN** a parameter carries full-depth audio-rate modulation and is randomized many times
- **THEN** the commanded values' empirical distribution matches the draw distribution within tolerance
- **THEN** the fraction of draws landing exactly on a clamp boundary stays consistent with the draw, not with accumulation

### Requirement: A wet/dry control cannot remove the instrument

A control that crossfades a dry signal against a processed one SHALL NOT be able
to remove the dry signal entirely. The ceiling SHALL be applied to the mapped
mix value rather than to the knob's range, so the control keeps sweeping its
whole travel and only the value its top end maps to is bounded.

This holds for every such crossfade, not for whichever one was most recently
reported. At the Reverb bank's maximum the dry signal SHALL still make up at
least 30% of that stage's output. The Delay bank's Wet/dry is the same
expression and SHALL carry the same kind of ceiling.

A wet/dry control SHALL NOT attenuate the dry signal in exchange for a processed
signal that cannot exist. Its authority to remove dry signal SHALL scale with
how much signal reaches the processed path: where nothing feeds that path the
control SHALL have no effect, and it SHALL earn its full travel as the path
comes to hold signal. Authority SHALL follow the processed path's measured
level rather than the amount being fed into it, so that a path made loud by
feedback rather than by its feed still grants the control its travel.

Where the feed is switched fully off, the measurement's TARGET SHALL drop at
once rather than tracking the tail the path still holds. The measurement itself
SHALL settle at its ordinary release rate, so the audible result is a fade of
about 100ms rather than a cut. The discontinuity belongs in the target, not in
the output.

#### Scenario: Full wet still passes dry signal
- **WHEN** the Reverb Wet/dry control is at its maximum
- **THEN** the dry signal's contribution to this stage's output is at least 30%

#### Scenario: The control keeps its full travel
- **WHEN** the Reverb Wet/dry control is swept from minimum to maximum
- **THEN** the resulting mix rises across the whole sweep, reaching its ceiling
  only at the top

#### Scenario: The wettest setting is still audible
- **WHEN** a wet/dry control is at its maximum and its processed path is fed
- **THEN** the output still carries dry signal

#### Scenario: An unfed processed path makes the control inert
- **WHEN** the Delay bank's Send is at zero and Wet/dry is swept to maximum
- **THEN** the output is the dry signal and does not fall silent

#### Scenario: A loud echo earns the control its travel
- **WHEN** the Delay bank's Send is low, Feedback is high, and the echo is loud
- **AND** Wet/dry is at maximum
- **THEN** the control removes dry signal in proportion to that echo, not to Send

#### Scenario: Switching the feed off fades rather than tracking the tail
- **WHEN** Send is turned to zero while echoes are still sounding
- **THEN** the control's authority falls at its release rate rather than
  following the echoes' own decay
- **AND** the output settles to the dry signal

#### Scenario: The patch the instrument ships with stays audible
- **WHEN** the default patch is played with Wet/dry at maximum
- **THEN** the instrument is audible

### Requirement: The pitch range excludes the inaudible end

VCO pitch SHALL map its knob exponentially across a named range whose
ceiling stays in pitched, audible territory — high enough to clear the top
of the piano with headroom, low enough that no part of the knob's travel is
spent above what a listener can hear as a pitch. The floor is unchanged and
launch sits on it. The range has one named definition read by the pitch
mapping, so the ceiling cannot drift back by way of a repeated literal. The
filter-frequency ceilings deliberately keep the full audible span: a filter
opened entirely out of the way is intended behaviour, and their ranges are
not this requirement's.

#### Scenario: The top of the knob is a pitch, not a whistle

- **WHEN** any VCO pitch knob sits at the top of its travel
- **THEN** the oscillator's fundamental is a high but audibly pitched note,
  above the top of the piano yet far below the audibility limit

#### Scenario: The launch chord is preserved

- **WHEN** the instrument launches with the default patch
- **THEN** the three oscillators sound the same fundamentals as before the
  ceiling moved, because the stored defaults are recomputed for the new
  range, and the checks that pin those fundamentals in the audio pass
  unchanged

#### Scenario: One definition of the pitch range

- **WHEN** the pitch range is changed in a future edit
- **THEN** the mapping and every check that pins it move together, because
  all read the same named constants

### Requirement: The damping range excludes the inaudible end

The Reverb bank's Damping control SHALL map geometrically onto the damping
filter's coefficient, with a floor high enough that its darkest setting still
passes audible content.

Turning the control UP SHALL darken the tail: the control's top end maps to the
smallest coefficient, and a smaller coefficient is a lower cutoff.

The floor exists because randomization draws each parameter uniformly across its
travel. Under a geometric mapping, half of all draws land below the range's
geometric mean, so a floor an order of magnitude below audibility makes half of
every randomized reverb a tail with nothing left in its top. The floor SHALL be
chosen so that the geometric mean of the range is a cutoff that still reads as a
reverb tail rather than as mud.

#### Scenario: The darkest setting still passes audible content

- **WHEN** the Damping control is at its maximum
- **THEN** the damping filter's cutoff is above 100 Hz at a 48 kHz sample rate

#### Scenario: Up is darker

- **WHEN** the Damping control is raised
- **THEN** the damping filter's coefficient falls, and its cutoff with it

#### Scenario: The mapping is geometric across its whole travel

- **WHEN** the Damping control is at its midpoint
- **THEN** the resulting coefficient is the geometric mean of the coefficients
  at the control's two ends

### Requirement: Tone controls share one range, and it excludes the inaudible end

The Drive bank's Tone and the Delay bank's Feedback tone SHALL map geometrically
onto the one-pole coefficient of the filter each closes, with a floor high
enough that the darkest setting is still a tone rather than a mute.

Both SHALL resolve their coefficient through ONE shared mapping rather than each
computing the range. They are the same control in two positions — a post-stage
low-pass whose knob top is exact bypass — so two expressions of the range would
be two things to keep in agreement by hand. The Reverb bank's damping filter
SHALL NOT share it: its range is narrower and its knob inverted, because it
darkens a tail rather than shaping a signal and never fully opens.

Turning the control DOWN SHALL darken the driven signal: the control's bottom
end maps to the smallest coefficient, and a smaller coefficient is a lower
cutoff. At the control's top the coefficient SHALL be exactly 1, which makes the
stage an exact identity, so an untouched Tone control removes nothing.

The floor exists for the same reason the Reverb bank's damping floor does:
randomization draws each parameter uniformly across its travel, and under a
geometric mapping half of all draws land below the range's geometric mean. A
floor an order of magnitude below anything musical therefore spends most of the
control's travel, and most randomized patches, behind a filter that removes the
signal rather than shaping it. The floor SHALL be chosen so that the geometric
mean of the range is a cutoff a driven signal can still be heard through.

#### Scenario: The darkest setting is still a tone

- **WHEN** either tone control is at its minimum
- **THEN** the resulting cutoff is above 500 Hz at a 48 kHz sample rate

#### Scenario: An untouched control removes nothing

- **WHEN** either tone control is at its default, fully open
- **THEN** the coefficient is exactly 1 and the stage passes its input unchanged

#### Scenario: The mapping is geometric across its whole travel

- **WHEN** either tone control is at its midpoint
- **THEN** the resulting coefficient is the geometric mean of the coefficients
  at that control's two ends

#### Scenario: Down is darker

- **WHEN** either tone control is lowered
- **THEN** the coefficient falls, and its cutoff with it

#### Scenario: The two controls agree by construction

- **WHEN** the Drive Tone and the Delay Feedback tone are set to the same knob
  position, anywhere across the travel
- **THEN** they resolve to the same coefficient, because they read the same
  mapping rather than each computing the range

### Requirement: Comb knob travel is spent on audible change

The comb feedback and Comb/Peak blend knobs SHALL spend their travel on
audible change rather than crowding it into one end. Comb feedback maps each
half of its bipolar travel so the feedback gap falls geometrically — equal
knob steps multiply the loop's ring time by equal ratios — with the center
still exactly zero and the rails still exactly the loop's maximum magnitude.
The Comb/Peak blend crossfades with equal power across a floored range:
the knob traverses 0.05 to 0.95 of the crossfade, so the comb is clearly
present by mid-travel and neither branch is ever fully absent — each
extreme holds the other branch near −22 dB. Launch knob positions are
unchanged, and launch, Reset All, and New agree with each other through
the one shared mapping; the launch output carries the comb at the blend
floor, which is accepted. Comb feedback SHALL default to its center — the
zero-feedback point of its bipolar travel — so the floored comb bed is a
single short echo of the dry signal at startup, not a ring.

#### Scenario: Feedback travel is log-linear in ring time

- **WHEN** the comb feedback knob moves outward from center in equal steps
  on either half, with the loop fed then silenced at each step
- **THEN** the measured ring times form equal ratios step to step, within
  tolerance
- **THEN** the center still produces zero feedback and each rail still
  produces the loop's maximum magnitude, numerically identical to before

#### Scenario: The comb is present by mid-blend

- **WHEN** the Comb/Peak blend sits at the middle of its travel
- **THEN** the comb branch contributes at equal power with the peak branch,
  not at half amplitude
- **THEN** at either extreme the selected branch dominates while the other
  branch stays present at the blend floor, near −22 dB, never fully absent

#### Scenario: Launch, Reset All, and New stay in agreement

- **WHEN** the instrument launches with the default patch, and the state is
  also reached through Reset All and through New
- **THEN** all three present the same knob values and the same output,
  including the floored comb bed — a single short echo, feedback at its
  centered zero default — because all three read the one shared mapping

### Requirement: The signal is not folded to mono before it reaches the device

The delay and reverb stages SHALL carry their stereo pairs to the output rather
than summing them mid-chain. Folding to a single channel SHALL happen at the
output, and only where the device itself is mono.

Both stages already compute a stereo pair internally. A stage that computes a
pair and sums it on the next line spends the work and discards the result, and
it renders every control downstream of the sum unable to affect the output.

A control named for a stereo property SHALL be able to change the output. Where
a Width control's effect cancels exactly in a sum, the sum is the defect, not
the control.

#### Scenario: A stereo device receives a stereo image
- **WHEN** the host offers two or more output channels and a Width control is
  away from its centre
- **THEN** the two channels differ

#### Scenario: The Reverb Width control changes the output
- **WHEN** the Reverb bank's Width is swept
- **THEN** the output changes

#### Scenario: The Delay Stereo width control changes the output
- **WHEN** the Delay bank's Stereo width is swept
- **THEN** the output changes

### Requirement: A Drive page control's travel spends itself where the control is heard

WHEN a Drive page control is mapped from its knob to the coefficient it drives, THE application SHALL choose the mapping so that comparable knob movements produce comparable audible changes across the control's travel, measured against the band the instrument actually produces rather than against the coefficient's own arithmetic range, and SHALL place the control's floor where it begins to act rather than below it: no more than a hundredth of a control's travel may be indistinguishable from its floor, judged on magnitude spectra so that inaudible phase rotation is not counted as an effect. A control whose stated job is inaudible across most of its travel SHALL be re-mapped, re-defaulted, or re-named to say what it does. Two controls on this page are held to this: the Blend stage's Phase allpass, whose audible effect exists only through how the rotated wet signal sums with the dry path, and the anti-alias control, which SHALL reject aliasing measurably across its travel rather than carry a name its filter cannot deliver.

#### Scenario: Phase moves the blend across its whole travel

- **WHEN** a 220 Hz tone is driven with Drive at 0.5 and Blend at 0.25, and Phase is swept from 0 to 1
- **THEN** the output level changes progressively across the sweep rather than only in its last tenth
- Check: `app/FroggersDspParityTests.cpp`, `drive_phase_sweep_moves_output_meaningfully_across_each_quarter_of_travel`, the Phase-travel case.

#### Scenario: The anti-alias control rejects aliasing

- **WHEN** a tone whose harmonics fold is driven hard, and the anti-alias control is swept from end to end
- **THEN** the loudest inharmonic partial, measured against the fundamental, falls monotonically by at least 15 dB across the sweep
- **AND** the in-band level moves by no more than 3 dB between the ends, so the sweep trades grit for cleanliness rather than trading tone for tone
- **AND** the test tone SHALL NOT divide the sample rate, and SHALL sit inside the range the chosen oversampling factor serves: at a tone dividing the sample rate every fold-image lands on a bin a genuine harmonic occupies, so no measurement can separate them, and above roughly 2 kHz the harmonics that matter have passed the 4x domain's own Nyquist where no decimation filter reaches them
- Check: `app/FroggersDspParityTests.cpp`, `drive_anti_alias_crossfade_falls_monotonically_and_the_old_one_pole_barely_moved_it`, the alias-sweep case, at 1487 Hz, read through a Hann-windowed bin measurement — measured 23.3 dB of travel with the in-band level moving 2.46 dB.

#### Scenario: A quantized control spends no travel on steps that do nothing

- **WHEN** the Bit depth control is swept from its floor
- **THEN** the first hundredth of its travel already scrambles a number of bits an operator can hear, rather than the first fifth resting on counts that are inaudible
- Check: `app/FroggersDspParityTests.cpp`, `drive_bit_depth_first_audible_knob_value_falls_from_0_19_to_a_hundredth`, the knob-threshold case.

#### Scenario: The knob's default reproduces what shipped before it

- **WHEN** the anti-alias control sits at its default
- **THEN** the path is bit-identical to the 2x one-pole oversampler that shipped before this control existed
- Check: `app/FroggersDspParityTests.cpp`, `frog_block_default_knob_values_reproduce_original_output_exactly`, which drives the whole chain at its registered defaults and compares against the pre-existing path. The oversampler parity pin alone does not prove this: it pins the 2x unit's own behaviour, not that the knob's default selects it.

#### Scenario: A control that is inert at a default is documented as such

- **WHEN** Symmetry is read with the folder disengaged
- **THEN** Symmetry's effect on the output is more than 10 dB smaller than with the folder engaged, not bit-identical — the floor that keeps Fuzz from silencing the folder's leg entirely also keeps a small residual of Symmetry's own effect
- **AND** the manual names which control has to be moved for Symmetry to act
- Check: `app/FroggersDspParityTests.cpp`, `drive_symmetry_is_inert_when_the_folder_is_not_engaged`, which measures Symmetry's own audible effect through `ProcessDriveBank` with the folder fully engaged (Fuzz 0.0) against the same sweep with the folder floored to its minimum blend weight (Fuzz 1.0) — measured about 3 dB engaged against about -19 dB disengaged, a real but far smaller effect there, matching the manual's own statement of which control (Fuzz) has to move for Symmetry to act.
- NOTE (pending `frogg3rs-stereo-field-and-density` delivery): Link and the old Waveshaper offset/Bias no longer exist; this scenario is updated ahead of that change's own archival so this gate does not dangle on the deleted tests in the interim. `openspec/changes/frogg3rs-stereo-field-and-density/tasks.md`'s spec-delta group restates this requirement's full scenario set at delivery.

### Requirement: An insert effect page's master returns the dry signal at its floor

WHEN an insert effect page carries a wet/dry master, THAT control SHALL crossfade the page's output against its input using a POWER-COMPLEMENTARY law — the two legs' gains SHALL be `cos` and `sin` of a common angle, so that their squares sum to one — and SHALL return the input exactly, sample for sample, at its floor, and it SHALL be named as an effect pedal names it and placed first among that page's parameters. A linear crossfade SHALL NOT be used: it holds level only where its two legs are fully correlated, and no page's wet leg is. Both endpoints SHALL be exact by construction rather than by trusting the trigonometric functions, which do not land on zero at a right angle in single precision. Where a page's wet path is fed through a Send, the master SHALL NOT duck the dry signal in proportion to a wet path that Send has left empty, and that behaviour SHALL be one shared mechanism rather than one implementation per page.

This requirement does NOT ask a page's wet path to be transparent at rest. No page's wet path is: a reverb's wet path is a tank, a delay's is echoes, and a distortion's is a waveshaper. Measured on the Reverb bank with every control at its registered default and the master turned fully up, the output is 8.46 dB below its input and differs from it by -4.07 dB relative to that input. Reverb and Delay read as transparent at rest because their Sends default closed and their wet paths are empty, not because those paths are unity. The property the three pages genuinely share is the one stated above, and each of them meets it by construction of the crossfade.

A page's gain stage SHALL be named Gain and SHALL govern the stage it drives rather than the page as a whole, because a page's bit and rate manglers act at any level and are not gated by gain. Every insert effect page whose wet path is a send-and-return SHALL open with the same two controls in the same order — the master wet/dry first, that page's Send second. A page's wet control SHALL leave a floor of dry signal in place where fully replacing the source destroys what the page is processing, as it does for a reverb and a delay, and SHALL reach fully wet where replacing the source is a sound the page exists to make, as it is for a distortion; wherever that floor applies, both the code and the manual SHALL record why it is there.

#### Scenario: The master returns the dry signal exactly at its floor

- **WHEN** an insert effect page's wet/dry control sits at its floor
- **THEN** that page's output is its input, sample for sample
- Check: `app/FroggersDspParityTests.cpp`'s `drive_blend_phase_authored_zero_blend_is_exact_passthrough`, the dry-at-floor pin for the Drive page, which passes today at -240 dB; `app/dsp/Reverb.hpp`'s mix and `app/dsp/Delay.hpp`'s are the same crossfade expression.

#### Scenario: The master does not lose level partway through its travel

- **WHEN** an insert effect page's wet/dry master is swept from its floor to its top
- **THEN** the page's output level does not dip below its dry level by more than a small margin anywhere on that travel, rather than notching partway and recovering
- **AND** this holds across notes and across the page's own gain settings, not at one tested frequency
- Check: `app/FroggersDspParityTests.cpp`, `drive_blend_travel_holds_level_within_1_3_db_across_gain_and_frequency`, the Drive page's blend-travel case, which measures a worst dip of -1.21989 dB where the linear law it replaced measured -4.10 dB.

#### Scenario: A tone control's level change is documented rather than compensated

- **WHEN** a control that shapes a path's spectrum — a reverb's Damping, a delay's Feedback tone — is swept from end to end
- **THEN** the level it removes is whatever its filter removes, and the manual states both what the control darkens and that it quiets the path it shapes
- **AND** no makeup gain is applied to hide that change, because how much level a lowpass removes depends on where the signal's energy sits: Reverb's Damping costs 9.98 dB across its travel on broadband noise and 0.52 dB on a 110 Hz tone, so a static term set for either one is wrong for the other
- Check: `app/FroggersDspParityTests.cpp`'s `reverb_damping_darkens_and_quiets_the_tank_while_room_size_does_neither`, which pins both the monotone broadband attenuation and the monotone spectral tilt, and carries Room size over the same travel as its positive control.

#### Scenario: The gain stage makes distortion and the manglers do not need it

- **WHEN** Gain sits at its floor and the XOR, Bit depth or either rate reducer is raised
- **THEN** that stage is audible, because it acts on the signal at any gain
- **AND** with every stage at its floor, raising Gain alone is what introduces harmonic distortion
- Check: `app/FroggersDspParityTests.cpp`'s `drive_gain_makes_distortion_and_the_manglers_act_at_any_gain`, which measures XOR, Bit depth and both rate reducers against a bit-identical all-floor reference at Gain 0, and Gain's own THD rise with every mangler at its floor.

#### Scenario: A fed page's master does nothing until the page is fed

- **WHEN** the Delay bank's Send sits at zero and its wet/dry control is raised to its top
- **THEN** the output is the dry signal, neither ducked nor replaced
- **AND** the Reverb bank behaves identically, through the same shared mechanism rather than a second copy of it
- Check: `app/FroggersDspParityTests.cpp`'s `delay_wet_dry_leaves_dry_untouched_while_send_is_closed_and_moves_it_once_fed` pins the Delay bank's dry-until-fed behavior against the renamed control, and `reverb_wet_authority_tracks_whether_send_is_open_and_the_tank_is_fed` pins the identical `wetAuthority` mechanism for Reverb's Send.

#### Scenario: The master's position and name say what it does

- **WHEN** the Drive, Delay and Reverb banks are enumerated
- **THEN** each opens with its wet/dry master at slot 0, and each page that has a Send carries it at slot 1
- **AND** the Drive page's master is named Wet/Dry and its gain stage is named Gain
- Check: `app/check_docs_match_parameter_table.py`, which cross-checks the table against the manual and the quick dictionary.

#### Scenario: The default patch does not arrive wet

- **WHEN** the instrument is loaded with its own default patch
- **THEN** the Drive page's Wet/Dry sits at its registered default rather than at a value a slot-indexed overlay left behind
- Check: `app/FroggersModulationTests.cpp`'s `default_patch_gain_is_20_percent` and `default_patch_wet_dry_reads_its_own_registered_default`; the overlay is `ApplyDriveBankOverlay` (`app/FroggersModulation.hpp`), which addresses its target by slot number.

#### Scenario: A control's range reaches the effect it is named for

- **WHEN** a reverb's pre-delay control is swept from end to end
- **THEN** the tail's arrival moves by tens of milliseconds, far enough for a listener to hear the source and its tail as separate events
- **AND** no two adjacent knob positions produce identical output
- Check: `app/FroggersDspParityTests.cpp`'s `reverb_predelay_sweeps_tens_of_milliseconds_with_no_dead_adjacent_steps` and `reverb_predelay_floor_and_ceiling_match_derived_millisecond_range`. Written first against the 2.08 ms total sweep the samples-for-milliseconds range produced, where they failed.

