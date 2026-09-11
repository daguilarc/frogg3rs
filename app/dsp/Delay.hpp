#pragma once

// synth_froggers::dsp::{DelayParams, StereoDelay, MapRowsToDelayParams} --
// a **copy** of the cited Froggers formulas -- read directly from
// the frozen source before porting, not from memory.
//
// CORRECTED SCOPE: an
// earlier draft claimed 8 of 9 Delay-page params had no frozen original
// because it searched only src/core/. A full original exists in
// the retired simulator's DelayState.hpp + StereoDelay.hpp (absent from src/core/ because
// the delay is a feature only the retired simulator and desktop app had -- the Daisy firmware never shipped it).
// All nine params are ported here; nothing on this page is newly authored.
//
// Ported from:
//   - the retired simulator's StereoDelay.hpp (whole file) -- DelayParams, WetPair (renamed
//     DelayWetPair to avoid any collision), and StereoDelay::{setSampleRate,
//     clearBuffers, process, toReverbMono, readAt, writeSample, wrapIndex,
//     advanceWrite}, verbatim.
//   - the retired simulator's DelayState.hpp:165-198 (processInsert) -- specifically the
//     row -> DelayParams mapping at :180-186 (dtim/dsnd/dfbk/dwid/ddet/
//     dmod/dmix <- rows 0-6) and the Color/Halo folding at :187-193
//     (`params.ddet = clamp(0.5*(ddet+color))`,
//     `params.dmod = clamp(0.5*(dmod+halo))`, folded under
//     `m_useV2Layout` -- this app WAS the V2 layout, so the fold applied
//     unconditionally at the time of this port), plus the final
//     `delay.process(bumpIn, params)` /
//     `delay.toReverbMono(bumpIn, wet, params.dmix)` call shape at :196-198.
//     REMOVED since: the fold no longer exists anywhere in this
//     file -- Detune/Color/Halo (rows 4/7/8) are retired/renamed to
//     Freeze/Reverse blend/Diffusion (FroggersParameters.hpp) and every row
//     maps to its own DelayParams field. See MapRowsToDelayParams below for
//     the current, fold-free mapping.
//
// NOT ported (deliberately): the rest
// of DelayState -- knob smoothing (RuntimeParam `smoothed[]`), mod-source /
// mod-depth routing (`modSource`, `modDepth`, `blendKnob`), the fuego
// cascade (`blendRow`/`Fuegoize`/`V2FuegoStack` -- Fuegoize.hpp already
// ported Fuegoize itself as its own standalone unit; the seam that applies
// it is out of scope here), crispy-row handling, and randomize/sanitize
// machinery (`randomizeKnobs`, `randomizeMod`, `clearModRoutesForIndex`,
// `sanitizeModSources`). Sheaf's parameter model and the app-tier fuego-
// application seam already own all of that -- this port is the AUDIO path
// only: the row ->
// DelayParams mapping and StereoDelay itself (the Color/Halo fold this
// paragraph originally described was removed -- see the
// "REMOVED since" note above).

#include "DspMath.hpp"
#include "Drive.hpp"    // reuse dsp::SampleRateReducer AS-IS for the Crush knob (see StereoDelay::SetCrush below).
#include "FilterFx.hpp"
#include "Limiter.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace synth_froggers::dsp {

// Answers the question "why can't we just have limiters for reverb
// and delay?"
// Tuning for `StereoDelay::wetLimiterL`/`wetLimiterR`
// below, a per-channel pair of `dsp::OutputLimiter` instances (dsp/
// Limiter.hpp) inserted on the WET tap (`dL`/`dR`), strictly AFTER the
// feedback loop's own `WriteSample` calls -- so the loop keeps writing the
// unlimited value (the in-loop `PadeSaturator::Saturate` bound,
// `|inSignal| + fbk`, is completely untouched) and only what ESCAPES this
// stage toward Reverb/the master limiter gets bounded further.
//
// Chosen BY MEASUREMENT (scratch harness driving this exact struct with
// dtim swept from 0.0 to 1.0, dfbk pinned to 1.0 -> fbk clamps to 0.98,
// dsnd=1.0, dwid=0.3, sustained full-scale input, 20000 samples per point --
// not by analogy to the master limiter: analogy-picked constants have been
// measured wrong before in this codebase):
//
//   - Starting point was the master's own tuning (threshold 0.9, attack
//     1ms, release 100ms). It is NOT sufficient: at the shortest reachable
//     delay time (dtim=0 -> ~48-sample round trip at 48kHz), the raw wet
//     tap rises from 0 to its ~1.96 per-sample bound (`inputAmplitude +
//     fbk`) within about 100 samples (~2ms) -- far faster than a 1ms
//     attack's own time constant can track -- and the 1ms-attack limiter's
//     own transient overshoot measured 1.673412, comfortably past the 1.0
//     ceiling. This is the delay's own version of the peak branch's own
//     finding (FilterFx.hpp):
//     "sustained material" is true of the STEADY STATE but the ONSET at
//     minimum delay time is a fast transient, not a slow swell, so a slow
//     attack under-reacts to it exactly like a slow attack under-reacted
//     to the peak branch's own per-sample-random height steps.
//   - kDelayWetLimiterThreshold (0.9): kept AT the master's own value (not
//     lowered) -- measurement (below) shows the attack alone, not the
//     threshold, is what needs to move; this stage still "catches first"
//     because it sits upstream of Reverb and the master in the signal
//     path (FroggersAppCore.hpp's `delay_.Process` runs before
//     `reverb_.Process` and before `SanitizeOutputSample`), so by the time
//     the master ever sees this signal it has already been reduced here.
//   - kDelayWetLimiterAttackSeconds (2 microseconds): sweeping attack alone
//     (threshold 0.9, release 100ms fixed) across dtim in {0, 0.02, 0.05,
//     0.1, 0.3, 1.0} found the worst-case (always at dtim=0, the fast-
//     round-trip extreme) crosses under the 1.0 ceiling between 2.5us
//     (1.000113) and 2.0us (0.999999); 2 microseconds sits just past that
//     crossing with a working margin, and going faster still (1.5us:
//     0.999998, 1.0us: 0.999998) buys nothing further -- the curve has
//     already flattened at the DesiredMagnitude asymptote for this bound,
//     so 2us is the measured knee, not an arbitrary small number.
//   - kDelayWetLimiterReleaseSeconds (100ms, matches the master): the
//     worst-case transient overshoot above is entirely an ATTACK-side
//     effect (a fast-rising raw signal outrunning the envelope's fall);
//     release governs recovery afterward, not this peak, and 100ms is the
//     same "gain reduction that does not pump" value this codebase has
//     already accepted for that job (dsp::OutputLimiter::kDefaultReleaseSeconds,
//     dsp::kPeakLimiterReleaseSeconds) -- reused rather than a fresh number
//     invented where the measurement gave no reason to move it.
// Retargeted from 0.9 to 0.72, preserving the ORIGINAL
// threshold/ceiling ratio (0.9/1.0 == 0.72/0.80) rather than picking a
// round number, so this stage's measured knee (attack/release above) keeps
// its character instead of being re-tuned by accident. 0.9 was strictly
// below kSharedCeiling (1.0) but is ABOVE the retargeted kStageCeiling
// (0.80) -- the negative-headroom exponential-amplifier trap the
// static_assert below exists to catch -- so it comes down in this same edit.
inline constexpr float kDelayWetLimiterThreshold = 0.72f;
inline constexpr float kDelayWetLimiterCeiling = kStageCeiling;
// See dsp::OutputLimiter::kDefaultThreshold's own static_assert
// (dsp/Limiter.hpp).
static_assert(kDelayWetLimiterThreshold < kDelayWetLimiterCeiling,
              "threshold must stay strictly below ceiling; a negative headroom turns "
              "DesiredMagnitude into an exponential amplifier -- see dsp/Limiter.hpp");
inline constexpr float kDelayWetLimiterAttackSeconds = 2.0e-6f;   // 2 microseconds -- see comment above.
inline constexpr float kDelayWetLimiterReleaseSeconds = kSharedReleaseSeconds;  // shared; see Limiter.hpp.

