#pragma once

// Red simulation for loader_addr_registry. The binary ships no scanner: a
// loader resolves entity addresses out-of-process into an encrypted registry
// section, red decodes them at runtime, and re-resolves after offset drift.
// This educational example mutates only sim::World.
#include "sim/world.hpp"
#include <string>

namespace examples::loader_addr_registry {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::loader_addr_registry
