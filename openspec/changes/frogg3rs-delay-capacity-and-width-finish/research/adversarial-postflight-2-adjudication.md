# Adversarial postflight 2 -- adjudication

Adjudicator did not participate in the audit that produced findings U/V/W.
Every claim below was independently reproduced against a scratch copy of
`app/` (`/private/tmp/.../scratchpad/adjudicate2*/app`), built with the same
flags `app/Makefile`'s `$(DSP_TEST_BIN)` rule uses
(`-I$(APP_DIR) -I$(SHEAF)/include -DFROGGERS_DSP_CHECKS -std=c++20 -Wall
-Wextra -Wpedantic -O2`, `clang++`, `nice -n 15`, one build at a time, binary
`rm`'d before each rebuild). The real repository was read-only throughout
except for writing this file; nothing under `openspec/` besides this file was
opened.

## What each check claims (from its own text)

- **Suite** (`FroggersDspParityTests.cpp` header comment): "parity test suite
  for the DSP port. Each TEST_CASE pins one ported unit to its cited Froggers
  formula."
- **Compile-gated compares** (`dsp/Delay.hpp:708-718`, at the `Process()`
  compare site): "ReadAt wraps an over-capacity request modulo the line and
  returns a short, wrong lag rather than failing, so the three bounds above
  are the only thing keeping these two reads inside what the line holds...
  Compiled only where FROGGERS_DSP_CHECKS is defined... A bare assert would
  not give that: the browser build compiles without NDEBUG, so it would abort
  the audio worklet on a violation instead of playing a wrong lag." The
  `ReadAt`-site comment adds: "a WrapIndex that admits `capacity` or a line
  allocated shorter than `capacity` fires here rather than reading past the
  end."
- **`check_delay_capacity_parameters_are_swept.py`** (module docstring):
  "fails when the Delay capacity-surface checks all pin some shipping
  parameter to one value... it collects every literal value that check
  family assigns to it... and fails if the union across every
  capacity-surface check body still has only one distinct value... Comment
  text does not count, so a literal written in a comment cannot stand in for
  a swept value." It is an explicitly mechanical, text-level literal scan --
  it never claims to verify that a value is actually exercised at runtime.
- **`check_delay_capacity_break_proofs.py`** (module docstring): "fails when
  the Delay capacity compares... stop rejecting the family of defects they
  exist to catch... This mutates dsp/Delay.hpp one way at a time, in an
  isolated copy, compiles the real DSP parity suite against each mutation,
  and requires the compiled binary to abort." Its claimed job is scoped to
  "the family of defects they exist to catch" -- the seven tabled mutations
  -- not to defects of arbitrary shape.
- **`app/Makefile`'s `test:` target**: prerequisite order is `... check-no-
  planning-history check-artifact-symbols-resolve check-delay-capacity-
  parameters-are-swept check-delay-capacity-break-proofs $(TEST_BIN)
  $(MONO_VALIDATION_BIN) $(DSP_TEST_BIN) ...`. Both Delay-capacity gates
  precede `$(DSP_TEST_BIN)` (the parity suite) in that list.

## Mechanism check: does `make` stop at the first failed prerequisite?

Reproduced with a minimal synthetic Makefile mirroring the real `test:`
target's shape (`test: gate-a gate-b suite-c`, `gate-b` exits 1):

```
$ make test
RAN_GATE_A
RAN_GATE_B
make: *** [gate-b] Error 1
MAKE_EXIT=2
```

`suite-c` (the `$(DSP_TEST_BIN)` stand-in) and the `test` recipe body never
ran. `app/Makefile` has no `.NOTPARALLEL`/`MAKEFLAGS`/`-j` override, so
default sequential, stop-on-first-failure `make` semantics apply to the real
`test:` target as well: a failing `check-delay-capacity-break-proofs`
prerequisite stops `make test` before `$(DSP_TEST_BIN)` is even built.

## Control (unmodified copy)

- Suite: `191/191 tests passed`, exit 0.
- `check_delay_capacity_parameters_are_swept.py`: OK, exit 0 (sample rate
  [48000, 96000]; dtim 6 values; dwid 4 values; Width balance 3 values; dmod
  2 values).
- `check_delay_capacity_break_proofs.py`: 7/7 PASS ("OK -- every break was
  rejected"), 6 entries abort (`exit=-6`, i.e. SIGABRT from the deleted-later
  asserts), `capacity-headroom-shortens-reads` exits 1 (caught by the
  suite's own expected-vs-measured `REQUIRE`s, per the script's own
  docstring). Total ~212s.

## Finding U -- `#define NDEBUG` before `#include <cassert>` in `dsp/Delay.hpp`

**Reproduction.** Inserted `#define NDEBUG` immediately before
`#include <cassert>` (`dsp/Delay.hpp:61-64`). Nothing else touched.

- Alone: suite `191/191 tests passed`, exit 0. Sweep gate: OK, exit 0.
  Mutation gate:
  ```
  PASS  modulation-bound-removed            exit=1
  PASS  width-bound-removed                 exit=1
  PASS  modulation-dropped-from-width-budget exit=1
  PASS  wrap-admits-capacity                exit=1
  FAIL  lines-allocated-short               exit=0   reason: stayed green
  FAIL  off-grid-width-balance-headroom     exit=0   reason: stayed green
  PASS  capacity-headroom-shortens-reads    exit=1
  check-delay-capacity-break-proofs: FAIL
  ```
  (exit 1 overall; 2 of 7 "stayed green".)
- Combination, NDEBUG + `lineL.assign(capacity - 64, 0.0f)` (built and run
  directly, not through the mutation harness): `191/191 tests passed`,
  exit 0.
- Combination, NDEBUG + the off-grid width-balance polynomial term added to
  `maxSpreadSeconds` (built and run directly): `191/191 tests passed`,
  exit 0.

All three of the finding's reported numbers reproduce exactly.

**Verdict: TRUE.**

**Scope: INSIDE** the compile-gated compares' own claimed job. The quoted
comment states these asserts are "the only thing keeping these two reads
inside what the line holds" in a `FROGGERS_DSP_CHECKS` build, precisely
because a bare (non-`FROGGERS_DSP_CHECKS`-gated) `assert` would be too
dangerous to leave live in the browser build. `#define NDEBUG` silently
defeats that exact promise inside every test/check build too -- squarely the
mechanism the claim is about, not something outside it.

**Blocks: BLOCKS DELIVERY**, and the artifact supports the second disputed
position, not the first. The Makefile-order fact plus the mechanism check
above settle it: `check-delay-capacity-break-proofs` is a `test`
prerequisite listed before `$(DSP_TEST_BIN)`, and the NDEBUG-alone case
already fails it (exit 1, reproduced above) -- `make test` aborts right
there, by default `make` semantics, before `$(DSP_TEST_BIN)` is even built.
The "combinations" never get an opportunity to run inside a `make test`
invocation; the alone case's own gate failure is what blocks. The position
"alone it blocks nothing, the two combinations block delivery" is not what
`make test` as a whole does.

**What breaks if not fixed.** For as long as this line ships, `make test`
stays permanently red at the `check-delay-capacity-break-proofs` gate (a
true, correct rejection): every `assert()` this header's capacity guard
relies on becomes a global no-op, in every build that includes this header,
not only test/check builds -- the capacity safety net is dead everywhere,
silently, and the parity suite alone (`191/191`) would not show it.

## Finding V -- test file only: `dmod` sweep pinned to `{0.0f}` + decoy

**Reproduction.** `dsp/Delay.hpp` untouched. In
`FroggersDspParityTests.cpp`: changed the grid test's
`const float dmods[] = {0.0f, 1.0f};` to `{0.0f}`; added a new
`TEST_CASE(stereo_delay_capacity_surface_placeholder)` containing an unused
local `int capacityMarkerUnused = 0;` and an `if (false) { ... }` block whose
dead code calls `SetWidthBalance(0.0f)` and sets `p.dmod = 1.0f;` (real code,
not a comment).

- Suite: `192/192 tests passed`, exit 0 (192 because the decoy TEST_CASE adds
  one; it always passes).
- Sweep gate: OK, exit 0 -- `Mod depth (p.dmod): [0.0, 1.0]`, padded by the
  decoy's dead-code literal even though the real grid test's runtime sweep no
  longer varies it.
- Mutation gate: 7/7 PASS, identical structure to the control run (6 aborts
  at `exit=-6`, `capacity-headroom-shortens-reads` at `exit=1`), "OK -- every
  break was rejected."

**Verdict: TRUE.**

**Scope: INSIDE** the sweep gate's own claimed job -- it explicitly says it
"collects every literal value ... assigns to it" via text scan, and
explicitly excludes only comments, not dead code, from counting; the decoy
literal is real code, so the gate is doing exactly what its own docstring
says. **OUTSIDE** `make test`'s end-to-end rejection: the mutation gate,
independently reproduced above, is completely unaffected by this change,
because `stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks`
(not classified as a capacity-surface body, so invisible to the sweep gate)
still drives `p.dmod` with fresh random values on every call and still hits
the live compile-gated asserts -- confirmed by the mutation gate rejecting
all 7 tabled breaks exactly as before. No mechanism that protects delivery
is broken by this finding.

**Blocks: NEITHER**, for `make test` as a whole. The sweep gate's "OK" for
Mod depth stops being trustworthy evidence that the *named* capacity-surface
test itself covers that parameter -- an observability loss -- but no defect
passes `make test` undetected as a result, since the random knob walk
continues to exercise the live asserts regardless.

**What breaks if not fixed.** Nothing blocks today; the gap is latent. If the
random knob walk test is ever weakened or removed later, this finding's
narrowing (real dmod coverage in the named test reduced to one value,
propped up only by a decoy) would become load-bearing with nothing left
independently exercising Mod depth.

## Finding W -- three compile-gated compare sites deleted outright

**Reproduction.** `dsp/Delay.hpp` only: removed the two-line assert pair in
`Process()` (`assert(timeL <= capacitySeconds); assert(timeR <=
capacitySeconds);`), the two-line pair in `ReadAt()` (`assert(idx0 <
line.size()); assert(idx1 < line.size());`), and the single assert in
`WriteSample()` (`assert(writePos < line.size());`), leaving the
`#if defined(FROGGERS_DSP_CHECKS)` / `#endif` shells empty. Nothing else
touched.

- Suite: `191/191 tests passed`, exit 0.
- Mutation gate:
  ```
  PASS  modulation-bound-removed            exit=1
  PASS  width-bound-removed                 exit=1
  PASS  modulation-dropped-from-width-budget exit=1
  PASS  wrap-admits-capacity                exit=1
  FAIL  lines-allocated-short               exit=0   reason: stayed green
  FAIL  off-grid-width-balance-headroom     exit=0   reason: stayed green
  PASS  capacity-headroom-shortens-reads    exit=1
  check-delay-capacity-break-proofs: FAIL
  ```
  (exit 1 overall; 5 PASS / 2 FAIL, matching the finding exactly.) Note the 5
  surviving PASS entries now exit with code 1 (a `REQUIRE_TRUE`/
  `REQUIRE_NEAR` failure), not `-6`/SIGABRT as in the control -- those five
  tabled shapes are independently caught by the suite's own
  expected-vs-measured behavioral assertions, not by the deleted asserts;
  only the two "stayed green" entries relied solely on what was deleted.

**Verdict: TRUE** (the deletion, and its 5 PASS / 2 FAIL split).

**Untabled-break run (required).** Chose a break distinct from the disputed
`lineR`-only-shortened case: weakened the modulation budget's ceiling rather
than removing its clamp --
`const float maxModSeconds = capacitySeconds - baseSeconds;` changed to
`const float maxModSeconds = capacitySeconds;` -- applied to a **fresh copy
with the three compare sites intact** (i.e. not layered on W's deletion),
then ran the ordinary suite binary directly (not the mutation-gate script):

```
[width spread grid sweep] dtim=0.5 dwid=0.75 widthBalance=0 dmod=1 ...
Assertion failed: (timeL <= capacitySeconds), function Process, file Delay.hpp, line 720.
RUN_EXIT=134
```

The suite aborted mid-run (SIGABRT) before completing; zero `[FAIL]` lines
were printed because the abort pre-empted the pass/fail tally.

**What this settles.** The mutation gate's own docstring scopes its claim to
"the family of defects they exist to catch" (the seven tabled mutations) --
it never claims to catch defects of arbitrary shape, so "the gate offers no
protection against differently shaped defects" is true of that script's own
narrow claim. But it is **false** of the actual protective mechanism: with
the compares intact, this independently-chosen, untabled, differently-shaped
break in the same bound family was caught end-to-end by the *ordinary*
`$(DSP_TEST_BIN)` suite run -- itself a `make test` prerequisite built with
`FROGGERS_DSP_CHECKS`, not merely something visible inside the mutation
harness's isolated copies.

**Scope: INSIDE** the compile-gated compares' claimed job (same quotes as
Finding U) for the deletion itself; the mutation gate's own claim is
correctly narrow (tabled family only) and the deletion is squarely inside
that narrow claim too (it is a tabled-family test, and 2 of 7 tabled entries
go undetected once the compares are gone).

**Blocks: BLOCKS DELIVERY.** Same Makefile-order mechanism as Finding U:
`check-delay-capacity-break-proofs` is a `test` prerequisite ahead of
`$(DSP_TEST_BIN)`, and it fails (exit 1, reproduced above) against the
as-shipped W header on its own, independent of any additional break stacked
on top. `make test` stops there by default `make` semantics. The two
disputed positions are each partly right, talking past each other: "a break
not in the table would go undetected end to end" is true only in the narrow
sense that the mutation-gate script does not name or specifically attribute
an untabled break -- and false in the sense that matters for `make test`'s
pass/fail outcome, which both the other examiner's `lineR`-only experiment
and my independent `maxModSeconds` experiment confirm gets caught (mine, by
the ordinary suite itself, since I left the compares intact to isolate that
question; the `lineR` case, by the mutation gate rejecting the already-shipped
W state regardless of what novel break rides along).

**What breaks if not fixed.** `make test` stays red at
`check-delay-capacity-break-proofs` for as long as the three sites stay
deleted (correctly blocking delivery). If that gate were ever bypassed, the
two shapes it alone still catches under this deletion -- a short-allocated
line, and a width-balance-shaped bound weakening that vanishes at the
sampled grid points -- would ship undetected by anything else in the suite.

## Summary

| Finding | Verdict | Scope | Blocks |
|---|---|---|---|
| U (`#define NDEBUG`) | TRUE | INSIDE compares' claim | BLOCKS DELIVERY (alone; combinations never reached) |
| V (dmod pinned + decoy) | TRUE | INSIDE sweep gate's claim, OUTSIDE end-to-end rejection | NEITHER |
| W (3 compare sites deleted) | TRUE (5 PASS/2 FAIL) | INSIDE compares' claim | BLOCKS DELIVERY |
