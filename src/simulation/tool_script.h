// Deterministic pseudo-random sequences of tool operations, shared by the emulator diff (tools/emu) and the unit tests so both
// replay exactly the same actions. Development / test support only.
#pragma once
#include <algorithm>
#include <cstdio>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "simulation/tools.h"

namespace sc2k {

struct ToolOp {
    enum Kind { Bulldoze, Network, Build, Zone } kind = Bulldoze;
    int a = 0, b = 0, ta = 0, tb = 0;          // tile / drag end
    int cat = 0, sub = 0;                      // tool category + subtype (cost / id table row)
    int zone = 0, unit = 0;                    // zone type painted, cost per tile
    NetworkKind net = NetworkKind::Road;
    bool underground = false;
    bool bridgeStart = false;                  // a network click on the water edge
    std::string name;
};

class ToolScript {
public:
    explicit ToolScript(uint32_t seed) : s_(seed) {}

    ToolOp next(const World& pw, const GameTables& t) {
        ToolOp op;
        const int kind = range(100);
        if (kind < 25) {
            op.kind = ToolOp::Bulldoze;
            op.underground = range(4) == 0;
            std::tie(op.a, op.b) = pick(pw, range(4) != 0);
            op.cat = 0; op.sub = 0;
            op.name = "bulldoze";
        } else if (kind < 60) {
            op.kind = ToolOp::Network;
            static const struct { NetworkKind k; int sub, cat; const char* name; } nets[5] = {
                {NetworkKind::Road, 0, toolcat::kRoads, "road"}, {NetworkKind::Rail, 0, toolcat::kRail, "rail"}, {NetworkKind::Power, 0, toolcat::kPower, "power"},
                {NetworkKind::Subway, 1, toolcat::kRail, "subway"}, {NetworkKind::Pipe, 0, toolcat::kWater, "pipe"}};
            const auto& n = nets[range(5)];
            op.net = n.k; op.sub = n.sub; op.cat = n.cat;
            const bool surface = n.k != NetworkKind::Subway && n.k != NetworkKind::Pipe;
            std::tie(op.a, op.b) = (surface && range(3) == 0) ? pickShore(pw) : pick(pw, false);
            op.ta = op.a; op.tb = op.b;
            if (range(2)) op.ta = std::clamp(op.a + range(25) - 12, 0, 127); else op.tb = std::clamp(op.b + range(25) - 12, 0, 127);
            if (range(10) == 0) { op.a = range(2) ? 0 : 127; op.ta = op.a; op.tb = std::clamp(op.b + range(9) - 4, 0, 127); }   // towards the neighbouring cities
            op.unit = t.toolCost[static_cast<size_t>(n.cat * 12 + n.sub)];
            op.bridgeStart = surface && (pw.xbit.at(op.a, op.b) & xbit::kWater) && pw.xter.at(op.a, op.b) < 0x40;
            op.name = std::string(op.bridgeStart ? "bridge-" : "") + n.name;
        } else if (kind < 85) {
            op.kind = ToolOp::Build;
            std::vector<std::pair<int, int>> buildable;
            for (int cat = 0; cat < 15; ++cat)
                for (int sub = 0; sub < 12; ++sub) {
                    const int id = toolBuildingId(cat, sub);
                    if (id != 0 && id != 0xD7) buildable.emplace_back(cat, sub);   // 0xD7 (stadium) opens a naming dialog
                }
            std::tie(op.cat, op.sub) = buildable[static_cast<size_t>(range(static_cast<int>(buildable.size())))];
            const int size = t.toolBuildingSize[static_cast<size_t>(op.cat * 12 + op.sub)];
            std::tie(op.a, op.b) = range(8) ? pickFree(pw, size == 255 ? 1 : size) : pick(pw, false);
            op.name = "build";
        } else {
            op.kind = ToolOp::Zone;
            static const int zoneTable[][3] = {{8, 0, 9}, {8, 1, 8}, {9, 0, 1}, {9, 1, 2}, {10, 0, 3}, {10, 1, 4}, {11, 0, 5}, {11, 1, 6}, {0, 4, 0}};
            const auto& zt = zoneTable[range(9)];
            op.cat = zt[0]; op.sub = zt[1]; op.zone = zt[2];
            std::tie(op.a, op.b) = range(3) ? pickFree(pw, 3) : pick(pw, false);
            op.ta = std::clamp(op.a + range(9) - 2, 0, 127); op.tb = std::clamp(op.b + range(9) - 2, 0, 127);
            op.unit = t.toolCost[static_cast<size_t>(op.cat * 12 + op.sub)];
            op.name = "zone";
        }
        return op;
    }

