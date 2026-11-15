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
// Recovery policy is shared by the filter, drive, VCO and app-level units;
// keep its tags and watched storage independent of any one DSP chain.

namespace synth_froggers::dsp {

// Tier 2 retains more than ten times the largest state observed in the
// redesign's measured stress fixtures. At 192 kHz, two seconds of a full-
// scale 100 Hz sine through both Q=5.5 SVFs and the .95-feedback comb
// (drive .25, 100 Hz tune) reached main 5.500752, comb 9.284996, peak 56.095894.
// Sweeping the two cutoffs oppositely at 173/41 Hz reached peak 8.715101 /
// 9.405576. These are isolated full-scale DSP stress measurements, not
// claims about every possible modulation trajectory or the app's input.
// Separate real-app heavy-Drive runs retain their per-press observations
// in the change evidence. 1000 exceeds ten times 56.095894; the prior 100
// ceiling's derivation belonged to the removed, trimmed Filter topology.
// Re-derive the ceiling when the stage structure or state range changes.
inline constexpr float kMaxUnitStateMagnitude = 1000.0f;

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
