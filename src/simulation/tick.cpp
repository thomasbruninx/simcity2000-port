// Random events, history graphs, disaster chooser and the 25-phase simulation tick (SIMCITY.EXE build .022:
// second half of 0x467AE0, 0x413910, 0x458040, 0x4720A0, 0x412FE0). UI work (dialogs, redraws, sounds) is omitted or routed
// through ZoneHooks. See docs/simulation/monthly-phases.md and docs/simulation/events-and-tick.md.
#include "simulation/demand.h"
#include "simulation/environment.h"
#include "simulation/finance.h"
#include "simulation/power.h"
#include "simulation/sim_context.h"
#include "simulation/water.h"

namespace sc2k {

namespace {
using namespace xbit;
int quarter(int v) { return static_cast<int16_t>((v + ((v >> 31) & 3)) >> 2); }
}  // namespace

// ---------------------------------------------------------------------------------------------------------------------
// second half of 0x467AE0
void runEvents(SimContext& c) {
    CityState& s = c.s;
    World& w = c.w;
    const GameTables& t = c.t;
    auto rnd = [&] { return c.rng.next(); };
    auto rmod = [&](int n) { return static_cast<int>(static_cast<uint32_t>(rnd() & 0xFFFF) % static_cast<uint32_t>(n)); };
    auto msg = [&](int id, int arg) { c.hooks.postMessage(id, arg); };
    auto water = [&](int a, int b) { return inMap(a, b) && (w.xbit.at(a, b) & kWater); };

    // --- one random tile: rubble clears, fire burns on and spreads ---
    int a = rnd() & 0x7F;
    int b = rnd() & 0x7F;
    const int id = w.xbld.at(a, b);
    if (id == 5 && (rnd() & 0xF) == 0) setTile(c, a, b, 0);
    if (!water(a, b) && ((id > 5 && id < 0xE) || (rnd() & 0xF) == 0)) {
        if (id > 5 && id < 0xC) setTile(c, a, b, id + 1);
        switch (rnd() & 3) {
            case 0: if (a < 0x7F) ++a; break;
            case 1: if (a != 0) --a; break;
            case 2: if (b < 0x7F) ++b; break;
            default: if (b != 0) --b; break;
        }
        const int id2 = w.xbld.at(a, b);
        if (!water(a, b) && id2 < 0xC && id2 != 5) setTile(c, a, b, id2 < 6 ? 6 : id2 + 1);
    }
    // 0x479F40: age the news queue (UI state), then post the monthly topic
    c.hooks.newsAge();
    msg(1, 0);
    switch (rmod(6)) {
        case 0: {
            int sel = 0x1B, arg = 0;
            if ((rnd() & 3) == 0) { sel = 0; arg = 6; msg(6, 0); }
            if ((rnd() & 3) == 0) msg(arg == 6 ? 6 : 0, sel);
            break;
        }
        case 1: msg(0xB, 0); break;
        case 2: msg(0xC, 0); break;
        case 3: msg(0xD, 0); break;
        case 4: msg(0xE, 0); break;
        default: msg(0xF, 0); break;
    }
    if (s.census[0xD7] > 0) {
        const int k = rmod(5);
        if ((s.militaryMask & (1 << k)) != 0) msg(8, k);
    }
    const int traffic = s.graph[4 * 52], pollution = s.graph[5 * 52], crime = s.graph[7 * 52];
    if ((rnd() & 0x7F) < traffic) msg(0x11, 0);
    if (traffic < static_cast<int>(rnd() & 0xF)) msg(0x3E, 0);
    if ((rnd() & 0x7F) < pollution) msg(0x12, 0);
    if (pollution < static_cast<int>(rnd() & 0xF)) msg(0x3F, 0);
    if ((rnd() & 0x7F) < crime) msg(0x10, 0);
    if (crime < static_cast<int>(rnd() & 0xF)) msg(0x3D, 0);
    if (static_cast<int>(rnd() & 0x3F) < s.c9610) msg(0x15, 0);
    if (s.c9610 < static_cast<int>(rnd() & 3)) msg(0x42, 0);
    if (s.cb4cc < 0x50u) {
        if (s.cb4cc < static_cast<uint32_t>(rmod(0x50))) msg(0x13, 0);
    } else if (static_cast<uint32_t>(rmod(0x50)) < s.cb4cc - 0x50u) {
        msg(0x40, 0);
    }
    if (s.ca428 < 0x3Cu) {
        if (static_cast<uint32_t>(rmod(0x3C)) > s.ca428) msg(0x14, 0);
    } else if (!(s.ca428 - 0x3Cu <= static_cast<uint32_t>(rmod(0x3C)))) {
        msg(0x41, 0);
    }
    // dated events (new tools, ...)
    if ((rnd() & 7) == 0) {
        const int year = static_cast<int16_t>(s.startYear) + static_cast<int16_t>(s.yearOffset());
        for (int i = 0; i < 17; ++i) {
            if (s.eventYears[static_cast<size_t>(i)] != 0 && s.eventYears[static_cast<size_t>(i)] <= year) {
                msg(i < 7 ? 4 : 5, i < 7 ? i : i - 7);
                s.eventYears[static_cast<size_t>(i)] = 0;
                c.hooks.scheduledEvent(i);
                break;
            }
        }
    }
    // weather
    const int season = ((s.month() + 1) % 12) / 3;
    const int next = t.weatherNext[static_cast<size_t>(((s.weather & 0xFF) + season * 12) * 8 + (rnd() & 7))];
    s.weather = next;
    s.cloudiness = static_cast<uint8_t>((t.weatherCloud[static_cast<size_t>(next)] + s.cloudiness) / 2);
    s.wind = static_cast<uint8_t>((t.weatherWind[static_cast<size_t>(next)] + s.wind) / 2);
    s.c9df8 = static_cast<uint8_t>(((s.c9df8 & 0xFF) + t.weatherHaze[static_cast<size_t>(next)]) / 2);
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x413910: shift and refill the history graphs
void recordHistory(SimContext& c) {
    CityState& s = c.s;
    auto g = [&](int k, int j) -> int32_t& { return s.graph[static_cast<size_t>(k) * 52 + static_cast<size_t>(j)]; };
    auto& mx = s.graphMax;
    for (int k = 0; k < 16; ++k)
        for (int j = 11; j > 0; --j) g(k, j) = g(k, j - 1);
    const int n = s.census[0xFE] + s.census[0xFB] + s.census[0xFC] + s.census[0xFD];
    const int q = (n + ((n >> 31) & 0xF)) >> 4;
    int bonus = 0;
    if (q > 0x8C) bonus = (q * 5 - 700) * 4000;
    const int base = s.extraPop + bonus;
    g(0, 0) = bonus + s.zonePop[0] * 10 + s.extraPop;
    g(1, 0) = base / 2 + s.catPop[0] * 10;
    g(2, 0) = ((base + ((base >> 31) & 3)) >> 2) + s.catPop[1] * 10;
    g(3, 0) = ((base + ((base >> 31) & 3)) >> 2) + s.catPop[2] * 10;
    int m = mx[0];
    if (m < g(0, 0)) m = g(0, 0);
    for (int k = 0; k < 4; ++k) mx[static_cast<size_t>(k)] = m;
    g(4, 0) = static_cast<int32_t>(static_cast<uint32_t>(s.trafficTotal) /
                                   (static_cast<uint32_t>(s.budget[12].currentCosts) + static_cast<uint32_t>(s.budget[11].currentCosts) + static_cast<uint32_t>(s.budget[10].currentCosts) + 1u));
    const uint32_t u = static_cast<uint32_t>(quarter(s.developedTiles) + 1);
    g(5, 0) = static_cast<int32_t>(static_cast<uint32_t>(s.pollutionTotal) / u);
    g(6, 0) = static_cast<int32_t>(static_cast<uint32_t>(s.landValueTotal) / u);
    g(7, 0) = static_cast<int32_t>(static_cast<uint32_t>(s.crimeTotal) / u);
    m = 0;
    for (int k = 4; k < 8; ++k) {
        if (m < mx[static_cast<size_t>(k)]) m = mx[static_cast<size_t>(k)];
        if (m < g(k, 0)) m = g(k, 0);
    }
    for (int k = 4; k < 8; ++k) mx[static_cast<size_t>(k)] = m;
    g(8, 0) = 100 - s.powerPct;
    g(9, 0) = 100 - s.waterPct;
    g(11, 0) = static_cast<int32_t>(s.cb4cc);
    g(10, 0) = static_cast<int32_t>(s.ca428);
    s.c9610 = static_cast<int32_t>((static_cast<uint32_t>(s.zonePop[7]) * 100u) / (static_cast<uint32_t>(s.zonePop[0]) + static_cast<uint32_t>(s.zonePop[7]) + 1u));
    g(12, 0) = s.c9610;
    g(13, 0) = static_cast<int32_t>(s.gnp);
    g(14, 0) = static_cast<int32_t>(s.nationalPop);
    g(15, 0) = static_cast<int16_t>(s.rateLevel);
    for (int k = 8; k < 16; ++k) if (mx[static_cast<size_t>(k)] < g(k, 0)) mx[static_cast<size_t>(k)] = g(k, 0);
    if (mx[13] < mx[14]) mx[13] = mx[14];
    const int month = s.month();
    if (month == 0 || month == 6) {
        for (int k = 0; k < 16; ++k) {
            for (int j = 0x1F; j > 0xC; --j) g(k, j) = g(k, j - 1);
            g(k, 0xC) = g(k, 0);
        }
    }
    if (month == 0 && s.yearOffset() % 5 == 0) {
        for (int k = 0; k < 16; ++k) {
            for (int j = 0x33; j > 0x20; --j) g(k, j) = g(k, j - 1);
            g(k, 0x20) = g(k, 0);
        }
    }
}

// 0x458040: traffic fades by a quarter per month and is summed
void trafficDecay(SimContext& c) {
    int32_t total = 0;
    for (auto& v : c.w.xtrf.v) {
        const int nv = static_cast<int16_t>(v - (static_cast<int16_t>(v) >> 2));
        v = static_cast<uint8_t>(nv);
        total += static_cast<int16_t>(nv);
    }
    c.s.trafficTotal = total;
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x4720A0: advisor topic (UI) and the random disaster request; consumes the same random numbers as the original
void chooseDisaster(SimContext& c) {
    CityState& s = c.s;
    ThingState& th = c.things;
    World& w = c.w;
    const auto& n = s.census;
    const auto& bud = s.budget;
    auto rnd = [&] { return c.rng.next(); };
    auto rmod = [&](int k) { return static_cast<int>(static_cast<uint32_t>(rnd() & 0xFFFF) % static_cast<uint32_t>(k)); };
    auto div9 = [&](int v) { return static_cast<uint32_t>(static_cast<int>(static_cast<int16_t>(v / 9))); };
    const int extra12 = s.extraPop / 12;
    const uint32_t pop = s.population10;
    int topic;
    if (s.weather < 9) {
        if (s.powerPct < 99) {
            if (pop / 100 < static_cast<uint32_t>(n[0xE9] + n[0xED] + bud[10].currentCosts)) {
                if (pop < 1000) {
                    topic = -1;
                } else {
                    const uint32_t k = pop / 20000;
                    const int d8 = n[0xD8];
                    if (k < static_cast<uint32_t>(static_cast<int16_t>((d8 + ((d8 >> 31) & 0xF)) >> 4) + static_cast<int>(static_cast<int16_t>(n[0xD2] / 9)))) {
                        if (k < div9(n[0xD3])) {
                            if (s.waterPct < 99) {
                                if (pop < 3000) topic = -1;
                                else if (pop / 25000 < div9(n[0xD1])) {
                                    if (k < div9(n[0xD6])) {
                                        if (pop < 8000) topic = -1;
                                        else if (n[0xE0] + s.c9408 < (bud[2].currentCosts - extra12) / 10000) topic = (s.c84d8 == 0 && s.ca410 == 0) ? 0xD : 7;
                                        else if (n[0xDD] + n[0xDE] + s.cb4f0 < (bud[1].currentCosts - extra12) / 2000) topic = 8;
                                        else {
                                            topic = -1;
                                            if (static_cast<int16_t>(n[0xD5] / 3) + n[0xD7] + n[0xDA] + n[0xF0 + 8] < (bud[0].currentCosts + extra12 * -2) / 1000) topic = (rnd() & 3) + 9;
                                        }
                                    } else topic = 6;
                                } else topic = 5;
                            } else topic = 4;
                        } else topic = 3;
                    } else topic = 2;
                }
            } else topic = 1;
        } else topic = 0;
    } else {
        topic = -2;
    }
    if (topic >= 0) c.hooks.postMessage(topic + 0x2E, 0);

    if (s.cb4ec != 0) return;
    const int limit = c.t.disasterGrace[static_cast<size_t>(s.econIndex) & 3];
    if (s.day / 0x19 < limit) return;
    if (limit == 0) return;   // the original divides by this value
    const int r = rmod(limit);
    if (s.weather == 10 && s.c84d8 != 0 && r < 0xF) { th.disasterRequest = 0x10; return; }
    if (s.weather == 0xB && r < 0xF) {
        th.disasterRequest = 7;
        const int fb = rmod(0x7E) + 1;
        const int fa = rmod(0x7E) + 1;
        th.focusA = fa; th.focusB = fb;
        return;
    }
    if (r != 0) return;
    const int kind = rmod(0x13);
    int pa = 0, pb = 0;   // focusB (param_1) and focusA (unaff_retaddr)
    auto center = [&](int mask, int off) { pb = (rnd() & mask) + s.centerB2 - off; pa = (rnd() & mask) + s.centerA2 - off; };
    auto anywhere = [&] { pb = rmod(0x7E) + 1; pa = rmod(0x7E) + 1; };
    bool setFocus = true;
    switch (kind) {
        case 1:
            if (static_cast<uint32_t>(s.c9df8) < static_cast<uint32_t>((rnd() & 0x7F) + 0x7F)) return;
            anywhere();
            break;
        case 4: {
            th.focusA = -1;
            int best = 0;
            for (int i = 0; i < 64; ++i)
                for (int j = 0; j < 64; ++j) {
                    const int v = w.xplt.at(i, j);
                    if (v > 0x95 && best < v && c.lfsr.mod(10) == 0) {
                        best = v;
                        pa = static_cast<int16_t>(c.lfsr.mod(10) - 5 + i * 2);
                        pb = static_cast<int16_t>(c.lfsr.mod(10) - 5 + j * 2);
                    }
                }
            if (pa < 0 || pa > 0x7F || pb < 0 || pb > 0x7F || best == 0) return;
            break;
        }
        case 6: anywhere(); break;
        case 7: if (s.weather < 8) return; anywhere(); break;
        case 8: if (s.population10 < 45000) return; center(0x1F, 0xF); break;
        case 9: if (n[0xCB] == 0) return; setFocus = false; break;
        case 10: if (n[0xCD] == 0) return; setFocus = false; break;
        case 0xD: if (s.population10 < 30000) return; /* fallthrough */
        case 3:
            if (s.c9610 < 10) return;
            if (s.c9df8 < 0xAA) return;
            center(0x1F, 0x10);
            break;
        case 0xE: if (s.weather < 9) return; /* fallthrough */
        case 2:
            if (s.ca410 == 0 && s.c84d8 == 0) return;
            if (s.weather < 3) return;
            anywhere();
            break;
        case 0xF: if (bud[2].currentCosts < 10000) return; center(0x1F, 0xF); break;
        case 0x10: if (s.weather < 8) return; if (s.c84d8 == 0) return; setFocus = false; break;
        case 0x12: if (n[0xDD] == 0) return; anywhere(); break;
        default: return;
    }
    if (setFocus) { th.focusA = static_cast<int16_t>(pa); th.focusB = static_cast<int16_t>(pb); }
    th.disasterRequest = kind;
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x412FE0: one simulation day
void refreshPower(SimContext& c) {
    PowerInputs in; in.ordinances = c.s.ordinances; in.wind = static_cast<uint8_t>(c.s.wind); in.cloudiness = static_cast<uint8_t>(c.s.cloudiness);
    const PowerResult r = runPowerScan(c.w, in, c.rng);
    c.s.powerPct = r.percentUsed; c.s.powerCapacity = r.capacity;
}

void refreshWater(SimContext& c) {
    WaterInputs in; in.rotation = c.s.rotation; in.cloudiness = static_cast<uint8_t>(c.s.cloudiness); in.waterFactor = static_cast<int16_t>(c.s.seaLevel);
    const WaterResult r = runWaterScan(c.w, in);
    c.s.waterPct = r.percent; c.s.waterSupply = static_cast<int>(r.supply);
    c.s.waterFlag = (r.served <= static_cast<unsigned>(quarter(c.s.census[0xF4]) * 2000)) ? 1 : 0;
}

void simulationTick(SimContext& c) {
    CityState& s = c.s;
    World& w = c.w;
    const GameTables& t = c.t;
    ++s.day;
    const int phase = s.day % 25;
    switch (phase) {
        case 0:
            runMonthlyFinance(s, t, c.rng, [&] { yearlyMicrosimUpdate(c); });
            for (auto& z : s.zonePop) z = 0;
            break;
        case 1: refreshPower(c); break;
        case 2: {
            EnvInputs in;
            in.ordinances = s.ordinances; in.waterFlag = s.waterFlag; in.c8440 = s.c8440; in.policeBase = s.policeBase;
            in.policeFunding = s.budget[kPolice].fundingPercent; in.fireFunding = s.budget[kFire].fundingPercent;
            in.layer4C8F70 = &w.xter;
            const EnvResult r = runEnvironmentScan(w, t, in);
            s.developedTiles = r.developedTiles; s.pollutionTotal = r.pollutionTotal; s.landValueTotal = r.landValueTotal; s.crimeTotal = r.crimeTotal;
            s.centerA2 = r.centerRow2; s.centerB2 = r.centerCol2;
            break;
        }
        case 19: trafficDecay(c); break;
        case 20: refreshWater(c); break;
        case 21: {
            DemandExtra x; x.c9408 = s.c9408; x.cb4f0 = s.cb4f0;
            runDemand(s, t, x);
            runEvents(c);
            runEconomy(c);
            recordHistory(c);
            break;
        }
        case 22: {
            const uint32_t th = t.milestonePop[static_cast<size_t>(s.milestone) & 7];
            if (th != 0 && th < s.population10) {
                const int old = s.milestone;
                ++s.milestone;
                c.hooks.postMessage(3, old & 0xFF);
                c.hooks.milestoneReached(s.milestone);
            }
            if (c.things.scenarioFlag != 0 && s.scenario) {
                int result = -1;
                if (scenarioGoalsMet(*s.scenario, s)) result = 1;
                else if (static_cast<uint16_t>(--s.scenarioMonthsLeft) == 0) result = 0;
                if (result >= 0) { c.hooks.gameOver(result); c.things.scenarioFlag = 0; }
            }
            if (s.treasury < -100000) { c.hooks.gameOver(2); c.things.scenarioFlag = 0; }
            break;
        }
        case 24: chooseDisaster(c); break;
        default:
            if (phase >= 3 && phase <= 18) runZonePass(c, (phase - 3) / 4, (phase - 3) % 4);
            break;
    }
}

}  // namespace sc2k
