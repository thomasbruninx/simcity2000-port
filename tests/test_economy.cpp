#include "test.h"
#include "simulation/demand.h"
#include "simulation/finance.h"
#include "save/error.h"
#include "save/misc_fields.h"

#include <cstdlib>
#include <string>

using namespace sc2k;

static GameTables testTables() {
    GameTables t;
    t.demandK = {1.0, 1.0 / 150000.0, 0.01, 15.0, 600.0};
    t.econFactor = {0.0, 1.2, 1.1, 0.95};
    const int16_t tax[] = {200, 160, 120, 100, 75, 50, 25, 0, -25, -50, -100, -150, -200, -250, -300, -350, -400, -450, -500, -550, -600, -650, -700, 0};
    for (size_t i = 0; i < 24; ++i) t.taxDemand[i] = tax[i];
    const int16_t div[] = {75, 75, 75, 75, -100, -1, -1, -2, -4, -1, -1000, -500, -400, -250, -250, -250};
    for (size_t i = 0; i < 16; ++i) t.budgetDivisor[i] = div[i];
    return t;
}

// ---------------- demand: vectors from tools/demand_model.py ----------------
TEST(demand_matches_independent_model_case_A) {
    CityState s;
    s.zonePop = {0, 300, 2000, 100, 900, 50, 700, 0};
    s.population10 = 39000; s.ratioMemory = 23000; s.c9200 = 40; s.econIndex = 1;
    s.census[0xD5] = 30; s.census[0xD7] = 4; s.census[0xDA] = 2; s.census[0xF8] = 3;
    s.census[0xDD] = 20; s.census[0xDE] = 10; s.census[0xE0] = 7;
    s.budget[0].fundingPercent = 7; s.budget[1].fundingPercent = 6; s.budget[2].fundingPercent = 7;
    s.demand = {100, -50, 0, 0, 0, 0, 0, 0};
    DemandResult r = runDemand(s, testTables());
    CHECK(r.numerator[0] == 1796.0);
    CHECK(r.numerator[1] == 5943.746430611079);
    CHECK(r.numerator[2] == 12000.0);
    CHECK(r.valve[0] == -0.21946979574098213);
    CHECK(r.valve[1] == 4.93780862198909);
    CHECK(r.valve[2] == 14.978695073235686);
    CHECK(s.demand[0] == -31 && s.demand[1] == 2000 && s.demand[2] == 2000);
    CHECK(s.zonePop[0] == 4050 && s.population10 == 40500 && s.ratioMemory == 2300);
    CHECK(s.budget[0].currentCosts == 23000 && s.budget[1].currentCosts == 10000 && s.budget[2].currentCosts == 7500);
    CHECK(r.populationDecline == 0 && r.populationGrowth == 1500);   // 40500 vs previous 39000
}
TEST(demand_matches_independent_model_case_B_with_ordinances_and_wraparound) {
    CityState s;
    s.zonePop = {0, 10, 150, 5, 40, 30, 60, 0};
    s.population10 = 2800; s.ratioMemory = 1500; s.c9200 = -5; s.econIndex = 2; s.extraPop = 900;
    s.ordinances = 0x4000 | 0x1000 | 0x80000;
    for (int i = 0; i < 3; ++i) s.budget[static_cast<size_t>(i)].fundingPercent = 9;
    s.demand = {1990, -1990, 3, 0, 0, 0, 0, 0};
    DemandResult r = runDemand(s, testTables());
    CHECK(r.numerator[0] == 138.0);
    CHECK(r.numerator[1] == 350.40441176470586);
    CHECK(r.numerator[2] == 1042.279411764706);
    CHECK(r.valve[0] == -0.1428571428571429);
    CHECK(r.valve[1] == 6.617487212276214);
    CHECK(r.valve[2] == 10.453619909502265);
    CHECK(s.demand[0] == 1880 && s.demand[1] == 1955 && s.demand[2] == 2000);
    CHECK(r.taxIndex[0] == 8 && r.taxIndex[1] == 8 && r.taxIndex[2] == 10);   // 9-1 (0x4000), 9-1 (0x1000), 9+1 (0x80000)
    CHECK(s.budget[0].currentCosts == 1600 + 150);   // pop*10 + extra/6
    CHECK(s.budget[1].currentCosts == 450 + 75 && s.budget[2].currentCosts == 900 + 75);   // + extra/12
}
TEST(demand_empty_city_pushes_industry_to_cap) {
    CityState s;
    DemandResult r = runDemand(s, testTables());
    CHECK(r.valve[0] == -1.0 && r.valve[1] == -1.0 && r.valve[2] == 14.0);   // D = (0, 0, 15) -> E = D/(0+1) - 1
}

