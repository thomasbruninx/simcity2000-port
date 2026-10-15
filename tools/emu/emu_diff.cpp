// sc2k-emu-diff: run original routines in the emulator and compare with the C++ ports on saved cities.
// Usage: sc2k-emu-diff power|water|env|demand|finance|zone|destroy|things <SIMCITY.EXE> <city files...>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <vector>
#include <string>

#include "emu/emu.h"
#include "emu/sim_image.h"
#include "simulation/sim_context.h"
#include "world/world_hash.h"
#include "save/error.h"
#include "save/misc_fields.h"
#include "simulation/environment.h"
#include "simulation/demand.h"
#include "simulation/finance.h"
#include "simulation/power.h"
#include "simulation/water.h"
#include "simulation/tool_script.h"
#include "simulation/availability.h"
#include "simulation/advisors.h"
#include "simulation/terrain_gen.h"
#include "simulation/bonds.h"

using namespace sc2k;
using namespace sc2k::emu;

namespace sc2k { extern int g_econStopStage; }
namespace {
// Game globals (VA)
constexpr uint32_t T_XBLD = 0x4CB508, T_XBIT = 0x4C9B28, T_ALTM = 0x4C9E28;
constexpr uint32_t G_ORDIN = 0x4C9A58, G_WIND = 0x4C9E24, G_CLOUD = 0x4CA3EC, G_PCT_POWER = 0x4C9A68, G_CAP_POWER = 0x4C83C8;
constexpr uint32_t G_ROT = 0x4C8444, G_WATERF = 0x4C9830, G_CENSUS_PTR = 0x4C94E0, G_PCT_WATER = 0x4CB4D0, G_SUP_WATER = 0x4C83C4;

int powerDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    World w = World::fromCity(c);
    // --- original ---
    Emu e(exe);
    e.installCrtRand(1);
    auto xbld = makeLayer(e, T_XBLD, 128, 128, 1), xbit = makeLayer(e, T_XBIT, 128, 128, 1), altm = makeLayer(e, T_ALTM, 128, 128, 2);
    uploadBytes(e, xbld, w.xbld.v.data());
    uploadBytes(e, xbit, w.xbit.v.data());
    uploadShorts(e, altm, w.altm.v.data());
    e.put<uint32_t>(G_ORDIN, c.miscWord(misc::kOrdinances));
    e.put<uint8_t>(G_WIND, uint8_t(c.miscWord(misc::kWind)));
    e.put<uint8_t>(G_CLOUD, uint8_t(c.miscWord(misc::kCloudiness)));
    e.call(0x4671F0);
    std::vector<uint8_t> origBit(128 * 128);
    downloadBytes(e, xbit, origBit.data());
    const int origPct = e.get<int32_t>(G_PCT_POWER), origCap = e.get<int32_t>(G_CAP_POWER);
    // --- port ---
    PowerInputs in; in.ordinances = c.miscWord(misc::kOrdinances); in.wind = uint8_t(c.miscWord(misc::kWind)); in.cloudiness = uint8_t(c.miscWord(misc::kCloudiness));
    CrtRand rng(1);
    PowerResult r = runPowerScan(w, in, rng);
    long diff = 0;
    for (size_t i = 0; i < origBit.size(); ++i) diff += origBit[i] != w.xbit.v[i];
    tiles += 128 * 128; bad += diff;
    std::printf("%-34s xbit diff %ld  capacity orig %d port %d  pct orig %d port %d  rand state %s  insns %llu\n", path.c_str(), diff, origCap, r.capacity, origPct, r.percentUsed,
                e.crtRandState() == rng.state() ? "same" : "DIFFERENT", (unsigned long long)e.instructionCount());
    return diff != 0 || origCap != r.capacity || origPct != r.percentUsed;
}

int waterDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    World w = World::fromCity(c);
    Emu e(exe);
    e.installCrtRand(1);
    auto xbld = makeLayer(e, T_XBLD, 128, 128, 1), xbit = makeLayer(e, T_XBIT, 128, 128, 1);
    uploadBytes(e, xbld, w.xbld.v.data());
    uploadBytes(e, xbit, w.xbit.v.data());
    const uint32_t census = e.alloc(512);
    e.put<uint32_t>(G_CENSUS_PTR, census);
    e.put<uint16_t>(G_ROT, uint16_t(c.miscInt(misc::kRotation)));
    e.put<uint8_t>(G_CLOUD, uint8_t(c.miscWord(misc::kCloudiness)));
    e.put<int16_t>(G_WATERF, int16_t(c.miscWord(misc::kWaterFactor)));
    e.call(0x463EE0);
    std::vector<uint8_t> origBit(128 * 128);
    downloadBytes(e, xbit, origBit.data());
    const uint32_t origPct = e.get<uint32_t>(G_PCT_WATER), origSup = e.get<uint32_t>(G_SUP_WATER);
    WaterInputs in; in.rotation = c.miscInt(misc::kRotation); in.cloudiness = uint8_t(c.miscWord(misc::kCloudiness)); in.waterFactor = int16_t(c.miscWord(misc::kWaterFactor));
    WaterResult r = runWaterScan(w, in);
    long diff = 0;
    for (size_t i = 0; i < origBit.size(); ++i) diff += origBit[i] != w.xbit.v[i];
    tiles += 128 * 128; bad += diff;
    std::printf("%-34s xbit diff %ld  supply orig %u port %u  pct orig %u port %d\n", path.c_str(), diff, origSup, r.supply, origPct, r.percent);
    return diff != 0 || origSup != r.supply || int(origPct) != r.percent;
}
int envDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    World w = World::fromCity(c);
    for (auto& f : w.xbit.v) f &= 0xF7;   // the power scan clears every mark before this phase
    GameTables tables = GameTables::fromExe(exe);
    EnvInputs in;
    in.ordinances = c.miscWord(misc::kOrdinances); in.waterFlag = c.miscInt(misc::kWaterFlag); in.c8440 = c.miscInt(misc::kC8440);
    in.policeBase = int16_t(c.miscWord(misc::kPoliceBase)); in.policeFunding = c.miscInt(misc::kFundingPercent(5)); in.fireFunding = c.miscInt(misc::kFundingPercent(6));
    in.layer4C8F70 = &w.xter;

    Emu e(exe);
    e.installCrtRand(1);
    auto lXBLD = makeLayer(e, 0x4CB508, 128, 128, 1), lXZON = makeLayer(e, 0x4C9208, 128, 128, 1), lXBIT = makeLayer(e, 0x4C9B28, 128, 128, 1), lXTER = makeLayer(e, 0x4C8F70, 128, 128, 1);
    auto lTRF = makeLayer(e, 0x4C9958, 64, 64, 1), lPLT = makeLayer(e, 0x4C9840, 64, 64, 1), lVAL = makeLayer(e, 0x4CA0C0, 64, 64, 1), lCRM = makeLayer(e, 0x4C94F0, 64, 64, 1);
    auto lPOP = makeLayer(e, 0x4C9460, 32, 32, 1), lROG = makeLayer(e, 0x4CA040, 32, 32, 1), lPLC = makeLayer(e, 0x4C8448, 32, 32, 1), lFIR = makeLayer(e, 0x4C9A90, 32, 32, 1);
    auto lTMP = makeLayer(e, 0x4CB710, 128, 128, 2);
    uploadBytes(e, lXBLD, w.xbld.v.data()); uploadBytes(e, lXZON, w.xzon.v.data()); uploadBytes(e, lXBIT, w.xbit.v.data()); uploadBytes(e, lXTER, w.xter.v.data());
    uploadBytes(e, lTRF, w.xtrf.v.data()); uploadBytes(e, lPLT, w.xplt.v.data()); uploadBytes(e, lVAL, w.xval.v.data()); uploadBytes(e, lCRM, w.xcrm.v.data());
    uploadBytes(e, lPOP, w.xpop.v.data()); uploadBytes(e, lROG, w.xrog.v.data()); uploadBytes(e, lPLC, w.xplc.v.data()); uploadBytes(e, lFIR, w.xfir.v.data());
    e.put<uint32_t>(0x4C9A58, in.ordinances);
    e.put<int16_t>(0x4CA3F4, int16_t(in.waterFlag)); e.put<int16_t>(0x4C8440, int16_t(in.c8440)); e.put<int16_t>(0x4C91F4, int16_t(in.policeBase));
    const uint32_t budget = e.alloc(16 * 0x70);
    e.put<uint32_t>(0x4CB4E4, budget);
    e.put<int32_t>(budget + 5 * 0x70 + 0x64, in.policeFunding); e.put<int32_t>(budget + 6 * 0x70 + 0x64, in.fireFunding);
    e.call(0x46A1C0);

    EnvResult r = runEnvironmentScan(w, tables, in);
    auto cmp = [&](const char* name, const LayerRef& l, const uint8_t* ours, size_t n) {
        std::vector<uint8_t> o(n); downloadBytes(e, l, o.data());
        long d = 0; for (size_t i = 0; i < n; ++i) d += o[i] != ours[i];
        return std::pair<const char*, long>{name, d};
    };
    long total = 0;
    std::string detail;
    for (auto [name, d] : {cmp("XBIT", lXBIT, w.xbit.v.data(), 16384), cmp("XPLT", lPLT, w.xplt.v.data(), 4096), cmp("XVAL", lVAL, w.xval.v.data(), 4096), cmp("XCRM", lCRM, w.xcrm.v.data(), 4096),
                           cmp("XPOP", lPOP, w.xpop.v.data(), 1024), cmp("XROG", lROG, w.xrog.v.data(), 1024), cmp("XPLC", lPLC, w.xplc.v.data(), 1024), cmp("XFIR", lFIR, w.xfir.v.data(), 1024)}) {
        total += d; if (d) detail += std::string(" ") + name + ":" + std::to_string(d);
    }
    const int32_t oPol = e.get<int32_t>(0x4CB928), oLv = e.get<int32_t>(0x4C9458), oCr = e.get<int32_t>(0x4CA46C), oDev = e.get<int32_t>(0x4C83CC);
    const int oC1 = e.get<int16_t>(0x4CA470), oC2 = e.get<int16_t>(0x4CA474);
    const bool totalsOk = oPol == r.pollutionTotal && oLv == r.landValueTotal && oCr == r.crimeTotal && oDev == r.developedTiles && oC1 == r.centerRow2 && oC2 == r.centerCol2;
    tiles += 16384 + 3 * 4096 + 4 * 1024; bad += total;
    std::printf("%-34s layer diffs %ld%s  totals %s (pol %d/%d lv %d/%d crime %d/%d dev %d/%d centre %d,%d/%d,%d)\n", path.c_str(), total, detail.c_str(), totalsOk ? "ok" : "DIFFER",
                oPol, r.pollutionTotal, oLv, r.landValueTotal, oCr, r.crimeTotal, oDev, r.developedTiles, oC1, oC2, r.centerRow2, r.centerCol2);
    return total != 0 || !totalsOk;
}
// ---- shared: write the economy-related globals of a CityState into the emulated game ----
struct EconPtrs { uint32_t census, zonePop, c9948, budget; };
EconPtrs loadEconomy(Emu& e, const CityState& s) {
    EconPtrs p;
    p.census = e.alloc(512); p.zonePop = e.alloc(8 * 4); p.c9948 = e.alloc(16); p.budget = e.alloc(16 * 0x70);
    e.put<uint32_t>(0x4C94E0, p.census); e.put<uint32_t>(0x4C9440, p.zonePop); e.put<uint32_t>(0x4C9948, p.c9948); e.put<uint32_t>(0x4CB4E4, p.budget);
    for (int i = 0; i < 256; ++i) e.put<int16_t>(p.census + 2u * i, s.census[static_cast<size_t>(i)]);
    for (int i = 0; i < 8; ++i) e.put<int32_t>(p.zonePop + 4u * i, s.zonePop[static_cast<size_t>(i)]);
    for (int k = 0; k < 16; ++k) {
        const BudgetRecord& r = s.budget[static_cast<size_t>(k)]; const uint32_t b = p.budget + 0x70u * k;
        for (int m = 0; m < 12; ++m) { e.put<int32_t>(b + 4u * m, r.countMonth[static_cast<size_t>(m)]); e.put<int32_t>(b + 0x30 + 4u * m, r.fundMonth[static_cast<size_t>(m)]); }
        e.put<int32_t>(b + 0x60, r.currentCosts); e.put<int32_t>(b + 0x64, r.fundingPercent); e.put<int32_t>(b + 0x68, r.yearToDate);
    }
    e.put<uint32_t>(0x4C9A58, s.ordinances); e.put<int32_t>(0x4C945C, s.treasury); e.put<int32_t>(0x4CB500, s.debt); e.put<int16_t>(0x4C9434, int16_t(s.c9434));
    e.put<int32_t>(0x4CB4EC, s.cb4ec); e.put<int32_t>(0x4C84DC, s.extraPop); e.put<uint32_t>(0x4C9A8C, s.population10); e.put<int16_t>(0x4C9200, int16_t(s.c9200));
    e.put<int16_t>(0x4CA41C, int16_t(s.econIndex)); e.put<int32_t>(0x4C9608, s.popHistorySum); e.put<int32_t>(0x4C91AC, s.ratioMemory); e.put<int32_t>(0x4C9D44, s.budgetYearFlag);
    for (int i = 0; i < 3; ++i) e.put<int16_t>(0x4CB910 + 2u * i, s.demand[static_cast<size_t>(i)]);
    e.put<int16_t>(0x4CA034, int16_t(s.month())); e.put<int32_t>(0x4C9E1C, s.day);
    return p;
}

int demandDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    GameTables t = GameTables::fromExe(exe);
    int failures = 0;
    // Variant 0: state as saved. Variants 1..3: perturbed counters/tax to exercise caps and clamping.
    for (int variant = 0; variant < 4; ++variant) {
        CityState s = CityState::fromCity(c);
        DemandExtra x;
        if (variant == 0) { x.c9408 = 0; x.cb4f0 = 0; }
        if (variant >= 1) {   // synthetic "full" zone counters, as at phase 21
            const int base = 200 * variant;
            for (int i = 1; i <= 6; ++i) s.zonePop[static_cast<size_t>(i)] = base * i + (variant * 37) % 91;
            s.population10 = 5000u * variant; s.ratioMemory = 700 * variant; s.c9200 = int16_t(30 * variant - 40);
            s.demand = {int16_t(1900 * (variant & 1 ? 1 : -1)), 150, int16_t(-1990), 0, 0, 0, 0, 0};
            x.c9408 = uint16_t(5 * variant); x.cb4f0 = uint16_t(9 * variant);
            s.extraPop = 1000 * variant - 1500;
            s.budget[0].fundingPercent = 5 + variant; s.budget[1].fundingPercent = 9; s.budget[2].fundingPercent = 12 - variant;
        }
        Emu e(exe);
        e.installCrtRand(1);
        loadEconomy(e, s);
        e.put<uint16_t>(0x4CB4F0, x.cb4f0); e.put<uint16_t>(0x4C9408, x.c9408);
        e.patchRetN(0x424A20, 4);   // app/UI helper called first (thiscall, 1 stack argument)
        e.call(0x467AE0, {}, 0, 0, 0x467FB7);   // stop after the valve section (before the random-event half)

        CityState ours = s;
        DemandResult r = runDemand(ours, t, x);
        const EconPtrs ptr{e.get<uint32_t>(0x4C94E0), e.get<uint32_t>(0x4C9440), e.get<uint32_t>(0x4C9948), e.get<uint32_t>(0x4CB4E4)};
        bool ok = true; std::string why;
        auto chk = [&](bool cond, const char* what) { if (!cond) { ok = false; why += std::string(" ") + what; } };
        chk(e.get<int32_t>(ptr.zonePop) == ours.zonePop[0], "zonePop0");
        chk(e.get<uint32_t>(0x4C9A8C) == ours.population10, "pop10");
        chk(e.get<int32_t>(0x4C9608) == ours.popHistorySum, "popHistory");
        chk(e.get<int32_t>(0x4C91AC) == ours.ratioMemory, "ratioMemory");
        chk(e.get<uint32_t>(0x4CA1C0) == r.populationDecline, "decline");
        chk(e.get<uint32_t>(0x4C94E4) == r.populationGrowth, "growth");
        for (int i = 0; i < 3; ++i) {
            chk(e.get<int16_t>(0x4CB910 + 2u * i) == ours.demand[static_cast<size_t>(i)], "demand");
            chk(e.get<int32_t>(ptr.budget + 0x70u * i + 0x60) == ours.budget[static_cast<size_t>(i)].currentCosts, "budgetCost");
        }
        tiles += 1; bad += ok ? 0 : 1;
        std::printf("%-34s variant %d demand orig (%d,%d,%d) port (%d,%d,%d) %s%s\n", path.c_str(), variant, e.get<int16_t>(0x4CB910), e.get<int16_t>(0x4CB912), e.get<int16_t>(0x4CB914),
                    ours.demand[0], ours.demand[1], ours.demand[2], ok ? "ok" : "DIFFER:", why.c_str());
        failures += !ok;
    }
    return failures;
}

int financeDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    GameTables t = GameTables::fromExe(exe);
    int failures = 0;
    for (int variant = 0; variant < 24; ++variant) {
        CityState s = CityState::fromCity(c);
        if (variant >= 1) {   // exercise January settlement, December flag and the random-ordinance path
            s.treasury = 3000000; s.cb4ec = 0;
            s.day = (variant & 1) ? 300 * (variant + 2) : 300 * (variant + 2) + 25 * 11;   // January or December
            s.budgetYearFlag = (variant & 1) ? 1 : 0;
            for (int k = 0; k < 16; ++k) s.budget[static_cast<size_t>(k)].yearToDate = 12000 * (k + 1) * (k % 2 ? -1 : 1);
        }
        const uint32_t seed = 1 + static_cast<uint32_t>(variant) * 7;
        Emu e(exe);
        e.installCrtRand(seed);
        loadEconomy(e, s);
        for (uint32_t f : {0x40CED0u, 0x44DD10u, 0x47BA90u, 0x40C9A0u, 0x40DC20u}) e.patchRet(f);   // UI-only callees
        // (EnableMenuItem path needs cb4c8 != 0 and treasury < 0: keep that flag zero so no menu code runs)
        e.put<int32_t>(0x4CB4C8, 0);
        e.call(0x473980);

        CityState ours = s; CrtRand rng(seed);
        runMonthlyFinance(ours, t, rng);
        bool ok = true; std::string why;
        auto chk = [&](bool cond, const char* what) { if (!cond) { ok = false; why += std::string(" ") + what; } };
        const uint32_t bptr = e.get<uint32_t>(0x4CB4E4);
        for (int k = 0; k < 16; ++k) {
            const BudgetRecord& r = ours.budget[static_cast<size_t>(k)]; const uint32_t b = bptr + 0x70u * k;
            chk(e.get<int32_t>(b + 0x60) == r.currentCosts, "cc"); chk(e.get<int32_t>(b + 0x64) == r.fundingPercent, "pct"); chk(e.get<int32_t>(b + 0x68) == r.yearToDate, "ytd");
            for (int m = 0; m < 12; ++m) { chk(e.get<int32_t>(b + 4u * m) == r.countMonth[static_cast<size_t>(m)], "count"); chk(e.get<int32_t>(b + 0x30 + 4u * m) == r.fundMonth[static_cast<size_t>(m)], "fund"); }
        }
        chk(e.get<int32_t>(0x4C945C) == ours.treasury, "treasury");
        chk(e.get<int32_t>(0x4C9D44) == ours.budgetYearFlag, "yearFlag");
        chk(e.get<uint32_t>(0x4C9A58) == ours.ordinances, "ordinances");
        chk(e.crtRandState() == rng.state(), "randState");
        tiles += 1; bad += ok ? 0 : 1;
        std::printf("%-34s variant %d treasury orig %d port %d ordinances %#x/%#x rand %s %s%s%s\n", path.c_str(), variant, e.get<int32_t>(0x4C945C), ours.treasury,
                    e.get<uint32_t>(0x4C9A58), ours.ordinances, e.crtRandState() == rng.state() ? "same" : "DIFF", ok ? "ok" : "DIFFER:", why.c_str(), ours.ordinances != s.ordinances ? " [ordinance enacted]" : "");
        failures += !ok;
    }
    return failures;
}
int zoneDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    GameTables t = GameTables::fromExe(exe);
    int failures = 0;
    for (int variant = 0; variant < 7; ++variant) {
        World w = World::fromCity(c);
        CityState s = CityState::fromCity(c);
        if (variant >= 3) {   // paint military / airport / seaport zone blocks over clear land and cycle the military base type
            s.militaryType = variant - 1;   // 2..5
            for (int ba = 0; ba + 12 <= 128; ba += 12) for (int bb = 0; bb + 12 <= 128; bb += 12) {
                const unsigned h = static_cast<unsigned>((ba / 12) * 31 + (bb / 12) * 17 + variant) * 2654435761u >> 28;
                if (h % 3 != 0) continue;
                bool clear = true;   // the 3x3 command centre can spill 2 tiles past a zone edge, so keep a margin free of other zones
                for (int a = ba; a < ba + 12; ++a) for (int b = bb; b < bb + 12; ++b) if ((w.xzon.at(a, b) & 0xF) != 0) clear = false;
                if (!clear) continue;
                for (int a = ba + 3; a < ba + 9; ++a) for (int b = bb + 3; b < bb + 9; ++b)
                    if (w.xbld.at(a, b) <= 0x1C) w.xzon.at(a, b) = static_cast<uint8_t>((w.xzon.at(a, b) & 0xF0) | (7 + (h / 3) % 3));
            }
        }
        if (variant >= 1) {   // push the economy around so growth, decline and decay branches all fire
            s.demand = {int16_t(variant == 1 ? 1500 : -1200), int16_t(variant == 1 ? -800 : 1900), int16_t(900), 0, 0, 0, 0, 0};
            for (int k = 10; k <= 15; ++k) s.budget[static_cast<size_t>(k)].fundingPercent = variant == 1 ? 40 : 75;
            s.population10 = variant == 1 ? 5000000 : 0;   // big-city flag on/off
        }
        const uint32_t seed = 1 + 11u * variant, lfsrSeed = 0xACE1u + 17u * variant;   // variants 3..6 also paint zones 7-9
        Emu e(exe);
        e.installCrtRand(seed);
        SimImage img(e);
        img.load(w, s); img.setLfsr(lfsrSeed); img.setLcg(1); img.stubUi(); { ThingState z; img.loadThings(z, s); }
        const int sweeps = std::getenv("SWEEPS") ? std::atoi(std::getenv("SWEEPS")) : (variant >= 3 ? 12 : 3);
        for (int sweep = 0; sweep < sweeps; ++sweep)
            for (int a0 = 0; a0 < 4; ++a0) for (int b0 = 0; b0 < 4; ++b0) e.call(0x435220, {uint32_t(a0), uint32_t(b0)});
        World ew = w; CityState es = s;
        img.store(ew, es);

        World pw = w; CityState ps = s;
        CrtRand rng(seed); Lfsr lf(lfsrSeed); ZoneHooks hooks; TrafficStats ts;
        SimContext ctx{pw, ps, t, rng, lf, hooks, ts, {}};
        for (int sweep = 0; sweep < sweeps; ++sweep)
            for (int a0 = 0; a0 < 4; ++a0) for (int b0 = 0; b0 < 4; ++b0) runZonePass(ctx, a0, b0);
        if (std::getenv("LOCKSTEP") && std::atoi(std::getenv("LOCKSTEP")) == variant) {   // re-run both sides pass by pass and report the first divergence
            Emu e2(exe); e2.installCrtRand(seed); SimImage i2(e2);
            World w2 = w; CityState s2 = s; i2.load(w2, s2); i2.setLfsr(lfsrSeed); i2.stubUi();
            World qw = w; CityState qs = s; CrtRand r2(seed); Lfsr l2(lfsrSeed); TrafficStats t2; SimContext c2{qw, qs, t, r2, l2, hooks, t2, {}};
            bool found = false;
            for (int sweep = 0; sweep < sweeps && !found; ++sweep)
                for (int a0 = 0; a0 < 4 && !found; ++a0) for (int b0 = 0; b0 < 4 && !found; ++b0) {
                    e2.call(0x435220, {uint32_t(a0), uint32_t(b0)}); runZonePass(c2, a0, b0);
                    World xw = qw; CityState xs = qs; i2.store(xw, xs);
                    std::string dd; long n = diffWorld(xw, qw, dd) + diffState(xs, qs, dd);
                    if (n) { std::printf("  lockstep: first divergence at sweep %d pass (%d,%d): %s\n", sweep, a0, b0, dd.c_str()); found = true; }
                }
        }

        std::string detail;
        ThingState et; img.storeThings(et);
        long d = diffWorld(ew, pw, detail) + diffState(es, ps, detail) + diffThings(et, ctx.things, detail) + (img.lcgState() != ctx.lcg.state());
        const bool randSame = e.crtRandState() == rng.state(), lfsrSame = img.lfsrState() == lf.state();
        const int32_t rideBus = e.get<int32_t>(0x4C75B8), rideRail = e.get<int32_t>(0x4C75BC), rideSub = e.get<int32_t>(0x4C7618);
        const bool rideSame = rideBus == ts.bus && rideRail == ts.rail && rideSub == ts.subway;
        long changed = 0, trafficAdded = 0, popAdded = 0, special = 0;
        for (size_t i = 0; i < ew.xbld.v.size(); ++i) special += (ew.xzon.v[i] & 0xF) >= 7 && ew.xbld.v[i] >= 0xDD && ew.xbld.v[i] != w.xbld.v[i];
        for (size_t i = 0; i < w.xbld.v.size(); ++i) changed += w.xbld.v[i] != ew.xbld.v[i];
        for (size_t i = 0; i < w.xtrf.v.size(); ++i) trafficAdded += int(ew.xtrf.v[i]) - int(w.xtrf.v[i]);
        for (int z = 1; z <= 7; ++z) popAdded += es.zonePop[static_cast<size_t>(z)] - s.zonePop[static_cast<size_t>(z)];
        tiles += 1; bad += (d || !randSame || !lfsrSame || !rideSame) ? 1 : 0;
        std::printf("%-34s variant %d [special %ld, tiles changed %ld, traffic +%ld, zonePop +%ld, rand %s->%s] rand %s lfsr %s riders %s diffs %ld%s  hash %016llx\n", path.c_str(), variant, special, changed, trafficAdded, popAdded,
                    "seed", "moved", randSame ? "same" : "DIFF", lfsrSame ? "same" : "DIFF", rideSame ? "same" : "DIFF", d, detail.c_str(), (unsigned long long)worldFingerprint(ew, es));
        failures += (d || !randSame || !lfsrSame || !rideSame);
    }
    return failures;
}

