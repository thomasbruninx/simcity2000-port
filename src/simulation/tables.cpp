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
    auto sh = [&](uint32_t va, auto& arr) { std::memcpy(arr.data(), pe.bytesAt(va, arr.size() * sizeof(arr[0])).data(), arr.size() * sizeof(arr[0])); };
    sh(0x4E7560, t.zoneDestMask); sh(0x4E7550, t.dirDx); sh(0x4E7558, t.dirDy); sh(0x4E757C, t.dirNextMask);
    sh(0x4E6428, t.buildingSize); sh(0x4E6580, t.sizePop); sh(0x4E652E, t.buildBase); sh(0x4E6556, t.buildRange);
    sh(0x4E75C8, t.footprint); sh(0x4E6A58, t.shoreDx); sh(0x4E6A70, t.shoreDy); sh(0x4E6AB0, t.shoreBit); sh(0x4E6948, t.shoreType);
    sh(0x4DB328, t.rotationZoneMask); sh(0x4DB330, t.rotationCorner);
    std::memcpy(t.budgetDivisor.data(), pe.bytesAt(0x4E8EA0, 2 * t.budgetDivisor.size()).data(), 2 * t.budgetDivisor.size());
    return t;
}

}  // namespace sc2k
