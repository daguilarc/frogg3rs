# Which of the three capacity bounds the parity suite actually enforces

Build environment: scratch copy of `app/` at
`/private/tmp/claude-501/-Users-diegoaguilar-canabal-Desktop/68105815-8b90-42de-aa6a-51f093984b11/scratchpad/breakproof/app/`,
a byte-identical copy of the real repository's `app/` (minus `build/`) used so builds and mutations never touch the real tree.

## Positive control

```
cd <scratch app dir>
rm -f build/froggers_dsp_parity_tests
nice -n 15 clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -std=c++20 -Wall -Wextra -Wpedantic -O2 FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests && ./build/froggers_dsp_parity_tests > build/run.log 2>&1; echo EXIT=$?
```

```
EXIT=0
190/190 tests passed
```

Each mutation below starts from a fresh `cp dsp/Delay.hpp.orig dsp/Delay.hpp` (the unmutated snapshot) before editing, and each is rebuilt with the same `rm -f build/froggers_dsp_parity_tests && clang++ ... && ./build/froggers_dsp_parity_tests` sequence as the positive control, so a no-op make or stale binary cannot report a prior mutation's result.

## Mutation 2: base bound removed

`dsp/Delay.hpp`, inside `StereoDelay::Process`, line 687:

```
diff -u dsp/Delay.hpp.orig dsp/Delay.hpp
--- dsp/Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -684,7 +684,7 @@
         // needs no headroom: the write for the current sample happens after
         // the read within this same call, so the sample from exactly one
         // full lap ago is still present when the read for that lap runs.
-        const float baseSeconds = std::min(baseSecondsRaw, capacitySeconds);
+        const float baseSeconds = baseSecondsRaw;
         const float modSecondsRaw = std::sin(lfoPhase) * p.dmod * baseSeconds * 0.08f;
         const float maxModSeconds = capacitySeconds - baseSeconds;
         const float modSeconds = std::min(modSecondsRaw, maxModSeconds);
```

```
rm -f build/froggers_dsp_parity_tests
nice -n 15 clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -std=c++20 -Wall -Wextra -Wpedantic -O2 FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests && ./build/froggers_dsp_parity_tests > build/run.mut2.log 2>&1; echo EXIT=$?
```

```
EXIT=0
190/190 tests passed
```

This confirms, rather than refutes, the prior claim: removing the base bound alone produces no test failure. `maxModSeconds = capacitySeconds - baseSeconds` goes negative once `baseSecondsRaw` exceeds `capacitySeconds`, and `modSeconds = std::min(modSecondsRaw, maxModSeconds)` then clamps `modSeconds` to that negative value whenever `modSecondsRaw` is not already below it, which telescopes `baseSeconds + modSeconds` back down toward `capacitySeconds` even though `baseSeconds` itself was never clamped. No grid row in the suite drives `baseSecondsRaw` far enough past `capacitySeconds`, combined with a `dmod`/`dwid` setting, to open a gap the downstream bounds fail to close. The suite is silent on an unclamped `baseSeconds` in isolation.

## Mutation 3: modulation bound removed

`dsp/Delay.hpp`, line 689:

```
diff -u dsp/Delay.hpp.orig dsp/Delay.hpp
--- dsp/Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -687,7 +687,7 @@
         const float baseSeconds = std::min(baseSecondsRaw, capacitySeconds);
         const float modSecondsRaw = std::sin(lfoPhase) * p.dmod * baseSeconds * 0.08f;
         const float maxModSeconds = capacitySeconds - baseSeconds;
-        const float modSeconds = std::min(modSecondsRaw, maxModSeconds);
+        const float modSeconds = modSecondsRaw;
```

```
rm -f build/froggers_dsp_parity_tests
nice -n 15 clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -std=c++20 -Wall -Wextra -Wpedantic -O2 FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests && ./build/froggers_dsp_parity_tests > build/run.mut3.log 2>&1; echo EXIT=$?
```

```
./dsp/Delay.hpp:689:21: warning: unused variable 'maxModSeconds' [-Wunused-variable]
EXIT=1
189/190 tests passed
[FAIL] stereo_delay_width_spread_bound_holds_across_the_reachable_grid: FroggersDspParityTests.cpp:10713 requirement failed: static_cast<float>(lagSamples) (967) ~= expectedLagSamples (96000), eps=6221.17
```

