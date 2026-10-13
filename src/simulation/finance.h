// Monthly finance (phase 0, SIMCITY.EXE build .022 function 0x473980) and ordinance cost table (0x42B590).
// Recorded evidence: docs/simulation/finance.md. UI calls of the original (budget dialog, menus) are not ported.
#pragma once
#include <functional>
#include "simulation/city_state.h"
#include "simulation/rng.h"
#include "simulation/tables.h"

namespace sc2k {

// 0x42B590: yearly cost (negative) / income (positive) of ordinance `index` (0..19; 0x10..0x13 are
// out-of-range slots of the same switch). Depends on the zone populations in budget[0..2] and extraPop/population10.
int32_t ordinanceCost(const CityState& s, int index);

// Recompute the maintenance classes of budget categories 3..15 from the tile census (0x473980, second half).
// Does not touch the zone categories 0..2 or the monthly records.
void updateMaintenanceCosts(CityState& s);

struct FinanceOutcome {
    bool yearApplied = false;        // January settlement happened
    bool randomOrdinanceEnacted = false;
    int ordinanceBit = -1;
};

// Budget-window arithmetic (the yearly settlement divides yearToDate by divisor*12, so these are consistent with it):
long long budgetAnnual(const CityState& s, const GameTables& t, int category);    // yearly amount at the current rate (income > 0, expense < 0)
long long budgetToDate(const CityState& s, const GameTables& t, int category);    // what the January settlement would pay out for this category now
long long budgetYearEnd(const CityState& s, const GameTables& t, int category);   // to date + the remaining months at the current rate (port estimate)

// One execution of the whole routine at the start of a month. `month` is derived from s.day like the original.
// `yearlyUpdate` runs in the January settlement right after the treasury was credited (original: call to 0x44DD10).
FinanceOutcome runMonthlyFinance(CityState& s, const GameTables& t, CrtRand& rng, const std::function<void()>& yearlyUpdate = {});

}  // namespace sc2k
