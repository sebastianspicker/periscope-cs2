#include "blue_example.hpp"

#include <cstdio>

namespace strategy::t1_etw_dual_provider {

BlueResult Blue::detect(const sim::World& w) noexcept {
  BlueResult r;
  const auto game = w.game_pid();

  std::printf("[blue:etw_dual_provider] sensor 1: primary TI health\n");
  r.ti_down = w.etw_ti_blind || !w.etw_enabled;
  if (r.ti_down) {
    r.reasons.emplace_back("primary_TI_blinded");
  }

  std::printf("[blue:etw_dual_provider] sensor 2: secondary ETW provider path\n");
  r.secondary_detected = w.etw_secondary_active || r.ti_down;
  if (r.secondary_detected) {
    r.reasons.emplace_back("secondary_provider_path");
  }

  bool foreign_handle = false;
  for (const auto& h : w.handles_to(game, true)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* owner = w.proc(h.owner_pid);
    if (owner != nullptr && !owner->is_game && !owner->is_ac) {
      foreign_handle = true;
      r.reasons.emplace_back("foreign_VM_READ_handle");
      break;
    }
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.detected = r.signals >= 2 && r.ti_down;
  r.detail = r.reasons.empty() ? "no_dual_provider_signals" : "";
  for (const auto& reason : r.reasons) r.detail += reason + " ";

  std::printf("[blue:etw_dual_provider] ti_down=%d secondary=%d handle=%d signals=%d | %s\n",
              r.ti_down ? 1 : 0, r.secondary_detected ? 1 : 0,
              foreign_handle ? 1 : 0, r.signals, r.detail.c_str());
  return r;
}

BlueResult Blue::mitigate(sim::World& w) noexcept {
  BlueResult r = detect(w);
  w.etw_secondary_active = true;
  if (r.detected) {
    w.ranked_access_denied = true;
    r.mitigated = true;
  }
  w.note("etw_dual_provider: secondary provider maintained; dual-path residual logged");
  std::printf("[blue:etw_dual_provider] mitigate: secondary_active=1\n");
  return r;
}

}  // namespace strategy::t1_etw_dual_provider
