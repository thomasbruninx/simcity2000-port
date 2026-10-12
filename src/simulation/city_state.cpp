#include "simulation/city_state.h"

#include "save/misc_fields.h"

namespace sc2k {

CityState CityState::fromCity(const CityFile& c) {
    CityState s;
    s.day = c.miscInt(misc::kDay);
    s.treasury = c.miscInt(misc::kTreasury);
    s.debt = c.miscInt(misc::kDebt);
    s.econIndex = c.miscInt(misc::kEconIndex);
    s.popHistorySum = c.miscInt(misc::kPopHistorySum);
    s.ratioMemory = c.miscInt(misc::kRatioMemory);
    s.ordinances = c.miscWord(misc::kOrdinances);
    s.budgetYearFlag = c.miscInt(misc::kBudgetYearFlag);
    s.c9434 = static_cast<int16_t>(c.miscWord(misc::kC9434));
    s.cb4ec = c.miscInt(misc::kCb4ec);
    s.extraPop = c.miscInt(misc::kExtraPop);
    s.population10 = c.miscWord(misc::kPopulation10);
    s.c9200 = static_cast<int16_t>(c.miscWord(misc::kC9200));
    s.rotation = c.miscInt(misc::kRotation);
    s.wind = static_cast<uint8_t>(c.miscWord(misc::kWind));
    s.cloudiness = static_cast<uint8_t>(c.miscWord(misc::kCloudiness));
    s.militaryType = static_cast<uint8_t>(c.miscWord(misc::kMilitaryType));
    for (size_t i = 0; i < 16; ++i) s.militaryCounts[i] = static_cast<int16_t>(c.miscWord(misc::kMilitaryCounts + i));
    for (size_t i = 0; i < 8; ++i) {
        s.zonePop[i] = c.miscInt(misc::kZonePop + i);
        s.demand[i] = static_cast<int16_t>(c.miscWord(misc::kDemand + i));
    }
    for (size_t i = 0; i < 256; ++i) s.census[i] = static_cast<int16_t>(c.miscWord(misc::kTileCensus + i));
    for (size_t k = 0; k < 16; ++k) {
        const size_t b = misc::kBudget + k * misc::kBudgetRecordWords;
        BudgetRecord& r = s.budget[k];
        r.currentCosts = c.miscInt(b); r.fundingPercent = c.miscInt(b + 1); r.yearToDate = c.miscInt(b + 2);
        for (size_t m = 0; m < 12; ++m) { r.countMonth[m] = c.miscInt(b + 3 + 2 * m); r.fundMonth[m] = c.miscInt(b + 4 + 2 * m); }
    }
    return s;
}

void CityState::writeBack(CityFile& c) const {
    auto put = [&](size_t i, int32_t v) { c.setMiscWord(i, static_cast<uint32_t>(v)); };
    // 16-bit globals: the game stores them in a 32-bit word; keep whatever the upper half held in the file.
    auto put16 = [&](size_t i, int16_t v) { c.setMiscWord(i, (c.miscWord(i) & 0xFFFF0000u) | static_cast<uint16_t>(v)); };
    put(misc::kDay, day); put(misc::kTreasury, treasury); put(misc::kDebt, debt); put(misc::kEconIndex, econIndex);
    put(misc::kPopHistorySum, popHistorySum); put(misc::kRatioMemory, ratioMemory);
    c.setMiscWord(misc::kOrdinances, ordinances);
    put(misc::kBudgetYearFlag, budgetYearFlag); put16(misc::kC9434, static_cast<int16_t>(c9434)); put(misc::kCb4ec, cb4ec); put(misc::kExtraPop, extraPop);
    c.setMiscWord(misc::kPopulation10, population10); put16(misc::kC9200, static_cast<int16_t>(c9200));
    for (size_t i = 0; i < 8; ++i) { put(misc::kZonePop + i, zonePop[i]); put16(misc::kDemand + i, demand[i]); }
    for (size_t i = 0; i < 256; ++i) put16(misc::kTileCensus + i, census[i]);
    for (size_t i = 0; i < 16; ++i) put16(misc::kMilitaryCounts + i, militaryCounts[i]);
    // rotation/wind/cloudiness/militaryType are read-only inputs here (their MISC words are left untouched)
    for (size_t k = 0; k < 16; ++k) {
        const size_t b = misc::kBudget + k * misc::kBudgetRecordWords;
        const BudgetRecord& r = budget[k];
        put(b, r.currentCosts); put(b + 1, r.fundingPercent); put(b + 2, r.yearToDate);
        for (size_t m = 0; m < 12; ++m) { put(b + 3 + 2 * m, r.countMonth[m]); put(b + 4 + 2 * m, r.fundMonth[m]); }
    }
}

}  // namespace sc2k
