// Environment scan: pollution, land value, population density, rate of growth, police/fire coverage and crime.
// Literal port of SIMCITY.EXE build .022 function 0x46A1C0 (plus coverage spreader 0x46B420/0x46B3B0).
// Stage-by-stage description: docs/simulation/environment-scan.md. Reading of the decompilation; equivalence with the
// running game is tested statistically against saved cities (docs/validation/004-*).
#pragma once
#include "simulation/tables.h"
#include "world/world.h"

namespace sc2k {

struct EnvInputs {
    uint32_t ordinances = 0;   // MISC[1000] @0x4C9A58 (bits 0x80000, 0x800, 0x10, 0x4 used here)
    int waterFlag = 0;         // MISC[1043] @0x4CA3F4 (set by the water scan)
    int c8440 = 0;             // MISC[1037] @0x4C8440 (pollution divisor term: base = waterFlag - c8440 + 4)
    int policeBase = 0;        // MISC[1039] @0x4C91F4 (police station strength = (base + 5) * funding / 2)
    int policeFunding = 0;     // budget[5].fundingPercent = MISC[479+5*27+1]
    int fireFunding = 0;       // budget[6].fundingPercent = MISC[479+6*27+1]
    // Which 128x128 byte layer the code reads at 0x4C8F70 (values 1..15 add +12 to the density accumulator).
    // UNVERIFIED identity (XTER vs XUND); default XTER.
    const Grid<uint8_t, CityFile::kMap>* layer4C8F70 = nullptr;
};

struct EnvResult {
    int developedTiles = 0;    // @0x4C83CC
    int pollutionTotal = 0;    // @0x4CB928
    int landValueTotal = 0;    // @0x4C9458
    int crimeTotal = 0;        // @0x4CA46C
    int centerRow2 = 0;        // @0x4CA470 (city centre row, 64-grid units * 2)
    int centerCol2 = 0;        // @0x4CA474
};

// Updates in place: xplt, xval, xcrm, xpop, xrog, xplc, xfir and the 0x08 "developed" marks in XBIT's top-left quadrant.
// xtrf, xbld, xzon, xbit (watered/powered/water flags) are inputs.
EnvResult runEnvironmentScan(World& w, const GameTables& t, const EnvInputs& in);

}  // namespace sc2k
