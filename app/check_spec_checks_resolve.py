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
a citation somebody checked.

Resolving is necessary but not sufficient. A path can exist and still be
nothing to a reader deciding whether the requirement above it is backed by
anything: a Check: line is evidence only once it also has a real case, a file
that DEFINES one, or a gate that runs on its own -- everything else it names
is context, carried for the reader rather than for this script.

Where a Check: names a file that DEFINES cases, it must also name a case that
file defines. The pairing is read off the citation's shape: the compound
`app/Foo Tests.cpp: some_case` holds the case to that token's file, and the
separate-backtick style takes a case from anywhere on the line. A real case
name beside a file that never defined it reads as backed, and so does that
same file named with no case anywhere on the line.

The case index itself reaches app/ and External/Sheaf, over the extensions a
test can be written in. openspec/ is specs and config in extensions the walk
never parses, and the frozen firmware tree under src/ carries those
extensions but defines no test macro anywhere in it -- widening the walk to
either tree adds zero entries, checked by reading both before choosing. So a
file's case set is whatever the index recorded for it, empty when the index
never reached it or found nothing there, and pairing holds any file with a
non-empty set to it, Sheaf included, not only app/.

A file with an empty case set can still resolve the line: this directory's
own `test` Makefile target runs a handful of scripts as gates in their own
right, where running the script IS the check rather than a stand-in for one.
A Check: naming one of those resolves on that alone. Which scripts qualify is
read from the Makefile's own `test:` recipe, not matched against the
filenames the check scripts happen to share -- a same-named script nothing in
`test` runs is not a gate, and this asks the Makefile rather than guessing
from a name. Any other file a Check: line names -- a DSP header cited as the
expression under test, a doc, a Makefile -- is context: real, but neither a
case-defining file nor a wired gate, so it neither passes nor fails the line
by itself. A line that reaches none of a real case, a wired gate, or the
declared-manual marker below has named only context, which is not a check.

COMMENT TEXT DEFINES NOTHING. The index reads each file with its comment
regions blanked out first, so a name that appears only inside a comment is not
a name the tree defines. `// TEST_CASE(gone)` otherwise indexes exactly like a
case the file runs, and a citation naming it resolves against nothing, which is
the defect this gate exists to reject. The bare-function form is the same hole
one move further along: its rule is a declaration plus a second mention of the
name, and a commented-out call is a mention, so the declaration alone would be
enough. `//` and `/* */` are the comment forms these trees use -- .cpp, .hpp,
.ts, .mjs and .js have no `#` comment. Blanking tracks string and character
literals so their contents survive: a test name is a quoted string in the
JavaScript form, and `"http://host/path"` is a path, not a comment.

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

# A run of Sheaf's UI and runtime test files skip TEST_CASE entirely and
# declare a test as a bare top-level function instead -- `void TestFoo()` in
# most of them, `static void TestFoo()` in a couple. The declaration alone
# does not make it a test: a same-shaped function nothing calls again would
# read as one for free. What makes it one is a second reference to the same
# name elsewhere in the file, because every file that uses this form invokes
# every such function it defines, either directly (`TestFoo();`, most files)
# or by handing the name and the function to a small runner
# (`Run("TestFoo", TestFoo);`, runtime_main_component_tests.cpp) -- both wire
# the name to something that executes it, and a plain word-boundary search
# for the name a second time catches either. The search runs over the file
# with its comments blanked, so the second mention has to be code: a
# commented-out call executes nothing and would otherwise make a declaration
# self-sufficient.
BARE_TEST_FN = re.compile(r"^(?:static\s+)?void\s+(Test[A-Za-z0-9_]*)\s*\(\s*\)\s*$",
                          re.MULTILINE)

IDENTIFIER_CHAR = re.compile(r"[A-Za-z0-9_]")


def blank_comment_regions(text, template_strings):
    """The file's text with every comment region blanked to spaces, character
    for character so line structure and offsets are unchanged.

    A name that only ever appears in a comment names no test, so nothing read
    out of a comment reaches the index. String and character literals are
    tracked and kept: a `//` inside `"http://host/path"` opens no comment, and
    the JavaScript declaration form puts the test's name in a quoted string.
    A `'` straight after a letter or digit is C++'s digit separator rather than
    the start of a character literal, which is what `1'000'000` is, and reading
    it as a literal swallows the code after it. Backticks delimit a string only
    in the languages that have template strings.
    """
    out = list(text)
    i = 0
    n = len(text)
    while i < n:
        char = text[i]
        if char == "/" and i + 1 < n and text[i + 1] in "/*":
            if text[i + 1] == "/":
                end = text.find("\n", i)
                end = n if end < 0 else end
            else:
                end = text.find("*/", i + 2)
                end = n if end < 0 else end + 2
            for k in range(i, end):
                if out[k] != "\n":
                    out[k] = " "
            i = end
            continue
        opens_literal = (
            char == '"'
            or (char == "`" and template_strings)
            or (char == "'" and not (i and IDENTIFIER_CHAR.match(text[i - 1])))
        )
        if opens_literal:
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == char:
                    i += 1
                    break
                if text[i] == "\n" and char != "`":
                    break
                i += 1
            continue
        i += 1
    return "".join(out)


