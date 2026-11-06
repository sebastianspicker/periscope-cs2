#pragma once

#include "sim/world.hpp"
#include <string>

namespace examples::multi_process_ipc_split {
struct RedResult {
  bool achieved;
  int steps;
  int split_count;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::multi_process_ipc_split
