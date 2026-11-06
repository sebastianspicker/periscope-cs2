#pragma once
// Lab RED for `vpn_proxy_graph` (theme multi-step, Simulated).
#include "sim/world.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace examples::vpn_proxy_graph {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
  int cluster = 0;
};
RedResult apply(sim::World& w);
}  // namespace examples::vpn_proxy_graph
