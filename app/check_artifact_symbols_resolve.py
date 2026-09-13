#!/usr/bin/env python3
"""check_artifact_symbols_resolve.py -- fails when a backticked token in a live
change's two planning files (ARTIFACTS below) names code the tree does not have.

Those files are where a change states what the code is and what it will become,
and every name in them is a claim. Nothing else reads them: the citation gate
reads C++ comments and the spec gate reads `Check:` lines. So naming a method
that moved, a path that was never created, or a class that does not own the
member it is credited with costs one line to write and reads as true for as
long as the change lives.

WHAT COUNTS AS A CLAIM. These files are prose, and most of what they backtick
is not code: shell commands, git refs and short shas, literal fragments quoted
out of another document, knob labels, algebra, and ordinary English. So a token
is a claim only when its SHAPE is one a sentence does not produce, and each
shape has exactly one way to resolve:

  app/dsp/Reverb.hpp      a path spelled from the repository root into an
                          indexed tree, which must be a file or a directory
                          there, with or without a trailing slash
  dsp::Reverb::Process    a qualified name, whose last segment must be a member
                          declared by the scope the rest of it names
  Reset()                 a bare name written as a call, which must be declared
                          somewhere in the indexed trees
  kStageCeiling           a k-prefixed constant, same
  some_case_name          a lower snake_case name, which must be a test case or
                          a symbol declared in C++

No Python declaration is indexed. These documents name scripts by path, and a
bare function name does not say which script's it is, so a snake_case token is
resolved as a test case or not at all.

A QUALIFIED NAME IS CHECKED AS A QUALIFICATION, which is the shape this gate is
mostly for. `Foo::bar` asks whether `bar` is a member of `Foo`, not whether the
two names exist somewhere. Membership is by DECLARING scope and inheritance is
not followed: a name belongs to the class that declares it, which is what a
sentence crediting a member to a class means. Writing the same claim as two
separate tokens -- "`bar` is a private member of `Foo`" -- is not checkable by
this or any other mechanism, so the qualified spelling is the cheap one.

WHAT IS NOT A CLAIM, and why the line is drawn there. A bare CamelCase word is
prose to this gate. `Density`, `Diff`, `Gain` and `Step` are knob labels and
short names; `RouteFilterBank` and `RouteDriveBank` are code; nothing lexical
separates them, and a gate that rejected the first four would be worked around
rather than fixed. A token with no `/` is not read as a path either: a bare
filename does not say which of the tree's several same-named files is meant,
and these documents legitimately name documents that live outside the
repository. Expressions, member accesses through a local (`peak.height`), and
anything carrying a space are prose.

A SPAN MAY HOLD MORE THAN ONE CLAIM. `app/Foo Tests.cpp: some_case` is a path
and a case name, and the same span shape carries a file beside a fabricated
case. So a span carrying whitespace is split and every piece is resolved on its
own shape; colons are kept, since they are what makes a qualified name one
claim rather than two.

FORWARD REFERENCES. A change describes work not yet done, so a name that does
not resolve may be one the change is about to create. The separator is an
explicit declaration in the change's own text: a token is exempt when its own
artifacts write it directly after `NEW` or `Name it`, or directly before `does
not exist`. One declaration anywhere in the change exempts that token throughout
it. The markers are emphatic on purpose -- lowercase `new` reads as ordinary
English and would exempt whatever a passing sentence happened to backtick.

A checkbox was considered as the separator and does not work. A ticked line in
this tree names a constant the change decided NOT to ship and reports that
decision as its outcome, and the second file has no checkboxes at all yet names
a header and a type the change will add. Both are honest, and both would fail.
What distinguishes a forward reference from a dangling one is that the document
says so, so the document is what is read.

WHAT THIS CANNOT DO. It resolves NAMES. It cannot tell whether a claim about a
real name is true. A sentence asserting that a real method does something it
does not do, is called from somewhere it is not, holds a value it does not hold,
or belongs to a layer it does not belong to passes this gate untouched, and so
does a correct qualification attached to a false statement about what the member
does. It also does not check prose-shaped tokens at all, by the rule above, so a
bare misspelled identifier passes; it reads declarations rather than compiling,
so a scope it fails to parse yields an empty member set and reports every
qualification into it as unresolved rather than silently accepting; and an
exemption lasts as long as the change does, including after the thing is
created, and a declaration exempts the name even where a later sentence claims
it already exists. A snake_case token that names a helper inside one of the
check scripts is rejected, because no Python declaration is indexed; write the
script's path instead.

Known and left alone: a stray backtick hides that ONE line's spans, and a span
written in Markdown's doubled-backtick form is not read; a directory is resolved
against the filesystem rather than the index, so a path into a build tree
resolves where a file inside it would not; only the two files in ARTIFACTS are
read, so a claim in a change's other documents is not checked; and a qualified
CALL in the trees grants membership, which is sound in C++ except through a
base class.

Usage: check_artifact_symbols_resolve.py <app-dir>
The repository root is that argument's parent, matching the one-argument,
cwd-independent convention every other check script in this directory uses.
"""

