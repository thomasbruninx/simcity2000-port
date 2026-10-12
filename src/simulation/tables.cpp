#include "simulation/tables.h"

#include <algorithm>
#include <cstring>

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
    for (size_t i = 0; i < t.demandK.size(); ++i) std::memcpy(&t.demandK[i], pe.bytesAt(0x4D9CE8 + 8 * static_cast<uint32_t>(i), 8).data(), 8);
    for (size_t i = 0; i < t.econFactor.size(); ++i) std::memcpy(&t.econFactor[i], pe.bytesAt(0x4E8528 + 8 * static_cast<uint32_t>(i), 8).data(), 8);
    std::memcpy(t.taxDemand.data(), pe.bytesAt(0x4E8318, 2 * t.taxDemand.size()).data(), 2 * t.taxDemand.size());
    std::memcpy(t.budgetDivisor.data(), pe.bytesAt(0x4E8EA0, 2 * t.budgetDivisor.size()).data(), 2 * t.budgetDivisor.size());
    return t;
}

}  // namespace sc2k
