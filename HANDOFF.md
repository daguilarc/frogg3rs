# Handoff — `frogg3rs-delay-width-wysiwyg-repair`

Start by reading `omni-rule.md`, then
`openspec/changes/frogg3rs-delay-width-wysiwyg-repair/proposal.md` and
`tasks.md`. Everything below is in those two files; this page tells you where to
put your first hour so you do not spend it the way the last six sessions did.

## The one thing that matters

**The defect is fully diagnosed and nobody has ever tried to fix it.** Six
sessions reached it, wrote it up, and handed it forward. The parallel defect in
the reverb tank was repaired in the same lineage while this one was passed on
again. Deferral is the failure mode here, not any rejected repair.

So: do not re-diagnose it, do not re-measure it to satisfy yourself, and do not
propose a follow-up change for it. Go to task 1.1 and start measuring the one
question that is still open.

An operator ruling already settles whether it should be fixed — `04efe9c`,
2026-08-29: *"the signal SHALL NOT be collapsed to mono in the middle of the
chain. Folding belongs at the output."*

## What is already on `main`

- `18f99b5` — Reverb's Density travel pinned against Stereo width's; the
  correlation helper lifted to file scope; four inherited figures withdrawn.
- `b05e83a` — `MANUAL.md` / `QUICK_DICT.md` for both pages' Diffusion, Density,
  Stereo width and Damping.
- `c5fe881` — this change created, the previous one superseded and deleted.

Baseline is **394 passing, 0 failing**. Both earlier commits had a fresh-context
postflight before they landed. Do not re-do them.

## The open question, which is task 1.1

Can the cross-feed widen this signal **at all**? Both delay lines get the same
mono input and differ only in read time, so cross-feeding blends two
time-shifted copies of one signal — which re-correlates, reaching exact mono at
weight 0.5. If no cross weight widens a mono-fed pair, that decides the
mechanism: the cross-feed comes off the Width knob, and this specification has
already made that exact move twice (the reverb tank's cross-feed retired to a
fixed coupling; Drive's Link kept as a named constant).

Measure it before choosing. Do not assume it, in either direction.

## Do not re-derive these — they are traced and in the proposal

1. `dsp::CrossFeedPair` is **shared** with `dsp::Reverb::Process`. Never edit the
   helper; the repair belongs in the delay's weight computation.
2. `stereo_delay_cross_feed_reproduces_its_captured_output_exactly` is a
   **self-capture** certifying that a de-duplication refactor changed nothing —
   not a parity pin. Recapture it, and say in its own comment that the capture
   point moved and why. Silently recapturing it is the thing an auditor will
   correctly reject.
3. **No firmware parity exists.** The Daisy firmware never shipped a delay;
   `git show f236915^:sim/StereoDelay.hpp` carries `const float cross = p.dwid *
   0.5f;` verbatim. The defect is original to this project.
4. At `dwid == 0` the cross-feed is a **no-op at any weight**, so any law that
   agrees with the current one there costs nothing at the registered default.
5. **Width balance is the parent defect.** Designed as complementary weights —
   the ratio between spread and cross-feed — and shipped as a common scalar on
   both. Its design document is
   `openspec/changes/archive/2026-08-18-frogg3rs-post-expansion-consolidation/research/RESEARCH2-drive-delay.md`,
   section "Width Balance — `WBal` — NEW". That `research/` directory is where
   prior sessions did the Valhalla and Bitwig work; read it before designing
   anything.

## What will actually get you rejected

Every audit rejection in this lineage hit **prose** — a figure, a bound, an
assertion's wording, a scope note. Not one ever rejected DSP code. The
executors' work held; the coordinating context's own text is where the defects
were. Route your own write-up through a fresh context exactly as you route an
executor's.

Specific traps this session hit, all of them cheap to avoid:

- `make test` halts at a pre-existing gate failure **before building anything**.
  Use `make -k`, then run every test binary by path.
- `app/Makefile`'s `BUILD_DIR` is an **absolute** path, so `make build/<binary>`
  has no rule and make exits **0** saying "up to date" for any binary that
  already exists. A failed rebuild hides completely. Build the absolute-path
  target or `rm` the binary first.
- Cap builds at `nice -n 10 make -j2`; this Mac freezes above that.
- Two gate failures are **not yours**: `check-spec-checks-resolve` on
  `openspec/changes/frogg3rs-randomize-depth-reclaim/`. Another session is
  authoring that change in this same working tree. Name the exclusion at every
  stage gate; never let "green" soften to cover it. A third failure is yours.
- Never `git add -A`, `git add .`, or `git commit -a` — two untracked change
  directories belong to other sessions.
- A check that passes **more comfortably when the thing it measures is dead** is
  worse than no check. One shipped in `18f99b5` and was caught only by an
  adversarial read: a ratio assertion went vacuous when its denominator hit zero.
  Ask that question of whatever you land.

## State of the tree

Clean, apart from two untracked directories owned by other sessions
(`frogg3rs-midi-controller-resilience`, `frogg3rs-randomize-depth-reclaim`).
Leave both alone.

Stage 1 has not started. Stage 2 (documents) and stage 3 (spec delta and
archival) depend on it, because the repair makes the Stereo width entries that
`b05e83a` just wrote stale.
