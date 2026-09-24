# Tasks — `frogg3rs-midi-out`

Every new test is shown to fail with its production change reverted, by the
executor, before the task is reported done, and the report says so. Builds run
under `nice`, `-j2`, one at a time, and never two builds or suites at once,
background included. A test that is red and not named in a task is reported,
never edited.

A measurement records what it tapped, where in the path, under which settings,
and its positive control. A measurement whose positive control does not move is
void and is re-run, not read. Under the operator's threshold framework a
finding is a missed audio block deadline, a block whose processing time reaches
its budget (frames divided by sample rate); a worst block below the budget is
reported as a number and gates nothing. A deadline miss counts only when both
baseline runs at that setting miss none; if a baseline misses, the setting is
re-run once. A setting whose baseline misses again on the re-run is one the
app misses without MIDI out: it is recorded as such, the MIDI-out path's own
worst time per block there is recorded beside it as a fraction of the budget,
and it gates nothing.

The Implementation section below starts with task 0, the rebase the operator
ruled: midi-out is developed and pushed on its own branches, and is rebased
onto main as main stands when the operator says the other sessions' work on
main is done; open changes nobody is working on are not waited for. Tasks 2
onward cannot start before the Sheaf change `app-midi-out` is in the pinned
submodule commit; task 1 says how to tell. The operator runs do not wait on
either.

Commands the tasks rely on, run from the repository root, one at a time:

```
# App binaries: builds and runs every binary in test's recipe; after it
# stops, run each remaining binary by path from app/build/.
nice make -C app -j2 test
# Plugin tests, as vst-plugin.yml runs them:
cmake -S app/vst -B app/vst/build -DCMAKE_BUILD_TYPE=Release
nice cmake --build app/vst/build --config Release -- -j2
ctest --test-dir app/vst/build --output-on-failure
# Browser specs, by the route under Implementation:
app/browser/build-browser.sh
(cd app/browser/e2e && npx playwright test <spec>)
# Standalone, the macOS launcher and the CMake build:
./app/build-launcher.sh
cmake -S app/standalone -B app/standalone/build -DCMAKE_BUILD_TYPE=Release
nice cmake --build app/standalone/build --config Release -- -j2
```

The evidence directory named below is
`/Users/diegoaguilar-canabal/.claude/projects/-Users-diegoaguilar-canabal-Desktop/fca76ec7-1fa9-41b0-854c-718d9051aa6c/evidence/`.

## Before approval (run inside the before-code audit)

These run before the proposal is approved, and their numbers are written into
this file before any implementation task starts. M1 and M3 are recorded from
the evidence directory's `measure-q/`, reconfirmed at the ruled range by
`measure-q/report-range.md`; M2 is withdrawn; M4 is recorded in
`app-midi-out` from `measure-native-2/`; M5 is withdrawn there; M6, which both
changes carry, is recorded from `measure-m6/report.md`. No measurement
remains open.

