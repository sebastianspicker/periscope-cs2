#include "blue_example.hpp"

#include <cstdio>

namespace examples::overwatch_multisignal {

BlueResult detect(sim::World& w) {
  BlueResult r;

  bool foreign_vm = false;
  for (const auto& h : w.handles_to(w.game_pid())) {
    if (sim::has(h.access, sim::AccessMask::VmRead)) {
      const auto* p = w.proc(h.owner_pid);
      if (p && !p->is_game && !p->is_ac) {
        foreign_vm = true;
        break;
      }
    }
  }
  if (foreign_vm) {
    r.reasons.emplace_back("foreign_vm_read_weak_handle");
  }
  if (w.vpn_proxy_active) {
    r.reasons.emplace_back("vpn_proxy_active residual");
  }
  if (w.binary_build_id != "shared" && !w.binary_build_id.empty()) {
    r.reasons.emplace_back("unique_binary_build_id=" + w.binary_build_id);
  }
  if (w.remote_read_ops > 0) {
    r.reasons.emplace_back("remote_read_ops=" + std::to_string(w.remote_read_ops));
  }

  const bool specific_scar = foreign_vm;
  if (specific_scar) {
    r.reasons.emplace_back("strategy scar: multi-weak overwatch bundle");
  }

  r.signals = static_cast<int>(r.reasons.size());
  w.overwatch_score = static_cast<double>(r.signals);
  r.detected = r.signals >= 2 && specific_scar;
  if (r.detected) {
    w.overwatch_queued = true;
    r.mitigated = r.signals >= 3;
    if (r.mitigated) w.ranked_access_denied = true;
  }

  r.detail = "overwatch_multisignal blue score=" + std::to_string(r.signals) +
             " queued=" + std::to_string(w.overwatch_queued ? 1 : 0) +
             " detected=" + std::to_string(r.detected ? 1 : 0) +
             " mitigated=" + std::to_string(r.mitigated ? 1 : 0);
  w.note(r.detail);
  std::printf("[overwatch_multisignal] BLUE signals=%d detected=%s\n", r.signals,
              r.detected ? "true" : "false");
  for (const auto& reason : r.reasons) {
    std::printf("[overwatch_multisignal]   %s\n", reason.c_str());
  }
  return r;
}

}  // namespace examples::overwatch_multisignal
