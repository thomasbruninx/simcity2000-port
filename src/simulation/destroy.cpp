// Destroy effects: literal port of SIMCITY.EXE build .022 functions 0x458690 (demolish a tile/building/bridge/tunnel with
// animation), 0x45AB10 (footprint), 0x4635D0 (highway/bridge segment classifier), 0x44C850 and 0x4469C0 (shoreline fixer).
// All drawing (dirty rectangles, sprite blits, sound, sleeps) is presentation and is omitted, but every rand() call made by
// the animation code is reproduced because it advances the shared random stream. docs/simulation/destroy-effects.md
#include "simulation/sim_context.h"

namespace sc2k {

using namespace xbit;

namespace {
int s16(int v) { return static_cast<int16_t>(v); }
bool onMap(int a, int b) { return inMap(a, b); }
}  // namespace

int footprintOf(const SimContext& c, int& x, int& y, int id) {
    const World& w = c.w;
    if ((id > 0x60 && id < 0x6C) || (id > 0x48 && id < 0x51)) {
        x = (x & ~1);            // original: *x &= 0xFE on the low byte
        y = (y & ~1) + 1;
        return 2;
    }
    if (id < 0x70) return 1;
    const int size = c.t.footprint[static_cast<size_t>(id)];
    if (size != 2) {
        if (size < 3 || size > 4) return size;
        const uint8_t* corner = &c.t.rotationCorner[static_cast<size_t>(c.s.rotation & 3) * 8];
        auto zhi = [&](int a, int b) { return w.xzon.at(s16(a) & 0x7F, s16(b) & 0x7F) & 0xF0; };   // neighbours stay on the map for anchored buildings
        int u = corner[0];
        if (zhi(x - 1, y - 1) == u) { x -= 1; y -= 1; }
        if (zhi(x, y - 1) == u) y -= 1;
        if (zhi(x - 1, y) == u) x -= 1;
        u = corner[2];
        if (zhi(x + 1, y - 1) == u) { x += 1; y -= 1; }
        if (zhi(x, y - 1) == u) y -= 1;
        if (zhi(x + 1, y) == u) x += 1;
        u = corner[4];
        if (zhi(x + 1, y + 1) == u) { x += 1; y += 1; }
        if (zhi(x, y + 1) == u) y += 1;
        if (zhi(x + 1, y) == u) x += 1;
        u = corner[6];
        if (zhi(x - 1, y + 1) == u) { x -= 1; y += 1; }
        if (zhi(x, y + 1) == u) y += 1;
        if (zhi(x - 1, y) == u) x -= 1;
    }
    const uint8_t* corner = &c.t.rotationCorner[static_cast<size_t>(c.s.rotation & 3) * 8];
    const int z = w.xzon.at(x, y) & 0xF0;
    if (corner[0] == z) { y = y + size - 1; return size; }
    if (corner[2] != z) {
        if (corner[4] == z) x = x - size + 1;
        return size;
    }
    y = y + size - 1;
    x = x + 1 - size;
    return size;
}

int bridgeSegment(const SimContext& c, int a, int b) {
    const World& w = c.w;
    if (!onMap(a, b)) return 0;
    int id = w.xbld.at(a, b);
    if (!((id > 0x60 && id < 0x6C) || (id > 0x48 && id < 0x51))) return static_cast<int16_t>(0xFFFF);
    auto water = [&](int x, int y) { return onMap(x, y) && (w.xbit.at(x, y) & kWater); };
    if ((w.xzon.at(a, b) & 0xF0) == 0xF0) {
        if (id > 0x4A && id < 0x51) return id & 1;
        if (water(a, b)) return 0xE - (id == 0x49);
        int idn = w.xbld.at(s16(a + 1) & 0x7F, b);
        if (idn > 0x4A && idn < 0x51) return idn & 1;
        if (water(a + 1, b)) return 0xE - (idn == 0x49);
        idn = w.xbld.at(s16(a + 1) & 0x7F, s16(b + 1) & 0x7F);
        if (idn > 0x4A && idn < 0x51) return idn & 1;
        if (water(a + 1, b + 1)) return 0xE - (idn == 0x49);
        idn = w.xbld.at(a, s16(b + 1) & 0x7F);
        if (idn > 0x4A && idn < 0x51) return idn & 1;
        if (water(a, b + 1)) return 0xE - (idn == 0x49);
        if (idn > 0x48 && idn < 0x4B) return (idn & 1) + 2;
        return static_cast<int16_t>(0xFFFF);
    }
    const int d = id - 0x5D;
    if (d > 0xC) {
        if (onMap(a, b + 1)) return 0x10 - ((w.xbit.at(a, b + 1) & 2) == 0);
        return 0xF;
    }
    return d;
}

void releaseTextOverlay(SimContext& c, int t) {
    if (t == 0) return;
    if (t < 0xC9) {
        if (t < 0x33) {
            if (t > 0) c.w.xlab[static_cast<size_t>(t) * 25] = 0;
        } else if (t > 0x3C) {
            c.w.xmic[static_cast<size_t>(t - 0x33) * 8] = 0;
            c.w.xlab[static_cast<size_t>(t) * 25] = 0;
        }
    } else if (t < 0xF1) {
        c.w.xthg[static_cast<size_t>(t - 0xC9) * 12 + 10] = 0;
    }
}

void fixShore(SimContext& c, int a, int b) {
    World& w = c.w;
    if (!onMap(a, b)) return;
    const int id = w.xbld.at(a, b);
    if (id > 0xC) destroyTile(c, a, b, 1);
    if (c.s.mode != 0 && id != 5) setTile(c, a, b, 0);
    if (w.xund.at(a, b) != 0) setUnderground(c, a, b, 0);
    int mask = 0;
    for (int i = 0; i < 8; ++i) {
        const int x = a + c.t.shoreDx[static_cast<size_t>(i)], y = b + c.t.shoreDy[static_cast<size_t>(i)];
        if (onMap(x, y) && (w.altm.at(a, b) & 0x1F) < (w.altm.at(x, y) & 0x1F)) mask |= c.t.shoreBit[static_cast<size_t>(i)];
    }
    const int8_t type = static_cast<int8_t>(c.t.shoreType[static_cast<size_t>(mask) & 0xFF]);
    if (type != 0) w.xzon.at(a, b) &= 0xF0;
    const int sea = static_cast<int16_t>(c.s.seaLevel);
    if (type != '2') {
        const int alt = w.altm.at(a, b) & 0x1F;
        if (sea <= alt) {
            w.xbit.at(a, b) &= 0xFB;
            w.xter.at(a, b) = static_cast<uint8_t>(type);
            return;
        }
        w.xbit.at(a, b) |= 4;
        w.altm.at(a, b) = static_cast<uint16_t>((w.altm.at(a, b) & 0xFC1F) | (sea << 5));
        if (w.xbld.at(a, b) != 5) setTile(c, a, b, 0);
        w.xter.at(a, b) = static_cast<uint8_t>((sea - alt) == 1 ? type + 0x20 : type + 0x10);
        return;
    }
    const uint16_t old = w.altm.at(a, b);
    const uint16_t alt = static_cast<uint16_t>((old & 0x1F) + 1);
    w.altm.at(a, b) = static_cast<uint16_t>((old & 0xFFE0) | alt);
    if (sea <= static_cast<int16_t>(alt)) {
        w.xbit.at(a, b) &= 0xFB;
        w.xter.at(a, b) = 0;
        return;
    }
    w.xbit.at(a, b) |= 4;
    w.altm.at(a, b) = static_cast<uint16_t>((sea << 5) | (w.altm.at(a, b) & 0xFC1F));
    if (w.xbld.at(a, b) != 5) setTile(c, a, b, 0);
    w.xter.at(a, b) = 0x10;
}

namespace {

// zero a tile the way the destroy code does after SetTile(…, id): clear the corner flags and the powered/piped bits
void wipeTile(SimContext& c, int x, int y, int newId, bool clearFlippedOnly) {
    setTile(c, x, y, newId);
    if (onMap(x, y)) {
        c.w.xzon.at(x, y) &= 0x0F;
        c.w.xbit.at(x, y) &= clearFlippedOnly ? 0xFD : 0x3D;
    }
}

// the two rand() calls of one animated explosion sprite (sprite id, flip)
void animSprite(SimContext& c) { c.rng.next(); c.rng.next(); }

// flood demolition of connected runway / taxiway pieces (ids lo, hi); `rubble` selects (rand&3)+1 instead of 0
void floodDemolish(SimContext& c, int a, int b, int idA, int idB, bool rubble, int animate) {
    World& w = c.w;
    c.queue.reset(a, b);
    auto size = [&] { return c.queue.size(); };
    while (size() != 0) {
        const auto p = c.queue.popBack();
        const int x = p.a, y = p.b;
        const int newId = rubble ? (c.rng.next() & 3) + 1 : 0;
        setTile(c, x, y, newId);
        if (onMap(x, y)) { w.xzon.at(x, y) &= 0x0F; w.xbit.at(x, y) &= 0x3D; }
        if (animate) animSprite(c);
        auto matches = [&](int nx, int ny) { const int v = w.xbld.at(nx, ny); return v != 0 && (v == idA || v == idB); };
        if (x > 0 && matches(x - 1, y)) c.queue.push(x - 1, y);
        if (x < 0x7F && matches(x + 1, y)) c.queue.push(x + 1, y);
        if (y > 0 && matches(x, y - 1)) c.queue.push(x, y - 1);
        if (x < 0x7F && matches(x, y + 1)) c.queue.push(x, y + 1);   // original bounds check uses x here (kept)
    }
}

}  // namespace

void destroyTile(SimContext& c, int a, int b, int animate) {
    World& w = c.w;
    const int id = w.xbld.at(a, b);
    if (id < 6) return;
    int x = a, y = b;
    const int size = footprintOf(c, x, y, id);

    int seg = 0;
    const bool bridgePath = (size == 1 && isBridge(id)) || (size == 2 && (seg = bridgeSegment(c, x, y - 1)) > 0xC);
    if (bridgePath) {
        if (size == 2) y -= 1;
        int stepX, stepY;
        if ((size == 1 && onMap(x, y) && (w.xbit.at(x, y) & 2)) || (size == 2 && (seg & 1) == 0)) { stepX = 1; stepY = 0; }
        else { stepX = 0; stepY = 1; }
        // walk back to the start of the span
        bool endedAtNonBridge = false;
        for (;;) {
            if (size != 2 || s16(bridgeSegment(c, x, y)) < 0xD) {
                if (size != 1) break;                       // size 2: reached the first piece
                if (!isBridge(w.xbld.at(x, y))) { endedAtNonBridge = true; break; }
            }
            x -= size * stepX;
            y -= size * stepY;
        }
        auto abutment = [&](bool clearFlipped) {
            if (!(onMap(x, y) && (w.xbit.at(x, y) & 4))) {
                setTile(c, x, y, 0);
                w.altm.at(x, y) = static_cast<uint16_t>((w.altm.at(x, y) & 0xFFE0) | ((w.altm.at(x, y) & 0x1F) - 1));
                w.xbit.at(x, y) |= 4;
                fixShore(c, x, y);
                if (clearFlipped) w.xbit.at(x, y) &= 0xFD;
            }
        };
        auto bigPieceFix = [&] {
            if (size == 2 && id != 0x49 && id != 0x4A) {
                fixShore(c, x, y); fixShore(c, x + 1, y); fixShore(c, x + 1, y + 1); fixShore(c, x, y + 1);
            }
        };
        if (endedAtNonBridge) abutment(true);
        bigPieceFix();
        const int dx = size * stepX, dy = size * stepY;
        x += dx; y += dy;
        // remove the span piece by piece
        int segHere = 0;
        for (;;) {
            if (size == 2) {
                segHere = s16(bridgeSegment(c, x, y));
                if (segHere >= 0xD) goto removePiece;
            }
            if (size != 1) break;
            if (!isBridge(w.xbld.at(x, y))) break;
        removePiece:
            if (animate) animSprite(c);
            wipeTile(c, x, y, 0, true);
            if (size == 2) {
                if (animate) { c.rng.next(); c.rng.next(); c.rng.next(); }
                wipeTile(c, x, y + 1, 0, true);
                wipeTile(c, x + 1, y + 1, 0, true);
                wipeTile(c, x + 1, y, 0, true);
            }
            x += dx; y += dy;
        }
        if (size == 1) {
            if (!(onMap(x, y) && (w.xbit.at(x, y) & 4))) {
                setTile(c, x, y, 0);
                w.altm.at(x, y) = static_cast<uint16_t>((w.altm.at(x, y) & 0xFFE0) | ((w.altm.at(x, y) & 0x1F) - 1));
                w.xbit.at(x, y) |= 4;
                fixShore(c, x, y);
            }
        }
        bigPieceFix();
        return;
    }

    if (id == 0xDF || id == 0xE0) { floodDemolish(c, a, b, 0xDF, 0xE0, false, animate); return; }
    if (id == 0xDD || id == 0xDE) { floodDemolish(c, a, b, 0xDD, 0xDE, true, animate); return; }

    if (id < 0x3F || id > 0x42) {
        // ordinary building
        if (animate && size > 0)
            for (int frame = 0; frame < size; ++frame)
                for (int i = 0; i < size; ++i)
                    for (int j = 0; j < size; ++j) animSprite(c);
        if (id == 0xF3) c.hooks.uiFlag(0);
        if (id == 0xD0) c.hooks.uiFlag(1);
        if (id == 0xDB) c.hooks.uiFlag(2);
        if (id == 0xFF) c.hooks.uiFlag(3);
        if (id == 0xD7) {
            const int t = w.xtxt.at(x, y);
            if (t > 0x32 && t < 0xC9 && w.xmic[static_cast<size_t>(t - 0x33) * 8] == 0xD7)
                c.s.militaryMask = static_cast<int16_t>(c.s.militaryMask + (-1 << (w.xmic[static_cast<size_t>(t - 0x33) * 8 + 4] & 0x1F)));
        }
        for (int i = 0; i < size; ++i)
            for (int j = 0; j < size; ++j) {
                const int X = x + i, Y = y - j;
                if (X < 0 || X >= 0x80 || Y < 0 || Y >= 0x80) continue;
                const int rubble = (w.xter.at(X, Y) == 0) ? (c.rng.next() & 3) + 1 : 0;
                setTile(c, X, Y, rubble);
                w.xbit.at(X, Y) &= 0x3D;
                w.xzon.at(X, Y) &= 0x0F;
                const int t = w.xtxt.at(X, Y);
                if (t != 0 && (t < 0xF1 || t == 0xFA)) {
                    if (t < 0xC9 || t == 0xFA) w.xtxt.at(X, Y) = 0;
                    releaseTextOverlay(c, t);
                    if (t == 0xFA) {
                        if (!isRoad(id)) c.s.c9408 = static_cast<uint16_t>(c.s.c9408 - 1);
                        else c.s.cb4f0 = static_cast<uint16_t>(c.s.cb4f0 - 1);
                    }
                }
            }
        if (isHighway(id)) { fixShore(c, x, y); fixShore(c, x + 1, y); fixShore(c, x + 1, y - 1); fixShore(c, x, y - 1); }
        if (size == 1 && id < 0x70 && w.xter.at(x, y) != 0) fixShore(c, x, y);
        return;
    }

    // tunnel mouth (ids 0x3F..0x42): remove the whole tunnel up to the other mouth
    int dx = 0, dy = 0;
    switch (id) { case 0x3F: dx = -1; dy = 0; break; case 0x40: dx = 0; dy = -1; break; case 0x41: dx = 1; dy = 0; break; case 0x42: dx = 0; dy = 1; break; }
    int ta = a, tb = b, cur = 0;
    setTile(c, ta, tb, 0);
    if (onMap(ta, tb)) { w.xzon.at(ta, tb) &= 0x0F; w.altm.at(ta, tb) &= 0x3FF; }
    animSprite(c);                         // drawn unconditionally in this branch
    while (cur < 0x3F || cur > 0x42) {
        ta += dx; tb += dy;
        if (onMap(ta, tb)) w.altm.at(ta, tb) &= 0x3FF;
        cur = w.xbld.at(ta, tb);
    }
    setTile(c, ta, tb, 0);
    if (onMap(ta, tb)) w.xzon.at(ta, tb) &= 0x0F;
    animSprite(c);
}

}  // namespace sc2k
