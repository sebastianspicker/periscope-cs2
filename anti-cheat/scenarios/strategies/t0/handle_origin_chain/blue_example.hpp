#pragma once

// Blue simulation for handle_origin_chain (165). Blue walks the duplicate
// chain behind a suspicious handle (owner -> owner of the source handle) and
// flags the origin misattribution an owner-only sensor would report.
#include "sim/world.hpp"

#include <string>
#include <vector>

namespace examples::handle_origin_chain {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::handle_origin_chain
