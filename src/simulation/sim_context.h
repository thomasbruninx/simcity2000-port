// Shared context of the tile-level simulation routines (zone growth, traffic, building placement).
#pragma once
#include <vector>

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
    virtual void terrainProgress(int /*stage*/) {}                         // 0x414E50: the terrain generator enters stage 0..8 (strings 801..809)
    virtual void scrollTo(int /*a*/, int /*b*/) {}                       // 0x440C90 (UI)
    virtual void newsAge() {}                                            // 0x479F40: the news queue ages (called once a month from the event phase)
    virtual void postMessage(int /*id*/, int /*arg*/) {}                 // 0x47BA90 (advisor/news queue)
    virtual void uiFlag(int /*which*/) {}
    // 25-byte building-name template (game text, loaded from the data files) for a building id; nullptr = all zeros.
    virtual const uint8_t* labelTemplate(int /*id*/, bool /*custom*/) { return nullptr; }
    virtual bool specialCityName() { return false; }
    virtual void showDialog(int /*stringId*/) {}                           // FUN_004B1243 message boxes
    virtual void scheduledEvent(int /*index*/) {}                          // 0x469A10: a dated event fired (unlocks tools; UI only)
    virtual void milestoneReached(int /*newIndex*/) {}                     // phase 22: city class changed (0x413D80 may grant a military base; dialogs)
    virtual void gameOver(int /*kind*/) {}                                 // 0x414470: 0 lost scenario, 1 won, 2 bankrupt
    virtual bool confirm(int /*stringId*/, int /*amount*/ = 0) { return false; }               // yes/no message box (neighbour connection fee, ...)
    virtual int chooseBridge(int /*length*/, int allowedMask) { return allowedMask ? __builtin_ctz(static_cast<unsigned>(allowedMask)) : -1; }   // 0x45B140 dialog: bridge type or -1
    virtual void toolMessage(int /*id*/) {}                                // 0x45AFD0 / 0x408600 status or error text
    // 0x47CE40 "Pick a Team and Name It" after a stadium is built: `takenMask` = sports that already have a team (bit k = sport k, MISC 1034);
    // the dialog offers the free sports (all when every one is taken). sport < 0 = closed without choosing.
    struct StadiumChoice { int sport = -1; std::string name; };
    virtual StadiumChoice chooseStadiumTeam(int /*takenMask*/) { return {}; }
    virtual void playSound(int /*id*/, int /*repeat*/ = 0) {}                // 0x424AA0 (repeat 0) / 0x424AD0: sound effect 500 + n (SOUNDS/5xx.WAV)
    virtual void yearlyBudget() {}                                          // 0x4736C0 at the start of a new budget year (before the settlement): music + the annual budget dialog
    virtual void stopSound(int /*id*/) {}
    virtual uint32_t tickCount() { return 0; }                              // GetTickCount (real-time gates of some sounds); 0 = none
    virtual void playMusic(int /*id*/) {}                                  // 0x424B30: start a MIDI tune (10000 + n)                                   // 0x424AF0
                         // true when the city name equals a built-in string (strcmp == 0 in 0x44D740 / 0x44DD10 for id 0xFF)                                // 0x441800 (toolbar 'rebuild' flags)
};

// Ridership statistics written by the traffic router.
struct TrafficStats {
    int32_t bus = 0;      // @0x4C75B8
    int32_t rail = 0;     // @0x4C75BC
    int32_t subway = 0;   // @0x4C7618
};

