#pragma once
// world.hpp — sim::World arena: fields + method declarations.
// Types live in sim/world_types.hpp (included here for single-include compat).
// Field bodies are split into world_fields_*.inc to stay under the 600-line cap.

#include "sim/world_types.hpp"

namespace sim {

struct World {
#include "sim/world_fields_core.inc"
#include "sim/world_fields_scars.inc"
#include "sim/world_api.hpp"
};

// Build a fresh educational arena: game + AC processes, baseline trust/callbacks.
World make_arena(const char* game_name = "lab-game");

}  // namespace sim
