#pragma once
// Lab BLUE: protector suite + watchdog/anti-suspend micro-detail residual.
#include "sim/world.hpp"
#include <string>
namespace examples::protector_suite {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int reasons = 0;
  bool watchdog_detail = false;
  bool anti_suspend_detail = false;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::protector_suite
