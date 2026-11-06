#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::eprocess_derivation {

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

  // Reason 1 (strategy scar): runtime structural derivation is active.
  sig(w.structural_derivation_active,
                      "runtime structural offset derivation is active");
  // Reason 2: the derivation probe pattern leaves a burst of invariant-probe
  // read scars (list walk + DTB probe + Ldr walk, repeated per pass).
  sig(w.derivation_probe_reads >= 6,
                      "derivation probe read scars count=" +
                          std::to_string(w.derivation_probe_reads));
  // Reason 3: version-churn pressure - red re-derived after a layout change,
  // so offsets are never shipped and blue must re-baseline on every churn.
  sig(w.derivation_churn_rescans > 0,
                      "version churn pressure: re-derivations=" +
                          std::to_string(w.derivation_churn_rescans) +
                          " layout generation=" + std::to_string(w.lab_pattern_generation));
  // Reason 4: generic inventory corroboration (kernel foothold needed to
  // walk EPROCESS-style lists outside the game).
  if (support::has_untrusted_memrw_driver(w)) {
    sig(true, "untrusted mem-rw driver could host the derivation probes");
  }

  const bool specific_scar = w.structural_derivation_active &&
                             w.derivation_probe_reads > 0;
  if (specific_scar) {
    result.reasons.emplace_back("strategy scar: derivation probe pattern observed");
  }

  // Mitigation: continuous re-validation. Blue re-baselines its own layout
  // knowledge on every churn (pattern_rescan_count) so red's runtime
  // derivation no longer outpaces blue's stale offsets.
  if (result.detected || specific_scar) {
    ++w.pattern_rescan_count;
    result.reasons.emplace_back("blue re-baselines layout on churn (rescans=" +
                                std::to_string(w.pattern_rescan_count) + ")");
  }

  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = result.detected && result.risk >= 0.65;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "eprocess_derivation blue signals=" +
                  std::to_string(result.signals) + " risk=" +
                  std::to_string(result.risk);
  std::printf("[T1 eprocess_derivation] BLUE signals=%d detected=%s mitigated=%s\n",
              result.signals, result.detected ? "true" : "false",
              result.mitigated ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T1 eprocess_derivation]   %s\n", reason.c_str());
  }
  w.note(result.detail);
  return result;
}

}  // namespace examples::eprocess_derivation
