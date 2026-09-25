The executor's deliverable is a report; code changes are a side effect of it.
A conflict between this file and the code, the story or another document is
reported and stops that task. Every new assertion is shown to fail with its
production change reverted, by the executor, before the task is reported done.
A test that is red before the task starts is reported, never edited. Build and
run one binary at a time; C++ and JUCE builds run at `-j2` under `nice`.
Nothing is installed. "Sheaf N.N" names a task in `app-operator-runs`.

The thresholds are the operator's established framework: an audio block over
its duration at 48 kHz (1,333,333 ns at 64 frames,
2,666,667 ns at 128, 5,333,333 ns at 256) or a UI, message or worker tick over
33,333,333 ns is a finding; a cost no larger than the spread of its own three
repetitions is dropped; anything between is reported.

## 1. Operator runs

- [ ] 1.1 BUG-04, RUN-04: log the POSIX call pthread_self() in the standalone's
      `MidiInHandler::handleIncomingMidiMessage` while a MIDI Fighter Twister
      and an APC40 mkII both stream. Confirms: two thread ids. Clears: one.
      Steps SWX-01, CTL-18, CTL-19. Fix if confirmed: Sheaf 1.2.
- [ ] 1.2 BUG-05, RUN-05: log the thread id in the plugin's `prepareToPlay` in
      Logic, Ableton Live and Reaper, beside the message thread's id.
      Confirms: any host calls it off the message thread. Clears: all three
      call it on the message thread. Steps PLG-03, PLG-05. Fix if confirmed:
      2.1.
- [ ] 1.3 BUG-08, RUN-08: in Live's MIDI Map mode drag one encoder in the
      plugin window; in Logic with the AU in Touch mode drag one encoder.
      Confirms: Live lists no parameter, or Logic writes no automation or
      releases it at the wrong point. Clears: Live lists it and Logic writes
      it for the drag. Steps PLG-08, PLG-09, PLG-18. Fix if confirmed: 2.2.
- [ ] 1.4 BUG-14, RUN-09: on the standalone with Send Clock on, count
      psynch_cvsignal on the audio thread for 60 s with `sudo dtrace`.
      Confirms: the count is close to the number of enqueued clock and
      transport events. Clears: the count is 0. Steps TRN-01, TRN-03, TRN-07,
      SYN-06. Fix if confirmed: Sheaf 1.1.
- [ ] 1.5 OPT-04 on x86, RUN-13's second half: log MXCSR inside the
      standalone's audio callback on an x86 machine. FPCR.FZ was measured not
      set on arm64, reading 0x0 (`fz_bit_set=false`) on the main thread and in
      five audio callbacks of the default output device (2026-09-23). If
      flush-to-zero is set on x86, the x86 half is closed. If not, time a
      decaying reverb and delay tail on x86 against the 256-frame deadline.
      Steps TRN-03, DLY-03, REV-03, REV-09. Implementation if a finding:
      Sheaf 1.3.
- [ ] 1.6 OPT-05, RUN-15: `pluginval --randomise-block-sizes` and offline
      bounces in Logic, Ableton Live and Reaper, logging each block size above
      the prepared size. The first 512-frame block after a 256 prepare was
      measured at 1,284,042 to 1,360,917 ns (12.0-12.8% of the 10,666,667 ns
      512-frame budget; 24.1-25.5% of the 5,333,333 ns 256-frame budget),
      against a same-run control of 1,204,667 to 1,238,750 ns for 512-frame
      blocks after a 512 prepare (2026-09-23). The item is a finding only if a
      supported host exceeds the prepared size and that block misses its
      deadline. Steps PLG-05, PLG-07. Implementation if a finding: 2.3.
