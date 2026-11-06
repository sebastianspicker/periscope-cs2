// RED example implementation for strategy `kernel_ioctl`.
// Multi-step World scars: lab mem-rw driver/device and IOCTL read without VM_READ handle.

#include "red_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>
#include <vector>

namespace examples::kernel_ioctl {

RedResult apply(sim::World& w) {
  RedResult r{};
  r.detail = "kernel IOCTL precondition failed: game unavailable";

  std::printf("[red:kernel_ioctl] require game process and ranked access allowed\n");
  const auto game = w.game_pid();
  if (!game || !w.proc(game) || w.ranked_access_denied) {
    return r;
  }

  std::printf("[red:kernel_ioctl] spawn IOCTL client and load lab-memrw.sys\n");
  const auto actor = w.spawn("ioctl-client.exe");
  w.load_driver({"lab-memrw.sys", "ioctl-demo-sha", "LabSigner", false, false,
                 false, false, true});
  if (!support::has_driver(w, "lab-memrw.sys")) {
    r.detail = "lab-memrw.sys registration failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:kernel_ioctl] create \\\\.\\LabMemRw memory device\n");
  w.create_device({"\\\\.\\LabMemRw", "lab-memrw.sys", true});
  if (!support::has_device(w, "\\\\.\\LabMemRw")) {
    r.detail = "LabMemRw device creation failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:kernel_ioctl] exercise kernel-backed device IOCTL read\n");
  std::vector<std::uint8_t> bytes;
  if (!w.device_ioctl_read(actor, "\\\\.\\LabMemRw", game, w.proc(game)->base, 4,
                           bytes) ||
      bytes.empty()) {
    r.detail = "kernel memory device read failed";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "kernel memory device read verified without VM_READ handle";
  w.note(r.detail);
  std::printf("[red:kernel_ioctl] %s (%d steps)\n", r.detail.c_str(), r.steps);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "kernel_ioctl",
         "Exercise a simulated kernel memory device.");
  auto r = apply(w);
  n.say(sim::Side::Red, r.detail);
  return r;
}

}  // namespace examples::kernel_ioctl