// The wet-level follower `StereoDelay::wetAuthority` reads (decides how much
// of the dry signal Wet/dry is allowed to remove) is `dsp::WetAuthorityFollower`
// (Drive.hpp), shared with `dsp::Reverb::wetAuthority` -- one definition for
// both stages' attack/release coefficients and full-authority level, so
// Reverb's own Send shares this exact protection rather than a second copy
// that happens to agree. See that struct's own header comment (Drive.hpp)
// for the tuning
// derivation.

// the retired simulator's StereoDelay.hpp:10-19, plus three fields added later that
// are not present in that frozen source -- see each field's own comment.
struct DelayParams
{
    float dtim = 0.0f;
    float dsnd = 0.0f;
    float dfbk = 0.0f;
    float dwid = 0.0f;
    float dfrz = 0.0f;  // Freeze (slot 5, was Detune/ddet). Crossfades
                        // new input and feedback loop gain toward a hold -- see StereoDelay::Process() below.
    bool dfrzLatched = false;  // Freeze override, meant to be set by the transport-button latch --
                                // NOT part of the row -> DelayParams mapping below. Inert as a SOURCE for
                                // now (nothing sets it yet; a future change wires the transport button to it), but
                                // honoured as a SINK the moment it is true -- see StereoDelay::Process() below.
    float dmod = 0.0f;
    float dmix = 0.0f;
    float drev = 0.0f;  // Reverse blend (slot 7, was Color). Drives
                        // StereoDelay's per-channel DelayReverser on the wet tap -- see StereoDelay::Process() below.
    float ddif = 0.0f;  // Diffusion (slot 8, was Halo). Drives StereoDelay's
                        // per-channel DelayDiffuser on the wet tap -- see StereoDelay::Process() below.
};

// the retired simulator's StereoDelay.hpp:21-25 (WetPair). The same shape the
// rest of the chain now carries, so it is that type under its own name rather
// than a second declaration of it.
using DelayWetPair = StereoSample;

// (Diffusion, Delay slot 8, this file's
// DelayParams::ddif comment): one Schroeder allpass section with an
// M-sample delay, M configured at runtime (up to `Capacity` samples, fixed
// at compile time so the audio path never allocates).
//
// Recurrence and coefficient-sign convention are the SAME as
// DriveBlendPhase's one-sample allpass (Drive.hpp:
// `phased = -a*wet + allpassX1 + a*allpassY1; allpassX1 = wet;
// allpassY1 = phased;`), generalized from one-sample registers
// (allpassX1/allpassY1) to M-sample circular buffers (xHistory/yHistory
// below) -- reuse the RECURRENCE FORM, not the one-sample STATE: a
// straight copy of DriveBlendPhase's one-
// sample memory produces frequency-dependent phase rotation (a phaser),
// not time smearing, so diffusion needs real per-section delay instead.
template <std::size_t Capacity>
struct SchroederAllpassSection
{
    // xHistory[pos]/yHistory[pos] hold x[n-M]/y[n-M] the instant Process()
    // reads them (read-before-write, the same ring-buffer idiom
    // StereoDelay's own ReadAt/WriteSample pair uses on lineL/lineR below,
    // just backed by a fixed-capacity array instead of a heap vector).
    std::array<float, Capacity> xHistory{};
    std::array<float, Capacity> yHistory{};
    std::size_t pos = 0;
    std::size_t m = 1;  // current delay length in samples; set by Configure(), clamped to [1, Capacity].

    // Recomputes M from a base delay time (seconds) and a sample rate --
    // called only from DelayDiffuser::SetSampleRate (never per-sample), so
    // the rounding/clamp here are not audio-path cost. Also clears this
    // section's history: a rate change invalidates old buffer alignment
    // either way, matching StereoDelay::SetSampleRate's own lineL/lineR
    // re-assignment just below.
    void Configure(float sampleRateHz, float baseSeconds)
    {
        long long rounded = std::lround(static_cast<double>(baseSeconds) * static_cast<double>(sampleRateHz));
        if (rounded < 1)
        {
            rounded = 1;
        }
        if (rounded > static_cast<long long>(Capacity))
        {
            // Defensive only -- this unit is sized for sample rates up to
            // 192kHz (see DelayDiffuser's own capacity comments below);
            // clamping here keeps every buffer access in-bounds even if a
            // caller supplies more, rather than relying on that ceiling
            // never being crossed.
            rounded = static_cast<long long>(Capacity);
        }
        m = static_cast<std::size_t>(rounded);
        Reset();
    }

    // y[n] = -a*x[n] + x[n-M] + a*y[n-M] -- Drive.hpp's recurrence,
    // M-sample memory instead of one-sample (class header comment above).
    float Process(float x, float a)
    {
        const float xDelayed = xHistory[pos];
        const float yDelayed = yHistory[pos];
        const float y = -a * x + xDelayed + a * yDelayed;
        xHistory[pos] = x;
        yHistory[pos] = y;
        pos = (pos + 1 >= m) ? 0 : pos + 1;
        return y;
    }

    void Reset()
    {
        std::fill(xHistory.begin(), xHistory.end(), 0.0f);
        std::fill(yHistory.begin(), yHistory.end(), 0.0f);
        pos = 0;
    }

    bool StateFinite() const
    {
        for (const float v : xHistory)
        {
            if (!std::isfinite(v))
            {
                return false;
            }
        }
        for (const float v : yHistory)
        {
            if (!std::isfinite(v))
            {
                return false;
            }
        }
        return true;
    }

    float StateMagnitude() const
    {
        float magnitude = 0.0f;
        for (const float v : xHistory)
        {
            magnitude = std::max(magnitude, std::fabs(v));
        }
        for (const float v : yHistory)
        {
            magnitude = std::max(magnitude, std::fabs(v));
        }
        return magnitude;
    }
};

// One channel's diffuser -- three SchroederAllpassSection cascades
// at non-harmonic base times (4.7ms/12.3ms/21.1ms) so the sections do not
// reinforce one another. One instance per
// channel (StereoDelay::diffuserL/diffuserR below), matching this file's
// existing per-channel-instance idiom (wetLimiterL/R, fbToneL/R, crushL/R)
// rather than one shared instance driven by e.g. max(|L|,|R|).
struct DelayDiffuser
{
    static constexpr float kSection1BaseSeconds = 0.0047f;  // 4.7 ms.
    static constexpr float kSection2BaseSeconds = 0.0123f;  // 12.3 ms.
    static constexpr float kSection3BaseSeconds = 0.0211f;  // 21.1 ms.

    // Capacities sized to each section's OWN 192kHz maximum, not all to the
    // longest section's.
    static constexpr std::size_t kSection1Capacity = 1024;  // 4.7ms@192kHz = 902.4 samples; headroom to a round cap.
    static constexpr std::size_t kSection2Capacity = 2560;  // 12.3ms@192kHz = 2361.6 samples; own maximum, not 4096.
    static constexpr std::size_t kSection3Capacity =
        4096;  // 21.1ms@192kHz ~= 4051.2 samples; a safe cap for the longest section.

    SchroederAllpassSection<kSection1Capacity> section1;
    SchroederAllpassSection<kSection2Capacity> section2;
    SchroederAllpassSection<kSection3Capacity> section3;

    void SetSampleRate(float sampleRateHz)
    {
        section1.Configure(sampleRateHz, kSection1BaseSeconds);
        section2.Configure(sampleRateHz, kSection2BaseSeconds);
        section3.Configure(sampleRateHz, kSection3BaseSeconds);
    }

    // Cascaded: section1's output feeds section2's input feeds section3's
    // input, all three driven by the SAME coefficient `a` (StereoDelay's
    // own ddif*kDiffusionCoeffScale mapping computes `a` once per sample --
    // see StereoDelay::ApplyDiffusion below).
    float Process(float x, float a)
    {
        return section3.Process(section2.Process(section1.Process(x, a), a), a);
    }

    void Reset()
    {
        section1.Reset();
        section2.Reset();
        section3.Reset();
    }

    bool StateFinite() const
    {
        return section1.StateFinite() && section2.StateFinite() && section3.StateFinite();
    }

