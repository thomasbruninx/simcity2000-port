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
    const uint8_t* at(uint32_t va) const { return va >= 0x4E75E8 ? &t.highwayData[va - 0x4E75E8] : va >= 0x4E75A8 ? &t.bridgeData[va - 0x4E75A8] : &t.connectData[va - 0x4E6B08]; }
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
                c.hooks.toolMessage(0xEC);   // "Citizens are protesting your ..."
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
int zoneRectCost(SimContext& c, int a0, int b0, int a1, int b1, int zoneType, int unitCost) {   // 0x447D00
    World& w = c.w;
    int count = 0;
    for (int a = std::max(0, std::min(a0, a1)); a <= std::min(0x7F, std::max(a0, a1)); ++a)
        for (int b = std::max(0, std::min(b0, b1)); b <= std::min(0x7F, std::max(b0, b1)); ++b) {
            if (w.xter.at(a, b) != 0) continue;
            const int id = w.xbld.at(a, b), z = w.xzon.at(a, b) & 0xF;
            if (id < 0x1D && id != 5 && id != 0xD && z != 7 && z != zoneType) ++count;
        }
    return unitCost * count;
}

bool paintZone(SimContext& c, ToolState& ts, int a0, int b0, int a1, int b1, int zoneType, int unitCost) {   // 0x448F70
    World& w = c.w;
    ts.kind = 5;
    const int lo_a = std::max(0, std::min(a0, a1)), hi_a = std::min(0x7F, std::max(a0, a1));
    const int lo_b = std::max(0, std::min(b0, b1)), hi_b = std::min(0x7F, std::max(b0, b1));
    if (inMap(a0, b0) && (w.xbit.at(a0, b0) & kWater)) return false;
    const int cost = zoneRectCost(c, a0, b0, a1, b1, zoneType, unitCost);
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
            c.hooks.toolMessage(0x6A);   // "Your citizens urge you to ..."
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

// ---- terrain tools ----
namespace {

struct Terr {
    Ctx& x;
    ToolState& ts;
    int dx8(int i) const { int16_t v; std::memcpy(&v, &x.c.t.terrainData[static_cast<size_t>(2 * i)], 2); return v; }        // 0x4E6848
    int dy8(int i) const { int16_t v; std::memcpy(&v, &x.c.t.terrainData[static_cast<size_t>(0x18 + 2 * i)], 2); return v; }  // 0x4E6860
    static int maskTable(int i) { static const int m[8] = {3, 2, 6, 4, 12, 8, 9, 1}; return m[i]; }   // 0x4E6888
    int a4x(int i) const { static const int v[4] = {0, 1, 0, -1}; return v[i]; }     // 0x4E6908
    int a4y(int i) const { static const int v[4] = {-1, 0, 1, 0}; return v[i]; }     // 0x4E6910
};

struct IntRect { int l = 0, t = 0, r = 0, b = 0; bool empty() const { return l >= r || t >= b; } };
IntRect unionRect(const IntRect& p, const IntRect& q) {   // USER32 UnionRect: empty rectangles are ignored
    if (p.empty()) return q.empty() ? IntRect{} : q;
    if (q.empty()) return p;
    return {std::min(p.l, q.l), std::min(p.t, q.t), std::max(p.r, q.r), std::max(p.b, q.b)};
}

void shoreFix9(SimContext& c, int a, int b) {   // 0x446E30
    for (int i = 0; i < 9; ++i) {
        const int dx = i < 8 ? c.t.shoreDx[static_cast<size_t>(i)] : 0, dy = i < 8 ? c.t.shoreDy[static_cast<size_t>(i)] : 0;
        fixShore(c, a + dx, b + dy);
    }
}

void adjustNeighbours(Ctx& x, int a, int b) {   // 0x445500: keep neighbouring building-free tiles within one step of this altitude
    SimContext& c = x.c;
    const int alt = x.alt(a, b);
    static const int dx4[4] = {0, 1, 0, -1}, dy4[4] = {-1, 0, 1, 0};
    for (int i = 0; i < 4; ++i) {
        const int na = a + dx4[i], nb = b + dy4[i];
        if (!inMap(na, nb) || c.w.xbld.at(na, nb) >= 0xD) continue;
        const int diff = alt - x.alt(na, nb);
        int target;
        if (diff < 2) {
            if (diff > -2) continue;
            target = alt + 1;
        } else target = alt - 1;
        c.w.altm.at(na, nb) = static_cast<uint16_t>((c.w.altm.at(na, nb) & 0xFFE0) | target);
        adjustNeighbours(x, na, nb);
        shoreFix9(c, na, nb);
    }
}

bool canRaise(Ctx& x, Terr& t, int a, int b) {   // 0x443660 (marks the tiles it visits with XBIT 8)
    World& w = x.c.w;
    if (!inMap(a, b)) return true;
    if ((w.xzon.at(a, b) & 0xF) == 7) return false;
    if ((w.xbit.at(a, b) & kWater) && t.ts.dozerSub == 0) return false;
    if (w.xbit.at(a, b) & kMark) return true;
    w.xbit.at(a, b) |= kMark;
    const int alt = x.alt(a, b);
    if (alt > 0x1D) return false;
    for (int i = 0; i < 8; ++i) {
        const int na = t.dx8(i) + a, nb = t.dy8(i) + b;
        if (!inMap(na, nb)) continue;
        if ((w.xzon.at(na, nb) & 0xF) == 7 || ((w.xbit.at(na, nb) & kWater) && t.ts.dozerSub == 0)) { w.xbit.at(a, b) &= static_cast<uint8_t>(~kMark); return false; }
    }
    auto sub = [&](int na, int nb) {
        if (x.alt(na, nb) < alt && !canRaise(x, t, na, nb)) { w.xbit.at(a, b) &= static_cast<uint8_t>(~kMark); return false; }
        return true;
    };
    if (a > 0 && !sub(a - 1, b)) return false;
    if (b > 0 && !sub(a, b - 1)) return false;
    if (a < 0x7F && !sub(a + 1, b)) return false;
    if (b < 0x7F && !sub(a, b + 1)) return false;
    return true;
}

void raiseMarked(Ctx& x, Terr& t, int a, int b) {   // 0x443B70
    World& w = x.c.w;
    if (!inMap(a, b) || !(w.xbit.at(a, b) & kMark)) return;
    w.xbit.at(a, b) &= static_cast<uint8_t>(~kMark);
    const int alt = x.alt(a, b);
    if (alt >= 0x1E) return;
    if (a > 0 && x.alt(a - 1, b) < alt) raiseMarked(x, t, a - 1, b);
    if (b > 0 && x.alt(a, b - 1) < alt) raiseMarked(x, t, a, b - 1);
    if (a < 0x7F && x.alt(a + 1, b) < alt) raiseMarked(x, t, a + 1, b);
    if (b < 0x7F && x.alt(a, b + 1) < alt) raiseMarked(x, t, a, b + 1);
    const int cost = x.c.t.toolCost[2];
    if (cost <= x.c.s.treasury) {
        x.c.s.treasury -= cost;
        w.altm.at(a, b) = static_cast<uint16_t>((w.altm.at(a, b) & 0xFFE0) | (alt + 1));
        w.xzon.at(a, b) &= 0xF0;
        adjustNeighbours(x, a, b);
        shoreFix9(x.c, a, b);
    }
}

}  // namespace

bool raiseTerrain(SimContext& c, ToolState& ts, int a, int b) {   // 0x443610
    Ctx x(c);
    Terr t{x, ts};
    if (!canRaise(x, t, a, b)) return false;
    raiseMarked(x, t, a, b);
    return true;
}

bool lowerTerrain(SimContext& c, ToolState& ts, int a0, int b0, bool pay) {   // 0x443DF0
    Ctx x(c);
    Terr t{x, ts};
    World& w = c.w;
    IntRect rect{a0, b0, a0 + 1, b0 + 1};
    const int cost = c.t.toolCost[static_cast<size_t>(ts.dozerSub)];
    if (pay && c.s.treasury < cost) return false;
    const int alt0 = x.alt(a0, b0);
    if (alt0 == 0) return false;
    if (inMap(a0, b0)) w.altm.at(a0, b0) = static_cast<uint16_t>((w.altm.at(a0, b0) & 0xFFE0) | (alt0 - 1));
    if (pay) c.s.treasury -= cost;
    c.queue.reset(a0, b0);
    while (!c.queue.empty()) {
        const auto q = c.queue.pop();
        const int alt = x.alt(q.a, q.b);
        if (inMap(q.a, q.b)) w.xzon.at(q.a, q.b) &= 0xF0;
        rect = unionRect(rect, IntRect{std::min(a0, q.a), std::min(b0, q.b), std::max(a0, q.a), std::max(b0, q.b)});
        int mask = 0;
        for (int i = 0; i < 8; ++i) {
            const int na = t.dx8(i) + q.a, nb = t.dy8(i) + q.b;
            if (inMap(na, nb) && alt < x.alt(na, nb)) mask |= Terr::maskTable(i);
        }
        for (int i = 0; i < 8; ++i) {
            const int na = t.dx8(i) + q.a, nb = t.dy8(i) + q.b;
            if (!inMap(na, nb)) continue;
            const int h = x.alt(na, nb);
            if (alt + 1 < h || (alt < h && mask == 0xF)) {
                if (pay) c.s.treasury -= cost;
                c.queue.push(na, nb);
                w.altm.at(na, nb) = static_cast<uint16_t>((w.altm.at(na, nb) & 0xFFE0) | (h - 1));
            }
        }
    }
    for (int a = rect.l - 1; a <= rect.r + 1; ++a)
        for (int b = rect.t - 1; b <= rect.b + 1; ++b) shoreFix9(c, a, b);
    if (pay && c.s.treasury < 0) c.s.treasury = 0;
    return true;
}

bool terrainSprayRaise(SimContext& c, ToolState&, int a0, int b0, int pa, int pb) {   // 0x443400, one sample
    Ctx x(c);
    const int r1 = c.rng.next(), r2 = c.rng.next(), r3 = c.rng.next();
    const int a = pa + ((r1 & 3) - (r2 & 3));
    const int r4 = c.rng.next();
    const int b = pb + ((r3 & 3) - (r4 & 3));
    if (!inMap(a, b)) return false;
    // 0x4449A0: a tile can rise one step when it is below 0x1F and no neighbour is lower
    const int alt = x.alt(a, b);
    if (alt > 0x1E) return false;
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    for (int i = 0; i < 4; ++i) {
        const int na = a + dx[i], nb = b + dy[i];
        if (inMap(na, nb) && x.alt(na, nb) < alt) return false;
    }
    c.w.altm.at(a, b) = static_cast<uint16_t>((c.w.altm.at(a, b) & 0xFFE0) | (alt + 1));
    adjustNeighbours(x, a0, b0);   // the original passes the tile where the drag began here, not the raised one
    shoreFix9(c, a, b);
    return true;
}

bool terrainSprayLower(SimContext& c, ToolState& ts, int pa, int pb) {   // 0x4432B0, one sample
    const int r1 = c.rng.next(), r2 = c.rng.next(), r3 = c.rng.next();
    const int a = pa + ((r1 & 3) - (r2 & 3));
    const int r4 = c.rng.next();
    const int b = pb + ((r3 & 3) - (r4 & 3));
    if (!inMap(a, b)) return false;
    return lowerTerrain(c, ts, a, b, false);
}

bool terrainStretch(SimContext& c, ToolState&, int a, int b, int startAlt, int& lastAlt, int dy, int zoom) {   // 0x4442A0, one sample
    Ctx x(c);
    const int scale = zoom == 0 ? 3 : zoom == 1 ? 6 : 12;
    const int neu = startAlt + static_cast<int16_t>(dy) / scale;
    if (!(neu > 0 && neu < 0x1F && startAlt - 10 <= neu && lastAlt != neu)) return false;
    c.w.altm.at(a, b) = static_cast<uint16_t>((c.w.altm.at(a, b) & 0xFFE0) | neu);
    adjustNeighbours(x, a, b);
    shoreFix9(c, a, b);
    lastAlt = neu;
    return true;
}

bool terrainLevelSample(SimContext& c, ToolState&, int pa, int pb, int prevA, int prevB, int startAlt) {   // 0x4423E0, one sample
    Ctx x(c);
    if (pa == prevA && pb == prevB) return false;
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};   // 0x4E6878 / 0x4E6880
    for (int i = 0; i < 4; ++i) {
        const int na = pa + dx[i], nb = pb + dy[i];
        if (!inMap(na, nb)) continue;
        c.w.altm.at(na, nb) = static_cast<uint16_t>((c.w.altm.at(na, nb) & 0xFFE0) | (startAlt & 0x1F));
        adjustNeighbours(x, na, nb);
        shoreFix9(c, na, nb);
    }
    if (inMap(prevA, prevB)) c.w.xtxt.at(prevA, prevB) = 0;   // the pointer-cursor marker (0xC9) moves with the pointer and is wiped from the old tile
    c.w.xtxt.at(pa, pb) = 0xC9;
    return true;
}

