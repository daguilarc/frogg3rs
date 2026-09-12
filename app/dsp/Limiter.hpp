#pragma once

// synth_froggers::dsp::OutputLimiter -- extracted out of app/FroggersAppCore.hpp
// (where it was a PRIVATE nested type) so a SECOND, independently-tuned
// instance can run on the Filter bank's peak branch (FilterFxChain,
// dsp/FilterFx.hpp) without duplicating the struct.
//
// WHY THIS FILE, NOT dsp/FilterFx.hpp AND NOT FroggersAppCore.hpp:
// `FroggersAppCore.hpp` includes `dsp/FilterFx.hpp` (never the reverse), so
// a peak-branch limiter living in `FilterFxChain` cannot reach UP into
// FroggersAppCore.hpp for the type without a circular include. It has to
// move DOWN into dsp/. A small dedicated header (rather than folding it
// into FilterFx.hpp beside PadeSaturator) keeps a general-purpose dynamics
// processor -- attack/release envelope follower, not a filter/saturator --
// out of a file whose own header comment scopes it to "comb/peak/scoop
// routing". `FilterFx.hpp` includes this header below; `FroggersAppCore.hpp`
// uses `dsp::OutputLimiter` transitively through that include (and directly,
// for its own `outputLimiter_` member).
//
// CRITICAL constraint: the MASTER output limiter's behaviour must
// not change by one sample. Before this move, `kThreshold` (0.9) /
// `kCeiling` (1.0) / `kHeadroom` / `kAttackSeconds` (1ms) / `kReleaseSeconds`
// (100ms) were `static constexpr`, so every instance of the type shared one
// tuning -- exactly why a second instance (the peak branch's own limiter)
// could not just be dropped in verbatim; it would duck identically to the
// master and be useless there.
// Converted to instance fields here. The single-argument `Configure(
// sampleRate)` overload below reproduces the master's ORIGINAL tuning via
// the exact same formula, same operand order, same float literals, so
// FroggersAppCore.hpp's one pre-existing call site
// (`outputLimiter_.Configure(sampleRate_)`, PrepareToPlay()) is textually
// unchanged and bit-identical to before this extraction -- the existing
// master-limiter tests (FroggersAudioRoutingTests.cpp) are the proof.

#include "DspMath.hpp"

#include <algorithm>
#include <cmath>

