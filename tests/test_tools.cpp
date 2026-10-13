#include "test.h"
#include "save/city.h"
#include "simulation/tool_script.h"
#include "simulation/availability.h"
#include "simulation/bonds.h"
#include "simulation/new_city.h"
#include "simulation/scenario.h"
#include "simulation/news.h"
#include "simulation/terrain_gen.h"
#include "simulation/query.h"
#include "resources/pe_resources.h"
#include "simulation/simulation.h"
#include "simulation/query.h"
#include "simulation/finance.h"
#include "world/world_hash.h"

#include <cstdlib>
#include <cstring>
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
    // level 3 adds the terrain editor's spray raise/lower and level samples
    CHECK(replay(root, "CITIES/TOKYO.SC2", 800, &ok, 3) == 0x7f04312fa4a7ba3full && ok > 300);
    CHECK(replay(root, "CITIES/VENICE.SC2", 800, &ok, 3) == 0xa5e67196d9f1958full && ok > 300);
}

TEST(terrain_editor_stretch_moves_one_tile_by_pointer_distance) {
    // The original reads an uninitialised pointer for the vertical distance, so stretch cannot be replayed against it; checked by hand.
    GameTables t; CityState s; CrtRand rng(1); Lfsr lf(1); YesHooks hooks; TrafficStats ts;
    World w{};
    SimContext ctx{w, s, t, rng, lf, hooks, ts, {}};
    ToolState tst;
    for (int a = 0; a < 128; ++a) for (int b = 0; b < 128; ++b) { w.altm.at(a, b) = 5; w.xter.at(a, b) = 0; }
    int last = 5;
    CHECK(!terrainStretch(ctx, tst, 40, 40, 5, last, 2, 1));       // 2 px at zoom 1 (6 px per level): no change
    CHECK(terrainStretch(ctx, tst, 40, 40, 5, last, 13, 1));       // 13 / 6 = 2 levels up
    CHECK((w.altm.at(40, 40) & 0x1F) == 7 && last == 7);
    CHECK(!terrainStretch(ctx, tst, 40, 40, 5, last, 13, 1));      // same target again
    CHECK(!terrainStretch(ctx, tst, 40, 40, 20, last, -6 * 11, 1));   // more than 10 levels below the starting altitude is refused
    CHECK(!terrainStretch(ctx, tst, 40, 40, 20, last, 6 * 11, 1));    // and nothing reaches altitude 31
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

TEST(bonds_issue_and_repay_keep_budget_and_rates_consistent) {
    GameTables t;
    CityState s; s.rateLevel = 3; s.treasury = 25000;
    { const int32_t perSubwayTile = 1000000; std::memcpy(&t.valueData[0x4DB118 - 0x4DACB8], &perSubwayTile, 4); s.c9434 = 1000; }   // a rich city: bonds are available
    CrtRand rng(1); Lfsr lf(1); World w; YesHooks hooks; TrafficStats ts;
    SimContext c{w, s, t, rng, lf, hooks, ts, {}};
    s.census[0xD2] = 0;
    CHECK(issueBond(c) && s.debt == 1 && s.treasury == 35000 && s.bondRates[0] == 4);
    CHECK(s.budget[kBond].currentCosts == 1 && s.budget[kBond].fundingPercent == 40000);   // 4 % per year of $10,000 = 400 = 1 * 40000 / 100
    s.rateLevel = 6;
    CHECK(issueBond(c) && s.debt == 2 && s.bondRates[1] == 7 && s.budget[kBond].fundingPercent == 55000);
    CHECK(repayOldestBond(c) && s.debt == 1 && s.bondRates[0] == 7 && s.treasury == 35000 && s.budget[kBond].fundingPercent == 70000);
    s.treasury = 9999;
    CHECK(!repayOldestBond(c) && s.debt == 1);
    s.debt = 50; s.treasury = 0;
    CHECK(!issueBond(c) && s.treasury == 0);
}

TEST(query_names_follow_the_exe_string_groups_and_report_the_tile) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    const std::string dir = root;
    CityFile c = CityFile::load(dir + "/CITIES/PARIS.SC2");
    GameTables t = GameTables::fromExe(dir + "/SIMCITY.EXE");
    World w = World::fromCity(c); CityState s = CityState::fromCity(c);
    CrtRand rng(1); Lfsr lf(1); YesHooks hooks; TrafficStats ts;
    SimContext ctx{w, s, t, rng, lf, hooks, ts, {}};
    w.xbld.at(10, 10) = 0x1D; w.xbld.at(11, 10) = 0x2C; w.xbld.at(12, 10) = 0x95; w.xbld.at(13, 10) = 0; w.xbld.at(14, 10) = 0xCF;
    CHECK(tileNameString(ctx, 10, 10) == 599);          // Road
    CHECK(tileNameString(ctx, 11, 10) == 600);          // Railway
    CHECK(tileNameString(ctx, 12, 10) == 593 + (0x95 - 0x66));
    CHECK(tileNameString(ctx, 14, 10) == 593 + (0xCF - 0x66));
    w.xbit.at(13, 10) = 0; w.xter.at(13, 10) = 0;
    CHECK(tileNameString(ctx, 13, 10) == 751);          // clear terrain
    w.xzon.at(13, 10) = 2;
    const QueryInfo q = queryTile(ctx, 13, 10);
    CHECK(!q.lines.empty() && q.lines[0].labelString == 812 && q.lines[0].value[0].stringId == 769);   // "Zoned : Residential"
    // signs
    CHECK(placeSign(ctx, 20, 20, "Downtown", false) && signText(ctx, 20, 20) == "Downtown" && w.xtxt.at(20, 20) >= 1);
    CHECK(placeSign(ctx, 20, 20, "", false) && signText(ctx, 20, 20).empty() && w.xtxt.at(20, 20) == 0);
}

