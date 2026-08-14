#pragma once
#include "sim/world.hpp"
#include <string>
namespace examples::desktop_dup_capture {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::desktop_dup_capture
