#include "test.h"
#include "simulation/sim_context.h"
#include "save/error.h"
#include "world/world_hash.h"

#include <cstdlib>
#include <string>

using namespace sc2k;

namespace {
GameTables zoneTables() {
    GameTables t;
    t.dirDx = {0, 1, 0, -1}; t.dirDy = {-1, 0, 1, 0};
    t.dirNextMask = {11, 7, 14, 13};
    t.zoneDestMask[1] = 0x0018; t.zoneDestMask[2] = 0x0018;   // arbitrary: residential trips end at commercial zones (3,4)
    t.sizePop = {0, 1, 8, 12, 36, 0, 0, 0};
    t.buildBase[1] = 112; t.buildRange[1] = 12;
    t.buildBase[13] = 136; t.buildRange[13] = 2;
    t.buildBase[17] = 138; t.buildRange[17] = 2;
    for (size_t i = 0; i < 4; ++i) t.rotationCorner[i * 2] = static_cast<uint8_t>(16u << i);
    t.rotationZoneMask = {0x80, 0x10, 0x20, 0x40};
    return t;
}
struct Fixture {
    World w{}; CityState s{}; GameTables t = zoneTables(); CrtRand rng{1}; Lfsr lfsr{0xACE1}; ZoneHooks hooks; TrafficStats ts;
    SimContext ctx{w, s, t, rng, lfsr, hooks, ts, {}};
};
}  // namespace

TEST(zone_set_tile_updates_census_and_military_counters) {
    Fixture f;
    f.w.xbld.at(5, 5) = 0x10; f.s.census[0x10] = 3;
    setTile(f.ctx, 5, 5, 0x70);
    CHECK(f.s.census[0x10] == 2 && f.s.census[0x70] == 1 && f.w.xbld.at(5, 5) == 0x70);
    f.w.xzon.at(6, 6) = 7; f.w.xbld.at(6, 6) = 0xE3;                 // military tile: separate counters
    setTile(f.ctx, 6, 6, 0xEF);
    CHECK(f.s.militaryCounts[6] == -1 && f.s.militaryCounts[3] == 1 && f.s.census[0xE3] == 0);
    setTile(f.ctx, 200, 5, 0x10);                                     // out of range: ignored
}
TEST(zone_stamp_size_two_writes_a_block_with_flags_and_corner_bits) {
    Fixture f;
    f.t.buildBase[2] = 140; f.t.buildRange[2] = 4;
    stampBuilding(f.ctx, 10, 10, 2, 0);                               // block x in [10,11], y in [9,10]
    for (int x = 10; x <= 11; ++x) for (int y = 9; y <= 10; ++y) {
        CHECK(f.w.xbld.at(x, y) >= 140 && f.w.xbld.at(x, y) < 144);
        CHECK((f.w.xbit.at(x, y) & 0xE0) == 0xE0);
    }
    CHECK((f.w.xzon.at(10, 9) & 0xF0) == 16 && (f.w.xzon.at(11, 9) & 0xF0) == 32);
    CHECK((f.w.xzon.at(11, 10) & 0xF0) == 64 && (f.w.xzon.at(10, 10) & 0xF0) == 128);
    CHECK(f.w.xbld.at(12, 10) == 0);
}
TEST(zone_collapse_and_clear_lot_primitives) {
    Fixture f;
    f.t.buildBase[2 + 16] = 150; f.t.buildRange[2 + 16] = 3;
    collapseBuilding(f.ctx, 20, 20, 1, 0);
    CHECK(f.w.xbld.at(20, 20) >= 138 && f.w.xbld.at(20, 20) < 140);
    f.w.xzon.at(40, 40) = 0x25;
    clearLotForArcology(f.ctx, 40, 40);
    CHECK(f.w.xbld.at(40, 40) == 0xF7 && f.w.xbld.at(41, 39) == 0xF7);
    CHECK((f.w.xzon.at(40, 40) & 0xF) == 0 && (f.w.xzon.at(40, 39) & 0xF0) == 16);
}
TEST(zone_helpers_road_and_power_neighbourhood) {
    Fixture f;
    CHECK(!hasRoadNearby(f.ctx, 50, 50));
    f.w.xbld.at(52, 51) = 0x1D;
    CHECK(hasRoadNearby(f.ctx, 50, 50));
    CHECK(!hasRoadNearby(f.ctx, 50, 60));
    f.w.xbit.at(1, 1) = xbit::kPowered;
    CHECK(poweredNearby(f.ctx, 1, 1));
    f.w.xbit.at(30, 30) = xbit::kPowered;
    CHECK(poweredNearby(f.ctx, 30, 31) && poweredNearby(f.ctx, 31, 30) && !poweredNearby(f.ctx, 32, 30));
}
TEST(zone_traffic_router_finds_destination_along_road_and_adds_traffic) {
    Fixture f;
    // origin zone tile at (10,10) type 1; road along b = 11..15; commercial zone (type 3) tile at (10,16)
    for (int b = 11; b <= 15; ++b) f.w.xbld.at(10, b) = 0x1D;
    f.w.xzon.at(10, 16) = 3;
    int r = routeTraffic(f.ctx, 10, 10, 1, 2, 100);
    CHECK(r == 1);
    int sum = 0; for (auto v : f.w.xtrf.v) sum += v;
    CHECK(sum >= 2);               // at least one road cell got +2
    // no road: no route
    Fixture g;
    CHECK(routeTraffic(g.ctx, 10, 10, 1, 2, 100) == 0);
}
TEST(zone_pass_leaves_empty_world_untouched) {
    Fixture f;
    for (int a = 0; a < 4; ++a) for (int b = 0; b < 4; ++b) runZonePass(f.ctx, a, b);
    for (auto v : f.w.xbld.v) CHECK(v == 0);
}