// ---------------- finance ----------------
TEST(finance_monthly_roll_records_and_accumulates) {
    CityState s; CrtRand rng; s.day = 25 * 3;   // month 3
    s.cb4ec = 1;                                  // disable random enactment so the RNG is untouched
    s.budget[0].currentCosts = 1000; s.budget[0].fundingPercent = 7;
    s.budget[10].currentCosts = 50; s.budget[10].fundingPercent = 100;
    runMonthlyFinance(s, testTables(), rng);
    CHECK(s.budget[0].countMonth[3] == 1000 && s.budget[0].fundMonth[3] == 7);
    CHECK(s.budget[0].yearToDate == 7000);
    CHECK(s.budget[10].countMonth[3] == 50);      // the monthly roll records the values *before* maintenance is recomputed
    CHECK(rng.state() == 1);
}
TEST(finance_january_settles_the_year_and_resets_year_to_date) {
    CityState s; CrtRand rng; s.day = 0;
    s.cb4ec = 1; s.budgetYearFlag = 1; s.treasury = 1000;
    s.budget[0].yearToDate = 900 * 5;             // divisor 75*12 = 900  -> +5
    s.budget[10].yearToDate = 1000 * 7;           // divisor -1000*12     -> 7000/-12000 = 0 (trunc)
    s.budget[4].yearToDate = -2400;               // divisor -100*12 = -1200 -> +2
    FinanceOutcome o = runMonthlyFinance(s, testTables(), rng);
    CHECK(o.yearApplied);
    CHECK(s.treasury == 1000 + 5 + 0 + 2);
    CHECK(s.budgetYearFlag == 0);
    CHECK(s.budget[0].yearToDate == 0 + s.budget[0].currentCosts * s.budget[0].fundingPercent);
}
TEST(finance_december_arms_the_year_flag) {
    CityState s; CrtRand rng; s.day = 25 * 11; s.cb4ec = 1;
    runMonthlyFinance(s, testTables(), rng);
    CHECK(s.budgetYearFlag == 1);
}
TEST(finance_maintenance_classes_from_census) {
    CityState s;
    s.debt = 77;
    s.census[0xD2] = 20; s.census[0xD3] = 18; s.census[0xD1] = 9; s.census[0xD6] = 27; s.census[0xD9] = 33;
    s.census[0x1D] = 10;  s.census[0x2B] = 5;                    // roads
    s.census[0x30] = 7;                                          // rail
    s.census[0x55] = 3;                                          // bridge
    s.census[0x65] = 2;                                          // highway
    s.census[0x40] = 4;                                          // road AND ... (0x3F..0x42 = tunnels too)
    s.census[0xE9] = 6; s.census[0xED] = 11; s.census[0xEC] = 9; s.c9434 = 5;
    updateMaintenanceCosts(s);
    CHECK(s.budget[kBond].currentCosts == 77);
    CHECK(s.budget[kPolice].currentCosts == 2 && s.budget[kFire].currentCosts == 2 && s.budget[kHealth].currentCosts == 1);
    CHECK(s.budget[kSchool].currentCosts == 3 && s.budget[kCollege].currentCosts == 2);
    CHECK(s.budget[kRoad].currentCosts == 10 + 5 + 4 + (9 / 4) * 250);
    CHECK(s.budget[kRail].currentCosts == 7 + 11);
    CHECK(s.budget[kBridge].currentCosts == 3);
    CHECK(s.budget[kHighway].currentCosts == 2);
    CHECK(s.budget[kTunnel].currentCosts == 4);
    CHECK(s.budget[kSubway].currentCosts == 6 + 5);
}
TEST(finance_ordinance_costs_follow_the_switch_table) {
    CityState s;
    s.budget[kRes].currentCosts = 600; s.budget[kCom].currentCosts = 300; s.budget[kInd].currentCosts = 120;
    s.extraPop = 40; s.population10 = 1000;
    CHECK(ordinanceCost(s, 0) == 300);
    CHECK(ordinanceCost(s, 1) == 600);
    CHECK(ordinanceCost(s, 2) == 600);
    CHECK(ordinanceCost(s, 4) == -200);
    CHECK(ordinanceCost(s, 7) == -150);
    CHECK(ordinanceCost(s, 13) == -120);
    CHECK(ordinanceCost(s, 16) == -1040);
    s.ordinances = (1u << 1) | (1u << 6);
    updateMaintenanceCosts(s);
    CHECK(s.budget[kOrdinance].currentCosts == 600 - 300);
}
TEST(finance_random_ordinance_enactment_is_gated) {
    CityState s; CrtRand rng; s.day = 1; s.treasury = 0;          // too poor: (rand&0xFFFF)+50000 < 0 is false
    FinanceOutcome o = runMonthlyFinance(s, testTables(), rng);
    CHECK(!o.randomOrdinanceEnacted && s.ordinances == 0);
}

