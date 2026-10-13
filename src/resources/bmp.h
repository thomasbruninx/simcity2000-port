// Minimal reader for the 8-bit Windows BMP files of the game (palettes and images).
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace sc2k {

struct Bmp8 {
    int width = 0, height = 0;
    std::array<uint32_t, 256> palette{};   // 0xAARRGGBB, alpha 255
    std::vector<uint8_t> index;            // top-down rows, width*height
    static Bmp8 load(const std::string& path);   // throws Sc2Error
};

}  // namespace sc2k
