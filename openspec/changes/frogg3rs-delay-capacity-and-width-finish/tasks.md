# Tasks — Finish the Delay capacity and width repair

**Work not listed here does not become a task unless the operator adds it.** An
executor meeting a defect this plan does not name reports and STOPS.

## Mechanics

Every stage ends in one commit and a push, after that stage's postflight. Stage
only the paths you name. `git add -A`, `git add .` and `git commit -a` are
forbidden — two untracked sibling change directories belong to other sessions,
and twenty-two staged deletions of the superseded change directory
frogg3rs-delay-width-wysiwyg-repair under openspec/changes (count them with `git status --short | grep -c '^D '`) must land with this
change's first commit, untouched otherwise. No AI
attribution. Delivery is a push to `main`, never a pull request.

**Suite green excludes one PATH, not a count:** any `check-spec-checks-resolve`
failure reported under the randomize-depth-reclaim change's own directory
under openspec/changes.
Re-measure every gate; a recorded count is not evidence about the next run.

**Builds:** `make test` halts early — use `make -k`, then run all twelve
binaries by absolute path. `BUILD_DIR` is absolute, so `make build/<binary>`
exits 0 without rebuilding; `rm` the binary first. Cap at `nice -n 10 make -j2`.

**Never kill an executor mid-procedure.** One was stopped while it had checked
out `HEAD` versions to capture a baseline, leaving the tree silently reverted.
If an agent must be stopped, verify the tree against a known snapshot afterward
rather than assuming it is where you left it.

**Any measurement or break-proof is recorded in `research/` as part of the task
that produces it.** Not reported and not written down means it did not happen —
an adjudicator on the predecessor threw out a figure for exactly that reason,
and two more were lost the same way.

## Stage 1 — the repair (LANDED, uncommitted; four defects open)

- [x] 1.1 Three ordered zero-headroom capacity bounds — base, then modulation,
      then width spread. All three routes closed, measured at 48 and 96 kHz.
- [x] 1.2 The liveness gate rejects non-finite directly; the grid check drives a
      real object at 96 kHz across four swept parameters; the near-capacity
      assertion is inclusive.
- [x] 1.3 The decorrelation check's four falsifiability proofs, recorded in
      `research/decorrelation-check-falsifiability.md`.
- [x] 1.4 Pin enumeration, both golden vectors recaptured, width-balance
      mapping test rewritten. Before-state measured at 394/0.
- [x] 1.4a The pinned-parameter gate, wired into `app/Makefile`, with two
      independent red proofs. Storm test now checks per channel.
- [x] 1.5 Five amended comments in `app/dsp/Delay.hpp`.
- [x] 1.6 Wet-limiter measurement: independent per-channel limiting DOES
      produce interchannel divergence (~0.5-0.7 dB) at roughly twice the
      shipped instrument's amplitude, isolated to width by a zero-width
      control. Whether real input reaches that level is untraced.
      `research/wet-limiter-independence-image-shift.md`.
- [x] 1.8 The tautological assertion is removed; the test is renamed
      `stereo_delay_width_balance_mapping_keeps_spread_at_or_below_todays_max`
      and keeps only the spread bound, which is a real assertion.
- [x] 1.9 The grid check's gap is closed — its swept `dtim` values are now
      `{0.0, 0.5, 0.9, 0.99, 1.0}` — and proven: deleting the modulation term
      from the width budget turns the grid red at 0.9 (measured 6216 against
      expected 96000). `research/grid-widening-break-proof.md`.
- [x] 1.11 Break-proofs for the three capacity bounds recorded in
      `research/capacity-bound-break-proofs.md`. Confirmed: breaking the base
      bound alone produces no violation, because the modulation bound
      telescopes an unclamped base back to capacity.
- [x] 1.12 Two compares inside `Process` — `timeL <= capacitySeconds`,
      `timeR <= capacitySeconds` — gated on `FROGGERS_DSP_CHECKS`, which
      `app/Makefile` passes to every binary it builds and no shipping build
      passes. A bare `assert` was the wrong instrument: the browser build
      compiles without `NDEBUG`. Holds across the suite; fires on both reads
      when a bound is removed. `research/process-capacity-assertion.md`.
- [x] 1.12a Adversarial postflight of the shipped checks: three attackers in
      one batch, identical briefs, none shown another's brief or the prior
      audits; an exchange round; adjudication by a context that did not
      participate. `research/adversarial-postflight-{a,b,c}.md` and
      `research/adversarial-postflight-adjudication.md`. Five findings, all
      reproduced by the adjudicator; the dropped-modulation-term break was
      tried by all three and caught by all three. Four rulings block delivery
      and one blocks execution; each becomes a repair below, and every repair
      is new untraced text that gets its own attacker batch (1.18).
