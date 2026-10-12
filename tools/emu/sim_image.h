// Dev-only: mirror a World + CityState into the emulated game's memory and back, so any original routine can be run on it.
#pragma once
#include "emu/emu.h"
#include "simulation/city_state.h"
#include "world/world.h"

namespace sc2k::emu {

struct SimImage {
    Emu& e;
    LayerRef xbld, xzon, xbit, xtxt, xund, xter, altm, xtrf, xplt, xval, xcrm, xpop, xrog, xplc, xfir, tmp;
    uint32_t census = 0, zonePop = 0, c9948 = 0, budget = 0, milCounts = 0, micro = 0, things = 0;

    explicit SimImage(Emu& emu);
    void load(const World& w, const CityState& s);
    void store(World& w, CityState& s) const;
    void setLfsr(uint32_t seed) { e.put<uint32_t>(0x4CCB94, seed); }
    uint16_t lfsrState() const { return e.get<uint16_t>(0x4CCB94); }
    // Neutralise UI / not-yet-ported callees so the original and the port skip the same things.
    void stubUi();
};

// Compare two Worlds / CityStates; returns number of differing elements and appends a summary of the first differences.
long diffWorld(const World& a, const World& b, std::string& detail);
long diffState(const CityState& a, const CityState& b, std::string& detail);

}  // namespace sc2k::emu
