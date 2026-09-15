# The in-Process capacity assertion: design, blind spot, and where it compiles

`dsp::StereoDelay::Process` (`app/dsp/Delay.hpp`) now carries two compares
immediately after `timeL` and `timeR` are formed:

```cpp
#if defined(FROGGERS_DSP_CHECKS)
        assert(timeL <= capacitySeconds);
        assert(timeR <= capacitySeconds);
#endif
```

## What it is for

`ReadAt` wraps an over-capacity request modulo the line and returns a short,
wrong lag rather than failing. The three ordered bounds above the compares are
the only thing keeping the two reads inside the line. The parity suite's
capacity checks sample the reachable knob grid — a hand-picked list of `dtim`,
`dwid`, `widthBalance` and `dmod` values at two sample rates — so every gap in
that list is a regime a broken bound can pass through, and each such gap has
cost an audit round. The compares hold for every caller and every knob
combination the test binaries drive through `Process`, including the routing
suite's randomized storm test, which carries none of the operand tokens the
grid check sweeps.

## Named blind spot

The compares prove the emitted read lag stays in range. They do not prove the
bounds compute the right value: a bound that clamps too early (shortening a
read the line could have served) passes them exactly as a correct one does.
Only the grid check's expected-versus-measured comparison catches that
direction, and only at the points it samples. Neither instrument proves the
formula; together they bound the output from both sides at the sampled points
and from one side everywhere else.

## Why a bare `assert` is the wrong instrument here

The plan asks for a debug-only check. `assert` is compiled out by `NDEBUG`,
and three of the four shipping builds define it; the fourth does not.

The macOS app, built by `app/build-launcher.sh` through
`External/Sheaf/projects/synth/runtime/juce_build.mk`:

```
$ sed -n 118,119p External/Sheaf/projects/synth/runtime/juce_build.mk
CPPFLAGS := -I$(SYNTH_ROOT)/include -I$(SYNTH_ROOT)/apps/miniapp -I$(SYNTH_ROOT)/juce -I$(SYNTH_ROOT)/runtime -I$(JUCE_DIR)/modules \
	-DNDEBUG \
```

The Windows standalone and the VST, CMake Release builds (CMake's Release
configuration adds `-DNDEBUG`):

```
$ grep -n "CMAKE_BUILD_TYPE" .github/workflows/desktop-release.yml .github/workflows/vst-plugin.yml
.github/workflows/desktop-release.yml:74:        run: cmake -S app/standalone -B app/standalone/build -DCMAKE_BUILD_TYPE=Release
.github/workflows/vst-plugin.yml:50:        run: cmake -S app/vst -B app/vst/build -DCMAKE_BUILD_TYPE=Release
.github/workflows/vst-plugin.yml:135:        run: cmake -S app/vst -B app/vst/build -DCMAKE_BUILD_TYPE=Release
```

The browser build, `app/browser/build-browser.sh` running
`External/Sheaf/projects/synth/browser/src/build-browser-apps.mjs`, whose
compiler arguments are:

```
$ sed -n '/function commonCompilerArgs/,/^}/p' External/Sheaf/projects/synth/browser/src/build-browser-apps.mjs
function commonCompilerArgs(browserRoot) {
  return [
    "-I", path.resolve(browserRoot, "..", "include"),
    "-I", path.join(browserRoot, "cpp"),
    "-std=c++20", "-O2",
    "-pthread",
    ...
```

No `NDEBUG`. A bare `assert` in `Process` would be live in the shipped
browser build and would abort the audio worklet on a violation, where the
plan's intent is that no shipping build carries the check at all. The
manifest the browser build reads (`app/browser/frogg3rs-browser-apps.json`)
carries include directories only, no defines, so the browser build cannot be
given `NDEBUG` from this repository without changing the Sheaf submodule.

So the compares are gated on `FROGGERS_DSP_CHECKS`, defined in one place:

```
$ grep -n "FROGGERS_DSP_CHECKS" app/Makefile app/dsp/Delay.hpp
app/Makefile:22:# FROGGERS_DSP_CHECKS compiles the DSP headers' invariant assertions in.
app/Makefile:27:CPPFLAGS := -I$(SHEAF_SYNTH_DIR)/include -DFROGGERS_DSP_CHECKS
app/dsp/Delay.hpp:708:        // Compiled only where FROGGERS_DSP_CHECKS is defined -- app/Makefile
app/dsp/Delay.hpp:713:#if defined(FROGGERS_DSP_CHECKS)
```

Every binary `app/Makefile` builds is a test, a check, or the unshipped
skeleton, and every one of them takes `CPPFLAGS`, so the compares are live in
all twelve test binaries and in none of the four shipping builds.

## Seen in passing, out of scope

`app/dsp/DspMath.hpp` already carries a bare `assert(cyclesPerSample > 0.0f)`.
By the trace above it is live in the shipped browser build. It guards a
programming error rather than a knob value, and nothing in this change
touches it; it is recorded here so the next reader of that file knows the
browser build does not strip it.

## Proof that it holds and that it fires

Recorded by the verification run, below.

### Run 1 — unmodified, with `FROGGERS_DSP_CHECKS`

No diff (`Delay.hpp` unmodified from `.orig`).

```
$ cd app && rm -f build/froggers_dsp_parity_tests && nice -n 15 clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -std=c++20 -Wall -Wextra -Wpedantic -O2 -DFROGGERS_DSP_CHECKS FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests && ./build/froggers_dsp_parity_tests > build/run.log 2>&1; echo EXIT=$?
```

```
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
190/190 tests passed
EXIT=0
```

Every compare in the suite, including the width-spread grid sweep, holds with the compares compiled in.

