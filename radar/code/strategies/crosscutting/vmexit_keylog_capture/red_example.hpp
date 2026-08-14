#pragma once

#include "sim/world.hpp"

#include <string>

namespace examples::vmexit_keylog_capture {

struct RedResult {
  bool achieved;
  int steps;
  int keys_captured;
  std::string detail;
};

RedResult apply(sim::World& w);

}  // namespace examples::vmexit_keylog_capture
