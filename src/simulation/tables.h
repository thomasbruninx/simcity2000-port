// Constant lookup tables and numeric constants read from the user's SIMCITY.EXE (build .022).
// Addresses are virtual addresses; see docs/simulation/tables.md. No game data is stored in this repository.
#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace sc2k {

struct GameTables {
    // Environment scan (indexed by XBLD tile id 0..255)
    std::array<uint8_t, 256> pollution{};      // @0x4E85A8
    std::array<uint8_t, 256> population{};     // @0x4E8638
    std::array<uint8_t, 256> landValueClass{}; // @0x4E6468 (2 = halves land value)
    // Demand
    std::array<double, 5> demandK{};           // @0x4D9CE8..0x4D9D08: 1.0, 1/150000, 0.01, 15.0, 600.0
    std::array<double, 4> econFactor{};        // @0x4E8528: indexed by MISC econIndex (0, 1.2, 1.1, 0.95)
    std::array<int16_t, 32> taxDemand{};       // @0x4E8318: demand delta by (adjusted) tax rate, 23 entries then foreign data
    // Finance
    std::array<int16_t, 16> budgetDivisor{};   // @0x4E8EA0: yearly treasury = yearToDate / (divisor * 12)

    // Throws Sc2Error if the file is not the supported build (PE timestamp check) or unreadable.
    static GameTables fromExe(const std::string& simcityExePath);
};

}  // namespace sc2k