namespace synth_froggers::dsp {

// Shared across EVERY per-stage limiter instance (peak branch, delay wet,
// reverb wet, and the master). These two values are identical at all four
// sites by design, not by coincidence, so they live here once rather than
// being re-declared per stage -- they were duplicated 4x and 3x
// respectively before this consolidation.
//
// What is DELIBERATELY NOT shared: each stage's `threshold` and
// `attackSeconds`. Both are MEASURED per stage and legitimately differ --
// peak 0.7/5us (single-sample transients from stored biquad energy), delay
// and reverb 0.9/2us (fast onset at the short-round-trip extreme), master
// 0.9/1ms (sustained material only). Inferring attack from mechanism shape
// was wrong twice: it is per-stage evidence, not a shared constant, and
// must not be folded in here.
//
// `kSharedCeiling` is full scale everywhere: a limiter's ceiling is what it
// must never let through, and that is 1.0 for every stage regardless of
// where the stage sits. This stays the MASTER's ceiling only
// (dsp::OutputLimiter::kDefaultCeiling) -- the master is deliberately
// unchanged by the retarget below.
inline constexpr float kSharedCeiling = 1.0f;
// The ceiling every NON-master per-stage limiter (peak, delay wet,
// reverb wet, Drive's output limiter) budgets to. Before this landed, every
// per-stage limiter shipped `ceiling = kSharedCeiling = 1.0` while the
// master's own threshold sits at 0.9, so a correctly-clamped stage could
// still legitimately deliver above the level the master starts working at
// -- a post-RequestRandomizeAll() + Filter Crispy max repro measured the
// master's envelope duty cycle at 1.000 (it NEVER returned to unity across
// 256 blocks; min 0.9666, mean 0.9784, range 0.0245). Narrowing every
// per-stage budget to this ceiling is what the make-up gain
// (1/kStageCeiling, applied once in FroggersAppCore.hpp) restores the
// headroom for.
inline constexpr float kStageCeiling = 0.80f;
// `kSharedReleaseSeconds`: 100ms at all four sites, derived from the
// peak's measured residual decay (-60dB in 11.79ms at max Q, so ~8.5x
// margin) and confirmed correct for delay and reverb too; each
// stage's own comment already said "matches the master". One value.
inline constexpr float kSharedReleaseSeconds = 0.1f;

// Tuning for `WetAuthorityFollower` below, shared by `dsp::StereoDelay`
// (dsp/Delay.hpp) and `dsp::Reverb` (dsp/Reverb.hpp). Both stages crossfade a
// dry signal against a wet path that a Send control feeds -- Delay's already,
// Reverb's once a later change gives it one -- and Send defaults to zero, so
// the wet path can be silent while the mix knob sits at maximum; a crossfade
// against silence is a mute. Authority tracks what the wet path actually
// HOLDS, so a low-Send high-feedback patch -- a loud echo fed by very little
// -- still earns the control its full travel.
//
// Each stage's own wet limiter cannot serve this job: it is a gain multiplier
// that sits at exactly 1.0 for anything under threshold (Limiter.hpp), so a
// quiet echo and silence read identically to it.
//
// Release is kSharedReleaseSeconds, which both stages' own wet limiters
// ALREADY apply to this exact signal, so authority tracks at a rate this path
// is measured not to pump at -- a genuinely shared value, not an analogy.
//
// Attack is 10ms, the same figure VcoEnvelopeFollowers uses, and that IS an
// analogy: it has not been measured for either path. It is a deliberately
// unshared literal for that reason. Delay's own wet-limiter comment records
// why -- its tuning was measured rather than taken from the master limiter,
// because "analogy-picked constants have been measured wrong before in this
// codebase" -- and Limiter.hpp's own per-stage thresholds record the same
// judgement, which did not collapse either. Sharing one constant across every
// site would assert a derivation that only the VCO followers have. Rising
// quickly is the safe direction here, so being approximately right costs
// little; the honest note is that it is approximate.
inline constexpr float kWetAuthorityAttackSeconds = 0.010f;
inline constexpr float kWetAuthorityReleaseSeconds = kSharedReleaseSeconds;
// The level at or above which the wet path has earned the control its full
// travel. This is an AUDIBILITY threshold, not a loudness one: the failure
// being fixed is a crossfade against silence, so anything the operator can
// plainly hear should give the knob its whole range, and only a path holding
// essentially nothing should take it away.
//
// MEASURED, not chosen by eye, against Delay's own wet path (dsp/Delay.hpp).
// The wet limiter's own threshold (0.72) was tried first and is wrong for
// this: a frozen delay line ringing at peak 0.37 -- unmistakably audible, and
// exactly the loud-echo-fed-by-little case the scaling exists to serve --
// would get only half its travel there. This value sits an order of
// magnitude above the routing suite's own measured noise floor for a
// self-sustaining ring (0.0063), so a path holding only numerical residue
// still reads as empty.
inline constexpr float kWetAuthorityFullLevel = 0.05f;

// Shared by `dsp::StereoDelay::wetAuthority` and `dsp::Reverb::wetAuthority`:
// one follower tracking how much level a wet path actually holds, so a
// wet/dry mix control can be scaled by what it has actually earned instead of
// crossfading against a path that a closed Send leaves silent. `Advance()` is
// called once per processed sample with the level the wet path just produced
// (or a fixed target when a stage has no gate of its own yet -- see
// `dsp::Reverb`'s own comment on its instance); `Authority()` reads the
// result without advancing it, so a caller can use it from a `const` method
// (mirrors `StereoDelay::ToStereo`, which reads this every call but only
// `Process()` updates it).
struct WetAuthorityFollower
{
    float level = 0.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    // Sample-rate-dependent, recomputed wherever a real sample rate becomes
    // known -- same rationale as every other Configure()/SetSampleRate() in
    // this file.
    void Configure(float sampleRate)
    {
        attackCoeff = std::exp(-1.0f / (kWetAuthorityAttackSeconds * sampleRate));
        releaseCoeff = std::exp(-1.0f / (kWetAuthorityReleaseSeconds * sampleRate));
    }

    // One-pole toward the measured level, faster up than down (see the
    // authority constants above).
    void Advance(float target)
    {
        const float coeff = target > level ? attackCoeff : releaseCoeff;
        level = target + coeff * (level - target);
    }