- [x] 1.14 Index-domain compares, gated the same way, against the vector
      actually indexed: in `ReadAt`, `idx0 < line.size()` and
      `idx1 < line.size()`; in `WriteSample`, `writePos < line.size()`.
      Closes finding P (a wrap admitting `capacity` reads one past the end,
      green) and Q (lines allocated shorter than `capacity`, green). Prove
      each fires. `research/index-domain-checks.md`.
- [x] 1.15 A random-walk capacity test in the parity suite: one `StereoDelay`
      driven continuously through random `dtim`, `dwid`, `dmod` and Width
      balance at 44.1, 48, 88.2 and 96 kHz, with the compile-gated compares
      as its capacity oracle and per-point finiteness as its own assertion.
      The parity test file refuses to compile without `FROGGERS_DSP_CHECKS`
      (`#error`), so the oracle cannot be silently absent. Closes finding R
      (a bound defect shaped to vanish at the grid's three Width balance
      values). Prove it fires on that exact defect.
      `research/random-walk-capacity-test.md`.
- [x] 1.16 `check_delay_capacity_parameters_are_swept.py` strips `//` and
      `/* */` comments, honouring string literals, before selecting bodies
      and collecting literals. Closes finding S (a decoy comment keeps the
      gate green while the sweep collapses). Prove: the decoy goes red; the
      unmodified tree reports the same value lists as before.
- [x] 1.17 A mutation gate, `check_delay_capacity_break_proofs.py`, wired
      into `make test`: for each break in an explicit table (modulation bound
      removed; width bound removed; modulation term dropped from the width
      budget; wrap admitting `capacity`; lines allocated short; the cubic
      Width-balance headroom; a 10 ms capacity headroom that shortens reads
      near capacity), compile the parity suite against a mutated copy of
      `dsp/` and the test sources with the Makefile's own flags, require the
      compile to succeed and the run to exit non-zero. The seventh break
      passes the compile-gated compares and is rejected only by the tests'
      expected-versus-measured assertions; weakening those makes it stay
      green and the gate fail, which is the gate's own liveness proof. The base-bound break is excluded
      because it is known to stay green. This is the check that fails when
      the tests are weakened, which is the half of finding T no fixed check
      can reject otherwise, and it makes the break-proof records permanent.
      Recorded: 218 s across seven breaks. `research/break-proof-gate.md`.
- [x] 1.18 Fresh attacker batch against 1.14–1.17 (identical briefs, one
      batch, no prior findings attached), exchange round, adjudication.
      `research/adversarial-postflight-2-{a,b,c}.md` and
      `research/adversarial-postflight-2-adjudication.md`. Three findings,
      all true; none gets past `make test`: defining `NDEBUG` ahead of the
      header or deleting the compares outright both fail the break-proof
      gate before the suite builds, and a decoy test body defeats only the
      sweep gate's literal scan, which its docstring now says is textual.
      No repair, so no third batch.
- [x] 1.19 Postflight repairs from the diff, repetition, scenario and hygiene
      axes (`research/postflight-*.md`): the Width-balance field comment no
      longer names the retired 0.5f cross-feed literal; `.PHONY` lists every
      gate; `check_delay_capacity_break_proofs.py` imports `read` from
      `check_common.py`; the capacity-in-samples formula has one definition,
      `StereoDelay::CapacityForSampleRate`, used by `SetSampleRate` and by
      every test that needs it; the comment stripper lives in `app/check_common.py`
      and both gates that strip comments import it; the parity suite's
      identical LCG steps share one helper, with the run log's printed
      figures bit-identical before and after. Stage 3 items the axes
      re-confirmed stay with their Stage 3 tasks.
- [x] 1.10 Record the twelve-binary before/after pass-state diff in `research/`,
      after 1.18, so the after-state is the tree that ships. Before-state
      recorded: all twelve pass at HEAD, parity suite 186/186; after:
      all twelve pass, parity suite 191/191, every gate OK.
      `research/twelve-binary-pass-state-diff.md`. Capture the
      before-state by copying the changed source files aside,
      `git checkout HEAD --` them, measuring, restoring, verifying by md5. NOT
      `git stash` — staged deletions are in the index.
- [x] 1.13 Stage gate: suite green with the path exclusion re-measured; the
      non-adversarial postflight axes in a fresh context (diff landed code
      against the proposal; repetition enumeration against the DIFF; every
      promoted scenario backed or marked not yet delivered); commit and push.

## Stage 2 — the documents

- [x] 2.1 `MANUAL.md` and `QUICK_DICT.md` Delay **Stereo width** entries now say
      the offset is the whole of the widening, nothing rides the feedback path,
      and the read never leaves the line.
- [x] 2.2 The same two files' **Width balance** entries: scales the time offset
      Stereo width produces, full at the default top of travel, none at the
      bottom.
