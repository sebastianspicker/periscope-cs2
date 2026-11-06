#pragma once

// Educational simulation: multi-sensor Blue coordinator for anti-cheat detection.
// Each ObservationView is an independent World-derived sensor. Actionable detection
// requires corroboration from 2+ anomalous views; 3+ yields strong_detection.
// Run red scars on sim::World first, then BlueCoordinator::evaluate().

#include "ac/types.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace blue {

/// Configuration for a single independent observation view.
struct BlueViewConfig {
  ac::ObservationView type{};
  std::string name;
  bool enabled = true;
  /// Higher sensitivity lowers the anomaly threshold (more FP, fewer FN).
  double sensitivity = 1.0;
};

/// Result from one observation view after evaluating World scars.
struct BlueViewResult {
  ac::ObservationView view{};
  bool anomaly_detected = false;
  double confidence = 0.0;  // 0.0–1.0 after sensitivity scaling
  std::string detail;
  /// Distinct signal tags that contributed (empty when clean).
  std::vector<std::string> reasons;
};

/// Aggregated multi-view detection outcome.
struct BlueAggregatedResult {
  bool red_detected = false;      // True if 2+ enabled views detected anomalies
  bool strong_detection = false;  // True if 3+ enabled views detected anomalies
  int views_with_anomalies = 0;
  int total_views_active = 0;
  std::vector<BlueViewResult> per_view;
  std::vector<std::string> detection_reasons;
  std::vector<ac::ObservationView> views_with_anomalies_list;
  /// Fused confidence over anomalous views (0 when none fire).
  double overall_confidence = 0.0;
  std::string summary;
};

/// Blue coordinator: runs all enabled observation views and aggregates results.
/// Sensors are pure World→verdict functions (no I/O); aggregation is multi-reason.
class BlueCoordinator {
 public:
  explicit BlueCoordinator(sim::World& world);

  /// Enable or disable a declared observation view.
  void set_view_enabled(ac::ObservationView view, bool enabled);

  /// Set per-view sensitivity (clamped to [0.1, 5.0]). Higher → easier anomaly fire.
  void set_sensitivity(ac::ObservationView view, double sensitivity);

  /// Current configuration snapshot (all six declared views).
  const std::vector<BlueViewConfig>& view_configs() const { return views_; }

  /// Whether a view is currently enabled (false if unknown).
  bool is_view_enabled(ac::ObservationView view) const;

  /// Sensitivity for a view (1.0 if unknown).
  double sensitivity_of(ac::ObservationView view) const;

  /// Run all enabled views and aggregate multi-view results.
  BlueAggregatedResult evaluate();

  /// Run a single declared view (applies that view's sensitivity even if disabled).
  BlueViewResult evaluate_view(ac::ObservationView view);

 private:
  sim::World& world_;
  std::vector<BlueViewConfig> views_;

  BlueViewConfig* find_config(ac::ObservationView view);
  const BlueViewConfig* find_config(ac::ObservationView view) const;
  double view_sensitivity(ac::ObservationView view) const;

  /// Apply sensitivity: scale raw confidence and decide anomaly vs threshold.
  static void finalize_view(BlueViewResult& result, double sensitivity,
                            double base_threshold = 0.40);

  BlueViewResult check_handle_table();
  BlueViewResult check_module_list();
  BlueViewResult check_memory_pattern();
  BlueViewResult check_in_process();
  BlueViewResult check_behavioral();
  BlueViewResult check_post_execution();
};

/// Human-readable name for an observation view (stable for tests/logs).
const char* view_name(ac::ObservationView view);

}  // namespace blue
