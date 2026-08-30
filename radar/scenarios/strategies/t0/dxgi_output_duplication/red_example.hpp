#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::dxgi_output_duplication {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; int frames_acquired = 0; bool composite_rendered = false; };
RedResult apply(sim::World& w);
}  // namespace examples::dxgi_output_duplication
