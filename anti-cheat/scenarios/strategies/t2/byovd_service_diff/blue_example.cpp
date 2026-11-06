#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::byovd_service_diff {

BlueResult detect(sim::World& w) {
  BlueResult result;
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    result.reasons.emplace_back("sensor precondition failed: game unavailable");
    result.signals = 1;
    return result;
  }

  const auto sig = [&result](bool present, std::string reason) {
    strategies::add_signal(result.signals, result.reasons, present,
                           reason.c_str());
  };

  // Reason 1: the module inventory still contains the driver image.
  bool module_present = false;
  bool module_known_bad = false;
  for (const auto& driver : w.drivers) {
    if (driver.sha256 == "cleanup_memrw_hash") {
      module_present = true;
      module_known_bad = driver.byovd_known_bad;
    }
  }
  sig(module_present, "driver image hash still present in module inventory");
  sig(module_known_bad, "module hash matches the vulnerable-driver catalog");

  // Reason 2: the service database was diffed and lost an entry while the
  // module stayed resident - the cleanup signature.
  sig(w.service_db_diff_count > 0,
      "service DB diff count=" + std::to_string(w.service_db_diff_count));
  sig(w.byovd_service_cleaned && w.loaded_module_hash_orphan,
      "service removed while module hash remains (orphan)");

  // Reason 3: device/IOCTL telemetry from the use window.
  sig(w.escalation_driver_loaded,
      "kernel driver load telemetry present");

  const bool specific_scar =
      module_present && w.loaded_module_hash_orphan && w.service_db_diff_count > 0;
  if (specific_scar) {
    result.reasons.emplace_back(
        "strategy scar: SCM/registry cleaned but loaded-module hash is orphaned");
  }

  // Mitigation: hash the loaded-module list independently of the service DB,
  // and deny ranked access while an orphaned known-bad hash is resident.
  if (result.detected || specific_scar) {
    result.reasons.emplace_back(
        "mitigation: module-hash inventory is authoritative over the service DB");
  }

  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = result.detected && result.risk >= 0.70;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "byovd_service_diff blue signals=" +
                  std::to_string(result.signals) + " risk=" +
                  std::to_string(result.risk);
  std::printf("[T2 byovd_service_diff] BLUE signals=%d detected=%s mitigated=%s\n",
              result.signals, result.detected ? "true" : "false",
              result.mitigated ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T2 byovd_service_diff]   %s\n", reason.c_str());
  }
  w.note(result.detail);
  return result;
}

}  // namespace examples::byovd_service_diff