import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_common import path_index, read, walk_sources  # noqa: E402
from check_spec_checks_resolve import blank_comment_regions, tests_by_file  # noqa: E402

NAME = "check-artifact-symbols-resolve"

ARTIFACTS = ("tasks.md", "proposal.md")

# The trees whose paths are indexed and whose declarations are read. A path
# token is a claim only when its first segment is one of these, so a fragment
# quoted as another file spells it -- an include's `dsp/Delay.hpp` -- is prose
# rather than a path this gate can look up.
PATH_ROOTS = ("app", "openspec", "External/Sheaf", "src")
SOURCE_EXT = (".cpp", ".hpp", ".h", ".mm")

# A backticked span never crosses a line here, and holding it to one line is
# what keeps a stray backtick from re-pairing every span after it in the file:
# the damage stops at that line instead of silently switching the rest off.
BACKTICKED = re.compile(r"`([^`\n]+)`")
# A span may hold several claims -- the citation style this repository already
# uses elsewhere is `path: case_name` -- so a span carrying whitespace is split
# and each piece is resolved on its own shape. Colons are not split on, because
# they are what makes a qualified name one claim rather than two.
SPAN_SPLIT = re.compile(r"[\s,;]+")
TRIM = ".,;:()[]'\"" + "`"

ID = r"[A-Za-z_][A-Za-z0-9_]*"
# A path is spelled in segments that each begin with a letter, digit or
# underscore. An ellipsis standing in for the middle of a path is therefore not
# a path, and neither is anything carrying a space.
SEGMENT = r"[A-Za-z0-9_][A-Za-z0-9_.-]*"
PATH_TOKEN = re.compile(r"^" + SEGMENT + r"(?:/" + SEGMENT + r")+$")
DIR_TOKEN = re.compile(r"^" + SEGMENT + r"(?:/" + SEGMENT + r")*/$")
QUALIFIED = re.compile(r"^" + ID + r"(?:::" + ID + r")+$")
CALL_FORM = re.compile(r"^(" + ID + r")\(\)$")
CONSTANT = re.compile(r"^k[A-Z][A-Za-z0-9]*$")
SNAKE_CASE = re.compile(r"^[a-z][a-z0-9]*(?:_[a-z0-9]+)+$")

# A forward reference is declared by the change's own words, and the markers are
# deliberately emphatic. Lowercase `new` was tried and dropped: it reads as
# ordinary English -- "a new multiply in the path" -- so a sentence nobody meant
# as a declaration would exempt whatever it happened to backtick next, and every
# forward name in this tree is already declared by one of the three below.
# `Rename the parameter to X` is absent too: renames here name knob labels,
# which are prose to this gate anyway.
DECLARED_NEW = re.compile(r"(?:\bNEW|\bName it)\s+`([^`]+)`")
DECLARED_ABSENT = re.compile(r"`([^`]+)`\s+does not exist")

