# Proposal — `frogg3rs-transport-and-shift-ux`

This change is written against frogg3rs `origin/main` at `b06ba16`, which pins
`External/Sheaf` at `e8894727`, the tip of the open pull request stack: the
"Record the delivery" commit of `fold-controller-wizard-into-add-row`
(jvictor0/Sheaf#19). The frogg3rs branch `frogg3rs-transport-and-shift-ux` is
based on `b06ba16`, and the Sheaf branch `shifted-encoder-turns` on
`e8894727`. frogg3rs `main` has since moved to `37c1b9c`, which pins
`External/Sheaf` at `f6266560`; task 7.3, the first open task, rebases both
branches onto those and updates the base hashes these artifacts name. Code is
named by symbol, not line.

## Why

The operator plays Froggers from the screen and from a MIDI Fighter Twister.
Four things behave in ways a player does not expect.

1. **Play gives no sign that the transport is running.** Freeze and Record
   swap their plate and glyph colours while latched or armed. On the base,
   Play draws the same plate whether the transport runs or not, so the screen
   does not say whether the instrument is playing.
2. **Releasing Freeze stops the music.** On the base, Play, then Freeze, then
   Freeze leaves the transport stopped and silent. The player pressed Freeze
   to hold a moment and pressed it again to let go of it; they expect to be
   back where they were, playing. The base's kFreeze release branch only
   clears the latch, and the promoted requirement says "resuming audio then
   requires Play".
3. **The Twister cannot reach the scene blend.** The blend is the one
   continuous scene control, and the base's Twister preset has no knob for
   it. The operator wants Shift held with the Crunchy knob to move the scene
   blend, and Crunchy back as soon as Shift is released.
4. **The Filter page's output is not limited when the comb carries it.** With
   audio-rate modulation on the comb's parameters and Crispy and Crunchy
   raised, the operator reports that the Filter page's output "gets crazy".
   On the base, the page's only limiter sits on the peak branch, ahead of the
   Comb/Peak blend, so the comb branch reaches the pages after Filter
   unlimited. The section "The Filter page's limiter moves to the page's
   output" below holds the mechanism, the operator's ruling and the
   measurements.

The four are one change because each makes the instrument do what a player
expects of it.

## What Changes

### frogg3rs

- **Play shows a held state while the transport runs.**
  `BuildPlayDrawCommands` takes a `running` flag and swaps plate and glyph
  colours when it is set, the same exchange `BuildFreezeDrawCommands` and
  `BuildRecordDrawCommands` make. The Play Draw node is a lambda that reads
  `FroggersAppCore::TransportRunning` on every rebuild, as the Freeze node
  reads `FroggersAppCore::FreezeLatched`. `TransportRunning()` is the audio
  thread's once-per-block publish of the master clock's own state
  (`FroggersAppCore::ProcessBlock` stores it), so the plate follows every
  route that starts or stops the transport: the buttons, their MIDI mappings,
  a MIDI Start or Stop from any controller (`RealtimeMidiInProcessor`,
  appended to every profile), the DAW transport in the plugin, and the
  restart `FroggersAppCore::PrepareToPlay` pushes after an audio-device
  change. Stop has no held state; a stopped transport is Play's idle state.
- **Releasing Freeze returns the transport to where it was.** When Freeze
  engages, `FroggersUiSurface::HandleAction`'s kFreeze branch records
  `TransportRunning()` in NEW `freezeEngagedWhileTransportRunning_`. When a
  second Freeze press releases the latch, the branch calls
  NEW `FroggersUiSurface::StartTransport` if the record is true (latch off,
  Start pushed, desired-running recorded, transport notice cleared), and
  otherwise clears the latch alone, which lets `latchReleasedWhileStopped`
  tear the drone down. The kPlay branch calls the same `StartTransport()`, so
  starting the transport has one definition. Play and Stop while frozen are
  unchanged: Play unfreezes and plays, Stop unfreezes and stops.
- **The Twister preset gives Crunchy's knob a shifted job.**
  `TwisterDeviceDefault` finds the encoder turn whose position is
  `kFroggersCrunchySlot` and sets its shifted job to Scene blend (the Sheaf
  addition below). The side buttons are unchanged; Shift stays on the bottom
  right side button.
- **The Twister manual diagrams show Crunchy's shifted job.** The base draws
  `MANUAL.md`'s two Twister diagrams from `assets/manual/twister-controls.json`,
  which `app/GenerateTwisterManualLabels.cpp` writes from the preset, and
  `check-twister-manual-diagrams-drift` fails whenever a fresh run of that
  program differs from the committed file. The preset's new `shiftedJob`
  field changes that output, and `app/render_twister_manual_diagrams.mjs`
  refuses an encoder field it does not know. So the label program writes a
  NEW `shiftedTurn` label on every encoder row, read from
  `EncoderShiftedJobCatalog()`; the renderer accepts `shiftedJob` and, in the
  Shift-held diagram, draws a shifted turn's label in place of its ordinary
  one; `assets/manual/twister-controls.json` and
  `assets/manual/twister-preset-shift.png` are regenerated with
  `make -C app manual-diagrams`; and the Shift-held diagram's alt text in
  `MANUAL.md` says Crunchy's knob reads Scene Blend.

### Sheaf (the `shifted-encoder-turns` addition)

On the base, Sheaf's Shift is a per-profile `ShiftState` that only the
system-button processor reads. `EncoderMidiInProcessor` takes a
`HoldDrillState` and no `ShiftState`, so a held Shift changes nothing about a
knob. Scene blend reaches the library only as the absolute
`MessageIn::SetSceneBlend`, from an analog CC. The Twister's encoders are
relative (Enc 3FH/41H). Nothing in the library turns a relative knob into a
blend move, with or without Shift.

The Twister cannot supply this from the device side either. Its manual
(`https://s3.amazonaws.com/MF_Support_Docs/Midi+Fighter+Twister+User+Guide.pdf`,
pages 12 and 14) offers two shift features: an encoder switch set to Shift
Encoder Hold or Toggle makes that encoder's own turns send a second value, and
a side button set to Shift Page A or B changes what the encoder switches send.
A side button cannot shift encoder turns, and the preset needs its Shift side
button to stay CC Hold for the five shifted button jobs.

The addition is general: a held Shift gives a knob turn a second job, as it
already gives a button a second job, and the job belongs to the turn's own
mapping, editable per row on the Controllers page. That is the rule the
operator set for button Shift.

- **A scene-blend increment message.** NEW `MessageIn::Type::SceneBlendIncDec`,
  appended after `Shift` so no ordinal moves, with a
  `MessageIn::SceneBlendIncDec(timestamp, delta)` factory. `MessageInBus::Apply`
  hands it to NEW `ParameterManager::IncDecSceneBlend`, which adds the delta
  to the blend and clamps to 0..1 beside `ParameterManager::SetSceneBlend`. It
  is applied on the audio thread, so fast turns never lose increments to a
  stale read. The kind has a case at every site of the `SetSceneBlend`
  family listed under Evidence except the test catalog table in
  `viewmodel_tests.cpp`, which lists row-dropdown kinds, and the increment is
  not one.
- **A shifted job on the turn mapping.** `EncoderMidiMapping` carries
  `shiftedJob`, a NEW `EncoderShiftedJob` (`None`, `SceneBlend`). It
  serializes as `"shiftedJob": "sceneBlend"` only when set. A missing key
  reads as None and an unknown value fails the load. A push mapping carrying
  one is an invalid profile.
- **The encoder processor reads Shift.** `EncoderMidiInProcessor` takes the
  profile's `ShiftState`, and the profile builder passes it as it passes
  `holdDrill`. `EncoderMidiInProcessor::Process` handles a turn in this order:
  Hold Drill as on the base; then, if Shift is held and the mapping's shifted
  job is Scene blend, a relative turn pushes `SceneBlendIncDec` with the
  decoded delta and an Absolute turn pushes `SetSceneBlend` with the
  normalized value; otherwise the turn is handled as on the base. The
  `ShiftState` comment names both processors that read it.
- **Controllers page.** An Individual encoder-turn row has the Shift field
  (`Field::ShiftAction`, header "Shift"), offering none and Scene Blend and
  committing through the row's existing flush path. The page's
  `Field::ShiftAction` combo branch in `ControllersPageUI.hpp` fills a turn
  row's options from NEW `EncoderShiftedJobCatalog()` and its selection from
  NEW `MidiConfigViewModel::EncoderTurnShiftedJobIndex`; a system row still
  uses `ShiftCatalog()` and `ShiftChoiceIndex()`. `BuildSectionRows` and
  `MidiConfigViewModel::GroupColumnFields` read a turn row's fields from one
  NEW `EncoderTurnEditableFields` function, as they read a system row's from
  `SystemRowEditableFields`. `ReconstructEncoderBlocks` never folds a turn
  with a shifted job into a block, so on a Twister row the encoder section
  reads as one 15-turn block plus Crunchy's own row showing Shift = Scene
  Blend. A block expands to turns with no shifted job. Push rows have no
  Shift field.
- **Sweep finding, fixed here.** On the base, `SystemRowEditableFields` adds
  the Shift field to every system row. The row dropdown offers Shift only
  when the app's catalog lists it (`UISystemMessageCatalog()` has no Shift
  entry), so an app without a catalog, such as braid-4 or the miniapp, shows
  a Shift column that nothing can ever hold. NEW `MessageCatalogOffersShift`
  answers whether the catalog offers Shift, and the Shift field on system
  rows and on turn rows appears only when it does. Froggers offers it, so
  Froggers' page is unchanged apart from the new turn field.

