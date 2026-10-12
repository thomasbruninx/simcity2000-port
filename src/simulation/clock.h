// Simulation clock and calendar reconstructed from build .022 (docs/simulation/clock-and-calendar.md).
#pragma once
#include <cstdint>

namespace sc2k {

enum class GameSpeed : int16_t { Paused = 1, Turtle = 2, Llama = 3, Cheetah = 4, AfricanSwallow = 5 };

struct Calendar {
    static constexpr int kTicksPerMonth = 25;  // 0x19
    static constexpr int kTicksPerYear = 300;  // 12 * 25
    int32_t day = 0;                           // original: signed 32-bit counter, incremented per sim step

    int month() const { return (day / kTicksPerMonth) % 12; }            // 0 = January
    int season() const { return ((month() + 1) % 12) / 3; }              // 0 = Dec-Feb
    int year() const { return day / kTicksPerYear; }                     // years since start
    int phase() const { return day % kTicksPerMonth; }                   // selects the per-tick work
};

// Reproduces the 200 ms WM_TIMER pacing and the speed gating of the original OnTimer.
// advance() returns how many simulation steps are due; the caller runs them.
class Clock {
public:
    static constexpr int kBaseTickMs = 200;
    GameSpeed speed = GameSpeed::Paused;
    bool disasterMode = false;  // original g_mode == 2 (HYPOTHESIS: disaster) halves the step rate

    // Feed one base timer tick (what the mm-timer callback posts).
    // Returns true if the "simulate" flag is raised by this tick.
    bool onTimerTick();
    // Convenience: accumulate wall time, return number of due steps.
    // African swallow: the original steps every idle iteration; here capped at maxSwallowSteps per call.
    int advance(int elapsedMs, int maxSwallowSteps = 8);

private:
    int32_t phase_ = 0;
    uint16_t parity_ = 0;
    int accumMs_ = 0;
};

}  // namespace sc2k
