// T1 RED prototype — full staged loader + offsets + World syscall RPM.

#include "ac/proto_log.hpp"
#include "lab/fixture_process.hpp"
#include "sim/world.hpp"
#include "t0_red/entity_pipeline.hpp"
#include "t0_red/radar_ui.hpp"
#include "t1_red/offset_blob.hpp"
#include "t1_red/staged_loader.hpp"
#include "t1_red/syscall_backend.hpp"
#include "t1_red/syscall_cheat.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

std::vector<std::uint8_t> make_lab_offset_blob(std::uint8_t key) {
  const char* name = "entity_off";
  std::vector<std::uint8_t> plain;
  plain.push_back(static_cast<std::uint8_t>(std::strlen(name)));
  for (const char* p = name; *p; ++p) {
    plain.push_back(static_cast<std::uint8_t>(*p));
  }
  const std::uint64_t val = 0x10;
  for (int b = 0; b < 8; ++b) {
    plain.push_back(static_cast<std::uint8_t>((val >> (8 * b)) & 0xff));
  }
  std::vector<std::uint8_t> enc(plain.size());
  for (std::size_t i = 0; i < plain.size(); ++i) {
    enc[i] = static_cast<std::uint8_t>(plain[i] ^ key);
  }
  return enc;
}

}  // namespace

int main() {
  ac::proto_banner("RED", "T1", "Syscall RPM + staged loader + offset blob");

  // Path A: fixture unit shape
  t1_red::StagedLoader loader;
  ac::proto_status("auth", loader.authenticate_lab("lab-token"));
  ac::proto_status("map_payload",
                   loader.map_payload_lab({0x90, 0x90, 0xC3}));
  ac::proto_kv("private_rx_region", loader.has_private_rx_region());

  t1_red::OffsetBlob offsets;
  constexpr std::uint8_t kKey = 0x5A;
  ac::proto_status("decrypt_offsets",
                   offsets.load_encrypted(make_lab_offset_blob(kKey), kKey));
  ac::proto_kv("entity_off",
               static_cast<std::size_t>(offsets.get("entity_off")));

  auto& fx = lab::global_fixture();
  t1_red::SyscallBackend backend;
  ac::proto_status("attach_fixture", backend.attach_fixture(fx.id()));
  ac::proto_kv("fixture_bypasses_hooks", backend.bypasses_usermode_api_hooks());
  ac::proto_kv("fixture_exposes_handle", backend.exposes_usermode_handle());

  t0_red::EntityPipeline pipe(backend);
  ac::proto_status("fixture_refresh", pipe.refresh(fx.base_address()));
  ac::proto_kv("fixture_entities", pipe.entities().size());

  // Path B: full World loop (primary lesson)
  auto world = sim::make_arena();
  t1_red::SyscallCheat cheat(world, "soft-radar.exe");
  auto rep = cheat.run_full_loop(true, true);
  ac::proto_kv("world_attached", rep.attached);
  ac::proto_kv("world_via_syscall", rep.via_syscall);
  ac::proto_kv("world_entities", static_cast<std::size_t>(rep.entity_count));
  ac::proto_kv("world_syscall_handles",
               static_cast<std::size_t>(rep.syscall_handles));
  ac::proto_kv("world_staged", rep.staged);
  ac::proto_kv("world_private_rx", rep.private_rx);
  ac::proto_kv("world_read_ops", static_cast<std::size_t>(rep.read_ops));
  ac::proto_line("world_detail", rep.detail);

  t0_red::RadarUi ui;
  ui.set_title("t1-packed-radar");
  ui.update(cheat.entities(), {});
  ac::proto_kv("blips", ui.blips().size());

  const bool ok = rep.attached && rep.via_syscall && rep.entity_count > 0 &&
                  rep.syscall_handles >= 1;
  ac::proto_kv("T1_RED_SCAR_OK", ok);
  std::puts(ok ? "prototype ok (full T1 syscall scar + staging + entities)"
               : "prototype FAIL");
  std::puts("\n[blue should counter] T1Agent / HandleTruthMonitor (not ntdll hooks)");
  return ok ? 0 : 1;
}