### Found during execution: Restore refuses every app preset

Rendering the Delivery Gate states through the page's own actions showed that
pressing Restore on a Twister row saved before this change is refused with
"Refused: restoring requires a resolved preset". On the base, the page's
lifecycle actions (Rename, Delete, Restore) all run through
`ControllersPageSurface::CommitLifecycleAction`, which builds a throwaway
`MidiConfigViewModel` and calls `Rebuild` on it without the layouts the
surface's own view model receives from `ControllersPageCallbacks::layouts` in
the constructor. With no layouts set, `MidiConfigViewModel::Layouts` falls
back to `MakeControllerWizardRegistry(MidiAppCatalog{})`, the library's own
registry, which holds none of an app's presets. So
`MidiConfigViewModel::RestoreController` cannot resolve any app preset's
wizard id, and Restore is refused on every Froggers preset row. The promoted
`froggers-sheaf-runtime-app` scenario "Restore appears only when there is
something to restore" says pressing Restore reinstalls the row's own preset;
its Check drives the library's own Twister preset, which the fallback
registry holds, so it passes on the base. The change depends on the fix,
because Restore is how a Twister row saved before this version gains
Shift + Crunchy.

The fix is general: NEW `ControllersPageSurface::ConfigureViewModel`
configures a view model from `ControllersPageCallbacks` (message catalog,
analog action catalog, layouts), and both the constructor and
`CommitLifecycleAction` call it, so the two cannot drift again. On a
successful Restore, `HandleRestoreController` also calls NEW
`MidiConfigViewModel::NoteControllerConfigReplaced` on the surface's own view
model, which drops that controller's cached section rows: an open section
keeps its rows until the controller is removed or the section is collapsed,
so without it the page keeps showing the pre-Restore encoder rows and the
next encoder edit writes them back over the restored mappings.

Twister rows made before Froggers had its own presets carry the library's
Twister id. `MakeControllerWizardRegistry` leaves the library's Twister out
of a registry whose catalog has its own Twister default, as Froggers' does, so
those rows show no Restore; the manual tells a player with such a row to
delete it and add the MIDI Fighter Twister preset again.

## Data flow

**Play plate.** Play press → `HandleAction` kPlay → `StartTransport()` →
`MessageIn::Start` on the UI bus → next block drains it → master clock
Running → `FroggersAppCore::ProcessBlock` stores `transportRunningDisplay_` →
next surface rebuild → Play lambda reads `TransportRunning()` true → held
colours. Stop or Freeze → Stop drained → clock Stopped → publish false → idle
colours.

**Freeze release.** Freeze press, latch off → surface records
`TransportRunning()` in `freezeEngagedWhileTransportRunning_` →
`LatchThenTransport(true, Stop, false)`. Second Freeze press, latch on → if
the record is true, `StartTransport()` runs (latch off before the Start push,
the happens-before order `LatchThenTransport` documents) and the transport
runs; if false, the latch clears and `ProcessBlock`'s
`latchReleasedWhileStopped` edge runs the teardown. Every Freeze route
reaches this branch: the screen plate, a MIDI-mapped Freeze (`Engine`'s
message-thread tick → `app_.PortableSurface().DispatchAction`), and the
plugin's Freeze host parameter (`FroggersPluginProcessor::PumpHostParameterBridge`
→ `DispatchAction(kFreeze)`). The plugin's DAW transport edges dispatch kPlay
and kStop the same way, from `FroggersPluginProcessor::timerCallback`.

**Shift + Crunchy.** Shift side button down (ch3 CC13, 127) → encoder
processor finds no turn or push → passes through → system-button processor →
`shift_->held = true`. Encoder 16 turned clockwise (ch0 CC for position 15,
value 65) → encoder processor finds the turn → Hold Drill not held → Shift
held and shifted job Scene blend → `DecodeDelta` gives +1 turn step →
`SceneBlendIncDec` pushed → audio thread `MessageInBus::Apply` →
`IncDecSceneBlend` → blend rises, Crunchy untouched → UI state mirrors the
blend and the on-screen slider moves. Shift up → held false → the next turn
pushes `ParamIncDec` for Crunchy.

## Decisions a reviewer should check

- **Freeze remembers the transport's actual state.** It records
  `TransportRunning()` rather than `desiredTransportRunning_`. The desired flag
  records only surface presses. It misses a MIDI Start or Stop from a
  controller and the test rig's `StartAt`, and it stays true after an external
  Stop. The published flag trails a press by at most one audio block, so a
  Freeze arriving in the same block as the Play before it records "stopped".
