# Adversarial audit of the four inherited checks -- third independent pass

Auditor axis: same as `research/adversarial-check-audit.md` and
`research/adversarial-check-audit-2.md` (get a broken mechanism past
`stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width`,
`stereo_delay_width_spread_never_reads_past_the_line_capacity`,
`stereo_delay_width_spread_bound_is_inert_away_from_capacity`, and
`stereo_delay_width_spread_bound_holds_across_the_reachable_grid`, all in
`app/FroggersDspParityTests.cpp`, checking `app/dsp/Delay.hpp`'s
`StereoDelay::Process`), both uncommitted (`git diff HEAD`). Read both prior
audits in full before mounting anything below; the attack here is not a
re-run of either -- it is derived from a different pinned quantity than
either pass named.

Method: read `app/dsp/Delay.hpp`'s `StereoDelay::SetSampleRate` and
`StereoDelay::Process` (the capacity guard and every quantity that feeds
`ReadAt`) end-to-end, then enumerated every value the four shipped checks
hold fixed rather than sweep -- `dmod` (found by pass 1), `widthBalance`
(found by pass 2), and a third one neither pass named: **sample rate
itself**. All three checks that call `StereoDelay::Process()` hardcode
`sampleRate = 48000.0f` as a local constant; the grid-sweep check (which
never calls `Process()` at all -- pass 2's Attack 6) hardcodes its own
`sr = 48000.0f` too. Sample rate is never varied anywhere in the four
checks.

Backed up both files first (`/tmp/attack_backup3/*.orig`), confirmed their
md5 matched both prior passes' recorded baseline before any edit:
- `app/dsp/Delay.hpp` = `2090d9902bae29e67d7eb57a27991cef`
- `app/FroggersDspParityTests.cpp` = `35b80ce13d6a4ffebae993663eabcddf`

Both match again after the attack's revert (checked immediately after the
mutation and confirmed a final time at the end of this audit -- see the
`md5`/`git status --short` output at the bottom). No production code was
edited for this attack -- it demonstrates a gap that already exists in the
current, unmutated tree, the same way pass 1's Attack 1 and pass 2's
Attack 5 did.

Built and ran for real via `nice -n 10 make -j2 "$(pwd)/build/froggers_dsp_parity_tests"`
from `app/`, `rm`-ing the binary before each rebuild
(`app/build/froggers_dsp_parity_tests`).

---

## Attack 7 (GOT THROUGH -- real, unmounted production gap): capacity guard's own geometry assumes 48kHz; any sample rate above it overruns with Stereo width fully OFF

**The mechanism.** `StereoDelay::SetSampleRate` (Delay.hpp):

```cpp
capacity = std::min(kMaxDelaySamples, static_cast<size_t>(std::ceil(kMaxDelaySeconds * sampleRate)));
```

`kMaxDelaySamples` is a fixed `96000`-sample ceiling, independent of
`sampleRate`. `kMaxDelaySeconds` is `2.0f`. At `sampleRate == 48000.0f`
(the value every one of the four shipped checks hardcodes), `ceil(2.0 *
48000) == 96000`, so the `min()` is a no-op and `capacitySeconds ==
capacity/sampleRate == 2.0s == kMaxDelaySeconds` exactly -- the boundary
case pass 1 and pass 2 both already probed, at zero headroom by
construction.

At ANY sample rate above 48000 -- and 88.2kHz, 96kHz, 176.4kHz and 192kHz
are all standard audio-interface rates, not exotic ones -- `ceil(2.0 *
sampleRate) > 96000`, so the `min()` now actually clamps: `capacity` stays
pinned at `96000` samples while `sampleRate` has grown, so
`capacitySeconds = capacity / sampleRate` **shrinks below 2.0s**. Checked
concretely:

