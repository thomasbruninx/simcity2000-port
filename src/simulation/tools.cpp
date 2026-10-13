// City-building tools; see tools.h and docs/simulation/tools.md.
#include "simulation/tools.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace sc2k {
namespace {

using namespace xbit;

// ---- typed views of the raw tool tables (copied from the user's SIMCITY.EXE) ----
struct Tbl {
    const GameTables& t;
    const uint8_t* at(uint32_t va) const { return va >= 0x4E75A8 ? &t.bridgeData[va - 0x4E75A8] : &t.connectData[va - 0x4E6B08]; }
    int u8(uint32_t va) const { return *at(va); }
    int i16(uint32_t va) const { int16_t v; std::memcpy(&v, at(va), 2); return v; }
    int i32(uint32_t va) const { int32_t v; std::memcpy(&v, at(va), 4); return v; }
    int dx(int d) const { return i16(0x4E6B08 + 2u * static_cast<uint32_t>(d & 3)); }      // 0,+1,0,-1
    int dy(int d) const { return i16(0x4E6B10 + 2u * static_cast<uint32_t>(d & 3)); }      // -1,0,+1,0
    int flatten(int ter) const { return i32(0x4E6B18 + 4u * static_cast<uint32_t>(ter & 15)); }   // tile must be levelled for a network
    int sloped(int ter) const { return i32(0x4E6B58 + 4u * static_cast<uint32_t>(ter & 15)); }    // sloped terrain piece
    int slopeRise(int ter) const { return i16(0x4E6B98 + 4u * static_cast<uint32_t>(ter & 15)); }
    int slopeBlock(int ter, int dir) const { return u8(0x4E6BD8 + static_cast<uint32_t>((ter & 15) * 4 + dir)); }
    int slopeVariant(int idx) const { return i16(0x4E6BC8 + 2u * static_cast<uint32_t>(idx)); }
    int slopePiece(int ter) const { return i16(0x4E6C50 + 2u * static_cast<uint32_t>(ter)); }
    int connectA(int ter) const { return u8(0x4E6CA8 + static_cast<uint32_t>(ter & 255)); }   // may join along a
    int connectB(int ter) const { return u8(0x4E6C60 + static_cast<uint32_t>(ter & 255)); }   // may join along b
    int pieceOf(int mask) const { return i16(0x4E6C30 + 2u * static_cast<uint32_t>(mask)); }
    int bridgeCost(int type) const { return i32(0x4E75A8 + 4u * static_cast<uint32_t>(type)); }
    int bdx(int d) const { return i32(0x4E75C8 + 4u * static_cast<uint32_t>(d & 3)); }
    int bdy(int d) const { return i32(0x4E75D8 + 4u * static_cast<uint32_t>(d & 3)); }
};

struct Ctx {
    SimContext& c;
    Tbl tb;
    explicit Ctx(SimContext& cc) : c(cc), tb{cc.t} {}
    World& w() { return c.w; }
    int bld(int a, int b) { return inMap(a, b) ? c.w.xbld.at(a, b) : 0; }
    int ter(int a, int b) { return inMap(a, b) ? c.w.xter.at(a, b) : 0; }
    int und(int a, int b) { return inMap(a, b) ? c.w.xund.at(a, b) : 0; }
    int bit(int a, int b) { return inMap(a, b) ? c.w.xbit.at(a, b) : 0; }
    int zon(int a, int b) { return inMap(a, b) ? c.w.xzon.at(a, b) : 0; }
    int alt(int a, int b) { return inMap(a, b) ? (c.w.altm.at(a, b) & 0x1F) : 0; }
    int depth(int a, int b) { return inMap(a, b) ? ((c.w.altm.at(a, b) & 0x7C00) >> 10) : 0; }
};

bool roadish(int n) { return (n > 0x1C && n < 0x2C) || (n > 0x3E && n < 0x47) || n == 0x4B || n == 0x4C || (n > 0x5C && n < 0x61); }
bool railish(int n) { return (n > 0x2B && n < 0x3F) || (n > 0x44 && n < 0x49) || (n > 0x6B && n < 0x70) || n == 0x4D || n == 0x4E; }
bool pipeish(int u) { return u > 0xF && u < 0x1F; }
bool pipeish2(int u) { return (u > 0xF && u < 0x1F) || u == 0x1F || u == 0x20; }
bool subwayish(int u) { return (u != 0 && u < 0x10) || u == 0x23 || u == 0x1F || u == 0x20 || u == 0x22; }

// terrain value used to index the slope tables: values above 0x2F (water) count as flat
int slopeIndex(int ter, int limit) { return ter <= limit ? ter : 0; }

// neighbour piece mask shared by the four fixers: a-1 -> 8, a+1 -> 2, b-1 -> 1, b+1 -> 4
template <class Join>
int neighbourMask(Ctx& x, int a, int b, Join join) {
    int m = 0;
    if (a > 0 && join(a - 1, b) && x.tb.connectA(x.ter(a - 1, b)) != 0) m = 8;
    if (a < 0x7F && join(a + 1, b) && x.tb.connectA(x.ter(a + 1, b)) != 0) m += 2;
    if (b > 0 && join(a, b - 1) && x.tb.connectB(x.ter(a, b - 1)) != 0) m += 1;
    if (b < 0x7F && join(a, b + 1) && x.tb.connectB(x.ter(a, b + 1)) != 0) m += 4;
    return m;
}

int edgeBits(Ctx& x, int a, int b) {   // border-connection marker (XTXT 0xFA) adds a stub towards the map edge
    int m = 0;
    if (x.c.w.xtxt.at(a, b) == 0xFA) {
        if (a == 0) m += 8;
        if (a == 0x7F) m += 2;
        if (b == 0) m += 1;
        if (b == 0x7F) m += 4;
    }
    return m;
}

// ---- piece fixers: recompute the orientation of one network tile from its neighbours ----
void fixPower(Ctx& x, int a, int b) {   // 0x44A400
    const int id = x.bld(a, b);
    if (id < 0xE || id > 0x1C) return;
    const int ter = slopeIndex(x.ter(a, b), 0x2F);
    int piece;
    if (x.tb.sloped(ter) != 0) piece = x.tb.slopePiece(ter);
    else {
        auto join = [&](int na, int nb) { return (x.bit(na, nb) & kPowerable) != 0; };
        piece = x.tb.pieceOf(neighbourMask(x, a, b, join));
    }
    setTile(x.c, a, b, piece + 0xE);
}

void fixRoad(Ctx& x, int a, int b) {   // 0x44A850
    const int id = x.bld(a, b);
    if (!(id > 0x1C && id < 0x2C)) return;
    const int ter = slopeIndex(x.ter(a, b), 0x2F);
    int piece;
    if (x.tb.sloped(ter) != 0) piece = x.tb.slopePiece(ter);
    else {
        auto join = [&](int na, int nb) { return roadish(x.bld(na, nb)); };
        piece = x.tb.pieceOf(neighbourMask(x, a, b, join) + edgeBits(x, a, b));
    }
    setTile(x.c, a, b, piece + 0x1D);
}

void fixRail(Ctx& x, int a, int b) {   // 0x44AD20
    const int id = x.bld(a, b);
    if (!(id > 0x2B && id < 0x3B)) return;
    const int ter = slopeIndex(x.ter(a, b), 0x2F);
    if (x.tb.sloped(ter) != 0) { setTile(x.c, a, b, x.tb.slopePiece(ter) + 0x2C); return; }
    int m = 0;
    auto join = [](int n) { return railish(n); };
    if (a > 0) {
        const int n = x.bld(a - 1, b);
        if (n == 0x2E) { setTile(x.c, a, b, 0x3B); return; }
        if (join(n) && x.tb.connectA(x.ter(a - 1, b)) != 0) m = 8;
    }
    if (a < 0x7F) {
        const int n = x.bld(a + 1, b);
        if (n == 0x30) { setTile(x.c, a, b, 0x3D); return; }
        if (join(n) && x.tb.connectA(x.ter(a + 1, b)) != 0) m += 2;
    }
    if (b > 0) {
        const int n = x.bld(a, b - 1);
        if (n == 0x2F) { setTile(x.c, a, b, 0x3C); return; }
        if (join(n) && x.tb.connectB(x.ter(a, b - 1)) != 0) m += 1;
    }
    if (b < 0x7F) {
        const int n = x.bld(a, b + 1);
        if (n == 0x31) { setTile(x.c, a, b, 0x3E); return; }
        if (join(n) && x.tb.connectB(x.ter(a, b + 1)) != 0) m += 4;
    }
    m += edgeBits(x, a, b);
    setTile(x.c, a, b, x.tb.pieceOf(m) + 0x2C);
}

void fixPipe(Ctx& x, int a, int b) {   // 0x44B250
    const int u = x.und(a, b);
    if (!(u > 0xF && u < 0x1F)) return;
    const int ter = x.ter(a, b) < 0x31 ? (x.ter(a, b) & 0xF) : 0;
    if (x.tb.sloped(ter) != 0) { setUnderground(x.c, a, b, x.tb.slopePiece(ter) + 0x10); return; }
    auto join = [&](int na, int nb) { return pipeish2(x.und(na, nb)); };
    int m = neighbourMask(x, a, b, join);
    if (m == 0) m = 0xF;
    setUnderground(x.c, a, b, x.tb.pieceOf(m) + 0x10);
}

void fixSubway(Ctx& x, int a, int b) {   // 0x44B5C0
    const int u = x.und(a, b);
    if (!(u != 0 && u < 0x10)) return;
    const int ter = x.ter(a, b) < 0x31 ? (x.ter(a, b) & 0xF) : 0;
    int piece;
    if (x.tb.sloped(ter) == 0) {
        auto join = [&](int na, int nb) { return subwayish(x.und(na, nb)); };
        piece = x.tb.pieceOf(neighbourMask(x, a, b, join));
    } else piece = x.tb.slopePiece(ter);
    setUnderground(x.c, a, b, piece + 1);
}

template <class F>
void fixSelfAndNeighbours(int a, int b, F fix) {
    fix(a, b);
    if (a > 0) fix(a - 1, b);
    if (a < 0x7F) fix(a + 1, b);
    if (b > 0) fix(a, b - 1);
    if (b < 0x7F) fix(a, b + 1);
}

void fixSurfaceNeighbours(Ctx& x, int a, int b) {   // 0x44C120: after a tile changed, refit road / rail / power pieces around it
    auto one = [&](int na, int nb) {
        const int id = x.bld(na, nb);
        if (id > 0x1C && id < 0x2C) fixRoad(x, na, nb);
        else if (id > 0x2B && id < 0x3B) fixRail(x, na, nb);
        if (id > 0xD && id < 0x1D) fixPower(x, na, nb);
    };
    if (a > 0) one(a - 1, b);
    if (a < 0x7F) one(a + 1, b);
    if (b > 0) one(a, b - 1);
    if (b < 0x7F) one(a, b + 1);
}

void fixUndergroundNeighbours(Ctx& x, int a, int b) {   // 0x44C330
    auto one = [&](int na, int nb) {
        const int u = x.und(na, nb);
        if (u != 0 && u < 0x10) fixSubway(x, na, nb);
        else if (u > 0xF && u < 0x1F) fixPipe(x, na, nb);
    };
    if (a > 0) one(a - 1, b);
    if (a < 0x7F) one(a + 1, b);
    if (b > 0) one(a, b - 1);
    if (b < 0x7F) one(a, b + 1);
}

// ---- piece appliers ----
}  // namespace