TEST(new_city_setup_follows_era_and_difficulty) {
    GameTables t;
    for (size_t i = 0; i < t.eventYearBase.size(); ++i) t.eventYearBase[i] = static_cast<int16_t>(1900 + 10 * i);
    World w{}; CityState s; CrtRand rng(5);
    w.altm.at(3, 4) = 0xFC05;
    NewCityOptions o; o.era = 1; o.difficulty = 2;
    setupNewCity(w, s, t, rng, o);
    CHECK(s.startYear == 1950 && s.nationalPop == 25000 && s.gnp == 7500 && s.rateLevel == 3);
    CHECK(s.treasury == 10000 && s.debt == 1 && s.bondRates[0] == 3 && s.budget[kBond].fundingPercent == 30000);
    CHECK(w.altm.at(3, 4) == 0x0005);                                // status flags above bit 9 are cleared
    CHECK(s.eventYears[0] == 0 && s.eventYears[16] >= 2060);       // 1900.. is before 1950; 2060 + rand%20 is not
    o.difficulty = 0;
    setupNewCity(w, s, t, rng, o);
    CHECK(s.treasury == 20000 && s.debt == 0 && s.budget[kBond].currentCosts == 0);
}

TEST(scenarios_start_their_disaster_and_have_goals) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    const std::string dir = root;
    CityFile c = CityFile::load(dir + "/SCENARIO/SANFRANC.SCN");
    GameTables t = GameTables::fromExe(dir + "/SIMCITY.EXE");
    Simulation sim(c, t, 3);
    const CityState& s0 = sim.state();
    CHECK(s0.scenario && s0.scenario->disaster == 6 && s0.scenario->focusA == 41 && s0.scenario->focusB == 61 && s0.scenario->timeLimit == 60);
    CHECK(s0.scenario->population10 == 68000 && !scenarioGoalsMet(*s0.scenario, s0));
    sim.tick();
    CHECK(sim.state().disasterKind == 6 && sim.state().disasterActive == 1);
    long fires = 0;
    for (int a = 0; a < 128; ++a) for (int b = 0; b < 128; ++b) { const int v = sim.world().xtxt.at(a, b); fires += v >= 0xFB && v <= 0xFF; }
    CHECK(fires > 0);
    // the goals are met once the city reaches the target population
    CityState met = sim.state();
    met.population10 = 70000;
    CHECK(scenarioGoalsMet(*met.scenario, met));
    CityFile d = CityFile::load(dir + "/DEFAULT.SC2");
    CHECK(!ScenarioInfo::fromCity(d).has_value());
}

