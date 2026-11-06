// RED example implementation for strategy `callback_strip`.
// Multi-step World scars: clear process notify, image notify, and AC callback flag.

#include "red_example.hpp"

#include <cstdio>

namespace examples::callback_strip {

RedResult apply(sim::World& w) {
  RedResult r{};
  r.detail = "callback strip precondition failed: callbacks absent";

  std::printf("[red:callback_strip] require process notify and AC callback baseline\n");
  if (!w.ac_callback_present || w.process_notify == 0) {
    return r;
  }
  const auto before = w.process_notify;

  std::printf("[red:callback_strip] clear process notify registrations\n");
  w.process_notify = 0;
  if (w.process_notify != 0 || before == 0) {
    r.detail = "failed to clear process_notify";
    return r;
  }
  ++r.steps;

  std::printf("[red:callback_strip] clear image notify registrations\n");
  w.image_notify = 0;
  if (w.image_notify != 0) {
    r.detail = "failed to clear image_notify";
    return r;
  }
  ++r.steps;

  std::printf("[red:callback_strip] clear AC callback present residual\n");
  w.ac_callback_present = false;
  if (w.process_notify != 0 || w.image_notify != 0 || w.ac_callback_present) {
    r.detail = "callback strip verification failed";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "process, image, and AC callback removal verified";
  w.note(r.detail);
  std::printf("[red:callback_strip] %s (%d steps)\n", r.detail.c_str(), r.steps);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "callback_strip",
         "Remove callback registration surfaces.");
  auto r = apply(w);
  n.say(sim::Side::Red, r.detail);
  return r;
}

}  // namespace examples::callback_strip
