#include "blue_example.hpp"
#include <string>

namespace examples::behavioral_decoupler {

namespace {

bool has_behavioral_scar(const sim::World& w) {
  for (const auto& entry : w.log) {
    if (entry.find("behavioral_decoupler:") != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

BlueResult Blue::detect(const sim::World& w) noexcept {
  BlueResult result;

  if (!has_behavioral_scar(w)) {
    result.detail = "no behavioral decoupling signature detected";
    return result;
  }

  for (const auto& entry : w.log) {
    if (entry.find("frames_skipped=") != std::string::npos) {
      result.detected_skip_patterns = true;
      ++result.detection_count;
    }
    if (entry.find("latency=") != std::string::npos) {
      result.detected_latency_consistency = true;
      ++result.detection_count;
    }
    if (entry.find("fuzzed=") != std::string::npos) {
      result.detected_fuzz_pattern = true;
      ++result.detection_count;
    }
    if (entry.find("risk=") != std::string::npos) {
      result.detected_vacnet_risk = true;
      ++result.detection_count;
    }
  }

  result.detail = "behavioral detection: skip=" +
                  std::to_string(result.detected_skip_patterns) +
                  " latency=" + std::to_string(result.detected_latency_consistency) +
                  " fuzz=" + std::to_string(result.detected_fuzz_pattern) +
                  " vacnet=" + std::to_string(result.detected_vacnet_risk) +
                  " count=" + std::to_string(result.detection_count);

  return result;
}

BlueResult Blue::mitigate(sim::World& w) noexcept {
  BlueResult result = detect(w);

  if (result.detection_count > 0) {
    std::vector<std::string> filtered;
    filtered.reserve(w.log.size());
    for (const auto& entry : w.log) {
      if (entry.find("behavioral_decoupler:") == std::string::npos) {
        filtered.push_back(entry);
      }
    }
    w.log.swap(filtered);
    result.detail += " [mitigated: cleared behavioral decoupling scars]";
  }

  return result;
}

} // namespace
