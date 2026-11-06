#include "red_example.hpp"

#include <cstdio>

namespace examples::block_input {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    std::printf("[T0 block_input] FAIL: game process is unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T0 block_input] step %d: game pid=%u validated\n", ++steps, game_pid);

  const auto actor = w.spawn("block_input-actor.exe");
  auto* actor_process = w.proc(actor);
  if (!actor_process) {
    std::printf("[T0 block_input] FAIL: actor creation was not observable\n");
    return {false, steps, "actor spawn verification failed"};
  }
  std::printf("[T0 block_input] step %d: actor pid=%u created\n", ++steps, actor);

  if (!w.open_process(actor, game_pid, sim::AccessMask::VmRead, false)) {
    std::printf("[T0 block_input] FAIL: VM_READ handle denied\n");
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
  std::printf("[T0 block_input] step %d: VM_READ edge verified\n", ++steps);

  auto read = w.read_mem(actor, game_pid, game->base, 4, true);
  if (read.status != ac::Status::Ok || read.bytes.size() != 4) {
    std::printf("[T0 block_input] FAIL: protected read failed\n");
    return {false, steps, "VM_READ verification read failed"};
  }
  std::printf("[T0 block_input] step %d: read telemetry recorded\n", ++steps);

  w.block_input_active = true;
  const bool scar_verified = w.block_input_active;
  if (!scar_verified) {
    std::printf("[T0 block_input] FAIL: input suppression was not retained\n");
    return {false, steps, "scar verification failed: input suppression"};
  }
  std::printf("[T0 block_input] step %d: input suppression planted and verified\n", ++steps);
  const std::string detail = "block_input: VM_READ plus input suppression";
  w.note(detail);
  return {true, steps, detail};
}

}  // namespace examples::block_input
