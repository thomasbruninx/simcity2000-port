// Button bitmaps taken from the player's SIMCITY.EXE (RT_BITMAP): the main toolbar strip (resource 2, 23-pixel cells) and the terrain strip
// (resource 207, 19-pixel cells). The background colour of the strips (the top-left pixel) is made transparent.
#pragma once
#include <SDL3/SDL.h>

#include <memory>
#include <vector>

#include "resources/bmp.h"

namespace sc2k {

class IconStrip {
public:
    // cellWidth: width of a button cell. segmented: icons are the runs of non-background columns (the main strip is not a uniform grid) and are
    // centred in a cell when drawn.
    IconStrip(SDL_Renderer* r, const Bmp8& bitmap, int cellWidth, bool segmented);
    ~IconStrip();
    IconStrip(const IconStrip&) = delete;
    IconStrip& operator=(const IconStrip&) = delete;
    int cells() const { return cells_; }
    int cellWidth() const { return cellW_; }
    int height() const { return h_; }
    void draw(SDL_Renderer* r, int cell, int x, int y, int scale = 1) const;
    void drawSized(SDL_Renderer* r, int cell, int x, int y, int w, int h) const;   // stretched to w x h

private:
    SDL_Texture* tex_ = nullptr;
    int cellW_ = 0, h_ = 0, cells_ = 0;
    std::vector<std::pair<int, int>> runs_;   // [x0, x1) of every icon
};

}  // namespace sc2k
