#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <vector>

// synth_froggers::dsp::FiniteOnly / synth_froggers::dsp::Magnitude -- the
// two per-unit fault-recovery tiers app/FroggersAppCore.hpp's RecoverUnitIfNeeded()/
// RecoverIfNonFinite() implement -- Tier 1 (finiteness only) and Tier 2
// (finiteness, then a sustained-over-ceiling magnitude watch). Every
// dsp:: struct that participates in recovery declares which tier IT gets
// at the single call site that enumerates it (each struct's own
// `ForEachStatefulUnit`, dsp/FilterFx.hpp's FilterFxChain, dsp/Drive.hpp's
// FrogBlock, FroggersAppCore.hpp's own composed walk) by passing one of
// these two tag TYPES, never by testing the unit's own interface (e.g.
// `if constexpr (requires { unit.StateMagnitude(); })`). Inferring would
// silently reclassify a unit's fault recovery the moment a read-only
// diagnostic is added to it -- exactly what happened when StateMagnitude()
// was added to dsp::StereoDelay/dsp::Reverb purely as a measurement
// aid: both structs' own comments require them to stay Tier-1-only (a
// BIBO-stable feedback loop can legitimately settle to a large-but-finite
// steady state under sustained loud input, so Tier 2's ceiling would
// misfire on them), and an interface-inferred tier would have promoted
// them to Tier 2 the instant that diagnostic landed.
//
// Only the tier is declared by tag. Within Tier 2, StateOverCeiling() below
// answers the over-ceiling question from a unit's own StateOverCeiling() when
// it has one (a unit whose memory is a WatchedBuffer, which knows in constant
// time) and from StateMagnitude() otherwise; that chooses how a Tier-2 unit is
// measured, never which tier it gets.
//
// TYPES, not an enum: the tier must be resolvable at COMPILE time so
// `if constexpr` can guard which of RecoverUnitIfNeeded()/RecoverIfNonFinite()
// actually gets instantiated for a given unit -- a runtime `enum class`
// value read through a plain `if` does not prevent instantiation, so
// RecoverUnitIfNeeded() (which requires `unit.StateMagnitude()`) would still
// get type-checked against every FiniteOnly-tagged unit's type, including
// ones (dsp::OutputLimiter) that never defined that method. An earlier
// version of this file used a runtime `enum class Recovery` for exactly
// that reason, and six of ten test binaries failed to compile as a result.
//
// WHY THIS FILE, NOT dsp/FilterFx.hpp: FilterFx.hpp's own header comment
// declares it a *port* -- "a **copy** of the cited Froggers
// formulas" -- of firmware DSP. These two tag types are a general-
// purpose recovery-policy vocabulary shared by FilterFx.hpp's own
// FilterFxChain, Drive.hpp's FrogBlock, and FroggersAppCore.hpp -- not a
// ported filter formula. Same reasoning dsp/Limiter.hpp's own header
// comment gives for why OutputLimiter is kept out of FilterFx.hpp
// ("out of a file whose own header comment scopes it to comb/peak/scoop
// routing"); this file follows that precedent rather than re-arguing it.

