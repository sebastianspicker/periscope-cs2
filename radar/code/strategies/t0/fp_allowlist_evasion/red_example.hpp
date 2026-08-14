#pragma once

// Red simulation for fp_allowlist_evasion. It plants the observable scar: reputable-looking reader co-residence.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <string>

namespace examples::fp_allowlist_evasion {
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
  bool is_legit_only = false;
  bool is_true_radar = false;
};
RedResult apply(sim::World& w);
RedResult apply_legit_only(sim::World& w);
RedResult apply_true_radar(sim::World& w);
}  // namespace examples::fp_allowlist_evasion
