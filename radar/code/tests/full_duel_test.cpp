#include <cstdio>
#include <cstdlib>
#include <string>

#include "blue/blue_system.hpp"
#include "sim/world.hpp"
#include "ac/types.hpp"
#include "real/win/xorstr.hpp"

// Full duel: plant classic external RPM scars, then evaluate via the shipped
// blue::BlueCoordinator (ac_blue). Local reimplementation is intentionally gone.

int main() {
  std::printf("=== Full Duel Test (shipped blue API) ===\n");

  sim::World world;
  const auto game_pid = world.spawn(OBF("cs2.exe"), true);
  world.plant_lab_entities(game_pid);
  std::printf("[Setup] Game pid=%u\n", game_pid);

  // RED PHASE: direct RPM cheat with sustained polling.
  const auto cheat_pid = world.spawn("cheat-client.exe");
  world.open_process(cheat_pid, game_pid, sim::AccessMask::VmRead, false);
  ac::ReadResult read;
  const auto* game = world.proc(game_pid);
  for (int i = 0; i <= 100; ++i) {
    read = world.read_mem(cheat_pid, game_pid, game->base, 4, true);
  }
  std::printf("[Red] RPM cheat pid=%u read_status=%d remote_ops=%u\n", cheat_pid,
              static_cast<int>(read.status), world.remote_read_ops);

  // BLUE PHASE: shipped multi-sensor coordinator.
  blue::BlueCoordinator blue(world);
  const auto result = blue.evaluate();

  std::printf("[BlueCoordinator] %s\n", result.summary.c_str());
  for (const auto& detail : result.detection_reasons) {
    std::printf("  [BlueCoordinator] %s\n", detail.c_str());
  }
  for (const auto& pv : result.per_view) {
    std::printf("  [view] %s anomaly=%d conf=%.2f detail=%s\n",
                blue::view_name(pv.view), static_cast<int>(pv.anomaly_detected),
                pv.confidence, pv.detail.c_str());
  }

  std::printf("[Duel] red_detected=%d views_with_anomalies=%d strong=%d\n",
              static_cast<int>(result.red_detected), result.views_with_anomalies,
              static_cast<int>(result.strong_detection));

  // CTest treats 0 as pass: multi-view detection of the RPM scar is required.
  const bool pass = result.red_detected && result.views_with_anomalies >= 2 &&
                    !result.detection_reasons.empty();
  const int exit_code = pass ? 0 : 1;
  std::printf("[Duel] Exit code: %d\n", exit_code);
  return exit_code;
}
