// Shared context of the tile-level simulation routines (zone growth, traffic, building placement).
#pragma once
#include "simulation/city_state.h"
#include "simulation/rng.h"
#include "simulation/tables.h"
#include "simulation/tile_queue.h"
#include "world/world.h"

namespace sc2k {

// Subsystems that are not ported yet (or are UI) are reached through these hooks; the defaults do nothing, exactly like
// the stubs used when the original code is run in the differential-test emulator.
struct ZoneHooks {
    virtual ~ZoneHooks() = default;
    virtual void scrollTo(int /*a*/, int /*b*/) {}                       // 0x440C90 (UI)
    virtual void postMessage(int /*id*/, int /*arg*/) {}                 // 0x47BA90 (advisor/news queue)
    virtual void uiFlag(int /*which*/) {}                                // 0x441800 (toolbar 'rebuild' flags)
    virtual void spawnHelicopterOrShip(int /*a*/, int /*b*/) {}          // 0x452EF0
    virtual void spawnTrain(int /*a*/, int /*b*/) {}                     // 0x452CE0
    virtual void spawnAircraft(int /*a*/, int /*b*/) {}                  // 0x452500 (airport)
    virtual void spawnPlane(int /*a*/, int /*b*/, int /*dir*/) {}        // 0x452190 (airport)
    virtual void spawnSeaTraffic(int /*a*/, int /*b*/) {}                // 0x452660 (seaport pier)
    virtual int thingLimitA() { return 0; }                              // DAT_004E89C8 (read by the 0xED handler)
    virtual int thingLimitB() { return 0; }                              // DAT_004E89C0 (read by the 0xF8 handler)
};

// Ridership statistics written by the traffic router.
struct TrafficStats {
    int32_t bus = 0;      // @0x4C75B8
    int32_t rail = 0;     // @0x4C75BC
    int32_t subway = 0;   // @0x4C7618
};

struct SimContext {
    World& w;
    CityState& s;
    const GameTables& t;
    CrtRand& rng;
    Lfsr& lfsr;
    ZoneHooks& hooks;
    TrafficStats& traffic;
    TileQueue queue;
};

// Tile-id classes shared by several routines (VERIFIED ranges; they overlap on purpose).
inline bool isRoad(int id) { return (id > 0x1C && id < 0x2C) || (id > 0x3E && id < 0x47) || id == 0x4B || id == 0x4C || (id > 0x5C && id < 0x61); }
inline bool isRail(int id) { return (id > 0x2B && id < 0x3F) || (id > 0x44 && id < 0x49) || (id > 0x6B && id < 0x70) || id == 0x4D || id == 0x4E; }
inline bool isBridge(int id) { return (id > 0x50 && id < 0x5D) || id == 0x6A || id == 0x6B; }
inline bool isHighway(int id) { return (id > 0x60 && id < 0x6C) || (id > 0x48 && id < 0x51); }
inline bool isTunnelRoad(int id) { return id > 0x3E && id < 0x43; }
inline bool isSubwayPiece(int u) { return (u != 0 && u < 0x10) || u == 0x23 || u == 0x1F || u == 0x20 || u == 0x22; }

// ---- tile primitives (docs/simulation/zone-growth.md) ----
void setTile(SimContext& c, int a, int b, int id);                       // 0x4418B0
void setUnderground(SimContext& c, int a, int b, int value);             // 0x441D20
bool hasRoadNearby(const SimContext& c, int a, int b);                   // 0x456E80
bool poweredNearby(const SimContext& c, int a, int b);                   // 0x436680
bool canBuildAt(const SimContext& c, int a, int b, int alt, int zone, int limit);   // 0x437AE0
void stampBuilding(SimContext& c, int a, int b, int size, int level);    // 0x436800
void collapseBuilding(SimContext& c, int a, int b, int size, int flip);  // 0x4376C0
void growBuilding(SimContext& c, int a, int b, int size, int zone);      // 0x436F40 (+0x437910)
void clearLotForArcology(SimContext& c, int a, int b);                   // 0x436C90
int routeTraffic(SimContext& c, int a, int b, int zone, int kind, int maxSteps);

// ---- destroy effects (docs/simulation/destroy-effects.md) ----
int footprintOf(const SimContext& c, int& x, int& y, int id);           // 0x45AB10: footprint size, moves (x, y) to the anchor corner
int bridgeSegment(const SimContext& c, int a, int b);                    // 0x4635D0
void releaseTextOverlay(SimContext& c, int overlay);                     // 0x44C850
void fixShore(SimContext& c, int a, int b);                              // 0x4469C0
void destroyTile(SimContext& c, int a, int b, int animate);              // 0x458690 (animation drawing is a presentation hook; its rand() calls are kept)   // 0x456F70, returns 1 if a destination was reached

// ---- military / airport / seaport placement (docs/simulation/special-zones.md) ----
void clearSpecialBuilding(SimContext& c, int a, int b);                  // 0x438C20
bool placeSpecialBuilding(SimContext& c, int a, int b, int id, int size);   // 0x440600 (microsim allocation: see special.cpp)
bool placeZoneBuilding(SimContext& c, int a, int b, int id, int zone);   // 0x437C30

// ---- the pass itself ----
void runZonePass(SimContext& c, int a0, int b0);                         // 0x435220 (all zone types)

}  // namespace sc2k
