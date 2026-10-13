#include "render/map_renderer.h"

#include <algorithm>

#include "simulation/sim_context.h"

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
bool inMapAB(int a, int b) { return a >= 0 && a < 128 && b >= 0 && b < 128; }
// vector (file axes) -> display axes
void rotVec(int rot, int va, int vb, int& ra, int& rb) {
    switch (rot & 3) {
        case 0: ra = va; rb = vb; break;
        case 1: ra = vb; rb = -va; break;
        case 2: ra = -va; rb = -vb; break;
        default: ra = -vb; rb = va; break;
    }
}
}  // namespace

void Camera::centerOn(int a, int b, int sx, int sy) {
    int da, db;
    toDisplay(rotation, a, b, da, db);
    scrollX = sx - (da - db) * (4 << zoom) - (4 << zoom);
    scrollY = sy - (da + db) * (2 << zoom);
}

void PaletteCycler::update(int elapsedMs) {
    waterMs_ += elapsedMs;
    blinkMs_ += elapsedMs;
    while (waterMs_ >= 140) {
        waterMs_ -= 140;
        std::array<uint32_t, 256> old = work_;
        for (int i = 0; i < 49; ++i) work_[static_cast<size_t>(0xAB + i)] = old[static_cast<size_t>(0xAB + t_.waterCycle[static_cast<size_t>(i)])];
    }
    while (blinkMs_ >= 500) {
        blinkMs_ -= 500;
        std::array<uint32_t, 256> old = work_;
        for (int i = 0; i < 16; ++i) work_[static_cast<size_t>(0xE0 + i)] = old[static_cast<size_t>(0xE0 + t_.blinkCycle[static_cast<size_t>(i)])];
    }
}

void MapRenderer::blit(Framebuffer& fb, int spriteId, int x, int yTop, bool flip, bool shadow) const {
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
            uint32_t& dst = fb.pixels[static_cast<size_t>(y) * static_cast<size_t>(fb.width) + static_cast<size_t>(px)];
            if (shadow) dst = 0xFF000000u | ((dst >> 1) & 0x7F7F7Fu);
            else dst = pal_[s->index[o]];
        }
    }
}

void MapRenderer::tint(Framebuffer& fb, int x, int yTop, uint32_t color, int alpha, int zoom) const {
    const Sprite* flat = sprites_.get(1256 - 1000 + 500 * zoom);   // flat land tile: its opaque pixels are the tile diamond
    if (!flat) return;
    for (int row = 0; row < flat->height; ++row) {
        const int y = yTop + row;
        if (y < 0 || y >= fb.height) continue;
        for (int col = 0; col < flat->width; ++col) {
            if (!flat->opaque[static_cast<size_t>(row) * static_cast<size_t>(flat->width) + static_cast<size_t>(col)]) continue;
            const int px = x + col;
            if (px < 0 || px >= fb.width) continue;
            uint32_t& d = fb.pixels[static_cast<size_t>(y) * static_cast<size_t>(fb.width) + static_cast<size_t>(px)];
            auto mix = [&](int shift) { const int dv = (d >> shift) & 0xFF, cv = (color >> shift) & 0xFF; return static_cast<uint32_t>((dv * (255 - alpha) + cv * alpha) / 255) << shift; };
            d = 0xFF000000u | mix(16) | mix(8) | mix(0);
        }
    }
}

