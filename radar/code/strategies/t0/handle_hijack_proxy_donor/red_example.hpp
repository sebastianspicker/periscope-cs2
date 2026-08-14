#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::handle_hijack_proxy_donor {
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
  std::uint32_t donor_pid = 0;
  std::uint32_t consumer_pid = 0;
  bool handle_duplicated = false;
};
RedResult apply(sim::World& w);
}  // namespace examples::handle_hijack_proxy_donor
