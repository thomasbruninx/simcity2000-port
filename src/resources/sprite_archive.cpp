#include "resources/sprite_archive.h"

#include <algorithm>
#include <fstream>

#include "save/error.h"

namespace sc2k {

namespace {
uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }
uint32_t be32(const uint8_t* p) { return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3]; }
}  // namespace

Sprite decodeSpriteStream(const uint8_t* p, const uint8_t* end, int width, int height, int* endArg) {
    Sprite s;
    s.width = width; s.height = height;
    s.index.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
    s.opaque.assign(s.index.size(), 0);
    // stream of 2-byte records [arg][op]: 0 nop, 1 new row, 3 skip, 4 literal run (padded to even), other = end
    int y = -1, x = 0;
    while (p + 1 < end) {
        const int arg = p[0], op = p[1];
        p += 2;
        if (op == 0) continue;
        if (op == 1) { ++y; x = 0; if (y >= height) break; }
        else if (op == 3) x += arg;
        else if (op == 4) {
            if (y < 0) break;
            for (int k = 0; k < arg && p + k < end; ++k)
                if (x + k < width) { const size_t o = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x + k); s.index[o] = p[k]; s.opaque[o] = 1; }
            x += arg;
            p += arg + (arg & 1);
        } else { if (endArg) *endArg = arg; break; }
    }
    return s;
}

std::vector<uint8_t> encodeSpriteStream(const Sprite& s, int endArg) {
    std::vector<uint8_t> out;
    for (int y = 0; y < s.height; ++y) {
        std::vector<uint8_t> row;
        int x = 0;
        auto at = [&](int xx) { return s.opaque[static_cast<size_t>(y) * static_cast<size_t>(s.width) + static_cast<size_t>(xx)] != 0; };
        while (x < s.width) {
            if (!at(x)) {
                const int start = x;
                while (x < s.width && !at(x)) ++x;
                if (x == s.width) break;   // transparent rest of the row is not stored
                for (int n = x - start; n > 0;) { const int m = std::min(n, 255); row.push_back(static_cast<uint8_t>(m)); row.push_back(3); n -= m; }
            } else {
                const int start = x;
                while (x < s.width && at(x) && x - start < 255) ++x;
                const int n = x - start;
                row.push_back(static_cast<uint8_t>(n)); row.push_back(4);
                for (int k = start; k < x; ++k) row.push_back(s.index[static_cast<size_t>(y) * static_cast<size_t>(s.width) + static_cast<size_t>(k)]);
                if (n & 1) row.push_back(0);
            }
        }
        out.push_back(static_cast<uint8_t>(row.size())); out.push_back(1);
        out.insert(out.end(), row.begin(), row.end());
    }
    out.push_back(static_cast<uint8_t>(endArg)); out.push_back(2);
    return out;
}

SpriteArchive SpriteArchive::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw Sc2Error("cannot open sprite archive " + path);
    SpriteArchive a;
    a.data_.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    if (a.data_.size() < 2) throw Sc2Error("sprite archive too small: " + path);
    const size_t n = be16(a.data_.data());
    if (2 + n * 10 > a.data_.size()) throw Sc2Error("sprite archive index truncated: " + path);
    for (size_t i = 0; i < n; ++i) {
        const uint8_t* e = a.data_.data() + 2 + 10 * i;
        Entry en{be16(e), be16(e + 8), be16(e + 6), be32(e + 2), 0};   // id, width, height, offset
        a.entries_.push_back(en);
    }
    for (size_t i = 0; i < n; ++i) {
        a.entries_[i].end = i + 1 < n ? a.entries_[i + 1].offset : static_cast<uint32_t>(a.data_.size());
        if (a.entries_[i].offset > a.data_.size() || a.entries_[i].end > a.data_.size() || a.entries_[i].end < a.entries_[i].offset) throw Sc2Error("sprite entry out of range: " + path);
        a.byId_.emplace(a.entries_[i].id, i);
    }
    return a;
}

const Sprite* SpriteArchive::find(int id) const {
    auto it = cache_.find(id);
    if (it != cache_.end()) return &it->second;
    auto b = byId_.find(id);
    if (b == byId_.end()) return nullptr;
    const Entry& e = entries_[b->second];
    Sprite s = decodeSpriteStream(data_.data() + e.offset, data_.data() + e.end, e.width, e.height);
    return &cache_.emplace(id, std::move(s)).first->second;
}

SpriteSet SpriteSet::load(const std::string& gameDir) {
    return SpriteSet(SpriteArchive::load(gameDir + "/DATA/SMALLMED.DAT"), SpriteArchive::load(gameDir + "/DATA/LARGE.DAT"), SpriteArchive::load(gameDir + "/DATA/SPECIAL.DAT"));
}

const Sprite* SpriteSet::get(int id) const {
    if (!overrides_.empty()) { const auto it = overrides_.find(id); if (it != overrides_.end()) return &it->second; }
    if (id >= 1000) return large_.find(id);
    if (const Sprite* s = small_.find(id)) return s;
    return special_.find(id);
}

}  // namespace sc2k
