# Tasks — `frogg3rs-a-page-is-never-a-bank`

Every new test is shown to fail with its production change reverted, by the
executor, before the task is reported done, and the report says so. Builds
run under `nice`, `-j2`, one at a time, and never two suites at once.

This is a wording-only change: no dispatch, no serialized value, and no
resolved index changes for any player who never opens this diff. Every task
below is checked against that claim, not against a new behavior, because
there is none.

Classification rule used throughout (from `proposal.md`): a "bank" hit is
**renamed** if it names the six-way page-selection concept (what
`FroggersActions::kResetPage`/`kRandomizePage` already call "page"); it is
**left alone** if it names Sheaf's own `synth::Bank` type/API, Froggers'
`FroggersBankId` parameter-model address, a serialized wire value, or the
Twister/APC40's genuine onboard hardware bank; a third case —
`EnvelopeFollowers.hpp`'s citation of the retired simulator's
`V2EnvelopeFollowerBank.hpp` — is a different, unrelated sense (a bank of
DSP units) and is also left alone.

- [x] 1. Enumerate and classify every hit before any file is edited. Run, and
      paste the literal output into the task report:
      `grep -rIn -i bank app/dsp/*.hpp` (expect 25, per `proposal.md`'s
      count) and classify each by the rule above, printing FOUND (25) vs. the
      count that is page-concept (rename) vs. Sheaf-`Bank`/third-sense (keep).
      Do the same for `app/FroggersMidiCatalog.hpp`, `app/FroggersUiSurface.hpp`,
      `app/FroggersAppCore.hpp`, `app/vst/FroggersPluginProcessor.{hpp,cpp}`,
      and `MANUAL.md`, each against the same rule; `app/FroggersModulation.hpp`,
      `app/FroggersParameters.hpp`, and every `*Tests.cpp` file are excluded
      from this enumeration — their "bank" hits are `synth::Bank`/
      `FroggersBankId` call sites, out of scope per `proposal.md`'s
      `FroggersBankId` decision, confirmed by this task only insofar as no
      hit outside the identifiers named in Task 2 needs a symbol rename in
      those files.
      Check: the report states FOUND vs. renamed vs. kept per file, with the
      keep list naming which of the four kept categories each hit falls
      into. A hit this task cannot classify against one of the four kept
      categories or the rename rule is not silently left as-is; it is
      reported and the task stops for a ruling.
- [x] 2. Rename the outward accessor/action identifiers, symbol names only,
      wire string values untouched:
      - `app/FroggersUiSurface.hpp`: `FroggersActions::kBankNext`→
        `kPageNext`, `kBankPrevious`→`kPagePrevious`, `kBankSelect`→
        `kPageSelect`; `FroggersNodeIds::kBankTabsRow`→`kPageTabsRow`,
        `kBankPrevArrow`→`kPagePrevArrow`, `kBankNextArrow`→`kPageNextArrow`;
        `CurrentBankIndex()`→`CurrentPageIndex()`. Each `constexpr const
        char*` declaration keeps its current string literal and gains a
        one-line comment: the value is a stored wire identifier and does not
        follow the symbol's name (see `proposal.md`'s persistence trace).
      - `app/FroggersAppCore.hpp`: `ActiveBankIndex()`→`ActivePageIndex()`,
        `activeBankIx_`→`activePageIx_`, `FroggersVisibleBankIndex()`→
        `FroggersVisiblePageIndex()`, `RequestBankSelect()`→
        `RequestPageSelect()`.
      - `app/FroggersParameters.hpp`: `kFroggersBankCount`→
        `kFroggersPageCount` (value `6` unchanged).
      - `app/vst/FroggersPluginProcessor.hpp`/`.cpp`: `kVisibleBankIndexKey`→
        `kVisiblePageIndexKey` (C++ name only; the JSON key string
        `"visibleBankIndex"` is unchanged, with a comment matching the one
        added above).
      - Every call site of the above across `app/` and `app/vst/`, including
        every `*Tests.cpp` file, updated to the new names.
      Check: `grep -rn 'kBankNext\|kBankPrevious\|kBankSelect\|kBankTabsRow\|kBankPrevArrow\|kBankNextArrow\|CurrentBankIndex\|ActiveBankIndex\|FroggersVisibleBankIndex\|kFroggersBankCount\|kVisibleBankIndexKey\|activeBankIx_\|RequestBankSelect' app/ app/vst/`
      prints nothing. `grep -c '"froggers.bank.next"\|"froggers.bank.previous"\|"froggers.bank.select"\|"froggers.bank.prev"\|"visibleBankIndex"' app/FroggersUiSurface.hpp app/vst/FroggersPluginProcessor.cpp`
      prints the same counts as before this task (the wire strings are
      unmoved — record the before/after counts in the report). The full
      test suite (Task 7) passes with these names.
