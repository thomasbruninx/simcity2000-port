#include "render/map_renderer.h"

#include <algorithm>

namespace sc2k {

namespace {
// display <-> file coordinates for the four view rotations (derived from the corner flags of multi-tile buildings, see special-zones.md)
void toDisplay(int rot, int a, int b, int& da, int& db) {
    switch (rot & 3) {
        case 0: da = a; db = b; break;
        case 1: da = b; db = 127 - a; break;
        case 2: da = 127 - a; db = 127 - b; break;
        default: da = 127 - b; db = a; break;
    }
}
void toFile(int rot, int da, int db, int& a, int& b) {
    switch (rot & 3) {
        case 0: a = da; b = db; break;
        case 1: a = 127 - db; b = da; break;
        case 2: a = 127 - da; b = 127 - db; break;
        default: a = db; b = 127 - da; break;
    }
}
}  // namespace

void Camera::centerOn(int a, int b, int sx, int sy) {
    int da, db;
    toDisplay(rotation, a, b, da, db);
    scrollX = sx - (da - db) * (4 << zoom) - (4 << zoom);
    scrollY = sy - (da + db) * (2 << zoom);
}

void MapRenderer::blit(Framebuffer& fb, int spriteId, int x, int yTop, bool flip) const {
    const Sprite* s = sprites_.get(spriteId);
    if (!s) return;
    for (int row = 0; row < s->height; ++row) {
        const int y = yTop + row;
        if (y < 0 || y >= fb.height) continue;
        for (int col = 0; col < s->width; ++col) {
            const size_t o = static_cast<size_t>(row) * static_cast<size_t>(s->width) + static_cast<size_t>(flip ? s->width - 1 - col : col);
            if (!s->opaque[o]) continue;
            const int px = x + col;
            if (px < 0 || px >= fb.width) continue;
            fb.pixels[static_cast<size_t>(y) * static_cast<size_t>(fb.width) + static_cast<size_t>(px)] = pal_[s->index[o]];
        }
    }
}

void MapRenderer::draw(Framebuffer& fb, const World& w, const CityState& s, const Camera& cam, const RenderLayers& layers) const {
    const int z = cam.zoom;
    const int zoomBase = 500 * z;
    const int tw = 4 << z, th = 2 << z, step = 3 << z;
    const uint16_t rotMask = static_cast<uint16_t>(t_.rotationZoneMask[static_cast<size_t>(cam.rotation & 3)]);
    (void)s;
    auto height = [&](int id) { const Sprite* sp = sprites_.get(id); return sp ? sp->height : 0; };
    auto widthOf = [&](int id) { const Sprite* sp = sprites_.get(id); return sp ? sp->width : 0; };
    const int margin = 260 << (z > 0 ? z - 1 : 0) >> 1;

    for (int sum = 0; sum <= 254; ++sum) {
        for (int da = std::max(0, sum - 127); da <= std::min(127, sum); ++da) {
            const int db = sum - da;
            const int x = (da - db) * tw + cam.scrollX;
            const int yBase = sum * th + cam.scrollY;
            if (x < -tw * 4 || x > fb.width + tw * 2) continue;
            if (yBase < -64 || yBase - margin > fb.height + 64) continue;
            int a, b;
            toFile(cam.rotation, da, db, a, b);
            const int xter = w.xter.at(a, b);
            const uint16_t altWord = w.altm.at(a, b);
            const int altLand = altWord & 0x1F, altWater = (altWord & 0x3E0) >> 5;
            const int alt = xter < 0x10 ? altLand : altWater;
            const int bottom = yBase - alt * step;
            const int id = w.xbld.at(a, b);
            const int zone = w.xzon.at(a, b) & 0xF;
            const bool flip = ((w.xbit.at(a, b) & xbit::kFlipped) != 0) != ((cam.rotation & 1) != 0);

            if (id >= 0x70) {
                if (!layers.buildings) continue;
                // big buildings are drawn once, from the corner tile that is leftmost on screen for the current rotation
                if ((rotMask & w.xzon.at(a, b) & 0xF0) == 0) continue;
                const int sid = id + 1000 + (z - 2) * 500;
                const int wd = widthOf(sid), ht = height(sid);
                blit(fb, sid, x, bottom + (wd >> 2) - th - ht, flip);
                continue;
            }
            // ground
            if (layers.terrain) {
                int ground = t_.terrainSprite[static_cast<size_t>(xter)] + zoomBase;
                if (id == 0 && xter == 0 && zone != 0 && layers.zones) ground = (0x50A + zone - 1000) + zoomBase;   // coloured zone tile on empty land

                if (ground > 0) blit(fb, ground, x, bottom - height(ground), false);
            }
            if (id == 0 || !layers.buildings) continue;
            int sid = id + zoomBase;
            if (id >= 0x1D && (id > 0x50 && id < 0x5D)) {   // bridges: drawn at the level of the water surface
                const int adj = (altLand - altWater) * step;
                blit(fb, sid, x, bottom + adj - height(sid), flip);
            } else if (id >= 0x61 && id < 0x70 && (id > 0x6B || id > 0x60)) {
                if ((rotMask & w.xzon.at(a, b) & 0xF0) != 0 || id > 0x6B) blit(fb, sid, x, bottom - height(sid), flip);
            } else {
                blit(fb, sid, x, bottom - height(sid), flip);
            }
        }
    }
    (void)layers.things;
}

}  // namespace sc2k
