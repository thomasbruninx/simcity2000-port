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

// Golden fingerprints produced by the ORIGINAL machine code (`sc2k-emu-diff zone`; the variant numbers are the tool's):
// variant 0 = city as saved, seed 1, LFSR 0xACE1, 3 sweeps of the 16 sub-passes;
// variants 4 and 6 = shifted demand/budget, zone 7/8/9 blocks painted over clear land, militaryType 3 / 5, 12 sweeps.
// They only change if the port diverges from the original.
namespace {
uint64_t zoneVariantHash(const GameTables& t, const CityFile& c, int variant) {
    World w = World::fromCity(c);
    CityState s = CityState::fromCity(c);
    if (variant >= 3) {
        s.militaryType = variant - 1;
        for (int ba = 0; ba + 12 <= 128; ba += 12) for (int bb = 0; bb + 12 <= 128; bb += 12) {
            const unsigned h = static_cast<unsigned>((ba / 12) * 31 + (bb / 12) * 17 + variant) * 2654435761u >> 28;
            if (h % 3 != 0) continue;
            bool clear = true;
            for (int a = ba; a < ba + 12; ++a) for (int b = bb; b < bb + 12; ++b) if ((w.xzon.at(a, b) & 0xF) != 0) clear = false;
            if (!clear) continue;
            for (int a = ba + 3; a < ba + 9; ++a) for (int b = bb + 3; b < bb + 9; ++b)
                if (w.xbld.at(a, b) <= 0x1C) w.xzon.at(a, b) = static_cast<uint8_t>((w.xzon.at(a, b) & 0xF0) | (7 + (h / 3) % 3));
        }
    }
    if (variant >= 1) {
        s.demand = {int16_t(variant == 1 ? 1500 : -1200), int16_t(variant == 1 ? -800 : 1900), int16_t(900), 0, 0, 0, 0, 0};
        for (int k = 10; k <= 15; ++k) s.budget[static_cast<size_t>(k)].fundingPercent = variant == 1 ? 40 : 75;
        s.population10 = variant == 1 ? 5000000 : 0;
    }
    CrtRand rng(1 + 11u * static_cast<uint32_t>(variant)); Lfsr lfsr(static_cast<uint16_t>(0xACE1u + 17u * static_cast<uint32_t>(variant)));
    ZoneHooks hooks; TrafficStats ts;
    SimContext ctx{w, s, t, rng, lfsr, hooks, ts, {}};
    const int sweeps = variant >= 3 ? 12 : 3;
    for (int sweep = 0; sweep < sweeps; ++sweep) for (int a = 0; a < 4; ++a) for (int b = 0; b < 4; ++b) runZonePass(ctx, a, b);
    return worldFingerprint(w, s);
}
}  // namespace
TEST(zone_pass_matches_fingerprints_recorded_from_the_original_code) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    struct Golden { const char* file; int variant; uint64_t hash; };
    const Golden goldens[] = {
        {"CITIES/NYC.SC2", 0, 0xc6c1a9a7b7df8b4full}, {"CITIES/NYC.SC2", 4, 0x47b788f44a997068ull}, {"CITIES/NYC.SC2", 6, 0xc2224ac6046fdd29ull},
        {"CITIES/CAPE.SC2", 0, 0x45f2fbbd6607ba63ull}, {"CITIES/CAPE.SC2", 4, 0x8ae3ef419e5b7e5eull}, {"CITIES/CAPE.SC2", 6, 0x6e69d428310046d0ull},
        {"CITIES/TOKYO.SC2", 0, 0x0d333cc2161512a6ull}, {"CITIES/TOKYO.SC2", 4, 0x20ba5b7031ecb155ull}, {"CITIES/TOKYO.SC2", 6, 0x8cf96022d440d70dull},
        {"SCENARIO/ATLANTA.SCN", 0, 0xd4ac80f0640e7fb4ull}, {"SCENARIO/ATLANTA.SCN", 4, 0xde6d6a81ae5ef8f9ull}, {"SCENARIO/ATLANTA.SCN", 6, 0xc6d059b36edb2951ull}};
    const GameTables t = GameTables::fromExe(std::string(root) + "/SIMCITY.EXE");
    for (const auto& g : goldens) {
        CityFile c = CityFile::load(std::string(root) + "/" + g.file);
        CHECK(zoneVariantHash(t, c, g.variant) == g.hash);
    }
}

