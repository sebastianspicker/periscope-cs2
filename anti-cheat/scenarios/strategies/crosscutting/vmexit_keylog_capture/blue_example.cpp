#include "blue_example.hpp"

#include <algorithm>
#include <utility>

namespace examples::vmexit_keylog_capture {
namespace {

void add_signal(BlueResult& result, std::string reason, double weight) {
  ++result.signals;
  result.risk += weight;
  result.reasons.push_back(std::move(reason));
}

}  // namespace

BlueResult detect(sim::World& w) {
  BlueResult result{false, false, 0, 0.0, {}, ""};
  const bool personal_hv = w.trust.personal_hv_active;
  const bool bridge_device = std::any_of(
      w.devices.begin(), w.devices.end(), [](const sim::Device& device) {
        return device.name == "\\Device\\VmExitBridge" &&
               device.owner_driver == "vmexitbridge.sys";
      });
  const bool bridge_driver = std::any_of(
      w.drivers.begin(), w.drivers.end(), [](const sim::Driver& driver) {
        return driver.name == "vmexitbridge.sys" && driver.is_bridge;
      });
  const bool bridge = bridge_device && bridge_driver;
  const bool no_game_handle = w.handles_to(w.game_pid()).empty();

  if (w.vmexit_keylog_active) {
    add_signal(result, "VM-exit keylog residual is present", 0.55);
  }
  if (w.vmexit_keys_captured > 0) {
    add_signal(result, "VM-exit keystroke capture count is nonzero", 0.15);
  }
  if (personal_hv) {
    add_signal(result, "personal HV coordinator is active", 0.10);
  }
  if (!w.trust.vbs || !w.trust.hvci || !w.trust.hvci_enabled) {
    add_signal(result, "VBS/HVCI trust policy is disabled", 0.10);
  }
  if (no_game_handle) {
    add_signal(result, "capture path requires no game-process handle", 0.05);
  }
  if (bridge) {
    add_signal(result, "bridge driver/device provides an exfiltration path", 0.15);
  }

  result.risk = std::min(result.risk, 1.0);
  result.detected = result.signals >= 2;
  // VM-exit interception leaves no guest hooks, callbacks, or game driver.
  result.mitigated = result.risk >= 0.75;
  if (result.mitigated) {
    w.ranked_access_denied = true;
  }
  result.detail = "VM-exit keylog detection is guest-visibility limited; signals=" +
                  std::to_string(result.signals) +
                  " risk=" + std::to_string(result.risk);
  w.note(result.detail);
  return result;
}

}  // namespace examples::vmexit_keylog_capture