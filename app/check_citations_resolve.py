#!/usr/bin/env python3
"""check_citations_resolve.py -- fails on a `path:line` citation in an app/
comment that points at nothing, on a line-numbered citation into a tree whose
lines move out from under it, and on one whose path is split across two
comment lines.

Four separate failures, one script, because all four are the same claim: "the
thing I am describing is over there."

UNRESOLVABLE. A citation whose path is not a file under `app/`,
`External/Sheaf/` or `src/`, and carries no commit pin, points at nothing.

LINE NUMBERS INTO A TREE THAT MOVES. A line number is only as good as the file
it counts lines in staying still. `app/` is edited by the very change that
reads the citation, so a citation into `app/` SHALL name the symbol instead of
a line. `External/Sheaf/` is a submodule whose pin moves with every library
change carried into this tree, so a citation into it with no commit pin is the
same defect: the line was true at some past pin and nothing re-checks it after
the next one. Only a commit-pinned citation, or one into the frozen `src/`
tree, keeps a line number.

BARE LINE NUMBERS. Comments in this tree often cite a file once and then refer
to a second spot in it with only `(:2894)` or `:569-627` -- no path repeated.
That is read as a citation into the nearest path cited earlier in the same
comment block (a run of consecutive comment lines; a blank or code line ends
it), and the rule for that path applies, so a bare number after an unpinned
`External/Sheaf/` citation fails exactly as a spelled-out one would. A bare
number with no path anywhere earlier in its comment block fails outright --
there is nothing to resolve it against.

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
from check_common import path_index, walk_sources  # noqa: E402

NAME = "check-citations-resolve"

# A path segment may carry digits, hyphens and dots -- `Braid4Core.hpp`,
# `V2FuegoStack.hpp`, `juce_Timer.cpp`. A pattern of `[A-Za-z_]+` truncates
# those to `Core.hpp` and `FuegoStack.hpp` and then reports files as missing
# that were never cited; that mistake produced a count of 47 where the real
# number was 22. `.html` is included because the browser build's own citations
# point into it (`index.html`), not only into script files.
#
# The git pin may be a bare sha, a parent (`sha^`) or an ancestor (`sha~2`).
# `f2369151^:sim/StereoDelay.hpp:60-64` is real in this tree, and a pattern
# without `^` reports it as dangling.
CITATION = re.compile(
    r"(?P<pin>\b[0-9a-f]{7,40}(?:\^|~\d+)?:)?"
    r"(?P<path>[A-Za-z0-9_./-]*[A-Za-z0-9_-]\.(?:hpp|cpp|h|mm|md|py|sh|mjs|ts|html|css))"
    r":(?P<line>\d+)"
)

# A bare line reference with no path of its own -- `(:2894)`, `:343`, or
# `(:223,230, ...)`. Restricted to comment lines by the caller: a colon-digit
# pair in code (a slice, a ratio, a time) is not a citation and is never
# tested against this pattern.
BARE_LINE = re.compile(r"(?<![\w./-])\(?(?P<bare>:\d+(?:\s*[-,]\s*\d+)*)")

# A path named with no line number of its own -- "b9a8199^:desktop-v2/
# Source/V2DesktopPageDisplayNames.hpp's forHostPageRow" -- naming the file
# once before a run of bare line references into it. Not itself one of the
# four failure shapes (nothing to resolve, nothing stale to point at a wrong
# line), so it raises no error; it only sets which path a later bare
# reference in the same comment block resolves against.
PATH_ONLY = re.compile(
    r"(?P<pin3>\b[0-9a-f]{7,40}(?:\^|~\d+)?:)?"
    r"(?P<path3>[A-Za-z0-9_./-]*[A-Za-z0-9_-]\.(?:hpp|cpp|h|mm|md|py|sh|mjs|ts|html|css))"
)

# All three shapes, in the order they can appear on one line and in the order
# each is tried at a given position: a full citation first (it is the most
# specific), then a bare line number, then a bare path mention. A comment
# that opens with a full citation and later falls back to a bare one is read
# left to right in one pass -- the bare one resolves against whichever path
# came immediately before it, not against the first path the file ever cited.
COMBINED = re.compile(CITATION.pattern + "|" + BARE_LINE.pattern + "|" + PATH_ONLY.pattern)

SEARCH_ROOTS = ("app", "External/Sheaf", "src")

# Comments read for citations: every language this tree writes comments in
# under `app/`. `.html` is a citation TARGET (above), not a source scanned for
# its own citations -- the browser build's markup carries no such comments.
SCAN_EXT = (".cpp", ".hpp", ".h", ".mm", ".mjs", ".js", ".ts", ".py", ".sh")

# A path with no line number required, for reading what a joined boundary says.
SPLIT_PATH = re.compile(r"[A-Za-z0-9_./-]*[A-Za-z0-9_-]\.(?:hpp|cpp|h|mm|md|py|sh|mjs|ts|js|c)")

# Every extension a comment can live in. Wider than SCAN_EXT above: a split in a
# shell script or a markdown file is no less invisible than one in a header.
SPLIT_EXT = (".cpp", ".hpp", ".h", ".c", ".mm", ".py", ".sh", ".mjs", ".js", ".ts", ".md")

COMMENT_HEAD = re.compile(r"^\s*(?://+|#+|\*+|<!--)\s?")

# A trailing line-comment marker anywhere after code on the same line --
# `params.dtim = timeKnob01;   // :180`. Whichever of `//`/`#` appears first
# opens the comment; nothing after it is code, so a bare citation there is
# exactly as real as one on a whole-line comment. Not applied to a line
# already read as a whole-line comment by COMMENT_HEAD (that case is
# resolved from column 0, not this marker's position), and not applied
# while still inside an unstarred block comment (BLOCK_TEXT_IN below
# already treats the whole line as comment text).
TRAILING_COMMENT = re.compile(r"//|#")


def comment_offset(line, in_block):
    """Column at which `line`'s comment text starts, or None if it carries
    none, plus whether the NEXT line begins still inside an unstarred `/*
    */` block. Three shapes, tried in this order because each is a special
    case the next one below does not otherwise cover:
      - already inside a block comment (`in_block`): the whole line is
        comment text from column 0, whether or not it leads with `*` (the
        gap this check closes), until a `*/` closes it.
      - a whole-line comment (`//`, `#`, `*`, `<!--`, via COMMENT_HEAD): unchanged
        from before this check existed.
      - an opening `/*` elsewhere on the line: comment text starts right
        after it (code before it is still code).
      - a trailing `//` or `#` after code: comment text starts right after
        it (the other gap this check closes).
    """
    if in_block:
        end = line.find("*/")
        return 0, end == -1
    head = COMMENT_HEAD.match(line)
    if head:
        return head.end(), False
    block_start = line.find("/*")
    if block_start != -1:
        end = line.find("*/", block_start + 2)
        return block_start + 2, end == -1
    trailing = TRAILING_COMMENT.search(line)
    if trailing:
        return trailing.end(), False
    return None, False


def classify(path, paths):
    """Which rule a resolved-or-not path falls under: `unresolved` (not a
    file under any search root), `internal` (`app/`, edited by this change),
    `sheaf` (`External/Sheaf/`, a submodule whose pin moves), or `external`
    (`src/`, frozen, kept under today's rule)."""
    if path not in paths:
        return "unresolved"
    if path.startswith("app/"):
        return "internal"
    if path.startswith("External/Sheaf/"):
        return "sheaf"
    return "external"


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


def scan_citations(rel, lines, paths):
    """Every citation error in one file's comments: unresolvable paths,
    line numbers into `app/` or unpinned `External/Sheaf/`, and bare line
    numbers with no resolvable path earlier in their comment block.

    Tracks the nearest path cited so far IN THE CURRENT COMMENT BLOCK -- a
    run of consecutive lines that each carry a comment (comment_offset
    above: a whole-line comment, a trailing `//`/`#` after code, or a line
    inside a still-open unstarred `/* */` block) -- so a bare citation
    resolves against it and a line with no comment on it at all clears it.
    A pinned citation is remembered as pinned, so a bare number that
    follows it is kept under today's rule too, exactly as a spelled-out
    pinned citation would be. A bare or bare-path match only counts when it
    falls at or after the line's own comment_offset, so a `:N` slice or a
    `#` in code before that offset is never read as a citation.
    """
    errors = {"unresolved": [], "internal": [], "sheaf": [], "bare_unresolved": []}
    pinned = external = 0
    last_path, last_pinned = None, False
    in_block = False

    for n, line in enumerate(lines, 1):
        offset, in_block = comment_offset(line, in_block)
        if offset is None:
            last_path, last_pinned = None, False

        for m in COMBINED.finditer(line):
            comment_line = offset is not None and m.start() >= offset
            if m.group("path"):
                if m.group("pin"):
                    pinned += 1
                    last_path, last_pinned = m.group("path"), True
                    continue
                path = m.group("path")
                last_path, last_pinned = path, False
                kind = classify(path, paths)
                if kind == "unresolved":
                    errors["unresolved"].append(
                        f"{rel}:{n} cites `{path}`, which is not a file under "
                        f"{'/, '.join(SEARCH_ROOTS)}/ and carries no commit pin; "
                        f"give the path from the repository root")
                elif kind == "internal":
                    errors["internal"].append(
                        f"{rel}:{n} cites `{path}:{m.group('line')}` -- "
                        f"a line number into this tree; name the symbol instead")
                elif kind == "sheaf":
                    errors["sheaf"].append(
                        f"{rel}:{n} cites `{path}:{m.group('line')}` -- a line number "
                        f"into External/Sheaf/ with no commit pin; name the symbol instead")
                else:
                    external += 1
            elif comment_line and m.group("path3"):
                if m.group("pin3"):
                    last_path, last_pinned = m.group("path3"), True
                else:
                    last_path, last_pinned = m.group("path3"), False
            elif comment_line and m.group("bare"):
                if last_path is None:
                    errors["bare_unresolved"].append(
                        f"{rel}:{n} cites the bare line number `{m.group('bare')}` with no "
                        f"path cited earlier in this comment block; name the file")
                    continue
                if last_pinned:
                    pinned += 1
                    continue
                kind = classify(last_path, paths)
                if kind == "unresolved":
                    errors["unresolved"].append(
                        f"{rel}:{n} cites the bare line number `{m.group('bare')}`, resolving "
                        f"to `{last_path}`, which is not a file under "
                        f"{'/, '.join(SEARCH_ROOTS)}/ and carries no commit pin")
                elif kind == "internal":
                    errors["internal"].append(
                        f"{rel}:{n} cites the bare line number `{m.group('bare')}`, resolving "
                        f"to `{last_path}` -- a line number into this tree; name the symbol instead")
                elif kind == "sheaf":
                    errors["sheaf"].append(
                        f"{rel}:{n} cites the bare line number `{m.group('bare')}`, resolving to "
                        f"`{last_path}` -- a line number into External/Sheaf/ with no commit pin; "
                        f"name the symbol instead")
                else:
                    external += 1

    return errors, pinned, external


def main():
    if len(sys.argv) != 2:
        print(f"{NAME}: FAIL - usage: check_citations_resolve.py <app-dir>", file=sys.stderr)
        return 2
    app_dir = os.path.abspath(sys.argv[1])
    repo = os.path.dirname(app_dir)
    paths = path_index(repo, SEARCH_ROOTS)
    self_name = os.path.basename(__file__)

    unresolved, internal, sheaf, bare_unresolved, split = [], [], [], [], []
    pinned = external = 0

    for full in walk_sources(app_dir, SCAN_EXT):
        if os.path.basename(full) in (self_name, "check_common.py"):
            continue
        rel = "app/" + os.path.relpath(full, app_dir)
        with open(full, "r", encoding="utf-8", errors="replace") as fh:
            lines = fh.read().splitlines()
        errors, file_pinned, file_external = scan_citations(rel, lines, paths)
        unresolved.extend(errors["unresolved"])
        internal.extend(errors["internal"])
        sheaf.extend(errors["sheaf"])
        bare_unresolved.extend(errors["bare_unresolved"])
        pinned += file_pinned
        external += file_external

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

    errors = unresolved + internal + sheaf + bare_unresolved + split
    if errors:
        for e in errors:
            print(f"{NAME}: FAIL - {e}", file=sys.stderr)
        print(f"{NAME}: FAIL - {len(unresolved)} unresolvable, {len(internal)} line-numbered "
              f"into this tree, {len(sheaf)} line-numbered into External/Sheaf/ with no pin, "
              f"{len(bare_unresolved)} bare line numbers with no earlier path, "
              f"{len(split)} split across two lines", file=sys.stderr)
        return 1

    print(f"{NAME}: OK - {pinned} commit-pinned, {external} into pinned or frozen trees, "
          f"0 unresolvable, 0 line-numbered into this tree, 0 line-numbered into "
          f"External/Sheaf/ with no pin, 0 bare line numbers with no earlier path, "
          f"0 split across two lines")
    return 0


if __name__ == "__main__":
    sys.exit(main())