// ---------------- military / airport / seaport ----------------
namespace {
GameTables specialTables() {
    GameTables t = zoneTables();
    t.pierDx = {0, 1, 0, -1}; t.pierDy = {1, 0, -1, 0};
    return t;
}
struct SFixture {
    World w{}; CityState s{}; GameTables t = specialTables(); CrtRand rng{1}; Lfsr lfsr{0xACE1}; ZoneHooks hooks; TrafficStats ts;
    SimContext ctx{w, s, t, rng, lfsr, hooks, ts, {}};
};
void paintZone(World& w, int a0, int b0, int a1, int b1, int zone) {
    for (int a = a0; a <= a1; ++a) for (int b = b0; b <= b1; ++b) w.xzon.at(a, b) = static_cast<uint8_t>(zone);
}
}  // namespace

TEST(special_one_by_one_buildings_are_rejected_on_military_tiles_and_need_power_elsewhere) {
    SFixture f;
    paintZone(f.w, 10, 10, 10, 10, 7);
    // original quirk (0x440600): any tile whose zone nibble is already 7 is rejected, so zone-7 tiles never get 1x1/2x2 buildings here
    CHECK(placeZoneBuilding(f.ctx, 10, 10, 0xE8, 7));      // (returns 1 even though 0x440600 refused the tile)
    CHECK(f.w.xbld.at(10, 10) == 0 && f.s.militaryCounts[13] == 0);
    f.w.xzon.at(20, 20) = 8;
    CHECK(!placeZoneBuilding(f.ctx, 20, 20, 0xE1, 8));      // airports need power nearby
    CHECK(f.w.xbld.at(20, 20) == 0);
    f.w.xbit.at(20, 20) = xbit::kPowered;
    CHECK(placeZoneBuilding(f.ctx, 20, 20, 0xE1, 8));
    CHECK(f.w.xbld.at(20, 20) == 0xE1 && (f.w.xbit.at(20, 20) & 0xE0) == 0xE0 && (f.w.xzon.at(20, 20) & 0xF) == 8);
    CHECK(f.s.census[0xE1] == 1);
}
TEST(special_two_by_two_requires_zone_on_all_four_tiles_and_clears_old_buildings) {
    SFixture f;
    paintZone(f.w, 30, 30, 31, 30, 8);                      // only 3 of 4 tiles
    paintZone(f.w, 30, 31, 30, 31, 8);
    f.w.xbit.at(31, 31) = xbit::kPowered;
    CHECK(!placeZoneBuilding(f.ctx, 31, 31, 0xEE, 8));
    paintZone(f.w, 31, 31, 31, 31, 8);
    f.w.xbld.at(30, 31) = 0xE3; f.s.census[0xE3] = 1;       // a 1x1 building in the way is cleared
    CHECK(placeZoneBuilding(f.ctx, 31, 31, 0xEE, 8));      // odd coordinates: anchored at (30,30)
    for (int a = 30; a <= 31; ++a) for (int b = 30; b <= 31; ++b) CHECK(f.w.xbld.at(a, b) == 0xEE && (f.w.xzon.at(a, b) & 0xF) == 8);
    CHECK(f.s.census[0xE3] == 0 && f.s.census[0xEE] == 4);
    CHECK((f.w.xzon.at(30, 30) & 0xF0) != 0);               // corner bits stamped
}
TEST(special_command_centre_slides_toward_zone_and_adds_underground_piece) {
    SFixture f;
    paintZone(f.w, 40, 40, 44, 44, 7);
    CHECK(placeZoneBuilding(f.ctx, 42, 42, 0xF9, 7));
    int n = 0;
    for (int a = 40; a <= 44; ++a) for (int b = 40; b <= 44; ++b) if (f.w.xbld.at(a, b) == 0xF9) { ++n; CHECK(f.w.xund.at(a, b) == 0x22); }
    CHECK(n == 9);
    CHECK(f.w.xbld.at(40, 40) == 0xF9);                     // anchor slid by two tiles to the zone corner
    CHECK(f.s.militaryCounts[15] == 9);
}
TEST(special_pier_needs_five_free_water_tiles_and_height_margin) {
    SFixture f;
    paintZone(f.w, 60, 60, 60, 60, 9);
    f.w.xbit.at(60, 61) = xbit::kWater;
    CHECK(!placeZoneBuilding(f.ctx, 60, 60, 0xE0, 9));      // no power
    f.w.xbit.at(60, 60) = xbit::kPowered;
    CHECK(!placeZoneBuilding(f.ctx, 60, 60, 0xE0, 9));      // only one water tile
    for (int b = 61; b <= 65; ++b) { f.w.xbit.at(60, b) = xbit::kWater; f.w.altm.at(60, b) = 0; }
    CHECK(!placeZoneBuilding(f.ctx, 60, 60, 0xE0, 9));      // altitude word: water level (bits 5..9) must be >= terrain + 2
    f.w.altm.at(60, 65) = 5 << 5;
    CHECK(placeZoneBuilding(f.ctx, 60, 60, 0xE0, 9));
    CHECK(f.w.xbld.at(60, 60) == 0xE0 && f.w.xbld.at(60, 61) == 0xDF && f.w.xbld.at(60, 64) == 0xDF && f.w.xbld.at(60, 65) == 0);
}
TEST(special_runway_alternates_with_parity_and_skips_existing_pieces) {
    SFixture f;
    paintZone(f.w, 80, 80, 80, 90, 8);
    f.w.xbit.at(80, 80) = xbit::kPowered;
    CHECK(!placeZoneBuilding(f.ctx, 80, 80, 0xDD, 8));      // even/even start has no direction when the runway count is even
    CHECK(placeZoneBuilding(f.ctx, 81, 80, 0xDD, 8) == false);   // zone check: (81, 80..84) is not zoned
    paintZone(f.w, 81, 80, 81, 90, 8);
    f.w.xbit.at(81, 80) = xbit::kPowered;
    CHECK(placeZoneBuilding(f.ctx, 81, 80, 0xDD, 8));      // odd a: the line runs along b
    for (int b = 80; b <= 84; ++b) CHECK(f.w.xbld.at(81, b) == 0xDD);
    CHECK(f.w.xbld.at(81, 85) == 0);
    f.s.census[0xDD] = 5;
    CHECK(placeZoneBuilding(f.ctx, 81, 80, 0xDD, 8));      // existing pieces do not count: the line is extended to 5 new tiles
}
TEST(special_zone_pass_places_buildings_for_each_military_type) {
    int placed[6] = {};
    for (int type = 2; type <= 5; ++type) {
        SFixture f;
        f.s.militaryType = type;
        paintZone(f.w, 8, 8, 31, 31, 7);
        if (type == 4) for (int a = 8; a <= 31; ++a) for (int b = 32; b <= 40; ++b) { f.w.xbit.at(a, b) = xbit::kWater; f.w.altm.at(a, b) = 5 << 5; }   // deep sea next to the base
        for (int sweep = 0; sweep < 40; ++sweep) for (int a = 0; a < 4; ++a) for (int b = 0; b < 4; ++b) runZonePass(f.ctx, a, b);
        for (int a = 8; a <= 31; ++a) for (int b = 8; b <= 40; ++b) placed[type] += f.w.xbld.at(a, b) >= 0xDD;   // (a pier on a zone-7 tile loses its head but keeps its 4 body tiles)
    }
    CHECK(placed[2] == 0);   // 1x1 / 2x2 buildings are rejected on zone-7 tiles (see above)
    CHECK(placed[3] > 0);    // runways
    CHECK(placed[4] > 0);    // piers (water adjacent)
    CHECK(placed[5] > 0);    // command centre
}
TEST(special_underground_decay_removes_subway_pieces_when_unfunded) {
    SFixture f;
    f.s.budget[kSubway].fundingPercent = 0;
    f.w.xzon.at(16, 16) = 10;
    f.w.xund.at(16, 16) = 3;
    f.s.c9434 = 1;
    for (int i = 0; i < 4000 && f.w.xund.at(16, 16) != 0; ++i) runZonePass(f.ctx, 0, 0);
    CHECK(f.w.xund.at(16, 16) == 0 && f.s.c9434 == 0);
    f.s.budget[kSubway].fundingPercent = 100;
    f.w.xund.at(16, 16) = 3;
    for (int i = 0; i < 4000; ++i) runZonePass(f.ctx, 0, 0);
    CHECK(f.w.xund.at(16, 16) == 3);                        // fully funded: never decays
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
