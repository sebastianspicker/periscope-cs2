#include "lab/scan_detector.hpp"

#include <algorithm>
#include <sstream>

namespace lab {

ScanDetector::ScanDetector(sim::World& world) : world_(world) {}

void ScanDetector::finalize(ScanDetectionReport& report) const {
  report.signal_count = static_cast<int>(report.indicators.size());
  std::ostringstream oss;
  oss << (report.pattern_scan_suspected
              ? "AOB scan behavior suspected from correlated read telemetry"
              : "no correlated AOB scan behavior observed")
      << " risk=" << report.risk_score
      << " signals=" << report.signal_count;
  report.detail = oss.str();
}

ScanDetectionReport ScanDetector::analyze() {
  ScanDetectionReport report;

  constexpr std::uint32_t kBulkReadBytes = 1u << 20;
  constexpr std::uint32_t kBurstReadOps = 4;
  report.bulk_read_detected = world_.remote_read_bytes >= kBulkReadBytes ||
                              world_.remote_read_ops >= kBurstReadOps;
  if (report.bulk_read_detected) {
    report.indicators.push_back("high-volume or burst cross-process reads");
  }

  // The simulation records deliberate scatter reads. A burst without that
  // evasion is the available sequential-access signal.
  report.sequential_access = world_.remote_read_ops >= kBurstReadOps &&
                             !world_.scattered_read_pattern;
  if (report.sequential_access) {
    report.indicators.push_back("sequential remote-read burst");
  }
  if (world_.scattered_read_pattern) {
    report.indicators.push_back("scattered reads still indicate entity collection");
  }

  report.marker_context = world_.lab_pattern_marker_present;
  report.pattern_scan_suspected =
      report.bulk_read_detected &&
      (report.sequential_access || report.marker_context);
  report.estimated_patterns_scanned =
      report.pattern_scan_suspected
          ? std::max(1, static_cast<int>(world_.remote_read_ops / 2))
          : 0;
  if (report.marker_context && report.pattern_scan_suspected) {
    report.indicators.push_back(
        "AOB marker layout present during remote scan activity");
  }

  double risk = 0.0;
  if (report.bulk_read_detected) risk += 0.35;
  if (report.sequential_access) risk += 0.25;
  if (report.marker_context && report.pattern_scan_suspected) risk += 0.20;
  if (world_.pattern_rescan_count > 0) {
    risk += 0.20;
    report.rescan_residual = true;
    report.indicators.push_back("signature re-scan after layout change");
  }
  report.risk_score = std::min(1.0, risk);
  finalize(report);

  world_.note("scan_detector risk=" + std::to_string(report.risk_score) +
              " suspected=" + (report.pattern_scan_suspected ? "1" : "0") +
              " signals=" + std::to_string(report.signal_count));

  return report;
}

ScanDetectionReport ScanDetector::analyze_with_aob(const AobScanReport& aob) {
  auto report = analyze();
  if (aob.found_count > 0 && aob.attempted_scans > 0) {
    report.aob_report_correlated = true;
    report.estimated_patterns_scanned =
        std::max(report.estimated_patterns_scanned, aob.found_count);
    report.indicators.push_back(
        "AOB report correlated found=" + std::to_string(aob.found_count) +
        "/" + std::to_string(aob.attempted_scans));
    report.risk_score = std::min(1.0, report.risk_score + 0.15);
    if (aob.found_count >= 3) {
      report.pattern_scan_suspected = true;
    }
  }
  if (aob.bytes_examined >= (1u << 16)) {
    report.bulk_read_detected = true;
    report.indicators.push_back("AOB report bytes_examined elevated");
    report.risk_score = std::min(1.0, report.risk_score + 0.10);
  }
  finalize(report);
  world_.note("scan_detector+aob risk=" + std::to_string(report.risk_score) +
              " aob_found=" + std::to_string(aob.found_count));
  return report;
}

ScanDetectionReport ScanDetector::analyze_backend(const LabMemoryStats& stats,
                                                  bool pattern_markers_present) {
  ScanDetectionReport report;
  report.bulk_read_detected = stats.bulk_read || stats.read_ops >= 4 ||
                              stats.read_bytes >= (1u << 20);
  report.sequential_access = stats.sequential_burst;
  report.marker_context = pattern_markers_present;

  if (report.bulk_read_detected) {
    report.indicators.push_back("backend bulk/burst reads");
  }
  if (report.sequential_access) {
    report.indicators.push_back("backend sequential burst");
  }
  if (stats.scatter_ops > 0) {
    report.indicators.push_back("backend scatter-read surface");
  }
  if (pattern_markers_present && report.bulk_read_detected) {
    report.indicators.push_back("markers present during backend scan");
  }

  report.pattern_scan_suspected =
      report.bulk_read_detected &&
      (report.sequential_access || pattern_markers_present ||
       stats.scatter_ops > 0);
  report.estimated_patterns_scanned =
      report.pattern_scan_suspected
          ? std::max(1, static_cast<int>(stats.read_ops / 2))
          : 0;

  double risk = 0.0;
  if (report.bulk_read_detected) risk += 0.35;
  if (report.sequential_access) risk += 0.25;
  if (pattern_markers_present) risk += 0.20;
  if (stats.scatter_ops > 0) risk += 0.10;
  report.risk_score = std::min(1.0, risk);
  report.signal_count = static_cast<int>(report.indicators.size());

  std::ostringstream oss;
  oss << (report.pattern_scan_suspected ? "backend AOB scan suspected"
                                        : "backend clean")
      << " risk=" << report.risk_score
      << " read_ops=" << stats.read_ops;
  report.detail = oss.str();
  return report;
}

}  // namespace lab
