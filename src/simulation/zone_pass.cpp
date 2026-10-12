// Zone/tile update pass: literal port of SIMCITY.EXE build .022 function 0x435220 for zone types 0..6.
// Zone types 7 (military), 8 (airport), 9 (seaport) call into code that is not ported yet and are skipped, as are the
// underground-decay branch for zone types >= 10. See docs/simulation/zone-growth.md.
#include "simulation/sim_context.h"

namespace sc2k {

namespace {

using namespace xbit;

int randMod(SimContext& c, int n) { return static_cast<int>(static_cast<uint32_t>(c.rng.next() & 0xFFFF) % static_cast<uint32_t>(n)); }

// Special-building section (label 0x43574A): airport/helipad/stadium style spawners and microsim "capacity" updates.
void specialSection(SimContext& c, int a, int b, int id) {
    if (id <= 0xEC) return;
    World& w = c.w;
    auto powered = [&] { return inMap(a, b) && (w.xbit.at(a, b) & kPowered); };
    auto quarter = [](int v) { return static_cast<int16_t>((v + ((v >> 31) & 3)) >> 2); };
    if (id == 0xED && powered() && c.lfsr.mask(3) == 0) {
        if (c.hooks.thingLimitA() < quarter(c.s.census[0xED])) c.hooks.spawnHelicopterOrShip(a, b);
    } else if (id == 0xF8 && powered() && c.lfsr.mask(3) == 0) {
        if (c.hooks.thingLimitB() < c.s.census[0xF8] / 9) c.hooks.spawnTrain(a, b);
    } else if (id > 0xFA && id < 0xFF && (w.xzon.at(a, b) & 0xF0) == 0x80) {
        const int micro = w.xtxt.at(a, b);
        if (micro > 0x32 && micro < 0xC9) {
            const size_t rec = static_cast<size_t>(micro - 0x33) * 8;
            const int kind = w.xmic[rec];
            if (kind > 0xFA && kind != 0xFF) {
                int v = (w.xval.at(a / 2, b >> 1) >> 5) - (w.xcrm.at(a / 2, b >> 1) >> 5) - (w.xplt.at(a / 2, b >> 1) >> 5) + 0xC;
                if (!powered()) v = static_cast<int16_t>(v) / 2;
                if (!(inMap(a, b) && (w.xbit.at(a, b) & kWatered))) v = static_cast<int16_t>(v) / 2;
                if (static_cast<int16_t>(v) < 0) v = 0;
                if (static_cast<int16_t>(v) > 0xC) v = 0xC;
                w.xmic[rec + 1] = static_cast<uint8_t>(v);
            }
        }
    }
}

}  // namespace

void runZonePass(SimContext& c, int a0, int b0) {
    World& w = c.w;
    CityState& s = c.s;
    const GameTables& t = c.t;
    const bool bigCity = static_cast<uint32_t>(static_cast<int>(s.census[0xF7]) * 2500) < s.population10;   // bVar14
    const int rotMask = static_cast<uint16_t>(t.rotationZoneMask[static_cast<size_t>(s.rotation & 3)]);
    auto fund = [&](int category) { return static_cast<int16_t>(s.budget[static_cast<size_t>(category)].fundingPercent); };

    for (int a = a0; a < 0x80; a += 4) {
        for (int b = b0; b < 0x80; b += 4) {
            const int zbyte = w.xzon.at(a, b);
            const int zone = zbyte & 0xF;
            const int id = w.xbld.at(a, b);

            if (zone == 0) {
                if (id <= 0x1C) continue;
                if (c.lfsr.mask(0x7F) != 0) { specialSection(c, a, b, id); continue; }
                if (!isRoad(id)) {
                    if (!isRail(id)) {
                        if (!isBridge(id)) {
                            if (id < 0x3F || id > 0x42) {
                                if (isHighway(id)) {
                                    const int p = fund(kHighway);
                                    if ((a & 1) || (b & 1) || p == 100 || randMod(c, 100) < p) continue;
                                    for (int k = 0; k < 4; ++k) {
                                        const int x = a + (k & 1), y = b + (k >> 1);
                                        const bool water = inMap(x, y) && (w.xbit.at(x, y) & kWater);
                                        setTile(c, x, y, water ? 0 : (c.rng.next() & 3) + 1);
                                    }
                                }
                            } else {
                                const int p = fund(kTunnel);
                                if (p == 100 || randMod(c, 100) < p) continue;
                                c.hooks.scrollTo(a, b);
                                destroyTile(c, a, b, 1);
                            }
                        } else {
                            const int p = fund(kBridge);
                            if (p == 100 || (randMod(c, 0x32) + s.wind) < p) continue;
                            c.hooks.scrollTo(a, b);
                            destroyTile(c, a, b, 1);
                            c.hooks.postMessage(0x27, 0);
                        }
                        specialSection(c, a, b, id);
                    } else {   // rail
                        const int p = fund(kRail);
                        if (p != 100 && p <= randMod(c, 100)) {
                            setTile(c, a, b, (c.rng.next() & 3) + 1);
                            w.xbit.at(a, b) &= 0x7F;
                            specialSection(c, a, b, id);
                        }
                    }
                } else {       // road
                    const int p = fund(kRoad);
                    if (p != 100 && p <= randMod(c, 100)) {
                        setTile(c, a, b, (c.rng.next() & 3) + 1);
                        w.xbit.at(a, b) &= 0x7F;
                        specialSection(c, a, b, id);
                    }
                }
                continue;
            }
            if (zone > 6) continue;   // military / airport / seaport / underground decay: not ported yet

            int sizeIdx = 0, size = 0;
            bool enter = false;
            if (id < 0x70) {
                if (id < 0x1D && hasRoadNearby(c, a, b)) { sizeIdx = 0; size = 0; enter = true; }
            } else if ((rotMask & zbyte & 0xF0) != 0) {
                sizeIdx = id - 0x70;
                size = static_cast<uint16_t>(t.buildingSize[static_cast<size_t>(sizeIdx)]);
                enter = true;
            }
            if (!enter) continue;

            // --- common growth / decline logic (label 0x4359F2) ---
            int cap6, cap7;   // sVar6 (growth weight), sVar7 (decline weight)
            if (!poweredNearby(c, a, b)) { cap6 = 0; cap7 = 4000; }
            else if (routeTraffic(c, a, b, zone, size, 100) == 0) { cap6 = 0; cap7 = 4000; }
            else { cap6 = static_cast<int16_t>(s.demand[static_cast<size_t>((zone - 1) / 2)] + 2000); cap7 = 4000 - cap6; }
            // tile class: the original indexes 0x4E64D8 (= class table 0x4E6468 + 0x70) by (id - 0x70), or by 0 for empty lots
            const int klass = t.landValueClass[static_cast<size_t>(0x70 + sizeIdx)];
            if (size > 0 && klass == 0) {
                s.zonePop[static_cast<size_t>(zone)] += t.sizePop[static_cast<size_t>(size)];
                if (static_cast<int>(c.rng.next() & 0xFFFF) < static_cast<int16_t>(cap7) / size) {
                    collapseBuilding(c, a, b, size, c.rng.next() & 1);
                    continue;
                }
            }
            if (klass == 1 && static_cast<int>(c.rng.next() & 0xFFFF) < 0x4000 / static_cast<int16_t>(size)) {
                if (bigCity && (size & 2) && zone < 3) clearLotForArcology(c, a, b);
                else stampBuilding(c, a, b, size, (zone - 1) / 2);
            } else if (klass == 2) {
                s.zonePop[7] += t.sizePop[static_cast<size_t>(size)];
                if (static_cast<int>(c.rng.next() & 0xFFFF) < (cap6 * 15) / static_cast<int16_t>(size))
                    stampBuilding(c, a, b, size, (zone - 1) / 2);
            } else if (size != 4 && ((zbyte & 1) == 0 || size < 1)) {
                const int lv = w.xval.at(a / 2, b / 2);
                const bool lvOk = zone > 4 || ((size != 1 || lv >= 0x20) && (size != 2 || lv >= 0x60) && (size != 3 || lv >= 0xC0));
                if (lvOk && static_cast<int>(c.rng.next() & 0xFFFF) < (cap6 * 3) / (size + 1))
                    growBuilding(c, a, b, size, zone);
            }
        }
    }
    // DAT_004C9D64 = -1000 (not modelled)
}

}  // namespace sc2k
