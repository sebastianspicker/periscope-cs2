#include "red_example.hpp"

#include <cstdio>
#include <vector>

namespace examples::escalation_cost_curve {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    std::printf("[T2 escalation_cost_curve] FAIL: game process is unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T2 escalation_cost_curve] step %d: game pid=%u validated\n", ++steps, game_pid);

  // Rung 1: plain RPM. A foreign process opens a normal VM_READ handle and
  // reads. Persistent mutations: none yet - but the handle edge is a scar.
  const auto rpm_pid = w.spawn("escalation-rpm.exe");
  if (!w.proc(rpm_pid) ||
      !w.open_process(rpm_pid, game_pid, sim::AccessMask::VmRead)) {
    return {false, steps, "rung 1 (RPM) handle open failed"};
  }
  auto rpm_read = w.read_mem(rpm_pid, game_pid, game->base + w.lab_entity_table_rel, 4, true);
  if (rpm_read.status != ac::Status::Ok) {
    return {false, steps, "rung 1 (RPM) read failed"};
  }
  w.escalation_rungs = 1;
  std::printf("[T2 escalation_cost_curve] step %d: rung 1 RPM read ok (handle scar left)\n", ++steps);

  // Rung 2: direct syscall path. Same data, different acquisition model -
  // leaves a via_syscall handle edge instead of a WinAPI edge.
  const auto sys_pid = w.spawn("escalation-syscall.exe");
  if (!w.proc(sys_pid) ||
      !w.open_process(sys_pid, game_pid, sim::AccessMask::VmRead, /*syscall_path=*/true)) {
    return {false, steps, "rung 2 (syscall) open failed"};
  }
  auto sys_read = w.read_mem(sys_pid, game_pid, game->base + w.lab_entity_table_rel, 4, true);
  if (sys_read.status != ac::Status::Ok) {
    return {false, steps, "rung 2 (syscall) read failed"};
  }
  w.escalation_rungs = 2;
  std::printf("[T2 escalation_cost_curve] step %d: rung 2 syscall read ok (syscall scar left)\n", ++steps);

  // Rung 3: kernel driver. Requires persistent system mutations to even load:
  // testsigning on, HVCI off, blocklist/registry toggles, pending reboot.
  sim::Driver drv{"escalation-memrw.sys", "escalation_memrw_hash", "Escalation Labs",
             false, false, false, false, true, 90};
  w.load_driver(std::move(drv));
  w.escalation_driver_loaded = true;
  w.create_device({"\\\\.\\escalation-memrw", "escalation-memrw.sys", true});

  w.trust.test_signing = true;             // testsigning flag flipped
  w.trust.hvci_enabled = false;            // HVCI toggled off
  w.blocklist_registry_toggled = true;     // vulnerable-driver blocklist toggled
  w.registry_persisted_config = true;      // registry persistence for the toggles
  w.registry_key_path = "HKLM\\SYSTEM\\CurrentControlSet\\Control\\CI\\Config";
  w.pending_reboot_required = true;        // the toggles need a reboot to apply
  std::printf("[T2 escalation_cost_curve] step %d: rung 3 driver loaded with posture mutations\n", ++steps);

  std::vector<std::uint8_t> out;
  const auto ioctl_ok = w.device_ioctl_read(rpm_pid, "\\\\.\\escalation-memrw",
                                            game_pid,
                                            game->base + w.lab_entity_table_rel, 4, out);
  if (!ioctl_ok || out.empty()) {
    return {false, steps, "rung 3 (kernel) ioctl read failed"};
  }
  w.escalation_rungs = 3;
  std::printf("[T2 escalation_cost_curve] step %d: rung 3 kernel ioctl read ok\n", ++steps);

  // Price the escalation: count the persistent mutations blue can observe.
  w.escalation_ladder_used = true;
  w.escalation_footprints = 0;
  if (w.trust.test_signing) ++w.escalation_footprints;
  if (!w.trust.hvci_enabled) ++w.escalation_footprints;
  if (w.blocklist_registry_toggled) ++w.escalation_footprints;
  if (w.registry_persisted_config) ++w.escalation_footprints;
  if (w.pending_reboot_required) ++w.escalation_footprints;
  if (w.escalation_driver_loaded) ++w.escalation_footprints;
  if (w.escalation_rungs != 3 || w.escalation_footprints < 4) {
    return {false, steps, "escalation ladder or footprints incomplete"};
  }
  std::printf("[T2 escalation_cost_curve] step %d: escalation priced at %d persistent footprints\n",
              ++steps, w.escalation_footprints);

  const std::string detail =
      "escalation_cost_curve: rungs=" + std::to_string(w.escalation_rungs) +
      " footprints=" + std::to_string(w.escalation_footprints) +
      " (testsigning+hvci+registry+reboot+driver)";
  w.note(detail);
  return {true, steps, detail};
}

}  // namespace examples::escalation_cost_curve
