// Placement of military, airport and seaport buildings (zone types 7..9): ports of SIMCITY.EXE build .022
// functions 0x437C30, 0x438C20 and 0x440600. See docs/simulation/special-zones.md.
#include "simulation/sim_context.h"

namespace sc2k {

namespace {

using namespace xbit;

// Guarded accessors: the original reads out-of-range tiles through the row-pointer tables (undefined); we read 0.
int bldAt(const SimContext& c, int a, int b) { return inMap(a, b) ? c.w.xbld.at(a, b) : 0; }
int zonNibble(const SimContext& c, int a, int b) { return inMap(a, b) ? (c.w.xzon.at(a, b) & 0xF) : 0; }
bool isWater(const SimContext& c, int a, int b) { return inMap(a, b) && (c.w.xbit.at(a, b) & kWater); }

void setZoneNibble(SimContext& c, int a, int b, int zone) {
    if (inMap(a, b)) { uint8_t& z = c.w.xzon.at(a, b); z = static_cast<uint8_t>((z & 0xF0) | (zone & 0xF)); }
}
void orZone(SimContext& c, int a, int b, int bits) { if (inMap(a, b)) c.w.xzon.at(a, b) |= bits; }
void orBit(SimContext& c, int a, int b, int bits) { if (inMap(a, b)) c.w.xbit.at(a, b) |= bits; }
void andBit(SimContext& c, int a, int b, int mask) { if (inMap(a, b)) c.w.xbit.at(a, b) &= mask; }
void clearMilitaryBits(SimContext& c, int a, int b) { andBit(c, a, b, 0x0F); }   // zone 7: no power/water/flags

}  // namespace

void clearSpecialBuilding(SimContext& c, int a, int b) {
    const int id = bldAt(c, a, b);
    if (id <= 0xC5) return;
    if (id < 0xDB || id > 0xEA) {   // 2x2 building: clear the even-aligned block
        const int a0 = a & ~1, b0 = b & ~1;
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j) {
                setTile(c, a0 + i, b0 + j, 0);
                andBit(c, a0 + i, b0 + j, 0x3F);
                if (inMap(a0 + i, b0 + j)) c.w.xzon.at(a0 + i, b0 + j) &= 0x0F;
            }
    } else {
        setTile(c, a, b, 0);
        andBit(c, a, b, 0x3F);
        if (inMap(a, b)) c.w.xzon.at(a, b) &= 0x0F;
    }
}

namespace {
uint16_t rdw(const SimContext& c, size_t o) { return static_cast<uint16_t>(c.w.xmic[o] | (c.w.xmic[o + 1] << 8)); }
void wrw(SimContext& c, size_t o, int v) { c.w.xmic[o] = static_cast<uint8_t>(v); c.w.xmic[o + 1] = static_cast<uint8_t>(v >> 8); }
}  // namespace

// 0x44F150: how many of a kind the city can currently support, capped at `limit`
int populationLimit(const SimContext& c, int limit, int divisor) {
    if (static_cast<int16_t>(divisor) == 0) divisor = 100;
    const auto& n = c.s.census;
    const int sum = n[0xFE] + n[0xFB] + n[0xFC] + n[0xFD];
    const uint32_t q16 = static_cast<uint32_t>((sum + ((sum >> 31) & 0xF)) >> 4);
    int bonus = 0;
    if (q16 > 0x8C) bonus = static_cast<int>(q16 * 20000u) - 2800000;
    const uint64_t quot = static_cast<uint64_t>(static_cast<uint32_t>(c.s.extraPop + bonus + static_cast<int32_t>(c.s.population10))) / static_cast<uint32_t>(static_cast<int16_t>(divisor));
    if (static_cast<int16_t>(limit) <= static_cast<int>(quot & 0xFFFF)) return static_cast<int16_t>(limit);
    return static_cast<int>(static_cast<uint32_t>(quot));
}

