#pragma once

// Blue simulation for pattern_offset_scan. It checks independent handle/read telemetry and the
// strategy-specific scar (pattern-scan telemetry) before reporting a risk-scored detection.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::pattern_offset_scan {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
  bool foreign_vm_read = false;
  bool bulk_remote_reads = false;
  bool suspicious_cooccurrence = false;
  bool rescan_residual = false;
  int reason_count = 0;
  bool structural_fog = false;
  bool schema_cache_residual = false;
  bool schema_remote_residual = false;
};
BlueResult detect(sim::World& w);
}  // namespace examples::pattern_offset_scan