void MapRenderer::drawUndergroundTile(Framebuffer& fb, const World& w, const Camera& cam, const RenderLayers& layers, int a, int b, int x, int yBase) const {
    // 0x43A9E0 (zoom-2 body; other zooms use the same formulas with the zoom's sprite base)
    const int z = cam.zoom, zoomBase = 500 * z, tw = 4 << z, step = 3 << z;
    auto height = [&](int id) { const Sprite* sp = sprites_.get(id); return sp ? sp->height : 0; };
    const uint16_t aw = w.altm.at(a, b);
    const int altLand = aw & 0x1F;
    const int yb = yBase - altLand * step;
    const int xter = w.xter.at(a, b);
    const int ug = t_.undergroundSprite[static_cast<size_t>(xter)];
    const int groundTop = yb - height(ug + zoomBase);
    const int wd = (aw & 0x7C00) >> 10;   // depth of the water table
    if (wd != 0) {
        int sid, dy;
        if (wd == 1) { sid = zoomBase + xter + 0x3E; dy = -height(sid); }
        else { sid = zoomBase + 0x160; dy = (wd - 1) * step - height(sid); }
        blit(fb, sid, x, yb + dy, false);
    }
    const int xund = w.xund.at(a, b), xb = w.xbit.at(a, b);
    const bool piped = (xb & xbit::kPiped) != 0, watered = (xb & xbit::kWatered) != 0;
    if ((xund < 0x10 || xund > 0x1E) && xund != 0x1F && xund != 0x20) {
        if (xund == 0) {
            const int sid = !piped ? zoomBase + ug : (!watered ? zoomBase + 0x15F : zoomBase + 0x1D3);
            blit(fb, sid, x, groundTop, false);
        } else {
            blit(fb, zoomBase + xund + 0x13E, x, groundTop, false);
            if (piped) blit(fb, watered ? zoomBase + 0x1D3 : zoomBase + 0x15F, x, groundTop, false);
        }
    } else {
        int sid = xund;
        if ((xb & 0x30) == 0x30) sid += 0x74;
        blit(fb, zoomBase + sid + 0x13E, x, groundTop, false);
    }
    const int id = w.xbld.at(a, b);
    if (id == 0 || !layers.buildings) return;
    if (id < 0x70) {
        if (id >= 0x61 && id <= 0x6B) return;       // elevated highways are not shown underground
        if (!layers.streets && (isRoad(id) || isRail(id) || (id >= 0x49 && id <= 0x5C))) return;   // roads, rails, highway ramps and bridges can be hidden
        if (id < 0xE) return;
        int by = yb;
        if (xb & xbit::kWater) by += (altLand - ((aw & 0x3E0) >> 5)) * step;
        else if (ug == 0x13E) by -= step;
        blit(fb, id + zoomBase, x, by - height(id + zoomBase), (xb & xbit::kFlipped) != 0);
    } else {
        const int fid = zoomBase + 0x162 + t_.zoneFootprint[static_cast<size_t>(w.xzon.at(a, b) & 0xF)];
        blit(fb, fid, x, yb - height(fid), false);
    }
    (void)tw;
}

