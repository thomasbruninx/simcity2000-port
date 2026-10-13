// The eight advisors: the port of SIMCITY.EXE's advice selection FUN_00419e10 (the advisor window's initialisation). Each advisor looks at the city's
// indicators and picks one sentence of the string table: advice index k -> string 294 + k (see docs/simulation/advisors.md).
#pragma once
#include "simulation/sim_context.h"

namespace sc2k {

constexpr int kAdvisorCount = 8;
constexpr int kNoAdvice = 0;     // "I have no advice at this time."

// Advice index (string id = 294 + index) of advisor 0..7. Advisor 5 (health) draws one rand(); the others are pure functions of the state.
int advisorAdvice(SimContext& c, int advisor);
int adviceString(int index);     // 294 + index
// Portrait bitmap ids of the advisors in the exe (RT_BITMAP 197..204).
int advisorPortrait(int advisor);

// The opinion poll 0x4729F0 (run when the newspaper opens): 100 random citizens each name the concern they pick from a roulette weighted by traffic,
// pollution, crime, tax rate x3, unemployment, education and health shortfalls (plus the land-value level as 'no complaint' weight). Fills CityState::
// approval, pollOrder and pollCount (descending) and draws from the shared rand(). Returns true when the approval crossed 80 % upwards (a message box).
bool runOpinionPoll(SimContext& c);

}  // namespace sc2k
