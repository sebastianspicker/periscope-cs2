#pragma once

// Blue simulation for cpuid_hypercall_channel. It scores abnormal CPUID leaf
// probing, the covert-channel scar, and unauthenticated channel abuse with
// independent reasons before reporting a risk-scored verdict.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::cpuid_hypercall_channel {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::cpuid_hypercall_channel