void MapRenderer::drawMonster(Framebuffer& fb, const World& w, const Camera& cam, int a, int b, int slot) const {
    // 0x451880: the monster is composed of mirrored body parts; coordinates are zoom-2 pixels scaled down for lower zooms
    const uint8_t* r = &w.xthg[static_cast<size_t>(slot) * 12];
    const int z = cam.zoom;
    int da, db;
    toDisplay(cam.rotation, a, b, da, db);
    const uint16_t altWord = w.altm.at(a, b);
    const int alt = (w.xbit.at(a, b) & xbit::kWater) ? (altWord & 0x3E0) >> 5 : (altWord & 0x1F);
    const int y5 = (da + db) * 8 + (alt + r[5]) * -12;
    const int x13 = ((da - db) - 3) * 16;
    const auto& m = t_.monsterPart;   // [0..1]=4E7248, [2..3]=4E724C, [4..5]=4E7250, [6..7]=4E7254, [8..9]=4E7258, [10..11]=4E725C, [12..13]=4E7260, [14..15]=4E7264
    auto put = [&](int base, int sx, int sy, bool flip) {
        int px = sx, py = sy;
        if (z == 1) { px /= 2; py /= 2; } else if (z == 0) { px /= 4; py /= 4; }
        blit(fb, base + z * 500, px + cam.scrollX, py + cam.scrollY, flip);
    };
    const int b8 = r[8], b9 = r[9];
    int u10 = b8 & 1, u7 = (b8 & 2) >> 1;
    const int x14 = x13 - 0x14, y8 = y5 - 0x4B;
    int s3 = m[static_cast<size_t>(0 + u10)], s11 = m[static_cast<size_t>(4 + u10)];
    put(0x1E2 + ((b8 & 4) >> 2), m[static_cast<size_t>(2 + u7)] + s3 + x14, m[static_cast<size_t>(6 + u7)] + s11 + y8, false);
    put(0x1E0 + u7, s3 + x14, s11 + y8, false);
    put(0x1DE + u10, x14, y8, false);
    u7 = (b8 & 0x10) >> 4; u10 = (b8 & 8) >> 3;
    s3 = m[static_cast<size_t>(4 + u10)];
    int s11b = (x13 + 0x52) - m[static_cast<size_t>(0 + u10)];
    put(0x1E2 + ((b8 & 0x20) >> 5), s11b - m[static_cast<size_t>(2 + u7)], m[static_cast<size_t>(6 + u7)] + y8 + s3, true);
    put(0x1E0 + u7, s11b, y8 + s3, true);
    put(0x1DE + u10, x13 + 0x52, y8, true);
    if (b8 & 0x80) put(0x181, x13 + 0x2E, y5 - 0x12, false);
    int head = t_.thingSprite[5];
    if (b9 & 0x80) head += (a + b) & 1;
    put(head, x13, y5 - 0x6E, false);
    put(head, x13 + 0x3C, y5 - 0x6E, true);
    const int y5b = y5 - 0x32;
    u7 = b9 & 1;
    int u10b = (b9 & 2) >> 1;
    int t3 = m[static_cast<size_t>(8 + u7)], t11 = m[static_cast<size_t>(12 + u7)], t8 = m[static_cast<size_t>(10 + u10b)], t4 = m[static_cast<size_t>(14 + u10b)];
    put(0x1E4 + u7, x14, y5b, false);
    put(0x1E6 + u10b, t3 + x14, t11 + y5b, false);
    put(0x1E8 + ((b9 & 4) >> 2), t8 + t3 + x14, t4 + t11 + y5b, false);
    u7 = (b9 & 8) >> 3; u10b = (b9 & 0x10) >> 4;
    t3 = m[static_cast<size_t>(12 + u7)];
    const int x14b = (x13 + 0x50) - m[static_cast<size_t>(8 + u7)];
    t11 = m[static_cast<size_t>(10 + u10b)]; t8 = m[static_cast<size_t>(14 + u10b)];
    put(0x1E4 + u7, x13 + 0x50, y5b, true);
    put(0x1E6 + u10b, x14b, t3 + y5b, true);
    put(0x1E8 + ((b9 & 0x20) >> 5), x14b - t11, t8 + t3 + y5b, true);
}

