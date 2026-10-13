// Headless simulation driver: owns a city's world, state, tables and the three RNGs and advances it day by day.
// One day = SimulationTick (25 phases, docs/simulation/events-and-tick.md); things are animated separately by
// thingFrame() the way the original's idle handler does (docs/simulation/things.md): the caller chooses how many frames pass per day.
#pragma once
#include <memory>
#include <string>

#include "simulation/sim_context.h"

namespace sc2k {

class Simulation {
public:
    // Builds a simulation from a loaded city. `tables` come from GameTables::fromExe (read from the user's SIMCITY.EXE).
    Simulation(const CityFile& city, GameTables tables, uint32_t seed = 1);
    ~Simulation();
    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    void tick();                                  // one simulation day
    void thingFrames(int n);                      // n animation frames of the thing system
    void advanceDays(int days, int thingFramesPerDay = 8);
    void setHooks(ZoneHooks& hooks);              // replace the default (no-op) UI hooks

    World& world();
    CityState& state();
    const GameTables& tables() const;
    SimContext& context();
    void replaceCity(const CityFile& city);       // loads another city into the same objects (references stay valid)
    void startNewCity(const CityFile& map, int era, int difficulty, const struct TerrainParams* terrain = nullptr);   // replaceCity (+ random terrain 0x410430) + the new-city setup 0x42DDE0
    uint64_t fingerprint() const;                 // worldFingerprint of the current state
    CityFile toCity(CityFile base) const;         // writes the current world + state into a copy of `base`

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace sc2k
