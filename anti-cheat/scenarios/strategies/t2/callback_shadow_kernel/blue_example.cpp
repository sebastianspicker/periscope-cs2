#include "blue_example.hpp"
#include <cstdio>

namespace examples::callback_shadow_kernel {

BlueResult detect(sim::World& w) {
  BlueResult result;
  std::size_t pn, in;
  bool ac;
  w.sample_callbacks(pn, in, ac);

  if (pn != w.process_notify_true || in != w.image_notify_true) {
    result.reasons.emplace_back("callback count mismatch: process=" + std::to_string(pn) + " vs true=" + std::to_string(w.process_notify_true));
    result.callback_anomaly = true;
  }
  if (!ac && w.ac_callback_true) result.reasons.emplace_back("AC callback missing from observed set");
  if (w.callback_kernel_shadow_active) {
    result.reasons.emplace_back("kernel callback shadow active");
    result.callback_anomaly = true;
  }
  if (w.callback_kernel_stripped_during_ioctl) result.reasons.emplace_back("callbacks stripped during IOCTL");

  result.signals = static_cast<int>(result.reasons.size());
  // Multi-reason: callback anomaly plus at least one corroborating residual.
  result.detected = result.signals >= 2 && result.callback_anomaly;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T2 callback_shadow_kernel] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::callback_shadow_kernel
