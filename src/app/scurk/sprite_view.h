// Draws sprites (palette-indexed, transparent where not opaque) through cached SDL textures.
#pragma once
#include <SDL3/SDL.h>

#include <array>
#include <string>
#include <unordered_map>

#include "resources/sprite_archive.h"

namespace sc2k::scurk {

class SpriteTextures {
public:
    ~SpriteTextures() { clear(); }
    // `key` identifies the picture (include a revision number): the same key returns the same texture without re-uploading.
    SDL_Texture* get(SDL_Renderer* r, const std::string& key, const Sprite& s, const std::array<uint32_t, 256>& palette);
    void clear();
    // Draws `s` (via the cache) scaled by `scale` with its top-left at (x, y); returns false when the sprite is empty.
    bool draw(SDL_Renderer* r, const std::string& key, const Sprite& s, const std::array<uint32_t, 256>& palette, float x, float y, float scale);
    // Draws it scaled down / up to fit a w x h box, centred (thumbnails).
    bool drawFit(SDL_Renderer* r, const std::string& key, const Sprite& s, const std::array<uint32_t, 256>& palette, float x, float y, float w, float h);
private:
    std::unordered_map<std::string, SDL_Texture*> cache_;
};

}  // namespace sc2k::scurk
