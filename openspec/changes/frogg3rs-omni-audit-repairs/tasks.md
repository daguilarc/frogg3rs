# Tasks — `frogg3rs-omni-audit-repairs`

Revised by the 2026-09-06 preflight (proposal, "Preflight 2026-09-06").
No fix outside this text. Subagent tier per task: **H** = lightest tier,
mechanical (enumerate, count, delete, substitute); **S** = one tier up, the
task decides what something means. Inline = the parent. Every build runs
under `nice` with `-j2` at most.

## 1. Preflight, inline — done 2026-09-06

- [x] 1.1 Every citation in `proposal.md` resolves at the current tree.
      Corrected: `FinishControl` `:442-484` (was `:441-481`); `Marbles.hpp`
      `:50-71`; the spec requirement `:34-43`; `package-catalog.mjs:56-71`.
- [x] 1.2 `git status --short` shows no uncommitted change to
      `app/FroggersUiSurface.hpp` or `app/FroggersAppCore.hpp`. The variants'
      `git diff --stat` is recorded under P21.
- [x] 1.3 First run of the gates, before line (2026-09-06, load 1.8):
      `make -C app test` exit 0, all 12 binaries exit 0 by path;
      `ctest app/vst/build` 5/5 (binaries of 2026-08-31, not rebuilt);
      `test/firmware` configure ok, build ok, ctest 5/5.
- [x] 1.4 `FinishControl` has no stacking path (P1). Task 3.3 stands.
- [x] 1.5 Three concepts, eight reachable states (P2). Task 4.5 dropped.
- [x] 1.6 Fuegoization: `Parameter.hpp:129-151`, mask from the `FUEG` row's
      knob via `m_fuegoizationKnob`, `mask = (1 << round(knob*8)) - 1`; knob
      0.5 → mask 15, knob 1.0 → mask 255. `VcoAdsrState`, `VcoWaveMorph`,
      independent PM: no flag any host sets (P3) → deleted, tests dropped.
- [x] 1.7 No Sheaf catalog source names `publish/` (P4). Task 5.4 deletes.
- [x] 1.8 `check_no_frozen_includes.sh:14` greps `src/` only (P5).

## 2. Firmware tests get an invocation (finding 2) — S

- [x] 2.1 Root `Makefile`: target `firmware-test` (added to `.PHONY`) that
      runs `cmake -S test/firmware -B test/firmware/build
      -DCMAKE_BUILD_TYPE=Release`, `cmake --build test/firmware/build -j2`,
      and `ctest --test-dir test/firmware/build --output-on-failure`, in one
      `&&` chain.
- [x] 2.2 `.github/workflows/firmware-tests.yml`: `ubuntu-latest`, checkout
      without submodules, `make firmware-test`; triggers on push and
      pull_request with paths `src/core/**`, `test/firmware/**`, `Makefile`,
      and the workflow itself. Diff the cmake/ctest lines against
      `vst-plugin.yml:80-81` and record the diff in the task ledger.
- [x] 2.3 Gate: `make firmware-test` locally, 5/5 pass. Record the pass line.
- [x] 2.4 `README.md`: one sentence under the firmware section naming
      `make firmware-test` as the firmware test entry point.

## 3. Citations resolve, assertions can fail (findings 3, 13, 4a, 4b) — H enumerates, S edits

- [x] 3.1 `app/FroggersDspParityTests.cpp:4226`, `:4258`, `:4464`: cite
      `f2369151^:sim/StereoDelay.hpp:<line>`; `:1065` and `:4198`: name the
      git object the section was ported from. No other text in those
      comments changes.
- [x] 3.2 The six `RequiredHeight` mentions (`FroggersAppCore.hpp:225`,
      `FroggersUiSurface.hpp:270`, `:281`, `:339`,
      `FroggersSurfaceTests.cpp:465`, `:515`): each sentence loses the name
      and says what is true now — `Config().uiHeight` is a plain initial
      window size kept equal to `FroggersPageLayout::kDefaultHeight`
      (`FroggersUiSurface.hpp:299`) by hand; the layout resolves against the
      root extent it is given. The rest of each comment stays.
