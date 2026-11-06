// T2 Duel — empty handles; drivers/devices/blocklists decide.

#include "ac/telemetry.hpp"
#include "sim/narrative.hpp"
#include "sim/world.hpp"
#include "t0_blue/ac_agent.hpp"
#include "t2_blue/kernel_ac.hpp"
#include "t2_red/kernel_radar.hpp"

#include <cstdio>

int main() {
  sim::Narrator n;
  n.say(sim::Side::Lesson,
        "T2: red leaves the handle graph. Blue must hunt kernel surfaces.");

  auto world = sim::make_arena();
  ac::MemoryTelemetrySink sink;

  n.move(sim::Side::Red, "BYOVD + IOCTL entity pull + callback strip",
         "Signed vulnerable driver → R/W without OpenProcess.");
  t2_red::KernelRadar red(world);
  auto rep = red.run_full_loop(t2_red::KernelPath::Byovd, /*strip_cbs=*/true);
  n.say(sim::Side::Red, rep.detail);
  std::printf("[red] entities=%d has_game_handle=%d byovd=%d ioctl_ops=%d\n",
              rep.entity_count, red.has_game_handle() ? 1 : 0, rep.byovd ? 1 : 0,
              rep.ioctl_ops);

  n.counter(sim::Side::Blue, "T0 handle agent",
            "Should be quiet if red is pure T2.");
  t0_blue::AcAgent t0(world, sink);
  auto hscan = t0.scan_handles();
  std::printf("    t0_handle_hit=%d\n", hscan.handle_hit ? 1 : 0);

  n.counter(sim::Side::Blue, "KernelAc full_scan",
            "Blocklist + unknown memrw + devices + callback audit.");
  t2_blue::KernelAc kac(world, sink);
  auto det = kac.full_scan();
  for (const auto& r : det.reasons) {
    n.say(sim::Side::Blue, r);
  }
  std::printf("[blue] %s risk=%.1f\n", det.summary.c_str(), det.risk);

  // Mitigate: block further IOCTL
  bool mit = kac.mitigate();
  std::vector<std::uint8_t> after;
  bool ioctl_after = world.device_ioctl_read(
      red.ui_pid(), red.device(), world.game_pid(),
      world.proc(world.game_pid())->base, 4, after);
  std::printf("    mitigate=%d ioctl_after_block=%d\n", mit ? 1 : 0,
              ioctl_after ? 1 : 0);

  const bool blue_wins =
      (det.byovd || det.suspicious_device || det.callback_tamper ||
       det.unknown_memrw_driver) &&
      det.risk > 0 && !det.reasons.empty();
  n.result(blue_wins,
           !hscan.handle_hit
               ? "Handle graph silent, but kernel suite caught T2 surface."
               : "Also saw handles (mixed red).");

  n.say(sim::Side::Lesson,
        "Next: T3 goes under the OS with a personal HV. Run duel_t3.");
  return blue_wins && !ioctl_after ? 0 : 1;
}
