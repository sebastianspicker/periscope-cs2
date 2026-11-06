// RED: gaming-chair protector suite + anti-suspend watchdog payload micro-detail.

#include "red_example.hpp"
#include <sstream>

namespace examples::protector_suite {

RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("protected-radar.exe");
  (void)w.open_process(r.actor_pid, w.game_pid(), sim::AccessMask::VmRead, false);

  w.anti_debug_active = true;
  w.anti_suspend_active = true;
  w.self_destruct_armed = true;
  w.protector_watchdog = true;
  w.thread_hide_from_debugger = true;
  w.peb_being_debugged_spoofed = true;
  // Micro-detail beyond coarse suite flags (gaming-chair watchdog payload class).
  const auto wd = w.spawn("watchdog-payload.exe");
  w.anti_suspend_watchdog_payload = true;
  w.anti_suspend_watchdog_pid = wd;
  w.watchdog_heartbeat_ticks = 5;
  w.watchdog_payload_hidden = true;
  if (auto* p = w.proc(wd)) {
    p->hidden_from_weak_enum = true;
  }

  w.canary_tripped = false;
  w.analysis_host = false;

  r.achieved = w.anti_debug_active && w.anti_suspend_active &&
               w.self_destruct_armed && w.protector_watchdog &&
               w.anti_suspend_watchdog_payload &&
               w.watchdog_heartbeat_ticks >= 2;
  std::ostringstream oss;
  oss << "protector_suite red anti_debug=1 anti_suspend=1 destruct=1 "
      << "watchdog=1 payload=1 heartbeat=" << w.watchdog_heartbeat_ticks
      << " wd_pid=" << w.anti_suspend_watchdog_pid
      << " hidden=" << (w.watchdog_payload_hidden ? 1 : 0);
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::protector_suite
