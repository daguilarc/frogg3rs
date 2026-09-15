# Postflight: repetition repairs (read/strip_comments/capacity formula/LCG step)

Baseline control, captured before any edit in this pass:

```
$ cd app && rm -f build/froggers_dsp_parity_tests && nice -n 10 make -j2 /Users/diegoaguilar-canabal/Desktop/frogg3rs/app/build/froggers_dsp_parity_tests && ./build/froggers_dsp_parity_tests > build/run-before.log 2>&1; echo EXIT=$?
EXIT=0
$ tail -1 build/run-before.log
191/191 tests passed
```

## Step 1 — `check_delay_capacity_break_proofs.py` imports `read`

Edit: added `sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))` and
`from check_common import read  # noqa: E402` after the stdlib imports (mirroring
`check_delay_capacity_parameters_are_swept.py`), and deleted the local `def read(path)`
that duplicated `check_common.read`.

Controls run (the 4-minute break-proofs run was substituted per the brief with the
cheaper syntactic/import check):

```
$ python3 -c "import ast,sys; ast.parse(open('app/check_delay_capacity_break_proofs.py').read()); print('AST OK')"
AST OK

$ cd app && python3 -c "import check_delay_capacity_break_proofs as m; print(m.read.__module__)"
check_common
```

Result: matches the expected `check_common`. Landed.

## Step 2 — one definition of the line capacity

FOUND (sites in `app/FroggersDspParityTests.cpp` that compute a delay-line capacity
from a sample rate, via `grep -n "ceil(" ` near `sr`/`kMaxDelaySamples`): **3**

```
$ grep -n "std::ceil" FroggersDspParityTests.cpp
10504:    const size_t capacity = static_cast<size_t>(std::ceil(2.0f * sr));
10546:    const size_t capacity = static_cast<size_t>(std::ceil(2.0f * sr));
10595:                                      static_cast<size_t>(std::ceil(dsp::StereoDelay::kMaxDelaySeconds * sr)));
```

CHANGED: **3** (all three). Each replaced with `dsp::StereoDelay::CapacityForSampleRate(sr)`.

`dsp/Delay.hpp`: added a public static `StereoDelay::CapacityForSampleRate(float rate)`
next to `kMaxDelaySamples`/`kMaxDelaySeconds`, containing the exact formula
`SetSampleRate` used inline (`std::min(kMaxDelaySamples, ceil(kMaxDelaySeconds*rate))`,
clamped to a 4-sample floor). `SetSampleRate` now calls it
(`capacity = CapacityForSampleRate(sampleRate);`) instead of recomputing the formula.

Both anchors the break-proofs gate table depends on still occur exactly once after
the edit:

```
$ grep -n "lineL.assign(capacity, 0.0f);\|while (idx >= capacity)" dsp/Delay.hpp
557:        lineL.assign(capacity, 0.0f);
1027:        while (idx >= capacity)
```

The grid test's comment that the formula was a private-field copy is now false and
was rewritten to say the value comes from the same function `SetSampleRate` calls.

Control:

```
$ cd app && rm -f build/froggers_dsp_parity_tests && nice -n 10 make -j2 .../build/froggers_dsp_parity_tests && ./build/froggers_dsp_parity_tests > build/run-step2.log 2>&1; echo EXIT=$?
EXIT=0
$ tail -1 build/run-step2.log
191/191 tests passed
$ diff build/run-before.log build/run-step2.log; echo DIFF_EXIT=$?
DIFF_EXIT=0
```

Empty diff, 191/191. Landed.

## Step 3 — one comment stripper

Moved the character-walk `strip_comments(text)` (with its docstring) from
`app/check_delay_capacity_parameters_are_swept.py` into `app/check_common.py`.
`check_delay_capacity_parameters_are_swept.py` now does
`from check_common import read, strip_comments`. `app/check_docs_match_parameter_table.py`
had its own, behaviorally different, regex-based `strip_comments` (no string/char-literal
protection, and its block-comment removal did not preserve embedded newlines the way the
character walk does) — that local definition was deleted and replaced with
`sys.path.insert(...)` + `from check_common import strip_comments`.

Controls, before and after, run exactly as the Makefile invokes them:

