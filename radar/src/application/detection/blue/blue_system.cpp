#include "blue/blue_system.hpp"
#include "blue/blue_system_internal.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>
#include <vector>

namespace blue {
using namespace blue_detail;

const char* view_name(ac::ObservationView view) {
  switch (view) {
    case ac::ObservationView::HandleTable:
      return "HandleTable";
    case ac::ObservationView::ModuleList:
      return "ModuleList";
    case ac::ObservationView::MemoryPattern:
      return "MemoryPattern";
    case ac::ObservationView::InProcess:
      return "InProcess";
    case ac::ObservationView::Behavioral:
      return "Behavioral";
    case ac::ObservationView::PostExecution:
      return "PostExecution";
  }
  return "Unknown";
}

BlueCoordinator::BlueCoordinator(sim::World& world) : world_(world) {
  views_ = {
      {ac::ObservationView::HandleTable, "HandleTable", true, 1.0},
      {ac::ObservationView::ModuleList, "ModuleList", true, 1.0},
      {ac::ObservationView::MemoryPattern, "MemoryPattern", true, 1.0},
      {ac::ObservationView::InProcess, "InProcess", true, 1.0},
      {ac::ObservationView::Behavioral, "Behavioral", true, 1.0},
      {ac::ObservationView::PostExecution, "PostExecution", true, 1.0},
  };
}

BlueViewConfig* BlueCoordinator::find_config(ac::ObservationView view) {
  for (auto& c : views_) {
    if (c.type == view) {
      return &c;
    }
  }
  return nullptr;
}

const BlueViewConfig* BlueCoordinator::find_config(ac::ObservationView view) const {
  for (const auto& c : views_) {
    if (c.type == view) {
      return &c;
    }
  }
  return nullptr;
}

void BlueCoordinator::set_view_enabled(ac::ObservationView view, bool enabled) {
  if (auto* c = find_config(view)) {
    c->enabled = enabled;
  }
}

void BlueCoordinator::set_sensitivity(ac::ObservationView view, double sensitivity) {
  if (auto* c = find_config(view)) {
    c->sensitivity = clamp_sensitivity(sensitivity);
  }
}

bool BlueCoordinator::is_view_enabled(ac::ObservationView view) const {
  const auto* c = find_config(view);
  return c != nullptr && c->enabled;
}

double BlueCoordinator::sensitivity_of(ac::ObservationView view) const {
  const auto* c = find_config(view);
  return c != nullptr ? c->sensitivity : 1.0;
}

double BlueCoordinator::view_sensitivity(ac::ObservationView view) const {
  return sensitivity_of(view);
}

void BlueCoordinator::finalize_view(BlueViewResult& result, double sensitivity,
                                    double base_threshold) {
  const double sens = clamp_sensitivity(sensitivity);
  // Higher sensitivity amplifies weak scars and lowers the fire threshold.
  result.confidence = clamp01(result.confidence * sens);
  const double threshold = base_threshold / sens;
  result.anomaly_detected = result.confidence >= threshold && !result.reasons.empty();
  if (result.reasons.empty() && result.confidence < threshold) {
    result.anomaly_detected = false;
    result.detail = "ok";
    result.confidence = 0.0;
  } else if (!result.reasons.empty()) {
    result.detail = join_reasons(result.reasons);
    // If reasons exist but under threshold, still report detail with no fire.
    if (!result.anomaly_detected) {
      result.detail = "subthreshold: " + result.detail;
    }
  } else {
    result.detail = "ok";
  }
}

BlueAggregatedResult BlueCoordinator::evaluate() {
  BlueAggregatedResult result;

  for (const auto& config : views_) {
    if (!config.enabled) {
      continue;
    }
    auto view_result = evaluate_view(config.type);
    result.per_view.push_back(view_result);
    ++result.total_views_active;

    if (view_result.anomaly_detected) {
      ++result.views_with_anomalies;
      result.views_with_anomalies_list.push_back(config.type);
      result.detection_reasons.push_back(config.name + ": " + view_result.detail);
    }
  }

  result.red_detected = result.views_with_anomalies >= 2;
  result.strong_detection = result.views_with_anomalies >= 3;
  result.overall_confidence = fuse_confidences(result.per_view);

  // Consistency: if multi-view gate fails, do not claim strong overall confidence.
  if (!result.red_detected) {
    result.overall_confidence *= 0.35;
  }

  std::ostringstream oss;
  oss << "Views: " << result.views_with_anomalies << "/" << result.total_views_active
      << " anomalous. Detection: " << (result.red_detected ? "YES" : "NO");
  if (result.strong_detection) {
    oss << " (STRONG)";
  }
  oss << " conf=" << result.overall_confidence;
  result.summary = oss.str();

  return result;
}

BlueViewResult BlueCoordinator::evaluate_view(ac::ObservationView view) {
  switch (view) {
    case ac::ObservationView::HandleTable:
      return check_handle_table();
    case ac::ObservationView::ModuleList:
      return check_module_list();
    case ac::ObservationView::MemoryPattern:
      return check_memory_pattern();
    case ac::ObservationView::InProcess:
      return check_in_process();
    case ac::ObservationView::Behavioral:
      return check_behavioral();
    case ac::ObservationView::PostExecution:
      return check_post_execution();
  }
  BlueViewResult unknown{view, false, 0.0, "unknown view", {}};
  return unknown;
}

// ── HandleTable: foreign VM_READ / proxy / hide-on-enum / donor scars ───────

}  // namespace blue
