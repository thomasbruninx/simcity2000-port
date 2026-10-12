// Tile primitives of the zone-growth code (SIMCITY.EXE build .022). See docs/simulation/zone-growth.md.
#include <cassert>

#include "simulation/sim_context.h"

namespace sc2k {
namespace {

using namespace xbit;

// military tile counters (MISC 1002..1017) by building id, 0x4418B0 switch tables
int militarySlot(int id) {
    switch (id) {
        case 0xDD: return 1; case 0xDE: return 2; case 0xE0: return 10; case 0xE2: return 11; case 0xE3: return 6;
        case 0xE4: return 7; case 0xE5: return 8; case 0xE7: return 12; case 0xE8: return 13; case 0xEA: return 5;
        case 0xEF: return 3; case 0xF1: return 9; case 0xF2: return 4; case 0xF6: return 14; case 0xF9: return 15;
        default: return 0;
    }
}

}  // namespace

void setTile(SimContext& c, int a, int b, int id) {
    if (!inMap(a, b)) return;
    uint8_t& tile = c.w.xbld.at(a, b);
    if ((c.w.xzon.at(a, b) & 0xF) != 7) {
        --c.s.census[tile];
        ++c.s.census[static_cast<size_t>(id) & 0xFF];
        tile = static_cast<uint8_t>(id);
        return;
    }
    --c.s.militaryCounts[static_cast<size_t>(militarySlot(tile))];
    ++c.s.militaryCounts[static_cast<size_t>(militarySlot(id))];
    tile = static_cast<uint8_t>(id);
}

void setUnderground(SimContext& c, int a, int b, int value) {
    if ((c.w.xzon.at(a, b) & 0xF) != 7) {
        if (isSubwayPiece(c.w.xund.at(a, b))) --c.s.c9434;
        if (isSubwayPiece(value) && value > 0) { ++c.s.c9434; c.w.xund.at(a, b) = static_cast<uint8_t>(value); return; }
    }
    c.w.xund.at(a, b) = static_cast<uint8_t>(value);
}

namespace {
// 24 neighbourhood offsets (a, b) filled by 0x456CE0 at start-up (order matters for the traffic search).
constexpr int kNeighbours[24][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}, {0, 2}, {2, 0}, {0, -2}, {-2, 0}, {0, 3}, {3, 0}, {0, -3}, {-3, 0},
                                    {1, 1}, {-1, 1}, {1, -1}, {-1, -1}, {2, 1}, {-2, 1}, {2, -1}, {-2, -1}, {1, 2}, {-1, 2}, {1, -2}, {-1, -2}};
}  // namespace

const int (*neighbourOffsets())[2] { return kNeighbours; }

bool hasRoadNearby(const SimContext& c, int a, int b) {
    for (const auto& o : kNeighbours) {
        const int x = a + o[0], y = b + o[1];
        if (!inMap(x, y)) continue;
        const int id = c.w.xbld.at(x, y);
        if (isRoad(id) || id == 0xEC || id == 0xED || id == 0xE9) return true;
    }
    return false;
}

bool poweredNearby(const SimContext& c, int a, int b) {
    auto powered = [&](int x, int y) { return inMap(x, y) && (c.w.xbit.at(x, y) & kPowered); };
    if (powered(a, b)) return true;
    if (a > 1 && powered(a - 1, b)) return true;          // note: original tests `1 < a`, not `0 < a`
    if (b > 1 && powered(a, b - 1)) return true;
    if (a < 0x7F && powered(a + 1, b)) return true;
    if (b < 0x7F && powered(a, b + 1)) return true;
    return false;
}

bool canBuildAt(const SimContext& c, int a, int b, int alt, int zone, int limit) {
    if (!inMap(a, b)) return false;
    if ((c.w.altm.at(a, b) & 0x1F) != alt) return false;
    if ((c.w.xzon.at(a, b) & 0xF) != zone) return false;
    const int id = c.w.xbld.at(a, b);
    if (limit <= id) return false;
    if (isRoad(id)) return false;
    if (isRail(id)) return false;
    return true;
}

