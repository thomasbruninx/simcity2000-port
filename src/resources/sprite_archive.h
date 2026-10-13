// Sprite archives LARGE.DAT / SMALLMED.DAT / SPECIAL.DAT of SimCity 2000 (docs/formats/sprite-archives.md).
// Pixels are 8-bit palette indices (palette: BITMAPS/PAL_MSTR.BMP); transparency is a separate mask.
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace sc2k {

struct Sprite {
    int width = 0, height = 0;
    std::vector<uint8_t> index;   // width*height palette indices
    std::vector<uint8_t> opaque;  // width*height, 1 = drawn
};

class SpriteArchive {
public:
    static SpriteArchive load(const std::string& path);   // throws Sc2Error
    size_t count() const { return entries_.size(); }
    // First sprite with this id (ids repeat in a few places; meaning unknown), decoded on first use. nullptr if absent.
    const Sprite* find(int id) const;

private:
    struct Entry { int id, width, height; uint32_t offset, end; };
    std::vector<uint8_t> data_;
    std::vector<Entry> entries_;
    std::unordered_map<int, size_t> byId_;
    mutable std::unordered_map<int, Sprite> cache_;
};

// The three archives of an installation, addressed like the game does: ids 1..999 in SMALLMED, 1000..1499 in LARGE, 573..755 also in SPECIAL.
class SpriteSet {
public:
    static SpriteSet load(const std::string& gameDir);    // gameDir contains DATA/
    const Sprite* get(int id) const;
private:
    SpriteArchive small_, large_, special_;
    SpriteSet(SpriteArchive s, SpriteArchive l, SpriteArchive p) : small_(std::move(s)), large_(std::move(l)), special_(std::move(p)) {}
};

}  // namespace sc2k
