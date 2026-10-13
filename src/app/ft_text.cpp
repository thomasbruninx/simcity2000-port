#include "app/ft_text.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#ifdef SC2K_HAVE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

namespace sc2k {

namespace {
#ifdef SC2K_HAVE_FREETYPE
unsigned codepoint(unsigned char b) {   // the game's text is Windows-1252
    static const unsigned hi[32] = {0x20AC, 0x81, 0x201A, 0x192, 0x201E, 0x2026, 0x2020, 0x2021, 0x2C6, 0x2030, 0x160, 0x2039, 0x152, 0x8D, 0x17D, 0x8F,
                                    0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x2DC, 0x2122, 0x161, 0x203A, 0x153, 0x9D, 0x17E, 0x178};
    return b >= 0x80 && b < 0xA0 ? hi[b - 0x80] : b;
}
#endif
}  // namespace

TextRenderer::TextRenderer(SDL_Renderer* r) : r_(r) {
#ifdef SC2K_HAVE_FREETYPE
    FT_Library lib = nullptr;
    if (FT_Init_FreeType(&lib) != 0) return;
    lib_ = lib;
    std::vector<std::string> candidates;
    if (const char* e = std::getenv("SC2K_FONT")) candidates.push_back(e);
    for (const char* p : {"/System/Library/Fonts/Supplemental/Times New Roman.ttf", "/Library/Fonts/Times New Roman.ttf", "/System/Library/Fonts/Supplemental/Times New Roman.ttf",
                          "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman.ttf", "/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf",
                          "/System/Library/Fonts/Supplemental/Georgia.ttf", "/System/Library/Fonts/Times.ttc"})
        candidates.push_back(p);
    for (const std::string& c : candidates) {
        FT_Face f = nullptr;
        if (FT_New_Face(lib, c.c_str(), 0, &f) == 0) { face_ = f; break; }
    }
    for (const char* p : {"/System/Library/Fonts/Supplemental/Times New Roman Bold.ttf", "/Library/Fonts/Times New Roman Bold.ttf", "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold.ttf", "/usr/share/fonts/truetype/liberation/LiberationSerif-Bold.ttf"}) {
        FT_Face f = nullptr;
        if (FT_New_Face(lib, p, 0, &f) == 0) { faceBold_ = f; break; }
    }
#endif
}

TextRenderer::~TextRenderer() {
    for (auto& [k, e] : cache_) if (e.tex) SDL_DestroyTexture(e.tex);
#ifdef SC2K_HAVE_FREETYPE
    if (face_) FT_Done_Face(static_cast<FT_Face>(face_));
    if (faceBold_) FT_Done_Face(static_cast<FT_Face>(faceBold_));
    if (lib_) FT_Done_FreeType(static_cast<FT_Library>(lib_));
#endif
}

int TextRenderer::width(const std::string& text, int px, bool bold) {
    const std::string key = std::to_string(px) + (bold ? "b:" : ":") + text;
    if (auto it = widths_.find(key); it != widths_.end()) return it->second;
    int w = 0;
#ifdef SC2K_HAVE_FREETYPE
    if (face_) {
        FT_Face f = static_cast<FT_Face>(faceFor(bold));
        FT_Set_Pixel_Sizes(f, 0, static_cast<FT_UInt>(std::max(1, px)));
        FT_UInt prev = 0;
        long pen = 0;
        for (unsigned char ch : text) {
            const FT_UInt gi = FT_Get_Char_Index(f, codepoint(ch));
            if (prev && FT_HAS_KERNING(f)) { FT_Vector k; FT_Get_Kerning(f, prev, gi, FT_KERNING_DEFAULT, &k); pen += k.x; }
            FT_Load_Glyph(f, gi, FT_LOAD_DEFAULT);
            pen += f->glyph->advance.x;
            prev = gi;
        }
        w = static_cast<int>((pen + 63) >> 6);
    } else
#endif
        w = static_cast<int>(text.size()) * std::max(1, px / 2);
    if (widths_.size() > 4000) widths_.clear();
    widths_[key] = w;
    return w;
}

TextRenderer::Entry& TextRenderer::render(const std::string& text, int px, bool bold) {
    const std::string key = std::to_string(px) + (bold ? "b:" : ":") + text;
    if (auto it = cache_.find(key); it != cache_.end()) return it->second;
    if (cache_.size() > 600) { for (auto& [k, e] : cache_) if (e.tex) SDL_DestroyTexture(e.tex); cache_.clear(); }
    Entry e;
#ifdef SC2K_HAVE_FREETYPE
    if (face_ && !text.empty()) {
        FT_Face f = static_cast<FT_Face>(faceFor(bold));
        FT_Set_Pixel_Sizes(f, 0, static_cast<FT_UInt>(std::max(1, px)));
        const int asc = static_cast<int>((f->size->metrics.ascender + 63) >> 6), desc = static_cast<int>((-f->size->metrics.descender + 63) >> 6);
        const int w = width(text, px, bold) + 2, h = asc + desc + 1;
        std::vector<uint8_t> a(static_cast<size_t>(w) * h, 0);
        FT_UInt prev = 0;
        long pen = 0;
        for (unsigned char ch : text) {
            const FT_UInt gi = FT_Get_Char_Index(f, codepoint(ch));
            if (prev && FT_HAS_KERNING(f)) { FT_Vector k; FT_Get_Kerning(f, prev, gi, FT_KERNING_DEFAULT, &k); pen += k.x; }
            if (FT_Load_Glyph(f, gi, FT_LOAD_RENDER) == 0) {
                const FT_GlyphSlot g = f->glyph;
                const int gx = static_cast<int>(pen >> 6) + g->bitmap_left, gy = asc - g->bitmap_top;
                for (unsigned row = 0; row < g->bitmap.rows; ++row)
                    for (unsigned col = 0; col < g->bitmap.width; ++col) {
                        const int X = gx + static_cast<int>(col), Y = gy + static_cast<int>(row);
                        if (X < 0 || Y < 0 || X >= w || Y >= h) continue;
                        uint8_t& d = a[static_cast<size_t>(Y) * w + X];
                        d = static_cast<uint8_t>(std::min(255, d + g->bitmap.buffer[row * g->bitmap.pitch + col]));
                    }
                pen += g->advance.x;
            }
            prev = gi;
        }
        SDL_Surface* s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
        if (s) {
            for (int y = 0; y < h; ++y) {
                Uint8* row = static_cast<Uint8*>(s->pixels) + static_cast<size_t>(y) * s->pitch;
                for (int x = 0; x < w; ++x) { row[x * 4] = row[x * 4 + 1] = row[x * 4 + 2] = 255; row[x * 4 + 3] = a[static_cast<size_t>(y) * w + x]; }
            }
            e.tex = SDL_CreateTextureFromSurface(r_, s);
            if (e.tex) SDL_SetTextureBlendMode(e.tex, SDL_BLENDMODE_BLEND);
            e.w = w; e.h = h;
            SDL_DestroySurface(s);
        }
    }
#endif
    return cache_[key] = e;
}

void TextRenderer::draw(const std::string& text, int x, int y, int px, Uint8 cr, Uint8 cg, Uint8 cb, bool bold) {
    if (text.empty()) return;
    Entry& e = render(text, px, bold);
    if (e.tex) {
        SDL_SetTextureColorMod(e.tex, cr, cg, cb);
        const SDL_FRect d{static_cast<float>(x), static_cast<float>(y), static_cast<float>(e.w), static_cast<float>(e.h)};
        SDL_RenderTexture(r_, e.tex, nullptr, &d);
        return;
    }
    const float sc = std::max(1.0f, px / 8.0f);   // fallback: scaled debug font
    SDL_SetRenderScale(r_, sc, sc);
    SDL_SetRenderDrawColor(r_, cr, cg, cb, 255);
    SDL_RenderDebugText(r_, x / sc, y / sc, text.c_str());
    SDL_SetRenderScale(r_, 1, 1);
}

}  // namespace sc2k
