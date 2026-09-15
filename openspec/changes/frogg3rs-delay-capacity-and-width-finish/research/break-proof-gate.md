Repair 4 — the mutation gate, `app/check_delay_capacity_break_proofs.py`.

## Shadowing mechanism: the brief's `-I` ordering does not work, fixed

The brief's stated mechanism was "the mutated dsp/ shadows the real one
because it comes first on the include path", confirmed once by a `-H`/`-M`
listing. That confirmation is exactly what caught a real defect: it does not
shadow anything, because `FroggersDspParityTests.cpp`'s `#include "dsp/Delay.hpp"`
is a QUOTED include, and quoted-include search always checks the including
file's OWN DIRECTORY first, before any `-I` path -- no flag on the plain `-I`
form overrides that.

Literal confirmation, first attempt (mutated dsp/ first on `-I`, real source
file compiled in place):
```
$ clang++ -H -I<app>/build/break-proofs/modulation-bound-removed -I<app> -I<Sheaf include> -DFROGGERS_DSP_CHECKS -std=c++20 -fsyntax-only <app>/FroggersDspParityTests.cpp 2>&1 | grep Delay.hpp
. /Users/diegoaguilar-canabal/Desktop/frogg3rs/app/dsp/Delay.hpp
```
The REAL header resolved, not the mutated one. Running the gate as first
written against the real tree confirmed this is not a corner case -- every
one of the six breaks compiled the unmutated header and ran to completion:
```
check-delay-capacity-break-proofs: FAIL  modulation-bound-removed  exit=0  41.34s
check-delay-capacity-break-proofs: FAIL  width-bound-removed  exit=0  41.44s
check-delay-capacity-break-proofs: FAIL  modulation-dropped-from-width-budget  exit=0  41.44s
check-delay-capacity-break-proofs: FAIL  wrap-admits-capacity  exit=0  42.46s
check-delay-capacity-break-proofs: FAIL  lines-allocated-short  exit=0  45.13s
check-delay-capacity-break-proofs: FAIL  off-grid-width-balance-headroom  exit=0  44.41s
check-delay-capacity-break-proofs: total 256.22s across 6 breaks
check-delay-capacity-break-proofs: FAIL
make: *** [check-delay-capacity-break-proofs] Error 1
```
All six "stayed green" for the same reason: none of them ever compiled the
mutated header.

