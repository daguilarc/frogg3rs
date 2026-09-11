#!/usr/bin/env python3
"""check_no_planning_history.py -- fails on comments and string literals under
app/ that name the planning artifact a change came from instead of describing
what the code does.

The standing rule is that a comment explains behaviour. A reader has never seen
the task list, the design document or the numbered item a change was cut from,
so "Task C" and "this task's own brief" tell them nothing and cannot be checked
against anything. The rule predates this script, was acted on twice, and was
re-violated twice; 53 lines survived in the tree when this was written, which is
what an unenforced rule is worth.

PATTERNS ARE OPERANDS, NOT SPELLINGS. An earlier attempt matched `this task's
own` and `Task [A-Z0-9]+` and missed `this task exists`, `this task fixes`,
`the task brief`, lowercase `task A/B/C`, `pre-Task-8` and `tasks 2.2-2.5`.
Matching the bare word instead finds all of them, and every one of its 49 hits
in this tree was a real violation: there is no task queue, no audio task and no
thread task here.

`D[0-9]+` IS DELIBERATELY NOT A PATTERN, and should not be added back by anyone
reading the proposal that first suggested it. It was aimed at `D1`-`D10`
design-list shorthand that has since been removed, and its only remaining hits
are the Envelope bank's own D1/D2/D3 decay labels -- a product name. Nine hits,
zero true positives. A check that fails the build on legitimate content gets
switched off, and then it protects nothing.

Usage: check_no_planning_history.py <app-dir>
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_common import walk_sources  # noqa: E402

NAME = "check-no-planning-history"

PATTERNS = [
    (re.compile(r"\btasks?\b", re.I), "names a task instead of the behaviour"),
    (re.compile(r"\bitems?\s+\d+", re.I), "names a numbered item"),
    (re.compile(r"\bgroup\s+\d+", re.I), "names a task group"),
    (re.compile(r"\bpackets?\s+\d+", re.I), "names a packet"),
    (re.compile(r"§\s*\d"), "cites a rule section"),
    (re.compile(r"\bproposals?\b", re.I), "cites a proposal"),
    (re.compile(r"\bSTEP\s+\d"), "cites a numbered planning step"),
]

# `design doc` is DELIBERATELY ABSENT for the same reason `D[0-9]+` is. Its one
# hit in this tree is `neither the governing spec nor the design doc names a
# specific limit` -- prose observing that NO document constrains a choice, which
# is the reasoning behind a value rather than a citation of a planning artifact.
# A pattern whose only match is legitimate does not protect anything; it teaches
# people to reach for a synonym, which is what happened the first time this
# shipped.
#
# DELIBERATELY ABSENT, and not to be added: lowercase `step N`, `part N`,
# `phase N`, `wave N`. Each reads as planning shorthand and is overwhelmingly
# technical prose in this tree -- `phase 0.5 -> < 1` is signal phase, `Part 1
# asserts the thing this test is named for` is test structure, and `step 1
# proved insufficient` points at a numbered list inside the same comment, which
# is a reader can follow. Uppercase `STEP N` is kept because every instance of
# it here is a dated planning label. Patterns are chosen by what they actually
# match in this tree, not by what they sound like they would match.

# A TEMP-BREAK note records HOW a figure was measured -- deliberately breaking a
# thing to watch a check go red. That is method, and a reader can act on it.
ALLOWED = (re.compile(r"TEMP-BREAK"),)

SCAN_EXT = (".cpp", ".hpp", ".py", ".sh")


def offending_lines(path):
    out = []
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for n, line in enumerate(fh, 1):
            if any(a.search(line) for a in ALLOWED):
                continue
            for pat, why in PATTERNS:
                if pat.search(line):
                    out.append((n, why, line.strip()[:100]))
                    break
    return out


def main():
    if len(sys.argv) != 2:
        print(f"{NAME}: FAIL - usage: check_no_planning_history.py <app-dir>", file=sys.stderr)
        return 2
    app_dir = sys.argv[1]

    errors = []
    scanned = 0
    for full in walk_sources(app_dir, SCAN_EXT):
        if os.path.basename(full) in (os.path.basename(__file__), "check_common.py"):
            continue
        scanned += 1
        for n, why, text in offending_lines(full):
            rel = os.path.relpath(full, app_dir)
            errors.append(f"app/{rel}:{n} {why}: {text}")

    if errors:
        for e in errors:
            print(f"{NAME}: FAIL - {e}", file=sys.stderr)
        print(f"{NAME}: FAIL - {len(errors)} planning-history reference(s) in {scanned} files",
              file=sys.stderr)
        return 1

    print(f"{NAME}: OK - {scanned} files, no planning-history references")
    return 0


if __name__ == "__main__":
    sys.exit(main())