    // How much of the mix control's travel the wet path has earned:
    // proportional to the level it holds, saturating where the path stops
    // getting louder.
    float Authority() const { return std::min(level / kWetAuthorityFullLevel, 1.0f); }

    // `startingLevel` defaults to 0.0f (Delay's own reset value -- a
    // just-cleared wet path has earned nothing). A stage with no gate of its
    // own yet passes a different starting level; see `dsp::Reverb::Reset()`.
    void Reset(float startingLevel = 0.0f) { level = startingLevel; }

    bool StateFinite() const { return std::isfinite(level); }
};

// The floor on dry signal shared by every wet/dry crossfade on the Delay and
// Reverb pages (`dsp::StereoDelay::ToStereo`, dsp/Delay.hpp;
// `dsp::Reverb::Process`'s `mixedL`/`mixedR`, dsp/Reverb.hpp). Both stages
// crossfade dry against wet with an EQUAL-POWER law -- `dry*cos(theta) +
// wet*sin(theta)`, the same law `dsp::DriveBlendPhase::Process` (Drive.hpp)
// already applies to the Drive page's own Wet/Dry -- and cap `theta` at
// `std::acos(kMinDryLevel)` rather than letting it reach a full quarter
// turn, so the dry signal's own gain never drops below this value no matter
// where the knob sits. This is a GAIN, not a share of a linear mix: dry and
// wet are two independent amplitude gains whose SQUARES sum to one
// (`kMinDryLevel*kMinDryLevel + sin(acos(kMinDryLevel))^2 == 1.0`, i.e.
// 0.300^2 + 0.954^2 == 1.0), constant power, not two mix shares adding to
// one.
//
// The equal-power law is why a wet control no longer loses level toward the
// wet end of its travel: against a wet path uncorrelated with dry, total
// output power holds at 1.0 across the WHOLE travel (dry^2 + wet^2 == 1.0 by
// construction, every theta), where the old linear crossfade (dry gain
// 1-mix, wet gain mix) sagged to dry^2+wet^2 == 0.3^2+0.7^2 == 0.58 at its
// own wettest setting -- about 2.4 dB quieter there than the dry input, with
// the wet leg itself only 0.700 rather than this law's 0.954. That sag, on a
// wet path largely uncorrelated with dry, is what read as the Delay and
// Reverb pages going quiet and noise-like at full wet (operator 2026-07-29
// "clamp the reverb wetness down, it's too fucking quiet", tightened again
// 2026-08-26): the linear law cancelled the dry signal away, leaving behind
// a quiet, uncorrelated wet path. Holding the dry floor at this same 0.300
// while fixing the law that was collapsing the wet leg's own level -- 0.954
// here versus 0.700 before -- addresses that report at its cause rather
// than only treating the symptom.
//
// The Drive page's own Wet/Dry carries no such floor -- it reaches fully
// wet -- because it is a distortion control an operator asks for by name,
// not a reverb or a delay that a full-wet setting would throw the
// instrument away for.
inline constexpr float kMinDryLevel = 0.30f;

// The equal-power wet/dry law itself, in one place. Every master mix on the
// instrument uses it -- `dsp::DriveBlendPhase::Process` (Drive.hpp),
// `dsp::StereoDelay::ToStereo` and `dsp::Reverb::Process` -- and before this
// was extracted each page carried its own copy, with Delay's and Reverb's
// byte-identical to each other. Three copies of one law is three places a
// later correction has to land, and the two floored pages had already drifted
// into citing each other's comments as if that made them shared.
//
// `minDryLevel` is what separates the pages, and it is the ONLY thing that
// does: at 0 the control reaches fully wet (the Drive page, where replacing
// the source is the point), and at kMinDryLevel it stops short so the dry
// signal survives (Delay and Reverb, per the operator ruling above).
//
// Callers pass the mix AFTER any authority scaling, since what a page has
// earned is that page's business and not this law's.
struct WetDryGains
{
    float dry;
    float wet;
};

inline WetDryGains EqualPowerWetDry(float mix01, float minDryLevel)
{
    // mix == 0 has to return the dry signal bit-for-bit; pins on all three
    // pages depend on it, and std::cos(0)/std::sin(0) landing on exactly
    // 1.0f/0.0f is checked rather than assumed by those pins.
    if (mix01 <= 0.0f) return WetDryGains{1.0f, 0.0f};

    constexpr float kHalfPi = 1.57079632679489661923f;
    // A floored page's thetaMax sits well short of pi/2, which is the only
    // place std::cos stops landing on an exact value. An unfloored page's
    // does not, so its top endpoint needs the caller's own exact case --
    // std::cos(pi/2) in float returns -4.37e-8, not 0, which would leak a
    // trace of dry into a nominally fully-wet output.
    const float thetaMax = (minDryLevel > 0.0f) ? std::acos(minDryLevel) : kHalfPi;
    const float theta = mix01 * thetaMax;
    return WetDryGains{std::cos(theta), std::sin(theta)};
}

// A second equal-power law, distinct from the one above: EqualPowerWetDry
// floors the DRY leg only, so an unfloored page still reaches a hard zero on
// one side. This one floors BOTH legs identically, for a blend where neither
// side may ever be multiplied by exactly zero -- `FilterFxChain::Process`'s
// Comb/Peak blend (dsp/FilterFx.hpp) and `FrogBlock::Process`'s Fold/Fuzz
// blend (dsp/Drive.hpp) both need this shape, and before this was extracted
// each carried its own copy of the identical floor/span/angle constants.
// Named for what both call sites do with it -- knob01 == 0 favours legA,
// knob01 == 1 favours legB, and neither ever reaches the other's gain of
// exactly 1.0 or 0.0.
struct FloorBlendGains
{
    float legA;
    float legB;
};

inline FloorBlendGains FlooredEqualPowerBlend(float knob01)
{
    constexpr float kBlendFloor = 0.05f;
    constexpr float kBlendSpan = 0.90f;
    constexpr float kHalfPi = 1.57079632679489661923f;
    const float flooredKnob = kBlendFloor + kBlendSpan * knob01;
    return FloorBlendGains{std::cos(flooredKnob * kHalfPi), std::sin(flooredKnob * kHalfPi)};
}

// The per-stage THRESHOLDS did NOT collapse into one shared value: peak
// (0.7, dsp/FilterFx.hpp) and Drive's output limiter (0.7, dsp/Drive.hpp)
// were already measured strictly below the new ceiling and are unchanged;
// delay and reverb wet (dsp/Delay.hpp, dsp/Reverb.hpp) were 0.9 -- ABOVE
// the new 0.80 ceiling, the negative-headroom exponential-amplifier trap
// the static_assert below exists to catch -- and were lowered to 0.72,
// preserving each one's
// original threshold/ceiling ratio (0.9/1.0 == 0.72/0.80) rather than being
// re-tuned by accident. Every non-master stage's `ceiling` now points at
// `kStageCeiling` above instead of `kSharedCeiling`; the master's stays
// `kSharedCeiling` (1.0) with threshold 0.9, since it remains the backstop
// that must fire LAST.

// VST/PLUGIN NOTE: this stage does NOT
// need to live inside a future VST/plugin build. A plugin host owns final
// gain staging on its own output bus and typically supplies its own limiter
// there, so in a plugin context the MASTER instance of this stage is
// redundant -- a candidate to bypass or compile out rather than run twice.
// (The peak-branch instance this struct also now serves is a different
// concern -- in-chain gain staging, not final output gain -- and is not
// covered by this note.)
struct OutputLimiter
{
    // Pinned defaults for the single-argument Configure(sampleRate) overload
    // below -- the MASTER output limiter's ORIGINAL tuning, UNCHANGED by
    // this extraction. Do not retune these; an independently-tuned
    // instance (e.g. the peak-branch limiter) calls the five-argument
    // Configure() overload instead of touching these.
    static constexpr float kDefaultThreshold = 0.9f;
    static constexpr float kDefaultCeiling = kSharedCeiling;
    // threshold < ceiling is a HARD invariant of DesiredMagnitude(),
    // not a style preference, and Configure() cannot check it (it takes
    // runtime floats). `headroom = ceiling - threshold`, and the return is
    // `threshold + headroom * (1 - exp(-(absX - threshold) / headroom))`:
    //   headroom == 0 -> +x/0 is +inf, exp(-inf) is 0, the term vanishes and
    //     this returns exactly `threshold`. A silent brickwall, not a NaN
    //     or a 0/0: the `absX <= threshold` early return means the
    //     numerator is always strictly positive by the time the division
    //     runs.
    //   headroom < 0  -> the exponent's sign flips and this stops being a
    //     limiter at all. It becomes an EXPONENTIAL AMPLIFIER: at threshold
    //     0.9 against ceiling 0.80, |x| = 1.5 returns 41.1 (a 27x gain) and
    //     |x| = 2.0 returns 2203. It stays FINITE until |x| ~ 9.8, so
    //     SawNaN() and RequireFiniteStereo() both pass straight through it.
    // This already happened once: the ceiling-narrowing fix above
    // reintroduced the very symptom it was written to cure, by leaving a
    // threshold un-narrowed. It got past every runtime guard in the suite,
    // because the amplifier stays finite. Hence the compile-time pin.
    static_assert(kDefaultThreshold < kDefaultCeiling,
                  "threshold must stay strictly below ceiling; a negative headroom turns "
                  "DesiredMagnitude into an exponential amplifier that stays finite and "
                  "therefore passes every NaN/finiteness guard in the suite");
    static constexpr float kDefaultAttackSeconds = 0.001f;  // fast: 1ms.
    static constexpr float kDefaultReleaseSeconds = kSharedReleaseSeconds;  // shared, so it does not pump.

