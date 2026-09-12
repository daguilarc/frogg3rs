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
# resolving to something no reader can open.
EXCLUDED_DIRS = ("build", "build-launcher", "node_modules", ".git", ".venv-pages")


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
