#include "save/city.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>

#include "save/error.h"
#include "save/rle.h"

namespace sc2k {
namespace {

uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
void putBe32(std::vector<uint8_t>& o, uint32_t v) {
    o.push_back(uint8_t(v >> 24)); o.push_back(uint8_t(v >> 16)); o.push_back(uint8_t(v >> 8)); o.push_back(uint8_t(v));
}

// Canonical chunk order of every supplied city (CNAM position varies and is emitted last by serialize()).
constexpr const char* kLayerOrder[] = {"MISC", "ALTM", "XTER", "XBLD", "XZON", "XUND", "XTXT", "XLAB", "XMIC", "XTHG",
                                       "XBIT", "XTRF", "XPLT", "XVAL", "XCRM", "XPLC", "XFIR", "XPOP", "XROG", "XGRP"};

bool isRaw(const std::string& t) { return t == "ALTM" || t == "CNAM"; }
bool isScenarioTag(const std::string& t) { return t == "TEXT" || t == "SCEN" || t == "PICT" || t == "TMPL"; }

template <typename Arr>
void fillBytes(Arr& dst, const std::vector<uint8_t>& src, const char* tag) {
    if (src.size() != dst.size()) throw Sc2Error(std::string(tag) + ": decoded size " + std::to_string(src.size()) + ", expected " + std::to_string(dst.size()));
    std::memcpy(dst.data(), src.data(), dst.size());
}
template <typename G>
void fillGrid(G& g, const std::vector<uint8_t>& src, const char* tag) {
    if (src.size() != g.v.size()) throw Sc2Error(std::string(tag) + ": decoded size " + std::to_string(src.size()) + ", expected " + std::to_string(g.v.size()));
    std::memcpy(g.v.data(), src.data(), g.v.size());
}

void appendChunk(std::vector<uint8_t>& out, const std::string& tag, std::span<const uint8_t> body) {
    out.insert(out.end(), tag.begin(), tag.end());
    putBe32(out, static_cast<uint32_t>(body.size()));
    out.insert(out.end(), body.begin(), body.end());
}

std::vector<uint8_t> wrapForm(const std::vector<uint8_t>& chunks) {
    std::vector<uint8_t> out{'F', 'O', 'R', 'M'};
    putBe32(out, static_cast<uint32_t>(chunks.size() + 4));  // FORM length = file size - 8 = 4 ("SCDH") + chunks
    out.insert(out.end(), {'S', 'C', 'D', 'H'});
    out.insert(out.end(), chunks.begin(), chunks.end());
    return out;
}

}  // namespace

CityFile CityFile::parse(std::span<const uint8_t> f) {
    if (f.size() < 12 || std::memcmp(f.data(), "FORM", 4) != 0) throw Sc2Error("not a FORM file");
    if (std::memcmp(f.data() + 8, "SCDH", 4) != 0) throw Sc2Error("FORM type is not SCDH");
    if (uint64_t(be32(f.data() + 4)) + 8 != f.size()) throw Sc2Error("FORM length does not match file size");

    CityFile c;
    size_t o = 12;
    std::map<std::string, std::vector<uint8_t>> decoded;  // tag -> decoded bytes (first occurrence)
    size_t index = 0;
    while (o < f.size()) {
        if (o + 8 > f.size()) throw Sc2Error("truncated chunk header");
        RawChunk ch;
        ch.tag.assign(reinterpret_cast<const char*>(f.data() + o), 4);
        const uint32_t n = be32(f.data() + o + 4);
        o += 8;
        if (n > f.size() - o) throw Sc2Error("chunk " + ch.tag + " extends past end of file");
        ch.stored.assign(f.begin() + o, f.begin() + o + n);
        o += n;

        const bool known = ch.tag == "CNAM" || std::find_if(std::begin(kLayerOrder), std::end(kLayerOrder), [&](const char* t) { return ch.tag == t; }) != std::end(kLayerOrder);
        if (known && !decoded.count(ch.tag)) {
            decoded[ch.tag] = isRaw(ch.tag) ? ch.stored : rleDecode(ch.stored);
        } else {
            if (isScenarioTag(ch.tag)) c.hasScenarioChunks = true;
            c.extras.push_back({index, ch});
        }
        c.original_.push_back(std::move(ch));
        ++index;
    }

    auto need = [&](const char* t) -> const std::vector<uint8_t>& {
        auto it = decoded.find(t);
        if (it == decoded.end()) throw Sc2Error(std::string("missing required chunk ") + t);
        return it->second;
    };
    if (auto it = decoded.find("CNAM"); it != decoded.end()) {
        if (it->second.size() != 32) throw Sc2Error("CNAM: expected 32 bytes");
        std::array<uint8_t, 32> a;
        std::memcpy(a.data(), it->second.data(), 32);
        c.cnam = a;
    }
    fillBytes(c.misc, need("MISC"), "MISC");
    {
        const auto& a = need("ALTM");
        if (a.size() != c.altm.v.size() * 2) throw Sc2Error("ALTM: decoded size " + std::to_string(a.size()) + ", expected 32768");
        for (size_t i = 0; i < c.altm.v.size(); ++i) c.altm.v[i] = uint16_t((a[2 * i] << 8) | a[2 * i + 1]);
    }
    fillGrid(c.xter, need("XTER"), "XTER"); fillGrid(c.xbld, need("XBLD"), "XBLD"); fillGrid(c.xzon, need("XZON"), "XZON");
    fillGrid(c.xund, need("XUND"), "XUND"); fillGrid(c.xtxt, need("XTXT"), "XTXT"); fillGrid(c.xbit, need("XBIT"), "XBIT");
    fillBytes(c.xlab, need("XLAB"), "XLAB"); fillBytes(c.xmic, need("XMIC"), "XMIC"); fillBytes(c.xthg, need("XTHG"), "XTHG");
    fillGrid(c.xtrf, need("XTRF"), "XTRF"); fillGrid(c.xplt, need("XPLT"), "XPLT"); fillGrid(c.xval, need("XVAL"), "XVAL"); fillGrid(c.xcrm, need("XCRM"), "XCRM");
    fillGrid(c.xplc, need("XPLC"), "XPLC"); fillGrid(c.xfir, need("XFIR"), "XFIR"); fillGrid(c.xpop, need("XPOP"), "XPOP"); fillGrid(c.xrog, need("XROG"), "XROG");
    {
        const auto& g = need("XGRP");
        if (g.size() != c.xgrp.size() * 4) throw Sc2Error("XGRP: decoded size " + std::to_string(g.size()) + ", expected 3328");
        for (size_t i = 0; i < c.xgrp.size(); ++i) c.xgrp[i] = static_cast<int32_t>(be32(g.data() + 4 * i));
    }
    return c;
}

CityFile CityFile::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw Sc2Error("cannot open " + path);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return parse(data);
}

