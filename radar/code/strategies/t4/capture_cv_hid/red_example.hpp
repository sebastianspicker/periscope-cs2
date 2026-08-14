#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::capture_cv_hid {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::capture_cv_hid
