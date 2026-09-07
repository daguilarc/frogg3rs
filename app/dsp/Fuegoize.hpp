#pragma once

// synth_froggers::dsp::{Fuegoize, FuegoStack} -- a **copy** of the cascade
// shape from the retired simulator's V2FuegoStack.hpp:9-23 (ApplyGlobal :9-12, ApplyMusicalRow
// :14-23), but with the scramble transform itself pinned to the DAISY
// FIRMWARE's defined behavior, NOT the retired simulator's Fuegoize.hpp -- see the
// discrepancy note below.
//
// PARITY REFERENCE DISCREPANCY (confirmed by direct reading of both
// files):
//   The retired simulator's Fuegoize.hpp:22-23 computed the modulo divisor as
//     `static_cast<uint8_t>((mask + 1u) ? (mask + 1u) : 1u)`
//   i.e. the CAST-TO-uint8_t is applied to the ternary's already-selected
//   result. At knob >= 0.9375, round(knob*8) rounds to 8, so
//   mask = (1<<8)-1 = 255, and (mask+1u) computed at unsigned-int width is
//   256 (truthy) -- the ternary picks 256 -- and THEN the cast truncates
//   256 to uint8_t, wrapping to 0. The next expression is `row % 0`:
//   undefined behavior, confirmed by UBSan, and observed to return
//   different values at -O0 vs -O2 for the same input.
//   08b5fd3:src/core/Parameter.hpp:129-151 (the Daisy firmware's own inline
//   fuegoize) does NOT have this bug: line 142 is
//     `uint8_t sh = 1u + (uint8_t)(m_position % ((mask + 1) ? (mask + 1) : 1));`
//   Here the cast is on the RESULT of the modulo, not on the divisor --
//   `mask + 1` stays at int width (up to 256) for the `%` itself, so the
//   divisor is never truncated to 0. This is the correct, UB-free formula,
//   and it is what this file ports: 08b5fd3:src/core/Parameter.hpp:142, not
//   the retired simulator's Fuegoize.hpp:23.

#include <cmath>
#include <cstdint>

namespace synth_froggers::dsp {

// 08b5fd3:src/core/Parameter.hpp:129-151, adapted from Parameter::Get's inline
// scramble to a free function taking (value, fuegoization knob, row) --
// the same signature the retired simulator's V2FuegoStack.hpp Fuegoize already used, so the
// cascade below (FuegoStack) reads the same as that V2FuegoStack.hpp:9-23.
//
// CALLER CONSTRAINT (not stated in the firmware source): `row` must stay
// within the encoder-grid domain this app actually uses, 0-15 (a bank slot
// index, crispyRow fixed at 14). The algorithm shifts `lowerBits` by `sh` bits,
// and `sh` can reach `1 + row` when the fuego knob is high enough to make
// `mask == 255`; once `sh` reaches a 32-bit int's width the shift is
// undefined behavior. This is a latent property of the algorithm itself
// (identical in both references this file is pinned to), not
// something introduced or fixed by this port -- it simply never fires
// because every real caller's row is 0-15.
inline float Fuegoize(float value, float fuegKnob, uint8_t row)
{
    if (fuegKnob <= 0.0f)
    {
        return value;
    }

    const float fuegoizationAmount = fuegKnob;
    const uint16_t mask = static_cast<uint16_t>(
        (1u << static_cast<uint16_t>(std::round(fuegoizationAmount * 8.0f))) - 1u);
    const uint16_t inputInt = static_cast<uint16_t>(value * 255.0f);
    const float inputRemainder = value * 255.0f - static_cast<float>(inputInt);
    uint16_t lowerBits = static_cast<uint16_t>(inputInt & mask);

    lowerBits ^= static_cast<uint16_t>((lowerBits << 3) & mask);
    lowerBits ^= static_cast<uint16_t>((lowerBits >> 5) & mask);
    lowerBits ^= static_cast<uint16_t>((lowerBits << 1) & mask);

    // 08b5fd3:src/core/Parameter.hpp:142 -- cast the RESULT of the modulo, not the divisor.
    // `mask + 1u` is computed here at uint32_t width (never truncated), so
    // at mask == 255 the divisor is genuinely 256, matching the firmware
    // (where `mask + 1` is computed at int width for the same reason).
    const uint32_t divisor = (static_cast<uint32_t>(mask) + 1u) != 0u ? (static_cast<uint32_t>(mask) + 1u) : 1u;
    const uint8_t sh = static_cast<uint8_t>(1u + static_cast<uint8_t>(row % divisor));
    lowerBits ^= static_cast<uint16_t>((lowerBits >> sh) & mask);

    const uint16_t outInt = static_cast<uint16_t>((inputInt & ~mask) | lowerBits);
    return (static_cast<float>(outInt) + inputRemainder) / 255.0f;
}

// The retired simulator's V2FuegoStack.hpp:9-23, verbatim cascade shape (only Fuegoize's body
// changed, per the discrepancy note above).
namespace FuegoStack {

// The retired simulator's V2FuegoStack.hpp:9-12.
inline float ApplyGlobal(float value, float globalCrunchy, uint8_t row)
{
    return Fuegoize(value, globalCrunchy, row);
}

// The retired simulator's V2FuegoStack.hpp:14-23.
inline float ApplyMusicalRow(float value,
                              float globalCrunchy,
                              float crispyKnobPreFuego,
                              uint8_t row,
                              uint8_t crispyRow)
{
    const float afterCrunchy = ApplyGlobal(value, globalCrunchy, row);
    const float crispyAfterCrunchy = ApplyGlobal(crispyKnobPreFuego, globalCrunchy, crispyRow);
    return Fuegoize(afterCrunchy, crispyAfterCrunchy, row);
}

}  // namespace FuegoStack
}  // namespace synth_froggers::dsp
