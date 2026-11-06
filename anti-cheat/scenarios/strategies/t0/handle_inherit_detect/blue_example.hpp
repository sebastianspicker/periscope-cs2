#pragma once
#include "ac/types.hpp"
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace strategy::t0_handle_inherit_detect {

struct BlueResult {
  bool inheritance_chain_detected{false};
  bool detected{false};
  bool mitigated{false};
  int signals{0};
  uint32_t child_pid{};
  std::vector<std::string> reasons;
  std::string detail;
};

class Blue {
 public:
  BlueResult detect(const sim::World& w) noexcept;
  BlueResult mitigate(sim::World& w) noexcept;
  static constexpr const char* kDescription =
      "Detects handle inheritance chains: a child process with a VM_READ "
      "handle to cs2.exe whose parent also has one. Red flags parent-child handle pairs.";
};

}  // namespace strategy::t0_handle_inherit_detect
