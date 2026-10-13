#include "simulation/news.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iterator>

namespace sc2k {

namespace {
std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return in ? std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {}) : std::vector<uint8_t>{};
}
uint32_t be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>(p[0] << 8 | p[1]); }
}  // namespace

NewsData NewsData::load(const std::string& dir, const std::array<int16_t, 256>& digraphTable) {
    NewsData n;
    n.digraph = digraphTable;
    const auto idx = slurp(dir + "/DATA/DATA_USA.IDX"), dat = slurp(dir + "/DATA/DATA_USA.DAT");
    if (idx.size() < 48 || dat.empty()) return n;
    // six (id, offset) pairs, each section runs to the next offset
    std::array<std::pair<uint32_t, uint32_t>, 6> sec{};
    auto le32 = [&](size_t o) { return uint32_t(idx[o]) | uint32_t(idx[o + 1]) << 8 | uint32_t(idx[o + 2]) << 16 | uint32_t(idx[o + 3]) << 24; };   // the index is little-endian, the data big-endian
    for (size_t i = 0; i < 6; ++i) sec[i] = {le32(i * 8), le32(i * 8 + 4)};
    auto part = [&](uint32_t id) -> std::vector<uint8_t> {
        for (size_t i = 0; i < 6; ++i)
            if (sec[i].first == id) {
                const uint32_t from = sec[i].second, to = i + 1 < 6 ? sec[i + 1].second : static_cast<uint32_t>(dat.size());
                if (from <= to && to <= dat.size()) return std::vector<uint8_t>(dat.begin() + from, dat.begin() + to);
            }
        return {};
    };
    const auto starts = part(1000), counts = part(1001), offs = part(1002), text = part(1003), pri = part(1004), dec = part(1005);
    if (starts.size() < 500 || counts.size() < 500 || offs.empty() || text.empty() || pri.size() < 160 || dec.size() < 160) return n;
    for (size_t i = 0; i < 250; ++i) { n.start[i] = be16(&starts[2 * i]); n.count[i] = be16(&counts[2 * i]); }
    for (size_t i = 0; i + 4 <= offs.size(); i += 4) n.offset.push_back(be32(&offs[i]));
    n.text = text;
    for (size_t i = 0; i < 80; ++i) { n.priority[i] = static_cast<int16_t>(be16(&pri[2 * i])); n.decay[i] = static_cast<int16_t>(be16(&dec[2 * i])); }
    n.loaded = true;
    return n;
}

void NewsRoom::reset(CrtRand& rng) {
    for (int i = 0; i < 6; ++i) {
        uint8_t* p = &pages_[static_cast<size_t>(i) * 5];
        p[0] = static_cast<uint8_t>(i); p[1] = p[2] = static_cast<uint8_t>(i % 3); p[3] = p[4] = static_cast<uint8_t>(i);
    }
    auto swap = [&](size_t a, size_t b) { std::swap(pages_[a], pages_[b]); };   // 0x479F20(a, b): *a <-> *b
    swap(3, static_cast<size_t>(rng.next() & 1) * 5 + 0x12);
    if (rng.next() & 1) swap(1, 6);
    for (int k = 12; k > 0; --k) {
        auto pick = [&](size_t base, int n) { return static_cast<size_t>(rng.next() & 0x7FFF) % static_cast<size_t>(n) * 5 + base; };
        { const size_t y = pick(0, 6); const size_t x = pick(0, 6); swap(x, y); }
        { const size_t y = pick(6, 5); const size_t x = pick(6, 5); swap(x, y); }
        { const size_t y = pick(2, 6); const size_t x = pick(2, 6); swap(x, y); }
        { const size_t y = pick(8, 5); const size_t x = pick(8, 5); swap(x, y); }
        { const size_t y = pick(4, 6); const size_t x = pick(4, 6); swap(x, y); }
    }
    for (int i = 0; i < 9; ++i) entries_[static_cast<size_t>(i)] = NewsEntry{i + 0xB, 0, 0, 0xFF, 0xFF, 0xFF};
}

void NewsRoom::post(int id, int arg) {
    if (!data_ || id < 0 || id >= 80) return;
    const int prio = data_->priority[static_cast<size_t>(id)];
    int k = 5;
    while (true) {
        if (prio < entries_[static_cast<size_t>(k)].priority) break;
        entries_[static_cast<size_t>(k) + 1] = entries_[static_cast<size_t>(k)];
        if (--k < 0) break;
    }
    entries_[static_cast<size_t>(k + 1)] = NewsEntry{id, prio, static_cast<uint8_t>(arg), 0xFF, 0xFF, 0xFF};
}

void NewsRoom::age() {
    if (!data_) return;
    for (size_t i = 0; i < 7; ++i) {
        NewsEntry& e = entries_[i];
        const int d = e.id >= 0 && e.id < 80 ? data_->decay[static_cast<size_t>(e.id)] : 0;
        e.priority = d < e.priority ? e.priority - d : 0;
    }
    for (size_t i = 0; i < 6; ++i)
        for (size_t j = i + 1; j < 7; ++j)
            if (entries_[i].priority < entries_[j].priority) std::swap(entries_[i], entries_[j]);
}

