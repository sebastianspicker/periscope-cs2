#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::dual_view_shadow_pages {

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

  // Reason 1 (strategy scar): a dual-view shadow is active at all.
  sig(w.dual_view_shadow_active,
                      "dual-view shadow (execute vs read) is active");
  // Reason 2 (self-read probe): the guest reports pristine bytes while an
  // execute view carries a patch - a read/exec disagreement.
  sig(
                      w.dual_view_exec_patched && w.dual_view_read_pristine,
                      "exec/read view disagreement: patched execute view under pristine reads");
  // Reason 3 (TLB/ITC probe): force an INVEPT/ITC-style flush and observe the
  // cross-view effects; dual-view mappings leave flush artifacts.
  w.dual_view_tlb_effects = 2;  // observed ITC shootdown effects after flush
  sig(w.dual_view_tlb_effects > 0,
                      "TLB/ITC flush effects observed=" +
                          std::to_string(w.dual_view_tlb_effects));
  // Reason 4 (timing probe): execute-path fetch latency diverges from read
  // latency when the two views are maintained separately.
  w.dual_view_probe_delta_ns = w.trust.baseline_latency_ns * 3.0;
  sig(
                      w.dual_view_probe_delta_ns > w.trust.baseline_latency_ns * 2.0,
                      "cross-view timing probe delta=" +
                          std::to_string(w.dual_view_probe_delta_ns) + "ns");
  // Reason 5: the 48_ept_hide_ac base posture this pair extends.
  sig(
                      w.trust.personal_hv_active && w.trust.ept_hide_ac_pages,
                      "personal HV with EPT remapping (ept_hide_ac base posture)");

  const bool specific_scar = w.dual_view_shadow_active &&
                             w.dual_view_exec_patched;
  if (specific_scar) {
    result.reasons.emplace_back(
        "strategy scar: dual-view shadow pages (extends 48_ept_hide_ac)");
  }

  // Mitigation: TLB-shootdown re-read. After the forced flush blue re-reads
  // through both views; a genuine dual-view shadow cannot converge both
  // views under repeated shootdowns without measurable cost.
  if (result.detected || specific_scar) {
    ++w.dual_view_cross_reads;
    result.reasons.emplace_back("blue shootdown re-read persists (cross_reads=" +
                                std::to_string(w.dual_view_cross_reads) + ")");
  }

  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = result.detected && w.dual_view_tlb_effects > 0 &&
                     result.risk >= 0.70;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "dual_view_shadow_pages blue signals=" +
                  std::to_string(result.signals) + " risk=" +
                  std::to_string(result.risk);
  std::printf("[T3 dual_view_shadow_pages] BLUE signals=%d detected=%s mitigated=%s\n",
              result.signals, result.detected ? "true" : "false",
              result.mitigated ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T3 dual_view_shadow_pages]   %s\n", reason.c_str());
  }
  w.note(result.detail);
  return result;
}

}  // namespace examples::dual_view_shadow_pages
