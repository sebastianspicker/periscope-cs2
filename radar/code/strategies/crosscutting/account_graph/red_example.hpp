#pragma once
// Lab RED for `account_graph` (theme multi-step, Simulated).
#include "sim/world.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace examples::account_graph {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
  int accounts = 0;
};
RedResult apply(sim::World& w);
}  // namespace examples::account_graph
