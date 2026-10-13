// 32-bit software framebuffer (0xAARRGGBB) used by the renderer; independent of any windowing library.
#pragma once
#include <cstdint>
#include <vector>

namespace sc2k {

struct Framebuffer {
    int width = 0, height = 0;
    std::vector<uint32_t> pixels;
    Framebuffer() = default;
    Framebuffer(int w, int h, uint32_t fill = 0xFF000000u) : width(w), height(h), pixels(static_cast<size_t>(w) * static_cast<size_t>(h), fill) {}
    void clear(uint32_t c) { std::fill(pixels.begin(), pixels.end(), c); }
};

// Writes an RGB PNG (stored deflate blocks, no compression). Returns false on I/O error.
bool writePng(const char* path, const Framebuffer& fb);

}  // namespace sc2k