// ---- thing system: original spawners/updater in the emulator vs. the port, frame by frame ----
struct ThingRun {
    Emu e; SimImage img; World w; CityState s; CrtRand rng; Lfsr lf; TrafficStats ts; ZoneHooks hooks; GameTables t; SimContext ctx;
    ThingRun(const std::string& exe, const GameTables& tables, const World& w0, const CityState& s0, uint32_t seed, uint32_t lfsrSeed, uint32_t lcgSeed, const ThingState& th)
        : e(exe), img((e.installCrtRand(seed), e)), w(w0), s(s0), rng(seed), lf(lfsrSeed), t(tables), ctx{w, s, t, rng, lf, hooks, ts, {}} {
        img.stubUi();
        ctx.things = th;
        ctx.lcg.seed(lcgSeed);
        img.load(w, s); img.setLfsr(lfsrSeed); img.setLcg(lcgSeed); img.loadThings(th, s);
    }
    // compare the emulated game with the port; returns number of differing items
    long compare(std::string& detail) {
        World ew = w; CityState es = s; ThingState et;
        img.store(ew, es); img.storeThings(et);
        long d = diffWorld(ew, w, detail) + diffState(es, s, detail) + diffThings(et, ctx.things, detail);
        if (ew.xthg != w.xthg) {
            for (int sl = 0; sl < 40; ++sl) {
                bool differs = false; for (int k = 0; k < 12; ++k) differs |= ew.xthg[sl * 12 + k] != w.xthg[sl * 12 + k];
                if (!differs) continue;
                char buf[200]; int n = std::snprintf(buf, sizeof buf, " [slot %d orig", sl);
                for (int k = 0; k < 12; ++k) n += std::snprintf(buf + n, sizeof buf - n, " %02x", ew.xthg[sl * 12 + k]);
                n += std::snprintf(buf + n, sizeof buf - n, " | port");
                for (int k = 0; k < 12; ++k) n += std::snprintf(buf + n, sizeof buf - n, " %02x", w.xthg[sl * 12 + k]);
                detail += buf; detail += "]";
            }
        }
        if (e.crtRandState() != rng.state()) { ++d; detail += " rand"; }
        if (img.lfsrState() != lf.state()) { ++d; detail += " lfsr"; }
        if (img.lcgState() != ctx.lcg.state()) { ++d; detail += " lcg"; }
        return d;
    }
};

int thingsDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    GameTables tables = GameTables::fromExe(exe);
    const World w0 = World::fromCity(c);
    CityState s0 = CityState::fromCity(c);
    int failures = 0;
    for (int variant = 0; variant < 4; ++variant) {
        const uint32_t seed = 3 + 7u * variant, lfsrSeed = 0xACE1u + 29u * variant, lcgSeed = 11 + 5u * variant;
        World w1 = w0;
        if (variant >= 1) {   // clear the saved things and keep the world; spawn a fresh mix through the spawners instead
            std::fill(w1.xthg.begin(), w1.xthg.end(), 0);
            for (auto& v : w1.xtxt.v) if (v >= 0xC9 && v < 0xF1) v = 0;
        }
        ThingState th0;
        // sprite heights: give buildings a height so the aircraft collision code is exercised
        th0.spriteHeight.assign(1400, 0);
        for (size_t i = 0; i < th0.spriteHeight.size(); ++i) th0.spriteHeight[i] = static_cast<uint16_t>((i * 37) % 90);
        ThingRun r(exe, tables, w1, s0, seed, lfsrSeed, lcgSeed, th0);
        std::string detail; long d = 0;
        if (variant >= 1) {
            // pick spawn spots from the map
            std::vector<std::pair<int,int>> rails, waters, lands;
            for (int a = 4; a < 124; ++a) for (int b = 4; b < 124; ++b) {
                const int id = w1.xbld.at(a, b);
                if (id >= 0x2C && id <= 0x35) rails.push_back({a, b});
                else if ((w1.xbit.at(a, b) & 4) && id == 0) waters.push_back({a, b});
                else if (id == 0 && !(w1.xbit.at(a, b) & 4)) lands.push_back({a, b});
            }
            std::vector<std::pair<int,int>> inner; for (auto& q : lands) if (q.first > 24 && q.first < 104 && q.second > 24 && q.second < 104) inner.push_back(q);
            auto pick = [&](const std::vector<std::pair<int,int>>& v, int k) { return v.empty() ? std::pair<int,int>{64, 64} : v[(static_cast<size_t>(k) * 2654435761u + variant * 97u) % v.size()]; };
            r.e.put<int32_t>(0x4C9A70, 0); (void)0;
            auto setFocus = [&](int a, int b) { r.ctx.things.focusA = a; r.ctx.things.focusB = b; r.e.put<int32_t>(0x4C9A70, a); r.e.put<int32_t>(0x4C9A74, b); };
            for (int k = 0; k < 6; ++k) {   // trains
                auto p = pick(rails, k);
                r.e.call(0x452F60, {uint32_t(p.first), uint32_t(p.second)}); spawnTrain(r.ctx, p.first, p.second);
            }
            for (int k = 0; k < 6; ++k) {   // sailboats via the marina routine on water tiles
                auto p = pick(waters, k);
                r.e.call(0x452CE0, {uint32_t(p.first), uint32_t(p.second)}); spawnSailboatsNearMarina(r.ctx, p.first, p.second);
            }
            { auto p = pick(lands, 1); r.e.call(0x452190, {uint32_t(p.first), uint32_t(p.second), 0u}); spawnPlane(r.ctx, p.first, p.second, 0); }
            { auto p = pick(lands, 2); r.e.call(0x452190, {uint32_t(p.first), uint32_t(p.second), 2u}); spawnPlane(r.ctx, p.first, p.second, 2); }
            { auto p = pick(lands, 3); r.e.call(0x452500, {uint32_t(p.first), uint32_t(p.second)}); spawnHelicopter(r.ctx, p.first, p.second); }
            { auto p = pick(waters, 9); r.e.call(0x452660, {uint32_t(p.first), uint32_t(p.second)}); spawnShip(r.ctx, p.first, p.second); }
            if (variant >= 2) {
                auto p = pick(inner, 5); setFocus(p.first, p.second);
                if (variant == 2) { r.e.call(0x45E780); spawnMonster(r.ctx); }
                if (variant == 3) { r.e.call(0x45EA00); spawnTornado(r.ctx); }
                r.e.call(0x452900, {uint32_t(variant == 2 ? 5 : 2)}); spawnVisitor(r.ctx, variant == 2 ? 5 : 2);
                auto q = pick(lands, 8);
                r.e.call(0x452BE0, {uint32_t(q.first), uint32_t(q.second), 3u, 0u, 1u}); spawnEffect(r.ctx, q.first, q.second, 3, 0, 1);
            }
            d = r.compare(detail);
            if (d) { std::printf("%-30s variant %d spawn: diffs %ld%s\n", path.c_str(), variant, d, detail.c_str()); tiles += 1; bad += 1; ++failures; continue; }
        }
        long spawned = 0; for (int sl = 1; sl < 40; ++sl) spawned += r.w.xthg[static_cast<size_t>(sl) * 12] != 0;
        const int frames = 160;
        int firstBad = -1, framesDone = 0;
        for (int f = 0; f < frames && firstBad < 0; ++f) {
            const auto before = r.w.xthg;
            try { r.e.call(0x450210); } catch (const std::exception&) { firstBad = -2; framesDone = f; break; }
            thingFrame(r.ctx);
            detail.clear();
            if (r.compare(detail)) {
                firstBad = f; d = 1;
                char buf[200]; int n = std::snprintf(buf, sizeof buf, " (before:");
                for (int sl = 1; sl < 40; ++sl) if (before[sl * 12] == 3 || before[sl * 12] == 9) { n += std::snprintf(buf + n, sizeof buf - n, " s%d", sl); for (int k = 0; k < 12; ++k) n += std::snprintf(buf + n, sizeof buf - n, "%02x", before[sl * 12 + k]); }
                detail += buf; detail += ")";
            }
        }
        if (firstBad == -2) {   // the original read outside its arrays (undefined behaviour, e.g. a thing at the map edge)
            std::printf("%-30s variant %d skipped: original faulted after %d frames (out-of-bounds read at the map edge)\n", path.c_str(), variant, framesDone);
            continue;
        }
        long active = 0; for (int sl = 1; sl < 40; ++sl) active += r.w.xthg[static_cast<size_t>(sl) * 12] != 0;
        char hb[40] = "";
        if (firstBad < 0) std::snprintf(hb, sizeof hb, " hash %016llx", (unsigned long long)thingFingerprint(r.w, r.ctx.things, r.rng.state(), r.lf.state(), r.ctx.lcg.state()));
        std::printf("%-30s variant %d things %ld->%ld frames %d %s%s%s\n", path.c_str(), variant, spawned, active, firstBad < 0 ? frames : firstBad,
                    firstBad < 0 ? "identical" : "DIVERGED at frame ", firstBad < 0 ? "" : (std::to_string(firstBad) + ":" + detail).c_str(), hb);
        tiles += 1; bad += firstBad >= 0; failures += firstBad >= 0;
    }
    return failures;
}


int economyDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    GameTables t = GameTables::fromExe(exe);
    int failures = 0;
    for (int variant = 0; variant < 6; ++variant) {
        World w = World::fromCity(c);
        CityState s = CityState::fromCity(c);
        s.catPop = {int32_t(s.zonePop[1] + s.zonePop[2]), int32_t(s.zonePop[3] + s.zonePop[4]), int32_t(s.zonePop[5] + s.zonePop[6])};
        if (s.catPop[2] == 0) s.catPop[2] = 4000;
        if (s.rateLevel == 0) s.rateLevel = 1;   // the original divides by rateLevel*25 (a zero faults the game)
        if (variant >= 1) {   // push the model into its different branches
            s.populationGrowth = variant == 1 ? 3000 : 0; s.populationDecline = variant == 2 ? 5000 : (variant == 3 ? 100 : 40);
            if (variant == 4) { s.populationGrowth = 70000; s.populationDecline = 0; }
            s.ordinances = variant == 1 ? 0x80000 | 0x40 | 0x400 : (variant == 2 ? 0x100 | 0x200 | 0x20 : 0);
            s.cb4cc = variant == 1 ? 140 : (variant == 2 ? 110 : 30);
            s.population10 = variant == 5 ? 0 : s.population10 + 50000;
            s.pollutionTotal = 300000 * variant;
            for (size_t i = 0; i < 20; ++i) if (s.jobsA[i] == 0) { s.jobsA[i] = 500 + 37 * uint32_t(i); s.jobsB[i] = 900 + 11 * uint32_t(i); s.jobsC[i] = 20000 + 700 * uint32_t(i); }
            for (size_t i = 0; i < 4; ++i) if (s.nbr9d28[i] == 0) { s.nbr9d28[i] = 2000000 + 100000 * uint32_t(i); s.nbr981c[i] = 1000000; }
            for (size_t i = 0; i < 11; ++i) if (s.sec408[i] == 0) { s.sec408[i] = 3000 + 500 * int(i); s.sec940c[i] = 40; s.sec3f8[i] = 30; }
            s.era = variant % 4; s.rateLevel = 1 + variant;
            s.nationalPop = variant == 3 ? 6000000u : 3000000u; s.gnp = variant == 4 ? 4000000u : 2000000u;
        }
        const uint32_t seed = 5 + 13u * variant, lfsrSeed = 0xACE1u + 7u * variant;
        Emu e(exe); e.installCrtRand(seed);
        SimImage img(e);
        img.load(w, s); img.setLfsr(lfsrSeed); img.stubUi();
        World pw = w; CityState ps = s;
        CrtRand rng(seed); Lfsr lf(lfsrSeed); ZoneHooks hooks; TrafficStats ts;
        SimContext ctx{pw, ps, t, rng, lf, hooks, ts, {}};
        std::string detail; long d = 0; int months = 0;
        for (; months < 40 && d == 0; ++months) {
            const int stage = std::getenv("ECONSTAGE") ? std::atoi(std::getenv("ECONSTAGE")) : 0;
            static const uint32_t stops[] = {0, 0x468EC8, 0x4690F2, 0x469228, 0x4692D6, 0x469579};
            g_econStopStage = stage;
            e.call(0x4686C0, {}, 0, 0, stops[stage]); runEconomy(ctx);
            World ew = pw; CityState es = ps; img.store(ew, es);
            d = diffState(es, ps, detail) + (e.crtRandState() != rng.state()) + (img.lfsrState() != lf.state());
            if (d && std::getenv("ECONDBG")) { for (size_t i = 0; i < 20; ++i) std::printf("  [%zu] A %u/%u B %u/%u C %u/%u\n", i, es.jobsA[i], ps.jobsA[i], es.jobsB[i], ps.jobsB[i], es.jobsC[i], ps.jobsC[i]); std::printf("  growth %u/%u decline %u/%u rand %d lfsr %d\n", es.populationGrowth, ps.populationGrowth, es.populationDecline, ps.populationDecline, int(e.crtRandState() != rng.state()), int(img.lfsrState() != lf.state())); }
        }
        tiles += 1; bad += d != 0;
        std::printf("%-30s variant %d months %d %s%s  jobsA0 %u pop %u\n", path.c_str(), variant, months, d ? "DIVERGED" : "identical", detail.c_str(), ps.jobsA[0], ps.nationalPop);
        failures += d != 0;
    }
    return failures;
}


// ---- the whole tick: SimulationTick 0x412FE0 in the emulator vs. simulationTick(), compared after every day ----
int tickDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    GameTables t = GameTables::fromExe(exe);
    int failures = 0;
    for (int variant = 0; variant < 3; ++variant) {
        World w = World::fromCity(c);
        CityState s = CityState::fromCity(c);
        s.catPop = {int32_t(s.zonePop[1] + s.zonePop[2]), int32_t(s.zonePop[3] + s.zonePop[4]), int32_t(s.zonePop[5] + s.zonePop[6])};
        if (s.rateLevel == 0) s.rateLevel = 1;
        if (s.econIndex == 0) s.econIndex = 1;   // the original divides by a difficulty table entry that is 0 for index 0 (it faults)
        if (s.population10 == 0) s.population10 = 1000;
        if (variant >= 1) {
            s.day = 300 * (variant + 3) - 1 - (variant == 2 ? 24 : 0);   // next tick is phase 0 of January (variant 1) or of a later month
            s.budgetYearFlag = variant == 1 ? 1 : 0;
            s.treasury = 3000000;
            s.ordinances = variant == 1 ? 0x40 : 0x300;
            s.weather = 4 + variant; s.cloudiness = 30; s.wind = 40;
            s.populationGrowth = 20; s.populationDecline = 10;
            if (variant == 1) for (size_t sl = 1; sl < 0x96; ++sl) { const uint8_t id = w.xmic[sl * 8]; if (id >= 0xC9 && id <= 0xCF) w.xmic[sl * 8 + 1] = 0x32; }   // power plants at the end of their life
            if (variant == 2) s.cb4ec = 1;   // replace instead of explode
        }
        const uint32_t seed = 9 + 17u * variant, lfsrSeed = 0xACE1u + 3u * variant, lcgSeed = 5 + variant;
        Emu e(exe); e.installCrtRand(seed);
        SimImage img(e);
        img.load(w, s); img.setLfsr(lfsrSeed); img.setLcg(lcgSeed); img.stubUi(); img.stubTick();
        ThingState th0; img.loadThings(th0, s);
        World pw = w; CityState ps = s;
        CrtRand rng(seed); Lfsr lf(lfsrSeed); ZoneHooks hooks; TrafficStats ts;
        SimContext ctx{pw, ps, t, rng, lf, hooks, ts, {}};
        ctx.lcg.seed(lcgSeed);
        std::string detail; int firstBad = -1; int ticks = std::getenv("TICKS") ? std::atoi(std::getenv("TICKS")) : 75;
        for (int k = 0; k < ticks && firstBad < 0; ++k) {
            try { e.call(0x412FE0); } catch (const std::exception& ex) { std::printf("%-30s variant %d tick %d: original faulted (%s)\n", path.c_str(), variant, k, ex.what()); firstBad = -2; break; }
            simulationTick(ctx);
            World ew = pw; CityState es = ps; ThingState et; img.store(ew, es); img.storeThings(et);
            detail.clear();
            long d = diffWorld(ew, pw, detail) + diffState(es, ps, detail) + diffThings(et, ctx.things, detail);
            if (e.crtRandState() != rng.state()) { ++d; detail += " rand"; }
            if (img.lfsrState() != lf.state()) { ++d; detail += " lfsr"; }
            if (img.lcgState() != ctx.lcg.state()) { ++d; detail += " lcg"; }
            if (e.get<int32_t>(0x4C75B8) != ts.bus || e.get<int32_t>(0x4C75BC) != ts.rail || e.get<int32_t>(0x4C7618) != ts.subway) { ++d; detail += " riders"; }
            if (d) {
                firstBad = k; std::printf("%-30s variant %d DIVERGED at tick %d (day %d, phase %d):%s\n", path.c_str(), variant, k, ps.day, ps.day % 25, detail.c_str());
                if (std::getenv("TICKDBG")) for (int sl = 0; sl < 150; ++sl) { bool df = false; for (int q = 0; q < 8; ++q) df |= ew.xmic[sl * 8 + q] != pw.xmic[sl * 8 + q]; if (df) { std::printf("  rec %d orig", sl); for (int q = 0; q < 8; ++q) std::printf(" %02x", ew.xmic[sl * 8 + q]); std::printf(" port"); for (int q = 0; q < 8; ++q) std::printf(" %02x", pw.xmic[sl * 8 + q]); std::printf("  (before %02x)\n", w.xmic[sl * 8]); } }
            }
        }
        if (firstBad == -1) std::printf("%-30s variant %d %d ticks identical  hash %016llx\n", path.c_str(), variant, ticks, (unsigned long long)worldFingerprint(pw, ps));
        tiles += 1; bad += firstBad != -1; failures += firstBad != -1;
    }
    return failures;
}

int destroyRun(const std::string& exe, const GameTables& t, const CityFile& c, int animate, int stride, bool perCall, std::string& report, long& calls) {
    World w = World::fromCity(c); CityState s = CityState::fromCity(c);
    const uint32_t seed = 7 + animate, lfsrSeed = 0xBEEF;
    Emu e(exe); e.installCrtRand(seed);
    SimImage img(e);
    img.load(w, s); img.setLfsr(lfsrSeed); img.stubUi();
    World pw = w; CityState ps = s;
    CrtRand rng(seed); Lfsr lf(lfsrSeed); ZoneHooks hooks; TrafficStats ts;
    SimContext ctx{pw, ps, t, rng, lf, hooks, ts, {}};
    calls = 0;
    for (int a = 0; a < 128; ++a) for (int b = 0; b < 128; ++b) {
        if (pw.xbld.at(a, b) < 6 || ((a * 7 + b * 13) % stride) != 0) continue;
        if (a < 3 || b < 3 || a > 124 || b > 124) continue;   // the original's footprint probes read outside its arrays at the border (undefined behaviour)
        const int id = pw.xbld.at(a, b);
        e.call(0x458690, {uint32_t(a), uint32_t(b), uint32_t(animate)}, img.fakeView);
        destroyTile(ctx, a, b, animate);
        ++calls;
        if (perCall) {
            World ew = pw; CityState es = ps; img.store(ew, es);
            std::string d; long n = diffWorld(ew, pw, d) + diffState(es, ps, d);
            if (n || e.crtRandState() != rng.state() || img.lfsrState() != lf.state()) {
                char buf[200]; std::snprintf(buf, sizeof buf, "first divergence after destroy(%d,%d) id %#x (call %ld):%s rand %s", a, b, id, calls, d.c_str(), e.crtRandState() == rng.state() ? "same" : "DIFF");
                report = buf; return 1;
            }
        }
    }
    World ew = pw; CityState es = ps; img.store(ew, es);
    std::string d; long n = diffWorld(ew, pw, d) + diffState(es, ps, d);
    const bool ok = !n && e.crtRandState() == rng.state() && img.lfsrState() == lf.state();
    { char hb[40]; std::snprintf(hb, sizeof hb, "ok hash %016llx", (unsigned long long)worldFingerprint(ew, es)); report = ok ? std::string(hb) : "DIVERGED:" + d + (e.crtRandState() == rng.state() ? "" : " rand"); }
    return ok ? 0 : 1;
}