| sampleRate | ceil(2.0*sr) | capacity (clamped) | capacitySeconds |
|---|---|---|---|
| 44100 | 88200 | 88200 | 2.000s |
| 48000 | 96000 | 96000 | 2.000s |
| 88200 | 176400 | 96000 | 1.088s |
| 96000 | 192000 | 96000 | 1.000s |
| 192000 | 384000 | 96000 | 0.500s |

Meanwhile `baseSeconds = ExpMapCompute(0.001f, kMaxDelaySeconds, p.dtim)`
(`Process()`'s first line) has a range fixed at `[0.001, 2.0]` **regardless
of sample rate** -- it is a function of `p.dtim` and two literal
constants only, with no `sampleRate` term anywhere in it. So at any
sample rate above 48kHz, `baseSeconds` alone (Delay time turned up, Stereo
width all the way OFF, Mod rate at its default 0, Width balance at any
value) can and does exceed `capacitySeconds` -- and **nothing in
`Process()` bounds `baseSeconds` itself**. The capacity guard this change
adds (`maxSpreadSeconds`/`widthSpread`) only ever shrinks the *spread*
term; `timeL = std::max(0.001f, baseSeconds + modSeconds)` carries no clamp
of its own, the same gap pass 1's Attack 1 named for the `modSeconds` term
-- reached here through an even shorter path, since it needs neither
`dmod` nor `dwid` to move off zero at all.

**None of the four shipped checks can see this**, for two independent
reasons stacking on top of each other:
1. All three `Process()`-calling checks hardcode `sampleRate = 48000.0f`,
   the one rate where `capacitySeconds` happens to equal `kMaxDelaySeconds`
   exactly, so this gap has zero surface at that specific rate.
2. The fourth check (`..._bound_holds_across_the_reachable_grid`) also
   hardcodes `sr = 48000.0f`, and -- per pass 2's Attack 6, confirmed still
   true by inspection here -- never calls `Process()` at all, so even if it
   swept `sr` it would still only be re-checking its own copy of the
   formula, not the real code path.

**Probe** (temporary `TEST_CASE`, unmodified production `Delay.hpp`,
appended after the shipped grid-sweep test, reusing the shipped
`MeasureReadAtLagSamples` helper unchanged): `SetSampleRate(96000.0f)`,
`dtim = 0.99f` (near max Delay time), **`dwid = 0.0f`** (Stereo width fully
off -- the width mechanism this change's own capacity guard exists to
protect plays no part), `dmod` at its `DelayParams` default of `0.0f`.
Warms the line fully at that same `p`, fires a single impulse, and
measures where it reappears via the same lag-detection technique the
shipped tests already use, then compares the measured lag against the
knob's own unclamped request (`baseSeconds * sr`) with a `REQUIRE_NEAR`
tolerance of one sample -- not merely "does it stay under capacity", the
same standard pass 1's non-finite-liveness finding argued for (a bound
must assert what it claims, not something weaker that happens to also
pass).

Literal output:
```
[adversarial samplerate] requested capacity=192000 clampedCapacity(<=kMaxDelaySamples)=96000
[adversarial samplerate] dtim=0.99 dwid=0.0 (width OFF) baseSeconds=1.85361576 expectedLagSamples=177947.113 measuredLagSamples=81947 peakAbs=0.791067362
[FAIL] adversarial_probe_samplerate_above_48k_overruns_capacity_with_width_off: .../FroggersDspParityTests.cpp:10644 requirement failed: static_cast<float>(lagSamples) (81947) ~= static_cast<float>(expectedLagSamples) (177947), eps=1
```

The knob asked for ~177947 samples (~1.85s at 96kHz) of delay; the line
actually returned the impulse at **81947 samples (~0.85s)** -- exactly
`177947 - 96000 = 81947`, a clean one-wrap-around-the-96000-sample-buffer
error, silently. `peakAbs=0.79` confirms a real, non-degenerate peak, not
a dead measurement.

