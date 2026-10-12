// Working copy of the map layers used by the simulation (127-index bounds checks live in the algorithms,
// as in the original). Built from a CityFile; keeps the original row-pointer indexing: layer.at(a, b) == layer[a][b].
#pragma once
#include "save/city.h"

namespace sc2k {

// XBIT flag bits (sc2kfix names; bit meanings confirmed by the scan code, see docs/simulation/map-layers-and-globals.md).
namespace xbit {
constexpr uint8_t kSaltWater = 0x01, kFlipped = 0x02, kWater = 0x04, kMark = 0x08,
                  kWatered = 0x10, kPiped = 0x20, kPowered = 0x40, kPowerable = 0x80;
}

struct World {
    Grid<uint8_t, CityFile::kMap> xbld, xzon, xbit, xund, xtxt, xter;
    Grid<uint16_t, CityFile::kMap> altm;
    std::array<uint8_t, 1200> xmic{};   // 150 microsim records x 8 bytes
    std::array<uint8_t, 6400> xlab{};   // 256 label records x 25 bytes
    std::array<uint8_t, 480> xthg{};    // 40 thing records x 12 bytes (record 0 unused)
    Grid<uint8_t, CityFile::kHalf> xtrf, xplt, xval, xcrm;
    Grid<uint8_t, CityFile::kQuarter> xplc, xfir, xpop, xrog;

    static World fromCity(const CityFile& c);
    void writeBack(CityFile& c) const;   // copies every layer back into a CityFile
};

inline bool inMap(int a, int b) { return a >= 0 && a < CityFile::kMap && b >= 0 && b < CityFile::kMap; }

}  // namespace sc2k
