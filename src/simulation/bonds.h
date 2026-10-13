// City bonds (SIMCITY.EXE build .022): asset value 0x469BF0, issuing 0x41ABF0 / 0x473F20 and repaying 0x4740B0. Bonds are $10,000 each, at most 50;
// `CityState::debt` is their number, `bondRates` their interest rates (oldest first) and budget[kBond] holds count and average rate (x10000).
// The confirmation dialogs of the originals are `ZoneHooks::confirm` (ids 0x121 issue, 0x123 repay, amount = the rate).
#pragma once
#include "simulation/sim_context.h"

namespace sc2k {

int32_t computeCityValue(const CityState& s, const GameTables& t);           // 0x469BF0
int currentBondRate(const CityState& s);                                    // the rate a new bond would get: federal rate + 1
bool bondsAvailable(const CityState& s, const GameTables& t);               // issue allowed by the city-value rule of 0x41ABF0
void refreshBondBudget(CityState& s);                                      // budget[kBond] from the count and rates
bool issueBond(SimContext& c);                                              // 0x41ABF0 + 0x473F20
bool repayOldestBond(SimContext& c);                                        // 0x4740B0

}  // namespace sc2k
