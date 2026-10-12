// sc2k-save-info: human-readable summary of a SimCity 2000 city/scenario file.
#include <algorithm>
#include <cstdio>

#include "save/city.h"
#include "save/error.h"

using namespace sc2k;

template <typename G>
static void layerStats(const char* name, const G& g) {
    unsigned mn = 255, mx = 0; uint64_t sum = 0;
    for (auto v : g.v) { mn = std::min<unsigned>(mn, v); mx = std::max<unsigned>(mx, v); sum += v; }
    std::printf("  %-5s %dx%d  min %3u max %3u sum %llu\n", name, G::kSize, G::kSize, mn, mx, (unsigned long long)sum);
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: sc2k-save-info <file.sc2|scn>...\n"); return 2; }
    int rc = 0;
    for (int a = 1; a < argc; ++a) {
        try {
            CityFile c = CityFile::load(argv[a]);
            std::printf("%s\n  type: %s   name: \"%s\"%s\n", argv[a], c.isScenario() ? "scenario" : "city", c.name().c_str(), c.cnam ? "" : " (no CNAM chunk)");
            layerStats("XTER", c.xter); layerStats("XBLD", c.xbld); layerStats("XZON", c.xzon); layerStats("XUND", c.xund);
            layerStats("XTXT", c.xtxt); layerStats("XBIT", c.xbit);
            layerStats("XTRF", c.xtrf); layerStats("XPLT", c.xplt); layerStats("XVAL", c.xval); layerStats("XCRM", c.xcrm);
            layerStats("XPLC", c.xplc); layerStats("XFIR", c.xfir); layerStats("XPOP", c.xpop); layerStats("XROG", c.xrog);
            unsigned wmin = 99, wmax = 0, water = 0;
            for (auto v : c.altm.v) { wmin = std::min<unsigned>(wmin, v & 31); wmax = std::max<unsigned>(wmax, v & 31); }
            for (auto v : c.xbit.v) water += (v & 4) != 0;
            std::printf("  ALTM  altitude %u..%u, water tiles (XBIT 0x04): %u\n", wmin, wmax, water);
            std::printf("  XGRP  city size (series 0) newest 12 months: ");
            for (int i = 0; i < 12; ++i) std::printf("%d ", c.xgrp[i]);
            std::printf("\n  extra chunks:");
            for (auto& e : c.extras) std::printf(" %s(%zu)", e.chunk.tag.c_str(), e.chunk.stored.size());
            std::printf("\n");
        } catch (const Sc2Error& e) {
            std::fprintf(stderr, "%s: error: %s\n", argv[a], e.what());
            rc = 1;
        }
    }
    return rc;
}