    float StateMagnitude() const
    {
        return std::max(section1.StateMagnitude(), std::max(section2.StateMagnitude(), section3.StateMagnitude()));
    }
};

// (Reverse Blend, Delay slot 7, this
// file's DelayParams::drev comment): per-channel backward-travelling read
// pointer into the SAME delay line the forward tap reads, plus the
// crossfade state that declicks its wrap. Shape follows DelayDiffuser
// above (DelayDiffuser's own precedent for a per-channel helper struct) --
// Reset()/StateFinite()/StateMagnitude(), one instance per channel
// (StereoDelay::reverserL/reverserR below) -- rather than inventing a
// different shape for the same kind of per-channel unit.
//
// UNLIKE DelayDiffuser, this struct's own state is buffer POSITIONS, not
// retained audio samples -- there is no separate history array to scan,
// because the reverse tap reads straight out of StereoDelay's existing
// lineL/lineR (already covered by StereoDelay::StateFinite()'s/
// StateMagnitude()'s own lineL/lineR scans). StateMagnitude() below is
// therefore a liveness signal for this struct's OWN direct use by tests
// (mirrors how the diffusion test calls diffuserL.StateMagnitude()
// directly, not through StereoDelay::StateMagnitude()'s aggregate) --
// see StereoDelay::StateMagnitude()'s own comment for why it is
// deliberately NOT folded into that aggregate.
struct DelayReverser
{
    // Absolute buffer position (SAME coordinate space as StereoDelay's own
    // `writePos`), decremented by exactly one sample every call
    // (StereoDelay::ApplyReverse) -- this is what makes the read travel
    // BACKWARD through the buffer, rather than track `writePos` the way
    // the forward tap's fixed-seconds ReadAt call does (that tap's read
    // position also moves forward, but only as a SIDE EFFECT of writePos
    // itself advancing under a roughly-constant delay time -- nothing
    // decrements it directly). Fed to ReadAt as a `seconds`-behind-the-
    // CURRENT-writePos value recomputed fresh every call
    // (`(writePos - pos) / sampleRate`); the algebra of that
    // recomputation cancels writePos's own value out exactly, so ReadAt
    // always ends up reading at buffer position `pos` (mod capacity)
    // regardless of where writePos currently sits, including across
    // writePos's own circular wraparound -- see ApplyReverse's own
    // comment for the full derivation.
    float pos = 0.0f;
    // Plain elapsed-sample counter since `pos` was last anchored to "now"
    // (i.e. since the current backward sweep started), incremented by
    // exactly one every call. Kept as its OWN counter rather than derived
    // from `pos` and the current `writePos` so it stays correct across
    // writePos's circular wraparound without a second modulo -- it exists
    // purely to time the wrap crossfade below (the sweep covers one
    // delay-time of history, then wraps).
    float elapsed = 0.0f;
    // Wrap-declick crossfade state ("not optional garnish... what makes
    // the control shippable"). `fading` is true only while a freshly-armed sweep
    // (fadePos) is being blended in against the outgoing one (pos); both
    // `pos` and `fadePos` advance every call while fading -- same
    // unconditional-advance rule as `pos`/`elapsed` above.
    float fadePos = 0.0f;
    float fadeGain = 0.0f;
    bool fading = false;

    void Reset()
    {
        pos = 0.0f;
        elapsed = 0.0f;
        fadePos = 0.0f;
        fadeGain = 0.0f;
        fading = false;
    }

    bool StateFinite() const
    {
        return std::isfinite(pos) && std::isfinite(elapsed) && std::isfinite(fadePos) && std::isfinite(fadeGain);
    }

    // Liveness signal for this struct's own direct use by tests (see this
    // struct's header comment) -- `elapsed` is the simplest field
    // guaranteed to grow away from Reset()'s 0.0f the moment ApplyReverse
    // is called at all, so it alone is sufficient to prove the pointer is
    // being advanced (the same kind of check
    // stereo_delay_diffusion_state_tracks_the_signal_while_ddif_is_zero
    // does for Diffusion).
    float StateMagnitude() const { return std::fabs(elapsed); }
};

// the retired simulator's StereoDelay.hpp:27-157 (StereoDelay), verbatim.
struct StereoDelay
{
    static constexpr float kMaxDelaySeconds = 2.0f;
    static constexpr size_t kMaxDelaySamples = 96000;

    // Per-channel wet-output limiters (see this file's header comment,
    // above the struct, for the tuning derivation). Per-channel, not one
    // instance driven by max(|dL|,|dR|): the codebase's own established
    // idiom for a two-instance choice is per-unit/per-channel independence
    // (mirrors `outputLimiter_`/`filterChain_.peakLimiter` being separate,
    // independently-configured instances rather than a single shared one,
    // dsp/Limiter.hpp's own header comment on why one shared-tuning instance
    // was wrong for two different jobs). Concretely for THIS stage: `dwid`'s
    // cross-feed (`fbL = dL*(1-cross) + dR*cross`) already keeps L and R
    // close to each other in practice, so the two channels rarely diverge
    // enough for one limiter engaging alone to read as an image shift; a
    // linked (max-driven) limiter would instead force BOTH channels to duck
    // whenever EITHER one alone crossed threshold, which is exactly the
    // whole-mix-ducking failure mode this stage exists to eliminate, just
    // narrowed from "the whole mix" to "the whole delay stage" -- a smaller
    // version of the same mistake. Per-channel keeps each channel's own
    // reduction tied to its own excess only, matching how every other
    // per-stage limiter in this codebase (`peakLimiter`, `outputLimiter_`)
    // is scoped to exactly the signal it bounds and nothing wider.
    OutputLimiter wetLimiterL;
    OutputLimiter wetLimiterR;
    // One follower for both channels: it tracks `monoWet`, the value
    // ToReverbMono already forms, so the measurement costs one fabs, one
    // compare, one multiply-add and one float of state per sample rather than
    // a pair of followers on a value nothing else needs. `WetAuthorityFollower`
    // (Limiter.hpp) is the same unit `dsp::Reverb::wetAuthority` owns.
    WetAuthorityFollower wetAuthority;

    // (Delay slot 9, "Feedback drive" / "FbDr"):
    // pre-gain on the ARGUMENT of Saturate only, see Process() below.
    // Default 1.0f (unity) -- matches today's un-driven feedback exactly
    // for any instance that never calls SetFeedbackDrive.
    float fbDrive = 1.0f;

    // (Delay slot 10, "Feedback tone" / "FbTn"): per-channel damping
    // ahead of Saturate. Default alpha 1.0f (bypass, exact identity -- see
    // FrogBlock::SetTone's own comment, Drive.hpp, for why alpha==1.0f is
    // an exact bypass rather than an approximation).
    OnePoleLowPass fbToneL{1.0f};
    OnePoleLowPass fbToneR{1.0f};

    // (Delay slot 12, "Width balance" / "WBal"): default 1.0f reproduces
    // today's 0.35f/0.5f literals exactly (see Process() below and
    // SetWidthBalance).
    float widthBalance = 1.0f;

    // (Delay slot 13, "Crush" / "Crsh"): feedback-tap crush, reused
    // dsp::SampleRateReducer AS-IS (Drive.hpp) rather than a hand-rolled
    // copy. Default freq 1.0f -- SampleRateReducer::Process's own
    // `freq >= 1.0f` branch returns input unchanged, an exact bypass ("no
    // crushing") for any instance that never calls SetCrush.
    SampleRateReducer crushL{/*freq=*/1.0f};
    SampleRateReducer crushR{/*freq=*/1.0f};

    // (Delay slot 8, "Diffusion"):
    // per-channel three-section Schroeder allpass cascade applied to the
    // wet tap (see Process() below, applied to dL/dR strictly BEFORE
    // wetLimiterL/R -- once per repeat, not once per round trip; NOT
    // inside the feedback loop, a deliberate placement choice -- see
    // Process()'s own comment at that call site).
    DelayDiffuser diffuserL;
    DelayDiffuser diffuserR;