The pitch path (the operator's ruling, in the proposal). The audio thread
folds each output sample to mono once, as `0.5 * (l + r)` after
`RouteAudioSample()`, and feeds that fold to Level's
`dsp::SingleEnvelopeFollower` and to Cycfi Q's pitch detector (cycfi::q::pitch_detector),
one call each per sample, whatever the content. The detector is constructed
only in `PrepareToPlay`, on the thread that calls `Engine::Prepare()`, never
lazily from the audio callback, because construction allocates and
allocation must stay off the audio thread. `MidiSender::Start()` also runs
only there (coordinator ruling, from M6: spawning its worker thread from
inside the AudioWorklet's own thread is not a safe call under
`-sPTHREAD_POOL_SIZE=1` and silently kills the worklet), at the host rate with a
lowest frequency of 50 Hz, a highest frequency of 5,000 Hz and a hysteresis
of -30 dB (Q-R, the coordinator's ruling: 5,000 Hz because the VCOs reach
5 kHz and Pitch sends the output's pitch; 50 Hz because it is the lowest
floor the <=100 ms latency ruling allows; the manual says notes below 50 Hz
are not tracked), the configuration `measure-q/report-range.md` measured.
There is no second thread, no ring, no decimator and no analysis-rate
resampling: the detector runs at the host rate on the audio thread in every
build and in every host mode, realtime or not.

M6, the one browser run before approval, is recorded below with the
browser and launch it actually used. No browser run remains before approval.

- [x] M1. Audio-thread cost of the MIDI-out path, range 50 to 5,000 Hz (Q-R).
      Recorded from `measure-q/report-range.md` Table 3
      (`measure-q/QCostRange.cpp`, `measure-q/qcostrange_output.txt`, run
      exit 0 after a `fixedBlockFrames` fix caught before reporting -- see
      `report-range.md`'s "What stopped me"; supersedes this task's earlier
      reading at 50 to 4,000 Hz).
      Tapped: per call, the wall time of `Engine::ProcessBlock` through
      `SynthRig<FroggersApp>` (two baseline reps, no addition), and,
      separately, the wall time of the addition (fold, follower and Q's
      pitch detector together) run over a captured device-output stream
      chunked to match each baseline's own per-call frame counts; combined
      per call index as the sum.
      Settings: 48 kHz/128-frame, 192 kHz/1-frame and 192 kHz/blocks varying
      1 to 2048 frames, each on the default patch and
      on phase-modulation-at-maximum, two reps per setting/patch; detector
      constructed at 50 to 5,000 Hz, -30 dB.

      | Setting | Patch | baseline worst (ns) | addition's own worst (ns) | combined worst (ns) | budget (ns) | baseline missed | combined missed |
      |---|---|---|---|---|---|---|---|
      | 48 kHz / 128-frame | default (rep1) | 1,089,833 | 4,625 | 1,090,791 | 2,666,667 | 0/750 | 0/750 |
      | 48 kHz / 128-frame | default (rep2) | 1,110,500 | 4,625 | 1,111,250 | 2,666,667 | 0/750 | 0/750 |
      | 48 kHz / 128-frame | phase-mod max (rep1) | 1,130,291 | 3,709 | 1,130,958 | 2,666,667 | 0/750 | 0/750 |
      | 48 kHz / 128-frame | phase-mod max (rep2) | 1,077,542 | 3,709 | 1,078,292 | 2,666,667 | 0/750 | 0/750 |
      | 192 kHz / 1-frame | default (rep1) | 354,959 | 21,166 | 354,959 | 5,208.3 | 384000/384000 | 384000/384000 |
      | 192 kHz / 1-frame | default (rep2) | 5,490,208 | 21,166 | 5,490,208 | 5,208.3 | 384000/384000 | 384000/384000 |
      | 192 kHz / 1-frame | phase-mod max (rep1) | 5,564,667 | 47,333 | 5,564,667 | 5,208.3 | 384000/384000 | 384000/384000 |
      | 192 kHz / 1-frame | phase-mod max (rep2) | 15,697,292 | 47,333 | 15,697,292 | 5,208.3 | 384000/384000 | 384000/384000 |
      | 192 kHz / varying (1..2048) | default (rep1) | 4,996,042 | 15,750 | 5,007,750 | 10,666,667 (@2048fr) | 231/678 | 231/678 |
      | 192 kHz / varying (1..2048) | default (rep2) | 5,267,375 | 15,750 | 5,280,416 | 10,666,667 (@2048fr) | 226/678 | 226/678 |
      | 192 kHz / varying (1..2048) | phase-mod max (rep1) | 6,031,292 | 18,125 | 6,044,542 | 10,666,667 (@2048fr) | 284/678 | 291/678 |
      | 192 kHz / varying (1..2048) | phase-mod max (rep2) | 6,173,958 | 18,125 | 6,187,625 | 10,666,667 (@2048fr) | 310/678 | 310/678 |

      Positive control: a padded `sin()` loop inside the addition, default
      patch, at each of the three settings, 0, 200 and 1,000 iterations per
      sample (its own baseline run per setting). 1,000 iterations raised the
      addition's own worst at every setting (13,625 to 1,577,875 ns at
      48 kHz/128; 4,542 to 67,750 ns at 192 kHz/1; 13,459 to 8,179,542 ns at
      192 kHz/varying) and the combined worst at every setting (5,275,083 to
      5,729,291 ns; 3,400,625 to 3,403,541 ns; 5,306,708 to 12,430,250 ns).
      The combined miss count moved only at 192 kHz/varying (231/678 to
      572/678); at 48 kHz/128 it stayed 1/750, the control run's own baseline
      miss, and at 192 kHz/1 every block already missed (384000/384000). The
      instrument is live on the worst-block time at every setting and on the
      miss count at the one setting where the pad could add misses.
      Reading under the framework: only 48 kHz/128-frame has both baselines
      miss none, and at that setting the addition causes zero combined
      misses on either patch. At 192 kHz (both block settings) both
      baselines already miss on their own, so this machine misses those
      deadlines without MIDI out and they gate nothing, except that at
      192 kHz/varying, phase-modulation-maximum's first rep shows 7
      additional misses at the margin (284 baseline, 291 combined) -- the one
      place the addition visibly pushes already-close blocks over their
      deadline, reported as the real difference it is and still gating
      nothing (the setting's baseline already misses). The addition's own
      worst per-block time stays in the low tens of microseconds at every
      setting and patch (3,709-47,333 ns), 16.8 to 340.6 times below the
      same row's baseline worst (0.355-15.7 ms across the rows).
      Result: the standalone offers Level and Pitch; the plugin offers Level
      and Pitch, running the same `FroggersAppCore::ProcessBlock` and adding
      only its buffer clear and at most two `addEvent` calls per block; the
      browser offers both too, per M6 below.
- [x] M2. Withdrawn. It timed the analysis thread's work per hop; the Q design
      has no analysis thread, and the detector's work per report runs inside
      the region M1 timed.
- [x] M3. Note latency and octave jumps for each consecutive-match count K.
      Recorded from measure-q parts 1 and 2 at 50-1,500 Hz
      (`measure-q/QLatency.cpp`, `measure-q/qlatency_output.txt`, run exit 0),
      reconfirmed at the ruled range, 50 to 5,000 Hz (Q-R), by
      `measure-q/report-range.md` Tables 1-2 (`measure-q/QLatencyRange.cpp`,
      `measure-q/qlatencyrange_output.txt`, run exit 0).
      Tapped: the real device output, `SynthRig::RunBlockAt()` then
      `Output()`, folded `0.5 * (l + r)` and fed to a fresh
      `pitch_detector(50 Hz, 5,000 Hz, 48,000, -30 dB)` sample by sample; a
      report is a detector call that returns true, stamped at its output
      sample. Latency: from the block where each of 20 steps raised the three
      VCO pitch knobs by a factor of 1.5 (target note 52) to the first
      confirmed note-on for note 52, over 40 alternating steps, one per
      quarter note, each landing 25 ms (1,200 frames) into the gate's open
      half. Counts: note changes and 12-semitone moves over 60 s at 48 kHz and
      128 frames.
      Positive controls (both ranges): control 0 (no steps, 3.5 s) found all
      77 reports inside the 25–250 ms landing window on note 45 at both block
      sizes, so a step lands on a steady note; a step to the same values
      confirmed 0 repeated notes; an added 10.000 ms delay moved the worst
      latency by 10.000 ms at both block sizes. Observed report cadence:
      960.01 samples, 20.000 ms, at both block sizes and both ranges -- set by
      the 50 Hz floor alone, which the range ruling does not change.

      K = 1 latency and the note/octave counts were re-measured directly at
      50-5,000 Hz; K = 2-4 below are the original 50-1,500 Hz figures, not
      re-run (K = 1 already governs, and Q's window and report cadence, so
      latency, come from the lowest frequency alone, unchanged at 50 Hz).

      | 48 kHz host block | K = 1 (50-5,000 Hz) | K = 2 (50-1,500 Hz) | K = 3 (50-1,500 Hz) | K = 4 (50-1,500 Hz) |
      |---|---|---|---|---|
      | 128 frames | 75.417 ms | 94.958 ms | 114.938 ms | 135.062 ms |
      | 256 frames | 94.271 ms | 113.312 ms | 133.312 ms | 153.625 ms |

      | Patch, 60 s | K = 1 changes / octave (50-5,000 Hz) | K = 2 (50-1,500 Hz) | K = 3 (50-1,500 Hz) | K = 4 (50-1,500 Hz) |
      |---|---|---|---|---|
      | default | 3 / 0 | 3 / 0 | 1 / 0 | 1 / 0 |
      | phase modulation at maximum | 677 / 114 | 547 / 46 | 430 / 38 | 348 / 31 |
      | ring modulation at maximum | 30 / 5 | 19 / 2 | 9 / 2 | 5 / 2 |
      | comb feedback at maximum | 3 / 0 | 3 / 0 | 1 / 0 | 1 / 0 |

      Ring-modulation-maximum's K = 1 count moved from 25/2 (50-1,500 Hz) to
      30/5 (50-5,000 Hz): the wider ceiling admits some of that patch's
      higher-frequency inharmonic content the narrower range excluded
      (report-range.md).

      Result: K = 1, the only count at or under 100 ms at both block sizes
      (coordinator ruling), reconfirmed at the ruled range. The latency is
      measured to the report's sample, which is the frame task 6 stamps the
      note at, so no block-read delay is added on the output timeline.

- [x] M6. Audio-worklet cost of the MIDI-out path in the real browser build.
      Recorded from `measure-m6/report.md` (harness
      `measure-m6/snapshot/frogg3rs/app/browser/e2e/m6_run.mjs`, builds
      `build_browser_output.txt` / `build_browser_output2.txt`, run
      `m6_run_output3.txt`).
      Tapped: each block's worklet callback time, the Emscripten clock pair
      around `ProcessAudioWorkletPlanarBlock()` that feeds
      `RecordCallbackMicros()` in Sheaf's
      `projects/synth/include/synth/browser/BrowserRuntime.hpp`, each block's
      own time with no averaging window. This clock resolves to 1 ms in this
      build: every recorded block time below is a multiple of 1000 µs, so a
      figure carries that granularity, not finer.
      Where: a copy of the real browser app built by
      `app/browser/build-browser.sh`, with Q reaching the compile through two
      `-I` flags added to the snapshot's copy of Sheaf's
      `build-browser-apps.mjs`, not through the manifest route task 2
      prescribes; served with COOP/COEP headers by
      `app/browser/serve-site.mjs`; driven by `@playwright/test` 1.60.0 from
      `app/browser/e2e/node_modules`, `chromium.launch` with
      `headless: true` and the one argument
      `--autoplay-policy=no-user-gesture-required`, against
      `chromium_headless_shell-1223` (`measure-m6/report.md`, "Toolchain" and
      "Method"; harness lines importing `chromium` from `@playwright/test`
      and passing that argument). Audio started at the AudioContext's
      own rate (48,000 Hz here) and 128-frame render quantum (2,666.7 µs
      budget), the default patch sounding for 2 s and then 5 s after the
      transport stops. With the additions: per sample, the mono fold, one
      `dsp::SingleEnvelopeFollower::Process` and one call of Q's pitch detector
      (cycfi::q::pitch_detector), and per block one `MidiSender::TryEnqueue` into the build's own
      sender, its worker started, all inside the timed region; without them,
      twice, as the baselines.
      Positive control: a fixed `sin()` loop inside the timed region: 200
      iterations per sample left the worst block unmoved (3000.0 µs, the same
      outlier every baseline shows); 2000 iterations moved it to 6000.0 µs
      (2.2500x budget) -- the instrument is live.
      A bug caught before reporting: the first build's additions pass
      recorded zero worklet callbacks, because `MidiSender::Start()` and the
      detector's construction were lazily triggered from inside the
      AudioWorklet thread on first use; `MidiSender::Start()` spawning a
      pthread there under this build's `-sPTHREAD_POOL_SIZE=1` silently kills
      the callback (the detector's construction spawns no thread and was not
      itself the cause). Fixed by
      moving both into `Prepare()`, which runs on the main thread before the
      worklet is created; rebuilt clean, reran clean (coordinator ruling,
      folded into the proposal and into task 6's check below).
      Deciding quantity: the worst block's callback time against 128 frames at
      the context's rate; it counts as a miss only when both baselines miss
      none, and a baseline that misses sends the setting back to be re-run.

      | Pass | worst block (µs) | frac. of budget | miss? |
      |---|---|---|---|
      | baseline1 | 3000.0 | 1.1250 | YES (block 0 only) |
      | baseline2 | 3000.0 | 1.1250 | YES (block 0 only) |
      | additions (Level+Pitch+MIDI-out) | 3000.0 | 1.1250 | YES (block 0 only) |
      | baseline1_rerun | 2000.0 | 0.7500 | no |
      | baseline2_rerun | 3000.0 | 1.1250 | YES (block 0 only) |

      Both original baselines missed once each, so per the framework the pair
      was re-run once; the second baseline's rerun missed again, so this setting (48
      kHz/128-frame) is one the app misses without MIDI out, at the same
      single event in every pass: block index 0, the worklet's first
      callback (verified against the raw per-block arrays; no other block in
      any pass reaches 3000 µs). The additions pass's own worst-block
      fraction, 1.1250x budget, is recorded beside it and gates nothing per
      the framework. `additionSampleCalls=345,600` (= 2,700 blocks x 128
      frames, one follower call and one detector call per sample, every
      sample, every block) and `additionBlockCalls=2,700` (one `TryEnqueue`
      per block) confirm the additions ran throughout, not just up to the
      miss.
      Result: no block anywhere in the additions run exceeds what the
      baselines already reach on their own. Browser Level and Pitch are shown
      feasible; the number is recorded and gates nothing. Task 3 lists Pitch
      on the browser build.
      This task appears in both `frogg3rs-midi-out` and `app-midi-out`; it was
      run once, in the `app-midi-out` snapshot, and its result recorded in
      both. No measurement remains open.

## Operator runs (gate delivery)

These need real hosts or real browser visibility. They run on this change's
builds, after implementation and before delivery. Each result is recorded
here, per host, and task 10 writes the manual from it.

- [ ] R2. Per-DAW routing, VST3 and AU, CC and notes. In Live 12, Logic
      (current), Reaper 7 and Bitwig 5, load this change's plugin build, set the
      MIDI button to Level and then to Pitch on the default patch, and route its
      MIDI output to a second track and to an IAC port. In Logic, use Instrument
      Output on a receiving track, then External Instrument or IAC for a
      hardware port.
      Confirms: the messages arrive, recorded per DAW, format and message
      type.
      Clears: nothing arrives, or only notes arrive.
      - Confirmed for a DAW and format: the manual names that pair as working.
      - Cleared for a DAW and format: the manual names it as not receiving MIDI
        out.
      - Cleared for every DAW of a format: plugin delivery stops and the
        operator rules whether that format ships.
      Gates plugin delivery.
- [ ] R7. Saved plugin state after the event bus is added. Save a project with
      the plugin built from `main` before this change, then reopen it with this
      change's build, in Live, Logic and Reaper.
      Confirms: state and routing restored.
      Clears: plugin reset, missing, or rescanned with state lost.
      - Confirmed in all three: plugin delivery may proceed.
      - Cleared in any: plugin delivery stops and the operator rules.
      Gates plugin delivery.
- [ ] R9. Standalone MIDI out end to end. Build the playable app with
      `./app/build-launcher.sh`, open `Frogg3rs.app`, open Controllers, choose
      an IAC port in the Audio to MIDI section, set Sends to Level and then
      to Pitch on the default patch, and watch the port in a MIDI monitor
      (or a receiving track in any DAW). Then choose None while a note is
      sounding, and relaunch with the port still chosen.
      Confirms: Control Changes on the set channel and CC number while Level
      is chosen, notes while Pitch is chosen, All Notes Off on every channel
      when None is chosen, and the port and Sends restored after relaunch.
      Clears: nothing arrives, the wrong message type arrives, a note stays
      held after None, or the setting is lost on relaunch.
      - Confirmed: standalone delivery may proceed.
      - Cleared: standalone delivery stops and the operator rules.
      Gates standalone delivery.
- [ ] R5. Browser cadence, throughput and offset. With Level on and audible,
      hide the tab for 3 minutes, in Chrome and in Firefox, using a build that
      logs drain intervals, messages and time per pass,
      `lateScheduledOutputCount`, and `port.send` time against due time and
      `AudioContext.outputLatency`.
      Confirms: Chrome keeps about 16 ms intervals while audible, and the late
      count stays 0.
      Clears: intervals of about 1 s, or a rising late count.
      - Confirmed: browser delivery may proceed; Firefox's figures are recorded
        and the manual states them.
      - Cleared: browser delivery stops and the operator rules.
      Gates browser delivery.
- [ ] R6. Firefox add-on flow. Open the site in Firefox with a MIDI device
      connected (IAC counts), accept the add-on, open Controllers, choose the
      port and set Sends to Level.
      Confirms: the add-on is offered and the output is received.
      Clears: no offer, or no output.
      - Confirmed: the manual names Firefox as working.
      - Cleared: the manual names Firefox as not receiving MIDI out, and the
        operator rules whether browser delivery waits.
      Gates browser delivery.

## Implementation

Browser tests here (task 6's new spec, and task 12's Pages e2e run) install
nothing. Both node_modules directories are symbolic links to the main
checkout's, each used only after its own `package.json` and
`package-lock.json` match the worktree's byte for byte, and both are removed
after the run:

- NEW `app/browser/e2e/node_modules` links to
  `/Users/diegoaguilar-canabal/Desktop/frogg3rs/app/browser/e2e/node_modules`,
  whose `package.json` and `package-lock.json` are byte-identical to this
  worktree's (md5 `2790b2cba897a5b7e262dffc072589e4` and
  `ade3d7bcfd025bca60604434fc87fff7` in both) and which holds
  `@playwright/test` 1.60.0. The specs run with `npx playwright test` from
  `app/browser/e2e`, whose `playwright.config.mjs` points
  `PLAYWRIGHT_BROWSERS_PATH` at `~/Library/Caches/ms-playwright`, which holds
  `chromium-1223` and `chromium_headless_shell-1223`, the revision
  `@playwright/test` 1.60.0 names.
