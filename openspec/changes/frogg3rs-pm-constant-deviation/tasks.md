# Tasks — `frogg3rs-pm-constant-deviation`

Preflighted 2026-09-07 and revised under that audit. Tier per task: **H** =
mechanical, **S** = decides what something means. Builds under `nice`, `-j2`
at most.

## 1. Preflight — closed

- [x] 1.1 Every citation in `proposal.md` resolves at the current tree. Six
      `Vco.hpp` citations were seven lines low, taken before `e69c5d9`
      inserted a comment block; all six corrected and re-read at `c65d603`.
      Every other citation resolved as written.
- [x] 1.2 `WrapPhase` is `p - floor(p)` (`app/dsp/DspMath.hpp:45-48`): folds
      any magnitude into [0, 1), so the floor's 1.5-cycle swing is safe.
- [x] 1.3 Default pitch knobs are 0.3087/0.4343/0.5077
      (`app/FroggersParameters.hpp:154`) = 110/220/330 Hz. At the 2 Hz floor
      at full depth the deviation is 1.885 Hz on every carrier, so in cents
      VCO1 gets 29.4, VCO2 14.8, VCO3 9.9. This CONTRADICTS the reported
      ordering rather than explaining it: VCO1 holds the lowest carrier and
      moves the most cents. The PM path was then traced for a per-VCO
      difference and has none (wiring, taper and mix all symmetric); a probe
      against the built DSP measured 1.8824/1.8839/1.8846 Hz for VCO1/2/3,
      carrier means 109.88/219.98/329.93 Hz as the live-instrument control.
      Recorded in `proposal.md` as an open question owned by no change here.
- [x] 1.4 Hz-constant deviation, per the operator's go on this proposal. The
      cents-constant alternative is recorded in "Not in this change".
- [x] 1.5 Before line: `nice make -C app -j2 test` green, 12 binaries / 351
      tests, exit 0. `openspec validate --all --strict` 28/28.

## 2. The measurement (finding first) — S

- [x] 2.1 Cherry-pick the test from `ed956cd`
      (`pm_pitch_deviation_is_the_same_at_every_rate`) alone, STRIPPING its
      "Without the rate scaling the floor would read one tenth of the top"
      sentence — a counterfactual about earlier behaviour, which comments
      here do not carry. Run the parity binary by path (`make test` stops at
      the first red binary): it must FAIL at knob 0 and 0.5 with 1.9 Hz and
      6.0 Hz printed and pass at knob 1. A pass at knob 0 voids the gate.

## 3. The change — H

- [x] 3.1 Cherry-pick the `Vco.hpp` hunk of `ed956cd` (the multiply in
      `StepPmLfo`); rewrite its two comment blocks so they state the rule
      (the audible quantity is the slope of the phase offset, so the LFO is
      scaled by `kPmLfoMaxHz / rate` and the deviation is
      `2 pi x kPmLfoDepth x kPmLfoMaxHz` at every rate) and name no earlier
      constant, no earlier behaviour, and no fix. `ed956cd`'s own replacement
      block still says the bottom of the knob "was inaudible ... whatever the
      depth knobs did"; that sentence does not come across.
- [x] 3.2 Gate: `nice make -C app -j2 test`; 2.1 passes with 18.8 Hz at
      all three knob positions; the numbers go into the ledger.

## 4. Spec and manual — S

- [x] 4.1 The delta in `specs/froggers-vco-topology/spec.md`; `openspec
      validate frogg3rs-pm-constant-deviation --strict`.
- [x] 4.2 `MANUAL.md`, present tense, no history. Ph.mod paragraph
      (`:366-369`): "...grows from a subtle vibrato into an increasingly
      warbly, FM-like wobble at full depth. The size of the wobble is set
      here and does not change with the rate." PM rate paragraph
      (`:376-378`): "one shared knob (2 Hz-20 Hz) setting how fast the
      phase-mod wobble runs for all three VCOs at once. Depth and rate are
      independent: this sets the speed for all three, and each VCO's own
      Phase mod knob sets how far its pitch swings." `QUICK_DICT.md:23`
      unchanged (it states the range only).
