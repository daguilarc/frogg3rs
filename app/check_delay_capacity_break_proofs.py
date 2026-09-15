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

import os
import shlex
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_common import read  # noqa: E402

NAME = "check-delay-capacity-break-proofs"

DELAY_HEADER_RELPATH = os.path.join("dsp", "Delay.hpp")

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

    any_failed = False
    total_seconds = 0.0
    for name, old_text, new_text in BREAKS:
        status, detail, exit_code, seconds = run_one_break(
            app_dir, cxx, flag_tokens, source_tokens, name, old_text, new_text
        )
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
