#pragma once

// Red simulation for handle_hijack_proxy. It plants the observable scar: proxy-owned VM_READ edge.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <string>

namespace examples::handle_hijack_proxy {
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
  std::uint32_t proxy_pid = 0;
  std::uint32_t actor_pid = 0;
};
RedResult apply(sim::World& w);
}  // namespace examples::handle_hijack_proxy
