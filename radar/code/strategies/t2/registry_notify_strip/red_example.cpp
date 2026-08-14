// RED example implementation for strategy `registry_notify_strip`.
// Multi-step World scars: clear registry_notify count and present flag, then verify.

#include "red_example.hpp"

#include <cstdio>

namespace examples::registry_notify_strip {

RedResult apply(sim::World& w) {
  RedResult r{};
  r.detail = "registry notify precondition failed";

  std::printf("[red:registry_notify_strip] require registry notifications present\n");
  if (!w.registry_notify_present || !w.registry_notify) {
    return r;
  }

  std::printf("[red:registry_notify_strip] zero registry notification callbacks\n");
  w.registry_notify = 0;
  if (w.registry_notify != 0) {
    r.detail = "failed to clear registry_notify";
    return r;
  }
  ++r.steps;

  std::printf("[red:registry_notify_strip] clear registry_notify_present residual\n");
  w.registry_notify_present = false;
  if (w.registry_notify_present) {
    r.detail = "failed to clear registry_notify_present";
    return r;
  }
  ++r.steps;

  std::printf("[red:registry_notify_strip] verify both scars remain cleared\n");
  if (w.registry_notify || w.registry_notify_present) {
    r.detail = "registry callback removal verification failed";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "registry callback removal verified";
  w.note(r.detail);
  std::printf("[red:registry_notify_strip] %s (%d steps)\n", r.detail.c_str(),
              r.steps);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "registry_notify_strip",
         "Remove registry notifications.");
  auto r = apply(w);
  n.say(sim::Side::Red, r.detail);
  return r;
}

}  // namespace examples::registry_notify_strip
