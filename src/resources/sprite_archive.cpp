#include "resources/sprite_archive.h"

#include <fstream>

#include "save/error.h"

namespace sc2k {

namespace {
uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }
uint32_t be32(const uint8_t* p) { return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3]; }
}  // namespace

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
    Sprite s;
    s.width = e.width; s.height = e.height;
    s.index.assign(static_cast<size_t>(e.width) * static_cast<size_t>(e.height), 0);
    s.opaque.assign(s.index.size(), 0);
    // stream of 2-byte records [arg][op]: 0 nop, 1 new row, 3 skip, 4 literal run (padded to even), other = end
    const uint8_t* p = data_.data() + e.offset;
    const uint8_t* end = data_.data() + e.end;
    int y = -1, x = 0;
    while (p + 1 < end) {
        const int arg = p[0], op = p[1];
        p += 2;
        if (op == 0) continue;
        if (op == 1) { ++y; x = 0; if (y >= e.height) break; }
        else if (op == 3) x += arg;
        else if (op == 4) {
            if (y < 0) break;
            for (int k = 0; k < arg && p + k < end; ++k)
                if (x + k < e.width) { const size_t o = static_cast<size_t>(y) * static_cast<size_t>(e.width) + static_cast<size_t>(x + k); s.index[o] = p[k]; s.opaque[o] = 1; }
            x += arg;
            p += arg + (arg & 1);
        } else break;
    }
    return &cache_.emplace(id, std::move(s)).first->second;
}

SpriteSet SpriteSet::load(const std::string& gameDir) {
    return SpriteSet(SpriteArchive::load(gameDir + "/DATA/SMALLMED.DAT"), SpriteArchive::load(gameDir + "/DATA/LARGE.DAT"), SpriteArchive::load(gameDir + "/DATA/SPECIAL.DAT"));
}

const Sprite* SpriteSet::get(int id) const {
    if (id >= 1000) return large_.find(id);
    if (const Sprite* s = small_.find(id)) return s;
    return special_.find(id);
}

}  // namespace sc2k
