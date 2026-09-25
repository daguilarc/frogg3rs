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

# Build outputs, vendored trees and git's own storage. `.git` matters: a walk
# without it can match a filename inside .git/objects and report a citation as
# resolving to something no reader can open. `dist` is app/browser/dist/, the
# browser bundle's own gitignored build output (.gitignore): minified,
# third-party (emscripten) JS in there carries colon-digit sequences that read
# as bare citations once a scan reads trailing comments, not just whole-line
# ones, but nothing under it is source this project wrote or a citation
# convention applies to.
EXCLUDED_DIRS = ("build", "build-launcher", "node_modules", ".git", ".venv-pages", "dist")


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
