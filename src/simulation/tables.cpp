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
    std::memcpy(&t.milestoneCycle, pe.bytesAt(0x4E88F4, 2).data(), 2);
    sh(0x4E6E14, t.plantCostIndex); sh(0x4DB030, t.plantCost);
    sh(0x4DAD18, t.weatherNext); sh(0x4DAE98, t.weatherHaze); sh(0x4DAEA8, t.weatherWind); sh(0x4DAEB8, t.weatherCloud); sh(0x4E5954, t.milestonePop); sh(0x4E88F8, t.disasterGrace);
    sh(0x4D9D10, t.econK); sh(0x4E8548, t.econEra); sh(0x4E8448, t.econYearTab); sh(0x4E84F8, t.econFinal);
    sh(0x4E7838, t.messagePriority); sh(0x4E6C8A, t.microsimClass);
    sh(0x4E6590, t.pierDx); sh(0x4E6598, t.pierDy);
    sh(0x4E70D0, t.dir8Dx); sh(0x4E70E0, t.dir8Dy); sh(0x4E70F0, t.startAlt); sh(0x4E71E8, t.speed);
    sh(0x4E7268, t.sailDx); sh(0x4E7270, t.sailDy); sh(0x4E7278, t.stationDx); sh(0x4E7280, t.stationDy);
    sh(0x4E7288, t.trainInitDx); sh(0x4E7290, t.trainInitDy); sh(0x4E72A8, t.turnOffsets);
    sh(0x4E72B0, t.probeDx); sh(0x4E72C0, t.probeDy); sh(0x4E72D0, t.quadDx); sh(0x4E72E0, t.quadDy);
    sh(0x4E72F0, t.trainTurnSprite); sh(0x4E7300, t.trainSearch); sh(0x4E7308, t.nbDx); sh(0x4E7310, t.nbDy);
    sh(0x4E7340, t.sailStepX); sh(0x4E7348, t.sailStepY); sh(0x4E7350, t.boatDx); sh(0x4E7360, t.boatDy);
    sh(0x4E7370, t.turnA); sh(0x4E7380, t.turnB); sh(0x4E7390, t.pierDx2); sh(0x4E7398, t.pierDy2);
    sh(0x4E73A0, t.shipDx); sh(0x4E73B0, t.shipDy); sh(0x4E73C0, t.shipTiles); sh(0x4E73D0, t.shipStepX); sh(0x4E73E0, t.shipStepY);
    std::memcpy(t.budgetDivisor.data(), pe.bytesAt(0x4E8EA0, 2 * t.budgetDivisor.size()).data(), 2 * t.budgetDivisor.size());
    return t;
}

}  // namespace sc2k
