// Water-supply scan, a literal port of SIMCITY.EXE build .022 functions 0x463EE0 (scan, 4 rotation orders)
// and 0x464370 (flood). See docs/simulation/monthly-phases.md.
#pragma once
#include "world/world.h"

namespace sc2k {

struct WaterInputs {
    int rotation = 0;       // MISC[2]: selects the iteration order of the outer scan (0..3)
    uint8_t cloudiness = 0; // MISC[26] @0x4CA3EC : pump output includes cloudiness/2
    int16_t waterFactor = 0;// MISC[912] @0x4C9830: pump output includes factor*5
};

struct WaterResult {
    unsigned served = 0;     // @0x4CB4D0 before the percentage step
    unsigned supply = 0;     // @0x4C83C4
    int percent = 0;         // @0x4CB4D0 after: served*100/supply (100 if no supply)
    int wateredTiles = 0;
};

// Clears watered/mark flags (towers 0xEB keep their stored-water flag), floods from every powered, not-yet-watered
// pump (0xDC) / desalination plant (0xFA), updates XBIT in place.
WaterResult runWaterScan(World& w, const WaterInputs& in);

}  // namespace sc2k
