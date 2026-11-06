#include "red_example.hpp"

#include "ac/types.hpp"

namespace examples::proxy_hijack_reader {

RedResult apply(sim::World& w, sim::Narrator& n) {
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    n.say(sim::Side::Red, "Proxy reader unavailable: game process is missing.");
    return {false, "precondition failed: game process unavailable"};
  }

  const auto proxy_pid = w.spawn("proxy.exe");
  const auto radar_pid = w.spawn("radar.exe");
  n.say(sim::Side::Red, "[proxy] Opening VM_READ handle to game (proxy process).");
  if (!w.open_process(proxy_pid, game_pid, sim::AccessMask::VmRead, false)) {
    return {false, "proxy VM_READ handle creation failed", proxy_pid, radar_pid};
  }

  n.say(sim::Side::Red, "[proxy] Creating simulated named-pipe IPC channel.");
  w.named_pipe_ipc_active = true;

  const ReadRequest request{"entity_list", game->base + w.lab_entity_table_rel,
                            sizeof(std::uint32_t)};
  n.say(sim::Side::Red, "[radar] Sending entity-list read request over IPC (no game handle).");
  const auto read = w.read_mem(proxy_pid, game_pid, request.address, request.size, true);
  if (read.status != ac::Status::Ok || read.bytes.size() != request.size) {
    return {false, "proxy failed to service IPC read request", proxy_pid, radar_pid};
  }
  ++w.named_pipe_message_count;

  bool radar_has_vm_read = false;
  for (const auto& handle : w.handles_to(game_pid, true)) {
    radar_has_vm_read = radar_has_vm_read ||
        (handle.owner_pid == radar_pid && sim::has(handle.access, sim::AccessMask::VmRead));
  }
  if (radar_has_vm_read) {
    return {false, "radar unexpectedly owns a VM_READ handle", proxy_pid, radar_pid};
  }

  n.say(sim::Side::Red, "[radar] Received entity data; no handle to game was created.");
  const std::string detail = "proxy_hijack_reader: proxy relayed entity data over simulated IPC";
  w.note(detail);
  return {true, detail, proxy_pid, radar_pid};
}

}  // namespace examples::proxy_hijack_reader