- **Resuming pushes Start, the same message Play pushes.** Unfreezing is
  pressing Play, as the operator described it.
- **Hold Drill wins over Shift on a knob.** A turn with both held drills, which
  keeps smi-16's "Shift and Hold Drill do not interfere" scenario true.
- **A Twister row saved before this change keeps its old mappings.** A preset
  is copied into a row once, and runtime config and patches carry the row, so
  an existing row shows Restore and turns Crunchy under Shift only after the
  player presses it. The manual says so. Restore is one press; a load-time
  migration would overwrite edits a player made to the row, so there is none.
- **The Twister's LED ring on encoder 16 is unchanged.** This change does not
  touch the encoder output config, so while Shift is held the ring shows
  Crunchy rather than the blend.

## The Filter page's limiter moves to the page's output

### Story

The operator plays audio-rate modulation into the comb filter's parameters
with Crispy and Crunchy raised, and reports: "the filter page output needs a
harder limiter clamp on output, shit gets crazy with audio rate crispy/crunchy
modulation on comb filter parameters." The player needs the Filter page's
output held under the level the pages after it expect, whatever the comb is
doing.

### Mechanism on the base

`FilterFxChain::Process` (`app/dsp/FilterFx.hpp`) builds the page's output
from two branches and returns their blend with no limiter after it:

- the peak branch: `peak.Process`, the `1/height` trim, then `peakLimiter`,
  an `OutputLimiter` tuned by `kPeakLimiterThreshold`, `kPeakLimiterCeiling`
  (`kStageCeiling`), `kPeakLimiterAttackSeconds` and
  `kPeakLimiterReleaseSeconds`;
- the comb branch: `pureDelay`, `Comb::Process` with `PadeSaturator` inside
  its feedback loop, then the `1/(1+|fb|)` trim, and no limiter;
- `mixed = peakPath * legA + combPath * legB`, returned as is.

`RouteFilterBank` hands that value to the Delay page. The comb branch reaches
Delay, Reverb and the master limiter without passing any limiter on the Filter
page. The comment on `peakLimiter` justifies this by calling the comb branch
"provably bounded", and the comb-trim comment derives the bound
`|comb| <= A + |fb|` from the saturator's ±1 clamp. That bound holds only at
Comb drive 1 and above. Comb drive divides the saturator's output by the same
factor that scales its input, so below drive 1 the fed-back term exceeds the
clamp: at drive 0.25 (the bottom of the knob) it reached 2.92 (measured
below).

Crispy, Crunchy and modulation reach the comb through the parameter model: the
fuego scramble is applied to post-modulation values in `ApplyFuegoSeam`,
before `RouteFilterBank` reads them, every sample.

### Ruling

The operator ruled: move `peakLimiter` from the peak branch to the Filter
page's output, after the Comb/Peak blend, at its current settings. There is no
comb-specific limiter and no strengthening. The change is unconditional, and
the modulated comb is only the reason for it.

### What changes

- `FilterFxChain::Process` passes the trimmed peak branch to the blend
  unlimited and returns the limiter applied to the blend. The limiter keeps
  its four tuning constants and their values.
- Every comment and doc the move makes false is corrected, including the
  two measured below as false on the base.
- OPEN, for the next session's preflight: whether the member and its names
  follow its new job. The proposed names are NEW `outputLimiter` for the
  member (the name `DriveBlendPhase` already gives the Drive page's output
  limiter), NEW `kFilterOutputLimiterThreshold`,
  NEW `kFilterOutputLimiterCeiling`, NEW `kFilterOutputLimiterAttackSeconds`
  and NEW `kFilterOutputLimiterReleaseSeconds` for the constants, and NEW
  `TestFilterOutputLimiter` for the test accessor. See "Open decisions"
  below.

### Data flow

Drive page output → `RouteFilterBank` → scoop blend → comb branch (pure delay,
comb, comb trim) and peak branch (peak, peak trim) → Comb/Peak blend → the
Filter page's limiter → Delay page. On the base, the limiter sits between the
peak trim and the blend, on the peak branch alone.

### Measurements

**Harness.** The measurements use a harness outside the tree, whose sources
and build scripts are in
`openspec/changes/frogg3rs-transport-and-shift-ux/evidence/limiter/` (called
`$E` below). `setup.sh` builds three source trees from frogg3rs `4ab820f`, the
tree the figures were measured on, with `External/Sheaf` at `751e82e0`:
`app/` unchanged, `cur/` (`app/` plus taps, `taps.patch`), and `moved/`
(`cur/` with the limiter moved, `move.patch`). The Makefile builds every
binary named below into `$WORK/bin`:

```
$ make -C External/Sheaf/projects/synth build
$ sh $E/setup.sh "$WORK"
$ cd "$WORK" && nice make -j2 -f $E/Makefile all
```

The move is the only difference between `cur/` and `moved/`:

```
$ cat $E/move.patch
--- a/dsp/FilterFx.hpp
+++ b/dsp/FilterFx.hpp
@@ -870,7 +870,7 @@
         // combPath below -- so it catches exactly the residual the
         // trim leaves behind, not instead of the trim (the trim stays;
         // this is additive).
-        const float peakPath = peakLimiter.Process(peakTrimmed);
+        const float peakPath = peakTrimmed;  // SCRATCH MOVE: the limiter now runs on the blended output below.
         // Floored equal-power blend, single-sourced with FrogBlock's own
         // Fold/Fuzz blend (dsp/Drive.hpp) rather than a second copy of the
         // same floor/span/angle law -- see dsp::FlooredEqualPowerBlend's own
@@ -880,7 +880,7 @@
         tapPeakLeg = peakPath * blendGains.legA;
         tapCombLeg = combPath * blendGains.legB;
         tapMixed = mixed;
-        return mixed;
+        return peakLimiter.Process(mixed);  // SCRATCH MOVE.
     }
 };
```

- **Taps.** The Filter page's output (what `RouteFilterBank` returns), the
  blended signal ahead of any limiter after the blend, each leg after its
  blend gain, and the master output the rig captures.
- **Rig.** SynthRig at 48 kHz with 256-sample blocks, transport started. The
  first 0.5 s is discarded and 2.0 s is measured.
- **Limiter bypass.** The Filter page's limiter is configured with threshold
  1e6 and ceiling 2e6, which leaves its gain at exactly 1.

**The taps change nothing, and the move changes only the limiter's
placement.** Master-output hashes, combined from the three runs'
`hash master` fields:

