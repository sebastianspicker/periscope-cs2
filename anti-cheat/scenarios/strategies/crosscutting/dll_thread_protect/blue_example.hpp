#pragma once
#include "sim/world.hpp"
#include <string>
namespace examples::dll_thread_protect {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::dll_thread_protect
