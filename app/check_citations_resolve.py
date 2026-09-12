#!/usr/bin/env python3
"""check_citations_resolve.py -- fails on a `path:line` citation in an app/
comment that points at nothing, on one into this tree that still cites a line
number, and on one whose path is split across two comment lines.

Three separate failures, one script, because all three are the same claim: "the
thing I am describing is over there."

SPLIT ACROSS LINES. The checks above read one line at a time, so a citation
whose path is wrapped onto a continuation line matches nothing and is not
checked at all -- it is not unresolvable, it is invisible, which is worse. This
check joins each pair of adjacent comment lines and asks whether the JOIN yields
a citation that RESOLVES and that neither line yields on its own. That is the
whole rule, and it is exact rather than heuristic: a resolving path that only
exists across the boundary was split there, and nothing else produces one.

WHAT THIS DELIBERATELY DOES NOT CATCH, and why. A per-line path rewrite once
left seven citations where the first line kept a stale fragment and the second
carried the WHOLE path -- `(External/Sheaf/` above the full path. Those were
repaired by hand. This check is silent on them by design, because the citation
on the second line resolves and IS checked: the damage is that the comment reads
wrong, not that anything went unverified. Two attempts to catch that shape as
well were withdrawn after a context that did not write them defeated both. The
reason is not a weak implementation, it is that the shape is not distinguishable
from prose that names a directory and then cites a file inside it --
"this logic used to live under External/Sheaf/.../synth/" above a correct
citation is the same text with a different intent. A check that cannot tell
those apart fires on correct comments, and a check people work around is worse
than none.
Usage: check_citations_resolve.py <app-dir>
The repository root is that argument's parent, matching the one-argument,
cwd-independent convention every other check script in this directory uses.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_common import path_index, walk_all, walk_sources  # noqa: E402

NAME = "check-citations-resolve"

# A path segment may carry digits, hyphens and dots -- `Braid4Core.hpp`,
# `V2FuegoStack.hpp`, `juce_Timer.cpp`. A pattern of `[A-Za-z_]+` truncates
# those to `Core.hpp` and `FuegoStack.hpp` and then reports files as missing
# that were never cited; that mistake produced a count of 47 where the real
# number was 22.
#
# The git pin may be a bare sha, a parent (`sha^`) or an ancestor (`sha~2`).
# `f2369151^:sim/StereoDelay.hpp:60-64` is real in this tree, and a pattern
# without `^` reports it as dangling.
CITATION = re.compile(
    r"(?P<pin>\b[0-9a-f]{7,40}(?:\^|~\d+)?:)?"
    r"(?P<path>[A-Za-z0-9_./-]*[A-Za-z0-9_-]\.(?:hpp|cpp|h|mm|md|py|sh|mjs|ts))"
    r":(?P<line>\d+)"
)

SEARCH_ROOTS = ("app", "External/Sheaf", "src")

# A trailing path fragment: one or more path segments ending in a separator. A
# bare comment marker is excluded explicitly -- `//` ends in a slash and is not
# a path.
SEARCH_ROOTS = ("app", "External/Sheaf", "src")
SCAN_EXT = (".cpp", ".hpp")

# A path with no line number required, for reading what a joined boundary says.
SPLIT_PATH = re.compile(r"[A-Za-z0-9_./-]*[A-Za-z0-9_-]\.(?:hpp|cpp|h|mm|md|py|sh|mjs|ts|js|c)")

# Every extension a comment can live in. Wider than SCAN_EXT above: a split in a
# shell script or a markdown file is no less invisible than one in a header.
SPLIT_EXT = (".cpp", ".hpp", ".h", ".c", ".mm", ".py", ".sh", ".mjs", ".js", ".ts", ".md")

COMMENT_HEAD = re.compile(r"^\s*(?://+|#+|\*+|<!--)\s?")


def joined_at(lines, n):
    """Line `n` (0-based) joined to the next line that carries any text.

    A citation can wrap across a bare `//` spacer, so the continuation is the
    next line with a body rather than strictly the next line. Returns the joined
    text, the index of the continuation, and the offset where the join happened.
    """
    head = lines[n].rstrip()
    for k in range(n + 1, min(n + 3, len(lines))):
        body = COMMENT_HEAD.sub("", lines[k])
        if body.strip():
            return head + body.lstrip(), k, len(head)
    return None, None, None


def citations_spanning(joined, boundary, paths):
    """Resolving citations that actually CROSS the join, not merely sit past it.

    Without this the check reports text it did not join. Concatenating
    `...transfer_function_detail` to `08b5fd3:src/core/TanhSaturator.hpp:25-30`
    welds `detail` onto the commit pin, which destroys the pin's word boundary
    and leaves an unpinned path the second line already carried correctly. Every
    such report is of a citation lying wholly on one side of the boundary, so
    requiring the match to straddle it removes the whole class -- and a citation
    that straddles the boundary is, by construction, one no single line spells.
    """
    out = set()
    for m in CITATION.finditer(joined):
        if m.start() >= boundary or m.end() <= boundary:
            continue
        if m.group("pin") or m.group("path") in paths:
            out.add(m.group(0))
    return out


def main():
    if len(sys.argv) != 2:
        print(f"{NAME}: FAIL - usage: check_citations_resolve.py <app-dir>", file=sys.stderr)
        return 2
    app_dir = os.path.abspath(sys.argv[1])
    repo = os.path.dirname(app_dir)
    paths = path_index(repo, SEARCH_ROOTS)
    self_name = os.path.basename(__file__)

    unresolved, internal, split = [], [], []
    pinned = external = 0

    for full in walk_sources(app_dir, SCAN_EXT):
        if os.path.basename(full) in (self_name, "check_common.py"):
            continue
        rel = "app/" + os.path.relpath(full, app_dir)
        with open(full, "r", encoding="utf-8", errors="replace") as fh:
            lines = fh.read().splitlines()
        for n, line in enumerate(lines, 1):
            for m in CITATION.finditer(line):
                if m.group("pin"):
                    pinned += 1
                    continue
                path = m.group("path")
                if path not in paths:
                    unresolved.append(f"{rel}:{n} cites `{path}`, which is not a file under "
                                      f"{'/, '.join(SEARCH_ROOTS)}/ and carries no commit pin; "
                                      f"give the path from the repository root")
                elif path.startswith("app/"):
                    internal.append(f"{rel}:{n} cites `{path}:{m.group('line')}` -- "
                                    f"a line number into this tree; name the symbol instead")
                else:
                    external += 1

    for full in walk_sources(app_dir, SPLIT_EXT):
        if os.path.relpath(full, app_dir) in (self_name, "check_common.py"):
            continue
        rel = "app/" + os.path.relpath(full, app_dir)
        with open(full, "r", encoding="utf-8", errors="replace") as fh:
            lines = fh.read().splitlines()
        for n in range(len(lines)):
            joined, k, boundary = joined_at(lines, n)
            if joined is None:
                continue
            for cite in sorted(citations_spanning(joined, boundary, paths)):
                split.append(f"{rel}:{n + 1} and :{k + 1} spell `{cite}` only across the "
                             f"line break, so neither line carries a citation anything "
                             f"checks; put the whole path on one line")

    errors = unresolved + internal + split
    if errors:
        for e in errors:
            print(f"{NAME}: FAIL - {e}", file=sys.stderr)
        print(f"{NAME}: FAIL - {len(unresolved)} unresolvable, {len(internal)} line-numbered "
              f"into this tree, {len(split)} split across two lines", file=sys.stderr)
        return 1

    print(f"{NAME}: OK - {pinned} commit-pinned, {external} into pinned or frozen trees, "
          f"0 unresolvable, 0 line-numbered into this tree, 0 split across two lines")
    return 0


if __name__ == "__main__":
    sys.exit(main())