```
$ bin/harness_app hashcheck; bin/harness_cur hashcheck; bin/harness_moved hashcheck
default                          limiter-on        app 983b9243ffc537d7  cur 983b9243ffc537d7
default                          limiter-bypassed  app a566dc9592b9c647  cur a566dc9592b9c647  moved a566dc9592b9c647
modcomb crispy1 blend0.5         limiter-on        app 1aed9973756b5a5f  cur 1aed9973756b5a5f
modcomb crispy1 blend0.5         limiter-bypassed  app 915006bf3f7ef1a7  cur 915006bf3f7ef1a7  moved 915006bf3f7ef1a7
comb-resonant fb1 cdrv0 blend1.0 limiter-on        app a699b1ab797c90c3  cur a699b1ab797c90c3
comb-resonant fb1 cdrv0 blend1.0 limiter-bypassed  app 73cb692ad4ba396f  cur 73cb692ad4ba396f  moved 73cb692ad4ba396f
```

With the limiter bypassed, the Filter-output hashes also match between the
current and moved builds (`c01702f2cc5d97e6`, `699134d4f6dee3b7`,
`143d488537fc8793`).

**One-run comparison, current placement against moved.** Rows and columns
selected from the output of `bin/harness_cur grid` and
`bin/harness_moved grid`. Levels are in dBFS, and "clamp" is the share of
master samples at the ±1 clamp. The patches are:

- **default:** the registered defaults.
- **modcomb:** VCO1 audio at full positive depth on Filter slots 3–7 (Comb
  offset, delay, feedback, LP and drive), Filter Crispy 1.0, Crunchy 0.
- **comb-resonant:** Comb delay on VCO1's pitch, Comb feedback 1.0, Comb drive
  0.0, Comb/Peak 1.0.

```
patch                      placement  FILTER OUT pk / rms   pre-limit blend pk  MASTER pk / rms / clamp  limiter env min
default                    current    -1.79 / -10.15        -1.79               -0.27 / -8.39 / 0.000%   0.9577
default                    moved      -2.20 / -10.29        -1.46               -0.44 / -8.44 / 0.000%   0.9198
default blend0.5           current     0.53 / -10.06         0.53               -0.01 / -9.32 / 0.000%   0.9577
default blend0.5           moved      -1.96 / -11.26         0.68               -0.33 / -9.52 / 0.000%   0.7387
modcomb crispy1 blend0.5   current     0.82 / -11.97         0.82               -0.01 / -11.44 / 0.001%  0.9577
modcomb crispy1 blend0.5   moved      -1.93 / -13.33         1.00               -0.32 / -11.61 / 0.000%  0.7243
comb-resonant cdrv0        current     1.95 / -8.95          1.95               -0.00 / -8.77 / 0.120%   0.9577
comb-resonant cdrv0        moved      -1.94 / -10.74         1.94               -0.33 / -9.03 / 0.000%   0.6490
```

- **Limiter liveness.** Bypassing the limiter moves every row with the limiter
  after the blend. On comb-resonant the current limiter is working (envelope
  minimum 0.9577), but on the peak branch while the comb carries the level, so
  bypassing it moves the Filter output by 0.01 dB.
- **What the move changes.** On the comb-resonant patch the Filter output
  falls from above full scale to −1.94 dB, and master clamping stops.
- **What it costs on the default patch.** 0.14 dB of rms at the Filter output
  and 0.05 dB at the master, measured this way.

The full rows, with the per-leg peaks and the ranges the comb's parameters
swept, are what the two `grid` runs print.

**The comb bound fails below Comb drive 1.** `bin/combbound` runs `dsp::Comb`
from `app/dsp` unchanged, with a 100-sample delay, the low-pass fully open,
feedback 0.95, and a sine at the comb's pitch at amplitude A. Rows at A = 1.00
selected, and the `max|tail|` column cut:

```
$ bin/combbound
 drive     A |  max|comb|     A+|fb| | max|fedback| | bound holds
  0.25  1.00 |     3.9222     1.9500 |       2.9222 | NO
  0.50  1.00 |     2.7044     1.9500 |       1.7044 | NO
  1.00  1.00 |     1.9308     1.9500 |       0.9308 | yes
  4.00  1.00 |     1.2375     1.9500 |       0.2375 | yes
```

The rows at A = 0.25 show the same split (0.25 → 2.0831 against 1.2000,
1.00 → 0.9806 against 1.2000).

**Drive runs before Filter.** A Drive change alters the Filter page's output.
A Delay change leaves it bit-identical and alters only the master (the
`hash` fields of each row):

```
$ bin/harness_cur order
default           hash filter 83faa2b4452f705c  master 983b9243ffc537d7
drive gain1 wet1  hash filter 0fe496643f706331  master da09d95834bb32b7
delay wet1 send1  hash filter 83faa2b4452f705c  master 8b43d20ad4f1252b
```

**The new placement test goes red on the current code and green on the
moved code.** `placementtest.cpp` is a prototype of the test task 8.3 adds:

```
$ bin/pt_cur; bin/pt_moved
unlimited peak 1.0846 (threshold 0.70) | shipped peak 1.0846 | ... shipped limiter envelope min 1.0000 | samples where shipped != limiter(blend) 95380 of 96000 -> RED
unlimited peak 1.0846 (threshold 0.70) | shipped peak 0.7979 | ... shipped limiter envelope min 0.7357 | samples where shipped != limiter(blend) 0 of 96000 -> GREEN
```

**`gate_period_tracks_tempo_change`'s audio proxy.** `routing-print.patch`
makes the test print its edge counts. `bin/rb_cur` and `bin/rb_moved` are the
same test with the Filter limiter bypassed (`routing-bypass.patch`):

```
$ for b in routing_cur rb_cur routing_moved rb_moved; do bin/$b | grep 'SCRATCH gate edges'; done
SCRATCH gate edges base=63 doubled=110 ratio=1.746
SCRATCH gate edges base=69 doubled=104 ratio=1.507
SCRATCH gate edges base=70 doubled=104 ratio=1.486
SCRATCH gate edges base=69 doubled=104 ratio=1.507
```

**Suites against the moved copy.** Each binary is built from the test source
copied beside the moved headers, and from the unmoved headers for comparison
(`bin/dsp_cur`, `bin/dsp_moved`, `bin/routing_*`, `bin/visualizer_*`,
`bin/headless_*`, `bin/parammodel_*`):

| Binary | current | moved |
|---|---|---|
| DSP parity | 203/203 | 200/203 |
| audio routing | 56/56 | 55/56 |
| visualizer | 4/4 | 4/4 |
| headless | 8/8 | 8/8 |
| parameter model | 12/12 | 12/12 |

The four failures are in the table below.

### Tests the move changes

| Test | Why it goes red | After |
|---|---|---|
| `filter_fx_chain_parallel_matches_manual_comb_peak_scoop_blend` | its replica applies the limiter to the peak branch (0.724068 against 0.727463) | the replica applies it after the blend |
| `filter_fx_chain_blend_extremes_hold_other_branch_at_floor_gain` | same (0.704129 against 0.704215) | same |
| `filter_bank_peak_branch_trim_versus_limiter_bound_on_pinned_comb` | replica against `Process`, worst sample delta 0.0730906 | the replica follows the new placement and matches `Process` exactly again |
| `gate_period_tracks_tempo_change` | its audio proxy's edge ratio is 1.746 with the limiter on the peak branch, 1.507 with no Filter limiter, and 1.486 with it after the blend; the lower bound 1.6 was fitted to the first | OPEN, see "Open decisions" |

