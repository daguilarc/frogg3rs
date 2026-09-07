#pragma once

// synth_froggers::dsp::{RGen, RandomShLane, lanes::MakeSource1..5} -- a
// **copy** of src/core/Marbles.hpp's bag/deja-vu core and
// src/core/RGen.hpp's xorshift32, generalized as described below. Ports
// sources 1-5 of six Random S&H sources total -- source #6 is a Sheaf
// GangedRandomLfo, not a Marbles instance.
//
// ============================================================================
// STRUCTURAL CHOICE: src/core/Marbles.hpp hardcodes exactly two
// channels via `m_marbles[2][8]` (:13), `m_filter[2]` (:15), `m_size[2]`,
// `m_index[2]`, `m_dejaVuKnob[2]`, `m_output[2]` (:16-20), one shared
// `RGen m_rgen` (:14), and one shared `float m_probability` (:19); every
// loop is `for (i = 0; i < 2; i++)` (:69,100,117). This port GENERALIZES
// the hardcoded 2 down to a single-lane struct (every `[2]` array becomes
// one plain scalar member, i.e. the per-channel width becomes 1) and gets
// five sources by constructing five independent RandomShLane instances --
// not by adding a runtime/template channel-count parameter. Reasoning: with
// the width forced to 1, "N instances" and "a struct templated on N" are the
// same amount of code, but N independent objects are far easier to seed,
// test, and reason about independently (each one is a complete, ownable
// unit with its own RGen, matching the "each carries a fixed character"
// framing used throughout this file), whereas an N-wide struct would still
// bury five lanes' state in parallel arrays the way Marbles.hpp does today
// -- exactly the shape this port is trying to get away from.
// ============================================================================

#include "DspMath.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace synth_froggers::dsp {

// ----------------------------------------------------------------------
// src/core/RGen.hpp, ported with ONE necessary structural change.
//
// DISCREPANCY FLAGGED: the firmware RGen's
// xorshift32 state is a **static** class member --
// `static uint32_t s_state;` (RGen.hpp:12), defined out-of-line as
// `inline uint32_t RGen::s_state = 0xa341316cu;` (RGen.hpp:66). It is NOT
// per-instance. Every `RGen` object anywhere in the firmware codebase (both
// of Marbles' channels, 08b5fd3:src/core/FroggersEngine.hpp:311, Parameter.hpp:197/207/223,
// 08b5fd3:src/core/AudioPairArState.hpp:117, and every ad-hoc `RGen()` temporary) reads
// and advances that ONE shared stream. Each RGen must be seeded distinctly,
// or the instances emit identical sequences and the sources become clones
// of each other -- but the firmware struct has no
// per-instance seed at all, so "seed distinctly" cannot be satisfied by a
// verbatim copy: five verbatim RGens would not merely correlate, they would
// all be cursors into one interleaved global sequence, each consuming the
// others' draws. This port makes the xorshift32 state an ordinary instance
// member (same recurrence, same constants) and takes a seed in the
// constructor, so distinct per-lane seeds actually produce independent,
// non-interleaved streams -- verified in FroggersDspParityTests.cpp.
struct RGen
{
    explicit RGen(uint32_t seed) : state_(seed != 0u ? seed : 0x6d2b79f5u) {}

    // RGen.hpp:14-27 (NextUInt), same xorshift32 recurrence, per-instance state.
    uint32_t NextUInt()
    {
        uint32_t x = state_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state_ = x;
        return x;
    }

    // RGen.hpp:45-53 (UniGen/UniGenRange), verbatim formulas.
    float UniGen() { return static_cast<float>(NextUInt() >> 8) * (1.0f / 16777216.0f); }
    float UniGenRange(float min, float max) { return min + (max - min) * UniGen(); }

    // RGen.hpp:55-63 (RangeGen), verbatim formula.
    size_t RangeGen(size_t max)
    {
        if (max == 0)
        {
            return 0;
        }
        return static_cast<size_t>(NextUInt() % max);
    }

private:
    uint32_t state_;
};

