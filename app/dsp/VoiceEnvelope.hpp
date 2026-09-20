#pragma once

// synth_froggers::dsp::{VcoAdsrState, MixOscVoices} -- a **copy** of the
// cited Froggers formulas.
//
// Ported from:
//   - 08b5fd3:src/core/VcoAdsrState.hpp (whole file, verbatim -- it already has no
//     #include of src/ paths of its own, so this is a
//     line-for-line copy under app/, not a shared header)
//   - 08b5fd3:src/core/FroggersEngine.hpp:772-809 (MixOscVoices) -- specifically
//     the `m_vcoAdsr && m_adsrParams` branch at 08b5fd3:src/core/FroggersEngine.hpp:774-784, plus the plain
//     average return at 08b5fd3:src/core/FroggersEngine.hpp:786-788.
//
// NOT ported (deliberately, v1 legacy):
//   - the `m_pairAr` fallback branch, 08b5fd3:src/core/FroggersEngine.hpp:789-808 (pair
//     attack/release smoothing + copysign recombination), since deleted from the
//     firmware. This app's port
//     always has an ASR state (VcoAdsrState is unconditional here, unlike
//     the firmware engine's optional m_vcoAdsr pointer), so control permanently
//     takes the 08b5fd3:src/core/FroggersEngine.hpp:774-784 branch and then the plain-average return at
//     src/core/FroggersEngine.hpp:528 -- the exact code path the citation says to keep.

#include "DspMath.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace synth_froggers::dsp {

