// Disasters: the handlers behind SIMCITY.EXE's DisasterTick 0x45C870 (build .022) and the per-tick driver 0x45F0D0 that spreads fire, flood
// and crash markers. Drawing, the camera and the message boxes are UI hooks. See docs/simulation/disasters.md.
//
// A disaster leaves markers in XTXT: 0xFF flames, 0xFB spreading fire, 0xFC flood water, 0xFD / 0xFE a burning vehicle on a road (the pair
// moves toward -x/-y or +x/+y); things (slot + 0xC9) are monsters, tornados, crashing planes and emergency units.
#include "simulation/sim_context.h"
#include "simulation/tools.h"

namespace sc2k {

namespace {

constexpr int kDx4[4] = {0, 1, 0, -1}, kDy4[4] = {-1, 0, 1, 0};    // 0x4E77C8.. (all the four-way tables are identical)
constexpr int kDx8[8] = {0, 1, 1, 1, 0, -1, -1, -1}, kDy8[8] = {-1, -1, 0, 1, 1, 1, 0, -1};   // 0x4E77F8 / 0x4E7808

struct D {
    SimContext& c;
    World& w;
    explicit D(SimContext& ctx) : c(ctx), w(ctx.w) {}
    int rnd() { return c.rng.next(); }
    int rmod(int n) { return static_cast<int>(static_cast<uint32_t>(rnd() & 0xFFFF) % static_cast<uint32_t>(n)); }
    int lf(int mask) { return c.lfsr.mask(static_cast<uint16_t>(mask)); }
    bool in(int a, int b) const { return inMap(a, b); }
    bool water(int a, int b) const { return in(a, b) && (w.xbit.at(a, b) & xbit::kWater); }
    int bld(int a, int b) const { return w.xbld.at(a, b); }
    uint8_t& txt(int a, int b) { return w.xtxt.at(a, b); }
    int alt(int a, int b) const { return w.altm.at(a, b) & 0x1F; }
    void focus(int a, int b) { c.things.focusA = a; c.things.focusB = b; }
    uint8_t& thing(int slot, int i) { return w.xthg[static_cast<size_t>(slot) * 12 + static_cast<size_t>(i)]; }
    // spiral walk used by several handlers: step, then count; the arm length grows after every second turn
    struct Spiral {
        int dir = 0, len = 1, count = 0;
        void advance() { if (len <= ++count) { count = 0; if (dir & 1) ++len; dir = (dir + 1) & 3; } }
    };
};

bool roadClass(int id) { return (id > 0x1C && id < 0x2C) || (id > 0x3E && id < 0x47) || id == 0x4B || id == 0x4C || (id > 0x5C && id < 0x61); }
bool flameClass(int id) { return (id != 0 && id < 5) || roadClass(id); }   // tiles a burning vehicle can move onto

// 0x460CA0: demolish what stands on the tile and (ignite) mark its footprint with flames; remembers the most important kind for the final message
void demolishAndIgnite(SimContext& c, int a, int b, int animate, int ignite) {
    D d(c);
    const int id = d.bld(a, b);
    if (!(id < 0x3F || id > 0x42)) return;
    int x = a, y = b;
    const int size = footprintOf(c, x, y, id);
    destroyTile(c, a, b, animate);
    for (int i = 0; i < size; ++i)
        for (int j = 0; j < size; ++j) {
            const int px = x + i, py = y - j;
            if (d.in(px, py) && !d.water(px, py) && ignite != 0) d.txt(px, py) = 0xFF;
        }
    int k = d.w.xzon.at(a, b) & 0xF;
    if (id >= 0xC6) { k = 10; if (id != 0xC6) k = id - 0xBD; }
    const auto& pr = c.t.messagePriority;
    if (c.things.fireMessageKind == -1 || pr[static_cast<size_t>(c.things.fireMessageKind)] < pr[static_cast<size_t>(k)]) c.things.fireMessageKind = k;
}

void markFire(SimContext& c, int a, int b, int value) {   // 0x460B00 (0xFB) / 0x460AB0: only on tiles without an overlay object
    D d(c);
    if (d.in(a, b) && d.txt(a, b) < 0x33) d.txt(a, b) = static_cast<uint8_t>(value);
}

void burnOut(SimContext& c, int a, int b) {   // 0x460B50: flames turn into rubble
    D d(c);
    if (!d.in(a, b) || d.txt(a, b) != 0xFF) return;
    d.txt(a, b) = 0;
    const int id = d.bld(a, b);
    if (id < 0x3F || id > 0x42) {
        if (id < 0x61) {
            destroyTile(c, a, b, 0);
            setTile(c, a, b, c.lfsr.mod(4) + 1);
            return;
        }
        if ((d.w.xzon.at(a, b) & 0xF0) == 0xF0) destroyTile(c, a, b, 0);
    }
}

void clearVehicleFire(SimContext& c, int a, int b) {   // 0x460C40
    D d(c);
    if (d.in(a, b) && (d.txt(a, b) == 0xFE || d.txt(a, b) == 0xFD)) d.txt(a, b) = 0;
}

void floodSpread(SimContext& c, int a, int b, int level) {   // 0x460970: water flows onto a tile that is not higher than the source
    D d(c);
    if (!d.in(a, b)) return;
    if (d.alt(a, b) > level) return;
    const int ter = d.w.xter.at(a, b);
    if (!(ter < 0x10 || ter > 0x1F)) return;
    const int v = d.txt(a, b);
    if (v != 0) {
        if (v < 0x33) releaseTextOverlay(c, v);
        else {
            if (v > 200) {
                if (v < 0xF1) return;
                if (v > 0xF9) return;
                setTile(c, a, b, c.lfsr.mod(4) + 1);
                return;
            }
            demolishAndIgnite(c, a, b, 1, 0);
        }
    }
    d.txt(a, b) = 0xFC;
    d.w.xtrf.at(a / 2, b / 2) = 0;
}

// ---- handlers (one per disaster kind) ----

bool fireHandler(SimContext& c) {   // 0x45E290: a fire near the city centre
    D d(c);
    const CityState& s = c.s;
    int a = (s.centerA2 - 0x14) + d.rmod(0x28);
    int b = (s.centerB2 - 0x14) + d.rmod(0x28);
    D::Spiral sp; sp.len = 1;
    do {
        a = static_cast<int16_t>(a + kDx4[sp.dir]);
        b = static_cast<int16_t>(b + kDy4[sp.dir]);
        if (d.in(a, b) && d.bld(a, b) > 0x6F && startFire(c, a, b, 0)) { d.focus(a, b); return true; }
        sp.advance();
    } while (sp.len < 0x40);
    for (int i = 0; i < 200; ++i) {
        const int x = d.lf(0x7F), y = d.lf(0x7F);
        if (startFire(c, x, y, 0)) { d.focus(x, y); return true; }
    }
    c.hooks.showDialog(0xF5);
    return false;
}

bool floodHandler(SimContext& c) {   // 0x45D460: water starts to flow next to the nearest low-lying tile
    D d(c);
    const int fa = c.things.focusA, fb = c.things.focusB;
    for (int r = 0; r < 0x80; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            const int x = fa + dx;
            for (int dy = -r; dy <= r; ++dy) {
                const int y = fb + dy;
                if (!d.in(x, y)) continue;
                const int ter = d.w.xter.at(x, y);
                if (ter <= 0x1F || ter >= 0x30) continue;
                if (x - 1 >= 0 && x - 1 < 0x80 && 0 < dx && !d.water(x - 1, y)) d.txt(x - 1, y) = 0xFC;
                if (x < 0x80 && y - 1 >= 0 && y - 1 < 0x80 && 0 < dy && !d.water(x, y - 1)) d.txt(x, y - 1) = 0xFC;
                if (x + 1 < 0x80 && dx < 0x7F && !d.water(x + 1, y)) d.txt(x + 1, y) = 0xFC;
                if (x >= 0 && y + 1 < 0x80 && dy < 0x7F && !d.water(x, y + 1)) d.txt(x, y + 1) = 0xFC;
                c.things.floodTimer = 0x3C;
                c.hooks.playSound(0x1FF);
                d.focus(x, y);
                return true;
            }
        }
    }
    for (int tries = 0; tries < 200;) {
        const int x = d.lf(0x7F), y = d.lf(0x7F);
        if (!d.in(x, y)) continue;
        if (d.w.xter.at(x, y) == 0) {
            d.txt(x, y) = 0xFC;
            c.hooks.playSound(0x1FF);
            c.things.floodTimer = 0x3C;
            d.focus(x, y);
            return true;
        }
        ++tries;
    }
    return false;
}

bool vehicleCrash(SimContext& c) {   // 0x45DF50: a burning vehicle on the nearest free road tile
    D d(c);
    int a = c.things.focusA, b = c.things.focusB;
    D::Spiral sp;
    do {
        a = static_cast<int16_t>(a + kDx4[sp.dir]);
        b = static_cast<int16_t>(b + kDy4[sp.dir]);
        if (d.in(a, b) && roadClass(d.bld(a, b)) && !d.water(a, b) && d.txt(a, b) == 0) {
            d.txt(a, b) = static_cast<uint8_t>((d.rnd() & 1) - 3);
            c.hooks.playSound(0x200);
            d.focus(a, b);
            return true;
        }
        sp.advance();
    } while (sp.len < 0x80);
    return false;
}

bool meltdownHandler(SimContext& c) {   // 0x45DEF0: flames at the focus tile
    D d(c);
    if (d.in(c.things.focusA, c.things.focusB)) d.txt(c.things.focusA, c.things.focusB) = 0xFB;
    return true;
}

bool earthquakeHandler(SimContext& c) {   // 0x45E490 (the shaking animation is UI)
    D d(c);
    const int fa = c.things.focusA, fb = c.things.focusB;
    for (int dx = -0x20; dx <= 0x20; ++dx)
        for (int dy = -0x20; dy <= 0x20; ++dy) {
            if ((d.rnd() & 0x3F) != 0) continue;
            const int a = fa + dx, b = fb + dy;
            if (!d.in(a, b)) continue;
            if (d.bld(a, b) > 0xD) {
                if ((d.rnd() & 3) == 0) startFire(c, a, b, 0);
                else demolishAndIgnite(c, a, b, 0, 0);
            }
        }
    return true;
}

bool volcanoHandler(SimContext& c) {   // 0x45EC20: the volcano tile (0xCB) erupts
    D d(c);
    int a = c.things.focusA, b = c.things.focusB;
    if (!d.in(a, b) || d.bld(a, b) != 0xCB) {
        bool found = false;
        for (a = 0; a < 0x80 && !found; ++a)
            for (b = 0; b < 0x80; ++b)
                if (d.bld(a, b) == 0xCB) { found = true; break; }
        if (!found) return false;
        --a;   // the outer loop increments once more after the hit
    }
    footprintOf(c, a, b, 0xCB);
    a += 1; b -= 1;
    demolishAndIgnite(c, a, b, 1, 1);
    for (int dx = -0x20; dx <= 0x20; ++dx)
        for (int dy = -0x20; dy <= 0x20; ++dy) {
            if ((d.rnd() & 0x1F) != 0) continue;
            const int x = a + dx, y = b + dy;
            if (!d.in(x, y)) continue;
            if ((d.rnd() & 3) == 0) startFire(c, x, y, 1);
            else {
                demolishAndIgnite(c, x, y, 0, 0);
                if ((d.rnd() & 1) != 0) {
                    if (!d.water(x, y)) setTile(c, x, y, 5);
                    else d.txt(x, y) = 0xFB;
                }
            }
        }
    for (int dx = -1; dx < 3; ++dx)
        for (int dy = -2; dy < 2; ++dy)
            if ((d.rnd() & 1) != 0 && d.in(a + dx, b + dy)) setTile(c, a + dx, b + dy, 5);
    return true;
}

bool lavaHandler(SimContext& c) {   // 0x45EF30: a lava flow runs 39 tiles from the volcano's crater (tile 0xCD)
    D d(c);
    int a = 0, b = 0;
    bool found = false;
    for (a = 0; a < 0x80 && !found; ++a)
        for (b = 0; b < 0x80; ++b)
            if (d.bld(a, b) == 0xCD) { found = true; break; }
    if (!found) return false;
    --a;
    int steps = 0x27;
    int r = d.rnd();
    for (;;) {
        if (a < 0 || a > 0x7F || b < 0 || b > 0x7F) return true;
        if (d.bld(a, b) != 0xCD) {
            if (d.water(a, b)) d.txt(a, b) = 0xFB;
            startFire(c, a, b, 1);
            c.hooks.playSound(0x202);
        }
        --steps;
        a += kDx8[r & 7];
        b += kDy8[r & 7];
        r = d.rnd();
        if (steps < 1) return true;
    }
}

bool meteorHandler(SimContext& c) {   // 0x45DC00: a barrage of impacts that raises craters; the city's funds are the terrain tool's budget meanwhile
    D d(c);
    const int fa = c.things.focusA, fb = c.things.focusB;
    const int saved = c.s.treasury;
    c.s.treasury = 25000;
    ToolState ts;
    ts.dozerSub = c.things.dozerSub;
    while (c.s.treasury >= 1) {
        int a, b;
        do {
            a = fa - 2 + d.rmod(5);
            b = fb - 2 + d.rmod(5);
        } while (!d.in(a, b));
        d.txt(a, b) = static_cast<uint8_t>(((d.rnd() & 1) == 0 ? 0xFC : 0) - 1);
        if (!raiseTerrain(c, ts, a, b)) c.s.treasury -= 1000;
        a = ((d.rnd() & 0x1F) - 0x10) + fa;
        b = ((d.rnd() & 0x1F) - 0x10) + fb;
        if (d.in(a, b)) d.txt(a, b) = d.water(a, b) ? 0xFB : 0xFF;
        if ((d.rnd() & 7) != 0) c.hooks.playSound(0x1F8);
    }
    c.s.treasury = saved;
    return true;
}

bool firestormHandler(SimContext& c) {   // 0x45D990: 65 fires spiralling out from the focus
    D d(c);
    int a = c.things.focusA, b = c.things.focusB, left = 0x41;
    bool any = false;
    D::Spiral sp;
    do {
        if (left == 0) break;
        a = static_cast<int16_t>(a + kDx4[sp.dir]);
        b = static_cast<int16_t>(b + kDy4[sp.dir]);
        if (d.in(a, b) && startFire(c, a, b, 1)) { --left; d.focus(a, b); any = true; }
        sp.advance();
    } while (sp.len < 0x80);
    return any;
}

int scale(const SimContext& c) { return static_cast<int16_t>(c.s.population10 / 10000 + 5); }

bool crashesHandler(SimContext& c) {   // 0x45DAE0: several vehicle fires around the focus
    D d(c);
    const int fa = c.things.focusA, fb = c.things.focusB;
    bool any = false;
    for (int n = scale(c); n > 0; --n) {
        const int a = ((d.rnd() & 0x1F) - 0x10) + fa;
        const int b = ((d.rnd() & 0x1F) - 0x10) + fb;
        if (!d.in(a, b)) continue;
        d.focus(a, b);
        if (vehicleCrash(c)) any = true;
    }
    return any;
}

bool floodsHandler(SimContext& c) {   // 0x45D330: several floods around the focus
    D d(c);
    const int fa = c.things.focusA, fb = c.things.focusB;
    bool any = false;
    for (int n = scale(c); n > 0; --n) {
        const int a = ((d.rnd() & 0x1F) - 0x10) + fa;
        const int b = ((d.rnd() & 0x1F) - 0x10) + fb;
        if (!d.in(a, b)) continue;
        d.focus(a, b);
        if (floodHandler(c)) any = true;
    }
    return any;
}

bool sparksHandler(SimContext& c) {   // 0x45D210: flames scattered around the focus
    D d(c);
    const int fa = c.things.focusA, fb = c.things.focusB;
    bool any = false;
    for (int n = scale(c); n > 0; --n) {
        const int a = ((d.rnd() & 7) - 4) + fa;
        const int b = ((d.rnd() & 7) - 4) + fb;
        if (!d.in(a, b)) continue;
        any = true;
        d.txt(a, b) = 0xFB;
        d.focus(a, b);
    }
    return any;
}

bool hurricaneHandler(SimContext& c) {   // 0x45CE40: a flood driven in from the map edge that faces away from the view
    D d(c);
    c.hooks.playSound(0x1F6);
    c.things.floodTimer = 0x3C;
    c.things.floodBudget = 0x32;
    const int side = (c.s.rotation + 1) & 3;
    int n = 0;
    switch (side) {
        case 0:
            for (n = 0x14; n != 0; --n) {
                int y = 0x7F; const int x = d.lf(0x7F);
                do {
                    if (0 < y && d.bld(x, y) > 0xC) break;
                    y = static_cast<int16_t>(y - c.lfsr.mod(0x14));
                } while (y >= 0);
                if (0 < y) demolishAndIgnite(c, x, y, 1, 0);
            }
            c.hooks.playSound(0x1F6);
            for (n = 0x32; n != 0; --n) {
                const int x = d.lf(0x7F); int y = 0x7F;
                do { if (5 < d.bld(x, y)) break; --y; } while (y >= 0);
                if (0 < y) d.txt(x, y) = 0xFC;
            }
            break;
        case 1:
            for (n = 0x14; n != 0; --n) {
                int a = 0x7F; const int b = d.lf(0x7F);
                do {
                    if (0 < a && d.bld(a, b) > 0xC) break;
                    a = static_cast<int16_t>(a - c.lfsr.mod(0x14));
                } while (a >= 0);
                if (0 < a) demolishAndIgnite(c, a, b, 0, 0);
            }
            c.hooks.playSound(0x1F6);
            for (n = 100; n != 0; --n) {
                const int b = d.lf(0x7F); int a = 0x7F;
                do { if (5 < d.bld(a, b)) break; --a; } while (a >= 0);
                if (0 < a) d.txt(a, b) = 0xFC;
            }
            break;
        case 2: {
            int count = 0;
            do {
                int next = count + 1;
                const int x = d.lf(0x7F); int y = 0;
                bool lt = true;   // y < 0x7F
                do {
                    if (lt && d.bld(x, y) > 0xC) break;
                    y = static_cast<int16_t>(y + c.lfsr.mod(0x14));
                    lt = y < 0x7F;
                } while (y < 0x80);
                if (y < 0x7F) { next = count + 2; demolishAndIgnite(c, x, y, 0, 0); }
                count = next;
            } while (count < 0x14);
            c.hooks.playSound(0x1F6);
            for (n = 100; n != 0; --n) {
                const int x = d.lf(0x7F); int y = 0;
                do { if (5 < d.bld(x, y)) break; ++y; } while (y < 0x7F);
                if (y < 0x7F) d.txt(x, y) = 0xFC;
            }
            break;
        }
        default:
            for (n = 0x14; n != 0; --n) {
                int a = 0; const int b = d.lf(0x7F);
                bool lt = true;
                do {
                    if (lt && d.bld(a, b) > 0xC) break;
                    a = static_cast<int16_t>(a + c.lfsr.mod(0x14));
                    lt = a < 0x7F;
                } while (a < 0x80);
                if (a < 0x7F) demolishAndIgnite(c, a, b, 1, 0);
            }
            c.hooks.playSound(0x1F6);
            for (n = 0x32; n != 0; --n) {
                const int b = d.lf(0x7F); int a = 0;
                do { if (5 < d.bld(a, b)) break; ++a; } while (a < 0x80);
                if (a < 0x7F) d.txt(a, b) = 0xFC;
            }
            break;
    }
    c.hooks.playSound(0x1F6);
    return true;
}

bool ufoHandler(SimContext& c) {   // 0x45E130: a plane-type thing in state 7 appears over the middle of the map
    D d(c);
    int a, b;
    do {
        a = d.lf(0x3F) + 0x20;
        b = d.lf(0x3F) + 0x20;
    } while (d.txt(a, b) != 0);
    const int slot = freeThingSlot(c);
    if (slot == 0) return false;
    d.txt(a, b) = static_cast<uint8_t>(slot + 0xC9);
    d.thing(slot, 0) = 1;
    d.thing(slot, 3) = static_cast<uint8_t>(a);
    d.thing(slot, 4) = static_cast<uint8_t>(b);
    d.thing(slot, 6) = 8; d.thing(slot, 7) = 8;
    d.thing(slot, 5) = 0x10;
    d.thing(slot, 10) = 0;
    d.thing(slot, 2) = 7;
    d.focus(a, b);
    return true;
}

}  // namespace

