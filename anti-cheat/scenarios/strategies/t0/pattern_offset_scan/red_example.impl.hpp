#pragma once

// Deep implementation of 29_pattern_offset_scan using real CS2 signatures.
// Red uses AOB scanning to locate entity data, then reads entities via RPM.

#include "cs2/signatures.hpp"
#include "lab/aob_scanner.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace examples::pattern_offset_scan {

struct RedScanResult {
  bool achieved = false;
  int patterns_scanned = 0;
  int patterns_found = 0;
  int entities_read = 0;
  std::vector<std::string> resolved_patterns;
  std::string detail;
};

RedScanResult run_deep_scan(sim::World& world, std::uint32_t reader_pid);

}  // namespace examples::pattern_offset_scan
