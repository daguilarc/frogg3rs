#!/usr/bin/env python3
"""check_docs_match_parameter_table.py -- a mechanical check that MANUAL.md
and QUICK_DICT.md name every parameter the same way the C++ source does:
same name, same slot, and a backticked label that matches what the encoder
grid actually renders (FroggersUiSurface.hpp's FroggersApprovedLabels), never
the internal shortName. Two documents restate one C++ table across a
language boundary, and that is a family that drifts silently unless
something fails on the drift -- this is that something.

It parses the six 14-entry bank tables out of FroggersBankLayouts()
(FroggersParameters.hpp) and the matching six rows of FroggersApprovedLabels()
(FroggersUiSurface.hpp) by symbol, not by line number, so it keeps working
as the source file is edited around those functions. It then parses every
bold parameter entry out of the six bank sections of MANUAL.md and
QUICK_DICT.md -- singles ("**Name** (slot N)" or "**Name** (`Label`, slot N)")
and groups ("**Name1 / Name2 / Name3** (slots a-c)" or
"**Name1 / Name2 / Name3** (`L1`/`L2`/`L3`, slots a/b/c)") -- and compares
each parsed (name, label, slot) triple against the table.

Usage: check_docs_match_parameter_table.py <app-dir>
"""

import os
import re
import sys

BANK_ORDER = ["Audio", "Envelope", "Filter", "Drive", "Delay", "Reverb"]
PARAMS_PER_BANK = 14
BANK_COUNT = len(BANK_ORDER)


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def fail(message):
    print("check-docs-match-parameter-table: FAIL - " + message, file=sys.stderr)
    sys.exit(1)


def parse_bank_layouts(path):
    """Returns {bank_name: [{"name", "shortName", "slot"}, ...]} in slot order."""
    text = strip_comments(open(path, encoding="utf-8").read())
    fn = re.search(r"FroggersBankLayouts\(\)\s*\{", text)
    if not fn:
        fail(f"{path}: FroggersBankLayouts() not found")
    ret = re.search(r"return layouts;", text[fn.end():])
    if not ret:
        fail(f"{path}: FroggersBankLayouts() has no 'return layouts;'")
    body = text[fn.end():fn.end() + ret.start()]

    markers = list(re.finditer(r"FroggersBankId::(\w+)", body))
    if len(markers) != BANK_COUNT:
        fail(f"{path}: FroggersBankLayouts() has {len(markers)} banks, expected {BANK_COUNT}")

    banks = {}
    for i, marker in enumerate(markers):
        bank_name = marker.group(1)
        if bank_name != BANK_ORDER[i]:
            fail(f"{path}: bank {i} is {bank_name}, expected {BANK_ORDER[i]} "
                 f"(the check assumes FroggersBankLayouts()'s own bank order)")
        seg_end = markers[i + 1].start() if i + 1 < len(markers) else len(body)
        segment = body[marker.end():seg_end]
        entries = re.findall(r'\{\s*"([^"]*)"\s*,\s*"([^"]*)"', segment)
        if len(entries) != PARAMS_PER_BANK:
            fail(f"{path}: bank {bank_name} has {len(entries)} parameter entries, "
                 f"expected {PARAMS_PER_BANK}")
        banks[bank_name] = [
            {"name": name, "shortName": short, "slot": slot}
            for slot, (name, short) in enumerate(entries)
        ]
    return banks


def parse_approved_labels(path):
    """Returns {bank_name: [label, ...]} in slot order, keyed by BANK_ORDER."""
    text = strip_comments(open(path, encoding="utf-8").read())
    fn = re.search(r"FroggersApprovedLabels\(\)\s*\{", text)
    if not fn:
        fail(f"{path}: FroggersApprovedLabels() not found")
    ret = re.search(r"return labels;", text[fn.end():])
    if not ret:
        fail(f"{path}: FroggersApprovedLabels() has no 'return labels;'")
    body = text[fn.end():fn.end() + ret.start()]

    rows = re.findall(r"\{\{([^{}]*)\}\}", body)
    if len(rows) != BANK_COUNT:
        fail(f"{path}: FroggersApprovedLabels() has {len(rows)} rows, expected {BANK_COUNT}")

    labels = {}
    for i, row in enumerate(rows):
        row_labels = re.findall(r'"([^"]*)"', row)
        if len(row_labels) != PARAMS_PER_BANK:
            fail(f"{path}: FroggersApprovedLabels() row {i} ({BANK_ORDER[i]}) has "
                 f"{len(row_labels)} labels, expected {PARAMS_PER_BANK}")
        labels[BANK_ORDER[i]] = row_labels
    return labels


def build_table(params_path, surface_path):
    banks = parse_bank_layouts(params_path)
    labels = parse_approved_labels(surface_path)
    for bank in BANK_ORDER:
        for entry in banks[bank]:
            entry["label"] = labels[bank][entry["slot"]]
    return banks


def heading_regex(bank, suffix):
    return re.compile(r"^##\s+" + re.escape(bank) + suffix + r"\s*$")