void terrainLevelEnd(SimContext& c, int lastA, int lastB) {   // end of the 0x4423E0 loop (button released)
    if (inMap(lastA, lastB)) c.w.xtxt.at(lastA, lastB) = 0;
}

bool levelTerrain(SimContext& c, ToolState& ts, int a, int b, int target) {   // 0x442BB0
    Ctx x(c);
    const int alt = x.alt(a, b);
    if (alt < target) return raiseTerrain(c, ts, a, b);
    if (target < alt) return lowerTerrain(c, ts, a, b, true);
    return true;
}

// ---- hydroelectric dam, tunnel, onramp, rail link ----
bool placeHydroDam(SimContext& c, ToolState&, int a, int b) {   // 0x440DB0
    Ctx x(c);
    World& w = c.w;
    constexpr int kFlip[16] = {1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1};
    if (w.xter.at(a, b) != 0x2E && w.xter.at(a, b) != 0x3E) { c.hooks.showDialog(0x6C); return false; }
    if (w.xbld.at(a, b) != 0) return false;
    const int cost = c.t.toolCost[static_cast<size_t>(toolcat::kPower * 12 + 3)];   // 400
    if (c.s.treasury < cost) return false;
    c.s.treasury -= cost;
    const int alt = x.alt(a, b);
    int m = 0;
    if (b > 0 && alt < x.alt(a, b - 1)) m = 1;
    if (a > 0 && a + 1 < 0x80 && alt < x.alt(a + 1, b)) m |= 2;
    if (b < 0x7F && alt < x.alt(a, b + 1)) m |= 4;
    if (a < 0x7F && a - 1 >= 0 && alt < x.alt(a - 1, b)) m |= 8;
    int kind = kFlip[m];
    if (c.s.rotation & 1) kind ^= 1;
    w.xbit.at(a, b) |= kPowerable;
    const int id = kind + 0xC6;
    setTile(c, a, b, id);
    w.xzon.at(a, b) = static_cast<uint8_t>((w.xzon.at(a, b) & 0xF0) | 0xF0);
    const int overlay = allocateMicrosim(c, id);
    if (overlay != 0) w.xtxt.at(a, b) = static_cast<uint8_t>(overlay);
    if (c.s.population10 < 50000) refreshPower(c);
    return true;
}

