#include "resources/winhelp.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <stdexcept>

namespace sc2k {

namespace {

using Bytes = std::vector<uint8_t>;

uint16_t u16(const Bytes& b, size_t o) { if (o + 2 > b.size()) throw std::runtime_error("help: truncated"); return static_cast<uint16_t>(b[o] | (b[o + 1] << 8)); }
uint32_t u32(const Bytes& b, size_t o) { if (o + 4 > b.size()) throw std::runtime_error("help: truncated"); return b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (static_cast<uint32_t>(b[o + 3]) << 24); }
int32_t s32(const Bytes& b, size_t o) { return static_cast<int32_t>(u32(b, o)); }

// the LZ77 variant of the help compiler: a flag byte, then per bit a literal byte or a 16-bit (offset 12 bits, length 4 bits) copy
Bytes lz77(const uint8_t* src, size_t n) {
    Bytes out;
    size_t i = 0;
    while (i < n) {
        unsigned mask = src[i++];
        for (int bit = 0; bit < 8 && i < n; ++bit, mask >>= 1) {
            if (mask & 1) {
                if (i + 2 > n) return out;
                const unsigned w = src[i] | (src[i + 1] << 8);
                i += 2;
                const size_t len = ((w >> 12) & 0xF) + 3, pos = (w & 0xFFF) + 1;
                if (pos > out.size()) throw std::runtime_error("help: bad LZ77 stream");
                for (size_t k = 0; k < len; ++k) out.push_back(out[out.size() - pos]);
            } else out.push_back(src[i++]);
        }
    }
    return out;
}

// compressed numbers (one or two bytes; the long form two or four)
unsigned scanWord(const Bytes& b, size_t& q) {
    if (q >= b.size()) throw std::runtime_error("help: truncated");
    if (b[q] & 1) { const unsigned v = u16(b, q) >> 1; q += 2; return v; }
    return b[q++] >> 1;
}
int scanInt(const Bytes& b, size_t& q) {
    if (q >= b.size()) throw std::runtime_error("help: truncated");
    if (b[q] & 1) { const int v = static_cast<int>(u16(b, q) >> 1) - 0x4000; q += 2; return v; }
    return static_cast<int>(b[q++] >> 1) - 0x40;
}
int scanLong(const Bytes& b, size_t& q) {
    if (q >= b.size()) throw std::runtime_error("help: truncated");
    if (b[q] & 1) { const int v = static_cast<int>(u32(b, q) >> 1) - 0x4000000; q += 4; return v; }
    const int v = static_cast<int>(u16(b, q) >> 1) - 0x4000;
    q += 2;
    return v;
}

struct Container {
    Bytes data;
    std::map<std::string, size_t> files;   // internal file name -> offset of its FILEHEADER