// 08b5fd3:src/core/VcoAdsrState.hpp, verbatim.
struct VcoAdsrState
{
    static constexpr size_t kNumVoices = 3;
    // Raised after the report "the sustain minimum value is too low. i'm
    // concerned that audio rate modulation in some of the envelope
    // parameters would result in silence."
    //
    // This is the MISSING SIBLING of the attack/decay/release floors
    // (kMinAttackSeconds/kMinDecaySeconds/kMinReleaseSeconds below). Attack
    // and release are both floored (mapAttack/mapRelease below) precisely so
    // modulation cannot drive them to a degenerate zero; sustain -- the
    // third ASR parameter, and the only per-voice LEVEL control -- was read
    // straight through as `clamp(knob, 0, 1)` with no floor at all. Two of
    // three had the guard, one did not.
    //
    // Degenerate at zero is not merely quiet, it is structurally broken:
    // `attackStep` is `sustainLevel / attackTime`, so at sustainLevel 0 the
    // step is 0 AND `m_level >= sustainLevel` is immediately true, dropping the
    // voice straight to Hold at silence with no attack at all. Worse, Attack's
    // own `m_level = min(sustainLevel, m_level + attackStep)` CLAMPS DOWN to a
    // falling sustain, so audio-rate modulation does not ride the envelope --
    // it hard-gates it to zero on every trough.
    //
    // Deliberate parity divergence from the `src/core` reference,
    // which is linear over [0.10, 1.0]: MapSustain below is exponential over
    // [kMinSustainLevel, 1.0], and this floor moved from 0.10 (-20 dB) to
    // 0.25 (-12 dB) to go with it. A straight exponential over the OLD
    // [0.10, 1.0] range would have made a randomized sustain quieter on
    // average, not louder -- raising the floor to 0.25 alongside the curve
    // change keeps the random mean statistically unchanged from the old
    // linear mapping while raising the worst-case random draw from -20 dB to
    // -12 dB. The floor's own reason for existing is unchanged: it is
    // audible, unmistakably quiet, and far enough above zero that a
    // full-depth modulation trough still passes signal.
    // TRADE-OFF: sustain 0 is no longer a hard mute for a
    // voice, it is this floor.
    static constexpr float kMinSustainLevel = 0.25f;
    // Deliberate parity divergence (same treatment as Fuegoize.hpp's own
    // divergence note): lowered from 2.5s to 1.0s. 1.0s still comfortably
    // covers a slow pad swell; 2.5s was judged unnecessarily long. Lowered
    // again later, 1.0s -> 0.5f ("that max attack is also way too long.
    // half a second at most"). Scope is ATTACK
    // ONLY -- see kMaxDecaySeconds's own
    // comment below for why its former "mirrors kMaxAttackSeconds" rationale
    // no longer holds and has been rewritten as decay's own judgment.
    // Halved again, 0.5f -> 0.25f: Attack is a modulation TARGET (see the
    // resonant peak's own ceiling comment, FroggersAppCore.hpp, for why that
    // matters), so a randomized depth visits this ceiling regularly, not
    // only when an operator dials there. Even after the exponential remap
    // above, the top tenth of randomized draws still exceeded 269 ms; this
    // halving moves that to about 144 ms.
    static constexpr float kMaxAttackSeconds = 0.25f;
    // Deliberate parity divergence from the `src/core` reference,
    // which maps Attack/Decay/Release LINEARLY: mapAttack/mapDecay/mapRelease
    // below moved to dsp::ExpMapCompute (every other time/frequency control in
    // the instrument already maps exponentially; these three were the odd
    // ones out). A single shared kMinTimeSeconds floor (0.5 ms) does not fit
    // an exponential map the way it fit a linear one -- on a linear map the
    // floor barely matters, but on an exponential map a uniform random knob
    // spends HALF its draws below the geometric midpoint, so a 0.5 ms floor
    // put a meaningful fraction of randomized attacks at a floor low enough
    // to click. Each of the three stages gets its own named floor instead.
    // Attack's own floor, 1 ms, is fast enough to read as instant while
    // staying clear of a literal zero-length ramp.
    static constexpr float kMinAttackSeconds = 0.001f;
    // Deliberate parity divergence: lowered
    // from the firmware's 10.0s to 5.0s. Note this does NOT by itself
    // make Stop responsive -- 5s of release after pressing Stop still reads as
    // broken, which is why FroggersAppCore's stop path forces a ~50ms fade
    // independent of this ceiling (see kStopFadeReleaseKnob there).
    //
    // A complaint that "sustain
    // and release maxima are simply too long" was traced before changing
    // anything: `sustainKnob`
    // is clamped to a LEVEL in [0,1] by `stepVoice` below, not mapped through
    // any seconds-ceiling constant -- there is no `kMaxSustainSeconds` in
    // this file, and none is buildable from a knob that is a level, not a
    // time. "Sustain maximum too long" therefore cannot literally mean
    // sustain; it is release read through the perceptual lens of "how long
    // the note keeps ringing after I let go" (release governs exactly that,
    // Stage::Release above), so this is treated as a release complaint.
    // Halved as a starting point, ear-tuned like the 5.0s
    // value it replaces -- this number is not derived from anything.
    static constexpr float kMaxReleaseSeconds = 2.5f;
    // mapRelease's own floor for the exponential map (see kMinAttackSeconds's
    // own comment for why a shared kMinTimeSeconds no longer fits). 5 ms,
    // same as kMinDecaySeconds below -- both are transient-completion stages
    // rather than the attack onset, so neither needs Attack's tighter 1 ms.
    static constexpr float kMinReleaseSeconds = 0.005f;
    // (Envelope slot 1/5/9, Decay). No design-doc
    // value exists for Decay's own time ceiling (the original spec only
    // specified the peak/numerator fix, not a Decay time range) -- this is
    // an implementer judgment call, reported as such rather than silently
    // asserted. Originally set to mirror kMaxAttackSeconds's then-value
    // (1.0f); kMaxAttackSeconds was halved to 0.5f for
    // Attack ALONE -- attack was named specifically, not decay --
    // so that mirror is now deliberately broken and this value stands on its
    // own judgment: 1.0f, unchanged, kept generous because Decay (like the
    // old mirrored rationale for Attack) is a per-note transient stage that
    // normally completes WHILE the gate is still held, so kMaxReleaseSeconds's
    // own halving rationale ("note keeps ringing after I let go") still does
    // not apply here. If Decay's own ceiling ever needs revisiting, it now
    // needs its own deliberate re-judgment -- it no longer inherits Attack's.
    static constexpr float kMaxDecaySeconds = 1.0f;
    // mapDecay's own floor for the exponential map -- see kMinAttackSeconds's
    // own comment for why a shared kMinTimeSeconds no longer fits three
    // different stages. 5 ms, same as kMinReleaseSeconds.
    static constexpr float kMinDecaySeconds = 0.005f;
    // (Envelope slot 13, Grace). Another
    // implementer judgment call: no governing specification fully pins this
    // ceiling, so it needed its own design consideration at implementation
    // time. 1.0f matches kMaxDecaySeconds's own scale (a generous but bounded
    // per-note floor, not an open-ended hang). This comment used
    // to also claim a match with kMaxAttackSeconds; halving attack to 0.5f
    // broke that mirror -- Grace's own ceiling was not part of that
    // change, so it stays 1.0f on its own judgment, matching decay alone.
    static constexpr float kMaxGraceSeconds = 1.0f;

    static constexpr float kGraceCurveBase = 25.0f;

    // Public and static because the parity tests need the same number this
    // maps to, and they used to restate the formula instead -- which is what
    // silently pinned them to the old linear map. One definition, both
    // callers.
    static float GraceSecondsForKnob(float knob)
    {
        const float clamped = std::min(std::max(knob, 0.0f), 1.0f);
        return ZeroedExpCompute(kGraceCurveBase, clamped) * kMaxGraceSeconds;
    }


