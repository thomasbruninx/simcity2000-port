#include <cstdio>

#include "render/framebuffer.h"

namespace sc2k {

namespace {
uint32_t crcTable[256];
bool crcInit = false;
uint32_t crc(const uint8_t* p, size_t n, uint32_t c = 0xFFFFFFFFu) {
    if (!crcInit) {
        for (uint32_t i = 0; i < 256; ++i) { uint32_t v = i; for (int k = 0; k < 8; ++k) v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1; crcTable[i] = v; }
        crcInit = true;
    }
    for (size_t i = 0; i < n; ++i) c = crcTable[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c;
}
void put32(std::vector<uint8_t>& v, uint32_t x) { for (int s = 24; s >= 0; s -= 8) v.push_back(static_cast<uint8_t>(x >> s)); }
void chunk(FILE* f, const char* type, const std::vector<uint8_t>& body) {
    std::vector<uint8_t> buf;
    put32(buf, static_cast<uint32_t>(body.size()));
    buf.insert(buf.end(), type, type + 4);
    buf.insert(buf.end(), body.begin(), body.end());
    const uint32_t c = ~crc(buf.data() + 4, buf.size() - 4);
    put32(buf, c);
    std::fwrite(buf.data(), 1, buf.size(), f);
}
}  // namespace

bool writePng(const char* path, const Framebuffer& fb) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::fwrite(sig, 1, 8, f);
    std::vector<uint8_t> ihdr;
    put32(ihdr, static_cast<uint32_t>(fb.width)); put32(ihdr, static_cast<uint32_t>(fb.height));
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
    chunk(f, "IHDR", ihdr);
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(fb.height) * (static_cast<size_t>(fb.width) * 3 + 1));
    for (int y = 0; y < fb.height; ++y) {
        raw.push_back(0);
        for (int x = 0; x < fb.width; ++x) {
            const uint32_t p = fb.pixels[static_cast<size_t>(y) * static_cast<size_t>(fb.width) + static_cast<size_t>(x)];
            raw.push_back(static_cast<uint8_t>(p >> 16)); raw.push_back(static_cast<uint8_t>(p >> 8)); raw.push_back(static_cast<uint8_t>(p));
        }
    }
    std::vector<uint8_t> z = {0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    for (size_t pos = 0; pos < raw.size();) {
        const size_t n = std::min<size_t>(65535, raw.size() - pos);
        z.push_back(pos + n >= raw.size() ? 1 : 0);
        z.push_back(static_cast<uint8_t>(n)); z.push_back(static_cast<uint8_t>(n >> 8));
        z.push_back(static_cast<uint8_t>(~n)); z.push_back(static_cast<uint8_t>((~n) >> 8));
        z.insert(z.end(), raw.begin() + static_cast<long>(pos), raw.begin() + static_cast<long>(pos + n));
        pos += n;
    }
    put32(z, (b << 16) | a);
    chunk(f, "IDAT", z);
    chunk(f, "IEND", {});
    std::fclose(f);
    return true;
}

}  // namespace sc2k
