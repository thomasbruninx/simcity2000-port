// Software renderer of the isometric city map, following the original's tile redraw routines (0x43A660 / 0x439EB0 / 0x43C0DB,
// docs/rendering/map-drawing.md). Sprite numbering: id = base + 500 * zoom; zoom 2 (32x16 tiles) uses LARGE.DAT.
#pragma once
#include <array>

#include "render/framebuffer.h"
#include "resources/sprite_archive.h"
#include "simulation/city_state.h"
#include "simulation/tables.h"
#include "world/world.h"

namespace sc2k {

struct Camera {
    int zoom = 2;          // 0 = 8x4 tiles, 1 = 16x8, 2 = 32x16
    int rotation = 0;      // view rotation 0..3 (MISC 2)
    int scrollX = 0;       // screen x of display tile (0,0)
    int scrollY = 0;       // screen y of the bottom vertex of display tile (0,0)
    int tileWidth() const { return 8 << zoom; }
    // Moves the camera so that file tile (a, b) appears at screen position (sx, sy).
    void centerOn(int a, int b, int sx, int sy);
};

struct RenderLayers {
    bool terrain = true, buildings = true, zones = true, things = true;
};

class MapRenderer {
public:
    MapRenderer(const GameTables& tables, const SpriteSet& sprites, const std::array<uint32_t, 256>& palette) : t_(tables), sprites_(sprites), pal_(palette) {}
    void draw(Framebuffer& fb, const World& w, const CityState& s, const Camera& cam, const RenderLayers& layers = {}) const;

private:
    void blit(Framebuffer& fb, int spriteId, int x, int yTop, bool flip) const;
    const GameTables& t_;
    const SpriteSet& sprites_;
    const std::array<uint32_t, 256>& pal_;
};

}  // namespace sc2k