void MapRenderer::drawThing(Framebuffer& fb, const World& w, const Camera& cam, int a, int b, int da, int db, int slot) const {
    const uint8_t* r = &w.xthg[static_cast<size_t>(slot) * 12];
    const int type = r[0];
    const int z = cam.zoom;
    if (type == 0 || type > 0x10) return;
    if (z < t_.thingMinZoom[static_cast<size_t>(type)]) return;
    const int dir = r[1];
    const int tw = 4 << z, th = 2 << z, step = 3 << z;
    const uint16_t altWord = w.altm.at(a, b);
    const int groundAlt = (w.xbit.at(a, b) & xbit::kWater) ? (altWord & 0x3E0) >> 5 : (altWord & 0x1F);
    const int tileLeft = (da - db) * tw + cam.scrollX;
    const int tileBase = (da + db) * th + cam.scrollY;
    auto spriteAt = [&](int id) { return sprites_.get(id); };

    if (type == 5) { drawMonster(fb, w, cam, a, b, slot); return; }
    if (type == 10 || type == 0xB) {   // train engine / car (0x450B80): drawn from the rail piece under it, large zoom only
        const int id = w.xbld.at(a, b);
        int idx, lift;
        if (id == 0x5A || id == 0x5B) {
            lift = (((altWord & 0x3E0) >> 5) + 1) * 12;
            idx = (w.xbit.at(a, b) & xbit::kFlipped) ? 1 : 0;
        } else {
            int k = id - 0x2C;
            if (k < 0 || k > 0x22) return;
            if (k > 0x12) k = id - 0x32;
            if (k > 0x16) k -= 4;
            int v = t_.railSprite[static_cast<size_t>(k) & 31];
            if (v == 0x32) v = t_.railTurn[static_cast<size_t>(r[8]) & 7];
            idx = v;
            lift = (altWord & 0x1F) * 12;
            if (w.xter.at(a, b) == 0xD) lift += 12;
        }
        const int sid = t_.thingSprite[static_cast<size_t>(type)] + t_.railBase[static_cast<size_t>(idx)] + 1000;
        const Sprite* sp = spriteAt(sid);
        if (!sp) return;
        const int x = tileLeft + tw - (sp->width >> 1) + t_.railDx[static_cast<size_t>(idx)];
        const int y = tileBase + t_.railDy[static_cast<size_t>(idx)] - sp->height - lift;
        blit(fb, sid, x, y, t_.railFlip[static_cast<size_t>(idx)] != 0);
        return;
    }
    if (type == 0xC || type == 0xD || type == 7 || type == 8 || type == 0xE || type == 4) return;

    int sprite = 0;
    bool flip = false;
    if (type == 1 || type == 2 || type == 3) {
        sprite = t_.aircraftFrame[static_cast<size_t>(dir) & 7] + t_.thingSprite[static_cast<size_t>(type)];
        flip = t_.aircraftFlip[static_cast<size_t>(dir) & 7] != 0;
    } else if (type == 6) {
        sprite = dir + t_.thingSprite[6];
        flip = ((a * 7 + b * 13 + slot) & 1) != 0;
    } else if (type == 0xF) {
        sprite = t_.tornadoSprite + ((a + b + slot) % 3);
        flip = ((a + b) & 1) != 0;
    } else if (type == 0x10) {
        sprite = t_.thingSprite[0x10];
        flip = dir > 3;
    } else {   // sailboat (9) and anything else
        if (type == 9 && r[2] != 0) { sprite = 0x17B; flip = (slot & 1) != 0; }
        else {
            sprite = t_.thingSprite[static_cast<size_t>(type)] + t_.boatFrame[static_cast<size_t>(dir) & 7];
            flip = t_.boatFlip[static_cast<size_t>(dir) & 7] != 0;
        }
    }
    sprite += z * 500;
    const Sprite* sp = spriteAt(sprite);
    if (!sp) return;
    const int c6 = r[6] - (type == 3 ? 6 : 8), c7 = r[7] - (type == 3 ? 6 : 8);
    int sA, sB;
    rotVec(cam.rotation, c6, c7, sA, sB);
    const int divX = t_.subDivX[static_cast<size_t>(z)], divY = t_.subDivY[static_cast<size_t>(z)];
    const int x = tileLeft + tw + (sA - sB) / divX - (sp->width >> 1);
    const int y = tileBase + (sA + sB + 16) / divY - sp->height - (th * r[5] + groundAlt * step);
    if ((type == 1 || type == 2 || type == 0x10) && w.xbld.at(a, b) < 0x70) blit(fb, sprite, x, y + (r[5] - 2) * th + 0, flip, true);   // shadow on the ground
    blit(fb, sprite, x, y, flip);
}

namespace {
// centre of the top diamond of file tile (a, b) on screen
void tileCentre(const World& w, const Camera& cam, int a, int b, int& cx, int& cy) {
    int da, db;
    toDisplay(cam.rotation, a, b, da, db);
    const int z = cam.zoom, tw = 4 << z, th = 2 << z, step = 3 << z;
    const int xter = w.xter.at(a, b);
    const uint16_t aw = w.altm.at(a, b);
    const int alt = xter < 0x10 ? (aw & 0x1F) : (aw & 0x3E0) >> 5;
    cx = (da - db) * tw + cam.scrollX + tw;
    cy = (da + db) * th + cam.scrollY - alt * step - th;
}
}  // namespace

void MapRenderer::tileScreenPos(const World& w, const Camera& cam, int a, int b, int& x, int& y) const { tileCentre(w, cam, a, b, x, y); }

