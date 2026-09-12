#!/usr/bin/env python3
"""check_modified_requirements_restate_promoted.py -- fails when a change's
MODIFIED requirement drops something the promoted requirement says, without
declaring it.

A MODIFIED requirement REPLACES the promoted requirement's whole body. Anything
the delta does not restate is deleted at archive time, silently: `openspec
validate --strict` counts neither scenarios nor clauses, so a delta that drops
half a requirement is as valid as one that restates it.

Reading cannot cover this. The delta and the promoted text are two members of a
family that must stay in sync and cannot be collapsed into one definition, and
the evidence that reading fails here is specific: a delta shipped whose slot 13
dropped the clause "anchored so that silence-in still produces silence-out"
while matching the promoted scenario on title AND on scenario count -- the two
things its own plan said to check. It survived the strict validator, survived
the spec-checks gate, and survived two full audits by contexts that had not
written it. The clause named a property the feedback path depends on.

WHAT THIS DOES NOT CHECK, and why. A delta that drops a promoted scenario
ENTIRELY is usually superseding it on purpose: `frogg3rs-midi-controller-
resilience` replaces "A missing release leaves Shift held until the next press
and release" with "A missing release does not outlive the profile that saw it",
which is the whole point of a change about bounded modifier lifetime. Nothing
mechanical separates that from an accidental omission, so gating it would fail
the build on correct work. Scenario counts are also the thing review already
handles: two independent audits counted them correctly here. What review missed,
twice, was a clause lost INSIDE a scenario the delta did restate -- which has no
legitimate silent form, because restating a scenario is a statement that you
carried it forward. That is the gap this gate covers, and only that.

So the rule is mechanical and binary: for every scenario the delta restates,
every promoted bullet under it appears VERBATIM or is DECLARED.

DECLARING AN EDIT. A MODIFIED requirement may carry a block naming each
promoted bullet it deliberately changes or drops:

    <!-- RESTATES-EXCEPT
    a distinctive fragment of the promoted bullet being changed
      keeps: a promoted phrase the replacement must still carry
    another fragment
      keeps: none
    -->

Each fragment must match at least one promoted bullet that the delta does not
carry verbatim. A fragment matching nothing is itself a failure: it is a
declaration that has decayed past the text it described, which is the same
defect in the other direction, and it would otherwise sit reading as true.

EVERY DECLARED EDIT MUST SAY WHAT IT KEEPS, and the first version of this gate
did not demand that -- which made it green on the very defect it was written
for. Declaring a clause edited told the gate to stop reading it, so the hole was
exactly the size of the declaration, and the anchor clause could be dropped from
the replacement without a word. Proven by reproducing that defect against the
first version and watching it pass.

So a `keeps:` line is required under every entry and its text must appear
verbatim somewhere in the MODIFIED requirement. `keeps: none` is legal and is a
statement -- some promoted clauses are transitional and outlive nothing -- but
it has to be written, because a default would be chosen by whoever was in a
hurry. The author still decides what is load-bearing; the difference is that the
decision is recorded once and then enforced on every later edit, instead of
being re-made silently by each person who touches the file.

The asymmetry is deliberate. Bullets the delta ADDS are not checked -- a change
is allowed to say more than the requirement it replaces. Only loss is silent,
so only loss is gated.

Usage: check_modified_requirements_restate_promoted.py <app-dir>
The repository root is that argument's parent, matching the one-argument,
cwd-independent convention every other check script here uses.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_common import read  # noqa: E402

NAME = "check-modified-requirements-restate-promoted"

REQUIREMENT = re.compile(r"^###\s+Requirement:\s*(?P<title>.+?)\s*$")
SECTION = re.compile(r"^##\s+(?P<name>.+?)\s*$")
SCENARIO = re.compile(r"^####\s+Scenario:\s*(?P<title>.+?)\s*$")
BULLET = re.compile(r"^\s*-\s+(?P<body>.*\S)\s*$")
EXCEPT_OPEN = re.compile(r"^\s*<!--\s*RESTATES-EXCEPT\s*$")
EXCEPT_CLOSE = re.compile(r"^\s*-->\s*$")


def normalize(text):
    """One spelling for text that differs only in how it was wrapped.

    Promoted specs hard-wrap their bullets across lines and deltas generally do
    not, so a comparison that respects line breaks reports every bullet as
    changed and the check becomes noise nobody reads."""
    return " ".join(text.split())


def join_wrapped(lines):
    """Bullets, with continuation lines folded in.

    A bullet runs until the next bullet, heading, blank line or comment marker.
    Without this every wrapped promoted bullet looks dropped."""
    out = []
    current = None
    for line in lines:
        if EXCEPT_OPEN.match(line) or EXCEPT_CLOSE.match(line):
            if current is not None:
                out.append(current)
                current = None
            continue
        m = BULLET.match(line)
        if m:
            if current is not None:
                out.append(current)
            current = m.group("body")
            continue
        if current is not None:
            if not line.strip() or line.startswith("#"):
                out.append(current)
                current = None
            else:
                current += " " + line.strip()
    if current is not None:
        out.append(current)
    return [normalize(b) for b in out]


def requirement_blocks(path, only_section=None):
    """Requirement title -> its body lines.

    `only_section` restricts to one `## ...` section, which is how a delta's
    MODIFIED requirements are told from its ADDED ones. A promoted spec has no
    such sections, so it is read whole."""
    blocks = {}
    section = None
    title = None
    body = []
    for line in read(path).split("\n"):
        sec = SECTION.match(line)
        if sec and not line.startswith("###"):
            if title is not None:
                blocks[title] = body
                title, body = None, []
            section = sec.group("name")
            continue
        req = REQUIREMENT.match(line)
        if req:
            if title is not None:
                blocks[title] = body
            if only_section is None or (section or "").upper().startswith(only_section):
                title, body = req.group("title"), []
            else:
                title, body = None, []
            continue
        if title is not None:
            body.append(line)
    if title is not None:
        blocks[title] = body
    return blocks


def without_declarations(lines):
    """The requirement's own text, with the RESTATES-EXCEPT blocks removed.

    A `keeps:` fragment has to be looked for in what the requirement ASSERTS,
    never in the declaration that promised it. Searching the whole body instead
    matches every fragment against its own declaration, so every promise
    verifies itself and the gate reports green on a requirement that kept
    nothing -- which is what the first version did, on the exact defect it was
    written to catch."""
    out = []
    inside = False
    for line in lines:
        if EXCEPT_OPEN.match(line):
            inside = True
            continue
        if inside:
            if EXCEPT_CLOSE.match(line):
                inside = False
            continue
        out.append(line)
    return out


def scenario_titles(lines):
    return [m.group("title") for m in (SCENARIO.match(l) for l in lines) if m]


def restated_only(promoted_body, delta_body):
    """The promoted lines that fall under a scenario the delta restates, plus
    the requirement's own preamble lines before any scenario.

    A scenario the delta drops entirely is left out: see the module docstring
    for why deliberate supersession and accidental omission cannot be told
    apart mechanically, and why gating the pair fails correct changes."""
    keep = set(scenario_titles(delta_body))
    out = []
    live = True
    for line in promoted_body:
        m = SCENARIO.match(line)
        if m:
            live = m.group("title") in keep
        if live:
            out.append(line)
    return out


KEEPS = re.compile(r"^\s+keeps:\s*(?P<text>.*\S)\s*$", re.I)


def declared_fragments(lines):
    """The RESTATES-EXCEPT entries as (fragment, keeps) pairs, in order.

    `keeps` is None when the entry carried no `keeps:` line at all, which is a
    failure rather than a default -- see the module docstring. The literal word
    `none` means the promoted clause outlives nothing, and is a decision the
    author writes down."""
    out = []
    inside = False
    for line in lines:
        if EXCEPT_OPEN.match(line):
            inside = True
            continue
        if inside and EXCEPT_CLOSE.match(line):
            inside = False
            continue
        if not inside or not line.strip():
            continue
        keeps = KEEPS.match(line)
        if keeps:
            if not out:
                out.append(("", None))
            fragment, _ = out[-1]
            text = normalize(keeps.group("text"))
            out[-1] = (fragment, None if text.lower() == "none" else text)
            continue
        out.append((normalize(line.strip().lstrip("-").strip()), _MISSING))
    return out


# Told apart from a written `keeps: none`, which is a decision, rather than an
# entry that simply never said.
_MISSING = object()


def delta_specs(repo):
    """Every unarchived change's spec deltas, paired with the capability they
    modify -- which is the delta's own parent directory name, the same way
    openspec resolves it."""
    out = []
    changes = os.path.join(repo, "openspec", "changes")
    if not os.path.isdir(changes):
        return out
    for entry in sorted(os.listdir(changes)):
        if entry == "archive":
            continue
        base = os.path.join(changes, entry, "specs")
        if not os.path.isdir(base):
            continue
        for capability in sorted(os.listdir(base)):
            spec = os.path.join(base, capability, "spec.md")
            if os.path.isfile(spec):
                out.append((entry, capability, spec))
    return out


def main():
    if len(sys.argv) != 2:
        print(f"{NAME}: FAIL - usage: {os.path.basename(__file__)} <app-dir>", file=sys.stderr)
        return 2
    app_dir = os.path.abspath(sys.argv[1])
    repo = os.path.dirname(app_dir)

    errors = []
    checked_requirements = 0
    checked_bullets = 0
    declared_edits = 0

    for change, capability, delta_path in delta_specs(repo):
        promoted_path = os.path.join(repo, "openspec", "specs", capability, "spec.md")
        rel_delta = os.path.relpath(delta_path, repo)
        modified = requirement_blocks(delta_path, only_section="MODIFIED")
        if not modified:
            continue
        if not os.path.isfile(promoted_path):
            # A MODIFIED requirement against a capability with no promoted spec
            # modifies nothing. Either the capability name is wrong or the
            # requirement is ADDED; both are worth failing on.
            errors.append(
                f"{rel_delta} declares MODIFIED requirements but "
                f"openspec/specs/{capability}/spec.md does not exist"
            )
            continue
        promoted = requirement_blocks(promoted_path)

        for title, delta_body in sorted(modified.items()):
            if title not in promoted:
                errors.append(
                    f"{rel_delta}: MODIFIED requirement '{title}' does not exist in "
                    f"openspec/specs/{capability}/spec.md, so it modifies nothing"
                )
                continue
            checked_requirements += 1
            promoted_body = promoted[title]

            asserted = without_declarations(delta_body)
            delta_bullets = set(join_wrapped(asserted))
            delta_text = normalize(" ".join(asserted))
            entries = declared_fragments(delta_body)
            declared_edits += len(entries)
            dropped = []
            for bullet in join_wrapped(restated_only(promoted_body, delta_body)):
                checked_bullets += 1
                if bullet not in delta_bullets:
                    dropped.append(bullet)

            matched = {f: 0 for f, _ in entries}
            for bullet in dropped:
                covering = [f for f, _ in entries if f and f in bullet]
                for f in covering:
                    matched[f] += 1
                if not covering:
                    shown = bullet if len(bullet) <= 140 else bullet[:137] + "..."
                    errors.append(
                        f"{rel_delta}: '{title}' drops a promoted clause without "
                        f"declaring it: {shown}"
                    )
            for fragment, keeps in entries:
                shown = fragment if len(fragment) <= 100 else fragment[:97] + "..."
                if not fragment or matched.get(fragment, 0) == 0:
                    errors.append(
                        f"{rel_delta}: '{title}' declares an edit that matches no "
                        f"dropped promoted clause: {shown}"
                    )
                    continue
                if keeps is _MISSING:
                    errors.append(
                        f"{rel_delta}: '{title}' declares an edit with no `keeps:` "
                        f"line, so nothing checks what the replacement still "
                        f"carries: {shown}"
                    )
                    continue
                if keeps is not None and keeps not in delta_text:
                    errors.append(
                        f"{rel_delta}: '{title}' declares it keeps \"{keeps}\", "
                        f"but that text appears nowhere in the restated "
                        f"requirement"
                    )

    if errors:
        for e in errors:
            print(f"{NAME}: FAIL - {e}", file=sys.stderr)
        print(f"{NAME}: FAIL - {len(errors)} undeclared difference(s)", file=sys.stderr)
        return 1
    print(
        f"{NAME}: OK - {checked_requirements} MODIFIED requirement(s), "
        f"{checked_bullets} promoted clause(s) restated or declared, "
        f"{declared_edits} declared edit(s)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
