#include "simulation/city_state.h"

#include "save/misc_fields.h"

namespace sc2k {

CityState CityState::fromCity(const CityFile& c) {
    CityState s;
    s.day = c.miscInt(misc::kDay);
    s.startYear = c.miscInt(misc::kStartYear);
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
    s.mode = c.miscInt(misc::kMode);
    s.seaLevel = static_cast<int16_t>(c.miscWord(misc::kWaterFactor));
    s.militaryMask = static_cast<int16_t>(c.miscWord(misc::kMilitaryMask));
    s.rotation = c.miscInt(misc::kRotation);
    s.graph = c.xgrp;
    s.c9448 = static_cast<int16_t>(c.miscWord(1038));
    s.scenario = ScenarioInfo::fromCity(c);
    if (s.scenario) s.scenarioMonthsLeft = s.scenario->timeLimit;
    s.disasterKind = static_cast<int16_t>(c.miscWord(1041)); s.disasterActive = static_cast<int16_t>(c.miscWord(1042));
    s.milestone = static_cast<int16_t>(c.miscWord(8));
    s.weather = static_cast<uint8_t>(c.miscWord(misc::kWeather)); s.c9df8 = static_cast<uint8_t>(c.miscWord(24));
    for (size_t i = 0; i < 17; ++i) s.eventYears[i] = static_cast<int16_t>(c.miscWord(462 + i));
    for (size_t i = 0; i < 50; ++i) s.bondRates[i] = static_cast<int16_t>(c.miscWord(388 + i));
    s.c84d8 = c.miscInt(913); s.ca410 = c.miscInt(914);
    s.landValueTotal = c.miscInt(misc::kLandValueTotal); s.crimeTotal = c.miscInt(misc::kCrimeTotal); s.trafficTotal = c.miscInt(misc::kTrafficTotal);
    s.waterFlag = static_cast<int16_t>(c.miscWord(misc::kWaterFlag)); s.policeBase = static_cast<int16_t>(c.miscWord(misc::kPoliceBase));
    s.nationalPop = c.miscWord(20); s.gnp = c.miscWord(21);
    s.rateLevel = static_cast<int16_t>(c.miscWord(22)); s.era = static_cast<int16_t>(c.miscWord(23));
    s.ca478 = c.miscWord(17); s.ca428 = c.miscWord(18); s.cb4cc = c.miscWord(19);
    s.pollutionTotal = c.miscInt(misc::kPollutionTotal);
    s.c8440 = static_cast<int16_t>(c.miscWord(misc::kC8440)); s.c9610 = c.miscInt(1001);
    for (size_t i = 0; i < 20; ++i) { s.jobsA[i] = c.miscWord(31 + 3 * i); s.jobsB[i] = c.miscWord(32 + 3 * i); s.jobsC[i] = c.miscWord(33 + 3 * i); }
    for (size_t i = 0; i < 11; ++i) { s.sec940c[i] = static_cast<int16_t>(c.miscWord(91 + 3 * i)); s.sec3f8[i] = static_cast<int16_t>(c.miscWord(92 + 3 * i)); s.sec408[i] = c.miscInt(93 + 3 * i); }
    for (size_t i = 0; i < 4; ++i) { s.nbrName[i] = static_cast<int16_t>(c.miscWord(438 + 4 * i)); s.nbr9d28[i] = c.miscWord(439 + 4 * i); s.nbr981c[i] = c.miscWord(440 + 4 * i); s.nbr9944[i] = c.miscWord(441 + 4 * i); }
    s.centerA2 = c.miscInt(misc::kCityCenterX2);
    s.centerB2 = c.miscInt(misc::kCityCenterY2);
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
    put(misc::kCityCenterX2, centerA2); put(misc::kCityCenterY2, centerB2);
    put(20, static_cast<int32_t>(nationalPop)); put(21, static_cast<int32_t>(gnp));
    put16(22, static_cast<int16_t>(rateLevel)); put16(23, static_cast<int16_t>(era));
    put(17, static_cast<int32_t>(ca478)); put(18, static_cast<int32_t>(ca428)); put(19, static_cast<int32_t>(cb4cc));
    put(misc::kPollutionTotal, pollutionTotal); put16(misc::kC8440, static_cast<int16_t>(c8440)); put(1001, c9610);
    for (size_t i = 0; i < 20; ++i) { put(31 + 3 * i, static_cast<int32_t>(jobsA[i])); put(32 + 3 * i, static_cast<int32_t>(jobsB[i])); put(33 + 3 * i, static_cast<int32_t>(jobsC[i])); }
    for (size_t i = 0; i < 11; ++i) { put16(91 + 3 * i, sec940c[i]); put16(92 + 3 * i, sec3f8[i]); put(93 + 3 * i, sec408[i]); }
    for (size_t i = 0; i < 4; ++i) { put16(438 + 4 * i, nbrName[i]); put(439 + 4 * i, static_cast<int32_t>(nbr9d28[i])); put(440 + 4 * i, static_cast<int32_t>(nbr981c[i])); put(441 + 4 * i, static_cast<int32_t>(nbr9944[i])); }
    c.xgrp = graph;
    put16(1038, static_cast<int16_t>(c9448));
    put16(1041, static_cast<int16_t>(disasterKind)); put16(1042, static_cast<int16_t>(disasterActive));
    put16(8, static_cast<int16_t>(milestone));
    for (size_t i = 0; i < 17; ++i) put16(462 + i, eventYears[i]);
    for (size_t i = 0; i < 50; ++i) put16(388 + i, bondRates[i]);
    put(913, c84d8); put(914, ca410);
    put(misc::kLandValueTotal, landValueTotal); put(misc::kCrimeTotal, crimeTotal); put(misc::kTrafficTotal, trafficTotal);
    put16(misc::kWaterFlag, static_cast<int16_t>(waterFlag));
    put(misc::kStartYear, startYear);   // (a city started in 1950 / 2000 / 2050 must not come back as 1900)
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
