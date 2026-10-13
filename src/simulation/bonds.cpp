#include "simulation/bonds.h"

#include <cstring>

namespace sc2k {
namespace {
int32_t tbl(const GameTables& t, uint32_t va) { int32_t v; std::memcpy(&v, &t.valueData[va - 0x4DACB8], 4); return v; }
int32_t cen(const CityState& s, int id) { return s.census[static_cast<size_t>(id)]; }
int16_t srl(int32_t v, int n) { return static_cast<int16_t>((v + ((v >> 31) & ((1 << n) - 1))) >> n); }   // signed divide by 2^n, truncated to 16 bits
void refreshBudget(CityState& s) {
    BudgetRecord& r = s.budget[kBond];
    r.currentCosts = s.debt;
    int32_t sum = 0;
    for (int i = 0; i < s.debt; ++i) sum += s.bondRates[static_cast<size_t>(i)];
    r.fundingPercent = s.debt == 0 ? 0 : sum * 10000 / s.debt;
}
}  // namespace

void refreshBondBudget(CityState& s) { refreshBudget(s); }

int32_t computeCityValue(const CityState& s, const GameTables& t) {
    int32_t v = s.c9434 * tbl(t, 0x4DB118);
    for (int id = 0xE; id < 0x70; ++id) {
        const int32_t n = cen(s, id);
        if (id < 0x1D) v += n * tbl(t, 0x4DB030);
        else if (id < 0x2C) v += n * tbl(t, 0x4DB0C0);
        else if (id < 0x3F) v += n * tbl(t, 0x4DB0F0);
        else if (id < 0x51) v += n * 0xF;
        else if (id > 0x60) v += n * (id < 0x6C ? tbl(t, 0x4DB0C4) : tbl(t, 0x4DB100));
        else v += n * 100;
    }
    for (int id = 0xC6; id < 0x100; ++id) {
        const int32_t n = cen(s, id);
        int32_t add = 0;
        switch (id) {
            case 0xC6: case 0xC7: case 0xC8: add = n * tbl(t, 0x4DAD20 + 4u * static_cast<uint32_t>(id)); break;
            case 0xC9: case 0xCA: case 0xCB: case 0xCC: case 0xCD: case 0xCE: case 0xCF: add = srl(n, 4) * tbl(t, 0x4DAD1C + 4u * static_cast<uint32_t>(id)); break;
            case 0xD1: add = (n / 9) * tbl(t, 0x4DB218); break;
            case 0xD2: add = (n / 9) * tbl(t, 0x4DB210); break;
            case 0xD3: add = (n / 9) * tbl(t, 0x4DB214); break;
            case 0xD4: add = (n / 9) * tbl(t, 0x4DB1EC); break;
            case 0xD5: add = (n / 9) * tbl(t, 0x4DB244); break;
            case 0xD6: add = (n / 9) * tbl(t, 0x4DB1E0); break;
            case 0xD7: add = srl(n, 4) * tbl(t, 0x4DB248); break;
            case 0xD8: add = srl(n, 4) * tbl(t, 0x4DB21C); break;
            case 0xD9: add = srl(n, 4) * tbl(t, 0x4DB1E4); break;
            case 0xDA: add = srl(n, 4) * tbl(t, 0x4DB24C); break;
            case 0xDC: add = n * tbl(t, 0x4DB064); break;
            case 0xDD: case 0xDE: case 0xE1: case 0xE4: case 0xE5: case 0xE6: case 0xEA: case 0xEE: case 0xF6: add = n * tbl(t, 0x4DB124); break;
            case 0xDF: case 0xE0: case 0xE3: case 0xF0: case 0xF2: add = n * tbl(t, 0x4DB120); break;
            case 0xE9: add = n * tbl(t, 0x4DB0FC); break;
            case 0xEB: add = srl(n, 2) * tbl(t, 0x4DB068); break;
            case 0xEC: add = srl(n, 2) * tbl(t, 0x4DB0D0); break;
            case 0xED: add = srl(n, 2) * tbl(t, 0x4DB0F8); break;
            case 0xF4: add = (n / 9) * tbl(t, 0x4DB06C); break;
            case 0xF5: add = srl(n, 2) * tbl(t, 0x4DB1E8); break;
            case 0xF8: add = (n / 9) * tbl(t, 0x4DB250); break;
            case 0xFA: add = (n / 9) * tbl(t, 0x4DB070); break;
            case 0xFB: case 0xFC: case 0xFD: case 0xFE: add = srl(n, 4) * tbl(t, 0x4DACB8 + 4u * static_cast<uint32_t>(id)); break;
            default: break;
        }
        v += add;
    }
    return v;
}

int currentBondRate(const CityState& s) { return s.rateLevel + 1; }

bool bondsAvailable(const CityState& s, const GameTables& t) {
    const uint32_t value = static_cast<uint32_t>(computeCityValue(s, t));
    return static_cast<int16_t>(static_cast<uint32_t>(s.debt) * 0x9C4u / (value + 1u)) < 6;
}

bool issueBond(SimContext& c) {
    CityState& s = c.s;
    s.cityValue = computeCityValue(s, c.t);
    if (!bondsAvailable(s, c.t)) { c.hooks.showDialog(0x125); return false; }
    if (s.debt >= 50) { c.hooks.showDialog(0x120); return false; }
    const int rate = currentBondRate(s);
    if (!c.hooks.confirm(0x121, rate)) return false;
    s.treasury += 10000;
    s.bondRates[static_cast<size_t>(s.debt)] = static_cast<int16_t>(rate);
    ++s.debt;
    refreshBudget(s);
    return true;
}

bool repayOldestBond(SimContext& c) {
    CityState& s = c.s;
    if (s.debt == 0) return false;
    if (s.treasury < 10000) { c.hooks.showDialog(0x122); return false; }
    if (!c.hooks.confirm(0x123, s.bondRates[0])) return false;
    s.treasury -= 10000;
    --s.debt;
    for (int i = 0; i < s.debt; ++i) s.bondRates[static_cast<size_t>(i)] = s.bondRates[static_cast<size_t>(i) + 1];
    refreshBudget(s);
    return true;
}

}  // namespace sc2k
