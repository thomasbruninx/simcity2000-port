// FNV-1a fingerprint of a simulated world, used for golden regression tests (hashes only; no game data).
#pragma once
#include <cstdint>

#include "simulation/city_state.h"
#include "simulation/sim_context.h"
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

// Fingerprint of the thing system's state: thing records, the XTXT overlay, the counters and the three RNG states.
inline uint64_t thingFingerprint(const World& w, const ThingState& t, uint32_t crtState, uint32_t lfsrState, uint32_t lcgState) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](const void* p, size_t n) { const auto* b = static_cast<const uint8_t*>(p); for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; } };
    mix(w.xthg.data(), w.xthg.size()); mix(w.xtxt.v.data(), w.xtxt.v.size()); mix(w.xbld.v.data(), w.xbld.v.size()); mix(w.xmic.data(), w.xmic.size());
    const int32_t c[] = {t.animFrame, t.planes, t.helis, t.ships, t.visitors, t.sailboats, t.monsters, t.trains, t.tornados, t.focusA, t.focusB, t.disasterRequest, t.lastSpawned, t.fireMessageKind};
    mix(c, sizeof c);
    mix(&crtState, 4); mix(&lfsrState, 4); mix(&lcgState, 4);
    return h;
}

}  // namespace sc2k