void stampBuilding(SimContext& c, int a, int b, int size, int level) {
    World& w = c.w;
    if (size == 1) {
        int id;
        if (level == 0) {
            int bucket = (inMap(a, b) ? w.xval.at(a >> 1, b >> 1) : 0) >> 6;
            if (bucket > 2) bucket = 2;
            id = (c.rng.next() & 3) + bucket * 4 + c.t.buildBase[1];
        } else {
            const size_t i = static_cast<size_t>(1 + level * 4);
            id = c.t.buildBase[i] + static_cast<int>(static_cast<uint32_t>(c.rng.next() & 0xFFFF) % static_cast<uint32_t>(c.t.buildRange[i]));
        }
        setTile(c, a, b, id);
        if (a >= 0 && inMap(a, b)) {
            w.xzon.at(a, b) |= 0xF0;
            w.xbit.at(a, b) |= 0xE0;
        }
        return;
    }
    const size_t i = static_cast<size_t>(size + level * 4);
    const uint32_t r = static_cast<uint32_t>(c.rng.next());
    const int range = c.t.buildRange[i], base = c.t.buildBase[i];
    const int half = size / 2;
    if (a > 1 && b > 1 && a <= 0x7E - half && b <= 0x7E - half) {
        const int b0 = b - half;
        for (int x = a; x <= a + half; ++x)
            for (int y = b0; y <= b0 + half; ++y) {
                setTile(c, x, y, base + static_cast<int>((r & 0xFFFF) % static_cast<uint32_t>(range)));
                if (inMap(x, y)) {
                    w.xzon.at(x, y) &= 0x0F;
                    w.xbit.at(x, y) |= 0xE0;
                }
            }
        const uint8_t* corner = &c.t.rotationCorner[static_cast<size_t>(c.s.rotation & 3) * 8];
        auto setCorner = [&](int x, int y, uint8_t bits) { if (inMap(x, y)) w.xzon.at(x, y) = static_cast<uint8_t>((w.xzon.at(x, y) & 0xF) | bits); };
        setCorner(a, b0, corner[0]);
        setCorner(a + half, b0, corner[2]);
        setCorner(a + half, b0 + half, corner[4]);
        setCorner(a, b0 + half, corner[6]);
    }
}

void collapseBuilding(SimContext& c, int a, int b, int size, int flip) {
    switch (size) {
        case 1: stampBuilding(c, a, b, 1, 4); return;
        case 2:
            if (flip == 0) { stampBuilding(c, a, b, 2, 4); return; }
            stampBuilding(c, a, b, 1, 4); stampBuilding(c, a + 1, b, 1, 4); stampBuilding(c, a + 1, b - 1, 1, 4); stampBuilding(c, a, b - 1, 1, 4);
            return;
        case 3:
            if (flip == 0) stampBuilding(c, a, b, 3, 4); else stampBuilding(c, a, b, 2, 4);
            return;
        case 4:
            if (flip != 0) {
                stampBuilding(c, a, b, 1, 4); stampBuilding(c, a + 1, b, 1, 4); stampBuilding(c, a + 2, b, 1, 4); stampBuilding(c, a + 2, b - 1, 1, 4);
                stampBuilding(c, a + 2, b - 2, 1, 4); stampBuilding(c, a + 1, b - 2, 1, 4); stampBuilding(c, a, b - 2, 1, 4); stampBuilding(c, a, b - 1, 1, 4);
                const int r = c.rng.next();
                stampBuilding(c, (r & 1) + a, b - ((r & 3) / 2), 3, 4);
            } else {
                stampBuilding(c, a, b, 4, 4);
            }
            return;
        default: return;
    }
}

namespace {
// 0x437910: knock down the building whose corner tile is (a, b) (corner flags -> orientation -> 2x2 rubble stamp)
void demolishSmallBuilding(SimContext& c, int a, int b) {
    int orient = 0;   // original leaves it uninitialised for corner flag values other than 0x10/0x20/0x40/0x80
    switch (c.w.xzon.at(a, b) & 0xF0) {
        case 0x10: orient = (-c.s.rotation) & 3; break;
        case 0x20: orient = (1 - c.s.rotation) & 3; break;
        case 0x40: orient = (-c.s.rotation - 2) & 3; break;
        case 0x80: orient = (-c.s.rotation - 1) & 3; break;
        default: break;
    }
    if (orient == 0) b += 1;
    else if (orient == 1) { b += 1; a -= 1; }
    else if (orient == 2) a -= 1;
    stampBuilding(c, a, b, 1, 4); stampBuilding(c, a + 1, b, 1, 4); stampBuilding(c, a + 1, b - 1, 1, 4); stampBuilding(c, a, b - 1, 1, 4);
}
}  // namespace

