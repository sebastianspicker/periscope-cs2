#include "blue_example.hpp"

#include <algorithm>

namespace examples::physmem_direct_read {

BlueResult detect(sim::World& w) {
  BlueResult result;
  const auto game_pid = w.game_pid();
  const bool has_foreign_vm_read_handle = std::any_of(
      w.handles.begin(), w.handles.end(), [&](const sim::Handle& handle) {
        return handle.owner_pid != game_pid && handle.target_pid == game_pid &&
               sim::has(handle.access, sim::AccessMask::VmRead);
      });

  if (w.physmem_direct_mapped) {
    ++result.signals;
    result.reasons.emplace_back("direct PhysicalMemory mapping scar present");
  }
  if (w.physmem_device_open) {
    ++result.signals;
    result.reasons.emplace_back("PhysicalMemory device access scar present");
  }
  if (has_foreign_vm_read_handle) {
    result.reasons.emplace_back("foreign VM_READ handle observed");
  } else {
    result.reasons.emplace_back(
        "no foreign VM_READ handle: direct physical path bypasses handle telemetry");
  }
  if (!w.trust.dse_enforced) {
    result.reasons.emplace_back("DSE disabled: PhysicalMemory access is exposed");
  }

  result.risk = std::min(1.0, 0.35 * result.signals);
  result.detected = result.signals >= 2;
  result.mitigated =
      (w.trust.dse_enforced &&
       (w.physmem_direct_mapped || w.physmem_device_open)) ||
      result.risk >= 0.7;
  result.detail = "physmem signals=" + std::to_string(result.signals) +
                  " dse_enforced=" +
                  std::string(w.trust.dse_enforced ? "true" : "false");
  return result;
}

}  // namespace examples::physmem_direct_read