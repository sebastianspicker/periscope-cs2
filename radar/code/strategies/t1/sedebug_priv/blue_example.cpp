#include "blue_example.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::sedebug_priv {

BlueResult detect(sim::World& w) {
  BlueResult r{};
  const auto game = w.game_pid();

  std::printf("[blue:sedebug_priv] sensor 1: token privilege inventory\n");
  const bool sedebug_hit = w.sedebug_privilege;
  if (sedebug_hit) {
    r.reasons.emplace_back("SeDebugPrivilege is active in the session");
  }

  std::printf("[blue:sedebug_priv] sensor 2: foreign handle corroboration\n");
  bool foreign_handle = false;
  for (const auto& h : w.handles_to(game, true)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* owner = w.proc(h.owner_pid);
    if (owner != nullptr && !owner->is_game && !owner->is_ac) {
      foreign_handle = true;
      break;
    }
  }
  if (foreign_handle) {
    r.reasons.emplace_back("foreign process holds game VM_READ under elevated privilege window");
  }

  std::printf("[blue:sedebug_priv] sensor 3: privilege + attach co-occurrence\n");
  if (sedebug_hit && foreign_handle) {
    r.reasons.emplace_back("SeDebug co-occurs with non-AC game memory handle");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.risk = std::min(1.0, r.signals * 0.28);
  r.detected = r.signals >= 2;
  r.mitigated = r.detected;
  if (r.risk >= 0.8) w.ranked_access_denied = true;
  r.detail = "sedebug=" + std::string(sedebug_hit ? "1" : "0") +
             " handle=" + (foreign_handle ? "1" : "0") +
             " signals=" + std::to_string(r.signals);
  std::printf("[blue:sedebug_priv] %s detected=%d\n", r.detail.c_str(), r.detected);
  return r;
}

}  // namespace examples::sedebug_priv
