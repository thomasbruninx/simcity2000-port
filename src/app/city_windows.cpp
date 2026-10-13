// The Industry (dialog 160), Neighbors (dialog 157, "SimNation") and City Map (dialog 246) windows of the original, drawn into the information window frame.
#include <algorithm>
#include <cstring>

#include "app/info_windows.h"
#include "app/ui_draw.h"
#include "simulation/sim_context.h"

namespace sc2k {

using namespace ui;

// ---- City Industry (0x471080): eleven sectors as horizontal bars, three modes ----------------------------------------------------------------------------
void InfoWindows::drawIndustry(SDL_Renderer* r, int x, int y) {
    const CityState& s = sim_.state();
    long v[11];
    long maxv = industryMode_ == 0 ? 70 : industryMode_ == 1 ? 30 : 100;   // the scale starts at the table 0x4E8894 and grows with the data
    for (int k = 0; k < 11; ++k) {
        v[k] = industryMode_ == 0 ? s.sec408[static_cast<size_t>(k)] : industryMode_ == 1 ? s.sec3f8[static_cast<size_t>(k)] : s.sec940c[static_cast<size_t>(k)];
        maxv = std::max(maxv, v[k]);
    }
    const int cx = x + 190, cw = 380, cy = y + 14, rowH = 24;
    fill(r, x + 12, y + 6, 596, 11 * rowH + 20, 236, 232, 220);
    frame(r, x + 12, y + 6, 596, 11 * rowH + 20, 70, 70, 80);
    for (int k = 0; k < 11; ++k) {
        const int ry = cy + k * rowH;
        text(r, x + 24, ry + 8, res_.string(422 + k), 20, 20, 30);
        const int w = v[k] > 0 ? static_cast<int>(v[k] * cw / std::max(1L, maxv)) : 0;
        if (w > 0) { fill(r, cx, ry + 2, w, rowH - 4, 0, 0, 192); frame(r, cx, ry + 2, w, rowH - 4, 0, 0, 127); }
        if (industryMode_ == 1) text(r, cx + cw + 6, ry + 8, std::to_string(v[k]) + "%", 20, 20, 30);
    }
    fill(r, cx - 2, cy, 1, 11 * rowH, 60, 60, 70);
    static const char* names[3] = {"Ratios", "Tax Rates", "Demand"};
    const auto* dlg = res_.dialog(160);
    for (int i = 0; i < 3; ++i) {
        std::string label = names[i];
        if (dlg) for (const auto& c : dlg->controls) if (c.id == 291 + i && !c.text.empty()) label = c.text;
        const bool on = industryMode_ == i;
        fill(r, x + 24 + i * 130, y + 11 * rowH + 36, 120, 22, on ? 70 : 40, on ? 100 : 46, on ? 150 : 56);
        frame(r, x + 24 + i * 130, y + 11 * rowH + 36, 120, 22, 190, 200, 230);
        text(r, x + 24 + i * 130 + 60 - static_cast<int>(label.size()) * 4, y + 11 * rowH + 43, label, 255, 255, 220);
    }
}

// ---- Neighbors (0x46BED2): the city and its four neighbours as blobs on the original's brown map, with names and populations --------------------------------
void InfoWindows::drawNeighbors(SDL_Renderer* r, int x, int y) {
    const CityState& s = sim_.state();
    if (!nbTried_) {
        nbTried_ = true;
        try { nbBmp_ = Bmp8::load(gameDir_ + "/BITMAPS/NEIGHBOR.BMP"); } catch (const std::exception&) {}
    }
    constexpr int W = 204, H = 160, S2 = 2;
    const int ox = x + (kW - W * S2) / 2, oy = y + 8;
    if (nbBmp_.width != 128 || nbBmp_.height < 6 * 64) { text(r, ox, oy, "(BITMAPS/NEIGHBOR.BMP not found)", 230, 150, 120); return; }
    std::vector<uint8_t> canvas(static_cast<size_t>(W) * H, 0x80);
    auto blob = [&](int bx, int by, uint32_t pop, bool flag) {   // 0x46C4D0 picks the 128x64 picture by population, 0x46C560 copies it (index 0 is transparent)
        const int row = (pop == 0 && flag) ? 0 : pop < 2000 ? 1 : pop < 10000 ? 2 : pop < 50000 ? 3 : pop < 100000 ? 4 : 5;
        for (int yy = 0; yy < 64; ++yy)
            for (int xx = 0; xx < 128; ++xx) {
                const int dx = bx + xx, dy = by + yy;
                if (dx < 0 || dy < 0 || dx >= W || dy >= H) continue;
                const uint8_t px = nbBmp_.index[static_cast<size_t>(row * 64 + yy) * 128 + static_cast<size_t>(xx)];
                if (px != 0) canvas[static_cast<size_t>(dy) * W + static_cast<size_t>(dx)] = px;
            }
    };
    const int rot = s.rotation & 3;
    const uint32_t cityPop = static_cast<uint32_t>(s.population10);
    uint32_t pops[4];
    for (int k = 0; k < 4; ++k) pops[k] = s.nbr9d28[static_cast<size_t>(k)];
    const int order[4] = {(rot + 2) & 3, (rot + 3) & 3, rot & 3, (rot + 1) & 3};   // NW, NE, SE, SW
    blob(0x26, 0x1f, cityPop, false);
    blob(-0x1a, -1, pops[order[0]], true);
    blob(0x66, -1, pops[order[1]], true);
    blob(0x66, 0x3f, pops[order[2]], true);
    blob(-0x1a, 0x3f, pops[order[3]], true);
    std::vector<uint32_t> px(canvas.size());
    for (size_t i = 0; i < px.size(); ++i) px[i] = nbBmp_.palette[canvas[i]];
    if (!nbTex_) {
        nbTex_.reset(SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H));
        if (nbTex_) SDL_SetTextureScaleMode(nbTex_.get(), SDL_SCALEMODE_NEAREST);
    }
    if (nbTex_) {
        SDL_UpdateTexture(nbTex_.get(), nullptr, px.data(), W * 4);
        const SDL_FRect dst{static_cast<float>(ox), static_cast<float>(oy), static_cast<float>(W * S2), static_cast<float>(H * S2)};
        SDL_RenderTexture(r, nbTex_.get(), nullptr, &dst);
    }
    if (!font_) font_ = std::make_unique<TextRenderer>(r);
    auto label = [&](const std::string& t, int cxp, int cyp) {   // white on a black outline (0x46C790), centred on the canvas position
        const int px2 = 16, tw = font_->width(t, px2, true);
        const int tx = ox + cxp * S2 - tw / 2, ty = oy + cyp * S2 - px2 / 2;
        for (int dx : {-1, 1}) for (int dy : {-1, 1}) font_->draw(t, tx + dx, ty + dy, px2, 0, 0, 0, true);
        font_->draw(t, tx, ty, px2, 255, 255, 255, true);
    };
    auto name = [&](int k) { return res_.string(0x223 + (s.nbrName[static_cast<size_t>(k)] > 0 ? s.nbrName[static_cast<size_t>(k)] : 1)); };
    const int pos[4][2] = {{40, 39}, {160, 39}, {160, 100}, {40, 100}};
    for (int i = 0; i < 4; ++i) {
        label(name(order[i]), pos[i][0], pos[i][1]);
        if (pops[order[i]] != 0) label(std::to_string(pops[order[i]]), pos[i][0], pos[i][1] + 10);
    }
    label(std::to_string(cityPop), 100, 75);
    label(cityName.empty() ? std::string("New City") : cityName, 100, 143);
    text(r, x + 16, y + 8 + H * S2 + 10, res_.string(421).substr(0, 10) + std::to_string(s.nationalPop) + "000", 200, 210, 230);
}