// the terrain editor's sea level (0x445A60) on the real cities: raised, lowered and set to extreme levels
int seaLevelDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    GameTables t = GameTables::fromExe(exe);
    int failures = 0;
    const int base = CityState::fromCity(c).seaLevel;
    for (int level : {base + 1, base - 1, 0, 4, 12, 31}) {
        if (level < 0 || level > 31) continue;
        World w = World::fromCity(c); CityState s = CityState::fromCity(c);
        Emu e(exe); e.installCrtRand(5);
        SimImage img(e);
        img.load(w, s); img.setLfsr(0xBEEF); img.stubUi();
        World pw = w; CityState ps = s;
        CrtRand rng(5); Lfsr lf(0xBEEF); ZoneHooks hooks; TrafficStats ts;
        SimContext ctx{pw, ps, t, rng, lf, hooks, ts, {}};
        e.call(0x445A60, {uint32_t(level)}, img.fakeView);
        applySeaLevel(ctx, level);
        World ew = pw; CityState es = ps; img.store(ew, es);
        std::string d; const long n = diffWorld(ew, pw, d) + diffState(es, ps, d);
        ++tiles; bad += n ? 1 : 0; failures += n ? 1 : 0;
        std::printf("%-34s sea level %2d (was %d): %s%s\n", path.c_str(), level, base, n ? "DIFF" : "identical", d.c_str());
    }
    return failures;
}

int destroyDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    GameTables t = GameTables::fromExe(exe);
    int failures = 0;
    for (int animate : {1, 0}) {
        std::string report; long calls = 0;
        int f = destroyRun(exe, t, c, animate, 5, false, report, calls);
        if (f) { std::string r2; long c2 = 0; destroyRun(exe, t, c, animate, 5, true, r2, c2); report += " | " + r2; }
        tiles += 1; bad += f;
        std::printf("%-34s animate %d destroyed %ld tiles: %s\n", path.c_str(), animate, calls, report.c_str());
        failures += f;
    }
    return failures;
}

// ---- building tools: bulldozer, zoning, network drags, bridges and single buildings ----
struct ToolHooks : ZoneHooks {
    bool confirm(int, int = 0) override { return true; }
};

void stubTools(Emu& e) {
    e.patchRet(0x40CED0); e.patchRet(0x438E00); e.patchRet(0x43A660); e.patchRetN(0x4B1223, 12); { const uint8_t yes12[8] = {0xB8, 0x06, 0, 0, 0, 0xC2, 0x0C, 0x00}; e.write(0x4B1243, yes12, 8); } e.patchRet(0x45AFD0);
    { const uint8_t shiftKey[8] = {0x0F, 0xB6, 0x05, 0x40, 0x28, 0x00, 0x7E, 0xC3}; e.write(0x42AD10, shiftKey, 8); }   // keyboard query -> test cell
    { const uint8_t pick[6] = {0xA1, 0x60, 0x28, 0x00, 0x7E, 0xC3}; e.write(0x4445A0, pick, 6); }   // pointer -> tile lookup
    { const uint8_t noMouse[3] = {0x31, 0xC0, 0xC3}; e.write(0x4457E0, noMouse, 3); }               // no more mouse samples: loops run once
    e.patchRetN(0x424AD0, 8);
    e.patchRetN(0x424AF0, 4); e.patchRet(0x42B0C0); e.patchRetN(0x406CA0, 4); e.patchRetN(0x4A2347, 4);
    const uint8_t yes1[3] = {0xB0, 0x01, 0xC3}; e.write(0x42AD70, yes1, 3);                                  // confirm -> true
    const uint8_t idyes[8] = {0xB8, 0x06, 0, 0, 0, 0xC2, 0x0C, 0x00}; e.write(0x408600, idyes, 8);           // message box -> IDYES
    const uint8_t yes650[8] = {0xB8, 0x06, 0, 0, 0, 0xC2, 0x0C, 0x00}; e.write(0x408650, yes650, 8);   // yes/no box -> IDYES
    const uint8_t chooser[12] = {0x8B, 0x44, 0x24, 0x08, 0x0F, 0xBC, 0xC0, 0x75, 0x04, 0x66, 0xB8, 0xFF};    // bridge type = lowest allowed bit, else -1
    uint8_t full[14]; std::memcpy(full, chooser, 12); full[12] = 0xFF; full[13] = 0xC3; e.write(0x45B140, full, 14);
    {   // tunnel: skip the formatted question (CString / heap code) and continue as if the player answered Yes
        const uint32_t from = 0x44BAA6, to = 0x44BBDE, rel = to - (from + 10);
        const uint8_t jmp[10] = {0xB8, 0x06, 0, 0, 0, 0xE9, uint8_t(rel), uint8_t(rel >> 8), uint8_t(rel >> 16), uint8_t(rel >> 24)};
        e.write(from, jmp, 10);
    }
    // SEH frame set-up through FS:[0] in the single-building and tunnel routines
    for (const auto& range : {std::pair<uint32_t, uint32_t>{0x43FB20, 0x1000}, {0x44B820, 0x700}}) {
        std::vector<uint8_t> code(range.second); e.read(range.first, code.data(), code.size());
        for (size_t i = 0; i + 7 <= code.size(); ++i) {
            const uint32_t va = range.first + uint32_t(i);
            if (code[i] == 0x64 && code[i + 1] == 0xA1 && !code[i + 2] && !code[i + 3] && !code[i + 4] && !code[i + 5]) { const uint8_t p[6] = {0x31, 0xC0, 0x90, 0x90, 0x90, 0x90}; e.write(va, p, 6); }
            else if (code[i] == 0x64 && code[i + 1] == 0xA3 && !code[i + 2] && !code[i + 3] && !code[i + 4] && !code[i + 5]) { const uint8_t p[6] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90}; e.write(va, p, 6); }
            else if (code[i] == 0x64 && code[i + 1] == 0x89 && !code[i + 3] && !code[i + 4] && !code[i + 5] && !code[i + 6]) { const uint8_t p[7] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90}; e.write(va, p, 7); }
        }
    }
}