void applyRoadPiece(SimContext& c, int a, int b) {   // 0x44A640
    Ctx x(c);
    const int id = x.bld(a, b);
    if (roadish(id)) return;
    int piece;
    if (id < 0xE) piece = 0x1D;
    else switch (id) {
        case 0xE: piece = 0x44; break;
        case 0xF: piece = 0x43; break;
        case 0x2C: piece = 0x46; break;
        case 0x2D: piece = 0x45; break;
        case 0x49: piece = 0x4B; break;
        case 0x4A: piece = 0x4C; break;
        default: return;
    }
    setTile(c, a, b, piece);
    if ((c.w.xzon.at(a, b) & 0xF) != 7) c.w.xzon.at(a, b) &= 0xF0;
    fixSelfAndNeighbours(a, b, [&](int na, int nb) { fixRoad(x, na, nb); });
}

void applyPowerPiece(SimContext& c, int a, int b) {   // 0x44A220
    Ctx x(c);
    if (inMap(a, b) && (c.w.xbit.at(a, b) & kPowerable)) return;
    const int id = x.bld(a, b);
    int piece;
    if (id < 0xE) piece = 0xE;
    else switch (id) {
        case 0x1D: piece = 0x43; break;
        case 0x1E: piece = 0x44; break;
        case 0x2C: piece = 0x47; break;
        case 0x2D: piece = 0x48; break;
        case 0x49: piece = 0x4F; break;
        case 0x4A: piece = 0x50; break;
        default: return;
    }
    setTile(c, a, b, piece);
    c.w.xbit.at(a, b) |= kPowerable;
    fixSelfAndNeighbours(a, b, [&](int na, int nb) { fixPower(x, na, nb); });
}