// ----------------------------------------------------------------------
// The distribution shape every lane, and source 6, apply to a uniform draw.
// Three fixed points: spread 0.5 returns the draw unchanged (uniform),
// spread 1 sends every draw to 0 or 1 (bimodal), spread 0 sends every draw
// to 0.5. Below 0.5 the centred draw v = 2u - 1 is pulled toward the
// centre by |v|^k with k = 1/(2 spread) > 1; above 0.5 it is pushed toward
// the extremes with k = 2(1 - spread) < 1. No spread bounds the output away
// from 0 and 1: an extreme draw stays extreme at every spread, only how
// often a draw lands near an extreme changes.
inline float ShapeSpread(float u01, float spread)
{
    if (spread <= 0.0f)
    {
        return 0.5f;
    }
    const float v = 2.0f * u01 - 1.0f;
    const float k = spread <= 0.5f ? 1.0f / (2.0f * spread) : 2.0f * (1.0f - spread);
    const float magnitude = std::pow(std::fabs(v), k);
    return std::clamp(0.5f + std::copysign(magnitude, v) * 0.5f, 0.0f, 1.0f);
}

// ----------------------------------------------------------------------
// One Random S&H lane: the Marbles bag/deja-vu core (src/core/Marbles.hpp:67-96
// Increment, :115-121 Process) at a width of one, with its character fixed
// at construction; there are no source-level controls.
//
// The bag holds eight values drawn from the lane's own RGen. Each tick
// `Increment()` moves the read index and, by `dejaVuKnob`, decides whether
// the bag changes:
//   - 0.0 to 0.5: the index walks forward by one and the slot it lands on
//     is overwritten with a fresh draw with probability 2(0.5 - knob), so 0
//     takes a fresh value every tick and exactly 0.5 replays the eight
//     values as a locked phrase.
//   - above 0.5: no slot is ever overwritten; the index jumps to a random
//     slot with probability 2(knob - 0.5), otherwise walks forward by one,
//     so 1.0 reads a random slot every tick.
// `Process()` reads the current slot through ShapeSpread at the lane's
// `spread`, snaps it to `quantizeLevels` (0 or 1 disables snapping), and
// slews it with a one-pole low-pass at `filterCutoffCyclesPerSample`. The
// shape runs before the quantizer so a near-bimodal lane snapped to three
// levels rests at the centre level only on the few draws that land there;
// quantizing first would put a third of the draws on the centre level and
// the shape would leave them there. A locked bag's stored values are
// shaped and snapped on every read, the same as fresh ones.
struct RandomShLane
{
    static constexpr size_t kNumSlots = 8;  // src/core/Marbles.hpp:12 (x_numMarbles)

    // Published for the lane's visualizer (app/FroggersRandomShVisualizer.hpp):
    // the raw bag and the read index, pre shape/quantize/slew, the same
    // convention Sheaf's own PopulateUIState methods use. Atomics only, so
    // this DSP file's Sheaf-dependency surface stays at zero.
    struct UiState
    {
        std::atomic<std::size_t> currentIndex{0};
        std::atomic<std::size_t> size{kNumSlots};
        std::array<std::atomic<float>, kNumSlots> slots{};
    };

    RandomShLane(uint32_t seed,
                 float dejaVuKnob,
                 float filterCutoffCyclesPerSample,
                 float spread,
                 int quantizeLevels)
        : rgen_(seed)
        , dejaVuKnob_(dejaVuKnob)
        , spread_(spread)
        , quantizeLevels_(quantizeLevels)
    {
        Reseed(seed);
        filter_.SetAlphaFromNatFreq(filterCutoffCyclesPerSample);
    }

    // Rebuilds the generator from `seed` and refills every slot from it.
    // The read index and the slew filter are left alone, so a reseed while
    // the lane is playing glides from the current output instead of
    // clicking.
    void Reseed(uint32_t seed)
    {
        rgen_ = RGen(seed);
        for (size_t i = 0; i < kNumSlots; ++i)
        {
            slots_[i] = rgen_.UniGenRange(0.0f, 1.0f);
        }
    }

    // Marbles.hpp:67-96 (Increment) at width one; the deja-vu branch is
    // Marbles.hpp:76 (`if (0.5 < m_dejaVuKnob[i])`).
    void Increment()
    {
        ++tickCount_;
        if (0.5f < dejaVuKnob_)
        {
            if (rgen_.UniGen() < 2.0f * (dejaVuKnob_ - 0.5f))
            {
                index_ = rgen_.RangeGen(kNumSlots);
            }
            else
            {
                index_ = (index_ + 1) % kNumSlots;
            }
        }
        else
        {
            index_ = (index_ + 1) % kNumSlots;
            if (rgen_.UniGen() < 2.0f * (0.5f - dejaVuKnob_))
            {
                slots_[index_] = rgen_.UniGenRange(0.0f, 1.0f);
            }
        }
    }

