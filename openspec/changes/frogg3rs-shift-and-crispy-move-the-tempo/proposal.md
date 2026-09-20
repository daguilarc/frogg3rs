# Proposal — `frogg3rs-shift-and-crispy-move-the-tempo`

This change gives the MIDI Fighter Twister preset a second shifted encoder
turn and tells the library which app action is the tempo. It depends on the
Sheaf change `shifted-turn-moves-the-tempo`, which adds the shifted job
itself, on the `shifted-encoder-turns` branch the submodule already tracks.
It is based on frogg3rs `main` at `424d7c8`, which pins Sheaf `40aea92d`.

## Why

Hold Shift and turn Crunchy and the scene blend moves. The player wants the
same reach for the tempo, and the knob next to Crunchy is the one free to
carry it: Crispy is position 14, Crunchy is position 15, and they sit side by
side on the bottom row of the Twister.

A shifted turn returns before the turn's slot or parameter page is resolved,
so the fact that Crispy is a per-page control and Crunchy is not makes no
difference to what Shift does with either knob. Verified at the dispatch:

```
$ sed -n '729,738p' External/Sheaf/projects/synth/src/MidiController.cpp
            if (shift_ != nullptr && shift_->held && mapping->shiftedJob == EncoderShiftedJob::SceneBlend) {
                if (config_.mode == EncoderMode::Absolute) {
                    Push(MessageIn::SetSceneBlend(NextTimestamp(),
                                                  AbsoluteEncoderByteToNormalized(midi.GetValue())));
                } else if (const std::optional<float> delta = DecodeDelta(midi.GetValue())) {
                    Push(MessageIn::SceneBlendIncDec(NextTimestamp(), *delta));
                }
                return;
            }
            if (config_.mode == EncoderMode::Absolute) {
```

## The route a MIDI message takes to the tempo today

Nothing in the app moves the tempo except an absolute control. The APC40's
master fader is mapped to the BPM app action, which carries the 30 to 300
range, and the route is:

controller CC → `AnalogMidiInProcessor::Process` pushes
`MessageIn::AppAction(appActionIx, normalized)` → audio thread
`MessageInBus::Apply` forwards it to the app-action output bus → message
thread `Engine::MessageThreadTick` rescales the normalized value into the
catalog action's range and dispatches `ui::Action("froggers.bpm", "<bpm>")` →
`FroggersUiSurface::DispatchAction`'s `kBpm` branch calls
`FroggersAppCore::RequestTempoBpm` → the atomic is drained on the audio
thread in `ProcessFrame`, which calls `MasterClock::SetTempoBpm`.

Two facts about that route:

- It carries a position, never a step. There is no relative tempo path, which
  is what the Sheaf change adds.
- The app is not the tempo's owner. `MasterClock::activeBpm_` is, and the
  app's own on-screen reading is a once-per-block mirror of it, published
  from `masterClock->TempoBpm()` in `FroggersAppCore`'s end-of-block section.
  A tempo moved inside the library therefore shows on the BPM slider with no
  app change at all.

Two claims in the brief for this change do not survive tracing, and the work
is written against what is there:

- "Nothing outside the master clock calls `MasterClock::SetTempoBpm`" is not
  so. Outside tests, `app/FroggersAppCore.hpp` calls it on the audio thread
  when it drains a pending request, and Sheaf's own miniapp calls it too.
  `MasterClock.cpp:963` is the definition, not a call.

```
$ git grep -n "SetTempoBpm(" HEAD -- app | grep -v -i "tests\|openspec\|docs/"
HEAD:app/FroggersAppCore.hpp:883:            context_->masterClock->SetTempoBpm(tempoRequest);

$ git -C External/Sheaf grep -n "SetTempoBpm(" HEAD -- projects/synth | grep -v -i "tests\|openspec\|docs/"
HEAD:projects/synth/apps/miniapp/MiniAppCore.hpp:301:                        context_->masterClock->SetTempoBpm(effectiveTempoBpm);
HEAD:projects/synth/include/synth/MasterClock.hpp:318:    bool SetTempoBpm(double bpm) noexcept;
HEAD:projects/synth/src/MasterClock.cpp:963:bool MasterClock::SetTempoBpm(double bpm) noexcept {
```

- The 30 to 300 range is not the master clock's. `SetTempoBpm` rejects only a
  non-finite or non-positive tempo, and refuses while slaved. The range lives
  in this app, as `kFroggersBpmMin` and `kFroggersBpmMax` in
  `app/FroggersUiSurface.hpp`, reaching the library once, as the BPM catalog
  action's analog range. That is why the Sheaf change takes its clamp from the
  catalog rather than inventing one.

