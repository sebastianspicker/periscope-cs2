#include "red_example.hpp"

#include <cstdio>

namespace examples::section_entropy_probe {

RedResult apply(sim::World& w) {
  RedResult result;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    return {false, 0, "precondition failed: game process unavailable"};
  }

  const auto loader = w.spawn("entropy-loader.exe");
  if (!w.proc(loader)) return {false, 0, "loader spawn verification failed"};
  result.steps = 1;

  // The registry is resolved offline and ships encrypted in a non-image
  // section. Nothing in the live payload scans for signatures.
  w.encrypted_addr_registry_active = true;
  w.addr_registry_section_encrypted = true;
  w.section_entropy_score = 87;
  result.steps = 2;

  if (!w.open_process(loader, game_pid, sim::AccessMask::VmRead)) {
    return {false, result.steps, "loader VM_READ open failed"};
  }
  result.steps = 3;

  // Patch-free decode: pointers are decoded into registers at use, so the
  // data section keeps its load-time bytes. The only remaining artifact is
  // the high-entropy section itself.
  w.decode_register_only = true;
  w.registry_data_section_patched = false;
  w.registry_decode_ops = 0;
  result.steps = 4;

  auto read = w.read_mem(loader, game_pid,
                         game->base + w.lab_entity_table_rel, 4, true);
  if (read.status != ac::Status::Ok || read.bytes.size() != 4) {
    return {false, result.steps, "registry-guided read failed"};
  }
  std::uint32_t entity_count = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    entity_count |= static_cast<std::uint32_t>(read.bytes[i]) << (8 * i);
  }
  if (entity_count == 0) {
    return {false, result.steps, "registry-guided read returned no entities"};
  }
  result.steps = 5;

  result.achieved = w.decode_register_only && !w.registry_data_section_patched;
  result.detail = "section_entropy_probe: entropy=87 register_only=1 "
                  "data_patch=0 entity_count=" + std::to_string(entity_count);
  std::printf("[T1 section_entropy_probe] step %d: patch-free registry read\n",
              result.steps);
  w.note(result.detail);
  return result;
}

}  // namespace examples::section_entropy_probe
