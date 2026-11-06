#include "blue_example.hpp"

#include <cstdio>

namespace examples::proxy_hijack_reader {

bool detect(sim::World& w, sim::Narrator& n) {
  const auto game_pid = w.game_pid();
  n.say(sim::Side::Blue, "sensor 1: handle-table scan for radar-owned VM_READ");

  bool radar_handle = false;
  bool proxy_handle = false;
  for (const auto& handle : w.handles_to(game_pid, true)) {
    if (!sim::has(handle.access, sim::AccessMask::VmRead)) continue;
    const auto* owner = w.proc(handle.owner_pid);
    if (!owner || owner->is_game || owner->is_ac) continue;
    if (owner->name == "radar.exe") {
      radar_handle = true;
      n.say(sim::Side::Blue, "DETECTED: radar.exe has a VM_READ handle.");
    }
    if (owner->name == "proxy.exe" || handle.via_proxy) {
      proxy_handle = true;
      n.say(sim::Side::Blue, "Found proxy-owned VM_READ handle.");
    }
  }

  n.say(sim::Side::Blue, "sensor 2: named-pipe IPC co-occurrence residual");
  const bool ipc = w.named_pipe_ipc_active || w.named_pipe_message_count > 0;
  if (ipc) {
    n.say(sim::Side::Blue, "Named-pipe IPC residual active during entity relay.");
  }

  n.say(sim::Side::Blue, "sensor 3: remote read activity without radar handle");
  const bool reads = w.remote_read_ops > 0 || w.named_pipe_message_count > 0;

  // Multi-reason score: independent handle / IPC / read residuals.
  int signals = 0;
  if (radar_handle) ++signals;
  if (proxy_handle) ++signals;
  if (ipc) ++signals;
  if (reads) ++signals;
  const bool detected = signals >= 2;
  std::printf("[blue:proxy_hijack_reader] radar=%d proxy=%d ipc=%d reads=%d signals=%d detected=%d\n",
              radar_handle ? 1 : 0, proxy_handle ? 1 : 0, ipc ? 1 : 0,
              reads ? 1 : 0, signals, detected ? 1 : 0);
  if (!detected) {
    n.say(sim::Side::Blue, "No multi-reason proxy/handle residual observed.");
  }
  return detected;
}

}  // namespace examples::proxy_hijack_reader
