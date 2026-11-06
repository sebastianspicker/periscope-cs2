#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::zero_run_carving {

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

  // Reason 1 (strategy scar): code was carved into existing pages.
  sig(w.zero_run_carve_active,
                      "zero-run carve residual is active (pages=" +
                          std::to_string(w.zero_run_pages_carved) + ")");
  // Reason 2 (the exploitable gap): the VAD-vs-PTE audit. Pages are
  // PTE-valid and now non-zero, yet absent from the VAD manager's
  // reservations - no allocation API ever created them.
  w.zero_run_vad_mismatch_pages = w.zero_run_vad_reserved
                                      ? 0
                                      : w.zero_run_pages_carved;
  sig(w.zero_run_vad_mismatch_pages > 0,
                      "VAD-vs-PTE mismatch: " +
                          std::to_string(w.zero_run_vad_mismatch_pages) +
                          " mapped page(s) with no VAD reservation");
  // Reason 3: no allocation API record backs the new bytes.
  sig(!w.zero_run_allocation_api_used,
                      "new executable bytes with no allocation API record");
  // Reason 4: memory-content delta inside the image without a corresponding
  // module load or section event (no module load or mapping explains it).
  const bool module_event =
      game->manual_mapped_region || game->modules.size() != 2 ||
      std::any_of(game->modules.begin(), game->modules.end(),
                  [](const sim::Module& m) { return !m.linked_in_peb; });
  sig(!module_event,
      "image-content delta unexplained by module/section events");

  const bool specific_scar = w.zero_run_carve_active &&
                             w.zero_run_vad_mismatch_pages > 0;
  if (specific_scar) {
    result.reasons.emplace_back(
        "strategy scar: code placed in zero pages outside the VAD manager");
  }

  // Mitigation: reserve/flag the pages in the VAD manager so the gap closes
  // - any future unreserved placement still shows up in the next audit.
  if (result.detected || specific_scar) {
    w.zero_run_vad_reserved = true;
    result.reasons.emplace_back(
        "blue reserves audited pages in the VAD manager (gap closed for future audits)");
  }

  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = result.detected && result.risk >= 0.65;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "zero_run_carving blue signals=" +
                  std::to_string(result.signals) + " risk=" +
                  std::to_string(result.risk);
  std::printf("[T2 zero_run_carving] BLUE signals=%d detected=%s mitigated=%s\n",
              result.signals, result.detected ? "true" : "false",
              result.mitigated ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T2 zero_run_carving]   %s\n", reason.c_str());
  }
  w.note(result.detail);
  return result;
}

}  // namespace examples::zero_run_carving