int toolsRun(const std::string& exe, const GameTables& t, const CityFile& city, int variant, int nOps, std::string& report, long& done) {
    World w = World::fromCity(city); CityState s = CityState::fromCity(city);
    s.treasury = variant == 0 ? 4000000 : 20000;
    if (s.population10 == 0) s.population10 = 1000;
    const uint32_t seed = 11 + variant, lfsrSeed = 0xACE1 + variant;
    Emu e(exe); e.installCrtRand(seed);
    SimImage img(e);
    img.load(w, s); img.setLfsr(lfsrSeed); img.stubUi(); stubTools(e);
    // USER32 rectangle helpers used by the terrain code
    e.nativeImport(0x4EED94, 0x14, [](Emu& em, uint32_t args) {   // SetRect(rect, l, t, r, b)
        const uint32_t rc = em.get<uint32_t>(args);
        for (int i = 0; i < 4; ++i) em.put<int32_t>(rc + 4u * i, em.get<int32_t>(args + 4u + 4u * i));
        return 1u;
    });
    e.nativeImport(0x4EED30, 0xC, [](Emu& em, uint32_t args) {    // UnionRect(dst, a, b): empty rectangles are ignored
        const uint32_t d = em.get<uint32_t>(args), p = em.get<uint32_t>(args + 4), q = em.get<uint32_t>(args + 8);
        int32_t a[4], b[4], o[4] = {0, 0, 0, 0};
        em.read(p, a, 16); em.read(q, b, 16);
        const bool ea = a[0] >= a[2] || a[1] >= a[3], eb = b[0] >= b[2] || b[1] >= b[3];
        if (!ea && !eb) { o[0] = std::min(a[0], b[0]); o[1] = std::min(a[1], b[1]); o[2] = std::max(a[2], b[2]); o[3] = std::max(a[3], b[3]); }
        else if (!ea) std::memcpy(o, a, 16); else if (!eb) std::memcpy(o, b, 16);
        em.write(d, o, 16);
        return ea && eb ? 0u : 1u;
    });
    World pw = w; CityState ps = s;
    CrtRand rng(seed); Lfsr lf(lfsrSeed); ToolHooks hooks; TrafficStats ts;
    SimContext ctx{pw, ps, t, rng, lf, hooks, ts, {}};
    ToolState tst;
    ToolScript script(0x1234u + 77u * uint32_t(variant), std::getenv("LEVEL") ? std::atoi(std::getenv("LEVEL")) : (std::getenv("EXTENDED") ? 1 : 0));
    static const struct { uint32_t entry, subGlobal; } netEntry[5] = {{0x449530, 0x4CB974}, {0x4498D0, 0x4CB976}, {0x449C90, 0x4CB96E}, {0x44A0A0, 0x4CB976}, {0x449F20, 0x4CB970}};
    done = 0;
    std::map<std::string, std::pair<int, int>> stat;   // op kind -> (succeeded, attempted)
    for (int opIndex = 0; opIndex < nOps; ++opIndex) {
        const ToolOp op = script.next(pw, t);
        const std::string desc = script.describe(op);
        const World prev = pw;
        if (std::getenv("OPDBG")) std::printf("  start op %d %s\n", opIndex, desc.c_str());
        int origRet = 0;
        switch (op.kind) {
            case ToolOp::Bulldoze:
                e.put<int32_t>(0x4E8E38, op.underground); e.put<uint16_t>(0x4CB968, 0);
                origRet = int(e.call(0x442C20, {uint32_t(op.a), uint32_t(op.b)}, 0) & 0xFF);
                break;
            case ToolOp::Network: {
                const auto& ne = netEntry[static_cast<int>(op.net)];
                e.put<int32_t>(0x4E8E38, 0); e.put<uint16_t>(ne.subGlobal, uint16_t(op.sub));
                uint32_t code = 0xFFFF; int cost = 0;
                if (!op.bridgeStart) {
                    e.put<int32_t>(0x4C6DA0, int(op.net));
                    if (e.call(0x448410, {uint32_t(op.a), uint32_t(op.b), uint32_t(op.ta), uint32_t(op.tb)}, img.fakeView) & 0xFF) {
                        cost = e.get<int32_t>(0x4C6D9C) * op.unit + e.get<int32_t>(0x4C6D70) * 25;
                        code = uint32_t(op.ta) + uint32_t(op.tb) * 256;
                    }
                }
                e.put<int32_t>(0x4C6D90, cost);
                e.put<uint32_t>(Emu::kRandState + 0x2800, code);   // the stub reads it (patching immediates would leave stale translated blocks)
                const uint8_t stub[8] = {0xA1, 0x00, 0x28, 0x00, 0x7E, 0xC2, 0x0C, 0x00}; e.write(0x447F90, stub, 8);
                origRet = int(e.call(ne.entry, {uint32_t(op.a), uint32_t(op.b)}, img.fakeView) & 0xFF);
                break;
            }
            case ToolOp::Build:
                origRet = int(e.call(0x43FB20, {uint32_t(op.a), uint32_t(op.b), uint32_t(op.cat), uint32_t(op.sub)}) & 0xFF);
                break;
            case ToolOp::Terrain: {
                e.put<uint16_t>(0x4CB968, uint16_t(op.sub));
                if (op.sub == 1) origRet = int(e.call(0x442BB0, {uint32_t(op.a), uint32_t(op.b), uint32_t(op.ta)}) & 0xFF);
                else if (op.sub == 2) origRet = int(e.call(0x443610, {uint32_t(op.a), uint32_t(op.b)}) & 0xFF);
                else origRet = int(e.call(0x443DF0, {uint32_t(op.a), uint32_t(op.b), 1}) & 0xFF);
                break;
            }
            case ToolOp::Hydro: e.put<uint16_t>(0x4CB96E, 3); origRet = int(e.call(0x440DB0, {uint32_t(op.a), uint32_t(op.b)}) & 0xFF); break;
            case ToolOp::Tunnel: e.put<uint16_t>(0x4CB974, uint16_t(op.sub)); origRet = int(e.call(0x44B820, {uint32_t(op.a), uint32_t(op.b)}) & 0xFF); break;
            case ToolOp::Onramp: e.put<uint16_t>(0x4CB974, uint16_t(op.sub)); origRet = int(e.call(0x441160, {uint32_t(op.a), uint32_t(op.b)}) & 0xFF); break;
            case ToolOp::Highway: {
                const int ua = op.a & ~1, ub = op.b & ~1;
                auto quadWater = [&](int a, int b) { for (int x = a; x <= a + 1; ++x) for (int y = b; y <= b + 1; ++y) if (inMap(x, y) && (pw.xbit.at(x, y) & 4)) return true; return false; };
                uint32_t code = 0xFFFF; int cost = 0;
                if (!quadWater(ua, ub)) {
                    // run the original planner (pure code) to count the 2x2 steps the interactive preview would charge for
                    e.call(0x461370, {uint32_t(ua), uint32_t(ub), uint32_t(op.ta), uint32_t(op.tb)});
                    const uint32_t cell = Emu::kRandState + 0x2900;
                    e.put<int16_t>(cell, int16_t(ua)); e.put<int16_t>(cell + 2, int16_t(ub));
                    int steps = 1;
                    std::vector<std::pair<int, int>> orig{{ua, ub}};
                    if (std::getenv("OPDBG")) std::printf("   after begin: dir %d target (%d,%d) bounds %d..%d %d..%d classify(6,108)=%x\n", e.get<int16_t>(0x4C6D78), e.get<int16_t>(0x4C6D94), e.get<int16_t>(0x4C6D98), e.get<int32_t>(0x4C6D80), e.get<int32_t>(0x4C6D88), e.get<int32_t>(0x4C6D84), e.get<int32_t>(0x4C6D8C), unsigned(e.call(0x4639D0, {6, 108}) & 0xFFFF));
                    while ((e.call(0x461460, {cell, cell + 2}) & 0xFF) != 0 && steps < 1000) { if (std::getenv("EMUTRACE") && steps == 1) Emu::dumpTrace(); ++steps; orig.emplace_back(e.get<int16_t>(cell), e.get<int16_t>(cell + 2)); }
                    if (std::getenv("OPDBG")) {
                        ToolState scratch; const PathPreview pv = previewHighway(ctx, scratch, ua, ub, op.ta, op.tb);
                        for (const auto& q : {std::make_pair(ua, ub), std::make_pair(ua + 2, ub), std::make_pair(ua, ub + 2)})
                            std::printf("   (%d,%d) classify orig %x port %x  height orig %d port %d\n", q.first, q.second, unsigned(e.call(0x4639D0, {uint32_t(q.first), uint32_t(q.second)}) & 0xFFFF), unsigned(highwayClassify(ctx, q.first, q.second) & 0xFFFF),
                                        int(int16_t(e.call(0x4638F0, {uint32_t(q.first), uint32_t(q.second)}))), highwayHeight(ctx, q.first, q.second));
                        std::printf("   orig path:"); for (auto& q : orig) std::printf(" (%d,%d)", q.first, q.second);
                        std::printf("\n   port path:"); for (auto& q : pv.tiles) std::printf(" (%d,%d)", q.first, q.second);
                        std::printf("\n");
                    }
                    cost = 100 * steps;
                    code = uint32_t(op.ta) + uint32_t(op.tb) * 256;
                    e.call(0x4488C0);   // reset the direction like the entry does at the end
                }
                e.put<int32_t>(0x4C6D90, cost);
                e.put<uint32_t>(Emu::kRandState + 0x2800, code);
                const uint8_t stub[8] = {0xA1, 0x00, 0x28, 0x00, 0x7E, 0xC2, 0x0C, 0x00}; e.write(0x460E90, stub, 8);
                origRet = int(e.call(0x461B80, {uint32_t(op.a), uint32_t(op.b)}, img.fakeView) & 0xFF);
                break;
            }
            case ToolOp::Landscape: case ToolOp::Forest: case ToolOp::Stream: case ToolOp::DispatchStart: case ToolOp::Dispatch: {
                e.put<uint8_t>(Emu::kRandState + 0x2840, uint8_t(op.ta));   // the stubbed keyboard query returns this
                if (op.kind == ToolOp::Landscape) {
                    const int cost = e.get<int32_t>(0x4DAFA0 + 4 * (12 + op.sub));
                    if (e.get<int32_t>(0x4C945C) < cost) origRet = 0;
                    else {
                        origRet = int(e.call(op.sub == 0 ? 0x43F4E0 : 0x43F7E0, {uint32_t(op.a), uint32_t(op.b)}) & 0xFF);
                        if (origRet) e.put<int32_t>(0x4C945C, e.get<int32_t>(0x4C945C) - cost);
                    }
                } else if (op.kind == ToolOp::Forest) { e.call(0x43F620, {uint32_t(op.a), uint32_t(op.b)}); origRet = 1; }
                else if (op.kind == ToolOp::Stream) { e.call(0x445DF0, {uint32_t(op.a), uint32_t(op.b), 100}); origRet = 1; }
                else if (op.kind == ToolOp::DispatchStart) { e.call(0x44F290); origRet = 1; }
                else { static const uint32_t fn[3] = {0x44F4D0, 0x44F6E0, 0x44F8F0}; e.call(fn[op.sub], {uint32_t(op.a), uint32_t(op.b)}); origRet = -1; }   // void in the original: only the state is compared
                break;
            }
            case ToolOp::SprayRaise: case ToolOp::SprayLower: case ToolOp::EditLevel: {
                e.put<uint32_t>(Emu::kRandState + 0x2860, uint32_t(op.ta) | (uint32_t(op.tb) << 8));   // the stubbed pointer-to-tile lookup (0x4445A0) returns this
                if (op.kind == ToolOp::SprayRaise) e.call(0x443400, {uint32_t(op.a), uint32_t(op.b)});
                else if (op.kind == ToolOp::SprayLower) { e.put<uint16_t>(0x4CB968, 3); e.call(0x4432B0, {uint32_t(op.a), uint32_t(op.b)}); }
                else {
                    // the level loop uses the first tile of the table as the previous position and writes a cursor into thing record 0
                    e.put<uint16_t>(0x4CB968, 1);
                    uint8_t keep[12]; e.read(e.get<uint32_t>(0x4C944C), keep, 12);
                    e.call(0x4423E0, {uint32_t(op.a), uint32_t(op.b)});
                    e.write(e.get<uint32_t>(0x4C944C), keep, 12);   // the cursor record is scratch: keep the saved value
                }
                origRet = -1;
                break;
            }
            case ToolOp::Link: origRet = int(e.call(0x441530, {uint32_t(op.a), uint32_t(op.b)}) & 0xFF); break;
            case ToolOp::Zone: {
                int count = 0;   // 0x447D00: tiles that would change zone
                for (int x = std::min(op.a, op.ta); x <= std::max(op.a, op.ta); ++x)
                    for (int y = std::min(op.b, op.tb); y <= std::max(op.b, op.tb); ++y) {
                        if (pw.xter.at(x, y) != 0) continue;
                        const int id = pw.xbld.at(x, y), z = pw.xzon.at(x, y) & 0xF;
                        if (id < 0x1D && id != 5 && id != 0xD && z != 7 && z != op.zone) ++count;
                    }
                const bool water = (pw.xbit.at(op.a, op.b) & 4) != 0;
                const uint32_t code = water ? 0xFFFF : uint32_t(op.ta) + uint32_t(op.tb) * 256;
                e.put<int32_t>(0x4C6D90, count * op.unit);
                e.put<uint32_t>(Emu::kRandState + 0x2800, code);
                const uint8_t stub[8] = {0xA1, 0x00, 0x28, 0x00, 0x7E, 0xC2, 0x0C, 0x00}; e.write(0x447910, stub, 8);
                origRet = int(e.call(0x448F70, {uint32_t(op.a), uint32_t(op.b), uint32_t(op.cat), uint32_t(op.sub)}, img.fakeView) & 0xFF);
                break;
            }
        }
        const int portRet = runToolOp(ctx, tst, op);
        ++done;
        { auto& st = stat[op.name]; st.first += origRet != 0; ++st.second; }
        World ew = pw; CityState es = ps; img.store(ew, es);
        std::string d; long n = diffWorld(ew, pw, d) + diffState(es, ps, d);
        if (std::getenv("OPDBG") && ew.xthg != pw.xthg) { for (size_t i = 0; i < ew.xthg.size(); ++i) if (ew.xthg[i] != pw.xthg[i]) { std::printf("   xthg[%zu] (rec %zu +%zu) orig %02x port %02x\n", i, i / 12, i % 12, ew.xthg[i], pw.xthg[i]); break; } }
        const bool retSame = origRet < 0 || (origRet != 0) == (portRet != 0);
        if (std::getenv("OPDBG")) std::printf("  op %d %s -> %d/%d\n", opIndex, desc.c_str(), origRet, portRet);
        if (n || !retSame || e.crtRandState() != rng.state() || img.lfsrState() != lf.state()) {
            char buf[400]; std::snprintf(buf, sizeof buf, "op %d %s: return %d/%d%s rand %s lfsr %s", opIndex, desc.c_str(), origRet, portRet, d.c_str(),
                                         e.crtRandState() == rng.state() ? "same" : "DIFF", img.lfsrState() == lf.state() ? "same" : "DIFF");
            report = buf;
            if (std::getenv("OPDBG")) {
                for (int a = 0; a < 128; ++a) for (int b = 0; b < 128; ++b) if (ew.xtxt.at(a, b) != pw.xtxt.at(a, b)) std::printf("   xtxt (%d,%d) orig %02x port %02x before %02x bld %02x/%02x ter %02x\n", a, b, ew.xtxt.at(a, b), pw.xtxt.at(a, b), prev.xtxt.at(a, b), ew.xbld.at(a, b), pw.xbld.at(a, b), pw.xter.at(a, b));
                int shown = 0;
                for (int a = 0; a < 128 && shown < 40; ++a) for (int b = 0; b < 128 && shown < 40; ++b)
                    if (ew.xund.at(a, b) != pw.xund.at(a, b) || ew.xter.at(a, b) != pw.xter.at(a, b) || ew.xbit.at(a, b) != pw.xbit.at(a, b) || ew.xbld.at(a, b) != pw.xbld.at(a, b)) {
                        ++shown;
                        std::printf("   (%d,%d) orig bld %02x und %02x ter %02x bit %02x | port bld %02x und %02x ter %02x bit %02x | before bld %02x und %02x ter %02x bit %02x alt %02x\n", a, b,
                                    ew.xbld.at(a, b), ew.xund.at(a, b), ew.xter.at(a, b), ew.xbit.at(a, b), pw.xbld.at(a, b), pw.xund.at(a, b), pw.xter.at(a, b), pw.xbit.at(a, b),
                                    prev.xbld.at(a, b), prev.xund.at(a, b), prev.xter.at(a, b), prev.xbit.at(a, b), pw.altm.at(a, b) & 0x1F);
                    }
            }
            return 1;
        }
    }
    char hb[60]; std::snprintf(hb, sizeof hb, "ok hash %016llx", (unsigned long long)worldFingerprint(pw, ps)); report = hb;
    for (const auto& [k, v] : stat) report += " " + k + " " + std::to_string(v.first) + "/" + std::to_string(v.second);
    return 0;
}


int availDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    GameTables t = GameTables::fromExe(exe);
    int failures = 0;
    for (int variant = 0; variant < 3; ++variant) {
        CityState s = CityState::fromCity(c);
        if (variant == 1) s.ordinances |= 0x20000;                      // nuclear free zone: no nuclear plant
        if (variant == 2) for (int i = 0; i < 17; ++i) s.eventYears[size_t(i)] = i % 2 ? 0 : int16_t(1900 + i);
        Emu e(exe);
        e.patchRet(0x441800); e.patchRet(0x40AD30); e.patchRet(0x40AD00); e.patchRet(0x40ACC0);
        for (int i = 0; i < 17; ++i) e.put<int16_t>(0x4CA448 + 2u * uint32_t(i), s.eventYears[size_t(i)]);
        e.put<uint32_t>(0x4C9A58, s.ordinances);
        e.call(0x469A10);
        const ToolAvailability a = ToolAvailability::fromState(s, t);
        std::string detail; int d = 0;
        for (int k = 0; k < 18; ++k) if (k != 5 && e.get<int32_t>(0x4E8A00 + 4u * uint32_t(k)) != int32_t(a.mask[size_t(k)])) { ++d; detail += " mask" + std::to_string(k); }
        if (e.get<int32_t>(0x4E8E78) != int32_t(a.plantMask)) { ++d; detail += " plants"; }
        if (e.get<int32_t>(0x4CCB44) != a.arcologies) { ++d; detail += " arco"; }
        tiles += 1; bad += d != 0; failures += d != 0;
        std::printf("%-34s variant %d %s%s\n", path.c_str(), variant, d ? "DIVERGED" : "identical", detail.c_str());
    }
    return failures;
}


int valueDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    GameTables t = GameTables::fromExe(exe);
    int failures = 0;
    for (int variant = 0; variant < 3; ++variant) {
        World w = World::fromCity(c); CityState s = CityState::fromCity(c);
        if (variant) for (size_t i = 0; i < 256; ++i) s.census[i] = int16_t((s.census[i] * (variant + 2) + int(i) * 3) & 0x3FFF);   // large / odd counts exercise the truncating divides
        Emu e(exe);
        SimImage img(e);
        img.load(w, s);
        e.call(0x469BF0);
        const int32_t orig = e.get<int32_t>(0x4C94E8), port = computeCityValue(s, t);
        const bool ok = orig == port;
        tiles += 1; bad += !ok; failures += !ok;
        std::printf("%-34s variant %d city value %d/%d %s\n", path.c_str(), variant, orig, port, ok ? "identical" : "DIVERGED");
    }
    return failures;
}

int toolsDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    GameTables t = GameTables::fromExe(exe);
    int failures = 0;
    const int nOps = std::getenv("OPS") ? std::atoi(std::getenv("OPS")) : 150;
    for (int variant = 0; variant < 2; ++variant) {
        std::string report; long done = 0;
        int f = 0;
        try { f = toolsRun(exe, t, c, variant, nOps, report, done); } catch (const std::exception& ex) { report = std::string("original faulted: ") + ex.what(); f = 1; }
        tiles += 1; bad += f;
        std::printf("%-34s variant %d ops %ld: %s\n", path.c_str(), variant, done, report.c_str());
        failures += f;
    }
    return failures;
}

// ---- disasters: the handlers of 0x45C870 and the per-tick driver 0x45F0D0 ----
int disasterRun(const std::string& exe, const GameTables& t, const CityFile& city, int variant, int nOps, std::string& report, long& done) {
    World w = World::fromCity(city); CityState s = CityState::fromCity(city);
    s.treasury = 40000;
    if (s.population10 == 0) s.population10 = 20000;
    const uint32_t seed = 0x51u + 13u * uint32_t(variant), lfsrSeed = 0x1D2Bu + uint32_t(variant);
    Emu e(exe); e.installCrtRand(seed);
    SimImage img(e);
    img.load(w, s); img.setLfsr(lfsrSeed); img.stubUi();
    for (uint32_t f : {0x40DC20u, 0x424B10u, 0x4A17AFu, 0x4A17C6u, 0x43F8D0u, 0x438E00u, 0x402C1Bu}) e.patchRet(f);
    e.patchRetN(0x424AD0, 8); e.patchRetN(0x438FF0, 8); e.patchRetN(0x406720, 4); e.patchRetN(0x406CA0, 4);
    e.patchRetN(0x476AF0, 0x1C); e.patchRetN(0x476DE0, 0x24);
    { const uint32_t v = img.fakeView; const uint8_t dc[8] = {0xB8, uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24), 0xC2, 0x04, 0x00}; e.write(0x4A9BF5, dc, 8); }   // window DC wrapper -> fake object
    { auto zero = [](Emu&, uint32_t) { return 0u; };
      e.nativeImport(0x4EED4C, 8, zero); e.nativeImport(0x4EED90, 4, zero); e.nativeImport(0x4EEDB8, 8, zero); e.nativeImport(0x4EEDC0, 4, zero);
      auto ticks = std::make_shared<uint32_t>(0);
      e.nativeImport(0x4EEAD8, 0, [ticks](Emu&, uint32_t) { *ticks += 5; return *ticks; }); }
    e.patchRetN(0x4B1243, 12); e.patchRetN(0x4B1223, 12);   // message boxes
    e.put<int32_t>(0x4C95F0, 0);   // no redraw requests
    World pw = w; CityState ps = s;
    CrtRand rng(seed); Lfsr lf(lfsrSeed); ZoneHooks hooks; TrafficStats ts;
    SimContext ctx{pw, ps, t, rng, lf, hooks, ts, {}};
    uint32_t r32 = 0x9E3779B9u * uint32_t(variant + 1);
    auto rn = [&](uint32_t n) { r32 ^= r32 << 13; r32 ^= r32 >> 17; r32 ^= r32 << 5; return r32 % n; };
    struct Kind { int id; uint32_t fn; };
    static const Kind kinds[] = {{1, 0x45E290}, {2, 0x45D460}, {3, 0x45DF50}, {4, 0x45DEF0}, {6, 0x45E490}, {9, 0x45EC20}, {10, 0x45EF30}, {0xB, 0x45DC00},
                                 {0xC, 0x45D990}, {0xD, 0x45DAE0}, {0xE, 0x45D330}, {0xF, 0x45D210}, {0x10, 0x45CE40}, {0x12, 0x45E130}, {0, 0}};
    const size_t nKinds = sizeof kinds / sizeof kinds[0];
    auto compare = [&](const std::string& what, long ro, long rp, std::string& out) {
        World ew = pw; CityState es = ps; ThingState et; img.store(ew, es); img.storeThings(et);
        std::string d; long n = diffWorld(ew, pw, d) + diffState(es, ps, d) + diffThings(et, ctx.things, d);
        if (e.crtRandState() != rng.state()) { ++n; d += " rand"; }
        if (img.lfsrState() != lf.state()) { ++n; d += " lfsr"; }
        if (ro != rp) { ++n; d += " return(" + std::to_string(ro) + "/" + std::to_string(rp) + ")"; }
        if (n) { out = what + ":" + d; return false; }
        return true;
    };
    done = 0;
    std::map<int, std::pair<int, int>> stat;
    for (int op = 0; op < nOps; ++op) {
        const Kind& k = kinds[static_cast<size_t>(op) % nKinds];
        const int fa = int(rn(128)), fb = int(rn(128));
        if (op % 5 == 4 || k.id == 0) {   // scatter markers and units, flood state
            for (int i = 0; i < 60; ++i) { const int a = int(rn(128)), b = int(rn(128)); if (pw.xtxt.at(a, b) < 0xC9) pw.xtxt.at(a, b) = uint8_t(0xFB + rn(5)); }
            for (int slot = 1; slot <= 3; ++slot) {
                static const uint8_t types[3] = {7, 8, 0xE};
                const int a = int(rn(128)), b = int(rn(128));
                if (pw.xtxt.at(a, b) != 0) continue;
                uint8_t* th = &pw.xthg[size_t(slot) * 12];
                std::fill(th, th + 12, uint8_t(0)); th[0] = types[rn(3)]; th[3] = uint8_t(a); th[4] = uint8_t(b);
                pw.xtxt.at(a, b) = uint8_t(slot + 0xC9);
            }
            ctx.things.floodTimer = int(rn(90)); ctx.things.floodBudget = int(rn(4));
            ctx.things.fireMessageKind = -1;
            img.load(pw, ps); img.loadThings(ctx.things, ps);
        }
        ctx.things.focusA = fa; ctx.things.focusB = fb; ctx.things.monsters = ctx.things.tornados = 0;
        img.loadThings(ctx.things, ps);
        long ro = 0, rp = 0;
        std::string what = "op " + std::to_string(op) + " kind " + std::to_string(k.id) + " focus (" + std::to_string(fa) + "," + std::to_string(fb) + ")";
        if (k.id != 0) {
            if (k.id == 3) { ro = e.call(k.fn) != 0; if (ro) { e.call(k.fn); e.call(k.fn); } }
            else ro = e.call(k.fn) != 0;
            rp = startDisaster(ctx, k.id) ? 1 : 0;
            std::string out;
            if (!compare(what, ro, rp, out)) { report = out; return 1; }
            auto& st = stat[k.id]; st.first += int(ro != 0); ++st.second;
        }
        for (int f = 0; f < 4; ++f) {
            ro = (e.call(0x45F0D0) & 0xFF) != 0; rp = disasterFrame(ctx) ? 1 : 0;
            std::string out;
            if (!compare(what + " frame " + std::to_string(f), ro, rp, out)) { report = out; return 1; }
        }
        ++done;
    }
    char hb[80]; std::snprintf(hb, sizeof hb, "ok hash %016llx", (unsigned long long)worldFingerprint(pw, ps));
    report = hb;
    for (const auto& [id, v] : stat) report += " k" + std::to_string(id) + " " + std::to_string(v.first) + "/" + std::to_string(v.second);
    return 0;
}

int disasterDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    GameTables t = GameTables::fromExe(exe);
    const int nOps = std::getenv("OPS") ? std::atoi(std::getenv("OPS")) : 60;
    int failures = 0;
    for (int variant = 0; variant < 2; ++variant) {
        std::string report; long done = 0;
        const int f = disasterRun(exe, t, c, variant, nOps, report, done);
        ++tiles; bad += f;
        std::printf("%-34s variant %d ops %ld: %s\n", path.c_str(), variant, done, report.c_str());
        failures += f;
    }
    return failures;
}

