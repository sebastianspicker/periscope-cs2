#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <algorithm>

namespace examples::acpi_pm_mem_read {

BlueResult detect(sim::World& w) {
  BlueResult result;
  const auto game_pid = w.game_pid();
  const bool handle_exists = std::any_of(
      w.handles.begin(), w.handles.end(), [&](const sim::Handle& handle) {
        return handle.owner_pid != game_pid && handle.target_pid == game_pid &&
               sim::has(handle.access, sim::AccessMask::VmRead);
      });

  support::add_signal(result.signals, result.reasons, w.acpi_pm_read_active,
                      "ACPI PM memory-read residual is active");
  support::add_signal(result.signals, result.reasons,
                      w.acpi_smi_trigger_count > 0,
                      "ACPI PM-triggered SMI activity is present");
  support::add_signal(result.signals, result.reasons, w.smm_residual,
                      "SMM residual requires platform-firmware inspection");
  if (handle_exists) {
    result.reasons.emplace_back("foreign VM_READ handle observed");
  } else {
    result.reasons.emplace_back(
        "no foreign VM_READ handle: SMM bypasses OS handle telemetry");
  }
  if (w.trust.dse_enforced) {
    result.reasons.emplace_back(
        "DSE enforced: PhysicalMemory device access is blocked");
  } else {
    result.reasons.emplace_back(
        "DSE disabled: PhysicalMemory access is additionally exposed");
  }

  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2;
  result.mitigated = result.risk >= 0.75;
  result.detail = "ACPI PM detection requires firmware monitoring; signals=" +
                  std::to_string(result.signals) +
                  " risk=" + std::to_string(result.risk);
  return result;
}

}  // namespace examples::acpi_pm_mem_read