- [x] 3.3 `app/FroggersUiSurface.hpp:188-194` and `:1489-1495`: state that
      `FinishControl` keeps caption and control in one row
      (`PortableUIBuilders.hpp:442-484`, the `Row` at `:456`), so a label
      below its slider stays a hand-rolled `Label`; the "when that lands"
      prediction and "ask 14" go; the "together" constraint stays.
- [x] 3.4 Enumerate (H) every `<Name>.(hpp|cpp):<n>` citation in
      `FroggersAppCore.hpp`, `FroggersUiSurface.hpp`,
      `FroggersModulation.hpp` whose file lives under `External/Sheaf`;
      for each, resolve (S) the cited symbol's current line and correct the
      number. Report the count found, the count moved, and every correction
      as before → after.
- [x] 3.5 (4a) `stereo_delay_clear_buffers_resets_to_silence`: feed 4800
      samples; keep the last pre-clear wet pair; `REQUIRE_TRUE(std::abs(l) +
      std::abs(r) > 0.0f)`; after `ClearBuffers()` and one zero sample,
      `REQUIRE_TRUE(wet.l == 0.0f && wet.r == 0.0f)`. If the pre-clear pair
      is zero at 4800 samples, that is a finding: print the pair and stop.
- [x] 3.6 (4b) `envelope_followers_track_abs_value_with_attack_release_asymmetry`:
      `expectedFall = ef.attackCoeff * (1.0f - ef.releaseCoeff)`;
      `REQUIRE_NEAR(out[0], expectedFall, 1e-6)`; the `(void)` line and the
      `<` assertion go.
- [x] 3.7 Gate: `make -C app test` and each test binary by path. 3.5 and 3.6
      must pass; record the parity binary's count line.

## 4. Structure in `app/` (findings 6, 7) — S

- [x] 4.0 Positive control, before any edit: in `RouteAudioSample()` negate
      `reverbWetMixEffective` (`:1967`), `rm app/build/froggers_audio_routing_tests`,
      rebuild, run it by path, record the failing test names, restore the
      line, `rm` the binary again. A green run here voids the gate.
- [x] 4.1 `FroggersUiSurface.hpp`: private
      `void LatchThenTransport(bool latched, synth::MessageIn message, bool running)`
      whose body is `app_->SetFreezeLatched(latched); PushMessage(message);
      app_->SetDesiredTransportRunning(running);` with the happens-before
      comment from `:2177-2178` and `:2212-2213` written once above it.
      Replace the three sites (`:2179-2187`, `:2214-2216`, `:2248-2252`);
      Freeze-release keeps its lone `SetFreezeLatched(false)` path. Then
      `app/vst/FroggersPluginProcessor.cpp:753-758`: the line citation
      resolves to the branches' new lines and names `LatchThenTransport`;
      `:1197-1199`: the quoted code matches the new Freeze branch.
- [x] 4.2 `FroggersAppCore.hpp`: one lambda beside `runStopTeardown`
      (`:966`) holding `ForEachStatefulUnit(... Reset ...);
      delayReverbClearPending_ = false;`, called at `:1027` and `:1079`.
- [x] 4.3 One helper mapping `(vco index, role)` → Audio-bank slot for
      pitch (`+0`), shape (`+3`), phase-mod (`+6`), ring-mod (`+9`); used at
      `:1093-1099` and `:1533-1540`; the `:1529-1530` comment goes.
- [x] 4.4 `RouteAudioSample()` (`:1506-1980`): the `knob` lambda (`:1507`)
      becomes a private method; `stoppedKnob`/`releaseKnob` (`:1658-1663`)
      become private methods beside it; the four constants at `:1642-1657`
      become private `static constexpr` members. Then extract each of the six
      sections its comments name (`:1511`, `:1542`, `:1691`, `:1717`,
      `:1878`, `:1932`) into a private method taking what the section reads
      and returning what it writes. The body becomes the sequence of calls.
      No arithmetic changes; the section comments move with their code.
- [x] 4.5 Dropped (P2).
- [x] 4.6 Remove the `:669-671` guard; `drillIn_` is emplaced in `Init()`
      (`:304`). Remove the `:1297-1299` guard and its three-line comment; the
      `drillLevelDisplay_` store reads `drillIn_->Level()` directly.
