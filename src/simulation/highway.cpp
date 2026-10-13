// Highways: 2x2-tile pieces with their own planner and bridge builder. Literal port of SIMCITY.EXE build .022 routines
// 0x461B80 (tool), 0x4639D0 (site classifier), 0x4638F0 (site height), 0x461370 / 0x461460 (planner begin / step), 0x462060 (apply),
// 0x4622A0 (piece chooser), 0x462860 (piece placer), 0x4634C0 (road/rail crossing fixer), 0x461200 (preview) and 0x45BB30 (highway bridges).
// See docs/simulation/tools.md.
#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "simulation/tools.h"

namespace sc2k {
namespace {

using namespace xbit;

struct Hw {
    SimContext& c;
    ToolState& ts;
    World& w;
    explicit Hw(SimContext& cc, ToolState& t) : c(cc), ts(t), w(cc.w) {}

    // typed views of the raw tables (GameTables::highwayData starts at 0x4E75E8)
    const uint8_t* at(uint32_t va) const { return &c.t.highwayData[va - 0x4E75E8]; }
    int s16(uint32_t va) const { int16_t v; std::memcpy(&v, at(va), 2); return v; }
    int s32(uint32_t va) const { int32_t v; std::memcpy(&v, at(va), 4); return v; }
    int classTbl(int xter) const { return s32(0x4E7A48 + 4u * static_cast<uint32_t>(xter & 255)); }
    int terrWeight(int xter) const { return s16(0x4E75E8 + 2u * static_cast<uint32_t>(xter & 255)); }
    int dx2(int d) const { return s16(0x4E7930 + 2u * static_cast<uint32_t>(d & 3)); }
    int dy2(int d) const { return s16(0x4E7938 + 2u * static_cast<uint32_t>(d & 3)); }
    int bdx(int d) const { int32_t v; std::memcpy(&v, &c.t.bridgeData[0x20 + 4 * static_cast<size_t>(d & 3)], 4); return v; }   // 0x4E75C8
    int bdy(int d) const { int32_t v; std::memcpy(&v, &c.t.bridgeData[0x30 + 4 * static_cast<size_t>(d & 3)], 4); return v; }   // 0x4E75D8

    int bld(int a, int b) const { return inMap(a, b) ? w.xbld.at(a, b) : 0; }
    int ter(int a, int b) const { return inMap(a, b) ? w.xter.at(a, b) : 0; }
    int alt(int a, int b) const { return inMap(a, b) ? (w.altm.at(a, b) & 0x1F) : 0; }
    bool water(int a, int b) const { return inMap(a, b) && (w.xbit.at(a, b) & kWater); }
    bool quadWater(int a, int b) const { return water(a, b) || water(a, b + 1) || water(a + 1, b) || water(a + 1, b + 1); }

    // 0x4638F0: highest corner of the 2x2 site (each non-flat corner counts one higher)
    int height(int a, int b) const {
        int h = alt(a, b) + (ter(a, b) != 0);
        h = std::max(h, alt(a + 1, b) + (ter(a + 1, b) != 0));
        h = std::max(h, alt(a + 1, b + 1) + (ter(a + 1, b + 1) != 0));
        h = std::max(h, alt(a, b + 1) + (ter(a, b + 1) != 0));
        return h;
    }

