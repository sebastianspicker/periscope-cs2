#pragma once

#include "sim/world.hpp"
#include <string>

namespace examples::scattered_entity_read {
struct RedResult {
  bool achieved;
  int steps;
  bool jitter_applied;
  bool non_sequential;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::scattered_entity_read