    // Per-instance tuning (was `static constexpr`, shared by every
    // instance of the type -- see this file's header comment for why that
    // had to change). Defaults match the master's original tuning so a
    // freshly-constructed-but-not-yet-`Configure()`'d instance is harmless
    // (identity below `kDefaultThreshold`, same as before).
    float threshold = kDefaultThreshold;
    float ceiling = kDefaultCeiling;
    float headroom = kDefaultCeiling - kDefaultThreshold;  // 0.1f.
    float attackSeconds = kDefaultAttackSeconds;
    float releaseSeconds = kDefaultReleaseSeconds;

    float attackCoeff = 0.0f;   // (re)computed by Configure(); one-pole coeff exp(-1/(t*sampleRate)).
    float releaseCoeff = 0.0f;
    float envelope = 1.0f;      // current gain multiplier; 1.0f == no reduction (also Reset()'s value).

    // Full configuration: sample-rate-dependent coefficients PLUS this
    // instance's own threshold/ceiling/attack/release. A second,
    // independently-tuned instance (the peak-branch limiter, FilterFx.hpp)
    // calls this overload; the master keeps calling the single-argument
    // overload below, unchanged.
    // Every production caller of this overload passes an
    // already-known-positive value. Five are rooted at
    // FroggersAppCore::PrepareToPlay() (the master via the single-argument
    // Configure() below, the peak branch via FilterFxChain::Configure(),
    // delay/reverb wet via StereoDelay::SetSampleRate()/Reverb::Configure(),
    // Drive's via DriveBlendPhase::Configure()), which validates the
    // host's sample rate ONCE before any downstream use. The sixth,
    // FilterFxChain's own constructor
    // (dsp/FilterFx.hpp, `peakLimiter.Configure(kDefaultAssumedSampleRate,
    // ...)`), never went through PrepareToPlay at all -- it passes a
    // hardcoded, always-positive local constant, so it was never actually
    // relying on this clamp either. No caller can reach this method with a
    // non-positive value. A defensive `std::max(1.0f, sampleRate)` clamp
    // would not just be redundant here but actively WRONG: at sr=1.0 (the
    // only value such a clamp could produce), attackSeconds*sr collapses
    // attack/release to near-instant rather than producing a working
    // limiter. 44100.0 (PrepareToPlay's fallback) is the value that
    // actually needs to survive that disagreement.
    void Configure(float sampleRate, float thresholdIn, float ceilingIn, float attackSecondsIn,
                   float releaseSecondsIn)
    {
        threshold = thresholdIn;
        ceiling = ceilingIn;
        headroom = ceiling - threshold;
        attackSeconds = attackSecondsIn;
        releaseSeconds = releaseSecondsIn;
        attackCoeff = std::exp(-1.0f / (attackSeconds * sampleRate));
        releaseCoeff = std::exp(-1.0f / (releaseSeconds * sampleRate));
    }

