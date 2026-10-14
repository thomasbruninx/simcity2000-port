#include "app/scurk/sprite_view.h"

#include <algorithm>
#include <vector>

namespace sc2k::scurk {

SDL_Texture* SpriteTextures::get(SDL_Renderer* r, const std::string& key, const Sprite& s, const std::array<uint32_t, 256>& palette) {
    if (s.width <= 0 || s.height <= 0) return nullptr;
    if (auto it = cache_.find(key); it != cache_.end()) return it->second;
    if (cache_.size() > 600) clear();
    std::vector<uint32_t> px(s.index.size());
    for (size_t i = 0; i < px.size(); ++i) px[i] = s.opaque[i] ? (palette[s.index[i]] | 0xFF000000u) : 0u;
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, s.width, s.height);
    if (t) {
        SDL_UpdateTexture(t, nullptr, px.data(), s.width * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    }
    cache_[key] = t;
    return t;
}

void SpriteTextures::clear() {
    for (auto& [k, t] : cache_) if (t) SDL_DestroyTexture(t);
    cache_.clear();
}

bool SpriteTextures::draw(SDL_Renderer* r, const std::string& key, const Sprite& s, const std::array<uint32_t, 256>& palette, float x, float y, float scale) {
    SDL_Texture* t = get(r, key, s, palette);
    if (!t) return false;
    const SDL_FRect d{x, y, s.width * scale, s.height * scale};
    SDL_RenderTexture(r, t, nullptr, &d);
    return true;
}

bool SpriteTextures::drawFit(SDL_Renderer* r, const std::string& key, const Sprite& s, const std::array<uint32_t, 256>& palette, float x, float y, float w, float h) {
    if (s.width <= 0 || s.height <= 0) return false;
    float scale = std::min(w / static_cast<float>(s.width), h / static_cast<float>(s.height));
    if (scale >= 1.f) scale = static_cast<float>(static_cast<int>(scale));   // whole-number magnification keeps pixels sharp
    return draw(r, key, s, palette, x + (w - s.width * scale) / 2, y + (h - s.height * scale) / 2, scale);
}

}  // namespace sc2k::scurk
