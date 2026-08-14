#include "blue_example.hpp"
namespace examples::ac_self_integrity {
BlueResult detect(sim::World& w) {
  BlueResult r;
  int dirty = 0; bool hook = false;
  if (auto* p = w.proc(w.ac_pid())) {
    for (const auto& m : p->modules) {
      if (m.text_hash != "clean") ++dirty;
      if (m.iat_hooked || m.eat_hooked) hook = true;
    }
  }
  // Multi-reason: dirty text hash + hook surface (independent signals).
  const int signals = (dirty >= 1 ? 1 : 0) + (hook ? 1 : 0);
  r.detected = signals >= 2;
  r.mitigated = r.detected;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "ac_self_integrity blue dirty=" + std::to_string(dirty) +
             " hook=" + std::to_string(hook ? 1 : 0) +
             " signals=" + std::to_string(signals);
  w.note(r.detail);
  return r;
}
}  // namespace examples::ac_self_integrity
