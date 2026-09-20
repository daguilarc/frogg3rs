#!/usr/bin/env python3
"""check_no_bank_page_conflation.py -- fails on the renamed-away "bank"
identifiers reappearing in app/ (including app/dsp/), on the page-switcher's
retired button LABELS reappearing in a string literal there, and on a bare
"bank" reappearing in MANUAL.md outside its two genuine references to the
MIDI Fighter Twister's own onboard hardware bank.

Froggers' outward-facing "which of the six pages is showing" concept is named
"page" throughout the app surface, the MIDI catalog, and MANUAL.md. "Bank"
stays legitimate in three other, unrelated senses this script does not
touch: Sheaf's own `synth::Bank`/`BankSlot`/`ParamBank` type and `MessageIn`
vocabulary, Froggers' `FroggersBankId` parameter-model address (bank+slot
addressing, `bankIx`, `RoutedKnob(FroggersBankId, ...)`, and friends), and
`app/dsp/EnvelopeFollowers.hpp`'s citations of the retired simulator's
`V2EnvelopeFollowerBank.hpp`. None of those are patterns here.

IDENTIFIER_PATTERNS catches the compound-identifier SHAPE the retired names
share -- "Bank" immediately followed by the UI/dispatch word that named what
it was showing or doing -- not an enumeration of the six retired names
themselves. Matching is substring, both ends: no leading `\\b`, because every
renamed-away identifier this script exists to catch is prefixed by a word
character (`kBankNext`, `ActiveBankIndex`) and `\\b` never matches between two
word characters, so a leading `\\b` would silently never match the very names
it names (verified empirically against `kBankNext`/`ActiveBankIndex` before
this script first shipped); and no trailing `\\b` on the main pattern either,
so a name that carries the shape further -- `BankSelected`, a hypothetical
`BankSwitcherButton` -- still matches on its `Bank`+word prefix rather than
needing its own alternative spelled out. `BankIndex` keeps ITS trailing `\\b`
deliberately: `app/vst/FroggersVstHostTests.cpp`'s
`BuildPatchTextWithVisibleBankIndexOverridden` is a live, legitimate helper
whose name continues past `Index` with more identifier -- exactly the shape
the trailing boundary exists to let through -- while `ActiveBankIndex` and
`CurrentBankIndex` still end right there and still match.

This shape test does not need to reach `Bank` used as a SUFFIX (`RouteFilterBank`,
`ProcessDriveBank`, `kFroggersSlotsPerBank`, Sheaf's own `SelectParamBank`) or
combined with an addressing word (`BankSlot`, `BankColor`, `BankRef`,
`FroggersBankLayouts`) -- none of those is a word this check's alternation
matches after `Bank`, and no such word is added to it without checking it
first against every one of those live names.

LABEL_PATTERN covers the same retired concept in a different shape: a string
literal a player reads, not a compiled symbol. It fires on a double-quoted
string opening with the word "Bank" as a whole word -- `"Bank Next"`,
`"Bank Previous"`, the `"Bank "` half of `"Bank " + std::to_string(ix + 1)`
-- the shape `app/FroggersMidiCatalog.hpp`'s catalog labels held before this
change renamed them to "Page". It does not fire on a wire value: those are
either lowercase and dotted (`"froggers.bank.next"`) or lowercase and
unspaced (`"visibleBankIndex"`, `"bank0.slot0"`-style test parameter ids), and
it does not fire on an unrelated fixture name that merely starts with the same
four letters (`"Bank9000 Future Slot 99"` in
`app/vst/FroggersVstHostTests.cpp`, a made-up parameter id with a digit
directly after "Bank" and no following space) because the pattern requires a
space or the closing quote right after the word.

Usage: check_no_bank_page_conflation.py <app-dir>
"""

import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__))))
from check_common import walk_sources  # noqa: E402

NAME = "check-no-bank-page-conflation"

# Bank immediately followed by the word that named what the retired button,
# tab, arrow or dispatch action DID -- catches the shape, not a fixed list of
# the six original names: `BankButton`, `BankSelected`, `RightKind::BankTabs`,
# and any future name built the same way, alongside the six this rename
# actually produced (`BankNext`, `BankPrevious`/`BankPrev`, `BankSelect`,
# `BankTabsRow`, `BankPrevArrow`, `BankNextArrow`).
IDENTIFIER_PATTERNS = [
    re.compile(r"Bank(?:Next|Prev|Select|Tab|Button|Row|Switcher)"),
    re.compile(r"BankIndex\b"),
    re.compile(r"\bkFroggersBankCount\b"),
    re.compile(r"\bkVisibleBankIndexKey\b"),
]

