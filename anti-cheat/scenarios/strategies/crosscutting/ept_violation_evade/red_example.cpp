#include "red_example.hpp"

#include <string>
#include <vector>

namespace examples::ept_violation_evade {

RedResult apply(sim::World& w) {
  RedResult result{false, 0, 0, false, ""};
  const auto game = w.game_pid();
  const auto* game_process = w.proc(game);
  if (game == 0 || game_process == nullptr) {
    return {false, result.steps, 0, false, "game process is unavailable"};
  }
  ++result.steps;

  w.trust.vbs = false;
  w.trust.hvci = false;
  w.trust.hvci_enabled = false;
  if (!w.try_start_personal_hv("LabEptTimingCoordinator")) {
    return {false, result.steps, 0, false, "personal HV coordinator was blocked"};
  }
  ++result.steps;

  const auto bridge = w.spawn("ept-timing-bridge-ui.exe");
  if (bridge == 0 || w.proc(bridge) == nullptr) {
    return {false, result.steps, 0, false, "HV bridge actor was unavailable"};
  }
  ++result.steps;

  // Lab-only scars represent EPT access traps, violation-timing scan windows,
  // and the INVEPT cache flush used to force subsequent AC accesses to exit.
  w.ept_sidechannel_active = true;
  w.invept_tlb_flush_used = true;
  w.ept_ac_scan_evasions = 3;
  result.ac_scans_evaded = w.ept_ac_scan_evasions;
  result.used_invept = w.invept_tlb_flush_used;
  ++result.steps;

  std::vector<std::uint8_t> entity_bytes;
  const bool entities_read = w.hv_read(bridge, game, game_process->base, 4,
                                       entity_bytes);
  ++result.steps;

  const bool no_game_handle = w.handles_to(game).empty();
  result.achieved = w.trust.personal_hv_active &&
                    result.ac_scans_evaded == 3 && result.used_invept &&
                    entities_read && !entity_bytes.empty() && no_game_handle;
  result.detail = "EPT timing side channel active=" +
                  std::to_string(w.ept_sidechannel_active) +
                  " evasions=" + std::to_string(result.ac_scans_evaded) +
                  " INVEPT=" + std::to_string(result.used_invept) +
                  " entities=" + std::to_string(entities_read) +
                  " no_game_handle=" + std::to_string(no_game_handle);
  w.note(result.detail);
  return result;
}

}  // namespace examples::ept_violation_evade
