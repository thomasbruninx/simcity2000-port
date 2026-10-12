#include "simulation/rng.h"

#include <cassert>

namespace sc2k {

int16_t Lcg::mod(int16_t n) {
    assert(n != 0);
    state_ = state_ * 0x41C64E6Du + 0x3039u;
    const int32_t r = static_cast<int32_t>((state_ >> 16) & 0x7FFFu);
    return static_cast<int16_t>(r % static_cast<int32_t>(n));  // r >= 0, so sign matches 16-bit IDIV
}

uint16_t Lfsr::step() {
    const bool top = (state_ & 0x8000u) != 0;
    state_ = static_cast<uint16_t>(state_ << 1);
    if (top) state_ ^= 0x1BF5u;
    return state_;
}

int16_t Lfsr::mod(int16_t n) {
    assert(n != 0);
    const int32_t s = step();
    return static_cast<int16_t>(s % static_cast<int32_t>(n));
}

}  // namespace sc2k