# A string literal whose text is the retired page-switcher's own button
# label: the word "Bank" standing alone, or opening a longer label
# ("Bank Next", "Bank " + a number). A wire value never matches: it is either
# dotted-lowercase ("froggers.bank.next") or has no space after "Bank"
# ("visibleBankIndex", "Bank9000 Future Slot 99").
LABEL_PATTERN = re.compile(r'"Bank(?=[ "])')

SCAN_DIRS = ("", "vst", "dsp")
SCAN_EXT = (".hpp", ".cpp")

MANUAL_RELATIVE_PATH = "MANUAL.md"
MANUAL_BANK_PATTERN = re.compile(r"\bbank\b", re.I)
MANUAL_ALLOWED_LINES = (
    "Bank Side Buttons",
    "whatever bank the Twister",
)


def blank_string_literals(line):
    """Replaces the CONTENTS of every double-quoted string literal on `line`
    with spaces, so an identifier pattern can scan the code around a wire
    string like `"visibleBankIndex"` without also matching the wire string's
    own preserved text. Quotes and backslash escapes are honoured; anything
    outside quotes -- including the identifiers this check exists to catch
    -- passes through unchanged."""
    out = []
    i = 0
    n = len(line)
    while i < n:
        c = line[i]
        if c == '"':
            out.append(c)
            i += 1
            while i < n and line[i] != '"':
                if line[i] == "\\" and i + 1 < n:
                    out.append(" ")
                    i += 1
                out.append(" ")
                i += 1
            if i < n:
                out.append(line[i])
                i += 1
            continue
        out.append(c)
        i += 1
    return "".join(out)


def identifier_violations(app_dir):
    errors = []
    scanned = 0
    for sub in SCAN_DIRS:
        root = os.path.join(app_dir, sub) if sub else app_dir
        if not os.path.isdir(root):
            continue
        for full in walk_sources(root, SCAN_EXT):
            if os.path.dirname(full) != root:
                continue  # each SCAN_DIRS entry is non-recursive, like the globs it replaces
            if os.path.basename(full) == os.path.basename(__file__):
                continue
            scanned += 1
            with open(full, "r", encoding="utf-8", errors="replace") as fh:
                for n, line in enumerate(fh, 1):
                    rel = os.path.relpath(full, os.path.dirname(app_dir))
                    code = blank_string_literals(line)
                    matched = False
                    for pat in IDENTIFIER_PATTERNS:
                        m = pat.search(code)
                        if m:
                            errors.append(f"{rel}:{n} reintroduces {m.group(0)!r}: {line.strip()[:100]}")
                            matched = True
                            break
                    if matched:
                        continue
                    # The label check reads the RAW line -- the string literal's
                    # own text is exactly what it is looking for, so it must not
                    # run against the blanked copy the identifier scan uses.
                    lm = LABEL_PATTERN.search(line)
                    if lm:
                        errors.append(f"{rel}:{n} reintroduces the retired \"Bank\" label: "
                                      f"{line.strip()[:100]}")
    return errors, scanned


def manual_violations(repo_root):
    manual_path = os.path.join(repo_root, MANUAL_RELATIVE_PATH)
    if not os.path.isfile(manual_path):
        return [f"{MANUAL_RELATIVE_PATH} not found at {manual_path}"]
    errors = []
    with open(manual_path, "r", encoding="utf-8", errors="replace") as fh:
        for n, line in enumerate(fh, 1):
            if any(allowed in line for allowed in MANUAL_ALLOWED_LINES):
                continue
            if MANUAL_BANK_PATTERN.search(line):
                errors.append(f"{MANUAL_RELATIVE_PATH}:{n} bare 'bank' outside the Twister hardware sentences: "
                               f"{line.strip()[:100]}")
    return errors


def main():
    if len(sys.argv) != 2:
        print(f"{NAME}: FAIL - usage: check_no_bank_page_conflation.py <app-dir>", file=sys.stderr)
        return 2
    app_dir = os.path.abspath(sys.argv[1])
    repo_root = os.path.dirname(app_dir)

    id_errors, scanned = identifier_violations(app_dir)
    manual_errors = manual_violations(repo_root)
    errors = id_errors + manual_errors

    if errors:
        for e in errors:
            print(f"{NAME}: FAIL - {e}", file=sys.stderr)
        print(f"{NAME}: FAIL - {len(errors)} bank/page conflation(s)", file=sys.stderr)
        return 1

    print(f"{NAME}: OK - {scanned} app source files, MANUAL.md; no bank/page conflation")
    return 0


if __name__ == "__main__":
    sys.exit(main())