- [x] 2.3 `frogg3rs.code-workspace`'s `files.watcherExclude`: checked against the
      filesystem, the node_modules glob resolves (a vendored node tree and
      Sheaf's browser install), so only the wasm/build and desktop/build
      entries were stale; those two are removed.
- [x] 2.4 Before staging, run `git diff HEAD -- MANUAL.md QUICK_DICT.md` — a
      bare `git diff` misses what the other session already staged — and stage
      only this change's hunks with `git add -p`. Re-run immediately before
      committing. If another session's edits are present, stop and report.
- [x] 2.5 `HANDOFF.md` removed. It was titled after a deleted change and
      described a tree state that no longer exists; rewriting it would
      duplicate this file, which is the handoff. Its inbound mentions are all
      records under `research/` and stay as history.
- [x] 2.6 Stage gate: suite green (every gate OK, 191/191, 49/49, 9/9), one
      postflight in a fresh context with no adversarial axis, zero findings
      (`research/postflight-stage-2.md`); committed and pushed.

## Stage 3 — the spec delta and archival

- [ ] 3.1 The stereo-image scenario: repoint its `Check:` onto the landed test,
      correct the diagnosis (the page drives ONE mechanism, not two), and mark
      the AND clause vacuous for Delay in the same form the file already uses
      for Reverb.
- [ ] 3.2 Mark the slot-12 Width Balance clause not yet delivered. It was false
      before any repair — both mechanisms scaled by the same factor, which
      cancels — and is now division by zero. Do not reword it to match code.
- [ ] 3.2a Land a check for the whole-travel clause at Feedback 0. The evidence
      exists (`research/instrument-derivation.md`: 1.000, 0.775, 0.571, 0.439,
      0.345 at `dfbk = 0.00`); no check exercises it. Keep Send raised. Prove it
      can fail. THIS CHECK GETS A SCOPED ADVERSARIAL PASS — every check this
      chain has shipped had a hole.
- [ ] 3.3 Re-count every MODIFIED requirement against the promoted text by
      hand; the restate gate keeps only dash-opening lines.
- [ ] 3.4 Resolve every scenario's `Check:` by grepping. No `Check:` may point
      at this change's own `tasks.md` — archival moves it.
- [ ] 3.5 Repoint the promoted spec's two transitional NOTEs. One cites
      the superseded change's tasks file under
      openspec/changes/frogg3rs-delay-width-wysiwyg-repair, already absent
      from the tree. Neither shipped gate reads NOTE prose.
- [ ] 3.6 Carry the "same job across pages" violation to the operator as a
      recorded finding. It is settled, not open: the requirement's own first
      sentence defines "job" as mechanism. Do not reword the requirement.
- [ ] 3.7 Reconcile `research/INDEX.md` against its directory IN BOTH
      DIRECTIONS. Its header names a superseded change and claims its files do
      not exist; the directory holds sixteen. Count, do not trust a list.
- [ ] 3.7a Repair citations archival will strand. `research/read-at-capacity.md`
      cites `preflight-6.md` through `-10.md` by bare name, and
      `axis-0.1-capacity-guard-measurement.md` cites `preflight-10.md`. Those
      resolve only inside a directory stage 3.8 removes, and no gate scans
      `openspec/changes/**/research/`. Inline what each said, or repoint at
      `prior-preflight-adjudication.md`.
- [ ] 3.7b Fix `research/decorrelation-check-falsifiability.md`'s stale line
      citation. Cite the SYMBOL, not a line range — this change edits that file
      and every line citation into it decays on contact.
- [ ] 3.8 Archive, promoting the delta. Remove the predecessor directories.
      Confirm the dangling reference in `frogg3rs-randomize-depth-reclaim`'s
      own table is still recorded here; it is out of scope to edit.
- [ ] 3.9 Stage gate: suite green, one postflight in a fresh context, commit
      and push.

## Audits

- [ ] A.1 Preflight in a context that did not write this change, if its triggers
      fire. This plan is a restatement of settled work; if nothing here rests on
      an unverified behavioural premise, say so and skip rather than running a
      round to produce process.
- [ ] A.2 Each stage gets one postflight in a fresh context before its commit.
      Axes: diff landed code against the proposal; re-run the repetition
      enumeration against the DIFF; confirm every promoted scenario is backed by
      a check that passes now or is marked not yet delivered. Add an adversarial
      axis ONLY where the stage ships something whose job is to reject.
- [ ] A.3 Adversarial passes run CONCURRENTLY AND INDEPENDENTLY — see the
      handoff. Two or three at once, none shown the others' briefs or the prior
      audit files. Convergence between them is the only evidence of coverage
      you can get; a relay of one-at-a-time passes destroys it.
