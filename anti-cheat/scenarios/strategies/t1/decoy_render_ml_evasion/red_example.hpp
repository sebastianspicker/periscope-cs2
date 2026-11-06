#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::decoy_render_ml_evasion {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; int decoy_frames = 0; int ml_patterns = 0; bool confusion_active = false; };
RedResult apply(sim::World& w);
}  // namespace examples::decoy_render_ml_evasion
