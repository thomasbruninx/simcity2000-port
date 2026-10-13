#include "test.h"
#include "save/city.h"
#include "simulation/tool_script.h"
#include "simulation/availability.h"
#include "simulation/finance.h"
#include "world/world_hash.h"

#include <cstdlib>
#include <string>

using namespace sc2k;

namespace {
struct YesHooks : ZoneHooks {
    bool confirm(int, int = 0) override { return true; }
};

// Replays the first `ops` scripted operations of the emulator diff (variant 0: 4M funds) on the port alone.
uint64_t replay(const std::string& dir, const std::string& cityFile, int ops, int* successes, int level = 0) {
    CityFile c = CityFile::load(dir + "/" + cityFile);
    GameTables t = GameTables::fromExe(dir + "/SIMCITY.EXE");
    World w = World::fromCity(c);
    CityState s = CityState::fromCity(c);
    s.treasury = 4000000;
    if (s.population10 == 0) s.population10 = 1000;
    CrtRand rng(11); Lfsr lf(0xACE1); YesHooks hooks; TrafficStats ts;
    SimContext ctx{w, s, t, rng, lf, hooks, ts, {}};
    ToolState tst;
    ToolScript script(0x1234u, level);
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
    // extended script: terrain, hydro dam, tunnel, onramp, rail link and highways (EXTENDED=1 OPS=800 sc2k-emu-diff tools ...)
    CHECK(replay(root, "CITIES/CAPE.SC2", 800, &ok, 1) == 0xebed3f169b8d6603ull && ok > 300);
    CHECK(replay(root, "CITIES/TOKYO.SC2", 800, &ok, 1) == 0x9cfbf4003e947df0ull && ok > 300);
    // level 2 adds landscape tools, forests, streams and emergency dispatch markers
    CHECK(replay(root, "CITIES/TOKYO.SC2", 800, &ok, 2) == 0xae9c4a1e5b0c3e00ull && ok > 300);
    CHECK(replay(root, "CITIES/PARIS.SC2", 800, &ok, 2) == 0x7621a0a0fa14d20aull);
}

TEST(budget_amounts_match_the_january_settlement) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    const std::string dir = root;
    CityFile c = CityFile::load(dir + "/CITIES/PARIS.SC2");
    GameTables t = GameTables::fromExe(dir + "/SIMCITY.EXE");
    CityState s = CityState::fromCity(c);
    long long expected = 0;
    for (int i = 0; i < 16; ++i) expected += budgetToDate(s, t, i);
    s.day = 25 * 12 * 3;            // a January
    s.budgetYearFlag = 1;
    const int before = s.treasury;
    CrtRand rng(1);
    runMonthlyFinance(s, t, rng);
    CHECK(s.treasury - before == expected);
    CHECK(budgetAnnual(s, t, kPolice) == static_cast<long long>(s.budget[kPolice].currentCosts) * s.budget[kPolice].fundingPercent / t.budgetDivisor[kPolice]);
}

TEST(tool_availability_follows_the_dated_events) {
    GameTables t;
    t.toolAvailBase = {31, 3, 3, 3, 7, 0, 5, 5, 1, 3, 3, 3, 15, 15, 31, 0, 0, 0};
    CityState s;
    s.eventYears.fill(1950);
    ToolAvailability a = ToolAvailability::fromState(s, t);
    CHECK(a.allows(3, 2) && a.allows(3, 3) && a.allows(3, 4) && !a.allows(3, 5) && !a.allows(3, 6));   // coal, hydro, oil only
    CHECK(!a.allows(6, 1) && a.allows(6, 0) && a.allows(6, 2));
    s.eventYears[0] = 0; s.eventYears[1] = 0; s.eventYears[7] = 0;
    a = ToolAvailability::fromState(s, t);
    CHECK(a.allows(3, 5) && a.allows(3, 6) && a.allows(6, 1) && a.allows(6, 3));
    s.ordinances = 0x20000;
    CHECK(!ToolAvailability::fromState(s, t).allows(3, 6));
    a.applyEvent(12, s); a.applyEvent(13, s);
    CHECK(a.allows(5, 5) && a.allows(5, 6) && !a.allows(5, 7));
}