    // Grace (Envelope slot 13) countdown sentinel. Named rather than
    // repeated: this was a bare -1.0f literal written/compared at 5
    // sites -- the comment further below explains why the 5th site, the
    // Hold-branch guard in stepVoice(), must match this value by EXACT
    // identity (`==`), not by sign or by any arithmetic-derived value.
    // Named once here, reused at every write and the one read, so a stray
    // -1.f/-1.0/an arithmetic near-miss at any site can no longer silently
    // break the countdown.
    //
    // This is an IDENTITY sentinel, not a magnitude -- "not started" is
    // whatever value this constant holds, not "any negative number". Exact
    // equality is safe (never loosen the `==` below to `<` or `<=`) because
    // of two invariants this file maintains together:
    //   1. Every WRITE that means "not started" -- init(), setGate(true),
    //      ForceReleaseAll() -- assigns this exact named constant; no code
    //      path ever derives "not started" any other way.
    //   2. The decrement path (`m_graceRemaining[i] -= 1.0f` in stepVoice())
    //      is reachable only while the countdown is strictly > 0.0f (guarded
    //      by the `<= 0.0f` expiry check immediately above it), so a live,
    //      decrementing countdown can never arithmetic its way back to
    //      exactly this value -- only a fresh write can produce it.
    static constexpr float kGraceNotStarted = -1.0f;

    enum class Stage : uint8_t
    {
        Idle = 0,
        Attack,
        Decay,
        Hold,
        Release,
    };

    void init(float sampleRate)
    {
        setSampleRate(sampleRate);
        m_gateHigh = false;
        for (size_t i = 0; i < kNumVoices; ++i)
        {
            m_stage[i] = Stage::Idle;
            m_level[i] = 0.0f;
            m_releasePending[i] = false;
            m_graceRemaining[i] = kGraceNotStarted;  // grace countdown not started
        }
    }

    // This struct's only production caller is
    // FroggersAppCore::PrepareToPlay() (audioAdsr_.init(sampleRate_), which
    // calls this), and that method validates the host's sample rate
    // ONCE before any downstream use -- so a defensive
    // `44100.0f` re-guard here would be unreachable. The bare
    // `m_sampleRate = 44100.0f` default below is a different concern (a
    // harmless pre-`init()` placeholder, not a re-guard of validated input)
    // and is left as is.
    void setSampleRate(float sampleRate)
    {
        m_sampleRate = sampleRate;
    }

    void setGate(bool high)
    {
        if (high == m_gateHigh)
        {
            return;
        }
        m_gateHigh = high;
        for (size_t i = 0; i < kNumVoices; ++i)
        {
            if (high)
            {
                // Hard retrigger, unchanged from before Grace existed: every
                // voice snaps to Attack regardless of its current stage/level
                // (m_level itself is untouched, so this does not click if a
                // voice was mid-release). Grace (slot 13) ITEM B: a retrigger
                // also cancels any deferred release from the previous gate-low
                // period outright -- chosen because a fresh note-on is a
                // stronger, more recent intent than an old pending release,
                // and leaving the old timer running could force an
                // unrelated, unexpected cutoff mid-way through the new note.
                m_stage[i] = Stage::Attack;
                m_releasePending[i] = false;
                m_graceRemaining[i] = kGraceNotStarted;
            }
            else
            {
                // Grace (slot 13) ITEM B: do NOT force Stage::Release here.
                // stepVoice() below resolves the deferred transition, because
                // only it has the live attack/decay/grace knob values needed
                // to decide (setGate() is never given them). Marking intent
                // here and resolving in stepVoice() keeps the resolution
                // synchronous with the very next stepVoice() call, so the
                // grace-inactive (default) case still transitions to Release
                // on the SAME sample setGate(false) was called on, exactly
                // like the old unconditional force did.
                m_releasePending[i] = true;
            }
        }
    }

    // True per-voice ASR. sustainKnob is not a time -- it is the target
    // level (0..1, clamped) that Attack/Decay ramp toward and Hold holds at.
    // curveKnob/graceKnob default to 0.0f (their exact neutral values -- see
    // mapGrace's own comment for why Grace has no floor) so existing callers
    // that only care about attack/decay/sustain/release keep compiling and
    // keep today's linear-shape, no-deferral behaviour unchanged.
    float apply(size_t voiceIndex,
                float input,
                float attackKnob,
                float decayKnob,
                float sustainKnob,
                float releaseKnob,
                float curveKnob = 0.0f,
                float graceKnob = 0.0f)
    {
        if (voiceIndex >= kNumVoices)
        {
            return input;
        }
        stepVoice(voiceIndex, attackKnob, decayKnob, sustainKnob, releaseKnob, curveKnob, graceKnob);
        return input * m_level[voiceIndex];
    }

    // Used by app/FroggersAppCore.hpp's Stop-transport delay/reverb reset:
    // true once every voice has fully released and stopped producing
    // signal, i.e. no voice can still be re-exciting a downstream feedback
    // structure (delay/reverb). Const, read-only -- does not step or alter
    // any voice's stage/level.
    bool AllIdle() const
    {
        for (size_t i = 0; i < kNumVoices; ++i)
        {
            if (m_stage[i] != Stage::Idle)
            {
                return false;
            }
        }
        return true;
    }

