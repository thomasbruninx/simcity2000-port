#include "simulation/terrain_gen.h"

#include <vector>

#include "simulation/tools.h"

namespace sc2k {

namespace {

struct Gen {
    SimContext& c;
    World& w;
    bool coast, river;
    int edge;   // 0x4CA3F0: altitude outside the map (hills + 10)
    int rnd() { return c.rng.next(); }
    uint16_t& alt(int a, int b) { return w.altm.at(a, b); }
    int get(int a, int b) {   // 0x447300
        if (a < 0) return edge;
        if (a > 0x7F) return coast ? 0 : edge;
        if (b < 0 || b > 0x7F) return edge;
        return static_cast<int16_t>(alt(a, b));
    }
    static int sar(int v, int n) { return v >> n; }

    void hillPass(int step, int mask) {   // 0x4470E0
        for (int a = 0; a < 0x80; a += step) {
            const bool aBit = (mask & a) != 0;
            for (int b = 0; b < 0x80; b += step) {
                const bool bBit = (mask & b) != 0;
                if (!aBit && !bBit) continue;
                const int r = static_cast<int>(static_cast<uint32_t>(rnd() & 0xFFFF) % static_cast<uint32_t>(step));
                int v;
                if (bBit) {
                    if (aBit) {
                        const int s1 = get(a - step, step + b), s3 = get(step + a, step + b), s4 = get(a - step, b - step), s2 = get(step + a, b - step);
                        v = sar(s2 + s1 + s3 + s4, 2) + r;
                    } else {
                        const int s1 = get(a, step + b), s2 = get(a, b - step);
                        v = r + sar(s1 + s2, 1);
                    }
                } else {
                    const int s1 = get(a - step, b), s2 = get(step + a, b);
                    v = r + sar(s1 + s2, 1);
                }
                if (v < 1) v = 1;
                alt(a, b) = static_cast<uint16_t>(v);
            }
        }
    }
    void hills(int h) {   // 0x447040
        for (int a = 0; a < 8; ++a)
            for (int b = 0; b < 8; ++b)
                alt(a * 16, b * 16) = static_cast<uint16_t>(static_cast<int>(static_cast<uint32_t>(rnd() & 0xFFFF) % static_cast<uint32_t>(h)) + 1);
        hillPass(8, 0xF); hillPass(4, 7); hillPass(2, 3); hillPass(1, 1);
    }
    void coastLine(int sea) {   // 0x4474C0
        int u = c.lcg.mod(10) + 10;
        for (int b = 0; b < 0x80; ++b) {
            int a = 0x80 - u;
            if (inMap(a, b)) alt(a, b) = static_cast<uint16_t>((alt(a, b) & 0xFFE0) | static_cast<uint16_t>(sea - 2));
            for (a = 0x81 - u; a < 0x80; ++a) {
                if (a < 0) continue;
                if (inMap(a, b)) alt(a, b) = static_cast<uint16_t>((alt(a, b) & 0xFFE0) | static_cast<uint16_t>(sea - 3));
                if (inMap(a, b)) w.xbit.at(a, b) |= 1;
            }
            const int target = c.lcg.mod(0x1E);
            int next = u;
            if (u < target) { ++u; next = u; }
            if (target < u) next = static_cast<int16_t>(next - 1);
            u = static_cast<uint16_t>(next);
        }
    }
    void riverLine(int sea) {   // 0x447610
        int col = 0x40, row = 0x7F;
        int drift = c.lcg.mod(3) - 1;
        do {
            for (int d = col - 3; d <= col + 3; ++d)
                if (inMap(row, d)) alt(row, d) = static_cast<uint16_t>((alt(row, d) & 0xFFE0) | static_cast<uint16_t>(sea - 3));
            if (inMap(row, col - 4)) alt(row, col - 4) = static_cast<uint16_t>((alt(row, col - 4) & 0xFFE0) | static_cast<uint16_t>(sea - 2));
            if (inMap(row, col + 4)) alt(row, col + 4) = static_cast<uint16_t>((alt(row, col + 4) & 0xFFE0) | static_cast<uint16_t>(sea - 2));
            if (c.lcg.mod(8) == 0) drift = c.lcg.mod(3) - 1;
            col += drift + c.lcg.mod(3) - 1;
            if (col < 5) col = 5;
            if (col > 0x7A) col = 0x7A;
            --row;
        } while (row >= 0);
    }
    void smooth() {   // 0x447380 (Jacobi step on a copy)
        std::vector<int16_t> t(128 * 128);
        for (int a = 0; a < 128; ++a) for (int b = 0; b < 128; ++b) t[static_cast<size_t>(a * 128 + b)] = static_cast<int16_t>(alt(a, b));
        auto at = [&](int a, int b) { return t[static_cast<size_t>(a * 128 + b)]; };
        for (int a = 0; a < 128; ++a)
            for (int b = 0; b < 128; ++b) {
                const int16_t cur = at(a, b);
                const int16_t up = a > 0 ? at(a - 1, b) : cur, left = b > 0 ? at(a, b - 1) : cur, down = a < 0x7F ? at(a + 1, b) : cur, right = b < 0x7F ? at(a, b + 1) : cur;
                const int16_t sum = static_cast<int16_t>(up + left + down + right);
                alt(a, b) = static_cast<uint16_t>(static_cast<int16_t>(((sum >> 2) + cur) >> 1));
            }
    }
    void cliff(int a, int b) {   // 0x446830: no tile may stand more than one level above a neighbour
        static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
        const int h = alt(a, b) & 0x1F;
        bool lower = false;
        for (int i = 0; i < 4; ++i) {
            const int na = a + dx[i], nb = b + dy[i];
            if (inMap(na, nb) && (alt(na, nb) & 0x1F) + 1 < h) { lower = true; break; }
        }
        if (!lower) return;
        const int nh = h - 1;
        alt(a, b) = static_cast<uint16_t>((alt(a, b) & 0xFFE0) | static_cast<uint16_t>(nh));
        for (int i = 0; i < 4; ++i) {
            const int na = a + dx[i], nb = b + dy[i];
            if (inMap(na, nb) && nh < static_cast<int16_t>(alt(na, nb) & 0x1F)) cliff(na, nb);
        }
    }
    void forests(int n) {   // 0x446EA0
        for (; n != 0; --n) {
            const int r1 = rnd(), r2 = rnd(), r3 = rnd();
            int count = r3 & 0x3F;
            if (count == 0) continue;
            do {
                --count;
                const int r5 = rnd(), r6 = rnd(), r7 = rnd();
                const int a = (static_cast<int>(static_cast<uint32_t>(r5 & 0xFFFF) % 5u) - static_cast<int>(static_cast<uint32_t>(r6 & 0xFFFF) % 5u)) + (r1 & 0x7F);
                const int r8 = rnd();
                const int b = (static_cast<int>(static_cast<uint32_t>(r7 & 0xFFFF) % 5u) - static_cast<int>(static_cast<uint32_t>(r8 & 0xFFFF) % 5u)) + (r2 & 0x7F);
                if (!inMap(a, b) || (w.xbit.at(a, b) & xbit::kWater)) continue;
                const int id = w.xbld.at(a, b);
                int nid;
                if (id < 6) nid = (rnd() & 1) + 6;
                else if (id < 0xB) nid = id + 1;
                else if (id > 0xC) continue;
                else nid = (rnd() & 1) + 0xB;
                setTile(c, a, b, nid);
            } while (count != 0);
        }
    }
    void coastFlags() {   // 0x4477C0
        for (int pass = 4; pass > 0; --pass)
            for (int b = 1; b < 0x80; ++b)
                for (int a = 1; a < 0x80; ++a) {
                    const int f = w.xbit.at(a, b) & 5;
                    if (f == 1) w.xbit.at(a, b) &= 0xFE;
                    if (f == 5) {
                        if (b - 1 >= 0) w.xbit.at(a, b - 1) |= 1;
                        if (a - 1 >= 0) w.xbit.at(a - 1, b) |= 1;
                    }
                }
    }
};

}  // namespace

void generateTerrain(SimContext& c, const TerrainParams& p) {
    World& w = c.w;
    Gen g{c, w, p.coast, p.river, p.hills + 10};
    for (int a = 0; a < 0x80; ++a)
        for (int b = 0; b < 0x80; ++b) {
            w.altm.at(a, b) = 0;
            w.xter.at(a, b) = 0;
            setTile(c, a, b, 0);
            w.xbit.at(a, b) &= 0xFB;
        }
    c.s.rotation = 0;
    g.hills(p.hills + 0xB);
    int sea = (p.water + 4 + ((p.water + 4) >> 31 & 7)) >> 3;
    c.s.seaLevel = sea;
    if (p.coast || p.river) {
        if (c.s.seaLevel < 4) c.s.seaLevel = 4;
        if (p.coast) g.coastLine(c.s.seaLevel);
        if (p.river) g.riverLine(c.s.seaLevel);
    }
    g.smooth();
    g.smooth();
    for (int a = 0; a < 0x80; ++a)
        for (int b = 0; b < 0x80; ++b) {
            const int16_t v = static_cast<int16_t>(g.alt(a, b));
            const int16_t t = static_cast<int16_t>((v + 3) >> 1);
            const int16_t u = static_cast<int16_t>(t - 4);
            int16_t r = u;
            if (u < 4) { r = t; if (u >= 0) r = 4; }
            g.alt(a, b) = static_cast<uint16_t>(r);
        }
    g.smooth();
    for (int a = 0; a < 0x80; ++a)
        for (int b = 0; b < 0x80; ++b) { g.alt(a, b) &= 0xFC1F; g.alt(a, b) &= 0x3FF; }
    for (int a = 0; a < 0x80; ++a) for (int b = 0; b < 0x80; ++b) g.cliff(a, b);
    for (int a = 0; a < 0x80; ++a) for (int b = 0; b < 0x80; ++b) fixShore(c, a, b);
    g.forests((p.trees * p.trees) / 2);
    if (p.coast) g.coastFlags();
    ToolState ts;
    for (int n = p.water / 4; n > 0; --n) {
        const int len = (c.rng.next() & 0x7F) + 0x32;
        const int b = c.rng.next() & 0x7F;
        const int a = c.rng.next() & 0x7F;
        placeStream(c, ts, a, b, len);
    }
}

}  // namespace sc2k