// Golden fingerprints produced by the ORIGINAL machine code (sc2k-emu-diff zone, variant 0: seed 1, LFSR 0xACE1,
// zone types 7-9 stripped, 3 sweeps of the 16 sub-passes). They only change if the port diverges from the original.
TEST(zone_pass_matches_fingerprints_recorded_from_the_original_code) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    struct Golden { const char* file; uint64_t hash; };
    const Golden goldens[] = {{"CITIES/NYC.SC2", 0x69ebdee44cc23862ull}, {"CITIES/CAPE.SC2", 0x719e4fed2163a9e3ull},
                              {"CITIES/TOKYO.SC2", 0xbf8f48a490da74a4ull}, {"SCENARIO/ATLANTA.SCN", 0xd4ac80f0640e7fb4ull}};
    const GameTables t = GameTables::fromExe(std::string(root) + "/SIMCITY.EXE");
    for (const auto& g : goldens) {
        CityFile c = CityFile::load(std::string(root) + "/" + g.file);
        World w = World::fromCity(c);
        CityState s = CityState::fromCity(c);
        for (auto& z : w.xzon.v) if ((z & 0xF) > 6) z &= 0xF0;
        CrtRand rng(1); Lfsr lfsr(0xACE1); ZoneHooks hooks; TrafficStats ts;
        SimContext ctx{w, s, t, rng, lfsr, hooks, ts, {}};
        for (int sweep = 0; sweep < 3; ++sweep) for (int a = 0; a < 4; ++a) for (int b = 0; b < 4; ++b) runZonePass(ctx, a, b);
        CHECK(worldFingerprint(w, s) == g.hash);
    }
}

// ---------------- destroy effects ----------------
namespace {
GameTables destroyTables() {
    GameTables t = zoneTables();
    for (size_t i = 0x70; i < 0x8C; ++i) t.footprint[i] = 1;
    for (size_t i = 0x8C; i < 0xAE; ++i) t.footprint[i] = 2;
    t.shoreType[0] = 0;
    return t;
}
struct DFixture {
    World w{}; CityState s{}; GameTables t = destroyTables(); CrtRand rng{1}; Lfsr lfsr{0xACE1}; ZoneHooks hooks; TrafficStats ts;
    SimContext ctx{w, s, t, rng, lfsr, hooks, ts, {}};
};
}  // namespace

