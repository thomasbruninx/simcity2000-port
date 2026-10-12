#include "test.h"
#include "simulation/environment.h"
#include "save/misc_fields.h"
#include "save/error.h"

#include <cstdlib>
#include <string>

using namespace sc2k;

static GameTables smallTables() {
    GameTables t;
    t.population[0x70] = 7;
    t.population[0x90] = 3;
    t.pollution[0x84] = 6;
    t.landValueClass[0x8A] = 2;
    return t;
}

TEST(env_empty_world_is_quiet) {
    World w{};
    EnvResult r = runEnvironmentScan(w, smallTables(), {});
    for (auto v : w.xpop.v) CHECK(v == 0);
    for (auto v : w.xplc.v) CHECK(v == 0);
    CHECK(r.developedTiles == 0 && r.crimeTotal == 0 && r.landValueTotal == 0);
}
TEST(env_density_comes_from_building_table_and_infrastructure_constants) {
    World w{};
    w.xbld.at(4, 4) = 0x70;            // cell (1,1): population table value 7
    w.xbld.at(5, 5) = 0xC8;            // >= 0xC6: +2
    w.xbld.at(6, 6) = 0xFB;            // 0xFB..0xFE: +12
    w.xbld.at(100, 100) = 0x90;        // cell (25,25): 3
    World w2 = w;
    runEnvironmentScan(w, smallTables(), {});
    CHECK(w.xpop.at(1, 1) == (7 + 2 + 12) * 4);
    CHECK(w.xpop.at(25, 25) == 3 * 4);
    CHECK(w.xpop.at(0, 0) == 0);
}
TEST(env_border_tiles_are_ignored_by_density) {
    World w{};
    w.xbld.at(0, 5) = 0x70; w.xbld.at(127, 5) = 0x70; w.xbld.at(5, 0) = 0x70; w.xbld.at(5, 127) = 0x70;
    runEnvironmentScan(w, smallTables(), {});
    for (auto v : w.xpop.v) CHECK(v == 0);
}
TEST(env_marks_live_in_the_top_left_quadrant_only) {
    World w{};
    w.xbld.at(5, 7) = 0x40;     // > 0x1C: developed -> marks XBIT[2][3]
    w.xbld.at(100, 100) = 0x40; // marks XBIT[50][50] (also inside the 64x64 corner)
    w.xzon.at(120, 120) = 1;    // zoned tile -> marks XBIT[60][60]
    runEnvironmentScan(w, smallTables(), {});
    CHECK(w.xbit.at(2, 3) & xbit::kMark);
    CHECK(w.xbit.at(50, 50) & xbit::kMark);
    CHECK(w.xbit.at(60, 60) & xbit::kMark);
    CHECK(!(w.xbit.at(5, 7) & xbit::kMark));
}
TEST(env_city_centre_is_mean_of_building_coordinates) {
    World w{};
    w.xbld.at(10, 20) = 0x70;   // sum rows 10, cols 20, count 1 (+1) -> 10/(2*2)=2, 20/4=5
    EnvResult r = runEnvironmentScan(w, smallTables(), {});
    CHECK(r.centerRow2 == 4 && r.centerCol2 == 10);
}
TEST(env_police_station_spreads_with_decaying_weights) {
    World w{};
    w.xbld.at(40, 40) = 0xD2; w.xzon.at(40, 40) = 0x80; w.xbit.at(40, 40) = xbit::kPowered;   // cell (10,10)
    EnvInputs in; in.policeBase = 1; in.policeFunding = 50;   // strength = (1+5)*50/2 = 150
    runEnvironmentScan(w, smallTables(), in);
    CHECK(w.xplc.at(10, 10) == 150);
    CHECK(w.xplc.at(9, 10) == 120);      // 150*4/5
    CHECK(w.xplc.at(9, 9) == 90);        // 120*3/4
    CHECK(w.xplc.at(8, 10) == 60);       // 90*2/3
    CHECK(w.xplc.at(7, 10) == 30);       // 60/2
    CHECK(w.xplc.at(8, 8) == 30);
    CHECK(w.xplc.at(6, 10) == 0);
}
TEST(env_unpowered_station_has_half_strength_and_origin_flag_is_required) {
    World w{};
    w.xbld.at(40, 40) = 0xD3; w.xzon.at(40, 40) = 0x80;   // fire station, unpowered
    w.xbld.at(80, 80) = 0xD3;                             // no origin flag: ignored
    EnvInputs in; in.fireFunding = 100;                   // 100*5/2 = 250 -> unpowered 125
    runEnvironmentScan(w, smallTables(), in);
    CHECK(w.xfir.at(10, 10) == 125);
    CHECK(w.xfir.at(20, 20) == 0);
}
TEST(env_coverage_saturates_at_255) {
    World w{};
    w.xbld.at(40, 40) = 0xD2; w.xzon.at(40, 40) = 0x80; w.xbit.at(40, 40) = xbit::kPowered;
    w.xbld.at(44, 40) = 0xD2; w.xzon.at(44, 40) = 0x80; w.xbit.at(44, 40) = xbit::kPowered;
    EnvInputs in; in.policeBase = 40; in.policeFunding = 100;
    runEnvironmentScan(w, smallTables(), in);
    CHECK(w.xplc.at(10, 10) == 255);
}
TEST(env_ordinance_coverage_bonus_per_building_tile) {
    World w{};
    w.xbld.at(8, 8) = 0x70;
    EnvInputs in; in.ordinances = 0x800 | 0x10;
    runEnvironmentScan(w, smallTables(), in);
    CHECK(w.xplc.at(2, 2) == 2 && w.xfir.at(2, 2) == 2);
}