// ---------------- data-driven checks (SC2K_DATA_PATH) ----------------
static bool haveData() { return std::getenv("SC2K_DATA_PATH") != nullptr; }
static std::string root() { return std::getenv("SC2K_DATA_PATH"); }

TEST(city_state_round_trips_misc_words_exactly) {
    if (!haveData()) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    for (const char* name : {"CITIES/CAPE.SC2", "CITIES/CERES.SC2", "CITIES/NYC.SC2", "SCENARIO/PARIS.SCN", "DEFAULT.SC2"}) {
        CityFile c = CityFile::load(root() + "/" + name);
        CityFile c2 = c;
        CityState::fromCity(c).writeBack(c2);
        CHECK(c.misc == c2.misc);
    }
}
TEST(maintenance_classes_match_saved_budget_for_cities_saved_at_phase_0) {
    if (!haveData()) return;
    for (const char* name : {"CITIES/LAKELAND.SC2", "SCENARIO/PARIS.SCN", "CITIES/TOMG-B1.SC2"}) {
        CityFile c = CityFile::load(root() + "/" + name);
        CityState saved = CityState::fromCity(c), ours = saved;
        CHECK(saved.phase() == 0);
        updateMaintenanceCosts(ours);
        for (int k = kOrdinance; k <= kTunnel; ++k) CHECK(ours.budget[static_cast<size_t>(k)].currentCosts == saved.budget[static_cast<size_t>(k)].currentCosts);
    }
}
TEST(year_to_date_equals_sum_of_monthly_records_in_game_saved_cities) {
    if (!haveData()) return;
    for (const char* name : {"CITIES/NYC.SC2", "CITIES/TOKYO.SC2", "CITIES/LAKELAND.SC2", "SCENARIO/PARIS.SCN", "CITIES/SPIRALOP.SC2"}) {
        CityState s = CityState::fromCity(CityFile::load(root() + "/" + name));
        for (const auto& r : s.budget) {
            int64_t sum = 0;
            for (int m = 0; m <= s.month(); ++m) sum += static_cast<int64_t>(r.countMonth[static_cast<size_t>(m)]) * r.fundMonth[static_cast<size_t>(m)];
            CHECK(r.yearToDate == static_cast<int32_t>(sum));
        }
    }
}
TEST(demand_pass_reproduces_population_fields_in_cities_saved_after_phase_21) {
    if (!haveData()) return;
    const GameTables t = GameTables::fromExe(root() + "/SIMCITY.EXE");
    for (const char* name : {"CITIES/CAPE.SC2", "SCENARIO/FLINT.SCN", "CITIES/EM.SC2", "CITIES/HAPPYLND.SC2", "CITIES/HI-LOH2O.SC2", "CITIES/NEWMONRO.SC2", "CITIES/ROME.SC2"}) {
        CityState saved = CityState::fromCity(CityFile::load(root() + "/" + name)), ours = saved;
        CHECK(saved.phase() >= 21);
        runDemand(ours, t);
        CHECK(ours.zonePop[0] == saved.zonePop[0]);
        CHECK(ours.population10 == saved.population10);
        for (size_t i = 0; i < 3; ++i) CHECK(ours.budget[i].currentCosts == saved.budget[i].currentCosts);   // incl. extraPop/6, /12
        // Only files with the 1995 semantics of MISC word 29 (= residential population, here FLINT) can be stepped meaningfully:
        // in files saved by another build it holds last month's residential graph value (~10x larger), which changes the ratio.
        if (saved.ratioMemory == saved.zonePop[1] + saved.zonePop[2])
            for (size_t i = 0; i < 3; ++i) CHECK(std::abs(int(ours.demand[i]) - int(saved.demand[i])) <= 400);
    }
}
