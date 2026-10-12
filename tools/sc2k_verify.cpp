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

int main(int argc, char** argv) {
    if (argc >= 3 && std::strcmp(argv[1], "water") == 0) {
        long mis = 0, tiles = 0;
        for (int i = 2; i < argc; ++i) { try { verifyWater(argv[i], mis, tiles); } catch (const Sc2Error& e) { std::fprintf(stderr, "%s: %s\n", argv[i], e.what()); } }
        std::printf("mismatching tiles %ld of %ld saved watered tiles\n", mis, tiles);
        return 0;
    }
    if (argc < 3 || std::strcmp(argv[1], "power") != 0) { std::fprintf(stderr, "usage: sc2k-verify power|water <file>...\n"); return 2; }
    long mis = 0, tiles = 0;
    for (int i = 2; i < argc; ++i) {
        try { verifyPower(argv[i], mis, tiles); }
        catch (const Sc2Error& e) { std::fprintf(stderr, "%s: %s\n", argv[i], e.what()); }
    }
    std::printf("deterministic-plant cities: mismatching tiles %ld of %ld saved powered tiles\n", mis, tiles);
    return 0;
}