    // 0x4639D0: 0x8000 = unusable site, 0xF = flat, 0x4000 = fine as is, otherwise a mask of the corners that must change
    int classify(int a, int b) const {
        static const int dxs[4] = {0, 1, 0, 1}, dys[4] = {0, 0, 1, 1};
        for (int i = 0; i < 4; ++i) {
            const int id = bld(a + dxs[i], b + dys[i]);
            if (!((id >= 0x61 && id <= 0x6B) || (id >= 0x49 && id <= 0x50))) {
                if (id == 0xD || id == 5) return 0x8000;
                if (id > 0x1E && id < 0x2C) return 0x8000;
                if (id > 0x2D && id < 0x49) return 0x8000;
                if (id > 0x50) return 0x8000;
            }
        }
        int s[5] = {0, 0, 0, 0, 0};
        s[classTbl(ter(a, b))] |= 1;
        s[classTbl(ter(a + 1, b))] |= 2;
        s[classTbl(ter(a + 1, b + 1))] |= 4;
        s[classTbl(ter(a, b + 1))] |= 8;
        const int s1 = s[1], s2 = s[2], s3 = s[3], s4 = s[4];
        const int uVar10 = s1 | s3;
        const int uVar9 = s1 | s2 | s3 | s4;
        if (uVar9 == 0) return 0xF;
        const int h00 = alt(a, b), h10 = alt(a + 1, b), h11 = alt(a + 1, b + 1), h01 = alt(a, b + 1);
        if ((h11 < h00 && (uVar9 & 1)) || (h00 < h11 && (uVar9 & 4))) return 0x8000;
        if ((h01 < h10 && (uVar9 & 2)) || (h10 < h01 && (uVar9 & 8))) return 0x8000;
        int low = std::min(std::min(h00, h10), std::min(h11, h01));
        int mask = (low < h00) ? 1 : 0;
        if (low < h10) mask |= 2;
        if (low < h11) mask |= 4;
        if (low < h01) mask |= 8;
        if (0xF - s4 == mask) return 0x4000;
        int r = 0;
        if (mask == 0) {
            if (ter(a, b) == 0xD) return 0x4000;
            if (uVar10 == 8 || uVar10 == 1 || uVar10 == 9) r = 1;
            if (uVar10 == 1 || uVar10 == 2 || uVar10 == 3) r |= 2;
            if (uVar10 == 2 || uVar10 == 4 || uVar10 == 6) r |= 4;
            if (uVar10 == 4 || uVar10 == 8 || uVar10 == 0xC) r |= 8;
        }
        if ((mask == 8 || mask == 1 || mask == 9) && (uVar10 & 6) == 6) r |= 1;
        if ((mask == 1 || mask == 2 || mask == 3) && (uVar10 & 0xC) == 0xC) r |= 2;
        if ((mask == 2 || mask == 4 || mask == 6) && (uVar10 & 9) == 9) r |= 4;
        if ((mask == 4 || mask == 8 || mask == 0xC) && (uVar10 & 3) == 3) r |= 8;
        if (uVar9 == 7) { if (s3 & 1) r |= 4; if (s3 & 4) r |= 2; }
        if (uVar9 == 0xB) { if (s3 & 8) r |= 2; if (s3 & 2) r |= 1; }
        if (uVar9 == 0xD) { if (s3 & 4) r |= 1; if (s3 & 1) r |= 8; }
        if (uVar9 == 0xE) { if (s3 & 8) r |= 4; if (s3 & 2) r |= 8; }
        return r;
    }

    // ---- planner (0x461370 / 0x461460) ----
    static int dirTowards(int a, int b, int ta, int tb) {
        const int dx = std::abs(a - ta), dy = std::abs(b - tb);
        if (dy < dx) return a <= ta ? 1 : 3;
        return tb < b ? 0 : 2;
    }
    static int dirAlternative(int a, int b, int ta, int tb) {
        const int dx = std::abs(a - ta), dy = std::abs(b - tb);
        if (dy < dx) { if (dy > 0) return tb < b ? 0 : 2; return a <= ta ? 1 : 3; }
        if (dx < 1) return tb < b ? 0 : 2;
        return a <= ta ? 1 : 3;
    }

    void begin(int a, int b, int ta, int tb) {   // 0x461370
        ts.targetA = ta & ~1; ts.targetB = tb & ~1;
        ts.dir = dirTowards(a, b, ta, tb);
        // (the original tests classify() == 0x8000 by comparing a sign-extended short with +0x8000: never true, so the target is never reset)
        ts.minA = std::min(a, ta); ts.maxA = std::max(a, ta);
        ts.minB = std::min(b, tb); ts.maxB = std::max(b, tb);
    }