- NEW `External/Sheaf/projects/synth/browser/node_modules` links to
  `/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/browser/node_modules`,
  whose `package.json` and `package-lock.json` are byte-identical to this
  worktree's `External/Sheaf` (md5 `a9181c03f5eedde2971ff2222b05cfaf` and
  `f62521f860aeda01f36fa6c686caf222` in both) and which holds
  `playwright-core` 1.62.0 and `typescript` 5.9.3, so
  `app/browser/build-browser.sh` skips its `npm install`.

Before linking either, the executor compares that pair's own md5 again; if
either pair differs or either path is missing, the run stops and reports
BLOCKED. Both links are removed after the run, and `git status --short` and
`git -C External/Sheaf status --short` then print nothing for them.

- [ ] 0. Rebase (operator ruling: midi-out is developed and pushed on its
      own branches, frogg3rs `midi-out` and Sheaf `app-midi-out`, and is
      rebased onto main as main stands when the operator says the other
      sessions' work on main is done; open changes nobody is working on are
      not waited for). Dispatched only when the coordinator relays that
      word. Run `git fetch`, rebase this worktree's branch
      onto `origin/main`, and rebase the `External/Sheaf` branch carrying
      `app-midi-out` onto Sheaf's upstream `main`; report both base commits.
      Then re-run task 2's build-site enumeration and every existence claim
      a task below makes about a named symbol, test or comment, and report
      each one the rebase changed; a task whose named symbol, test or
      comment no longer exists stops and reports rather than guessing its
      replacement.
      Check: `git log --oneline -1 origin/main` equals the reported frogg3rs
      base and `git merge-base --is-ancestor origin/main HEAD` exits 0.