bool buildTunnel(SimContext& c, ToolState&, int a0, int b0, int roadSub) {   // 0x44B820
    Ctx x(c);
    World& w = c.w;
    if (!(w.xbld.at(a0, b0) < 0xE && w.xbld.at(a0, b0) != 5 && w.xund.at(a0, b0) == 0)) return false;
    const int ter0 = w.xter.at(a0, b0);
    if (ter0 == 0 || ter0 > 4) { c.hooks.toolMessage(0x74); return false; }
    const int startAlt = x.alt(a0, b0);
    const int dir = (ter0 + 2) & 3;
    int len = 0, a = a0, b = b0;
    do {
        if (a < 0 || b < 0 || a > 0x7E || b > 0x7E) { c.hooks.toolMessage(0x70); return false; }
        if (x.depth(a, b) != 0) { c.hooks.toolMessage(0x6F); return false; }
        const int d = x.alt(a, b) - startAlt;
        if (d > 0x1E) { c.hooks.toolMessage(0x71); return false; }
        if (d == 1) {
            const int u = x.und(a, b);
            if (pipeish2(u) || subwayish(u)) { c.hooks.toolMessage(0x73); return false; }
        }
        ++len;
        a += x.tb.dx(dir); b += x.tb.dy(dir);
        if (a < 0) { c.hooks.toolMessage(0x72); return false; }
    } while (a < 0x80 && b >= 0 && b < 0x80 && startAlt < x.alt(a, b));
    if (a < 0 || a > 0x7F || b < 0 || b > 0x7F || w.xter.at(a, b) - ((ter0 + 1) & 3) != 1) { c.hooks.toolMessage(0x72); return false; }
    const int cost = c.t.toolCost[static_cast<size_t>(toolcat::kRoads * 12 + roadSub)] * (len + 1);
    if (!c.hooks.confirm(0x15A, cost)) return false;
    if (cost > c.s.treasury) { c.hooks.toolMessage(0x102); return false; }
    c.s.treasury -= cost;
    int pa = a0, pb = b0;
    setTile(c, pa, pb, ter0 + 0x3E);
    w.altm.at(pa, pb) = static_cast<uint16_t>((w.altm.at(pa, pb) & 0x3FF) | 0x400);
    auto roads = [&](int ra, int rb) {
        if (ra > 0) fixRoad(x, ra - 1, rb);
        if (ra < 0x7F) fixRoad(x, ra + 1, rb);
        if (rb > 0) fixRoad(x, ra, rb - 1);
        if (rb < 0x7F) fixRoad(x, ra, rb + 1);
    };
    roads(pa, pb);
    for (int i = 0; i < len; ++i) {   // `len` steps from the entrance end on the exit tile
        pa += x.tb.dx(dir); pb += x.tb.dy(dir);
        const uint16_t old = w.altm.at(pa, pb);
        w.altm.at(pa, pb) = static_cast<uint16_t>((((old & 0x1F) - startAlt) + 1) * 0x400 | (old & 0x3FF));
    }
    setTile(c, pa, pb, ((ter0 + 1) & 3) + 0x3F);
    w.altm.at(pa, pb) = static_cast<uint16_t>((w.altm.at(pa, pb) & 0x3FF) | 0x400);
    roads(pa, pb);
    return true;
}

