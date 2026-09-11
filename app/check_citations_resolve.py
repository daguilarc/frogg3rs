#!/usr/bin/env python3
"""check_citations_resolve.py -- fails on a `path:line` citation in an app/
comment that points at nothing, and on one into this tree that still cites a
line number.

Two separate failures, one script, because both are the same claim: "the thing
I am describing is over there."

UNRESOLVABLE. A citation naming a file that exists nowhere sends the next reader
somewhere real-looking and wrong, which is worse than no citation. Twenty-two of
these were in the tree when this was written, most naming the retired simulator.
The repository already has the form that fixes them and uses it 72 times: pin
the commit, `08b5fd3:src/core/VcoWaveEval.hpp:7-23`. A pinned path cannot rot,
so pinned citations are accepted whatever they name.

LINE NUMBERS INTO THIS TREE. A citation into app/ is wrong by the next commit
that edits the cited file, and this repository has measured the rate: one change
left 14 of 24 citations stale, six of them pointing at code it had itself
deleted, and four comments attributing a struct to the wrong file entirely.
Symbols do not drift. So an app/-internal citation names the symbol, and the
trailing `:NNN` is what this check refuses.

SCOPE, and why the excluded buckets are excluded. External/Sheaf/ citations name
a submodule pinned at a commit, so their lines are already as stable as a git
pin. src/ citations are the Daisy firmware port's provenance, and that tree is
frozen -- the whole value of the citation is saying which line of the original a
formula was copied from. Rewriting either would destroy provenance to fix a rot
that cannot happen.

Usage: check_citations_resolve.py <app-dir>
The repository root is that argument's parent, matching the one-argument,
cwd-independent convention every other check script in this directory uses.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_common import walk_all, walk_sources  # noqa: E402

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
SCAN_EXT = (".cpp", ".hpp")


def build_index(repo):
    """basename -> set of top-level roots it lives under."""
    index = {}
    for base in SEARCH_ROOTS:
        root_dir = os.path.join(repo, base)
        if not os.path.isdir(root_dir):
            continue
        for full in walk_all(root_dir):
            index.setdefault(os.path.basename(full), set()).add(base)
    return index


def main():
    if len(sys.argv) != 2:
        print(f"{NAME}: FAIL - usage: check_citations_resolve.py <app-dir>", file=sys.stderr)
        return 2
    app_dir = os.path.abspath(sys.argv[1])
    repo = os.path.dirname(app_dir)
    index = build_index(repo)
    self_name = os.path.basename(__file__)

    unresolved, internal = [], []
    pinned = external = 0

    for full in walk_sources(app_dir, SCAN_EXT):
            if os.path.basename(full) in (self_name, "check_common.py"):
                continue
            rel = "app/" + os.path.relpath(full, app_dir)
            with open(full, "r", encoding="utf-8", errors="replace") as fh:
                for n, line in enumerate(fh, 1):
                    for m in CITATION.finditer(line):
                        if m.group("pin"):
                            pinned += 1
                            continue
                        path = m.group("path")
                        roots = index.get(os.path.basename(path), set())
                        if not roots:
                            unresolved.append(f"{rel}:{n} cites `{path}`, which exists nowhere "
                                              f"under {'/, '.join(SEARCH_ROOTS)}/ and carries no commit pin")
                        elif "app" in roots:
                            internal.append(f"{rel}:{n} cites `{path}:{m.group('line')}` -- "
                                            f"a line number into this tree; name the symbol instead")
                        else:
                            external += 1

    errors = unresolved + internal
    if errors:
        for e in errors:
            print(f"{NAME}: FAIL - {e}", file=sys.stderr)
        print(f"{NAME}: FAIL - {len(unresolved)} unresolvable, {len(internal)} line-numbered "
              f"into this tree", file=sys.stderr)
        return 1

    print(f"{NAME}: OK - {pinned} commit-pinned, {external} into pinned or frozen trees, "
          f"0 unresolvable, 0 line-numbered into this tree")
    return 0


if __name__ == "__main__":
    sys.exit(main())
