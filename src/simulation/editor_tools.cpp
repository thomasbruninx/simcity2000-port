#include "simulation/editor_tools.h"

#include <algorithm>

namespace sc2k {

void editorDemolish(SimContext& c, ToolState& ts, int a, int b, bool underground) {
    World& w = c.w;
    if (!inMap(a, b)) return;
    if (underground) {
        ts.underground = true;
        const int money = c.s.treasury;
        bulldozeTile(c, ts, a, b);
        c.s.treasury = money;
        return;
    }
    const int id = w.xbld.at(a, b);
    if (id == 0) return;
    if (id < 0xD) { setTile(c, a, b, 0); return; }
    std::vector<std::pair<int, int>> before;   // rubble that already lies around stays; new rubble is cleared
    for (int x = a - 4; x <= a + 4; ++x)
        for (int y = b - 4; y <= b + 4; ++y)
            if (inMap(x, y) && w.xbld.at(x, y) >= 1 && w.xbld.at(x, y) <= 4) before.push_back({x, y});
    ts.underground = false;
    const int treasury = c.s.treasury;
    bulldozeTile(c, ts, a, b);
    c.s.treasury = treasury;
    for (int x = a - 4; x <= a + 4; ++x)
        for (int y = b - 4; y <= b + 4; ++y)
            if (inMap(x, y) && w.xbld.at(x, y) >= 1 && w.xbld.at(x, y) <= 4 && std::find(before.begin(), before.end(), std::make_pair(x, y)) == before.end()) setTile(c, x, y, 0);
}

int editorObjectSize(const SimContext& c, int id) {
    return id >= 0x70 ? std::max<int>(1, c.t.footprint[static_cast<size_t>(id) & 0xFF]) : 1;
}

bool editorPlaceObject(SimContext& c, ToolState& ts, int a, int b, int id) {
    const int size = editorObjectSize(c, id);
    const int o = size > 2 ? -1 : 0;
    for (int x = a + o; x < a + o + size; ++x)
        for (int y = b + o; y < b + o + size; ++y)
            if (!inMap(x, y)) return false;
    for (int x = a + o; x < a + o + size; ++x)
        for (int y = b + o; y < b + o + size; ++y) editorDemolish(c, ts, x, y, false);
    if (id >= 0x70) return placeSpecialBuilding(c, a, b, id, size, true);
    setTile(c, a, b, id);
    return true;
}

std::vector<int> bridgeTypesOf(int allowedMask) {
    std::vector<int> types;
    for (int type = 0; type < 7; ++type)
        if (allowedMask & (1 << type)) types.push_back(type);
    return types;
}

}  // namespace sc2k
