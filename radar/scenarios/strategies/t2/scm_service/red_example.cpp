// RED example implementation for strategy `scm_service`.
// Multi-step World scars: SCM kernel service + scm-lab.sys driver load.

#include "red_example.hpp"

#include <cstdio>

namespace examples::scm_service {

RedResult apply(sim::World& w) {
  RedResult r{};
  r.detail = "SCM precondition failed: ranked access denied";

  std::printf("[red:scm_service] require ranked access still permitted\n");
  if (w.ranked_access_denied) {
    return r;
  }

  std::printf("[red:scm_service] create LabKernelService pointing at scm-lab.sys\n");
  w.add_service({"LabKernelService", "scm-lab.sys", true});
  if (w.services.empty() || !w.services.back().kernel_driver) {
    r.detail = "kernel service registration failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:scm_service] load scm-lab.sys through the service path\n");
  w.load_driver({"scm-lab.sys", "scm-demo-sha", "Unknown", false, false, false,
                 false, true});
  if (w.drivers.empty() || w.drivers.back().name != "scm-lab.sys") {
    r.detail = "scm-lab.sys load failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:scm_service] verify service + driver residuals\n");
  if (w.services.empty() || !w.services.back().kernel_driver || w.drivers.empty()) {
    r.detail = "SCM service/driver verification failed";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "kernel service installation and driver load verified";
  w.note(r.detail);
  std::printf("[red:scm_service] %s (%d steps)\n", r.detail.c_str(), r.steps);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "scm_service", "Create a kernel driver service.");
  auto r = apply(w);
  n.say(sim::Side::Red, r.detail);
  return r;
}

}  // namespace examples::scm_service
