#!/usr/bin/env python3
"""check_delay_capacity_break_proofs.py -- fails when the Delay capacity
compares (dsp/Delay.hpp, compiled under FROGGERS_DSP_CHECKS) stop rejecting
the family of defects they exist to catch.

A passing DSP parity suite says those compares RAN; it says nothing about
whether they still REJECT a broken capacity mechanism -- the checks and the
mechanism they guard are two things that must stay in sync, and drift
between them shows up only as a gate that stays green while the thing it
protects has quietly stopped doing its job. This mutates dsp/Delay.hpp one
way at a time, in an isolated copy, compiles the real DSP parity suite
against each mutation, and requires the compiled binary to abort. A break
that compiles and runs to a zero exit has gone undetected -- printed as FAIL
with reason "stayed green", not as a passing build.

Two breaks compile at a time -- this machine's cap -- and the printed lines
are always in the table's order below, not the order breaks finish in.

The base-term bound (`baseSeconds = std::min(baseSecondsRaw, capacitySeconds)`)
is deliberately absent from the table below: removing it does not, on its
own, produce an unrejected break to test for. `maxModSeconds` and
`maxSpreadSeconds` are both derived from `capacitySeconds - baseSeconds`
(directly or via `- modSeconds` on top of it), so an unbounded baseSeconds
telescopes into an unbounded modSeconds/widthSpread bound as well -- the
modulation-bound and width-bound breaks already below in this table cover
the same failure surface a base-bound break would exercise, and a fourth
entry for it is known to stay green for that reason, not from having gone
unchecked.

`capacity-headroom-shortens-reads` keeps every read inside capacity and
shortens reads near it by 10 ms, so it passes the compile-gated compares and
is rejected only by the suite's expected-versus-measured assertions; it is
what proves those assertions still reject.

`width-term-removed`'s anchor is the width term's own definition line, so any
edit to that expression -- a new factor, a gate on another knob -- fails this
gate until the table is updated to match, which puts the edit in front of a
reader rather than letting it pass unremarked.

Before any of the mutation breaks above run, this also hashes four fixed
regions of the real (unmutated) dsp/Delay.hpp: the whole of
`StereoDelay::Process` (from its own signature through the function's own
closing brace, located by brace matching rather than a second text anchor,
so a body edit that moves where the function ends cannot slip past this
check by leaving an old end-anchor line untouched), the `ReadAt` function
itself, `SetSampleRate` (signature through its own closing brace, also
brace-matched), and the single line of `SetWidthBalance`. The whole of
`Process`, the read function and the two setters that write the offset's
inputs are hashed, so any edit inside them fails this gate until the stored
hash below is updated after a reader has seen the edit -- the tests decide
whether the edit is right, the hash decides only that it was seen. Region
extraction by brace matching fails this gate loudly, as a pinned-region FAIL
before any break compiles, if its start line occurs other than once in the
header -- the same guard the `read-at-function` and `set-width-balance`
regions' text-anchor pairs already apply at both ends. Edits to dsp/Delay.hpp
outside all four regions are not pinned by this hash check at all; whether
they are still correct is the break-proof loop's and the DSP parity suite's
job below, not this hash's. A hash mismatch fails this gate until the stored
constant below is updated to match, which puts that edit in front of a
reader rather than letting it drift past unremarked, the same role
`width-term-removed` plays for one specific line. Each region's raw text has
its whole-line `//` comments dropped and its whitespace runs collapsed to
one space before hashing, so reflowing or re-commenting the block does not
itself change the hash -- only a change to what it computes does.

Usage:
    check_delay_capacity_break_proofs.py <app-dir> <cxx> "<flags>" "<dsp-test-sources>"

<flags> and <dsp-test-sources> are each a single shell-style argument,
whitespace-split internally -- this is how app/Makefile hands over
"-I$(APP_DIR) $(CPPFLAGS) $(CXXFLAGS)" and "$(DSP_TEST_SOURCES)" as one
token apiece.

Each source in <dsp-test-sources> is `#include "dsp/Delay.hpp"` -- a QUOTED
include, whose search order checks the including file's OWN DIRECTORY before
consulting any -I path, no flag overrides that for the plain -I form. Putting
the mutated dsp/ first on -I therefore does not shadow anything: confirmed by
`-H` against a compile of the real, unmoved source, which resolved straight
to the real app_dir/dsp/Delay.hpp regardless of -I order (see
research/break-proof-gate.md for the literal listing). The fix actually
copies each source into build/break-proofs/<name>/ alongside the mutated
dsp/, so the copy's OWN directory is what the quoted include finds first --
robust regardless of include-search-order flags, rather than depending on
one.
"""