    // Forced release for
    // the transport running->stopped edge, called from
    // FroggersAppCore.hpp beside the delayReverbClearPending_ logic. Every
    // non-idle voice enters Stage::Release IMMEDIATELY, bypassing the Grace
    // minimum-hold (setGate()'s own comment: a play-time gate-false edge
    // only marks m_releasePending[i] and defers the actual transition to
    // stepVoice(), specifically so Grace's Hold-stage countdown can run) and
    // any in-progress Attack/Decay -- this restores the pre-Grace
    // `setGate(false)` synchronous-force semantic for Stop ONLY. Play-time
    // gating (setGate()) is completely untouched by this method; it is not
    // called from there, and it does not read or write m_gateHigh.
    //
    // m_level is left alone: Stage::Release's own ComputeRampStep call
    // (stepVoice(), below) ramps down from wherever the voice currently
    // sits, exactly like the old unconditional force did and exactly like
    // an ordinary Release entered via the Grace ladder does today -- this
    // method only short-circuits WHICH stage a voice is in, not the level
    // it ramps from. Idle voices are left at Idle (the `if` guard): forcing
    // Idle->Release here would fabricate a release tail out of nothing and
    // would also flip FroggersAppCore's `AllIdle()`-at-edge check (this
    // method's own caller) from "nothing to wait for, clear now" to
    // "something to wait for", which is backwards for a voice that was
    // already silent.
    //
    // m_releasePending/m_graceRemaining are unconditionally reset (even for
    // an already-idle voice, harmlessly) so no stale deferred-release state
    // from before this edge can act once play resumes and setGate() starts
    // issuing fresh edges -- m_graceRemaining's own kGraceNotStarted "not
    // started" sentinel is restored exactly as init()/setGate(true) already
    // do.
    void ForceReleaseAll()
    {
        for (size_t i = 0; i < kNumVoices; ++i)
        {
            if (m_stage[i] != Stage::Idle)
            {
                m_stage[i] = Stage::Release;
            }
            m_releasePending[i] = false;
            m_graceRemaining[i] = kGraceNotStarted;
        }
    }

    // The third of the three ASR range maps, and the one that was missing.
    // Exponential over [kMinSustainLevel, 1.0] -- see kMinSustainLevel's own
    // comment for why zero is degenerate here, and for the parity divergence
    // from the firmware's linear reference. No longer `constexpr`: ExpMapCompute
    // goes through std::pow, which is not a constant expression on this
    // toolchain.
    //
    // PUBLIC and static, unlike its two private siblings, for one reason: the
    // parity tests assert the settled level a given sustain KNOB produces, and
    // they must read that from this one function rather than re-deriving the
    // formula themselves. A test that retypes the formula is a second
    // definition site of it, and would go on passing against the old shape if
    // this map were ever changed. dsp::kMaxResonantBumpHeight's own comment
    // records this project already shipping exactly that bug: a test that
    // computed its expectation with its own hardcoded copy of the constant and
    // therefore passed unchanged when the real value moved.
    static float MapSustain(float knob)
    {
        const float clamped = std::min(std::max(knob, 0.0f), 1.0f);
        return ExpMapCompute(kMinSustainLevel, 1.0f, clamped);
    }

    // Floor on the curved branch's per-sample progress (see ComputeRampStep
    // below) -- kept at 0.4f, not the floor equal to the exponential's own
    // 1% point (~0.046), because the wider grid a stage's duration is
    // measured over (every sample rate, every Sustain the knob reaches, not
    // just the one the shipped sweep used to pin) put the smaller floor's
    // worst multiple at 10.17x, over the completion requirement's own
    // ceiling; 0.4f measures 2.60x over the same grid
    // (research/curve-candidate-adjudication.md). mapCurve below is the
    // clamped identity, so this constant has no warp counterpart to drift
    // against anymore.
    //
    // Public: the parity tests sweep this constant's own effect densely and
    // must read it from here rather than retyping it.
    static constexpr float kCurveMinProgress = 0.4f;

    // Natural log of 100: exp(-kReach99) == 0.01, so a unit-travel ramp
    // driven by the exponential step alone (curveAmount==1) reaches 99% of
    // its travel exactly when a linear ramp of the same stepMagnitude
    // completes -- the rate that lands the curved branch's own duration
    // near the linear one instead of scaling away from it.
    //
    // Public: same reason as kCurveMinProgress above.
    static constexpr float kReach99 = 4.605170f;

    // mapCurve and ComputeRampStep are pure functions of their arguments,
    // static so the compiler forbids them from reading the state (in
    // particular the sample rate), and public so the tests can sweep them
    // densely instead of retyping their constants.

    // Curve (Envelope slot 12) as the operator turns it. The exponential
    // branch below (ComputeRampStep) does not need a warp: its duration is
    // already bounded by the floor and sits near the linear duration across
    // the knob's travel (worst multiple 2.60x, not the old one-pole
    // family's unbounded 1/(1-c)), so there is no quantity left to
    // linearise -- the knob passed straight through IS curveAmount, the
    // blend's one direct parameter.
    static float mapCurve(float knob)
    {
        return std::min(std::max(knob, 0.0f), 1.0f);
    }

