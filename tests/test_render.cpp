#include "test.h"
#include "render/map_renderer.h"
#include "resources/bmp.h"
#include "save/city.h"

#include <cstdlib>
#include <string>

using namespace sc2k;

namespace {
uint64_t fnv(const Framebuffer& fb) {
    uint64_t h = 1469598103934665603ull;
    for (uint32_t p : fb.pixels) for (int s = 0; s < 32; s += 8) { h ^= (p >> s) & 0xFF; h *= 1099511628211ull; }
    return h;
}
}  // namespace

TEST(render_camera_centering_round_trips_through_the_projection) {
    Camera c;
    c.zoom = 2; c.rotation = 0;
    c.centerOn(40, 30, 500, 300);
    // tile (40,30): x = (40-30)*16 + scrollX = 500 - 16, bottom y = 70*8 + scrollY = 300
    CHECK(c.scrollX + 10 * 16 == 500 - 16 && c.scrollY + 70 * 8 == 300);
}

TEST(render_sprites_and_map_match_the_recorded_output) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    const std::string dir = root;
    SpriteSet sprites = SpriteSet::load(dir);
    const Sprite* flat = sprites.get(1256);        // flat land tile of the large set: 32 x 17
    CHECK(flat && flat->width == 32 && flat->height == 17);
    CHECK(sprites.get(1) != nullptr && sprites.get(100000) == nullptr);
    Bmp8 pal = Bmp8::load(dir + "/BITMAPS/PAL_MSTR.BMP");
    CHECK(pal.palette[0] == 0xFF000000u);
    CityFile city = CityFile::load(dir + "/CITIES/NYC.SC2");
    World w = World::fromCity(city);
    CityState s = CityState::fromCity(city);
    GameTables t = GameTables::fromExe(dir + "/SIMCITY.EXE");
    Camera cam; cam.centerOn(34, 32, 320, 200);
    Framebuffer fb(640, 400, 0xFF101820u);
    MapRenderer(t, sprites, pal.palette).draw(fb, w, s, cam);
    size_t drawn = 0;
    for (uint32_t p : fb.pixels) drawn += p != 0xFF101820u;
    CHECK(drawn > 100000);                           // the city fills the view
    CHECK(fnv(fb) == 0xbc68d1d2bfa08ae6ull);        // regression fingerprint of THIS renderer (visually checked, not recorded from the original)
}
