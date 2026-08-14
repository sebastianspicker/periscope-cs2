#include "blue_example.hpp"

#include <cstdio>
#include <sstream>

namespace examples::fov_viewmodel_mod {

BlueResult detect(sim::World& w) {
  BlueResult r;

  const bool fov = w.fov_mod_active && w.client_fov_override > 90.f;
  const bool vm = w.viewmodel_fov_override > 54.f;

  // Independent reasons — either knob alone is weak; multi-reason raises bar.
  if (w.fov_mod_active) {
    r.reasons.emplace_back("fov_mod_session_flag");
  }
  if (w.client_fov_override > 90.f) {
    r.reasons.emplace_back("client_fov_override=" +
                           std::to_string(w.client_fov_override));
  }
  if (vm) {
    r.reasons.emplace_back("viewmodel_fov_override=" +
                           std::to_string(w.viewmodel_fov_override));
  }

  const bool specific_scar = fov || vm;
  if (specific_scar) {
    r.reasons.emplace_back("strategy scar: presentation FOV/viewmodel residual");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.detected = r.signals >= 2 && specific_scar;
  // Mitigate only when both presentation knobs fire (weak alone).
  r.mitigated = r.detected && fov && vm;
  if (r.mitigated) {
    w.client_fov_override = 0.f;
    w.viewmodel_fov_override = 0.f;
    w.fov_mod_active = false;
    w.ranked_access_denied = true;
  }

  std::ostringstream oss;
  oss << "fov_viewmodel_mod blue fov=" << (fov ? 1 : 0)
      << " viewmodel=" << (vm ? 1 : 0)
      << " signals=" << r.signals
      << " detected=" << (r.detected ? 1 : 0)
      << " mitigated=" << (r.mitigated ? 1 : 0);
  r.detail = oss.str();
  w.note(r.detail);
  std::printf("[fov_viewmodel_mod] BLUE signals=%d detected=%s\n", r.signals,
              r.detected ? "true" : "false");
  for (const auto& reason : r.reasons) {
    std::printf("[fov_viewmodel_mod]   %s\n", reason.c_str());
  }
  return r;
}

}  // namespace examples::fov_viewmodel_mod