These tests pass after the move, but their comments, labels or printed
figures describe a peak-branch limiter, and some figures change:

- `peak_branch_output_respects_computed_bound_under_audio_rate_height_modulation_with_limiter`
- `peak_ceiling_candidate_limiter_measurement`: post-limiter worst 0.885603 before the move, 0.809305 after, for candidate 3.
- `peak_ceiling_scoop_modulation_limiter_measurement`: post-limiter worst 3.36585e+29 before the move, 0.877405 after.
- `topology_morph_peak_branch_headroom_across_full_range`
- `filter_bank_peak_gain_travel_measurement_at_and_away_from_resonance`: 1 kHz falls 6.04 dB before the move and 5.27 dB after, 5 kHz falls 6.52 and 5.75, and broadband falls 7.12 and 6.72.

### Open decisions

These are OPEN. The next session's preflight settles each one, and the task
that depends on it (8.1, 8.2, 8.5) is rewritten with the ruling before it is
dispatched.

- **The rename (task 8.2).** Moving the limiter makes the names
  `peakLimiter`, `kPeakLimiter*` and `TestFilterPeakLimiter` false. The draft
  proposes renaming them to the NEW names under "What changes"; the
  alternative is to keep the names and have the comments say where the
  limiter sits.
- **`gate_period_tracks_tempo_change`'s lower bound (task 8.5).** Measured
  with the test's own patch, the proxy ratio falls below 1.6 as soon as no
  limiter rides the peak branch alone: 1.507 with no Filter limiter at all,
  and 1.486 with the limiter after the blend. The test's header says the
  bound exists to catch a collapse, with ratios "well under 1.3". One
  candidate is to set the lower bound to 1.3 and restate the measured counts
  (base=70, doubled=104). The other is to change the test's patch until the
  ratio clears 1.6 again, which fits the test to the limiter rather than to
  the gate it is named for. Neither is decided.
- **The default patch's level (task 8.1).** The default patch loses 0.14 dB
  of rms at the Filter output and 0.05 dB at the master (see the grid). The
  ruling moves the limiter at its current settings; whether the change
  compensates the default patch's level is open.

## Sheaf addition and delivery

The Sheaf half follows how every Sheaf addition in this project has shipped.

- **Additive, apart from two fixes.** A new message kind appended after the
  last enumerator, a new optional field whose absence reads as the base's
  behaviour, a new processor input defaulting to null, and a Controllers-page
  field that appears only when the app's catalog offers Shift. No existing
  message, mapping or saved document changes meaning. A saved instrument or
  patch without `shiftedJob` loads exactly as before, and an app that never
  sets a shifted job sees no change. The first fix is the sweep finding: apps
  that cannot map Shift lose a Shift column that could never fire. The second
  is the Restore fix (found during execution, above), which changes behaviour
  for every app with its own presets: Restore resolves the app's presets
  instead of refusing them. It changes no saved document. The test for each
  Sheaf edit is whether an app author who has never heard of frogg3rs would
  want it. Shift on knobs passes that test, and so does a Restore that
  resolves.
