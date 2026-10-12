// sc2k-sim: headless simulation of a saved city.
// Usage: sc2k-sim <SIMCITY.EXE> <city.sc2> <days> [thing-frames-per-day] [out.sc2] [seed]
// Prints one line per simulated month; the optional output file is a normal city file (serialized by libsc2).
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <string>

#include "save/city.h"
#include "save/error.h"
#include "simulation/simulation.h"

using namespace sc2k;

int main(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: sc2k-sim <SIMCITY.EXE> <city.sc2> <days> [frames-per-day] [out.sc2] [seed]\n"); return 2; }
    try {
        CityFile city = CityFile::load(argv[2]);
        GameTables tables = GameTables::fromExe(argv[1]);
        const int days = std::atoi(argv[3]);
        const int frames = argc > 4 ? std::atoi(argv[4]) : 8;
        const uint32_t seed = argc > 6 ? static_cast<uint32_t>(std::strtoul(argv[6], nullptr, 10)) : 1u;
        Simulation sim(city, std::move(tables), seed);
        std::printf("day-start %d, %d days, %d thing frames/day\n", sim.state().day, days, frames);
        for (int d = 0; d < days; ++d) {
            sim.advanceDays(1, frames);
            const CityState& s = sim.state();
            if (s.day % 25 == 0)
                std::printf("day %7d  y%4d m%2d  pop %9u  treasury %10d  demand R%5d C%5d I%5d  power %3d%% water %3d%%  things %d\n", s.day, s.startYear + s.day / 300, s.month() + 1,
                            s.population10, s.treasury, s.demand[0], s.demand[1], s.demand[2], s.powerPct, s.waterPct,
                            [&] { int n = 0; for (int i = 1; i < 0x28; ++i) n += sim.world().xthg[static_cast<size_t>(i) * 12] != 0; return n; }());
        }
        std::printf("fingerprint %016llx\n", static_cast<unsigned long long>(sim.fingerprint()));
        if (argc > 5) {
            CityFile out = sim.toCity(city);
            const std::vector<uint8_t> bytes = out.serialize();
            std::ofstream f(argv[5], std::ios::binary);
            f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            std::printf("wrote %s\n", argv[5]);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    return 0;
}
