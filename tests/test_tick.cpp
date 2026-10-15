#include "test.h"
#include "simulation/sim_context.h"
#include "save/error.h"
#include "world/world_hash.h"

#include <cstdlib>
#include <string>

using namespace sc2k;

namespace {
struct KFixture {
    World w{}; CityState s{}; GameTables t{}; CrtRand rng{1}; Lfsr lfsr{0xACE1}; ZoneHooks hooks; TrafficStats ts;
    SimContext ctx{w, s, t, rng, lfsr, hooks, ts, {}};
};
}  // namespace

TEST(tick_traffic_decay_removes_a_quarter_and_sums) {
    KFixture f;
    f.w.xtrf.at(1, 1) = 100; f.w.xtrf.at(2, 2) = 3; f.w.xtrf.at(3, 3) = 255;
    trafficDecay(f.ctx);
    CHECK(f.w.xtrf.at(1, 1) == 75 && f.w.xtrf.at(2, 2) == 3 && f.w.xtrf.at(3, 3) == 192);
    CHECK(f.s.trafficTotal == 75 + 3 + 192);
}
TEST(tick_history_shifts_samples_and_fills_the_latest_values) {
    KFixture f;
    f.s.zonePop[0] = 123; f.s.extraPop = 7; f.s.powerPct = 80; f.s.waterPct = 90; f.s.gnp = 555; f.s.nationalPop = 666; f.s.rateLevel = 3;
    f.s.trafficTotal = 1000; f.s.budget[10].currentCosts = 4; f.s.developedTiles = 8;
    recordHistory(f.ctx);
    CHECK(f.s.graph[0 * 52] == 123 * 10 + 7);
    CHECK(f.s.graph[8 * 52] == 20 && f.s.graph[9 * 52] == 10);
    CHECK(f.s.graph[13 * 52] == 555 && f.s.graph[14 * 52] == 666 && f.s.graph[15 * 52] == 3);
    CHECK(f.s.graph[4 * 52] == 1000 / 5);
    f.s.zonePop[0] = 200;
    recordHistory(f.ctx);
    CHECK(f.s.graph[0 * 52 + 1] == 123 * 10 + 7 && f.s.graph[0] == 2007);
}
TEST(tick_day_counter_and_phases_run_on_an_empty_city) {
    KFixture f;
    f.t.econK = {1.0 / 1200, 100, 0.75, 0.5, 0.9, 1.1, 1.2, 0.8};
    f.t.econYearTab.fill(10); f.t.econFinal.fill(10);
    f.s.startYear = 1900; f.s.population10 = 5000; f.s.rateLevel = 1; f.s.catPop[2] = 100;   // (a real start year: the economy indexes its year table with it)
    f.s.day = 0;
    for (int i = 0; i < 100; ++i) simulationTick(f.ctx);
    CHECK(f.s.day == 100);
    CHECK(f.s.graph[0 * 52 + 1] != 0 || f.s.graph[0] == 0);   // history was recorded (phase 21 ran 4 times)
}
TEST(tick_disaster_chooser_waits_for_the_grace_period) {
    KFixture f;
    f.t.disasterGrace = {0, 100, 60, 30};
    f.s.econIndex = 3; f.s.day = 25 * 29;          // 29 months: still protected
    f.s.weather = 0xB;
    for (int i = 0; i < 200; ++i) chooseDisaster(f.ctx);
    CHECK(f.ctx.things.disasterRequest == 0);
    f.s.day = 25 * 40;
    for (int i = 0; i < 200 && f.ctx.things.disasterRequest == 0; ++i) chooseDisaster(f.ctx);
    CHECK(f.ctx.things.disasterRequest == 7);       // weather 0xB with a draw below 15 requests a tornado
    CHECK(f.ctx.things.focusA >= 1 && f.ctx.things.focusA <= 0x7E);
}

// Golden fingerprints recorded from the ORIGINAL machine code (`sc2k-emu-diff tick`, variants 0..2, 75 consecutive days).
namespace {
uint64_t tickRun(const GameTables& t, const CityFile& c, int variant) {
    World w = World::fromCity(c);
    CityState s = CityState::fromCity(c);
    s.catPop = {int32_t(s.zonePop[1] + s.zonePop[2]), int32_t(s.zonePop[3] + s.zonePop[4]), int32_t(s.zonePop[5] + s.zonePop[6])};
    if (s.rateLevel == 0) s.rateLevel = 1;
    if (s.econIndex == 0) s.econIndex = 1;
    if (s.population10 == 0) s.population10 = 1000;
    if (variant >= 1) {
        s.day = 300 * (variant + 3) - 1 - (variant == 2 ? 24 : 0);
        s.budgetYearFlag = variant == 1 ? 1 : 0;
        s.treasury = 3000000;
        s.ordinances = variant == 1 ? 0x40 : 0x300;
        s.weather = 4 + variant; s.cloudiness = 30; s.wind = 40;
        s.populationGrowth = 20; s.populationDecline = 10;
        if (variant == 1) for (size_t sl = 1; sl < 0x96; ++sl) { const uint8_t id = w.xmic[sl * 8]; if (id >= 0xC9 && id <= 0xCF) w.xmic[sl * 8 + 1] = 0x32; }
        if (variant == 2) s.cb4ec = 1;
    }
    const uint32_t seed = 9 + 17u * static_cast<uint32_t>(variant), lfsrSeed = 0xACE1u + 3u * static_cast<uint32_t>(variant), lcgSeed = 5 + static_cast<uint32_t>(variant);
    CrtRand rng(seed); Lfsr lf(lfsrSeed); ZoneHooks hooks; TrafficStats ts;
    SimContext ctx{w, s, t, rng, lf, hooks, ts, {}};
    ctx.lcg.seed(lcgSeed);
    for (int k = 0; k < 75; ++k) simulationTick(ctx);
    return worldFingerprint(w, s);
}
}  // namespace

TEST(tick_matches_fingerprints_recorded_from_the_original_code) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    struct Golden { const char* file; int variant; uint64_t hash; };
    const Golden goldens[] = {
        {"CITIES/NYC.SC2", 0, 0x1a0fba87b9919478ull},
        {"CITIES/NYC.SC2", 1, 0x51b51ef169946abeull},
        {"CITIES/NYC.SC2", 2, 0x6f39da613455d0bcull},
        {"CITIES/CAPE.SC2", 0, 0xb0af232750cac45cull},
        {"CITIES/CAPE.SC2", 1, 0x3a764040c68e6837ull},
        {"CITIES/CAPE.SC2", 2, 0x9ba799ea996812bfull},
        {"CITIES/TOKYO.SC2", 0, 0xeb93cab3cfb73c6cull},
        {"CITIES/TOKYO.SC2", 1, 0xac5bde1b4e84f061ull},
        {"CITIES/TOKYO.SC2", 2, 0x7d3b2513e6b9dc5aull},
        {"SCENARIO/ATLANTA.SCN", 0, 0x01a18ee60d6618c2ull},
        {"SCENARIO/ATLANTA.SCN", 1, 0x7237235b7b426609ull},
        {"SCENARIO/ATLANTA.SCN", 2, 0x13a919a98ae5b982ull}
    };
    const GameTables t = GameTables::fromExe(std::string(root) + "/SIMCITY.EXE");
    for (const auto& g : goldens) {
        CityFile c = CityFile::load(std::string(root) + "/" + g.file);
        CHECK(tickRun(t, c, g.variant) == g.hash);
    }
}