import hashlib
import os
import re
import shlex
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_common import read  # noqa: E402

NAME = "check-delay-capacity-break-proofs"

DELAY_HEADER_RELPATH = os.path.join("dsp", "Delay.hpp")

# Each entry is either
#   (name, "anchor", start_needle, end_needle, expected_sha256)
# or
#   (name, "brace", start_needle, None, expected_sha256).
# start_needle (and, for "anchor", end_needle) must each occur in exactly one
# line of the real dsp/Delay.hpp -- an anchor that has drifted (renamed,
# reformatted, duplicated) fails loudly here rather than silently hashing the
# wrong span or none at all. An "anchor" region is every line from the start
# line through the end line inclusive. A "brace" region starts at the start
# line and runs through whichever later line's closing brace first brings
# the open-brace count -- counted one character at a time from the start
# line onward -- back to zero, so it needs no second anchor and keeps
# tracking the function's real end across a body edit that moves it. Either
# way, whole-line `//` comments (lines whose first non-space characters are
# `//`) are dropped, the remaining lines are joined and their whitespace
# runs collapsed to one space, and that string is sha256-hashed. Constants
# below were produced by:
#   python3 -c "
#   import hashlib, re
#   text = open('dsp/Delay.hpp', encoding='utf-8').read()
#   lines = text.splitlines()
#   def region(s, e):
#       kept = [l for l in lines[s:e + 1] if not l.lstrip().startswith('//')]
#       return re.sub(r'\s+', ' ', ' '.join(kept)).strip()
#   def anchor(start, end):
#       s = [i for i, l in enumerate(lines) if start in l][0]
#       e = [i for i, l in enumerate(lines) if end in l][0]
#       return region(s, e)
#   def brace(start):
#       s = [i for i, l in enumerate(lines) if start in l][0]
#       depth, opened = 0, False
#       for i in range(s, len(lines)):
#           for ch in lines[i]:
#               if ch == '{':
#                   depth += 1
#                   opened = True
#               elif ch == '}':
#                   depth -= 1
#           if opened and depth == 0:
#               return region(s, i)
#       raise SystemExit('no close found')
#   texts = [
#       brace('DelayWetPair Process(float bumpIn, const DelayParams& p)'),
#       anchor('float ReadAt(float seconds, const std::vector<float>& line) const',
#              'return line[idx0] * (1.0f - frac) + line[idx1] * frac;'),
#       brace('void SetSampleRate('),
#       anchor('void SetWidthBalance(float knob01)', 'void SetWidthBalance(float knob01)'),
#   ]
#   for t in texts:
#       print(hashlib.sha256(t.encode('utf-8')).hexdigest())
#   "
# run from app/, against the real (unmutated) dsp/Delay.hpp.
PINNED_REGIONS = [
    (
        "process-body",
        "brace",
        "DelayWetPair Process(float bumpIn, const DelayParams& p)",
        None,
        "ffe638c3598f8f7fed28cf3160b10cb55459220782b8220fcb62925b3fc9a666",
    ),
    (
        "read-at-function",
        "anchor",
        "float ReadAt(float seconds, const std::vector<float>& line) const",
        "return line[idx0] * (1.0f - frac) + line[idx1] * frac;",
        "72e427aa653eee63b30acd42e303a8891004fa19c9c2c87fd1adb315de8d9b8d",
    ),
    (
        "set-sample-rate",
        "brace",
        "void SetSampleRate(",
        None,
        "cede9e1c8f1445d08e2f648cc9d7d81925993306611f41ed794aba4ad773ba0c",
    ),
    (
        "set-width-balance",
        "anchor",
        "void SetWidthBalance(float knob01)",
        "void SetWidthBalance(float knob01)",
        "abc35b53f9f13759b97c9883520c2337c24d27267624105bd83ea72796d2f9a5",
    ),
]


def _collapse(lines):
    kept = [line for line in lines if not line.lstrip().startswith("//")]
    return re.sub(r"\s+", " ", " ".join(kept)).strip()


def _hash_pinned_region(text, start_needle, end_needle):
    """Returns (status, detail_or_hash) -- status is "ok" with the hex
    digest, or "error" with a human-readable reason (anchor missing or
    duplicated), mirroring run_one_break's own old_text-count guard below.
    """
    lines = text.splitlines()
    start_idxs = [i for i, line in enumerate(lines) if start_needle in line]
    end_idxs = [i for i, line in enumerate(lines) if end_needle in line]
    if len(start_idxs) != 1:
        return "error", f"start anchor matched {len(start_idxs)} times (need exactly 1): {start_needle!r}"
    if len(end_idxs) != 1:
        return "error", f"end anchor matched {len(end_idxs)} times (need exactly 1): {end_needle!r}"
    start_idx, end_idx = start_idxs[0], end_idxs[0]
    if end_idx < start_idx:
        return "error", "end anchor occurs before start anchor"

    collapsed = _collapse(lines[start_idx:end_idx + 1])
    return "ok", hashlib.sha256(collapsed.encode("utf-8")).hexdigest()


