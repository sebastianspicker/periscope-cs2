// RED example implementation for strategy `minifilter_strip`.
// Multi-step World scars: clear minifilter callbacks and present flag, then verify.

#include "red_example.hpp"

#include <cstdio>

namespace examples::minifilter_strip {

RedResult apply(sim::World& w) {
  RedResult r{};
  r.detail = "minifilter precondition failed";

  std::printf("[red:minifilter_strip] require minifilter present with callbacks\n");
  if (!w.minifilter_present || !w.minifilter_callbacks) {
    return r;
  }

  std::printf("[red:minifilter_strip] zero minifilter callback registrations\n");
  w.minifilter_callbacks = 0;
  if (w.minifilter_callbacks != 0) {
    r.detail = "failed to clear minifilter_callbacks";
    return r;
  }
  ++r.steps;

  std::printf("[red:minifilter_strip] clear minifilter_present residual\n");
  w.minifilter_present = false;
  if (w.minifilter_present) {
    r.detail = "failed to clear minifilter_present";
    return r;
  }
  ++r.steps;

  std::printf("[red:minifilter_strip] verify both scars remain cleared\n");
  if (w.minifilter_callbacks || w.minifilter_present) {
    r.detail = "minifilter strip verification failed";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "minifilter callback removal verified";
  w.note(r.detail);
  std::printf("[red:minifilter_strip] %s (%d steps)\n", r.detail.c_str(), r.steps);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "minifilter_strip", "Remove minifilter callbacks.");
  auto r = apply(w);
  n.say(sim::Side::Red, r.detail);
  return r;
}

}  // namespace examples::minifilter_strip
