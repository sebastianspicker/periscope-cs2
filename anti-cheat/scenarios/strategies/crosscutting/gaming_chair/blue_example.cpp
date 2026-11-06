#include "blue_example.hpp"

#include <cstdio>

namespace examples::gaming_chair {

BlueResult detect(sim::World& w) {
  BlueResult r;

  if (w.remote_read_ops > 200) {
    r.detected_features.push_back("high_reads");
    r.reasons.emplace_back("high_remote_read_ops=" +
                           std::to_string(w.remote_read_ops));
    r.count++;
  }
  if (w.remote_read_bytes > 1000000) {
    r.detected_features.push_back("high_volume");
    r.reasons.emplace_back("high_remote_read_bytes=" +
                           std::to_string(w.remote_read_bytes));
    r.count++;
  }
  if (w.handle_proxy_active) {
    r.detected_features.push_back("proxy");
    r.reasons.emplace_back("handle_proxy_active");
    r.count++;
  }
  if (w.object_glow_product) {
    r.detected_features.push_back("esp");
    r.reasons.emplace_back("object_glow_product");
    r.count++;
  }
  if (w.schema_cache_active || w.schema_saas_product) {
    r.detected_features.push_back("schema");
    r.reasons.emplace_back("schema_cache_or_saas_product");
    r.count++;
  }
  if (w.triggerbot_active) {
    r.detected_features.push_back("aimbot");
    r.reasons.emplace_back("triggerbot_active");
    r.count++;
  }

  const bool specific_scar =
      w.handle_proxy_active || w.object_glow_product || w.schema_cache_active;
  if (specific_scar) {
    r.reasons.emplace_back("strategy scar: gaming-chair product suite");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.risk = r.count / 3.0;
  r.detected = r.signals >= 2 && specific_scar;
  r.mitigated = r.detected;
  if (r.mitigated) {
    w.ranked_access_denied = true;
    w.handle_proxy_active = false;
    w.object_glow_product = false;
    w.schema_cache_active = false;
  }

  r.detail = "gaming_chair blue features=" + std::to_string(r.count) +
             " signals=" + std::to_string(r.signals) +
             " detected=" + std::to_string(r.detected ? 1 : 0);
  w.note(r.detail);
  std::printf("[gaming_chair] BLUE signals=%d detected=%s\n", r.signals,
              r.detected ? "true" : "false");
  for (const auto& reason : r.reasons) {
    std::printf("[gaming_chair]   %s\n", reason.c_str());
  }
  return r;
}

}  // namespace examples::gaming_chair
