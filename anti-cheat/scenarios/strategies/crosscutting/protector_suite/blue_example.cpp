// BLUE: suite flags + watchdog/anti-suspend payload micro-detail multi-reason.

#include "blue_example.hpp"
#include <sstream>

namespace examples::protector_suite {

BlueResult detect(sim::World& w) {
  BlueResult r;
  int reasons = 0;
  if (w.anti_debug_active || w.thread_hide_from_debugger ||
      w.peb_being_debugged_spoofed) {
    ++reasons;
  }
  if (w.anti_suspend_active) {
    ++reasons;
    r.anti_suspend_detail = true;
  }
  if (w.self_destruct_armed) ++reasons;
  if (w.protector_watchdog) ++reasons;

  // Micro-detail residual: hidden watchdog payload with heartbeats.
  r.watchdog_detail = w.anti_suspend_watchdog_payload &&
                      w.watchdog_heartbeat_ticks >= 2 &&
                      (w.watchdog_payload_hidden ||
                       w.anti_suspend_watchdog_pid != 0);
  if (r.watchdog_detail) ++reasons;

  bool handle = false;
  for (const auto& h : w.handles_to(w.game_pid())) {
    if (sim::has(h.access, sim::AccessMask::VmRead)) {
      const auto* p = w.proc(h.owner_pid);
      if (p && !p->is_game && !p->is_ac) {
        handle = true;
        break;
      }
    }
  }
  if (handle) ++reasons;

  r.reasons = reasons;
  r.detected = reasons >= 2;
  r.mitigated = reasons >= 3 && r.watchdog_detail;
  if (r.mitigated) {
    w.anti_debug_active = false;
    w.anti_suspend_active = false;
    w.self_destruct_armed = false;
    w.protector_watchdog = false;
    w.anti_suspend_watchdog_payload = false;
    w.watchdog_heartbeat_ticks = 0;
    w.watchdog_payload_hidden = false;
    w.anti_suspend_watchdog_pid = 0;
    w.ranked_access_denied = true;
  }
  std::ostringstream oss;
  oss << "protector_suite blue reasons=" << reasons
      << " handle=" << (handle ? 1 : 0)
      << " watchdog_detail=" << (r.watchdog_detail ? 1 : 0)
      << " anti_suspend_detail=" << (r.anti_suspend_detail ? 1 : 0);
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::protector_suite