    static float ComputeRampStep(float from, float target, float stepMagnitude, float curveAmount)
    {
        if (curveAmount <= 0.0f)
        {
            return (target >= from) ? std::min(target, from + stepMagnitude)
                                     : std::max(target, from - stepMagnitude);
        }
        const float remaining = target - from;
        const float absRemaining = std::fabs(remaining);
        if (absRemaining <= stepMagnitude)
        {
            return target;  // finishing step, same snap-to-target the linear path uses.
        }
        // Shape: one law, expressed in remaining distance, for all three
        // stages. The exponential step below moves fast while far from
        // target and slows as it closes in, in proportion to how much
        // distance is left -- Attack rises fast and flattens into its
        // peak; Decay and Release, descending the same law, fall fast and
        // linger into their target. curveAmount blends it with the plain
        // linear step: 0 is all linear, 1 is all exponential.
        const float exponentialStep = kReach99 * stepMagnitude * absRemaining;
        const float appliedMagnitude = (1.0f - curveAmount) * stepMagnitude + curveAmount * exponentialStep;

        // Under this law the crawl sits near the target, not far from it
        // (the old one-pole shape's failure mode): unfloored, remaining
        // distance decays as (1-r)^n forever, so a stage whose travel is
        // small against the full 0..1 scale -- Release at a low Sustain,
        // Decay at a high one -- takes longer to close the last stretch,
        // and that is what completion time is bounded against here, not an
        // unbounded far-from-target crawl. Floor the per-sample progress
        // MAGNITUDE (toward target, whichever direction -- this covers
        // descending Decay/Release the same as ascending Attack) at
        // kCurveMinProgress of the linear step: below an absolute
        // remaining distance of kCurveMinProgress/kReach99 (about 0.087),
        // the floor takes over and the last 8.8% of the ramp's travel runs
        // linear at the top of the knob -- the cost of bounding
        // completion. Below Curve 0.5 the floor is inert (curveAmount is
        // small enough that appliedMagnitude never falls under the floor
        // for a remaining distance this ramp actually reaches).
        //
        // 1/kCurveMinProgress (2.5x at 0.4f) is a floor on the realized
        // completion multiple, not a ceiling on it: the accumulator
        // (`bounded = from + boundedProgress` below) quantizes the floored
        // step to whole ulps of its float32 value, so the realized rate is
        // slightly under nominal and the measured worst multiple over
        // every sample rate, the knob grid and Sustain 0.05/0.5/0.95 is
        // 2.60x, not 2.5x (research/curve-candidate-adjudication.md).
        const float minProgressMagnitude = stepMagnitude * kCurveMinProgress;
        const float boundedProgress = (appliedMagnitude < minProgressMagnitude)
                                           ? std::copysign(minProgressMagnitude, remaining)
                                           : std::copysign(appliedMagnitude, remaining);
        const float bounded = from + boundedProgress;
        return (remaining >= 0.0f) ? std::min(target, bounded) : std::max(target, bounded);
    }

private:
    // Exponential, not linear -- see kMinAttackSeconds's own comment. Every
    // other time/frequency control in the instrument already maps this way;
    // a uniform random knob on the old linear map put half its draws above
    // the geometric midpoint, giving no transient on half of all randomized
    // voices.
    float mapAttack(float knob) const
    {
        const float clamped = std::min(std::max(knob, 0.0f), 1.0f);
        return ExpMapCompute(kMinAttackSeconds, kMaxAttackSeconds, clamped);
    }

    // Exponential -- see mapAttack's own comment.
    float mapRelease(float knob) const
    {
        const float clamped = std::min(std::max(knob, 0.0f), 1.0f);
        return ExpMapCompute(kMinReleaseSeconds, kMaxReleaseSeconds, clamped);
    }

    // Decay (Envelope slot 1/5/9). Exponential, same reasoning as mapAttack's
    // own comment -- see kMaxDecaySeconds's own comment for why that constant's value is
    // an implementer judgment call, not a recorded design number.
    float mapDecay(float knob) const
    {
        const float clamped = std::min(std::max(knob, 0.0f), 1.0f);
        return ExpMapCompute(kMinDecaySeconds, kMaxDecaySeconds, clamped);
    }

    // Grace (Envelope slot 13). Exponential like Attack/Decay/Release, but
    // through ZeroedExpCompute rather than ExpMapCompute. Those two differ in
    // exactly the way that matters here: ExpMapCompute floors at a small
    // positive time, so a modulated knob can never drive a ramp to a
    // degenerate zero-length step, while Grace needs the opposite -- knob==0
    // (the registered default, FroggersParameters.hpp) must map to EXACTLY
    // 0.0f seconds, because the "Grace at default is a no-op" requirement
    // depends on bit-exact zero rather than a small floor.
    //
    // This was linear for that reason. Linear spends the whole bottom
    // twentieth of the knob on 0-50ms and the rest on holds long enough to
    // be a different control, which is unusable where the short holds live.
    // ZeroedExpCompute gives the exponential travel AND the exact zero.
    // Base 25 across the knob's travel. ZeroedExpCompute returns EXACTLY 0
    // at knob 0 and EXACTLY 1 at knob 1 -- (base^0-1)/(base-1) and
    // (base^1-1)/(base-1) -- so the bit-exact zero the no-op requirement
    // rests on survives, which a floored ExpMapCompute could not give.
    float mapGrace(float knob) const { return GraceSecondsForKnob(knob); }

