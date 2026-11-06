#include "red_example.hpp"

#include <cstdio>

namespace examples::section_map {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    std::printf("[T0 section_map] FAIL: game process is unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T0 section_map] step %d: game pid=%u validated\n", ++steps, game_pid);

  const auto actor = w.spawn("section_map-actor.exe");
  auto* actor_process = w.proc(actor);
  if (!actor_process) {
    std::printf("[T0 section_map] FAIL: actor creation was not observable\n");
    return {false, steps, "actor spawn verification failed"};
  }
  std::printf("[T0 section_map] step %d: actor pid=%u created\n", ++steps, actor);

  if (!w.open_process(actor, game_pid, sim::AccessMask::VmRead, false)) {
    std::printf("[T0 section_map] FAIL: VM_READ handle denied\n");
    return {false, steps, "OpenProcess VM_READ failed"};
  }
  bool has_handle = false;
  bool has_brief_handle = false;
  for (const auto& handle : w.handles_to(game_pid)) {
    if (handle.owner_pid == actor && sim::has(handle.access, sim::AccessMask::VmRead)) {
      has_handle = true;
      has_brief_handle = handle.brief_reopen;
    }
  }
  if (!has_handle) {
    return {false, steps, "VM_READ handle verification failed"};
  }
  std::printf("[T0 section_map] step %d: VM_READ edge verified\n", ++steps);

  auto read = w.read_mem(actor, game_pid, game->base, 4, true);
  if (read.status != ac::Status::Ok || read.bytes.size() != 4) {
    std::printf("[T0 section_map] FAIL: protected read failed\n");
    return {false, steps, "VM_READ verification read failed"};
  }
  std::printf("[T0 section_map] step %d: read telemetry recorded\n", ++steps);

  w.add_section(sim::SharedSection{"entity-cache", actor, game_pid, true});
  bool has_section = false;
  for (const auto& section : w.sections) {
    if (section.creator_pid == actor && section.consumer_pid == game_pid &&
        section.carries_entity_bytes) {
      has_section = true;
      break;
    }
  }
  const bool scar_verified = has_section;
  if (!scar_verified) {
    std::printf("[T0 section_map] FAIL: shared entity-data section was not retained\n");
    return {false, steps, "scar verification failed: shared entity-data section"};
  }
  std::printf("[T0 section_map] step %d: shared entity-data section planted and verified\n", ++steps);
  const std::string detail = "section_map: VM_READ plus shared entity-data section";
  w.note(detail);
  return {true, steps, detail};
}

}  // namespace examples::section_map
