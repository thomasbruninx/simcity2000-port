// Start of a new city: the port of SIMCITY.EXE's new-city setup 0x42DDE0 (with the global reset 0x434250 it calls). The dialog supplies the
// starting era and the difficulty; the map itself comes from the caller (DEFAULT.SC2 or an edited terrain).
// See docs/simulation/new-city.md.
#pragma once
#include "simulation/sim_context.h"

namespace sc2k {

struct NewCityOptions {
    int era = 0;          // 0..3: starts in 1900, 1950, 2000, 2050 (the dialog's choice; start year = (era + 0x26) * 50)
    int difficulty = 0;   // 0..2: easy ($20,000), medium ($10,000), hard ($10,000 and one bond at the base interest rate)
};

// 0x434250: calendar, milestones, view and city-centre defaults; the per-tile status flags above bit 9 of ALTM are cleared.
void resetCityGlobals(World& w, CityState& s);
// 0x42DDE0 (the call to the availability set-up 0x469A10 is part of it; the caller refreshes ToolAvailability from the new state).
void setupNewCity(World& w, CityState& s, const GameTables& t, CrtRand& rng, const NewCityOptions& o);

}  // namespace sc2k
