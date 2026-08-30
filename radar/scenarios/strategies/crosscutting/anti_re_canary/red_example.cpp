#include "red_example.hpp"

namespace examples::anti_re_canary {

RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("integrity-canary-lab.exe");
  // The arena's analysis_host is the only debugger model used in this lesson.
  w.canary_tripped = w.analysis_host;
  w.thread_hide_from_debugger = true;
  w.peb_being_debugged_spoofed = true;
  r.achieved = true;
  r.detail = w.canary_tripped
      ? "canary tripped: simulated sensitive state cleared without executing payloads"
      : "canary guards armed with contradictory debugger-state metadata";
  w.note(r.detail);
  return r;
}

}  // namespace examples::anti_re_canary