    // Coefficient requirement: a = ddif * kDiffusionCoeffScale.
    // The bound is enforced BY THIS MAPPING -- 0.7 sits comfortably inside
    // DriveBlendPhase's own 0.98 margin (Drive.hpp:
    // `0.98f * (2.0f*phaseKnob01 - 1.0f)` -> the open interval (-0.98,
    // 0.98), this codebase's established allpass stability margin), so for
    // any ddif in the knob's own [0,1] range, |a| <= 0.7 stays strictly
    // inside the unit circle by construction, not by an unenforced comment.
    static constexpr float kDiffusionCoeffScale = 0.7f;

    // (Delay slot 7, "Reverse blend"):
    // per-channel backward read pointer applied to the forward taps dL/dR
    // (see Process() below, applied immediately after the forward reads
    // and BEFORE the feedback cross-mix/write -- a deliberate placement
    // choice, see that call site's own comment).
    DelayReverser reverserL;
    DelayReverser reverserR;

    // The wrap-declick crossfade
    // duration. "A few ms, sample-rate scaled" -- 5ms sits in that range and
    // is clamped to at
    // most half the current reverse window in ApplyReverse below, so it
    // never exceeds the sweep it is meant to smooth even at the shortest
    // reachable delay time (baseSeconds floors at 0.001s, i.e. 48 samples
    // at 48kHz -- shorter than one un-clamped 5ms fade).
    static constexpr float kReverseWrapCrossfadeSeconds = 0.005f;

    // The feedback the Freeze transport LATCH applies, overriding the
    // Freeze encoder's own unity ceiling so the latch reaches a state the
    // knob cannot. Strictly above 1.0, so
    // the loop amplifies rather than merely recirculating losslessly.
    //
    // BY-EAR CONSTANT, NOT DERIVED. Nothing measures or justifies 1.05 --
    // it is a starting value, meant to be tuned by ear, recorded as such so
    // no later reader mistakes it for a computed bound the way this file's
    // own history warns about. Its only hard requirement is `> 1.0f`.
    static constexpr float kFreezeLatchOverdrive = 1.05f;

    // Freeze's whole mapping, in one place, so Process() below and
    // the regression tests exercise the SAME arithmetic rather than a test
    // re-deriving it (which would assert the formula against itself and
    // prove nothing). Static and pure -- it reads no member state, and
    // deliberately takes no fbDrive: see Process()'s own comment for why
    // Feedback Drive must not appear in this mapping.
    //
    // Properties this function is required to have, each pinned by a test:
    //   - monotonically non-decreasing in `freeze` over [0,1], at any `fbk`
    //   - `FreezeFeedback(fbk, 0.0f, false) == fbk` bit-exactly
    //   - ceiling of exactly 1.0f at freeze==1 (lossless, no self-gain)
    //   - latched value strictly greater than any unlatched value
    static float FreezeFeedback(float fbk, float freeze, bool latched)
    {
        if (latched) {
            return kFreezeLatchOverdrive;
        }
        return fbk + (1.0f - fbk) * freeze;
    }

    // :33-40 (clearBuffers).
    void ClearBuffers()
    {
        std::fill(lineL.begin(), lineL.end(), 0.0f);
        std::fill(lineR.begin(), lineR.end(), 0.0f);
        writePos = 0;
        lfoPhase = 0.0f;
        lastWet = {};
        // Cleared with the line it measures: a recovered unit whose follower
        // still held the level of the audio that faulted would grant Wet/dry
        // authority over a path that is now empty. Defaults to 0.0f (a
        // just-cleared wet path has earned nothing) -- see
        // WetAuthorityFollower::Reset's own comment (Limiter.hpp) for why that
        // default differs for `dsp::Reverb`.
        wetAuthority.Reset();
        // The wet limiters carry their own per-sample `envelope` state
        // (dsp::OutputLimiter::Process), so a buffer clear -- whether the
        // Stop-transport reset or Tier 1 fault recovery via Reset() below --
        // must reset them too, the same way it resets `lastWet` above. Not
        // load-bearing for post-Stop silence (a reduced-gain envelope only
        // ever multiplies toward zero, never away from it, and the lines
        // are already zeroed), but keeps this unit in a single deterministic
        // state after a clear, matching `lfoPhase`'s own reset rationale.
        wetLimiterL.Reset();
        wetLimiterR.Reset();
        // Same "must reset them too" rationale as the wet limiters
        // just above -- the feedback-tap tone filter and crush stage are
        // new recursive state sitting inside the loop this clear is meant
        // to silence.
        fbToneL.output = 0.0f;
        fbToneR.output = 0.0f;
        crushL.Reset();
        crushR.Reset();
        // Same "must reset them too" rule this function's own
        // header comment states -- the diffuser's three sections per
        // channel are new recursive state (each section's M-sample
        // xHistory/yHistory) sitting on the wet tap this clear is meant to
        // silence.
        diffuserL.Reset();
        diffuserR.Reset();
        // Same "must reset them too" rule this function's own
        // header comment states -- the reverse pointer's position, elapsed
        // counter and wrap-crossfade state are new recursive state sitting
        // on the wet tap this clear is meant to silence.
        reverserL.Reset();
        reverserR.Reset();
    }

    // (Tier 1 recovery, app/FroggersAppCore.hpp): a thin name
    // alias, not a new implementation -- delegates straight to the existing
    // ClearBuffers() above so RecoverIfNonFinite<Unit>() (FroggersAppCore.hpp)
    // can call a uniform Reset() across every recoverable unit without a
    // special case for this one's differently-named method.
    void Reset() { ClearBuffers(); }

    // dsp::StereoDelay::Process() has an early-return guard
    // (`p.dsnd <= 0.0001f`) that most patches take, since Send defaults to
    // 0 -- but once Send is nonzero this unit is just as exposed to
    // upstream-fault cascades as dsp::Reverb (ReadAt/WriteSample have no
    // finiteness guard of their own), so it gets the same Tier 1 coverage.
    bool StateFinite() const
    {
        if (!std::isfinite(lfoPhase) || !std::isfinite(lastWet.l) || !std::isfinite(lastWet.r) ||
            !wetAuthority.StateFinite())
        {
            return false;
        }
        // The wet limiters' own `envelope` state participates in this
        // unit's aggregate finiteness the same way every other member here
        // does -- `RecoverIfNonFinite(delay_)` (FroggersAppCore.hpp) calls
        // this StateFinite()/the Reset() above uniformly across the whole
        // unit, so a poisoned limiter envelope must be visible here rather
        // than needing its own separate top-level recovery call.
        if (!wetLimiterL.StateFinite() || !wetLimiterR.StateFinite())
        {
            return false;
        }
        // Same "aggregate finiteness" rule the wetLimiter check
        // above already follows -- the diffuser's own history buffers must
        // be visible here too.
        if (!diffuserL.StateFinite() || !diffuserR.StateFinite())
        {
            return false;
        }
        // Same "aggregate finiteness" rule the diffuser check just
        // above already follows -- the reverse pointer's own position/
        // elapsed/crossfade state must be visible here too.
        if (!reverserL.StateFinite() || !reverserR.StateFinite())
        {
            return false;
        }
        for (const float sample : lineL)
        {
            if (!std::isfinite(sample))
            {
                return false;
            }
        }
        for (const float sample : lineR)
        {
            if (!std::isfinite(sample))
            {
                return false;
            }
        }
        return true;
    }

