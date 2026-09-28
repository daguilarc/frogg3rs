# Proposal — `frogg3rs-plugin-input-as-sidechain`

Based on frogg3rs `main` at `33b23e6`, which pins Sheaf `8d08d24c`. This
proposal makes no Sheaf change and moves no submodule pin.

## Why

A Frogg3rs VST3 user in Ableton Live 12 or Bitwig cannot route another
track's audio into the plugin to FM its oscillators through IN: and
External Audio. The plugin's only input bus is declared with
`isActivatedByDefault = false`
(`app/vst/FroggersPluginProcessor.cpp`'s `BusesProperties().withInput(...)`)
but carries no VST3 client extensions, so JUCE's VST3 wrapper reports it to
the host as a **main** input:

```
$ grep -n "getPluginHasMainInput\|Vst::kMain\|Vst::kAux" ~/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp
3229:                    const auto isFirstBus = (index == 0);
3231:                    if (dir == Vst::kInput)
3233:                        if (isFirstBus)
3236:                                return extensions->getPluginHasMainInput() ? Vst::kMain : Vst::kAux;
3238:                            return Vst::kMain;
```

Bus index 0 is designated `Vst::kMain` unless
`AudioProcessor::getVST3ClientExtensions()` returns an extensions object
whose `getPluginHasMainInput()` answers false; with no override, the
default extensions object's method answers true:

```
$ grep -n "getPluginHasMainInput" ~/JUCE/modules/juce_audio_processors_headless/utilities/juce_VST3ClientExtensions.h
105:    virtual bool getPluginHasMainInput() const  { return true; }
```

`FroggersPluginProcessor` overrides neither `getVST3ClientExtensions()` nor
this method — confirmed by reading `app/vst/` for both names, no hit. A
main input and an aux (sidechain) input are different offers to a VST3
host: Ableton Live 12 and Bitwig give an instrument audio from another
track only through an aux/sidechain bus, never through what they read as
its main input, so this plugin's one input bus is currently unreachable
from another track in either host, whatever the operator does in the
routing UI. JUCE's AU wrapper carries no such main/aux distinction, so AU
hosts are unaffected. Every read of the input bus inside `app/vst/`
(`isBusesLayoutSupported`, `ComputeInputOptionLabels`,
`ResolveSelectedInputChannel`, `processBlock`) addresses it by bus index
and channel count, never by its VST3 bus-type designation, so none of that
code changes.

Decision waiting on the operator: a Live or Bitwig test that this bus, once
reported as an aux input, actually appears as a routable sidechain target
and that a routed track's signal reaches External Audio/External EF. This
proposal makes the reported designation correct; it does not itself
confirm a host's routing UI honors it, which is a manual step (task 3).

## What changes

`FroggersPluginProcessor` gains a `juce::VST3ClientExtensions` member whose
`getPluginHasMainInput()` returns false, and overrides
`getVST3ClientExtensions()` to return a pointer to it. This is the only
production change: no bus is added, removed, or renumbered, the channel
layout and default-disabled state are unchanged
(`BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(),
false)`), and every existing input-bus code path
(`isBusesLayoutSupported`, `ComputeInputOptionLabels`,
`ResolveSelectedInputChannel`, `processorLayoutsChanged`, `processBlock`)
is untouched — they read the bus by index and channel count, which this
change does not alter. The AU wrapper has no main/aux concept and is
unaffected; no AU-specific code exists to change.

The spec gains one requirement: `froggers-vst-host`'s "Bus and MIDI posture
match the core's real I/O" requirement states the bus's presence, channel
layout, and opt-in consent semantics, but nowhere states how the bus is
categorized to a VST3 host. A new requirement states that.

## Impact

- `app/vst/FroggersPluginProcessor.hpp` / `.cpp` (the new
  `VST3ClientExtensions` member and the `getVST3ClientExtensions()`
  override).
- `app/vst/FroggersVstHostTests.cpp` (one new host test).
- `openspec/changes/frogg3rs-plugin-input-as-sidechain/specs/froggers-vst-host/spec.md`
  (this change's own delta).
- `MANUAL.md`, conditionally: task 3, gated on the operator's Live/Bitwig
  run and not executed by this change.

## Delivery

Not pushed by this change. The worktree and branch
(`.claude/worktrees/plugin-sidechain`, `plugin-sidechain`) stay local,
pending the operator's Live/Bitwig routing test (task 3) and a full
`make test` plus the CI-run host suites before any push to `main` on
`daguilarc/frogg3rs`, never as a pull request.
