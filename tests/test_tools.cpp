#include "test.h"
#include "save/city.h"
#include "simulation/tool_script.h"
#include "world/world_hash.h"

#include <cstdlib>
#include <string>

using namespace sc2k;

namespace {
struct YesHooks : ZoneHooks {
    bool confirm(int) override { return true; }
};

// Replays the first `ops` scripted operations of the emulator diff (variant 0: 4M funds) on the port alone.
uint64_t replay(const std::string& dir, const std::string& cityFile, int ops, int* successes) {
    CityFile c = CityFile::load(dir + "/" + cityFile);
    GameTables t = GameTables::fromExe(dir + "/SIMCITY.EXE");
    World w = World::fromCity(c);
    CityState s = CityState::fromCity(c);
    s.treasury = 4000000;
    if (s.population10 == 0) s.population10 = 1000;
    CrtRand rng(11); Lfsr lf(0xACE1); YesHooks hooks; TrafficStats ts;
    SimContext ctx{w, s, t, rng, lf, hooks, ts, {}};
    ToolState tst;
    ToolScript script(0x1234u);
    int ok = 0;
    for (int i = 0; i < ops; ++i) ok += runToolOp(ctx, tst, script.next(w, t));
    if (successes) *successes = ok;
    return worldFingerprint(w, s);
}
}  // namespace

TEST(tools_tables_and_ids) {
    CHECK(toolBuildingId(3, 2) == 0xCF);       // coal power plant
    CHECK(toolBuildingId(13, 0) == 0xD2);      // police station
    CHECK(toolBuildingId(7, 3) == 0xE9);       // subway station
    CHECK(toolBuildingId(0, 0) == 0);
}

TEST(tools_match_the_original_machine_code) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    // Golden values: `sc2k-emu-diff tools` on the same files runs the original SIMCITY.EXE routines through the same 500 operations (variant 0)
    // and prints these hashes ("identical" there means every tile, counter and RNG state matched after every operation).
    int ok = 0;
    CHECK(replay(root, "DEFAULT.SC2", 500, &ok) == 0x31b5b55e35e576ecull && ok > 300);
    CHECK(replay(root, "CITIES/CAPE.SC2", 500, &ok) == 0xc5319bfe274eca83ull && ok > 200);
}
