// sc2k-verify: recompute simulation scans from a saved city's input layers and compare with the layers the game
// itself saved (a saved city is the output of the game's last scan). Statistical evidence, see docs/validation/.
// Usage: sc2k-verify power <file>...
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include "save/city.h"
#include "save/error.h"
#include "simulation/power.h"

using namespace sc2k;

static int verifyPower(const char* path, long& totMis, long& totTiles) {
    CityFile c = CityFile::load(path);
    World w = World::fromCity(c);
    std::set<int> plants;
    for (auto v : w.xbld.v) if (v >= 0xC6 && v <= 0xCF) plants.insert(v);
    const bool random = plants.count(0xC8) || plants.count(0xCC);

    CrtRand rng;
    PowerResult r = runPowerScan(w, {}, rng);

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

int main(int argc, char** argv) {
    if (argc < 3 || std::strcmp(argv[1], "power") != 0) { std::fprintf(stderr, "usage: sc2k-verify power <file>...\n"); return 2; }
    long mis = 0, tiles = 0;
    for (int i = 2; i < argc; ++i) {
        try { verifyPower(argv[i], mis, tiles); }
        catch (const Sc2Error& e) { std::fprintf(stderr, "%s: %s\n", argv[i], e.what()); }
    }
    std::printf("deterministic-plant cities: mismatching tiles %ld of %ld saved powered tiles\n", mis, tiles);
    return 0;
}
