// T4 Duel — off-box DMA residual. Client process list clean; server residual.

#include "server/info_advantage.hpp"
#include "server/interest_mgmt.hpp"
#include "sim/narrative.hpp"
#include "sim/world.hpp"
#include "t4_blue/dma_defense.hpp"
#include "t4_red/dma_radar.hpp"

#include <cstdio>

int main() {
  sim::Narrator n;
  n.say(sim::Side::Lesson,
        "T4: hardware residual. Often no local cheat process — blue leans on "
        "platform policy + server authority.");

  auto world = sim::make_arena();

  n.move(sim::Side::Red, "Full multi-step DMA loop",
         "Enable FPGA/2nd-PC path, probe IOMMU block, pull entities, plant residuals.");
  t4_red::DmaRadar red(world);
  auto red_r = red.run_full_loop(/*residuals=*/true);
  std::printf(
      "    hardware=%d clean=%d offbox_read=%d entities=%d iommu_probe=%d "
      "dma_ops=%d residuals(cap=%d desk=%d clone=%d)\n",
      red_r.hardware_enabled ? 1 : 0, red_r.process_list_clean ? 1 : 0,
      red_r.offbox_read_ok ? 1 : 0, red_r.entity_count,
      red_r.iommu_blocked_probe ? 1 : 0, red_r.dma_ops,
      red_r.capture_residual ? 1 : 0, red_r.desktop_dup ? 1 : 0,
      red_r.external_clone ? 1 : 0);
  n.say(sim::Side::Red, red_r.detail);

  n.counter(sim::Side::Blue, "Multi-sensor platform + residual scan",
            "DMA/IOMMU signal + capture/desktop/clone residual scars.");
  t4_blue::DmaDefense blue(world);
  auto scan = blue.full();
  for (const auto& reason : scan.reasons) {
    n.say(sim::Side::Blue, reason);
  }
  std::printf(
      "    platform=%d dma=%d fog=%d iommu_enf=%d structural=%d ia=%.1f risk=%.1f "
      "reasons=%zu\n",
      scan.platform_signal ? 1 : 0, scan.dma_device ? 1 : 0,
      scan.fog_applied ? 1 : 0, scan.iommu_enforced ? 1 : 0,
      scan.structural_kill ? 1 : 0, scan.ia_score, scan.risk,
      scan.reasons.size());
  n.say(sim::Side::Blue, scan.detail);

  n.counter(sim::Side::Blue, "Server interest filter (demo)",
            "Far enemies culled from client view.");
  server::InterestManager im;
  std::vector<server::WorldEntity> all = {
      {0, {0, 0, 0}, 1, true},
      {1, {500, 0, 500}, 2, true},
  };
  auto vis = im.filter_for_client({{0, 0, 0}, 0}, 1, all, 50.f);
  std::printf("    replicated_entities=%zu (far enemy culled)\n", vis.size());

  n.counter(sim::Side::Blue, "Info-advantage residual",
            "Human radar use still leaks impossible knowledge on-server.");
  server::InfoAdvantageScorer ia;
  for (int i = 0; i < 3; ++i) {
    ia.on_frame({double(i), {}, 90.f, false, false, true});
  }
  std::printf("    info_advantage_score=%.1f hits=%d (full_ia=%.1f)\n",
              ia.result().score, ia.result().hits, scan.ia_score);

  const bool blue_wins =
      (scan.platform_signal || scan.dma_device || !scan.reasons.empty()) &&
      (scan.fog_applied || scan.structural_kill ||
       !world.server_sends_full_enemy_origin);
  n.result(blue_wins,
           "T4: process list clean for red, but IOMMU signal + fog + behavioral "
           "constrain the residual.");

  n.say(sim::Side::Lesson,
        "Curriculum: client AC is nearly blind here — design for server-side "
        "authority. Re-run strategy_lab 06_dma_hardware.");
  return blue_wins ? 0 : 1;
}