def tests_by_file(repo):
    """Which cases live in which file, so a `Check:` naming both can be held to
    the pair rather than to each half separately. A real case name beside the
    wrong file is a false citation, and checking them independently blesses it.

    Three declaration forms count: `TEST_CASE(name)`, `test("name")` /
    `it("name")`, and a bare `[static] void TestFoo()` that something in the
    same file calls again by name (see BARE_TEST_FN) -- a case is whichever
    of these the file's own text shows running, not merely defining.

    Every one of them is read from the file with its comments blanked out, so
    a declaration written in a comment declares nothing and a commented-out
    call is not the second mention the bare form needs.

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
                text = blank_comment_regions(read(full), full.endswith((".ts", ".mjs", ".js")))
            except OSError:
                continue
            found = set(TEST_CASE.findall(text)) | {m.group(2) for m in JS_TEST.finditer(text)}
            for name in BARE_TEST_FN.findall(text):
                if len(re.findall(r"\b" + re.escape(name) + r"\b", text)) >= 2:
                    found.add(name)
            if found:
                per_file[os.path.relpath(full, repo)] = found
    return per_file


SEARCH_ROOTS = ("app", "openspec", "External/Sheaf", "src")

MAKE_RULE = re.compile(r"^([A-Za-z][A-Za-z0-9_.-]*)\s*:(?!=)(.*)$")
GATE_RECIPE_PATH = re.compile(r"\$\(APP_DIR\)/([^\s\"']+\.(?:py|sh|cpp))")


def gate_scripts(app_dir, repo):
    """Paths the `test` Makefile target runs as a gate in their own right --
    a script whose exit code IS the check, the way a compiled test binary's
    exit code is, rather than a file whose cases this script's own index
    reads.

    Read mechanically from the Makefile, not matched against a filename: the
    `test:` rule's own prerequisite list names every gate target, other than
    the `$(VAR)`-built test binaries, which the case index already covers
    because their source defines the cases directly. Each named target's own
    recipe lines are searched for a literal `$(APP_DIR)/name` -- every check
    script this Makefile runs is invoked that way. A target that reaches its
    script only through another variable's build output, rather than a
    literal path in its own recipe, is not chased further; none of this
    Makefile's check targets do that today. A script that merely looks like a
    check but that `test` does not depend on must not resolve a citation, and
    a real gate must resolve however it happens to be named -- a filename
    pattern gets both of those backwards.
    """
    try:
        lines = read(os.path.join(app_dir, "Makefile")).splitlines()
    except OSError:
        return set()

    rules = {}
    for i, line in enumerate(lines):
        m = MAKE_RULE.match(line)
        if not m:
            continue
        block = [m.group(2)]
        for follow in lines[i + 1:]:
            if not follow.startswith("\t"):
                break
            block.append(follow)
        rules.setdefault(m.group(1), []).append("\n".join(block))

    prereqs = rules.get("test", [""])[0].split()
    targets = [p for p in prereqs if not p.startswith("$(")]

    app_rel = os.path.relpath(app_dir, repo)
    scripts = set()
    for target in targets:
        for block in rules.get(target, []):
            for name in GATE_RECIPE_PATH.findall(block):
                scripts.add(os.path.normpath(os.path.join(app_rel, name)))
    return scripts


FILES = set()
BY_FILE = {}
GATE_SCRIPTS = set()


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

    global FILES, BY_FILE, GATE_SCRIPTS
    FILES = path_index(repo, SEARCH_ROOTS)
    BY_FILE = tests_by_file(repo)
    GATE_SCRIPTS = gate_scripts(app_dir, repo)
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
                # by anyone. A file that DEFINES cases is held to one, whatever
                # tree it sits in -- a citation is a claim about one specific
                # file, and the tree it sits in does not change what the
                # sentence asserts. A file that defines none is not handled
                # here: it falls through to the context/gate check below.
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
                    for f in [p for p in parts if p in BY_FILE]:
                        file_cases = BY_FILE.get(f, set())
                        stray = [c for c in inside if c not in file_cases]
                        if stray:
                            pairing.append(f"{rel}:{n} pairs `{stray[0]}` with {f}, "
                                           f"which does not define it")
                        elif not line_cases & file_cases:
                            pairing.append(f"{rel}:{n} names {f} but no case defined in it; "
                                           f"backtick a case name from that file")
                if pairing:
                    errors += pairing
                    continue

                # A line can carry nothing but context and still reach here:
                # the pairing block above only ever fires for a file that
                # DEFINES cases, so a Check: naming solely a DSP header, a
                # doc, or a script `test` never runs passes through it
                # untouched. What makes a line a check is a real case
                # somewhere on it, a gate `test` actually runs somewhere on
                # it, or the declared-manual marker -- short of all three,
                # everything named is context, and context is not a check.
                gate_here = any(p in GATE_SCRIPTS for t in claims for p in split_parts(t))
                if not declared and not line_cases and not gate_here:
                    shown = ", ".join(f"`{t}`" for t in claims[:3])
                    errors.append(f"{rel}:{n} names only context, no case and "
                                  f"no wired gate: {shown}")
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
