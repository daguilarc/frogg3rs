# Proposal — `frogg3rs-operator-runs`

Holds the runs an audit of frogg3rs `main` against its ratified user story
left that only the operator can make, because they need controller hardware,
host applications, `sudo`, an x86 machine or the operator's own
configuration, and every fix gated on one of them. Its library half is the
Sheaf change `app-operator-runs`
(`External/Sheaf/openspec/changes/app-operator-runs/`). Operator ruling
2026-09-23: "Operator runs never hold delivery. Agents run everything an agent
can run on this Mac. The operator's runs, and every fix gated on one, ship
marked not yet delivered, naming the change that will deliver each." This is
that change. The ratified story is `openspec/story.md`.

## Why

The audit left these questions open after every agent-runnable run:

- BUG-04: whether JUCE calls two MIDI input ports' callbacks on two threads
  (two controllers streaming).
- BUG-05: whether any supported host calls the plugin's `prepareToPlay` off the
  message thread (Logic, Ableton Live, Reaper).
- BUG-08: whether Live's MIDI Map mode and Logic's touch automation see a knob
  drag in the plugin window.
- BUG-14: whether the standalone's audio thread enters the kernel on clock and
  transport enqueues (`sudo dtrace`).
- OPT-04 on x86: flush-to-zero in the standalone's audio callback.
- OPT-05: whether a supported host sends a block larger than the prepared size
  (pluginval and three hosts).
- OPT-26, OPT-28, OPT-30 on real hardware and the operator's own
  configuration: typing on the browser with a real Twister, saving the
  operator's own `config.json`, and page faults over a 10-minute take.
- The operator's reading of the manual's Connect messages, offline port and
  Reset subsections against the app.

## What changes

The runs (tasks 1.1 to 1.10), then each fix only if its run confirms it
(section 2 here; the library fixes are `app-operator-runs` 1.1 to 1.3). The
`froggers-vst-host` delta (`specs/froggers-vst-host/`) states the two plugin
requirements the runs settle.

## Impact

Conditional on the runs: `app/FroggersAppCore.hpp`,
`app/vst/FroggersPluginProcessor.cpp` and `.hpp`,
`app/vst/FroggersPluginEditor.cpp`, `app/vst/FroggersVstHostTests.cpp`, and the
`External/Sheaf` pin.

## Delivery

Pushed to `main` on `daguilarc/frogg3rs`, never as a pull request, after the
Sheaf branch `app-operator-runs` is pushed to the fork `daguilarc/Sheaf` and its
pull request against `jvictor0/Sheaf` `main` is open, with the pin moved to
that branch's tip (task 3.1).
