#include "resources/pe_resources.h"

#include <functional>

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
        if (!(type.offset & 0x80000000u)) continue;
        if (type.name & 0x80000000u) {   // custom resource type by name, e.g. "DATA": the newspaper's text tables (ids 1000..1005)
            const uint32_t at = root + (type.name & 0x7FFFFFFFu);
            std::string typeName;
            for (int j = 0, len = r.u16(at); j < len; ++j) typeName.push_back(static_cast<char>(r.u16(at + 2 + 2u * static_cast<uint32_t>(j))));
            for (const Entry& name : entries(r, root + (type.offset & 0x7FFFFFFFu))) {
                if ((name.name & 0x80000000u) || !(name.offset & 0x80000000u)) continue;
                const auto langs = entries(r, root + (name.offset & 0x7FFFFFFFu));
                if (langs.empty() || (langs[0].offset & 0x80000000u)) continue;
                const uint32_t leaf = root + langs[0].offset;
                const auto b = pe.bytesAtRva(r.u32(leaf), r.u32(leaf + 4));
                custom_[typeName][static_cast<int>(name.name)].assign(b.begin(), b.end());
            }
            continue;
        }
        const int typeId = static_cast<int>(type.name);
        if (typeId != 2 && typeId != 4 && typeId != 5 && typeId != 6) continue;
        for (const Entry& name : entries(r, root + (type.offset & 0x7FFFFFFFu))) {
            if (!(name.offset & 0x80000000u)) continue;
            std::string nameText;
            const bool named = (name.name & 0x80000000u) != 0;
            if (named) {
                const uint32_t at = root + (name.name & 0x7FFFFFFFu);
                const int len = r.u16(at);
                for (int j = 0; j < len; ++j) { const uint16_t ch = r.u16(at + 2 + 2u * static_cast<uint32_t>(j)); nameText.push_back(ch < 0x80 ? static_cast<char>(ch) : '?'); }
                if (typeId != 2 && typeId != 4) continue;
            }
            const auto langs = entries(r, root + (name.offset & 0x7FFFFFFFu));
            if (langs.empty() || (langs[0].offset & 0x80000000u)) continue;
            const uint32_t leaf = root + langs[0].offset;
            const uint32_t dataRva = r.u32(leaf), size = r.u32(leaf + 4);
            const int id = static_cast<int>(name.name);
            if (typeId == 4) {   // MENUTEMPLATE: header (4 bytes), then items: flags, [id], text; MF_POPUP 0x10 has no id and nests, MF_END 0x80 ends a level
                const auto raw = pe.bytesAtRva(dataRva, size);
                size_t o = 4;
                auto rd16 = [&](size_t at) { return at + 2 <= raw.size() ? static_cast<unsigned>(raw[at] | (raw[at + 1] << 8)) : 0u; };
                std::function<void(MenuItem&)> level = [&](MenuItem& parent) {
                    while (o + 2 <= raw.size()) {
                        MenuItem it;
                        it.flags = rd16(o); o += 2;
                        if (!(it.flags & 0x10)) { it.id = static_cast<int>(rd16(o)); o += 2; }
                        while (o + 2 <= raw.size()) { const unsigned ch = rd16(o); o += 2; if (!ch) break; it.text.push_back(ch < 0x80 ? static_cast<char>(ch) : '?'); }
                        if (it.flags & 0x10) level(it);
                        parent.items.push_back(it);
                        if (it.flags & 0x80) break;
                    }
                };
                MenuItem root;
                level(root);
                menus_[named ? nameText : "#" + std::to_string(id)] = root;
            } else if (typeId == 5) {   // DLGTEMPLATE (classic layout)
                const auto raw = pe.bytesAtRva(dataRva, size);
                const uint8_t* d = raw.data();
                size_t o = 0;
                auto u16 = [&](size_t at) { return at + 2 <= size ? static_cast<int>(d[at] | (d[at + 1] << 8)) : 0; };
                auto u32 = [&](size_t at) { return at + 4 <= size ? static_cast<uint32_t>(d[at] | (d[at + 1] << 8) | (d[at + 2] << 16) | (static_cast<uint32_t>(d[at + 3]) << 24)) : 0u; };
                auto wstr = [&](size_t& at) { std::string out; while (at + 2 <= size) { const int ch = u16(at); at += 2; if (!ch) break; out.push_back(ch < 0x80 ? static_cast<char>(ch) : '?'); } return out; };
                auto ordOrStr = [&](size_t& at, std::string& text) { if (u16(at) == 0xFFFF) { text = "#" + std::to_string(u16(at + 2)); at += 4; } else text = wstr(at); };
                const uint32_t style = u32(0);
                const int count = u16(8);
                o = 18;
                std::string menu, cls, title;
                ordOrStr(o, menu); ordOrStr(o, cls);
                Dialog dlg;
                dlg.title = wstr(o);
                if (style & 0x40) { o += 2; wstr(o); }   // DS_SETFONT: point size + face name
                for (int k = 0; k < count && o + 18 <= size; ++k) {
                    o = (o + 3) & ~static_cast<size_t>(3);
                    DialogControl c{};
                    c.x = u16(o + 8); c.y = u16(o + 10); c.w = u16(o + 12); c.h = u16(o + 14); c.id = u16(o + 16);
                    o += 18;
                    ordOrStr(o, c.cls);
                    ordOrStr(o, c.text);
                    o += 2 + static_cast<size_t>(u16(o));
                    dlg.controls.push_back(c);
                }
                dialogs_[id] = dlg;
            } else if (typeId == 2) { const auto b = pe.bytesAtRva(dataRva, size); if (named) namedBitmaps_[nameText].assign(b.begin(), b.end()); else bitmaps_[id].assign(b.begin(), b.end()); }
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