With this same probe present, the four shipped checks were run in the same
binary and all four still report green:
```
[PASS] stereo_delay_cross_feed_removal_decorrelates_the_feedback_pair_across_width
[PASS] stereo_delay_width_spread_never_reads_past_the_line_capacity
[PASS] stereo_delay_width_spread_bound_is_inert_away_from_capacity
[PASS] stereo_delay_width_spread_bound_holds_across_the_reachable_grid
```

**Verdict:** GOT THROUGH. Real, currently-present coverage gap, broader
than either of the first two passes' findings in one specific sense: it
needs neither `dmod` (pass 1's route) nor `widthBalance < 1.0` (pass 2's
route) nor even `dwid` away from its zero default -- Delay time alone, at
any host sample rate above 48kHz, overruns the line with Stereo width
completely inert. It is also the sibling of pass 1's Attack 1
(`modSeconds` unguarded) and pass 2's Attack 5
(`widthBalance`-gated guard) in the same family: all three are different
knobs reaching the same unguarded `baseSeconds`/`timeL` term, none of
which the capacity fix in this diff touches -- the diff only ever clamps
`widthSpread`.

**Blocks delivery** -- a third, independent way to reach the exact overrun
class this change's own stated purpose is to close, on a parameter (host
sample rate) that is not even a knob a user turns -- it is set once from
the audio device and is completely out of the player's control, so any
interface running above 48kHz (88.2/96/176.4/192kHz are all standard) hits
this on ordinary Delay-time settings with Stereo width left at its own
default of off. Does not block execution of unrelated work. Fix shape:
either `baseSeconds`/`timeL` need their own capacity clamp independent of
`widthSpread`'s (the same fix shape pass 1 already proposed for
`modSeconds`, since both terms share the same missing bound), or
`ExpMapCompute`'s own delay-time range needs to be derived from
`capacitySeconds` rather than the fixed `kMaxDelaySeconds` literal. Either
way, at least one shipped check needs to construct `StereoDelay` at a
sample rate other than 48000 and call the real `Process()` there --
currently none does.

---

## Summary table (this pass)

| # | Attack class | Result | Blocks |
|---|---|---|---|
| 7 | Capacity guard's geometry assumes `sampleRate == 48000`; any higher host sample rate lets Delay time alone (Stereo width fully OFF) overrun the line, and all four shipped checks hardcode 48000 so none can see it | **GOT THROUGH** (real, present-in-current-tree, reachable with no user-facing knob other than ordinary Delay time; confirmed with a real overrun via the public API) | delivery of this change |

Relationship to the first two passes' attacks: Attack 7 here is a third
sibling of pass 1's Attack 1 (`modSeconds` unguarded) and pass 2's Attack 5
(`widthBalance`-gated guard) -- all three reach the same underlying hole
(`baseSeconds`/`timeL` has no capacity clamp of its own; only
`widthSpread` does) through three different, independently-reachable
inputs the four shipped checks never move off one pinned value: `dmod`,
`widthBalance`, and now sample rate itself. No prior pass named sample
rate as a pinned quantity, and this is the only one of the three that
needs neither `dwid` nor `dmod` away from their own defaults to reach the
overrun.

---

`git status --short` and `md5` of both files, confirming no residual diff
beyond what was already present (`M`) before this audit started:

```
$ md5 app/dsp/Delay.hpp app/FroggersDspParityTests.cpp
MD5 (app/dsp/Delay.hpp) = 2090d9902bae29e67d7eb57a27991cef
MD5 (app/FroggersDspParityTests.cpp) = 35b80ce13d6a4ffebae993663eabcddf
```
(matches the baseline both prior passes recorded, confirmed identical
before and after this pass's single mutation)

```
$ git status --short
 M app/FroggersDspParityTests.cpp
 M app/dsp/Delay.hpp
D  openspec/changes/frogg3rs-delay-width-wysiwyg-repair/... (12 files, pre-existing, not touched by this audit)
?? openspec/changes/  (untracked directories belonging to other sessions, not touched by this audit)
```
