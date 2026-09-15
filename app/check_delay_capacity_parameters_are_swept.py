#!/usr/bin/env python3
"""check_delay_capacity_parameters_are_swept.py -- fails when the Delay
capacity-surface checks all pin some shipping parameter to one value.

`StereoDelay::Process`'s width-spread bound (dsp/Delay.hpp) is a function of
exactly five reachable knobs: sample rate, Delay time, Stereo width, Width
balance and Mod depth. Three adversarial passes each found the same shape of
defect one instance at a time -- Mod depth pinned at 0, Width balance pinned
at 1.0, sample rate pinned at 48000 -- and each cost a full audit round,
because an enumeration written down once protects only the pass that wrote
it: the next parameter someone pins goes unnoticed the same way.

This finds the capacity-surface test bodies mechanically -- every TEST_CASE
in the DSP parity suite whose body mentions "capacity" (case-insensitive),
the operand none of those checks can avoid sharing -- rather than by a
hardcoded list of test names, so a renamed or newly added capacity check is
picked up automatically and a deleted one silently drops out. For each of
the five parameters, it collects every literal value that check family
assigns to it -- a direct `p.dtim = 0.3f`, an argument to `SetSampleRate(...)`
or `SetWidthBalance(...)`, or the contents of an array literal named for the
parameter (`dtims[] = {0.0f, 0.5f, ...}`) that a loop later assigns from --
and fails if the union across every capacity-surface check body still has
only one distinct value. A parameter this file cannot find at all is the
same failure as one pinned everywhere: no check exercises it. Comment text
does not count, so a literal written in a comment cannot stand in for a
swept value. The scan is textual: it does not judge whether a literal is
reachable, so a value inside dead code still counts. It measures the
breadth of the sweep the tests declare; whether a bound is actually
rejected is the parity suite's and the break-proof gate's job.

Usage: check_delay_capacity_parameters_are_swept.py <app-dir>
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_common import read, strip_comments  # noqa: E402

NAME = "check-delay-capacity-parameters-are-swept"

DSP_TEST_RELPATH = "FroggersDspParityTests.cpp"

TEST_CASE_RE = re.compile(r"TEST_CASE\(([A-Za-z0-9_]+)\)\s*\{")

# One entry per capacity-surface knob: the name reported in a failure, and
# every source-level operand that spells it -- a bare scalar token, a
# SetXxx(...) call, and the stem an array-of-swept-values would carry
# (`dtims`, `dwids`, ...). Matched case-insensitively as whole words so
# `widthBalance` and `wb` both resolve to the same tracked parameter without
# also matching unrelated identifiers that merely contain the substring.
TRACKED_PARAMETERS = [
    ("sample rate", ["SetSampleRate", r"\bsr\b"]),
    ("Delay time (p.dtim)", ["p\\.dtim", r"\bdtim\b", r"\bdtims\b"]),
    ("Stereo width (p.dwid)", ["p\\.dwid", r"\bdwid\b", r"\bdwids\b"]),
    ("Width balance", ["SetWidthBalance", r"\bwidthBalance\b", r"\bwb\b", r"\bwbs\b"]),
    ("Mod depth (p.dmod)", ["p\\.dmod", r"\bdmod\b", r"\bdmods\b"]),
]

FLOAT_LITERAL = r"[0-9]+(?:\.[0-9]+)?"


def find_test_case_bodies(text):
    """Yield (name, body) for every TEST_CASE in `text`, body delimited by
    brace matching from the opening brace TEST_CASE_RE lands on -- these
    bodies are not one-liners, so a regex without brace counting would stop
    at the first nested `}` instead of the test's own close."""
    for m in TEST_CASE_RE.finditer(text):
        name = m.group(1)
        depth = 1
        i = m.end()
        start = i
        while i < len(text) and depth > 0:
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
            i += 1
        yield name, text[start:i - 1]


def capacity_surface_bodies(text):
    """The width-spread capacity bound is the one under protection here, not
    every incidental use of the word "capacity" -- the DSP parity suite also
    uses it in Reverb pre-delay comments and in two StereoDelay tests that
    pin Stereo width at 0 and so never make the bound do anything. The
    narrowest pair of operands neither of those carries, and every real
    capacity-surface check does: the word "capacity" itself, AND a call to
    `SetWidthBalance` -- Width balance only has a reason to be set at all on
    a path that also computes the width-spread term the capacity bound
    exists to clamp."""
    for name, body in find_test_case_bodies(text):
        stripped = strip_comments(body)
        if re.search(r"capacity", stripped, re.I) and "SetWidthBalance" in stripped:
            yield name, stripped


def literal_values_for(body, tokens):
    """Every literal float this parameter is assigned or passed across
    `body`: `<token> = <lit>`, `<token>(<lit>)`, and the comma-separated
    contents of `<token-stem>[] = { ... }` array literals a sweep loop reads
    from. Values are normalised through float() so `1`, `1.0` and `1.0f`
    collapse to one entry rather than inflating the distinct-value count."""
    values = set()
    for tok in tokens:
        for m in re.finditer(rf"{tok}\s*=\s*({FLOAT_LITERAL})f?\b", body):
            values.add(round(float(m.group(1)), 6))
        for m in re.finditer(rf"{tok}\s*\(\s*({FLOAT_LITERAL})f?\s*\)", body):
            values.add(round(float(m.group(1)), 6))
        for m in re.finditer(rf"{tok}\s*\[\s*\]\s*=\s*\{{([^}}]*)\}}", body):
            for lit in re.finditer(rf"({FLOAT_LITERAL})f?", m.group(1)):
                values.add(round(float(lit.group(1)), 6))
    return values


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <app-dir>", file=sys.stderr)
        return 2
    app_dir = sys.argv[1]
    dsp_test_path = os.path.join(app_dir, DSP_TEST_RELPATH)
    if not os.path.isfile(dsp_test_path):
        print(f"{NAME}: FAIL -- {dsp_test_path} does not exist")
        return 1

    text = read(dsp_test_path)
    bodies = list(capacity_surface_bodies(text))
    if not bodies:
        print(f"{NAME}: FAIL -- no capacity-surface TEST_CASE found in "
              f"{DSP_TEST_RELPATH} (none of its test bodies mention "
              f"\"capacity\"); the bound this file protects has nothing "
              f"exercising it at all")
        return 1

    failures = []
    report_lines = [f"{NAME}: capacity-surface checks found: "
                     f"{', '.join(name for name, _ in bodies)}"]
    for param_name, tokens in TRACKED_PARAMETERS:
        union = set()
        for _test_name, body in bodies:
            union |= literal_values_for(body, tokens)
        report_lines.append(f"  {param_name}: {sorted(union)}")
        if len(union) <= 1:
            failures.append((param_name, sorted(union)))

    print("\n".join(report_lines))

    if failures:
        print(f"{NAME}: FAIL")
        for param_name, values in failures:
            shown = values[0] if values else "(never found)"
            print(f"  {param_name} is held at one value ({shown}) across "
                  f"every capacity-surface check in {DSP_TEST_RELPATH} -- "
                  f"the capacity bound's dependence on it is unexercised")
        return 1

    print(f"{NAME}: OK -- every tracked parameter takes more than one "
          f"value across the capacity-surface checks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
