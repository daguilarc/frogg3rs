Repair 1 — index-domain compares in `app/dsp/Delay.hpp`.

Change landed: `ReadAt` gets two compares right after `idx1` is computed,
comparing the indices actually used against the vector actually indexed
(the existing `Process` compares are in seconds against `capacitySeconds`,
a different domain). `WriteSample` gets the same treatment for `writePos`.
Both compile-gated on `FROGGERS_DSP_CHECKS`, matching the existing pair in
`Process`.

Real suite, landed header:
```
$ cd app && rm -f build/froggers_dsp_parity_tests && nice -n 10 make -j2 "/Users/diegoaguilar-canabal/Desktop/frogg3rs/app/build/froggers_dsp_parity_tests" > build/compile.log 2>&1; echo BUILD_EXIT=$?
BUILD_EXIT=0
$ ./build/froggers_dsp_parity_tests > build/run.log 2>&1; echo EXIT=$?
EXIT=0
$ tail -1 build/run.log
190/190 tests passed
```
Expected 190/190, EXIT=0 — matches.

Note on the build command: the brief's literal `make -j2 build/froggers_dsp_parity_tests`
does not match any target — `DSP_TEST_BIN := $(BUILD_DIR)/froggers_dsp_parity_tests`
and `BUILD_DIR := $(APP_DIR)/build` is an absolute path (`APP_DIR` is
`abspath`), so the relative spelling gets `No rule to make target`. Ran make
against the absolute path instead; same target, same recipe, same binary.

Proof 1 — scratch copy, fresh rsync of the landed `app/` (`--exclude build`),
`WrapIndex`'s `while (idx >= capacity)` -> `while (idx > capacity)`:
```
--- Delay.hpp.orig
+++ Delay.hpp
@@ -1018,7 +1018,7 @@
     size_t WrapIndex(size_t idx) const
     {
-        while (idx >= capacity)
+        while (idx > capacity)
```
Compile (`clang++ -I. -I<Sheaf include> -std=c++20 -Wall -Wextra -Wpedantic
-O2 -DFROGGERS_DSP_CHECKS FroggersDspParityTests.cpp -o ...`): exit 0.
Run: `EXIT=134`.
```
[phase sweep] RMS at Phase 0.00/0.25/0.50/0.75/1.00 = 0.33198 / 0.328764 / 0.334879 / Assertion failed: (idx1 < line.size()), function ReadAt, file Delay.hpp, line 1004.
```
Expected: abort naming `idx1 < line.size()` (or `idx0`, whichever fires
first). Observed: `idx1`. Matches. Test running at the moment of abort:
`drive_phase_sweep_moves_output_meaningfully_across_each_quarter_of_travel`
(the assertion fires mid-line of its own diagnostic print, before that
test's own `[PASS]`/`[FAIL]` line).

Proof 2 — separate fresh scratch copy, `SetSampleRate`'s
`lineL.assign(capacity, 0.0f); lineR.assign(capacity, 0.0f);` ->
`lineL.assign(capacity - 64, 0.0f); lineR.assign(capacity - 64, 0.0f);`:
```
--- Delay.hpp.orig
+++ Delay.hpp
@@ -548,8 +548,8 @@
-        lineL.assign(capacity, 0.0f);
-        lineR.assign(capacity, 0.0f);
+        lineL.assign(capacity - 64, 0.0f);
+        lineR.assign(capacity - 64, 0.0f);
```
Compile: exit 0. Run: `EXIT=134`.
```
[phase sweep] RMS at Phase 0.00/0.25/0.50/0.75/1.00 = 0.33198 / 0.328764 / 0.334879 / Assertion failed: (idx0 < line.size()), function ReadAt, file Delay.hpp, line 1003.
```
Expected: abort naming one of the three new compares. Observed: `idx0 <
line.size()`. Matches.

Note on log ordering: in both raw logs the assertion text appears physically
above several `[PASS]` lines that logically precede it in test order (stdout
is fully buffered when redirected to a file; stderr's assert message is
unbuffered and lands in the file first, then a late stdout flush catches up
the buffered tail). The genuinely last `[PASS]` line printed in both logs is
`map_rows_to_delay_params_passes_through_all_rows_0_to_8_directly` — not
identified further here since the brief does not ask which test was
running for this repair (unlike Repair 2's proof, which does).