- [ ] 1. Move the `External/Sheaf` pin to the commit carrying `app-midi-out`.
      Check: `git -C External/Sheaf grep -c "kAppMidiOutSinkIx" HEAD --
      projects/synth/include/synth/MidiController.hpp` prints a non-zero count,
      and `git -C External/Sheaf status` is clean. If the Sheaf change has not
      landed, report that and stop.
- [ ] 2. Add Cycfi Q and its infra dependency as submodules (the operator
      approved the download): NEW `External/q` from
      `https://github.com/cycfi/q.git` at
      `0920556c6eacb0091634310950f0c0ff5d66434a`, and NEW `External/infra` from
      `https://github.com/cycfi/infra.git` at
      `2dff97a4b107eced78e426152f5001a2331cb1cf`, both entered in
      `.gitmodules`. Q's license text ships in its own tree as the root
      `LICENSE` (Boost Software License 1.0). infra's tree at that commit holds
      no license file of its own: its files are `README.md`,
      `cmake/LICENSE_SFIZZ.md` (sfizz's BSD 2-Clause text for its cmake
      helpers), and headers whose opening comment says "Distributed under the
      MIT License" with Joel de Guzman's copyright (2016–2023). NEW
      `External/infra-LICENSE.txt` holds the MIT License text with that
      copyright line, so both license texts ship with the submodules.
      One include route, used by every build: the two directories
      `External/q/q_lib/include` and `External/infra/include`, added at each
      site where a build sets the app's include path. The sites are
      enumerated by this command, run from the repository root:
      ```
      git ls-files | grep -v '^External/' | grep -iE '(^|/)(Makefile|CMakeLists\.txt)$|\.sh$|\.mk$|\.cmake$|apps\.json$' | xargs grep -n -iE 'target_include_directories|includeDirs|EXTRA_APP_DIR|allowed-source-root|^[A-Z_]*CPPFLAGS *[+:]?='
      ```
      At `2255303` it prints eleven lines, and each is handled:
      - `app/Makefile`, the `CPPFLAGS` assignment: add both as `-I` flags.
        `TEST_CPPFLAGS` is built from `CPPFLAGS` and needs nothing; every app
        test binary, `app/check_no_juce.cpp` included, compiles with one of the two.
      - `app/standalone/CMakeLists.txt`, the include-directories call for
        `FroggersStandalone` (the Windows job in `desktop-release.yml`).
      - `app/vst/CMakeLists.txt`, the four include-directories calls, for
        `FroggersVst`, `FroggersVstSmokeTest`, `FroggersVstHostTests` and
        `FroggersVstEditorTest` (`vst-plugin.yml`).
      - `app/browser/frogg3rs-browser-apps.json`, `includeDirs`: add
        `../../../../../External/q/q_lib/include` and
        `../../../../../External/infra/include` (paths resolve from Sheaf's
        `projects/synth/browser`, as the existing `../../../../../app` does).
      - `app/browser/build-browser.sh`, the `--allowed-source-root`
        argument: add `--allowed-source-root "$REPO_ROOT/External/q/q_lib/include"`
        and `--allowed-source-root "$REPO_ROOT/External/infra/include"`.
        Sheaf's manifest reader rejects an include directory outside every
        allowed root, and its command line takes the option more than once.
        `app/browser/Makefile` and `pages.yml` build through this script.
      - `app/build-launcher.sh`, the `EXTRA_APP_DIR` argument: add
        `EXTRA_APP_INCLUDE_DIRS="$REPO_ROOT/External/q/q_lib/include $REPO_ROOT/External/infra/include"`,
        the variable `app-midi-out` adds to Sheaf's
        `apps/sheaf-patch/Makefile`, which today adds only
        `-I$(EXTRA_APP_DIR)` for the app (the macOS job in
        `desktop-release.yml` runs this script). Q's headers are not added to
        `EXTRA_APP_HEADERS`; they change only with their pins, and task 12
        removes the launcher's build directory (APP_BUILD_DIR in the script)
        before building.
      - `test/firmware/CMakeLists.txt`: no change; the firmware tests include
        no app header, shown by
        `grep -rlE 'include "(\.\./)?(FroggersAppCore|Froggers|FroggersRegistration)\.hpp"' --include='*.cpp' --include='*.hpp' --include='*.mm' app src test`,
        which at `2255303` lists only files under `app/`.
      A line the enumeration prints that this list does not handle stops the
      task and is reported. A comment written in `app/` that cites Q's
      source names the file and symbol with no line number, or carries Q's
      commit pin, since `check-citations-resolve` indexes only `app`,
      `External/Sheaf` and `src`. CI's `submodules: recursive` checkouts
      (`desktop-release.yml`, `vst-plugin.yml`, `pages.yml`) will also clone
      Q's own nested `infra` submodule (Q's `.gitmodules`) and infra's three
      `external/` submodules (`filesystem`, `string-view-lite`,
      `optional-lite`) under each copy; nothing includes them.
      Check: `git submodule status External/q External/infra` prints the two
      commits above; `External/q/LICENSE` and `External/infra-LICENSE.txt`
      exist; and task 12 builds every target above by the commands at the
      top of this section, once task 6 makes the app core include Q.
