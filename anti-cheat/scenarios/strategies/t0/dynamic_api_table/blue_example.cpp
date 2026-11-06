#include "blue_example.hpp"
#include <cstdio>

namespace examples::dynamic_api_table {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.dynamic_import_resolution) {
    result.reasons.emplace_back("dynamic import resolution active");
    result.dynamic_imports_detected = true;
  }
  if (w.dynamic_import_count > 0) result.reasons.emplace_back("APIs resolved dynamically=" + std::to_string(w.dynamic_import_count));

  for (const auto& [pid, proc] : w.processes) {
    for (const auto& m : proc.modules) {
      if (m.iat_hooked) result.reasons.emplace_back("IAT hook detected in " + m.name);
      if (m.eat_hooked) result.reasons.emplace_back("EAT hook detected in " + m.name);
    }
  }

  result.signals = static_cast<int>(result.reasons.size());
  // Multi-reason: dynamic import path plus at least one independent residual.
  result.detected = result.signals >= 2 && result.dynamic_imports_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T0 dynamic_api_table] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::dynamic_api_table