    bool step(int& pa, int& pb) {   // 0x461460
        const int a = pa, b = pb;
        if (a == ts.targetA && b == ts.targetB) return false;
        bool straight = false;
        const int cls = classify(a, b);   // callers' `== 0x8000` tests are vacuous in the original (sign-extended compare), see begin()
        auto crossingOk = [&](int id) { const int s = id + (ts.dir & 1); return s == 0x1E || s == 0xF || s == 0x2D || s == 0x40; };
        if (cls == 0xF) {
            const int xs[4] = {a, a + 1, a + 1, a}, ys[4] = {b, b, b + 1, b + 1};
            for (int i = 0; i < 4; ++i) {
                const int id = bld(xs[i], ys[i]);
                if (id > 0xE) {
                    straight = crossingOk(id);
                    if (!straight) return false;
                }
            }
        } else straight = true;
        const int h = height(a, b);
        int na, nb, nd;
        auto fits = [&](int xa, int xb) {
            const int h2 = height(xa, xb);
            return h2 <= h + 1 && h - 1 <= h2;
        };
        if (straight) {
            na = dx2(ts.dir) * 2 + a; nb = dy2(ts.dir) * 2 + b;
            if (na < ts.minA || nb < ts.minB || ts.maxA < na || ts.maxB < nb) return false;
            if (!fits(na, nb)) return false;
            nd = ts.dir;
        } else {
            int d1 = dirTowards(a, b, ts.targetA, ts.targetB);
            na = dx2(d1) * 2 + a; nb = dy2(d1) * 2 + b;
            if (fits(na, nb)) nd = d1;
            else {
                const int d2 = dirAlternative(a, b, ts.targetA, ts.targetB);
                na = dx2(d2) * 2 + a; nb = dy2(d2) * 2 + b;
                if (!fits(na, nb)) return false;
                nd = d2;
            }
        }
        ts.dir = nd;
        if (quadWater(na, nb)) return false;
        const int xs[4] = {na, na + 1, na + 1, na}, ys[4] = {nb, nb, nb + 1, nb + 1};
        for (int i = 0; i < 4; ++i) {
            const int id = bld(xs[i], ys[i]);
            if (id > 0xE && id - (ts.dir & 1) == 0x2C) return false;
        }
        pa = na; pb = nb;
        return true;
    }

    // ---- pieces ----
    void crossing(int a, int b, int flip) {   // 0x4634C0
        int id;
        switch (bld(a, b)) {
            case 0xE: id = 0x50; break;
            case 0xF: id = 0x4F; break;
            case 0x1D: id = 0x4C; break;
            case 0x1E: id = 0x4B; break;
            case 0x2C: id = 0x4E; break;
            case 0x2D: id = 0x4D; break;
            default: id = flip + 0x49; break;
        }
        setTile(c, a, b, id);
        if (inMap(a, b)) w.xzon.at(a, b) |= 0xF0;
    }

    void corners(int a, int b) {   // zone byte corner flags of a 2x2 piece for the current rotation
        const uint8_t* corner = &c.t.rotationCorner[static_cast<size_t>(c.s.rotation & 3) * 8];
        auto put = [&](int x, int y, uint8_t v) { if (inMap(x, y)) w.xzon.at(x, y) = static_cast<uint8_t>((w.xzon.at(x, y) & 0xF) | v); };
        put(a, b, corner[0]); put(a + 1, b, corner[2]); put(a + 1, b + 1, corner[4]); put(a, b + 1, corner[6]);
    }

