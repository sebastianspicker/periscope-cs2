// RED example implementation for strategy `object_callback_strip`.
// Multi-step World scars: clear object callbacks and compare against prior ground truth.

#include "red_example.hpp"

#include <cstdio>

namespace examples::object_callback_strip {

RedResult apply(sim::World& w) {
  RedResult r{};
  r.detail = "object callback precondition failed";

  std::printf("[red:object_callback_strip] require object callbacks present\n");
  if (!w.object_callbacks_present || !w.object_callbacks) {
    return r;
  }

  std::printf("[red:object_callback_strip] snapshot ground-truth count before strip\n");
  const auto before = w.object_callbacks;
  if (before == 0) {
    r.detail = "ground-truth object callback count already zero";
    return r;
  }
  ++r.steps;

  std::printf("[red:object_callback_strip] zero live object callback registrations\n");
  w.object_callbacks = 0;
  w.object_callbacks_present = false;
  if (w.object_callbacks || w.object_callbacks_present) {
    r.detail = "failed to clear object callback scars";
    return r;
  }
  ++r.steps;

  std::printf("[red:object_callback_strip] verify live state diverges from prior ground truth\n");
  if (w.object_callbacks || w.object_callbacks_present || before == 0) {
    r.detail = "object-handle callback removal verification failed";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "object-handle callback removal verified";
  w.note(r.detail);
  std::printf("[red:object_callback_strip] %s (%d steps)\n", r.detail.c_str(),
              r.steps);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "object_callback_strip", "Remove object callbacks.");
  auto r = apply(w);
  n.say(sim::Side::Red, r.detail);
  return r;
}

}  // namespace examples::object_callback_strip
