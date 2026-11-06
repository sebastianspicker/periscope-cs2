#include "blue_example.hpp"

#include <string>

namespace examples::anti_re_canary {

BlueResult detect(sim::World& w) {
  BlueResult r;
  const int reasons = (w.thread_hide_from_debugger ? 1 : 0) +
                      (w.peb_being_debugged_spoofed ? 1 : 0) +
                      (w.canary_tripped ? 1 : 0);
  r.detected = reasons >= 2;
  r.mitigated = reasons >= 2;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "anti-analysis consistency reasons=" + std::to_string(reasons);
  return r;
}

}  // namespace examples::anti_re_canary
