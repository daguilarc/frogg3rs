#pragma once

// synth_froggers::dsp::Reverb -- a **copy** of the cited Froggers formulas
// -- read directly from the firmware source before porting, not from memory.
//
// Ported (7 of the Reverb page's 9 params -- Wet/dry, Room size, Decay,
// Pre-delay, Damping, Stereo width, Diffusion) from:
//   - 08b5fd3:src/core/FroggersEngine.hpp:493-536  ProcessReverb, verbatim signal
//     path (pre-delay tap, twin comb-ish delay lines A/B with cross-feed
//     diffusion, shared damping low-pass, stereo width blend)
//   - 08b5fd3:src/core/FroggersEngine.hpp:455-461  param wiring:
//       :455 Wet/dry   = m_reverbParams->GetParam(0), direct passthrough
//       :456 Room size = ExpMap(0.05, 1.0, GetParam(1))
//       :457 Decay     = ExpMap(0.1, 0.98, GetParam(2))
//       :458 Pre-delay = ExpMap(1/sr, 100/sr, GetParam(3)) [see note below]
//       :459 Damping   = ExpMap(0.001, 0.2, 1 - GetParam(4)), fed directly
//            as the shared damping filter's alpha (:574 `m_rvDampFilter
//            .m_alpha = m_rvDamp.Process()`) -- NOT run through
//            SetAlphaFromNatFreq; the ExpMap output IS the alpha.
//       :460 Stereo width = GetParam(5), direct passthrough
//       :461 Diffusion    = GetParam(6), direct passthrough
//   - 08b5fd3:src/core/FroggersEngine.hpp:847 (ApplyOutputFx) -- the Wet/dry
//     blend `(1-rvMix)*output + rvMix*rvb` that actually consumes Wet/dry;
//     ProcessReverb itself never reads m_rvMix.
//   - 08b5fd3:src/core/FroggersEngine.hpp:65-69 (x_rvSize = 4096, the three
//     x_rvSize-length ring buffers, and m_rvDampFilter's type OPLowPassFilter,
//     ported here as the shared dsp::OnePoleLowPass).
//
// Pre-delay note: the firmware's ExpMap divided both endpoints by sampleRate
// (`ExpMap(1.0f/sr, 100.0f/sr, knob)`) and ProcessReverb then re-multiplied
// the result by sampleRate to get a sample count (:305 `preNorm *
// m_sampleRate`). Those sampleRate terms are exact algebraic inverses of
// one another around the exponential map, so the round trip always
// cancelled back to a range of 1 to 100 SAMPLES (0.02ms to 2.08ms at
// 48kHz) -- a range that reads as milliseconds but is not one: it moves
// the tail's arrival by about two milliseconds across the WHOLE knob,
// inside the window where a listener fuses a pre-delay with its own tail
// rather than hearing it as separate. `PreDelayNormFromKnob` below instead
// maps the knob across an absolute millisecond range, converted to samples
// by the ACTUAL sampleRate at each call so the map still returns a time
// (in seconds) that `Process()` multiplies back into a sample count --
// the exponential shape is unchanged, only what its endpoints mean is.
//
// Smoothing NOT ported: FroggersEngine.hpp reads every one of these seven
// knobs through a RuntimeParam (a one-pole smoothing filter on the raw
// 0..1 target, to avoid zipper noise on knob movement) before using the
// value. That smoothing is parameter-model infrastructure, not DSP, and
// per this port's established convention (see Vco.hpp, FilterFx.hpp) is
// owned by Sheaf's parameter model -- callers of this unit pass already-
// resolved 0..1 knob values per sample.
//
// NOT ported (deliberately): Mod depth and Hold (Reverb page
// slots 7 and 8). `m_reverbParams->GetParam(7)`/`GetParam(8)` are never
// read anywhere in FroggersEngine.hpp -- confirmed by grep -- so there is
// no formula to pin. They are newly authored below, clearly marked,
// with behavioral (not parity) tests. At their neutral defaults
// (modDepthKnob=0, holdKnob=0) the authored stage is a no-op, so the seven
// ported params reproduce ProcessReverb + the Wet/dry blend exactly.

#include "DspMath.hpp"
#include "Drive.hpp"     // reuse dsp::DigitalReorganizer AS-IS for the Grit knob (same reuse Delay.hpp makes of dsp::SampleRateReducer).
#include "FilterFx.hpp"  // reuse dsp::PadeSaturator (same reuse Delay.hpp makes).
#include "Limiter.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace synth_froggers::dsp {