Fix: `run_one_break` now also copies each file in `<dsp-test-sources>` into
`build/break-proofs/<name>/` at its path relative to `app-dir` (so
`FroggersDspParityTests.cpp`'s copy sits directly beside the mutated `dsp/`),
and compiles those copies instead of the originals. The copy's own directory
is what the quoted include's first search step finds, so this does not
depend on include-search-order flags at all. The `-I<break-root>` flag stays
as a harmless second layer. Re-confirmed by the same `-H` listing against
the regenerated copy:
```
$ clang++ -H -I<app>/build/break-proofs/modulation-bound-removed -I<app> -I<Sheaf include> -DFROGGERS_DSP_CHECKS -std=c++20 -fsyntax-only <app>/build/break-proofs/modulation-bound-removed/FroggersDspParityTests.cpp 2>&1 | grep Delay.hpp
. /Users/diegoaguilar-canabal/Desktop/frogg3rs/app/build/break-proofs/modulation-bound-removed/dsp/Delay.hpp
```
The mutated copy now resolves. This is documented in the script's own header
comment as well, not just here.

## Real run, fixed script

```
$ cd app && nice -n 10 make check-delay-capacity-break-proofs; echo MAKE_EXIT=$?
check-delay-capacity-break-proofs: PASS  modulation-bound-removed  exit=-6  42.70s
check-delay-capacity-break-proofs: PASS  width-bound-removed  exit=-6  43.39s
check-delay-capacity-break-proofs: PASS  modulation-dropped-from-width-budget  exit=-6  50.46s
check-delay-capacity-break-proofs: PASS  wrap-admits-capacity  exit=-6  4.55s
check-delay-capacity-break-proofs: PASS  lines-allocated-short  exit=-6  4.88s
check-delay-capacity-break-proofs: PASS  off-grid-width-balance-headroom  exit=-6  54.84s
check-delay-capacity-break-proofs: total 200.82s across 6 breaks
check-delay-capacity-break-proofs: OK -- every break was rejected
MAKE_EXIT=0
```
Six PASS, exit 0, matching expectation. `exit=-6` is Python's
`subprocess.run` reporting SIGABRT (signal 6) as a negative return code --
the same abort the DSP parity suite's `assert()` raises everywhere else in
this change.

## Proof the gate is live

Scratch copy of `app/` (fresh `rsync --exclude build`); grid test's final
`REQUIRE_NEAR(static_cast<float>(lagSamples), expectedLagSamples, tolerance);`
changed to use tolerance `static_cast<float>(capacity)`. Ran the gate script
directly against that copy (same `cxx`/flags, `-I<scratch-app>` in place of
the real `app`, real Sheaf include dir unchanged):
```
check-delay-capacity-break-proofs: PASS  modulation-bound-removed  exit=-6  53.97s
check-delay-capacity-break-proofs: PASS  width-bound-removed  exit=-6  46.69s
check-delay-capacity-break-proofs: PASS  modulation-dropped-from-width-budget  exit=-6  46.08s
check-delay-capacity-break-proofs: PASS  wrap-admits-capacity  exit=-6  4.72s
check-delay-capacity-break-proofs: PASS  lines-allocated-short  exit=-6  3.96s
check-delay-capacity-break-proofs: PASS  off-grid-width-balance-headroom  exit=-6  40.47s
check-delay-capacity-break-proofs: total 195.91s across 6 breaks
check-delay-capacity-break-proofs: OK -- every break was rejected
```

Expected (per brief): at least one break prints FAIL `stayed green`. Observed:
all six still PASS -- none stayed green. This is a finding, not a fabricated
result: recorded as measured, not adjusted toward the expected outcome.

Why: every one of the six breaks in the table is caught by a compile-gated
`assert()` inside `StereoDelay::Process`/`ReadAt`/`WriteSample` (the abort
each PASS row's `exit=-6` shows) -- and each fires DURING the sample-by-
sample measurement loop that runs BEFORE the grid test's own
`REQUIRE_NEAR(..., tolerance)` line is ever reached. Weakening that one
downstream numerical-tolerance comparison cannot change whether the process
already aborted earlier in the same iteration; the mutated line in the test
file is dead code with respect to these six breaks by the time any of them
would fire. Widening the tolerance would only matter for a break that
produces a wrong-but-in-bounds lag value (something REQUIRE_NEAR alone would
have to catch) -- none of the six entries in this table are that shape; all
six are out-of-bounds/index-domain violations the compares catch before
REQUIRE_NEAR runs. The adversarial control this specific mutation was meant
to demonstrate does not apply to this break table as specified; it would
need a break whose failure mode is a silently-wrong-but-in-range value, which
is not one of the six the brief names.

## `check_no_planning_history.py`

```
$ python3 app/check_no_planning_history.py app
check-no-planning-history: OK - 59 files, no planning-history references
```

## Seventh break: `capacity-headroom-shortens-reads`

The six breaks above all put a read outside capacity or past the line's
allocated length, so every one aborts inside a compile-gated compare in
`StereoDelay::Process`/`ReadAt` (`FROGGERS_DSP_CHECKS`) before the parity
suite's own `REQUIRE` lines run -- the prior finding above names this
directly: none of the six is "a break whose failure mode is a
silently-wrong-but-in-range value." `capacity-headroom-shortens-reads`
closes that gap. It mutates `capacitySeconds` to
`static_cast<float>(capacity) / sampleRate - 0.01f`, which keeps every read
inside capacity (so the compile-gated compares never fire) while shortening
how far a read can reach by 10 ms near the boundary. It is caught only by
the suite's expected-versus-measured assertions, and it exists to prove
those assertions still reject a break the compares are structurally blind
to.

### Run 1 -- real gate, real rebuild

```
$ cd app && nice -n 10 make check-delay-capacity-break-proofs; echo MAKE_EXIT=$?
check-delay-capacity-break-proofs: PASS  modulation-bound-removed  exit=-6  43.46s
check-delay-capacity-break-proofs: PASS  width-bound-removed  exit=-6  42.89s
check-delay-capacity-break-proofs: PASS  modulation-dropped-from-width-budget  exit=-6  42.10s
check-delay-capacity-break-proofs: PASS  wrap-admits-capacity  exit=-6  4.39s
check-delay-capacity-break-proofs: PASS  lines-allocated-short  exit=-6  4.10s
check-delay-capacity-break-proofs: PASS  off-grid-width-balance-headroom  exit=-6  41.27s
check-delay-capacity-break-proofs: PASS  capacity-headroom-shortens-reads  exit=1  40.09s
check-delay-capacity-break-proofs: total 218.29s across 7 breaks
check-delay-capacity-break-proofs: OK -- every break was rejected
MAKE_EXIT=0
```
Seven PASS, exit 0. The new entry's `exit=1` (not `-6`) confirms it is
rejected by a different mechanism than the other six: a `REQUIRE` failure
returning from `main`, not an `assert()` raising SIGABRT.

Rebuild of the real (unmutated) parity binary, forcing a real recompile
first:
```
$ rm -f build/froggers_dsp_parity_tests && nice -n 10 make -j2 <app>/build/froggers_dsp_parity_tests && ./build/froggers_dsp_parity_tests | tail -3
c++ -I<app> -I<Sheaf include> -DFROGGERS_DSP_CHECKS -std=c++20 -Wall -Wextra -Wpedantic -O2 <app>/FroggersDspParityTests.cpp -o <app>/build/froggers_dsp_parity_tests
  [random knob walk] points=2000 sample rates=44100/48000/88200/96000
[PASS] stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks
191/191 tests passed
```

### Run 2 -- which assertions reject it

Scratch copy of `app/` (`rsync -a --exclude build`), only the new break
applied to its `dsp/Delay.hpp`, compiled with the Makefile's flags:
```
$ clang++ -I<scratch-app> -I<Sheaf include> -std=c++20 -Wall -Wextra -Wpedantic -O2 -DFROGGERS_DSP_CHECKS FroggersDspParityTests.cpp -o build/froggers_dsp_parity_tests
$ ./build/froggers_dsp_parity_tests; echo RUN_EXIT=$?
...
[FAIL] stereo_delay_width_spread_never_reads_past_the_line_capacity: FroggersDspParityTests.cpp:10534 requirement failed: lagSamples > static_cast<long long>(capacity) - 100
...
[FAIL] stereo_delay_width_spread_bound_holds_across_the_reachable_grid: FroggersDspParityTests.cpp:10720 requirement failed: static_cast<float>(lagSamples) (95040) ~= expectedLagSamples (96000), eps=5
189/191 tests passed
RUN_EXIT=1
```
No `Assertion failed` abort anywhere in the output -- the binary runs to
completion and returns 1 from `main` because two `REQUIRE`s failed:
`stereo_delay_width_spread_never_reads_past_the_line_capacity` and
`stereo_delay_width_spread_bound_holds_across_the_reachable_grid`, exactly
the two named in the brief.

### Run 3 -- gate stays live when those two assertions are weakened

Fresh scratch copy of `app/`. In `FroggersDspParityTests.cpp` only:
`stereo_delay_width_spread_never_reads_past_the_line_capacity`'s
`REQUIRE_TRUE(lagSamples > static_cast<long long>(capacity) - 100);` becomes
`REQUIRE_TRUE(lagSamples > static_cast<long long>(capacity) - 100 || true);`,
and `stereo_delay_width_spread_bound_holds_across_the_reachable_grid`'s final
`REQUIRE_NEAR(static_cast<float>(lagSamples), expectedLagSamples, tolerance);`
becomes
`REQUIRE_NEAR(static_cast<float>(lagSamples), expectedLagSamples, static_cast<float>(capacity));`.
Ran the gate script directly against that copy, same `cxx`/flags the
Makefile passes, `-I<scratch-app>` substituted for the real `app` (real
Sheaf include dir unchanged, since only `app/` was copied):
```
check-delay-capacity-break-proofs: PASS  modulation-bound-removed  exit=-6  44.34s
check-delay-capacity-break-proofs: PASS  width-bound-removed  exit=-6  49.53s
check-delay-capacity-break-proofs: PASS  modulation-dropped-from-width-budget  exit=-6  48.78s
check-delay-capacity-break-proofs: PASS  wrap-admits-capacity  exit=-6  4.24s
check-delay-capacity-break-proofs: PASS  lines-allocated-short  exit=-6  3.91s
check-delay-capacity-break-proofs: PASS  off-grid-width-balance-headroom  exit=-6  42.67s
check-delay-capacity-break-proofs: FAIL  capacity-headroom-shortens-reads  exit=0  44.99s
check-delay-capacity-break-proofs:   reason: stayed green
check-delay-capacity-break-proofs: total 238.45s across 7 breaks
check-delay-capacity-break-proofs: FAIL
SCRIPT_EXIT=1
```
The six compile-gated breaks stay green regardless (weakening these two
`REQUIRE`s cannot touch an abort that already fired earlier in the same
iteration). `capacity-headroom-shortens-reads` flips to FAIL `stayed green`
and the script exits 1, which is the positive control this entry exists to
supply: with the two expected-versus-measured assertions weakened, nothing
else in the suite catches this break, and the gate correctly reports that.