- [x] 4.3 Gate: `openspec validate --all --strict`.

## 5. Hygiene (§8.0, over `app/dsp/` — the tree this change touches) — H

- [x] 5.1 The VCO pitch ceiling in the app's docs. `Vco.hpp:142` sets
      `kPitchMaxHz = 5000.0f`; `MANUAL.md:357-358` and `QUICK_DICT.md:19`
      both still say "20 Hz to 20 kHz". Correct both to 5 kHz.
      `DAISY_MANUAL.md:221` says 20 kHz for the FIRMWARE, where
      `src/core/FroggersEngine.hpp:242-244` really does map to 20000 Hz —
      leave it alone.
- [x] 5.2 The 22 firmware citations in `app/dsp/` that point at the wrong
      code, and the 6 that are correct only at `08b5fd3` and carry no pin.
      Full resolved list with exact replacements is in the preflight record;
      it spans `Vco.hpp` (4), `DspMath.hpp` (1), `Drive.hpp` (7),
      `Reverb.hpp` (6), `FilterFx.hpp` (3), `VoiceEnvelope.hpp` (2),
      `RandomShLane.hpp` (1), `Fuegoize.hpp` (2). Two are not drift and must
      be fixed as a family: `Parameter.hpp:143` should be `:142` at all three
      sites that cite it (`Drive.hpp:290`, `Fuegoize.hpp:26,66`), and
      `RandomShLane.hpp:47`'s third RGen site is `Parameter.hpp:214`, not
      `:223`. Comments only; no code changes in this task.
- [x] 5.3 The history-narrating comment blocks in the file this change edits:
      `Vco.hpp:118-123` (names the superseded 0.3 Hz floor and states the
      deviation rule this change makes false) is rewritten by 3.1;
      `Vco.hpp:260-276` narrates a shipped scope-write bug and quotes the
      report that found it. Rewrite the latter to state what the code does.
- [x] 5.4 Gate: `nice make -C app -j2 test` after the hygiene edits, since
      5.2 and 5.3 touch headers every binary compiles.

## 6. Verification and delivery

- [x] 6.1 Every gate in the table on the final tree; VST rebuilt and
      `ctest`.
- [x] 6.2 Postflight (S, fresh context): implementation versus this text.
- [x] 6.3 Sheaf first, because the submodule pin cannot resolve until its
      target exists on a remote: force-push `fix-out-of-tree-app-gaps`,
      `surface-background-constant`, `browser-audio-activation` and
      `app-midi-catalog` to `fork` (the rebase rewrote all four), and confirm
      `b50cca18` resolves. `.gitmodules` points at upstream
      `jvictor0/Sheaf`, so this relies on the shared fork/upstream object
      store — the same way the current pin `9368bf17` already resolves.
      Then three frogg3rs commits, no AI attribution: the PM change (2, 3, 4);
      the `app/dsp` hygiene (5, 7.4); the operator requests (7.1 colour and
      the `External/Sheaf` pin bump, which ships HERE even though the Sheaf
      change itself ships there). `Vco.hpp` and `MANUAL.md` carry edits from
      both the first two groups — the PM change and the same-file hygiene
      interleave in them — so they land whole in the first commit rather than
      being split hunk by hunk. Fast-forward push to `main`.
- [x] 6.4 Rebuild the releases: move the `frogg3rs_v2` and `frogg3rs_vst`
      tags to the new head and force-push them, which is what re-runs
      `desktop-release.yml` and `vst-plugin.yml`; `pages.yml` runs off the
      `main` push itself. Confirm all three workflows green.
- [x] 6.5 OPERATOR: on the desktop app, PM rate at its floor with any
      Ph.mod knob up wobbles the pitch plainly on all three VCOs, and the
      top of the rate knob sounds as it did. Expect a wide warble at the
      floor at full depth — 274 cents on VCO1, 142 on VCO2, 96 on VCO3,
      which is what the top of the rate knob already produces today.

## Operator requests folded in