Failing grid row (the widened `dtim=0.9` point, printed as `dtim=0.899999976`):

```
  [width spread grid sweep] dtim=0.899999976 dwid=0 widthBalance=1 dmod=1 expected=96000 measured=967
[FAIL] stereo_delay_width_spread_bound_holds_across_the_reachable_grid: FroggersDspParityTests.cpp:10713 requirement failed: static_cast<float>(lagSamples) (967) ~= expectedLagSamples (96000), eps=6221.17
```

`stereo_delay_width_spread_bound_holds_across_the_reachable_grid` goes red. With `modSeconds` unclamped, `timeL = baseSeconds + modSeconds` alone can exceed `capacitySeconds` even at `dwid=0` where the width term is zero and cannot compensate; `ReadAt` wraps the over-capacity read and the measured lag drops far below the expected 96000 samples.

## Mutation 4: width bound removed

`dsp/Delay.hpp`, line 698:

```
diff -u dsp/Delay.hpp.orig dsp/Delay.hpp
--- dsp/Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -695,7 +695,7 @@
         // base and modulation terms above have not already spent.
         const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
         const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
-        const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
+        const float widthSpread = widthSpreadRaw;
```

```
rm -f build/froggers_dsp_parity_tests
nice -n 15 clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -std=c++20 -Wall -Wextra -Wpedantic -O2 FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests && ./build/froggers_dsp_parity_tests > build/run.mut4.log 2>&1; echo EXIT=$?
```

```
./dsp/Delay.hpp:697:21: warning: unused variable 'maxSpreadSeconds' [-Wunused-variable]
EXIT=1
188/190 tests passed
[FAIL] stereo_delay_width_spread_never_reads_past_the_line_capacity: FroggersDspParityTests.cpp:10527 requirement failed: lagSamples > static_cast<long long>(capacity) - 100
[FAIL] stereo_delay_width_spread_bound_holds_across_the_reachable_grid: FroggersDspParityTests.cpp:10713 requirement failed: static_cast<float>(lagSamples) (1640) ~= expectedLagSamples (96000), eps=5
```

Two tests go red. `stereo_delay_width_spread_never_reads_past_the_line_capacity` fails directly, at its own dedicated check. `stereo_delay_width_spread_bound_holds_across_the_reachable_grid` fails at the same widened `dtim=0.9` grid point as mutations 1 and 3:

```
  [width spread grid sweep] dtim=0.899999976 dwid=0.5 widthBalance=0.5 dmod=0 expected=96000 measured=1640
[FAIL] stereo_delay_width_spread_bound_holds_across_the_reachable_grid: FroggersDspParityTests.cpp:10713 requirement failed: static_cast<float>(lagSamples) (1640) ~= expectedLagSamples (96000), eps=5
```

With `widthSpread` unclamped, `timeR = baseSeconds + modSeconds + widthSpread` exceeds `capacitySeconds` whenever `dwid` and `widthBalance` are both non-zero, independent of `dmod`; this row fails at `dmod=0`, unlike mutations 1 and 3 which needed `dmod=1` to expose the gap, because the width term here carries no modulation dependence to gate it.

## Cleanup and md5 confirmation

`dsp/Delay.hpp` restored from `dsp/Delay.hpp.orig` after mutation 4:

```
cp dsp/Delay.hpp.orig dsp/Delay.hpp
md5 dsp/Delay.hpp
MD5 (dsp/Delay.hpp) = 65a3ad2c430e1dc28b8a2c3b6d6b52a2
```

Matches the required `65a3ad2c430e1dc28b8a2c3b6d6b52a2`.

Real repository, read-only for this work except for the two files in this `research/` directory:

```
md5 app/dsp/Delay.hpp app/FroggersDspParityTests.cpp   # at start
MD5 (app/dsp/Delay.hpp) = 65a3ad2c430e1dc28b8a2c3b6d6b52a2
MD5 (app/FroggersDspParityTests.cpp) = 79337cca69141816e64344026609a154
```

These mutations run against `dsp/Delay.hpp` as it stood before the
compile-gated capacity assertion inside `Process` was added (that later
version is `2434131fb88dc72296151d39297e5b83`; see
`process-capacity-assertion.md`). That is the right baseline for these
proofs: they show the parity suite's own checks going red on each break. With
the assertion compiled in, a broken bound aborts the binary at the first
violating `Process` call instead, which is the assertion's own proof and is
recorded there.