// ---- equivalence with the game on saved cities (needs SC2K_DATA_PATH) ----
static bool haveData() { return std::getenv("SC2K_DATA_PATH") != nullptr; }
static std::string root() { return std::getenv("SC2K_DATA_PATH"); }

static EnvInputs inputsFrom(const CityFile& c) {
    EnvInputs in;
    in.ordinances = c.miscWord(misc::kOrdinances);
    in.waterFlag = c.miscInt(misc::kWaterFlag);
    in.c8440 = c.miscInt(misc::kC8440);
    in.policeBase = int16_t(c.miscWord(misc::kPoliceBase));
    in.policeFunding = c.miscInt(misc::kFundingPercent(5));
    in.fireFunding = c.miscInt(misc::kFundingPercent(6));
    return in;
}

TEST(env_scan_reproduces_saved_layers_for_cities_saved_right_after_the_scan) {
    if (!haveData()) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    const GameTables t = GameTables::fromExe(root() + "/SIMCITY.EXE");
    // Files saved at phase 2 of a month (docs/validation/004): coverage and density must match exactly.
    for (const char* name : {"CITIES/NYC.SC2", "SCENARIO/ATLANTA.SCN", "SCENARIO/BARCELON.SCN", "SCENARIO/CHARLEST.SCN", "SCENARIO/CHICAGO.SCN",
                             "SCENARIO/DAVENPOR.SCN", "SCENARIO/HOLLYWOO.SCN", "SCENARIO/HOMESTEA.SCN", "SCENARIO/MALIBU.SCN", "SCENARIO/MANHATTA.SCN"}) {
        CityFile c = CityFile::load(root() + "/" + name);
        World w = World::fromCity(c);
        for (auto& f : w.xbit.v) f &= 0xF7;
        EnvResult r = runEnvironmentScan(w, t, inputsFrom(c));
        CHECK(w.xplc == c.xplc);
        CHECK(w.xfir == c.xfir);
        CHECK(w.xpop == c.xpop);
        CHECK(r.centerRow2 == c.miscInt(misc::kCityCenterX2));
        CHECK(r.centerCol2 == c.miscInt(misc::kCityCenterY2));
        for (int a = 0; a < 64; ++a) for (int b = 0; b < 64; ++b) CHECK(((w.xbit.at(a, b) ^ c.xbit.at(a, b)) & xbit::kMark) == 0);
        long sumAbs = 0;
        for (size_t i = 0; i < w.xval.v.size(); ++i) sumAbs += std::abs(int(w.xval.v[i]) - int(c.xval.v[i]));
        CHECK(sumAbs < 4L * 4096);   // land value is recomputed from stale inputs (observed mean |d| 0.9-3.2 per city)
    }
}

TEST(game_tables_reject_other_builds) {
    if (!haveData()) return;
    bool threw = false;
    try { GameTables::fromExe(root() + "/WINSCURK.EXE"); } catch (const Sc2Error&) { threw = true; }
    CHECK(threw);
}
