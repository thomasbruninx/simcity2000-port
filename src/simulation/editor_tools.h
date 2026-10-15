// The city editor's tools (SCURK's Place & Print): no costs, no game constraints. The port's own functions on top of the verified primitives
// (docs/scurk.md); they are here, not in the interface, so that they can be tested.
#pragma once
#include <vector>

#include "simulation/sim_context.h"
#include "simulation/tools.h"

namespace sc2k {

// Demolish/Clear: removes what stands on (a, b) - a building, tree or small object - without rubble; terrain and zoning stay and the funds are not touched.
// Underground (the view shows pipes and subway) it is the bulldozer's underground mode, also free.
void editorDemolish(SimContext& c, ToolState& ts, int a, int b, bool underground);

// Footprint (tiles per side) of an object id: buildings (>= 0x70) have the table's size, everything else is one tile.
int editorObjectSize(const SimContext& c, int id);

// Places object `id` with (a, b) as the tile that was clicked (big buildings are centred like in the game): what stood on the footprint is removed first and
// terrain, water and neighbours are ignored. False when the footprint leaves the map.
bool editorPlaceObject(SimContext& c, ToolState& ts, int a, int b, int id);

// The crossing types of the bridge question: bits 0..4 are the road and rail bridges, 5 and 6 the highway bridges (mask as passed to ZoneHooks::chooseBridge).
std::vector<int> bridgeTypesOf(int allowedMask);

}  // namespace sc2k