void growBuilding(SimContext& c, int a, int b, int size, int zone) {
    auto id = [&](int x, int y) { return static_cast<int>(c.w.xbld.at(x, y)); };
    switch (size) {
        case 0: stampBuilding(c, a, b, 1, 3); return;
        case 1: {
            const int alt = c.w.altm.at(a, b) & 0x1F;
            auto ok = [&](int x, int y) { return canBuildAt(c, x, y, alt, zone, 0x8C); };
            int bits = 0;
            if (ok(a + 1, b)) bits |= 1;
            if (ok(a, b + 1)) bits |= 4;
            if (ok(a + 1, b + 1)) bits |= 2;
            if (bits == 7) { stampBuilding(c, a, b + 1, 2, 3); return; }
            if (ok(a, b - 1)) bits |= 0x10;
            if (ok(a + 1, b - 1)) bits |= 8;
            if ((bits & 0x19) == 0x19) { stampBuilding(c, a, b, 2, 3); return; }
            if (ok(a - 1, b)) bits |= 0x40;
            if (ok(a - 1, b + 1)) bits |= 0x20;
            if ((bits & 100) == 100) { stampBuilding(c, a - 1, b + 1, 2, 3); return; }
            if (ok(a - 1, b - 1)) bits |= 0x80;
            if ((bits & 0xD0) == 0xD0) { stampBuilding(c, a - 1, b, 2, 3); return; }
            return;
        }
        case 2: stampBuilding(c, a, b, 3, 3); return;
        case 3: {
            const int alt = c.w.altm.at(a, b) & 0x1F;
            auto ok = [&](int x, int y) { return canBuildAt(c, x, y, alt, zone, 0xAE); };
            for (int k = 0; k < 4; ++k) {
                const int u = a - (k & 1), s = b + k / 2;
                if (!(ok(u, s) && ok(u, s - 1) && ok(u, s - 2) && ok(u + 1, s - 2) && ok(u + 2, s - 2) && ok(u + 2, s - 1) && ok(u + 2, s) && ok(u + 1, s))) continue;
                // the block must touch a road junction piece ('#','\'','(','+' etc.) at one of four probe tiles
                auto in = [&](int v, std::initializer_list<int> set) { for (int x : set) if (v == x) return true; return false; };
                bool near = false;
                if (u >= 1 && s <= 0x7E && in(id(u - 1, s + 1), {0x23, 0x27, 0x28, 0x2B})) near = true;
                if (!near && u > 0 && s > 2 && in(id(u - 1, s - 3), {0x24, 0x28, 0x29, 0x2B})) near = true;
                if (!near && u < 0x7D && s > 2 && in(id(u + 3, s - 3), {0x25, 0x29, 0x2A, 0x2B})) near = true;
                if (!near && u < 0x7D && s < 0x7F && in(id(u + 3, s + 1), {0x26, 0x2A, 0x27, 0x2B})) near = true;
                if (!near) continue;
                const int tiles[8][2] = {{u, s}, {u, s - 1}, {u, s - 2}, {u + 1, s - 2}, {u + 2, s - 2}, {u + 2, s - 1}, {u + 2, s}, {u + 1, s}};
                for (const auto& t : tiles) if (id(t[0], t[1]) > 0x8B) demolishSmallBuilding(c, t[0], t[1]);
                stampBuilding(c, u, s, 4, 3);
                return;
            }
            return;
        }
        default: return;
    }
}

void clearLotForArcology(SimContext& c, int a, int b) {
    World& w = c.w;
    if (!(a > 0 && b > 0 && a < 0x7F && b < 0x7F)) return;
    for (int x = a; x <= a + 1; ++x)
        for (int y = b - 1; y <= b; ++y) {
            setTile(c, x, y, 0xF7);
            if (inMap(x, y)) { w.xzon.at(x, y) = 0; w.xbit.at(x, y) |= 0xE0; }
        }
    const uint8_t* corner = &c.t.rotationCorner[static_cast<size_t>(c.s.rotation & 3) * 8];
    auto setCorner = [&](int x, int y, uint8_t bits) { w.xzon.at(x, y) = static_cast<uint8_t>((w.xzon.at(x, y) & 0xF) | bits); };
    setCorner(a, b - 1, corner[0]); setCorner(a + 1, b - 1, corner[2]); setCorner(a + 1, b, corner[4]); setCorner(a, b, corner[6]);
}

}  // namespace sc2k
