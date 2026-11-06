#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::cpuid_hypercall_channel {

BlueResult detect(sim::World& w) {
  BlueResult result;
  // Signal shim: the shared add_signal takes const char*; wrap for std::string reasons.
  const auto sig = [&result](bool present, std::string reason) {
    strategies::add_signal(result.signals, result.reasons, present, reason.c_str());
  };
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);

  // Reason 1 (strategy scar): the CPUID-magic hypercall channel is active.
  sig(w.cpuid_hypercall_channel_active,
                      "CPUID-magic-key hypercall covert channel is active");
  // Reason 2: abnormal CPUID leaf probing (real leaf ranges + magic probe).
  sig(w.cpuid_leaf_probes >= 3,
                      "abnormal CPUID leaf probing count=" +
                          std::to_string(w.cpuid_leaf_probes));
  // Reason 3: the channel is unauthenticated - any local process knowing the
  // magic leaf can read protected data, not just the game or the AC agent.
  const auto* consumer = w.proc(w.cpuid_channel_consumer_pid);
  const bool foreign_consumer =
      consumer != nullptr && !consumer->is_game && !consumer->is_ac;
  support::add_signal(result.signals, result.reasons,
                      w.cpuid_channel_unauthenticated && foreign_consumer,
                      "unauthenticated channel use by a foreign local process");
  // Reason 4: the handle graph stayed clean - channel traffic is invisible to
  // handle telemetry, which is itself corroboration for a covert path.
  bool any_foreign_vm_read = false;
  if (game) {
    for (const auto& handle : w.handles_to(game_pid, true)) {
      const auto* owner = w.proc(handle.owner_pid);
      if (owner && !owner->is_game && !owner->is_ac &&
          sim::has(handle.access, sim::AccessMask::VmRead)) {
        any_foreign_vm_read = true;
      }
    }
  }
  sig(!any_foreign_vm_read,
                      "no foreign VM_READ handle: channel bypasses handle telemetry");
  // Reason 5: platform hypervisor posture that makes the interface reachable.
  sig(w.trust.platform_hv_active,
                      "platform hypervisor exposes the hypercall interface");

  const bool specific_scar = w.cpuid_hypercall_channel_active &&
                             w.hypercall_channel_reads > 0;
  if (specific_scar) {
    result.reasons.emplace_back("strategy scar: hypercall channel reads=" +
                                std::to_string(w.hypercall_channel_reads));
  }

  // Mitigation: require channel authentication. Blue invalidates the
  // unauthenticated magic so only vetted callers can re-arm the leaf.
  if (result.detected || specific_scar) {
    w.cpuid_channel_unauthenticated = false;
    result.reasons.emplace_back(
        "blue requires authenticated hypercall channel use (magic invalidated)");
  }

  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = result.detected && !w.cpuid_channel_unauthenticated &&
                     result.risk >= 0.70;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "cpuid channel blue signals=" + std::to_string(result.signals) +
                  " risk=" + std::to_string(result.risk);
  std::printf("[T3 cpuid_hypercall_channel] BLUE signals=%d detected=%s mitigated=%s\n",
              result.signals, result.detected ? "true" : "false",
              result.mitigated ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T3 cpuid_hypercall_channel]   %s\n", reason.c_str());
  }
  w.note(result.detail);
  return result;
}

}  // namespace examples::cpuid_hypercall_channel
