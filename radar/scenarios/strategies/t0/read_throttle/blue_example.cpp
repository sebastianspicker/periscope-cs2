#include "blue_example.hpp"

#include <cstdio>

namespace examples::read_throttle {

BlueResult detect(sim::World& w) {
  BlueResult result{false, 0, {}};
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
  if (foreign_handles > 0) result.reasons.emplace_back("foreign VM_READ handle count=" + std::to_string(foreign_handles));
  if (w.remote_read_ops > 0 || w.remote_read_bytes > 0) result.reasons.emplace_back("remote read telemetry ops=" + std::to_string(w.remote_read_ops));
  if (hidden_handles > 0) result.reasons.emplace_back("handle evasion attribute count=" + std::to_string(hidden_handles));

  const bool specific_scar = hidden_handles > 0;
  if (specific_scar) result.reasons.emplace_back("strategy scar: brief, throttled remote read");
  if (w.ranked_access_denied) result.reasons.emplace_back("ranked policy already denied this session");
  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && specific_scar;
  if (result.detected && result.signals >= 3) w.ranked_access_denied = true;
  std::printf("[T0 read_throttle] BLUE signals=%d detected=%s\n", result.signals,
              result.detected ? "true" : "false");
  for (const auto& reason : result.reasons) std::printf("[T0 read_throttle]   %s\n", reason.c_str());
  return result;
}

}  // namespace examples::read_throttle
