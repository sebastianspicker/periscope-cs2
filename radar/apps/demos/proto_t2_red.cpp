// T2 RED prototype — full kernel/BYOVD channel on sim::World, no game handle.

#include "ac/proto_log.hpp"
#include "lab/fixture_process.hpp"
#include "sim/world.hpp"
#include "t0_red/entity_pipeline.hpp"
#include "t0_red/radar_ui.hpp"
#include "t2_red/byovd_surface.hpp"
#include "t2_red/ioctl_backend.hpp"
#include "t2_red/kernel_radar.hpp"
#include "t2_red/t2_driver_abi.h"

#include <cstdio>

int main() {
  ac::proto_banner("RED", "T2", "IOCTL kernel read + BYOVD (full sim path)");

  // Path A: fixture unit shape
  t2_red::ByovdSurface byovd;
  byovd.set_candidate({
      .image_name = "LabVulnDrv.sys",
      .sha256_hex = "lab_byovd_hash_001",
      .signed_driver = true,
  });
  ac::proto_status("byovd_load_sim", byovd.attempt_load_lab());
  ac::proto_kv("byovd_loaded", byovd.loaded());

  auto& fx = lab::global_fixture();
  t2_red::IoctlReadBackend backend;
  ac::proto_status("fixture_attach", backend.attach_fixture(fx.id()));
  ac::proto_kv("fixture_game_handle", backend.exposes_usermode_handle_to_game());
  t0_red::EntityPipeline pipe(backend);
  ac::proto_status("fixture_refresh", pipe.refresh(fx.base_address()));
  ac::proto_kv("fixture_entities", pipe.entities().size());
  std::printf("  %-28s 0x%08x (READ_VA lab)\n", "ioctl_code_lab",
              static_cast<unsigned>(T2_IOCTL_DEC(T2_IOCTL_READ_VA_ENC)));

  // Path B: full World loop
  auto world = sim::make_arena();
  t2_red::KernelRadar radar(world);
  auto rep = radar.run_full_loop(t2_red::KernelPath::Byovd, true);
  ac::proto_kv("world_brought_up", rep.brought_up);
  ac::proto_kv("world_entities", static_cast<std::size_t>(rep.entity_count));
  ac::proto_kv("world_no_game_handle", rep.no_game_handle);
  ac::proto_kv("world_byovd", rep.byovd);
  ac::proto_kv("world_ioctl_ops", static_cast<std::size_t>(rep.ioctl_ops));
  ac::proto_kv("world_callback_stripped", rep.callback_stripped);
  ac::proto_line("world_detail", rep.detail);
  ac::proto_line("device", rep.device);
  ac::proto_line("driver_sha", rep.driver_sha);

  t0_red::RadarUi ui;
  ui.set_title("t2-ud-radar");
  ui.update(radar.entities(), {});
  ac::proto_kv("blips", ui.blips().size());

  const bool ok = rep.brought_up && rep.entities_ok && rep.no_game_handle &&
                  rep.entity_count > 0;
  ac::proto_kv("T2_RED_SCAR_OK", ok);
  std::puts(ok ? "prototype ok (full T2 no-handle kernel scar + entities)"
               : "prototype FAIL");
  // Full multi-sensor blue scan: merge independent reasons/risk (not one bool).
  std::puts("\n[blue should counter] KernelAc full_scan (blocklist+device+callbacks)");
  return ok ? 0 : 1;
}