    // Read-only diagnostic,
    // NOT wired into RecoverPoisonedUnitState's Tier 2 (this unit stays
    // Tier-1-only, per the comment above StateFinite() -- a BIBO-stable
    // feedback loop can legitimately settle to a large-but-finite steady
    // state under sustained loud input, so the fault-recovery ceiling used
    // for the other ten units would misfire here). Exists so the Stop-flush
    // measurement harness can tell "cleared once and stayed clear" apart
    // from "cleared once, then refilled by a still-ringing upstream unit"
    // -- mirrors dsp::Comb::StateMagnitude()'s own shape (scan the
    // recirculating line, the unit's actual memory).
    float StateMagnitude() const
    {
        float magnitude = std::max(std::fabs(lastWet.l), std::fabs(lastWet.r));
        for (const float sample : lineL)
        {
            magnitude = std::max(magnitude, std::fabs(sample));
        }
        for (const float sample : lineR)
        {
            magnitude = std::max(magnitude, std::fabs(sample));
        }
        // Fold the diffuser's own history into this unit's
        // aggregate magnitude, same idiom as lineL/lineR just above.
        magnitude = std::max(magnitude, std::max(diffuserL.StateMagnitude(), diffuserR.StateMagnitude()));
        // reverserL/R's own StateMagnitude() is DELIBERATELY NOT
        // folded in here, unlike the diffuser just above -- the diffuser's
        // StateMagnitude() scans retained AUDIO SAMPLES (same scale as
        // lineL/lineR, so std::max-ing them together is meaningful), while
        // DelayReverser::StateMagnitude() reports an elapsed BUFFER-POSITION
        // counter that grows into the hundreds or thousands of samples
        // during ordinary play and only resets at ClearBuffers() -- folding
        // it into a std::max() with actual sample magnitudes would make
        // this function report a large "magnitude" from a properly-silent
        // unit any time Reverse Blend had run since the last clear, which
        // would break the Stop-flush harness's own "cleared once and
        // stayed clear" use of this function (this struct's own header
        // comment). See DelayReverser's header comment for where its
        // StateMagnitude() IS used instead (directly, by tests).
        return magnitude;
    }

    // :42-56 (setSampleRate). This
    // struct's
    // only production caller is FroggersAppCore::PrepareToPlay()
    // (delay_.SetSampleRate(sampleRate_)), which validates the host's
    // sample rate ONCE before any downstream use -- so `hz` here is always
    // already positive, and a defensive `44100.0f` re-guard here would be
    // unreachable. The bare `sampleRate = 44100.0f`
    // member default (this struct's `sampleRate` field, declared near its
    // other members below) is a different concern -- a harmless
    // pre-`SetSampleRate()` placeholder; every caller already calls
    // SetSampleRate() before Process() (see the wetLimiter configuration a
    // few lines below this method for that citation), so it is never
    // observably read unconfigured -- and is left as is.
    void SetSampleRate(float hz)
    {
        sampleRate = hz;
        capacity = std::min(kMaxDelaySamples, static_cast<size_t>(std::ceil(kMaxDelaySeconds * sampleRate)));
        if (capacity < 4)
        {
            capacity = 4;
        }
        lineL.assign(capacity, 0.0f);
        lineR.assign(capacity, 0.0f);
        writePos = 0;
        lfoPhase = 0.0f;
        lfoInc = 2.0f * 3.14159265f * 0.25f / sampleRate;
        // The wet limiters' attack/release coefficients are sample-
        // rate-dependent (dsp::OutputLimiter::Configure), so they are
        // (re)configured here, the one place a real sample rate is known --
        // every caller of this struct (production's `delay_.SetSampleRate`
        // in FroggersAppCore::PrepareToPlay(), and every DSP-parity test
        // that constructs a StereoDelay) already calls SetSampleRate()
        // before ever calling Process(), so there is no path that reaches
        // the limiter unconfigured.
        wetLimiterL.Configure(sampleRate, kDelayWetLimiterThreshold, kDelayWetLimiterCeiling,
                               kDelayWetLimiterAttackSeconds, kDelayWetLimiterReleaseSeconds);
        wetLimiterR.Configure(sampleRate, kDelayWetLimiterThreshold, kDelayWetLimiterCeiling,
                               kDelayWetLimiterAttackSeconds, kDelayWetLimiterReleaseSeconds);
        // Same reason as the limiters immediately above: this is the one place
        // a real sample rate is known.
        wetAuthority.Configure(sampleRate);
        // Section delay lengths (4.7ms/12.3ms/21.1ms) are converted
        // to samples here, the one place a real sample rate is known --
        // same rationale as the wetLimiter Configure calls just above.
        diffuserL.SetSampleRate(sampleRate);
        diffuserR.SetSampleRate(sampleRate);
        // DelayReverser carries no sample-rate-dependent
        // configuration of its own (its window length is derived fresh
        // from timeL/timeR every Process() call, the same way the forward
        // tap's own delay time already is) -- but `writePos` is reset to 0
        // immediately above, and reverserL/R's `pos`/`elapsed` are only
        // ever meaningful relative to the CURRENT writePos, so they must
        // be reset here too or the first read after a rate change would
        // measure position against a writePos that no longer matches what
        // they were last anchored to.
        reverserL.Reset();
        reverserR.Reset();
    }

    // ExpMapCompute range [0.25, 4.0] -- the SAME idiom already
    // established in this codebase for a "drive" pre-gain into a saturator
    // (FilterFxChain's Comb Drive, FroggersAppCore.hpp, Filter slot
    // 7), reused rather than a fresh range invented for the same concept.
    // Default knob 0.5f reproduces fbDrive == 1.0f exactly (unity):
    // ExpMapCompute(0.25,4,0.5) == 0.25*sqrt(16) == 1.0.
    void SetFeedbackDrive(float knob01) { fbDrive = ExpMapCompute(0.25f, 4.0f, knob01); }

    // The SAME control as the Drive bank's Tone, so the same map: both read
    // DspMath.hpp's ToneAlphaFromKnob, which is where the range and the
    // reason for its floor live. This one sits inside the feedback loop, so
    // its filter is applied on every pass and its darkening compounds across
    // repeats -- which makes an inaudibly low floor worse here than on a
    // through-signal, not better.
    void SetFeedbackTone(float knob01)
    {
        const float alpha = ToneAlphaFromKnob(knob01);
        fbToneL.alpha = alpha;
        fbToneR.alpha = alpha;
    }

    // (Delay slot 11, "Mod rate" / "MdRt"): replaces the hardcoded
    // 0.25Hz baked into SetSampleRate's own lfoInc formula above. Range
    // [0.05, 1.25] Hz chosen so the geometric mean lands exactly on today's
    // 0.25Hz (0.05*1.25 == 0.0625 == 0.25^2), giving an exact default at
    // knob 0.5f: ExpMapCompute(0.05,1.25,0.5) == sqrt(0.0625) == 0.25.
    // Recomputes lfoInc with the SAME formula SetSampleRate uses (2*pi*hz/
    // sampleRate); only has effect once SetSampleRate has already run
    // (every production/test caller already does, see SetSampleRate's own
    // comment above) -- until then, SetSampleRate's own fixed-0.25Hz
    // fallback (unchanged above) still applies.
    void SetModRate(float knob01)
    {
        const float hz = ExpMapCompute(0.05f, 1.25f, knob01);
        lfoInc = 2.0f * 3.14159265f * hz / sampleRate;
    }

    // Identity map -- widthBalance == knob01 directly, relying on the
    // SAME [0,1] knob-range invariant `p.dwid` itself already relies on
    // (Process()'s own pre-existing comment below: "p.dwid*0.5f cannot
    // exceed 0.5"). Because both new weights in Process() are
    // `<literal> * widthBalance * p.dwid` and widthBalance never exceeds
    // 1.0f (it IS the raw [0,1] knob value), neither weight can exceed its
    // OLD ceiling (0.35f / 0.5f respectively) at any knob position:
    //   (a) cross-feed stays in [0,1]: max is 0.5*1.0*1.0 == 0.5, the same
    //       ceiling as today, itself already inside [0,1].
    //   (b) spread never exceeds today's own maximum
    //       (0.35f*baseSeconds*dwid): widthBalance<=1 makes the new term a
    //       pointwise-smaller-or-equal version of the old one for every
    //       (dwid, baseSeconds) pair, so no combination of settings newly
    //       reaches the pre-existing buffer-capacity overrun that did not
    //       already reach it today -- reachability can only shrink, never
    //       grow.
    // Default knob 1.0f reproduces widthBalance == 1.0f exactly, i.e.
    // today's fixed 0.35f/0.5f literals exactly.
    void SetWidthBalance(float knob01) { widthBalance = knob01; }

