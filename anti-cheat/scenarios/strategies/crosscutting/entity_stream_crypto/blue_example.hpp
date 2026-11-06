#pragma once

// Lab BLUE for `entity_stream_crypto` on sim::World only (theme multi-reason detect/mitigate).

#include "sim/world.hpp"

#include <string>
#include <vector>

namespace examples::entity_stream_crypto {

struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  std::vector<std::string> reasons;
  std::string detail;
};

BlueResult detect(sim::World& w);

}  // namespace examples::entity_stream_crypto
