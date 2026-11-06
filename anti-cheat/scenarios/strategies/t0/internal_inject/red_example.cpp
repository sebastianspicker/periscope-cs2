#include "red_example.hpp"

#include <cstdio>

namespace examples::internal_inject {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    std::printf("[T0 internal_inject] FAIL: game process is unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T0 internal_inject] step %d: game pid=%u validated\n", ++steps, game_pid);

  const auto actor = w.spawn("internal_inject-actor.exe");
  auto* actor_process = w.proc(actor);
  if (!actor_process) {
    std::printf("[T0 internal_inject] FAIL: actor creation was not observable\n");
    return {false, steps, "actor spawn verification failed"};
  }
  std::printf("[T0 internal_inject] step %d: actor pid=%u created\n", ++steps, actor);

  if (!w.open_process(actor, game_pid, sim::AccessMask::VmRead, false)) {
    std::printf("[T0 internal_inject] FAIL: VM_READ handle denied\n");
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
  std::printf("[T0 internal_inject] step %d: VM_READ edge verified\n", ++steps);

  auto read = w.read_mem(actor, game_pid, game->base, 4, true);
  if (read.status != ac::Status::Ok || read.bytes.size() != 4) {
    std::printf("[T0 internal_inject] FAIL: protected read failed\n");
    return {false, steps, "VM_READ verification read failed"};
  }
  std::printf("[T0 internal_inject] step %d: read telemetry recorded\n", ++steps);

  game->has_foreign_thread = true; game->manual_mapped_region = true;
  const bool scar_verified = game->has_foreign_thread && game->manual_mapped_region;
  if (!scar_verified) {
    std::printf("[T0 internal_inject] FAIL: foreign module and execution thread was not retained\n");
    return {false, steps, "scar verification failed: foreign module and execution thread"};
  }
  std::printf("[T0 internal_inject] step %d: foreign module and execution thread planted and verified\n", ++steps);
  const std::string detail = "internal_inject: VM_READ plus foreign module and execution thread";
  w.note(detail);
  return {true, steps, detail};
}

}  // namespace examples::internal_inject