// Answers the question "why can't we just have limiters for reverb
// and delay?"
// Tuning for `Reverb::wetLimiter` below, a single
// `dsp::OutputLimiter` (dsp/Limiter.hpp) instance inserted on the value
// `Process()` returns, AFTER both tanks and the dry/wet mix -- NOT inside
// the feedback loop, and it never touches `fb`/Hold's own computation
// (unchanged, see below). Hold (`fb` -> ~0.99998 at Hold's
// ceiling, ~50,000x steady-state gain, deliberately parked just under
// self-oscillation) keeps sustaining exactly as before; this stage only
// caps the LEVEL that escapes toward the master limiter, the same "bound
// what escapes, don't touch what persists" treatment Delay's own wet
// limiter gets.
//
// Chosen BY MEASUREMENT (scratch harness driving this exact struct with
// Hold pinned to 1.0/max, decay knob swept {0,0.5,1.0}, room size knob swept
// {0,0.3,0.6,1.0}, sustained full-scale input, fully wet (mix=1.0, isolates
// the tank's own bound from any diluting dry floor), 20000 samples per
// point):
//
//   - Raw (no limiter) growth is genuinely unbounded and, unlike Delay's
//     worst case, DOES build over hundreds of milliseconds to seconds at
//     moderate room sizes -- measured raw |output| reached ~7x input by
//     100ms, ~14x by 200ms, ~72x by 1s, ~290x by 4s of sustained input
//     (size=0.6, decay=1.0, Hold=1.0) -- confirming the "sustained
//     material" premise for the STEADY-STATE climb.
//   - But master's own tuning (0.9/1ms/100ms) is still NOT sufficient: the
//     measured worst-case TRANSIENT overshoot (at the smallest room size,
//     shortest tank round trip -- the reverb's analogue of Delay's
//     shortest-delay-time worst case) was 1.282757 against the 1.0
//     ceiling, at a 1ms attack. Same failure shape as Delay's own wet
//     limiter: the onset is fast even though the long-run climb is slow.
//   - kReverbWetLimiterThreshold (0.9): kept AT the master's value, for the
//     identical reason given in dsp/Delay.hpp's own wet-limiter comment -- this stage
//     runs inside `Reverb::Process`, upstream of `SanitizeOutputSample`'s
//     master limiter in FroggersAppCore.hpp, so it always reduces first
//     regardless of where its threshold sits relative to the master's.
//   - kReverbWetLimiterAttackSeconds (2 microseconds): sweeping attack
//     alone (threshold 0.9, release 100ms, worst case taken across every
//     room-size/decay combination above) found the worst-case overshoot
//     crosses under the 1.0 ceiling between 3us (1.000006) and 2us
//     (1.000000); 2 microseconds sits at that measured crossing -- the
//     identical value Delay's own sweep found for its own worst case
//     (dsp/Delay.hpp's own comment), not copied from it by analogy.
//   - kReverbWetLimiterReleaseSeconds (100ms, matches the master): as with
//     Delay's own wet limiter, the worst-case peak is an attack-side effect; release does not
//     move it, so it stays at the value this codebase already uses for
//     "gain reduction that does not pump" (dsp::OutputLimiter::
//     kDefaultReleaseSeconds, dsp::kPeakLimiterReleaseSeconds,
//     dsp::kDelayWetLimiterReleaseSeconds) rather than inventing a fourth
//     number where measurement gave no reason to move it.
// Retargeted from 0.9 to 0.72, preserving the ORIGINAL
// threshold/ceiling ratio (0.9/1.0 == 0.72/0.80) rather than picking a
// round number, so this stage's measured knee (attack/release above) keeps
// its character instead of being re-tuned by accident. 0.9 was strictly
// below kSharedCeiling (1.0) but is ABOVE the retargeted kStageCeiling
// (0.80) -- the negative-headroom exponential-amplifier trap the
// static_assert below exists to catch -- so it comes down in this same edit.
inline constexpr float kReverbWetLimiterThreshold = 0.72f;
inline constexpr float kReverbWetLimiterCeiling = kStageCeiling;
// See dsp::OutputLimiter::kDefaultThreshold's own static_assert
// (dsp/Limiter.hpp).
static_assert(kReverbWetLimiterThreshold < kReverbWetLimiterCeiling,
              "threshold must stay strictly below ceiling; a negative headroom turns "
              "DesiredMagnitude into an exponential amplifier -- see dsp/Limiter.hpp");
inline constexpr float kReverbWetLimiterAttackSeconds = 2.0e-6f;   // 2 microseconds -- see comment above.
inline constexpr float kReverbWetLimiterReleaseSeconds = kSharedReleaseSeconds;  // shared; see Limiter.hpp.

// Reverb slots 10-13 ("TkDv"/"Grit"/"Tilt"/"Tund"), plus the rate half of
// the collapsed Mod control ("MdRt", no live slot of its own any more --
// see modLfoHz's own comment in Process(), below): these five parameters
// were registered (FroggersParameters.hpp) but never read -- every knob
// defaulted 0.0f and had no effect. No Froggers original exists for any of
// them (same footing as Mod depth/Hold above -- a "newly
// authored" category), so each mapping below is authored, with its own
// derivation noted at its call site/setter.
struct Reverb
{
    // 08b5fd3:src/core/FroggersEngine.hpp:65 (x_rvSize).
    static constexpr size_t kSize = 4096;

    // Authored Mod depth constants (no equivalent to cite) -- a slow
    // wow on the tank read-taps, deliberately small (well under one sample
    // at typical knob values) so it colors the tail without destabilising
    // the delay-line indexing.
    static constexpr float kModMaxOffsetSamples = 24.0f;

    // Range [0.07, 1.75] Hz for the LFO underneath Mod depth's wow: the
    // geometric mean lands exactly on 0.35 Hz (0.07*1.75 == 0.1225 ==
    // 0.35^2), the SAME "geometric-mean range" idiom Delay's own Mod rate
    // knob already established (dsp::StereoDelay::SetModRate, dsp/
    // Delay.hpp: range [0.05, 1.25], 25x ratio, geometric mean 0.25 Hz) --
    // reused directly, including the 25x lo/hi ratio (0.07*25 == 1.75),
    // not invented fresh. Mod depth and Mod rate collapsed to one control
    // (see modLfoHz's own comment in Process(), below) -- this range is
    // now read at a fixed knob of 0.5f, ExpMapCompute(0.07,1.75,0.5) ==
    // sqrt(0.07*1.75) == sqrt(0.1225) == 0.35 Hz, rather than swept live.
    static constexpr float kModLfoHzMin = 0.07f;
    static constexpr float kModLfoHzMax = 1.75f;

