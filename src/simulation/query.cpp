#include "simulation/query.h"

#include <cstring>

namespace sc2k {
namespace {
using namespace xbit;

int groupOf(int id) {   // table @0x4E5F70: first entry greater than the id
    static const int limits[22] = {0x1, 0x5, 0x6, 0xD, 0xE, 0x1D, 0x2C, 0x3F, 0x43, 0x47, 0x49, 0x51, 0x56, 0x5C, 0x5D, 0x61, 0x6A, 0x6C, 0x70, 0x74, 0x78, 0x7C};
    for (int k = 0; k < 22; ++k) if (id < limits[k]) return k;
    return id - 0x66;
}
bool transportTile(int id) {
    return (id > 0x1C && id < 0x2C) || (id > 0x3E && id < 0x47) || id == 0x4B || id == 0x4C || (id > 0x5C && id < 0x61) || (id > 0x60 && id < 0x6C) || (id > 0x48 && id < 0x51) ||
           (id > 0x50 && id < 0x5C) || id == 0x6A || id == 0x6B;
}
QueryPiece num(long long v, bool warn = false) { QueryPiece p; p.text = std::to_string(v); p.warn = warn; return p; }
QueryPiece str(int id, bool warn = false) { QueryPiece p; p.stringId = id; p.warn = warn; return p; }
int level(uint8_t v) { static const int thr[5] = {0, 1, 0x3C, 0x78, 0xB4}; int k = 1; while (k < 5 && v >= thr[k]) ++k; return 776 + k; }   // None .. Very High = strings 777..781
}  // namespace

int tileNameString(const SimContext& c, int a, int b) {
    const World& w = c.w;
    const int id = w.xbld.at(a, b);
    if (id == 0) {
        if (w.xbit.at(a, b) & kWater) {
            if (!(w.xbit.at(a, b) & kSaltWater)) return 752;    // fresh water
            const int t = w.xtxt.at(a, b);
            if (t > 200 && t < 0xF1 && w.xthg[static_cast<size_t>(t - 0xC9) * 12] == 9) return 754;   // a sailboat
            return 753;                                          // salt water
        }
        return 751;   // clear terrain
    }
    return 593 + groupOf(id);
}

QueryInfo queryTile(const SimContext& c, int a, int b) {
    const World& w = c.w;
    QueryInfo q;
    q.a = a; q.b = b;
    q.nameString = tileNameString(c, a, b);
    q.overlay = w.xtxt.at(a, b);
    const int zone = w.xzon.at(a, b) & 0xF, id = w.xbld.at(a, b);
    static const int zoneName[16] = {0, 769, 769, 770, 770, 771, 771, 772, 773, 774, 773, 0, 0, 0, 0, 0}, density[16] = {0, 775, 776, 775, 776, 775, 776, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    bool underwater = false;
    auto xtrf = [&](int ra, int cb) { return (ra >= 0 && ra < 64 && cb >= 0 && cb < 64) ? static_cast<int>(w.xtrf.at(ra, cb)) : 0; };
    if (zone == 0) {
        if (transportTile(id)) {
            int t = 0;
            if (a > 0) t = xtrf((a - 1) >> 1, b >> 1);
            if (b > 0) t += xtrf(a >> 1, (b - 1) >> 1);
            if (a < 0x80) t += xtrf((a + 1) >> 1, b >> 1);
            if (b < 0x80) t += xtrf(a >> 1, (b + 1) >> 1);
            if ((id > 0x60 && id < 0x6C) || (id > 0x48 && id < 0x51)) t *= 2;
            q.lines.push_back({813, {num(static_cast<uint16_t>(static_cast<int16_t>(t) >> 2) / 2), str(814)}});
        }
    } else {
        QueryLine l{812, {str(zoneName[zone])}};
        q.lines.push_back(l);
        if (density[zone]) q.lines.push_back({-1, {str(density[zone])}});
    }
    const int alt = w.altm.at(a, b) & 0x1F, sea = c.s.seaLevel;
    if (alt < sea) {
        q.lines.push_back({827, {num((sea - alt) * 100 - 50), str(815)}});
        underwater = true;
    } else {
        int feet;
        const int ter = w.xter.at(a, b);
        if (ter == 0 || ter > 0xF) feet = (alt - sea) * 100 + 50;
        else feet = ((alt - sea) * 4 + 4) * 25;
        q.lines.push_back({827, {num(feet), str(824)}});
    }
    if (!underwater) q.lines.push_back({816, {num(w.xval.at(a >> 1, b >> 1) + 1), str(825)}});
    q.lines.push_back({817, {str(level(w.xcrm.at(a >> 1, b >> 1)))}});
    q.lines.push_back({818, {str(level(w.xplt.at(a >> 1, b >> 1)))}});
    if (id > 0xC && zone != 7 && !underwater) {
        const bool powered = (w.xbit.at(a, b) & kPowered) != 0;
        q.lines.push_back({826, {str(powered ? 819 : 820, !powered)}});
        if (id == 0xDC) {
            int g = static_cast<int16_t>((c.s.cloudiness >> 1) + sea * 5);
            if (powered)
                for (int x = a - 1; x <= a + 1; ++x)
                    for (int y = b - 1; y <= b + 1; ++y)
                        if (inMap(x, y) && (w.xbit.at(x, y) & 5) == 4) g += 10;
            q.lines.push_back({821, {num(static_cast<int16_t>(powered ? g : 0) * 0x2D0), str(822)}});
        } else if (id == 0xEB) {
            int x = a, y = b;
            footprintOf(c, x, y, 0xEB);
            int n = 0;
            auto fed = [&](int p, int r) { return inMap(p, r) && (w.xbit.at(p, r) & kWatered); };
            n += fed(x, y); n += fed(x + 1, y); n += fed(x, y - 1); n += fed(x + 1, y - 1);
            q.lines.push_back({821, {num(n * 10000), str(823)}});
        } else {
            const bool watered = (w.xbit.at(a, b) & kWatered) != 0;
            q.lines.push_back({821, {str(watered ? 819 : 820, !watered)}});
        }
    }
    return q;
}

std::string signText(const SimContext& c, int a, int b) {
    const int v = c.w.xtxt.at(a, b);
    if (v == 0 || v >= 0x33) return {};
    const char* p = reinterpret_cast<const char*>(&c.w.xlab[static_cast<size_t>(v) * 25]);
    return std::string(p, strnlen(p, 24));
}

bool placeSign(SimContext& c, int a, int b, const std::string& text, bool cancelled) {   // 0x44D140
    World& w = c.w;
    int idx = w.xtxt.at(a, b);
    if (idx == 0) {
        for (idx = 1; idx < 0x33; ++idx) if (w.xlab[static_cast<size_t>(idx) * 25] == 0) break;
        if (idx >= 0x33) { c.hooks.showDialog(0x107); return false; }
    } else if (idx >= 0x33) { c.hooks.showDialog(0x108); return false; }
    if (cancelled) { w.xtxt.at(a, b) = 0; return true; }
    if (text.empty()) { releaseTextOverlay(c, idx); w.xtxt.at(a, b) = 0; return true; }
    w.xtxt.at(a, b) = static_cast<uint8_t>(idx);
    const size_t n = std::min<size_t>(text.size(), 24);
    std::memcpy(&w.xlab[static_cast<size_t>(idx) * 25], text.data(), n);
    w.xlab[static_cast<size_t>(idx) * 25 + n] = 0;
    return true;
}

}  // namespace sc2k
