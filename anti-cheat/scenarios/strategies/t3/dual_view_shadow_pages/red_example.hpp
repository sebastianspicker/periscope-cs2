#pragma once

// Red simulation for dual_view_shadow_pages. It extends the ept_hide_ac base
// with a dual-view split: the execute view carries the patched code while the
// read view still reports pristine bytes, so guest self-reads and self-hashes
// both stay clean. This educational example mutates only sim::World.
#include "sim/world.hpp"
#include <string>

namespace examples::dual_view_shadow_pages {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::dual_view_shadow_pages
