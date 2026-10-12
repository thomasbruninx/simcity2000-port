// Known-answer vectors come from tools/rng_model.py (independent model of the disassembly).
#include "test.h"
#include "simulation/rng.h"
#include <array>

using namespace sc2k;

TEST(lcg_seed1_mod100) {
    Lcg r(1);
    const std::array<int, 8> want{38, 58, 13, 15, 51, 27, 10, 19};
    for (int w : want) CHECK(r.mod(100) == w);
}
TEST(lcg_seed12345679_mod200) {
    Lcg r(0x12345679);
    const std::array<int, 8> want{167, 178, 131, 146, 94, 102, 105, 13};
    for (int w : want) CHECK(r.mod(200) == w);
}
TEST(lcg_state_update) {
    Lcg r(1); r.mod(100);
    CHECK(r.state() == 1u * 0x41C64E6Du + 0x3039u);
}
TEST(lfsr_seed1_raw) {
    Lfsr r(1);
    for (int w : {2, 4, 8, 16, 32, 64, 128, 256}) CHECK(r.step() == w);
}
TEST(lfsr_mod128) {
    Lfsr r(0xACE1);
    for (int w : {55, 110, 41, 82, 36, 72, 101, 74}) CHECK(r.mod(128) == w);
}
TEST(lfsr_mask3) {
    Lfsr r(0xACE1);
    for (int w : {3, 2, 1, 2, 0, 0, 1, 2}) CHECK(r.mask(3) == w);
}
TEST(lfsr_seed0_is_fixed_point) {
    Lfsr r(0);
    for (int i = 0; i < 10; ++i) CHECK(r.step() == 0);
}
TEST(lfsr_high_seed_bits_ignored) {
    Lfsr a(0xFFFF0001u), b(1);
    for (int i = 0; i < 100; ++i) CHECK(a.step() == b.step());
}
TEST(lfsr_period_from_seed1_is_57337) {
    Lfsr r(1); int n = 0;
    do { r.step(); ++n; } while (r.state() != 1 && n < 70000);
    CHECK(n == 57337);
}
