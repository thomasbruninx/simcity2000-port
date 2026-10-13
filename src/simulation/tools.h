// City-building tools: bulldozer, zone painting, networks (road, rail, power line, pipes, subway) with bridges, and
// single buildings. Literal ports of SIMCITY.EXE build .022 routines 0x442C20, 0x448F70, 0x449530/0x449C90/0x4498D0/0x449F20/0x44A0A0,
// the path planner 0x448540/0x4488D0/0x4487A0, the piece appliers 0x44A640/0x44A220/0x44AB20/0x44B110/0x44B4A0, the neighbour
// fixers 0x44A850/0x44AD20/0x44A400/0x44B250/0x44B5C0/0x44C120/0x44C330, the bridge builder 0x45B280 and the building placer 0x43FB20
// (see docs/simulation/tools.md). The interactive parts of the originals (mouse loops, dialogs) are replaced by explicit
// parameters and ZoneHooks callbacks; everything that changes the city is the original logic.
#pragma once
#include <utility>
#include <vector>

#include "simulation/sim_context.h"

namespace sc2k {

enum class NetworkKind : int { Road = 0, Rail = 1, Power = 2, Subway = 3, Pipe = 4 };   // DAT_004C6DA0 values

// Tool categories of the original toolbar (DAT_004CA47C), also the row of the cost / id tables.
namespace toolcat {
constexpr int kBulldozer = 0, kPower = 3, kWater = 4, kServices = 5, kRoads = 6, kRail = 7, kZoneAirSea = 8, kZoneRes = 9, kZoneCom = 10,
              kZoneInd = 11, kEducation = 12, kSafety = 13, kRecreation = 14;
}

// Runtime globals of the tool code (0x4C6D70..0x4C6DA0, 0x4E8E38).
struct ToolState {
    int dir = 0;                 // 0x4C6D78 current step direction 0..3 (b-1, a+1, b+1, a-1)
    int targetA = 0, targetB = 0;   // 0x4C6D94 / 0x4C6D98
    int minA = 0, minB = 0, maxA = 0, maxB = 0;   // 0x4C6D80.. bounding box of the drag
    int kind = -1;               // 0x4C6DA0 NetworkKind / 5 = zoning
    int slopeTiles = 0;          // 0x4C6D70 tiles that must be levelled (extra 25 each)
    int length = 0;              // 0x4C6D9C tiles on the path
    int cost = 0;                // 0x4C6D90
    bool underground = false;    // 0x4E8E38: the underground view is active (bulldozer works on pipes/subway)
};

struct PathPreview {
    bool valid = false;
    int cost = 0, length = 0, slopeTiles = 0;
    std::vector<std::pair<int, int>> tiles;   // tiles in walking order, (a, b)
};

// Bulldozer on one tile (0x442C20). Returns false when nothing was done (no funds, nothing to remove, military, ...).
bool bulldozeTile(SimContext& c, ToolState& ts, int a, int b);

// Zone painting over the rectangle spanned by (a0,b0)-(a1,b1) (0x448F70). `zoneType` 0 = de-zone, 1..6 R/C/I, 8 airport, 9 seaport.
bool paintZone(SimContext& c, ToolState& ts, int a0, int b0, int a1, int b1, int zoneType, int unitCost);

// Cost of painting the rectangle (tiles that would change zone x unit cost, 0x447D00).
int zoneRectCost(SimContext& c, int a0, int b0, int a1, int b1, int zoneType, int unitCost);

// Cost / route of dragging a network from (a0,b0) to (a1,b1) without changing anything (0x448410).
PathPreview previewNetwork(SimContext& c, ToolState& ts, NetworkKind kind, int a0, int b0, int a1, int b1, int unitCost);

// Drags a network (0x449530 road, 0x4498D0 rail, 0x449C90 power, 0x44A0A0 subway, 0x449F20 pipes). `sub` is the subtype (cost row).
bool buildNetwork(SimContext& c, ToolState& ts, NetworkKind kind, int sub, int a0, int b0, int a1, int b1);

// Bridge starting at a shore tile (0x45B280 via 0x45AF70 / 0x45AFB0 / 0x45AF90); mask: 0x1C road, 1 power, 2 rail. flag != 0: choose direction
// from the view rotation (a click on the shore), 0: continue ts.dir (a drag that ended at the shore).
bool buildBridge(SimContext& c, ToolState& ts, int a, int b, int mask, int flag);

// Single-building tools (0x43FB20): power plants, water buildings, services, rewards, schools, safety, recreation, stations.
bool placeBuilding(SimContext& c, ToolState& ts, int category, int sub, int a, int b);
int toolBuildingId(int category, int sub);   // the id the (category, subtype) button places, 0 = none
int toolCost(const SimContext& c, int category, int sub);

// Piece appliers and neighbour fixers, exposed for tests.
void applyRoadPiece(SimContext& c, int a, int b);
void applyPowerPiece(SimContext& c, int a, int b);
void applyRailPiece(SimContext& c, int a, int b);
void applyPipePiece(SimContext& c, int a, int b);
void applySubwayPiece(SimContext& c, int a, int b);

}  // namespace sc2k
