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

// Microsim/label allocation (0x44C8E0) returns 0 for every building id that the zone 7..9 code places (none of
// 0xDD..0xF9 placed here has a microsim class in the table at 0x4E6C8A), so no overlay is allocated.
bool placeSpecialBuilding(SimContext& c, int a, int b, int id, int size) {
    int sz = size - 1;
    if (sz > 1) { --a; --b; }
    const int a1 = a + sz, b1 = b + sz;
    int waterCount = 0;
    for (int x = a; x <= a1; ++x)
        for (int y = b; y <= b1; ++y) {
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
    if (id == 0xF8 && (waterCount == 0 || waterCount == 9)) return false;   // original posts message 0x6B
    const int flags = (id == 0xD5 || id == 0xD) ? 0x20 : 0xE0;
    if (id == 0xD && bldAt(c, a, b) > 0xC) return false;
    for (int x = a; x <= a1; ++x)
        for (int y = b; y <= b1; ++y) {
            c.w.xbit.at(x, y) = static_cast<uint8_t>((c.w.xbit.at(x, y) & 0x1F) | flags);
            setTile(c, x, y, id);
            c.w.xzon.at(x, y) &= 0xF0;
            c.w.xzon.at(x, y) &= 0x0F;
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