TEST(microsim_dialog_lines_follow_the_building_class) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    const std::string dir = root;
    CityFile c = CityFile::load(dir + "/CITIES/NYC.SC2");
    GameTables t = GameTables::fromExe(dir + "/SIMCITY.EXE");
    Simulation sim(c, t, 3);
    PeImage exe = PeImage::load(dir + "/SIMCITY.EXE");
    PeResources res(exe);
    const auto str = [&](int id) { return res.string(id); };
    int found = 0;
    bool sawPlant = false;
    for (int a = 0; a < 128 && !sawPlant; ++a)
        for (int b = 0; b < 128; ++b) {
            const MicrosimInfo m = microsimInfo(sim.context(), a, b);
            if (!m.valid) continue;
            ++found;
            if (m.buildingId >= 0xC9 && m.buildingId <= 0xCF) {   // a power plant: output, load, age
                std::string all;
                for (int id : m.lineStrings) if (id >= 0) all += formatMicrosimLine(m, res.string(id), str) + "|";
                CHECK(all.find("MWatts") != std::string::npos && all.find("Capacity") != std::string::npos);
                sawPlant = true;
                break;
            }
        }
    CHECK(found > 0 && sawPlant);
    MicrosimInfo m; m.b1 = 12; m.w2 = 3; m.w4 = 2; m.w6 = 1990;
    CHECK(formatMicrosimLine(m, "Built in : #3, grade #G, #W", str) == "Built in : 1990, grade A+, 12-28");
    CHECK(formatMicrosimLine(m, "Design Capacity : #1000", str) == "Design Capacity : 3000");
}

TEST(newspaper_text_grammar_generates_stories) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    const std::string dir = root;
    GameTables t = GameTables::fromExe(dir + "/SIMCITY.EXE");
    const NewsData data = NewsData::load(dir, t.newsDigraph);
    CHECK(data.loaded && data.count[1] == 8 && data.start[1] == 93 && data.priority[2] == 1000 && data.decay[2] == 500);
    NewsRoom room(&data);
    CrtRand rng(7);
    room.reset(rng);
    CHECK(room.entries()[0].id == 11 && room.entries()[8].id == 19);
    room.post(22, 0);            // priority 1000: goes to the top
    room.post(7, 0);             // priority 200: below
    CHECK(room.entries()[0].id == 22 && room.entries()[1].id == 7);
    room.age();
    CHECK(room.entries()[0].id == 22);
    NewsRoom::Context ctx; ctx.cityName = "Testville"; ctx.label = [](int) { return std::string("Label"); };
    CrtRand a(5), b(5);
    const std::string h1 = room.headline(0, a, ctx), h2 = room.headline(0, b, ctx);
    CHECK(!h1.empty() && h1 == h2);                       // deterministic for the same seed
    CrtRand c1(5), c2(5);
    const std::string art = room.article(0, c1, ctx);
    CHECK(art.size() > 40 && art.find("Testville") != std::string::npos);
    std::printf("  headline: %s\n  article: %.160s\n", h1.c_str(), art.c_str());
}

TEST(random_terrain_matches_the_original_generator) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    const std::string dir = root;
    CityFile c = CityFile::load(dir + "/CITIES/PARIS.SC2");
    GameTables t = GameTables::fromExe(dir + "/SIMCITY.EXE");
    auto run = [&](bool coast, bool river, int hills, int water, int trees) {
        World w = World::fromCity(c); CityState s = CityState::fromCity(c);
        CrtRand rng(31); Lfsr lf(0x1234); ZoneHooks hooks; TrafficStats ts;
        SimContext ctx{w, s, t, rng, lf, hooks, ts, {}};
        ctx.lcg.seed(0x4321);
        generateTerrain(ctx, TerrainParams{coast, river, hills, water, trees});
        return worldFingerprint(w, s);
    };
    // the hashes printed by `sc2k-emu-diff terrain` for the same four parameter sets (identical on the original's machine code); seeds 31..34 there, the first set here
    CHECK(run(false, false, 20, 20, 20) == 0xe9e9f5a43cb300bcull);
}
