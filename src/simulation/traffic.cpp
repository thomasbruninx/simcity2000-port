// Traffic router: literal port of SIMCITY.EXE build .022 function 0x456F70.
// A depth-first search over the transport network starting next to a zone tile; it succeeds when it steps onto a tile
// whose zone type is a destination for the origin zone, and then adds `kind` units of traffic along the path.
#include <array>

#include "simulation/sim_context.h"

namespace sc2k {

const int (*neighbourOffsets())[2];   // tile_ops.cpp

int routeTraffic(SimContext& c, int a, int b, int zone, int kind, int maxSteps) {
    const World& w = c.w;
    const GameTables& t = c.t;
    const auto* nb = neighbourOffsets();

    // 1) find a transport tile within the 24-neighbourhood to start from
    int16_t type = -1;
    int x = 0, y = 0;
    for (int i = 0; i < 24 && type < 0; ++i) {
        x = a + nb[i][0];
        y = b + nb[i][1];
        if (inMap(x, y)) {
            const int id = w.xbld.at(x, y);
            if (!isRoad(id)) {
                if (id == 0xEC) type = 8;
                else if (id == 0xED) type = 10;
                else if (id == 0xE9) type = 0xB;
            } else {
                type = 0;
            }
        }
    }
    if (type < 0) return 0;

    // 2) depth-first search
    std::array<uint8_t, 520> maskStack{}, stepStack{};
    std::array<int8_t, 520> typeStack{};
    c.queue.reset(x, y);
    int mask = 0xF;
    maskStack[1] = 0xF;
    typeStack[0] = static_cast<int8_t>(type);
    bool success = false;
    int depth = 1;
    int steps = 0;
    const uint32_t parityBit = static_cast<uint32_t>(c.rng.next());
    int16_t limit = static_cast<int16_t>(maxSteps);
    if (kind == 1) limit = static_cast<int16_t>(limit - (limit >> 2));
    const int destMask = static_cast<uint16_t>(t.zoneDestMask[static_cast<size_t>(zone)]);

    do {
        if (limit <= static_cast<int16_t>(steps)) break;
        bool found = false;
        int dir = c.rng.next() & 3;
        int nx = 0, ny = 0;
        for (int tries = 0; tries < 4; ++tries) {
            if (found) break;
            dir = (static_cast<int>(parityBit & 1) * 2 + 1 + dir) & 3;
            if (!((1 << dir) & static_cast<int16_t>(mask))) continue;
            mask -= (1 << dir);
            nx = x + t.dirDx[static_cast<size_t>(dir)];
            ny = y + t.dirDy[static_cast<size_t>(dir)];
            if (!inMap(nx, ny)) {
                if (w.xtxt.at(x, y) == 0xFA) { found = true; success = true; }
                continue;
            }
            const int id = w.xbld.at(nx, ny);
            const int zoneAt = w.xzon.at(nx, ny) & 0xF;
            const bool destHit = (destMask & (1 << zoneAt)) != 0;
            auto reach = [&]() { found = true; success = true; };           // LAB_00457a4f
            auto take = [&](int add) { steps += add; found = true; };
            switch (type) {
                case 0:
                    if (!destHit) {
                        if (id < 0x3F || id > 0x42) {
                            if (!isBridge(id)) {
                                if (id < 0x5D || id > 0x60) {
                                    if (!isRoad(id)) {
                                        if (id == 0xEC) { take(4); type = 8; }
                                        else if (id == 0xED) { take(4); type = 10; }
                                        else if (id == 0xE9) { take(4); type = 0xB; }
                                        else if (id > 0xFA) reach();
                                    } else take(3);
                                } else { take(2); type = 1; }
                            } else { take(3); type = 3; }
                        } else { take(3); type = 2; }
                    } else reach();
                    break;
                case 1:
                    if (!isHighway(id)) { if (id > 0x5C && id < 0x61) { type = 0; take(1); } }
                    else take(1);
                    break;
                case 2:
                    if (((w.altm.at(nx, ny) >> 8) & 0x7C) == 0) { if (isRoad(id)) { take(3); type = 0; } }
                    else take(3);
                    break;
                case 3:
                    if (!isBridge(id)) { if (isRoad(id)) { take(3); type = 0; } }
                    else take(3);
                    break;
                case 4:
                    if (destHit) { reach(); break; }
                    if (id < 0x3F || id > 0x42) {
                        if (!isBridge(id)) {
                            if (id < 0x5D || id > 0x60) {
                                if (!isRoad(id)) {
                                    if (id == 0xEC) { take(4); type = 9; }
                                    else if (id == 0xED) { take(4); type = 10; }
                                    else if (id == 0xE9) { take(4); type = 0xB; }
                                    else if (id > 0xFA) reach();
                                } else take(2);
                            } else { take(2); type = 5; }
                        } else { take(2); type = 7; }
                    } else { take(2); type = 6; }
                    break;
                case 5:
                    if (!isHighway(id)) { if (id > 0x5C && id < 0x61) { type = 4; take(1); } }
                    else take(1);
                    break;
                case 6:
                    if (((w.altm.at(nx, ny) >> 8) & 0x7C) == 0) { if (isRoad(id)) { take(2); type = 4; } }
                    else take(2);
                    break;
                case 7:
                    if (!isBridge(id)) { if (isRoad(id)) { take(2); type = 4; } }
                    else take(2);
                    break;
                case 8:
                    if (destHit) { reach(); break; }
                    if (id == 0xEC) take(4);
                    else if (isRoad(id)) { take(2); type = 4; }
                    break;
                case 9:
                    if (destHit) { reach(); break; }
                    if (id == 0xEC || id == 0xED) take(4);
                    else if (isRoad(id)) { take(3); type = 0; }
                    break;
                case 10:
                    if (id == 0xED) take(4);
                    else if (isRail(id)) { type = 0xC; take(1); }
                    break;
                case 0xB:
                    if (isSubwayPiece(w.xund.at(nx, ny))) { type = 0xD; take(1); }
                    break;
                case 0xC:
                    if (id == 0xED) { take(4); type = 9; }
                    else if (!isRail(id)) { if (id > 0xFA) reach(); }
                    else take(1);
                    break;
                case 0xD:
                    if (id == 0xE9) { take(4); type = 9; }
                    else if (isSubwayPiece(w.xund.at(nx, ny))) take(1);
                    break;
                default: break;
            }
        }
        if (found) {
            if (success) break;
            x = nx; y = ny;
            maskStack[static_cast<size_t>(depth)] = static_cast<uint8_t>(mask);
            c.queue.push(nx, ny);
            if (type == 3 || type == 7) mask = 1 << dir;
            else if (type == 0xB) mask = 0xF;
            else mask = t.dirNextMask[static_cast<size_t>(dir)];
            ++depth;
            maskStack[static_cast<size_t>(depth)] = static_cast<uint8_t>(mask);
            stepStack[static_cast<size_t>(depth)] = static_cast<uint8_t>(steps);
            typeStack[static_cast<size_t>(depth - 1)] = static_cast<int8_t>(type);
        } else {
            do {
                --depth;
                if (depth > 0) {
                    c.queue.popBack();
                    const auto top = c.queue.peekBack();
                    x = top.a; y = top.b;
                    steps = stepStack[static_cast<size_t>(depth)];
                    type = typeStack[static_cast<size_t>(depth - 1)];
                    mask = maskStack[static_cast<size_t>(depth)];
                }
            } while (mask == 0 && depth > 0);
            if (depth == 0) steps = limit;
        }
    } while (!success);

    // 3) on success, walk the path back adding traffic and ridership statistics
    bool sawSubway = false, sawRail = false, sawBus = false;
    if (success && kind > 0 && !c.queue.empty()) {
        do {
            const auto p = c.queue.popBack();
            --depth;
            const int ptype = typeStack[static_cast<size_t>(depth)];
            if (ptype == 0xB) sawSubway = true;
            if (ptype == 10) sawRail = true;
            if (ptype == 8) sawBus = true;
            if (ptype == 0 || ptype == 1 || ptype == 3) {
                int v = c.w.xtrf.at(p.a / 2, p.b / 2) + kind;
                if (v > 0xFF) v = 0xFF;
                c.w.xtrf.at(p.a / 2, p.b / 2) = static_cast<uint8_t>(v);
            }
        } while (!c.queue.empty());
    }
    if (sawSubway) c.traffic.subway += kind;
    if (sawRail) c.traffic.rail += kind;
    if (sawBus) c.traffic.bus += kind;
    return success ? 1 : 0;
}

}  // namespace sc2k