// 0x44D740: initial contents of a freshly allocated microsim record
static void initMicrosim(SimContext& c, int slot, int id) {
    const size_t r = static_cast<size_t>(slot) * 8;
    const int cls = c.t.microsimClass[static_cast<size_t>(id)];
    const int year = static_cast<int16_t>(c.s.yearOffset() + c.s.startYear);
    auto rnd = [&] { return c.rng.next(); };
    if (cls < 0x11) {
        wrw(c, r + 2, 0); wrw(c, r + 4, 0); wrw(c, r + 6, 0); c.w.xmic[r + 1] = 0;
        if (static_cast<unsigned>(id - 0xC9) < 0x37u) {
            switch (id) {
                case 0xC9: wrw(c, r + 2, 0x32); break;
                case 0xCA: wrw(c, r + 2, 0xDC); break;
                case 0xCB: wrw(c, r + 2, 500); break;
                case 0xCC: wrw(c, r + 2, 0x32); break;
                case 0xCD: wrw(c, r + 2, 0x640); break;
                case 0xCE: wrw(c, r + 2, 0x9C4); break;
                case 0xCF: wrw(c, r + 2, 200); break;
                case 0xD0: wrw(c, r + 2, populationLimit(c, 200, 900)); wrw(c, r + 4, year); break;
                case 0xD1: case 0xD6: case 0xD9: c.w.xmic[r + 1] = 6; wrw(c, r + 4, 0); wrw(c, r + 2, 0); break;
                case 0xD2: wrw(c, r + 2, populationLimit(c, static_cast<int16_t>(c.s.budget[kPolice].fundingPercent) * 2, 0x5A)); break;
                case 0xD3: wrw(c, r + 2, populationLimit(c, static_cast<int16_t>(c.s.budget[kFire].fundingPercent >> 1), 0x46)); wrw(c, r + 4, 4); break;
                case 0xD4: c.w.xmic[r + 1] = 100; break;
                case 0xDB: wrw(c, r + 2, year); break;
                case 0xF3: wrw(c, r + 2, year); wrw(c, r + 4, static_cast<int>(static_cast<uint32_t>(rnd() & 0xFFFF) % 0x1E) + 10); wrw(c, r + 6, static_cast<int>(static_cast<uint32_t>(rnd() & 0xFFFF) % 0x3C)); break;
                case 0xFB: wrw(c, r + 2, 0x37); c.w.xmic[r + 1] = 5; wrw(c, r + 6, year); break;
                case 0xFC: wrw(c, r + 2, 0x1E); c.w.xmic[r + 1] = 5; wrw(c, r + 6, year); break;
                case 0xFD: wrw(c, r + 2, 0x2D); c.w.xmic[r + 1] = 5; wrw(c, r + 6, year); break;
                case 0xFE: wrw(c, r + 2, 0x41); c.w.xmic[r + 1] = 5; wrw(c, r + 6, year); break;
                case 0xFF:
                    if (c.hooks.specialCityName()) wrw(c, r + 6, year);
                    else wrw(c, r + 6, rnd() & 0x3F);
                    break;
                default: break;
            }
        }
    } else if (static_cast<unsigned>(id - 0xC6) < 0x28u) {
        switch (id) {
            case 0xC6: case 0xC7: wrw(c, r + 2, rdw(c, r + 2) + 1); wrw(c, r + 4, rdw(c, r + 4) + 0x14); break;
            case 0xC8: wrw(c, r + 2, rdw(c, r + 2) + 1); wrw(c, r + 4, rdw(c, r + 4) + 4); break;
            case 0xD5: wrw(c, r + 4, rdw(c, r + 4) + 9); break;
            case 0xE9: case 0xEC: case 0xED: wrw(c, r + 2, rdw(c, r + 2) + 1); break;
            default: break;
        }
    }
}

// 0x44C8E0: reserve a microsim record (and label) for building `id`; returns the XTXT overlay value (slot + 0x33) or 0
int allocateMicrosim(SimContext& c, int id) {
    if (id < 0xC6) return 0;
    const int cls = c.t.microsimClass[static_cast<size_t>(id)];
    if (cls == 0) return 0;
    int slot;
    if (cls < 0x11) {
        slot = 10;
        do { if (c.w.xmic[static_cast<size_t>(slot) * 8] == 0) break; ++slot; } while (slot < 0x96);
    } else {
        slot = cls - 0x10;
    }
    if (slot > 0x95) {
        if (id < 0xFB) return 0;
        slot = 10;
        do { if (c.w.xmic[static_cast<size_t>(slot) * 8] < 0xFB) break; ++slot; } while (slot < 0x96);
        if (slot > 0x95) return 0;
        for (int i = 0; i < 0x80; ++i)
            for (int j = 0; j < 0x80; ++j)
                if (c.w.xtxt.at(i, j) - slot == 0x33) c.w.xtxt.at(i, j) = 0;
    }
    c.w.xmic[static_cast<size_t>(slot) * 8] = static_cast<uint8_t>(id);
    initMicrosim(c, slot, id);
    uint8_t* label = &c.w.xlab[static_cast<size_t>(slot + 0x33) * 25];
    auto copyLabel = [&](bool custom) {
        const uint8_t* src = c.hooks.labelTemplate(id, custom);
        for (int i = 0; i < 25; ++i) label[i] = src ? src[i] : 0;
    };
    // the original keeps a table of 500 optional custom name pointers (indexed by id % 500) and a built-in name table
    const bool hasCustom = c.hooks.labelTemplate(id, true) != nullptr;
    if (hasCustom) { copyLabel(true); return slot + 0x33; }
    if (cls < 0x11 || label[0] == 0) copyLabel(false);
    return slot + 0x33;
}

