"""check_common.py -- the tree walk the check scripts share.

Three scripts each walked the tree with their own copy of the same exclusion
list, and the copies had already drifted apart before any of them shipped: one
omitted `.git`, so it would have descended into git's own object store given a
matching filename. That is this repository's standing failure in miniature -- a
rule enforced by three separate enumerations, protecting only the pass that ran
each one -- and it is worth not committing in the very change that exists to
stop it.

Not a general utility. One walk, one exclusion list, one place to correct them.
"""

import os
import re

# Build outputs, vendored trees and git's own storage. `.git` matters: a walk
# without it can match a filename inside .git/objects and report a citation as
# resolving to something no reader can open.
EXCLUDED_DIRS = ("build", "build-launcher", "node_modules", ".git", ".venv-pages")

# A comment or markup line's own marker, stripped so `joined_at` below can
# concatenate a wrapped comment's continuation onto its head without also
# gluing on a second `//`. Shared because two checks now join wrapped comment
# lines this way; see `joined_at`'s own docstring for what it does and does
# not preserve at the join point.
COMMENT_HEAD = re.compile(r"^\s*(?://+|#+|\*+|<!--)\s?")


def joined_at(lines, n):
    """Line `n` (0-based) joined to the next line that carries any text.

    A citation, or a phrase, can wrap across a bare `//` spacer, so the
    continuation is the next line with a body rather than strictly the next
    line. Returns the joined text, the index of the continuation, and the
    offset where the join happened.

    The join drops whatever whitespace sat at the wrap point (the comment
    head's own trailing space is stripped along with the marker, and the
    head text is rstripped): correct for a path or identifier split at a
    non-space character, where nothing belongs at the seam. A pattern
    matching prose wrapped at a word boundary must allow for the resulting
    zero-width seam itself (`\\s*`, not `\\s+`, between the words that used to
    have a real space between them) rather than have this join insert one,
    which would corrupt the path/identifier case every other caller relies
    on.
    """
    head = lines[n].rstrip()
    for k in range(n + 1, min(n + 3, len(lines))):
        body = COMMENT_HEAD.sub("", lines[k])
        if body.strip():
            return head + body.lstrip(), k, len(head)
    return None, None, None


def walk_sources(root, extensions):
    """Yield every file under `root` whose name ends in one of `extensions`,
    skipping the excluded directories. Paths come back absolute."""
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in EXCLUDED_DIRS]
        for name in sorted(filenames):
            if name.endswith(extensions):
                yield os.path.join(dirpath, name)


def walk_all(root):
    """Yield every file under `root`, same exclusions. Used where the question
    is whether a path exists at all rather than what kind of file it is."""
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in EXCLUDED_DIRS]
        for name in sorted(filenames):
            yield os.path.join(dirpath, name)


def read(path):
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        return fh.read()


def strip_comments(text):
    """Removes `//` line comments and `/* ... */` block comments from `text`,
    leaving string literals (`"..."`, backslash escapes honoured) and
    character literals (`'...'`) intact and every newline in place -- a
    character walk rather than a regex, because a regex alternation of
    string/comment patterns is exactly the shape that starts matching a
    `//` or `/*` sitting inside a string literal instead of skipping it."""
    result = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            quote = c
            result.append(c)
            i += 1
            while i < n:
                result.append(text[i])
                if text[i] == "\\" and i + 1 < n:
                    i += 1
                    result.append(text[i])
                    i += 1
                    continue
                if text[i] == quote:
                    i += 1
                    break
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            i += 2
            while i < n and text[i] != "\n":
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                if text[i] == "\n":
                    result.append("\n")
                i += 1
            i = min(i + 2, n)
            continue
        result.append(c)
        i += 1
    return "".join(result)


def path_index(repo, roots):
    """Every file's repo-relative path under `roots`, and only that -- the one
    spelling a citation may use to name a file.

    Basenames are deliberately not indexed: a bare filename with no directory
    says nothing about WHICH file of that name is meant, and this tree reuses
    names freely -- every vendored example keeps its own `Makefile`, and common
    script and document names repeat across unrelated directories. The index is
    also the only way a path is accepted; asking the filesystem instead would
    accept directories, paths that climb out of the tree, and everything inside
    the build and `.git` trees this walk excludes.
    """
    paths = set()
    for base in roots:
        root_dir = os.path.join(repo, base)
        if not os.path.isdir(root_dir):
            continue
        for full in walk_all(root_dir):
            paths.add(os.path.relpath(full, repo))
    return paths
