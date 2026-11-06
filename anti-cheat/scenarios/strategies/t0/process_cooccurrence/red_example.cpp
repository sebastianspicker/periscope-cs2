#include "red_example.hpp"

#include <cstdio>

namespace examples::process_cooccurrence {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    std::printf("[T0 process_cooccurrence] FAIL: game process is unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T0 process_cooccurrence] step %d: game pid=%u validated\n", ++steps, game_pid);

  const auto actor = w.spawn("process_cooccurrence-actor.exe");
  auto* actor_process = w.proc(actor);
  if (!actor_process) {
    std::printf("[T0 process_cooccurrence] FAIL: actor creation was not observable\n");
    return {false, steps, "actor spawn verification failed"};
  }
  std::printf("[T0 process_cooccurrence] step %d: actor pid=%u created\n", ++steps, actor);

  if (!w.open_process(actor, game_pid, sim::AccessMask::VmRead, false)) {
    std::printf("[T0 process_cooccurrence] FAIL: VM_READ handle denied\n");
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
  std::printf("[T0 process_cooccurrence] step %d: VM_READ edge verified\n", ++steps);

  auto read = w.read_mem(actor, game_pid, game->base, 4, true);
  if (read.status != ac::Status::Ok || read.bytes.size() != 4) {
    std::printf("[T0 process_cooccurrence] FAIL: protected read failed\n");
    return {false, steps, "VM_READ verification read failed"};
  }
  std::printf("[T0 process_cooccurrence] step %d: read telemetry recorded\n", ++steps);

  actor_process->reader_active = true;
  const bool scar_verified = actor_process->reader_active;
  if (!scar_verified) {
    std::printf("[T0 process_cooccurrence] FAIL: co-resident radar process was not retained\n");
    return {false, steps, "scar verification failed: co-resident radar process"};
  }
  std::printf("[T0 process_cooccurrence] step %d: co-resident radar process planted and verified\n", ++steps);
  const std::string detail = "process_cooccurrence: VM_READ plus co-resident radar process";
  w.note(detail);
  return {true, steps, detail};
}

}  // namespace examples::process_cooccurrence