## What Changes

- **Crispy's turn carries Tempo.** The Twister preset sets the shifted job on
  the turn at Crispy's slot, found by `kFroggersCrispySlot` the way Crunchy's
  is found by `kFroggersCrunchySlot`, never by writing a CC number into the
  preset. While Shift is held, turning Crispy moves the tempo and Crispy does
  not move; released, the same turn moves Crispy again.
- **The catalog names its tempo action.** `FroggersMidiCatalog()` sets
  `catalog.tempoAction = FroggersActions::kBpm`, beside
  `catalog.encoderPressAction`. This is what hands the library the 30 to 300
  range it clamps to, from the one place the app already states it. Without
  it the shifted turn would be inert, so it is not optional here.

  This is one line of app code beyond the preset line. It is here rather than
  in Sheaf because the range is this app's, and it reads a constant this app
  already defines; putting a tempo range in the library instead would invent a
  policy no library user asked for and would state 30 to 300 twice.
- **The Controllers page shows it.** No page code changes. Crispy's turn stops
  folding into the Twister's encoder block for the same reason Crunchy's does,
  so the encoder section reads as one 14-turn block plus Crispy's row showing
  Shift = BPM and Crunchy's row showing Shift = Scene Blend.
- **The manual and its diagrams.** MANUAL.md's list of what can be mapped
  says BPM is an analog control; it is now also a shifted turn. Its MIDI
  Fighter Twister subsection names Crunchy's shifted job and says no other
  encoder has one; it names both now, and its Restore paragraph says what
  Restore installs. `assets/manual/twister-controls.json` and the two preset
  PNGs are regenerated from the code by `make manual-diagrams`, and the shift
  diagram's alt text names what the picture shows.
- **Hygiene, fixed here.** The header comment in `app/FroggersMidiCatalog.hpp`
  describing the Twister preset ends "No other Twister encoder has a shifted
  job", which this change makes false. `app/GenerateTwisterManualLabels.cpp`
  carries a comment naming the shifted-job catalog's entries as "(none)" or
  "Scene Blend", which the Sheaf change makes incomplete.

## What holds whatever one detent is worth

How far one detent moves the tempo is set by `kTempoBpmPerEncoderDetent` in
the Sheaf change. Nothing in this change depends on its value:

- The tempo is clamped to 30 at the bottom and 300 at the top. Turning down
  past 30 leaves it at 30 and turning up past 300 leaves it at 300; neither
  end wraps, and the next detent the other way moves it again.
- Direction follows the turn.
- While the transport is slaved to external MIDI clock, a shifted turn moves
  nothing, which is what the BPM slider already does — it renders as a
  read-only line in that state.
- The BPM slider follows a shifted turn without any app change, because it
  reads the master clock.

## Capabilities

### Modified Capabilities
- `froggers-midi-controller-mappings`: the Twister preset requirement is
  modified and one requirement is added.

## Impact

- `app/FroggersMidiCatalog.hpp` (the preset's shifted turn, the catalog's
  tempo action, and the header comment describing the preset).
- `app/GenerateTwisterManualLabels.cpp` (one comment).
- `MANUAL.md`, `assets/manual/twister-controls.json`,
  `assets/manual/twister-preset.png`,
  `assets/manual/twister-preset-shift.png`.
- Tests: `app/FroggersMidiCatalogTests.cpp`,
  `app/FroggersControllersPageTests.cpp`.
- The submodule pin at `External/Sheaf`, moved to the commit that carries
  `shifted-turn-moves-the-tempo`.
- `openspec/changes/frogg3rs-shift-and-crispy-move-the-tempo/specs/froggers-midi-controller-mappings/spec.md`:
  this change's own delta, carrying a `RESTATES-EXCEPT` block over the two
  clauses the modified Twister requirement narrows from one shifted turn to
  two (`whose job is Scene blend` and `still read as one block`), and the
  added requirement naming the catalog's tempo action.

## Delivery

Delivered by pushing to `main` on `daguilarc/frogg3rs`, never as a pull
request, after the Sheaf work it depends on is pushed to the fork and the pin
moves with it. A patch saved before this change carries the old Twister row;
loading it and pressing Restore installs both shifted turns, as the manual
says. A patch saved after it carries Crispy's shifted job like any other
mapping.
