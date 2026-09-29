#!/usr/bin/env python3
"""check_android_sources_match.py -- fails when app/android/Frogg3rs.jucer's
list of Sheaf sources differs from the tree it is supposed to mirror.

Projucer lists files; it has no glob. The Android build's Sheaf source list
(app/android/Frogg3rs.jucer's <FILE> entries under External/Sheaf/) is
therefore a second, hand-maintained enumeration of exactly the set
External/Sheaf/projects/synth/runtime/juce_build.mk already globs for every
other Sheaf synth app (its SYNTH_SRC plus SYNTH_RUNTIME_SRC): every
`External/Sheaf/projects/synth/src/*.cpp`, plus
`External/Sheaf/projects/synth/runtime/HostDataPaths.cpp`. A source added or
removed upstream would otherwise drift the two silently -- the macOS and
Windows builds would pick it up through their own globs, and only the next
Android link (or, worse, a successful link that silently omits new code)
would show the difference. This makes that drift a `make test` failure
instead.

Usage: check_android_sources_match.py <app-dir> [path/to/Frogg3rs.jucer]

<app-dir> is the repository's app/ directory (as every other check_*.py in
this directory takes it). The .jucer path defaults to
<app-dir>/android/Frogg3rs.jucer, and can be overridden -- to point this
script at a scratch copy of the project file instead, so a Sheaf source line
deleted from that copy is confirmed to fail this check without touching the
real project file.
"""

import os
import sys
import xml.etree.ElementTree as ET

NAME = "check-android-sources-match"

SHEAF_SYNTH_RELPATH = os.path.join("External", "Sheaf", "projects", "synth")
SHEAF_SRC_RELPATH = os.path.join(SHEAF_SYNTH_RELPATH, "src")
HOST_DATA_PATHS_RELPATH = os.path.join(SHEAF_SYNTH_RELPATH, "runtime", "HostDataPaths.cpp")


def expected_sources(repo_root):
    """The set of Sheaf source paths (repo-root-relative, forward-slashed)
    every other Sheaf synth app already builds from: every
    External/Sheaf/projects/synth/src/*.cpp, plus runtime/HostDataPaths.cpp."""
    src_dir = os.path.join(repo_root, SHEAF_SRC_RELPATH)
    sources = set()
    for name in os.listdir(src_dir):
        if name.endswith(".cpp"):
            sources.add(os.path.join(SHEAF_SRC_RELPATH, name).replace(os.sep, "/"))
    sources.add(HOST_DATA_PATHS_RELPATH.replace(os.sep, "/"))
    return sources


def jucer_sheaf_sources(jucer_path, repo_root):
    """The set of Sheaf source paths (repo-root-relative, forward-slashed)
    the .jucer's own <FILE> entries name, resolved from each FILE's `file`
    attribute against app/android/ (Projucer's universal convention is
    "relative to the .jucer file's own directory", and app/android/ is
    where this project file actually lives) -- NOT against wherever
    `jucer_path` itself physically sits, so a scratch copy of the .jucer
    can be checked (its content is what a red control mutates) without
    also having to reproduce the whole repository layout around it. Only
    entries whose resolved path falls under External/Sheaf/projects/synth/
    are considered -- app/FroggersMain.cpp and Resources/Icon.png are not
    part of what this check compares."""
    jucer_dir = os.path.join(repo_root, "app", "android")

    tree = ET.parse(jucer_path)
    sources = set()
    for file_el in tree.getroot().iter("FILE"):
        rel = file_el.get("file")
        if rel is None:
            continue
        resolved = os.path.normpath(os.path.join(jucer_dir, rel))
        repo_relative = os.path.relpath(resolved, repo_root).replace(os.sep, "/")
        if repo_relative.startswith(SHEAF_SYNTH_RELPATH.replace(os.sep, "/") + "/"):
            sources.add(repo_relative)

    return sources


def main(argv):
    if len(argv) < 2:
        print(f"usage: {argv[0]} <app-dir> [path/to/Frogg3rs.jucer]", file=sys.stderr)
        return 2

    app_dir = os.path.abspath(argv[1])
    repo_root = os.path.dirname(app_dir)
    jucer_path = argv[2] if len(argv) > 2 else os.path.join(app_dir, "android", "Frogg3rs.jucer")

    if not os.path.isfile(jucer_path):
        print(f"{NAME}: no such .jucer file: {jucer_path}", file=sys.stderr)
        return 2

    actual = jucer_sheaf_sources(jucer_path, repo_root)
    expected = expected_sources(repo_root)

    missing = sorted(expected - actual)
    extra = sorted(actual - expected)

    if not missing and not extra:
        print(f"{NAME}: {jucer_path} matches {SHEAF_SRC_RELPATH}/*.cpp plus HostDataPaths.cpp "
              f"({len(expected)} sources)")
        return 0

    print(f"{NAME}: {jucer_path}'s Sheaf sources do not match {SHEAF_SRC_RELPATH}/*.cpp "
          f"plus HostDataPaths.cpp", file=sys.stderr)
    for path in missing:
        print(f"  missing from the .jucer: {path}", file=sys.stderr)
    for path in extra:
        print(f"  in the .jucer but not on disk: {path}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