bool MapRenderer::pickTile(const World& w, const Camera& cam, int px, int py, int& oa, int& ob) const {
    const int z = cam.zoom, tw = 4 << z, th = 2 << z;
    int bestSum = -1;
    for (int a = 0; a < 128; ++a)
        for (int b = 0; b < 128; ++b) {
            int cx, cy;
            tileCentre(w, cam, a, b, cx, cy);
            if (std::abs(px - cx) * th + std::abs(py - cy) * tw > tw * th) continue;
            int da, db;
            toDisplay(cam.rotation, a, b, da, db);
            if (da + db >= bestSum) { bestSum = da + db; oa = a; ob = b; }
        }
    return bestSum >= 0;
}

void MapRenderer::highlightTile(Framebuffer& fb, const World& w, const Camera& cam, int a, int b, uint32_t color, int alpha, bool outline) const {
    if (!inMapAB(a, b)) return;
    int cx, cy;
    tileCentre(w, cam, a, b, cx, cy);
    const int z = cam.zoom, tw = 4 << z, th = 2 << z;
    auto put = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= fb.width || y >= fb.height) return;
        uint32_t& d = fb.pixels[static_cast<size_t>(y) * static_cast<size_t>(fb.width) + static_cast<size_t>(x)];
        auto mix = [&](int shift) { const int dv = (d >> shift) & 0xFF, cv = (color >> shift) & 0xFF; return static_cast<uint32_t>((dv * (255 - alpha) + cv * alpha) / 255) << shift; };
        d = 0xFF000000u | mix(16) | mix(8) | mix(0);
    };
    for (int y = -th; y <= th; ++y) {
        const int half = (th - std::abs(y)) * tw / th;
        if (outline) { put(cx - half, cy + y); put(cx + half, cy + y); put(cx - half + 1, cy + y); put(cx + half - 1, cy + y); }
        else for (int x = -half; x <= half; ++x) put(cx + x, cy + y);
    }
}