- [ ] 3. In `FroggersMidiCatalog()`, list two MIDI-out contents: id `level`,
      label "Level (CC)", Control Change; id `pitch`, label "Pitch (notes)",
      notes, listed only on builds that offer Pitch. Whether a build offers
      Pitch is NEW compile-time `kFroggersOffersPitch` in
      `app/FroggersMidiCatalog.hpp`: true for the standalone and the plugin,
      which share one value (M1), and for the browser build, told apart by
      `__EMSCRIPTEN__`, true as well (M6 recorded no missed deadline).
      Check: `device_defaults_are_valid_and_address_exactly_the_documented_controls`
      in `app/FroggersMidiCatalogTests.cpp` passes unchanged, and a new test in
      that file asserts the two contents, their ids, labels and kinds, with
      Pitch present on the standalone.
- [ ] 4. Settings intake: NEW `FroggersAppCore` setter for the MIDI-out
      setting (content id, channel 0 to 15, CC number, velocity), callable on
      the message thread. It resolves the content id against the catalog's
      MIDI-out contents on the message thread (Off, `level`, `pitch`; an
      unknown id is Off) and packs the content, channel, CC number and
      velocity (0 meaning follow the level) into one NEW
      `std::atomic<std::uint32_t>` pending slot, with a sentinel meaning
      nothing pending, in the shape of `pendingExternalAudioRouted_`: the
      audio thread exchanges it for the sentinel at the start of the next
      block and applies what it took. `Init` registers the setter with the
      app context's MIDI-out callback.
      Check: a new test in NEW `app/FroggersMidiOutTests.cpp`, beside
      `app/FroggersAudioRoutingTests.cpp`, asserts the Off scenario: ten
      seconds of the default patch, transport running, at the default setting
      append nothing. It is shown red against a build whose default content
      is Level, not against a tree without the setter, where nothing appends
      either. `app/Makefile` builds the file into NEW
      `app/build/froggers_midi_out_tests` with the same flags and libraries
      as `$(AUDIO_ROUTING_BIN)`, and runs it in `test`: the binary is added
      to `test`'s prerequisites and to its recipe, beside
      `$(AUDIO_ROUTING_BIN)`. Run: `nice make -C app -j2 test`, and, if it
      stops before this binary, `app/build/froggers_midi_out_tests` by path.
      Every later test in this file runs the same way.
