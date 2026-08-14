#include "blue_example.hpp"

#include <cstdio>

namespace examples::handle_hijack_proxy {

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
  bool consumer_has_handle = false;
  for (const auto& handle : w.handles_to(game_pid, true)) {
    const auto* owner = w.proc(handle.owner_pid);
    if (owner && !owner->is_game && !owner->is_ac &&
        sim::has(handle.access, sim::AccessMask::VmRead)) {
      ++foreign_handles;
      if (handle.hidden_during_enum || handle.brief_reopen || handle.via_proxy) ++hidden_handles;
    }
  }
  if (foreign_handles > 0) result.reasons.emplace_back("foreign VM_READ handle count=" + std::to_string(foreign_handles));
  if (w.remote_read_ops > 0 || w.remote_read_bytes > 0) result.reasons.emplace_back("remote read telemetry ops=" + std::to_string(w.remote_read_ops));
  if (hidden_handles > 0) result.reasons.emplace_back("handle evasion attribute count=" + std::to_string(hidden_handles));

  const bool specific_scar = w.handle_proxy_active &&
      w.handle_proxy_owner_pid != 0 && w.handle_proxy_consumer_pid != 0;
  for (const auto& handle : w.handles_to(game_pid, true)) {
    if (handle.owner_pid == w.handle_proxy_owner_pid &&
        sim::has(handle.access, sim::AccessMask::VmRead)) {
      result.proxy_handle = true;
    }
    if (handle.owner_pid == w.handle_proxy_consumer_pid &&
        sim::has(handle.access, sim::AccessMask::VmRead)) {
      consumer_has_handle = true;
    }
  }
  result.consumer_no_handle = !consumer_has_handle;
  if (specific_scar) result.reasons.emplace_back("strategy scar: proxy-owned VM_READ edge");
  if (w.ranked_access_denied) result.reasons.emplace_back("ranked policy already denied this session");
  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && specific_scar && result.proxy_handle && result.consumer_no_handle;
  if (result.detected) {
    w.ranked_access_denied = true;
    result.mitigated = true;
  }
  std::printf("[T0 handle_hijack_proxy] BLUE signals=%d detected=%s\n", result.signals,
              result.detected ? "true" : "false");
  for (const auto& reason : result.reasons) std::printf("[T0 handle_hijack_proxy]   %s\n", reason.c_str());
  return result;
}

}  // namespace examples::handle_hijack_proxy