    // (slot 10, "Tank drive" / "TkDv"): SAME [0.25, 4.0] ExpMapCompute
    // range and knob-0.5-is-unity convention already established for a
    // "drive" pre-gain into a saturator elsewhere (Delay's
    // Feedback drive, dsp/Delay.hpp SetFeedbackDrive; Filter's Comb drive,
    // FroggersAppCore.hpp) -- reused, not reinvented. Default knob
    // 0.5f reproduces unity (1.0f) exactly: ExpMapCompute(0.25,4,0.5) ==
    // 0.25*sqrt(16) == 1.0.
    static constexpr float kTankDriveMin = 0.25f;
    static constexpr float kTankDriveMax = 4.0f;

    // (slot 13, "Tuned" / "Tund"): a static (non-LFO) offset on dA/dB,
    // fed through the SAME offsetSamples/applyMod mechanism Mod depth's
    // LFO wow already uses below -- there is no pitch tracker on this
    // control: it is an ordinary parameter, not a tracked oscillator pitch.
    // Range +-300 samples
    // (chosen and MEASURED for stability under rapid sweeps). Default knob 0.5f reproduces an exact zero offset
    // ((2*0.5-1)*300 == 0), i.e. dA/dB unchanged from what Room size alone
    // produces today.
    static constexpr float kTunedMaxOffsetSamples = 300.0f;

    // (slot 12, "Tilt" / "Tilt"): bipolar post-tank tone shave, crossfaded
    // around the knob's centre (0.5f) between a direct lowpass tap and its
    // complementary highpass (input - lowpass(input)). kTiltCrossoverHz is
    // the FIXED corner both taps share (the knob controls only the
    // low/high BALANCE, never the corner itself); kTiltDepth is the
    // maximum weight applied to the low/high difference at either extreme.
    // Both values were chosen, then the wetLimiter's existing tuning was
    // RE-MEASURED against this stage at its brightest setting to confirm
    // it still holds -- comparing peak levels at centre versus brightest,
    // and including a sabotage check proving the measurement could see a
    // change at all, so a null result meant the tuning held rather than
    // that the instrument was blind.
    static constexpr float kTiltCrossoverHz = 1000.0f;
    static constexpr float kTiltDepth = 1.0f;

    float lineA[kSize]{};
    float lineB[kSize]{};
    float preLine[kSize]{};
    size_t indexA = 0;
    size_t indexB = 0;
    size_t preIndex = 0;

    // 08b5fd3:src/core/FroggersEngine.hpp:69, shared between the A and B taps just
    // as the single m_rvDampFilter instance is (ported faithfully,
    // including the shared-state quirk of filtering A then B in sequence
    // through the same one-pole state each sample).
    OnePoleLowPass dampFilter;

    // (slot 12, "Tilt"): twin OnePoleLowPass instances sharing the same
    // fixed corner (kTiltCrossoverHz, recomputed from `sampleRate` every
    // Process() call, same "recompute fresh, sampleRate is a per-call
    // argument here" idiom modLfoPhase's own increment below already uses)
    // -- tiltLowPass provides the direct lowpass tap, tiltHighPass provides
    // the complementary highpass tap via `input - tiltHighPass.Process(input)`
    // in Process() below. Two separate instances (not one reused twice)
    // because each carries its own persistent one-pole state; reusing a
    // single instance for both taps would advance that state twice per
    // sample and desync the two reads.
    OnePoleLowPass tiltLowPass;
    OnePoleLowPass tiltHighPass;
    // The tilt is the one post-mix stage that genuinely doubles for stereo:
    // it is a filter, so it carries per-channel recursive state and cannot be
    // shared the way the limiter's linked envelope can. Two extra one-poles
    // per sample, and the honest reason the whole stereo path is not free.
    OnePoleLowPass tiltLowPassR;
    OnePoleLowPass tiltHighPassR;

    float wetL = 0.0f;
    float wetR = 0.0f;

    // Authored Mod depth's own LFO phase (no equivalent).
    float modLfoPhase = 0.0f;

    // The stage's own output limiter, applied to what Process() (below)
    // returns -- after both tanks and the dry/wet mix, never inside the
    // feedback loop. See this file's header comment (above the struct) for
    // the tuning and its measurement.
    OutputLimiter wetLimiter;

    // `WetAuthorityFollower` (Limiter.hpp) is the same unit
    // `dsp::StereoDelay::wetAuthority` owns (dsp/Delay.hpp) -- one shared
    // definition of the attack/release coefficients and full-authority
    // level, so Reverb's Send gets exactly Delay's protection against
    // crossfading the dry signal away against a wet path nothing feeds,
    // rather than a second copy that happens to agree.
    //
    // Reverb now has its own Send (`sendKnob01`, Process() below): the tank
    // is fed `send * input`, exactly the way `StereoDelay::Process` feeds its
    // line `inSignal = bumpIn * send` (dsp/Delay.hpp), so this follower
    // starts at 0.0f -- the same "a just-cleared wet path has earned
    // nothing" default `WetAuthorityFollower::Reset()` documents -- and
    // tracks what the tank is actually producing from here on.
    WetAuthorityFollower wetAuthority;