def _hash_pinned_region_brace(text, start_needle):
    """Like _hash_pinned_region, but the region's end is found by brace
    matching from the start line rather than a second text anchor: the
    region runs through whichever later line's closing brace first brings
    the open-brace count -- counted one character at a time from the start
    line onward -- back to zero. Returns the same (status, detail_or_hash)
    contract as _hash_pinned_region, including the duplicated/missing-start
    guard.
    """
    lines = text.splitlines()
    start_idxs = [i for i, line in enumerate(lines) if start_needle in line]
    if len(start_idxs) != 1:
        return "error", f"start anchor matched {len(start_idxs)} times (need exactly 1): {start_needle!r}"
    start_idx = start_idxs[0]

    depth = 0
    opened = False
    end_idx = None
    for i in range(start_idx, len(lines)):
        for ch in lines[i]:
            if ch == "{":
                depth += 1
                opened = True
            elif ch == "}":
                depth -= 1
        if opened and depth == 0:
            end_idx = i
            break
    if end_idx is None:
        return "error", f"no brace-matched close found for start anchor: {start_needle!r}"

    collapsed = _collapse(lines[start_idx:end_idx + 1])
    return "ok", hashlib.sha256(collapsed.encode("utf-8")).hexdigest()


def check_pinned_regions(app_dir):
    """Hashes each PINNED_REGIONS entry against app_dir's dsp/Delay.hpp and
    prints one PASS/FAIL line per region. Returns True only if every region
    matched its stored hash.
    """
    header_path = os.path.join(app_dir, DELAY_HEADER_RELPATH)
    text = read(header_path)
    all_ok = True
    for name, kind, start_needle, end_needle, expected_hash in PINNED_REGIONS:
        if kind == "brace":
            status, value = _hash_pinned_region_brace(text, start_needle)
        else:
            status, value = _hash_pinned_region(text, start_needle, end_needle)
        if status != "ok":
            print(f"{NAME}: FAIL pinned-region {name} {value}")
            all_ok = False
            continue
        if value != expected_hash:
            print(f"{NAME}: FAIL pinned-region {name} hash {value} expected {expected_hash}")
            all_ok = False
            continue
        print(f"{NAME}: PASS pinned-region {name}")
    return all_ok

# (name, old_text, new_text). old_text is matched verbatim against the
# landed dsp/Delay.hpp and must occur exactly once -- a break whose anchor
# text has drifted (renamed, reformatted, or already fixed differently)
# fails loudly here rather than silently mutating the wrong thing or
# nothing at all.
BREAKS = [
    (
        "modulation-bound-removed",
        "const float modSeconds = std::min(modSecondsRaw, maxModSeconds);",
        "const float modSeconds = modSecondsRaw;",
    ),
    (
        "width-bound-removed",
        "const float widthSpread = std::min(widthSpreadRaw, maxSpreadSeconds);",
        "const float widthSpread = widthSpreadRaw;",
    ),
    (
        "modulation-dropped-from-width-budget",
        "capacitySeconds - baseSeconds - modSeconds);",
        "capacitySeconds - baseSeconds);",
    ),
    (
        "wrap-admits-capacity",
        "while (idx >= capacity)",
        "while (idx > capacity)",
    ),
    (
        "lines-allocated-short",
        "lineL.assign(capacity, 0.0f);",
        "lineL.assign(capacity - 64, 0.0f);",
    ),
    (
        "off-grid-width-balance-headroom",
        "const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds);",
        "const float maxSpreadSeconds = std::max(0.0f, capacitySeconds - baseSeconds - modSeconds "
        "+ (-2.0f) * widthBalance * (widthBalance - 0.5f) * (widthBalance - 1.0f));",
    ),
    (
        "capacity-headroom-shortens-reads",
        "const float capacitySeconds = static_cast<float>(capacity) / sampleRate;",
        "const float capacitySeconds = static_cast<float>(capacity) / sampleRate - 0.01f;",
    ),
    (
        "width-term-removed",
        "const float widthSpreadRaw = p.dwid * baseSeconds * 0.35f * widthBalance;",
        "const float widthSpreadRaw = 0.0f;",
    ),
]


