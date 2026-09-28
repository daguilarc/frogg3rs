The executor's deliverable is a report; code changes are a side effect of it.
A conflict between this file and the code or `proposal.md` is reported and
stops that task. The new test is shown to fail with its production change
reverted, by the executor, before the task is reported done, and the report
names the break used. Build and run one binary at a time, `-j2` under
`nice`. Nothing is installed. No push.

- [ ] 1. `FroggersPluginProcessor` gains a private
      `juce::VST3ClientExtensions`-derived member (its `getPluginHasMainInput()`
      returning false) and overrides `getVST3ClientExtensions()` to return a
      pointer to it. No other method changes: the bus's declared layout,
      default-disabled state, and every existing read of it by index and
      channel count are untouched. The AU wrapper has no main/aux concept, so
      no AU-side change exists to make.
      Check: `app/vst/FroggersPluginProcessor.cpp`/`.hpp` build; the new
      override is the only production diff.
- [ ] 2. NEW host test in `app/vst/FroggersVstHostTests.cpp` asserting the
      processor's `getVST3ClientExtensions()` returns non-null and that
      pointer's `getPluginHasMainInput()` is false. Named break: removing the
      override (or reverting `getPluginHasMainInput()` to its default `true`)
      turns the assertion red; the break is named beside the test and is not
      run by this task — a verifier runs it separately.
      Check: `FroggersVstHostTests` built and run once
      (`cmake -S app/vst -B app/vst/build -DCMAKE_BUILD_TYPE=Release`, JUCE
      at `~/JUCE`; `nice -n 10 cmake --build app/vst/build --config Release
      --target FroggersVstHostTests -- -j2`; run the built binary directly,
      not through ctest), green, once. No rerun, no ctest, no bundle build.
- [ ] 3. MANUAL.md: state, in the plugin section, that the input bus appears
      to the host as a sidechain/aux input a track can be routed into, not a
      required main input. Gated on the operator's Live or Bitwig test
      confirming the bus actually appears as a routable sidechain target and
      that a routed signal reaches External Audio/External EF (`proposal.md`'s
      "Decision waiting"). Not written by this task; written only once that
      run has reported, so the manual never states a routing behavior no one
      has driven through a real host.
      Check: not yet delivered, pending the operator's run.
- [ ] 4. Delivery. Two commits: the OpenSpec change (this directory), then the
      code and test (task 1 and 2's diff). No push: the branch stays on
      `.claude/worktrees/plugin-sidechain` (`plugin-sidechain`) until task 3's
      operator run reports and a full `make test` plus the CI-run host suites
      pass, per `proposal.md`'s Delivery section.