bool placeOnramp(SimContext& c, ToolState&, int a, int b, int roadSub) {   // 0x441160
    Ctx x(c);
    World& w = c.w;
    constexpr int kTable[16] = {0, 10, 5, 12, 10, 0, 9, 0, 5, 6, 0, 0, 3, 0, 0, 0};
    if (x.bld(a, b) > 0xD || x.bld(a, b) == 5 || w.xter.at(a, b) != 0) return false;
    int hw = 0, rd = 0;
    auto isHw = [](int n) { return n > 0x48 && n < 0x51; };
    auto isRd = [](int n) { return n > 0x1C && n < 0x2C; };
    if (b > 0) { const int n = x.bld(a, b - 1); if (isHw(n)) hw = 1; if (isRd(n)) rd = 1; }
    if (a < 0x7F) { const int n = x.bld(a + 1, b); if (isHw(n)) hw |= 2; if (isRd(n)) rd |= 2; }
    if (b < 0x7F) { const int n = x.bld(a, b + 1); if (isHw(n)) hw |= 4; if (isRd(n)) rd |= 4; }
    if (a > 0) { const int n = x.bld(a - 1, b); if (isHw(n)) hw |= 8; if (isRd(n)) rd |= 8; }
    rd &= kTable[hw];
    if (rd == 0) return false;
    const int cost = c.t.toolCost[static_cast<size_t>(toolcat::kRoads * 12 + roadSub)];
    if (c.s.treasury < cost) return false;
    c.s.treasury -= cost;
    int piece;
    if (!(rd & 1)) {
        if (!(rd & 2)) {
            if (!(rd & 4)) {
                if (!(rd & 8)) return true;
                setTile(c, a - 1, b, 0x2B);
                piece = (hw & 1) ? 0x5E : 0x5F;
            } else {
                setTile(c, a, b + 1, 0x2B);
                w.xbit.at(a, b) |= kFlipped;
                piece = (hw & 2) ? 0x60 : 0x5D;
            }
        } else {
            setTile(c, a + 1, b, 0x2B);
            piece = (hw & 1) ? 0x5D : 0x60;
        }
    } else {
        w.xbit.at(a, b) |= kFlipped;
        setTile(c, a, b - 1, 0x2B);
        piece = (hw & 2) ? 0x5F : 0x5E;
    }
    setTile(c, a, b, piece);
    return true;
}