    void place(int a, int b, int piece) {   // 0x462860
        const int h = height(a, b);
        auto setT = [&](int x, int y, int v) { if (inMap(x, y)) w.xter.at(x, y) = static_cast<uint8_t>(v); };
        auto setAlt = [&](int x, int y, int v) { if (inMap(x, y)) w.altm.at(x, y) = static_cast<uint16_t>((w.altm.at(x, y) & 0xFFE0) | (v & 0x1F)); };
        auto stamp = [&](int id) {
            setTile(c, a, b, id); setTile(c, a + 1, b, id); setTile(c, a + 1, b + 1, id); setTile(c, a, b + 1, id);
        };
        if (piece < 4) {
            if (alt(a, b) < h) setT(a, b, 0xD);
            if (alt(a, b + 1) < h) setT(a, b + 1, 0xD);
            if (alt(a + 1, b + 1) < h) setT(a + 1, b + 1, 0xD);
            if (alt(a + 1, b) < h) setT(a + 1, b, 0xD);
            const int xs[4] = {a, a + 1, a, a + 1}, ys[4] = {b, b, b + 1, b + 1};
            for (int i = 0; i < 4; ++i) {
                const int id = bld(xs[i], ys[i]);
                if (id == 0x1D || id == 0xE || id == 0x2C) piece = 3;
                else if (id == 0x1E || id == 0xF || id == 0x2D) piece = 0;
            }
            const int flip = piece & 1;
            crossing(a, b, flip); crossing(a + 1, b, flip); crossing(a + 1, b + 1, flip); crossing(a, b + 1, flip);
            return;
        }
        if (piece > 7) {
            if (piece > 0xC) {
                stamp(piece + 0x5D);
                corners(a, b);
                return;
            }
            if (ter(a, b) != 0 || alt(a, b) < h) setT(a, b, 0xD);
            if (ter(a + 1, b) != 0 || alt(a + 1, b) < h) setT(a + 1, b, 0xD);
            if (ter(a + 1, b + 1) != 0 || alt(a + 1, b + 1) < h) setT(a + 1, b + 1, 0xD);
            if (ter(a, b + 1) != 0 || alt(a, b + 1) < h) setT(a, b + 1, 0xD);
            stamp(piece + 0x5D);
            corners(a, b);
            return;
        }
        if (alt(a, b) != h || alt(a + 1, b) != h || alt(a + 1, b + 1) != h || alt(a, b + 1) != h) {
            setAlt(a, b, h - 1); setAlt(a + 1, b, h - 1); setAlt(a + 1, b + 1, h - 1); setAlt(a, b + 1, h - 1);
        }
        switch (piece) {
            case 4: setT(a, b, 0xD); setT(a + 1, b, 1); setT(a, b + 1, 0xD); setT(a + 1, b + 1, 1); break;
            case 5: setT(a, b, 0xD); setT(a + 1, b, 0xD); setT(a, b + 1, 2); setT(a + 1, b + 1, 2); break;
            case 6: setT(a, b, 3); setT(a + 1, b, 0xD); setT(a, b + 1, 3); setT(a + 1, b + 1, 0xD); break;
            case 7: setT(a, b, 4); setT(a + 1, b, 4); setT(a, b + 1, 0xD); setT(a + 1, b + 1, 0xD); break;
            default: break;
        }
        stamp(piece + 0x5D);
        corners(a, b);
    }

