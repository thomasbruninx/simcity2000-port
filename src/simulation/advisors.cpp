#include "simulation/advisors.h"


namespace sc2k {

namespace {
// The budget window's projection (+0x6c of a budget record, FUN_00418340): to date plus the remaining months at the current rate, raw (not divided).
int32_t projection(const CityState& s, int category) {
    const BudgetRecord& r = s.budget[static_cast<size_t>(category)];
    const int32_t rate = r.currentCosts * r.fundingPercent;
    return s.budgetYearFlag == 0 ? (11 - s.month()) * rate + r.yearToDate : rate * 12;
}
int32_t cen(const CityState& s, int id) { return s.census[static_cast<size_t>(id)]; }
}  // namespace

int adviceString(int index) { return 294 + index; }
int advisorPortrait(int advisor) { return 197 + advisor; }

int advisorAdvice(SimContext& c, int advisor) {
    CityState& s = c.s;
    const uint32_t pop = s.population10;
    const uint32_t funds = static_cast<uint32_t>(s.treasury);   // the original compares the treasury with the population as unsigned
    const int demandSum = static_cast<int16_t>(s.demand[0]) + static_cast<int16_t>(s.demand[1]) + static_cast<int16_t>(s.demand[2]);
    const int pollution = s.graph[5 * 52], crime = s.graph[7 * 52];
    const uint32_t ord = s.ordinances;
    switch (advisor) {
        case 0:
            if (demandSum < -0x29A) return 11;
            if (funds < pop) return demandSum < 0x29B ? 13 : 12;
            return 0;
        case 1:
            if ((ord & 0x80000) == 0 && pollution > 0x1E) return 0xE;
            if ((ord & 0x10000) == 0 && s.powerPct > 0x62) return 0xF;
            if (crime > 0x1E) {
                if ((ord & 0x800) == 0) return 0x10;
                if ((ord & 0x200) != 0) return (ord & 4) == 0 ? 0 : 0x12;
                return 0x11;
            }
            if ((ord & 2) != 0 && static_cast<int16_t>(s.demand[0]) < -0x29A) return 0x13;
            if ((ord & 1) != 0 && static_cast<int16_t>(s.demand[1]) < -0x29A) return 0x14;
            return 0;
        case 2: {
            if (s.treasury < -1000) return 1;
            if (s.treasury < 0) return 4;
            const uint32_t value = static_cast<uint32_t>(s.cityValue);   // 0x4C94E8: only refreshed by the bond routines
            if (static_cast<int16_t>(static_cast<uint32_t>(s.debt) * 25000u / (value + 1u)) + s.rateLevel + 1 < 4) return 2;
            if (projection(s, kBond) <= projection(s, kRes) + projection(s, kInd) + projection(s, kCom)) return 0;
            return 3;
        }
        case 3:
            if (crime > 0x28) return 7;
            if (crime > 0x1E && (ord & 0x800) == 0) return 0x10;
            if (crime > 0x13) return 6;
            return 5;
        case 4: {
            const uint32_t cap = static_cast<uint32_t>((cen(s, 0xD3) * 0x96) / 9 * s.budget[kFire].fundingPercent);
            if (cap < pop) return 10;
            if (static_cast<uint32_t>(static_cast<int32_t>(cap * 2) / 3) <= pop) return 9;
            return 8;
        }
        case 5: {
            const uint32_t cap = static_cast<uint32_t>((cen(s, 0xD1) * 0xFA) / 9 * s.budget[kHealth].fundingPercent);
            if (cap < pop) return 0x16;
            if (pop < static_cast<uint32_t>(static_cast<int32_t>(cap * 2) / 3)) return 0x15;
            switch (static_cast<uint32_t>(c.rng.next() & 0xFFFF) % 3) {
                case 0: return (ord & 0x20) != 0 ? 0x1A : 0x17;
                case 1: return (ord & 0x400) != 0 ? 0x1A : 0x18;
                default: return (ord & 0x40) != 0 ? 0x1A : 0x19;
            }
        }
        case 6: {
            const uint32_t schools = static_cast<uint32_t>((cen(s, 0xD6) * 0xF) / 9 * s.budget[kSchool].fundingPercent);
            if (schools < s.jobsA[2] + s.jobsA[1]) return 0x1C;
            const int32_t k = cen(s, 0xD9) * 0x32;
            const uint32_t colleges = static_cast<uint32_t>(((k + ((k >> 31) & 0xF)) >> 4) * s.budget[kCollege].fundingPercent);
            if (s.jobsA[3] <= colleges) return 0x1B;
            return 0x1D;
        }
        case 7: {
            uint32_t costs = 0; int32_t funding = 0;
            for (int i = kRoad; i <= kTunnel; ++i) { costs += static_cast<uint32_t>(s.budget[static_cast<size_t>(i)].currentCosts); funding += s.budget[static_cast<size_t>(i)].fundingPercent; }
            if (funding < 600) return 0x1E;
            if (costs < pop / 100) return 0x1F;
            if (costs <= pop / 10) return 0;
            return 0x20;
        }
        default: return 0;
    }
}

bool runOpinionPoll(SimContext& c) {
    CityState& s = c.s;
    int16_t weight[7];
    weight[0] = static_cast<int16_t>(s.graph[4 * 52]);
    weight[1] = static_cast<int16_t>(s.graph[5 * 52]);
    weight[2] = static_cast<int16_t>(s.graph[7 * 52]);
    weight[4] = static_cast<int16_t>(s.c9610);
    weight[3] = static_cast<int16_t>(static_cast<int16_t>(s.budget[kRes].fundingPercent) * 3);
    weight[5] = s.cb4cc < 0x65 ? static_cast<int16_t>(100 - static_cast<int16_t>(s.cb4cc)) : 0;
    weight[6] = s.ca428 < 0x47 ? static_cast<int16_t>(0x46 - static_cast<int16_t>(s.ca428)) : 0;
    int16_t total = static_cast<int16_t>(static_cast<int16_t>(s.graph[6 * 52]) + 0x32);
    int16_t counts[7] = {};
    for (int i = 0; i < 7; ++i) { s.pollOrder[static_cast<size_t>(i)] = static_cast<int16_t>(i); s.pollCount[static_cast<size_t>(i)] = 0; total = static_cast<int16_t>(total + weight[i]); }
    const int old = static_cast<int16_t>(s.approval);
    if (total == 0 || s.population10 < 100) return false;
    s.approval = 0;
    for (int n = 100; n > 0; --n) {
        int16_t roll = static_cast<int16_t>(static_cast<uint32_t>(c.rng.next() & 0xFFFF) % static_cast<uint32_t>(static_cast<int32_t>(total)));
        int k = 0;
        do {
            if (roll < weight[k]) break;
            roll = static_cast<int16_t>(roll - weight[k]);
            ++k;
        } while (k < 7);
        if (k == 7) ++s.approval; else ++counts[k];
    }
    for (int pass = 6; pass > 0; --pass)
        for (int j = 0; j < pass; ++j) {
            const int16_t a = s.pollOrder[static_cast<size_t>(j)];
            if (counts[a] < counts[s.pollOrder[static_cast<size_t>(j) + 1]]) { s.pollOrder[static_cast<size_t>(j)] = s.pollOrder[static_cast<size_t>(j) + 1]; s.pollOrder[static_cast<size_t>(j) + 1] = a; }
        }
    for (int i = 0; i < 7; ++i) s.pollCount[static_cast<size_t>(i)] = counts[s.pollOrder[static_cast<size_t>(i)]];
    return old < 0x50 && s.approval > 0x4F;
}

}  // namespace sc2k
