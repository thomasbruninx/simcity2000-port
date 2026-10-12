#pragma once
#include <stdexcept>
#include <string>

namespace sc2k {
struct Sc2Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};
}  // namespace sc2k
