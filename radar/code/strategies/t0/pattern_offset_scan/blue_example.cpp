#include "blue_example.hpp"

#include <cstdio>

namespace examples::pattern_offset_scan {

BlueResult detect(sim::World& w) {
  BlueResult result;
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    result.reasons.emplace_back("sensor precondition failed: game unavailable");
    result.signals = static_cast<int>(result.reasons.size());
    return result;
  }

  int foreign_handles = 0;
  int hidden_handles = 0;
  for (const auto& handle : w.handles_to(game_pid, true)) {
    const auto* owner = w.proc(handle.owner_pid);
    if (owner && !owner->is_game && !owner->is_ac &&
        sim::has(handle.access, sim::AccessMask::VmRead)) {
      ++foreign_handles;
      if (handle.hidden_during_enum || handle.brief_reopen || handle.via_proxy) ++hidden_handles;
    }
  }
  result.foreign_vm_read = foreign_handles > 0;
  result.bulk_remote_reads = w.remote_read_ops >= 4;
  if (result.foreign_vm_read) result.reasons.emplace_back("foreign VM_READ handle count=" + std::to_string(foreign_handles));
  if (result.bulk_remote_reads) result.reasons.emplace_back("bulk remote read telemetry ops=" + std::to_string(w.remote_read_ops));
  if (hidden_handles > 0) result.reasons.emplace_back("handle evasion attribute count=" + std::to_string(hidden_handles));

  result.rescan_residual = w.pattern_rescan_count > 0;
  result.schema_cache_residual = w.schema_cache_active;
  result.schema_remote_residual = w.schema_remote_update || w.schema_fetch_count > 0;
  result.suspicious_cooccurrence = result.foreign_vm_read && result.bulk_remote_reads;
  const bool specific_scar = w.lab_pattern_marker_present && result.rescan_residual && result.schema_cache_residual;
  if (specific_scar) result.reasons.emplace_back("strategy scar: pattern-scan telemetry");
  if (w.ranked_access_denied) result.reasons.emplace_back("ranked policy already denied this session");
  result.signals = static_cast<int>(result.reasons.size());
  result.reason_count = result.signals;
  result.detected = result.signals >= 2;
  if (result.detected) {
    w.ranked_access_denied = true;
    w.server_sends_full_enemy_origin = false;
    result.mitigated = true;
    result.structural_fog = true;
  }
  result.risk = static_cast<double>(result.signals);
  result.detail = result.detected ? "pattern scan telemetry correlated and mitigated"
                                  : "pattern scan telemetry below enforcement threshold";
  std::printf("[T0 pattern_offset_scan] BLUE signals=%d detected=%s\n", result.signals,
              result.detected ? "true" : "false");
  for (const auto& reason : result.reasons) std::printf("[T0 pattern_offset_scan]   %s\n", reason.c_str());
  return result;
}

}  // namespace examples::pattern_offset_scan