- [ ] 5. Level: a NEW `dsp::SingleEnvelopeFollower` member for the MIDI out,
      given its sample rate in `PrepareToPlay`, fed the mono fold of each
      `RouteAudioSample()` result every sample. `ProcessBlock` already forms
      that fold twice per sample, for Record and for a mono device, each under
      its own condition; this task computes it once per sample, right after
      `RouteAudioSample()`, and the Record write, the mono device write, the
      follower and task 6's detector all read that one value. While Level is
      chosen, at each block's last frame the app appends one Control Change
      at that frame when `round(127 * level)` differs from the last value sent
      and at least `ceil(0.02 * sampleRate)` frames lie between the last
      Control Change's frame and this one (coordinator ruling: at most once
      per 20 ms, only on a changed value). The frame count since the last
      Control Change carries across blocks; a change inside the 20 ms is not
      queued, and is sent at the first later block end past the 20 ms if the
      value then still differs, stamped at that block's last frame.
      Check: new tests in the MIDI-out test file assert the Level
      requirement's four behaviour scenarios and the "Only the chosen content
      is sent" scenario's Level half; the 50-a-second scenario's test is shown
      red against a build without the 20 ms condition; every existing test in
      `app/FroggersAudioRoutingTests.cpp` passes unchanged (binary
      `$(AUDIO_ROUTING_BIN)`).
- [ ] 6. Pitch, as the operator's ruling states it (above, "The pitch path"):
      NEW member of `FroggersAppCore` holding Cycfi Q's pitch detector,
      including `<q/pitch/pitch_detector.hpp>`. Q's detector has no default
      constructor, so the member is a std::optional of Q's pitch_detector
      type, emplaced only in `PrepareToPlay` from the host rate as
      `pitch_detector(50 Hz, 5,000 Hz, sampleRate, -30 dB)` (Q-R), never
      lazily from `ProcessBlock` on first use, because construction allocates
      and allocation must stay off the audio thread; it runs alongside
      `MidiSender::Start()`, which M6 found must never run from inside the
      AudioWorklet's own thread, since spawning its worker's pthread there
      silently kills the worklet under `-sPTHREAD_POOL_SIZE=1` (coordinator
      ruling). `ProcessBlock` checks
      once per block whether the detector exists; a block before the first
      `PrepareToPlay` runs no detector and sends no note. NEW
      `FroggersAppCore::PitchDetectorConstructions` returns how many times
      the member has been emplaced, counted at the one emplace site; this
      task owns that seam and its test is its only reader.
      The detector is called once per sample on task 5's fold, after the
      follower. While Pitch is chosen, each sample is handled in this order:
      1. If a note is sounding and the follower's level is below 0.001
         (−60 dBFS), the note ends at this frame and the detector's `reset()`
         is called (coordinator ruling).
      2. Otherwise, if the call returned true, Q's get_frequency() is above
         0, and the follower's level is at or above 0.001, the note is
         `round(69 + 12 * log2(get_frequency() / 440))`; K = 1 (M3), so a
         note other than the sounding one, or any note when none sounds,
         becomes the sounding note at this frame. A note-on below 0.001 is
         not taken, because step 1 would end it on the next sample
         (coordinator ruling).
      3. A report whose get_frequency() is 0 (Q returns one before its
         first report periodic enough to set a frequency, and after
         `reset()`) changes nothing, and the note formula is not evaluated
         on it (coordinator ruling; measure-q's harness skipped these too).
      At the block's end, if the note sounding then differs from the one
      sounding at the block's start, the app appends the old note's note-off
      (if one was sounding) and the new note's note-on (if one is sounding),
      both stamped at the frame where the new state was set, the note-off
      first; the note-on's velocity is the Velocity field's value, or the
      follower's level at that frame times 127, rounded and held within 1 to
      127, when the field is Level. Nothing else is appended for Pitch in
      that block.
      Check, all in `app/FroggersMidiOutTests.cpp` unless named otherwise:
      - New tests assert the Pitch requirement's five scenarios and the
        "Only the chosen content is sent" scenario's Pitch half.
      - Latency. The test repeats M3's steps through the shipped path
        (`measure-q/QLatencyRange.cpp`'s schedule: one step per quarter note,
        1,200 frames into the gate's open half, 20 raises of the three VCO
        pitch knobs by the factor 1.5, target note 52) at 48 kHz with 128
        and with 256 frames, and asserts every raise's note-on for note 52
        is stamped at most 4,800 frames (100 ms) after the start of the block
        in which its step was set. It prints the worst latency at each block
        size beside measure-q's shipped-rule measurements (`measure-q/report-shipped-rule.md`
        Table 2: 75.417 ms at 128-frame, 94.271 ms at 256-frame). It is shown red at
        both block sizes against a build that stamps every note-on 4,800
        frames later, which puts every latency past 100 ms whatever the
        green worst was. If the green worst differs from the shipped-rule figure
        at either block size, the report says by how much.
      - Counts. The test renders 60 s of each of M3's four patches at 48 kHz
        and 128 frames with Pitch chosen (the default patch, and the three
        set as `measure-pitch/snapshot/frogg3rs/app/M3Harness.cpp` sets
        them), counts the note-ons and the note-ons 12 semitones from the
        previous note-on, and asserts these numbers from the shipped rule
        (`measure-q/report-shipped-rule.md` Table 1, render setup 48 kHz / 128-frame):
        default patch 3 note-ons and 0 octave jumps; phase modulation at maximum 677
        note-ons and 114 octave jumps; ring modulation at maximum 30 note-ons and 5 octave jumps;
        comb feedback at maximum 3 note-ons and 0 octave jumps. It also prints how many
        note-offs step 1 caused per patch. It is shown red against a build whose detector's highest
        frequency is 1,500 Hz, where measure-q recorded ring modulation at
        maximum as 25 and 2. If the green run fails, the test prints every
        count, the task stops, and the report gives the counts and step 1's
        note-off counts; the numbers in the test are not edited.
      - Silence. The test plays the default patch with Pitch chosen for 2 s,
        stops the transport, and renders up to 30 s more; it asserts that a
        note-off for the note sounding at the stop is appended after the
        stop and that no note-on follows it in the rest of the render. The
        measurement (`measure-q/report-shipped-rule.md` item 3) showed the
        default patch's level falls below 0.001 at 0.2823 s after the stop,
        triggering the note-off by step 1 well within the 30 s render window.
        It is shown red against a build without step 1. If no note-off comes within
        the 30 s, the test fails saying so, and the task stops and reports.
      - Construction. After `Init` and `PrepareToPlay` and before any
        `ProcessBlock`, `FroggersAppCore::PitchDetectorConstructions`
        reads 1; after 1,000 `ProcessBlock` calls it still reads 1. It is
        shown red against a build that emplaces the detector on the first
        `ProcessBlock` instead (the count reads 0 after `PrepareToPlay`).
      - The catalog's Pitch presence is asserted on each build by task 3's
        test (standalone), a new test in `app/vst/FroggersVstHostTests.cpp`
        (plugin, run by `ctest`), and a new spec in `app/browser/e2e/` that
        reads the Controllers page's Sends options (browser, run by
        `npx playwright test`), each against what task 3 records.
