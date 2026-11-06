#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::escalation_cost_curve {

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

  // Footprint 1 (strategy scar): the ladder itself was climbed to the kernel
  // rung - a non-AC memrw driver is loaded.
  const bool kernel_driver = support::has_untrusted_memrw_driver(w);
  sig(kernel_driver,
                      "non-AC memrw driver loaded (kernel rung reached)");
  // Footprint 2: testsigning flag flipped to load it.
  sig(w.trust.test_signing,
                      "testsigning flag enabled (persistent posture mutation)");
  // Footprint 3: HVCI toggled off.
  sig(!w.trust.hvci_enabled,
                      "HVCI disabled (persistent posture mutation)");
  // Footprint 4: blocklist/registry toggles persisted.
  support::add_signal(result.signals, result.reasons,
                      w.blocklist_registry_toggled && w.registry_persisted_config,
                      "vulnerable-driver blocklist / CI registry toggles persisted");
  // Footprint 5: pending reboot - the toggles have not even applied yet.
  sig(w.pending_reboot_required,
                      "pending reboot required from posture mutations");
  // Footprint 6: the lower rungs left handle scars of their own.
  int syscall_edges = 0;
  int plain_edges = 0;
  for (const auto& handle : w.handles_to(game_pid, true)) {
    const auto* owner = w.proc(handle.owner_pid);
    if (!owner || owner->is_game || owner->is_ac ||
        !sim::has(handle.access, sim::AccessMask::VmRead)) {
      continue;
    }
    if (handle.via_syscall_path) {
      ++syscall_edges;
    } else {
      ++plain_edges;
    }
  }
  sig(syscall_edges + plain_edges >= 2,
                      "lower-rung handle scars: " + std::to_string(plain_edges) +
                          " winapi + " + std::to_string(syscall_edges) + " syscall edges");

  const bool specific_scar = w.escalation_ladder_used && kernel_driver;
  if (specific_scar) {
    result.reasons.emplace_back("strategy scar: escalation ladder walked to the kernel rung (" +
                                std::to_string(w.escalation_footprints) + " footprints)");
  }

  // Pricing: the escalation-cost curve - each rung costs more than it is
  // worth once the footprints are visible. At high footprint risk blue
  // denies ranked play outright.
  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = result.detected && w.escalation_footprints >= 4 &&
                     result.risk >= 0.70;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "escalation_cost_curve blue signals=" +
                  std::to_string(result.signals) + " footprints=" +
                  std::to_string(w.escalation_footprints) + " risk=" +
                  std::to_string(result.risk);
  std::printf("[T2 escalation_cost_curve] BLUE signals=%d detected=%s mitigated=%s\n",
              result.signals, result.detected ? "true" : "false",
              result.mitigated ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T2 escalation_cost_curve]   %s\n", reason.c_str());
  }
  w.note(result.detail);
  return result;
}

}  // namespace examples::escalation_cost_curve