// 0x43C0DB: the drawbridge (piece 0x58) is drawn raised (0x59) when a ship (thing type 3) is within a Manhattan distance of 8 tiles
bool MapRenderer::drawbridgeUp(const World& w, int a, int b) const {
    for (int slot = 1; slot < 0x28; ++slot) {
        const uint8_t* rec = &w.xthg[static_cast<size_t>(slot) * 12];
        if (rec[0] == 3 && std::abs(rec[3] - a) + std::abs(rec[4] - b) < 8) return true;
    }
    return false;
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

    // ground sprite of a file tile drawn at (x, yBase); yBase is the tile's bottom vertex (before altitude)
    auto groundOf = [&](int a, int b, int x, int yBase) {
        const int xter = w.xter.at(a, b);
        const uint16_t aw = w.altm.at(a, b);
        const int alt = xter < 0x10 ? (aw & 0x1F) : (aw & 0x3E0) >> 5;
        const int g = t_.terrainSprite[static_cast<size_t>(xter)] + zoomBase;
        if (g > 0) blit(fb, g, x, yBase - alt * step - height(g), false);
    };

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
            const int thingValue = w.xtxt.at(a, b);

            if (layers.underground) {
                drawUndergroundTile(fb, w, cam, layers, a, b, x, yBase);
                continue;
            }
            auto overlay = [&] {
                if (!layers.powerOverlay && !layers.waterOverlay) return;
                if (!(zone != 0 || id >= 0x70)) return;           // only zoned land and buildings consume power/water
                const int xb = w.xbit.at(a, b);
                const int ty = bottom - height(t_.terrainSprite[0] + zoomBase);
                if (layers.powerOverlay) {
                    if (xb & xbit::kPowered) tint(fb, x, ty, 0xFFFFE040u, 90, z);
                    else if (layers.blinkOn) tint(fb, x, ty, 0xFFFF2020u, 150, z);
                } else if (layers.waterOverlay) {
                    if (xb & xbit::kWatered) tint(fb, x, ty, 0xFF2080FFu, 90, z);
                    else if (layers.blinkOn) tint(fb, x, ty, 0xFFB07030u, 150, z);
                }
            };
            auto drawThings = [&] {
                if (layers.things && thingValue >= 0xC9 && thingValue <= 0xF0) drawThing(fb, w, cam, a, b, da, db, thingValue - 0xC9);
            };

            if (id >= 0x70) {
                if (layers.buildings && (rotMask & w.xzon.at(a, b) & 0xF0) != 0) {
                    // big buildings are drawn once, from the corner tile that is leftmost on screen for the current rotation
                    const int sid = id + 1000 + (z - 2) * 500;
                    blit(fb, sid, x, bottom + (widthOf(sid) >> 2) - th - height(sid), flip);
                }
                overlay();
                drawThings();
                continue;
            }
            if (id >= 0x61 && id <= 0x6B) {   // highway: a 2x2 piece drawn from its left-most tile over the four ground tiles
                if ((rotMask & w.xzon.at(a, b) & 0xF0) != 0 && layers.buildings) {
                    if (layers.terrain) {
                        groundOf(a, b, x, yBase);
                        int fa, fb2;
                        toFile(cam.rotation, da, db - 1, fa, fb2); if (inMapAB(fa, fb2) && db > 0) groundOf(fa, fb2, x + tw, yBase - th);
                        toFile(cam.rotation, da + 1, db - 1, fa, fb2); if (inMapAB(fa, fb2) && da < 127 && db > 0) groundOf(fa, fb2, x + 2 * tw, yBase);
                        toFile(cam.rotation, da + 1, db, fa, fb2); if (inMapAB(fa, fb2) && da < 127) groundOf(fa, fb2, x + tw, yBase + th);
                    }
                    const int sid = id + zoomBase;
                    blit(fb, sid, x, bottom + th - height(sid), flip);
                    const int density = w.xtrf.at(a >> 1, b >> 1);
                    if (layers.traffic && density > 0x1C) {
                        int v = t_.carVariant[static_cast<size_t>(id)];
                        if (v != 0) {
                            if (density > 0x38) v = t_.heavyCar[static_cast<size_t>(v) & 15];
                            const int cid = (0x577 - 1000) + v + zoomBase;
                            blit(fb, cid, x, bottom + th - height(cid), flip);
                        }
                    }
                }
                drawThings();
                continue;
            }
            // ground
            if (layers.terrain) {
                int ground = t_.terrainSprite[static_cast<size_t>(xter)] + zoomBase;
                if (id < 0x1D && xter == 0 && zone != 0 && layers.zones) ground = (0x50A + zone - 1000) + zoomBase;   // coloured zone tile under empty land and under the small objects (rubble, trees, power lines: ids below 0x1D, 0x43C0DB)
                if (ground > 0) blit(fb, ground, x, bottom - height(ground), false);
            }
            if (id != 0 && layers.buildings) {
                int sid = id + zoomBase;
                int top;
                // 0x43C0DB: objects stand on the (water-level) altitude of their tile like the ground; only slope type 0xD lifts the small ones by one step
                if (id == 0x58 && drawbridgeUp(w, a, b)) sid = 0x59 + zoomBase;   // the drawbridge opens for a ship within 8 tiles (Manhattan distance)
                top = bottom - height(sid) - (xter == 0xD && id < 0x1D ? 12 : 0);
                blit(fb, sid, x, top, flip);
                if (layers.traffic && id >= 0x1D) {
                    const int density = w.xtrf.at(a >> 1, b >> 1);
                    const bool highwayPiece = id >= 0x49 && id <= 0x50;
                    const int low = highwayPiece ? 0x1C : 0x55, heavy = highwayPiece ? 0x38 : 0xAA;
                    if (density > low) {
                        int v = t_.carVariant[static_cast<size_t>(id)];
                        bool cflip = (w.xbit.at(a, b) & xbit::kFlipped) != 0;
                        if (v == 0xB) { if (a & 1) v = 0xC; }
                        else if (v == 0xC) { cflip = true; if (b & 1) v = 0xB; }
                        if (v != 0) {
                            if (density > heavy) v = t_.heavyCar[static_cast<size_t>(v) & 15];
                            const int cid = (0x577 - 1000) + v + zoomBase;
                            blit(fb, cid, x, bottom - height(cid), cflip);
                        }
                    }
                }
            }
            overlay();
            drawThings();
        }
    }
}

}  // namespace sc2k
