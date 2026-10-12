// Residential/commercial/industrial demand valves: first half of SIMCITY.EXE build .022 function 0x467AE0.
// x87 double arithmetic of the original (53-bit precision) is reproduced with IEEE doubles in the same operation order.
// Build with -ffp-contract=off. Random events/messages of the second half are not part of this port.
#pragma once
#include "simulation/city_state.h"
#include "simulation/tables.h"

namespace sc2k {

struct DemandExtra {
    // Counters whose source is not yet located in the loader (set elsewhere in the game); default 0.
    uint16_t c9408 = 0;   // @0x4C9408 term of the industrial limit
    uint16_t cb4f0 = 0;   // @0x4CB4F0 term of the commercial limit
};

struct DemandResult {
    uint32_t populationDecline = 0;   // @0x4CA1C0
    uint32_t populationGrowth = 0;    // @0x4C94E4
    double numerator[3]{};            // capped numerators D[0..2] before dividing by (population+1)
    double valve[3]{};                // E[i] = D[i]/(pop+1) - 1.0
    int taxIndex[3]{};                // adjusted tax rate used to index the tax->demand table
};

// Updates in state: zonePop[0], population10, popHistorySum, ratioMemory, budget[0..2].currentCosts, demand[0..2].
DemandResult runDemand(CityState& s, const GameTables& t, const DemandExtra& x = {});

}  // namespace sc2k
