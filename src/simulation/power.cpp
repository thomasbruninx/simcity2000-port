#include "simulation/power.h"

#include <algorithm>
#include <cassert>

#include "simulation/tile_queue.h"

namespace sc2k {
namespace {

using namespace xbit;


// 0x467340. Returns capacity of this flood; adds supplied tile count to `served`.
int floodPower(World& w, int a0, int b0, const PowerInputs& in, CrtRand& rng, int& capacityAcc, int& served) {
    TileQueue q;
    int supply = 0;
    int consumers = 0;

    // Pass 1: breadth-first over powerable tiles; mark (0x08); sum plant output; count consumer tiles.
    q.reset(a0, b0);
    while (!q.empty()) {
        const auto [a, b] = q.pop();
        if (inMap(a, b) && (w.xbit.at(a, b) & kMark)) continue;
        if (!inMap(a, b)) continue;
        if (!(w.xbit.at(a, b) & kPowerable)) continue;

        const uint8_t id = w.xbld.at(a, b);
        if (id < 0xC6) {
            if (id > 0x6F) ++consumers;
        } else {
            switch (id) {
                case 0xC6: case 0xC7: supply += 0x28; break;
                case 0xC8: {
                    const unsigned r = static_cast<unsigned>(rng.next());
                    const unsigned m = (in.wind >> 3) + 1u;
                    supply += ((w.altm.at(a, b) & 0x1F) + static_cast<int>(r % m)) / 2;
                    break;
                }
                case 0xC9: supply += 0x0B; break;
                case 0xCA: supply += 0x30; break;
                case 0xCB: supply += 0x6F; break;
                case 0xCC: {
                    const unsigned r = static_cast<unsigned>(rng.next());
                    const int m = (100 - in.cloudiness) / 10;
                    assert(m > 0);  // the original divides by zero (crashes) when cloudiness >= 91
                    supply += static_cast<int>(r % static_cast<unsigned>(m)) + 5;
                    break;
                }
                case 0xCD: supply += 0x163; break;
                case 0xCE: supply += 0x22B; break;
                case 0xCF: supply += 0x2C; break;
                default: ++consumers; break;
            }
        }
        w.xbit.at(a, b) |= kMark;
        if (b > 0 && !(w.xbit.at(a, b - 1) & kMark)) q.push(a, b - 1);
        if (a > 0 && !(w.xbit.at(a - 1, b) & kMark)) q.push(a - 1, b);
        if (b < 0x7F && !(w.xbit.at(a, b + 1) & kMark)) q.push(a, b + 1);
        if (a < 0x7F && !(w.xbit.at(a + 1, b) & kMark)) q.push(a + 1, b);
    }

    capacityAcc += supply;
    if (in.ordinances & 0x10000) supply += supply / 12;
    served += std::min(supply, consumers);

    // Pass 2: same traversal over marked tiles; hand out power while supply lasts, clearing marks.
    q.reset(a0, b0);
    while (!q.empty()) {
        const auto [a, b] = q.pop();
        if (!inMap(a, b) || !(w.xbit.at(a, b) & kMark)) continue;
        if (supply != 0) {
            if (w.xbld.at(a, b) > 0x6F) --supply;
            w.xbit.at(a, b) |= kPowered;
        }
        w.xbit.at(a, b) &= static_cast<uint8_t>(~kMark);
        if (b > 0 && (w.xbit.at(a, b - 1) & kMark)) q.push(a, b - 1);
        if (a > 0 && (w.xbit.at(a - 1, b) & kMark)) q.push(a - 1, b);
        if (b < 0x7F && (w.xbit.at(a, b + 1) & kMark)) q.push(a, b + 1);
        if (a < 0x7F && (w.xbit.at(a + 1, b) & kMark)) q.push(a + 1, b);
    }
    return supply;
}

}  // namespace

PowerResult runPowerScan(World& w, const PowerInputs& in, CrtRand& rng) {
    // 0x4671F0: clear mark and powered everywhere.
    for (auto& f : w.xbit.v) f &= 0xB7;

    PowerResult r;
    for (int a = 0; a < CityFile::kMap; ++a) {
        for (int b = 0; b < CityFile::kMap; ++b) {
            const uint8_t id = w.xbld.at(a, b);
            if (!(w.xbit.at(a, b) & kPowered) && id > 0xC5 && id < 0xD0)
                floodPower(w, a, b, in, rng, r.capacity, r.served);
        }
    }
    if (r.capacity == 0) r.percentUsed = 100;
    else r.percentUsed = r.served * 100 / r.capacity;
    if (r.percentUsed > 100) r.percentUsed = 100;
    for (auto f : w.xbit.v) r.poweredTiles += (f & kPowered) != 0;
    return r;
}

}  // namespace sc2k