std::vector<uint8_t> CityFile::serializeOriginal() const {
    std::vector<uint8_t> body;
    for (const auto& ch : original_) appendChunk(body, ch.tag, ch.stored);
    return wrapForm(body);
}

std::vector<uint8_t> CityFile::serialize() const {
    std::vector<uint8_t> body;
    auto layer = [&](const char* tag, std::span<const uint8_t> raw) { appendChunk(body, tag, rleEncode(raw)); };
    auto bytes = [](const auto& g) { return std::span<const uint8_t>(g.data(), g.size()); };
    if (cnam) appendChunk(body, "CNAM", *cnam);  // all supplied files with CNAM first or last; first is the game's own choice for new cities
    layer("MISC", misc);
    {
        std::vector<uint8_t> a(altm.v.size() * 2);
        for (size_t i = 0; i < altm.v.size(); ++i) { a[2 * i] = uint8_t(altm.v[i] >> 8); a[2 * i + 1] = uint8_t(altm.v[i]); }
        appendChunk(body, "ALTM", a);
    }
    layer("XTER", bytes(xter.v)); layer("XBLD", bytes(xbld.v)); layer("XZON", bytes(xzon.v)); layer("XUND", bytes(xund.v));
    layer("XTXT", bytes(xtxt.v)); layer("XLAB", xlab); layer("XMIC", xmic); layer("XTHG", xthg); layer("XBIT", bytes(xbit.v));
    layer("XTRF", bytes(xtrf.v)); layer("XPLT", bytes(xplt.v)); layer("XVAL", bytes(xval.v)); layer("XCRM", bytes(xcrm.v));
    layer("XPLC", bytes(xplc.v)); layer("XFIR", bytes(xfir.v)); layer("XPOP", bytes(xpop.v)); layer("XROG", bytes(xrog.v));
    {
        std::vector<uint8_t> g;
        for (int32_t v : xgrp) putBe32(g, static_cast<uint32_t>(v));
        layer("XGRP", g);
    }
    for (const auto& e : extras) appendChunk(body, e.chunk.tag, e.chunk.stored);
    return wrapForm(body);
}

uint32_t CityFile::miscWord(size_t i) const { return be32(misc.data() + 4 * i); }

void CityFile::setMiscWord(size_t i, uint32_t v) {
    misc[4 * i] = uint8_t(v >> 24); misc[4 * i + 1] = uint8_t(v >> 16); misc[4 * i + 2] = uint8_t(v >> 8); misc[4 * i + 3] = uint8_t(v);
}

std::string CityFile::name() const {
    if (!cnam) return {};
    const size_t n = std::min<size_t>((*cnam)[0], 31);
    return std::string(reinterpret_cast<const char*>(cnam->data() + 1), n);
}

}  // namespace sc2k