# Declaration shapes read out of the C++ trees. A scope's opening brace may sit
# on its own line and may follow a base clause, so the opener is matched across
# the gap rather than line by line.
SCOPE_OPEN = re.compile(
    r"\b(?P<kind>namespace|class|struct|union|enum\s+class|enum\s+struct|enum)\s+"
    r"(?P<name>" + ID + r"(?:\s*::\s*" + ID + r")*)"
    r"\s*(?:final\b\s*)?(?::[^;{}]*)?\{", re.S)
# An enumerator is a bare name in the body, with or without an initialiser, so
# it matches none of the declaration shapes a class body uses.
ENUMERATOR = re.compile(r"^\s*(?P<name>" + ID + r")\s*(?:=[^,]*)?,?\s*$")
# A constant declared inside a function body belongs to no scope a qualified
# name can reach, and the trees put several of the ones these documents cite
# there. They are recorded as existing without an owner.
LOCAL_CONST = re.compile(r"\b(?:constexpr|const)\s+(?:[A-Za-z_][A-Za-z0-9_:<>,\s*&]*?\s|\s*)"
                         r"(?P<name>" + ID + r")\s*(?:=[^=]|\{)")
# A line that opens a scope or changes access declares no member of its own.
SCOPE_LINE = re.compile(r"^\s*(?:template\b|namespace\b|class\b|struct\b|union\b"
                        r"|enum\b|public:|private:|protected:)")
DECL_FN = re.compile(r"(?P<name>" + ID + r")\s*\(")
DECL_VAR = re.compile(r"(?P<name>" + ID + r")\s*(?:\[[^\]]*\])?\s*(?:=[^=]|;|\{)")
OUT_OF_LINE = re.compile(r"\b(?P<scope>" + ID + r"(?:::" + ID + r")*)::(?P<name>" + ID + r")\s*\(")
# Control flow and type names read as declarations under the patterns above.
KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "catch", "else", "do",
            "case", "new", "delete", "static_cast", "const_cast", "reinterpret_cast",
            "dynamic_cast", "operator", "template", "typename", "noexcept", "decltype",
            "static_assert", "explicit", "using", "friend", "void", "int", "float",
            "double", "bool", "char", "auto", "const", "static", "inline", "struct",
            "class", "namespace", "enum", "union", "constexpr", "unsigned", "long",
            "short", "size_t", "throw", "try"}


def scope_keys(qualified):
    """Every suffix of a scope's full path, so `synth_froggers::dsp::Reverb` is
    reachable as `Reverb`, `dsp::Reverb` and in full. A sentence names a scope
    by whichever suffix reads clearly, and all of them mean the same scope."""
    parts = qualified.split("::")
    return ["::".join(parts[i:]) for i in range(len(parts))]