    // Unlike `dsp::StereoDelay` (which is always `SetSampleRate()`'d
    // before any `Process()` call, dsp/Delay.hpp), `Reverb` has no such
    // entry point of its own -- every caller passes `sampleRate` directly
    // into `Process()` each call instead, and several existing tests
    // default-construct a `dsp::Reverb` and call `Process()` immediately
    // with no configuration step at all. Mirrors `FilterFxChain`'s own
    // constructor (dsp/FilterFx.hpp): pre-configure `wetLimiter` at an
    // assumed 48kHz here so a bare `dsp::Reverb rv;` still gets this stage's
    // intended threshold/attack/release rather than `OutputLimiter`'s
    // zero-initialized attack/release coefficients (unconfigured, `Process()`
    // would apply no smoothing at all). `Configure()` below re-configures at
    // the real sample rate once FroggersAppCore::PrepareToPlay() calls it
    // (mirrors `filterChain_.Configure(sampleRate_)`/`delay_.SetSampleRate(
    // sampleRate_)` there).
    Reverb()
    {
        constexpr float kDefaultAssumedSampleRate = 48000.0f;
        Configure(kDefaultAssumedSampleRate);
    }

    // Sample-rate-dependent configuration for `wetLimiter`, separate
    // from the constructor above for the identical reason `FilterFxChain::
    // Configure()` is (dsp/FilterFx.hpp) -- the real sample rate is only
    // known once FroggersAppCore::PrepareToPlay() runs.
    void Configure(float sampleRate)
    {
        wetLimiter.Configure(sampleRate, kReverbWetLimiterThreshold, kReverbWetLimiterCeiling,
                              kReverbWetLimiterAttackSeconds, kReverbWetLimiterReleaseSeconds);
        // Same "configure before any Process() call" rule wetLimiter's own
        // line just above follows -- see wetAuthority's own field comment
        // for what these coefficients now govern.
        wetAuthority.Configure(sampleRate);
    }

    // (Stop-transport reset, app/FroggersAppCore.hpp's ProcessBlock
    // running->stopped edge): zero every member that carries signal energy
    // between calls to Process() -- the recursive comb-ish tank (lineA/
    // lineB), the pre-delay line, all three ring indices, the shared
    // damping filter's one-pole state (dampFilter.output; its `alpha`
    // coefficient is recomputed from dampKnob01 every Process() call, so
    // leaving it untouched is correct -- this clears state, it does not
    // reconfigure), and the last computed wet outputs. Decay (up to 0.98)
    // and Hold (`fb` approaching but never reaching 1.0) make this
    // tank self-sustaining on its own, so without this the reverb keeps
    // ringing after the operator stops the transport.
    //
    // modLfoPhase: reset to 0 too, even though it carries no *signal*
    // energy (it only offsets which sample of the already-zeroed lineA/
    // lineB gets read back via `Sine01(modLfoPhase)` in Process() below) --
    // silence after Reset() does not depend on its value. It is zeroed anyway so a
    // Reset()'d Reverb is in a single deterministic state regardless of how
    // long the instrument had been running before Stop, rather than
    // carrying over an arbitrary phase from the previous run.
    void Reset()
    {
        std::fill(lineA, lineA + kSize, 0.0f);
        std::fill(lineB, lineB + kSize, 0.0f);
        std::fill(preLine, preLine + kSize, 0.0f);
        indexA = 0;
        indexB = 0;
        preIndex = 0;
        dampFilter.output = 0.0f;
        // `tiltLowPass`/`tiltHighPass` carry their own recursive
        // one-pole state, same "must reset them too" rationale as
        // `dampFilter.output` just above -- new state sitting downstream of
        // the tank this clear is meant to silence.
        tiltLowPass.output = 0.0f;
        tiltHighPass.output = 0.0f;
        tiltLowPassR.output = 0.0f;
        tiltHighPassR.output = 0.0f;
        wetL = 0.0f;
        wetR = 0.0f;
        modLfoPhase = 0.0f;
        // `wetLimiter` carries its own per-sample `envelope` state, so
        // a buffer clear -- Stop-transport reset or Tier 1 fault recovery,
        // both routed through this same Reset() -- resets it too, the same
        // treatment `StereoDelay::wetLimiterL`/`R` gets (dsp/Delay.hpp).
        wetLimiter.Reset();
        // Same "must reset them too" rule the wetLimiter line just above
        // already follows -- `WetAuthorityFollower::Reset()`'s own 0.0f
        // default is correct here: a just-cleared tank has earned nothing,
        // the same starting point `StereoDelay::wetAuthority` resets to.
        wetAuthority.Reset();
    }

    // (Tier 1 recovery, app/FroggersAppCore.hpp): Reverb has NO
    // gate/bypass -- Process() unconditionally writes into lineA/lineB/
    // preLine and unconditionally feeds `input` through dampFilter's shared
    // one-pole state every call, regardless of any parameter -- so a single
    // non-finite sample arriving from an upstream fault propagates into this
    // unit's own state (first the pre-delay tap, typically within a handful
    // of samples at default settings, then the room delay taps once the
    // tainted ring slot is eventually read back) and, once dampFilter.output
    // itself goes non-finite, poisons every future sample this Reverb ever
    // produces -- permanently, since nothing else clears it. This is exactly
    // the "audio never comes back" failure mode Tier 1 exists to fix, and
    // Reverb is just as exposed to it as any other unit under Tier 1
    // recovery; reuses this existing Reset() (the Stop-transport reset
    // above) rather than adding a duplicate.
    bool StateFinite() const
    {
        if (!std::isfinite(dampFilter.output) || !std::isfinite(wetL) || !std::isfinite(wetR) ||
            !std::isfinite(modLfoPhase) || !std::isfinite(tiltLowPass.output) || !std::isfinite(tiltHighPass.output) ||
            !std::isfinite(tiltLowPassR.output) || !std::isfinite(tiltHighPassR.output))
        {
            return false;
        }
        // Fold `wetLimiter`'s own finiteness into this unit's
        // aggregate -- `RecoverIfNonFinite(reverb_)` (FroggersAppCore.hpp)
        // calls this StateFinite()/the Reset() above uniformly, so a
        // poisoned limiter envelope must be visible here.
        if (!wetLimiter.StateFinite())
        {
            return false;
        }
        // Same "aggregate finiteness" rule the wetLimiter check just above
        // already follows -- the wet-authority follower's own level must be
        // visible here too.
        if (!wetAuthority.StateFinite())
        {
            return false;
        }
        for (size_t i = 0; i < kSize; ++i)
        {
            if (!std::isfinite(lineA[i]) || !std::isfinite(lineB[i]) || !std::isfinite(preLine[i]))
            {
                return false;
            }
        }
        return true;
    }

