// RED example implementation for strategy `byovd`.
// Multi-step World scars: vulnerable driver, mem-rw device, and IOCTL read of game memory.

#include "red_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>
#include <vector>

namespace examples::byovd {

RedResult apply(sim::World& w) {
  RedResult r{};
  r.detail = "BYOVD precondition failed: no game process";

  std::printf("[red:byovd] require a game process and no BYOVD policy block\n");
  const auto game = w.game_pid();
  if (!game || !w.proc(game) || w.byovd_policy_block) {
    return r;
  }

  std::printf("[red:byovd] spawn BYOVD lab client and load legacy-vuln.sys\n");
  const auto actor = w.spawn("byovd-lab-client.exe");
  w.load_driver({"legacy-vuln.sys", "byovd-demo-sha", "LegacyVendor", false, true,
                 false, false, true});
  if (!support::has_driver(w, "legacy-vuln.sys")) {
    r.detail = "legacy-vuln.sys registration failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:byovd] expose \\\\.\\LegacyVuln memory device\n");
  w.create_device({"\\\\.\\LegacyVuln", "legacy-vuln.sys", true});
  if (!support::has_device(w, "\\\\.\\LegacyVuln")) {
    r.detail = "LegacyVuln device creation failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:byovd] exercise device IOCTL read of game memory\n");
  std::vector<std::uint8_t> bytes;
  if (!w.device_ioctl_read(actor, "\\\\.\\LegacyVuln", game, w.proc(game)->base, 4,
                           bytes) ||
      bytes.size() != 4) {
    r.detail = "BYOVD IOCTL read failed";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "BYOVD driver, device, and IOCTL read verified";
  w.note(r.detail);
  std::printf("[red:byovd] %s (%d steps)\n", r.detail.c_str(), r.steps);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "byovd",
         "Load vulnerable driver and verify IOCTL read.");
  auto r = apply(w);
  n.say(sim::Side::Red, r.detail);
  return r;
}

}  // namespace examples::byovd
