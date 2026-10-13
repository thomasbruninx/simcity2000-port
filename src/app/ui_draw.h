// Tiny SDL_Renderer drawing helpers shared by the UI windows (rectangles and SDL's built-in 8x8 debug font).
#pragma once
#include <SDL3/SDL.h>

#include <string>

namespace sc2k::ui {

inline void fill(SDL_Renderer* r, int x, int y, int w, int h, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca = 255) {
    SDL_SetRenderDrawBlendMode(r, ca < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    const SDL_FRect f{static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)};
    SDL_RenderFillRect(r, &f);
}
inline void frame(SDL_Renderer* r, int x, int y, int w, int h, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    const SDL_FRect f{static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)};
    SDL_RenderRect(r, &f);
}
inline void text(SDL_Renderer* r, int x, int y, const std::string& s, Uint8 cr = 235, Uint8 cg = 235, Uint8 cb = 235) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderDebugText(r, static_cast<float>(x), static_cast<float>(y), s.c_str());
}
inline std::string money(long long v) {
    std::string digits = std::to_string(v < 0 ? -v : v), out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i && (digits.size() - i) % 3 == 0) out.push_back(',');
        out.push_back(digits[i]);
    }
    return (v < 0 ? "-$" : "$") + out;
}

}  // namespace sc2k::ui