Before:
```
$ python3 app/check_delay_capacity_parameters_are_swept.py app
check-delay-capacity-parameters-are-swept: capacity-surface checks found: stereo_delay_width_spread_never_reads_past_the_line_capacity, stereo_delay_width_spread_bound_is_inert_away_from_capacity, stereo_delay_width_spread_bound_holds_across_the_reachable_grid
  sample rate: [48000.0, 96000.0]
  Delay time (p.dtim): [0.0, 0.3, 0.5, 0.9, 0.99, 1.0]
  Stereo width (p.dwid): [0.0, 0.5, 0.75, 1.0]
  Width balance: [0.0, 0.5, 1.0]
  Mod depth (p.dmod): [0.0, 1.0]
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
(EXIT=0)

$ python3 app/check_docs_match_parameter_table.py /Users/diegoaguilar-canabal/Desktop/frogg3rs/app
check-docs-match-parameter-table: OK - MANUAL.md 84 entries/0 failures; QUICK_DICT.md 84 entries/0 failures
(EXIT=0)
```

After (same commands):
```
check-delay-capacity-parameters-are-swept: capacity-surface checks found: stereo_delay_width_spread_never_reads_past_the_line_capacity, stereo_delay_width_spread_bound_is_inert_away_from_capacity, stereo_delay_width_spread_bound_holds_across_the_reachable_grid
  sample rate: [48000.0, 96000.0]
  Delay time (p.dtim): [0.0, 0.3, 0.5, 0.9, 0.99, 1.0]
  Stereo width (p.dwid): [0.0, 0.5, 0.75, 1.0]
  Width balance: [0.0, 0.5, 1.0]
  Mod depth (p.dmod): [0.0, 1.0]
check-delay-capacity-parameters-are-swept: OK -- every tracked parameter takes more than one value across the capacity-surface checks
(EXIT=0)

check-docs-match-parameter-table: OK - MANUAL.md 84 entries/0 failures; QUICK_DICT.md 84 entries/0 failures
(EXIT=0)
```

`diff` of before/after output for both gates: empty (exit 0 both). The docs gate's
output did NOT change against this repository's current MANUAL.md/QUICK_DICT.md/
FroggersParameters.hpp/FroggersUiSurface.hpp content, despite the stripper's behavior
differing from the old regex version on inputs with `//`/`/*` inside string literals or
embedded newlines inside block comments — none of those source files exercise that
difference today. Landed; only one `strip_comments` definition remains
(`app/check_common.py`), confirmed by:

```
$ grep -n "def strip_comments" check_delay_capacity_parameters_are_swept.py check_docs_match_parameter_table.py check_common.py
check_common.py:46:def strip_comments(text):
```

## Step 4 — one LCG step

FOUND (`grep -n "1664525"`): **8** occurrences of the LCG step.

```
$ grep -n "1664525" FroggersDspParityTests.cpp   # (before step 4 edits)
3331:            lcg = lcg * 1664525u + 1013904223u;
9444:        lcg = lcg * 1664525u + 1013904223u;
9549:        lcg = lcg * 1664525u + 1013904223u;
9632:        lcg = lcg * 1664525u + 1013904223u;
9747:        lcg = lcg * 1664525u + 1013904223u;
9808:        altLcg = altLcg * 1664525u + 1013904223u;
9886:        lcg = lcg * 1664525u + 1013904223u;
10736:        lcg = lcg * 1664525u + 1013904223u;
```

Classified by the exact state-to-float expression (shift, divisor, offset)
immediately following each step:

- **Class A** (shift `>> 8`, divisor `/ 8388608.0f`, offset `- 1.0f`; bipolar
  `[-1, 1)`): lines 3331, 9444, 9549, 9632, 9747, 9808, 9886 — **7 members**, all
  byte-identical in this arithmetic (an outer scalar multiplier differs by call site —
  `inputAmplitude` at 3331, `0.5f` at the rest — but that multiplier sits outside the
  classified expression).
- **Class B** (shift `>> 8`, divisor `/ 16777216.0f`, no offset; unipolar `[0, 1)`):
  line 10736 — **1 member**, arithmetically different from Class A, left unchanged.