bool placeRailLink(SimContext& c, ToolState&, int a, int b) {   // 0x441530
    Ctx x(c);
    World& w = c.w;
    if (x.bld(a, b) > 0xC || x.bld(a, b) == 5 || w.xter.at(a, b) != 0) return false;
    static const int dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};   // 0x4E6830 / 0x4E6838
    int i = 0, na = 0, nb = 0;
    for (; i < 4; ++i) {
        na = dx[i] + a; nb = dy[i] + b;
        if (inMap(na, nb) && ((x.bld(na, nb) > 0x2B && x.bld(na, nb) < 0x3F) || (x.bld(na, nb) > 0x44 && x.bld(na, nb) < 0x49) || (x.bld(na, nb) > 0x6B && x.bld(na, nb) < 0x70))) break;
    }
    if (i == 4) {
        for (i = 0; i < 4; ++i) {
            na = dx[i] + a; nb = dy[i] + b;
            if (inMap(na, nb) && subwayish(x.und(na, nb))) { i = (i + 2) & 3; break; }
        }
        if (i == 4) return false;
    }
    applySubwayPiece(c, a, b);
    setUnderground(c, a, b, 0x23);
    w.xbit.at(a, b) &= static_cast<uint8_t>(~kPiped);
    setTile(c, a, b, i + 0x6C);
    w.xzon.at(a, b) |= 0xF0;
    fixRail(x, na, nb);
    return true;
}