    std::string describe(const ToolOp& op) const {
        char buf[160];
        switch (op.kind) {
            case ToolOp::Bulldoze: std::snprintf(buf, sizeof buf, "bulldoze(%d,%d) under=%d", op.a, op.b, op.underground); break;
            case ToolOp::Network: std::snprintf(buf, sizeof buf, "%s(%d,%d)->(%d,%d)", op.name.c_str(), op.a, op.b, op.ta, op.tb); break;
            case ToolOp::Build: std::snprintf(buf, sizeof buf, "build cat %d sub %d id %#x at (%d,%d)", op.cat, op.sub, toolBuildingId(op.cat, op.sub), op.a, op.b); break;
            case ToolOp::Zone: std::snprintf(buf, sizeof buf, "zone cat %d sub %d (%d,%d)-(%d,%d)", op.cat, op.sub, op.a, op.b, op.ta, op.tb); break;
        }
        return buf;
    }

private:
    uint32_t s_;
    uint32_t nextRaw() { s_ = s_ * 1664525u + 1013904223u; return s_ >> 8; }
    int range(int n) { return static_cast<int>(nextRaw() % static_cast<uint32_t>(n)); }

    std::pair<int, int> pick(const World& pw, bool occupied) {
        for (int tries = 0; tries < 200; ++tries) {
            const int a = 2 + range(124), b = 2 + range(124);
            if (!occupied || pw.xbld.at(a, b) != 0 || pw.xund.at(a, b) != 0) return {a, b};
        }
        return {2 + range(124), 2 + range(124)};
    }
    std::pair<int, int> pickFree(const World& pw, int size) {   // flat empty land with a margin, scanned from a random start
        const int start = range(120 * 120);
        for (int k = 0; k < 120 * 120; ++k) {
            const int idx = (start + k) % (120 * 120), a = 3 + idx / 120, b = 3 + idx % 120;
            bool ok = true;
            for (int x = a - 1; x <= a + size + 1 && ok; ++x)
                for (int y = b - 1; y <= b + size + 1 && ok; ++y)
                    if (!inMap(x, y) || pw.xbld.at(x, y) > 0xC || pw.xter.at(x, y) != 0 || (pw.xbit.at(x, y) & xbit::kWater)) ok = false;
            if (ok) return {a, b};
        }
        return {3 + range(120), 3 + range(120)};
    }
    std::pair<int, int> pickShore(const World& pw) {   // a water-edge slope: clicking it starts a bridge
        const int start = range(120 * 120);
        for (int k = 0; k < 120 * 120; ++k) {
            const int idx = (start + k) % (120 * 120), a = 3 + idx / 120, b = 3 + idx % 120;
            if ((pw.xbit.at(a, b) & xbit::kWater) && pw.xter.at(a, b) >= 0x20 && pw.xter.at(a, b) < 0x40) return {a, b};
        }
        return {3 + range(120), 3 + range(120)};
    }
};

// Port side of one scripted operation.
inline bool runToolOp(SimContext& c, ToolState& ts, const ToolOp& op) {
    switch (op.kind) {
        case ToolOp::Bulldoze: ts.underground = op.underground; return bulldozeTile(c, ts, op.a, op.b);
        case ToolOp::Network: return buildNetwork(c, ts, op.net, op.sub, op.a, op.b, op.ta, op.tb);
        case ToolOp::Build: return placeBuilding(c, ts, op.cat, op.sub, op.a, op.b);
        case ToolOp::Zone: return paintZone(c, ts, op.a, op.b, op.ta, op.tb, op.zone, op.unit);
    }
    return false;
}

}  // namespace sc2k
