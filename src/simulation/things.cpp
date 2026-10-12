// Thing system: ships, aircraft, trains, monsters, tornados, sailboats and their spawners. Literal port of SIMCITY.EXE
// build .022 functions 0x450210 (frame update) and its callees; drawing is omitted. See docs/simulation/things.md.
//
// A "thing" is a 12-byte record in World::xthg (slots 1..0x27, slot 0 is the tool cursor):
//   [0] type  [1] heading  [2] state  [3] a  [4] b  [5] altitude  [6] [7] sub-position (or next tile, trains)
//   [8] [9] target (or sprite phase)  [10] XTXT value saved from under the thing  [11] extra
// The tile a thing stands on carries the XTXT value (slot + 0xC9).
#include "simulation/sim_context.h"

namespace sc2k {

namespace {

using namespace xbit;

constexpr int kSlots = 0x28;

enum Type { kPlane = 1, kHelicopter = 2, kShip = 3, kMonster = 5, kEffect = 6, kSailboat = 9, kTrain = 10, kTrainCar = 11, kTrainB = 12, kTrainCarB = 13,
            kTornado = 0xF, kVisitor = 0x10 };

struct Th {
    SimContext& c;
    uint8_t& f(int slot, int i) const { return c.w.xthg[static_cast<size_t>(slot) * 12 + static_cast<size_t>(i)]; }
    bool inM(int a, int b) const { return inMap(a, b); }
    uint8_t txt(int a, int b) const { return inMap(a, b) ? c.w.xtxt.at(a, b) : 0; }
    void setTxt(int a, int b, int v) const { if (inMap(a, b)) c.w.xtxt.at(a, b) = static_cast<uint8_t>(v); }
    bool water(int a, int b) const { return inMap(a, b) && (c.w.xbit.at(a, b) & kWater); }
    int bld(int a, int b) const { return inMap(a, b) ? c.w.xbld.at(a, b) : 0; }
    int randv() const { return c.rng.next(); }
    int randMod(int n) const { return static_cast<int>(static_cast<uint32_t>(c.rng.next() & 0xFFFF) % static_cast<uint32_t>(n)); }
    int lfsrMod(int n) const { return c.lfsr.mod(static_cast<int16_t>(n)); }
};

int i16(int v) { return static_cast<int16_t>(v); }

// 0x450E10: turn one step toward heading `target`
int turnToward(int cur, int target) {
    if (cur <= target) return (target - cur > 4) ? ((cur - 1) & 7) : ((cur + 1) & 7);
    return (cur - target > 4) ? ((cur + 1) & 7) : ((cur - 1) & 7);
}

// 0x450F60: 8-way heading from (a,b) toward (ta,tb)
int headingToward(int a, int b, int ta, int tb) {
    const int dx = i16(ta - a), dy = i16(tb - b);
    const int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    if (adx < (ady + 1) / 2) return dy < 0 ? 0 : 4;
    const int h = (adx + 1) / 2;
    if (dx < 0) { if (ady < h) return 6; return dy >= 0 ? 5 : 7; }
    if (ady < h) return 2;
    return dy >= 0 ? 3 : 1;
}

// 0x450ED0: heading after one steering step from `cur` toward the target direction
int steerToward(int cur, int a, int b, int ta, int tb) {
    const int t = headingToward(a, b, ta, tb);
    if (t == cur) return cur;
    if (t < cur) return (cur - t > 4) ? ((cur + 1) & 7) : ((cur - 1) & 7);
    return (t - cur > 4) ? ((cur - 1) & 7) : ((cur + 1) & 7);
}

// 0x450E60: coarse heading used while approaching a runway/helipad
int coarseHeading(int a, int b, int ta, int tb) {
    const int dy = i16(tb - b);
    if (i16(ta - a) < 0) { if (dy < 0) return 7; return dy < 1 ? 6 : 5; }
    if (ta == a) return dy < 0 ? 0 : 4;
    if (dy < 0) return 1;
    return dy > 0 ? 3 : 2;
}

// 0x451030: Manhattan distance
int manhattan(int a, int b, int ta, int tb) {
    int dx = i16(ta - a), dy = i16(tb - b);
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx + dy;
}

// 0x450DC0: with probability 1/n wander one heading step
int wander(Th& t, int dir, int n) {
    if (t.randv() % n == 0) dir = (dir + (t.randv() % 3) - 1) & 7;
    return dir;
}

// 0x451070: move by `kind`-speed in heading `dir`, carrying sub-position overflow into the tile and moving the XTXT marker
void moveThing(Th& t, int kind, int s, int dir) {
    SimContext& c = t.c;
    const GameTables& g = c.t;
    const int sp = g.speed[static_cast<size_t>(kind)];
    int sa = t.f(s, 6) + g.dir8Dx[static_cast<size_t>(dir)] * sp;
    int sb = t.f(s, 7) + g.dir8Dy[static_cast<size_t>(dir)] * sp;
    sa = i16(sa); sb = i16(sb);
    int ra = sa, carryA = sa > 0x10 ? 1 : 0;
    if (sa > 0x10) ra = sa - 0x10;
    const bool negA = ra < 0;
    if (negA) { carryA = -1; ra += 0x10; }
    const bool bigB = sb > 0x10;
    int rb = sb, carryB = bigB ? 1 : 0;
    if (bigB) rb = sb - 0x10;
    const bool negB = rb < 0;
    if (negB) { carryB = -1; rb += 0x10; }
    t.f(s, 6) = static_cast<uint8_t>(ra);
    t.f(s, 7) = static_cast<uint8_t>(rb);
    if (negB || bigB || negA || sa > 0x10) {
        const int ob = t.f(s, 4), oa = t.f(s, 3);
        int nb = ob + carryB, na = oa + carryA;
        t.setTxt(oa, ob, t.f(s, 10));
        if (inMap(na, nb)) {
            do {
                if (t.txt(na, nb) < 0xC9) {
                    t.f(s, 3) = static_cast<uint8_t>(na); t.f(s, 4) = static_cast<uint8_t>(nb);
                    t.f(s, 10) = t.txt(na, nb);
                    t.setTxt(na, nb, s + 0xC9);
                    return;
                }
                t.f(s, 3) = static_cast<uint8_t>(na); t.f(s, 4) = static_cast<uint8_t>(nb);
                na = t.f(s, 3) + carryA; nb = t.f(s, 4) + carryB;
            } while (inMap(na, nb));
        }
        removeThing(c, s);
    }
}

// 0x453F20: is the tile three steps ahead in `dir` a solid building (>= 0xFB)?
bool blockedAhead(Th& t, int s, int dir) {
    const GameTables& g = t.c.t;
    const int x = t.f(s, 3) + g.probeDx[static_cast<size_t>(dir)], y = t.f(s, 4) + g.probeDy[static_cast<size_t>(dir)];
    if (!inMap(x, y)) return false;
    return t.c.w.xbld.at(x, y) >= 0xFB;
}

// 0x453E80: try other headings when the way ahead is blocked
void avoidBlocked(Th& t, int s) {
    const GameTables& g = t.c.t;
    if (!blockedAhead(t, s, t.f(s, 1))) return;
    int nd = 0;
    for (int i = 0; i < 7; ++i) {
        nd = (t.f(s, 1) + g.turnOffsets[static_cast<size_t>(i)]) & 7;
        if (!blockedAhead(t, s, nd)) break;
    }
    t.f(s, 1) = static_cast<uint8_t>(nd);
}

void makeExplosion(Th& t, int s, int state, int extra) {   // thing becomes a type-6 effect in place
    t.f(s, 0) = kEffect;
    t.f(s, 2) = static_cast<uint8_t>(state);
    t.f(s, 11) = static_cast<uint8_t>(extra);
    t.f(s, 1) = 0;
}

// ---- trains ----
bool isRailId(int id) { return (id > 0x2B && id < 0x3F) || (id > 0x44 && id < 0x49) || (id > 0x6B && id < 0x70) || id == 0x4D || id == 0x4E; }

// 0x4555C0: may a train of this type enter (x,y)?
bool trainCanEnter(Th& t, int x, int y, int type) {
    if (!inMap(x, y)) return false;
    if (t.c.w.xtxt.at(x, y) > 200) return false;
    const int id = t.c.w.xbld.at(x, y);
    if (type == 10) return isRailId(id) || id == 0x5A || id == 0x5B;
    const int u = t.c.w.xund.at(x, y);
    if ((u != 0 && u < 0x10) || u == 0x23 || u == 0x1F || u == 0x20 || u == 0x22) return true;
    return id > 0x6B && id < 0x71;
}

// 0x455A50: first free heading (0..3) to continue on, or -1
int findTrainHeading(Th& t, int a, int b, int dir, int type) {
    SimContext& c = t.c;
    const GameTables& g = c.t;
    const int r = c.lcg.mod(2);
    for (int i = 0; i < 4; ++i) {
        const int off = r == 0 ? g.trainSearch[static_cast<size_t>(4 + i)] : g.trainSearch[static_cast<size_t>(i)];
        const int nd = (dir + off) & 3;
        const int x = a + g.nbDx[static_cast<size_t>(nd)], y = b + g.nbDy[static_cast<size_t>(nd)];
        if (inMap(x, y) && trainCanEnter(t, x, y, type) && c.w.xtxt.at(x, y) < 0xC9) return nd;
    }
    return -1;
}

// 0x455720: is (a,b) a tile without track (the train has derailed)?
bool offTrack(Th& t, int a, int b) {
    if (!inMap(a, b)) return false;
    const int id = t.c.w.xbld.at(a, b);
    if (isRailId(id) || id == 0x5A || id == 0x5B) return false;
    const int u = t.c.w.xund.at(a, b);
    const bool subway = (u != 0 && u <= 0xF) || u == 0x23 || u == 0x1F || u == 0x20 || u == 0x22;
    if (subway) return false;
    if (id > 0x6B && id < 0x71) return false;
    return true;
}

// 0x455840: copy the position of thing `from` to `to` (cars follow the engine)
void copyPosition(Th& t, int from, int to) {
    for (int i : {3, 4, 6, 7, 10, 1, 8, 0}) t.f(to, i) = t.f(from, i);
    if (t.f(to, 0) == 10) t.f(to, 0) = 11;
    if (t.f(to, 0) == 12) t.f(to, 0) = 13;
}

// 0x455910: reverse a train: engine and last car swap places
void reverseTrain(Th& t, int engine, int last) {
    const uint8_t a = t.f(last, 3), under = t.f(last, 10), b = t.f(last, 4), h = t.f(last, 1);
    for (int i : {3, 4, 6, 7, 10}) t.f(last, i) = t.f(engine, i);
    t.f(engine, 3) = a; t.f(engine, 4) = b; t.f(engine, 6) = a; t.f(engine, 7) = b; t.f(engine, 10) = under;
    const uint8_t nh = static_cast<uint8_t>((h + 2) & 3);
    t.f(engine, 8) = static_cast<uint8_t>(nh + 4);
    t.f(engine, 1) = nh;
}

// 0x454E60: train engine (types 10 and 12) update
void updateTrain(Th& t, int s) {
    SimContext& c = t.c;
    const GameTables& g = c.t;
    int a = t.f(s, 3), b = t.f(s, 4);
    const int na = t.f(s, 6), nb = t.f(s, 7);
    int type = t.f(s, 0);
    if (type == 10 && (c.lfsr.mask(1) != 0)) {   // 0x47FF80
        int k;
        if ((t.f(s, 1) & 1) == 0) k = c.lcg.mod(2);
        else k = c.lcg.mod(2) + 2;
        switch (k) {
            case 0: if (a != 0 && t.c.w.xbld.at(a - 1, b) == 0xED) return; break;
            case 1: if (a < 0x7F && t.c.w.xbld.at(a + 1, b) == 0xED) return; break;
            case 2: if (b != 0 && t.c.w.xbld.at(a, b - 1) == 0xED) return; break;
            case 3: if (b < 0x7F && t.c.w.xbld.at(a, b + 1) == 0xED) return; break;
            default: break;
        }
    }
    const int car1 = t.f(s, 2);
    const int car2 = t.f(car1, 2);
    if (offTrack(t, a, b)) {
        t.f(s, 0) = 0; t.f(car1, 0) = 0; t.f(car2, 0) = 0;
        if (t.txt(a, b) - s == 0xC9) t.setTxt(a, b, 0);
        {
            const int pa = t.f(car1, 3), pb = t.f(car1, 4);
            if (t.txt(pa, pb) - car1 == 0xC9) t.setTxt(pa, pb, 0);
        }
        {
            const int pa = t.f(car2, 3), pb = t.f(car2, 4);
            if (t.txt(pa, pb) - car2 == 0xC9) t.setTxt(pa, pb, 0);
        }
        removeThing(c, s);
        removeThing(c, t.f(s, 2));
        removeThing(c, t.f(t.f(s, 2), 2));
        --c.things.trains;
        spawnEffect(c, a, b, 0, 0, 0);
        return;
    }
    if (t.txt(na, nb) < 0xC9 || t.txt(na, nb) - car2 == 0xC9) {
        if (a != na || b != nb) {
            t.setTxt(a, b, car1 + 0xC9);
            t.setTxt(t.f(car1, 3), t.f(car1, 4), car2 + 0xC9);
            t.setTxt(t.f(car2, 3), t.f(car2, 4), t.f(car2, 10));
            copyPosition(t, car1, car2);
            copyPosition(t, s, car1);
            t.f(s, 10) = t.txt(na, nb);
            t.setTxt(na, nb, s + 0xC9);
        }
        t.f(s, 3) = t.f(s, 6);
        t.f(s, 4) = t.f(s, 7);
        a = t.f(s, 3); b = t.f(s, 4);
        int d = t.f(s, 1) & 0xF;
        const int id = t.bld(a, b);
        if (id > 0x6B && id < 0x71) {
            t.f(s, 0) = 0xC;
            if (type != 10) t.f(s, 0) = 10;
            type = t.f(s, 0);
        }
        if (t.lfsrMod(4) == 0) {
            const int nd = t.lfsrMod(2) == 0 ? d - 1 : d + 1;
            if (trainCanEnter(t, a + g.quadDx[static_cast<size_t>(nd & 3)], b + g.quadDy[static_cast<size_t>(nd & 3)], type)) d = nd & 3;
            (void)t.randv();   // horn sound gate: (char)rand() == 0
        }
        const int tx = a + g.quadDx[static_cast<size_t>(d)], ty = b + g.quadDy[static_cast<size_t>(d)];
        if (!trainCanEnter(t, tx, ty, type)) {
            const int nd = findTrainHeading(t, a, b, d, type);
            if (nd < 0) { reverseTrain(t, s, car2); return; }
            t.f(s, 1) = static_cast<uint8_t>(nd);
            t.f(s, 8) = g.trainTurnSprite[static_cast<size_t>(d * 4 + nd)];
            t.f(s, 6) = static_cast<uint8_t>(g.quadDx[static_cast<size_t>(nd)] + a);
            t.f(s, 7) = static_cast<uint8_t>(g.quadDy[static_cast<size_t>(nd)] + b);
            return;
        }
        t.f(s, 1) = static_cast<uint8_t>(t.f(s, 1) & 0xF);
        t.f(s, 8) = static_cast<uint8_t>(d * 2);
        t.f(s, 6) = static_cast<uint8_t>(tx);
        t.f(s, 7) = static_cast<uint8_t>(ty);
    }
}

// ---- ships ----
// 0x456690: may a ship sail through (x,y)?
bool shipTileOk(Th& t, int x, int y) {
    if (!inMap(x, y)) return false;
    SimContext& c = t.c;
    if (!(c.w.xbit.at(x, y) & kWater)) return false;
    const int ter = c.w.xter.at(x, y);
    if (ter < 0x10 || ter > 0x1F) return false;
    const int id = c.w.xbld.at(x, y);
    if (id == 0xF8) return false;
    if (id == 0) return true;
    for (uint8_t v : c.t.shipTiles) if (v == id) return true;
    return false;
}

// 0x4565C0: can the ship at `s` move one step in `dir`?
bool shipCanMove(Th& t, int s, int dir) {
    const GameTables& g = t.c.t;
    const int x = t.f(s, 3) + g.shipDx[static_cast<size_t>(dir)], y = t.f(s, 4) + g.shipDy[static_cast<size_t>(dir)];
    if (!inMap(x, y)) return true;
    if (!shipTileOk(t, x, y)) return false;
    return t.c.w.xtxt.at(x, y) < 0xC9;
}

// 0x456760: advance the ship by its sub-tile step (12 units per tile)
void shipStep(Th& t, int s) {
    SimContext& c = t.c;
    const GameTables& g = c.t;
    const int h = t.f(s, 1);
    int sa = i16(g.shipStepX[static_cast<size_t>(h)] + t.f(s, 6)), sb = i16(t.f(s, 7) + g.shipStepY[static_cast<size_t>(h)]);
    int ra = sa, carryA = sa > 0xC ? 1 : 0;
    if (sa > 0xC) ra = sa - 0xC;
    const bool negA = ra < 0;
    if (negA) { carryA = -1; ra += 0xC; }
    int rb = sb, carryB = sb > 0xC ? 1 : 0;
    if (sb > 0xC) rb = sb - 0xC;
    const bool negB = rb < 0;
    if (negB) { carryB = -1; rb += 0xC; }
    t.f(s, 6) = static_cast<uint8_t>(ra);
    t.f(s, 7) = static_cast<uint8_t>(rb);
    if (negB || sb > 0xC || negA || sa > 0xC) {
        const int oa = t.f(s, 3), ob = t.f(s, 4);
        const int na = carryA + oa, nb = carryB + ob;
        t.setTxt(oa, ob, t.f(s, 10));
        if (!inMap(na, nb)) return;
        t.f(s, 3) = static_cast<uint8_t>(na);
        t.f(s, 4) = static_cast<uint8_t>(nb);
        t.f(s, 10) = t.txt(na, nb);
        t.setTxt(na, nb, s + 0xC9);
    }
}

void updateShip(Th& t, int s) {
    SimContext& c = t.c;
    const GameTables& g = c.t;
    ++c.things.ships;
    (void)t.randv();   // (char)rand() == 0 -> sound
    const int a = t.f(s, 3), b = t.f(s, 4);
    const int t8 = t.f(s, 8);
    int bStack2 = t.f(s, 9);
    if (a < 0x80 && b < 0x80 && (c.w.xbit.at(a, b) & kWater)) {
        switch (t.f(s, 2)) {
            case 0: {
                if (t.lfsrMod(10) == 0) {
                    const int nd = steerToward(t.f(s, 1), a, b, t8, bStack2);
                    bStack2 = nd & 0xFF;
                    if (shipCanMove(t, s, nd)) t.f(s, 1) = static_cast<uint8_t>(bStack2);
                }
                if (!shipCanMove(t, s, t.f(s, 1))) t.f(s, 2) = 1;
                else shipStep(t, s);
                for (int i = 0; i < 4; ++i) {
                    const int x = g.pierDx2[static_cast<size_t>(i)] + a;
                    const int y = g.pierDy2[static_cast<size_t>(i)] + b;   // (the decompiler shows the target b here; the machine code uses the ship's own b)
                    if (inMap(x, y) && c.w.xbld.at(x, y) == 0xDF) t.f(s, 2) = 3;
                }
                return;
            }
            case 1: {
                const int nd = headingToward(a, b, t8, bStack2);
                t.f(s, 1) = static_cast<uint8_t>(turnToward(t.f(s, 1), nd));
                if (t.f(s, 1) == nd) {
                    t.f(s, 2) = shipCanMove(t, s, nd) ? 0 : 2;
                    return;
                }
                break;
            }
            case 2: {
                const int h = t.f(s, 1);
                int nd = 0, i = 0;
                if (t.lfsrMod(2) == 0) {
                    do { nd = (g.turnB[static_cast<size_t>(i)] + h) & 7; if (shipCanMove(t, s, nd)) break; ++i; } while (i < 9);
                } else {
                    do { nd = (g.turnA[static_cast<size_t>(i)] + h) & 7; if (shipCanMove(t, s, nd)) break; ++i; } while (i < 9);
                }
                if (i == 8) removeThing(c, s);
                t.f(s, 1) = static_cast<uint8_t>(nd);
                t.f(s, 2) = 0;
                return;
            }
            case 3:
                if (t.lfsrMod(0x1E) == 0) {
                    t.f(s, 2) = 4;
                    t.f(s, 8) = static_cast<uint8_t>(c.things.shipTargetA);
                    t.f(s, 9) = static_cast<uint8_t>(c.things.shipTargetB);
                    return;
                }
                break;
            case 4: {
                if (shipCanMove(t, s, t.f(s, 1))) { shipStep(t, s); return; }
                const int h = t.f(s, 1);
                int i = t.lfsrMod(2), nd = h;
                if (i < 9) {
                    do { nd = (g.turnA[static_cast<size_t>(i)] + h) & 7; if (shipCanMove(t, s, nd)) break; ++i; } while (i < 9);
                }
                t.f(s, 1) = static_cast<uint8_t>(nd);
                return;
            }
            default: break;
        }
    } else {
        t.f(s, 0) = kEffect; t.f(s, 2) = 0; t.f(s, 11) = 0; t.f(s, 1) = 0;
    }
}

// ---- sailboats ----
// 0x455F50: may the boat move one step in `dir`?  (0xF8 = marina: the boat docks and vanishes)
bool sailboatCanMove(Th& t, int s, int dir) {
    const GameTables& g = t.c.t;
    const int x = t.f(s, 3) + g.boatDx[static_cast<size_t>(dir)], y = t.f(s, 4) + g.boatDy[static_cast<size_t>(dir)];
    if (!inMap(x, y)) return true;
    const int id = t.c.w.xbld.at(x, y);
    if (id == 0xF8) {
        t.f(s, 0) = 0;
        t.setTxt(t.f(s, 3), t.f(s, 4), 0);
        return false;
    }
    if (id == 0xDF) return false;
    if (t.c.w.xtxt.at(x, y) != 0) return false;
    return (t.c.w.xbit.at(x, y) & kWater) != 0;
}

// 0x455DC0: move the sailboat by 16-unit steps
void sailboatStep(Th& t, int s) {
    SimContext& c = t.c;
    const GameTables& g = c.t;
    const int h = t.f(s, 1);
    int sa = i16(g.sailStepX[static_cast<size_t>(h)] + t.f(s, 6)), sb = i16(t.f(s, 7) + g.sailStepY[static_cast<size_t>(h)]);
    int ra = sa, carryA = sa > 0x10 ? 1 : 0;
    if (sa > 0x10) ra = sa - 0x10;
    const bool negA = ra < 0;
    if (negA) { carryA = -1; ra += 0x10; }
    int rb = sb, carryB = sb > 0x10 ? 1 : 0;
    if (sb > 0x10) rb = sb - 0x10;
    const bool negB = rb < 0;
    if (negB) { carryB = -1; rb += 0x10; }
    t.f(s, 6) = static_cast<uint8_t>(ra);
    t.f(s, 7) = static_cast<uint8_t>(rb);
    if (negB || sb > 0x10 || negA || sa > 0x10) {
        const int ob = t.f(s, 4), oa = t.f(s, 3);
        const int na = carryA + oa, nb = ob + carryB;
        t.setTxt(oa, ob, 0);
        if (na < 0 || nb < 0 || na > 0x7E || nb > 0x7E) { removeThing(c, s); return; }
        t.f(s, 3) = static_cast<uint8_t>(na);
        t.f(s, 4) = static_cast<uint8_t>(nb);
        t.setTxt(na, nb, s + 0xC9);
    }
}

void updateSailboat(Th& t, int s) {
    SimContext& c = t.c;
    ++c.things.sailboats;
    if (c.things.sailboats > 4) { removeThing(c, s); return; }
    if (t.f(s, 2) == 0) {
        if (t.lfsrMod(4) == 0) {
            const int a = t.f(s, 3), b = t.f(s, 4);
            if (a < 0x80 && b < 0x80 && (c.w.xbit.at(a, b) & kWater)) {
                if (t.lfsrMod(4000) == 0) t.f(s, 2) = 1;
                const int h = t.f(s, 1);
                t.f(s, 1) = static_cast<uint8_t>((h + t.randMod(3) - 1) & 3);
                return;
            }
            removeThing(c, s);
        } else if (sailboatCanMove(t, s, t.f(s, 1))) {
            sailboatStep(t, s);
        }
    } else if (t.lfsrMod(5) == 0) {
        removeThing(c, s);
    }
}

// ---- aircraft ----
void updatePlane(Th& t, int s) {
    SimContext& c = t.c;
    ++c.things.planes;
    const int a = t.f(s, 3), b = t.f(s, 4);
    const int id = c.w.xbld.at(a, b);
    if (id > 0x70 && (c.w.xzon.at(a, b) & 0xF) != 8) {
        if (id > 0xFA) { makeExplosion(t, s, 5, t.lfsrMod(0x10) == 0 ? 1 : 0); return; }
        if (static_cast<uint16_t>(t.f(s, 5)) < static_cast<uint16_t>(c.things.spriteHeightOf(id) / 3)) { makeExplosion(t, s, 5, 1); return; }
    }
    switch (t.f(s, 2) & 0xF) {
        case 0: {
            moveThing(t, 1, s, t.f(s, 1));
            const int alt = t.f(s, 5);
            if (alt < 0xE) t.f(s, 5) = static_cast<uint8_t>(alt + 1);
            else t.f(s, 2) = 2;
            return;
        }
        case 1: {
            moveThing(t, 1, s, t.f(s, 1));
            t.f(s, 5) = static_cast<uint8_t>(t.f(s, 5) - 1);
            if (t.f(s, 5) == 0) {
                const int ta = t.f(s, 3), tb = t.f(s, 4);
                t.setTxt(ta, tb, t.f(s, 10));
                if (c.w.xbld.at(ta, tb) != 0xDD) { makeExplosion(t, s, 5, 1); return; }
                removeThing(c, s);
                return;
            }
            break;
        }
        case 2:
            t.f(s, 1) = static_cast<uint8_t>(wander(t, t.f(s, 1), 5));
            avoidBlocked(t, s);
            moveThing(t, 1, s, t.f(s, 1));
            return;
        case 3: {
            const int h = coarseHeading(a, b, t.f(s, 8), t.f(s, 9));
            t.f(s, 1) = static_cast<uint8_t>(h);
            avoidBlocked(t, s);
            moveThing(t, 1, s, t.f(s, 1));
            if (manhattan(t.f(s, 3), t.f(s, 4), t.f(s, 8), t.f(s, 9)) < 2) {
                const int st = t.f(s, 2) >> 4;
                t.f(s, 1) = static_cast<uint8_t>(turnToward(h, st));
                t.f(s, 2) = static_cast<uint8_t>(st * 16 + 4);
                switch (st) {
                    case 1: t.f(s, 9) = static_cast<uint8_t>(t.f(s, 9) - 6); break;
                    case 3: t.f(s, 8) = static_cast<uint8_t>(t.f(s, 8) + 6); break;
                    case 5: t.f(s, 9) = static_cast<uint8_t>(t.f(s, 9) + 6); break;
                    case 7: t.f(s, 8) = static_cast<uint8_t>(t.f(s, 8) - 6); break;
                    default: break;
                }
                return;
            }
            break;
        }
        case 4: {
            const int nd = steerToward(t.f(s, 1), a, b, t.f(s, 8), t.f(s, 9));
            t.f(s, 1) = static_cast<uint8_t>(nd);
            moveThing(t, 1, s, nd);
            if (manhattan(t.f(s, 3), t.f(s, 4), t.f(s, 8), t.f(s, 9)) < 2) {
                const int st = t.f(s, 2) >> 4;
                t.f(s, 1) = static_cast<uint8_t>(st);
                t.f(s, 2) = 1;
                switch (st) {
                    case 1: case 5: t.f(s, 3) = t.f(s, 8); break;
                    case 3: case 7: t.f(s, 4) = t.f(s, 9); break;
                    default: break;
                }
                return;
            }
            break;
        }
        case 7:
            if (t.f(s, 5) != 0) {
                t.f(s, 5) = static_cast<uint8_t>(t.f(s, 5) - 1);
                const int h = t.f(s, 1);
                t.f(s, 1) = static_cast<uint8_t>((h + 1) & 7);
                moveThing(t, 1, s, h);
                return;
            }
            makeExplosion(t, s, 5, 1);
            break;
        default: break;
    }
}

void updateHelicopter(Th& t, int s) {
    SimContext& c = t.c;
    ++c.things.helis;
    const int h0 = t.f(s, 1);
    const int a = t.f(s, 3), b = t.f(s, 4);
    if (c.w.xbld.at(a, b) > 0xFA) { makeExplosion(t, s, 5, 0); return; }
    switch (t.f(s, 2)) {
        case 0:
            t.f(s, 1) = static_cast<uint8_t>((h0 + 1) & 7);
            if (t.f(s, 5) < 10) t.f(s, 5) = static_cast<uint8_t>(t.f(s, 5) + 1);
            else t.f(s, 2) = 2;
            return;
        case 2: {
            t.f(s, 1) = static_cast<uint8_t>(steerToward(h0, a, b, t.f(s, 8), t.f(s, 9)));
            avoidBlocked(t, s);
            moveThing(t, 2, s, t.f(s, 1));
            if (manhattan(t.f(s, 3), t.f(s, 4), t.f(s, 8), t.f(s, 9)) < 2) {
                int na = ((t.randv() & 0x3F) - 0x20) + c.s.centerA2;
                int nb = ((t.randv() & 0x3F) - 0x20) + c.s.centerB2;
                na = i16(na); nb = i16(nb);
                if (na < 0 || na > 0x7F) na = i16((t.randv() & 0x3F) + 0x20);
                if (nb < 0 || nb > 0x7F) nb = i16((t.randv() & 0x3F) + 0x20);
                t.f(s, 8) = static_cast<uint8_t>(na);
                t.f(s, 9) = static_cast<uint8_t>(nb);
                if ((t.randv() & 1) != 0) {
                    const int pa = t.f(s, 3), pb = t.f(s, 4);
                    if (c.w.xbld.at(pa, pb) == 0 && c.w.xter.at(pa, pb) == 0) t.f(s, 2) = 3;
                }
            }
            return;
        }
        case 3:
            t.f(s, 1) = static_cast<uint8_t>((h0 + 1) & 7);
            if (t.f(s, 5) > 2) { t.f(s, 5) = static_cast<uint8_t>(t.f(s, 5) - 1); return; }
            t.f(s, 2) = 4;
            return;
        case 4:
            if (t.randMod(0x14) == 0) t.f(s, 2) = 0;
            return;
        case 5: {
            t.f(s, 1) = static_cast<uint8_t>((h0 + 2) & 7);
            const int alt = t.f(s, 5);
            if (alt > 2) { t.f(s, 5) = static_cast<uint8_t>(alt - 1); return; }
            makeExplosion(t, s, 0x11, 1);
            return;
        }
        default: return;
    }
}

// ---- effects (explosions, smoke) ----
void updateEffect(Th& t, int s) {
    SimContext& c = t.c;
    const int phase = t.f(s, 1);
    if (phase < 2) { t.f(s, 1) = static_cast<uint8_t>(phase + 1); return; }
    bool lit = false;
    const int b = t.f(s, 4), a = t.f(s, 3);
    removeThing(c, s);
    t.setTxt(a, b, 0);
    int n = 4;
    if (t.f(s, 11) != 0 && (c.s.cb4ec == 0 || c.things.scenarioFlag != 0)) {
        do {
            --n;
            const int x = (a - 2) + t.lfsrMod(5);
            const int y = (b - 2) + t.lfsrMod(5);
            if (inMap(x, y)) {
                if (startFire(c, x, y, 1)) lit = true;
            }
        } while (n != 0);
    }
    if (lit && c.s.mode != 2) {
        c.things.focusA = a; c.things.focusB = b;
        const int st = t.f(s, 2);
        c.things.disasterRequest = st != 0 ? st : 1;
    }
}

// ---- tornado ----
void updateTornado(Th& t, int s) {
    SimContext& c = t.c;
    ++c.things.tornados;
    const int a = t.f(s, 3), b = t.f(s, 4);
    const int id = c.w.xbld.at(a, b);
    if (id > 5) destroyTile(c, a, b, 1);
    const int d = t.f(s, 1);
    const int r1 = t.randv(), r2 = t.randv();
    moveThing(t, 0xF, s, ((r1 % 3) - (r2 % 3) + d) & 7);
    if ((t.randv() & 0xFF) == 0) removeThing(c, s);
    if (id < 0xD) {
        const int q1 = t.randv(), q2 = t.randv();
        moveThing(t, 0xF, s, (((q1 & 1) - (q2 & 1)) + d) & 7);
        if ((t.randv() & 0xFF) == 0) removeThing(c, s);
    }
}

// ---- monster ----
void updateMonster(Th& t, int s) {
    SimContext& c = t.c;
    const GameTables& g = c.t;
    ++c.things.monsters;
    const int a = t.f(s, 3), b = t.f(s, 4);
    if (c.things.planes != 0)
        for (int i = 1; i < kSlots; ++i) if (t.f(i, 0) == kPlane) t.f(i, 2) = 7;
    if (c.things.helis != 0)
        for (int i = 1; i < kSlots; ++i) if (t.f(i, 0) == kHelicopter) t.f(i, 2) = 5;
    int newDir = t.f(s, 1);
    switch (t.f(s, 2)) {
        case 0:
            if (t.f(s, 5) < 9) t.f(s, 2) = 1;
            else t.f(s, 5) = static_cast<uint8_t>(t.f(s, 5) - 1);
            t.f(s, 8) = 0; t.f(s, 9) = 0;
            newDir = headingToward(a, b, c.s.centerA2, c.s.centerB2) & 0xFF;
            break;
        case 1: {
            if (t.randMod(0x19) == 0) t.f(s, 2) = 2;
            t.f(s, 8) = static_cast<uint8_t>(t.randv() & 0x7F);
            t.f(s, 9) = static_cast<uint8_t>(t.randv() & 0x7F);
            newDir = wander(t, t.f(s, 1), 5);
            if (a < 0x7F && b < 0x7F) {
                const int x1 = a + 1, y1 = b + 1;
                const int id = c.w.xbld.at(x1, y1);
                if (t.f(s, 11) == 0) {
                    if (id > 5 && startFire(c, x1, y1, 0)) t.f(s, 8) = static_cast<uint8_t>(t.f(s, 8) | 0x80);
                } else if ((x1 > 0x7F || y1 > 0x7F || !(c.w.xbit.at(x1, y1) & kWater)) && id > 0xD && id != 200) {
                    destroyTile(c, x1, y1, 1);
                    const int k = t.f(s, 11);
                    if (k == 1) setTile(c, x1, y1, (t.randv() & 3) + 9);
                    else if (k == 2) floodTile(c, x1, y1);
                    else if (k == 3) placeSpecialBuilding(c, x1, y1, 200, 1);
                    t.f(s, 8) = static_cast<uint8_t>(t.f(s, 8) | 0x80);
                }
            }
            break;
        }
        case 2: {
            if (t.f(s, 5) < 0xF) t.f(s, 5) = static_cast<uint8_t>(t.f(s, 5) + 1);
            else t.f(s, 2) = (t.randMod(3) == 0) ? 3 : 0;
            const int v = (t.randv() & 7) * 9;
            t.f(s, 8) = static_cast<uint8_t>(v); t.f(s, 9) = static_cast<uint8_t>(v);
            newDir = wander(t, t.f(s, 1), 5);
            break;
        }
        case 3:
            t.f(s, 8) = 0; t.f(s, 9) = 0;
            if ((t.randv() & 1) != 0) { t.f(s, 8) = 0x24; t.f(s, 9) = 0x24; }
            newDir = t.f(s, 1);
            if (t.lfsrMod(100) == 0) { removeThing(c, s); return; }
            break;
        default: break;
    }
    if (t.f(s, 2) != 3) {
        const int x = a + g.dir8Dx[static_cast<size_t>(newDir & 7)], y = b + g.dir8Dy[static_cast<size_t>(newDir & 7)];
        if (inMap(x, y)) {
            const int v = c.w.xtxt.at(x, y);
            if (v != 0 && v > 200 && v < 0xF1 && t.f(v - 0xC9, 0) == 0xE) { t.f(s, 2) = 3; return; }
        }
    }
    t.f(s, 1) = static_cast<uint8_t>(newDir);
    moveThing(t, 5, s, newDir);
}

// ---- visitor (type 0x10) ----
void updateVisitor(Th& t, int s) {
    SimContext& c = t.c;
    const GameTables& g = c.t;
    ++c.things.visitors;
    int bottomDir = -1;   // heading of the move executed at the end of the routine (-1: none)
    const int st = t.f(s, 2);
    if (st == 0) {
        int ux, uy;
        bool searchFailed = false;
        const int tgt = t.f(s, 11);
        if (tgt < 0xF1) {
            uy = t.f(tgt, 4); ux = t.f(tgt, 3);
        } else {
            uy = t.f(s, 9); ux = t.f(s, 8);
            if (t.txt(ux, uy) < 0xF1) {
                t.f(s, 2) = 2;
                int x = i16(t.f(s, 3) - 0x20);
                ux = x;
                if (ux <= t.f(s, 3) + 0x20) {
                    do {
                        ux = x;
                        if (t.f(s, 2) != 2) break;
                        uy = i16(t.f(s, 4) - 0x20);
                        if (uy <= t.f(s, 4) + 0x20) {
                            do {
                                if (t.f(s, 2) != 2) break;
                                if (inMap(ux, uy) && c.w.xtxt.at(ux, uy) > 0xF0) {
                                    t.f(s, 8) = static_cast<uint8_t>(ux); t.f(s, 9) = static_cast<uint8_t>(uy); t.f(s, 2) = 0;
                                }
                                uy = i16(uy + 1);
                            } while (uy <= t.f(s, 4) + 0x20);
                        }
                        ux = i16(ux + 1);
                        x = ux;
                    } while (ux <= t.f(s, 3) + 0x20);
                }
                if (t.f(s, 2) == 2) searchFailed = true;
            }
        }
        if (!searchFailed) {
            const int d = headingToward(t.f(s, 3), t.f(s, 4), ux, uy);
            t.f(s, 1) = static_cast<uint8_t>(d);
            const int nx = t.f(s, 3) + g.dir8Dx[static_cast<size_t>(d)], ny = t.f(s, 4) + g.dir8Dy[static_cast<size_t>(d)];
            const int v = t.txt(nx, ny);
            if (v < 0xFB) {
                if (v > 200) {
                    if (t.f(s, 11) == v - 0xC9 && (t.randv() & 3) == 0) {
                        removeThing(c, t.f(s, 11));
                        spawnEffect(c, nx, ny, t.f(s, 5), 0, 1);
                        t.f(s, 2) = 2;
                    } else {
                        t.f(s, 2) = 1;
                    }
                } else {
                    moveThing(t, 0x10, s, d);
                    const int px = t.f(s, 3) + g.dir8Dx[static_cast<size_t>(d)], py = t.f(s, 4) + g.dir8Dy[static_cast<size_t>(d)];
                    if (t.txt(px, py) <= 200) bottomDir = d;
                }
            } else if ((t.randv() & 1) != 0) {
                t.setTxt(nx, ny, 0);
                bottomDir = d;
            }
        }
    } else if (st == 1) {
        if ((t.randv() & 3) == 0) {
            t.f(s, 2) = 0;
        } else {
            const int k = t.randv() & 7;
            if (t.txt(g.dir8Dx[static_cast<size_t>(k)] + t.f(s, 3), g.dir8Dy[static_cast<size_t>(k)] + t.f(s, 3)) < 0xC9) {   // (sic) the original uses the a-coordinate twice
                moveThing(t, 0x10, s, k);
                t.f(s, 1) = static_cast<uint8_t>(k);
            }
        }
    } else if (st == 2) {
        moveThing(t, 0x10, s, t.f(s, 1));
        bottomDir = t.f(s, 1);
    }
    if (bottomDir >= 0) moveThing(t, 0x10, s, bottomDir);
    // altitude follows the target thing or the ground / water surface
    if (t.f(s, 11) < 0xF1) {
        t.f(s, 5) = t.f(t.f(s, 11), 5);
        return;
    }
    const int a = t.f(s, 3), b = t.f(s, 4);
    if (a < 0x80 && b < 0x80 && (c.w.xbit.at(a, b) & kWater)) {
        t.f(s, 5) = static_cast<uint8_t>(((c.w.altm.at(a, b) >> 5) & 0x1F) + t.f(s, 2));
        return;
    }
    t.f(s, 5) = static_cast<uint8_t>((c.w.altm.at(a, b) & 0x1F) + t.f(s, 2));
}

}  // namespace

// ================= public: slot management =================
int freeThingSlot(const SimContext& c) {
    for (int s = 1; s < kSlots; ++s) if (c.w.xthg[static_cast<size_t>(s) * 12] == 0) return s;
    return 0;
}

void removeThing(SimContext& c, int slot) {
    Th t{c};
    t.f(slot, 0) = 0;
    const int a = t.f(slot, 3), b = t.f(slot, 4);
    if (a < 0x80 && b < 0x80) c.w.xtxt.at(a, b) = 0;
}

// ================= public: spawners =================
void spawnHelicopter(SimContext& c, int a, int b) {
    Th t{c};
    if (t.txt(a, b) < 0xC9 && c.things.monsters == 0 && c.things.helis < 1) {
        const int s = freeThingSlot(c);
        if (s != 0) {
            t.f(s, 0) = 2; t.f(s, 1) = 2; t.f(s, 3) = static_cast<uint8_t>(a); t.f(s, 4) = static_cast<uint8_t>(b);
            t.f(s, 6) = 8; t.f(s, 7) = 8;
            t.f(s, 8) = static_cast<uint8_t>(t.randv() & 0x7F);
            t.f(s, 9) = static_cast<uint8_t>(t.randv() & 0x7F);
            t.f(s, 5) = 0;
            t.f(s, 10) = t.txt(a, b);
            t.f(s, 2) = 0;
            ++c.things.helis;
            c.things.soundGate = 0;
            t.setTxt(a, b, s + 0xC9);
        }
    }
}

void spawnPlane(SimContext& c, int a, int b, int dir) {
    Th t{c};
    if (t.txt(a, b) < 0xC9 && c.things.monsters == 0 && c.things.planes < 2) {
        const int s = freeThingSlot(c);
        if (s != 0) {
            t.f(s, 0) = 1; t.f(s, 6) = 8; t.f(s, 7) = 8;
            if (t.randMod(10) < 5) {
                switch (t.randv() & 3) {
                    case 0: t.f(s, 3) = 0; t.f(s, 4) = static_cast<uint8_t>(t.randMod(100) + 10); t.f(s, 1) = 3; break;
                    case 1: t.f(s, 4) = 0; t.f(s, 3) = static_cast<uint8_t>(t.randMod(100) + 10); t.f(s, 1) = 5; break;
                    case 2: t.f(s, 3) = 0x7F; t.f(s, 4) = static_cast<uint8_t>(t.randMod(100) + 10); t.f(s, 1) = 7; break;
                    default: t.f(s, 4) = 0x7F; t.f(s, 3) = static_cast<uint8_t>(t.randMod(100) + 10); t.f(s, 1) = 1; break;
                }
                t.f(s, 5) = 0x10;
                t.f(s, 2) = static_cast<uint8_t>(dir * 16 + 3);
                if (dir == 0) { t.f(s, 8) = static_cast<uint8_t>(a); t.f(s, 9) = static_cast<uint8_t>(b + 0x10); }
                else { t.f(s, 8) = static_cast<uint8_t>(a - 0x10); t.f(s, 9) = static_cast<uint8_t>(b); }
                t.f(s, 10) = t.txt(t.f(s, 3), t.f(s, 4));
                t.setTxt(t.f(s, 3), t.f(s, 4), s + 0xC9);
            } else {
                t.f(s, 3) = static_cast<uint8_t>(a); t.f(s, 4) = static_cast<uint8_t>(b);
                t.f(s, 8) = 0x14; t.f(s, 9) = 0x14;
                t.f(s, 1) = static_cast<uint8_t>(dir);
                t.f(s, 5) = 0; t.f(s, 2) = 0;
                t.f(s, 10) = t.txt(t.f(s, 3), t.f(s, 4));
                t.setTxt(a, b, s + 0xC9);
            }
            ++c.things.planes;
        }
    }
}

void spawnShip(SimContext& c, int pierA, int pierB) {
    Th t{c};
    if (c.things.ships >= 1) return;
    int fa = 0, fb = 0, found = 0;
    switch (t.randv() & 3) {
        case 0: for (int i = 0; i < 0x80; ++i) if (c.w.xter.at(2, i) == 0x10) { fa = 2; fb = i; found = 5; } break;
        case 1: for (int i = 0; i < 0x80; ++i) if (c.w.xter.at(0x7E, i) == 0x10) { fa = 0x7E; fb = i; found = 5; } break;
        case 2: for (int i = 0; i < 0x80; ++i) if (c.w.xter.at(i, 2) == 0x10) { fb = 2; fa = i; found = 5; } break;
        default: for (int i = 0; i < 0x80; ++i) if (c.w.xter.at(i, 0x7E) == 0x10) { fb = 0x7E; fa = i; found = 5; } break;
    }
    if (found == 0) return;
    if (c.w.xtxt.at(fa, fb) < 0xC9) {
        const int s = freeThingSlot(c);
        if (s != 0) {
            t.f(s, 0) = 3;
            t.f(s, 1) = static_cast<uint8_t>(headingToward(fa, fb, pierA, pierB));
            t.f(s, 3) = static_cast<uint8_t>(fa); t.f(s, 4) = static_cast<uint8_t>(fb);
            t.f(s, 6) = 8; t.f(s, 7) = 8; t.f(s, 5) = 1;
            t.f(s, 10) = c.w.xtxt.at(fa, fb);
            t.f(s, 2) = 0;
            c.things.shipTargetB = fb; c.things.shipTargetA = fa;
            ++c.things.ships;
            c.w.xtxt.at(fa, fb) = static_cast<uint8_t>(s + 0xC9);
        }
    }
}

void spawnVisitor(SimContext& c, int kind) {
    Th t{c};
    if (c.things.visitors > 0) return;
    const int s = freeThingSlot(c);
    if (s == 0) return;
    int a = i16(c.things.focusA), b = i16(c.things.focusB);
    switch (t.randv() & 3) {
        case 0: a += 0x10; if (a > 0x7F) a = 0x7F; break;
        case 1: b += 0x10; if (b > 0x7F) b = 0x7F; break;
        case 2: a -= 0x10; if (a < 0) a = 0; break;
        default: b -= 0x10; if (b < 0) b = 0; break;
    }
    t.f(s, 0) = 0x10;
    t.f(s, 1) = static_cast<uint8_t>(headingToward(a, b, i16(c.things.focusA), i16(c.things.focusB)));
    t.f(s, 3) = static_cast<uint8_t>(a); t.f(s, 4) = static_cast<uint8_t>(b);
    t.f(s, 5) = static_cast<uint8_t>((c.w.altm.at(a, b) & 0x1F) + 2);
    t.f(s, 6) = 8; t.f(s, 7) = 8;
    t.f(s, 10) = c.w.xtxt.at(a, b);
    t.f(s, 2) = 0;
    t.f(s, 8) = static_cast<uint8_t>(c.things.focusA); t.f(s, 9) = static_cast<uint8_t>(c.things.focusB);
    switch (kind) {
        case 1: case 5: case 6: case 9: case 10: case 0xB: case 0xC: t.f(s, 11) = 0xFF; break;
        case 2: case 0xE: t.f(s, 11) = 0xFC; break;
        case 3: case 0xD: t.f(s, 11) = static_cast<uint8_t>((t.randv() & 1) - 3); break;
        case 4: case 0xF: t.f(s, 11) = 0xFB; break;
        case 7: case 8: t.f(s, 11) = static_cast<uint8_t>(c.things.lastSpawned); break;
        default: break;
    }
    ++c.things.visitors;
    c.w.xtxt.at(a, b) = static_cast<uint8_t>(s + 0xC9);
}

void spawnEffect(SimContext& c, int a, int b, int alt, int state, int extra) {
    Th t{c};
    const int s = freeThingSlot(c);
    if (s == 0) return;
    if (t.txt(a, b) < 0xC9) {
        t.f(s, 0) = kEffect; t.f(s, 1) = 0;
        t.f(s, 3) = static_cast<uint8_t>(a); t.f(s, 4) = static_cast<uint8_t>(b);
        t.f(s, 6) = 8; t.f(s, 7) = 8;
        t.f(s, 5) = static_cast<uint8_t>(alt);
        t.f(s, 10) = t.txt(a, b);
        t.f(s, 11) = static_cast<uint8_t>(extra);
        t.f(s, 2) = static_cast<uint8_t>(state);
        t.setTxt(a, b, s + 0xC9);
    }
}

namespace {
// 0x452DC0
void spawnSailboat(Th& t, int a, int b) {
    SimContext& c = t.c;
    const int s = freeThingSlot(c);
    if (s == 0) return;
    if (c.w.xbld.at(a, b) == 0 && c.w.xtxt.at(a, b) == 0) {
        t.f(s, 0) = kSailboat;
        t.f(s, 1) = static_cast<uint8_t>(t.lfsrMod(3));
        t.f(s, 3) = static_cast<uint8_t>(a); t.f(s, 4) = static_cast<uint8_t>(b);
        t.f(s, 6) = 4; t.f(s, 7) = 4;
        t.f(s, 5) = 0; t.f(s, 10) = 0; t.f(s, 2) = 0;
        c.w.xtxt.at(a, b) = static_cast<uint8_t>(s + 0xC9);
        ++c.things.sailboats;
    }
}
}  // namespace

void spawnSailboatsNearMarina(SimContext& c, int a, int b) {
    Th t{c};
    if (c.things.sailboats >= 4) return;
    const GameTables& g = c.t;
    for (int i = 0; i < 4; ++i) {
        const int x = g.sailDx[static_cast<size_t>(i)] + a, y = g.sailDy[static_cast<size_t>(i)] + b;
        if (inMap(x, y) && (c.w.xbit.at(x, y) & kWater) && c.w.xtxt.at(x, y) == 0) spawnSailboat(t, x, y);
    }
}

bool spawnTrain(SimContext& c, int a, int b) {
    Th t{c};
    const GameTables& g = c.t;
    if (c.things.trains > 4) return false;
    if (a < 2 || a > 0x7C || b < 2 || b > 0x7C) return false;
    const int id = c.w.xbld.at(a, b);
    if (!isRailId(id)) return false;
    if (i16(id - 0x2C) > 9) return false;
    if (c.w.xtxt.at(a, b) != 0) return false;
    const int d = t.lfsrMod(4);
    const int dir = findTrainHeading(t, a, b, d, 10);
    if (dir < 0) return false;
    const int s1 = freeThingSlot(c); t.f(s1, 0) = 10;
    const int s2 = freeThingSlot(c); t.f(s2, 0) = 11;
    const int s3 = freeThingSlot(c); t.f(s3, 0) = 11;
    for (int s : {s1, s2, s3}) { t.f(s, 1) = static_cast<uint8_t>(dir); t.f(s, 3) = static_cast<uint8_t>(a); t.f(s, 4) = static_cast<uint8_t>(b); }
    t.f(s1, 6) = static_cast<uint8_t>(g.trainInitDx[static_cast<size_t>(dir)] + a);
    t.f(s2, 6) = static_cast<uint8_t>(a); t.f(s3, 6) = static_cast<uint8_t>(a);
    t.f(s1, 7) = static_cast<uint8_t>(g.trainInitDy[static_cast<size_t>(dir)] + b);
    t.f(s2, 7) = static_cast<uint8_t>(b); t.f(s3, 7) = static_cast<uint8_t>(b);
    for (int s : {s1, s2, s3}) t.f(s, 5) = 0;
    t.f(s1, 10) = c.w.xtxt.at(a, b);
    t.f(s2, 10) = 0; t.f(s3, 10) = 0;
    t.f(s1, 2) = static_cast<uint8_t>(s2);
    t.f(s2, 2) = static_cast<uint8_t>(s3);
    c.w.xtxt.at(a, b) = static_cast<uint8_t>(s1 + 0xC9);
    ++c.things.trains;
    return true;
}

void spawnTrainNearStation(SimContext& c, int a, int b) {
    const GameTables& g = c.t;
    for (int i = 0; i < 4; ++i) {
        const int x = g.stationDx[static_cast<size_t>(i)] + a, y = g.stationDy[static_cast<size_t>(i)] + b;
        if (inMap(x, y) && spawnTrain(c, x, y)) break;
    }
}

// ================= public: fire and flood helpers =================
bool startFire(SimContext& c, int a, int b, int flag) {
    Th t{c};
    if (!inMap(a, b)) return false;
    if (c.w.xbit.at(a, b) & kWater) return false;
    if (c.w.xbld.at(a, b) < 6 && flag == 0) return false;
    const int v = c.w.xtxt.at(a, b);
    if (v != 0) {
        if (v < 0x33) {
            releaseTextOverlay(c, v);
        } else if (v < 0xC9) {
            // 0x460CA0: demolish the building and mark its footprint as burning
            const int id = c.w.xbld.at(a, b);
            if (id < 0x3F || id > 0x42) {
                int x = a, y = b;
                const int size = footprintOf(c, x, y, id);
                destroyTile(c, a, b, 1);
                for (int i = 0; i < size; ++i)
                    for (int j = 0; j < size; ++j) {
                        const int px = x + i, py = y - j;
                        if (inMap(px, py) && !(c.w.xbit.at(px, py) & kWater)) c.w.xtxt.at(px, py) = 0xFF;
                    }
                int k = (c.w.xzon.at(a, b) & 0xF);
                if (id >= 0xC6) { k = 10; if (id != 0xC6) k = id - 0xBD; }
                const auto& pr = c.t.messagePriority;
                if (c.things.fireMessageKind == -1 || pr[static_cast<size_t>(c.things.fireMessageKind)] < pr[static_cast<size_t>(k)]) c.things.fireMessageKind = k;
            }
        } else {
            if (v < 0xF1) return false;
            if (v < 0xFA) { setTile(c, a, b, c.lfsr.mod(4) + 1); return false; }
            if (v != 0xFA) return false;
            c.w.xtxt.at(a, b) = 0;
            const int id = c.w.xbld.at(a, b);
            if ((id < 0x1D || id > 0x2B) && (id < 0x3F || id > 0x46) && id != 0x4B && id != 0x4C && (id < 0x5D || id > 0x60)) --c.s.c9408;
            else --c.s.cb4f0;
        }
    }
    c.w.xtxt.at(a, b) = 0xFF;
    c.w.xtrf.at(a / 2, b / 2) = 0;
    (void)t;
    return true;
}

void floodTile(SimContext& c, int a, int b) {
    static const int kShore[16] = {0xD, 0x15, 0x12, 8, 0x13, 0x10, 5, 1, 0x14, 7, 0x11, 4, 6, 3, 2, 0};
    static const int kDiag[16] = {0, 9, 10, 0, 0xB, 0, 0, 0, 0xC, 0, 0, 0, 0, 0, 0, 0};
    auto wat = [&](int x, int y) { return inMap(x, y) && (c.w.xbit.at(x, y) & kWater); };
    uint8_t& ter = c.w.xter.at(a, b);
    const int t0 = ter;
    if (t0 == 0x2E || t0 == 0x3E) return;
    int v;
    if (t0 == 0 || t0 > 0xF) {
        int mask = 0;
        if (b > 0 && wat(a, b - 1)) mask = 1;
        if (a < 0x7F && wat(a + 1, b)) mask |= 2;
        if (b < 0x7F && a >= 0 && wat(a, b + 1)) mask |= 4;
        if ((b < 0x7F ? a >= 0 : true) && a > 0 && wat(a - 1, b)) mask |= 8;
        if (mask < 0xF) {
            v = kShore[mask];
        } else {
            v = 0;
            if (b > 0) {
                if (a > 0 && !wat(a - 1, b - 1)) v = 1;
                if (a < 0x7F && !wat(a + 1, b - 1)) v |= 2;
            }
            if (b < 0x7F) {
                if (a < 0x7F && !wat(a + 1, b + 1)) v |= 4;
                if (a > 0 && !wat(a - 1, b + 1)) v |= 8;
            }
            v = kDiag[v];
        }
    } else {
        v = 0xE;
    }
    if (t0 < 0x10) {
        ter = static_cast<uint8_t>(v + 0x30);
    } else if (t0 < 0x30) {
        if (t0 > 0x1F) {
            if (((v ^ t0) & 0xF) == 0) return;
            ter = static_cast<uint8_t>(t0 - 0x10);
        }
    } else {
        if (v - t0 == -0x30) return;
        ter = static_cast<uint8_t>(v + 0x30);
    }
    if (c.w.xbit.at(a, b) & kWater) return;
    setTile(c, a, b, 0);
    uint16_t& alt = c.w.altm.at(a, b);
    alt = static_cast<uint16_t>((alt & 0xFC1F) | ((alt & 0x1F) << 5));
    c.w.xbit.at(a, b) |= kWater;
    static const int kN[8][2] = {{-1, 0}, {0, 1}, {1, 0}, {0, -1}, {-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    for (const auto& n : kN) if (wat(a + n[0], b + n[1])) floodTile(c, a + n[0], b + n[1]);
}

// ================= public: disaster spawns =================
bool spawnMonster(SimContext& c) {   // 0x45E780
    Th t{c};
    if (c.things.monsters > 0) return false;
    const int s = freeThingSlot(c);
    if (s == 0) return false;
    int fa = i16(c.things.focusA), fb = i16(c.things.focusB);
    if (fa < 0) fa = 0;
    if (fb < 0) fb = 0;
    if (fa > 0x7F) fa = 0x7F;
    if (fb > 0x7F) fb = 0x7F;
    const int v = c.w.xtxt.at(fa, fb);
    if (v > 200 && v < 0xF1) removeThing(c, v - 0xC9);
    t.f(s, 0) = 5; t.f(s, 1) = 2;
    t.f(s, 3) = static_cast<uint8_t>(fa); t.f(s, 4) = static_cast<uint8_t>(fb);
    t.f(s, 6) = 8; t.f(s, 7) = 8;
    t.f(s, 8) = static_cast<uint8_t>(t.randv() & 0x7F);
    t.f(s, 9) = static_cast<uint8_t>(t.randv() & 0x7F);
    t.f(s, 5) = 0xF;
    t.f(s, 10) = c.w.xtxt.at(fa, fb);
    t.f(s, 2) = 0;
    if (c.things.scenarioFlag == 0) {
        if ((t.randv() & 1) == 0) t.f(s, 11) = static_cast<uint8_t>(t.randMod(3) + 1);
        else t.f(s, 11) = 0;
    } else {
        t.f(s, 11) = 0;
    }
    ++c.things.monsters;
    c.w.xtxt.at(fa, fb) = static_cast<uint8_t>(s + 0xC9);
    c.things.lastSpawned = s;
    return true;
}

bool spawnTornado(SimContext& c) {   // 0x45EA00
    Th t{c};
    if (c.things.tornados > 0) return false;
    const int s = freeThingSlot(c);
    if (s == 0) return false;
    int fa = i16(c.things.focusA), fb = i16(c.things.focusB);
    if (fa < 0) fa = 0;
    if (fb < 0) fb = 0;
    if (fa > 0x7F) fa = 0x7F;
    if (fb > 0x7F) fb = 0x7F;
    const int v = c.w.xtxt.at(fa, fb);
    if (v > 200 && v < 0xF1) removeThing(c, v - 0xC9);
    t.f(s, 0) = 0xF;
    t.f(s, 1) = static_cast<uint8_t>(t.randv() & 7);
    t.f(s, 3) = static_cast<uint8_t>(fa); t.f(s, 4) = static_cast<uint8_t>(fb);
    t.f(s, 6) = 8; t.f(s, 7) = 8;
    t.f(s, 8) = static_cast<uint8_t>(t.randv() & 0x7F);
    t.f(s, 9) = static_cast<uint8_t>(t.randv() & 0x7F);
    t.f(s, 5) = static_cast<uint8_t>(c.w.altm.at(fa, fb) & 0x1F);
    t.f(s, 10) = c.w.xtxt.at(fa, fb);
    t.f(s, 2) = 0;
    ++c.things.tornados;
    c.w.xtxt.at(fa, fb) = static_cast<uint8_t>(s + 0xC9);
    c.things.lastSpawned = s;
    return true;
}

// ================= the frame =================
void thingFrame(SimContext& c) {
    Th t{c};
    ThingState& th = c.things;
    if (++th.animFrame > 0x3FF) th.animFrame = 0;
    th.planes = th.helis = th.ships = th.sailboats = th.visitors = 0;
    th.monsters = th.trains = th.tornados = 0;
    if (th.trafficAverage > 0x23 && (t.randv() & 0xFF) == 0) { /* traffic sound */ }
    if (c.s.census[0xD2] != 0 && th.crimeAverage > 0x28 && (t.randv() & 0xFF) == 0) { /* siren sound */ }
    static const bool kActive[17] = {false, true, true, true, true, true, true, false, false, true, true, false, true, false, false, true, true};
    for (int s = 1; s < kSlots; ++s) {
        const int type = t.f(s, 0);
        if (type == 0 || type > 0x10 || !kActive[type]) continue;
        switch (type) {
            case 1: updatePlane(t, s); break;
            case 2: updateHelicopter(t, s); break;
            case 3: updateShip(t, s); break;
            case 5: updateMonster(t, s); break;
            case 6: updateEffect(t, s); break;
            case 9: updateSailboat(t, s); break;
            case 10: case 12: ++th.trains; updateTrain(t, s); break;
            case 0xF: updateTornado(t, s); break;
            case 0x10: updateVisitor(t, s); break;
            default: break;
        }
    }
}

}  // namespace sc2k
