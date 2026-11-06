#include "red_example.hpp"

#include <cstdio>

namespace examples::loader_addr_registry {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game || game->memory.empty()) {
    std::printf("[T1 loader_addr_registry] FAIL: game process is unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T1 loader_addr_registry] step %d: game pid=%u validated\n", ++steps, game_pid);

  const auto loader = w.spawn("registry-loader.exe");
  if (!w.proc(loader)) {
    std::printf("[T1 loader_addr_registry] FAIL: loader actor could not start\n");
    return {false, steps, "loader spawn verification failed"};
  }
  std::printf("[T1 loader_addr_registry] step %d: loader pid=%u created\n", ++steps, loader);

  // Step: no scanner ships in the payload. The loader resolved the entity
  // addresses out-of-process (offline signature run) into an encrypted
  // registry section; the live process only decodes.
  w.encrypted_addr_registry_active = true;
  w.addr_registry_section_encrypted = true;
  if (!w.encrypted_addr_registry_active || !w.addr_registry_section_encrypted) {
    return {false, steps, "encrypted registry section was not established"};
  }
  std::printf("[T1 loader_addr_registry] step %d: encrypted address registry armed (no scanner shipped)\n",
              ++steps);

  if (!w.open_process(loader, game_pid, sim::AccessMask::VmRead)) {
    std::printf("[T1 loader_addr_registry] FAIL: loader VM_READ handle denied\n");
    return {false, steps, "loader VM_READ handle failed"};
  }
  std::printf("[T1 loader_addr_registry] step %d: loader VM_READ edge opened\n", ++steps);

  // Step: decode-time access. Decoding writes the plaintext pointers into the
  // data section for the read window - a short-lived, detectable mutation.
  w.registry_decode_ops = 3;
  w.registry_data_section_patched = true;
  std::printf("[T1 loader_addr_registry] step %d: registry decoded into data section (ops=%d)\n",
              ++steps, w.registry_decode_ops);

  auto read = w.read_mem(loader, game_pid, game->base + w.lab_entity_table_rel, 4, true);
  if (read.status != ac::Status::Ok || read.bytes.size() != 4) {
    std::printf("[T1 loader_addr_registry] FAIL: registry-guided read failed\n");
    return {false, steps, "registry-guided entity read failed"};
  }
  std::uint32_t entity_count = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    entity_count |= static_cast<std::uint32_t>(read.bytes[i]) << (8 * i);
  }
  if (entity_count == 0) {
    return {false, steps, "registry-guided read produced no entity data"};
  }
  std::printf("[T1 loader_addr_registry] step %d: registry-guided read entity_count=%u\n",
              ++steps, entity_count);

  // Sub-scenario: offset drift. The build moves its layout; the live binary
  // cannot re-scan (no scanner), so the loader must re-resolve out-of-process
  // and refresh the encrypted registry.
  if (!w.mutate_lab_pattern_layout(game_pid, 0x100, 0x200)) {
    std::printf("[T1 loader_addr_registry] FAIL: layout drift not applied\n");
    return {false, steps, "layout drift simulation failed"};
  }
  w.registry_decode_ops += 2;  // decode the refreshed registry
  ++w.registry_drift_recoveries;
  w.offset_drift_recovered = true;
  auto read2 = w.read_mem(loader, game_pid, game->base + w.lab_entity_table_rel, 4, true);
  if (read2.status != ac::Status::Ok || read2.bytes.empty()) {
    std::printf("[T1 loader_addr_registry] FAIL: post-drift read failed\n");
    return {false, steps, "post-drift registry read failed"};
  }
  std::printf("[T1 loader_addr_registry] step %d: offset drift recovered via loader re-resolve (gen=%u)\n",
              ++steps, w.lab_pattern_generation);

  const std::string detail =
      "loader_addr_registry: decode_ops=" + std::to_string(w.registry_decode_ops) +
      " drift_recoveries=" + std::to_string(w.registry_drift_recoveries) +
      " section_encrypted=" + std::to_string(w.addr_registry_section_encrypted);
  w.note(detail);
  return {true, steps, detail};
}

}  // namespace examples::loader_addr_registry