void applyRailPiece(SimContext& c, int a, int b) {   // 0x44AB20
    Ctx x(c);
    const int id = x.bld(a, b);
    if (railish(id)) return;
    int piece;
    if (id < 0xE) piece = 0x2C;
    else switch (id) {
        case 0xE: piece = 0x48; break;
        case 0xF: piece = 0x47; break;
        case 0x1D: piece = 0x45; break;
        case 0x1E: piece = 0x46; break;
        case 0x49: piece = 0x4D; break;
        case 0x4A: piece = 0x4E; break;
        default: return;
    }
    setTile(c, a, b, piece);
    c.w.xzon.at(a, b) &= 0xF0;
    fixSelfAndNeighbours(a, b, [&](int na, int nb) { fixRail(x, na, nb); });
}

void applyPipePiece(SimContext& c, int a, int b) {   // 0x44B110
    Ctx x(c);
    const int u = x.und(a, b);
    if (pipeish(u) || u == 0x1F || u == 0x20) return;
    int piece;
    if (u == 0) piece = 0x10; else if (u == 1) piece = 0x1F; else if (u == 2) piece = 0x20; else return;
    setUnderground(c, a, b, piece);
    c.w.xbit.at(a, b) |= kPiped;
    fixSelfAndNeighbours(a, b, [&](int na, int nb) { fixPipe(x, na, nb); });
}

void applySubwayPiece(SimContext& c, int a, int b) {   // 0x44B4A0
    Ctx x(c);
    const int u = x.und(a, b);
    if (!((u == 0 || u > 0xF) && u != 0x23 && u != 0x1F && u != 0x20 && u != 0x22)) return;
    int piece;
    if (u == 0) piece = 1; else if (u == 0x10) piece = 0x20; else if (u == 0x11) piece = 0x1F; else return;
    setUnderground(c, a, b, piece);
    fixSelfAndNeighbours(a, b, [&](int na, int nb) { fixSubway(x, na, nb); });
}

