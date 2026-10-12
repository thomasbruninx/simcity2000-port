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
    virtual void destroyTile(int /*a*/, int /*b*/, int /*mode*/) {}      // 0x458690 (collapse/fire/explosion effects)
    virtual void spawnHelicopterOrShip(int /*a*/, int /*b*/) {}          // 0x452EF0
    virtual void spawnTrain(int /*a*/, int /*b*/) {}                     // 0x452CE0
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
int routeTraffic(SimContext& c, int a, int b, int zone, int kind, int maxSteps);   // 0x456F70, returns 1 if a destination was reached

// ---- the pass itself ----
void runZonePass(SimContext& c, int a0, int b0);                         // 0x435220 (zones 0..6; zones >= 7 not ported yet)

}  // namespace sc2k