- [ ] 7. Note-off rules: a sounding note's note-off, on its own channel, is
      appended at frame 0 of the block where Pitch stops being the content or
      the channel changes, before any other message in that block; task 6's
      note rule then starts that block with no note sounding, so the block
      still appends at most two messages. When Pitch stops being the content,
      the detector's `reset()` is also called then (coordinator ruling).
      Check: new tests in `app/FroggersMidiOutTests.cpp` assert the "No note
      is left sounding" requirement's two scenarios, each shown red against a
      build without its note-off.
- [ ] 8. Plugin output: set `NEEDS_MIDI_OUTPUT TRUE` in `app/vst/CMakeLists.txt`;
      `producesMidi()` returns true; `processBlock` clears `midiMessages` before
      anything else, and after `engine_.ProcessBlock` adds each entry of the
      engine's MIDI-out list with `midiMessages.addEvent(bytes, 3, frame)`.
      Check: new tests in `app/vst/FroggersVstHostTests.cpp` assert the plugin
      output requirement's three scenarios, run by `ctest` as above.
- [ ] 9. Plugin surface and session. The Channel field displays 0 to 15, the
      same construct and numbering `app-midi-out`'s Controllers-page Channel
      field and the controller rows use (ruling, `app-midi-out`'s Q1). In
      plugin mode, a
      MIDI button after the IN button in the transport row cycling "MIDI:
      OFF" and then "MIDI: " followed by each MIDI-out content's id in
      capitals, in catalog order ("MIDI: LEVEL", and "MIDI: PITCH" where the
      catalog lists Pitch), the options built from `FroggersMidiCatalog()`'s
      MIDI-out contents rather than listed again here, and a
      NEW row directly after the transport row in the same parent, present
      only in plugin mode, holding three `TextField` nodes: Channel, CC, and
      Velocity, which accepts "Level" or a whole number 1 to 127. Each field
      checks an entry with `app-midi-out`'s `ParseAppMidiOutChannel`,
      `ParseAppMidiOutCcNumber` or `ParseAppMidiOutVelocity`; an entry they
      refuse leaves the stored value and the field shows it again, as sru-71
      rules on the Controllers page.
      The IN button is the first plugin-mode cycling picker and the MIDI
      button the second, so NEW `CyclingHostPicker` in
      `app/FroggersUiSurface.hpp` holds the labels, the selection, the
      label prefix and the changed callback, and builds the button label
      and the next selection; the IN button moves onto it
      (`SetInputOptions`, `SetInputSelectionChangedCallback`,
      `InputSelectButtonLabel` and the `kInputSelect` branch of
      `HandleAction` delegate to it) and the MIDI button is the second
      instance. The plugin applies the button and fields through task 4's
      setter. The two sites that write session extras (the constructor's
      seed and `PumpStatePersistence`) build the same object today; NEW
      `BuildSessionExtras` in `FroggersPluginProcessor.cpp`, taking the arena,
      writes the freeze latch, visible page, input selection and the new
      MIDI-out entry, and both sites call it. The restore path reads the
      MIDI-out entry beside `kInputSelectionKey`; a missing entry restores Off
      with channel 0, CC 16 and Level.
      The new `FroggersActions` constants for the button and the fields are
      the plugin host's own MIDI-out controls, not offered by the MIDI
      catalog: they are added to the exclusion list of
      `check-catalog-covers-screen-actions` in `app/Makefile`, and to the
      comment above it, beside `kInputSelect`, whose count ("the five")
      becomes the new total.
      If the plugin-mode layout test below fails with the new row in place,
      the task stops and reports that, with the overlapping or clipped node
      ids.
      Check: `plugin_mode_transport_row_thins_to_freeze_and_label_only` and
      `production_processor_surface_is_plugin_mode_with_bpm_display_only_while_host_tempo_engaged`
      in `app/vst/FroggersVstHostTests.cpp` are both updated to four children
      with the MIDI button fourth, reading "MIDI: OFF", and pass; a new test
      in that file builds the surface in plugin mode at
      `FroggersPageLayout::kDefaultWidth` by `kDefaultHeight` (the design size
      the plugin editor scales uniformly) and asserts that every node in the
      transport row and the new row lies fully inside its parent and the
      root, and that no two siblings in either row overlap; it is shown red
      against a row widened past the transport stack's width; new tests in
      that file assert the surface requirement's other scenarios, the one for
      "The standalone's setting does not switch the plugin on" shown red
      against a plugin whose engine enables MIDI-out routing, not against a
      tree without the change, where nothing appends either;
      `state_information_round_trips_the_input_selection_when_a_channel_is_chosen`
      passes unchanged; `make -C app check-catalog-covers-screen-actions`
      passes; every existing test in `app/FroggersSurfaceTests.cpp` passes
      unchanged.
- [ ] 10. MANUAL.md: an Audio to MIDI section under Audio configuration naming
      where it is set on each build; what Level sends, and that it sends at
      most 50 times a second and only when the value changes; that Pitch is
      monophonic, sending one note at a time, and detects fundamentals from
      50 Hz to 5,000 Hz, so a note below 50 Hz or above 5,000 Hz is not sent
      as its own pitch and is not tracked (Q-R: 5,000 Hz is the VCOs' own
      ceiling, 50 Hz is the floor the <=100 ms latency ruling allows); that
      the default patch's
      release tails change its output's pitch — for about the last 100 ms of
      each closed gate half the output is VCO2 alone at 220 Hz
      (`measure-floor/report.md`) — and the note changes and octave jumps in
      60 s that task 6's count test measured on the shipped code for that
      patch and for phase modulation at its maximum (measure-q recorded 3
      and 0, and 677 and 114), naming no note held through the tails, since
      no run recorded one; the worst note latency task 6's latency test
      printed for the shipped code at 128-frame and 256-frame buffers at
      48 kHz (measure-q recorded 75 ms and 94 ms), and that 128–256 frames
      are the buffer sizes it was measured at; that a note ends when the
      output falls quiet; the defaults; and only
      the hosts and browsers R2, R5 and R6 confirmed, and the standalone as R9
      confirmed it. The Plugin paragraph's MIDI sentence says the plugin now
      has a MIDI output; the Transport and tempo paragraph's "The plugin's own
      surface shows only Freeze" sentence names what the plugin's transport
      row holds: Freeze, its label, the IN button and the MIDI button, with
      the Channel, CC and Velocity row beneath; the Standalone paragraph's
      Sync page sentence names Send clock and Send transport and says they
      reach controller outputs and never the MIDI out port.
      Check: `grep -n -i "send clock" MANUAL.md` prints at least one line, and
      every host named in the section appears as confirmed in the R2, R5, R6
      or R9 results recorded above.
- [ ] 11. Comments made false by this change, in the files it touches: in
      `app/vst/FroggersPluginProcessor.hpp`, the class comment's "This class
      adds no note handling: the MIDI buffer is accepted and ignored" and the
      `acceptsMidi()` comment ("The buffer is ignored every block"); in
      `app/vst/FroggersPluginProcessor.cpp`, the comment opening
      `processBlock` ("The plugin does NOT consume notes: the buffer is
      accepted and ignored"); the `NEEDS_MIDI_INPUT` comment in
      `app/vst/CMakeLists.txt` ("processBlock() ignores the MIDI buffer
      entirely"); the header of `dsp::SingleEnvelopeFollower` in
      `app/dsp/EnvelopeFollowers.hpp`, which says it feeds the external-audio
      modulation source only, and `SingleEnvelopeFollower::SetSampleRate`'s
      comment, which calls its caller chain single. The `releaseResources`
      comment's "(zero input channels, by construction)", which the
      constructor's optional stereo input already contradicts (adj-M, S4), is
      `frogg3rs-o1-audit` task 4.9's, which lands first; after task 0's
      rebase this task leaves that comment as 4.9 wrote it, and if the
      sentence is still present, reports that rather than rewriting it. The
      comments task 9 makes false by moving the IN button's
      members into `CyclingHostPicker`: in `app/FroggersUiSurface.hpp`, the
      comment on `SetInputOptions` ("the ONLY entry point that can move
      inputSelection_ ... the ONLY other writer"), the comment on
      `InputSelectButtonLabel`, `AppendTransportRow`'s comment on the
      input-select label capture, the comment in `HandleAction`'s
      `kInputSelect` branch, and the comment on the `inputOptionLabels_`
      member; and in `app/vst/FroggersVstHostTests.cpp`, the comment citing
      "inputOptionLabels_'s own NSDMI comment, FroggersUiSurface.hpp". The
      comment above `check-catalog-covers-screen-actions` in `app/Makefile`
      is task 9's.
      Check: with each file's lines joined into paragraphs, a
      case-insensitive search for "ignored", "ignores the MIDI buffer" and
      "zero input channels" in `app/vst/` prints nothing that describes the
      MIDI buffer or the input channels; a search for `inputOptionLabels_`
      and `inputSelection_` across `app/` prints no comment naming a member
      that no longer exists; the follower's header names both of its users;
      and each rewritten comment says what the code does.
- [ ] 12. Run the full app suite and the host suites CI runs, reading the
      workflows for which those are, by the commands at the top of this
      section: `nice make -C app -j2 test`, then every test binary by path
      after it stops at the carried deadline tests; the plugin build and
      `ctest`; the launcher build from a removed launcher build directory,
      and the standalone CMake build; `app/browser/build-browser.sh`; and the
      `app/browser/e2e` suite by the route above.
      Check: pass and fail counts reported per binary and per spec as
      measured; every failure is either fixed here or shown to fail
      identically at the base commit task 0 reported.