// ---- bulldozer ----
bool bulldozeTile(SimContext& c, ToolState& ts, int a, int b) {   // 0x442C20
    Ctx x(c);
    World& w = c.w;
    if (!inMap(a, b)) return false;
    if (w.xtxt.at(a, b) == 0xFF && !c.hooks.confirm(0xFF)) return false;
    if ((w.xzon.at(a, b) & 0xF) == 7) return false;
    const int id = w.xbld.at(a, b);
    const int wd = x.depth(a, b);
    if (!ts.underground) {
        if (id == 5) return false;
        if (id == 0 && w.xter.at(a, b) < 0x30) return false;
    } else if (w.xund.at(a, b) == 0 && wd != 1 && !(w.xbit.at(a, b) & kPiped)) return false;
    const int cost = c.t.toolCost[0];   // bulldozer subtype 0
    if (c.s.treasury < cost) return false;
    c.s.treasury -= cost;
    if (ts.underground) {
        if (id < 0x70) w.xbit.at(a, b) &= static_cast<uint8_t>(~kPiped);
        if (wd == 1 || w.xund.at(a, b) == 0x23) destroyTile(c, a, b, 1);
        setUnderground(c, a, b, 0);
    } else {
        bool destroyed = false;
        if (id == 0xE9 || (id > 0x6B && id < 0x71)) setUnderground(c, a, b, 0);
        if (id < 0xD) {
            if (id > 5 && (static_cast<unsigned>(c.rng.next()) & 0xFFFF) % 0x14 == 0) {   // 1 in 20 trees: the environmentalists object
                c.hooks.postMessage(0x28, 0);
                return false;
            }
            setTile(c, a, b, 0);
        } else {
            destroyTile(c, a, b, 1);
            destroyed = true;
        }
        const int terr = w.xter.at(a, b);
        if (terr >= 0x30) {
            const bool water = (w.xbit.at(a, b) & kWater) != 0;
            if (terr == 0x3E) fixShore(c, a, b); else w.xter.at(a, b) = 0;
            if (destroyed && water) floodTile(c, a, b);
            else w.xbit.at(a, b) &= static_cast<uint8_t>(~kWater);
            auto wet = [&](int na, int nb) { if (inMap(na, nb) && (w.xbit.at(na, nb) & kWater)) floodTile(c, na, nb); };
            wet(a - 1, b); wet(a, b + 1); wet(a + 1, b); wet(a, b - 1);
            wet(a - 1, b - 1); wet(a + 1, b - 1); wet(a + 1, b + 1); wet(a - 1, b + 1);
        }
    }
    fixSurfaceNeighbours(x, a, b);
    fixUndergroundNeighbours(x, a, b);
    return true;
}

// ---- zoning ----
bool paintZone(SimContext& c, ToolState& ts, int a0, int b0, int a1, int b1, int zoneType, int unitCost) {   // 0x448F70
    World& w = c.w;
    ts.kind = 5;
    const int lo_a = std::max(0, std::min(a0, a1)), hi_a = std::min(0x7F, std::max(a0, a1));
    const int lo_b = std::max(0, std::min(b0, b1)), hi_b = std::min(0x7F, std::max(b0, b1));
    if (inMap(a0, b0) && (w.xbit.at(a0, b0) & kWater)) return false;
    int count = 0;   // 0x447D00: tiles of the rectangle that would change zone
    for (int a = lo_a; a <= hi_a; ++a)
        for (int b = lo_b; b <= hi_b; ++b) {
            if (w.xter.at(a, b) != 0) continue;
            const int id = w.xbld.at(a, b), z = w.xzon.at(a, b) & 0xF;
            if (id < 0x1D && id != 5 && id != 0xD && z != 7 && z != zoneType) ++count;
        }
    const int cost = unitCost * count;
    ts.cost = cost;
    if (cost == 0 || cost > c.s.treasury) return false;
    c.s.treasury -= cost;
    for (int a = lo_a; a <= hi_a; ++a)
        for (int b = lo_b; b <= hi_b; ++b) {
            if (w.xbit.at(a, b) & kWater) continue;
            const int id = w.xbld.at(a, b);
            if (id < 0x70 && w.xter.at(a, b) == 0 && id < 0x1D && id != 5 && id != 0xD && (w.xzon.at(a, b) & 0xF) != 7) {
                if (zoneType == 0 && id != 0 && id < 5) setTile(c, a, b, 0);
                w.xzon.at(a, b) = static_cast<uint8_t>(((zoneType ^ w.xzon.at(a, b)) & 0xF) ^ w.xzon.at(a, b));
            }
        }
    return true;
}

