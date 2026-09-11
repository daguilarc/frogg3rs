#!/usr/bin/env python3
"""check_spec_checks_resolve.py -- fails when a spec's `Check:` line names a
test or a file that does not exist.

A specification records what the system DOES, and a `Check:` line is the
evidence for that. It is also the cheapest false claim available anywhere in
this repository: nothing parses prose, so `Check: the stage-independence case`
costs one line to write and reads as true forever. One shipped exactly that way
-- an ADDED requirement whose named test existed nowhere in the tree, by name or
by content -- and it survived every pass that trusted it.

An honest gap stays sayable. A `Check:` whose text begins `none` or
`operator step` passes and is counted, because two promoted scenarios already
depend on that being legal: some things really are checked by a person.

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
from check_common import read, walk_all, walk_sources  # noqa: E402

NAME = "check-spec-checks-resolve"

CHECK_LINE = re.compile(r"^\s*-\s*Check:\s*(?P<body>.+)$")
BACKTICKED = re.compile(r"`([^`]+)`")
TEST_CASE = re.compile(r"TEST_CASE\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)")
# `File.cpp: some_test_name` and bare identifiers both appear in practice.
IDENT = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
NO_CHECK = re.compile(r"^\s*(none|operator step)", re.I)
# A token is evidence if it looks like a path or a test name: it carries a
# separator, or it is snake_case. A bare CamelCase or lowercase word is part of
# the sentence -- `scrollWidth`, `BlockStartPos` -- and is not required to
# resolve to anything.
NAMES_SOMETHING = re.compile(r"[/.]|_")
# Only app/ test files get the name-the-case rule. Sheaf's and the browser
# suites' cases are not indexed reliably enough from here to demand it, and 17
# promoted Check: lines name them; tightening that is its own change, not a
# side effect of this one.
APP_TEST_FILE = re.compile(r"^app/.*[Tt]ests?\.cpp$")


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
# Browser end-to-end names come from `test("...")` in .spec.ts/.mjs.
JS_TEST = re.compile(r"""\b(?:test|it)\(\s*['"`]([^'"`]+)""")


def tests_by_file(repo):
    """Which cases live in which file, so a `Check:` naming both can be held to
    the pair rather than to each half separately. A real case name beside the
    wrong file is a false citation, and checking them independently blesses it."""
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
            found = set(TEST_CASE.findall(text)) | set(JS_TEST.findall(text))
            if found:
                per_file[os.path.relpath(full, repo)] = found
    return per_file


def known_test_names(repo):
    names = set()
    for base in ("app", "External/Sheaf"):
        root_dir = os.path.join(repo, base)
        if not os.path.isdir(root_dir):
            continue
        for full in walk_sources(root_dir, (".cpp", ".hpp", ".ts", ".mjs", ".js")):
            try:
                text = read(full)
            except OSError:
                continue
            names.update(TEST_CASE.findall(text))
            names.update(JS_TEST.findall(text))
    return names


def file_index(repo):
    """Every filename in the tree, so a Check: may name a path or a basename."""
    names = set()
    for base in ("app", "openspec", "External/Sheaf", "src"):
        root_dir = os.path.join(repo, base)
        if not os.path.isdir(root_dir):
            continue
        for full in walk_all(root_dir):
            names.add(os.path.basename(full))
            names.add(os.path.relpath(full, repo))
    return names


FILES = set()
BY_FILE = {}


def is_claim(tok):
    """Does this backticked token assert the existence of something?

    Yes if it carries a path or test-name separator, or if it happens to name a
    real file -- `Makefile` has neither a slash, a dot nor an underscore and is
    still evidence. No for a bare CamelCase or lowercase word, which belongs to
    the sentence: `scrollWidth`, `clientWidth`, `BlockStartPos`.
    """
    tok = tok.strip()
    return bool(NAMES_SOMETHING.search(tok)) or tok in FILES


def part_resolves(part, repo, tests):
    """One colon-separated piece of a token names a real test or a real file."""
    if part in tests:
        return True
    if part in FILES:
        return True
    if os.path.exists(os.path.join(repo, part)):
        return True
    # A binary named without its extension, e.g. `blocks_tests`.
    if any(f == part or f.startswith(part + ".") for f in FILES):
        return True
    # A path given relative to a sub-project root rather than the repo --
    # `browser/tests/ui-backend.spec.ts` for a file that actually lives at
    # External/Sheaf/projects/synth/browser/tests/ui-backend.spec.ts.
    if "/" in part and any(f.endswith("/" + part) for f in FILES):
        return True
    return False


def resolves(tok, repo, tests):
    """EVERY named part of a token must resolve, not merely one of them.

    The compound form `File.cpp: some_case_name` is the dominant style in this
    repository's specs, and an earlier version of this function returned true on
    the first part that resolved and never looked at the rest. That passed a
    real file beside a fictional case -- which is the exact bug this script was
    written to catch, reproduced against the very format most specs use. Found
    by a postflight audit constructing its own evasions rather than re-reading
    the ones already fixed.
    """
    parts = [p.strip() for p in tok.split(":") if p.strip()]
    named = [p for p in parts if is_claim(p)]
    if not named:
        return False
    return all(part_resolves(p, repo, tests) for p in named)


def main():
    if len(sys.argv) != 2:
        print(f"{NAME}: FAIL - usage: check_spec_checks_resolve.py <app-dir>", file=sys.stderr)
        return 2
    app_dir = os.path.abspath(sys.argv[1])
    repo = os.path.dirname(app_dir)

    global FILES, BY_FILE
    FILES = file_index(repo)
    BY_FILE = tests_by_file(repo)
    tests = known_test_names(repo)
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

                # A `none` / `operator step` prefix declares that no automated
                # check exists. It does NOT license whatever follows: an earlier
                # version returned immediately on seeing it, so `Check: none,
                # see `a_fake_case`` passed with a false claim attached. The
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
                # A token is a claim if its shape says so, OR if it happens to
                # name a real file -- `Makefile` carries no separator and no
                # underscore, and is still evidence.
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
                bad = [t for t in claims if not resolves(t, repo, tests)]
                if bad:
                    shown = ", ".join(f"`{t}`" for t in bad[:3])
                    errors.append(f"{rel}:{n} names nothing that resolves: {shown}")
                    continue

                # Naming a test FILE is not evidence on its own: these files
                # run to thousands of lines and hundreds of cases, so "the
                # stage-independence case" in prose beside one cannot be
                # checked by anyone. Where the file is ours, the case must be
                # named too.
                named_app_files = [t.strip() for t in claims if APP_TEST_FILE.match(t.strip())]
                if named_app_files:
                    cited = {p.strip() for t in tokens for p in t.split(":")}
                    ok = False
                    for f in named_app_files:
                        # The case must live in THE FILE NAMED BESIDE IT. A real
                        # case name belonging to a different file passed before,
                        # because each half was checked on its own.
                        if cited & BY_FILE.get(f, set()):
                            ok = True
                    if not ok:
                        owners = ", ".join(named_app_files)
                        errors.append(f"{rel}:{n} names {owners} but no TEST_CASE defined in it; "
                                      f"backtick a case name from that file")
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
