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
    // Economy model (0x4686C0)
    std::array<double, 8> econK{};                       // @0x4D9D10: 1/1200, 100, 0.75, 0.5, 0.9, 1.1, 1.2, 0.8
    std::array<int16_t, 4> econEra{};                    // @0x4E8548
    std::array<uint32_t, 44> econYearTab{};              // @0x4E8448: 4 rows x 11 sector targets (interpolated between 50-year rows)
    std::array<uint32_t, 11> econFinal{};                // @0x4E84F8: targets after the last row
    // Events / weather / milestones (phase 21-24)
    std::array<uint8_t, 384> weatherNext{};              // @0x4DAD18: next weather state [(state + 12 * season) * 8 + (rand & 7)]
    std::array<uint8_t, 16> weatherHaze{}, weatherWind{}, weatherCloud{};   // @0x4DAE98 / 0x4DAEA8 / 0x4DAEB8 (by weather state)
    std::array<uint32_t, 8> milestonePop{};              // @0x4E5954: population thresholds of the city classes
    std::array<int16_t, 4> disasterGrace{};              // @0x4E88F8: months before disasters can start, by difficulty (MISC 7)
    std::array<int16_t, 256> plantCostIndex{};           // @0x4E6E14: index into plantCost by building id
    int16_t milestoneCycle = 0;                          // @0x4E88F4
    std::array<int32_t, 16> plantCost{};                 // @0x4DB030: replacement cost of a power plant that reached the end of its life
    std::array<int16_t, 256> terrainSprite{};            // @0x4E6608: sprite (zoom-0 numbering) for each XTER value; +500 per zoom level
    // Drawing tables (renderer): things, traffic cars, palette cycling, see docs/rendering/
    std::array<int16_t, 49> waterCycle{};                // @0x4E74A8: palette entries 0xAB.. rotate by this permutation
    std::array<int16_t, 16> blinkCycle{};                // @0x4E7510: palette entries 0xE0..0xEF swap by this permutation
    std::array<uint8_t, 128> carVariant{};               // @0x4E670B: car sprite variant on the road piece with this XBLD id (0 = none)
    std::array<uint8_t, 16> heavyCar{};                  // @0x4E6778: variant used under heavy traffic
    std::array<int16_t, 24> thingSprite{};               // @0x4E7070: base sprite by thing type
    std::array<int16_t, 24> thingMinZoom{};              // @0x4E7098: smallest zoom at which the thing type is drawn
    std::array<uint8_t, 8> aircraftFrame{};              // @0x4E7108: frame offset by heading (types 1..3)
    std::array<uint32_t, 8> aircraftFlip{};              // @0x4E7110
    std::array<uint8_t, 8> boatFrame{};                  // @0x4E7130
    std::array<uint32_t, 8> boatFlip{};                  // @0x4E7138
    std::array<uint8_t, 4> subDivX{}, subDivY{};         // @0x4E7148 / 0x4E714C: sub-tile position divisor by zoom
    std::array<uint8_t, 32> railSprite{};                // @0x4E7150
    std::array<uint8_t, 8> railTurn{};                   // @0x4E7170
    std::array<uint8_t, 24> railBase{}, railFlip{}, railDx{};   // @0x4E7178 / 0x4E7190 / 0x4E71A8
    std::array<int16_t, 24> railDy{};                    // @0x4E71C0
    int16_t tornadoSprite = 0;                           // @0x4E708E
    std::array<int16_t, 16> monsterPart{};               // @0x4E7248.. (-15,-3 | -24,14 | 6,52 | 43,33 | -15,2 | -20,18 | 6,32 | 49,46)
    // Finance
    std::array<int16_t, 16> budgetDivisor{};   // @0x4E8EA0: yearly treasury = yearToDate / (divisor * 12)

    // Throws Sc2Error if the file is not the supported build (PE timestamp check) or unreadable.
    static GameTables fromExe(const std::string& simcityExePath);
};

}  // namespace sc2k
