#include "simulation/clock.h"

#include <cstdlib>

namespace sc2k {

bool Clock::onTimerTick() {
    // 0x40BBD0: phase counter wraps when |phase| & 7 == 0
    ++phase_;
    if ((std::abs(phase_) & 7) == 0) phase_ = 0;

    bool flag = false;
    switch (speed) {
        case GameSpeed::Paused: flag = (phase_ == 0); break;
        case GameSpeed::Turtle: flag = ((std::abs(phase_) & 3) == 0); break;
        case GameSpeed::Llama: flag = ((std::abs(phase_) & 1) == 0); break;
        case GameSpeed::Cheetah:
        case GameSpeed::AfricanSwallow: flag = true; break;
    }
    // 0x40BCD0: in mode 2 (and not swallow) every second raised flag is dropped
    if (flag && disasterMode && speed != GameSpeed::AfricanSwallow) {
        const uint16_t old = parity_++;
        if (old & 1) flag = false;
    }
    return flag;
}

int Clock::advance(int elapsedMs, int maxSwallowSteps) {
    accumMs_ += elapsedMs;
    int steps = 0;
    while (accumMs_ >= kBaseTickMs) {
        accumMs_ -= kBaseTickMs;
        if (onTimerTick() && speed != GameSpeed::Paused) ++steps;
    }
    if (speed == GameSpeed::AfricanSwallow && steps < maxSwallowSteps) steps = maxSwallowSteps;
    return speed == GameSpeed::Paused ? 0 : steps;
}

}  // namespace sc2k
