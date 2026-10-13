// The toolbar of the port: groups of tools that map onto the original's (category, subtype) pairs (docs/simulation/tools.md).
#pragma once
#include <string>
#include <vector>

#include "simulation/tools.h"

namespace sc2k {

enum class ToolKind { Bulldoze, Zone, Network, Highway, Terrain, Special, Building, Query, Unavailable };   // Special = single-click tools (dam, tunnel, onramp, link)

struct ToolItem {
    const char* name;
    ToolKind kind;
    int cat, sub;               // cost / id table row of the original
    int zone = 0;               // Zone: zone type painted
    NetworkKind net = NetworkKind::Road;
};

struct ToolGroup {
    const char* name;
    std::vector<ToolItem> items;
};

const std::vector<ToolGroup>& toolCatalog();

}  // namespace sc2k
