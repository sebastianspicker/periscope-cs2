// T4 RED prototype — off-box DMA residual.

#include "ac/proto_log.hpp"
#include "sim/world.hpp"
#include "t4_red/dma_radar.hpp"

#include <cstdio>
#include <vector>

int main() {
  ac::proto_banner("RED", "T4", "Off-box DMA + residual scars (IOMMU fail path)");

  // Scenario A: DMA device present but IOMMU still on → denied.
  {
    auto blocked = sim::make_arena();
    blocked.trust.dma_device_present = true;
    // iommu stays on (arena default)
    std::vector<std::uint8_t> buf;
    auto* g = blocked.proc(blocked.game_pid());
    const bool denied =
        g && !blocked.dma_read(blocked.game_pid(), g->base, 4, buf);
    ac::proto_kv("iommu_on_blocks_dma", denied);
  }

  // Scenario B: full multi-step loop (enable → probe → pull → residuals).
  auto world = sim::make_arena();
  t4_red::DmaRadar radar(world);
  auto rep = radar.run_full_loop(/*residuals=*/true);

  ac::proto_kv("hardware_enabled", rep.hardware_enabled);
  ac::proto_kv("iommu_off", rep.iommu_off);
  ac::proto_kv("iommu_blocked_probe", rep.iommu_blocked_probe);
  ac::proto_kv("process_list_clean", rep.process_list_clean);
  ac::proto_kv("offbox_read_ok", rep.offbox_read_ok);
  ac::proto_kv("entities_ok", rep.entities_ok);
  ac::proto_kv("entity_count", static_cast<std::size_t>(rep.entity_count));
  ac::proto_kv("dma_ops", static_cast<std::size_t>(rep.dma_ops));
  ac::proto_kv("bytes_read", static_cast<std::size_t>(rep.bytes_read));
  ac::proto_kv("capture_residual", rep.capture_residual);
  ac::proto_kv("desktop_dup", rep.desktop_dup);
  ac::proto_kv("external_clone", rep.external_clone);
  ac::proto_line("detail", rep.detail);

  const bool ok = rep.hardware_enabled && rep.offbox_read_ok &&
                  rep.process_list_clean && rep.entity_count >= 1;
  std::puts(ok ? "prototype ok (full T4 pure DMA path, clean process list)"
               : "prototype FAIL");
  return ok ? 0 : 1;
}