def declarations(repo):
    """Which names each scope declares, and the set of every declared name.

    The tree is read with comments blanked, so a name that appears only in a
    comment declares nothing. Scopes are tracked by brace depth: a member is a
    name declared on a line sitting directly inside a class, struct, enum or
    namespace body, plus any `Scope::name(` definition written out of line."""
    members = {}
    scopes = set()
    loose = set()

    def add(keys, name):
        for key in keys:
            members.setdefault(key, set()).add(name)

    for base in ("app", "External/Sheaf", "src"):
        root_dir = os.path.join(repo, base)
        if not os.path.isdir(root_dir):
            continue
        for full in walk_sources(root_dir, SOURCE_EXT):
            try:
                text = blank_comment_regions(read(full), False)
            except OSError:
                continue
            loose |= {m.group("name") for m in LOCAL_CONST.finditer(text)}
            opens = {m.end() - 1: (re.sub(r"\s+", "", m.group("name")),
                                   m.group("kind").startswith("enum"))
                     for m in SCOPE_OPEN.finditer(text)}
            lines = text.split("\n")
            starts, pos = [], 0
            for line in lines:
                starts.append(pos)
                pos += len(line) + 1

            # One pass over the braces records, for each line, the scope whose
            # body that line sits directly in. Reading the depth at the line's
            # START is what lets a declaration that itself opens a brace --
            # `float RouteFilterBank(float x) {` -- still count as a member.
            enclosing = []
            depth, stack, index = 0, [], 0
            for offset, char in enumerate(text):
                while index < len(starts) and starts[index] == offset:
                    inside = stack[-1][2:] if stack and depth == stack[-1][1] + 1 else ((), False)
                    enclosing.append(inside)
                    index += 1
                if char == "{":
                    if offset in opens:
                        name, is_enum = opens[offset]
                        qualified = "::".join([s[0] for s in stack] + [name])
                        keys = scope_keys(qualified)
                        for key in keys:
                            members.setdefault(key, set())
                            scopes.add(key)
                        if stack:
                            add(stack[-1][2], name.split("::")[0])
                        stack.append((name, depth, keys, is_enum))
                    depth += 1
                elif char == "}":
                    depth -= 1
                    if stack and stack[-1][1] == depth:
                        stack.pop()
            while len(enclosing) < len(lines):
                enclosing.append(((), False))

            for n, line in enumerate(lines):
                keys, is_enum = enclosing[n]
                if keys and is_enum:
                    m = ENUMERATOR.match(line)
                    if m and m.group("name") not in KEYWORDS:
                        add(keys, m.group("name"))
                elif keys and not SCOPE_LINE.match(line):
                    for m in DECL_FN.finditer(line):
                        if m.group("name") not in KEYWORDS:
                            add(keys, m.group("name"))
                    for m in DECL_VAR.finditer(line):
                        if m.group("name") not in KEYWORDS:
                            add(keys, m.group("name"))
                for m in OUT_OF_LINE.finditer(line):
                    add(scope_keys(m.group("scope")), m.group("name"))

    declared = set(scopes) | loose
    for names in members.values():
        declared |= names
    return members, scopes, declared


def change_artifacts(repo):
    """Both planning files of every live change, grouped by change. The archive
    records what was true when a change shipped, and the names it used may since
    have moved, so it is not scanned. A change directory's other documents are
    its preflight and postflight reports, which quote text nobody is asserting
    any more, so only the two files named in ARTIFACTS are read.

    A file is this gate's subject once it is TRACKED or STAGED -- that is, once
    it is about to enter the repository. An untracked, unstaged change is work
    nobody else can see, and a name it has not finished writing must not stop
    the build for everyone; staging it is the point at which its author is
    asking for it to become everyone's problem. Tracked alone is not enough: a
    brand new change directory is untracked until its first commit, so a gate
    reading only tracked files opens nothing and reports OK, which is a green
    that proves the gate ran rather than that the artifacts resolve."""
    out = []
    changes = os.path.join(repo, "openspec", "changes")
    if not os.path.isdir(changes):
        return out
    tracked = set()
    try:
        for args in (["ls-files", "openspec/changes"],
                     ["diff", "--cached", "--name-only", "--", "openspec/changes"]):
            listing = subprocess.run(["git", "-C", repo] + args,
                                     capture_output=True, text=True, timeout=30)
            if listing.returncode == 0:
                tracked |= {os.path.join(repo, line)
                            for line in listing.stdout.splitlines() if line}
    except (OSError, subprocess.SubprocessError):
        tracked = set()
    for entry in sorted(os.listdir(changes)):
        if entry == "archive":
            continue
        directory = os.path.join(changes, entry)
        if not os.path.isdir(directory):
            continue
        files = [os.path.join(directory, n) for n in ARTIFACTS
                 if os.path.isfile(os.path.join(directory, n))
                 and (not tracked or os.path.join(directory, n) in tracked)]
        if files:
            out.append((entry, files))
    return out


