#include "red_example.hpp"
namespace examples::ac_self_integrity {
RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("ac-tamper.exe");
  auto ac = w.ac_pid();
  if (auto* p = w.proc(ac)) {
    // Phase 1: corrupt .text hashes on AC modules.
    for (auto& m : p->modules) {
      m.text_hash = "tampered";
      ++r.dirty;
    }
    // Phase 2: plant IAT hooks on the same modules (second scar class).
    for (auto& m : p->modules) {
      m.iat_hooked = true;
    }
  }
  // Phase 3: note session scar for blue correlation.
  w.note("ac_self_integrity: text+iat tamper planted");
  r.achieved = r.dirty >= 1;
  r.detail = "ac_self_integrity red dirty_modules=" + std::to_string(r.dirty) + " iat_hooks=1";
  return r;
}
}  // namespace examples::ac_self_integrity