- [x] 4.7 Gate: `make -C app test` and each test binary by path; then
      `nice cmake --build app/vst/build -j2` and `ctest --test-dir
      app/vst/build --output-on-failure`. Record pass lines and the VST
      binaries' mtimes (newer than the headers).

## 7a. desktop-v2 traces in `app/` and the root (finding 15) — H

- [x] 7.1 `.gitignore`: delete `:2-8` (six trees that no longer exist; keep
      `app/browser/dist/`); reword `:13` without the task label; reword `:22`
      so it no longer cites an archived tasks file (say what `.sdd/` is, in
      one line).
- [x] 7.3 The six `app/` comments drop `desktop-v2` (and the parity file's
      `:6` list of `wasm/, vcv/, web/, desktop/`), keeping `src/`. Rename
      `app/check_no_frozen_includes.sh` → `app/check_no_firmware_includes.sh`
      and the target `check-no-frozen-deps` → `check-no-firmware-includes`
      (`app/Makefile:147`, `:149`, `:175-176`, `:265`; `app/dsp/DspMath.hpp:8`;
      `FroggersDspParityTests.cpp:4`); inside the script, "frozen tree" →
      "firmware tree". `make -C app check-no-firmware-includes` passes.
- [x] 7.4 The spec delta in this change removes
      `v2-excludes-dual-midi-jack-mod-rack`; on archive it lands in
      `openspec/specs/mod-rack-dual-midi-jacks/spec.md`.

## 8a. Docs and backlog, pushable part (findings 8, 14) — S

- [x] 8.1a `README.md:64-67` and `:72`: `src/` is the Daisy Field firmware
      with two variants, `FroggersSolo` and `FroggersGuitar`, built and
      tested by `make firmware-test`; "frozen" and "takes no new work" go.
- [x] 8.1b (H) Enumerate every `frozen`/`FROZEN`/`Frozen` hit under `app/`
      (excluding `app/browser/dist/`). (S) Classify: describes `src/` as
      not-under-development, or another meaning (a Delay/Reverb freeze
      control, the Freeze button, a frozen value). (H) Edit the first class:
      "frozen firmware" → "firmware", "the FROZEN source" → "the firmware
      source", "frozen tree" → "firmware tree", "frozen `src/core/`" →
      "`src/core/`", and the like; the sentence must still read. Report
      FOUND / CHANGED per file.
- [x] 8.2 `openspec/changes/frogg3rs-controllers-page-user-story/proposal.md`:
      one line under the 2026-09-02 supersession note naming
      `frogg3rs-controllers-page-name-in-the-editor` as where tasks 2.1–2.4
      continued. Then `openspec archive frogg3rs-controllers-page-user-story
      --skip-specs -y`.
- [x] 8.3 `openspec/changes/frogg3rs-guitar-and-solo-variants/specs/external-ring-mod-mix/spec.md`:
      reflow the MODIFIED requirement "External gate selects VCO-only vs ring
      mod" so its first body line carries the SHALL; no word changes. Gate:
      `openspec validate --all --strict` all green. Record the before/after
      totals.

## 6a. Comment labels in `app/` (finding 12) — H enumerates, S classifies and edits

- [x] 6.1 (H) Enumerate every line in `app/*.hpp`, `app/*.cpp`, `app/dsp`,
      `app/vst/*.cpp`, `app/vst/*.hpp`, `app/standalone`, `app/Makefile`,
      `app/browser/*.mjs`, `src/core`, `src/common`, `src/Froggers*`,
      `src/mk`, `test/firmware/*.cpp`, `test/firmware/*.inl`,
      `test/firmware/CMakeLists.txt` matching any of:
      `\b(Task|task|Packet|packet) [0-9]+(\.[0-9]+)*\b`;
      `\b[DFTBPS][0-9]{1,2}[a-z]?(\.[0-9]+[a-z]?)?\b`; `\bsru-[0-9]+`;
      `\bsprs-[0-9]+`; `\bOMNI\b`; `\bask [0-9]+\b`; `§[0-9]`. Exclude
      `0x…` and `#include` lines. Output: file:line:text, no edits.
- [x] 6.2 (S) Classify every hit: label (a planning identifier the reader
      cannot resolve from the code) or not (`B1`-style bank names, key
      names, physical units, `D5` parameter slots and the like). Report
      counts per file and the full list of labels.