// ---- City Map (dialog 246): 128 x 128 tiles, 18 layers -----------------------------------------------------------------------------------------------------
namespace {
constexpr uint32_t rgb(int r, int g, int b) { return 0xFF000000u | static_cast<uint32_t>(r << 16 | g << 8 | b); }
uint32_t heat(int v, int maxv) {   // black -> red -> yellow
    const int t = std::clamp(v * 255 / std::max(1, maxv), 0, 255);
    return rgb(std::min(255, t * 2), t > 128 ? (t - 128) * 2 : 0, 0);
}
}  // namespace

void InfoWindows::drawMap(SDL_Renderer* r, int x, int y) {
    World& w = sim_.world();
    constexpr int N = 128, S3 = 3;
    std::vector<uint32_t> px(static_cast<size_t>(N) * N);
    const int layer = mapLayer_;
    for (int a = 0; a < N; ++a)
        for (int b = 0; b < N; ++b) {
            const int id = w.xbld.at(a, b), zone = w.xzon.at(a, b) & 0xF, xb = w.xbit.at(a, b);
            const bool water = (xb & xbit::kWater) != 0;
            uint32_t c = water ? rgb(40, 80, 200) : rgb(28, 56, 24);
            const bool road = isRoad(id) || (id >= 0x49 && id <= 0x50) || (id >= 0x51 && id <= 0x5C) || (id >= 0x61 && id <= 0x6B);
            const bool rail = isRail(id);
            auto zoneColor = [&](int z, bool dim) {
                const int k = dim ? 2 : 4;
                if (z == 1 || z == 2) return rgb(60 * k / 4 + 20, 200 * k / 4, 60 * k / 4);
                if (z == 3 || z == 4) return rgb(40 * k / 4, 80 * k / 4, 255 * k / 4);
                if (z == 5 || z == 6) return rgb(230 * k / 4, 210 * k / 4, 40 * k / 4);
                return rgb(200 * k / 4, 200 * k / 4, 200 * k / 4);
            };
            switch (layer) {
                case 0: case 1: case 16: case 17:   // structures, zones (schools / colleges show the structures)
                    if (id >= 0x70) c = zoneColor(zone, false);
                    else if (road) c = rgb(150, 150, 150);
                    else if (rail) c = rgb(176, 112, 48);
                    else if (id != 0 && id < 0x1D) c = rgb(20, 90, 30);
                    else if (layer == 1 && zone != 0 && !water) c = zoneColor(zone, true);
                    break;
                case 2: if (road) c = rgb(255, 255, 255); else if (id >= 0x70) c = rgb(70, 70, 70); break;
                case 3: if (rail) c = rgb(255, 200, 120); else if (road) c = rgb(80, 80, 80); break;
                case 4: if (!water) c = heat(w.xtrf.at(a >> 1, b >> 1), 255); break;
                case 5: if (!water && (xb & xbit::kPowerable)) c = (xb & xbit::kPowered) ? rgb(240, 220, 40) : rgb(220, 40, 40); break;
                case 6: if (!water) { if (xb & xbit::kWatered) c = rgb(40, 140, 255); else if (xb & xbit::kPiped) c = rgb(30, 80, 160); else if (id >= 0x70 || zone) c = rgb(150, 100, 40); } break;
                case 7: if (!water) c = heat(w.xpop.at(a >> 2, b >> 2), 255); break;
                case 8: if (!water) c = heat(w.xrog.at(a >> 2, b >> 2), 255); break;
                case 9: if (!water) c = heat(w.xcrm.at(a >> 1, b >> 1), 255); break;
                case 10: case 11: if (!water) c = heat(w.xplc.at(a >> 2, b >> 2), 255); break;
                case 12: if (!water) c = heat(w.xplt.at(a >> 1, b >> 1), 255); break;
                case 13: if (!water) c = heat(w.xval.at(a >> 1, b >> 1), 255); break;
                case 14: case 15: if (!water) c = heat(w.xfir.at(a >> 2, b >> 2), 255); break;
                default: break;
            }
            px[static_cast<size_t>(b) * N + static_cast<size_t>(a)] = c;
        }
    if (!mapTex_) {
        mapTex_.reset(SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, N, N));
        if (mapTex_) SDL_SetTextureScaleMode(mapTex_.get(), SDL_SCALEMODE_NEAREST);
    }
    const int mx = x + 16, my = y + 8;
    if (mapTex_) {
        SDL_UpdateTexture(mapTex_.get(), nullptr, px.data(), N * 4);
        const SDL_FRect dst{static_cast<float>(mx), static_cast<float>(my), static_cast<float>(N * S3), static_cast<float>(N * S3)};
        SDL_RenderTexture(r, mapTex_.get(), nullptr, &dst);
    }
    frame(r, mx - 1, my - 1, N * S3 + 2, N * S3 + 2, 190, 200, 230);
    int va = 0, vb = 0;
    if (mapShowView_ && viewCenter && viewCenter(va, vb)) {
        const SDL_FRect box{static_cast<float>(mx + (va - 5) * S3), static_cast<float>(my + (vb - 5) * S3), 11.f * S3, 11.f * S3};
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        SDL_RenderRect(r, &box);
    }
    // layer list (strings 327..344) and the "Show City In Window" switch
    const int lx = x + 420;
    for (int i = 0; i < 18; ++i) {
        const bool on = i == layer;
        fill(r, lx, my + i * 18, 180, 16, on ? 70 : 34, on ? 100 : 40, on ? 150 : 52);
        text(r, lx + 8, my + 4 + i * 18, res_.string(327 + i), on ? 255 : 210, on ? 255 : 215, on ? 200 : 225);
    }
    const int by = my + 18 * 18 + 8;
    fill(r, lx, by, 180, 18, mapShowView_ ? 70 : 34, mapShowView_ ? 100 : 40, mapShowView_ ? 150 : 52);
    text(r, lx + 8, by + 5, "Show City In Window", 230, 230, 220);
}

bool InfoWindows::windowClick(int mx, int my, int x, int y) {   // (x, y) = top-left of the content area
    if (open == Which::Industry) {
        for (int i = 0; i < 3; ++i) if (mx >= x + 24 + i * 130 && mx < x + 144 + i * 130 && my >= y + 11 * 24 + 36 && my < y + 11 * 24 + 58) industryMode_ = i;
        return true;
    }
    if (open == Which::Map) {
        const int gx = x + 16, gy = y + 8;
        if (mx >= gx && my >= gy && mx < gx + 384 && my < gy + 384) { if (onMapClick) onMapClick((mx - gx) / 3, (my - gy) / 3); return true; }
        const int lx = x + 420;
        for (int i = 0; i < 18; ++i) if (mx >= lx && mx < lx + 180 && my >= gy + i * 18 && my < gy + i * 18 + 16) mapLayer_ = i;
        if (mx >= lx && mx < lx + 180 && my >= gy + 18 * 18 + 8 && my < gy + 18 * 18 + 26) mapShowView_ = !mapShowView_;
        return true;
    }
    return open == Which::Neighbors;
}

}  // namespace sc2k