// ---- path planner (0x448540 / 0x4488D0 / 0x4487A0) ----
namespace {

int dirTowards(int a, int b, int ta, int tb) {   // 0x44C000: dominant axis
    const int dx = std::abs(a - ta), dy = std::abs(b - tb);
    if (dy < dx) return a <= ta ? 1 : 3;
    return tb < b ? 0 : 2;
}

int dirAlternative(int a, int b, int ta, int tb) {   // 0x44C070: the other axis when it still makes progress
    const int dx = std::abs(a - ta), dy = std::abs(b - tb);
    if (dy < dx) {
        if (dy > 0) return tb < b ? 0 : 2;
        return a <= ta ? 1 : 3;
    }
    if (dx < 1) return tb < b ? 0 : 2;
    return a <= ta ? 1 : 3;
}

struct Planner {
    Ctx& x;
    ToolState& ts;

    bool begin(int a, int b, int ta, int tb) {   // 0x448540
        const int id = x.bld(a, b);
        if (ts.kind >= 0 && ts.kind < 3) {
            if (id > 0x50 || (x.ter(a, b) > 0xF && x.ter(a, b) < 0x20)) return false;
            if (id == 0xD) return false;
        }
        if (ts.kind != 4 && ts.kind != 3 && id == 5) return false;
        if (ts.kind != 6 && (x.zon(a, b) & 0xF) == 7) return false;
        ts.targetA = ta; ts.targetB = tb;
        ts.dir = dirTowards(a, b, ta, tb);
        const int ter = x.ter(a, b);
        if (ter != 0 && ter < 5) {
            if (x.tb.slopeBlock(ter, ts.dir) != 0) {
                ts.dir = dirAlternative(a, b, ts.targetA, ts.targetB);
                if (x.tb.slopeBlock(ter, ts.dir) != 0) { ts.targetA = a; ts.targetB = b; }
            }
        }
        ts.minA = std::min(ta, a); ts.maxA = std::max(ta, a);
        ts.minB = std::min(tb, b); ts.maxB = std::max(tb, b);
        return true;
    }

    // Advance (a, b) one tile along the route; false at the end or when blocked.
    bool next(int& pa, int& pb) {   // 0x4488D0
        const int a = pa, b = pb;
        if (ts.targetA == a && ts.targetB == b) return false;
        const Tbl& tb = x.tb;
        bool special = false;
        if (ts.kind < 3) {
            const int id = x.bld(a, b);
            if (id > 0xD) { const int s = id + (ts.dir & 1); special = (s == 0x1E || s == 0x2D || s == 0xF || s == 0x4A); }
        } else {
            const int u = x.und(a, b);
            if (u != 0) { const int s = u + (ts.dir & 1); special = (s == 0x11 || s == 2); }
        }
        int na, nb, d;
        if (special || tb.sloped(x.ter(a, b)) != 0) {
            d = ts.dir;
            na = tb.dx(d) + a; nb = tb.dy(d) + b;
            if (na < ts.minA || nb < ts.minB || ts.maxA < na || ts.maxB < nb) return false;
            const int t = x.ter(na, nb);
            if (ts.kind == 1 && t != 0 && x.alt(na, nb) != x.alt(a, b)) return false;
            if (t < 0x40 && tb.slopeBlock(t, d) != 0) return false;
        } else {
            const int h = x.alt(a, b) + tb.slopeRise(x.ter(a, b));
            d = dirTowards(a, b, ts.targetA, ts.targetB);
            na = tb.dx(d) + a; nb = tb.dy(d) + b;
            if (!inMap(na, nb)) return false;   // the original reads past the map here (undefined); stepping off the map is never valid
            int t = x.ter(na, nb);
            bool useAlt = false;
            if (t < 0x40 && tb.slopeBlock(t, d) != 0) useAlt = true;
            else {
                if (tb.slopeRise(t) != 0) {
                    const int h2 = x.alt(na, nb);
                    if (h2 == h || h2 - h == -2) useAlt = true;
                }
                if (!useAlt && t == 0 && x.alt(na, nb) - h == -1) useAlt = true;
            }
            if (useAlt) {
                d = dirAlternative(a, b, ts.targetA, ts.targetB);
                na = tb.dx(d) + a; nb = tb.dy(d) + b;
                if (!inMap(na, nb)) return false;
                t = x.ter(na, nb);
                if (t < 0x40 && tb.slopeBlock(t, d) != 0) return false;
                if (tb.slopeRise(t) != 0) {
                    const int h2 = x.alt(na, nb);
                    if (h2 == h || h2 - h == -2) return false;
                }
                if (t == 0 && x.alt(na, nb) - h == -1) return false;
            }
            if (ts.kind == 1 && ts.dir != d && t != 0) return false;
        }
        ts.dir = d;
        if (ts.kind != 6 && (x.zon(na, nb) & 0xF) == 7) return false;
        if (ts.kind < 3) {
            if (inMap(na, nb) && (x.bit(na, nb) & kWater) && x.ter(na, nb) < 0x40) return false;
            const int id = x.bld(na, nb);
            if (id == 5 || id == 0xD) return false;
            if (id > 0xC) { const int s = id + (ts.dir & 1); if (s != 0x1E && s != 0x2D && s != 0xF && s != 0x4A) return false; }
        } else {
            const int wd = x.depth(na, nb);
            if (wd == 1 || wd == 2) return false;
            const int u = x.und(na, nb);
            if (u != 0) { const int s = u + (ts.dir & 1); if (s != 0x11 && s != 2) return false; }
        }
        pa = na; pb = nb;
        return true;
    }

