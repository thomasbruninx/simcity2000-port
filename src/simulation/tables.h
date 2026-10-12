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
    // Destroy effects / shoreline
    std::array<int16_t, 256> footprint{};      // @0x4E75C8: footprint size of building id (valid for ids >= 0x70)
    std::array<int16_t, 8> shoreDx{}, shoreDy{};  // @0x4E6A58 / 0x4E6A70: the 8 neighbours examined by the shoreline fixer
    std::array<uint8_t, 8> shoreBit{};         // @0x4E6AB0: mask bit contributed by each neighbour
    std::array<uint8_t, 256> shoreType{};      // @0x4E6948: terrain tile type for a neighbour-altitude mask
    // Special buildings (military / airport / seaport placement)
    std::array<int16_t, 4> pierDx{}, pierDy{};   // @0x4E6590 / 0x4E6598: the 4 directions searched for water next to a pier
    // Thing system (ships, aircraft, trains, monsters): small direction/step tables @0x4E70D0..0x4E73E0, see docs/simulation/things.md
    std::array<int16_t, 16> dir8Dx{}, dir8Dy{};          // @0x4E70D0 / 0x4E70E0: step vectors of the 8 compass directions
    std::array<uint8_t, 24> startAlt{};                  // @0x4E70F0: initial altitude by thing type
    std::array<uint8_t, 24> speed{};                     // @0x4E71E8: sub-tile speed by movement kind
    std::array<int16_t, 4> sailDx{}, sailDy{};           // @0x4E7268 / 0x4E7270: tiles examined around a marina
    std::array<int16_t, 4> stationDx{}, stationDy{};     // @0x4E7278 / 0x4E7280: tiles examined around a rail station
    std::array<int16_t, 16> trainInitDx{}, trainInitDy{}; // @0x4E7288 / 0x4E7290: next-tile offset of a new train by heading
    std::array<uint8_t, 8> turnOffsets{};                // @0x4E72A8: heading offsets tried by the obstacle avoider
    std::array<int16_t, 16> probeDx{}, probeDy{};         // @0x4E72B0 / 0x4E72C0: 3-tile look-ahead by heading
    std::array<int16_t, 16> quadDx{}, quadDy{};           // @0x4E72D0 / 0x4E72E0: train steps by heading 0..3
    std::array<uint8_t, 32> trainTurnSprite{};           // @0x4E72F0: [heading * 4 + new heading]
    std::array<uint8_t, 8> trainSearch{};                // @0x4E7300: two 4-entry search orders
    std::array<int16_t, 16> nbDx{}, nbDy{};               // @0x4E7308 / 0x4E7310
    std::array<int16_t, 16> sailStepX{}, sailStepY{};     // @0x4E7340 / 0x4E7348: 16-unit steps per heading
    std::array<int16_t, 16> boatDx{}, boatDy{};           // @0x4E7350 / 0x4E7360: sailboat steps
    std::array<int16_t, 16> turnA{}, turnB{};             // @0x4E7370 / 0x4E7380: ship turn preferences
    std::array<int16_t, 4> pierDx2{}, pierDy2{};         // @0x4E7390 / 0x4E7398: tiles examined for a pier head
    std::array<int16_t, 16> shipDx{}, shipDy{};           // @0x4E73A0 / 0x4E73B0: ship look-ahead per heading
    std::array<uint8_t, 9> shipTiles{};                  // @0x4E73C0: XBLD ids a ship may sail through
    std::array<int16_t, 16> shipStepX{}, shipStepY{};     // @0x4E73D0 / 0x4E73E0: ship sub-tile steps (12 units per tile)
    std::array<int16_t, 80> messagePriority{};            // @0x4E7838: priority of the disaster message by burnt-building class
    std::array<uint8_t, 256> microsimClass{};             // @0x4E6C8A: microsim slot class by building id (0 = none)
    // Finance
    std::array<int16_t, 16> budgetDivisor{};   // @0x4E8EA0: yearly treasury = yearToDate / (divisor * 12)

    // Throws Sc2Error if the file is not the supported build (PE timestamp check) or unreadable.
    static GameTables fromExe(const std::string& simcityExePath);
};

}  // namespace sc2k
