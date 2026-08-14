#pragma once

// blue_system_internal.hpp — shared helpers for BlueCoordinator check_* TUs.

#include "blue/blue_system.hpp"
#include "sim/world.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace blue {
namespace blue_detail {

inline double clamp01(double v) {
  if (v < 0.0) {
    return 0.0;
  }
  if (v > 1.0) {
    return 1.0;
  }
  return v;
}

inline double clamp_sensitivity(double s) {
  if (s < 0.1) {
    return 0.1;
  }
  if (s > 5.0) {
    return 5.0;
  }
  return s;
}

inline void add_reason(BlueViewResult& r, std::string tag, double weight) {
  if (weight <= 0.0) {
    return;
  }
  r.reasons.push_back(std::move(tag));
  r.confidence = clamp01(r.confidence + weight);
}

inline std::string join_reasons(const std::vector<std::string>& reasons) {
  if (reasons.empty()) {
    return "ok";
  }
  std::ostringstream oss;
  for (std::size_t i = 0; i < reasons.size(); ++i) {
    if (i > 0) {
      oss << "; ";
    }
    oss << reasons[i];
  }
  return oss.str();
}

// Soft-OR fusion: 1 - product(1 - c_i) over anomalous confidences.
inline double fuse_confidences(const std::vector<BlueViewResult>& views) {
  double remain = 1.0;
  int n = 0;
  for (const auto& v : views) {
    if (!v.anomaly_detected) {
      continue;
    }
    remain *= (1.0 - clamp01(v.confidence));
    ++n;
  }
  if (n == 0) {
    return 0.0;
  }
  return clamp01(1.0 - remain);
}

}  // namespace blue_detail
}  // namespace blue