    // 0x4622A0: which piece belongs on the 2x2 site (a, b), or -1
    int choose(int a, int b) {
        if (a < 0 || a > 0x7F || b < 0 || b > 0x7F) return -1;
        int seg = bridgeSegment(c, a, b);
        if (seg == 0 || seg == 1) return -1;
        if (seg > 3 && seg < 8) return -1;
        if (seg > 0xC) return -1;
        int r = 0;
        const int h = height(a, b);
        auto tbl79c8 = [&](int k) { return s32(0x4E79C8 + 4u * static_cast<uint32_t>(k)); };
        auto tbl7980 = [&](int k) { return s32(0x4E7980 + 4u * static_cast<uint32_t>(k)); };
        if (b > 1) {
            const int n = bridgeSegment(c, a, b - 2);
            if (n == 1 || n == 3 || (n > 3 && tbl79c8(n) != 0) || (n == 2 && classify(a, b - 2) != 0x4000)) {
                r = 1;
                const int h2 = height(a, b - 2);
                if (h < h2 && n != 5) r = 0x11;
                if (h2 < h || (h2 == h && n == 5)) r |= 0x40;
            }
        }
        if (a < 0x7E) {
            const int n = bridgeSegment(c, a + 2, b);
            if (n == 0 || n == 2 || (n > 3 && tbl7980(n) != 0) || (n == 3 && classify(a + 2, b) != 0x4000)) {
                const int h2 = height(a + 2, b);
                int u = r | 2;
                if (h < h2 && n != 6) u = r | 0x22;
                r = u;
                if (h2 < h || (h2 == h && n == 6)) r |= 0x80;
            }
        }
        if (b < 0x7E) {
            const int n = bridgeSegment(c, a, b + 2);
            if (n == 1 || n == 3 || (n > 3 && tbl79c8(n) != 0) || (n == 2 && classify(a, b + 2) != 0x4000)) {
                const int h2 = height(a, b + 2);
                int u = r | 4;
                if (h < h2 && n != 7) u = r | 0x44;
                r = u;
                if (h2 < h || (h2 == h && n == 7)) r |= 0x10;
            }
        }
        if (a > 1) {
            const int n = bridgeSegment(c, a - 2, b);
            if (n == 0 || n == 2 || (n > 3 && tbl7980(n) != 0) || (n == 3 && classify(a - 2, b) != 0x4000)) {
                const int h2 = height(a - 2, b);
                int u = r | 8;
                if (h < h2 && n != 4) u = r | 0x88;
                r = u;
                if (h2 < h || (h2 == h && n == 4)) r |= 0x20;
            }
        }
        if (w.xtxt.at(a, b) == 0xFA) {
            if (a < 2) r |= 8;
            if (a > 0x7D) r |= 2;
            if (b < 2) r |= 1;
            if (b > 0x7D) r |= 4;
        }
        const int own = bridgeSegment(c, a, b);
        const unsigned cl = static_cast<unsigned>(classify(a, b));
        const int cs = static_cast<int16_t>(cl);
        if (own == 2 || own == 3) {
            if (r == 1 || r == 4) return 2;
            if (r == 2 || r == 8) return 3;
            if (cs == 0x4000) return -1;
        }
        if (r == 0) {
            if (cs == 0xF) return (ts.dir & 1) + 2;
            if (cl & 2) return 5;
            if (cl & 8) return 7;
            if (cl & 1) return 4;
            if (cl & 4) return 6;
        }
        if ((r & 0x10) && (cl & 2)) return 5;
        if ((r & 0x40) && (cl & 8)) return 7;
        if ((r & 0x80) && (cl & 1)) return 4;
        if ((r & 0x20) && (cl & 4)) return 6;
        if (cs != 0xF) {
            if ((r & 1) && (cl & 2)) return 5;
            if ((r & 4) && (cl & 8)) return 7;
            if ((r & 8) && (cl & 1)) return 4;
            if ((r & 2) && (cl & 4)) return 6;
            return s16Raw(0x4E7960, r & 0xF);
        }
        return s16Raw(0x4E7940, r & 0xF);
    }
    int s16Raw(uint32_t base, int idx) const { return s16(base + 2u * static_cast<uint32_t>(idx)); }

