#include "red_example.hpp"

namespace examples::vmexit_keylog_capture {

RedResult apply(sim::World& w) {
  RedResult result{false, 0, 0, ""};
  const auto game = w.game_pid();
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, result.steps, 0, "game process is unavailable"};
  }
  ++result.steps;

  w.trust.vbs = false;
  w.trust.hvci = false;
  w.trust.hvci_enabled = false;
  if (!w.try_start_personal_hv("LabVmExitInterruptCoordinator")) {
    return {false, result.steps, 0, "personal HV coordinator was blocked"};
  }
  ++result.steps;

  const auto bridge = w.spawn("vmexit-keylog-bridge-ui.exe");
  w.load_driver({"vmexitbridge.sys", "sha-vmexitbridge", "lab-self-signed",
                 false, false, false, true, false, 10});
  w.create_device({"\\Device\\VmExitBridge", "vmexitbridge.sys", false});
  if (bridge == 0 || w.proc(bridge) == nullptr) {
    return {false, result.steps, 0, "VM-exit bridge actor was unavailable"};
  }
  ++result.steps;

  // Lab-only scars represent VMCS external-interrupt interception for keyboard
  // IRQs and exfiltration of captured data to the bridge UI process.
  w.vmexit_keylog_active = true;
  w.vmexit_keys_captured = 15;
  result.keys_captured = w.vmexit_keys_captured;
  ++result.steps;

  const bool bridge_ready = !w.devices.empty();
  const bool no_game_handle = w.handles_to(game).empty();
  result.achieved = w.trust.personal_hv_active && w.vmexit_keylog_active &&
                    result.keys_captured > 0 && bridge_ready && no_game_handle;
  result.detail = "VM-exit keylog active=" +
                  std::to_string(w.vmexit_keylog_active) +
                  " keys=" + std::to_string(result.keys_captured) +
                  " bridge=" + std::to_string(bridge_ready) +
                  " no_game_handle=" + std::to_string(no_game_handle);
  w.note(result.detail);
  return result;
}

}  // namespace examples::vmexit_keylog_capture