- [x] 6.3a (S) Edit each label line under `app/`: the label goes, the
      sentence after it is kept and reads as a sentence. A comment that is
      only a label is deleted. The five omni-rule citations are in this set.
- [x] 6.4a Gate: `make -C app test` and each test binary by path; `nice
      cmake --build app/vst/build -j2 && ctest --test-dir app/vst/build`.
      `git diff -w --stat` and a read of every hunk: no non-comment line
      changed.

## Push point

- [x] P.1 Commits so far: change dir; 2; 3; 4; 7a; 8a; 6a. Push the branch,
      open the pull request (body: the task-group list with each gate's pass
      line). Then one commit snapshotting the variants' uncommitted work
      (message: `frogg3rs-guitar-and-solo-variants: uncommitted work as of
      2026-09-06, base for the local-only commits below`).

## 5. Structure in `src/`, dead code, dead dependency, stale output (findings 10, 11, 9, P3, P12) — local-only

- [x] 5.1 (H) Delete `src/common/Comb.hpp`, `Marbles.hpp`, `ModMgr.hpp`,
      `PolynomialDrive.hpp`, `ResonantBump.hpp`. Confirm first by grep that no
      include resolves to any of them (operands: `common/<Name>.hpp`, and
      `"<Name>.hpp"` from inside `src/common/`).
- [x] 5.2 (S) `src/TestControl/` never compiled (P22): `git rm -r` it, then
      delete `src/common/Include.hpp` and the remaining eleven shims. Then
      `DaisyIO.hpp:5-6` → `../core/Page.hpp`, `../core/SchmidtTrigger.hpp`;
      `FieldMutationQueue.hpp:3` → `../core/Page.hpp`. `App.hpp`,
      `DaisyIO.hpp`, `FieldMutationQueue.hpp`, `FieldSwitchGuard.hpp` stay.
- [x] 5.3 (H) `git rm External/theallelectricsmartgrid`; delete its
      `.gitmodules` block; remove `.git/modules/External/theallelectricsmartgrid`
      if present; `git submodule status` lists only `External/Sheaf`.
- [x] 5.4 (H) `git rm -r publish/`. (S) Rewrite
      `app/browser/package-catalog.mjs:56-71` to describe only what the
      script does now: publisher id `daguilarc`, and the catalog and package
      generated into `app/browser/dist/site` on each deploy.
- [x] 5.4b (S) Move `RefreshGate` from `src/common/DaisyIO.hpp:15-32` to
      `src/core/RefreshGate.hpp`; `DaisyIO.hpp` includes it. Delete the
      restated struct and the `:13-15` comment from
      `test/firmware/RefreshGate_test.cpp`; the test includes the real header.
- [x] 5.6 (S) Simulator host hooks out of `src/core/FroggersEngine.hpp`.
      First: `HookIdentity_test.cpp` prints
      `checksum=<sum of out[i]*(i+1)>` and `maxabs=<max |out|>` after its
      PASS line, is renamed `EngineDeterminism_test.cpp` (CMakeLists `:19`,
      `:33`); build and run it; record the two numbers (the before line;
      `maxabs` must be > 0 or the instrument is dead). Then delete, keeping
      the legacy branch of every collapsed ternary or `if` verbatim:
      includes `:15-17`; `SimFxInsertFn` `:25`; `m_pm3` `:43` and `:210`;
      `:111-119` except `m_pairAr` (`:117`) and the FX insert pair (`:115-116`); `:122-172` (PM flag, phases,
      `x_pmLfo*`, `PmDepthScale`) — keep `m_pair12`/`m_pair23`; the
      `EvalWaveMorph` wrapper `:202-205`; `SetSimWaveMorph` `:233-236`;
      `SetSimDedicatedPm3Knob` `:238-241`;
      `SetVcoAdsrState` `:255-259`; `GetAdsrParamForTest` and its comment
      `:261-270`; `SetSimIndependentPm` `:272-275`; the morph accessors
      `:288-370`; the sanitize loop `:395-401`; the `if` at `:452-458`
      collapses to its `else` body; buttons 3/4 `:688-708` keep only the
      wave-cycle statements; `StepIndependentPmLfo` and comment `:711-724`;
      `pm3d = ZeroedExp(fuegKnob)` at `:730`; `:736-742` and `:772-774` keep
      `EvalWave`/`SDDSine::Evaluate`; `:744-766` keeps the legacy block;
      `:786-797` (the `m_vcoAdsr` block).
      Delete `src/core/VcoAdsrState.hpp`, `VcoWaveMorph.hpp`,
      `VcoWaveEval.hpp`. `Marbles.hpp:50-71` (`ResetPageToDefaults` and its
      comment). `Page.hpp:262-274` (`SanitizeSimModAssignments`).
      `VariantMix_body.inl:58-59`, `:71-72`; `VariantMix_solo.cpp:21-22`.
      Then `make firmware-test` and run `EngineDeterminism_test` by path: the
      checksum and maxabs equal the before line. Report every line deleted
      as before → after ranges, and FOUND vs CHANGED for each operand
      (`m_sim`, `Morph`, `pmLfo`, `Adsr`, `SimFx`, `m_pm3`).