    // Read-only diagnostic,
    // NOT wired into RecoverPoisonedUnitState's Tier 2, same reasoning as
    // dsp::StereoDelay::StateMagnitude()'s own comment (BIBO-stable
    // feedback loop, legitimately large-but-finite under sustained loud
    // input -- Tier 2's ceiling would misfire here). Exists so the
    // Stop-flush measurement harness can tell "cleared once and stayed
    // clear" apart from "cleared once, then refilled."
    float StateMagnitude() const
    {
        float magnitude = std::max({std::fabs(dampFilter.output), std::fabs(wetL), std::fabs(wetR),
                                     std::fabs(tiltLowPass.output), std::fabs(tiltHighPass.output),
                                     std::fabs(tiltLowPassR.output), std::fabs(tiltHighPassR.output)});
        for (size_t i = 0; i < kSize; ++i)
        {
            magnitude = std::max({magnitude, std::fabs(lineA[i]), std::fabs(lineB[i]), std::fabs(preLine[i])});
        }
        return magnitude;
    }

    // -- Ported formula helpers, exposed statically for direct pinning
    // (mirrors dsp::Vco's PitchToPhaseIncrement/PmDepthScale precedent) --

    // :456 Room size.
    static float RoomSizeFromKnob(float knob01) { return ExpMapCompute(0.05f, 1.0f, knob01); }

    // :457 Decay.
    static float DecayFeedbackFromKnob(float knob01) { return ExpMapCompute(0.1f, 0.98f, knob01); }

    // :458 Pre-delay (see file-header note on the /sr, *sr round trip that
    // used to leave this a samples range dressed up as a time computation).
    // Floor is 1ms -- the port's own "1.0" literal, freed of the erroneous
    // /sr that turned it into one sample. Ceiling is derived from what the
    // pre-delay line (`preLine[kSize]`) can actually hold: `kSize - 1`
    // samples at whatever sampleRate this call runs at, converted to
    // milliseconds, so the knob's top position lands exactly on the
    // buffer's own capacity rather than on a guessed number -- at 48kHz
    // that ceiling is 4095/48000*1000 = 85.3125ms. Process()'s own
    // `preDelay >= kSize` clamp (immediately below) still guards the
    // rounding at the top of the sweep, but never needs to engage.
    static float PreDelayNormFromKnob(float knob01, float sampleRate)
    {
        constexpr float kPreDelayFloorMs = 1.0f;
        const float ceilingMs = 1000.0f * static_cast<float>(kSize - 1) / sampleRate;
        return ExpMapCompute(kPreDelayFloorMs / 1000.0f, ceilingMs / 1000.0f, knob01);
    }

    // :459, :574 Damping -- the ExpMap output IS the damping filter's alpha.
    // The floor is 0.02, not the 0.001 this was ported with. Alpha IS the
    // one-pole's coefficient and a smaller alpha is a darker tail, so 0.001
    // is a damping cutoff near 8 Hz at 48kHz -- a tail with nothing audible
    // left in it. That matters because randomization draws every parameter
    // uniformly across its travel while this mapping is geometric, so half of
    // all draws land below the range's geometric mean: sqrt(0.001 * 0.2) =
    // 0.0141, about 108 Hz. Half of every randomized reverb was mud. A floor
    // of 0.02 makes the range one decade, whose geometric mean is 0.0632 --
    // about 500 Hz -- and whose darkest setting is about 154 Hz. The knob
    // keeps its full travel and its direction; only what the dark end maps
    // onto moves.
    static float DampAlphaFromKnob(float knob01) { return ExpMapCompute(0.02f, 0.2f, 1.0f - knob01); }

    // No live slot any more -- Mod rate collapsed into the single Mod
    // control (slot 8) alongside Mod depth; this is now only ever called
    // at the fixed knob 0.5f (see modLfoHz's own comment in Process(),
    // below). See kModLfoHzMin/Max's own comment above.
    static float ModRateHzFromKnob(float knob01) { return ExpMapCompute(kModLfoHzMin, kModLfoHzMax, knob01); }

    // (slot 10, Tank drive) -- see kTankDriveMin/Max's own comment above.
    static float TankDriveFromKnob(float knob01) { return ExpMapCompute(kTankDriveMin, kTankDriveMax, knob01); }

    // (slot 13, Tuned) -- see kTunedMaxOffsetSamples's own comment above.
    // Identity-shaped around the centre (knob 0.5f -> 0 exactly), same
    // bipolar-around-centre idiom DriveBlendPhase's own aTarget mapping
    // uses (dsp/Drive.hpp), scaled to a sample-count range instead of an
    // allpass coefficient range.
    static float TunedOffsetSamplesFromKnob(float knob01)
    {
        return (2.0f * knob01 - 1.0f) * kTunedMaxOffsetSamples;
    }

