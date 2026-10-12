#include "test.h"
#include "simulation/power.h"
#include "simulation/tile_queue.h"

#include <cstdlib>
#include <string>

using namespace sc2k;

TEST(tile_queue_fifo_and_overflow) {
    TileQueue q;
    q.reset(1, 2);
    q.push(3, 4);
    CHECK(q.pop().a == 1);
    CHECK(q.pop().a == 3);
    CHECK(q.empty());
    // overflow: pushing 512 items into the 512-slot ring drops the oldest
    q.reset(0, 0);
    for (int i = 1; i < 520; ++i) q.push(i, 0);
    int n = 0; while (!q.empty()) { q.pop(); ++n; }
    CHECK(n == 511);
}

static World emptyWorld() { return World{}; }

TEST(power_single_plant_supplies_connected_consumers_in_bfs_order) {
    World w = emptyWorld();
    // Plant tile (0xCF = 44 units) at (10,10) and a row of 100 powerable consumer tiles (id 0x70) to its right.
    w.xbld.at(10, 10) = 0xCF; w.xbit.at(10, 10) = xbit::kPowerable;
    for (int b = 11; b < 111; ++b) { w.xbld.at(10, b) = 0x70; w.xbit.at(10, b) = xbit::kPowerable; }
    CrtRand rng;
    PowerResult r = runPowerScan(w, {}, rng);
    CHECK(r.capacity == 44);
    CHECK(r.served == 44);
    CHECK(r.percentUsed == 100);
    // Quirk preserved: pass 2 charges 1 unit for every tile with id > 0x6F, including the plant tile itself
    // (pass 1 does not count plants as consumers), so 44 units power the plant + the nearest 43 consumers.
    CHECK(w.xbit.at(10, 10) & xbit::kPowered);
    for (int b = 11; b < 11 + 43; ++b) CHECK(w.xbit.at(10, b) & xbit::kPowered);
    for (int b = 11 + 43; b < 111; ++b) CHECK(!(w.xbit.at(10, b) & xbit::kPowered));
    CHECK(!(w.xbit.at(10, 10) & xbit::kMark));
}
TEST(power_energy_conservation_adds_one_twelfth) {
    World w = emptyWorld();
    w.xbld.at(5, 5) = 0xC9; w.xbit.at(5, 5) = xbit::kPowerable;   // 11 units
    for (int b = 6; b < 40; ++b) { w.xbld.at(5, b) = 0x70; w.xbit.at(5, b) = xbit::kPowerable; }
    CrtRand rng;
    PowerInputs in; in.ordinances = 0x10000;
    PowerResult r = runPowerScan(w, in, rng);
    CHECK(r.capacity == 11);
    CHECK(r.served == 11 + 11 / 12);  // 11/12 == 0 in C integer division
}
TEST(power_unconnected_tiles_stay_unpowered) {
    World w = emptyWorld();
    w.xbld.at(0, 0) = 0xCE; w.xbit.at(0, 0) = xbit::kPowerable;
    w.xbld.at(0, 2) = 0x70; w.xbit.at(0, 2) = xbit::kPowerable;   // gap at (0,1): not powerable
    CrtRand rng;
    runPowerScan(w, {}, rng);
    CHECK(!(w.xbit.at(0, 2) & xbit::kPowered));
}

// Equivalence with the game: recompute from saved cities and compare to the powered bits the game saved.
TEST(power_scan_reproduces_saved_powered_bits_in_original_cities) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    for (const char* name : {"CITIES/NYC.SC2", "CITIES/TOKYO.SC2", "CITIES/LONDON.SC2", "CITIES/SPIRALOP.SC2", "SCENARIO/ATLANTA.SCN", "SCENARIO/FLINT.SCN"}) {
        CityFile c = CityFile::load(std::string(root) + "/" + name);
        World w = World::fromCity(c);
        CrtRand rng;
        runPowerScan(w, {}, rng);
        int mismatches = 0;
        for (size_t i = 0; i < w.xbit.v.size(); ++i)
            mismatches += ((w.xbit.v[i] ^ c.xbit.v[i]) & xbit::kPowered) != 0;
        CHECK(mismatches <= 2);
    }
}

#include "simulation/water.h"
#include "save/misc_fields.h"

TEST(water_pump_supplies_pipes_and_consumers) {
    World w = World{};
    // powered pump at (10,10) fed by waterFactor 2 (=10 units) next to a consumer chain along piped tiles
    w.xbld.at(10, 10) = 0xDC; w.xbit.at(10, 10) = xbit::kPiped | xbit::kPowered;
    for (int b = 11; b < 31; ++b) { w.xbld.at(10, b) = 0x70; w.xbit.at(10, b) = xbit::kPiped; }
    WaterInputs in; in.waterFactor = 2;   // pump output = 0 + 2*5 = 10
    WaterResult r = runWaterScan(w, in);
    CHECK(r.supply == 10);
    CHECK(r.served == 10);
    CHECK(r.percent == 100);
    for (int b = 11; b < 21; ++b) CHECK(w.xbit.at(10, b) & xbit::kWatered);
    for (int b = 21; b < 31; ++b) CHECK(!(w.xbit.at(10, b) & xbit::kWatered));
    CHECK(w.xbit.at(10, 10) & xbit::kWatered);
}
TEST(water_unpowered_pump_gives_nothing) {
    World w = World{};
    w.xbld.at(3, 3) = 0xDC; w.xbit.at(3, 3) = xbit::kPiped;   // not powered -> never a flood source
    w.xbld.at(3, 4) = 0x70; w.xbit.at(3, 4) = xbit::kPiped;
    WaterInputs in; in.waterFactor = 9;
    WaterResult r = runWaterScan(w, in);
    CHECK(r.supply == 0 && r.wateredTiles == 0 && r.percent == 100);
}

TEST(water_scan_reproduces_saved_watered_bits_in_original_cities) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    // cities whose saved state matches a fresh scan exactly (see docs/validation/003); covers rotations 0..3
    for (const char* name : {"CITIES/NYC.SC2", "CITIES/TOKYO.SC2", "CITIES/LONDON.SC2", "CITIES/SPIRALOP.SC2",
                             "CITIES/CAPE.SC2", "CITIES/BAYVIEW.SC2", "CITIES/EM.SC2", "SCENARIO/MANHATTA.SCN"}) {
        CityFile c = CityFile::load(std::string(root) + "/" + name);
        World w = World::fromCity(c);
        PowerInputs pin; pin.ordinances = c.miscWord(misc::kOrdinances); pin.wind = uint8_t(c.miscWord(misc::kWind)); pin.cloudiness = uint8_t(c.miscWord(misc::kCloudiness));
        CrtRand rng; runPowerScan(w, pin, rng);
        WaterInputs in; in.rotation = c.miscInt(misc::kRotation); in.cloudiness = pin.cloudiness; in.waterFactor = int16_t(c.miscWord(misc::kWaterFactor));
        runWaterScan(w, in);
        int mismatches = 0;
        for (size_t i = 0; i < w.xbit.v.size(); ++i) mismatches += ((w.xbit.v[i] ^ c.xbit.v[i]) & xbit::kWatered) != 0;
        CHECK(mismatches <= 2);
    }
}
