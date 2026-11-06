// T0 RED prototype — lab-only external RPM radar shape.

#include "ac/proto_log.hpp"
#include "lab/fixture_process.hpp"
#include "t0_red/entity_pipeline.hpp"
#include "t0_red/radar_ui.hpp"
#include "t0_red/rpm_backend.hpp"

#include <cstdio>

int main() {
  ac::proto_banner("RED", "T0", "Usermode RPM external radar");

  auto& fx = lab::global_fixture();
  t0_red::RpmBackend backend;
  ac::proto_status("attach(fixture)", backend.attach(fx.id()));
  ac::proto_kv("exposes_usermode_handle", backend.exposes_usermode_handle());
  ac::proto_line("backend", backend.name());

  t0_red::EntityPipeline pipe(backend);
  ac::proto_status("refresh(entities)", pipe.refresh(fx.base_address()));
  ac::proto_kv("entity_count", pipe.entities().size());

  for (const auto& e : pipe.entities()) {
    std::printf("    entity id=%u team=%u alive=%d pos=(%.1f,%.1f,%.1f)\n",
                e.id, e.team, e.alive ? 1 : 0, e.origin.x, e.origin.y,
                e.origin.z);
  }

  t0_red::RadarUi ui;
  ui.set_title("t0-lab-radar");
  ui.update(pipe.entities(), ac::Vec3{0, 0, 0});
  ac::proto_line("ui_title", ui.title());
  ac::proto_kv("blip_count", ui.blips().size());
  for (const auto& b : ui.blips()) {
    std::printf("    blip team=%u map=(%.1f,%.1f)\n", b.team, b.map_x, b.map_y);
  }

  std::puts("\n[blue should counter] HandleGraphMonitor + ProcessCooccurrence");
  std::puts("prototype ok");
  return 0;
}
