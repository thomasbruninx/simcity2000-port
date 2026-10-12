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
    // Zone growth / traffic
    std::array<int16_t, 16> zoneDestMask{};    // @0x4E7560: bit mask of destination zone types for a trip from zone type i
    std::array<int16_t, 4> dirDx{}, dirDy{};   // @0x4E7550 / 0x4E7558: step vectors for directions 0..3
    std::array<uint8_t, 4> dirNextMask{};      // @0x4E757C: allowed-direction mask after stepping in direction i
    std::array<int16_t, 128> buildingSize{};   // @0x4E6428: footprint size (1..4) of building id (0x70 + i)
    std::array<int16_t, 8> sizePop{};          // @0x4E6580: zone-population units by footprint size
    std::array<int16_t, 40> buildBase{};       // @0x4E652E: first building id for (size + 4*level)
    std::array<int16_t, 40> buildRange{};      // @0x4E6556: number of variants for (size + 4*level)
    std::array<int16_t, 4> rotationZoneMask{}; // @0x4DB328: XZON corner bit tested for the current view rotation
    std::array<uint8_t, 32> rotationCorner{};  // @0x4DB330: corner bits written by building stamps (byte at rotation*8 + {0,2,4,6})
    // Finance
    std::array<int16_t, 16> budgetDivisor{};   // @0x4E8EA0: yearly treasury = yearToDate / (divisor * 12)

    // Throws Sc2Error if the file is not the supported build (PE timestamp check) or unreadable.
    static GameTables fromExe(const std::string& simcityExePath);
};

}  // namespace sc2k