// ---- landscape ----
bool plantTree(SimContext& c, ToolState& ts, int a, int b) {   // 0x43F4E0
    World& w = c.w;
    if (inMap(a, b) && (w.xbit.at(a, b) & kWater)) return false;
    if (w.xbld.at(a, b) == 5) return false;
    if (w.xter.at(a, b) == 0x2E || w.xter.at(a, b) == 0x3E) return false;
    int id;
    if (!ts.shift) {
        const int cur = w.xbld.at(a, b);
        if (cur < 6) id = (static_cast<unsigned>(c.rng.next()) & 1) + 6;
        else if (cur < 0xB) id = cur + 1;
        else if (cur > 0xC) return false;
        else id = (static_cast<unsigned>(c.rng.next()) & 1) + 0xB;
    } else id = 0;
    setTile(c, a, b, id);
    return true;
}

void plantForest(SimContext& c, ToolState& ts, int a0, int b0) {   // 0x43F620: 20 random spots around the click
    World& w = c.w;
    for (int n = 0; n < 20; ++n) {
        const int r1 = c.rng.next() & 7, r2 = c.rng.next() & 7, r3 = c.rng.next() & 7;
        const int a = a0 + (r1 - r2);
        const int r4 = c.rng.next() & 7;
        const int b = b0 + (r3 - r4);
        if (!inMap(a, b) || (w.xbit.at(a, b) & kWater)) continue;
        const int ter = w.xter.at(a, b);
        if (ter == 0x2E || ter == 0x3E) continue;
        const int cur = w.xbld.at(a, b);
        if (!ts.shift) {
            if (cur < 6) setTile(c, a, b, (static_cast<unsigned>(c.rng.next()) & 1) + 6);
            else if (cur < 0xB) setTile(c, a, b, cur + 1);
            else if (cur < 0xD) setTile(c, a, b, (static_cast<unsigned>(c.rng.next()) & 1) + 0xB);
        } else if (cur > 5) {
            if (cur == 6) setTile(c, a, b, 0);
            else if (cur <= 0xC) setTile(c, a, b, cur - 1);
        }
    }
}

