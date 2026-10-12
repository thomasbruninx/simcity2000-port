// Power-supply scan, a literal port of SIMCITY.EXE build .022 functions 0x4671F0 (scan) and 0x467340 (flood).
// See docs/simulation/monthly-phases.md. Evidence: decompilation (VERIFIED reading); equivalence with the running game
// is only tested statistically against saved cities (docs/validation/).
#pragma once
#include "simulation/rng.h"
#include "world/world.h"

namespace sc2k {

struct PowerInputs {
    uint32_t ordinances = 0;   // @0x4C9A58; bit 0x10000 = +1/12 supply
    uint8_t wind = 0;          // @0x4C9E24  (wind tile output = (altitude + rand % (wind/8 + 1)) / 2)
    uint8_t cloudiness = 0;    // @0x4CA3EC  (solar tile output = 5 + rand % ((100 - cloud) / 10))
};

struct PowerResult {
    int capacity = 0;          // @0x4C83C8 : summed plant output of every flood performed
    int served = 0;            // @0x4C9A68 before the percentage step : tiles actually supplied
    int percentUsed = 0;       // @0x4C9A68 after: served*100/capacity, 100 if no capacity, clamped to 100
    int poweredTiles = 0;
};

// Clears marks/powered flags, floods power from every unpowered plant tile (ids 0xC6..0xCF), updates XBIT in place.
PowerResult runPowerScan(World& w, const PowerInputs& in, CrtRand& rng);

}  // namespace sc2k
