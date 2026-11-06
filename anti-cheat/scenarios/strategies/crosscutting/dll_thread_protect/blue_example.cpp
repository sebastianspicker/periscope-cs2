#include "blue_example.hpp"
#include <sstream>
namespace examples::dll_thread_protect {
BlueResult detect(sim::World& w) {
  BlueResult r;
  int reasons = 0;
  if (w.dll_protection_active) ++reasons;
  if (w.thread_protection_active) ++reasons;
  if (w.protector_find_target) ++reasons;
  if (w.protected_thread_count >= 2) ++reasons;
  r.detected = reasons >= 2;
  r.mitigated = reasons >= 3;
  if (r.mitigated) {
    w.dll_protection_active = false;
    w.thread_protection_active = false;
    w.protector_find_target = false;
    w.protected_thread_count = 0;
    w.ranked_access_denied = true;
  }
  std::ostringstream oss;
  oss << "dll_thread_protect blue reasons=" << reasons
      << " detail_not_suite_only=1";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}
}  // namespace examples::dll_thread_protect