bool placeSpecialBuilding(SimContext& c, int a, int b, int id, int size, bool unconstrained) {
    int sz = size - 1;
    if (sz > 1) { --a; --b; }
    const int a1 = a + sz, b1 = b + sz;
    int waterCount = 0;
    for (int x = a; x <= a1; ++x)
        for (int y = b; y <= b1; ++y) {
            if (unconstrained) { if (!inMap(x, y)) return false; continue; }
            if (sz < 1) { if (!inMap(x, y)) return false; }
            else if (x < 1 || y < 1 || x > 0x7E || y > 0x7E) return false;
            const int t = c.w.xbld.at(x, y);
            if (t > 0x1C || t == 5 || t == 0xD) return false;
            if ((c.w.xzon.at(x, y) & 0xF) == 7) return false;
            const bool water = (c.w.xbit.at(x, y) & kWater) != 0;
            if (id == 0xF8 && water) { ++waterCount; continue; }
            if (c.w.xter.at(x, y) != 0) return false;
            if (water) return false;
        }
    if (!unconstrained && id == 0xF8 && (waterCount == 0 || waterCount == 9)) return false;   // original posts message 0x6B
    const int flags = (id == 0xD5 || id == 0xD) ? 0x20 : 0xE0;
    if (!unconstrained && id == 0xD && bldAt(c, a, b) > 0xC) return false;
    const int overlay = allocateMicrosim(c, id);
    for (int x = a; x <= a1; ++x)
        for (int y = b; y <= b1; ++y) {
            c.w.xbit.at(x, y) = static_cast<uint8_t>((c.w.xbit.at(x, y) & 0x1F) | flags);
            setTile(c, x, y, id);
            c.w.xzon.at(x, y) &= 0xF0;
            c.w.xzon.at(x, y) &= 0x0F;
            if (overlay != 0) c.w.xtxt.at(x, y) = static_cast<uint8_t>(overlay);
        }
    const uint8_t* corner = &c.t.rotationCorner[static_cast<size_t>(c.s.rotation & 3) * 8];
    if (sz == 0) orZone(c, a, b, 0xF0);
    else {
        auto put = [&](int x, int y, uint8_t v) { if (inMap(x, y)) c.w.xzon.at(x, y) = static_cast<uint8_t>((c.w.xzon.at(x, y) & 0xF) | v); };
        put(a, b, corner[0]); put(a1, b, corner[2]); put(a1, b1, corner[4]); put(a, b1, corner[6]);
    }
    return true;
}

