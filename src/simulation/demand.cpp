#include "simulation/demand.h"

namespace sc2k {

DemandResult runDemand(CityState& s, const GameTables& t, const DemandExtra& x) {
    DemandResult res;
    const double K0 = t.demandK[0], K1 = t.demandK[1], K2 = t.demandK[2], K3 = t.demandK[3], K4 = t.demandK[4];
    auto cen = [&](int id) -> int32_t { return s.census[static_cast<size_t>(id)]; };

    // zone totals
    int32_t total = 0;
    for (int i = 1; i <= 6; ++i) total = static_cast<int32_t>(static_cast<uint32_t>(total) + static_cast<uint32_t>(s.zonePop[i]));
    s.zonePop[0] = total;
    const uint32_t newPop10 = static_cast<uint32_t>(total) * 10u;
    if (newPop10 < s.population10) res.populationDecline = s.population10 - newPop10;
    else res.populationGrowth = newPop10 - s.population10;
    s.population10 = newPop10;
    s.popHistorySum = static_cast<int32_t>(static_cast<uint32_t>(s.popHistorySum) + newPop10);

    // category populations and the commercial+industrial sum (double, FILD qword of zero-extended values)
    int32_t cat[3];
    double comInd = 0.0;
    for (int i = 0; i < 3; ++i) {
        const uint32_t a = static_cast<uint32_t>(s.zonePop[1 + 2 * i]), b = static_cast<uint32_t>(s.zonePop[2 + 2 * i]);
        cat[i] = static_cast<int32_t>(a + b);
        if (i == 1 || i == 2) comInd = (static_cast<double>(a) + static_cast<double>(b)) + comInd;
    }

    const double ratio = static_cast<double>(s.ratioMemory) / (1.0 + comInd);
    s.ratioMemory = cat[0];
    const int32_t q = cat[0] / 50;
    double R = static_cast<double>(q) + comInd;
    const double indPop = static_cast<double>(static_cast<uint32_t>(cat[2]));
    const double p50 = static_cast<double>(newPop10 + 50000u);
    const double c92 = static_cast<double>(static_cast<int16_t>(s.c9200));
    double X = ((p50 * K1) * ratio) * indPop;
    const double Y = (c92 * K2 + t.econFactor[static_cast<size_t>(s.econIndex) & 3]) * ratio;
    double Z = indPop * Y;
    if (Z < K3) Z = 15.0;

    // caps from the tile census
    const int32_t l1 = ((static_cast<int32_t>(static_cast<int16_t>(cen(0xD5) / 3))) + cen(0xD7) + cen(0xDA) + cen(0xF8) + 10) * 150;
    if (static_cast<double>(l1) < R) R = static_cast<double>(l1);
    const uint32_t l2 = static_cast<uint32_t>(cat[1]) * 4u + 500u;
    if (static_cast<double>(l2) < R) R = static_cast<double>(l2);
    const int32_t q3 = (cen(0xDD) + cen(0xDE) + static_cast<int32_t>(x.cb4f0)) / 5;
    const int32_t l3 = (q3 * 4 + 4) * 375;
    if (static_cast<double>(l3) < X) X = static_cast<double>(l3);
    const int32_t l4 = ((cen(0xE0) + static_cast<int32_t>(x.c9408) + 1) * 4) * 375;
    if (static_cast<double>(l4) < Z) Z = static_cast<double>(l4);

    const double D[3] = {R, X, Z};
    for (int i = 0; i < 3; ++i) {
        const uint32_t pop = static_cast<uint32_t>(cat[i]);
        s.budget[static_cast<size_t>(i)].currentCosts = static_cast<int32_t>(pop * 10u);
        res.numerator[i] = D[i];
        res.valve[i] = D[i] / static_cast<double>(static_cast<uint64_t>(pop) + 1) - K0;
    }
    s.budget[kRes].currentCosts += s.extraPop / 6;
    s.budget[kCom].currentCosts += s.extraPop / 12;
    s.budget[kInd].currentCosts += s.extraPop / 12;

    // tax/ordinance adjusted tax index -> demand delta; accumulate into the 16-bit valves, clamp to +-2000
    for (int i = 0; i < 3; ++i) {
        int tax = s.budget[static_cast<size_t>(i)].fundingPercent;
        const uint32_t o = s.ordinances;
        if (i == 0) { if (o & 2) ++tax; if (o & 0x4000) --tax; }
        else if (i == 1) { if (o & 1) ++tax; if (o & 0x1000) --tax; if (o & 0x8000) --tax; if (o & 0x40000) --tax; }
        else { if (o & 0x2000) --tax; if (o & 0x80000) ++tax; }
        if (tax < 0) tax = 0;
        res.taxIndex[i] = tax;
        const int16_t tableVal = t.taxDemand[static_cast<size_t>(tax) & 31];
        const double v = res.valve[i] * K4 + static_cast<double>(tableVal);
        const int32_t delta = static_cast<int32_t>(v);                       // x87 _ftol: truncate toward zero
        int16_t d = static_cast<int16_t>(static_cast<int16_t>(delta) + s.demand[static_cast<size_t>(i)]);  // 16-bit add (wraps)
        if (d < -2000) d = -2000; else if (d > 2000) d = 2000;
        s.demand[static_cast<size_t>(i)] = d;
    }
    return res;
}

}  // namespace sc2k
