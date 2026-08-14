#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <algorithm>

namespace examples::hypercall_mem_read {

BlueResult detect(sim::World& w) {
  BlueResult result;
  const auto game_pid = w.game_pid();
  const bool handle_exists = std::any_of(
      w.handles.begin(), w.handles.end(), [&](const sim::Handle& handle) {
        return handle.owner_pid != game_pid && handle.target_pid == game_pid &&
               sim::has(handle.access, sim::AccessMask::VmRead);
      });

  support::add_signal(result.signals, result.reasons, w.hypercall_read_active,
                      "hypercall memory-read residual is active");
  if (!w.hypercall_vendor.empty()) {
    ++result.signals;
    result.reasons.emplace_back("hypercall vendor is recorded: " +
                                w.hypercall_vendor);
  }
  support::add_signal(result.signals, result.reasons, w.trust.platform_hv_active,
                      "platform hypervisor is active");
  if (handle_exists) {
    result.reasons.emplace_back("foreign VM_READ handle observed");
  } else {
    result.reasons.emplace_back(
        "no foreign VM_READ handle: hypercall path bypasses handle telemetry");
  }
  if (!w.trust.vbs) {
    ++result.signals;
    result.reasons.emplace_back(
        "VBS disabled: platform hypercalls are more accessible");
  } else {
    result.reasons.emplace_back("VBS enabled: hypercall interface is restricted");
  }

  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2;
  result.mitigated = result.risk >= 0.70;
  result.detail = "hypercall detection requires HV-level monitoring; signals=" +
                  std::to_string(result.signals) +
                  " risk=" + std::to_string(result.risk);
  return result;
}

}  // namespace examples::hypercall_mem_read