#pragma once

#include "sim/world.hpp"

#include <string>

namespace examples::smm_read_channel {

struct RedResult {
  bool achieved;
  int steps;
  bool bypasses_hv;
  std::string detail;
};

RedResult apply(sim::World& w);

}  // namespace examples::smm_read_channel