// Runtime globals of the thing system (ships, aircraft, trains, monsters, ...). None of them is stored in a save file.
struct ThingState {
    int animFrame = 0;                          // 0x4E70CC frame counter (wraps at 0x400)
    int planes = 0, helis = 0, ships = 0;       // 0x4E89B0 / B4 / B8 (counted again every frame by the updaters)
    int visitors = 0, sailboats = 0;            // 0x4E89BC / C0
    int monsters = 0, trains = 0, tornados = 0; // 0x4E89C4 / C8 / CC
    int soundGate = 0;                          // 0x4E89D0 (real-time gate of a traffic-report sound; not simulated)
    int focusA = 0, focusB = 0;                 // 0x4C9A70 / 74: location of the latest disaster / event
    int disasterRequest = 0;                    // 0x4C9438: disaster kind requested by a thing (consumed by 0x45C870)
    int fireMessageKind = -1;                   // 0x4E76A4: highest-priority burning building class since the last report (-1 = none)
    int lastSpawned = 0;                        // 0x4C9834: slot of the thing spawned last
    int shipTargetA = 0, shipTargetB = 0;       // 0x4E70BC / 0x4E70C0: pier a ship was sent to
    int floodTimer = 0;                         // 0x4C83C0: counts down while a flood spreads (set to 60 by the flood handlers)
    int floodBudget = 0;                        // 0x4E76A0: random flood-damage rolls left (set to 50 by the big flood)
    int dozerSub = 0;                           // 0x4CB968: sub-tool of the terrain tools; the meteor's terrain raise reads it
    int scenarioFlag = 0;                       // 0x4C9D5C (written by the game-over / scenario code)
    std::vector<uint16_t> spriteHeight;         // word at +4 of each 8-byte sprite record (0x4CA1D0); read from the sprite archives
    int spriteHeightOf(int id) const { return id >= 0 && static_cast<size_t>(id) < spriteHeight.size() ? spriteHeight[static_cast<size_t>(id)] : 0; }
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
    ThingState things{};
    Lcg lcg{1};
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
bool placeSpecialBuilding(SimContext& c, int a, int b, int id, int size, bool unconstrained = false);   // unconstrained: the Place & Print editor ignores terrain, water and existing objects   // 0x440600
int allocateMicrosim(SimContext& c, int id);                             // 0x44C8E0 (+0x44D740): XTXT overlay value of the new record, or 0
int populationLimit(const SimContext& c, int limit, int divisor);        // 0x44F150
bool placeZoneBuilding(SimContext& c, int a, int b, int id, int zone);   // 0x437C30

// ---- thing system (docs/simulation/things.md); drawing is omitted ----
void thingFrame(SimContext& c);                                          // 0x450210: one animation frame of every active thing
void removeThing(SimContext& c, int slot);                               // 0x450150
int freeThingSlot(const SimContext& c);                                  // 0x4501D0: 1..0x27, or 0 when the table is full
void spawnHelicopter(SimContext& c, int a, int b);                       // 0x452500
void spawnPlane(SimContext& c, int a, int b, int dir);                   // 0x452190
void spawnShip(SimContext& c, int pierA, int pierB);                     // 0x452660
void spawnVisitor(SimContext& c, int kind);                              // 0x452900
void spawnEffect(SimContext& c, int a, int b, int alt, int state, int extra);   // 0x452BE0
void spawnSailboatsNearMarina(SimContext& c, int a, int b);              // 0x452CE0
void spawnTrainNearStation(SimContext& c, int a, int b);                 // 0x452EF0
bool spawnTrain(SimContext& c, int a, int b);                            // 0x452F60
bool spawnMonster(SimContext& c);                                        // 0x45E780
bool spawnTornado(SimContext& c);                                        // 0x45EA00
bool startFire(SimContext& c, int a, int b, int flag);                   // 0x460780
void floodTile(SimContext& c, int a, int b);                             // 0x446000

void refreshPower(SimContext& c);                                       // phase 1 body (0x4671F0): also run by building tools while the city is small
void refreshWater(SimContext& c);                                       // phase 20 body (0x463EE0)
void runEconomy(SimContext& c);
void yearlyMicrosimUpdate(SimContext& c);                               // 0x44DD10: January upkeep of microsim buildings (power plants age and explode, capacities, city-size bonus)
int findOverlayTile(const SimContext& c, int overlay);                  // 0x44F210: y*256 + x of the tile carrying this XTXT overlay, or 0
void runEvents(SimContext& c);                                           // second half of 0x467AE0: fire spread, news triggers, weather
void recordHistory(SimContext& c);                                       // 0x413910 graph history
void trafficDecay(SimContext& c);                                        // 0x458040 (phase 19)
// ---- disasters (docs/simulation/disasters.md) ----
bool startDisaster(SimContext& c, int kind);                             // the switch of 0x45C870: handler for one disaster kind 1..0x12, true when it happened
bool disasterFrame(SimContext& c);                                       // 0x45F0D0: spreads / extinguishes fire, flood and crash markers; false when nothing is left burning
void disasterTick(SimContext& c);                                        // 0x45C870: consumes the pending request, runs the frame while a disaster is active, posts the end-of-disaster message
void chooseDisaster(SimContext& c);                                      // 0x4720A0: advisor topic + random disaster request (phase 24)
void simulationTick(SimContext& c);                                      // 0x412FE0 (UI omitted)                                          // 0x4686C0 economy / job-market model (phase 21)

// ---- the pass itself ----
void runZonePass(SimContext& c, int a0, int b0);                         // 0x435220 (all zone types)

}  // namespace sc2k
