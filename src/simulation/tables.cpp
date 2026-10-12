#include "simulation/tables.h"

#include <algorithm>

#include "resources/pe_image.h"
#include "save/error.h"

namespace sc2k {

GameTables GameTables::fromExe(const std::string& path) {
    const PeImage pe = PeImage::load(path);
    if (pe.timestamp() != kTimestampWin95Cd1995)
        throw Sc2Error("unsupported SIMCITY.EXE build (PE timestamp mismatch); only the 1995 CD Collection build is supported");
    GameTables t;
    auto copy = [&](uint32_t va, std::array<uint8_t, 256>& dst) {
        auto s = pe.bytesAt(va, 256);
        std::copy(s.begin(), s.end(), dst.begin());
    };
    copy(0x4E85A8, t.pollution);
    copy(0x4E8638, t.population);
    copy(0x4E6468, t.landValueClass);
    return t;
}

}  // namespace sc2k
