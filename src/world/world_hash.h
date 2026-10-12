// FNV-1a fingerprint of a simulated world, used for golden regression tests (hashes only; no game data).
#pragma once
#include <cstdint>

#include "simulation/city_state.h"
#include "world/world.h"

namespace sc2k {

inline uint64_t worldFingerprint(const World& w, const CityState& s) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](const void* p, size_t n) { const auto* b = static_cast<const uint8_t*>(p); for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; } };
    mix(w.xbld.v.data(), w.xbld.v.size()); mix(w.xzon.v.data(), w.xzon.v.size()); mix(w.xbit.v.data(), w.xbit.v.size());
    mix(w.xtxt.v.data(), w.xtxt.v.size()); mix(w.xund.v.data(), w.xund.v.size()); mix(w.xtrf.v.data(), w.xtrf.v.size());
    mix(w.xmic.data(), w.xmic.size());
    mix(s.census.data(), s.census.size() * 2); mix(s.zonePop.data(), s.zonePop.size() * 4); mix(s.militaryCounts.data(), s.militaryCounts.size() * 2);
    return h;
}

}  // namespace sc2k