    // SampleRateReducer, reused AS-IS -- same mapping shape already
    // used for the Drive bank's SRR1/SRR2 knobs (FroggersAppCore.hpp Drive
    // bank wiring), reused rather than invented fresh for the same
    // concept. Default knob 0.0f gives
    // freq == 1e-2 + ZeroedExpCompute(10,1) == 1.01 >= 1.0f, which is
    // SampleRateReducer::Process's own exact-bypass branch -- "no
    // crushing" at default is bit-identical passthrough, not merely an
    // approximation.
    void SetCrush(float knob01)
    {
        const float freq = 1e-2f + ZeroedExpCompute(10.0f, 1.0f - knob01);
        crushL.SetFreq(freq);
        crushR.SetFreq(freq);
    }

    // :58-101 (process).
    DelayWetPair Process(float bumpIn, const DelayParams& p)
    {
        if (p.dsnd <= 0.0001f || capacity == 0)
        {
            lastWet = {};
            // Send is off, so Wet/dry goes inert regardless of what the line
            // still holds: the follower's TARGET drops at once rather than
            // tracking the decaying tail. The follower itself keeps running,
            // so what an operator hears is a fade at the release constant, not
            // a cut. This is why the update is reached on this path at all --
            // an early return that skipped it would freeze authority at
            // whatever the last fed sample left behind.
            AdvanceWetLevel(0.0f);
            return lastWet;
        }

        const float baseSeconds = ExpMapCompute(0.001f, kMaxDelaySeconds, p.dtim);
        lfoPhase += lfoInc;
        if (lfoPhase > 6.2831853f)
        {
            lfoPhase -= 6.2831853f;
        }
        const float modSeconds = std::sin(lfoPhase) * p.dmod * baseSeconds * 0.08f;
        // 0.35f now scaled by widthBalance (SetWidthBalance, never
        // exceeds 1.0f) -- see that setter's own comment for how bound (b)
        // holds by construction.
        const float widthSpread = p.dwid * baseSeconds * 0.35f * widthBalance;
        float timeL = std::max(0.001f, baseSeconds + modSeconds);
        float timeR = std::max(0.001f, baseSeconds + modSeconds + widthSpread);

        float dL = ReadAt(timeL, lineL);
        float dR = ReadAt(timeR, lineR);

        // (Reverse Blend, Delay slot 7): a second, backward-
        // travelling read pointer per channel (reverserL/reverserR,
        // ApplyReverse below), blended against the forward tap read just
        // above. Placement is deliberate -- immediately after the forward
        // reads and BEFORE the feedback cross-mix/write below, so both the
        // feedback path and the wet output see the SAME blended dL/dR from
        // here on; Diffusion (further below) therefore diffuses
        // whatever Reverse Blend already produced, not the raw forward tap.
        //
        // THE REVERSE TAP RUNS UNCONDITIONALLY, and only its OUTPUT is
        // branched -- the same postflight-fix shape this file already
        // carries for Diffusion (see that comment further below).
        // ApplyReverse both advances the backward pointer/wrap-crossfade
        // and returns this sample's reverse-tap value, so neither ever
        // goes stale while drev sits at its 0.0f default -- the same defect
        // class already guarded for Diffusion, guarded here the same way.
        //
        // Exactness at the default is preserved by the output branch
        // below, not by relying on `dL*(1-0)+reverse*0` to happen to be
        // exact -- "guaranteed rather than argued," the same rule
        // Diffusion's own call site states for the identical reason.
        const float reverseTapL = ApplyReverse(reverserL, timeL, lineL);
        const float reverseTapR = ApplyReverse(reverserR, timeR, lineR);
        const float revBlendedL = dL * (1.0f - p.drev) + reverseTapL * p.drev;
        const float revBlendedR = dR * (1.0f - p.drev) + reverseTapR * p.drev;
        dL = (p.drev == 0.0f) ? dL : revBlendedL;
        dR = (p.drev == 0.0f) ? dR : revBlendedR;

        // 0.5f now scaled by widthBalance -- see SetWidthBalance's own
        // comment for how bound (a) (cross-feed stays in [0,1]) holds by
        // construction.
        const float cross = p.dwid * 0.5f * widthBalance;
        float fbL = dL * (1.0f - cross) + dR * cross;
        float fbR = dR * (1.0f - cross) + dL * cross;
        const float fbk = std::min(std::max(p.dfbk, 0.0f), 0.98f);
        const float send = std::min(std::max(p.dsnd, 0.0f), 1.0f);
        const float inSignal = bumpIn * send;

        // (Crush): feedback tap's repeats routed through
        // SampleRateReducer, reused AS-IS, ahead of the tone filter and
        // drive gain below. Both bypass exactly at their own defaults
        // (see field comments above), so this is a no-op chain at defaults.
        fbL = crushL.Process(fbL);
        fbR = crushR.Process(fbR);

        // (Feedback tone): damps the tap ahead of Saturate.
        fbL = fbToneL.Process(fbL);
        fbR = fbToneR.Process(fbR);

        // ("delay is the
        // only unsaturated feedback stage"): `fbL`/`fbR` are unbounded reads
        // straight off the delay line (ReadAt), so the pre-fix
        // `inSignal + fbL * fbk` fed a linear, unsaturated loop -- steady
        // state `in*send/(1-fbk)`, 50x input at fbk's 0.98 clamp. Wraps the
        // fed-back term in the SAME PadeSaturator::Saturate the comb's own
        // loop already uses (`Comb::Process` above, FilterFx.hpp -- reused,
        // not reimplemented), applied to the tapped signal before
        // the feedback-gain multiply, exactly mirroring `feedback *
        // Saturate(filter.Process(tapped))`. `Saturate` clamps to +-1
        // unconditionally, so this line can never write more than
        // `|inSignal| + fbk` regardless of how many round trips have
        // already run -- a per-sample bound, not just a steady-state one.
        //
        // (Feedback drive): fbDrive multiplies the ARGUMENT of Saturate
        // ONLY -- Saturate's
        // own +-1 clamp still bounds this line to `|inSignal| + fbk`
        // regardless of fbDrive (writing `fbDrive * fbk * Saturate(...)`
        // instead would raise that bound; deliberately not done).
        //
        // (Freeze, Delay slot 5): a
        // crossfade, not a write-enable toggle. Two quantities change at
        // the write below -- inSignal's own attenuation and fbk's own
        // effective value -- computed here because both are consumed ONLY
        // at this write.
        //
        // freezeEff attenuates new input (`inSignal * (1 - freezeEff)`):
        // at freeze==0 that is a multiply by an exact 1.0f, so the write
        // stays bit-identical to the prior line -- guaranteed by the
        // arithmetic, the same "branch keeps equality guaranteed rather
        // than argued" convention ddif/drev's own call sites use above.
        //
        // Supersedes an earlier product-clamp approach: Freeze is specified
        // in Freeze's OWN terms and this
        // mapping deliberately does not mention `fbDrive`.
        //
        // The knob runs fbk -> 1.0 monotonically UPWARD, so turning Freeze
        // up always increases feedback at every Feedback Drive setting. It
        // tops out at unity: lossless recirculation, no self-amplification.
        // That ceiling IS the clamp -- expressed as a property of the
        // mapping rather than a min() applied afterward.
        //
        // The PRIOR mapping was `fbk + (1/fbDrive - fbk) * freeze`, which
        // clamped the fbEff*fbDrive PRODUCT to unity. That form is
        // NON-MONOTONIC: at fbDrive 4.0 it runs 0.98 -> 0.25, so raising
        // Freeze LOWERED loop gain (3.92 -> 1.00). A knob that goes down as
        // it turns up violates this change's own requirement that a control
        // move "in the direction its name implies". What `fbDrive` multiplies
        // afterward is Feedback Drive's business, exactly as it is for every
        // other value in this loop.
        //
        // At freeze==0, `fbk + (1.0f - fbk) * 0.0f` is bit-exact `fbk` (the
        // zero multiply is what makes it exact), so today's fbk*fbDrive
        // product -- up to ~3.92 at max Feedback Drive -- is left completely
        // untouched, and the accidental Stop-sustain behaviour survives
        // unchanged.
        //
        // dfrzLatched overrides the ceiling outright with an above-unity
        // value, so the loop amplifies and the latch reaches a state the
        // encoder cannot -- the override is reachable only through
        // dfrzLatched, never through dfrz.
        const float freeze = std::min(std::max(p.dfrz, 0.0f), 1.0f);
        const float freezeEff = p.dfrzLatched ? 1.0f : freeze;
        const float fbEff = FreezeFeedback(fbk, freeze, p.dfrzLatched);
        WriteSample(inSignal * (1.0f - freezeEff) + fbEff * PadeSaturator::Saturate(fbDrive * fbL), lineL);
        WriteSample(inSignal * (1.0f - freezeEff) + fbEff * PadeSaturator::Saturate(fbDrive * fbR), lineR);
        AdvanceWrite();

        // The loop write above already committed using the
        // UNLIMITED `dL`/`dR` taps -- the in-loop saturator alone still
        // governs the loop's own dynamics/persistence, unaffected.
        // `wetLimiterL`/`wetLimiterR` are applied strictly AFTER
        // that write, to what actually ESCAPES this stage toward
        // `ToReverbMono`/Reverb/the master limiter, so a hot delay tail no
        // longer forces the downstream chain (Reverb, then the master) to
        // duck around it. See this file's header comment (above the struct)
        // for the tuning and its measurement.
        // (Diffusion, Delay slot 8): applied to dL/dR here -- AFTER
        // the feedback loop's WriteSample/AdvanceWrite calls above (which
        // already committed using the unlimited taps, an established
        // split) and BEFORE wetLimiterL/R just below. Applying
        // it at this exact point means diffusion smears the wet tap ONCE
        // per repeat, equally for every repeat, rather than compounding
        // once per round trip the way an in-loop placement would (a
        // different, rejected control -- see the SchroederAllpassSection/
        // DelayDiffuser header comments above for the full rationale); the
        // existing wet limiters still bound whatever escapes afterward,
        // unchanged.
        //
        // THE DIFFUSER RUNS UNCONDITIONALLY, and only its OUTPUT is branched
        // (a postflight fix -- the original wrote
        // `(p.ddif == 0.0f) ? dL : ApplyDiffusion(...)`, which skipped the
        // CALL and therefore never advanced the sections' xHistory/yHistory
        // while Diffusion sat at its default). Frozen history is stale
        // history: each section holds up to kSection3BaseSeconds (21.1 ms)
        // of it, so re-engaging Diffusion replayed audio captured at
        // whenever the knob was last nonzero. `ddif` is an ordinary bank
        // parameter and therefore a modulation TARGET like the other 83, so
        // a source sweeping it across zero would have frozen and thawed that
        // stale content at audio rate -- not merely a knob-turn click.
        //
        // Exactness at the default is preserved by the output branch below
        // rather than by skipping the work: at ddif==0 the coefficient `a`
        // is 0, so each section degenerates to a pure M-sample delay whose
        // history tracks the real signal, and `dry*(1-0) + diffused*0`
        // compares equal to `dry` regardless. The branch keeps that equality
        // guaranteed rather than argued.
        const float diffusedWetL = ApplyDiffusion(diffuserL, dL, p.ddif);
        const float diffusedWetR = ApplyDiffusion(diffuserR, dR, p.ddif);
        const float diffusedL = (p.ddif == 0.0f) ? dL : diffusedWetL;
        const float diffusedR = (p.ddif == 0.0f) ? dR : diffusedWetR;

        lastWet.l = wetLimiterL.Process(diffusedL);
        lastWet.r = wetLimiterR.Process(diffusedR);
        // Follows the same `monoWet` ToReverbMono forms, so one follower
        // serves both channels and measures the value the crossfade actually
        // mixes in.
        AdvanceWetLevel(std::fabs((lastWet.l + lastWet.r) * 0.5f));
        return lastWet;
    }

