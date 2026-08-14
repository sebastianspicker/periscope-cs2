#include "red_example.hpp"

#include <cstdio>

namespace examples::handle_minimize {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    std::printf("[T0 handle_minimize] FAIL: game process is unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T0 handle_minimize] step %d: game pid=%u validated\n", ++steps, game_pid);

  const auto actor = w.spawn("handle_minimize-actor.exe");
  auto* actor_process = w.proc(actor);
  if (!actor_process) {
    std::printf("[T0 handle_minimize] FAIL: actor creation was not observable\n");
    return {false, steps, "actor spawn verification failed"};
  }
  std::printf("[T0 handle_minimize] step %d: actor pid=%u created\n", ++steps, actor);

  if (!w.open_process(actor, game_pid, sim::AccessMask::VmRead, false)) {
    std::printf("[T0 handle_minimize] FAIL: VM_READ handle denied\n");
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
  std::printf("[T0 handle_minimize] step %d: VM_READ edge verified\n", ++steps);

  auto read = w.read_mem(actor, game_pid, game->base, 4, true);
  if (read.status != ac::Status::Ok || read.bytes.size() != 4) {
    std::printf("[T0 handle_minimize] FAIL: protected read failed\n");
    return {false, steps, "VM_READ verification read failed"};
  }
  std::printf("[T0 handle_minimize] step %d: read telemetry recorded\n", ++steps);

  // Plant brief-reopen scar, then re-verify from World handles (not the pre-mark snapshot).
  for (auto& h : w.handles) {
    if (h.owner_pid == actor && h.target_pid == game_pid &&
        sim::has(h.access, sim::AccessMask::VmRead)) {
      h.brief_reopen = true;
    }
  }
  has_brief_handle = false;
  for (const auto& handle : w.handles_to(game_pid, true)) {
    if (handle.owner_pid == actor &&
        sim::has(handle.access, sim::AccessMask::VmRead) && handle.brief_reopen) {
      has_brief_handle = true;
      break;
    }
  }
  if (!has_brief_handle) {
    std::printf("[T0 handle_minimize] FAIL: brief VM_READ handle was not retained\n");
    return {false, steps, "scar verification failed: brief VM_READ handle"};
  }
  std::printf("[T0 handle_minimize] step %d: brief VM_READ handle planted and verified\n", ++steps);
  const std::string detail = "handle_minimize: VM_READ plus brief VM_READ handle";
  w.note(detail);
  return {true, steps, detail};
}

}  // namespace examples::handle_minimize