    void stepVoice(size_t voiceIndex,
                    float attackKnob,
                    float decayKnob,
                    float sustainKnob,
                    float releaseKnob,
                    float curveKnob,
                    float graceKnob)
    {
        const float sustainLevel = MapSustain(sustainKnob);
        // Attack's ceiling is now an independent peak
        // (1.0f, the same ceiling Vco's own output already has -- no named
        // constant) instead of
        // sustainLevel, so attackStep's NUMERATOR must be 1.0f too, or the
        // realized attack time silently becomes attackTime/sustainLevel (up
        // to 4x longer at kMinSustainLevel=0.25f).
        const float attackStep = 1.0f / std::max(mapAttack(attackKnob) * m_sampleRate, 1.0f);
        // Decay ramps DOWN from that same 1.0f peak to sustainLevel; its
        // numerator is that range, following attackStep/releaseStep's own
        // divide-by-mapped-time idiom.
        const float decayStep = (1.0f - sustainLevel) / std::max(mapDecay(decayKnob) * m_sampleRate, 1.0f);
        const float releaseStep = 1.0f / std::max(mapRelease(releaseKnob) * m_sampleRate, 1.0f);
        const float curveAmount = mapCurve(curveKnob);

        // Grace: resolve any deferred gate-false transition BEFORE
        // the stage switch below, so the switch this same call already sees
        // the resolved stage -- this is what keeps the grace-inactive
        // (default) case landing on the SAME sample as the old eager
        // setGate(false) force (see setGate()'s own comment).
        if (m_releasePending[voiceIndex])
        {
            const float graceSeconds = mapGrace(graceKnob);
            const bool graceInactive = graceSeconds <= 0.0f;
            const Stage stage = m_stage[voiceIndex];
            if (graceInactive && (stage == Stage::Attack || stage == Stage::Decay || stage == Stage::Hold))
            {
                // Neutral default: identical to the old unconditional force
                // -- forcibly cut short whatever stage this voice was in.
                m_stage[voiceIndex] = Stage::Release;
                m_releasePending[voiceIndex] = false;
            }
            else if (!graceInactive && stage == Stage::Hold)
            {
                // Minimum-hold countdown, started the first sample Hold is
                // reached with a release still pending (Attack/Decay are
                // left alone above, so they always run to completion first
                // -- "at minimum Attack (and Decay) has completed").
                // Sentinel-conflation bug, found and fixed: this guard used
                // to be `< 0.0f`, which treats ANY negative value as "not
                // started" -- but a countdown initialized to a non-float-
                // exact `graceSeconds * m_sampleRate` (most grace knobs;
                // only ones landing on an exact integer sample count are
                // immune) decrements past zero to a small negative,
                // non-sentinel value without ever landing exactly on 0.0f,
                // and `< 0.0f` mistook that pending-expiry value for "not
                // started", re-arming it to the full grace forever.
                // Matching the exact kGraceNotStarted sentinel instead means
                // only a genuinely fresh countdown re-initializes; a
                // decremented-negative value falls through untouched to the
                // `<= 0.0f` expiry check immediately below.
                if (m_graceRemaining[voiceIndex] == kGraceNotStarted)
                {
                    m_graceRemaining[voiceIndex] = graceSeconds * m_sampleRate;
                }
                if (m_graceRemaining[voiceIndex] <= 0.0f)
                {
                    m_stage[voiceIndex] = Stage::Release;
                    m_releasePending[voiceIndex] = false;
                    m_graceRemaining[voiceIndex] = kGraceNotStarted;
                }
                else
                {
                    m_graceRemaining[voiceIndex] -= 1.0f;
                }
            }
            // else: grace active and still in Attack/Decay -- keep
            // progressing toward Hold untouched; Release/Idle -- pending is
            // stale (already resolved), harmless, cleared on the next
            // setGate() edge.
        }

        switch (m_stage[voiceIndex])
        {
            case Stage::Idle:
                m_level[voiceIndex] = 0.0f;
                break;
            case Stage::Attack:
                m_level[voiceIndex] = ComputeRampStep(m_level[voiceIndex], 1.0f, attackStep, curveAmount);
                if (m_level[voiceIndex] >= 1.0f)
                {
                    m_stage[voiceIndex] = Stage::Decay;
                }
                break;
            case Stage::Decay:
                m_level[voiceIndex] = ComputeRampStep(m_level[voiceIndex], sustainLevel, decayStep, curveAmount);
                if (m_level[voiceIndex] <= sustainLevel)
                {
                    m_stage[voiceIndex] = Stage::Hold;
                }
                break;
            case Stage::Hold:
                // Gate-high hold and gate-low-but-still-deferred hold are the
                // same visible state (sustainLevel); the grace resolution
                // above already decided whether/when this stage moves on.
                m_level[voiceIndex] = sustainLevel;
                break;
            case Stage::Release:
                m_level[voiceIndex] = ComputeRampStep(m_level[voiceIndex], 0.0f, releaseStep, curveAmount);
                if (m_level[voiceIndex] <= 0.0f)
                {
                    m_stage[voiceIndex] = Stage::Idle;
                }
                break;
        }
    }