    // Master-limiter convenience overload (BEHAVIOUR-PRESERVING): sample-rate
    // only, pinned to the original tuning above -- the exact call this
    // struct's one pre-existing caller (FroggersAppCore.hpp's
    // `outputLimiter_.Configure(sampleRate_)`) makes, textually unchanged by
    // this extraction.
    void Configure(float sampleRate)
    {
        Configure(sampleRate, kDefaultThreshold, kDefaultCeiling, kDefaultAttackSeconds, kDefaultReleaseSeconds);
    }

    // Instantaneous desired output magnitude for a given input magnitude:
    // identity below `threshold` (so a below-threshold sample's own target
    // gain is always exactly 1.0f), and above it an exponential-saturation
    // curve that asymptotes toward `ceiling` as `absX -> infinity` without
    // ever reaching or exceeding it for any finite input. Continuous AND
    // C1-continuous (equal value AND slope) at `absX == threshold`, so there
    // is no audible knee discontinuity between the two branches.
    float DesiredMagnitude(float absX) const
    {
        if (absX <= threshold) {
            return absX;
        }
        return threshold + headroom * (1.0f - std::exp(-(absX - threshold) / headroom));
    }

    // `targetGain == DesiredMagnitude(|x|) / |x|`: for any `|x| <= threshold`
    // this is exactly `|x| / |x| == 1.0f` (guarded at `absX == 0` to avoid a
    // 0/0). `envelope` then one-pole-smooths toward `targetGain`, fast
    // (`attackCoeff`) when MORE reduction is needed (`targetGain <
    // envelope`) and slow (`releaseCoeff`) when easing back off, so recovery
    // from a loud transient does not pump.
    //
    // Bit-identical claim for an entirely-below-threshold signal (unchanged
    // by this extraction -- `threshold`/`headroom` are read from instance
    // fields now instead of `static constexpr`, but hold the identical
    // values for the master instance, so the Sterbenz-lemma argument below
    // still applies bit for bit): if `envelope` is already exactly 1.0f and
    // `targetGain` is exactly 1.0f, then `coeff*1.0f + (1.0f-coeff)*1.0f` is
    // exactly 1.0f in IEEE-754 float, not merely in real-number arithmetic --
    // both `attackCoeff` and `releaseCoeff` sit in (0.5, 1) for any
    // musically reasonable sample rate (`exp(-1/N)` for `N` on the order of
    // tens of samples or more), so Sterbenz's lemma makes `1.0f - coeff`
    // exact, `coeff*1.0f`/`(1.0f-coeff)*1.0f` exact (multiplying by 1.0f is
    // always exact), and their sum exactly reconstructs the representable
    // value 1.0f with zero rounding error. So `envelope` never moves off
    // exactly 1.0f while the signal stays under threshold, and `x * 1.0f ==
    // x` bit for bit.
    float Process(float x) { return x * NextGain(std::fabs(x)); }

