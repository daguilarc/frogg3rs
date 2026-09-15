# Adjudicated preflight findings, carried forward from the superseded change

The change this one supersedes, `frogg3rs-delay-width-wysiwyg-repair`, had its
stage 1 executed and then ran a full preflight: five axes concurrently
(checklist compliance, executability, spec-versus-code truth, hygiene and blast
radius, adversarial against the checks), a debate round across all five, and
adjudication by a context that did not participate. The adjudicator verified
rather than weighed, and treated convergence as worth nothing.

These are its rulings. They are carried here as KNOWN defects this change must
close, not as findings to rediscover.

## Did not survive verification

Three claims the auditors raised were killed by the adjudicator.

- Task 2.5's premise. `HANDOFF.md` item 4 already carries its correction and the
  heading already reads 1.1a/1.1b. The task is a no-op confirm-and-close, and
  nothing breaks if it is not "fixed".
- Task 1.4's missing baseline method. A recorded baseline of 394 passing and 0
  failing across twelve binaries IS the complete before-state; no stash or
  checkout choreography is needed to reconstruct it.
- A contradiction between the two terminating executability reports. Git history
  shows the Send-pinning sentence absent before `0260ce9` and present after, and
  the second report names the first's finding and re-verifies it against source.
  That is a fix-then-reverify cycle, not an unreconciled contradiction.

## Open

The pan-law adversarial result. An attack reporting `|corr| = 0.980831035` at
width 0.25 was produced during the audit, but no tracked artifact records the
run, and a figure that does not travel with its command is a manufactured
premise no matter who produced it. The mechanism argument — Pearson correlation
is invariant under per-channel scaling, so a pan-only law cannot clear a
correlation bound — is sound and independent of the figure. This converts to a
task: the adversarial pass must produce and RECORD this run before anything
cites it.

## Blocks execution

1. The two golden-vector pins, `stereo_delay_cross_feed_reproduces_its_captured_output_exactly`
   and `stereo_delay_freeze_at_default_reproduces_pinned_original_output_through_real_process`,
   fail against the landed code and have not been recaptured. Confirmed by a
   real run: 188 passing, 2 failing in that binary.
2. The pin enumeration has two confirmed holes.
   `stereo_delay_width_balance_mapping_keeps_cross_in_0_1_and_spread_at_or_below_todays_max`
   never calls `Process` and recomputes a formula production no longer runs, so
   its own state never changes. And `randomize_all_storm_test_never_blows_out_or_permanently_silences`
   drives randomized width and feedback through the real router while carrying
   none of the operand tokens an enumeration searches for, and its silence check
   takes one scalar maximum across both channels pooled, so one dead channel
   beside a live one never trips it.
3. Five comments in `app/dsp/Delay.hpp` are false of the code as it now stands:
   the provenance header, `SetWidthBalance`'s doc comment, the comment preceding
   the width-spread computation, the comment preceding the cross-feed
   assignment, and the wet-limiter comment that credits cross-feed with keeping
   the channels close.
4. The landed decorrelation check's required proof that each assertion can fail
   leaves no evidence in the tracked tree. The adjudicator reran the correlation
   break itself and confirmed the assertion does go red, so the mechanism works
   — what is missing is the record, not the property.
5. The liveness gate is written `rmsL != 0.0 && rmsR != 0.0`, which is TRUE for
   a NaN under IEEE-754, so it cannot reject a non-finite instrument. The check
   still rejects NaN, but only because two degenerate branches coerce it to a
   1.0 sentinel that exceeds both thresholds — an accident, not a guard.

## Blocks delivery

1. `research/INDEX.md`'s header still says none of its six files exist. All six
   exist, populated and committed.
2. The stereo-image scenario's check prose says the repair makes "the two
   mechanisms widen together" — false, since one mechanism remains — and names
   no test.
3. The slot-12 Width Balance clause is false of the code and made worse by the
   repair: with the cross-feed weight fixed at zero, the ratio it describes is a
   division by zero. It carries no not-yet-delivered marker.
4. `MANUAL.md` and `QUICK_DICT.md` still describe the cross-feed riding the
   feedback path and reaching the repeats once Feedback or Freeze leaves zero.
5. `frogg3rs.code-workspace` still excludes two paths that do not exist.
6. The capacity grid-sweep check recomputes the production bound inline rather
   than calling production, omits the modulation term production subtracts, and
   hardcodes the capacity rather than naming the constant. Removing the
   production clamp leaves it passing unchanged.
