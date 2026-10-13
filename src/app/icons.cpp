#include "app/icons.h"

namespace sc2k {

IconStrip::IconStrip(SDL_Renderer* r, const Bmp8& b, int cellWidth, bool segmented) : cellW_(cellWidth), h_(b.height) {
    const uint8_t bg0 = b.index.empty() ? 0 : b.index[0];
    if (segmented) {
        int start = -1;
        for (int x = 0; x <= b.width; ++x) {
            bool empty = true;
            if (x < b.width) for (int y = 0; y < b.height && empty; ++y) empty = b.index[static_cast<size_t>(y) * static_cast<size_t>(b.width) + static_cast<size_t>(x)] == bg0;
            if (!empty && start < 0) start = x;
            if (empty && start >= 0) { runs_.emplace_back(start, x); start = -1; }
        }
    } else
        for (int c = 0; c < b.width / cellWidth; ++c) runs_.emplace_back(c * cellWidth, (c + 1) * cellWidth);
    cells_ = static_cast<int>(runs_.size());
    std::vector<uint32_t> px(b.index.size());
    const uint8_t bg = b.index.empty() ? 0 : b.index[0];
    for (size_t i = 0; i < px.size(); ++i) px[i] = b.index[i] == bg ? 0u : b.palette[b.index[i]];
    tex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, b.width, b.height);
    if (!tex_) return;
    SDL_UpdateTexture(tex_, nullptr, px.data(), b.width * 4);
    SDL_SetTextureBlendMode(tex_, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex_, SDL_SCALEMODE_NEAREST);
}

IconStrip::~IconStrip() { if (tex_) SDL_DestroyTexture(tex_); }

void IconStrip::draw(SDL_Renderer* r, int cell, int x, int y, int scale) const {
    if (!tex_ || cell < 0 || cell >= cells_) return;
    const auto [x0, x1] = runs_[static_cast<size_t>(cell)];
    const int w = x1 - x0, pad = (cellW_ - w) / 2;
    const SDL_FRect src{static_cast<float>(x0), 0.f, static_cast<float>(w), static_cast<float>(h_)};
    const SDL_FRect dst{static_cast<float>(x + pad * scale), static_cast<float>(y), static_cast<float>(w * scale), static_cast<float>(h_ * scale)};
    SDL_RenderTexture(r, tex_, &src, &dst);
}

void IconStrip::drawSized(SDL_Renderer* r, int cell, int x, int y, int w, int h) const {
    if (!tex_ || cell < 0 || cell >= cells_) return;
    const auto [x0, x1] = runs_[static_cast<size_t>(cell)];
    const SDL_FRect src{static_cast<float>(x0), 0.f, static_cast<float>(x1 - x0), static_cast<float>(h_)};
    const SDL_FRect dst{static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)};
    SDL_RenderTexture(r, tex_, &src, &dst);
}

}  // namespace sc2k