### Run 2 — width bound removed, with `FROGGERS_DSP_CHECKS`

```
$ diff -u dsp/Delay.hpp.orig dsp/Delay.hpp
--- dsp/Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -696,7 +696,7 @@
         // base and modulation terms above have not already spent.
         const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
         const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
-        const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
+        const float widthSpread = widthSpreadRaw;
         float timeL = std::max(0.001f, baseSeconds + modSeconds);
         float timeR = std::max(0.001f, baseSeconds + modSeconds + widthSpread);
         // ReadAt wraps an over-capacity request modulo the line and returns
```

```
$ cd app && rm -f build/froggers_dsp_parity_tests && nice -n 15 clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -std=c++20 -Wall -Wextra -Wpedantic -O2 -DFROGGERS_DSP_CHECKS FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests && ./build/froggers_dsp_parity_tests > build/run.log 2>&1; echo EXIT=$?
```

Last lines of `build/run.log` before the abort (stdout/stderr interleave as written by the process; the assert fires mid-line of the preceding buffered stdout write):

```
  [peak trim duty]     liveness: steady-tone reduction trim=2.12321903 dB cancelled=9.14658699 dB; composite fundamental power trim=0.194488988 cancelled=0.199117849
  [peak trim duty]   Peak gain=1
  [peak trim duty]     harmonic  branch trim=-23.7658998 (+/-2.93359744e-05)  cancelled=-22.5179539 (+/-2.83724249e-05)   composite trim=-23.3259477 (+/-2.85184233e-05)  cancelled=-22.2169456 (+/-2.60234322e-05)  dB
  [peak trim duty]     intermod  branch trim=-29.8843124 (+/-0)  cancelled=-24.165402 (+/-0)   composite trim=-29.8858534 (+/-0)  cancelled=-24.6059053 (+/-0)  dB
  [peak trim duty]     reduction trim mean=1.12430577 range=1.79078402 (+/-6.07304123e-05) slope=60.3713519 dB/s   cancelled mean=8.1125941 range=8.03726432 (+/-9.82657783e-06) slope=160.523867 dB/s
  [peak trim duty]     liveness: steady-tone reduction trim=2.17407905Assertion failed: (timeR <= capacitySeconds), function Process, file Delay.hpp, line 715.
 dB cancelled=11.5555848 dB; composite fundamental power trim=0.192445244 cancelled=0.197898923
[PASS] filter_bank_peak_trim_removal_distortion_intermodulation_and_limiter_pumping
```

```
EXIT=134
```

The binary aborts before reaching its own summary line; no `N/190 tests passed` line appears in `build/run.log`.

### Run 3 — same mutation, without `FROGGERS_DSP_CHECKS`

Same diff as run 2 (width bound removed).

```
$ cd app && rm -f build/froggers_dsp_parity_tests && nice -n 15 clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -std=c++20 -Wall -Wextra -Wpedantic -O2 FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests && ./build/froggers_dsp_parity_tests > build/run.log 2>&1; echo EXIT=$?
```

Failing tests (by name, via the `REQUIRE` path, no abort):

```
[FAIL] stereo_delay_width_spread_never_reads_past_the_line_capacity: FroggersDspParityTests.cpp:10527 requirement failed: lagSamples > static_cast<long long>(capacity) - 100
[FAIL] stereo_delay_width_spread_bound_holds_across_the_reachable_grid: FroggersDspParityTests.cpp:10713 requirement failed: static_cast<float>(lagSamples) (1640) ~= expectedLagSamples (96000), eps=5
```

Summary line:

```
188/190 tests passed
EXIT=1
```

Compiling without `FROGGERS_DSP_CHECKS` compiles the two `assert`s out; the same broken bound is caught only by the grid sweep's own `REQUIRE`, reported by test name, with the process running to completion.

### Run 4 — modulation bound removed, with `FROGGERS_DSP_CHECKS`

```
$ diff -u dsp/Delay.hpp.orig dsp/Delay.hpp
--- dsp/Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -688,7 +688,7 @@
         const float baseSeconds = std::min(baseSecondsRaw, capacitySeconds);
         const float modSecondsRaw = std::sin(lfoPhase) * p.dmod * baseSeconds * 0.08f;
         const float maxModSeconds = capacitySeconds - baseSeconds;
-        const float modSeconds = std::min(modSecondsRaw, maxModSeconds);
+        const float modSeconds = modSecondsRaw;
         // 0.35f scaled by widthBalance (SetWidthBalance, an identity map on
         // the [0,1] knob and never exceeding 1.0f) sets how much of
         // baseSeconds this term asks for; the min() below is what actually
```

```
$ cd app && rm -f build/froggers_dsp_parity_tests && nice -n 15 clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -std=c++20 -Wall -Wextra -Wpedantic -O2 -DFROGGERS_DSP_CHECKS FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests && ./build/froggers_dsp_parity_tests > build/run.log 2>&1; echo EXIT=$?
```

Abort line:

```
Assertion failed: (timeL <= capacitySeconds), function Process, file Delay.hpp, line 714.
```

```
EXIT=134
```

`timeL` is the left read the modulation term pushes over capacity first, matching the expectation that the left-side compare (line 714) fires ahead of the right-side one (line 715) for this mutation.

### Restore

```
$ cp dsp/Delay.hpp.orig dsp/Delay.hpp && md5 dsp/Delay.hpp
MD5 (dsp/Delay.hpp) = 2434131fb88dc72296151d39297e5b83
```

Matches the pre-run md5; `diff -u dsp/Delay.hpp.orig dsp/Delay.hpp` is empty.
