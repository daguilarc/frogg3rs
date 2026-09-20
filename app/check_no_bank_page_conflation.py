#!/usr/bin/env python3
"""check_no_bank_page_conflation.py -- fails on the renamed-away "bank"
identifiers reappearing in app/ (including app/dsp/), and on a bare "bank"
reappearing in MANUAL.md outside its two genuine references to the MIDI
Fighter Twister's own onboard hardware bank.

Froggers' outward-facing "which of the six pages is showing" concept is named
"page" throughout the app surface, the MIDI catalog, and MANUAL.md. "Bank"
stays legitimate in three other, unrelated senses this script does not
touch: Sheaf's own `synth::Bank`/`BankSlot` type and `MessageIn` vocabulary,
Froggers' `FroggersBankId` parameter-model address (bank+slot addressing,
`bankIx`, `RoutedKnob(FroggersBankId, ...)`, and friends), and
`app/dsp/EnvelopeFollowers.hpp`'s citations of the retired simulator's
`V2EnvelopeFollowerBank.hpp`. None of those are patterns here -- a pattern
broad enough to reach them would need an allowlist as large as their own
legitimate usage, which is the failure mode `check_no_planning_history.py`'s
own header comment warns against.

The four identifier patterns below deliberately do NOT anchor a `\\b` before
"Bank": every renamed-away identifier this script exists to catch is prefixed
by a word character (`kBankNext`, `ActiveBankIndex`), and `\\b` never matches
between two word characters regardless of a case change, so a leading `\\b`
would silently never match the very names it names. Verified empirically
against `kBankNext`/`ActiveBankIndex` before this script shipped.

Usage: check_no_bank_page_conflation.py <app-dir>
"""

import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__))))
from check_common import walk_sources  # noqa: E402

NAME = "check-no-bank-page-conflation"

IDENTIFIER_PATTERNS = [
    re.compile(r"Bank(?:Next|Previous|Select|TabsRow|PrevArrow|NextArrow)\b"),
    re.compile(r"BankIndex\b"),
    re.compile(r"\bkFroggersBankCount\b"),
    re.compile(r"\bkVisibleBankIndexKey\b"),
]

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
                    code = blank_string_literals(line)
                    for pat in IDENTIFIER_PATTERNS:
                        m = pat.search(code)
                        if m:
                            rel = os.path.relpath(full, os.path.dirname(app_dir))
                            errors.append(f"{rel}:{n} reintroduces {m.group(0)!r}: {line.strip()[:100]}")
                            break
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
