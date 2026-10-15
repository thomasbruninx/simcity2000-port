#include <cstdlib>
#include <string>

#include "resources/pe_resources.h"
#include "save/city.h"
#include "simulation/editor_tools.h"
#include "simulation/simulation.h"
#include "simulation/tables.h"
#include "test.h"
#include "world/world_hash.h"

using namespace sc2k;

namespace {
struct EditorFixture {
    World w{}; CityState s{}; GameTables t{}; CrtRand rng{1}; Lfsr lfsr{0xACE1}; ZoneHooks hooks; TrafficStats ts;
    SimContext ctx{w, s, t, rng, lfsr, hooks, ts, {}};
    ToolState tool;
    EditorFixture() { t.footprint[0xAE] = 4; t.footprint[0x80] = 3; t.footprint[0x72] = 1; s.treasury = 777; }
    bool building(int a, int b, int id) const { return w.xbld.at(a, b) == id; }
};
}  // namespace

TEST(editor_place_ignores_terrain_water_and_old_objects) {
    EditorFixture f;
    f.w.xter.at(20, 20) = 0x05;                          // a slope
    f.w.xbit.at(21, 21) = xbit::kWater;                 // and water under the footprint
    f.w.xbld.at(22, 22) = 8;                            // a tree
    CHECK(editorPlaceObject(f.ctx, f.tool, 21, 21, 0xAE));   // a 4x4 building is centred on the clicked tile: 20..23
    for (int a = 20; a < 24; ++a) for (int b = 20; b < 24; ++b) CHECK(f.building(a, b, 0xAE));
    CHECK(f.w.xbld.at(19, 19) == 0 && f.w.xbld.at(24, 24) == 0);
    CHECK(editorPlaceObject(f.ctx, f.tool, 50, 50, 0x72) && f.building(50, 50, 0x72) && f.w.xbld.at(51, 50) == 0);   // one tile
    CHECK(editorPlaceObject(f.ctx, f.tool, 10, 10, 0x80) && f.building(9, 9, 0x80) && f.building(11, 11, 0x80));       // 3x3 from one tile before
    CHECK(!editorPlaceObject(f.ctx, f.tool, 0, 0, 0xAE));    // the footprint would leave the map: nothing changes
    CHECK(f.w.xbld.at(0, 0) == 0 && f.w.xbld.at(1, 1) == 0);
    CHECK(f.s.treasury == 777);                         // free
}

TEST(editor_demolish_removes_objects_without_rubble_or_cost) {
    EditorFixture f;
    f.w.xzon.at(60, 60) = 0x01; f.w.xbld.at(60, 60) = 8;     // a zoned tile with a tree
    editorDemolish(f.ctx, f.tool, 60, 60, false);
    CHECK(f.w.xbld.at(60, 60) == 0 && (f.w.xzon.at(60, 60) & 0xF) == 1);                          // the zone stays
    editorDemolish(f.ctx, f.tool, 70, 70, false);            // nothing there: nothing happens
    editorDemolish(f.ctx, f.tool, -3, 200, false);           // outside the map
    CHECK(f.s.treasury == 777);
    const char* root = std::getenv("SC2K_DATA_PATH");        // buildings need the game's tables (footprints, corner flags)
    if (!root) { std::printf("  (buildings skipped: SC2K_DATA_PATH not set)\n"); return; }
    EditorFixture g;
    g.t = GameTables::fromExe(std::string(root) + "/SIMCITY.EXE");
    CHECK(editorPlaceObject(g.ctx, g.tool, 41, 41, 0xAE));
    editorDemolish(g.ctx, g.tool, 42, 42, false);            // any tile of the building takes it all away
    for (int a = 36; a < 48; ++a) for (int b = 36; b < 48; ++b) CHECK(g.w.xbld.at(a, b) == 0);   // no rubble (ids 1..4) left behind either
    CHECK(g.s.treasury == 777);
}

TEST(bridge_question_offers_the_road_rail_and_highway_types) {
    CHECK(bridgeTypesOf(0x1F) == (std::vector<int>{0, 1, 2, 3, 4}));
    CHECK(bridgeTypesOf(0x20) == (std::vector<int>{5}));          // a highway over water: the question used to offer nothing
    CHECK(bridgeTypesOf(0x60) == (std::vector<int>{5, 6}));
    CHECK(bridgeTypesOf(0x05) == (std::vector<int>{0, 2}));
    CHECK(bridgeTypesOf(0).empty());
}

TEST(a_played_city_saves_and_loads_back_unchanged) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    const std::string dir = root;
    for (const char* name : {"CITIES/PARIS.SC2", "SCENARIO/ATLANTA.SCN"}) {
        const CityFile city = CityFile::load(dir + "/" + name);
        Simulation sim(city, GameTables::fromExe(dir + "/SIMCITY.EXE"), 5);
        sim.advanceDays(90);
        sim.world().xbld.at(5, 5) = 0;   // the player's edit
        const std::vector<uint8_t> bytes = sim.toCity(city).serialize();
        const CityFile back = CityFile::parse(bytes);
        const World w2 = World::fromCity(back);
        const CityState s2 = CityState::fromCity(back);
        CHECK(w2.xbld == sim.world().xbld && w2.xzon == sim.world().xzon && w2.xbit == sim.world().xbit && w2.xund == sim.world().xund);
        CHECK(w2.xtxt == sim.world().xtxt && w2.xter == sim.world().xter && w2.altm == sim.world().altm);
        CHECK(w2.xtrf == sim.world().xtrf && w2.xval == sim.world().xval && w2.xcrm == sim.world().xcrm && w2.xpop == sim.world().xpop);
        CHECK(s2.treasury == sim.state().treasury && s2.day == sim.state().day && s2.population10 == sim.state().population10 && s2.rateLevel == sim.state().rateLevel);
        CHECK(s2.census == sim.state().census && s2.seaLevel == sim.state().seaLevel);
        // a second round trip is byte-identical: nothing drifts when a saved city is saved again
        Simulation again(back, GameTables::fromExe(dir + "/SIMCITY.EXE"), 5);
        CHECK(again.toCity(back).serialize() == bytes);
    }
}

TEST(the_start_year_of_a_new_city_survives_saving) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    const std::string dir = root;
    const CityFile map = CityFile::load(dir + "/DEFAULT.SC2");
    for (int era = 0; era < 4; ++era) {
        Simulation sim(map, GameTables::fromExe(dir + "/SIMCITY.EXE"), 3);
        sim.startNewCity(map, era, 0, nullptr);
        CHECK(sim.state().startYear == 1900 + 50 * era);
        const CityFile back = CityFile::parse(sim.toCity(map).serialize());
        CHECK(CityState::fromCity(back).startYear == 1900 + 50 * era);   // it came back as 1900 for every era
    }
}
