#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::handle_hijack_proxy_donor {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
  bool donor_handle_found = false;
  bool consumer_has_no_handle = false;
};
BlueResult detect(sim::World& w);
}  // namespace examples::handle_hijack_proxy_donor
