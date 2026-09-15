# Widened grid sweep catches a width bound with the modulation term deleted

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

## Mutation: width bound loses its modulation term

`dsp/Delay.hpp`, inside `StereoDelay::Process`, around line 697:

```
diff -u dsp/Delay.hpp.orig dsp/Delay.hpp
--- dsp/Delay.hpp.orig
+++ dsp/Delay.hpp
@@ -694,7 +694,7 @@
         // keeps it in bounds, against whatever of the line's capacity the
         // base and modulation terms above have not already spent.
         const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
-        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
+        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds);
         const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);
```

Rebuild and run:

```
rm -f build/froggers_dsp_parity_tests
nice -n 15 clang++ -I. -I/Users/diegoaguilar-canabal/Desktop/frogg3rs/External/Sheaf/projects/synth/include -std=c++20 -Wall -Wextra -Wpedantic -O2 FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests && ./build/froggers_dsp_parity_tests > build/run.mut1.log 2>&1; echo EXIT=$?
```

```
EXIT=1
189/190 tests passed
[FAIL] stereo_delay_width_spread_bound_holds_across_the_reachable_grid: FroggersDspParityTests.cpp:10713 requirement failed: static_cast<float>(lagSamples) (6216) ~= expectedLagSamples (96000), eps=5
```

The widened grid sweep goes red for this mutation. It fails on the first row where the deleted `modSeconds` term matters, at the widened `dtim=0.9` grid point (printed as `dtim=0.899999976`, the nearest representable `float` to 0.9):

```
  [width spread grid sweep] dtim=0.899999976 dwid=0.5 widthBalance=0.5 dmod=0 expected=96000 measured=96000
  [width spread grid sweep] dtim=0.899999976 dwid=0.5 widthBalance=0.5 dmod=1 expected=96000 measured=6216
[FAIL] stereo_delay_width_spread_bound_holds_across_the_reachable_grid: FroggersDspParityTests.cpp:10713 requirement failed: static_cast<float>(lagSamples) (6216) ~= expectedLagSamples (96000), eps=5
```

At `dmod=0` the row still passes (96000 == 96000): with no modulation, `modSeconds` is 0 and the deleted term subtracts nothing, so the mutation is invisible. At `dmod=1`, with `dtim=0.9` driving a large `baseSeconds` and the LFO active, `modSeconds` is non-zero; the mutated `maxSpreadSeconds` now overstates the width budget by that amount, `widthSpread` is clamped too loosely, and the read trajectory (`timeR = baseSeconds + modSeconds + widthSpread`) exceeds `capacitySeconds`. `ReadAt` wraps the over-capacity request modulo the line length rather than failing, so the measured lag collapses from the expected 96000 samples to 6216 — a read from the wrong place in the line, not a bounds error the framework catches on its own.

The test itself stops at the first failing `REQUIRE_NEAR` in this nested-loop sweep, so no later grid rows print for this run; the `dtim=1` rows (`modSeconds` re-derives to the same magnitude at that grid point) are not reached once the `dtim=0.9` row throws. The full unmutated row set for `dtim=0.899999976` and `dtim=1`, for reference, is the positive-control log's `[width spread grid sweep]` output; every row there passes (`measured == expected` in each).

## Cleanup and md5 confirmation

`dsp/Delay.hpp` restored from `dsp/Delay.hpp.orig` in the scratch copy after the last of the four mutations (see `capacity-bound-break-proofs.md` for mutations 2-4); final scratch md5:

```
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