- [x] 5.7 (H enumerates, S resolves) Every `app/` citation into a `src/core`
      file group 5 edited or deleted: operands `FroggersEngine.hpp:`,
      `Marbles.hpp:`, `Page.hpp:`, `VcoAdsrState.hpp`, `VcoWaveMorph.hpp`,
      `VcoWaveEval.hpp`. Line citations re-resolve to the current line of the
      cited symbol; citations into the three deleted files become
      `08b5fd3:src/core/<Name>.hpp[:<n>]`. Report FOUND / MOVED / RETARGETED
      per file. Tick the handover item at
      `openspec/changes/frogg3rs-guitar-and-solo-variants/tasks.md:221` with
      a one-line pointer to this task.
- [x] 5.5 Gate: `rm -rf src/FroggersSolo/build src/FroggersGuitar/build`;
      `nice make -C src/FroggersSolo -j2`, `nice make -C src/FroggersGuitar
      -j2`; `make firmware-test`. Record pass lines and the two `.bin` sizes.

## 6b. Comment labels in `src/` and `test/` (finding 12) — H, S

- [x] 6.1b (H) Re-run 6.1 over `src/core`, `src/common`, `src/Froggers*`,
      `src/mk`, `test/firmware` after group 5. (S) Classify as 6.2.
- [x] 6.3b (S) Edit each label line as 6.3a.
- [x] 6.4b Gate: `make firmware-test`; `git diff -w` read hunk by hunk: no
      non-comment line changed in this commit.

## 7b, 8b. desktop-v2 and docs, local-only part

- [x] 7.2 `src/core/FroggersEngine.hpp`: any surviving comment naming
      desktop-v2, web, "V2 hosts" or "Daisy/v1" as a host distinction says
      what is true (one firmware, two variants). `Marbles.hpp:50-56` is gone
      with 5.6; confirm `grep -n 'desktop-v2\|Desktop-v2' src/` is empty.
- [x] 8.1c `DAISY_MANUAL.md:15`: the "Frozen" sentence becomes: the firmware
      is built and tested by `make firmware-test`; the two variants are the
      table above. `grep -c FroggersTiga DAISY_MANUAL.md` is 0.

## 9. Firmware coverage (finding 1) — S, after group 5

- [x] 9.1 `test/firmware/Parameter_fuego_test.cpp`: knob 0 → output equals
      input for every row 0–15; knob giving mask 15 and knob giving mask 255
      → output differs from input for at least one row (the positive
      control) and equals the formula at `Parameter.hpp:129-151` for every
      row. Registered in `CMakeLists.txt` through `add_firmware_test`.
- [x] 9.2 `DspModules_test.cpp`: one property each for `BiquadSection`,
      `Comb`, `OPLowPassFilter`, `PolynomialDrive` (`FrogBlock`), `RGen`,
      `SDDSine`, `SampleRateReducer`, `TanhSaturator`, `Marbles`, each with
      the input that makes the property false as the control.
- [x] 9.3 `Page_test.cpp`: `KnobUpdate` moves a parameter, mod source and
      depth round-trip, `Randomize` changes at least one value and never the
      `FUEG` row (`Parameter.hpp:191-194`), page navigation wraps
      (`PageNext` `:429`, `PagePrevious` `:438`).