    // Thin name alias over WetAuthorityFollower::Advance (Limiter.hpp) --
    // called from both of Process()'s exits, which is what makes Send-off a
    // target change rather than a frozen follower.
    void AdvanceWetLevel(float target) { wetAuthority.Advance(target); }

    // How much of Wet/dry's travel the wet path has earned: proportional to
    // the level it holds, saturating where this stage stops getting louder.
    float WetAuthority() const { return wetAuthority.Authority(); }

    DelayWetPair GetLastWet() const { return lastWet; }

    // :108-113 (toReverbMono).
    // Crossfades the dry signal against the wet PAIR and keeps the pair. This
    // used to fold to mono here, which spent the whole stereo delay -- two
    // lines, a cross-feed, per-channel limiters -- and threw the result away
    // on the next line, leaving the Stereo width control unable to widen
    // anything.
    //
    // Scaled by what the wet path actually holds, so the control cannot trade
    // the instrument away for a processed signal that is not there. Reads
    // follower state rather than updating it, which is what lets this stay
    // const; Process() owns the update.
    //
    // The dry source is mono (everything upstream of this stage is a single
    // float), so it feeds both channels equally and the image comes entirely
    // from the wet pair.
    StereoSample ToStereo(float bumpIn, DelayWetPair wet, float dmix) const
    {
        const float mix = std::min(std::max(dmix, 0.0f), 1.0f) * WetAuthority();
        // Equal-power crossfade, not linear: a linear `(1-mix)*dry + mix*wet`
        // only holds level when the two legs are correlated, and here they
        // are not -- the wet path is a delayed, filtered, cross-fed copy of
        // the same signal, largely uncorrelated with dry at the frequencies
        // that matter. MEASURED worst dip across the control's whole travel
        // under the old linear law: -4.92 dB at 220 Hz, -6.50 dB at 440 Hz,
        // -16.53 dB at 880 Hz. cos/sin quadrant weights hold level regardless
        // of that correlation, the same fix dsp::DriveBlendPhase::Process
        // (Drive.hpp) already applies to the Drive page's own Wet/Dry.
        //
        // theta is capped at acos(kMinDryLevel) rather than a full quarter
        // turn (dsp::kMinDryLevel's own comment, Limiter.hpp), so mix == 1.0
        // reaches this stage's dry floor, not silence -- the dry signal's
        // own gain never drops below kMinDryLevel no matter where Wet/dry
        // sits.
        //
        // The law itself lives in dsp::EqualPowerWetDry (Limiter.hpp), shared
        // with the Drive page, whose only difference from this one is the
        // floor it passes. A nonzero floor is also why this page needs no
        // exact case at the top of its travel: thetaMax sits well short of
        // pi/2, the one place std::cos stops landing on an exact value. The
        // zero endpoint the helper does hold exactly, because mix == 0 has to
        // return `bumpIn` bit-for-bit here and an existing pin depends on it.
        const WetDryGains gains = EqualPowerWetDry(mix, kMinDryLevel);
        const float dryGain = gains.dry;
        const float wetGain = gains.wet;
        const float dry = dryGain * bumpIn;
        return {dry + wetGain * wet.l, dry + wetGain * wet.r};
    }

private:
    // :116-124 (readAt). NOTE: before `writePos` has advanced past
    // `delaySamples` (i.e. during the first ~delaySamples calls after
    // construction/clear), `readPos` is negative, and casting a negative
    // float straight to `size_t` is a negative-float-to-unsigned
    // conversion the standard leaves unspecified/UB when the value is out
    // of range -- identical to the retired simulator's frozen StereoDelay.hpp:120.
    //
    // FIX, NOT A REPRODUCTION: as
    // with DigitalReorganizer::Process (Drive.hpp), there is no correct
    // frozen reference to port here instead -- the retired simulator's frozen StereoDelay.hpp
    // hits the identical UB, so carrying it forward would have no parity
    // value. The intended behavior during warm-up (documented above as
    // "confirmed harmless on this target -- correct silence, since the
    // still-unwritten region of the buffer is zero-filled") is made
    // portable and well-defined by flooring and wrapping the index in a
    // signed integral domain *before* ever narrowing to `size_t`, instead
    // of relying on this target's incidental truncating-cast-of-negative-
    // float-saturates-to-0 behavior. For any `readPos >= 0` (the
    // already-well-defined case) `static_cast<long long>(floorPos)` equals
    // the original `static_cast<size_t>(readPos)` exactly (both truncate
    // toward zero for non-negative values, and `readPos - floor(readPos)`
    // is unchanged), so behavior is bit-for-bit identical there; only the
    // negative case, previously UB, is newly defined (floor-mod wraps into
    // the buffer, reading back the zero it was cleared to).
    float ReadAt(float seconds, const std::vector<float>& line) const
    {
        const float delaySamples = seconds * sampleRate;
        const float readPos = static_cast<float>(writePos) - delaySamples;
        const float floorPos = std::floor(readPos);
        const float frac = readPos - floorPos;

        const long long capacityI = static_cast<long long>(capacity);
        long long idx0I = static_cast<long long>(floorPos) % capacityI;
        if (idx0I < 0)
        {
            idx0I += capacityI;
        }
        const size_t idx0 = WrapIndex(static_cast<size_t>(idx0I));
        const size_t idx1 = WrapIndex(idx0 + 1);
        return line[idx0] * (1.0f - frac) + line[idx1] * frac;
    }

