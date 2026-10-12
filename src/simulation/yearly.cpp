// January upkeep of all microsim buildings: literal port of SIMCITY.EXE build .022 function 0x44DD10.
// Computes capacities / usage of hospitals, schools, parks, stadiums, ..., ages power plants (they explode after 50 years),
// accumulates the extra population of arcologies and resets the ridership counters. See docs/simulation/events-and-tick.md.
#include "simulation/sim_context.h"

namespace sc2k {

int findOverlayTile(const SimContext& c, int overlay) {
    for (int a = 0; a < 0x80; ++a)
        for (int b = 0; b < 0x80; ++b) {
            const int v = c.w.xtxt.at(a, b);
            if (v != 0 && v > 0x32 && v < 0xC9 && v - overlay == 0x33) return b * 0x100 + a;
        }
    return 0;
}

void yearlyMicrosimUpdate(SimContext& c) {
    CityState& s = c.s;
    World& w = c.w;
    const auto& n = s.census;
    const auto& bud = s.budget;
    auto rnd = [&] { return c.rng.next(); };
    auto rmodu = [&](uint32_t k) { return static_cast<uint32_t>(rnd() & 0xFFFF) % k; };
    auto rd = [&](size_t o) { return static_cast<uint16_t>(w.xmic[o] | (w.xmic[o + 1] << 8)); };
    auto wr = [&](size_t o, int v) { w.xmic[o] = static_cast<uint8_t>(v); w.xmic[o + 1] = static_cast<uint8_t>(v >> 8); };
    auto pl = [&](int limit, int div) { return static_cast<uint32_t>(populationLimit(c, limit, div)); };
    auto fund = [&](int k) { return bud[static_cast<size_t>(k)].fundingPercent; };

    const int oldPolice = static_cast<int16_t>(s.policeBase);
    s.policeBase = 0;
    int32_t extra = 0;
    int arcologies = 0;
    const uint32_t popDiv50 = s.population10 / 0x32u;
    const int arcSum = n[0xFE] + n[0xFB] + n[0xFC] + n[0xFD];
    const int arc16 = static_cast<int16_t>((arcSum + ((arcSum >> 31) & 0xF)) >> 4);
    const int d8 = n[0xD8];
    const int hi28 = static_cast<int16_t>((d8 + ((d8 >> 31) & 0xF)) >> 4);   // high word of the original local_28
    uint32_t lo28 = 0;
    const int sch = n[0xD6], col = n[0xD9];
    bool warnA = false, warnB = false;
    auto quarter = [](int v) { return static_cast<int16_t>((v + ((v >> 31) & 3)) >> 2); };
    TrafficStats& tr = c.traffic;

    for (int slot = 1; slot < 0x96; ++slot) {
        const size_t r = static_cast<size_t>(slot) * 8;
        const int id = w.xmic[r];
        if (id == 0) continue;
        uint8_t& b1 = w.xmic[r + 1];
        switch (id) {
            case 0xC6: case 0xC7: wr(r + 2, n[0xC6] + n[0xC7]); wr(r + 4, static_cast<int16_t>(rd(r + 2) * 0x14)); break;
            case 0xC8: wr(r + 2, n[0xC8]); wr(r + 4, n[0xC8] << 2); break;
            case 0xC9: case 0xCA: case 0xCB: case 0xCC: case 0xCD: case 0xCE: case 0xCF: {
                ++b1;
                wr(r + 4, (rnd() & 7) + static_cast<int16_t>(s.powerPct));
                if (b1 > 0x30) c.hooks.postMessage(0x24, id + 0x37);
                if (b1 > 0x32) {
                    const int found = findOverlayTile(c, slot);
                    if (found != 0) {
                        const int x = found & 0xFF, y = static_cast<int8_t>(found >> 8);
                        const int cost = c.t.plantCost[static_cast<size_t>(c.t.plantCostIndex[static_cast<size_t>(id)]) & 15];
                        if (s.cb4ec == 0) {
                            destroyTile(c, x, y, 1);
                            w.xmic[r] = 0;
                        } else if (s.treasury < cost) {
                            destroyTile(c, x, y, 1);
                            w.xmic[r] = 0;
                        } else {
                            s.treasury -= cost;
                            b1 = 0;
                        }
                    }
                }
                break;
            }
            case 0xD0: wr(r + 2, static_cast<int>(pl(200, 900))); break;
            case 0xD1: {
                wr(r + 6, fund(7) / 2);
                int32_t u = static_cast<int16_t>(static_cast<int16_t>(n[0xD1] / 9)) * 0x19;
                if (u < 1) u = 1;
                int32_t v = static_cast<int32_t>(s.population10 / static_cast<uint32_t>(u)) + (rnd() & 0xF);
                if (v > 1000) v = (rnd() & 0x7F) + 1000;
                const uint32_t u21 = pl(static_cast<int16_t>(v), 0x1E);
                wr(r + 2, static_cast<int>(u21));
                int32_t v2 = fund(7) + (rnd() & 7) + b1 * 2 - 0x18;
                if (v2 < 0) v2 = 0;
                const uint32_t u17 = pl(static_cast<int16_t>(v2), 0x78);
                wr(r + 4, static_cast<int>(u17));
                uint32_t den = u17 & 0xFFFF; if (den == 0) den = 1;
                const uint32_t q = ((u21 & 0xFFFF) * 10u) / den;
                b1 = q < 0x32 ? 0xC : (q < 0x6F ? static_cast<uint8_t>((0x6E - static_cast<int>(q)) / 5) : 0);
                break;
            }
            case 0xD2: {
                b1 = static_cast<uint8_t>(fund(5));
                wr(r + 2, static_cast<int>(pl(static_cast<int16_t>(fund(5)) * 2, 0x5A)));
                uint32_t u = static_cast<uint32_t>(static_cast<int>(static_cast<int16_t>(n[0xD2])));
                if (static_cast<int32_t>(u) < 1) u = 1;
                const uint64_t crime = static_cast<uint32_t>(s.crimeTotal);
                int iv = 5 - oldPolice;
                wr(r + 4, static_cast<int>(crime / u));
                if (iv < 1) iv = 1;
                const int total = static_cast<int>((rnd() & 0xF) + static_cast<int>(crime / u) / iv);
                wr(r + 6, total);
                if ((lo28 & 0xFFFF) + static_cast<uint32_t>(total) < 0xFFFF) lo28 = (lo28 + static_cast<uint32_t>(static_cast<int16_t>(total))) & 0xFFFF;
                else lo28 = 0xFFFF;
                break;
            }
            case 0xD3:
                b1 = static_cast<uint8_t>(fund(6));
                wr(r + 2, static_cast<int>(pl(static_cast<int16_t>(fund(6) >> 1), 0x46)));
                wr(r + 4, (rd(r + 2) >> 4) + 1);
                wr(r + 6, static_cast<int>(rmodu(0x14)) + 2);
                break;
            case 0xD4:
                wr(r + 2, static_cast<int>(pl(static_cast<int16_t>(n[0xD4] * static_cast<int16_t>(fund(9) << 2)), 0x14)));
                wr(r + 4, static_cast<int16_t>(fund(9) / 10) * n[0xD4]);
                break;
            case 0xD5: {
                uint32_t u = static_cast<uint32_t>(rd(r + 4)) * 0x19Cu;
                if (u > 65000) u = 65000;
                uint32_t u2 = s.population10 / 6u;
                if (u2 > 65000) u2 = 65000;
                if (u2 < u) u = u2;
                wr(r + 2, static_cast<int>(u));
                wr(r + 4, n[0xD5] + n[0xD]);
                wr(r + 6, static_cast<int>(pl(rd(r + 4) / 9, 0x78)));
                break;
            }
            case 0xD6: {
                wr(r + 6, quarter(fund(8)));
                if (rd(r + 6) < 0x14) c.hooks.postMessage(0x26, 0);
                uint32_t u = static_cast<uint32_t>(static_cast<int>(static_cast<int16_t>(sch / 9)));
                if (static_cast<int32_t>(u) < 1) u = 1;
                const uint32_t jobs = s.jobsA[2] + s.jobsA[1];
                int32_t v = static_cast<int32_t>((rnd() & 0xF) + jobs / u);
                if (v > 0x5DC) v = static_cast<int32_t>((rnd() & 0xFF) + 0x5DC);
                const uint32_t u21 = pl(static_cast<int16_t>(v), 0x14);
                wr(r + 2, static_cast<int>(u21));
                int32_t v2 = static_cast<int32_t>(rnd() & 7) + (fund(8) * 6) / 10 + (static_cast<int>(b1) - 0xC);
                if (v2 < 0) v2 = 0;
                const uint32_t u17 = pl(static_cast<int16_t>(v2), 100);
                wr(r + 4, static_cast<int>(u17));
                uint32_t den = u17 & 0xFFFF; if (den == 0) den = 1;
                const uint32_t q = (u21 & 0xFFFF) / den;
                int bb;
                if (q < 0xF) bb = 0xC; else if (q < 0x34) bb = (0x33 - static_cast<int>(q)) / 3; else bb = 0;
                b1 = static_cast<uint8_t>(bb);
                if (bb < 4) warnA = true;
                break;
            }
            case 0xD7: {
                uint32_t u = static_cast<uint32_t>(static_cast<int>(static_cast<int16_t>(n[0xD7])));
                if (static_cast<int32_t>(u) < 1) u = 1;
                const int q = (arcSum + ((arcSum >> 31) & 0xF)) >> 4;
                int bonus = 0;
                if (q > 0x8C) bonus = (q * 5 - 700) * 4000;
                uint32_t v = static_cast<uint32_t>(s.extraPop + bonus + static_cast<int32_t>(s.population10)) / u;
                if (static_cast<int32_t>(v) > 25000) v = 25000 - (rnd() & 0xFF);
                const uint32_t pv = pl(static_cast<int16_t>(v), 5);
                wr(r + 2, static_cast<int>((rnd() & 0xFF) + static_cast<int16_t>(pv)));
                b1 = static_cast<uint8_t>((rnd() & 0x1F) + 9);
                break;
            }
            case 0xD8: {
                int dd = hi28; if (dd < 1) dd = 1;
                int32_t v = static_cast<int32_t>(rd(r + 2)) - (static_cast<int32_t>(rd(r + 2)) >> 2) + static_cast<int32_t>(static_cast<uint64_t>(static_cast<uint16_t>(s.c9448)) / static_cast<uint64_t>(dd));
                if (v > 10000) v = static_cast<int32_t>((rnd() & 0x3FF) + 10000);
                wr(r + 2, static_cast<int>(pl(static_cast<int16_t>(v), 0x14)));
                wr(r + 4, static_cast<int>(pl(static_cast<int16_t>(fund(5) * 3), 0x78)));
                const int v3 = v / 100;
                wr(r + 6, v3);
                s.policeBase = static_cast<int16_t>(s.policeBase + static_cast<int16_t>(v3));
                if (v3 > 0x69) warnB = true;
                if (v3 < 0x5B) b1 = 0;
                else {
                    const int dv = v3 - 0x5A + (100 - fund(5)) / 10;
                    const uint32_t m = static_cast<uint32_t>(rnd() & 0xFFFF);
                    if (dv != 0) b1 = static_cast<uint8_t>(static_cast<int>(m) % dv);
                }
                break;
            }
            case 0xD9: {
                wr(r + 6, fund(9));
                uint32_t u = static_cast<uint32_t>(static_cast<int>(static_cast<int16_t>((col + ((col >> 31) & 0xF)) >> 4)));
                if (static_cast<int32_t>(u) < 1) u = 1;
                int32_t v = static_cast<int32_t>(s.jobsA[3] / u + (rnd() & 0x1F));
                if (v > 5000) v = static_cast<int32_t>((rnd() & 0x1FF) + 5000);
                const uint32_t u21 = pl(static_cast<int16_t>(v), 0x1E);
                wr(r + 2, static_cast<int>(u21));
                int32_t v2 = static_cast<int32_t>(rnd() & 0xF) + fund(9) * 2 + b1 * 4 - 0x30;
                if (v2 < 0) v2 = 0;
                const uint32_t u17 = pl(static_cast<int16_t>(v2), 100);
                wr(r + 4, static_cast<int>(u17));
                uint32_t den = u17 & 0xFFFF; if (den == 0) den = 1;
                const uint32_t q = ((u21 & 0xFFFF) * 4u) / den;
                b1 = q < 0x32 ? 0xC : (q < 0x6F ? static_cast<uint8_t>((0x6E - static_cast<int>(q)) / 5) : 0);
                break;
            }
            case 0xDA:
                b1 = static_cast<uint8_t>(c.lcg.mod(100));
                wr(r + 2, c.lcg.mod(100)); wr(r + 4, c.lcg.mod(100)); wr(r + 6, c.lcg.mod(100));
                break;
            case 0xDB: wr(r + 4, static_cast<int>(rmodu(0x2A))); break;
            case 0xE9: wr(r + 2, n[0xE9]); wr(r + 6, tr.subway); break;
            case 0xEC: wr(r + 2, quarter(n[0xEC])); wr(r + 4, n[0xEC]); wr(r + 6, tr.bus); break;
            case 0xED: wr(r + 2, quarter(n[0xED])); wr(r + 6, tr.rail); break;
            case 0xF3: {
                wr(r + 4, c.t.milestoneCycle);
                const int v6 = rd(r + 6);
                if (v6 != 0) { wr(r + 6, v6 - 1); ++b1; }
                break;
            }
            case 0xF4: case 0xFA: {
                b1 = static_cast<uint8_t>((rnd() & 7) + (s.waterPct & 0xFF));
                wr(r + 2, static_cast<int>(rmodu(100)));
                uint32_t v = (rnd() & 0x1F) + 0x87;
                if (popDiv50 < v) v = popDiv50;
                wr(r + 4, static_cast<int>(v));
                break;
            }
            case 0xF5: {
                wr(r + 2, static_cast<int>(pl(static_cast<int16_t>(n[0xF5] * static_cast<int16_t>(fund(8) << 2)), 0x12)));
                const int v = rd(r + 4) + (fund(8) - 0x32) * n[0xF5];
                if (v < 32000 && v > 0) wr(r + 4, v);
                int u = static_cast<int>(s.population10); if (u < 1) u = 1;
                int v2 = (n[0xF5] * fund(8) * 300) / u;
                if (v2 > 0xC) v2 = 0xC;
                b1 = static_cast<uint8_t>(v2);
                break;
            }
            case 0xF8: {
                const int rr = c.lfsr.mod(0x14);
                wr(r + 2, static_cast<int>(pl(static_cast<int16_t>(rr + n[0xF8] * 8), 0x96)));
                break;
            }
            case 0xFE: ++arcologies; /* fallthrough */
            case 0xFB: case 0xFC: case 0xFD: {
                int sv = arc16; if (sv < 1) sv = 1;
                const uint32_t u17 = pl(static_cast<int16_t>((static_cast<uint32_t>(rd(r + 2)) * 1000u) / 10u), static_cast<int16_t>(sv * 0x14));
                int32_t u18 = ((((0x3C - fund(1)) - fund(0)) - fund(2)) / 6 + static_cast<int>(b1)) * 5 - 0x32;
                u18 *= 0x28;
                int32_t u21 = static_cast<int32_t>(u17 & 0xFFFF);
                if (u18 < u21) u21 = u18;
                int32_t v = u21 + rd(r + 4) / 0x32 + rd(r + 4);
                const int32_t cap = static_cast<int32_t>(rd(r + 2)) * 1000;
                if (cap < v) v = cap;
                const int rr = c.lfsr.mask(0x3F);
                wr(r + 4, static_cast<int16_t>(rr) + static_cast<int16_t>(v));
                extra += rd(r + 4);
                break;
            }
            case 0xFF:
                if (c.hooks.specialCityName()) {
                    wr(r + 2, static_cast<int>((rnd() & 0x3FF) + (s.population10 >> 3)));
                    b1 = static_cast<uint8_t>(rnd() & 0x7F);
                    wr(r + 4, static_cast<int>((rnd() & 0x7F) + 10));
                } else {
                    b1 = static_cast<uint8_t>(rnd());
                    const int v = static_cast<int>((s.population10 >> 3) + (rnd() & 0x3FF));
                    wr(r + 2, v);
                    wr(r + 4, static_cast<int>((rnd() & 0x7F) + static_cast<int16_t>(v >> 3)));
                    wr(r + 6, static_cast<int>((rnd() & 0x3F) + static_cast<int16_t>(v >> 4)));
                }
                break;
            default: break;
        }
    }
    s.c9448 = static_cast<int16_t>(lo28);
    s.extraPop = extra;
    tr.bus = tr.rail = tr.subway = 0;
    s.policeBase = (hi28 < 1 || 0x4F < static_cast<int16_t>(s.policeBase) / hi28) ? 0 : 1;
    if (warnA) c.hooks.postMessage(0x26, 0);
    if (warnB) c.hooks.postMessage(0x25, 0);
    {
        const int a = n[0xFE];
        if (static_cast<int16_t>((a + ((a >> 31) & 0xF)) >> 4) > 300 && s.extraPop > 6000000) {
            c.hooks.showDialog(0x211);
            for (int x = 0; x < 0x80; ++x)
                for (int y = 0; y < 0x80; ++y)
                    if (w.xbld.at(x, y) == 0xFE) destroyTile(c, x, y, 1);
            s.treasury += arcologies * 100000;
            c.hooks.showDialog(0x212);
        }
    }
}

}  // namespace sc2k
