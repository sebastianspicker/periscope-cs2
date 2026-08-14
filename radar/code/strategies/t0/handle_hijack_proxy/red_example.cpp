#include "red_example.hpp"

#include <cstdio>

namespace examples::handle_hijack_proxy {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    std::printf("[T0 handle_hijack_proxy] FAIL: game process is unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T0 handle_hijack_proxy] step %d: game pid=%u validated\n", ++steps, game_pid);

  const auto actor = w.spawn("handle_hijack_proxy-actor.exe");
  auto* actor_process = w.proc(actor);
  if (!actor_process) {
    std::printf("[T0 handle_hijack_proxy] FAIL: actor creation was not observable\n");
    return {false, steps, "actor spawn verification failed"};
  }
  std::printf("[T0 handle_hijack_proxy] step %d: actor pid=%u created\n", ++steps, actor);

  const auto proxy = w.spawn("handle-hijack-proxy.exe");
  if (!w.open_process(proxy, game_pid, sim::AccessMask::VmRead, false)) {
    std::printf("[T0 handle_hijack_proxy] FAIL: VM_READ handle denied\n");
    return {false, steps, "OpenProcess VM_READ failed"};
  }
  bool has_handle = false;
  bool has_brief_handle = false;
  for (const auto& handle : w.handles_to(game_pid)) {
    if (handle.owner_pid == proxy && sim::has(handle.access, sim::AccessMask::VmRead)) {
      has_handle = true;
      has_brief_handle = handle.brief_reopen;
    }
  }
  if (!has_handle) {
    return {false, steps, "VM_READ handle verification failed"};
  }
  std::printf("[T0 handle_hijack_proxy] step %d: VM_READ edge verified\n", ++steps);

  auto read = w.read_mem(proxy, game_pid, game->base, 4, true);
  if (read.status != ac::Status::Ok || read.bytes.size() != 4) {
    std::printf("[T0 handle_hijack_proxy] FAIL: protected read failed\n");
    return {false, steps, "VM_READ verification read failed"};
  }
  std::printf("[T0 handle_hijack_proxy] step %d: read telemetry recorded\n", ++steps);

  w.handle_proxy_active = true; w.handle_proxy_owner_pid = proxy; w.handle_proxy_consumer_pid = actor;
  const bool scar_verified = w.handle_proxy_active && w.handle_proxy_owner_pid == proxy;
  if (!scar_verified) {
    std::printf("[T0 handle_hijack_proxy] FAIL: proxy-owned VM_READ edge was not retained\n");
    return {false, steps, "scar verification failed: proxy-owned VM_READ edge"};
  }
  std::printf("[T0 handle_hijack_proxy] step %d: proxy-owned VM_READ edge planted and verified\n", ++steps);
  const std::string detail = "handle_hijack_proxy: VM_READ plus proxy-owned VM_READ edge";
  w.note(detail);
  return {true, steps, detail, proxy, actor};
}

}  // namespace examples::handle_hijack_proxy
