// Constant lookup tables read from the user's SIMCITY.EXE (build .022). Addresses are virtual addresses;
// see docs/simulation/tables.md. Tables are indexed by XBLD tile id (0..255).
#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace sc2k {

struct GameTables {
    std::array<uint8_t, 256> pollution{};      // @0x4E85A8: pollution contributed by a building tile id (used for ids >= 0x70)
    std::array<uint8_t, 256> population{};     // @0x4E8638: population-density contribution of a tile id (ids 0x70..0xC5)
    std::array<uint8_t, 256> landValueClass{}; // @0x4E6468: 2 = tile halves land value (also zone-pass class, see docs)

    // Throws Sc2Error if the file is not the supported build (PE timestamp check) or unreadable.
    static GameTables fromExe(const std::string& simcityExePath);
};

}  // namespace sc2k
