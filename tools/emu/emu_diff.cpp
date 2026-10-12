// sc2k-emu-diff: run original routines in the emulator and compare with the C++ ports on saved cities.
// Usage: sc2k-emu-diff power|water|env|demand|finance <SIMCITY.EXE> <city files...>
#include <cstdio>
#include <cstring>
#include <string>

#include "emu/emu.h"
#include "emu/sim_image.h"
#include "simulation/sim_context.h"
#include "save/error.h"
#include "save/misc_fields.h"
#include "simulation/environment.h"
#include "simulation/demand.h"
#include "simulation/finance.h"
#include "simulation/power.h"
#include "simulation/water.h"

using namespace sc2k;
using namespace sc2k::emu;

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
    for (int variant = 0; variant < 3; ++variant) {
        World w = World::fromCity(c);
        CityState s = CityState::fromCity(c);
        for (auto& z : w.xzon.v) if ((z & 0xF) > 6) z &= 0xF0;      // zone types 7-9 not ported yet
        if (variant >= 1) {   // push the economy around so growth, decline and decay branches all fire
            s.demand = {int16_t(variant == 1 ? 1500 : -1200), int16_t(variant == 1 ? -800 : 1900), int16_t(900), 0, 0, 0, 0, 0};
            for (int k = 10; k <= 15; ++k) s.budget[static_cast<size_t>(k)].fundingPercent = variant == 1 ? 40 : 75;
            s.population10 = variant == 1 ? 5000000 : 0;   // big-city flag on/off
        }
        const uint32_t seed = 1 + 11u * variant, lfsrSeed = 0xACE1u + 17u * variant;
        Emu e(exe);
        e.installCrtRand(seed);
        SimImage img(e);
        img.load(w, s); img.setLfsr(lfsrSeed); img.stubUi();
        for (int sweep = 0; sweep < 3; ++sweep)
            for (int a0 = 0; a0 < 4; ++a0) for (int b0 = 0; b0 < 4; ++b0) e.call(0x435220, {uint32_t(a0), uint32_t(b0)});
        World ew = w; CityState es = s;
        img.store(ew, es);

        World pw = w; CityState ps = s;
        CrtRand rng(seed); Lfsr lf(lfsrSeed); ZoneHooks hooks; TrafficStats ts;
        SimContext ctx{pw, ps, t, rng, lf, hooks, ts, {}};
        for (int sweep = 0; sweep < 3; ++sweep)
            for (int a0 = 0; a0 < 4; ++a0) for (int b0 = 0; b0 < 4; ++b0) runZonePass(ctx, a0, b0);

        std::string detail;
        long d = diffWorld(ew, pw, detail) + diffState(es, ps, detail);
        const bool randSame = e.crtRandState() == rng.state(), lfsrSame = img.lfsrState() == lf.state();
        const int32_t rideBus = e.get<int32_t>(0x4C75B8), rideRail = e.get<int32_t>(0x4C75BC), rideSub = e.get<int32_t>(0x4C7618);
        const bool rideSame = rideBus == ts.bus && rideRail == ts.rail && rideSub == ts.subway;
        long changed = 0, trafficAdded = 0, popAdded = 0;
        for (size_t i = 0; i < w.xbld.v.size(); ++i) changed += w.xbld.v[i] != ew.xbld.v[i];
        for (size_t i = 0; i < w.xtrf.v.size(); ++i) trafficAdded += int(ew.xtrf.v[i]) - int(w.xtrf.v[i]);
        for (int z = 1; z <= 7; ++z) popAdded += es.zonePop[static_cast<size_t>(z)] - s.zonePop[static_cast<size_t>(z)];
        tiles += 1; bad += (d || !randSame || !lfsrSame || !rideSame) ? 1 : 0;
        std::printf("%-34s variant %d [tiles changed %ld, traffic +%ld, zonePop +%ld, rand %s->%s] rand %s lfsr %s riders %s diffs %ld%s\n", path.c_str(), variant, changed, trafficAdded, popAdded,
                    "seed", "moved", randSame ? "same" : "DIFF", lfsrSame ? "same" : "DIFF", rideSame ? "same" : "DIFF", d, detail.c_str());
        failures += (d || !randSame || !lfsrSame || !rideSame);
    }
    return failures;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: sc2k-emu-diff power|water|env|demand|finance <SIMCITY.EXE> <files...>\n"); return 2; }
    const std::string mode = argv[1], exe = argv[2];
    long tiles = 0, bad = 0; int failures = 0;
    for (int i = 3; i < argc; ++i) {
        try {
            if (mode == "power") failures += powerDiff(exe, argv[i], tiles, bad);
            else if (mode == "water") failures += waterDiff(exe, argv[i], tiles, bad);
            else if (mode == "env") failures += envDiff(exe, argv[i], tiles, bad);
            else if (mode == "zone") failures += zoneDiff(exe, argv[i], tiles, bad);
            else if (mode == "demand") failures += demandDiff(exe, argv[i], tiles, bad);
            else if (mode == "finance") failures += financeDiff(exe, argv[i], tiles, bad);
        } catch (const std::exception& ex) { std::fprintf(stderr, "%s: %s\n", argv[i], ex.what()); ++failures; }
    }
    std::printf("%s: %d file(s) differ; differing tiles %ld of %ld\n", mode.c_str(), failures, bad, tiles);
    return failures ? 1 : 0;
}