- [x] 3. Rename catalog display labels in `app/FroggersMidiCatalog.hpp`:
      `"Bank Next"`→`"Page Next"`, `"Bank Previous"`→`"Page Previous"`,
      `"Bank " + std::to_string(ix + 1)`→`"Page " + std::to_string(ix + 1)`.
      These are the third and fourth fields of each `catalog.actions.push_back`
      entry — display text, never serialized (Task 1's persistence trace
      covers only `appAction`/`appActionValue`/`shiftedAppAction`/
      `shiftedAppActionValue`, none of which is the label). Regenerate the
      manual's Twister artifacts: `make manual-diagrams`, then commit
      `assets/manual/twister-controls.json` and the two preset PNGs.
      Check: `make check-twister-manual-diagrams-drift` passes;
      `grep -n '"press": "Page Next"\|"shiftedPress": "Page Previous"'
      assets/manual/twister-controls.json` finds both, and
      `grep -c 'appAction.*froggers.bank' assets/manual/twister-controls.json`
      is unchanged from before this task (the wire values in the generated
      asset move with the source, not with the label rename).
- [x] 4. Update `app/check_docs_match_parameter_table.py`'s MANUAL.md
      heading suffix from `" bank"` to `" page"` (the tuple at the bottom of
      `app/check_docs_match_parameter_table.py` pairing `"MANUAL.md"`, the
      manual path variable, and `" bank"`).
      `QUICK_DICT.md`'s own headings are already bare (`## Audio`, never
      `## Audio bank`) and are unchanged.
      Check: `make check-docs-match-parameter-table` still fails at this
      point in the sequence (MANUAL.md's headings have not been renamed
      yet — Task 5 does that); confirming it fails here, and specifically on
      the heading text rather than on a script error, is this task's
      positive control that the suffix change is live. Re-run after Task 5;
      it passes there.
- [x] 5. Rewrite MANUAL.md per Task 1's classification: the "Bank selection"
      section heading and body, the six `## <Name> bank` section headings
      (become `## <Name> page`), "Bank Next"/"Bank Previous" and "Bank 1"
      through "Bank 6" in the controller-mapping prose and tables (APC40 and
      Launchpad subsections), and the running prose using "bank(s)" for the
      page concept ("Six parameter banks," "switching banks," "that bank's
      own 14 page parameters," "the Audio parameter bank," and siblings).
      Leave unchanged the two sentences naming the Twister's own onboard
      hardware bank: "Bank Side Buttons" (the Midi Fighter Utility setting)
      and "whatever bank the Twister shows."
      Check: `make check-docs-match-parameter-table` passes (continued from
      Task 4). `grep -c -i bank MANUAL.md` is exactly 2, and
      `grep -n -i bank MANUAL.md` prints only the "Bank Side Buttons" and
      "whatever bank the Twister shows" lines — pasted into the report.
- [x] 6. Rename `app/dsp/*.hpp` comments per Task 1's classification (page
      concept only; `EnvelopeFollowers.hpp`'s `V2EnvelopeFollowerBank.hpp`
      citations untouched).
      Check: `grep -rIn -i bank app/dsp/*.hpp` prints only the citations to
      `V2EnvelopeFollowerBank.hpp`, pasted into the report, and the count
      matches Task 1's "keep" count for `app/dsp/`.
- [x] 7. Add `app/check_no_bank_page_conflation.py`, following
      `app/check_no_planning_history.py`'s shape (import the shared tree walk
      from `app/check_common.py`, a compiled pattern list, an `ALLOWED` list,
      one exit
      code). Patterns: `Bank(Next|Prev|Select|Tab|Button|Row|Switcher)`
      (no anchors — matches the shape a reintroduced identifier takes, not
      a fixed list of the six original names), `BankIndex\b`,
      `\bkFroggersBankCount\b`, `\bkVisibleBankIndexKey\b` (case-sensitive —
      these are C++ identifiers, not prose), plus a string-literal check for
      the retired button labels (`"Bank Next"`, `"Bank Previous"`, the
      `"Bank "` half of `"Bank " + std::to_string(ix + 1)`), scanned over
      `app/*.hpp`, `app/*.cpp`, `app/vst/*.hpp`, `app/vst/*.cpp`,
      `app/dsp/*.hpp`. A second scan of `MANUAL.md` fails on any
      case-insensitive `\bbank\b` outside two allowed lines, matched by the
      literal substrings "Bank Side Buttons" and "whatever bank the Twister
      shows." Wire the script into
      `app/Makefile` as `check-no-bank-page-conflation`, added to
      `.PHONY` and to whichever aggregate target runs the other
      `check-*` scripts today (read the Makefile for that target's name
      rather than assuming one).
      Check, in this order, as the omni rule's break-then-restore control
      (`rm` any build artifact between runs so a stale binary cannot report
      the earlier run's result):
      (a) with the tree as this change leaves it, `make
      check-no-bank-page-conflation` passes;
      (b) temporarily reintroduce one instance of `kBankNext` in
      `app/FroggersUiSurface.hpp` (a `TEMP-BREAK`-tagged, uncommitted edit)
      and confirm the same command fails, naming the file and line;
      (c) revert the temporary edit and confirm the command passes again.
      All three outcomes, with their literal output, go in the report.
- [x] 8. Run the full app suite and the host suites CI runs, reading the
      workflows for which those are, by running every test binary by path
      after `make test` stops at the carried deadline tests.
      Check: pass and fail counts reported per binary as measured; every
      failure is either fixed here or shown to fail identically before this
      change, at the commit this change is based on. A test named red in
      this report is reported, never edited. Because this change alters no
      serialized value and no runtime behavior, a test that newly fails here
      is evidence the classification in Task 1 or 2 put something in the
      wrong list — not a test to adjust.
