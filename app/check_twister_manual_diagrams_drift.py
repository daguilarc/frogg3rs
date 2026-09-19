#!/usr/bin/env python3
"""check_twister_manual_diagrams_drift.py -- fails when the MIDI Fighter
Twister diagrams or MANUAL.md's own side-button table have drifted from the
preset (app/FroggersMidiCatalog.hpp's TwisterDeviceDefault()).

assets/manual/twister-controls.json (the label file
app/GenerateTwisterManualLabels.cpp writes, and
app/render_twister_manual_diagrams.mjs draws the two PNGs from) and the
preset it describes are two members of a family that must stay in sync and
cannot be collapsed into one definition -- nothing keeps them that way except
a check that fails on the next drift. This is that check: it rebuilds the
label file from the code by running the compiled label program into a scratch
file, fails when that result differs from the committed one, and separately
fails when a row of MANUAL.md's own side-button table no longer matches the
(now confirmed current) label file's own "sideButtons" rows -- read here, not
re-derived a second time from the raw preset the way render_twister_manual_
diagrams.mjs draws its diagrams.

Usage: check_twister_manual_diagrams_drift.py <app-dir>
"""

import json
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_common import read  # noqa: E402

NAME = "check-twister-manual-diagrams-drift"

TABLE_ROW = re.compile(r"^\|\s*(?P<button>[^|]+?)\s*\|\s*(?P<press>[^|]+?)\s*\|\s*(?P<shift>[^|]+?)\s*\|\s*$")


def side_button_rows(controls):
    """The six (place, press, shifted-press) triples MANUAL.md's side-button
    table restates, read directly off the "sideButtons" rows
    GenerateTwisterManualLabels.cpp already resolved -- one definition of what
    a side button's job is, read here and by the render script from the same
    label-file rows, never retyped as a second literal table. None if the
    label file's own rows are not the six this check expects."""
    buttons = controls.get("sideButtons")
    if not isinstance(buttons, list) or len(buttons) != 6:
        return None
    rows = []
    for button in buttons:
        place = button.get("place")
        press = button.get("press")
        shifted = button.get("shiftedPress")
        if not isinstance(place, str) or not isinstance(press, str) or not isinstance(shifted, str):
            return None
        rows.append((place, press, shifted))
    return rows


def parse_manual_table(manual_text):
    """The rows of MANUAL.md's "### MIDI Fighter Twister" side-button table,
    as (Button, Press, Shift + press) string triples, or None if the heading
    or its table is missing."""
    lines = manual_text.split("\n")
    heading = None
    for i, line in enumerate(lines):
        if line.strip() == "### MIDI Fighter Twister":
            heading = i
            break
    if heading is None:
        return None

    rows = []
    seen_header = False
    for line in lines[heading:]:
        m = TABLE_ROW.match(line)
        if not m:
            if seen_header:
                break
            continue
        if not seen_header:
            seen_header = True  # the "| Button | Press | Shift + press |" header row itself
            continue
        if set(line.replace("|", "").strip()) <= set("-: "):
            continue  # the "|---|---|---|" separator row
        rows.append((m.group("button").strip(), m.group("press").strip(), m.group("shift").strip()))
    return rows if seen_header else None


def main():
    if len(sys.argv) != 2:
        print(f"{NAME}: FAIL - usage: {os.path.basename(__file__)} <app-dir>", file=sys.stderr)
        return 2
    app_dir = os.path.abspath(sys.argv[1])
    repo = os.path.dirname(app_dir)

    generator = os.path.join(app_dir, "build", "froggers_generate_twister_manual_labels")
    if not os.path.isfile(generator):
        print(
            f"{NAME}: FAIL - {generator} is not built; run "
            f"`make -C app build/froggers_generate_twister_manual_labels`",
            file=sys.stderr,
        )
        return 1

    committed_path = os.path.join(repo, "assets", "manual", "twister-controls.json")
    if not os.path.isfile(committed_path):
        print(f"{NAME}: FAIL - {committed_path} does not exist; run `make -C app manual-diagrams`", file=sys.stderr)
        return 1
    committed_text = read(committed_path)

    with tempfile.TemporaryDirectory() as tmp:
        fresh_path = os.path.join(tmp, "twister-controls.json")
        result = subprocess.run([generator, fresh_path], capture_output=True, text=True, check=False)
        if result.returncode != 0:
            print(f"{NAME}: FAIL - {generator} exited {result.returncode}: {result.stderr.strip()}", file=sys.stderr)
            return 1
        fresh_text = read(fresh_path)

    if fresh_text != committed_text:
        print(
            f"{NAME}: FAIL - {os.path.relpath(committed_path, repo)} no longer matches the preset; "
            f"run `make -C app manual-diagrams` and commit the result",
            file=sys.stderr,
        )
        return 1

    controls = json.loads(fresh_text)
    rows = side_button_rows(controls)
    if rows is None:
        print(
            f"{NAME}: FAIL - the label file's \"sideButtons\" rows no longer match the six-button layout this "
            f"check expects",
            file=sys.stderr,
        )
        return 1

    manual_path = os.path.join(repo, "MANUAL.md")
    table_rows = parse_manual_table(read(manual_path))
    if table_rows is None:
        print(
            f"{NAME}: FAIL - {os.path.relpath(manual_path, repo)} has no MIDI Fighter Twister side-button table",
            file=sys.stderr,
        )
        return 1

    if table_rows != rows:
        print(
            f"{NAME}: FAIL - MANUAL.md's MIDI Fighter Twister side-button table has drifted from the "
            f"preset; run `make -C app manual-diagrams`, and update the table to match:",
            file=sys.stderr,
        )
        width = max(len(rows), len(table_rows))
        for i in range(width):
            expected = rows[i] if i < len(rows) else None
            actual = table_rows[i] if i < len(table_rows) else None
            if expected != actual:
                print(f"{NAME}: FAIL -   row {i + 1}: expected {expected}, found {actual}", file=sys.stderr)
        return 1

    print(
        f"{NAME}: OK - {os.path.relpath(committed_path, repo)} matches the preset, "
        f"{len(rows)} side-button table row(s) match"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