namespace synth_froggers::dsp {

// The Tier 2 recovery ceiling. It is DERIVED, not measured, and re-derived here rather than
// re-tuned by feel:
//   - The Filter chain's own input is the Drive page's output,
//     `dsp::DriveBlendPhase::Process` (dsp/Drive.hpp), which ends in
//     `outputLimiter.Process(blended)`, configured to
//     `dsp::kStageCeiling` (0.80, dsp/Limiter.hpp).
//     `dsp::PadeSaturator::Saturate` only clamps the comb saturator's
//     own output, inside this chain, not the Drive page's -- the Drive
//     output limiter is what bounds the input here. Measured
//     (`DriveBlendPhase` run standalone at 48 kHz, Wet/Dry at 1.0 -- any
//     lower setting only trades power between the dry and wet legs,
//     `FlooredEqualPowerBlend`'s own equal-power law, so 1.0 is the
//     largest this stage's output can be driven -- Phase at its own
//     registered default 0.86, dry and wet both a full-scale 220 Hz
//     sine): max|out| 0.796730, against the 0.80 ceiling.
//   - The comb's fed-back term is at most `|fb|/combDrive`
//     (`Comb::Process`'s own comment, dsp/FilterFx.hpp): 0.95/0.25 ==
//     3.8 at the bottom of Comb drive, `Comb::GetFeedback`'s own
//     `kMaxFeedbackMagnitude` (0.95) over `RouteFilterBank`'s Comb-drive
//     floor (0.25).
//   - At Topology 1 (`FilterFxChain::Process`'s own comment,
//     dsp/FilterFx.hpp) the peak's input is the trimmed comb branch, not
//     the chain's raw input, and Peak gain redrawn every sample --
//     `RouteFilterBank`'s own per-sample cadence for every Filter-bank
//     knob -- drives the peak past the steady-state gain a held-fixed
//     height would settle to.
// One run measures what those three combine to, directly, rather than
// composed by hand: `FilterFxChain` at 48 kHz, Comb feedback knob 1
// (0.95), Comb drive knob 0 (0.25), Topology 1, Peak freq and Comb delay
// at their registered defaults (100 Hz each -- Comb delay maps to 480
// samples at 48 kHz, the same 100 Hz pitch), Peak gain at
// `kMaxResonantBumpHeight` redrawn every sample, a full-scale sine at
// the comb's own 100 Hz pitch, 3 s, rerun to 6 s to check for a
// still-climbing transient: comb and peak are exactly unchanged; the
// scoop notch drifts from 1.000034 to 1.000050, under 0.0001 over the
// extra 3 s, negligible against the margin below and not the
// still-climbing transient the recheck exists to catch.
// `StateMagnitude()` maximum: comb 3.598300, peak 5.707133, scoopNotch
// 1.000034 (scoopMix is 0 at this patch, but `scoopNotch.Process` still
// runs unconditionally -- `RouteFilterBank`'s own comment on why). The
// largest of the three is 5.707133, and 100.0 sits more than 10x above
// it (10x is 57.07133).
// DO NOT retune this constant without re-deriving it from the above.
inline constexpr float kMaxUnitStateMagnitude = 100.0f;

// A unit's sample memory (a delay line, an allpass history) that keeps exact
// counts of the non-finite samples and the samples above the ceiling it
// currently holds. Write() takes the overwritten sample out of the counts and
// adds the new one; Clear() and Resize() zero the memory and the counts; there
// is no other way to change it. Recovery runs once per audio callback, so a
// walk of the whole memory there cost the same at 96 frames as at 4800 and
// took most of a phone's audio budget; AllFinite() and AnyOverCeiling() give
// the walk's answer in constant time.
template <typename Storage>
class WatchedBuffer
{
public:
    // Dynamic storage only.
    void Resize(std::size_t size)
    {
        data_.assign(size, 0.0f);
        nonFiniteCount_ = 0;
        overCeilingCount_ = 0;
    }

    void Clear()
    {
        std::fill(data_.begin(), data_.end(), 0.0f);
        nonFiniteCount_ = 0;
        overCeilingCount_ = 0;
    }

    void Write(std::size_t index, float value)
    {
        Remove(data_[index]);
        data_[index] = value;
        Add(value);
    }

    float operator[](std::size_t index) const { return data_[index]; }
    std::size_t size() const { return data_.size(); }
    auto begin() const { return data_.begin(); }
    auto end() const { return data_.end(); }

    bool AllFinite() const { return nonFiniteCount_ == 0; }
    bool AnyOverCeiling() const { return overCeilingCount_ > 0; }

    // Reads every sample: diagnostics and tests only, never the audio thread.
    float Magnitude() const
    {
        float magnitude = 0.0f;
        for (const float sample : data_)
        {
            magnitude = std::max(magnitude, std::fabs(sample));
        }
        return magnitude;
    }

private:
    static bool OverCeiling(float value) { return std::fabs(value) > kMaxUnitStateMagnitude; }

    void Add(float value)
    {
        nonFiniteCount_ += std::isfinite(value) ? 0 : 1;
        overCeilingCount_ += OverCeiling(value) ? 1 : 0;
    }

    void Remove(float value)
    {
        nonFiniteCount_ -= std::isfinite(value) ? 0 : 1;
        overCeilingCount_ -= OverCeiling(value) ? 1 : 0;
    }

    Storage data_{};
    std::size_t nonFiniteCount_ = 0;
    std::size_t overCeilingCount_ = 0;
};

template <std::size_t Size>
using FixedWatchedBuffer = WatchedBuffer<std::array<float, Size>>;
using DynamicWatchedBuffer = WatchedBuffer<std::vector<float>>;

// Tier 2's question: is this unit's state over the ceiling? See the header
// comment above for why this may test the unit's interface.
template <typename Unit>
bool StateOverCeiling(const Unit& unit)
{
    if constexpr (requires { { unit.StateOverCeiling() } -> std::same_as<bool>; })
    {
        return unit.StateOverCeiling();
    }
    else
    {
        return unit.StateMagnitude() > kMaxUnitStateMagnitude;
    }
}

struct FiniteOnly {};  // Tier 1: reset only if the unit's state has gone non-finite.
struct Magnitude {};   // Tier 2: Tier 1, plus reset after a sustained over-ceiling magnitude.

}  // namespace synth_froggers::dsp