    float m_sampleRate = 44100.0f;
    bool m_gateHigh = false;
    std::array<float, kNumVoices> m_level{};
    std::array<Stage, kNumVoices> m_stage{};
    // Grace per-voice state. m_releasePending: true from a
    // gate-false edge until stepVoice() actually forces Stage::Release (see
    // setGate()/stepVoice()'s own comments -- this is the only new
    // information setGate() itself records, since it lacks the knob values
    // needed to decide). m_graceRemaining: minimum-hold countdown in
    // samples, kGraceNotStarted sentinel meaning "not started yet"; only
    // used while a release is pending AND the voice is in Hold.
    std::array<bool, kNumVoices> m_releasePending{};
    // A bare `{}` here value-initializes every element to 0.0f,
    // NOT kGraceNotStarted -- traced whether that is a live bug (the
    // Hold-branch exact-equality guard in stepVoice() above would misread a
    // never-init()'d 0.0f as "expire now" instead of "not started").
    //
    // Verdict: NOT reachable, independent of init()-vs-stepVoice() ordering.
    // The `== kGraceNotStarted` guard is only evaluated inside `if
    // (m_releasePending[voiceIndex])` (stepVoice() above), and
    // m_releasePending can only become true via setGate(false) -- which
    // itself no-ops unless m_gateHigh was already true (`if (high ==
    // m_gateHigh) return;`, setGate() above), i.e. only after a PRIOR
    // setGate(true) call. setGate(true)'s own "high" branch unconditionally
    // writes `m_graceRemaining[i] = kGraceNotStarted` for every voice
    // (setGate(), above) before m_releasePending can ever flip true. So by
    // the time the Hold+pending branch can execute at all, this member has
    // already been overwritten with the real sentinel by setGate(true) --
    // regardless of whether init() ever ran. (Separately, init() IS in fact
    // guaranteed to run first in production: FroggersAppCore::PrepareToPlay
    // calls audioAdsr_.init(sampleRate_), and
    // Sheaf's synth::Engine::Prepare() -> app_.PrepareToPlay() call
    // (External/Sheaf/projects/synth/include/synth/Engine.hpp) runs
    // from Runtime::audioDeviceAboutToStart()
    // (External/Sheaf/projects/synth/runtime/Runtime.hpp), which the
    // host is contractually required to call before the first
    // audioDeviceIOCallback/ProcessBlock -- but that ordering is not what
    // makes this safe; the setGate(true) argument above holds even if it
    // were violated.)
    //
    // The default initializer is changed to kGraceNotStarted anyway, purely
    // so the type's own default-constructed state is self-consistent with
    // what every other "not started" write in this file uses -- not because
    // a live bug was found.
    std::array<float, kNumVoices> m_graceRemaining = []
    {
        std::array<float, kNumVoices> result{};
        result.fill(kGraceNotStarted);
        return result;
    }();
};

// The three
// GATED (post-`adsr.apply`) per-voice values, for a caller that needs them
// for something other than the mixed average below -- specifically,
// FroggersAppCore::RouteAudioSample()'s post-gate scope tap (see this
// struct's own header comment and Vco.hpp's `Process()` comment for why the
// pre-gate write it used to do there was wrong). A plain struct, not
// `std::array<float, 3>`, so call sites read `.v1`/`.v2`/`.v3` instead of
// index magic numbers that would have to match VcoAdsrState's own voice
// index convention (0/1/2) by coincidence rather than by name.
struct GatedVoices
{
    float v1 = 0.0f;
    float v2 = 0.0f;
    float v3 = 0.0f;
};