bool placeWater(SimContext& c, ToolState&, int a, int b) {   // 0x43F7E0
    World& w = c.w;
    if (inMap(a, b) && (w.xbit.at(a, b) & kWater)) return false;
    if (w.xtxt.at(a, b) > 0xF9) return false;
    if (w.xbld.at(a, b) >= 0xE || w.xbld.at(a, b) == 5) return false;
    floodTile(c, a, b);
    w.xzon.at(a, b) &= 0xF0;
    return true;
}

void placeStream(SimContext& c, ToolState&, int a, int b, int length) {   // 0x445DF0
    Ctx x(c);
    World& w = c.w;
    static const int dx[4] = {-1, 0, 1, 0}, dy[4] = {0, 1, 0, -1}, turn[4] = {0, 1, 3, 2};   // 0x4E6A88 / 0x4E6A90 / 0x4E6A98
    int heading = 1;
    floodTile(c, a, b);
    while (length-- > 0) {
        int alt = x.alt(a, b);
        if (w.xter.at(a, b) == 0x3E) ++alt;
        int k = 0, na = 0, nb = 0;
        for (; k < 4; ++k) {
            const int d = (turn[k] + heading) & 3;
            na = dx[d] + a; nb = dy[d] + b;
            if (!inMap(na, nb)) return;
            const int h = x.alt(na, nb);
            if (h <= alt) {
                const int t = w.xter.at(na, nb);
                if (t > 0xF && t < 0x30) return;
                if (h < alt || t == 0) break;
            }
        }
        if (k == 4) return;
        floodTile(c, na, nb);
        heading += turn[k];
        const unsigned r = static_cast<unsigned>(c.rng.next());
        if ((r & 0xFFFF) % 3 != 0) {
            const unsigned r2 = static_cast<unsigned>(c.rng.next());
            heading = static_cast<int16_t>(heading + static_cast<int>(r2) * 2 + 1) & 3;
        }
        a = na; b = nb;
    }
}

bool landscapeTool(SimContext& c, ToolState& ts, int sub, int a, int b) {   // 0x441DF0
    const int cost = c.t.toolCost[static_cast<size_t>(12 + sub)];
    if (c.s.treasury < cost) return false;
    bool ok = false;
    if (sub == 0) ok = plantTree(c, ts, a, b);
    else if (sub == 1) ok = placeWater(c, ts, a, b);
    if (ok) c.s.treasury -= cost;
    return ok;
}

