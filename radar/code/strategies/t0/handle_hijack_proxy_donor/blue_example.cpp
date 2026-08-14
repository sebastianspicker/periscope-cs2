#include "blue_example.hpp"
#include <cstdio>

namespace examples::handle_hijack_proxy_donor {

BlueResult detect(sim::World& w) {
  BlueResult result;
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    result.reasons.emplace_back("sensor precondition failed: game unavailable");
    result.signals = 1;
    return result;
  }

  int foreign_handles = 0;
  int hidden_handles = 0;
  bool donor_handle = false;
  bool consumer_handle = false;

  for (const auto& h : w.handles_to(game_pid, true)) {
    const auto* owner = w.proc(h.owner_pid);
    if (owner && !owner->is_game && !owner->is_ac && sim::has(h.access, sim::AccessMask::VmRead)) {
      ++foreign_handles;
      if (h.hidden_during_enum || h.brief_reopen || h.via_proxy) ++hidden_handles;
      if (w.donor_hijack_donor_pid != 0 && h.owner_pid == w.donor_hijack_donor_pid) donor_handle = true;
      if (w.donor_hijack_consumer_pid != 0 && h.owner_pid == w.donor_hijack_consumer_pid) consumer_handle = true;
    }
  }

  if (foreign_handles > 0) result.reasons.emplace_back("foreign VM_READ handle count=" + std::to_string(foreign_handles));
  if (hidden_handles > 0) result.reasons.emplace_back("handle evasion count=" + std::to_string(hidden_handles));
  if (w.remote_read_ops > 0) result.reasons.emplace_back("remote read ops=" + std::to_string(w.remote_read_ops));

  result.donor_handle_found = donor_handle;
  result.consumer_has_no_handle = !consumer_handle;

  if (w.donor_hijack_active && w.donor_handle_duplicated) {
    result.reasons.emplace_back("strategy scar: donor hijack active with duplicated handle");
    if (donor_handle) result.reasons.emplace_back("donor owns VmRead handle");
    if (!consumer_handle) result.reasons.emplace_back("consumer has no direct handle (proxy pattern)");
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && w.donor_hijack_active && donor_handle && !consumer_handle;
  if (result.detected) {
    w.ranked_access_denied = true;
    result.mitigated = true;
  }
  std::printf("[T0 handle_hijack_proxy_donor] BLUE signals=%d detected=%s\n", result.signals, result.detected ? "true" : "false");
  for (const auto& r : result.reasons) std::printf("[T0 handle_hijack_proxy_donor]   %s\n", r.c_str());
  return result;
}

}  // namespace examples::handle_hijack_proxy_donor
