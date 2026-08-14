#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <algorithm>

namespace examples::dkom_token_steal {
namespace {

bool has_full_access_handle(const sim::World& w) {
  return std::any_of(w.handles.begin(), w.handles.end(), [&](const auto& handle) {
    const auto* owner = w.proc(handle.owner_pid);
    return owner && !owner->is_ac && !owner->is_game &&
           sim::has(handle.access, sim::AccessMask::VmRead) &&
           sim::has(handle.access, sim::AccessMask::VmWrite) &&
           sim::has(handle.access, sim::AccessMask::VmOperation);
  });
}

bool has_non_system_token_recipient(const sim::World& w) {
  return std::any_of(w.processes.begin(), w.processes.end(), [](const auto& entry) {
    const auto& process = entry.second;
    return process.name == "syshelper.exe" && !process.is_ac && !process.is_game;
  });
}

}  // namespace

BlueResult detect(sim::World& w) {
  BlueResult result;
  const bool full_access_handle = has_full_access_handle(w);
  const bool non_system_token_recipient = has_non_system_token_recipient(w);

  support::add_signal(result.signals, result.reasons, w.dkom_token_stolen,
                      "DKOM token-steal scar present");
  support::add_signal(result.signals, result.reasons,
                      w.token_bypasses_handle_acls,
                      "token bypasses handle ACL checks");
  support::add_signal(result.signals, result.reasons, full_access_handle,
                      "non-AC process holds VM read/write/operation handle");
  support::add_signal(result.signals, result.reasons,
                      w.dkom_token_source_pid == 4 && non_system_token_recipient,
                      "SYSTEM token source assigned to a non-SYSTEM process");
  support::add_signal(result.signals, result.reasons, non_system_token_recipient,
                      "syshelper.exe is not an expected SYSTEM token holder");

  result.detected = result.signals >= 2;
  result.risk = support::risk_for(result.signals);
  result.mitigated = result.risk >= 0.75;
  result.detail = "token-anomaly signals=" + std::to_string(result.signals) +
                  " risk=" + std::to_string(result.risk);
  return result;
}

}  // namespace examples::dkom_token_steal