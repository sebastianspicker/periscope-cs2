#pragma once

// Blue simulation for handle_hijack_proxy. It checks independent handle/read telemetry and the
// strategy-specific scar (proxy-owned VM_READ edge) before reporting a risk-scored detection.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::handle_hijack_proxy {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
  bool proxy_handle = false;
  bool consumer_no_handle = false;
};
BlueResult detect(sim::World& w);
}  // namespace examples::handle_hijack_proxy