// ---- emergency dispatch ----
void startDispatch(SimContext& c, ToolState& ts) {   // 0x44F290
    World& w = c.w;
    ts.dispatchWarned[0] = ts.dispatchWarned[1] = ts.dispatchWarned[2] = false;
    ts.dispatchLimit[0] = static_cast<int16_t>(c.s.census[0xD2]) >> 3;
    ts.dispatchLimit[1] = static_cast<int16_t>(c.s.census[0xD3]) >> 3;
    static const int militaryByBase[6] = {0, 0, 5, 2, 3, 0};
    ts.dispatchLimit[2] = militaryByBase[static_cast<size_t>(c.s.militaryType) % 6];
    if (!ts.dispatchLimit[0] && !ts.dispatchLimit[1] && !ts.dispatchLimit[2]) {
        ts.dispatchLimit[2] = 1;
        c.hooks.toolMessage(0x77);
    }
    ts.dispatchRing[0] = ts.dispatchRing[1] = ts.dispatchRing[2] = 0;
    for (int a = 0; a < 128; ++a)
        for (int b = 0; b < 128; ++b) {
            const int v = w.xtxt.at(a, b);
            if (v > 200 && v < 0xF1) {
                uint8_t& type = w.xthg[static_cast<size_t>(v - 0xC9) * 12];
                if (type == 7 || type == 8 || type == 0xE) { w.xtxt.at(a, b) = 0; type = 0; }
            }
        }
}

bool placeDispatch(SimContext& c, ToolState& ts, int kind, int a, int b) {   // 0x44F4D0 / 0x44F6E0 / 0x44F8F0
    World& w = c.w;
    static const int type[3] = {7, 8, 0xE}, noneMsg[3] = {0x78, 0x7A, 0x7C}, badMsg[3] = {0x79, 0x7B, 0x7D};
    if (ts.dispatchLimit[kind] == 0) { c.hooks.showDialog(noneMsg[kind]); return false; }
    if ((!inMap(a, b) || !(w.xbit.at(a, b) & kWater)) && w.xtxt.at(a, b) == 0) {
        int& ring = ts.dispatchRing[kind];
        if (++ring > ts.dispatchLimit[kind]) ring = 1;
        const int oa = ts.dispatchX[kind][ring], ob = ts.dispatchY[kind][ring];
        if (oa < 0x80 && ob < 0x80) {
            const int v = w.xtxt.at(oa, ob);
            if (v > 200 && v < 0xF1 && w.xthg[static_cast<size_t>(v - 0xC9) * 12] == type[kind]) {
                w.xthg[static_cast<size_t>(v - 0xC9) * 12] = 0;
                w.xtxt.at(oa, ob) = 0;
            }
        }
        // 0x450010: a free thing slot carries the marker (the overlay value is slot + 0xC9)
        int slot = 1;
        while (slot < 0x28 && w.xthg[static_cast<size_t>(slot) * 12] != 0) ++slot;
        if (slot == 0x28) {
            if (c.s.mode != 2) { ts.dispatchX[kind][ring] = static_cast<uint8_t>(a); ts.dispatchY[kind][ring] = static_cast<uint8_t>(b); return true; }
            slot = 0x27;
            removeThing(c, 0x27);
        }
        if (w.xtxt.at(a, b) == 0) {
            uint8_t* rec = &w.xthg[static_cast<size_t>(slot) * 12];
            rec[0] = static_cast<uint8_t>(type[kind]); rec[1] = 0; rec[3] = static_cast<uint8_t>(a); rec[4] = static_cast<uint8_t>(b);
            rec[6] = 0; rec[7] = 0; rec[5] = 0; rec[10] = w.xtxt.at(a, b);
            w.xtxt.at(a, b) = static_cast<uint8_t>(slot + 0xC9);
        }
        ts.dispatchX[kind][ring] = static_cast<uint8_t>(a);
        ts.dispatchY[kind][ring] = static_cast<uint8_t>(b);
        return true;
    }
    if (!ts.dispatchWarned[kind]) { c.hooks.showDialog(badMsg[kind]); ts.dispatchWarned[kind] = true; }
    return false;
}


}  // namespace sc2k