// ---- advisors: FUN_00419e10 (advice selection) on randomised city indicators ----
int advisorDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    GameTables t = GameTables::fromExe(exe);
    World w = World::fromCity(c); CityState s = CityState::fromCity(c);
    Emu e(exe); e.installCrtRand(99);
    SimImage img(e);
    img.load(w, s); img.setLfsr(0x1234); img.stubUi();
    const uint32_t cell = Emu::kRandState + 0x3100, fake = img.fakeView;
    { auto zero = [](Emu&, uint32_t) { return 0u; }; e.nativeImport(0x4EEDD4, 4, zero); }
    { const uint8_t zeroEax[3] = {0x31, 0xC0, 0xC3}; e.write(0x48AFFE, zeroEax, 3); }
    e.patchRetN(0x401636, 8); e.patchRet(0x401424); e.patchRet(0x4A62E3); e.patchRetN(0x4A2AD3, 4); e.patchRetN(0x401B90, 4); e.patchRetN(0x424AA0, 4);
    { const uint8_t st[10] = {0x8B, 0x44, 0x24, 0x04, 0xA3, uint8_t(cell), uint8_t(cell >> 8), uint8_t(cell >> 16), uint8_t(cell >> 24), 0xC2}; e.write(0x41A620, st, 10); const uint8_t tail[2] = {0x04, 0x00}; e.write(0x41A62A, tail, 2); }
    CrtRand rng(99); Lfsr lf(0x1234); ZoneHooks hooks; TrafficStats ts;
    World pw = w; CityState ps = s;
    SimContext ctx{pw, ps, t, rng, lf, hooks, ts, {}};
    uint32_t r32 = 0xA5A5A5A5u; auto rn = [&](uint32_t n) { r32 ^= r32 << 13; r32 ^= r32 >> 17; r32 ^= r32 << 5; return r32 % n; };
    long checked = 0; std::map<int, long> seen; int failures = 0;
    for (int trial = 0; trial < 400; ++trial) {
        ps.treasury = int(rn(400000)) - 40000; ps.population10 = rn(1500000); ps.debt = int(rn(20)); ps.rateLevel = 1 + int(rn(8)); ps.cityValue = int(rn(4000000));
        for (int i = 0; i < 3; ++i) ps.demand[size_t(i)] = int16_t(int(rn(3000)) - 1500);
        ps.ordinances = rn(1u << 20); ps.powerPct = int(rn(130)); ps.graph[5 * 52] = int(rn(60)); ps.graph[7 * 52] = int(rn(60));
        ps.budgetYearFlag = int(rn(2)); ps.day = int(rn(300 * 3)) * 1;
        for (int id : {0xD1, 0xD3, 0xD6, 0xD9}) ps.census[size_t(id)] = int16_t(rn(80));
        for (int i = 0; i < 20; ++i) ps.jobsA[size_t(i)] = rn(40000);
        for (size_t i = 0; i < 16; ++i) { ps.budget[i].currentCosts = int(rn(3000)); ps.budget[i].fundingPercent = int(rn(110)); ps.budget[i].yearToDate = int(rn(100000)); }
        img.load(pw, ps);
        for (size_t i = 0; i < 16; ++i) {   // the budget window's projections (+0x6c of each record)
            const BudgetRecord& r = ps.budget[i]; const int32_t rate = r.currentCosts * r.fundingPercent;
            e.put<int32_t>(img.budget + 0x70u * uint32_t(i) + 0x6C, ps.budgetYearFlag == 0 ? (11 - ps.month()) * rate + r.yearToDate : rate * 12);
        }
        e.put<uint32_t>(cell, 0xFFFF);
        for (int adv = 0; adv < 8; ++adv) {
            e.put<uint32_t>(fake + 0x48, uint32_t(adv));
            e.call(0x419E10, {}, fake);
            const int orig = int(e.get<uint32_t>(cell));
            const int port = advisorAdvice(ctx, adv);
            ++checked; ++seen[adv * 100 + port];
            if (orig != port || e.crtRandState() != rng.state()) {
                std::printf("%-30s trial %d advisor %d: original %d port %d rand %s\n", path.c_str(), trial, adv, orig, port, e.crtRandState() == rng.state() ? "same" : "DIFF");
                ++failures; ++bad; ++tiles; return 1;
            }
        }
    }
    // the opinion poll 0x4729F0
    e.patchRet(0x42B0C0);
    for (int trial = 0; trial < 300; ++trial) {
        ps.population10 = rn(3) == 0 ? rn(150) : rn(900000); ps.cb4cc = rn(130); ps.ca428 = rn(100); ps.c9610 = int(rn(70)); ps.approval = int(rn(100));
        for (int k : {4, 5, 6, 7}) ps.graph[size_t(k) * 52] = int(rn(120));
        ps.budget[kRes].fundingPercent = int(rn(25));
        img.load(pw, ps); e.put<int16_t>(0x4E88F4, int16_t(ps.approval));
        e.call(0x4729F0);
        const bool crossed = runOpinionPoll(ctx);
        (void)crossed;
        bool same = e.crtRandState() == rng.state() && e.get<int16_t>(0x4E88F4) == int16_t(ps.approval);
        for (int i = 0; i < 7; ++i) same = same && e.get<int16_t>(0x4C8428 + 2u * i) == ps.pollOrder[size_t(i)] && e.get<int16_t>(0x4C8418 + 2u * i) == ps.pollCount[size_t(i)];
        if (!same) { std::printf("%-30s poll trial %d differs (approval %d/%d)\n", path.c_str(), trial, int(e.get<int16_t>(0x4E88F4)), int(ps.approval)); ++tiles; ++bad; return 1; }
    }
    ++tiles;
    std::printf("%-34s %ld advice choices and 300 opinion polls identical (%zu distinct advisor/advice pairs)\n", path.c_str(), checked, seen.size());
    return failures;
}

// ---- random terrain: the whole generator 0x410430 on several parameter sets ----
int terrainGenDiff(const std::string& exe, const std::string& path, long& tiles, long& bad) {
    CityFile c = CityFile::load(path);
    GameTables t = GameTables::fromExe(exe);
    int failures = 0;
    struct P { bool coast, river; int hills, water, trees; };
    static const P sets[] = {{false, false, 20, 20, 20}, {true, false, 30, 24, 10}, {false, true, 10, 30, 30}, {true, true, 40, 12, 25}, {false, false, 0, 0, 0}, {true, true, 47, 47, 47}, {true, false, 5, 3, 47}};
    int variant = 0;
    for (const P& p : sets) {
        World w = World::fromCity(c); CityState s = CityState::fromCity(c);
        const uint32_t seed = 31 + uint32_t(variant), lcgSeed = 0x4321u + uint32_t(variant);
        Emu e(exe); e.installCrtRand(seed);
        SimImage img(e);
        img.load(w, s); img.setLfsr(0x1234); img.stubUi();
        e.put<uint32_t>(0x4CCB98, lcgSeed);
        { auto zero = [](Emu&, uint32_t) { return 0u; }; e.nativeImport(0x4EEE10, 4, zero); e.nativeImport(0x4EEDD4, 4, zero); }
        e.nativeImport(0x4EED44, 0, [](Emu&, uint32_t) { return 0u; });   // wsprintfA (cdecl, varargs): the progress text is not needed
        // MFC / dialog helpers of the progress window: no-ops with the right stack cleanup
        for (uint32_t f : {0x4A1B1Cu, 0x4A347Fu, 0x438E00u, 0x40ED80u, 0x41085Du, 0x410872u}) e.patchRet(f);
        e.patchRetN(0x4A2347, 4); e.patchRetN(0x47FDE0, 8); e.patchRetN(0x414B70, 4); e.patchRetN(0x414DF0, 4); e.patchRetN(0x414F00, 4); e.patchRetN(0x414E50, 4); e.patchRetN(0x424AA0, 4);
        // SEH frame set-up through FS:[0]
        {
            std::vector<uint8_t> code(0x500); e.read(0x410430, code.data(), code.size());
            for (size_t i = 0; i + 7 <= code.size(); ++i) {
                const uint32_t va = 0x410430 + uint32_t(i);
                if (code[i] == 0x64 && code[i + 1] == 0xA1 && !code[i + 2] && !code[i + 3] && !code[i + 4] && !code[i + 5]) { const uint8_t q[6] = {0x31, 0xC0, 0x90, 0x90, 0x90, 0x90}; e.write(va, q, 6); }
                else if (code[i] == 0x64 && code[i + 1] == 0xA3 && !code[i + 2] && !code[i + 3] && !code[i + 4] && !code[i + 5]) { const uint8_t q[6] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90}; e.write(va, q, 6); }
                else if (code[i] == 0x64 && code[i + 1] == 0x89 && !code[i + 3] && !code[i + 4] && !code[i + 5] && !code[i + 6]) { const uint8_t q[7] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90}; e.write(va, q, 7); }
            }
        }
        World pw = w; CityState ps = s;
        CrtRand rng(seed); Lfsr lf(0x1234); ZoneHooks hooks; TrafficStats ts;
        SimContext ctx{pw, ps, t, rng, lf, hooks, ts, {}};
        ctx.lcg.seed(lcgSeed);
        std::string report;
        try { e.call(0x410430, {uint32_t(p.coast), uint32_t(p.river), uint32_t(p.hills), uint32_t(p.water), uint32_t(p.trees)}, img.fakeView); }
        catch (const std::exception& ex) { std::printf("%-34s set %d: original faulted: %s\n", path.c_str(), variant, ex.what()); ++failures; ++bad; ++tiles; ++variant; continue; }
        generateTerrain(ctx, TerrainParams{p.coast, p.river, p.hills, p.water, p.trees});
        World ew = pw; CityState es = ps; img.store(ew, es);
        std::string d; long n = diffWorld(ew, pw, d) + diffState(es, ps, d);
        if (e.crtRandState() != rng.state()) { ++n; d += " rand"; }
        if (e.get<uint32_t>(0x4CCB98) != ctx.lcg.state()) { ++n; d += " lcg"; }
        if (e.get<int16_t>(0x4C9830) != int16_t(ps.seaLevel)) { ++n; d += " seaLevel"; }
        ++tiles; bad += n ? 1 : 0; failures += n ? 1 : 0;
        std::printf("%-34s coast %d river %d hills %d water %d trees %d: %s%s hash %016llx\n", path.c_str(), p.coast, p.river, p.hills, p.water, p.trees, n ? "DIFF" : "identical", d.c_str(), (unsigned long long)worldFingerprint(pw, ps));
        ++variant;
    }
    return failures;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: sc2k-emu-diff power|water|env|demand|finance|zone|destroy|things <SIMCITY.EXE> <files...>\n"); return 2; }
    if (std::getenv("OPDBG")) std::setvbuf(stdout, nullptr, _IONBF, 0);
    const std::string mode = argv[1], exe = argv[2];
    long tiles = 0, bad = 0; int failures = 0;
    for (int i = 3; i < argc; ++i) {
        try {
            if (mode == "power") failures += powerDiff(exe, argv[i], tiles, bad);
            else if (mode == "water") failures += waterDiff(exe, argv[i], tiles, bad);
            else if (mode == "env") failures += envDiff(exe, argv[i], tiles, bad);
            else if (mode == "zone") failures += zoneDiff(exe, argv[i], tiles, bad);
            else if (mode == "tick") failures += tickDiff(exe, argv[i], tiles, bad);
            else if (mode == "economy") failures += economyDiff(exe, argv[i], tiles, bad);
            else if (mode == "things") failures += thingsDiff(exe, argv[i], tiles, bad);
            else if (mode == "value") failures += valueDiff(exe, argv[i], tiles, bad);
            else if (mode == "avail") failures += availDiff(exe, argv[i], tiles, bad);
            else if (mode == "tools") failures += toolsDiff(exe, argv[i], tiles, bad);
            else if (mode == "disaster") failures += disasterDiff(exe, argv[i], tiles, bad);
            else if (mode == "advisor") failures += advisorDiff(exe, argv[i], tiles, bad);
            else if (mode == "terrain") failures += terrainGenDiff(exe, argv[i], tiles, bad);
            else if (mode == "destroy") failures += destroyDiff(exe, argv[i], tiles, bad);
            else if (mode == "sealevel") failures += seaLevelDiff(exe, argv[i], tiles, bad);
            else if (mode == "demand") failures += demandDiff(exe, argv[i], tiles, bad);
            else if (mode == "finance") failures += financeDiff(exe, argv[i], tiles, bad);
        } catch (const std::exception& ex) { std::fprintf(stderr, "%s: %s\n", argv[i], ex.what()); ++failures; }
    }
    std::printf("%s: %d file(s) differ; differing tiles %ld of %ld\n", mode.c_str(), failures, bad, tiles);
    return failures ? 1 : 0;
}
