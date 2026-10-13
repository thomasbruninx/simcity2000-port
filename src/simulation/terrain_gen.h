// Random terrain for a new map: the port of SIMCITY.EXE's generator 0x410430 ("Edit New Map": hills, coast, river, water level, trees, streams)
// and its helpers 0x447040 / 0x4470E0 (hills), 0x4474C0 (coast), 0x447610 (river), 0x447380 (smoothing), 0x446800 / 0x446830 (cliff limiter),
// 0x446EA0 (forests), 0x4477C0 (coast flags). See docs/simulation/terrain-generation.md.
#pragma once
#include "simulation/sim_context.h"

namespace sc2k {

struct TerrainParams {
    bool coast = false;     // an ocean along the high-a edge of the map
    bool river = false;     // a river running down the middle
    int hills = 20;         // 0..47 (the dialog's "% Hills" = hills * 100 / 47)
    int water = 20;         // 0..47: sea level = (water + 4) / 8
    int trees = 20;         // 0..47: forests = trees^2 / 2
};

// Replaces the terrain of `c.w` (altitudes, tiles, water flags), sets the sea level and the rotation. Draws from rand() (hills, forests, streams) and the LCG
// (coast, river). The cities' buildings are cleared (every tile becomes id 0).
void generateTerrain(SimContext& c, const TerrainParams& p);

}  // namespace sc2k
