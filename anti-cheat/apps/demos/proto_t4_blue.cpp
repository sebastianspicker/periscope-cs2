// T4 BLUE prototype — platform signal + structural fog + behavioral residual.

#include "ac/proto_log.hpp"
#include "sim/world.hpp"
#include "t4_blue/dma_defense.hpp"
#include "t4_red/dma_radar.hpp"

#include <cstdio>

int main() {
  ac::proto_banner("BLUE", "T4",
                   "Platform IOMMU + interest fog + info-advantage residual");

  // Scar world via shipped red path.
  auto world = sim::make_arena();
  t4_red::DmaRadar radar(world);
  auto rep = radar.run_full_loop(true);
  ac::proto_kv("red_entities", static_cast<std::size_t>(rep.entity_count));
  ac::proto_kv("red_clean", rep.process_list_clean);

  t4_blue::DmaDefense blue(world);
  auto plat = blue.scan_platform();
  ac::proto_kv("platform_signal", plat.platform_signal);
  ac::proto_kv("dma_device", plat.dma_device);

  auto full = blue.full();
  ac::proto_kv("platform_signal_full", full.platform_signal);
  ac::proto_kv("fog_applied", full.fog_applied);
  ac::proto_kv("structural_kill", full.structural_kill);
  ac::proto_kv("iommu_enforced", full.iommu_enforced);
  ac::proto_kv("stream_encrypted", full.stream_encrypted);
  ac::proto_kv("info_advantage_hit", full.info_advantage_hit);
  ac::proto_kv("ia_score", full.ia_score);
  ac::proto_kv("risk", full.risk);
  ac::proto_kv("reasons", full.reasons.size());
  ac::proto_kv("fog_replicated", static_cast<std::size_t>(full.fog_replicated));
  ac::proto_line("detail", full.detail);

  for (const auto& r : full.reasons) {
    ac::proto_line("reason", r);
  }

  ac::proto_kv("server_fog", !world.server_sends_full_enemy_origin);
  ac::proto_kv("iommu_on_after", world.trust.iommu_on);

  const bool detected =
      full.platform_signal || full.dma_device || !full.reasons.empty();
  const bool mitigated =
      full.fog_applied || full.structural_kill || !world.server_sends_full_enemy_origin;
  ac::proto_kv("T4_DETECTED", detected);
  ac::proto_kv("T4_MITIGATED", mitigated);

  const bool ok = detected && mitigated && full.risk > 0;
  std::puts(ok ? "prototype ok (t4 multi-sensor + structural residual)"
               : "prototype FAIL");
  return ok ? 0 : 1;
}
