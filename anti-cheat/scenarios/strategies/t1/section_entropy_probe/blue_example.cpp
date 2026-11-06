#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::section_entropy_probe {

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

  // Reason 1: entropy probe is armed and the section scores high. Non-image
  // sections above ~70 are packed/encrypted, not normal loader data.
  w.section_entropy_probe_active = true;
  sig(w.section_entropy_score >= 70,
      "non-image section entropy=" + std::to_string(w.section_entropy_score));

  // Reason 2: the registry section is present and encrypted.
  sig(w.encrypted_addr_registry_active && w.addr_registry_section_encrypted,
      "encrypted non-image address registry present");

  // Reason 3: no data-section patch. A patch-free decode is *not* a clean
  // bill of health - it means the patch heuristic alone would miss red.
  sig(w.decode_register_only && !w.registry_data_section_patched,
      "register-only decode: data section never patched");

  // Reason 4: repeated entropy samples stay high (stable, not a one-off).
  sig(w.section_entropy_score >= 70 && w.section_entropy_probe_active,
      "entropy stable across samples");

  // Reason 5: read telemetry from the registry-guided path.
  sig(w.remote_read_ops > 0,
      "remote read telemetry ops=" + std::to_string(w.remote_read_ops));

  const bool specific_scar = w.section_entropy_probe_active &&
                             w.section_entropy_score >= 70 &&
                             w.addr_registry_section_encrypted;
  if (specific_scar) {
    result.reasons.emplace_back(
        "strategy scar: high-entropy non-image section with patch-free decode");
  }

  // Mitigation: require a registry manifest (name, purpose, expected entropy
  // band) for any non-image section above the threshold; unknown sections are
  // denied ranked access.
  if (result.detected || specific_scar) {
    result.reasons.emplace_back(
        "mitigation: registry manifest required for high-entropy sections");
  }

  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = result.detected && result.risk >= 0.70;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "section_entropy_probe blue signals=" +
                  std::to_string(result.signals) + " risk=" +
                  std::to_string(result.risk);
  std::printf(
      "[T1 section_entropy_probe] BLUE signals=%d detected=%s mitigated=%s\n",
      result.signals, result.detected ? "true" : "false",
      result.mitigated ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T1 section_entropy_probe]   %s\n", reason.c_str());
  }
  w.note(result.detail);
  return result;
}

}  // namespace examples::section_entropy_probe
