// Reconstruction of SimCity 2000's two random-number generators (Win95 build .022).
// Behaviour documented in docs/simulation/rng.md (VERIFIED from disassembly). Do not "improve".
#pragma once
#include <cstdint>

namespace sc2k {

// State: 32-bit; next = state * 0x41C64E6D + 0x3039; r = (state >> 16) & 0x7FFF.
class Lcg {
public:
    explicit Lcg(uint32_t seed = 1) : state_(seed) {}
    void seed(uint32_t s) { state_ = s; }
    uint32_t state() const { return state_; }
    // Original: 16-bit signed IDIV; n == 0 faults on the original (precondition here).
    int16_t mod(int16_t n);
private:
    uint32_t state_;
};

// State: 16-bit; shift left, XOR 0x1BF5 when the old top bit was set. Seed 0 is a fixed point.
// Period from seed 1 is 57337 (not maximal) -- preserved quirk.
class Lfsr {
public:
    explicit Lfsr(uint32_t seed = 1) : state_(static_cast<uint16_t>(seed)) {}
    void seed(uint32_t s) { state_ = static_cast<uint16_t>(s); }  // only the low 16 bits are ever used
    uint16_t state() const { return state_; }
    uint16_t step();
    int16_t mod(int16_t n);             // state % n (n sign-extended, state zero-extended)
    uint16_t mask(uint16_t m) { return static_cast<uint16_t>(step() & m); }  // original: &1,&3,&0xF,&0x3F,&0x7F
private:
    uint16_t state_;
};

}  // namespace sc2k
