#include "resources/pe_image.h"

#include <cstring>
#include <fstream>
#include <iterator>

#include "save/error.h"

namespace sc2k {
namespace {
uint32_t rd32(const std::vector<uint8_t>& d, size_t o) {
    if (o + 4 > d.size()) throw Sc2Error("PE: truncated header");
    uint32_t v; std::memcpy(&v, d.data() + o, 4); return v;   // little-endian host (arm64/x86_64)
}
uint16_t rd16(const std::vector<uint8_t>& d, size_t o) {
    if (o + 2 > d.size()) throw Sc2Error("PE: truncated header");
    uint16_t v; std::memcpy(&v, d.data() + o, 2); return v;
}
}  // namespace

PeImage PeImage::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw Sc2Error("cannot open " + path);
    PeImage p;
    p.file_.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    const auto& d = p.file_;
    if (d.size() < 0x40 || d[0] != 'M' || d[1] != 'Z') throw Sc2Error("not an MZ executable");
    const size_t pe = rd32(d, 0x3C);
    if (pe + 24 > d.size() || std::memcmp(d.data() + pe, "PE\0\0", 4) != 0) throw Sc2Error("no PE signature");
    const uint16_t nsec = rd16(d, pe + 6);
    p.timestamp_ = rd32(d, pe + 8);
    const uint16_t optSize = rd16(d, pe + 20);
    const size_t opt = pe + 24;
    if (rd16(d, opt) != 0x10B) throw Sc2Error("only PE32 is supported");
    p.imageBase_ = rd32(d, opt + 28);
    p.sizeOfImage_ = rd32(d, opt + 56);
    if (rd32(d, opt + 92) > 2) p.resourceRva_ = rd32(d, opt + 96 + 2 * 8);   // NumberOfRvaAndSizes, data directory[2] = resources
    size_t so = opt + optSize;
    for (uint16_t i = 0; i < nsec; ++i, so += 40) {
        Section s{rd32(d, so + 12), rd32(d, so + 8), rd32(d, so + 20), rd32(d, so + 16)};
        p.sections_.push_back(s);
    }
    return p;
}

std::span<const uint8_t> PeImage::bytesAt(uint32_t va, size_t n) const {
    const uint32_t rva = va - imageBase_;
    for (const auto& s : sections_) {
        if (rva >= s.va && rva + n <= s.va + s.rawSize && s.rawPtr + (rva - s.va) + n <= file_.size())
            return {file_.data() + s.rawPtr + (rva - s.va), n};
    }
    throw Sc2Error("PE: address not backed by file data");
}

}  // namespace sc2k
