// sc2k-verify: recompute simulation scans from a saved city's input layers and compare with the layers the game
// itself saved (a saved city is the output of the game's last scan). Statistical evidence, see docs/validation/.
// Usage: sc2k-verify power <file>...
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <cstring>
#include <set>
#include <string>

#include "save/city.h"
#include "save/error.h"
#include "save/misc_fields.h"
#include "simulation/power.h"
#include "simulation/water.h"
#include "simulation/environment.h"

using namespace sc2k;

static int verifyPower(const char* path, long& totMis, long& totTiles) {
    CityFile c = CityFile::load(path);
    World w = World::fromCity(c);
    std::set<int> plants;
    for (auto v : w.xbld.v) if (v >= 0xC6 && v <= 0xCF) plants.insert(v);
    const bool random = plants.count(0xC8) || plants.count(0xCC);

    CrtRand rng;
    PowerInputs in;
    in.ordinances = c.miscWord(misc::kOrdinances);
    in.wind = uint8_t(c.miscWord(misc::kWind));
    in.cloudiness = uint8_t(c.miscWord(misc::kCloudiness));
    PowerResult r = runPowerScan(w, in, rng);

    long falsePos = 0, falseNeg = 0, saved = 0;
    for (size_t i = 0; i < w.xbit.v.size(); ++i) {
        const bool s = (c.xbit.v[i] & xbit::kPowered) != 0, m = (w.xbit.v[i] & xbit::kPowered) != 0;
        saved += s;
        falsePos += (m && !s);
        falseNeg += (!m && s);
    }
    std::printf("%-34s saved %5ld ours %5d  +%-4ld -%-4ld cap %6d used%% %3d %s\n", path, saved, r.poweredTiles, falsePos, falseNeg, r.capacity, r.percentUsed,
                random ? "(random plants)" : "");
    if (!random) { totMis += falsePos + falseNeg; totTiles += saved; }
    return 0;
}

static int verifyWater(const char* path, long& totMis, long& totTiles) {
    CityFile c = CityFile::load(path);
    World w = World::fromCity(c);   // saved XBIT already holds power + pipes + watered flags
    WaterInputs in;
    in.rotation = c.miscInt(misc::kRotation);
    in.cloudiness = uint8_t(c.miscWord(misc::kCloudiness));
    in.waterFactor = int16_t(c.miscWord(misc::kWaterFactor));
    // Water needs "powered" for pumps: rerun the power scan first, as the game does (power phase precedes water).
    PowerInputs pin; pin.ordinances = c.miscWord(misc::kOrdinances); pin.wind = uint8_t(c.miscWord(misc::kWind)); pin.cloudiness = in.cloudiness;
    CrtRand rng; runPowerScan(w, pin, rng);
    WaterResult r = runWaterScan(w, in);
    long fp = 0, fn = 0, saved = 0;
    for (size_t i = 0; i < w.xbit.v.size(); ++i) {
        const bool s = (c.xbit.v[i] & xbit::kWatered) != 0, m = (w.xbit.v[i] & xbit::kWatered) != 0;
        saved += s; fp += (m && !s); fn += (!m && s);
    }
    if (std::getenv("SC2K_DEBUG") && (fp || fn)) {
        int r0 = 999, r1 = -1, c0 = 999, c1 = -1; std::map<int, int> ids; int piped = 0, powered = 0, n = 0;
        for (int a = 0; a < 128; ++a) for (int b = 0; b < 128; ++b) {
            const bool sv = c.xbit.at(a, b) & xbit::kWatered, m = w.xbit.at(a, b) & xbit::kWatered;
            if (sv != m) { r0 = std::min(r0, a); r1 = std::max(r1, a); c0 = std::min(c0, b); c1 = std::max(c1, b); ids[w.xbld.at(a, b)]++; piped += (w.xbit.at(a, b) & xbit::kPiped) != 0; powered += (w.xbit.at(a, b) & xbit::kPowered) != 0; ++n; }
        }
        std::printf("   diff bbox rows %d-%d cols %d-%d, n=%d piped=%d powered=%d ids:", r0, r1, c0, c1, n, piped, powered);
        int k = 0; for (auto& [id, cnt] : ids) if (k++ < 12) std::printf(" %02x:%d", id, cnt);
        std::printf("\n");
        // pumps/desal/towers/treatment present
        std::map<int, int> inf; for (int a = 0; a < 128; ++a) for (int b = 0; b < 128; ++b) { int id = w.xbld.at(a, b); if (id == 0xDC || id == 0xEB || id == 0xF4 || id == 0xFA) inf[id] += 1; }
        std::printf("   infra tiles:"); for (auto& [id, cnt] : inf) std::printf(" %02x:%d", id, cnt); std::printf("\n");
    }
    std::printf("%-34s saved %5ld ours %5d  +%-4ld -%-4ld supply %6u served%% %3d rot %d\n", path, saved, r.wateredTiles, fp, fn, r.supply, r.percent, in.rotation);
    totMis += fp + fn; totTiles += saved;
    return 0;
}