bool startDisaster(SimContext& c, int kind) {
    switch (kind) {
        case 1: return fireHandler(c);
        case 2: return floodHandler(c);
        case 3: { const bool r = vehicleCrash(c); if (r) { vehicleCrash(c); vehicleCrash(c); } return r; }
        case 4: return meltdownHandler(c);
        case 5: return true;
        case 6: return earthquakeHandler(c);
        case 7: return spawnTornado(c);
        case 8: return spawnMonster(c);
        case 9: return volcanoHandler(c);
        case 10: return lavaHandler(c);
        case 0xB: return meteorHandler(c);
        case 0xC: return firestormHandler(c);
        case 0xD: return crashesHandler(c);
        case 0xE: return floodsHandler(c);
        case 0xF: return sparksHandler(c);
        case 0x10: return hurricaneHandler(c);
        case 0x11: return true;
        case 0x12: return ufoHandler(c);
        default: return false;
    }
}

bool disasterFrame(SimContext& c) {
    D d(c);
    bool anyFF = false, anyVehicle = false, anyFB = false, anyFC = false;
    if (c.things.floodTimer > 0) --c.things.floodTimer;
    for (int x = 0; x < 0x80; ++x) {
        for (int y = 0; y < 0x80; ++y) {
            const int v = d.txt(x, y);
            if (v == 0xFF) {
                anyFF = true;
                if ((d.rnd() & 3) != 0) continue;
                if (d.water(x, y)) { d.txt(x, y) = 0; continue; }
                switch (d.rnd() & 7) {
                    case 0: if (d.in(x - 1, y)) startFire(c, x - 1, y, 0); break;
                    case 1: if (d.in(x, y - 1)) startFire(c, x, y - 1, 0); break;
                    case 2: if (d.in(x + 1, y)) startFire(c, x + 1, y, 0); break;
                    case 3: if (d.in(x, y + 1)) startFire(c, x, y + 1, 0); break;
                    case 5:
                        if (d.bld(x, y) > 0x6F) {
                            const int id = d.bld(x, y);
                            demolishAndIgnite(c, x, y, 1, 1);
                            burnOut(c, x, y);
                            if (d.lf(0xF) == 0) spawnEffect(c, x, y, 0, 0, 1);
                            if (id == 0x85 || id == 0x9F || id == 0xBC) {
                                int a = x, b = y;
                                const int size = footprintOf(c, a, b, id);
                                markFire(c, a, b, 0xFB);
                                if (size > 1) { markFire(c, a, b - 1, 0xFB); markFire(c, a + 1, b - 1, 0xFB); markFire(c, a + 1, b, 0xFB); }
                                if (size > 2) {
                                    markFire(c, a, b - 2, 0xFB); markFire(c, a + 1, b - 2, 0xFB); markFire(c, a + 2, b - 2, 0xFB);
                                    markFire(c, a + 2, b - 1, 0xFB); markFire(c, a + 2, b, 0xFB);
                                }
                            }
                        }
                        break;
                    default: {   // 4, 6, 7: the fire catches according to the fire-station coverage
                        const int limit = d.w.xfir.at(x / 4, y / 4) + 8;
                        if ((d.rnd() & 0xFF) < limit) { demolishAndIgnite(c, x, y, 1, 1); burnOut(c, x, y); }
                        break;
                    }
                }
                continue;
            }
            if (v == 0xFE || v == 0xFD) {
                anyVehicle = true;
                if ((d.rnd() & 3) != 0) continue;
                if ((d.rnd() & 0xFF) == 0 || d.water(x, y)) { d.txt(x, y) = 0; continue; }
                d.w.xtrf.at(x / 2, y / 2) = 0;
                const bool minus = v == 0xFE;   // 0xFE burns toward -x/-y, 0xFD toward +x/+y
                int na = x, nb = y;
                bool go = true;
                switch (d.rnd() & 0x7F) {
                    case 0: na = x - 1; break;
                    case 1: nb = y - 1; break;
                    case 2: na = x + 1; break;
                    case 3: nb = y + 1; break;
                    default: go = false;
                }
                if (go) startFire(c, na, nb, 0);
                int m = 0;
                const int sx = minus ? -1 : 1;
                const bool aOk = minus ? x > 0 : x < 0x7F, bOk = minus ? y > 0 : y < 0x7F;
                if (aOk && flameClass(d.bld(x + sx, y))) m = 1;
                if (bOk && flameClass(d.bld(x, y + sx))) m |= 2;
                const int self = minus ? 0xFD : 0xFE, other = minus ? 0xFE : 0xFD;
                if (m == 0) d.txt(x, y) = static_cast<uint8_t>(self);
                else {
                    d.txt(x, y) = (d.rnd() & 7) == 0 ? static_cast<uint8_t>(self) : 0;
                    if (m == 3) m = (d.rnd() & 1) + 1;
                    if (m == 1) markFire(c, x + sx, y, other);
                    else markFire(c, x, y + sx, other);
                }
                continue;
            }
            if (v == 0xFB) {
                anyFB = true;
                if ((d.rnd() & 1) != 0) continue;
                bool out = d.lf(0x3F) == 0;
                if (!out && d.water(x, y) && (d.rnd() & 0xF) == 0) out = true;
                if (out) { d.txt(x, y) = 0; continue; }
                const int id = d.bld(x, y);
                if (id > 0x6F && id < 0xC6 && c.t.collapseBlocked[static_cast<size_t>(id - 0x70)] == 0) {
                    int a = x, b = y;
                    int size = footprintOf(c, a, b, id);
                    if (size != 2 && size == 3) size = 4;
                    collapseBuilding(c, a, b, size, 0);
                }
                int best = -1, low = d.alt(x, y);
                for (int i = 0; i < 4; ++i) {
                    const int na = x + kDx4[i], nb = y + kDy4[i];
                    if (!d.in(na, nb)) continue;
                    const int al = d.alt(na, nb);
                    if (al < low) { best = i; low = al; }
                }
                if (best < 0) best = d.rnd() & 3;
                d.txt(x, y) = 0;
                markFire(c, x + kDx4[best], y + kDy4[best], 0xFB);
                continue;
            }
            if (v == 0xFC) {
                anyFC = true;
                if (c.things.floodTimer == 0 && c.lfsr.mask(1) != 0) { d.txt(x, y) = 0; continue; }
                if (c.things.floodTimer > 0x33 || (d.rnd() & 3) == 0) {
                    if (c.things.floodTimer < 0x1E && (d.rnd() & 3) == 0) {
                        if (d.bld(x, y) > 0x6F) demolishAndIgnite(c, x, y, 0, 0);
                        d.txt(x, y) = 0;
                    }
                    if (c.things.floodTimer > 0) {
                        switch (d.rnd() & 3) {
                            case 0: floodSpread(c, x - 1, y, d.alt(x, y)); break;
                            case 1: floodSpread(c, x, y - 1, d.alt(x, y)); break;
                            case 2: floodSpread(c, x + 1, y, d.alt(x, y)); break;
                            default: floodSpread(c, x, y + 1, d.alt(x, y)); break;
                        }
                    }
                }
                continue;
            }
            if (v > 200 && v < 0xF1) {
                const int type = d.thing(v - 0xC9, 0);
                if (type == 8 || type == 0xE || (type == 7 && c.lfsr.mask(0xF) == 0)) {
                    switch (d.rnd() & 3) {
                        case 0: if (d.in(x - 1, y)) burnOut(c, x - 1, y); break;
                        case 1: if (d.in(x, y - 1)) burnOut(c, x, y - 1); break;
                        case 2: if (d.in(x + 1, y)) burnOut(c, x + 1, y); break;
                        default: if (d.in(x, y + 1)) burnOut(c, x, y + 1); break;
                    }
                }
                if (type == 7 || type == 0xE) {
                    int a = x, b = y;
                    switch (d.rnd() & 3) { case 0: --a; break; case 1: --b; break; case 2: ++a; break; default: ++b; break; }
                    clearVehicleFire(c, a, b);
                }
            }
        }
    }
    if (anyVehicle && (d.rnd() & 7) == 0) c.hooks.playSound(0x200);
    if (anyFC && (d.rnd() & 7) == 0) c.hooks.playSound(0x1FF);
    if (anyFF) c.hooks.playSound(0x1FB, -1);
    if (c.things.floodTimer != 0 && c.things.floodBudget != 0) {
        if ((d.rnd() & 7) == 0) c.hooks.playSound(0x1F6);
        if (c.lfsr.mask(1) == 0) {
            const int a = c.lfsr.mod(0x80), b = c.lfsr.mod(0x80);
            if (d.bld(a, b) > 0x70) demolishAndIgnite(c, a, b, 1, 0);
        }
        --c.things.floodBudget;
    }
    return anyFF || anyVehicle || anyFB || anyFC;
}

