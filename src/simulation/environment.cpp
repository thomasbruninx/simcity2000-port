#include "simulation/environment.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace sc2k {
namespace {

using namespace xbit;
constexpr int kN = CityFile::kMap;      // 128
constexpr int kH = CityFile::kHalf;     // 64
constexpr int kQ = CityFile::kQuarter;  // 32

// 128x128 int16 scratch (game: row-pointer table @0x4CB710). The code treats it as 64x64 in stages 1/2/8, as
// 32x32 in stages 4..7 and, through the pointer table @0x4CB790, as rows 32..63 ("B") -- literal port keeps one array.
struct Scratch {
    std::array<int16_t, kN * kN> v{};
    int16_t& at(int a, int b) { return v[static_cast<size_t>(a) * kN + b]; }
};

// 0x46B3B0: saturating add of a signed amount into a 32x32 byte layer, ignoring out-of-range cells.
void addCell(Grid<uint8_t, kQ>& g, int r, int c, int16_t amount) {
    if (r < 0 || c < 0 || r >= kQ || c >= kQ) return;
    const int16_t s = static_cast<int16_t>(amount + g.at(r, c));
    g.at(r, c) = s > 0xFF ? 0xFF : (s < 0 ? 0 : static_cast<uint8_t>(s));
}

// 0x46B420: spread `strength` over a diamond-ish footprint with decaying weights.
void spread(Grid<uint8_t, kQ>& g, int r, int c, int16_t strength) {
    addCell(g, r, c, strength);
    int16_t s = static_cast<int16_t>((static_cast<int>(strength) << 2) / 5);
    addCell(g, r - 1, c, s); addCell(g, r + 1, c, s); addCell(g, r, c - 1, s); addCell(g, r, c + 1, s);
    s = static_cast<int16_t>((s * 3) / 4);   // original: (x + (x>>31 & 3)) >> 2 on 3x, i.e. truncating /4
    addCell(g, r - 1, c - 1, s); addCell(g, r + 1, c - 1, s); addCell(g, r - 1, c + 1, s); addCell(g, r + 1, c + 1, s);
    s = static_cast<int16_t>((s * 2) / 3);
    addCell(g, r - 2, c - 1, s); addCell(g, r - 2, c, s); addCell(g, r - 2, c + 1, s);
    addCell(g, r + 2, c - 1, s); addCell(g, r + 2, c, s); addCell(g, r + 2, c + 1, s);
    addCell(g, r - 1, c - 2, s); addCell(g, r, c - 2, s); addCell(g, r + 1, c - 2, s);
    addCell(g, r - 1, c + 2, s); addCell(g, r, c + 2, s); addCell(g, r + 1, c + 2, s);
    s = static_cast<int16_t>(s / 2);
    addCell(g, r - 3, c - 1, s); addCell(g, r - 3, c, s); addCell(g, r - 3, c + 1, s);
    addCell(g, r + 3, c - 1, s); addCell(g, r + 3, c, s); addCell(g, r + 3, c + 1, s);
    addCell(g, r - 1, c - 3, s); addCell(g, r, c - 3, s); addCell(g, r + 1, c - 3, s);
    addCell(g, r - 1, c + 3, s); addCell(g, r, c + 3, s); addCell(g, r + 1, c + 3, s);
    addCell(g, r - 2, c - 2, s); addCell(g, r - 2, c + 2, s); addCell(g, r + 2, c - 2, s); addCell(g, r + 2, c + 2, s);
}

}  // namespace