- [x] 9.4 Gate: `make firmware-test`, 8/8 pass. Each new binary is run
      once with one assertion inverted to show it fails (recorded, then
      restored; `rm` the binary between the two builds).

## 10. Verification and delivery

- [x] 10.1 Every gate in the proposal's table, run again on the final tree,
      pass lines recorded next to the first-run lines from 1.3.
- [x] 10.2 §5 against the diff (S, fresh context): every new helper
      (`LatchThenTransport`, the clear lambda, the slot helper, the routing
      stages and knob methods, `RefreshGate.hpp`) grepped by its operands
      across `app/` and `src/` for a second computation of the same thing.
- [x] 10.3 Postflight (S, fresh context): implementation vs this text, per
      task, divergence reported strictly; the spec deltas checked against a
      passing check each.
- [x] 10.4 Local-only commits listed by hash in the ledger; `git status`
      clean at repo and submodule level.
- [x] 10.5 OPERATOR: Play, Stop, Freeze on and off in the desktop app; the
      instrument silences on Stop and on Freeze release exactly as before.
- [x] 10.6 The pair-AR path (P19): deleted from src/core, test/firmware, and the specs; see the ledger.

## Ledger — 2026-09-06

Delivery correction: this repository does not use pull requests. The pull
request the proposal called for was opened, closed unmerged, and its branch
deleted; every commit went to `main` directly. Operator's go for the
firmware-side commits (the variants snapshot included) given 2026-09-06 after
the final gates.

On `main`, in order: the change directory; group 2 (`6359e50`); group 3
(`b776b8a`); group 4 (`7299808`); 7a (`336bb59`); 8a (`c4f4ff6`, `56d9853`);
6a (`98a6e97`); the CI toolchain fix (`4262c59`, P23); the §5 and postflight
fixes (`e5c83cc`: the VCO slot map moved to `FroggersParameters.hpp` and used
by `ApplyAudioBankOverlay`; 19 printed test labels stripped; the caption
comment's history paragraph removed); the variants snapshot (`3fe8941`);
group 5 (`2cda5dd`); 8.1c (`95dba56`); group 9 (`6211830`); 5.7 and the
artifact findings (`62279d0`); one `Check.hpp` for the firmware tests
(`e0f6384`); this ledger.

Gates on the final tree: `make -C app test` exit 0 and all 12 binaries exit 0
by path (parity 147/147, modulation 46/46, marbles clock 8/8); VST rebuilt,
`100% tests passed, 0 tests failed out of 5`; `make firmware-test` `100% tests
passed, 0 tests failed out of 8`; `EngineDeterminism_test`
`checksum=-274743.095 maxabs=0.855762899` before and after the engine
deletion; `FroggersSolo.bin` 86956 bytes, `FroggersGuitar.bin` 85784 bytes
(88548 and 87368 before group 5); `check_no_firmware_includes: OK`;
`openspec validate --all --strict` `26 passed, 0 failed`; the firmware-test
workflow passes on GitHub.

Positive controls recorded: 4.0 (two audio-routing tests red on a negated
wet mix); 5.6 (checksum equality); 9.4 (each new binary red with one
assertion inverted); the Check-helper move (Page_test red with one check
inverted).

Counts: Sheaf citations 82 found, 24 moved, 0 unresolved; `src/core`
citations from `app/` 89 found, ~39 moved, ~30 retargeted to `08b5fd3:`;
labels 216 regex hits in `app/` (135 labels edited) plus 43 the regex missed
(include-line comments, printed strings, `design E3d`, `item N`); `frozen` 93
found, 46 changed, 47 another meaning; 22 `src/common` files deleted, 3
`src/core` headers deleted, ~300 engine lines deleted.

Pair-AR removal (operator ruling 2026-09-06): PairArEnvelope.hpp, AudioPairArState.hpp, the m_pairAr branch, PairArEnvelope_test, the VariantMix PairArRun probe; REMOVED pair-ar-vcv-time-range; MODIFIED external-ring-mod-mix, mod-blend-semantics, field-operator-doc-parity.

Open for the operator: 10.5 (Play/Stop/Freeze by ear).

Operator confirmed these checks 2026-09-07.