void disasterTick(SimContext& c) {
    CityState& s = c.s;
    int handled = 1;
    const int kind = c.things.disasterRequest;
    if (kind != 0) {
        c.things.disasterRequest = 0;
        handled = startDisaster(c, kind) ? 1 : 0;
        if (handled == 0) {
            if (s.disasterActive == 0) return;
        } else {
            s.disasterKind = kind;
            s.disasterActive = 1;
            c.hooks.playSound(0x208, 5);
            c.hooks.scrollTo(c.things.focusA, c.things.focusB);
            if (s.militaryType == 1 && c.lfsr.mask(3) == 0) spawnVisitor(c, kind);
        }
    }
    if (s.disasterActive != 0) {
        s.disasterActive = disasterFrame(c) ? 1 : 0;
        for (int slot = 1; slot < 0x28; ++slot) {
            const int type = c.w.xthg[static_cast<size_t>(slot) * 12], state = c.w.xthg[static_cast<size_t>(slot) * 12 + 2];
            if (type == 5 || type == 0xF || (type == 1 && state == 7) || type == 6) { s.disasterActive = 1; break; }
        }
        if (s.disasterActive != 0) return;
    }
    c.things.lastSpawned = 0xFFFF;
    if (c.things.fireMessageKind == -1) c.things.fireMessageKind = 0;
    if (handled != 0) {
        static const int kMessage[0x12] = {0x16, 0x17, 0x23, 0x20, 0x18, 0x1B, 0x1A, 0x1C, 0x1D, 0x1E, 0x1F, 0x16, 0x23, 0x17, 0x21, 0x22, 0x19, 0x18};
        if (s.disasterKind >= 1 && s.disasterKind <= 0x12) c.hooks.postMessage(kMessage[s.disasterKind - 1], c.things.fireMessageKind & 0xFF);
    }
    c.things.fireMessageKind = -1;
    for (int a = 0; a < 0x80; ++a)
        for (int b = 0; b < 0x80; ++b) {
            const int v = c.w.xtxt.at(a, b);
            if (v > 200 && v < 0xF1) {
                uint8_t& type = c.w.xthg[static_cast<size_t>(v - 0xC9) * 12];
                if (type == 7 || type == 8 || type == 0xE) { c.w.xtxt.at(a, b) = 0; type = 0; }
            }
        }
}

}  // namespace sc2k