EnvResult runEnvironmentScan(World& w, const GameTables& t, const EnvInputs& in) {
    Scratch tmp;
    EnvResult res;
    const Grid<uint8_t, kN>& layer8F70 = in.layer4C8F70 ? *in.layer4C8F70 : w.xter;
    auto B = [&](int q, int p) -> int16_t& { return tmp.at(32 + q, p); };   // pointer table @0x4CB790 == rows 32.. of 0x4CB710

    // ---- Stage 1: pollution accumulation (64x64) ----
    for (int r = 0; r < kH; ++r)
        for (int c = 0; c < kH; ++c) {
            int v = w.xtrf.at(r, c) / 5 + w.xplt.at(r, c);
            const int ids[4] = {w.xbld.at(2 * r, 2 * c), w.xbld.at(2 * r + 1, 2 * c), w.xbld.at(2 * r, 2 * c + 1), w.xbld.at(2 * r + 1, 2 * c + 1)};
            for (int id : ids) {
                if (id > 0x6F) v += t.pollution[id];
                if (id == 5) v += 200;
            }
            tmp.at(r, c) = static_cast<int16_t>(v);
        }

    // ---- Stage 2: smooth into the pollution layer ----
    int16_t base = static_cast<int16_t>((in.waterFlag - in.c8440) + 4);
    if (in.ordinances & 0x80000) base = static_cast<int16_t>(base + 1);
    res.pollutionTotal = 0;
    for (int r = 0; r < kH; ++r)
        for (int c = 0; c < kH; ++c) {
            int v = tmp.at(r, c) * 2;
            int div = base < 1 ? 1 : base;
            if (r > 0) { ++div; v += tmp.at(r - 1, c); }
            if (r < kH - 1) { ++div; v += tmp.at(r + 1, c); }
            if (c > 0) { ++div; v += tmp.at(r, c - 1); }
            if (c < kH - 1) { ++div; v += tmp.at(r, c + 1); }
            v /= div;
            if (v > 0xFF) v = 0xFF;
            w.xplt.at(r, c) = static_cast<uint8_t>(v);
            res.pollutionTotal += v;
        }

    // ---- Stage 3: city centre; stamp 0x28 on building tiles; clear their marks ----
    uint32_t rowSum = 0, colSum = 0;
    int count = 1;
    for (int a = 0; a < kN; ++a)
        for (int b = 0; b < kN; ++b)
            if (w.xbld.at(a, b) > 0x6F) {
                rowSum += a; colSum += b; ++count;
                tmp.at(a, b) = 0x28;
                w.xbit.at(a, b) &= static_cast<uint8_t>(~kMark);
            }
    const int16_t centerR = static_cast<int16_t>(rowSum / static_cast<uint32_t>(count * 2));
    const int16_t centerC = static_cast<int16_t>(colSum / static_cast<uint32_t>(count * 2));
    res.centerRow2 = static_cast<int16_t>(centerR * 2);
    res.centerCol2 = static_cast<int16_t>(centerC * 2);

    // ---- Stage 4: terrain/vegetation/water accumulators on 32x32 (tmp rows 0..31 and B), "developed" marks ----
    res.developedTiles = 0;
    for (int a = 0; a < kN; ++a)
        for (int b = 0; b < kN; ++b) {
            const int id = w.xbld.at(a, b);
            const int q = a / 4, p = b / 4;
            int16_t A = tmp.at(q, p);
            int16_t Bv = B(q, p);
            if (id == 0) {
                if (!(w.xbit.at(a, b) & kWater)) A = static_cast<int16_t>(A + 4);
                else { A = static_cast<int16_t>(A + 0xC); Bv = static_cast<int16_t>(Bv + 0xC); }
            } else if (id == 0xD5) {
                A = static_cast<int16_t>(A + 0x28);
            } else if (id < 6 || id > 0xD) {
                if (id < 6) A = static_cast<int16_t>(A - 0x14);
            } else {
                A = static_cast<int16_t>(A + 0x14);
            }
            if (id > 0x1C || (w.xzon.at(a, b) & 0xF) != 0) {
                w.xbit.at(a / 2, b / 2) |= kMark;
                ++res.developedTiles;
            }
            if (w.xbit.at(a, b) & kWatered) { A = static_cast<int16_t>(A + 4); Bv = static_cast<int16_t>(Bv + 4); }
            const uint8_t u = layer8F70.at(a, b);
            if (u != 0 && u < 0x10) A = static_cast<int16_t>(A + 0xC);
            tmp.at(q, p) = A;
            B(q, p) = Bv;
        }

    // ---- Stage 4b: land value (64x64) ----
    res.landValueTotal = 0;
    for (int r = 0; r < kH; ++r)
        for (int c = 0; c < kH; ++c) {
            if (!(w.xbit.at(r, c) & kMark)) { w.xval.at(r, c) = 0; continue; }
            const int q = r / 2, p = c / 2;
            uint8_t z = w.xzon.at(2 * r, 2 * c);
            if ((z & 0xF) == 0) z = w.xzon.at(2 * r + 1, 2 * c + 1);
            const int zone = z & 0xF;
            const int dr = std::abs(centerR - r), dc = std::abs(static_cast<int>(centerC) - c);
            int v;
            if (zone == 3 || zone == 4) {
                int sum = tmp.at(q, p), cnt = 1;
                if (q > 0) { sum += tmp.at(q - 1, p); ++cnt; }
                if (q < kQ - 1) { sum += tmp.at(q + 1, p); ++cnt; }
                if (p > 0) { sum += tmp.at(q, p - 1); ++cnt; }
                if (p < kQ - 1) { sum += tmp.at(q, p + 1); ++cnt; }
                v = sum / cnt;
                const int bonus = (0x40 - dc) - dr;
                if (bonus > 0) v += bonus;
                v = ((v - (w.xplt.at(r, c) >> 2)) - (w.xcrm.at(r, c) / 3)) + (w.xpop.at(q, p) / 3);
            } else if (zone == 5 || zone == 6) {
                // Mixed-array average (original quirk): self and horizontal neighbours from B, vertical from tmp rows q+-1.
                int sum = B(q, p), cnt = 1;
                if (q > 0) { sum += tmp.at(q - 1, p); ++cnt; }
                if (q < kQ - 1) { sum += tmp.at(q + 1, p); ++cnt; }
                if (p > 0) { sum += B(q, p - 1); ++cnt; }
                if (p < kQ - 1) { sum += B(q, p + 1); ++cnt; }
                v = sum / cnt;
                if (zone == 6) v += 0x15;
                const int bonus = ((0x40 - dc) - dr) / 4;
                if (bonus > 0) v += bonus;
                v -= (w.xplt.at(r, c) >> 4);
                v -= (w.xcrm.at(r, c) >> 2);
            } else {
                int sum = tmp.at(q, p), cnt = 1;
                if (q > 0) { sum += tmp.at(q - 1, p); ++cnt; }
                if (q < kQ - 1) { sum += tmp.at(q + 1, p); ++cnt; }
                if (p > 0) { sum += tmp.at(q, p - 1); ++cnt; }
                if (p < kQ - 1) { sum += tmp.at(q, p + 1); ++cnt; }
                v = sum / cnt;
                if (w.xpop.at(q, p) < 0x40) v += 0x15;
                const int bonus = ((0x40 - dc) - dr) / 2;
                if (bonus > 0) v += bonus;
                v -= w.xplt.at(r, c) / 5;
                v -= w.xcrm.at(r, c) / 3;
            }
            const int id = w.xbld.at(2 * r, 2 * c);
            if (id > 0x6F && t.landValueClass[id] == 2) v -= v / 2;
            if (v < 0) v = 0; else if (v > 0xFF) v = 0xFF;
            w.xval.at(r, c) = static_cast<uint8_t>(v);
            res.landValueTotal += v;
        }

    // ---- Stage 5: reset accumulators and coverage ----
    for (int q = 0; q < kQ; ++q)
        for (int p = 0; p < kQ; ++p) { tmp.at(q, p) = 0; w.xplc.at(q, p) = 0; w.xfir.at(q, p) = 0; }

    // ---- Stage 6: density accumulation and police/fire coverage over interior tiles ----
    for (int a = 1; a < kN - 1; ++a)
        for (int b = 1; b < kN - 1; ++b) {
            const int id = w.xbld.at(a, b);
            const int q = a / 4, p = b / 4;
            if (id < 0x70) continue;
            if (id > 0xC5) {
                tmp.at(q, p) = static_cast<int16_t>(tmp.at(q, p) + ((id >= 0xFB && id <= 0xFE) ? 0xC : 2));
                if (!(w.xzon.at(a, b) & 0x80)) continue;
                const bool powered = (w.xbit.at(a, b) & kPowered) != 0;
                if (id == 0xD2) {
                    int16_t s = static_cast<int16_t>(((in.policeBase + 5) * in.policeFunding) / 2);
                    if (!powered) s = static_cast<int16_t>(s / 2);
                    spread(w.xplc, q, p, s);
                } else if (id == 0xD3) {
                    int16_t s = static_cast<int16_t>((in.fireFunding * 5) / 2);
                    if (!powered) s = static_cast<int16_t>(s / 2);
                    spread(w.xfir, q, p, s);
                }
                continue;
            }
            tmp.at(q, p) = static_cast<int16_t>(tmp.at(q, p) + t.population[id]);
            if (in.ordinances & 0x800) { uint8_t& v = w.xplc.at(q, p); if (v < 0xFE) v = static_cast<uint8_t>(v + 2); }
            if (in.ordinances & 0x10) { uint8_t& v = w.xfir.at(q, p); if (v < 0xFE) v = static_cast<uint8_t>(v + 2); }
        }

    // ---- Stage 7: population density and rate of growth (32x32) ----
    for (int q = 0; q < kQ; ++q)
        for (int p = 0; p < kQ; ++p) {
            int v = tmp.at(q, p) * 4;
            if (v > 0xFF) v = 0xFF;
            const uint8_t old = w.xpop.at(q, p);
            w.xpop.at(q, p) = static_cast<uint8_t>(v);
            int g = w.xrog.at(q, p) * 7 + static_cast<int16_t>((static_cast<int16_t>(v) - old) * 8 + 0x80);
            g = (g + ((g >> 31) & 7)) >> 3;
            if (static_cast<int16_t>(g) < 0) g = 0;
            if (static_cast<int16_t>(g) > 0xFF) g = 0xFF;
            w.xrog.at(q, p) = static_cast<uint8_t>(g);
        }

    // ---- Stage 8: crime ----
    res.crimeTotal = 0;
    for (int r = 0; r < kH; ++r)
        for (int c = 0; c < kH; ++c) {
            if (!(w.xbit.at(r, c) & kMark)) { tmp.at(r, c) = 0; continue; }
            const int q = r / 2, p = c / 2;
            int16_t v = static_cast<int16_t>((w.xpop.at(q, p) - (w.xval.at(r, c) >> 2)) - (w.xplc.at(q, p) >> 1));
            if (in.ordinances & 4) v = static_cast<int16_t>(v + 0x10);
            tmp.at(r, c) = v;
        }
    for (int r = 0; r < kH; ++r)
        for (int c = 0; c < kH; ++c) {
            int sum = tmp.at(r, c), cnt = 1;
            if (r > 0) { sum += tmp.at(r - 1, c); ++cnt; }
            if (r < kH - 1) { sum += tmp.at(r + 1, c); ++cnt; }
            if (c > 0) { sum += tmp.at(r, c - 1); ++cnt; }
            if (c < kH - 1) { sum += tmp.at(r, c + 1); ++cnt; }
            int v = sum / cnt;
            if (v > 0xFF) v = 0xFF; else if (v < 0) v = 0;
            w.xcrm.at(r, c) = static_cast<uint8_t>(v);
            res.crimeTotal += v;
        }
    return res;
}

}  // namespace sc2k
