#!/usr/bin/env python3
"""check_spec_checks_resolve.py -- fails when a spec's `Check:` line names a
test or a file that does not exist.

A specification records what the system DOES, and a `Check:` line is the
evidence for that. It is also the cheapest false claim available anywhere in
this repository: nothing parses prose, so `Check: the stage-independence case`
costs one line to write and reads as true forever. One shipped exactly that way
-- an ADDED requirement whose named test existed nowhere in the tree, by name or
by content -- and it survived every pass that trusted it.

WHAT COUNTS AS EVIDENCE. A backticked token resolves only if every named piece
of it is positively identified: a test-case name read verbatim out of the tree,
or a repo-relative path indexed verbatim from the tree. Nothing else resolves --
not a bare basename, not a path relative to a sub-project root, not a name with
its extension dropped, not a path the filesystem happens to have. Each of those
guesses which real thing a line meant, and a guess that lands reads exactly like
a citation somebody checked. The accepting surface is the thing that has to stay
small enough to enumerate, so it is two rules and they are both exact.

Where a Check: names one of this repository's own test files, it must also name
a case that file DEFINES. The pairing is read off the citation's shape, so the
compound `app/Foo Tests.cpp: some_case` holds the case to that token's file
while the separate-backtick style takes a case from anywhere on the line. Held
apart, a real case name beside a file that never defined it reads as backed.

An honest gap stays sayable. A `Check:` whose text begins `none`,
`operator step`, or `not yet delivered` passes and is counted, because
promoted scenarios and in-flight requirements both depend on that being
legal: some things really are checked by a person, and some are checked by
a test that does not exist until its own change lands.

SCOPE IS SPEC FILES, NOT CHANGE DIRECTORIES. A change directory also holds its
preflight and postflight reports, which QUOTE `Check:` lines -- including ones
already dropped from the live spec -- and scanning those fails the build on text
nobody is asserting. Archived changes are excluded too: they record what was
true when they shipped, and their tests may since have been renamed.

Usage: check_spec_checks_resolve.py <app-dir>
The repository root is that argument's parent, matching the one-argument,
cwd-independent convention every other check script in this directory uses.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_common import path_index, read, walk_all, walk_sources  # noqa: E402

NAME = "check-spec-checks-resolve"

CHECK_LINE = re.compile(r"^\s*-\s*Check:\s*(?P<body>.+)$")
BACKTICKED = re.compile(r"`([^`]+)`")
TEST_CASE = re.compile(r"TEST_CASE\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)")
NO_CHECK = re.compile(r"^\s*(none|operator step|not yet delivered)", re.I)
# A token is evidence if it looks like a path or a test name: it carries a
# separator, or it is snake_case. A bare CamelCase or lowercase word is part of
# the sentence -- `scrollWidth`, `BlockStartPos` -- and is not required to
# resolve to anything.
NAMES_SOMETHING = re.compile(r"[/.]|_")


def spec_files(repo):
    """Spec files only, and never an archived change. A change directory also
    holds its preflight and postflight reports, which QUOTE `Check:` lines --
    sometimes ones already dropped from the live spec -- and scanning those
    fails the build on text nobody is asserting. Archived changes record what
    was true when they shipped; their tests may since have been renamed."""
    out = []
    specs = os.path.join(repo, "openspec", "specs")
    if os.path.isdir(specs):
        out += [f for f in walk_all(specs) if os.path.basename(f) == "spec.md"]
    changes = os.path.join(repo, "openspec", "changes")
    if os.path.isdir(changes):
        for entry in sorted(os.listdir(changes)):
            if entry == "archive":
                continue
            base = os.path.join(changes, entry, "specs")
            if os.path.isdir(base):
                out += [f for f in walk_all(base) if os.path.basename(f) == "spec.md"]
    return sorted(out)


# Test names live in app/ AND in External/Sheaf: several promoted scenarios are
# checked by Sheaf's own suites, which is legitimate -- the app depends on them.
# Browser end-to-end names come from `test("...")` in .spec.ts/.mjs. The closing
# quote must be the one that opened, or a name reads only as far as its first
# apostrophe: `test("a stopped recording downloads under today's date")` indexes
# as "...under today", and the spec that cites the real name then cannot resolve
# it.
JS_TEST = re.compile(r"""\b(?:test|it)\(\s*(['"`])((?:\\.|(?!\1).)*)\1""")


def tests_by_file(repo):
    """Which cases live in which file, so a `Check:` naming both can be held to
    the pair rather than to each half separately. A real case name beside the
    wrong file is a false citation, and checking them independently blesses it.

    This is the only place case names are collected. The set of every name in
    the tree is the union of these, so reading the tree twice would be two
    indexes free to disagree about what exists."""
    per_file = {}
    for base in ("app", "External/Sheaf"):
        root_dir = os.path.join(repo, base)
        if not os.path.isdir(root_dir):
            continue
        for full in walk_sources(root_dir, (".cpp", ".hpp", ".ts", ".mjs", ".js")):
            try:
                text = read(full)
            except OSError:
                continue
            found = set(TEST_CASE.findall(text)) | {m.group(2) for m in JS_TEST.finditer(text)}
            if found:
                per_file[os.path.relpath(full, repo)] = found
    return per_file


SEARCH_ROOTS = ("app", "openspec", "External/Sheaf", "src")


FILES = set()
BY_FILE = {}
APP_CASE_FILES = set()


def is_claim(tok):
    """Does this backticked token assert the existence of something?

    Yes if it carries a path or test-name separator: a slash, a dot, or an
    underscore. No for a bare CamelCase or lowercase word, which belongs to
    the sentence -- `scrollWidth`, `clientWidth`, `BlockStartPos` -- and no
    for a plain word with none of those marks either, like `Makefile` on its
    own: it names a mechanism, not one specific file, and resolving it would
    have to guess which of the many files sharing that name is meant.
    """
    return bool(NAMES_SOMETHING.search(tok.strip()))


def split_parts(tok):
    """The colon-separated pieces of one backticked token, whitespace trimmed.
    `app/Foo Tests.cpp: some_case` is two pieces, a file and a case name."""
    return [p.strip() for p in tok.split(":") if p.strip()]


def part_resolves(part, tests):
    """One piece of a token is evidence only if it is positively identified.

    Exactly two things count: a test-case name collected verbatim from the
    tree, and a repo-relative path indexed verbatim from the tree. Every other
    spelling is rejected, including ones that are nearly right -- a bare
    basename, a path relative to a sub-project root, a name with its extension
    dropped. Each of those has to GUESS which real thing was meant, and a guess
    that lands is indistinguishable from a citation that was never checked.
    The gate exists to reject prose that reads as evidence, so anything it
    cannot identify exactly is prose.
    """
    return part in tests or part in FILES


def resolves(tok, tests):
    """EVERY named part of a token must resolve, not merely one of them.

    The compound form `File.cpp: some_case_name` is the dominant style in this
    repository's specs, and an earlier version of this function returned true on
    the first part that resolved and never looked at the rest. That passed a
    real file beside a fictional case -- which is the exact bug this script was
    written to catch, reproduced against the very format most specs use.
    """
    named = [p for p in split_parts(tok) if is_claim(p)]
    if not named:
        return False
    return all(part_resolves(p, tests) for p in named)


def main():
    if len(sys.argv) != 2:
        print(f"{NAME}: FAIL - usage: check_spec_checks_resolve.py <app-dir>", file=sys.stderr)
        return 2
    app_dir = os.path.abspath(sys.argv[1])
    repo = os.path.dirname(app_dir)

    global FILES, BY_FILE, APP_CASE_FILES
    FILES = path_index(repo, SEARCH_ROOTS)
    BY_FILE = tests_by_file(repo)
    # The files the case-naming rule covers are the ones this repository owns
    # and this walk actually read cases out of, not the ones whose names look
    # like test files. External/Sheaf is a pinned submodule and its suites are
    # indexed less completely, so a Check: naming one is held to the file only.
    APP_CASE_FILES = {f for f in BY_FILE if f.startswith("app/")}
    tests = set().union(*BY_FILE.values())
    errors = []
    resolved = 0
    declared_manual = 0

    for spec in spec_files(repo):
        rel = os.path.relpath(spec, repo)
        with open(spec, "r", encoding="utf-8", errors="replace") as fh:
            for n, line in enumerate(fh, 1):
                m = CHECK_LINE.match(line)
                if not m:
                    continue
                body = m.group("body").strip()
                tokens = BACKTICKED.findall(body)
                declared = bool(NO_CHECK.match(body))

                # A `none` / `operator step` / `not yet delivered` prefix
                # declares that no automated check exists yet. It does NOT
                # license whatever follows: an earlier version returned
                # immediately on seeing it, so `Check: none, see
                # `a_fake_case`` passed with a false claim attached. The
                # declaration is accepted; anything it names is still resolved.
                if declared and not tokens:
                    declared_manual += 1
                    continue
                if not tokens:
                    errors.append(f"{rel}:{n} names nothing checkable: {body[:80]}")
                    continue
                # A Check: line is prose and carries two kinds of backticked
                # token. Some are the evidence -- a path, or a test name. Some
                # belong to the sentence: `scrollWidth`, `clientWidth`,
                # `BlockStartPos`. Only the first kind has to resolve, told
                # apart by shape (see NAMES_SOMETHING).
                #
                # An earlier version passed a line when ANY token resolved.
                # That reopened the exact hole this script exists to close: a
                # real file plus a fictional case name passed, which is the
                # precise shape of the requirement that shipped naming a test
                # that existed nowhere. Caught by the postflight audit, on the
                # script's own motivating example.
                claims = [t for t in tokens if is_claim(t)]
                if not claims:
                    # Every token is sentence-shaped, so the line names no
                    # evidence at all. Passing this let a fictional CamelCase
                    # case name through on its own.
                    if declared:
                        declared_manual += 1
                        continue
                    shown = ", ".join(f"`{t}`" for t in tokens[:3])
                    errors.append(f"{rel}:{n} names no test and no file, only prose: {shown}")
                    continue
                bad = [t for t in claims if not resolves(t, tests)]
                if bad:
                    shown = ", ".join(f"`{t}`" for t in bad[:3])
                    errors.append(f"{rel}:{n} names nothing that resolves: {shown}")
                    continue

                # Naming a test FILE is not evidence on its own: these files run
                # to thousands of lines and hundreds of cases, so "the
                # stage-independence case" in prose beside one cannot be checked
                # by anyone. Where the file is one this repository owns, a case
                # must be named too, and it must be a case that file DEFINES.
                #
                # The pairing is read off the citation's own shape, so every
                # shape answers to it. `app/Foo Tests.cpp: some_case` pairs the
                # two inside one token, and the case is held to that token's
                # file. The separate-backtick style pairs them across the line,
                # so the case may come from anywhere on it. Checking the two
                # halves independently -- which is what naming the file and the
                # case separately amounts to -- blesses a real case name sitting
                # beside a file that has never defined it.
                line_cases = {p for t in tokens for p in split_parts(t) if p in tests}
                pairing = []
                for tok in claims:
                    parts = split_parts(tok)
                    inside = [p for p in parts if p in tests]
                    for f in [p for p in parts if p in APP_CASE_FILES]:
                        stray = [c for c in inside if c not in BY_FILE[f]]
                        if stray:
                            pairing.append(f"{rel}:{n} pairs `{stray[0]}` with {f}, "
                                           f"which does not define it")
                        elif not line_cases & BY_FILE[f]:
                            pairing.append(f"{rel}:{n} names {f} but no case defined in it; "
                                           f"backtick a case name from that file")
                if pairing:
                    errors += pairing
                    continue
                if declared:
                    declared_manual += 1
                else:
                    resolved += 1

    if errors:
        for e in errors:
            print(f"{NAME}: FAIL - {e}", file=sys.stderr)
        print(f"{NAME}: FAIL - {len(errors)} unresolvable Check reference(s)", file=sys.stderr)
        return 1

    print(f"{NAME}: OK - {resolved} Check reference(s) resolved, "
          f"{declared_manual} declared as having no automated check")
    return 0


if __name__ == "__main__":
    sys.exit(main())