bool placeZoneBuilding(SimContext& c, int a, int b, int id, int zone) {
    if (zone != 7 && !poweredNearby(c, a, b)) return false;
    const bool oddRot = (c.s.rotation & 1) != 0;
    const GameTables& t = c.t;
    switch (id) {
        case 0xDD: {   // runway: 5 tiles in a line; direction from the parity of the runway count (census[0xDD])
            int stepA = 0, stepB = 0;
            if ((c.s.census[0xDD] & 1) == 0) {
                if ((a & 1) == 0) { if ((b & 1) == 0) return false; stepA = 1; }
                else stepB = 1;
            } else if ((b & 1) == 0) {
                if ((a & 1) == 0) return false;
                stepB = 1;
            } else stepA = 1;
            int x = a, y = b;
            for (int n = 0; n < 5; ++n) {   // existing runway tiles do not count towards the 5
                if (!inMap(x, y)) return false;
                if ((c.w.xzon.at(x, y) & 0xF) != zone) return false;
                const int tile = c.w.xbld.at(x, y);
                if (tile == 0xDD || tile == 0xDE) --n;
                x += stepA; y += stepB;
            }
            const bool flipped = (stepA == 0) ? oddRot : !oddRot;
            x = a; y = b;
            for (int n = 0; n < 5; ++n) {
                const int tile = bldAt(c, x, y);
                if (tile == 0xDD || tile == 0xDE) {
                    --n;
                    if (tile == 0xDD) {
                        const bool have = inMap(x, y) && (c.w.xbit.at(x, y) & kFlipped) == kFlipped;
                        if (flipped != have) {
                            setTile(c, x, y, 0xDE);
                            orZone(c, x, y, 0xF0);
                            if (zone != 7) orBit(c, x, y, 0xC0);
                            andBit(c, x, y, 0xFD);
                        }
                    }
                } else {
                    if (tile > 0xC) clearSpecialBuilding(c, x, y);
                    setTile(c, x, y, 0xDD);
                    orZone(c, x, y, 0xF0);
                    if (zone != 7) orBit(c, x, y, 0xC0);
                    if (flipped) orBit(c, x, y, 2);
                }
                x += stepA; y += stepB;
            }
            break;
        }
        case 0xE0: {   // pier: needs water next to it and 5 clear water tiles in that direction
            int d = 0;
            for (; d < 4; ++d) {
                const int x = t.pierDx[static_cast<size_t>(d)] + a, y = t.pierDy[static_cast<size_t>(d)] + b;
                if (isWater(c, x, y)) break;
            }
            if (d == 4) return false;
            const int dx = t.pierDx[static_cast<size_t>(d)], dy = t.pierDy[static_cast<size_t>(d)];
            if (dy != 0 && (a & 1)) return false;
            if (dx != 0 && (b & 1)) return false;
            int x = a, y = b;
            for (int n = 0; n < 5; ++n) {
                x += dx; y += dy;
                if (!inMap(x, y)) return false;
                if (!isWater(c, x, y)) return false;
                if (c.w.xbld.at(x, y) != 0) return false;
            }
            const uint16_t alt = c.w.altm.at(x, y);
            if (static_cast<uint16_t>((alt & 0x3E0) >> 5) < static_cast<uint16_t>((alt & 0x1F) + 2)) return false;
            if (bldAt(c, a, b) > 0xC) clearSpecialBuilding(c, a, b);
            placeSpecialBuilding(c, a, b, 0xE0, 1);
            setZoneNibble(c, a, b, zone);
            if (zone == 7) clearMilitaryBits(c, a, b);
            const bool flipped = (dx == 0) ? oddRot : !oddRot;
            x = a; y = b;
            for (int n = 0; n < 4; ++n) {
                x += dx; y += dy;
                setTile(c, x, y, 0xDF);
                orZone(c, x, y, 0xF0);
                if (flipped) orBit(c, x, y, 2);
            }
            break;
        }
        case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: case 0xE6: case 0xE7: case 0xE8: case 0xEA:
            if (bldAt(c, a, b) < 0xD) {
                placeSpecialBuilding(c, a, b, id, 1);
                setZoneNibble(c, a, b, zone);
                if (zone == 7) clearMilitaryBits(c, a, b);
            }
            break;
        case 0xEE: case 0xEF: case 0xF0: case 0xF1: case 0xF2: case 0xF6: {   // 2x2
            const int a0 = a & ~1, b0 = b & ~1;
            const int first = bldAt(c, a0, b0);
            if (first > 0xEA || first == 0xDD || first == 0xDE || first == 0xE0) return false;
            const int ox[3] = {1, 0, 1}, oy[3] = {0, 1, 1};
            for (int k = 0; k < 3; ++k) {
                const int tile = bldAt(c, a0 + ox[k], b0 + oy[k]);
                if (tile == 0xDD || tile == 0xDE || tile == 0xE0) return false;
            }
            for (int k = 0; k < 4; ++k)
                if (zonNibble(c, a0 + (k & 1), b0 + (k >> 1)) != zone) return false;
            if (first > 0xC) clearSpecialBuilding(c, a0, b0);
            if (bldAt(c, a0 + 1, b0) > 0xC) clearSpecialBuilding(c, a0 + 1, b0);
            if (bldAt(c, a0, b0 + 1) > 0xC) clearSpecialBuilding(c, a0, b0 + 1);
            if (bldAt(c, a0 + 1, b0 + 1) > 0xC) clearSpecialBuilding(c, a0 + 1, b0 + 1);
            placeSpecialBuilding(c, a0, b0, id, 2);
            for (int k = 0; k < 4; ++k) setZoneNibble(c, a0 + (k & 1), b0 + (k >> 1), zone);
            if (zone == 7)
                for (int k = 0; k < 4; ++k) clearMilitaryBits(c, a0 + (k & 1), b0 + (k >> 1));
            break;
        }
        case 0xF9: {   // 3x3 command centre: slide the anchor towards tiles of the same zone, then stamp
            int x = a, y = b;
            if (zonNibble(c, x - 1, y) == zone) --x;
            if (zonNibble(c, x - 1, y) == zone) --x;
            if (zonNibble(c, x, y - 1) == zone) --y;
            if (zonNibble(c, x, y - 1) == zone) --y;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) {
                    setTile(c, x + i, y + j, 0xF9);
                    setUnderground(c, x + i, y + j, 0x22);
                }
            const uint8_t* corner = &t.rotationCorner[static_cast<size_t>(c.s.rotation & 3) * 8];
            auto put = [&](int px, int py, uint8_t v) { if (inMap(px, py)) c.w.xzon.at(px, py) = static_cast<uint8_t>((c.w.xzon.at(px, py) & 0xF) | v); };
            put(x, y, corner[0]); put(x + 2, y, corner[2]); put(x + 2, y + 2, corner[4]); put(x, y + 2, corner[6]);
            break;
        }
        default: break;
    }
    return true;
}

}  // namespace sc2k