def extract_bank_sections(text, suffix):
    headings = list(re.finditer(r"^##[ \t]+.*$", text, re.M))
    sections = {}
    for bank in BANK_ORDER:
        pattern = heading_regex(bank, suffix)
        found = None
        for i, h in enumerate(headings):
            if pattern.match(h.group(0)):
                found = i
                break
        if found is None:
            fail(f"missing '## {bank}{suffix}' heading")
        start = headings[found].end()
        end = headings[found + 1].start() if found + 1 < len(headings) else len(text)
        sections[bank] = text[start:end]
    return sections


ENTRY_RE = re.compile(r"\*\*(.+?)\*\*\s*\(([^)]*)\)")


def parse_entries(section_text, doc_name, bank_name, errors):
    triples = []
    for m in ENTRY_RE.finditer(section_text):
        names_part = m.group(1).strip()
        paren = m.group(2).strip()

        labels_part = None
        slot_part = paren
        if paren.startswith("`"):
            comma = paren.find(",")
            if comma == -1:
                continue  # not a parameter entry
            labels_part = paren[:comma].strip()
            slot_part = paren[comma + 1:].strip()

        slot_part_lower = slot_part.lower()
        if slot_part_lower.startswith("slots "):
            spec = slot_part[len("slots "):].strip()
        elif slot_part_lower.startswith("slot "):
            spec = slot_part[len("slot "):].strip()
        else:
            continue  # not a parameter entry

        if "–" in spec:
            a, b = spec.split("–")
            slots = list(range(int(a.strip()), int(b.strip()) + 1))
        elif "/" in spec:
            slots = [int(x.strip()) for x in spec.split("/")]
        else:
            slots = [int(spec)]

        names = [n.strip() for n in names_part.split(" / ")] if " / " in names_part else [names_part]

        if labels_part is not None:
            doc_labels = [l.strip().strip("`") for l in labels_part.split("/")]
        else:
            doc_labels = [None] * len(names)

        if len(names) != len(slots) or len(names) != len(doc_labels):
            errors.append(
                f"{doc_name}: {bank_name}: entry '{names_part}' has mismatched "
                f"names/labels/slots counts ({len(names)}/{len(doc_labels)}/{len(slots)})"
            )
            continue

        triples.extend(zip(names, doc_labels, slots))
    return triples


def validate(doc_name, bank_name, triples, table):
    errors = []
    by_name = {e["name"]: e for e in table[bank_name]}
    seen = set()

    for name, doc_label, doc_slot in triples:
        entry = by_name.get(name)
        if entry is None:
            errors.append(f"{doc_name}: {bank_name}: '{name}' is not a registered parameter name")
            continue
        seen.add(name)

        if entry["slot"] != doc_slot:
            errors.append(
                f"{doc_name}: {bank_name}: '{name}' is shown at slot {doc_slot}, "
                f"the table has it at slot {entry['slot']}"
            )

        rendered = entry["label"]
        if rendered == name:
            if doc_label is not None:
                errors.append(
                    f"{doc_name}: {bank_name}: '{name}' shows label `{doc_label}`, "
                    f"but the rendered label equals the name"
                )
        else:
            if doc_label is None:
                errors.append(
                    f"{doc_name}: {bank_name}: '{name}' has no backticked label, "
                    f"but the rendered label is `{rendered}`"
                )
            elif doc_label != rendered:
                errors.append(
                    f"{doc_name}: {bank_name}: '{name}' shows label `{doc_label}`, "
                    f"the rendered label is `{rendered}`"
                )

    for entry in table[bank_name]:
        if entry["name"] not in seen:
            errors.append(f"{doc_name}: {bank_name}: table entry '{entry['name']}' has no document entry")

    return errors


def main():
    if len(sys.argv) != 2:
        print("usage: check_docs_match_parameter_table.py <app-dir>", file=sys.stderr)
        return 1

    app_dir = os.path.abspath(sys.argv[1])
    parent_dir = os.path.dirname(app_dir)
    params_path = os.path.join(app_dir, "FroggersParameters.hpp")
    surface_path = os.path.join(app_dir, "FroggersUiSurface.hpp")
    manual_path = os.path.join(parent_dir, "MANUAL.md")
    quick_dict_path = os.path.join(parent_dir, "QUICK_DICT.md")

    for p in (params_path, surface_path, manual_path, quick_dict_path):
        if not os.path.isfile(p):
            print(f"check-docs-match-parameter-table: FAIL - missing {p}", file=sys.stderr)
            return 1

    table = build_table(params_path, surface_path)

    docs = [
        ("MANUAL.md", manual_path, " bank"),
        ("QUICK_DICT.md", quick_dict_path, ""),
    ]

    all_errors = []
    counts = []
    for doc_name, path, suffix in docs:
        text = open(path, encoding="utf-8").read()
        sections = extract_bank_sections(text, suffix)
        entry_count = 0
        doc_errors = []
        for bank in BANK_ORDER:
            triples = parse_entries(sections[bank], doc_name, bank, doc_errors)
            entry_count += len(triples)
            doc_errors.extend(validate(doc_name, bank, triples, table))
        all_errors.extend(doc_errors)
        counts.append(f"{doc_name} {entry_count} entries/{len(doc_errors)} failures")

    if all_errors:
        for e in all_errors:
            print(f"check-docs-match-parameter-table: FAIL - {e}", file=sys.stderr)
        return 1

    print("check-docs-match-parameter-table: OK - " + "; ".join(counts))
    return 0


if __name__ == "__main__":
    sys.exit(main())
