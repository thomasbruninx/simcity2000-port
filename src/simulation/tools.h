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
    int dozerSub = 0;            // 0x4CB968 bulldozer subtype (0 bulldoze, 1 level, 2 raise, 3 lower, 4 de-zone): selects the cost row
    bool shift = false;          // the original reads the keyboard (0x42AD10): shift held while using the tree tools removes trees
    // emergency dispatch markers (0x44F290 / 0x44F4D0 ...): per kind 0 police, 1 fire, 2 military
    int dispatchLimit[3] = {0, 0, 0};
    int dispatchRing[3] = {0, 0, 0};
    uint8_t dispatchX[3][64] = {}, dispatchY[3][64] = {};
    bool dispatchWarned[3] = {false, false, false};
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

// Terrain tools (0x443610 raise, 0x443DF0 lower, 0x442BB0 level to the altitude `target`). Return false when nothing could be done.
bool raiseTerrain(SimContext& c, ToolState& ts, int a, int b);
bool lowerTerrain(SimContext& c, ToolState& ts, int a, int b, bool pay);
bool levelTerrain(SimContext& c, ToolState& ts, int a, int b, int target);

// Highways (0x461B80): 2x2-tile pieces on even coordinates with their own planner; (a1, b1) is the drag end. previewHighway = 0x461200.
bool buildHighway(SimContext& c, ToolState& ts, int a0, int b0, int a1, int b1);
bool buildHighwayBridge(SimContext& c, ToolState& ts, int a, int b, int flag);   // 0x45BB30: flag != 0 = a click on a shore, 0 = continuing a drag
int highwayClassify(SimContext& c, int a, int b);   // 0x4639D0 (exposed for tests)
int highwayHeight(SimContext& c, int a, int b);     // 0x4638F0
PathPreview previewHighway(SimContext& c, ToolState& ts, int a0, int b0, int a1, int b1);

// Single-click transport / power tools: hydroelectric dam (0x440DB0), tunnel (0x44B820), road onramp (0x441160), rail-subway link (0x441530).
bool placeHydroDam(SimContext& c, ToolState& ts, int a, int b);
bool buildTunnel(SimContext& c, ToolState& ts, int a, int b, int roadSub);
bool placeOnramp(SimContext& c, ToolState& ts, int a, int b, int roadSub);
bool placeRailLink(SimContext& c, ToolState& ts, int a, int b);

// Landscape tools: tree (0x43F4E0, subtype 0 of category 1, $3), water (0x43F7E0, subtype 1, $100); forest (0x43F620) and stream (0x445DF0) exist in
// the original terrain editor only (free). Return false when nothing was placed; money is charged by `landscapeTool`.
bool plantTree(SimContext& c, ToolState& ts, int a, int b);
void plantForest(SimContext& c, ToolState& ts, int a, int b);
bool placeWater(SimContext& c, ToolState& ts, int a, int b);
void placeStream(SimContext& c, ToolState& ts, int a, int b, int length = 100);
bool landscapeTool(SimContext& c, ToolState& ts, int sub, int a, int b);   // 0x441DF0 body for one tile: cost check, tool, payment

// Emergency dispatch markers (category 2): startDispatch = 0x44F290 (when a disaster begins), then one click per marker (0x44F4D0 police, 0x44F6E0 fire, 0x44F8F0 military).
void startDispatch(SimContext& c, ToolState& ts);
bool placeDispatch(SimContext& c, ToolState& ts, int kind, int a, int b);

// Piece appliers and neighbour fixers, exposed for tests.
void applyRoadPiece(SimContext& c, int a, int b);
void applyPowerPiece(SimContext& c, int a, int b);
void applyRailPiece(SimContext& c, int a, int b);
void applyPipePiece(SimContext& c, int a, int b);
void applySubwayPiece(SimContext& c, int a, int b);

}  // namespace sc2k