    void prepare(int a, int b) {   // 0x4487A0: reshape the terrain piece under the route
        const int ter = x.ter(a, b);
        if (ter >= 0x30) return;
        const int i = ter & 15;
        if (x.tb.flatten(i) == 0) return;
        uint8_t& t = x.c.w.xter.at(a, b);
        if (x.tb.sloped(i) == 0) {
            if (ter < 0x10) t = 0x0D; else { t = 0x1D; if (ter > 0x1F) t = 0x0D; }
            if (x.c.w.xbit.at(a, b) & kWater) x.c.w.xbit.at(a, b) &= static_cast<uint8_t>(~kWater);
        } else {
            const int idx = ts.dir + i * 4;
            if (ter >= 0x10) t = static_cast<uint8_t>(x.tb.slopeVariant(idx) + 0x10);
            else t = static_cast<uint8_t>(x.tb.slopeVariant(idx) & 0xFF);
        }
    }
};

}  // namespace

PathPreview previewNetwork(SimContext& c, ToolState& ts, NetworkKind kind, int a0, int b0, int a1, int b1, int unitCost) {   // 0x448410
    Ctx x(c);
    Planner p{x, ts};
    PathPreview out;
    ts.kind = static_cast<int>(kind);
    if (!inMap(a0, b0) || !inMap(a1, b1)) return out;
    if (!p.begin(a0, b0, a1, b1)) return out;
    int a = a0, b = b0;
    ts.slopeTiles = 0; ts.length = 0;
    do {
        ++ts.length;
        if (x.ter(a, b) < 0x30 && x.tb.flatten(x.ter(a, b)) != 0) ++ts.slopeTiles;
        out.tiles.emplace_back(a, b);
    } while (p.next(a, b));
    ts.cost = ts.length * unitCost + ts.slopeTiles * 25;
    out.valid = true; out.cost = ts.cost; out.length = ts.length; out.slopeTiles = ts.slopeTiles;
    ts.dir = 0;
    return out;
}

// ---- bridges ----
bool buildBridge(SimContext& c, ToolState& ts, int a, int b, int mask, int flag) {   // 0x45B280
    Ctx x(c);
    World& w = c.w;
    constexpr int kAllowed[16] = {0, 2, 4, 8, 1, 6, 0xC, 9, 3, 0, 0, 0, 0, 0, 0, 0};
    const int terr = x.ter(a, b);
    if (terr < 0x20 || terr > 0x3F) return false;
    const int dirs = kAllowed[terr & 15];
    if (dirs == 0) return false;
    int dir;
    if (flag == 0) dir = ts.dir;
    else {
        const int r = c.s.rotation & 3;
        if (dirs & (1 << r)) dir = r;
        else if (dirs & (1 << ((r + 2) & 3))) dir = (r + 2) & 3;
        else if (dirs & (1 << ((r + 1) & 3))) dir = (r + 1) & 3;
        else if (dirs & (1 << ((r - 1) & 3))) dir = (r - 1) & 3;
        else return false;
    }
    int len = 0, sa = a, sb = b;
    do {
        if (len != 0 && x.bld(sa, sb) != 0) { if (flag) c.hooks.toolMessage(0x101); return false; }
        sa += x.tb.bdx(dir); sb += x.tb.bdy(dir);
        if (!inMap(sa, sb)) { if (flag) c.hooks.toolMessage(0x100); return false; }
        ++len;
    } while (x.ter(sa, sb) > 0xF && x.ter(sa, sb) < 0x40);
    int types = 7;
    if (len > 4 && len < 0xC) types = 0xF;
    if (len > 6) types |= 0x10;
    const int type = c.hooks.chooseBridge(len, types & mask);
    if (type < 0 || type > 4) return false;
    const int cost = x.tb.bridgeCost(type) * len;
    if (c.s.treasury < cost) { c.hooks.toolMessage(0x102); return false; }
    c.s.treasury -= cost;

    auto raise = [&](int ra, int rb) {
        if (w.xter.at(ra, rb) < 0x30) w.altm.at(ra, rb) = static_cast<uint16_t>((w.altm.at(ra, rb) & 0xFFE0) | ((w.altm.at(ra, rb) & 0x1F) + 1));
    };
    auto apply = [&](int ra, int rb) {
        switch (type) {
            case 0: applyPowerPiece(c, ra, rb); break;
            case 1: applyRailPiece(c, ra, rb); break;
            default: applyRoadPiece(c, ra, rb); break;
        }
    };
    raise(a, b);
    w.xter.at(a, b) = static_cast<uint8_t>(((dir + 1) & 3) + 1);
    w.xbit.at(a, b) &= static_cast<uint8_t>(~kWater);
    apply(a, b);

    int pa = a, pb = b;
    for (int i = 1; i < len - 1; ++i) {
        pa += x.tb.bdx(dir); pb += x.tb.bdy(dir);
        if (dir & 1) w.xbit.at(pa, pb) |= kFlipped;
        int id = -1;
        switch (type) {
            case 0:
                setTile(c, pa, pb, 0x5C);
                w.xbit.at(pa, pb) |= kPowerable;
                break;
            case 1: {
                const int half = len / 2;
                id = (i != half && half - 2 <= i && i <= half + 2) ? 0x5B : 0x5A;
                break;
            }
            case 2: id = 0x57; break;
            case 3: {
                const int q = (len + 1) / 4;
                if (i < q) id = 0x57;
                else if (i == q) id = 0x56;
                else if (i < len - q - 1) id = 0x58;
                else if (len - q - i == 1) id = 0x56;
                else id = 0x57;
                break;
            }
            case 4: {
                const int s5 = (len - 2) % 5 + 2;
                const int q = s5 / 2;
                if (i < q || (-1 - s5) / 2 + len <= i) id = 0x57;
                else if (dir == 0 || dir == 3) id = (i - q) % 5 + 0x51;
                else id = 0x55 - (i - q) % 5;
                break;
            }
            default: break;
        }
        if (id >= 0) setTile(c, pa, pb, id);
    }
    const int ea = pa + x.tb.bdx(dir), eb = pb + x.tb.bdy(dir);
    raise(ea, eb);
    w.xter.at(ea, eb) = static_cast<uint8_t>(((dir - 1) & 3) + 1);
    w.xbit.at(ea, eb) &= static_cast<uint8_t>(~kWater);
    apply(ea, eb);
    return true;
}

