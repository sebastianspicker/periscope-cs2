#pragma once

// Red simulation for stale_chunk_cache. It memoizes container chunk pointers
// (with the generation they were valid at) and serves entity reads from the
// cached chunks - fast until the container reallocates underneath it.
// This educational example mutates only sim::World.
#include "sim/world.hpp"
#include <string>

namespace examples::stale_chunk_cache {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::stale_chunk_cache
