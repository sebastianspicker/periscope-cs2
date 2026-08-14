#include "blue_example.hpp"

namespace examples::overwatch_queue {

// BLUE: never invent World scars. Queue on multi-weak residuals only.
BlueResult detect(sim::World& w) {
  BlueResult r;
  bool weak = false;
  for (const auto& h : w.handles_to(w.game_pid())) {
    if (sim::has(h.access, sim::AccessMask::VmRead)) {
      const auto* p = w.proc(h.owner_pid);
      if (p && !p->is_game && !p->is_ac) {
        weak = true;
      }
    }
  }
  // Second weak must already exist on World (vpn or non-shared build).
  const bool second =
      w.vpn_proxy_active ||
      (w.binary_build_id != "shared" && !w.binary_build_id.empty());

  int signals = (weak ? 1 : 0) + (second ? 1 : 0);
  r.signals = signals;
  if (weak) {
    r.reasons.emplace_back("foreign_vm_read_weak_handle");
  }
  if (second) {
    if (w.vpn_proxy_active) r.reasons.emplace_back("vpn_proxy_active residual");
    if (w.binary_build_id != "shared" && !w.binary_build_id.empty()) {
      r.reasons.emplace_back("unique_binary_build_id=" + w.binary_build_id);
    }
  }

  // Multi-reason: full mitigate/queue only when handle + second residual.
  // Mild single-signal: detected=true (observed) but no queue/mitigate.
  if (signals >= 2) {
    r.detected = true;
    r.mitigated = true;
    w.overwatch_queued = true;
    w.overwatch_score = static_cast<double>(signals);
  } else if (weak) {
    r.detected = true;
    r.mitigated = false;
    w.overwatch_score = 1.0;
  } else {
    r.detected = false;
    r.mitigated = false;
  }

  r.detail = "overwatch_queue blue weak_handle=" + std::to_string(weak ? 1 : 0) +
             " second=" + std::to_string(second ? 1 : 0) +
             " signals=" + std::to_string(signals) +
             " build=" + w.binary_build_id +
             " queued=" + std::to_string(w.overwatch_queued ? 1 : 0);
  w.note(r.detail);
  return r;
}

}  // namespace examples::overwatch_queue