void NewsRoom::expandInto(std::string& out, int rule, int entry, int mode, CrtRand& rng, const Context& ctx) {
    if (!data_ || !data_->loaded || rule < 0 || static_cast<size_t>(rule) >= data_->offset.size()) return;
    NewsEntry& e = entries_[static_cast<size_t>(entry)];
    const uint8_t* p = &data_->text[data_->offset[static_cast<size_t>(rule)]];
    const uint8_t* end = data_->text.data() + data_->text.size();
    auto rmod = [&](int n) { return static_cast<int>(static_cast<uint32_t>(rng.next() & 0x7FFF) % static_cast<uint32_t>(n)); };
    auto number = [&](int v) { out += std::to_string(v); };
    auto alt = [&](int group, int index) { return static_cast<int>(data_->start[static_cast<size_t>(group)]) + index; };
    auto group = [&](int c) { return std::min(c, 249); };
    size_t i = 0;
    if (mode == 1) {   // skip the headline
        while (p + i < end && p[i] != 0 && p[i] != '+') ++i;
        if (p[i] == 0) return;
        ++i;
        mode = 2;
    }
    for (; p + i < end && p[i] != 0; ++i) {
        const uint8_t ch = p[i];
        if (mode == 0 && ch == '+') return;
        switch (ch) {
            case '$': number(e.arg); break;
            case '%': { const int n = p[++i]; number(n > 1 ? rmod(n - 1) + 2 : 0); break; }
            case '&': { const int g = group(p[++i]); if (data_->count[static_cast<size_t>(g)] <= e.arg) e.arg = 0; expandInto(out, alt(g, e.arg), entry, mode, rng, ctx); break; }
            case '*': { const int g = group(p[++i]); const int n = std::max<int>(1, data_->count[static_cast<size_t>(g)]); expandInto(out, alt(g, rmod(n)), entry, mode, rng, ctx); break; }
            case '-': out += "\n     "; break;
            case '<': { const int n = p[++i]; const int v = n > 1 ? rmod(n - 1) + 2 : 0; if (e.c7 == 0xFF) e.c7 = static_cast<uint8_t>(v); number(e.c7); break; }
            case '=': out += ctx.cityName; break;
            case '>': out += ctx.label ? ctx.label(e.arg + 0xFB) : std::string(); break;
            case '@': { const int g = group(p[++i]); const int n = std::max<int>(1, data_->count[static_cast<size_t>(g)]); if (n <= e.c6) e.c6 = static_cast<uint8_t>(rmod(n)); expandInto(out, alt(g, e.c6), entry, mode, rng, ctx); break; }
            case '[': {
                const int g = group(p[++i]);
                const int n = std::max<int>(1, data_->count[static_cast<size_t>(g)]);
                if (cache_[static_cast<size_t>(g)] == 0xFF) cache_[static_cast<size_t>(g)] = static_cast<uint8_t>(rmod(n));
                expandInto(out, alt(g, cache_[static_cast<size_t>(g)]), entry, mode, rng, ctx);
                break;
            }
            case '\\': out += static_cast<char>(p[++i]); break;
            case '^': { const int g = group(p[++i]); const int n = std::max<int>(1, data_->count[static_cast<size_t>(g)]); if (n <= e.c5) e.c5 = static_cast<uint8_t>(rmod(n)); expandInto(out, alt(g, e.c5), entry, mode, rng, ctx); break; }
            case '~': out += ctx.label ? ctx.label(0) : std::string(); break;
            default:
                if (data_->digraph[ch] != 0) expandInto(out, data_->digraph[ch], entry, mode, rng, ctx);
                else out += static_cast<char>(ch);
                break;
        }
    }
}

std::string NewsRoom::expand(int rule, int entry, int mode, CrtRand& rng, const Context& ctx) {
    std::string out;
    expandInto(out, rule, entry, mode, rng, ctx);
    return out;
}

std::string NewsRoom::headline(int entry, CrtRand& rng, const Context& ctx) {
    if (!data_ || !data_->loaded) return {};
    cache_.fill(0xFF);
    const NewsEntry& e = entries_[static_cast<size_t>(entry)];
    const int g = std::min(e.id, 249);
    const int n = std::max<int>(1, data_->count[static_cast<size_t>(g)]);
    std::string s = expand(data_->start[static_cast<size_t>(g)] + static_cast<int>(static_cast<uint32_t>(rng.next() & 0x7FFF) % static_cast<uint32_t>(n)), entry, 0, rng, ctx);
    for (char& c : s) if (static_cast<uint8_t>(c) == 0xD5) c = '\'';
    bool afterLetter = false;   // capitalise the first letter of every word (not after "L'")
    for (size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == '\'') { if (i > 0 && s[i - 1] == 'L') afterLetter = false; continue; }
        if (!afterLetter && std::islower(c)) { s[i] = static_cast<char>(std::toupper(c)); afterLetter = true; }
        else afterLetter = std::isalnum(c) != 0;
    }
    return s;
}

std::string NewsRoom::article(int entry, CrtRand& rng, const Context& ctx) {
    if (!data_ || !data_->loaded) return {};
    const NewsEntry& e = entries_[static_cast<size_t>(entry)];
    const int g = std::min(e.id, 249);
    const int n = std::max<int>(1, data_->count[static_cast<size_t>(g)]);
    std::string s = expand(data_->start[static_cast<size_t>(g)] + static_cast<int>(static_cast<uint32_t>(rng.next() & 0x7FFF) % static_cast<uint32_t>(n)), entry, 1, rng, ctx);
    for (char& c : s) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u == 0xD5 || u == 0xD4) c = '\'';
        if (u == 0xD2 || u == 0xD3) c = '"';
    }
    int state = 0;   // sentence-case: a lower-case letter after two sentence delimiters gets a capital
    for (char& c : s) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (state < 2 || u < 'a' || u > 'z') {
            if (c == ' ' || c == '.' || c == '!' || c == '?' || u == 0xD2 || c == '"') ++state;
            else state = -(c == ',');
        } else {
            state = 0;
            c = static_cast<char>(u - 0x20);
        }
    }
    return s;
}

}  // namespace sc2k