    // Ticks received since construction; a reseed does not reset it.
    uint32_t TickCount() const { return tickCount_; }

    // Marbles.hpp:115-121 (Process): the current slot, shaped, snapped,
    // slewed.
    float Process()
    {
        float value = ShapeSpread(slots_[index_], spread_);
        if (quantizeLevels_ > 1)
        {
            const float steps = static_cast<float>(quantizeLevels_ - 1);
            value = std::round(value * steps) / steps;
        }
        return filter_.Process(value);
    }

    void PopulateUiState(UiState& state) const
    {
        state.currentIndex.store(index_, std::memory_order_relaxed);
        state.size.store(kNumSlots, std::memory_order_relaxed);
        for (size_t i = 0; i < kNumSlots; ++i)
        {
            state.slots[i].store(slots_[i], std::memory_order_relaxed);
        }
    }

private:
    RGen rgen_;
    size_t index_ = 0;
    float dejaVuKnob_;
    float spread_;
    int quantizeLevels_;
    uint32_t tickCount_ = 0;
    float slots_[kNumSlots];
    OnePoleLowPass filter_;
};

// ----------------------------------------------------------------------
// Five fixed characters, one per Random S&H source. The tick period is the
// slate's (FroggersModulation.hpp's rate table); deja vu, spread,
// quantization and slew are fixed here. Every axis is monotone from source
// 1 to source 5: the period lengthens, fresh values and jumps get rarer,
// the spread moves from the extremes toward the centre, the level grid
// coarsens toward source 1 and disappears from source 4 on, and the slew
// lengthens.
namespace lanes {

// Slew is the one-pole low-pass on the held value, named by its time
// constant at the reference sample rate: cutoff = 1 / (2 pi tau fs), the
// inverse of OnePoleLowPass::SetAlphaFromNatFreq. A lane is built before
// the slate learns its sample rate, so at 96 kHz every time constant is
// half of its name.
inline constexpr float kSlewReferenceSampleRate = 48000.0f;
constexpr float SlewCutoff(float tauSeconds)
{
    return 1.0f / (6.28318530717958647692f * tauSeconds * kSlewReferenceSampleRate);
}
inline constexpr float kFastCutoff = 0.45f;  // cycles/sample: a third of a sample, near-instant
inline constexpr float kSlew5ms = SlewCutoff(0.005f);
inline constexpr float kSlew20ms = SlewCutoff(0.02f);
inline constexpr float kSlew100ms = SlewCutoff(0.1f);
inline constexpr float kSlew200ms = SlewCutoff(0.2f);

// #1: three ticks per quarter note; a fresh value on every tick; nearly
// every value lands at an extreme (spread 0.95), snapped to three levels
// so about one tick in sixteen rests at the centre; no slew.
inline RandomShLane MakeSource1(uint32_t seed)
{
    return RandomShLane(seed, /*dejaVuKnob=*/0.0f, kFastCutoff, /*spread=*/0.95f, /*quantizeLevels=*/3);
}

// #2: two ticks per quarter note; a fresh value on half the ticks; values
// favour the extremes (spread 0.85), snapped to five levels; 5 ms slew.
inline RandomShLane MakeSource2(uint32_t seed)
{
    return RandomShLane(seed, 0.25f, kSlew5ms, 0.85f, 5);
}

// #3: one tick per quarter note; eight held values read at a random slot
// on every tick; values lean to the extremes (spread 0.7), snapped to
// eight levels; 20 ms slew.
inline RandomShLane MakeSource3(uint32_t seed)
{
    return RandomShLane(seed, 1.0f, kSlew20ms, 0.7f, 8);
}

// #4: one tick per two quarter notes, a four-bar phrase of eight held
// values whose read position jumps on half the ticks; values lean slightly
// to the extremes (spread 0.6), unsnapped; 100 ms slew.
inline RandomShLane MakeSource4(uint32_t seed)
{
    return RandomShLane(seed, 0.75f, kSlew100ms, 0.6f, 0);
}

// #5: one tick per four quarter notes, an eight-bar phrase read in order
// with one jump every fifty ticks; values hug the centre (spread 0.4),
// unsnapped; 200 ms slew.
inline RandomShLane MakeSource5(uint32_t seed)
{
    return RandomShLane(seed, 0.51f, kSlew200ms, 0.4f, 0);
}

}  // namespace lanes
}  // namespace synth_froggers::dsp