// ---- network drags ----
bool buildNetwork(SimContext& c, ToolState& ts, NetworkKind kind, int sub, int a0, int b0, int a1, int b1) {
    Ctx x(c);
    World& w = c.w;
    if (!inMap(a0, b0)) return false;
    static constexpr int kCategory[5] = {toolcat::kRoads, toolcat::kRail, toolcat::kPower, toolcat::kRail, toolcat::kWater};
    const int category = kCategory[static_cast<int>(kind)];
    const int unit = c.t.toolCost[static_cast<size_t>(category * 12 + sub)];
    if (unit < 0) return false;
    const int bridgeMask = kind == NetworkKind::Road ? 0x1C : kind == NetworkKind::Rail ? 2 : 1;
    if (kind != NetworkKind::Subway && kind != NetworkKind::Pipe) {
        if ((w.xbit.at(a0, b0) & kWater) && w.xter.at(a0, b0) < 0x40) {   // a click on the water edge starts a bridge
            ts.dir = c.s.rotation;
            return buildBridge(c, ts, a0, b0, bridgeMask, 1);
        }
    }
    const PathPreview pv = previewNetwork(c, ts, kind, a0, b0, a1, b1, unit);
    if (!pv.valid || pv.cost == 0 || c.s.treasury < pv.cost) return false;
    c.s.treasury -= pv.cost;

    Planner p{x, ts};
    ts.kind = static_cast<int>(kind);
    p.begin(a0, b0, a1, b1);
    auto apply = [&](int a, int b) {
        p.prepare(a, b);
        switch (kind) {
            case NetworkKind::Road: applyRoadPiece(c, a, b); break;
            case NetworkKind::Rail: applyRailPiece(c, a, b); break;
            case NetworkKind::Power: applyPowerPiece(c, a, b); break;
            case NetworkKind::Subway: applySubwayPiece(c, a, b); break;
            case NetworkKind::Pipe: applyPipePiece(c, a, b); break;
        }
    };
    int a = a0, b = b0, ea = a0, eb = b0;
    while (p.next(ea, eb)) { apply(a, b); a = ea; b = eb; }
    apply(a, b);
    if (kind == NetworkKind::Subway || kind == NetworkKind::Pipe) { ts.dir = 0; return true; }

    const int sa = a + x.tb.dx(ts.dir), sb = b + x.tb.dy(ts.dir);
    if (inMap(sa, sb) && (w.xbit.at(sa, sb) & kWater) && w.xter.at(sa, sb) < 0x40)
        buildBridge(c, ts, sa, sb, bridgeMask, 0);
    if (kind != NetworkKind::Power) {
        const bool done = inMap(sa, sb) && ((a0 != ea || b0 != eb) || (ea != 0 && eb != 0 && ea != 0x7F && eb != 0x7F));
        const int fee = kind == NetworkKind::Road ? 1000 : 0x5DC;
        if (!done && w.xtxt.at(ea, eb) != 0xFA && c.s.treasury >= fee && c.hooks.confirm(kind == NetworkKind::Road ? 0x6D : 0x6E)) {
            c.s.treasury -= fee;
            w.xtxt.at(ea, eb) = 0xFA;
            if (kind == NetworkKind::Road) { fixRoad(x, ea, eb); ++c.s.cb4f0; }
            else { fixRail(x, ea, eb); ++c.s.c9408; }
        }
    }
    ts.dir = 0;
    return true;
}

