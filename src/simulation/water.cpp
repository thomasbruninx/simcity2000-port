#include "simulation/water.h"

#include "simulation/tile_queue.h"

namespace sc2k {
namespace {

using namespace xbit;

constexpr uint8_t kPump = 0xDC, kTower = 0xEB, kTreatment = 0xF4, kDesal = 0xFA;

void floodWater(World& w, int a0, int b0, const WaterInputs& in, unsigned& servedAcc, unsigned& supplyAcc) {
    TileQueue q;
    unsigned supply = 0, consumers = 0, towerCap = 0;

    // Pass 1 (0x464370): BFS over piped tiles; mark; sum pump/desal/tower supply; count consumers.
    q.reset(a0, b0);
    while (!q.empty()) {
        const auto [a, b] = q.pop();
        if (!inMap(a, b)) continue;
        if (w.xbit.at(a, b) & kMark) continue;
        if (!(w.xbit.at(a, b) & kPiped)) continue;

        const uint8_t id = w.xbld.at(a, b);
        if (id > 0x6F) {
            switch (id) {
                case kPump:
                    if (w.xbit.at(a, b) & kPowered) {
                        supply += static_cast<unsigned>(in.cloudiness >> 1) + static_cast<unsigned>(static_cast<int>(in.waterFactor) * 5);
                        for (int da = a - 1; da <= a + 1; ++da)
                            for (int db = b - 1; db <= b + 1; ++db)
                                if (inMap(da, db) && (w.xbit.at(da, db) & (kSaltWater | kWater)) == kWater) supply += 10;
                    }
                    break;
                case kTower:
                    towerCap += 100;
                    if (w.xbit.at(a, b) & kWatered) supply += 100;
                    w.xbit.at(a, b) &= static_cast<uint8_t>(~kWatered);
                    break;
                case kTreatment:
                    break;
                case kDesal:
                    if (w.xbit.at(a, b) & kPowered) {
                        for (int da = a - 1; da <= a + 1; ++da)
                            for (int db = b - 1; db <= b + 1; ++db)
                                if (inMap(da, db) && (w.xbit.at(da, db) & (kSaltWater | kWater)) == (kSaltWater | kWater)) supply += 0x14;
                    }
                    break;
                default:
                    ++consumers;
                    break;
            }
        }
        w.xbit.at(a, b) |= kMark;
        if (b > 0 && !(w.xbit.at(a, b - 1) & kMark)) q.push(a, b - 1);
        if (a > 0 && !(w.xbit.at(a - 1, b) & kMark)) q.push(a - 1, b);
        if (b < 0x7F && !(w.xbit.at(a, b + 1) & kMark)) q.push(a, b + 1);
        if (a < 0x7F && !(w.xbit.at(a + 1, b) & kMark)) q.push(a + 1, b);
    }

    if (supply < consumers) consumers = supply;
    if (supply - consumers < towerCap) towerCap = supply - consumers;
    towerCap = (towerCap + 0x32) / 100;
    servedAcc += consumers;
    supplyAcc += supply;

    // Pass 2: hand out water along the marked component, clearing marks.
    q.reset(a0, b0);
    while (!q.empty()) {
        const auto [a, b] = q.pop();
        if (!inMap(a, b) || !(w.xbit.at(a, b) & kMark)) continue;
        const uint8_t id = w.xbld.at(a, b);
        switch (id) {
            case kPump: case kTreatment: case kDesal:
                if (w.xbit.at(a, b) & kPowered) w.xbit.at(a, b) |= kWatered;
                break;
            case kTower:
                if ((w.xbit.at(a, b) & kPowered) && towerCap != 0) { w.xbit.at(a, b) |= kWatered; --towerCap; }
                break;
            default:
                if (consumers != 0) {
                    w.xbit.at(a, b) |= kWatered;
                    if (id > 0x6F) --consumers;
                }
                break;
        }
        w.xbit.at(a, b) &= static_cast<uint8_t>(~kMark);
        if (b > 0 && (w.xbit.at(a, b - 1) & kMark)) q.push(a, b - 1);
        if (a > 0 && (w.xbit.at(a - 1, b) & kMark)) q.push(a - 1, b);
        if (b < 0x7F && (w.xbit.at(a, b + 1) & kMark)) q.push(a, b + 1);
        if (a < 0x7F && (w.xbit.at(a + 1, b) & kMark)) q.push(a + 1, b);
    }
}

}  // namespace

WaterResult runWaterScan(World& w, const WaterInputs& in) {
    // 0x463EE0 part 1: clear flags. Water towers keep their stored-water (0x10) flag.
    for (int a = 0; a < CityFile::kMap; ++a)
        for (int b = 0; b < CityFile::kMap; ++b) {
            auto& f = w.xbit.at(a, b);
            if (w.xbld.at(a, b) != kTower) f &= static_cast<uint8_t>(~kWatered);
            f &= static_cast<uint8_t>(~kMark);
        }

    WaterResult r;
    auto visit = [&](int row, int col) {
        if (!(w.xbit.at(row, col) & kWatered)) {
            const uint8_t id = w.xbld.at(row, col);
            if ((id == kPump || id == kDesal) && (w.xbit.at(row, col) & kPowered))
                floodWater(w, row, col, in, r.served, r.supply);
        }
    };
    // Four iteration orders chosen by view rotation (VERIFIED from 0x463EE0).
    switch (in.rotation) {
        case 0: for (int c = 0; c < 128; ++c) for (int row = 0; row < 128; ++row) visit(row, c); break;
        case 1: for (int row = 0; row < 128; ++row) for (int c = 127; c >= 0; --c) visit(row, c); break;
        case 2: for (int c = 127; c >= 0; --c) for (int row = 127; row >= 0; --row) visit(row, c); break;
        case 3: for (int row = 127; row >= 0; --row) for (int c = 0; c < 128; ++c) visit(row, c); break;
        default: break;  // the original leaves the switch without action for other values
    }
    r.percent = r.supply != 0 ? static_cast<int>(r.served * 100 / r.supply) : 100;
    for (auto f : w.xbit.v) r.wateredTiles += (f & kWatered) != 0;
    return r;
}

}  // namespace sc2k
