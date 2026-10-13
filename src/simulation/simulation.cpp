#include "simulation/simulation.h"
#include "simulation/new_city.h"

#include "world/world_hash.h"

namespace sc2k {

struct Simulation::Impl {
    World world;
    CityState state;
    GameTables tables;
    CrtRand rng;
    Lfsr lfsr;
    ZoneHooks defaultHooks;
    ZoneHooks* hooks;
    TrafficStats traffic;
    std::unique_ptr<SimContext> ctx;

    Impl(const CityFile& c, GameTables t, uint32_t seed)
        : world(World::fromCity(c)), state(CityState::fromCity(c)), tables(std::move(t)), rng(seed), lfsr(seed * 2654435761u + 0xACE1u), hooks(&defaultHooks) {
        // runtime values the game derives from saved state at load time
        state.catPop = {state.zonePop[1] + state.zonePop[2], state.zonePop[3] + state.zonePop[4], state.zonePop[5] + state.zonePop[6]};
        if (state.rateLevel == 0) state.rateLevel = 1;
        ctx = std::make_unique<SimContext>(SimContext{world, state, tables, rng, lfsr, *hooks, traffic, {}});
        ctx->lcg.seed(seed ^ 0x5DEECE6Du);
    }
};

Simulation::Simulation(const CityFile& city, GameTables tables, uint32_t seed) : impl_(std::make_unique<Impl>(city, std::move(tables), seed)) {}
Simulation::~Simulation() = default;

void Simulation::tick() { simulationTick(*impl_->ctx); }
void Simulation::thingFrames(int n) { for (int i = 0; i < n; ++i) thingFrame(*impl_->ctx); }
void Simulation::advanceDays(int days, int framesPerDay) {
    for (int d = 0; d < days; ++d) { tick(); thingFrames(framesPerDay); }
}
void Simulation::setHooks(ZoneHooks& hooks) {
    impl_->hooks = &hooks;
    SimContext old = std::move(*impl_->ctx);
    impl_->ctx = std::make_unique<SimContext>(SimContext{impl_->world, impl_->state, impl_->tables, impl_->rng, impl_->lfsr, hooks, impl_->traffic, {}});
    impl_->ctx->things = std::move(old.things);
    impl_->ctx->lcg = old.lcg;
}
void Simulation::replaceCity(const CityFile& c) {
    impl_->world = World::fromCity(c);
    impl_->state = CityState::fromCity(c);
    CityState& st = impl_->state;
    st.catPop = {st.zonePop[1] + st.zonePop[2], st.zonePop[3] + st.zonePop[4], st.zonePop[5] + st.zonePop[6]};
    if (st.rateLevel == 0) st.rateLevel = 1;
    impl_->ctx->things = ThingState{};
}
void Simulation::startNewCity(const CityFile& map, int era, int difficulty) {
    replaceCity(map);
    NewCityOptions o; o.era = era; o.difficulty = difficulty;
    setupNewCity(impl_->world, impl_->state, impl_->tables, impl_->rng, o);
}
World& Simulation::world() { return impl_->world; }
CityState& Simulation::state() { return impl_->state; }
const GameTables& Simulation::tables() const { return impl_->tables; }
SimContext& Simulation::context() { return *impl_->ctx; }
uint64_t Simulation::fingerprint() const { return worldFingerprint(impl_->world, impl_->state); }
CityFile Simulation::toCity(CityFile base) const {
    impl_->world.writeBack(base);
    impl_->state.writeBack(base);
    return base;
}

}  // namespace sc2k