    void apply(int a, int b) {   // 0x462060
        for (int dx = 0; dx < 2; ++dx)
            for (int dy = 0; dy < 2; ++dy)
                if (inMap(a + dx, b + dy)) w.xzon.at(a + dx, b + dy) &= 0xF0;
        int p = choose(a, b);
        if (p >= 0) place(a, b, p);
        const int ox[4] = {-2, 2, 0, 0}, oy[4] = {0, 0, -2, 2};
        for (int i = 0; i < 4; ++i) {
            const int na = a + ox[i], nb = b + oy[i];
            if (bridgeSegment(c, na, nb) > 1) {
                p = choose(na, nb);
                if (p >= 0) place(na, nb, p);
            }
        }
        p = choose(a, b);
        if (p >= 0) place(a, b, p);
    }
};

}  // namespace

int highwayClassify(SimContext& c, int a, int b) { ToolState ts; return Hw(c, ts).classify(a, b); }
int highwayHeight(SimContext& c, int a, int b) { ToolState ts; return Hw(c, ts).height(a, b); }

PathPreview previewHighway(SimContext& c, ToolState& ts, int a0, int b0, int a1, int b1) {   // 0x461200
    Hw h(c, ts);
    PathPreview out;
    h.begin(a0, b0, a1, b1);
    int a = a0, b = b0;
    ts.slopeTiles = 0; ts.length = 0;
    do {
        ts.length += 4;
        out.tiles.emplace_back(a, b);
    } while (h.step(a, b));
    ts.cost = 25 * ts.length;
    out.valid = true; out.cost = ts.cost; out.length = ts.length;
    ts.dir = 0;
    return out;
}

bool buildHighwayBridge(SimContext& c, ToolState& ts, int pa, int pb, int flag) {   // 0x45BB30
    Hw h(c, ts);
    World& w = c.w;
    constexpr int kAllowed[16] = {0, 6, 0xC, 4, 9, 0, 8, 0xC, 3, 2, 0, 6, 1, 3, 9, 0};
    const int a0 = pa & ~1, b0 = pb & ~1;
    auto quadLow = [&](int a, int b) { return h.bld(a, b) <= 0xD && h.bld(a, b + 1) <= 0xD && h.bld(a + 1, b + 1) <= 0xD && h.bld(a + 1, b) <= 0xD; };
    auto classMask = [&](int a, int b) {   // 16-bit sum of terrain classes of the four tiles, doubled per tile as in the original
        const uint16_t v = static_cast<uint16_t>(((h.terrWeight(h.ter(a, b + 1)) * 2 + h.terrWeight(h.ter(a + 1, b + 1))) * 2 + h.terrWeight(h.ter(a + 1, b))) * 2 + h.terrWeight(h.ter(a, b)));
        return v;
    };
    if (!quadLow(a0, b0)) { if (flag) c.hooks.toolMessage(0xFF); return false; }
    int dir;
    if (flag == 0) dir = ts.dir;
    else {
        uint16_t m = classMask(a0, b0);
        if (m & 0xF00) { c.hooks.toolMessage(0x104); return false; }
        if (static_cast<uint8_t>(m >> 8) != 0) m = static_cast<uint16_t>(m >> 12);
        const int avail = kAllowed[m & 0xF];
        const int r = c.s.rotation & 3;
        if (avail & (1 << r)) dir = r;
        else if (avail & (1 << ((r + 2) & 3))) dir = (r + 2) & 3;
        else if (avail & (1 << ((r + 1) & 3))) dir = (r + 1) & 3;
        else if (avail & (1 << ((r - 1) & 3))) dir = (r - 1) & 3;
        else return false;
    }
    int len = 0, sa = a0, sb = b0;
    for (;;) {
        sa += h.bdx(dir) * 2; sb += h.bdy(dir) * 2;
        ++len;
        if (sa < 0 || sa > 0x7F || sb < 0 || sb > 0x7F) { if (flag) c.hooks.toolMessage(0x105); return false; }
        if (!quadLow(sa, sb)) { if (flag) c.hooks.toolMessage(0x106); return false; }
        const uint16_t m = classMask(sa, sb);
        if (m & 0xF00) { if (flag) c.hooks.toolMessage(0x103); return false; }
        if (static_cast<uint8_t>(m) == 0) break;
    }
    int mask = 0x20;
    if (len > 2) {
        const int hh = h.height(a0, b0);
        const int ba = a0 - h.bdx(dir) * 2, bb = b0 - h.bdy(dir) * 2;
        if (h.bld(ba, bb) < 0xE && h.bld(ba, bb + 1) < 0xE && h.bld(ba + 1, bb + 1) < 0xE && h.bld(ba + 1, bb) < 0xE) {
            const int h1 = h.height(ba, bb);
            const int cl = h.classify(ba, bb);
            if (hh <= h1 && hh + 1 >= h1 && (hh != h1 || (cl & (1 << dir)) != 0)) {
                const int ea = a0 + h.bdx(dir) * len * 2, eb = b0 + h.bdy(dir) * len * 2;
                if (h.bld(ea, eb) < 0xE && h.bld(ea, eb + 1) < 0xE && h.bld(ea + 1, eb + 1) < 0xE && h.bld(ea + 1, eb) < 0xE) {
                    const int h2 = h.height(ea, eb);
                    const int cl2 = h.classify(ea, eb);
                    if (hh <= h2 && h2 <= hh + 1 && (hh != h2 || (cl2 & (1 << ((dir + 2) & 3))) != 0)) mask = 0x60;
                }
            }
        }
    }
    const int type = c.hooks.chooseBridge(len, mask);
    if (type < 0) return false;
    int32_t perTile;
    std::memcpy(&perTile, &c.t.bridgeData[4 * static_cast<size_t>(type)], 4);
    const int cost = perTile * len;
    if (static_cast<uint32_t>(c.s.treasury) <= static_cast<uint32_t>(cost) && cost - c.s.treasury != 0) { c.hooks.toolMessage(0x102); return false; }
    if (type == 6) {
        const int hh = h.height(a0, b0);
        const int ba = a0 - h.bdx(dir) * 2, bb = b0 - h.bdy(dir) * 2;
        const int hb = h.height(ba, bb);
        h.place(ba, bb, hh - hb == -1 ? (dir & 1) : ((dir + 1) & 3) + 4);
        const int ea = a0 + h.bdx(dir) * len * 2, eb = b0 + h.bdy(dir) * len * 2;
        const int he = h.height(ea, eb);
        h.place(ea, eb, hh - he == -1 ? (dir & 1) : ((dir - 1) & 3) + 4);
    }
    int a = a0, b = b0;
    for (int i = 0; i < len; ++i) {
        if (type == 5) {
            const int id = (dir & 1) + 0x49;
            const int xs[4] = {a, a, a + 1, a + 1}, ys[4] = {b, b + 1, b + 1, b};
            for (int k = 0; k < 4; ++k) {
                setTile(c, xs[k], ys[k], id);
                if (inMap(xs[k], ys[k])) w.xzon.at(xs[k], ys[k]) |= 0xF0;
            }
        }
        if (type == 6) {
            h.place(a, b, (i & 1) == 0 ? 0xE : 0xD);
            const int xs[4] = {a, a, a + 1, a + 1}, ys[4] = {b, b + 1, b + 1, b};
            for (int k = 0; k < 4; ++k) {
                if (!inMap(xs[k], ys[k])) continue;
                if ((dir & 1) == 0) w.xbit.at(xs[k], ys[k]) &= static_cast<uint8_t>(~kFlipped);
                else w.xbit.at(xs[k], ys[k]) |= kFlipped;
            }
        }
        a += h.bdx(dir) * 2; b += h.bdy(dir) * 2;
    }
    return true;
}

bool buildHighway(SimContext& c, ToolState& ts, int a0, int b0, int a1, int b1) {   // 0x461B80
    Hw h(c, ts);
    World& w = c.w;
    const int ua = a0 & ~1, ub = b0 & ~1;
    if (h.quadWater(ua, ub)) return buildHighwayBridge(c, ts, ua, ub, 1);
    const PathPreview pv = previewHighway(c, ts, ua, ub, a1, b1);
    if (!pv.valid || pv.cost == 0 || c.s.treasury < pv.cost) return false;
    c.s.treasury -= pv.cost;
    h.begin(ua, ub, a1, b1);
    int a = ua, b = ub, ea = ua, eb = ub;
    while (h.step(ea, eb)) { h.apply(a, b); a = ea; b = eb; }
    h.apply(a, b);
    const int na = a + h.dx2(ts.dir) * 2, nb = b + h.dy2(ts.dir) * 2;
    if (na + 1 >= 0 && na + 1 < 0x80 && nb + 1 >= 0 && nb + 1 < 0x80 && h.quadWater(na, nb)) return buildHighwayBridge(c, ts, na, nb, 0);
    const bool outside = na < 0 || na > 0x7F || nb < 0 || nb > 0x7F;
    if ((outside || (ua == a && ub == b && (ua == 0 || ub == 0 || ua == 0x7E || ub == 0x7E))) && w.xtxt.at(ua, ub) != 0xFA /* the original tests the START tile */ && c.s.treasury > 0x5DB &&
        c.hooks.confirm(0xF6, 1500)) {
        c.s.treasury -= 0x5DC;
        w.xtxt.at(a, b) = 0xFA;
        ++c.s.c9408;
    }
    ts.dir = 0;
    return true;
}

}  // namespace sc2k
