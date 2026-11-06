#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::loader_addr_registry {

BlueResult detect(sim::World& w) {
  BlueResult result;
  // Signal shim: the shared add_signal takes const char*; wrap for std::string reasons.
  const auto sig = [&result](bool present, std::string reason) {
    strategies::add_signal(result.signals, result.reasons, present, reason.c_str());
  };
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    result.reasons.emplace_back("sensor precondition failed: game unavailable");
    result.signals = static_cast<int>(result.reasons.size());
    return result;
  }

  // Reason 1 (strategy scar): a data section carries decoded pointers that
  // were not there at image load - decode-time writes patch the section.
  sig(w.registry_data_section_patched,
                      "data section patched at decode time (decoded pointer table)");
  // Reason 2: decode-time access burst - pointer-table accesses cluster in a
  // short window (registry_decode_ops), unlike steady gameplay reads.
  sig(w.registry_decode_ops >= 3,
                      "decode-time access burst ops=" + std::to_string(w.registry_decode_ops));
  // Reason 3: an encrypted registry section is present - packed/encrypted
  // non-image data that decrypts only at runtime is a loader artifact.
  support::add_signal(result.signals, result.reasons,
                      w.encrypted_addr_registry_active && w.addr_registry_section_encrypted,
                      "encrypted address registry section present");
  // Reason 4: drift-recovery telemetry - the loader re-resolves after layout
  // churn, the same remote-update shape schema C2 traffic shows.
  sig(w.registry_drift_recoveries > 0,
                      "offset-drift re-resolve after churn (recoveries=" +
                          std::to_string(w.registry_drift_recoveries) + ")");
  // Reason 5: generic remote-read telemetry from the registry-guided reads.
  sig(w.remote_read_ops > 0,
                      "remote read telemetry ops=" + std::to_string(w.remote_read_ops));

  const bool specific_scar = w.encrypted_addr_registry_active &&
                             (w.registry_data_section_patched || w.registry_decode_ops > 0);
  if (specific_scar) {
    result.reasons.emplace_back(
        "strategy scar: loader-resolved encrypted address registry");
  }

  // Mitigation: randomized decode-window re-checks. Blue samples the data
  // section at random times so the decoded-pointer window cannot stay hidden,
  // and requires a registry manifest for any encrypted non-image section.
  if (result.detected || specific_scar) {
    ++w.pattern_rescan_count;
    result.reasons.emplace_back("blue randomized decode-window re-checks (rescans=" +
                                std::to_string(w.pattern_rescan_count) + ")");
  }

  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = result.detected && result.risk >= 0.70;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "loader_addr_registry blue signals=" +
                  std::to_string(result.signals) + " risk=" +
                  std::to_string(result.risk);
  std::printf("[T1 loader_addr_registry] BLUE signals=%d detected=%s mitigated=%s\n",
              result.signals, result.detected ? "true" : "false",
              result.mitigated ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T1 loader_addr_registry]   %s\n", reason.c_str());
  }
  w.note(result.detail);
  return result;
}

}  // namespace examples::loader_addr_registry
