#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::handle_rights_strip {

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

  // Reason 1 (strategy scar): kernel attach-context reads are active.
  sig(w.kernel_attach_read_active,
                      "kernel attach-context read residual is active");
  // Reason 2: attach read telemetry without any handle edge.
  sig(w.kernel_attach_read_ops > 0,
                      "attach-context read ops=" + std::to_string(w.kernel_attach_read_ops));
  bool any_foreign_vm_read = false;
  for (const auto& handle : w.handles_to(game_pid, true)) {
    const auto* owner = w.proc(handle.owner_pid);
    if (owner && !owner->is_game && !owner->is_ac &&
        sim::has(handle.access, sim::AccessMask::VmRead)) {
      any_foreign_vm_read = true;
    }
  }
  sig(!any_foreign_vm_read,
                      "reads occurred with no foreign VM_READ handle: attach bypass");
  // Reason 3: corroborating inventory (untrusted kernel surface).
  if (support::has_untrusted_memrw_driver(w)) {
    sig(true, "untrusted mem-rw driver could host the attach path");
  }

  const bool specific_scar = w.kernel_attach_read_active && w.kernel_attach_read_ops > 0;
  if (specific_scar) {
    result.reasons.emplace_back("strategy scar: attach read bypasses handle rights");
  }

  // Mitigation: ObRegisterCallbacks-style pre-operation strip. Any future
  // VM_READ/VmWrite open against the protected pid is downgraded to Query.
  if (result.detected || specific_scar) {
    w.ob_handle_rights_strip_active = true;
    w.rights_strip_target_pid = game_pid;
    for (const auto& handle : w.handles_to(game_pid, true)) {
      if (sim::has(handle.access, sim::AccessMask::VmRead) ||
          sim::has(handle.access, sim::AccessMask::VmWrite)) {
        ++w.rights_strip_downgrades;
      }
    }
    result.reasons.emplace_back(
        "handle-rights strip armed on protected pid (downgrades=" +
        std::to_string(w.rights_strip_downgrades) + ")");
  }

  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = result.detected && w.ob_handle_rights_strip_active &&
                     result.risk >= 0.65;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "handle_rights_strip blue signals=" +
                  std::to_string(result.signals) + " risk=" +
                  std::to_string(result.risk);
  std::printf("[T2 handle_rights_strip] BLUE signals=%d detected=%s mitigated=%s\n",
              result.signals, result.detected ? "true" : "false",
              result.mitigated ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T2 handle_rights_strip]   %s\n", reason.c_str());
  }
  w.note(result.detail);
  return result;
}

}  // namespace examples::handle_rights_strip
