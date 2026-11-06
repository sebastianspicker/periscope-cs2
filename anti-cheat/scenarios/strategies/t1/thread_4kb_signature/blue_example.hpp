#pragma once
#include "ac/types.hpp"
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace strategy::t1_thread_4kb_signature {

struct BlueResult {
  int patterns_found{0};
  bool rwx_detected{false};
  bool detected{false};
  bool mitigated{false};
  int signals{0};
  std::vector<std::string> reasons;
  std::string detail;
};

class Blue {
 public:
  BlueResult detect(const sim::World& w) noexcept;
  BlueResult mitigate(sim::World& w) noexcept;
  static constexpr const char* kDescription =
      "Copy 4KB at thread start address, scan for known cheat patterns";
};

}  // namespace strategy::t1_thread_4kb_signature