- [x] 7.1 Delay bank encoder colour: `synth::Color::Indigo` (75,0,130) ->
      `synth::Color::Rgb(255,105,180)`, hot pink
      (`app/FroggersParameters.hpp:240`). Sheaf's palette carries no pink and
      `Color::Rgb` is `constexpr`, so this needs no submodule change. Nothing
      else in the tree referenced `Indigo`.
- [x] 7.2 The sidebar's "CPU N%" readout. TRACED: it is not CPU usage. It is
      the share of each audio block's time budget the render consumed, which
      JUCE supplies as `AudioDeviceManager::getCpuUsage()`
      (`External/Sheaf/projects/synth/runtime/Runtime.hpp:480`), formatted
      `"CPU %.0f%%"` by `Layout::FormatDeadlineText`
      (`.../include/synth/RuntimePages.hpp:434-438`) into node
      `runtime.sidebar.deadline` (`:48`). Over 100% is therefore MEANINGFUL:
      the render overran its budget and the audio glitches. Capping or
      normalizing would delete the only signal it carries, so the fix is the
      label: "DSP %", which reads correctly above 100%.
      The label was SPECIFIED, not incidental: requirement `sru-59` in Sheaf's
      own unmerged change `fix-out-of-tree-app-gaps` (PR #9) mandated the
      `CPU ` prefix, with a scenario asserting it. Renaming therefore amends
      the requirement at its source rather than contradicting it. Sheaf commit
      `9973d3ab` on `fix-out-of-tree-app-gaps` changes 17 of 20 sites: the
      formatter, its prose comment, four test pins
      (`juce/RuntimePagesJuceTests.cpp:87`,
      `tests/portable_ui_tests.cpp:3595,4383,4599`,
      `tests/runtime_main_component_tests.cpp:851`,
      `browser/tests/audio-flow.spec.ts:880,888,907`), `sru-59` and its
      scenario, and the two changes describing the label. Three are left as
      historical records: the pre-change `"CPU %.1f%%"` format and `"CPU 0.0%"`
      sentinel quoted by `shorten-deadline-readout-window`, and an operator's
      verification log in `ui-state-before-audio/tasks.md:120`.
      Stack rebased onto the amended #9: #9 `9973d3ab`, #11 `60ad4419`,
      #12 `62820f63`, #13 `b50cca18`.
- [x] 7.3 Sheaf hygiene found by that work: `sru-48`'s SHALL sat on the second
      body line, where `openspec validate` does not look, so `--strict` called
      the requirement SHALL-less. Reflowed, not reworded (Sheaf `b50cca18`).
      Sheaf `openspec validate --all --strict` 86/1 -> 87/0.
- [x] 7.4 Pin the two citation families this change left half-pinned:
      `TanhSaturator.hpp:25-30` (FOUND 4, CHANGED 4 — `FilterFx.hpp:18,101`,
      `Drive.hpp:53,393`) and `Parameter.hpp` (FOUND 6, CHANGED 6 —
      `Drive.hpp:290`, `Fuegoize.hpp:20,26,34,66`, `RandomShLane.hpp:47`).
      Pinning a family's members one at a time is what leaves the rest to
      dangle silently.

## Not fixed here, and why

`app/dsp/` carries 46 further firmware citations that are correct today but
unpinned, so they dangle silently as `src/core/` drifts — which is how the 22
in 5.2 broke. The family's real deliverable under §5 is a check that fails on
drift, and the only check that works is "every `src/core` citation carries a
commit pin", which requires re-pinning 46 comments that are currently right.
Editing 46 correct comments is a different and riskier change than repairing
22 wrong ones, and it needs its own proposal. Named here so it is not lost.

Closed 2026-09-07. Postflight ran fresh-context over both repos and its
findings were applied before delivery. Sheaf pushed first (#9 `9973d3ab`,
#11 `60ad4419`, #12 `62820f63`, #13 `b50cca18`), then frogg3rs `bae961a`,
`57f98e3`, `6e76316` to `main`, then both release tags moved. CI green:
Desktop Release 6m17s, VST Plugin 17m3s, Pages 4m16s, Firmware tests on
both tags. Operator confirmed the PM behaviour.
