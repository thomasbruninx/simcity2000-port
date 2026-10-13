// sc2k-render: draw a saved city to a PNG without any window (verification tool for the renderer).
// Usage: sc2k-render <SC2K dir> <city.sc2> <out.png> [zoom 0..2] [width] [height] [centerA centerB] [rotation]
#include <cstdio>
#include <cstdlib>

#include "render/map_renderer.h"
#include "resources/bmp.h"
#include "save/city.h"
#include "save/error.h"

using namespace sc2k;

int main(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: sc2k-render <SC2K dir> <city.sc2> <out.png> [zoom] [w] [h] [a b] [rotation]\n"); return 2; }
    try {
        const std::string dir = argv[1];
        CityFile city = CityFile::load(argv[2]);
        World w = World::fromCity(city);
        CityState s = CityState::fromCity(city);
        GameTables t = GameTables::fromExe(dir + "/SIMCITY.EXE");
        SpriteSet sprites = SpriteSet::load(dir);
        Bmp8 pal = Bmp8::load(dir + "/BITMAPS/PAL_MSTR.BMP");
        Camera cam;
        cam.zoom = argc > 4 ? std::atoi(argv[4]) : 2;
        const int W = argc > 5 ? std::atoi(argv[5]) : 1024, H = argc > 6 ? std::atoi(argv[6]) : 768;
        cam.rotation = argc > 9 ? std::atoi(argv[9]) : s.rotation;
        const int ca = argc > 8 ? std::atoi(argv[7]) : s.centerA2 / 2, cb = argc > 8 ? std::atoi(argv[8]) : s.centerB2 / 2;
        cam.centerOn(ca, cb, W / 2, H / 2);
        Framebuffer fb(W, H, 0xFF101820u);
        MapRenderer(t, sprites, pal.palette).draw(fb, w, s, cam);
        if (!writePng(argv[3], fb)) { std::fprintf(stderr, "cannot write %s\n", argv[3]); return 1; }
        std::printf("wrote %s (%dx%d, zoom %d, rotation %d, centre %d,%d)\n", argv[3], W, H, cam.zoom, cam.rotation, ca, cb);
    } catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); return 1; }
    return 0;
}
