#pragma once

// Blue team detector for AOB/pattern scanning behavior.
// Scans leave observable traces: bulk reads, sequential access patterns,
// cross-process memory access, etc.

#include "ac/types.hpp"
#include "lab/aob_scanner.hpp"
#include "lab/lab_memory.hpp"
#include "sim/world.hpp"

#include <string>
#include <vector>

namespace lab {

struct ScanDetectionReport {
  bool bulk_read_detected = false;
  bool pattern_scan_suspected = false;
  bool sequential_access = false;
  bool marker_context = false;
  bool rescan_residual = false;
  bool aob_report_correlated = false;
  int estimated_patterns_scanned = 0;
  int signal_count = 0;
  double risk_score = 0.0;
  std::vector<std::string> indicators;
  std::string detail;
};

class ScanDetector {
 public:
  explicit ScanDetector(sim::World& world);

  // Primary path: analyze World remote-read telemetry.
  ScanDetectionReport analyze();

  // Correlate a completed AOB report with World telemetry.
  ScanDetectionReport analyze_with_aob(const AobScanReport& aob);

  // Analyze LabMemoryBackend stats without a World (fixture-only unit tests).
  static ScanDetectionReport analyze_backend(const LabMemoryStats& stats,
                                             bool pattern_markers_present = false);

 private:
  sim::World& world_;
  void finalize(ScanDetectionReport& report) const;
};

}  // namespace lab
