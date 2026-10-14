#include "scurk/mif.h"

#include <fstream>

#include "save/error.h"

namespace sc2k {

namespace {
uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }
uint32_t be32(const uint8_t* p) { return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3]; }
void put16(std::vector<uint8_t>& v, unsigned x) { v.push_back(static_cast<uint8_t>(x >> 8)); v.push_back(static_cast<uint8_t>(x)); }
void put32(std::vector<uint8_t>& v, uint32_t x) { for (int s = 24; s >= 0; s -= 8) v.push_back(static_cast<uint8_t>(x >> s)); }
void putTag(std::vector<uint8_t>& v, const char* t) { for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>(t[i])); }
}  // namespace

Sprite MifShape::decode() const { return decodeSpriteStream(stream.data(), stream.data() + stream.size(), width, height); }

void MifShape::setSprite(const Sprite& s) {
    width = s.width; height = s.height;
    stream = encodeSpriteStream(s, endArg);
}

MifFile MifFile::parse(const std::vector<uint8_t>& d) {
    if (d.size() < 16 || std::string(d.begin(), d.begin() + 4) != "MIFF" || std::string(d.begin() + 8, d.begin() + 12) != "SC2K") throw Sc2Error("not a MIF file");
    MifFile m;
    size_t o = 12;
    while (o + 8 <= d.size()) {
        const std::string tag(d.begin() + static_cast<long>(o), d.begin() + static_cast<long>(o) + 4);
        const size_t size = be32(&d[o + 4]);
        if (o + 8 + size > d.size()) throw Sc2Error("MIF chunk " + tag + " runs past the end of the file");
        const uint8_t* body = &d[o + 8];
        if (tag == "INFO") m.info.assign(body, body + size);
        else if (tag == "TILE" && size >= 2) {
            const int count = be16(body);
            size_t p = 2;
            for (int i = 0; i < count && p + 8 <= size; ++i) {
                const std::string t(reinterpret_cast<const char*>(body + p), 4);
                const size_t s = be32(body + p + 4);
                if (p + 8 + s > size) throw Sc2Error("MIF sub-chunk runs past its chunk");
                const uint8_t* b = body + p + 8;
                if (t == "SHAP" && s >= 10) {
                    MifShape sh;
                    sh.id = be16(b); sh.width = be16(b + 2); sh.height = be16(b + 4);
                    const size_t n = std::min<size_t>(be32(b + 6), s - 10);
                    sh.stream.assign(b + 10, b + 10 + n);
                    if (n >= 2) sh.endArg = sh.stream[n - 2];
                    m.order.emplace_back('S', static_cast<int>(m.shapes.size()));
                    m.shapes.push_back(std::move(sh));
                } else if (t == "NAME" && s >= 4) {
                    const int id = be16(b);
                    const size_t len = std::min<size_t>(be16(b + 2), s - 4);
                    m.names[id] = std::string(reinterpret_cast<const char*>(b + 4), len);
                    m.order.emplace_back('N', id);
                }
                p += 8 + s;
            }
        }
        o += 8 + size;
    }
    return m;
}

MifFile MifFile::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw Sc2Error("cannot open " + path);
    return parse(std::vector<uint8_t>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()));
}

std::vector<uint8_t> MifFile::serialize() const {
    std::vector<uint8_t> tile;
    int count = 0;
    std::vector<uint8_t> body;
    auto emit = [&](const std::pair<char, int>& it) {
        std::vector<uint8_t> b;
        if (it.first == 'S') {
            const MifShape& s = shapes[static_cast<size_t>(it.second)];
            put16(b, static_cast<unsigned>(s.id)); put16(b, static_cast<unsigned>(s.width)); put16(b, static_cast<unsigned>(s.height));
            put32(b, static_cast<uint32_t>(s.stream.size()));
            b.insert(b.end(), s.stream.begin(), s.stream.end());
            putTag(body, "SHAP");
        } else {
            const auto n = names.find(it.second);
            if (n == names.end()) return;
            put16(b, static_cast<unsigned>(it.second)); put16(b, static_cast<unsigned>(n->second.size()));
            b.insert(b.end(), n->second.begin(), n->second.end());
            putTag(body, "NAME");
        }
        put32(body, static_cast<uint32_t>(b.size()));
        body.insert(body.end(), b.begin(), b.end());
        ++count;
    };
    std::vector<bool> shapeDone(shapes.size(), false);
    std::map<int, bool> nameDone;
    for (const auto& it : order) {
        if (it.first == 'S') { shapeDone[static_cast<size_t>(it.second)] = true; emit(it); }
        else if (!nameDone[it.second]) { nameDone[it.second] = true; emit(it); }
    }
    for (size_t i = 0; i < shapes.size(); ++i) if (!shapeDone[i]) emit({'S', static_cast<int>(i)});          // shapes added after loading
    for (const auto& [id, n] : names) if (!nameDone[id]) emit({'N', id});
    put16(tile, static_cast<unsigned>(count));
    tile.insert(tile.end(), body.begin(), body.end());
    std::vector<uint8_t> out;
    putTag(out, "MIFF"); put32(out, 0); putTag(out, "SC2K");
    putTag(out, "INFO"); put32(out, static_cast<uint32_t>(info.size())); out.insert(out.end(), info.begin(), info.end());
    putTag(out, "TILE"); put32(out, static_cast<uint32_t>(tile.size())); out.insert(out.end(), tile.begin(), tile.end());
    const uint32_t total = static_cast<uint32_t>(out.size() - 8);
    for (int i = 0; i < 4; ++i) out[4 + static_cast<size_t>(i)] = static_cast<uint8_t>(total >> (24 - 8 * i));
    return out;
}

void MifFile::save(const std::string& path) const {
    const auto bytes = serialize();
    std::ofstream f(path, std::ios::binary);
    if (!f) throw Sc2Error("cannot write " + path);
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

MifShape* MifFile::find(int id) { for (auto& s : shapes) if (s.id == id) return &s; return nullptr; }
const MifShape* MifFile::find(int id) const { for (const auto& s : shapes) if (s.id == id) return &s; return nullptr; }
void MifFile::put(const MifShape& s) {
    if (MifShape* e = find(s.id)) { *e = s; return; }
    shapes.push_back(s);
}

}  // namespace sc2k