// ---- single buildings (0x43FB20) ----
namespace {
// id per [category * 12 + subtype], copied from the stack initialiser of 0x43FB20 (0 = no building on that button)
const std::array<uint8_t, 216>& buildingIds() {
    static const std::array<uint8_t, 216> ids = [] {
        std::array<uint8_t, 216> t{};
        const int e[][2] = {{0x26, 0xCF}, {0x28, 0xCA}, {0x29, 0xC9}, {0x2A, 0xCB}, {0x2B, 0xC8}, {0x2C, 0xCC}, {0x2D, 0xCD}, {0x2E, 0xCE},
                            {0x31, 0xDC}, {0x32, 0xEB}, {0x33, 0xF4}, {0x34, 0xFA}, {0x3C, 0xF3}, {0x3D, 0xD0}, {0x3E, 0xDB}, {0x3F, 0xFF},
                            {0x41, 0xFB}, {0x42, 0xFC}, {0x43, 0xFD}, {0x44, 0xFE}, {0x4C, 0xEC}, {0x56, 0xED}, {0x57, 0xE9}, {0x90, 0xD6},
                            {0x91, 0xD9}, {0x92, 0xF5}, {0x93, 0xD4}, {0x9C, 0xD2}, {0x9D, 0xD3}, {0x9E, 0xD1}, {0x9F, 0xD8}, {0xA8, 0x0D},
                            {0xA9, 0xD5}, {0xAA, 0xDA}, {0xAB, 0xD7}, {0xAC, 0xF8}};
        for (const auto& p : e) t[static_cast<size_t>(p[0])] = static_cast<uint8_t>(p[1]);
        return t;
    }();
    return ids;
}
}  // namespace

int toolBuildingId(int category, int sub) {
    const int i = category * 12 + sub;
    return i >= 0 && i < 216 ? buildingIds()[static_cast<size_t>(i)] : 0;
}

int toolCost(const SimContext& c, int category, int sub) {
    const int i = category * 12 + sub;
    return i >= 0 && i < 216 ? c.t.toolCost[static_cast<size_t>(i)] : -1;
}

bool placeBuilding(SimContext& c, ToolState&, int category, int sub, int a, int b) {
    Ctx x(c);
    World& w = c.w;
    const int index = category * 12 + sub;
    if (index < 0 || index >= 216) return false;
    const int cost = c.t.toolCost[static_cast<size_t>(index)];
    if (c.s.treasury < cost && cost != 0) return false;
    const int id = buildingIds()[static_cast<size_t>(index)];
    if (id == 0) return false;
    const int size = c.t.toolBuildingSize[static_cast<size_t>(index)];
    if (size >= 2) {
        const bool ok = size == 2 ? (a > 0 && b > 0 && a + 3 < 0x81 && b + 3 < 0x81) : (a > 1 && b > 1 && a + size < 0x81 && b + size < 0x81);
        if (!ok) { c.hooks.showDialog(0x69); return false; }
    }
    if (id == 0xF4 || id == 0xCF || id == 0xCA || id == 0xCB || id == 0xC9 || id == 0xD8) {   // plants, treatment works and prisons: neighbours object
        int zoned = 0;
        for (int xa = a - 8; xa < a + 8 + size; ++xa)
            for (int xb = b - 8; xb < b + 8 + size; ++xb) {
                if (!inMap(xa, xb)) continue;
                const int z = w.xzon.at(xa, xb) & 0xF;
                if (z == 1 || z == 2) ++zoned;
            }
        if (static_cast<int16_t>(c.lfsr.mod(200)) < zoned) {
            c.hooks.postMessage(0x200, 0);
            return false;
        }
    }
    if (!placeSpecialBuilding(c, a, b, id, size)) return false;
    c.s.treasury -= cost;
    switch (id) {
        case 0xD1: ++c.s.budget[kHealth].currentCosts; break;   // another unit to maintain in the budget
        case 0xD2: ++c.s.budget[kPolice].currentCosts; break;
        case 0xD3: ++c.s.budget[kFire].currentCosts; break;
        case 0xD6: ++c.s.budget[kSchool].currentCosts; break;
        case 0xD9: ++c.s.budget[kCollege].currentCosts; break;
        case 0xDB: w.xbit.at(a, b) &= 0x7F; break;
        case 0xDC: applyPipePiece(c, a, b); break;
        case 0xE9:
            applySubwayPiece(c, a, b);
            setUnderground(c, a, b, 0x23);
            w.xbit.at(a, b) &= static_cast<uint8_t>(~kPiped);
            break;
        default: break;
    }
    if ((w.xbit.at(a, b) & kPowerable) && c.s.population10 < 50000) refreshPower(c);
    if ((w.xbit.at(a, b) & kPiped) && c.s.population10 < 50000) refreshWater(c);
    return true;
}

}  // namespace sc2k
