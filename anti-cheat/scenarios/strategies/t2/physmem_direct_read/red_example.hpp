#pragma once

#include "sim/world.hpp"

#include <string>

namespace examples::physmem_direct_read {

struct RedResult {
  bool achieved;
  int steps;
  bool via_physical_device;
  std::string detail;
};

RedResult apply(sim::World& w);

}  // namespace examples::physmem_direct_read
