// sc2k-resources: lists the string table and extracts the bitmap resources of the user's SIMCITY.EXE (development aid).
// Usage: sc2k-resources <SIMCITY.EXE> strings | bitmaps <outdir>
#include <cstdio>
#include <cstring>
#include <string>

#include "resources/pe_resources.h"
#include "save/error.h"

using namespace sc2k;

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: sc2k-resources <SIMCITY.EXE> strings | bitmaps <outdir>\n"); return 2; }
    try {
        const PeImage pe = PeImage::load(argv[1]);
        const PeResources res(pe);
        const std::string cmd = argv[2];
        if (cmd == "strings") {
            for (const auto& [id, s] : res.strings()) std::printf("%d\t%s\n", id, s.c_str());
        } else if (cmd == "bitmaps" && argc > 3) {
            for (int id : res.bitmapIds()) {
                const auto dib = res.bitmap(id);
                uint32_t hdr = 0, bits = 0, colors = 0;
                std::memcpy(&hdr, dib.data(), 4);
                uint16_t bpp = 0; std::memcpy(&bpp, dib.data() + 14, 2);
                std::memcpy(&colors, dib.data() + 32, 4);
                if (colors == 0 && bpp <= 8) colors = 1u << bpp;
                bits = 14 + hdr + colors * 4;
                const std::string path = std::string(argv[3]) + "/" + std::to_string(id) + ".bmp";
                if (FILE* f = std::fopen(path.c_str(), "wb")) {
                    const uint32_t size = 14 + static_cast<uint32_t>(dib.size());
                    const uint8_t fh[14] = {'B', 'M', uint8_t(size), uint8_t(size >> 8), uint8_t(size >> 16), uint8_t(size >> 24), 0, 0, 0, 0, uint8_t(bits), uint8_t(bits >> 8), uint8_t(bits >> 16), uint8_t(bits >> 24)};
                    std::fwrite(fh, 1, 14, f); std::fwrite(dib.data(), 1, dib.size(), f); std::fclose(f);
                    std::printf("%s (%zu bytes)\n", path.c_str(), dib.size());
                }
            }
        }
    } catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); return 1; }
    return 0;
}