CHANGED per class: **Class A: 7** (all of them) — consolidated into one `static`
helper, `NextLcgBipolarSample(std::uint32_t& state)`, added above the first use
(above `TEST_CASE(filter_bank_peak_gain_travel_measurement_at_and_away_from_resonance)`,
which contains line 3331). Each of the 7 sites now reads
`<multiplier> * NextLcgBipolarSample(<state>)` (or, for the array-assignment site,
`noise[...] = 0.5f * NextLcgBipolarSample(lcg);`). **Class B: 0** — single member,
not looped/abstracted, left as-is (the lambda at line ~10739 in
`stereo_delay_read_lag_stays_inside_the_line_across_random_knob_walks`).

After the edit, `grep -n "1664525"` shows exactly 2 remaining occurrences of the raw
step: the one inside `NextLcgBipolarSample`'s own body, and the one Class-B site:

```
$ grep -n "1664525" FroggersDspParityTests.cpp
3175:    state = state * 1664525u + 1013904223u;
10739:        lcg = lcg * 1664525u + 1013904223u;
```

Control:

```
$ cd app && rm -f build/froggers_dsp_parity_tests && nice -n 10 make -j2 .../build/froggers_dsp_parity_tests && ./build/froggers_dsp_parity_tests > build/run-step4.log 2>&1; echo EXIT=$?
EXIT=0
$ tail -1 build/run-step4.log
191/191 tests passed
$ diff build/run-before.log build/run-step4.log; echo DIFF_EXIT=$?
DIFF_EXIT=0
```

Empty diff, 191/191. Landed.

## Finish

```
$ python3 app/check_no_planning_history.py app
check-no-planning-history: OK - 59 files, no planning-history references
(EXIT=0)
```

### MD5s

All six touched files, current (after) state via `md5`:

```
MD5 (app/dsp/Delay.hpp) = a0fa9d738fd5a7fd14831bfcfec8f560
MD5 (app/FroggersDspParityTests.cpp) = 92c605312f97319687f09e51885656f8
MD5 (app/check_common.py) = d72bcc55487bb0c536b36d2408ef33d9
MD5 (app/check_delay_capacity_break_proofs.py) = 4ac841d95896a9234d7a69f03ca21e5a
MD5 (app/check_delay_capacity_parameters_are_swept.py) = 3e1bd9d7d1d35bf58f51f68619233c3e
MD5 (app/check_docs_match_parameter_table.py) = 06722ed08cf37a45f5c7c99553a77f86
```

"Before" (pre-this-pass) state for the same six files. The working tree already
carried uncommitted changes from earlier sessions when this pass started (`git status`
showed `app/FroggersDspParityTests.cpp`, `app/check_common.py`,
`app/check_docs_match_parameter_table.py`, `app/dsp/Delay.hpp` already modified
against `HEAD`, and `app/check_delay_capacity_break_proofs.py`/
`app/check_delay_capacity_parameters_are_swept.py` already untracked), so `git show
HEAD:...` does not yield this pass's actual starting point and was not used. Instead
each file's pre-edit content was reconstructed by mechanically reversing this pass's
own Edit calls (exact old_string/new_string pairs, applied in reverse chronological
order, each reversal asserting the expected occurrence count before substituting — no
assertion failed) and hashed with `md5` on the reconstructed copies:

```
MD5 (Delay.hpp) = 61d6108ca9a96bf957339d8492d6eafc
MD5 (FroggersDspParityTests.cpp) = 2caabd1aa6b3f123334100776004c5ff
MD5 (check_common.py) = 78593d9253fcdd3e1695859e306542fe
MD5 (check_delay_capacity_break_proofs.py) = a7f19a372f61750cf88280ae9cd0d695
MD5 (check_delay_capacity_parameters_are_swept.py) = 2ba99da0d671138478ddccf69d955576
MD5 (check_docs_match_parameter_table.py) = 0e651707ba3264cb224bdacc5ccc8d9d
```

(Cross-checked: Python `hashlib.md5` and the `md5` command line tool on the written
reconstructed files agree; all four reconstructed Python files also re-parse cleanly
with `ast.parse`.) Flagged as a process gap: this pass did not snapshot md5s of the
live files before editing, so the "before" figures above are reconstructions, not a
directly-observed pre-edit state — reported rather than presented as an unqualified
measurement.
