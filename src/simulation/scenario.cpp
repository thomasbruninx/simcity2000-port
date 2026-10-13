#include "simulation/scenario.h"

#include "simulation/city_state.h"

namespace sc2k {

std::optional<ScenarioInfo> ScenarioInfo::fromCity(const CityFile& c) {
    for (const auto& x : c.extras) {
        if (x.chunk.tag != "SCEN" || x.chunk.stored.size() < 4 + 46) continue;
        const uint8_t* b = x.chunk.stored.data() + 4;
        const size_t n = x.chunk.stored.size() - 4;
        auto u16 = [&](size_t o) -> uint32_t { return o + 2 <= n ? (uint32_t(b[o]) << 8 | b[o + 1]) : 0u; };
        auto u32 = [&](size_t o) -> uint32_t { return o + 4 <= n ? (uint32_t(b[o]) << 24 | uint32_t(b[o + 1]) << 16 | uint32_t(b[o + 2]) << 8 | b[o + 3]) : 0u; };
        ScenarioInfo g;
        g.disaster = static_cast<int>(u16(0)); g.focusA = b[2]; g.focusB = b[3]; g.timeLimit = static_cast<int>(u16(4));
        g.population10 = u32(6);
        g.residential = static_cast<int32_t>(u32(10)); g.commercial = static_cast<int32_t>(u32(14)); g.industrial = static_cast<int32_t>(u32(18));
        g.funds = static_cast<int32_t>(u32(22)); g.landValue = static_cast<int32_t>(u32(26));
        g.scoreA = static_cast<int16_t>(u16(30)); g.scoreB = static_cast<int16_t>(u16(32));
        g.maxPollution = u32(34); g.maxCrime = u32(38); g.maxTraffic = u32(42);
        g.countIdA = n > 46 ? b[46] : 0; g.countIdB = n > 47 ? b[47] : 0;
        g.countA = static_cast<int16_t>(u16(48)); g.countB = static_cast<int16_t>(u16(50));
        return g;
    }
    return std::nullopt;
}

bool scenarioGoalsMet(const ScenarioInfo& g, const CityState& s) {
    bool ok = static_cast<uint32_t>(static_cast<int32_t>(g.scoreB)) <= s.cb4cc && static_cast<uint32_t>(static_cast<int32_t>(g.scoreA)) <= s.ca428 &&
              g.landValue <= s.landValueTotal && g.funds <= s.treasury - s.debt && g.industrial <= s.budget[kInd].currentCosts &&
              g.commercial <= s.budget[kCom].currentCosts && g.residential <= s.budget[kRes].currentCosts && g.population10 <= s.population10;
    if (static_cast<int32_t>(g.maxPollution) > 0 && g.maxPollution < static_cast<uint32_t>(s.pollutionTotal)) ok = false;
    if (static_cast<int32_t>(g.maxCrime) > 0 && g.maxCrime < static_cast<uint32_t>(s.crimeTotal)) ok = false;
    if (static_cast<int32_t>(g.maxTraffic) > 0 && g.maxTraffic < static_cast<uint32_t>(s.trafficTotal)) ok = false;
    if (g.countIdA != 0 && s.census[g.countIdA] < g.countA) ok = false;
    if (g.countIdB != 0 && s.census[g.countIdB] < g.countB) ok = false;
    return ok;
}

}  // namespace sc2k