    // One envelope for both channels, driven by whichever is louder. Linking
    // is the point: two independent envelopes would duck the channels by
    // different amounts and swing the stereo image on every peak, which is a
    // worse artefact than the gain reduction itself. With l == r this reduces
    // EXACTLY to the mono call above -- same magnitude, same envelope, same
    // multiply -- so a mono source through a stereo bus stays bit-identical.
    StereoSample Process(StereoSample x)
    {
        const float gain = NextGain(std::max(std::fabs(x.l), std::fabs(x.r)));
        return {x.l * gain, x.r * gain};
    }

    // Per-unit recovery: unity gain is this unit's quiescent
    // state, the same convention `dsp::DriveBlendPhase::Reset()`
    // (Drive.hpp) uses for its own allpass history -- "no reduction" is this
    // stage's equivalent of "no recursive history". `envelope` cannot
    // legitimately leave `(0, 1]` from finite input (`targetGain` is always
    // in that range), so `StateFinite()` only exists to satisfy
    // `RecoverIfNonFinite<Unit>`'s template contract and defend against an
    // unforeseen path in.
    void Reset() { envelope = 1.0f; }

private:
    // The envelope update itself, in one place, so the mono and stereo entry
    // points cannot drift into computing gain two slightly different ways.
    float NextGain(float magnitude)
    {
        const float targetGain = magnitude > 0.0f ? DesiredMagnitude(magnitude) / magnitude : 1.0f;
        const float coeff = targetGain < envelope ? attackCoeff : releaseCoeff;
        envelope = coeff * envelope + (1.0f - coeff) * targetGain;
        return envelope;
    }

public:
    bool StateFinite() const { return std::isfinite(envelope); }
};

}  // namespace synth_froggers::dsp
