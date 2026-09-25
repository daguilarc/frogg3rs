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
from check_common import joined_at, walk_sources  # noqa: E402

NAME = "check-no-planning-history"

# ID_LABEL_PATTERN catches an audit-style id used as a standalone label: one
# capital letter and one or two digits set off by the punctuation a label
# uses and prose never does -- wrapped in parens ("(A15)"), immediately
# followed by a colon ("D3:"), or hyphen-joined to a single bare capital
# letter that is not itself a range endpoint ("R2-B", not "D1-D10" or
# "E1-E4", where the letter after the hyphen carries its own digits). A bare
# `[A-Z][0-9]{1,2}` with no such punctuation is deliberately NOT matched: it
# is exactly the shape of a note name (`A4`), a register (`R2`), an ADSR
# stage label (`S1`, `D2`), a compiler flag (`-O2`), or a grid/row reference
# (`E1-E4`), and this tree uses all of those as real technical vocabulary --
# confirmed by running the unpunctuated pattern across app/ and finding zero
# of its hits were a planning-history citation.
ID_LABEL_PATTERN = re.compile(r"\([A-Z][0-9]{1,2}\)|\b[A-Z][0-9]{1,2}:|\b[A-Z][0-9]{1,2}-[A-Z]\b")

# `\s*`, not `\s+`: these two are two-word phrases, and a phrase this long
# wraps onto a continuation comment line in practice (both did, in the two
# lines this pattern was written to catch). `offending_lines` below also
# searches these against `joined_at`'s output, which drops the wrap point's
# whitespace entirely rather than inserting a space back in (see that
# function's own docstring) -- `\s+` would then never match the join, and
# the phrase would still slip through split exactly the way it did before.
COORDINATOR_RULING = re.compile(r"\bcoordinator\s*ruling\b", re.I)
FLAGGED_FOR_REVIEW = re.compile(r"\bflagged\s*for\s*review\b", re.I)

PATTERNS = [
    (re.compile(r"\btasks?\b", re.I), "names a task instead of the behaviour"),
    (re.compile(r"\bitems?\s+\d+", re.I), "names a numbered item"),
    (re.compile(r"\bgroup\s+\d+", re.I), "names a task group"),
    (re.compile(r"\bpackets?\s+\d+", re.I), "names a packet"),
    (re.compile(r"§\s*\d"), "cites a rule section"),
    (re.compile(r"\bproposals?\b", re.I), "cites a proposal"),
    (re.compile(r"\bSTEP\s+\d"), "cites a numbered planning step"),
    (COORDINATOR_RULING, "cites a coordinator ruling instead of describing the code"),
    (FLAGGED_FOR_REVIEW, "flags a sentence for review instead of stating the fact"),
    (ID_LABEL_PATTERN, "cites an audit-style id used as a label"),
]

# Checked again against two lines joined (see `offending_lines`): the phrase
# a wrapped comment splits across a line break, so a single-line scan never
# sees it whole. Not the whole PATTERNS list -- a single word like `task` or
# `proposal` cannot lose its meaning to a wrap, and re-running every pattern
# against every joined pair would risk a match manufactured by content that
# was never actually split (two unrelated lines whose concatenation happens
# to spell a hit). Limited to the two patterns this shipped to fix, both of
# which are known to wrap in this tree's actual comment width.
WRAP_PATTERNS = [
    (COORDINATOR_RULING, "cites a coordinator ruling instead of describing the code"),
    (FLAGGED_FOR_REVIEW, "flags a sentence for review instead of stating the fact"),
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
#
# `tasks.md` and `proposal.md` WITH their extension are filenames, not planning
# references. A gate that reads those files has to name them, and a reader
# following the name arrives at a file rather than at a numbered item they
# cannot see. The bare words stay banned: the extension is what separates a path
# from a pointer into a plan.
ALLOWED = (
    re.compile(r"TEMP-BREAK"),
    re.compile(r"\b(?:tasks|proposal)\.md\b"),
)

SCAN_EXT = (".cpp", ".hpp", ".py", ".sh")


def offending_lines(path):
    out = []
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        lines = fh.read().splitlines()

    for n, line in enumerate(lines, 1):
        if any(a.search(line) for a in ALLOWED):
            continue
        for pat, why in PATTERNS:
            if pat.search(line):
                out.append((n, why, line.strip()[:100]))
                break

    for n in range(len(lines)):
        joined, k, boundary = joined_at(lines, n)
        if joined is None or any(a.search(joined) for a in ALLOWED):
            continue
        for pat, why in WRAP_PATTERNS:
            m = pat.search(joined)
            # Only a match straddling the join is one neither line spelled on
            # its own; anything else was already reported by the per-line
            # pass above, and re-reporting it here would double-count it.
            if m and m.start() < boundary < m.end():
                out.append((n + 1, why + " (split across the line break from :" + str(k + 1) + ")",
                             (lines[n].strip() + " / " + lines[k].strip())[:100]))
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
