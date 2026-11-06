#pragma once

#include "sim/world.hpp"

#include <string>

namespace examples::hypercall_mem_read {

struct RedResult {
  bool achieved;
  int steps;
  std::string hv_vendor;
  std::string detail;
};

RedResult apply(sim::World& w);

}  // namespace examples::hypercall_mem_read