- [ ] 1.7 OPT-26, RUN-28: with 8 × 128 rows and a Twister row on the browser,
      type five digits into one field with a real Twister connected, count
      `drain-midi-output` messages and note any visible stall. The agent half
      measured rebuilds of 89,334 to 156,500 ns (at most 0.47% of the tick)
      and 966 MIDI output messages per rebuild (23.6% of the sender's queue),
      with 7 Generic controllers of 128 rows and one Twister (2026-09-23);
      Reported. If this run is a finding, the implementation is to be
      written, as a new task in `app-operator-runs`.
- [ ] 1.8 OPT-28, RUN-30: time `SaveRuntimeConfiguration` on the operator's
      own launched app with its own `config.json`
      (`~/Library/Application Support/Sheaf/synth/sheaf-patch/config` on
      macOS). The file did not exist on the audit's Mac. The agent half
      measured `SaveRuntimeConfiguration` at 3,326,875 to 5,581,917 ns at
      8 × 128 controllers and 973,042 to 1,454,000 ns at 32 controllers of 8
      rows (at most 16.7% of the tick, 2026-09-23); Reported. If this run is
      a finding, the implementation is to be written, as a new task in
      `app-operator-runs`, after the operator rules on the one behaviour it
      changes.
- [ ] 1.9 OPT-30, RUN-33: page faults and the sidebar's deadline figure over a
      10-minute take on the standalone, with and without a prior take. The
      agent-runnable half of this cost -- `ArmRecording` zero-filling the
      whole capture buffer, measured at 27,403,958 to 39,616,500 ns on the
      first arm and 28,426,791 to 34,343,917 ns on a re-arm (2026-09-23) -- is
      already fixed: the capture buffer is allocated uninitialised and zeroed
      in parts across message ticks (`frogg3rs-presses-on-the-bus` task 2.4).
      If this run still finds a page-fault or deadline finding, its
      implementation is to be written.
- [ ] 1.10 Read the manual's Connect messages, offline port and Reset
      subsections against the Controllers page and against Reset All and
      Reset Page on a parameter page and in a modulation view. A sentence the
      app contradicts is reported as a defect against MANUAL.md.

## 2. Fixes gated on the runs

- [ ] 2.1 BUG-05's fix, only if 1.2 confirms. Scope: the plugin only.
      `Engine::Prepare` calls `FroggersAppCore::PrepareToPlay` for every host;
      the standalone and browser call it on the message thread and have no
      plugin timer, and `transport_survives_audio_device_reprepare_after_play`
      in `app/FroggersSurfaceTests.cpp` covers them. NEW
      `FroggersAppCore::SetReassertTransportFromPrepare(bool)`, default true;
      when false, `PrepareToPlay` does not push `MessageIn::Start` onto
      `uiBus`. NEW `FroggersAppCore::DesiredTransportRunning`() reads
      `desiredTransportRunning_` with acquire. `FroggersPluginProcessor`'s
      constructor sets it false, and `FroggersPluginProcessor::prepareToPlay`,
      after `engine_.Prepare`, stores `PendingTransportEdge::kStart` in
      `pendingTransportEdge_` when `DesiredTransportRunning()` is true; the
      timer's existing dispatch then presses Play on the message thread.
      What changes for the player: in the plugin, after a host re-prepares
      during playback, the transport restarts at the plugin's next timer
      tick, up to 33 ms later, instead of at the next block.
      Check: the plugin host tests in `app/vst/FroggersVstHostTests.cpp` stay
      green; NEW `prepare_off_the_message_thread_restarts_the_transport_from_the_timer`
      there calls `prepareToPlay` from a second thread with the transport
      desired running, asserts nothing was pushed onto `uiBus` from that
      thread, runs `timerCallback()`, and asserts the transport runs; 1.2's
      run repeated shows the push on the timer's thread in all three hosts.
- [ ] 2.2 BUG-08's fix, only if 1.3 confirms. First find where a drag starts
      and ends in the plugin editor's portable surface; if the editor has no
      such signal, stop and report. Otherwise call the host parameter's
      `beginChangeGesture` at the drag's start and `endChangeGesture` at its
      end around the `setValueNotifyingHost` calls
      `FroggersPluginProcessor::PumpHostParameterBridge` already makes.
      Check: 1.3's run repeated clears.
- [ ] 2.3 OPT-05's fix, only if 1.6 is a finding: `processBlock`'s scratch
      buffer is sized in `prepareToPlay` to the largest block the hosts in 1.6
      sent. Equal: the output, sample for sample.
- [ ] 2.4 OPT-30's fix, only as 1.9 says.

## 3. Delivery

- [ ] 3.1 In this order: mark every scenario of the `froggers-vst-host` delta
      with the check that backs it; in `External/Sheaf`, on the branch
      `app-operator-runs` made from the commit frogg3rs pins, archive
      `app-operator-runs` with `openspec archive app-operator-runs` and
      commit; `git -C External/Sheaf push fork app-operator-runs` (remote
      `fork` = `git@github.com:daguilarc/Sheaf.git`, added with `git remote
      add` if `git remote get-url fork` fails); open the next sequential pull
      request with `gh pr create --repo jvictor0/Sheaf --base main --head
      daguilarc:app-operator-runs`; stage the `External/Sheaf` pin at the
      pushed `fork/app-operator-runs` tip; archive this change with
      `openspec archive frogg3rs-operator-runs` and commit; run the full
      frogg3rs gate; `git push origin HEAD:main`, never a pull request. A
      fix after an archive is a new commit, never a second archive.
