// RED example implementation for strategy `pool_tag_hide`.
// Multi-step World scars: pool_tag_anomaly residual + pool-obscure.sys driver.

#include "red_example.hpp"

#include <cstdio>

namespace examples::pool_tag_hide {

RedResult apply(sim::World& w) {
  RedResult r{};
  r.detail = "pool-tag precondition failed";

  std::printf("[red:pool_tag_hide] require pool tag anomaly not already set\n");
  if (w.pool_tag_anomaly) {
    return r;
  }

  std::printf("[red:pool_tag_hide] plant pool allocation tag anomaly residual\n");
  w.pool_tag_anomaly = true;
  if (!w.pool_tag_anomaly) {
    r.detail = "failed to set pool_tag_anomaly";
    return r;
  }
  ++r.steps;

  std::printf("[red:pool_tag_hide] load pool-obscure.sys lab driver\n");
  w.load_driver({"pool-obscure.sys", "pool-sha", "Unknown", false, false, false,
                 false, true});
  if (w.drivers.empty() || w.drivers.back().name != "pool-obscure.sys") {
    r.detail = "pool-obscure.sys registration failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:pool_tag_hide] verify anomaly residual and driver presence\n");
  if (!w.pool_tag_anomaly || w.drivers.empty()) {
    r.detail = "pool-tag hide verification failed";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "pool allocation disguise residual and driver verified";
  w.note(r.detail);
  std::printf("[red:pool_tag_hide] %s (%d steps)\n", r.detail.c_str(), r.steps);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "pool_tag_hide",
         "Plant pool-tag camouflage residual.");
  auto r = apply(w);
  n.say(sim::Side::Red, r.detail);
  return r;
}

}  // namespace examples::pool_tag_hide
