#include "simulation/new_city.h"

#include "simulation/bonds.h"

namespace sc2k {

void resetCityGlobals(World& w, CityState& s) {
    s.day = 0;
    s.milestone = 0;
    s.cb4ec = 0;
    s.startYear = 1900;
    s.rotation = 0;
    s.ratioMemory = 0;
    s.centerA2 = 0x40; s.centerB2 = 0x40;
    for (int a = 0; a < 128; ++a)
        for (int b = 0; b < 128; ++b) w.altm.at(a, b) &= 0x3FF;
}

void setupNewCity(World& w, CityState& s, const GameTables& t, CrtRand& rng, const NewCityOptions& o) {
    resetCityGlobals(w, s);
    s.startYear = (o.era + 0x26) * 0x32;
    static const uint32_t kNationalPop[4] = {10000, 25000, 60000, 150000};
    s.nationalPop = kNationalPop[static_cast<size_t>(o.era & 3)];
    s.rateLevel = 3;
    s.gnp = s.nationalPop * 3 / 10;
    s.era = o.era;
    // dated events: nominal year + rand % 20, dropped (0 = already available) when it precedes the start year
    for (size_t i = 0; i < s.eventYears.size(); ++i) {
        const int year = static_cast<int16_t>(t.eventYearBase[i] + static_cast<int16_t>(static_cast<uint16_t>(rng.next()) % 20));
        s.eventYears[i] = static_cast<int16_t>(year < s.startYear ? 0 : year);
    }
    const int difficulty = o.difficulty + 1;
    s.debt = 0;
    s.bondRates.fill(0);
    if (difficulty == 1) s.treasury = 20000;
    else if (difficulty == 2) s.treasury = 10000;
    else if (difficulty == 3) {
        s.treasury = 10000;
        s.bondRates[0] = static_cast<int16_t>(s.rateLevel);
        s.debt = 1;
    }
    refreshBondBudget(s);
    if (s.c84d8 != 0) {   // a map without neighbours: the names and connection slots are cleared
        s.nbrName.fill(0); s.nbr9d28.fill(0); s.nbr981c.fill(0); s.nbr9944.fill(0);
    }
}

}  // namespace sc2k