    // Returns the fully mixed (dry/wet-blended) output, i.e. what
    // ApplyOutputFx's `(1.0f - rvMix) * output + rvMix * rvb` computes
    // (08b5fd3:src/core/FroggersEngine.hpp:847), folding the Wet/dry knob (:455) in here
    // since this is the port's only consumer of that ported parameter.
    StereoSample Process(StereoSample input,
                   float mixKnob01,
                   float sizeKnob01,
                   float decayKnob01,
                   float preKnob01,
                   float dampKnob01,
                   float widthKnob01,
                   float diffusionKnob01,
                   float sampleRate,
                   float modDepthKnob01 = 0.0f,
                   float holdKnob01 = 0.0f,
                   // Every
                   // default below reproduces today's exact prior
                   // behavior bit-for-bit when a caller omits them (see
                   // each default's own derivation at its constant's
                   // comment above) -- so every existing call site in this
                   // file's own tests, which never pass these six, is
                   // unaffected.
                   //
                   // modRateKnob01 is kept in the signature so every
                   // existing call site still compiles and every existing
                   // test still passes, but Mod depth and Mod rate now
                   // collapse to the ONE Mod control (modDepthKnob01) --
                   // see modLfoHz's own comment below for why and for the
                   // measurement that chose the fixed rate.
                   [[maybe_unused]] float modRateKnob01 = 0.5f,
                   float tankDriveKnob01 = 0.5f,
                   float gritKnob01 = 0.0f,
                   float tiltKnob01 = 0.5f,
                   float tunedKnob01 = 0.5f,
                   // Reverb slot 1 ("Send"): gates how much of `input`
                   // reaches the tank feed below, the same job
                   // `StereoDelay::Process`'s own `send` plays
                   // (`inSignal = bumpIn * send`, dsp/Delay.hpp). Default
                   // 1.0f reproduces today's exact prior behavior -- the
                   // tank fully fed, bit-for-bit -- for every existing call
                   // site in this file's own tests, which never pass it.
                   float sendKnob01 = 1.0f)
    {
        // :458, :497-504 -- pre-delay tap.
        const float preNorm = PreDelayNormFromKnob(preKnob01, sampleRate);
        size_t preDelay = static_cast<size_t>(std::round(preNorm * sampleRate));
        if (preDelay >= kSize)
        {
            preDelay = kSize - 1;
        }
        const float send = std::min(std::max(sendKnob01, 0.0f), 1.0f);
        // The tank's feed is mono: it is one pre-delay line into a
        // two-line network, and giving it two inputs would be a different
        // reverb, not the same one plumbed through. Only the feed folds --
        // the dry path below keeps its pair, and the tank's own output is
        // already stereo. Scaled by Send, the page's own feed into the
        // tank -- at Send's default-closed 0.0f this write is exactly 0.0f
        // every call, the same "no signal in the tank" starting point
        // `StereoDelay`'s own closed Send leaves its delay line at.
        preLine[preIndex] = send * 0.5f * (input.l + input.r);
        const size_t preRead = (preIndex + kSize - preDelay) % kSize;
        const float preOut = preLine[preRead];
        preIndex = (preIndex + 1) % kSize;

        // :456, :506-512 -- room size sets both tank delay lengths.
        const float sizeNorm = RoomSizeFromKnob(sizeKnob01);
        size_t baseA = static_cast<size_t>(180.0f + sizeNorm * 1300.0f);
        size_t baseB = static_cast<size_t>(260.0f + sizeNorm * 1800.0f);
        size_t dA = std::min(kSize - 1, std::max(static_cast<size_t>(1), baseA));
        size_t dB = std::min(kSize - 1, std::max(static_cast<size_t>(1), baseB));

        // Authored Mod depth: a small sinusoidal wow on the read taps.
        // modDepthKnob01 == 0 -> modOffset == 0 -> dA/dB unchanged, so this
        // never disturbs the parity case.
        //
        // Mod rate no longer has its own knob: it was measurably inert
        // whenever Mod depth sat at zero (the multiply just below), so the
        // two controls served one capability. MEASURED (fully wet, a
        // steady 300 Hz tone, 90 seconds rendered per point so even the
        // slowest rate completes several cycles, the tail's short-time RMS
        // envelope as the sideband proxy): envelope DEPTH tracks
        // modDepthKnob01 and stays within 0.02 dB of its own row's mean
        // across every rate (measured rows at depth 0/0.25/0.5/0.75/1.0:
        // 0.000/0.129/0.191/0.222/0.242 dB), while the envelope's energy
        // AT the rate-predicted frequency stays within 0.3 dB of its own
        // column's mean across every depth once depth is nonzero -- the
        // two axes are close to orthogonal. A single depth knob at a
        // FIXED rate therefore reaches nearly the full depth range the
        // two-knob grid reaches (0.000-0.240 dB against the grid's own
        // 0.000-0.263 dB ceiling), while tying rate to depth instead (the
        // other candidate law) forces every shallow setting to also be
        // slow and every deep setting to also be fast, losing the
        // deep-but-slow and shallow-but-fast corners today's two knobs
        // reach together. Fixed at ModRateHzFromKnob(0.5f) -- 0.35 Hz, the
        // geometric mean of the range and already the exact value the
        // ported parity case assumes, so modRateKnob01's own default
        // continues to reproduce it, now unconditionally.
        const float modLfoHz = ModRateHzFromKnob(0.5f);
        modLfoPhase = WrapPhase(modLfoPhase + modLfoHz / sampleRate);
        const float modOffset = modDepthKnob01 * kModMaxOffsetSamples * Sine01(modLfoPhase);
        // Tuned adds a static (non-LFO) offset through this SAME
        // offsetSamples/applyMod mechanism -- tunedKnob01 == 0.5f reproduces
        // exactly a zero offset (see kTunedMaxOffsetSamples's own comment),
        // so dA/dB are unchanged from today's Room-size-only result at the
        // default, exactly like modOffset==0 above.
        const float tunedOffset = TunedOffsetSamplesFromKnob(tunedKnob01);
        const long offsetSamples = std::lround(modOffset + tunedOffset);
        const auto applyMod = [&](size_t baseDelay) -> size_t {
            long modulated = static_cast<long>(baseDelay) + offsetSamples;
            modulated = std::max<long>(1, std::min<long>(static_cast<long>(kSize) - 1, modulated));
            return static_cast<size_t>(modulated);
        };
        dA = applyMod(dA);
        dB = applyMod(dB);

        const size_t readA = (indexA + kSize - dA) % kSize;
        const size_t readB = (indexB + kSize - dB) % kSize;

        const float valA = lineA[readA];
        const float valB = lineB[readB];

        // :457, :514-515 -- decay/feedback, folded with authored Hold.
        // holdKnob01 == 0 -> fb == decayFb exactly (parity default). Hold
        // is clamped strictly below 1.0 so the tail lengthens without ever
        // reaching true self-oscillation (bounded/finite requirement).
        const float decayFb = DecayFeedbackFromKnob(decayKnob01);
        const float fb = decayFb + (1.0f - decayFb) * std::min(holdKnob01, 0.999f);

        const float diffusion = diffusionKnob01;  // :461, direct passthrough
        const float cross = diffusion * 0.5f;
        const float aFb = valB * (1.0f - cross) + valA * cross;
        const float bFb = valA * (1.0f - cross) + valB * cross;

        // The reverb tank has an in-loop saturator, the same one used
        // elsewhere in this codebase, for consistency.
        // Mirrors the same fix in dsp/Delay.hpp (StereoDelay::Process's own comment)
        // exactly, same reasoning, same fix shape: `aFb`/`bFb` above are
        // unbounded reads straight off lineA/lineB (this tank's own cross-fed
        // taps), so the pre-fix `preOut + aFb * fb` fed a linear, unsaturated
        // loop -- steady state `preOut / (1 - fb)`, and with Hold maxed `fb`
        // (computed just above) -> ~0.99998, i.e. ~50,000x. Wraps each fed-back tap in
        // the SAME `PadeSaturator::Saturate` the comb's own loop
        // (FilterFx.hpp's Comb::Process) and the delay's own fix already use
        // -- reused, not reimplemented -- applied BEFORE the `fb`
        // multiply, exactly mirroring that fix's `fbk * PadeSaturator::Saturate(fbL)`.
        // `Saturate` clamps to +-1 unconditionally, so every write to
        // lineA/lineB is now bounded by `|preOut| + fb` regardless of how
        // many round trips have already run -- a per-sample bound, not
        // merely a steady-state one.
        //
        // Hold's PERSISTENCE is untouched: PadeSaturator's small-signal gain
        // at x=0 is exactly 1.0 (FilterFx.hpp's own comment on the comb's
        // saturator, "the derivative at x=0 ... = 27/27 = 1"), so once a
        // tap's magnitude decays under the saturator's linear region the
        // fed-back term is again Saturate(x) ~= x and the tail keeps decaying
        // at the same fb-per-round-trip rate as before this fix -- this adds
        // a MAGNITUDE ceiling on a hot tank, not a change to the decay time
        // constant at ordinary levels. Same "bound what can blow up, don't
        // touch what legitimately persists" split this file's own wetLimiter
        // already keeps relative to `fb`'s own computation just above (also
        // untouched by that fix) -- this is that same split, one loop
        // further in: a structural guard, not a tone change.
        //
        // (slot 11, Grit): routes aFb/bFb through dsp::DigitalReorganizer
        // (Drive.hpp), reused AS-IS -- not hand-rolled -- specifically so
        // its own DC-blocked Process() (Mangle(input,·,·) - Mangle(0,·,·))
        // is what runs here, not a copy that would reintroduce the f(0)!=0
        // defect that DC-block construction exists to fix (see Drive.hpp's
        // own divergence-note comment). Local instance (no persistent
        // signal state of its own -- flip/hashBits are config, reassigned
        // fresh every call from gritKnob01, same idiom dampFilter.alpha
        // uses above). gritKnob01 == 0.0f -> flip == 0, hashBits == 0 ->
        // Mangle(x,0,0) - Mangle(0,0,0) == x - 0 == x exactly (Drive.hpp's
        // own Mangle formula reduces to the identity at flip==hashBits==0),
        // an EXACT bit-identical bypass, not merely a small value -- the
        // aFb/bFb signals pass through this stage unchanged at default.
        DigitalReorganizer gritReorganizer;
        gritReorganizer.SetFlip(gritKnob01);
        gritReorganizer.SetHash(gritKnob01);
        const float aFbGrit = gritReorganizer.Process(aFb);
        const float bFbGrit = gritReorganizer.Process(bFb);
        // (slot 10, Tank drive): pre-gain on the ARGUMENT of Saturate
        // ONLY -- Saturate's
        // own +-1 clamp still bounds this line to `|preOut| + fb` regardless
        // of tankDrive (writing `tankDrive * fb * Saturate(...)` instead
        // would raise that bound; deliberately not done, same reasoning
        // Delay's own Feedback-drive comment gives, dsp/Delay.hpp).
        // tankDriveKnob01 == 0.5f reproduces tankDrive == 1.0f exactly
        // (unity -- see kTankDriveMin/Max's own comment), so this alone
        // never disturbs the parity case either.
        const float tankDrive = TankDriveFromKnob(tankDriveKnob01);
        const float aIn = preOut + fb * PadeSaturator::Saturate(tankDrive * aFbGrit);
        const float bIn = preOut + fb * PadeSaturator::Saturate(tankDrive * bFbGrit);

        dampFilter.alpha = DampAlphaFromKnob(dampKnob01);  // :459, :574
        const float aOut = dampFilter.Process(valA);
        const float bOut = dampFilter.Process(valB);

        lineA[indexA] = aIn;
        lineB[indexB] = bIn;
        indexA = (indexA + 1) % kSize;
        indexB = (indexB + 1) % kSize;

        const float mid = 0.5f * (aOut + bOut);
        const float width = widthKnob01;  // :460, direct passthrough
        wetL = mid + width * (aOut - mid);
        wetR = mid + width * (bOut - mid);
        // wetL/wetR used to be summed here, which made the Width control above
        // mathematically inert: with mid == 0.5(aOut+bOut), wetL + wetR is
        // 2*mid at every width, so the knob could not change what was heard.
        // Keeping the pair is what makes it a control.
        // Scaled by what the wet path has earned, the same protection
        // `StereoDelay::ToStereo` already applies (dsp/Delay.hpp). The
        // ADVANCE TARGET mirrors `StereoDelay::Process`'s own two exits: a
        // closed Send drops the target to 0.0f at once -- Wet/dry goes
        // inert immediately rather than fading out with whatever the tank
        // still holds on Hold/Decay's own feedback -- and an open Send
        // tracks the level the tank is actually producing (`wetL`/`wetR`
        // just above), so a Send that has not yet earned the mix its full
        // travel does not get it.
        const float wetAuthorityTarget = (send <= 0.0001f) ? 0.0f : std::fabs(0.5f * (wetL + wetR));
        wetAuthority.Advance(wetAuthorityTarget);
        const float mix = mixKnob01 * wetAuthority.Authority();  // :455, direct passthrough
        // Equal-power crossfade, not linear: :846's `(1-mix)*dry + mix*wet`
        // only holds level when the two legs are correlated, and the tank's
        // tail is largely uncorrelated with the dry input. MEASURED worst
        // dip across the control's whole travel under the old linear law:
        // -1.55 dB at 220 Hz, -2.06 dB at 440 Hz, -4.47 dB at 880 Hz.
        // cos/sin quadrant weights hold level regardless of that
        // correlation, the same fix dsp::DriveBlendPhase::Process
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
        // return `input` bit-for-bit here and an existing pin depends on it.
        const WetDryGains gains = EqualPowerWetDry(mix, kMinDryLevel);
        const float dryGain = gains.dry;
        const float wetGain = gains.wet;
        const float mixedL = dryGain * input.l + wetGain * wetL;
        const float mixedR = dryGain * input.r + wetGain * wetR;

        // (slot 12, Tilt): bipolar post-tank tone shave, applied to
        // mixedOut BEFORE wetLimiter.Process() below. tiltLowPass/
        // tiltHighPass share the same fixed corner (kTiltCrossoverHz,
        // recomputed from `sampleRate` every call, same per-call-argument
        // idiom modLfoHz's own increment above uses) -- tiltLow is the
        // direct lowpass tap, tiltHigh is its complement
        // (`mixedOut - tiltHighPass.Process(mixedOut)`). tiltAmount is 0.0f
        // EXACTLY at tiltKnob01 == 0.5f (centre), so `tiltAmount * (...)`
        // is an exact 0.0f multiply and `tilted` equals `mixedOut` bit-for-
        // bit at the default -- both filters still run every call (so their
        // state stays live for a knob move mid-tail), but their output is
        // provably unreachable at centre, the same "still computed, exact
        // no-op by a zero multiply" idiom modOffset/tunedOffset above use.
        tiltLowPass.SetAlphaFromNatFreq(kTiltCrossoverHz / sampleRate);
        tiltHighPass.SetAlphaFromNatFreq(kTiltCrossoverHz / sampleRate);
        tiltLowPassR.SetAlphaFromNatFreq(kTiltCrossoverHz / sampleRate);
        tiltHighPassR.SetAlphaFromNatFreq(kTiltCrossoverHz / sampleRate);
        const float tiltAmount = tiltKnob01 - 0.5f;  // -0.5 (darkest) .. 0 (centre) .. 0.5 (brightest)
        const float tiltLowL = tiltLowPass.Process(mixedL);
        const float tiltHighL = mixedL - tiltHighPass.Process(mixedL);
        const float tiltedL = mixedL + tiltAmount * (tiltHighL - tiltLowL) * kTiltDepth;
        const float tiltLowR = tiltLowPassR.Process(mixedR);
        const float tiltHighR = mixedR - tiltHighPassR.Process(mixedR);
        const float tiltedR = mixedR + tiltAmount * (tiltHighR - tiltLowR) * kTiltDepth;

        // Applied to the fully mixed dry/wet output, AFTER both
        // tanks (`lineA`/`lineB` already written above) and AFTER the mix
        // -- `fb`/Hold's own computation (`const float fb = ...` above) is
        // untouched, so Hold keeps sustaining exactly as before; this only bounds the
        // LEVEL that escapes this stage toward the master limiter. See this
        // file's header comment (above the struct) for the tuning and its
        // measurement. Applied to `tilted` (== `mixedOut` exactly
        // at Tilt's centre default) rather than `mixedOut` directly --
        // MEASURED against the brightest Tilt setting.
        return wetLimiter.Process(StereoSample{tiltedL, tiltedR});
    }
};

}  // namespace synth_froggers::dsp