// (VCO balance, Audio slot 13): a normalized 3-point crossfade that
// tilts emphasis VCO1 -> VCO2 -> VCO3 as knob01 sweeps 0 -> 1, replacing
// MixOscVoices's old hardcoded equal-thirds average. BINDING INVARIANT:
// the three weights sum to exactly 1 and each stays
// within [0.10, 0.80] at EVERY knob01 -- guaranteed BY CONSTRUCTION, not
// checked after the fact:
//   - Below knob01 0.5, the result is a convex combination ((1-u), u for
//     u in [0,1]) of two anchor triples: (0.80, 0.10, 0.10) at knob01==0
//     and (1/3, 1/3, 1/3) at knob01==0.5.
//   - Above knob01 0.5, it is a convex combination of (1/3, 1/3, 1/3) at
//     knob01==0.5 and (0.10, 0.10, 0.80) at knob01==1.
// Both anchor triples in each half sum to exactly 1, and a convex
// combination of two vectors that each sum to 1 always sums to 1 (the
// weights (1-u)+u==1 factor straight through the sum). Both anchor triples
// in each half also lie within [0.10, 0.80] component-wise, and a convex
// combination of two points inside an interval never leaves that interval
// -- so every weight is bounded the same way, for the same reason,
// regardless of knob01. At knob01==0.5 (centre default) every weight is
// exactly kThird, i.e. exactly the equal-thirds mix this replaces (same
// constant MixOscVoices's old `(v1+v2+v3)*(1.0f/3.0f)` used).
inline void ComputeVcoBalanceWeights(float knob01, float& w1, float& w2, float& w3)
{
    constexpr float kFloor = 0.10f;
    constexpr float kCap = 0.80f;
    constexpr float kThird = 1.0f / 3.0f;
    const float knob = std::min(std::max(knob01, 0.0f), 1.0f);
    if (knob <= 0.5f)
    {
        const float u = knob / 0.5f;  // 0 at knob01==0, 1 at knob01==0.5.
        w1 = kCap * (1.0f - u) + kThird * u;
        w2 = kFloor * (1.0f - u) + kThird * u;
        w3 = kFloor * (1.0f - u) + kThird * u;
    }
    else
    {
        const float v = (knob - 0.5f) / 0.5f;  // 0 at knob01==0.5, 1 at knob01==1.
        w1 = kThird * (1.0f - v) + kFloor * v;
        w2 = kThird * (1.0f - v) + kFloor * v;
        w3 = kThird * (1.0f - v) + kCap * v;
    }
}

// 08b5fd3:src/core/FroggersEngine.hpp:772-809 (MixOscVoices), the m_vcoAdsr && m_adsrParams
// branch (08b5fd3:src/core/FroggersEngine.hpp:774-784) applied unconditionally -- this app always has an ASR
// state, so the firmware engine's `if (m_vcoAdsr && m_adsrParams)` guard has
// no off-state here -- followed by the plain average return at 08b5fd3:src/core/FroggersEngine.hpp:786-788.
// adsr[v] rows are (attack, sustain, release) per voice, matching
// the retired simulator's per-VCO ADSR page triplet row order.
//
// `balanceKnob01` (Audio slot 13): drives ComputeVcoBalanceWeights
// above, replacing the old hardcoded equal-thirds average. Defaults to 0.5f
// (this mapping's own centre, which reproduces the exact 1/3-each weights
// the old average used) so callers that pass neither this nor `outGated`
// keep their prior mixed-average behaviour unchanged.
//
// `outGated`: optional out-parameter carrying the three post-gate voice values, added
// so FroggersAppCore::RouteAudioSample() (this function's single
// production call site) can tap the GATED signal for the scope instead of
// the pre-gate raw VCO output Vco::Process() used to write directly (this
// extends this existing single call site rather than re-applying
// `adsr.apply` a second time at a new call site, which would duplicate a
// ported formula and create two sites that could drift). Defaults to
// `nullptr` so the two existing test call sites
// (FroggersDspParityTests.cpp) that
// only need the mixed average keep compiling unchanged. `outGated` still
// carries the raw per-voice GATED values (unweighted) -- balanceKnob01
// only changes how the three are combined into the single mixed return.
// `decay1/2/3` (Envelope slot 1/5/9): per-voice Decay knobs, added alongside
// attack/sustain/release. `curveKnob`/`graceKnob` (Envelope slot 12/13) are
// SHARED across all three voices (matching the spec delta's "applying to all
// three voices'" / single-knob-per-page framing for these two, unlike the
// per-VCO attack/decay/sustain/release quads) and default to 0.0f -- their
// exact neutral values -- so callers that only care about decay keep
// compiling and keep today's shape/no-deferral behaviour.
inline float MixOscVoices(VcoAdsrState& adsr,
                           float v1,
                           float v2,
                           float v3,
                           float attack1,
                           float decay1,
                           float sustain1,
                           float release1,
                           float attack2,
                           float decay2,
                           float sustain2,
                           float release2,
                           float attack3,
                           float decay3,
                           float sustain3,
                           float release3,
                           float curveKnob = 0.0f,
                           float graceKnob = 0.0f,
                           float balanceKnob01 = 0.5f,
                           GatedVoices* outGated = nullptr)
{
    v1 = adsr.apply(0, v1, attack1, decay1, sustain1, release1, curveKnob, graceKnob);
    v2 = adsr.apply(1, v2, attack2, decay2, sustain2, release2, curveKnob, graceKnob);
    v3 = adsr.apply(2, v3, attack3, decay3, sustain3, release3, curveKnob, graceKnob);
    if (outGated != nullptr)
    {
        outGated->v1 = v1;
        outGated->v2 = v2;
        outGated->v3 = v3;
    }
    float w1 = 0.0f;
    float w2 = 0.0f;
    float w3 = 0.0f;
    ComputeVcoBalanceWeights(balanceKnob01, w1, w2, w3);
    return w1 * v1 + w2 * v2 + w3 * v3;
}

}  // namespace synth_froggers::dsp