def declared_forward(texts):
    """Tokens the change says it is about to create or says are missing."""
    forward = set()
    for text in texts:
        flat = re.sub(r"\s+", " ", text)
        for pattern in (DECLARED_NEW, DECLARED_ABSENT):
            forward |= {m.group(1).strip() for m in pattern.finditer(flat)}
    return forward


def under_indexed_root(token):
    """Is this path spelled from the repository root, into a tree this gate
    indexes? A path spelled any other way -- the relative form an include uses,
    or a bare filename -- names no one file and is not looked up."""
    bare = token.rstrip("/")
    return any(bare == root or bare.startswith(root + "/") for root in PATH_ROOTS)


def claim_parts(span):
    """The pieces of one backticked span that can each carry a claim."""
    span = re.sub(r"\s+", " ", span).strip()
    pieces = SPAN_SPLIT.split(span) if " " in span else [span]
    return [p for p in (piece.strip(TRIM) for piece in pieces) if p]


def resolve(token, repo, paths, members, scopes, declared, tests):
    """Why this token does not resolve, or None when it does or is prose."""
    if PATH_TOKEN.match(token) or DIR_TOKEN.match(token):
        if not under_indexed_root(token):
            return None
        if token in paths or os.path.isdir(os.path.join(repo, token.rstrip("/"))):
            return None
        return (f"`{token}` is neither a file nor a directory under "
                f"{'/, '.join(PATH_ROOTS)}/; give the path from the repository root")

    if QUALIFIED.match(token):
        scope, _, leaf = token.rpartition("::")
        if leaf in members.get(scope, ()):
            return None
        if scope not in scopes:
            return f"`{token}` names no scope `{scope}` declared in this tree"
        return f"`{token}` names `{leaf}`, which `{scope}` does not declare"

    call = CALL_FORM.match(token)
    if call:
        if call.group(1) in declared:
            return None
        return f"`{token}` names no function declared in this tree"

    if CONSTANT.match(token):
        if token in declared:
            return None
        return f"`{token}` names no constant declared in this tree"

    if SNAKE_CASE.match(token):
        if token in tests or token in declared:
            return None
        return f"`{token}` names no test case and no symbol in this tree"

    return None


def main():
    if len(sys.argv) != 2:
        print(f"{NAME}: FAIL - usage: check_artifact_symbols_resolve.py <app-dir>",
              file=sys.stderr)
        return 2
    app_dir = os.path.abspath(sys.argv[1])
    repo = os.path.dirname(app_dir)

    paths = path_index(repo, PATH_ROOTS)
    members, scopes, declared = declarations(repo)
    tests = set().union(set(), *tests_by_file(repo).values())

    errors = []
    checked = forward = 0

    for change, files in change_artifacts(repo):
        texts = {f: read(f) for f in files}
        forward_here = declared_forward(texts.values())
        for full, text in texts.items():
            rel = os.path.relpath(full, repo)
            for m in BACKTICKED.finditer(text):
                for token in claim_parts(m.group(1)):
                    problem = resolve(token, repo, paths, members, scopes,
                                      declared, tests)
                    if problem is None:
                        continue
                    if token in forward_here:
                        forward += 1
                        continue
                    line = text.count("\n", 0, m.start()) + 1
                    errors.append(f"{rel}:{line} {problem}")
            checked += 1

    if errors:
        for e in sorted(set(errors)):
            print(f"{NAME}: FAIL - {e}", file=sys.stderr)
        print(f"{NAME}: FAIL - {len(set(errors))} unresolvable name(s) in change artifacts",
              file=sys.stderr)
        return 1

    print(f"{NAME}: OK - {checked} artifact file(s) resolve, "
          f"{forward} name(s) declared as not yet created")
    return 0


if __name__ == "__main__":
    sys.exit(main())
