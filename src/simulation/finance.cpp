#include "simulation/finance.h"

namespace sc2k {

int32_t ordinanceCost(const CityState& s, int index) {
    const int32_t r = s.budget[kRes].currentCosts, c = s.budget[kCom].currentCosts, i = s.budget[kInd].currentCosts;
    switch (index) {
        case 0: return c;
        case 1: return r;
        case 2: return c * 2;
        case 3: return r / 2;
        case 4: return r / -3;
        case 5: return c / -6;
        case 6: return -(r / 2);
        case 7: return -(r / 4);          // original: (r + (r>>31 & 3)) >> 2 == truncating /4
        case 8: return r / -6;
        case 9: return r / -5;
        case 10: return r / -6;
        case 11: return r / -3;
        case 12: return -c;
        case 13: return -i;
        case 14: return -(r / 4);
        case 15: return c / -3;
        case 16: return -(s.extraPop + static_cast<int32_t>(s.population10));
        case 17: return 0;
        case 18: return -(r / 2);
        case 19: return -i;
        default: return 0;                // original returns an uninitialised local
    }
}

void updateMaintenanceCosts(CityState& s) {
    auto& b = s.budget;
    auto cen = [&](int id) -> int32_t { return s.census[static_cast<size_t>(id)]; };
    b[kOrdinance].currentCosts = 0;
    b[kBond].currentCosts = s.debt;
    b[kPolice].currentCosts = cen(0xD2) / 9;
    b[kFire].currentCosts = cen(0xD3) / 9;
    b[kHealth].currentCosts = cen(0xD1) / 9;
    b[kSchool].currentCosts = cen(0xD6) / 9;
    b[kCollege].currentCosts = cen(0xD9) / 16;
    for (int k = kRoad; k <= kTunnel; ++k) b[k].currentCosts = 0;

    for (int id = 0x1D; id < 0x70; ++id) {
        const int32_t n = cen(id);
        if ((id > 0x1C && id < 0x2C) || (id > 0x3E && id < 0x47) || id == 0x4B || id == 0x4C || (id > 0x5C && id < 0x61)) b[kRoad].currentCosts += n;
        if ((id > 0x2B && id < 0x3F) || (id > 0x44 && id < 0x49) || (id > 0x6B && id < 0x70) || id == 0x4D || id == 0x4E) b[kRail].currentCosts += n;
        if ((id > 0x50 && id < 0x5D) || id == 0x6A || id == 0x6B) b[kBridge].currentCosts += n;
        if ((id > 0x60 && id < 0x6C) || (id > 0x48 && id < 0x51)) b[kHighway].currentCosts += n;
        if (id > 0x3E && id < 0x43) b[kTunnel].currentCosts += n;
    }
    b[kSubway].currentCosts = cen(0xE9) + s.c9434;
    b[kRail].currentCosts += cen(0xED);
    b[kRoad].currentCosts += (cen(0xEC) / 4) * 0xFA;

    int32_t ord = 0;
    for (int bit = 0; bit < 20; ++bit)
        if (s.ordinances & (1u << bit)) ord += ordinanceCost(s, bit);
    b[kOrdinance].currentCosts = ord;
}

long long budgetAnnual(const CityState& s, const GameTables& t, int i) {
    const long long div = t.budgetDivisor[static_cast<size_t>(i)];
    return div == 0 ? 0 : static_cast<long long>(s.budget[static_cast<size_t>(i)].currentCosts) * s.budget[static_cast<size_t>(i)].fundingPercent / div;
}
long long budgetToDate(const CityState& s, const GameTables& t, int i) {
    const long long div = static_cast<long long>(t.budgetDivisor[static_cast<size_t>(i)]) * 12;
    return div == 0 ? 0 : static_cast<long long>(s.budget[static_cast<size_t>(i)].yearToDate) / div;
}
long long budgetYearEnd(const CityState& s, const GameTables& t, int i) { return budgetToDate(s, t, i) + budgetAnnual(s, t, i) * (11 - s.month()) / 12; }

FinanceOutcome runMonthlyFinance(CityState& s, const GameTables& t, CrtRand& rng, const std::function<void()>& yearlyUpdate) {
    FinanceOutcome out;
    const int month = s.month();
    if (s.budgetYearFlag != 0 && month == 0) {
        for (int i = 0; i < 16; ++i) {
            const int32_t div = static_cast<int32_t>(t.budgetDivisor[i]) * 12;
            if (div != 0) s.treasury += s.budget[i].yearToDate / div;
            s.budget[i].yearToDate = 0;
        }
        s.budgetYearFlag = 0;
        out.yearApplied = true;
        if (yearlyUpdate) yearlyUpdate();
    }
    for (auto& r : s.budget) {
        r.countMonth[month] = r.currentCosts;
        r.fundMonth[month] = r.fundingPercent;
        r.yearToDate = static_cast<int32_t>(static_cast<uint32_t>(r.yearToDate) + static_cast<uint32_t>(r.currentCosts) * static_cast<uint32_t>(r.fundingPercent));
    }
    if (month == 11) s.budgetYearFlag = 1;

    updateMaintenanceCosts(s);

    // Random enactment of an ordinance when the city is rich (conditions short-circuit in this order).
    if (s.cb4ec == 0 && (rng.next() & 7) == 0 && static_cast<int32_t>((rng.next() & 0xFFFF) + 50000) < s.treasury) {
        const int bit = static_cast<int>((rng.next() & 0xFFFF) % 20);
        s.ordinances |= 1u << bit;
        out.randomOrdinanceEnacted = true;
        out.ordinanceBit = bit;
    }
    return out;
}

}  // namespace sc2k