    // :126-129 (writeSample).
    void WriteSample(float sample, std::vector<float>& line) { line[writePos] = sample; }

    // :131-138 (wrapIndex).
    size_t WrapIndex(size_t idx) const
    {
        while (idx >= capacity)
        {
            idx -= capacity;
        }
        return idx;
    }

    // :140-147 (advanceWrite).
    void AdvanceWrite()
    {
        writePos++;
        if (writePos >= capacity)
        {
            writePos = 0;
        }
    }

    // Applies one channel's wet-tap diffuser and returns the
    // blended result -- `dry*(1-ddif) + diffused*ddif`.
    // Called for EVERY sample, including ddif==0, so the sections' history
    // stays current -- see Process()'s own call site for why skipping the
    // call at zero was a defect. The caller branches only the OUTPUT, which
    // is what makes the ddif==0 result exactly `dry`.
    float ApplyDiffusion(DelayDiffuser& diffuser, float dry, float ddif)
    {
        const float a = ddif * kDiffusionCoeffScale;
        const float diffused = diffuser.Process(dry, a);
        return dry * (1.0f - ddif) + diffused * ddif;
    }

    // Advances one channel's backward read pointer by exactly one
    // sample and returns its (possibly wrap-crossfaded) output. Called for
    // EVERY sample, including drev==0, so the pointer and its crossfade
    // never go stale -- see Process()'s own call site for why skipping the
    // call at zero was the same defect already fixed for Diffusion, guarded here the same
    // way. `timeSeconds` is the SAME per-channel forward delay time
    // (timeL/timeR) Process() already computed for the forward tap just
    // above -- the reverse window is "one delay-time long", not an
    // independently-configured length.
    //
    // Reuses ReadAt/WrapIndex for the actual interpolated read (avoiding a
    // duplicate implementation of that math) by
    // expressing the backward pointer's absolute buffer position as a
    // `seconds`-behind-the-CURRENT-writePos value, recomputed fresh every
    // call: `seconds = (writePos - rev.pos) / sampleRate`. Substituting
    // into ReadAt's own `readPos = writePos - seconds*sampleRate` cancels
    // the current `writePos` out exactly -- `readPos == rev.pos` (mod
    // capacity) always, regardless of what writePos currently is,
    // INCLUDING across writePos's own circular wraparound (WrapIndex's
    // modulo absorbs any multiple of `capacity` in the intermediate
    // `seconds` value the same way it already does for any other read).
    // `rev.pos` itself is the only piece of state that free-runs,
    // decremented by exactly one sample every call -- that decrement, not
    // the seconds/writePos arithmetic around it, is what makes this read
    // travel backward through history instead of tracking writePos the way
    // the forward tap's roughly-constant-seconds ReadAt call does.
    float ApplyReverse(DelayReverser& rev, float timeSeconds, const std::vector<float>& line)
    {
        const float delaySamplesWindow = timeSeconds * sampleRate;
        const float fadeSamples = std::min(kReverseWrapCrossfadeSeconds * sampleRate, delaySamplesWindow * 0.5f);
        const float writePosF = static_cast<float>(writePos);

        // Unconditional advance -- both the primary pointer and the plain elapsed-time
        // counter that times the wrap crossfade move every call,
        // regardless of drev.
        rev.pos -= 1.0f;
        rev.elapsed += 1.0f;

        float out = ReadAt((writePosF - rev.pos) / sampleRate, line);

        if (rev.fading)
        {
            // Second pointer, armed below, sweeping its own fresh lap in
            // parallel with the outgoing one -- also advanced
            // unconditionally while armed, same rule as `pos` above.
            rev.fadePos -= 1.0f;
            const float incoming = ReadAt((writePosF - rev.fadePos) / sampleRate, line);
            rev.fadeGain = std::min(1.0f, rev.fadeGain + 1.0f / fadeSamples);
            out = out * (1.0f - rev.fadeGain) + incoming * rev.fadeGain;

            if (rev.fadeGain >= 1.0f)
            {
                // Crossfade complete: the incoming lap becomes primary.
                // `elapsed` carries forward at `fadeSamples` (how long the
                // incoming lap has already been running), not 0, so the
                // NEXT wrap's own timing is measured correctly rather than
                // restarting the count from scratch.
                rev.pos = rev.fadePos;
                rev.elapsed = fadeSamples;
                rev.fading = false;
                rev.fadeGain = 0.0f;
            }
        }
        else if (rev.elapsed >= delaySamplesWindow - fadeSamples)
        {
            // Enter the wrap crossfade (the sweep covers one
            // delay-time of history, then wraps, and the wrap must be
            // declicked): arm a second pointer starting a fresh lap at
            // "now" -- one sample behind the write head, the freshest
            // already-committed sample (writePos itself still holds THIS
            // call's about-to-be-overwritten slot until WriteSample runs
            // later in Process(), so age 0 would read stale,
            // capacity-samples-old content instead of the newest one).
            rev.fading = true;
            rev.fadePos = writePosF - 1.0f;
            rev.fadeGain = 0.0f;
        }

        return out;
    }

    float sampleRate = 44100.0f;
    size_t capacity = 0;
    size_t writePos = 0;
    std::vector<float> lineL;
    std::vector<float> lineR;
    DelayWetPair lastWet{};
    float lfoPhase = 0.0f;
    float lfoInc = 0.0f;
};

// the retired simulator's DelayState.hpp:165-198 (processInsert): originally the row ->
// DelayParams mapping (:180-186) and the Color/Halo fold (:187-193).
// REMOVED: the fold is gone -- every one of the nine arguments below now
// maps to exactly one DelayParams field, no two are combined, and the
// freeze/reverse/diffusion arguments carry the retired Detune/Color/Halo
// rows' new identities (Freeze/Reverse blend/Diffusion,
// FroggersParameters.hpp). Named by DSP field, not by Delay-bank slot --
// the bank's own slots move independently of this order as the Delay bank
// gets renumbered (its own call site says which slot feeds which argument).
// Callers supply the nine already-fuegoized/modulated 0..1 values in this
// fixed field order; this function owns none of the smoothing/mod/fuego
// machinery that produces them (see file header "NOT ported").
inline DelayParams MapRowsToDelayParams(float timeKnob01,
                                        float sendKnob01,
                                        float feedbackKnob01,
                                        float widthKnob01,
                                        float freezeKnob01,
                                        float modKnob01,
                                        float mixKnob01,
                                        float reverseKnob01,
                                        float diffusionKnob01)
{
    DelayParams params;
    params.dtim = timeKnob01;   // :180
    params.dsnd = sendKnob01;   // :181
    params.dfbk = feedbackKnob01;  // :182
    params.dwid = widthKnob01;  // :183
    params.dfrz = freezeKnob01;  // was :184 (Detune/ddet) -- Delay bank slot 5 is now Freeze; see DelayParams::dfrz.
    params.dmod = modKnob01;    // :185
    params.dmix = mixKnob01;    // :186
    params.drev = reverseKnob01;    // Delay bank slot 7 is Reverse blend (was Color, previously folded into ddet).
    params.ddif = diffusionKnob01;  // Delay bank slot 8 is Diffusion (was Halo, previously folded into dmod).
    return params;
}

}  // namespace synth_froggers::dsp
