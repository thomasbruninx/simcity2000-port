#include <cstdlib>
#include <string>

#include "scurk/mif.h"
#include "test.h"

using namespace sc2k;

TEST(sprite_stream_encode_decode_round_trip) {
    Sprite s;
    s.width = 7; s.height = 3;
    s.index.assign(21, 0); s.opaque.assign(21, 0);
    auto set = [&](int x, int y, int v) { s.index[static_cast<size_t>(y * 7 + x)] = static_cast<uint8_t>(v); s.opaque[static_cast<size_t>(y * 7 + x)] = 1; };
    set(1, 0, 10); set(2, 0, 11); set(3, 0, 12); set(6, 1, 99); set(0, 2, 5);
    const auto bytes = encodeSpriteStream(s, 2);
    int endArg = -1;
    const Sprite d = decodeSpriteStream(bytes.data(), bytes.data() + bytes.size(), 7, 3, &endArg);
    CHECK(endArg == 2 && d.opaque == s.opaque);
    for (size_t i = 0; i < 21; ++i) if (s.opaque[i]) CHECK(d.index[i] == s.index[i]);
}

TEST(mif_files_of_the_installation_round_trip) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    for (const char* name : {"ORIGINAL", "FUTURE", "PARIS", "TOKYO"}) {
        const std::string path = std::string(root) + "/SCURKART/" + name + ".MIF";
        MifFile m = MifFile::load(path);
        CHECK(!m.shapes.empty());
        MifFile again = MifFile::parse(m.serialize());
        CHECK(again.shapes.size() == m.shapes.size() && again.info == m.info);
        bool same = true;
        for (size_t i = 0; i < m.shapes.size(); ++i) same = same && again.shapes[i].stream == m.shapes[i].stream && again.shapes[i].id == m.shapes[i].id;
        CHECK(same);
        // every real shape survives decode -> encode -> decode
        int checked = 0;
        for (const MifShape& s : m.shapes) {
            if (s.placeholder() || checked > 40) continue;
            const Sprite a = s.decode();
            MifShape t = s;
            t.setSprite(a);
            const Sprite b = t.decode();
            CHECK(a.opaque == b.opaque);
            for (size_t i = 0; i < a.index.size(); ++i) if (a.opaque[i] && a.index[i] != b.index[i]) { CHECK(false); break; }
            ++checked;
        }
    }
    // an edited shape keeps the rest of the file untouched
    MifFile m = MifFile::load(std::string(root) + "/SCURKART/ORIGINAL.MIF");
    const std::vector<uint8_t> before = m.serialize();
    for (auto& s : m.shapes) if (!s.placeholder()) { Sprite sp = s.decode(); if (!sp.index.empty()) { sp.index[0] ^= 1; sp.opaque[0] = 1; } s.setSprite(sp); break; }
    CHECK(m.serialize() != before);
}