    Bytes file(const std::string& name) const {
        const auto it = files.find(name);
        if (it == files.end()) return {};
        const size_t o = it->second;
        const uint32_t used = u32(data, o + 4);
        if (o + 9 + used > data.size()) throw std::runtime_error("help: file out of range");
        return Bytes(data.begin() + static_cast<long>(o + 9), data.begin() + static_cast<long>(o + 9 + used));
    }
};

// Walks a B+ tree (a 38 byte header, then pages) and calls `leaf(page, offset)` for every leaf entry; `leaf` returns the offset after the entry.
// `indexKey` returns the size of an index entry's key at `offset`.
void walkBtree(const Bytes& raw, const std::function<size_t(const Bytes&, size_t)>& indexKeySize, const std::function<size_t(const Bytes&, size_t)>& leaf) {
    if (raw.size() < 38 || u16(raw, 0) != 0x293B) throw std::runtime_error("help: bad B-tree");
    const size_t pageSize = u16(raw, 4);
    const int root = static_cast<int16_t>(u16(raw, 26)), levels = u16(raw, 32);
    const uint32_t entries = u32(raw, 34);
    if (entries == 0 || root < 0) return;
    std::function<void(int, int)> walk = [&](int page, int level) {
        const size_t base = 38 + static_cast<size_t>(page) * pageSize;
        if (base + pageSize > raw.size()) throw std::runtime_error("help: B-tree page out of range");
        Bytes pg(raw.begin() + static_cast<long>(base), raw.begin() + static_cast<long>(base + pageSize));
        const unsigned n = u16(pg, 2);
        if (level == levels) {
            size_t o = 8;
            for (unsigned i = 0; i < n; ++i) o = leaf(pg, o);
        } else {
            walk(static_cast<int16_t>(u16(pg, 4)), level + 1);
            size_t o = 6;
            for (unsigned i = 0; i < n; ++i) {
                o += indexKeySize(pg, o);
                walk(u16(pg, o), level + 1);
                o += 2;
            }
        }
    };
    walk(root, 1);
}

}  // namespace

WinHelp WinHelp::load(const std::string& path) {
    Container c;
    {
        std::ifstream f(path, std::ios::binary);
        if (!f) throw std::runtime_error("cannot open " + path);
        c.data.assign(std::istreambuf_iterator<char>(f), {});
    }
    if (c.data.size() < 16 || u32(c.data, 0) != 0x00035F3F) throw std::runtime_error("not a help file: " + path);
    const size_t dirStart = u32(c.data, 4);
    {   // the directory: a B+ tree of (file name, offset)
        const uint32_t used = u32(c.data, dirStart + 4);
        if (dirStart + 9 + used > c.data.size()) throw std::runtime_error("help: bad directory");
        const Bytes raw(c.data.begin() + static_cast<long>(dirStart + 9), c.data.begin() + static_cast<long>(dirStart + 9 + used));
        const auto strSize = [](const Bytes& b, size_t o) { size_t e = o; while (e < b.size() && b[e]) ++e; return e - o + 1; };
        walkBtree(raw, strSize, [&](const Bytes& pg, size_t o) {
            const size_t len = strSize(pg, o);
            const std::string name(reinterpret_cast<const char*>(&pg[o]), len - 1);
            c.files[name] = u32(pg, o + len);
            return o + len + 4;
        });
    }
    const Bytes sys = c.file("|SYSTEM");
    if (sys.size() < 12) throw std::runtime_error("help: no |SYSTEM");
    const unsigned minor = u16(sys, 2), flags = u16(sys, 10);
    if (minor < 16) throw std::runtime_error("help: Windows 3.0 help files are not supported");
    const bool compressed = flags == 4 || flags == 8;
    constexpr size_t kBlock = 4096;

    // phrases
    std::vector<std::string> phrases;
    if (const Bytes p = c.file("|Phrases"); !p.empty()) {
        const unsigned n = u16(p, 0), size = u32(p, 4);
        const size_t table = 8 + 2 * (static_cast<size_t>(n) + 1);
        if (table > p.size()) throw std::runtime_error("help: bad phrases");
        Bytes data(p.begin() + static_cast<long>(table), p.end());
        if (p.size() - 8 < size) data = lz77(data.data(), data.size());
        const unsigned first = u16(p, 8);
        for (unsigned i = 0; i < n; ++i) {
            const size_t a = u16(p, 8 + 2 * i) - first, e = u16(p, 8 + 2 * (i + 1)) - first;
            phrases.emplace_back(reinterpret_cast<const char*>(data.data()) + std::min(a, data.size()), std::min(e, data.size()) - std::min(a, data.size()));
        }
    }
    auto expand = [&](const Bytes& raw) {
        Bytes out;
        for (size_t i = 0; i < raw.size(); ++i) {
            const uint8_t ch = raw[i];
            if (ch > 0 && ch < 16 && i + 1 < raw.size()) {
                const unsigned idx = 256u * (ch - 1) + raw[++i];
                if (idx / 2 < phrases.size()) out.insert(out.end(), phrases[idx / 2].begin(), phrases[idx / 2].end());
                if (idx & 1) out.push_back(' ');
            } else out.push_back(ch);
        }
        return out;
    };

    // the topic stream: blocks of 4 KB with a 12 byte header; records never start in the last bytes of a block (zero padding)
    const Bytes topic = c.file("|TOPIC");
    Bytes stream;
    std::vector<size_t> starts;
    for (size_t o = 0; o + 12 < topic.size(); o += kBlock) {
        const size_t end = std::min(topic.size(), o + kBlock);
        starts.push_back(stream.size());
        const Bytes block = compressed ? lz77(&topic[o + 12], end - o - 12) : Bytes(topic.begin() + static_cast<long>(o + 12), topic.begin() + static_cast<long>(end));
        stream.insert(stream.end(), block.begin(), block.end());
    }
    struct Pending { size_t topic, line, run; uint32_t hash; };
    std::vector<Pending> links;
    std::vector<std::vector<std::vector<uint32_t>>> hashOf;   // [topic][line][run] hash (0 = none)

    WinHelp h;
    size_t p = 0;
    while (p + 21 <= stream.size()) {
        const int32_t blockSize = s32(stream, p), dataLen2 = s32(stream, p + 4), dataLen1 = s32(stream, p + 16);
        const uint8_t type = stream[p + 20];
        if (blockSize <= 0) {   // padding at the end of a block: the next record starts with the next block
            const auto it = std::upper_bound(starts.begin(), starts.end(), p);
            if (it == starts.end()) break;
            p = *it;
            continue;
        }
        if (dataLen1 < 21 || dataLen1 > blockSize || p + static_cast<size_t>(blockSize) > stream.size()) break;
        const Bytes d1(stream.begin() + static_cast<long>(p + 21), stream.begin() + static_cast<long>(p + static_cast<size_t>(dataLen1)));
        Bytes d2(stream.begin() + static_cast<long>(p + static_cast<size_t>(dataLen1)), stream.begin() + static_cast<long>(p + static_cast<size_t>(blockSize)));
        if (static_cast<int32_t>(d2.size()) < dataLen2 && !phrases.empty()) d2 = expand(d2);
        if (type == 2) {
            Topic t;
            t.title.assign(reinterpret_cast<const char*>(d2.data()), strnlen(reinterpret_cast<const char*>(d2.data()), d2.size()));
            h.topics_.push_back(std::move(t));
            hashOf.emplace_back();
        } else if ((type == 0x20) && !h.topics_.empty()) {
            Topic& t = h.topics_.back();
            auto& hashes = hashOf.back();
            std::vector<std::string> segs;
            for (size_t i = 0, s = 0; i <= d2.size(); ++i)
                if (i == d2.size() || d2[i] == 0) { segs.emplace_back(reinterpret_cast<const char*>(d2.data()) + s, i - s); s = i + 1; }
            try {
                size_t q = 0;
                scanLong(d1, q);
                scanWord(d1, q);
                q += 4;
                const unsigned fl = u16(d1, q);
                q += 2;
                if (fl & 1) scanLong(d1, q);
                for (unsigned bit : {2u, 4u, 8u, 0x10u, 0x20u, 0x40u}) if (fl & bit) scanInt(d1, q);
                if (fl & 0x100) q += 3;
                if (fl & 0x200) {
                    const int n = scanInt(d1, q);
                    for (int i = 0; i < n; ++i) if (scanWord(d1, q) & 0x4000) scanWord(d1, q);
                }
                Line line;
                std::vector<uint32_t> lineHashes;
                uint32_t hotspot = 0;
                bool popup = false;
                size_t si = 0;
                auto addText = [&](const std::string& s) {
                    if (s.empty()) return;
                    if (!line.runs.empty() && line.runs.back().popup == popup && lineHashes.back() == hotspot) line.runs.back().text += s;
                    else { Run r; r.text = s; r.popup = popup && hotspot != 0; line.runs.push_back(r); lineHashes.push_back(hotspot); }
                };
                auto endLine = [&](bool paragraph) {
                    line.paragraphEnd = paragraph;
                    t.lines.push_back(std::move(line));
                    hashes.push_back(std::move(lineHashes));
                    line = Line{}; lineHashes.clear();
                };
                while (q < d1.size()) {
                    addText(si < segs.size() ? segs[si] : std::string());
                    ++si;
                    const uint8_t cmd = d1[q];
                    if (cmd == 0xFF) break;
                    switch (cmd) {
                        case 0x20: q += 5; break;
                        case 0x21: case 0x80: q += 3; break;
                        case 0x81: endLine(false); q += 1; break;
                        case 0x82: endLine(true); q += 1; break;
                        case 0x83: addText("    "); q += 1; break;
                        case 0x89: hotspot = 0; popup = false; q += 1; break;
                        case 0x8B: addText(" "); q += 1; break;
                        case 0x8C: addText("-"); q += 1; break;
                        case 0x86: case 0x87: case 0x88: { q += 2; const int n = scanLong(d1, q); q += static_cast<size_t>(std::max(0, n)); break; }
                        case 0xC8: case 0xCC: case 0xEA: case 0xEB: case 0xEE: case 0xEF: q += static_cast<size_t>(static_cast<int16_t>(u16(d1, q + 1)) + 3); break;
                        case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE6: case 0xE7:
                            hotspot = u32(d1, q + 1); popup = (cmd == 0xE0 || cmd == 0xE2 || cmd == 0xE6);
                            q += 5;
                            break;
                        default: q += 1; break;
                    }
                }
                if (!line.runs.empty()) endLine(false);
            } catch (const std::exception&) {   // an unreadable record: its plain text still goes in
                Line line;
                for (const auto& s : segs) if (!s.empty()) { Run r; r.text = s; line.runs.push_back(r); }
                if (!line.runs.empty()) { t.lines.push_back(line); hashes.push_back(std::vector<uint32_t>(line.runs.size(), 0)); }
            }
        }
        p += static_cast<size_t>(blockSize);
    }

    // hyperlinks: |CONTEXT maps a hash to a topic offset; the topic offsets of |TTLBTREE, in ascending order, are the topics in stream order
    std::vector<uint32_t> topicOffsets;
    if (const Bytes raw = c.file("|TTLBTREE"); !raw.empty()) {
        walkBtree(raw, [](const Bytes&, size_t) { return size_t{4}; }, [&](const Bytes& pg, size_t o) {
            topicOffsets.push_back(u32(pg, o));
            size_t e = o + 4;
            while (e < pg.size() && pg[e]) ++e;
            return e + 1;
        });
        std::sort(topicOffsets.begin(), topicOffsets.end());
    }
    std::map<uint32_t, uint32_t> context;
    if (const Bytes raw = c.file("|CONTEXT"); !raw.empty())
        walkBtree(raw, [](const Bytes&, size_t) { return size_t{4}; }, [&](const Bytes& pg, size_t o) { context[u32(pg, o)] = u32(pg, o + 4); return o + 8; });
    for (size_t t = 0; t < h.topics_.size(); ++t)
        for (size_t l = 0; l < h.topics_[t].lines.size(); ++l)
            for (size_t r = 0; r < h.topics_[t].lines[l].runs.size(); ++r) {
                const uint32_t hash = hashOf[t][l][r];
                if (hash == 0) continue;
                const auto it = context.find(hash);
                if (it == context.end()) continue;
                const auto pos = std::upper_bound(topicOffsets.begin(), topicOffsets.end(), it->second);
                if (pos == topicOffsets.begin()) continue;
                const size_t idx = static_cast<size_t>(pos - topicOffsets.begin()) - 1;
                if (idx < h.topics_.size()) h.topics_[t].lines[l].runs[r].link = static_cast<int>(idx);
            }
    if (h.topics_.empty()) throw std::runtime_error("help file without topics: " + path);
    return h;
}

std::string WinHelp::plainText(size_t topic) const {
    std::string out;
    if (topic >= topics_.size()) return out;
    out = topics_[topic].title + "\n";
    for (const Line& l : topics_[topic].lines) {
        for (const Run& r : l.runs) out += r.text;
        out += '\n';
    }
    return out;
}

}  // namespace sc2k