def run_one_break(app_dir, cxx, flag_tokens, source_tokens, name, old_text, new_text):
    """Copies app_dir/dsp into build/break-proofs/<name>/dsp, mutates
    Delay.hpp there, copies each of source_tokens into build/break-proofs/
    <name>/ at its path relative to app_dir (so each copy's own directory
    holds the mutated dsp/ -- see this file's header comment for why a
    quoted include needs that rather than -I order), compiles those copies,
    runs the result, and returns (status, detail, exit_code, seconds) where
    status is "PASS", "FAIL", or a fail reason string.
    """
    break_root = os.path.join(app_dir, "build", "break-proofs", name)
    dst_dsp_dir = os.path.join(break_root, "dsp")
    if os.path.isdir(break_root):
        shutil.rmtree(break_root)
    shutil.copytree(os.path.join(app_dir, "dsp"), dst_dsp_dir)

    header_path = os.path.join(dst_dsp_dir, "Delay.hpp")
    original = read(header_path)
    count = original.count(old_text)
    if count != 1:
        return "FAIL", f"old_text matched {count} times in {DELAY_HEADER_RELPATH} (need exactly 1)", None, 0.0

    with open(header_path, "w", encoding="utf-8") as fh:
        fh.write(original.replace(old_text, new_text))

    abs_app_dir = os.path.abspath(app_dir)
    copied_sources = []
    for source in source_tokens:
        rel = os.path.relpath(os.path.abspath(source), abs_app_dir)
        if rel.startswith(".."):
            return "FAIL", f"source {source} does not live under app_dir {app_dir}", None, 0.0
        dst = os.path.join(break_root, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(source, dst)
        copied_sources.append(dst)

    bin_path = os.path.join(break_root, "break_proof_bin")
    compile_cmd = [cxx, f"-I{break_root}"] + flag_tokens + copied_sources + ["-o", bin_path]
    if compile_cmd[0] != "nice":
        compile_cmd = ["nice", "-n", "10"] + compile_cmd
    compile_start = time.time()
    compiled = subprocess.run(compile_cmd, capture_output=True, text=True)
    compile_seconds = time.time() - compile_start
    if compiled.returncode != 0:
        tail = "\n".join(compiled.stderr.strip().splitlines()[-20:])
        return "FAIL", f"compile failed (exit {compiled.returncode}):\n{tail}", None, compile_seconds

    run_start = time.time()
    ran = subprocess.run([bin_path], capture_output=True, text=True)
    run_seconds = time.time() - run_start
    total_seconds = compile_seconds + run_seconds

    if ran.returncode == 0:
        return "FAIL", "stayed green", ran.returncode, total_seconds

    return "PASS", None, ran.returncode, total_seconds


def main():
    if len(sys.argv) != 5:
        print(f"usage: {sys.argv[0]} <app-dir> <cxx> \"<flags>\" \"<dsp-test-sources>\"", file=sys.stderr)
        return 2

    app_dir, cxx, flags, dsp_test_sources = sys.argv[1:5]
    flag_tokens = shlex.split(flags)
    source_tokens = shlex.split(dsp_test_sources)

    if not check_pinned_regions(app_dir):
        print(f"{NAME}: FAIL")
        return 1

    # Two breaks compile at once (this machine's compile cap); each still
    # gets its own build/break-proofs/<name>/ directory, so the workers
    # never touch each other's files. Futures are collected in submission
    # (table) order below, not completion order, so the printed lines stay
    # in the same order as BREAKS regardless of which break finishes first.
    with ThreadPoolExecutor(max_workers=2) as executor:
        futures = [
            executor.submit(run_one_break, app_dir, cxx, flag_tokens, source_tokens, name, old_text, new_text)
            for name, old_text, new_text in BREAKS
        ]
        results = [future.result() for future in futures]

    any_failed = False
    total_seconds = 0.0
    for (name, old_text, new_text), (status, detail, exit_code, seconds) in zip(BREAKS, results):
        total_seconds += seconds
        exit_field = "n/a" if exit_code is None else str(exit_code)
        print(f"{NAME}: {status}  {name}  exit={exit_field}  {seconds:.2f}s")
        if status != "PASS":
            any_failed = True
            reason = detail if detail else "unknown failure"
            print(f"{NAME}:   reason: {reason}")

    print(f"{NAME}: total {total_seconds:.2f}s across {len(BREAKS)} breaks")

    if any_failed:
        print(f"{NAME}: FAIL")
        return 1

    print(f"{NAME}: OK -- every break was rejected")
    return 0


if __name__ == "__main__":
    sys.exit(main())