template <typename G>
static void cmpLayer(const char* name, const G& saved, const G& ours, bool print = true) {
    long same = 0, n = saved.v.size(), sumAbs = 0; int mx = 0;
    for (size_t i = 0; i < saved.v.size(); ++i) {
        int d = std::abs(int(saved.v[i]) - int(ours.v[i]));
        same += (d == 0); sumAbs += d; mx = std::max(mx, d);
    }
    if (print) std::printf("   %-5s exact %5.1f%%  mean|d| %6.3f  max|d| %3d\n", name, 100.0 * same / n, double(sumAbs) / n, mx);
}

static void verifyEnv(const char* path, const GameTables& t, int layerChoice) {
    CityFile c = CityFile::load(path);
    World w = World::fromCity(c);
    for (auto& f : w.xbit.v) f &= 0xF7;   // the power scan (phase 1) clears every mark just before this phase
    EnvInputs in;
    in.ordinances = c.miscWord(misc::kOrdinances);
    in.waterFlag = c.miscInt(misc::kWaterFlag);
    in.c8440 = c.miscInt(misc::kC8440);
    in.policeBase = int16_t(c.miscWord(misc::kPoliceBase));
    in.policeFunding = c.miscInt(misc::kFundingPercent(5));
    in.fireFunding = c.miscInt(misc::kFundingPercent(6));
    in.layer4C8F70 = layerChoice == 0 ? &w.xter : &w.xund;
    EnvResult r = runEnvironmentScan(w, t, in);
    std::printf("%s  (layer4C8F70=%s)\n", path, layerChoice == 0 ? "XTER" : "XUND");
    long markSame = 0, markN = 0;
    for (int a = 0; a < 64; ++a) for (int b = 0; b < 64; ++b) { markSame += ((w.xbit.at(a, b) & 8) == (c.xbit.at(a, b) & 8)); ++markN; }
    std::printf("   marks (developed) exact %ld/%ld   centre ours (%d,%d) saved (%d,%d)  developed %d\n", markSame, markN,
                r.centerRow2, r.centerCol2, c.miscInt(misc::kCityCenterX2), c.miscInt(misc::kCityCenterY2), r.developedTiles);
    if (std::getenv("SC2K_DEBUG")) {
        std::printf("   ordinances %#x policeBase %d policeFunding %d fireFunding %d\n", in.ordinances, in.policeBase, in.policeFunding, in.fireFunding);
        int shown = 0;
        for (int q = 0; q < 32 && shown < 12; ++q) for (int p = 0; p < 32 && shown < 12; ++p)
            if (c.xplc.at(q, p) != w.xplc.at(q, p)) { std::printf("   plc[%d][%d] saved %d ours %d | fir saved %d ours %d\n", q, p, c.xplc.at(q, p), w.xplc.at(q, p), c.xfir.at(q, p), w.xfir.at(q, p)); ++shown; }
        int stations = 0; for (int a = 0; a < 128; ++a) for (int b = 0; b < 128; ++b) { int id = w.xbld.at(a, b); if ((id == 0xD2 || id == 0xD3) && (w.xzon.at(a, b) & 0x80)) ++stations; }
        std::printf("   station anchors: %d\n", stations);
    }
    cmpLayer("XPLC", c.xplc, w.xplc); cmpLayer("XFIR", c.xfir, w.xfir); cmpLayer("XPOP", c.xpop, w.xpop); cmpLayer("XROG", c.xrog, w.xrog);
    cmpLayer("XPLT", c.xplt, w.xplt); cmpLayer("XVAL", c.xval, w.xval); cmpLayer("XCRM", c.xcrm, w.xcrm);
    std::printf("   totals ours/saved: pollution %d/%d  landvalue %d/%d  crime %d/%d\n", r.pollutionTotal, c.miscInt(misc::kPollutionTotal),
                r.landValueTotal, c.miscInt(misc::kLandValueTotal), r.crimeTotal, c.miscInt(misc::kCrimeTotal));
}

int main(int argc, char** argv) {
    if (argc >= 4 && std::strcmp(argv[1], "env") == 0) {   // sc2k-verify env <SIMCITY.EXE> <layer 0|1> <files...>
        GameTables t = GameTables::fromExe(argv[2]);
        for (int i = 4; i < argc; ++i) { try { verifyEnv(argv[i], t, std::atoi(argv[3])); } catch (const Sc2Error& e) { std::fprintf(stderr, "%s: %s\n", argv[i], e.what()); } }
        return 0;
    }
    if (argc >= 3 && std::strcmp(argv[1], "water") == 0) {
        long mis = 0, tiles = 0;
        for (int i = 2; i < argc; ++i) { try { verifyWater(argv[i], mis, tiles); } catch (const Sc2Error& e) { std::fprintf(stderr, "%s: %s\n", argv[i], e.what()); } }
        std::printf("mismatching tiles %ld of %ld saved watered tiles\n", mis, tiles);
        return 0;
    }
    if (argc < 3 || std::strcmp(argv[1], "power") != 0) { std::fprintf(stderr, "usage: sc2k-verify power|water <file>... | env <SIMCITY.EXE> <0|1> <file>...\n"); return 2; }
    long mis = 0, tiles = 0;
    for (int i = 2; i < argc; ++i) {
        try { verifyPower(argv[i], mis, tiles); }
        catch (const Sc2Error& e) { std::fprintf(stderr, "%s: %s\n", argv[i], e.what()); }
    }
    std::printf("deterministic-plant cities: mismatching tiles %ld of %ld saved powered tiles\n", mis, tiles);
    return 0;
}
