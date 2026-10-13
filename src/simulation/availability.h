// Which tools the player may use, by year and ordinances: the port of SIMCITY.EXE's tool-availability set-up 0x469A10 (run when a city is loaded)
// and the per-event unlocks it repeats. Dated events (CityState::eventYears) fire from the tick through ZoneHooks::scheduledEvent.
// See docs/simulation/tool-availability.md.
#pragma once
#include <array>
#include <cstdint>

#include "simulation/city_state.h"
#include "simulation/tables.h"

namespace sc2k {

struct ToolAvailability {
    std::array<uint32_t, 18> mask{};   // @0x4E8A00: bit `sub` set = the button (category, sub) is enabled
    uint32_t plantMask = 7;            // @0x4E8E78: bit k = power plant k (coal, hydro, oil, gas, nuclear, wind, solar, microwave, fusion)
    int arcologies = 0;                // @0x4CCB44: number of unlocked arcology designs (events 12..15)

    // 0x469A10: base masks from the exe, then every event that already fired (eventYears[i] == 0) is applied.
    static ToolAvailability fromState(const CityState& s, const GameTables& t);
    void applyEvent(int index, const CityState& s);   // unlock of dated event 0..16
    bool allows(int category, int sub) const;
};

}  // namespace sc2k
