// SCURK's .MIF files (the "SimCity 2000 Urban Renewal Kit" object sets, docs/formats/mif.md): an IFF-like container
//   "MIFF" u32be size, "SC2K", then chunks  tag[4] u32be size body:  INFO (tool info), TILE (u16be count + sub-chunks SHAP / NAME).
// SHAP = u16be sprite id, u16be width, u16be height, u32be stream size, row-RLE pixel stream; NAME = u16be id, u16be length, text.
// A sprite id N is one object seen at three zooms: N (small, 1..499), N + 500 (medium), N + 1000 (large). Shapes of height 1 are placeholders
// ("use the original graphics"). Unchanged shapes keep their original bytes when the file is written.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "resources/sprite_archive.h"

namespace sc2k {

struct MifShape {
    int id = 0, width = 0, height = 0;
    std::vector<uint8_t> stream;   // as stored (keeps editor padding bytes)
    int endArg = 2;
    bool placeholder() const { return height <= 1; }
    Sprite decode() const;
    void setSprite(const Sprite& s);   // re-encodes the stream
};

struct MifFile {
    std::vector<uint8_t> info;                 // INFO body, kept as is
    std::vector<MifShape> shapes;              // in file order
    std::map<int, std::string> names;          // NAME chunks: object id -> custom name
    std::vector<std::pair<char, int>> order;   // file order of the sub-chunks: ('S', shape index) / ('N', name id)

    static MifFile load(const std::string& path);                  // throws Sc2Error
    static MifFile parse(const std::vector<uint8_t>& bytes);
    std::vector<uint8_t> serialize() const;
    void save(const std::string& path) const;
    MifShape* find(int id);
    const MifShape* find(int id) const;
    void put(const MifShape& s);               // replaces or appends a shape
};

}  // namespace sc2k
