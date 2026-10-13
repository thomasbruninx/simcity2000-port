// sc2k-resources: lists the string table and extracts the bitmap resources of the user's SIMCITY.EXE (development aid).
// Usage: sc2k-resources <SIMCITY.EXE> strings | dialogs | bitmaps <outdir>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>

#include "resources/pe_resources.h"
#include "save/error.h"

using namespace sc2k;

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: sc2k-resources <SIMCITY.EXE> strings | dialogs | bitmaps <outdir>\n"); return 2; }
    try {
        const PeImage pe = PeImage::load(argv[1]);
        const PeResources res(pe);
        const std::string cmd = argv[2];
        if (cmd == "strings") {
            for (const auto& [id, s] : res.strings()) std::printf("%d\t%s\n", id, s.c_str());
        } else if (cmd == "menus") {
            std::function<void(const PeResources::MenuItem&, int)> dump = [&](const PeResources::MenuItem& m, int d) {
                for (const auto& it : m.items) {
                    std::printf("%*s%s%s  [id %d flags %02x]\n", d * 2, "", it.text.empty() ? "-------" : it.text.c_str(), "", it.id, it.flags);
                    if (!it.items.empty()) dump(it, d + 1);
                }
            };
            for (const auto& [name, m] : res.menus()) { std::printf("MENU %s\n", name.c_str()); dump(m, 1); }
        } else if (cmd == "dialogs") {
            for (const auto& [id, d] : res.dialogs()) {
                std::printf("DIALOG %d \"%s\"\n", id, d.title.c_str());
                for (const auto& c : d.controls) std::printf("  ctl %d [%s] (%d,%d %dx%d) %s\n", c.id, c.cls.c_str(), c.x, c.y, c.w, c.h, c.text.c_str());
            }
        } else if (cmd == "bitmaps" && argc > 3) {
            auto dump = [&](const std::string& name, const std::vector<uint8_t>& dib) {
                uint32_t hdr = 0, colors = 0;
                std::memcpy(&hdr, dib.data(), 4);
                uint16_t bpp = 0; std::memcpy(&bpp, dib.data() + 14, 2);
                std::memcpy(&colors, dib.data() + 32, 4);
                if (colors == 0 && bpp <= 8) colors = 1u << bpp;
                const uint32_t bits = 14 + hdr + colors * 4;
                const std::string path = std::string(argv[3]) + "/" + name + ".bmp";
                if (FILE* f = std::fopen(path.c_str(), "wb")) {
                    const uint32_t size = 14 + static_cast<uint32_t>(dib.size());
                    const uint8_t fh[14] = {'B', 'M', uint8_t(size), uint8_t(size >> 8), uint8_t(size >> 16), uint8_t(size >> 24), 0, 0, 0, 0, uint8_t(bits), uint8_t(bits >> 8), uint8_t(bits >> 16), uint8_t(bits >> 24)};
                    std::fwrite(fh, 1, 14, f); std::fwrite(dib.data(), 1, dib.size(), f); std::fclose(f);
                    std::printf("%s (%zu bytes)\n", path.c_str(), dib.size());
                }
            };
            for (int id : res.bitmapIds()) dump(std::to_string(id), res.bitmap(id));
            for (const auto& [name, dib] : res.namedBitmaps()) dump(name, dib);
        }
    } catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); return 1; }
    return 0;
}
