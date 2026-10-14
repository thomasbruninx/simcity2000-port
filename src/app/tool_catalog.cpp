#include "app/tool_catalog.h"

namespace sc2k {

const std::vector<ToolGroup>& toolCatalog() {
    using K = ToolKind;
    // Names follow the game's menus; the (cat, sub) pairs and costs come from the original's tables. Entries marked Unavailable are
    // original tools that are not ported yet (docs/simulation/tools.md).
    static const std::vector<ToolGroup> groups = {
        {"Bulldozer", {{"Bulldozer", K::Bulldoze, 0, 0}, {"De-zone", K::Zone, 0, 4, 0}}},
        {"Landscape", {{"Tree (shift: remove)", K::Landscape, 1, 0}, {"Water", K::Landscape, 1, 1}, {"Forest (editor)", K::Landscape, 1, 2}, {"Stream (editor)", K::Landscape, 1, 3}}},
        {"Terrain editor", {{"Raise land (spray)", K::TerrainEdit, 0, 1}, {"Lower land (spray)", K::TerrainEdit, 0, 2}, {"Stretch land (drag up/down)", K::TerrainEdit, 0, 3}, {"Level land (drag)", K::TerrainEdit, 0, 4}}},
        {"Dispatch", {{"Police", K::Dispatch, 2, 0}, {"Fire", K::Dispatch, 2, 1}, {"Military", K::Dispatch, 2, 2}}},
        {"Terrain", {{"Level terrain", K::Terrain, 0, 1}, {"Raise terrain", K::Terrain, 0, 2}, {"Lower terrain", K::Terrain, 0, 3}}},
        {"Zones", {{"Light residential", K::Zone, 9, 0, 1}, {"Dense residential", K::Zone, 9, 1, 2}, {"Light commercial", K::Zone, 10, 0, 3},
                   {"Dense commercial", K::Zone, 10, 1, 4}, {"Light industrial", K::Zone, 11, 0, 5}, {"Dense industrial", K::Zone, 11, 1, 6},
                   {"Airport zone", K::Zone, 8, 1, 8}, {"Seaport zone", K::Zone, 8, 0, 9}}},
        {"Power", {{"Power line", K::Network, 3, 0, 0, NetworkKind::Power}, {"Coal plant", K::Building, 3, 2}, {"Hydroelectric (on a waterfall)", K::Special, 3, 3},
                   {"Oil plant", K::Building, 3, 4}, {"Gas plant", K::Building, 3, 5}, {"Nuclear plant", K::Building, 3, 6}, {"Wind turbine", K::Building, 3, 7},
                   {"Solar plant", K::Building, 3, 8}, {"Microwave plant", K::Building, 3, 9}, {"Fusion plant", K::Building, 3, 10}}},
        {"Water", {{"Pipes", K::Network, 4, 0, 0, NetworkKind::Pipe}, {"Water pump", K::Building, 4, 1}, {"Water tower", K::Building, 4, 2},
                   {"Treatment plant", K::Building, 4, 3}, {"Desalination plant", K::Building, 4, 4}}},
        {"Roads", {{"Road (bridge: click the shore)", K::Network, 6, 0, 0, NetworkKind::Road}, {"Highway", K::Highway, 6, 1}, {"Tunnel (on a hillside)", K::Special, 6, 2},
                   {"Onramp", K::Special, 6, 3}, {"Bus depot", K::Building, 6, 4}}},
        {"Rail", {{"Rail", K::Network, 7, 0, 0, NetworkKind::Rail}, {"Subway", K::Network, 7, 1, 0, NetworkKind::Subway}, {"Train station", K::Building, 7, 2},
                  {"Subway station", K::Building, 7, 3}, {"Rail-subway link", K::Special, 7, 4}}},
        {"Education", {{"School", K::Building, 12, 0}, {"College", K::Building, 12, 1}, {"Library", K::Building, 12, 2}, {"Museum", K::Building, 12, 3}}},
        {"Safety", {{"Police station", K::Building, 13, 0}, {"Fire station", K::Building, 13, 1}, {"Hospital", K::Building, 13, 2}, {"Prison", K::Building, 13, 3}}},
        {"Recreation", {{"Small park", K::Building, 14, 0}, {"Big park", K::Building, 14, 1}, {"Zoo", K::Building, 14, 2}, {"Stadium", K::Building, 14, 3},
                        {"Marina", K::Building, 14, 4}}},
        {"Rewards", {{"Mayor's house", K::Building, 5, 0}, {"City hall", K::Building, 5, 1}, {"Statue", K::Building, 5, 2}, {"Braun Llama dome", K::Building, 5, 3}, {"Plymouth arcology", K::Building, 5, 5}, {"Forest arcology", K::Building, 5, 6},
                     {"Darco arcology", K::Building, 5, 7}, {"Launch arcology", K::Building, 5, 8}}},
        {"Sign", {{"Place sign", K::Sign, 0, 0}}},
        {"Query", {{"Query", K::Query, 0, 0}}},
    };
    return groups;
}

}  // namespace sc2k