TEST(destroy_single_building_becomes_rubble_and_updates_census) {
    DFixture f;
    f.w.xbld.at(30, 30) = 0x72; f.s.census[0x72] = 1;
    f.w.xbit.at(30, 30) = 0xE0; f.w.xzon.at(30, 30) = 0xF1; f.w.xtxt.at(30, 30) = 0;
    destroyTile(f.ctx, 30, 30, 0);
    CHECK(f.w.xbld.at(30, 30) >= 1 && f.w.xbld.at(30, 30) <= 4);
    CHECK(f.s.census[0x72] == 0);
    CHECK((f.w.xzon.at(30, 30) & 0xF0) == 0 && (f.w.xzon.at(30, 30) & 0xF) == 1);   // corner flags cleared, zone kept
    CHECK((f.w.xbit.at(30, 30) & 0xC2) == 0);                                       // 0x3D mask: powered/powerable/flipped bits cleared
}
TEST(destroy_ignores_rubble_and_clear_land) {
    DFixture f;
    f.w.xbld.at(5, 5) = 3;
    destroyTile(f.ctx, 5, 5, 1);
    CHECK(f.w.xbld.at(5, 5) == 3 && f.rng.state() == 1);
}
TEST(destroy_animation_consumes_two_rand_calls_per_sprite) {
    DFixture f;
    f.w.xbld.at(30, 30) = 0x72;
    CrtRand ref(1);
    destroyTile(f.ctx, 30, 30, 1);
    for (int i = 0; i < 2 * 1 * 1 * 1 + 1; ++i) ref.next();   // 2 (animation, size 1) + 1 (rubble pick)
    CHECK(f.rng.state() == ref.state());
}
TEST(destroy_tunnel_removes_both_mouths_and_clears_depth_bits) {
    DFixture f;
    f.w.xbld.at(40, 40) = 0x41; f.w.xbld.at(44, 40) = 0x3F;      // mouth with direction (+1,0) ... far mouth
    for (int a = 41; a <= 43; ++a) f.w.altm.at(a, 40) = 0x7C00 | 5;
    f.s.census[0x41] = 1; f.s.census[0x3F] = 1;
    destroyTile(f.ctx, 40, 40, 0);
    CHECK(f.w.xbld.at(40, 40) == 0 && f.w.xbld.at(44, 40) == 0);
    for (int a = 41; a <= 43; ++a) CHECK((f.w.altm.at(a, 40) & 0xFC00) == 0);
}
TEST(destroy_runway_flood_removes_connected_pieces) {
    DFixture f;
    for (int b = 60; b < 66; ++b) { f.w.xbld.at(60, b) = 0xDF; f.s.census[0xDF] += 1; }
    f.w.xbld.at(60, 67) = 0xDF;                                   // disconnected piece survives
    destroyTile(f.ctx, 60, 62, 0);
    for (int b = 60; b < 66; ++b) CHECK(f.w.xbld.at(60, b) == 0);
    CHECK(f.w.xbld.at(60, 67) == 0xDF);
}
TEST(destroy_footprint_of_one_tile_building_and_highway_pairing) {
    DFixture f;
    int x = 10, y = 11;
    CHECK(footprintOf(f.ctx, x, y, 0x73) == 1 && x == 10 && y == 11);
    x = 10; y = 11;
    CHECK(footprintOf(f.ctx, x, y, 0x62) == 2 && x == 10 && y == 11);   // highway piece: even x, odd y
    x = 11; y = 10;
    CHECK(footprintOf(f.ctx, x, y, 0x62) == 2 && x == 10 && y == 11);
}

// Golden fingerprints from the ORIGINAL code: destroy(…, animate=1) on every 5th non-clear tile in scan order
// (hash of the resulting world; tiles within 3 of the border skipped), see `sc2k-emu-diff destroy`.
TEST(destroy_matches_fingerprints_recorded_from_the_original_code) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    struct Golden { const char* file; uint64_t hash; };
    const Golden goldens[] = {{"CITIES/NYC.SC2", 0x534711f982e2941dull}, {"CITIES/CAPE.SC2", 0xb73590fb53b8e7b4ull},
                              {"CITIES/TOKYO.SC2", 0x757ab466d41f9193ull}, {"SCENARIO/ATLANTA.SCN", 0x99c19512e2313ffaull}};
    const GameTables t = GameTables::fromExe(std::string(root) + "/SIMCITY.EXE");
    for (const auto& g : goldens) {
        CityFile c = CityFile::load(std::string(root) + "/" + g.file);
        World w = World::fromCity(c); CityState s = CityState::fromCity(c);
        CrtRand rng(7 + 1); Lfsr lfsr(0xBEEF); ZoneHooks hooks; TrafficStats ts;
        SimContext ctx{w, s, t, rng, lfsr, hooks, ts, {}};
        for (int a = 0; a < 128; ++a) for (int b = 0; b < 128; ++b) {
            if (w.xbld.at(a, b) < 6 || ((a * 7 + b * 13) % 5) != 0) continue;
            if (a < 3 || b < 3 || a > 124 || b > 124) continue;
            destroyTile(ctx, a, b, 1);
        }
        CHECK(worldFingerprint(w, s) == g.hash);
    }
}
