#include "resources/bmp.h"

#include <fstream>

#include "save/error.h"

namespace sc2k {

namespace {
uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t{p[3]} << 24); }
uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
}  // namespace

Bmp8 Bmp8::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw Sc2Error("cannot open " + path);
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (d.size() < 54 || d[0] != 'B' || d[1] != 'M') throw Sc2Error("not a BMP: " + path);
    const uint32_t dataOff = le32(&d[10]), hdr = le32(&d[14]);
    Bmp8 b;
    b.width = static_cast<int>(le32(&d[18]));
    const int32_t h = static_cast<int32_t>(le32(&d[22]));
    b.height = h < 0 ? -h : h;
    if (le16(&d[28]) != 8 || le32(&d[30]) != 0) throw Sc2Error("unsupported BMP (need 8-bit uncompressed): " + path);
    uint32_t colors = le32(&d[46]);
    if (colors == 0) colors = 256;
    for (uint32_t i = 0; i < colors && i < 256 && 14 + hdr + 4 * i + 3 < d.size(); ++i) {
        const uint8_t* c = &d[14 + hdr + 4 * i];
        b.palette[i] = 0xFF000000u | (uint32_t{c[2]} << 16) | (uint32_t{c[1]} << 8) | c[0];
    }
    const size_t stride = (static_cast<size_t>(b.width) + 3) & ~size_t{3};
    if (dataOff + stride * static_cast<size_t>(b.height) > d.size()) { b.index.clear(); return b; }   // palette-only file
    b.index.resize(static_cast<size_t>(b.width) * static_cast<size_t>(b.height));
    for (int y = 0; y < b.height; ++y) {
        const int srcRow = h > 0 ? b.height - 1 - y : y;
        for (int x = 0; x < b.width; ++x) b.index[static_cast<size_t>(y) * static_cast<size_t>(b.width) + static_cast<size_t>(x)] = d[dataOff + static_cast<size_t>(srcRow) * stride + static_cast<size_t>(x)];
    }
    return b;
}

}  // namespace sc2k
