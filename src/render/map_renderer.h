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
    bool terrain = true, buildings = true, zones = true, things = true, traffic = true;
    bool underground = false;     // pipes, subway and water table instead of the surface (0x43A9E0)
    bool powerOverlay = false;    // tint powered / unpowered tiles
    bool waterOverlay = false;    // tint watered / piped / dry tiles
    bool blinkOn = true;          // phase of the blinking no-power bolt
};

// Palette animation of the original (0x456A60 / 0x456B70): 49 entries from index 0xAB rotate (water shimmer) and 16 entries from 0xE0
// swap in pairs (blinking lights). Keeps a working copy of the palette that the renderer reads.
class PaletteCycler {
public:
    PaletteCycler(const std::array<uint32_t, 256>& base, const GameTables& t) : base_(base), work_(base), t_(t) {}
    void update(int elapsedMs);
    const std::array<uint32_t, 256>& palette() const { return work_; }
private:
    std::array<uint32_t, 256> base_, work_;
    const GameTables& t_;
    int waterMs_ = 0, blinkMs_ = 0;
};

class MapRenderer {
public:
    // `palette` is read on every draw: pass PaletteCycler::palette() to animate the water.
    MapRenderer(const GameTables& tables, const SpriteSet& sprites, const std::array<uint32_t, 256>& palette) : t_(tables), sprites_(sprites), pal_(palette) {}
    void draw(Framebuffer& fb, const World& w, const CityState& s, const Camera& cam, const RenderLayers& layers = {}) const;

private:
    void blit(Framebuffer& fb, int spriteId, int x, int yTop, bool flip, bool shadow = false) const;
    void tint(Framebuffer& fb, int x, int yTop, uint32_t color, int alpha, int zoom) const;   // blends a colour over a tile diamond whose sprite box starts at (x, yTop)
    void drawUndergroundTile(Framebuffer& fb, const World& w, const Camera& cam, const RenderLayers& layers, int a, int b, int x, int yBase) const;
    void drawThing(Framebuffer& fb, const World& w, const Camera& cam, int a, int b, int da, int db, int slot) const;
    void drawMonster(Framebuffer& fb, const World& w, const Camera& cam, int a, int b, int slot) const;
    const GameTables& t_;
    const SpriteSet& sprites_;
    const std::array<uint32_t, 256>& pal_;
};

}  // namespace sc2k
