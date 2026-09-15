Repair 2 — random-walk capacity test in `app/FroggersDspParityTests.cpp`.

Landed: an `#if !defined(FROGGERS_DSP_CHECKS) #error` guard right after the
includes, and a new `TEST_CASE(stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks)`
placed immediately after `stereo_delay_width_spread_bound_holds_across_the_reachable_grid`,
per the brief's exact body: four sample rates, an LCG (`20260914u`,
`lcg = lcg * 1664525u + 1013904223u; return (lcg >> 8) / 16777216.0f;`), 500
points per rate, `dtim`/`dwid`/`dmod` and `SetWidthBalance` each drawn from
`next01()`, `dsnd = dmix = 1.0f`, 1000 `Process()` calls per point with input
`next01() - 0.5f`, `REQUIRE_TRUE(allFinite)` per point, `REQUIRE_TRUE(points == 2000)`
after the loops, one diagnostic `std::cout` line.

Real suite, landed code:
```
$ cd app && rm -f build/froggers_dsp_parity_tests && nice -n 10 make -j2 "/Users/diegoaguilar-canabal/Desktop/frogg3rs/app/build/froggers_dsp_parity_tests" > build/compile2.log 2>&1; echo BUILD_EXIT=$?
BUILD_EXIT=0   (clean: no warnings from -Wall -Wextra -Wpedantic)
$ time ./build/froggers_dsp_parity_tests > build/run2.log 2>&1; echo EXIT=$?
EXIT=0
$ tail -3 build/run2.log
  [random knob walk] points=2000 sample rates=44100/48000/88200/96000
[PASS] stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks
191/191 tests passed
$ cat build/time2.log
./build/froggers_dsp_parity_tests > build/run2.log 2>&1  36.98s user 0.25s system 98% cpu 37.707 total
```
Expected 191/191, EXIT=0 — matches. New test's printed line:
`[random knob walk] points=2000 sample rates=44100/48000/88200/96000`.
Suite wall time: 37.707s total (36.98s user).

Proof — scratch copy (fresh rsync of the landed `app/`, `--exclude build`),
`Process`'s `maxSpreadSeconds` mutated:
```
--- Delay.hpp.orig
+++ Delay.hpp
@@ -695,7 +695,7 @@
         const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;
-        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);
+        const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds + (-2.0f) * widthBalance * (widthBalance - 0.5f) * (widthBalance - 1.0f));
```
Compile: exit 0. Run: `EXIT=134`.
```
Assertion failed: (timeR <= capacitySeconds), function Process, file Delay.hpp, line 715.
```
Expected assertion text — matches exactly.

Which test was running: the cubic term `(-2.0f)*wb*(wb-0.5f)*(wb-1.0f)` is
exactly zero at `wb` in `{0, 0.5, 1}` — precisely the grid test's own `wbs[]`
values — so the grid test cannot see this defect by construction and runs
to completion. The log shows all 190 pre-existing tests print `[PASS]`,
including `stereo_delay_width_spread_bound_holds_across_the_reachable_grid`
itself (`grep -c '^\[PASS\]' build/run_p2.log` = 190), and the new test's own
`[random knob walk]` print / `[PASS]` line never appears. The new test,
`stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks`, is
the one running when the abort fired, as expected.

(The raw log's stderr line for the assertion appears interleaved mid-string
inside one of the grid test's own buffered diagnostic prints — fully-
buffered stdout vs. unbuffered stderr writing to the same redirected fd.
File-offset position of that text is not report order; the PASS/points
tally above is the reliable signal, not the offset the assertion text
happens to land at.)

## Sweep gate: comments stripped

Added `strip_comments(text)` to `app/check_delay_capacity_parameters_are_swept.py`:
a character walk removing `//`-to-EOL and `/* ... */` spans, leaving string
and character literals intact and every newline in place. Applied once per
body inside `capacity_surface_bodies` (the stripped body is what both the
`capacity`/`SetWidthBalance` membership test and, via the generator's
yielded value, `literal_values_for` in `main()` see -- one application, not
two). Docstring's bodies-are-found paragraph gets one added sentence:
comment text does not count, so a literal written in a comment cannot stand
in for a swept value.

Proof (a) -- real tree:
```
$ python3 app/check_delay_capacity_parameters_are_swept.py app; echo EXIT=$?
check-delay-capacity-parameters-are-swept: capacity-surface checks found: stereo_delay_width_spread_never_reads_past_the_line_capacity, stereo_delay_width_spread_bound_is_inert_away_from_capacity, stereo_delay_width_spread_bound_holds_across_the_reachable_grid
  sample rate: [48000.0, 96000.0]
  Delay time (p.dtim): [0.0, 0.3, 0.5, 0.9, 0.99, 1.0]
  Stereo width (p.dwid): [0.0, 0.5, 0.75, 1.0]
  Width balance: [0.0, 0.5, 1.0]
  Mod depth (p.dmod): [0.0, 1.0]
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
EXIT=0
```
Every list matches the brief's exact expected lists. OK.

Proof (b) -- scratch copy of `app/FroggersDspParityTests.cpp` only (script
takes `<app-dir>` as an argument, so a directory holding just the mutated
file is sufficient), grid test's `const float dmods[] = {0.0f, 1.0f};` ->
`const float dmods[] = {0.0f};  // was dmod = 1.0f as well`:
```
$ python3 app/check_delay_capacity_parameters_are_swept.py <scratch-app-dir>; echo EXIT=$?
  Mod depth (p.dmod): [0.0]
check-delay-capacity-parameters-are-swept: FAIL
  Mod depth (p.dmod) is held at one value (0.0) across every capacity-surface check in FroggersDspParityTests.cpp -- the capacity bound's dependence on it is unexercised
EXIT=1
```
Expected: gate prints FAIL naming Mod depth. Matches -- the comment-only
`1.0f` does not count.
