// Scenario goals: the SCEN chunk of a .SCN file (loader 0x433F30) and the win / lose test run in tick phase 22 (0x412FE0, case 0x16).
// See docs/simulation/scenarios.md.
#pragma once
#include <cstdint>
#include <optional>

#include "save/city.h"

namespace sc2k {

struct CityState;

// Fields of the chunk, in the order they are stored (all integers big-endian; the first four bytes of the chunk are skipped by the loader).
// A goal of 0 is "no goal"; the maximum goals (pollution, crime, traffic) only count when positive.
struct ScenarioInfo {
    int disaster = 0;               // +0 u16: disaster kind (1..0x12) triggered when the scenario starts, 0 = none
    int focusA = 0, focusB = 0;     // +2 / +3: where it strikes
    int timeLimit = 0;              // +4 u16: months until the scenario is lost
    uint32_t population10 = 0;      // +6 u32: goal compared with MISC 1035 (population x 10)
    int32_t residential = 0, commercial = 0, industrial = 0;   // +10 / +14 / +18: budget record 'population' goals (x 10 like the record)
    int32_t funds = 0;              // +22: treasury minus the number of bonds
    int32_t landValue = 0;          // +26: total land value (MISC 9 @0x4C9458)
    int16_t scoreA = 0, scoreB = 0; // +30 / +32: city scores MISC 18 @0x4CA428 / MISC 19 @0x4CB4CC
    uint32_t maxPollution = 0, maxCrime = 0, maxTraffic = 0;   // +34 / +38 / +42
    uint8_t countIdA = 0, countIdB = 0;                        // +46 / +47: tile ids whose census must reach the counts (56-byte chunks)
    int16_t countA = 0, countB = 0;                            // +48 / +50

    static std::optional<ScenarioInfo> fromCity(const CityFile& c);
};

bool scenarioGoalsMet(const ScenarioInfo& g, const CityState& s);   // the comparison chain of phase 22

}  // namespace sc2k
