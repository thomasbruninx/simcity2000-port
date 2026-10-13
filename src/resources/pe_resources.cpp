#include "resources/pe_resources.h"

#include <cstring>

namespace sc2k {
namespace {
struct Rd {
    const PeImage& pe;
    uint32_t u32(uint32_t rva) const { uint32_t v; std::memcpy(&v, pe.bytesAtRva(rva, 4).data(), 4); return v; }
    uint16_t u16(uint32_t rva) const { uint16_t v; std::memcpy(&v, pe.bytesAtRva(rva, 2).data(), 2); return v; }
};
struct Entry { uint32_t name; uint32_t offset; };   // offset: high bit = subdirectory
std::vector<Entry> entries(const Rd& r, uint32_t dir) {
    std::vector<Entry> v;
    const int n = r.u16(dir + 12) + r.u16(dir + 14);
    for (int i = 0; i < n; ++i) v.push_back({r.u32(dir + 16 + 8u * static_cast<uint32_t>(i)), r.u32(dir + 20 + 8u * static_cast<uint32_t>(i))});
    return v;
}
}  // namespace

PeResources::PeResources(const PeImage& pe) {
    const uint32_t root = pe.resourceRva();
    if (!root) return;
    Rd r{pe};
    for (const Entry& type : entries(r, root)) {
        if (!(type.offset & 0x80000000u) || (type.name & 0x80000000u)) continue;
        const int typeId = static_cast<int>(type.name);
        if (typeId != 2 && typeId != 6) continue;
        for (const Entry& name : entries(r, root + (type.offset & 0x7FFFFFFFu))) {
            if (!(name.offset & 0x80000000u) || (name.name & 0x80000000u)) continue;
            const auto langs = entries(r, root + (name.offset & 0x7FFFFFFFu));
            if (langs.empty() || (langs[0].offset & 0x80000000u)) continue;
            const uint32_t leaf = root + langs[0].offset;
            const uint32_t dataRva = r.u32(leaf), size = r.u32(leaf + 4);
            const int id = static_cast<int>(name.name);
            if (typeId == 2) { const auto b = pe.bytesAtRva(dataRva, size); bitmaps_[id].assign(b.begin(), b.end()); }
            else {   // string table block: 16 length-prefixed UTF-16 strings; block id N holds ids (N-1)*16 .. (N-1)*16+15
                uint32_t p = dataRva;
                for (int k = 0; k < 16 && p < dataRva + size; ++k) {
                    const int len = r.u16(p);
                    p += 2;
                    std::string s;
                    for (int j = 0; j < len; ++j) { const uint16_t ch = r.u16(p + 2u * static_cast<uint32_t>(j)); s.push_back(ch < 0x80 ? static_cast<char>(ch) : '?'); }
                    p += 2u * static_cast<uint32_t>(len);
                    if (len) strings_[(id - 1) * 16 + k] = s;
                }
            }
        }
    }
}

std::string PeResources::string(int id) const {
    const auto it = strings_.find(id);
    return it == strings_.end() ? std::string() : it->second;
}

std::vector<uint8_t> PeResources::bitmap(int id) const {
    const auto it = bitmaps_.find(id);
    return it == bitmaps_.end() ? std::vector<uint8_t>() : it->second;
}

std::vector<int> PeResources::bitmapIds() const {
    std::vector<int> v;
    for (const auto& kv : bitmaps_) v.push_back(kv.first);
    return v;
}

}  // namespace sc2k