- **On the fork, as the next PR in the stack.** The work is done in this
  change's worktree's `External/Sheaf` on a branch named after the Sheaf
  openspec change, `shifted-encoder-turns`, based on the stack tip `e8894727`
  (`fold-controller-wizard-into-add-row`, jvictor0/Sheaf#19). It carries its
  own openspec change, `External/Sheaf/openspec/changes/shifted-encoder-turns/`,
  which holds the Sheaf half of this proposal and the Sheaf spec deltas. It
  modifies smi-16 and sru-15 as they stand in `shift-and-file-export` (#14),
  modifies the promoted sru-10, and adds smi-17 and sru-66
  (`fold-controller-wizard-into-add-row` added sru-64 and sru-65). smi-16 and
  that sru-15 exist only in open changes, so `shifted-encoder-turns` stays
  active in Sheaf, like #14, #17, #18 and #19, until upstream merges the
  stack; only the frogg3rs change is archived. It is pushed to the fork
  (`fork` remote, `daguilarc:shifted-encoder-turns`) and opened as the next
  pull request against jvictor0/Sheaf `main`, after #19. Nothing in the stack
  is merged into either Sheaf `main`, and frogg3rs runs on the stack tip. The
  pull request description carries step-by-step testing instructions for
  Shift + knob on a Twister.
- **Order.** The worktree's `External/Sheaf` git directory is private to the
  worktree. Its commits are fetched into the main checkout's `External/Sheaf`
  (`git -C <main>/External/Sheaf fetch <worktree gitdir> shifted-encoder-turns:shifted-encoder-turns`)
  and pushed to the fork from there, before frogg3rs `main` moves its pin, so
  `main` never pins an unfetchable commit. Once the pull request is open, a
  "Record the delivery of shifted-encoder-turns: pushed to the fork as
  jvictor0/Sheaf#N, pin moved on frogg3rs main" commit is made in this
  worktree's `External/Sheaf`, fetched and pushed the same way, and frogg3rs
  pins that tip only after `fork/shifted-encoder-turns` in the main checkout's
  `External/Sheaf` equals it. If the fork's stack tip or frogg3rs
  `origin/main` has moved past the base this change is rebased on by then,
  the branch rebases onto it and its suites and screenshots are redone before
  the push.
- **frogg3rs delivery** is a fast-forward push to `main`. There is no pull
  request on daguilarc/frogg3rs.

## Capabilities

### Modified Capabilities
- `froggers-transport-and-reset-controls`: the Freeze release clause of "Stop
  silences the instrument in bounded time, in every patch" and its "pressed a
  second time" scenario; a new requirement, "The Play plate shows whether the
  transport is running".
- `froggers-midi-controller-mappings`: "The MIDI Fighter Twister preset maps
  five buttons with shifted jobs and one Shift" gains Crunchy's shifted turn.
- `froggers-sheaf-parameter-model`: "A control's travel does not silently
  cost level" names the limiter at the Filter page's output in its Check; a
  new requirement, "The Filter page limits its output after the Comb/Peak
  blend".

### Sheaf capabilities (in the Sheaf change's `specs/`)
- `synth-midi-instrument`: smi-16 modified, smi-17 added.
- `synth-runtime-ui`: sru-10 and sru-15 modified, sru-66 added.

## Impact

- frogg3rs: `app/FroggersUiSurface.hpp` (Play draw and node, Freeze release,
  `StartTransport`), `app/FroggersMidiCatalog.hpp` (Twister turn, header
  comment), `app/FroggersAppCore.hpp` (the `TransportRunning()` comment,
  which on the base says "used only to annotate the BPM control" while
  `ArmRecording` reads it, and Play now does too; the gate comment on the
  Freeze release), `app/GenerateTwisterManualLabels.cpp`,
  `app/render_twister_manual_diagrams.mjs`,
  `assets/manual/twister-controls.json`,
  `assets/manual/twister-preset-shift.png`.
- frogg3rs, the Filter page's limiter: `app/dsp/FilterFx.hpp` (the move,
  and the rename if task 8.2 rules it in), the comments naming the limiter in
  `app/dsp/Limiter.hpp`, `app/FroggersAppCore.hpp`, `app/dsp/Delay.hpp`,
  `app/dsp/Drive.hpp` and `app/dsp/Reverb.hpp`, the tests in
  `app/FroggersDspParityTests.cpp` and `gate_period_tracks_tempo_change` in
  `app/FroggersAudioRoutingTests.cpp`, and the measurement harness in
  `openspec/changes/frogg3rs-transport-and-shift-ux/evidence/limiter/`.
- frogg3rs tests: `app/FroggersSurfaceTests.cpp`,
  `app/FroggersAudioRoutingTests.cpp`, `app/FroggersMidiCatalogTests.cpp`,
  `app/FroggersControllersPageTests.cpp`.
- Comments this change makes false, corrected with the code that makes them
  false: the `BuildFreezeDrawCommands` comment in `app/FroggersUiSurface.hpp`,
  the header above `play_disarms_the_freeze_latch_and_returns_the_voice_gate_to_the_transport`,
  the quoted kFreeze branch in `app/vst/FroggersPluginProcessor.cpp`
  (`PumpHostParameterBridge`), the gate comment in `app/FroggersAppCore.hpp`
  that says a release always tears down, the cleanup comment in
  `app/vst/FroggersVstHostTests.cpp` (false on the base: the code only
  releases resources), `Field::ShiftAction`'s "System row only" comment, the
  `GroupColumnFields` comment that calls the per-group field tables a single
  source of truth, and the comments in the label program and renderer that
  name a per-encoder shifted job as a field they do not know.
- Hygiene sweep: comments in `app/vst/FroggersPluginProcessor.hpp` and
  `app/vst/FroggersPluginProcessor.cpp` that cited planning section numbers,
  a misspelled spec name and a line number, and a comment in
  `app/FroggersAudioRoutingTests.cpp` that gave `pageParameters_` the wrong
  width.
- Sheaf, under `External/Sheaf/projects/synth/`:
  `External/Sheaf/projects/synth/include/synth/ParameterModulation.hpp`,
  `External/Sheaf/projects/synth/include/synth/MidiController.hpp`,
  `External/Sheaf/projects/synth/include/synth/MidiConfigViewModel.hpp`,
  `External/Sheaf/projects/synth/include/synth/MidiConfigBlocks.hpp` (the
  `SystemMessageSortKey` comment that counts the message kinds),
  `External/Sheaf/projects/synth/src/ParameterModulation.cpp`,
  `External/Sheaf/projects/synth/src/MidiController.cpp`,
  `External/Sheaf/projects/synth/src/MidiConfigViewModel.cpp`,
  `External/Sheaf/projects/synth/src/MidiConfigBlocks.cpp`, and
  `External/Sheaf/projects/synth/include/synth/ControllersPageUI.hpp` (its
  Shift combo branch, and the lifecycle actions' view model). Tests in
  `External/Sheaf/projects/synth/tests/`: `instrument_tests.cpp`,
  `parameter_modulation_tests.cpp`, `viewmodel_tests.cpp`, `blocks_tests.cpp`,
  `portable_ui_tests.cpp`, `controllers_page_ui_tests.cpp`. The Sheaf change
  directory `External/Sheaf/openspec/changes/shifted-encoder-turns/`.
- `External/Sheaf`: pin bump to the `shifted-encoder-turns` tip.
- Documentation step (after postflight): `MANUAL.md` (Play's held plate;
  Freeze release; the Shift subsection covers knobs; the Twister subsection
  gains Shift + Crunchy; the Filter page's signal path, Comb drive and Peak
  gain), `QUICK_DICT.md` (Play / Stop and Freeze / Record entries; Peak
  gain), the bundled copies the build embeds.

### Tests this change moves

Every test that presses Freeze was read. Three encode the old release rule.

| Test | On the base | With this change |
|---|---|---|
| FroggersAudioRoutingTests.cpp: releasing_freeze_does_not_restart_the_transport (deleted, no longer in the tree) | engages from running, asserts release leaves it stopped | replaced by `releasing_freeze_resumes_the_transport_it_stopped` |
| FroggersAudioRoutingTests.cpp: freeze_latch_release_while_stopped_silences_within_the_bound (deleted, no longer in the tree) | builds the drone with Play then Freeze, releases, asserts silence | replaced by `freeze_engaged_while_stopped_releases_to_silence_within_the_bound`, which engages Freeze from a stopped transport, confirms the drone is audible, releases, and asserts silence within the bound and a stopped transport |
| `FroggersAudioRoutingTests.cpp: no_freeze_stop_press_sequence_leaves_the_instrument_sounding_after_stop` | sequence 2's comment calls the instrument "already-unlatched-and-silent" before the final Stop | comment corrected; the release resumes play, and the final Stop still has to silence it |

Unaffected, read to confirm: `freeze_action_toggles_the_latch_and_a_second_click_releases_it`,
`freeze_draw_commands_genuinely_invert_plate_and_glyph_colours`,
`freeze_alone_holds_the_ring_above_an_audible_floor_and_stops_the_transport`,
`play_disarms_the_freeze_latch_and_returns_the_voice_gate_to_the_transport`,
`midi_app_action_walk_moves_the_state_the_screen_moves`, and the plugin's
`freeze_via_production_seam_holds_audio_and_reads_stopped_like_t7_3a`. None of
them asserts the transport after a release.

## Failure present on the base

The plugin's `FroggersVstHostTests` binary (a target of
`app/vst/CMakeLists.txt`, built into `app/vst/build/`) fails one case on this
branch, and the
failing path is the base's, not this change's. `$L` is a scratch log file
outside the tree:

```
$ app/vst/build/FroggersVstHostTests > "$L" 2>&1; echo "EXIT=$?"
EXIT=1
$ grep -c '^\[PASS\]' "$L"
45
$ grep '^\[FAIL\]' "$L" | sed "s#$PWD/##"
[FAIL] state_information_save_and_restore_never_write_the_shared_data_root: app/vst/FroggersVstHostTests.cpp:1800 requirement failed: before == after
```

The case restores the plugin's own state five times and asserts that the
data root is byte-for-byte unchanged. A DAW state restore reaches the engine
as a patch: `FroggersPluginProcessor::PumpStatePersistence` pushes
`PatchMessageIn::LoadFromJSON` onto the patch bus; because Froggers' catalog
sets `patchCarriesMappings`, the engine's patch drain hands the document's
`midiInstrument` section to `Engine::StashLoadedPatchInstrument`; and
`Engine::MessageThreadTick` applies that instrument through `EditInstrument`
and then calls `Engine::SaveRuntimeConfiguration`, which writes the data
root's `config.json`. The save after applying a patch's instrument came in
with the base, and this change touches neither `Engine.hpp`, the patch
persistence code, nor any non-comment line of the plugin processor:

```
$ git -C External/Sheaf log --format='%h %s' -S'configuration is saved so the next launch keeps it' -- projects/synth/include/synth/Engine.hpp
fc35a428 Make the add row the one way to set up a controller, open the row it adds, and save every edit as it is made
$ git -C External/Sheaf diff --stat e8894727 HEAD -- projects/synth/include/synth/Engine.hpp projects/synth/src/PatchPersistence.cpp projects/synth/include/synth/PatchPersistence.hpp
$ git diff b06ba16 HEAD -- app/vst/FroggersPluginProcessor.cpp | grep '^[-+]' | grep -v '^[-+][-+]' | grep -v '^[-+]\s*//'
$
```

The failing run left a `config.json` in the case's scratch data root
(`froggers-vst-host-tests/state_no_write_through` under the system temporary
directory). The cause above is traced by reading. This change does not fix
it. The `fold-controller-wizard-into-add-row` session reports that Sheaf
`f6266560`, which frogg3rs `37c1b9c` ("Keep a DAW's plugin state restore from
writing the shared data folder") pins, fixes it; that report is not verified
here. Task 7.4
runs the case after the rebase onto `37c1b9c`; if it still fails, this
failure stands as a finding of this change.

## Evidence

On the base, Shift reaches only the button processor. The encoder processor's
constructor takes a `HoldDrillState` and no `ShiftState`; only the
system-button processor's takes one:

```
$ git -C External/Sheaf grep -n -E "ShiftState\*|HoldDrillState\*" e8894727 -- projects/synth/include projects/synth/src
e8894727:projects/synth/include/synth/MidiController.hpp:265:                           HoldDrillState* holdDrill = nullptr);
e8894727:projects/synth/include/synth/MidiController.hpp:281:    HoldDrillState* holdDrill_ = nullptr;
e8894727:projects/synth/include/synth/MidiController.hpp:390:                                HoldDrillState* holdDrill = nullptr, ShiftState* shift = nullptr);
e8894727:projects/synth/include/synth/MidiController.hpp:401:    HoldDrillState* holdDrill_ = nullptr;
e8894727:projects/synth/include/synth/MidiController.hpp:402:    ShiftState* shift_ = nullptr;
e8894727:projects/synth/src/MidiController.cpp:681:                                               HoldDrillState* holdDrill)
e8894727:projects/synth/src/MidiController.cpp:923:                                                          HoldDrillState* holdDrill, ShiftState* shift)
e8894727:projects/synth/src/MidiController.cpp:3028:    HoldDrillState* const holdDrill = result.holdDrill.get();
e8894727:projects/synth/src/MidiController.cpp:3030:    ShiftState* const shift = result.shift.get();
```

The family `SceneBlendIncDec` visits: every switch case, name mapping and
factory for its sibling `SetSceneBlend` on the base (lines cut at 150
characters):

```
$ git -C External/Sheaf grep -n -E "case MessageIn::Type::SetSceneBlend|\"setSceneBlend\"|Type::SetSceneBlend;|Type::SetSceneBlend," e8894727 -- projects/synth/src projects/synth/include projects/synth/tests/blocks_tests.cpp projects/synth/tests/viewmodel_tests.cpp | cut -c1-150
e8894727:projects/synth/src/MidiConfigBlocks.cpp:92:        case MessageIn::Type::SetSceneBlend:
e8894727:projects/synth/src/MidiConfigViewModel.cpp:43:        case MessageIn::Type::SetSceneBlend:
e8894727:projects/synth/src/MidiConfigViewModel.cpp:86:        case MessageIn::Type::SetSceneBlend:
e8894727:projects/synth/src/MidiConfigViewModel.cpp:181:        case MessageIn::Type::SetSceneBlend:
e8894727:projects/synth/src/MidiConfigViewModel.cpp:735:        case MessageIn::Type::SetSceneBlend:
e8894727:projects/synth/src/MidiController.cpp:222:    case MessageIn::Type::SetSceneBlend:
e8894727:projects/synth/src/MidiController.cpp:223:        return "setSceneBlend";
e8894727:projects/synth/src/MidiController.cpp:279:    } else if (value == "setSceneBlend") {
e8894727:projects/synth/src/MidiController.cpp:280:        type = MessageIn::Type::SetSceneBlend;
e8894727:projects/synth/src/MidiController.cpp:1870:    case MessageIn::Type::SetSceneBlend:
e8894727:projects/synth/src/MidiController.cpp:2428:    case MessageIn::Type::SetSceneBlend:
e8894727:projects/synth/src/MidiController.cpp:2501:    case MessageIn::Type::SetSceneBlend:
e8894727:projects/synth/src/ParameterModulation.cpp:4017:    message.type = Type::SetSceneBlend;
e8894727:projects/synth/src/ParameterModulation.cpp:4212:    case MessageIn::Type::SetSceneBlend:
e8894727:projects/synth/tests/blocks_tests.cpp:139:        case MessageIn::Type::SetSceneBlend:
e8894727:projects/synth/tests/viewmodel_tests.cpp:3943:        {UISystemMessage::SetSceneBlend, synth::MessageIn::Type::SetSceneBlend, false, false,
```

Found 16 lines at 14 sites; the change adds a `SceneBlendIncDec` counterpart
at 13 of them and not at the `viewmodel_tests.cpp` catalog row, since the
increment is no row-dropdown kind:

```
$ git -C External/Sheaf grep -n -E "case MessageIn::Type::SceneBlendIncDec|Type::SceneBlendIncDec;|MessageIn MessageIn::SceneBlendIncDec" HEAD -- projects/synth/src projects/synth/tests/blocks_tests.cpp projects/synth/tests/viewmodel_tests.cpp | cut -c1-150
HEAD:projects/synth/src/MidiConfigBlocks.cpp:94:        case MessageIn::Type::SceneBlendIncDec:
HEAD:projects/synth/src/MidiConfigViewModel.cpp:52:        case MessageIn::Type::SceneBlendIncDec:
HEAD:projects/synth/src/MidiConfigViewModel.cpp:96:        case MessageIn::Type::SceneBlendIncDec:
HEAD:projects/synth/src/MidiConfigViewModel.cpp:197:        case MessageIn::Type::SceneBlendIncDec:
HEAD:projects/synth/src/MidiConfigViewModel.cpp:781:        case MessageIn::Type::SceneBlendIncDec:
HEAD:projects/synth/src/MidiController.cpp:238:    case MessageIn::Type::SceneBlendIncDec:
HEAD:projects/synth/src/MidiController.cpp:298:        type = MessageIn::Type::SceneBlendIncDec;
HEAD:projects/synth/src/MidiController.cpp:1889:    case MessageIn::Type::SceneBlendIncDec:
HEAD:projects/synth/src/MidiController.cpp:2456:    case MessageIn::Type::SceneBlendIncDec:
HEAD:projects/synth/src/MidiController.cpp:2530:    case MessageIn::Type::SceneBlendIncDec:
HEAD:projects/synth/src/ParameterModulation.cpp:4093:MessageIn MessageIn::SceneBlendIncDec(std::uint64_t timestamp, float delta) {
HEAD:projects/synth/src/ParameterModulation.cpp:4096:    message.type = Type::SceneBlendIncDec;
HEAD:projects/synth/src/ParameterModulation.cpp:4229:    case MessageIn::Type::SceneBlendIncDec:
HEAD:projects/synth/tests/blocks_tests.cpp:156:        case MessageIn::Type::SceneBlendIncDec:
```

Every production Play, Stop and Freeze route outside the screen ends in the
surface's `DispatchAction`. On the base, `FroggersPluginProcessor::timerCallback`
dispatches kPlay and kStop on the DAW transport's edges and
`FroggersPluginProcessor::PumpHostParameterBridge` dispatches kFreeze;
`FroggersPluginProcessor::TestStartTransport` and `TestStopTransport` are the
host tests' seams onto the same call:

```
$ git grep -n -E 'PortableSurface\(\)\.DispatchAction\(|void DispatchAction\(' b06ba16 -- app ':!app/*Tests.cpp' ':!app/**/*Test*.cpp'
b06ba16:app/FroggersUiSurface.hpp:988:    void DispatchAction(const synth::ui::Action& action) override {
b06ba16:app/vst/FroggersPluginProcessor.cpp:765:        engine_.Application().PortableSurface().DispatchAction(
b06ba16:app/vst/FroggersPluginProcessor.cpp:768:        engine_.Application().PortableSurface().DispatchAction(
b06ba16:app/vst/FroggersPluginProcessor.cpp:964:    engine_.Application().PortableSurface().DispatchAction(
b06ba16:app/vst/FroggersPluginProcessor.cpp:970:    engine_.Application().PortableSurface().DispatchAction(
b06ba16:app/vst/FroggersPluginProcessor.cpp:1097:    // PortableSurface().DispatchAction(kFreeze), the EXACT seam the
b06ba16:app/vst/FroggersPluginProcessor.cpp:1207:                    engine_.Application().PortableSurface().DispatchAction(
b06ba16:app/vst/FroggersPluginProcessor.hpp:237:    // engine_.Application().PortableSurface().DispatchAction()) the real

$ git grep -n -A1 'PortableSurface().DispatchAction($' b06ba16 -- app/vst/FroggersPluginProcessor.cpp
b06ba16:app/vst/FroggersPluginProcessor.cpp:765:        engine_.Application().PortableSurface().DispatchAction(
b06ba16:app/vst/FroggersPluginProcessor.cpp-766-            synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));
--
b06ba16:app/vst/FroggersPluginProcessor.cpp:768:        engine_.Application().PortableSurface().DispatchAction(
b06ba16:app/vst/FroggersPluginProcessor.cpp-769-            synth::ui::Action::Named(synth_froggers::FroggersActions::kStop));
--
b06ba16:app/vst/FroggersPluginProcessor.cpp:964:    engine_.Application().PortableSurface().DispatchAction(
b06ba16:app/vst/FroggersPluginProcessor.cpp-965-        synth::ui::Action::Named(synth_froggers::FroggersActions::kPlay));
--
b06ba16:app/vst/FroggersPluginProcessor.cpp:970:    engine_.Application().PortableSurface().DispatchAction(
b06ba16:app/vst/FroggersPluginProcessor.cpp-971-        synth::ui::Action::Named(synth_froggers::FroggersActions::kStop));
--
b06ba16:app/vst/FroggersPluginProcessor.cpp:1207:                    engine_.Application().PortableSurface().DispatchAction(
b06ba16:app/vst/FroggersPluginProcessor.cpp-1208-                        synth::ui::Action::Named(synth_froggers::FroggersActions::kFreeze));
```

MIDI app actions arrive through Sheaf's `Engine::MessageThreadTick`, whose
`ParameterMessageOut::Type::AppAction` branch ends in
`app_.PortableSurface().DispatchAction(dispatched)`.

The Twister's turns are one block on the base and Crunchy's turn is CC 15:
`EncoderMidiInConfig::TwisterDefault` returns `RowMajorInputDefault`, which
maps position `p` to channel 0, CC `EncoderPositionToCC(p)`, which returns
`position % 16`, and `ReconstructEncoderBlocks` extends a run while slot and
channel hold and position and CC both advance by one.

No Sheaf app defines a MIDI catalog; the only definition outside the engine's
own accessor is a test fixture, so braid-4, the miniapp and one-second-delay
build their row dropdown from `UISystemMessageCatalog()`, which has no Shift:

```
$ git -C External/Sheaf grep -n -E "MidiAppCatalog MidiCatalog\(|MidiCatalog\(\) (const )?\{|static .*MidiCatalog\(" e8894727
e8894727:projects/synth/include/synth/Engine.hpp:698:    const MidiAppCatalog& MidiCatalog() const { return midiCatalog_; }
e8894727:projects/synth/tests/engine_tests.cpp:3044:    synth::MidiAppCatalog MidiCatalog() const { return catalog; }
```

## Preflight record

Preflight ran on the previous base, frogg3rs `b0c03a9` with Sheaf `f73d4202`,
before the Filter limiter joined this change. The limiter's measurements
were taken on frogg3rs `4ab820f` with Sheaf `751e82e0`, and no preflight has
run over them yet: the next session's preflight covers the limiter and the
open decisions it names.

- Hygiene sweep: three false statements corrected across six comments in
  `app/vst/FroggersPluginProcessor.hpp`/`.cpp` and
  `app/FroggersAudioRoutingTests.cpp`. Preflight later found one more false
  comment the sweep missed (`GroupColumnFields`, task 1.7). Both suites
  matched their baselines.
- Behavioural checks, run before execution: after Play, the clock reads
  running within one block from the screen and two from a MIDI app action,
  and it reads running when the Freeze handler records it; the Freeze plate's
  colours swap on the next rebuild; a turn row with the Shift field is 430 px
  and the widest encoder section 594 px, against 900 px; Twister encoder 16
  decodes 65 as a positive step and 63 as a negative one. On the current
  base the 900 px bound is held by
  `TestControllersRowFitsWithinFroggersNarrowestHost`, which builds a
  shifted turn row; the pixel widths were not re-measured.
- Four independent attackers, a debate over their merged findings, and an
  adjudicator who took no part: ten findings true, two false. The true ones
  are written into this proposal and the tasks.

## Delivery Gate

Before merge to `main`, the operator approves from screenshots of the built
app, taken on the current base. The screenshots cover:

- the transport row with Play idle, with Play held while running, and with
  Freeze latched and Play idle;
- the Controllers page with a Twister row's encoder section open: the 15-turn
  block and Crunchy's own row with Shift = Scene Blend;
- the same row's system-message section, unchanged apart from layout;
- a Custom row's encoder section with a turn row showing its Shift field set
  to none;
- a Twister row created before this change, before and after Restore.
